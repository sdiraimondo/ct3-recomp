/* Profil CPUID annoncé par le runtime (voir runtime/src/rt_cpu.c). */

#include "rt.h"

#include <stdio.h>
#include <string.h>

int rt_mem_init(void);
int rt_mem_protect(uint32_t addr, uint32_t size, int prot);

static int failures;

static uint32_t result(cpu_t *c, uint32_t leaf, unsigned word)
{
    cpu_wr(c, 0, 4, leaf);
    uint32_t block = rt_cpuid_block(c);
    uint32_t v;
    memcpy(&v, rt_mem_base + block + 4 * word, 4);
    return v;
}

#define CHECK_EQ(a, b, what)                                                         \
    do {                                                                             \
        if ((a) == (b)) {                                                            \
            printf("  ok     %s\n", what);                                          \
        } else {                                                                     \
            failures++;                                                              \
            printf("  ECHEC  %s : 0x%08x, attendu 0x%08x\n", what, (unsigned)(a), (unsigned)(b)); \
        }                                                                            \
    } while (0)

int main(void)
{
    static cpu_t c;
    if (rt_mem_init() != 0 || rt_mem_protect(RT_SCRATCH_BASE, RT_SCRATCH_SIZE, 3) != 0) {
        return 1;
    }
    memset(&c, 0, sizeof c);

    CHECK_EQ(result(&c, 0, 1), 0x68747541u, "CPUID 0 : EBX = \"Auth\"");
    CHECK_EQ(result(&c, 0, 2), 0x444D4163u, "CPUID 0 : ECX = \"cAMD\"");
    CHECK_EQ(result(&c, 0, 3), 0x69746E65u, "CPUID 0 : EDX = \"enti\"");
    CHECK_EQ(result(&c, 1, 3) & (1u << 23), 1u << 23, "CPUID 1 : MMX");
    CHECK_EQ(result(&c, 1, 3) & (1u << 25), 1u << 25, "CPUID 1 : SSE");
    CHECK_EQ(result(&c, 1, 3) & (1u << 26), 1u << 26, "CPUID 1 : SSE2");
    CHECK_EQ(result(&c, 0x80000001u, 3) & (1u << 31), 1u << 31, "CPUID 0x80000001 : 3DNow!");
    CHECK_EQ(result(&c, 0x80000001u, 3) & (1u << 30), 1u << 30, "CPUID 0x80000001 : 3DNow! ext");

    printf("\n%s (%d échec(s))\n", failures ? "CPU ECHEC" : "CPU OK", failures);
    return failures ? 2 : 0;
}
