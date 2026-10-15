/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA wavetable generator: builds a factory table into an oscillator's frames from its recipe
 * (tools/gen_wavetables.py writes the recipes, names and keyframes as build/gen/strata_wt.h, and the reference
 * renders tests/sy_wt_test.c holds this file to; the math below is that script's, in single precision).
 * A frame's harmonics 1..256 (cosine and sine amplitudes A_k, B_k; no DC) come either straight from its family
 * (spectral) or from the FFT of the cycle rendered at 2048 points, each the mean of 4 sub-samples (time domain).
 * Each level is then the inverse FFT of its harmonics at its own size. A frame is first stored at its own full
 * scale (0.95), and when the last one is done every frame is scaled by its peak / the table's peak: the table's
 * largest sample over all frames and levels is 0.95 of full scale and the frames keep their relative levels.
 * Main loop only (12.5 KiB of pool scratch). One frame a call of wg_step: the UI stays responsive. */
typedef struct {
    uint8_t family, frames, a0, a1;   /* family (WTF_*), frames 1..16, two options */
    uint8_t p[4][2];                  /* four parameters, start and end across the frames, 0..255 */
    uint8_t flags, cat;               /* WT_TAPER; the category (browser) */
    uint16_t key;                     /* KEYS: the first byte of its keyframes in WT_KEYDATA */
} wt_recipe_t;
#include "strata_wt.h"

enum { WTF_ANALOG, WTF_PWM, WTF_ADDITIVE, WTF_BANDS, WTF_FORMANT, WTF_CHAOS, WTF_SYNC, WTF_FM, WTF_FOLD, WTF_PD,
       WTF_DIGITAL, WTF_KEYS };
#define WT_TAPER 1u                   /* the top octave of every level tapered to -12 dB (bright tables) */
#define WG_NH 256u                    /* harmonics at level 0 */
#define WG_NT 2048u                   /* time-domain points a cycle */
#define WG_PEAK (0.95f * 32767.0f)
#define WG_PI 3.14159265f

static float wg_buf[WT_FRAME] SY_POOL;          /* the 2048-point cycle / its FFT, then the frame's nine levels */
static float wg_a[WG_NH], wg_b[WG_NH] SY_POOL;  /* the frame's harmonics */
static float wg_sin[513] SY_POOL;                /* sin(2 pi j / 2048), j = 0 .. 512 (a quarter) */
static struct {
    const wt_recipe_t *r;
    int16_t *dst;                     /* frame 0 of the table being built */
    uint8_t f, n, id;                 /* the next frame, the frames, the table */
    float peak[SY_NFRAME];            /* each frame's peak before its scaling */
} wg;

/* the integer hash of the phases (gen_wavetables.py hash_phase): 0..1023 */
static uint32_t wg_hash(uint32_t k, uint32_t seed)
{
    uint32_t h = (k + 256u * seed) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA77u;
    h ^= h >> 13;
    return h >> 22;
}

static void wg_init(void)
{
    uint32_t j;
    for (j = 0; j <= 512u; j++)
        wg_sin[j] = sy_sin_q((float)j * (1.0f / 2048.0f));
}

/* cos and sin of 2 pi j / 2048 */
static void wg_cs(uint32_t j, float *c, float *s)
{
    uint32_t r = j & 511u;
    switch ((j >> 9) & 3u) {
    case 0: *c = wg_sin[512u - r]; *s = wg_sin[r]; break;
    case 1: *c = -wg_sin[r]; *s = wg_sin[512u - r]; break;
    case 2: *c = -wg_sin[512u - r]; *s = -wg_sin[r]; break;
    default: *c = wg_sin[r]; *s = -wg_sin[512u - r]; break;
    }
}

/* in-place complex FFT of m points (power of two, <= 1024), re / im interleaved; inv: e^+i, unscaled */
static void wg_fft(float *z, uint32_t m, int inv)
{
    uint32_t i, j, k, len;
    for (i = 1, j = 0; i < m; i++) {                 /* bit reversal */
        uint32_t bit = m >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            float t = z[2u * i]; z[2u * i] = z[2u * j]; z[2u * j] = t;
            t = z[2u * i + 1u]; z[2u * i + 1u] = z[2u * j + 1u]; z[2u * j + 1u] = t;
        }
    }
    for (len = 2; len <= m; len <<= 1) {
        uint32_t h = len >> 1, step = 2048u / len;
        for (k = 0; k < h; k++) {
            float c, s;
            wg_cs(k * step, &c, &s);
            if (!inv)
                s = -s;
            for (i = k; i < m; i += len) {
                float *a = z + 2u * i, *b = z + 2u * (i + h);
                float tr = b[0] * c - b[1] * s, ti = b[0] * s + b[1] * c;
                b[0] = a[0] - tr;
                b[1] = a[1] - ti;
                a[0] += tr;
                a[1] += ti;
            }
        }
    }
}

