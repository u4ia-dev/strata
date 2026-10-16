/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA's screen: sy_ui.c's five regions, each drawn on gfx.c's canvas with sy_gfx.c's shapes and sent on its own:
 *   header 240 x 22 at (0, 0): the section, its power light, the page; at the right the oscillator's OCT SEM FIN,
 *     an envelope's or LFO's tabs, the effects that are on, the sources into the filter; HOME: the tempo, the voices
 *     sounding, the load; in the MOD layer the banner (the source, what it set);
 *   display 240 x 114 at (0, 22): the window (228 x 110) and the page's graph: HOME's sound and its output live (a
 *     scope), the table in 3D (its frames warped as they play, the current one, the range its modulation sweeps),
 *     one frame in 2D (warped, the unwarped one behind) with the unison's voices, an envelope (the other one behind,
 *     the voices on it), an LFO's cycle (its points, the one picked, the segment KNOB 4 curves; where the voices
 *     are), the tempo and the time signature (a bar's beats), the filter's and the effects' (sy_ui_fx.c), a list of
 *     the section, the palettes;
 *   knob cells 60 x 76 at (60 k, 136): a dial (the value's arc, the modulation's halos, the pointer) or a list's pill,
 *     the name, the value (the MOD layer: the picked source's depth); the knob turning in NOW, its value bold;
 *   footer 240 x 28 at (0, 212): the mod sources' chips (lit: moving something on this page; the MOD layer's pick
 *     filled), or a word for a moment, or a hint (the SETTINGS, an LFO's POINTS);
 * and sy_ui_frame: what changed, the live graphs (voices on an envelope or an LFO, the scope, the chorus's taps,
 * OTT's levels, a table being built), the knob turning let go, the LEDs. */

#define R_DISP_Y 22
#define R_KNOB_Y 136
#define R_FOOT_Y 212
#define WX 7                                        /* the window's inside, in the display's canvas */
#define WY 4
#define WW 226
#define WH 108
static void ui_g_filter(void);                      /* sy_ui_fx.c: the filter's and the effects' graphs */
static void ui_g_drive(void);
static void ui_g_chorus(void);
static void ui_g_delay(void);
static void ui_g_reverb(void);
static void ui_g_ott(void);
static void ui_g_eq(void);
static void ui_flag(int32_t x, int32_t y, const char *s);
static int ui_eq_modded(void);
static void ui_g_save(void);                        /* sy_ui_save.c: SAVE's screen, the browser */
static void ui_g_browse(void);
static void ui_save_cell(int32_t id, int act);
static void ui_save_head(char *t);
static int ui_save_renaming(void);

/* sound playing: a voice on, or the output still over -70 dBFS (a delay's or a reverb's tail). The live graphs move
 * only while it does; otherwise they stand still and nothing is redrawn */
static int ui_sounding(void)
{
    uint32_t i;
    for (i = 0; i < SY_NVOICE; i++)
        if (sy_v[i].on)
            return 1;
    return sy_out_pk > 10u;
}

/* s centred in w px from x0 (y: its line top), cut with an ellipsis when it is wider */
static void ui_text_c(int32_t x0, int32_t y, int32_t w, const aafont_t *f, const char *s, uint16_t fg, uint16_t bg)
{
    char b[48];
    text_fit(b, sizeof b, s, f, w);
    cv_text_in(x0, y, w, f, b, fg, bg);
}

/* a depth as text: "+20.0%" */
static char *ui_pct(char *b, int32_t v) { return sp_put(sp_fix(b, v, 1u, 1), "%"); }

/* a slot's depth of source src on parameter id (0: none) */
static int32_t ui_depth(uint32_t src, uint32_t id)
{
    uint32_t s;
    for (s = 0; s < NSLOT; s++)
        if (sy_patch.p[SP_M1_SRC + 3u * s] == (int32_t)src && sy_patch.p[SP_M1_DST + 3u * s] == (int32_t)id)
            return sy_patch.p[SP_M1_AMT + 3u * s];
    return 0;
}

/* --------------------------------------------------------------------------------------- header --- */
static int32_t ui_ld_shown = -1;                    /* HOME's load as last drawn (-1: none) */
static int32_t ui_tab(int32_t xr, const char *s, int on)   /* a tab ending at xr; returns its left */
{
    int32_t w = text_w(&AF_XS, s) + 10, x = xr - w;
    cv_rrect(x, 4, w, 14, 3, on ? sk.rline2 : sk.rule, sk.ground);
    cv_rrect(x + 1, 5, w - 2, 12, 2, on ? sk.raised : sk.ground, on ? sk.rline2 : sk.rule);
    ui_text_c(x, 4 + CAP_IN(XS, 14), w, &AF_XS, s, on ? sk.text : sk.dim, on ? sk.raised : sk.ground);
    return x;
}
static int32_t ui_readout(int32_t xr, const char *lab, int32_t v)   /* "OCT 0" ending at xr; returns its left */
{
    char t[8];
    int32_t y = CAP_IN(XS, 21);
    sp_fix(t, v, 0u, v != 0);
    xr = cv_text_r(xr, y, &AF_XS, t, sk.text, sk.ground) - 3;
    return cv_text_r(xr, y, &AF_XS, lab, sk.dim, sk.ground);
}

static void ui_draw_head(void)
{
    const ui_page_t *pg = &UI_PG[ui.page];
    char t[32], *e;
    uint32_t n, of, s;
    int32_t x, pw;
    if (ui.lay) {                                   /* a hold layer's banner: its name, what it does */
        static const char *const NAME[5] = {"", "PERFORM", "MACROS", "MOMENTARY", "JUMP"};
        if (ui.lay == LAY_MOM) {
            if (ui.mom_n)
                sp_put(sp_fix(t, ui.mom_n, 0u, 0), " CHANGED");
            else
                sp_put(t, "A KNOB: FOR NOW");
        } else {
            sp_put(t, ui.lay == LAY_PERF ? "KNOB 1 BEND \xB7 2 WHEEL" : ui.lay == LAY_MAC ? "KEYS: THE EFFECTS"
                                                                                 : "A KEY: ITS SECTION");
        }
        cv_begin(240, 22, sk.banner);
        cv_text_on(8, CAP_IN(SB, 22), &AF_SB, NAME[ui.lay % 5u], sk.bink, sk.banner);
        cv_text_r(232, CAP_IN(XS, 22), &AF_XS, t, sk.bsub, sk.banner);
        cv_blit(0, 0);
        return;
    }
    if (ui.mod) {                                   /* the MOD banner */
        const char *src = SRC_NAMES[ui.mod_src];
        int32_t w = text_w(&AF_XS, src) + 10;
        cv_begin(240, 22, sk.banner);
        x = cv_text_on(8, CAP_IN(SB, 22), &AF_SB, "MOD", sk.bink, sk.banner) + 6;
        cv_rrect(x, 4, w, 14, 3, sk.bink, sk.banner);
        ui_text_c(x, 4 + CAP_IN(XS, 14), w, &AF_XS, src, sk.banner, sk.bink);
        if (ui.mod_last >= 0) {
            e = sp_put(t, SP[ui.mod_last].name);
            e = sp_put(e, " ");
            ui_pct(e, ui_depth(ui.mod_src, (uint32_t)ui.mod_last));
        } else {
            sp_put(t, ui.mod_btn == B_REC ? "KEY: SOURCE  KNOB: DEPTH" : "KNOB: DEPTH");
        }
        cv_text_r(232, CAP_IN(XS, 22), &AF_XS, t, ui.mod_last >= 0 ? sk.bink : sk.bsub, sk.banner);
        cv_blit(0, 0);
        return;
    }
    cv_begin(240, 22, sk.ground);
    cv_rect(0, 21, 240, 1, sk.rule);
    if (pg->graph == G_HOME) {                      /* HOME: the tempo, the voices sounding, the load */
        int32_t y = CAP_IN(XS, 21), ld = ui_ld_shown = sy_app_load();
        for (n = 0, s = 0; s < SY_NVOICE; s++)
            n += sy_v[s].on != 0;
        cv_text_on(8, CAP_IN(SB, 21), &AF_SB, "HOME", sk.text, sk.ground);
        x = 232;
        if (ld >= 0) {
            sp_put(sp_fix(t, ld, 0u, 0), "%");
            x = cv_text_r(x, y, &AF_XS, t, sk.text, sk.ground) - 3;
            x = cv_text_r(x, y, &AF_XS, "CPU", sk.dim, sk.ground) - 8;
        }
        x = cv_text_r(x, y, &AF_XS, n == 1u ? "VOICE" : "VOICES", sk.dim, sk.ground) - 3;
        sp_fix(t, (int32_t)n, 0u, 0);
        x = cv_text_r(x, y, &AF_XS, t, sk.text, sk.ground) - 8;
        x = cv_text_r(x, y, &AF_XS, "BPM", sk.dim, sk.ground) - 3;
        sp_fix(t, ui.bpm, 0u, 0);
        cv_text_r(x, y, &AF_XS, t, sk.text, sk.ground);
        cv_blit(0, 0);
        return;
    }
    x = 8;
    if ((pw = ui_sec_power(pg->sec)) >= 0) {        /* the power light */
        if (sy_patch.p[pw]) {
            cv_rrect(8, 8, 6, 6, 2, sk.sound, sk.ground);
        } else {
            cv_rrect(8, 8, 6, 6, 2, sk.faint, sk.ground);
            cv_rrect(9, 9, 4, 4, 1, sk.ground, sk.faint);
        }
        x = 19;
    }
    x = cv_text_on(x, CAP_IN(SB, 21), &AF_SB, ui_sec_name(pg->sec), sk.text, sk.ground) + 6;
    {                                               /* the right first: what the section shows there */
        int32_t r = 232;
        switch (pg->sec) {
        case SEC_OSCA:
        case SEC_OSCB: {
            uint32_t b = pg->sec == SEC_OSCB ? (uint32_t)(SP_B_ON - SP_A_ON) : 0u;
            r = ui_readout(232, "FIN", sy_patch.p[SP_A_FINE + b]) - 6;
            r = ui_readout(r, "SEM", sy_patch.p[SP_A_SEMI + b]) - 6;
            r = ui_readout(r, "OCT", sy_patch.p[SP_A_OCT + b]);
            break;
        }
        case SEC_ENV1:
        case SEC_ENV2:
            r = ui_tab(ui_tab(232, "ENV 2", pg->sec == SEC_ENV2) - 3, "ENV 1", pg->sec == SEC_ENV1);
            break;
        case SEC_LFO1:
        case SEC_LFO2:
            r = ui_tab(ui_tab(232, "LFO 2", pg->sec == SEC_LFO2) - 3, "LFO 1", pg->sec == SEC_LFO1);
            break;
        case SEC_DRIVE:
        case SEC_CHORUS:
        case SEC_DELAY:
        case SEC_REVERB:
        case SEC_OTT:
        case SEC_EQ:
            for (n = 0, s = SEC_DRIVE; s < SEC_DRIVE + UI_NFX; s++)
                n += sy_patch.p[ui_sec_power(s)] != 0;
            r = cv_text_r(232, CAP_IN(XS, 21), &AF_XS, " OF 6 ON", sk.dim, sk.ground);
            sp_fix(t, (int32_t)n, 0u, 0);
            r = cv_text_r(r, CAP_IN(XS, 21), &AF_XS, t, sk.text, sk.ground);
            break;
        case SEC_KEYS:                              /* the keys' octave (OCT- / OCT+) */
            sp_fix(sp_put(t, "OCT "), sy_app_oct(), 0u, sy_app_oct() != 0);
            r = cv_text_r(232, CAP_IN(XS, 21), &AF_XS, t, sk.dim, sk.ground);
            break;
        case SEC_MATRIX:                            /* the slots in use (a source and an amount) */
            for (n = 0, s = 0; s < NSLOT; s++)
                n += sy_patch.p[SP_M1_SRC + 3u * s] > SRC_NONE && sy_patch.p[SP_M1_AMT + 3u * s] != 0;
            r = cv_text_r(232, CAP_IN(XS, 21), &AF_XS, " OF 8 IN USE", sk.dim, sk.ground);
            sp_fix(t, (int32_t)n, 0u, 0);
            r = cv_text_r(r, CAP_IN(XS, 21), &AF_XS, t, sk.text, sk.ground);
            break;
        case SEC_SAVE:                              /* SAVE: the slot it goes to; the browser: the category, the */
        case SEC_BROWSE:                            /* place in it */
            ui_save_head(t);
            r = cv_text_r(232, CAP_IN(XS, 21), &AF_XS, t, sk.dim, sk.ground);
            break;
        case SEC_FILTER: {                          /* the sources through it: lit */
            static const uint16_t ON[4] = {SP_A_ON, SP_B_ON, SP_SUB_ON, SP_NZ_ON};
            static const uint16_t RT[4] = {SP_A_ROUTE, SP_B_ROUTE, SP_SUB_ROUTE, SP_NZ_ROUTE};
            static const char *const CH[4] = {"A", "B", "S", "N"};
            for (s = 0; s < 4u; s++) {
                int on = sy_patch.p[ON[s]] && !sy_patch.p[RT[s]];
                int32_t cx = 232 - 13 - (int32_t)(3u - s) * 16;
                cv_rrect(cx, 4, 13, 13, 2, on ? sk.wmid : sk.track, sk.ground);
                cv_rrect(cx + 1, 5, 11, 11, 2, on ? sk.wtint : sk.ground, on ? sk.wmid : sk.track);
                ui_text_c(cx, 4 + CAP_IN(XS, 13), 13, &AF_XS, CH[s], on ? sk.sound : sk.faint,
                          on ? sk.wtint : sk.ground);
            }
            r = 232 - 4 * 16 + 3;
            break;
        }
        }
        ui_page_of(ui.page, &n, &of);               /* the page, and its number where there is room */
        e = pg->sec != SEC_SAVE ? sp_put(t, pg->name) : sp_put(t, ui_save_renaming() ? "RENAME" : "NAME IT");
        if (pg->sec == SEC_SAVE && ui.sv_sym)
            e = sp_put(e, " \xB7 123");
        if (of > 1u) {
            e = sp_fix(sp_put(e, " "), (int32_t)n, 0u, 0);
            sp_fix(sp_put(e, "/"), (int32_t)of, 0u, 0);
            if (text_w(&AF_XS, t) > r - 8 - x)
                sp_put(t, pg->name);
        }
        cv_text_on(x, CAP_IN(XS, 21), &AF_XS, t, sk.sub, sk.ground);
    }
    cv_blit(0, 0);
}

/* ----------------------------------------------------------------------------------- knob cells --- */
/* a list's value in its pill, the arrows each side (a long value: the whole pill, no arrows); act: turning (NOW) */
static void ui_pill(const char *s, int dim, int act)
{
    uint16_t bd = dim ? sk.rule : act ? sk.now : sk.rline, bg = dim ? sk.pdim : sk.raised, ch = dim ? sk.faint : sk.dim;
    uint16_t fg = dim ? sk.faint : act ? sk.now : sk.text;
    cv_rrect(3, 13, 54, 22, 4, bd, sk.ground);
    cv_rrect(4, 14, 52, 20, 3, bg, bd);
    if (text_w(&AF_XS, s) > 36) {
        ui_text_c(5, 14 + CAP_IN(XS, 20), 50, &AF_XS, s, fg, bg);
        return;
    }
    cv_seg(8.5f, 20.5f, 6.0f, 24.0f, 1.2f, ch);
    cv_seg(6.0f, 24.0f, 8.5f, 27.5f, 1.2f, ch);
    cv_seg(51.5f, 20.5f, 54.0f, 24.0f, 1.2f, ch);
    cv_seg(54.0f, 24.0f, 51.5f, 27.5f, 1.2f, ch);
    ui_text_c(9, 14 + CAP_IN(XS, 20), 42, &AF_XS, s, fg, bg);
}

/* a dial at t (0..1): the value's arc (bip: from the middle), the knob, the pointer; act: turning (NOW) */
static void ui_dial_at(float t, int bip, int dim, int act)
{
    float dx, dy;
    cv_arc(30.0f, 24.0f, 14.0f, 3.0f, bip ? 0.5f : 0.0f, t, dim ? sk.faint : act ? sk.now : sk.value);
    cv_disc(30.0f, 24.0f, 10.0f, act ? sk.now : sk.dl);
    cv_disc(30.0f, 24.0f, 9.0f, sk.disc);
    sy_dial_dir(t, &dx, &dy);
    cv_seg(30.0f + 3.5f * dx, 24.0f + 3.5f * dy, 30.0f + 8.5f * dx, 24.0f + 8.5f * dy, 1.6f,
           dim ? sk.dim : act ? sk.now : sk.text);
}

/* a cell's value under its dial: the knob turning in NOW and bold */
static void ui_cell_value(const char *v, uint16_t c, int act)
{
    ui_text_c(0, 58, 60, act ? &AF_SB : &AF_S, v, act ? sk.now : c, sk.ground);
}

static void ui_dial(uint32_t id, int dim, int act)  /* id's value: its arc, its modulation, the pointer */
{
    const sp_desc_t *d = &SP[id];
    float t = (float)(sy_patch.p[id] - d->lo) / (float)(d->hi - d->lo), olo, ohi, plo, phi;
    cv_arc(30.0f, 24.0f, 14.0f, 3.0f, 0.0f, 1.0f, sk.track);
    ui_mod_span(id, ui.mod ? ui.mod_src : SRC_COUNT, &olo, &ohi, &plo, &phi);
    if (ohi > olo)                                  /* the others (the MOD layer: grey under the pick's) */
        cv_arc(30.0f, 24.0f, 19.0f, 2.5f, t + olo, t + ohi, ui.mod ? sk.modoff : ohi > 0.0f ? sk.mod : sk.modneg);
    if (phi > plo)
        cv_arc(30.0f, 24.0f, 19.0f, 2.5f, t + plo, t + phi, phi > 0.0f ? sk.mod : sk.modneg);
    ui_dial_at(t, d->lo < 0, dim, act);
}

/* an LFO's POINTS page, KNOB 1-4: the point picked of the points (a pill), its place, its level, the curve from it
 * (the last point: into it); the place dim for the first and the last (they stay at the ends) */
static void ui_lfo_cell(int32_t id, int act)
{
    static const char *const NAME[4] = {"POINT", "TIME", "LEVEL", "CURVE"};
    uint32_t k = ui_lfo(), i;
    sp_draw_t tp;
    const sp_draw_t *d = ui_pts(k, &tp);
    char t[16];
    int dim = ui.mod != 0;
    i = ui_lpt(k, d);
    if (id == UI_LPT) {
        sp_fix(sp_put(sp_fix(t, (int32_t)i + 1, 0u, 0), "/"), d->n, 0u, 0);
        ui_pill(t, dim, act);
    } else {
        int32_t v = id == UI_LX ? d->x[i] : id == UI_LY ? d->y[i] : d->c[i + 1u < d->n ? i : i - 1u];
        int still = dim || (id == UI_LX && (i == 0u || i + 1u == d->n));
        cv_arc(30.0f, 24.0f, 14.0f, 3.0f, 0.0f, 1.0f, sk.track);
        ui_dial_at(id == UI_LX ? (float)v * 0.001f : (float)(v + 1000) * 0.0005f, id != UI_LX, still, act && !still);
        sp_put(sp_fix(t, v, 1u, id != UI_LX), "%");
        ui_cell_value(t, still ? sk.dim : sk.text, act && !still);
    }
    ui_text_c(0, 46, 60, &AF_XS, NAME[id - UI_LPT], dim ? sk.faint : act ? sk.now : sk.label, sk.ground);
}

/* GLOBAL, KNOB 1-3: BPM (a dial), TS-BEAT, TS-NOTE (pills) */
static void ui_tempo_cell(int32_t id, int act)
{
    static const char *const NAME[3] = {"BPM", "TS-BEAT", "TS-NOTE"};
    char t[8];
    int dim = ui.mod != 0;
    if (id == UI_BPM) {
        cv_arc(30.0f, 24.0f, 14.0f, 3.0f, 0.0f, 1.0f, sk.track);
        ui_dial_at((float)(ui.bpm - 20u) * (1.0f / 280.0f), 0, dim, act);
        sp_fix(t, ui.bpm, 0u, 0);
        ui_cell_value(t, dim ? sk.dim : sk.text, act);
    } else {
        sp_fix(t, id == UI_TSB ? ui.tsb : UI_TSN_VAL[ui.tsn], 0u, 0);
        ui_pill(t, dim, act);
    }
    ui_text_c(0, 46, 60, &AF_XS, NAME[id - UI_BPM], dim ? sk.faint : act ? sk.now : sk.label, sk.ground);
}

/* PERFORM's cells: the bend (both ways from the middle, in semitones) and the mod wheel, GLIDE and VOLUME; MACROS':
 * MACRO 1-4 under their names */
static void ui_lay_cell(uint32_t k)
{
    char t[SP_TEXT_N + 4];
    if (ui.lay == LAY_PERF && k == 0u) {
        int32_t st = ui.bend * (ui.bend >= 0 ? sy_patch.p[SP_V_BUP] : sy_patch.p[SP_V_BDN]) * 10 / 8192;
        ui_dial_at(0.5f + (float)ui.bend * (0.5f / 8192.0f), 1, 0, 0);
        sp_put(sp_fix(t, st, 1u, st != 0), " ST");
        ui_cell_value(t, sk.text, 0);
        ui_text_c(0, 46, 60, &AF_XS, "BEND", sk.label, sk.ground);
    } else if (ui.lay == LAY_PERF && k == 1u) {
        ui_dial_at((float)ui.wheel * (1.0f / 127.0f), 0, 0, 0);
        sp_fix(t, ui.wheel, 0u, 0);
        ui_cell_value(t, sk.text, 0);
        ui_text_c(0, 46, 60, &AF_XS, "WHEEL", sk.label, sk.ground);
    } else {
        uint32_t id = ui.lay == LAY_MAC ? SP_MAC1 + k : k == 2u ? SP_V_GLIDE : SP_V_VOL;
        ui_dial(id, 0, 0);
        sp_text(id, sy_patch.p[id], t);
        ui_cell_value(t, sk.text, 0);
        ui_text_c(0, 46, 60, &AF_XS, ui.lay == LAY_MAC ? sy_patch.macro[k] : SP[id].name, sk.label, sk.ground);
    }
}

/* SCL's cells: the scale, the root, the fit (pills), the keys' velocity (a dial) */
static void ui_keys_cell(int32_t id, int act)
{
    static const char *const NAME[4] = {"SCALE", "ROOT", "FIT", "VELOCITY"};
    char t[8];
    if (id == UI_KVEL) {
        ui_dial_at((float)(ui.kvel - 1u) * (1.0f / 126.0f), 0, ui.mod != 0, act);
        sp_fix(t, ui.kvel, 0u, 0);
        ui_cell_value(t, ui.mod ? sk.dim : sk.text, act);
    } else {
        ui_pill(id == UI_SCALE ? UI_SCALE_NAME[ui.scale % UI_NSCALE] : id == UI_ROOT ? UI_NOTE_NAME[ui.root % 12u]
                                                                                    : UI_FIT_NAME[ui.fit % UI_NFIT],
                ui.mod != 0, act);
    }
    ui_text_c(0, 46, 60, &AF_XS, NAME[id - UI_SCALE], ui.mod ? sk.faint : act ? sk.now : sk.label, sk.ground);
}

static void ui_draw_knob(uint32_t k)
{
    static const char *const EQPT[3] = {"LOW", "MID", "HIGH"};
    int32_t it = UI_PG[ui.page].id[k], id = ui_virt(it);   /* (the EQ's POINTS: the picked point's parameter) */
    int act = ui.act_k == k && !ui.mod;             /* (the knob turning) */
    char t[SP_TEXT_N + 4];
    cv_begin(60, 76, sk.ground);
    if (ui.lay == LAY_PERF || ui.lay == LAY_MAC) {
        ui_lay_cell(k);
    } else if (id >= UI_BPM && id <= UI_TSN) {
        ui_tempo_cell(id, act);
    } else if (id >= UI_LPT && id <= UI_LC) {
        ui_lfo_cell(id, act);
    } else if (id >= UI_SCUR && id <= UI_BBANK) {   /* SAVE's, the browser's */
        ui_save_cell(id, act);
    } else if (id == UI_EQPT) {                     /* the EQ's point picked */
        ui_pill(EQPT[ui.eqpt < 3u ? ui.eqpt : 0u], ui.mod != 0, act);
        ui_text_c(0, 46, 60, &AF_XS, "POINT", ui.mod ? sk.faint : act ? sk.now : sk.label, sk.ground);
    } else if (id >= UI_SCALE && id <= UI_KVEL) {   /* SCL's */
        ui_keys_cell(id, act);
    } else if (id == UI_MSLOT) {                    /* the matrix's slot picked */
        sp_fix(t, (int32_t)ui.mslot + 1, 0u, 0);
        ui_pill(t, ui.mod != 0, act);
        ui_text_c(0, 46, 60, &AF_XS, "SLOT", ui.mod ? sk.faint : act ? sk.now : sk.label, sk.ground);
    } else if (id >= UI_ITEM) {                     /* the SETTINGS' items */
        ui_pill(id == UI_COLOR ? SY_PAL[ui.pal].name : ui.accel ? "ON" : "OFF", 0, act);
        ui_text_c(0, 46, 60, &AF_XS, id == UI_COLOR ? "COLOR" : "ACCEL", act ? sk.now : sk.label, sk.ground);
    } else if (id >= 0) {
        int dim = ui.mod && !(SP[id].flags & F_MOD);
        int32_t e = ui_eff(id);                     /* (RATE, TIME with SYNC FIXED: the note value, under their name) */
        const char *name = id >= SP_MAC1 && id <= SP_MAC4 ? sy_patch.macro[id - SP_MAC1]
                           : it == UI_EQF ? "FREQ" : it == UI_EQG ? "GAIN" : it == UI_EQQ ? "Q" : SP[id].name;
        if (ui_list((uint32_t)e)) {
            sp_text((uint32_t)e, sy_patch.p[e], t);
            ui_pill(t, dim, act);
        } else {
            ui_dial((uint32_t)e, dim, act);
            if (ui.mod) {                           /* the picked source's depth here */
                int32_t a = dim ? 0 : ui_depth(ui.mod_src, (uint32_t)id);
                if (a)
                    ui_pct(t, a);
                else
                    sp_put(t, "-");
                ui_text_c(0, 58, 60, &AF_S, t, a ? sk.mnear : sk.dim, sk.ground);
            } else {
                sp_text((uint32_t)e, sy_patch.p[e], t);
                ui_cell_value(t, sk.text, act);
            }
        }
        ui_text_c(0, 46, 60, &AF_XS, name, dim ? sk.faint : act ? sk.now : sk.label, sk.ground);
    }
    cv_blit(60u * k, R_KNOB_Y);
}

/* --------------------------------------------------------------------------------------- footer --- */
static void ui_draw_foot(void)
{
    uint32_t i;
    cv_begin(240, 28, sk.ground);
    if (ui.toast[0]) {
        ui_text_c(0, CAP_IN(XS, 20), 240, &AF_XS, ui.toast, sk.text, sk.ground);
    } else if (ui.lay) {                            /* a hold layer: how it ends */
        ui_text_c(0, CAP_IN(XS, 20), 240, &AF_XS,
                  ui.lay == LAY_PERF ? "LET GO OF GLO: THE BEND BACK TO 0"
                  : ui.lay == LAY_MAC ? "F3 .. D4: AN EFFECT ON OR OFF"
                  : ui.lay == LAY_MOM ? "LET GO OF LFO: EVERY CHANGE BACK" : "LET GO OF EDIT: THE PAGE",
                  sk.dim, sk.ground);
    } else if (ui_sec() == SEC_SAVE) {
        ui_text_c(0, CAP_IN(XS, 20), 240, &AF_XS, ui.sv_sym ? "OCT+ ABC \xB7 OCT- DEL \xB7 SAVE: WRITE"
                                                            : "OCT+ 123 \xB7 OCT- DEL \xB7 SAVE: WRITE", sk.dim, sk.ground);
    } else if (ui_sec() == SEC_BROWSE) {
        ui_text_c(0, CAP_IN(XS, 20), 240, &AF_XS, "KNOB 2 LOADS \xB7 SAVE: NAME IT", sk.dim, sk.ground);
    } else if (ui_sec() == SEC_MENU) {
        ui_text_c(0, CAP_IN(XS, 20), 240, &AF_XS, "KNOB 1: COLOR \xB7 KNOB 2: ACCEL", sk.dim, sk.ground);
    } else if (ui_sec() == SEC_KEYS) {
        ui_text_c(0, CAP_IN(XS, 20), 240, &AF_XS, "SCL HELD + A KEY: THE ROOT", sk.dim, sk.ground);
    } else if (UI_PG[ui.page].id[0] == UI_LPT && !ui.mod) {
        ui_text_c(0, CAP_IN(XS, 20), 240, &AF_XS, "LFO TWICE: ADD A POINT", sk.dim, sk.ground);
    } else {
        for (i = 0; i < 8u; i++) {
            int32_t x = 10 + (int32_t)i * 28;
            uint32_t src = UI_CHIP_SRC[i];
            if (ui.mod && ui.mod_src == src) {
                cv_rrect(x, 2, 25, 16, 3, sk.mod, sk.ground);
                ui_text_c(x, 2 + CAP_IN(XS, 16), 25, &AF_XS, UI_CHIP_NAME[i], sk.bink, sk.mod);
            } else if (ui_src_on_page(src)) {
                cv_rrect(x, 2, 25, 16, 3, sk.mod, sk.ground);
                cv_rrect(x + 1, 3, 23, 14, 2, sk.mtint, sk.mod);
                ui_text_c(x, 2 + CAP_IN(XS, 16), 25, &AF_XS, UI_CHIP_NAME[i], sk.mtext, sk.mtint);
            } else {
                cv_rrect(x, 2, 25, 16, 3, sk.chipl, sk.ground);
                cv_rrect(x + 1, 3, 23, 14, 2, sk.ground, sk.chipl);
                ui_text_c(x, 2 + CAP_IN(XS, 16), 25, &AF_XS, UI_CHIP_NAME[i], sk.faint, sk.ground);
            }
        }
    }
    cv_blit(0, R_FOOT_Y);
}

/* ---------------------------------------------------------------------------------------- graphs --- */
#define UI_OB(o) ((o) ? (uint32_t)(SP_B_ON - SP_A_ON) : 0u)
static uint32_t ui_osc(void) { return ui_sec() == SEC_OSCB; }

/* a frame's level 2 (512 samples and the guard) at phase p, -1..1 */
static float ui_wave_at(const int16_t *f, float p)
{
    float x = (p - (float)sy_floori(p)) * 512.0f, u;
    int32_t i = sy_floori(x);
    if (i > 511)
        i = 511;
    u = x - (float)i;
    return ((float)f[i] + ((float)f[i + 1] - (float)f[i]) * u) * (1.0f / 32768.0f);
}

static void ui_label(int32_t x, int top, const char *s, uint16_t c)   /* a word in a corner of the window */
{
    int32_t y = top ? WY + 4 : WY + WH - 15;
    if (x >= 0)
        cv_text_on(WX + 6 + x, y, &AF_XS, s, c, sk.display);
    else
        cv_text_r(WX + WW - 6, y, &AF_XS, s, c, sk.display);
}

/* the first source on parameter id, as "LFO 1 +-20.0%" (0: none) */
static int ui_mod_word(uint32_t id, char *t)
{
    uint32_t s;
    for (s = 0; s < NSLOT; s++) {
        int32_t src = sy_patch.p[SP_M1_SRC + 3u * s], a = sy_patch.p[SP_M1_AMT + 3u * s];
        if (src > SRC_NONE && src < SRC_COUNT && sy_patch.p[SP_M1_DST + 3u * s] == (int32_t)id && a) {
            char *e = sp_put(sp_put(t, SRC_NAMES[src]), " ");
            if (ui_src_bip((uint32_t)src)) {
                *e++ = (char)0xB1;                  /* (the plus-minus sign, Latin-1) */
                sp_put(sp_fix(e, a < 0 ? -a : a, 1u, 0), "%");
            } else {
                ui_pct(e, a);
            }
            return 1;
        }
    }
    return 0;
}

/* the phases a warp reads across a frame's 132 columns (sy_osc.c osc_warp_q: what the oscillator plays; FM is no map:
 * as it is) */
static void ui_warp_cols(uint32_t mode, float amt, float *q)
{
    uint32_t j;
    float slope;
    for (j = 0; j < 132u; j++)
        q[j] = mode == WARP_OFF || mode == WARP_FM ? (float)j / 131.0f : osc_warp_q(mode, amt, (float)j / 131.0f, &slope);
}

/* the frame playing (between two) through phases q, at its depth */
static void ui_wt_now(const wt_table_t *t, float cur, const float *q, float base, float *ys)
{
    uint32_t n = t->n, k0 = (uint32_t)sy_floori(cur), k1 = k0 + 1u < n ? k0 + 1u : k0, j;
    float u = cur - (float)k0;
    const int16_t *f0 = wt_frame(t, k0, 2u), *f1 = wt_frame(t, k1, 2u);
    for (j = 0; j < 132u; j++) {
        float a = ui_wave_at(f0, q[j]);
        ys[j] = base - (a + (ui_wave_at(f1, q[j]) - a) * u) * 18.0f;
    }
}

/* OSC: the table's frames in 3D, front to back up and to the right, each through the warp as it plays, the current
 * one on top; WARP or AMOUNT turning: the current frame before the turn dashed behind it */
static void ui_g_wt3d(void)
{
    uint32_t o = ui_osc(), b = UI_OB(o), n, k, j, mode = (uint32_t)sy_patch.p[SP_A_WARP + b];
    const wt_table_t *t = &wt_tab[o];
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_id();
    float ys[132], q[132], pos, cur, dcur, olo, ohi, plo, phi, lo, hi, amt = (float)sy_patch.p[SP_A_WAMT + b] * 0.001f;
    char w[32];
    int emph, warping = act == (int32_t)(SP_A_WARP + b) || act == (int32_t)(SP_A_WAMT + b);
    if (t->busy || !t->n) {
        ui_text_c(WX, WY + 46, WW, &AF_XS, "BUILDING THE TABLE", sk.dim, sk.display);
        return;
    }
    n = t->n;
    ui_warp_cols(mode, amt, q);
    pos = (float)sy_patch.p[SP_A_POS + b] * 0.001f;
    cur = pos * (float)(n - 1u);
    dcur = n > 1u ? cur * 15.0f / (float)(n - 1u) : 0.0f;
    ui_mod_span(SP_A_POS + b, ui.mod ? ui.mod_src : SRC_COUNT, &olo, &ohi, &plo, &phi);
    emph = ui.mod && phi > plo;                     /* the MOD layer: the frames the pick sweeps */
    lo = olo + plo;
    hi = ohi + phi;
    for (k = n; k-- > 0;) {
        float dep = n > 1u ? (float)k * 15.0f / (float)(n - 1u) : 0.0f, base = (float)WY + 80.0f - dep * 3.4f;
        float near = sy_maxf(0.0f, 1.0f - sy_absf(dep - dcur) / 7.0f);
        const int16_t *f = wt_frame(t, k, 2u);
        int32_t x0 = sy_floori((float)WX + 10.0f + dep * 4.6f + 0.5f);
        uint16_t c;
        for (j = 0; j < 132u; j++)
            ys[j] = base - ui_wave_at(f, q[j]) * 18.0f;
        cv_under(x0, ys, 132u, base + 19.0f, sk.display);
        if (emph) {
            float d0 = sy_clampf(pos + plo, 0.0f, 1.0f) * 15.0f, d1 = sy_clampf(pos + phi, 0.0f, 1.0f) * 15.0f;
            int in = dep >= d0 - 0.5f && dep <= d1 + 0.5f;
            c = in ? sy_mix565(sk.mfar, sk.mnear, (uint32_t)(near * 256.0f))
                   : sy_mix565(sk.dfar, sk.dnear, (uint32_t)(near * 256.0f));
        } else {
            c = sy_mix565(sk.wfar, sk.sound, (uint32_t)(near * 256.0f));
        }
        cv_curve(x0, ys, 132u, 1.0f, c);
    }
    {                                               /* the frame playing: between two, at its depth */
        float base = (float)WY + 80.0f - dcur * 3.4f;
        int32_t x0 = sy_floori((float)WX + 10.0f + dcur * 4.6f + 0.5f);
        if (gh && warping) {                        /* (before the turn) */
            float g[132];
            ui_warp_cols((uint32_t)gh->p[SP_A_WARP + b], (float)gh->p[SP_A_WAMT + b] * 0.001f, g);
            ui_wt_now(t, cur, g, base, ys);
            cv_curve_dash(x0, ys, 132u, 1.0f, sk.ghost, 3u, 2u);
        }
        ui_wt_now(t, cur, q, base, ys);
        cv_curve(x0, ys, 132u, 1.6f, sk.now);
        if (warping) {
            char *e = sp_put(sp_put(w, WARP_NAMES[mode < WARP_COUNT ? mode : 0]), " ");
            sp_put(sp_fix(e, sy_patch.p[SP_A_WAMT + b], 1u, 0), "%");
            if (mode == WARP_OFF)
                sp_put(w, "WARP OFF");
            ui_flag(x0 + 40, sy_floori(base) - 36, w);
        }
    }
    if (hi > lo) {                                  /* the range modulation sweeps: a bracket at the right */
        float d0 = sy_clampf(pos + lo, 0.0f, 1.0f) * 15.0f, d1 = sy_clampf(pos + hi, 0.0f, 1.0f) * 15.0f;
        float xa = (float)WX + 148.0f + d0 * 4.6f, ya = (float)WY + 80.0f - d0 * 3.4f;
        float xb = (float)WX + 148.0f + d1 * 4.6f, yb = (float)WY + 80.0f - d1 * 3.4f;
        cv_seg(xa - 3.0f, ya, xa, ya, 1.5f, sk.mod);
        cv_seg(xa, ya, xb, yb, 1.5f, sk.mod);
        cv_seg(xb, yb, xb - 3.0f, yb, 1.5f, sk.mod);
    }
    sp_put(sp_put(sp_put(w, WT_NAMES[t->id]), " \xB7 "), WT_CATNAMES[WT_RECIPES[t->id].cat]);
    ui_label(0, 1, w, sk.dim);
    if (mode != WARP_OFF) {                         /* the warp the frames show (FM: by the other oscillator) */
        char *e = sp_put(sp_put(w, "3D \xB7 "), WARP_NAMES[mode < WARP_COUNT ? mode : 0]);
        sp_put(sp_fix(sp_put(e, " "), sy_patch.p[SP_A_WAMT + b], 1u, 0), "%");
        ui_label(-1, 1, w, warping ? sk.now : sk.dim);
    } else {
        ui_label(-1, 1, "3D", sk.dim);
    }
    {
        char *e = sp_put(w, "FRAME ");
        int32_t x = WX + 6 + text_w(&AF_XS, w);
        ui_label(0, 0, w, sk.dim);
        sp_fix(e, sy_floori((cur + 1.0f) * 10.0f + 0.5f), 1u, 0);
        cv_text_on(x, WY + WH - 15, &AF_XS, e, sk.now, sk.display);
    }
    if (ui_mod_word(SP_A_POS + b, w))
        ui_label(-1, 0, w, sk.mod);
}

/* OSC: the frame playing in 2D, warped (the unwarped one behind); unison: the voices' spread under it */
static void ui_g_wt2d(int strip)
{
    uint32_t o = ui_osc(), b = UI_OB(o), n, j, mode = (uint32_t)sy_patch.p[SP_A_WARP + b];
    const wt_table_t *t = &wt_tab[o];
    float ys[211], gs[211], cur, u, amt = (float)sy_patch.p[SP_A_WAMT + b] * 0.001f, slope;
    float cy = strip ? (float)WY + 40.0f : (float)WY + 54.0f, amp = strip ? 30.0f : 40.0f;
    const int16_t *f0, *f1;
    uint32_t k0, k1;
    char w[32], *e;
    if (t->busy || !t->n) {
        ui_text_c(WX, WY + 46, WW, &AF_XS, "BUILDING THE TABLE", sk.dim, sk.display);
        return;
    }
    n = t->n;
    cur = (float)sy_patch.p[SP_A_POS + b] * 0.001f * (float)(n - 1u);
    k0 = (uint32_t)sy_floori(cur);
    k1 = k0 + 1u < n ? k0 + 1u : k0;
    u = cur - (float)k0;
    f0 = wt_frame(t, k0, 2u);
    f1 = wt_frame(t, k1, 2u);
    for (j = 0; j < 211u; j++) {
        float p = (float)j / 210.0f, q = mode == WARP_FM ? p : osc_warp_q(mode, amt, p, &slope);
        float a = ui_wave_at(f0, p), c = ui_wave_at(f0, q);
        gs[j] = cy - (a + (ui_wave_at(f1, p) - a) * u) * amp;
        ys[j] = cy - (c + (ui_wave_at(f1, q) - c) * u) * amp;
    }
    for (j = 0; j < 9u; j++)                        /* the grid, the zero line */
        cv_rect(WX + 8 + (int32_t)j * 26, WY + 2, 1, strip ? 78 : WH - 4, sk.grid);
    cv_rect(WX + 8, sy_floori(cy), 211, 1, sk.grid2);
    if (mode != WARP_OFF && mode != WARP_FM)
        cv_curve(WX + 8, gs, 211u, 1.0f, sk.ghost);
    cv_curve(WX + 8, ys, 211u, 1.6f, sk.sound);
    e = sp_put(w, "FRAME ");
    ui_label(0, 1, w, sk.dim);
    sp_fix(e, sy_floori((cur + 1.0f) * 10.0f + 0.5f), 1u, 0);
    cv_text_on(WX + 6 + text_w(&AF_XS, "FRAME "), WY + 4, &AF_XS, e, sk.now, sk.display);
    e = sp_put(sp_put(w, "2D  "), WARP_NAMES[mode < WARP_COUNT ? mode : 0]);
    if (mode != WARP_OFF)
        sp_put(sp_fix(sp_put(e, " "), sy_patch.p[SP_A_WAMT + b], 1u, 0), "%");
    ui_label(-1, 1, w, sk.dim);
    if (strip) {                                    /* the unison: n voices over +-DETUNE cents (150 x DETUNE^2) */
        uint32_t nv = (uint32_t)sy_patch.p[SP_A_UNI + b], umode = (uint32_t)sy_patch.p[SP_A_UMODE + b];
        float det = (float)sy_patch.p[SP_A_DETUNE + b] * 0.001f, blend = (float)sy_patch.p[SP_A_BLEND + b] * 0.001f;
        float cents = 150.0f * det * det, ax = (float)WX + 113.0f;
        cv_rect(WX + 20, WY + 92, 186, 1, sk.track);
        cv_rect(WX + 113, WY + 88, 1, 9, sk.ghost);
        for (j = 0; j < nv; j++) {
            float x = nv > 1u ? (float)(2u * j) / (float)(nv - 1u) - 1.0f : 0.0f, h;
            int centre = nv & 1u ? j == nv / 2u : j == nv / 2u || j + 1u == nv / 2u;
            if (umode)
                x *= 0.3f + 0.7f * x * x;
            h = centre || nv <= 2u ? 13.0f : 13.0f * blend;
            cv_seg(ax + x * 93.0f, (float)WY + 92.0f, ax + x * 93.0f, (float)WY + 92.0f - h, 2.0f,
                   centre ? sk.now : sk.sound);
        }
        w[0] = '-';
        sp_put(sp_fix(w + 1, sy_floori(cents * 10.0f + 0.5f), 1u, 0), " CT");
        ui_label(0, 0, w, sk.dim);
        w[0] = '+';
        ui_label(-1, 0, w, sk.dim);
        e = sp_fix(w, (int32_t)nv, 0u, 0);
        sp_put(sp_put(e, nv == 1u ? " VOICE  " : " VOICES  "), UMODE_NAMES[umode & 1u]);
        ui_text_c(WX, WY + WH - 15, WW, &AF_XS, w, sk.dim, sk.display);
    }
}

/* an envelope: its shape over its own time (zoom locked to it), the other envelope behind, the voices on it */
#define UI_EY(v) ((float)WY + 90.0f - (v) * 68.0f)    /* an envelope's level as a row of the window */
static float ui_env_level(uint32_t e, float s, float sus_t)   /* envelope e's level s seconds into a note */
{
    uint32_t b = e ? (uint32_t)(SP_E2_ATK - SP_E1_ATK) : 0u;
    float a = sp_time_s((float)sy_patch.p[SP_E1_ATK + b] * 0.001f);
    float h = sy_patch.p[SP_E1_HOLD + b] ? sp_time_s((float)sy_patch.p[SP_E1_HOLD + b] * 0.001f) : 0.0f;
    float d = sp_time_s((float)sy_patch.p[SP_E1_DEC + b] * 0.001f), r = sp_time_s((float)sy_patch.p[SP_E1_REL + b] * 0.001f);
    float S = (float)sy_patch.p[SP_E1_SUS + b] * 0.001f;
    float ca = (float)sy_patch.p[SP_E1_ACRV + b] * 0.001f, cd = (float)sy_patch.p[SP_E1_DCRV + b] * 0.001f;
    float cr = (float)sy_patch.p[SP_E1_RCRV + b] * 0.001f;
    if (s < a)
        return env_curve(s / a, ca);
    s -= a;
    if (s < h)
        return 1.0f;
    s -= h;
    if (s < d)
        return S + (1.0f - S) * (1.0f - env_curve(s / d, cd));
    s -= d;
    if (s < sus_t)
        return S;
    s -= sus_t;
    return s < r ? S * (1.0f - env_curve(s / r, cr)) : 0.0f;
}

static void ui_g_env(void)
{
    uint32_t e = ui_sec() == SEC_ENV2, b = e ? (uint32_t)(SP_E2_ATK - SP_E1_ATK) : 0u, i, nv = 0;
    float a = sp_time_s((float)sy_patch.p[SP_E1_ATK + b] * 0.001f);
    float h = sy_patch.p[SP_E1_HOLD + b] ? sp_time_s((float)sy_patch.p[SP_E1_HOLD + b] * 0.001f) : 0.0f;
    float d = sp_time_s((float)sy_patch.p[SP_E1_DEC + b] * 0.001f), r = sp_time_s((float)sy_patch.p[SP_E1_REL + b] * 0.001f);
    float S = (float)sy_patch.p[SP_E1_SUS + b] * 0.001f, sus_t = 0.3f * (a + h + d + r) + 0.05f;
    float total = a + h + d + sus_t + r, ys[209], gs[209], step = 0.1f, tk;
    char w[24];
    for (i = 0; i < 209u; i++) {
        float s = (float)i / 208.0f * total;
        ys[i] = UI_EY(ui_env_level(e, s, sus_t));
        gs[i] = UI_EY(ui_env_level(!e, s, sus_t));
    }
    while (total / step > 5.0f)                     /* the time ruler: at most 5 ticks */
        step *= step == 0.1f || step == 1.0f || step == 10.0f ? 2.0f : 2.5f;
    for (tk = step; tk < total; tk += step) {
        int32_t x = WX + 8 + sy_floori(tk / total * 208.0f + 0.5f);
        cv_rect(x, WY + 6, 1, 84, sk.grid);
    }
    cv_curve(WX + 8, gs, 209u, 1.2f, sk.ghost);
    cv_under(WX + 8, ys, 209u, UI_EY(0.0f), sk.wfill);
    cv_dash(WX + 8, WX + 216, sy_floori(UI_EY(S) + 0.5f), 2, 2, sk.wmid);
    cv_curve(WX + 8, ys, 209u, 1.6f, sk.sound);
    {                                               /* the breakpoints */
        float px[4] = {a + h, a + h + d, a + h + d + sus_t, total}, py[4] = {1.0f, S, S, 0.0f};
        for (i = 0; i < 4u; i++) {
            int32_t x = WX + 8 + sy_floori(px[i] / total * 208.0f + 0.5f) - 2, y = sy_floori(UI_EY(py[i]) + 0.5f) - 2;
            cv_rect(x, y, 5, 5, sk.sound);
            cv_rect(x + 1, y + 1, 3, 3, sk.display);
        }
    }
    for (i = 0; i < SY_NVOICE; i++) {               /* the voices: where each is (a value the audio writes) */
        const env_t *ev = &sy_v[i].env[e];
        float s;
        if (!sy_v[i].on || ev->st == ENV_IDLE)
            continue;
        switch (ev->st) {
        case ENV_ATK: s = ev->t * a; break;
        case ENV_HOLD: s = a + ev->t * h; break;
        case ENV_DEC: s = a + h + ev->t * d; break;
        case ENV_SUS: s = a + h + d + sus_t * (0.2f + 0.6f * (float)i / (float)SY_NVOICE); break;
        default: s = a + h + d + sus_t + ev->t * r; break;
        }
        cv_disc((float)WX + 8.0f + s / total * 208.0f, UI_EY(ev->v), 2.8f, sk.display);
        cv_disc((float)WX + 8.0f + s / total * 208.0f, UI_EY(ev->v), 2.2f, sk.now);
        nv++;
    }
    if (nv) {
        sp_fix(w, (int32_t)nv, 0u, 0);
        cv_text_on(WX + 6, WY + 4, &AF_XS, w, sk.now, sk.display);
        cv_text_on(WX + 6 + text_w(&AF_XS, w), WY + 4, &AF_XS, nv == 1u ? " VOICE" : " VOICES", sk.dim, sk.display);
    }
    ui_label(0, 0, "0", sk.faint);
    sp_secs(w, total);
    ui_label(-1, 0, w, sk.faint);
}

/* an LFO: a cycle of its shape, as the audio side plays it (a random one: five cycles of sample levels); on the
 * POINTS page its points, the one picked and the segment KNOB 4 curves; where the voices are in their cycles */
#define UI_LY(v) ((float)WY + 54.0f - (v) * 34.0f)    /* an LFO's value as a row of the window */
static void ui_g_lfo(void)
{
    uint32_t k = ui_lfo(), b = UI_LB(k), shape = (uint32_t)sy_patch.p[SP_L1_SHAPE + b], i;
    uint32_t mode = (uint32_t)sy_patch.p[SP_L1_MODE + b], sync = (uint32_t)sy_patch.p[SP_L1_SYNC + b];
    int rnd = ui_lfo_rnd(shape), pts = UI_PG[ui.page].id[0] == UI_LPT, drawn = rnd || shape == LFO_DRAW;
    sp_draw_t tp;
    const sp_draw_t *d = ui_pts(k, &tp);
    lfo_draw_t dr;
    lfo_par_t lp;
    lfo_t l0;
    float ys[209], ph = (float)sy_patch.p[SP_L1_PHASE + b] * (1.0f / 360.0f);
    char w[32];
    lfo_draw_of(&dr, d);
    memset(&lp, 0, sizeof lp);
    memset(&l0, 0, sizeof l0);
    lp.shape = (uint8_t)shape;
    lp.dr = &dr;
    for (i = 1; i < 4u; i++)                        /* the grid: the cycle's quarters, the middle */
        cv_rect(WX + 8 + (int32_t)i * 52, WY + 17, 1, 75, sk.grid);
    cv_rect(WX + 8, sy_floori(UI_LY(0.0f)), 209, 1, sk.grid2);
    for (i = 0; i < 209u; i++) {
        float x = sy_minf((float)i / 208.0f, 0.99999f);
        ys[i] = UI_LY(drawn ? lfo_draw_at(&dr, (float)i / 208.0f) : lfo_shape(&l0, &lp, x));
    }
    cv_under(WX + 8, ys, 209u, UI_LY(0.0f), sk.wfill);
    cv_curve(WX + 8, ys, 209u, 1.6f, sk.sound);
    if (pts) {                                      /* the points, the segment KNOB 4 curves, the one picked */
        uint32_t p = ui_lpt(k, d), g = p + 1u < d->n ? p : p - 1u;
        int32_t c0 = sy_floori((float)d->x[g] * 0.208f + 0.5f), c1 = sy_floori((float)d->x[g + 1u] * 0.208f + 0.5f);
        if (c1 > c0)
            cv_curve(WX + 8 + c0, ys + c0, (uint32_t)(c1 - c0 + 1), 1.6f, sk.now);
        for (i = 0; i < d->n; i++) {
            int32_t x = WX + 8 + sy_floori((float)d->x[i] * 0.208f + 0.5f);
            int32_t y = sy_floori(UI_LY((float)d->y[i] * 0.001f) + 0.5f);
            if (i == p)
                continue;
            cv_rect(x - 2, y - 2, 5, 5, sk.sound);
            cv_rect(x - 1, y - 1, 3, 3, sk.display);
        }
        cv_rect(WX + 8 + sy_floori((float)d->x[p] * 0.208f + 0.5f) - 3,
                sy_floori(UI_LY((float)d->y[p] * 0.001f) + 0.5f) - 3, 7, 7, sk.now);
    }
    if (!rnd)                                       /* the voices: where each is in its cycle (FREE: one for all) */
        for (i = 0; i < SY_NVOICE; i++) {
            const lfo_t *l = mode == LFOM_FREE ? &sy_glfo[k] : &sy_v[i].lfo[k];
            float x = (float)(l->ph >> 8) * (1.0f / 16777216.0f), y;
            if (!sy_v[i].on)
                continue;
            if (mode == LFOM_FREE)
                x += ph, x -= (float)sy_floori(x);
            y = UI_LY(drawn ? lfo_draw_at(&dr, x) : lfo_shape(&l0, &lp, x));
            cv_disc((float)WX + 8.0f + x * 208.0f, y, 2.8f, sk.display);
            cv_disc((float)WX + 8.0f + x * 208.0f, y, 2.2f, sk.now);
            if (mode == LFOM_FREE)
                break;
        }
    ui_label(0, 1, LFO_NAMES[shape < LFO_NSHAPE ? shape : 0], sk.dim);
    if (sync)                                       /* FIXED: the note value */
        sp_text(k ? SP_L2_NOTE : SP_L1_NOTE, sy_patch.p[k ? SP_L2_NOTE : SP_L1_NOTE], w);
    else
        sp_text(SP_L1_RATE + b, sy_patch.p[SP_L1_RATE + b], w);
    ui_label(-1, 1, w, sk.dim);
    sp_text(SP_L1_MODE + b, (int32_t)mode, w);
    ui_label(0, 0, w, sk.faint);
    ui_label(-1, 0, rnd ? "RANDOM \xB7 5 CYCLES" : "1 CYCLE", sk.faint);
}

/* GLOBAL: the tempo and the time signature, large; a bar's beats (the first marked); how long a beat and a bar last */
static void ui_g_tempo(void)
{
    uint32_t i, n = ui.tsb, note = UI_TSN_VAL[ui.tsn];
    int32_t x, bw, gap = n > 8u ? 3 : 5, x0, y = WY + 10;
    float beat = 60.0f / (float)ui.bpm;
    char w[32], *e;
    sp_fix(w, ui.bpm, 0u, 0);
    x = cv_text_on(WX + 14, y, &AF_L, w, sk.text, sk.display);
    cv_text_on(x + 6, y + AF_L_CAP_Y + AF_L_CAP_H - AF_XS_CAP_Y - AF_XS_CAP_H, &AF_XS, "BPM", sk.dim, sk.display);
    sp_fix(sp_put(sp_fix(w, (int32_t)n, 0u, 0), "/"), (int32_t)note, 0u, 0);
    cv_text_r(WX + WW - 14, y, &AF_L, w, sk.sound, sk.display);
    bw = sy_clampi((WW - 28 - (int32_t)(n - 1u) * gap) / (int32_t)n, 6, 26);
    x0 = WX + (WW - ((int32_t)n * bw + (int32_t)(n - 1u) * gap)) / 2;
    for (i = 0; i < n; i++) {                       /* the bar's beats: the first marked */
        int32_t bx = x0 + (int32_t)i * (bw + gap);
        cv_rrect(bx, WY + 54, bw, 18, 3, i ? sk.wmid : sk.sound, sk.display);
        if (i)
            cv_rrect(bx + 1, WY + 55, bw - 2, 16, 2, sk.display, sk.wmid);
    }
    e = sp_put(w, "BEAT 1/");
    e = sp_fix(e, (int32_t)note, 0u, 0);
    sp_secs(sp_put(e, " \xB7 "), beat);
    ui_label(0, 0, w, sk.dim);
    sp_secs(sp_put(w, "BAR "), beat * (float)n);
    ui_label(-1, 0, w, sk.dim);
}

/* a section's parameters as a list, the page's rows marked */
static void ui_g_list(void)
{
    uint32_t sec = ui_sec(), id, n = 0, first = 0, row = 0, k;
    char t[SP_TEXT_N + 4];
    for (id = 0; id < SP_COUNT; id++)
        if (SP[id].sec == sec) {
            for (k = 0; k < 4u; k++)
                if (UI_PG[ui.page].id[k] == (int32_t)id && !first)
                    first = n + 1u;
            n++;
        }
    if (first)
        first--;
    row = first > 2u ? first - 2u : 0u;             /* the page's rows in view */
    if (n > 8u && row > n - 8u)
        row = n - 8u;
    for (id = 0, k = 0; id < SP_COUNT; id++) {
        uint32_t j;
        int on = 0;
        int32_t y;
        if (SP[id].sec != sec)
            continue;
        if (k++ < row || k > row + 8u)
            continue;
        for (j = 0; j < 4u; j++)
            on |= UI_PG[ui.page].id[j] == (int32_t)id;
        y = WY + 2 + (int32_t)(k - row - 1u) * 13;
        if (on)
            cv_rrect(WX + 2, y, WW - 4, 13, 2, sk.raised, sk.display);
        cv_text_on(WX + 8, y + CAP_IN(XS, 13), &AF_XS, SP[id].name, on ? sk.label : sk.dim, on ? sk.raised : sk.display);
        sp_text(id, sy_patch.p[id], t);
        cv_text_r(WX + WW - 8, y + CAP_IN(XS, 13), &AF_XS, t, on ? sk.text : sk.text2, on ? sk.raised : sk.display);
    }
}

/* a bar of t (0 .. 1) in w px from x, or from its middle when bip (t -1 .. 1), in SOUND on the track */
static void ui_bar(int32_t x, int32_t y, int32_t w, float t, int bip)
{
    int32_t m = x + w / 2, e;
    cv_rrect(x, y, w, 8, 3, sk.track, sk.display);
    if (bip) {
        e = m + sy_floori(t * (float)(w / 2) + 0.5f);
        cv_rect(e < m ? e : m, y + 1, (e < m ? m - e : e - m) + 1, 6, sk.sound);
        cv_rect(m, y - 2, 1, 12, sk.label);
    } else if (t > 0.0f) {
        cv_rrect(x + 1, y + 1, sy_floori(t * (float)(w - 2) + 0.5f), 6, 2, sk.sound, sk.track);
    }
}

/* GLO held, PERFORM: the bend from the middle and the mod wheel as bars, with GLIDE and VOLUME as they are */
static void ui_g_perf(void)
{
    char t[SP_TEXT_N + 8];
    int32_t st = ui.bend * (ui.bend >= 0 ? sy_patch.p[SP_V_BUP] : sy_patch.p[SP_V_BDN]) * 10 / 8192;
    cv_text_on(WX + 8, WY + 6, &AF_XS, "BEND", sk.label, sk.display);
    sp_put(sp_fix(t, st, 1u, st != 0), " SEMITONES");
    cv_text_r(WX + WW - 8, WY + 6, &AF_XS, t, ui.bend ? sk.text : sk.dim, sk.display);
    ui_bar(WX + 8, WY + 20, WW - 16, (float)ui.bend * (1.0f / 8192.0f), 1);
    cv_text_on(WX + 8, WY + 38, &AF_XS, "MOD WHEEL", sk.label, sk.display);
    sp_fix(t, ui.wheel, 0u, 0);
    cv_text_r(WX + WW - 8, WY + 38, &AF_XS, t, ui.wheel ? sk.text : sk.dim, sk.display);
    ui_bar(WX + 8, WY + 52, WW - 16, (float)ui.wheel * (1.0f / 127.0f), 0);
    sp_text(SP_V_GLIDE, sy_patch.p[SP_V_GLIDE], t);
    cv_text_on(WX + 8, WY + 74, &AF_XS, "GLIDE", sk.label, sk.display);
    cv_text_on(WX + 60, WY + 74, &AF_XS, t, sk.text, sk.display);
    sp_text(SP_V_VOL, sy_patch.p[SP_V_VOL], t);
    cv_text_on(WX + 120, WY + 74, &AF_XS, "VOLUME", sk.label, sk.display);
    cv_text_r(WX + WW - 8, WY + 74, &AF_XS, t, sk.text, sk.display);
    cv_text_on(WX + 8, WY + 92, &AF_XS, "THE KEYS PLAY ON", sk.faint, sk.display);
}

/* FX held, MACROS: the six effects in their order, each lit when on, with the key that turns it on and off; the four
 * macros as bars under their names */
static void ui_g_mac(void)
{
    static const char *const KEY[6] = {"F3", "G3", "A3", "B3", "C4", "D4"};
    uint8_t o[SP_NFX];
    uint32_t i;
    char t[SP_TEXT_N + 4];
    sp_order(sy_patch.p[SP_FX_ORDER], o);
    for (i = 0; i < SP_NFX; i++) {
        uint32_t sec = SEC_DRIVE + o[i];
        int on = sy_patch.p[ui_sec_power(sec)] != 0;
        int32_t x = WX + 4 + (int32_t)i * 37;
        cv_rrect(x, WY + 4, 34, 18, 3, on ? sk.wmid : sk.rline, sk.display);
        cv_rrect(x + 1, WY + 5, 32, 16, 2, on ? sk.wtint : sk.display, on ? sk.wmid : sk.rline);
        ui_text_c(x, WY + 5 + CAP_IN(XS, 16), 34, &AF_XS, SEC_SHORT[sec], on ? sk.sound : sk.faint,
                  on ? sk.wtint : sk.display);
        ui_text_c(x, WY + 26, 34, &AF_XS, KEY[o[i]], sk.dim, sk.display);
    }
    for (i = 0; i < 4u; i++) {
        int32_t y = WY + 44 + (int32_t)i * 15;
        cv_text_on(WX + 8, y, &AF_XS, sy_patch.macro[i], sk.label, sk.display);
        ui_bar(WX + 74, y + 1, 100, (float)sy_patch.p[SP_MAC1 + i] * 0.001f, 0);
        sp_text(SP_MAC1 + i, sy_patch.p[SP_MAC1 + i], t);
        cv_text_r(WX + WW - 8, y, &AF_XS, t, sk.text, sk.display);
    }
}

/* EDIT held, JUMP: the keys and the section each opens (the one shown in NOW); a white key's code in two letters,
 * one over the other (13 px is too narrow for both side by side) */
static void ui_g_jump(void)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};   /* from F */
    static const char *const CODE[SEC_N] = {"A", "B", "SB", "NZ", "FL", "E1", "E2", "L1", "L2", "V", "H", "MX",
                                            "DR", "CH", "DL", "RV", "OT", "EQ", "S", "G", "", "", "K"};
    uint32_t k, w, pass, cur = ui_sec();
    char t[32];
    int32_t y = WY + 44;
    sp_put(sp_put(t, "NOW: "), ui_sec_name(cur));
    cv_text_on(WX + 6, WY + 3, &AF_SB, t, sk.text, sk.display);
    cv_text_on(WX + 6, WY + 21, &AF_XS, "H HOME \xB7 V VOICE \xB7 G GLOBAL", sk.faint, sk.display);
    cv_text_on(WX + 6, WY + 31, &AF_XS, "K KEYS \xB7 S SETTINGS", sk.faint, sk.display);
    for (pass = 0; pass < 2u; pass++)               /* the white keys, then the black over them */
        for (k = 0, w = 0; k < 27u; k++) {
            int black = BLACK[k % 12u], on = UI_KEY_JUMP[k] == cur, has = UI_KEY_JUMP[k] != 0xFF;
            uint16_t fill = on ? sk.now : has ? (black ? sk.ground : sk.raised) : sk.display;
            uint16_t edge = on ? sk.now : has ? sk.rline2 : sk.track;
            if (!black) {
                if (!pass) {
                    int32_t kx = WX + 5 + (int32_t)(w * 27u) / 2;
                    cv_rrect(kx, y, 13, 56, 2, edge, sk.display);
                    cv_rrect(kx + 1, y + 1, 11, 54, 1, fill, edge);
                    if (has) {
                        const char *c = CODE[UI_KEY_JUMP[k]];
                        char a[2] = {c[0], 0}, b[2] = {c[0] ? c[1] : 0, 0};
                        uint16_t ink = on ? sk.ground : sk.text;
                        if (b[0]) {
                            ui_text_c(kx, y + 30, 13, &AF_XS, a, ink, fill);
                            ui_text_c(kx, y + 41, 13, &AF_XS, b, ink, fill);
                        } else {
                            ui_text_c(kx, y + 41, 13, &AF_XS, a, ink, fill);
                        }
                    }
                }
                w++;
            } else if (pass) {
                int32_t kx = WX + 5 + (int32_t)(w * 27u) / 2 - 5;
                cv_rrect(kx, y, 9, 32, 2, edge, sk.raised);
                cv_rrect(kx + 1, y + 1, 7, 30, 1, fill, edge);
                if (has)
                    ui_text_c(kx, y + 19, 9, &AF_XS, CODE[UI_KEY_JUMP[k]], on ? sk.ground : sk.label, fill);
            }
        }
}

/* SCL's KEYS: the root and the scale by name, its notes from the root, and the 27 keys as they play with FIT: the
 * root's in NOW, the scale's in SOUND, the rest plain; each white key names the note it plays (SNAP: a key off the
 * scale, the note below; WHITE: the scale's notes, the black keys silent and dark) */
static void ui_g_keys(void)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};   /* from F */
    uint32_t mask = UI_SCALE_MASK[ui.scale % UI_NSCALE], k, w, pass, i;
    char t[40];
    int32_t x, y = WY + 44;
    sp_put(sp_put(sp_put(t, UI_NOTE_NAME[ui.root % 12u]), " "), UI_SCALE_LONG[ui.scale % UI_NSCALE]);
    cv_text_on(WX + 6, WY + 4, &AF_SB, t, sk.text, sk.display);
    sp_put(sp_put(t, "FIT "), UI_FIT_NAME[ui.fit % UI_NFIT]);
    cv_text_r(WX + WW - 6, WY + 6, &AF_XS, t, ui.fit ? sk.label : sk.faint, sk.display);
    for (x = WX + 6, i = 0; i < 12u; i++)           /* the scale's notes from the root */
        if ((mask >> i) & 1u)
            x = cv_text_on(x, WY + 25, &AF_XS, UI_NOTE_NAME[(ui.root + i) % 12u], i ? sk.label : sk.now,
                           sk.display) + 5;
    for (pass = 0; pass < 2u; pass++)               /* the white keys, then the black over them */
        for (k = 0, w = 0; k < 27u; k++) {
            int black = BLACK[k % 12u];
            int32_t n = sy_ui_key_note(k, 0), own = n == UI_KEY0 + (int32_t)k || ui.fit != UI_FIT_SNAP;
            int32_t pc = n >= 0 && own ? (n - ui.root + 120) % 12 : -1;   /* (a key SNAP moves: plain) */
            int root = pc == 0, in = pc > 0 && ((mask >> (uint32_t)pc) & 1u);
            uint16_t fill = n < 0 ? sk.ground : root ? sk.now : in ? sk.wtint : black ? sk.ground : sk.raised;
            uint16_t edge = n < 0 ? sk.track : root ? sk.now : in ? sk.wmid : black ? sk.rline : sk.rline2;
            if (!black) {
                if (!pass) {
                    int32_t kx = WX + 5 + (int32_t)(w * 27u) / 2;
                    cv_rrect(kx, y, 13, 52, 2, edge, sk.display);
                    cv_rrect(kx + 1, y + 1, 11, 50, 1, fill, edge);
                    if (n >= 0) {
                        const char *nm = UI_NOTE_NAME[n % 12];
                        cv_text_on(kx + (13 - text_w(&AF_XS, nm)) / 2, y + 38, &AF_XS, nm,
                                   root ? sk.ground : in ? sk.sound : sk.dim, fill);
                    }
                }
                w++;
            } else if (pass) {
                int32_t kx = WX + 5 + (int32_t)(w * 27u) / 2 - 5;
                cv_rrect(kx, y, 9, 30, 2, edge, sk.raised);
                cv_rrect(kx + 1, y + 1, 7, 28, 1, fill, edge);
            }
        }
}

