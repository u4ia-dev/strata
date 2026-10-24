/* SPDX-License-Identifier: GPL-3.0-only */
/* The test projects (V0.87.1): four built-in, read-only projects, each stressing one part of Strata, loaded from
 * DEBUG > TESTS or the console's `tproj N`, each playing 60 to 90 s and then stopping by itself:
 *   1 VOICES  8-note chords at velocity 127 on SUPERSAW, PAD, VOWEL PAD and OTT GROWL in turn (1, 2 and 4 notes first:
 *             the voices and the load as the chord grows), a filter envelope and an LFO across the wavetable;
 *   2 FX      an eight-slot chain that goes through all 13 types and all 24 instances in three chains, every knob
 *             swept to both ends by the LFOs and ENV 2 through the matrix (eight at a time), the switches and lists
 *             stepped through their values;
 *   3 DRUMS   two drum tracks over all seven models (their 15 variants, in two kits), ratchets, chance and accents,
 *             each drum's DRIVE (all four), EQ and SHAPE, the kit's ACCENT, its HOME knobs, the DUCK's SOURCE, CURVE,
 *             times and HOLD, COMP; the duck on a held PAD chord;
 *   4 LOOPER  all eight tracks (drums, bass, KEY chords, the ARP, a lead with recorded motion, SPEED x2 and x1/2, a
 *             pad, MIDI out on track 8), a SONG of five scenes with repeats, mutes and solos, VOLUME and PAN, SPEED
 *             changed as it plays, CLIP TOOLS' copy, double and halve.
 * What they are: compact data below (TP_*), built into the looper (its tracks, clips, notes and hits, recorded motion,
 * SONG), the kit and the sound when one loads (sy_tp_load: hushed and stopped, as PROJECT's LOAD), then a list of
 * timed changes the main loop takes as the transport passes them (sy_tp_tick: the sound, the mixer, SPEED, the clip
 * tools, the kit's values; TEST 2's chain and sweeps worked out from the parameter table). The LFOs and envelopes do
 * the sweeps through matrix slots (M5 .. M8; TEST 2 all eight).
 * Nothing is saved while one is loaded: the projects' auto-save, the sound playing's and the settings' (sy_app.c
 * sy_flash_ms) wait. What ends it: loading a project (PROJECT's LOAD, any of the four: the one loaded is not saved
 * first, the test's never is), or a restart. Loading another test project replaces it (still a test). sy_tp_stop,
 * the end of its time and PLAY's stop stop it playing; it stays loaded, auto-save off, until one of those. When it
 * ends, the sound that was playing before the first test (its bytes kept as the auto-save would write them), the
 * tempo and time signature and the looper's settings (QUANTIZE, COUNT-IN, CLICK, LAUNCH) come back.
 * Needs everything above it in strata.c (and web/emu/strata_web.c): the engine, sy_loop.c, sy_drum.c, sy_app.c, the
 * UI (ui, sy_ui_touch, sy_ui_project, ui_hist_push). */
#define SY_TP_N 4u
static const char *const SY_TP_NAME[SY_TP_N] = {"VOICES", "FX", "DRUMS", "LOOPER"};

/* ------------------------------------------------------------------------------------------ the data --- */
typedef struct { int16_t id, v; } tp_kv_t;               /* a parameter of the sound and its value (id -1 ends) */
typedef struct { uint8_t at, len, note, vel; } tp_note_t; /* a note: its sixteenth in the clip, its length in them
                                                           * (TP_HIT: a drum's hit, ACCENT, CHANCE, RATCHET as LP_*) */
#define TP_HIT 0x80u
typedef struct { uint8_t trk, scn, bars, speed; uint16_t n0, n; } tp_clip_t;   /* a clip: its notes n0 .. n0 + n - 1 */
typedef struct { uint8_t clip, row, vel, fl; uint16_t mask; uint8_t bars, rsv; } tp_beat_t;   /* a drum's hits: each
                                                           * sixteenth of mask, in each bar of bars (bits), fl LP_*'s */
typedef struct { uint8_t clip, step; uint16_t id; int16_t lo, hi; uint8_t shape, rsv; } tp_move_t;   /* recorded
                                                           * motion: parameter id every step sixteenths, lo .. hi along
                                                           * shape (0 a ramp, 1 a triangle, 2 a sine) */
typedef struct { uint8_t type, vol, pan, chord, out, ch; } tp_trk_t;           /* a track (chord: TP_CH_*) */
typedef struct { uint16_t at; uint8_t op, a; int16_t id, v; } tp_act_t;       /* a change at sixteenth at */
enum { TP_SOUND, TP_MUTE, TP_SOLO, TP_VOL, TP_PAN, TP_SPEED, TP_DOUBLE, TP_HALVE, TP_COPY, TP_SCENE, TP_KIT, TP_ROW,
       TP_END };
enum { TP_CH_NONE, TP_CH_KEY7, TP_CH_ARP };
typedef struct {
    uint16_t len;                                          /* sixteenths (it stops at the last: an END there) */
    uint8_t bpm, sound, kv, kit;                           /* 120; the starter sound, its changes; factory kit + 1 */
    uint8_t nclip, nbeat, nmove, nact;
    const tp_trk_t *trk;                                   /* eight */
    const tp_clip_t *clip;
    const tp_note_t *note;
    const tp_beat_t *beat;
    const tp_move_t *move;
    const tp_act_t *act;
    const uint8_t *song;                                   /* SONG: scene, repeats pairs (0xFF ends; 0: none) */
} tp_proj_t;

