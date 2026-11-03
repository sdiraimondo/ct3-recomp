/* Filtre d'image D3DX : version native de FUN_004DACC6 (code invité), voir DECISIONS.md, « Portage natif ».
 *
 * Équivalence avec la traduction, bit à bit :
 *  - mêmes opérations, dans le même ordre ;
 *  - les produits intermédiaires restent en long double (l'x87 ne les arrondit pas avant le FSTP) ;
 *  - un arrondi en float uniquement aux points de stockage (FSTP) ;
 *  - les appels vers le code invité (tables de poids, allocations, lignes des images) restent invités,
 *    dans le même ordre, avec les mêmes tailles : le tas évolue exactement comme avec la traduction.
 *
 * Activée par défaut ; CT3_FILTER_NATIVE=0 : traduction seule.
 * CT3_FILTER_CHECK=N : pour les N premiers appels, exécute la traduction puis la
 * version native sur le même état, et compare le tas (contenu, sommet, listes libres) et les registres.
 * Une différence arrête le programme (code 7). */

#include "rt.h"
#include "rt_proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rt_native_filter_on;
static long check_left;
static uint32_t cw_addr[16];
static int cw_count, cw_track;
static long dither_calls;
static int check_dither_only;
static int dither_native_on = 1;
static int dxt3_native_on = 1;
static long dxt3_calls;

/* Traduction de la routine (généré) : présente seulement dans les exécutables qui la lient. */
extern void sub_004dacc6(cpu_t *c) __attribute__((weak));

enum { FN_TABLE = 0x4D7A41, FN_NEW = 0x514666, FN_ARRAY_CTOR = 0x401000, FN_ARRAY_DTOR = 0x4DA5F5,
       FN_FREE = 0x5154BF, CTOR_F4 = 0x4C856C, CTOR_REC = 0x4D7A2B };

static uint32_t rd32(uint32_t a)
{
    uint32_t v;
    memcpy(&v, rt_mem_base + a, 4);
    return v;
}

static void wr32(uint32_t a, uint32_t v)
{
    memcpy(rt_mem_base + a, &v, 4);
}

static float rdf(uint32_t a)
{
    float f;
    memcpy(&f, rt_mem_base + a, 4);
    return f;
}

static void wrf(uint32_t a, float f)
{
    memcpy(rt_mem_base + a, &f, 4);
}

/* Appel d'une fonction invitée depuis le natif : args poussés (premier argument au sommet), RET de l'appelé,
 * ESP rétabli ensuite (l'appelant cdecl nettoie ainsi lui-même). thiscall : ECX = self. */
static uint32_t gcall(cpu_t *c, uint32_t fn, int thiscall, uint32_t self, const uint32_t *args, unsigned n)
{
    uint32_t esp0 = (uint32_t)cpu_rd(c, 16, 4);
    uint32_t esp = esp0;
    for (unsigned k = n; k-- > 0;) {
        esp -= 4;
        guest_st(c, esp, 4, args[k]);
    }
    esp -= 4;
    guest_st(c, esp, 4, RT_HOST_RET);
    cpu_wr(c, 16, 4, esp);
    if (thiscall) cpu_wr(c, 4, 4, self);
    dispatch(c, fn);
    uint32_t r = (uint32_t)cpu_rd(c, 0, 4);
    cpu_wr(c, 16, 4, esp0);
    return r;
}

static void gfree(cpu_t *c, uint32_t p)
{
    gcall(c, FN_FREE, 0, 0, &p, 1);
}

/* Appel d'un constructeur de tableau invité : ptr, taille d'élément, nombre, constructeur. */
static void array_ctor(cpu_t *c, uint32_t ptr, uint32_t esize, uint32_t count, uint32_t ctor)
{
    uint32_t a[4] = { ptr, esize, count, ctor };
    gcall(c, FN_ARRAY_CTOR, 0, 0, a, 4);
}

/* Constantes du code (lues en mémoire invitée, comme la traduction). */
static float k_threshold(void) { return rdf(0x0053CC20); }
static float k_one(void) { return rdf(0x0053BB60); }

/* Serrage selon le mode : variante avec seuil (modes 2 et 3) et variante [0, 1] (mode 1 et voie 3 du mode 2). */
static float clamp_std(float x, float one)
{
    if (0.0f <= x) {
        if (one <= x) return 1.0f;
        return x;
    }
    return 0.0f;
}

