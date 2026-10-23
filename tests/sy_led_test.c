/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* Host test of Strata's LED levels (V0.87, U9: hal/fm1_input.h with FM1_LED_Q 1): the scan's tick in quarters, TIMER5
 * at 40 kHz, fm1_input_tick every fourth interrupt and fm1_input_quarter(1 .. 3) between. The GPIO is memory, every
 * write of the LED lines is traced with its time (TIMER4 ticks, 24 a us: a read of the clock 1 us, a bus access 1/6),
 * and each LED's lit time a frame is summed from the trace. Checked, for LIGHTS HIGH, MED, LOW and OFF:
 *   - lit, mid and the glow each their level: lit the whole tick (HIGH), half (MED), a quarter (LOW); mid half of
 *     lit's quarters (LOW: the glow's pulse); the glow a pulse of ~3.2 / 2.4 / 1.6 us; OFF nothing at all;
 *   - steady: every frame the same lit time (+-1 us), so nothing is lit one frame and dark the next (V0.86's mid and
 *     the breath's low end were: 22 .. 114 Hz, the flicker seen on the FM-1);
 *   - each LED only on its own column, the lines dark at every 595 latch;
 *   - the breath: from the glow up to lit's quarters and back over FM1_LED_BREATH_FRAMES, a step at a time (monotonic
 *     each way, never one level and back within 8 frames: no dither);
 *   - a quarter writes the lines at most once, and not at all when nothing ends there.
 * Not shown here: how bright or steady the panel looks (needs a hardware test). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t host_now;                          /* TIMER4 ticks (24 MHz) */
#define FM1_INPUT_NOW() host_now
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_time.h"
#include "../firmware/hal/fm1_gpio.h"
static volatile uint32_t host_reg[16][64];        /* the GPIO registers, as memory */
static uint32_t host_ticks(void) { return host_now += 24u; }
static uint32_t lines_now, lines_t, col_now, latch_lit, writes;
static uint64_t on_t[11][5];                       /* each LED's lit time (ticks) since the last clear */
static void led_trace(uint32_t m)
{
    uint32_t r;
    for (r = 1; r < 5u; r++)
        if ((lines_now >> r) & 1u)
            on_t[col_now % 11u][r] += host_now - lines_t;
    lines_now = m;
    lines_t = host_now;
    writes++;
}
#define FM1_LED_TRACE(m) led_trace(m)
#define FM1_SR_LATCH_TRACE() (latch_lit |= lines_now, col_now = (col_now + 1u) % 11u)
#undef FM1_PR
#define FM1_PR(p, r) (*(host_now += 4u, &host_reg[(p) & 15u][((r) / 4u) & 63u]))
#define fm1_ticks host_ticks
#define FM1_LED_Q 1
#include "../firmware/hal/fm1_input.h"

