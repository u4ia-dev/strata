/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA filter (docs/ARCHITECTURE.md §9), one a voice, stereo, float.
 * LP / HP / BP 12 and 24 dB, NOTCH, PEAK: the trapezoidal state-variable filter (zero-delay feedback, after
 * Zavalishin's and Simper's derivations): tuned to Nyquist, stable under any modulation; 24 dB is two in series,
 * the first at a fourth-order Butterworth's Q and the resonance on the second. LADDER: four trapezoidal
 * one-poles in a loop, the feedback through a soft clip (bounded at any resonance: it can sing), the passband's
 * loss partly made up. FORMANT: three band-passes on the formants of a vowel, CUTOFF moving through A E I O U.
 * Coefficients come from the cutoff once a block (the prewarp tan) and ramp linearly across it. DRIVE: gain into
 * a soft clip, partly made up. VAR: the stereo spread of the cutoff (left down, right up, to an octave each) or,
 * for FORMANT, the formants' shift. MIX: dry / wet. The states are flushed to 0 under 1e-15 and held under +-64
 * at each block's end: no denormals, never a NaN. */
enum { FT_LP12, FT_LP24, FT_HP12, FT_HP24, FT_BP12, FT_BP24, FT_NOTCH, FT_PEAK, FT_LADDER, FT_FORMANT, FT_COUNT };
static const char *const FT_NAMES[FT_COUNT] = {"LP 12", "LP 24", "HP 12", "HP 24", "BP 12", "BP 24", "NOTCH",
                                               "PEAK", "LADDER", "FORMANT"};
#define FLT_GMAX 10.0f                     /* tan(pi 0.468): the cutoff stays under 0.47 fs */

typedef struct {
    float s[2][6];                         /* states [channel]: 2 per SVF stage or band; the ladder's 4 */
    float c[2][3][4];                      /* coefficients at the end of the last block [channel][stage / band] */
    float fg[3];                           /* FORMANT: the bands' gains at the end of the last block */
    uint8_t type, fresh;
} flt_t;

typedef struct {                           /* a block of the filter */
    uint8_t type;
    float hz;                              /* cutoff (modulated, key tracked) */
    float res, drive, var, mix;            /* 0..1 */
} flt_par_t;

/* vowel formants for FORMANT: Hz and gain of the first three, A E I O U (gen_wavetables.py's VOWELS) */
static const float FLT_VOW[5][3][2] = {
    {{600.0f, 1.0f}, {1040.0f, 0.45f}, {2250.0f, 0.35f}},
    {{400.0f, 1.0f}, {1620.0f, 0.25f}, {2400.0f, 0.35f}},
    {{250.0f, 1.0f}, {1750.0f, 0.03f}, {2600.0f, 0.16f}},
    {{400.0f, 1.0f}, {750.0f, 0.28f}, {2400.0f, 0.09f}},
    {{350.0f, 1.0f}, {600.0f, 0.10f}, {2400.0f, 0.03f}},
};

static inline float flt_g(float hz) { return sy_minf(sy_tanpi(sy_minf(hz, 0.468f * SY_FS) * (1.0f / SY_FS)), FLT_GMAX); }

/* the SVF's a1 a2 a3 and k */
static void flt_svf_coef(float hz, float k, float *c)
{
    float g = flt_g(hz), a1 = 1.0f / (1.0f + g * (g + k));
    c[0] = a1;
    c[1] = g * a1;
    c[2] = g * g * a1;
    c[3] = k;
}

/* resonance 0..1 -> k: kmax (flat) .. 0.02 (+34 dB at the cutoff), even in dB */
static inline float flt_k(float res, float kmax) { return kmax * sy_exp2f(-sy_log2f(kmax * 50.0f) * res); }

/* one SVF stage over a block of one channel, in place, the state st (ic1 ic2), the coefficients ramping c0 -> c1.
 * Each response is a loop of its own, from v0 (the input), v1 (the band), v2 (the low) and k: LP v2, BP k v1 (unity
 * at the centre), HP v0 - k v1 - v2, NOTCH v0 - k v1, PEAK v0 + 3 k v1 (+12 dB). (Both channels in one loop would
 * share the ramp, but it overflows the target's 16 registers and is no faster there: tools/costprobe.) The
 * coefficients a1 a2 a3 are each bounded (0..1, 0..0.5, 0..1), so a ramp from one filter to another passes only
 * through tame ones, even with the cutoff jumping 20 Hz -> 20 kHz in a block (ramping k + g, or g, would not) */
