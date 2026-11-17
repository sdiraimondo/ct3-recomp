/* Validation croisée de la conversion x87 <-> binary128 :
 *   - aller-retour exact sur des motifs x87 aléatoires valides ;
 *   - comparaison bit à bit avec l'implémentation générique (ldexpl/frexpl)
 *     qui avait validé test_x87 avant l'optimisation ;
 *   - idempotence sur des motifs binary128 aléatoires. */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "rt_fp.h"

static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;

static uint64_t rnd64(void)
{
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}

/* --- référence : implémentation générique validée --- */
static uint64_t ref_round(long double v)
{
    uint64_t i = (uint64_t)v;
    long double f = v - (long double)i;
    if (f > 0.5L || (f == 0.5L && (i & 1))) i++;
    return i;
}

static long double ref_get(const uint8_t *p)
{
    uint64_t sig;
    uint16_t se;
    unsigned sign, exp;
    memcpy(&sig, p, 8);
    memcpy(&se, p + 8, 2);
    sign = se >> 15;
    exp = se & 0x7FFFu;
    if (exp == 0x7FFFu) {
        long double r = sig == 0x8000000000000000ULL ? __builtin_infl() : __builtin_nanl("");
        return sign ? -r : r;
    }
    if (exp == 0) {
        if (sig == 0) return sign ? -0.0L : 0.0L;
        long double v = ldexpl((long double)sig, -16445);
        return sign ? -v : v;
    }
    long double v = ldexpl((long double)sig, (int)exp - 16446);
    return sign ? -v : v;
}

static void ref_put(uint8_t *p, long double v)
{
    uint64_t sig = 0;
    uint16_t se = 0;
    unsigned sign = signbit(v) ? 1u : 0u;
    long double a = fabsl(v);

    if (isnan(v)) {
        sig = 0xC000000000000000ULL;
        se = (uint16_t)((sign << 15) | 0x7FFFu);
    } else if (isinf(a)) {
        sig = 0x8000000000000000ULL;
        se = (uint16_t)((sign << 15) | 0x7FFFu);
    } else if (a != 0.0L) {
        int e;
        long double m = frexpl(a, &e);
        sig = ref_round(ldexpl(m, 64));
        if (sig == 0) { sig = 0x8000000000000000ULL; e++; }
        int exp = e + 16382;
        if (exp >= 0x7FFF) {
            sig = 0x8000000000000000ULL;
            exp = 0x7FFF;
        } else if (exp <= 0) {
            sig = ref_round(ldexpl(a, 16445));
            exp = 0;
            if (sig >= 0x8000000000000000ULL) { sig = 0x8000000000000000ULL; exp = 1; }
        }
        se = (uint16_t)((sign << 15) | (exp & 0x7FFFu));
    } else {
        se = (uint16_t)(sign << 15);
    }
    memcpy(p, &sig, 8);
    memcpy(p + 8, &se, 2);
}

static void make_x87(uint8_t *p)
{
    uint64_t sig = rnd64();
    uint16_t se = (uint16_t)rnd64();
    unsigned exp = se & 0x7FFFu;
    /* Motif valide : dénormalisé (sig sans bit entier), normal ou inf/NaN.
     * Les « unnormals » et pseudo-NaN (exposant non nul, bit entier clair)
     * sont exclus : le x87 réel ne les produit pas. */
    if (exp != 0) sig |= 0x8000000000000000ULL;
    if (exp == 0) sig &= 0x7FFFFFFFFFFFFFFFULL;
    memcpy(p, &sig, 8);
    memcpy(p + 8, &se, 2);
}

