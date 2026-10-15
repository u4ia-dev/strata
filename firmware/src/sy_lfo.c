/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA LFOs (docs/ARCHITECTURE.md §10): twelve shapes, a rate in Hz or a note value of the tempo, three modes
 * (FREE: one phase for every voice; RETRIG: a phase per voice from PHASE at each note; ENV: one cycle from the
 * note, then its last value), a delay and a rise (fade in) from the note, smoothing. Evaluated once a block,
 * bipolar -1..1. */
enum { LFO_SINE, LFO_TRI, LFO_SAWUP, LFO_SAWDN, LFO_SQUARE, LFO_SH, LFO_SMOOTH, LFO_STEP4, LFO_STAIRS, LFO_PULSE,
       LFO_BOUNCE, LFO_TRAPEZ, LFO_NSHAPE };
static const char *const LFO_NAMES[LFO_NSHAPE] = {"SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "S&H",
                                                  "SMOOTH", "STEP 4", "STAIRS", "PULSE", "BOUNCE", "TRAPEZ"};
enum { LFOM_FREE, LFOM_RETRIG, LFOM_ENV };

typedef struct {
    uint32_t ph;                 /* phase, 2^32 a cycle */
    float v;                     /* the output at the end of the last block (smoothed) */
    float r0, r1;                /* S&H / SMOOTH: this cycle's random value, the next's */
    float t;                     /* seconds since the note (delay, rise) */
    uint32_t seed;
    uint8_t done;                /* ENV: the cycle is over */
} lfo_t;

typedef struct {                 /* an LFO's settings for a block */
    uint8_t shape, mode, sync;
    float rate;                  /* 0..1 (U_LFOHZ), modulated */
    float delay, rise, phase, smooth;   /* 0..1 */
} lfo_par_t;

static float sy_tempo = 120.0f;  /* beats a minute: the internal clock or the MIDI clock (sy_engine.c) */

/* the shape's value at phase x (0..1) */
static float lfo_shape(const lfo_t *l, uint32_t shape, float x)
{
    switch (shape) {
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
    default:                                             /* LFO_TRAPEZ: up, hold, down, hold */
        return x < 0.125f ? 8.0f * x * 2.0f - 1.0f : x < 0.5f ? 1.0f : x < 0.625f ? 1.0f - 16.0f * (x - 0.5f) : -1.0f;
    }
}

/* cycles a second */
static float lfo_hz(const lfo_par_t *p)
{
    if (p->sync && p->sync < 18u)
        return sy_tempo * (1.0f / 60.0f) / SYNC_BEATS[p->sync];
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
    y = lfo_shape(l, p->shape, x);
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
