/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA effects (docs/ARCHITECTURE.md §14): DRIVE -> CHORUS -> DELAY -> REVERB -> OTT -> LIMITER, global,
 * stereo, float; each with ON and a MIX (OTT: DEPTH). A module switched on or off fades over a block (its mix
 * ramps from or to 0); one that is off and silent costs nothing. Delay lines are int16 at 1/2 of full scale
 * (headroom for feedback) in the pool. The delay and the reverb run at half the rate (22 kHz: twice the time in
 * the same memory, half the work; their tails stop near 10 kHz anyway): a 1-2-1 filter and every other sample
 * in, linear interpolation out.
 * DRIVE: SOFT (cubic clip), TUBE (the clip biased: even harmonics), HARD (a clamp), FOLD (a sine folder); gain 1..32,
 * TONE tilts the result about 2 kHz, the level partly made up (gain^-0.3). CHORUS: two taps a side on a 23 ms
 * line, swept by sines in quadrature (left / right opposite), DELAY 2..15 ms, DEPTH to 3 ms, FEEDBACK. DELAY: STEREO or
 * PING-PONG, 10..900 ms or a note value of the tempo, FEEDBACK through a low pass and a high pass. REVERB: PLATE
 * (4 lines) or HALL (8 lines), a feedback delay network: Hadamard mixing, a damping low pass and the decay's gain
 * on every line (the same RT60 for all), two lines' taps drifting slowly (no metallic ringing), two input
 * diffusers, PRE-DELAY up to 50 ms, WIDTH, LOW CUT. OTT: three bands split by complementary crossovers (low =
 * LP(x), the rest = x - low: the bands sum back to x exactly) at 88 Hz and 2.5 kHz, each compressed down above
 * -12 dB (DOWNWARD: to ~30:1) and up below -40 dB (UPWARD: to 3:1, at most +24 dB), a peak follower whose
 * times scale with TIME, the gains once a block, ramped; LOW MID HIGH set each band's level. LIMITER: a peak
 * limiter at -0.5 dBFS, instant attack, 80 ms release, then a knee above 0.75 (transparent under it). */
enum { DRV_SOFT, DRV_TUBE, DRV_HARD, DRV_FOLD };
static const char *const DRV_NAMES[4] = {"SOFT", "TUBE", "HARD", "FOLD"};
static const char *const DLY_NAMES[2] = {"STEREO", "PINGPONG"};
static const char *const RV_NAMES[2] = {"PLATE", "HALL"};
#define FX_CHO_N 1024u                    /* chorus line a side (23 ms) */
#define FX_DLY_N 19968u                   /* delay line a side at half rate: 905 ms */
#define FX_RV_NL 8u
#define FX_RV_PRE 1104u                   /* pre-delay at half rate: 50 ms */
#define FX_RV_AP0 142u
#define FX_RV_AP1 379u
#define FX_HB (SY_BLK / 2u)               /* a block at half rate */
static const uint16_t FX_RV_LEN[FX_RV_NL] = {1499, 1153, 1741, 853, 1321, 1031, 1607, 937};   /* 39..79 ms */
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
    uint32_t cho_w, cho_ph;
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
static void fx_drive(float *l, float *r)
{
    float m0, wet[2][SY_BLK], *ch[2] = {l, r};
    uint32_t c, i, type = (uint32_t)sy_patch.p[SP_DR_TYPE];
    float amt = mod_get(sy_goff, SP_DR_AMT), g = 1.0f + 31.0f * amt * amt, mk = sy_exp2f(-0.3f * sy_log2f(g));
    float tone = sp_lin(SP_DR_TONE, mod_get(sy_goff, SP_DR_TONE)) * 0.001f;
    if (!fx_mixes(0, SP_DR_ON, mod_get(sy_goff, SP_DR_MIX), &m0))
        return;
    for (c = 0; c < 2u; c++) {
        float lp = fx.drv_lp[c];
        for (i = 0; i < SY_BLK; i++) {
            float x = ch[c][i] * g, y;
            switch (type) {
            case DRV_TUBE: y = sy_softclip(x + 0.3f) - 0.296f; break;       /* (softclip(0.3)) */
            case DRV_HARD: y = sy_clampf(x, -1.0f, 1.0f); break;
            case DRV_FOLD: y = sy_sin2pi(0.25f * x); break;
            default: y = sy_softclip(x); break;
            }
            lp += (y - lp) * 0.25f;                       /* TONE: lows x (1 - t), highs x (1 + t), ~2 kHz */
            wet[c][i] = (lp * (1.0f - tone) + (y - lp) * (1.0f + tone)) * mk;
        }
        SY_FLUSH(lp);
        fx.drv_lp[c] = lp;
        fx_blend(ch[c], wet[c], m0, fx.mix[0]);
    }
}

