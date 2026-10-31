/* Diagnostic (build de traçage CT3_TRACE_INSN=1) : sonde d'instructions invitées.
 *
 * CT3_PROBE=<lo>-<hi> (adresses hexadécimales) affiche chaque instruction invitée exécutée dans
 * la plage, jusqu'à 200 000 lignes. */

#include "rt.h"

#include <stdio.h>
#include <stdlib.h>

uint32_t rt_probe_lo, rt_probe_span;

void rt_probe_hit(uint32_t addr)
{
    static unsigned shown = 0;
    if (shown++ >= 200000) return;
    fprintf(stderr, "[probe 0x%08x]\n", addr);
}

void rt_diag_init(void)
{
    const char *pr = getenv("CT3_PROBE");
    if (!pr) return;
    char *end = NULL;
    uint32_t lo = (uint32_t)strtoul(pr, &end, 16);
    uint32_t hi = end && *end == '-' ? (uint32_t)strtoul(end + 1, NULL, 16) : lo;
    rt_probe_lo = lo;
    rt_probe_span = hi - lo + 1;
}
