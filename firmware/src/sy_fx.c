/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA effects (docs/ARCHITECTURE.md §14): a chain of up to eight modules in the patch's order (FX CHAIN, sy_param.c
 * FXC1 .. FXC8), then the LIMITER; global, stereo, float. A module is an instance of a type (sy_param.c FXI_*: two of
 * each, one DELAY and one REVERB) with its ON (in the chain or bypassed) and a MIX (OTT: DEPTH; EQ, TREMOLO: none). A
 * new chain is taken with every module's wet part faded out over eight blocks and back over eight (fx_block: a dip of
 * 11.6 ms; V0.6's 2.9 ms ticked with RING MOD, BITCRUSH and TAPE at full mix); a module that joins starts from rest (its
 * state and its line cleared). A module joining, going, bypassed or back fades over eight blocks (5.8 ms: its switch,
 * fx.sw, a step of 1/8 a block; V0.6: one block), except at the start and at a sound's load (fx.settle: nothing plays
 * through the effects then, they join as before); one that is bypassed and silent costs nothing. Delay lines are int16 at 1/2 of
 * full scale (headroom for feedback): the delay's, the reverb's and the choruses' in the pool, the flangers', the
 * tapes' and the FX filters' (COMB) in .bss. The delay and the reverb run at half the rate (22 kHz: twice the time in
 * the same memory, half the work; their tails stop near 10 kHz anyway): a 1-2-1 filter and every other sample in,
 * linear interpolation out.
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
 * peak and a high shelf, +-18 dB each, on trapezoidal state-variable filters (fx_eq).
 * FLANGER: one tap a side on an 11.6 ms line swept by a sine (RATE, or a note value with SYNC FIXED; the right side
 * SPREAD of half a cycle later), from DELAY (0.3 .. 5 ms) up by DEPTH (to 5 ms more), FEEDBACK either way (negative:
 * hollow), MIX (50 %: the deepest notches). PHASER: 2 .. 12 first-order all-pass stages swept around CENTER (DEPTH: to
 * 2.5 octaves each way), FEEDBACK from the last stage, SPREAD as the flanger's. FILTER (the FX filter): 21 types
 * (FF_*): the voice filter's ten (sy_filter.c), one-pole LP 6 and HP 6, DIRTY LP (the resonance's band through a soft
 * clip), COMB + and - (a 512-sample line: 86 Hz .. 20 kHz), LOW and HIGH SHELF and TILT (RESO a gain, -18 .. +18 dB),
 * DJ (CUTOFF under the middle a low pass, over it a high pass), LO-FI (a lower rate and fewer bits, then a low pass),
 * RING (times a carrier at CUTOFF, RESO from sine to square); DRIVE before it, MIX after; a new TYPE fades out, then
 * in. BITCRUSH: BITS 1 .. 16, the rate held down to RATE. TREMOLO: the level (AUTOPAN: the side) swept by SINE,
 * TRIANGLE, SQUARE (rounded) or SAW at RATE, DEPTH. RING MOD: times a carrier at FREQ (the right SPREAD up to a fifth
 * higher), SHAPE from sine to square. TAPE: a line's tap moved by WOW (0.6 Hz, to 2.5 ms) and FLUTTER (9 and 13 Hz, to
 * 0.4 ms), SATURATE into a soft clip, TONE the head's loss (a low pass from 20 kHz down to 2.5 kHz).
 * LIMITER: a peak limiter at -0.5 dBFS, instant attack, 80 ms release, then a knee above 0.75 (transparent under it). */
enum { DRV_SOFT, DRV_TUBE, DRV_HARD, DRV_FOLD };
static const char *const DRV_NAMES[4] = {"SOFT", "TUBE", "HARD", "FOLD"};
static const char *const DLY_NAMES[2] = {"STEREO", "PINGPONG"};
static const char *const RV_NAMES[2] = {"PLATE", "HALL"};
/* the FX filter's types */
enum { FF_LP6, FF_LP12, FF_LP24, FF_HP6, FF_HP12, FF_HP24, FF_BP12, FF_BP24, FF_NOTCH, FF_PEAK, FF_LADDER, FF_DIRTY,
       FF_FORMANT, FF_COMBP, FF_COMBN, FF_LSHELF, FF_HSHELF, FF_TILT, FF_DJ, FF_LOFI, FF_RING, FF_COUNT };
static const char *const FF_NAMES[FF_COUNT] = {"LP 6", "LP 12", "LP 24", "HP 6", "HP 12", "HP 24", "BP 12", "BP 24",
                                               "NOTCH", "PEAK", "LADDER", "DIRTY LP", "FORMANT", "COMB +", "COMB -",
                                               "LO SHELF", "HI SHELF", "TILT", "DJ", "LO-FI", "RING"};
static const uint8_t FF_FLT[FF_COUNT] = {0xFF, FT_LP12, FT_LP24, 0xFF, FT_HP12, FT_HP24, FT_BP12, FT_BP24, FT_NOTCH,
                                         FT_PEAK, FT_LADDER, 0xFF, FT_FORMANT, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                         0xFF, 0xFF};      /* (the voice filter's types: theirs) */
static int ff_gain_type(int32_t t) { return t == FF_LSHELF || t == FF_HSHELF || t == FF_TILT; }   /* RESO a gain */
static int32_t ff_gain_dbx(int32_t res) { return (res - 500) * 36 / 100; }   /* .. RESO 0..1000 as 0.1 dB: -18 .. +18 */

#define FX_CHO_N 1024u                    /* chorus line a side (23 ms) */
#define FX_DLY_N 16544u                   /* delay line a side at half rate: 750 ms */
#define FX_RV_NL 8u
#define FX_RV_PRE 1104u                   /* pre-delay at half rate: 50 ms */
#define FX_RV_AP0 142u
#define FX_RV_AP1 379u
#define FX_HB (SY_BLK / 2u)               /* a block at half rate */
#define FX_RV_DPH ((uint32_t)(0.27f * 2.0f / SY_FS * 4294967296.0f))   /* the reverb's drift: 0.27 Hz */
#define FX_LN 512u                        /* the flangers', the tapes', the combs' lines a side (11.6 ms) */
static const uint16_t FX_RV_LEN[FX_RV_NL] = {1499, 1153, 1741, 853, 1321, 1031, 1607, 937};   /* 39..79 ms */
static const uint16_t FX_RV_BASE[FX_RV_NL] = {0, 1507, 2668, 4417, 5278, 6607, 7646, 9261};   /* (each + 8 guard) */
#define FX_RV_TOTAL (1499u + 1153u + 1741u + 853u + 1321u + 1031u + 1607u + 937u + 8u * 8u)
static int16_t fx_cho[2][2][FX_CHO_N] SY_POOL;   /* [instance][side] */
static int16_t fx_dly[2][FX_DLY_N] SY_POOL;
static int16_t fx_rv[FX_RV_TOTAL] SY_POOL;
static int16_t fx_rvpre[FX_RV_PRE] SY_POOL;
static int16_t fx_rvap[FX_RV_AP0 + FX_RV_AP1] SY_POOL;
static int16_t fx_flg[2][2][FX_LN] SY_SPARE, fx_tap[2][2][FX_LN] SY_SPARE, fx_comb[2][2][FX_LN] SY_SPARE;   /* [instance] */
                                                  /* [side]: 12 KiB in strata.ld's spare SRAM (V0.66; .bss before) */
_Static_assert(FX_RV_TOTAL * 2u + FX_RV_PRE * 2u + (FX_RV_AP0 + FX_RV_AP1) * 2u <= 24u * 1024u, "reverb in 24 KiB");

