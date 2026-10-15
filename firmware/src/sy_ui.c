/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA's user interface on the FM-1, Phase 4 (docs/ARCHITECTURE.md §17; its look: the Strata Screen Design canvas).
 *   - Pages: a section's parameters four to a page, KNOB 1-4 left to right as on the panel, and what the display
 *     draws. A family button shows its sections' pages: a tap, the next page (EDIT: of the section ALGORITHM
 *     picked; the other families through all of theirs); ALGORITHM picks the family's section, SELECT steps its
 *     pages. HOME held: the MENU (COLOR: the palette; ACCEL: the knobs' feel).
 *   - Knobs: speed-sensitive (a slow turn 0.1 % a detent, a quick one up to 5 %; ACCEL OFF: 1 % a detent); a list,
 *     a count or a short range one value a detent.
 *   - The MOD layer: REC held (a double tap latches it, a tap lets it go). A white key picks the source, a knob
 *     sets that source's depth on its parameter: a matrix slot, made when needed and freed when the layer closes
 *     at 0. ENV held: the same for ENV 2 (an envelope onto a knob).
 *   - The screen in five regions (header, display, the four knob cells, footer), each drawn when it changes, one
 *     canvas at a time (sy_ui_draw.c); the LEDs (sy_ui_draw.c sy_ui_leds).
 * After the look stop: the LFO, FX, GLO and SCL layers, presets and SAVE, MIDI learn, help, the rest of the MENU,
 * the graphs of the filter, the LFOs, the matrix and the rack. Needs sy_app.c's keys and sounds (declared here),
 * panel.c, gfx.c, sy_gfx.c, the engine and sy_text.c. */

static int sy_app_held(uint32_t k);                 /* sy_app.c: key k's note sounding (its LED) */
static void sy_app_octave(int32_t d);               /* .. OCT- / OCT+ */
static int32_t sy_app_oct(void);
static void sy_app_sound_step(int32_t s);           /* .. PRESETS: the starter sounds */

/* ------------------------------------------------------------------------------------------- pages --- */
#define UI_ITEM 1000                                /* an item that is no parameter: the MENU's */
enum { UI_COLOR = UI_ITEM, UI_ACCEL };
#define SEC_MENU SEC_COUNT                          /* (a section of the UI's own) */
enum { G_LIST, G_WT3D, G_WT2D, G_UNI, G_ENV, G_COLOR };
typedef struct {
    uint8_t sec, graph;                             /* SEC_*, G_*: what the display draws */
    char name[8];                                   /* the page's name in the header */
    int16_t id[4];                                  /* KNOB 1-4: parameters (or UI_* items), -1 none */
} ui_page_t;
#define UI_OSC(O, S)                                                                                      \
    {S, G_WT3D, "WAVE", {SP_##O##_TABLE, SP_##O##_POS, SP_##O##_WARP, SP_##O##_WAMT}},                     \
    {S, G_UNI, "UNISON", {SP_##O##_UNI, SP_##O##_DETUNE, SP_##O##_BLEND, SP_##O##_WIDTH}},                \
    {S, G_UNI, "SPREAD", {SP_##O##_UMODE, SP_##O##_SPREAD, SP_##O##_PHASE, SP_##O##_RAND}},              \
    {S, G_WT2D, "PITCH", {SP_##O##_OCT, SP_##O##_SEMI, SP_##O##_FINE, SP_##O##_PHMODE}},                  \
    {S, G_WT2D, "OUTPUT", {SP_##O##_LEVEL, SP_##O##_PAN, SP_##O##_ROUTE, SP_##O##_ON}},
#define UI_ENV(E, S)                                                                                      \
    {S, G_ENV, "ADSR", {SP_##E##_ATK, SP_##E##_DEC, SP_##E##_SUS, SP_##E##_REL}},                          \
    {S, G_ENV, "SHAPE", {SP_##E##_HOLD, SP_##E##_ACRV, SP_##E##_DCRV, SP_##E##_RCRV}},
#define UI_LFO(L, S)                                                                                      \
    {S, G_LIST, "SHAPE", {SP_##L##_SHAPE, SP_##L##_SYNC, SP_##L##_RATE, SP_##L##_MODE}},                   \
    {S, G_LIST, "TIME", {SP_##L##_DELAY, SP_##L##_RISE, SP_##L##_PHASE, SP_##L##_SMOOTH}},
static const ui_page_t UI_PG[] = {
    UI_OSC(A, SEC_OSCA)
    UI_OSC(B, SEC_OSCB)
    {SEC_SUB, G_LIST, "MAIN", {SP_SUB_SHAPE, SP_SUB_OCT, SP_SUB_LEVEL, SP_SUB_PW}},
    {SEC_SUB, G_LIST, "OUTPUT", {SP_SUB_PAN, SP_SUB_ROUTE, SP_SUB_ON, -1}},
    {SEC_NOISE, G_LIST, "MAIN", {SP_NZ_TYPE, SP_NZ_COLOR, SP_NZ_STEREO, SP_NZ_LEVEL}},
    {SEC_NOISE, G_LIST, "OUTPUT", {SP_NZ_PAN, SP_NZ_ROUTE, SP_NZ_ON, -1}},
    {SEC_FILTER, G_LIST, "TONE", {SP_F_TYPE, SP_F_CUT, SP_F_RES, SP_F_DRIVE}},
    {SEC_FILTER, G_LIST, "MORE", {SP_F_KEY, SP_F_VAR, SP_F_MIX, SP_F_ON}},
    UI_ENV(E1, SEC_ENV1)
    UI_ENV(E2, SEC_ENV2)
    UI_LFO(L1, SEC_LFO1)
    UI_LFO(L2, SEC_LFO2)
    {SEC_VOICE, G_LIST, "VOICE", {SP_V_MODE, SP_V_POLY, SP_V_GLIDE, SP_V_GLMODE}},
    {SEC_VOICE, G_LIST, "PLAY", {SP_V_BUP, SP_V_BDN, SP_V_VEL, SP_V_VOL}},
    {SEC_MACRO, G_LIST, "MACROS", {SP_MAC1, SP_MAC2, SP_MAC3, SP_MAC4}},
    {SEC_MATRIX, G_LIST, "SLOT 1", {SP_M1_SRC, SP_M1_DST, SP_M1_AMT, -1}},
    {SEC_DRIVE, G_LIST, "DRIVE", {SP_DR_TYPE, SP_DR_AMT, SP_DR_TONE, SP_DR_MIX}},
    {SEC_DRIVE, G_LIST, "POWER", {SP_DR_ON, -1, -1, -1}},
    {SEC_CHORUS, G_LIST, "CHORUS", {SP_CH_RATE, SP_CH_DEPTH, SP_CH_DELAY, SP_CH_MIX}},
    {SEC_CHORUS, G_LIST, "MORE", {SP_CH_FDBK, SP_CH_ON, -1, -1}},
    {SEC_DELAY, G_LIST, "DELAY", {SP_DL_MODE, SP_DL_SYNC, SP_DL_TIME, SP_DL_FDBK}},
    {SEC_DELAY, G_LIST, "TONE", {SP_DL_LP, SP_DL_HP, SP_DL_MIX, SP_DL_ON}},
    {SEC_REVERB, G_LIST, "REVERB", {SP_RV_TYPE, SP_RV_SIZE, SP_RV_DECAY, SP_RV_MIX}},
    {SEC_REVERB, G_LIST, "TONE", {SP_RV_DAMP, SP_RV_PRE, SP_RV_WIDTH, SP_RV_LOCUT}},
    {SEC_REVERB, G_LIST, "POWER", {SP_RV_ON, -1, -1, -1}},
    {SEC_OTT, G_LIST, "OTT", {SP_OT_DEPTH, SP_OT_TIME, SP_OT_UP, SP_OT_DOWN}},
    {SEC_OTT, G_LIST, "BANDS", {SP_OT_LOW, SP_OT_MID, SP_OT_HIGH, SP_OT_ON}},
    {SEC_MENU, G_COLOR, "SCREEN", {UI_COLOR, UI_ACCEL, -1, -1}},
};
#define UI_NPG (sizeof UI_PG / sizeof UI_PG[0])

/* the families: the sections each button shows (0xFF ends) */
enum { FAM_HOME, FAM_EDIT, FAM_ENV, FAM_LFO, FAM_SEQ, FAM_FX, FAM_GLO, FAM_MENU, FAM_N };
static const uint8_t FAM_SECS[FAM_N][6] = {
    {SEC_MACRO, 0xFF}, {SEC_OSCA, SEC_OSCB, SEC_SUB, SEC_NOISE, SEC_FILTER, 0xFF}, {SEC_ENV1, SEC_ENV2, 0xFF},
    {SEC_LFO1, SEC_LFO2, 0xFF}, {SEC_MATRIX, 0xFF}, {SEC_DRIVE, SEC_CHORUS, SEC_DELAY, SEC_REVERB, SEC_OTT, 0xFF},
    {SEC_VOICE, 0xFF}, {SEC_MENU, 0xFF},
};
static const uint8_t FAM_BTN[FAM_N] = {B_HOME, B_EDIT, B_ENV, B_LFO, B_SEQ, B_FX, B_GLO, NB};

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
#define UI_DTAP_MS 300u                             /* a second tap within this: a double tap (it latches) */
static struct {
    uint16_t page;
    uint16_t fpage[FAM_N];                          /* the page a family returns to */
    uint16_t spage[SEC_COUNT + 1];                  /* the page a section returns to (ALGORITHM) */
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
    uint16_t prev_page;                             /* the page before the last tap (a double tap puts it back) */
    uint32_t en_t[NE];                              /* an encoder's last turn (the speed) */
    int8_t en_dir[NE];
    char toast[24];                                 /* a word in the footer, until toast_t */
    uint32_t toast_t;
    uint32_t live_t;                                /* a live graph's last frame */
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
static const char *ui_sec_name(uint32_t s) { return s < SEC_COUNT ? SEC_NAMES[s] : "MENU"; }

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
                if (UI_PG[ui.page].id[k] >= 0 && sy_patch.p[SP_M1_DST + 3u * s] == UI_PG[ui.page].id[k])
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
    ui.spage[UI_PG[page].sec] = (uint16_t)page;
    ui.dirty = D_ALL;
}

/* a family button's tap: its page (another family), else the next of the section (EDIT) or of the family */
static void ui_family_tap(uint32_t f)
{
    uint32_t p = ui.page, n;
    if (ui.fam != f) {
        ui_go(ui_in_fam(ui.fpage[f], f) ? ui.fpage[f] : ui_first_page(FAM_SECS[f][0]));
        return;
    }
    for (n = 0; n < UI_NPG; n++) {
        p = (p + 1u) % UI_NPG;
        if (f == FAM_EDIT ? UI_PG[p].sec == ui_sec() : ui_in_fam(p, f))
            break;
    }
    ui_go(p);
}

/* SELECT: the family's pages */
static void ui_step_page(int32_t s)
{
    uint32_t p = ui.page, n;
    while (s) {
        for (n = 0; n < UI_NPG; n++) {
            p = s > 0 ? (p + 1u) % UI_NPG : (p + UI_NPG - 1u) % UI_NPG;
            if (ui_in_fam(p, ui.fam))
                break;
        }
        s += s > 0 ? -1 : 1;
    }
    ui_go(p);
}

/* ALGORITHM: the family's sections (each on the page it showed last) */
static void ui_step_section(int32_t s)
{
    uint32_t n = 0, k = 0, sec = ui_sec(), next;
    while (FAM_SECS[ui.fam][n] != 0xFF) {
        if (FAM_SECS[ui.fam][n] == sec)
            k = n;
        n++;
    }
    if (n < 2u)
        return;
    k = (uint32_t)(((int32_t)k + s % (int32_t)n + (int32_t)n) % (int32_t)n);
    next = FAM_SECS[ui.fam][k];
    ui_go(UI_PG[ui.spage[next]].sec == next ? ui.spage[next] : ui_first_page(next));
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

/* the MOD layer: source pick's depth on parameter id, by s steps */
static void ui_mod_turn(uint32_t id, int32_t s, uint32_t e)
{
    uint32_t k;
    int32_t free = -1, slot = -1, amt;
    if (!(SP[id].flags & F_MOD))
        return;
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

/* the MENU's items */
static void ui_item_turn(int32_t item, int32_t s)
{
    if (item == UI_COLOR) {
        ui.pal = (uint8_t)((ui.pal + (uint32_t)(s % (int32_t)SY_NPAL + (int32_t)SY_NPAL)) % SY_NPAL);
        sy_palette(ui.pal);
        ui.dirty = D_ALL;
    } else if (item == UI_ACCEL) {
        ui.accel = s > 0;
        ui.dirty |= D_KNOBS | D_DISP;
    }
}

static void ui_knob(uint32_t k, int32_t s)
{
    int32_t id = UI_PG[ui.page].id[k];
    const sp_desc_t *d;
    if (id < 0)
        return;
    if (id >= UI_ITEM) {
        ui_item_turn(id, s);
        return;
    }
    if (ui.mod) {
        ui_mod_turn((uint32_t)id, s, EN_K1 + k);
        return;
    }
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
    sy_patch.p[id] = sp_clamp((uint32_t)id, sy_patch.p[id] + s);
    ui.dirty |= D_KNOB(k) | D_DISP;
    if (ui_sec_power(ui_sec()) == id || UI_PG[ui.page].graph == G_WT3D || UI_PG[ui.page].graph == G_WT2D)
        ui.dirty |= D_HEAD;                         /* (the light; OCT SEM FIN) */
}

/* a parameter changed from elsewhere (MIDI, a sound): what shows it */
static void sy_ui_touch(void) { ui.dirty |= D_KNOBS | D_DISP | D_HEAD | D_FOOT; }

/* ---------------------------------------------------------------------------------------- buttons --- */
static int ui_has_layer(uint32_t b) { return b == B_REC || b == B_ENV; }

static void ui_press(uint32_t b)
{
    if (b == B_OCTDN || b == B_OCTUP) {
        char t[16] = "KEYS OCT ";
        sy_app_octave(b == B_OCTUP ? 1 : -1);
        sp_fix(t + 9, sy_app_oct(), 0u, 1);
        ui_toast(t);
    }
}

static void ui_hold(uint32_t b)                     /* held past HOLD: its layer */
{
    if (b == B_REC && !ui.mod)
        ui_mod_open(B_REC, ui.mod_pick, 0);
    else if (b == B_ENV && !ui.mod)
        ui_mod_open(B_ENV, SRC_ENV2, 0);
    else if (b == B_HOME)
        ui_go(ui_first_page(SEC_MENU));
}

static void ui_release(uint32_t b, int held)
{
    uint32_t f;
    if (held) {                                     /* a held layer closes (a latched one stays) */
        if (ui.mod == 1u && ui.mod_btn == b)
            ui_mod_close();
        return;
    }
    if (ui_has_layer(b) && fm1_ms - ui.tap_t[b] < UI_DTAP_MS && !ui.mod) {   /* a double tap: latched */
        if (b == B_ENV)
            ui_go(ui.prev_page);                    /* (the first tap's page step undone) */
        ui_mod_open(b, b == B_ENV ? SRC_ENV2 : ui.mod_pick, 1);
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
            ui.bt_t[b] = fm1_ms;
            ui_press(b);
        }
        if ((ui.bt_down & bit) && !(ui.bt_held & bit) && fm1_ms - ui.bt_t[b] >= HOLD_MS[settings_hold % 4u]) {
            ui.bt_held |= (uint16_t)bit;
            ui_hold(b);
        }
        if ((rel >> panel.btn[b]) & 1u) {
            int held = !!(ui.bt_held & bit);
            ui.bt_down &= (uint16_t)~bit;
            ui.bt_held &= (uint16_t)~bit;
            ui_release(b, held);
        }
    }
}

/* the keys in REC's MOD layer: a white key picks its source, no key plays; 1 = taken (no note). (ENV's layer:
 * the keys play, to hear what ENV 2 does) */
static int sy_ui_key(uint32_t k)
{
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
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0)
            ui_knob(k, s);
    if ((s = panel_enc(EN_SELECT)) != 0)
        ui_step_page(s);
    if ((s = panel_enc(EN_ALGO)) != 0)
        ui_step_section(s);
    if ((s = panel_enc(EN_PRESET)) != 0)
        sy_app_sound_step(s);
    if (ui.toast[0] && (int32_t)(fm1_ms - ui.toast_t) >= 0) {
        ui.toast[0] = 0;
        ui.dirty |= D_FOOT;
    }
}

/* panel.c's settings hooks: the palette (MENU > COLOR), their save (Phase 4, part 2: flash) */
static void palette_apply(void) { sy_palette(ui.pal); }
static void settings_save(void) {}

static void sy_ui_boot(void)
{
    ui.pal = 0;                                     /* STRATA */
    ui.accel = 1;
    ui.mod_pick = SRC_LFO1;
    sy_palette(ui.pal);
    ui_go(ui_first_page(SEC_OSCA));
}
