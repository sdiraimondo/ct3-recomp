// Pont Direct3D 9 : voir runtime/include/rt_d3d9.h.

#include "rt_d3d9.h"
#include "rt_proc.h"
#include "rt_win.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace {

// Identifiant 0 réservé : un objet invité porte l'identifiant (index dans objs) à l'offset 4.
std::vector<rt_com_obj *> objs(1, nullptr);
std::unordered_map<void *, rt_com_obj *> by_host;
uint32_t vtable_guest[64];

void put32(uint32_t addr, uint32_t v)
{
    memcpy(rt_mem_base + addr, &v, 4);
}

uint32_t get32(uint32_t addr)
{
    uint32_t v;
    memcpy(&v, rt_mem_base + addr, 4);
    return v;
}

uint32_t vtable_of(int ifidx)
{
    if (!vtable_guest[ifidx]) {
        const rt_com_iface &ifc = rt_com_ifaces[ifidx];
        uint32_t vt = rt_heap_alloc(4 * ifc.count, 1);
        for (unsigned k = 0; k < ifc.count; k++) {
            put32(vt + 4 * k, rt_dyn_stub_addr(ifc.methods[k].name));
        }
        vtable_guest[ifidx] = vt;
    }
    return vtable_guest[ifidx];
}

}  // namespace

extern "C" {

int rt_com_iface_index(const char *name)
{
    for (unsigned i = 0; i < rt_com_iface_count; i++) {
        if (strcmp(rt_com_ifaces[i].name, name) == 0) return (int)i;
    }
    return -1;
}

uint32_t rt_com_wrap(void *host, int ifidx)
{
    if (!host) return 0;
    auto it = by_host.find(host);
    if (it != by_host.end()) return it->second->guest;
    rt_com_obj *o = new rt_com_obj{host, ifidx, 0, 0, 0};
    o->guest = rt_heap_alloc(8, 1);
    put32(o->guest, vtable_of(ifidx));
    put32(o->guest + 4, (uint32_t)objs.size());
    objs.push_back(o);
    by_host[host] = o;
    return o->guest;
}

void rt_com_forget(void *host)
{
    auto it = by_host.find(host);
    if (it == by_host.end()) return;
    it->second->host = nullptr;
    by_host.erase(it);
}

void *rt_com_host(uint32_t guest)
{
    if (!guest) return nullptr;
    uint32_t id = get32(guest + 4);
    if (id == 0 || id >= objs.size()) {
        fprintf(stderr, "rt: objet COM invité inconnu 0x%08x\n", guest);
        exit(3);
    }
    return objs[id]->host;
}

rt_com_obj *rt_com_obj_of(uint32_t guest)
{
    if (!guest) return nullptr;
    uint32_t id = get32(guest + 4);
    return (id == 0 || id >= objs.size()) ? nullptr : objs[id];
}

void rt_com_out(uint32_t guest_slot, void *host, int ifidx)
{
    if (!guest_slot) return;
    put32(guest_slot, rt_com_wrap(host, ifidx));
}

HWND rt_hwnd(uint32_t guest)
{
    return static_cast<HWND>(rt_win_host(guest));
}

float rt_u2f(uint32_t v)
{
    float f;
    memcpy(&f, &v, 4);
    return f;
}

uint32_t rt_d3d9_create(uint32_t sdk_version)
{
    rt_win_init();
    IDirect3D9 *d3d = Direct3DCreate9(sdk_version);
    if (!d3d) return 0;
    return rt_com_wrap(d3d, rt_com_iface_index("IDirect3D9"));
}

}  // extern "C"

// Layout invité (MSVC, 32 bits) : tous les champs font 4 octets, HWND compris.
void rt_pp_in(uint32_t guest, D3DPRESENT_PARAMETERS &out)
{
    memset(&out, 0, sizeof out);
    out.BackBufferWidth = get32(guest + 0);
    out.BackBufferHeight = get32(guest + 4);
    out.BackBufferFormat = (D3DFORMAT)get32(guest + 8);
    out.BackBufferCount = get32(guest + 12);
    out.MultiSampleType = (D3DMULTISAMPLE_TYPE)get32(guest + 16);
    out.MultiSampleQuality = get32(guest + 20);
    out.SwapEffect = (D3DSWAPEFFECT)get32(guest + 24);
    out.hDeviceWindow = rt_hwnd(get32(guest + 28));
    out.Windowed = (WINBOOL)get32(guest + 32);
    out.EnableAutoDepthStencil = (WINBOOL)get32(guest + 36);
    out.AutoDepthStencilFormat = (D3DFORMAT)get32(guest + 40);
    out.Flags = get32(guest + 44);
    out.FullScreen_RefreshRateInHz = get32(guest + 48);
    out.PresentationInterval = get32(guest + 52);
}
