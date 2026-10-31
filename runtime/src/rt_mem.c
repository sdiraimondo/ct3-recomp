#include "rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

uint8_t *rt_mem_base = NULL;

void rt_fatal(const char *msg)
{
    fprintf(stderr, "rt: %s\n", msg);
    exit(3);
}

int rt_mem_init(void)
{
    void *p = mmap(NULL, RT_GUEST_SIZE, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED) {
        perror("rt_mem_init: mmap");
        return -1;
    }
    rt_mem_base = p;
    return 0;
}

int rt_mem_protect(uint32_t addr, uint32_t size, int prot)
{
    int native = 0;
    if (prot & 1) native |= PROT_READ;
    if (prot & 2) native |= PROT_WRITE;
    if (prot & 4) native |= PROT_EXEC;
    uint32_t start = addr & ~0xFFFu;
    uint32_t end = (addr + size + 0xFFFu) & ~0xFFFu;
    if (mprotect(rt_mem_base + start, end - start, native) != 0) {
        perror("rt_mem_protect: mprotect");
        return -1;
    }
    return 0;
}

void rt_mem_fill(uint32_t addr, uint32_t size, uint8_t value)
{
    memset(rt_mem_base + addr, value, size);
}
