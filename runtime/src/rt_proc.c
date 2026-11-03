#include <stdio.h>
#include <stdlib.h>
#include "rt_proc.h"
#include "rt_thread.h"
void rt_diag_init(void);

#include <stdio.h>
#include <string.h>

int rt_mem_protect(uint32_t addr, uint32_t size, int prot);
int rt_mem_init(void);

/* Dernière instruction invitée exécutée (CT3_TRACE_INSN=1 à la traduction) : affichée au plantage. */
uint32_t rt_cur_insn;

__thread uint32_t rt_cs_stack[RT_CS_MAX];
__thread unsigned rt_cs_depth;

void rt_regs_save(cpu_t *c, uint32_t s[4])
{
    s[0] = (uint32_t)cpu_rd(c, 12, 4);  /* EBX */
    s[1] = (uint32_t)cpu_rd(c, 20, 4);  /* EBP */
    s[2] = (uint32_t)cpu_rd(c, 24, 4);  /* ESI */
    s[3] = (uint32_t)cpu_rd(c, 28, 4);  /* EDI */
}

void rt_regs_check(cpu_t *c, const uint32_t s[4], uint32_t site, uint32_t target)
{
    /* Exceptions du CRT MSVC : l'épilogue 0x51B3CB restaure les registres du cadre (retour de fonction,
     * pas un appel ordinaire) ; les prologues SEH 0x51B390 et 0x519D80 modifient EBP légitimement ; le helper 64 bits 0x518E20 utilise EBX. */
    static int disabled = -1;
    if (disabled < 0) disabled = getenv("CT3_NOREGCHK") != NULL;
    if (disabled || target == 0x0051B3CBu) return;
    static const char *names[4] = {"EBX", "EBP", "ESI", "EDI"};
    static const unsigned offs[4] = {12, 20, 24, 28};
    for (unsigned k = 0; k < 4; k++) {
        if (k == 1 && (target == 0x0051B390u || target == 0x00519D80u)) continue;
        if (k == 0 && target == 0x00518E20u) continue;  /* _aulldiv : EBX scratch, l'appelant le réaffecte */
        uint32_t now = (uint32_t)cpu_rd(c, offs[k], 4);
        if (now != s[k]) {
            fprintf(stderr, "rt: registre %s modifié par l'appel à 0x%08x depuis 0x%08x : 0x%08x -> 0x%08x\n",
                    names[k], target, site, s[k], now);
            exit(6);
        }
    }
}

/* Tas : allocateur à classes de taille (puissances de deux). Chaque bloc a un en-tête de 8 octets : taille
 * demandée (ou taille restante pour un bloc libre) et marque d'état. La taille totale d'un bloc se déduit de
 * l'en-tête (block_total). Un bloc libre est mis dans la liste de sa classe ; son premier mot de charge utile
 * est le lien. Les blocs libres sont découpés pour les demandes plus petites et fusionnés avec le bloc
 * physiquement suivant ; le bloc de tête revient au pointeur de croissance. */
static uint32_t heap_next = RT_HEAP_BASE;

#define RT_BLOCK_USED 0x5A110CEDu
#define RT_BLOCK_FREE 0xF4EEF4EEu
#define RT_HEAP_BINS 32

static uint32_t heap_bins[RT_HEAP_BINS];

static void put32(uint32_t addr, uint32_t v)
{
    memcpy(rt_mem_base + addr, &v, 4);
}

static uint32_t get32(uint32_t addr)
{
    uint32_t v;
    memcpy(&v, rt_mem_base + addr, 4);
    return v;
}

static uint32_t block_total(uint32_t size)
{
    return (size + 8 + 15) & ~15u;
}

static unsigned bin_of(uint32_t total)
{
    return 31u - (unsigned)__builtin_clz(total);
}

static void bin_insert(uint32_t block, uint32_t total)
{
    unsigned b = bin_of(total);
    put32(block + 4, RT_BLOCK_FREE);
    put32(block + 8, heap_bins[b]);
    heap_bins[b] = block;
}

static void bin_remove(uint32_t block, uint32_t total)
{
    unsigned b = bin_of(total);
    uint32_t prev = 0;
    for (uint32_t cur = heap_bins[b]; cur; prev = cur, cur = get32(cur + 8)) {
        if (cur == block) {
            if (prev) put32(prev + 8, get32(block + 8));
            else heap_bins[b] = get32(block + 8);
            return;
        }
    }
}

