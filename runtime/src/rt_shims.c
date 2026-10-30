#include "rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rt_pe.h"

static rt_pe_image *rt_image;

/* Journal des appels interceptés (utilisé par les tests G2). */
unsigned rt_shim_calls;
uint32_t rt_shim_last_arg0;

typedef uint32_t (*rt_shim_fn)(const uint32_t *args);

static uint32_t shim_CloseHandle(const uint32_t *args)
{
    rt_shim_calls++;
    rt_shim_last_arg0 = args[0];
    return 1; /* TRUE */
}

static const struct {
    const char *name;
    unsigned nargs;
    rt_shim_fn fn;
} rt_shims[] = {
    {"KERNEL32.dll::CloseHandle", 1, shim_CloseHandle},
};

void rt_set_image(rt_pe_image *img)
{
    rt_image = img;
}

void rt_shim_call(cpu_t *c, uint32_t addr)
{
    uint32_t idx = (addr - RT_STUB_BASE) / 16;
    if (!rt_image || idx >= rt_image->import_count) {
        fprintf(stderr, "rt: talon inconnu 0x%08x\n", addr);
        abort();
    }
    const char *name = rt_image->import_names[idx];
    for (unsigned i = 0; i < sizeof rt_shims / sizeof rt_shims[0]; i++) {
        if (strcmp(rt_shims[i].name, name) == 0) {
            uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
            uint32_t args[8] = {0};
            for (unsigned k = 0; k < rt_shims[i].nargs; k++) {
                args[k] = (uint32_t)guest_ld(c, esp + 4 + 4 * k, 4);
            }
            uint32_t ret = rt_shims[i].fn(args);
            cpu_wr(c, 0, 4, ret);
            /* stdcall : l'appelé dépile l'adresse de retour et les arguments. */
            cpu_wr(c, 16, 4, esp + 4 + 4 * rt_shims[i].nargs);
            return;
        }
    }
    fprintf(stderr, "rt: import non implémenté : %s\n", name);
    abort();
}

void rt_unresolved(cpu_t *c, uint32_t addr)
{
    (void)c;
    fprintf(stderr, "rt: appel vers une adresse non traduite 0x%08x\n", addr);
    abort();
}

void rt_unsupported(const char *what, uint32_t addr)
{
    fprintf(stderr, "rt: opération p-code non prise en charge (%s) à 0x%08x\n", what, addr);
    abort();
}