static float clamp_thr(float x, float one, float thr)
{
    float f = thr;
    if (thr <= x) {
        if (one <= x) f = 1.0f;
        else f = x;
    }
    return f;
}

/* Corps du filtre. P = pointeur vers l'objet filtre (ECX à l'entrée). Renvoie le HRESULT. */
static uint32_t filter_body(cpu_t *c, uint32_t P)
{
    uint32_t S = rd32(P), D = rd32(P + 4), flags = rd32(P + 8);
    uint32_t mem1 = 0, tab2 = 0, rows = 0, buf = 0, freel = 0;
    uint32_t hr = 0;
    uint32_t a[3];

    if (rd32(D + 0x70) != 1 || rd32(S + 0x70) != 1) return 0x80004005u;

    a[0] = rd32(S + 0x68);
    a[1] = rd32(D + 0x68);
    a[2] = ~(flags >> 16) & 1u;
    mem1 = gcall(c, FN_TABLE, 0, 0, a, 3);
    if (mem1) {
        a[0] = rd32(S + 0x6C);
        a[1] = rd32(D + 0x6C);
        a[2] = ~(flags >> 17) & 1u;
        tab2 = gcall(c, FN_TABLE, 0, 0, a, 3);
    }
    if (!mem1 || !tab2) {
        hr = 0x80004005u;
        goto cleanup;
    }

    {
        uint32_t len1 = rd32(mem1), len2 = rd32(tab2);
        uint32_t n = rd32(D + 0x6C);
        uint32_t sz = n * 12 + 4;
        uint32_t end2 = tab2 + len2;

        rows = gcall(c, FN_NEW, 0, 0, &sz, 1);
        if (!rows) {
            hr = 0x8007000Eu;
            goto cleanup;
        }
        wr32(rows, n);
        rows += 4;
        {
            uint32_t z[4] = { rows, 12, n, CTOR_REC };
            gcall(c, FN_ARRAY_CTOR, 0, 0, z, 4);
        }

        {
            uint32_t w = rd32(S + 0x68);
            uint32_t bsz = w << 4;
            buf = gcall(c, FN_NEW, 0, 0, &bsz, 1);
            if (!buf) {
                hr = 0x8007000Eu;
                goto cleanup;
            }
            array_ctor(c, buf, 0x10, w, CTOR_F4);
        }

        /* Décompte des utilisations de chaque ligne de destination. */
        for (uint32_t p = tab2 + 4; p < end2;) {
            uint32_t end = p + rd32(p);
            for (uint32_t q = p + 4; q < end; q += 8) {
                uint32_t e = rows + rd32(q) * 12;
                wr32(e + 8, rd32(e + 8) + 1);
            }
            p = end;
        }

        {
            uint32_t one_row = 0;
            uint32_t p = tab2 + 4;
            float one = k_one(), thr = k_threshold();

            while (p < end2) {
                uint32_t end = p + rd32(p);
                uint32_t q0 = p + 4;

                /* Allocation (ou réutilisation) des lignes d'accumulation de ce groupe. */
                for (uint32_t q = q0; q < end; q += 8) {
                    uint32_t e = rows + rd32(q) * 12;
                    if (rd32(e) == 0) {
                        if (freel == 0) {
                            uint32_t w = rd32(D + 0x68);
                            uint32_t asz = w << 4;
                            uint32_t acc = gcall(c, FN_NEW, 0, 0, &asz, 1);
                            if (acc) array_ctor(c, acc, 0x10, w, CTOR_F4);
                            wr32(e, acc);
                            if (!acc) {
                                hr = 0x8007000Eu;
                                goto cleanup;
                            }
                        } else {
                            wr32(e, rd32(freel));
                            wr32(freel, 0);
                            freel = rd32(freel + 4);
                        }
                        memset(rt_mem_base + rd32(e), 0, (size_t)((rd32(D + 0x68) & 0xfffffff) << 4));
                    }
                }

                /* Ligne source « one_row » : lecture par la vtable de la source (slot 1). */
                {
                    uint32_t args[3] = { one_row, 0, buf };
                    gcall(c, rd32(rd32(S) + 4), 1, S, args, 3);
                }

                /* Accumulation : pour chaque colonne source, poids horizontaux (table 1) × verticaux (table 2). */
                if (mem1 + 4 < mem1 + len1) {
                    uint32_t g = mem1 + 4;
                    uint32_t gi = 0;
                    do {
                        uint32_t gend = g + rd32(g);
                        uint32_t src = buf + (gi << 4);
                        if (q0 < end) {
                            uint32_t l8 = q0;
                            do {
                                uint32_t acc = rd32(rows + rd32(l8) * 12);
                                for (uint32_t b = g + 4; b < gend; b += 8) {
                                    long double pr = (long double)rdf(b + 4) * (long double)rdf(l8 + 4);
                                    uint32_t dst = acc + rd32(b) * 16;
                                    int k;
                                    for (k = 0; k < 4; k++) {
                                        float s = rdf(src + 4 * k);
                                        float cur = rdf(dst + 4 * k);
                                        wrf(dst + 4 * k, (float)((long double)pr * s + (long double)cur));
                                    }
                                }
                                l8 += 8;
                            } while (l8 < end);
                        }
                        gi++;
                        g = gend;
                    } while (g < mem1 + len1);
                }

                /* Fin de groupe : décompte, serrage et écriture des lignes complètes. */
                for (uint32_t q = q0; q < end; q += 8) {
                    uint32_t idx = rd32(q);
                    uint32_t e = rows + idx * 12;
                    uint32_t cnt = rd32(e + 8) - 1;
                    wr32(e + 8, cnt);
                    if (cnt == 0) {
                        uint32_t mode = rd32(S + 8);
                        uint32_t acc = rd32(e);
                        uint32_t W = rd32(D + 0x68);
                        uint32_t t;
                        if (mode == 1 || mode == 2 || mode == 3) {
                            for (t = 0; t < W; t++) {
                                uint32_t px = acc + t * 16;
                                for (int k = 0; k < 4; k++) {
                                    float x = rdf(px + 4 * k);
                                    float y;
                                    if (mode == 1 || (mode == 2 && k == 3)) y = clamp_std(x, one);
                                    else y = clamp_thr(x, one, thr);
                                    wrf(px + 4 * k, y);
                                }
                            }
                        }
                        {
                            uint32_t args[3] = { idx, 0, acc };
                            gcall(c, rd32(rd32(D) + 8), 1, D, args, 3);
                        }
                        wr32(e + 4, freel);
                        freel = e;
                    }
                }

                one_row++;
                p = end;
            }
        }
        hr = 0;
    }

cleanup:
    if (rows) {
        uint32_t three = 3;
        gcall(c, FN_ARRAY_DTOR, 1, rows, &three, 1);
    }
    gfree(c, tab2);
    gfree(c, mem1);
    gfree(c, buf);
    gfree(c, 0);
    return hr;
}

