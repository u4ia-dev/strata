/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* The Strata engine on the host, built with --coverage, for the target cost model (tools/costprobe/sy_dyn.sh): one
 * scenario plays; after a warm-up the coverage counters are reset, so gcov's line counts are the audio path's work
 * for PROF_BLOCKS blocks (sy_block alone: the main loop's service is not the audio's). sy_dyncount.py weighs each
 * instruction of the target listing by its line's count.
 *   sy_profile SCENARIO           (0 .. PROF_N - 1; no argument: the list) */
#define SY_ENGINE 1
#include "../../tests/sy_host.h"
#include <unistd.h>

extern void __gcov_reset(void);
extern void __gcov_dump(void);

#define PROF_BLOCKS 1024u
#define PROF_WARM 64u

typedef struct {
    const char *name;
    uint8_t voices;
    int16_t kv[16][2];                     /* parameter, value pairs (-1 ends) */
} prof_t;

#define FX_ALL {SP_DR_ON, 1}, {SP_CH_ON, 1}, {SP_DL_ON, 1}, {SP_RV_ON, 1}, {SP_OT_ON, 1}, {SP_EQ_ON, 1},   \
               {SP_EQ_LOW, 30}, {SP_EQ_MID, -30}, {SP_EQ_HIGH, 30}
static const prof_t PROF[] = {
    {"idle: no voice, no effect", 0, {{-1, 0}}},
    {"1 voice: A x 1", 1, {{-1, 0}}},
    {"1 voice: A x 16", 1, {{SP_A_UNI, 16}, {-1, 0}}},
    {"8 voices: A x 1", 8, {{-1, 0}}},
    {"8 voices: A + B x 1, LP12", 8, {{SP_B_ON, 1}, {SP_F_ON, 1}, {SP_F_TYPE, FT_LP12}, {-1, 0}}},
    {"8 voices: A + B x 3, LP24, every effect", 8,
     {{SP_B_ON, 1}, {SP_A_UNI, 3}, {SP_B_UNI, 3}, {SP_F_ON, 1}, {SP_F_TYPE, FT_LP24}, FX_ALL, {-1, 0}}},
    {"8 voices: A + B x 16 (budget 48), LADDER, every effect", 8,
     {{SP_B_ON, 1}, {SP_A_UNI, 16}, {SP_B_UNI, 16}, {SP_F_ON, 1}, {SP_F_TYPE, FT_LADDER}, FX_ALL, {-1, 0}}},
    {"8 voices: A BEND+ / B SYNC x 1, LP12, SUB + pink NOISE", 8,
     {{SP_A_WARP, WARP_BENDP}, {SP_A_WAMT, 500}, {SP_B_ON, 1}, {SP_B_WARP, WARP_SYNC}, {SP_B_WAMT, 500},
      {SP_F_ON, 1}, {SP_F_TYPE, FT_LP12}, {SP_SUB_ON, 1}, {SP_NZ_ON, 1}, {SP_NZ_TYPE, 1}, {-1, 0}}},
    {"1 voice: A x 1, every effect", 1, {FX_ALL, {-1, 0}}},
};
#define PROF_N (sizeof PROF / sizeof PROF[0])

int main(int argc, char **argv)
{
    int32_t out[2u * SY_BLK];
    const prof_t *p;
    uint32_t i, s;
    if (argc < 2) {
        for (s = 0; s < PROF_N; s++)
            printf("%u  %s\n", s, PROF[s].name);
        return 0;
    }
    s = (uint32_t)atoi(argv[1]);
    if (s >= PROF_N)
        return 1;
    p = &PROF[s];
    sy_init();
    sy_tables_now();
    for (i = 0; p->kv[i][0] >= 0; i++)
        sy_patch.p[p->kv[i][0]] = sp_clamp((uint32_t)p->kv[i][0], p->kv[i][1]);
    sp_snap();
    sy_tables_now();
    for (i = 0; i < p->voices; i++)
        sy_note_on(48 + 4 * i, 100);
    for (i = 0; i < PROF_WARM; i++) {
        sy_block(out);
        sy_service();
    }
    __gcov_reset();
    for (i = 0; i < PROF_BLOCKS; i++)
        sy_block(out);
    __gcov_dump();                                 /* (once: at exit gcov would merge a second dump into it) */
    printf("%u blocks: %s (oscillator instances %u)\n", PROF_BLOCKS, p->name, sy_instances());
    fflush(stdout);
    _exit(0);
}