static int fails;
static int check(const char *what, int ok)
{
    printf("led: %-96s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
    return ok;
}
static uint32_t tick_n;
static void run_ticks(uint32_t n)                  /* n ticks, each with its three quarters, 25 us apart */
{
    uint32_t i, q;
    for (i = 0; i < n; i++, tick_n++) {
        uint32_t base = tick_n * 2400u;
        if (host_now < base)
            host_now = base;
        fm1_input_tick();
        for (q = 1; q < 4u; q++) {
            uint32_t w0 = writes;
            if (host_now < base + 600u * q)
                host_now = base + 600u * q;
            fm1_input_quarter(q);
            if (writes - w0 > 1u)
                fails += !check("a quarter writes the lines once at most", 0);
        }
    }
}
static void clear_on(void)
{
    led_trace(lines_now);
    memset(on_t, 0, sizeof on_t);
}
static float frame_us(uint32_t c, uint32_t r, uint32_t frames)   /* (after clear_on and run_ticks of frames) */
{
    led_trace(lines_now);
    return (float)on_t[c][r] / 24.0f / (float)frames;
}
/* the lit time of (c, r) in each of n frames: its least and most */
static void per_frame(uint32_t c, uint32_t r, uint32_t n, float *lo, float *hi)
{
    uint32_t f;
    *lo = 1e9f, *hi = 0.0f;
    for (f = 0; f < n; f++) {
        float x;
        clear_on();
        run_ticks(FM1_NCOL);
        x = frame_us(c, r, 1u);
        *lo = x < *lo ? x : *lo;
        *hi = x > *hi ? x : *hi;
    }
}
static void leds_clear(void)
{
    memset(fm1_led, 0, sizeof fm1_led);
    memset(fm1_led_mid, 0, sizeof fm1_led_mid);
    memset(fm1_led_dim, 0, sizeof fm1_led_dim);
    memset(fm1_led_breath, 0, sizeof fm1_led_breath);
}

int main(void)
{
    static const char *const NAME[4] = {"OFF", "LOW", "MED", "HIGH"};
    float lit[4], mid[4], glow[4], lo, hi;
    uint32_t lv, c, r;
    char t[160];
    host_now = 1u;
    run_ticks(64);                                 /* (the scan settled: the dim pulse's bits found) */
    for (lv = 0; lv < 4u; lv++) {                  /* lit on column 3 row 1, mid on column 5 row 2, the glow column 7 */
        leds_clear();                              /* row 3 */
        fm1_led_level(lv);
        fm1_led[3] = 1u << 1;
        fm1_led_mid[5] = 1u << 2;
        fm1_led_dim[7] = 1u << 3;
        run_ticks(64 * FM1_NCOL);                  /* (the glow's pulse settles to its new width) */
        latch_lit = 0;
        clear_on();
        run_ticks(64 * FM1_NCOL);
        lit[lv] = frame_us(3, 1, 64), mid[lv] = frame_us(5, 2, 64), glow[lv] = frame_us(7, 3, 64);
        printf("led:   %-4s lit %5.1f us a frame, mid %5.1f, the glow %4.2f (a tick 100 us)\n", NAME[lv], lit[lv], mid[lv],
               glow[lv]);
        {
            uint64_t other = 0;
            for (c = 0; c < FM1_NCOL; c++)
                for (r = 1; r < 5u; r++)
                    if (!(c == 3u && r == 1u) && !(c == 5u && r == 2u) && !(c == 7u && r == 3u))
                        other += on_t[c][r];
            snprintf(t, sizeof t, "%s: each LED lit only on its own column and row, the lines dark at every latch",
                     NAME[lv]);
            check(t, other == 0u && latch_lit == 0u);
        }
        per_frame(3, 1, 32, &lo, &hi);
        snprintf(t, sizeof t, "%s: lit steady, every frame the same (%.1f .. %.1f us)", NAME[lv], lo, hi);
        check(t, hi - lo <= 1.0f);
        per_frame(5, 2, 32, &lo, &hi);
        snprintf(t, sizeof t, "%s: mid steady, every frame the same (%.1f .. %.1f us)", NAME[lv], lo, hi);
        check(t, hi - lo <= 1.0f);
    }
    check("HIGH: lit the whole tick (85 .. 100 us); MED half of it (35 .. 52); LOW a quarter (10 .. 27)",
          lit[3] >= 85.0f && lit[3] <= 100.0f && lit[2] >= 35.0f && lit[2] <= 52.0f && lit[1] >= 10.0f && lit[1] <= 27.0f);
    check("mid: HIGH as MED's lit, MED as LOW's lit (+-3 us), LOW the glow's pulse",
          mid[3] > lit[2] - 3.0f && mid[3] < lit[2] + 3.0f && mid[2] > lit[1] - 3.0f && mid[2] < lit[1] + 3.0f &&
          mid[1] > 0.5f && mid[1] < 3.0f);
    check("the glow: HIGH ~3.2 us, MED ~2.4, LOW ~1.6 (+-40 %), each less than the one above",
          glow[3] > 1.9f && glow[3] < 4.5f && glow[2] > 1.4f && glow[2] < 3.4f && glow[1] > 0.9f && glow[1] < 2.3f &&
          glow[1] < glow[2] && glow[2] < glow[3]);
    check("OFF: nothing lit at all (lit, mid, the glow)", lit[0] == 0.0f && mid[0] == 0.0f && glow[0] == 0.0f);

    {                                              /* the breath (HIGH): a step at a time, up and back */
        static float lv_f[FM1_LED_BREATH_FRAMES + 8u];
        uint32_t f, rise = 1, fall = 1, back = 0, peak = 0, steps = 0;
        leds_clear();
        fm1_led_level(3);
        fm1_led_breath[2] = 1u << 4;
        run_ticks(FM1_NCOL);                       /* (it starts at its peak, then the phase from the middle) */
        for (f = 0; f < FM1_LED_BREATH_FRAMES; f++) {
            clear_on();
            run_ticks(FM1_NCOL);
            lv_f[f] = frame_us(2, 4, 1u);
            if (lv_f[f] > lv_f[peak])
                peak = f;
        }
        for (f = 1; f < FM1_LED_BREATH_FRAMES; f++) {   /* (from the frame after the peak: down, then up again) */
            float d = lv_f[f] - lv_f[f - 1u];
            if (d > 1.0f || d < -1.0f)
                steps++;
        }
        for (f = 0; f + 8u < FM1_LED_BREATH_FRAMES; f++) {
            uint32_t j;
            for (j = f + 2u; j < f + 8u; j++)       /* (one level and back within 8 frames: a dither) */
                if (lv_f[j] > lv_f[f] - 1.0f && lv_f[j] < lv_f[f] + 1.0f && (lv_f[f + 1u] > lv_f[f] + 1.0f || lv_f[f + 1u] < lv_f[f] - 1.0f))
                    back++;
        }
        {
            float mn = 1e9f, mx = 0.0f;
            for (f = 0; f < FM1_LED_BREATH_FRAMES; f++) {
                mn = lv_f[f] < mn ? lv_f[f] : mn;
                mx = lv_f[f] > mx ? lv_f[f] : mx;
            }
            for (f = 1; f < FM1_LED_BREATH_FRAMES / 2u; f++)   /* (the first half from its peak: down) */
                fall &= lv_f[f] <= lv_f[f - 1u] + 1.0f;
            for (f = FM1_LED_BREATH_FRAMES / 2u + 1u; f < FM1_LED_BREATH_FRAMES; f++)
                rise &= lv_f[f] + 1.0f >= lv_f[f - 1u];
            printf("led:   breath (HIGH): %.1f .. %.1f us a frame, %u steps a period, its peak at frame %u\n", mn, mx,
                   steps, peak);
            check("the breath: the glow .. lit (HIGH), down then up, a step at a time (8 .. 10 a period), never a level "
                  "and back within 8 frames", mn < 4.5f && mx > 85.0f && fall && rise && steps >= 6u && steps <= 10u &&
                  back == 0u);
        }
    }
    {                                              /* nothing ending at a quarter: no write there */
        uint32_t w0, q;
        leds_clear();
        fm1_led_level(3);
        fm1_led[1] = 0x1Eu;
        run_ticks(1);
        w0 = writes;
        for (q = 1; q < 4u; q++)
            fm1_input_quarter(q);
        check("a quarter with nothing ending there: no write of the lines", writes == w0);
    }
    printf("led: %d failed\n", fails);
    return fails != 0;
}
