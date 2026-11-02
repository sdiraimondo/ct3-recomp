// Talons GDI32, ADVAPI32, ole32 et petites fonctions KERNEL32 (voir rt_shims.c pour les autres).
//
// GDI : les objets (pinceaux, polices, DC, DIB) sont des handles fictifs. Le texte n'est pas encore
// dessiné : ExtTextOutA ne fait rien (signalé une fois). Les DIB sections sont de vraies zones de
// mémoire invitée, pour un rendu futur.

#include "rt_proc.h"
#include "rt_shim.h"
#include "rt_font.h"
#include "rt_win.h"

#include <SDL.h>

#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace {

constexpr uint32_t kStockBase = 0x00040000u;
constexpr uint32_t kDcBase = 0x00050000u;
constexpr uint32_t kObjBase = 0x00060000u;
uint32_t next_obj = kObjBase;
bool warned_text;

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

std::string cstr(uint32_t addr, size_t max = 4096)
{
    std::string s;
    if (!addr) return s;
    for (size_t i = 0; i < max; i++) {
        char ch = (char)rt_mem_base[addr + i];
        if (!ch) break;
        s.push_back(ch);
    }
    return s;
}

void put_cstr(uint32_t dst, const std::string &s, uint32_t max)
{
    if (!max) return;
    uint32_t n = (uint32_t)s.size() < max - 1 ? (uint32_t)s.size() : max - 1;
    memcpy(rt_mem_base + dst, s.data(), n);
    rt_mem_base[dst + n] = 0;
}

uint32_t new_obj()
{
    return next_obj += 0x10;
}

// ---- GDI ------------------------------------------------------------------
// Texte : polices hôte (rt_font.c) dessinées dans la mémoire du DIB sélectionné dans le DC. Les pinceaux et
// le reste restent des handles fictifs.

struct GdiFont {
    int font;   // index dans rt_font
    float px;   // taille em en pixels
};
struct GdiBitmap {
    uint32_t bits;  // adresse invitée des pixels (DIB section)
    int32_t w, h;
    uint16_t bpp;
};
struct GdiDc {
    uint32_t bitmap, font, text_color, bk_color, text_align;
};
std::map<uint32_t, GdiFont> gdi_fonts;
std::map<uint32_t, GdiBitmap> gdi_bitmaps;
std::map<uint32_t, GdiDc> gdi_dcs;

GdiFont default_font()
{
    static GdiFont f = {-1, 16.0f};
    if (f.font < 0) f.font = rt_font_open("Arial", 0, 0);
    return f;
}

GdiFont font_of(uint32_t hfont)
{
    auto it = gdi_fonts.find(hfont);
    if (it != gdi_fonts.end() && it->second.font >= 0) return it->second;
    return default_font();
}

GdiDc dc_of(uint32_t hdc)
{
    auto it = gdi_dcs.find(hdc);
    if (it != gdi_dcs.end()) return it->second;
    return GdiDc{0, 0, 0x000000u, 0xFFFFFFu, 0};
}

uint32_t sh_GetStockObject(cpu_t *c, const uint32_t *a) { (void)c; return kStockBase + a[0]; }
uint32_t sh_CreateCompatibleDC(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    uint32_t h = new_obj();
    gdi_dcs[h] = GdiDc{0, 0, 0x000000u, 0xFFFFFFu, 0};
    return h;
}
uint32_t sh_DeleteDC(cpu_t *c, const uint32_t *a) { (void)c; gdi_dcs.erase(a[0]); return 1; }
uint32_t sh_DeleteObject(cpu_t *c, const uint32_t *a)
{
    (void)c;
    gdi_fonts.erase(a[0]);
    gdi_bitmaps.erase(a[0]);
    return 1;
}
uint32_t sh_SelectObject(cpu_t *c, const uint32_t *a)
{
    (void)c;
    GdiDc &dc = gdi_dcs[a[0]];
    if (gdi_bitmaps.count(a[1])) {
        uint32_t old = dc.bitmap;
        dc.bitmap = a[1];
        return old ? old : kObjBase;
    }
    if (gdi_fonts.count(a[1])) {
        uint32_t old = dc.font;
        dc.font = a[1];
        return old ? old : kObjBase;
    }
    return kObjBase;
}
uint32_t sh_SetBkColor(cpu_t *c, const uint32_t *a)
{
    (void)c;
    GdiDc &dc = gdi_dcs[a[0]];
    uint32_t old = dc.bk_color;
    dc.bk_color = a[1];
    return old;
}
uint32_t sh_SetTextColor(cpu_t *c, const uint32_t *a)
{
    (void)c;
    GdiDc &dc = gdi_dcs[a[0]];
    uint32_t old = dc.text_color;
    dc.text_color = a[1];
    return old;
}
uint32_t sh_SetTextAlign(cpu_t *c, const uint32_t *a)
{
    (void)c;
    GdiDc &dc = gdi_dcs[a[0]];
    uint32_t old = dc.text_align;
    dc.text_align = a[1];
    return old;
}
uint32_t sh_SetMapMode(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }

// CreateFontA(nHeight, nWidth, nEscapement, nOrientation, fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
//             fdwCharSet, fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily, lpszFace)
uint32_t sh_CreateFontA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    int32_t height = (int32_t)a[0];
    float px = height < 0 ? (float)-height : (height > 0 ? (float)height : 16.0f);
    int bold = (int32_t)a[4] >= 600;
    int italic = a[5] != 0;
    std::string face = cstr(a[13], 32);
    bool readable = !face.empty();
    for (unsigned char ch : face) {
        if (!isprint(ch)) readable = false;
    }
    if (!readable) face = "Arial";
    GdiFont f = {rt_font_open(face.c_str(), bold, italic), px};
    uint32_t h = new_obj();
    gdi_fonts[h] = f;
    return h;
}

uint32_t sh_GetDeviceCaps(cpu_t *c, const uint32_t *a)
{
    (void)c;
    switch (a[1]) {
    case 8: return 1280;   // HORZRES
    case 10: return 720;   // VERTRES
    case 12: return 32;    // BITSPIXEL
    case 14: return 1;     // PLANES
    case 24: return 0;     // NUMCOLORS (couleurs réelles)
    case 88: return 96;    // LOGPIXELSX
    case 90: return 96;    // LOGPIXELSY
    default: return 0;
    }
}

// Largeur d'une chaîne (lpDx facultatif : distances par caractère).
float text_width(const std::string &text, GdiFont gf, uint32_t dx)
{
    float total = 0;
    for (size_t i = 0; i < text.size(); i++) {
        total += dx ? (float)(int32_t)get32(dx + 4 * (uint32_t)i)
                    : rt_font_advance(gf.font, gf.px, (unsigned char)text[i]);
    }
    return total;
}

std::string read_text(uint32_t addr, uint32_t count)
{
    std::string s;
    if (!addr) return s;
    if (count > 1024) count = 1024;
    s.assign((const char *)rt_mem_base + addr, count);
    return s;
}

uint32_t sh_GetTextExtentPoint32A(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // (hdc, lpString, c, lpSize)
    GdiDc dc = dc_of(a[0]);
    GdiFont gf = font_of(dc.font);
    int asc = 0, desc = 0;
    rt_font_metrics(gf.font, gf.px, &asc, &desc);
    std::string text = read_text(a[1], a[2]);
    put32(a[3] + 0, (uint32_t)(int32_t)(text_width(text, gf, 0) + 0.5f));
    put32(a[3] + 4, (uint32_t)(asc + desc));
    return 1;
}

// Écrit un pixel (fond opaque ou couverture de glyphe) dans un DIB 32 ou 24 bits.
void dib_blend(const GdiBitmap &bm, int x, int y, uint32_t color, unsigned alpha)
{
    int h = bm.h < 0 ? -bm.h : bm.h;
    if (x < 0 || y < 0 || x >= bm.w || y >= h || alpha == 0) return;
    uint32_t row = bm.h > 0 ? (uint32_t)(h - 1 - y) : (uint32_t)y;
    uint8_t src[3] = {(uint8_t)(color >> 16), (uint8_t)(color >> 8), (uint8_t)color};  // BGR
    if (bm.bpp == 32) {
        uint8_t *p = rt_mem_base + bm.bits + row * (uint32_t)bm.w * 4u + (uint32_t)x * 4u;
        for (int k = 0; k < 3; k++) p[k] = (uint8_t)((p[k] * (255 - alpha) + src[k] * alpha + 127) / 255);
    } else if (bm.bpp == 24) {
        uint32_t pitch = ((uint32_t)bm.w * 3u + 3u) & ~3u;
        uint8_t *p = rt_mem_base + bm.bits + row * pitch + (uint32_t)x * 3u;
        for (int k = 0; k < 3; k++) p[k] = (uint8_t)((p[k] * (255 - alpha) + src[k] * alpha + 127) / 255);
    } else if (bm.bpp == 1) {
        /* DIB monochrome (atlas de police) : le bit est allumé si la couleur est claire, sur les pixels couverts
         * à plus de 50 %. Lignes alignées sur 32 bits, bit de poids fort en premier. */
        if (alpha < 128) return;
        uint32_t pitch = (((uint32_t)bm.w + 31u) / 32u) * 4u;
        uint8_t *p = rt_mem_base + bm.bits + row * pitch + ((uint32_t)x >> 3);
        uint8_t mask = (uint8_t)(0x80u >> ((uint32_t)x & 7u));
        uint32_t lum = (src[2] * 299u + src[1] * 587u + src[0] * 114u) / 1000u;
        if (lum >= 128) *p |= mask;
        else *p &= (uint8_t)~mask;
    }
}

