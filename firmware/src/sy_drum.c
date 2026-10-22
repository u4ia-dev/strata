/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* STRATA's drum synth (V0.85; docs/LOOPER.md stage 2, docs/CONTROLS.md "The drums"): the project's kit of eight lanes,
 * each its own model, and the sidechain's duck.
 *   KICK    a sine swept down to its pitch from PUNCH's height; TONE drives it and adds a click
 *   SNARE   two sines (its body) and noise through a high-pass (TONE); SNAP the noise against the body
 *   CLAP    noise through a band-pass at its pitch (TONE: narrower) in four bursts SPREAD apart, then its tail
 *   HAT CL, HAT OP, CYM   six square waves at the TR-808's frequencies and noise (METAL: the squares' share)
 *           through a high-pass (TONE); a HAT CL hit ends a HAT OP still ringing (the choke)
 *   WT 1, WT 2   a one-shot on OSC A's and OSC B's wavetable (TONE: the frame), BEND a fall from up to three octaves
 * A lane is one voice: a hit starts it again. TUNE moves it +-24 semitones; DECAY is its time to -60 dB (each model's
 * range, DK_T60). A hit's gain: LEVEL x its velocity (VELOCITY: how much velocity counts) x ACCENT's lift on an
 * accented hit, then PAN, and its track's VOLUME and PAN (sy_tgain, ramped). The drums add in after the effects
 * (sy_block; V0.9's sends will put them through the rack): the synth's sound is ducked first, the drums never. The DUCK
 * row's hits (silent) start the duck: the synth down by AMOUNT over ATTACK, back over RELEASE along SHAPE. All of it
 * runs in the audio interrupt; the kit (dkk) is the main loop's to edit (bytes, each read whole at a hit).
 * V0.86 (step 2, the drum editor; K3, K5, K6, K9): each drum's DRIVE (SOFT, HARD, FOLD, CRUSH, with its MIX), EQ (LOW
 * and HIGH first-order shelves, MID a band-pass at FREQ) and SHAPE (its start and its body, a gain on its own
 * envelope, in its pan's gains), in that order, each set at the hit and spending nothing at its neutral setting;
 * COMP on the eight together (a peak a control block, 4:1 above AMOUNT's threshold, its MIX in parallel); the duck's
 * SOURCE (its row, the KICK's hits, both) and HOLD; ACCENT's TONE (a soft hit darker, an accented one brighter). Eight
 * factory kits (DK_KITS).
 * Needs sy_core.h, sy_wt.c (wt_tab, wt_frame, wt_level), sy_voice.c (sy_tgain, SY_NOTRK). */

/* --------------------------------------------------------------------------------------------- the kit --- */
enum { DK_KICK, DK_SNARE, DK_CLAP, DK_HATC, DK_HATO, DK_CYM, DK_WT1, DK_WT2, DK_N };
#define DK_DUCK DK_N                               /* the ghost row: the duck's, no sound */
#define DK_ROWS (DK_N + 1u)
static const char *const DK_NAME[DK_ROWS] = {"KICK", "SNARE", "CLAP", "HAT CL", "HAT OP", "CYM", "WT 1", "WT 2",
                                             "DUCK"};
static const char *const DK_CHR[DK_N] = {"PUNCH", "SNAP", "SPREAD", "METAL", "METAL", "METAL", "BEND", "BEND"};
static const uint8_t DK_NOTE[DK_ROWS] = {36, 38, 39, 42, 46, 49, 45, 50, 24};   /* each row's note: GM's drums, DUCK
                                                                                 * C1 (outside them) */
enum { DS_LIN, DS_EXP, DS_PUMP, DS_N };            /* the duck's SHAPE, its way back: even, quick then slow, late */
static const char *const DS_NAME[DS_N] = {"LINEAR", "EXP", "PUMP"};
enum { DD_SOFT, DD_HARD, DD_FOLD, DD_CRUSH, DD_N };  /* V0.86: a drum's DRIVE's TYPE */
static const char *const DD_NAME[DD_N] = {"SOFT", "HARD", "FOLD", "CRUSH"};
enum { DSRC_ROW, DSRC_KICK, DSRC_BOTH, DSRC_N };    /* .. the duck's SOURCE: its row's hits, the KICK's, both */
static const char *const DSRC_NAME[DSRC_N] = {"ROW", "KICK", "BOTH"};

typedef struct {
    int8_t tune;                                   /* TUNE: semitones, -24 .. 24 */
    uint8_t decay, tone, chr;                      /* DECAY, TONE, the model's own (DK_CHR): 0 .. 100 */
    uint8_t level, pan;                            /* LEVEL 0 .. 127; PAN 1 .. 127 (64 the centre) */
    uint8_t rsv[2];
} dk_lane_t;
typedef struct {                                   /* V0.86: a drum's effects (DRIVE, EQ, SHAPE: V0.86 step 2) */
    uint8_t dtype, drive, dmix;                    /* DRIVE: SOFT HARD FOLD CRUSH, its amount, its mix */
    uint8_t lo, mid, hi, freq;                     /* EQ: three gains (50 flat), the middle's place */
    uint8_t att, sus;                              /* SHAPE: the hit's start and its body (50 as played) */
    uint8_t rsv;
} dk_fx_t;
typedef struct {
    dk_lane_t l[DK_N];
    uint8_t accent, vsens;                         /* ACCENT: an accented hit's lift (100: +6 dB); VELOCITY: how much it
                                                    * counts (0: every hit as LEVEL, 100: by its square) */
    uint8_t amount, attack, release, shape;        /* DUCK: AMOUNT %, ATTACK ms (0 .. 50), RELEASE (0 .. 100: 20 ms ..
                                                    * 1 s), SHAPE */
    uint8_t rsv[2];
    /* V0.86 (the project's stream version 3; versions 1 and 2 take these as a kit starts): */
    int8_t ktune;                                  /* HOME's on a drum track: every drum's TUNE moved, -24 .. 24 */
    uint8_t kdecay;                                /* .. every drum's DECAY time x 2^((kdecay - 50) / 50): 0 .. 100 */
    int8_t ktone;                                  /* .. every drum's TONE moved, -50 .. 50 */
    uint8_t dsrc, dhold, atone;                    /* DUCK's SOURCE and HOLD, ACCENT's TONE (step 2) */
    uint8_t comp[4];                               /* the kit's COMP: AMOUNT, ATTACK, RELEASE, MIX (step 2) */
    char name[13];                                 /* the kit's name (HOME shows it; the factory kits': step 2) */
    uint8_t kit;                                   /* .. the factory kit it came from (0xFF none) */
    dk_fx_t fx[DK_N];
    uint8_t rsv3[6];
} dk_kit_t;
#define DK_KIT2 72u                                /* (version 2's kit: the bytes before ktune) */
_Static_assert(sizeof(dk_lane_t) == 8u && sizeof(dk_fx_t) == 10u && sizeof(dk_kit_t) == 182u &&
               __builtin_offsetof(dk_kit_t, ktune) == DK_KIT2, "the kit's bytes (a project's)");
static dk_kit_t dkk;

/* DECAY's range a model, s to -60 dB (0 .. 100: exponential between) */
static const float DK_T60[DK_N][2] = {{0.08f, 2.0f}, {0.06f, 1.0f}, {0.06f, 1.2f}, {0.02f, 0.4f},
                                      {0.15f, 2.0f}, {0.4f, 6.0f},  {0.05f, 3.0f}, {0.05f, 3.0f}};
#define DK_GAIN 0.5f                               /* a lane's peak at LEVEL 127, velocity 127 (-6 dB: room to add) */

static const dk_lane_t DK_DEF[DK_N] = {           /* the lanes as a kit starts */
    {0, 45, 30, 55, 110, 64, {0, 0}}, {0, 40, 45, 55, 100, 64, {0, 0}}, {0, 40, 40, 45, 95, 70, {0, 0}},
    {0, 30, 55, 70, 80, 58, {0, 0}},  {0, 40, 55, 70, 75, 58, {0, 0}},  {0, 50, 45, 55, 70, 72, {0, 0}},
    {0, 45, 0, 40, 95, 54, {0, 0}},   {12, 40, 0, 30, 90, 74, {0, 0}},
};
#define DK_FX0 {DD_SOFT, 0, 100, 50, 50, 50, 50, 50, 50, 0}   /* a drum's effects at rest */
static void dk_fx_rest(dk_fx_t *f) { static const dk_fx_t R = DK_FX0; *f = R; }
static void dk_defaults3(void)                     /* V0.86's: as a kit starts (a version 1 or 2 project's too) */
{
    static const char NM[] = "DRUM KIT";
    uint32_t i;
    dkk.ktune = 0;
    dkk.kdecay = 50;
    dkk.ktone = 0;
    dkk.dsrc = 0;
    dkk.dhold = 0;
    dkk.atone = 0;
    dkk.comp[0] = 0, dkk.comp[1] = 20, dkk.comp[2] = 40, dkk.comp[3] = 100;
    memset(dkk.name, 0, sizeof dkk.name);
    memcpy(dkk.name, NM, sizeof NM);
    dkk.kit = 0xFF;
    for (i = 0; i < DK_N; i++)
        dk_fx_rest(&dkk.fx[i]);
    memset(dkk.rsv3, 0, sizeof dkk.rsv3);
}
static void dk_defaults(void)                      /* the kit as a project starts */
{
    memcpy(dkk.l, DK_DEF, sizeof DK_DEF);
    dkk.accent = 50;
    dkk.vsens = 70;
    dkk.amount = 60;
    dkk.attack = 2;
    dkk.release = 45;
    dkk.shape = DS_EXP;
    dkk.rsv[0] = dkk.rsv[1] = 0;
    dk_defaults3();
}
static void dk_sane(void)                          /* a kit read back: every value in its range */
{
    uint32_t i;
    for (i = 0; i < DK_N; i++) {
        dk_lane_t *k = &dkk.l[i];
        k->tune = (int8_t)sy_clampi(k->tune, -24, 24);
        k->decay = (uint8_t)(k->decay > 100u ? 100u : k->decay);
        k->tone = (uint8_t)(k->tone > 100u ? 100u : k->tone);
        k->chr = (uint8_t)(k->chr > 100u ? 100u : k->chr);
        k->level = (uint8_t)(k->level & 127u);
        k->pan = (uint8_t)sy_clampi(k->pan, 1, 127);
    }
    dkk.accent = (uint8_t)(dkk.accent > 100u ? 100u : dkk.accent);
    dkk.vsens = (uint8_t)(dkk.vsens > 100u ? 100u : dkk.vsens);
    dkk.amount = (uint8_t)(dkk.amount > 100u ? 100u : dkk.amount);
    dkk.attack = (uint8_t)(dkk.attack > 50u ? 50u : dkk.attack);
    dkk.release = (uint8_t)(dkk.release > 100u ? 100u : dkk.release);
    dkk.shape = (uint8_t)(dkk.shape % DS_N);
    dkk.ktune = (int8_t)sy_clampi(dkk.ktune, -24, 24);   /* (V0.86's) */
    dkk.kdecay = (uint8_t)(dkk.kdecay > 100u ? 100u : dkk.kdecay);
    dkk.ktone = (int8_t)sy_clampi(dkk.ktone, -50, 50);
    dkk.dsrc = (uint8_t)(dkk.dsrc % DSRC_N);
    dkk.dhold = (uint8_t)(dkk.dhold > 100u ? 100u : dkk.dhold);
    dkk.atone = (uint8_t)(dkk.atone > 100u ? 100u : dkk.atone);
    for (i = 0; i < 4u; i++)
        dkk.comp[i] = (uint8_t)(dkk.comp[i] > 100u ? 100u : dkk.comp[i]);
    dkk.name[12] = 0;
    for (i = 0; i < 12u; i++)
        if (dkk.name[i] && (dkk.name[i] < 32 || dkk.name[i] > 126))
            dkk.name[i] = ' ';
    for (i = 0; i < DK_N; i++) {
        dk_fx_t *f = &dkk.fx[i];
        f->dtype = (uint8_t)(f->dtype % DD_N);
        f->drive = (uint8_t)(f->drive > 100u ? 100u : f->drive);
        f->dmix = (uint8_t)(f->dmix > 100u ? 100u : f->dmix);
        f->lo = (uint8_t)(f->lo > 100u ? 100u : f->lo);
        f->mid = (uint8_t)(f->mid > 100u ? 100u : f->mid);
        f->hi = (uint8_t)(f->hi > 100u ? 100u : f->hi);
        f->freq = (uint8_t)(f->freq > 100u ? 100u : f->freq);
        f->att = (uint8_t)(f->att > 100u ? 100u : f->att);
        f->sus = (uint8_t)(f->sus > 100u ? 100u : f->sus);
    }
}
/* V0.86 (K8): the eight factory kits, in the image (HOME twice on a drum track lists them; one loaded is the project's
 * kit, edited and kept as any). Their lanes TUNE, DECAY, TONE, own, LEVEL, PAN; ACCENT, VELOCITY; the duck's AMOUNT,
 * ATTACK, RELEASE, SHAPE; SOURCE, HOLD, ACCENT's TONE; COMP; each drum's DRIVE (TYPE, amount, MIX), EQ (LOW, MID,
 * HIGH, FREQ), SHAPE (ATTACK, SUSTAIN) */
#define DK_N8 DK_FX0, DK_FX0, DK_FX0, DK_FX0, DK_FX0, DK_FX0, DK_FX0, DK_FX0
#define DK_NKIT 8u
static const dk_kit_t DK_KITS[DK_NKIT] = {
    {/* CLASSIC: 808-like: a long round kick, a thin snare, the clap's spread */
     .l = {{0, 72, 18, 40, 115, 64, {0, 0}}, {2, 34, 40, 50, 92, 62, {0, 0}}, {0, 46, 45, 58, 92, 70, {0, 0}},
         {0, 24, 62, 78, 76, 56, {0, 0}}, {0, 46, 58, 78, 72, 56, {0, 0}}, {0, 62, 46, 62, 64, 74, {0, 0}},
         {-5, 48, 0, 30, 92, 50, {0, 0}}, {7, 42, 0, 26, 88, 78, {0, 0}}},
     .accent = 50, .vsens = 70, .amount = 50, .attack = 2, .release = 45, .shape = DS_EXP,
     .kdecay = 50, .dsrc = DSRC_ROW, .dhold = 0, .atone = 30, .comp = {0, 20, 40, 100}, .name = "CLASSIC",
     .fx = {DK_N8}},
    {/* PUNCH: 909-like: a short driven kick, a bright snare, the kit pressed together */
     .l = {{0, 42, 62, 66, 112, 64, {0, 0}}, {0, 42, 62, 66, 100, 62, {0, 0}}, {0, 40, 50, 48, 92, 68, {0, 0}},
         {0, 20, 72, 86, 80, 54, {0, 0}}, {0, 40, 66, 86, 74, 54, {0, 0}}, {0, 72, 60, 70, 66, 76, {0, 0}},
         {-3, 40, 0, 44, 90, 50, {0, 0}}, {5, 36, 0, 40, 88, 78, {0, 0}}},
     .accent = 60, .vsens = 70, .amount = 55, .attack = 2, .release = 40, .shape = DS_PUMP,
     .kdecay = 50, .dsrc = DSRC_KICK, .dhold = 10, .atone = 20, .comp = {35, 30, 40, 100}, .name = "PUNCH",
     .fx = {{DD_SOFT, 25, 100, 58, 50, 54, 50, 70, 46, 0}, {DD_SOFT, 15, 60, 50, 56, 56, 60, 62, 50, 0}, DK_FX0, DK_FX0,
         DK_FX0, DK_FX0, DK_FX0, DK_FX0}},
    {/* INDUSTRIAL: hard and folded, the metal bright, the duck deep */
     .l = {{-3, 55, 70, 80, 112, 64, {0, 0}}, {-2, 46, 70, 70, 100, 60, {0, 0}}, {-4, 50, 64, 70, 94, 72, {0, 0}},
         {-2, 26, 82, 100, 78, 50, {0, 0}}, {-2, 50, 78, 100, 72, 50, {0, 0}}, {-5, 70, 70, 90, 66, 80, {0, 0}},
         {-12, 52, 0, 60, 90, 44, {0, 0}}, {-7, 46, 0, 70, 86, 84, {0, 0}}},
     .accent = 70, .vsens = 60, .amount = 70, .attack = 3, .release = 50, .shape = DS_EXP,
     .kdecay = 50, .dsrc = DSRC_BOTH, .dhold = 15, .atone = 50, .comp = {60, 15, 35, 100}, .name = "INDUSTRIAL",
     .fx = {{DD_HARD, 70, 100, 62, 44, 50, 40, 60, 50, 0}, {DD_FOLD, 50, 80, 50, 60, 56, 55, 60, 46, 0},
         {DD_CRUSH, 40, 70, 50, 50, 50, 50, 50, 50, 0}, {DD_HARD, 30, 60, 50, 50, 60, 50, 50, 50, 0}, DK_FX0, DK_FX0,
         {DD_FOLD, 40, 100, 56, 50, 50, 50, 50, 50, 0}, {DD_HARD, 55, 90, 50, 50, 50, 50, 50, 50, 0}}},
    {/* LO-FI: crushed and dull: the highs cut, the lows up */
     .l = {{0, 55, 30, 45, 104, 64, {0, 0}}, {0, 40, 36, 55, 96, 60, {0, 0}}, {0, 44, 36, 50, 90, 70, {0, 0}},
         {0, 26, 46, 70, 74, 56, {0, 0}}, {0, 44, 44, 70, 68, 56, {0, 0}}, {0, 56, 40, 60, 60, 74, {0, 0}},
         {-5, 46, 0, 30, 90, 50, {0, 0}}, {5, 40, 0, 26, 86, 78, {0, 0}}},
     .accent = 40, .vsens = 60, .amount = 45, .attack = 2, .release = 50, .shape = DS_EXP,
     .kdecay = 50, .dsrc = DSRC_ROW, .dhold = 0, .atone = 40, .comp = {30, 30, 50, 100}, .name = "LO-FI",
     .fx = {{DD_CRUSH, 55, 80, 62, 50, 30, 50, 50, 50, 0}, {DD_CRUSH, 70, 100, 56, 54, 26, 50, 50, 50, 0},
         {DD_CRUSH, 70, 100, 50, 54, 26, 50, 50, 50, 0}, {DD_CRUSH, 75, 100, 50, 50, 30, 60, 50, 50, 0},
         {DD_CRUSH, 75, 100, 50, 50, 30, 60, 50, 50, 0}, {DD_CRUSH, 70, 100, 50, 50, 26, 60, 50, 50, 0},
         {DD_CRUSH, 60, 100, 56, 50, 30, 50, 50, 50, 0}, {DD_CRUSH, 60, 100, 56, 50, 30, 50, 50, 50, 0}}},
    {/* MINIMAL: short and clicky: tight starts, little body */
     .l = {{0, 30, 40, 30, 92, 64, {0, 0}}, {3, 26, 54, 60, 64, 58, {0, 0}}, {0, 30, 50, 34, 82, 70, {0, 0}},
         {0, 14, 70, 80, 74, 52, {0, 0}}, {0, 30, 64, 80, 64, 52, {0, 0}}, {0, 40, 56, 64, 48, 78, {0, 0}},
         {0, 30, 0, 20, 84, 48, {0, 0}}, {12, 24, 0, 16, 80, 80, {0, 0}}},
     .accent = 45, .vsens = 75, .amount = 40, .attack = 1, .release = 30, .shape = DS_EXP,
     .kdecay = 50, .dsrc = DSRC_KICK, .dhold = 0, .atone = 20, .comp = {20, 30, 30, 100}, .name = "MINIMAL",
     .fx = {{DD_SOFT, 0, 100, 54, 50, 50, 50, 62, 34, 0}, {DD_SOFT, 0, 100, 50, 50, 54, 50, 58, 38, 0}, DK_FX0,
         {DD_SOFT, 0, 100, 50, 50, 50, 50, 60, 40, 0}, DK_FX0, DK_FX0, DK_FX0, DK_FX0}},
    {/* TRIBAL: the WT lanes as toms, long and low; the clap wide */
     .l = {{-5, 60, 24, 34, 110, 64, {0, 0}}, {-3, 44, 34, 40, 88, 60, {0, 0}}, {-2, 50, 40, 80, 88, 72, {0, 0}},
         {0, 22, 50, 66, 62, 54, {0, 0}}, {0, 42, 50, 66, 56, 54, {0, 0}}, {-2, 64, 40, 56, 54, 76, {0, 0}},
         {-9, 66, 0, 46, 100, 42, {0, 0}}, {-2, 60, 0, 40, 96, 86, {0, 0}}},
     .accent = 55, .vsens = 80, .amount = 35, .attack = 3, .release = 55, .shape = DS_LIN,
     .kdecay = 50, .dsrc = DSRC_ROW, .dhold = 0, .atone = 40, .comp = {25, 25, 45, 100}, .name = "TRIBAL",
     .fx = {DK_FX0, DK_FX0, DK_FX0, DK_FX0, DK_FX0, DK_FX0, {DD_SOFT, 20, 60, 56, 62, 46, 40, 56, 64, 0},
         {DD_SOFT, 20, 60, 56, 62, 46, 46, 56, 64, 0}}},
    {/* GLITCH: high, crushed and folded, short, panned wide */
     .l = {{5, 34, 50, 60, 108, 64, {0, 0}}, {12, 28, 70, 80, 92, 30, {0, 0}}, {7, 30, 64, 20, 86, 100, {0, 0}},
         {5, 12, 84, 100, 76, 20, {0, 0}}, {7, 28, 80, 100, 66, 108, {0, 0}}, {9, 40, 70, 90, 56, 90, {0, 0}},
         {12, 26, 0, 100, 88, 24, {0, 0}}, {19, 22, 0, 100, 84, 104, {0, 0}}},
     .accent = 50, .vsens = 50, .amount = 45, .attack = 1, .release = 25, .shape = DS_PUMP,
     .kdecay = 50, .dsrc = DSRC_BOTH, .dhold = 0, .atone = 0, .comp = {45, 10, 25, 100}, .name = "GLITCH",
     .fx = {{DD_FOLD, 35, 70, 50, 50, 50, 50, 60, 40, 0}, {DD_CRUSH, 80, 100, 50, 56, 50, 70, 50, 50, 0},
         {DD_CRUSH, 85, 100, 50, 50, 56, 60, 50, 50, 0}, DK_FX0, {DD_FOLD, 60, 60, 50, 50, 50, 50, 50, 50, 0},
         {DD_CRUSH, 60, 80, 50, 50, 50, 50, 50, 50, 0}, {DD_FOLD, 70, 100, 50, 50, 50, 50, 50, 50, 0},
         {DD_CRUSH, 90, 100, 50, 50, 50, 50, 50, 50, 0}}},
    {/* SOFT: low, dark and dynamic: soft starts, soft hits darker still */
     .l = {{0, 56, 10, 30, 100, 64, {0, 0}}, {0, 38, 24, 40, 80, 60, {0, 0}}, {0, 44, 26, 50, 76, 70, {0, 0}},
         {0, 22, 34, 60, 62, 56, {0, 0}}, {0, 40, 32, 60, 56, 56, {0, 0}}, {0, 58, 28, 50, 50, 74, {0, 0}},
         {-5, 50, 0, 20, 84, 50, {0, 0}}, {5, 44, 0, 16, 80, 78, {0, 0}}},
     .accent = 35, .vsens = 90, .amount = 30, .attack = 4, .release = 70, .shape = DS_LIN,
     .kdecay = 50, .dsrc = DSRC_ROW, .dhold = 0, .atone = 70, .comp = {0, 20, 40, 100}, .name = "SOFT",
     .fx = {{DD_SOFT, 0, 100, 54, 50, 42, 50, 36, 56, 0}, {DD_SOFT, 0, 100, 50, 50, 40, 50, 38, 54, 0},
         {DD_SOFT, 0, 100, 50, 50, 40, 50, 40, 52, 0}, {DD_SOFT, 0, 100, 50, 50, 40, 50, 40, 50, 0},
         {DD_SOFT, 0, 100, 50, 50, 40, 50, 40, 50, 0}, {DD_SOFT, 0, 100, 50, 50, 40, 50, 40, 52, 0}, DK_FX0,
         DK_FX0}},
};
static void dk_kit_load(uint32_t i)                /* factory kit i as the project's (the main loop's) */
{
    if (i >= DK_NKIT)
        return;
    dkk = DK_KITS[i];
    dkk.kit = (uint8_t)i;
    dk_sane();
}

static float dk_t60(uint32_t lane, uint32_t d)     /* DECAY d's time to -60 dB, s */
{
    const float *r = DK_T60[lane % DK_N];
    return r[0] * sy_exp2f(sy_log2f(r[1] / r[0]) * (float)d * 0.01f);
}
static float dk_kdec(float t60) { return sy_exp2f(-9.9658f / (t60 * SY_FS)); }   /* a sample's factor to fall by it */
static float dk_rel_s(void) { return 0.02f * sy_exp2f(5.6439f * (float)dkk.release * 0.01f); }   /* RELEASE: 20 ms .. 1 s */

/* ---------------------------------------------------------------------------------------------- voices --- */
typedef struct {
    uint8_t on, trk;                               /* sounding; the track it played for (its gains) */
    uint32_t n, hold;                              /* samples since the hit; before it, never off (CLAP's bursts) */
    float g[2], tg[2];                             /* the hit's gains (level, velocity, accent, pan); the track's last */
    float a, ka;                                   /* the amplitude, a sample's factor */
    float b, kb;                                   /* a second envelope (KICK's sweep, SNARE's body, CLAP's burst, WT's
                                                    * bend) */
    float ph, ph2, f, f2;                          /* phases (turns) and their steps */
    float s1, s2, c1, c2, c3, k;                   /* a filter's state and coefficients */
    float p0, p1;                                  /* a model's own */
    uint32_t rnd;                                  /* noise */
    uint32_t mph[6], minc[6];                      /* the metal's squares */
    uint32_t wph, winc, wsh;                       /* WT: the phase, its step, the shift to an index */
    const int16_t *wf;                             /* .. the level's samples */
    uint32_t next;                                 /* CLAP: the next burst's sample */
    /* V0.86 (step 2): the drum's effects as its hit set them (none in use: nothing spent) */
    uint8_t fx_sh, fx_dr, fx_eq, dr_t;             /* SHAPE, DRIVE, EQ in use (EQ: its bands, LOW 1, MID 2, HIGH 4);
                                                    * DRIVE's TYPE */
    float sh_s, sh_d, sh_w, sh_kw;                 /* SHAPE: the body's gain, the start's over it, the start's share,
                                                    * its fall a block */
    float dr_g, dr_c, dr_m;                        /* DRIVE: its gain in (CRUSH: its steps), out, its MIX */
    float e_lx, e_ly, e_hx, e_hy, e_ic1, e_ic2;    /* EQ: the LOW shelf's and the HIGH shelf's last in and out, MID's SVF */
    float e_l0, e_l1, e_la, e_h0, e_h1, e_ha;      /* .. the shelves' coefficients (b0, b1, a1) */
    float e_c1, e_c2, e_c3, e_k, e_gm;             /* .. MID's, and its gain less one */
} dk_voice_t;
static dk_voice_t dkv[DK_N];
static volatile uint8_t dk_last;                   /* the last lane hit, 0xFF none (the UI's: the kit's pages follow it) */
static uint32_t dk_nhit[DK_ROWS];                  /* hits so far, a row (the tests', the console's) */
static uint32_t dk_seed;
static struct {
    float d;                                       /* the duck's depth now: 0 .. 1 */
    float r;                                       /* its release's place: 1 at the start .. 0 */
    float g;                                       /* the synth's gain where the last block ended */
    uint8_t st;                                    /* 0 none, 1 attack, 2 release, 3 HOLD (V0.86) */
    int32_t h;                                     /* .. HOLD's samples left */
} dkd;
static struct {
    float e, g;                                    /* V0.86: COMP's level (a block's peak, followed), its gain at the end
                                                    * of the last block */
} dkc;
static float dk_db(int32_t v, float span) { return sy_exp2f((float)(v - 50) * (span / 50.0f) * 0.16609640f); }   /* a
                                                    * 0 .. 100 knob as a gain, 50 unity, span dB at its ends */
#define DK_HOLD_MS 2.5f                            /* HOLD: 0 .. 100 as 0 .. 250 ms */

static inline float dk_noise(uint32_t *s) { return (float)(int32_t)sy_rand(s) * (1.0f / 2147483648.0f); }

/* the Simper SVF's coefficients (sy_filter.c's), cutoff hz, damping k (2: Q 0.5) */
static void dk_svf(dk_voice_t *v, float hz, float k)
{
    float g = sy_tanpi(sy_minf(hz, 0.45f * SY_FS) / SY_FS);
    v->k = k;
    v->c1 = 1.0f / (1.0f + g * (g + k));
    v->c2 = g * v->c1;
    v->c3 = g * v->c2;
}

static void dk_duck_start(void) { dkd.st = 1; }

/* V0.86: drum lane's effects set for its hit: what is at rest stays off (DRIVE, EQ, then SHAPE: a gain on what
 * they give) */
static void dk_fx_hit(dk_voice_t *v, uint32_t lane)
{
    const dk_fx_t *f = &dkk.fx[lane];
    v->fx_sh = f->att != 50u || f->sus != 50u;
    if (v->fx_sh) {                                /* (SHAPE: the gain s + d w, w from 1 falling over ~5 ms) */
        float a = dk_db(f->att, 12.0f);
        v->sh_s = dk_db(f->sus, 12.0f);
        v->sh_d = a - v->sh_s;
        v->sh_w = 1.0f;
        v->sh_kw = sy_exp2f(-1.4426950f * (float)SY_BLK / (0.005f * SY_FS));   /* (w's fall a block) */
    }
    v->fx_dr = f->drive && f->dmix;
    if (v->fx_dr) {
        float d = (float)f->drive * 0.01f;
        v->dr_t = (uint8_t)(f->dtype % DD_N);
        v->dr_m = (float)f->dmix * 0.01f;
        if (v->dr_t == DD_CRUSH) {                 /* (its steps a unit: 15 bits at 1 % .. 2 bits at 100 %) */
            v->dr_g = sy_exp2f(15.0f - 13.0f * d);
            v->dr_c = 1.0f / v->dr_g;
        } else {                                   /* (in up to +24 dB; out held about level) */
            v->dr_g = sy_exp2f(4.0f * d);
            v->dr_c = 1.0f / (1.0f + 1.5f * d);
        }
    }
    v->fx_eq = (uint8_t)((f->lo != 50u) | (f->mid != 50u) << 1 | (f->hi != 50u) << 2);   /* (the bands in use) */
    if (v->fx_eq) {                                /* (+-12 dB a band: LOW a shelf at 200 Hz, HIGH one at 2 kHz (first */
        float fm = 200.0f * sy_exp2f(5.3219281f * (float)f->freq * 0.01f), g, gl, gh, kl, kh;   /* order, bilinear), */
        gl = dk_db(f->lo, 12.0f), gh = dk_db(f->hi, 12.0f);                                    /* MID at FREQ, 200 Hz */
        kl = 1.0f / sy_tanpi(200.0f / SY_FS), kh = 1.0f / sy_tanpi(2000.0f / SY_FS);           /* .. 8 kHz) */
        v->e_l0 = (kl + gl) / (kl + 1.0f), v->e_l1 = (gl - kl) / (kl + 1.0f), v->e_la = (1.0f - kl) / (kl + 1.0f);
        v->e_h0 = (gh * kh + 1.0f) / (kh + 1.0f), v->e_h1 = (1.0f - gh * kh) / (kh + 1.0f), v->e_ha = (1.0f - kh) / (kh + 1.0f);
        v->e_gm = dk_db(f->mid, 12.0f) - 1.0f;
        g = sy_tanpi(sy_minf(fm, 0.45f * SY_FS) / SY_FS);
        v->e_k = 1.4f;
        v->e_c1 = 1.0f / (1.0f + g * (g + v->e_k));
        v->e_c2 = g * v->e_c1;
        v->e_c3 = g * v->e_c2;
        v->e_lx = v->e_ly = v->e_hx = v->e_hy = v->e_ic1 = v->e_ic2 = 0.0f;
    }
}
/* .. SHAPE's gain at this block's start and end (*g0, *g1; its start's share stepped on a block): folded into the
 * drum's own gains (sy_drum_block's ramp), it costs nothing a sample */
static void dk_sh_gains(dk_voice_t *v, float *g0, float *g1)
{
    float w = v->sh_w;
    *g0 = v->sh_s + v->sh_d * w;
    w = w > 1e-5f ? w * v->sh_kw : 0.0f;
    *g1 = v->sh_s + v->sh_d * w;
    v->sh_w = w;
}
/* .. a block of DRIVE and EQ on its sound x (its coefficients held in registers through each loop: the stores to x
 * would make them reread) */
static void dk_fx_block(dk_voice_t *v, float *x)
{
    uint32_t i;
    if (v->fx_dr) {                                /* (a loop a TYPE) */
        const float g = v->dr_g, c = v->dr_c, m = v->dr_m;
        switch (v->dr_t) {
        case DD_SOFT:
            for (i = 0; i < SY_BLK; i++)
                x[i] += (sy_softclip(x[i] * g) * c - x[i]) * m;
            break;
        case DD_HARD:
            for (i = 0; i < SY_BLK; i++)
                x[i] += (sy_clampf(x[i] * g, -1.0f, 1.0f) * c - x[i]) * m;
            break;
        case DD_FOLD:                              /* (a triangle's fold: u while |u| < 1, then back) */
            for (i = 0; i < SY_BLK; i++) {
                float t = 0.25f * x[i] * g + 0.25f;
                t -= (float)sy_floori(t);
                x[i] += ((1.0f - 4.0f * sy_absf(t - 0.5f)) * c - x[i]) * m;
            }
            break;
        default:                                   /* (CRUSH: onto its steps) */
            for (i = 0; i < SY_BLK; i++)
                x[i] += ((float)sy_floori(x[i] * g + 0.5f) * c - x[i]) * m;
            break;
        }
    }
    if (v->fx_eq) {                                /* (one band: its own loop; more: one loop for the three, a band at */
        const float l0 = v->e_l0, l1 = v->e_l1, la = v->e_la, h0 = v->e_h0, h1 = v->e_h1, ha = v->e_ha;   /* its middle */
        const float c1 = v->e_c1, c2 = v->e_c2, c3 = v->e_c3, gk = v->e_gm * v->e_k;                    /* exactly */
        float lx = v->e_lx, ly = v->e_ly, hx = v->e_hx, hy = v->e_hy, ic1 = v->e_ic1, ic2 = v->e_ic2;  /* as it was) */
        if (v->fx_eq == 1u) {
            for (i = 0; i < SY_BLK; i++) {
                float x0 = x[i];
                ly = l0 * x0 + l1 * lx - la * ly;
                lx = x0;
                x[i] = ly;
            }
        } else if (v->fx_eq == 4u) {
            for (i = 0; i < SY_BLK; i++) {
                float x0 = x[i];
                hy = h0 * x0 + h1 * hx - ha * hy;
                hx = x0;
                x[i] = hy;
            }
        } else if (v->fx_eq == 2u) {
            for (i = 0; i < SY_BLK; i++) {
                float v3 = x[i] - ic2, v1 = c1 * ic1 + c2 * v3, v2 = ic2 + c2 * ic1 + c3 * v3;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                x[i] += gk * v1;
            }
        } else {
            for (i = 0; i < SY_BLK; i++) {
                float x0 = x[i], y, v3, v1, v2;
                ly = l0 * x0 + l1 * lx - la * ly;   /* (LOW) */
                lx = x0;
                v3 = ly - ic2, v1 = c1 * ic1 + c2 * v3, v2 = ic2 + c2 * ic1 + c3 * v3;   /* (MID) */
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                y = ly + gk * v1;
                hy = h0 * y + h1 * hx - ha * hy;    /* (HIGH) */
                hx = y;
                x[i] = hy;
            }
        }
        SY_FLUSH(ly);
        SY_FLUSH(hy);
        SY_FLUSH(ic1);
        SY_FLUSH(ic2);
        v->e_lx = lx, v->e_ly = ly, v->e_hx = hx, v->e_hy = hy, v->e_ic1 = ic1, v->e_ic2 = ic2;
    }
}

/* a hit of row lane (DK_DUCK: the duck) for track trk, velocity vel (1 .. 127), accented */
static void sy_drum_hit(uint32_t lane, uint32_t trk, uint32_t vel, int acc)
{
    static const float M[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};   /* the TR-808's six */
    const dk_lane_t *k;
    dk_voice_t *v;
    float g, m, p, x, s, t60;
    uint32_t i, tone;
    if (lane >= DK_ROWS)
        return;
    dk_nhit[lane]++;
    if (lane == DK_DUCK) {                         /* (V0.86: SOURCE KICK: the row's hits start nothing) */
        if (dkk.dsrc != DSRC_KICK)
            dk_duck_start();
        return;
    }
    if (lane == DK_KICK && dkk.dsrc != DSRC_ROW)   /* (.. KICK or BOTH: the KICK's hits start it) */
        dk_duck_start();
    k = &dkk.l[lane];
    v = &dkv[lane];
    x = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    s = (float)dkk.vsens * 0.01f;
    g = DK_GAIN * (float)k->level * (1.0f / 127.0f) * (1.0f - s + s * x * x);
    if (acc)
        g *= 1.0f + (float)dkk.accent * 0.01f;
    p = sy_clampf(((float)k->pan - 64.0f) * (1.0f / 63.0f), -1.0f, 1.0f);
    m = sy_exp2f((float)(k->tune + dkk.ktune) * (1.0f / 12.0f));   /* (V0.86: the kit's TUNE, DECAY, TONE on its own) */
    tone = (uint32_t)sy_clampi((int32_t)k->tone + dkk.ktone, 0, 100);
    if (dkk.atone)                                 /* (V0.86, K9: ACCENT's TONE: a soft hit darker, an accented one
                                                    * brighter) */
        tone = (uint32_t)sy_clampi((int32_t)tone - (int32_t)((float)dkk.atone * 0.6f * (1.0f - x) + 0.5f) +
                                   (acc ? (int32_t)dkk.atone / 4 : 0), 0, 100);
    t60 = dk_t60(lane, k->decay);
    if (dkk.kdecay != 50u)
        t60 *= sy_exp2f(((float)dkk.kdecay - 50.0f) * 0.02f);
    if (lane == DK_WT1 || lane == DK_WT2) {        /* (its table first: none, or being built, no hit) */
        const wt_table_t *t = &wt_tab[lane - DK_WT1];
        uint32_t fr, lv;
        float top = sy_exp2f(3.0f * (float)k->chr * 0.01f), f = 130.81f * m;
        if (!t->n || t->busy)
            return;
        fr = (uint32_t)sy_floori((float)tone * 0.01f * (float)(t->n - 1u) + 0.5f);
        lv = wt_level((uint32_t)sy_minf(f * top / SY_FS * 4294967296.0f, 4.0e9f));
        lv = lv < WT_NLEV ? lv : WT_NLEV - 1u;
        v->wf = wt_frame(t, fr < t->n ? fr : t->n - 1u, lv);
        v->wsh = 32u - WT_BITS[lv];
        v->wph = 0;
        v->f = f / SY_FS;
        v->p0 = top - 1.0f;                        /* (the bend: f x (1 + p0 b), b falling over ~40 ms) */
        v->b = 1.0f;
        v->kb = dk_kdec(0.28f);
    }
    if (!v->on) {                                  /* (from silence: its track's gains as they are) */
        v->tg[0] = sy_tgain[trk % (SY_NOTRK + 1u)][0];
        v->tg[1] = sy_tgain[trk % (SY_NOTRK + 1u)][1];
    }
    v->g[0] = g * sy_minf(1.0f, 1.0f - p);
    v->g[1] = g * sy_minf(1.0f, 1.0f + p);
    v->trk = (uint8_t)(trk % (SY_NOTRK + 1u));
    v->n = v->hold = 0;
    v->a = 1.0f;
    v->ka = dk_kdec(t60);
    v->rnd = (dk_seed += 0x9E3779B9u) | 1u;
    v->s1 = v->s2 = 0.0f;
    switch (lane) {
    case DK_KICK:
        v->ph = 0.0f;
        v->f = 48.0f * m / SY_FS;
        v->p0 = 1.0f + 6.0f * (float)k->chr * 0.01f;   /* (the sweep's height: up to x7) */
        v->b = 1.0f;
        v->kb = dk_kdec(0.2f);
        v->p1 = 1.0f + 3.0f * (float)tone * 0.01f;  /* (the drive) */
        v->f2 = (float)tone * 0.004f;              /* (the click's level) */
        break;
    case DK_SNARE:
        v->ph = v->ph2 = 0.0f;
        v->f = 185.0f * m / SY_FS;
        v->f2 = 330.0f * m / SY_FS;
        v->b = 1.0f;
        v->kb = dk_kdec(0.05f + 0.35f * t60);
        v->c1 = 1.0f - sy_exp2f(-9.0647f * 1000.0f * sy_exp2f(3.0f * (float)tone * 0.01f) / SY_FS);   /* (the
                                                       * high-pass's one-pole, 1 .. 8 kHz) */
        v->p0 = 0.25f + 0.75f * (float)k->chr * 0.01f;   /* (the noise) */
        v->p1 = 1.0f - 0.5f * (float)k->chr * 0.01f;     /* (the body) */
        break;
    case DK_CLAP:
        dk_svf(v, 1100.0f * m, 1.6f - 1.4f * (float)tone * 0.01f);
        v->p0 = (0.004f + 0.016f * (float)k->chr * 0.01f) * SY_FS;   /* (SPREAD: 4 .. 20 ms) */
        v->next = (uint32_t)v->p0;
        v->hold = 3u * (uint32_t)v->p0 + 1u;     /* (the fourth burst at 3 SPREAD, then the tail) */
        v->kb = dk_kdec(0.025f);
        break;
    default:                                       /* HAT CL, HAT OP, CYM */
        if (lane <= DK_CYM) {
            float sc = lane == DK_CYM ? 0.85f : 1.0f, hz = (lane == DK_CYM ? 3000.0f : 5500.0f) *
                                                            sy_exp2f(1.2f * ((float)tone - 50.0f) * 0.02f);
            for (i = 0; i < 6u; i++)
                v->minc[i] = (uint32_t)(M[i] * sc * m / SY_FS * 4294967296.0f);
            v->p0 = (float)k->chr * 0.01f * (1.0f / 3.0f);   /* (the squares, -6 .. 6, as -2 .. 2 x METAL) */
            v->p1 = 1.0f - (float)k->chr * 0.01f;            /* (the noise) */
            dk_svf(v, hz, 1.4f);
            if (lane == DK_HATC && dkv[DK_HATO].on)
                dkv[DK_HATO].ka = dk_kdec(0.03f);  /* (the choke) */
        }
        break;
    }
    dk_fx_hit(v, lane);
    v->on = 1;
    dk_last = (uint8_t)lane;
}

/* a block of each model into x (mono, the amplitude in) */
static void dk_kick(dk_voice_t *v, float *x)
{
    uint32_t i;
    float ph = v->ph, a = v->a, b = v->b;
    for (i = 0; i < SY_BLK; i++) {
        float y = sy_softclip(sy_sin2pi(ph) * v->p1);
        if (v->n + i < 66u)                        /* (the click: 1.5 ms of noise) */
            y += v->f2 * dk_noise(&v->rnd) * (1.0f - (float)(v->n + i) * (1.0f / 66.0f));
        x[i] = y * a;
        ph += v->f * (1.0f + v->p0 * b);
        if (ph >= 1.0f)
            ph -= 1.0f;
        b *= v->kb;
        a *= v->ka;
    }
    v->ph = ph, v->a = a, v->b = b;
}
static void dk_snare(dk_voice_t *v, float *x)
{
    uint32_t i;
    float ph = v->ph, ph2 = v->ph2, a = v->a, b = v->b, lp = v->s1;
    for (i = 0; i < SY_BLK; i++) {
        float nz = dk_noise(&v->rnd);
        lp += (nz - lp) * v->c1;
        x[i] = (0.6f * sy_sin2pi(ph) + 0.4f * sy_sin2pi(ph2)) * b * v->p1 + (nz - lp) * a * v->p0;
        ph += v->f;
        ph2 += v->f2;
        ph -= ph >= 1.0f ? 1.0f : 0.0f;
        ph2 -= ph2 >= 1.0f ? 1.0f : 0.0f;
        a *= v->ka;
        b *= v->kb;
    }
    v->ph = ph, v->ph2 = ph2, v->a = a, v->b = b, v->s1 = lp;
}
static void dk_clap(dk_voice_t *v, float *x)
{
    uint32_t i;
    float a = v->a, ic1 = v->s1, ic2 = v->s2;
    for (i = 0; i < SY_BLK; i++) {
        float x0 = dk_noise(&v->rnd), v3 = x0 - ic2, v1 = v->c1 * ic1 + v->c2 * v3, v2 = ic2 + v->c2 * ic1 + v->c3 * v3;
        uint32_t n = v->n + i;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (n < v->hold) {                         /* (the bursts: each a quick fall, the next from the top) */
            if (n == v->next) {
                a = 1.0f;
                v->next += (uint32_t)v->p0;
            }
            a *= v->kb;
        } else {
            a *= v->ka;
        }
        x[i] = 2.5f * v->k * v1 * a;
    }
    v->a = a, v->s1 = ic1, v->s2 = ic2;
}
static void dk_metal(dk_voice_t *v, float *x)
{
    uint32_t i, p0 = v->mph[0], p1 = v->mph[1], p2 = v->mph[2], p3 = v->mph[3], p4 = v->mph[4], p5 = v->mph[5];
    const uint32_t i0 = v->minc[0], i1 = v->minc[1], i2 = v->minc[2], i3 = v->minc[3], i4 = v->minc[4], i5 = v->minc[5];
    int8_t sq[SY_BLK];
    float a = v->a, ic1 = v->s1, ic2 = v->s2;
    for (i = 0; i < SY_BLK; i++) {                 /* (two passes, each in the registers: the squares' sums (-6 .. 6), */
        p0 += i0, p1 += i1, p2 += i2, p3 += i3, p4 += i4, p5 += i5;   /* then the rest: V0.85's cost model) */
        sq[i] = (int8_t)(2 * (int32_t)((p0 >> 31) + (p1 >> 31) + (p2 >> 31) + (p3 >> 31) + (p4 >> 31) + (p5 >> 31)) - 6);
    }
    v->mph[0] = p0, v->mph[1] = p1, v->mph[2] = p2, v->mph[3] = p3, v->mph[4] = p4, v->mph[5] = p5;
    for (i = 0; i < SY_BLK; i++) {
        float x0, v3, v1, v2;
        x0 = v->p0 * (float)sq[i] + v->p1 * dk_noise(&v->rnd);
        v3 = x0 - ic2;
        v1 = v->c1 * ic1 + v->c2 * v3;
        v2 = ic2 + v->c2 * ic1 + v->c3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        x[i] = (x0 - v->k * v1 - v2) * a;
        a *= v->ka;
    }
    v->a = a, v->s1 = ic1, v->s2 = ic2;
}
static void dk_wt(dk_voice_t *v, uint32_t lane, float *x)
{
    const wt_table_t *t = &wt_tab[lane - DK_WT1];
    uint32_t i, inc, inc1, ph = v->wph, sh = v->wsh;
    int32_t dinc;
    float a = v->a, b1 = v->b * sy_exp2f(sy_log2f(v->kb) * (float)SY_BLK), fade = 1.0f, df = 0.0f;
    inc = (uint32_t)(v->f * (1.0f + v->p0 * v->b) * 4294967296.0f);
    inc1 = (uint32_t)(v->f * (1.0f + v->p0 * b1) * 4294967296.0f);
    dinc = ((int32_t)inc1 - (int32_t)inc) / (int32_t)SY_BLK;
    if (t->busy) {                                 /* (its table being built: out over this block, then off) */
        df = -1.0f / (float)SY_BLK;
        v->hold = 0;
    }
    for (i = 0; i < SY_BLK; i++) {
        uint32_t ix = ph >> sh, fr = (ph << (32u - sh)) >> 17;
        int32_t s0 = v->wf[ix], s1 = v->wf[ix + 1u];
        x[i] = (float)(s0 + (((s1 - s0) * (int32_t)fr) >> 15)) * (1.0f / 32768.0f) * a * fade;
        ph += inc;
        inc = (uint32_t)((int32_t)inc + dinc);
        a *= v->ka;
        fade += df;
    }
    if (t->busy)
        a = 0.0f;
    v->wph = ph, v->a = a, v->b = b1;
}

/* the duck on the synth's sound (ml, mr: after the effects), a block: idle and at rest, untouched */
static void dk_duck_block(float *ml, float *mr)
{
    float g0 = dkd.g, g1, d = 0.0f, dg;
    uint32_t i;
    if (!dkd.st && g0 >= 1.0f)
        return;
    if (dkd.st == 1u) {                            /* (ATTACK: from where it is to the bottom, straight) */
        float att = (float)dkk.attack * 0.001f * SY_FS;
        dkd.d = att <= (float)SY_BLK ? 1.0f : sy_minf(1.0f, dkd.d + (float)SY_BLK / att);
        if (dkd.d >= 1.0f) {                       /* (V0.86: HOLD first, if any) */
            dkd.st = dkk.dhold ? 3u : 2u, dkd.r = 1.0f;
            dkd.h = (int32_t)((float)dkk.dhold * DK_HOLD_MS * 0.001f * SY_FS);
        }
        d = dkd.d;
    } else if (dkd.st == 3u) {                     /* (HOLD: down, then the release) */
        dkd.h -= (int32_t)SY_BLK;
        if (dkd.h <= 0)
            dkd.st = 2;
        d = 1.0f;
    } else if (dkd.st == 2u) {                     /* (RELEASE: back along SHAPE) */
        float r;
        dkd.r = sy_maxf(0.0f, dkd.r - (float)SY_BLK / (dk_rel_s() * SY_FS));
        r = dkd.r;
        d = dkk.shape == DS_EXP ? r * r * r : dkk.shape == DS_PUMP ? 1.0f - (1.0f - r) * (1.0f - r) * (1.0f - r) : r;
        dkd.d = d;
        if (r <= 0.0f)
            dkd.st = 0;
    }
    g1 = 1.0f - (float)dkk.amount * 0.01f * d;
    dg = (g1 - g0) * (1.0f / (float)SY_BLK);
    for (i = 0; i < SY_BLK; i++, g0 += dg) {
        ml[i] *= g0;
        mr[i] *= g0;
    }
    dkd.g = g1;
}

/* V0.86: COMP on the drums' sum (l, r) a block: a peak a block, followed (ATTACK 0.1 .. 30 ms, RELEASE 20 ms .. 1 s);
 * above AMOUNT's threshold (0 .. -30 dB) 4:1, AMOUNT's make-up (up to +9 dB); MIX the pressed in parallel. The gain
 * goes down from the block's start (its peak known before it plays: a hit's start pressed with it; ATTACK how fast the
 * level follows), back up over the block */
static void dk_comp_block(float *l, float *r)
{
    float pk = 0.0f, amt = (float)dkk.comp[0] * 0.01f, ta, tr, thr, g0 = dkc.g, g1 = 1.0f, dg;
    float m = (float)dkk.comp[3] * 0.01f;
    uint32_t i;
    for (i = 0; i < SY_BLK; i++)
        pk = sy_maxf(pk, sy_maxf(sy_absf(l[i]), sy_absf(r[i])));
    ta = 0.0001f * sy_exp2f(8.2288f * (float)dkk.comp[1] * 0.01f);
    tr = 0.02f * sy_exp2f(5.6439f * (float)dkk.comp[2] * 0.01f);
    dkc.e += (pk - dkc.e) * (1.0f - sy_exp2f(-1.4426950f * (float)SY_BLK / ((pk > dkc.e ? ta : tr) * SY_FS)));
    thr = sy_exp2f(-30.0f * amt * 0.16609640f);
    if (dkc.e > thr)
        g1 = sy_exp2f(0.75f * sy_log2f(thr / dkc.e));
    if (amt > 0.2f)                                /* (make-up: half the reduction a -6 dB peak would get) */
        g1 *= sy_exp2f(0.375f * (30.0f * amt - 6.0f) * 0.16609640f);
    if (g1 < g0)                                   /* (down: from the block's start, its peak known: 0.7 ms ahead) */
        g0 = g1;
    dg = (g1 - g0) * (1.0f / (float)SY_BLK);
    for (i = 0; i < SY_BLK; i++, g0 += dg) {
        l[i] += (l[i] * g0 - l[i]) * m;
        r[i] += (r[i] * g0 - r[i]) * m;
    }
    SY_FLUSH(dkc.e);
    dkc.g = g1;
}

/* a block (sy_block, after the effects): the duck on the synth, then the drums sounding added in (V0.86: through
 * COMP when it is on) */
static void sy_drum_block(float *ml, float *mr)
{
    float x[SY_BLK], bl[SY_BLK], br[SY_BLK], *ol = ml, *orr = mr;
    uint32_t l, i, bus = dkk.comp[0] != 0u;
    dk_duck_block(ml, mr);
    if (bus) {
        memset(bl, 0, sizeof bl);
        memset(br, 0, sizeof br);
        ol = bl, orr = br;
    }
    for (l = 0; l < DK_N; l++) {
        dk_voice_t *v = &dkv[l];
        float gl, gr, tl, tr, dl, dr, el, er;
        if (!v->on)
            continue;
        switch (l) {
        case DK_KICK: dk_kick(v, x); break;
        case DK_SNARE: dk_snare(v, x); break;
        case DK_CLAP: dk_clap(v, x); break;
        case DK_WT1: case DK_WT2: dk_wt(v, l, x); break;
        default: dk_metal(v, x); break;
        }
        if (v->fx_dr | v->fx_eq)
            dk_fx_block(v, x);
        tl = sy_tgain[v->trk][0], tr = sy_tgain[v->trk][1];
        gl = v->g[0] * v->tg[0], gr = v->g[1] * v->tg[1];
        el = v->g[0] * tl, er = v->g[1] * tr;
        if (v->fx_sh) {                            /* (SHAPE: in the ramp) */
            float s0, s1;
            dk_sh_gains(v, &s0, &s1);
            gl *= s0, gr *= s0, el *= s1, er *= s1;
        }
        dl = (el - gl) * (1.0f / (float)SY_BLK), dr = (er - gr) * (1.0f / (float)SY_BLK);
        for (i = 0; i < SY_BLK; i++, gl += dl, gr += dr) {
            ol[i] += x[i] * gl;
            orr[i] += x[i] * gr;
        }
        v->tg[0] = tl, v->tg[1] = tr;
        v->n += SY_BLK;
        SY_FLUSH(v->s1);
        SY_FLUSH(v->s2);
        if (v->a < 1e-4f && v->n >= v->hold)       /* (-80 dB: done) */
            v->on = 0;
    }
    if (bus) {
        dk_comp_block(bl, br);
        for (i = 0; i < SY_BLK; i++) {
            ml[i] += bl[i];
            mr[i] += br[i];
        }
    } else {
        dkc.e = 0.0f, dkc.g = 1.0f;                /* (COMP off: at rest for when it comes on) */
    }
}

static void sy_drum_off(void)                      /* every drum silent, the duck let go (all sound off, a load) */
{
    uint32_t l;
    for (l = 0; l < DK_N; l++)
        dkv[l].on = 0;
    dkd.st = 0;
    dkd.d = 0.0f;
    dkd.g = 1.0f;
    dkd.h = 0;
    dkc.e = 0.0f;
    dkc.g = 1.0f;
}
static int sy_drum_sounding(void)                  /* any drum sounding, or the duck not at rest */
{
    uint32_t l;
    for (l = 0; l < DK_N; l++)
        if (dkv[l].on)
            return 1;
    return dkd.st != 0u || dkd.g < 1.0f || (dkk.comp[0] && dkc.e > 1e-4f);
}
static void sy_drum_init(void)
{
    memset(dkv, 0, sizeof dkv);
    dk_seed = 0x6A09E667u;
    dk_last = 0xFFu;
    dk_defaults();
    sy_drum_off();
}
