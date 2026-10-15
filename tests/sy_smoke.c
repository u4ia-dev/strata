/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* A first sound from the whole engine: the default patch, a chord, a release; build/sy_demo/smoke.wav. */
#define SY_ENGINE 1
#include "sy_host.h"

#define SECS 3u
static float L[SECS * 44118u], R[SECS * 44118u];

int main(void)
{
    int32_t out[2u * SY_BLK];
    uint32_t n = 0, b, i, nb = (uint32_t)(SECS * SY_FS) / SY_BLK;
    t_name = "smoke";
    sy_init();
    sy_tables_now();
    for (b = 0; b < nb; b++) {
        if (b == 10u) {
            sy_note_on(48, 100);
            sy_note_on(55, 100);
            sy_note_on(60, 100);
        }
        if (b == nb / 2u) {
            sy_note_off(48);
            sy_note_off(55);
            sy_note_off(60);
        }
        sy_block(out);
        for (i = 0; i < SY_BLK; i++, n++) {
            L[n] = (float)out[2u * i] / 32768.0f;
            R[n] = (float)out[2u * i + 1u] / 32768.0f;
        }
    }
    wav_write("build/sy_demo/smoke.wav", L, R, n);
    printf("smoke: peak %.3f, rms %.3f\n", peak(L, n), rms(L, n));
    check("sound comes out", rms(L, n) > 0.01);
    check("the release ends in silence", peak(L + n - 4410, 4410) < 1e-4);
    return t_done();
}