/* ----------------------------------------------------------------------------------------- CHORUS --- */
static void fx_chorus(float *l, float *r)
{
    float m0, wet[2][SY_BLK], *ch[2] = {l, r};
    uint32_t i, c;
    float base = (2.0f + 13.0f * mod_get(sy_goff, SP_CH_DELAY)) * 0.001f * SY_FS;
    float depth = 3.0f * mod_get(sy_goff, SP_CH_DEPTH) * 0.001f * SY_FS, fb = 0.9f * mod_get(sy_goff, SP_CH_FDBK);
    uint32_t inc = (uint32_t)(int32_t)(sp_lfo_hz(mod_get(sy_goff, SP_CH_RATE)) * 0.25f / SY_FS * 4294967296.0f);
    if (!fx_mixes(1, SP_CH_ON, mod_get(sy_goff, SP_CH_MIX), &m0))
        return;
    for (i = 0; i < SY_BLK; i++) {
        uint32_t w = fx.cho_w & (FX_CHO_N - 1u);
        float ph = (float)(fx.cho_ph >> 8) * (1.0f / 16777216.0f);
        for (c = 0; c < 2u; c++) {
            float acc = 0.0f;
            uint32_t t;
            for (t = 0; t < 2u; t++) {                  /* two taps a side, a quarter cycle apart; R opposite L */
                float d = base + depth * (0.5f + 0.5f * sy_sin2pi(ph + 0.25f * (float)t + 0.5f * (float)c));
                float rp = (float)(fx.cho_w + FX_CHO_N) - d;
                int32_t i0 = sy_floori(rp);
                float f = rp - (float)i0, a = fx_cho[c][(uint32_t)i0 & (FX_CHO_N - 1u)];
                float b = fx_cho[c][(uint32_t)(i0 + 1) & (FX_CHO_N - 1u)];
                acc += (a + (b - a) * f) * FX_DQ;
            }
            acc *= 0.5f;
            wet[c][i] = acc;
            fx_cho[c][w] = fx_q(ch[c][i] + acc * fb);
        }
        fx.cho_w++;
        fx.cho_ph += inc;
    }
    for (c = 0; c < 2u; c++)
        fx_blend(ch[c], wet[c], m0, fx.mix[1]);
}