/* the sounds: a starter sound (sy_sounds.h, its index; 8 INIT) and these over it */
#define TPK(p, v) {SP_##p, v}
#define TP_KEND {-1, 0}
static const tp_kv_t TP_KV[] = {
    /* 0 TEST 1, SUPERSAW: ENV 2 on the cutoff, LFO 1 (FREE) down the table */
    TPK(F_CUT, 560), TPK(E2_ATK, 400), TPK(E2_DEC, 700), TPK(E2_SUS, 300), TPK(E2_REL, 600), TPK(M5_SRC, SRC_ENV2),
    TPK(M5_DST, SP_F_CUT), TPK(M5_AMT, 380), TPK(M6_SRC, SRC_LFO1), TPK(M6_DST, SP_A_POS), TPK(M6_AMT, -700),
    TPK(L1_MODE, 0), TPK(L1_RATE, 350), TPK(V_VOL, 550), TP_KEND,
    /* 1 PAD: a low pass under ENV 2, LFO 2 across OSC B's table (LFO 1 has OSC A's) */
    TPK(F_ON, 1), TPK(F_TYPE, FT_LP12), TPK(F_CUT, 520), TPK(F_RES, 250), TPK(E2_ATK, 500), TPK(E2_DEC, 720),
    TPK(E2_SUS, 300), TPK(E2_REL, 650), TPK(M5_SRC, SRC_ENV2), TPK(M5_DST, SP_F_CUT), TPK(M5_AMT, 400),
    TPK(M6_SRC, SRC_LFO2), TPK(M6_DST, SP_B_POS), TPK(M6_AMT, 500), TPK(L2_MODE, 0), TPK(L2_RATE, 300),
    TPK(V_VOL, 550), TP_KEND,
    /* 2 VOWEL PAD: ENV 2 moves the vowel, LFO 1 across the table */
    TPK(E2_ATK, 450), TPK(E2_DEC, 700), TPK(E2_SUS, 200), TPK(E2_REL, 600), TPK(M5_SRC, SRC_ENV2),
    TPK(M5_DST, SP_F_CUT), TPK(M5_AMT, 300), TPK(M6_SRC, SRC_LFO1), TPK(M6_DST, SP_A_POS), TPK(M6_AMT, 600),
    TPK(L1_MODE, 0), TPK(L1_RATE, 350), TPK(OT_LOW, 50), TPK(OT_MID, 50), TPK(OT_HIGH, 50), TP_KEND,
    /* 3 OTT GROWL */
    TPK(E2_ATK, 300), TPK(E2_DEC, 650), TPK(E2_SUS, 300), TPK(M5_SRC, SRC_ENV2), TPK(M5_DST, SP_F_CUT),
    TPK(M5_AMT, 400), TPK(M6_SRC, SRC_LFO1), TPK(M6_DST, SP_A_POS), TPK(M6_AMT, 500), TPK(L1_MODE, 0),
    TPK(L1_RATE, 350), TPK(OT_LOW, 20), TPK(OT_MID, 20), TPK(OT_HIGH, 20), TP_KEND,
    /* 4 TEST 2 (over INIT): a saw through a low pass, six voices; the LFOs FREE (SINE 0.5 Hz, TRIANGLE 0.75 Hz) and
     * ENV 2 for the sweeps (the slots, the chain: tp_fx) */
    TPK(A_TABLE, 1), TPK(A_POS, 1000), TPK(F_ON, 1), TPK(F_TYPE, FT_LP12), TPK(F_CUT, 720), TPK(F_RES, 200),
    TPK(E1_DEC, 550), TPK(E1_SUS, 500), TPK(E1_REL, 450), TPK(V_POLY, 6), TPK(E2_ATK, 450), TPK(E2_DEC, 650),
    TPK(E2_SUS, 0), TPK(E2_REL, 500), TPK(L1_SHAPE, 0), TPK(L1_MODE, 0), TPK(L1_RATE, 472), TPK(L2_SHAPE, 1),
    TPK(L2_MODE, 0), TPK(L2_RATE, 521), TPK(M1_SRC, 0), TPK(M2_SRC, 0), TPK(M3_SRC, 0), TPK(M4_SRC, 0),
    TPK(M_POL, 0xC0), TPK(V_VOL, 510), TP_KEND,
    /* 5 TEST 3: PAD lighter (two unison voices an oscillator), a slow attack: the duck's chord */
    TPK(A_UNI, 2), TPK(B_UNI, 2), TPK(E1_ATK, 500), TPK(E1_REL, 600), TPK(RV_MIX, 300), TPK(V_VOL, 560), TP_KEND,
    /* 6 TEST 4: FM KEYS through a low pass (the lead's motion moves it), a delay */
    TPK(F_ON, 1), TPK(F_TYPE, FT_LP12), TPK(F_CUT, 760), TPK(F_RES, 300), TPK(DL_ON, 1), TPK(DL_MIX, 200),
    TPK(E1_REL, 450), TPK(V_VOL, 580), TP_KEND,
};

/* --------------------------------------------------------------------------------------- TEST 1 VOICES --- */
static const tp_trk_t TP1_TRK[LP_NTRK] = {{LT_SYNTH, 100, 64, 0, 0, 0}};
#define C8(at, a, b, c, d, e, f, g, h) {at, 15, a, 127}, {at, 15, b, 127}, {at, 15, c, 127}, {at, 15, d, 127}, \
    {at, 15, e, 127}, {at, 15, f, 127}, {at, 15, g, 127}, {at, 15, h, 127}
static const tp_note_t TP1_NOTE[] = {
    {0, 15, 60, 127},                                                              /* 1 note */
    {16, 15, 55, 127}, {16, 15, 64, 127},                                          /* 2 */
    {32, 15, 48, 127}, {32, 15, 55, 127}, {32, 15, 60, 127}, {32, 15, 64, 127},    /* 4 */
    C8(48, 48, 55, 60, 64, 67, 71, 74, 79),                                        /* 8: C, Am, F, G, C */
    C8(64, 45, 52, 57, 60, 64, 67, 71, 76),
    C8(80, 41, 48, 53, 57, 60, 64, 67, 72),
    C8(96, 43, 50, 55, 59, 62, 65, 69, 74),
    C8(112, 48, 55, 60, 64, 67, 72, 76, 79),
};
static const tp_clip_t TP1_CLIP[] = {{0, 0, 9, LS_1, 0, sizeof TP1_NOTE / sizeof TP1_NOTE[0]}};
static const tp_act_t TP1_ACT[] = {                       /* (each sound's 9 bars: the next in the last bar's rest) */
    {136, TP_SOUND, 2, 0, 1}, {280, TP_SOUND, 6, 0, 2}, {424, TP_SOUND, 7, 0, 3}, {576, TP_END, 0, 0, 0},
};

/* ------------------------------------------------------------------------------------------ TEST 2 FX --- */
static const tp_trk_t TP2_TRK[LP_NTRK] = {{LT_SYNTH, 100, 64, 0, 0, 0}};
#define TP2_BAR(b, r, m) {16 * (b), 15, 36 + (r), 100}, {16 * (b), 3, 48 + (r), 110}, {16 * (b), 3, 52 + (r) - (m), 110}, \
    {16 * (b), 3, 55 + (r), 110}, {16 * (b) + 4, 1, 60 + (r), 96}, {16 * (b) + 6, 1, 64 + (r) - (m), 90},              \
    {16 * (b) + 8, 1, 67 + (r), 96}, {16 * (b) + 10, 2, 72 + (r), 104}, {16 * (b) + 12, 1, 67 + (r), 90},             \
    {16 * (b) + 14, 1, 64 + (r) - (m), 96}
static const tp_note_t TP2_NOTE[] = {TP2_BAR(0, 0, 0), TP2_BAR(1, 9, 1), TP2_BAR(2, 5, 0), TP2_BAR(3, 7, 0)};
static const tp_clip_t TP2_CLIP[] = {{0, 0, 4, LS_1, 0, sizeof TP2_NOTE / sizeof TP2_NOTE[0]}};
static const tp_act_t TP2_ACT[] = {{576, TP_END, 0, 0, 0}};
/* the three chains: every instance once, all 13 types in the first two */
static const uint8_t TP2_CHAIN[3][NFXC] = {
    {FXI_DRIVE, FXI_CHORUS, FXI_DELAY, FXI_REVERB, FXI_OTT, FXI_EQ, FXI_FLANGER, FXI_PHASER},
    {FXI_FFILT, FXI_CRUSH, FXI_TREM, FXI_RING, FXI_TAPE, FXI_DRIVE2, FXI_CHORUS2, FXI_EQ2},
    {FXI_OTT2, FXI_FLANGER2, FXI_PHASER2, FXI_FFILT2, FXI_CRUSH2, FXI_TREM2, FXI_RING2, FXI_TAPE2}};
#define TP2_PH 192u                                       /* a chain's sixteenths (24 s at 120) */
#define TP2_STEP 8u                                       /* the switches and lists stepped every 2 quarters */

