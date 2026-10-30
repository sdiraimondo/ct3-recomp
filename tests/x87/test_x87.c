/* Tests x87 : routines réelles de CT3.exe traduites, comparées à une référence C.
 *
 *   0x518ffc __ftol          FNSTCW/FLDCW (troncature), FISTP 64 bits, restauration du mot de contrôle
 *   0x4696b0 FUN_004696b0    FILD 32 bits, FST/FSTP mémoire, thiscall ret 8
 *   0x42e5a0 FUN_0042e5a0    FLD/FADD mémoire, stdcall ret 4
 *   0x426d60 FUN_00426d60    FLD/FMUL mémoire, stdcall ret 4
 *   0x4576f0 FUN_004576f0    FUCOMPP, FNSTSW AX, FSQRT, constante en .rdata, cdecl
 *   0x51f780 FUN_0051f780    2^x : FRNDINT, F2XM1, FSCALE, pile x87, ret nu
 *
 * Usage : test_x87 <chemin de CT3.exe>
 */

#include "harness.h"

#include <math.h>

static void expect_float_mem(uint32_t addr, float expected, const char *what)
{
    float got;
    memcpy(&got, rt_mem_base + addr, 4);
    CHECK(got == expected, "%s = %a, attendu %a", what, (double)got, (double)expected);
}

static void test_ftol(void)
{
    static const struct {
        long double in;
        int64_t expected;
    } cases[] = {
        {3.7L, 3},
        {-3.7L, -3},
        {2.5L, 2},
        {-2.5L, -2},
        {-7.999L, -7},
        {1e9L + 0.5L, 1000000000LL},
        {5e9L + 0.2L, 5000000000LL},
    };

    printf("__ftol (0x518ffc) : troncature de ST0 en entier 64 bits\n");
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        reset_cpu();
        st_set(0, cases[i].in);
        call_guest(0x518ffc);
        uint64_t got = ((uint64_t)edx() << 32) | eax();
        CHECK(got == (uint64_t)cases[i].expected, "entrée %Lg : résultat %lld, attendu %lld",
              cases[i].in, (long long)(int64_t)got, (long long)cases[i].expected);
    }
    CHECK(cpu_rd(&cpu, 4256, 2) == 0x037F, "mot de contrôle restauré : 0x%04llx, attendu 0x037f",
          (unsigned long long)cpu_rd(&cpu, 4256, 2));
    CHECK(esp() == RT_STACK_TOP, "esp=0x%08x (ret nu)", esp());
}

static void test_fild_fst(void)
{
    uint32_t obj = RT_SCRATCH_BASE + 0x100;
    int32_t a = 7;
    int32_t b = -123456789;

    printf("FUN_004696b0 (0x4696b0) : FILD, FST, FSTP mémoire\n");
    memset(rt_mem_base + obj, 0, 0x40);
    reset_cpu();
    cpu_wr(&cpu, 4, 4, obj);
    push32((uint32_t)b);
    push32((uint32_t)a);
    call_guest(0x4696b0);
    expect_float_mem(obj + 0x08, (float)a, "[this+8]");
    expect_float_mem(obj + 0x20, (float)a, "[this+0x20]");
    expect_float_mem(obj + 0x24, (float)b, "[this+0x24]");
    CHECK(esp() == RT_STACK_TOP, "esp=0x%08x (stdcall ret 8)", esp());
}

static void test_fadd_mem(void)
{
    uint32_t dst = RT_SCRATCH_BASE + 0x200;
    uint32_t src = RT_SCRATCH_BASE + 0x240;
    float d[3] = {1.5f, 2.25f, -3.0f};
    float s[3] = {0.5f, 0.75f, 4.0f};

    printf("FUN_0042e5a0 (0x42e5a0) : dst[i] += src[i] en x87\n");
    memcpy(rt_mem_base + dst, d, sizeof d);
    memcpy(rt_mem_base + src, s, sizeof s);
    reset_cpu();
    cpu_wr(&cpu, 4, 4, dst);
    push32(src);
    call_guest(0x42e5a0);
    expect_float_mem(dst + 0, 2.0f, "dst[0]");
    expect_float_mem(dst + 4, 3.0f, "dst[1]");
    expect_float_mem(dst + 8, 1.0f, "dst[2]");
    CHECK(esp() == RT_STACK_TOP, "esp=0x%08x (stdcall ret 4)", esp());

    float d2[3] = {0.1f, 1234.5678f, -1e-3f};
    float s2[3] = {0.7f, -1234.5f, 3.3e5f};
    memcpy(rt_mem_base + dst, d2, sizeof d2);
    memcpy(rt_mem_base + src, s2, sizeof s2);
    reset_cpu();
    cpu_wr(&cpu, 4, 4, dst);
    push32(src);
    call_guest(0x42e5a0);
    for (unsigned i = 0; i < 3; i++) {
        float expected = (float)((long double)d2[i] + (long double)s2[i]);
        float got;
        memcpy(&got, rt_mem_base + dst + 4 * i, 4);
        CHECK(got == expected, "dst[%u] = %a, attendu %a", i, (double)got, (double)expected);
    }
}