uint32_t sh_ExtTextOutA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    static unsigned ext_count;
    static const char *dump = getenv("CT3_ATLAS_DUMP");
    ext_count++;
    // (hdc, x, y, fuOptions, lprect, lpString, c, lpDx)
    GdiDc dc = dc_of(a[0]);
    auto bit = gdi_bitmaps.find(dc.bitmap);
    if (bit == gdi_bitmaps.end()) return 1;  // rien à dessiner : aucun DIB sélectionné
    const GdiBitmap &bm = bit->second;
    GdiFont gf = font_of(dc.font);
    int asc = 0, desc = 0;
    rt_font_metrics(gf.font, gf.px, &asc, &desc);
    std::string text = read_text(a[5], a[6]);
    int32_t x = (int32_t)a[1], y = (int32_t)a[2];
    if (getenv("CT3_GDIDBG")) fprintf(stderr, "[gdi] texte '%s' x=%d y=%d opts=0x%x dib=%dx%d bpp=%u\n", text.c_str(), x, y, a[3], bm.w, bm.h, bm.bpp);
    static bool dumped;
    static unsigned atlas_draws;
    atlas_draws++;
    if (dump && !dumped && atlas_draws >= (unsigned)atoi(getenv("CT3_ATLAS_N") ? getenv("CT3_ATLAS_N") : "95") && bm.bpp == 1) {
        dumped = true;
        /* Diagnostic : atlas monochrome écrit en PGM (P5) pour contrôle visuel. */
        int h = bm.h < 0 ? -bm.h : bm.h;
        if (FILE *f = fopen(dump, "wb")) {
            fprintf(f, "P5\n%d %d\n255\n", bm.w, h);
            uint32_t pitch = (((uint32_t)bm.w + 31u) / 32u) * 4u;
            for (int y2 = 0; y2 < h; y2++) {
                uint32_t row = bm.h > 0 ? (uint32_t)(h - 1 - y2) : (uint32_t)y2;
                for (int x2 = 0; x2 < bm.w; x2++) {
                    uint8_t byte = rt_mem_base[bm.bits + row * pitch + ((uint32_t)x2 >> 3)];
                    uint8_t v = (byte >> (7 - (x2 & 7))) & 1 ? 255 : 0;
                    fputc(v, f);
                }
            }
            fclose(f);
        }
    }
    float width = text_width(text, gf, a[7]);
    int left = x;
    if ((dc.text_align & 6) == 6) left = x - (int)(width / 2 + 0.5f);       // TA_CENTER
    else if ((dc.text_align & 6) == 2) left = x - (int)(width + 0.5f);      // TA_RIGHT
    int baseline = y + asc;
    if (dc.text_align & 24) baseline = y;                                    // TA_BASELINE
    else if (dc.text_align & 8) baseline = y - desc;                        // TA_BOTTOM
    if ((a[3] & 2) && a[4]) {                                               // ETO_OPAQUE
        int32_t r[4];
        for (int k = 0; k < 4; k++) r[k] = (int32_t)get32(a[4] + 4 * k);
        for (int32_t py = r[1]; py < r[3]; py++)
            for (int32_t px = r[0]; px < r[2]; px++) dib_blend(bm, px, py, dc.bk_color, 255);
    }
    float pen = (float)left;
    for (size_t i = 0; i < text.size(); i++) {
        unsigned cp = (unsigned char)text[i];
        int gw = 0, gh = 0, xo = 0, yo = 0;
        unsigned char *cov = rt_font_glyph(gf.font, gf.px, cp, &gw, &gh, &xo, &yo);
        int ox = (int)(pen + 0.5f);
        if (cov) {
            for (int gy = 0; gy < gh; gy++)
                for (int gx = 0; gx < gw; gx++)
                    dib_blend(bm, ox + xo + gx, baseline + yo + gy, dc.text_color, cov[gy * gw + gx]);
            rt_font_glyph_free(cov);
        }
        pen += a[7] ? (float)(int32_t)get32(a[7] + 4 * (uint32_t)i) : rt_font_advance(gf.font, gf.px, cp);
    }
    return 1;
}

uint32_t sh_GetObjectA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;
}

uint32_t sh_GetDIBits(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;
}

// BITMAPINFOHEADER : biWidth (+4), biHeight (+8), biBitCount (+14).
uint32_t sh_CreateDIBSection(cpu_t *c, const uint32_t *a)
{
    // (hdc, pbmi, usage, ppvBits, hSection, offset)
    (void)c;
    int32_t w = (int32_t)get32(a[1] + 4);
    int32_t h = (int32_t)get32(a[1] + 8);
    uint16_t bpp = (uint16_t)(get32(a[1] + 12) & 0xFFFF);
    uint32_t size = (uint32_t)(w < 0 ? -w : w) * (uint32_t)(h < 0 ? -h : h) * ((bpp + 7) / 8);
    uint32_t bits = rt_heap_alloc(size ? size : 1, 1);
    if (a[3]) put32(a[3], bits);
    uint32_t hbm = new_obj();
    gdi_bitmaps[hbm] = GdiBitmap{bits, w, h, bpp};
    return hbm;
}

// ---- ADVAPI32 / ole32 -----------------------------------------------------

uint32_t sh_RegOpenKeyA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 2;  // ERROR_FILE_NOT_FOUND : clé absente
}

uint32_t sh_RegQueryValueExA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 2;
}

uint32_t sh_RegCloseKey(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;
}

uint32_t sh_ok0(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;  // S_OK
}

uint32_t sh_zero(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;
}

uint32_t sh_one(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 1;
}

uint32_t sh_CoTaskMemAlloc(cpu_t *c, const uint32_t *a)
{
    (void)c;
    return rt_heap_alloc(a[0] ? a[0] : 1, 0);
}

// ---- KERNEL32 : chaînes, arithmétique, informations ----------------------

uint32_t sh_lstrlenA(cpu_t *c, const uint32_t *a) { (void)c; return (uint32_t)cstr(a[0]).size(); }

uint32_t sh_lstrcpyA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    put_cstr(a[0], cstr(a[1]), 1u << 20);
    return a[0];
}

uint32_t sh_lstrcpynA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[2] > 0) put_cstr(a[0], cstr(a[1]), a[2]);
    return a[0];
}

uint32_t sh_lstrcatA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::string dst = cstr(a[0]);
    dst += cstr(a[1]);
    put_cstr(a[0], dst, 1u << 20);
    return a[0];
}

uint32_t sh_lstrcmpA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    int r = strcmp(cstr(a[0]).c_str(), cstr(a[1]).c_str());
    return r < 0 ? (uint32_t)-1 : r > 0 ? 1u : 0u;
}

uint32_t sh_lstrcmpiA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::string x = cstr(a[0]), y = cstr(a[1]);
    for (auto &ch : x) ch = (char)tolower((unsigned char)ch);
    for (auto &ch : y) ch = (char)tolower((unsigned char)ch);
    int r = strcmp(x.c_str(), y.c_str());
    return r < 0 ? (uint32_t)-1 : r > 0 ? 1u : 0u;
}

uint32_t sh_MulDiv(cpu_t *c, const uint32_t *a)
{
    (void)c;
    int64_t p = (int64_t)(int32_t)a[0] * (int64_t)(int32_t)a[1];
    int64_t d = (int32_t)a[2];
    if (d == 0) return (uint32_t)-1;
    int64_t q = p / d;
    return (uint32_t)(int32_t)q;
}

uint32_t sh_GetCurrentProcess(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0xFFFFFFFFu; }

uint32_t sh_GetSystemInfo(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // SYSTEM_INFO : wProcessorArchitecture(2), dwPageSize(+4), ..., dwNumberOfProcessors(+32).
    memset(rt_mem_base + a[0], 0, 36);
    put32(a[0] + 4, 4096);
    put32(a[0] + 32, 1);
    return 0;
}

uint32_t sh_OutputDebugStringA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    fprintf(stderr, "[debug] %s\n", cstr(a[0]).c_str());
    return 0;
}

uint32_t sh_GetProfileIntA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    return a[2];  // valeur par défaut
}

uint32_t sh_DebugBreak(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }

uint32_t sh_FatalAppExitA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    fprintf(stderr, "rt: FatalAppExit : %s\n", cstr(a[1]).c_str());
    exit(5);
}