/* ------------------------------------------------------------------------------------------ DELAY --- */
static void fx_delay(float *l, float *r)
{
    float m0, wet[2][SY_BLK], *ch[2] = {l, r};
    uint32_t i, c, pp = sy_patch.p[SP_DL_MODE] != 0, sync = (uint32_t)sy_patch.p[SP_DL_SYNC];
    float secs = sync && sync < 18u ? SYNC_BEATS[sync] * 60.0f / sy_tempo : sp_dly_s(mod_get(sy_goff, SP_DL_TIME));
    float target = sy_clampf(secs * SY_FS * 0.5f, 2.0f, (float)(FX_DLY_N - 4u));
    float fb = 0.95f * mod_get(sy_goff, SP_DL_FDBK);
    float klp = sy_minf(1.0f, 6.2831853f * sp_cut_hz(mod_get(sy_goff, SP_DL_LP)) * 2.0f / SY_FS);
    float khp = sy_minf(1.0f, 6.2831853f * sp_cut_hz(mod_get(sy_goff, SP_DL_HP)) * 0.1f * 2.0f / SY_FS);
    if (!fx_mixes(2, SP_DL_ON, mod_get(sy_goff, SP_DL_MIX), &m0))
        return;
    if (fx.dly_t <= 0.0f)
        fx.dly_t = target;
    for (i = 0; i < SY_BLK; i += 2u) {
        float out[2], t;
        fx.dly_t += (target - fx.dly_t) * 0.002f;      /* a time change glides (tape-like), never jumps */
        t = (float)fx.dly_w + (float)FX_DLY_N - fx.dly_t;
        for (c = 0; c < 2u; c++) {                      /* read, half rate */
            int32_t i0 = sy_floori(t);
            float f = t - (float)i0, a = fx_dly[c][(uint32_t)i0 % FX_DLY_N], b = fx_dly[c][(uint32_t)(i0 + 1) % FX_DLY_N];
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
        for (c = 0; c < 2u; c++) {                      /* out: linear between half-rate samples */
            wet[c][i] = 0.5f * (fx.dly_o[c][1] + out[c]);
            wet[c][i + 1u] = out[c];
            fx.dly_o[c][1] = out[c];
            fx.dly_in[c] = ch[c][i + 1u];
        }
        fx.dly_w = fx.dly_w + 1u < FX_DLY_N ? fx.dly_w + 1u : 0u;
    }
    for (c = 0; c < 2u; c++) {
        SY_FLUSH(fx.dly_lp[c]);
        SY_FLUSH(fx.dly_hp[c]);
        fx_blend(ch[c], wet[c], m0, fx.mix[2]);
    }
}

/* ----------------------------------------------------------------------------------------- REVERB --- */
static void fx_reverb(float *l, float *r)
{
    float m0, wet[2][SY_BLK], *ch[2] = {l, r};
    uint32_t i, c, k, nl = sy_patch.p[SP_RV_TYPE] && !fx_lo ? 8u : 4u;
    float size = 0.45f + 0.55f * mod_get(sy_goff, SP_RV_SIZE);
    float rt = 0.3f * sy_exp2f(6.0589f * mod_get(sy_goff, SP_RV_DECAY));          /* 0.3 .. 20 s */
    float damp = 1.0f - 0.85f * mod_get(sy_goff, SP_RV_DAMP), width = mod_get(sy_goff, SP_RV_WIDTH);
    float khp = sy_minf(1.0f, 6.2831853f * sp_cut_hz(mod_get(sy_goff, SP_RV_LOCUT)) * 0.05f / SY_FS);
    uint32_t pre = (uint32_t)(sp_n[SP_RV_PRE] * (float)(FX_RV_PRE - 1u));
    float gain[FX_RV_NL], len[FX_RV_NL];
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
    for (k = 0; k < nl; k++) {                          /* each line's length and its decay to RT60 */
        len[k] = (float)FX_RV_LEN[k] * size;
        gain[k] = sy_exp2f(-9.9658f * len[k] / (rt * SY_FS * 0.5f));
    }
    for (i = 0; i < SY_BLK; i += 2u) {
        float m1 = 0.5f * (l[i + 1u] + r[i + 1u]), x = 0.25f * (fx.rv_in + m1) + 0.25f * (l[i] + r[i]), y[FX_RV_NL], o[2];
        float ph = (float)(fx.rv_ph >> 8) * (1.0f / 16777216.0f);
        fx.rv_in = m1;                                  /* (the mono input, 1-2-1, every other sample) */
        fx.rv_hp[0] += (x - fx.rv_hp[0]) * khp;        /* LOW CUT */
        x -= fx.rv_hp[0];
        fx_rvpre[fx.rv_pw] = fx_q(x);                   /* PRE-DELAY */
        x = (float)fx_rvpre[(fx.rv_pw + FX_RV_PRE - pre) % FX_RV_PRE] * FX_DQ;
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
        for (k = 0, base = 0; k < nl; k++) {            /* read each line (two of them drift by +-4 samples) */
            uint32_t n = FX_RV_LEN[k] + 8u;
            float d = len[k] + (k == 1u ? 4.0f * sy_sin2pi(ph) : k == 4u ? 4.0f * sy_sin2pi(ph + 0.37f) : 0.0f);
            float rp = (float)fx.rv_w[k] + (float)n - d;
            int32_t i0 = sy_floori(rp);
            float f = rp - (float)i0, a = fx_rv[base + (uint32_t)i0 % n], b = fx_rv[base + (uint32_t)(i0 + 1) % n];
            float s = (a + (b - a) * f) * FX_DQ;
            fx.rv_lp[k] += (s - fx.rv_lp[k]) * damp;   /* DAMP */
            y[k] = fx.rv_lp[k] * gain[k];
            base += n;
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
        for (k = 0, base = 0; k < nl; k++) {            /* write back with the input */
            uint32_t n = FX_RV_LEN[k] + 8u;
            fx_rv[base + fx.rv_w[k]] = fx_q(y[k] + x * (k & 1u ? -0.5f : 0.5f));
            fx.rv_w[k] = (uint16_t)(fx.rv_w[k] + 1u < n ? fx.rv_w[k] + 1u : 0u);
            base += n;
        }
        for (c = 0; c < 2u; c++) {                      /* WIDTH: the sides towards their middle */
            float s = 0.5f * (o[0] + o[1]), v = s + (o[c] - s) * width;
            v *= 0.6f;
            wet[c][i] = 0.5f * (fx.rv_o[c][1] + v);
            wet[c][i + 1u] = v;
            fx.rv_o[c][1] = v;
        }
        fx.rv_ph += (uint32_t)(0.27f * 2.0f / SY_FS * 4294967296.0f);
    }
    for (k = 0; k < FX_RV_NL; k++)
        SY_FLUSH(fx.rv_lp[k]);
    SY_FLUSH(fx.rv_hp[0]);
    for (c = 0; c < 2u; c++)
        fx_blend(ch[c], wet[c], m0, fx.mix[3]);
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
            bands[2][c][i] -= bands[1][c][i];
            for (b = 0; b < 3u; b++)
                pk[b] = sy_maxf(pk[b], sy_absf(bands[b][c][i]));
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
            for (b = 0; b < 3u; b++)
                g[b] += dg[b];
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
    fx_limit(l, r);
}
