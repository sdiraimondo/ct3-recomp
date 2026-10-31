#pragma once

#include <stdint.h>
#include <string.h>

/* Espace d'adresses invité : 4 GiB réservés, image chargée à 0x400000. */
#define RT_GUEST_SIZE   (1ULL << 32)
#define RT_IMAGE_BASE   0x00400000u
#define RT_SCRATCH_BASE 0x10000000u
#define RT_SCRATCH_SIZE 0x00400000u
/* Plan de la zone 0x10000000-0x10400000 : tests (< 0x10080000), CPUID 0x10080000, TEB/PEB/TLS 0x10090000,
 * zone de travail 0x10100000-0x10300000, pile 0x10300000-0x103FFF00. */
#define RT_STACK_TOP    0x103FFF00u
#define RT_STUB_BASE    0x7F000000u
#define RT_STUB_END     0x7F100000u
#define RT_HOST_RET     0x7EFFFFF0u
#define RT_REG_BYTES    8192

typedef struct cpu {
    uint8_t reg[RT_REG_BYTES];
} cpu_t;

extern uint8_t *rt_mem_base;

void rt_fatal(const char *msg);

static inline uint64_t cpu_rd(cpu_t *c, unsigned off, unsigned size)
{
    uint64_t v = 0;
    memcpy(&v, c->reg + off, size);
    return v;
}

static inline void cpu_wr(cpu_t *c, unsigned off, unsigned size, uint64_t v)
{
    memcpy(c->reg + off, &v, size);
}

static inline uint64_t guest_ld(cpu_t *c, uint32_t addr, unsigned size)
{
    uint64_t v = 0;
    (void)c;
    memcpy(&v, rt_mem_base + addr, size);
    return v;
}

static inline void guest_st(cpu_t *c, uint32_t addr, unsigned size, uint64_t v)
{
    (void)c;
    memcpy(rt_mem_base + addr, &v, size);
}

static inline uint64_t rt_mask(unsigned size)
{
    return size >= 8 ? ~0ULL : ((1ULL << (8 * size)) - 1);
}

static inline uint64_t rt_div_u(uint64_t a, uint64_t b);
static inline uint64_t rt_rem_u(uint64_t a, uint64_t b);
static inline int64_t rt_div_s(int64_t a, int64_t b);
static inline int64_t rt_rem_s(int64_t a, int64_t b);
static inline uint64_t rt_carry_add(uint64_t a, uint64_t b, unsigned size);
static inline uint64_t rt_scarry_add(uint64_t a, uint64_t b, unsigned size);
static inline uint64_t rt_sborrow_sub(uint64_t a, uint64_t b, unsigned size);

static inline uint64_t rt_sign(uint64_t v, unsigned size)
{
    return (v >> (8 * size - 1)) & 1;
}

static inline uint64_t rt_carry_add(uint64_t a, uint64_t b, unsigned size)
{
    if (size >= 8) {
        return (a + b) < a;
    }
    return (a + b) > rt_mask(size);
}

static inline uint64_t rt_scarry_add(uint64_t a, uint64_t b, unsigned size)
{
    uint64_t r = (a + b) & rt_mask(size);
    return rt_sign(a, size) == rt_sign(b, size) && rt_sign(r, size) != rt_sign(a, size);
}

static inline uint64_t rt_sborrow_sub(uint64_t a, uint64_t b, unsigned size)
{
    uint64_t r = (a - b) & rt_mask(size);
    return rt_sign(a, size) != rt_sign(b, size) && rt_sign(r, size) != rt_sign(a, size);
}

static inline uint64_t rt_div_u(uint64_t a, uint64_t b)
{
    if (b == 0) {
        rt_fatal("division par zéro (non signée)");
    }
    return a / b;
}

static inline uint64_t rt_rem_u(uint64_t a, uint64_t b)
{
    if (b == 0) {
        rt_fatal("reste par zéro (non signé)");
    }
    return a % b;
}

static inline int64_t rt_div_s(int64_t a, int64_t b)
{
    if (b == 0) {
        rt_fatal("division par zéro (signée)");
    }
    return a / b;
}

static inline int64_t rt_rem_s(int64_t a, int64_t b)
{
    if (b == 0) {
        rt_fatal("reste par zéro (signé)");
    }
    return a % b;
}

static inline uint64_t rt_rdtsc(void)
{
    return __builtin_ia32_rdtsc();
}

void rt_int3(uint32_t addr);
uint32_t rt_cpuid_block(cpu_t *c);
void dispatch(cpu_t *c, uint32_t addr);
void rt_fatal(const char *msg);
void rt_unresolved(cpu_t *c, uint32_t addr);
void rt_unsupported(const char *what, uint32_t addr);
void rt_shim_call(cpu_t *c, uint32_t addr);
