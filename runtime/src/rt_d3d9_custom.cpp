// Méthodes D3D9 écrites à la main (liste CUSTOM de tools/d3d9/gen_com.py).
//
// Verrouillage : le tampon hôte n'est pas dans l'espace invité. Lock fournit une zone de transit
// dans la mémoire invitée, recopiée depuis l'hôte ; Unlock recopie la zone vers l'hôte.

#include "rt_d3d9.h"
#include "rt_proc.h"
#include "rt_win.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

extern int rt_trace_imports;  /* rt_shims.c : trace des imports (CT3_IMPORTS, CT3_TRACE_FROM) */

namespace {

struct lock_rec {
    rt_com_obj *obj;
    uint32_t sub;       // niveau (texture), ou face<<16 | niveau (cube)
    uint32_t buf;       // zone de transit invitée
    uint32_t pitch;
    uint32_t rows;
    uint32_t rowbytes;
    uint8_t *host_bits;
    uint32_t dxt = 0;   // format BC logique d'une texture de repli (0 sinon)
    uint32_t rw = 0;    // largeur de la région en pixels (décodage BC)
    uint32_t rh = 0;    // hauteur de la région en pixels (décodage BC)
};

std::vector<lock_rec> locks;
std::multimap<uint32_t, uint32_t> free_bufs;  // taille -> zone de transit libre

bool same_iid(const IID &a, const IID &b)
{
    return memcmp(&a, &b, sizeof(IID)) == 0;
}

uint32_t get32(uint32_t addr)
{
    uint32_t v;
    memcpy(&v, rt_mem_base + addr, 4);
    return v;
}

void put32(uint32_t addr, uint32_t v)
{
    memcpy(rt_mem_base + addr, &v, 4);
}

uint32_t stage_alloc(uint32_t size)
{
    auto it = free_bufs.lower_bound(size);
    if (it != free_bufs.end() && it->first < size * 2) {
        uint32_t buf = it->second;
        free_bufs.erase(it);
        return buf;
    }
    return rt_heap_alloc(size ? size : 1, 0);
}

lock_rec *find_lock(rt_com_obj *o, uint32_t sub)
{
    for (auto &r : locks) {
        if (r.obj == o && r.sub == sub) return &r;
    }
    return nullptr;
}

void release_lock(lock_rec *r)
{
    free_bufs.insert({r->pitch * r->rows, r->buf});
    *r = locks.back();
    locks.pop_back();
}

// Taille d'un bloc (formats compressés) ou d'un pixel, en octets.
struct fmt_info {
    uint32_t bpp;
    bool block;
};

fmt_info format_info(D3DFORMAT f)
{
    switch (f) {
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A2B10G10R10:
    case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8: case D3DFMT_G16R16: case D3DFMT_G16R16F:
    case D3DFMT_R32F: case D3DFMT_A2R10G10B10: case D3DFMT_D24S8: case D3DFMT_D32:
    case D3DFMT_D24X8: case D3DFMT_D24FS8:
        return {4, false};
    case D3DFMT_A16B16G16R16: case D3DFMT_A16B16G16R16F: case D3DFMT_G32R32F:
        return {8, false};
    case D3DFMT_A32B32G32R32F:
        return {16, false};
    case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4:
    case D3DFMT_A8L8: case D3DFMT_A8P8: case D3DFMT_X4R4G4B4: case D3DFMT_A8R3G3B2:
    case D3DFMT_R16F: case D3DFMT_D16: case D3DFMT_D16_LOCKABLE: case D3DFMT_V8U8:
    case D3DFMT_L16:
        return {2, false};
    case D3DFMT_DXT1:
        return {8, true};
    case D3DFMT_DXT2: case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5:
        return {16, true};
    default:
        return {1, false};
    }
}

bool is_dxt(uint32_t fmt)
{
    return fmt == D3DFMT_DXT1 || fmt == D3DFMT_DXT2 || fmt == D3DFMT_DXT3 || fmt == D3DFMT_DXT4 ||
           fmt == D3DFMT_DXT5;
}

// Couleurs d'un bloc BC1 (8 octets) : palette RGB565 et index 2 bits.
void decode_bc1_block(const uint8_t *b, uint8_t out[16][4])
{
    uint16_t c0 = (uint16_t)(b[0] | (b[1] << 8));
    uint16_t c1 = (uint16_t)(b[2] | (b[3] << 8));
    uint8_t pal[4][4];
    auto expand = [](uint16_t c, uint8_t rgb[3]) {
        rgb[0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
        rgb[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
        rgb[2] = (uint8_t)((c & 31) * 255 / 31);
    };
    expand(c0, pal[0]);
    expand(c1, pal[1]);
    for (int k = 0; k < 3; k++) {
        pal[2][k] = (uint8_t)((2 * pal[0][k] + pal[1][k]) / 3);
        pal[3][k] = (uint8_t)((pal[0][k] + 2 * pal[1][k]) / 3);
    }
    if (c0 <= c1) {
        for (int k = 0; k < 3; k++) pal[2][k] = (uint8_t)((pal[0][k] + pal[1][k]) / 2);
        pal[3][0] = pal[3][1] = pal[3][2] = 0;
    }
    uint32_t idx = b[4] | (b[5] << 8) | (b[6] << 16) | ((uint32_t)b[7] << 24);
    for (int i = 0; i < 16; i++) {
        const uint8_t *p = pal[(idx >> (2 * i)) & 3];
        out[i][0] = p[0];
        out[i][1] = p[1];
        out[i][2] = p[2];
        out[i][3] = 255;
    }
}

// Décode les blocs de la zone de transit (stride = pitch, comme le voit le code invité) vers la
// surface hôte X8R8G8B8 (octets B, G, R, X).
void decode_dxt(const lock_rec *r)
{
    const uint32_t bs = r->dxt == D3DFMT_DXT1 ? 8 : 16;
    const uint32_t bw = (r->rw + 3) / 4, bh = (r->rh + 3) / 4;
    for (uint32_t by = 0; by < bh; by++) {
        for (uint32_t bx = 0; bx < bw; bx++) {
            const uint8_t *blk = rt_mem_base + r->buf + (size_t)by * r->pitch + (size_t)bx * bs;
            uint8_t px[16][4];
            decode_bc1_block(bs == 16 ? blk + 8 : blk, px);
            for (uint32_t j = 0; j < 4; j++) {
                for (uint32_t i = 0; i < 4; i++) {
                    uint32_t x = 4 * bx + i, y = 4 * by + j;
                    if (x >= r->rw || y >= r->rh) continue;
                    uint8_t *d = r->host_bits + (size_t)y * r->pitch + (size_t)x * 4;
                    const uint8_t *c = px[j * 4 + i];
                    d[0] = c[2];
                    d[1] = c[1];
                    d[2] = c[0];
                    d[3] = 0xFF;
                }
            }
        }
    }
}

// Copie ligne à ligne entre la zone de transit invitée et la mémoire hôte.
void copy_rows(lock_rec *r, bool to_guest)
{
    for (uint32_t row = 0; row < r->rows; row++) {
        uint8_t *host = r->host_bits + (size_t)row * r->pitch;
        uint8_t *guest = rt_mem_base + r->buf + (size_t)row * r->pitch;
        if (to_guest) {
            memcpy(guest, host, r->rowbytes);
        } else {
            memcpy(host, guest, r->rowbytes);
        }
    }
}

// Description de la zone verrouillée : lignes et octets par ligne.
void region_of(D3DFORMAT fmt, uint32_t width, uint32_t height, const RECT *rect,
               uint32_t pitch, uint32_t &rows, uint32_t &rowbytes)
{
    fmt_info fi = format_info(fmt);
    uint32_t l = 0, t = 0, r = width, b = height;
    if (rect) {
        l = (uint32_t)rect->left;
        t = (uint32_t)rect->top;
        r = (uint32_t)rect->right;
        b = (uint32_t)rect->bottom;
    }
    if (fi.block) {
        rows = (b - t + 3) / 4;
        rowbytes = ((r - l + 3) / 4) * fi.bpp;
        if (!rect) {
            rows = (height + 3) / 4;
            rowbytes = pitch;
        }
    } else {
        rows = b - t;
        rowbytes = (r - l) * fi.bpp;
        if (!rect) {
            rows = height;
            rowbytes = pitch;
        }
    }
}

}  // namespace

// ---- IUnknown -------------------------------------------------------------

extern "C" uint32_t custom_IUnknown_QueryInterface(cpu_t *c, const uint32_t *a)
{
    IUnknown *self = rt_self<IUnknown>(a[0]);
    const IID *iid = static_cast<const IID *>(rt_gptr(a[1]));
    void *out = nullptr;
    HRESULT hr = self->QueryInterface(*iid, &out);
    int idx = rt_com_iface_index("IDirect3D9");
    if (same_iid(*iid, IID_IDirect3DDevice9)) idx = rt_com_iface_index("IDirect3DDevice9");
    else if (same_iid(*iid, IID_IDirect3DSurface9)) idx = rt_com_iface_index("IDirect3DSurface9");
    else if (same_iid(*iid, IID_IDirect3DTexture9)) idx = rt_com_iface_index("IDirect3DTexture9");
    else if (same_iid(*iid, IID_IDirect3DVertexBuffer9)) idx = rt_com_iface_index("IDirect3DVertexBuffer9");
    else if (same_iid(*iid, IID_IDirect3DIndexBuffer9)) idx = rt_com_iface_index("IDirect3DIndexBuffer9");
    else if (same_iid(*iid, IID_IDirect3DSwapChain9)) idx = rt_com_iface_index("IDirect3DSwapChain9");
    if (FAILED(hr)) out = nullptr;
    if (a[2]) guest_st(c, a[2], 4, FAILED(hr) ? 0 : rt_com_wrap(out, idx));
    return (uint32_t)hr;
}

extern "C" uint32_t custom_IUnknown_Release(cpu_t *c, const uint32_t *a)
{
    (void)c;
    void *host = rt_com_host(a[0]);
    ULONG r = static_cast<IUnknown *>(host)->Release();
    if (r == 0) rt_com_forget(host);
    return (uint32_t)r;
}

// ---- IDirect3D9 / IDirect3DDevice9 ----------------------------------------

extern "C" uint32_t custom_IDirect3D9_GetAdapterMonitor(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    self->GetAdapterMonitor(a[1]);
    return 0x0000F001u;  // moniteur fictif : handle invité stable
}

extern "C" uint32_t custom_IDirect3DDevice9_GetCreationParameters(cpu_t *c, const uint32_t *a)
{
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    D3DDEVICE_CREATION_PARAMETERS p;
    memset(&p, 0, sizeof p);
    HRESULT hr = self->GetCreationParameters(&p);
    if (SUCCEEDED(hr) && a[1]) {
        guest_st(c, a[1] + 0, 4, p.AdapterOrdinal);
        guest_st(c, a[1] + 4, 4, p.DeviceType);
        guest_st(c, a[1] + 8, 4, rt_win_guest(p.hFocusWindow));
        guest_st(c, a[1] + 12, 4, p.BehaviorFlags);
    }
    return (uint32_t)hr;
}

extern "C" uint32_t custom_IDirect3DDevice9_CreateVertexBuffer(cpu_t *c, const uint32_t *a)
{
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DVertexBuffer9 *vb = nullptr;
    HRESULT hr = self->CreateVertexBuffer(a[1], a[2], a[3], (D3DPOOL)a[4], &vb,
                                          static_cast<HANDLE *>(rt_gptr(a[6])));
    if (SUCCEEDED(hr) && vb) {
        uint32_t g = rt_com_wrap(vb, rt_com_iface_index("IDirect3DVertexBuffer9"));
        rt_com_obj_of(g)->extra = a[1];
        if (a[5]) guest_st(c, a[5], 4, g);
    } else if (a[5]) {
        guest_st(c, a[5], 4, 0);
    }
    return (uint32_t)hr;
}

extern "C" uint32_t custom_IDirect3DDevice9_CreateIndexBuffer(cpu_t *c, const uint32_t *a)
{
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DIndexBuffer9 *ib = nullptr;
    HRESULT hr = self->CreateIndexBuffer(a[1], a[2], (D3DFORMAT)a[3], (D3DPOOL)a[4], &ib,
                                         static_cast<HANDLE *>(rt_gptr(a[6])));
    if (SUCCEEDED(hr) && ib) {
        uint32_t g = rt_com_wrap(ib, rt_com_iface_index("IDirect3DIndexBuffer9"));
        rt_com_obj_of(g)->extra = a[1];
        if (a[5]) guest_st(c, a[5], 4, g);
    } else if (a[5]) {
        guest_st(c, a[5], 4, 0);
    }
    return (uint32_t)hr;
}

// ---- Tampons de sommets et d'index ----------------------------------------

static uint32_t lock_buffer(cpu_t *c, rt_com_obj *o, uint32_t off, uint32_t size, uint32_t out_addr,
                            HRESULT hr, void *host_ptr)
{
    if (FAILED(hr) || !host_ptr) {
        if (out_addr) guest_st(c, out_addr, 4, 0);
        return (uint32_t)hr;
    }
    uint32_t buf = stage_alloc(size);
    lock_rec r{o, 0, buf, size, 1, size, static_cast<uint8_t *>(host_ptr)};
    (void)off;
    copy_rows(&r, true);
    locks.push_back(r);
    if (out_addr) guest_st(c, out_addr, 4, buf);
    return (uint32_t)hr;
}

static uint32_t unlock_buffer(rt_com_obj *o, lock_rec *r)
{
    if (r) {
        copy_rows(r, false);
        release_lock(r);
    }
    (void)o;
    return 0;
}

extern "C" uint32_t custom_IDirect3DVertexBuffer9_Lock(cpu_t *c, const uint32_t *a)
{
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    rt_com_obj *o = rt_com_obj_of(a[0]);
    uint32_t size = a[2] ? a[2] : o->extra - a[1];
    void *p = nullptr;
    HRESULT hr = self->Lock(a[1], a[2], &p, a[4]);
    return lock_buffer(c, o, a[1], size, a[3], hr, p);
}

extern "C" uint32_t custom_IDirect3DVertexBuffer9_Unlock(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    rt_com_obj *o = rt_com_obj_of(a[0]);
    unlock_buffer(o, find_lock(o, 0));
    self->Unlock();
    return 0;
}

extern "C" uint32_t custom_IDirect3DIndexBuffer9_Lock(cpu_t *c, const uint32_t *a)
{
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    rt_com_obj *o = rt_com_obj_of(a[0]);
    uint32_t size = a[2] ? a[2] : o->extra - a[1];
    void *p = nullptr;
    HRESULT hr = self->Lock(a[1], a[2], &p, a[4]);
    return lock_buffer(c, o, a[1], size, a[3], hr, p);
}

extern "C" uint32_t custom_IDirect3DIndexBuffer9_Unlock(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    rt_com_obj *o = rt_com_obj_of(a[0]);
    unlock_buffer(o, find_lock(o, 0));
    self->Unlock();
    return 0;
}

// ---- Surfaces et textures -------------------------------------------------

static uint32_t lock_image(cpu_t *c, rt_com_obj *o, uint32_t sub, D3DFORMAT fmt, uint32_t w, uint32_t h,
                           uint32_t out_addr, const RECT *rect, HRESULT hr, D3DLOCKED_RECT &hl)
{
    const uint32_t dxt = o->dxt;
    if (dxt) fmt = (D3DFORMAT)dxt;
    if (FAILED(hr) || !hl.pBits) {
        if (out_addr) {
            guest_st(c, out_addr + 0, 4, 0);
            guest_st(c, out_addr + 4, 4, 0);
        }
        return (uint32_t)hr;
    }
    uint32_t rows = 0, rowbytes = 0;
    region_of(fmt, w, h, rect, (uint32_t)hl.Pitch, rows, rowbytes);
    uint32_t size = (uint32_t)hl.Pitch * rows;
    uint32_t buf = stage_alloc(size);
    lock_rec r{o, sub, buf, (uint32_t)hl.Pitch, rows, rowbytes, static_cast<uint8_t *>(hl.pBits)};
    r.dxt = dxt;
    r.rw = rect ? (uint32_t)(rect->right - rect->left) : w;
    r.rh = rect ? (uint32_t)(rect->bottom - rect->top) : h;
    if (dxt) {
        memset(rt_mem_base + buf, 0, size);
    } else {
        copy_rows(&r, true);
    }
    locks.push_back(r);
    if (out_addr) {
        guest_st(c, out_addr + 0, 4, (uint32_t)hl.Pitch);
        guest_st(c, out_addr + 4, 4, buf);
    }
    return (uint32_t)hr;
}

static uint32_t unlock_image(rt_com_obj *o, uint32_t sub)
{
    lock_rec *r = find_lock(o, sub);
    if (getenv("CT3_LOCKDBG") && r) {
        uint32_t nz = 0;
        for (uint32_t k = 0; k < r->rows * r->pitch; k += 4) nz += rt_mem_base[r->buf + k] != 0;
        fprintf(stderr, "[unlock] o=%p host=%p sub=%u buf=0x%x pitch=%u rows=%u nz=%u dxt=%u\n", (void *)o, (void *)o->host, sub, r->buf, r->pitch, r->rows, nz, r->dxt);
    }
    if (r) {
        if (r->dxt) {
            decode_dxt(r);
        } else {
            copy_rows(r, false);
        }
        release_lock(r);
    }
    return 0;
}

extern "C" uint32_t custom_IDirect3DSurface9_LockRect(cpu_t *c, const uint32_t *a)
{
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    rt_com_obj *o = rt_com_obj_of(a[0]);
    D3DSURFACE_DESC d;
    memset(&d, 0, sizeof d);
    self->GetDesc(&d);
    D3DLOCKED_RECT hl;
    memset(&hl, 0, sizeof hl);
    HRESULT hr = self->LockRect(&hl, static_cast<const RECT *>(rt_gptr(a[2])), a[3]);
    return lock_image(c, o, 0, d.Format, d.Width, d.Height, a[1], static_cast<const RECT *>(rt_gptr(a[2])), hr, hl);
}

extern "C" uint32_t custom_IDirect3DSurface9_UnlockRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    unlock_image(rt_com_obj_of(a[0]), 0);
    return (uint32_t)self->UnlockRect();
}

// Texture : si l'hôte refuse un format BC (GPU sans compression BC, ex. llvmpipe), une texture
// X8R8G8B8 la remplace ; les blocs écrits par le code invité sont décodés au déverrouillage.
extern "C" uint32_t custom_IDirect3DDevice9_CreateTexture(cpu_t *c, const uint32_t *a)
{
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    D3DFORMAT fmt = (D3DFORMAT)a[5];
    HANDLE *shared = nullptr;  // ressources partagées non utilisées par CT3
    IDirect3DTexture9 *tex = nullptr;
    HRESULT hr = self->CreateTexture(a[1], a[2], a[3], a[4], fmt, (D3DPOOL)a[6], &tex, shared);
    uint32_t dxt = 0;
    if (FAILED(hr) && is_dxt(fmt)) {
        hr = self->CreateTexture(a[1], a[2], a[3], a[4], D3DFMT_X8R8G8B8, (D3DPOOL)a[6], &tex, shared);
        if (SUCCEEDED(hr)) dxt = fmt;
    }
    if (SUCCEEDED(hr) && tex) {
        uint32_t g = rt_com_wrap(tex, rt_com_iface_index("IDirect3DTexture9"));
        rt_com_obj_of(g)->dxt = dxt;
        if (a[7]) guest_st(c, a[7], 4, g);
    } else if (a[7]) {
        guest_st(c, a[7], 4, 0);
    }
    return (uint32_t)hr;
}

extern "C" uint32_t custom_IDirect3DTexture9_LockRect(cpu_t *c, const uint32_t *a)
{
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    rt_com_obj *o = rt_com_obj_of(a[0]);
    D3DSURFACE_DESC d;
    memset(&d, 0, sizeof d);
    self->GetLevelDesc(a[1], &d);
    D3DLOCKED_RECT hl;
    memset(&hl, 0, sizeof hl);
    const RECT *rect = static_cast<const RECT *>(rt_gptr(a[3]));
    HRESULT hr = self->LockRect(a[1], &hl, rect, a[4]);
    return lock_image(c, o, a[1], d.Format, d.Width, d.Height, a[2], rect, hr, hl);
}

extern "C" uint32_t custom_IDirect3DTexture9_UnlockRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    unlock_image(rt_com_obj_of(a[0]), a[1]);
    return (uint32_t)self->UnlockRect(a[1]);
}