/* --------------------------------------------------------------------------------------- TEST 3 DRUMS --- */
static const tp_trk_t TP3_TRK[LP_NTRK] = {{LT_DRUM, 88, 64, 0, 0, 0}, {LT_DRUM, 76, 40, 0, 0, 0},
                                          {LT_SYNTH, 80, 64, 0, 0, 0}};
static const tp_note_t TP3_NOTE[] = {                     /* the pad: a chord of four held each clip */
    {0, 31, 48, 100}, {0, 31, 55, 100}, {0, 31, 60, 100}, {0, 31, 64, 100},
    {0, 31, 45, 100}, {0, 31, 52, 100}, {0, 31, 57, 100}, {0, 31, 60, 100},
    {0, 31, 41, 100}, {0, 31, 48, 100}, {0, 31, 53, 100}, {0, 31, 57, 100},
    {0, 31, 43, 100}, {0, 31, 50, 100}, {0, 31, 55, 100}, {0, 31, 59, 100},
};
static const tp_clip_t TP3_CLIP[] = {
    {0, 0, 1, LS_1, 0, 0}, {0, 1, 2, LS_1, 0, 0}, {0, 2, 2, LS_1, 0, 0}, {0, 3, 2, LS_1, 0, 0},   /* 0-3 T1 A-D */
    {1, 1, 2, LS_1, 0, 0}, {1, 2, 2, LS_1, 0, 0}, {1, 3, 1, LS_2, 0, 0},                          /* 4-6 T2 B-D */
    {2, 0, 2, LS_1, 0, 4}, {2, 1, 2, LS_1, 4, 4}, {2, 2, 2, LS_1, 8, 4}, {2, 3, 2, LS_1, 12, 4},  /* 7-10 the pad */
};
#define ACC LP_ACC
#define CHN(k) ((k) << 2)
static const tp_beat_t TP3_BEAT[] = {
    /* T1 A: a plain beat, accents on the ones */
    {0, DK_KICK, 112, ACC, 0x0001, 1}, {0, DK_KICK, 104, 0, 0x1110, 1}, {0, DK_SNARE, 100, 0, 0x1010, 1},
    {0, DK_HATC, 78, 0, 0x1555, 1}, {0, DK_HATO, 84, 0, 0x4000, 1},
    /* T1 B: ratchets and chance on the hats, a fill, the cymbal */
    {1, DK_KICK, 110, ACC, 0x0001, 3}, {1, DK_KICK, 100, 0, 0x0110, 3}, {1, DK_KICK, 96, 0, 0x0400, 2},
    {1, DK_SNARE, 102, 0, 0x1010, 3}, {1, DK_SNARE, 60, CHN(4), 0x8080, 3}, {1, DK_CLAP, 92, 0, 0x1000, 2},
    {1, DK_HATC, 70, 0, 0x2222, 3}, {1, DK_HATC, 76, 1, 0x0404, 3}, {1, DK_HATC, 72, 3, 0x4000, 2},
    {1, DK_HATC, 64, CHN(3), 0x0808, 3}, {1, DK_HATO, 80, 0, 0x0040, 3}, {1, DK_CYM, 96, ACC, 0x0001, 1},
    {1, DK_WT1, 90, 0, 0x1100, 2}, {1, DK_WT2, 94, 2, 0x4400, 2},
    /* T1 C (the second kit): busier, accents across */
    {2, DK_KICK, 112, ACC, 0x0101, 3}, {2, DK_KICK, 100, 0, 0x1010, 1}, {2, DK_KICK, 100, 0, 0x1210, 2},
    {2, DK_SNARE, 100, ACC, 0x1010, 3}, {2, DK_CLAP, 90, 0, 0x0800, 3}, {2, DK_HATC, 80, 1, 0xAAAA, 3},
    {2, DK_HATO, 82, CHN(2), 0x5555, 3}, {2, DK_CYM, 90, 0, 0x0001, 2}, {2, DK_WT1, 92, 0, 0x2020, 3},
    {2, DK_WT2, 88, 3, 0x8000, 2},
    /* T1 D: the DUCK row on the quarters (its SOURCE ROW), the rest as C's */
    {3, DK_KICK, 112, ACC, 0x0001, 3}, {3, DK_KICK, 100, 0, 0x1110, 3}, {3, DK_SNARE, 100, 0, 0x1010, 3},
    {3, DK_HATC, 76, 2, 0x4444, 3}, {3, DK_HATC, 70, CHN(5), 0xAAAA, 3}, {3, DK_HATO, 84, 0, 0x0010, 2},
    {3, DK_WT1, 90, 1, 0x0100, 2}, {3, DK_WT2, 90, 0, 0x0001, 2}, {3, DK_DUCK, 100, 0, 0x1111, 3},
    /* T2: percussion over the second kit's rows */
    {4, DK_CYM, 70, 0, 0x4444, 3}, {4, DK_WT1, 80, CHN(2), 0x0A0A, 3}, {4, DK_WT2, 84, 1, 0x2000, 2},
    {5, DK_HATO, 72, 0, 0x0808, 3}, {5, DK_WT1, 86, 0, 0x1248, 3}, {5, DK_CYM, 80, ACC, 0x0001, 1},
    {6, DK_CLAP, 84, 2, 0x1010, 1}, {6, DK_WT2, 80, CHN(1), 0x5050, 1},
};
#undef ACC
#undef CHN
#define TPKO(f) ((int16_t)__builtin_offsetof(dk_kit_t, f))
#define TPFX(r, f) ((int16_t)(__builtin_offsetof(dk_kit_t, fx) + (r) * sizeof(dk_fx_t) + __builtin_offsetof(dk_fx_t, f)))
static const tp_act_t TP3_ACT[] = {
    /* the first kit (at the start): ANALOG BODY, PUNCH SNARE and CLAP, METAL CLOSED, OPEN and CYMBAL, WAVE A, FM DRUM;
     * the duck from the KICK, EXP */
    {0, TP_ROW, 0, KP_808K, 0}, {0, TP_ROW, 1, KP_909S, 0}, {0, TP_ROW, 2, KP_808C, 0}, {0, TP_ROW, 3, KP_808H, 0},
    {0, TP_ROW, 4, KP_808O, 0}, {0, TP_ROW, 5, KP_808CY, 0}, {0, TP_ROW, 6, KP_TABA, 0}, {0, TP_ROW, 7, KP_FMT, 0},
    {0, TP_KIT, 0, TPKO(amount), 70}, {0, TP_KIT, 0, TPKO(shape), DS_EXP}, {0, TP_KIT, 0, TPKO(dsrc), DSRC_KICK},
    {0, TP_KIT, 0, TPKO(accent), 50}, {0, TP_SCENE, 0, 0, 0},
    {64, TP_KIT, 0, TPKO(accent), 100}, {64, TP_KIT, 0, TPKO(atone), 90},           /* KIT ACCENT at its top */
    {96, TP_KIT, 0, TPKO(shape), DS_LIN}, {96, TP_KIT, 0, TPKO(release), 15},      /* the duck's CURVE, times */
    {124, TP_SCENE, 1, 0, 0},
    {128, TP_KIT, 0, TPKO(shape), DS_PUMP}, {128, TP_KIT, 0, TPKO(amount), 90}, {128, TP_KIT, 0, TPKO(dhold), 40},
    {192, TP_KIT, 0, TPKO(accent), 0}, {192, TP_KIT, 0, TPKO(comp[0]), 60}, {192, TP_KIT, 0, TPKO(comp[1]), 10},
    {192, TP_KIT, 0, TPKO(comp[2]), 50},                                           /* COMP on */
    {252, TP_SCENE, 2, 0, 0},
    /* the second kit: PUNCH KICK, FM BELL, NOISE, CHIP NOISE, SQUARE and TRIANGLE, WAVE B, FM BELL; each drum's
     * effects: DRIVE SOFT, HARD, FOLD, CRUSH; EQ's ends; SHAPE's ends */
    {256, TP_ROW, 0, KP_909K, 0}, {256, TP_ROW, 1, KP_RIM, 0}, {256, TP_ROW, 2, KP_SNAP, 0},
    {256, TP_ROW, 3, KP_CHIPH, 0}, {256, TP_ROW, 4, KP_LASER, 0}, {256, TP_ROW, 5, KP_CHIPT, 0},
    {256, TP_ROW, 6, KP_TABB, 0}, {256, TP_ROW, 7, KP_COW, 0}, {256, TP_KIT, 0, TPKO(dsrc), DSRC_BOTH},
    {256, TP_KIT, 0, TPFX(0, drive), 70}, {256, TP_KIT, 0, TPFX(1, dtype), DD_HARD}, {256, TP_KIT, 0, TPFX(1, drive), 60},
    {256, TP_KIT, 0, TPFX(2, dtype), DD_FOLD}, {256, TP_KIT, 0, TPFX(2, drive), 50},
    {256, TP_KIT, 0, TPFX(3, dtype), DD_CRUSH}, {256, TP_KIT, 0, TPFX(3, drive), 80},
    {256, TP_KIT, 0, TPFX(4, lo), 100}, {256, TP_KIT, 0, TPFX(4, hi), 0}, {256, TP_KIT, 0, TPFX(5, lo), 0},
    {256, TP_KIT, 0, TPFX(5, mid), 100}, {256, TP_KIT, 0, TPFX(5, hi), 100}, {256, TP_KIT, 0, TPFX(5, freq), 30},
    {256, TP_KIT, 0, TPFX(6, att), 100}, {256, TP_KIT, 0, TPFX(6, sus), 0}, {256, TP_KIT, 0, TPFX(7, att), 0},
    {256, TP_KIT, 0, TPFX(7, sus), 100},
    {320, TP_KIT, 0, TPKO(comp[0]), 100}, {320, TP_KIT, 0, TPKO(comp[3]), 50},     /* COMP at its top, in parallel */
    {380, TP_SCENE, 3, 0, 0},
    {384, TP_KIT, 0, TPKO(dsrc), DSRC_ROW}, {384, TP_KIT, 0, TPKO(shape), DS_EXP}, {384, TP_KIT, 0, TPKO(amount), 100},
    {384, TP_KIT, 0, TPKO(attack), 50}, {384, TP_KIT, 0, TPKO(release), 100},       /* the duck at its ends */
    {416, TP_KIT, 0, TPKO(ktune), 12}, {416, TP_KIT, 0, TPKO(kdecay), 100}, {416, TP_KIT, 0, TPKO(ktone), 50},
    {448, TP_KIT, 0, TPKO(ktune), -12}, {448, TP_KIT, 0, TPKO(kdecay), 0}, {448, TP_KIT, 0, TPKO(ktone), -50},
    {448, TP_KIT, 0, TPFX(0, dtype), DD_CRUSH}, {448, TP_KIT, 0, TPFX(3, dtype), DD_SOFT},
    {448, TP_KIT, 0, TPFX(4, lo), 0}, {448, TP_KIT, 0, TPFX(4, hi), 100}, {448, TP_KIT, 0, TPFX(6, att), 0},
    {448, TP_KIT, 0, TPFX(6, sus), 100}, {448, TP_KIT, 0, TPKO(attack), 0}, {448, TP_KIT, 0, TPKO(release), 0},
    {480, TP_KIT, 0, TPKO(ktune), 0}, {480, TP_KIT, 0, TPKO(kdecay), 50}, {480, TP_KIT, 0, TPKO(ktone), 0},
    {508, TP_SCENE, 2, 0, 0}, {512, TP_KIT, 0, TPKO(comp[0]), 30}, {512, TP_KIT, 0, TPKO(release), 45},
    {576, TP_END, 0, 0, 0},
};