int main(void)
{
    int bad = 0;

    printf("sizeof(long double)=%zu LDBL_MANT_DIG=%d LDBL_MAX_EXP=%d binary128=%d\n",
           sizeof(long double), LDBL_MANT_DIG, LDBL_MAX_EXP, RT_F80_BINARY128);
    if (!RT_F80_BINARY128) {
        printf("ECHEC : chemin binary128 non compilé\n");
        return 1;
    }

    /* 1. Aller-retour exact sur motifs x87 valides. */
    for (int i = 0; i < 1000000; i++) {
        uint8_t a[10], b[10];
        make_x87(a);
        rt_f80_put(b, rt_f80_get(a));
        if (memcmp(a, b, 10) != 0) {
            if (bad++ < 5) {
                printf("ECHEC aller-retour : ");
                for (int k = 0; k < 10; k++) printf("%02x", a[k]);
                printf(" -> ");
                for (int k = 0; k < 10; k++) printf("%02x", b[k]);
                printf("\n");
            }
        }
    }

    /* 2. Comparaison de valeur avec la référence générique (hors NaN). */
    for (int i = 0; i < 200000; i++) {
        uint8_t a[10];
        make_x87(a);
        unsigned exp = ((unsigned)a[9] << 8 | a[8]) & 0x7FFFu;
        if (exp == 0x7FFFu) continue;
        long double x = rt_f80_get(a);
        long double y = ref_get(a);
        if (memcmp(&x, &y, sizeof x) != 0) {
            if (bad++ < 5) {
                printf("ECHEC valeur : ");
                for (int k = 0; k < 10; k++) printf("%02x", a[k]);
                printf(" : %La != %La\n", x, y);
            }
        }
    }

    /* 3. Motifs binary128 aléatoires : rt_f80_put == ref_put (hors NaN). */
    for (int i = 0; i < 200000; i++) {
        uint8_t src[16], a[10], b[10];
        uint64_t lo = rnd64(), hi = rnd64();
        memcpy(src, &lo, 8);
        memcpy(src + 8, &hi, 8);
        unsigned exp = (unsigned)((hi >> 48) & 0x7FFFu);
        if (exp == 0x7FFFu) continue;
        long double v;
        memcpy(&v, src, 16);
        rt_f80_put(a, v);
        ref_put(b, v);
        if (memcmp(a, b, 10) != 0) {
            if (bad++ < 5) {
                printf("ECHEC encodage : %La : ", v);
                for (int k = 0; k < 10; k++) printf("%02x", a[k]);
                printf(" != ");
                for (int k = 0; k < 10; k++) printf("%02x", b[k]);
                printf("\n");
            }
        }
    }

    /* 4. Cas ciblés. */
    {
        static const uint8_t cases[][10] = {
            {0,0,0,0,0,0,0,0x00,0x00,0x00},                      /* +0 */
            {0,0,0,0,0,0,0,0x00,0x00,0x80},                      /* -0 */
            {0,0,0,0,0,0,0,0x80,0xff,0x3f},                      /* 1 */
            {0,0,0,0,0,0,0,0x80,0xff,0xbf},                      /* -1 */
            {0,0,0,0,0,0,0,0x80,0xfe,0x3f},                      /* 0.5 */
            {0,0,0,0,0,0,0,0xc0,0xff,0x3f},                      /* 1.5 */
            {0,0,0,0,0,0,0,0x80,0x00,0x40},                      /* 2 */
            {1,0,0,0,0,0,0,0x00,0x00,0x00},                      /* min dénormalisé */
            {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0x7f}, /* max fini */
            {0,0,0,0,0,0,0,0x80,0xff,0x7f},                      /* +inf */
            {0,0,0,0,0,0,0,0x80,0xff,0xff},                      /* -inf */
            {0,0,0,0,0,0,0,0xc0,0xff,0x7f},                      /* NaN */
            {0,0,0,0,0,0,0,0xc0,0xff,0xff},                      /* -NaN */
        };
        for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
            uint8_t out[10];
            rt_f80_put(out, rt_f80_get(cases[i]));
            if (memcmp(out, cases[i], 10) != 0) {
                bad++;
                printf("ECHEC cas ciblé %u\n", i);
            }
        }
    }

    printf("F80 CROISE %s (%d echec(s))\n", bad ? "ECHEC" : "OK", bad);
    return bad != 0;
}