#define FLT_SVF_KERNEL(NAME, OUT)                                                                         \
    static void NAME(float *restrict x, float *st, const float *c0, const float *c1)                     \
    {                                                                                                     \
        float ic1 = st[0], ic2 = st[1], a1 = c0[0], a2 = c0[1], a3 = c0[2], k = c0[3];                    \
        const float inv = 1.0f / (float)SY_BLK;                                                           \
        float d1 = (c1[0] - a1) * inv, d2 = (c1[1] - a2) * inv, d3 = (c1[2] - a3) * inv;                  \
        float dk = (c1[3] - k) * inv;                                                                     \
        uint32_t i;                                                                                       \
        for (i = 0; i < SY_BLK; i++) {                                                                    \
            float v0 = x[i], v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;       \
            ic1 = 2.0f * v1 - ic1;                                                                        \
            ic2 = 2.0f * v2 - ic2;                                                                        \
            x[i] = OUT;                                                                                   \
            a1 += d1;                                                                                     \
            a2 += d2;                                                                                     \
            a3 += d3;                                                                                     \
            k += dk;                                                                                      \
        }                                                                                                 \
        (void)k;                                                                                          \
        st[0] = ic1;                                                                                      \
        st[1] = ic2;                                                                                      \
    }
FLT_SVF_KERNEL(flt_svf_lp, v2)
FLT_SVF_KERNEL(flt_svf_bp, k * v1)
FLT_SVF_KERNEL(flt_svf_hp, v0 - k * v1 - v2)
FLT_SVF_KERNEL(flt_svf_notch, v0 - k * v1)
FLT_SVF_KERNEL(flt_svf_peak, v0 + 3.0f * k * v1)

typedef void (*flt_svf_t)(float *restrict, float *, const float *, const float *);
enum { FR_LP, FR_BP, FR_HP, FR_NOTCH, FR_PEAK };
static const flt_svf_t FLT_SVF[5] = {flt_svf_lp, flt_svf_bp, flt_svf_hp, flt_svf_notch, flt_svf_peak};
static const uint8_t FLT_RESP[FT_LADDER] = {FR_LP, FR_LP, FR_HP, FR_HP, FR_BP, FR_BP, FR_NOTCH, FR_PEAK};

/* the ladder over a block of one channel: g (the prewarped cutoff) and k (feedback 0..4) ramp. Two divisions a
 * sample (G, and the solve). Newton steps in their place were tried: no fewer instructions on the target (72
 * against 62 and the two divides, whose time there is not yet measured: the BENCH page) and, at 90 % resonance,
 * -59 dB off exact division in a fast sweep, -18 dB with the cutoff jumping; ramping G instead of g moves a
 * fast-swept ladder by -44 dB. Kept exact */
static void flt_ladder(float *x, float *st, float g0, float g1, float k0, float k1)
{
    float s1 = st[0], s2 = st[1], s3 = st[2], s4 = st[3];
    const float inv = 1.0f / (float)SY_BLK;
    float g = g0, dg = (g1 - g0) * inv, k = k0, dk = (k1 - k0) * inv;
    uint32_t i;
    for (i = 0; i < SY_BLK; i++) {
        float G = g / (1.0f + g), b = 1.0f - G, G2 = G * G, G4 = G2 * G2;
        float S = b * (G2 * G * s1 + G2 * s2 + G * s3 + s4);           /* y4 = G^4 u + S */
        float y4 = (G4 * x[i] + S) / (1.0f + k * G4);                   /* the linear solve; the clip on it */
        float u = x[i] - k * sy_softclip(y4), v, y;
        v = (u - s1) * G; y = v + s1; s1 = y + v;
        v = (y - s2) * G; y = v + s2; s2 = y + v;
        v = (y - s3) * G; y = v + s3; s3 = y + v;
        v = (y - s4) * G; y = v + s4; s4 = y + v;
        x[i] = y * (1.0f + 0.5f * k);
        g += dg;
        k += dk;
    }
    st[0] = s1; st[1] = s2; st[2] = s3; st[3] = s4;
}


static void flt_note(flt_t *f)
{
    memset(f, 0, sizeof *f);
    f->fresh = 1;
}