static uint8_t fx_lo;                     /* the CPU guard: OTT's upward compression off, the reverb on 4 lines */

typedef struct {                          /* CHORUS */
    uint32_t w, ph;                       /* the write index (0..FX_CHO_N - 1), its LFO's phase */
    float bm, hd;                         /* its taps' centre and half swing (samples) at the end of the last block */
} fx_cho_t;
typedef struct {                          /* OTT */
    float s[2][2][2];                     /* the crossovers' SVF states [crossover][channel][ic1 ic2] */
    float env[3], g[3];                   /* each band's follower and its gain at the end of the last block */
} fx_ott_t;
typedef struct {                          /* EQ */
    float s[3][2][2];                     /* the SVF states [band][channel][ic1 ic2] */
    float c[3][6];                        /* .. each band's coefficients at the end of the last block */
    uint8_t run;                          /* .. the bands that ran in it (a band at 0 dB is skipped) */
} fx_eq_t;
typedef struct {                          /* FLANGER */
    uint32_t w, ph;
    float d[2], fb[2];                    /* each side's tap (samples) at the end of the last block, its last output */
} fx_flg_t;
typedef struct {                          /* PHASER */
    float s[2][12], fb[2];                /* the stages' states, the last output a side */
    float a[2];                           /* each side's coefficient at the end of the last block */
    uint32_t ph;
} fx_phs_t;
typedef struct {                          /* the FX filter */
    flt_t f;                              /* the voice filter's types */
    float s[2][4];                        /* the others' states (one-poles, SVFs, shelves) */
    float c[4][6];                        /* .. their coefficients at the end of the last block */
    float hold[2], hph, d;                /* LO-FI's held samples and its phase; COMB's delay (samples) */
    uint32_t w, rph;                      /* COMB's write index; RING's carrier phase */
    uint8_t type, fresh, out;             /* the type playing; its coefficients from rest; faded out for a new one */
} fx_ff_t;
typedef struct {                          /* TAPE */
    uint32_t w, wph, fph, fph2;
    float d, lp[2];                       /* the tap (samples) at the end of the last block; the head's loss */
} fx_tap_t;

static struct {
    float mix[FXI_N];                     /* each instance's mix at the end of the last block (0: out) */
    float sw[FXI_N];                      /* .. its switch (in, out, bypassed), 0..1, a step of FX_SW_STEP a block */
    uint8_t fill[FXI_N];                  /* .. switched on from silence: blocks left running unheard (its line) */
    uint8_t settle;                       /* the start, a sound's load: modules join switched in (sy_engine.c) */
    uint8_t ch[NFXC], n;                  /* the chain playing: its instances, their count */
    uint32_t out;                         /* .. instances on their way out (faded over a block, then gone) */
    uint8_t dip;                          /* a new chain: blocks of its dip left (2 x FX_DIP_BLKS: down, the swap, up) */
    float duck;                           /* every module's wet part this block (1: as set) */
    float drv_lp[2][2];                   /* DRIVE: its tone's low pass [instance][side] */
    fx_cho_t cho[2];
    uint32_t dly_w;                       /* DELAY: write index (half rate) */
    float dly_t;                          /* the read delay, half-rate samples (smoothed) */
    float dly_in[2], dly_o[2][2];         /* the decimator's last input; the last two half-rate outputs */
    float dly_lp[2], dly_hp[2];
    uint16_t rv_w[FX_RV_NL], rv_pw, rv_a0, rv_a1;   /* REVERB */
    float rv_lp[FX_RV_NL], rv_in, rv_o[2][2], rv_hp[2];
    uint32_t rv_ph;
    uint8_t rv_nl;                        /* reverb lines running */
    fx_ott_t ott[2];
    fx_eq_t eq[2];
    fx_flg_t flg[2];
    fx_phs_t phs[2];
    fx_ff_t ff[2];
    float cru_hold[2][2], cru_ph[2];      /* BITCRUSH: the held samples, the hold's phase */
    uint32_t trm_ph[2];                   /* TREMOLO */
    float trm_g[2][2];                    /* .. each side's gain at the end of the last block */
    uint32_t rng_ph[2][2];                /* RING MOD: the carriers' phases [instance][side] */
    fx_tap_t tap[2];
    float lim_g;                          /* the limiter's gain */
} fx;

static uint32_t fx_nth(uint32_t i) { return FXT_INST[FXI_TYPE[i]][0] != i; }   /* instance i: its type's first (0) or second */

static void fx_join(uint32_t i)           /* instance i from rest (it joins the chain) */
{
    uint32_t k = fx_nth(i);
    fx.mix[i] = 0.0f;
    fx.sw[i] = fx.settle ? 1.0f : 0.0f;
    switch (FXI_TYPE[i]) {
    case FXT_DRIVE: fx.drv_lp[k][0] = fx.drv_lp[k][1] = 0.0f; break;
    case FXT_CHORUS:
        memset(&fx.cho[k], 0, sizeof fx.cho[k]);
        memset(fx_cho[k], 0, sizeof fx_cho[k]);
        break;
    case FXT_DELAY:
        fx.dly_w = 0;
        fx.dly_t = 0.0f;
        sy_zerof(fx.dly_in, 2u);
        sy_zerof(fx.dly_o[0], 4u);
        sy_zerof(fx.dly_lp, 2u);
        sy_zerof(fx.dly_hp, 2u);
        memset(fx_dly, 0, sizeof fx_dly);
        break;
    case FXT_REVERB:
        memset(fx.rv_w, 0, sizeof fx.rv_w);
        fx.rv_pw = fx.rv_a0 = fx.rv_a1 = 0;
        sy_zerof(fx.rv_lp, FX_RV_NL);
        fx.rv_in = 0.0f;
        sy_zerof(fx.rv_o[0], 4u);
        sy_zerof(fx.rv_hp, 2u);
        fx.rv_nl = FX_RV_NL;
        memset(fx_rv, 0, sizeof fx_rv);
        memset(fx_rvpre, 0, sizeof fx_rvpre);
        memset(fx_rvap, 0, sizeof fx_rvap);
        break;
    case FXT_OTT:
        memset(&fx.ott[k], 0, sizeof fx.ott[k]);
        fx.ott[k].g[0] = fx.ott[k].g[1] = fx.ott[k].g[2] = 1.0f;
        break;
    case FXT_EQ: memset(&fx.eq[k], 0, sizeof fx.eq[k]); break;
    case FXT_FLANGER:
        memset(&fx.flg[k], 0, sizeof fx.flg[k]);
        memset(fx_flg[k], 0, sizeof fx_flg[k]);
        break;
    case FXT_PHASER: memset(&fx.phs[k], 0, sizeof fx.phs[k]); break;
    case FXT_FFILT:
        memset(&fx.ff[k], 0, sizeof fx.ff[k]);
        flt_note(&fx.ff[k].f);
        fx.ff[k].type = 0xFF;             /* (its type taken at its first block, from rest) */
        memset(fx_comb[k], 0, sizeof fx_comb[k]);
        break;
    case FXT_CRUSH: fx.cru_hold[k][0] = fx.cru_hold[k][1] = fx.cru_ph[k] = 0.0f; break;
    case FXT_TREM:
        fx.trm_ph[k] = 0;
        fx.trm_g[k][0] = fx.trm_g[k][1] = 1.0f;
        break;
    case FXT_RING: fx.rng_ph[k][0] = fx.rng_ph[k][1] = 0; break;
    default:
        memset(&fx.tap[k], 0, sizeof fx.tap[k]);
        memset(fx_tap[k], 0, sizeof fx_tap[k]);
        break;
    }
}