/* the 2048 points in wg_buf -> harmonics 1..256 in wg_a / wg_b (a 1024-point complex FFT of the points as
 * pairs, then the split: X_k = E_k + e^(-2 pi i k / 2048) O_k) */
static void wg_forward(void)
{
    uint32_t k;
    wg_fft(wg_buf, WG_NT / 2u, 0);
    for (k = 1; k <= WG_NH; k++) {
        const float *zk = wg_buf + 2u * k, *zm = wg_buf + 2u * (WG_NT / 2u - k);
        float er = 0.5f * (zk[0] + zm[0]), ei = 0.5f * (zk[1] - zm[1]);   /* E = (Z_k + conj Z_m) / 2 */
        float dr = zk[0] - zm[0], di = zk[1] + zm[1];                     /* Z_k - conj Z_m */
        float or_ = 0.5f * di, oi = -0.5f * dr;                           /* O = that / 2i */
        float c, s;
        wg_cs(k, &c, &s);                                                 /* W = c - i s */
        wg_a[k - 1u] = (er + or_ * c + oi * s) * (2.0f / (float)WG_NT);
        wg_b[k - 1u] = -(ei + oi * c - or_ * s) * (2.0f / (float)WG_NT);
    }
}

/* level l of the harmonics into out (its WT_N(l) samples and the guard): a half-size complex inverse FFT of
 * the spectrum folded into pairs (Z_k = E_k + i O_k), the pairs are the samples */
static void wg_level(uint32_t l, float *out, uint32_t taper)
{
    uint32_t n = WT_N(l), m = n >> 1, h = WG_NH >> l, k;
    for (k = 0; k < m; k++) {
        float xr = 0.0f, xi = 0.0f, yr = 0.0f, yi = 0.0f, g, c, s;
        uint32_t j = m - k;                           /* X_k and conj X_(m-k) */
        if (k >= 1u && k <= h) {
            g = taper && h >= 4u && k > h / 2u ? 1.0f - 0.75f * ((float)(k - h / 2u) / (float)(h / 2u)) *
                                                   ((float)(k - h / 2u) / (float)(h / 2u)) : 1.0f;
            xr = wg_a[k - 1u] * g;
            xi = -wg_b[k - 1u] * g;
        }
        if (j >= 1u && j <= h) {
            g = taper && h >= 4u && j > h / 2u ? 1.0f - 0.75f * ((float)(j - h / 2u) / (float)(h / 2u)) *
                                                   ((float)(j - h / 2u) / (float)(h / 2u)) : 1.0f;
            yr = wg_a[j - 1u] * g;
            yi = wg_b[j - 1u] * g;                    /* conj */
        }
        {
            float er = 0.5f * (xr + yr), ei = 0.5f * (xi + yi);
            float dr = 0.5f * (xr - yr), di = 0.5f * (xi - yi);
            wg_cs(k * (2048u / n), &c, &s);           /* O = (X_k - conj X_j) / 2 * e^(+2 pi i k / n) */
            out[2u * k] = er - (dr * s + di * c);     /* Z = E + i O */
            out[2u * k + 1u] = ei + (dr * c - di * s);
        }
    }
    wg_fft(out, m, 1);
    out[n] = out[0];
}

/* the parameters of frame f, 0..1 */
static void wg_params(const wt_recipe_t *r, uint32_t f, float *v)
{
    float u = r->frames > 1u ? (float)f / (float)(r->frames - 1u) : 0.0f;
    uint32_t j;
    for (j = 0; j < 4u; j++)
        v[j] = ((float)r->p[j][0] + (float)((int32_t)r->p[j][1] - (int32_t)r->p[j][0]) * u) * (1.0f / 255.0f);
}