/* SEQ's MATRIX: the eight slots, each its source, where it goes (the destination's section and name) and how much
 * (positive in VALUE, negative in MOD-), the one picked raised (KNOB 2-4 set it); a slot without a source: EMPTY */
static void ui_g_matrix(void)
{
    uint32_t s;
    char t[SP_TEXT_N + 8];
    for (s = 0; s < NSLOT; s++) {
        int32_t y = WY + 2 + (int32_t)s * 13, ty = y + CAP_IN(XS, 13), src = sy_patch.p[SP_M1_SRC + 3u * s];
        int32_t amt = sy_patch.p[SP_M1_AMT + 3u * s];
        int on = s == ui.mslot, used = src > SRC_NONE && src < (int32_t)SRC_COUNT;
        uint16_t bg = on ? sk.raised : sk.display;
        if (on)
            cv_rrect(WX + 2, y, WW - 4, 13, 2, sk.raised, sk.display);
        sp_fix(t, (int32_t)s + 1, 0u, 0);
        cv_text_on(WX + 7, ty, &AF_XS, t, on ? sk.now : sk.faint, bg);
        if (!used) {
            cv_text_on(WX + 20, ty, &AF_XS, "EMPTY", sk.faint, bg);
            continue;
        }
        cv_text_on(WX + 20, ty, &AF_XS, SRC_NAMES[src], on ? sk.text : sk.text2, bg);
        cv_rect(WX + 79, y + 6, 7, 1, amt ? sk.label : sk.faint);   /* (an arrow: source to where it goes) */
        cv_rect(WX + 84, y + 5, 1, 3, amt ? sk.label : sk.faint);
        cv_rect(WX + 83, y + 4, 1, 5, amt ? sk.label : sk.faint);
        sp_text(SP_M1_DST + 3u * s, sy_patch.p[SP_M1_DST + 3u * s], t);
        cv_text_fit(WX + 91, ty, &AF_XS, t, on ? sk.text : sk.text2, bg, 86);
        ui_pct(t, amt);
        cv_text_r(WX + WW - 6, ty, &AF_XS, t, amt > 0 ? sk.value : amt < 0 ? sk.modneg : sk.faint, bg);
    }
}

