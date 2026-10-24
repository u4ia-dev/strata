/* SPDX-License-Identifier: GPL-3.0-only */
/* The test projects (V0.87.1, firmware/src/sy_testproj.c) rendered on the host: web/emu/strata_web.c built natively
 * (as web/emu/strata_clip.c), the device from power-on with an empty flash, test project K loaded as DEBUG > TESTS
 * and the console's `tproj K` load it (sy_tp_load), played to its end and 2 s of its tails, the master's output (Q15,
 * MASTER at unity: what the DAC and the USB audio get) into a stereo 16-bit WAV at 44,118 Hz. The emulator has no
 * clock within a half (SY_LOAD_KNOWN 0): the CPU guard and the admission never act here, every voice plays.
 *   cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/strata_tproj web/emu/strata_tproj.c -lm
 *   build/emu/strata_tproj K out.wav     K 1 .. 4: one line of figures (peak, RMS, clipped samples, the voices' most,
 *                                        the guard's levels), and for TEST 1 the voices of each chord
 *   build/emu/strata_tproj check         PLAY before a load's hush: the load gives way. The owner's work saved, edited
 *                                        again, a test loaded 0.5 s later: the edit saved first, kept by a restart (a
 *                                        process of its own, from the image). Each test played and left 10 s (the
 *                                        auto-save's time): the flash as it was. PROJECT's LOAD, an empty project and
 *                                        NEW end it: the sound, tempo, CLOCK IN, chords back. A start that never comes
 *                                        ends the load (forced) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define WEB_NO_LINT 1
#include "strata_web.c"

#define FS 44118u
#define MAXS (100u * FS)                          /* (a test's 60 .. 90 s and its tails) */
static int16_t wav_buf[2u * MAXS];

static void run_ms(uint32_t ms)
{
    while (ms--)
        web_ms();
}

static void wav_write(const char *path, const int16_t *x, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t v, d = n * 4u;
    if (!f) {
        printf("cannot write %s\n", path);
        exit(1);
    }
    fwrite("RIFF", 1, 4, f);
    v = 36u + d; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f);
    v = 1u | 2u << 16; fwrite(&v, 4, 1, f);
    v = FS; fwrite(&v, 4, 1, f);
    v = FS * 4u; fwrite(&v, 4, 1, f);
    v = 4u | 16u << 16; fwrite(&v, 4, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&d, 4, 1, f);
    fwrite(x, 2, 2u * n, f);
    fclose(f);
}

/* one millisecond, the half it rendered (if one) into the WAV's buffer at *n */
static void step(uint32_t *n)
{
    uint32_t h = web_half;
    web_ms();
    if (web_half != h && *n + SY_HALF <= MAXS) {
        const int32_t *o = &sy_abuf[h * SY_HALF_WORDS];
        uint32_t j;
        for (j = 0; j < 2u * SY_HALF; j++)
            wav_buf[2u * *n + j] = (int16_t)(o[j] >> SY_OUT_SHIFT);
        *n += SY_HALF;
    }
}

