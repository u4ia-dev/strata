/* SPDX-License-Identifier: GPL-3.0-only */
/* The STRATA UI's drums (V0.85; the drum synth: sy_drum.c, the looper's side: sy_loop.c; docs/CONTROLS.md "The drums"):
 *   - EDIT on a drum track (V0.86, step 2; K1, K2, K7): DRUM, the drum PRESETS picks: SOUND (TUNE, DECAY, TONE (WT 1,
 *     WT 2: FRAME), its own: PUNCH, SNAP, SPREAD, METAL or BEND; its fall drawn), DRIVE (TYPE, DRIVE, MIX: what comes
 *     out for what goes in), EQ (LOW, MID, HIGH, FREQ: the response), SHAPE (ATTACK, SUSTAIN: its fall shaped), MIX
 *     (LEVEL, PAN: the eight as a mixer); KIT, the kit's: ACCENT (ACCENT's lift, VELOCITY's share, TONE: a soft hit
 *     darker), DUCK (SOURCE, AMOUNT, ATTACK, RELEASE: the synth's level from a hit), CURVE (SHAPE, HOLD), COMP
 *     (AMOUNT, ATTACK, RELEASE, MIX: its curve). Stopped, the drum played is the one picked on DRUM's pages;
 *   - CLIP on a drum track: GRID (the clip in sixteenths, a row a drum and the DUCK row; the step picked, its VELOCITY,
 *     ACCENT), HIT (its CHANCE, RATCHET), CLIP (as a synth track's), TOOLS. On GRID and HIT the white keys put in or
 *     take out the row's hits on the sixteen steps shown (stopped: heard as they go in), the black keys pick the row
 *     (1 KICK .. 9 DUCK); their LEDs show the steps and the rows. V0.86 (S7): a step's key held, the four knobs set
 *     its hit's VELOCITY, ACCENT, CHANCE, RATCHET; a hit there taken out at the release only when no knob turned.
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
/* a 0 .. 100 knob of the kit's turned: by ACCEL; centre: a bipolar one's (50 the middle: F26) */
static uint8_t ui_d_pct(int32_t item, uint8_t v, int32_t s, uint32_t e, int centre)
{
    int32_t n = sy_clampi(v + ui_accel(e, s, 100), 0, 100);
    return (uint8_t)(centre ? ui_centre(item, s, v, n, 50) : n);
}
static void ui_drum_turn(int32_t item, int32_t s, uint32_t e)
{
    uint32_t l = ui_d_lane();
    dk_lane_t *k = &dkk.l[l];
    dk_fx_t *f = &dkk.fx[l];
    int kit = 1;                                    /* (V0.86: the kit edited since loaded: HOME's dot) */
    switch (item) {
    case UI_KLANE: ui.krow = (uint8_t)sy_clampi((int32_t)ui_d_lane() + s, 0, DK_N - 1); kit = 0; break;
    case UI_KMODEL: case UI_KVAR: {                 /* (V0.87, step 2: the row's model, its variant; a new model: the
                                                     * row no longer its preset, its place's name) */
        uint32_t vr, m = dk_model(l, &vr), m2 = item == UI_KMODEL ? (uint32_t)sy_clampi((int32_t)m + s, 0, DM_N - 1) : m;
        uint32_t v2 = item == UI_KVAR ? (uint32_t)sy_clampi((int32_t)vr + s, 0, (int32_t)DM_NVAR[m] - 1) : m2 == m ? vr : 0u;
        if (m2 == m && v2 == vr)
            return;
        k->model = (uint8_t)(m2 + 1u), k->var = (uint8_t)v2;
        if (m2 != m)
            *dk_pid(l) = 0;
        break;
    }
    case UI_EHITS: case UI_ESTEPS: case UI_EROT: {  /* (V0.87, step 2: EUCLID: the row picked refilled) */
        if (item == UI_ESTEPS)
            ui.e_steps = (uint8_t)sy_clampi(ui.e_steps + s, 1, 16);
        else if (item == UI_EHITS)
            ui.e_hits = (uint8_t)sy_clampi(ui.e_hits + s, 0, 16);
        else
            ui.e_rot = (uint8_t)sy_clampi(ui.e_rot + s, 0, 15);
        ui.e_hits = (uint8_t)(ui.e_hits > ui.e_steps ? ui.e_steps : ui.e_hits);
        ui.e_rot = (uint8_t)(ui.e_rot % ui.e_steps);
        if (lp_grid_euclid(ui_d_row(), ui.e_hits, ui.e_steps, ui.e_rot, 100u) < 0)
            ui_toast("NOT NOW: A TAKE RECORDING, OR FULL");
        kit = 0;
        break;
    }
    case UI_KTUNE: k->tune = (int8_t)ui_centre(item, s, k->tune, sy_clampi(k->tune + s, -24, 24), 0); break;
    case UI_KDEC: k->decay = (uint8_t)sy_clampi(k->decay + ui_accel(e, s, 100), 0, 100); break;
    case UI_KTONE: k->tone = (uint8_t)sy_clampi(k->tone + ui_accel(e, s, 100), 0, 100); break;
    case UI_KCHR: k->chr = (uint8_t)sy_clampi(k->chr + ui_accel(e, s, 100), 0, 100); break;
    case UI_KLVL: k->level = (uint8_t)sy_clampi(k->level + ui_accel(e, s, 127), 0, 127); break;
    case UI_KPAN: k->pan = (uint8_t)ui_centre(item, s, k->pan, sy_clampi(k->pan + ui_accel(e, s, 126), 1, 127), 64); break;
    case UI_KACC: dkk.accent = (uint8_t)sy_clampi(dkk.accent + ui_accel(e, s, 100), 0, 100); break;
    case UI_KVSN: dkk.vsens = (uint8_t)sy_clampi(dkk.vsens + ui_accel(e, s, 100), 0, 100); break;
    case UI_DAMT: dkk.amount = (uint8_t)sy_clampi(dkk.amount + ui_accel(e, s, 100), 0, 100); break;
    case UI_DATT: dkk.attack = (uint8_t)sy_clampi(dkk.attack + ui_accel(e, s, 50), 0, 50); break;
    case UI_DREL: dkk.release = (uint8_t)sy_clampi(dkk.release + ui_accel(e, s, 100), 0, 100); break;
    case UI_DSHP: dkk.shape = (uint8_t)sy_clampi(dkk.shape + s, 0, DS_N - 1); break;
    case UI_KDTYPE: f->dtype = (uint8_t)sy_clampi(f->dtype + s, 0, DD_N - 1); break;   /* (V0.86, step 2: a drum's */
    case UI_KDRIVE: f->drive = ui_d_pct(item, f->drive, s, e, 0); break;               /* DRIVE, EQ, SHAPE; the kit's */
    case UI_KDMIX: f->dmix = ui_d_pct(item, f->dmix, s, e, 0); break;                  /* ACCENT TONE, the duck's */
    case UI_KEQL: f->lo = ui_d_pct(item, f->lo, s, e, 1); break;                       /* SOURCE and HOLD, COMP) */
    case UI_KEQM: f->mid = ui_d_pct(item, f->mid, s, e, 1); break;
    case UI_KEQH: f->hi = ui_d_pct(item, f->hi, s, e, 1); break;
    case UI_KEQF: f->freq = ui_d_pct(item, f->freq, s, e, 0); break;
    case UI_KSATT: f->att = ui_d_pct(item, f->att, s, e, 1); break;
    case UI_KSSUS: f->sus = ui_d_pct(item, f->sus, s, e, 1); break;
    case UI_KATONE: dkk.atone = ui_d_pct(item, dkk.atone, s, e, 0); break;
    case UI_DSRC: dkk.dsrc = (uint8_t)sy_clampi(dkk.dsrc + s, 0, DSRC_N - 1); break;
    case UI_DHOLD: dkk.dhold = ui_d_pct(item, dkk.dhold, s, e, 0); break;
    case UI_KCAMT: case UI_KCATT: case UI_KCREL: case UI_KCMIX: {
        uint32_t i = (uint32_t)(item - UI_KCAMT);
        dkk.comp[i] = ui_d_pct(item, dkk.comp[i], s, e, 0);
        break;
    }
    case UI_GSTEP: ui.gstep = (uint16_t)sy_clampi((int32_t)(ui.gstep % ui_d_steps()) + s, 0, (int32_t)ui_d_steps() - 1); kit = 0; break;
    case UI_GROW: ui.grow = (uint8_t)sy_clampi((int32_t)ui_d_row() + s, 0, DK_ROWS - 1); kit = 0; break;
    case UI_GVEL: case UI_GACC: case UI_GCHN: case UI_GRAT: {
        uint32_t g = (uint32_t)(item - UI_GVEL);
        if (lp_grid_attr(ui_d_row(), ui.gstep % ui_d_steps(), g, g ? s : ui_accel(e, s, 126)) < 0) {
            ui_toast("NO HIT: A WHITE KEY PUTS ONE");
            return;
        }
        ui.ghold_turn = 1;                          /* (V0.86: held, the hit stays at the release) */
        kit = 0;
        break;
    }
    default: kit = 0; break;
    }
    if (kit)
        ui.kedited = 1;
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
    if (ui.ghold)                                   /* (V0.86: a step held: no other key till it goes) */
        return 1;
    if (black) {
        if (w < DK_ROWS) {
            ui.grow = (uint8_t)w;
            ui.dirty |= D_KNOBS | D_DISP;
        }
        return 1;
    }
    st = ui_d_page0() + w;
    if (st < ui_d_steps() && lp_cur_clip()->len && lp_grid_at(lp_cur_clip(), ui_d_row(), st) != LP_END) {
        ui.gstep = (uint16_t)st;                    /* (S7: a hit there held: the knobs its own; out at the release */
        ui.ghold = 1, ui.ghold_k = (uint8_t)k;      /* unless one turned) */
        ui.ghold_new = 0, ui.ghold_turn = 0;
        ui.dirty = D_ALL;
        return 1;
    }
    if (k == 0u || k == 26u) {
        uint32_t i = k ? 1u : 0u;                   /* (V0.875: F3 or G5 is a DEBUG corner: the clip it may make kept, for the opening) */
        ui.dbg_gt[i] = (uint8_t)(lpp.sel % LP_NTRK), ui.dbg_gc[i] = (uint8_t)(lpp.scn % LP_NSCN);
        ui.dbg_ge = (uint8_t)((ui.dbg_ge & ~(1u << i)) | (!lp_cur_clip()->len ? 1u << i : 0u));
    }
    r = lp_grid_toggle(ui_d_row(), st, sy_ui_key_vel());
    if (r < 0) {
        ui_toast(lp_cur_rec() && lpr.st == LR_TAKE ? "THE TAKE SETS THIS CLIP" : st >= ui_d_steps() ? "PAST THE CLIP'S END"
                                                                                                    : "FULL: 2048 NOTES AND MOVES");
        return 1;
    }
    ui.gstep = (uint16_t)st;
    if (r) {                                        /* (put in: held, the knobs its own) */
        ui.ghold = 1, ui.ghold_k = (uint8_t)k;
        ui.ghold_new = 1, ui.ghold_turn = 0;
        if (k == 0u || k == 26u) {                  /* (V0.875: the step kept for the opening, to take out) */
            uint32_t i = k ? 1u : 0u;
            ui.dbg_ok |= (uint8_t)(DBG_GR0 << i);
            ui.dbg_gr[i] = (uint8_t)ui_d_row();
            ui.dbg_gs[i] = (uint16_t)st;
        }
    }
    if (r && !cht.run && lpr.st == LR_OFF)
        sy_app_drum(DK_NOTE[ui_d_row()], sy_ui_key_vel());
    sp_put(sp_put(sp_fix(sp_put(sp_put(sp_put(m, dk_rname(ui_d_row())), " "), "STEP "), (int32_t)st + 1, 0u, 0), " "),
           r ? "IN" : "OUT");
    ui_toast(m);
    ui.dirty = D_ALL;
    return 1;
}
/* V0.875 (item 25): the steps F3 and G5 put in (ui.dbg_g*) taken out, in the clip each went into (the selection may have
 * moved since); a clip a step made (it was empty) with nothing left in it cleared again: its length, and the launch that
 * came with it (lp_clip_clear) */