/* CT3_HEAPCHK=1 : vérifie la cohérence du tas après chaque opération (diagnostic, lent). */
static void heap_check(const char *where)
{
    static int on = -1;
    if (on < 0) on = getenv("CT3_HEAPCHK") != NULL;
    if (!on) return;
    unsigned free_seen = 0, free_listed = 0;
    for (uint32_t b = RT_HEAP_BASE; b < heap_next;) {
        uint32_t st = get32(b + 4);
        uint32_t t = block_total(get32(b));
        if (t < 16 || b + t > heap_next || (st != RT_BLOCK_USED && st != RT_BLOCK_FREE)) {
            fprintf(stderr, "rt: TAS CORROMPU (%s) bloc 0x%08x taille=0x%x etat=0x%08x\n", where, b, get32(b), st);
            exit(7);
        }
        if (st == RT_BLOCK_FREE) free_seen++;
        b += t;
    }
    for (unsigned k = 0; k < RT_HEAP_BINS; k++) {
        for (uint32_t cur = heap_bins[k]; cur; cur = get32(cur + 8)) free_listed++;
    }
    if (free_seen != free_listed) {
        fprintf(stderr, "rt: TAS INCOHERENT (%s) libres vus=%u listés=%u\n", where, free_seen, free_listed);
        exit(7);
    }
}

/* CT3_HEAPTRACE=1 : une ligne par allocation ([h+]) et libération ([h-]) du tas, pour vérifier les chevauchements. */
static int heap_trace(void)
{
    static int on = -1;
    if (on < 0) on = getenv("CT3_HEAPTRACE") != NULL;
    return on;
}

/* État du tas côté hôte (sommet et listes libres), pour la vérification du portage natif (rt_filter.c). */
void rt_heap_get_state(uint32_t *top, uint32_t *bins)
{
    *top = heap_next;
    memcpy(bins, heap_bins, sizeof heap_bins);
}

void rt_heap_set_state(uint32_t top, const uint32_t *bins)
{
    heap_next = top;
    memcpy(heap_bins, bins, sizeof heap_bins);
}

uint32_t rt_heap_alloc(uint32_t size, int zero)
{
    uint32_t total = block_total(size);
    uint32_t block = 0;
    for (unsigned b = bin_of(total); b < RT_HEAP_BINS && !block; b++) {
        uint32_t prev = 0;
        for (uint32_t cur = heap_bins[b]; cur; prev = cur, cur = get32(cur + 8)) {
            uint32_t have = block_total(get32(cur));
            if (have >= total) {
                if (prev) put32(prev + 8, get32(cur + 8));
                else heap_bins[b] = get32(cur + 8);
                block = cur;
                break;
            }
        }
    }
    if (block) {
        uint32_t have = block_total(get32(block));
        if (have - total >= 32) {
            uint32_t rest = block + total;
            put32(rest, have - total - 8);
            bin_insert(rest, have - total);
        } else {
            total = have;
        }
    } else {
        if (heap_next + total > RT_HEAP_BASE + RT_HEAP_SIZE) {
            rt_fatal("tas invité épuisé");
        }
        block = heap_next;
        heap_next += total;
    }
    uint32_t cap = total - 8;
    put32(block, cap);
    put32(block + 4, RT_BLOCK_USED);
    if (zero) {
        memset(rt_mem_base + block + 8, 0, cap);
    }
    if (heap_trace()) fprintf(stderr, "[h+] 0x%08x 0x%x\n", block + 8, cap);
    heap_check("alloc");
    return block + 8;
}

void rt_heap_free(uint32_t p)
{
    if (!p) return;
    uint32_t block = p - 8;
    /* Seuls les blocs alloués sont libérables : double libération et pointeurs intérieurs sont ignorés. */
    if (block < RT_HEAP_BASE || block >= heap_next || get32(block + 4) != RT_BLOCK_USED) return;
    static int poison = -1;
    if (poison < 0) poison = getenv("CT3_HEAPPOISON") != NULL;
    if (heap_trace()) fprintf(stderr, "[h-] 0x%08x 0x%x\n", p, get32(block));
    if (poison) memset(rt_mem_base + p, 0xDB, get32(block));
    uint32_t total = block_total(get32(block));
    uint32_t next = block + total;
    while (next < heap_next && get32(next + 4) == RT_BLOCK_FREE) {
        uint32_t nt = block_total(get32(next));
        bin_remove(next, nt);
        total += nt;
        next = block + total;
    }
    if (next == heap_next) {
        heap_next = block;
        heap_check("libre-fin");
        return;
    }
    put32(block, total - 8);
    bin_insert(block, total);
    heap_check("libre");
}

