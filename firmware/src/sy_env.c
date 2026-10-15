/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA envelopes (docs/ARCHITECTURE.md §10): ATTACK HOLD DECAY SUSTAIN RELEASE, 1 ms .. 30 s, and a curve for
 * each of attack, decay and release. Evaluated once a block (the voice ramps the amp linearly across it). A
 * stage's linear position t (0..1) is shaped by f(t) = t / (t + (1 - t) K), K = 2^(4 c) for its curve c (-1..1):
 * 0 straight, below 0 fast at first (an RC's exponential: the default for decay and release), above 0 slow at
 * first; bounded, exact at both ends, one divide. A note (or a retrigger) attacks from the current level and a
 * release falls from it: reused or retriggered voices never jump. */
enum { ENV_IDLE, ENV_ATK, ENV_HOLD, ENV_DEC, ENV_SUS, ENV_REL };

typedef struct {
    float v;                     /* the value at the end of the last block */
    float t;                     /* the position in the stage, 0..1 */
    float from;                  /* the level the stage started from (attack, release) */
    uint8_t st;                  /* ENV_* */
} env_t;

typedef struct {                 /* an envelope's settings for a block (normalised values, modulated) */
    float atk, hold, dec, sus, rel;   /* 0..1 */
    float ca, cd, cr;                 /* curves -1..1 */
} env_par_t;

static inline float env_curve(float t, float c)
{
    float k = sy_exp2f(4.0f * c);
    return t / (t + (1.0f - t) * k);
}

/* the part of a stage one block takes: a block / the stage's time (U_TIME: 1 ms .. 30 s) */
static inline float env_step(float n) { return (float)SY_BLK / (SY_FS * sp_time_s(n)); }

static void env_gate(env_t *e, int on)
{
    if (on) {
        e->st = ENV_ATK;
        e->from = e->v;
    } else if (e->st != ENV_IDLE) {
        e->st = ENV_REL;
        e->from = e->v;
    }
    e->t = 0.0f;
}

/* one block: the value at its end */
static float env_block(env_t *e, const env_par_t *p)
{
    float s = p->sus;
    switch (e->st) {
    case ENV_ATK:
        e->t += env_step(p->atk);
        if (e->t < 1.0f) {
            e->v = e->from + (1.0f - e->from) * env_curve(e->t, p->ca);
            break;
        }
        e->t = 0.0f;
        e->v = 1.0f;
        e->st = p->hold > 0.0f ? ENV_HOLD : ENV_DEC;
        break;
    case ENV_HOLD:
        e->t += env_step(p->hold);
        e->v = 1.0f;
        if (e->t >= 1.0f) {
            e->t = 0.0f;
            e->st = ENV_DEC;
        }
        break;
    case ENV_DEC:
        e->t += env_step(p->dec);
        if (e->t < 1.0f) {
            e->v = s + (1.0f - s) * (1.0f - env_curve(e->t, p->cd));
            break;
        }
        e->st = ENV_SUS;
        /* fall through */
    case ENV_SUS:
        e->v = s;
        break;
    case ENV_REL:
        e->t += env_step(p->rel);
        if (e->t < 1.0f) {
            e->v = e->from * (1.0f - env_curve(e->t, p->cr));
            break;
        }
        e->st = ENV_IDLE;
        e->v = 0.0f;
        break;
    default:
        e->v = 0.0f;
        break;
    }
    return e->v;
}
