/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA's eight starter sounds: parameter, value pairs over the defaults. The engine test plays a phrase on each for
 * its golden renders (tests/sy_engine_test.c); the app (sy_app.c, until Phase 5's factory presets) and the browser
 * workbench (web/strata) offer them. Needs sy_param.c, sy_osc.c, sy_filter.c, sy_mod.c, sy_fx.c. */
#ifndef SY_SOUNDS_H
#define SY_SOUNDS_H
typedef struct {
    const char *name;
    int16_t kv[24][2];                                    /* parameter, value pairs (-1 ends) */
    uint8_t cat;                                          /* CAT_* (HOME, the browser) */
} test_sound_t;

static const test_sound_t SOUNDS[8] = {
    {"supersaw", {{SP_A_TABLE, 1}, {SP_A_POS, 1000}, {SP_A_UNI, 7}, {SP_A_DETUNE, 350}, {SP_A_UMODE, 1},
                  {SP_F_ON, 1}, {SP_F_TYPE, FT_LP24}, {SP_F_CUT, 820}, {SP_E1_REL, 400}, {SP_CH_ON, 1}, {SP_RV_ON, 1},
                  {-1, 0}}, CAT_LEAD},
    {"pluck", {{SP_A_TABLE, 45}, {SP_A_POS, 600}, {SP_F_ON, 1}, {SP_F_TYPE, FT_LP12}, {SP_F_CUT, 350}, {SP_F_RES, 400},
               {SP_E1_DEC, 500}, {SP_E1_SUS, 0}, {SP_E2_DEC, 400}, {SP_M1_SRC, SRC_ENV2}, {SP_M1_DST, SP_F_CUT},
               {SP_M1_AMT, 500}, {SP_DL_ON, 1}, {-1, 0}}, CAT_PLUCK},
    {"pad", {{SP_A_TABLE, 27}, {SP_A_UNI, 4}, {SP_B_ON, 1}, {SP_B_TABLE, 31}, {SP_B_UNI, 3}, {SP_E1_ATK, 600},
             {SP_E1_REL, 650}, {SP_L1_RATE, 350}, {SP_M1_SRC, SRC_LFO1}, {SP_M1_DST, SP_A_POS}, {SP_M1_AMT, 400},
             {SP_RV_ON, 1}, {SP_RV_MIX, 400}, {SP_CH_ON, 1}, {-1, 0}}, CAT_PAD},
    {"fm keys", {{SP_A_TABLE, 37}, {SP_A_POS, 500}, {SP_B_ON, 1}, {SP_B_TABLE, 5}, {SP_A_WARP, WARP_FM},
                 {SP_A_WAMT, 400}, {SP_B_LEVEL, 0}, {SP_E1_DEC, 560}, {SP_E1_SUS, 300}, {-1, 0}}, CAT_KEYS},
    {"sync lead", {{SP_A_TABLE, 1}, {SP_A_POS, 1000}, {SP_A_WARP, WARP_SYNC}, {SP_A_WAMT, 450}, {SP_V_MODE, 1},
                   {SP_V_GLIDE, 350}, {SP_L1_RATE, 650}, {SP_M1_SRC, SRC_LFO1}, {SP_M1_DST, SP_A_WAMT},
                   {SP_M1_AMT, 200}, {SP_DR_ON, 1}, {SP_DL_ON, 1}, {-1, 0}}, CAT_LEAD},
    {"ladder bass", {{SP_A_TABLE, 2}, {SP_A_POS, 1000}, {SP_SUB_ON, 1}, {SP_SUB_OCT, -1}, {SP_F_ON, 1},
                     {SP_F_TYPE, FT_LADDER}, {SP_F_CUT, 450}, {SP_F_RES, 600}, {SP_F_DRIVE, 400}, {SP_E2_DEC, 450},
                     {SP_M1_SRC, SRC_ENV2}, {SP_M1_DST, SP_F_CUT}, {SP_M1_AMT, 450}, {SP_V_MODE, 2}, {-1, 0}}, CAT_BASS},
    {"vowel pad", {{SP_A_TABLE, 28}, {SP_A_UNI, 5}, {SP_A_DETUNE, 200}, {SP_F_ON, 1}, {SP_F_TYPE, FT_FORMANT},
                   {SP_L2_RATE, 300}, {SP_M2_SRC, SRC_LFO2}, {SP_M2_DST, SP_F_CUT}, {SP_M2_AMT, 300},
                   {SP_NZ_ON, 1}, {SP_NZ_TYPE, 1}, {SP_NZ_LEVEL, 150}, {SP_OT_ON, 1}, {-1, 0}}, CAT_PAD},
    {"ott growl", {{SP_A_TABLE, 41}, {SP_A_POS, 700}, {SP_A_WARP, WARP_BENDP}, {SP_A_WAMT, 500}, {SP_B_ON, 1},
                   {SP_B_TABLE, 47}, {SP_B_OCT, -1}, {SP_F_ON, 1}, {SP_F_TYPE, FT_BP12}, {SP_F_CUT, 600},
                   {SP_OT_ON, 1}, {SP_DR_ON, 1}, {SP_DR_TYPE, DRV_TUBE}, {-1, 0}}, CAT_BASS},
};
#endif