/* -------------------------------------------------------------------------------------- TEST 4 LOOPER --- */
static const tp_trk_t TP4_TRK[LP_NTRK] = {
    {LT_DRUM, 100, 64, 0, 0, 0},  {LT_SYNTH, 100, 64, 0, 0, 1}, {LT_SYNTH, 78, 40, TP_CH_KEY7, 0, 2},
    {LT_SYNTH, 74, 92, TP_CH_ARP, 0, 3}, {LT_SYNTH, 90, 64, 0, 0, 4}, {LT_SYNTH, 70, 20, 0, 0, 5},
    {LT_SYNTH, 68, 108, 0, 0, 6}, {LT_MIDI, 100, 64, 0, 0, 7}};
static const tp_note_t TP4_NOTE[] = {
    /* 0 bass A (2 bars: C, Am) */
    {0, 1, 36, 104}, {2, 1, 48, 96}, {4, 1, 36, 104}, {6, 1, 48, 96}, {8, 1, 36, 104}, {10, 1, 48, 96},
    {12, 1, 43, 100}, {14, 1, 48, 96}, {16, 1, 33, 104}, {18, 1, 45, 96}, {20, 1, 33, 104}, {22, 1, 45, 96},
    {24, 1, 33, 104}, {26, 1, 45, 96}, {28, 1, 40, 100}, {30, 1, 45, 96},
    /* 16 bass C (2 bars: F, G, syncopated) */
    {0, 2, 29, 108}, {3, 2, 41, 96}, {6, 2, 29, 100}, {8, 2, 36, 96}, {11, 2, 41, 100}, {14, 2, 29, 96},
    {16, 2, 31, 108}, {19, 2, 43, 96}, {22, 2, 31, 100}, {24, 2, 38, 96}, {27, 2, 43, 100}, {30, 2, 31, 96},
    /* 28 bass E (1 bar of eighths, doubled by CLIP TOOLS as it plays) */
    {0, 1, 36, 110}, {2, 1, 36, 90}, {4, 1, 48, 100}, {6, 1, 36, 90}, {8, 1, 36, 110}, {10, 1, 43, 90},
    {12, 1, 48, 100}, {14, 1, 46, 90},
    /* 36 chords B (KEY: each note its 7th chord in C) */
    {0, 7, 60, 96}, {8, 7, 64, 90}, {16, 7, 57, 96}, {24, 7, 65, 90},
    /* 40 chords E */
    {0, 3, 65, 100}, {4, 3, 67, 92}, {8, 3, 69, 100}, {12, 3, 67, 92},
    /* 44 the ARP's chords (held) */
    {0, 15, 60, 100}, {0, 15, 64, 100}, {0, 15, 67, 100}, {16, 15, 57, 100}, {16, 15, 60, 100}, {16, 15, 64, 100},
    /* 50 the lead (4 bars) */
    {0, 4, 72, 110}, {4, 2, 74, 100}, {6, 2, 76, 104}, {8, 8, 79, 112}, {16, 4, 77, 104}, {20, 4, 76, 100},
    {24, 8, 74, 108}, {32, 6, 72, 110}, {38, 2, 74, 100}, {40, 8, 76, 104}, {48, 4, 79, 112}, {52, 4, 81, 108},
    {56, 8, 79, 110},
    /* 63 the counter-melody (2 bars, at x2) */
    {0, 2, 67, 96}, {4, 2, 64, 90}, {8, 2, 67, 96}, {12, 2, 69, 90}, {16, 2, 67, 96}, {20, 2, 64, 90},
    {24, 2, 62, 96}, {28, 2, 60, 90},
    /* 71 the pad (a bar at x1/2) */
    {0, 15, 55, 84}, {0, 15, 59, 84},
    /* 73 MIDI out (track 8) */
    {0, 2, 60, 100}, {4, 2, 64, 100}, {8, 2, 67, 100}, {12, 2, 72, 100},
};
static const tp_clip_t TP4_CLIP[] = {
    {0, 0, 1, LS_1, 0, 0}, {1, 0, 2, LS_1, 0, 16}, {7, 0, 1, LS_1, 73, 4},                                  /* A */
    {0, 1, 1, LS_1, 0, 0}, {1, 1, 2, LS_1, 0, 16}, {2, 1, 2, LS_1, 36, 4}, {3, 1, 2, LS_1, 44, 6},
    {7, 1, 1, LS_1, 73, 4},                                                                                  /* B */
    {0, 2, 2, LS_1, 0, 0}, {1, 2, 2, LS_1, 16, 12}, {2, 2, 2, LS_1, 36, 4}, {3, 2, 2, LS_1, 44, 6},
    {4, 2, 4, LS_1, 50, 13}, {5, 2, 2, LS_2, 63, 8}, {6, 2, 1, LS_HALF, 71, 2}, {7, 2, 1, LS_1, 73, 4},     /* C */
    {3, 3, 2, LS_1, 44, 6}, {4, 3, 4, LS_1, 50, 13}, {6, 3, 1, LS_HALF, 71, 2},                            /* D */
    {0, 4, 1, LS_1, 0, 0}, {1, 4, 1, LS_1, 28, 8}, {2, 4, 1, LS_1, 40, 4}, {3, 4, 2, LS_1, 44, 6},
    {4, 4, 4, LS_1, 50, 13}, {5, 4, 2, LS_2, 63, 8}, {6, 4, 1, LS_HALF, 71, 2}, {7, 4, 1, LS_1, 73, 4},     /* E */
};
#define ACC LP_ACC
#define CHN(k) ((k) << 2)
static const tp_beat_t TP4_BEAT[] = {
    {0, DK_KICK, 112, 0, 0x1111, 1}, {0, DK_SNARE, 100, 0, 0x1010, 1}, {0, DK_HATC, 76, 0, 0x5555, 1},
    {0, DK_HATO, 82, 0, 0x4000, 1},
    {3, DK_KICK, 112, 0, 0x1111, 1}, {3, DK_SNARE, 100, 0, 0x1010, 1}, {3, DK_HATC, 76, 0, 0x5555, 1},
    {3, DK_HATO, 82, 0, 0x4000, 1}, {3, DK_CLAP, 86, 0, 0x1000, 1},
    {8, DK_KICK, 114, ACC, 0x0001, 3}, {8, DK_KICK, 104, 0, 0x1110, 3}, {8, DK_KICK, 92, 0, 0x0480, 2},
    {8, DK_SNARE, 100, 0, 0x1010, 3}, {8, DK_CLAP, 90, 0, 0x1010, 2}, {8, DK_HATC, 66, 0, 0xBBBB, 3},
    {8, DK_HATC, 84, ACC, 0x4444, 3}, {8, DK_HATO, 80, 0, 0x4040, 1}, {8, DK_CYM, 92, 0, 0x0001, 1},
    {8, DK_WT1, 88, 0, 0xA000, 2}, {8, DK_WT2, 92, 0, 0x5000, 2},
    {19, DK_KICK, 112, 0, 0x1111, 1}, {19, DK_SNARE, 100, 0, 0x1010, 1}, {19, DK_CLAP, 96, ACC, 0x1000, 1},
    {19, DK_HATC, 76, 0, 0x1414, 1}, {19, DK_HATC, 70, 2, 0x4040, 1}, {19, DK_HATC, 64, CHN(3), 0x0101, 1},
};
#undef ACC
#undef CHN
static const tp_move_t TP4_MOVE[] = {                     /* recorded motion on the lead's clips */
    {12, 1, SP_F_CUT, 300, 950, 1}, {12, 2, SP_A_WAMT, 100, 800, 2},    /* C: the cutoff up and down, FM's depth */
    {17, 2, SP_F_RES, 0, 750, 0},                                       /* D: the resonance up */
    {23, 2, SP_A_POS, 0, 1000, 2}, {23, 1, SP_F_CUT, 500, 900, 2},      /* E: the table's place, the cutoff */
};
static const tp_act_t TP4_ACT[] = {
    {80, TP_MUTE, 1, 0, 1}, {96, TP_MUTE, 1, 0, 0},                      /* the bass muted a bar */
    {128, TP_SOLO, 4, 0, 1}, {144, TP_SOLO, 4, 0, 0},                    /* the lead soloed a bar */
    {160, TP_VOL, 2, 0, 40}, {176, TP_VOL, 2, 0, 100}, {176, TP_PAN, 3, 0, 0}, {192, TP_PAN, 3, 0, 127},
    {192, TP_SPEED, 4, 2, LS_HALF}, {208, TP_PAN, 3, 0, 92}, {224, TP_SPEED, 4, 2, LS_1},   /* the lead at x1/2 */
    {240, TP_COPY, 2, 1 | 2 << 4 | 4 << 8, -1},                          /* CLIP TOOLS: chords B into E, */
    {256, TP_DOUBLE, 1, 4, 0},                                            /* .. E's bass doubled, */
    {272, TP_HALVE, 4, 4, 0},                                             /* .. E's lead halved */
    {320, TP_SPEED, 5, 4, LS_1},                                          /* the counter-melody back to x1 */
    {352, TP_MUTE, 0, 0, 1}, {352, TP_SOLO, 1, 0, 1}, {368, TP_MUTE, 0, 0, 0}, {368, TP_SOLO, 1, 0, 0},
    {368, TP_SOLO, 5, 0, 1}, {384, TP_SOLO, 5, 0, 0},
    {416, TP_VOL, 0, 0, 70}, {448, TP_VOL, 0, 0, 100}, {448, TP_PAN, 6, 0, 20}, {480, TP_PAN, 6, 0, 108},
    {544, TP_END, 0, 0, 0},
};
static const uint8_t TP4_SONG[] = {0, 2, 1, 1, 2, 2, 3, 1, 4, 2, 2, 1, 0, 2, 0xFF};   /* A x2 B C x2 D E x2 C A x2 */