uint32_t sh_LocalFree(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_FormatMessageA(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_GetUserDefaultLCID(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0x0409; }
uint32_t sh_GetLocaleInfoA(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }

uint32_t sh_ExitProcess(cpu_t *c, const uint32_t *a)
{
    (void)c;
    fflush(stdout);
    fflush(stderr);
    exit((int)a[0]);
}

uint32_t sh_VirtualQuery(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;
}

uint32_t sh_IsBadPtr(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;  // pointeur considéré valide
}

// DirectSound silencieux : objets invités à vtable de talons. Les tampons ont une mémoire réelle
// (Lock/Unlock fonctionnent), la lecture ne produit aucun son pour l'instant.
// IDirectSound : QueryInterface, AddRef, Release, CreateSoundBuffer, GetCaps, DuplicateSoundBuffer,
// SetCooperativeLevel, Compact, GetSpeakerConfig, SetSpeakerConfig, Initialize.
// IDirectSoundBuffer : QueryInterface, AddRef, Release, GetCaps, GetCurrentPosition, GetFormat,
// GetVolume, GetPan, GetFrequency, GetStatus, Initialize, Lock, Play, SetCurrentPosition, SetFormat,
// SetVolume, SetPan, SetFrequency, Stop, Unlock, Restore.
const char *const kDsMethods[] = {"QueryInterface", "AddRef", "Release", "CreateSoundBuffer", "GetCaps",
                                  "DuplicateSoundBuffer", "SetCooperativeLevel", "Compact",
                                  "GetSpeakerConfig", "SetSpeakerConfig", "Initialize"};
const char *const kDsbMethods[] = {"QueryInterface", "AddRef", "Release", "GetCaps", "GetCurrentPosition",
                                   "GetFormat", "GetVolume", "GetPan", "GetFrequency", "GetStatus",
                                   "Initialize", "Lock", "Play", "SetCurrentPosition", "SetFormat",
                                   "SetVolume", "SetPan", "SetFrequency", "Stop", "Unlock", "Restore",
                                   "SetFX", "AcquireResources", "GetObjectInPath"};  // IDirectSoundBuffer8
uint32_t ds_vtable;
uint32_t dsb_vtable;
uint32_t ds3d_vtable;
uint32_t ds3d_obj;
// IDirectSound3DBuffer : ordre de la vtable de dsound.h (index 17 = SetMinDistance, 3 arguments).
const char *const kDs3dMethods[] = {"QueryInterface", "AddRef", "Release", "GetAllParameters", "GetConeAngles",
                                    "GetConeOrientation", "GetConeOutsideVolume", "GetMaxDistance",
                                    "GetMinDistance", "GetMode", "GetPosition", "GetVelocity",
                                    "SetAllParameters", "SetConeAngles", "SetConeOrientation",
                                    "SetConeOutsideVolume", "SetMaxDistance", "SetMinDistance", "SetMode",
                                    "SetPosition", "SetVelocity"};
// {279AFA86-4981-11CE-A521-0020AF0BE560} : IID_IDirectSound3DBuffer (vérifié dans CT3.exe à 0x5CEF4C).
const uint8_t kIidDs3dBuffer[16] = {0x86, 0xFA, 0x9A, 0x27, 0x81, 0x49, 0xCE, 0x11,
                                    0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60};

uint32_t make_vtable(const char *prefix, const char *const *names, unsigned count)
{
    uint32_t vt = rt_heap_alloc(4 * count, 1);
    for (unsigned k = 0; k < count; k++) {
        std::string full = std::string(prefix) + names[k];
        uint32_t stub = rt_dyn_stub_addr(full.c_str());
        memcpy(rt_mem_base + vt + 4 * k, &stub, 4);
        if (k == 8 && getenv("CT3_SONDBG")) fprintf(stderr, "[vt] %s vt=%08x slot8=%08x\n", prefix, vt, stub);
    }
    return vt;
}

uint32_t new_object(uint32_t vtable, uint32_t extra_bytes)
{
    uint32_t obj = rt_heap_alloc(16, 1);
    put32(obj, vtable);
    put32(obj + 4, rt_heap_alloc(extra_bytes ? extra_bytes : 4, 1));  // stockage (tampon)
    put32(obj + 8, extra_bytes);
    return obj;
}

// DirectSoundCreate (DSOUND ordinal 11) : (lpGuid, ppDS, pUnkOuter)
// Formatage « à la wsprintf » : les arguments sont lus dans la liste invitée (pointeur vers des
// mots de 32 bits, %f consomme 8 octets). Les chaînes %s sont des adresses invitées.
std::string guest_format(const char *fmt, uint32_t args)
{
    std::string out;
    auto next32 = [&args]() {
        uint32_t v = get32(args);
        args += 4;
        return v;
    };
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            out.push_back(*p);
            continue;
        }
        if (p[1] == '%') {
            out.push_back('%');
            p++;
            continue;
        }
        char spec[32];
        size_t n = 0;
        spec[n++] = '%';
        const char *q = p + 1;
        while (*q && strchr("-+ #0123456789.*lhI", *q) && n < sizeof spec - 4) {
            if (*q == '*') {
                spec[n] = 0;
                n += (size_t)snprintf(spec + n, sizeof spec - n, "%d", (int)next32());
            } else if (*q != 'l' && *q != 'h' && *q != 'I') {
                spec[n++] = *q;
            }
            q++;
        }
        if (!*q) break;
        char conv = *q;
        p = q;
        char buf[512];
        switch (conv) {
        case 'd': case 'i':
            spec[n++] = 'd';
            spec[n] = 0;
            snprintf(buf, sizeof buf, spec, (int)next32());
            out += buf;
            break;
        case 'u': case 'x': case 'X': case 'o':
            spec[n++] = conv;
            spec[n] = 0;
            snprintf(buf, sizeof buf, spec, (unsigned)next32());
            out += buf;
            break;
        case 'c':
            out.push_back((char)next32());
            break;
        case 's': {
            std::string str = cstr(next32(), 4096);
            spec[n++] = 's';
            spec[n] = 0;
            snprintf(buf, sizeof buf, spec, str.c_str());
            out += buf;
            break;
        }
        case 'S': {
            // wsprintfA : %S lit une chaîne large (UTF-16) ; convertie ici en ASCII (non-ASCII : '?').
            uint32_t p = next32();
            for (unsigned k = 0; k < 4096 && p >= 0x10000u && p + 2 * k + 2 <= 0x7F000000u; k++) {
                uint16_t u;
                memcpy(&u, rt_mem_base + p + 2 * k, 2);
                if (!u) break;
                out.push_back(u < 0x80 ? (char)u : '?');
            }
            break;
        }
        case 'p':
            snprintf(buf, sizeof buf, "%08X", (unsigned)next32());
            out += buf;
            break;
        case 'f': case 'g': case 'e': {
            double d;
            memcpy(&d, rt_mem_base + args, 8);
            args += 8;
            spec[n++] = conv;
            spec[n] = 0;
            snprintf(buf, sizeof buf, spec, d);
            out += buf;
            break;
        }
        default:
            out.push_back('%');
            out.push_back(conv);
            break;
        }
    }
    return out;
}

uint32_t store_text(uint32_t dst, const std::string &text)
{
    memcpy(rt_mem_base + dst, text.data(), text.size());
    rt_mem_base[dst + text.size()] = 0;
    return (uint32_t)text.size();
}

// wvsprintfA(lpOut, lpFmt, arglist) : stdcall.
uint32_t sh_wvsprintfA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    return store_text(a[0], guest_format(cstr(a[1]).c_str(), a[2]));
}

// wsprintfA(lpOut, lpFmt, ...) : cdecl. Les arguments variadiques sont lus dans la pile invitée
// (l'appelant les dépile) : nargs = 0 dans la table.
uint32_t sh_wsprintfA(cpu_t *c, const uint32_t *a)
{
    (void)a;
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    uint32_t out = get32(esp + 4), fmt = get32(esp + 8);
    return store_text(out, guest_format(cstr(fmt).c_str(), esp + 12));
}

bool ds_audio_open();
void ds_buffer_created(uint32_t obj, uint32_t wfx);

uint32_t sh_DirectSoundCreate(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (!ds_vtable) ds_vtable = make_vtable("dsoundcom::IDirectSound::", kDsMethods, 11);
    if (!dsb_vtable) dsb_vtable = make_vtable("dsoundcom::IDirectSoundBuffer::", kDsbMethods, 24);
    if (!ds3d_vtable) ds3d_vtable = make_vtable("dsoundcom3d::IDirectSound3DBuffer::", kDs3dMethods, 21);
    ds_audio_open();
    uint32_t obj = new_object(ds_vtable, 0);
    if (a[1]) put32(a[1], obj);
    return 0;  // DS_OK
}

// CreateSoundBuffer(this, lpDSBufferDesc, lplpDsb, pUnkOuter) : dwBufferBytes en +8 du descripteur.
uint32_t sh_ds_CreateSoundBuffer(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint32_t bytes = get32(a[1] + 8);
    uint32_t obj = new_object(dsb_vtable, bytes);
    put32(a[2], obj);
    uint32_t wfx = get32(a[1] + 16);
    ds_buffer_created(obj, wfx);
    if (getenv("CT3_DSLOG")) {
        uint16_t tag = 0, ch = 0, bits = 0;
        uint32_t rate = 0;
        if (wfx) {
            memcpy(&tag, rt_mem_base + wfx, 2);
            memcpy(&ch, rt_mem_base + wfx + 2, 2);
            memcpy(&rate, rt_mem_base + wfx + 4, 4);
            memcpy(&bits, rt_mem_base + wfx + 14, 2);
        }
        fprintf(stderr, "[dsound] tampon %08x : %u octets, flags %08x, format tag %u, %u voies, %u Hz, %u bits\n",
                obj, bytes, get32(a[1] + 4), tag, ch, rate, bits);
    }
    return 0;
}

// Lock(this, dwOffset, dwBytes, ppvAudioPtr1, pdwAudioBytes1, ppvAudioPtr2, pdwAudioBytes2, dwFlags)
uint32_t sh_dsb_Lock(cpu_t *c, const uint32_t *a)
{
    (void)c;
    /* Un verrouillage qui dépasse la fin du tampon renvoie deux zones : jusqu'à la fin, puis le début.
     * DSBLOCK_ENTIREBUFFER (0x2) ignore dwBytes. Sans cela, l'écriture déborde dans le tas. */
    uint32_t storage = get32(a[0] + 4);
    uint32_t size = get32(a[0] + 8);
    uint32_t off = a[1] < size ? a[1] : 0;
    uint32_t n = ((a[7] & 2) || !a[2]) ? size - off : a[2];
    uint32_t first = n < size - off ? n : size - off;
    uint32_t rest = n - first;
    if (a[3]) put32(a[3], storage + off);
    if (a[4]) put32(a[4], first);
    if (a[5]) put32(a[5], rest ? storage : 0);
    if (a[6]) put32(a[6], rest);
    return 0;
}