/* Dépile l'adresse de retour comme le RET traduit. */
static void ret_pop(cpu_t *c)
{
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    uint32_t ret = (uint32_t)guest_ld(c, esp, 4);
    cpu_wr(c, 16, 4, esp + 4);
    cpu_wr(c, 644, 4, ret);
}

/* Octets hauts du mot +0x7C des objets écrits par rt_native_dither : indéfinis dans la traduction (restes de pile). */
static int heap_masked(uint32_t a)
{
    for (int i = 0; i < cw_count; i++)
        if (a == cw_addr[i] + 2 || a == cw_addr[i] + 3) return 1;
    return 0;
}

static void check_call(cpu_t *c, uint32_t P)
{
    uint32_t top0, top_t, top_n;
    uint32_t bins0[32], bins_t[32], bins_n[32];
    rt_heap_get_state(&top0, bins0);
    size_t len0 = top0 - RT_HEAP_BASE;
    uint8_t *snap = malloc(len0);
    memcpy(snap, rt_mem_base + RT_HEAP_BASE, len0);
    cpu_t cs = *c;

    if (!sub_004dacc6) {
        fprintf(stderr, "rt: CT3_FILTER_CHECK : traduction absente de ce programme\n");
        exit(7);
    }
    {
        /* Référence : traduction pure (aucun remplacement natif pendant son exécution). */
        int saved = rt_native_filter_on;
        rt_native_filter_on = 0;
        sub_004dacc6(c);
        rt_native_filter_on = saved;
    }
    cpu_t ct = *c;
    rt_heap_get_state(&top_t, bins_t);
    size_t lent = top_t - RT_HEAP_BASE;
    uint8_t *memt = malloc(lent);
    memcpy(memt, rt_mem_base + RT_HEAP_BASE, lent);

    memcpy(rt_mem_base + RT_HEAP_BASE, snap, len0);
    rt_heap_set_state(top0, bins0);
    *c = cs;
    cw_count = 0;
    cw_track = 1;
    {
        uint32_t hr = filter_body(c, P);
        cpu_wr(c, 0, 4, hr);
        ret_pop(c);
    }
    cw_track = 0;
    rt_heap_get_state(&top_n, bins_n);

    static const unsigned regs[] = { 0, 12, 16, 20, 24, 28 };
    static const char *rnames[] = { "EAX", "EBX", "ESP", "EBP", "ESI", "EDI" };
    int bad = 0;
    if (top_n != top_t) {
        fprintf(stderr, "rt: CT3_FILTER_CHECK : sommet du tas %08x (natif) != %08x (traduit)\n", top_n, top_t);
        bad = 1;
    }
    if (memcmp(bins_n, bins_t, sizeof bins_n)) {
        fprintf(stderr, "rt: CT3_FILTER_CHECK : listes libres du tas différentes\n");
        bad = 1;
    }
    if (!bad) {
        size_t k;
        for (k = 0; k < lent; k++) {
            if (rt_mem_base[RT_HEAP_BASE + k] == memt[k] || heap_masked(RT_HEAP_BASE + k)) continue;
            fprintf(stderr, "rt: CT3_FILTER_CHECK : contenu du tas différent à 0x%08x (natif %02x, traduit %02x)\n",
                    (unsigned)(RT_HEAP_BASE + k), rt_mem_base[RT_HEAP_BASE + k], memt[k]);
            bad = 1;
            break;
        }
    }
    for (unsigned r = 0; r < 6; r++) {
        uint32_t vn = (uint32_t)cpu_rd(c, regs[r], 4), vt = (uint32_t)cpu_rd(&ct, regs[r], 4);
        if (vn != vt) {
            fprintf(stderr, "rt: CT3_FILTER_CHECK : %s natif %08x, traduit %08x\n", rnames[r], vn, vt);
            bad = 1;
        }
    }
    if (bad) {
        fprintf(stderr, "rt: CT3_FILTER_CHECK : arrêt (appel %ld, objet 0x%08x)\n", 1L, P);
        exit(7);
    }
    fprintf(stderr, "rt: CT3_FILTER_CHECK : appel identique (objet 0x%08x, mode %u, %ux%u -> %ux%u, tas %u octets, tramage natif cumulé %ld, DXT3 natif cumulé %ld)\n",
            P, rd32(rd32(P) + 8), rd32(rd32(P) + 0x68), rd32(rd32(P) + 0x6C), rd32(rd32(P + 4) + 0x68),
            rd32(rd32(P + 4) + 0x6C), (unsigned)lent, dither_calls, dxt3_calls);
    fprintf(stderr, "rt: CT3_FILTER_CHECK : fourcc S=%08x D=%08x\n", rd32(rd32(P) + 4), rd32(rd32(P + 4) + 4));
    fprintf(stderr, "rt: CT3_FILTER_CHECK : S vt+4=%08x S+10=%u S+54=%u S+5c=%u | D vt+8=%08x D+10=%u D+54=%u D+5c=%u\n",
            rd32(rd32(rd32(P)) + 4), rd32(rd32(P) + 0x10), rd32(rd32(P) + 0x54), rd32(rd32(P) + 0x5C),
            rd32(rd32(rd32(P + 4)) + 8), rd32(rd32(P + 4) + 0x10), rd32(rd32(P + 4) + 0x54), rd32(rd32(P + 4) + 0x5C));
    /* Identique : on garde l'état traduit (même que le natif). */
    memcpy(rt_mem_base + RT_HEAP_BASE, memt, lent);
    rt_heap_set_state(top_t, bins_t);
    *c = ct;
    free(snap);
    free(memt);
}