#define TPN(a) (uint8_t)(sizeof a / sizeof a[0])
static const tp_proj_t TP_PROJ[SY_TP_N] = {
    {576, 120, 0, 0, 0, TPN(TP1_CLIP), 0, 0, TPN(TP1_ACT), TP1_TRK, TP1_CLIP, TP1_NOTE, 0, 0, TP1_ACT, 0},
    {576, 120, 8, 4, 0, TPN(TP2_CLIP), 0, 0, TPN(TP2_ACT), TP2_TRK, TP2_CLIP, TP2_NOTE, 0, 0, TP2_ACT, 0},
    {576, 120, 2, 5, 1, TPN(TP3_CLIP), TPN(TP3_BEAT), 0, TPN(TP3_ACT), TP3_TRK, TP3_CLIP, TP3_NOTE, TP3_BEAT, 0,
     TP3_ACT, 0},
    {544, 120, 3, 6, 2, TPN(TP4_CLIP), TPN(TP4_BEAT), TPN(TP4_MOVE), TPN(TP4_ACT), TP4_TRK, TP4_CLIP, TP4_NOTE,
     TP4_BEAT, TP4_MOVE, TP4_ACT, TP4_SONG},
};

/* ---------------------------------------------------------------------------------------- the state --- */
enum { TPS_NONE, TPS_LOAD, TPS_WAIT, TPS_RUN, TPS_DONE };
static uint8_t sy_tp_on;                         /* the test project loaded + 1, 0 none */
static struct {
    uint8_t st, k;                               /* TPS_*; the one under way (TPS_LOAD: the one to load) */
    uint8_t pc;                                  /* its next change */
    uint8_t had;                                 /* the owner's state below is kept */
    uint8_t ran;                                 /* the transport seen running */
    uint8_t ph, grp, stp, glen;                  /* TEST 2: its chain, its group of slots, the switches' step; a
                                                  * group's sixteenths */
    uint16_t cur[NSLOT];                         /* .. the parameters the slots sweep */
    uint32_t t0, t1;                             /* its start; its end (TPS_DONE) */
    uint16_t bpm;                                /* the owner's: the tempo, TS-BEAT, TS-NOTE; the looper's settings */
    uint8_t tsb, tsn, quant, cin, click, launch, edited;
} tp;
static uint8_t tp_was[3u + SP_PACK_MAX] SY_POOL;   /* .. the sound playing as the auto-save writes it (sy_edit_pack) */
static uint32_t tp_was_n;

