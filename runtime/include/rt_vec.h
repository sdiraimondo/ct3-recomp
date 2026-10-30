#pragma once

/* Opérations MMX, 3DNow! et SSE du runtime.
 * Chaque fonction lit ses entrées (a, b) puis écrit out. out peut être égal à a ou b.
 * MMX et 3DNow! : 8 octets. SSE : 16 octets. */

#include <stdint.h>

void rt_v_PACKSSWB(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PACKSSDW(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PADDSW(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PSUBSW(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PMADDWD(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PMULHW(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PSLLW(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PSRAW(uint8_t *out, const uint8_t *a, const uint8_t *b);

/* Opérations entières sur valeurs de plus de 8 octets (n octets). out peut alias a ou b. */
void rt_w_and(uint8_t *out, const uint8_t *a, const uint8_t *b, unsigned n);
void rt_w_or(uint8_t *out, const uint8_t *a, const uint8_t *b, unsigned n);
void rt_w_xor(uint8_t *out, const uint8_t *a, const uint8_t *b, unsigned n);
void rt_w_not(uint8_t *out, const uint8_t *a, unsigned n);
void rt_w_shr(uint8_t *out, const uint8_t *a, unsigned n, unsigned bits);
void rt_w_shl(uint8_t *out, const uint8_t *a, unsigned n, unsigned bits);
void rt_w_zext(uint8_t *out, const uint8_t *a, unsigned nin, unsigned nout);

/* 3DNow! : dst = a (registre de destination), src = b. Voies : [0] = bas, [1] = haut. */
void rt_v_PFADD(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFSUB(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFSUBR(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFMUL(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFMIN(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFMAX(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFCMPGE(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFCMPGT(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFCMPEQ(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFACC(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFNACC(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PSWAPD(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PI2FD(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PF2ID(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFRCP(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFRSQRT(uint8_t *out, const uint8_t *a, const uint8_t *b);
/* Étapes d'itération de Newton-Raphson : formules mathématiques, non vérifiées au matériel (D-025). */
void rt_v_PFRCPIT1(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFRSQIT1(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_PFRCPIT2(uint8_t *out, const uint8_t *a, const uint8_t *b);

/* SSE : 16 octets, quatre voies flottantes simple précision. */
void rt_v_RCPPS(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_RCPSS(uint8_t *out, const uint8_t *a, const uint8_t *b);
void rt_v_RSQRTSS(uint8_t *out, const uint8_t *a, const uint8_t *b);