/* SETTINGS > COLOR: the palettes, each with its own ground, raised, sound, now, value and mod */
static void ui_g_color(void)
{
    uint32_t i, j;
    for (i = 0; i < SY_NPAL; i++) {
        int32_t y = WY + 2 + (int32_t)i * 13, cur = i == ui.pal;
        static const uint8_t ROLE[6] = {PR_GROUND, PR_RAISED, PR_SOUND, PR_NOW, PR_VALUE, PR_MOD};
        static const char *const TAG[SY_NPAL] = {"DEFAULT", "", "", "", "LIGHT", "DIM", "CONTRAST", "GREYS"};
        uint16_t bg = cur ? sk.raised : sk.display;
        if (cur) {
            cv_rrect(WX + 2, y, WW - 4, 13, 2, sk.rline2, sk.display);
            cv_rrect(WX + 3, y + 1, WW - 6, 11, 2, sk.raised, sk.rline2);
            cv_seg((float)WX + 7.0f, (float)y + 3.5f, (float)WX + 9.5f, (float)y + 6.5f, 1.2f, sk.now);
            cv_seg((float)WX + 9.5f, (float)y + 6.5f, (float)WX + 7.0f, (float)y + 9.5f, 1.2f, sk.now);
        }
        cv_text_on(WX + 14, y + CAP_IN(XS, 13), &AF_XS, SY_PAL[i].name, cur ? sk.text : sk.text2, bg);
        for (j = 0; j < 6u; j++) {
            int32_t x = WX + 70 + (int32_t)j * 13;
            cv_rrect(x, y + 3, 11, 8, 2, sk.rline, bg);
            cv_rrect(x + 1, y + 4, 9, 6, 1, sy_rgb(SY_PAL[i].c[ROLE[j]]), sk.rline);
        }
        cv_text_r(WX + WW - 8, y + CAP_IN(XS, 13), &AF_XS, TAG[i], cur ? sk.label : sk.faint, bg);
    }
}