static const char *sy_tp_name(uint32_t k) { return k < SY_TP_N ? SY_TP_NAME[k] : ""; }
static int sy_tp_active(void) { return sy_tp_on; }
/* its running time: since its transport started, still while it plays (0 before; its whole once it stopped) */
static uint32_t sy_tp_ms(uint32_t k)
{
    if (!sy_tp_on || k + 1u != sy_tp_on || tp.st < TPS_RUN)
        return 0u;
    return (tp.st == TPS_RUN ? fm1_ms : tp.t1) - tp.t0;
}

/* --------------------------------------------------------------------------------------- the building --- */
/* (out of line, as the others with a patch on the stack: their frames not in the main loop's, under every frame it
 * draws; sy_app.c sy_app_boot) */
static SY_NOINLINE void tp_sound(uint32_t k, uint32_t set, uint32_t n)   /* starter sound k, change set over it, named
                                                                        * "Tn ..." */
{
    sy_patch_t p;
    uint32_t i = 0, j, chain = 0;
    sy_sound_patch(k, &p);
    for (j = 0; j < set; i++)                    /* (set's first row: past set ends) */
        j += TP_KV[i].id < 0;
    for (; TP_KV[i].id >= 0; i++) {
        p.p[TP_KV[i].id] = sp_clamp((uint32_t)TP_KV[i].id, TP_KV[i].v);
        chain |= TP_KV[i].id >= (int16_t)SP_FXC1 && TP_KV[i].id <= (int16_t)SP_FXC8;
    }
    if (chain)
        sp_chain_fix(p.p);
    else
        sp_chain_from_old(&p);                   /* (an effect switched on among the six: in the chain) */
    {                                            /* ("T1 SUPERSAW": the test's, the starter's; INIT's "T2 TEST") */
        const char *nm = k + 1u < SY_NFACT ? SOUNDS[k].name : "TEST";
        p.name[0] = 'T';
        p.name[1] = (char)('1' + n);
        p.name[2] = ' ';
        for (i = 3, j = 0; i < 16u && nm[j]; i++, j++)
            p.name[i] = (char)(nm[j] >= 'a' && nm[j] <= 'z' ? nm[j] - 32 : nm[j]);
        p.name[i] = 0;
    }
    sy_patch_load(&p);
    sy_sound = k < SY_NFACT ? k : SY_NFACT - 1u;
    sy_slot = -1;
    ui.edited = 0;
    sy_ui_touch();
}

static void tp_chord(uint32_t t, uint32_t c)     /* track t's chords as they start, then preset c (TP_CH_*) */
{
    ch_use(t);
    ch_defaults();
    if (c == TP_CH_KEY7) {
        chd.mode = CH_KEY, chd.ktype = KT_7TH, chd.spread = 1, chd.mask = 0xAB5u, chd.root = 0;   /* (C major) */
    } else if (c == TP_CH_ARP) {
        chd.perf = 1, chd.style = PF_ARP, chd.move[PF_ARP] = AR_UPDN, chd.feel[PF_ARP] = 2, chd.gate = 60;
        chd.mask = 0xAB5u, chd.root = 0;
    }
    chd.gen++;
}

/* event e of clip c: a note at sixteenth `at` of length len (TP_HIT: a hit), note, velocity */
static int tp_ev(lp_clip_t *c, uint32_t t, uint32_t len, uint32_t note, uint32_t vel)
{
    uint32_t e = lp_alloc();
    if (e == LP_END)
        return 0;
    lp_ev[e].t = (uint16_t)(t & LP_MAXT);
    lp_ev[e].len = (uint16_t)(len & TP_HIT ? LP_HIT | (len & 0x3Fu) : (len ? len : 1u) * LP_STEP - 2u);
    lp_ev[e].a = (uint8_t)note;
    lp_ev[e].b = (uint8_t)vel;
    lp_insert(c, e);
    return 1;
}
static void tp_move(lp_clip_t *c, const tp_move_t *m)   /* recorded motion into clip c */
{
    uint32_t n = c->len / (m->step * LP_STEP), i;
    for (i = 0; i < n; i++) {
        float u = (float)i / (float)n, w;
        uint32_t e = lp_alloc();
        int32_t v;
        if (e == LP_END)
            return;
        w = m->shape == 0u ? u : m->shape == 1u ? 1.0f - sy_absf(2.0f * u - 1.0f) : 0.5f - 0.5f * sy_cos2pi(u);
        v = m->lo + (int32_t)((float)(m->hi - m->lo) * w + 0.5f);
        lp_ev[e].t = (uint16_t)(LP_MOVE | (i * m->step * LP_STEP));
        lp_ev[e].len = m->id;
        lp_ev[e].a = (uint8_t)((uint16_t)v & 0xFFu);
        lp_ev[e].b = (uint8_t)((uint16_t)v >> 8);
        lp_insert(c, e);
    }
}

static void tp_exec(const tp_act_t *a);
static void tp_fx(uint32_t q);

