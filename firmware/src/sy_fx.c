/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA effects (docs/ARCHITECTURE.md §14): DRIVE -> CHORUS -> DELAY -> REVERB -> OTT -> EQ -> LIMITER, global,
 * stereo, float; each with ON and a MIX (OTT: DEPTH; EQ: none). A module switched on or off fades over a block (its mix
 * ramps from or to 0); one that is off and silent costs nothing. Delay lines are int16 at 1/2 of full scale
 * (headroom for feedback) in the pool. The delay and the reverb run at half the rate (22 kHz: twice the time in
 * the same memory, half the work; their tails stop near 10 kHz anyway): a 1-2-1 filter and every other sample
 * in, linear interpolation out.
 * DRIVE: SOFT (cubic clip), TUBE (the clip biased: even harmonics), HARD (a clamp), FOLD (a sine folder); gain 1..32,
 * TONE tilts the result about 2 kHz, the level partly made up (gain^-0.3). CHORUS: two taps a side on a 23 ms
 * line, swept by sines in quadrature (left / right opposite), DELAY 2..15 ms, DEPTH to 3 ms, FEEDBACK. DELAY: STEREO or
 * PING-PONG, 10..750 ms or a note value of the tempo, FEEDBACK through a low pass and a high pass. REVERB: PLATE
 * (4 lines) or HALL (8 lines), a feedback delay network: Hadamard mixing, a damping low pass and the decay's gain
 * on every line (the same RT60 for all), two lines' taps drifting slowly (no metallic ringing), two input
 * diffusers, PRE-DELAY up to 50 ms, WIDTH, LOW CUT. OTT: three bands split by complementary crossovers (low =
 * LP(x), the rest = x - low: the bands sum back to x exactly) at 88 Hz and 2.5 kHz, each compressed down above
 * -12 dB (DOWNWARD: to ~30:1) and up below -40 dB (UPWARD: to 3:1, at most +24 dB), a peak follower whose
 * times scale with TIME, the gains once a block, ramped; LOW MID HIGH set each band's level. EQ: a low shelf, a
 * peak and a high shelf, +-18 dB each, on trapezoidal state-variable filters (fx_eq). LIMITER: a peak
 * limiter at -0.5 dBFS, instant attack, 80 ms release, then a knee above 0.75 (transparent under it). */
enum { DRV_SOFT, DRV_TUBE, DRV_HARD, DRV_FOLD };
static const char *const DRV_NAMES[4] = {"SOFT", "TUBE", "HARD", "FOLD"};
static const char *const DLY_NAMES[2] = {"STEREO", "PINGPONG"};
static const char *const RV_NAMES[2] = {"PLATE", "HALL"};
#define FX_CHO_N 1024u                    /* chorus line a side (23 ms) */
#define FX_DLY_N 16544u                   /* delay line a side at half rate: 750 ms */
#define FX_RV_NL 8u
#define FX_RV_PRE 1104u                   /* pre-delay at half rate: 50 ms */
#define FX_RV_AP0 142u
#define FX_RV_AP1 379u
#define FX_HB (SY_BLK / 2u)               /* a block at half rate */
#define FX_RV_DPH ((uint32_t)(0.27f * 2.0f / SY_FS * 4294967296.0f))   /* the reverb's drift: 0.27 Hz */
static const uint16_t FX_RV_LEN[FX_RV_NL] = {1499, 1153, 1741, 853, 1321, 1031, 1607, 937};   /* 39..79 ms */
static const uint16_t FX_RV_BASE[FX_RV_NL] = {0, 1507, 2668, 4417, 5278, 6607, 7646, 9261};   /* (each + 8 guard) */
#define FX_RV_TOTAL (1499u + 1153u + 1741u + 853u + 1321u + 1031u + 1607u + 937u + 8u * 8u)
static int16_t fx_cho[2][FX_CHO_N] SY_POOL;
static int16_t fx_dly[2][FX_DLY_N] SY_POOL;
static int16_t fx_rv[FX_RV_TOTAL] SY_POOL;
static int16_t fx_rvpre[FX_RV_PRE] SY_POOL;
static int16_t fx_rvap[FX_RV_AP0 + FX_RV_AP1] SY_POOL;
_Static_assert(FX_RV_TOTAL * 2u + FX_RV_PRE * 2u + (FX_RV_AP0 + FX_RV_AP1) * 2u <= 24u * 1024u, "reverb in 24 KiB");

static uint8_t fx_lo;                     /* the CPU guard: OTT's upward compression off, the reverb on 4 lines */

