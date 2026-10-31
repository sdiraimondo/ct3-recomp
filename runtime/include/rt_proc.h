#pragma once

#include "rt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Processus minimal : TEB/PEB, tas et pile du thread principal. */

#define RT_FS_OFFSET_REG 272u /* offset p-code du registre FS_OFFSET */
#define RT_TEB_BASE      0x10090000u
#define RT_PEB_BASE      0x10091000u
#define RT_TLS_BASE      0x10092000u   /* emplacements TlsAlloc */
#define RT_HEAP_BASE     0x20000000u
#define RT_HEAP_SIZE     0x0C000000u   /* 192 MiB */
#define RT_STACK_LIMIT   0x10300000u

int rt_process_init(cpu_t *c);
uint32_t rt_heap_alloc(uint32_t size, int zero);
void rt_heap_free(uint32_t p);
uint32_t rt_heap_size(uint32_t p);
uint32_t rt_heap_realloc(uint32_t p, uint32_t size);
uint32_t rt_scratch_alloc(uint32_t size);

#ifdef __cplusplus
}
#endif
