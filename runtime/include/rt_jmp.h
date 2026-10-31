#pragma once

/* setjmp/longjmp invités (__setjmp3 / _longjmp du CRT MSVC).
 *
 * Le code traduit appelle setjmp() hôte dans le cadre qui appelle __setjmp3 : ce cadre reste actif
 * tant que la reprise est possible. Un longjmp invité restaure l'état du CPU puis saute à l'adresse
 * de reprise : rt_jmp_try reconnaît le cadre (adresse de reprise et ESP identiques) et exécute un
 * longjmp hôte vers ce cadre, en abandonnant les cadres C intermédiaires. */

#include "rt.h"

#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

void rt_jmp_register(cpu_t *c, jmp_buf *jb, uint32_t guest_buf, uint32_t resume);
void rt_jmp_try(cpu_t *c, uint32_t addr);

#ifdef __cplusplus
}
#endif
