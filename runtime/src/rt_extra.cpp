// Talons GDI32, ADVAPI32, ole32 et petites fonctions KERNEL32 (voir rt_shims.c pour les autres).
//
// GDI : les objets (pinceaux, polices, DC, DIB) sont des handles fictifs. Le texte n'est pas encore
// dessiné : ExtTextOutA ne fait rien (signalé une fois). Les DIB sections sont de vraies zones de
// mémoire invitée, pour un rendu futur.

#include "rt_proc.h"
#include "rt_shim.h"
#include "rt_font.h"
#include "rt_win.h"
#include "rt_d3d9.h"
#include "rt_file.h"

#include <SDL.h>

#if CT3_HAVE_FFMPEG
/* Les en-têtes du préfixe de dépendances n'ont pas de garde extern "C" : sans elle, les symboles
 * FFmpeg seraient décorés en C++ et l'édition de liens échouerait. */
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <cctype>
#include <cstdarg>
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
    if (getenv("CT3_PAD_LOG")) fprintf(stderr, "[reg] RegOpenKeyA %s\n", (const char *)(rt_mem_base + a[1]));
    return 2;  // ERROR_FILE_NOT_FOUND : clé absente
}

uint32_t sh_RegQueryValueExA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (getenv("CT3_PAD_LOG")) fprintf(stderr, "[reg] RegQueryValueExA %s\n", (const char *)(rt_mem_base + a[1]));
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
    /* 3D */
    bool is3d = false;
    float min_dist = 1.0f;
    float max_dist = 1000000000.0f;
    float pos_x = 0, pos_y = 0, pos_z = 0;
};
std::map<uint32_t, dsb_play> dsb_plays;
std::mutex ds_mutex;
SDL_AudioDeviceID ds_dev = 0;
bool ds_audio_tried = false;
/* Écouteur 3D (valeurs par défaut) */
float ds_listener_x = 0, ds_listener_y = 0, ds_listener_z = 0;

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
            /* Atténuation 3D */
            if (p.is3d) {
                float dx = p.pos_x - ds_listener_x;
                float dy = p.pos_y - ds_listener_y;
                float dz = p.pos_z - ds_listener_z;
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                float d = dist;
                if (d < p.min_dist) d = p.min_dist;
                if (p.max_dist > 0 && d > p.max_dist) d = p.max_dist;
                /* Modèle inverse distance avec facteur de chute 1.0 */
                float atten = p.min_dist / (d <= 0 ? p.min_dist : d);
                if (atten < 0) atten = 0;
                if (atten > 1.0f) atten = 1.0f;
                gain *= atten;
                /* Panoramique simple basé sur la position X */
                float pan = dx / (d > 0.0001f ? d : 1.0f); /* -1 à 1 */
                p.pan = (int32_t)(pan * 10000.0f);
            }
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
static uint32_t sh_ds_objinpath(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[4]) put32(a[4], 0);
    return 0x80004002u; /* E_NOINTERFACE */
}
// QueryInterface : le même objet, pour IUnknown, IDirectSoundBuffer, IDirectSoundNotify...
uint32_t sh_ds_qi(cpu_t *c, const uint32_t *a) { (void)c; if (a[2]) put32(a[2], a[0]); return 0; }

// IDirectSoundBuffer::QueryInterface : l'interface 3D est un objet distinct (vtable 3D, arités propres) ;
// les autres IID renvoient le tampon lui-même.
uint32_t sh_dsb_qi(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (!a[1] || !a[2]) return 0;
    if (memcmp(rt_mem_base + a[1], kIidDs3dBuffer, 16) == 0) {
        if (!ds3d_obj) ds3d_obj = new_object(ds3d_vtable, 4);
        /* stocker le tampon associé dans le stockage de l'objet 3D */
        put32(get32(ds3d_obj + 4), a[0]);
        put32(a[2], ds3d_obj);
    } else {
        put32(a[2], a[0]);
    }
    return 0;
}

// Méthodes 3D
static uint32_t ds3d_get_buf(uint32_t ds3d_this)
{
    if (!ds3d_this) return 0;
    /* stockage dans l'objet 3D */
    uint32_t store = get32(ds3d_this + 4);
    if (!store) return 0;
    return get32(store);
}
static float f32_from_u32(uint32_t v) { float f; memcpy(&f, &v, 4); return f; }
static uint32_t u32_from_f32(float f) { uint32_t v; memcpy(&v, &f, 4); return v; }