// ---- Son : mixage des tampons DirectSound vers SDL (D-067) -------------------------------------
// Chaque tampon garde son format (CreateSoundBuffer, SetFormat), sa fréquence, son volume et son panoramique.
// Un fil SDL mélange les tampons en lecture vers une sortie 44,1 kHz stéréo 16 bits et fait avancer leur
// curseur : le jeu lit ainsi la progression réelle de la sortie. Sans périphérique audio, le curseur suit
// l'horloge comme avant (repli). Les tampons 3D ne reçoivent pas d'atténuation par distance pour l'instant.
struct dsb_play {
    bool playing = false;
    bool looping = false;
    uint32_t pos = 0;                  // position de lecture, en octets (avancée par le mixage)
    double frac = 0;                   // fraction de trame source (rééchantillonnage)
    uint32_t rate = 44100;             // fréquence du format (Hz)
    uint32_t freq = 0;                 // fréquence demandée (SetFrequency) ; 0 = fréquence du format
    uint16_t channels = 2, bits = 16, align = 4;
    int32_t volume = 0, pan = 0;       // centièmes de dB, comme DirectSound
    double start = 0;                  // repli horloge
    uint32_t base = 0;                 // repli horloge
};
std::map<uint32_t, dsb_play> dsb_plays;
std::mutex ds_mutex;
SDL_AudioDeviceID ds_dev = 0;
bool ds_audio_tried = false;

double mono_now()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void ds_read_format(uint32_t wfx, dsb_play &p)
{
    if (!wfx) return;
    uint16_t ch = 0, bits = 0;
    uint32_t rate = 0;
    memcpy(&ch, rt_mem_base + wfx + 2, 2);
    memcpy(&rate, rt_mem_base + wfx + 4, 4);
    memcpy(&bits, rt_mem_base + wfx + 14, 2);
    if (ch == 0 || ch > 2 || (bits != 8 && bits != 16) || rate == 0) return;  /* format non géré : défauts */
    p.channels = ch;
    p.bits = bits;
    p.rate = rate;
    p.align = (uint16_t)(ch * bits / 8);
}

/* Mixage des tampons en lecture vers la sortie (44,1 kHz stéréo). Fil SDL, sous ds_mutex. */
void ds_mix(void *, Uint8 *stream, int len)
{
    int16_t *out = (int16_t *)stream;
    int frames = len / 4;
    static std::vector<float> acc;
    acc.assign((size_t)frames * 2, 0.0f);
    {
        std::lock_guard<std::mutex> lk(ds_mutex);
        for (auto &kv : dsb_plays) {
            dsb_play &p = kv.second;
            if (!p.playing) continue;
            uint32_t storage = get32(kv.first + 4), size = get32(kv.first + 8);
            if (!storage || !size || p.align == 0) {
                p.playing = false;
                continue;
            }
            double step = (double)(p.freq ? p.freq : p.rate) / 44100.0;
            float gain = (float)pow(10.0, p.volume / 2000.0);
            float gl = gain, gr = gain;
            if (p.pan > 0) gl *= (float)pow(10.0, -p.pan / 2000.0);
            if (p.pan < 0) gr *= (float)pow(10.0, p.pan / 2000.0);
            for (int i = 0; i < frames && p.playing; i++) {
                const uint8_t *src = rt_mem_base + storage + p.pos;
                int32_t s0, s1;
                if (p.bits == 8) {
                    s0 = ((int32_t)src[0] - 128) << 8;
                    s1 = p.channels > 1 ? ((int32_t)src[1] - 128) << 8 : s0;
                } else {
                    int16_t a16, b16 = 0;
                    memcpy(&a16, src, 2);
                    if (p.channels > 1) memcpy(&b16, src + 2, 2);
                    s0 = a16;
                    s1 = p.channels > 1 ? b16 : s0;
                }
                acc[(size_t)i * 2] += s0 * gl;
                acc[(size_t)i * 2 + 1] += s1 * gr;
                p.frac += step;
                while (p.frac >= 1.0 && p.playing) {
                    p.frac -= 1.0;
                    p.pos += p.align;
                    if (p.pos >= size) {
                        if (p.looping) {
                            p.pos = 0;
                        } else {
                            p.playing = false;
                            p.pos = size;
                        }
                    }
                }
            }
        }
    }
    static double last_log = 0;
    static int active_last = 0;
    float peak = 0;
    for (int i = 0; i < frames * 2; i++) {
        float v = acc[(size_t)i];
        if (v > 32767.0f) v = 32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        out[i] = (int16_t)v;
        if (fabsf(v) > peak) peak = fabsf(v);
    }
    /* CT3_DSLOG : une ligne par seconde, avec le nombre de tampons en lecture et la crête du mélange. */
    if (getenv("CT3_DSLOG")) {
        double now = mono_now();
        if (now - last_log >= 1.0) {
            int active = 0;
            {
                std::lock_guard<std::mutex> lk(ds_mutex);
                for (auto &kv : dsb_plays) active += kv.second.playing ? 1 : 0;
            }
            fprintf(stderr, "[dsound] mixage : %d tampon(s) en lecture, crête %.0f\n", active, peak);
            last_log = now;
            active_last = active;
        }
    }
}

/* Ouvre la sortie audio une fois. Faux si elle n'est pas disponible (repli horloge). */
bool ds_audio_open()
{
    if (ds_audio_tried) return ds_dev != 0;
    ds_audio_tried = true;
    if (getenv("CT3_NOAUDIO")) return false;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "rt: audio SDL indisponible : %s\n", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = ds_mix;
    ds_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!ds_dev) {
        fprintf(stderr, "rt: sortie audio indisponible : %s\n", SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(ds_dev, 0);
    return true;
}

/* Position de lecture d'un tampon (octets). Sous ds_mutex. */
uint32_t ds_cursor_locked(uint32_t obj, dsb_play &p)
{
    uint32_t size = get32(obj + 8);
    if (ds_dev) return p.pos;
    if (!p.playing || size == 0) return p.base < size ? p.base : size;
    uint64_t pos = p.base + (uint64_t)((mono_now() - p.start) * (double)p.rate * p.align);
    if (p.looping) return (uint32_t)(pos % size);
    if (pos >= size) {
        p.playing = false;
        p.base = size;
        p.pos = size;
        return size;
    }
    return (uint32_t)pos;
}

void ds_buffer_created(uint32_t obj, uint32_t wfx)
{
    std::lock_guard<std::mutex> lk(ds_mutex);
    ds_read_format(wfx, dsb_plays[obj]);
}

uint32_t sh_dsb_GetCurrentPosition(cpu_t *c, const uint32_t *a)
{
    (void)c;
    const uint32_t kBlock = 4;
    std::lock_guard<std::mutex> lk(ds_mutex);
    dsb_play &p = dsb_plays[a[0]];
    uint32_t size = get32(a[0] + 8);
    uint32_t cur = ds_cursor_locked(a[0], p) / kBlock * kBlock;
    uint32_t wr = size ? (cur + 4096) % size / kBlock * kBlock : cur;
    if (a[1]) put32(a[1], cur);
    if (a[2]) put32(a[2], wr);
    return 0;
}

uint32_t sh_dsb_GetStatus(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    dsb_play &p = dsb_plays[a[0]];
    ds_cursor_locked(a[0], p);
    uint32_t st = p.playing ? (1u | (p.looping ? 4u : 0u)) : 0u;  // DSBSTATUS_PLAYING, DSBSTATUS_LOOPING
    if (a[1]) put32(a[1], st);
    return 0;
}

// Play(this, dwReserved1, dwPriority, dwFlags) : DSBPLAY_LOOPING = 1. Reprend au curseur courant.
uint32_t sh_dsb_Play(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    dsb_play &p = dsb_plays[a[0]];
    uint32_t size = get32(a[0] + 8);
    uint32_t cur = ds_cursor_locked(a[0], p);
    bool looping = (a[3] & 1) != 0;
    if (!looping && size && cur >= size) cur = 0;
    p.pos = cur;
    p.base = cur;
    p.looping = looping;
    p.start = mono_now();
    p.playing = true;
    if (getenv("CT3_DSLOG")) fprintf(stderr, "[dsound] Play %08x (boucle %d, curseur %u)\n", a[0], looping, cur);
    return 0;
}

uint32_t sh_dsb_Stop(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    dsb_play &p = dsb_plays[a[0]];
    p.pos = ds_cursor_locked(a[0], p);
    p.base = p.pos;
    p.playing = false;
    if (getenv("CT3_DSLOG")) fprintf(stderr, "[dsound] Stop %08x\n", a[0]);
    return 0;
}

// SetCurrentPosition(this, dwNewPosition)
uint32_t sh_dsb_SetCurrentPosition(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    dsb_play &p = dsb_plays[a[0]];
    p.pos = a[1];
    p.base = a[1];
    if (p.playing) p.start = mono_now();
    return 0;
}

// GetFormat(this, lpwfx, cbwfx, lpdwWritten) : WAVEFORMATEX PCM du tampon (voies, fréquence, bits).
uint32_t sh_dsb_GetFormat(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    const dsb_play &p = dsb_plays[a[0]];
    uint8_t wf[18] = {0};
    uint16_t tag = 1, ch = p.channels, align = p.align, bits = p.bits, cb = 0;
    uint32_t rate = p.rate, avg = p.rate * p.align;
    memcpy(wf + 0, &tag, 2);
    memcpy(wf + 2, &ch, 2);
    memcpy(wf + 4, &rate, 4);
    memcpy(wf + 8, &avg, 4);
    memcpy(wf + 12, &align, 2);
    memcpy(wf + 14, &bits, 2);
    memcpy(wf + 16, &cb, 2);
    if (a[1]) memcpy(rt_mem_base + a[1], wf, a[2] < 18 ? a[2] : 18);
    if (a[3]) put32(a[3], 18);
    return 0;
}

// SetFormat(this, lpwfx) : nouveau format du tampon.
uint32_t sh_dsb_SetFormat(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    ds_read_format(a[1], dsb_plays[a[0]]);
    return 0;
}

// SetFrequency(this, dwFrequency) : 0 = fréquence du format.
uint32_t sh_dsb_SetFrequency(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    dsb_plays[a[0]].freq = (a[1] < 100 || a[1] > 200000) ? 0 : a[1];
    return 0;
}

// SetVolume / SetPan(this, valeur) : centièmes de dB (volume 0 à -10000 ; pan -10000 à 10000).
uint32_t sh_dsb_SetVolume(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    dsb_plays[a[0]].volume = (int32_t)a[1];
    return 0;
}

uint32_t sh_dsb_SetPan(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    dsb_plays[a[0]].pan = (int32_t)a[1];
    return 0;
}

uint32_t sh_dsb_GetVolume(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    if (a[1]) put32(a[1], (uint32_t)dsb_plays[a[0]].volume);
    return 0;
}

uint32_t sh_dsb_GetPan(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    if (a[1]) put32(a[1], (uint32_t)dsb_plays[a[0]].pan);
    return 0;
}

// GetFrequency(this, lpdwFrequency) : fréquence demandée, ou celle du format.
uint32_t sh_dsb_GetFrequency(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    const dsb_play &p = dsb_plays[a[0]];
    if (a[1]) put32(a[1], p.freq ? p.freq : p.rate);
    return 0;
}

// DSBCAPS (20 octets) : taille, drapeaux, taille du tampon.
uint32_t sh_dsb_GetCaps(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) {
        memset(rt_mem_base + a[1], 0, 20);
        put32(a[1], 20);
        put32(a[1] + 8, get32(a[0] + 8));
    }
    return 0;
}

