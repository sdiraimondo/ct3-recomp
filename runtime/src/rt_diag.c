/* Diagnostic (build de traçage CT3_TRACE_INSN=1) : sondes invitées, activées par variables d'environnement.
 *
 * CT3_PROBE=<lo>-<hi>      : affiche chaque instruction invitée exécutée dans la plage (adresses hexadécimales),
 *                            avec les registres principaux et des lectures bornées.
 * CT3_MEMWATCH=<lo>-<hi>   : affiche chaque écriture invitée dans la plage, avec l'instruction qui l'a faite.
 * CT3_MEMWATCH_VAL=<hex>  : idem pour toute écriture de cette valeur (32 bits), à n'importe quelle adresse.
 *
 * Chaque sortie est limitée à quelques dizaines de milliers de lignes. */

#include "rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t rt_probe_lo, rt_probe_span;

/* CT3_RING=<n presents> : après ce nombre de présentations, les 256 dernières instructions sont gardées
 * dans un tampon circulaire et affichées à l'appel d'une adresse non traduite (rt_probe_dump). */
static uint32_t ring_addr[256], ring_esp[256];
static unsigned ring_n, presents;
static unsigned ring_after = ~0u;
static int ring_on, ring_after_set;

uint32_t rt_memwatch_lo, rt_memwatch_span, rt_memwatch_val;

static uint32_t probe_rd(uint32_t g)
{
    uint32_t v = 0;
    if (g >= 0x10000u && g < 0x7F000000u - 4u) memcpy(&v, rt_mem_base + g, 4);
    return v;
}

void rt_probe_hit(cpu_t *c, uint32_t addr)
{
    static unsigned shown = 0;
    if (ring_on) {
        ring_addr[ring_n & 255] = addr;
        ring_esp[ring_n & 255] = (uint32_t)cpu_rd(c, 16, 4);
        ring_n++;
        return;
    }
    if (shown++ >= 200000) return;
    uint32_t eax = (uint32_t)cpu_rd(c, 0, 4);
    uint32_t ecx = (uint32_t)cpu_rd(c, 4, 4);
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    uint32_t edi = (uint32_t)cpu_rd(c, 28, 4);
    fprintf(stderr, "[probe 0x%08x] eax=0x%08x esp=0x%08x ecx=0x%08x edi=0x%08x FILE{ptr=0x%08x cnt=%u base=0x%08x flag=0x%x} pile[+44..+4c]=%08x %08x %08x\n",
            addr, eax, esp, ecx, edi, probe_rd(edi), probe_rd(edi + 4), probe_rd(edi + 8), probe_rd(edi + 12),
            probe_rd(esp + 0x44), probe_rd(esp + 0x48), probe_rd(esp + 0x4c));
    if (addr == 0x00539728u) {
        uint32_t sp = (uint32_t)cpu_rd(c, 16, 4);
        fprintf(stderr, "    handler 539728: esp=0x%08x [esp]=0x%08x [esp+4]=0x%08x [esp+8]=0x%08x [esp+0xc]=0x%08x insn precedente=0x%08x\n",
                sp, probe_rd(sp), probe_rd(sp + 4), probe_rd(sp + 8), probe_rd(sp + 12), rt_cur_insn);
    }
    if (addr == 0x00538e1bu) {
        uint32_t ebp = (uint32_t)cpu_rd(c, 20, 4);
        fprintf(stderr, "    funclet: esp=0x%08x [esp]=0x%08x [esp+4]=0x%08x ebp=0x%08x [ebp-0x10]=0x%08x (insn precedente 0x%08x)\n",
                (uint32_t)cpu_rd(c, 16, 4), probe_rd((uint32_t)cpu_rd(c, 16, 4)), probe_rd((uint32_t)cpu_rd(c, 16, 4) + 4), ebp, probe_rd(ebp - 0x10), rt_cur_insn);
    }
    if (addr == 0x00515c71u) {
        uint32_t ebp = (uint32_t)cpu_rd(c, 20, 4);
        fprintf(stderr, "    epilogue: eax=0x%08x ebp=0x%08x [ebp+4]=0x%08x [ebp+8]=0x%08x [ebp+0xc]=0x%08x\n",
                eax, ebp, probe_rd(ebp + 4), probe_rd(ebp + 8), probe_rd(ebp + 12));
    }
    if (addr == 0x004147deu || addr == 0x004147e5u)
        fprintf(stderr, "    arg1[esp+0xfc]=0x%08x arg2[esp+0x100]=0x%08x compte[esp+0x18]=%u\n",
                probe_rd(esp + 0xfc), probe_rd(esp + 0x100), probe_rd(esp + 0x18));
}

void rt_probe_present(void)
{
    if (!ring_after_set) return;
    if (++presents >= ring_after && !ring_on) {
        ring_on = 1;
        rt_probe_lo = 0;
        rt_probe_span = 0xFFFFFFFEu;
        fprintf(stderr, "rt: tampon d'instructions actif (présentation %u)\n", presents);
    }
}

void rt_probe_dump(void)
{
    if (!ring_on) return;
    fprintf(stderr, "rt: dernières instructions (plus ancienne d'abord) :\n");
    unsigned n = ring_n < 256 ? ring_n : 256;
    for (unsigned k = ring_n - n; k < ring_n; k++)
        fprintf(stderr, "    0x%08x esp=0x%08x\n", ring_addr[k & 255], ring_esp[k & 255]);
}

void rt_memwatch_hit(cpu_t *c, uint32_t addr, unsigned size, uint64_t v)
{
    static unsigned shown = 0;
    (void)c;
    if (shown++ >= 20000) return;
    fprintf(stderr, "[ecriture 0x%08x] taille=%u valeur=0x%llx depuis l'instruction 0x%08x\n",
            addr, size, (unsigned long long)v, rt_cur_insn);
}

static void parse_range(const char *v, uint32_t *lo, uint32_t *span)
{
    char *end = NULL;
    uint32_t a = (uint32_t)strtoul(v, &end, 16);
    uint32_t b = end && *end == '-' ? (uint32_t)strtoul(end + 1, NULL, 16) : a;
    *lo = a;
    *span = b - a + 1;
}

void rt_diag_init(void)
{
    const char *mw = getenv("CT3_MEMWATCH");
    if (mw) parse_range(mw, &rt_memwatch_lo, &rt_memwatch_span);
    const char *mv = getenv("CT3_MEMWATCH_VAL");
    if (mv) rt_memwatch_val = (uint32_t)strtoul(mv, NULL, 16);
    const char *rg = getenv("CT3_RING");
    if (rg) {
        ring_after = (unsigned)strtoul(rg, NULL, 10);
        ring_after_set = 1;
    }
    const char *pr = getenv("CT3_PROBE");
    if (pr) parse_range(pr, &rt_probe_lo, &rt_probe_span);
}
