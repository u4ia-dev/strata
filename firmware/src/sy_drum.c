/* SPDX-License-Identifier: GPL-3.0-only */
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
 * V0.87 (step 2; the plan's D1-D5, U7): seven models, any row any model: ANALOG (a sine body swept down, its second
 * body and noise: SNAP), PUNCH (V0.85's KICK, SNARE, CLAP), FM (two operators: RATIO, INDEX; DRUM a pitch drop and a
 * click, BELL neither), CHIP (8-bit: a square or a 4-bit triangle swept, or a 15-bit noise register; the level in 16
 * steps), NOISE (noise through a band-pass), METAL (the six squares: CLOSED, OPEN, CYMBAL), WAVE (the one-shot on
 * OSC A's or OSC B's table). A row's model and variant ride in its lane's two spare bytes (0: the row's own, V0.85's:
 * a project of before plays as it did, bit for bit); a voice's model state is a union (one model a row at a time).
 * Drum presets by category (DKP: a name, a model, its four knobs, its effects), the kits copies of them (a row keeps
 * the preset it came from, its name shown: the kit's spare bytes), six factory kits (DK_FKITS); the user's 64 drum
 * presets and 16 kits in the store (sy_app.c sy_dk_*), their names here (dk_up, dk_uk).
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
    uint8_t model, var;                            /* V0.87: its model + 1 (DM_*; 0 the row's own, V0.85's), variant */
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
    uint8_t pid0[2];                               /* V0.87: rows 1, 2's presets (dk_pid: 0 none, 1 .. DK_NPRE the
                                                    * factory's, 0x80 + n the user's n) */
    /* V0.86 (the project's stream version 3; versions 1 and 2 take these as a kit starts): */
    int8_t ktune;                                  /* HOME's on a drum track: every drum's TUNE moved, -24 .. 24 */
    uint8_t kdecay;                                /* .. every drum's DECAY time x 2^((kdecay - 50) / 50): 0 .. 100 */
    int8_t ktone;                                  /* .. every drum's TONE moved, -50 .. 50 */
    uint8_t dsrc, dhold, atone;                    /* DUCK's SOURCE and HOLD, ACCENT's TONE (step 2) */
    uint8_t comp[4];                               /* the kit's COMP: AMOUNT, ATTACK, RELEASE, MIX (step 2) */
    char name[13];                                 /* the kit's name (HOME shows it; the factory kits': step 2) */
    uint8_t kit;                                   /* .. the factory kit it came from (0xFF none; V0.87: 0x80 + n the
                                                    * user's kit n) */
    dk_fx_t fx[DK_N];
    uint8_t pid1[6];                               /* V0.87: rows 3 .. 8's presets */
} dk_kit_t;
#define DK_KIT2 72u                                /* (version 2's kit: the bytes before ktune) */
_Static_assert(sizeof(dk_lane_t) == 8u && sizeof(dk_fx_t) == 10u && sizeof(dk_kit_t) == 182u &&
               __builtin_offsetof(dk_kit_t, ktune) == DK_KIT2, "the kit's bytes (a project's)");
static dk_kit_t dkk;

/* V0.87: the models, their variants, what TONE and the model's own knob are on each */
enum { DM_ANALOG, DM_PUNCH, DM_FM, DM_CHIP, DM_NOISE, DM_METAL, DM_WAVE, DM_N };
static const char *const DM_NAME[DM_N] = {"ANALOG", "PUNCH", "FM", "CHIP", "NOISE", "METAL", "WAVE"};
static const uint8_t DM_NVAR[DM_N] = {1, 3, 2, 3, 1, 3, 2};
static const char *const DM_VAR[DM_N][3] = {{"BODY"}, {"KICK", "SNARE", "CLAP"}, {"DRUM", "BELL"},
                                            {"SQUARE", "TRIANGLE", "NOISE"}, {"BAND"}, {"CLOSED", "OPEN", "CYMBAL"},
                                            {"TABLE A", "TABLE B"}};
static const char *const DM_CHR[DM_N][3] = {{"SNAP"}, {"PUNCH", "SNAP", "SPREAD"}, {"INDEX", "INDEX"},
                                            {"SWEEP", "SWEEP", "LOOP"}, {"WIDTH"}, {"METAL", "METAL", "METAL"},
                                            {"BEND", "BEND"}};
static const char *const DM_TONE[DM_N] = {"DROP", "TONE", "RATIO", "SPEED", "FREQ", "TONE", "FRAME"};
/* DECAY's range a model and variant, s to -60 dB (0 .. 100: exponential between; V0.85's rows as they were) */
static const float DM_T60[DM_N][3][2] = {
    {{0.05f, 4.0f}}, {{0.08f, 2.0f}, {0.06f, 1.0f}, {0.06f, 1.2f}}, {{0.03f, 2.5f}, {0.1f, 4.0f}},
    {{0.03f, 1.5f}, {0.03f, 1.5f}, {0.02f, 1.5f}}, {{0.02f, 2.0f}}, {{0.02f, 0.4f}, {0.15f, 2.0f}, {0.4f, 6.0f}},
    {{0.05f, 3.0f}, {0.05f, 3.0f}}};
