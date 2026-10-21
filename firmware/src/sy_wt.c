/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* STRATA wavetables: the frame layout, the pool, the level an increment plays, and the swap of a table under
 * the oscillators (docs/ARCHITECTURE.md §6).
 * A frame is one single cycle as nine band-limited mip levels: level L keeps harmonics 1 .. 256 >> L in WT_N(L)
 * samples, plus one guard sample (a copy of sample 0: the interpolation reads i + 1 without a mask). Linear
 * interpolation leaves images of each harmonic around multiples of the level's own rate, about 21 dB under it at
 * 4x oversampling, 34 dB at 8x, 46 at 16x; they fold back as aliases. A level with few harmonics has loud top
 * harmonics, so the small levels get more oversampling (they cost little): 4x at levels 0 and 1, 8x at 2 and
 * 3, 16x at 4 and 5, 32x above (tests/sy_osc_test.c measures every table). 2,921 int16 a frame. The pool holds
 * 32 frames: OSC A's 16 and OSC B's 16, each oscillator's table in its own half (two oscillators on the same
 * table each build it). Tables are built in the main loop (sy_wtgen.c), never in the audio ISR: while one is
 * rebuilt its oscillator fades out over a block, stays silent, and fades back in over a block. */
#define WT_NLEV 9u
#define WT_FRAME 2921u
#define WT_POOL (SY_NOSC * SY_NFRAME)
static const uint16_t WT_OFF[WT_NLEV] = {0, 1025, 1538, 2051, 2308, 2565, 2694, 2823, 2888};
static const uint8_t WT_BITS[WT_NLEV] = {10, 9, 9, 8, 8, 7, 7, 6, 5};
#define WT_N(l) (1u << WT_BITS[l])
_Static_assert(2888u + 32u + 1u == WT_FRAME, "WT_OFF: nine levels and their guards");

/* the largest increment level 0 plays: its 256th harmonic at or under 21 kHz (0.476 fs: 21000 / 44117.647);
 * each level doubles it. Above level 8's (a sine at 21 kHz) the oscillator is silent */
#define WT_INC_L0 7985954u

static int16_t wt_pool[WT_POOL][WT_FRAME] SY_POOL;

typedef struct {
    const int16_t *f;                /* frame 0; frame k at f + k * WT_FRAME */
    uint8_t n;                       /* frames, 1 .. 16 */
    uint8_t id;                      /* the factory table in it */
    volatile uint8_t busy;           /* main loop: being rebuilt; the oscillators fade out and stop reading it */
    volatile uint8_t quiet;          /* audio: nothing reads it any more (set while busy) */
} wt_table_t;
static wt_table_t wt_tab[SY_NOSC];

/* the level for an increment (2^32 = one cycle a sample): the lowest whose top harmonic stays <= 21 kHz;
 * WT_NLEV = too high to play */
static uint32_t wt_level(uint32_t inc)                  /* the smallest l with inc <= WT_INC_L0 << l, or WT_NLEV: */
{                                                       /* how many of the 9 limits inc is over (4 compares) */
#define WT_LIM(l) (WT_INC_L0 << (l))
    if (inc > WT_LIM(4))
        return inc > WT_LIM(6) ? (inc > WT_LIM(7) ? (inc > WT_LIM(8) ? 9u : 8u) : 7u) : (inc > WT_LIM(5) ? 6u : 5u);
    return inc > WT_LIM(1) ? (inc > WT_LIM(2) ? (inc > WT_LIM(3) ? 4u : 3u) : 2u) : (inc > WT_LIM(0) ? 1u : 0u);
#undef WT_LIM
}

/* a level of frame k of a table */
static inline const int16_t *wt_frame(const wt_table_t *t, uint32_t k, uint32_t l)
{
    return t->f + k * WT_FRAME + WT_OFF[l];
}