/* HOME: the sound playing (its category, its bank and number, its name large) and its output, live: the left
 * (SOUND) over the right, three periods of the newest voice's note across (sy_engine.c's scope, kept here) */
#define UI_SCOPE_Y (WY + 87)                        /* the scope's 0 */
#define UI_LIVE_R0 (WY + 68)                        /* .. its rows, from here: redrawn alone (D_LIVE) */
static int16_t ui_scope[2][SY_SCOPE_N];
static uint8_t ui_scope_ok;

static uint32_t ui_scope_step(void)                 /* samples a point: three periods of the newest note across */
{
    const sy_voice_t *nv = 0;
    uint32_t i;
    float hz;
    for (i = 0; i < SY_NVOICE; i++)
        if (sy_v[i].on && (!nv || sy_v[i].age > nv->age))
            nv = &sy_v[i];
    if (!nv)
        return 4u;
    hz = 440.0f * sy_exp2f((nv->pitch - 69.0f) * (1.0f / 12.0f));
    return (uint32_t)sy_clampi(sy_floori(3.0f * SY_FS / hz / (float)SY_SCOPE_N) + 1, 1, 16);
}

static void ui_g_home(void)
{
    uint32_t i, num, cnt;
    const char *bank = sy_app_sound_bank(&num, &cnt);
    const char *cat = CAT_NAMES[sy_patch.cat < CAT_COUNT ? sy_patch.cat : CAT_OTHER];
    float yl[SY_SCOPE_N], yr[SY_SCOPE_N];
    int32_t x, w = text_w(&AF_XS, cat) + 10;
    char t[24];
    cv_rrect(WX + 6, WY + 6, w, 14, 3, sk.rline, sk.display);   /* the category */
    cv_rrect(WX + 7, WY + 7, w - 2, 12, 2, sk.raised, sk.rline);
    ui_text_c(WX + 6, WY + 6 + CAP_IN(XS, 14), w, &AF_XS, cat, sk.label, sk.raised);
    x = WX + WW - 6;                                /* "STARTER 02 / 08" */
    if (cnt) {
        t[0] = (char)('0' + cnt / 10u % 10u);
        t[1] = (char)('0' + cnt % 10u);
        t[2] = 0;
        x = cv_text_r(x, WY + 7, &AF_XS, t, sk.dim, sk.display);
        x = cv_text_r(x, WY + 7, &AF_XS, " / ", sk.dim, sk.display);
        t[0] = (char)('0' + num / 10u % 10u);
        t[1] = (char)('0' + num % 10u);
        x = cv_text_r(x, WY + 7, &AF_XS, t, sk.text, sk.display);
        cv_text_r(x - 4, WY + 7, &AF_XS, bank, sk.dim, sk.display);
    } else if (num) {                               /* "USER 12" */
        sp_fix(t, (int32_t)num, 0u, 0);
        x = cv_text_r(x, WY + 7, &AF_XS, t, sk.text, sk.display);
        cv_text_r(x - 4, WY + 7, &AF_XS, bank, sk.dim, sk.display);
    } else {
        cv_text_r(x, WY + 7, &AF_XS, bank, sk.dim, sk.display);
    }
    text_fit(t, sizeof t, sy_patch.name, &AF_L, WW - 12);   /* the name */
    cv_text_on(WX + 6, WY + 24, &AF_L, t, sk.text, sk.display);
    cv_seg((float)WX + 10.0f, (float)WY + 57.0f, (float)WX + 7.5f, (float)WY + 60.5f, 1.2f, sk.dim);
    cv_seg((float)WX + 7.5f, (float)WY + 60.5f, (float)WX + 10.0f, (float)WY + 64.0f, 1.2f, sk.dim);
    x = cv_text_on(WX + 14, WY + 55, &AF_XS, "PRESETS", sk.dim, sk.display) + 4;
    cv_seg((float)x, (float)WY + 57.0f, (float)x + 2.5f, (float)WY + 60.5f, 1.2f, sk.dim);
    cv_seg((float)x + 2.5f, (float)WY + 60.5f, (float)x, (float)WY + 64.0f, 1.2f, sk.dim);
    cv_text_on(x + 10, WY + 55, &AF_XS, "TURN TO BROWSE", sk.faint, sk.display);
    cv_text_r(WX + WW - 6, WY + 55, &AF_XS, "OUT L / R", sk.faint, sk.display);
    cv_rect(WX + 7, UI_SCOPE_Y, SY_SCOPE_N, 1, sk.grid2);
    {                                               /* the peak to 16 px (quiet: at most x4, full scale 17 px) */
        int32_t pk = 2048;
        float g;
        for (i = 0; ui_scope_ok && i < 2u * SY_SCOPE_N; i++) {
            int32_t v = ui_scope[i / SY_SCOPE_N][i % SY_SCOPE_N];
            pk = v > pk ? v : -v > pk ? -v : pk;
        }
        g = sy_minf(16.0f / (float)pk, 68.0f / 32768.0f);
        for (i = 0; i < SY_SCOPE_N; i++) {
            yl[i] = (float)UI_SCOPE_Y + 0.5f - (ui_scope_ok ? (float)ui_scope[0][i] * g : 0.0f);
            yr[i] = (float)UI_SCOPE_Y + 0.5f - (ui_scope_ok ? (float)ui_scope[1][i] * g : 0.0f);
        }
    }
    cv_curve(WX + 7, yr, SY_SCOPE_N, 1.0f, sk.wmid);
    cv_curve(WX + 7, yl, SY_SCOPE_N, 1.2f, sk.sound);
}

