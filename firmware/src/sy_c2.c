/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* CORE 2 (SETTINGS > CORE 2; the console's `bench`, its c2_ lines): the FM-1's second core measured, before the
 * looper's V0.9 puts voices on it (docs/LOOPER.md, stage 0).
 *
 * ON (the setting) starts core 1 at the next power-on, as Felucca does (main.c: before the audio and the timers; started
 * later it never reaches its entry), with Felucca's mailbox (hal/fm1_cpu1.h, from X0X, Melodee and Jangada: no supply
 * raise). A unit whose core 1 does not answer, misreads a job or stops answering is held and runs on one core, and ON
 * is cleared (the event log says which). Never after a boot that crashed or failed (SAFE START).
 *
 * TEST runs 30 s in phases of a second each, in turn, in the audio interrupt:
 *   IDLE  core 1 waits: core 0's render of the half alone (the baseline)
 *   PAR   core 1 renders LOAD unison voices of wavetable oscillators (the voices' kernels, osc_run: four oscillators
 *         of their own phases and buffer, reading table A, the output thrown away) while core 0 renders the half; then
 *         core 0 waits for it. Core 1 times itself
 *   SOLO  core 0 renders the same oscillators itself, after the half
 * So: core 1's time for the work against core 0's (SOLO), how much core 0's own render slows while core 1 runs (PAR
 * against IDLE and SOLO's renders: the shared bus and caches), and how long core 0 waits. The load the CPU guard sees
 * leaves the test's part out (it would trim the sound being measured). Core 1 works only inside the audio interrupt,
 * which waits for it before it returns: the flash is never off under it (fm1_cpu1.h). */
#ifndef STRATA_CPU2
#define STRATA_CPU2 0                            /* (the emulator: one core) */
#endif
#define C2_NOSC 4u                               /* the test's oscillators */
#define C2_SECS 30u                              /* TEST's length */
#define C2_HALF 128u                             /* a half's samples (sy_app.c SY_HALF, asserted there) */
#define C2_HALVES_S 345u                         /* halves a second (44,118 / 128) */
#define C2_LOAD0 16u                             /* LOAD's default: unison voices in all (4 .. 32, the 4 share them) */
enum { C2_OFF, C2_ON, C2_NOANSWER, C2_MISREAD, C2_STALLED, C2_ONECORE, C2_SAFE, C2_NSTATE };
static const char *const C2_STATE[C2_NSTATE] = {"OFF", "ANSWERING", "NO ANSWER: OFF", "MISREAD: OFF", "STOPPED: OFF",
                                                "ONE CORE BUILD", "SAFE START: NOT ON"};
enum { C2_IDLE, C2_PAR, C2_SOLO, C2_NPH };

#define C2_NUNI 8u                               /* unison voices an oscillator at most (LOAD 32: 4 x 8) */
static uint32_t c2_ph[C2_NOSC][C2_NUNI] SY_POOL;   /* their phases, increments (the pool is set at power-on: c2_boot) */
static uint32_t c2_inc[C2_NOSC][C2_NUNI] SY_POOL;
static int32_t c2_lr[2u * SY_BLK] SY_POOL;
static struct {
    uint8_t on;                                  /* the setting: start core 1 at power-on */
    uint8_t load;                                /* LOAD: unison voices in all, 4 .. 32 */
    uint8_t state;                               /* C2_*: what this power-on found */
    uint8_t told;                                /* the main loop has said what the interrupt found (a give-up) */
    volatile uint8_t test;                       /* 1: TEST running (the main loop starts it, the interrupt ends it) */
    volatile uint8_t done;                       /* .. ran to its end since power-on */
    volatile uint8_t busy;                       /* PAR: core 1 has this half's job */
    uint32_t halves;                             /* the test's halves so far */
    uint32_t r_sum[C2_NPH], r_n[C2_NPH];         /* core 0's render of a half, by phase (ticks; halves) */
    uint32_t j0_sum, j0_n;                       /* SOLO: the oscillators on core 0 */
    uint32_t j1_sum, j1_n, j1_max;               /* PAR: on core 1, its own time */
    uint32_t wait_max;                           /* PAR: core 0's longest wait after its render */
    volatile uint32_t j1_ticks;                  /* core 1's: its last job's time */
} c2;

