/* Phase 0 cost probe: representative DSP kernels for the target listing.
 * Not product code. Compiled with the JieLi clang (pi32v2, -mcpu=r3 -mfprev1)
 * and counted per sample by count_loops.py. */
#include <stdint.h>

#define N 32                                   /* samples per block */

/* ---------------------------------------------------------------- osc */
/* int16 table (one mip level of two adjacent frames), 32-bit phase,
 * linear interpolation inside each frame, crossfade between frames,
 * stereo accumulate with Q15 pan gains. tbits = log2(table length). */
void osc_q15(int32_t *restrict l, int32_t *restrict r, const int16_t *fa, const int16_t *fb,
             uint32_t ph, uint32_t inc, uint32_t tbits, int32_t xf, int32_t gl, int32_t gr, uint32_t *out_ph)
{
    uint32_t sh = 32u - tbits, mask = (1u << tbits) - 1u;
    for (int i = 0; i < N; i++) {
        uint32_t i0 = ph >> sh, i1 = (i0 + 1u) & mask;
        int32_t fr = (int32_t)((ph << tbits) >> 17);            /* Q15 */
        int32_t a = fa[i0] + (((fa[i1] - fa[i0]) * fr) >> 15);
        int32_t b = fb[i0] + (((fb[i1] - fb[i0]) * fr) >> 15);
        int32_t s = a + (((b - a) * xf) >> 15);
        l[i] += (s * gl) >> 15;
        r[i] += (s * gr) >> 15;
        ph += inc;
    }
    *out_ph = ph;
}

/* the same, two samples per pass (branch amortised) */
void osc_q15_x2(int32_t *restrict l, int32_t *restrict r, const int16_t *fa, const int16_t *fb,
                uint32_t ph, uint32_t inc, uint32_t tbits, int32_t xf, int32_t gl, int32_t gr, uint32_t *out_ph)
{
    uint32_t sh = 32u - tbits, mask = (1u << tbits) - 1u;
    for (int i = 0; i < N; i += 2) {
        uint32_t i0 = ph >> sh, i1 = (i0 + 1u) & mask;
        int32_t fr = (int32_t)((ph << tbits) >> 17);
        int32_t a = fa[i0] + (((fa[i1] - fa[i0]) * fr) >> 15);
        int32_t b = fb[i0] + (((fb[i1] - fb[i0]) * fr) >> 15);
        int32_t s = a + (((b - a) * xf) >> 15);
        l[i] += (s * gl) >> 15;
        r[i] += (s * gr) >> 15;
        ph += inc;
        i0 = ph >> sh; i1 = (i0 + 1u) & mask;
        fr = (int32_t)((ph << tbits) >> 17);
        a = fa[i0] + (((fa[i1] - fa[i0]) * fr) >> 15);
        b = fb[i0] + (((fb[i1] - fb[i0]) * fr) >> 15);
        s = a + (((b - a) * xf) >> 15);
        l[i + 1] += (s * gl) >> 15;
        r[i + 1] += (s * gr) >> 15;
        ph += inc;
    }
    *out_ph = ph;
}

/* the same work, written for the register file: pointer walks, the next-sample index from a
 * second phase (no mask), the frame crossfade folded into one multiply per frame */
void osc_q15_tight(int32_t *restrict l, int32_t *restrict r, const int16_t *fa, const int16_t *fb,
                   uint32_t ph, uint32_t inc, uint32_t tbits, int32_t xf, int32_t gl, int32_t gr, uint32_t *out_ph)
{
    const uint32_t sh = 32u - tbits, fsh = tbits;
    int32_t *end = l + N;
    const int32_t ixf = 32768 - xf;
    do {
        uint32_t i0 = ph >> sh;
        int32_t fr = (int32_t)((ph << fsh) >> 17);
        int32_t a0 = (fa[i0] * ixf + fb[i0] * xf) >> 15;          /* frames mixed at both points */
        int32_t a1 = (fa[i0 + 1] * ixf + fb[i0 + 1] * xf) >> 15;   /* tables carry one guard sample */
        int32_t s = a0 + (((a1 - a0) * fr) >> 15);
        *l++ += (s * gl) >> 15;
        *r++ += (s * gr) >> 15;
        ph += inc;
    } while (l != end);
    *out_ph = ph;
}

/* mono output (the voice is panned once after the filter) */
void osc_q15_mono(int32_t *restrict o, const int16_t *fa, const int16_t *fb,
                  uint32_t ph, uint32_t inc, uint32_t tbits, int32_t xf, int32_t g, uint32_t *out_ph)
{
    uint32_t sh = 32u - tbits, mask = (1u << tbits) - 1u;
    for (int i = 0; i < N; i++) {
        uint32_t i0 = ph >> sh, i1 = (i0 + 1u) & mask;
        int32_t fr = (int32_t)((ph << tbits) >> 17);
        int32_t a = fa[i0] + (((fa[i1] - fa[i0]) * fr) >> 15);
        int32_t b = fb[i0] + (((fb[i1] - fb[i0]) * fr) >> 15);
        int32_t s = a + (((b - a) * xf) >> 15);
        o[i] += (s * g) >> 15;
        ph += inc;
    }
    *out_ph = ph;
}