// DSCAPS (96 octets) : capacités matérielles plausibles (32 tampons de mixage, 16 Mio de mémoire).
uint32_t sh_ds_GetCaps(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) {
        memset(rt_mem_base + a[1], 0, 96);
        put32(a[1] + 0, 96);
        put32(a[1] + 1 * 4, 0x00000060);           // DSCAPS_PRIMARY16BIT | DSCAPS_PRIMARYSTEREO
        put32(a[1] + 2 * 4, 100);                  // dwMinSecondarySampleRate
        put32(a[1] + 3 * 4, 96000);                // dwMaxSecondarySampleRate
        put32(a[1] + 4 * 4, 1);                    // dwPrimaryBuffers
        put32(a[1] + 5 * 4, 32);                   // dwMaxHwMixingAllBuffers
        put32(a[1] + 6 * 4, 16);                   // dwMaxHwMixingStaticBuffers
        put32(a[1] + 7 * 4, 16);                   // dwMaxHwMixingStreamingBuffers
        put32(a[1] + 8 * 4, 32);                   // dwFreeHwMixingAllBuffers
        put32(a[1] + 9 * 4, 16);                   // dwFreeHwMixingStaticBuffers
        put32(a[1] + 10 * 4, 16);                  // dwFreeHwMixingStreamingBuffers
        put32(a[1] + 17 * 4, 16u << 20);           // dwTotalHwMemBytes
        put32(a[1] + 18 * 4, 16u << 20);           // dwFreeHwMemBytes
        put32(a[1] + 19 * 4, 16u << 20);           // dwMaxContigFreeHwMemBytes
    }
    return 0;
}