static uint32_t dk_model(uint32_t l, uint32_t *var)   /* row l's model (DM_*) and its variant */
{
    static const uint8_t OWN[DK_N] = {DM_PUNCH, DM_PUNCH, DM_PUNCH, DM_METAL, DM_METAL, DM_METAL, DM_WAVE, DM_WAVE};
    static const uint8_t OWNV[DK_N] = {0, 1, 2, 0, 1, 2, 0, 1};
    const dk_lane_t *k = &dkk.l[l % DK_N];
    uint32_t m = k->model && k->model <= DM_N ? k->model - 1u : OWN[l % DK_N];
    *var = k->model && k->model <= DM_N ? k->var % DM_NVAR[m] : OWNV[l % DK_N];
    return m;
}
static const char *dk_tone_name(uint32_t m, uint32_t v) { return m == DM_CHIP && v == 2u ? "PITCH" : DM_TONE[m % DM_N]; }
#define DK_GAIN 0.5f                               /* a lane's peak at LEVEL 127, velocity 127 (-6 dB: room to add) */

static const dk_lane_t DK_DEF[DK_N] = {           /* the lanes as a kit starts */
    {0, 45, 30, 55, 110, 64, 0, 0}, {0, 40, 45, 55, 100, 64, 0, 0}, {0, 40, 40, 45, 95, 70, 0, 0},
    {0, 30, 55, 70, 80, 58, 0, 0},  {0, 40, 55, 70, 75, 58, 0, 0},  {0, 50, 45, 55, 70, 72, 0, 0},
    {0, 45, 0, 40, 95, 54, 0, 0},   {12, 40, 0, 30, 90, 74, 0, 0},
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
    memset(dkk.pid1, 0, sizeof dkk.pid1);
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
    dkk.pid0[0] = dkk.pid0[1] = 0;
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
        if (k->model > DM_N)                       /* (V0.87: the model, its variant) */
            k->model = 0;
        k->var = (uint8_t)(k->model ? k->var % DM_NVAR[k->model - 1u] : 0u);
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
/* V0.87 (step 2, D2: the plan's categories): the factory drum presets, 8 a category: a name, a category, the lane
 * (TUNE, DECAY, TONE, the model's own, LEVEL, PAN, the model + 1, its variant) and the effects (DRIVE's TYPE, amount,
 * MIX; EQ LOW, MID, HIGH, FREQ; SHAPE's ATTACK, SUSTAIN). A preset loaded into a row brings all but LEVEL and PAN */
enum { DC_KICK, DC_SNARE, DC_CLAP, DC_HAT, DC_CYMBAL, DC_TOM, DC_PERC, DC_FX, DC_N };
static const char *const DC_NAME[DC_N] = {"KICK", "SNARE", "CLAP", "HAT", "CYMBAL", "TOM", "PERC", "FX"};
static const uint8_t DK_ROWCAT[DK_N] = {DC_KICK, DC_SNARE, DC_CLAP, DC_HAT, DC_HAT, DC_CYMBAL, DC_PERC, DC_TOM};
typedef struct {
    char name[12];                                 /* (11 characters at most, then 0) */
    uint8_t cat;
    dk_lane_t l;
    dk_fx_t fx;
    uint8_t rsv;
} dk_pre_t;
_Static_assert(sizeof(dk_pre_t) == 32u, "a drum preset's bytes (the store's records)");
#define DKP_(n, c, m, v, tu, de, to, ch) {n, c, {tu, de, to, ch, 100, 64, (m) + 1, v}, DK_FX0, 0}
#define DKPF(n, c, m, v, tu, de, to, ch, ...) {n, c, {tu, de, to, ch, 100, 64, (m) + 1, v}, {__VA_ARGS__}, 0}
static const dk_pre_t DKP[] = {
    DKP_("808 KICK", DC_KICK, DM_ANALOG, 0, 0, 70, 30, 0),
    DKP_("909 KICK", DC_KICK, DM_PUNCH, 0, 0, 42, 62, 66),
    DKPF("TRAP SUB", DC_KICK, DM_ANALOG, 0, -2, 92, 22, 0, DD_SOFT, 22, 100, 56, 50, 46, 50, 50, 50, 0),
    DKP_("FM KICK", DC_KICK, DM_FM, 0, 0, 34, 9, 30),
    DKP_("CHIP KICK", DC_KICK, DM_CHIP, 0, -12, 30, 70, 70),
    DKPF("HARD KICK", DC_KICK, DM_PUNCH, 0, 0, 50, 80, 80, DD_HARD, 50, 100, 58, 46, 50, 40, 62, 46, 0),
    DKPF("LO-FI KICK", DC_KICK, DM_ANALOG, 0, 0, 55, 35, 5, DD_CRUSH, 50, 80, 62, 50, 30, 50, 50, 50, 0),
    DKP_("DEEP KICK", DC_KICK, DM_ANALOG, 0, -4, 82, 45, 0),
    DKP_("808 SNARE", DC_SNARE, DM_ANALOG, 0, 24, 35, 15, 60),
    DKP_("909 SNARE", DC_SNARE, DM_PUNCH, 1, 0, 42, 62, 66),
    DKP_("TRAP SNARE", DC_SNARE, DM_PUNCH, 1, 2, 38, 72, 76),
    DKP_("FM SNARE", DC_SNARE, DM_FM, 0, 24, 30, 62, 72),
    DKP_("CHIP SNARE", DC_SNARE, DM_CHIP, 2, 0, 30, 60, 20),
    DKP_("RIMSHOT", DC_SNARE, DM_FM, 1, 12, 5, 45, 40),
    DKP_("BRUSH", DC_SNARE, DM_NOISE, 0, 0, 45, 55, 10),
    DKPF("LO-FI SNARE", DC_SNARE, DM_PUNCH, 1, 0, 40, 36, 55, DD_CRUSH, 70, 100, 56, 54, 26, 50, 50, 50, 0),
    DKP_("808 CLAP", DC_CLAP, DM_PUNCH, 2, 0, 46, 45, 58),
    DKP_("909 CLAP", DC_CLAP, DM_PUNCH, 2, 2, 40, 55, 48),
    DKP_("TRAP CLAP", DC_CLAP, DM_PUNCH, 2, 3, 34, 65, 30),
    DKP_("WIDE CLAP", DC_CLAP, DM_PUNCH, 2, -2, 55, 40, 85),
    DKP_("SNAP", DC_CLAP, DM_NOISE, 0, 0, 10, 70, 60),
    DKP_("CHIP CLAP", DC_CLAP, DM_CHIP, 2, 0, 25, 75, 10),
    DKPF("DIRTY CLAP", DC_CLAP, DM_PUNCH, 2, 0, 44, 50, 60, DD_FOLD, 40, 80, 50, 50, 50, 50, 50, 50, 0),
    DKP_("ROOM CLAP", DC_CLAP, DM_PUNCH, 2, -1, 70, 35, 70),
    DKP_("808 HAT", DC_HAT, DM_METAL, 0, 0, 24, 62, 78),
    DKP_("808 OPEN", DC_HAT, DM_METAL, 1, 0, 46, 58, 78),
    DKP_("909 HAT", DC_HAT, DM_METAL, 0, 0, 20, 72, 86),
    DKP_("909 OPEN", DC_HAT, DM_METAL, 1, 0, 40, 66, 86),
    DKP_("TRAP HAT", DC_HAT, DM_METAL, 0, 2, 12, 80, 70),
    DKP_("CHIP HAT", DC_HAT, DM_CHIP, 2, 0, 8, 95, 80),
    DKP_("FM HAT", DC_HAT, DM_FM, 1, 24, 10, 85, 90),
    DKP_("NOISE HAT", DC_HAT, DM_NOISE, 0, 0, 15, 92, 5),
    DKP_("808 CYMBAL", DC_CYMBAL, DM_METAL, 2, 0, 62, 46, 62),
    DKP_("909 CRASH", DC_CYMBAL, DM_METAL, 2, 0, 72, 60, 70),
    DKP_("RIDE", DC_CYMBAL, DM_METAL, 2, 5, 55, 70, 85),
    DKPF("CHINA", DC_CYMBAL, DM_METAL, 2, -3, 66, 55, 100, DD_FOLD, 30, 70, 50, 50, 50, 50, 50, 50, 0),
    DKP_("FM RIDE", DC_CYMBAL, DM_FM, 1, 24, 60, 80, 60),
    DKP_("CHIP CRASH", DC_CYMBAL, DM_CHIP, 2, 0, 55, 85, 10),
    DKP_("NOISE CRASH", DC_CYMBAL, DM_NOISE, 0, 0, 70, 85, 0),
    DKP_("SPLASH", DC_CYMBAL, DM_METAL, 2, 7, 35, 66, 80),
    DKP_("808 TOM LO", DC_TOM, DM_ANALOG, 0, 7, 55, 25, 0),
    DKP_("808 TOM MID", DC_TOM, DM_ANALOG, 0, 12, 50, 25, 0),
    DKP_("808 TOM HI", DC_TOM, DM_ANALOG, 0, 17, 45, 25, 0),
    DKP_("909 TOM", DC_TOM, DM_PUNCH, 0, 14, 40, 40, 40),
    DKP_("FM TOM", DC_TOM, DM_FM, 0, 12, 45, 9, 25),
    DKP_("CHIP TOM", DC_TOM, DM_CHIP, 1, 5, 40, 40, 50),
    DKP_("CONGA", DC_TOM, DM_ANALOG, 0, 24, 30, 15, 10),
    DKP_("TIMPANI", DC_TOM, DM_FM, 0, 3, 75, 9, 15),
    DKP_("COWBELL", DC_PERC, DM_FM, 1, 14, 25, 30, 30),
    DKP_("CLAVE", DC_PERC, DM_FM, 1, 24, 8, 9, 5),
    DKPF("SHAKER", DC_PERC, DM_NOISE, 0, 0, 15, 85, 20, DD_SOFT, 0, 100, 50, 50, 50, 50, 30, 50, 0),
    DKP_("BELL", DC_PERC, DM_FM, 1, 12, 60, 55, 40),
    DKP_("BLOCK", DC_PERC, DM_FM, 1, 12, 10, 30, 20),
    DKP_("MARACA", DC_PERC, DM_CHIP, 2, 0, 10, 90, 30),
    DKP_("TABLE A HIT", DC_PERC, DM_WAVE, 0, 0, 45, 0, 40),
    DKP_("TABLE B HIT", DC_PERC, DM_WAVE, 1, 12, 40, 0, 30),
    DKP_("ZAP", DC_FX, DM_FM, 0, 24, 25, 45, 100),
    DKP_("LASER", DC_FX, DM_CHIP, 0, 24, 35, 70, 100),
    DKP_("COIN", DC_FX, DM_CHIP, 0, 24, 25, 10, 0),
    DKP_("NOISE BURST", DC_FX, DM_NOISE, 0, 0, 45, 50, 30),
    DKP_("BLIP", DC_FX, DM_CHIP, 1, 24, 10, 20, 30),
    DKP_("DROP", DC_FX, DM_WAVE, 0, -12, 70, 50, 100),
    DKP_("METAL HIT", DC_FX, DM_FM, 1, 5, 50, 95, 100),
    DKPF("CRUSHER", DC_FX, DM_PUNCH, 1, -5, 50, 80, 90, DD_CRUSH, 90, 100, 50, 50, 50, 50, 50, 50, 0),
};
#define DK_NPRE (sizeof DKP / sizeof DKP[0])
_Static_assert(DK_NPRE == 8u * DC_N && DK_NPRE < 0x80u, "eight factory drum presets a category");
/* .. the factory kits (D4: the user's six): each row a copy of a preset (the row's place: its key, its GM note), its
 * LEVEL and PAN; ACCENT, VELOCITY; the duck's AMOUNT, ATTACK, RELEASE, SHAPE, SOURCE, HOLD; ACCENT's TONE; COMP */
typedef struct {
    char name[13];
    uint8_t pre[DK_N], lvl[DK_N], pan[DK_N];
    uint8_t accent, vsens, amount, attack, release, shape, dsrc, dhold, atone, comp[4];
} dk_fkit_t;
enum { KP_808K, KP_909K, KP_TRAPK, KP_FMK, KP_CHIPK, KP_HARDK, KP_LOFIK, KP_DEEPK, KP_808S, KP_909S, KP_TRAPS, KP_FMS,
       KP_CHIPS, KP_RIM, KP_BRUSH, KP_LOFIS, KP_808C, KP_909C, KP_TRAPC, KP_WIDEC, KP_SNAP, KP_CHIPC, KP_DIRTYC,
       KP_ROOMC, KP_808H, KP_808O, KP_909H, KP_909O, KP_TRAPH, KP_CHIPH, KP_FMH, KP_NOISEH, KP_808CY, KP_909CR,
       KP_RIDE, KP_CHINA, KP_FMRIDE, KP_CHIPCR, KP_NOISECR, KP_SPLASH, KP_TOMLO, KP_TOMMID, KP_TOMHI, KP_909T, KP_FMT,
       KP_CHIPT, KP_CONGA, KP_TIMP, KP_COW, KP_CLAVE, KP_SHAKER, KP_BELL, KP_BLOCK, KP_MARACA, KP_TABA, KP_TABB,
       KP_ZAP, KP_LASER, KP_COIN, KP_BURST, KP_BLIP, KP_DROP, KP_METAL, KP_CRUSH, KP__N };
_Static_assert(KP__N == DK_NPRE, "the presets' names here, in DKP's order");
static const dk_fkit_t DK_FKITS[] = {
    {"808", {KP_808K, KP_808S, KP_808C, KP_808H, KP_808O, KP_808CY, KP_COW, KP_TOMMID}, {115, 92, 92, 76, 72, 64, 80, 88},
     {64, 62, 70, 56, 56, 74, 50, 78}, 50, 70, 50, 2, 45, DS_EXP, DSRC_ROW, 0, 30, {0, 20, 40, 100}},
    {"909", {KP_909K, KP_909S, KP_909C, KP_909H, KP_909O, KP_909CR, KP_RIM, KP_909T}, {112, 100, 92, 80, 74, 66, 82, 88},
     {64, 62, 68, 54, 54, 76, 50, 78}, 60, 70, 55, 2, 40, DS_PUMP, DSRC_KICK, 10, 20, {30, 30, 40, 100}},
    {"LO-FI", {KP_LOFIK, KP_LOFIS, KP_DIRTYC, KP_NOISEH, KP_808O, KP_NOISECR, KP_SHAKER, KP_TABA}, {104, 96, 88, 70, 64, 56, 74, 84},
     {64, 60, 70, 56, 56, 74, 50, 78}, 40, 60, 45, 2, 50, DS_EXP, DSRC_ROW, 0, 40, {30, 30, 50, 100}},
    {"VIDEO GAME", {KP_CHIPK, KP_CHIPS, KP_CHIPC, KP_CHIPH, KP_MARACA, KP_CHIPCR, KP_CHIPT, KP_LASER}, {104, 94, 88, 74, 70, 60, 86, 80},
     {64, 60, 70, 54, 74, 70, 48, 80}, 40, 50, 40, 1, 30, DS_EXP, DSRC_KICK, 0, 0, {0, 20, 40, 100}},
    {"TRAP", {KP_TRAPK, KP_TRAPS, KP_TRAPC, KP_TRAPH, KP_808O, KP_808CY, KP_TOMLO, KP_ZAP}, {120, 98, 94, 74, 66, 60, 86, 78},
     {64, 62, 66, 52, 56, 74, 48, 80}, 55, 75, 65, 2, 60, DS_EXP, DSRC_KICK, 20, 30, {20, 30, 50, 100}},
    {"DRUM & BASS", {KP_HARDK, KP_FMS, KP_909C, KP_909H, KP_909O, KP_RIDE, KP_FMT, KP_ZAP}, {112, 102, 88, 78, 70, 64, 84, 76},
     {64, 62, 70, 52, 54, 76, 48, 80}, 60, 65, 50, 2, 35, DS_PUMP, DSRC_KICK, 5, 30, {40, 15, 35, 100}},
};
#define DK_NKIT (sizeof DK_FKITS / sizeof DK_FKITS[0])
static uint8_t *dk_pid(uint32_t r) { r %= DK_N; return r < 2u ? &dkk.pid0[r] : &dkk.pid1[r - 2u]; }
/* row r a copy of drum preset p (its LEVEL and PAN kept), pid where it came from */
static void dk_row_set(uint32_t r, const dk_pre_t *p, uint32_t pid)
{
    dk_lane_t *k = &dkk.l[r % DK_N];
    uint8_t lv = k->level, pn = k->pan;
    *k = p->l;
    k->level = lv, k->pan = pn;
    dkk.fx[r % DK_N] = p->fx;
    *dk_pid(r) = (uint8_t)pid;
}
static void dk_sane(void);
static void dk_kit_load(uint32_t i)                /* factory kit i as the project's (the main loop's) */
{
    const dk_fkit_t *f;
    uint32_t r;
    if (i >= DK_NKIT)
        return;
    f = &DK_FKITS[i];
    dk_defaults();
    for (r = 0; r < DK_N; r++) {
        dkk.l[r].level = f->lvl[r], dkk.l[r].pan = f->pan[r];
        dk_row_set(r, &DKP[f->pre[r]], f->pre[r] + 1u);
    }
    dkk.accent = f->accent, dkk.vsens = f->vsens, dkk.amount = f->amount, dkk.attack = f->attack;
    dkk.release = f->release, dkk.shape = f->shape, dkk.dsrc = f->dsrc, dkk.dhold = f->dhold, dkk.atone = f->atone;
    memcpy(dkk.comp, f->comp, sizeof dkk.comp);
    memset(dkk.name, 0, sizeof dkk.name);
    memcpy(dkk.name, f->name, sizeof dkk.name - 1u);
    dkk.kit = (uint8_t)i;
    dk_sane();
}
/* the user's library (sy_app.c sy_dk_*: read at power-on, kept with each save and delete): the names (""
 * empty), a drum preset's category */
#define DK_NUP 64u
#define DK_NUK 16u
static struct {
    char name[12];
    uint8_t cat;
} dk_up[DK_NUP];
static char dk_uk[DK_NUK][13];
static dk_kit_t dk_rec;                            /* (a record's bytes on their way: a kit's, a drum preset's first 32) */
static const char *dk_up_name(uint32_t u) { return u < DK_NUP && dk_up[u].name[0] ? dk_up[u].name : 0; }
static const char *dk_uk_name(uint32_t u) { return u < DK_NUK && dk_uk[u][0] ? dk_uk[u] : 0; }
/* row r's name: the preset it came from, else its place's (DK_NAME) */
static const char *dk_rname(uint32_t r)
{
    uint32_t p;
    if (r >= DK_N)
        return DK_NAME[r % DK_ROWS];
    p = *dk_pid(r);
    if (p & 0x80u) {
        const char *n = dk_up_name(p & 0x7Fu);
        if (n)
            return n;
    } else if (p && p <= DK_NPRE) {
        return DKP[p - 1u].name;
    }
    return DK_NAME[r];
}
static uint32_t dk_row_cat(uint32_t r)             /* .. its category (its preset's, else its place's) */
{
    uint32_t p = *dk_pid(r % DK_N);
    if ((p & 0x80u) && dk_up_name(p & 0x7Fu))
        return dk_up[p & 0x7Fu].cat % DC_N;
    return p && p <= DK_NPRE ? DKP[p - 1u].cat : DK_ROWCAT[r % DK_N];
}

static float dk_t60(uint32_t lane, uint32_t d)     /* DECAY d's time to -60 dB, s (V0.87: the row's model's range) */
{
    uint32_t v, m = dk_model(lane, &v);
    const float *r = DM_T60[m][v];
    return r[0] * sy_exp2f(sy_log2f(r[1] / r[0]) * (float)d * 0.01f);
}
static float dk_kdec(float t60) { return sy_exp2f(-9.9658f / (t60 * SY_FS)); }   /* a sample's factor to fall by it */
static float dk_rel_s(void) { return 0.02f * sy_exp2f(5.6439f * (float)dkk.release * 0.01f); }   /* RELEASE: 20 ms .. 1 s */

/* ---------------------------------------------------------------------------------------------- voices --- */
typedef struct {
    uint8_t on, trk, mdl, var;                     /* sounding; the track it played for (its gains); V0.87 its model and
                                                    * variant, as the hit set them */
    uint32_t n, hold;                              /* samples since the hit; before it, never off (CLAP's bursts) */
    float g[2], tg[2];                             /* the hit's gains (level, velocity, accent, pan); the track's last */
    float a, ka;                                   /* the amplitude, a sample's factor */
    float b, kb;                                   /* a second envelope (KICK's sweep, SNARE's body, CLAP's burst, WT's
                                                    * bend) */
    float ph, ph2, f, f2;                          /* phases (turns) and their steps */
    float s1, s2, c1, c2, c3, k;                   /* a filter's state and coefficients */
    float p0, p1;                                  /* a model's own */
    uint32_t rnd;                                  /* noise */
    union {                                        /* V0.87 (D1): a model's own state, one a voice at a time: METAL's */
        struct {                                   /* squares, WAVE's table, CLAP's bursts, CHIP's noise register */
            uint32_t ph[6], inc[6];
        } m;
        struct {
            uint32_t ph, sh;                       /* WAVE: the phase, the shift to an index */
            const int16_t *f;                      /* .. the level's samples */
        } w;
        uint32_t next;                             /* CLAP: the next burst's sample */
        struct {
            uint32_t lfsr, tap;                    /* CHIP's NOISE: the register, its feedback's tap (1, or 6: short) */
        } c;
    } u;
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

/* a hit of row lane (DK_DUCK: the duck) for track trk, velocity vel (1 .. 127), accented. V0.87: the row's model */
static void sy_drum_hit(uint32_t lane, uint32_t trk, uint32_t vel, int acc)
{
    static const float M[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};   /* the TR-808's six */
    const dk_lane_t *k;
    dk_voice_t *v;
    float g, m, p, x, s, t60;
    uint32_t i, tone, var, mdl;
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
    mdl = dk_model(lane, &var);
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
    if (mdl == DM_WAVE) {                          /* (its table first: none, or being built, no hit) */
        const wt_table_t *t = &wt_tab[var & 1u];
        uint32_t fr, lv;
        float top = sy_exp2f(3.0f * (float)k->chr * 0.01f), f = 130.81f * m;
        if (!t->n || t->busy)
            return;
        fr = (uint32_t)sy_floori((float)tone * 0.01f * (float)(t->n - 1u) + 0.5f);
        lv = wt_level((uint32_t)sy_minf(f * top / SY_FS * 4294967296.0f, 4.0e9f));
        lv = lv < WT_NLEV ? lv : WT_NLEV - 1u;
        v->u.w.f = wt_frame(t, fr < t->n ? fr : t->n - 1u, lv);
        v->u.w.sh = 32u - WT_BITS[lv];
        v->u.w.ph = 0;
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
    v->mdl = (uint8_t)mdl, v->var = (uint8_t)var;
    switch (mdl) {
    case DM_PUNCH:
        if (var == 0u) {                           /* (KICK) */
            v->ph = 0.0f;
            v->f = 48.0f * m / SY_FS;
            v->p0 = 1.0f + 6.0f * (float)k->chr * 0.01f;   /* (the sweep's height: up to x7) */
            v->b = 1.0f;
            v->kb = dk_kdec(0.2f);
            v->p1 = 1.0f + 3.0f * (float)tone * 0.01f;  /* (the drive) */
            v->f2 = (float)tone * 0.004f;              /* (the click's level) */
        } else if (var == 1u) {                    /* (SNARE) */
            v->ph = v->ph2 = 0.0f;
            v->f = 185.0f * m / SY_FS;
            v->f2 = 330.0f * m / SY_FS;
            v->b = 1.0f;
            v->kb = dk_kdec(0.05f + 0.35f * t60);
            v->c1 = 1.0f - sy_exp2f(-9.0647f * 1000.0f * sy_exp2f(3.0f * (float)tone * 0.01f) / SY_FS);   /* (the
                                                           * high-pass's one-pole, 1 .. 8 kHz) */
            v->p0 = 0.25f + 0.75f * (float)k->chr * 0.01f;   /* (the noise) */
            v->p1 = 1.0f - 0.5f * (float)k->chr * 0.01f;     /* (the body) */
        } else {                                   /* (CLAP) */
            dk_svf(v, 1100.0f * m, 1.6f - 1.4f * (float)tone * 0.01f);
            v->p0 = (0.004f + 0.016f * (float)k->chr * 0.01f) * SY_FS;   /* (SPREAD: 4 .. 20 ms) */
            v->u.next = (uint32_t)v->p0;
            v->hold = 3u * (uint32_t)v->p0 + 1u;     /* (the fourth burst at 3 SPREAD, then the tail) */
            v->kb = dk_kdec(0.025f);
        }
        break;
    case DM_METAL: {                               /* (CLOSED, OPEN, CYMBAL) */
        float sc = var == 2u ? 0.85f : 1.0f, hz = (var == 2u ? 3000.0f : 5500.0f) *
                                                  sy_exp2f(1.2f * ((float)tone - 50.0f) * 0.02f);
        for (i = 0; i < 6u; i++)
            v->u.m.inc[i] = (uint32_t)(M[i] * sc * m / SY_FS * 4294967296.0f);
        v->p0 = (float)k->chr * 0.01f * (1.0f / 3.0f);   /* (the squares, -6 .. 6, as -2 .. 2 x METAL) */
        v->p1 = 1.0f - (float)k->chr * 0.01f;            /* (the noise) */
        dk_svf(v, hz, 1.4f);
        if (lane == DK_HATC && dkv[DK_HATO].on)
            dkv[DK_HATO].ka = dk_kdec(0.03f);      /* (the choke: the HAT CL row's hit ends the HAT OP row's) */
        break;
    }
    case DM_ANALOG:                                /* (a sine body swept down from DROP's height; SNAP its second body
                                                    * and noise: a kick, a tom, a conga, an 808 snare) */
        v->ph = v->ph2 = 0.0f;
        v->f = sy_minf(50.0f * m / SY_FS, 0.06f);
        v->f2 = v->f * 1.47f;
        v->p0 = 4.0f * (float)tone * 0.01f;        /* (the drop: up to x5) */
        v->b = 1.0f;
        v->kb = dk_kdec(0.1f);
        v->p1 = (float)k->chr * 0.01f;
        v->c1 = 1.0f - sy_exp2f(-9.0647f * 1500.0f / SY_FS);   /* (the noise's high-pass, 1.5 kHz) */
        break;
    case DM_FM: {                                  /* (RATIO the modulator's, INDEX its depth, falling with the hit;
                                                    * DRUM a pitch drop from x3 and a click, BELL neither) */
        static const float R[12] = {0.5f, 1.0f, 1.41f, 1.5f, 2.0f, 2.76f, 3.0f, 3.5f, 4.0f, 5.4f, 7.0f, 8.0f};
        v->ph = v->ph2 = 0.0f;
        v->f = sy_minf((var ? 240.0f : 60.0f) * m / SY_FS, 0.15f);
        v->f2 = sy_minf(v->f * R[tone * 12u / 101u], 0.15f);
        v->p1 = 0.96f * (float)k->chr * 0.01f;    /* (INDEX: up to 6 radians, as turns) */
        v->b = 1.0f;
        v->kb = dk_kdec(0.5f * t60 + 0.02f);
        v->s1 = var ? 0.0f : 1.0f;
        v->c1 = dk_kdec(0.06f);
        v->p0 = 2.0f;
        v->k = var ? 0.0f : 0.3f;
        break;
    }
    case DM_CHIP:                                  /* (SQUARE, TRIANGLE: swept down from SWEEP's height over SPEED;
                                                    * NOISE: the register clocked at PITCH, LOOP from 50 its short loop) */
        v->ph = 0.0f;
        if (var == 2u) {
            v->f = sy_minf(1.0f, 2000.0f * m * sy_exp2f(4.0f * (float)tone * 0.01f) / SY_FS);
            v->u.c.lfsr = 0x7FFFu;
            v->u.c.tap = k->chr >= 50u ? 6u : 1u;
        } else {
            v->f = sy_minf(110.0f * m / SY_FS, 0.05f);
            v->p0 = 7.0f * (float)k->chr * 0.01f;
            v->b = 1.0f;
            v->kb = dk_kdec(0.31f - 0.3f * (float)tone * 0.01f);
        }
        break;
    case DM_NOISE: {                               /* (a band-pass at FREQ, WIDTH: narrower; its level as the band's:
                                                    * noise's power goes as its width) */
        float hz = sy_minf(200.0f * m * sy_exp2f(6.0f * (float)tone * 0.01f), 0.45f * SY_FS);
        float kk = 2.0f - 1.9f * (float)k->chr * 0.01f, c = sy_minf(2.0f, 1.1f * sy_exp2f(-0.5f * sy_log2f(hz * 0.001f * kk)));
        dk_svf(v, hz, kk);
        v->g[0] *= c, v->g[1] *= c;
        break;
    }
    default:                                       /* (WAVE: above) */
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
            if (n == v->u.next) {
                a = 1.0f;
                v->u.next += (uint32_t)v->p0;
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
    uint32_t i, p0 = v->u.m.ph[0], p1 = v->u.m.ph[1], p2 = v->u.m.ph[2], p3 = v->u.m.ph[3], p4 = v->u.m.ph[4],
             p5 = v->u.m.ph[5];
    const uint32_t i0 = v->u.m.inc[0], i1 = v->u.m.inc[1], i2 = v->u.m.inc[2], i3 = v->u.m.inc[3], i4 = v->u.m.inc[4],
                   i5 = v->u.m.inc[5];
    int8_t sq[SY_BLK];
    float a = v->a, ic1 = v->s1, ic2 = v->s2;
    for (i = 0; i < SY_BLK; i++) {                 /* (two passes, each in the registers: the squares' sums (-6 .. 6), */
        p0 += i0, p1 += i1, p2 += i2, p3 += i3, p4 += i4, p5 += i5;   /* then the rest: V0.85's cost model) */
        sq[i] = (int8_t)(2 * (int32_t)((p0 >> 31) + (p1 >> 31) + (p2 >> 31) + (p3 >> 31) + (p4 >> 31) + (p5 >> 31)) - 6);
    }
    v->u.m.ph[0] = p0, v->u.m.ph[1] = p1, v->u.m.ph[2] = p2, v->u.m.ph[3] = p3, v->u.m.ph[4] = p4, v->u.m.ph[5] = p5;
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
static void dk_wt(dk_voice_t *v, float *x)
{
    const wt_table_t *t = &wt_tab[v->var & 1u];
    uint32_t i, inc, inc1, ph = v->u.w.ph, sh = v->u.w.sh;
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
        int32_t s0 = v->u.w.f[ix], s1 = v->u.w.f[ix + 1u];
        x[i] = (float)(s0 + (((s1 - s0) * (int32_t)fr) >> 15)) * (1.0f / 32768.0f) * a * fade;
        ph += inc;
        inc = (uint32_t)((int32_t)inc + dinc);
        a *= v->ka;
        fade += df;
    }
    if (t->busy)
        a = 0.0f;
    v->u.w.ph = ph, v->a = a, v->b = b1;
}
/* V0.87's models: ANALOG (a sine body and SNAP's second body and noise, a soft click), FM (two operators), CHIP */
static void dk_analog(dk_voice_t *v, float *x)
{
    uint32_t i;
    float ph = v->ph, ph2 = v->ph2, a = v->a, b = v->b, lp = v->s1, sn = v->p1, body = 1.0f - 0.5f * sn;
    for (i = 0; i < SY_BLK; i++) {
        float d = 1.0f + v->p0 * b, nz = dk_noise(&v->rnd), y;
        lp += (nz - lp) * v->c1;
        y = (sy_sin2pi(ph) + 0.6f * sn * sy_sin2pi(ph2)) * body + 0.6f * sn * (nz - lp);
        if (v->n + i < 44u)                        /* (the click: 1 ms) */
            y += 0.15f * nz * (1.0f - (float)(v->n + i) * (1.0f / 44.0f));
        x[i] = y * a;
        ph += v->f * d;
        ph2 += v->f2 * d;
        ph -= ph >= 1.0f ? 1.0f : 0.0f;
        ph2 -= ph2 >= 1.0f ? 1.0f : 0.0f;
        b *= v->kb;
        a *= v->ka;
    }
    v->ph = ph, v->ph2 = ph2, v->a = a, v->b = b, v->s1 = lp;
}
static void dk_fm(dk_voice_t *v, float *x)
{
    uint32_t i;
    float ph = v->ph, ph2 = v->ph2, a = v->a, b = v->b, pe = v->s1;
    for (i = 0; i < SY_BLK; i++) {
        float d = 1.0f + v->p0 * pe, y = sy_sin2pi(ph + v->p1 * b * sy_sin2pi(ph2));
        if (v->n + i < 66u)                        /* (DRUM's click: 1.5 ms of noise) */
            y += v->k * dk_noise(&v->rnd) * (1.0f - (float)(v->n + i) * (1.0f / 66.0f));
        x[i] = y * a;
        ph += v->f * d;
        ph2 += v->f2 * d;
        ph -= ph >= 1.0f ? 1.0f : 0.0f;
        ph2 -= ph2 >= 1.0f ? 1.0f : 0.0f;
        pe *= v->c1;
        b *= v->kb;
        a *= v->ka;
    }
    v->ph = ph, v->ph2 = ph2, v->a = a, v->b = b, v->s1 = pe;
}
static void dk_chip(dk_voice_t *v, float *x)
{
    uint32_t i;
    float ph = v->ph, a = v->a, b = v->b;
    for (i = 0; i < SY_BLK; i++) {
        float y, q = (float)(int32_t)(a * 15.0f + 0.5f) * (1.0f / 15.0f);   /* (the level in 16 steps) */
        if (v->var == 2u) {
            ph += v->f;
            if (ph >= 1.0f) {                      /* (the register clocked: a bit out) */
                uint32_t r = v->u.c.lfsr, fb = (r ^ (r >> v->u.c.tap)) & 1u;
                v->u.c.lfsr = (r >> 1) | (fb << 14);
                ph -= 1.0f;
            }
            y = (v->u.c.lfsr & 1u) ? 0.7f : -0.7f;
        } else {
            ph += v->f * (1.0f + v->p0 * b);
            ph -= ph >= 1.0f ? 1.0f : 0.0f;
            if (v->var == 0u) {
                y = ph < 0.5f ? 0.6f : -0.6f;
            } else {                               /* (the triangle in 16 steps) */
                float t = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
                y = (float)(int32_t)(t * 7.5f + 8.0f) * (1.0f / 7.5f) - 1.0f;
            }
            b *= v->kb;
        }
        x[i] = y * q;
        a *= v->ka;
    }
    v->ph = ph, v->a = a, v->b = b;
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
        switch (v->mdl) {                          /* (V0.87: the model the hit set) */
        case DM_PUNCH:
            if (v->var == 0u)
                dk_kick(v, x);
            else if (v->var == 1u)
                dk_snare(v, x);
            else
                dk_clap(v, x);
            break;
        case DM_NOISE: dk_clap(v, x); break;        /* (no bursts: CLAP's band-pass and tail) */
        case DM_WAVE: dk_wt(v, x); break;
        case DM_ANALOG: dk_analog(v, x); break;
        case DM_FM: dk_fm(v, x); break;
        case DM_CHIP: dk_chip(v, x); break;
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