static struct {
    float mix[6];                         /* each module's mix at the end of the last block (0: off) */
    float drv_lp[2];
    uint32_t cho_w, cho_ph;               /* the chorus's write index (0..FX_CHO_N - 1), its LFO's phase */
    float cho_bm, cho_hd;                 /* its taps' centre and half swing (samples) at the end of the last block */
    float cho_fb[2];
    uint32_t dly_w;                       /* write index (half rate) */
    float dly_t;                          /* the read delay, half-rate samples (smoothed) */
    float dly_in[2], dly_o[2][2];         /* the decimator's last input; the last two half-rate outputs */
    float dly_lp[2], dly_hp[2];
    uint8_t dly_odd;
    uint16_t rv_w[FX_RV_NL], rv_pw, rv_a0, rv_a1;
    float rv_lp[FX_RV_NL], rv_in, rv_o[2][2], rv_hp[2];
    uint32_t rv_ph;
    float ott_s[2][2][2];                 /* the crossovers' SVF states [crossover][channel][ic1 ic2] */
    float ott_env[3], ott_g[3];           /* each band's follower and its gain at the end of the last block */
    float eq_s[3][2][2];                  /* the EQ's SVF states [band][channel][ic1 ic2] */
    float eq_c[3][6];                     /* .. each band's coefficients at the end of the last block */
    uint8_t eq_run;                       /* .. the bands that ran in it (a band at 0 dB is skipped) */
    uint8_t rv_nl;                        /* reverb lines running */
    float lim_g;                          /* the limiter's gain */
} fx;

static void fx_init(void)
{
    memset(&fx, 0, sizeof fx);
    memset(fx_cho, 0, sizeof fx_cho);
    memset(fx_dly, 0, sizeof fx_dly);
    memset(fx_rv, 0, sizeof fx_rv);
    memset(fx_rvpre, 0, sizeof fx_rvpre);
    memset(fx_rvap, 0, sizeof fx_rvap);
    fx.ott_g[0] = fx.ott_g[1] = fx.ott_g[2] = 1.0f;
    fx.lim_g = 1.0f;
    fx.rv_nl = FX_RV_NL;
}

static inline int16_t fx_q(float x)        /* float -> a line's int16 (1/2 full scale = 1.0) */
{
    float y = x * 16383.0f;
    return (int16_t)(y > 32767.0f ? 32767 : y < -32767.0f ? -32767 : (int32_t)y);
}
#define FX_DQ (1.0f / 16383.0f)

/* a module's mix for this block (0 when off), and whether it runs */
static int fx_mixes(uint32_t k, uint32_t on_id, float m1, float *m0)
{
    *m0 = fx.mix[k];
    if (!sy_patch.p[on_id])
        m1 = 0.0f;
    fx.mix[k] = m1;
    return *m0 > 0.0f || m1 > 0.0f;
}

/* out = dry + (wet - dry) x mix, the mix ramped */
static void fx_blend(float *x, const float *wet, float m0, float m1)
{
    float m = m0, dm = (m1 - m0) * (1.0f / (float)SY_BLK);
    uint32_t i;
    for (i = 0; i < SY_BLK; i++, m += dm)
        x[i] += (wet[i] - x[i]) * m;
}

/* ------------------------------------------------------------------------------------------ DRIVE --- */
/* a loop a curve (SHAPE of v, the input times the gain); TONE: lows x (1 - t), highs x (1 + t) about 2 kHz; the
 * blend in the same loop */
#define DRV_LOOP(SHAPE)                                                                                   \
    for (i = 0; i < SY_BLK; i++, m += dm) {                                                               \
        float v = x[i] * g, y = SHAPE, w;                                                                 \
        lp += (y - lp) * 0.25f;                                                                           \
        w = (lp * (1.0f - tone) + (y - lp) * (1.0f + tone)) * mk;                                         \
        x[i] += (w - x[i]) * m;                                                                           \
    }
static void fx_drive(float *l, float *r)
{
    float m0, *ch[2] = {l, r};
    uint32_t c, i, type = (uint32_t)sy_patch.p[SP_DR_TYPE];
    float amt = mod_get(sy_goff, SP_DR_AMT), g = 1.0f + 31.0f * amt * amt, mk = sy_exp2f(-0.3f * sy_log2f(g));
    float tone = sp_lin(SP_DR_TONE, mod_get(sy_goff, SP_DR_TONE)) * 0.001f;
    if (!fx_mixes(0, SP_DR_ON, mod_get(sy_goff, SP_DR_MIX), &m0))
        return;
    for (c = 0; c < 2u; c++) {
        float lp = fx.drv_lp[c], *x = ch[c], m = m0, dm = (fx.mix[0] - m0) * (1.0f / (float)SY_BLK);
        switch (type) {
        case DRV_TUBE: DRV_LOOP(sy_softclip(v + 0.3f) - 0.296f); break;      /* (softclip(0.3)) */
        case DRV_HARD: DRV_LOOP(sy_clampf(v, -1.0f, 1.0f)); break;
        case DRV_FOLD: DRV_LOOP(sy_sin2pi(0.25f * v)); break;
        default: DRV_LOOP(sy_softclip(v)); break;
        }
        SY_FLUSH(lp);
        fx.drv_lp[c] = lp;
    }
}

/* ----------------------------------------------------------------------------------------- CHORUS --- */
/* the taps read at the write index (0..FX_CHO_N - 1) less their delay: positive, so truncation is the floor. The
 * LFO is a sine and cosine pair turned a sample at a time, set from the phase at each block's start (no drift);
 * DELAY and DEPTH ramp across the block; the blend is done in the same loop */
