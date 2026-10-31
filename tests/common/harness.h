/* Harnais commun des tests de routines traduites : chargement de CT3.exe, pile, registres. */

#pragma once

#include "rt.h"
#include "rt_pe.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

rt_pe_image *rt_pe_load(const char *path);
void rt_set_image(rt_pe_image *img);
void rt_set_exe_path(const char *path);
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
    cpu_wr(&cpu, 4256, 2, 0x037F); /* mot de contrôle x87 par défaut */
}

static void push32(uint32_t v)
{
    uint32_t esp = (uint32_t)cpu_rd(&cpu, 16, 4) - 4;
    cpu_wr(&cpu, 16, 4, esp);
    guest_st(&cpu, esp, 4, v);
}

static uint32_t esp(void) { return (uint32_t)cpu_rd(&cpu, 16, 4); }
static uint32_t eax(void) { return (uint32_t)cpu_rd(&cpu, 0, 4); }
static uint32_t edx(void) { return (uint32_t)cpu_rd(&cpu, 8, 4); }

static void call_guest(uint32_t entry)
{
    push32(RT_HOST_RET);
    dispatch(&cpu, entry);
}

static uint32_t *guest_u32(uint32_t addr) { return (uint32_t *)(rt_mem_base + addr); }

/* ST(i) : offset p-code 4352 + 16 * i, valeur sur 10 octets (format x87). */
static void st_set(unsigned i, long double v)
{
    memcpy(cpu.reg + 4352 + 16 * i, &v, 10);
}

static long double st_get(unsigned i)
{
    long double v = 0;
    memcpy(&v, cpu.reg + 4352 + 16 * i, 10);
    return v;
}

static rt_pe_image *harness_load(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage : %s <CT3.exe>\n", argv[0]);
        return NULL;
    }
    rt_pe_image *img = rt_pe_load(argv[1]);
    if (!img) {
        return NULL;
    }
    rt_set_image(img);
    char resolved[1024];
    if (realpath(argv[1], resolved)) {
        rt_set_exe_path(resolved);
    }
    if (rt_mem_protect(RT_SCRATCH_BASE, RT_SCRATCH_SIZE, 3) != 0) {
        return NULL;
    }
    printf("image chargée à 0x%08x, taille 0x%x, %u imports\n", RT_IMAGE_BASE, img->size, img->import_count);
    return img;
}