uint32_t sh_ds3d_ok(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_ds3d_get1(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    uint32_t buf = ds3d_get_buf(a[0]);
    if (buf && dsb_plays.count(buf)) {
        if (a[1]) put32(a[1], u32_from_f32(dsb_plays[buf].min_dist)); /* par défaut min_dist pour GetMinDistance */
        return 0;
    }
    if (a[1]) put32(a[1], u32_from_f32(1.0f));
    return 0;
}
uint32_t sh_ds3d_get_vec(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    uint32_t buf = ds3d_get_buf(a[0]);
    float x=0,y=0,z=0;
    if (buf && dsb_plays.count(buf)) { x = dsb_plays[buf].pos_x; y = dsb_plays[buf].pos_y; z = dsb_plays[buf].pos_z; }
    if (a[1]) { put32(a[1], u32_from_f32(x)); put32(a[1]+4, u32_from_f32(y)); put32(a[1]+8, u32_from_f32(z)); }
    return 0;
}
uint32_t sh_ds3d_get_angles(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) put32(a[1], u32_from_f32(0));
    if (a[2]) put32(a[2], u32_from_f32(0));
    return 0;
}
static uint32_t sh_ds3d_SetMinDistance(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    uint32_t buf = ds3d_get_buf(a[0]);
    if (buf && dsb_plays.count(buf)) { dsb_plays[buf].min_dist = f32_from_u32(a[1]); dsb_plays[buf].is3d = true; }
    return 0;
}
static uint32_t sh_ds3d_SetMaxDistance(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    uint32_t buf = ds3d_get_buf(a[0]);
    if (buf && dsb_plays.count(buf)) { dsb_plays[buf].max_dist = f32_from_u32(a[1]); dsb_plays[buf].is3d = true; }
    return 0;
}
static uint32_t sh_ds3d_SetPosition(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::lock_guard<std::mutex> lk(ds_mutex);
    uint32_t buf = ds3d_get_buf(a[0]);
    if (buf && dsb_plays.count(buf)) {
        dsb_plays[buf].pos_x = f32_from_u32(a[1]);
        dsb_plays[buf].pos_y = f32_from_u32(a[2]);
        dsb_plays[buf].pos_z = f32_from_u32(a[3]);
        dsb_plays[buf].is3d = true;
    }
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

// Manette DirectInput : constantes et vtable (voir plus bas).
// {6F1D2B70-D5A0-11CF-BFC7-444553540000}
const uint8_t kGuidJoystick[16] = {0x70, 0x2B, 0x1D, 0x6F, 0xA0, 0xD5, 0xCF, 0x11,
                                   0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
uint32_t di_joy_vtable;
/* Plage signée, comme Xidi (kAnalogValueMin/Max) : au repos, les axes valent 0. Une plage 0..65535 faisait
 * lire le centre (32767) comme une poussée maximale vers le bas (menus bloqués sur « Bas »). */
int32_t joy_range_min = -32767, joy_range_max = 32767;
const uint32_t kDi8DevTypeGamepad = 0x15;
const uint32_t kDiPropRange = 4;   // DIPROP_RANGE = MAKEDIPROP(4)
const uint32_t kDiErrNotImpl = 0x80004001u;

// Index DirectInput des boutons pour chaque bouton SDL (A, B, X, Y, Back, Guide, Start, LS, RS, LB, RB, croix).
const uint8_t kJoyDiFromSdl[15] = {0, 1, 2, 3, 6, 10, 7, 8, 9, 4, 5, 11, 12, 13, 14};
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
    if (a[1] && a[2] && memcmp(rt_mem_base + a[1], rt_joy_config.guid, 16) == 0 && rt_joy_config.enabled) {
        if (!di_joy_vtable) {
            di_joy_vtable = rt_heap_alloc(4 * 32, 1);
            for (unsigned k = 0; k < 32; k++) {
                std::string full = std::string("dinput8joy::IDirectInputDevice8A::") + kDiDevMethods[k];
                uint32_t stub = rt_dyn_stub_addr(full.c_str());
                memcpy(rt_mem_base + di_joy_vtable + 4 * k, &stub, 4);
            }
        }
        uint32_t dev = rt_heap_alloc(8, 1);
        put32(dev, di_joy_vtable);
        put32(a[2], dev);
        if (getenv("CT3_PAD_LOG")) fprintf(stderr, "[dinput] périphérique manette créé\n");
        return 0;
    }
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
    if (getenv("CT3_PAD_LOG")) fprintf(stderr, "[dinput] EnumDevices classe=%u manette=%d\n", type, rt_win_joy_connected());
    if (!cb) return 0;
    if (rt_joy_config.enabled && (type == 0 || type == 1 || type == 4)) {
        const uint32_t kSizeJ = 0x244;
        uint32_t jinst = rt_heap_alloc(kSizeJ, 1);
        put32(jinst, kSizeJ);
        memcpy(rt_mem_base + jinst + 4, rt_joy_config.guid, 16);   /* guidInstance */
        memcpy(rt_mem_base + jinst + 20, kGuidJoystick, 16);  /* guidProduct */
        put32(jinst + 36, kDi8DevTypeGamepad);
        static const char kJoyName[] = "Manette";
        memcpy(rt_mem_base + jinst + 40, kJoyName, sizeof kJoyName);
        memcpy(rt_mem_base + jinst + 300, kJoyName, sizeof kJoyName);
        if (getenv("CT3_PAD_LOG")) fprintf(stderr, "[dinput] manette énumérée (classe %u)\n", type);
        uint32_t jargs[2] = {jinst, ref};
        rt_call_guest(c, cb, jargs, 2);
    }
    if (type != 0 && type != 1 && type != 3) return 0;
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

// Manette DirectInput. État DIJOYSTATE (80 octets) rempli depuis SDL (rt_win) ; réglages [Joystick] de Config.ini.
// Plage des axes : -32767 à 32767 par défaut (centre 0), modifiable par SetProperty(DIPROP_RANGE).

/* Écrit l'état de la manette à l'adresse invitée dst (cb octets, au plus 272 : DIJOYSTATE puis zéros). */
static void joy_build_state(uint32_t dst, uint32_t cb)
{
    int16_t ax[6];
    uint8_t bt[15];
    uint8_t buf[272];
    rt_win_joy_state(ax, bt);
    const float dzf = rt_joy_config.deadzone / 32767.0f;
    auto norm = [](int16_t v) { return v / 32767.0f; };
    auto dz = [dzf](float v) { return (v > -dzf && v < dzf) ? 0.0f : v; };
    auto clamp1 = [](float v) { return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v); };
    const int32_t mn = joy_range_min, mx = joy_range_max;
    const int32_t center = (mn + mx) / 2;
    const float half = (float)(mx - mn) / 2.0f;
    auto to_axis = [&](float v) { return (int32_t)(center + clamp1(v) * half); };

    memset(buf, 0, sizeof buf);
    float sx = dz(norm(ax[0])), sy = dz(norm(ax[1]));
    /* Les gâchettes arrivent déjà calibrées et normalisées par rt_win (0 = repos, 32767 = course
     * complète), même si la manette annonce un repos décalé. La zone morte s'applique aussi à la
     * différence LT-RT : sans elle, un écart de repos de quelques centaines d'unités suffisait à
     * maintenir une direction dans les menus. */
    float lt = norm(ax[4]), rt = norm(ax[5]);
    lt = lt < dzf ? 0.0f : lt;
    rt = rt < dzf ? 0.0f : rt;
    float trig = lt - rt;                       /* LT vers le haut de l'axe, RT vers le bas (comme Xidi) */
    if (trig > -dzf && trig < dzf) trig = 0.0f;
    int dpx = (int)bt[14] - (int)bt[13];        /* droite - gauche */
    int dpy = (int)bt[12] - (int)bt[11];        /* bas - haut */
    const int pov = rt_joy_config.dpad_pov;
    float vx = (rt_joy_config.stick_x ? sx : 0.0f) + (pov ? 0.0f : (float)dpx);
    float vy = (rt_joy_config.triggers_z ? 0.0f : trig) + (pov ? 0.0f : (float)dpy) + (rt_joy_config.stick_y ? sy : 0.0f);
    float vz = rt_joy_config.triggers_z ? trig : 0.0f;
    int32_t axes[8] = { to_axis(vx), to_axis(vy), to_axis(vz), center, center, center, center, center };
    memcpy(buf, axes, 32);
    /* Croix en POV (centièmes de degré, 0 = haut, sens horaire), neutre = 0xFFFFFFFF. */
    uint32_t pov0 = 0xFFFFFFFFu;
    if (pov && (dpx != 0 || dpy != 0)) {
        int ix = dpx + 1, iy = dpy + 1;   /* 0 à 2 */
        static const int32_t kAngle[3][3] = {
            { 31500, 0, 4500 },    /* haut : gauche-haut, haut, droite-haut */
            { 27000, -1, 9000 },   /* milieu : gauche, neutre, droite */
            { 22500, 18000, 13500 } /* bas : gauche-bas, bas, droite-bas */
        };
        if (kAngle[iy][ix] >= 0) pov0 = (uint32_t)kAngle[iy][ix];
    }
    for (int k = 0; k < 4; k++) {
        uint32_t v = (k == 0) ? pov0 : 0xFFFFFFFFu;
        memcpy(buf + 32 + 4 * k, &v, 4);
    }
    for (int i = 0; i < 15; i++) {
        buf[48 + kJoyDiFromSdl[i]] = bt[i] ? 0x80 : 0;
    }
    if (cb > sizeof buf) cb = sizeof buf;
    memcpy(rt_mem_base + dst, buf, cb);
    if (getenv("CT3_PAD_LOG")) {
        static uint8_t last[80];
        static int first = 1;
        if (first || memcmp(last, buf, 80) != 0) {
            int32_t lx, ly, lz;
            uint32_t p0;
            memcpy(&lx, buf, 4);
            memcpy(&ly, buf + 4, 4);
            memcpy(&lz, buf + 8, 4);
            memcpy(&p0, buf + 32, 4);
            int bmask = 0;
            for (int i = 0; i < 15; i++) if (buf[48 + i]) bmask |= 1 << i;
            fprintf(stderr, "[dinput] état : X=%d Y=%d Z=%d POV=%08x boutons=%04x (axes SDL LX=%d LY=%d LT=%d RT=%d)\n",
                    lx, ly, lz, p0, bmask, ax[0], ax[1], ax[4], ax[5]);
            memcpy(last, buf, 80);
            first = 0;
        }
    }
}

// GetDeviceState(this, cbData, lpvData) de la manette.
uint32_t sh_joy_GetDeviceState(cpu_t *c, const uint32_t *a)
{
    static int logged;
    (void)c;
    if (a[2]) joy_build_state(a[2], a[1]);
    if (!logged && getenv("CT3_PAD_LOG") && a[2]) {
        logged = 1;
        int32_t v[3];
        memcpy(v, rt_mem_base + a[2], 12);
        fprintf(stderr, "[dinput] GetDeviceState manette : %u octets, branchée=%d, X=%d Y=%d Z=%d\n", a[1],
                rt_win_joy_connected(), v[0], v[1], v[2]);
    }
    return 0;
}
// GetCapabilities(this, lpDIDevCaps) : DIDEVCAPS (dwSize, dwFlags, dwDevType, dwAxes, dwButtons, dwPOVs).
uint32_t sh_joy_GetCapabilities(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) {
        put32(a[1] + 8, kDi8DevTypeGamepad);
        put32(a[1] + 12, 6);
        put32(a[1] + 16, 32);
        put32(a[1] + 20, 1);
    }
    return 0;
}
// SetProperty / GetProperty(this, rguid, pdiph) : seule la plage (DIPROP_RANGE) est gérée.
static uint32_t guest_u32(uint32_t addr)
{
    uint32_t v;
    memcpy(&v, rt_mem_base + addr, 4);
    return v;
}
uint32_t sh_joy_SetProperty(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1] == kDiPropRange && a[2]) {
        joy_range_min = (int32_t)guest_u32(a[2] + 16);
        joy_range_max = (int32_t)guest_u32(a[2] + 20);
        if (getenv("CT3_PAD_LOG")) fprintf(stderr, "[dinput] plage des axes : %d à %d\n", joy_range_min, joy_range_max);
    } else if (getenv("CT3_PAD_LOG")) {
        fprintf(stderr, "[dinput] SetProperty manette (propriété %08x)\n", a[1]);
    }
    return 0;
}
uint32_t sh_joy_GetProperty(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1] == kDiPropRange && a[2]) {
        put32(a[2] + 16, (uint32_t)joy_range_min);
        put32(a[2] + 20, (uint32_t)joy_range_max);
    }
    return 0;
}
// EnumObjects : pas encore d'objets (axes, boutons) énumérés.
uint32_t sh_joy_EnumObjects(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (getenv("CT3_PAD_LOG")) fprintf(stderr, "[dinput] EnumObjects manette (indicateurs %08x)\n", a[2]);
    return 0;
}
// GetDeviceData : pas d'événements tamponnés.
uint32_t sh_joy_GetDeviceData(cpu_t *c, const uint32_t *a) { (void)c; if (a[3]) put32(a[3], 0); return 0; }
// Effets de force : non gérés pour l'instant.
uint32_t sh_joy_notimpl(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return kDiErrNotImpl; }

}  // namespace