static void fx_chorus(float *l, float *r)
{
    float m0, *ch[2] = {l, r};
    uint32_t i, c, w = fx.cho_w;
    float base = (2.0f + 13.0f * mod_get(sy_goff, SP_CH_DELAY)) * 0.001f * SY_FS;
    float depth = 3.0f * mod_get(sy_goff, SP_CH_DEPTH) * 0.001f * SY_FS, fb = 0.9f * mod_get(sy_goff, SP_CH_FDBK);
    uint32_t inc = (uint32_t)(int32_t)(sp_lfo_hz(mod_get(sy_goff, SP_CH_RATE)) / SY_FS * 4294967296.0f);
    float bm1 = base + 0.5f * depth, hd1 = 0.5f * depth, bm, hd, dbm, dhd, sn, cs, sr, cr, m, dm;
    if (!fx_mixes(1, SP_CH_ON, mod_get(sy_goff, SP_CH_MIX), &m0))
        return;
    if (m0 <= 0.0f)                                     /* (switched on: no ramp from long ago) */
        fx.cho_bm = bm1, fx.cho_hd = hd1;
    bm = fx.cho_bm;
    hd = fx.cho_hd;
    dbm = (bm1 - bm) * (1.0f / (float)SY_BLK);
    dhd = (hd1 - hd) * (1.0f / (float)SY_BLK);
    {
        float ph = (float)(fx.cho_ph >> 8) * (1.0f / 16777216.0f), dph = (float)(inc >> 8) * (1.0f / 16777216.0f);
        sn = sy_sin2pi(ph);
        cs = sy_sin2pi(ph + 0.25f);
        sr = sy_sin2pi(dph);
        cr = sy_sin2pi(dph + 0.25f);
    }
    m = m0;
    dm = (fx.mix[1] - m0) * (1.0f / (float)SY_BLK);
    for (i = 0; i < SY_BLK; i++, m += dm) {
        float hs = hd * sn, hc = hd * cs, wf = (float)(w + FX_CHO_N), t;
        float d[2][2] = {{bm + hs, bm + hc}, {bm - hs, bm - hc}};      /* two taps a side, a quarter cycle apart; */
        for (c = 0; c < 2u; c++) {                                      /* the right opposite the left */
            float acc = 0.0f;
            uint32_t k;
            for (k = 0; k < 2u; k++) {
                float rp = wf - d[c][k], f, a, b2;
                int32_t i0 = (int32_t)rp;
                f = rp - (float)i0;
                a = fx_cho[c][(uint32_t)i0 & (FX_CHO_N - 1u)];
                b2 = fx_cho[c][(uint32_t)(i0 + 1) & (FX_CHO_N - 1u)];
                acc += (a + (b2 - a) * f) * FX_DQ;
            }
            acc *= 0.5f;
            fx_cho[c][w] = fx_q(ch[c][i] + acc * fb);
            ch[c][i] += (acc - ch[c][i]) * m;
        }
        w = (w + 1u) & (FX_CHO_N - 1u);
        t = sn * cr + cs * sr;
        cs = cs * cr - sn * sr;
        sn = t;
        bm += dbm;
        hd += dhd;
    }
    fx.cho_w = w;
    fx.cho_ph += inc * SY_BLK;
    fx.cho_bm = bm1;
    fx.cho_hd = hd1;
}