static void test_fmul_mem(void)
{
    uint32_t obj = RT_SCRATCH_BASE + 0x300;
    float v[3] = {2.0f, 3.0f, 4.0f};

    printf("FUN_00426d60 (0x426d60) : [this+i] = x * [this+i] en x87\n");
    memcpy(rt_mem_base + obj, v, sizeof v);
    reset_cpu();
    cpu_wr(&cpu, 4, 4, obj);
    push32(0x3F000000u); /* 0.5f */
    call_guest(0x426d60);
    expect_float_mem(obj + 0, 1.0f, "[this+0]");
    expect_float_mem(obj + 4, 1.5f, "[this+4]");
    expect_float_mem(obj + 8, 2.0f, "[this+8]");
    CHECK(esp() == RT_STACK_TOP, "esp=0x%08x (stdcall ret 4)", esp());

    float x = 0.1f;
    float w[3] = {1.7f, -3.1f, 1e4f};
    memcpy(rt_mem_base + obj, w, sizeof w);
    reset_cpu();
    cpu_wr(&cpu, 4, 4, obj);
    uint32_t xbits;
    memcpy(&xbits, &x, 4);
    push32(xbits);
    call_guest(0x426d60);
    for (unsigned i = 0; i < 3; i++) {
        float expected = (float)((long double)x * (long double)w[i]);
        float got;
        memcpy(&got, rt_mem_base + obj + 4 * i, 4);
        CHECK(got == expected, "[this+%u] = %a, attendu %a", 4 * i, (double)got, (double)expected);
    }
}

static void test_fucompp_sqrt(void)
{
    float c;
    memcpy(&c, rt_mem_base + 0x53bbfc, 4);
    printf("FUN_004576f0 (0x4576f0) : constante .rdata = %a\n", (double)c);

    struct {
        float x;
        const char *what;
    } cases[] = {
        {4.0f, "x = 4 (racine)"},
        {9.0f, "x = 9 (racine)"},
        {-4.0f, "x = -4 (inférieur : racine négative, NaN)"},
        {c, "x = constante (égal)"},
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        uint32_t xbits;
        memcpy(&xbits, &cases[i].x, 4);
        reset_cpu();
        push32(xbits);
        call_guest(0x4576f0);
        long double expected = (cases[i].x == c) ? (long double)c : sqrtl((long double)cases[i].x);
        if (isnan(expected)) {
            CHECK(isnan(st_get(0)), "%s : ST0 = %Lg, attendu NaN", cases[i].what, st_get(0));
        } else {
            CHECK(st_get(0) == expected, "%s : ST0 = %Lg, attendu %Lg", cases[i].what, st_get(0), expected);
        }
        CHECK(esp() == RT_STACK_TOP - 4, "%s : esp=0x%08x (cdecl, argument laissé)", cases[i].what, esp());
    }

    reset_cpu();
    push32(0x7FC00000u); /* NaN */
    call_guest(0x4576f0);
    CHECK(isnan(st_get(0)), "x = NaN : ST0 = %Lg, attendu NaN", st_get(0));
}

static void test_exp2(void)
{
    printf("FUN_0051f780 (0x51f780) : ST0 = 2^ST0\n");
    static const long double xs[] = {3.7L, -3.25L, 0.0L, 10.5L, -0.4L};
    for (unsigned i = 0; i < sizeof xs / sizeof xs[0]; i++) {
        reset_cpu();
        st_set(0, xs[i]);
        call_guest(0x51f780);
        long double expected = exp2l(xs[i]);
        long double got = st_get(0);
        long double err = fabsl(got - expected) / expected;
        CHECK(err < 1e-15L, "2^%Lg = %.19Lg, attendu %.19Lg (erreur relative %Lg)", xs[i], got, expected, err);
        CHECK(esp() == RT_STACK_TOP, "esp=0x%08x (ret nu)", esp());
    }
}

int main(int argc, char **argv)
{
    rt_pe_image *img = harness_load(argc, argv);
    if (!img) {
        return 1;
    }

    test_ftol();
    test_fild_fst();
    test_fadd_mem();
    test_fmul_mem();
    test_fucompp_sqrt();
    test_exp2();

    printf("\n%s (%d échec(s))\n", failures ? "X87 ECHEC" : "X87 OK", failures);
    return failures ? 2 : 0;
}
