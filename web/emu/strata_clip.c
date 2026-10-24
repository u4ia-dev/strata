/* SPDX-License-Identifier: GPL-3.0-only */
/* Headroom on the host: web/emu/strata_web.c built natively, each starter sound played as 1, 4 and 8 notes at
 * velocity 64 and 127 for 2 s (C3 up in fourths and fifths), and the master's output (Q15, before MASTER: what the
 * DAC and the USB audio get) counted: its peak, the samples at full scale (the hard clip in sy_engine.c's sy_block)
 * and the stretches of them. The V0.87 hardware test heard crackle on OTT GROWL and VOWEL PAD and "at louder
 * volumes": this says where the mix runs out of headroom.
 *   cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/strata_clip web/emu/strata_clip.c -lm
 *   build/emu/strata_clip */
#include <stdio.h>
#define WEB_NO_LINT 1
#include "strata_web.c"

static const uint8_t CH8[8] = { 48, 55, 60, 64, 67, 71, 74, 79 };   /* C3 G3 C4 E4 G4 B4 D5 G5 */

static void run_ms(uint32_t ms)
{
    uint32_t k;
    for (k = 0; k < ms; k++)
        web_ms();
}

int main(void)
{
    uint32_t k, c, vi;
    const uint32_t counts[3] = { 1, 4, 8 }, vels[2] = { 64, 127 };
    web_nor_erase();
    web_boot();
    run_ms(400);
    printf("%-14s %5s %3s  %7s %8s %6s\n", "sound", "notes", "vel", "peak", "clipped", "runs");
    for (k = 0; k + 1u < SY_NFACT; k++) {
        for (vi = 0; vi < 2u; vi++)
            for (c = 0; c < 3u; c++) {
                uint32_t i, n, half0, clipped = 0, runs = 0, in_run = 0;
                int32_t peak = 0;
                web_midi(0xB0, 123, 0);
                run_ms(1500);
                web_test_sound_set(k);
                run_ms(300);
                for (i = 0; i < counts[c]; i++)
                    web_midi(0x90, CH8[i], vels[vi]);
                half0 = web_half;
                for (n = 0; n < 2000u; n++) {
                    uint32_t h = web_half;
                    web_ms();
                    if (web_half != h) {               /* a half rendered into sy_abuf[h] */
                        const int32_t *o = &sy_abuf[h * SY_HALF_WORDS];
                        uint32_t j;
                        for (j = 0; j < 2u * SY_HALF; j++) {
                            int32_t a = o[j] < 0 ? -o[j] : o[j];
                            if (a > peak)
                                peak = a;
                            if (a >= (32767 << SY_OUT_SHIFT)) {
                                clipped++;
                                if (!in_run)
                                    runs++;
                                in_run = 1;
                            } else
                                in_run = 0;
                        }
                    }
                }
                (void)half0;
                for (i = 0; i < counts[c]; i++)
                    web_midi(0x80, CH8[i], 0);
                printf("%-14s %5u %3u  %6.1f%% %8u %6u\n", web_test_patch_name(), counts[c], vels[vi],
                       100.0 * (double)peak / (double)(32767 << SY_OUT_SHIFT), clipped, runs);
            }
    }
    return 0;
}