/* ------------------------------------------------------------------------------------------ DELAY --- */
static void fx_delay(float *l, float *r)
{
    float m0, m, dm, *ch[2] = {l, r};
    uint32_t i, c, pp = sy_patch.p[SP_DL_MODE] != 0;
    float secs = sy_patch.p[SP_DL_SYNC]                 /* SYNC FIXED: a note value (TIME's modulation steps it) */
                     ? sy_note_q(mod_note(sy_goff, SP_DL_TIME, sy_patch.p[SP_DL_NOTE], -1)) * 60.0f / sy_tempo
                     : sp_dly_s(mod_get(sy_goff, SP_DL_TIME));
    float target = sy_clampf(secs * SY_FS * 0.5f, 2.0f, (float)(FX_DLY_N - 4u));
    float fb = 0.95f * mod_get(sy_goff, SP_DL_FDBK);
    float klp = sy_minf(1.0f, 6.2831853f * sp_cut_hz(mod_get(sy_goff, SP_DL_LP)) * 2.0f / SY_FS);
    float khp = sy_minf(1.0f, 6.2831853f * sp_cut_hz(mod_get(sy_goff, SP_DL_HP)) * 2.0f / SY_FS);
    if (!fx_mixes(2, SP_DL_ON, mod_get(sy_goff, SP_DL_MIX), &m0))
        return;
    if (fx.dly_t <= 0.0f)
        fx.dly_t = target;
    m = m0;
    dm = (fx.mix[2] - m0) * (1.0f / (float)SY_BLK);
    for (i = 0; i < SY_BLK; i += 2u) {
        float out[2], t;
        uint32_t i0, i1;
        fx.dly_t += (target - fx.dly_t) * 0.002f;      /* a time change glides (tape-like), never jumps */
        t = (float)fx.dly_w + (float)FX_DLY_N - fx.dly_t;
        i0 = (uint32_t)(int32_t)t;                      /* (t > 0: the floor; under 2 FX_DLY_N: one wrap at most) */
        i1 = i0 + 1u;
        i0 = i0 >= FX_DLY_N ? i0 - FX_DLY_N : i0;
        i1 = i1 >= FX_DLY_N ? i1 - FX_DLY_N : i1;
        for (c = 0; c < 2u; c++) {                      /* read, half rate */
            float f = t - (float)(int32_t)t, a = fx_dly[c][i0], b = fx_dly[c][i1];
            float y = (a + (b - a) * f) * FX_DQ;
            fx.dly_lp[c] += (y - fx.dly_lp[c]) * klp;  /* the loop's low pass, high pass */
            fx.dly_hp[c] += (fx.dly_lp[c] - fx.dly_hp[c]) * khp;
            out[c] = fx.dly_lp[c] - fx.dly_hp[c];
        }
        {                                               /* write: 1-2-1, every other sample */
            float x0 = 0.25f * (fx.dly_in[0] + ch[0][i + 1u]) + 0.5f * ch[0][i];
            float x1 = 0.25f * (fx.dly_in[1] + ch[1][i + 1u]) + 0.5f * ch[1][i];
            if (pp) {                                   /* PING-PONG: the mono input left, each side to the other */
                fx_dly[0][fx.dly_w] = fx_q(0.5f * (x0 + x1) + fb * out[1]);
                fx_dly[1][fx.dly_w] = fx_q(fb * out[0]);
            } else {
                fx_dly[0][fx.dly_w] = fx_q(x0 + fb * out[0]);
                fx_dly[1][fx.dly_w] = fx_q(x1 + fb * out[1]);
            }
        }
        {                                               /* out: linear between half-rate samples, blended */
            float m2 = m + dm;
            for (c = 0; c < 2u; c++) {
                float w0 = 0.5f * (fx.dly_o[c][1] + out[c]);
                fx.dly_o[c][1] = out[c];
                fx.dly_in[c] = ch[c][i + 1u];
                ch[c][i] += (w0 - ch[c][i]) * m;
                ch[c][i + 1u] += (out[c] - ch[c][i + 1u]) * m2;
            }
            m = m2 + dm;
        }
        fx.dly_w = fx.dly_w + 1u < FX_DLY_N ? fx.dly_w + 1u : 0u;
    }
    for (c = 0; c < 2u; c++) {
        SY_FLUSH(fx.dly_lp[c]);
        SY_FLUSH(fx.dly_hp[c]);
    }
}