static void c2_osc_init(void)
{
    static const uint8_t NOTE[C2_NOSC] = {48, 55, 60, 67};   /* a chord's spread of mip levels */
    uint32_t i, j;
    for (i = 0; i < C2_NOSC; i++)
        for (j = 0; j < C2_NUNI; j++) {
            c2_inc[i][j] = voice_inc((float)NOTE[i] + 0.1f * ((float)j - 3.5f));   /* +-35 cents of detune */
            c2_ph[i][j] = j * 0x20000000u;
        }
}

/* the work: a half (four blocks) of the four oscillators, n unison voices among them, through the oscillators' own
 * kernels (osc_run: the plain wavetable read of two frames crossfaded, WT POS sweeping as a moving position plays).
 * osc_run, not osc_block: the voices' code is built as before (osc_block stays inlined into the voice, its only
 * caller). On core 1 (PAR) or core 0 (SOLO), never both at once */
static SY_NOINLINE void c2_job(uint32_t n)
{
    uint32_t t0 = fm1_ticks(), b, i, j, k;
    wt_table_t t = wt_tab[0];
    if (!t.busy && t.n) {
        float top = (float)(t.n - 1u), dp = top * (1.0f / 1024.0f / (float)SY_BLK);
        for (b = 0; b < C2_HALF / SY_BLK; b++) {
            float p0 = top * (float)((c2.halves * 4u + b) % 1024u) * (1.0f / 1024.0f);
            for (k = 0; k < 2u * SY_BLK; k++)
                c2_lr[k] = 0;
            for (i = 0; i < C2_NOSC; i++) {
                uint32_t u = n / C2_NOSC + (i < n % C2_NOSC);
                for (j = 0; j < u && j < C2_NUNI; j++) {
                    osc_job_t jb;
                    uint32_t lv = wt_level(c2_inc[i][j]);
                    memset(&jb, 0, sizeof jb);
                    jb.ph = c2_ph[i][j];
                    jb.inc = c2_inc[i][j];
                    jb.gl = jb.gr = 4096;
                    c2_ph[i][j] = osc_run(OSK_PLAIN, &jb, &t, lv < WT_NLEV ? lv : WT_NLEV - 1u, p0, dp, c2_lr);
                }
            }
        }
    }
    c2.j1_ticks = fm1_ticks() - t0;
}

static uint32_t c2_phase(void) { return (c2.halves / C2_HALVES_S) % C2_NPH; }

/* power-on, the settings read, before the audio: core 1 started when ON (and not SAFE START) */
static void c2_boot(int safe)
{
    c2_osc_init();
    if (!c2.load)
        c2.load = C2_LOAD0;
#if STRATA_CPU2
    if (!c2.on) {
        c2.state = C2_OFF;
    } else if (safe) {
        c2.state = C2_SAFE;
    } else {
        int rc;
        fm1_guard_pc_open();                     /* (core 1 starts in the chip's ROM: the PC limits open meanwhile) */
        rc = fm1_cpu1_start();
        fm1_guard_enable(FM1_GUARD_PC);
        c2.state = rc ? C2_NOANSWER : C2_ON;
        if (rc)
            c2.on = 0;
    }
#else
    (void)safe;
    c2.state = C2_ONECORE;
#endif
}

/* the audio interrupt, before the half's render: PAR hands core 1 the oscillators */
static inline void c2_half_start(void)
{
#if STRATA_CPU2
    if (c2.test && c2_phase() == C2_PAR && fm1_c1_on)
        c2.busy = (uint8_t)fm1_cpu1_run(c2_job, c2.load);
#endif
}

/* .. after it (r: the render's ticks): PAR waits for core 1, SOLO runs the oscillators on core 0. The ticks this took
 * (left out of the guard's load) */
