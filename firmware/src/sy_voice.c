/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA voice (docs/ARCHITECTURE.md §4, §8): OSC A, OSC B, SUB and NOISE, each to the FILTER or DIRECT, then the
 * amp (ENV 1 x velocity) into the mix. Once a block: the matrix's offsets (from the last block's sources), the
 * envelopes, the LFOs, the pitch (glide, bend), each source's settings. Then the audio: the oscillators into an
 * integer bus a route (Q15, their level, pan and fades ramped in their unison voices' gains), made float once; the
 * sub and the noise added in float; the filter (in place when all wet); the amp ramped across the block. A source switched on or off, or changing kind
 * (warp, shape, type, route), fades out over 4 blocks (2.9 ms), changes while silent and fades back in; the filter
 * too (its MIX from or to 0): nothing a knob or a switch does steps the output.
 * SUB: sine, triangle, saw, square, pulse (polyBLEP edges) or a rounded square, OCT below the note. NOISE: white,
 * pink (three one-poles fitted to -3 dB / octave within 0.4 dB from 30 Hz to 18 kHz) or brown, COLOR tilts it,
 * STEREO decorrelates left and right. */
enum { SUB_SINE, SUB_TRI, SUB_SAW, SUB_SQUARE, SUB_PULSE, SUB_ROUND, SUB_NSHAPE };
static const char *const SUB_NAMES[SUB_NSHAPE] = {"SINE", "TRIANGLE", "SAW", "SQUARE", "PULSE", "ROUND"};
static const char *const NZ_NAMES[3] = {"WHITE", "PINK", "BROWN"};
#define SY_INC_A4 (440.0f / SY_FS * 4294967296.0f)
#define VOICE_STEAL_BLKS 4u                /* a stolen voice fades out over 4 blocks (2.9 ms) */
#define VOICE_FADE 0.25f                   /* a switch's fade a block: over 4 blocks */
enum { VS_A, VS_B, VS_SUB, VS_NZ };        /* the sources */

typedef struct {
    uint8_t on;                            /* sounding (or fading out) */
    uint8_t note, vel;                     /* the note playing, its velocity */
    uint8_t gate;                          /* held (a key down or the sustain pedal) */
    uint8_t key;                           /* its key is down */
    uint8_t steal;                         /* fading out for next_note: blocks left (0: not stealing) */
    uint8_t next_note, next_vel;           /* the note a steal starts (next_vel 0: none) */
    uint8_t alt, uni_cap;                  /* note-on alternate 0/1; unison voices the budget allows */
    uint8_t ran;                           /* bit per source: it ran the last block (else it fades in) */
    uint8_t fresh;                         /* a new note: no ramps from the voice's last sound */
    uint32_t age;                          /* note-on order */
    float pitch, target;                   /* semitones: where the glide is, where it goes */
    float amp;                             /* the amp at the end of the last block (envelope x velocity) */
    float fade;                            /* steal fade at the end of the last block, 1..0 */
    float fmix;                            /* the filter's MIX x its switch fade at the end of the last block */
    float fon;                             /* the filter's switch fade, 0..1 */
    uint8_t ftype;                         /* the filter type playing */
    float son[4];                          /* each source's switch fade, 0..1 */
    uint8_t kind[4];                       /* the kind each source plays: warp / shape / type, route << 7 */
    float rnd;                             /* note-on random 0..1 */
    float g[4][2];                         /* each source's level x pan at the end of the last block */
    env_t env[2];
    lfo_t lfo[2];
    osc_v_t osc[SY_NOSC];
    flt_t flt;
    uint32_t sub_ph;
    float sub_lp;
    uint32_t nz_seed[2];
    float nz[2][5];                        /* per channel: pink's three poles, brown, the tilt's low pass */
    float src[SRC_VOICE_LAST + 1u];        /* per-voice modulation sources (end of the last block) */
    float off[NSLOT];                      /* modulation offsets */
} sy_voice_t;

static sy_voice_t sy_v[SY_NVOICE];
static float sy_bend_v;                    /* -1..1 */
static float wt_g[SY_NOSC][2];             /* a table swap's fade of each oscillator: this block's start and end */
static float sy_glfo_v[2];                 /* the FREE LFOs' values this block (sy_engine.c) */

/* ------------------------------------------------------------------------------------------ helpers --- */
static inline uint32_t voice_inc(float semis)          /* a pitch (MIDI note) -> increment (< 2^31) */
{
    float f = SY_INC_A4 * sy_exp2f((semis - 69.0f) * (1.0f / 12.0f));
    return (uint32_t)(int32_t)sy_minf(f, 2147483520.0f);
}

/* polyBLEP: the correction around t (cycles) for a step of +2 at phase 0; dt the increment, idt 1 / dt */
static inline float voice_blep(float t, float dt, float idt)
{
    if (t < dt) {
        t *= idt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) * idt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/* pan -1..1 and a level -> balance gains (centre: both at the level; a side fades the other) */
static inline void voice_bal(float pan, float lvl, float *g)
{
    g[0] = lvl * sy_minf(1.0f, 1.0f - pan);
    g[1] = lvl * sy_minf(1.0f, 1.0f + pan);
}

/* --------------------------------------------------------------------------------------------- sub --- */
/* one loop a shape (a switch in the loop keeps every shape's constants out of the target's registers) */
#define SUB_LOOP(BODY)                                                                                    \
    for (i = 0; i < SY_BLK; i++, ph += inc) {                                                             \
        float t = (float)(ph >> 8) * (1.0f / 16777216.0f);                                                \
        BODY;                                                                                             \
    }
static void voice_sub(sy_voice_t *v, uint32_t inc, uint32_t shape, float pw, float *x)
{
    uint32_t i, ph = v->sub_ph;
    float dt = (float)inc * (1.0f / 4294967296.0f), idt = dt > 0.0f ? 1.0f / dt : 0.0f;
    float w = 0.5f - 0.45f * pw, lp = v->sub_lp, k = sy_minf(1.0f, 25.1327f * dt);   /* ROUND: ~4 x f0 */
    switch (shape) {
    case SUB_SINE:
        SUB_LOOP(x[i] = sy_sin2pi(t));
        break;
    case SUB_TRI:
        SUB_LOOP(x[i] = 4.0f * sy_absf(t - 0.5f) - 1.0f);
        break;
    case SUB_SAW:
        SUB_LOOP(x[i] = 2.0f * t - 1.0f - voice_blep(t, dt, idt));
        break;
    case SUB_PULSE: {                                      /* (less its mean: no DC) */
        float m = 2.0f * w - 1.0f;
        SUB_LOOP(float u = t + 1.0f - w; u -= (float)sy_floori(u);
                 x[i] = (t < w ? 1.0f : -1.0f) + voice_blep(t, dt, idt) - voice_blep(u, dt, idt) - m);
        break;
    }
    case SUB_ROUND:                                        /* the square through a one-pole */
        SUB_LOOP(float u = t + 0.5f; u -= (float)sy_floori(u);
                 lp += ((t < 0.5f ? 1.0f : -1.0f) + voice_blep(t, dt, idt) - voice_blep(u, dt, idt) - lp) * k;
                 x[i] = lp);
        break;
    default:                                               /* SQUARE */
        SUB_LOOP(float u = t + 0.5f; u -= (float)sy_floori(u);
                 x[i] = (t < 0.5f ? 1.0f : -1.0f) + voice_blep(t, dt, idt) - voice_blep(u, dt, idt));
        break;
    }
    v->sub_ph = ph;
    SY_FLUSH(lp);
    v->sub_lp = lp;
}

/* ------------------------------------------------------------------------------------------- noise --- */
/* one loop a type; COLOR: a one-pole at about 1 kHz splits it, lows x (1 - c), highs x (1 + c) */
#define NZ_LOOP(BODY)                                                                                     \
    for (i = 0; i < SY_BLK; i++) {                                                                        \
        float w = sy_randf(sy_rand(&seed)), y;                                                            \
        BODY;                                                                                             \
        tl += (y - tl) * 0.1329f;                                                                         \
        o[i] = tl * lo + (y - tl) * hi;                                                                   \
    }
static void voice_noise(sy_voice_t *v, uint32_t type, float color, float stereo, float *l, float *r)
{
    uint32_t i, c;
    float *out[2] = {l, r}, lo = 1.0f - color, hi = 1.0f + color;
    for (c = 0; c < 2u; c++) {
        float *s = v->nz[c], *o = out[c], tl = s[4];
        uint32_t seed = v->nz_seed[c];
        if (type == 1u) {                               /* pink */
            float p0 = s[0], p1 = s[1], p2 = s[2];
            NZ_LOOP(p0 += (w - p0) * 0.0056806f; p1 += (w - p1) * 0.0553753f; p2 += (w - p2) * 0.4342910f;
                    y = 8.665f * p0 + 1.980f * p1 + 0.7332f * p2 + 0.1205f * w);
            s[0] = p0;
            s[1] = p1;
            s[2] = p2;
        } else if (type == 2u) {                        /* brown */
            float b = s[3];
            NZ_LOOP(b = 0.998f * b + 0.0632f * w; y = b);
            s[3] = b;
        } else {
            NZ_LOOP(y = w);
        }
        s[4] = tl;
        v->nz_seed[c] = seed;
        for (i = 0; i < 5u; i++)
            SY_FLUSH(s[i]);
    }
    if (stereo < 1.0f)                                  /* STEREO: the right towards the left */
        for (i = 0; i < SY_BLK; i++)
            r[i] = l[i] + (r[i] - l[i]) * stereo;
}

/* ------------------------------------------------------------------------------------------- notes --- */
static void voice_init(sy_voice_t *v, uint32_t k)
{
    uint32_t j;
    memset(v, 0, sizeof *v);
    v->nz_seed[0] = 0x9E3779B9u ^ (k * 2654435761u + 1u);
    v->nz_seed[1] = 0x7F4A7C15u ^ (k * 40503u + 7u);
    v->lfo[0].seed = 0x1234567u + k;
    v->lfo[1].seed = 0x89ABCDEu + k;
    v->uni_cap = SY_NUNI;
    v->fade = 1.0f;
    for (j = 0; j < SY_NOSC * SY_NUNI; j++)         /* FREE phases start anywhere */
        v->osc[j / SY_NUNI].ph[j % SY_NUNI] = sy_rand(&v->nz_seed[0]);
}

static float sy_last_pitch = 60.0f;        /* the last note played (glide starts from it) */
static uint32_t sy_age, sy_rseed = 0x2545F491u;
static uint8_t sy_alt;

/* starts a note on a voice: a silent voice from scratch; a sounding one (a retrigger, MONO) keeps its phases
 * and its envelopes rise from where they are */
static void voice_start(sy_voice_t *v, uint32_t note, uint32_t vel, float from_pitch, int glide)
{
    uint32_t o;
    int fresh = !v->on;
    v->on = 1;
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->gate = v->key = 1;
    v->steal = 0;
    v->next_vel = 0;
    v->age = ++sy_age;
    v->uni_cap = SY_NUNI;                  /* (the budget takes unison voices back from the oldest voices) */
    v->target = (float)note;
    v->pitch = glide ? from_pitch : (float)note;
    v->rnd = (float)(sy_rand(&sy_rseed) >> 8) * (1.0f / 16777216.0f);
    v->alt = sy_alt ^= 1u;
    if (fresh) {
        v->amp = 0.0f;
        v->fade = 1.0f;
        v->fresh = 1;
        v->ran = 0;
        v->fmix = 0.0f;
        v->env[0].v = v->env[1].v = 0.0f;
        v->env[0].st = v->env[1].st = ENV_IDLE;
        flt_note(&v->flt);
        v->sub_ph = 0;
        v->sub_lp = 0.0f;
        memset(v->nz, 0, sizeof v->nz);
        for (o = 0; o < SY_NOSC; o++) {
            const int16_t *p = sy_patch.p + (o ? SP_B_ON : SP_A_ON);
            osc_note(&v->osc[o], p[SP_A_PHMODE - SP_A_ON] == 0,
                     (float)p[SP_A_PHASE - SP_A_ON] * (1.0f / 360.0f), (float)p[SP_A_RAND - SP_A_ON] * 0.001f,
                     &sy_rseed);
        }
    }
    env_gate(&v->env[0], 1);
    env_gate(&v->env[1], 1);
    for (o = 0; o < 2u; o++) {
        lfo_par_t lp;
        uint32_t b = o ? SP_L2_SHAPE - SP_L1_SHAPE : 0u;
        lp.mode = (uint8_t)sy_patch.p[SP_L1_MODE + b];
        lp.phase = (float)sy_patch.p[SP_L1_PHASE + b] * (1.0f / 360.0f);
        lfo_note(&v->lfo[o], &lp);
    }
    v->src[SRC_VEL] = (float)vel * (1.0f / 127.0f);
    v->src[SRC_NOTE] = ((float)note - 64.0f) * (1.0f / 64.0f);
    v->src[SRC_RAND] = v->rnd;
    v->src[SRC_ALT] = (float)v->alt;
    sy_last_pitch = (float)note;
}

static void voice_release(sy_voice_t *v)
{
    v->gate = 0;
    env_gate(&v->env[0], 0);
    env_gate(&v->env[1], 0);
}

/* ------------------------------------------------------------------------------------------- block --- */
static void voice_osc_par(const sy_voice_t *v, uint32_t o, float pitch, osc_par_t *op)
{
    const float *off = v->off;
    uint32_t b = o ? SP_B_ON - SP_A_ON : 0u;
    float semi = sp_lin(SP_A_SEMI + b, mod_get(off, SP_A_SEMI + b));
    float fine = sp_lin(SP_A_FINE + b, mod_get(off, SP_A_FINE + b));
    int32_t nu = sy_patch.p[SP_A_UNI + b];
    op->inc1 = voice_inc(pitch + 12.0f * (float)sy_patch.p[SP_A_OCT + b] + semi + fine * 0.01f);
    op->inc0 = op->inc1;
    op->pos = mod_get(off, SP_A_POS + b);
    op->warp = (uint8_t)sy_patch.p[SP_A_WARP + b];
    op->wamt = mod_get(off, SP_A_WAMT + b);
    op->nuni = (uint8_t)(nu < 1 ? 1 : nu > (int32_t)v->uni_cap ? v->uni_cap : nu);
    op->umode = (uint8_t)sy_patch.p[SP_A_UMODE + b];
    op->det = mod_get(off, SP_A_DETUNE + b);
    op->blend = mod_get(off, SP_A_BLEND + b);
    op->width = mod_get(off, SP_A_WIDTH + b);
    op->spread = mod_get(off, SP_A_SPREAD + b);
    op->pm = 0;
    op->pm_inc = 0;
}

static void voice_lfo_par(const float *off, uint32_t k, lfo_par_t *lp)
{
    uint32_t b = k ? SP_L2_SHAPE - SP_L1_SHAPE : 0u;
    lp->shape = (uint8_t)sy_patch.p[SP_L1_SHAPE + b];
    lp->mode = (uint8_t)sy_patch.p[SP_L1_MODE + b];
    lp->sync = (uint8_t)sy_patch.p[SP_L1_SYNC + b];
    lp->note = (uint8_t)mod_note(off, SP_L1_RATE + b, sy_patch.p[k ? SP_L2_NOTE : SP_L1_NOTE], 1);
    lp->rate = off ? mod_get(off, SP_L1_RATE + b) : sp_n[SP_L1_RATE + b];
    lp->delay = sp_n[SP_L1_DELAY + b];
    lp->rise = sp_n[SP_L1_RISE + b];
    lp->phase = (float)sy_patch.p[SP_L1_PHASE + b] * (1.0f / 360.0f);
    lp->smooth = sp_n[SP_L1_SMOOTH + b];
    lp->dr = &lfo_dr[k];
}

/* adds a source's block (x, y) into its bus with its gains ramped from the last block's */
static void voice_mix(float *bl, float *br, const float *x, const float *y, const float *g0, const float *g1)
{
    float gl = g0[0], gr = g0[1], dgl = (g1[0] - gl) * (1.0f / (float)SY_BLK), dgr = (g1[1] - gr) * (1.0f / (float)SY_BLK);
    uint32_t i;
    for (i = 0; i < SY_BLK; i++, gl += dgl, gr += dgr) {
        bl[i] += x[i] * gl;
        br[i] += y[i] * gr;
    }
}

/* a route's bus from its oscillators' (Q15 in int32, left and right interleaved; none: silence) */
static void voice_from_q(float *bl, float *br, const int32_t *q)
{
    uint32_t i;
    if (!q) {
        sy_zerof(bl, SY_BLK);
        sy_zerof(br, SY_BLK);
        return;
    }
    for (i = 0; i < SY_BLK; i++) {
        bl[i] = (float)q[2u * i] * (1.0f / 32768.0f);
        br[i] = (float)q[2u * i + 1u] * (1.0f / 32768.0f);
    }
}

/* one block of a voice added into ml / mr */
static void voice_block(sy_voice_t *v, float *ml, float *mr)
{
    static int32_t qb[2][2u * SY_BLK], pm[SY_NOSC][2u * SY_BLK];   /* the oscillators' buses: FILTER, DIRECT */
    static float fl[SY_BLK], fr[SY_BLK], dl[SY_BLK], dr[SY_BLK], x[SY_BLK], y[SY_BLK];
    const float *off = v->off;
    float amp0 = v->amp * v->fade, amp1, fade1, g[4][2], pitch;
    osc_par_t op[SY_NOSC];
    uint32_t o, i, k, ran = 0;
    int on[4], anydir = 0;
    env_par_t ep;

    /* control: offsets, envelopes, LFOs, pitch */
    mod_offsets(v->off, v->src, 0);
    for (k = 0; k < 2u; k++) {
        uint32_t b = k ? SP_E2_ATK - SP_E1_ATK : 0u;
        ep.atk = mod_get(off, SP_E1_ATK + b);
        ep.hold = mod_get(off, SP_E1_HOLD + b);
        ep.dec = mod_get(off, SP_E1_DEC + b);
        ep.sus = mod_get(off, SP_E1_SUS + b);
        ep.rel = mod_get(off, SP_E1_REL + b);
        ep.ca = (float)sy_patch.p[SP_E1_ACRV + b] * 0.001f;
        ep.cd = (float)sy_patch.p[SP_E1_DCRV + b] * 0.001f;
        ep.cr = (float)sy_patch.p[SP_E1_RCRV + b] * 0.001f;
        if (ep.hold < 1e-4f)
            ep.hold = 0.0f;
        v->src[SRC_ENV1 + k] = env_block(&v->env[k], &ep);
    }
    for (k = 0; k < 2u; k++) {
        lfo_par_t lp;
        voice_lfo_par(off, k, &lp);
        v->src[SRC_LFO1 + k] = (lp.mode == LFOM_FREE ? sy_glfo_v[k] : lfo_block(&v->lfo[k], &lp)) *
                               lfo_fade(&v->lfo[k], &lp);
    }
    if (sp_n[SP_V_GLIDE] > 0.0f && v->pitch != v->target) {   /* glide: exponential, ~98 % in GLIDE's time */
        float kk = sy_minf(1.0f, 4.0f * (float)SY_BLK / (SY_FS * sp_time_s(sp_n[SP_V_GLIDE])));
        v->pitch += (v->target - v->pitch) * kk;
        if (sy_absf(v->target - v->pitch) < 0.001f)
            v->pitch = v->target;
    } else {
        v->pitch = v->target;
    }
    amp1 = v->src[SRC_ENV1] * (1.0f - sp_n[SP_V_VEL] + sp_n[SP_V_VEL] * v->src[SRC_VEL] * v->src[SRC_VEL]);
    if (v->steal) {
        v->steal--;
        fade1 = (float)v->steal * (1.0f / (float)VOICE_STEAL_BLKS);
    } else {
        fade1 = v->fade;
    }
    pitch = v->pitch + sy_bend_v * (float)(sy_bend_v > 0.0f ? sy_patch.p[SP_V_BUP] : sy_patch.p[SP_V_BDN]);

    /* each source's settings; its switch fade: switched on or off, or its kind changed (a warp, a shape, a type,
     * the route), it fades out over 4 blocks, takes the new kind while silent, and fades in; a table being swapped
     * fades its oscillator (wt_g) */
    {
        uint8_t want[4];
        int won[4];
        for (o = 0; o < SY_NOSC; o++) {
            uint32_t b = o ? SP_B_ON - SP_A_ON : 0u;
            voice_osc_par(v, o, pitch, &op[o]);
            want[o] = (uint8_t)(op[o].warp | (sy_patch.p[SP_A_ROUTE + b] != 0) << 7);
            won[o] = sy_patch.p[SP_A_ON + b] && wt_tab[o].n;
            voice_bal(sp_lin(SP_A_PAN + b, mod_get(off, SP_A_PAN + b)) * 0.001f, mod_get(off, SP_A_LEVEL + b), g[o]);
        }
        want[VS_SUB] = (uint8_t)(sy_patch.p[SP_SUB_SHAPE] | (sy_patch.p[SP_SUB_ROUTE] != 0) << 7);
        won[VS_SUB] = sy_patch.p[SP_SUB_ON] != 0;
        voice_bal(sp_lin(SP_SUB_PAN, mod_get(off, SP_SUB_PAN)) * 0.001f, mod_get(off, SP_SUB_LEVEL), g[VS_SUB]);
        want[VS_NZ] = (uint8_t)(sy_patch.p[SP_NZ_TYPE] | (sy_patch.p[SP_NZ_ROUTE] != 0) << 7);
        won[VS_NZ] = sy_patch.p[SP_NZ_ON] != 0;
        voice_bal(sp_lin(SP_NZ_PAN, mod_get(off, SP_NZ_PAN)) * 0.001f, mod_get(off, SP_NZ_LEVEL), g[VS_NZ]);
        for (k = 0; k < 4u; k++) {
            float s0, s1, t;
            if (v->fresh) {
                v->kind[k] = want[k];
                v->son[k] = won[k] ? 1.0f : 0.0f;
            }
            s0 = v->son[k];
            t = v->kind[k] == want[k] && won[k] ? 1.0f : 0.0f;
            s1 = s0 < t ? sy_minf(t, s0 + VOICE_FADE) : sy_maxf(t, s0 - VOICE_FADE);
            g[k][0] *= s1;
            g[k][1] *= s1;
            if (v->fresh)
                v->g[k][0] = g[k][0], v->g[k][1] = g[k][1];    /* a new note: its envelope fades it in */
            else if (!(v->ran & (1u << k)))
                v->g[k][0] = v->g[k][1] = 0.0f;                /* (was not running: from 0) */
            on[k] = s0 > 0.0f || s1 > 0.0f;
            v->son[k] = s1;
        }
        for (o = 0; o < SY_NOSC; o++) {
            op[o].warp = (uint8_t)(v->kind[o] & 0x7Fu);        /* the kind playing, until it has faded */
            if (on[o] && !(v->ran & (1u << o)) && !v->fresh)
                v->osc[o].fresh = 1;                           /* no pitch or position ramps from long ago */
        }

        /* FM: each oscillator's modulator from the other's block-start state, before either runs; a table being
         * swapped takes its modulation down with its own fade */
        for (o = 0; o < SY_NOSC; o++) {
            uint32_t m = o ^ 1u;
            if (op[o].warp == WARP_FM && on[o] && wt_tab[m].n && (wt_g[m][0] > 0.0f || wt_g[m][1] > 0.0f)) {
                float s = wt_g[m][0], ds = (wt_g[m][1] - s) * (1.0f / (float)SY_BLK);
                osc_modulator(&v->osc[m], &wt_tab[m], &op[m], pm[o], !on[m]);
                if (s < 1.0f || ds != 0.0f)
                    for (i = 0; i < SY_BLK; i++, s += ds)
                        pm[o][2u * i] = (int32_t)((float)pm[o][2u * i] * s);
                op[o].pm = pm[o];
                op[o].pm_inc = op[m].inc1;
            }
        }

        /* audio: the sources into the FILTER bus (fl fr) or DIRECT (dl dr), by the route playing (DIRECT only
         * when a source takes it). The oscillators add into an integer bus a route, their level, pan and fades in
         * their unison voices' gains, made float once; the sub and the noise add in float */
        int qon[2] = {0, 0};
        for (k = 0; k < 4u; k++)
            anydir |= on[k] && (v->kind[k] >> 7);
        for (o = 0; o < SY_NOSC; o++) {
            int dir = v->kind[o] >> 7;
            float sg[4];
            if (!on[o] || (wt_g[o][0] <= 0.0f && wt_g[o][1] <= 0.0f) || !wt_tab[o].n)
                continue;
            sg[0] = v->g[o][0] * wt_g[o][0];                   /* left at the block's start and end, right */
            sg[1] = g[o][0] * wt_g[o][1];
            sg[2] = v->g[o][1] * wt_g[o][0];
            sg[3] = g[o][1] * wt_g[o][1];
            if (!qon[dir]) {
                sy_zeroi(qb[dir], 2u * SY_BLK);
                qon[dir] = 1;
            }
            osc_block(&v->osc[o], &wt_tab[o], &op[o], qb[dir], sg);
            ran |= 1u << o;
        }
        voice_from_q(fl, fr, qon[0] ? qb[0] : 0);
        if (anydir)
            voice_from_q(dl, dr, qon[1] ? qb[1] : 0);
        if (on[VS_SUB]) {
            int dir = v->kind[VS_SUB] >> 7;
            voice_sub(v, voice_inc(pitch + 12.0f * (float)sy_patch.p[SP_SUB_OCT]), v->kind[VS_SUB] & 0x7Fu,
                      mod_get(off, SP_SUB_PW), x);
            voice_mix(dir ? dl : fl, dir ? dr : fr, x, x, v->g[VS_SUB], g[VS_SUB]);
            ran |= 1u << VS_SUB;
        }
        if (on[VS_NZ]) {
            int dir = v->kind[VS_NZ] >> 7;
            voice_noise(v, v->kind[VS_NZ] & 0x7Fu, sp_lin(SP_NZ_COLOR, mod_get(off, SP_NZ_COLOR)) * 0.001f,
                        sp_n[SP_NZ_STEREO], x, y);
            voice_mix(dir ? dl : fl, dir ? dr : fr, x, y, v->g[VS_NZ], g[VS_NZ]);
            ran |= 1u << VS_NZ;
        }
        sy_copyf(v->g[0], g[0], 8u);
        v->ran = (uint8_t)ran;
        for (k = 0; k < 4u; k++)                               /* a kind change takes effect once faded out */
            if (v->son[k] <= 0.0f)
                v->kind[k] = want[k];
    }

    /* the filter: off is MIX 0; switching it, or its type, fades it over 4 blocks the same way */
    {
        uint8_t wt = (uint8_t)sy_patch.p[SP_F_TYPE];
        float f0, f1, t, m0, m1;
        if (v->fresh) {
            v->ftype = wt;
            v->fon = sy_patch.p[SP_F_ON] ? 1.0f : 0.0f;
        }
        f0 = v->fon;
        t = v->ftype == wt && sy_patch.p[SP_F_ON] ? 1.0f : 0.0f;
        f1 = f0 < t ? sy_minf(t, f0 + VOICE_FADE) : sy_maxf(t, f0 - VOICE_FADE);
        m0 = v->fresh ? mod_get(off, SP_F_MIX) * f1 : v->fmix;
        m1 = mod_get(off, SP_F_MIX) * f1;
        if (m0 > 0.0f || m1 > 0.0f) {
            flt_par_t fp;
            float kt = mod_get(off, SP_F_KEY), wet[2][SY_BLK], dm = (m1 - m0) * (1.0f / (float)SY_BLK), m = m0;
            fp.type = v->ftype;
            fp.hz = sp_cut_hz(mod_get(off, SP_F_CUT)) * sy_exp2f(kt * (v->pitch - 60.0f) * (1.0f / 12.0f));
            fp.res = mod_get(off, SP_F_RES);
            fp.drive = mod_get(off, SP_F_DRIVE);
            fp.var = mod_get(off, SP_F_VAR);
            fp.mix = 1.0f;
            if (f0 <= 0.0f && !v->fresh)
                flt_note(&v->flt);                             /* switched on: from rest */
            if (m0 >= 1.0f && m1 >= 1.0f) {
                flt_block(&v->flt, &fp, fl, fr);               /* all wet: in place */
            } else {
                sy_copyf(wet[0], fl, SY_BLK);
                sy_copyf(wet[1], fr, SY_BLK);
                flt_block(&v->flt, &fp, wet[0], wet[1]);
                for (i = 0; i < SY_BLK; i++, m += dm) {
                    fl[i] += (wet[0][i] - fl[i]) * m;
                    fr[i] += (wet[1][i] - fr[i]) * m;
                }
            }
        }
        if (f1 <= 0.0f)
            v->ftype = wt;
        v->fon = f1;
        v->fmix = m1;
    }

    /* the amp (envelope x velocity x steal fade), ramped */
    {
        float a = amp0, da = (amp1 * fade1 - amp0) * (1.0f / (float)SY_BLK);
        if (ran && anydir)
            for (i = 0; i < SY_BLK; i++, a += da) {
                ml[i] += (fl[i] + dl[i]) * a;
                mr[i] += (fr[i] + dr[i]) * a;
            }
        else if (ran)
            for (i = 0; i < SY_BLK; i++, a += da) {
                ml[i] += fl[i] * a;
                mr[i] += fr[i] * a;
            }
    }
    v->amp = amp1;
    v->fade = fade1;
    v->fresh = 0;
    if (v->env[0].st == ENV_IDLE || fade1 <= 0.0f)
        v->on = 0;                                             /* (a stolen voice restarts in sy_engine.c) */
}