/* ----------------------------------------------------------------------------------------- REVERB --- */
static void fx_reverb(float *l, float *r)
{
    float m0, m, dm, *ch[2] = {l, r};
    uint32_t i, c, k, nl = sy_patch.p[SP_RV_TYPE] && !fx_lo ? 8u : 4u;
    float size = 0.45f + 0.55f * mod_get(sy_goff, SP_RV_SIZE);
    float rt = 0.3f * sy_exp2f(6.0589f * mod_get(sy_goff, SP_RV_DECAY));          /* 0.3 .. 20 s */
    float damp = 1.0f - 0.85f * mod_get(sy_goff, SP_RV_DAMP), width = mod_get(sy_goff, SP_RV_WIDTH);
    float khp = sy_minf(1.0f, 6.2831853f * sp_cut_hz(mod_get(sy_goff, SP_RV_LOCUT)) * 2.0f / SY_FS);
    uint32_t pre = (uint32_t)(sp_n[SP_RV_PRE] * (float)(FX_RV_PRE - 1u));
    float gain[FX_RV_NL], len[FX_RV_NL], l1, l4, d1, d4, dd1, dd4;
    uint32_t base = 0;
    if (!fx_mixes(3, SP_RV_ON, mod_get(sy_goff, SP_RV_MIX), &m0))
        return;
    if (nl != fx.rv_nl) {                              /* lines joining: from silence */
        if (nl > fx.rv_nl)
            for (k = 0, base = 0; k < FX_RV_NL; k++) {
                uint32_t n = FX_RV_LEN[k] + 8u;
                if (k >= fx.rv_nl) {
                    sy_zero16(fx_rv + base, n);
                    fx.rv_lp[k] = 0.0f;
                }
                base += n;
            }
        fx.rv_nl = (uint8_t)nl;
    }
    for (k = 0; k < FX_RV_NL; k++)                      /* each line's length and its decay to RT60 */
        len[k] = (float)FX_RV_LEN[k] * size;
    for (k = 0; k < nl; k++)
        gain[k] = sy_exp2f(-9.9658f * len[k] / (rt * SY_FS * 0.5f));
    {                                                   /* two lines drift by +-4 samples (0.27 Hz): the drift */
        uint32_t ph0 = fx.rv_ph, ph1 = ph0 + FX_RV_DPH * FX_HB;    /* at the block's ends, straight between */
        float a = (float)(ph0 >> 8) * (1.0f / 16777216.0f), b = (float)(ph1 >> 8) * (1.0f / 16777216.0f);
        d1 = 4.0f * sy_sin2pi(a);
        d4 = 4.0f * sy_sin2pi(a + 0.37f);
        dd1 = (4.0f * sy_sin2pi(b) - d1) * (1.0f / (float)FX_HB);
        dd4 = (4.0f * sy_sin2pi(b + 0.37f) - d4) * (1.0f / (float)FX_HB);
        fx.rv_ph = ph1;
    }
    l1 = len[1];
    l4 = len[4];
    m = m0;
    dm = (fx.mix[3] - m0) * (1.0f / (float)SY_BLK);
    for (i = 0; i < SY_BLK; i += 2u) {
        float m1 = 0.5f * (l[i + 1u] + r[i + 1u]), x = 0.25f * (fx.rv_in + m1) + 0.25f * (l[i] + r[i]), y[FX_RV_NL], o[2];
        fx.rv_in = m1;                                  /* (the mono input, 1-2-1, every other sample) */
        fx.rv_hp[0] += (x - fx.rv_hp[0]) * khp;        /* LOW CUT */
        x -= fx.rv_hp[0];
        fx_rvpre[fx.rv_pw] = fx_q(x);                   /* PRE-DELAY */
        {
            uint32_t q = fx.rv_pw + FX_RV_PRE - pre;
            x = (float)fx_rvpre[q >= FX_RV_PRE ? q - FX_RV_PRE : q] * FX_DQ;
        }
        fx.rv_pw = (uint16_t)(fx.rv_pw + 1u < FX_RV_PRE ? fx.rv_pw + 1u : 0u);
        {                                               /* two allpass diffusers */
            float d = (float)fx_rvap[fx.rv_a0] * FX_DQ, v = x + 0.6f * d;
            fx_rvap[fx.rv_a0] = fx_q(v);
            x = d - 0.6f * v;
            fx.rv_a0 = (uint16_t)(fx.rv_a0 + 1u < FX_RV_AP0 ? fx.rv_a0 + 1u : 0u);
            d = (float)fx_rvap[FX_RV_AP0 + fx.rv_a1] * FX_DQ;
            v = x + 0.6f * d;
            fx_rvap[FX_RV_AP0 + fx.rv_a1] = fx_q(v);
            x = d - 0.6f * v;
            fx.rv_a1 = (uint16_t)(fx.rv_a1 + 1u < FX_RV_AP1 ? fx.rv_a1 + 1u : 0u);
        }
        len[1] = l1 + d1;
        len[4] = l4 + d4;
        d1 += dd1;
        d4 += dd4;
        for (k = 0; k < nl; k++) {                      /* read each line: at its write index + n less the delay */
            uint32_t n = FX_RV_LEN[k] + 8u, i0, i1;    /* (positive: truncation is the floor; under 2n) */
            const int16_t *ln = fx_rv + FX_RV_BASE[k];
            float rp = (float)(fx.rv_w[k] + n) - len[k], f, a, b, sv;   /* (the sum exact either way) */
            i0 = (uint32_t)(int32_t)rp;
            f = rp - (float)i0;
            i1 = i0 + 1u;
            a = ln[i0 >= n ? i0 - n : i0];
            b = ln[i1 >= n ? i1 - n : i1];
            sv = (a + (b - a) * f) * FX_DQ;
            fx.rv_lp[k] += (sv - fx.rv_lp[k]) * damp;  /* DAMP */
            y[k] = fx.rv_lp[k] * gain[k];
        }
        o[0] = y[0] - y[2] + y[4] - y[6] * (nl > 4u);  /* the outputs before the mixing */
        o[1] = y[1] - y[3] + (y[5] - y[7]) * (nl > 4u);
        if (nl == 8u) {                                 /* Hadamard 8 (scaled 1 / sqrt 8) */
            float a0 = y[0] + y[1], a1 = y[0] - y[1], a2 = y[2] + y[3], a3 = y[2] - y[3];
            float a4 = y[4] + y[5], a5 = y[4] - y[5], a6 = y[6] + y[7], a7 = y[6] - y[7];
            float b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3, b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;
            y[0] = (b0 + b4) * 0.35355339f; y[1] = (b1 + b5) * 0.35355339f; y[2] = (b2 + b6) * 0.35355339f;
            y[3] = (b3 + b7) * 0.35355339f; y[4] = (b0 - b4) * 0.35355339f; y[5] = (b1 - b5) * 0.35355339f;
            y[6] = (b2 - b6) * 0.35355339f; y[7] = (b3 - b7) * 0.35355339f;
        } else {                                        /* Hadamard 4 (scaled 1 / 2) */
            float a0 = y[0] + y[1], a1 = y[0] - y[1], a2 = y[2] + y[3], a3 = y[2] - y[3];
            y[0] = (a0 + a2) * 0.5f; y[1] = (a1 + a3) * 0.5f; y[2] = (a0 - a2) * 0.5f; y[3] = (a1 - a3) * 0.5f;
        }
        {                                               /* write back with the input, +- by line */
            float xh = x * 0.5f;
            for (k = 0; k < nl; k++) {
                uint32_t n = FX_RV_LEN[k] + 8u;
                fx_rv[FX_RV_BASE[k] + fx.rv_w[k]] = fx_q(k & 1u ? y[k] - xh : y[k] + xh);
                fx.rv_w[k] = (uint16_t)(fx.rv_w[k] + 1u < n ? fx.rv_w[k] + 1u : 0u);
            }
        }
        {                                               /* WIDTH: the sides towards their middle; blended */
            float m2 = m + dm;
            for (c = 0; c < 2u; c++) {
                float s = 0.5f * (o[0] + o[1]), v = s + (o[c] - s) * width, w0;
                v *= 0.6f;
                w0 = 0.5f * (fx.rv_o[c][1] + v);
                fx.rv_o[c][1] = v;
                ch[c][i] += (w0 - ch[c][i]) * m;
                ch[c][i + 1u] += (v - ch[c][i + 1u]) * m2;
            }
            m = m2 + dm;
        }
    }
    for (k = 0; k < FX_RV_NL; k++)
        SY_FLUSH(fx.rv_lp[k]);
    SY_FLUSH(fx.rv_hp[0]);
}