/* Décodeur DXT1 (FUN_004F096A, stdcall, RET 8) : bloc de 8 octets -> 16 texels float4.
 * Couleurs : c0 (palette 0), c1 (palette 1) ; si c1 < c0, palette 2 et 3 par interpolation (1/3, 2/3) ;
 * sinon moyenne (palette 2) et transparent noir (palette 3). Les écarts de bleu et d'alpha sont des
 * variables float dans la traduction (arrondis) ; les autres intermédiaires restent en long double. */
static void expand565(uint32_t v, float *o, float k5, float k6)
{
    o[0] = (float)((v >> 11) & 0x1f) * k5;
    o[1] = (float)((v >> 5) & 0x3f) * k6;
    o[2] = (float)(v & 0x1f) * k5;
    o[3] = 1.0f;
}

static void dxt1_decode(uint32_t dst, uint32_t src)
{
    float k5 = rdf(0x005D646C), k6 = rdf(0x005D6464), k1 = rdf(0x005A70FC), k2 = rdf(0x005DD5EC),
          k3 = rdf(0x0053CC68);
    uint16_t c0 = (uint16_t)rd32(src) & 0xffff, c1 = (uint16_t)(rd32(src) >> 16);
    uint32_t w = rd32(src + 4);
    float p[4][4];
    float out[16][4];
    int t, k;

    expand565(c0, p[0], k5, k6);
    expand565(c1, p[1], k5, k6);
    if (c1 < c0) {
        float db = (float)((long double)p[1][2] - p[0][2]);
        float da = (float)((long double)p[1][3] - p[0][3]);
        for (k = 0; k < 2; k++) {
            float kk = k == 0 ? k1 : k2;
            float *q = p[2 + k];
            q[0] = (float)(((long double)p[1][0] - p[0][0]) * kk + p[0][0]);
            q[1] = (float)(((long double)p[1][1] - p[0][1]) * kk + p[0][1]);
            q[2] = (float)((long double)db * kk + p[0][2]);
            q[3] = (float)((long double)da * kk + p[0][3]);
        }
    } else {
        for (k = 0; k < 4; k++) {
            p[2][k] = (float)(((long double)p[1][k] - p[0][k]) * k3 + p[0][k]);
        }
        p[3][0] = p[3][1] = p[3][2] = p[3][3] = 0.0f;
    }
    for (t = 0; t < 16; t++) {
        unsigned idx = (w >> (2 * t)) & 3u;
        memcpy(out[t], p[idx], sizeof out[t]);
    }
    memcpy(rt_mem_base + dst, out, sizeof out);
}