static SY_NOINLINE uint32_t c2_half_end_test(uint32_t r);
static inline uint32_t c2_half_end(uint32_t r) { return c2.test ? c2_half_end_test(r) : 0u; }
static SY_NOINLINE uint32_t c2_half_end_test(uint32_t r)
{
    uint32_t t0, d = 0, ph;
    ph = c2_phase();
    t0 = fm1_ticks();
#if STRATA_CPU2
    if (c2.busy) {
        c2.busy = 0;
        if (fm1_cpu1_wait() == 0) {
            uint32_t j = c2.j1_ticks;
            d = fm1_ticks() - t0;
            c2.j1_sum += j, c2.j1_n++;
            if (j > c2.j1_max)
                c2.j1_max = j;
            if (d > c2.wait_max)
                c2.wait_max = d;
        } else {                                 /* (it misread or stopped: held, one core; the main loop tells) */
            c2.state = fm1_c1_gave_up == 1u ? C2_MISREAD : C2_STALLED;
            c2.test = 0;
            return fm1_ticks() - t0;
        }
    } else
#endif
    if (ph == C2_SOLO) {
        c2_job(c2.load);
        d = fm1_ticks() - t0;
        c2.j0_sum += d, c2.j0_n++;
    }
    c2.r_sum[ph] += r, c2.r_n[ph]++;
    if (++c2.halves >= C2_SECS * C2_HALVES_S)
        c2.test = 0, c2.done = 1;
    return d;
}

/* TEST: started (the sums cleared) or stopped. 0: started; -1: core 1 is not answering */
static int c2_test(int start)
{
    if (!start) {
        c2.test = 0;
        return 0;
    }
    if (c2.state != C2_ON)
        return -1;
    c2.test = 0;
    c2.busy = 0;
    memset(c2.r_sum, 0, sizeof c2.r_sum);
    memset(c2.r_n, 0, sizeof c2.r_n);
    c2.j0_sum = c2.j0_n = c2.j1_sum = c2.j1_n = c2.j1_max = c2.wait_max = 0;
    c2.halves = 0;
    c2.done = 0;
    c2.test = 1;
    return 0;
}

/* the figures, in microseconds: core 1's average and longest job, core 0's average for the same work, core 0's render
 * alone and while core 1 ran, core 0's longest wait (0: none yet) */
static uint32_t c2_us(uint32_t sum, uint32_t n) { return n ? sum / n / FM1_TICKS_PER_US : 0u; }
static uint32_t c2_c1_us(void) { return c2_us(c2.j1_sum, c2.j1_n); }
static uint32_t c2_c1_max_us(void) { return c2.j1_max / FM1_TICKS_PER_US; }
static uint32_t c2_c0_us(void) { return c2_us(c2.j0_sum, c2.j0_n); }
static uint32_t c2_alone_us(void)
{
    return c2_us(c2.r_sum[C2_IDLE] + c2.r_sum[C2_SOLO], c2.r_n[C2_IDLE] + c2.r_n[C2_SOLO]);
}
static uint32_t c2_with_us(void) { return c2_us(c2.r_sum[C2_PAR], c2.r_n[C2_PAR]); }
static int32_t c2_slowed_pct(void)               /* core 0's render while core 1 ran, against alone: +% */
{
    uint32_t a = c2_alone_us(), w = c2_with_us();
    return a && w ? (int32_t)(w * 100u / a) - 100 : 0;
}
static uint32_t c2_stack(void)                   /* bytes of core 1's stack ever used */
{
#if STRATA_CPU2
    return fm1_cpu1_stack_used();
#else
    return 0;
#endif
}
static uint32_t c2_bad(void)                     /* jobs core 1 would not run (a misread: then held) */
{
#if STRATA_CPU2
    return fm1_c1_mb.bad;
#else
    return 0;
#endif
}
static uint32_t c2_secs_left(void)
{
    uint32_t h = c2.halves;
    return c2.test ? (C2_SECS * C2_HALVES_S - (h < C2_SECS * C2_HALVES_S ? h : C2_SECS * C2_HALVES_S)) / C2_HALVES_S + 1u
                   : 0u;
}
