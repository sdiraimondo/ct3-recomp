#include "rt_proc.h"
#include "rt_thread.h"

#include <stdio.h>
#include <string.h>

int rt_mem_protect(uint32_t addr, uint32_t size, int prot);
int rt_mem_init(void);

/* Tas : allocateur à croissance monotone, avec réutilisation des blocs libérés (listes par taille).
 * Chaque bloc a un en-tête de 8 octets (taille demandée). */
static uint32_t heap_next = RT_HEAP_BASE;

#define RT_HEAP_CLASSES 512
#define RT_HEAP_FREE_MAX 4096

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
    if (zero) {
        memset(rt_mem_base + block + 8, 0, size);
    }
    return block + 8;
}

void rt_heap_free(uint32_t p)
{
    if (!p) return;
    uint32_t block = p - 8;
    if (block < RT_HEAP_BASE || block >= heap_next) return;
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
    return 0;
}