/* Point d'entrée traduit : args (dst, src) au sommet de la pile, RET 8. */
void rt_native_dxt1(cpu_t *c)
{
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    uint32_t dst = (uint32_t)guest_ld(c, esp + 4, 4), src = (uint32_t)guest_ld(c, esp + 8, 4);
    dxt1_decode(dst, src);
    cpu_wr(c, 0, 4, 0);
    {
        uint32_t ret = (uint32_t)guest_ld(c, esp, 4);
        cpu_wr(c, 16, 4, esp + 4 + 8);
        cpu_wr(c, 644, 4, ret);
    }
}

/* Écriture par tramage à diffusion d'erreur (FUN_004DDB6F, slot 2 du record 0x5D64B0 : surface X8R8G8B8,
 * construite par FUN_004E16E6). Opérations d'après l'asm : les erreurs restent en long double, un arrondi float
 * au seul FSTP ; ROUND() de la décompilation est un FISTP sous RC=11 (troncature), d'où fist_trunc.
 * Le mot de contrôle x87 est restauré en fin de routine : il n'est jamais modifié ici. */
extern void sub_004ddb6f(cpu_t *c) __attribute__((weak));

static int32_t fist_trunc(long double v)
{
    if (!(v > -2147483649.0L && v < 2147483648.0L)) return INT32_MIN;
    return (int32_t)v;
}

static uint32_t clamp255(int32_t v)
{
    if (v < 1) return 0;
    if (v > 254) return 255;
    return (uint32_t)v;
}

/* FUN_004DD45C : buf[t] += err[t+1] (float4), puis remise à zéro du tampon d'erreur (W+2 texels). */
static void dither_prop(uint32_t self, uint32_t buf)
{
    uint32_t W = rd32(self + 0x68), err = rd32(self + 0x5C);
    for (uint32_t t = 0; t < W; t++) {
        uint32_t e = err + 16 * (t + 1), b = buf + 16 * t;
        for (int k = 0; k < 4; k++) wrf(b + 4 * k, (float)((long double)rdf(e + 4 * k) + rdf(b + 4 * k)));
    }
    memset(rt_mem_base + err, 0, (size_t)(((W + 2) & 0xfffffff) << 4));
}