extern "C" uint32_t custom_IDirect3DCubeTexture9_LockRect(cpu_t *c, const uint32_t *a)
{
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    rt_com_obj *o = rt_com_obj_of(a[0]);
    D3DSURFACE_DESC d;
    memset(&d, 0, sizeof d);
    self->GetLevelDesc(a[2], &d);
    D3DLOCKED_RECT hl;
    memset(&hl, 0, sizeof hl);
    const RECT *rect = static_cast<const RECT *>(rt_gptr(a[4]));
    HRESULT hr = self->LockRect((D3DCUBEMAP_FACES)a[1], a[2], &hl, rect, a[5]);
    return lock_image(c, o, (a[1] << 16) | a[2], d.Format, d.Width, d.Height, a[3], rect, hr, hl);
}

extern "C" uint32_t custom_IDirect3DCubeTexture9_UnlockRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    unlock_image(rt_com_obj_of(a[0]), (a[1] << 16) | a[2]);
    return (uint32_t)self->UnlockRect((D3DCUBEMAP_FACES)a[1], a[2]);
}

// Volumes : non pris en charge pour l'instant (aucun usage observé dans CT3 au démarrage).

extern "C" uint32_t custom_IDirect3DVolumeTexture9_LockBox(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    fprintf(stderr, "rt: IDirect3DVolumeTexture9::LockBox non pris en charge\n");
    return (uint32_t)E_NOTIMPL;
}