/* -------------------------------------------------------------------------------------------- OTT --- */
static void fx_ott(float *l, float *r)
{
    float m0, *ch[2] = {l, r}, bands[3][2][SY_BLK], c1[4], c2[4], g1[3], pk[3] = {0.0f, 0.0f, 0.0f};
    uint32_t i, c, b;
    float up = fx_lo ? 0.0f : mod_get(sy_goff, SP_OT_UP), down = mod_get(sy_goff, SP_OT_DOWN);
    float tm = sy_exp2f(4.0f * mod_get(sy_goff, SP_OT_TIME) - 2.0f);          /* x0.25 .. x4 */
    float lv[3];
    if (!fx_mixes(4, SP_OT_ON, mod_get(sy_goff, SP_OT_DEPTH), &m0))
        return;
    lv[0] = sy_exp2f(sp_lin(SP_OT_LOW, mod_get(sy_goff, SP_OT_LOW)) * 0.0166096f);
    lv[1] = sy_exp2f(sp_lin(SP_OT_MID, mod_get(sy_goff, SP_OT_MID)) * 0.0166096f);
    lv[2] = sy_exp2f(sp_lin(SP_OT_HIGH, mod_get(sy_goff, SP_OT_HIGH)) * 0.0166096f);
    flt_svf_coef(88.3f, 1.414f, c1);
    flt_svf_coef(2500.0f, 1.414f, c2);
    for (c = 0; c < 2u; c++)                            /* the split: low = LP(x), mid = LP(x - low) .. */
        sy_copyf(bands[0][c], ch[c], SY_BLK);
    for (c = 0; c < 2u; c++)
        flt_svf_lp(bands[0][c], fx.ott_s[0][c], c1, c1);
    for (c = 0; c < 2u; c++) {
        for (i = 0; i < SY_BLK; i++)
            bands[1][c][i] = ch[c][i] - bands[0][c][i];
        sy_copyf(bands[2][c], bands[1][c], SY_BLK);
    }
    for (c = 0; c < 2u; c++)
        flt_svf_lp(bands[1][c], fx.ott_s[1][c], c2, c2);
    for (c = 0; c < 2u; c++)
        for (i = 0; i < SY_BLK; i++) {
            float x2 = bands[2][c][i] - bands[1][c][i];
            bands[2][c][i] = x2;
            pk[0] = sy_maxf(pk[0], sy_absf(bands[0][c][i]));
            pk[1] = sy_maxf(pk[1], sy_absf(bands[1][c][i]));
            pk[2] = sy_maxf(pk[2], sy_absf(x2));
        }
    for (b = 0; b < 3u; b++) {                          /* followers, the gains in dB once a block */
        static const float ATK[3] = {0.20f, 0.35f, 0.50f}, REL[3] = {0.010f, 0.025f, 0.050f};
        float e = fx.ott_env[b], ldb, gdb = 0.0f;
        e += (pk[b] - e) * sy_minf(1.0f, (pk[b] > e ? ATK[b] : REL[b]) / tm);
        fx.ott_env[b] = e;
        ldb = 6.0206f * sy_log2f(sy_maxf(e, 1e-6f));
        if (ldb > -12.0f)
            gdb = -(ldb + 12.0f) * (1.0f - 1.0f / (1.0f + 29.0f * down));
        else if (ldb < -40.0f && ldb > -90.0f)
            gdb = sy_minf((-40.0f - ldb) * (1.0f - 1.0f / (1.0f + 2.0f * up)), 24.0f);
        g1[b] = sy_exp2f(gdb * 0.1660964f) * lv[b];
    }
    {
        float m = m0, dm = (fx.mix[4] - m0) * (1.0f / (float)SY_BLK), g[3], dg[3];
        for (b = 0; b < 3u; b++) {
            g[b] = fx.ott_g[b];
            dg[b] = (g1[b] - g[b]) * (1.0f / (float)SY_BLK);
        }
        for (i = 0; i < SY_BLK; i++, m += dm) {
            for (c = 0; c < 2u; c++) {
                float w = bands[0][c][i] * g[0] + bands[1][c][i] * g[1] + bands[2][c][i] * g[2];
                ch[c][i] += (w - ch[c][i]) * m;
            }
            g[0] += dg[0];
            g[1] += dg[1];
            g[2] += dg[2];
        }
        sy_copyf(fx.ott_g, g1, 3u);
    }
    for (b = 0; b < 3u; b++)
        SY_FLUSH(fx.ott_env[b]);
    for (b = 0; b < 2u; b++)
        for (c = 0; c < 2u; c++) {
            SY_FLUSH(fx.ott_s[b][c][0]);
            SY_FLUSH(fx.ott_s[b][c][1]);
        }
}

