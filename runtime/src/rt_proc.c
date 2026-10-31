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
     * pas un appel ordinaire) ; le prologue SEH 0x51B390 modifie EBP légitimement. */
    static int disabled = -1;
    if (disabled < 0) disabled = getenv("CT3_NOREGCHK") != NULL;
    if (disabled || target == 0x0051B3CBu) return;
    static const char *names[4] = {"EBX", "EBP", "ESI", "EDI"};
    static const unsigned offs[4] = {12, 20, 24, 28};
    for (unsigned k = 0; k < 4; k++) {
        if (k == 1 && target == 0x0051B390u) continue;
        uint32_t now = (uint32_t)cpu_rd(c, offs[k], 4);
        if (now != s[k]) {
            fprintf(stderr, "rt: registre %s modifié par l'appel à 0x%08x depuis 0x%08x : 0x%08x -> 0x%08x\n",
                    names[k], target, site, s[k], now);
            exit(6);
        }
    }
}

/* Tas : allocateur à croissance monotone, avec réutilisation des blocs libérés (listes par taille).
 * Chaque bloc a un en-tête de 8 octets (taille demandée). */
static uint32_t heap_next = RT_HEAP_BASE;

#define RT_HEAP_CLASSES 512
#define RT_HEAP_FREE_MAX 4096

#define RT_BLOCK_USED 0x5A110CEDu
#define RT_BLOCK_FREE 0xF4EEF4EEu

typedef struct rt_free_block {
    uint32_t block;   /* adresse invitée de l'en-tête */
    uint32_t total;   /* taille totale du bloc */
} rt_free_block;

static rt_free_block free_blocks[RT_HEAP_FREE_MAX];
static unsigned free_count;

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

uint32_t rt_heap_alloc(uint32_t size, int zero)
{
    uint32_t total = block_total(size);
    uint32_t block = 0;
    for (unsigned i = 0; i < free_count; i++) {
        if (free_blocks[i].total == total) {
            block = free_blocks[i].block;
            free_blocks[i] = free_blocks[--free_count];
            break;
        }
    }
    if (!block) {
        if (heap_next + total > RT_HEAP_BASE + RT_HEAP_SIZE) {
            rt_fatal("tas invité épuisé");
        }
        block = heap_next;
        heap_next += total;
    }
    put32(block, size);
    put32(block + 4, RT_BLOCK_USED);
    if (zero) {
        memset(rt_mem_base + block + 8, 0, size);
    }
    return block + 8;
}

void rt_heap_free(uint32_t p)
{
    if (!p) return;
    uint32_t block = p - 8;
    /* Seuls les blocs alloués sont libérables : double libération et pointeurs intérieurs sont ignorés. */
    if (block < RT_HEAP_BASE || block >= heap_next || get32(block + 4) != RT_BLOCK_USED) return;
    put32(block + 4, RT_BLOCK_FREE);
    if (free_count < RT_HEAP_FREE_MAX) {
        free_blocks[free_count].block = block;
        free_blocks[free_count].total = block_total(get32(block));
        free_count++;
    }
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

void rt_watch_entry(cpu_t *c, uint32_t addr)
{
    static uint32_t watch = 0;
    static int init = 0, shown = 0;
    if (!init) {
        const char *w = getenv("CT3_WATCH");
        watch = w ? (uint32_t)strtoul(w, NULL, 16) : 0;
        init = 1;
    }
    if (!watch || addr != watch || shown >= 5000) return;
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
    fprintf(stderr, "[watch 0x%08x] ecx=0x%08x arg=0x%08x n8c=%u tbl=0x%08x e0=0x%08x w18=%u etat=%u\n",
            addr, ecx, arg, n8c, n88, t0, w18, watch_rd(0x7bc334u, 4));
}