static inline float wg_pow(float x, float e) { return sy_exp2f(e * sy_log2f(x)); }

/* the basic shapes into wg_a / wg_b, weighted: 0 sine, 1 triangle, 2 saw (falling), 3 square, 4 pulse */
static void wg_shape(uint32_t i, float w, float wt)
{
    uint32_t k;
    for (k = 1; k <= WG_NH; k++) {
        float fk = (float)k, a = 0.0f, b = 0.0f;
        if (i == 0u)
            b = k == 1u ? 1.0f : 0.0f;
        else if (i == 1u)
            b = k & 1u ? 8.0f / (WG_PI * WG_PI * fk * fk) * (((k - 1u) >> 1) & 1u ? -1.0f : 1.0f) : 0.0f;
        else if (i == 2u)
            b = 2.0f / (WG_PI * fk);
        else if (i == 3u)
            b = k & 1u ? 4.0f / (WG_PI * fk) : 0.0f;
        else {
            a = 2.0f / (WG_PI * fk) * sy_sin2pi(fk * w);
            b = 2.0f / (WG_PI * fk) * (1.0f - sy_cos2pi(fk * w));
        }
        wg_a[k - 1u] += a * wt;
        wg_b[k - 1u] += b * wt;
    }
}

static void wg_bright(float v)                     /* 1 / sqrt(1 + (k / kc)^4), kc = 2^(1 + 8 v) */
{
    float kc = sy_exp2f(1.0f + 8.0f * v);
    uint32_t k;
    for (k = 1; k <= WG_NH; k++) {
        float q = (float)k / kc, g;
        q *= q;
        g = sy_exp2f(-0.5f * sy_log2f(1.0f + q * q));
        wg_a[k - 1u] *= g;
        wg_b[k - 1u] *= g;
    }
}

/* amplitudes in wg_b -> (A, B) by the phase mode: 0 sine, 1 alternating sign, 2 hashed */
static void wg_phases(uint32_t mode, uint32_t seed)
{
    uint32_t k;
    for (k = 1; k <= WG_NH; k++) {
        float a = wg_b[k - 1u];
        if (mode == 1u) {
            wg_b[k - 1u] = k & 1u ? a : -a;
        } else if (mode == 2u) {
            float ph = (float)wg_hash(k, seed) * (1.0f / 1024.0f);
            wg_a[k - 1u] = a * sy_sin2pi(ph);
            wg_b[k - 1u] = a * sy_cos2pi(ph);
        }
    }
}

/* typical adult vowel formants: (Hz, bandwidth Hz, gain dB) x 5 for A E I O U (gen_wavetables.py VOWELS) */
static const int16_t WG_VOWEL[5][5][3] = {
    {{600, 60, 0}, {1040, 70, -7}, {2250, 110, -9}, {2450, 120, -9}, {2750, 130, -20}},
    {{400, 40, 0}, {1620, 80, -12}, {2400, 100, -9}, {2800, 120, -12}, {3100, 120, -18}},
    {{250, 60, 0}, {1750, 90, -30}, {2600, 100, -16}, {3050, 120, -22}, {3340, 120, -28}},
    {{400, 40, 0}, {750, 80, -11}, {2400, 100, -21}, {2600, 120, -20}, {2900, 120, -40}},
    {{350, 40, 0}, {600, 80, -20}, {2400, 100, -32}, {2675, 120, -28}, {2950, 120, -36}},
};

static void wg_chaos_key(uint32_t seed, uint32_t kf, float dens, float beta, float *m, float *p)
{
    uint32_t s = ((seed + 1u) << 16) ^ (kf * 0x9E3779B9u) ^ 0x1234567u, k;
    for (k = 0; k < WG_NH; k++) {
        uint32_t r1 = sy_rand(&s), r2 = sy_rand(&s), r3 = sy_rand(&s);
        m[k] = (float)(r1 >> 8) * (1.0f / 16777216.0f) < dens || k == 0u
            ? wg_pow((float)(k + 1u), -beta) * (0.25f + 0.75f * (float)(r2 >> 8) * (1.0f / 16777216.0f)) : 0.0f;
        p[k] = (float)(r3 >> 22) * (1.0f / 1024.0f);
    }
}

