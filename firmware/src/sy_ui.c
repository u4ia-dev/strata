/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA's user interface on the FM-1, Phase 4 (docs/ARCHITECTURE.md §17; its look: the Strata Screen Design canvas).
 *   - Pages: a section's parameters four to a page, KNOB 1-4 left to right as on the panel, and what the display
 *     draws. A family button shows its sections' pages: a tap, the next page (EDIT: of the section ALGORITHM
 *     picked; the other families through all of theirs); ALGORITHM picks the family's section, SELECT steps its
 *     pages; OCT- / OCT+ with a family button (or REC) held, the section's pages, round (the octave stays, the
 *     button's release and hold do nothing). HOME: the sound playing, the macros, the output live; HOME held 5 s:
 *     the SETTINGS (COLOR: the palette; ACCEL: the knobs' feel; a countdown in the footer from HOLD). FX: DRIVE,
 *     CHORUS, DELAY, REVERB, OTT, EQ. GLO's GLOBAL page: BPM (20-300), TS-BEAT (1-16 beats a bar), TS-NOTE (the
 *     note a beat is: 1 2 4 8 16).
 *   - Knobs: speed-sensitive (a slow turn 0.1 % a detent, a quick one up to 5 %; ACCEL OFF: 1 % a detent); a list,
 *     a count or a short range one value a detent. An LFO's RATE and the delay's TIME with SYNC FIXED set a note
 *     value (the MOD layer: RATE and TIME themselves, whose modulation steps it). A knob turning lights its cell, and
 *     on the filter's and the effects' pages the part of the graph it moves, its value in a flag, the shape before
 *     the turn dashed behind, until the gesture ends (UI_GESTURE_MS without a turn).
 *   - SAVE (sy_ui_save.c): tapped once, the sound saved as a new preset at once (its next version, "NAME v2");
 *     held 5 s, over its own slot (a countdown in the footer from HOLD); tapped twice, the browser (SAVE there:
 *     NAME IT for the sound playing). SAVE + OCT- undoes, SAVE + OCT+ redoes: the patch's last 8 states (an edit
 *     gesture or a sound's load each).
 *   - An LFO's POINTS page: KNOB 1 picks a point (round), KNOB 2 moves it along the cycle (between its
 *     neighbours; the first and last stay at the ends), KNOB 3 up and down, KNOB 4 curves the segment from it (the
 *     last point: the one into it). A shape other than DRAWN shows its points; the first edit draws from them
 *     (SHAPE DRAWN). LFO tapped twice on the page: a point left of the one picked, halfway to the one before (the
 *     first point picked: right of it), on the curve. LFO held 5 s on the page: the point picked goes (a countdown
 *     in the footer from HOLD; let go, a knob or another button stops it; the first and last points stay).
 *   - The MOD layer: REC held (a double tap latches it, a tap lets it go). A white key picks the source, a knob
 *     sets that source's depth on its parameter: a matrix slot, made when needed and freed when the layer closes
 *     at 0. ENV held: the same for ENV 2 (an envelope onto a knob).
 *   - The screen in five regions (header, display, the four knob cells, footer), each drawn when it changes, one
 *     canvas at a time (sy_ui_draw.c); the LEDs (sy_ui_draw.c sy_ui_leds).
 * Still to come: the LFO, FX, GLO and SCL layers, MIDI learn, help, the rest of the SETTINGS, the matrix's
 * graph. Needs sy_app.c's keys and sounds (declared here), panel.c, gfx.c, sy_gfx.c, the engine and
 * sy_text.c. */

static int sy_app_held(uint32_t k);                 /* sy_app.c: key k's note sounding (its LED) */
static void sy_app_octave(int32_t d);               /* .. OCT- / OCT+ */
static int32_t sy_app_oct(void);
static void sy_app_sound_step(int32_t s);           /* .. PRESETS: the starter sounds */
static const char *sy_app_sound_bank(uint32_t *i, uint32_t *n);   /* .. the sound playing: its bank, number, count */
static int32_t sy_app_load(void);                   /* .. the audio's load in %, -1 unknown */
#define SY_NFACT (sizeof SOUNDS / sizeof SOUNDS[0] + 1u)   /* .. the factory's presets: the starter sounds, INIT */
#define SY_USER 0x100u                              /* .. a user slot's preset id: SY_USER + its slot */
static uint32_t sy_app_list(uint32_t bank, uint32_t cat, uint16_t *ids);   /* .. the presets of a bank, category */
static const char *sy_app_entry(uint32_t id, uint8_t *cat);                /* .. a preset's name, category */
static uint32_t sy_app_entry_now(void);                                     /* .. the one playing */
static void sy_app_entry_mark(uint32_t id);                                 /* .. undo, redo: the one a state was */
static void sy_app_entry_load(uint32_t id);
static int32_t sy_app_slot(void);                   /* .. the user slot the sound playing is from (-1 none) */
static int32_t sy_app_slot_free(void);
static const char *sy_app_slot_name(uint32_t s);
static int sy_app_save(uint32_t s, const char *name, uint32_t cat);   /* .. a SAVE (sy_ui_saved when done) */

/* ------------------------------------------------------------------------------------------- pages --- */
#define UI_ITEM 1000                        /* an item that is no parameter: the SETTINGS', an LFO's points, GLOBAL's */
enum { UI_COLOR = UI_ITEM, UI_ACCEL, UI_LPT, UI_LX, UI_LY, UI_LC, UI_BPM, UI_TSB, UI_TSN, UI_EQPT, UI_EQF, UI_EQG, UI_EQQ,
       UI_SCUR, UI_SCHR, UI_SCAT, UI_SSLOT, UI_BCAT, UI_BPRE, UI_BBANK,   /* (SAVE's, the browser's: sy_ui_save.c) */
       UI_MSLOT, UI_MSRC, UI_MDST, UI_MAMT,           /* (the matrix's: the slot picked, its source, place, amount) */
       UI_SCALE, UI_ROOT, UI_FIT, UI_KVEL };          /* (SCL's: the keys' scale, its root, how keys fit, velocity) */
#define SEC_MENU SEC_COUNT                          /* (sections of the UI's own: the SETTINGS, GLOBAL) */
#define SEC_GLOBAL (SEC_COUNT + 1)
#define SEC_SAVE (SEC_COUNT + 2)
#define SEC_BROWSE (SEC_COUNT + 3)
#define SEC_KEYS (SEC_COUNT + 4)                    /* (SCL: the keys' scale) */
#define SEC_N (SEC_COUNT + 5)                       /* every section, the UI's own too (ui.spage) */
enum { G_LIST, G_WT3D, G_WT2D, G_UNI, G_ENV, G_LFO, G_TEMPO, G_COLOR, G_HOME, G_FILTER, G_DRIVE, G_CHORUS, G_DELAY,
       G_REVERB, G_OTT, G_EQ, G_SAVE, G_BROWSE, G_MATRIX, G_KEYS };