static int render(uint32_t k, const char *path)
{
    uint32_t n = 0, i, vmax = 0, clip = 0, lvmax = 0, fxlo = 0, t_end = 0, vseg[9] = {0}, mout0, ev;
    int32_t peak = 0;
    double ss = 0, ss_on = 0;
    uint32_t n_on = 0, inst = 0, nmv = 0, mvn = 0, mv[128], mvar = 0, hits0[DK_ROWS], r, scn = 0, solo = 0, mute = 0;
    uint8_t spd = 0;
    web_nor_erase();
    web_boot();
    run_ms(500);
    mout0 = web_mout_n;
    if (!sy_tp_load(k - 1u)) {
        printf("tproj %u: not loaded\n", k);
        return 1;
    }
    memset(hits0, 0, sizeof hits0);
    while (n < MAXS) {
        uint32_t v = web_test_voices();
        step(&n);
        if (tp.st == TPS_RUN) {                      /* (what it went through: the chains' instances, the knobs the
                                                      * slots swept, the drums' models and variants, the scenes) */
            uint32_t s2, j;
            for (s2 = 0; s2 < NFXC; s2++)
                if (sy_patch.p[SP_FXC1 + s2] > 0)
                    inst |= 1u << (sy_patch.p[SP_FXC1 + s2] - 1);
            for (s2 = 0; s2 < NSLOT; s2++) {
                uint32_t d = (uint32_t)sy_patch.p[SP_M1_DST + 3u * s2];
                if (k != 2u || !sy_patch.p[SP_M1_SRC + 3u * s2] || !d)
                    continue;
                for (j = 0; j < mvn && mv[j] != d; j++)
                    ;
                if (j == mvn && mvn < 128u)
                    mv[mvn++] = d;
            }
            for (r = 0; r < DK_N; r++)
                if (dk_nhit[r] != hits0[r]) {
                    uint32_t var, m = dk_model(r, &var);
                    mvar |= 1u << (m * 3u + var);
                    hits0[r] = dk_nhit[r];
                }
            for (r = 0; r < LP_NTRK; r++) {
                if (lps[r].play < LP_NSCN)
                    scn |= 1u << lps[r].play;
                solo |= (uint32_t)lpp.trk[r].solo << r;
                mute |= (uint32_t)lpp.trk[r].mute << r;
                if (lps[r].play < LP_NSCN)
                    spd |= (uint8_t)(1u << lpp.speed[r][lps[r].play]);
            }
        }
        vmax = v > vmax ? v : vmax;
        lvmax = sy_g.level > lvmax ? sy_g.level : lvmax;
        fxlo |= sy_g.fx_lo;
        if (k == 1u && tp.st == TPS_RUN && cht.run) {   /* (TEST 1: each bar's chord, its voices; the same 9 a sound) */
            uint32_t bar = (uint32_t)(cht.tq / 4.0f) % 9u;
            vseg[bar] = v > vseg[bar] ? v : vseg[bar];
        }
        if (tp.st == TPS_DONE && !t_end)
            t_end = n;
        if (t_end && n >= t_end + 2u * FS)
            break;
    }
    ev = LP_NEV - lp_nfree;
    for (i = 0; i < 2u * n; i++) {
        int32_t a = wav_buf[i] < 0 ? -wav_buf[i] : wav_buf[i];
        double x = (double)wav_buf[i] / 32768.0;
        peak = a > peak ? a : peak;
        clip += a >= 32767;
        ss += x * x;
    }
    for (i = 0; i + 4410u <= n; i += 4410u) {      /* (the RMS of the 100 ms that sound, -60 dBFS and over) */
        double e = 0;
        uint32_t j;
        for (j = 2u * i; j < 2u * (i + 4410u); j++)
            e += ((double)wav_buf[j] / 32768.0) * ((double)wav_buf[j] / 32768.0);
        if (e / 8820.0 > 1e-6) {
            ss_on += e;
            n_on += 4410u;
        }
    }
    wav_write(path, wav_buf, n);
    printf("tproj %u %s: %.1f s (its end at %.1f s), peak %.1f %% (%.2f dBFS), RMS %.1f dBFS (sounding %.1f dBFS), "
           "clipped %u, voices max %u, guard level max %u, fx_lo %s, events %u of %u, MIDI out %u, drum hits %u, "
           "voices refused %u, shed %u\n",
           k, SY_TP_NAME[k - 1u], (double)n / FS, (double)t_end / FS, 100.0 * peak / 32767.0,
           20.0 * log10((double)(peak ? peak : 1) / 32767.0), 10.0 * log10(ss / (2.0 * n) + 1e-20),
           10.0 * log10(ss_on / (2.0 * (n_on ? n_on : 1)) + 1e-20), clip, vmax, lvmax, fxlo ? "on" : "never", ev,
           LP_NEV, web_mout_n - mout0, (unsigned)(dk_nhit[0] + dk_nhit[1] + dk_nhit[2] + dk_nhit[3] + dk_nhit[4] +
                                                  dk_nhit[5] + dk_nhit[6] + dk_nhit[7]),
           (unsigned)sy_adm.refused, (unsigned)sy_g.shed);
    {
        uint32_t c = 0, b;
        for (b = 0; b < 32u; b++)
            c += (mvar >> b) & 1u;
        nmv = 0;
        for (b = 0; b < 3u; b++)
            nmv += tp_fx_ids(b, 1, (uint16_t[64]){0}, 64u);
        for (b = 0, r = 0; b < 24u; b++)
            r += (inst >> b) & 1u;
        printf("tproj %u went through: chain instances %u of 24; knobs the matrix swept %u of %u; drum models x variants "
               "%u of 15; scenes 0x%02X; tracks soloed 0x%02X, muted 0x%02X; SPEEDs heard 0x%X (1 x1, 2 x1/2, 4 x2)\n",
               k, r, mvn, nmv, c, scn, solo, mute, spd);
    }
    if (k == 1u)
        printf("tproj 1 voices by bar of each sound (1, 2, 4, 8, 8, 8, 8, 8 notes, rest): %u %u %u %u %u %u %u %u %u\n",
               vseg[0], vseg[1], vseg[2], vseg[3], vseg[4], vseg[5], vseg[6], vseg[7], vseg[8]);
    return 0;
}

