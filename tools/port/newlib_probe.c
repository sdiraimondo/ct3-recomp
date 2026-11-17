/*
 * Sonde newlib/devkitA64 pour le port Switch (tâche S1).
 *
 * Mesure le format de long double, la précision réelle et le comportement de
 * ldexpl/frexpl dans la libm de devkitA64. Aucune dépendance à libnx : le
 * binaire est bare-metal et écrit via semihosting, pour être exécuté par
 * qemu-system-aarch64.
 *
 * Compilation (dans l'image devkitpro/devkita64) :
 *   aarch64-none-elf-gcc -O2 -std=c11 -Wall -Wextra \
 *       -DPROBE_BAREMETAL -nostartfiles -Wl,-e,_start \
 *       -Wl,-Ttext-segment=0x40000000 --specs=nosys.specs \
 *       newlib_probe.c -o newlib_probe.elf -lm
 *
 * Exécution :
 *   qemu-system-aarch64 -M virt -cpu cortex-a57 -m 256M \
 *       -semihosting-config enable=on,target=native -nographic \
 *       -kernel newlib_probe.elf
 */

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>

static int failures;

/* Semihosting ARM : SYS_WRITE0 = 0x04, SYS_EXIT = 0x18. */
static void sh_puts(const char *s)
{
    register long x0 __asm__("x0") = 0x04;
    register const char *x1 __asm__("x1") = s;
    __asm__ volatile("hlt #0xF000" : "+r"(x0) : "r"(x1) : "memory");
}

static void sh_exit(int code)
{
    long block[2] = { 0x20026 /* ADP_Stopped_ApplicationExit */, code };
    register long x0 __asm__("x0") = 0x18;
    register long *x1 __asm__("x1") = block;
    __asm__ volatile("hlt #0xF000" : "+r"(x0) : "r"(x1) : "memory");
    for (;;) {
    }
}

static void put_u64(unsigned long long v)
{
    char buf[21];
    int i = 20;
    buf[i] = '\0';
    do {
        buf[--i] = (char)('0' + (v % 10));
        v /= 10;
    } while (v != 0);
    sh_puts(&buf[i]);
}

static void put_i64(long long v)
{
    if (v < 0) {
        sh_puts("-");
        put_u64((unsigned long long)(-(v + 1)) + 1ULL);
    } else {
        put_u64((unsigned long long)v);
    }
}

static void check(const char *name, int ok)
{
    sh_puts(name);
    sh_puts("=");
    sh_puts(ok ? "1\n" : "0\n");
    if (!ok) {
        failures++;
    }
}

static void report(const char *name, long long value)
{
    sh_puts(name);
    sh_puts("=");
    put_i64(value);
    sh_puts("\n");
}

#ifdef PROBE_BAREMETAL
/*
 * QEMU ne positionne pas SP avant de sauter à l'entrée : on installe une pile
 * statique et on appelle main directement, sans crt0 newlib.
 */
unsigned char probe_stack[32768] __attribute__((aligned(16), used));

int main(void);

__asm__(
    ".global _start\n"
    ".type _start, %function\n"
    "_start:\n"
    "  mrs  x1, cpacr_el1\n"
    "  orr  x1, x1, #(0x3 << 20)\n"
    "  msr  cpacr_el1, x1\n"
    "  isb\n"
    "  adrp x0, probe_stack\n"
    "  add  x0, x0, :lo12:probe_stack\n"
    "  add  x0, x0, #32768\n"
    "  mov  sp, x0\n"
    "  bl   main\n"
    "1: b 1b\n"
    ".size _start, .-_start\n");
#endif

int main(void)
{
    sh_puts("NEWLIB PROBE (devkitA64)\n");

    /* Format compile-time. */
    report("sizeof_long_double", (long long)sizeof(long double));
    report("ldbl_mant_dig", (long long)LDBL_MANT_DIG);
    report("ldbl_max_exp", (long long)LDBL_MAX_EXP);
    report("ldbl_min_exp", (long long)LDBL_MIN_EXP);
    report("sizeof_double", (long long)sizeof(double));
    report("dbl_mant_dig", (long long)DBL_MANT_DIG);

    /* Boutisme. */
    {
        union {
            unsigned int u;
            unsigned char b[4];
        } e;
        e.u = 0x01020304u;
        report("little_endian", e.b[0] == 0x04 && e.b[3] == 0x01);
    }

    /* Signe de char (AArch64 est non signé par défaut). */
    report("char_signed", (char)0xFF < 0);
    report("char_min", (long long)CHAR_MIN);

    /* Précision au-delà du double. */
    check("ldbl_eps_nonzero", LDBL_EPSILON > 0.0L);
    check("ldbl_precision_gt_double", 1.0L + LDBL_EPSILON != 1.0L);
    check("ldbl_rounds_to_double",
          (long double)(double)(1.0L + LDBL_EPSILON) == 1.0L);
    check("ldbl_max_finite", isfinite(LDBL_MAX) && LDBL_MAX > 0.0L);
    check("ldbl_min_positive", LDBL_MIN > 0.0L && isfinite(LDBL_MIN));

    /* ldexpl : cas de base. */
    check("ldexpl_zero", ldexpl(1.0L, 0) == 1.0L);
    check("ldexpl_up", ldexpl(1.0L, 1) == 2.0L);
    check("ldexpl_down", ldexpl(1.0L, -1) == 0.5L);

    /* ldexpl : bornes du binary128. */
    check("ldexpl_max_finite", isfinite(ldexpl(1.0L, 16383)));
    check("ldexpl_overflow_inf", isinf(ldexpl(1.0L, 16384)));
    check("ldexpl_underflow_zero", ldexpl(1.0L, -16495) == 0.0L);

    /* frexpl : cas de base et signes. */
    {
        int e = -12345;
        long double m = frexpl(3.0L, &e);
        check("frexpl_basic_mantissa", m == 0.75L);
        report("frexpl_basic_exp", e);
    }
    {
        int e = -12345;
        long double m = frexpl(-3.0L, &e);
        check("frexpl_negative_mantissa", m == -0.75L);
        report("frexpl_negative_exp", e);
    }
    {
        int e = -12345;
        long double m = frexpl(0.0L, &e);
        check("frexpl_zero", m == 0.0L);
        report("frexpl_zero_exp", e);
    }
    {
        int e = -12345;
        long double m = frexpl(INFINITY, &e);
        check("frexpl_inf", isinf(m) && m > 0.0L);
        report("frexpl_inf_exp", e);
    }

    /* Aller-retour ldexpl/frexpl sur une plage d'exposants. */
    {
        int ok = 1;
        int e;
        for (e = -16000; e <= 16000; e += 997) {
            int got = 0;
            long double x = ldexpl(1.0L, e);
            long double m = frexpl(x, &got);
            if (m != 0.5L || got != e + 1) {
                ok = 0;
                report("roundtrip_fail_exp", e);
                break;
            }
        }
        check("ldexpl_frexpl_roundtrip", ok);
    }

    sh_puts("RESULT=");
    if (failures == 0) {
        sh_puts("PASS\n");
    } else {
        sh_puts("FAIL ");
        put_i64(failures);
        sh_puts("\n");
    }

    sh_exit(failures == 0 ? 0 : 1);
    return failures != 0;
}