/* the display's rows as the panel has them, a hash each; 0: not known (everything redrawn, everything sent) */
static uint32_t ui_rowh[114];
static uint8_t ui_rows_ok;

/* the display; from row r0 only (0: all of it): HOME's scope alone. Only the band of rows that changed goes to the
 * panel: a voice's dot moving sends a few rows, an envelope held at its sustain none (the SPI, 12 MHz, takes 36 ms
 * for all 114) */
static void ui_draw_disp(uint32_t r0)
{
    uint16_t line = ui.mod ? sk.mline : sk.dline;
    uint32_t y, x, lo = 114u, hi = 0u;
    if (!ui_rows_ok)
        r0 = 0;
    cv_begin(240, 114, sk.ground);
    cv_rrect(6, 3, 228, 110, 4, line, sk.ground);
    cv_rrect(7, 4, 226, 108, 3, sk.display, line);
    switch (ui.lay == LAY_PERF || ui.lay == LAY_MAC || ui.lay == LAY_JUMP ? 0xFFu : UI_PG[ui.page].graph) {
    case 0xFFu:                                     /* (a hold layer's own) */
        if (ui.lay == LAY_PERF)
            ui_g_perf();
        else if (ui.lay == LAY_MAC)
            ui_g_mac();
        else
            ui_g_jump();
        break;
    case G_WT3D: ui_g_wt3d(); break;
    case G_WT2D: ui_g_wt2d(0); break;
    case G_UNI: ui_g_wt2d(1); break;
    case G_ENV: ui_g_env(); break;
    case G_LFO: ui_g_lfo(); break;
    case G_TEMPO: ui_g_tempo(); break;
    case G_COLOR: ui_g_color(); break;
    case G_HOME: ui_g_home(); break;
    case G_FILTER: ui_g_filter(); break;
    case G_DRIVE: ui_g_drive(); break;
    case G_CHORUS: ui_g_chorus(); break;
    case G_DELAY: ui_g_delay(); break;
    case G_REVERB: ui_g_reverb(); break;
    case G_OTT: ui_g_ott(); break;
    case G_EQ: ui_g_eq(); break;
    case G_SAVE: ui_g_save(); break;
    case G_BROWSE: ui_g_browse(); break;
    case G_MATRIX: ui_g_matrix(); break;
    case G_KEYS: ui_g_keys(); break;
    default: ui_g_list(); break;
    }
    for (y = r0; y < cv_h; y++) {
        const uint16_t *p = &cv_px[y * cv_w];
        uint32_t h = 2166136261u;                   /* (FNV-1a) */
        for (x = 0; x < cv_w; x++)
            h = (h ^ p[x]) * 16777619u;
        if (!ui_rows_ok || h != ui_rowh[y]) {
            ui_rowh[y] = h;
            lo = y < lo ? y : lo;
            hi = y;
        }
    }
    ui_rows_ok = 1;
    if (lo <= hi)
        lcd_blit(0, R_DISP_Y + lo, cv_w, hi - lo + 1u, cv_px + lo * cv_w);
}

