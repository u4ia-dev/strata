/* SPDX-License-Identifier: GPL-3.0-only */
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
    int16_t kv[40][2];                     /* parameter, value pairs (-1 ends) */
    int8_t sound;                          /* V0.81: a starter sound under them (sy_sounds.h; -1 none) */
    uint8_t drums;                         /* V0.85: 1 every lane hit each 64 blocks (~46 ms), 2 and the duck each 32;
                                            * V0.86: 3 as 1, every drum's effects on and the kit's COMP */
    uint8_t xf;                            /* V0.87.1: the guard's fx_lo turned each 17 blocks (its 16-block ramp each
                                            * way): the reverb's lines 5-8 on their way (sy_fx.c fx_rv_xf) */
    uint8_t chain[8];                      /* V0.875: the chain (instances + 1, each ON) in place of the starter
                                            * sound's: the effects one at a time and TEST 2's chains (0: as it is) */
} prof_t;

#define T2_SYN {SP_A_TABLE, 1}, {SP_A_POS, 1000}, {SP_F_ON, 1}, {SP_F_TYPE, FT_LP12}, {SP_F_CUT, 720}, {SP_F_RES, 200}, \
               {SP_L1_RATE, 472}
#define T2_SW(k, id) {SP_M1_SRC + 3 * ((k) - 1), SRC_LFO1}, {SP_M1_DST + 3 * ((k) - 1), SP_##id}, \
                     {SP_M1_AMT + 3 * ((k) - 1), 1000}
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
    {"drums: the eight lanes on V0.87's models", 0, {{-1, 0}}, -1, 4},    /* (V0.87 step 2: ANALOG, FM, CHIP, NOISE) */
    {"PLUCK, 1 note", 1, {{-1, 0}}, 1},        /* (V0.87.1: every starter sound at 1 note and 8, the voices' cost */
    {"PAD, 1 note", 1, {{-1, 0}}, 2},          /* against the guard's: DECISIONS item 14) */
    {"FM KEYS, 1 note", 1, {{-1, 0}}, 3},
    {"FM KEYS, 8 notes", 8, {{-1, 0}}, 3},
    {"SYNC LEAD, 1 note", 1, {{-1, 0}}, 4},
    {"LADDER BASS, 1 note", 1, {{-1, 0}}, 5},
    {"VOWEL PAD, 1 note", 1, {{-1, 0}}, 6},
    {"VOWEL PAD, 8 notes", 8, {{-1, 0}}, 6},
    {"OTT GROWL, 1 note", 1, {{-1, 0}}, 7},
    {"OTT GROWL, 8 notes", 8, {{-1, 0}}, 7},
    {"SUPERSAW, 1 note, the reverb's lines 5-8 always on their way (16 blocks)", 1, {{-1, 0}}, 0, 0, 1},
    /* V0.875: the effects one at a time over TEST 2's synth (sy_testproj.c: SAW at its end, LP12, six voices), each
     * at its heaviest setting (PHASER 12 stages, the FX filter's LADDER, the reverb HALL); then TEST 2's three chains
     * with a knob of each module swept by LFO 1 (the matrix's eight slots), as the test sweeps them */
    {"TEST 2's synth: 6 voices, SAW, LP12, no effect", 6, {T2_SYN, {-1, 0}}, -1},
    {"TEST 2's synth + DRIVE", 6, {T2_SYN, {-1, 0}}, -1, 0, 0, {FXI_DRIVE + 1}},
    {"TEST 2's synth + CHORUS", 6, {T2_SYN, {-1, 0}}, -1, 0, 0, {FXI_CHORUS + 1}},
    {"TEST 2's synth + DELAY", 6, {T2_SYN, {-1, 0}}, -1, 0, 0, {FXI_DELAY + 1}},
    {"TEST 2's synth + REVERB (HALL)", 6, {T2_SYN, {-1, 0}}, -1, 0, 0, {FXI_REVERB + 1}},
    {"TEST 2's synth + OTT", 6, {T2_SYN, {-1, 0}}, -1, 0, 0, {FXI_OTT + 1}},
    {"TEST 2's synth + EQ (three bands off 0 dB)", 6, {T2_SYN, {SP_EQ_LOW, 60}, {SP_EQ_MID, -60}, {SP_EQ_HIGH, 60},
     {-1, 0}}, -1, 0, 0, {FXI_EQ + 1}},
    {"TEST 2's synth + FLANGER", 6, {T2_SYN, {-1, 0}}, -1, 0, 0, {FXI_FLANGER + 1}},
    {"TEST 2's synth + PHASER (12 stages)", 6, {T2_SYN, {SP_PH_STAGES, 5}, {-1, 0}}, -1, 0, 0, {FXI_PHASER + 1}},
    {"TEST 2's synth + FILTER (LADDER)", 6, {T2_SYN, {SP_FF_TYPE, FF_LADDER}, {-1, 0}}, -1, 0, 0, {FXI_FFILT + 1}},
    {"TEST 2's synth + BITCRUSH", 6, {T2_SYN, {-1, 0}}, -1, 0, 0, {FXI_CRUSH + 1}},
    {"TEST 2's synth + TREMOLO", 6, {T2_SYN, {-1, 0}}, -1, 0, 0, {FXI_TREM + 1}},
    {"TEST 2's synth + RING MOD", 6, {T2_SYN, {-1, 0}}, -1, 0, 0, {FXI_RING + 1}},
    {"TEST 2's synth + TAPE", 6, {T2_SYN, {-1, 0}}, -1, 0, 0, {FXI_TAPE + 1}},
    {"TEST 2's chain 1, a knob each swept", 6,
     {T2_SYN, {SP_PH_STAGES, 5}, T2_SW(1, DR_AMT), T2_SW(2, CH_DEPTH), T2_SW(3, DL_TIME), T2_SW(4, RV_SIZE),
      T2_SW(5, OT_DEPTH), T2_SW(6, EQ_MFREQ), T2_SW(7, FL_DELAY), T2_SW(8, PH_CENTER), {-1, 0}}, -1, 0, 0,
     {FXI_DRIVE + 1, FXI_CHORUS + 1, FXI_DELAY + 1, FXI_REVERB + 1, FXI_OTT + 1, FXI_EQ + 1, FXI_FLANGER + 1,
      FXI_PHASER + 1}},
    {"TEST 2's chain 2, a knob each swept", 6,
     {T2_SYN, {SP_FF_TYPE, FF_LADDER}, T2_SW(1, FF_CUT), T2_SW(2, CR_RATE), T2_SW(3, TR_RATE), T2_SW(4, RG_FREQ),
      T2_SW(5, TP_WOW), T2_SW(6, DR2_AMT), T2_SW(7, CH2_DEPTH), T2_SW(8, EQ2_MFREQ), {-1, 0}}, -1, 0, 0,
     {FXI_FFILT + 1, FXI_CRUSH + 1, FXI_TREM + 1, FXI_RING + 1, FXI_TAPE + 1, FXI_DRIVE2 + 1, FXI_CHORUS2 + 1,
      FXI_EQ2 + 1}},
    {"TEST 2's chain 3, a knob each swept", 6,
     {T2_SYN, {SP_PH2_STAGES, 5}, {SP_FF2_TYPE, FF_LADDER}, T2_SW(1, OT2_DEPTH), T2_SW(2, FL2_DELAY),
      T2_SW(3, PH2_CENTER), T2_SW(4, FF2_CUT), T2_SW(5, CR2_RATE), T2_SW(6, TR2_RATE), T2_SW(7, RG2_FREQ),
      T2_SW(8, TP2_WOW), {-1, 0}}, -1, 0, 0,
     {FXI_OTT2 + 1, FXI_FLANGER2 + 1, FXI_PHASER2 + 1, FXI_FFILT2 + 1, FXI_CRUSH2 + 1, FXI_TREM2 + 1, FXI_RING2 + 1,
      FXI_TAPE2 + 1}},
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
    if (p->chain[0])                               /* (V0.875: a chain of its own) */
        for (i = 0; i < NFXC; i++) {
            sy_patch.p[SP_FXC1 + i] = p->chain[i];
            if (p->chain[i])
                sy_patch.p[FXI_ON[p->chain[i] - 1u]] = 1;
        }
    sp_snap();
    sy_tables_now();
    for (i = 0; i < p->voices; i++)
        sy_note_on(48 + 4 * i, 100);
    if (p->drums == 4u) {                          /* (V0.87: the new models, their heaviest variants, on every row) */
        static const uint8_t MV[DK_N][2] = {{DM_ANALOG, 0}, {DM_FM, 0}, {DM_FM, 1}, {DM_CHIP, 0}, {DM_CHIP, 2},
                                            {DM_NOISE, 0}, {DM_CHIP, 1}, {DM_ANALOG, 0}};
        for (i = 0; i < DK_N; i++)
            dkk.l[i].model = (uint8_t)(MV[i][0] + 1u), dkk.l[i].var = MV[i][1], dkk.l[i].chr = 60, dkk.l[i].decay = 90;
    }
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
        if (p->xf && i % 17u == 0u)             /* (V0.87.1: fx_lo turned, as the guard turns it) */
            sy_g_fx(!sy_g.fx_lo, 0);
        sy_block(out);
        if (i < PROF_WARM)
            sy_service();
    }
    __gcov_dump();                                 /* (once: at exit gcov would merge a second dump into it) */
    printf("%u blocks: %s (oscillator instances %u)\n", PROF_BLOCKS, p->name, sy_instances());
    fflush(stdout);
    _exit(0);
}