static void fx_init(void)
{
    uint32_t i;
    memset(&fx, 0, sizeof fx);
    fx.settle = 1;
    for (i = 0; i < FXI_N; i++)
        fx_join(i);
    fx.lim_g = 1.0f;
    fx.duck = 1.0f;
}

/* the synced LFOs from their start (PLAY, MIDI start: sy_engine.c) */
static void fx_lfo_reset(void)
{
    uint32_t k;
    for (k = 0; k < 2u; k++) {
        fx.flg[k].ph = fx.phs[k].ph = fx.trm_ph[k] = 0;
    }
}

static inline int16_t fx_q(float x)        /* float -> a line's int16 (1/2 full scale = 1.0) */
{
    float y = x * 16383.0f;
    return (int16_t)(y > 32767.0f ? 32767 : y < -32767.0f ? -32767 : (int32_t)y);
}
#define FX_DQ (1.0f / 16383.0f)

/* a module's mix for this block: its MIX through its switch (in: 1; bypassed or going: 0; a step of 1/8 a block
 * towards it) and the dip; whether it runs. A module with a modulated line (CHORUS, FLANGER, TAPE, the FX FILTER's
 * COMB) switched on from silence (joined, or back from bypassed: it has not run since) first runs unheard for its line's
 * length, so that the line holds the sound playing before any of it is heard: else the read crosses from silence (or
 * the sound of when it stopped) into the new at full mix (tests/sy_engine_test.c t_fades: CHORUS, FLANGER and TAPE
 * added 75x .. 188x the steady output's burst on V0.6) */
#ifndef FX_SW_STEP
#define FX_SW_STEP (1.0f / 8.0f)               /* (1: V0.6's one block, for tests/sy_engine_test.c's comparison) */
#endif
static const uint8_t FX_FILL[FXT_N] = {[FXT_CHORUS] = 24, [FXT_FLANGER] = 16, [FXT_TAPE] = 16, [FXT_FFILT] = 16};
static int fx_mixes(uint32_t k, uint32_t on_id, float m1, float *m0)
{
    float s = fx.sw[k], to = sy_patch.p[on_id] && !((fx.out >> k) & 1u) ? 1.0f : 0.0f;
    *m0 = fx.mix[k];
    if (to <= 0.0f)
        fx.fill[k] = 0;
    else if (s <= 0.0f && FX_FILL[FXI_TYPE[k]]) {       /* (on from silence: its line first) */
        if (!fx.fill[k])
            fx.fill[k] = FX_FILL[FXI_TYPE[k]];
        if (--fx.fill[k]) {
            fx.mix[k] = 0.0f;
            return 1;
        }
    }
    s = to > s ? sy_minf(to, s + FX_SW_STEP) : sy_maxf(to, s - FX_SW_STEP);
    fx.sw[k] = s;
    m1 *= s * fx.duck;
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

/* an LFO's rate in Hz: RATE, or with SYNC FIXED its NOTE of the tempo (RATE's modulation stepping it, faster up) */
static float fx_rate_hz(uint32_t rate, uint32_t sync, uint32_t note)
{
    if (sy_patch.p[sync]) {
        float s = sy_note_q(mod_note(sy_goff, rate, sy_patch.p[note], 1)) * 60.0f / sy_tempo;
        return s > 0.0f ? 1.0f / s : 1.0f;
    }
    return sp_lfo_hz(mod_get(sy_goff, rate));
}
#define FX_PHI(hz) ((uint32_t)(int32_t)((hz) / SY_FS * 4294967296.0f))   /* a phase's step a sample */
static inline float fx_ph01(uint32_t ph) { return (float)(ph >> 8) * (1.0f / 16777216.0f); }

/* a line's read at r samples behind its write index w (n a power of 2), linear */
static inline float fx_tap_at(const int16_t *ln, uint32_t n, uint32_t w, float r)
{
    float rp = (float)(w + n) - r, f;
    int32_t i0 = (int32_t)rp;
    f = rp - (float)i0;
    return ((float)ln[(uint32_t)i0 & (n - 1u)] + ((float)ln[(uint32_t)(i0 + 1) & (n - 1u)] -
                                                  (float)ln[(uint32_t)i0 & (n - 1u)]) * f) * FX_DQ;
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
static void fx_drive(float *l, float *r, uint32_t fi)
{
    uint32_t o = FXI_OFF(fi), q = fx_nth(fi);
    float m0, *ch[2] = {l, r};
    uint32_t c, i, type = (uint32_t)sy_patch.p[SP_DR_TYPE + o];
    float amt = mod_get(sy_goff, SP_DR_AMT + o), g = 1.0f + 31.0f * amt * amt, mk = sy_exp2f(-0.3f * sy_log2f(g));
    float tone = sp_lin(SP_DR_TONE, mod_get(sy_goff, SP_DR_TONE + o)) * 0.001f;
    if (!fx_mixes(fi, SP_DR_ON + o, mod_get(sy_goff, SP_DR_MIX + o), &m0))
        return;
    for (c = 0; c < 2u; c++) {
        float lp = fx.drv_lp[q][c], *x = ch[c], m = m0, dm = (fx.mix[fi] - m0) * (1.0f / (float)SY_BLK);
        switch (type) {
        case DRV_TUBE: DRV_LOOP(sy_softclip(v + 0.3f) - 0.296f); break;      /* (softclip(0.3)) */
        case DRV_HARD: DRV_LOOP(sy_clampf(v, -1.0f, 1.0f)); break;
        case DRV_FOLD: DRV_LOOP(sy_sin2pi(0.25f * v)); break;
        default: DRV_LOOP(sy_softclip(v)); break;
        }
        SY_FLUSH(lp);
        fx.drv_lp[q][c] = lp;
    }
}

/* ----------------------------------------------------------------------------------------- CHORUS --- */
/* the taps read at the write index (0..FX_CHO_N - 1) less their delay: positive, so truncation is the floor. The
 * LFO is a sine and cosine pair turned a sample at a time, set from the phase at each block's start (no drift);
 * DELAY and DEPTH ramp across the block; the blend is done in the same loop */
static void fx_chorus(float *l, float *r, uint32_t fi)
{
    uint32_t o = FXI_OFF(fi), q = fx_nth(fi);
    fx_cho_t *st = &fx.cho[q];
    int16_t (*line)[FX_CHO_N] = fx_cho[q];
    float m0, *ch[2] = {l, r};
    uint32_t i, c, w = st->w;
    float base = (2.0f + 13.0f * mod_get(sy_goff, SP_CH_DELAY + o)) * 0.001f * SY_FS;
    float depth = 3.0f * mod_get(sy_goff, SP_CH_DEPTH + o) * 0.001f * SY_FS, fb = 0.9f * mod_get(sy_goff, SP_CH_FDBK + o);
    uint32_t inc = (uint32_t)(int32_t)(sp_lfo_hz(mod_get(sy_goff, SP_CH_RATE + o)) / SY_FS * 4294967296.0f);
    float bm1 = base + 0.5f * depth, hd1 = 0.5f * depth, bm, hd, dbm, dhd, sn, cs, sr, cr, m, dm;
    if (!fx_mixes(fi, SP_CH_ON + o, mod_get(sy_goff, SP_CH_MIX + o), &m0))
        return;
    if (m0 <= 0.0f)                                     /* (switched on: no ramp from long ago) */
        st->bm = bm1, st->hd = hd1;
    bm = st->bm;
    hd = st->hd;
    dbm = (bm1 - bm) * (1.0f / (float)SY_BLK);
    dhd = (hd1 - hd) * (1.0f / (float)SY_BLK);
    {
        float ph = (float)(st->ph >> 8) * (1.0f / 16777216.0f), dph = (float)(inc >> 8) * (1.0f / 16777216.0f);
        sn = sy_sin2pi(ph);
        cs = sy_sin2pi(ph + 0.25f);
        sr = sy_sin2pi(dph);
        cr = sy_sin2pi(dph + 0.25f);
    }
    m = m0;
    dm = (fx.mix[fi] - m0) * (1.0f / (float)SY_BLK);
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
                a = line[c][(uint32_t)i0 & (FX_CHO_N - 1u)];
                b2 = line[c][(uint32_t)(i0 + 1) & (FX_CHO_N - 1u)];
                acc += (a + (b2 - a) * f) * FX_DQ;
            }
            acc *= 0.5f;
            line[c][w] = fx_q(ch[c][i] + acc * fb);
            ch[c][i] += (acc - ch[c][i]) * m;
        }
        w = (w + 1u) & (FX_CHO_N - 1u);
        t = sn * cr + cs * sr;
        cs = cs * cr - sn * sr;
        sn = t;
        bm += dbm;
        hd += dhd;
    }
    st->w = w;
    st->ph += inc * SY_BLK;
    st->bm = bm1;
    st->hd = hd1;
}

