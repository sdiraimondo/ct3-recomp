#include "rt.h"
#include "rt_pe.h"
#include "rt_proc.h"
#include "rt_thread.h"
#include "rt_file.h"
#include "rt_shim.h"

#include <ctype.h>
#include <pthread.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static rt_pe_image *rt_image;

/* Journal des appels interceptés (utilisé par les tests G2). */
unsigned rt_shim_calls;
uint32_t rt_shim_last_arg0;

/* Trace des imports : 1 = écrit chaque appel sur stderr. */
int rt_trace_imports = 0;  /* CT3_IMPORTS=1 : trace des imports (volumineuse) */


static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

static char exe_path[1024] = "CT3.exe";

static uint32_t guest_str_copy(const char *s)
{
    uint32_t n = (uint32_t)strlen(s) + 1;
    uint32_t p = rt_scratch_alloc(n);
    memcpy(rt_mem_base + p, s, n);
    return p;
}

static uint32_t sh_CloseHandle(cpu_t *c, const uint32_t *a) { (void)c; rt_shim_calls++; rt_shim_last_arg0 = a[0]; rt_handle_close(a[0]); return 1; }
static uint32_t sh_GetVersion(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0x0A280105u; /* NT 5.1, build 2600 */ }
static uint32_t sh_GetCommandLineA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    static uint32_t p;
    if (!p) {
        char line[1100];
        snprintf(line, sizeof line, "\"%s\"", exe_path);
        p = guest_str_copy(line);
    }
    return p;
}
static uint32_t sh_GetStartupInfoA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    memset(rt_mem_base + a[0], 0, 68);
    uint32_t cb = 68;
    memcpy(rt_mem_base + a[0], &cb, 4);
    return 0;
}
/* STD_INPUT_HANDLE = -10, STD_OUTPUT = -11, STD_ERROR = -12 : pseudo-handles 0x10, 0x11, 0x12. */
static uint32_t sh_GetStdHandle(cpu_t *c, const uint32_t *a) { (void)c; return 0x10u + (0u - a[0] - 10u); }
static uint32_t sh_SetHandleCount(cpu_t *c, const uint32_t *a) { (void)c; return a[0]; }
static uint32_t sh_GetFileType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (rt_file_fd(a[0]) >= 0) return 1;  /* FILE_TYPE_DISK */
    return a[0] >= 0x10 && a[0] <= 0x12 ? 2 : 0;  /* FILE_TYPE_CHAR (stdio) ou inconnu */
}
static uint32_t sh_GetProcessHeap(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0x30000000u; }
static uint32_t sh_HeapCreate(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0x30000001u; }
/* CT3_ALLOCLOG=1 : journal des allocations et libérations avec les adresses de retour de code du jeu
 * trouvées sur la pile (diagnostic de fuite). */