/* the looper, the kit, the chords, the tempo and the sound as test k has them (the main loop's; hushed, stopped) */
static SY_NOINLINE void tp_build(uint32_t k)
{
    const tp_proj_t *p = &TP_PROJ[k];
    uint32_t t, i, j;
    if (!tp.had) {                               /* (the first test since the owner's own: what comes back after) */
        tp_was_n = sy_edit_pack(tp_was);
        tp.bpm = ui.bpm, tp.tsb = ui.tsb, tp.tsn = ui.tsn;
        tp.quant = lpg.quant, tp.cin = lpg.cin, tp.click = lpg.click, tp.launch = lpg.launch;
        tp.edited = ui.edited;
        ui_hist_push();                          /* (SAVE + OCT- takes the sound before back, as a preset's load) */
        tp.had = 1;
    }
    ui.bpm = p->bpm, ui.tsb = 4, ui.tsn = 2;     /* (4/4) */
    sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
    lpg.quant = LQ_OFF, lpg.cin = 0, lpg.click = LC_OFF, lpg.launch = LL_BAR;
    sy_ev_push(&sy_ev_ui, SY_EV_PANIC, 0u, 0u);  /* (the chords' sides, the voices, the drums: from scratch) */
    lp_project_clear();
    if (p->kit)
        dk_kit_load(p->kit - 1u);
    for (t = 0; t < LP_NTRK; t++) {
        const tp_trk_t *r = &p->trk[t];
        lp_trk_t *d = &lpp.trk[t];
        d->type = r->vol ? r->type : LT_SYNTH;
        d->vol = r->vol ? r->vol : 100u;
        d->pan = r->vol ? r->pan : 64u;
        d->out = r->out, d->ch = r->ch;
        d->scale = r->chord ? 1u : 0u;           /* (SCALE MAJOR where the chords are: theirs C major) */
        tp_chord(t, r->chord);
    }
    ch_use(0);
    for (i = 0; i < p->nclip; i++) {
        const tp_clip_t *c = &p->clip[i];
        lp_clip_t *d = &lpp.clip[c->trk % LP_NTRK][c->scn % LP_NSCN];
        d->len = (uint16_t)(c->bars * lp_bar_ticks());
        lpp.speed[c->trk % LP_NTRK][c->scn % LP_NSCN] = c->speed;
        for (j = 0; j < c->n; j++) {
            const tp_note_t *n = &p->note[c->n0 + j];
            tp_ev(d, n->at * LP_STEP, n->len, n->note, n->vel);
        }
        for (j = 0; j < p->nbeat; j++) {         /* (its drums' hits) */
            const tp_beat_t *b = &p->beat[j];
            uint32_t bar, s;
            if (b->clip != i)
                continue;
            for (bar = 0; bar < c->bars; bar++)
                for (s = 0; s < 16u && (b->bars >> bar & 1u); s++)
                    if (b->mask >> s & 1u)
                        tp_ev(d, (bar * 16u + s) * LP_STEP, TP_HIT | b->fl, DK_NOTE[b->row], b->vel);
        }
        for (j = 0; j < p->nmove; j++)
            if (p->move[j].clip == i)
                tp_move(d, &p->move[j]);
    }
    if (p->song) {                               /* (SONG from the first bar; lp_project_clear ended it after 0) */
        for (i = 0; i < LP_NSONG && p->song[2u * i] != 0xFFu; i++)
            lpp.song[i] = p->song[2u * i], lpp.srep[i] = p->song[2u * i + 1u];
        lpp.song_on = 1;
    } else {
        lp_launch_scene(0);                      /* (scene A on: it plays from the start) */
    }
    lpp.sel = 0;
    lpp.scn = 0;
    tp_sound(p->sound, p->kv, k);
    tp.pc = 0;
    tp.ph = tp.grp = tp.stp = 0xFF;
    memset(tp.cur, 0xFF, sizeof tp.cur);
    while (tp.pc < p->nact && p->act[tp.pc].at == 0u)   /* (the changes at 0 before it plays: the first kit, a scene) */
        tp_exec(&p->act[tp.pc++]);
    sy_tp_on = (uint8_t)(k + 1u);
    sy_ui_project(1, 0);                         /* (the UI: its track, its pages) */
    lpx.seq++;
}

/* --------------------------------------------------------------------------------------- the changes --- */
static void tp_exec(const tp_act_t *a)
{
    uint32_t t = a->a % LP_NTRK;
    switch (a->op) {
    case TP_SOUND: tp_sound(a->a, (uint32_t)a->v, tp.k); break;
    case TP_MUTE: lpp.trk[t].mute = a->v != 0; break;
    case TP_SOLO: lpp.trk[t].solo = a->v != 0; break;
    case TP_VOL: lpp.trk[t].vol = (uint8_t)(a->v & 127); break;
    case TP_PAN: lpp.trk[t].pan = (uint8_t)sy_clampi(a->v, 1, 127); break;
    case TP_SPEED: lp_speed_set(t, (uint32_t)a->id, (uint32_t)a->v); break;
    case TP_DOUBLE: lp_clip_double(t, (uint32_t)a->id); break;
    case TP_HALVE: lp_clip_halve(t, (uint32_t)a->id, 1); break;
    case TP_COPY: lp_clip_copy(t, (uint32_t)a->id & 15u, ((uint32_t)a->id >> 4) & 15u, (uint32_t)a->id >> 8, a->v); break;
    case TP_SCENE: lp_launch_scene(a->a); break;
    case TP_KIT:
        if (a->id >= 0 && (uint32_t)a->id < sizeof dkk) {
            ((uint8_t *)&dkk)[a->id] = (uint8_t)a->v;   /* (a byte the audio side reads whole at a hit) */
            dk_sane();
        }
        break;
    case TP_ROW:
        if ((uint32_t)a->id < DK_NPRE)
            dk_row_set(t, &DKP[a->id], (uint32_t)a->id + 1u);
        break;
    default:                                     /* TP_END: it stops */
        sy_app_run(0);
        break;
    }
    lpx.seq++;
    ui.dirty = D_ALL;
}

/* TEST 2: chain ph's parameters of a kind (the matrix's destinations, or the switches and lists: neither ON nor
 * those kept for their ids), into ids; their count */
static uint32_t tp_fx_ids(uint32_t ph, int mod, uint16_t *ids, uint32_t max)
{
    uint32_t s, n = 0, j, m;
    uint16_t q[SP_FX_MAXP];
    for (s = 0; s < NFXC; s++) {
        uint32_t inst = TP2_CHAIN[ph][s];
        m = sp_inst_ids(inst, q);
        for (j = 0; j < m && n < max; j++)
            if (q[j] != FXI_ON[inst] && !(SP[q[j]].flags & F_OLD) && ((SP[q[j]].flags & F_MOD) != 0) == (mod != 0))
                ids[n++] = q[j];
    }
    return n;
}
/* TEST 2 at sixteenth q: its chain (one of three, 24 s each), its eight slots on the next eight knobs (each knob at
 * its middle, the slot's amount 100 % either way: it reaches both ends; three slots LFO 1, three LFO 2, two ENV 2
 * turned to -1 .. 1), the switches and lists stepped (a list through its values, a range between its ends) */