/* --------------------------------------------------------------------------------------------- EQ --- */
/* a band: the filter's trapezoidal SVF (sy_filter.c), out = m0 v0 + m1 v1 + m2 v2 of its input, band and low (Simper's
 * shelves and peak). With A = 10^(dB / 40) (A^2 the gain at a shelf's far side, at the peak's centre): LOW SHELF 1,
 * k (A - 1), A^2 - 1, its corner g / sqrt A; PEAK 1, k (A^2 - 1), 0 with k = 1 / (Q A); HIGH SHELF A^2, k (1 - A) A,
 * 1 - A^2, its corner g sqrt A. The shelves at Q 0.707 (no overshoot), FREQ where their slope is halfway in dB */
enum { EQ_LS, EQ_PK, EQ_HS };
static void fx_eq_shape(uint32_t type, float hz, float db, float q, float *gk, float *m)   /* g, k; m0 m1 m2 */
{
    float a = sy_exp2f(db * 0.08304820f), s = sy_exp2f(db * 0.04152410f), g = flt_g(hz), k = 1.41421356f;
    if (type == EQ_LS)
        g /= s;
    else if (type == EQ_HS)
        g = sy_minf(g * s, FLT_GMAX);
    else
        k = 1.0f / (q * a);
    gk[0] = g;
    gk[1] = k;
    m[0] = type == EQ_HS ? a * a : 1.0f;
    m[1] = type == EQ_LS ? k * (a - 1.0f) : type == EQ_PK ? k * (a * a - 1.0f) : k * (1.0f - a) * a;
    m[2] = type == EQ_LS ? a * a - 1.0f : type == EQ_PK ? 0.0f : 1.0f - a * a;
}
static void fx_eq_coef(uint32_t type, float hz, float db, float q, float *c)   /* a1 a2 a3 m0 m1 m2 */
{
    float gk[2];
    fx_eq_shape(type, hz, db, q, gk, c + 3);
    c[0] = 1.0f / (1.0f + gk[0] * (gk[0] + gk[1]));
    c[1] = gk[0] * c[0];
    c[2] = gk[0] * c[1];
}

/* a band's gain in dB at f (the screen's EQ curve): the SVF's response is its analog prototype at the warped
 * frequency tan(pi f / fs) / g, so this is what fx_eq_band plays, exactly */
static float fx_eq_db(uint32_t type, float hz, float db, float q, float f)
{
    float gk[2], m[3], w, nr, ni, dr, di;
    fx_eq_shape(type, hz, db, q, gk, m);
    w = sy_tanpi(sy_minf(f, 0.468f * SY_FS) * (1.0f / SY_FS)) / gk[0];
    dr = 1.0f - w * w;
    di = gk[1] * w;
    nr = m[0] * dr + m[2];
    ni = (m[0] * gk[1] + m[1]) * w;
    return 3.0103f * sy_log2f((nr * nr + ni * ni) / (dr * dr + di * di));
}

/* one band over a block of one channel, in place, the state st (ic1 ic2), the coefficients ramping c0 -> c1 (a1 a2
 * a3 bounded as the filter's: a ramp passes only through tame filters) */
static SY_NOINLINE void fx_eq_band(float *restrict x, float *st, const float *c0, const float *c1)
{
    float ic1 = st[0], ic2 = st[1], a1 = c0[0], a2 = c0[1], a3 = c0[2], m0 = c0[3], m1 = c0[4], m2 = c0[5];
    const float inv = 1.0f / (float)SY_BLK;
    float d1 = (c1[0] - a1) * inv, d2 = (c1[1] - a2) * inv, d3 = (c1[2] - a3) * inv;
    float e0 = (c1[3] - m0) * inv, e1 = (c1[4] - m1) * inv, e2 = (c1[5] - m2) * inv;
    uint32_t i;
    for (i = 0; i < SY_BLK; i++) {
        float v0 = x[i], v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        x[i] = m0 * v0 + m1 * v1 + m2 * v2;
        a1 += d1;
        a2 += d2;
        a3 += d3;
        m0 += e0;
        m1 += e1;
        m2 += e2;
    }
    st[0] = ic1;
    st[1] = ic2;
}
/* .. the coefficients steady (nothing moving: the same as the ramp of 0, in fewer registers) */
static SY_NOINLINE void fx_eq_band_k(float *restrict x, float *st, const float *c)
{
    float ic1 = st[0], ic2 = st[1], a1 = c[0], a2 = c[1], a3 = c[2], m0 = c[3], m1 = c[4], m2 = c[5];
    uint32_t i;
    for (i = 0; i < SY_BLK; i++) {
        float v0 = x[i], v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        x[i] = m0 * v0 + m1 * v1 + m2 * v2;
    }
    st[0] = ic1;
    st[1] = ic2;
}