extern "C" uint32_t custom_IDirect3DVolumeTexture9_UnlockBox(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return (uint32_t)E_NOTIMPL;
}

extern "C" uint32_t custom_IDirect3DVolume9_LockBox(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    fprintf(stderr, "rt: IDirect3DVolume9::LockBox non pris en charge\n");
    return (uint32_t)E_NOTIMPL;
}

extern "C" uint32_t custom_IDirect3DVolume9_UnlockBox(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return (uint32_t)E_NOTIMPL;
}

// ---- Présentation : capture de diagnostic ----------------------------------
// CT3_SHOT=N (variable d'environnement) : écrit le back buffer de la N-ième présentation dans
// $CT3_SHOT_DIR/frame_N.ppm (répertoire /tmp/opencode par défaut). Sans effet sinon.

namespace {
uint32_t present_count;

void dump_backbuffer(IDirect3DDevice9 *dev, uint32_t frame)
{
    IDirect3DSurface9 *bb = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return;
    D3DSURFACE_DESC d;
    memset(&d, 0, sizeof d);
    bb->GetDesc(&d);
    IDirect3DSurface9 *sys = nullptr;
    if (SUCCEEDED(dev->CreateOffscreenPlainSurface(d.Width, d.Height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                    &sys, nullptr)) && sys) {
        if (SUCCEEDED(dev->GetRenderTargetData(bb, sys))) {
            D3DLOCKED_RECT lr;
            memset(&lr, 0, sizeof lr);
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                const char *dir = getenv("CT3_SHOT_DIR");
                char path[1024];
                snprintf(path, sizeof path, "%s/frame_%u.ppm", dir ? dir : "/tmp/opencode/ct3", frame);
                if (FILE *f = fopen(path, "wb")) {
                    fprintf(f, "P6\n%u %u\n255\n", d.Width, d.Height);
                    for (uint32_t y = 0; y < d.Height; y++) {
                        const uint8_t *row = static_cast<const uint8_t *>(lr.pBits) + (size_t)y * lr.Pitch;
                        for (uint32_t x = 0; x < d.Width; x++) {
                            const uint8_t *px = row + 4 * x;  // B, G, R, X
                            uint8_t rgb[3] = {px[2], px[1], px[0]};
                            fwrite(rgb, 1, 3, f);
                        }
                    }
                    fclose(f);
                    fprintf(stderr, "rt: capture %s (%ux%u)\n", path, d.Width, d.Height);
                }
                sys->UnlockRect();
            }
        }
        sys->Release();
    }
    bb->Release();
}
}  // namespace