void osc_f32(float *restrict l, float *restrict r, const int16_t *fa, const int16_t *fb,
             uint32_t ph, uint32_t inc, uint32_t tbits, float xf, float gl, float gr, uint32_t *out_ph)
{
    uint32_t sh = 32u - tbits, mask = (1u << tbits) - 1u;
    const float k = 1.0f / 32768.0f;
    for (int i = 0; i < N; i++) {
        uint32_t i0 = ph >> sh, i1 = (i0 + 1u) & mask;
        float fr = (float)((ph << tbits) >> 8) * (1.0f / 16777216.0f);
        float a0 = fa[i0], a1 = fa[i1], b0 = fb[i0], b1 = fb[i1];
        float a = a0 + (a1 - a0) * fr, b = b0 + (b1 - b0) * fr;
        float s = (a + (b - a) * xf) * k;
        l[i] += s * gl;
        r[i] += s * gr;
        ph += inc;
    }
    *out_ph = ph;
}

/* ------------------------------------------------------------- filters */
typedef struct { float ic1, ic2; } svf_t;
/* TPT state variable filter (Simper), low pass out; g, k per block */
void svf_lp_f32(float *restrict x, svf_t *s, float g, float k)
{
    float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    float ic1 = s->ic1, ic2 = s->ic2;
    for (int i = 0; i < N; i++) {
        float v0 = x[i];
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        x[i] = v2;
    }
    s->ic1 = ic1; s->ic2 = ic2;
}

/* multimode: low/band/high/notch blended by m0..m2 (one code path for every type) */
void svf_mm_f32(float *restrict x, svf_t *s, float g, float k, float m0, float m1, float m2)
{
    float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    float ic1 = s->ic1, ic2 = s->ic2;
    for (int i = 0; i < N; i++) {
        float v0 = x[i];
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        x[i] = m0 * v0 + m1 * v1 + m2 * v2;
    }
    s->ic1 = ic1; s->ic2 = ic2;
}

/* fixed-point Chamberlin SVF, Q14 coefficients (f, q per block) */
typedef struct { int32_t lp, bp; } svfq_t;
void svf_q(int32_t *restrict x, svfq_t *s, int32_t f, int32_t q)
{
    int32_t lp = s->lp, bp = s->bp;
    for (int i = 0; i < N; i++) {
        int32_t hp = x[i] - lp - ((q * bp) >> 14);
        bp += (f * hp) >> 14;
        lp += (f * bp) >> 14;
        x[i] = lp;
    }
    s->lp = lp; s->bp = bp;
}

/* 4-pole ladder, one-pole stages with a cheap saturator in the feedback */
static inline float sat(float v) { return v / (1.0f + (v < 0 ? -v : v)); }
typedef struct { float z[4]; } ladder_t;
void ladder_f32(float *restrict x, ladder_t *s, float g, float res)
{
    float G = g / (1.0f + g);
    float z0 = s->z[0], z1 = s->z[1], z2 = s->z[2], z3 = s->z[3];
    for (int i = 0; i < N; i++) {
        float u = x[i] - res * sat(z3);
        float v;
        v = (u - z0) * G; float y0 = v + z0; z0 = y0 + v;
        v = (y0 - z1) * G; float y1 = v + z1; z1 = y1 + v;
        v = (y1 - z2) * G; float y2 = v + z2; z2 = y2 + v;
        v = (y2 - z3) * G; float y3 = v + z3; z3 = y3 + v;
        x[i] = y3;
    }
    s->z[0] = z0; s->z[1] = z1; s->z[2] = z2; s->z[3] = z3;
}

/* ------------------------------------------------------ voice amp, pan */
void amp_pan(float *restrict l, float *restrict r, const float *restrict x, float g0, float dg, float pl, float pr)
{
    float g = g0;
    for (int i = 0; i < N; i++) {
        float v = x[i] * g;
        l[i] += v * pl;
        r[i] += v * pr;
        g += dg;
    }
}

/* stereo voice (unison spread): filter both channels; amp ramp both */
void amp_st(float *restrict l, float *restrict r, const float *restrict xl, const float *restrict xr, float g0, float dg)
{
    float g = g0;
    for (int i = 0; i < N; i++) {
        l[i] += xl[i] * g;
        r[i] += xr[i] * g;
        g += dg;
    }
}