/* the spectral families: the harmonics of frame f */
static void wg_spectral(const wt_recipe_t *r, uint32_t f)
{
    float v[4];
    uint32_t k, j;
    wg_params(r, f, v);
    for (k = 0; k < WG_NH; k++)
        wg_a[k] = wg_b[k] = 0.0f;
    switch (r->family) {
    case WTF_ANALOG: {
        float s = 4.0f * v[0], x, w = 0.5f - 0.48f * v[2];
        uint32_t i = (uint32_t)sy_floori(s);
        if (i > 3u)
            i = 3u;
        x = s - (float)i;
        wg_shape(i, w, 1.0f - x);
        wg_shape(i + 1u, w, x);
        wg_bright(v[1]);
        break;
    }
    case WTF_PWM:
        wg_shape(4, 0.5f - 0.48f * v[0], 1.0f);
        wg_bright(v[1]);
        break;
    case WTF_ADDITIVE: {
        float beta = 0.5f + 2.0f * v[0], godd = sy_minf(1.0f, 2.0f * (1.0f - v[1])), gev = sy_minf(1.0f, 2.0f * v[1]);
        float per = 2.0f + 14.0f * v[3];
        for (k = 1; k <= WG_NH; k++) {
            float a = k == 1u ? 1.0f : wg_pow((float)k, -beta) * (k & 1u ? godd : gev);
            wg_b[k - 1u] = a * (1.0f - v[2] * 0.5f * (1.0f + sy_cos2pi((float)k / per)));
        }
        wg_phases(r->a1, r->a0);
        break;
    }
    case WTF_BANDS: {
        float c = 8.0f * v[0], sg = 0.15f + 1.35f * v[1], d = 2.0f * v[3], q = -1.442695041f / (2.0f * sg * sg);
        for (k = 1; k <= WG_NH; k++) {
            float lk = sy_log2f((float)k), u0 = lk - (c - 0.5f * d), u1 = lk - (c + 0.5f * d);
            wg_b[k - 1u] = sy_exp2f(q * u0 * u0) + sy_exp2f(q * u1 * u1) + (k == 1u ? v[2] : 0.0f);
        }
        wg_phases(r->a1, r->a0);
        break;
    }
    case WTF_FORMANT: {
        float p = 4.0f * v[0], x, sh = sy_exp2f(2.0f * v[1] - 1.0f), tilt = -(2.0f - v[2]);
        uint32_t i = (uint32_t)sy_floori(p);
        if (i > 3u)
            i = 3u;
        x = p - (float)i;
        for (k = 1; k <= WG_NH; k++)
            wg_b[k - 1u] = 0.0f;
        for (j = 0; j < 5u; j++) {
            const int16_t *a = WG_VOWEL[i][j], *b = WG_VOWEL[i + 1u][j];
            float fr = (float)a[0] * sy_exp2f(x * sy_log2f((float)b[0] / (float)a[0])) * sh;
            float hb = 0.5f * ((float)a[1] + (float)(b[1] - a[1]) * x) * sh * 2.5f;
            float g = sy_exp2f(((float)a[2] + (float)(b[2] - a[2]) * x) * 0.16609640474f);
            for (k = 1; k <= WG_NH; k++) {
                float u = ((float)k * 110.0f - fr) / hb;
                wg_b[k - 1u] += g / (1.0f + u * u);
            }
        }
        for (k = 1; k <= WG_NH; k++)
            wg_b[k - 1u] *= wg_pow((float)k, tilt);
        break;
    }
    case WTF_CHAOS: {
        float u = 3.0f * v[0], x, *m0 = wg_buf, *p0 = wg_buf + WG_NH, *m1 = wg_buf + 2u * WG_NH,
              *p1 = wg_buf + 3u * WG_NH;
        float dens = 0.1f + 0.9f * v[2], beta = 0.5f + 2.0f * v[1];
        uint32_t i = (uint32_t)sy_floori(u);
        if (i > 2u)
            i = 2u;
        x = u - (float)i;
        wg_chaos_key(r->a0, i, dens, beta, m0, p0);
        wg_chaos_key(r->a0, i + 1u, dens, beta, m1, p1);
        for (k = 0; k < WG_NH; k++) {
            float a = m0[k] + (m1[k] - m0[k]) * x, d = p1[k] - p0[k], ph;
            d -= (float)sy_floori(d + 0.5f);
            ph = p0[k] + d * x;
            wg_a[k] = a * sy_sin2pi(ph);
            wg_b[k] = a * sy_cos2pi(ph);
        }
        break;
    }
    case WTF_KEYS: {
        uint32_t nk = r->a0, i;
        float u = (r->frames > 1u ? (float)f / (float)(r->frames - 1u) : 0.0f) * (float)(nk - 1u), x;
        const uint8_t *k0, *k1;
        i = (uint32_t)sy_floori(u);
        if (i > nk - 2u)
            i = nk - 2u;
        x = u - (float)i;
        k0 = WT_KEYDATA + r->key + i * 2u * WG_NH;
        k1 = k0 + 2u * WG_NH;
        for (k = 0; k < WG_NH; k++) {
            float m = (float)k0[k] + (float)((int32_t)k1[k] - (int32_t)k0[k]) * x, a, ph;
            int32_t d = ((int32_t)k1[WG_NH + k] - (int32_t)k0[WG_NH + k] + 384) % 256 - 128;
            a = m > 0.0f ? sy_exp2f(-(255.0f - m) * 0.0830482f) : 0.0f;
            ph = ((float)k0[WG_NH + k] + (float)d * x) * (1.0f / 256.0f);
            wg_a[k] = a * sy_sin2pi(ph);
            wg_b[k] = a * sy_cos2pi(ph);
        }
        break;
    }
    }
}