/* ------------------------------------------------------------------------------------------ DELAY --- */
static void fx_delay(float *l, float *r, uint32_t fi)
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
    if (!fx_mixes(fi, SP_DL_ON, mod_get(sy_goff, SP_DL_MIX), &m0))
        return;
    if (fx.dly_t <= 0.0f)
        fx.dly_t = target;
    m = m0;
    dm = (fx.mix[fi] - m0) * (1.0f / (float)SY_BLK);
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
static void fx_reverb(float *l, float *r, uint32_t fi)
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
    if (!fx_mixes(fi, SP_RV_ON, mod_get(sy_goff, SP_RV_MIX), &m0))
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
    dm = (fx.mix[fi] - m0) * (1.0f / (float)SY_BLK);
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
        if (nl == 8u) {                                 /* the outputs before the mixing (4 lines: theirs only; */
            o[0] = y[0] - y[2] + y[4] - y[6];           /* y[4..7] were never set) */
            o[1] = y[1] - y[3] + (y[5] - y[7]);
        } else {
            o[0] = y[0] - y[2];
            o[1] = y[1] - y[3];
        }
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
static void fx_ott(float *l, float *r, uint32_t fi)
{
    uint32_t o = FXI_OFF(fi);
    fx_ott_t *st = &fx.ott[fx_nth(fi)];
    float m0, *ch[2] = {l, r}, bands[3][2][SY_BLK], c1[4], c2[4], g1[3], pk[3] = {0.0f, 0.0f, 0.0f};
    uint32_t i, c, b;
    float up = fx_lo ? 0.0f : mod_get(sy_goff, SP_OT_UP + o), down = mod_get(sy_goff, SP_OT_DOWN + o);
    float tm = sy_exp2f(4.0f * mod_get(sy_goff, SP_OT_TIME + o) - 2.0f);          /* x0.25 .. x4 */
    float lv[3];
    if (!fx_mixes(fi, SP_OT_ON + o, mod_get(sy_goff, SP_OT_DEPTH + o), &m0))
        return;
    lv[0] = sy_exp2f(sp_lin(SP_OT_LOW, mod_get(sy_goff, SP_OT_LOW + o)) * 0.0166096f);
    lv[1] = sy_exp2f(sp_lin(SP_OT_MID, mod_get(sy_goff, SP_OT_MID + o)) * 0.0166096f);
    lv[2] = sy_exp2f(sp_lin(SP_OT_HIGH, mod_get(sy_goff, SP_OT_HIGH + o)) * 0.0166096f);
    flt_svf_coef(88.3f, 1.414f, c1);
    flt_svf_coef(2500.0f, 1.414f, c2);
    for (c = 0; c < 2u; c++)                            /* the split: low = LP(x), mid = LP(x - low) .. */
        sy_copyf(bands[0][c], ch[c], SY_BLK);
    for (c = 0; c < 2u; c++)
        flt_svf_lp(bands[0][c], st->s[0][c], c1, c1);
    for (c = 0; c < 2u; c++) {
        for (i = 0; i < SY_BLK; i++)
            bands[1][c][i] = ch[c][i] - bands[0][c][i];
        sy_copyf(bands[2][c], bands[1][c], SY_BLK);
    }
    for (c = 0; c < 2u; c++)
        flt_svf_lp(bands[1][c], st->s[1][c], c2, c2);
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
        float e = st->env[b], ldb, gdb = 0.0f;
        e += (pk[b] - e) * sy_minf(1.0f, (pk[b] > e ? ATK[b] : REL[b]) / tm);
        st->env[b] = e;
        ldb = 6.0206f * sy_log2f(sy_maxf(e, 1e-6f));
        if (ldb > -12.0f)
            gdb = -(ldb + 12.0f) * (1.0f - 1.0f / (1.0f + 29.0f * down));
        else if (ldb < -40.0f && ldb > -90.0f)
            gdb = sy_minf((-40.0f - ldb) * (1.0f - 1.0f / (1.0f + 2.0f * up)), 24.0f);
        g1[b] = sy_exp2f(gdb * 0.1660964f) * lv[b];
    }
    {
        float m = m0, dm = (fx.mix[fi] - m0) * (1.0f / (float)SY_BLK), g[3], dg[3];
        for (b = 0; b < 3u; b++) {
            g[b] = st->g[b];
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
        sy_copyf(st->g, g1, 3u);
    }
    for (b = 0; b < 3u; b++)
        SY_FLUSH(st->env[b]);
    for (b = 0; b < 2u; b++)
        for (c = 0; c < 2u; c++) {
            SY_FLUSH(st->s[b][c][0]);
            SY_FLUSH(st->s[b][c][1]);
        }
}

/* --------------------------------------------------------------------------------------------- EQ --- */
/* a band: the filter's trapezoidal SVF (sy_filter.c), out = m0 v0 + m1 v1 + m2 v2 of its input, band and low (Simper's
 * shelves and peak). With A = 10^(dB / 40) (A^2 the gain at a shelf's far side, at the peak's centre): LOW SHELF 1,
 * k (A - 1), A^2 - 1, its corner g / sqrt A; PEAK 1, k (A^2 - 1), 0 with k = 1 / (Q A); HIGH SHELF A^2, k (1 - A) A,
 * 1 - A^2, its corner g sqrt A; a shelf's k = 1 / Q (LO Q, HI Q: 0.707 none over or under, more a bump at the corner
 * and a dip before it). FREQ is where a shelf's slope is halfway in dB */