/* ------------------------------------------------------------------------------------------ LEDs --- */
static void ui_led(uint8_t *l, uint32_t id)         /* matrix id: a button's (panel.btn), a key's 14 + k */
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id)
                l[c] |= (uint8_t)(1u << r);
}
#define UI_LED_PLAY_GREEN ((8u << 3) | 1u)         /* PLAY's green LED: no key there, column 8 row PA9 (Felucca) */
static void ui_led_breathe(uint32_t id)             /* .. breathing: neither lit nor glowing */
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id) {
                fm1_led[c] &= (uint8_t)~(1u << r);
                fm1_led_dim[c] &= (uint8_t)~(1u << r);
                fm1_led_breath[c] |= (uint8_t)(1u << r);
            }
}

/* the keys held (REC's MOD layer: the picked source lit, the sources in use dim), the page's family button lit,
 * the other family buttons dim, REC or ENV lit while its layer is on */
static void sy_ui_leds(void)
{
    uint32_t c, k, f, mask = UI_SCALE_MASK[ui.scale % UI_NSCALE];
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = fm1_led_dim[c] = fm1_led_mid[c] = fm1_led_breath[c] = 0;
    for (k = 0; k < 27u; k++) {
        if (ui.mod && ui.mod_btn == B_REC) {
            if (UI_KEY_SRC[k] == ui.mod_src)
                ui_led(fm1_led, 14u + k);
            else if (UI_KEY_SRC[k] && ui_src_used(UI_KEY_SRC[k]))
                ui_led(fm1_led_dim, 14u + k);
        } else if (ui.lay == LAY_MAC && UI_KEY_FX[k] >= 0) {   /* MACROS: the effects' keys, lit when on */
            ui_led(sy_patch.p[ui_sec_power(SEC_DRIVE + (uint32_t)UI_KEY_FX[k])] ? fm1_led : fm1_led_dim, 14u + k);
        } else if (ui.lay == LAY_JUMP && UI_KEY_JUMP[k] != 0xFF) {   /* JUMP: the keys that open a section, */
            ui_led(UI_KEY_JUMP[k] == ui_sec() ? fm1_led : fm1_led_dim, 14u + k);   /* the one shown lit */
        } else if (sy_app_held(k)) {
            ui_led(fm1_led, 14u + k);
        } else if (ui.scale && ui_sec() != SEC_SAVE) {   /* a scale (not CHROM): its keys glow, the root's at the */
            int32_t n = sy_ui_key_note(k, 0);           /* mid level; a key SNAP moves or WHITE silences: dark */
            uint32_t pc = n >= 0 && (ui.fit != UI_FIT_SNAP || n == UI_KEY0 + (int32_t)k)
                              ? (uint32_t)(n - ui.root + 120) % 12u : 12u;
            if (pc == 0u)
                ui_led(fm1_led_mid, 14u + k);
            else if (pc < 12u && ((mask >> pc) & 1u))
                ui_led(fm1_led_dim, 14u + k);
        }
    }
    for (f = 0; f < FAM_N; f++)
        if (FAM_BTN[f] < NB)
            ui_led(f == ui.fam ? fm1_led : fm1_led_dim, panel.btn[FAM_BTN[f]]);
    if (ui.mod)
        ui_led(fm1_led, panel.btn[ui.mod_btn]);
    else
        ui_led(fm1_led_dim, panel.btn[B_REC]);
    if (ui.run) {                                   /* PLAY: its green LED while running, its own each beat (a */
        uint32_t beat = 60000u / (ui.bpm ? ui.bpm : 120u);   /* quarter of it lit) */
        fm1_led[UI_LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (UI_LED_PLAY_GREEN & 7u));
        if ((fm1_ms - ui.run_t) % beat < beat / 4u)
            ui_led(fm1_led, panel.btn[B_PLAY]);
    }
    if (ui.mod == 2u)                               /* a latched MOD layer's button, a hold layer's: breathing */
        ui_led_breathe(panel.btn[ui.mod_btn]);
    else if (ui.lay)
        ui_led_breathe(panel.btn[ui.lay_btn]);
}