/* FUN_004DDB6F (thiscall, RET 0xC) : args p2 (ligne ou passe, bit 0 = sens), p3 (ligne), buf (float4 × W). */
void rt_native_dither(cpu_t *c)
{
    uint32_t self = (uint32_t)cpu_rd(c, 4, 4);
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    uint32_t ret = (uint32_t)guest_ld(c, esp, 4);
    uint32_t p2 = (uint32_t)guest_ld(c, esp + 4, 4), p3 = (uint32_t)guest_ld(c, esp + 8, 4);
    uint32_t buf = (uint32_t)guest_ld(c, esp + 12, 4);
    uint32_t ocw = (uint32_t)cpu_rd(c, 4256, 2);

    if (!dither_native_on || rd32(self + 0x54) || rd32(self + 0x10)) {
        sub_004ddb6f(c);
        return;
    }
    dither_calls++;
    if (cw_track && cw_count < 16) cw_addr[cw_count++] = self + 0x7C;
    wr32(self + 0x7C, ocw);
    if (rd32(self + 0x5C)) dither_prop(self, buf);

    {
        uint32_t W = rd32(self + 0x68);
        if (W) {
            uint32_t err = rd32(self + 0x5C);
            int dir = (p2 & 1) ? -1 : 1;
            int start = (p2 & 1) ? (int)W - 1 : 0;
            uint32_t wpos = rd32(self + 0x60) * p2 + (W - 1) * (p2 & 1) * 4 + rd32(self + 0x64) * p3 + rd32(self + 0x20);
            uint32_t mat = rd32(self + 0x34) + ((p3 & 3) + (p2 & 3) * 8) * 4;
            long double k255 = rdf(0x0053CC08), k1 = rdf(0x0053CB68);
            long double c3 = rdf(0x005D6484), c5 = rdf(0x005D6480), c1 = rdf(0x00541854);
            double d7;
            float carry[3] = { 0.0f, 0.0f, 0.0f };
            long double c7;
            memcpy(&d7, rt_mem_base + 0x005D6478, 8);
            c7 = d7;

            for (uint32_t t = 0; t < W; t++) {
                int idx = start + dir * (int)t;
                uint32_t pix = buf + (uint32_t)(idx * 16);
                float f1 = rdf(mat + 4 * (t & 3));
                float v[3];
                int32_t iv[3];
                int k;

                for (k = 0; k < 3; k++) {
                    v[k] = (float)(((long double)carry[k] + rdf(pix + 4 * k)) * k255);
                    iv[k] = fist_trunc((long double)(float)((long double)v[k] + f1));
                }
                if (err) {
                    long double e[3];
                    for (k = 0; k < 3; k++) {
                        uint32_t base = err + 0x10 + 4 * k;
                        uint32_t am = base + (uint32_t)((idx - dir) * 16);
                        uint32_t a0 = base + (uint32_t)(idx * 16);
                        uint32_t ap = base + (uint32_t)((idx + dir) * 16);
                        e[k] = ((long double)v[k] - (long double)iv[k]) * k1;
                        wrf(am, (float)(c3 * e[k] + (long double)rdf(am)));
                        wrf(a0, (float)(c5 * e[k] + (long double)rdf(a0)));
                        wrf(ap, (float)(c1 * e[k] + (long double)rdf(ap)));
                        carry[k] = (float)(e[k] * c7);
                    }
                }
                wr32(wpos, (clamp255(iv[0]) << 16) | (clamp255(iv[1]) << 8) | clamp255(iv[2]));
                wpos += (uint32_t)(dir * 4);
            }
        }
    }

    cpu_wr(c, 0, 4, ocw);
    cpu_wr(c, 4, 4, self);
    cpu_wr(c, 16, 4, esp + 16);
    cpu_wr(c, 644, 4, ret);
}

/* Encodeur DXT3 (FUN_004F0D8B, stdcall, RET 0xC) : bloc de 16 octets = alpha (8 octets, 16 nibbles) puis couleur.
 * Alpha : s = (float)(v×15 + 0.5) tronqué (RC=11) ; si dither, v reçoit l'erreur de la diffusion (lignes de 4
 * texels, poids 7/16, 3/16, 5/16, 1/16), et l'erreur e = v - n×(1/15) reste en extended jusqu'au FSTP.
 * Couleur : FUN_004F0019 traduite, appelée avec EAX = source (comme la traduction). */
