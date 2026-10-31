// Talons GDI32, ADVAPI32, ole32 et petites fonctions KERNEL32 (voir rt_shims.c pour les autres).
//
// GDI : les objets (pinceaux, polices, DC, DIB) sont des handles fictifs. Le texte n'est pas encore
// dessiné : ExtTextOutA ne fait rien (signalé une fois). Les DIB sections sont de vraies zones de
// mémoire invitée, pour un rendu futur.

#include "rt_proc.h"
#include "rt_shim.h"

#include <string>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

uint32_t sh_GetStockObject(cpu_t *c, const uint32_t *a) { (void)c; return kStockBase + a[0]; }
uint32_t sh_CreateCompatibleDC(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return new_obj(); }
uint32_t sh_DeleteDC(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
uint32_t sh_DeleteObject(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
uint32_t sh_SelectObject(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return kObjBase; }
uint32_t sh_SetBkColor(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_SetTextColor(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_SetTextAlign(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_SetMapMode(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
uint32_t sh_CreateFontA(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return new_obj(); }

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

uint32_t sh_GetTextExtentPoint32A(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // (hdc, lpString, c, lpSize) : largeur approchée, à remplacer par la métrique réelle de la police.
    put32(a[3] + 0, a[2] * 8);
    put32(a[3] + 4, 16);
    return 1;
}

uint32_t sh_ExtTextOutA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    if (!warned_text) {
        warned_text = true;
        fprintf(stderr, "rt: ExtTextOutA : texte non rendu (pas encore pris en charge)\n");
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
    return new_obj();
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

uint32_t sh_CoCreateInstance(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0x80040111u;  // CLASS_E_CLASSNOTAVAILABLE
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

uint32_t make_vtable(const char *prefix, const char *const *names, unsigned count)
{
    uint32_t vt = rt_heap_alloc(4 * count, 1);
    for (unsigned k = 0; k < count; k++) {
        std::string full = std::string(prefix) + names[k];
        uint32_t stub = rt_dyn_stub_addr(full.c_str());
        memcpy(rt_mem_base + vt + 4 * k, &stub, 4);
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

uint32_t sh_DirectSoundCreate(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (!ds_vtable) ds_vtable = make_vtable("dsoundcom::IDirectSound::", kDsMethods, 11);
    if (!dsb_vtable) dsb_vtable = make_vtable("dsoundcom::IDirectSoundBuffer::", kDsbMethods, 24);
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
    return 0;
}

// Lock(this, dwOffset, dwBytes, ppvAudioPtr1, pdwAudioBytes1, ppvAudioPtr2, pdwAudioBytes2, dwFlags)
uint32_t sh_dsb_Lock(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint32_t storage = get32(a[0] + 4);
    uint32_t size = get32(a[0] + 8);
    uint32_t off = a[1] < size ? a[1] : 0;
    uint32_t n = a[2] ? a[2] : size - off;
    if (a[3]) put32(a[3], storage + off);
    if (a[4]) put32(a[4], n);
    if (a[5]) put32(a[5], 0);
    if (a[6]) put32(a[6], 0);
    return 0;
}

uint32_t sh_dsb_GetCurrentPosition(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) put32(a[1], 0);
    if (a[2]) put32(a[2], 0);
    return 0;
}

uint32_t sh_dsb_GetStatus(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) put32(a[1], 0);
    return 0;
}

// GetFormat(this, lpwfx, cbwfx, lpdwWritten) : WAVEFORMATEX PCM 44,1 kHz stéréo 16 bits.
uint32_t sh_dsb_GetFormat(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint8_t wf[18] = {0};
    uint16_t tag = 1, ch = 2, align = 4, bits = 16, cb = 0;
    uint32_t rate = 44100, avg = 176400;
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

// GetFrequency / GetCaps : valeurs constantes (fréquence 44,1 kHz ; DSBCAPS de 96 octets).
uint32_t sh_dsb_GetFrequency(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1]) put32(a[1], 44100);
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
uint32_t sh_di_CreateDevice(cpu_t *c, const uint32_t *a) { (void)c; if (a[2]) put32(a[2], 0); return 0x80040154u; }  // DIERR_DEVICENOTREG
uint32_t sh_di_EnumDevices(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
uint32_t sh_di_fail(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0x80004005u; }
uint32_t sh_di_ok(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }

}  // namespace

// Table des talons (consultée par rt_shims.c).
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
    {"dinput8com::IDirectInput8A::EnumDevices", 5, sh_di_EnumDevices},
    {"dinput8com::IDirectInput8A::GetDeviceStatus", 2, sh_di_fail},
    {"dinput8com::IDirectInput8A::RunControlPanel", 3, sh_di_fail},
    {"dinput8com::IDirectInput8A::Initialize", 3, sh_di_ok},
    {"dinput8com::IDirectInput8A::FindDevice", 4, sh_di_fail},
    {"dinput8com::IDirectInput8A::EnumDevicesBySemantics", 6, sh_di_EnumDevices},
    {"dinput8com::IDirectInput8A::ConfigureDevices", 5, sh_di_fail},
    {"d3d9.dll::DebugSetMute", 0, sh_DebugSetMute},
    {"DSOUND.dll::#11", 3, sh_DirectSoundCreate},
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
    {"dsoundcom::IDirectSoundBuffer::QueryInterface", 3, sh_ds_qi},
    {"dsoundcom::IDirectSoundBuffer::AddRef", 1, sh_ds_addref},
    {"dsoundcom::IDirectSoundBuffer::Release", 1, sh_ds_release},
    {"dsoundcom::IDirectSoundBuffer::GetCaps", 2, sh_dsb_GetCaps},
    {"dsoundcom::IDirectSoundBuffer::GetCurrentPosition", 3, sh_dsb_GetCurrentPosition},
    {"dsoundcom::IDirectSoundBuffer::GetFormat", 4, sh_dsb_GetFormat},
    {"dsoundcom::IDirectSoundBuffer::GetVolume", 2, sh_ds_zero},
    {"dsoundcom::IDirectSoundBuffer::GetPan", 2, sh_ds_zero},
    {"dsoundcom::IDirectSoundBuffer::GetFrequency", 2, sh_dsb_GetFrequency},
    {"dsoundcom::IDirectSoundBuffer::GetStatus", 2, sh_dsb_GetStatus},
    {"dsoundcom::IDirectSoundBuffer::Initialize", 3, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::Lock", 8, sh_dsb_Lock},
    {"dsoundcom::IDirectSoundBuffer::Play", 4, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::SetCurrentPosition", 2, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::SetFormat", 2, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::SetVolume", 2, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::SetPan", 2, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::SetFrequency", 2, sh_ds_ok},
    {"dsoundcom::IDirectSoundBuffer::Stop", 1, sh_ds_ok},
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
