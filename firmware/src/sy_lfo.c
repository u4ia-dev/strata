/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* STRATA LFOs (docs/ARCHITECTURE.md §10): twelve shapes and a drawn one (the patch's points, sy_param.c sp_draw_t),
 * a rate in Hz or a note value of the tempo, three modes (FREE: one phase for every voice; RETRIG: a phase per voice
 * from PHASE at each note; ENV: one cycle from the note, then its last value), a delay and a rise (fade in) from
 * the note, smoothing. Evaluated once a block, bipolar -1..1. */
enum { LFO_SINE, LFO_TRI, LFO_SAWUP, LFO_SAWDN, LFO_SQUARE, LFO_SH, LFO_SMOOTH, LFO_STEP4, LFO_STAIRS, LFO_PULSE,
       LFO_BOUNCE, LFO_TRAPEZ, LFO_DRAW, LFO_NSHAPE };
static const char *const LFO_NAMES[LFO_NSHAPE] = {"SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "S&H",
                                                  "SMOOTH", "STEP 4", "STAIRS", "PULSE", "BOUNCE", "TRAPEZ",
                                                  "DRAWN"};
enum { LFOM_FREE, LFOM_RETRIG, LFOM_ENV };
_Static_assert(LFO_DRAW == SP_NSHAPE_PTS, "sy_param.c's SP_SHAPE_PTS: every shape but DRAWN, in this order");

/* a drawn shape as the audio side reads it: each point's place (0..1) and value, and the segment from it: its
 * curve's 2^(4 c) and 1 / its width (0: a step) */
typedef struct {
    uint32_t n;
    float x[SP_NPT], y[SP_NPT], k[SP_NPT], w[SP_NPT];
} lfo_draw_t;
static lfo_draw_t lfo_dr[2];                 /* the LFOs' drawn shapes (audio side) */
static volatile uint32_t sy_draw_seq;        /* the main loop's edits of sy_patch.draw: odd while one is under way */
static uint32_t lfo_dr_seq;                  /* .. the edit lfo_dr is of (odd: none, take the patch's again) */

typedef struct {
    uint32_t ph;                 /* phase, 2^32 a cycle */
    float v;                     /* the output at the end of the last block (smoothed) */
    float r0, r1;                /* S&H / SMOOTH: this cycle's random value, the next's */
    float t;                     /* seconds since the note (delay, rise) */
    uint32_t seed;
    uint8_t done;                /* ENV: the cycle is over */
} lfo_t;

typedef struct {                 /* an LFO's settings for a block */
    uint8_t shape, mode, sync;       /* (sync: FIXED, its rate a note value) */
    uint8_t note;                    /* FIXED: the note value (NOTE_Q), RATE's modulation stepped in */
    float rate;                  /* 0..1 (U_LFOHZ), modulated */
    float delay, rise, phase, smooth;   /* 0..1 */
    const lfo_draw_t *dr;        /* its drawn shape */
} lfo_par_t;

static float sy_tempo = 120.0f;  /* quarter notes a minute: GLOBAL's BPM and TS-NOTE, or the MIDI clock */
static float sy_bar_q = 4.0f;    /* a bar in quarter notes: GLOBAL's TS-BEAT and TS-NOTE (sy_set_tempo) */
static float sy_beat_q = 1.0f;   /* a beat (TS-NOTE) in quarter notes (V0.8: the looper's click, its positions) */

/* note value i's length in quarter notes (the bars by the time signature) */
static float sy_note_q(uint32_t i)
{
    if (i >= SY_NNOTE)
        i = SY_NNOTE - 1u;
    return i < 4u ? (float)(8u >> i) * sy_bar_q : NOTE_Q[i];
}

/* ---------------------------------------------------------------------------------- the drawn shape --- */
/* the patch's points as the audio side reads them (whatever they hold: n, the places' order and the ranges kept) */
static void lfo_draw_of(lfo_draw_t *o, const sp_draw_t *d)
{
    uint32_t i, n = d->n < 2u ? 2u : d->n > SP_NPT ? SP_NPT : d->n;
    float x0 = 0.0f;
    o->n = n;
    for (i = 0; i < n; i++) {
        float x = i + 1u == n ? 1.0f : sy_clampf((float)d->x[i] * 0.001f, x0, 1.0f);
        o->x[i] = x0 = i ? x : 0.0f;
        o->y[i] = sy_clampf((float)d->y[i] * 0.001f, -1.0f, 1.0f);
        o->k[i] = sy_exp2f(4.0f * sy_clampf((float)d->c[i] * 0.001f, -1.0f, 1.0f));
    }
    for (i = 0; i + 1u < n; i++)
        o->w[i] = o->x[i + 1u] > o->x[i] ? 1.0f / (o->x[i + 1u] - o->x[i]) : 0.0f;
    o->w[n - 1u] = 0.0f;
}

/* the value at x (0..1): the segment holding x (a step: none holds it, the next one does), along its curve */
static float lfo_draw_at(const lfo_draw_t *d, float x)
{
    uint32_t i = 1;
    float u;
    while (i + 1u < d->n && x >= d->x[i])
        i++;
    u = sy_clampf((x - d->x[i - 1u]) * d->w[i - 1u], 0.0f, 1.0f);
    u = u / (u + (1.0f - u) * d->k[i - 1u]);
    return d->y[i - 1u] + (d->y[i] - d->y[i - 1u]) * u;
}

/* audio side, at a block's start: the patch's drawn shapes when they changed, and only whole (an edit under way
 * waits: the last ones play on) */
static void lfo_draw_sync(void)
{
    uint32_t s = sy_draw_seq;
    if ((s & 1u) || s == lfo_dr_seq)
        return;
    lfo_draw_of(&lfo_dr[0], &sy_patch.draw[0]);
    lfo_draw_of(&lfo_dr[1], &sy_patch.draw[1]);
    lfo_dr_seq = sy_draw_seq == s ? s : 1u;  /* (written meanwhile, from another core: again next block) */
}

/* main loop: sy_patch.draw is written between these */
static void sy_draw_edit(void)
{
    sy_draw_seq++;
    SY_CBAR();
}
static void sy_draw_done(void)
{
    SY_CBAR();
    sy_draw_seq++;
}

/* the shape's value at phase x (0..1) */
static float lfo_shape(const lfo_t *l, const lfo_par_t *p, float x)
{
    switch (p->shape) {
    case LFO_SINE: return sy_sin2pi(x);
    case LFO_TRI: return x < 0.25f ? 4.0f * x : x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f;
    case LFO_SAWUP: return 2.0f * x - 1.0f;
    case LFO_SAWDN: return 1.0f - 2.0f * x;
    case LFO_SQUARE: return x < 0.5f ? 1.0f : -1.0f;
    case LFO_SH: return l->r0;
    case LFO_SMOOTH: {                                   /* cosine between the random points */
        float c = 0.5f - 0.5f * sy_cos2pi(0.5f * x);
        return l->r0 + (l->r1 - l->r0) * c;
    }
    case LFO_STEP4: return 1.0f - (2.0f / 3.0f) * (float)sy_floori(x * 4.0f);
    case LFO_STAIRS: return -1.0f + (2.0f / 7.0f) * (float)sy_floori(x * 8.0f);
    case LFO_PULSE: return x < 0.25f ? 1.0f : -1.0f;
    case LFO_BOUNCE: {                                   /* four bounces, each lower */
        float b = x * 4.0f, f = b - (float)sy_floori(b);
        return 2.0f * sy_absf(sy_sin2pi(0.5f * f)) * (1.0f - 0.22f * (float)sy_floori(b)) - 1.0f;
    }
    case LFO_TRAPEZ:                                     /* up, hold, down, hold */
        return x < 0.125f ? 8.0f * x * 2.0f - 1.0f : x < 0.5f ? 1.0f : x < 0.625f ? 1.0f - 16.0f * (x - 0.5f) : -1.0f;
    default:                                             /* LFO_DRAW */
        return lfo_draw_at(p->dr, x);
    }
}

/* cycles a second */
static float lfo_hz(const lfo_par_t *p)
{
    if (p->sync)
        return sy_tempo * (1.0f / 60.0f) / sy_note_q(p->note);
    return sp_lfo_hz(p->rate);
}

static void lfo_note(lfo_t *l, const lfo_par_t *p)
{
    if (p->mode != LFOM_FREE)
        l->ph = osc_phase(p->phase);
    l->t = 0.0f;
    l->done = 0;
}

/* one block: the phase on, a new random value at each cycle; returns the value at the block's end. For FREE,
 * the global LFO runs this and the voices take its value through lfo_voice */
static float lfo_block(lfo_t *l, const lfo_par_t *p)
{
    float hz = lfo_hz(p), x, y;
    uint32_t inc = (uint32_t)(int32_t)sy_minf(hz * (float)SY_BLK / SY_FS * 4294967296.0f, 2147483647.0f);
    uint32_t old = l->ph;
    if (!l->done) {
        l->ph += inc;
        if (l->ph < old) {                               /* a cycle ended */
            if (p->mode == LFOM_ENV) {
                l->done = 1;
                l->ph = 0xFFFFFFFFu;
            }
            l->r0 = l->r1;
            l->r1 = sy_randf(sy_rand(&l->seed));
        }
    }
    x = (float)(l->ph >> 8) * (1.0f / 16777216.0f);
    if (p->mode == LFOM_FREE)
        x += p->phase, x -= (float)sy_floori(x);
    y = lfo_shape(l, p, x);
    if (p->smooth > 0.0f) {                              /* SMOOTH: a one-pole, up to ~0.3 s */
        float k = sy_exp2f(-8.0f * p->smooth);
        y = l->v + (y - l->v) * k;
    }
    l->v = y;
    return y;
}

/* the fade-in from a note: DELAY, then RISE (0..1 normalised U_TIME0) */
static float lfo_fade(lfo_t *l, const lfo_par_t *p)
{
    float d = p->delay > 0.0f ? sp_time_s(p->delay) : 0.0f, r = p->rise > 0.0f ? sp_time_s(p->rise) : 0.0f;
    l->t += (float)SY_BLK / SY_FS;
    if (l->t < d)
        return 0.0f;
    return r > 0.0f ? sy_minf(1.0f, (l->t - d) / r) : 1.0f;
}
