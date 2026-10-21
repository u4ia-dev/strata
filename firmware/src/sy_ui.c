/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* STRATA's user interface on the FM-1, Phase 4 (docs/ARCHITECTURE.md §17; its look: the STRATA UI, docs/UI.md).
 *   - Pages: a section's parameters four to a page, KNOB 1-4 left to right as on the panel, and what the display
 *     draws. A family button shows its sections' pages: a tap, the next page (EDIT: of the section ALGORITHM
 *     picked; the other families through all of theirs); ALGORITHM picks the family's section, SELECT steps its
 *     pages; OCT- / OCT+ with a family button (or REC) held, the section's pages, round (the octave stays, the
 *     button's release and hold do nothing). HOME: the sound playing, the macros, the output live; HOME held 5 s:
 *     the SETTINGS (COLOR: the palette; ACCEL: the knobs' feel; a countdown screen from HOLD). FX: DRIVE,
 *     CHORUS, DELAY, REVERB, OTT, EQ. GLO's GLOBAL page: BPM (20-300), TS-BEAT (1-16 beats a bar), TS-NOTE (the
 *     note a beat is: 1 2 4 8 16).
 *   - Knobs: speed-sensitive (a slow turn 0.1 % a detent, a quick one up to 5 %; ACCEL OFF: 1 % a detent); a list,
 *     a count or a short range one value a detent. An LFO's RATE and the delay's TIME with SYNC FIXED set a note
 *     value (the MOD layer: RATE and TIME themselves, whose modulation steps it). A knob turning lights its cell, and
 *     on the filter's and the effects' pages the part of the graph it moves, its value in a flag, the shape before
 *     the turn dashed behind, until the gesture ends (UI_GESTURE_MS without a turn).
 *   - SAVE (sy_ui_save.c): tapped once, the sound saved as a new preset at once (its next version, "NAME v2");
 *     held 5 s, over its own slot (a countdown screen from HOLD); tapped twice, the browser (SAVE there:
 *     NAME IT for the sound playing). SAVE + OCT- undoes, SAVE + OCT+ redoes: the patch's last 8 states (an edit
 *     gesture or a sound's load each).
 *   - An LFO's POINTS page: KNOB 1 picks a point (round), KNOB 2 moves it along the cycle (between its
 *     neighbours; the first and last stay at the ends), KNOB 3 up and down, KNOB 4 curves the segment from it (the
 *     last point: the one into it). A shape other than DRAWN shows its points; the first edit draws from them
 *     (SHAPE DRAWN). LFO tapped twice on the page: a point left of the one picked, halfway to the one before (the
 *     first point picked: right of it), on the curve. LFO held 5 s on the page: the point picked goes (a countdown
 *     screen from HOLD; let go, a knob or another button stops it; the first and last points stay).
 *   - The MOD layer: REC held (a double tap latches it, a tap lets it go). A white key picks the source, a knob
 *     sets that source's depth on its parameter: a matrix slot, made when needed and freed when the layer closes
 *     at 0. ENV held: the same for ENV 2 (an envelope onto a knob).
 *   - The other hold layers, while their button is held: GLO, PERFORM (KNOB 1 the bend, back to the middle when GLO
 *     is let go; 2 the mod wheel; 3 GLIDE; 4 VOLUME); FX, MACROS (KNOB 1-4 MACRO 1-4; the white keys F3 .. D4 turn
 *     the six effects on and off; ALGORITHM moves the effect shown along the chain); LFO, MOMENTARY (a knob's
 *     change lasts until LFO is let go; on an LFO's POINTS page LFO held deletes a point instead); EDIT, JUMP (a
 *     key opens its section: the white keys OSC A .. EQ, five black keys HOME, VOICE, GLOBAL, KEYS, SETTINGS).
 *   - SCL: the keys' scale, its root, how they fit it, their velocity; SCL held, a key is the root.
 *   - PLAY: the transport on and off (the FREE LFOs start on the beat; PLAY's green LED on, its own blinking each
 *     beat; ARP and PATTERN step on its beat). CLOCK IN MIDI (GLOBAL): MIDI's clock the tempo, its start and stop the
 *     transport.
 *   - V0.75, one grammar for the menu buttons (HOME, EDIT, ENV, LFO, FX, GLO, SCL, ARP): a tap opens the menu where it
 *     was left, another its next page (through its sections; ALGORITHM reaches VALUES); twice, its next section (FX:
 *     the chain view <-> the module picked; HOME: the presets); held, its layer (above); held 5 s, what it shows back
 *     to its starting values (a countdown from 2 s, nothing touched meanwhile; HOME: the SETTINGS); with OCT+ / OCT-,
 *     on / off (ui_combo). SELECT: the menu's section (one section: its pages); ALGORITHM: the section's pages. SAVE
 *     keeps its own (once, twice, held 5 s, + OCT undo and redo).
 *   - The SETTINGS (HOME held 5 s): SCREEN (the palette, HELP: a hint at the bottom entering a section, the LEDs'
 *     GLOW), PANEL (ACCEL, the HOLD time), MIDI (its CHANNEL, LEARN, CLEAR the learned CCs, PROGRAM changes).
 *     MIDI LEARN on: the next knob turned on any page picks its parameter, the next CC is set to it (OCT- takes
 *     the picked one's away, OCT+ ends it); a learned CC is set before a parameter's own.
 *   - The screen: one picture a module (the STRATA UI, docs/UI.md), drawn in strips and sent by the rows that changed
 *     (sy_ui_draw.c); the LEDs (sy_ui_draw.c sy_ui_leds).
 * MIDI's clock, start and stop come with integration (the real MIDI path). Needs sy_app.c's keys and sounds (declared here), panel.c, gfx.c, sy_gfx.c, the engine and
 * sy_text.c. */

static int sy_app_held(uint32_t k);                 /* sy_app.c: key k's note sounding (its LED) */
static void sy_app_octave(int32_t d);               /* .. OCT- / OCT+ */
static int32_t sy_app_oct(void);
static void sy_app_run(uint32_t on);                /* .. the transport on, off (V0.75: the audio side's beat) */
static int sy_app_clock(void);                      /* .. the tempo from MIDI's clock now (CLOCK IN MIDI) */
static int sy_app_clock_lost(void);                 /* .. it stopped coming (1 once: the panel's tempo again) */
static void sy_app_keys_hand(void);                 /* .. the keys held over to the chords (they just took the keys) */
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
static void sy_app_bend(uint32_t v);                /* .. the panel's bend (0 .. 16383, 8192 the middle) */
static void sy_app_cc(uint32_t cc, uint32_t v);     /* .. a controller (the mod wheel) */
/* .. and the platform's: BENCH (SETTINGS > BENCH, the console's `bench`), what it measured, the peaks since CLEAR
 * (sy_app.c fills the audio's; known 0: no clock of its own within a half, the emulator: only the counts mean
 * something) */
static struct {
    uint32_t known;                              /* 1: the FM-1's figures */
    uint32_t boot_ms;                            /* power-on to the first frame */
    uint32_t half_us, half_max_us, late;         /* a half's render: the last, the longest; halves the DMA had left */
    uint32_t late_held;                          /* .. late after the interrupts were held (flash work, RUN: hushed) */
    uint32_t frame_us, frame_max_us;             /* a frame's drawing: the last, the longest */
    uint16_t frame_max_page;                     /* .. the page the longest was of (UI_PG) */
    uint32_t gap_max_us, gap_save_max_us;        /* the longest the main loop left the keys (a layer open, a screen that
                                                  * types: else TIMER5 plays them), MIDI and the knobs waiting: while it
                                                  * drew, and over flash work (a SAVE, the background saves) */
    uint32_t t5_gap_max_us;                      /* the longest between two TIMER5 ticks (10 kHz: the keys' scan, the
                                                  * USB audio's packets; longer only with the interrupts held) */
    uint32_t step_max_us, build_max_ms;          /* a wavetable build: its longest step (a frame), its whole time */
    uint32_t erase_n, erase_max_us, prog_max_us; /* the flash: erases, the longest (4 KiB), a page's program */
    uint32_t stack_isr, stack_main;              /* bytes of the interrupts' and the main loop's stacks ever used */
    uint32_t spare, spare_hit;                   /* the SRAM Strata leaves unused (sy_main.c), bytes; .. written since */
    uint32_t run_int_us, run_flt_us;             /* RUN: a fixed integer loop and a float loop, each timed */
    uint8_t run;                                 /* RUN: 1 asked (the platform's main loop runs it), 2 done */
} sy_bench;

/* ------------------------------------------------------------------------------------------- pages --- */
#define UI_ITEM 1000                        /* an item that is no parameter: the SETTINGS', an LFO's points, GLOBAL's */
enum { UI_COLOR = UI_ITEM, UI_ACCEL, UI_LPT, UI_LX, UI_LY, UI_LC, UI_BPM, UI_TSB, UI_TSN, UI_EQPT, UI_EQF, UI_EQG, UI_EQQ,
       UI_SCUR, UI_SCHR, UI_SCAT, UI_SSLOT, UI_BCAT, UI_BPRE, UI_BBANK,   /* (SAVE's, the browser's: sy_ui_save.c) */
       UI_MSLOT, UI_MSRC, UI_MDST, UI_MAMT,           /* (the matrix's: the slot picked, its source, place, amount) */
       UI_SCALE, UI_ROOT, UI_FIT, UI_KVEL,            /* (SCL's: the keys' scale, its root, how keys fit, velocity) */
       UI_HELP, UI_GLOW, UI_HOLD, UI_MCH, UI_LEARN, UI_MCLR, UI_MPC,   /* (the SETTINGS' others) */
       UI_BRUN, UI_BCLR,                              /* (BENCH's: RUN, CLEAR the peaks) */
       UI_CTYPE, UI_CSLOT, UI_CMOVE, UI_CMIX,         /* (the FX chain view's: a type to add, a slot, MOVE, its MIX) */
       UI_HMODE, UI_HTYPE, UI_HVOI, UI_HSPR,          /* (V0.7, SCL's chords (sy_chord.c chd): CHORD's, the ARP's */
       UI_PSTY, UI_PRATE, UI_PMOVE, UI_PFEEL,         /* STYLE's, BASS's, MIDI's) */
       UI_BON, UI_BOCT, UI_BLVL, UI_CIN, UI_COUT, UI_COCH, UI_CBCH,
       UI_AGATE, UI_ASWING, UI_ALEN, UI_ALATCH,       /* (V0.75, ARP's: GROOVE's; a step's (the one picked: STEPS', */
       UI_SSTEP, UI_STYPE, UI_SLVL, UI_SOCT, UI_SRAT, UI_SCHN, UI_SSLD,   /* RATCHET's); STRUM's; GLOBAL's */
       UI_STRUM, UI_SDIR, UI_SHUM, UI_STAP,           /* CLOCK IN) */
       UI_CLOCK,
       UI_QSTEP, UI_QNOTE, UI_QVEL, UI_QLEN,          /* (V0.8, the sequencer's (sy_seq.c): NOTES', FEEL's and MOTION's */
       UI_QCHN, UI_QNUD, UI_QOCT,                     /* (the step picked: KNOB 1), PATTERN's, PLAY+REC's, CHAIN's: */
       UI_QLANE, UI_QVAL, UI_QPAR,                    /* sy_ui_seq.c) */
       UI_QPAT, UI_QPLEN, UI_QRATE, UI_QSWG,
       UI_QTRN, UI_QDIR, UI_QRMD, UI_QQNT,
       UI_QSLOT, UI_QCPAT, UI_QCREP, UI_QCHAIN };
#define SEC_MENU SEC_COUNT                          /* (sections of the UI's own: the SETTINGS, GLOBAL) */
#define SEC_GLOBAL (SEC_COUNT + 1)
#define SEC_SAVE (SEC_COUNT + 2)
#define SEC_BROWSE (SEC_COUNT + 3)
#define SEC_KEYS (SEC_COUNT + 4)                    /* (SCL: the keys' scale) */
#define SEC_ARP (SEC_COUNT + 5)                     /* (V0.75, the ARP's: STYLE, GROOVE; */
#define SEC_CHORD (SEC_COUNT + 6)                   /* SCL's chords: CHORD, STRUM, BASS; */
#define SEC_CMID (SEC_COUNT + 7)                    /* .. their MIDI; */
#define SEC_STEPS (SEC_COUNT + 8)                   /* the ARP's STEPS, RATCHET) */
#define SEC_SEQ (SEC_COUNT + 9)                     /* (V0.8, the SEQ button's: the steps (NOTES, FEEL, MOTION), */
#define SEC_SPAT (SEC_COUNT + 10)                   /* the pattern (PATTERN, PLAY+REC), */
#define SEC_SCHN (SEC_COUNT + 11)                   /* CHAIN) */
#define SEC_N (SEC_COUNT + 12)                      /* every section, the UI's own too (ui.spage) */
enum { G_LIST, G_WAVE, G_UNI, G_SPREAD, G_PITCH, G_OUT, G_SUB, G_NOISE, G_ENV, G_LFO, G_TEMPO, G_COLOR, G_HOME,
       G_FILTER, G_DRIVE, G_CHORUS, G_DELAY, G_REVERB, G_OTT, G_EQ, G_SAVE, G_BROWSE, G_MATRIX, G_KEYS, G_PANEL,
       G_MIDI, G_BENCH, G_CHAIN, G_FLANGER, G_PHASER, G_FFILT, G_CRUSH, G_TREM, G_RING, G_TAPE, G_VOICE, G_PLAY,
       G_VALUES, G_CHORD, G_PERF, G_BASS, G_CMIDI, G_STEPS, G_RATCH, G_GROOVE, G_STRUM,
       G_QNOTES, G_QFEEL, G_QMOTION, G_QPAT, G_QPLAY, G_QCHAIN, G_N };   /* (UI_PAGE) */
static const uint8_t UI_TSN_VAL[5] = {1, 2, 4, 8, 16};   /* TS-NOTE: a whole note .. a sixteenth */
typedef struct {
    uint8_t sec, graph;                             /* SEC_*, G_*: what the display draws */
    char name[9];                                   /* the page's name in the header (8 at most: V0.6's [8] left "RING
                                                     * MOD" without its end, the header ran on into id[]) */
    int16_t id[4];                                  /* KNOB 1-4: parameters (or UI_* items), -1 none */
} ui_page_t;
/* V0.67: VALUES, a module's last zone: its pages' knobs again, a page each in their order, as figures (sy_ui_draw.c
 * ui_p_values), turned in the finest unit (ui_knob); the zones' row names them as one zone, "VALUES 2/5" */
#define UI_V(S, A, B, C, D) {S, G_VALUES, "VALUES", {A, B, C, D}},
#define UI_OSC(O, S)                                                                                      \
    {S, G_WAVE, "WAVE", {SP_##O##_POS, SP_##O##_TABLE, SP_##O##_WARP, SP_##O##_WAMT}},                     \
    {S, G_UNI, "UNISON", {SP_##O##_UNI, SP_##O##_DETUNE, SP_##O##_BLEND, SP_##O##_WIDTH}},                \
    {S, G_SPREAD, "SPREAD", {SP_##O##_UMODE, SP_##O##_SPREAD, SP_##O##_PHASE, SP_##O##_RAND}},           \
    {S, G_PITCH, "PITCH", {SP_##O##_OCT, SP_##O##_SEMI, SP_##O##_FINE, SP_##O##_PHMODE}},                 \
    {S, G_OUT, "OUTPUT", {SP_##O##_LEVEL, SP_##O##_PAN, SP_##O##_ROUTE, -1}},                             \
    UI_V(S, SP_##O##_POS, SP_##O##_TABLE, SP_##O##_WARP, SP_##O##_WAMT)                                    \
    UI_V(S, SP_##O##_UNI, SP_##O##_DETUNE, SP_##O##_BLEND, SP_##O##_WIDTH)                                 \
    UI_V(S, SP_##O##_UMODE, SP_##O##_SPREAD, SP_##O##_PHASE, SP_##O##_RAND)                                \
    UI_V(S, SP_##O##_OCT, SP_##O##_SEMI, SP_##O##_FINE, SP_##O##_PHMODE)                                   \
    UI_V(S, SP_##O##_LEVEL, SP_##O##_PAN, SP_##O##_ROUTE, -1)
#define UI_ENV(E, S)                                                                                      \
    {S, G_ENV, "ADSR", {SP_##E##_ATK, SP_##E##_DEC, SP_##E##_SUS, SP_##E##_REL}},                          \
    {S, G_ENV, "SHAPE", {SP_##E##_HOLD, SP_##E##_ACRV, SP_##E##_DCRV, SP_##E##_RCRV}},                     \
    UI_V(S, SP_##E##_ATK, SP_##E##_DEC, SP_##E##_SUS, SP_##E##_REL)                                        \
    UI_V(S, SP_##E##_HOLD, SP_##E##_ACRV, SP_##E##_DCRV, SP_##E##_RCRV)
#define UI_LFO(L, S)                                                                                      \
    {S, G_LFO, "POINTS", {UI_LPT, UI_LX, UI_LY, UI_LC}},                                                   \
    {S, G_LFO, "SHAPE", {SP_##L##_SHAPE, SP_##L##_SYNC, SP_##L##_RATE, SP_##L##_MODE}},                    \
    {S, G_LFO, "TIME", {SP_##L##_DELAY, SP_##L##_RISE, SP_##L##_PHASE, SP_##L##_SMOOTH}},                  \
    UI_V(S, UI_LPT, UI_LX, UI_LY, UI_LC)                                                                   \
    UI_V(S, SP_##L##_SHAPE, SP_##L##_SYNC, SP_##L##_RATE, SP_##L##_MODE)                                   \
    UI_V(S, SP_##L##_DELAY, SP_##L##_RISE, SP_##L##_PHASE, SP_##L##_SMOOTH)
/* the effect modules' pages, of an instance whose ids are its type's first's moved by O (sy_param.c FXI_OFF); no ON:
 * the FX-held keys bypass a module */
#define UI_DRVP(S, O)                                                                                     \
    {S, G_DRIVE, "DRIVE", {SP_DR_TYPE + O, SP_DR_AMT + O, SP_DR_TONE + O, SP_DR_MIX + O}},                \
    UI_V(S, SP_DR_TYPE + O, SP_DR_AMT + O, SP_DR_TONE + O, SP_DR_MIX + O)
#define UI_CHOP(S, O)                                                                                     \
    {S, G_CHORUS, "CHORUS", {SP_CH_RATE + O, SP_CH_DEPTH + O, SP_CH_DELAY + O, SP_CH_MIX + O}},           \
    {S, G_CHORUS, "MORE", {SP_CH_FDBK + O, -1, -1, -1}},                                                  \
    UI_V(S, SP_CH_RATE + O, SP_CH_DEPTH + O, SP_CH_DELAY + O, SP_CH_MIX + O)                              \
    UI_V(S, SP_CH_FDBK + O, -1, -1, -1)
#define UI_OTTP(S, O)                                                                                     \
    {S, G_OTT, "OTT", {SP_OT_DEPTH + O, SP_OT_TIME + O, SP_OT_UP + O, SP_OT_DOWN + O}},                   \
    {S, G_OTT, "BANDS", {SP_OT_LOW + O, SP_OT_MID + O, SP_OT_HIGH + O, -1}},                              \
    UI_V(S, SP_OT_DEPTH + O, SP_OT_TIME + O, SP_OT_UP + O, SP_OT_DOWN + O)                                \
    UI_V(S, SP_OT_LOW + O, SP_OT_MID + O, SP_OT_HIGH + O, -1)
#define UI_EQP(S, O)                                                                                      \
    {S, G_EQ, "POINTS", {UI_EQPT, UI_EQF, UI_EQG, UI_EQQ}},                                               \
    {S, G_EQ, "GAINS", {SP_EQ_LOW + O, SP_EQ_MID + O, SP_EQ_HIGH + O, -1}},                               \
    UI_V(S, UI_EQPT, UI_EQF, UI_EQG, UI_EQQ)                                                              \
    UI_V(S, SP_EQ_LOW + O, SP_EQ_MID + O, SP_EQ_HIGH + O, -1)
#define UI_FLGP(S, O)                                                                                     \
    {S, G_FLANGER, "FLANGER", {SP_FL_RATE + O, SP_FL_DEPTH + O, SP_FL_DELAY + O, SP_FL_MIX + O}},         \
    {S, G_FLANGER, "MORE", {SP_FL_FDBK + O, SP_FL_SPREAD + O, SP_FL_SYNC + O, -1}},                       \
    UI_V(S, SP_FL_RATE + O, SP_FL_DEPTH + O, SP_FL_DELAY + O, SP_FL_MIX + O)                              \
    UI_V(S, SP_FL_FDBK + O, SP_FL_SPREAD + O, SP_FL_SYNC + O, -1)
#define UI_PHSP(S, O)                                                                                     \
    {S, G_PHASER, "PHASER", {SP_PH_RATE + O, SP_PH_DEPTH + O, SP_PH_CENTER + O, SP_PH_MIX + O}},          \
    {S, G_PHASER, "MORE", {SP_PH_FDBK + O, SP_PH_STAGES + O, SP_PH_SPREAD + O, SP_PH_SYNC + O}},          \
    UI_V(S, SP_PH_RATE + O, SP_PH_DEPTH + O, SP_PH_CENTER + O, SP_PH_MIX + O)                             \
    UI_V(S, SP_PH_FDBK + O, SP_PH_STAGES + O, SP_PH_SPREAD + O, SP_PH_SYNC + O)
#define UI_FFXP(S, O)                                                                                     \
    {S, G_FFILT, "FILTER", {SP_FF_TYPE + O, SP_FF_CUT + O, SP_FF_RES + O, SP_FF_MIX + O}},                \
    {S, G_FFILT, "MORE", {SP_FF_DRIVE + O, -1, -1, -1}},                                                  \
    UI_V(S, SP_FF_TYPE + O, SP_FF_CUT + O, SP_FF_RES + O, SP_FF_MIX + O)                                  \
    UI_V(S, SP_FF_DRIVE + O, -1, -1, -1)
#define UI_CRUP(S, O)                                                                                     \
    {S, G_CRUSH, "CRUSH", {SP_CR_BITS + O, SP_CR_RATE + O, SP_CR_MIX + O, -1}},                           \
    UI_V(S, SP_CR_BITS + O, SP_CR_RATE + O, SP_CR_MIX + O, -1)
#define UI_TRMP(S, O)                                                                                     \
    {S, G_TREM, "TREMOLO", {SP_TR_MODE + O, SP_TR_SHAPE + O, SP_TR_RATE + O, SP_TR_DEPTH + O}},           \
    {S, G_TREM, "MORE", {SP_TR_SYNC + O, -1, -1, -1}},                                                    \
    UI_V(S, SP_TR_MODE + O, SP_TR_SHAPE + O, SP_TR_RATE + O, SP_TR_DEPTH + O)                             \
    UI_V(S, SP_TR_SYNC + O, -1, -1, -1)
#define UI_RNGP(S, O)                                                                                     \
    {S, G_RING, "RING MOD", {SP_RG_FREQ + O, SP_RG_SHAPE + O, SP_RG_SPREAD + O, SP_RG_MIX + O}},          \
    UI_V(S, SP_RG_FREQ + O, SP_RG_SHAPE + O, SP_RG_SPREAD + O, SP_RG_MIX + O)
#define UI_TAPP(S, O)                                                                                     \
    {S, G_TAPE, "TAPE", {SP_TP_WOW + O, SP_TP_FLUT + O, SP_TP_SAT + O, SP_TP_MIX + O}},                   \
    {S, G_TAPE, "TONE", {SP_TP_TONE + O, -1, -1, -1}},                                                    \
    UI_V(S, SP_TP_WOW + O, SP_TP_FLUT + O, SP_TP_SAT + O, SP_TP_MIX + O)                                  \
    UI_V(S, SP_TP_TONE + O, -1, -1, -1)
static const ui_page_t UI_PG[] = {
    UI_OSC(A, SEC_OSCA)
    UI_OSC(B, SEC_OSCB)
    {SEC_SUB, G_SUB, "MAIN", {SP_SUB_SHAPE, SP_SUB_OCT, SP_SUB_LEVEL, SP_SUB_PW}},
    {SEC_SUB, G_OUT, "OUTPUT", {SP_SUB_PAN, SP_SUB_ROUTE, -1, -1}},
    UI_V(SEC_SUB, SP_SUB_SHAPE, SP_SUB_OCT, SP_SUB_LEVEL, SP_SUB_PW)
    UI_V(SEC_SUB, SP_SUB_PAN, SP_SUB_ROUTE, -1, -1)
    {SEC_NOISE, G_NOISE, "MAIN", {SP_NZ_TYPE, SP_NZ_COLOR, SP_NZ_STEREO, SP_NZ_LEVEL}},
    {SEC_NOISE, G_OUT, "OUTPUT", {SP_NZ_PAN, SP_NZ_ROUTE, -1, -1}},
    UI_V(SEC_NOISE, SP_NZ_TYPE, SP_NZ_COLOR, SP_NZ_STEREO, SP_NZ_LEVEL)
    UI_V(SEC_NOISE, SP_NZ_PAN, SP_NZ_ROUTE, -1, -1)
    {SEC_FILTER, G_FILTER, "TONE", {SP_F_TYPE, SP_F_CUT, SP_F_RES, SP_F_DRIVE}},
    {SEC_FILTER, G_FILTER, "MORE", {SP_F_KEY, SP_F_VAR, SP_F_MIX, -1}},
    UI_V(SEC_FILTER, SP_F_TYPE, SP_F_CUT, SP_F_RES, SP_F_DRIVE)
    UI_V(SEC_FILTER, SP_F_KEY, SP_F_VAR, SP_F_MIX, -1)
    UI_ENV(E1, SEC_ENV1)
    UI_ENV(E2, SEC_ENV2)
    UI_LFO(L1, SEC_LFO1)
    UI_LFO(L2, SEC_LFO2)
    {SEC_VOICE, G_VOICE, "VOICE", {SP_V_MODE, SP_V_POLY, SP_V_GLIDE, SP_V_GLMODE}},
    {SEC_VOICE, G_PLAY, "PLAY", {SP_V_BUP, SP_V_BDN, SP_V_VEL, SP_V_VOL}},
    UI_V(SEC_VOICE, SP_V_MODE, SP_V_POLY, SP_V_GLIDE, SP_V_GLMODE)
    UI_V(SEC_VOICE, SP_V_BUP, SP_V_BDN, SP_V_VEL, SP_V_VOL)
    {SEC_GLOBAL, G_TEMPO, "TEMPO", {UI_BPM, UI_TSB, UI_TSN, UI_CLOCK}},
    {SEC_MACRO, G_HOME, "HOME", {SP_MAC1, SP_MAC2, SP_MAC3, SP_MAC4}},
    {SEC_MATRIX, G_MATRIX, "SLOTS", {UI_MSLOT, UI_MSRC, UI_MDST, UI_MAMT}},
    {SEC_CHAIN, G_CHAIN, "CHAIN", {UI_CTYPE, UI_CSLOT, UI_CMOVE, UI_CMIX}},
    UI_DRVP(SEC_DRIVE, 0)
    UI_CHOP(SEC_CHORUS, 0)
    {SEC_DELAY, G_DELAY, "DELAY", {SP_DL_MODE, SP_DL_SYNC, SP_DL_TIME, SP_DL_FDBK}},
    {SEC_DELAY, G_DELAY, "TONE", {SP_DL_LP, SP_DL_HP, SP_DL_MIX, -1}},
    UI_V(SEC_DELAY, SP_DL_MODE, SP_DL_SYNC, SP_DL_TIME, SP_DL_FDBK)
    UI_V(SEC_DELAY, SP_DL_LP, SP_DL_HP, SP_DL_MIX, -1)
    {SEC_REVERB, G_REVERB, "REVERB", {SP_RV_TYPE, SP_RV_SIZE, SP_RV_DECAY, SP_RV_MIX}},
    {SEC_REVERB, G_REVERB, "TONE", {SP_RV_DAMP, SP_RV_PRE, SP_RV_WIDTH, SP_RV_LOCUT}},
    UI_V(SEC_REVERB, SP_RV_TYPE, SP_RV_SIZE, SP_RV_DECAY, SP_RV_MIX)
    UI_V(SEC_REVERB, SP_RV_DAMP, SP_RV_PRE, SP_RV_WIDTH, SP_RV_LOCUT)
    UI_OTTP(SEC_OTT, 0)
    UI_EQP(SEC_EQ, 0)
    UI_DRVP(SEC_DRIVE2, SP_DR2_ON - SP_DR_ON)
    UI_CHOP(SEC_CHORUS2, SP_CH2_ON - SP_CH_ON)
    UI_OTTP(SEC_OTT2, SP_OT2_ON - SP_OT_ON)
    UI_EQP(SEC_EQ2, SP_EQ2_ON - SP_EQ_ON)
    UI_FLGP(SEC_FLANGER, 0)
    UI_FLGP(SEC_FLANGER2, SP_FL2_ON - SP_FL_ON)
    UI_PHSP(SEC_PHASER, 0)
    UI_PHSP(SEC_PHASER2, SP_PH2_ON - SP_PH_ON)
    UI_FFXP(SEC_FFILT, 0)
    UI_FFXP(SEC_FFILT2, SP_FF2_ON - SP_FF_ON)
    UI_CRUP(SEC_CRUSH, 0)
    UI_CRUP(SEC_CRUSH2, SP_CR2_ON - SP_CR_ON)
    UI_TRMP(SEC_TREM, 0)
    UI_TRMP(SEC_TREM2, SP_TR2_ON - SP_TR_ON)
    UI_RNGP(SEC_RING, 0)
    UI_RNGP(SEC_RING2, SP_RG2_ON - SP_RG_ON)
    UI_TAPP(SEC_TAPE, 0)
    UI_TAPP(SEC_TAPE2, SP_TP2_ON - SP_TP_ON)
    {SEC_MENU, G_COLOR, "SCREEN", {UI_COLOR, UI_HELP, UI_GLOW, -1}},
    {SEC_MENU, G_PANEL, "PANEL", {UI_ACCEL, UI_HOLD, -1, -1}},
    {SEC_MENU, G_MIDI, "MIDI", {UI_MCH, UI_LEARN, UI_MCLR, UI_MPC}},
    {SEC_MENU, G_BENCH, "BENCH", {UI_BRUN, UI_BCLR, -1, -1}},
    {SEC_KEYS, G_KEYS, "SCALE", {UI_SCALE, UI_ROOT, UI_FIT, UI_KVEL}},
    {SEC_CHORD, G_CHORD, "CHORD", {UI_HMODE, UI_HTYPE, UI_HVOI, UI_HSPR}},   /* (V0.7: sy_ui_chord.c) */
    {SEC_CHORD, G_STRUM, "STRUM", {UI_STRUM, UI_SDIR, UI_SHUM, UI_STAP}},
    {SEC_CHORD, G_BASS, "BASS", {UI_BON, UI_BOCT, UI_BLVL, -1}},
    {SEC_CMID, G_CMIDI, "MIDI", {UI_CIN, UI_COUT, UI_COCH, UI_CBCH}},
    {SEC_ARP, G_PERF, "STYLE", {UI_PSTY, UI_PRATE, UI_PMOVE, UI_PFEEL}},    /* (V0.75: the ARP button's) */
    {SEC_ARP, G_GROOVE, "GROOVE", {UI_AGATE, UI_ASWING, UI_ALEN, UI_ALATCH}},
    {SEC_STEPS, G_STEPS, "STEPS", {UI_SSTEP, UI_STYPE, UI_SLVL, UI_SOCT}},
    {SEC_STEPS, G_RATCH, "RATCHET", {UI_SSTEP, UI_SRAT, UI_SCHN, UI_SSLD}},
    {SEC_SEQ, G_QNOTES, "NOTES", {UI_QSTEP, UI_QNOTE, UI_QVEL, UI_QLEN}},     /* (V0.8: sy_ui_seq.c) */
    {SEC_SEQ, G_QFEEL, "FEEL", {UI_QSTEP, UI_QCHN, UI_QNUD, UI_QOCT}},
    {SEC_SEQ, G_QMOTION, "MOTION", {UI_QSTEP, UI_QLANE, UI_QVAL, UI_QPAR}},
    {SEC_SPAT, G_QPAT, "PATTERN", {UI_QPAT, UI_QPLEN, UI_QRATE, UI_QSWG}},
    {SEC_SPAT, G_QPLAY, "PLAY+REC", {UI_QTRN, UI_QDIR, UI_QRMD, UI_QQNT}},
    {SEC_SCHN, G_QCHAIN, "CHAIN", {UI_QSLOT, UI_QCPAT, UI_QCREP, UI_QCHAIN}},
    {SEC_SAVE, G_SAVE, "NAME IT", {UI_SCUR, UI_SCHR, UI_SCAT, UI_SSLOT}},
    {SEC_BROWSE, G_BROWSE, "BROWSE", {UI_BCAT, UI_BPRE, UI_BBANK, -1}},
};
#define UI_NPG (sizeof UI_PG / sizeof UI_PG[0])

/* the families: the sections each button shows (0xFF ends). FX: the chain view (SEC_CHAIN), then the modules in the
 * chain, in its order (ui_fam_secs; every effect section is FX's: ui_fam_of). SEQ (V0.8): the sequencer's steps,
 * its pattern, its chain (the matrix is GLO's) */
enum { FAM_HOME, FAM_EDIT, FAM_ENV, FAM_LFO, FAM_SEQ, FAM_FX, FAM_GLO, FAM_MENU, FAM_SAVE, FAM_SCL, FAM_ARP, FAM_N };
static const uint8_t FAM_SECS[FAM_N][7] = {
    {SEC_MACRO, 0xFF}, {SEC_OSCA, SEC_OSCB, SEC_SUB, SEC_NOISE, SEC_FILTER, 0xFF}, {SEC_ENV1, SEC_ENV2, 0xFF},
    {SEC_LFO1, SEC_LFO2, 0xFF}, {SEC_SEQ, SEC_SPAT, SEC_SCHN, 0xFF}, {SEC_CHAIN, 0xFF},
    {SEC_VOICE, SEC_MATRIX, SEC_GLOBAL, 0xFF}, {SEC_MENU, 0xFF},
    {SEC_SAVE, SEC_BROWSE, 0xFF}, {SEC_KEYS, SEC_CHORD, SEC_CMID, 0xFF}, {SEC_ARP, SEC_STEPS, 0xFF},
};
static const uint8_t FAM_BTN[FAM_N] = {B_HOME, B_EDIT, B_ENV, B_LFO, B_SEQ, B_FX, B_GLO, NB, NB, B_SCL, B_ARP};
static int ui_is_fx(uint32_t sec) { return sec >= SEC_DRIVE && sec < SEC_CHAIN; }   /* an effect module's section */
/* the effects' costs on one core, % (TESTING.md §4: the cost model at the measured ~150 instructions a us; the new
 * modules' estimated), by type: the chain view's sum */
static const uint8_t FXT_COST[FXT_N] = {2, 5, 6, 13, 5, 5, 4, 4, 3, 1, 1, 2, 3};

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

/* MIDI LEARN's map: a CC each (0xFF none) and the parameter it sets, kept with the settings. MIDI's side (sy_app.c, an
 * interrupt on the FM-1) reads it and asks for a learn through ml_heard; the main loop writes it, the parameter first
 * and then the CC that makes an entry count */
#define ML_N 32u
static volatile uint8_t ml_cc[ML_N];
static volatile uint16_t ml_id[ML_N];
static volatile uint8_t ml_heard;                   /* a CC heard while LEARN listens: its number + 1 (0 none) */
static int ml_never(uint32_t cc) { return cc == 0u || cc == 32u || cc == 64u || cc >= 120u; }   /* (bank, sustain, */
static int32_t sy_ui_learned(uint32_t cc)           /* the parameter cc sets, -1 none */      /* the modes) */
{
    uint32_t i;
    for (i = 0; i < ML_N; i++)
        if (ml_cc[i] == cc)
            return ml_id[i];
    return -1;
}
static uint32_t ml_count(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < ML_N; i++)
        n += ml_cc[i] != 0xFFu;
    return n;
}
static void ml_drop(uint32_t cc, int32_t id)        /* the entries of cc or of parameter id go */
{
    uint32_t i;
    for (i = 0; i < ML_N; i++)
        if (ml_cc[i] != 0xFFu && (ml_cc[i] == cc || (int32_t)ml_id[i] == id))
            ml_cc[i] = 0xFFu;
}

/* the MOD layer's white keys, F3 .. G5: the sources (black keys: none) */
static const uint8_t UI_KEY_SRC[27] = {
    SRC_ENV1, 0, SRC_ENV2, 0, SRC_LFO1, 0, SRC_LFO2, SRC_MAC1, 0, SRC_MAC2, 0, SRC_MAC3, SRC_MAC4, 0,
    SRC_VEL, 0, SRC_NOTE, 0, SRC_RAND, SRC_ALT, 0, SRC_WHEEL, 0, SRC_BEND, SRC_AT, 0, SRC_EXPR,
};
/* FX held (MACROS): the white keys F3 .. F4 bypass the chain's modules 1 .. 8 or take them back (-1 none) */
static const int8_t UI_KEY_FX[27] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1,
                                     -1, -1, -1, -1, -1, -1};
/* EDIT held (JUMP): the section each key opens (0xFF none): the white keys in the panel's order, the black keys the
 * rest; the last six white keys the chain's modules 1 .. 6 (UI_JUMP_FX + its slot), A#4 the chain view */
#define UI_JUMP_FX 0xE0u
static const uint8_t UI_KEY_JUMP[27] = {
    SEC_OSCA, SEC_MACRO, SEC_OSCB, SEC_VOICE, SEC_SUB, SEC_GLOBAL, SEC_NOISE, SEC_FILTER, SEC_KEYS, SEC_ENV1, SEC_MENU,
    SEC_ENV2, SEC_LFO1, 0xFF, SEC_LFO2, 0xFF, SEC_MATRIX, SEC_CHAIN, UI_JUMP_FX, UI_JUMP_FX + 1, 0xFF, UI_JUMP_FX + 2,
    0xFF, UI_JUMP_FX + 3, UI_JUMP_FX + 4, 0xFF, UI_JUMP_FX + 5};
/* the section key k opens in JUMP: a chain slot's, its module's (0xFF: none) */
static uint32_t ui_jump_sec(uint32_t k)
{
    uint32_t c = k < 27u ? UI_KEY_JUMP[k] : 0xFFu;
    int32_t i;
    if (c == 0xFFu || c < UI_JUMP_FX)
        return c;
    i = sp_chain_at(sy_patch.p, c - UI_JUMP_FX);
    return i < 0 ? 0xFFu : SEC_DRIVE + (uint32_t)i;
}

/* ------------------------------------------------------------------------------------------- state --- */
#define D_HEAD 1u
#define D_DISP 2u
#define D_KNOB(k) (4u << (k))
#define D_KNOBS 60u
#define D_FOOT 64u
#define D_ALL 127u
#define D_LIVE 128u                                 /* the display's live rows alone (HOME's scope) */
#define UI_DTAP_MS 300u                             /* a second tap within this: a double tap (it latches) */
enum { LAY_NONE, LAY_PERF, LAY_MAC, LAY_MOM, LAY_JUMP, LAY_PAT };   /* the hold layers besides MOD: GLO, FX, LFO, EDIT,
                                                                      * SEQ (V0.8: PATTERNS) */
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
    uint8_t lay, lay_btn;                           /* another hold layer (LAY_*) and its button */
    int16_t bend;                                   /* PERFORM: the panel's bend (-8192 .. 8191), its mod wheel */
    uint8_t wheel;
    uint8_t mom_n;                                  /* MOMENTARY: the parameters changed, as they were */
    uint16_t mom_id[16];
    int16_t mom_v[16];
    int16_t mod_last;                               /* the parameter it set last (the banner), -1 none */
    uint32_t bt_t[NB], tap_t[NB];                   /* a button's press, its last tap */
    uint16_t bt_down, bt_held;                      /* buttons down; held past HOLD (their layer on) */
    uint16_t bt_chord;                              /* .. held while OCT stepped the pages (no tap, no layer) */
    uint16_t prev_page;                             /* the page before the last tap (a double tap puts it back) */
    uint8_t lpt[2];                                 /* the LFOs' points picked (POINTS) */
    uint8_t eqpt;                                   /* the EQ's point picked (POINTS): LOW, MID, HIGH */
    uint8_t mslot;                                  /* the matrix's slot picked (SEQ), 0 .. NSLOT - 1 */
    uint8_t help, glow;                             /* SETTINGS: a section's hint on entering it; the LEDs' glow (LO) */
    uint8_t mch, mpc;                               /* .. MIDI's channel (0 any, 1 .. 16); program changes taken */
    uint8_t learn;                                  /* .. MIDI LEARN on (the next knob picks, the next CC goes to it) */
    int16_t ml_pick;                                /* .. the parameter picked, -1 none */
    uint32_t clr_t;                                 /* .. CLEAR turned once: a second turn within 2 s clears; 0 none */
    uint8_t run;                                    /* PLAY: the transport running, since run_t (its beat: the audio */
    uint32_t run_t;                                 /* side's, chs.tq) */
    uint8_t astep;                                  /* ARP's STEPS, RATCHET: the step picked (0 .. 15) */
    uint8_t qslot;                                  /* V0.8, CHAIN: the slot picked (0 .. 15) */
    uint32_t qtouch_t;                              /* .. a lane's knob's last turn (sqd.ltouch let go after it) */
    uint8_t lastmode;                               /* SCL + OCT+: the chords' MODE back on (KEY, FREE) */
    uint8_t lay_lock;                               /* GLO + OCT+: PERFORM's layer latched (a tap, GLO + OCT-: let go) */
    uint32_t abeat_t;                               /* .. a beat of ARP's or PATTERN's (the ARP LED dark a moment) */
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
    char toast[32];                                 /* a word in the bottom row, until toast_t */
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
    uint8_t undel;                                  /* .. a preset just deleted: SAVE + OCT- puts it back (sy_app.c) */
    uint8_t ctype, cslot;                           /* the FX chain view: the type picked (left), the slot (right; */
                                                    /* past the last module: the place to add one) */
    uint32_t ub_t0;                                 /* OCT- + OCT+ held (sy_main.c): its countdown from then; 0 none */
    uint8_t safe;                                   /* SAFE START shown (1 a crash before, 2 HOME held), until safe_t */
    uint32_t safe_t;
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

/* SAFE START's screen gone (a button, a knob) */
static void ui_safe_end(void)
{
    if (!ui.safe)
        return;
    ui.safe = 0;
    ui.dirty = D_ALL;
}
/* sy_app.c: the sound before was not brought back (1: a crash, a hang or a failed boot before; 2: HOME held) */
static void sy_ui_safe(uint32_t why)
{
    ui.safe = (uint8_t)why;
    ui.safe_t = fm1_ms + 6000u;
    ui.dirty = D_ALL;
}

/* MIDI's (sy_app.c sy_midi_in): this CC is for LEARN (1: taken, nothing else to do with it); channel ch (0 .. 15)
 * heard; program changes taken */
static int sy_ui_learn_cc(uint32_t cc)
{
    if (!ui.learn || ui.ml_pick < 0 || ml_never(cc))
        return 0;
    ml_heard = (uint8_t)(cc + 1u);
    return 1;
}
static int sy_ui_midi_ch(uint32_t ch) { return !ui.mch || ch + 1u == ui.mch; }
static int sy_ui_pc_on(void) { return ui.mpc; }
/* MIDI's start (top: from the top), continue, stop with CLOCK IN MIDI (sy_app.c): PLAY's state (its LEDs, GLOBAL's
 * metronome; the audio side has its own event) */
static void sy_ui_run(int on, int top)
{
    if (on && (top || !ui.run)) {
        ui.run_t = fm1_ms;
        if (top)
            sy_lfo_sync = 1;
    }
    ui.run = (uint8_t)(on != 0);
    ui.dirty |= D_DISP | D_HEAD;
}

/* ------------------------------------------------------------------------------------------ helpers --- */
static uint32_t ui_sec(void) { return UI_PG[ui.page].sec; }
static const char *ui_sec_name(uint32_t s)
{
    return s < SEC_COUNT ? SEC_NAMES[s] : s == SEC_MENU ? "SETTINGS" : s == SEC_GLOBAL ? "GLOBAL" : s == SEC_SAVE ? "SAVE"
           : s == SEC_KEYS ? "SCALE" : s == SEC_ARP ? "ARP" : s == SEC_CHORD ? "CHORD" : s == SEC_CMID ? "MIDI"
           : s == SEC_STEPS ? "STEPS" : s == SEC_SEQ ? "SEQ" : s == SEC_SPAT ? "PATTERN" : s == SEC_SCHN ? "CHAIN"
           : "PRESETS";
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
    }
    return ui_is_fx(s) ? (int32_t)FXI_ON[s - SEC_DRIVE] : -1;   /* (a module: bypassed or in) */
}
/* the effect module the page shows: its instance (-1: not a module's page), its ids' offset from its type's first's */
static int32_t ui_fx_inst(void) { return ui_is_fx(UI_PG[ui.page].sec) ? (int32_t)(UI_PG[ui.page].sec - SEC_DRIVE) : -1; }
static uint32_t ui_fx_off(void) { return ui_fx_inst() >= 0 ? FXI_OFF((uint32_t)ui_fx_inst()) : 0u; }

static uint32_t ui_fam_of(uint32_t sec)
{
    uint32_t f, k;
    if (ui_is_fx(sec))
        return FAM_FX;
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
static int32_t ui_cmix(void);                        /* the chain view's MIX: the slot picked's (-1 none) */
static int32_t ui_virt(int32_t id)
{
    static const uint8_t F[3] = {SP_EQ_LFREQ, SP_EQ_MFREQ, SP_EQ_HFREQ}, G[3] = {SP_EQ_LOW, SP_EQ_MID, SP_EQ_HIGH};
    static const uint8_t Q[3] = {SP_EQ_LQ, SP_EQ_Q, SP_EQ_HQ};
    uint32_t p = ui.eqpt < 3u ? ui.eqpt : 0u, m = 3u * (ui.mslot < NSLOT ? ui.mslot : 0u), o = ui_fx_off();
    if (id >= UI_MSRC && id <= UI_MAMT)
        return (int32_t)(SP_M1_SRC + m) + (id - UI_MSRC);
    if (id == UI_CMIX)
        return ui_cmix();
    return id == UI_EQF ? (int32_t)(F[p] + o) : id == UI_EQG ? (int32_t)(G[p] + o) : id == UI_EQQ ? (int32_t)(Q[p] + o)
                                                                                               : id;
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
static int ui_src_used(uint32_t src)                /* source src moves anything */
{
    uint32_t s;
    for (s = 0; s < NSLOT; s++)
        if (sy_patch.p[SP_M1_SRC + 3u * s] == (int32_t)src && sy_patch.p[SP_M1_AMT + 3u * s])
            return 1;
    return 0;
}

/* ---------------------------------------------------------------------------------------- navigation --- */
/* HELP: a section's hint at the bottom as it opens, 2 s (0 none: its page says it, or it needs none) */
static const char *ui_hint(uint32_t sec)
{
    if (sec == SEC_OSCA || sec == SEC_OSCB || sec == SEC_SUB || sec == SEC_NOISE || sec == SEC_FILTER)
        return "EDIT+OCT+ ON \xB7 EDIT+OCT- OFF";
    if (sec == SEC_ENV1 || sec == SEC_ENV2)
        return "ENV HELD: ENV 2 ONTO A KNOB";
    if (sec == SEC_LFO1 || sec == SEC_LFO2)
        return "LFO+OCT+ ADDS A POINT, OCT- DELETES";
    if (sec == SEC_VOICE)
        return "GLO HELD: BEND, WHEEL, GLIDE";
    if (sec == SEC_MACRO)
        return "SAVE: KEEP IT \xB7 TWICE: BROWSE";
    if (sec == SEC_MATRIX)
        return "KNOB 1: THE SLOT TO SET";
    if (ui_is_fx(sec))
        return "FX TWICE: THE CHAIN";
    if (sec == SEC_CHAIN)
        return "FX+OCT+ ADDS \xB7 FX TWICE OPENS";
    if (sec == SEC_GLOBAL)
        return "PLAY: THE LFOS ON THE BEAT";
    if (sec == SEC_MENU)
        return "HOME: BACK TO THE SOUND";
    if (sec == SEC_KEYS)                            /* (V0.7) */
        return "SCL HELD + A KEY: THE ROOT";
    if (sec == SEC_CHORD)
        return "SCL+OCT+ CHORDS ON, OCT- OFF";
    if (sec == SEC_ARP)                             /* (V0.75) */
        return "ARP+OCT+ ON, OCT- OFF";
    if (sec == SEC_STEPS)
        return "KNOB 1: THE STEP";
    if (sec == SEC_SEQ)                             /* (V0.8) */
        return "REC ARMS IT \xB7 STOPPED: KEYS FILL A STEP";
    if (sec == SEC_SPAT)
        return "SEQ+OCT+ NEXT PATTERN, OCT- BACK";
    if (sec == SEC_SCHN)
        return "SEQ HELD: THE PATTERNS";
    return 0;
}

static void ui_go(uint32_t page)
{
    uint32_t f = ui_fam_of(UI_PG[page].sec), was = UI_PG[ui.page].sec;
    const char *h;
    ui.page = (uint16_t)page;
    ui.fam = (uint8_t)f;
    ui.fpage[f] = (uint16_t)page;
    if (UI_PG[page].sec < SEC_N)
        ui.spage[UI_PG[page].sec] = (uint16_t)page;
    ui.dirty = D_ALL;
    if (ui.help && UI_PG[page].sec != was && !ui.mod && !ui.lay && (h = ui_hint(UI_PG[page].sec)) != 0) {
        ui_toast(h);
        ui.toast_t = fm1_ms + 2000u;
    }
}

/* family f's sections in the order they show (FX's: the chain's modules, in its order; none: the chain view), into
 * sec (NFXC at least); their count */
static uint32_t ui_fam_secs(uint32_t f, uint8_t *sec)
{
    uint32_t n;
    if (f == FAM_FX) {
        uint32_t k, nc = sp_chain_n(sy_patch.p);
        for (k = 0; k < nc; k++)
            sec[k] = (uint8_t)(SEC_DRIVE + (uint32_t)sp_chain_at(sy_patch.p, k));
        if (!nc)
            sec[nc++] = SEC_CHAIN;
        return nc;
    }
    for (n = 0; FAM_SECS[f][n] != 0xFF; n++)
        sec[n] = FAM_SECS[f][n];
    return n;
}

/* s pages on, round: the family's (SELECT, a family button's tap: its sections in their order, each one's pages),
 * the section's (sec: EDIT's tap, OCT- / OCT+ with a family button held); graphs: the drawings only (a family
 * button's tap: ALGORITHM and OCT reach the VALUES pages) */
static void ui_step_pages(int32_t s, int sec, int graphs)
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
            if (UI_PG[i].sec == secs[k] && (!graphs || UI_PG[i].graph != G_VALUES || i == ui.page)) {
                if (i == ui.page)
                    cur = n;
                pg[n++] = (uint16_t)i;
            }
    if (n)
        ui_go(pg[(uint32_t)(((int32_t)cur + s % (int32_t)n + (int32_t)n) % (int32_t)n)]);
}

/* a module's page shown, its module still in the chain (or not a module's page) */
static int ui_page_live(uint32_t page)
{
    uint32_t s = UI_PG[page].sec;
    return !ui_is_fx(s) || sp_chain_find(sy_patch.p, s - SEC_DRIVE) >= 0;
}
/* a family button's tap: its page (another family; FX: the view it showed, the chain view if that module has gone),
 * else the next of the section (EDIT, FX's module) or of the family (the chain view: none) */
static void ui_family_tap(uint32_t f)
{
    uint8_t secs[NFXC];
    if (ui.fam != f)
        ui_go(ui_in_fam(ui.fpage[f], f) && ui_page_live(ui.fpage[f]) ? ui.fpage[f]
              : f == FAM_FX                                       ? ui_first_page(SEC_CHAIN)
                                                                  : (ui_fam_secs(f, secs), ui_first_page(secs[0])));
    else if (ui_sec() != SEC_CHAIN)
        ui_step_pages(1, 0, 1);
}

/* SELECT: the family's sections in their order (each on the page it showed last) */
static void ui_step_section(int32_t s)
{
    uint8_t secs[NFXC];
    uint32_t n = ui_fam_secs(ui.fam, secs), k = 0, i, sec = ui_sec(), next;
    for (i = 0; i < n; i++)
        if (secs[i] == sec)
            k = i;
    if (n < 2u) {                                   /* (one section: its pages, as ALGORITHM) */
        ui_step_pages(s, 1, 0);
        return;
    }
    k = (uint32_t)(((int32_t)k + s % (int32_t)n + (int32_t)n) % (int32_t)n);
    next = secs[k];
    ui_go(UI_PG[ui.spage[next]].sec == next ? ui.spage[next] : ui_first_page(next));
}

/* --------------------------------------------------------------------------------------- the chain --- */
/* the FX chain view (SEC_CHAIN), as the preset browser: the types at the left (KNOB 1, ALGORITHM), the chain at the
 * right (KNOB 2, SELECT: its modules, then the place after the last while there is room); KNOB 3 moves the module
 * picked along the chain, KNOB 4 sets its MIX. FX + OCT+ adds a module of the type picked below the slot picked (at
 * the place after the last: there), from its defaults; FX + OCT- takes the module picked out (its matrix slots with
 * it). FX twice opens the module picked (its pages: SELECT the modules in the chain's order, ALGORITHM its pages);
 * FX twice there, back to the chain, that module picked. Undo takes any of it back */
static void ui_edit(void);
static void ui_edit_step(void);
static void ui_toast(const char *s);
static uint32_t ui_cn(void) { return sp_chain_n(sy_patch.p); }
static uint32_t ui_cslot(void)                       /* the slot picked, kept in range */
{
    uint32_t n = ui_cn(), top = n < NFXC ? n : NFXC - 1u;
    return ui.cslot > top ? top : ui.cslot;
}
static int32_t ui_cinst(void) { return sp_chain_at(sy_patch.p, ui_cslot()); }   /* its module, -1 the empty place */
static int32_t ui_cmix_of(int32_t i)                 /* instance i's MIX (OTT: DEPTH; TREMOLO: DEPTH; EQ: none) */
{
    static const int16_t MIX[FXT_N] = {SP_DR_MIX, SP_CH_MIX, SP_DL_MIX, SP_RV_MIX, SP_OT_DEPTH, -1, SP_FL_MIX,
                                       SP_PH_MIX, SP_FF_MIX, SP_CR_MIX, SP_TR_DEPTH, SP_RG_MIX, SP_TP_MIX};
    if (i < 0 || MIX[FXI_TYPE[i]] < 0)
        return -1;
    return MIX[FXI_TYPE[i]] + (int32_t)FXI_OFF((uint32_t)i);
}
static int32_t ui_cmix(void) { return ui_cmix_of(ui_cinst()); }   /* .. the slot picked's */
/* the type's instance free to add (-1: both in the chain, or its one) */
static int32_t ui_cfree(uint32_t t)
{
    uint32_t k;
    for (k = 0; k < 2u; k++)
        if (FXT_INST[t][k] != 0xFF && sp_chain_find(sy_patch.p, FXT_INST[t][k]) < 0)
            return FXT_INST[t][k];
    return -1;
}
static void ui_chain_turn(int32_t item, int32_t s)
{
    uint32_t n = ui_cn(), at = ui_cslot();
    if (item == UI_CTYPE) {
        ui.ctype = (uint8_t)((ui.ctype + (uint32_t)(s % (int32_t)FXT_N + (int32_t)FXT_N)) % FXT_N);
    } else if (item == UI_CSLOT) {
        ui.cslot = (uint8_t)sy_clampi((int32_t)at + s, 0, (int32_t)(n < NFXC ? n : NFXC - 1u));
    } else if (item == UI_CMOVE) {                  /* the module picked, past its neighbours one a detent */
        int32_t to = sy_clampi((int32_t)at + s, 0, (int32_t)n - 1);
        if (at >= n)
            return;
        if (to == (int32_t)at) {
            ui_toast(at ? "LAST IN THE CHAIN" : "FIRST IN THE CHAIN");
            return;
        }
        ui_edit();
        while ((int32_t)at != to) {
            uint32_t nb = to > (int32_t)at ? at + 1u : at - 1u;
            int16_t x = sy_patch.p[SP_FXC1 + nb];
            sy_patch.p[SP_FXC1 + nb] = sy_patch.p[SP_FXC1 + at];
            sy_patch.p[SP_FXC1 + at] = x;
            at = nb;
        }
        ui.cslot = (uint8_t)at;
    }
    ui.dirty |= D_DISP | D_KNOBS | D_HEAD;
}
/* FX + OCT+: a module of the type picked, below the slot picked (on the empty place: there), from its defaults */
static void ui_chain_add(void)
{
    uint16_t ids[SP_FX_MAXP];
    uint32_t n = ui_cn(), at = ui_cslot(), pos = at < n ? at + 1u : n, m, k;
    int32_t i = ui_cfree(ui.ctype);
    char t[32];
    if (n >= NFXC) {
        ui_toast("THE CHAIN IS FULL: 8 MODULES");
        return;
    }
    if (i < 0) {                                    /* "DRIVE: TWO AT MOST", "DELAY: ONE AT MOST" */
        sp_put(sp_put(t, FXT_NAMES[ui.ctype]), FXT_INST[ui.ctype][1] == 0xFF ? ": ONE AT MOST" : ": TWO AT MOST");
        ui_toast(t);
        return;
    }
    ui_edit_step();
    m = sp_inst_ids((uint32_t)i, ids);              /* (its values first, the chain after: the audio side takes */
    for (k = 0; k < m; k++)                         /* it whole) */
        sy_patch.p[ids[k]] = SP[ids[k]].def;
    sy_patch.p[FXI_ON[i]] = 1;
    for (k = n; k > pos; k--)
        sy_patch.p[SP_FXC1 + k] = sy_patch.p[SP_FXC1 + k - 1u];
    sy_patch.p[SP_FXC1 + pos] = (int16_t)(i + 1);
    ui.cslot = (uint8_t)pos;
    sp_put(sp_fix(sp_put(sp_put(sp_put(t, "ADDED: "), SEC_NAMES[SEC_DRIVE + (uint32_t)i]), " \xB7 "), (int32_t)pos + 1,
                  0u, 0), " OF 8");
    ui_toast(t);
    ui.dirty |= D_DISP | D_KNOBS | D_HEAD;
}
/* FX + OCT-: the module picked out, the matrix slots onto it freed */
static void ui_chain_del(void)
{
    uint32_t n = ui_cn(), at = ui_cslot(), k;
    int32_t i = ui_cinst();
    char t[32];
    if (i < 0) {
        ui_toast(n ? "PICK A MODULE (KNOB 2)" : "THE CHAIN IS EMPTY");
        return;
    }
    ui_edit_step();
    for (k = 0; k < NSLOT; k++)
        if (sp_inst_of((uint32_t)sy_patch.p[SP_M1_DST + 3u * k]) == i) {
            sy_patch.p[SP_M1_SRC + 3u * k] = SRC_NONE;
            sy_patch.p[SP_M1_DST + 3u * k] = 0;
            sy_patch.p[SP_M1_AMT + 3u * k] = 0;
        }
    for (k = at; k + 1u < NFXC; k++)
        sy_patch.p[SP_FXC1 + k] = sy_patch.p[SP_FXC1 + k + 1u];
    sy_patch.p[SP_FXC8] = 0;
    sp_put(sp_put(t, "TAKEN OUT: "), SEC_NAMES[SEC_DRIVE + (uint32_t)i]);
    ui_toast(t);
    ui.dirty |= D_DISP | D_KNOBS | D_HEAD | D_FOOT;
}
/* FX twice: the chain view <-> the module picked (the page it showed last) */
static void ui_chain_flip(void)
{
    if (ui_sec() == SEC_CHAIN) {
        int32_t i = ui_cinst();
        uint32_t sec;
        if (i < 0) {
            ui_toast(ui_cn() ? "FX + OCT+ ADDS ONE HERE" : "THE CHAIN IS EMPTY: FX + OCT+");
            return;
        }
        sec = SEC_DRIVE + (uint32_t)i;
        ui_go(UI_PG[ui.spage[sec]].sec == sec ? ui.spage[sec] : ui_first_page(sec));
    } else if (ui_fx_inst() >= 0) {
        int32_t k = sp_chain_find(sy_patch.p, (uint32_t)ui_fx_inst());
        if (k >= 0)
            ui.cslot = (uint8_t)k;
        ui_go(ui_first_page(SEC_CHAIN));
    }
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
static sy_patch_t ui_hist[UI_NHIST] SY_SPARE;       /* (7 KB: in strata.ld's spare SRAM since V0.66) */
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
    ui.undel = 0;                                   /* (a delete is put back only straight after it) */
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
static void ui_edit_step(void)                      /* .. an edit a step of its own (a module added, taken out, */
{                                                   /* bypassed: each undone alone) */
    ui.edit_t = 0;
    ui_edit();
    ui.edit_t = 0;
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
 * it picked), HOME opens the SETTINGS, SAVE writes the sound over its own slot. From HOLD a countdown screen;
 * let go, a knob, another button, OCT (its pages, or SAVE's undo and redo: a chord), another page or the MOD layer
 * stop it */
#define UI_LONG_MS 5000u
#define UI_CD_SHOW_MS 2000u                         /* a menu button's countdown shows from here (HOME's, SAVE's from */
                                                    /* HOLD) */
static void ui_reset(uint32_t b);
static void ui_mod_close(void);
static void ui_lay_close(void);
static int ui_cd_quiet(uint32_t b) { return b != B_HOME && b != B_SAVE && b != B_OCTDN && b != NB; }
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
    ui.dirty |= D_DISP;                             /* (the countdown's screen: the page back) */
}
static void ui_lfo_del(void)
{
    uint32_t k = ui_lfo(), i, j;
    sp_draw_t t, *w;
    const sp_draw_t *d = ui_pts(k, &t);
    char s[32];
    i = ui_lpt(k, d);
    if (i == 0u || i + 1u >= d->n) {
        ui_toast("THE END POINTS STAY");
        return;
    }
    if (sy_next_st)
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
#define UI_DEL_MS 3000u                             /* OCT- held in the browser: the preset playing deleted */
static void ui_browse_del(void);                    /* sy_ui_save.c */
static void ui_cd_tick(void)
{
    uint32_t b = ui.cd_btn, held = fm1_ms - ui.bt_t[b], left, dur = b == B_OCTDN ? UI_DEL_MS : UI_LONG_MS;
    if (!(ui.bt_down & (1u << b)) || (ui.bt_chord & (1u << b)) || (ui.mod && (ui.mod_btn != b || ui.mod_last >= 0)) ||
        ui.mom_n || (b == B_SAVE && sy_app_slot() < 0) ||
        (b == B_OCTDN && (ui_sec() != SEC_BROWSE || sy_app_slot() < 0))) {
        ui_cd_stop();
        return;
    }
    if (held >= dur) {
        ui.cd_btn = NB;
        ui.dirty |= D_DISP;
        if (b == B_SAVE) {
            ui_save_over();
        } else if (b == B_OCTDN) {
            ui_browse_del();
        } else if (b == B_HOME) {
            ui_go(ui_first_page(SEC_MENU));
        } else {                                    /* (a menu button: its layer closes, what it shows reset) */
            if (ui.mod && ui.mod_btn == b)
                ui_mod_close();
            if (ui.lay && ui.lay_btn == b)
                ui_lay_close();
            ui_reset(b);
        }
        return;
    }
    left = (dur - held + 999u) / 1000u;             /* (sy_ui_draw.c: the countdown's screen, its ring live) */
    ui.cd_s = (uint8_t)left;
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

/* the SETTINGS' items besides COLOR and ACCEL: HELP, GLOW, HOLD, MIDI's CHANNEL, LEARN, CLEAR (turned twice within
 * 2 s), PROGRAM */
static void ui_set_turn(int32_t item, int32_t s)
{
    if (item == UI_HELP) {
        ui.help = s > 0;
    } else if (item == UI_GLOW) {
        ui.glow = s > 0;
        fm1_led_dim_level(ui.glow);
    } else if (item == UI_HOLD) {
        settings_hold = (uint8_t)sy_clampi(settings_hold + s, 0, 3);
    } else if (item == UI_MCH) {
        ui.mch = (uint8_t)sy_clampi(ui.mch + s, 0, 16);
    } else if (item == UI_LEARN) {
        ui.learn = s > 0;
        ui.ml_pick = -1;
        ui_toast(ui.learn ? "LEARN: TURN A KNOB" : "LEARN OFF");
    } else if (item == UI_MCLR) {
        if (ui.clr_t && fm1_ms - ui.clr_t < 2000u) {
            memset((void *)ml_cc, 0xFF, sizeof ml_cc);
            ui.clr_t = 0;
            ui_toast("THE LEARNED CCS CLEARED");
        } else {
            ui.clr_t = fm1_ms | 1u;
            ui_toast(ml_count() ? "TURN AGAIN: ALL CLEARED" : "NONE LEARNED");
        }
    } else {
        ui.mpc = s > 0;
    }
    ui.dirty |= D_KNOBS | D_DISP | D_FOOT;
}

/* BENCH: RUN (the platform's main loop times its loops, the sound hushed a moment: sy_app.c sy_bench), CLEAR the
 * peaks (the longest half, frame, wait, build step; the halves late) */
static void ui_bench_turn(int32_t item)
{
    if (item == UI_BRUN) {
        if (!sy_bench.known) {
            ui_toast("RUN TIMES THE FM-1 ONLY");
        } else if (sy_bench.run != 1u) {
            sy_bench.run = 1;
            ui_toast("RUNNING: A MOMENT'S SILENCE");
        }
    } else {
        sy_bench.half_max_us = sy_bench.frame_max_us = sy_bench.gap_max_us = sy_bench.step_max_us = 0;
        sy_bench.gap_save_max_us = sy_bench.t5_gap_max_us = 0;
        sy_bench.late = sy_bench.late_held = 0;
        ui_toast("THE PEAKS CLEARED");
    }
    ui.dirty |= D_KNOBS | D_DISP | D_FOOT;
}

/* every millisecond: a CC MIDI LEARN heard, set to the parameter picked ("CC 74: FLT CUTOFF") */
static void ui_learn_ms(void)
{
    uint32_t cc, i;
    char t[32];
    if (!ml_heard)
        return;
    cc = ml_heard - 1u;
    ml_heard = 0;
    if (!ui.learn || ui.ml_pick < 0)
        return;
    ml_drop(cc, ui.ml_pick);
    for (i = 0; i < ML_N && ml_cc[i] != 0xFFu; i++)
        ;
    if (i == ML_N) {
        ui_toast("LEARN: 32 AT MOST");
        return;
    }
    ml_id[i] = (uint16_t)ui.ml_pick;               /* (the parameter, then the CC that makes it count) */
    ml_cc[i] = (uint8_t)cc;
    sp_put(sp_put(sp_put(sp_put(sp_fix(sp_put(t, "CC "), (int32_t)cc, 0u, 0), ": "), SEC_SHORT[SP[ui.ml_pick].sec]),
                  " "), SP[ui.ml_pick].name);
    ui_toast(t);
    ui.dirty |= D_DISP | D_FOOT;
}

/* the items: the MENU's; an LFO's points and GLOBAL's tempo (not in the MOD layer: nothing there to modulate) */
/* SCL's chords (V0.7, sy_chord.c chd): a turn of CHORD's, PERFORM's (the style shown's own), BASS's or MIDI's knobs;
 * the audio side takes them at its next block (chd.gen: the chord made again). V0.75, ARP's: PERFORM's RATE, MOVE
 * and FEEL, GROOVE's, a step's (the one picked) taken as they stand, from the next step (the arpeggio goes on) */
static void ui_chord_turn(int32_t item, int32_t s)
{
    uint32_t st = chd.style % PF_N, a = ui.astep % AR_NSTEP;
    int32_t lo, hi, step, nm = (int32_t)ch_moves(st);
    int was = sy_chord_active();
    if ((item >= UI_PRATE && item <= UI_PFEEL) || item >= UI_AGATE) {
        switch (item) {
        case UI_PRATE: chd.rate[st] = (uint8_t)sy_clampi(chd.rate[st] + s, 0, CH_NRATE - 1); break;
        case UI_PMOVE: chd.move[st] = (uint8_t)sy_clampi(chd.move[st] + s, 0, nm - 1); break;
        case UI_PFEEL:
            ch_feel_range(st, &lo, &hi, &step);
            chd.feel[st] = (uint8_t)sy_clampi(chd.feel[st] + s * step, lo, hi);
            break;
        case UI_AGATE: chd.gate = (uint8_t)sy_clampi(chd.gate + 5 * s, 5, 200); break;
        case UI_ASWING: chd.swing = (uint8_t)sy_clampi(chd.swing + s, 50, 75); break;
        case UI_ALEN: chd.len = (uint8_t)sy_clampi(chd.len + s, 1, AR_NSTEP); break;
        case UI_ALATCH:
            chd.latch = s > 0;
            if (!was && sy_chord_active())
                sy_app_keys_hand();             /* (the keys held: latched from now) */
            break;
        case UI_SSTEP: ui.astep = (uint8_t)((a + (uint32_t)(s % 16 + 16)) % AR_NSTEP); break;
        case UI_STYPE: chd.st[a] = (uint8_t)sy_clampi(chd.st[a] + s, 0, AS_N - 1); break;
        case UI_SLVL: chd.lvl[a] = (uint8_t)sy_clampi(chd.lvl[a] + 5 * s, 5, 100); break;
        case UI_SOCT: chd.oct[a] = (uint8_t)sy_clampi(chd.oct[a] + s, 0, 4); break;
        case UI_SRAT: chd.rat[a] = (uint8_t)sy_clampi(chd.rat[a] + s, 1, 4); break;
        case UI_SCHN: chd.chn[a] = (uint8_t)sy_clampi(chd.chn[a] + 5 * s, 0, 100); break;
        case UI_SSLD: chd.sld[a] = s > 0; break;
        case UI_STRUM: chd.strum = (uint8_t)sy_clampi(chd.strum + s, 0, CH_NSTRUM - 1); break;
        case UI_SDIR: chd.sdir = (uint8_t)sy_clampi(chd.sdir + s, 0, CH_NSDIR - 1); break;
        case UI_SHUM: chd.human = (uint8_t)sy_clampi(chd.human + 5 * s, 0, 100); break;
        default: chd.taper = (uint8_t)sy_clampi(chd.taper + 5 * s, 0, 100); break;
        }
        ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
        return;
    }
    switch (item) {
    case UI_HMODE:
        chd.mode = (uint8_t)sy_clampi(chd.mode + s, 0, CH_NMODE - 1);
        if (chd.mode != CH_OFF)
            ui.lastmode = chd.mode;
        break;
    case UI_HTYPE:                                  /* (KEY's SHAPE, FREE's TYPE: each its own) */
        if (chd.mode == CH_KEY)
            chd.ktype = (uint8_t)sy_clampi(chd.ktype + s, 0, CH_KT_N - 1);
        else
            chd.ftype = (uint8_t)sy_clampi(chd.ftype + s, 0, CH_FT_N - 1);
        break;
    case UI_HVOI: chd.voicing = (uint8_t)sy_clampi(chd.voicing + s, 0, CH_NVOIC - 1); break;
    case UI_HSPR: chd.spread = (uint8_t)sy_clampi(chd.spread + s, 0, CH_NSPREAD - 1); break;
    case UI_PSTY: chd.style = (uint8_t)sy_clampi(st + s, 0, PF_N - 1); break;
    case UI_BON: chd.bass = s > 0; break;
    case UI_BOCT: chd.boct = (uint8_t)sy_clampi(chd.boct + s, 1, 2); break;
    case UI_BLVL: chd.blvl = (uint8_t)sy_clampi(chd.blvl + 5 * s, 5, 100); break;
    case UI_CIN: chd.min = s > 0; break;
    case UI_COUT: chd.mout = s > 0; break;
    case UI_COCH: chd.och = (uint8_t)sy_clampi(chd.och + s, 0, 15); break;
    default: chd.bch = (uint8_t)sy_clampi(chd.bch + s, 0, 15); break;
    }
    if (!was && sy_chord_active())
        sy_app_keys_hand();                         /* (V0.75: the keys held go over to the chords) */
    chd.gen++;
    ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
}

static void ui_seq_turn(int32_t item, int32_t s, uint32_t e);   /* sy_ui_seq.c (V0.8) */
static int ui_seq_key(uint32_t k);
static void ui_item_turn(int32_t item, int32_t s, uint32_t e)
{
    if (item == UI_COLOR) {
        ui.pal = (uint8_t)((ui.pal + (uint32_t)(s % (int32_t)SY_NPAL + (int32_t)SY_NPAL)) % SY_NPAL);
        sy_palette(ui.pal);
        ui.dirty = D_ALL;
    } else if (item == UI_ACCEL) {
        ui.accel = s > 0;
        ui.dirty |= D_KNOBS | D_DISP;
    } else if (item >= UI_HELP && item <= UI_MPC) {   /* the SETTINGS' others */
        ui_set_turn(item, s);
    } else if (item == UI_BRUN || item == UI_BCLR) {   /* BENCH's */
        ui_bench_turn(item);
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
    } else if (item >= UI_CTYPE && item <= UI_CMOVE) {   /* the FX chain view's */
        ui_chain_turn(item, s);
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
    } else if (item >= UI_HMODE && item <= UI_STAP) {   /* SCL's chords, ARP's */
        ui_chord_turn(item, s);
    } else if (item >= UI_QSTEP && item <= UI_QCHAIN) {   /* the sequencer's (V0.8: sy_ui_seq.c) */
        ui_seq_turn(item, s, e);
    } else if (item == UI_CLOCK) {                  /* GLOBAL's CLOCK IN: the panel's tempo, MIDI's clock */
        chd.clock = s > 0;
        ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
    } else if (item >= UI_BPM && item <= UI_TSN) {
        if (item == UI_BPM && sy_app_clock()) {
            ui_toast("THE TEMPO IS MIDI'S CLOCK");
            return;
        }
        if (item == UI_BPM)
            ui.bpm = (uint16_t)sy_clampi(ui.bpm + ui_accel(e, s, 280), 20, 300);
        else if (item == UI_TSB)
            ui.tsb = (uint8_t)sy_clampi(ui.tsb + s, 1, 16);
        else
            ui.tsn = (uint8_t)sy_clampi(ui.tsn + s, 0, 4);
        if (sy_app_clock())                         /* (MIDI's tempo stays; the bar as the signature says) */
            sy_bar_q = (float)ui.tsb * 4.0f / (float)UI_TSN_VAL[ui.tsn];
        else
            sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
        ui.dirty |= D_KNOBS | D_DISP;
    } else {
        ui_lfo_turn(item, s, e);
    }
}

/* the parameter a knob sets: an LFO's RATE or the delay's TIME with SYNC FIXED, its note value (the MOD layer: RATE
 * and TIME themselves, whose modulation steps it) */
static const uint16_t UI_SYNCED[6][3] = {         /* a rate (a time), its SYNC, its NOTE (an effect's: its first's) */
    {SP_L1_RATE, SP_L1_SYNC, SP_L1_NOTE}, {SP_L2_RATE, SP_L2_SYNC, SP_L2_NOTE}, {SP_DL_TIME, SP_DL_SYNC, SP_DL_NOTE},
    {SP_FL_RATE, SP_FL_SYNC, SP_FL_NOTE}, {SP_PH_RATE, SP_PH_SYNC, SP_PH_NOTE}, {SP_TR_RATE, SP_TR_SYNC, SP_TR_NOTE}};
static int32_t ui_eff(int32_t id)
{
    uint32_t b, o, k;
    if (ui.mod || id < 0 || id >= (int32_t)SP_COUNT)
        return id;
    b = sp_fx_base((uint32_t)id);
    o = (uint32_t)id - b;
    for (k = 0; k < 6u; k++)
        if (b == UI_SYNCED[k][0] && sy_patch.p[UI_SYNCED[k][1] + o])
            return (int32_t)(UI_SYNCED[k][2] + o);
    return id;
}
static int ui_is_sync(uint32_t id)                  /* a SYNC (its rate's knob shows Hz or a note value) */
{
    uint32_t b = sp_fx_base(id), k;
    for (k = 0; k < 6u; k++)
        if (b == UI_SYNCED[k][1])
            return 1;
    return 0;
}

/* the knob turning: its cell lit, and what it moves on the graph, until UI_GESTURE_MS pass without a detent
 * (sy_ui_draw.c sy_ui_frame puts them back); the parameter it sets (its note value with SYNC FIXED) or item, -1 none */
static void ui_act(uint32_t k)
{
    if (ui.act_k < 4u && ui.act_k != k)
        ui.dirty |= D_KNOB(ui.act_k);
    ui.act_k = (uint8_t)k;
    ui.act_t = fm1_ms;
    ui.dirty |= D_KNOB(k) | D_DISP | D_FOOT;       /* (V0.75: its value in the bottom row) */
}
static int32_t ui_act_id(void)
{
    int32_t id;
    if (ui.act_k >= 4u || ui.mod || ui.lay)
        return -1;
    id = ui_virt(UI_PG[ui.page].id[ui.act_k]);
    return id >= 0 && id < UI_ITEM ? ui_eff(id) : id;
}
/* the patch as it was before the turn (the gesture's first state on the history), while a knob turns; 0 none */
static const sy_patch_t *ui_ghost(void)
{
    if (ui.act_k >= 4u || ui.mod || ui.lay || !ui.edit_t || fm1_ms - ui.edit_t > UI_GESTURE_MS || !ui.h_at)
        return 0;
    return ui_hist_at(ui.h_at - 1u);
}

/* the hold layers besides MOD: open while their button is held; let go, PERFORM's bend springs back to the middle
 * and MOMENTARY puts back what it changed */
static void ui_lay_open(uint32_t lay, uint32_t b)
{
    ui.lay = (uint8_t)lay;
    ui.lay_btn = (uint8_t)b;
    ui.mom_n = 0;
    ui.dirty = D_ALL;
}
static void ui_lay_close(void)
{
    uint32_t i;
    if (ui.lay == LAY_PERF && ui.bend) {
        ui.bend = 0;
        sy_app_bend(8192u);
    }
    for (i = 0; i < ui.mom_n; i++)
        sy_patch.p[ui.mom_id[i]] = ui.mom_v[i];
    ui.mom_n = 0;
    ui.lay = LAY_NONE;
    ui.lay_lock = 0;
    ui.dirty = D_ALL;
}
/* PERFORM's and MACROS' knobs: the bend (16 detents from the middle to an end), the mod wheel, GLIDE, VOLUME;
 * MACRO 1-4 (an edit: undo takes it back) */
static void ui_lay_knob(uint32_t k, int32_t s, uint32_t e)
{
    if (ui.lay == LAY_PERF && k == 0u) {
        ui.bend = (int16_t)sy_clampi(ui.bend + s * 512, -8192, 8191);
        sy_app_bend((uint32_t)(ui.bend + 8192));
    } else if (ui.lay == LAY_PERF && k == 1u) {
        ui.wheel = (uint8_t)sy_clampi(ui.wheel + ui_accel(e, s, 127), 0, 127);
        sy_app_cc(1u, ui.wheel);
    } else {
        uint32_t id = ui.lay == LAY_MAC ? SP_MAC1 + k : k == 2u ? SP_V_GLIDE : SP_V_VOL;
        ui_edit();
        sy_patch.p[id] = sp_clamp(id, sy_patch.p[id] + ui_accel(e, s, SP[id].hi - SP[id].lo));
    }
    ui.dirty |= D_KNOB(k) | D_DISP;
}
/* MOMENTARY: parameter id as it was before the layer's first change to it, kept to put back (1: room for it) */
static int ui_mom_keep(uint32_t id)
{
    uint32_t i;
    for (i = 0; i < ui.mom_n; i++)
        if (ui.mom_id[i] == id)
            return 1;
    if (ui.mom_n >= 16u) {
        ui_toast("MOMENTARY: 16 AT MOST");
        return 0;
    }
    ui.mom_id[ui.mom_n] = (uint16_t)id;
    ui.mom_v[ui.mom_n++] = sy_patch.p[id];
    ui.dirty |= D_HEAD;                             /* (the banner counts them) */
    return 1;
}

static void ui_knob(uint32_t k, int32_t s)
{
    int32_t id = ui_virt(UI_PG[ui.page].id[k]);     /* (the EQ's POINTS: the picked point's own) */
    const sp_desc_t *d;
    ui_safe_end();
    ui_cd_stop();
    if (ui.lay == LAY_PERF || ui.lay == LAY_MAC) {
        ui_lay_knob(k, s, EN_K1 + k);
        return;
    }
    if (ui.lay == LAY_JUMP || ui.lay == LAY_PAT)    /* (the map covers the page: nothing changes unseen) */
        return;
    if (id < 0)
        return;
    if (!ui.mod && !ui.lay)
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
    if (ui.learn && ui.ml_pick != id) {             /* (MIDI LEARN: this knob's parameter picked, the bottom row */
        ui.ml_pick = (int16_t)id;                   /* saying so at once) */
        ui.toast[0] = 0;
        ui.dirty |= D_FOOT | D_DISP;
    }
    if (d->unit == U_DST) {                         /* a destination: the next parameter that can be modulated (an */
        int32_t v = sy_patch.p[id], dir = s > 0 ? 1 : -1, n = s > 0 ? s : -s;   /* effect's: of a module in the chain) */
        while (n--) {
            int32_t w = v;
            do
                w += dir;
            while (w > 0 && w < (int32_t)SP_COUNT &&
                   (!(SP[w].flags & F_MOD) ||
                    (sp_inst_of((uint32_t)w) >= 0 && sp_chain_find(sy_patch.p, (uint32_t)sp_inst_of((uint32_t)w)) < 0)));
            if (w < 0 || w >= (int32_t)SP_COUNT)
                break;
            v = w;
        }
        s = v - sy_patch.p[id];
    } else if (!ui_list((uint32_t)id)) {            /* (VALUES: the finest unit a detent, at any speed) */
        s = UI_PG[ui.page].graph == G_VALUES ? s * (d->hi - d->lo >= 2000 ? (d->hi - d->lo) / 1000 : 1)
                                             : ui_accel(EN_K1 + k, s, d->hi - d->lo);
    }
    if (ui.lay == LAY_MOM) {                        /* (MOMENTARY: kept to put back; no undo of its own) */
        if (!ui_mom_keep((uint32_t)id))
            return;
    } else {
        ui_edit();
    }
    sy_patch.p[id] = sp_clamp((uint32_t)id, sy_patch.p[id] + s);
    if (ui.lay != LAY_MOM) {                        /* (V0.8: its lane in the pattern playing let go while it turns; */
        uint32_t l;                                 /* REC armed and playing, written at the step playing) */
        if (!sy_seq_knob((uint32_t)id, &l))
            ui_toast("MOTION: BOTH LANES IN USE");
        else if (sqd.ltouch)
            ui.qtouch_t = fm1_ms;
    }
    ui.dirty |= D_KNOB(k) | D_DISP;                 /* (the page's body: sy_ui_draw.c redraws rows 16 .. 221) */
    if (ui_sec() == SEC_LFO1 || ui_sec() == SEC_LFO2 || ui_sec() == SEC_MATRIX)
        ui.dirty |= D_HEAD;                         /* (their top line: an LFO's rate and mode, the slots in use) */
}

/* a parameter changed from elsewhere (MIDI, a sound): what shows it */
static void sy_ui_touch(void) { ui.dirty |= D_KNOBS | D_DISP | D_HEAD | D_FOOT; }

/* ---------------------------------------------------------------------------------------- buttons --- */
#define UI_CHORD ((1u << B_HOME) | (1u << B_EDIT) | (1u << B_ENV) | (1u << B_LFO) | (1u << B_FX) | (1u << B_GLO) | \
                  (1u << B_SCL) | (1u << B_ARP) | (1u << B_REC) | (1u << B_SEQ))   /* held with OCT- / OCT+: ui_combo */
static void ui_browse_undel(void);                  /* sy_ui_save.c */
static int ui_src_sec(uint32_t s)                   /* a section EDIT + OCT turns on and off */
{
    return s == SEC_OSCA || s == SEC_OSCB || s == SEC_SUB || s == SEC_NOISE || s == SEC_FILTER;
}
static uint32_t ui_btn_fam(uint32_t b)              /* the menu a button opens (FAM_N: none) */
{
    uint32_t f;
    for (f = 0; f < FAM_N && (FAM_BTN[f] != b || b == NB); f++)
        ;
    return f;
}
/* V0.8: pattern n picked (SEQ + OCT, PATTERNS, PATTERN's knob): playing, from the end of the one playing; else now */
static void ui_seq_pick(uint32_t n)
{
    char t[24];
    n %= SQ_NPAT;
    if (ui.run && sqs.run) {
        sqd.next = n == sqd.cur ? SQ_NONE : (uint8_t)n;
        sp_fix(sp_put(t, sqd.next == SQ_NONE ? "PATTERN " : "NEXT: PATTERN "), (int32_t)n + 1, 0u, 0);
    } else {
        sqd.cur = (uint8_t)n;
        sqd.next = SQ_NONE;
        sp_fix(sp_put(t, "PATTERN "), (int32_t)n + 1, 0u, 0);
    }
    ui_toast(t);
    ui.dirty |= D_DISP | D_HEAD | D_KNOBS;
}

/* the section menu f shows: the one shown, else where it was left (else its first) */
static uint32_t ui_menu_sec(uint32_t f)
{
    uint8_t secs[NFXC];
    if (ui.fam == f)
        return ui_sec();
    if (ui_in_fam(ui.fpage[f], f) && ui_page_live(ui.fpage[f]))
        return UI_PG[ui.fpage[f]].sec;
    return ui_fam_secs(f, secs) ? secs[0] : SEC_CHAIN;
}
/* a menu button held with OCT+ (up) or OCT- (V0.75, one grammar: on, add, next / off, take out, back): HOME the next
 * or previous sound; EDIT the source or the filter its menu shows on, off; FX a module added, taken out (the chain
 * view), the module its menu shows in, bypassed; ENV, REC their MOD layer latched, let go; GLO PERFORM's layer
 * latched, let go; LFO a point added, the one picked deleted (its POINTS page shown); SCL the chords on (MODE as it
 * was), off; ARP the ARP on, off. From any page: the page stays (LFO's: its POINTS) */
static void ui_combo(uint32_t b, int up)
{
    char t[32];
    uint32_t sec;
    t[0] = 0;
    if (b == B_HOME) {
        sy_app_sound_step(up ? 1 : -1);
    } else if (b == B_EDIT) {
        sec = ui_menu_sec(FAM_EDIT);
        if (ui_src_sec(sec) && !ui.mod) {
            ui_edit_step();
            sy_patch.p[ui_sec_power(sec)] = (int16_t)up;
            sp_put(sp_put(sp_put(t, ui_sec_name(sec)), ": "), up ? "ON" : "OFF");
        }
    } else if (b == B_FX) {
        if (ui.mod)
            return;
        sec = ui_menu_sec(FAM_FX);
        if (ui_sec() == SEC_CHAIN || sec == SEC_CHAIN) {
            if (ui_sec() != SEC_CHAIN)
                ui_go(ui_first_page(SEC_CHAIN));
            if (up)
                ui_chain_add();
            else
                ui_chain_del();
        } else if (ui_is_fx(sec)) {
            ui_edit_step();
            sy_patch.p[FXI_ON[sec - SEC_DRIVE]] = (int16_t)up;
            sp_put(sp_put(t, SEC_NAMES[sec]), up ? " IN" : " BYPASSED");
        }
    } else if (b == B_ENV || b == B_REC) {
        if (up && ui.mod && ui.mod_btn == b) {
            ui.mod = 2u;
        } else if (up && !ui.mod) {
            ui_mod_open(b, b == B_ENV ? SRC_ENV2 : ui.mod_pick, 1);
        } else if (!up && ui.mod && ui.mod_btn == b) {
            ui_mod_close();
        }
        ui.dirty = D_ALL;
    } else if (b == B_GLO) {
        if (up && !ui.lay && !ui.mod)
            ui_lay_open(LAY_PERF, B_GLO);
        if (up && ui.lay == LAY_PERF)
            ui.lay_lock = 1;
        else if (!up && ui.lay == LAY_PERF)
            ui_lay_close();
        ui.dirty = D_ALL;
    } else if (b == B_LFO) {
        if (ui.mod || ui.lay)
            return;
        sec = ui_menu_sec(FAM_LFO);
        if (UI_PG[ui.page].sec != sec || UI_PG[ui.page].id[0] != UI_LPT)
            ui_go(ui_first_page(sec));               /* (its POINTS page) */
        if (up)
            ui_lfo_add();
        else
            ui_lfo_del();
    } else if (b == B_SEQ) {                        /* (V0.8: the next pattern, the one before; playing, from the */
        uint32_t at = ui.run && sqd.next < SQ_NPAT ? sqd.next : sqd.cur;   /* end of the one playing) */
        ui_seq_pick(at + (up ? 1u : SQ_NPAT - 1u));
        return;
    } else if (b == B_SCL || b == B_ARP) {
        int was = sy_chord_active();
        if (b == B_SCL)
            chd.mode = (uint8_t)(up ? (ui.lastmode % CH_NMODE ? ui.lastmode % CH_NMODE : CH_KEY) : CH_OFF);
        else
            chd.perf = (uint8_t)up;
        if (!was && sy_chord_active())
            sy_app_keys_hand();                     /* (a chord held: it plays the new way at once) */
        chd.gen++;
        if (b == B_SCL)
            sp_put(sp_put(t, "CHORDS "), up ? CH_MODE_NAME[chd.mode % CH_NMODE] : "OFF");
        else
            sp_put(sp_put(t, up ? "ARP ON: " : "ARP OFF"), up ? PF_NAME[chd.style % PF_N] : "");
    }
    if (t[0])
        ui_toast(t);
    ui.dirty |= D_DISP | D_HEAD | D_KNOBS;
}
/* held 5 s (V0.75): what the menu shows back to its starting values: a section of the sound's (its parameters'
 * defaults, its ON as it is; an LFO's points too; SAVE + OCT- undoes it), GLOBAL's tempo, SCL's scale, its chords,
 * their MIDI, the ARP's settings, its steps */
static void ui_reset(uint32_t b)
{
    uint32_t f = ui_btn_fam(b), sec, id;
    char t[32];
    if (f == FAM_N)
        return;
    sec = ui_menu_sec(f);
    if (sec < SEC_COUNT) {
        int32_t pw = ui_sec_power(sec);
        ui_edit_step();
        for (id = 0; id < SP_COUNT; id++)
            if (SP[id].sec == sec && (int32_t)id != pw)
                sy_patch.p[id] = SP[id].def;
        if (sec == SEC_LFO1 || sec == SEC_LFO2) {
            uint32_t k = sec == SEC_LFO2;
            sy_draw_edit();
            sp_shape_pts(0, &sy_patch.draw[k]);
            sy_draw_done();
            ui.lpt[k] = 0;
        }
    } else if (sec == SEC_GLOBAL) {
        ui.bpm = 120;
        ui.tsb = 4;
        ui.tsn = 2;
        chd.clock = 0;
        sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
    } else if (sec == SEC_KEYS) {
        ui.scale = 0;
        ui.root = 0;
        ui.fit = UI_FIT_OFF;
        ui.kvel = 100;
    } else if (sec == SEC_CHORD || sec == SEC_CMID || sec == SEC_ARP) {
        if (sec == SEC_CHORD)
            ch_chord_defaults();
        else if (sec == SEC_CMID)
            ch_midi_defaults();
        else
            ch_arp_defaults();
        chd.gen++;
    } else if (sec == SEC_STEPS) {
        ch_steps_clear();
        ui.astep = 0;
    } else if (sec == SEC_SEQ) {                    /* (V0.8: the pattern's steps, into PATTERNS' copy first: PASTE */
        sq_clear();                                 /* puts them back) */
        sqd.cursor = 0;
        sp_put(sp_fix(sp_put(t, "PATTERN "), (int32_t)sqd.cur % SQ_NPAT + 1, 0u, 0), " CLEARED: PASTE UNDOES");
        ui_toast(t);
        ui.dirty = D_ALL;
        return;
    } else if (sec == SEC_SPAT) {                   /* (its LENGTH, RATE, SWING, DIRECTION, TRANSPOSE; RECORD, */
        sq_pat_t *p = &sq_pat[sqd.cur % SQ_NPAT];   /* QUANTIZE) */
        p->length = 16;
        p->rate = SQ_RATE0;
        p->swing = 50;
        p->dir = SQ_FWD;
        p->transpose = 0;
        sqd.recmode = SQ_OVERDUB;
        sqd.quant = 0;
    } else if (sec == SEC_SCHN) {
        sq_chain_defaults();
        ui.qslot = 0;
    } else {
        return;
    }
    sp_put(sp_put(t, ui_sec_name(sec)), ": RESET");
    ui_toast(t);
    ui.dirty = D_ALL;
}

static void ui_press(uint32_t b)
{
    ui_safe_end();
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
        if (ui.bt_down & (1u << B_SAVE)) {          /* SAVE held: undo, redo (in the browser, a preset just */
            ui.bt_chord |= (uint16_t)((1u << B_SAVE) | (1u << b));   /* deleted: back) */
            if (b == B_OCTDN && ui_sec() == SEC_BROWSE && ui.undel)
                ui_browse_undel();
            else
                ui_undo(b == B_OCTUP);
            return;
        }
        if (held) {                                 /* a menu button held: its OCT+ / OCT- (ui_combo) */
            uint32_t h;
            for (h = 0; h < NB && !((held >> h) & 1u); h++)
                ;
            ui.bt_chord |= (uint16_t)(held | (1u << b));
            ui_combo(h, b == B_OCTUP);
            return;
        }
        if (ui.learn) {                             /* MIDI LEARN: OCT- the picked one's CC away, OCT+ done */
            if (b == B_OCTUP) {
                ui.learn = 0;
                ui_toast("LEARN OFF");
            } else if (ui.ml_pick >= 0) {
                ml_drop(0xFFu, ui.ml_pick);
                ui_toast("ITS CC TAKEN AWAY");
            }
            ui.dirty |= D_FOOT | D_DISP | D_KNOBS;
            return;
        }
        if (b == B_OCTDN && ui_sec() == SEC_BROWSE)
            return;                                 /* (the browser: OCT- held deletes; its tap, at its release) */
        sy_app_octave(b == B_OCTUP ? 1 : -1);
        sp_fix(t + 9, sy_app_oct(), 0u, 1);
        ui_toast(t);
        ui.dirty |= D_HEAD;                         /* (SCL's header: the octave) */
    }
}

static void ui_hold(uint32_t b)
{
    if (ui_sec() == SEC_SAVE || ui_sec() == SEC_BROWSE) {
        if (b == B_HOME)
            ui_cd_arm(B_HOME);
        else if (b == B_OCTDN && ui_sec() == SEC_BROWSE && !(ui.bt_chord & (1u << B_OCTDN)))
            sy_app_slot() >= 0 ? ui_cd_arm(B_OCTDN) : ui_toast("A STARTER SOUND STAYS");   /* (3 s: deleted) */
        return;
    }
    if (b == B_REC && !ui.mod)
        ui_mod_open(B_REC, ui.mod_pick, 0);
    else if (b == B_HOME && !ui.mod)
        ui_cd_arm(B_HOME);                          /* (5 s: the SETTINGS) */
    else if (b == B_SAVE && !ui.mod)
        ui_save_hold();                             /* (5 s: over its own slot) */
    else if (ui_btn_fam(b) < FAM_N && !ui.mod && !ui.lay) {   /* a menu button: its layer, and its 5 s armed */
        if (b == B_ENV)
            ui_mod_open(B_ENV, SRC_ENV2, 0);
        else if (b == B_GLO)
            ui_lay_open(LAY_PERF, b);
        else if (b == B_FX)
            ui_lay_open(LAY_MAC, b);
        else if (b == B_LFO)
            ui_lay_open(LAY_MOM, b);
        else if (b == B_EDIT)
            ui_lay_open(LAY_JUMP, b);
        else if (b == B_SEQ)
            ui_lay_open(LAY_PAT, b);                /* (V0.8: PATTERNS) */
        else if (b == B_SCL)
            ui_toast("SCL HELD: A KEY IS THE ROOT");
        ui_cd_arm(b);
    }
}

static void ui_release(uint32_t b, int held)
{
    uint32_t f;
    if (b == ui.cd_btn)
        ui_cd_stop();                               /* (let go before the end: nothing happens) */
    if (held) {                                     /* a held layer closes (a latched one stays); SAVE held on a */
        if (ui.mod == 1u && ui.mod_btn == b)        /* sound from no user slot, no OCT meanwhile (undo, redo): */
            ui_mod_close();                         /* nothing to save over */
        if (ui.lay && ui.lay_btn == b && !ui.lay_lock)
            ui_lay_close();
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
    if (b == B_PLAY) {                              /* PLAY: the transport on (the FREE LFOs from the beat), off */
        ui.run = (uint8_t)!ui.run;
        ui.run_t = fm1_ms;
        if (ui.run)
            sy_lfo_sync = 1;
        sy_app_run(ui.run);                         /* (the beat: from here, the audio side's) */
        ui_toast(ui.run ? "PLAY" : "STOP");
        return;
    }
    if (b == B_OCTDN || b == B_OCTUP) {             /* (the browser: OCT-'s tap, the octave at its release) */
        if (b == B_OCTDN && ui_sec() == SEC_BROWSE) {
            char t[16] = "KEYS OCT ";
            sy_app_octave(-1);
            sp_fix(t + 9, sy_app_oct(), 0u, 1);
            ui_toast(t);
        }
        return;
    }
    if (b == B_HOME && (ui_sec() == SEC_SAVE || ui_sec() == SEC_BROWSE)) {
        ui.tap_t[b] = fm1_ms;                       /* HOME: NAME IT back to the browser, unsaved; the browser back */
        ui_go(ui_sec() == SEC_SAVE ? ui_first_page(SEC_BROWSE) : ui.sv_ret);
        return;
    }
    if (ui.mod == 2u && b == ui.mod_btn) {          /* a tap lets a latched layer go */
        ui_mod_close();
        ui.tap_t[b] = 0;
        return;
    }
    if (ui.lay && ui.lay_lock && b == ui.lay_btn) {
        ui_lay_close();
        ui.tap_t[b] = 0;
        return;
    }
    if (b == B_REC) {                               /* REC (V0.8): the sequencer armed, or not (held: its MOD layer; */
        if (ui.mod)                                 /* REC + OCT+ latches it) */
            return;
        sqd.arm ^= 1u;
        ui_toast(!sqd.arm ? "REC OFF" : ui.run ? "RECORDING" : "REC ARMED");
        ui.dirty |= D_DISP | D_HEAD;
        return;
    }
    f = ui_btn_fam(b);
    if (f == FAM_N)
        return;
    if (fm1_ms - ui.tap_t[b] < UI_DTAP_MS && ui.fam == f && !ui.mod && !ui.lay) {   /* twice: the next section */
        if (ui_fam_of(UI_PG[ui.prev_page].sec) == f)   /* (the first tap's page step undone) */
            ui_go(ui.prev_page);
        if (f == FAM_FX)
            ui_chain_flip();                        /* (FX: the chain view <-> the module picked) */
        else if (f == FAM_HOME)
            ui_browse_open();                       /* (HOME: the presets) */
        else
            ui_step_section(1);
        ui.tap_t[b] = 0;
        return;
    }
    ui.tap_t[b] = fm1_ms;
    ui.prev_page = ui.page;
    ui_family_tap(f);
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

/* the keys only play (sy_ui_key would take none, and change nothing): no layer, no screen that types, SCL not held
 * (its own state read: the main loop's may be a frame old). sy_main.c reads the keys between a redraw's strips then */
static int sy_ui_keys_plain(void)
{
    return !ui.lay && !ui.mod && ui_sec() != SEC_SAVE && !(fm1_in.buttons & (1u << panel.btn[B_SCL]));
}

/* the keys in REC's MOD layer: a white key picks its source, no key plays; 1 = taken (no note). (ENV's layer:
 * the keys play, to hear what ENV 2 does) */
static int sy_ui_key(uint32_t k)
{
    if (ui_sec() == SEC_SAVE)                       /* (SAVE's screen: the keys type) */
        return ui_save_key(k);
    if (ui.lay == LAY_MAC && k < 27u && UI_KEY_FX[k] >= 0) {   /* MACROS: a module bypassed or back in */
        char t[32];
        int32_t i = sp_chain_at(sy_patch.p, (uint32_t)UI_KEY_FX[k]);
        uint32_t pw;
        if (i < 0) {
            ui_toast("NO MODULE THERE YET");
            return 1;
        }
        pw = FXI_ON[i];
        ui_edit_step();
        sy_patch.p[pw] = !sy_patch.p[pw];
        sp_put(sp_put(sp_put(t, SEC_NAMES[SEC_DRIVE + (uint32_t)i]), " "), sy_patch.p[pw] ? "IN" : "BYPASSED");
        ui_toast(t);
        ui.dirty |= D_HEAD | D_DISP;
        return 1;
    }
    if (ui.lay == LAY_PAT)                          /* PATTERNS (V0.8): a key picks a pattern, COPY, PASTE, CLEAR */
        return ui_seq_key(k);
    if (ui.lay == LAY_JUMP && k < 27u && UI_KEY_JUMP[k] != 0xFF) {   /* JUMP: the key's section (a chain slot's: */
        uint32_t sec = UI_KEY_JUMP[k];                                /* its module) */
        if (sec >= UI_JUMP_FX) {
            int32_t i = sp_chain_at(sy_patch.p, sec - UI_JUMP_FX);
            if (i < 0) {
                ui_toast("NO MODULE THERE YET");
                return 1;
            }
            sec = SEC_DRIVE + (uint32_t)i;
        }
        ui_go(UI_PG[ui.spage[sec]].sec == sec ? ui.spage[sec] : ui_first_page(sec));
        return 1;
    }
    if ((ui.bt_down & (1u << B_SCL)) && !ui.mod) {  /* SCL held: a key is the root (no note; SCL's tap none); while */
        char t[24];                                 /* HARP plays, the harp's chord's (V0.7) */
        ui.bt_chord |= (uint16_t)(1u << B_SCL);
        ui_cd_stop();
        ui.root = (uint8_t)((UI_KEY0 + (int32_t)k) % 12);
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
    static uint16_t seq, beat, atseq;
    static uint32_t at_t;
    uint32_t k;
    int32_t s;
    chd.root = ui.root;                             /* (the chords' KEY mode: SCALE's scale, its root; the keys' */
    chd.mask = UI_SCALE_MASK[ui.scale % UI_NSCALE]; /* velocity) */
    chd.vel = (uint8_t)sy_ui_key_vel();
    if (chs.seq != seq) {                           /* (a chord changed: SCL's pages name it, show it) */
        seq = chs.seq;
        if (ui.fam == FAM_SCL || ui.fam == FAM_ARP)
            ui.dirty |= D_DISP | D_HEAD;
    }
    if (chs.beat != beat) {                         /* (ARP's, PATTERN's beat: the ARP LED dark a moment) */
        beat = chs.beat;
        ui.abeat_t = fm1_ms | 1u;
    }
    if (chs.atseq != atseq && ui.fam == FAM_ARP && fm1_ms - at_t >= 40u) {   /* (a step played: ARP's pages */
        atseq = chs.atseq;                                                     /* show where, 25 a second at most) */
        at_t = fm1_ms;
        ui.dirty |= D_DISP;
    }
    {                                               /* V0.8, the sequencer: the keys fill the step picked (stopped, */
        static uint16_t qat, qseq;                  /* armed, SEQ's steps shown); a step played or recorded: SEQ's */
        static uint32_t q_t;                        /* pages show it (25 a second at most); a lane's knob let go */
        sqd.steprec = (uint8_t)(sqd.arm && !ui.run && !sqs.run && ui_sec() == SEC_SEQ && !ui.lay && !ui.mod);
        if ((sqs.atseq != qat || sqs.seq != qseq) && fm1_ms - q_t >= 40u) {
            if (sqs.seq != qseq || ui.fam == FAM_SEQ || ui.lay == LAY_PAT)
                ui.dirty |= D_DISP | D_HEAD | D_KNOBS;
            qat = sqs.atseq;
            qseq = sqs.seq;
            q_t = fm1_ms;
        }
        if (sqd.ltouch && fm1_ms - ui.qtouch_t >= UI_GESTURE_MS)
            sqd.ltouch = 0;
    }
    if (sy_app_clock_lost()) {                      /* (MIDI's clock gone: the panel's tempo again) */
        sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
        ui.dirty |= D_DISP | D_HEAD;
    }
    ui_buttons();
    if (ui.cd_btn != NB)
        ui_cd_tick();
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0)
            ui_knob(k, s);
    if ((s = panel_enc(EN_SELECT)) != 0) {          /* SELECT: the section (the chain view: the slot) */
        ui_cd_stop();
        if (ui_sec() == SEC_CHAIN && !ui.mod && !ui.lay)
            ui_chain_turn(UI_CSLOT, s);
        else
            ui_step_section(s);
    }
    if ((s = panel_enc(EN_ALGO)) != 0) {            /* ALGORITHM: the section's pages (V0.75: everywhere) */
        ui_cd_stop();
        ui_step_pages(s, 1, 0);
    }
    if ((s = panel_enc(EN_PRESET)) != 0)
        sy_app_sound_step(s);
    ui_save_ms();
    ui_learn_ms();
    if (ui.toast[0] && (int32_t)(fm1_ms - ui.toast_t) >= 0) {
        ui.toast[0] = 0;
        ui.dirty |= D_FOOT;
    }
}

/* panel.c's settings hooks: the palette (SETTINGS > COLOR); saved by sy_app.c when they change (sy_ui_settings) */
static void palette_apply(void) { sy_palette(ui.pal); }
static void settings_save(void) {}

/* the settings the flash keeps (sy_app.c, SS_SETTINGS): "SET1", the palette, ACCEL, BPM (2), TS-BEAT, TS-NOTE, the
 * HOLD time; SCL's scale, root, fit and velocity; HELP, GLOW, MIDI's channel and PROGRAM; MIDI LEARN's map (a CC
 * and a parameter, 2 bytes, each); V0.7, the chords' (sy_chord.c chd: CHORD 4, PERFORM 2 + 3 x 6, BASS 3, MIDI 4,
 * KEY's type; V0.75 (none installed before): MODE, FREE's TYPE, KEY's SHAPE, VOICING, the ARP on, its STYLE, its
 * three styles' RATE, MOVE and FEEL, BASS 3, MIDI 4, SPREAD, STRUM 4, SCL + OCT+'s MODE); V0.75, the ARP's (GROOVE's
 * GATE, SWING, LATCH, LENGTH, GLOBAL's CLOCK IN; the steps: a byte each of the kind, SLIDE, OCTAVE and RATCHET, then
 * their LEVELs, their CHANCEs); V0.8, the sequencer's (the pattern shown, CHAIN on, RECORD, QUANTIZE, the chain's
 * slots: their patterns, their repeats).
 * Fields are only added at the end: a shorter record (an older build's, 11 bytes at least) leaves the rest as they
 * were. n 0: written into b, their length; else b's (n bytes) taken: 1 (0: not these) */
#define SS_SET_ML (19u + 3u * ML_N)
#define SS_SET_AR (SS_SET_ML + 32u)
#define SS_SET_SQ (SS_SET_AR + 5u + 3u * AR_NSTEP)
#define SS_SETTINGS_LEN (SS_SET_SQ + 4u + 2u * SQ_NCHAIN)
static uint32_t sy_ui_settings(uint8_t *b, uint32_t n)
{
    if (!n) {
        uint32_t i;
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
        b[15] = ui.help;
        b[16] = ui.glow;
        b[17] = ui.mch;
        b[18] = ui.mpc;
        for (i = 0; i < ML_N; i++) {
            b[19u + 3u * i] = ml_cc[i];
            b[20u + 3u * i] = (uint8_t)ml_id[i];
            b[21u + 3u * i] = (uint8_t)(ml_id[i] >> 8);
        }
        {
            uint8_t *c = b + SS_SET_ML;
            memset(c, 0, 32u);
            c[0] = chd.mode, c[1] = chd.ftype, c[2] = chd.ktype, c[3] = chd.voicing, c[4] = chd.perf, c[5] = chd.style;
            for (i = 0; i < PF_N; i++)
                c[6u + i] = chd.rate[i], c[9u + i] = chd.move[i], c[12u + i] = chd.feel[i];
            c[15] = chd.bass, c[16] = chd.boct, c[17] = chd.blvl;
            c[18] = chd.min, c[19] = chd.mout, c[20] = chd.och, c[21] = chd.bch;
            c[22] = chd.spread, c[23] = chd.strum, c[24] = chd.sdir, c[25] = chd.human, c[26] = chd.taper;
            c[27] = ui.lastmode;
        }
        {
            uint8_t *a = b + SS_SET_AR;
            a[0] = chd.gate, a[1] = chd.swing, a[2] = chd.latch, a[3] = chd.len, a[4] = chd.clock;
            for (i = 0; i < AR_NSTEP; i++) {
                a[5u + i] = (uint8_t)(chd.st[i] | chd.sld[i] << 2 | chd.oct[i] << 3 | (chd.rat[i] - 1u) << 6);
                a[5u + AR_NSTEP + i] = chd.lvl[i];
                a[5u + 2u * AR_NSTEP + i] = chd.chn[i];
            }
        }
        {
            uint8_t *q = b + SS_SET_SQ;
            q[0] = sqd.cur, q[1] = sqd.chain, q[2] = sqd.recmode, q[3] = sqd.quant;
            for (i = 0; i < SQ_NCHAIN; i++)
                q[4u + i] = sqd.cpat[i], q[4u + SQ_NCHAIN + i] = sqd.crep[i];
        }
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
    if (n >= 19u) {
        ui.help = b[15] != 0;
        ui.glow = b[16] != 0;
        ui.mch = (uint8_t)(b[17] <= 16u ? b[17] : 0u);
        ui.mpc = b[18] != 0;
        fm1_led_dim_level(ui.glow);
    }
    if (n >= SS_SET_AR) {                           /* (the chords, each kept in its range) */
        const uint8_t *c = b + SS_SET_ML;
        uint32_t i;
        chd.mode = (uint8_t)(c[0] % CH_NMODE), chd.ftype = (uint8_t)(c[1] % CH_FT_N), chd.ktype = (uint8_t)(c[2] % CH_KT_N);
        chd.voicing = (uint8_t)(c[3] % CH_NVOIC), chd.perf = c[4] != 0, chd.style = (uint8_t)(c[5] % PF_N);
        for (i = 0; i < PF_N; i++) {
            int32_t lo, hi, st;
            ch_feel_range(i, &lo, &hi, &st);
            chd.rate[i] = (uint8_t)(c[6u + i] % CH_NRATE);
            chd.move[i] = (uint8_t)(c[9u + i] % ch_moves(i));
            chd.feel[i] = (uint8_t)sy_clampi(c[12u + i], lo, hi);
        }
        chd.bass = c[15] != 0, chd.boct = (uint8_t)sy_clampi(c[16], 1, 2), chd.blvl = (uint8_t)sy_clampi(c[17], 1, 100);
        chd.min = c[18] != 0, chd.mout = c[19] != 0, chd.och = c[20] & 15u, chd.bch = c[21] & 15u;
        chd.spread = (uint8_t)(c[22] % CH_NSPREAD), chd.strum = (uint8_t)(c[23] % CH_NSTRUM);
        chd.sdir = (uint8_t)(c[24] % CH_NSDIR), chd.human = (uint8_t)sy_clampi(c[25], 0, 100);
        chd.taper = (uint8_t)sy_clampi(c[26], 0, 100);
        ui.lastmode = (uint8_t)(c[27] % CH_NMODE ? c[27] % CH_NMODE : CH_KEY);
        chd.gen++;
    }
    if (n >= SS_SET_SQ) {                           /* (V0.75's ARP) */
        const uint8_t *a = b + SS_SET_AR;
        uint32_t i;
        chd.gate = (uint8_t)sy_clampi(a[0], 5, 200), chd.swing = (uint8_t)sy_clampi(a[1], 50, 75);
        chd.latch = a[2] != 0, chd.len = (uint8_t)sy_clampi(a[3], 1, AR_NSTEP), chd.clock = a[4] != 0;
        for (i = 0; i < AR_NSTEP; i++) {
            uint32_t p = a[5u + i];
            chd.st[i] = (uint8_t)((p & 3u) % AS_N), chd.sld[i] = (p >> 2) & 1u;
            chd.oct[i] = (uint8_t)sy_clampi((int32_t)((p >> 3) & 7u), 0, 4), chd.rat[i] = (uint8_t)((p >> 6) + 1u);
            chd.lvl[i] = (uint8_t)sy_clampi(a[5u + AR_NSTEP + i], 5, 100);
            chd.chn[i] = (uint8_t)sy_clampi(a[5u + 2u * AR_NSTEP + i], 0, 100);
        }
    }
    if (n >= SS_SETTINGS_LEN) {                     /* (V0.8's sequencer) */
        const uint8_t *q = b + SS_SET_SQ;
        uint32_t i;
        sqd.cur = (uint8_t)(q[0] % SQ_NPAT), sqd.chain = q[1] != 0;
        sqd.recmode = (uint8_t)(q[2] % SQ_NRMODE), sqd.quant = q[3] != 0;
        for (i = 0; i < SQ_NCHAIN; i++) {
            sqd.cpat[i] = q[4u + i] < SQ_NPAT ? q[4u + i] : SQ_NONE;
            sqd.crep[i] = (uint8_t)sy_clampi(q[4u + SQ_NCHAIN + i], 1, 16);
        }
    }
    if (n >= SS_SET_ML) {
        uint32_t i;
        for (i = 0; i < ML_N; i++) {
            uint32_t cc = b[19u + 3u * i], id = b[20u + 3u * i] | (uint32_t)b[21u + 3u * i] << 8;
            ml_cc[i] = 0xFFu;
            ml_id[i] = (uint16_t)id;
            if (cc < 128u && !ml_never(cc) && id < SP_COUNT)
                ml_cc[i] = (uint8_t)cc;
        }
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
    ui.help = 1;                                    /* SETTINGS: hints on, any MIDI channel, no program changes */
    ui.ml_pick = -1;
    memset((void *)ml_cc, 0xFF, sizeof ml_cc);
    ch_defaults();                                  /* (V0.7: the chords off) */
    sq_defaults();                                  /* (V0.8: sixteen empty patterns, 16 steps of 1/16 each) */
    ui.lastmode = CH_KEY;
    ui.bpm = 120;                                   /* 120 BPM in 4/4 */
    ui.tsb = 4;
    ui.tsn = 2;
    sy_set_tempo(ui.bpm, ui.tsb, UI_TSN_VAL[ui.tsn]);
    sy_palette(ui.pal);
    ui_go(ui_first_page(SEC_OSCA));
}