extern "C" uint32_t custom_IDirect3DDevice9_Present(cpu_t *c, const uint32_t *a)
{
    rt_probe_present();
    {
        /* CT3_TRACE_FROM=N, CT3_TRACE_TO=M : trace des imports seulement entre les présentations N et M. */
        static long tr_from = -2, tr_to = -1;
        if (tr_from == -2) {
            const char *f = getenv("CT3_TRACE_FROM"), *t = getenv("CT3_TRACE_TO");
            tr_from = f ? atol(f) : -1;
            tr_to = t ? atol(t) : -1;
        }
        if (tr_from >= 0) {
            if ((long)present_count + 1 == tr_from) rt_trace_imports = 1;
            if ((long)present_count + 1 == tr_to) rt_trace_imports = 0;
        }
    }
    /* CT3_SHOT_PRE=1 : capture avant Present (le contenu du back buffer n'est pas garanti après Present). */
    if (getenv("CT3_SHOT_PRE")) {
        const char *every0 = getenv("CT3_SHOT_EVERY");
        uint32_t next = present_count + 1;
        if (every0 && atoi(every0) > 0 && next % (uint32_t)atoi(every0) == 0) dump_backbuffer(rt_self<IDirect3DDevice9>(a[0]), next);
    }
    {
        static long key_at = -2;
        if (key_at == -2) {
            const char *k = getenv("CT3_KEY_AT");
            key_at = k ? atol(k) : -1;
        }
        if (key_at >= 0 && present_count == key_at) rt_win_test_key(1);
        if (key_at >= 0 && present_count == key_at + 20) rt_win_test_key(0);
        static long key_at2 = -2;
        if (key_at2 == -2) {
            const char *k = getenv("CT3_KEY_AT2");
            key_at2 = k ? atol(k) : -1;
        }
        if (key_at2 >= 0 && present_count == key_at2) rt_win_test_key(1);
        if (key_at2 >= 0 && present_count == key_at2 + 20) rt_win_test_key(0);
        /* CT3_KEYS=N1,N2,... : appuis sur Entrée aux présentations N1, N2, ... (maintenus 20 présentations). */
        static std::vector<long> keys;
        static int keys_init = 0;
        if (!keys_init) {
            keys_init = 1;
            const char *ks = getenv("CT3_KEYS");
            while (ks && *ks) {
                char *end = nullptr;
                long v = strtol(ks, &end, 10);
                if (end == ks) break;
                keys.push_back(v);
                ks = (*end == ',') ? end + 1 : end;
            }
        }
        for (long k : keys) {
            if ((long)present_count == k) rt_win_test_key(1);
            if ((long)present_count == k + 20) rt_win_test_key(0);
        }
    }
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT hr = self->Present(static_cast<const RECT *>(rt_gptr(a[1])), static_cast<const RECT *>(rt_gptr(a[2])),
                               rt_hwnd(a[3]), static_cast<const RGNDATA *>(rt_gptr(a[4])));
    present_count++;
    const char *shot = getenv("CT3_SHOT");
    if (shot && present_count == (uint32_t)atoi(shot)) dump_backbuffer(self, present_count);
    /* CT3_SHOT_EVERY=N : une capture toutes les N présentations (suivi des écrans, diagnostic). */
    const char *every = getenv("CT3_SHOT_EVERY");
    const char *every_from = getenv("CT3_SHOT_FROM");
    if (every && atoi(every) > 0 && present_count % (uint32_t)atoi(every) == 0
        && present_count >= (uint32_t)(every_from ? atoi(every_from) : 0))
        dump_backbuffer(self, present_count);
    return (uint32_t)hr;
}