enum { EQ_LS, EQ_PK, EQ_HS };
static void fx_eq_shape(uint32_t type, float hz, float db, float q, float *gk, float *m)   /* g, k; m0 m1 m2 */
{
    float a = sy_exp2f(db * 0.08304820f), s = sy_exp2f(db * 0.04152410f), g = flt_g(hz), k = 1.0f / q;
    if (type == EQ_LS)
        g /= s;
    else if (type == EQ_HS)
        g = sy_minf(g * s, FLT_GMAX);
    else
        k /= a;
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
static void fx_eq(float *l, float *r, uint32_t fi)
{
    static const uint8_t DB[3] = {SP_EQ_LOW, SP_EQ_MID, SP_EQ_HIGH}, HZ[3] = {SP_EQ_LFREQ, SP_EQ_MFREQ, SP_EQ_HFREQ};
    static const uint8_t Q[3] = {SP_EQ_LQ, SP_EQ_Q, SP_EQ_HQ};
    uint32_t o = FXI_OFF(fi);
    fx_eq_t *st = &fx.eq[fx_nth(fi)];
    float m0, wet[2][SY_BLK], *ch[2] = {l, r}, *w[2];
    uint32_t b, c, run = 0;
    int full;
    if (!fx_mixes(fi, SP_EQ_ON + o, 1.0f, &m0))
        return;
    if (m0 <= 0.0f)                                     /* (switched on: every band from rest) */
        st->run = 0;
    full = m0 >= 1.0f && fx.mix[fi] >= 1.0f;
    for (c = 0; c < 2u; c++) {
        w[c] = full ? ch[c] : wet[c];
        if (!full)
            sy_copyf(wet[c], ch[c], SY_BLK);
    }
    for (b = 0; b < 3u; b++) {
        float db = sp_lin(DB[b], mod_get(sy_goff, DB[b] + o)) * 0.1f, c1[6];
        if (sy_absf(db) < 0.01f)
            continue;
        fx_eq_coef(b, sp_cut_hz(mod_get(sy_goff, HZ[b] + o)), db, sp_q(mod_get(sy_goff, Q[b] + o)), c1);
        if (!((st->run >> b) & 1u)) {
            sy_copyf(st->c[b], c1, 3u);
            st->c[b][3] = 1.0f;
            st->c[b][4] = st->c[b][5] = 0.0f;
            sy_zerof(st->s[b][0], 4u);
        }
        {
            uint32_t k, same = 1;
            for (k = 0; k < 6u; k++)
                same &= st->c[b][k] == c1[k];
            for (c = 0; c < 2u; c++) {
                if (same)
                    fx_eq_band_k(w[c], st->s[b][c], c1);
                else
                    fx_eq_band(w[c], st->s[b][c], st->c[b], c1);
            }
        }
        sy_copyf(st->c[b], c1, 6u);
        run |= 1u << b;
    }
    st->run = (uint8_t)run;
    if (!full)
        for (c = 0; c < 2u; c++)
            fx_blend(ch[c], wet[c], m0, fx.mix[fi]);
    for (b = 0; b < 3u; b++)
        for (c = 0; c < 2u; c++) {
            SY_FLUSH(st->s[b][c][0]);
            SY_FLUSH(st->s[b][c][1]);
        }
}

/* ---------------------------------------------------------------------------------------- FLANGER --- */
/* each side's tap moves straight from where the last block left it to where the LFO is at this block's end */
static void fx_flanger(float *l, float *r, uint32_t fi)
{
    uint32_t o = FXI_OFF(fi), q = fx_nth(fi), i, c;
    fx_flg_t *st = &fx.flg[q];
    float m0, *ch[2] = {l, r};
    float hz = fx_rate_hz(SP_FL_RATE + o, SP_FL_SYNC + o, SP_FL_NOTE + o);
    float lo = (0.3f + 4.7f * mod_get(sy_goff, SP_FL_DELAY + o)) * 0.001f * SY_FS;
    float sw = 5.0f * mod_get(sy_goff, SP_FL_DEPTH + o) * 0.001f * SY_FS;
    float fb = 0.95f * sp_lin(SP_FL_FDBK, mod_get(sy_goff, SP_FL_FDBK + o)) * 0.001f;
    float sp = 0.5f * mod_get(sy_goff, SP_FL_SPREAD + o);
    uint32_t ph1 = st->ph + FX_PHI(hz) * SY_BLK, w = st->w;
    if (!fx_mixes(fi, SP_FL_ON + o, mod_get(sy_goff, SP_FL_MIX + o), &m0))
        return;
    for (c = 0; c < 2u; c++) {
        float d1 = lo + sw * (0.5f - 0.5f * sy_sin2pi(fx_ph01(ph1) + (c ? sp : 0.0f) + 0.25f)), d, dd, y = st->fb[c];
        float m = m0, dm = (fx.mix[fi] - m0) * (1.0f / (float)SY_BLK), *x = ch[c];
        uint32_t wc = w;
        if (m0 <= 0.0f)
            st->d[c] = d1;                              /* (on again: no sweep from long ago) */
        d = st->d[c];
        dd = (d1 - d) * (1.0f / (float)SY_BLK);
        for (i = 0; i < SY_BLK; i++, m += dm, d += dd) {
            y = fx_tap_at(fx_flg[q][c], FX_LN, wc, d);
            fx_flg[q][c][wc] = fx_q(x[i] + fb * y);
            wc = (wc + 1u) & (FX_LN - 1u);
            x[i] += (y - x[i]) * m;
        }
        st->d[c] = d1;
        st->fb[c] = y;
    }
    st->w = (w + SY_BLK) & (FX_LN - 1u);
    st->ph = ph1;
}

/* ----------------------------------------------------------------------------------------- PHASER --- */
/* a first-order all-pass a stage, y = a x + s, s = x - a y, a = (t - 1) / (t + 1), t = tan(pi f / fs): its coefficient
 * straight across the block from the last one to this block's end's */
static float fx_ap_coef(float hz)
{
    float t = sy_tanpi(sy_clampf(hz, 20.0f, 18000.0f) * (1.0f / SY_FS));
    return (t - 1.0f) / (t + 1.0f);
}
static void fx_phaser(float *l, float *r, uint32_t fi)
{
    static const uint8_t NST[6] = {2, 4, 6, 8, 10, 12};
    uint32_t o = FXI_OFF(fi), q = fx_nth(fi), i, c, k;
    fx_phs_t *st = &fx.phs[q];
    float m0, *ch[2] = {l, r};
    float hz = fx_rate_hz(SP_PH_RATE + o, SP_PH_SYNC + o, SP_PH_NOTE + o);
    float cen = sp_cut_hz(mod_get(sy_goff, SP_PH_CENTER + o)), oct = 2.5f * mod_get(sy_goff, SP_PH_DEPTH + o);
    float fb = 0.9f * sp_lin(SP_PH_FDBK, mod_get(sy_goff, SP_PH_FDBK + o)) * 0.001f;
    float sp = 0.5f * mod_get(sy_goff, SP_PH_SPREAD + o);
    uint32_t ns = NST[sy_clampi(sy_patch.p[SP_PH_STAGES + o], 0, 5)], ph1 = st->ph + FX_PHI(hz) * SY_BLK;
    if (!fx_mixes(fi, SP_PH_ON + o, mod_get(sy_goff, SP_PH_MIX + o), &m0))
        return;
    for (c = 0; c < 2u; c++) {
        float a1 = fx_ap_coef(cen * sy_exp2f(oct * sy_sin2pi(fx_ph01(ph1) + (c ? sp : 0.0f)))), a, da, y = st->fb[c];
        float m = m0, dm = (fx.mix[fi] - m0) * (1.0f / (float)SY_BLK), *x = ch[c], *s = st->s[c];
        if (m0 <= 0.0f)
            st->a[c] = a1;
        a = st->a[c];
        da = (a1 - a) * (1.0f / (float)SY_BLK);
        for (i = 0; i < SY_BLK; i++, m += dm, a += da) {
            float v = x[i] + fb * y;
            for (k = 0; k < ns; k++) {
                float u = a * v + s[k];
                s[k] = v - a * u;
                v = u;
            }
            y = v;
            x[i] += (y - x[i]) * m;
        }
        st->a[c] = a1;
        st->fb[c] = y;
        SY_FLUSH(st->fb[c]);
        for (k = 0; k < 12u; k++)
            SY_FLUSH(s[k]);
    }
    st->ph = ph1;
}

/* ------------------------------------------------------------------------------------- FX FILTER --- */
/* a one-pole (TPT) a side, G ramping: low pass, or high pass (x - low) */
static void fx_ff_onepole(float *x, float *s, float g0, float g1, int hp)
{
    float g = g0, dg = (g1 - g0) * (1.0f / (float)SY_BLK), z = *s;
    uint32_t i;
    for (i = 0; i < SY_BLK; i++, g += dg) {
        float v = (x[i] - z) * g, lp = v + z;
        z = lp + v;
        x[i] = hp ? x[i] - lp : lp;
    }
    *s = z;
}
/* DIRTY LP: the SVF's low pass, the band's state through a soft clip (the resonance bounded: it growls, never runs
 * away) */
static void fx_ff_dirty(float *x, float *st, const float *c0, const float *c1)
{
    float ic1 = st[0], ic2 = st[1], a1 = c0[0], a2 = c0[1], a3 = c0[2];
    const float inv = 1.0f / (float)SY_BLK;
    float d1 = (c1[0] - a1) * inv, d2 = (c1[1] - a2) * inv, d3 = (c1[2] - a3) * inv;
    uint32_t i;
    for (i = 0; i < SY_BLK; i++) {
        float v0 = x[i], v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * sy_softclip(v1 - 0.5f * ic1);    /* (bounded at +-2) */
        ic2 = 2.0f * v2 - ic2;
        x[i] = v2;
        a1 += d1;
        a2 += d2;
        a3 += d3;
    }
    st[0] = ic1;
    st[1] = ic2;
}

static void fx_ffilt(float *l, float *r, uint32_t fi)
{
    uint32_t o = FXI_OFF(fi), q = fx_nth(fi), i, c, t = (uint32_t)sy_clampi(sy_patch.p[SP_FF_TYPE + o], 0, FF_COUNT - 1);
    fx_ff_t *st = &fx.ff[q];
    float m0, m1, wet[2][SY_BLK], *ch[2] = {l, r};
    float cn = mod_get(sy_goff, SP_FF_CUT + o), hz = sp_cut_hz(cn), res = mod_get(sy_goff, SP_FF_RES + o);
    float drv = mod_get(sy_goff, SP_FF_DRIVE + o);
    m1 = mod_get(sy_goff, SP_FF_MIX + o);
    if (t != st->type) {                                /* a new type: out over a block, then from rest */
        if (st->type != 0xFF && !st->out && fx.mix[fi] > 0.0f) {
            st->out = 1;
            t = st->type;                               /* (the old one, fading) */
            m1 = 0.0f;
        } else {
            st->type = (uint8_t)t;
            st->out = 0;
            st->fresh = 1;
            flt_note(&st->f);
            sy_zerof(st->s[0], 8u);
            st->hold[0] = st->hold[1] = st->hph = 0.0f;
            st->d = 0.0f;
            memset(fx_comb[q], 0, sizeof fx_comb[q]);
        }
    }
    if (!fx_mixes(fi, SP_FF_ON + o, m1, &m0))
        return;
    for (c = 0; c < 2u; c++)
        sy_copyf(wet[c], ch[c], SY_BLK);
    if (drv > 0.0f && FF_FLT[t] == 0xFF) {              /* (the voice filter's types drive themselves) */
        float g = 1.0f + 15.0f * drv * drv, mk = sy_exp2f(-0.5f * sy_log2f(g)) * (1.0f + drv);
        for (c = 0; c < 2u; c++)
            for (i = 0; i < SY_BLK; i++)
                wet[c][i] = sy_softclip(wet[c][i] * g) * mk;
    }
    if (FF_FLT[t] != 0xFF) {                            /* the voice filter's: as a voice plays them */
        flt_par_t fp;
        fp.type = FF_FLT[t];
        fp.hz = hz;
        fp.res = res;
        fp.drive = drv;
        fp.var = t == FF_FORMANT ? 0.5f : 0.0f;
        fp.mix = 1.0f;
        flt_block(&st->f, &fp, wet[0], wet[1]);
    } else if (t == FF_LP6 || t == FF_HP6) {
        float g = flt_g(hz), g1 = g / (1.0f + g), g0 = st->fresh ? g1 : st->c[0][0];
        for (c = 0; c < 2u; c++)
            fx_ff_onepole(wet[c], &st->s[c][0], g0, g1, t == FF_HP6);
        st->c[0][0] = g1;
    } else if (t == FF_DIRTY) {
        float cc[4];
        flt_svf_coef(hz, flt_k(res, 1.414f), cc);
        for (c = 0; c < 2u; c++)
            fx_ff_dirty(wet[c], st->s[c], st->fresh ? cc : st->c[0], cc);
        sy_copyf(st->c[0], cc, 4u);
    } else if (t == FF_COMBP || t == FF_COMBN) {        /* a feedback comb: y = x + fb y(n - D), D = fs / CUTOFF */
        float d1 = sy_clampf(SY_FS / hz, 2.0f, (float)(FX_LN - 4u)), d = st->fresh ? d1 : st->d;
        float dd = (d1 - d) * (1.0f / (float)SY_BLK), fb = (t == FF_COMBP ? 0.97f : -0.97f) * res;
        float out = 1.0f - 0.5f * sy_absf(fb);
        for (i = 0; i < SY_BLK; i++, d += dd) {
            for (c = 0; c < 2u; c++) {
                float y = wet[c][i] + fb * fx_tap_at(fx_comb[q][c], FX_LN, st->w, d);
                fx_comb[q][c][st->w] = fx_q(y);
                wet[c][i] = y * out;
            }
            st->w = (st->w + 1u) & (FX_LN - 1u);
        }
        st->d = d1;
    } else if (t == FF_LSHELF || t == FF_HSHELF || t == FF_TILT) {   /* the EQ's shelves, RESO their gain */
        float db = (res - 0.5f) * 36.0f, cc[2][6];
        uint32_t nb = t == FF_TILT ? 2u : 1u, b;
        if (t == FF_TILT) {                             /* lows down as the highs go up, about CUTOFF */
            fx_eq_coef(EQ_LS, hz, -0.5f * db, 0.7071f, cc[0]);
            fx_eq_coef(EQ_HS, hz, 0.5f * db, 0.7071f, cc[1]);
        } else {
            fx_eq_coef(t == FF_LSHELF ? EQ_LS : EQ_HS, hz, db, 0.7071f, cc[0]);
        }
        for (b = 0; b < nb; b++) {
            for (c = 0; c < 2u; c++)
                fx_eq_band(wet[c], st->s[c] + 2u * b, st->fresh ? cc[b] : st->c[b], cc[b]);
            sy_copyf(st->c[b], cc[b], 6u);
        }
    } else if (t == FF_DJ) {                            /* under the middle a low pass sweeps down, over it a high */
        float lo = cn < 0.49f ? sp_cut_hz(cn / 0.49f) : 20000.0f, hi = cn > 0.51f ? sp_cut_hz((cn - 0.51f) / 0.49f) : 20.0f;
        float cl[4], ch2[4];                            /* pass up: both always on (each flat at its far end) */
        flt_svf_coef(lo, flt_k(res, 1.414f), cl);
        flt_svf_coef(hi, flt_k(res, 1.414f), ch2);
        for (c = 0; c < 2u; c++) {
            flt_svf_lp(wet[c], st->s[c], st->fresh ? cl : st->c[0], cl);
            flt_svf_hp(wet[c], st->s[c] + 2, st->fresh ? ch2 : st->c[1], ch2);
        }
        sy_copyf(st->c[0], cl, 4u);
        sy_copyf(st->c[1], ch2, 4u);
    } else if (t == FF_LOFI) {                          /* held at CUTOFF's rate, RESO's bits off (16 .. 2), then a */
        float step = hz * (1.0f / SY_FS), qs = sy_exp2f(-15.0f + 14.0f * res), cc[4];   /* low pass under it */
        for (i = 0; i < SY_BLK; i++) {
            st->hph += step;
            if (st->hph >= 1.0f) {
                st->hph -= (float)sy_floori(st->hph);
                for (c = 0; c < 2u; c++)
                    st->hold[c] = (float)sy_floori(wet[c][i] / qs + 0.5f) * qs;
            }
            wet[0][i] = st->hold[0];
            wet[1][i] = st->hold[1];
        }
        flt_svf_coef(sy_minf(hz * 0.45f, 20000.0f), 1.414f, cc);
        for (c = 0; c < 2u; c++)
            flt_svf_lp(wet[c], st->s[c], st->fresh ? cc : st->c[0], cc);
        sy_copyf(st->c[0], cc, 4u);
    } else {                                            /* RING: times a carrier at CUTOFF, sine to square */
        uint32_t inc = FX_PHI(hz);
        for (i = 0; i < SY_BLK; i++, st->rph += inc) {
            float v = sy_sin2pi(fx_ph01(st->rph)), sq = v < 0.0f ? -1.0f : 1.0f;
            v += (sq - v) * res;
            wet[0][i] *= v;
            wet[1][i] *= v;
        }
    }
    st->fresh = 0;
    for (c = 0; c < 2u; c++) {
        fx_blend(ch[c], wet[c], m0, fx.mix[fi]);
        for (i = 0; i < 4u; i++) {
            SY_FLUSH(st->s[c][i]);
            st->s[c][i] = sy_clampf(st->s[c][i], -64.0f, 64.0f);
        }
    }
}

/* ---------------------------------------------------------------------------------------- BITCRUSH --- */
static void fx_crush(float *l, float *r, uint32_t fi)
{
    uint32_t o = FXI_OFF(fi), q = fx_nth(fi), i, c;
    float m0, wet[2][SY_BLK], *ch[2] = {l, r};
    float step = sp_cut_hz(mod_get(sy_goff, SP_CR_RATE + o)) * (1.0f / SY_FS);
    float qs = sy_exp2f(1.0f - (float)sy_clampi(sy_patch.p[SP_CR_BITS + o], 1, 16)), ph = fx.cru_ph[q];
    if (!fx_mixes(fi, SP_CR_ON + o, mod_get(sy_goff, SP_CR_MIX + o), &m0))
        return;
    for (i = 0; i < SY_BLK; i++) {
        ph += step;
        if (ph >= 1.0f) {
            ph -= (float)sy_floori(ph);
            for (c = 0; c < 2u; c++)
                fx.cru_hold[q][c] = sy_clampf((float)sy_floori(ch[c][i] / qs + 0.5f) * qs, -1.0f, 1.0f);
        }
        wet[0][i] = fx.cru_hold[q][0];
        wet[1][i] = fx.cru_hold[q][1];
    }
    fx.cru_ph[q] = ph;
    for (c = 0; c < 2u; c++)
        fx_blend(ch[c], wet[c], m0, fx.mix[fi]);
}

/* ---------------------------------------------------------------------------------------- TREMOLO --- */
/* the LFO's value -1..1 at phase p (0..1): SINE, TRIANGLE, SQUARE (rounded: no click), SAW (up) */
static float fx_trm_lfo(uint32_t shape, float p)
{
    float s;
    switch (shape) {
    case 1: return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
    case 2: s = sy_sin2pi(p); return sy_softclip(3.0f * s);                         /* (+-1 from a third in) */
    case 3: return 2.0f * p - 1.0f;
    default: return sy_sin2pi(p);
    }
}
static void fx_trem(float *l, float *r, uint32_t fi)
{
    uint32_t o = FXI_OFF(fi), q = fx_nth(fi), i, c, shape = (uint32_t)sy_clampi(sy_patch.p[SP_TR_SHAPE + o], 0, 3);
    float m0, *ch[2] = {l, r};
    float hz = fx_rate_hz(SP_TR_RATE + o, SP_TR_SYNC + o, SP_TR_NOTE + o), dep = mod_get(sy_goff, SP_TR_DEPTH + o);
    uint32_t ph1 = fx.trm_ph[q] + FX_PHI(hz) * SY_BLK;
    float v = fx_trm_lfo(shape, fx_ph01(ph1)), g1[2];
    if (!fx_mixes(fi, SP_TR_ON + o, 1.0f, &m0))
        return;
    if (sy_patch.p[SP_TR_MODE + o]) {                  /* AUTOPAN: the sides' gains across, the power kept */
        float pan = 0.5f + 0.5f * dep * v;
        g1[0] = 1.4142f * sy_sin2pi(0.25f * (1.0f - pan));
        g1[1] = 1.4142f * sy_sin2pi(0.25f * pan);
    } else {
        g1[0] = g1[1] = 1.0f - dep * (0.5f - 0.5f * v);
    }
    if (m0 <= 0.0f)
        fx.trm_g[q][0] = g1[0], fx.trm_g[q][1] = g1[1];
    for (c = 0; c < 2u; c++) {
        float g = fx.trm_g[q][c], dg = (g1[c] - g) * (1.0f / (float)SY_BLK);
        float m = m0, dm = (fx.mix[fi] - m0) * (1.0f / (float)SY_BLK), *x = ch[c];
        for (i = 0; i < SY_BLK; i++, g += dg, m += dm)
            x[i] += (x[i] * g - x[i]) * m;
        fx.trm_g[q][c] = g1[c];
    }
    fx.trm_ph[q] = ph1;
}

/* ---------------------------------------------------------------------------------------- RING MOD --- */
static void fx_ring(float *l, float *r, uint32_t fi)
{
    uint32_t o = FXI_OFF(fi), q = fx_nth(fi), i, c;
    float m0, wet[2][SY_BLK], *ch[2] = {l, r};
    float hz = sp_cut_hz(mod_get(sy_goff, SP_RG_FREQ + o)), shape = mod_get(sy_goff, SP_RG_SHAPE + o);
    uint32_t inc[2];
    inc[0] = FX_PHI(hz);
    inc[1] = FX_PHI(sy_minf(hz * sy_exp2f(mod_get(sy_goff, SP_RG_SPREAD + o) * (7.0f / 12.0f)), 0.45f * SY_FS));
    if (!fx_mixes(fi, SP_RG_ON + o, mod_get(sy_goff, SP_RG_MIX + o), &m0))
        return;
    for (c = 0; c < 2u; c++) {
        uint32_t ph = fx.rng_ph[q][c];
        for (i = 0; i < SY_BLK; i++, ph += inc[c]) {
            float v = sy_sin2pi(fx_ph01(ph)), sq = v < 0.0f ? -1.0f : 1.0f;
            wet[c][i] = ch[c][i] * (v + (sq - v) * shape);
        }
        fx.rng_ph[q][c] = ph;
        fx_blend(ch[c], wet[c], m0, fx.mix[fi]);
    }
}

/* -------------------------------------------------------------------------------------------- TAPE --- */
#define FX_TAP_D0 (0.004f * SY_FS)           /* the tap's rest: 4 ms behind */
static void fx_tape(float *l, float *r, uint32_t fi)
{
    uint32_t o = FXI_OFF(fi), q = fx_nth(fi), i, c;
    fx_tap_t *st = &fx.tap[q];
    float m0, wet[2][SY_BLK], *ch[2] = {l, r};
    float wow = mod_get(sy_goff, SP_TP_WOW + o) * 0.0025f * SY_FS, flt = mod_get(sy_goff, SP_TP_FLUT + o) * 0.0004f * SY_FS;
    float sat = mod_get(sy_goff, SP_TP_SAT + o), g = 1.0f + 7.0f * sat, mk = sy_exp2f(-0.5f * sy_log2f(g));
    float kl = sy_minf(1.0f, 6.2831853f * 20000.0f * sy_exp2f(-3.0f * mod_get(sy_goff, SP_TP_TONE + o)) / SY_FS);
    uint32_t w1 = st->wph + FX_PHI(0.6f) * SY_BLK, f1 = st->fph + FX_PHI(9.0f) * SY_BLK, f2 = st->fph2 + FX_PHI(13.0f) * SY_BLK;
    float d1 = FX_TAP_D0 + wow * sy_sin2pi(fx_ph01(w1)) +
               flt * (0.67f * sy_sin2pi(fx_ph01(f1)) + 0.33f * sy_sin2pi(fx_ph01(f2))), d, dd;
    if (!fx_mixes(fi, SP_TP_ON + o, mod_get(sy_goff, SP_TP_MIX + o), &m0))
        return;
    if (m0 <= 0.0f)
        st->d = d1;
    d = st->d;
    dd = (d1 - d) * (1.0f / (float)SY_BLK);
    for (i = 0; i < SY_BLK; i++, d += dd) {
        for (c = 0; c < 2u; c++) {
            float y;
            fx_tap[q][c][st->w] = fx_q(sat > 0.0f ? sy_softclip(ch[c][i] * g) * mk : ch[c][i]);
            y = fx_tap_at(fx_tap[q][c], FX_LN, st->w, d);
            st->lp[c] += (y - st->lp[c]) * kl;
            wet[c][i] = st->lp[c];
        }
        st->w = (st->w + 1u) & (FX_LN - 1u);
    }
    st->d = d1;
    st->wph = w1;
    st->fph = f1;
    st->fph2 = f2;
    for (c = 0; c < 2u; c++) {
        SY_FLUSH(st->lp[c]);
        fx_blend(ch[c], wet[c], m0, fx.mix[fi]);
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

/* ----------------------------------------------------------------------------------------- the chain --- */
typedef void (*fx_run_t)(float *, float *, uint32_t);
static const fx_run_t FX_RUN[FXT_N] = {fx_drive, fx_chorus, fx_delay, fx_reverb, fx_ott, fx_eq, fx_flanger,
                                       fx_phaser, fx_ffilt, fx_crush, fx_trem, fx_ring, fx_tape};

/* the patch's chain, made whole (the main loop writes it a slot at a time), into ch: its count */
static uint32_t fx_chain_want(uint8_t *ch)
{
    uint32_t k, n = 0, seen = 0;
    for (k = 0; k < NFXC; k++) {
        int32_t v = sy_patch.p[SP_FXC1 + k];
        if (v <= 0 || v > (int32_t)FXI_N)
            break;
        if (!((seen >> (v - 1)) & 1u)) {
            seen |= 1u << (v - 1);
            ch[n++] = (uint8_t)(v - 1);
        }
    }
    return n;
}
/* a (n) in b (m) in the same order, perhaps with others between */
static int fx_within(const uint8_t *a, uint32_t n, const uint8_t *b, uint32_t m)
{
    uint32_t i = 0, j;
    for (j = 0; j < m && i < n; j++)
        i += a[i] == b[j];
    return i == n;
}

/* the modules in the chain's order. The patch's chain against the one playing: modules only added (the others in
 * their order), taken at once, each new one from rest and switched in over eight blocks (fx_mixes); modules only
 * taken out, each switched out over eight blocks first (fx.out), then gone; any other change (a module moved), taken
 * while every wet part is out: FX_DIP_BLKS blocks down, the swap, FX_DIP_BLKS up (each ramped across its block) */
#ifndef FX_DIP_BLKS
#define FX_DIP_BLKS 8u                          /* (2: V0.6's dip) */
#endif
static void fx_block(float *l, float *r)
{
    uint8_t want[NFXC];
    uint32_t i, j, n = fx_chain_want(want), same = n == fx.n, keep = 0;
    for (i = 0; i < n; i++)
        keep |= 1u << want[i];
    fx.out &= ~keep;                                    /* (wanted again: no longer going) */
    for (i = 0; same && i < n; i++)
        same = want[i] == fx.ch[i];
    if (!same && !fx.dip) {
        if (fx_within(fx.ch, fx.n, want, n)) {          /* added: in at once */
            uint32_t was = 0;
            for (i = 0; i < fx.n; i++)
                was |= 1u << fx.ch[i];
            for (i = 0; i < n; i++) {
                fx.ch[i] = want[i];
                if (!((was >> want[i]) & 1u))
                    fx_join(want[i]);
            }
            fx.n = (uint8_t)n;
        } else if (fx_within(want, n, fx.ch, fx.n)) {   /* taken out: faded first, out once silent */
            for (i = 0, j = 0; i < fx.n; i++)
                if ((keep >> fx.ch[i]) & 1u || fx.mix[fx.ch[i]] > 0.0f) {
                    if (!((keep >> fx.ch[i]) & 1u))
                        fx.out |= 1u << fx.ch[i];
                    fx.ch[j++] = fx.ch[i];
                }
            fx.n = (uint8_t)j;
        } else {
            fx.dip = 2u * FX_DIP_BLKS;
        }
    }
    if (fx.dip == FX_DIP_BLKS) {                        /* (the swap: the modules that join from rest) */
        uint32_t was = 0;
        for (i = 0; i < fx.n; i++)
            was |= 1u << fx.ch[i];
        fx.n = (uint8_t)fx_chain_want(fx.ch);
        fx.out = 0;
        for (i = 0; i < fx.n; i++)
            if (!((was >> fx.ch[i]) & 1u))
                fx_join(fx.ch[i]);
    }
    fx.duck = fx.dip > FX_DIP_BLKS ? (float)(fx.dip - FX_DIP_BLKS - 1u) * (1.0f / (float)FX_DIP_BLKS)   /* down */
              : fx.dip ? (float)(FX_DIP_BLKS + 1u - fx.dip) * (1.0f / (float)FX_DIP_BLKS) : 1.0f;      /* up */
    if (fx.dip)
        fx.dip--;
    for (i = 0; i < fx.n; i++)
        FX_RUN[FXI_TYPE[fx.ch[i]]](l, r, fx.ch[i]);
    if (fx.out) {                                       /* (those faded out: gone) */
        for (i = 0, j = 0; i < fx.n; i++)
            if (!((fx.out >> fx.ch[i]) & 1u) || fx.mix[fx.ch[i]] > 0.0f)
                fx.ch[j++] = fx.ch[i];
            else
                fx.out &= ~(1u << fx.ch[i]);
        fx.n = (uint8_t)j;
    }
    fx_limit(l, r);
    fx.settle = 0;
}