static void tp_fx(uint32_t q)
{
    uint16_t ids[64];
    uint32_t ph = q / TP2_PH, n = 0, g, s, fl, st = q / TP2_STEP;
    if (ph >= 3u || (ph == tp.ph && (q % TP2_PH) / tp.glen == tp.grp && (uint8_t)st == tp.stp))
        return;                                  /* (nothing to change: most milliseconds) */
    n = tp_fx_ids(ph, 1, ids, 64u);
    fl = LP_LOCK();                              /* (a chain or a slot whole before the audio side looks) */
    if (ph != tp.ph) {
        for (s = 0; s < NFXC; s++) {
            sy_patch.p[SP_FXC1 + s] = (int16_t)(TP2_CHAIN[ph][s] + 1u);
            sy_patch.p[FXI_ON[TP2_CHAIN[ph][s]]] = 1;
        }
        tp.ph = (uint8_t)ph;
        g = (n + NSLOT - 1u) / NSLOT;
        tp.glen = (uint8_t)((TP2_PH + g - 1u) / g);
        tp.grp = 0xFF;
    }
    g = (q % TP2_PH) / tp.glen;
    if (g != tp.grp) {
        for (s = 0; s < NSLOT; s++) {
            uint32_t i = g * NSLOT + s, id = i < n ? ids[i] : 0u;
            if (tp.cur[s] < SP_COUNT)
                sy_patch.p[tp.cur[s]] = SP[tp.cur[s]].def;   /* (the last knob back where it was) */
            tp.cur[s] = (uint16_t)(i < n ? id : 0xFFFFu);
            sy_patch.p[SP_M1_SRC + 3u * s] = (int16_t)(i < n ? (s < 3u ? SRC_LFO1 : s < 6u ? SRC_LFO2 : SRC_ENV2) : SRC_NONE);
            sy_patch.p[SP_M1_DST + 3u * s] = (int16_t)(i < n ? id : 0u);
            sy_patch.p[SP_M1_AMT + 3u * s] = (int16_t)(s & 1u ? -1000 : 1000);
            if (i < n)
                sy_patch.p[id] = (int16_t)((SP[id].lo + SP[id].hi) / 2);
        }
        tp.grp = (uint8_t)g;
    }
    LP_UNLOCK(fl);
    if ((uint8_t)st != tp.stp) {
        uint32_t m = tp_fx_ids(ph, 0, ids, 64u), i;
        for (i = 0; i < m; i++) {
            const sp_desc_t *d = &SP[ids[i]];
            int32_t span = d->hi - d->lo;
            sy_patch.p[ids[i]] = (int16_t)(span <= 24 ? d->lo + (int32_t)((st + i) % (uint32_t)(span + 1))
                                                      : (st + i) & 1u ? d->hi : d->lo);
        }
        tp.stp = (uint8_t)st;
    }
    sy_ui_touch();
}

/* ------------------------------------------------------------------------------------ the interface --- */
/* test k (0 .. 3): loaded and started over the next milliseconds (sy_tp_tick); 0 when it can't now (a project's
 * load, a save, a recording, another test's load under way) */
static int sy_tp_load(uint32_t k)
{
    if (k >= SY_TP_N || tp.st == TPS_LOAD || tp.st == TPS_WAIT || sy_fl.busy || sy_fl.prj_want >= 0 || lpr.st != LR_OFF)
        return 0;
    if (cht.run)
        sy_app_run(0);
    tp.k = (uint8_t)k;
    tp.st = TPS_LOAD;
    return 1;
}
static void sy_tp_stop(void)                     /* it stops playing (still loaded: auto-save off until a load) */
{
    if (tp.st == TPS_LOAD) {
        sy_hush_req = 0;
        tp.st = sy_tp_on ? TPS_DONE : TPS_NONE;
        return;
    }
    if (tp.st == TPS_WAIT || tp.st == TPS_RUN) {
        if (cht.run)
            sy_app_run(0);
        tp.t1 = fm1_ms;
        if (tp.st == TPS_WAIT)
            tp.t0 = tp.t1;
        tp.st = TPS_DONE;
    }
}
/* a project loaded (sy_app.c sy_flash_ms, after it): the test is over; the sound, the tempo and the looper's
 * settings as they were before it */
static SY_NOINLINE void sy_tp_end(void)
{
    sy_patch_t p;
    if (!sy_tp_on && tp.st == TPS_NONE)
        return;
    if (tp.st == TPS_LOAD)
        sy_hush_req = 0;
    if (tp.had) {
        if (tp_was_n > 3u && sp_unpack(&p, tp_was + 3, tp_was_n - 3u)) {
            int16_t sl = (int16_t)(tp_was[0] | tp_was[1] << 8);
            sy_patch_load(&p);
            sy_slot = sl >= 0 && sl < (int16_t)SS_NUSER && sy_uidx[sl].used ? sl : -1;
            sy_sound = tp_was[2] < SY_NFACT ? tp_was[2] : 0u;
        }
        ui.bpm = tp.bpm, ui.tsb = tp.tsb, ui.tsn = tp.tsn;
        sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn % 5u]);
        lpg.quant = tp.quant, lpg.cin = tp.cin, lpg.click = tp.click, lpg.launch = tp.launch;
        ui.edited = tp.edited;
        sy_ui_touch();
    }
    tp.had = 0;
    tp.st = TPS_NONE;
    sy_tp_on = 0;
}
/* every millisecond (sy_app.c sy_app_ms): a load under way (stopped, hushed, built, the sound in, then PLAY), the
 * changes as the transport passes them, the end */
static SY_NOINLINE void sy_tp_tick(void)
{
    const tp_proj_t *p = &TP_PROJ[tp.k % SY_TP_N];
    switch (tp.st) {
    case TPS_LOAD:                               /* (as PROJECT's LOAD: stopped, nothing recording, hushed) */
        if (sy_fl.prj_want >= 0) {
            tp.st = sy_tp_on ? TPS_DONE : TPS_NONE;
            return;
        }
        if (cht.run || lpr.st != LR_OFF || sy_fl.busy || sy_next_st)
            return;
        sy_hush_req = 1;
        if (!sy_hushed)
            return;
        tp_build(tp.k);
        sy_hush_req = 0;
        tp.st = TPS_WAIT;
        tp.ran = 0;
        tp.t0 = fm1_ms;
        return;
    case TPS_WAIT:                               /* (the sound swapped, its tables built, a moment: PLAY) */
        if (sy_next_st || wt_build_o != 0xFF || wt_tab[0].busy || wt_tab[1].busy || fm1_ms - tp.t0 < 60u)
            return;
        if (tp.k == 1u)                          /* (TEST 2's first chain into the sound playing: the swap took the */
            tp_fx(0);                            /* sound whole, what the main loop wrote meanwhile with it) */
        sy_app_run(1);
        tp.t0 = fm1_ms;
        tp.st = TPS_RUN;
        return;
    case TPS_RUN: {
        uint32_t q;
        if (cht.run)
            tp.ran = 1;
        else if (tp.ran || fm1_ms - tp.t0 > 500u) {   /* (stopped: its END, or PLAY) */
            tp.t1 = fm1_ms;
            tp.st = TPS_DONE;
            return;
        }
        if (!cht.run)
            return;
        q = (uint32_t)(cht.tq * 4.0f);
        while (tp.pc < p->nact && p->act[tp.pc].at <= q) {
            const tp_act_t *a = &p->act[tp.pc];
            if (a->op == TP_SOUND && sy_next_st)
                break;                           /* (a sound still swapping: next time) */
            tp_exec(a);
            tp.pc++;
        }
        if (tp.k == 1u && q < p->len)
            tp_fx(q);
        return;
    }
    default:
        return;
    }
}