static void ui_d_ungrid(void)
{
    uint32_t i, k, fl;
    for (i = 0; i < 2u; i++) {
        lp_clip_t *c = &lpp.clip[ui.dbg_gt[i] % LP_NTRK][ui.dbg_gc[i] % LP_NSCN];
        if (!(ui.dbg_ok & (DBG_GR0 << i)) || !c->len)
            continue;
        fl = LP_LOCK();
        while ((k = lp_grid_at(c, ui.dbg_gr[i], ui.dbg_gs[i])) != LP_END) {
            lp_unlink(c, k);
            lp_forget(k);
            lp_release(k);
        }
        lps[ui.dbg_gt[i] % LP_NTRK].resync = 1;
        LP_UNLOCK(fl);
    }
    for (i = 0; i < 2u; i++)
        if (((ui.dbg_ge >> i) & 1u) && (ui.dbg_ok & (DBG_GR0 << i)) &&
            lpp.clip[ui.dbg_gt[i] % LP_NTRK][ui.dbg_gc[i] % LP_NSCN].len &&
            lpp.clip[ui.dbg_gt[i] % LP_NTRK][ui.dbg_gc[i] % LP_NSCN].head == LP_END)
            lp_clip_clear(ui.dbg_gt[i], ui.dbg_gc[i]);
    ui.dbg_ge = 0;
    lpx.seq++;
}
/* .. the step held let go (or its page gone): a hit that was there, no knob turned, taken out (a tap's toggle) */
static void ui_grid_ms(void)
{
    char m[40];
    if (!ui.ghold || ((fm1_in.notes >> ui.ghold_k) & 1u && ui_grid_keys()))
        return;
    if (ui_grid_keys() && !ui.ghold_new && !ui.ghold_turn &&
        lp_grid_toggle(ui_d_row(), ui.gstep % ui_d_steps(), sy_ui_key_vel()) == 0) {
        sp_put(sp_fix(sp_put(sp_put(m, dk_rname(ui_d_row())), " STEP "), (int32_t)(ui.gstep % ui_d_steps()) + 1, 0u, 0),
               " OUT");
        ui_toast(m);
    }
    ui.ghold = 0;
    ui.dirty = D_ALL;
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
    ui_grid_ms();
    if (cht.run) {                                  /* (running: the clips' hits are not the one played) */
        dk_last = 0xFF;
        return;
    }
    if (l < DK_N && ui_sec() == SEC_DRUM) {
        dk_last = 0xFF;
        if (l != ui.krow) {
            ui.krow = l;
            ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
        }
    }
}