// ---- Création et réinitialisation : fenêtre à la taille du back buffer -----

extern "C" uint32_t custom_IDirect3D9_CreateDevice(cpu_t *c, const uint32_t *a)
{
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    D3DPRESENT_PARAMETERS pp;
    if (a[5]) rt_pp_in(a[5], pp);
    IDirect3DDevice9 *dev = nullptr;
    /* La swapchain est créée avec la taille de la fenêtre : la fenêtre est dimensionnée d'abord. */
    if (a[5]) rt_win_fit(a[3], pp.BackBufferWidth, pp.BackBufferHeight, pp.Windowed != 0);
    HRESULT hr = self->CreateDevice(a[1], (D3DDEVTYPE)a[2], rt_hwnd(a[3]), a[4], a[5] ? &pp : nullptr, &dev);
    rt_com_out(a[6], dev, rt_com_iface_index("IDirect3DDevice9"));
    (void)c;
    return (uint32_t)hr;
}

extern "C" uint32_t custom_IDirect3DDevice9_Reset(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    D3DPRESENT_PARAMETERS pp;
    if (a[1]) rt_pp_in(a[1], pp);
    if (a[1]) {
        D3DDEVICE_CREATION_PARAMETERS cp;
        if (SUCCEEDED(self->GetCreationParameters(&cp))) {
            rt_win_fit(rt_win_guest(cp.hFocusWindow), pp.BackBufferWidth, pp.BackBufferHeight, pp.Windowed != 0);
        }
    }
    return (uint32_t)self->Reset(a[1] ? &pp : nullptr);
}
