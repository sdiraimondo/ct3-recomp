#include "rt.h"

/* Profil processeur annoncé par CPUID : AMD, famille 6 (Athlon XP), avec MMX, SSE, SSE2 et 3DNow!.
 * Les extensions annoncées doivent correspondre à celles que le runtime implémente. */

#define RT_CPUID_SCRATCH (RT_SCRATCH_BASE + 0x00080000u)

#define CPUID1_EDX_FPU (1u << 0)
#define CPUID1_EDX_TSC (1u << 4)
#define CPUID1_EDX_CX8 (1u << 8)
#define CPUID1_EDX_CMOV (1u << 15)
#define CPUID1_EDX_MMX (1u << 23)
#define CPUID1_EDX_FXSR (1u << 24)
#define CPUID1_EDX_SSE (1u << 25)
#define CPUID1_EDX_SSE2 (1u << 26)

#define CPUIDX_EDX_MMXEXT (1u << 22)
#define CPUIDX_EDX_3DNOWEXT (1u << 30)
#define CPUIDX_EDX_3DNOW (1u << 31)

extern uint8_t *rt_mem_base;

uint32_t rt_cpuid_block(cpu_t *c)
{
    uint32_t leaf = (uint32_t)cpu_rd(c, 0, 4);
    uint32_t r[4] = {0, 0, 0, 0}; /* eax, ebx, ecx, edx */

    switch (leaf) {
    case 0x00000000u:
        r[0] = 1;
        r[1] = 0x68747541u; /* "Auth" */
        r[2] = 0x444D4163u; /* "cAMD" */
        r[3] = 0x69746E65u; /* "enti" */
        break;
    case 0x00000001u:
        r[0] = 0x681u;
        r[3] = CPUID1_EDX_FPU | CPUID1_EDX_TSC | CPUID1_EDX_CX8 | CPUID1_EDX_CMOV |
               CPUID1_EDX_MMX | CPUID1_EDX_FXSR | CPUID1_EDX_SSE | CPUID1_EDX_SSE2;
        break;
    case 0x80000000u:
        r[0] = 0x80000001u;
        break;
    case 0x80000001u:
        r[3] = CPUID1_EDX_FPU | CPUID1_EDX_CX8 | CPUID1_EDX_CMOV | CPUID1_EDX_MMX |
               CPUIDX_EDX_MMXEXT | CPUIDX_EDX_3DNOWEXT | CPUIDX_EDX_3DNOW;
        break;
    default:
        break;
    }

    memcpy(rt_mem_base + RT_CPUID_SCRATCH, r, sizeof r);
    return RT_CPUID_SCRATCH;
}