/* the three bands in series: a band at 0 dB is skipped (flat: as it is), and comes back from rest (its state 0, its
 * coefficients ramping from flat); fully on, in place */
static void fx_eq(float *l, float *r)
{
    static const uint8_t DB[3] = {SP_EQ_LOW, SP_EQ_MID, SP_EQ_HIGH}, HZ[3] = {SP_EQ_LFREQ, SP_EQ_MFREQ, SP_EQ_HFREQ};
    float m0, wet[2][SY_BLK], *ch[2] = {l, r}, *w[2], q = sp_q(mod_get(sy_goff, SP_EQ_Q));
    uint32_t b, c, run = 0;
    int full;
    if (!fx_mixes(5, SP_EQ_ON, 1.0f, &m0))
        return;
    if (m0 <= 0.0f)                                     /* (switched on: every band from rest) */
        fx.eq_run = 0;
    full = m0 >= 1.0f && fx.mix[5] >= 1.0f;
    for (c = 0; c < 2u; c++) {
        w[c] = full ? ch[c] : wet[c];
        if (!full)
            sy_copyf(wet[c], ch[c], SY_BLK);
    }
    for (b = 0; b < 3u; b++) {
        float db = sp_lin(DB[b], mod_get(sy_goff, DB[b])) * 0.1f, c1[6];
        if (sy_absf(db) < 0.01f)
            continue;
        fx_eq_coef(b, sp_cut_hz(mod_get(sy_goff, HZ[b])), db, q, c1);
        if (!((fx.eq_run >> b) & 1u)) {
            sy_copyf(fx.eq_c[b], c1, 3u);
            fx.eq_c[b][3] = 1.0f;
            fx.eq_c[b][4] = fx.eq_c[b][5] = 0.0f;
            sy_zerof(fx.eq_s[b][0], 4u);
        }
        {
            uint32_t k, same = 1;
            for (k = 0; k < 6u; k++)
                same &= fx.eq_c[b][k] == c1[k];
            for (c = 0; c < 2u; c++) {
                if (same)
                    fx_eq_band_k(w[c], fx.eq_s[b][c], c1);
                else
                    fx_eq_band(w[c], fx.eq_s[b][c], fx.eq_c[b], c1);
            }
        }
        sy_copyf(fx.eq_c[b], c1, 6u);
        run |= 1u << b;
    }
    fx.eq_run = (uint8_t)run;
    if (!full)
        for (c = 0; c < 2u; c++)
            fx_blend(ch[c], wet[c], m0, fx.mix[5]);
    for (b = 0; b < 3u; b++)
        for (c = 0; c < 2u; c++) {
            SY_FLUSH(fx.eq_s[b][c][0]);
            SY_FLUSH(fx.eq_s[b][c][1]);
        }
}

/* ---------------------------------------------------------------------------------------- LIMITER --- */
/* a knee: straight up to 0.75, then towards 1 (slope 1 at the knee, never above 1) */
static inline float fx_knee(float x)
{
    float a = sy_absf(x), u;
    if (a <= 0.75f)
        return x;
    u = (a - 0.75f) * 4.0f;
    a = 0.75f + 0.25f * u / (1.0f + u);
    return x < 0.0f ? -a : a;
}

static void fx_limit(float *l, float *r)
{
    float g = fx.lim_g;
    uint32_t i;
    if (g == 1.0f) {                                    /* at rest, and the block under the knee: as it is (the */
        float pk = 0.0f;                                /* loop below would leave it so, and g at 1) */
        for (i = 0; i < SY_BLK; i++)
            pk = sy_maxf(pk, sy_maxf(sy_absf(l[i]), sy_absf(r[i])));
        if (pk <= 0.75f)
            return;
    }
    for (i = 0; i < SY_BLK; i++) {
        float p = sy_maxf(sy_absf(l[i]), sy_absf(r[i])) * g;
        if (p > 0.944f)                                 /* -0.5 dBFS: down at once */
            g *= 0.944f / p;
        else
            g += (1.0f - g) * 0.00028f;                 /* 80 ms back up */
        l[i] = fx_knee(l[i] * g);
        r[i] = fx_knee(r[i] * g);
    }
    fx.lim_g = g;
}

static void fx_block(float *l, float *r)
{
    fx_drive(l, r);
    fx_chorus(l, r);
    fx_delay(l, r);
    fx_reverb(l, r);
    fx_ott(l, r);
    fx_eq(l, r);
    fx_limit(l, r);
}
