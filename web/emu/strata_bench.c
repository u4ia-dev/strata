/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* The UI's drawing cost on the host: web/emu/strata_web.c built natively (cc, as the host tests), each frame's work
 * (sy_ui_frame: what it draws and sends) timed, in five sessions of the panel: HOME in silence, HOME with a chord
 * (the scope, the bloom), the filter's TONE page with KNOB 2 turning (a detent every 40 ms), ENV 1 with a chord held
 * (its voices), and every page shown once. Each: the frames' time in microseconds and the pixels sent. The host is
 * not the FM-1: the figures compare one build's UI with another's on the same machine (strata vs strata-ui), not
 * the device's milliseconds.
 *   cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/strata_bench web/emu/strata_bench.c -lm
 *   build/emu/strata_bench */
#include <stdio.h>
#include <time.h>
static unsigned long long web_bench_ns, web_bench_t0, web_bench_frames;
static unsigned long long web_now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000000000ull + (unsigned long long)t.tv_nsec;
}
#define WEB_FRAME_BEGIN() (web_bench_t0 = web_now_ns())
#define WEB_FRAME_END() (web_bench_ns += web_now_ns() - web_bench_t0, web_bench_frames++)
#define WEB_NO_LINT 1                             /* (the screens' check left out: its text boxes cost time) */
#include "strata_web.c"

static void run(double ms)
{
    uint32_t n = (uint32_t)(ms * 44.1 + 0.5), k;
    for (k = 0; k < n; k += 128u)
        web_render(n - k < 128u ? n - k : 128u);
}
static void page(uint32_t sec, const char *name)
{
    uint32_t p, k;
    for (p = 0; p < UI_NPG; p++) {
        for (k = 0; name[k] && UI_PG[p].name[k] == name[k]; k++)
            ;
        if (UI_PG[p].sec == sec && !name[k] && !UI_PG[p].name[k]) {
            web_test_go(p);
            return;
        }
    }
    printf("no page %u %s\n", sec, name);
}
static unsigned long long px0;
static void start(void)
{
    web_bench_ns = 0;
    web_bench_frames = 0;
    px0 = web_px;
}
static void report(const char *what)
{
    printf("%-44s %8.0f us of frames, %6llu frames, %8llu px sent\n", what, (double)web_bench_ns / 1000.0, web_bench_frames,
           (unsigned long long)(web_px - px0));
}

int main(void)
{
    const uint32_t chord = (1u << 7) | (1u << 11) | (1u << 14);   /* C4 E4 G4 */
    uint32_t i, p;
    web_nor_erase();
    web_master(700);
    web_boot();
    web_test_set_get(0, 0);
    run(800);
    page(SEC_MACRO, "HOME");
    run(1500);
    start();
    run(2000);
    report("HOME, silence, 2 s");
    web_keys(chord);
    run(300);
    start();
    run(2000);
    report("HOME, a chord, 2 s (the scope / the bloom)");
    web_keys(0);
    run(1500);
    page(SEC_FILTER, "TONE");
    run(1500);
    start();
    for (i = 0; i < 50u; i++) {                     /* KNOB 2: a detent every 40 ms */
        web_enc(EN_K2, (i / 10u) & 1u ? -1 : 1);
        run(40);
    }
    report("FILTER TONE, KNOB 2 turning, 2 s");
    run(1500);
    page(SEC_ENV1, "ADSR");
    run(1500);
    web_keys(chord);
    start();
    run(2000);
    report("ENV 1 ADSR, a chord held, 2 s");
    web_keys(0);
    run(1500);
    start();
    for (p = 0; p < UI_NPG; p++) {                  /* every page once (SAVE's, the browser's: not by web_test_go) */
        web_test_go(p);
        run(20);
    }
    report("every page shown once");
    page(SEC_FILTER, "VALUES");                     /* V0.67: VALUES, KNOB 2 turning */
    run(1500);
    start();
    for (i = 0; i < 50u; i++) {
        web_enc(EN_K2, (i / 10u) & 1u ? -1 : 1);
        run(40);
    }
    report("FILTER VALUES, KNOB 2 turning, 2 s");
    run(1500);
    web_test_chd(4, 1);                             /* V0.75: ARP over C E G, 1/16 at 120 BPM: its pages as it plays */
    web_test_chd(5, 0);                             /* (V0.8: the style ARP, the page STYLE: their V0.75 names) */
    run(50);
    page(SEC_ARP, "STYLE");
    web_keys(chord);
    run(300);
    start();
    run(2000);
    report("ARP STYLE, the arpeggio playing, 2 s");
    page(SEC_STEPS, "STEPS");
    run(300);
    start();
    run(2000);
    report("ARP STEPS, the arpeggio playing, 2 s");
    web_keys(0);
    web_test_chd(4, 0);
    run(300);
    for (i = 0; i < 16u; i++) {                     /* V0.8, the looper: three clips playing, CLIPS (redrawn 25 a */
        web_test_lp_put(0, 0, 24u * i, 20u, 60u + (i % 8u) * 2u, 100u);   /* second), then NOTES */
        web_test_lp_put(1, 0, 24u * i, 20u, 48u + (i % 4u) * 3u, 90u);
        web_test_lp_put(2, 0, 96u * (i % 4u), 90u, 72u + (i % 3u), 80u);
    }
    for (i = 0; i < 3u; i++)
        web_test_lp_launch(i, 0);
    page(SEC_SEQ, "CLIPS");
    web_buttons(1u << B_PLAY);
    run(30);
    web_buttons(0);
    run(300);
    start();
    run(2000);
    report("LOOP CLIPS, three clips playing, 2 s");
    page(SEC_LCLIP, "NOTES");
    run(300);
    start();
    run(2000);
    report("CLIP NOTES, its clip playing, 2 s");
    return 0;
}
