#pragma once

/* Opérations flottantes x87 du runtime (valeurs en mémoire guest, 4, 8 ou 10 octets).
 * Précision : long double 80 bits sur x86_64 (même format que x87). Ailleurs (AArch64),
 * long double est du binary128 : les 10 octets x87 sont alors convertis explicitement,
 * sans perte (binary128 contient exactement le format étendu x87), par décalage de
 * mantisse — mêmes 15 bits d'exposant et même biais. Repli générique (ldexpl/frexpl)
 * pour les autres formats.
 * Le contrôle de précision (PC) n'est pas modélisé : précision étendue, comme par défaut. */

#include "rt.h"

#include <float.h>
#include <math.h>

typedef struct {
    uint8_t b[16];
} rt_wide;

/* Vrai quand long double est le format étendu x87 (x86 32/64 bits). */
#if LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384
#define RT_F80_NATIVE 1
#else
#define RT_F80_NATIVE 0
#endif

/* Vrai quand long double est du binary128 IEEE 754 little-endian (AArch64).
 * Les deux formats partagent la largeur et le biais de l'exposant (15 bits,
 * 16383) : la conversion est un décalage de mantisse, sans appel à la libm. */
#if !RT_F80_NATIVE && LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384 && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define RT_F80_BINARY128 1
#else
#define RT_F80_BINARY128 0
#endif

/* Format étendu x87 sur 10 octets : mantisse 64 bits (bit entier explicite),
 * exposant biaisé 15 bits (16383), signe. */
static inline long double rt_f80_get(const uint8_t *p)
{
    uint64_t sig;
    uint16_t se;
    unsigned sign, exp;

    memcpy(&sig, p, 8);
    memcpy(&se, p + 8, 2);
    sign = se >> 15;
    exp = se & 0x7FFFu;

#if RT_F80_BINARY128
    /* Correspondance directe : mêmes 15 bits d'exposant et même biais. La
     * mantisse x87 (bit entier explicite) occupe le haut des 113 bits de la
     * mantisse binary128 : décalage de 49, bit entier masqué. Vaut aussi pour
     * les dénormalisés (exposant 0 des deux côtés) et les infinis/NaN. */
    {
        uint64_t x = sig & 0x7FFFFFFFFFFFFFFFULL;
        uint64_t hi = ((uint64_t)sign << 63) | ((uint64_t)exp << 48) | (x >> 15);
        uint64_t lo = x << 49;
        long double v;
        memcpy(&v, &lo, 8);
        memcpy((uint8_t *)&v + 8, &hi, 8);
        return v;
    }
#else
    if (exp == 0x7FFFu) {
        long double r = sig == 0x8000000000000000ULL ? __builtin_infl() : __builtin_nanl("");
        return sign ? -r : r;
    }
    if (exp == 0) {
        if (sig == 0) return sign ? -0.0L : 0.0L;
        long double v = ldexpl((long double)sig, -16445); /* dénormalisé */
        return sign ? -v : v;
    }
    long double v = ldexpl((long double)sig, (int)exp - 16446);
    return sign ? -v : v;
#endif
}

/* Arrondi au plus proche, pair en cas d'égalité (round-to-nearest-even). */
static inline uint64_t rt_f80_round(long double v)
{
    uint64_t i = (uint64_t)v;
    long double f = v - (long double)i;
    if (f > 0.5L || (f == 0.5L && (i & 1))) i++;
    return i;
}

static inline void rt_f80_put(uint8_t *p, long double v)
{
    uint64_t sig = 0;
    uint16_t se = 0;

#if RT_F80_BINARY128
    {
        uint64_t lo, hi;
        unsigned sign, exp;

        memcpy(&lo, &v, 8);
        memcpy(&hi, (const uint8_t *)&v + 8, 8);
        sign = (unsigned)(hi >> 63);
        exp = (unsigned)((hi >> 48) & 0x7FFFu);

        if (exp == 0x7FFFu) {
            uint64_t frac = hi & 0xFFFFFFFFFFFFULL;
            sig = frac == 0 && lo == 0
                ? 0x8000000000000000ULL                              /* infini */
                : 0x8000000000000000ULL | (frac << 15) | (lo >> 49); /* NaN, charge conservée */
        } else {
            /* Mantisse 113 bits : bit implicite (exposant non nul) puis fraction.
             * La mantisse x87 est le haut de cette valeur, arrondi au plus proche pair. */
            uint64_t mhi = (hi & 0xFFFFFFFFFFFFULL) | (exp ? (1ULL << 48) : 0);
            uint64_t rem = lo & ((1ULL << 49) - 1);
            sig = (mhi << 15) | (lo >> 49);
            if (rem > (1ULL << 48) || (rem == (1ULL << 48) && (sig & 1))) sig++;
            if (sig == 0) {
                if (exp != 0) { sig = 0x8000000000000000ULL; exp++; } /* retenue */
            } else if (exp == 0 && sig >= 0x8000000000000000ULL) {
                exp = 1;                                              /* plus petit normal */
            }
        }
        se = (uint16_t)((sign << 15) | exp);
    }
#else
    {
        unsigned sign = signbit(v) ? 1u : 0u;
        long double a = fabsl(v);

        if (isnan(v)) {
            sig = 0xC000000000000000ULL; /* NaN silencieux */
            se = (uint16_t)((sign << 15) | 0x7FFFu);
        } else if (isinf(a)) {
            sig = 0x8000000000000000ULL;
            se = (uint16_t)((sign << 15) | 0x7FFFu);
        } else if (a != 0.0L) {
            int e;
            long double m = frexpl(a, &e); /* a = m * 2^e, m dans [0.5, 1) */
            long double scaled = ldexpl(m, 64);
            sig = rt_f80_round(scaled);
            if (sig == 0) { /* arrondi au-delà de 2^64 : retenue */
                sig = 0x8000000000000000ULL;
                e++;
            }
            int exp = e + 16382;
            if (exp >= 0x7FFF) {
                sig = 0x8000000000000000ULL;
                exp = 0x7FFF;
            } else if (exp <= 0) {
                /* Dénormalisé : valeur = mantisse * 2^-16445. */
                sig = rt_f80_round(ldexpl(a, 16445));
                exp = 0;
                if (sig >= 0x8000000000000000ULL) { /* arrondi vers le premier normal */
                    sig = 0x8000000000000000ULL;
                    exp = 1;
                }
            }
            se = (uint16_t)((sign << 15) | (exp & 0x7FFFu));
        } else {
            se = (uint16_t)(sign << 15);
        }
    }
#endif
    memcpy(p, &sig, 8);
    memcpy(p + 8, &se, 2);
}

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
    } else if (RT_F80_NATIVE) {
        memcpy(&v, p, 10);
    } else {
        v = rt_f80_get(p);
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
    } else if (RT_F80_NATIVE) {
        memcpy(p, &v, 10);
    } else {
        rt_f80_put(p, v);
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
    default:
        /* Troncature : le cast entier tronque déjà vers zéro ; au-delà de 2^63, la valeur est déjà entière. */
        return fabsl(v) < 9.2e18L ? (long double)(int64_t)v : v;
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
    return (uint64_t)(int64_t)v;
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