/* the flash before and after; 0 the same */
static uint32_t nor_diff(const uint8_t *a)
{
    uint32_t i, d = 0;
    for (i = 0; i < sizeof nor; i++)
        d += a[i] != nor[i];
    return d;
}

/* the owner's state as the check left it: the sound (its name, CUTOFF), BPM, CLOCK IN, track 1's chords' mode, its
 * clip's notes; 0 all as expected */
static uint32_t owner_ok(const char *name, int32_t cut, uint32_t bpm, int32_t notes)
{
    return !(!strcmp(sy_patch.name, name) && sy_patch.p[SP_F_CUT] == cut && ui.bpm == bpm && ch_clock == 1u &&
             ch_d[0].mode == CH_KEY && (notes < 0 || web_test_lp_clip(0, 0, 2, -1) == notes));
}
static uint32_t until(uint32_t st, uint32_t ms)  /* the test's state st reached (in ms at most): 1 */
{
    while (ms-- && tp.st != st)
        web_ms();
    return tp.st == st;
}

/* a restart from the flash image in path: the owner's edits there (the one made just before a test's load too) */
static int restart(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint32_t d;
    if (!f || fread(nor, 1, sizeof nor, f) != sizeof nor)
        return 1;
    fclose(f);
    web_boot();
    run_ms(500);
    d = sy_tp_active() || owner_ok("LADDER BASS", 444, 98u, 9);
    printf("check: a restart: test %s; the sound %s (CUTOFF %d), BPM %u, CLOCK IN %u, track 1 chords %u, the clip %d "
           "notes (9: the one added 0.5 s before the test's load)  %s\n", sy_tp_active() ? "loaded" : "none",
           sy_patch.name, sy_patch.p[SP_F_CUT], ui.bpm, ch_clock, ch_d[0].mode, web_test_lp_clip(0, 0, 2, -1),
           d ? "FAIL" : "ok");
    return d != 0;
}

