/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA parameters: one table defines every patch parameter (docs/ARCHITECTURE.md §12). A row: the id
 * (SP_*, its place in the table: append-only, presets store values in this order with a count), the name
 * (8 characters), the section, the range and default as 16-bit integers, the unit (how the UI shows it and the
 * DSP reads it), flags and a default MIDI CC (0: none). The DSP reads continuous parameters as a normalised
 * value 0..1 that follows the stored one with a 10 ms one-pole a block (sp_block): a knob turned fast or a CC
 * jump never steps the audio. Lists, counts and switches are read as stored, at a block start. The matrix adds
 * its offsets to the normalised value (an amount of 100 % moves a destination across its whole range). */

enum { SEC_OSCA, SEC_OSCB, SEC_SUB, SEC_NOISE, SEC_FILTER, SEC_ENV1, SEC_ENV2, SEC_LFO1, SEC_LFO2, SEC_VOICE,
       SEC_MACRO, SEC_MATRIX, SEC_DRIVE, SEC_CHORUS, SEC_DELAY, SEC_REVERB, SEC_OTT, SEC_EQ, SEC_COUNT };

enum {                           /* units: the display and the DSP's reading of a value */
    U_INT,                       /* an integer as it is */
    U_LIST,                      /* one of a list of names (sp_list) */
    U_ONOFF,
    U_PCT,                       /* 0..1000 = 0..100.0 % */
    U_BIPCT,                     /* -1000..1000 = -100.0..100.0 % */
    U_CENT, U_SEMI, U_OCT, U_DEG,
    U_HZ,                        /* filter cutoff: 0..1000 = 20 Hz .. 20 kHz, exponential */
    U_TIME,                      /* envelope and LFO times: 0..1000 = 1 ms .. 30 s, exponential (sp_time_s) */
    U_TIME0,                     /* the same, 0 = none (hold, LFO delay and rise, glide) */
    U_LFOHZ,                     /* 0..1000 = 0.01 .. 40 Hz, exponential */
    U_DB,                        /* volume: 0 = off, else (v - 600) / 10 dB: -60.0 .. +6.0 */
    U_TABLE,                     /* a factory wavetable (WT_NAMES) */
    U_SRC, U_DST,                /* a modulation source (SRC_*), a destination (a parameter id) */
    U_SYNC,                      /* a note value (NOTE_NAMES: 8 BAR .. 1/32), an LFO's or the delay's SYNC FIXED */
    U_MS,                        /* the delay's time: 0..1000 = 10 .. 750 ms, exponential */
    U_DBX,                       /* a level in 0.1 dB (the OTT's bands: -24.0 .. +24.0; the EQ's: -18.0 .. +18.0) */
    U_Q,                         /* a band's Q: 0..1000 = 0.25 .. 8, exponential (sp_q) */
    U_ORDER                      /* the effects' order: a Lehmer code 0..719 (sp_order), shown as the default places */
};

#define F_MOD 1u                 /* a modulation destination */
#define F_VOICE 2u               /* per voice (else global: effects, macros, the patch volume) */
#define F_SMOOTH 4u              /* continuous: smoothed a block (else read as stored) */
#define F_TABLE 8u               /* a change rebuilds a wavetable (main loop) */
#define FMV (F_MOD | F_VOICE | F_SMOOTH)
#define FMG (F_MOD | F_SMOOTH)

/* an oscillator's rows (OSC A, OSC B): ON, TABLE, POS, WARP, WARP amount, OCT, SEMI, FINE, PHASE, RAND (random
 * start phase), PHMODE (RETRIG / FREE), LEVEL, PAN, UNI (unison voices), DETUNE, BLEND, WIDTH, UNIMODE (LINEAR /
 * SUPER), SPREAD (unison position spread), ROUTE (FILTER / DIRECT) */
