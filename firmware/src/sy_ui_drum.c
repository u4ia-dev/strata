/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* The STRATA UI's drums (V0.85; the drum synth: sy_drum.c, the looper's side: sy_loop.c; docs/CONTROLS.md "The drums"):
 *   - KIT, EDIT's on a drum track: DRUM (the drum picked: TUNE, DECAY, TONE (WT 1, WT 2: FRAME); its fall drawn),
 *     MORE (its PUNCH, SNAP, SPREAD, METAL or BEND, LEVEL, PAN: the eight as a mixer), ACCENT (ACCENT's lift,
 *     VELOCITY's share: the gain by velocity drawn), DUCK (AMOUNT, ATTACK, RELEASE, SHAPE: the synth's level drawn
 *     over time from a ghost hit). Stopped, the drum played is the one picked;
 *   - CLIP on a drum track: GRID (the clip in sixteenths, a row a drum and the DUCK row; the step picked, its VELOCITY,
 *     ACCENT), HIT (its CHANCE, RATCHET), CLIP (as a synth track's). On GRID and HIT the white keys put in or take
 *     out the row's hits on the sixteen steps shown (stopped: heard as they go in), the black keys pick the row
 *     (1 KICK .. 9 DUCK); their LEDs show the steps and the rows.
 * A drum track's keys elsewhere (sy_loop.c): white keys 1-8 the drums, 9-16 the same accented, the first black key
 * DUCK; they glow as such. Needs sy_ui.c, sy_ui_draw.c, sy_ui_loop.c (ui_l_head), sy_gfx.c. */

/* ------------------------------------------------------------------------------------------ helpers --- */
static uint32_t ui_d_lane(void) { return ui.krow % DK_N; }
static uint32_t ui_d_row(void) { return ui.grow % DK_ROWS; }
static uint32_t ui_d_steps(void)                    /* the selected clip's sixteenths (none yet: a bar's) */
{
    uint32_t n = lp_cur_clip()->len / LP_STEP;
    return n ? n : lp_bar_ticks() / LP_STEP;
}
static uint32_t ui_d_page0(void) { return (ui.gstep % ui_d_steps()) / 16u * 16u; }   /* the steps shown: from */
static char *ui_d_time(char *v, float s)            /* a time: "340 MS", "1.5 S" */
{
    if (s < 0.9995f)
        return sp_put(sp_fix(v, (int32_t)(s * 1000.0f + 0.5f), 0u, 0), " MS");
    return sp_put(sp_fix(v, (int32_t)(s * 10.0f + 0.5f), 1u, 0), " S");
}
static uint32_t ui_d_white(uint32_t k, int *black)  /* key k's place among the white keys (or the black, *black) */
{
    uint32_t i, n = 0;
    *black = LK_BLACK[k % 12u] != 0;
    for (i = 0; i < k; i++)
        n += (LK_BLACK[i % 12u] != 0) == *black;
    return n;
}
static int ui_grid_keys(void)                       /* the grid's pages: the keys are its steps and rows */
{
    uint32_t g = UI_PG[ui.page].graph;
    return !ui.lay && !ui.mod && (g == G_LGRID || g == G_LHIT);
}
static void sy_app_drum(uint32_t note, uint32_t vel);   /* sy_app.c: a drum heard (as MIDI's note, the track selected) */

/* ------------------------------------------------------------------------------------------ turning --- */
static void ui_drum_turn(int32_t item, int32_t s, uint32_t e)
{
    dk_lane_t *k = &dkk.l[ui_d_lane()];
    switch (item) {
    case UI_KLANE: ui.krow = (uint8_t)sy_clampi((int32_t)ui_d_lane() + s, 0, DK_N - 1); break;
    case UI_KTUNE: k->tune = (int8_t)sy_clampi(k->tune + s, -24, 24); break;
    case UI_KDEC: k->decay = (uint8_t)sy_clampi(k->decay + ui_accel(e, s, 100), 0, 100); break;
    case UI_KTONE: k->tone = (uint8_t)sy_clampi(k->tone + ui_accel(e, s, 100), 0, 100); break;
    case UI_KCHR: k->chr = (uint8_t)sy_clampi(k->chr + ui_accel(e, s, 100), 0, 100); break;
    case UI_KLVL: k->level = (uint8_t)sy_clampi(k->level + ui_accel(e, s, 127), 0, 127); break;
    case UI_KPAN: k->pan = (uint8_t)sy_clampi(k->pan + ui_accel(e, s, 126), 1, 127); break;
    case UI_KACC: dkk.accent = (uint8_t)sy_clampi(dkk.accent + ui_accel(e, s, 100), 0, 100); break;
    case UI_KVSN: dkk.vsens = (uint8_t)sy_clampi(dkk.vsens + ui_accel(e, s, 100), 0, 100); break;
    case UI_DAMT: dkk.amount = (uint8_t)sy_clampi(dkk.amount + ui_accel(e, s, 100), 0, 100); break;
    case UI_DATT: dkk.attack = (uint8_t)sy_clampi(dkk.attack + ui_accel(e, s, 50), 0, 50); break;
    case UI_DREL: dkk.release = (uint8_t)sy_clampi(dkk.release + ui_accel(e, s, 100), 0, 100); break;
    case UI_DSHP: dkk.shape = (uint8_t)sy_clampi(dkk.shape + s, 0, DS_N - 1); break;
    case UI_GSTEP: ui.gstep = (uint16_t)sy_clampi((int32_t)(ui.gstep % ui_d_steps()) + s, 0, (int32_t)ui_d_steps() - 1); break;
    case UI_GROW: ui.grow = (uint8_t)sy_clampi((int32_t)ui_d_row() + s, 0, DK_ROWS - 1); break;
    case UI_GVEL: case UI_GACC: case UI_GCHN: case UI_GRAT: {
        uint32_t f = (uint32_t)(item - UI_GVEL);
        if (lp_grid_attr(ui_d_row(), ui.gstep % ui_d_steps(), f, f ? s : ui_accel(e, s, 126)) < 0) {
            ui_toast("NO HIT: A WHITE KEY PUTS ONE");
            return;
        }
        break;
    }
    default: break;
    }
    lpx.seq++;
    ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
}

/* the grid's keys: a white key the step of the sixteen shown (in or out; stopped and in: heard), a black key the row */
static int ui_grid_key(uint32_t k)
{
    int black;
    uint32_t w, st;
    int32_t r;
    char m[40];
    if (k >= 27u)
        return 1;
    w = ui_d_white(k, &black);
    if (black) {
        if (w < DK_ROWS) {
            ui.grow = (uint8_t)w;
            ui.dirty |= D_KNOBS | D_DISP;
        }
        return 1;
    }
    st = ui_d_page0() + w;
    r = lp_grid_toggle(ui_d_row(), st, sy_ui_key_vel());
    if (r < 0) {
        ui_toast(lp_cur_rec() && lpr.st == LR_TAKE ? "THE TAKE SETS THIS CLIP" : st >= ui_d_steps() ? "PAST THE CLIP'S END"
                                                                                                    : "FULL: 2048 NOTES AND MOVES");
        return 1;
    }
    ui.gstep = (uint16_t)st;
    if (r && !cht.run && lpr.st == LR_OFF)
        sy_app_drum(DK_NOTE[ui_d_row()], sy_ui_key_vel());
    sp_put(sp_put(sp_fix(sp_put(sp_put(sp_put(m, DK_NAME[ui_d_row()]), " "), "STEP "), (int32_t)st + 1, 0u, 0), " "),
           r ? "IN" : "OUT");
    ui_toast(m);
    ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
    return 1;
}

/* the keys' LEDs (sy_ui_draw.c sy_ui_leds; 1: key k's set here). The grid: the row's hits on the steps shown lit, the
 * playhead's step at the mid level, the clip's other steps dark; the row picked lit among the black keys, rows with
 * hits there glowing. A drum track's keys elsewhere: the drums glowing, the accented ones at the mid level, DUCK */
static int ui_drum_leds(uint32_t k)
{
    int black;
    uint32_t w;
    if (ui.lay || ui.mod || ui_sec() == SEC_SAVE || !ui_drums())
        return 0;
    w = ui_d_white(k, &black);
    if (ui_grid_keys()) {
        const lp_clip_t *c = lp_cur_clip();
        uint32_t p0 = ui_d_page0(), t = lpp.sel % LP_NTRK, play = cht.run && lps[t].play == lpp.scn && c->len;
        if (!black) {
            uint32_t st = p0 + w;
            if (st < ui_d_steps() && c->len && lp_grid_at(c, ui_d_row(), st) != LP_END)
                ui_led(fm1_led, 14u + k);
            else if (play && lps[t].pos / LP_STEP == st)
                ui_led(fm1_led_mid, 14u + k);
        } else if (w < DK_ROWS) {
            uint32_t s, any = 0;
            for (s = p0; s < p0 + 16u && s < ui_d_steps() && c->len && !any; s++)
                any = lp_grid_at(c, w, s) != LP_END;
            if (w == ui_d_row())
                ui_led(fm1_led, 14u + k);
            else if (any)
                ui_led(fm1_led_dim, 14u + k);
        }
        return 1;
    }
    if (sy_app_held(k))
        return 0;                                   /* (held: lit, as any key) */
    if (!black && w < DK_N)
        ui_led(fm1_led_dim, 14u + k);
    else if (!black && w < 2u * DK_N)
        ui_led(fm1_led_mid, 14u + k);
    else if (black && w == 0u)
        ui_led(fm1_led_dim, 14u + k);
    return 1;
}

/* every millisecond (sy_ui_loop.c sy_ui_loop_ms): stopped, the drum played is KIT's drum picked */
static void sy_ui_drum_ms(void)
{
    uint8_t l = dk_last;
    if (cht.run) {                                  /* (running: the clips' hits are not the one played) */
        dk_last = 0xFF;
        return;
    }
    if (l < DK_N && ui_sec() == SEC_KIT) {
        dk_last = 0xFF;
        if (l != ui.krow && UI_PG[ui.page].graph != G_KDUCK && UI_PG[ui.page].graph != G_KACC) {
            ui.krow = l;
            ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
        }
    }
}

/* ------------------------------------------------------------------------------------------- values --- */
static const char *ui_drum_nv(uint32_t id, char *v)
{
    uint32_t l = ui_d_lane();
    const dk_lane_t *k = &dkk.l[l];
    v[0] = 0;
    switch (id) {
    case UI_KLANE: sp_put(v, DK_NAME[l]); return "DRUM";
    case UI_KTUNE:
        if (k->tune)
            sp_put(sp_fix(v, k->tune, 0u, 1), " ST");
        else
            sp_put(v, "0");
        return "TUNE";
    case UI_KDEC: ui_d_time(v, dk_t60(l, k->decay)); return "DECAY";
    case UI_KTONE:
        if (l >= DK_WT1) {
            const wt_table_t *t = &wt_tab[l - DK_WT1];
            if (t->n)
                sp_fix(sp_put(sp_fix(v, sy_floori((float)k->tone * 0.01f * (float)(t->n - 1u) + 0.5f) + 1, 0u, 0), " OF "),
                       t->n, 0u, 0);
            else
                sp_put(v, "NO TABLE");
            return "FRAME";
        }
        sp_fix(v, k->tone, 0u, 0);
        return "TONE";
    case UI_KCHR: sp_fix(v, k->chr, 0u, 0); return DK_CHR[l];
    case UI_KLVL: sp_fix(v, k->level, 0u, 0); return "LEVEL";
    case UI_KPAN:
        if (k->pan == 64u)
            sp_put(v, "CENTRE");
        else
            sp_put(sp_fix(v, k->pan < 64u ? 64 - k->pan : k->pan - 64, 0u, 0), k->pan < 64u ? " L" : " R");
        return "PAN";
    case UI_KACC:
        sp_put(sp_fix(v, (int32_t)(sy_log2f(1.0f + (float)dkk.accent * 0.01f) * 60.206f + 0.5f), 1u, 1), " DB");
        return "ACCENT";
    case UI_KVSN: sp_put(sp_fix(v, dkk.vsens, 0u, 0), " %"); return "VELOCITY";
    case UI_DAMT: sp_put(sp_fix(v, dkk.amount, 0u, 0), " %"); return "AMOUNT";
    case UI_DATT: sp_put(sp_fix(v, dkk.attack, 0u, 0), " MS"); return "ATTACK";
    case UI_DREL: ui_d_time(v, dk_rel_s()); return "RELEASE";
    case UI_DSHP: sp_put(v, DS_NAME[dkk.shape % DS_N]); return "SHAPE";
    case UI_GSTEP:
        sp_fix(sp_put(sp_fix(v, (int32_t)(ui.gstep % ui_d_steps()) + 1, 0u, 0), " OF "), (int32_t)ui_d_steps(), 0u, 0);
        return "STEP";
    case UI_GROW: sp_put(v, DK_NAME[ui_d_row()]); return "ROW";
    case UI_GVEL: case UI_GACC: case UI_GCHN: case UI_GRAT: {
        uint32_t f = id - UI_GVEL;
        int32_t x = lp_cur_clip()->len ? lp_grid_attr(ui_d_row(), ui.gstep % ui_d_steps(), f, 0) : -1;
        if (x < 0)
            sp_put(v, "\x97");
        else if (f == 0u)
            sp_fix(v, x, 0u, 0);
        else if (f == 1u)
            sp_put(v, x ? "ON" : "OFF");
        else if (f == 2u)
            sp_put(sp_fix(v, (int32_t)((8 - x) * 100 + 4) / 8, 0u, 0), " %");
        else
            sp_put(sp_fix(v, x + 1, 0u, 0), x ? " HITS" : " HIT");
        return f == 0u ? "VELOCITY" : f == 1u ? "ACCENT" : f == 2u ? "CHANCE" : "RATCHET";
    }
    default: return "";
    }
}

/* ------------------------------------------------------------------------------------------- pages --- */
/* knobs k0 .. k0 + 2 in a row under a page's drawing (as the looper's: a word over the value each) */
static void ui_d_knobs(uint32_t k0, const int32_t *ids)
{
    static const float CX[3] = {26.0f, 104.0f, 182.0f};
    char v[40], f[40];
    uint32_t i;
    for (i = 0; i < 3u; i++) {
        const char *nm;
        if (ids[i] < 0)
            continue;
        nm = ui_drum_nv((uint32_t)ids[i], v);
        lk_text(CX[i], 176.0f, T9, nm, sk.lab, sk.ground, 20, 0);
        lk_text(CX[i], 188.0f, T10B, lk_fit(f, sizeof f, v, T10B, 0, i < 2u ? 74.0f : 50.0f), ui_ink(ui_hot(k0 + i)),
                sk.ground, 0, 0);
        ui_H(k0 + i, CX[i] - 18.0f, 185.0f);
    }
    pg.figs = 0xFu;
}
/* KIT's title: the drum picked (KNOB 1), the eight under it, the picked one boxed */
static void ui_d_lanes(void)
{
    uint32_t l, cur = ui_d_lane();
    lk_text(UI_TITLE_X, 24.0f, T14B, DK_NAME[cur], ui_ink(ui_hot(0)), sk.ground, 0, 17);
    ui_H(0, 8.0f, 27.0f);
    for (l = 0; l < DK_N; l++) {
        float x = 26.0f + 52.0f * (float)(l % 4u), y = 46.0f + 13.0f * (float)(l / 4u);
        if (l == cur) {
            lk_box(x - 3.0f, y - 1.0f, x + lk_tw(T9B, DK_NAME[l], 20) + 3.0f, y + 11.0f, ui_ink(ui_hot(0)));
            lk_text(x, y, T9B, DK_NAME[l], sk.ground, ui_ink(ui_hot(0)), 20, 0);
        } else {
            lk_text(x, y, T9, DK_NAME[l], sk.lab, sk.ground, 20, 0);
        }
    }
}

/* DRUM: its fall over time (the amplitude to -60 dB; KICK's sweep dashed, CLAP's bursts), DECAY's time marked */
static void ui_p_kdrum(void)
{
    static const int32_t IDS[3] = {UI_KTUNE, UI_KDEC, UI_KTONE};
    uint32_t l = ui_d_lane(), i;
    const dk_lane_t *k = &dkk.l[l];
    float t60 = dk_t60(l, k->decay), span = t60 * 1.25f, x0 = 26.0f, x1 = 226.0f, y0 = 82.0f, y1 = 158.0f;
    float px = x0, py = y0, sp = l == DK_CLAP ? 0.004f + 0.016f * (float)k->chr * 0.01f : 0.0f;
    char t[40];
    ui_d_lanes();
    lk_seg(x0, y1 + 0.5f, x1, y1 + 0.5f, 1.0f, sk.line);
    for (i = 1; i <= 48u; i++) {                    /* (the level, dB on a linear scale: -60 at the bottom) */
        float tt = span * (float)i / 48.0f, db = -60.0f * tt / t60, x = x0 + (x1 - x0) * (float)i / 48.0f, y;
        if (sp > 0.0f && tt < 3.0f * sp)            /* (CLAP: each burst a fall of 25 ms, the next from the top) */
            db = -60.0f * (tt - sp * (float)sy_floori(tt / sp)) / 0.025f;
        else if (sp > 0.0f)
            db = -60.0f * (tt - 3.0f * sp) / t60;
        y = y0 + (y1 - y0) * sy_minf(1.0f, -db / 60.0f);
        lk_seg(px, py, x, y, 1.5f, ui_ink(ui_hot(2)));
        px = x, py = y;
    }
    if (l == DK_KICK) {                             /* (the sweep: from PUNCH's height down to the pitch) */
        float p0 = 1.0f + 6.0f * (float)k->chr * 0.01f, qx = x0, qy = 0.0f;
        for (i = 0; i <= 24u; i++) {
            float tt = span * (float)i / 24.0f, f = 1.0f + p0 * sy_exp2f(-9.9658f * tt / 0.2f), x = x0 + (x1 - x0) * (float)i / 24.0f;
            float y = y1 - (y1 - y0) * 0.9f * f / (1.0f + p0);
            if (i)
                lk_seg_dash(qx, qy, x, y, sk.mid, 2.0f, 2.0f);
            qx = x, qy = y;
        }
        lk_text_r(x1, y0 - 14.0f, T9, "PITCH DASHED", sk.lab, sk.ground, 20, 0);
    }
    {                                               /* (DECAY's time: a tick on the axis, its words) */
        float xd = x0 + (x1 - x0) * t60 / span;
        lk_seg(xd, y1 - 3.0f, xd, y1 + 4.0f, 1.0f, ui_ink(ui_hot(2)));
        ui_d_time(sp_put(t, "-60 DB AT "), t60);
        lk_text_r(x1, y1 + 6.0f, T9, t, sk.lab, sk.ground, 20, 0);
    }
    ui_d_knobs(1u, IDS);
}

/* MORE: the eight as a mixer (LEVEL a fader, PAN a dot over it), the one picked framed */
static void ui_p_kmore(void)
{
    static const int32_t IDS[3] = {UI_KCHR, UI_KLVL, UI_KPAN};
    static const char *const AB[DK_N] = {"BD", "SD", "CP", "CH", "OH", "CY", "W1", "W2"};
    uint32_t l, cur = ui_d_lane();
    ui_d_lanes();
    for (l = 0; l < DK_N; l++) {
        const dk_lane_t *k = &dkk.l[l];
        float x = 30.0f + 25.0f * (float)l, cx = x + 12.5f, top = 82.0f, bot = 150.0f;
        float lv = bot - (bot - top) * (float)k->level / 127.0f, px = cx + 8.0f * ((float)k->pan - 64.0f) / 63.0f;
        int me = l == cur;
        lk_seg(cx - 8.0f, 76.0f, cx + 8.0f, 76.0f, 1.0f, sk.line);
        lk_disc(px, 76.0f, 2.0f, me && ui_hot(3) ? sk.hot : sk.ink);
        lk_seg(cx, top, cx, bot, 1.0f, sk.line);
        lk_box(cx - 3.0f, lv, cx + 3.0f, bot, me && ui_hot(2) ? sk.hot : me ? sk.ink : sk.mid);
        lk_text_c(cx, 154.0f, me ? T9B : T9, AB[l], me ? sk.ink : sk.lab, sk.ground, 0, 0);
        if (me)
            lk_frame(x + 0.5f, 70.5f, 24.0f, 96.0f, 1.0f, sk.mid);
    }
    ui_d_knobs(1u, IDS);
}

/* ACCENT: a hit's gain by its velocity, plain (full) and accented (HOT when turning ACCENT), VELOCITY's share */
static void ui_p_kacc(void)
{
    static const int32_t IDS[3] = {UI_KACC, UI_KVSN, -1};
    float x0 = 30.0f, x1 = 226.0f, y0 = 50.0f, y1 = 158.0f, s = (float)dkk.vsens * 0.01f, a = 1.0f + (float)dkk.accent * 0.01f;
    float px = 0.0f, py = 0.0f, qy = 0.0f;
    uint32_t i;
    ui_title("ACCENT", UI_TITLE_X);
    lk_seg(x0, y1 + 0.5f, x1, y1 + 0.5f, 1.0f, sk.line);
    lk_seg(x0 - 0.5f, y0, x0 - 0.5f, y1, 1.0f, sk.line);
    lk_text_r(x1, y1 + 4.0f, T9, "VELOCITY 1 .. 127", sk.lab, sk.ground, 20, 0);
    lk_text(x0 + 4.0f, y0, T9, "GAIN", sk.lab, sk.ground, 20, 0);
    for (i = 0; i <= 32u; i++) {
        float v = (float)i / 32.0f, g = 1.0f - s + s * v * v, x = x0 + (x1 - x0) * v;
        float y = y1 - (y1 - y0 - 14.0f) * g / 2.0f, ya = y1 - (y1 - y0 - 14.0f) * sy_minf(2.0f, g * a) / 2.0f;
        if (i) {
            lk_seg(px, py, x, y, 1.5f, ui_ink(ui_hot(1)));
            lk_seg(px, qy, x, ya, 1.5f, ui_hot(0) ? sk.hot : sk.mid);
        }
        px = x, py = y, qy = ya;
    }
    lk_text_r(x1, y1 - (y1 - y0 - 14.0f) * a / 2.0f - 12.0f, T9, "ACCENTED", sk.lab, sk.ground, 20, 0);
    ui_d_knobs(0u, IDS);
}

/* DUCK: the synth's level from a ghost hit (0): down by AMOUNT over ATTACK, back over RELEASE along SHAPE */
static void ui_p_kduck(void)
{
    static const int32_t IDS[3] = {UI_DATT, UI_DREL, UI_DSHP};
    float att = (float)dkk.attack * 0.001f, rel = dk_rel_s(), span = (att + rel) * 1.25f + 0.02f;
    float x0 = 30.0f, x1 = 226.0f, y0 = 64.0f, y1 = 150.0f, am = (float)dkk.amount * 0.01f, px = x0, py = y0;
    uint32_t i;
    char t[40];
    sp_put(sp_put(sp_fix(sp_put(t, "DUCK "), dkk.amount, 0u, 0), " %"), "");
    lk_text(UI_TITLE_X, 24.0f, T14B, t, ui_ink(ui_hot(0)), sk.ground, 0, 17);
    ui_H(0, 8.0f, 27.0f);
    lk_text(UI_TITLE_X, 44.0f, T9, "THE SYNTH'S LEVEL, A DUCK HIT AT 0", sk.lab, sk.ground, 20, 0);
    lk_seg(x0, y1 + 0.5f, x1, y1 + 0.5f, 1.0f, sk.line);
    lk_seg_dash(x0, y0, x1, y0, sk.line, 1.0f, 3.0f);
    for (i = 1; i <= 64u; i++) {
        float tt = span * (float)i / 64.0f - 0.01f, d, x = x0 + (x1 - x0) * (float)i / 64.0f, y;
        if (tt < 0.0f)
            d = 0.0f;
        else if (tt < att)
            d = tt / att;
        else {
            float r = sy_maxf(0.0f, 1.0f - (tt - att) / rel);
            d = dkk.shape == DS_EXP ? r * r * r : dkk.shape == DS_PUMP ? 1.0f - (1.0f - r) * (1.0f - r) * (1.0f - r) : r;
        }
        y = y0 + (y1 - y0) * am * d;
        lk_seg(px, py, x, y, 1.5f, ui_ink(ui_hot(0) || ui_hot(1) || ui_hot(2) || ui_hot(3)));
        px = x, py = y;
    }
    {
        float xa = x0 + (x1 - x0) * 0.01f / span;
        lk_tri(xa - 3.5f, y1 + 9.0f, xa + 3.5f, y1 + 9.0f, xa, y1 + 4.0f, sk.mid);
    }
    ui_d_time(sp_put(t, "BACK IN "), att + rel);
    lk_text_r(x1, y1 + 6.0f, T9, t, sk.lab, sk.ground, 20, 0);
    ui_d_knobs(1u, IDS);
}

/* GRID, HIT: the clip's sixteen steps shown, a row a drum and DUCK; a hit a box (its velocity its brightness; ACCENT
 * the cell's full height; CHANCE under 100 % hollow; RATCHET its hits as slivers), DUCK's as frames; the step picked
 * framed, the playhead over its step; the beats ticked under */
#define UI_GX0 48.0f
#define UI_GDX 11.0f
#define UI_GY0 60.0f
#define UI_GDY 12.0f
static void ui_d_grid(int hit)
{
    const lp_clip_t *c = lp_cur_clip();
    uint32_t t = lpp.sel % LP_NTRK, p0 = ui_d_page0(), n = ui_d_steps(), r, s, k, cur = ui.gstep % n;
    uint32_t bt = lp_bar_ticks() / LP_STEP, qt = (uint32_t)(sy_beat_q * 4.0f + 0.5f);
    char tx[48], *e;
    int hot = ui_hot(0) || ui_hot(1);
    e = sp_put(sp_fix(sp_put(sp_put(sp_put(sp_fix(sp_put(tx, "T"), (int32_t)t + 1, 0u, 0), " \xB7 "),
                                    (char[2]){(char)('A' + lpp.scn % LP_NSCN), 0}), " \xB7 STEPS "),
                             (int32_t)p0 + 1, 0u, 0), "-");
    sp_fix(sp_put(sp_fix(e, (int32_t)(p0 + 16u < n ? p0 + 16u : n), 0u, 0), " OF "), (int32_t)n, 0u, 0);
    if (!c->len)
        sp_put(sp_put(tx, ""), "A WHITE KEY STARTS A BAR");
    lk_text(UI_TITLE_X, 40.0f, T10, tx, lp_cur_rec() ? sk.hot : sk.lab, sk.ground, 20, 12);
    for (r = 0; r < DK_ROWS; r++) {                 /* (the rows' names, the one picked bold) */
        float y = UI_GY0 + UI_GDY * (float)r;
        lk_text(8.0f, y + 1.0f, r == ui_d_row() ? T9B : T9, DK_NAME[r], r == ui_d_row() ? ui_ink(ui_hot(1)) : sk.lab,
                sk.ground, 0, 0);
        for (s = 0; s < 16u && p0 + s < n; s++)    /* (an empty step: a dot, a beat's first a little brighter) */
            lk_box(UI_GX0 + UI_GDX * (float)s + 4.5f, y + 5.0f, UI_GX0 + UI_GDX * (float)s + 6.5f, y + 7.0f,
                   qt && (p0 + s) % qt == 0u ? sk.mid : sk.line);
    }
    for (s = 0; s <= 16u && p0 + s <= n; s++)       /* (the beats and bars: ticks under) */
        if (qt && (p0 + s) % qt == 0u)
            lk_seg(UI_GX0 + UI_GDX * (float)s, UI_GY0 + UI_GDY * 9.0f + 1.0f, UI_GX0 + UI_GDX * (float)s,
                   UI_GY0 + UI_GDY * 9.0f + (bt && (p0 + s) % bt == 0u ? 6.0f : 3.0f), 1.0f, bt && (p0 + s) % bt == 0u ? sk.mid : sk.line);
    for (k = c->head; k != LP_END; k = lp_ev[k].next) {   /* (the hits of the steps shown) */
        uint32_t et = lp_et(k), st = et / LP_STEP, row, len, ac, ch, ra;
        float x, y, h;
        uint16_t col;
        if ((lp_ev[k].t & LP_MOVE) || st < p0 || st >= p0 + 16u || (row = lp_drum_row(lp_ev[k].a)) >= DK_ROWS)
            continue;
        len = lp_ev[k].len & LP_HIT ? lp_ev[k].len : LP_HIT;
        ac = (len & LP_ACC) != 0u, ch = LP_CHN(len), ra = LP_RAT(len);
        x = UI_GX0 + UI_GDX * (float)(st - p0) + 1.0f;
        y = UI_GY0 + UI_GDY * (float)row + (ac ? 1.0f : 2.5f);
        h = ac ? 10.0f : 7.0f;
        col = row == DK_DUCK ? sk.mid : sy_mix565(sk.mid, sk.ink, (uint32_t)(lp_ev[k].b & 0x7F) * 2u);
        if (st == cur && row == ui_d_row() && (ui_hot(2) || ui_hot(3)))
            col = sk.hot;                           /* (its VELOCITY, ACCENT, CHANCE or RATCHET turning) */
        if (row == DK_DUCK || ch) {
            lk_box(x, y, x + UI_GDX - 2.0f, y + h, sk.ground);   /* (over the empty step's dot) */
            lk_frame(x + 0.5f, y + 0.5f, UI_GDX - 3.0f, h - 1.0f, 1.0f, col);
            if (ch && row != DK_DUCK)               /* (CHANCE: filled as far as it is heard) */
                lk_box(x + 1.0f, y + h - 1.0f - (h - 2.0f) * (float)(8u - ch) / 8.0f, x + UI_GDX - 3.0f, y + h - 1.0f, col);
        } else if (ra) {
            uint32_t j;
            float w = (UI_GDX - 2.0f) / (float)(ra + 1u);
            for (j = 0; j <= ra; j++)
                lk_box(x + w * (float)j, y, x + w * (float)j + sy_maxf(w - 1.0f, 1.0f), y + h, col);
        } else {
            lk_box(x, y, x + UI_GDX - 2.0f, y + h, col);
        }
    }
    if (cur >= p0 && cur < p0 + 16u)               /* (the step picked: framed) */
        lk_frame(UI_GX0 + UI_GDX * (float)(cur - p0) - 0.5f, UI_GY0 + UI_GDY * (float)ui_d_row() - 0.5f, UI_GDX + 1.0f,
                 UI_GDY + 1.0f, 1.0f, hot ? sk.hot : sk.ink);
    if (cht.run && lps[t].play == lpp.scn && c->len) {   /* (the playhead: over its step) */
        uint32_t ps = lps[t].pos / LP_STEP;
        if (ps >= p0 && ps < p0 + 16u) {
            float xp = UI_GX0 + UI_GDX * ((float)(ps - p0) + 0.5f) - 0.5f;
            lk_tri(xp - 3.5f, UI_GY0 - 6.0f, xp + 3.5f, UI_GY0 - 6.0f, xp, UI_GY0 - 1.0f, sk.hot);
        }
    }
    {                                               /* (KNOB 1: the step, in the title) */
        char v[24];
        ui_drum_nv(UI_GSTEP, v);
        sp_put(sp_put(tx, "STEP "), v);
        lk_text(UI_TITLE_X, 22.0f, T14B, tx, ui_ink(ui_hot(0)), sk.ground, 0, 17);
        ui_H(0, 8.0f, 25.0f);
    }
}
static void ui_p_lgrid(void)
{
    static const int32_t IDS[3] = {UI_GROW, UI_GVEL, UI_GACC};
    ui_d_grid(0);
    ui_d_knobs(1u, IDS);
}
static void ui_p_lhit(void)
{
    static const int32_t IDS[3] = {UI_GROW, UI_GCHN, UI_GRAT};
    ui_d_grid(1);
    ui_d_knobs(1u, IDS);
}