/* ----------------------------------------------------------------- FX */
/* stereo delay, int16 lines, fixed read offset, feedback through a one-pole LP */
typedef struct { int16_t *bl, *br; uint32_t mask, w, d; float lpl, lpr; } dly_t;
void delay_st(float *restrict l, float *restrict r, dly_t *s, float fb, float damp, float wet)
{
    uint32_t w = s->w, m = s->mask, d = s->d;
    float lpl = s->lpl, lpr = s->lpr;
    for (int i = 0; i < N; i++) {
        float yl = s->bl[(w - d) & m] * (1.0f / 32768.0f);
        float yr = s->br[(w - d) & m] * (1.0f / 32768.0f);
        lpl += (yl - lpl) * damp;
        lpr += (yr - lpr) * damp;
        float il = l[i] + lpr * fb, ir = r[i] + lpl * fb;    /* ping-pong cross feed */
        il = il > 1.0f ? 1.0f : il < -1.0f ? -1.0f : il;
        ir = ir > 1.0f ? 1.0f : ir < -1.0f ? -1.0f : ir;
        s->bl[w & m] = (int16_t)(il * 32767.0f);
        s->br[w & m] = (int16_t)(ir * 32767.0f);
        l[i] += yl * wet;
        r[i] += yr * wet;
        w++;
    }
    s->w = w; s->lpl = lpl; s->lpr = lpr;
}

/* chorus: two modulated taps per side, linear interpolation, int16 line */
typedef struct { int16_t *b; uint32_t mask, w; uint32_t lph; } cho_t;
void chorus_st(float *restrict l, float *restrict r, cho_t *s, uint32_t d0, int32_t dmod_l, int32_t dmod_r, float wet)
{
    uint32_t w = s->w, m = s->mask;
    for (int i = 0; i < N; i++) {
        int16_t in = (int16_t)((l[i] + r[i]) * 16383.0f);
        s->b[w & m] = in;
        uint32_t pl = (w << 8) - d0 - (uint32_t)dmod_l, pr = (w << 8) - d0 - (uint32_t)dmod_r;
        uint32_t il = pl >> 8, ir = pr >> 8;
        float fl = (float)(pl & 255u) * (1.0f / 256.0f), fr = (float)(pr & 255u) * (1.0f / 256.0f);
        float al = s->b[il & m], bl = s->b[(il + 1u) & m];
        float ar = s->b[ir & m], br = s->b[(ir + 1u) & m];
        l[i] += (al + (bl - al) * fl) * wet;
        r[i] += (ar + (br - ar) * fr) * wet;
        w++;
    }
    s->w = w;
}

/* FDN reverb, 4 lines, Householder feedback, one-pole damping per line */
typedef struct { int16_t *b[4]; uint32_t len[4], w[4]; float lp[4]; } fdn_t;
void fdn4(float *restrict l, float *restrict r, fdn_t *s, float fb, float damp, float wet)
{
    for (int i = 0; i < N; i++) {
        float y[4];
        for (int k = 0; k < 4; k++) {
            uint32_t p = s->w[k];
            float v = s->b[k][p] * (1.0f / 32768.0f);
            s->lp[k] += (v - s->lp[k]) * damp;
            y[k] = s->lp[k];
        }
        float sum = (y[0] + y[1] + y[2] + y[3]) * 0.5f;
        float in = (l[i] + r[i]) * 0.25f;
        for (int k = 0; k < 4; k++) {
            float v = (y[k] - sum) * fb + in;
            v = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
            uint32_t p = s->w[k];
            s->b[k][p] = (int16_t)(v * 32767.0f);
            p = p + 1u == s->len[k] ? 0u : p + 1u;
            s->w[k] = p;
        }
        l[i] += (y[0] + y[2]) * wet;
        r[i] += (y[1] + y[3]) * wet;
    }
}

/* OTT-style 3-band up/down compressor: two LR-ish crossovers (cascaded one-poles
 * here; the real one uses LR4), per band peak follower and a gain computed per
 * block; per sample: split, apply smoothed gain, sum */
typedef struct { float lp1[2], lp2[2], env[3], g[3], dg[3]; } ott_t;
void ott_apply(float *restrict l, float *restrict r, ott_t *s, float c1, float c2, float att, float rel)
{
    for (int i = 0; i < N; i++) {
        float x[2] = {l[i], r[i]};
        for (int c = 0; c < 2; c++) {
            s->lp1[c] += (x[c] - s->lp1[c]) * c1;          /* low band */
            float rest = x[c] - s->lp1[c];
            s->lp2[c] += (rest - s->lp2[c]) * c2;         /* mid band */
            float hi = rest - s->lp2[c];
            float lo = s->lp1[c], mid = s->lp2[c];
            float a;
            a = lo < 0 ? -lo : lo;   s->env[0] += (a - s->env[0]) * (a > s->env[0] ? att : rel);
            a = mid < 0 ? -mid : mid; s->env[1] += (a - s->env[1]) * (a > s->env[1] ? att : rel);
            a = hi < 0 ? -hi : hi;   s->env[2] += (a - s->env[2]) * (a > s->env[2] ? att : rel);
            x[c] = lo * s->g[0] + mid * s->g[1] + hi * s->g[2];
        }
        for (int b = 0; b < 3; b++)
            s->g[b] += s->dg[b];
        l[i] = x[0];
        r[i] = x[1];
    }
}

/* soft saturator (drive), stereo */
void drive_st(float *restrict l, float *restrict r, float pre, float post)
{
    for (int i = 0; i < N; i++) {
        float a = l[i] * pre, b = r[i] * pre;
        l[i] = a / (1.0f + (a < 0 ? -a : a)) * post;
        r[i] = b / (1.0f + (b < 0 ? -b : b)) * post;
    }
}
