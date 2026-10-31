#include "rt_proc.h"

#include <stdio.h>
#include <string.h>

int rt_mem_protect(uint32_t addr, uint32_t size, int prot);
int rt_mem_init(void);

/* Tas : allocateur à croissance monotone. Chaque bloc a un en-tête de 8 octets (taille). */
static uint32_t heap_next = RT_HEAP_BASE;

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

uint32_t rt_heap_alloc(uint32_t size, int zero)
{
    uint32_t total = (size + 8 + 15) & ~15u;
    if (heap_next + total > RT_HEAP_BASE + RT_HEAP_SIZE) {
        rt_fatal("tas invité épuisé");
    }
    uint32_t block = heap_next;
    heap_next += total;
    put32(block, size);
    if (zero) {
        memset(rt_mem_base + block + 8, 0, size);
    }
    return block + 8;
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
    return 0;
}
