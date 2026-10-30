#include "rt_vec.h"

#include <math.h>
#include <string.h>

static int16_t sat16(int32_t v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static int8_t sat8(int32_t v)
{
    if (v > 127) return 127;
    if (v < -128) return -128;
    return (int8_t)v;
}

void rt_v_PACKSSWB(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    int16_t aw[4], bw[4];
    int8_t r[8];
    memcpy(aw, a, 8);
    memcpy(bw, b, 8);
    for (int i = 0; i < 4; i++) {
        r[i] = sat8(aw[i]);
        r[4 + i] = sat8(bw[i]);
    }
    memcpy(out, r, 8);
}

void rt_v_PACKSSDW(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    int32_t ad[2], bd[2];
    int16_t r[4];
    memcpy(ad, a, 8);
    memcpy(bd, b, 8);
    r[0] = sat16(ad[0]);
    r[1] = sat16(ad[1]);
    r[2] = sat16(bd[0]);
    r[3] = sat16(bd[1]);
    memcpy(out, r, 8);
}

void rt_v_PADDSW(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    int16_t aw[4], bw[4], r[4];
    memcpy(aw, a, 8);
    memcpy(bw, b, 8);
    for (int i = 0; i < 4; i++) r[i] = sat16((int32_t)aw[i] + bw[i]);
    memcpy(out, r, 8);
}

void rt_v_PSUBSW(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    int16_t aw[4], bw[4], r[4];
    memcpy(aw, a, 8);
    memcpy(bw, b, 8);
    for (int i = 0; i < 4; i++) r[i] = sat16((int32_t)aw[i] - bw[i]);
    memcpy(out, r, 8);
}

void rt_v_PMADDWD(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    int16_t aw[4], bw[4];
    int32_t r[2];
    memcpy(aw, a, 8);
    memcpy(bw, b, 8);
    /* Somme en 64 bits puis troncature : seul cas de débordement, 0x8000 * 0x8000 + 0x8000 * 0x8000. */
    r[0] = (int32_t)((int64_t)aw[0] * bw[0] + (int64_t)aw[1] * bw[1]);
    r[1] = (int32_t)((int64_t)aw[2] * bw[2] + (int64_t)aw[3] * bw[3]);
    memcpy(out, r, 8);
}

void rt_v_PMULHW(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    int16_t aw[4], bw[4], r[4];
    memcpy(aw, a, 8);
    memcpy(bw, b, 8);
    for (int i = 0; i < 4; i++) r[i] = (int16_t)(((int32_t)aw[i] * bw[i]) >> 16);
    memcpy(out, r, 8);
}

void rt_v_PSLLW(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    uint16_t aw[4], r[4];
    uint64_t count;
    memcpy(aw, a, 8);
    memcpy(&count, b, 8);
    for (int i = 0; i < 4; i++) r[i] = count > 15 ? 0 : (uint16_t)(aw[i] << count);
    memcpy(out, r, 8);
}

void rt_v_PSRAW(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    int16_t aw[4], r[4];
    uint64_t count;
    memcpy(aw, a, 8);
    memcpy(&count, b, 8);
    if (count > 15) count = 15;
    for (int i = 0; i < 4; i++) r[i] = (int16_t)(aw[i] >> count);
    memcpy(out, r, 8);
}

void rt_w_and(uint8_t *out, const uint8_t *a, const uint8_t *b, unsigned n)
{
    for (unsigned i = 0; i < n; i++) out[i] = a[i] & b[i];
}

void rt_w_or(uint8_t *out, const uint8_t *a, const uint8_t *b, unsigned n)
{
    for (unsigned i = 0; i < n; i++) out[i] = a[i] | b[i];
}

void rt_w_xor(uint8_t *out, const uint8_t *a, const uint8_t *b, unsigned n)
{
    for (unsigned i = 0; i < n; i++) out[i] = a[i] ^ b[i];
}

void rt_w_not(uint8_t *out, const uint8_t *a, unsigned n)
{
    for (unsigned i = 0; i < n; i++) out[i] = (uint8_t)~a[i];
}

/* Décalages sur n octets little-endian ; bits peut dépasser la taille (résultat nul). */
void rt_w_shr(uint8_t *out, const uint8_t *a, unsigned n, unsigned bits)
{
    uint8_t tmp[64];
    unsigned bytes = bits / 8, rem = bits % 8;
    for (unsigned i = 0; i < n; i++) {
        unsigned src = i + bytes;
        uint32_t lo = src < n ? a[src] : 0;
        uint32_t hi = src + 1 < n ? a[src + 1] : 0;
        tmp[i] = (uint8_t)(((hi << 8 | lo) >> rem) & 0xFF);
    }
    memcpy(out, tmp, n);
}

void rt_w_shl(uint8_t *out, const uint8_t *a, unsigned n, unsigned bits)
{
    uint8_t tmp[64];
    unsigned bytes = bits / 8, rem = bits % 8;
    for (unsigned i = 0; i < n; i++) {
        uint32_t cur = i >= bytes ? a[i - bytes] : 0;
        uint32_t prev = i >= bytes + 1 ? a[i - bytes - 1] : 0;
        uint32_t carry = rem ? prev >> (8 - rem) : 0;
        tmp[i] = (uint8_t)((cur << rem) | carry);
    }
    memcpy(out, tmp, n);
}

void rt_w_zext(uint8_t *out, const uint8_t *a, unsigned nin, unsigned nout)
{
    uint8_t tmp[64];
    memset(tmp, 0, nout);
    memcpy(tmp, a, nin < nout ? nin : nout);
    memcpy(out, tmp, nout);
}

/* ---- 3DNow! ---- */

static void get_f2(const uint8_t *p, float f[2]) { memcpy(f, p, 8); }
static void put_f2(uint8_t *out, const float f[2]) { memcpy(out, f, 8); }

void rt_v_PFADD(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float x[2], y[2], r[2];
    get_f2(a, x); get_f2(b, y);
    r[0] = x[0] + y[0]; r[1] = x[1] + y[1];
    put_f2(out, r);
}

void rt_v_PFSUB(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float x[2], y[2], r[2];
    get_f2(a, x); get_f2(b, y);
    r[0] = x[0] - y[0]; r[1] = x[1] - y[1];
    put_f2(out, r);
}

void rt_v_PFSUBR(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float x[2], y[2], r[2];
    get_f2(a, x); get_f2(b, y);
    r[0] = y[0] - x[0]; r[1] = y[1] - x[1];
    put_f2(out, r);
}

void rt_v_PFMUL(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float x[2], y[2], r[2];
    get_f2(a, x); get_f2(b, y);
    r[0] = x[0] * y[0]; r[1] = x[1] * y[1];
    put_f2(out, r);
}

void rt_v_PFMIN(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float x[2], y[2], r[2];
    get_f2(a, x); get_f2(b, y);
    for (int i = 0; i < 2; i++) r[i] = x[i] < y[i] ? x[i] : y[i];
    put_f2(out, r);
}

void rt_v_PFMAX(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float x[2], y[2], r[2];
    get_f2(a, x); get_f2(b, y);
    for (int i = 0; i < 2; i++) r[i] = x[i] > y[i] ? x[i] : y[i];
    put_f2(out, r);
}

static void cmp_mask(uint8_t *out, const uint8_t *a, const uint8_t *b, int op)
{
    float x[2], y[2];
    uint32_t r[2];
    get_f2(a, x); get_f2(b, y);
    for (int i = 0; i < 2; i++) {
        int t = op == 0 ? x[i] >= y[i] : op == 1 ? x[i] > y[i] : x[i] == y[i];
        r[i] = t ? 0xFFFFFFFFu : 0;
    }
    memcpy(out, r, 8);
}

void rt_v_PFCMPGE(uint8_t *out, const uint8_t *a, const uint8_t *b) { cmp_mask(out, a, b, 0); }
void rt_v_PFCMPGT(uint8_t *out, const uint8_t *a, const uint8_t *b) { cmp_mask(out, a, b, 1); }
void rt_v_PFCMPEQ(uint8_t *out, const uint8_t *a, const uint8_t *b) { cmp_mask(out, a, b, 2); }

void rt_v_PFACC(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float x[2], y[2], r[2];
    get_f2(a, x); get_f2(b, y);
    r[0] = x[0] + x[1]; r[1] = y[0] + y[1];
    put_f2(out, r);
}

void rt_v_PFNACC(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float x[2], y[2], r[2];
    get_f2(a, x); get_f2(b, y);
    r[0] = x[0] - x[1]; r[1] = y[0] - y[1];
    put_f2(out, r);
}

void rt_v_PSWAPD(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    (void)a;
    uint32_t y[2], r[2];
    memcpy(y, b, 8);
    r[0] = y[1]; r[1] = y[0];
    memcpy(out, r, 8);
}

void rt_v_PI2FD(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    (void)a;
    int32_t y[2];
    float r[2];
    memcpy(y, b, 8);
    r[0] = (float)y[0]; r[1] = (float)y[1];
    put_f2(out, r);
}

static int32_t f2i_sat(float v)
{
    if (v != v) return 0;
    if (v >= 2147483648.0f) return 0x7FFFFFFF;
    if (v < -2147483648.0f) return (int32_t)0x80000000u;
    return (int32_t)v;
}

void rt_v_PF2ID(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    (void)a;
    float y[2];
    int32_t r[2];
    get_f2(b, y);
    r[0] = f2i_sat(y[0]); r[1] = f2i_sat(y[1]);
    memcpy(out, r, 8);
}

void rt_v_PFRCP(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    (void)a;
    float y[2], r[2];
    get_f2(b, y);
    r[0] = r[1] = 1.0f / y[0];
    put_f2(out, r);
}

/* Convention de la référence Unicorn (D-025) : signe(x) / sqrt(|x|), y compris pour x négatif. */
void rt_v_PFRSQRT(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    (void)a;
    float y[2], r[2];
    get_f2(b, y);
    r[0] = r[1] = copysignf(1.0f, y[0]) / sqrtf(fabsf(y[0]));
    put_f2(out, r);
}

/* Newton : x1 = x0 * (2 - d * x0) en deux étapes. a = d (ou t), b = x0. */
void rt_v_PFRCPIT1(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float d[2], x[2], r[2];
    get_f2(a, d); get_f2(b, x);
    r[0] = 2.0f - d[0] * x[0]; r[1] = 2.0f - d[1] * x[1];
    put_f2(out, r);
}

void rt_v_PFRSQIT1(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float d[2], y[2], r[2];
    get_f2(a, d); get_f2(b, y);
    r[0] = 1.5f - 0.5f * d[0] * y[0] * y[0];
    r[1] = 1.5f - 0.5f * d[1] * y[1] * y[1];
    put_f2(out, r);
}

void rt_v_PFRCPIT2(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float t[2], x[2], r[2];
    get_f2(a, t); get_f2(b, x);
    r[0] = t[0] * x[0]; r[1] = t[1] * x[1];
    put_f2(out, r);
}

/* ---- SSE ---- */

void rt_v_RCPPS(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    (void)a;
    float y[4], r[4];
    memcpy(y, b, 16);
    for (int i = 0; i < 4; i++) r[i] = 1.0f / y[i];
    memcpy(out, r, 16);
}

void rt_v_RCPSS(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float x[4], y[4];
    memcpy(x, a, 16);
    memcpy(y, b, 16);
    x[0] = 1.0f / y[0];
    memcpy(out, x, 16);
}

void rt_v_RSQRTSS(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    float x[4], y[4];
    memcpy(x, a, 16);
    memcpy(y, b, 16);
    x[0] = 1.0f / sqrtf(y[0]);
    memcpy(out, x, 16);
}