uint32_t rt_heap_size(uint32_t p)
{
    return get32(p - 8);
}

uint32_t rt_heap_realloc(uint32_t p, uint32_t size)
{
    uint32_t q = rt_heap_alloc(size, 1);
    if (p) {
        uint32_t old = rt_heap_size(p);
        memcpy(rt_mem_base + q, rt_mem_base + p, old < size ? old : size);
        rt_heap_free(p);
    }
    return q;
}

/* Zone de travail (chaînes renvoyées par les shims) : 0x10100000-0x10300000. La pile est 0x10300000-0x103FFF00. */
static uint32_t scratch_next = RT_SCRATCH_BASE + 0x00100000u;

uint32_t rt_scratch_alloc(uint32_t size)
{
    uint32_t p = scratch_next;
    scratch_next += (size + 15) & ~15u;
    if (scratch_next > RT_SCRATCH_BASE + 0x00300000u) {
        rt_fatal("zone de travail invitée épuisée");
    }
    return p;
}

int rt_process_init(cpu_t *c)
{
    if (rt_mem_protect(RT_HEAP_BASE, RT_HEAP_SIZE, 3) != 0) {
        return -1;
    }
    /* TEB : ExceptionList = fin de chaîne, StackBase/StackLimit, Self, PEB, TLS. */
    memset(rt_mem_base + RT_TEB_BASE, 0, 0x1000);
    put32(RT_TEB_BASE + 0x00, 0xFFFFFFFFu);
    put32(RT_TEB_BASE + 0x04, RT_STACK_TOP);
    put32(RT_TEB_BASE + 0x08, RT_STACK_LIMIT);
    put32(RT_TEB_BASE + 0x18, RT_TEB_BASE);
    put32(RT_TEB_BASE + 0x30, RT_PEB_BASE);
    put32(RT_TEB_BASE + 0x2C, RT_TLS_BASE);
    /* PEB : ImageBaseAddress à 0x08. */
    memset(rt_mem_base + RT_PEB_BASE, 0, 0x1000);
    put32(RT_PEB_BASE + 0x08, RT_IMAGE_BASE);
    memset(rt_mem_base + RT_TLS_BASE, 0, 0x1000);
    cpu_wr(c, RT_FS_OFFSET_REG, 4, RT_TEB_BASE);
    rt_thread_main_init(c);
    rt_install_crash_handler();
    rt_diag_init();
    return 0;
}

/* CT3_WATCH=<adresse> : à chaque entrée de cette fonction (5000 premières fois), affiche ECX, le premier
 * argument et la table pointée par [ECX+0x88] (premier élément et son mot en +0x18). Les lectures
 * invitées sont bornées : une adresse hors zone lisible donne 0 au lieu de faire planter le diagnostic. */
static uint32_t watch_rd(uint32_t g, unsigned n)
{
    uint32_t v = 0;
    if (g >= 0x10000u && g < 0x7F000000u - 8u && g + n <= 0x7F000000u) memcpy(&v, rt_mem_base + g, n);
    return v;
}

static const uint32_t call_list[] = {0x41e6a0u, 0x4342d0u, 0x435570u, 0x41abb0u, 0x421220u, 0x423220u, 0x44d9a0u, 0x462fa0u};
static uint32_t call_cnt[sizeof call_list / sizeof call_list[0]];

