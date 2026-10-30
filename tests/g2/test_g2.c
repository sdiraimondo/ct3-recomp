/* Gate G2 : chargement de CT3.exe à 0x400000 et exécution de routines traduites.
 *
 * Routines testées (adresses dans CT3.exe, voir functions.txt) :
 *   0x518640 _strrchr          cdecl, chaînes, repne scasb / std / cld
 *   0x42e740 FUN_0042e740      thiscall + stdcall (ret 8), copie de 64 octets
 *   0x4c1960 FUN_004c1960      thiscall, appel IAT vers CloseHandle (talon Win32)
 *
 * Usage : test_g2 <chemin de CT3.exe>
 */

#include "rt.h"
#include "rt_pe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

rt_pe_image *rt_pe_load(const char *path);
void rt_set_image(rt_pe_image *img);
int rt_mem_protect(uint32_t addr, uint32_t size, int prot);
extern unsigned rt_shim_calls;
extern uint32_t rt_shim_last_arg0;

static int failures;
static cpu_t cpu;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (cond) {                                       \
            printf("  ok     %s\n", #cond);               \
        } else {                                          \
            failures++;                                   \
            printf("  ECHEC  %s : ", #cond);              \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static void reset_cpu(void)
{
    memset(&cpu, 0, sizeof cpu);
    cpu_wr(&cpu, 16, 4, RT_STACK_TOP);
}

static void push32(uint32_t v)
{
    uint32_t esp = (uint32_t)cpu_rd(&cpu, 16, 4) - 4;
    cpu_wr(&cpu, 16, 4, esp);
    guest_st(&cpu, esp, 4, v);
}

static uint32_t esp(void) { return (uint32_t)cpu_rd(&cpu, 16, 4); }
static uint32_t eax(void) { return (uint32_t)cpu_rd(&cpu, 0, 4); }

static void call_guest(uint32_t entry)
{
    push32(RT_HOST_RET);
    dispatch(&cpu, entry);
}

static uint32_t *guest_u32(uint32_t addr) { return (uint32_t *)(rt_mem_base + addr); }

static void test_strrchr(void)
{
    static const char text[] = "C:\\CT3\\Media\\bank00.art";
    uint32_t str = RT_SCRATCH_BASE;
    memcpy(rt_mem_base + str, text, sizeof text);

    printf("_strrchr(\"%s\", '\\\\')\n", text);
    reset_cpu();
    push32('\\');
    push32(str);
    call_guest(0x518640);

    uint32_t expected = str + (uint32_t)(strrchr(text, '\\') - text);
    CHECK(eax() == expected, "eax=0x%08x attendu 0x%08x", eax(), expected);
    CHECK(esp() == RT_STACK_TOP - 8, "esp=0x%08x (cdecl : l'appelant dépile)", esp());
}

static void test_pool_entry(void)
{
    uint32_t obj = RT_SCRATCH_BASE + 0x1000;
    uint32_t dst = RT_SCRATCH_BASE + 0x2000;
    uint32_t src = obj + 512; /* index 7 : (7 + 1) << 6 */

    memset(rt_mem_base + RT_SCRATCH_BASE, 0, 0x3000);
    *guest_u32(obj + 0x840) = 10;
    for (unsigned i = 0; i < 16; i++) {
        *guest_u32(src + 4 * i) = 0x1000 + 3 * i;
    }

    printf("FUN_0042e740(this=0x%08x, dst=0x%08x, n=3)\n", obj, dst);
    reset_cpu();
    cpu_wr(&cpu, 4, 4, obj);
    push32(3);
    push32(dst);
    call_guest(0x42e740);

    int copy_ok = 1;
    for (unsigned i = 0; i < 16; i++) {
        if (*guest_u32(dst + 4 * i) != 0x1000 + 3 * i) {
            copy_ok = 0;
        }
    }
    CHECK(*guest_u32(obj + 0x840) == 7, "compteur=%u attendu 7", *guest_u32(obj + 0x840));
    CHECK(copy_ok, "les 16 DWORD copiés ne correspondent pas à l'entrée source");
    CHECK(esp() == RT_STACK_TOP, "esp=0x%08x (stdcall ret 8 : deux arguments dépilés)", esp());
}

static void test_import_call(void)
{
    uint32_t handle_slot = RT_SCRATCH_BASE + 0x3000;

    printf("FUN_004c1960(this=0x%08x) avec handle 0x1234\n", handle_slot);
    *guest_u32(handle_slot) = 0x1234;
    reset_cpu();
    cpu_wr(&cpu, 4, 4, handle_slot);
    rt_shim_calls = 0;
    call_guest(0x4c1960);
    CHECK(rt_shim_calls == 1, "CloseHandle interceptée %u fois, attendu 1", rt_shim_calls);
    CHECK(rt_shim_last_arg0 == 0x1234, "argument=0x%08x, attendu 0x1234", rt_shim_last_arg0);
    CHECK(esp() == RT_STACK_TOP, "esp=0x%08x (thiscall, ret nu)", esp());

    printf("FUN_004c1960 avec handle nul : pas d'appel\n");
    *guest_u32(handle_slot) = 0;
    reset_cpu();
    cpu_wr(&cpu, 4, 4, handle_slot);
    rt_shim_calls = 0;
    call_guest(0x4c1960);
    CHECK(rt_shim_calls == 0, "CloseHandle appelée %u fois, attendu 0", rt_shim_calls);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage : %s <CT3.exe>\n", argv[0]);
        return 1;
    }

    rt_pe_image *img = rt_pe_load(argv[1]);
    if (!img) {
        return 1;
    }
    rt_set_image(img);
    printf("image chargée à 0x%08x, taille 0x%x, %u imports\n", RT_IMAGE_BASE, img->size, img->import_count);
    printf("point d'entrée 0x%08x\n", img->entry);

    uint32_t iat_slot_close = 0x53b078; /* KERNEL32.dll::CloseHandle */
    uint32_t stub = *guest_u32(iat_slot_close);
    CHECK(stub >= RT_STUB_BASE && stub < RT_STUB_END, "IAT CloseHandle = 0x%08x (talon attendu)", stub);
    CHECK(stub >= RT_STUB_BASE && stub < RT_STUB_END &&
              strcmp(img->import_names[(stub - RT_STUB_BASE) / 16], "KERNEL32.dll::CloseHandle") == 0,
          "le talon ne correspond pas à KERNEL32.dll::CloseHandle");

    if (rt_mem_protect(RT_SCRATCH_BASE, RT_SCRATCH_SIZE, 3) != 0) {
        return 1;
    }

    test_strrchr();
    test_pool_entry();
    test_import_call();

    printf("\n%s (%d échec(s))\n", failures ? "G2 ECHEC" : "G2 OK", failures);
    return failures ? 2 : 0;
}