/* the sine-phase triangle of a phase in turns 0..1: 0 at 0, +1 at 1/4, -1 at 3/4 */
static inline float wg_tri(float u)
{
    u += 0.75f;
    u -= (float)sy_floori(u);
    return 4.0f * sy_absf(u - 0.5f) - 1.0f;
}

/* the time-domain families: the cycle at 2048 points into wg_buf, each the mean of 4 sub-samples s / 8192 */
static void wg_time(const wt_recipe_t *r, uint32_t f)
{
    float v[4], acc = 0.0f;
    uint32_t s, rq = 0, st = 0;
    float half = 1.0f, fm = 0.0f, gain = 1.0f, bend = 0.5f, wid = 1.0f, ratio = 1.0f;
    wg_params(r, f, v);
    switch (r->family) {
    case WTF_SYNC: rq = (uint32_t)(sy_exp2f(4.0f * v[0]) * 65536.0f + 0.5f); break;
    case WTF_FM: fm = 8.0f * v[0] * (1.0f / (2.0f * WG_PI)); break;
    case WTF_FOLD: gain = 1.0f + 9.0f * v[0]; break;
    case WTF_PD: bend = 0.5f - 0.49f * v[0]; wid = 1.0f - 0.98f * v[0]; ratio = 1.0f + 15.0f * v[0]; break;
    case WTF_DIGITAL:
        st = v[1] > 0.5f / 255.0f ? 4u + (uint32_t)(60.0f * (1.0f - v[1])) : 0u;
        half = 0.5f * sy_exp2f(1.0f + 7.0f * (1.0f - v[0]));
        break;
    }
    for (s = 0; s < WG_NT * 4u; s++) {
        float x = (float)s * (1.0f / 8192.0f), y;
        switch (r->family) {
        case WTF_SYNC: {
            uint32_t ph = (uint32_t)(((uint64_t)(s << 19) * rq) >> 16);
            float xr = (float)ph * (1.0f / 4294967296.0f);
            if (r->a0 == 0u)
                y = 1.0f - 2.0f * xr;
            else if (r->a0 == 1u)
                y = ph < 0x80000000u ? 1.0f : -1.0f;
            else if (r->a0 == 2u)
                y = sy_sin2pi(xr);
            else
                y = 4.0f * sy_absf((float)(ph + 0xC0000000u) * (1.0f / 4294967296.0f) - 0.5f) - 1.0f;
            break;
        }
        case WTF_FM: {
            float c = (float)((r->a0 * s) & 8191u) * (1.0f / 8192.0f);
            float q = (float)((r->a1 * s) & 8191u) * (1.0f / 8192.0f);
            y = sy_sin2pi(c + fm * sy_sin2pi(q));
            break;
        }
        case WTF_FOLD: {
            float z = gain * (r->a0 == 0u ? sy_sin2pi(x) : wg_tri(x)) + v[1];
            if (r->a1 == 0u) {
                y = sy_sin2pi(0.25f * z);
            } else {
                float m = (z + 1.0f) - 4.0f * (float)sy_floori(0.25f * (z + 1.0f));
                y = 1.0f - sy_absf(m - 2.0f);
            }
            break;
        }
        case WTF_PD:
            if (r->a0 == 0u) {
                float phi = x < bend ? x / (2.0f * bend) : 0.5f + (x - bend) / (2.0f * (1.0f - bend));
                y = -sy_cos2pi(phi);
            } else if (r->a0 == 1u) {
                float x2 = 2.0f * x, fl = (float)sy_floori(x2), h = sy_minf(1.0f, (x2 - fl) / wid);
                y = -sy_cos2pi(0.5f * (fl + h));
            } else {
                float win = r->a0 == 2u ? 1.0f - x : r->a0 == 3u ? 1.0f - sy_absf(2.0f * x - 1.0f)
                                                                 : sy_minf(1.0f, 2.0f * (1.0f - x));
                y = win * sy_cos2pi(ratio * x);
            }
            break;
        default: {                                     /* WTF_DIGITAL */
            float xq = st ? (float)((s * st) >> 13) / (float)st : x, b;
            b = r->a0 == 0u ? sy_sin2pi(xq) : r->a0 == 1u ? 1.0f - 2.0f * xq : wg_tri(xq);
            y = (float)sy_floori(b * half + 0.5f) / half;
            break;
        }
        }
        acc += y;
        if ((s & 3u) == 3u) {
            wg_buf[s >> 2] = 0.25f * acc;
            acc = 0.0f;
        }
    }
    wg_forward();
}