#define SP_OSC(X, O, S, on, tab, ccpos, ccw)                                                              \
    X(O##_ON, "ON", S, 0, 1, on, U_ONOFF, 0, 0)                                                           \
    X(O##_TABLE, "TABLE", S, 0, WT_NFACT - 1, tab, U_TABLE, F_TABLE, 0)                                   \
    X(O##_POS, "WT POS", S, 0, 1000, 0, U_PCT, FMV, ccpos)                                                \
    X(O##_WARP, "WARP", S, 0, 8, 0, U_LIST, 0, 0)                                                         \
    X(O##_WAMT, "AMOUNT", S, 0, 1000, 0, U_PCT, FMV, ccw)                                                 \
    X(O##_OCT, "OCT", S, -4, 4, 0, U_OCT, 0, 0)                                                           \
    X(O##_SEMI, "SEMI", S, -12, 12, 0, U_SEMI, FMV, 0)                                                    \
    X(O##_FINE, "FINE", S, -100, 100, 0, U_CENT, FMV, 0)                                                  \
    X(O##_PHASE, "PHASE", S, 0, 360, 0, U_DEG, 0, 0)                                                      \
    X(O##_RAND, "RAND", S, 0, 1000, 1000, U_PCT, 0, 0)                                                    \
    X(O##_PHMODE, "PH MODE", S, 0, 1, 0, U_LIST, 0, 0)                                                    \
    X(O##_LEVEL, "LEVEL", S, 0, 1000, 750, U_PCT, FMV, 0)                                                 \
    X(O##_PAN, "PAN", S, -1000, 1000, 0, U_BIPCT, FMV, 0)                                                 \
    X(O##_UNI, "UNISON", S, 1, 16, 1, U_INT, 0, 0)                                                        \
    X(O##_DETUNE, "DETUNE", S, 0, 1000, 250, U_PCT, FMV, 0)                                               \
    X(O##_BLEND, "BLEND", S, 0, 1000, 750, U_PCT, FMV, 0)                                                 \
    X(O##_WIDTH, "WIDTH", S, 0, 1000, 1000, U_PCT, FMV, 0)                                                \
    X(O##_UMODE, "UNI MODE", S, 0, 1, 0, U_LIST, 0, 0)                                                    \
    X(O##_SPREAD, "WT SPRD", S, 0, 1000, 0, U_PCT, FMV, 0)                                                \
    X(O##_ROUTE, "ROUTE", S, 0, 1, 0, U_LIST, 0, 0)

#define SP_ENV(X, E, S, a, d, s, r)                                                                       \
    X(E##_ATK, "ATTACK", S, 0, 1000, a, U_TIME, FMV, 0)                                                   \
    X(E##_HOLD, "HOLD", S, 0, 1000, 0, U_TIME0, FMV, 0)                                                   \
    X(E##_DEC, "DECAY", S, 0, 1000, d, U_TIME, FMV, 0)                                                    \
    X(E##_SUS, "SUSTAIN", S, 0, 1000, s, U_PCT, FMV, 0)                                                   \
    X(E##_REL, "RELEASE", S, 0, 1000, r, U_TIME, FMV, 0)                                                  \
    X(E##_ACRV, "A CURVE", S, -1000, 1000, 0, U_BIPCT, 0, 0)                                              \
    X(E##_DCRV, "D CURVE", S, -1000, 1000, -500, U_BIPCT, 0, 0)                                           \
    X(E##_RCRV, "R CURVE", S, -1000, 1000, -500, U_BIPCT, 0, 0)

#define SP_LFO(X, L, S, rate)                                                                             \
    X(L##_SHAPE, "SHAPE", S, 0, 12, 0, U_LIST, 0, 0)                                                      \
    X(L##_RATE, "RATE", S, 0, 1000, rate, U_LFOHZ, FMV, 0)                                                \
    X(L##_SYNC, "SYNC", S, 0, 1, 0, U_LIST, 0, 0)                                                         \
    X(L##_MODE, "MODE", S, 0, 2, 1, U_LIST, 0, 0)                                                         \
    X(L##_DELAY, "DELAY", S, 0, 1000, 0, U_TIME0, 0, 0)                                                   \
    X(L##_RISE, "RISE", S, 0, 1000, 0, U_TIME0, 0, 0)                                                     \
    X(L##_PHASE, "PHASE", S, 0, 360, 0, U_DEG, 0, 0)                                                      \
    X(L##_SMOOTH, "SMOOTH", S, 0, 1000, 0, U_PCT, 0, 0)

#define SP_SLOT(X, N)                                                                                     \
    X(M##N##_SRC, "SOURCE", SEC_MATRIX, 0, SRC_COUNT - 1, 0, U_SRC, 0, 0)                                 \
    X(M##N##_DST, "DEST", SEC_MATRIX, 0, 511, 0, U_DST, 0, 0)                                             \
    X(M##N##_AMT, "AMOUNT", SEC_MATRIX, -1000, 1000, 0, U_BIPCT, 0, 0)

/* the parameters, in id order (append-only) */
#define SY_PARAMS(X)                                                                                      \
    SP_OSC(X, A, SEC_OSCA, 1, 0, 70, 21)                                                                  \
    SP_OSC(X, B, SEC_OSCB, 0, 1, 20, 22)                                                                  \
    X(SUB_ON, "ON", SEC_SUB, 0, 1, 0, U_ONOFF, 0, 0)                                                      \
    X(SUB_SHAPE, "SHAPE", SEC_SUB, 0, 5, 0, U_LIST, 0, 0)                                                 \
    X(SUB_OCT, "OCT", SEC_SUB, -3, 0, -1, U_OCT, 0, 0)                                                    \
    X(SUB_LEVEL, "LEVEL", SEC_SUB, 0, 1000, 750, U_PCT, FMV, 0)                                           \
    X(SUB_PAN, "PAN", SEC_SUB, -1000, 1000, 0, U_BIPCT, FMV, 0)                                           \
    X(SUB_PW, "WIDTH", SEC_SUB, 0, 1000, 0, U_PCT, FMV, 0)                                                \
    X(SUB_ROUTE, "ROUTE", SEC_SUB, 0, 1, 0, U_LIST, 0, 0)                                                 \
    X(NZ_ON, "ON", SEC_NOISE, 0, 1, 0, U_ONOFF, 0, 0)                                                     \
    X(NZ_TYPE, "TYPE", SEC_NOISE, 0, 2, 0, U_LIST, 0, 0)                                                  \
    X(NZ_COLOR, "COLOR", SEC_NOISE, -1000, 1000, 0, U_BIPCT, FMV, 0)                                      \
    X(NZ_STEREO, "STEREO", SEC_NOISE, 0, 1000, 1000, U_PCT, 0, 0)                                         \
    X(NZ_LEVEL, "LEVEL", SEC_NOISE, 0, 1000, 500, U_PCT, FMV, 0)                                          \
    X(NZ_PAN, "PAN", SEC_NOISE, -1000, 1000, 0, U_BIPCT, FMV, 0)                                          \
    X(NZ_ROUTE, "ROUTE", SEC_NOISE, 0, 1, 0, U_LIST, 0, 0)                                                \
    X(F_ON, "ON", SEC_FILTER, 0, 1, 0, U_ONOFF, 0, 0)                                                     \
    X(F_TYPE, "TYPE", SEC_FILTER, 0, 9, 0, U_LIST, 0, 0)                                                  \
    X(F_CUT, "CUTOFF", SEC_FILTER, 0, 1000, 700, U_HZ, FMV, 74)                                           \
    X(F_RES, "RESO", SEC_FILTER, 0, 1000, 100, U_PCT, FMV, 71)                                            \
    X(F_DRIVE, "DRIVE", SEC_FILTER, 0, 1000, 0, U_PCT, FMV, 0)                                            \
    X(F_KEY, "KEYTRACK", SEC_FILTER, 0, 1000, 0, U_PCT, FMV, 0)                                           \
    X(F_VAR, "VAR", SEC_FILTER, 0, 1000, 0, U_PCT, FMV, 0)                                                \
    X(F_MIX, "MIX", SEC_FILTER, 0, 1000, 1000, U_PCT, FMV, 0)                                             \
    SP_ENV(X, E1, SEC_ENV1, 0, 670, 1000, 263)                                                            \
    SP_ENV(X, E2, SEC_ENV2, 0, 670, 0, 263)                                                               \
    SP_LFO(X, L1, SEC_LFO1, 500)                                                                          \
    SP_LFO(X, L2, SEC_LFO2, 400)                                                                          \
    X(V_MODE, "MODE", SEC_VOICE, 0, 2, 0, U_LIST, 0, 0)                                                   \
    X(V_POLY, "VOICES", SEC_VOICE, 1, 8, 8, U_INT, 0, 0)                                                  \
    X(V_GLIDE, "GLIDE", SEC_VOICE, 0, 1000, 0, U_TIME0, 0, 5)                                             \
    X(V_GLMODE, "GL MODE", SEC_VOICE, 0, 1, 0, U_LIST, 0, 0)                                              \
    X(V_BUP, "BEND UP", SEC_VOICE, 0, 24, 2, U_SEMI, 0, 0)                                                \
    X(V_BDN, "BEND DN", SEC_VOICE, 0, 24, 2, U_SEMI, 0, 0)                                                \
    X(V_VEL, "VEL AMP", SEC_VOICE, 0, 1000, 500, U_PCT, 0, 0)                                             \
    X(V_VOL, "VOLUME", SEC_VOICE, 0, 660, 600, U_DB, FMG, 7)                                              \
    X(MAC1, "MACRO 1", SEC_MACRO, 0, 1000, 0, U_PCT, FMG, 16)                                             \
    X(MAC2, "MACRO 2", SEC_MACRO, 0, 1000, 0, U_PCT, FMG, 17)                                             \
    X(MAC3, "MACRO 3", SEC_MACRO, 0, 1000, 0, U_PCT, FMG, 18)                                             \
    X(MAC4, "MACRO 4", SEC_MACRO, 0, 1000, 0, U_PCT, FMG, 19)                                             \
    SP_SLOT(X, 1) SP_SLOT(X, 2) SP_SLOT(X, 3) SP_SLOT(X, 4)                                               \
    SP_SLOT(X, 5) SP_SLOT(X, 6) SP_SLOT(X, 7) SP_SLOT(X, 8)                                               \
    X(DR_ON, "ON", SEC_DRIVE, 0, 1, 0, U_ONOFF, 0, 0)                                                     \
    X(DR_TYPE, "TYPE", SEC_DRIVE, 0, 3, 0, U_LIST, 0, 0)                                                  \
    X(DR_AMT, "DRIVE", SEC_DRIVE, 0, 1000, 300, U_PCT, FMG, 0)                                            \
    X(DR_TONE, "TONE", SEC_DRIVE, -1000, 1000, 0, U_BIPCT, FMG, 0)                                        \
    X(DR_MIX, "MIX", SEC_DRIVE, 0, 1000, 1000, U_PCT, FMG, 0)                                             \
    X(CH_ON, "ON", SEC_CHORUS, 0, 1, 0, U_ONOFF, 0, 0)                                                    \
    X(CH_RATE, "RATE", SEC_CHORUS, 0, 1000, 472, U_LFOHZ, FMG, 0)                                       \
    X(CH_DEPTH, "DEPTH", SEC_CHORUS, 0, 1000, 500, U_PCT, FMG, 0)                                         \
    X(CH_DELAY, "DELAY", SEC_CHORUS, 0, 1000, 400, U_PCT, FMG, 0)                                         \
    X(CH_FDBK, "FEEDBACK", SEC_CHORUS, 0, 1000, 0, U_PCT, FMG, 0)                                         \
    X(CH_MIX, "MIX", SEC_CHORUS, 0, 1000, 500, U_PCT, FMG, 93)                                            \
    X(DL_ON, "ON", SEC_DELAY, 0, 1, 0, U_ONOFF, 0, 0)                                                     \
    X(DL_MODE, "MODE", SEC_DELAY, 0, 1, 1, U_LIST, 0, 0)                                                  \
    X(DL_TIME, "TIME", SEC_DELAY, 0, 1000, 600, U_MS, FMG, 0)                                             \
    X(DL_SYNC, "SYNC", SEC_DELAY, 0, 1, 1, U_LIST, 0, 0)                                                  \
    X(DL_FDBK, "FEEDBACK", SEC_DELAY, 0, 1000, 400, U_PCT, FMG, 0)                                        \
    X(DL_LP, "LP", SEC_DELAY, 0, 1000, 800, U_HZ, FMG, 0)                                                 \
    X(DL_HP, "HP", SEC_DELAY, 0, 1000, 150, U_HZ, FMG, 0)                                                 \
    X(DL_MIX, "MIX", SEC_DELAY, 0, 1000, 300, U_PCT, FMG, 94)                                             \
    X(RV_ON, "ON", SEC_REVERB, 0, 1, 0, U_ONOFF, 0, 0)                                                    \
    X(RV_TYPE, "TYPE", SEC_REVERB, 0, 1, 1, U_LIST, 0, 0)                                                 \
    X(RV_SIZE, "SIZE", SEC_REVERB, 0, 1000, 600, U_PCT, FMG, 0)                                           \
    X(RV_DECAY, "DECAY", SEC_REVERB, 0, 1000, 500, U_PCT, FMG, 0)                                         \
    X(RV_DAMP, "DAMP", SEC_REVERB, 0, 1000, 400, U_PCT, FMG, 0)                                           \
    X(RV_PRE, "PREDELAY", SEC_REVERB, 0, 1000, 100, U_PCT, 0, 0)                                          \
    X(RV_WIDTH, "WIDTH", SEC_REVERB, 0, 1000, 1000, U_PCT, FMG, 0)                                        \
    X(RV_LOCUT, "LOW CUT", SEC_REVERB, 0, 1000, 200, U_HZ, FMG, 0)                                        \
    X(RV_MIX, "MIX", SEC_REVERB, 0, 1000, 250, U_PCT, FMG, 91)                                            \
    X(OT_ON, "ON", SEC_OTT, 0, 1, 0, U_ONOFF, 0, 0)                                                       \
    X(OT_DEPTH, "DEPTH", SEC_OTT, 0, 1000, 1000, U_PCT, FMG, 0)                                           \
    X(OT_TIME, "TIME", SEC_OTT, 0, 1000, 500, U_PCT, FMG, 0)                                              \
    X(OT_UP, "UPWARD", SEC_OTT, 0, 1000, 1000, U_PCT, FMG, 0)                                             \
    X(OT_DOWN, "DOWNWARD", SEC_OTT, 0, 1000, 1000, U_PCT, FMG, 0)                                         \
    X(OT_LOW, "LOW", SEC_OTT, -240, 240, 0, U_DBX, FMG, 0)                                                \
    X(OT_MID, "MID", SEC_OTT, -240, 240, 0, U_DBX, FMG, 0)                                                \
    X(OT_HIGH, "HIGH", SEC_OTT, -240, 240, 0, U_DBX, FMG, 0)                                              \
    X(L1_NOTE, "NOTE", SEC_LFO1, 0, 16, 8, U_SYNC, 0, 0)                                                  \
    X(L2_NOTE, "NOTE", SEC_LFO2, 0, 16, 8, U_SYNC, 0, 0)                                                  \
    X(DL_NOTE, "NOTE", SEC_DELAY, 0, 16, 10, U_SYNC, 0, 0)                                                \
    X(EQ_ON, "ON", SEC_EQ, 0, 1, 0, U_ONOFF, 0, 0)                                                        \
    X(EQ_LOW, "LOW", SEC_EQ, -180, 180, 0, U_DBX, FMG, 0)                                                 \
    X(EQ_MID, "MID", SEC_EQ, -180, 180, 0, U_DBX, FMG, 0)                                                 \
    X(EQ_HIGH, "HIGH", SEC_EQ, -180, 180, 0, U_DBX, FMG, 0)                                               \
    X(EQ_LFREQ, "LO FREQ", SEC_EQ, 0, 1000, 292, U_HZ, FMG, 0)                                            \
    X(EQ_MFREQ, "MID FREQ", SEC_EQ, 0, 1000, 567, U_HZ, FMG, 0)                                           \
    X(EQ_Q, "MID Q", SEC_EQ, 0, 1000, 400, U_Q, FMG, 0)                                                   \
    X(EQ_HFREQ, "HI FREQ", SEC_EQ, 0, 1000, 767, U_HZ, FMG, 0)                                            \
    X(FX_ORDER, "FX ORDER", SEC_DRIVE, 0, 719, 0, U_ORDER, 0, 0)

/* modulation sources: per voice, then global */
enum { SRC_NONE, SRC_ENV1, SRC_ENV2, SRC_LFO1, SRC_LFO2, SRC_VEL, SRC_NOTE, SRC_RAND, SRC_ALT,
       SRC_MAC1, SRC_MAC2, SRC_MAC3, SRC_MAC4, SRC_WHEEL, SRC_BEND, SRC_AT, SRC_EXPR, SRC_FIXED, SRC_COUNT };
#define SRC_VOICE_LAST SRC_ALT    /* SRC_ENV1 .. SRC_ALT have a value per voice */

#define SP_ENUM(id, name, sec, lo, hi, def, unit, flags, cc) SP_##id,
enum { SY_PARAMS(SP_ENUM) SP_COUNT };
#define NSLOT 8u
_Static_assert(SP_COUNT <= 512, "U_DST names a parameter in 9 bits");

typedef struct {
    char name[9];
    uint8_t sec, unit, flags, cc;
    int16_t lo, hi, def;
} sp_desc_t;
#define SP_ROW(id, nm, sc, l, h, d, u, f, c) {nm, sc, u, f, c, l, h, d},
static const sp_desc_t SP[SP_COUNT] = {SY_PARAMS(SP_ROW)};

/* an LFO's drawn shape (SHAPE DRAWN): points over a cycle, the first at 0, the last at 1000, each at or after the
 * one before (two at one place: a step); between two, a curve: the value goes from the one's y to the next's as
 * u / (u + (1 - u) 2^(4 c)) of the way at u of the segment (c 0: straight; > 0 slow then quick, as an envelope's
 * curves). sy_lfo.c reads it (lfo_draw_sync) */
#define SP_NPT 16u
typedef struct {
    uint8_t n;                        /* points, 2 .. SP_NPT */
    int16_t x[SP_NPT];                /* 0 .. 1000: the place in the cycle */
    int16_t y[SP_NPT];                /* -1000 .. 1000 */
    int16_t c[SP_NPT];                /* -1000 .. 1000: the curve from this point to the next */
} sp_draw_t;

/* the patch: what presets store and the UI and MIDI write */
#define SP_NPAIR (SP_COUNT / 2u)
typedef struct {
    union {
        int16_t p[SP_COUNT];
        uint32_t pw[SP_NPAIR];        /* (the values two at a time: sp_block looks for changes a word at a time) */
    };
    char name[17];
    uint8_t cat;                      /* CAT_* (the preset browser) */
    char macro[4][9];                 /* the macros' names */
    sp_draw_t draw[2];                /* the LFOs' drawn shapes (written between sy_draw_edit and sy_draw_done) */
} sy_patch_t;
enum { CAT_BASS, CAT_LEAD, CAT_PAD, CAT_PLUCK, CAT_KEYS, CAT_FX, CAT_SEQ, CAT_OTHER, CAT_COUNT };

static sy_patch_t sy_patch;          /* the sound playing (the edit buffer) */
static float sp_n[SP_COUNT];         /* audio side: each value normalised 0..1 (the continuous ones smoothed) */
static float sp_t[SP_COUNT];         /* .. where each is going: its stored value normalised */
static union {
    int16_t v[SP_COUNT];
    uint32_t w[SP_NPAIR];
} sp_last;                           /* .. the stored values sp_t is of */
static uint8_t sp_mv[SP_COUNT], sp_mvf[SP_COUNT], sp_nmv;   /* the smoothed values still on their way */
#define SP_SMOOTH_K 0.0700f          /* a block's step of the one-pole: tau 10 ms (1 - e^(-32 / (0.010 fs))) */

static inline float sp_norm_of(uint32_t id, int32_t v)
{
    const sp_desc_t *d = &SP[id];
    return (float)(v - d->lo) / (float)(d->hi - d->lo);
}

/* the LFOs' built-in shapes (sy_lfo.c's LFO_*, in its order) as points: SHAPE DRAWN starts from the shape shown when
 * its points are first edited. A count, then x, y, curve a point. All but SINE and BOUNCE are exact; their curves
 * (294: no one curve comes closer to a quarter sine) within 2.6 % of the LFO's range on SINE, 5.3 % on BOUNCE's
 * first bounce. The random shapes (S&H, SMOOTH) are five cycles of sample levels: drawn, five steps or glides a
 * cycle */
static const int16_t SP_SHAPE_PTS[] = {
    5, 0, 0, -294, 250, 1000, 294, 500, 0, -294, 750, -1000, 294, 1000, 0, 0,                        /* SINE */
    4, 0, 0, 0, 250, 1000, 0, 750, -1000, 0, 1000, 0, 0,                                              /* TRIANGLE */
    2, 0, -1000, 0, 1000, 1000, 0,                                                                    /* SAW UP */
    2, 0, 1000, 0, 1000, -1000, 0,                                                                    /* SAW DOWN */
    4, 0, 1000, 0, 500, 1000, 0, 500, -1000, 0, 1000, -1000, 0,                                       /* SQUARE */
    10, 0, 600, 0, 200, 600, 0, 200, -350, 0, 400, -350, 0, 400, 900, 0, 600, 900, 0, 600, -800, 0,    /* S&H */
    800, -800, 0, 800, 250, 0, 1000, 250, 0,
    11, 0, 600, 294, 100, 125, -294, 200, -350, 294, 300, 275, -294, 400, 900, 294, 500, 50, -294,    /* SMOOTH */
    600, -800, 294, 700, -275, -294, 800, 250, 294, 900, -125, -294, 1000, -500, 0,
    8, 0, 1000, 0, 250, 1000, 0, 250, 333, 0, 500, 333, 0, 500, -333, 0, 750, -333, 0, 750, -1000, 0,   /* STEP 4 */
    1000, -1000, 0,
    16, 0, -1000, 0, 125, -1000, 0, 125, -714, 0, 250, -714, 0, 250, -429, 0, 375, -429, 0,            /* STAIRS */
    375, -143, 0, 500, -143, 0, 500, 143, 0, 625, 143, 0, 625, 429, 0, 750, 429, 0, 750, 714, 0,
    875, 714, 0, 875, 1000, 0, 1000, 1000, 0,
    4, 0, 1000, 0, 250, 1000, 0, 250, -1000, 0, 1000, -1000, 0,                                       /* PULSE */
    9, 0, -1000, -294, 125, 1000, 294, 250, -1000, -294, 375, 560, 294, 500, -1000, -294,             /* BOUNCE */
    625, 120, 294, 750, -1000, -294, 875, -320, 294, 1000, -1000, 0,
    5, 0, -1000, 0, 125, 1000, 0, 500, 1000, 0, 625, -1000, 0, 1000, -1000, 0,                        /* TRAPEZ */
};
#define SP_NSHAPE_PTS 12u                /* (the shapes above: every one but DRAWN) */

/* a drawn shape made whole: 2 .. SP_NPT points, the first at 0 and the last at 1000, each at or after the one
 * before, every value in its range */
static void sp_draw_fix(sp_draw_t *d)
{
    uint32_t i, n = d->n < 2u ? 2u : d->n > SP_NPT ? SP_NPT : d->n;
    d->n = (uint8_t)n;
    for (i = 0; i < n; i++) {
        d->x[i] = (int16_t)(i == 0u ? 0 : i + 1u == n ? 1000 : sy_clampi(d->x[i], d->x[i - 1u], 1000));
        d->y[i] = (int16_t)sy_clampi(d->y[i], -1000, 1000);
        d->c[i] = (int16_t)sy_clampi(d->c[i], -1000, 1000);
    }
}

/* built-in shape s as points (s past them: the sine) */
static void sp_shape_pts(uint32_t s, sp_draw_t *d)
{
    const int16_t *q = SP_SHAPE_PTS;
    uint32_t i;
    if (s >= SP_NSHAPE_PTS)
        s = 0;
    while (s--)
        q += 1 + 3 * q[0];
    memset(d, 0, sizeof *d);
    d->n = (uint8_t)q[0];
    for (i = 0; i < d->n; i++) {
        d->x[i] = q[1u + 3u * i];
        d->y[i] = q[2u + 3u * i];
        d->c[i] = q[3u + 3u * i];
    }
}

static void sp_defaults(sy_patch_t *pt)
{
    uint32_t i;
    for (i = 0; i < SP_COUNT; i++)
        pt->p[i] = SP[i].def;
    memcpy(pt->name, "INIT", 5);
    pt->cat = CAT_OTHER;
    for (i = 0; i < 4u; i++) {
        memcpy(pt->macro[i], "MACRO 1", 8);
        pt->macro[i][6] = (char)('1' + i);
    }
    sp_shape_pts(0, &pt->draw[0]);
    sp_shape_pts(0, &pt->draw[1]);
}

/* a value into its range (the matrix's destinations: modulatable parameters only) */
static int16_t sp_clamp(uint32_t id, int32_t v)
{
    if (id >= SP_COUNT)
        return 0;
    if ((SP[id].unit == U_DST) && (v < 0 || v >= SP_COUNT || !(SP[v].flags & F_MOD)))
        return 0;
    return (int16_t)sy_clampi(v, SP[id].lo, SP[id].hi);
}

/* every value to its normalised one at once (a patch load: no glide from the old sound) */
static void sp_snap(void)
{
    uint32_t i;
    for (i = 0; i < SP_COUNT; i++) {
        sp_last.v[i] = sy_patch.p[i];
        sp_n[i] = sp_t[i] = sp_norm_of(i, sp_last.v[i]);
        sp_mvf[i] = 0;
    }
    sp_nmv = 0;
}

/* once a block: the values follow the stored ones (whoever wrote them: the UI, MIDI, a test). A changed value is
 * normalised once; a continuous one then glides (a step of a 10 ms one-pole a block, exactly at it when under
 * 1e-5), the others (lists, switches and the values read as stored) take it at once */
static void sp_changed(uint32_t i)
{
    sp_last.v[i] = sy_patch.p[i];
    sp_t[i] = sp_norm_of(i, sp_last.v[i]);
    if (!(SP[i].flags & F_SMOOTH))
        sp_n[i] = sp_t[i];
    else if (!sp_mvf[i])
        sp_mvf[i] = 1, sp_mv[sp_nmv++] = (uint8_t)i;
}

static void sp_block(void)
{
    uint32_t i, k;
    for (k = 0; k < SP_NPAIR; k++)
        if (sy_patch.pw[k] != sp_last.w[k])
            for (i = 2u * k; i < 2u * k + 2u; i++)
                if (sy_patch.p[i] != sp_last.v[i])
                    sp_changed(i);
    if ((SP_COUNT & 1u) && sy_patch.p[SP_COUNT - 1u] != sp_last.v[SP_COUNT - 1u])
        sp_changed(SP_COUNT - 1u);
    for (k = 0; k < sp_nmv;) {
        uint32_t id = sp_mv[k];
        float t = sp_t[id], d = t - sp_n[id];
        sp_n[id] = sy_absf(d) < 1e-5f ? t : sp_n[id] + d * SP_SMOOTH_K;
        if (sp_n[id] == t)
            sp_mvf[id] = 0, sp_mv[k] = sp_mv[--sp_nmv];
        else
            k++;
    }
}

/* ------------------------------------------------------------------------------- units for the DSP --- */
/* the normalised value as the unit's number (lists and integers: rounded) */
static inline float sp_lin(uint32_t id, float n) { return (float)SP[id].lo + n * (float)(SP[id].hi - SP[id].lo); }
static inline float sp_time_s(float n) { return 0.001f * sy_exp2f(n * 14.8727f); }        /* 1 ms .. 30 s */
static inline float sp_cut_hz(float n) { return 20.0f * sy_exp2f(n * 9.96578f); }         /* 20 Hz .. 20 kHz */
static inline float sp_lfo_hz(float n) { return 0.01f * sy_exp2f(n * 11.9658f); }         /* 0.01 .. 40 Hz */
static inline float sp_dly_s(float n) { return 0.010f * sy_exp2f(n * 6.22882f); }         /* 10 .. 750 ms */
static inline float sp_q(float n) { return 0.25f * sy_exp2f(n * 5.0f); }                  /* 0.25 .. 8 */
static inline float sp_db_gain(int32_t v) { return v <= 0 ? 0.0f : sy_exp2f((float)(v - 600) * 0.0166096f); }

/* note values (U_SYNC: an LFO's RATE, the delay's TIME with SYNC FIXED), longest first: quarter notes a cycle; the
 * bars (0 at the top four) follow the time signature (sy_lfo.c sy_note_q) */
#define SY_NNOTE 17u
static const float NOTE_Q[SY_NNOTE] = {0.0f, 0.0f, 0.0f, 0.0f, 3.0f, 2.0f, 4.0f / 3.0f, 1.5f, 1.0f, 2.0f / 3.0f,
                                       0.75f, 0.5f, 1.0f / 3.0f, 0.375f, 0.25f, 1.0f / 6.0f, 0.125f};
static const char *const NOTE_NAMES[SY_NNOTE] = {"8 BAR", "4 BAR", "2 BAR", "1 BAR", "1/2D", "1/2", "1/2T", "1/4D",
                                                 "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T",
                                                 "1/32"};

/* the effects' order (FX ORDER; the rack's first, DRIVE, keeps it in its section): the six, DRIVE CHORUS DELAY
 * REVERB OTT EQ (0..5), as a Lehmer code 0..719, so every value is an order (0: as listed). sp_order: the code's
 * effects, first to last; sp_order_code: back */
#define SP_NFX 6u
static const uint8_t SP_FACT[SP_NFX] = {120, 24, 6, 2, 1, 1};   /* (5! .. 0!) */
static void sp_order(int32_t code, uint8_t *o)
{
    uint8_t left[SP_NFX] = {0, 1, 2, 3, 4, 5};
    uint32_t i, j, c = (uint32_t)sy_clampi(code, 0, 719);
    for (i = 0; i < SP_NFX; i++) {
        uint32_t k = c / SP_FACT[i];
        c %= SP_FACT[i];
        o[i] = left[k];
        for (j = k; j + 1u < SP_NFX - i; j++)
            left[j] = left[j + 1u];
    }
}
static int16_t sp_order_code(const uint8_t *o)
{
    uint32_t i, j, c = 0;
    for (i = 0; i < SP_NFX; i++)
        for (j = i + 1u; j < SP_NFX; j++)
            c += (o[j] < o[i]) * SP_FACT[i];
    return (int16_t)c;
}

/* ------------------------------------------------------------------------------------------ presets --- */
/* a patch as bytes (presets, autosave): "SY", the version, the count of values, the values (int16, little endian,
 * in id order), the name (16), the category, the macros' names (4 x 8); then 'D' and the LFOs' drawn shapes (each
 * its count of points, then x, y, curve a point, int16). A patch written with fewer parameters (an older build)
 * takes the defaults of the ones it lacks, one without the drawn shapes the default's; every value is clamped
 * into its range, a drawn shape made whole (sp_draw_fix) */
#define SP_PACK_MAX (6u + 2u * SP_COUNT + 16u + 1u + 32u + 1u + 2u * (1u + 6u * SP_NPT))
static uint32_t sp_pack(const sy_patch_t *pt, uint8_t *o)
{
    uint32_t i, k, n = 0;
    o[n++] = 'S';
    o[n++] = 'Y';
    o[n++] = 1;
    o[n++] = 0;
    o[n++] = (uint8_t)SP_COUNT;
    o[n++] = (uint8_t)(SP_COUNT >> 8);
    for (i = 0; i < SP_COUNT; i++) {
        o[n++] = (uint8_t)pt->p[i];
        o[n++] = (uint8_t)((uint16_t)pt->p[i] >> 8);
    }
    for (i = 0; i < 16u; i++)
        o[n++] = (uint8_t)pt->name[i];
    o[n++] = pt->cat;
    for (i = 0; i < 32u; i++)
        o[n++] = (uint8_t)pt->macro[i / 8u][i % 8u];
    o[n++] = 'D';
    for (k = 0; k < 2u; k++) {
        const sp_draw_t *d = &pt->draw[k];
        o[n++] = d->n;
        for (i = 0; i < d->n; i++) {
            const int16_t v[3] = {d->x[i], d->y[i], d->c[i]};
            uint32_t j;
            for (j = 0; j < 3u; j++) {
                o[n++] = (uint8_t)v[j];
                o[n++] = (uint8_t)((uint16_t)v[j] >> 8);
            }
        }
    }
    return n;
}

static int sp_unpack(sy_patch_t *pt, const uint8_t *b, uint32_t len)
{
    uint32_t i, k, np, n = 6;
    if (len < 6u || b[0] != 'S' || b[1] != 'Y' || b[2] != 1)
        return 0;
    np = (uint32_t)b[4] | (uint32_t)b[5] << 8;
    if (len < 6u + 2u * np + 49u)
        return 0;
    sp_defaults(pt);
    for (i = 0; i < np; i++, n += 2u)
        if (i < SP_COUNT)
            pt->p[i] = sp_clamp(i, (int16_t)(b[n] | b[n + 1u] << 8));
    for (i = 0; i < 16u; i++)
        pt->name[i] = (char)b[n++];
    pt->name[16] = 0;
    pt->cat = b[n] < CAT_COUNT ? b[n] : CAT_OTHER;
    n++;
    for (i = 0; i < 32u; i++)
        pt->macro[i / 8u][i % 8u] = (char)b[n++];
    for (i = 0; i < 4u; i++)
        pt->macro[i][8] = 0;
    if (n < len && b[n] == 'D')                     /* the drawn shapes (each whole, or the default's) */
        for (n++, k = 0; k < 2u && n < len; k++) {
            uint32_t m = b[n++];
            sp_draw_t *d = &pt->draw[k];
            if (m < 2u || m > SP_NPT || len - n < 6u * m)
                break;
            d->n = (uint8_t)m;
            for (i = 0; i < m; i++, n += 6u) {
                d->x[i] = (int16_t)(b[n] | b[n + 1u] << 8);
                d->y[i] = (int16_t)(b[n + 2u] | b[n + 3u] << 8);
                d->c[i] = (int16_t)(b[n + 4u] | b[n + 5u] << 8);
            }
            sp_draw_fix(d);
        }
    return 1;
}