/* one block, in place on l / r */
static void flt_block(flt_t *f, const flt_par_t *p, float *l, float *r)
{
    float dry[2][SY_BLK], *ch[2] = {l, r}, cn[2][3][4], gn[3] = {0.0f, 0.0f, 0.0f};
    uint32_t c, i, b, t = p->type;
    sy_zerof(cn[0][0], 24u);                       /* (the stages a type leaves unset: 0, not stack garbage) */
    if (t != f->type) {                            /* a new topology: from rest (the voice fades it in) */
        memset(f->s, 0, sizeof f->s);
        f->type = (uint8_t)t;
        f->fresh = 1;
    }
    if (p->mix < 1.0f)
        for (c = 0; c < 2u; c++)
            sy_copyf(dry[c], ch[c], SY_BLK);
    if (p->drive > 0.0f) {                         /* gain 1 .. 16 into the clip; out at 1 / sqrt(gain) x (1 + drive) */
        float g = 1.0f + 15.0f * p->drive * p->drive, mk = sy_exp2f(-0.5f * sy_log2f(g)) * (1.0f + p->drive);
        for (c = 0; c < 2u; c++)
            for (i = 0; i < SY_BLK; i++)
                ch[c][i] = sy_softclip(ch[c][i] * g) * mk;
    }
    if (t == FT_FORMANT) {                         /* CUTOFF 20 Hz .. 20 kHz walks A E I O U; VAR shifts +-1 octave */
        float pos = sy_clampf((sy_log2f(p->hz) - 4.3219f) * (4.0f / 9.9658f), 0.0f, 4.0f), x;
        float sh = sy_exp2f(2.0f * p->var - 1.0f), k = flt_k(p->res, 0.5f);
        uint32_t v = (uint32_t)sy_floori(pos);
        if (v > 3u)
            v = 3u;
        x = pos - (float)v;
        for (b = 0; b < 3u; b++) {
            float hz = FLT_VOW[v][b][0] * sy_exp2f(x * sy_log2f(FLT_VOW[v + 1u][b][0] / FLT_VOW[v][b][0])) * sh;
            flt_svf_coef(hz, k, cn[0][b]);
            gn[b] = 2.0f * (FLT_VOW[v][b][1] + (FLT_VOW[v + 1u][b][1] - FLT_VOW[v][b][1]) * x);
        }
        for (c = 0; c < 2u; c++) {
            float out[SY_BLK];
            sy_zerof(out, SY_BLK);
            for (b = 0; b < 3u; b++) {
                float tmp[SY_BLK], g0 = f->fresh ? gn[b] : f->fg[b], dg = (gn[b] - g0) * (1.0f / (float)SY_BLK);
                sy_copyf(tmp, ch[c], SY_BLK);
                flt_svf_bp(tmp, f->s[c] + 2u * b, f->fresh ? cn[0][b] : f->c[0][b], cn[0][b]);
                for (i = 0; i < SY_BLK; i++, g0 += dg)
                    out[i] += tmp[i] * g0;
            }
            sy_copyf(ch[c], out, SY_BLK);
        }
        sy_copyf(f->c[0][0], cn[0][0], 12u);
        sy_copyf(f->fg, gn, 3u);
    } else if (t == FT_LADDER) {
        for (c = 0; c < 2u; c++) {                 /* VAR: the left channel's cutoff down, the right's up */
            float hz = p->hz * sy_exp2f(c ? p->var : -p->var);
            float (*c0)[4] = f->fresh ? cn[c] : f->c[c];
            cn[c][0][0] = flt_g(hz);
            cn[c][0][3] = 4.0f * p->res;
            flt_ladder(ch[c], f->s[c], c0[0][0], cn[c][0][0], c0[0][3], cn[c][0][3]);
        }
        sy_copyf(f->c[0][0], cn[0][0], 24u);
    } else {                                       /* the SVFs: 24 dB, stage 1 at Q 0.54 and stage 2 at Q 1.31 .. 50 */
        uint32_t ns = (t == FT_LP24 || t == FT_HP24 || t == FT_BP24) ? 2u : 1u, rs = FLT_RESP[t];
        float ka = ns == 2u ? 1.848f : flt_k(p->res, 1.414f), kb = ns == 2u ? flt_k(p->res, 0.765f) : 0.0f;
        float (*c0)[3][4] = f->fresh ? cn : f->c;
        for (c = 0; c < 2u; c++) {                 /* VAR: the left channel's cutoff down, the right's up */
            float hz;
            if (c && p->var == 0.0f) {
                sy_copyf(cn[1][0], cn[0][0], 12u);
                break;
            }
            hz = p->hz * sy_exp2f(c ? p->var : -p->var);
            flt_svf_coef(hz, ka, cn[c][0]);
            if (ns == 2u)
                flt_svf_coef(hz, kb, cn[c][1]);
        }
        for (c = 0; c < 2u; c++)
            for (b = 0; b < ns; b++)
                FLT_SVF[rs](ch[c], f->s[c] + 2u * b, c0[c][b], cn[c][b]);
        sy_copyf(f->c[0][0], cn[0][0], 24u);
    }
    if (p->mix < 1.0f)
        for (c = 0; c < 2u; c++)
            for (i = 0; i < SY_BLK; i++)
                ch[c][i] = dry[c][i] + (ch[c][i] - dry[c][i]) * p->mix;
    for (c = 0; c < 2u; c++)
        for (i = 0; i < 6u; i++) {
            SY_FLUSH(f->s[c][i]);
            f->s[c][i] = sy_clampf(f->s[c][i], -64.0f, 64.0f);
        }
    f->fresh = 0;
}
