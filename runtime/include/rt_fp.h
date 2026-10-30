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

static inline int rt_fcmp_lt(long double a, long double b) { return a < b; }
static inline int rt_fcmp_le(long double a, long double b) { return a <= b; }
static inline int rt_fcmp_eq(long double a, long double b) { return a == b; }
static inline int rt_fcmp_ne(long double a, long double b) { return a != b; }
static inline int rt_fcmp_nan(long double a) { return isnan(a) != 0; }