// Table des talons (consultée par rt_shims.c).
// ---- DirectShow : graphe factice et cinématiques (D-059) --------------------------------------
// Le graphe est créé comme le fait le jeu (CLSID_FilterGraph, IGraphBuilder, IMediaControl,
// IMediaEvent, source, broche), mais il n'y a ni source ni filtre réels : les vidéos sont décodées
// par FFmpeg et copiées dans la texture du jeu par son propre filtre « INTRO TEXTURE RENDERER »
// (0x40CE00), appelé à chaque présentation. EC_COMPLETE n'est posté qu'à la fin réelle de la vidéo
// (ou immédiatement si le fichier est absent, pour ne pas figer la scène SA à la position 181).
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
const char *const kDsposMethods[] = {"QueryInterface", "AddRef", "Release", "GetTypeInfoCount", "GetTypeInfo",
                                     "GetIDsOfNames", "Invoke", "get_Duration", "put_CurrentPosition",
                                     "get_CurrentPosition", "get_StopTime", "put_StopTime",
                                     "get_PrerollTime", "put_PrerollTime", "put_Rate", "get_Rate"};
// IMediaSample factice : seule GetPointer est utilisée par le filtre du jeu (vtable +0xC).
const char *const kMsampleMethods[] = {"QueryInterface", "AddRef", "Release", "GetPointer"};
const char *const kDssMethods[] = {"QueryInterface", "AddRef", "Release", "GetClassID", "Stop", "Pause",
                                   "Run", "GetState", "SetSyncSource", "GetSyncSource", "EnumPins", "FindPin"};