extern void sub_004f0d8b(cpu_t *c) __attribute__((weak));

static void dxt3_alpha(uint32_t out, uint32_t src, uint32_t dither)
{
    float err[16];
    float k15 = rdf(0x0054169C), half = rdf(0x0053CC68), two32 = rdf(0x0053B7A0), k1s = rdf(0x005A70A4);
    float c7 = rdf(0x005DD60C), c3 = rdf(0x005D6484), c5 = rdf(0x005D6480), c1 = rdf(0x00541854);
    uint32_t t;

    wr32(out, 0);
    wr32(out + 4, 0);
    memset(err, 0, sizeof err);
    for (t = 0; t < 16; t++) {
        float v = rdf(src + 0xC + 16 * t);
        float s;
        int32_t n;
        uint32_t a = out + (t >> 3) * 4;
        int j = (int)(t & 3);

        if (dither) v = (float)((long double)v + err[t]);
        s = (float)((long double)v * k15 + half);
        n = fist_trunc((long double)s);
        wr32(a, (rd32(a) >> 4) | ((uint32_t)n << 28));
        if (dither) {
            long double fn = (long double)n, e;
            if (n < 0) fn = fn + two32;
            e = (long double)v - fn * k1s;
            if (j != 3) err[t + 1] = (float)((long double)c7 * e + err[t + 1]);
            if (t < 12) {
                if (j != 0) err[t + 3] = (float)((long double)c3 * e + err[t + 3]);
                err[t + 4] = (float)((long double)c5 * e + err[t + 4]);
                if (j != 3) err[t + 5] = (float)((long double)c1 * e + err[t + 5]);
            }
        }
    }
}

void rt_native_dxt3(cpu_t *c)
{
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    uint32_t out = (uint32_t)guest_ld(c, esp + 4, 4), src = (uint32_t)guest_ld(c, esp + 8, 4);
    uint32_t dither = (uint32_t)guest_ld(c, esp + 12, 4), ret = (uint32_t)guest_ld(c, esp, 4);

    if (!dxt3_native_on) {
        sub_004f0d8b(c);
        return;
    }
    dxt3_calls++;
    dxt3_alpha(out, src, dither);
    {
        uint32_t args[3] = { out + 8, 0, dither };
        cpu_wr(c, 0, 4, src);
        gcall(c, 0x004F0019u, 0, 0, args, 3);
    }
    cpu_wr(c, 16, 4, esp + 16);
    cpu_wr(c, 644, 4, ret);
}

void rt_native_filter(cpu_t *c)
{
    uint32_t P = (uint32_t)cpu_rd(c, 4, 4);
    /* Seul le mode de serrage 1 est porté (vérifié bit à bit) ; les autres modes restent sur la traduction. */
    if (rd32(rd32(P) + 8) != 1) {
        sub_004dacc6(c);
        return;
    }
    if (check_left > 0 && (!check_dither_only || rd32(rd32(rd32(P + 4)) + 8) == 0x004DDB6Fu)) {
        check_left--;
        check_call(c, P);
        return;
    }
    {
        uint32_t hr = filter_body(c, P);
        cpu_wr(c, 0, 4, hr);
    }
    ret_pop(c);
}

__attribute__((constructor)) static void rt_filter_init(void)
{
    const char *ck = getenv("CT3_FILTER_CHECK");
    const char *nat = getenv("CT3_FILTER_NATIVE");
    /* Activée par défaut ; CT3_FILTER_NATIVE=0 revient à la traduction. */
    rt_native_filter_on = !(nat && strcmp(nat, "0") == 0);
    check_left = ck ? atol(ck) : 0;
    check_dither_only = getenv("CT3_FILTER_CHECK_DITHER") != NULL;
    dither_native_on = !(getenv("CT3_DITHER_NATIVE") && strcmp(getenv("CT3_DITHER_NATIVE"), "0") == 0);
    dxt3_native_on = !(getenv("CT3_DXT3_NATIVE") && strcmp(getenv("CT3_DXT3_NATIVE"), "0") == 0);
}