/* ------------------------------------------------------------------------------------------- values --- */
static char *ui_d_db(char *v, int32_t x, float span)   /* a 0 .. 100 knob (50 the middle) as dB: "+3.0 DB", "0 DB" */
{
    int32_t t = (int32_t)sy_floori(((float)x - 50.0f) * span * 0.2f + (x >= 50 ? 0.5f : -0.5f));   /* (tenths) */
    if (x == 50)
        return sp_put(v, "0 DB");
    return sp_put(sp_fix(v, t, 1u, 1), " DB");
}
static const char *ui_drum_nv(uint32_t id, char *v)
{
    uint32_t l = ui_d_lane();
    const dk_lane_t *k = &dkk.l[l];
    const dk_fx_t *f = &dkk.fx[l];
    v[0] = 0;
    switch (id) {
    case UI_KLANE: sp_put(v, dk_rname(l)); return "DRUM";
    case UI_KMODEL: {                               /* (V0.87, step 2) */
        uint32_t vr, m = dk_model(l, &vr);
        sp_put(v, DM_NAME[m]);
        return "MODEL";
    }
    case UI_KVAR: {
        uint32_t vr, m = dk_model(l, &vr);
        sp_put(v, DM_VAR[m][vr]);
        return "TYPE";
    }
    case UI_EHITS: sp_fix(v, ui.e_hits, 0u, 0); return "HITS";
    case UI_ESTEPS: sp_fix(v, ui.e_steps, 0u, 0); return "STEPS";
    case UI_EROT: sp_fix(v, ui.e_rot, 0u, 0); return "ROTATE";
    case UI_KTUNE:
        if (k->tune)
            sp_put(sp_fix(v, k->tune, 0u, 1), " ST");
        else
            sp_put(v, "0");
        return "TUNE";
    case UI_KDEC: ui_d_time(v, dk_t60(l, k->decay)); return "DECAY";
    case UI_KTONE: {
        uint32_t vr, m = dk_model(l, &vr);
        if (m == DM_WAVE) {
            const wt_table_t *t = &wt_tab[vr & 1u];
            if (t->n)
                sp_fix(sp_put(sp_fix(v, sy_floori((float)k->tone * 0.01f * (float)(t->n - 1u) + 0.5f) + 1, 0u, 0), " OF "),
                       t->n, 0u, 0);
            else
                sp_put(v, "NO TABLE");
            return "FRAME";
        }
        sp_fix(v, k->tone, 0u, 0);
        return dk_tone_name(m, vr);
    }
    case UI_KCHR: {
        uint32_t vr, m = dk_model(l, &vr);
        sp_fix(v, k->chr, 0u, 0);
        return DM_CHR[m][vr];
    }
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
    case UI_KDTYPE: sp_put(v, DD_NAME[f->dtype % DD_N]); return "TYPE";   /* (V0.86, step 2) */
    case UI_KDRIVE:
        if (!f->drive)
            sp_put(v, "OFF");
        else if (f->dtype == DD_CRUSH)
            sp_put(sp_fix(v, sy_floori(15.0f - 13.0f * (float)f->drive * 0.01f + 0.5f), 0u, 0), " BITS");
        else
            sp_put(sp_fix(v, (int32_t)(24.0f * (float)f->drive * 0.1f + 0.5f), 1u, 1), " DB");
        return "DRIVE";
    case UI_KDMIX: sp_put(sp_fix(v, f->dmix, 0u, 0), " %"); return "MIX";
    case UI_KEQL: ui_d_db(v, f->lo, 12.0f); return "LOW";
    case UI_KEQM: ui_d_db(v, f->mid, 12.0f); return "MID";
    case UI_KEQH: ui_d_db(v, f->hi, 12.0f); return "HIGH";
    case UI_KEQF: {
        float hz = 200.0f * sy_exp2f(5.3219281f * (float)f->freq * 0.01f);
        if (hz < 999.5f)
            sp_put(sp_fix(v, sy_floori(hz + 0.5f), 0u, 0), " HZ");
        else
            sp_put(sp_fix(v, sy_floori(hz * 0.01f + 0.5f), 1u, 0), "K");
        return "FREQ";
    }
    case UI_KSATT: ui_d_db(v, f->att, 12.0f); return "ATTACK";
    case UI_KSSUS: ui_d_db(v, f->sus, 12.0f); return "SUSTAIN";
    case UI_KATONE: sp_put(sp_fix(v, dkk.atone, 0u, 0), " %"); return "TONE";
    case UI_DSRC: sp_put(v, DSRC_NAME[dkk.dsrc % DSRC_N]); return "SOURCE";
    case UI_DHOLD: sp_put(sp_fix(v, (int32_t)((float)dkk.dhold * DK_HOLD_MS + 0.5f), 0u, 0), " MS"); return "HOLD";
    case UI_KCAMT: sp_put(sp_fix(v, dkk.comp[0], 0u, 0), " %"); return "AMOUNT";
    case UI_KCATT: {
        float t = 0.1f * sy_exp2f(8.2288f * (float)dkk.comp[1] * 0.01f);   /* (ms) */
        if (t < 9.95f)
            sp_put(sp_fix(v, sy_floori(t * 10.0f + 0.5f), 1u, 0), " MS");
        else
            sp_put(sp_fix(v, sy_floori(t + 0.5f), 0u, 0), " MS");
        return "ATTACK";
    }
    case UI_KCREL: ui_d_time(v, 0.02f * sy_exp2f(5.6439f * (float)dkk.comp[2] * 0.01f)); return "RELEASE";
    case UI_KCMIX: sp_put(sp_fix(v, dkk.comp[3], 0u, 0), " %"); return "MIX";
    case UI_GSTEP:
        sp_fix(sp_put(sp_fix(v, (int32_t)(ui.gstep % ui_d_steps()) + 1, 0u, 0), " OF "), (int32_t)ui_d_steps(), 0u, 0);
        return "STEP";
    case UI_GROW: sp_put(v, dk_rname(ui_d_row())); return "ROW";
    case UI_GVEL: case UI_GACC: case UI_GCHN: case UI_GRAT: {
        uint32_t g = id - UI_GVEL;
        int32_t x = lp_cur_clip()->len ? lp_grid_attr(ui_d_row(), ui.gstep % ui_d_steps(), g, 0) : -1;
        if (x < 0)
            sp_put(v, "\x97");
        else if (g == 0u)
            sp_fix(v, x, 0u, 0);
        else if (g == 1u)
            sp_put(v, x ? "ON" : "OFF");
        else if (g == 2u)
            sp_put(sp_fix(v, (int32_t)((8 - x) * 100 + 4) / 8, 0u, 0), " %");
        else
            sp_put(sp_fix(v, x + 1, 0u, 0), x ? " HITS" : " HIT");
        return g == 0u ? "VELOCITY" : g == 1u ? "ACCENT" : g == 2u ? "CHANCE" : "RATCHET";
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
/* V0.86: the four knobs two by two under the drawing (S7's held step; the drum editor's pages); ids -1: none */
static void ui_d_knobs4(const int32_t *ids)
{
    char v[40], f[40];
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        float x = i & 1u ? 134.0f : 26.0f, y = i & 2u ? 192.0f : 168.0f;
        const char *nm;
        if (ids[i] < 0)
            continue;
        nm = ui_drum_nv((uint32_t)ids[i], v);
        lk_text(x, y, T9, nm, sk.lab, sk.ground, 20, 0);
        lk_text(x, y + 11.0f, T10B, lk_fit(f, sizeof f, v, T10B, 0, 90.0f), ui_ink(ui_hot(i)), sk.ground, 0, 0);
        ui_H(i, x - 18.0f, y + 8.0f);
    }
    pg.figs = 0xFu;
}
/* DRUM's title (V0.86, K1: PRESETS picks the drum, its P): the drum picked, the eight under it, the picked one boxed */
static void ui_d_lanes(void)
{
    uint32_t l, cur = ui_d_lane();
    lk_text(UI_TITLE_X, 24.0f, T14B, dk_rname(cur), sk.ink, sk.ground, 0, 17);
    lk_ptag(10.0f, 28.0f, 0);
    for (l = 0; l < DK_N; l++) {
        float x = 26.0f + 52.0f * (float)(l % 4u), y = 44.0f + 13.0f * (float)(l / 4u);
        if (l == cur) {
            lk_box(x - 3.0f, y - 1.0f, x + lk_tw(T9B, dk_rname(l), 20) + 3.0f, y + 11.0f, sk.ink);
            lk_text(x, y, T9B, dk_rname(l), sk.ground, sk.ink, 20, 0);
        } else {
            lk_text(x, y, T9, dk_rname(l), sk.lab, sk.ground, 20, 0);
        }
    }
}
/* the drum's fall over time, from its hit: amplitude in dB (-60 at y1), each a column's; shaped: SHAPE's gain on it */
static float ui_d_fall_db(uint32_t l, float tt, float t60, int shaped)
{
    const dk_fx_t *f = &dkk.fx[l];
    float sp = l == DK_CLAP ? 0.004f + 0.016f * (float)dkk.l[l].chr * 0.01f : 0.0f, db;
    if (sp > 0.0f && tt < 3.0f * sp)                /* (CLAP: each burst a fall of 25 ms, the next from the top) */
        db = -60.0f * (tt - sp * (float)sy_floori(tt / sp)) / 0.025f;
    else
        db = -60.0f * (tt - 3.0f * sp) / t60;
    if (shaped > 0 && (f->att != 50u || f->sus != 50u)) {
        float a = (float)((int32_t)f->att - 50) * 0.24f, s = (float)((int32_t)f->sus - 50) * 0.24f, w = sy_exp2f(-tt / 0.005f * 1.4426950f);
        float g = sy_exp2f(s * 0.16609640f) + (sy_exp2f(a * 0.16609640f) - sy_exp2f(s * 0.16609640f)) * w;
        db += 20.0f * sy_log2f(sy_maxf(g, 1e-4f)) * 0.30103f;
    }
    return db;
}
static void ui_d_fall(uint32_t l, float x0, float x1, float y0, float y1, float span, float t60, int shaped, uint16_t c,
                      float top)               /* (y0: top dB, y1: -60 dB) */
{
    uint32_t i;
    float px = x0, py = y0 + (y1 - y0) * sy_clampf((top - ui_d_fall_db(l, 0.0f, t60, shaped)) / (top + 60.0f), 0.0f, 1.0f);
    for (i = 1; i <= 48u; i++) {
        float tt = span * (float)i / 48.0f, x = x0 + (x1 - x0) * (float)i / 48.0f;
        float y = y0 + (y1 - y0) * sy_clampf((top - ui_d_fall_db(l, tt, t60, shaped)) / (top + 60.0f), 0.0f, 1.0f);
        if (shaped >= 0)
            lk_seg(px, py, x, y, 1.5f, c);
        else
            lk_seg_dash(px, py, x, y, c, 2.0f, 2.0f);
        px = x, py = y;
    }
}

/* DRUM > SOUND: its fall over time (the amplitude to -60 dB; KICK's sweep dashed, CLAP's bursts), DECAY's time marked */
static void ui_p_kdrum(void)
{
    uint32_t l = ui_d_lane(), i, vr, kick = dk_model(l, &vr) == DM_PUNCH && vr == 0u;   /* (V0.87: PUNCH's KICK swept) */
    const dk_lane_t *k = &dkk.l[l];
    float t60 = dk_t60(l, k->decay), span = t60 * 1.25f, x0 = 26.0f, x1 = 226.0f, y0 = 78.0f, y1 = 140.0f;
    char t[40];
    ui_d_lanes();
    lk_seg(x0, y1 + 0.5f, x1, y1 + 0.5f, 1.0f, sk.line);
    ui_d_fall(l, x0, x1, y0, y1, span, t60, 0, ui_ink(ui_hot(1)), 0.0f);
    if (kick) {                                     /* (the sweep: from PUNCH's height down to the pitch) */
        float p0 = 1.0f + 6.0f * (float)k->chr * 0.01f, qx = x0, qy = 0.0f;
        for (i = 0; i <= 24u; i++) {
            float tt = span * (float)i / 24.0f, f = 1.0f + p0 * sy_exp2f(-9.9658f * tt / 0.2f), x = x0 + (x1 - x0) * (float)i / 24.0f;
            float y = y1 - (y1 - y0) * 0.9f * f / (1.0f + p0);
            if (i)
                lk_seg_dash(qx, qy, x, y, ui_hot(3) ? sk.hot : sk.mid, 2.0f, 2.0f);
            qx = x, qy = y;
        }
    }
    {                                               /* (DECAY's time: a tick on the axis, its words) */
        float xd = x0 + (x1 - x0) * t60 / span;
        lk_seg(xd, y1 - 3.0f, xd, y1 + 4.0f, 1.0f, ui_ink(ui_hot(1)));
        ui_d_time(sp_put(t, kick ? "PITCH DASHED \xB7 -60 DB AT " : "-60 DB AT "), t60);
        lk_text_r(x1, y1 + 6.0f, T9, t, sk.lab, sk.ground, 20, 0);
    }
    {                                               /* (SOUND's four; V0.87 MODEL's MODEL and TYPE) */
        const int16_t *pid = UI_PG[ui.page].id;
        const int32_t ids[4] = {pid[0], pid[1], pid[2], pid[3]};
        ui_d_knobs4(ids);
    }
}

/* DRUM > DRIVE (V0.86): what comes out for what goes in (-1 .. 1 each way; as it is dashed), the TYPE and the gain */
static void ui_p_ddrive(void)
{
    static const int32_t IDS[4] = {UI_KDTYPE, UI_KDRIVE, UI_KDMIX, -1};
    const dk_fx_t *f = &dkk.fx[ui_d_lane()];
    float cx = 126.0f, cy = 114.0f, w = 100.0f, h = 34.0f, px = 0.0f, py = 0.0f, d = (float)f->drive * 0.01f, m = (float)f->dmix * 0.01f;
    float g = f->dtype == DD_CRUSH ? sy_exp2f(15.0f - 13.0f * d) : sy_exp2f(4.0f * d), c = f->dtype == DD_CRUSH ? 1.0f / g : 1.0f / (1.0f + 1.5f * d);
    uint32_t i;
    ui_d_lanes();
    lk_seg(cx - w, cy + 0.5f, cx + w, cy + 0.5f, 1.0f, sk.line);
    lk_seg(cx + 0.5f, cy - h - 2.0f, cx + 0.5f, cy + h + 2.0f, 1.0f, sk.line);
    lk_seg_dash(cx - w, cy + h, cx + w, cy - h, sk.mid, 2.0f, 3.0f);
    for (i = 0; i <= 100u; i++) {
        float x = -1.0f + 0.02f * (float)i, u = x * 0.5f * g, y;   /* (in: a drum's level, half of full scale) */
        if (!f->drive)
            y = x * 0.5f;
        else if (f->dtype == DD_SOFT)
            y = sy_softclip(u) * c;
        else if (f->dtype == DD_HARD)
            y = sy_clampf(u, -1.0f, 1.0f) * c;
        else if (f->dtype == DD_FOLD) {
            float t = 0.25f * u + 0.25f;
            t -= (float)sy_floori(t);
            y = (1.0f - 4.0f * sy_absf(t - 0.5f)) * c;
        } else {
            y = (float)sy_floori(u + 0.5f) * c;
        }
        y = x * 0.5f + (y - x * 0.5f) * (f->drive ? m : 0.0f);
        {
            float sx = cx + w * x, sy = cy - h * 2.0f * y;
            if (i)
                lk_seg(px, py, sx, sy, 1.5f, ui_ink(ui_hot(0) || ui_hot(1) || ui_hot(2)));
            px = sx, py = sy;
        }
    }
    ui_d_knobs4(IDS);
}

/* DRUM > EQ (V0.86): the response from 20 Hz to 20 kHz (+-12 dB), each band's handle on it (FREQ's under MID) */
static float ui_d_eq_db(const dk_fx_t *f, float hz)
{
    float gl = sy_exp2f((float)((int32_t)f->lo - 50) * 0.24f * 0.16609640f), gh = sy_exp2f((float)((int32_t)f->hi - 50) * 0.24f * 0.16609640f);
    float gm = sy_exp2f((float)((int32_t)f->mid - 50) * 0.24f * 0.16609640f) - 1.0f;
    float fm = 200.0f * sy_exp2f(5.3219281f * (float)f->freq * 0.01f), ql = hz / 200.0f, qh = hz / 2000.0f, r = hz / fm;
    float lm = sy_sqrtf((ql * ql + gl * gl) / (ql * ql + 1.0f)), hm = sy_sqrtf((gh * gh * qh * qh + 1.0f) / (qh * qh + 1.0f));
    float re = 1.0f - r * r, im = r * 1.4f, den = re * re + im * im;   /* (MID: 1 + gm x its band-pass, Q 1 / 1.4) */
    float br = im * im / den, bi = im * re / den, mr = 1.0f + gm * br, mi = gm * bi;
    return 20.0f * 0.30103f * sy_log2f(sy_maxf(lm * hm * sy_sqrtf(mr * mr + mi * mi), 1e-4f));
}
static float ui_d_eq_x(float hz) { return 26.0f + 200.0f * sy_log2f(hz / 20.0f) / 9.9657843f; }
static void ui_p_deq(void)
{
    static const char *const FL[4] = {"20", "200", "2K", "20K"};
    const dk_fx_t *f = &dkk.fx[ui_d_lane()];
    float cy = 114.0f, sc = 34.0f / 12.0f, px = 0.0f, py = 0.0f, fm = 200.0f * sy_exp2f(5.3219281f * (float)f->freq * 0.01f);
    float hz3[3] = {70.0f, fm, 9000.0f};
    uint32_t i;
    char t[16];
    ui_d_lanes();
    for (i = 0; i < 4u; i++) {
        float x = 26.0f + 200.0f * (float)i / 3.0f;
        lk_seg_dash(x, cy - 36.0f, x, cy + 36.0f, sk.line, 1.0f, 3.0f);
        if (i == 0u)
            lk_text(x, 154.0f, T9, FL[i], sk.lab, sk.ground, 0, 0);
        else if (i == 3u)
            lk_text_r(x, 154.0f, T9, FL[i], sk.lab, sk.ground, 0, 0);
        else
            lk_text_c(x, 154.0f, T9, FL[i], sk.lab, sk.ground, 0, 0);
    }
    lk_seg_dash(26.0f, cy, 226.0f, cy, sk.mid, 2.0f, 3.0f);
    for (i = 0; i <= 64u; i++) {
        float hz = 20.0f * sy_exp2f(9.9657843f * (float)i / 64.0f), x = ui_d_eq_x(hz);
        float y = cy - sc * sy_clampf(ui_d_eq_db(f, hz), -13.0f, 13.0f);
        if (i)
            lk_seg(px, py, x, y, 1.8f, ui_ink(ui_turning() >= 0));
        px = x, py = y;
    }
    for (i = 0; i < 3u; i++) {                      /* (LOW, MID, HIGH: a handle on the curve over its band) */
        float x = ui_d_eq_x(hz3[i]), y = cy - sc * sy_clampf(ui_d_eq_db(f, hz3[i]), -13.0f, 13.0f);
        ui_H(i, x - 7.0f, y + (y < cy ? -14.0f : 4.0f));
    }
    {                                               /* (FREQ: arrows under the middle band, its handle beside) */
        float x = ui_d_eq_x(fm);
        lk_arrow(x - 4.0f, 168.0f, x - 22.0f, 168.0f, 3.0f, 1.0f, ui_hot(3) ? sk.hot : sk.mid);
        lk_arrow(x + 4.0f, 168.0f, x + 22.0f, 168.0f, 3.0f, 1.0f, ui_hot(3) ? sk.hot : sk.mid);
        ui_H(3, x + 26.0f, 161.0f);
    }
    {                                               /* (the four values in a row under it) */
        static const int32_t IDS[4] = {UI_KEQL, UI_KEQM, UI_KEQH, UI_KEQF};
        char v[24];
        for (i = 0; i < 4u; i++) {
            const char *nm = ui_drum_nv((uint32_t)IDS[i], v);
            float x = 26.0f + 52.0f * (float)i;
            lk_text(x, 182.0f, T9, nm, sk.lab, sk.ground, 20, 0);
            lk_text(x, 194.0f, T10B, lk_fit(t, sizeof t, v, T10B, 0, 50.0f), ui_ink(ui_hot(i)), sk.ground, 0, 0);
        }
    }
    pg.figs = 0xFu;
}

/* DRUM > SHAPE (V0.86): the drum's fall as it is (dashed) and shaped: ATTACK at its start, SUSTAIN on its body */
static void ui_p_dshape(void)
{
    static const int32_t IDS[4] = {UI_KSATT, UI_KSSUS, -1, -1};
    uint32_t l = ui_d_lane();
    float t60 = dk_t60(l, dkk.l[l].decay), span = sy_minf(t60 * 1.25f, 0.4f), y0 = 74.0f, y1 = 140.0f;
    char t[40];
    ui_d_lanes();
    lk_seg(26.0f, y1 + 0.5f, 226.0f, y1 + 0.5f, 1.0f, sk.line);
    lk_seg_dash(26.0f, y0 + 11.0f, 226.0f, y0 + 11.0f, sk.line, 1.0f, 3.0f);   /* (0 dB: +12 room over it) */
    ui_d_fall(l, 26.0f, 226.0f, y0, y1, span, t60, -1, sk.mid, 12.0f);
    ui_d_fall(l, 26.0f, 226.0f, y0, y1, span, t60, 1, ui_ink(ui_turning() >= 0), 12.0f);
    ui_d_time(sp_put(t, "THE FIRST "), span);
    lk_text_r(226.0f, y1 + 6.0f, T9, t, sk.lab, sk.ground, 20, 0);
    ui_d_knobs4(IDS);
}

/* DRUM > MIX: the eight as a mixer (LEVEL a fader, PAN a dot over it), the one picked framed */
static void ui_p_kmore(void)
{
    static const int32_t IDS[4] = {UI_KLVL, UI_KPAN, -1, -1};
    static const char *const AB[DK_N] = {"BD", "SD", "CP", "CH", "OH", "CY", "W1", "W2"};
    uint32_t l, cur = ui_d_lane();
    ui_d_lanes();
    for (l = 0; l < DK_N; l++) {
        const dk_lane_t *k = &dkk.l[l];
        float x = 30.0f + 25.0f * (float)l, cx = x + 12.5f, top = 82.0f, bot = 142.0f;
        float lv = bot - (bot - top) * (float)k->level / 127.0f, px = cx + 8.0f * ((float)k->pan - 64.0f) / 63.0f;
        int me = l == cur;
        lk_seg(cx - 8.0f, 76.0f, cx + 8.0f, 76.0f, 1.0f, sk.line);
        lk_disc(px, 76.0f, 2.0f, me && ui_hot(1) ? sk.hot : sk.ink);
        lk_seg(cx, top, cx, bot, 1.0f, sk.line);
        lk_box(cx - 3.0f, lv, cx + 3.0f, bot, me && ui_hot(0) ? sk.hot : me ? sk.ink : sk.mid);
        lk_text_c(cx, 146.0f, me ? T9B : T9, AB[l], me ? sk.ink : sk.lab, sk.ground, 0, 0);
        if (me)
            lk_frame(x + 0.5f, 70.5f, 24.0f, 88.0f, 1.0f, sk.mid);
    }
    ui_d_knobs4(IDS);
}

/* KIT > ACCENT: a hit's gain by its velocity, plain (full) and accented (HOT when turning ACCENT), VELOCITY's share;
 * V0.86 (K9): TONE, how much darker a soft hit is (a strip under it: the TONE moved, by velocity) */
static void ui_p_kacc(void)
{
    static const int32_t IDS[3] = {UI_KACC, UI_KVSN, UI_KATONE};
    float x0 = 30.0f, x1 = 226.0f, y0 = 46.0f, y1 = 132.0f, s = (float)dkk.vsens * 0.01f, a = 1.0f + (float)dkk.accent * 0.01f;
    float px = 0.0f, py = 0.0f, qy = 0.0f;
    uint32_t i;
    char t[40];
    ui_title("ACCENT", UI_TITLE_X);
    lk_seg(x0, y1 + 0.5f, x1, y1 + 0.5f, 1.0f, sk.line);
    lk_seg(x0 - 0.5f, y0, x0 - 0.5f, y1, 1.0f, sk.line);
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
    {                                               /* (TONE: soft hits darker, by velocity) */
        float yt = 150.0f, d = (float)dkk.atone * 0.6f * 0.12f;
        lk_seg_dash(x0, yt, x1, yt, sk.line, 2.0f, 3.0f);
        lk_seg(x0, yt + d, x1, yt, 1.5f, ui_ink(ui_hot(2)));
        sp_put(sp_put(t, "TONE BY VELOCITY \xB7 "), dkk.atone ? "SOFT HITS DARKER" : "NONE");
        lk_text(x0 + 4.0f, 158.0f, T9, t, sk.lab, sk.ground, 20, 0);
    }
    ui_d_knobs(0u, IDS);
}

/* KIT > DUCK, CURVE: the synth's level from a ghost hit (0): down by AMOUNT over ATTACK, held HOLD, back over RELEASE
 * along SHAPE */
static void ui_d_duck(float y0, float y1, int hot)
{
    float att = (float)dkk.attack * 0.001f, hold = (float)dkk.dhold * DK_HOLD_MS * 0.001f, rel = dk_rel_s();
    float span = (att + hold + rel) * 1.25f + 0.02f, x0 = 26.0f, x1 = 226.0f, am = (float)dkk.amount * 0.01f, px = x0, py = y0;
    uint32_t i;
    char t[40];
    lk_seg(x0, y1 + 0.5f, x1, y1 + 0.5f, 1.0f, sk.line);
    lk_seg_dash(x0, y0, x1, y0, sk.line, 1.0f, 3.0f);
    for (i = 1; i <= 64u; i++) {
        float tt = span * (float)i / 64.0f - 0.01f, d, x = x0 + (x1 - x0) * (float)i / 64.0f, y;
        if (tt < 0.0f)
            d = 0.0f;
        else if (tt < att)
            d = tt / att;
        else if (tt < att + hold)
            d = 1.0f;
        else {
            float r = sy_maxf(0.0f, 1.0f - (tt - att - hold) / rel);
            d = dkk.shape == DS_EXP ? r * r * r : dkk.shape == DS_PUMP ? 1.0f - (1.0f - r) * (1.0f - r) * (1.0f - r) : r;
        }
        y = y0 + (y1 - y0) * am * d;
        lk_seg(px, py, x, y, 1.5f, ui_ink(hot));
        px = x, py = y;
    }
    {
        float xa = x0 + (x1 - x0) * 0.01f / span;
        lk_tri(xa - 3.5f, y1 + 9.0f, xa + 3.5f, y1 + 9.0f, xa, y1 + 4.0f, sk.mid);
    }
    ui_d_time(sp_put(t, "BACK IN "), att + hold + rel);
    lk_text_r(x1, y1 + 6.0f, T9, t, sk.lab, sk.ground, 20, 0);
}
static void ui_p_kduck(void)
{
    static const int32_t IDS[4] = {UI_DSRC, UI_DAMT, UI_DATT, UI_DREL};
    static const char *const SRC[DSRC_N] = {"FROM ITS ROW", "FROM THE KICK", "ROW AND KICK"};
    const char *c = SRC[dkk.dsrc % DSRC_N];
    float cx = 230.0f - lk_tw(T10B, c, 20) - 10.0f;
    ui_title("DUCK", UI_TITLE_X);
    ui_chip(cx, 24.0f, c, 1, ui_hot(0));
    lk_text(UI_TITLE_X, 46.0f, T9, "THE SYNTH'S LEVEL FROM A HIT AT 0", sk.lab, sk.ground, 20, 0);
    ui_d_duck(66.0f, 136.0f, ui_hot(1) || ui_hot(2) || ui_hot(3));
    ui_d_knobs4(IDS);
}
static void ui_p_kcurve(void)
{
    static const int32_t IDS[4] = {UI_DSHP, UI_DHOLD, -1, -1};
    lk_text(UI_TITLE_X, 24.0f, T14B, DS_NAME[dkk.shape % DS_N], ui_ink(ui_hot(0)), sk.ground, 0, 17);
    lk_text(UI_TITLE_X, 46.0f, T9, dkk.dhold ? "HELD DOWN, THEN BACK" : "BACK AT ONCE", sk.lab, sk.ground, 20, 0);
    ui_d_duck(66.0f, 136.0f, ui_turning() >= 0);
    ui_d_knobs4(IDS);
}

/* KIT > COMP (V0.86): what comes out for what goes in, in dB (-48 .. 0 each way; as it is dashed): 4:1 above
 * AMOUNT's threshold, its make-up, MIX in parallel; the threshold marked */
static void ui_p_kcomp(void)
{
    static const int32_t IDS[4] = {UI_KCAMT, UI_KCATT, UI_KCREL, UI_KCMIX};
    float amt = (float)dkk.comp[0] * 0.01f, thr = -30.0f * amt, mk = amt > 0.2f ? 0.375f * (30.0f * amt - 6.0f) : 0.0f;
    float m = (float)dkk.comp[3] * 0.01f, x0 = 26.0f, x1 = 226.0f, y0 = 58.0f, y1 = 146.0f, px = 0.0f, py = 0.0f;
    uint32_t i;
    char t[48], *e;
    ui_title("COMP", UI_TITLE_X);
    e = sp_put(t, "4:1 OVER ");
    e = sp_put(sp_fix(e, (int32_t)(thr - 0.5f), 0u, 0), " DB");
    if (mk > 0.05f)
        sp_put(sp_put(sp_fix(sp_put(e, " \xB7 +"), (int32_t)(mk * 10.0f + 0.5f), 1u, 0), " DB"), "");
    lk_text(UI_TITLE_X, 42.0f, T9, dkk.comp[0] ? t : "OFF: THE DRUMS AS THEY ARE", sk.lab, sk.ground, 20, 0);
    lk_seg(x0, y1 + 0.5f, x1, y1 + 0.5f, 1.0f, sk.line);
    lk_seg_dash(x0, y1, x1, y0, sk.mid, 2.0f, 3.0f);
    if (dkk.comp[0]) {
        float xt = x0 + (x1 - x0) * (thr + 48.0f) / 48.0f;
        lk_seg_dash(xt, y0, xt, y1, sk.line, 1.0f, 3.0f);
        lk_tri(xt - 3.5f, y1 + 9.0f, xt + 3.5f, y1 + 9.0f, xt, y1 + 4.0f, ui_hot(0) ? sk.hot : sk.mid);
    }
    for (i = 0; i <= 48u; i++) {
        float in = -48.0f + (float)i, out = (in > thr ? thr + (in - thr) * 0.25f : in) + (dkk.comp[0] ? mk : 0.0f), y, x;
        float a = sy_exp2f(in * 0.16609640f), b = sy_exp2f(out * 0.16609640f);
        out = 20.0f * 0.30103f * sy_log2f(sy_maxf(a + (b - a) * (dkk.comp[0] ? m : 0.0f), 1e-6f));
        x = x0 + (x1 - x0) * (float)i / 48.0f;
        y = y1 - (y1 - y0) * sy_clampf((out + 48.0f) / 48.0f, 0.0f, 1.0f);
        if (i)
            lk_seg(px, py, x, y, 1.8f, ui_ink(ui_hot(0) || ui_hot(3)));
        px = x, py = y;
    }
    lk_text(x0, y1 + 10.0f, T9, "-48", sk.lab, sk.ground, 0, 0);
    lk_text_r(x1, y1 + 10.0f, T9, "0 DB IN", sk.lab, sk.ground, 20, 0);
    ui_d_knobs4(IDS);
}

/* GRID, HIT: the clip's sixteen steps shown, a row a drum and DUCK; a hit a box (its velocity its brightness; ACCENT
 * the cell's full height; CHANCE under 100 % hollow; RATCHET its hits as slivers), DUCK's as frames; the step picked
 * framed, the playhead over its step; the beats ticked under */
#define UI_GX0 48.0f
#define UI_GDX 11.0f
#define UI_GY0 60.0f
#define UI_GDY 11.0f
/* (V0.87.1) the rows' names at the left of the grid: their place's, short (a row's preset name, long, is KNOB 2's) so they
 * stop short of the first step at x 48 */
static const char *const UI_GLAB[DK_ROWS] = {"KICK", "SNARE", "CLAP", "HAT C", "HAT O", "CYM", "WT 1", "WT 2", "DUCK"};
static void ui_d_grid(int hit)
{
    const lp_clip_t *c = lp_cur_clip();
    uint32_t t = lpp.sel % LP_NTRK, p0 = ui_d_page0(), n = ui_d_steps(), r, s, k, cur = ui.gstep % n;
    uint32_t bt = lp_bar_ticks() / LP_STEP, qt = (uint32_t)(sy_beat_q * 4.0f + 0.5f);
    char tx[48], *e;
    int hot = ui.ghold ? ui_turning() >= 0 : ui_hot(0) || ui_hot(1);
    if (ui.ghold && cur >= p0 && cur < p0 + 16u)   /* (S7: the step held, its column lit) */
        lk_box(UI_GX0 + UI_GDX * (float)(cur - p0), UI_GY0 - 2.0f, UI_GX0 + UI_GDX * (float)(cur - p0 + 1u) - 1.0f,
               UI_GY0 + UI_GDY * (float)DK_ROWS, sk.line);
    e = sp_put(sp_fix(sp_put(sp_put(sp_put(sp_fix(sp_put(tx, "T"), (int32_t)t + 1, 0u, 0), " \xB7 "),
                                    (char[2]){(char)('A' + lpp.scn % LP_NSCN), 0}), " \xB7 STEPS "),
                             (int32_t)p0 + 1, 0u, 0), "-");
    sp_fix(sp_put(sp_fix(e, (int32_t)(p0 + 16u < n ? p0 + 16u : n), 0u, 0), " OF "), (int32_t)n, 0u, 0);
    if (!c->len)
        sp_put(sp_put(tx, ""), "A WHITE KEY STARTS A BAR");
    if (ui.ghold)                                   /* (held: the track, the clip, the row) */
        sp_put(sp_put(sp_put(sp_put(sp_fix(sp_put(tx, "T"), (int32_t)t + 1, 0u, 0), " \xB7 "),
                             (char[2]){(char)('A' + lpp.scn % LP_NSCN), 0}), " \xB7 "), dk_rname(ui_d_row()));
    lk_text(UI_TITLE_X, 40.0f, T10, tx, lp_cur_rec() ? sk.hot : sk.lab, sk.ground, 20, 12);
    for (r = 0; r < DK_ROWS; r++) {                 /* (the rows' names, the one picked bold) */
        float y = UI_GY0 + UI_GDY * (float)r;
        lk_text(8.0f, y + 1.0f, r == ui_d_row() ? T9B : T9, UI_GLAB[r], r == ui_d_row() ? ui_ink(ui_hot(1)) : sk.lab,
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
        h = ac ? 9.0f : 6.0f;
        col = row == DK_DUCK ? sk.mid : sy_mix565(sk.mid, sk.ink, (uint32_t)(lp_ev[k].b & 0x7F) * 2u);
        if (st == cur && row == ui_d_row() && (ui.ghold ? hot : ui_hot(2) || ui_hot(3)))
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
    if (ui.ghold) {                                 /* (S7: held, the step's words; the knobs under) */
        sp_put(sp_fix(sp_put(tx, "STEP "), (int32_t)cur + 1, 0u, 0), " HELD");
        lk_text(UI_TITLE_X, 22.0f, T14B, tx, sk.ink, sk.ground, 0, 17);
    } else if (UI_PG[ui.page].id[0] == UI_EHITS) {  /* (V0.87: EUCLID, KNOB 1 its HITS in the title) */
        sp_put(sp_fix(sp_put(sp_fix(tx, ui.e_hits, 0u, 0), " HITS IN "), ui.e_steps, 0u, 0), " STEPS");
        lk_text(UI_TITLE_X, 22.0f, T14B, tx, ui_ink(ui_hot(0)), sk.ground, 0, 17);
        ui_H(0, 8.0f, 25.0f);
    } else {                                        /* (KNOB 1: the step, in the title) */
        char v[24];
        ui_drum_nv(UI_GSTEP, v);
        sp_put(sp_put(tx, "STEP "), v);
        lk_text(UI_TITLE_X, 22.0f, T14B, tx, ui_ink(ui_hot(0)), sk.ground, 0, 17);
        ui_H(0, 8.0f, 25.0f);
    }
}
static const int32_t UI_D_HELD[4] = {UI_GVEL, UI_GACC, UI_GCHN, UI_GRAT};   /* (S7: a step held) */
static void ui_p_lgrid(void)
{
    static const int32_t IDS[3] = {UI_GROW, UI_GVEL, UI_GACC}, EIDS[3] = {UI_ESTEPS, UI_EROT, UI_GROW};
    ui_d_grid(0);
    if (ui.ghold)
        ui_d_knobs4(UI_D_HELD);
    else
        ui_d_knobs(1u, UI_PG[ui.page].id[0] == UI_EHITS ? EIDS : IDS);   /* (V0.87: EUCLID's) */
}
static void ui_p_lhit(void)
{
    static const int32_t IDS[3] = {UI_GROW, UI_GCHN, UI_GRAT};
    ui_d_grid(1);
    if (ui.ghold)
        ui_d_knobs4(UI_D_HELD);
    else
        ui_d_knobs(1u, IDS);
}
