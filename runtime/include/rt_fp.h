#pragma once

/* Opérations flottantes x87 du runtime (valeurs en mémoire guest, 4, 8 ou 10 octets).
 * Précision : long double de gcc/clang sur x86_64 (80 bits, même format que x87).
 * Le contrôle de précision (PC) n'est pas modélisé : précision étendue, comme par défaut. */

#include "rt.h"

#include <math.h>

typedef struct {
    uint8_t b[16];
} rt_wide;

static inline long double rt_fget(const uint8_t *p, unsigned size)
{
    long double v = 0;
    if (size == 4) {
        float f;
        memcpy(&f, p, 4);
        v = f;
    } else if (size == 8) {
        double d;
        memcpy(&d, p, 8);
        v = d;
    } else {
        memcpy(&v, p, 10);
    }
    return v;
}

static inline void rt_fput(uint8_t *p, unsigned size, long double v)
{
    if (size == 4) {
        float f = (float)v;
        memcpy(p, &f, 4);
    } else if (size == 8) {
        double d = (double)v;
        memcpy(p, &d, 8);
    } else {
        memcpy(p, &v, 10);
    }
}

/* FRNDINT / FIST : arrondi selon le RC du mot de contrôle x87 (bits 10-11). */
static inline long double rt_fround(cpu_t *c, long double v)
{
    unsigned rc = (unsigned)(cpu_rd(c, 4256, 2) >> 10) & 3u;
    switch (rc) {
    case 0: return nearbyintl(v);
    case 1: return floorl(v);
    case 2: return ceill(v);
    default: return truncl(v);
    }
}

/* FIST / FISTP : conversion en entier signé de osz octets, valeur indéfinie x87 en cas de dépassement. */
static inline uint64_t rt_ftrunc(const uint8_t *p, unsigned isize, unsigned osize)
{
    long double v = rt_fget(p, isize);
    long double limit = (long double)(1ULL << (8 * osize - 1));
    if (isnan(v) || v >= limit || v < -limit) {
        return 1ULL << (8 * osize - 1);
    }
    return (uint64_t)(int64_t)truncl(v);
}

static inline uint64_t rt_load_bytes(const uint8_t *p, unsigned size)
{
    uint64_t v = 0;
    memcpy(&v, p, size);
    return v;
}

/* FPREM (troncature, ieee = 0) et FPREM1 (arrondi, ieee = 1) : reste exact (fmodl / remainderl), réduction
 * complète (C2 = 0) et bits de quotient C0, C1, C3. La p-code de Ghidra ne modélise pas ces bits : la boucle
 * « FPREM ; FSTSW ; JP » du jeu ne se terminait pas quand C2 restait à 1. */
static inline void rt_fprem(cpu_t *c, int ieee)
{
    long double a = rt_fget(c->reg + 4352, 10);
    long double b = rt_fget(c->reg + 4368, 10);
    long double r = ieee ? remainderl(a, b) : fmodl(a, b);
    long double q = ieee ? rintl(a / b) : truncl(a / b);
    unsigned sw = (unsigned)cpu_rd(c, 4258, 2) & ~0x4700u;
    if (isfinite(q) && fabsl(q) < 9.0e18L) {
        uint64_t qi = (uint64_t)fabsl(q);
        if (qi & 4) sw |= 0x0100u;
        if (qi & 2) sw |= 0x4000u;
        if (qi & 1) sw |= 0x0200u;
    }
    cpu_wr(c, 4258, 2, sw);
    rt_fput(c->reg + 4352, 10, r);
}

static inline int rt_fcmp_lt(long double a, long double b) { return a < b; }
static inline int rt_fcmp_le(long double a, long double b) { return a <= b; }
static inline int rt_fcmp_eq(long double a, long double b) { return a == b; }
static inline int rt_fcmp_ne(long double a, long double b) { return a != b; }
static inline int rt_fcmp_nan(long double a) { return isnan(a) != 0; }
