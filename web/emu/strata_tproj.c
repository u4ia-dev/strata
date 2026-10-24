/* SPDX-License-Identifier: GPL-3.0-only */
/* The test projects (V0.87.1, firmware/src/sy_testproj.c) rendered on the host: web/emu/strata_web.c built natively
 * (as web/emu/strata_clip.c), the device from power-on with an empty flash, test project K loaded as DEBUG > TESTS
 * and the console's `tproj K` load it (sy_tp_load), played to its end and 2 s of its tails, the master's output (Q15,
 * MASTER at unity: what the DAC and the USB audio get) into a stereo 16-bit WAV at 44,118 Hz. The emulator has no
 * clock within a half (SY_LOAD_KNOWN 0): the CPU guard and the admission never act here, every voice plays.
 *   cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/strata_tproj web/emu/strata_tproj.c -lm
 *   build/emu/strata_tproj K out.wav     K 1 .. 4: one line of figures (peak, RMS, clipped samples, the voices' most,
 *                                        the guard's levels), and for TEST 1 the voices of each chord
 *   build/emu/strata_tproj check         the flash: a project and a sound saved first, then each test loaded, played
 *                                        and left 10 s (the auto-save's time), the image compared with the one before;
 *                                        then a project's load ends it: the sound and the tempo as before, the image
 *                                        still the same */
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

static int check(void)
{
    static uint8_t img[sizeof nor];
    uint32_t k, d, bad = 0, w0, n = 0;
    int16_t cut;
    char name[17];
    web_nor_erase();
    web_boot();
    run_ms(500);
    web_test_sound_set(5);                       /* (the owner's: LADDER BASS, a knob turned; a clip of notes) */
    run_ms(200);
    web_test_set(SP_F_CUT, 333);
    cut = sy_patch.p[SP_F_CUT];
    memcpy(name, sy_patch.name, sizeof name);
    for (k = 0; k < 8u; k++)
        web_test_lp_put(0, 0, 48u * k, 40u, 48u + k, 100u);
    ui.bpm = 97;
    sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
    run_ms(8000);                                /* (quiet, stopped: the auto-save writes them) */
    memcpy(img, nor, sizeof nor);
    w0 = web_flash_writes;
    printf("check: the owner's project and sound saved (%u flash writes so far)\n", web_flash_writes);
    for (k = 0; k < SY_TP_N; k++) {
        uint32_t t;
        if (!sy_tp_load(k)) {
            printf("check: test %u not loaded\n", k + 1u);
            return 1;
        }
        for (t = 0; t < 100000u && tp.st != TPS_DONE; t++)
            step(&n), n = 0;
        run_ms(10000);                           /* (stopped and quiet: when the auto-save would write) */
        d = nor_diff(img);
        printf("check: test %u %s loaded, played %.1f s, 10 s after: the flash %s (%u bytes differ, %u writes)  %s\n",
               k + 1u, SY_TP_NAME[k], sy_tp_ms(k) / 1000.0, d ? "CHANGED" : "unchanged", d,
               web_flash_writes - w0, d || web_flash_writes != w0 ? "FAIL" : "ok");
        bad += d || web_flash_writes != w0;
    }
    sy_app_project(lpg.prj);                     /* (a project's load ends it) */
    run_ms(500);
    d = sy_tp_active() || sy_patch.p[SP_F_CUT] != cut || strcmp(sy_patch.name, name) || ui.bpm != 97u ||
        web_test_lp_clip(0, 0, 2, -1) != 8;
    printf("check: PROJECT's LOAD ended it: test %s, the sound %s (%s, CUTOFF %d), BPM %u, the project's clip %d notes  "
           "%s\n", sy_tp_active() ? "still loaded" : "none", d ? "not back" : "back", sy_patch.name,
           sy_patch.p[SP_F_CUT], ui.bpm, web_test_lp_clip(0, 0, 2, -1), d ? "FAIL" : "ok");
    bad += d;
    run_ms(10000);
    d = nor_diff(img);
    printf("check: 10 s after: the flash %s (%u bytes differ, %u writes)  %s\n", d ? "CHANGED" : "unchanged", d,
           web_flash_writes - w0, d ? "FAIL" : "ok");
    bad += d != 0;
    printf("check: %u failed\n", bad);
    return bad != 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "check"))
        return check();
    if (argc != 3 || atoi(argv[1]) < 1 || atoi(argv[1]) > (int)SY_TP_N) {
        printf("strata_tproj K out.wav (K 1 .. %u), or strata_tproj check\n", SY_TP_N);
        return 2;
    }
    return render((uint32_t)atoi(argv[1]), argv[2]);
}