uint32_t dsg_vtable, dsc_vtable, dse_vtable, dsa_vtable, dspos_vtable, dssrc_vtable, dspin_vtable;
uint32_t dsc_obj, dse_obj, dsa_obj, dspos_obj, dssrc_obj, dspin_obj;
uint32_t msample_vtable, msample_obj;
bool ds_ec_posted, ds_ec_pending;

namespace {

// Une seule lecture à la fois (les objets DirectShow du runtime sont uniques). Le décodeur est
// ouvert au Run et alimenté depuis Present, une fois que le jeu a fourni la texture au filtre.
struct movie_state {
    bool running;
    bool paused;
    bool started;
    bool completed;
    bool nofile;
    uint32_t renderer;
    uint32_t texture;
    uint32_t buf;
    uint32_t buf_cap;
    std::vector<uint8_t> flip_row;
    int out_w, out_h, out_stride;
    long frame_count;
    double clock_start;
    double paused_at;
    double paused_total;
    char path[1024];
    char open_path[1024];
#if CT3_HAVE_FFMPEG
    AVFormatContext *fmt;
    AVCodecContext *dec;
    SwsContext *sws;
    AVFrame *frame;
    AVPacket *pkt;
    AVStream *stream;
    AVRational tb;
    int stream_index;
    int video_w, video_h, pix_fmt;
    double fps;
    double duration;
    double first_pts, next_pts, last_pts;
    bool have_first_pts;
    bool have_frame;
    bool eof;
    bool draining;
    bool opened;
#endif
};

movie_state g_movie;

double movie_now_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

void movie_logf(const char *fmt, ...)
{
    static int on = -1;
    if (on < 0) on = getenv("CT3_MOVIE_LOG") != nullptr;
    if (!on) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void movie_free_ffmpeg()
{
#if CT3_HAVE_FFMPEG
    movie_state &m = g_movie;
    if (m.sws) {
        sws_freeContext(m.sws);
        m.sws = nullptr;
    }
    if (m.frame) av_frame_free(&m.frame);
    if (m.pkt) av_packet_free(&m.pkt);
    if (m.dec) avcodec_free_context(&m.dec);
    if (m.fmt) avformat_close_input(&m.fmt);
    m.stream = nullptr;
    m.opened = false;
    m.have_frame = m.eof = m.draining = m.have_first_pts = false;
#endif
}

void movie_close_decoder()
{
    movie_free_ffmpeg();
    g_movie.open_path[0] = 0;
}

void movie_reset_clock()
{
    movie_state &m = g_movie;
    m.started = false;
    m.clock_start = movie_now_ms();
    m.paused_at = 0.0;
    m.paused_total = 0.0;
}

#if CT3_HAVE_FFMPEG
bool movie_open_decoder(const char *path)
{
    movie_state &m = g_movie;
    movie_close_decoder();
    if (!path || !*path) return false;
    char host[1200];
    rt_file_resolve(path, host, sizeof host);

    if (avformat_open_input(&m.fmt, host, nullptr, nullptr) < 0) {
        m.fmt = nullptr;
        movie_logf("[movie] ouverture impossible : %s (%s)\n", path, host);
        return false;
    }
    if (avformat_find_stream_info(m.fmt, nullptr) < 0) {
        movie_logf("[movie] flux illisible : %s\n", path);
        movie_close_decoder();
        return false;
    }
    const AVCodec *codec = nullptr;
    int si = av_find_best_stream(m.fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (si < 0 || !codec) {
        movie_logf("[movie] pas de flux vidéo : %s\n", path);
        movie_close_decoder();
        return false;
    }
    m.stream = m.fmt->streams[si];
    m.stream_index = si;
    m.dec = avcodec_alloc_context3(codec);
    if (!m.dec || avcodec_parameters_to_context(m.dec, m.stream->codecpar) < 0 ||
        avcodec_open2(m.dec, codec, nullptr) < 0) {
        movie_logf("[movie] décodeur indisponible : %s\n", path);
        movie_close_decoder();
        return false;
    }
    m.frame = av_frame_alloc();
    m.pkt = av_packet_alloc();
    if (!m.frame || !m.pkt) {
        movie_close_decoder();
        return false;
    }
    m.tb = m.stream->time_base;
    m.video_w = m.dec->width;
    m.video_h = m.dec->height;
    m.pix_fmt = m.dec->pix_fmt;
    AVRational fr = av_guess_frame_rate(m.fmt, m.stream, nullptr);
    m.fps = (fr.num > 0 && fr.den > 0) ? av_q2d(fr) : 30.0;
    m.duration = 0.0;
    if (m.stream->duration != AV_NOPTS_VALUE)
        m.duration = m.stream->duration * av_q2d(m.tb);
    else if (m.fmt->duration != AV_NOPTS_VALUE)
        m.duration = m.fmt->duration / (double)AV_TIME_BASE;
    m.first_pts = m.next_pts = m.last_pts = 0.0;
    m.have_first_pts = m.have_frame = m.eof = m.draining = false;
    m.opened = true;
    snprintf(m.open_path, sizeof m.open_path, "%s", path);
    movie_logf("[movie] %s : %dx%d, %.2f s, %.2f i/s (%s)\n", path, m.video_w, m.video_h, m.duration, m.fps,
               m.dec->codec->name);
    return true;
}

// 1 = image décodée, 0 = fin, -1 = erreur.
int movie_next_frame()
{
    movie_state &m = g_movie;
    for (;;) {
        int ret = avcodec_receive_frame(m.dec, m.frame);
        if (ret == 0) {
            double pts = (m.frame->best_effort_timestamp != AV_NOPTS_VALUE)
                             ? m.frame->best_effort_timestamp * av_q2d(m.tb)
                             : m.next_pts;
            if (!m.have_first_pts) {
                m.first_pts = pts;
                m.have_first_pts = true;
            }
            pts -= m.first_pts;
            if (pts < 0.0) pts = 0.0;
            m.last_pts = pts;
            m.next_pts = pts + (m.fps > 0.0 ? 1.0 / m.fps : 0.033);
            return 1;
        }
        if (ret == AVERROR_EOF) return 0;
        if (ret != AVERROR(EAGAIN)) return -1;
        int r = av_read_frame(m.fmt, m.pkt);
        if (r < 0) {
            if (m.draining) return 0;
            m.draining = true;
            avcodec_send_packet(m.dec, nullptr);
            continue;
        }
        if (m.pkt->stream_index == m.stream_index) avcodec_send_packet(m.dec, m.pkt);
        av_packet_unref(m.pkt);
    }
}

bool movie_configure(int ow, int oh)
{
    movie_state &m = g_movie;
    if (ow <= 0 || oh <= 0) {
        ow = m.video_w;
        oh = m.video_h;
    }
    if (ow <= 0 || oh <= 0) return false;
    if (ow > 4096) ow = 4096;
    if (oh > 4096) oh = 4096;
    m.out_w = ow;
    m.out_h = oh;
    m.out_stride = (ow * 3 + 3) & ~3;
    m.flip_row.resize((size_t)m.out_stride);
    uint32_t need = (uint32_t)m.out_stride * (uint32_t)oh;
    if (need > m.buf_cap) {
        m.buf = rt_heap_alloc(need ? need : 1, 0);
        m.buf_cap = need;
    }
    if (m.sws) {
        sws_freeContext(m.sws);
        m.sws = nullptr;
    }
    return true;
}

bool movie_scale_frame()
{
    movie_state &m = g_movie;
    if (!m.sws) {
        int src_fmt = m.pix_fmt;
        if (src_fmt <= AV_PIX_FMT_NONE) src_fmt = m.frame->format;
        if (src_fmt <= AV_PIX_FMT_NONE) return false;
        int flags = (m.out_w == m.video_w && m.out_h == m.video_h) ? SWS_POINT : SWS_BILINEAR;
        m.sws = sws_getContext(m.video_w, m.video_h, (AVPixelFormat)src_fmt, m.out_w, m.out_h,
                               AV_PIX_FMT_BGR24, flags, nullptr, nullptr, nullptr);
        if (!m.sws) return false;
    }
    uint8_t *dst[4] = {rt_mem_base + m.buf, nullptr, nullptr, nullptr};
    int dst_stride[4] = {m.out_stride, 0, 0, 0};
    sws_scale(m.sws, m.frame->data, m.frame->linesize, 0, m.video_h, dst, dst_stride);
    /* Le renderer du jeu attend une image DIB bas-en-haut (biHeight positif) : la texture est
     * échantillonnée à l'envers. FFmpeg fournit des lignes de haut en bas, on inverse ici. */
    uint8_t *base = rt_mem_base + m.buf;
    for (int y = 0; y < m.out_h / 2; y++) {
        uint8_t *a = base + (size_t)y * m.out_stride;
        uint8_t *b = base + (size_t)(m.out_h - 1 - y) * m.out_stride;
        memcpy(m.flip_row.data(), a, (size_t)m.out_stride);
        memcpy(a, b, (size_t)m.out_stride);
        memcpy(b, m.flip_row.data(), (size_t)m.out_stride);
    }
    return true;
}

void movie_seek_seconds(double pos)
{
    movie_state &m = g_movie;
    if (!m.fmt || !m.dec) return;
    if (pos < 0.0) pos = 0.0;
    int64_t ts = (int64_t)(pos / av_q2d(m.tb));
    if (av_seek_frame(m.fmt, m.stream_index, ts, AVSEEK_FLAG_BACKWARD) < 0)
        av_seek_frame(m.fmt, m.stream_index, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(m.dec);
    m.have_first_pts = m.have_frame = m.eof = m.draining = false;
    m.first_pts = m.next_pts = m.last_pts = 0.0;
    m.started = false;
    m.clock_start = movie_now_ms();
    m.paused_total = 0.0;
    m.paused_at = 0.0;
}
#else
bool movie_open_decoder(const char *) { return false; }
int movie_next_frame() { return 0; }
bool movie_configure(int, int) { return false; }
bool movie_scale_frame() { return false; }
void movie_seek_seconds(double) {}
#endif

// Démarre une nouvelle lecture. Si le fichier est absent, EC_COMPLETE est posté immédiatement,
// comme avant l'arrivée des vidéos, pour que le réalisateur ne reste pas bloqué.
void movie_begin()
{
    movie_state &m = g_movie;
    m.running = true;
    m.paused = false;
    m.completed = false;
    m.nofile = false;
    m.texture = 0;
    ds_ec_posted = false;
    ds_ec_pending = false;
    movie_close_decoder();
    movie_reset_clock();
    if (!movie_open_decoder(m.path)) {
        m.nofile = true;
        m.completed = true;
        ds_ec_posted = true;
        ds_ec_pending = true;
        movie_logf("[movie] lecture immédiate (fichier absent) : %s\n", m.path);
    }
}

void movie_stop()
{
    movie_state &m = g_movie;
    movie_close_decoder();
    m.running = false;
    m.paused = false;
    m.completed = false;
    m.nofile = false;
    m.texture = 0;
    m.renderer = 0;
    movie_reset_clock();
}

bool movie_is_open()
{
#if CT3_HAVE_FFMPEG
    return g_movie.opened;
#else
    return false;
#endif
}

void movie_video_size(int *w, int *h)
{
#if CT3_HAVE_FFMPEG
    *w = g_movie.video_w;
    *h = g_movie.video_h;
#else
    *w = 0;
    *h = 0;
#endif
}

}  // namespace

extern "C" void rt_movie_tick(cpu_t *c)
{
    movie_state &m = g_movie;
    if (!m.running || m.paused || m.nofile) return;
    if (!m.renderer) return;
    uint32_t tex = get32(m.renderer + 0xE0);
    if (!tex) return;
#if CT3_HAVE_FFMPEG
    if (!m.opened) return;
#endif
    if (tex != m.texture) {
        uint32_t tw = 0, th = 0;
        if (!rt_d3d9_guest_size(tex, &tw, &th)) {
            int vw = 0, vh = 0;
            movie_video_size(&vw, &vh);
            tw = (uint32_t)(vw > 0 ? vw : 1);
            th = (uint32_t)(vh > 0 ? vh : 1);
        }
        if (!tw || !th) return;
        if (!movie_configure((int)tw, (int)th)) return;
        m.texture = tex;
        m.started = false;
        /* Le graphe factice ne connecte pas la broche : SetMediaType (0x40CD60) n'est jamais
         * appelé et le renderer garde une largeur, une hauteur et un pas de source non
         * initialisés. Receive les utilise pour convertir l'image : on les renseigne d'après la
         * texture cible, comme le ferait SetMediaType. */
        put32(m.renderer + 0xE8, (uint32_t)m.out_w);
        put32(m.renderer + 0xEC, (uint32_t)m.out_h);
        put32(m.renderer + 0xF0, (uint32_t)(((m.out_w + 1) * 3) & ~3));
        movie_logf("[movie] texture 0x%08x : %dx%d (pas source %d)\n", tex, m.out_w, m.out_h,
                   ((m.out_w + 1) * 3) & ~3);
    }
    if (!m.started) {
        m.started = true;
        m.clock_start = movie_now_ms();
        m.paused_total = 0.0;
        m.paused_at = 0.0;
        m.frame_count = 0;
    }
    double elapsed = (movie_now_ms() - m.clock_start - m.paused_total) / 1000.0;
    if (elapsed < 0.0) elapsed = 0.0;
    bool drew = false;
    while (!m.eof) {
        if (!m.have_frame) {
            int r = movie_next_frame();
            if (r <= 0) {
                movie_logf("[movie] décodeur : %s (r=%d, dernière %.2f s)\n",
                           r == 0 ? "fin de flux" : "erreur", r, m.last_pts);
                m.eof = true;
                break;
            }
            m.have_frame = true;
        }
        if (m.last_pts > elapsed + 0.0005) break;
        if (!movie_scale_frame()) {
            movie_logf("[movie] conversion impossible (format %d)\n", m.pix_fmt);
            m.eof = true;
            break;
        }
        m.have_frame = false;
        drew = true;
    }
    if (drew && msample_obj) {
        uint32_t save_ecx = (uint32_t)cpu_rd(c, 4, 4);
        cpu_wr(c, 4, 4, m.renderer);
        uint32_t sample = msample_obj;
        rt_call_guest(c, 0x40CE00, &sample, 1);
        cpu_wr(c, 4, 4, save_ecx);
        m.frame_count++;
        if (m.frame_count == 1 || m.frame_count % 60 == 0)
            movie_logf("[movie] %ld images, t=%.2f s\n", m.frame_count, elapsed);
    }
    double end = m.duration > 0.0 ? m.duration : m.last_pts + 0.1;
    if (!m.completed && m.eof && elapsed + 0.001 >= end) {
        m.completed = true;
        ds_ec_posted = true;
        ds_ec_pending = true;
        movie_logf("[movie] fin après %.2f s\n", elapsed);
    }
}

void ds_graph_init()
{
    if (dsg_vtable) return;
    dsg_vtable = make_vtable("dsg::", kDsgMethods, 18);
    dsc_vtable = make_vtable("dsc::", kDscMethods, 10);
    dse_vtable = make_vtable("dse::", kDseMethods, 13);
    dsa_vtable = make_vtable("dsa::", kDsaMethods, 8);
    dspos_vtable = make_vtable("dspos::", kDsposMethods, 16);
    dssrc_vtable = make_vtable("dssrc::", kDssMethods, 12);
    dspin_vtable = make_vtable("dspin::", kDsSimpleMethods, 3);
    msample_vtable = make_vtable("msample::", kMsampleMethods, 4);
    dsc_obj = new_object(dsc_vtable, 0);
    dse_obj = new_object(dse_vtable, 0);
    dsa_obj = new_object(dsa_vtable, 0);
    dspos_obj = new_object(dspos_vtable, 0);
    dssrc_obj = new_object(dssrc_vtable, 0);
    dspin_obj = new_object(dspin_vtable, 0);
    msample_obj = new_object(msample_vtable, 0);
}


// CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv)
uint32_t sh_CoCreateInstance(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (!guest_guid_is(a[0], kClsidFilterGraph)) return 0x80040111u;  // CLASS_E_CLASSNOTAVAILABLE
    if (!guest_guid_is(a[3], kIidGraphBuilder) && !guest_guid_is(a[3], kIidUnknown)) return 0x80004002u;
    ds_graph_init();
    movie_stop();
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

std::string wide_path(uint32_t p)
{
    std::string out;
    if (!p) return out;
    const uint16_t *w = reinterpret_cast<const uint16_t *>(rt_mem_base + p);
    for (size_t i = 0; i < 1024 && w[i]; i++) {
        uint16_t cp = w[i];
        if (cp < 0x80) {
            out.push_back((char)cp);
        } else if (cp < 0x800) {
            out.push_back((char)(0xC0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

// IGraphBuilder::AddFilter : le filtre du jeu est le renderer à filter+0xC (base à filter-0xC).
uint32_t sh_dsg_AddFilter(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) {
        g_movie.renderer = a[1] - 0xC;
        g_movie.texture = 0;
        movie_logf("[movie] renderer 0x%08x\n", g_movie.renderer);
    }
    return 0;
}

uint32_t sh_dsg_AddSourceFilter(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::string p = wide_path(a[1]);
    if (!p.empty()) {
        snprintf(g_movie.path, sizeof g_movie.path, "%s", p.c_str());
        movie_logf("[movie] source %s\n", p.c_str());
    }
    if (a[3]) put32(a[3], dssrc_obj);
    return 0;
}

uint32_t sh_dssrc_FindPin(cpu_t *c, const uint32_t *a)
{
    (void)c;
    put32(a[2], dspin_obj);
    return 0;
}

// IMediaControl::Run : ouvre le décodeur et attend que le jeu fournisse la texture (SetSurface).
uint32_t sh_dsc_Run(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    movie_state &m = g_movie;
    movie_logf("[movie] Run (running=%d paused=%d started=%d completed=%d)\n", m.running, m.paused,
               m.started, m.completed);
    if (!m.running) {
        movie_begin();
    } else if (m.paused) {
        m.paused = false;
        m.paused_total += movie_now_ms() - m.paused_at;
        m.paused_at = 0.0;
        if (!m.started) m.clock_start = movie_now_ms();
    } else if (m.completed && movie_is_open()) {
        m.completed = false;
        ds_ec_posted = false;
        ds_ec_pending = false;
        movie_seek_seconds(0.0);
        movie_logf("[movie] reprise après EC_COMPLETE\n");
    }
    return 0;
}

uint32_t sh_dsc_Pause(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    movie_state &m = g_movie;
    movie_logf("[movie] Pause (running=%d paused=%d)\n", m.running, m.paused);
    if (m.running && !m.paused) {
        m.paused = true;
        m.paused_at = movie_now_ms();
    }
    return 0;
}

uint32_t sh_dsc_Stop(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    movie_logf("[movie] Stop\n");
    movie_stop();
    ds_ec_pending = false;
    return 0;
}

// IMediaSample factice : GetPointer renvoie la dernière image décodée (BGR24).
uint32_t sh_msample_qi(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[2]) put32(a[2], a[0]);
    return 0;
}

uint32_t sh_msample_GetPointer(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) put32(a[1], g_movie.buf);
    return 0;
}

// IMediaPosition : 0.0 rembobine et relance (boucle des vidéos de replay).
uint32_t sh_dspos_put_CurrentPosition(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint64_t bits = ((uint64_t)a[2] << 32) | a[1];
    double pos = 0.0;
    memcpy(&pos, &bits, sizeof pos);
    movie_state &m = g_movie;
    if (m.running && movie_is_open()) {
        if (pos < 0.0) pos = 0.0;
        m.completed = false;
        ds_ec_posted = false;
        ds_ec_pending = false;
        movie_seek_seconds(pos);
        movie_logf("[movie] position -> %.2f s\n", pos);
    }
    return 0;
}

uint32_t sh_dspos_get_Duration(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) {
        double d = 0.0;
#if CT3_HAVE_FFMPEG
        d = g_movie.duration;
#endif
        memcpy(rt_mem_base + a[1], &d, sizeof d);
    }
    return 0;
}

uint32_t sh_dspos_get_CurrentPosition(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) {
        double d = 0.0;
        memcpy(rt_mem_base + a[1], &d, sizeof d);
    }
    return 0;
}

// IMediaEvent::GetEvent(this, *lEventCode, *lParam1, *lParam2, msTimeout)
uint32_t sh_dse_GetEvent(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (!ds_ec_pending) return 0x80004004u;  // E_ABORT : aucun événement
    ds_ec_pending = false;
    movie_logf("[movie] GetEvent -> EC_COMPLETE\n");
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
    {"dinput8joy::IDirectInputDevice8A::QueryInterface", 3, sh_di_QueryInterface},
    {"dinput8joy::IDirectInputDevice8A::AddRef", 1, sh_di_AddRef},
    {"dinput8joy::IDirectInputDevice8A::Release", 1, sh_di_Release},
    {"dinput8joy::IDirectInputDevice8A::GetCapabilities", 2, sh_joy_GetCapabilities},
    {"dinput8joy::IDirectInputDevice8A::EnumObjects", 4, sh_joy_EnumObjects},
    {"dinput8joy::IDirectInputDevice8A::GetProperty", 3, sh_joy_GetProperty},
    {"dinput8joy::IDirectInputDevice8A::SetProperty", 3, sh_joy_SetProperty},
    {"dinput8joy::IDirectInputDevice8A::Acquire", 1, sh_di_ok},
    {"dinput8joy::IDirectInputDevice8A::Unacquire", 1, sh_di_ok},
    {"dinput8joy::IDirectInputDevice8A::GetDeviceState", 3, sh_joy_GetDeviceState},
    {"dinput8joy::IDirectInputDevice8A::GetDeviceData", 5, sh_joy_GetDeviceData},
    {"dinput8joy::IDirectInputDevice8A::SetDataFormat", 2, sh_di_ok},
    {"dinput8joy::IDirectInputDevice8A::SetEventNotification", 2, sh_di_ok},
    {"dinput8joy::IDirectInputDevice8A::SetCooperativeLevel", 3, sh_di_ok},
    {"dinput8joy::IDirectInputDevice8A::Poll", 1, sh_di_ok},
    {"dinput8joy::IDirectInputDevice8A::CreateEffect", 4, sh_joy_notimpl},
    {"dinput8joy::IDirectInputDevice8A::EnumEffects", 4, sh_joy_notimpl},
    {"dinput8joy::IDirectInputDevice8A::SendForceFeedbackCommand", 2, sh_joy_notimpl},
    {"dinput8joy::IDirectInputDevice8A::GetForceFeedbackState", 2, sh_joy_notimpl},
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
    {"dsg::AddFilter", 3, sh_dsg_AddFilter},
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
    {"dsc::Pause", 1, sh_dsc_Pause},
    {"dsc::Stop", 1, sh_dsc_Stop},
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
    {"dspos::GetTypeInfoCount", 2, sh_ds_zero},
    {"dspos::GetTypeInfo", 4, sh_ds_zero},
    {"dspos::GetIDsOfNames", 6, sh_ds_zero},
    {"dspos::Invoke", 8, sh_ds_zero},
    {"dspos::get_Duration", 2, sh_dspos_get_Duration},
    {"dspos::put_CurrentPosition", 3, sh_dspos_put_CurrentPosition},
    {"dspos::get_CurrentPosition", 2, sh_dspos_get_CurrentPosition},
    {"dspos::get_StopTime", 2, sh_ds_zero},
    {"dspos::put_StopTime", 3, sh_ds_ok},
    {"dspos::get_PrerollTime", 2, sh_ds_zero},
    {"dspos::put_PrerollTime", 3, sh_ds_ok},
    {"dspos::put_Rate", 3, sh_ds_ok},
    {"dspos::get_Rate", 2, sh_ds_zero},
    {"msample::QueryInterface", 3, sh_msample_qi},
    {"msample::AddRef", 1, sh_ds_addref},
    {"msample::Release", 1, sh_ds_release},
    {"msample::GetPointer", 2, sh_msample_GetPointer},
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
    {"dsoundcom3d::IDirectSound3DBuffer::SetMaxDistance", 3, sh_ds3d_SetMaxDistance},
    {"dsoundcom3d::IDirectSound3DBuffer::SetMinDistance", 3, sh_ds3d_SetMinDistance},
    {"dsoundcom3d::IDirectSound3DBuffer::SetMode", 3, sh_ds3d_ok},
    {"dsoundcom3d::IDirectSound3DBuffer::SetPosition", 5, sh_ds3d_SetPosition},
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
    {"dsoundcom::IDirectSoundBuffer::GetObjectInPath", 5, sh_ds_objinpath},
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