static inline int32_t wg_round(float x) { return (int32_t)(x >= 0.0f ? x + 0.5f : x - 0.5f); }

/* starts building factory table id into dst (16 frames' room) */
static void wg_begin(uint32_t id, int16_t *dst)
{
    wg.r = &WT_RECIPES[id < WT_NFACT ? id : 0u];
    wg.id = (uint8_t)(id < WT_NFACT ? id : 0u);
    wg.dst = dst;
    wg.f = 0;
    wg.n = wg.r->frames;
}

/* builds the next frame; 1 = the table is done (the last call also scales every frame to the table's peak) */
static int wg_step(void)
{
    const wt_recipe_t *r = wg.r;
    uint32_t l, i;
    float pk = 0.0f, sc;
    int16_t *q;
    if (wg.f >= wg.n)
        return 1;
    if (r->family >= WTF_SYNC && r->family <= WTF_DIGITAL)
        wg_time(r, wg.f);
    else
        wg_spectral(r, wg.f);
    for (l = 0; l < WT_NLEV; l++)
        wg_level(l, wg_buf + WT_OFF[l], r->flags & WT_TAPER);
    for (i = 0; i < WT_FRAME; i++)
        pk = sy_maxf(pk, sy_absf(wg_buf[i]));
    wg.peak[wg.f] = pk;
    sc = pk > 0.0f ? WG_PEAK / pk : 0.0f;
    q = wg.dst + wg.f * WT_FRAME;
    for (i = 0; i < WT_FRAME; i++)
        q[i] = (int16_t)wg_round(wg_buf[i] * sc);
    if (++wg.f < wg.n)
        return 0;
    for (pk = 0.0f, i = 0; i < wg.n; i++)          /* the table's peak; every frame down to its share */
        pk = sy_maxf(pk, wg.peak[i]);
    for (l = 0; l < wg.n && pk > 0.0f; l++) {
        float k = wg.peak[l] / pk;
        q = wg.dst + l * WT_FRAME;
        for (i = 0; i < WT_FRAME; i++)
            q[i] = (int16_t)wg_round((float)q[i] * k);
    }
    return 1;
}