void rt_watch_entry(cpu_t *c, uint32_t addr)
{
    static uint32_t watch = 0;
    static int init = 0, shown = 0;
    /* CT3_CALLS=1 : compte les entrées des fonctions du rendu de scène entre deux lignes de surveillance. */
    for (unsigned k = 0; k < sizeof call_list / sizeof call_list[0]; k++)
        if (addr == call_list[k]) call_cnt[k]++;
    if (addr == 0x4342d0u && getenv("CT3_CALLS")) {
        /* Diagnostic : compteur de progression de l'objet de scène (table 0x53F198, pas 0x94, champ +0x74). */
        static uint32_t n = 0;
        if (n++ % 60 == 0) {
            uint32_t obj = (uint32_t)cpu_rd(c, 0, 4);
            uint32_t idx = watch_rd(obj + 0x80, 4);
            uint32_t rec = 0x53f198u + idx * 0x94u;
            uint32_t bits = watch_rd(rec + 0x74, 4);
            float f; memcpy(&f, &bits, 4);
            uint32_t fl = watch_rd(obj + 0x8c, 4);
            fprintf(stderr, "[progres] obj=%08x idx=%u t=%g flags=%08x\n", obj, idx, f, fl);
        }
    }
    if (!init) {
        const char *w = getenv("CT3_WATCH");
        watch = w ? (uint32_t)strtoul(w, NULL, 16) : 0;
        init = 1;
    }
    if (!watch || addr != watch || shown >= 50000) return;
    shown++;
    uint32_t ecx = (uint32_t)cpu_rd(c, 4, 4);
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    uint32_t arg = watch_rd(esp + 4, 4);
    uint32_t n88 = watch_rd(ecx + 0x88, 4), n8c = watch_rd(ecx + 0x8c, 4);
    uint32_t t0 = 0, e0 = 0, w18 = 0;
    if (n88 && n8c) {
        t0 = watch_rd(n88, 4);
        if (t0) {
            e0 = watch_rd(t0, 4);
            w18 = watch_rd(e0 + 0x18, 2);
        }
    }
    struct timespec wts;
    clock_gettime(CLOCK_MONOTONIC, &wts);
    fprintf(stderr, "[watch 0x%08x] ms=%ld etat=%u sous=%u trame=%u fondu=%u chargeur=%u flag=%u file=%u travail=%u scene=%u\n", addr,
            (long)(wts.tv_sec * 1000 + wts.tv_nsec / 1000000),
            watch_rd(0x7bc334u, 4), watch_rd(0x7bc354u, 4), watch_rd(0x717884u, 4), watch_rd(0x7178fcu, 4),
            watch_rd(0x71ef8cu, 4), watch_rd(0x717889u, 1), watch_rd(0x71650cu, 4), watch_rd(0x716504u, 1),
            watch_rd(0x71e96cu, 4));
    if (getenv("CT3_CALLS")) {
        /* Réalisateur de cinématique : index courant (+0x18), nombre d'entrées (+0x1c). */
        uint32_t dir = watch_rd(0x7ca0acu, 4);
        fprintf(stderr, "[realis] idx=%u nb=%u\n", dir ? watch_rd(dir + 0x18, 4) : 0, dir ? watch_rd(dir + 0x1c, 4) : 0);
        /* Position de référence de l'événement (0x71EBB4 : x, y, z en flottants). */
        float px, py, pz; uint32_t bx = watch_rd(0x71ebb4u, 4), by = watch_rd(0x71ebb8u, 4), bz = watch_rd(0x71ebbcu, 4);
        memcpy(&px, &bx, 4); memcpy(&py, &by, 4); memcpy(&pz, &bz, 4);
        fprintf(stderr, "[ref] x=%g y=%g z=%g\n", px, py, pz);
        /* Lecture du replay : drapeau actif, compteur de temps, pointeur d'enregistrement, indice, base. */
        fprintf(stderr, "[rep] act=%u t=%u rec=%08x slot=%u base=%08x\n", watch_rd(0x7bd080u, 4), watch_rd(0x7bd078u, 4),
                watch_rd(0x7bd068u, 4), watch_rd(0x7bc340u, 4), watch_rd(0x7bc368u, 4));
        {
            uint32_t sn = watch_rd(0x7164c4u, 4);
            uint32_t svt = sn ? watch_rd(sn, 4) : 0;
            fprintf(stderr, "[son] obj=%08x vt=%08x vt+20=%08x vt+24=%08x\n", sn, svt, svt ? watch_rd(svt + 0x20, 4) : 0, svt ? watch_rd(svt + 0x24, 4) : 0);
        }
        {
            uint32_t o = watch_rd(0x7c9f18u, 4), q = watch_rd(0x7ca098u, 4);
            fprintf(stderr, "[obj] o=%08x o0=%08x o8=%u q=%08x q0=%08x q8=%u\n", o, o ? watch_rd(o, 4) : 0,
                    o ? watch_rd(o + 8, 4) : 0, q, q ? watch_rd(q, 4) : 0, q ? watch_rd(q + 8, 4) : 0);
        }
        fprintf(stderr, "[appels]");
        for (unsigned k = 0; k < sizeof call_list / sizeof call_list[0]; k++) {
            fprintf(stderr, " %x=%u", call_list[k], call_cnt[k]);
            call_cnt[k] = 0;
        }
        fprintf(stderr, "\n");
    }
}