static int alloclog = -1;
static void log_heap(cpu_t *c, const char *what, uint32_t size)
{
    if (alloclog < 0) alloclog = getenv("CT3_ALLOCLOG") != NULL;
    if (!alloclog) return;
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    fprintf(stderr, "[%s] taille=0x%x", what, size);
    unsigned n = 0;
    for (uint32_t k = 0; k < 160 && n < 2 && esp + 4 * k < 0x103FFF00u; k++) {
        uint32_t v = (uint32_t)guest_ld(c, esp + 4 * k, 4);
        if (v >= 0x401000u && v < 0x540000u && (v < 0x514000u || v >= 0x51F000u)) {
            fprintf(stderr, " 0x%08x", v);
            n++;
        }
    }
    fprintf(stderr, "\n");
}
static uint32_t sh_HeapAlloc(cpu_t *c, const uint32_t *a) { log_heap(c, "alloc", a[2]); return rt_heap_alloc(a[2], (a[1] & 0x8) != 0); }
static uint32_t sh_HeapFree(cpu_t *c, const uint32_t *a) { log_heap(c, "libre", a[2] ? rt_heap_size(a[2]) : 0); rt_heap_free(a[2]); return 1; }
static uint32_t sh_HeapSize(cpu_t *c, const uint32_t *a) { (void)c; return rt_heap_size(a[2]); }
static uint32_t sh_HeapReAlloc(cpu_t *c, const uint32_t *a) { (void)c; return rt_heap_realloc(a[2], a[3]); }
static uint32_t sh_GetEnvironmentStringsW(cpu_t *c, const uint32_t *a) { (void)c; (void)a; static uint32_t p; if (!p) { p = rt_scratch_alloc(4); memset(rt_mem_base + p, 0, 4); } return p; }
static uint32_t sh_FreeEnvironmentStringsW(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
static uint32_t sh_GetACP(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1252; }
static uint32_t sh_GetOEMCP(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 437; }
static uint32_t sh_GetCPInfo(cpu_t *c, const uint32_t *a)
{
    (void)c;
    memset(rt_mem_base + a[1], 0, 18);
    uint32_t one = 1;
    memcpy(rt_mem_base + a[1], &one, 4); /* MaxCharSize */
    return 1;
}
static uint32_t sh_IsValidCodePage(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
static uint32_t sh_InitializeCriticalSection(cpu_t *c, const uint32_t *a) { (void)c; memset(rt_mem_base + a[0], 0, 24); rt_cs_init(a[0]); return 0; }
static uint32_t sh_EnterCriticalSection(cpu_t *c, const uint32_t *a) { (void)c; rt_cs_enter(a[0]); return 0; }
static uint32_t sh_LeaveCriticalSection(cpu_t *c, const uint32_t *a) { (void)c; rt_cs_leave(a[0]); return 0; }
static uint32_t sh_DeleteCriticalSection(cpu_t *c, const uint32_t *a) { (void)c; rt_cs_delete(a[0]); return 0; }
static uint32_t sh_NoopCS(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
static uint32_t tls_used;
static uint32_t sh_TlsAlloc(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return tls_used < RT_TLS_SLOTS ? tls_used++ : 0xFFFFFFFFu; }
static uint32_t sh_TlsGetValue(cpu_t *c, const uint32_t *a) { (void)c; return a[0] < RT_TLS_SLOTS ? rt_current()->tls[a[0]] : 0; }
static uint32_t sh_TlsSetValue(cpu_t *c, const uint32_t *a) { (void)c; if (a[0] < RT_TLS_SLOTS) rt_current()->tls[a[0]] = a[1]; return a[0] < RT_TLS_SLOTS; }
static uint32_t sh_TlsFree(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
static uint32_t sh_SetUnhandledExceptionFilter(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
static uint32_t sh_GetCurrentThreadId(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return rt_current()->id; }
static uint32_t sh_GetCurrentProcessId(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1234; }
static uint32_t sh_GetTickCount(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return now_ms(); }
static uint32_t sh_timeGetTime(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return now_ms(); }
uint32_t rt_d3d9_create(uint32_t sdk_version);
static uint32_t sh_Direct3DCreate9(cpu_t *c, const uint32_t *a) { (void)c; return rt_d3d9_create(a[0]); }

static uint32_t sh_GetLastError(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return rt_error; }
static uint32_t sh_SetLastError(cpu_t *c, const uint32_t *a) { (void)c; rt_error = a[0]; return 0; }

/* Fichiers (rt_file.c) : lit la chaîne ASCII invitée, bornée à la taille du tampon. */
static uint32_t sh_CreateFileA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    char name[1024];
    unsigned i = 0;
    for (; i < sizeof name - 1; i++) {
        char ch = (char)rt_mem_base[a[0] + i];
        if (ch == 0) break;
        name[i] = ch;
    }
    name[i] = 0;
    if (getenv("CT3_FILELOG")) fprintf(stderr, "[ouverture] %s\n", name);
    return rt_file_open(name, a[1], a[4]);
}
static uint32_t sh_ReadFile(cpu_t *c, const uint32_t *a) { (void)c; return (uint32_t)rt_file_read(a[0], a[1], a[2], a[3]); }
static uint32_t sh_WriteFile(cpu_t *c, const uint32_t *a) { (void)c; return (uint32_t)rt_file_write(a[0], a[1], a[2], a[3]); }
static uint32_t sh_SetFilePointer(cpu_t *c, const uint32_t *a) { (void)c; return rt_file_seek(a[0], a[1], a[2], a[3]); }
static uint32_t sh_GetFileSize(cpu_t *c, const uint32_t *a) { (void)c; return rt_file_size(a[0], a[1]); }
static uint32_t sh_VirtualAlloc(cpu_t *c, const uint32_t *a) { (void)c; return rt_heap_alloc(a[1], 1); }
static uint32_t sh_VirtualFree(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
static uint32_t sh_InitializeCriticalSectionAndSpinCount(cpu_t *c, const uint32_t *a) { (void)c; memset(rt_mem_base + a[0], 0, 24); rt_cs_init(a[0]); return 1; }
/* Fiber local storage : un seul fil, valeurs stockées par index. */
static uint32_t fls_values[256];
static uint32_t fls_next;
static uint32_t sh_FlsAlloc(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return fls_next < 256 ? fls_next++ : 0xFFFFFFFFu; }
static uint32_t sh_FlsGetValue(cpu_t *c, const uint32_t *a) { (void)c; return a[0] < 256 ? fls_values[a[0]] : 0; }
static uint32_t sh_FlsSetValue(cpu_t *c, const uint32_t *a) { (void)c; if (a[0] < 256) fls_values[a[0]] = a[1]; return 1; }
static uint32_t sh_FlsFree(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
/* Conversions : pages de codes monooctet (1252, OEM, ACP) traitées comme Latin-1 ; hors plage -> '?'. */
static uint32_t sh_MultiByteToWideChar(cpu_t *c, const uint32_t *a)
{
    (void)c;
    int32_t srclen = (int32_t)a[3];
    uint32_t n = srclen == -1 ? (uint32_t)strlen((const char *)(rt_mem_base + a[2])) + 1 : (uint32_t)srclen;
    if (a[4] == 0) return n;
    if (a[5] < n) return 0;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t w = rt_mem_base[a[2] + i];
        memcpy(rt_mem_base + a[4] + 2 * i, &w, 2);
    }
    return n;
}
static uint32_t sh_WideCharToMultiByte(cpu_t *c, const uint32_t *a)
{
    (void)c;
    int32_t srclen = (int32_t)a[3];
    uint32_t n = 0;
    if (srclen == -1) {
        while (1) {
            uint16_t w;
            memcpy(&w, rt_mem_base + a[2] + 2 * n, 2);
            n++;
            if (w == 0) break;
        }
    } else {
        n = (uint32_t)srclen;
    }
    if (a[4] == 0) return n;
    if (a[5] < n) return 0;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t w;
        memcpy(&w, rt_mem_base + a[2] + 2 * i, 2);
        rt_mem_base[a[4] + i] = w <= 0xFF ? (uint8_t)w : '?';
    }
    return n;
}
/* Types de caractères CT_CTYPE1 (C1_*), sur ASCII. Au-delà de 127 : 0 (locale C). */
static uint16_t ctype1(uint32_t ch)
{
    uint16_t t = 0;
    if (ch > 127) return 0;
    if (isupper((int)ch)) t |= 0x001 | 0x100;
    if (islower((int)ch)) t |= 0x002 | 0x100;
    if (isdigit((int)ch)) t |= 0x004;
    if (isspace((int)ch)) t |= 0x008;
    if (ispunct((int)ch)) t |= 0x010;
    if (iscntrl((int)ch)) t |= 0x020;
    if (ch == ' ' || ch == '\t') t |= 0x040;
    if (isxdigit((int)ch)) t |= 0x080;
    if (isalpha((int)ch)) t |= 0x100;
    return t;
}
static uint32_t get_string_type(const uint32_t *a, int wide)
{
    if (a[0] != 1) rt_fatal("GetStringType : type d'information non implémenté");
    uint32_t n = a[2];
    uint32_t out = a[3];
    for (uint32_t i = 0; i < n; i++) {
        uint32_t ch = wide ? (uint32_t)(uint16_t)guest_ld(NULL, a[1] + 2 * i, 2) : rt_mem_base[a[1] + i];
        uint16_t t = ctype1(ch);
        memcpy(rt_mem_base + out + 2 * i, &t, 2);
    }
    return 1;
}
static uint32_t sh_GetStringTypeW(cpu_t *c, const uint32_t *a) { (void)c; return get_string_type(a, 1); }
static uint32_t sh_GetStringTypeA(cpu_t *c, const uint32_t *a) { (void)c; return get_string_type(a, 0); }
/* LCMapString : LCMAP_LOWERCASE (0x100) et LCMAP_UPPERCASE (0x200) sur ASCII ; autres indicateurs refusés. */
static uint32_t lcmap(const uint32_t *a, int wide)
{
    uint32_t flags = a[1], n = a[3];
    if (flags != 0x100 && flags != 0x200) rt_fatal("LCMapString : indicateurs non implémentés");
    if (n == (uint32_t)-1) rt_fatal("LCMapString : longueur -1 non implémentée");
    if (a[5] == 0) return n;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t ch = wide ? (uint32_t)(uint16_t)guest_ld(NULL, a[2] + 2 * i, 2) : rt_mem_base[a[2] + i];
        if (ch < 128) ch = flags == 0x100 ? (uint32_t)tolower((int)ch) : (uint32_t)toupper((int)ch);
        if (wide) {
            uint16_t w = (uint16_t)ch;
            memcpy(rt_mem_base + a[4] + 2 * i, &w, 2);
        } else {
            rt_mem_base[a[4] + i] = (uint8_t)ch;
        }
    }
    return n;
}
static uint32_t sh_LCMapStringW(cpu_t *c, const uint32_t *a) { (void)c; return lcmap(a, 1); }
static uint32_t sh_LCMapStringA(cpu_t *c, const uint32_t *a) { (void)c; return lcmap(a, 0); }
/* Chemin absolu de l'exécutable, fourni par le lanceur (rt_set_exe_path). */

void rt_set_exe_path(const char *path)
{
    snprintf(exe_path, sizeof exe_path, "%s", path);
    rt_file_set_base(exe_path);
}

static uint32_t sh_GetModuleFileNameA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint32_t len = (uint32_t)strlen(exe_path);
    if (a[2] == 0) return 0;
    uint32_t n = len < a[2] - 1 ? len : a[2] - 1;
    memcpy(rt_mem_base + a[1], exe_path, n);
    rt_mem_base[a[1] + n] = 0;
    return n;
}
/* PF_* de IsProcessorFeaturePresent : cohérent avec le profil CPUID (rt_cpu.c). */
static uint32_t sh_IsProcessorFeaturePresent(cpu_t *c, const uint32_t *a)
{
    (void)c;
    switch (a[0]) {
    case 3:  /* MMX */
    case 6:  /* SSE */
    case 7:  /* 3DNow! */
    case 8:  /* RDTSC */
    case 10: /* SSE2 */
        return 1;
    default:
        return 0;
    }
}
/* Temps : horloge de l'hôte. FILETIME = 100 ns depuis le 1er janvier 1601. */
static void host_now(struct timespec *ts) { clock_gettime(CLOCK_REALTIME, ts); }

static uint32_t sh_GetSystemTimeAsFileTime(cpu_t *c, const uint32_t *a)
{
    (void)c;
    struct timespec ts;
    host_now(&ts);
    uint64_t ft = ((uint64_t)ts.tv_sec + 11644473600ULL) * 10000000ULL + (uint64_t)ts.tv_nsec / 100;
    uint32_t lo = (uint32_t)ft, hi = (uint32_t)(ft >> 32);
    memcpy(rt_mem_base + a[0], &lo, 4);
    memcpy(rt_mem_base + a[0] + 4, &hi, 4);
    return 0;
}

/* SYSTEMTIME : wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds (16 octets). */
static void fill_systemtime(uint32_t dst, const struct tm *t, unsigned ms)
{
    uint16_t v[8] = {(uint16_t)(t->tm_year + 1900), (uint16_t)(t->tm_mon + 1), (uint16_t)t->tm_wday,
                     (uint16_t)t->tm_mday, (uint16_t)t->tm_hour, (uint16_t)t->tm_min, (uint16_t)t->tm_sec,
                     (uint16_t)ms};
    memcpy(rt_mem_base + dst, v, 16);
}

static uint32_t sh_GetSystemTime(cpu_t *c, const uint32_t *a)
{
    (void)c;
    struct timespec ts;
    host_now(&ts);
    struct tm t;
    gmtime_r(&ts.tv_sec, &t);
    fill_systemtime(a[0], &t, (unsigned)(ts.tv_nsec / 1000000));
    return 0;
}

static uint32_t sh_GetLocalTime(cpu_t *c, const uint32_t *a)
{
    (void)c;
    struct timespec ts;
    host_now(&ts);
    struct tm t;
    localtime_r(&ts.tv_sec, &t);
    fill_systemtime(a[0], &t, (unsigned)(ts.tv_nsec / 1000000));
    return 0;
}

/* Compteur haute résolution : nanosecondes monotones, fréquence 1 GHz. */
static uint32_t sh_QueryPerformanceCounter(cpu_t *c, const uint32_t *a)
{
    (void)c;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t v = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    memcpy(rt_mem_base + a[0], &v, 8);
    return 1;
}

static uint32_t sh_QueryPerformanceFrequency(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint64_t v = 1000000000ULL;
    memcpy(rt_mem_base + a[0], &v, 8);
    return 1;
}

/* Sleep : attente réelle de l'hôte (la cadence du jeu dépend de ce délai, voir D-017). */
static uint32_t sh_Sleep(cpu_t *c, const uint32_t *a)
{
    uint32_t ms = a[0];
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    uint32_t ret = (uint32_t)guest_ld(c, esp, 4);
    /* CT3_PRESENT_LOG : durée demandée et adresse de retour de l'appelant (diagnostic du démarrage). */
    if (getenv("CT3_PRESENT_LOG")) {
        struct timespec sts;
        clock_gettime(CLOCK_MONOTONIC, &sts);
        fprintf(stderr, "[sleep] ms=%ld %u ms (retour 0x%08x)\n", (long)(sts.tv_sec * 1000 + sts.tv_nsec / 1000000), ms, ret);
    }
    rt_sleep_ms(ms);
    return 0;
}

/* Opérations atomiques : un seul fil d'exécution pour l'instant, donc opérations ordinaires. */
static uint32_t atomic_get(uint32_t p) { uint32_t v; memcpy(&v, rt_mem_base + p, 4); return v; }
static void atomic_set(uint32_t p, uint32_t v) { memcpy(rt_mem_base + p, &v, 4); }
static uint32_t sh_InterlockedIncrement(cpu_t *c, const uint32_t *a) { (void)c; uint32_t v = atomic_get(a[0]) + 1; atomic_set(a[0], v); return v; }
static uint32_t sh_InterlockedDecrement(cpu_t *c, const uint32_t *a) { (void)c; uint32_t v = atomic_get(a[0]) - 1; atomic_set(a[0], v); return v; }
static uint32_t sh_InterlockedExchange(cpu_t *c, const uint32_t *a) { (void)c; uint32_t old = atomic_get(a[0]); atomic_set(a[0], a[1]); return old; }
static uint32_t sh_InterlockedCompareExchange(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint32_t old = atomic_get(a[0]);
    if (old == a[2]) atomic_set(a[0], a[1]);
    return old;
}
static uint32_t sh_GetCurrentThread(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0xFFFFFFFEu; }
/* ---- threads et synchronisation (rt_thread.c) ---- */
static uint32_t sh_CreateThread(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint32_t id = 0;
    uint32_t h = rt_thread_create(a[2], a[3], (a[4] & 4) != 0, &id);
    if (a[5]) memcpy(rt_mem_base + a[5], &id, 4);
    return h;
}
static uint32_t sh_ExitThread(cpu_t *c, const uint32_t *a)
{
    (void)c;
    rt_current()->exit_code = a[0];
    rt_current()->finished = 1;
    rt_notify_state();
    rt_unlock();
    pthread_exit(NULL);
}
static uint32_t sh_GetExitCodeThread(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint32_t code = rt_thread_exit_code(a[0], NULL);
    memcpy(rt_mem_base + a[1], &code, 4);
    return 1;
}
static uint32_t sh_ResumeThread(cpu_t *c, const uint32_t *a) { (void)c; return rt_thread_resume(a[0]) ? 1 : 0xFFFFFFFFu; }
static uint32_t sh_SuspendThread(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
static uint32_t sh_TerminateThread(cpu_t *c, const uint32_t *a) { (void)c; rt_thread_terminate(a[0], a[1]); return 1; }
static uint32_t sh_SetThreadPriority(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
static uint32_t sh_GetThreadPriority(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
static uint32_t sh_CreateEventA(cpu_t *c, const uint32_t *a) { (void)c; return rt_event_create(a[1] != 0, a[2] != 0); }
static uint32_t sh_SetEvent(cpu_t *c, const uint32_t *a) { (void)c; return rt_event_set(a[0], 1); }
static uint32_t sh_ResetEvent(cpu_t *c, const uint32_t *a) { (void)c; return rt_event_set(a[0], 0); }
static uint32_t sh_CreateMutexA(cpu_t *c, const uint32_t *a) { (void)c; return rt_mutex_create(a[1] != 0); }
static uint32_t sh_ReleaseMutex(cpu_t *c, const uint32_t *a) { (void)c; return rt_mutex_release(a[0]); }
static uint32_t sh_WaitForSingleObject(cpu_t *c, const uint32_t *a) { (void)c; return rt_wait_handles(&a[0], 1, 0, a[1]); }
static uint32_t sh_WaitForMultipleObjects(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint32_t hs[64];
    if (a[0] > 64) return 0xFFFFFFFFu;
    for (uint32_t i = 0; i < a[0]; i++) memcpy(&hs[i], rt_mem_base + a[1] + 4 * i, 4);
    return rt_wait_handles(hs, a[0], a[2] != 0, a[3]);
}
static uint32_t sh_timeSetEvent(cpu_t *c, const uint32_t *a)
{
    (void)c;
    /* timeSetEvent(uDelay, uResolution, lpTimeProc, dwUser, fuEvent) */
    return rt_timer_create(a[0], a[2], a[3], a[2], a[4]);
}
static uint32_t sh_timeKillEvent(cpu_t *c, const uint32_t *a) { (void)c; rt_timer_kill(a[0]); return 0; }
static uint32_t sh_timeBeginPeriod(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
static uint32_t sh_timeEndPeriod(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 0; }
static uint32_t sh_InitCommonControls(cpu_t *c, const uint32_t *a) { (void)c; (void)a; return 1; }
/* OSVERSIONINFOA : dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId, szCSDVersion. */
static uint32_t sh_GetVersionExA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    uint32_t v[5] = {0, 5, 1, 2600, 2};
    memcpy(&v[0], rt_mem_base + a[0], 4);
    memcpy(rt_mem_base + a[0], v, 4 * 5);
    memset(rt_mem_base + a[0] + 20, 0, 128);
    return 1;
}
static uint32_t sh_ExitProcess(cpu_t *c, const uint32_t *a)
{
    (void)c;
    fprintf(stderr, "rt: ExitProcess(%u)\n", a[0]);
    exit((int)a[0]);
}

static uint32_t sh_GetModuleHandleA_real(cpu_t *c, const uint32_t *a);
static uint32_t sh_GetProcAddress(cpu_t *c, const uint32_t *a);
static const rt_shim_entry *find_entry(const char *name);
#define RT_SHIM_MAX_ARGS 16
static uint32_t sh_LoadLibraryA(cpu_t *c, const uint32_t *a);

static const rt_shim_entry rt_shims[] = {
    {"KERNEL32.dll::CloseHandle", 1, sh_CloseHandle},
    {"KERNEL32.dll::GetVersion", 0, sh_GetVersion},
    {"KERNEL32.dll::CreateThread", 6, sh_CreateThread},
    {"KERNEL32.dll::ExitThread", 1, sh_ExitThread},
    {"KERNEL32.dll::GetExitCodeThread", 2, sh_GetExitCodeThread},
    {"KERNEL32.dll::ResumeThread", 1, sh_ResumeThread},
    {"KERNEL32.dll::SuspendThread", 1, sh_SuspendThread},
    {"KERNEL32.dll::TerminateThread", 2, sh_TerminateThread},
    {"KERNEL32.dll::SetThreadPriority", 2, sh_SetThreadPriority},
    {"KERNEL32.dll::GetThreadPriority", 1, sh_GetThreadPriority},
    {"KERNEL32.dll::CreateEventA", 4, sh_CreateEventA},
    {"KERNEL32.dll::SetEvent", 1, sh_SetEvent},
    {"KERNEL32.dll::ResetEvent", 1, sh_ResetEvent},
    {"KERNEL32.dll::CreateMutexA", 3, sh_CreateMutexA},
    {"KERNEL32.dll::ReleaseMutex", 1, sh_ReleaseMutex},
    {"KERNEL32.dll::WaitForSingleObject", 2, sh_WaitForSingleObject},
    {"KERNEL32.dll::WaitForMultipleObjects", 4, sh_WaitForMultipleObjects},
    {"WINMM.dll::timeSetEvent", 5, sh_timeSetEvent},
    {"WINMM.dll::timeKillEvent", 1, sh_timeKillEvent},
    {"WINMM.dll::timeBeginPeriod", 1, sh_timeBeginPeriod},
    {"WINMM.dll::timeEndPeriod", 1, sh_timeEndPeriod},
    {"KERNEL32.dll::InterlockedIncrement", 1, sh_InterlockedIncrement},
    {"KERNEL32.dll::InterlockedDecrement", 1, sh_InterlockedDecrement},
    {"KERNEL32.dll::InterlockedExchange", 2, sh_InterlockedExchange},
    {"KERNEL32.dll::InterlockedCompareExchange", 3, sh_InterlockedCompareExchange},
    {"KERNEL32.dll::GetCurrentThread", 0, sh_GetCurrentThread},
    {"KERNEL32.dll::GetSystemTimeAsFileTime", 1, sh_GetSystemTimeAsFileTime},
    {"KERNEL32.dll::GetSystemTime", 1, sh_GetSystemTime},
    {"KERNEL32.dll::GetLocalTime", 1, sh_GetLocalTime},
    {"KERNEL32.dll::QueryPerformanceCounter", 1, sh_QueryPerformanceCounter},
    {"KERNEL32.dll::QueryPerformanceFrequency", 1, sh_QueryPerformanceFrequency},
    {"KERNEL32.dll::Sleep", 1, sh_Sleep},
    {"KERNEL32.dll::IsProcessorFeaturePresent", 1, sh_IsProcessorFeaturePresent},
    {"KERNEL32.dll::GetModuleFileNameA", 3, sh_GetModuleFileNameA},
    {"KERNEL32.dll::LCMapStringW", 6, sh_LCMapStringW},
    {"KERNEL32.dll::LCMapStringA", 6, sh_LCMapStringA},
    {"KERNEL32.dll::GetStringTypeW", 4, sh_GetStringTypeW},
    {"KERNEL32.dll::GetStringTypeA", 5, sh_GetStringTypeA},
    {"KERNEL32.dll::MultiByteToWideChar", 6, sh_MultiByteToWideChar},
    {"KERNEL32.dll::WideCharToMultiByte", 8, sh_WideCharToMultiByte},
    {"KERNEL32.dll::GetVersionExA", 1, sh_GetVersionExA},
    {"KERNEL32.dll::GetModuleHandleA", 1, sh_GetModuleHandleA_real},
    {"KERNEL32.dll::GetProcAddress", 2, sh_GetProcAddress},
    {"KERNEL32.dll::LoadLibraryA", 1, sh_LoadLibraryA},
    {"KERNEL32.dll::GetCommandLineA", 0, sh_GetCommandLineA},
    {"KERNEL32.dll::GetStartupInfoA", 1, sh_GetStartupInfoA},
    {"KERNEL32.dll::GetStdHandle", 1, sh_GetStdHandle},
    {"KERNEL32.dll::SetHandleCount", 1, sh_SetHandleCount},
    {"KERNEL32.dll::GetFileType", 1, sh_GetFileType},
    {"d3d9.dll::Direct3DCreate9", 1, sh_Direct3DCreate9},
    {"KERNEL32.dll::CreateFileA", 7, sh_CreateFileA},
    {"KERNEL32.dll::ReadFile", 5, sh_ReadFile},
    {"KERNEL32.dll::WriteFile", 5, sh_WriteFile},
    {"KERNEL32.dll::SetFilePointer", 4, sh_SetFilePointer},
    {"KERNEL32.dll::GetFileSize", 2, sh_GetFileSize},
    {"KERNEL32.dll::GetProcessHeap", 0, sh_GetProcessHeap},
    {"KERNEL32.dll::HeapCreate", 3, sh_HeapCreate},
    {"KERNEL32.dll::HeapAlloc", 3, sh_HeapAlloc},
    {"KERNEL32.dll::HeapFree", 3, sh_HeapFree},
    {"KERNEL32.dll::HeapSize", 3, sh_HeapSize},
    {"KERNEL32.dll::HeapReAlloc", 4, sh_HeapReAlloc},
    {"KERNEL32.dll::GetEnvironmentStringsW", 0, sh_GetEnvironmentStringsW},
    {"KERNEL32.dll::FreeEnvironmentStringsW", 1, sh_FreeEnvironmentStringsW},
    {"KERNEL32.dll::GetACP", 0, sh_GetACP},
    {"KERNEL32.dll::GetOEMCP", 0, sh_GetOEMCP},
    {"KERNEL32.dll::GetCPInfo", 2, sh_GetCPInfo},
    {"KERNEL32.dll::IsValidCodePage", 1, sh_IsValidCodePage},
    {"KERNEL32.dll::InitializeCriticalSection", 1, sh_InitializeCriticalSection},
    {"KERNEL32.dll::InitializeCriticalSectionAndSpinCount", 2, sh_InitializeCriticalSectionAndSpinCount},
    {"KERNEL32.dll::EnterCriticalSection", 1, sh_EnterCriticalSection},
    {"KERNEL32.dll::LeaveCriticalSection", 1, sh_LeaveCriticalSection},
    {"KERNEL32.dll::DeleteCriticalSection", 1, sh_DeleteCriticalSection},
    {"KERNEL32.dll::TlsAlloc", 0, sh_TlsAlloc},
    {"KERNEL32.dll::TlsGetValue", 1, sh_TlsGetValue},
    {"KERNEL32.dll::TlsSetValue", 2, sh_TlsSetValue},
    {"KERNEL32.dll::TlsFree", 1, sh_TlsFree},
    {"KERNEL32.dll::FlsAlloc", 1, sh_FlsAlloc},
    {"KERNEL32.dll::FlsGetValue", 1, sh_FlsGetValue},
    {"KERNEL32.dll::FlsSetValue", 2, sh_FlsSetValue},
    {"KERNEL32.dll::FlsFree", 1, sh_FlsFree},
    {"KERNEL32.dll::SetUnhandledExceptionFilter", 1, sh_SetUnhandledExceptionFilter},
    {"KERNEL32.dll::GetCurrentThreadId", 0, sh_GetCurrentThreadId},
    {"KERNEL32.dll::GetCurrentProcessId", 0, sh_GetCurrentProcessId},
    {"KERNEL32.dll::GetTickCount", 0, sh_GetTickCount},
    {"KERNEL32.dll::GetLastError", 0, sh_GetLastError},
    {"KERNEL32.dll::SetLastError", 1, sh_SetLastError},
    {"KERNEL32.dll::VirtualAlloc", 4, sh_VirtualAlloc},
    {"KERNEL32.dll::VirtualFree", 3, sh_VirtualFree},
    {"KERNEL32.dll::ExitProcess", 1, sh_ExitProcess},
    {"WINMM.dll::timeGetTime", 0, sh_timeGetTime},
    {"COMCTL32.dll::#17", 0, sh_InitCommonControls},
};

void rt_set_image(rt_pe_image *img)
{
    rt_image = img;
}

/* Talons dynamiques : fonctions obtenues par GetProcAddress, numérotées après les imports statiques. */
#define RT_DYN_MAX 4096
static char *dyn_names[RT_DYN_MAX];
static unsigned dyn_count;

/* Modules : handles pseudo-fixes par nom de DLL (GetModuleHandleA). */
#define RT_MOD_BASE 0x60000000u
static char *mod_names[64];
static unsigned mod_count;

static const char *import_name(uint32_t addr)
{
    uint32_t idx = (addr - RT_STUB_BASE) / 16;
    uint32_t n = rt_image ? rt_image->import_count : 0;
    if (idx < n) {
        return rt_image->import_names[idx];
    }
    if (idx - n < dyn_count) {
        return dyn_names[idx - n];
    }
    return NULL;
}

static uint32_t module_handle(const char *dll)
{
    for (unsigned i = 0; i < mod_count; i++) {
        if (strcmp(mod_names[i], dll) == 0) return RT_MOD_BASE + 16 * i;
    }
    if (mod_count >= 64) rt_fatal("trop de modules");
    mod_names[mod_count] = strdup(dll);
    return RT_MOD_BASE + 16 * mod_count++;
}

static const char *module_name(uint32_t h)
{
    if (h < RT_MOD_BASE || (h - RT_MOD_BASE) / 16 >= mod_count) return NULL;
    return mod_names[(h - RT_MOD_BASE) / 16];
}

uint32_t rt_dyn_stub_addr(const char *full);

static uint32_t dyn_stub(const char *full)
{
    for (unsigned i = 0; i < dyn_count; i++) {
        if (strcmp(dyn_names[i], full) == 0) return RT_STUB_BASE + 16 * (rt_image->import_count + i);
    }
    if (dyn_count >= RT_DYN_MAX) rt_fatal("trop de fonctions dynamiques");
    dyn_names[dyn_count] = strdup(full);
    return RT_STUB_BASE + 16 * (rt_image->import_count + dyn_count++);
}

static uint32_t sh_GetModuleHandleA_real(cpu_t *c, const uint32_t *a)
{
    if (a[0] == 0) return RT_IMAGE_BASE;
    char name[256];
    unsigned i = 0;
    for (; i < sizeof name - 1; i++) {
        char ch = (char)rt_mem_base[a[0] + i];
        if (ch == 0) break;
        name[i] = (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
    }
    name[i] = 0;
    (void)c;
    return module_handle(name);
}

/* Bibliothèques fournies par le runtime (talons) : LoadLibraryA renvoie leur handle de module. */
static int provided_dll(const char *name)
{
    static const char *known[] = {"d3d9.dll", "dinput8.dll", "dsound.dll", "winmm.dll", "user32.dll",
                                  "gdi32.dll", "kernel32.dll", "advapi32.dll", "comctl32.dll", "ole32.dll"};
    const char *base = strrchr(name, '\\');
    base = base ? base + 1 : name;
    for (unsigned i = 0; i < sizeof known / sizeof known[0]; i++) {
        if (strcasecmp(base, known[i]) == 0) return 1;
    }
    return 0;
}

static uint32_t sh_LoadLibraryA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    char name[256];
    unsigned i = 0;
    for (; i < sizeof name - 1 && rt_mem_base[a[0] + i]; i++) name[i] = (char)rt_mem_base[a[0] + i];
    name[i] = 0;
    if (!provided_dll(name)) return 0;  /* ERROR_MOD_NOT_FOUND : bibliothèque absente */
    char lower[256];
    for (unsigned k = 0; k <= i; k++) lower[k] = (char)tolower((unsigned char)name[k]);
    return module_handle(lower);
}

static uint32_t sh_GetProcAddress(cpu_t *c, const uint32_t *a)
{
    (void)c;
    const char *dll = module_name(a[0]);
    if (!dll) return 0;
    char full[320];
    snprintf(full, sizeof full, "%s::%s", dll, (const char *)(rt_mem_base + a[1]));
    /* Export inconnu du runtime : NULL, comme Windows pour un nom absent. */
    if (!find_entry(full)) {
        if (rt_trace_imports) fprintf(stderr, "[getprocaddress] absent : %s\n", full);
        return 0;
    }
    return dyn_stub(full);
}

/* Compare « DLL::fonction » : DLL sans tenir compte de la casse, fonction exacte. */
static int same_import(const char *a, const char *b)
{
    const char *sa = strstr(a, "::"), *sb = strstr(b, "::");
    if (!sa || !sb) return strcmp(a, b) == 0;
    /* Suffixe « .dll » optionnel dans le nom de module. */
    size_t la = (size_t)(sa - a), lb = (size_t)(sb - b);
    if (la > 4 && strncasecmp(sa - 4, ".dll", 4) == 0) la -= 4;
    if (lb > 4 && strncasecmp(sb - 4, ".dll", 4) == 0) lb -= 4;
    if (la != lb || strncasecmp(a, b, la) != 0) return 0;
    return strcmp(sa, sb) == 0;
}

static const rt_shim_entry *find_com_entry(const char *name)
{
    for (unsigned i = 0; i < rt_com_iface_count; i++) {
        const rt_com_iface *ifc = &rt_com_ifaces[i];
        for (unsigned k = 0; k < ifc->count; k++) {
            if (strcmp(ifc->methods[k].name, name) == 0) return &ifc->methods[k];
        }
    }
    return NULL;
}

static const rt_shim_entry *find_entry(const char *name)
{
    for (unsigned i = 0; i < sizeof rt_shims / sizeof rt_shims[0]; i++) {
        if (same_import(rt_shims[i].name, name)) return &rt_shims[i];
    }
    for (unsigned i = 0; i < rt_user32_count; i++) {
        if (same_import(rt_user32_table[i].name, name)) return &rt_user32_table[i];
    }
    for (unsigned i = 0; i < rt_extra_count; i++) {
        if (same_import(rt_extra_table[i].name, name)) return &rt_extra_table[i];
    }
    for (unsigned i = 0; i < rt_fs_count; i++) {
        if (same_import(rt_fs_table[i].name, name)) return &rt_fs_table[i];
    }
    return find_com_entry(name);
}

/* Cache par adresse de talon : évite la recherche par nom à chaque appel (COM, Direct3D). */
#define RT_STUB_SLOTS ((RT_STUB_END - RT_STUB_BASE) / 16)
static const rt_shim_entry *shim_cache[RT_STUB_SLOTS];

void rt_shim_call(cpu_t *c, uint32_t addr)
{
    uint32_t slot = (addr - RT_STUB_BASE) / 16;
    rt_lock_enter();
    const rt_shim_entry *e = slot < RT_STUB_SLOTS ? shim_cache[slot] : NULL;
    if (!e) {
        const char *name = import_name(addr);
        if (!name) {
            fprintf(stderr, "rt: talon inconnu 0x%08x\n", addr);
            exit(3);
        }
        e = find_entry(name);
        if (!e) {
            fprintf(stderr, "rt: import non implémenté : %s (appel à 0x%08x)\n", name, addr);
            exit(3);
        }
        if (slot < RT_STUB_SLOTS) shim_cache[slot] = e;
    }
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    if (e->nargs > RT_SHIM_MAX_ARGS) rt_fatal("talon avec trop d'arguments");
    uint32_t args[RT_SHIM_MAX_ARGS] = {0};
    for (unsigned k = 0; k < e->nargs; k++) {
        args[k] = (uint32_t)guest_ld(c, esp + 4 + 4 * k, 4);
    }
    if (rt_trace_imports) {
        fprintf(stderr, "[import t%u] (retour 0x%08x) %s", rt_current()->id, (uint32_t)guest_ld(c, esp, 4), e->name);
        for (unsigned k = 0; k < e->nargs; k++) {
            fprintf(stderr, "%s0x%x", k ? ", " : "(", args[k]);
        }
        fprintf(stderr, e->nargs ? ")\n" : "()\n");
    }
    uint32_t ret = e->fn(c, args);
    if (rt_trace_imports) {
        fprintf(stderr, "[retour] %s -> 0x%x\n", e->name, ret);
    }
    if (getenv("CT3_FRAMESTAT")) {
        static uint32_t draws, tex, texnull, clears, prims;
        if (strstr(e->name, "DrawPrimitive")) { draws++; prims += args[3]; }
        else if (strstr(e->name, "::Clear")) clears++;
        else if (strstr(e->name, "::SetTexture") && args[1] == 0) { if (args[2]) tex++; else texnull++; }
        else if (strstr(e->name, "::Present")) {
            fprintf(stderr, "[frame] draws=%u prims=%u clears=%u tex0=%u tex0null=%u\n", draws, prims, clears, tex, texnull);
            draws = prims = clears = tex = texnull = 0;
        }
    }
    cpu_wr(c, 0, 4, ret);
    /* stdcall : l'appelé dépile l'adresse de retour et les arguments. */
    cpu_wr(c, 16, 4, esp + 4 + 4 * e->nargs);
    rt_lock_leave();
}

uint32_t rt_call_guest(cpu_t *c, uint32_t fn, const uint32_t *args, unsigned n)
{
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    for (unsigned k = n; k-- > 0;) {
        esp -= 4;
        guest_st(c, esp, 4, args[k]);
    }
    esp -= 4;
    guest_st(c, esp, 4, RT_HOST_RET);
    cpu_wr(c, 16, 4, esp);
    dispatch(c, fn);
    return (uint32_t)cpu_rd(c, 0, 4);
}

uint32_t rt_dyn_stub_addr(const char *full)
{
    return dyn_stub(full);
}

void rt_unresolved(cpu_t *c, uint32_t addr)
{
    /* Retour vers l'hôte (threads, rappels) : dispatch rend la main à l'appelant C. */
    if (addr == RT_HOST_RET) return;
    /* Trampoline non traduit (ex. thunk isolé « jmp rel32 » que l'export n'a pas reconnu) : un saut direct ne
     * touche pas la pile, il équivaut à aller à sa cible. Suivi ici, avec signalement ; le reste reste fatal. */
    if (addr >= 0x400000u && addr < 0x800000u) {
        uint8_t op = (uint8_t)guest_ld(c, addr, 1);
        int32_t rel = 0;
        uint32_t target = 0;
        uint32_t ecx_set = 0;
        int set_ecx = 0;
        /* « mov ecx, imm32 ; jmp » : thunk MSVC (ajustement de « this » pour un destructeur, D-047). */
        if (op == 0xB9 && (uint8_t)guest_ld(c, addr + 5, 1) == 0xE9) {
            ecx_set = (uint32_t)guest_ld(c, addr + 1, 4);
            set_ecx = 1;
            rel = (int32_t)guest_ld(c, addr + 6, 4);
            target = addr + 10 + (uint32_t)rel;
        } else if (op == 0xE9) {
            rel = (int32_t)guest_ld(c, addr + 1, 4);
            target = addr + 5 + (uint32_t)rel;
        } else if (op == 0xEB) {
            rel = (int8_t)guest_ld(c, addr + 1, 1);
            target = addr + 2 + (uint32_t)rel;
        }
        if (target) {
            static int warned = 0;
            if (!warned++) fprintf(stderr, "rt: trampoline non traduit 0x%08x -> 0x%08x (suivi)\n", addr, target);
            if (set_ecx) cpu_wr(c, 4, 4, ecx_set);
            dispatch(c, target);
            return;
        }
    }
    /* Le sommet de pile invité contient l'adresse de retour du dernier appel traduit. */
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    uint32_t ret = (uint32_t)guest_ld(c, esp, 4);
    fprintf(stderr, "rt: appel vers une adresse non traduite 0x%08x (retour invité 0x%08x)\n", addr, ret);
    rt_probe_dump();
    if (getenv("CT3_UNRESDBG")) {
        uint32_t obj = (uint32_t)guest_ld(c, 0x724e10u, 4);
        uint32_t vt = obj ? (uint32_t)guest_ld(c, obj, 4) : 0;
        fprintf(stderr, "rt: [724e10]=0x%08x vtable=0x%08x [vt+0x18]=0x%08x [vt+0x0]=0x%08x\n", obj, vt,
                vt ? (uint32_t)guest_ld(c, vt + 0x18, 4) : 0, vt ? (uint32_t)guest_ld(c, vt, 4) : 0);
    }
    exit(3);
}

void rt_unsupported(const char *what, uint32_t addr)
{
    fprintf(stderr, "rt: opération p-code non prise en charge (%s) à 0x%08x\n", what, addr);
    exit(3);
}

void rt_int3(uint32_t addr)
{
    fprintf(stderr, "rt: point d'arrêt INT3 atteint à 0x%08x\n", addr);
    exit(3);
}