/* ------------------------------------------------------------------------------------------ frame --- */
/* a frame (~60 a second): the knob turning let go, the live graphs, then each region that changed, then the LEDs */
static void sy_ui_frame(void)
{
    uint32_t g = UI_PG[ui.page].graph, i, live = 0, nv = 0;
    static uint8_t was_live, was_busy, was_nv;
    static uint32_t load_t;
    if ((ui.dirty & D_ALL) == D_ALL)                /* (everything redrawn: everything sent, whatever the panel had) */
        ui_rows_ok = 0;
    if (ui.act_k < 4u && fm1_ms - ui.act_t >= UI_GESTURE_MS) {   /* (its cell, the graph as it rests) */
        ui.dirty |= D_KNOB(ui.act_k) | D_DISP;
        ui.act_k = 0xFF;
    }
    for (i = 0; i < SY_NVOICE; i++)
        nv += sy_v[i].on != 0;
    if (g == G_ENV || g == G_LFO)                   /* what moves, only while sound plays (then once more, still): */
        live = nv != 0;                             /* the voices on an envelope or an LFO; the chorus's taps, */
    else if (g == G_CHORUS || g == G_OTT)           /* OTT's levels (the effect on); the EQ's points where their */
        live = sy_patch.p[g == G_CHORUS ? SP_CH_ON : SP_OT_ON] && ui_sounding();   /* modulation has them */
    else if (g == G_EQ)
        live = ui_sounding() && ui_eq_modded();
    if ((live || was_live) && fm1_ms - ui.live_t >= 70u) {
        ui.live_t = fm1_ms;
        ui.dirty |= D_DISP;
        was_live = (uint8_t)live;
    }
    if (g == G_HOME) {                              /* the scope: while sound plays, and once after (silence, flat), */
        static uint8_t fresh, flat = 1;             /* a capture kept (drawn when it differs), the next asked for */
        int snd = ui_sounding();
        if (sy_scope.st == 2u) {
            SY_CBAR();
            if (!ui_scope_ok || memcmp(ui_scope, sy_scope.v, sizeof ui_scope)) {
                memcpy(ui_scope, sy_scope.v, sizeof ui_scope);
                fresh = 1;
            }
            for (flat = 1, i = 0; i < 2u * SY_SCOPE_N; i++) {   /* (a capture under -66 dB: flat on the screen) */
                int32_t v = ui_scope[i / SY_SCOPE_N][i % SY_SCOPE_N];
                flat &= v > -16 && v < 16;
            }
            ui_scope_ok = 1;
            sy_scope.st = 0;
        }
        if (sy_scope.st == 0u && (snd || !flat))
            sy_scope_arm(ui_scope_step());
        if (fresh && fm1_ms - ui.live_t >= 40u) {
            ui.live_t = fm1_ms;
            ui.dirty |= D_LIVE;
            fresh = 0;
        }
        if (fm1_ms - load_t >= 500u) {              /* the load: when its figure moves (by 2 in silence: its */
            int32_t ld = sy_app_load(), d = ld - ui_ld_shown;   /* jitter draws nothing) */
            load_t = fm1_ms;
            if (ld >= 0 && (d >= 2 || d <= -2 || (d && nv)))
                ui.dirty |= D_HEAD;
        }
        if (nv != was_nv)                           /* the voices */
            ui.dirty |= D_HEAD;
    } else if (sy_scope.st == 1u) {
        sy_scope.st = 0;                            /* (another page: no capture) */
    }
    was_nv = (uint8_t)nv;
    if (g == G_WT3D || g == G_WT2D || g == G_UNI) {
        uint8_t busy = wt_tab[ui_osc()].busy;
        if (busy != was_busy)
            ui.dirty |= D_DISP;                     /* (a table built: its frames) */
        was_busy = busy;
    }
    for (i = 0; i < 4u; i++)
        if (ui.dirty & D_KNOB(i)) {
            ui.dirty &= (uint8_t)~D_KNOB(i);
            ui_draw_knob(i);
        }
    if (ui.dirty & D_HEAD) {
        ui.dirty &= (uint8_t)~D_HEAD;
        ui_draw_head();
    }
    if (ui.dirty & D_DISP) {
        ui.dirty &= (uint8_t)~(D_DISP | D_LIVE);
        ui_draw_disp(0u);
    } else if (ui.dirty & D_LIVE) {
        ui.dirty &= (uint8_t)~D_LIVE;
        ui_draw_disp(UI_LIVE_R0);
    }
    if (ui.dirty & D_FOOT) {
        ui.dirty &= (uint8_t)~D_FOOT;
        ui_draw_foot();
    }
    sy_ui_leds();
}