uint32_t sh_ds_ok(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_ds_zero(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_ds_addref(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 2; }
uint32_t sh_ds_release(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
// QueryInterface : le même objet, pour IUnknown, IDirectSoundBuffer, IDirectSoundNotify...
uint32_t sh_ds_qi(cpu_t *c, const uint32_t *a) { (void)c; if (a[2]) put32(a[2], a[0]); return 0; }

// IDirectSoundBuffer::QueryInterface : l'interface 3D est un objet distinct (vtable 3D, arités propres) ;
// les autres IID renvoient le tampon lui-même.
uint32_t sh_dsb_qi(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (!a[1] || !a[2]) return 0;
    if (memcmp(rt_mem_base + a[1], kIidDs3dBuffer, 16) == 0) {
        if (!ds3d_obj) ds3d_obj = new_object(ds3d_vtable, 0);
        put32(a[2], ds3d_obj);
    } else {
        put32(a[2], a[0]);
    }
    return 0;
}

// Méthodes 3D sans effet : les paramètres ne sont pas appliqués au son (il est muet pour l'instant).
// Les méthodes Get écrivent des zéros dans leurs sorties pour que le jeu lise des valeurs définies.
uint32_t sh_ds3d_ok(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_ds3d_get1(cpu_t *c, const uint32_t *a) { (void)c; if (a[1]) put32(a[1], 0); return 0; }
uint32_t sh_ds3d_get_vec(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) { put32(a[1], 0); put32(a[1] + 4, 0); put32(a[1] + 8, 0); }
    return 0;
}
uint32_t sh_ds3d_get_angles(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) put32(a[1], 0);
    if (a[2]) put32(a[2], 0);
    return 0;
}

uint32_t sh_DebugSetMute(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;
}

// DirectInput8 : objet minimal sans périphérique (l'entrée passe par les messages de fenêtre pour
// l'instant). Vtable IDirectInput8A : QueryInterface, AddRef, Release, CreateDevice, EnumDevices,
// GetDeviceStatus, RunControlPanel, Initialize, FindDevice, EnumDevicesBySemantics, ConfigureDevices.
const char *const kDiMethods[] = {"QueryInterface", "AddRef", "Release", "CreateDevice", "EnumDevices",
                                  "GetDeviceStatus", "RunControlPanel", "Initialize", "FindDevice",
                                  "EnumDevicesBySemantics", "ConfigureDevices"};

uint32_t di_vtable;

uint32_t sh_DirectInput8Create(cpu_t *c, const uint32_t *a)
{
    // (hinst, dwVersion, riidltf, ppvOut, punkOuter)
    if (!di_vtable) {
        di_vtable = rt_heap_alloc(4 * 11, 1);
        for (unsigned k = 0; k < 11; k++) {
            std::string full = std::string("dinput8com::IDirectInput8A::") + kDiMethods[k];
            uint32_t stub = rt_dyn_stub_addr(full.c_str());
            memcpy(rt_mem_base + di_vtable + 4 * k, &stub, 4);
        }
    }
    uint32_t obj = rt_heap_alloc(8, 1);
    memcpy(rt_mem_base + obj, &di_vtable, 4);
    if (a[3]) memcpy(rt_mem_base + a[3], &obj, 4);
    (void)c;
    return 0;  // DI_OK
}

uint32_t sh_di_QueryInterface(cpu_t *c, const uint32_t *a) { (void)c; if (a[2]) put32(a[2], 0); return 0x80004002u; }
uint32_t sh_di_AddRef(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 2; }
uint32_t sh_di_Release(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
// Périphérique clavier DirectInput (GUID_SysKeyboard). L'état vient des événements SDL (rt_win).
// {6F1D2B61-D5A0-11CF-BFC7-444553540000}
const uint8_t kGuidSysKeyboard[16] = {0x61, 0x2B, 0x1D, 0x6F, 0xA0, 0xD5, 0xCF, 0x11,
                                      0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
uint32_t di_dev_vtable;
const char *const kDiDevMethods[32] = {
    "QueryInterface", "AddRef", "Release", "GetCapabilities", "EnumObjects", "GetProperty", "SetProperty",
    "Acquire", "Unacquire", "GetDeviceState", "GetDeviceData", "SetDataFormat", "SetEventNotification",
    "SetCooperativeLevel", "GetObjectInfo", "GetDeviceInfo", "RunControlPanel", "Initialize", "CreateEffect",
    "EnumEffects", "GetEffectInfo", "GetForceFeedbackState", "SendForceFeedbackCommand",
    "EnumCreatedEffectObjects", "Escape", "Poll", "SendDeviceData", "EnumEffectsInFile", "WriteEffectToFile",
    "BuildActionMap", "SetActionMap", "GetImageInfo"};

// GetDeviceState(this, cbData, lpvData) : clavier = 256 octets (DIK), sinon rien.
uint32_t sh_dikb_GetDeviceState(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1] == 256 && a[2]) {
        uint8_t keys[256];
        rt_win_key_states(keys);
        memcpy(rt_mem_base + a[2], keys, 256);
    }
    return 0;
}
// GetDeviceData(this, cbObjectData, rgdod, pdwInOut, flags) : pas d'événements tamponnés.
uint32_t sh_dikb_GetDeviceData(cpu_t *c, const uint32_t *a) { (void)c; if (a[3]) put32(a[3], 0); return 0; }
uint32_t sh_dikb_GetCapabilities(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_dikb_zero(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }

// CreateDevice(this, rguid, lplpDevice, pUnkOuter)
uint32_t sh_di_CreateDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (!a[1] || !a[2] || memcmp(rt_mem_base + a[1], kGuidSysKeyboard, 16) != 0) {
        if (a[2]) put32(a[2], 0);
        return 0x80040154u;  // DIERR_DEVICENOTREG
    }
    if (!di_dev_vtable) {
        di_dev_vtable = rt_heap_alloc(4 * 32, 1);
        for (unsigned k = 0; k < 32; k++) {
            std::string full = std::string("dinput8dev::IDirectInputDevice8A::") + kDiDevMethods[k];
            uint32_t stub = rt_dyn_stub_addr(full.c_str());
            memcpy(rt_mem_base + di_dev_vtable + 4 * k, &stub, 4);
        }
    }
    uint32_t dev = rt_heap_alloc(8, 1);
    put32(dev, di_dev_vtable);
    put32(a[2], dev);
    return 0;  // DI_OK
}

// EnumDevices(this, dwDevType, lpCallback, pvRef, dwFlags) : appelle le callback du jeu avec le clavier
// (types 0 = tous, 1 = périphériques, 3 = clavier). Pas de manette.
uint32_t sh_di_EnumDevices(cpu_t *c, const uint32_t *a)
{
    uint32_t type = a[1], cb = a[2], ref = a[3];
    if (!cb || (type != 0 && type != 1 && type != 3)) return 0;
    const uint32_t kSize = 0x244;  // DIDEVICEINSTANCEA (DirectInput 8)
    uint32_t inst = rt_heap_alloc(kSize, 1);
    put32(inst, kSize);
    memcpy(rt_mem_base + inst + 4, kGuidSysKeyboard, 16);   // guidInstance
    memcpy(rt_mem_base + inst + 20, kGuidSysKeyboard, 16); // guidProduct
    put32(inst + 36, 0x13);                                 // dwDevType : DI8DEVTYPE_KEYBOARD
    static const char kName[] = "Clavier";
    memcpy(rt_mem_base + inst + 40, kName, sizeof kName);   // tszInstanceName (ANSI, MAX_PATH)
    memcpy(rt_mem_base + inst + 300, kName, sizeof kName);  // tszProductName
    uint32_t args[2] = {inst, ref};
    rt_call_guest(c, cb, args, 2);
    return 0;
}
uint32_t sh_di_fail(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0x80004005u; }
uint32_t sh_di_ok(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }

}  // namespace

// Table des talons (consultée par rt_shims.c).
// ---- DirectShow : graphe factice pour les vidéos (D-059) ------------------------------------
// Les vidéos du jeu (Media/avi/*.avi) ne sont pas dans cette copie. Le graphe est créé comme le
// fait le jeu (CLSID_FilterGraph, IGraphBuilder, IMediaControl, IMediaEvent, source, broche), mais
// la lecture se termine aussitôt : un seul EC_COMPLETE, posé au premier Run. Le réalisateur attend
// cet événement après un cue d'arrêt vidéo (pause de l'état 3, sous-état 2) : sans lui, la scène SA
// reste figée à la position 181 (voir DECISIONS.md).
const uint8_t kClsidFilterGraph[16] = {0xB3, 0xEB, 0x36, 0xE4, 0x4F, 0x52, 0xCE, 0x11,
                                       0x9F, 0x53, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70};
const uint8_t kIidUnknown[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
// IGraphBuilder, IMediaControl, IMediaPosition, IBasicAudio, IMediaEvent (base 56A868xx-0AD4-11CE-B03A).
const uint8_t kIidGraphBuilder[16] = {0xA9, 0x68, 0xA8, 0x56, 0xD4, 0x0A, 0xCE, 0x11, 0xB0, 0x3A, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70};
const uint8_t kIidMediaControl[16] = {0xB1, 0x68, 0xA8, 0x56, 0xD4, 0x0A, 0xCE, 0x11, 0xB0, 0x3A, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70};
const uint8_t kIidMediaPosition[16] = {0xB2, 0x68, 0xA8, 0x56, 0xD4, 0x0A, 0xCE, 0x11, 0xB0, 0x3A, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70};
const uint8_t kIidBasicAudio[16] = {0xB3, 0x68, 0xA8, 0x56, 0xD4, 0x0A, 0xCE, 0x11, 0xB0, 0x3A, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70};
const uint8_t kIidMediaEvent[16] = {0xB6, 0x68, 0xA8, 0x56, 0xD4, 0x0A, 0xCE, 0x11, 0xB0, 0x3A, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70};

bool guest_guid_is(uint32_t p, const uint8_t g[16])
{
    return p != 0 && p < 0x7F000000u - 16 && memcmp(rt_mem_base + p, g, 16) == 0;
}

// Vtables selon l'ordre de DirectShow (IUnknown 0-2, IDispatch 3-6 pour les interfaces IDispatch).
const char *const kDsgMethods[] = {"QueryInterface", "AddRef", "Release", "AddFilter", "RemoveFilter",
                                   "EnumFilters", "FindFilterByName", "ConnectDirect", "Reconnect",
                                   "Disconnect", "SetDefaultSyncSource", "Connect", "Render",
                                   "RenderFile", "AddSourceFilter", "SetLogFile", "Abort",
                                   "ShouldOperationContinue"};
const char *const kDscMethods[] = {"QueryInterface", "AddRef", "Release", "GetTypeInfoCount", "GetTypeInfo",
                                   "GetIDsOfNames", "Invoke", "Run", "Pause", "Stop"};
const char *const kDseMethods[] = {"QueryInterface", "AddRef", "Release", "GetTypeInfoCount", "GetTypeInfo",
                                   "GetIDsOfNames", "Invoke", "GetEventHandle", "GetEvent",
                                   "WaitForCompletion", "CancelDefaultHandling", "RestoreDefaultHandling",
                                   "FreeEventParams"};
const char *const kDsaMethods[] = {"QueryInterface", "AddRef", "Release", "GetTypeInfoCount", "GetTypeInfo",
                                   "GetIDsOfNames", "Invoke", "put_Volume"};
const char *const kDsSimpleMethods[] = {"QueryInterface", "AddRef", "Release"};
const char *const kDssMethods[] = {"QueryInterface", "AddRef", "Release", "GetClassID", "Stop", "Pause",
                                   "Run", "GetState", "SetSyncSource", "GetSyncSource", "EnumPins", "FindPin"};

uint32_t dsg_vtable, dsc_vtable, dse_vtable, dsa_vtable, dspos_vtable, dssrc_vtable, dspin_vtable;
uint32_t dsc_obj, dse_obj, dsa_obj, dspos_obj, dssrc_obj, dspin_obj;
bool ds_ec_posted, ds_ec_pending;

void ds_graph_init()
{
    if (dsg_vtable) return;
    dsg_vtable = make_vtable("dsg::", kDsgMethods, 18);
    dsc_vtable = make_vtable("dsc::", kDscMethods, 10);
    dse_vtable = make_vtable("dse::", kDseMethods, 13);
    dsa_vtable = make_vtable("dsa::", kDsaMethods, 8);
    dspos_vtable = make_vtable("dspos::", kDsSimpleMethods, 3);
    dssrc_vtable = make_vtable("dssrc::", kDssMethods, 12);
    dspin_vtable = make_vtable("dspin::", kDsSimpleMethods, 3);
    dsc_obj = new_object(dsc_vtable, 0);
    dse_obj = new_object(dse_vtable, 0);
    dsa_obj = new_object(dsa_vtable, 0);
    dspos_obj = new_object(dspos_vtable, 0);
    dssrc_obj = new_object(dssrc_vtable, 0);
    dspin_obj = new_object(dspin_vtable, 0);
}

// CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv)
uint32_t sh_CoCreateInstance(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (!guest_guid_is(a[0], kClsidFilterGraph)) return 0x80040111u;  // CLASS_E_CLASSNOTAVAILABLE
    if (!guest_guid_is(a[3], kIidGraphBuilder) && !guest_guid_is(a[3], kIidUnknown)) return 0x80004002u;
    ds_graph_init();
    ds_ec_posted = false;
    ds_ec_pending = false;
    put32(a[4], new_object(dsg_vtable, 0));
    return 0;
}

// IGraphBuilder::QueryInterface : chaque interface a son propre objet (index de vtable différents).
uint32_t sh_dsg_QueryInterface(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint32_t obj;
    if (guest_guid_is(a[1], kIidMediaControl)) obj = dsc_obj;
    else if (guest_guid_is(a[1], kIidMediaEvent)) obj = dse_obj;
    else if (guest_guid_is(a[1], kIidMediaPosition)) obj = dspos_obj;
    else if (guest_guid_is(a[1], kIidBasicAudio)) obj = dsa_obj;
    else if (guest_guid_is(a[1], kIidGraphBuilder) || guest_guid_is(a[1], kIidUnknown)) obj = a[0];
    else {
        put32(a[2], 0);
        return 0x80004002u;  // E_NOINTERFACE
    }
    put32(a[2], obj);
    return 0;
}

uint32_t sh_dsg_AddSourceFilter(cpu_t *c, const uint32_t *a)
{
    (void)c;
    put32(a[3], dssrc_obj);
    return 0;
}

uint32_t sh_dssrc_FindPin(cpu_t *c, const uint32_t *a)
{
    (void)c;
    put32(a[2], dspin_obj);
    return 0;
}

// IMediaControl::Run : première lecture du graphe, fin immédiate (EC_COMPLETE en file).
uint32_t sh_dsc_Run(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    if (!ds_ec_posted) {
        ds_ec_posted = true;
        ds_ec_pending = true;
    }
    return 0;
}

// IMediaEvent::GetEvent(this, *lEventCode, *lParam1, *lParam2, msTimeout)
uint32_t sh_dse_GetEvent(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (!ds_ec_pending) return 0x80004004u;  // E_ABORT : aucun événement
    ds_ec_pending = false;
    if (a[1]) put32(a[1], 1);  // EC_COMPLETE
    if (a[2]) put32(a[2], 0);
    if (a[3]) put32(a[3], 0);
    return 0;
}

extern "C" const rt_shim_entry rt_extra_table[] = {
    {"GDI32.dll::GetStockObject", 1, sh_GetStockObject},
    {"GDI32.dll::CreateCompatibleDC", 1, sh_CreateCompatibleDC},
    {"GDI32.dll::DeleteDC", 1, sh_DeleteDC},
    {"GDI32.dll::DeleteObject", 1, sh_DeleteObject},
    {"GDI32.dll::SelectObject", 2, sh_SelectObject},
    {"GDI32.dll::SetBkColor", 2, sh_SetBkColor},
    {"GDI32.dll::SetTextColor", 2, sh_SetTextColor},
    {"GDI32.dll::SetTextAlign", 2, sh_SetTextAlign},
    {"GDI32.dll::SetMapMode", 2, sh_SetMapMode},
    {"GDI32.dll::CreateFontA", 14, sh_CreateFontA},
    {"GDI32.dll::GetDeviceCaps", 2, sh_GetDeviceCaps},
    {"GDI32.dll::GetTextExtentPoint32A", 4, sh_GetTextExtentPoint32A},
    {"GDI32.dll::ExtTextOutA", 8, sh_ExtTextOutA},
    {"GDI32.dll::GetObjectA", 3, sh_GetObjectA},
    {"GDI32.dll::GetDIBits", 7, sh_GetDIBits},
    {"GDI32.dll::CreateDIBSection", 6, sh_CreateDIBSection},
    {"ADVAPI32.dll::RegOpenKeyA", 3, sh_RegOpenKeyA},
    {"ADVAPI32.dll::RegQueryValueExA", 6, sh_RegQueryValueExA},
    {"ADVAPI32.dll::RegCloseKey", 1, sh_RegCloseKey},
    {"ole32.dll::CoInitialize", 1, sh_ok0},
    {"ole32.dll::CoUninitialize", 0, sh_zero},
    {"ole32.dll::CoFreeUnusedLibraries", 0, sh_zero},
    {"ole32.dll::CoCreateInstance", 5, sh_CoCreateInstance},
    {"ole32.dll::CoTaskMemAlloc", 1, sh_CoTaskMemAlloc},
    {"ole32.dll::CoTaskMemFree", 1, sh_zero},
    {"KERNEL32.dll::lstrlenA", 1, sh_lstrlenA},
    {"KERNEL32.dll::lstrcpyA", 2, sh_lstrcpyA},
    {"KERNEL32.dll::lstrcpynA", 3, sh_lstrcpynA},
    {"KERNEL32.dll::lstrcatA", 2, sh_lstrcatA},
    {"KERNEL32.dll::lstrcmpA", 2, sh_lstrcmpA},
    {"KERNEL32.dll::lstrcmpiA", 2, sh_lstrcmpiA},
    {"KERNEL32.dll::MulDiv", 3, sh_MulDiv},
    {"KERNEL32.dll::GetCurrentProcess", 0, sh_GetCurrentProcess},
    {"KERNEL32.dll::GetSystemInfo", 1, sh_GetSystemInfo},
    {"KERNEL32.dll::OutputDebugStringA", 1, sh_OutputDebugStringA},
    {"KERNEL32.dll::GetProfileIntA", 3, sh_GetProfileIntA},
    {"KERNEL32.dll::DebugBreak", 0, sh_DebugBreak},
    {"KERNEL32.dll::FatalAppExitA", 2, sh_FatalAppExitA},
    {"KERNEL32.dll::LocalFree", 1, sh_LocalFree},
    {"KERNEL32.dll::FormatMessageA", 7, sh_FormatMessageA},
    {"KERNEL32.dll::GetUserDefaultLCID", 0, sh_GetUserDefaultLCID},
    {"KERNEL32.dll::GetLocaleInfoA", 4, sh_GetLocaleInfoA},
    {"KERNEL32.dll::ExitProcess", 1, sh_ExitProcess},
    {"KERNEL32.dll::TerminateProcess", 2, sh_ExitProcess},
    {"KERNEL32.dll::VirtualQuery", 3, sh_VirtualQuery},
    {"KERNEL32.dll::IsBadReadPtr", 2, sh_IsBadPtr},
    {"KERNEL32.dll::IsBadWritePtr", 2, sh_IsBadPtr},
    {"KERNEL32.dll::IsBadCodePtr", 1, sh_IsBadPtr},
    {"DINPUT8.dll::DirectInput8Create", 5, sh_DirectInput8Create},
    {"dinput8com::IDirectInput8A::QueryInterface", 3, sh_di_QueryInterface},
    {"dinput8com::IDirectInput8A::AddRef", 1, sh_di_AddRef},
    {"dinput8com::IDirectInput8A::Release", 1, sh_di_Release},
    {"dinput8com::IDirectInput8A::CreateDevice", 4, sh_di_CreateDevice},
    {"dinput8dev::IDirectInputDevice8A::QueryInterface", 3, sh_di_QueryInterface},
    {"dinput8dev::IDirectInputDevice8A::AddRef", 1, sh_di_AddRef},
    {"dinput8dev::IDirectInputDevice8A::Release", 1, sh_di_Release},
    {"dinput8dev::IDirectInputDevice8A::GetCapabilities", 2, sh_dikb_GetCapabilities},
    {"dinput8dev::IDirectInputDevice8A::EnumObjects", 4, sh_dikb_zero},
    {"dinput8dev::IDirectInputDevice8A::GetProperty", 3, sh_dikb_zero},
    {"dinput8dev::IDirectInputDevice8A::SetProperty", 3, sh_dikb_zero},
    {"dinput8dev::IDirectInputDevice8A::Acquire", 1, sh_dikb_zero},
    {"dinput8dev::IDirectInputDevice8A::Unacquire", 1, sh_dikb_zero},
    {"dinput8dev::IDirectInputDevice8A::GetDeviceState", 3, sh_dikb_GetDeviceState},
    {"dinput8dev::IDirectInputDevice8A::GetDeviceData", 5, sh_dikb_GetDeviceData},
    {"dinput8dev::IDirectInputDevice8A::SetDataFormat", 2, sh_dikb_zero},
    {"dinput8dev::IDirectInputDevice8A::SetEventNotification", 2, sh_dikb_zero},
    {"dinput8dev::IDirectInputDevice8A::SetCooperativeLevel", 3, sh_dikb_zero},
    {"dinput8dev::IDirectInputDevice8A::GetObjectInfo", 4, sh_dikb_zero},
    {"dinput8dev::IDirectInputDevice8A::GetDeviceInfo", 2, sh_dikb_zero},
    {"dinput8dev::IDirectInputDevice8A::Poll", 1, sh_dikb_zero},
    {"dinput8com::IDirectInput8A::EnumDevices", 5, sh_di_EnumDevices},
    {"dinput8com::IDirectInput8A::GetDeviceStatus", 2, sh_di_fail},
    {"dinput8com::IDirectInput8A::RunControlPanel", 3, sh_di_fail},
    {"dinput8com::IDirectInput8A::Initialize", 3, sh_di_ok},
    {"dinput8com::IDirectInput8A::FindDevice", 4, sh_di_fail},
    {"dinput8com::IDirectInput8A::EnumDevicesBySemantics", 6, sh_di_EnumDevices},
    {"dinput8com::IDirectInput8A::ConfigureDevices", 5, sh_di_fail},
    {"d3d9.dll::DebugSetMute", 0, sh_DebugSetMute},
    {"DSOUND.dll::#11", 3, sh_DirectSoundCreate},
    {"dsg::QueryInterface", 3, sh_dsg_QueryInterface},
    {"dsg::AddRef", 1, sh_ds_addref},
    {"dsg::Release", 1, sh_ds_release},
    {"dsg::AddFilter", 3, sh_ds_ok},
    {"dsg::RemoveFilter", 2, sh_ds_ok},
    {"dsg::EnumFilters", 2, sh_ds_zero},
    {"dsg::FindFilterByName", 3, sh_ds_zero},
    {"dsg::ConnectDirect", 4, sh_ds_ok},
    {"dsg::Reconnect", 2, sh_ds_ok},
    {"dsg::Disconnect", 2, sh_ds_ok},
    {"dsg::SetDefaultSyncSource", 1, sh_ds_ok},
    {"dsg::Connect", 3, sh_ds_ok},
    {"dsg::Render", 2, sh_ds_ok},
    {"dsg::RenderFile", 3, sh_ds_ok},
    {"dsg::AddSourceFilter", 4, sh_dsg_AddSourceFilter},
    {"dsg::SetLogFile", 2, sh_ds_ok},
    {"dsg::Abort", 1, sh_ds_ok},
    {"dsg::ShouldOperationContinue", 1, sh_ds_ok},
    {"dsc::QueryInterface", 3, sh_ds_qi},
    {"dsc::AddRef", 1, sh_ds_addref},
    {"dsc::Release", 1, sh_ds_release},
    {"dsc::GetTypeInfoCount", 2, sh_ds_zero},
    {"dsc::GetTypeInfo", 4, sh_ds_zero},
    {"dsc::GetIDsOfNames", 6, sh_ds_zero},
    {"dsc::Invoke", 8, sh_ds_zero},
    {"dsc::Run", 1, sh_dsc_Run},
    {"dsc::Pause", 1, sh_ds_ok},
    {"dsc::Stop", 1, sh_ds_ok},
    {"dse::QueryInterface", 3, sh_ds_qi},
    {"dse::AddRef", 1, sh_ds_addref},
    {"dse::Release", 1, sh_ds_release},
    {"dse::GetTypeInfoCount", 2, sh_ds_zero},
    {"dse::GetTypeInfo", 4, sh_ds_zero},
    {"dse::GetIDsOfNames", 6, sh_ds_zero},
    {"dse::Invoke", 8, sh_ds_zero},
    {"dse::GetEventHandle", 2, sh_ds_zero},
    {"dse::GetEvent", 5, sh_dse_GetEvent},
    {"dse::WaitForCompletion", 3, sh_ds_zero},
    {"dse::CancelDefaultHandling", 2, sh_ds_ok},
    {"dse::RestoreDefaultHandling", 1, sh_ds_ok},
    {"dse::FreeEventParams", 4, sh_ds_ok},
    {"dsa::QueryInterface", 3, sh_ds_qi},
    {"dsa::AddRef", 1, sh_ds_addref},
    {"dsa::Release", 1, sh_ds_release},
    {"dsa::GetTypeInfoCount", 2, sh_ds_zero},
    {"dsa::GetTypeInfo", 4, sh_ds_zero},
    {"dsa::GetIDsOfNames", 6, sh_ds_zero},
    {"dsa::Invoke", 8, sh_ds_zero},
    {"dsa::put_Volume", 2, sh_ds_ok},
    {"dspos::QueryInterface", 3, sh_ds_qi},
    {"dspos::AddRef", 1, sh_ds_addref},
    {"dspos::Release", 1, sh_ds_release},
    {"dspin::QueryInterface", 3, sh_ds_qi},
    {"dspin::AddRef", 1, sh_ds_addref},
    {"dspin::Release", 1, sh_ds_release},
    {"dssrc::QueryInterface", 3, sh_ds_qi},
    {"dssrc::AddRef", 1, sh_ds_addref},
    {"dssrc::Release", 1, sh_ds_release},
    {"dssrc::GetClassID", 2, sh_ds_ok},
    {"dssrc::Stop", 1, sh_ds_ok},
    {"dssrc::Pause", 1, sh_ds_ok},
    {"dssrc::Run", 2, sh_ds_ok},
    {"dssrc::GetState", 3, sh_ds_ok},
    {"dssrc::SetSyncSource", 2, sh_ds_ok},
    {"dssrc::GetSyncSource", 2, sh_ds_zero},
    {"dssrc::EnumPins", 2, sh_ds_zero},
    {"dssrc::FindPin", 3, sh_dssrc_FindPin},
    {"USER32.dll::wvsprintfA", 3, sh_wvsprintfA},
    {"USER32.dll::wsprintfA", 0, sh_wsprintfA},
    {"dsoundcom::IDirectSound::QueryInterface", 3, sh_ds_qi},
    {"dsoundcom::IDirectSound::AddRef", 1, sh_ds_addref},
    {"dsoundcom::IDirectSound::Release", 1, sh_ds_release},
    {"dsoundcom::IDirectSound::CreateSoundBuffer", 4, sh_ds_CreateSoundBuffer},
    {"dsoundcom::IDirectSound::GetCaps", 2, sh_ds_GetCaps},
    {"dsoundcom::IDirectSound::DuplicateSoundBuffer", 3, sh_ds_zero},
    {"dsoundcom::IDirectSound::SetCooperativeLevel", 3, sh_ds_ok},
    {"dsoundcom::IDirectSound::Compact", 1, sh_ds_ok},
    {"dsoundcom::IDirectSound::GetSpeakerConfig", 2, sh_ds_zero},
    {"dsoundcom::IDirectSound::SetSpeakerConfig", 2, sh_ds_ok},
    {"dsoundcom::IDirectSound::Initialize", 2, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::QueryInterface", 3, sh_dsb_qi},
    {"dsoundcom3d::IDirectSound3DBuffer::QueryInterface", 3, sh_ds_qi},
    {"dsoundcom3d::IDirectSound3DBuffer::AddRef", 1, sh_ds_addref},
    {"dsoundcom3d::IDirectSound3DBuffer::Release", 1, sh_ds_release},
    {"dsoundcom3d::IDirectSound3DBuffer::GetAllParameters", 2, sh_ds3d_ok},
    {"dsoundcom3d::IDirectSound3DBuffer::GetConeAngles", 3, sh_ds3d_get_angles},
    {"dsoundcom3d::IDirectSound3DBuffer::GetConeOrientation", 2, sh_ds3d_get_vec},
    {"dsoundcom3d::IDirectSound3DBuffer::GetConeOutsideVolume", 2, sh_ds3d_get1},
    {"dsoundcom3d::IDirectSound3DBuffer::GetMaxDistance", 2, sh_ds3d_get1},
    {"dsoundcom3d::IDirectSound3DBuffer::GetMinDistance", 2, sh_ds3d_get1},
    {"dsoundcom3d::IDirectSound3DBuffer::GetMode", 2, sh_ds3d_get1},
    {"dsoundcom3d::IDirectSound3DBuffer::GetPosition", 2, sh_ds3d_get_vec},
    {"dsoundcom3d::IDirectSound3DBuffer::GetVelocity", 2, sh_ds3d_get_vec},
    {"dsoundcom3d::IDirectSound3DBuffer::SetAllParameters", 3, sh_ds3d_ok},
    {"dsoundcom3d::IDirectSound3DBuffer::SetConeAngles", 4, sh_ds3d_ok},
    {"dsoundcom3d::IDirectSound3DBuffer::SetConeOrientation", 5, sh_ds3d_ok},
    {"dsoundcom3d::IDirectSound3DBuffer::SetConeOutsideVolume", 3, sh_ds3d_ok},
    {"dsoundcom3d::IDirectSound3DBuffer::SetMaxDistance", 3, sh_ds3d_ok},
    {"dsoundcom3d::IDirectSound3DBuffer::SetMinDistance", 3, sh_ds3d_ok},
    {"dsoundcom3d::IDirectSound3DBuffer::SetMode", 3, sh_ds3d_ok},
    {"dsoundcom3d::IDirectSound3DBuffer::SetPosition", 5, sh_ds3d_ok},
    {"dsoundcom3d::IDirectSound3DBuffer::SetVelocity", 5, sh_ds3d_ok},
    {"dsoundcom::IDirectSoundBuffer::AddRef", 1, sh_ds_addref},
    {"dsoundcom::IDirectSoundBuffer::Release", 1, sh_ds_release},
    {"dsoundcom::IDirectSoundBuffer::GetCaps", 2, sh_dsb_GetCaps},
    {"dsoundcom::IDirectSoundBuffer::GetCurrentPosition", 3, sh_dsb_GetCurrentPosition},
    {"dsoundcom::IDirectSoundBuffer::GetFormat", 4, sh_dsb_GetFormat},
    {"dsoundcom::IDirectSoundBuffer::GetVolume", 2, sh_dsb_GetVolume},
    {"dsoundcom::IDirectSoundBuffer::GetPan", 2, sh_dsb_GetPan},
    {"dsoundcom::IDirectSoundBuffer::GetFrequency", 2, sh_dsb_GetFrequency},
    {"dsoundcom::IDirectSoundBuffer::GetStatus", 2, sh_dsb_GetStatus},
    {"dsoundcom::IDirectSoundBuffer::Initialize", 3, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::Lock", 8, sh_dsb_Lock},
    {"dsoundcom::IDirectSoundBuffer::Play", 4, sh_dsb_Play},
    {"dsoundcom::IDirectSoundBuffer::SetCurrentPosition", 2, sh_dsb_SetCurrentPosition},
    {"dsoundcom::IDirectSoundBuffer::SetFormat", 2, sh_dsb_SetFormat},
    {"dsoundcom::IDirectSoundBuffer::SetVolume", 2, sh_dsb_SetVolume},
    {"dsoundcom::IDirectSoundBuffer::SetPan", 2, sh_dsb_SetPan},
    {"dsoundcom::IDirectSoundBuffer::SetFrequency", 2, sh_dsb_SetFrequency},
    {"dsoundcom::IDirectSoundBuffer::Stop", 1, sh_dsb_Stop},
    {"dsoundcom::IDirectSoundBuffer::Unlock", 5, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::Restore", 1, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::SetFX", 4, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::AcquireResources", 4, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::GetObjectInPath", 5, sh_ds_zero},
    {"KERNEL32.dll::FlushFileBuffers", 1, sh_one},
    {"KERNEL32.dll::SetStdHandle", 2, sh_one},
    {"KERNEL32.dll::SetEndOfFile", 1, sh_one},
    {"KERNEL32.dll::VirtualProtect", 4, sh_one},
    {"KERNEL32.dll::HeapDestroy", 1, sh_one},
    {"KERNEL32.dll::IsValidLocale", 2, sh_one},
    {"KERNEL32.dll::FreeEnvironmentStringsA", 1, sh_one},
    {"KERNEL32.dll::FreeLibrary", 1, sh_one},
    {"KERNEL32.dll::UnhandledExceptionFilter", 1, sh_zero},
};

extern "C" const unsigned rt_extra_count = sizeof rt_extra_table / sizeof rt_extra_table[0];
