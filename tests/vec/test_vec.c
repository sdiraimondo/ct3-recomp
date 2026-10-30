/* Vecteurs des opérations MMX, 3DNow! et SSE : Unicorn (référence) contre le runtime. */

#include "rt_vec.h"
#include "vectors.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef void (*vec_fn)(uint8_t *, const uint8_t *, const uint8_t *);

static int failures;

/* Deux flottants NaN sont égaux (seul le motif de bits de NaN peut différer). */
static int same_lane(const uint8_t *x, const uint8_t *y, int size, int is_float)
{
    if (!is_float) return memcmp(x, y, size) == 0;
    for (int i = 0; i + 4 <= size; i += 4) {
        float fx, fy;
        memcpy(&fx, x + i, 4);
        memcpy(&fy, y + i, 4);
        if (fx != fx && fy != fy) continue;
        if (memcmp(x + i, y + i, 4) != 0) return 0;
    }
    return 1;
}

static void run(const char *name, vec_fn fn, const void *vecs_, int count, int size, int is_float)
{
    const uint8_t (*vecs)[3][16] = (const uint8_t (*)[3][16])vecs_;
    int bad = 0;
    for (int i = 0; i < count; i++) {
        uint8_t out[16] = {0};
        fn(out, vecs[i][0], vecs[i][1]);
        if (!same_lane(out, vecs[i][2], size, is_float)) {
            if (bad == 0) {
                printf("  ECHEC  %s vecteur %d\n", name, i);
            }
            bad++;
        }
    }
    if (bad) {
        failures++;
    } else {
        printf("  ok     %s (%d vecteurs)\n", name, count);
    }
}

#define RUN(NAME) run(#NAME, rt_v_##NAME, vec_##NAME, VEC_COUNT_##NAME, VEC_SIZE_##NAME, 0)
#define RUNF(NAME) run(#NAME, rt_v_##NAME, vec_##NAME, VEC_COUNT_##NAME, VEC_SIZE_##NAME, 1)

/* Newton : une étape doit réduire l'erreur d'une approximation de l'inverse ou de la racine inverse.
 * Ces opérations ne sont pas comparées au matériel (D-025). */
static void check_newton(void)
{
    static const float ds[] = {0.5f, 1.0f, 3.0f, 7.25f, 100.0f, 1e-3f, 4096.0f};
    int bad = 0;
    for (unsigned i = 0; i < sizeof ds / sizeof ds[0]; i++) {
        float d = ds[i];
        uint8_t dd[8], x[8], t[8], y[8];
        float xi[2] = {(1.0f / d) * 1.001f, (1.0f / d) * 1.001f};
        memcpy(dd, &(float[2]){d, d}, 8);
        memcpy(x, xi, 8);
        rt_v_PFRCPIT1(t, dd, x);
        rt_v_PFRCPIT2(y, t, x);
        float x1[2];
        memcpy(x1, y, 8);
        double e0 = fabs((double)xi[0] * d - 1.0), e1 = fabs((double)x1[0] * d - 1.0);
        if (!(e1 <= 4.0 * e0 * e0 + 1e-6)) bad++;

        float yi[2] = {(1.0f / sqrtf(d)) * 1.001f, (1.0f / sqrtf(d)) * 1.001f};
        memcpy(x, yi, 8);
        rt_v_PFRSQIT1(t, dd, x);
        rt_v_PFRCPIT2(y, t, x);
        memcpy(x1, y, 8);
        double r0 = fabs((double)yi[0] * sqrt((double)d) - 1.0), r1 = fabs((double)x1[0] * sqrt((double)d) - 1.0);
        if (!(r1 <= 4.0 * r0 * r0 + 1e-6)) bad++;
    }
    if (bad) {
        failures++;
        printf("  ECHEC  Newton : %d cas sans convergence\n", bad);
    } else {
        printf("  ok     Newton : réciproque et racine inverse convergent (non vérifié au matériel)\n");
    }
}

/* Décalages sur valeurs de 16 octets contre __int128. */
static void check_wide_shifts(void)
{
    uint64_t seed = 0x9E3779B97F4A7C15ull;
    int bad = 0;
    for (int k = 0; k < 2000; k++) {
        uint8_t a[16];
        for (int i = 0; i < 16; i++) {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            a[i] = (uint8_t)(seed >> 33);
        }
        unsigned bits = (unsigned)(k % 140);
        unsigned __int128 v = 0;
        memcpy(&v, a, 16);
        unsigned __int128 r_shr = bits >= 128 ? 0 : v >> bits;
        unsigned __int128 r_shl = bits >= 128 ? 0 : v << bits;
        uint8_t out[16];
        rt_w_shr(out, a, 16, bits);
        if (memcmp(out, &r_shr, 16) != 0) bad++;
        rt_w_shl(out, a, 16, bits);
        if (memcmp(out, &r_shl, 16) != 0) bad++;
    }
    if (bad) {
        failures++;
        printf("  ECHEC  décalages 128 bits : %d écarts\n", bad);
    } else {
        printf("  ok     décalages 128 bits (2000 cas, 0 à 139 bits)\n");
    }
}

int main(void)
{
    printf("MMX (entiers)\n");
    RUN(PACKSSWB);
    RUN(PACKSSDW);
    RUN(PADDSW);
    RUN(PSUBSW);
    RUN(PMADDWD);
    RUN(PMULHW);
    RUN(PSLLW);
    RUN(PSRAW);

    printf("\n3DNow! (comparé à Unicorn, modèle Athlon)\n");
    RUNF(PFADD);
    RUNF(PFSUB);
    RUNF(PFSUBR);
    RUNF(PFMUL);
    RUNF(PFMIN);
    RUNF(PFMAX);
    RUN(PFCMPGE);
    RUN(PFCMPGT);
    RUN(PFCMPEQ);
    RUNF(PFACC);
    RUNF(PFNACC);
    RUNF(PSWAPD);
    RUNF(PI2FD);
    RUN(PF2ID);
    RUNF(PFRCP);
    RUNF(PFRSQRT);
    check_newton();

    printf("\nSSE (comparé à Unicorn, modèle par défaut)\n");
    RUNF(RCPPS);
    RUNF(RCPSS);
    RUNF(RSQRTSS);

    printf("\nValeurs de plus de 8 octets\n");
    check_wide_shifts();

    printf("\n%s (%d échec(s))\n", failures ? "VEC ECHEC" : "VEC OK", failures);
    return failures ? 2 : 0;
}