static const uint8_t UI_TSN_VAL[5] = {1, 2, 4, 8, 16};   /* TS-NOTE: a whole note .. a sixteenth */
typedef struct {
    uint8_t sec, graph;                             /* SEC_*, G_*: what the display draws */
    char name[8];                                   /* the page's name in the header */
    int16_t id[4];                                  /* KNOB 1-4: parameters (or UI_* items), -1 none */
} ui_page_t;
#define UI_OSC(O, S)                                                                                      \
    {S, G_WT3D, "WAVE", {SP_##O##_POS, SP_##O##_TABLE, SP_##O##_WARP, SP_##O##_WAMT}},                     \
    {S, G_UNI, "UNISON", {SP_##O##_UNI, SP_##O##_DETUNE, SP_##O##_BLEND, SP_##O##_WIDTH}},                \
    {S, G_UNI, "SPREAD", {SP_##O##_UMODE, SP_##O##_SPREAD, SP_##O##_PHASE, SP_##O##_RAND}},              \
    {S, G_WT2D, "PITCH", {SP_##O##_OCT, SP_##O##_SEMI, SP_##O##_FINE, SP_##O##_PHMODE}},                  \
    {S, G_WT2D, "OUTPUT", {SP_##O##_LEVEL, SP_##O##_PAN, SP_##O##_ROUTE, SP_##O##_ON}},
#define UI_ENV(E, S)                                                                                      \
    {S, G_ENV, "ADSR", {SP_##E##_ATK, SP_##E##_DEC, SP_##E##_SUS, SP_##E##_REL}},                          \
    {S, G_ENV, "SHAPE", {SP_##E##_HOLD, SP_##E##_ACRV, SP_##E##_DCRV, SP_##E##_RCRV}},
#define UI_LFO(L, S)                                                                                      \
    {S, G_LFO, "POINTS", {UI_LPT, UI_LX, UI_LY, UI_LC}},                                                   \
    {S, G_LFO, "SHAPE", {SP_##L##_SHAPE, SP_##L##_SYNC, SP_##L##_RATE, SP_##L##_MODE}},                    \
    {S, G_LFO, "TIME", {SP_##L##_DELAY, SP_##L##_RISE, SP_##L##_PHASE, SP_##L##_SMOOTH}},
static const ui_page_t UI_PG[] = {
    UI_OSC(A, SEC_OSCA)
    UI_OSC(B, SEC_OSCB)
    {SEC_SUB, G_LIST, "MAIN", {SP_SUB_SHAPE, SP_SUB_OCT, SP_SUB_LEVEL, SP_SUB_PW}},
    {SEC_SUB, G_LIST, "OUTPUT", {SP_SUB_PAN, SP_SUB_ROUTE, SP_SUB_ON, -1}},
    {SEC_NOISE, G_LIST, "MAIN", {SP_NZ_TYPE, SP_NZ_COLOR, SP_NZ_STEREO, SP_NZ_LEVEL}},
    {SEC_NOISE, G_LIST, "OUTPUT", {SP_NZ_PAN, SP_NZ_ROUTE, SP_NZ_ON, -1}},
    {SEC_FILTER, G_FILTER, "TONE", {SP_F_TYPE, SP_F_CUT, SP_F_RES, SP_F_DRIVE}},
    {SEC_FILTER, G_FILTER, "MORE", {SP_F_KEY, SP_F_VAR, SP_F_MIX, SP_F_ON}},
    UI_ENV(E1, SEC_ENV1)
    UI_ENV(E2, SEC_ENV2)
    UI_LFO(L1, SEC_LFO1)
    UI_LFO(L2, SEC_LFO2)
    {SEC_VOICE, G_LIST, "VOICE", {SP_V_MODE, SP_V_POLY, SP_V_GLIDE, SP_V_GLMODE}},
    {SEC_VOICE, G_LIST, "PLAY", {SP_V_BUP, SP_V_BDN, SP_V_VEL, SP_V_VOL}},
    {SEC_GLOBAL, G_TEMPO, "TEMPO", {UI_BPM, UI_TSB, UI_TSN, -1}},
    {SEC_MACRO, G_HOME, "HOME", {SP_MAC1, SP_MAC2, SP_MAC3, SP_MAC4}},
    {SEC_MATRIX, G_MATRIX, "SLOTS", {UI_MSLOT, UI_MSRC, UI_MDST, UI_MAMT}},
    {SEC_DRIVE, G_DRIVE, "DRIVE", {SP_DR_TYPE, SP_DR_AMT, SP_DR_TONE, SP_DR_MIX}},
    {SEC_DRIVE, G_DRIVE, "POWER", {SP_DR_ON, -1, -1, -1}},
    {SEC_CHORUS, G_CHORUS, "CHORUS", {SP_CH_RATE, SP_CH_DEPTH, SP_CH_DELAY, SP_CH_MIX}},
    {SEC_CHORUS, G_CHORUS, "MORE", {SP_CH_FDBK, SP_CH_ON, -1, -1}},
    {SEC_DELAY, G_DELAY, "DELAY", {SP_DL_MODE, SP_DL_SYNC, SP_DL_TIME, SP_DL_FDBK}},
    {SEC_DELAY, G_DELAY, "TONE", {SP_DL_LP, SP_DL_HP, SP_DL_MIX, SP_DL_ON}},
    {SEC_REVERB, G_REVERB, "REVERB", {SP_RV_TYPE, SP_RV_SIZE, SP_RV_DECAY, SP_RV_MIX}},
    {SEC_REVERB, G_REVERB, "TONE", {SP_RV_DAMP, SP_RV_PRE, SP_RV_WIDTH, SP_RV_LOCUT}},
    {SEC_REVERB, G_REVERB, "POWER", {SP_RV_ON, -1, -1, -1}},
    {SEC_OTT, G_OTT, "OTT", {SP_OT_DEPTH, SP_OT_TIME, SP_OT_UP, SP_OT_DOWN}},
    {SEC_OTT, G_OTT, "BANDS", {SP_OT_LOW, SP_OT_MID, SP_OT_HIGH, SP_OT_ON}},
    {SEC_EQ, G_EQ, "POINTS", {UI_EQPT, UI_EQF, UI_EQG, UI_EQQ}},
    {SEC_EQ, G_EQ, "GAINS", {SP_EQ_LOW, SP_EQ_MID, SP_EQ_HIGH, SP_EQ_ON}},
    {SEC_MENU, G_COLOR, "SCREEN", {UI_COLOR, UI_ACCEL, -1, -1}},
    {SEC_KEYS, G_KEYS, "SCALE", {UI_SCALE, UI_ROOT, UI_FIT, UI_KVEL}},
    {SEC_SAVE, G_SAVE, "NAME IT", {UI_SCUR, UI_SCHR, UI_SCAT, UI_SSLOT}},
    {SEC_BROWSE, G_BROWSE, "BROWSE", {UI_BCAT, UI_BPRE, UI_BBANK, -1}},
};
#define UI_NPG (sizeof UI_PG / sizeof UI_PG[0])

/* the families: the sections each button shows (0xFF ends) */
enum { FAM_HOME, FAM_EDIT, FAM_ENV, FAM_LFO, FAM_SEQ, FAM_FX, FAM_GLO, FAM_MENU, FAM_SAVE, FAM_SCL, FAM_N };
static const uint8_t FAM_SECS[FAM_N][7] = {
    {SEC_MACRO, 0xFF}, {SEC_OSCA, SEC_OSCB, SEC_SUB, SEC_NOISE, SEC_FILTER, 0xFF}, {SEC_ENV1, SEC_ENV2, 0xFF},
    {SEC_LFO1, SEC_LFO2, 0xFF}, {SEC_MATRIX, 0xFF}, {SEC_DRIVE, SEC_CHORUS, SEC_DELAY, SEC_REVERB, SEC_OTT, SEC_EQ, 0xFF},
    {SEC_VOICE, SEC_GLOBAL, 0xFF}, {SEC_MENU, 0xFF}, {SEC_SAVE, SEC_BROWSE, 0xFF}, {SEC_KEYS, 0xFF},
};
#define UI_NFX 6u                                   /* the effects, SEC_DRIVE .. SEC_EQ */
static const uint8_t FAM_BTN[FAM_N] = {B_HOME, B_EDIT, B_ENV, B_LFO, B_SEQ, B_FX, B_GLO, NB, NB, B_SCL};

/* SCL: the scales (Felucca's, seq.c SCALE_MASK), each a bit a semitone from the root; how the keys fit them */
#define UI_NSCALE 16u
static const uint16_t UI_SCALE_MASK[UI_NSCALE] = {
    0xFFF, 0xAB5, 0x5AD, 0x6AD, 0x6B5, 0x295, 0x4A9, 0x9AD,   /* chromatic; major 0 2 4 5 7 9 11; minor 0 2 3 5 7 8 10; */
    0x5AB, 0xAD5, 0x56B, 0xAAD, 0x4E9, 0x555, 0x6DB, 0xB6D,   /* dorian, mixolydian, pentatonics, harmonic minor; */
};                                                            /* phrygian .. diminished as named below */
static const char *const UI_SCALE_NAME[UI_NSCALE] = {"CHROM", "MAJOR", "MINOR", "DORIAN", "MIXOLYD", "PENTA",
                                                     "M PENTA", "HARM MIN", "PHRYGIAN", "LYDIAN", "LOCRIAN",
                                                     "MEL MIN", "BLUES", "WHOLE", "DIM H-W", "DIM W-H"};
static const char *const UI_SCALE_LONG[UI_NSCALE] = {
    "CHROMATIC", "MAJOR", "NATURAL MINOR", "DORIAN", "MIXOLYDIAN", "MAJOR PENTATONIC", "MINOR PENTATONIC",
    "HARMONIC MINOR", "PHRYGIAN", "LYDIAN", "LOCRIAN", "MELODIC MINOR", "BLUES", "WHOLE TONE", "DIMINISHED H-W",
    "DIMINISHED W-H"};
static const char *const UI_NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
enum { UI_FIT_OFF, UI_FIT_SNAP, UI_FIT_WHITE, UI_NFIT };
static const char *const UI_FIT_NAME[UI_NFIT] = {"OFF", "SNAP", "WHITE"};
#define UI_KEY0 53                                  /* the lowest key, F3 (sy_app.c SY_KEY0) */

/* the MOD layer's white keys, F3 .. G5: the sources (black keys: none) */
static const uint8_t UI_KEY_SRC[27] = {
    SRC_ENV1, 0, SRC_ENV2, 0, SRC_LFO1, 0, SRC_LFO2, SRC_MAC1, 0, SRC_MAC2, 0, SRC_MAC3, SRC_MAC4, 0,
    SRC_VEL, 0, SRC_NOTE, 0, SRC_RAND, SRC_ALT, 0, SRC_WHEEL, 0, SRC_BEND, SRC_AT, 0, SRC_EXPR,
};
/* the footer's sources (the design's chips) */
static const uint8_t UI_CHIP_SRC[8] = {SRC_ENV1, SRC_ENV2, SRC_LFO1, SRC_LFO2, SRC_MAC1, SRC_MAC2, SRC_MAC3, SRC_MAC4};
static const char *const UI_CHIP_NAME[8] = {"E1", "E2", "L1", "L2", "M1", "M2", "M3", "M4"};

/* ------------------------------------------------------------------------------------------- state --- */
#define D_HEAD 1u
#define D_DISP 2u
#define D_KNOB(k) (4u << (k))
#define D_KNOBS 60u
#define D_FOOT 64u
#define D_ALL 127u
#define D_LIVE 128u                                 /* the display's live rows alone (HOME's scope) */
#define UI_DTAP_MS 300u                             /* a second tap within this: a double tap (it latches) */
static struct {
    uint16_t page;
    uint16_t fpage[FAM_N];                          /* the page a family returns to */
    uint16_t spage[SEC_N];                          /* the page a section returns to (ALGORITHM) */
    uint8_t fam;
    uint8_t dirty;                                  /* D_*: what to draw */
    uint8_t pal, accel;                             /* MENU: the palette, the knobs' feel */
    uint8_t mod;                                    /* the MOD layer: 0 off, 1 held, 2 latched */
    uint8_t mod_btn;                                /* the button that opened it (REC, ENV) */
    uint8_t mod_src;                                /* the source it sets */
    uint8_t mod_pick;                               /* REC's pick (kept across layers) */
    uint8_t mod_made;                               /* slots this layer made (freed at 0 when it closes) */
    int16_t mod_last;                               /* the parameter it set last (the banner), -1 none */
    uint32_t bt_t[NB], tap_t[NB];                   /* a button's press, its last tap */
    uint16_t bt_down, bt_held;                      /* buttons down; held past HOLD (their layer on) */
    uint16_t bt_chord;                              /* .. held while OCT stepped the pages (no tap, no layer) */
    uint16_t prev_page;                             /* the page before the last tap (a double tap puts it back) */
    uint8_t lpt[2];                                 /* the LFOs' points picked (POINTS) */
    uint8_t eqpt;                                   /* the EQ's point picked (POINTS): LOW, MID, HIGH */
    uint8_t mslot;                                  /* the matrix's slot picked (SEQ), 0 .. NSLOT - 1 */
    uint8_t scale, root, fit, kvel;                 /* SCL: the keys' scale, its root (0 C .. 11 B), how a key fits */
                                                    /* it (UI_FIT_*), the keys' velocity (1 .. 127) */
    uint8_t cd_btn, cd_s;                           /* a long hold counting down (NB: none); its seconds shown */
    uint8_t act_k;                                  /* the knob turning (its cell lit; 0xFF none) */
    uint32_t act_t;                                 /* .. its last detent */
    uint16_t bpm;                                   /* GLOBAL: BPM, TS-BEAT, TS-NOTE (UI_TSN_VAL's index) */
    uint8_t tsb, tsn;
    uint8_t h_first, h_cnt, h_at;                   /* the history (ui_hist): its oldest, its states, the patch's place */
    uint32_t edit_t;                                /* the last edit (a gesture: none for UI_GESTURE_MS before); 0 none */
    uint32_t en_t[NE];                              /* an encoder's last turn (the speed) */
    int8_t en_dir[NE];
    char toast[32];                                 /* a word in the footer, until toast_t */
    uint32_t toast_t;
    uint32_t live_t;                                /* a live graph's last frame */
    uint16_t sv_ret;                                /* SAVE, the browser: the page to go back to */
    char sv_name[17];                               /* SAVE: the name typed, its length, the cursor, the category */
    uint8_t sv_len, sv_cur, sv_cat, sv_slot;        /* .. the slot it goes to */
    uint8_t sv_sym, sv_over, saving;                /* .. symbols on the keys; over its own slot; writing */
    uint8_t sv_key;                                 /* .. the key typed last (lit a moment; 0xFF none) */
    uint32_t sv_key_t, sv_tap_t;                    /* .. when; SAVE tapped once: a new version (in the browser: */
                                                    /* NAME IT) at this time, no second tap by then (0 none) */
    uint8_t br_cat, br_bank;                        /* the browser: the category (0 all), the bank (all, factory, user) */
    uint16_t br_want;                               /* .. a preset to load (its id + 1; 0 none) */
} ui;

static void ui_toast(const char *s)
{
    uint32_t i;
    for (i = 0; s[i] && i + 1u < sizeof ui.toast; i++)
        ui.toast[i] = s[i];
    ui.toast[i] = 0;
    ui.toast_t = fm1_ms + 1500u;
    ui.dirty |= D_FOOT;
}

/* ------------------------------------------------------------------------------------------ helpers --- */
static uint32_t ui_sec(void) { return UI_PG[ui.page].sec; }
static const char *ui_sec_name(uint32_t s)
{
    return s < SEC_COUNT ? SEC_NAMES[s] : s == SEC_MENU ? "SETTINGS" : s == SEC_GLOBAL ? "GLOBAL" : s == SEC_SAVE ? "SAVE"
           : s == SEC_KEYS ? "KEYS" : "PRESETS";
}
static void ui_save_open(void);                     /* sy_ui_save.c */
static void ui_save_over(void);
static void ui_save_hold(void);
static void ui_save_write(void);
static void ui_save_del(void);
static int ui_save_key(uint32_t k);
static void ui_save_item_turn(int32_t item, int32_t s);
static void ui_browse_open(void);
static void ui_save_ms(void);

static int32_t ui_sec_power(uint32_t s)            /* a section's ON (its header light), -1 none */
{
    switch (s) {
    case SEC_OSCA: return SP_A_ON;
    case SEC_OSCB: return SP_B_ON;
    case SEC_SUB: return SP_SUB_ON;
    case SEC_NOISE: return SP_NZ_ON;
    case SEC_FILTER: return SP_F_ON;
    case SEC_DRIVE: return SP_DR_ON;
    case SEC_CHORUS: return SP_CH_ON;
    case SEC_DELAY: return SP_DL_ON;
    case SEC_REVERB: return SP_RV_ON;
    case SEC_OTT: return SP_OT_ON;
    case SEC_EQ: return SP_EQ_ON;
    }
    return -1;
}

static uint32_t ui_fam_of(uint32_t sec)
{
    uint32_t f, k;
    for (f = 0; f < FAM_N; f++)
        for (k = 0; FAM_SECS[f][k] != 0xFF; k++)
            if (FAM_SECS[f][k] == sec)
                return f;
    return FAM_EDIT;
}
static int ui_in_fam(uint32_t page, uint32_t f) { return ui_fam_of(UI_PG[page].sec) == f; }
static uint32_t ui_first_page(uint32_t sec)
{
    uint32_t p;
    for (p = 0; p < UI_NPG; p++)
        if (UI_PG[p].sec == sec)
            return p;
    return 0;
}
/* a page's number in its section and the section's pages: 1 / 5 */
static void ui_page_of(uint32_t page, uint32_t *n, uint32_t *of)
{
    uint32_t p, s = UI_PG[page].sec;
    *n = *of = 0;
    for (p = 0; p < UI_NPG; p++)
        if (UI_PG[p].sec == s) {
            (*of)++;
            if (p <= page)
                (*n)++;
        }
}

/* the EQ's POINTS page, KNOB 2-4: the point picked's FREQ, GAIN and Q (its parameters: the MOD layer sets their
 * depths, undo takes them back); the matrix's, KNOB 2-4: the slot picked's SOURCE, DEST, AMOUNT; any other id as it
 * is */
static int32_t ui_virt(int32_t id)
{
    static const uint8_t F[3] = {SP_EQ_LFREQ, SP_EQ_MFREQ, SP_EQ_HFREQ}, G[3] = {SP_EQ_LOW, SP_EQ_MID, SP_EQ_HIGH};
    static const uint8_t Q[3] = {SP_EQ_LQ, SP_EQ_Q, SP_EQ_HQ};
    uint32_t p = ui.eqpt < 3u ? ui.eqpt : 0u, m = 3u * (ui.mslot < NSLOT ? ui.mslot : 0u);
    if (id >= UI_MSRC && id <= UI_MAMT)
        return (int32_t)(SP_M1_SRC + m) + (id - UI_MSRC);
    return id == UI_EQF ? F[p] : id == UI_EQG ? G[p] : id == UI_EQQ ? Q[p] : id;
}

static int ui_list(uint32_t id)                     /* shown as a list (a pill), turned one entry a detent */
{
    uint32_t u = SP[id].unit;
    return u == U_LIST || u == U_TABLE || u == U_ONOFF || u == U_SYNC || u == U_SRC || u == U_DST;
}
static int ui_src_bip(uint32_t src)                 /* a source that swings both ways (its halo both sides) */
{
    return src == SRC_LFO1 || src == SRC_LFO2 || src == SRC_NOTE || src == SRC_RAND || src == SRC_ALT ||
           src == SRC_BEND;
}

/* the modulation on parameter id, as parts of its range: of the source pick (plo .. phi) and of all the others
 * (olo .. ohi); returns the slots on it */
static uint32_t ui_mod_span(uint32_t id, uint32_t pick, float *olo, float *ohi, float *plo, float *phi)
{
    uint32_t s, n = 0;
    *olo = *ohi = *plo = *phi = 0.0f;
    for (s = 0; s < NSLOT; s++) {
        int32_t src = sy_patch.p[SP_M1_SRC + 3u * s], dst = sy_patch.p[SP_M1_DST + 3u * s];
        float d = (float)sy_patch.p[SP_M1_AMT + 3u * s] * 0.001f, lo, hi;
        if (src <= SRC_NONE || src >= SRC_COUNT || dst != (int32_t)id || d == 0.0f)
            continue;
        lo = ui_src_bip((uint32_t)src) ? -sy_absf(d) : sy_minf(d, 0.0f);
        hi = ui_src_bip((uint32_t)src) ? sy_absf(d) : sy_maxf(d, 0.0f);
        if ((uint32_t)src == pick) {
            *plo += lo;
            *phi += hi;
        } else {
            *olo += lo;
            *ohi += hi;
        }
        n++;
    }
    return n;
}
/* source src moves something on the page shown */
static int ui_src_on_page(uint32_t src)
{
    uint32_t s, k;
    for (s = 0; s < NSLOT; s++)
        if (sy_patch.p[SP_M1_SRC + 3u * s] == (int32_t)src && sy_patch.p[SP_M1_AMT + 3u * s])
            for (k = 0; k < 4u; k++)
                if (UI_PG[ui.page].id[k] >= 0 && sy_patch.p[SP_M1_DST + 3u * s] == ui_virt(UI_PG[ui.page].id[k]))
                    return 1;
    return 0;
}
static int ui_src_used(uint32_t src)                /* .. anything */
{
    uint32_t s;
    for (s = 0; s < NSLOT; s++)
        if (sy_patch.p[SP_M1_SRC + 3u * s] == (int32_t)src && sy_patch.p[SP_M1_AMT + 3u * s])
            return 1;
    return 0;
}

/* ---------------------------------------------------------------------------------------- navigation --- */
static void ui_go(uint32_t page)
{
    uint32_t f = ui_fam_of(UI_PG[page].sec);
    ui.page = (uint16_t)page;
    ui.fam = (uint8_t)f;
    ui.fpage[f] = (uint16_t)page;
    if (UI_PG[page].sec < SEC_N)
        ui.spage[UI_PG[page].sec] = (uint16_t)page;
    ui.dirty = D_ALL;
}

/* family f's sections in the order they show (FX's: the rack's, FX ORDER), into sec; their count */
static uint32_t ui_fam_secs(uint32_t f, uint8_t *sec)
{
    uint32_t n;
    if (f == FAM_FX) {
        uint8_t o[SP_NFX];
        sp_order(sy_patch.p[SP_FX_ORDER], o);
        for (n = 0; n < SP_NFX; n++)
            sec[n] = (uint8_t)(SEC_DRIVE + o[n]);
        return n;
    }
    for (n = 0; FAM_SECS[f][n] != 0xFF; n++)
        sec[n] = FAM_SECS[f][n];
    return n;
}

/* s pages on, round: the family's (SELECT, a family button's tap: its sections in their order, each one's pages),
 * the section's (sec: EDIT's tap, OCT- / OCT+ with a family button held) */
static void ui_step_pages(int32_t s, int sec)
{
    uint16_t pg[UI_NPG];
    uint8_t secs[8];
    uint32_t n = 0, ns = 1, cur = 0, i, k;
    if (sec)
        secs[0] = (uint8_t)ui_sec();
    else
        ns = ui_fam_secs(ui.fam, secs);
    for (k = 0; k < ns; k++)
        for (i = 0; i < UI_NPG; i++)
            if (UI_PG[i].sec == secs[k]) {
                if (i == ui.page)
                    cur = n;
                pg[n++] = (uint16_t)i;
            }
    if (n)
        ui_go(pg[(uint32_t)(((int32_t)cur + s % (int32_t)n + (int32_t)n) % (int32_t)n)]);
}

/* a family button's tap: its page (another family), else the next of the section (EDIT) or of the family */
static void ui_family_tap(uint32_t f)
{
    uint8_t secs[8];
    if (ui.fam != f)
        ui_go(ui_in_fam(ui.fpage[f], f) ? ui.fpage[f] : (ui_fam_secs(f, secs), ui_first_page(secs[0])));
    else
        ui_step_pages(1, f == FAM_EDIT);
}

/* ALGORITHM: the family's sections in their order (each on the page it showed last) */
static void ui_step_section(int32_t s)
{
    uint8_t secs[8];
    uint32_t n = ui_fam_secs(ui.fam, secs), k = 0, i, sec = ui_sec(), next;
    for (i = 0; i < n; i++)
        if (secs[i] == sec)
            k = i;
    if (n < 2u)
        return;
    k = (uint32_t)(((int32_t)k + s % (int32_t)n + (int32_t)n) % (int32_t)n);
    next = secs[k];
    ui_go(UI_PG[ui.spage[next]].sec == next ? ui.spage[next] : ui_first_page(next));
}

/* FX held, ALGORITHM turned on an effect's page: that effect s places later in the rack (earlier: s < 0), as far as
 * its ends; FX's release is no tap after it (a chord) */
static void ui_edit(void);
static void ui_toast(const char *s);
static void ui_fx_move(int32_t s)
{
    uint8_t o[SP_NFX], e = (uint8_t)(ui_sec() - SEC_DRIVE), x;
    int32_t at = 0, to;
    char t[24];
    ui.bt_chord |= (uint16_t)(1u << B_FX);
    sp_order(sy_patch.p[SP_FX_ORDER], o);
    while (o[at] != e)
        at++;
    to = sy_clampi(at + s, 0, (int32_t)SP_NFX - 1);
    if (to == at) {
        ui_toast(at ? "LAST IN THE RACK" : "FIRST IN THE RACK");
        return;
    }
    ui_edit();
    for (; at != to; at += to > at ? 1 : -1) {      /* (past its neighbours, one at a time) */
        x = o[at + (to > at ? 1 : -1)];
        o[at + (to > at ? 1 : -1)] = o[at];
        o[at] = x;
    }
    sy_patch.p[SP_FX_ORDER] = sp_order_code(o);
    sp_put(sp_fix(sp_put(sp_put(t, SEC_NAMES[SEC_DRIVE + e]), ": "), to + 1, 0u, 0), " OF 6");
    ui_toast(t);
    ui.dirty |= D_DISP;
}

/* ------------------------------------------------------------------------------------------- knobs --- */
/* steps s of encoder e on a range of `range` values: speed-sensitive by the time between detents (a new turn,
 * and a slow one, one step of 0.1 %; quick ones up to 50); ACCEL OFF, 1 % a detent; a short range (lists,
 * counts, octaves) one value a detent */
static int32_t ui_accel(uint32_t e, int32_t s, int32_t range)
{
    uint32_t now = fm1_ms, dt = now - ui.en_t[e], per, m;
    int8_t dir = s > 0 ? 1 : -1;
    int32_t unit = range >= 1000 ? range / 1000 : 1;
    per = dt / (uint32_t)(s < 0 ? -s : s);
    if (dir != ui.en_dir[e] || dt > 250u)
        per = 1000u;                                /* (a turn starts slow) */
    ui.en_t[e] = now;
    ui.en_dir[e] = dir;
    if (range <= 48)
        return s;
    if (!ui.accel)
        return s * (range > 100 ? range / 100 : 1);
    m = per >= 50u ? 1u : per >= 30u ? 2u : per >= 20u ? 4u : per >= 12u ? 10u : per >= 7u ? 25u : 50u;
    return s * (int32_t)m * unit;
}

/* --------------------------------------------------------------------------------------- undo, redo --- */
/* the patch's history: SAVE + OCT- steps back, SAVE + OCT+ forward again. A state goes on it when an edit gesture
 * starts (nothing edited for UI_GESTURE_MS before: a knob, the MOD layer, an LFO's points) and before a sound
 * loads; full, the oldest goes. A state taken back is written as the knobs write (the engine fades what would
 * click), with the preset it was (a load undone: the sound is the one before again, and SAVE held writes over its
 * slot, not the one loaded). States 0 .. h_at - 1 are undo's, h_at + 1 .. h_cnt - 1 redo's (h_at the patch's own,
 * kept once undone) */
#define UI_NHIST 8u
#define UI_GESTURE_MS 1000u
static sy_patch_t ui_hist[UI_NHIST];                /* (4.7 KB) */
static uint16_t ui_hist_id[UI_NHIST];               /* .. the preset each was (sy_app_entry_now) */

static uint32_t ui_hist_ix(uint32_t j) { return (ui.h_first + j) % UI_NHIST; }
static sy_patch_t *ui_hist_at(uint32_t j) { return &ui_hist[ui_hist_ix(j)]; }
static void ui_hist_put(uint32_t j)                 /* the patch as it is, with the preset it is, into state j */
{
    *ui_hist_at(j) = sy_patch;
    ui_hist_id[ui_hist_ix(j)] = (uint16_t)sy_app_entry_now();
}
static void ui_hist_drop(void)                      /* (full: the oldest goes) */
{
    ui.h_first = (uint8_t)((ui.h_first + 1u) % UI_NHIST);
    ui.h_cnt--;
    ui.h_at--;
}
/* the patch as it is, on the history (any redo's go) */
static void ui_hist_push(void)
{
    ui.h_cnt = ui.h_at;
    if (ui.h_cnt == UI_NHIST)
        ui_hist_drop();
    ui_hist_put(ui.h_at);
    ui.h_cnt++;
    ui.h_at++;
}
static void ui_edit(void)                           /* an edit: the first of a gesture puts the patch on the history */
{
    if (!ui.edit_t || fm1_ms - ui.edit_t > UI_GESTURE_MS)
        ui_hist_push();
    ui.edit_t = fm1_ms | 1u;
}

static void ui_hist_take(const sy_patch_t *h)       /* a state into the patch, as the knobs write it */
{
    uint32_t i;
    for (i = 0; i < SP_COUNT; i++)
        sy_patch.p[i] = h->p[i];
    memcpy(sy_patch.name, h->name, sizeof sy_patch.name);
    sy_patch.cat = h->cat;
    memcpy(sy_patch.macro, h->macro, sizeof sy_patch.macro);
    sy_draw_edit();
    sy_patch.draw[0] = h->draw[0];
    sy_patch.draw[1] = h->draw[1];
    sy_draw_done();
    ui.dirty |= D_KNOBS | D_DISP | D_HEAD | D_FOOT;
}

/* SAVE + OCT-: back a state (the patch as it is kept, to come back to); SAVE + OCT+ (redo): forward one */
static void ui_undo(int redo)
{
    if (sy_next_st)
        return;                                     /* (a sound loading: the history has the one before) */
    if (!redo) {
        if (!ui.h_at) {
            ui_toast("NOTHING TO UNDO");
            return;
        }
        if (ui.h_at == ui.h_cnt) {
            if (ui.h_cnt == UI_NHIST)
                ui_hist_drop();
            ui_hist_put(ui.h_at);
            ui.h_cnt++;
        }
        ui.h_at--;
    } else {
        if (ui.h_at + 1u >= ui.h_cnt) {
            ui_toast("NOTHING TO REDO");
            return;
        }
        ui.h_at++;
    }
    ui_hist_take(ui_hist_at(ui.h_at));
    sy_app_entry_mark(ui_hist_id[ui_hist_ix(ui.h_at)]);
    ui.edit_t = 0;                                  /* (the next edit: a gesture of its own) */
    ui_toast(redo ? "REDO" : "UNDO");
}

/* ------------------------------------------------------------------------------------ LFO points --- */
#define UI_LB(k) ((k) ? (uint32_t)(SP_L2_SHAPE - SP_L1_SHAPE) : 0u)
static uint32_t ui_lfo(void) { return ui_sec() == SEC_LFO2; }   /* the LFO shown: 0, 1 */
static int ui_lfo_rnd(uint32_t shape) { return shape == LFO_SH || shape == LFO_SMOOTH; }

/* LFO k's points as shown: its drawn shape, or (SHAPE another) that shape's, into t */
static const sp_draw_t *ui_pts(uint32_t k, sp_draw_t *t)
{
    uint32_t shape = (uint32_t)sy_patch.p[SP_L1_SHAPE + UI_LB(k)];
    if (shape == LFO_DRAW)
        return &sy_patch.draw[k];
    sp_shape_pts(shape, t);
    return t;
}
static uint32_t ui_lpt(uint32_t k, const sp_draw_t *d) { return ui.lpt[k] < d->n ? ui.lpt[k] : d->n - 1u; }

/* an edit of LFO k's points: begun (SHAPE another: its points become the drawn shape), then done (SHAPE DRAWN,
 * once the audio side can take the points whole) */
static sp_draw_t *ui_pts_edit(uint32_t k)
{
    uint32_t shape = (uint32_t)sy_patch.p[SP_L1_SHAPE + UI_LB(k)];
    sy_draw_edit();
    if (shape != LFO_DRAW)
        sp_shape_pts(shape, &sy_patch.draw[k]);
    return &sy_patch.draw[k];
}
static void ui_pts_done(uint32_t k)
{
    int16_t *sh = &sy_patch.p[SP_L1_SHAPE + UI_LB(k)];
    sy_draw_done();
    if (*sh != LFO_DRAW) {
        char t[24];
        sp_put(sp_put(t, "DRAWN FROM "), LFO_NAMES[*sh < LFO_NSHAPE ? *sh : 0]);
        *sh = LFO_DRAW;
        ui_toast(t);
    }
    ui.dirty |= D_KNOBS | D_DISP;
}

static int16_t ui_curve_of(float k)                 /* a segment's 2^(4 c) as its curve c */
{
    return (int16_t)sy_clampi(sy_floori(sy_log2f(k) * 250.0f + 0.5f), -1000, 1000);
}

/* KNOB 1-4 on the POINTS page: the point picked, its place, its level, the curve from it (not while a sound
 * loads: it brings its own points) */
static void ui_lfo_turn(int32_t item, int32_t s, uint32_t e)
{
    uint32_t k = ui_lfo(), i, g;
    sp_draw_t t, *w;
    const sp_draw_t *d = ui_pts(k, &t);
    if (sy_next_st)
        return;
    i = ui_lpt(k, d);
    if (item == UI_LPT) {
        ui.lpt[k] = (uint8_t)(((int32_t)i + s % (int32_t)d->n + (int32_t)d->n) % (int32_t)d->n);
        ui.dirty |= D_KNOBS | D_DISP;
        return;
    }
    if (item == UI_LX && (i == 0u || i + 1u == d->n))
        return;                                     /* (the first and the last stay at the cycle's ends) */
    ui.lpt[k] = (uint8_t)i;
    ui_edit();
    w = ui_pts_edit(k);
    g = i + 1u < w->n ? i : i - 1u;                 /* the segment from the point (the last: the one into it) */
    if (item == UI_LX)
        w->x[i] = (int16_t)sy_clampi(w->x[i] + ui_accel(e, s, 1000), w->x[i - 1u], w->x[i + 1u]);
    else if (item == UI_LY)
        w->y[i] = (int16_t)sy_clampi(w->y[i] + ui_accel(e, s, 2000), -1000, 1000);
    else
        w->c[g] = (int16_t)sy_clampi(w->c[g] + ui_accel(e, s, 2000), -1000, 1000);
    ui_pts_done(k);
}

/* LFO tapped twice on the POINTS page: a point left of the one picked (the first: right of it), halfway to the
 * one before, on the curve between them, which it splits in two (the shape stays as it was); it is picked */
static void ui_lfo_add(void)
{
    uint32_t k = ui_lfo(), a, j;
    sp_draw_t t, *w;
    const sp_draw_t *d = ui_pts(k, &t);
    int32_t x0, x1, xm;
    float u, kc, dd;
    if (sy_next_st)
        return;
    if (d->n >= SP_NPT) {
        ui_toast("16 POINTS AT MOST");
        return;
    }
    a = ui_lpt(k, d);
    a = a ? a - 1u : 0u;                            /* the segment a .. a + 1 takes it */
    x0 = d->x[a];
    x1 = d->x[a + 1u];
    if (x1 - x0 < 2) {
        ui_toast("NO ROOM THERE");
        return;
    }
    xm = (x0 + x1) / 2;
    ui_edit();
    w = ui_pts_edit(k);
    u = (float)(xm - x0) / (float)(x1 - x0);
    kc = sy_exp2f(4.0f * (float)w->c[a] * 0.001f);
    dd = u + kc * (1.0f - u);                       /* the curve split at u: 2^(4 c) of kc / dd before, dd after */
    for (j = w->n; j > a + 1u; j--) {
        w->x[j] = w->x[j - 1u];
        w->y[j] = w->y[j - 1u];
        w->c[j] = w->c[j - 1u];
    }
    w->x[a + 1u] = (int16_t)xm;
    w->y[a + 1u] = (int16_t)sy_floori((float)w->y[a] + (float)(w->y[a + 2u] - w->y[a]) * u / dd + 0.5f);
    w->c[a + 1u] = ui_curve_of(dd);
    w->c[a] = ui_curve_of(kc / dd);
    w->n++;
    ui.lpt[k] = (uint8_t)(a + 1u);
    ui_pts_done(k);
}

/* a long hold, UI_LONG_MS: LFO on the POINTS page deletes the point picked (not the first or the last; the one before
 * it picked), HOME opens the SETTINGS, SAVE writes the sound over its own slot. From HOLD a countdown in the footer;
 * let go, a knob, another button, OCT (its pages, or SAVE's undo and redo: a chord), another page or the MOD layer
 * stop it */
#define UI_LONG_MS 5000u
static void ui_cd_arm(uint32_t b)
{
    ui.cd_btn = (uint8_t)b;
    ui.cd_s = 0;
}
static void ui_cd_stop(void)
{
    if (ui.cd_btn == NB)
        return;
    ui.cd_btn = NB;
    ui.toast[0] = 0;
    ui.dirty |= D_FOOT;
}
static void ui_lfo_del_arm(void)                    /* (LFO's hold, on POINTS) */
{
    uint32_t k = ui_lfo(), i;
    sp_draw_t t;
    const sp_draw_t *d = ui_pts(k, &t);
    i = ui_lpt(k, d);
    if (i == 0u || i + 1u >= d->n) {
        ui_toast("THE END POINTS STAY");
        return;
    }
    ui_cd_arm(B_LFO);
}
static void ui_lfo_del(void)
{
    uint32_t k = ui_lfo(), i, j;
    sp_draw_t t, *w;
    const sp_draw_t *d = ui_pts(k, &t);
    char s[32];
    i = ui_lpt(k, d);
    if (sy_next_st || i == 0u || i + 1u >= d->n)
        return;
    ui_edit();
    w = ui_pts_edit(k);
    for (j = i; j + 1u < w->n; j++) {               /* (the segment before it now runs to the next, its curve kept) */
        w->x[j] = w->x[j + 1u];
        w->y[j] = w->y[j + 1u];
        w->c[j] = w->c[j + 1u];
    }
    w->n--;
    ui.lpt[k] = (uint8_t)(i - 1u);
    ui_pts_done(k);
    sp_put(sp_fix(sp_put(s, "POINT "), (int32_t)i + 1, 0u, 0), " DELETED");
    ui_toast(s);
}
static void ui_cd_tick(void)                        /* every millisecond while it counts */
{
    uint32_t b = ui.cd_btn, held = fm1_ms - ui.bt_t[b], left;
    char s[32], *e;
    if (!(ui.bt_down & (1u << b)) || (ui.bt_chord & (1u << b)) || ui.mod ||
        (b == B_LFO && UI_PG[ui.page].id[0] != UI_LPT) || (b == B_SAVE && sy_app_slot() < 0)) {
        ui_cd_stop();
        return;
    }
    if (held >= UI_LONG_MS) {
        ui.cd_btn = NB;
        if (b == B_LFO) {
            ui_lfo_del();
        } else if (b == B_SAVE) {
            ui.toast[0] = 0;
            ui_save_over();
        } else {
            ui.toast[0] = 0;
            ui_go(ui_first_page(SEC_MENU));
        }
        return;
    }
    left = (UI_LONG_MS - held + 999u) / 1000u;
    if (left != ui.cd_s) {                          /* "HOLD: DELETE POINT 3 IN 4", "HOLD: SETTINGS IN 4", */
                                                    /* "HOLD: SAVE OVER USER 3 IN 4" */
        ui.cd_s = (uint8_t)left;
        e = sp_put(s, "HOLD: ");
        if (b == B_LFO) {
            uint32_t k = ui_lfo();
            sp_draw_t t;
            e = sp_fix(sp_put(e, "DELETE POINT "), (int32_t)ui_lpt(k, ui_pts(k, &t)) + 1, 0u, 0);
        } else if (b == B_SAVE) {
            e = sp_fix(sp_put(e, "SAVE OVER USER "), sy_app_slot() + 1, 0u, 0);
        } else {
            e = sp_put(e, "SETTINGS");
        }
        sp_fix(sp_put(e, " IN "), (int32_t)left, 0u, 0);
        ui_toast(s);
    }
}

/* the MOD layer: source pick's depth on parameter id, by s steps */
static void ui_mod_turn(uint32_t id, int32_t s, uint32_t e)
{
    uint32_t k;
    int32_t free = -1, slot = -1, amt;
    if (!(SP[id].flags & F_MOD))
        return;
    ui_edit();
    for (k = 0; k < NSLOT && slot < 0; k++) {
        int32_t src = sy_patch.p[SP_M1_SRC + 3u * k];
        if (src == (int32_t)ui.mod_src && sy_patch.p[SP_M1_DST + 3u * k] == (int32_t)id)
            slot = (int32_t)k;
        else if (free < 0 && src <= SRC_NONE)
            free = (int32_t)k;
    }
    if (slot < 0) {
        if (free < 0) {
            ui_toast("MATRIX FULL");
            return;
        }
        slot = free;
        sy_patch.p[SP_M1_AMT + 3u * (uint32_t)slot] = 0;
        sy_patch.p[SP_M1_DST + 3u * (uint32_t)slot] = (int16_t)id;
        sy_patch.p[SP_M1_SRC + 3u * (uint32_t)slot] = (int16_t)ui.mod_src;
        ui.mod_made |= (uint8_t)(1u << slot);
    }
    amt = sy_clampi(sy_patch.p[SP_M1_AMT + 3u * (uint32_t)slot] + ui_accel(e, s, 2000), -1000, 1000);
    sy_patch.p[SP_M1_AMT + 3u * (uint32_t)slot] = (int16_t)amt;
    ui.mod_last = (int16_t)id;
    ui.dirty |= D_HEAD | D_DISP | D_KNOBS | D_FOOT;
}

static void ui_mod_open(uint32_t btn, uint32_t src, int latched)
{
    ui.mod = latched ? 2u : 1u;
    ui.mod_btn = (uint8_t)btn;
    ui.mod_src = (uint8_t)src;
    ui.mod_last = -1;
    ui.mod_made = 0;
    ui.dirty = D_ALL;
}
static void ui_mod_close(void)
{
    uint32_t k;
    for (k = 0; k < NSLOT; k++)                     /* what it made and left at 0 goes */
        if ((ui.mod_made >> k) & 1u && !sy_patch.p[SP_M1_AMT + 3u * k]) {
            sy_patch.p[SP_M1_SRC + 3u * k] = SRC_NONE;
            sy_patch.p[SP_M1_DST + 3u * k] = 0;
        }
    ui.mod = 0;
    ui.dirty = D_ALL;
}

/* the items: the MENU's; an LFO's points and GLOBAL's tempo (not in the MOD layer: nothing there to modulate) */
static void ui_item_turn(int32_t item, int32_t s, uint32_t e)
{
    if (item == UI_COLOR) {
        ui.pal = (uint8_t)((ui.pal + (uint32_t)(s % (int32_t)SY_NPAL + (int32_t)SY_NPAL)) % SY_NPAL);
        sy_palette(ui.pal);
        ui.dirty = D_ALL;
    } else if (item == UI_ACCEL) {
        ui.accel = s > 0;
        ui.dirty |= D_KNOBS | D_DISP;
    } else if (item >= UI_SCUR && item <= UI_BBANK) {   /* SAVE's, the browser's */
        ui_save_item_turn(item, s);
    } else if (item == UI_EQPT) {                   /* the EQ's point: LOW, MID, HIGH, round (the MOD layer too: */
        ui.eqpt = (uint8_t)((ui.eqpt + (uint32_t)(s % 3 + 3)) % 3u);   /* whose depths KNOB 2-4 set) */
        ui.dirty |= D_KNOBS | D_DISP;
    } else if (item == UI_MSLOT) {                  /* the matrix's slot, round */
        ui.mslot = (uint8_t)((ui.mslot + (uint32_t)(s % (int32_t)NSLOT + (int32_t)NSLOT)) % NSLOT);
        ui.dirty |= D_KNOBS | D_DISP;
    } else if (ui.mod) {
        return;
    } else if (item >= UI_SCALE && item <= UI_KVEL) {   /* SCL: the scale, the root (round), the fit, the velocity */
        if (item == UI_SCALE)
            ui.scale = (uint8_t)((ui.scale + (uint32_t)(s % (int32_t)UI_NSCALE + (int32_t)UI_NSCALE)) % UI_NSCALE);
        else if (item == UI_ROOT)
            ui.root = (uint8_t)((ui.root + (uint32_t)(s % 12 + 12)) % 12u);
        else if (item == UI_FIT)
            ui.fit = (uint8_t)sy_clampi(ui.fit + s, 0, UI_NFIT - 1);
        else
            ui.kvel = (uint8_t)sy_clampi(ui.kvel + ui_accel(e, s, 126), 1, 127);
        ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
    } else if (item >= UI_BPM && item <= UI_TSN) {
        if (item == UI_BPM)
            ui.bpm = (uint16_t)sy_clampi(ui.bpm + ui_accel(e, s, 280), 20, 300);
        else if (item == UI_TSB)
            ui.tsb = (uint8_t)sy_clampi(ui.tsb + s, 1, 16);
        else
            ui.tsn = (uint8_t)sy_clampi(ui.tsn + s, 0, 4);
        sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
        ui.dirty |= D_KNOBS | D_DISP;
    } else {
        ui_lfo_turn(item, s, e);
    }
}

/* the parameter a knob sets: an LFO's RATE or the delay's TIME with SYNC FIXED, its note value (the MOD layer: RATE
 * and TIME themselves, whose modulation steps it) */
static int32_t ui_eff(int32_t id)
{
    if (ui.mod)
        return id;
    if (id == SP_L1_RATE && sy_patch.p[SP_L1_SYNC])
        return SP_L1_NOTE;
    if (id == SP_L2_RATE && sy_patch.p[SP_L2_SYNC])
        return SP_L2_NOTE;
    if (id == SP_DL_TIME && sy_patch.p[SP_DL_SYNC])
        return SP_DL_NOTE;
    return id;
}

/* the knob turning: its cell lit, and what it moves on the graph, until UI_GESTURE_MS pass without a detent
 * (sy_ui_draw.c sy_ui_frame puts them back); the parameter it sets (its note value with SYNC FIXED) or item, -1 none */
static void ui_act(uint32_t k)
{
    if (ui.act_k < 4u && ui.act_k != k)
        ui.dirty |= D_KNOB(ui.act_k);
    ui.act_k = (uint8_t)k;
    ui.act_t = fm1_ms;
    ui.dirty |= D_KNOB(k) | D_DISP;
}
static int32_t ui_act_id(void)
{
    int32_t id;
    if (ui.act_k >= 4u || ui.mod)
        return -1;
    id = ui_virt(UI_PG[ui.page].id[ui.act_k]);
    return id >= 0 && id < UI_ITEM ? ui_eff(id) : id;
}
/* the patch as it was before the turn (the gesture's first state on the history), while a knob turns; 0 none */
static const sy_patch_t *ui_ghost(void)
{
    if (ui.act_k >= 4u || ui.mod || !ui.edit_t || fm1_ms - ui.edit_t > UI_GESTURE_MS || !ui.h_at)
        return 0;
    return ui_hist_at(ui.h_at - 1u);
}

static void ui_knob(uint32_t k, int32_t s)
{
    int32_t id = ui_virt(UI_PG[ui.page].id[k]);     /* (the EQ's POINTS: the picked point's own) */
    const sp_desc_t *d;
    ui_cd_stop();
    if (id < 0)
        return;
    if (!ui.mod)
        ui_act(k);
    if (id >= UI_ITEM) {
        ui_item_turn(id, s, EN_K1 + k);
        return;
    }
    if (ui.mod) {
        ui_mod_turn((uint32_t)id, s, EN_K1 + k);
        return;
    }
    id = ui_eff(id);
    d = &SP[id];
    if (d->unit == U_DST) {                         /* a destination: the next parameter that can be modulated */
        int32_t v = sy_patch.p[id], dir = s > 0 ? 1 : -1, n = s > 0 ? s : -s;
        while (n--) {
            int32_t w = v;
            do
                w += dir;
            while (w > 0 && w < (int32_t)SP_COUNT && !(SP[w].flags & F_MOD));
            if (w < 0 || w >= (int32_t)SP_COUNT)
                break;
            v = w;
        }
        s = v - sy_patch.p[id];
    } else if (!ui_list((uint32_t)id)) {
        s = ui_accel(EN_K1 + k, s, d->hi - d->lo);
    }
    ui_edit();
    sy_patch.p[id] = sp_clamp((uint32_t)id, sy_patch.p[id] + s);
    ui.dirty |= D_KNOB(k) | D_DISP;
    if (id == SP_L1_SYNC || id == SP_L2_SYNC || id == SP_DL_SYNC)
        ui.dirty |= D_KNOBS;                        /* (RATE, TIME: Hz or ms, or a note value) */
    if (ui_sec_power(ui_sec()) == id || UI_PG[ui.page].graph == G_WT3D || UI_PG[ui.page].graph == G_WT2D ||
        ui_sec() == SEC_MATRIX)
        ui.dirty |= D_HEAD;                         /* (the light; OCT SEM FIN; the slots in use) */
}

/* a parameter changed from elsewhere (MIDI, a sound): what shows it */
static void sy_ui_touch(void) { ui.dirty |= D_KNOBS | D_DISP | D_HEAD | D_FOOT; }

/* ---------------------------------------------------------------------------------------- buttons --- */
static int ui_has_layer(uint32_t b) { return b == B_REC || b == B_ENV; }
#define UI_CHORD ((1u << B_HOME) | (1u << B_EDIT) | (1u << B_ENV) | (1u << B_LFO) | (1u << B_SEQ) | (1u << B_FX) | \
                  (1u << B_GLO) | (1u << B_REC))    /* held with OCT- / OCT+: the section's pages */

static void ui_press(uint32_t b)
{
    if (b != ui.cd_btn)
        ui_cd_stop();
    if ((b == B_OCTDN || b == B_OCTUP) && ui_sec() == SEC_SAVE) {   /* SAVE's screen: OCT+ the symbols, OCT- deletes */
        if (b == B_OCTUP) {
            ui.sv_sym ^= 1u;
            ui.dirty |= D_DISP | D_HEAD | D_FOOT;
        } else {
            ui_save_del();
        }
        return;
    }
    if (b == B_OCTDN || b == B_OCTUP) {
        char t[16] = "KEYS OCT ";
        uint16_t held = (uint16_t)(ui.bt_down & UI_CHORD);
        if (ui.bt_down & (1u << B_SAVE)) {          /* SAVE held: undo, redo */
            ui.bt_chord |= (uint16_t)(1u << B_SAVE);
            ui_undo(b == B_OCTUP);
            return;
        }
        if (held) {
            ui.bt_chord |= held;
            ui_step_pages(b == B_OCTUP ? 1 : -1, 1);
            return;
        }
        sy_app_octave(b == B_OCTUP ? 1 : -1);
        sp_fix(t + 9, sy_app_oct(), 0u, 1);
        ui_toast(t);
        ui.dirty |= D_HEAD;                         /* (SCL's header: the octave) */
    }
}

static void ui_hold(uint32_t b)                     /* held past HOLD: its layer (none on SAVE's or the browser's) */
{
    if (ui_sec() == SEC_SAVE || ui_sec() == SEC_BROWSE) {
        if (b == B_HOME)
            ui_cd_arm(B_HOME);
        return;
    }
    if (b == B_REC && !ui.mod)
        ui_mod_open(B_REC, ui.mod_pick, 0);
    else if (b == B_ENV && !ui.mod)
        ui_mod_open(B_ENV, SRC_ENV2, 0);
    else if (b == B_HOME && !ui.mod)
        ui_cd_arm(B_HOME);                          /* (5 s: the SETTINGS) */
    else if (b == B_LFO && UI_PG[ui.page].id[0] == UI_LPT && !ui.mod)
        ui_lfo_del_arm();
    else if (b == B_SAVE && !ui.mod)
        ui_save_hold();                             /* (5 s: over its own slot) */
    else if (b == B_SCL && !ui.mod)
        ui_toast("SCL HELD: A KEY IS THE ROOT");
}

static void ui_release(uint32_t b, int held)
{
    uint32_t f;
    if (b == ui.cd_btn)
        ui_cd_stop();                               /* (let go before the end: nothing happens) */
    if (held) {                                     /* a held layer closes (a latched one stays); SAVE held on a */
        if (ui.mod == 1u && ui.mod_btn == b)        /* sound from no user slot, no OCT meanwhile (undo, redo): */
            ui_mod_close();                         /* nothing to save over */
        if (b == B_SAVE && !(ui.bt_chord & (1u << B_SAVE)) && !ui.mod && sy_app_slot() < 0 &&
            ui_sec() != SEC_SAVE && ui_sec() != SEC_BROWSE)
            ui_toast("NOTHING TO SAVE OVER: TAP SAVE");
        return;
    }
    if (b == B_SAVE) {                              /* SAVE: on NAME IT, written; else once (no second tap within */
        ui.sv_tap_t = 0;                            /* UI_DTAP_MS) a new version, in the browser NAME IT; twice */
        if (ui.mod)                                 /* the browser, in it back */
            return;
        if (ui_sec() == SEC_SAVE)
            ui_save_write();
        else if (fm1_ms - ui.tap_t[b] >= UI_DTAP_MS)
            ui.sv_tap_t = (fm1_ms + UI_DTAP_MS) | 1u;
        else if (ui_sec() == SEC_BROWSE)
            ui_go(ui.sv_ret);
        else
            ui_browse_open();
        ui.tap_t[b] = ui.sv_tap_t ? fm1_ms : 0u;
        return;
    }
    if (b == B_HOME && (ui_sec() == SEC_SAVE || ui_sec() == SEC_BROWSE)) {
        ui.tap_t[b] = fm1_ms;                       /* HOME: NAME IT back to the browser, unsaved; the browser back */
        ui_go(ui_sec() == SEC_SAVE ? ui_first_page(SEC_BROWSE) : ui.sv_ret);
        return;
    }
    if (ui_has_layer(b) && fm1_ms - ui.tap_t[b] < UI_DTAP_MS && !ui.mod) {   /* a double tap: latched */
        if (b == B_ENV)
            ui_go(ui.prev_page);                    /* (the first tap's page step undone) */
        ui_mod_open(b, b == B_ENV ? SRC_ENV2 : ui.mod_pick, 1);
        ui.tap_t[b] = 0;
        return;
    }
    if (b == B_LFO && fm1_ms - ui.tap_t[b] < UI_DTAP_MS && !ui.mod && UI_PG[ui.prev_page].id[0] == UI_LPT) {
        ui_go(ui.prev_page);                        /* twice on the POINTS page: a point (the page step undone) */
        ui_lfo_add();
        ui.tap_t[b] = 0;
        return;
    }
    ui.tap_t[b] = fm1_ms;
    if (ui.mod == 2u && b == ui.mod_btn) {          /* a tap lets a latched layer go */
        ui_mod_close();
        return;
    }
    if (b == B_REC)
        return;
    for (f = 0; f < FAM_N; f++)
        if (FAM_BTN[f] == b) {
            ui.prev_page = ui.page;
            ui_family_tap(f);
        }
}

static void ui_buttons(void)
{
    uint32_t rel, pr = fm1_input_edges(&rel), b, bit;
    for (b = 0; b < NB; b++) {
        bit = 1u << b;
        if ((pr >> panel.btn[b]) & 1u) {
            ui.bt_down |= (uint16_t)bit;
            ui.bt_held &= (uint16_t)~bit;
            ui.bt_chord &= (uint16_t)~bit;
            ui.bt_t[b] = fm1_ms;
            ui_press(b);
        }
        if ((ui.bt_down & bit) && !(ui.bt_held & bit) && fm1_ms - ui.bt_t[b] >= HOLD_MS[settings_hold % 4u]) {
            ui.bt_held |= (uint16_t)bit;
            if (!(ui.bt_chord & bit))               /* (held for OCT's pages: no layer) */
                ui_hold(b);
        }
        if ((rel >> panel.btn[b]) & 1u) {
            int held = !!(ui.bt_held & bit);
            ui.bt_down &= (uint16_t)~bit;
            ui.bt_held &= (uint16_t)~bit;
            if (held || !(ui.bt_chord & bit))       /* (held for OCT's pages: no tap) */
                ui_release(b, held);
        }
    }
}

/* key k's note at octave oct, as SCL fits it (-1: silent): OFF, the key's own; SNAP, a note off the scale moved down
 * onto it; WHITE, the white keys walk the scale from the root at C4 and the black keys are silent (Felucca's
 * kb_pitch) */
static int32_t sy_ui_key_note(uint32_t k, int32_t oct)
{
    static const int8_t DEG[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};   /* the white keys' degrees from C */
    uint32_t mask = UI_SCALE_MASK[ui.scale % UI_NSCALE], i, cnt = 0;
    int32_t n = UI_KEY0 + (int32_t)k, d, wo;
    if (ui.fit == UI_FIT_SNAP) {
        for (i = 0; i < 12u && !((mask >> (uint32_t)((n - ui.root + 120) % 12)) & 1u); i++)
            n--;
    } else if (ui.fit == UI_FIT_WHITE) {
        if (DEG[n % 12] < 0)
            return -1;
        d = DEG[n % 12] + (n / 12 - 5) * 7;         /* (white keys from C4) */
        for (i = 0; i < 12u; i++)
            cnt += (mask >> i) & 1u;
        wo = d / (int32_t)cnt;
        d %= (int32_t)cnt;
        if (d < 0) {
            d += (int32_t)cnt;
            wo--;
        }
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (!d)
                    break;
                d--;
            }
        n = 60 + ui.root + 12 * wo + (int32_t)i;
    }
    n += 12 * oct;
    return n < 0 ? 0 : n > 127 ? 127 : n;
}
static uint32_t sy_ui_key_vel(void) { return ui.kvel ? ui.kvel : 100u; }

/* the keys in REC's MOD layer: a white key picks its source, no key plays; 1 = taken (no note). (ENV's layer:
 * the keys play, to hear what ENV 2 does) */
static int sy_ui_key(uint32_t k)
{
    if (ui_sec() == SEC_SAVE)                       /* (SAVE's screen: the keys type) */
        return ui_save_key(k);
    if ((ui.bt_down & (1u << B_SCL)) && !ui.mod) {  /* SCL held: a key is the root (no note; SCL's tap none) */
        char t[16];
        ui.root = (uint8_t)((UI_KEY0 + (int32_t)k) % 12);
        ui.bt_chord |= (uint16_t)(1u << B_SCL);
        sp_put(sp_put(t, "ROOT "), UI_NOTE_NAME[ui.root]);
        ui_toast(t);
        ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
        return 1;
    }
    if (!ui.mod || ui.mod_btn != B_REC)
        return 0;
    if (k < 27u && UI_KEY_SRC[k]) {
        ui.mod_src = ui.mod_pick = UI_KEY_SRC[k];
        ui.mod_last = -1;
        ui.dirty = D_ALL;
    }
    return 1;
}

/* ----------------------------------------------------------------------------------------- the loop --- */
/* every millisecond: the buttons, the encoders */
static void sy_ui_ms(void)
{
    uint32_t k;
    int32_t s;
    ui_buttons();
    if (ui.cd_btn != NB)
        ui_cd_tick();
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0)
            ui_knob(k, s);
    if ((s = panel_enc(EN_SELECT)) != 0)
        ui_step_pages(s, 0);
    if ((s = panel_enc(EN_ALGO)) != 0) {
        if ((ui.bt_down & (1u << B_FX)) && ui_sec() >= SEC_DRIVE && ui_sec() < SEC_DRIVE + SP_NFX && !ui.mod)
            ui_fx_move(s);                          /* (FX held: the effect along the rack) */
        else
            ui_step_section(s);
    }
    if ((s = panel_enc(EN_PRESET)) != 0)
        sy_app_sound_step(s);
    ui_save_ms();
    if (ui.toast[0] && (int32_t)(fm1_ms - ui.toast_t) >= 0) {
        ui.toast[0] = 0;
        ui.dirty |= D_FOOT;
    }
}

/* panel.c's settings hooks: the palette (SETTINGS > COLOR); saved by sy_app.c when they change (sy_ui_settings) */
static void palette_apply(void) { sy_palette(ui.pal); }
static void settings_save(void) {}

/* the settings the flash keeps (sy_app.c, SS_SETTINGS): "SET1", the palette, ACCEL, BPM (2), TS-BEAT, TS-NOTE, the
 * HOLD time; then SCL's scale, root, fit and velocity. Fields are only added at the end: a shorter record (an older
 * build's, 11 bytes at least) leaves the rest as they were. n 0: written into b, their length; else b's (n bytes)
 * taken: 1 (0: not these) */
#define SS_SETTINGS_LEN 15u
static uint32_t sy_ui_settings(uint8_t *b, uint32_t n)
{
    if (!n) {
        b[0] = 'S', b[1] = 'E', b[2] = 'T', b[3] = '1';
        b[4] = ui.pal;
        b[5] = ui.accel;
        b[6] = (uint8_t)ui.bpm;
        b[7] = (uint8_t)(ui.bpm >> 8);
        b[8] = ui.tsb;
        b[9] = ui.tsn;
        b[10] = settings_hold;
        b[11] = ui.scale;
        b[12] = ui.root;
        b[13] = ui.fit;
        b[14] = ui.kvel;
        return SS_SETTINGS_LEN;
    }
    if (n < 11u || n > SS_SETTINGS_LEN || b[0] != 'S' || b[1] != 'E' || b[2] != 'T' || b[3] != '1')
        return 0;
    ui.pal = (uint8_t)(b[4] % SY_NPAL);
    ui.accel = b[5] != 0;
    ui.bpm = (uint16_t)sy_clampi(b[6] | b[7] << 8, 20, 300);
    ui.tsb = (uint8_t)sy_clampi(b[8], 1, 16);
    ui.tsn = (uint8_t)sy_clampi(b[9], 0, 4);
    settings_hold = (uint8_t)(b[10] % 4u);
    if (n >= 15u) {
        ui.scale = (uint8_t)(b[11] % UI_NSCALE);
        ui.root = (uint8_t)(b[12] % 12u);
        ui.fit = (uint8_t)(b[13] % UI_NFIT);
        ui.kvel = (uint8_t)sy_clampi(b[14], 1, 127);
    }
    sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
    sy_palette(ui.pal);
    ui.dirty = D_ALL;
    return 1;
}

static void sy_ui_boot(void)
{
    ui.pal = 0;                                     /* STRATA */
    ui.accel = 1;
    ui.mod_pick = SRC_LFO1;
    ui.cd_btn = NB;
    ui.act_k = 0xFF;
    ui.sv_key = 0xFF;
    ui.kvel = 100;                                  /* SCL: chromatic from C, the keys as they are, 100 */
    ui.bpm = 120;                                   /* 120 BPM in 4/4 */
    ui.tsb = 4;
    ui.tsn = 2;
    sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
    sy_palette(ui.pal);
    ui_go(ui_first_page(SEC_OSCA));
}