static int check(const char *self)
{
    static uint8_t img[sizeof nor];
    uint32_t k, d, bad = 0, w0, w1;
    web_nor_erase();
    web_boot();
    run_ms(500);
    /* 1. a test's load while PLAY's start comes before the hush: the load gives way, nothing silent */
    sy_tp_load(3);
    run_ms(2);
    k = tp.hush && !sy_hushed;
    sy_app_run(1);
    run_ms(100);
    d = !k || sy_tp_active() || tp.st != TPS_NONE || sy_hush_req || sy_hushed || !cht.run;
    printf("check: PLAY while a test's load waits for its hush: the load %s, the hush %s, the transport %s  %s\n",
           tp.st == TPS_NONE ? "dropped" : "on", sy_hush_req || sy_hushed ? "kept" : "let go",
           cht.run ? "running" : "stopped", d ? "FAIL" : "ok");
    bad += d;
    sy_app_run(0);
    run_ms(200);
    /* 2. the owner's work, saved; then edited again within a second of a test's load */
    web_test_sound_set(5);                       /* (the owner's: LADDER BASS, a knob turned; a clip of notes; */
    run_ms(200);                                 /* CLOCK IN MIDI, track 1's chords KEY) */
    web_test_set(SP_F_CUT, 333);
    for (k = 0; k < 8u; k++)
        web_test_lp_put(0, 0, 48u * k, 40u, 48u + k, 100u);
    ui.bpm = 97;
    sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
    ch_clock = 1;
    ch_d[0].mode = CH_KEY, ch_d[0].gen++;
    run_ms(8000);                                /* (quiet, stopped: the auto-save writes them) */
    memcpy(img, nor, sizeof nor);
    w0 = web_flash_writes;
    web_midi(0x90, 60, 100);                     /* (the edits while a note sounds: the auto-save waits for 3 s of */
    run_ms(100);                                 /* quiet, so none of them is on the flash at the load) */
    web_test_set(SP_F_CUT, 444);
    web_test_lp_put(0, 0, 360u, 20u, 72u, 100u);
    ui.bpm = 98;
    sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
    run_ms(400);
    web_midi(0x80, 60, 0);
    d = web_flash_writes != w0;
    if (!sy_tp_load(0) || !until(TPS_WAIT, 2000u)) {
        printf("check: test 1 not loaded\n");
        return 1;
    }
    d = d || !nor_diff(img);
    printf("check: a test loaded 0.5 s after an edit: the owner's work saved first (%u bytes, %u writes)  %s\n",
           nor_diff(img), web_flash_writes - w0, d ? "FAIL" : "ok");
    bad += d;
    memcpy(img, nor, sizeof nor);
    w0 = web_flash_writes;
    {                                            /* (a restart now, from this image: its own process) */
        char cmd[512];
        FILE *f = fopen("build/tproj/check_nor.bin", "wb");
        if (!f) {
            printf("check: cannot write build/tproj/check_nor.bin (mkdir -p build/tproj)  FAIL\n");
            return 1;
        }
        fwrite(nor, 1, sizeof nor, f);
        fclose(f);
        snprintf(cmd, sizeof cmd, "%s restart build/tproj/check_nor.bin", self);
        fflush(stdout);
        bad += system(cmd) != 0;
    }
    /* 3. each test played, the flash left as it was */
    for (k = 0; k < SY_TP_N; k++) {
        if (k && !sy_tp_load(k)) {
            printf("check: test %u not loaded\n", k + 1u);
            return 1;
        }
        until(TPS_DONE, 100000u);
        run_ms(10000);                           /* (stopped and quiet: when the auto-save would write) */
        d = nor_diff(img);
        printf("check: test %u %s loaded, played %.1f s, 10 s after: the flash %s (%u bytes differ, %u writes)  %s\n",
               k + 1u, SY_TP_NAME[k], sy_tp_ms(k) / 1000.0, d ? "CHANGED" : "unchanged", d,
               web_flash_writes - w0, d || web_flash_writes != w0 ? "FAIL" : "ok");
        bad += d || web_flash_writes != w0;
    }
    /* 4. PROJECT's LOAD ends it */
    sy_app_project(lpg.prj);
    run_ms(500);
    d = sy_tp_active() || owner_ok("LADDER BASS", 444, 98u, 9);
    printf("check: PROJECT's LOAD ended it: test %s, the sound %s (CUTOFF %d), BPM %u, CLOCK IN %u, the clip %d notes  "
           "%s\n", sy_tp_active() ? "still loaded" : "none", sy_patch.name, sy_patch.p[SP_F_CUT], ui.bpm, ch_clock,
           web_test_lp_clip(0, 0, 2, -1), d ? "FAIL" : "ok");
    bad += d;
    run_ms(10000);
    d = nor_diff(img);
    printf("check: 10 s after: the flash %s (%u bytes differ)  %s\n", d ? "CHANGED" : "unchanged", d, d ? "FAIL" : "ok");
    bad += d != 0;
    /* 5. an empty slot loaded after a test: the owner's chords, not the test's */
    sy_tp_load(3);
    until(TPS_RUN, 3000u);
    run_ms(300);
    sy_app_project((lpg.prj + 1u) % LP_NPRJ);
    run_ms(500);
    d = sy_tp_active() || owner_ok("LADDER BASS", 444, 98u, -1);
    printf("check: an empty project loaded after TEST 4: test %s, CLOCK IN %u, track 1 chords %u (KEY %u), BPM %u  %s\n",
           sy_tp_active() ? "still loaded" : "none", ch_clock, ch_d[0].mode, CH_KEY, ui.bpm, d ? "FAIL" : "ok");
    bad += d;
    /* 6. PROJECT's NEW ends it too */
    sy_tp_load(1);
    until(TPS_RUN, 3000u);
    run_ms(300);
    w1 = web_flash_writes;
    sy_app_project_new();
    run_ms(500);
    d = sy_tp_active() || strcmp(sy_patch.name, "LADDER BASS") || sy_patch.p[SP_F_CUT] != 444 || ui.bpm != 98u;
    run_ms(8000);
    printf("check: PROJECT's NEW ended it: test %s, the sound %s (CUTOFF %d), BPM %u; auto-save back on (%u writes "
           "after: the new project, as NEW always writes it)  %s\n", sy_tp_active() ? "still loaded" : "none",
           sy_patch.name, sy_patch.p[SP_F_CUT], ui.bpm, web_flash_writes - w1, d || web_flash_writes == w1 ? "FAIL" : "ok");
    bad += d || web_flash_writes == w1;
    /* 7. a start that never comes (its PLAY taken out of the ring each time, as a full ring drops it): the load ends,
     * the owner's project back */
    sy_tp_load(0);
    until(TPS_WAIT, 3000u);
    for (k = 0; k < 2000u && tp.st != TPS_NONE; k++) {
        web_ms();
        while (sy_ev_ui.w != sy_ev_ui.r && sy_ev_ui.q[(sy_ev_ui.w - 1u) % SY_EVQ].type == SY_EV_RUN &&
               sy_ev_ui.q[(sy_ev_ui.w - 1u) % SY_EVQ].a == 1u)
            sy_ev_ui.w--;
    }
    run_ms(500);
    d = sy_tp_active() || tp.st != TPS_NONE || sy_patch.p[SP_F_CUT] != 444;
    printf("check: a test whose start never came: `tproj` would say %s, the sound %s  %s\n",
           sy_tp_active() ? "it is loaded" : "none", sy_patch.p[SP_F_CUT] == 444 ? "back" : "not back", d ? "FAIL" : "ok");
    bad += d;
    printf("check: %u failed\n", bad);
    return bad != 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "check"))
        return check(argv[0]);
    if (argc == 3 && !strcmp(argv[1], "restart"))
        return restart(argv[2]);
    if (argc != 3 || atoi(argv[1]) < 1 || atoi(argv[1]) > (int)SY_TP_N) {
        printf("strata_tproj K out.wav (K 1 .. %u), or strata_tproj check\n", SY_TP_N);
        return 2;
    }
    return render((uint32_t)atoi(argv[1]), argv[2]);
}
