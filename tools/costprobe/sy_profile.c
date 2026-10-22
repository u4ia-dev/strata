/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* The Strata engine on the host, built with --coverage, for the target cost model (tools/costprobe/sy_dyn.sh): one
 * scenario plays; after a warm-up the coverage counters are reset, so gcov's line counts are the audio path's work
 * for PROF_BLOCKS blocks (sy_block alone: the main loop's service is not the audio's). sy_dyncount.py weighs each
 * instruction of the target listing by its line's count.
 *   sy_profile SCENARIO           (0 .. PROF_N - 1; no argument: the list) */
#define SY_ENGINE 1
#include "../../tests/sy_host.h"
#include "../../firmware/src/sy_sounds.h"
#include <unistd.h>

extern void __gcov_reset(void);
extern void __gcov_dump(void);

#define PROF_BLOCKS 1024u
#define PROF_WARM 64u

typedef struct {
    const char *name;
    uint8_t voices;
    int16_t kv[24][2];                     /* parameter, value pairs (-1 ends) */
    int8_t sound;                          /* V0.81: a starter sound under them (sy_sounds.h; -1 none) */
    uint8_t drums;                         /* V0.85: 1 every lane hit each 64 blocks (~46 ms), 2 and the duck each 32;
                                            * V0.86: 3 as 1, every drum's effects on and the kit's COMP */
} prof_t;

#define FX_ALL {SP_DR_ON, 1}, {SP_CH_ON, 1}, {SP_DL_ON, 1}, {SP_RV_ON, 1}, {SP_OT_ON, 1}, {SP_EQ_ON, 1},   \
               {SP_EQ_LOW, 30}, {SP_EQ_MID, -30}, {SP_EQ_HIGH, 30}
static const prof_t PROF[] = {
    {"idle: no voice, no effect", 0, {{-1, 0}}, -1},
    {"1 voice: A x 1", 1, {{-1, 0}}, -1},
    {"1 voice: A x 16", 1, {{SP_A_UNI, 16}, {-1, 0}}, -1},
    {"8 voices: A x 1", 8, {{-1, 0}}, -1},
    {"8 voices: A + B x 1, LP12", 8, {{SP_B_ON, 1}, {SP_F_ON, 1}, {SP_F_TYPE, FT_LP12}, {-1, 0}}, -1},
    {"8 voices: A + B x 3, LP24, every effect", 8,
     {{SP_B_ON, 1}, {SP_A_UNI, 3}, {SP_B_UNI, 3}, {SP_F_ON, 1}, {SP_F_TYPE, FT_LP24}, FX_ALL, {-1, 0}}, -1},
    {"8 voices: A + B x 16 (budget 48), LADDER, every effect", 8,
     {{SP_B_ON, 1}, {SP_A_UNI, 16}, {SP_B_UNI, 16}, {SP_F_ON, 1}, {SP_F_TYPE, FT_LADDER}, FX_ALL, {-1, 0}}, -1},
    {"8 voices: A BEND+ / B SYNC x 1, LP12, SUB + pink NOISE", 8,
     {{SP_A_WARP, WARP_BENDP}, {SP_A_WAMT, 500}, {SP_B_ON, 1}, {SP_B_WARP, WARP_SYNC}, {SP_B_WAMT, 500},
      {SP_F_ON, 1}, {SP_F_TYPE, FT_LP12}, {SP_SUB_ON, 1}, {SP_NZ_ON, 1}, {SP_NZ_TYPE, 1}, {-1, 0}}, -1},
    {"1 voice: A x 1, every effect", 1, {FX_ALL, {-1, 0}}, -1},
    {"SUPERSAW, 1 note", 1, {{-1, 0}}, 0},     /* (V0.81: the starter sounds, as the V0.65 test's load ran them) */
    {"SUPERSAW, 8 notes", 8, {{-1, 0}}, 0},
    {"PLUCK, 8 notes", 8, {{-1, 0}}, 1},
    {"PAD, 8 notes", 8, {{-1, 0}}, 2},
    {"drums: the eight lanes sounding", 0, {{-1, 0}}, -1, 1},   /* (V0.85: sy_drum.c) */
    {"SUPERSAW, 8 notes, the drums, the duck", 8, {{-1, 0}}, 0, 2},
    {"drums: the eight lanes, every effect, COMP", 0, {{-1, 0}}, -1, 3},   /* (V0.86: sy_drum.c's effects) */
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
    if (p->sound >= 0)                             /* (a starter sound: its values over the defaults) */
        for (i = 0; SOUNDS[p->sound].kv[i][0] >= 0; i++)
            sy_patch.p[SOUNDS[p->sound].kv[i][0]] =
                sp_clamp((uint32_t)SOUNDS[p->sound].kv[i][0], SOUNDS[p->sound].kv[i][1]);
    for (i = 0; p->kv[i][0] >= 0; i++)
        sy_patch.p[p->kv[i][0]] = sp_clamp((uint32_t)p->kv[i][0], p->kv[i][1]);
    sp_chain_from_old(&sy_patch);                  /* (V0.81: the effects ON as the chain, as the starter sounds have
                                                    * it: before, "every effect" ran none, the chain empty) */
    sp_snap();
    sy_tables_now();
    for (i = 0; i < p->voices; i++)
        sy_note_on(48 + 4 * i, 100);
    if (p->drums == 3u) {                          /* (every drum's DRIVE, EQ and SHAPE off their rest, COMP on) */
        for (i = 0; i < DK_N; i++) {
            dk_fx_t *f = &dkk.fx[i];
            f->dtype = DD_SOFT, f->drive = 40, f->dmix = 80, f->lo = 60, f->mid = 40, f->hi = 60, f->att = 60, f->sus = 40;
        }
        dkk.comp[0] = 50;
    }
    for (i = 0; i < PROF_WARM + PROF_BLOCKS; i++) {
        uint32_t l;
        if (i == PROF_WARM)
            __gcov_reset();
        if (p->drums && i % 64u == 0u)           /* (the drums: every lane, then (2) the duck, as a beat would) */
            for (l = 0; l < DK_N; l++)
                sy_drum_hit(l, 0, 100, (int)(l & 1u));
        if (p->drums == 2u && i % 32u == 0u)
            sy_drum_hit(DK_DUCK, 0, 100, 0);
        sy_block(out);
        if (i < PROF_WARM)
            sy_service();
    }
    __gcov_dump();                                 /* (once: at exit gcov would merge a second dump into it) */
    printf("%u blocks: %s (oscillator instances %u)\n", PROF_BLOCKS, p->name, sy_instances());
    fflush(stdout);
    _exit(0);
}
