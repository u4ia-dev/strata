/* SPDX-License-Identifier: GPL-3.0-only */
/* The STRATA UI's looper (V0.8; the looper itself: sy_loop.c; docs/LOOPER.md), the SEQ button's menu. SEQ once:
 * LOOP and CLIP; twice: TRACK and SONG (V0.86, S1). PRESETS picks the track on every page (C1):
 *   - LOOP: CLIPS (the eight tracks' clips in the eight scenes: the one playing filled with its place, launched
 *     blinking, recording HOT; the track selected's column lit, the scene picked framed; TYPE, SCENE; the keys launch:
 *     white 1-8 this scene's clips, black 1-8 the scenes), MIXER (VOLUME, PAN, MUTE / SOLO);
 *   - TRACK: RECORD (QUANTIZE; the track's CHANNEL out, MIDI IN), CLICK (OFF, REC, ON; LEVEL; COUNT-IN, LAUNCH);
 *   - CLIP (the selected track's clip in the scene picked): NOTES (a piano roll a bar at a time round the note
 *     picked: its PITCH, START, LENGTH), NOTE+ (VELOCITY, GRID, DELETE), CLIP (LENGTH in bars, SPEED, QUANTIZE, its
 *     MOTION: a parameter's moves drawn), TOOLS (COPY, PASTE, DOUBLE / HALVE, CLEAR: V0.86, S8);
 *   - SONG: SONG (sixteen slots, a scene and its repeats each; ON), PROJECT (four, LOAD, NEW);
 *   - the launch pad (SEQ held): white keys 1-8 the tracks' clips in the scene picked (launch, again stop; an empty
 *     one: the track selected), 9-16 the scenes; black keys 1-8 MUTE, then STOP ALL, SOLO, UNDO PASS.
 * The track selected (lpp.sel) is the one the keys play and SCL, ARP and the chords edit (sy_chord.c ch_use). Needs
 * sy_ui.c, sy_ui_draw.c (pg, ui_H, ui_hot, ui_ink, ui_title, UI_TITLE_X), sy_ui_chord.c (ui_chip), sy_gfx.c. */

/* ------------------------------------------------------------------------------------------ helpers --- */
#define LG_N 5u                                     /* NOTES' GRID: a move's step */
static const char *const LG_NAME[LG_N] = {"FREE", "1/32", "1/16", "1/8", "1/4"};
static const uint8_t LG_TICKS[LG_N] = {1, 12, 24, 48, 96};
static const char *const LT_TYPE3[4] = {"SYNTH", "SYNTH+MIDI", "MIDI", "DRUMS"};   /* TYPE as the knob steps it */
static const char *const LT_TYPE3S[4] = {"SYNTH", "SYN+MIDI", "MIDI", "DRUMS"};     /* .. on the knob (V0.86) */
static uint32_t ui_l_type3(uint32_t t)              /* a track's TYPE: 0 SYNTH, 1 SYNTH with MIDI out, 2 MIDI, 3 DRUMS */
{
    const lp_trk_t *k = &lpp.trk[t % LP_NTRK];
    return k->type == LT_DRUM ? 3u : k->type == LT_MIDI ? 2u : k->out ? 1u : 0u;
}
static void sy_ui_drum_ms(void);                    /* sy_ui_drum.c (V0.85) */
#define UI_ASK_MS 2000u                             /* V0.86: a tool's second turn within this does it */
#define UI_LT_MS 500u                               /* .. (C6) TYPE goes this long after its last detent */
static void ui_l_asks_off(void)                     /* .. the tools' and TYPE's asks dropped (another clip, track) */
{
    ui.paste_t = ui.halve_t = ui.clr_t2 = 0;
    ui.lt_want = 0;
    ui.lt_ask = 0;
    if (ui.qk == Q_TYPE || ui.qk == Q_CLIPDEL)
        ui.qk = Q_NONE, ui.dirty = D_ALL;
}
/* V0.85: the page shown as the selected track's TYPE wants it: a drum track's EDIT is its KIT, its CLIP the grid; a
 * synth's, the sound's and NOTES (the page of the same drawing where there is one) */
static void ui_l_follow(void)
{
    uint32_t s = ui_sec(), to = 0xFFu, p;
    int drum = ui_drums();
    if (drum && s == SEC_LCLIP)
        to = SEC_LGRID;
    else if (!drum && s == SEC_LGRID)
        to = SEC_LCLIP;
    else if (drum && ui.fam == FAM_EDIT && s != SEC_KIT && s != SEC_DRUM)
        to = SEC_DRUM;                              /* (V0.86: DRUM, then KIT) */
    else if (!drum && (s == SEC_KIT || s == SEC_DRUM))
        to = SEC_OSCA;
    if (to == 0xFFu)
        return;
    for (p = 0; p < UI_NPG; p++)
        if (UI_PG[p].sec == to && UI_PG[p].graph == UI_PG[ui.page].graph)
            break;
    ui_go(p < UI_NPG ? p : ui_first_page(to));
}
static char ui_l_scn(uint32_t s) { return (char)('A' + s % LP_NSCN); }
static char *ui_l_trk(char *b, uint32_t t)          /* "T3 SYNTH" */
{
    return sp_put(sp_put(sp_fix(sp_put(b, "T"), (int32_t)(t % LP_NTRK) + 1, 0u, 0), " "), LT_TYPE3[ui_l_type3(t)]);
}
static char *ui_l_note(char *b, uint32_t n)         /* a note's name: "C4" (60) */
{
    return sp_fix(sp_put(b, UI_NOTE_NAME[n % 12u]), (int32_t)(n / 12u) - 1, 0u, 0);
}
static char *ui_l_pos(char *b, uint32_t tk)         /* a tick as bar.beat.16th ("2.3.1"; off the sixteenths: "+") */
{
    uint32_t bt = lp_bar_ticks(), qt = (uint32_t)(sy_beat_q * (float)LP_PPQ + 0.5f), x;
    char *e;
    qt = qt ? qt : LP_PPQ;
    x = tk % bt;
    e = sp_fix(sp_put(sp_fix(sp_put(sp_fix(b, (int32_t)(tk / bt) + 1, 0u, 0), "."), (int32_t)(x / qt) + 1, 0u, 0), "."),
               (int32_t)(x % qt / 24u) + 1, 0u, 0);
    return x % qt % 24u ? sp_put(e, "+") : e;
}
static char *ui_l_len(char *b, uint32_t len)        /* a length: sixteenths ("3/16"; "1/32"), else ticks */
{
    if (len == 12u)
        return sp_put(b, "1/32");
    if (len % 24u == 0u)
        return sp_put(sp_fix(b, (int32_t)(len / 24u), 0u, 0), "/16");
    return sp_put(sp_fix(b, (int32_t)len, 0u, 0), " T");   /* (ticks: 96 a quarter) */
}
static uint32_t ui_l_note_cur(void)                 /* the note picked, inside the clip's count */
{
    uint32_t n = lp_note_n(lp_cur_clip());
    return n ? (ui.lnote < n ? ui.lnote : n - 1u) : 0u;
}
static int ui_l_playing(uint32_t t)                 /* track t's clip playing (the transport running) */
{
    return cht.run && lps[t % LP_NTRK].play < LP_NSCN;
}
static uint32_t ui_l_moves(uint16_t *ids)           /* the selected clip's moved parameters (8 at most) */
{
    return lp_moves_of(lp_cur_clip(), ids, 8u);
}

/* ------------------------------------------------------------------------------------------ turning --- */
/* the selected track: its SCALE the keys', its chords the menus' (keep: the one before keeps the UI's SCALE; else,
 * as a project is read, the tracks' own) */
static void lp_select_to(uint32_t t, int keep)
{
    lp_trk_t *o = &lpp.trk[lpp.sel % LP_NTRK], *k;
    t %= LP_NTRK;
    if (keep)
        o->scale = ui.scale, o->root = ui.root, o->fit = ui.fit, o->kvel = ui.kvel;
    lpp.sel = (uint8_t)t;
    k = &lpp.trk[t];
    ui.scale = (uint8_t)(k->scale % UI_NSCALE), ui.root = (uint8_t)(k->root % 12u);
    ui.fit = (uint8_t)(k->fit % UI_NFIT), ui.kvel = (uint8_t)sy_clampi(k->kvel, 1, 127);
    ch_use(t);
    ui.lnote = 0;
    ui_l_asks_off();
    lpx.seq++;
    ui.dirty = D_ALL;
    ui_l_follow();                                  /* (V0.85: a drum track's pages, or a synth's) */
}
static void lp_select(uint32_t t) { lp_select_to(t, 1); }
static void lp_scale_keep(void)                     /* the selected track's SCALE as the UI has it (before a save) */
{
    lp_trk_t *k = &lpp.trk[lpp.sel % LP_NTRK];
    k->scale = ui.scale, k->root = ui.root, k->fit = ui.fit, k->kvel = ui.kvel;
}
/* a clip launched from the UI (CLIPS' LAUNCH, the pad): the one playing again: stopped; stopped, the transport
 * starts with it */
static void ui_l_launch(uint32_t t, uint32_t s)
{
    char m[32], *e;
    t %= LP_NTRK, s %= LP_NSCN;
    e = sp_put(sp_put(sp_fix(sp_put(m, "T"), (int32_t)t + 1, 0u, 0), " "), "");
    if (!lpp.clip[t][s].len) {
        sp_put(sp_put(sp_put(e, "SCENE "), (char[2]){ui_l_scn(s), 0}), ": EMPTY");
    } else if (lps[t].play == s && lps[t].next == LP_NOSCN && cht.run) {
        lp_launch(t, LP_STOP);
        sp_put(e, lpg.launch == LL_NOW ? "STOPS" : "STOPS AT THE BAR");
    } else {
        lp_launch(t, s);
        sp_put(sp_put(sp_put(e, "PLAYS "), (char[2]){ui_l_scn(s), 0}), cht.run && lpg.launch != LL_NOW ? " AT THE BAR" : "");
        if (!cht.run && !ui.run) {
            ui.run = 1;
            ui.run_t = fm1_ms;
            sy_lfo_sync = 1;
            sy_app_run(1);
        }
    }
    ui_toast(m);
}
static void ui_l_scene(uint32_t s)                  /* a scene launched (the pad, CLIPS' SCENE) */
{
    uint32_t t, any = 0;
    char m[24];
    s %= LP_NSCN;
    for (t = 0; t < LP_NTRK; t++)
        any |= lpp.clip[t][s].len != 0u;
    lpp.scn = (uint8_t)s;
    if (!any) {
        sp_put(sp_put(sp_put(m, "SCENE "), (char[2]){ui_l_scn(s), 0}), ": EMPTY");
        ui_toast(m);
        ui.dirty = D_ALL;
        return;
    }
    lp_launch_scene(s);
    sp_put(sp_put(sp_put(m, "SCENE "), (char[2]){ui_l_scn(s), 0}), cht.run && lpg.launch != LL_NOW ? " AT THE BAR" : "");
    if (!cht.run && !ui.run) {
        ui.run = 1;
        ui.run_t = fm1_ms;
        sy_lfo_sync = 1;
        sy_app_run(1);
    }
    ui_toast(m);
    ui.dirty = D_ALL;
}
/* V0.86: clip (t, s) recording: no tool on it (a word why) */
static int ui_l_busy(uint32_t t, uint32_t s)
{
    if (lpr.st != LR_OFF && lpr.trk == t && lpr.scn == s) {
        ui_toast("RECORDING: REC ENDS IT");
        return 1;
    }
    return 0;
}
static char *ui_l_clip(char *b, uint32_t t, uint32_t s)   /* "T2 B" */
{
    return sp_put(sp_put(sp_fix(sp_put(b, "T"), (int32_t)(t % LP_NTRK) + 1, 0u, 0), " "), (char[2]){ui_l_scn(s), 0});
}
static int ui_l_asked(uint32_t *t)                  /* a tool's second turn in time (it goes); else its first (asked) */
{
    if (*t && fm1_ms - *t < UI_ASK_MS) {
        *t = 0;
        return 1;
    }
    *t = fm1_ms | 1u;
    return 0;
}
/* TOOLS' COPY: the clip (a first turn, or another clip's) or a bar of it (turning on) kept to paste */
static void ui_l_copy(uint32_t t, int32_t s)
{
    uint32_t sc = lpp.scn % LP_NSCN, bars = lp_clip_bars(t, sc);
    int32_t pos = 0;
    char m[40], *e;
    if (!lpp.clip[t][sc].len) {
        ui_toast("EMPTY: NOTHING TO COPY");
        return;
    }
    if (ui.cb_on && ui.cb_t == t && ui.cb_s == sc)
        pos = sy_clampi(ui.cb_bar + 1 + s, 0, (int32_t)bars);
    ui.cb_on = 1, ui.cb_t = (uint8_t)t, ui.cb_s = (uint8_t)sc, ui.cb_bar = (int8_t)(pos - 1);
    ui.paste_t = 0;
    e = sp_put(m, "COPIED: ");
    if (pos)
        e = sp_put(sp_fix(sp_put(e, "BAR "), pos, 0u, 0), " OF ");
    ui_l_clip(e, t, sc);
    ui_toast(m);
}
/* .. PASTE: what was copied into this clip (whole: it replaced; a bar: the same bar here); asked first if something
 * goes */
static void ui_l_paste(uint32_t t)
{
    uint32_t sc = lpp.scn % LP_NSCN, bt = lp_bar_ticks(), k, have = 0;
    const lp_clip_t *d = &lpp.clip[t][sc];
    int32_t bar = ui.cb_bar, r;
    char m[40], *e;
    if (!ui.cb_on) {
        ui_toast("COPY FIRST: KNOB 1");
        return;
    }
    if (ui.cb_t == t && ui.cb_s == sc) {
        ui_toast("THIS IS THE CLIP COPIED");
        return;
    }
    if (!lpp.clip[ui.cb_t % LP_NTRK][ui.cb_s % LP_NSCN].len) {
        ui_toast("THE CLIP COPIED IS EMPTY NOW");
        return;
    }
    if (ui_l_busy(t, sc))
        return;
    if (bar >= 0 && (uint32_t)bar * bt >= d->len) {
        sp_fix(sp_put(ui_l_clip(m, t, sc), " HAS NO BAR "), bar + 1, 0u, 0);
        ui_toast(m);
        return;
    }
    for (k = d->head; k != LP_END; k = lp_ev[k].next)
        have += bar < 0 || (lp_et(k) >= (uint32_t)bar * bt && lp_et(k) < (uint32_t)(bar + 1) * bt);
    if ((have || (bar < 0 && d->len)) && !ui_l_asked(&ui.paste_t)) {
        e = sp_put(m, "AGAIN: ");
        if (bar >= 0)
            e = sp_put(sp_fix(sp_put(e, "BAR "), bar + 1, 0u, 0), " OF ");
        sp_put(ui_l_clip(e, t, sc), " GOES");
        ui_toast(m);
        return;
    }
    ui.paste_t = 0;
    r = lp_clip_copy(ui.cb_t, ui.cb_s, t, sc, bar);
    if (r == -1) {
        ui_toast("FULL: 2048 NOTES AND MOVES");
        return;
    }
    e = sp_put(m, "PASTED INTO ");
    if (bar >= 0)
        e = sp_put(sp_fix(sp_put(e, "BAR "), bar + 1, 0u, 0), " OF ");
    ui_l_clip(e, t, sc);
    ui_toast(m);
}
/* .. DOUBLE (right: twice as long, played again in its second half) / HALVE (left: its second half gone, asked first
 * if notes go with it) */
static void ui_l_dbl(uint32_t t, int32_t s)
{
    uint32_t sc = lpp.scn % LP_NSCN;
    int32_t r;
    char m[40];
    if (!lpp.clip[t][sc].len) {
        ui_toast("EMPTY: REC RECORDS IT");
        return;
    }
    if (ui_l_busy(t, sc))
        return;
    if (s > 0) {
        ui.halve_t = 0;
        r = lp_clip_double(t, sc);
        if (r == -1) {
            ui_toast("FULL: 2048 NOTES AND MOVES");
            return;
        }
        if (r == -2) {
            sp_put(sp_fix(sp_put(m, "LONGEST: "), (int32_t)lp_max_bars(), 0u, 0), " BARS");
            ui_toast(m);
            return;
        }
    } else {
        r = lp_clip_halve(t, sc, 0);
        if (r == -2) {
            ui_toast("ONE BAR: NO HALF OF IT");
            return;
        }
        if (r > 0 && !ui_l_asked(&ui.halve_t)) {
            ui_toast("AGAIN: ITS SECOND HALF GOES");
            return;
        }
        ui.halve_t = 0;
        lp_clip_halve(t, sc, 1);
    }
    r = (int32_t)lp_clip_bars(t, sc);
    sp_put(sp_fix(sp_put(m, s > 0 ? "DOUBLED: " : "HALVED: "), r, 0u, 0), r == 1 ? " BAR" : " BARS");
    ui_toast(m);
}
/* .. CLEAR (right: its notes, a drum track's hits; left: the motion MOTION picked on CLIP), asked first */
static void ui_l_clear(uint32_t t, int32_t s)
{
    uint32_t sc = lpp.scn % LP_NSCN, n, dir = s > 0 ? 1u : 2u;
    uint16_t ids[8];
    int drum = ui_drums();
    char m[48];
    if (ui_l_busy(t, sc))
        return;
    if (ui.clr_dir != dir)
        ui.clr_t2 = 0;
    ui.clr_dir = (uint8_t)dir;
    if (dir == 1u) {
        n = lp_note_n(&lpp.clip[t][sc]);
        if (!n) {
            ui_toast(drum ? "NO HITS IN THIS CLIP" : "NO NOTES IN THIS CLIP");
            return;
        }
        if (!ui_l_asked(&ui.clr_t2)) {
            ui_toast(drum ? "AGAIN: ITS HITS GO" : "AGAIN: ITS NOTES GO");
            return;
        }
        lp_notes_clear(t, sc);
        ui.lnote = 0;
        ui_toast(drum ? "HITS CLEARED" : "NOTES CLEARED");
        return;
    }
    n = ui_l_moves(ids);
    if (!n) {
        ui_toast("NO MOTION IN THIS CLIP");
        return;
    }
    if (!ui_l_asked(&ui.clr_t2)) {
        sp_put(sp_put(sp_put(m, "AGAIN: "), SP[ids[ui.lmv % n]].name), ": MOTION GOES");
        ui_toast(m);
        return;
    }
    lp_moves_clear(ids[ui.lmv % n]);
    sp_put(sp_put(m, SP[ids[ui.lmv % n]].name), ": MOTION CLEARED");
    ui_toast(m);
    ui.lmv = 0;
}
/* (C6) TYPE as turned to: set (the pages follow it). V0.87 (S1): on a track with clips SAVE answers the question */
static void ui_l_type_go(void)
{
    uint32_t t = lpp.sel % LP_NTRK, v = (uint32_t)(ui.lt_want - 1);
    char m[32];
    ui.lt_want = 0;
    ui.lt_ask = 0;
    if (ui.qk == Q_TYPE)
        ui.qk = Q_NONE;
    if (v > 3u || v == ui_l_type3(t))
        return;
    lp_type_set(t, v == 3u ? LT_DRUM : v == 2u ? LT_MIDI : LT_SYNTH, v == 1u);
    ui_l_follow();
    ui_l_trk(m, t);
    ui_toast(m);
    ui.dirty = D_ALL;
}
static void ui_l_type_keep(void)                    /* .. the question answered no (anything but SAVE) */
{
    ui.lt_want = 0;
    ui.lt_ask = 0;
    if (ui.qk == Q_TYPE)
        ui.qk = Q_NONE;
    ui_toast("TYPE KEPT");
    ui.dirty = D_ALL;
}

/* V0.87 (U1, S3, S4): SEQ + OCT+ an empty clip of 1 bar on the selected track in the scene picked (SEQ held on,
 * PRESETS its length); SEQ + OCT- the clip there deleted, asked first (a second SEQ + OCT-) */
static void ui_l_clip_new(void)
{
    uint32_t t = lpp.sel % LP_NTRK, s = lpp.scn % LP_NSCN;
    char m[32];
    ui_l_asks_off();
    if (lpp.clip[t][s].len || (lpr.st != LR_OFF && lpr.trk == t && lpr.scn == s)) {
        sp_put(ui_l_clip(m, t, s), " HAS A CLIP");
        ui_toast(m);
        return;
    }
    lp_clip_bars_set(1u);
    ui.new_clip = 1;
    sp_put(ui_l_clip(m, t, s), ": NEW, 1 BAR");
    ui_toast(m);
    ui.dirty = D_ALL;
}
static void ui_l_new_len(int32_t s)                 /* .. its length (PRESETS, SEQ still held) */
{
    uint32_t t = lpp.sel % LP_NTRK, b = (uint32_t)sy_clampi((int32_t)lp_clip_bars(t, lpp.scn) + s, 1, (int32_t)lp_max_bars());
    char m[32];
    lp_clip_bars_set(b);
    sp_put(sp_fix(sp_put(ui_l_clip(m, t, lpp.scn), ": "), (int32_t)b, 0u, 0), b == 1u ? " BAR" : " BARS");
    ui_toast(m);
    ui.dirty = D_ALL;
}
static void ui_l_clip_del(void)
{
    uint32_t t = lpp.sel % LP_NTRK, s = lpp.scn % LP_NSCN, v = t * LP_NSCN + s + 1u;
    char m[32];
    if (!lpp.clip[t][s].len) {
        sp_put(ui_l_clip(m, t, s), " IS EMPTY");
        ui_toast(m);
        return;
    }
    if (ui.qk != Q_CLIPDEL || ui.qv != v) {         /* (asked: DELETE?) */
        ui.qk = Q_CLIPDEL;
        ui.qv = (uint8_t)v;
        ui.dirty = D_ALL;
        return;
    }
    ui.qk = Q_NONE;
    if (lpr.st != LR_OFF && lpr.trk == t && lpr.scn == s) {
        ui_toast("RECORDING IT: STOP FIRST");
        return;
    }
    lp_clip_clear(t, s);
    ui.lnote = ui.lmv = 0;
    sp_put(ui_l_clip(m, t, s), " DELETED");
    ui_toast(m);
    ui.dirty = D_ALL;
}
/* .. SEQ + OCT- held 5 s (U2): the page's settings as they start (its clips stay): MIXER the track's VOLUME, PAN, MUTE
 * and SOLO; SONG its slots and SONG off; RECORD QUANTIZE, CHANNEL and MIDI IN; CLICK its four; CLIP's pages the clip's
 * SPEED and NOTE+'s GRID; CLIPS nothing */
static void ui_l_reset(void)
{
    uint32_t g = UI_PG[ui.page].graph, t = lpp.sel % LP_NTRK;
    lp_trk_t *k = &lpp.trk[t];
    char m[32];
    if (g == G_LMIX) {
        k->vol = 100, k->pan = 64, k->mute = 0, k->solo = 0;
    } else if (g == G_LSONG) {
        lp_song_defaults();
        ui.lslot = 0;
    } else if (g == G_LREC) {
        lpg.quant = LQ_OFF;
        k->ch = (uint8_t)t, k->cin = 0;
    } else if (g == G_LCLICK) {
        lpg.cin = 1, lpg.launch = LL_BAR, lpg.click = LC_REC, lpg.clvl = 6;
    } else if (g == G_LCLIPS) {
        ui_toast("NOTHING TO RESET HERE");
        return;
    } else {
        lpp.speed[t][lpp.scn % LP_NSCN] = LS_1;
        ui.lgrid = 2;
        lps[t].resync = 1;
    }
    lpx.seq++;
    sp_put(sp_put(m, UI_PG[ui.page].name), ": RESET");
    ui_toast(m);
    ui.dirty = D_ALL;
}

/* the question's window (V0.87, sy_ui.c Q_*): DELETE? a clip; T3: DRUMS? its TYPE (S1); WIPE? the settings (T1) */
static void ui_p_q(void)
{
    char t[32], n[32];
    const char *l1 = "SAVE+OCT- AGAIN: DELETE IT", *l2 = "ANYTHING ELSE: KEEP IT", *l3 = 0, *l4 = 0;
    uint32_t tr = lpp.sel % LP_NTRK;
    if (ui.qk == Q_TYPE) {
        uint32_t want = ui.lt_want ? (uint32_t)(ui.lt_want - 1) & 3u : ui_l_type3(tr);
        sp_put(sp_put(sp_put(sp_fix(sp_put(t, "T"), (int32_t)tr + 1, 0u, 0), ": "), LT_TYPE3[want]), "?");
        sp_put(sp_put(n, "ANYTHING ELSE: STAYS "), LT_TYPE3[ui_l_type3(tr)]);
        l1 = "SEQ TWICE: CHANGE IT", l2 = n, l3 = "ITS CLIPS STAY EITHER WAY";
    } else if (ui.qk == Q_WIPE) {
        sp_put(t, "WIPE?");
        l1 = "HOME TWICE: EVERY SETTING BACK", l2 = "ONCE, OR ANOTHER BUTTON: KEEP THEM", l3 = "PRESETS AND PROJECTS STAY";
    } else if (ui.qk == Q_TEST) {
        sp_put(sp_put(sp_put(sp_fix(sp_put(t, "TEST "), (int32_t)ui.tp_k + 1, 0u, 0), ": "), sy_tp_name(ui.tp_k)), "?");
        l1 = "HOME TWICE: LOAD IT AND START IT", l2 = "ONCE, OR ANOTHER BUTTON: NO", l3 = "NOTHING FROM IT IS EVER SAVED",
        l4 = "LOADING A PROJECT ENDS IT";   /* (V0.87.1) */
    } else {
        uint32_t c = (uint32_t)(ui.qv ? ui.qv - 1u : 0u), b = lp_clip_bars(c / LP_NSCN, c % LP_NSCN);
        sp_put(t, "DELETE?");
        sp_put(sp_fix(sp_put(ui_l_clip(n, c / LP_NSCN, c % LP_NSCN), " \xB7 "), (int32_t)b, 0u, 0), b == 1u ? " BAR" : " BARS");
        l1 = "SEQ+OCT- AGAIN: DELETE IT";
        l3 = n;
    }
    lk_frame(14.5f, 44.5f, 211.0f, 128.0f, 1.5f, sk.hot);
    lk_text(26.0f, 56.0f, T14B, t, sk.hot, sk.ground, 0, 17);
    if (ui.qk == Q_CLIPDEL)
        lk_text(26.0f, 82.0f, T12B, l3, sk.ink, sk.ground, 0, 15);
    lk_text(26.0f, 108.0f, T9, l1, sk.ink, sk.ground, 20, 0);
    lk_text(26.0f, 124.0f, T9, l2, sk.lab, sk.ground, 20, 0);
    if (l3 && ui.qk != Q_CLIPDEL)
        lk_text(26.0f, 148.0f, T9, l3, sk.lab, sk.ground, 20, 0);
    if (l4)
        lk_text(26.0f, 160.0f, T9, l4, sk.lab, sk.ground, 20, 0);
}

static int sy_app_project(uint32_t p);              /* sy_app.c: the project saved, project p loaded */
static void sy_app_project_new(void);               /* .. the project cleared */
/* a turn of the looper's knobs (sy_ui.c ui_item_turn) */
static void ui_loop_turn(int32_t item, int32_t s, uint32_t e)
{
    uint32_t t = lpp.sel % LP_NTRK;
    lp_trk_t *k = &lpp.trk[t];
    char m[40];
    switch (item) {
    case UI_LTRK: lp_select((uint32_t)sy_clampi((int32_t)t + s, 0, LP_NTRK - 1)); return;
    case UI_LSCN:
        lpp.scn = (uint8_t)sy_clampi((int32_t)lpp.scn + s, 0, LP_NSCN - 1);
        ui.lnote = 0;
        ui.paste_t = ui.halve_t = ui.clr_t2 = 0;
        break;
    case UI_LVOL: k->vol = (uint8_t)sy_clampi(k->vol + ui_accel(e, s, 127), 0, 127); break;
    case UI_LPAN: k->pan = (uint8_t)ui_centre(item, s, k->pan, sy_clampi(k->pan + ui_accel(e, s, 127), 1, 127), 64); break;
    case UI_LMS: {                                  /* (PLAYS, MUTE, SOLO) */
        int32_t v = k->solo ? 2 : k->mute ? 1 : 0;
        v = sy_clampi(v + s, 0, 2);
        k->mute = v == 1, k->solo = v == 2;
        break;
    }
    case UI_LTYPE:                                  /* (V0.85: DRUMS too. V0.86, C6: it goes when the turning stops; */
        if (ui.lt_ask) {                            /* a track with clips: V0.87 (S1), another type asked (back */
            int32_t w = sy_clampi((int32_t)(ui.lt_want - 1) + s, 0, 3);   /* to its own: kept) */
            if ((uint32_t)w == ui_l_type3(t)) {
                ui_l_type_keep();
                return;
            }
            ui.lt_want = (int8_t)(w + 1);
            ui.dirty = D_ALL;
            return;
        }
        ui.lt_want = (int8_t)(sy_clampi((ui.lt_want ? ui.lt_want - 1 : (int32_t)ui_l_type3(t)) + s, 0, 3) + 1);
        ui.lt_t = fm1_ms;
        break;
    case UI_LSPEED: {                               /* (V0.86, S8: x1/2, x1, x2 as the knob goes) */
        static const uint8_t ORD[3] = {LS_HALF, LS_1, LS_2};
        uint32_t sc = lpp.scn % LP_NSCN, sp = lpp.speed[t][sc] % LS_N;
        if (!lpp.clip[t][sc].len) {
            ui_toast("EMPTY: REC RECORDS IT");
            return;
        }
        if (lp_cur_rec() && lpr.st == LR_TAKE) {
            ui_toast("THE TAKE SETS IT: REC ENDS IT");
            return;
        }
        lp_speed_set(t, sc, ORD[sy_clampi((sp == LS_HALF ? 0 : sp == LS_1 ? 1 : 2) + s, 0, 2)]);
        break;
    }
    case UI_LCOPY: ui_l_copy(t, s); break;
    case UI_LPASTE:
        if (s > 0)
            ui_l_paste(t);
        break;
    case UI_LDBL: ui_l_dbl(t, s); break;
    case UI_LCLR: ui_l_clear(t, s); break;
    case UI_LCH: k->ch = (uint8_t)sy_clampi(k->ch + s, 0, 15); break;
    case UI_LCIN: k->cin = (uint8_t)sy_clampi(k->cin + s, 0, 16); break;
    case UI_LQNT: lpg.quant = (uint8_t)sy_clampi(lpg.quant + s, 0, LQ_N - 1); break;
    case UI_LCNT: lpg.cin = s > 0; break;
    case UI_LLAUNCH: lpg.launch = (uint8_t)sy_clampi(lpg.launch + s, 0, LL_N - 1); break;
    case UI_LCLK: lpg.click = (uint8_t)sy_clampi(lpg.click + s, 0, LC_N - 1); break;
    case UI_LCLVL: lpg.clvl = (uint8_t)sy_clampi(lpg.clvl + s, 0, 10); break;
    case UI_LNOTE: {
        uint32_t n = lp_note_n(lp_cur_clip());
        ui.lnote = (uint8_t)(n ? sy_clampi((int32_t)ui_l_note_cur() + s, 0, (int32_t)n - 1) : 0);
        break;
    }
    case UI_LPITCH: case UI_LSTART: case UI_LLEN: case UI_LVEL: {
        int32_t d = item == UI_LPITCH ? ui_accel(e, s, 24) : item == UI_LVEL ? ui_accel(e, s, 126)
                                                            : s * (int32_t)LG_TICKS[ui.lgrid % LG_N];
        int32_t r = lp_note_edit(ui_l_note_cur(), item == UI_LPITCH ? 0u : item == UI_LSTART ? 1u : item == UI_LLEN ? 2u : 3u, d);
        if (r < 0) {
            ui_toast("NO NOTES IN THIS CLIP");
            return;
        }
        ui.lnote = (uint8_t)r;
        break;
    }
    case UI_LGRID: ui.lgrid = (uint8_t)sy_clampi(ui.lgrid + s, 0, LG_N - 1); break;
    case UI_LDEL:
        if (s > 0) {
            int32_t r = lp_note_edit(ui_l_note_cur(), 4u, 0);
            ui.lnote = (uint8_t)(r < 0 ? 0 : r);
            ui_toast(r < -1 ? "NO NOTES IN THIS CLIP" : "NOTE DELETED");
        }
        break;
    case UI_LBARS:
        if (lp_cur_rec() && lpr.st == LR_TAKE) {
            ui_toast("THE TAKE SETS IT: REC ENDS IT");
            return;
        }
        lp_clip_bars_set((uint32_t)sy_clampi((int32_t)lp_clip_bars(t, lpp.scn) + s, 1, (int32_t)lp_max_bars()));
        break;
    case UI_LQAPP:
        if (s > 0 && lp_cur_clip()->len) {
            uint32_t g = LG_TICKS[ui.lgrid % LG_N];
            lp_clip_quantize(g > 1u ? g : 24u);
            sp_put(sp_put(m, "ITS NOTES ON "), g > 1u ? LG_NAME[ui.lgrid % LG_N] : "1/16");
            ui_toast(m);
        }
        break;
    case UI_LMOVE: {
        uint16_t ids[8];
        uint32_t n = ui_l_moves(ids);
        ui.lmv = (uint8_t)(n ? sy_clampi(ui.lmv + s, 0, (int32_t)n - 1) : 0);
        break;
    }
    case UI_LSLOT: ui.lslot = (uint8_t)sy_clampi(ui.lslot + s, 0, LP_NSONG - 1); break;
    case UI_LSSCN: {                                /* (END, then the scenes) */
        int32_t v = lpp.song[ui.lslot % LP_NSONG] < LP_NSCN ? lpp.song[ui.lslot % LP_NSONG] : -1;
        v = sy_clampi(v + s, -1, LP_NSCN - 1);
        lpp.song[ui.lslot % LP_NSONG] = v < 0 ? LP_NOSCN : (uint8_t)v;
        break;
    }
    case UI_LSREP: lpp.srep[ui.lslot % LP_NSONG] = (uint8_t)sy_clampi(lpp.srep[ui.lslot % LP_NSONG] + s, 1, 16); break;
    case UI_LSON: lpp.song_on = s > 0; break;
    default: break;
    }
    lpx.seq++;
    ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
}

/* the launch pad (SEQ held): key k's role: 0 .. 7 a track's clip, 8 .. 15 a scene (the white keys), 16 .. 23 a track's
 * MUTE, 24 STOP ALL, 25 SOLO, 26 UNDO PASS (the black ones); -1 none */
static int32_t ui_pad_of(uint32_t k)
{
    uint32_t i, n = 0;
    if (k >= 27u)
        return -1;
    for (i = 0; i < k; i++)
        n += (LK_BLACK[i % 12u] != 0) == (LK_BLACK[k % 12u] != 0);
    return LK_BLACK[k % 12u] ? (int32_t)(16u + n) : (int32_t)n;
}
static int ui_loop_key(uint32_t k)                  /* .. its press (1: taken, no note) */
{
    int32_t r = ui_pad_of(k);
    char m[32];
    ui.bt_chord |= (uint16_t)(1u << B_SEQ);         /* (a key used: no reset at 5 s) */
    ui_cd_stop();
    if (r < 0)
        return 1;
    if (r < 8) {                                    /* a track: selected; its clip in the scene picked launched */
        lp_select((uint32_t)r);
        if (lpp.clip[r][lpp.scn % LP_NSCN].len)
            ui_l_launch((uint32_t)r, lpp.scn);
        else
            ui_l_trk(m, (uint32_t)r), ui_toast(m);
    } else if (r < 16) {
        ui_l_scene((uint32_t)r - 8u);
    } else if (r < 24) {
        lp_trk_t *t = &lpp.trk[r - 16];
        t->mute ^= 1u;
        sp_put(sp_fix(sp_put(m, "T"), r - 15, 0u, 0), t->mute ? " MUTED" : " PLAYS");
        ui_toast(m);
    } else if (r == 24) {
        lpr.req = LP_REQ_STOPALL;
        ui_toast(cht.run && lpg.launch != LL_NOW ? "ALL STOP AT THE BAR" : "ALL STOP");
    } else if (r == 25) {
        lp_trk_t *t = &lpp.trk[lpp.sel % LP_NTRK];
        t->solo ^= 1u;
        sp_put(sp_fix(sp_put(m, "T"), (int32_t)lpp.sel % LP_NTRK + 1, 0u, 0), t->solo ? " SOLO" : " SOLO OFF");
        ui_toast(m);
    } else {
        lpr.req = LP_REQ_UNDO;
        ui_toast(lpr.nundo ? "PASS PUT BACK" : "PASS TAKEN BACK");
    }
    lpx.seq++;
    ui.dirty = D_ALL;
    return 1;
}

/* V0.86, S5: on CLIPS the keys launch, SEQ not held: white 1-8 this scene's clips on tracks 1-8 (again: it stops),
 * black 1-8 the scenes A-H; the others nothing */
static int ui_clips_keys(void) { return !ui.lay && !ui.mod && UI_PG[ui.page].graph == G_LCLIPS; }
static int ui_clips_key(uint32_t k)
{
    int32_t r = ui_pad_of(k);
    if (r >= 0 && r < 8)
        ui_l_launch((uint32_t)r, lpp.scn);
    else if (r >= 16 && r < 24)
        ui_l_scene((uint32_t)r - 16u);
    lpx.seq++;
    ui.dirty = D_ALL;
    return 1;
}
/* .. their LEDs (sy_ui_draw.c sy_ui_leds; 1: key k's set here): a clip playing lit, launched blinking, there glowing;
 * the scene picked lit, a scene with clips glowing */
static int ui_clips_leds(uint32_t k)
{
    int32_t r;
    uint32_t sc = lpp.scn % LP_NSCN, t;
    int on = 0, dim = 0;
    if (!ui_clips_keys())
        return 0;
    r = ui_pad_of(k);
    if (r >= 0 && r < 8) {
        on = (cht.run && lps[r].play == sc && lps[r].next == LP_NOSCN) ||
             ((lps[r].next == sc || (lpr.st != LR_OFF && lpr.trk == (uint32_t)r && lpr.scn == sc)) && ((fm1_ms / 250u) & 1u));
        dim = lpp.clip[r][sc].len != 0u;
    } else if (r >= 16 && r < 24) {
        on = (uint32_t)r - 16u == sc;
        for (t = 0; t < LP_NTRK; t++)
            dim |= lpp.clip[t][r - 16].len != 0u;
    }
    if (on)
        ui_led(fm1_led, 14u + k);
    else if (dim)
        ui_led(fm1_led_dim, 14u + k);
    return 1;
}

/* a project read (sy_app.c; ok 2: none kept, the UI's SCALE kept for its tracks), or not (ok 0); blocked: the one
 * loaded is not saved where older presets are */
static void sy_ui_project(int ok, int blocked)
{
    char m[40];
    if (blocked) {
        sp_put(sp_fix(sp_put(m, "PROJECT "), (int32_t)lpg.prj % LP_NPRJ + 1, 0u, 0), ": OLD PRESETS THERE");
        ui_toast(m);
        ui.dirty = D_ALL;
        return;
    }
    lp_select_to(lpp.sel, ok == 2);
    ui.kedited = 0;                                 /* (V0.86: the project's kit as it was kept) */
    ui.lnote = ui.lmv = ui.lslot = 0;
    if (!ok)
        ui_toast("NOT SAVED: NOT LOADED");
    ui.dirty = D_ALL;
}
/* every millisecond (sy_ui.c sy_ui_ms): the transport as the audio side has it (a COUNT-IN or a launch started it),
 * what the recording did (a word), the looper's pages redrawn when it moved (25 a second at most) */
static void sy_ui_loop_ms(void)
{
    static uint16_t seq;
    static uint8_t st;
    static uint32_t t;
    char m[40], *e;
    if ((cht.run != 0) != (ui.run != 0) && fm1_ms - ui.run_t > 80u)
        sy_ui_run(cht.run, 1);
    if (lpr.st != st) {
        e = sp_put(sp_put(sp_fix(sp_put(m, "T"), (int32_t)lpr.trk % LP_NTRK + 1, 0u, 0), " "), (char[2]){ui_l_scn(lpr.scn), 0});
        switch (lpr.st) {
        case LR_COUNT: sp_put(e, ": COUNT-IN"); break;
        case LR_WAIT: sp_put(e, lpr.dub ? ": OVERDUB, ITS START" : ": REC AT THE BAR"); break;
        case LR_TAKE: sp_put(e, ": RECORDING"); break;
        case LR_DUB: sp_put(e, ": OVERDUB"); break;
        default:
            if (st == LR_TAKE) {
                uint32_t b = lp_clip_bars(lpr.trk, lpr.scn);
                sp_put(sp_fix(sp_put(e, ": "), (int32_t)b, 0u, 0), b == 1u ? " BAR, LOOPING" : " BARS, LOOPING");
            } else {
                sp_put(e, st == LR_DUB ? ": OVERDUB OFF" : ": REC OFF");
            }
            break;
        }
        ui_toast(m);
        st = lpr.st;
        ui.dirty |= D_DISP | D_HEAD | D_KNOBS;
    }
    if (lpr.full) {
        lpr.full = 0;
        ui_toast("FULL: 2048 NOTES AND MOVES");
    }
    sy_ui_drum_ms();                                /* (V0.85: stopped, the drum played is KIT's) */
    if (ui.lt_want && !ui.lt_ask && fm1_ms - ui.lt_t >= UI_LT_MS) {   /* (C6: TYPE stopped turning: it goes, or a */
        uint32_t tr = lpp.sel % LP_NTRK, sc, any = 0;                    /* track with clips asks) */
        for (sc = 0; sc < LP_NSCN; sc++)
            any |= lpp.clip[tr][sc].len != 0u;
        if ((uint32_t)(ui.lt_want - 1) == ui_l_type3(tr)) {
            ui.lt_want = 0;
        } else if (any) {                           /* (V0.87, S1: the question, until answered) */
            ui.lt_ask = 1;
            ui.lt_t = fm1_ms;
            ui.qk = Q_TYPE;
            ui.dirty = D_ALL;
        } else {
            ui_l_type_go();
        }
        ui.dirty |= D_DISP | D_KNOBS;
    }
    if ((ui.paste_t && fm1_ms - ui.paste_t >= UI_ASK_MS) || (ui.halve_t && fm1_ms - ui.halve_t >= UI_ASK_MS) ||
        (ui.clr_t2 && fm1_ms - ui.clr_t2 >= UI_ASK_MS)) {   /* (an ask not answered: dropped, TOOLS redrawn) */
        ui.paste_t = ui.halve_t = ui.clr_t2 = 0;
        ui.dirty |= D_DISP | D_KNOBS;
    }
    if ((ui.fam == FAM_SEQ || ui.lay == LAY_PAD) && (lpx.seq != seq || cht.run) &&
        fm1_ms - t >= (lpx.seq != seq ? 40u : 80u)) {   /* (a change at once; the places moving 12 a second) */
        seq = lpx.seq;
        t = fm1_ms;
        ui.dirty |= D_DISP | D_HEAD | D_KNOBS;
    }
}

/* ------------------------------------------------------------------------------------------- values --- */
/* knob k's name and value on the looper's pages (sy_ui_draw.c ui_knob_nv) */
static const char *ui_loop_nv(uint32_t id, char *v)
{
    uint32_t t = lpp.sel % LP_NTRK;
    const lp_trk_t *k = &lpp.trk[t];
    const lp_clip_t *c = lp_cur_clip();
    v[0] = 0;
    switch (id) {
    case UI_LTRK: ui_l_trk(v, t); return "TRACK";
    case UI_LSCN: v[0] = ui_l_scn(lpp.scn), v[1] = 0; return "SCENE";
    case UI_LVOL: sp_fix(v, k->vol, 0u, 0); return "VOLUME";
    case UI_LPAN:
        if (k->pan == 64u)
            sp_put(v, "CENTRE");
        else
            sp_put(sp_fix(v, k->pan < 64u ? 64 - k->pan : k->pan - 64, 0u, 0), k->pan < 64u ? " L" : " R");
        return "PAN";
    case UI_LMS: sp_put(v, k->solo ? "SOLO" : k->mute ? "MUTE" : "PLAYS"); return "MUTE/SOLO";
    case UI_LTYPE:                                  /* (C6: the type it goes to, while it waits) */
        sp_put(sp_put(v, LT_TYPE3S[ui.lt_want ? (uint32_t)(ui.lt_want - 1) & 3u : ui_l_type3(t)]), ui.lt_ask ? "?" : "");
        return "TYPE";
    case UI_LSPEED: {
        uint32_t sp = lpp.speed[t][lpp.scn % LP_NSCN] % LS_N;
        sp_put(v, !c->len ? "\x97" : sp == LS_HALF ? "\xD7" "1/2" : sp == LS_2 ? "\xD7" "2" : "\xD7" "1");
        return "SPEED";
    }
    case UI_LCOPY:
        if (!ui.cb_on)
            sp_put(v, c->len ? "TURN" : "\x97");
        else if (ui.cb_bar >= 0)
            sp_fix(sp_put(v, "BAR "), ui.cb_bar + 1, 0u, 0);
        else
            ui_l_clip(v, ui.cb_t, ui.cb_s);
        return "COPY";
    case UI_LPASTE: sp_put(v, ui.paste_t ? "AGAIN" : ui.cb_on ? "TURN" : "\x97"); return "PASTE";
    case UI_LDBL: sp_put(v, ui.halve_t ? "AGAIN" : c->len ? "\xD7" "2 / HALF" : "\x97"); return "DOUBLE/HALVE";
    case UI_LCLR: sp_put(v, ui.clr_t2 ? "AGAIN" : c->len ? "TURN" : "\x97"); return "CLEAR";
    case UI_LCH: sp_fix(sp_put(v, "CH "), k->ch + 1, 0u, 0); return "CHANNEL";
    case UI_LCIN:
        if (k->cin)
            sp_fix(sp_put(v, "CH "), k->cin, 0u, 0);
        else
            sp_put(v, "SELECTED");
        return "MIDI IN";
    case UI_LQNT: sp_put(v, LQ_NAME[lpg.quant % LQ_N]); return "QUANTIZE";
    case UI_LCNT: sp_put(v, lpg.cin ? "1 BAR" : "OFF"); return "COUNT-IN";
    case UI_LLAUNCH: sp_put(v, LL_NAME[lpg.launch % LL_N]); return "LAUNCH";
    case UI_LCLK: sp_put(v, LC_NAME[lpg.click % LC_N]); return "CLICK";
    case UI_LCLVL: sp_fix(v, lpg.clvl, 0u, 0); return "LEVEL";
    case UI_LNOTE: {
        uint32_t n = lp_note_n(c);
        if (n)
            sp_fix(sp_put(sp_fix(v, (int32_t)ui_l_note_cur() + 1, 0u, 0), " OF "), (int32_t)n, 0u, 0);
        else
            sp_put(v, "NONE");
        return "NOTE";
    }
    case UI_LPITCH: case UI_LSTART: case UI_LLEN: case UI_LVEL: {
        uint32_t i = lp_note_at(c, ui_l_note_cur());
        if (i == LP_END)
            sp_put(v, "\x97");
        else if (id == UI_LPITCH)
            ui_l_note(v, lp_ev[i].a);
        else if (id == UI_LSTART)
            ui_l_pos(v, lp_et(i));
        else if (id == UI_LLEN)
            ui_l_len(v, lp_len(i));
        else
            sp_fix(v, lp_ev[i].b & 0x7F, 0u, 0);
        return id == UI_LPITCH ? "PITCH" : id == UI_LSTART ? "START" : id == UI_LLEN ? "LENGTH" : "VELOCITY";
    }
    case UI_LGRID: sp_put(v, LG_NAME[ui.lgrid % LG_N]); return "GRID";
    case UI_LDEL: sp_put(v, lp_note_n(c) ? "TURN" : "\x97"); return "DELETE";
    case UI_LBARS: {
        uint32_t b = lp_clip_bars(t, lpp.scn);
        if (b)
            sp_put(sp_fix(v, (int32_t)b, 0u, 0), b == 1u ? " BAR" : " BARS");
        else
            sp_put(v, "EMPTY");
        return "LENGTH";
    }
    case UI_LQAPP: sp_put(sp_put(v, "TO "), ui.lgrid ? LG_NAME[ui.lgrid % LG_N] : "1/16"); return "QUANTIZE";
    case UI_LMOVE: {
        uint16_t ids[8];
        uint32_t n = ui_l_moves(ids);
        sp_put(v, n ? SP[ids[ui.lmv % n]].name : "NONE");
        return "MOTION";
    }
    case UI_LSLOT: sp_fix(v, (int32_t)ui.lslot % LP_NSONG + 1, 0u, 0); return "SLOT";
    case UI_LSSCN:
        if (lpp.song[ui.lslot % LP_NSONG] < LP_NSCN)
            v[0] = ui_l_scn(lpp.song[ui.lslot % LP_NSONG]), v[1] = 0;
        else
            sp_put(v, "END");
        return "SCENE";
    case UI_LSREP:
        if (lpp.srep[ui.lslot % LP_NSONG] <= 1u)
            sp_put(v, "ONCE");
        else
            sp_put(sp_fix(v, lpp.srep[ui.lslot % LP_NSONG], 0u, 0), " TIMES");
        return "REPEATS";
    case UI_LSON: sp_put(v, lpp.song_on ? "ON" : "OFF"); return "SONG";
    default: return "";
    }
}

/* ------------------------------------------------------------------------------------------- pages --- */
/* knobs k0 .. k0 + 2 in a row under a page's drawing: a word over the value each */
static void ui_l_row(uint32_t k0, const int32_t *ids)
{
    static const float CX[3] = {26.0f, 104.0f, 182.0f};
    char v[40], f[40];
    uint32_t i;
    for (i = 0; i < 3u; i++) {
        const char *nm;
        if (ids[i] < 0)
            continue;
        nm = ui_loop_nv((uint32_t)ids[i], v);
        lk_text(CX[i], 176.0f, T9, nm, sk.lab, sk.ground, 20, 0);
        lk_text(CX[i], 188.0f, T10B, lk_fit(f, sizeof f, v, T10B, 0, i < 2u ? 74.0f : 50.0f), ui_ink(ui_hot(k0 + i)),
                sk.ground, 0, 0);
        ui_H(k0 + i, CX[i] - 18.0f, 185.0f);
    }
    pg.figs = 0xFu;
}
/* a page's title: the track selected or a word (ptag: the track's, PRESETS turning it: a P before it, V0.86 C1); at
 * the right what the transport or the recording does */
static void ui_l_head(const char *word, int ptag)
{
    char t[24];
    const char *st = lpr.st == LR_COUNT ? "COUNT-IN" : lpr.st == LR_WAIT ? "REC: NEXT BAR"
                     : lpr.st == LR_TAKE ? "REC" : lpr.st == LR_DUB ? "OVERDUB" : cht.run ? "PLAYING" : "STOPPED";
    if (!word)
        ui_l_trk(t, lpp.sel);
    lk_text(UI_TITLE_X, 24.0f, T14B, word ? word : t, sk.ink, sk.ground, 0, 17);
    if (ptag)
        lk_ptag(10.0f, 28.0f, 0);
    lk_chip(230.0f - lk_tw(T10B, st, 20) - 10.0f, 24.0f, st, lpr.st != LR_OFF || cht.run);
}

/* CLIPS: the grid. V0.87 (U20): a row a track, a column a scene (the tracks down the side, their clips across) */
#define UI_LX0 30.0f
#define UI_LCW 25.0f
#define UI_LY0 62.0f
#define UI_LRH 13.0f
static void ui_p_lclips(void)
{
    uint32_t t, s, sel = lpp.sel % LP_NTRK, cs = lpp.scn % LP_NSCN, blink = (fm1_ms / 250u) & 1u;
    char n[4];
    ui_l_head(0, 0);                                /* (V0.87.1: no P tag: PRESETS is the sound's again) */
    lk_box(UI_LX0 - 26.0f, UI_LY0 + UI_LRH * (float)sel, UI_LX0 + UI_LCW * (float)LP_NSCN,
           UI_LY0 + UI_LRH * (float)(sel + 1u), sk.line);   /* (the track selected's row lit) */
    for (s = 0; s < LP_NSCN; s++) {                 /* (the scenes' letters: the one picked bold) */
        char l[2] = {ui_l_scn(s), 0};
        lk_text_c(UI_LX0 + UI_LCW * ((float)s + 0.5f), 48.0f, s == cs ? T9B : T9, l,
                  s == cs ? sk.ink : sk.lab, sk.ground, 0, 0);
    }
    for (t = 0; t < LP_NTRK; t++) {
        float y = UI_LY0 + UI_LRH * (float)t;
        sp_fix(sp_put(n, "T"), (int32_t)t + 1, 0u, 0);   /* (the tracks' numbers: the one selected boxed, a muted one */
        if (t == sel) {                                     /* underlined) */
            lk_box(5.0f, y + 1.0f, 25.0f, y + UI_LRH - 1.0f, sk.ink);
            lk_text_c(15.0f, y + 1.5f, T9B, n, sk.ground, sk.ink, 0, 0);
        } else {
            lk_text_c(15.0f, y + 1.5f, T9, n, sk.lab, sk.ground, 0, 0);
            if (lp_muted(t))
                lk_seg(8.0f, y + UI_LRH - 1.5f, 22.0f, y + UI_LRH - 1.5f, 1.0f, sk.mid);
        }
        for (s = 0; s < LP_NSCN; s++) {
            const lp_clip_t *c = &lpp.clip[t][s];
            float x = UI_LX0 + UI_LCW * (float)s, x0 = x + 3.0f, x1 = x + UI_LCW - 3.0f, y0 = y + 2.0f, y1 = y + UI_LRH - 2.0f;
            int rec = lpr.st != LR_OFF && lpr.trk == t && lpr.scn == s, on = lps[t].play == s, nx = lps[t].next == s && blink;
            uint16_t bg = t == sel ? sk.line : sk.ground;
            if (rec) {
                lk_box(x0, y0, x1, y1, sk.hot);
            } else if (on && c->len) {
                lk_box(x0, y0, x1, y1, cht.run ? (lp_muted(t) ? sk.mid : sk.ink) : sk.mid);
                if (cht.run)                         /* (its place in it) */
                    lk_box(x0 + 1.0f, y1 - 2.0f, x0 + 1.0f + (x1 - x0 - 2.0f) * (float)lps[t].pos / (float)c->len, y1 - 1.0f,
                           bg);
                if (lps[t].next == LP_STOP)
                    lk_box(0.5f * (x0 + x1) - 2.0f, y0 + 2.0f, 0.5f * (x0 + x1) + 2.0f, y0 + 6.0f, bg);
            } else if (c->len) {
                lk_frame(x0 + 0.5f, y0 + 0.5f, x1 - x0 - 1.0f, y1 - y0 - 1.0f, 1.0f, lp_muted(t) && t != sel ? sk.line : sk.mid);
            } else {
                lk_box(0.5f * (x0 + x1) - 1.0f, 0.5f * (y0 + y1) - 1.0f, 0.5f * (x0 + x1) + 1.0f, 0.5f * (y0 + y1) + 1.0f,
                       t == sel ? sk.mid : sk.line);
            }
            if (nx)                                 /* (launched: from the next bar) */
                lk_frame(x0 - 1.5f, y0 - 1.5f, x1 - x0 + 3.0f, y1 - y0 + 3.0f, 1.0f, sk.hot);
            if (t == sel && s == cs)
                lk_frame(x + 0.5f, y + 0.5f, UI_LCW - 1.0f, UI_LRH - 1.0f, 1.0f, sk.ink);
        }
    }
    {                                               /* (V0.87.1: TYPE, MUTE / SOLO, and KNOB 4's track: KNOB 3 has none; */
        static const float CX[3] = {26.0f, 104.0f, 182.0f};   /* OCT- and OCT+ step the scenes) */
        static const int32_t IDS[3] = {UI_LTYPE, UI_LMS, UI_LTRK};
        char v[40], f[40];
        for (t = 0; t < 3u; t++) {
            const char *nm;
            if (IDS[t] == UI_LTRK) {
                nm = "TRACK";
                sp_fix(sp_put(v, "T"), (int32_t)sel + 1, 0u, 0);
            } else {
                nm = ui_loop_nv((uint32_t)IDS[t], v);
            }
            lk_text(CX[t], 176.0f, T9, nm, sk.lab, sk.ground, 20, 0);
            lk_text(CX[t], 188.0f, T10B, lk_fit(f, sizeof f, v, T10B, 0, t < 2u ? 74.0f : 50.0f), ui_ink(ui_hot(t == 2u ? 3u : t)),
                    sk.ground, 0, 0);
            ui_H(t == 2u ? 3u : t, CX[t] - 18.0f, 185.0f);
        }
        lk_text(8.0f, 205.0f, T9, "KEYS: LAUNCH \xB7 OCT-, OCT+: SCENE", sk.lab, sk.ground, 20, 0);
        pg.figs = 0xFu;
    }
}

/* MIXER: a strip a track (its PAN over a fader of VOLUME; M, S under), the one selected framed */
static void ui_p_lmix(void)
{
    static const int32_t IDS[3] = {UI_LVOL, UI_LPAN, UI_LMS};
    uint32_t t, sel = lpp.sel % LP_NTRK;
    char n[4];
    ui_l_head(0, 1);                                /* (V0.86: VOLUME, PAN, MUTE / SOLO on KNOBS 1-3) */
    for (t = 0; t < LP_NTRK; t++) {
        const lp_trk_t *k = &lpp.trk[t];
        float x = UI_LX0 + UI_LCW * (float)t, cx = x + 0.5f * UI_LCW, top = 74.0f, bot = 146.0f;
        float lv = bot - (bot - top) * (float)k->vol / 127.0f, px = cx + 8.0f * ((float)k->pan - 64.0f) / 63.0f;
        int me = t == sel;
        uint16_t ink = lp_muted(t) ? sk.mid : sk.ink;
        sp_fix(n, (int32_t)t + 1, 0u, 0);
        if (me) {
            lk_box(x + 3.0f, 46.0f, x + UI_LCW - 3.0f, 58.0f, sk.ink);
            lk_text_c(cx, 47.0f, T9B, n, sk.ground, sk.ink, 0, 0);
        } else {
            lk_text_c(cx, 47.0f, T9, n, sk.lab, sk.ground, 0, 0);
        }
        lk_seg(cx - 8.0f, 66.0f, cx + 8.0f, 66.0f, 1.0f, sk.line);
        lk_disc(px, 66.0f, 2.0f, me && ui_hot(1) ? sk.hot : ink);
        lk_seg(cx, top, cx, bot, 1.0f, sk.line);
        lk_seg(cx - 4.0f, bot - (bot - top) * 100.0f / 127.0f, cx + 4.0f, bot - (bot - top) * 100.0f / 127.0f, 1.0f, sk.line);
        lk_box(cx - 3.0f, lv, cx + 3.0f, bot, me && ui_hot(0) ? sk.hot : ink);
        if (k->mute)
            lk_text_c(cx - 5.0f, 151.0f, T9B, "M", me && ui_hot(2) ? sk.hot : sk.ink, sk.ground, 0, 0);
        if (k->solo)
            lk_text_c(cx + 5.0f, 151.0f, T9B, "S", sk.hot, sk.ground, 0, 0);
        if (me)
            lk_frame(x + 0.5f, 62.5f, UI_LCW - 1.0f, 102.0f, 1.0f, sk.mid);
    }
    ui_l_row(0u, IDS);
}

/* RECORD (V0.86: the TRACK page's CHANNEL and MIDI IN here, S1): QUANTIZE as chips, the track's CHANNEL out, MIDI IN */
static void ui_p_lrec(void)
{
    uint32_t t = lpp.sel % LP_NTRK, ty = ui_l_type3(t), i;
    const lp_trk_t *k = &lpp.trk[t];
    char v[32];
    float x;
    pg.figs = 0xFu;
    sp_fix(sp_put(v, "TRACK "), (int32_t)t + 1, 0u, 0);
    ui_l_head(v, 1);
    lk_label(UI_TITLE_X, 52.0f, "QUANTIZE: THE NOTES PLAYED IN", 20);
    for (i = 0, x = UI_TITLE_X; i < LQ_N; i++)
        x = ui_chip(x, 66.0f, LQ_NAME[i], i == lpg.quant % LQ_N, ui_hot(0));
    lk_label(UI_TITLE_X, 92.0f, ty == 1u || ty == 2u ? "ITS NOTES OUT ON USB MIDI: CHANNEL"
                                : "MIDI CHANNEL (OUT WITH SYNTH+MIDI)", 20);
    sp_fix(sp_put(v, "CH "), k->ch + 1, 0u, 0);
    lk_text(UI_TITLE_X, 106.0f, T12B, v, ui_ink(ui_hot(1)), sk.ground, 0, 0);
    lk_label(UI_TITLE_X, 132.0f, "MIDI IN: NOTES ON", 20);
    if (k->cin)
        sp_fix(sp_put(v, "CH "), k->cin, 0u, 0);
    else
        sp_put(v, "ANY, WHILE SELECTED");
    lk_text(UI_TITLE_X, 146.0f, T12B, v, ui_ink(ui_hot(2)), sk.ground, 0, 0);
    ui_l_trk(v, t);
    lk_text(UI_TITLE_X, 176.0f, T9, v, sk.lab, sk.ground, 20, 0);
    lk_text(UI_TITLE_X, 190.0f, T9, "REC: A TAKE; ON A CLIP: OVERDUB", sk.lab, sk.ground, 20, 0);
    ui_H(0, 8.0f, 69.0f);
    ui_H(1, 8.0f, 109.0f);
    ui_H(2, 8.0f, 149.0f);
}

/* CLICK: OFF, REC, ON; LEVEL as ten cells, the bar's beats beside (the one now lit); COUNT-IN, LAUNCH as chips (V0.86:
 * from RECORD) */
static void ui_p_lclick(void)
{
    uint32_t i, nb = (uint32_t)sy_clampi(ui.tsb, 1, 16), at = ui_beat_now();
    float x, sp = nb > 1u ? sy_minf(12.0f, 64.0f / (float)(nb - 1u)) : 0.0f;
    pg.figs = 0xFu;
    ui_l_head("CLICK", 0);
    lk_label(UI_TITLE_X, 48.0f, "ON THE BEATS: RECORDING, OR ALWAYS", 20);
    for (i = 0, x = UI_TITLE_X; i < LC_N; i++)
        x = ui_chip(x, 60.0f, LC_NAME[i], i == lpg.click % LC_N, ui_hot(0));
    lk_label(UI_TITLE_X, 84.0f, "LEVEL", 20);
    for (i = 0; i < 10u; i++) {
        float xc = UI_TITLE_X + 11.0f * (float)i;
        if (i < lpg.clvl)
            lk_box(xc, 97.0f, xc + 8.0f, 107.0f, ui_ink(ui_hot(1)));
        else
            lk_frame(xc + 0.5f, 97.5f, 7.0f, 9.0f, 1.0f, sk.line);
    }
    lk_label(150.0f, 84.0f, "THE BAR", 20);
    for (i = 0; i < nb; i++) {
        float cx = 155.0f + sp * (float)i;
        if (cht.run && i == at)
            lk_disc(cx, 102.0f, i ? 3.0f : 4.5f, sk.hot);
        else
            lk_ring(cx, 102.0f, i ? 2.5f : 4.0f, 1.0f, sk.mid);
    }
    lk_label(UI_TITLE_X, 120.0f, "COUNT-IN: A BAR BEFORE A TAKE", 20);
    x = ui_chip(UI_TITLE_X, 132.0f, "OFF", !lpg.cin, ui_hot(2));
    ui_chip(x, 132.0f, "1 BAR", lpg.cin, ui_hot(2));
    lk_label(UI_TITLE_X, 156.0f, "LAUNCH: A CLIP PLAYS FROM", 20);
    for (i = 0, x = UI_TITLE_X; i < LL_N; i++)
        x = ui_chip(x, 168.0f, LL_NAME[i], i == lpg.launch % LL_N, ui_hot(3));
    lk_text(UI_TITLE_X, 194.0f, T9, "THE DOWNBEAT CLICKS HIGHER", sk.lab, sk.ground, 20, 0);
    ui_H(0, 8.0f, 63.0f);
    ui_H(1, 8.0f, 99.0f);
    ui_H(2, 8.0f, 135.0f);
    ui_H(3, 8.0f, 171.0f);
}

/* NOTES, EDIT: the clip a bar at a time round the note picked, a piano roll (velo: a lane of velocities under it) */
#define UI_LRX0 34.0f
#define UI_LRX1 226.0f
static void ui_l_roll(int velo)
{
    const lp_clip_t *c = lp_cur_clip();
    uint32_t bt = lp_bar_ticks(), qt = (uint32_t)(sy_beat_q * (float)LP_PPQ + 0.5f), pick = lp_note_at(c, ui_l_note_cur());
    uint32_t w0 = pick != LP_END ? lp_et(pick) / bt * bt : 0u, k, b;
    int32_t lo = 127, hi = 0, x, r;
    float y0 = 60.0f, y1 = velo ? 132.0f : 162.0f, h, sx = (UI_LRX1 - UI_LRX0) / (float)bt;
    char t[40], *e;
    int hot = velo ? ui_hot(0) : ui_hot(1) || ui_hot(2) || ui_hot(3);   /* (NOTE+: VELOCITY on KNOB 1) */
    qt = qt ? qt : LP_PPQ;
    e = sp_put(sp_put(t, "T"), "");
    e = sp_fix(e, (int32_t)lpp.sel % LP_NTRK + 1, 0u, 0);
    e = sp_put(sp_put(sp_put(e, " \xB7 "), (char[2]){ui_l_scn(lpp.scn), 0}), " \xB7 ");
    if (c->len)
        sp_fix(sp_put(sp_fix(sp_put(e, "BAR "), (int32_t)(w0 / bt) + 1, 0u, 0), " OF "), (int32_t)lp_clip_bars(lpp.sel, lpp.scn),
               0u, 0);
    else
        sp_put(e, "EMPTY: REC RECORDS IT");
    lk_text(UI_TITLE_X, 42.0f, T10, t, lp_cur_rec() ? sk.hot : sk.lab, sk.ground, 20, 12);
    for (k = c->head; k != LP_END; k = lp_ev[k].next)   /* (the range of the notes in the window) */
        if (!(lp_ev[k].t & LP_MOVE) && lp_et(k) >= w0 && lp_et(k) < w0 + bt) {
            x = lp_ev[k].a;
            lo = x < lo ? x : lo;
            hi = x > hi ? x : hi;
        }
    if (lo > hi)
        lo = 60, hi = 71;
    if (hi - lo < 11) {
        lo -= (11 - (hi - lo)) / 2;
        hi = lo + 11;
    }
    if (lo < 0)
        hi -= lo, lo = 0;
    if (hi > 127)
        lo -= hi - 127, hi = 127;
    r = hi - lo + 1;
    h = (y1 - y0 - 2.0f) / (float)r;
    for (b = 0; b * qt <= bt && qt; b++) {         /* (the beats: ticks under it) */
        float xb = UI_LRX0 + sx * (float)(b * qt);
        lk_seg(xb, y1 + 1.0f, xb, y1 + (b ? 3.0f : 6.0f), 1.0f, b ? sk.line : sk.mid);
    }
    for (x = lo; x <= hi; x++)                      /* (the Cs: a line, the name at the left) */
        if (x % 12 == 0) {
            float y = y1 - 1.0f - h * ((float)(x - lo) + 0.5f);
            lk_seg_dash(UI_LRX0, y, UI_LRX1, y, sk.line, 1.0f, 3.0f);
            ui_l_note(t, (uint32_t)x);
            lk_text_r(UI_LRX0 - 3.0f, y - 5.0f, T9, t, sk.lab, sk.ground, 0, 0);
        }
    lk_frame(UI_LRX0 - 0.5f, y0 - 0.5f, UI_LRX1 - UI_LRX0 + 1.0f, y1 - y0 + 1.0f, 1.0f, sk.line);
    for (k = c->head; k != LP_END; k = lp_ev[k].next) {
        uint32_t et = lp_et(k);
        float xa, xb, y, hh = sy_maxf(0.5f * h - 0.75f, 1.0f);
        uint16_t col;
        if ((lp_ev[k].t & LP_MOVE) || et < w0 || et >= w0 + bt)
            continue;
        y = y1 - 1.0f - h * ((float)(lp_ev[k].a - lo) + 0.5f);
        xa = UI_LRX0 + 1.0f + sx * (float)(et - w0);
        xb = sy_minf(xa + sy_maxf(sx * (float)lp_len(k) - 1.0f, 2.0f), UI_LRX1 - 1.0f);
        col = k == pick && hot ? sk.hot : sy_mix565(sk.mid, sk.ink, (uint32_t)(lp_ev[k].b & 0x7F) * 2u);
        lk_box(xa, y - hh, xb, y + hh, col);
        if (k == pick && !hot)
            lk_frame(xa - 1.5f, y - hh - 1.5f, xb - xa + 3.0f, 2.0f * hh + 3.0f, 1.0f, sk.ink);
        if (velo) {
            float yv = 162.0f - 24.0f * (float)(lp_ev[k].b & 0x7F) / 127.0f;
            lk_seg(xa, 162.0f, xa, yv, k == pick ? 2.0f : 1.0f, k == pick && hot ? sk.hot : sk.mid);
        }
    }
    if (lps[lpp.sel % LP_NTRK].play == lpp.scn && cht.run && c->len) {   /* (the playhead, in this bar) */
        uint32_t p = lps[lpp.sel % LP_NTRK].pos;
        if (p >= w0 && p < w0 + bt) {
            float xp = UI_LRX0 + sx * (float)(p - w0);
            lk_tri(xp - 3.5f, y0 - 7.0f, xp + 3.5f, y0 - 7.0f, xp, y0 - 2.0f, sk.hot);
        }
    }
}
static void ui_p_lnotes(void)
{
    static const int32_t IDS[3] = {UI_LPITCH, UI_LSTART, UI_LLEN};
    char t[24], v[24];
    const char *nm = ui_loop_nv(UI_LNOTE, v);
    (void)nm;
    sp_put(sp_put(t, "NOTE "), v);
    lk_text(UI_TITLE_X, 24.0f, T14B, lp_note_n(lp_cur_clip()) ? t : "NO NOTES", ui_ink(ui_hot(0)), sk.ground, 0, 17);
    ui_H(0, 8.0f, 27.0f);
    ui_l_roll(0);
    ui_l_row(1u, IDS);
}
static void ui_p_ledit(void)
{
    static const int32_t IDS[3] = {UI_LVEL, UI_LGRID, UI_LDEL};
    char t[24], v[24];
    ui_loop_nv(UI_LNOTE, v);
    sp_put(sp_put(t, "NOTE "), v);
    lk_text(UI_TITLE_X, 24.0f, T14B, lp_note_n(lp_cur_clip()) ? t : "NO NOTES", sk.ink, sk.ground, 0, 17);
    ui_l_roll(1);
    ui_l_row(0u, IDS);                              /* (V0.86, C2: NOTE+, the note picked on NOTES) */
}

/* CLIP: LENGTH as cells, the clip's notes at a glance, MOTION's moves of the parameter picked as a line */
static void ui_p_lclip(void)
{
    static const int32_t IDS[3] = {UI_LSPEED, UI_LQAPP, UI_LMOVE};   /* (V0.86: SPEED; CLEAR on TOOLS) */
    const lp_clip_t *c = lp_cur_clip();
    uint32_t bars = lp_clip_bars(lpp.sel, lpp.scn), mb = lp_max_bars(), i, k, nn = lp_note_n(c), nm;
    uint16_t ids[8];
    char t[40], *e;
    int32_t lo = 127, hi = 0;
    nm = ui_l_moves(ids);
    e = sp_put(t, "");
    if (bars)
        sp_put(sp_fix(e, (int32_t)bars, 0u, 0), bars == 1u ? " BAR" : " BARS");
    else
        sp_put(e, "EMPTY");
    lk_text(UI_TITLE_X, 24.0f, T14B, t, ui_ink(ui_hot(0)), sk.ground, 0, 17);
    ui_H(0, 8.0f, 27.0f);
    e = sp_put(sp_put(sp_fix(sp_put(t, "T"), (int32_t)lpp.sel % LP_NTRK + 1, 0u, 0), " \xB7 "), (char[2]){ui_l_scn(lpp.scn), 0});
    e = sp_put(sp_fix(sp_put(e, " \xB7 "), (int32_t)nn, 0u, 0), nn == 1u ? " NOTE" : " NOTES");
    sp_put(sp_fix(sp_put(e, " \xB7 "), (int32_t)nm, 0u, 0), " IN MOTION");
    lk_text(UI_TITLE_X, 42.0f, T10, t, sk.lab, sk.ground, 20, 12);
    for (i = 0; i < LP_MAXBAR; i++) {               /* (LENGTH: a cell a bar; past what this time signature allows: */
        float x = 26.0f + 12.5f * (float)i;         /* none) */
        if (i < bars)
            lk_box(x, 60.0f, x + 10.0f, 68.0f, ui_ink(ui_hot(0)));
        else if (i < mb)
            lk_frame(x + 0.5f, 60.5f, 9.0f, 7.0f, 1.0f, sk.line);
    }
    if (c->len) {                                   /* (the notes: a tick each over the clip, by pitch) */
        for (k = c->head; k != LP_END; k = lp_ev[k].next)
            if (!(lp_ev[k].t & LP_MOVE)) {
                lo = lp_ev[k].a < lo ? lp_ev[k].a : lo;
                hi = lp_ev[k].a > hi ? lp_ev[k].a : hi;
            }
        lk_seg(26.0f, 112.0f, 226.0f, 112.0f, 1.0f, sk.line);
        for (k = c->head; k != LP_END; k = lp_ev[k].next)
            if (!(lp_ev[k].t & LP_MOVE)) {
                float x = 26.0f + 200.0f * (float)lp_et(k) / (float)c->len;
                float y = 108.0f - (hi > lo ? 30.0f * (float)(lp_ev[k].a - lo) / (float)(hi - lo) : 15.0f);
                lk_box(x, y - 1.0f, sy_minf(x + sy_maxf(200.0f * (float)lp_len(k) / (float)c->len, 1.5f), 226.0f),
                       y + 1.0f, sk.ink);
            }
        if (nm) {                                   /* (the moves of the one picked: a step line) */
            uint32_t id = ids[ui.lmv % nm];
            float px = 26.0f, py = -1.0f;
            int32_t plo = SP[id].lo, phi = SP[id].hi;
            lk_seg(26.0f, 162.0f, 226.0f, 162.0f, 1.0f, sk.line);
            for (k = c->head; k != LP_END; k = lp_ev[k].next)
                if ((lp_ev[k].t & LP_MOVE) && lp_ev[k].len == id) {
                    int16_t v = (int16_t)(lp_ev[k].a | (uint16_t)lp_ev[k].b << 8);
                    float x = 26.0f + 200.0f * (float)lp_et(k) / (float)c->len;
                    float y = 160.0f - 36.0f * (phi > plo ? (float)(v - plo) / (float)(phi - plo) : 0.5f);
                    if (py >= 0.0f) {
                        lk_seg(px, py, x, py, 1.0f, ui_ink(ui_hot(3)));
                        lk_seg(x, py, x, y, 1.0f, ui_ink(ui_hot(3)));
                    }
                    px = x, py = y;
                }
            if (py >= 0.0f)
                lk_seg(px, py, 226.0f, py, 1.0f, ui_ink(ui_hot(3)));
        } else {
            lk_text(26.0f, 140.0f, T9, "NO MOTION: A KNOB TURNED IN REC", sk.lab, sk.ground, 20, 0);
        }
    } else {
        lk_text(26.0f, 96.0f, T9, "REC: A TAKE, AS LONG AS PLAYED", sk.lab, sk.ground, 20, 0);
        lk_text(26.0f, 110.0f, T9, "OR KNOB 1: AN EMPTY CLIP", sk.lab, sk.ground, 20, 0);
    }
    ui_l_row(1u, IDS);
}

/* TOOLS (V0.86, S8): COPY, PASTE, DOUBLE / HALVE, CLEAR, a row each: what each does, what it would do now (an ask
 * waiting: HOT) */
static void ui_p_ltools(void)
{
    const lp_clip_t *c = lp_cur_clip();
    uint32_t t = lpp.sel % LP_NTRK, sc = lpp.scn % LP_NSCN, bars = lp_clip_bars(t, sc), nn = lp_note_n(c), nm, i;
    uint16_t ids[8];
    int drum = ui_drums();
    char v[48], f[48], *e;
    pg.figs = 0xFu;
    nm = ui_l_moves(ids);
    ui_l_head(0, 1);
    e = sp_put(sp_put(sp_put(v, "CLIP "), (char[2]){ui_l_scn(sc), 0}), " \xB7 ");
    if (bars) {
        e = sp_put(sp_fix(e, (int32_t)bars, 0u, 0), bars == 1u ? " BAR \xB7 " : " BARS \xB7 ");
        e = sp_put(sp_fix(e, (int32_t)nn, 0u, 0), drum ? (nn == 1u ? " HIT" : " HITS") : nn == 1u ? " NOTE" : " NOTES");
        if (nm)
            sp_put(sp_fix(sp_put(e, " \xB7 "), (int32_t)nm, 0u, 0), " IN MOTION");
    } else {
        sp_put(e, "EMPTY");
    }
    lk_text(UI_TITLE_X, 42.0f, T10, v, lp_cur_rec() ? sk.hot : sk.lab, sk.ground, 20, 12);
    for (i = 0; i < 4u; i++) {
        float y = 58.0f + 36.0f * (float)i;
        const char *lb = "";
        int ask = 0;
        e = v;
        v[0] = 0;
        switch (i) {
        case 0:
            lb = "COPY: THIS CLIP, THEN A BAR OF IT";
            if (!ui.cb_on) {
                sp_put(v, "NOTHING COPIED YET");
            } else {
                e = ui_l_clip(v, ui.cb_t, ui.cb_s);
                if (ui.cb_bar >= 0)
                    sp_fix(sp_put(e, ", BAR "), ui.cb_bar + 1, 0u, 0);
                else
                    sp_put(e, ", ALL OF IT");
            }
            break;
        case 1:
            lb = ui.cb_on && ui.cb_bar >= 0 ? "PASTE: INTO THE SAME BAR HERE" : "PASTE: INTO THIS CLIP, REPLACING IT";
            ask = ui.paste_t != 0u;
            if (ask)
                sp_put(v, "AGAIN: WHAT IS HERE GOES");
            else if (ui.cb_on)
                ui_l_clip(sp_put(v, "INTO "), t, sc);
            else
                sp_put(v, "\x97");
            break;
        case 2:
            lb = "RIGHT: TWICE AS LONG, LEFT: HALF";
            ask = ui.halve_t != 0u;
            if (ask)
                sp_put(v, "AGAIN: ITS SECOND HALF GOES");
            else if (bars)
                sp_put(sp_fix(v, (int32_t)bars, 0u, 0), bars == 1u ? " BAR" : " BARS");
            else
                sp_put(v, "EMPTY");
            break;
        default:
            lb = drum ? "RIGHT: ITS HITS, LEFT: THE MOTION" : "RIGHT: ITS NOTES, LEFT: THE MOTION";
            ask = ui.clr_t2 != 0u;
            if (ask && ui.clr_dir == 1u)
                sp_put(v, drum ? "AGAIN: ITS HITS GO" : "AGAIN: ITS NOTES GO");
            else if (ask)
                sp_put(sp_put(sp_put(v, "AGAIN: "), nm ? SP[ids[ui.lmv % nm]].name : ""), " GOES");
            else if (nm)
                sp_put(sp_put(v, "MOTION PICKED: "), SP[ids[ui.lmv % nm]].name);
            else
                sp_put(v, nn ? "NO MOTION" : "\x97");
            break;
        }
        lk_label(UI_TITLE_X, y, lb, 20);
        lk_text(UI_TITLE_X, y + 12.0f, T12B, lk_fit(f, sizeof f, v, T12B, 0, 204.0f), ask ? sk.hot : ui_ink(ui_hot(i)),
                sk.ground, 0, 0);
        ui_H(i, 8.0f, y + 15.0f);
    }
}

/* SONG: its sixteen slots (each a scene, its repeats; the one picked framed, the one playing marked; past the first
 * without a scene in LINE), SONG's chip */
static void ui_p_lsong(void)
{
    static const int32_t IDS[3] = {UI_LSLOT, UI_LSSCN, UI_LSREP};
    uint32_t i, n, sl = ui.lslot % LP_NSONG, bars = 0;
    const char *cs = lpp.song_on ? "SONG ON" : "SONG OFF";
    float cx = 230.0f - lk_tw(T10B, cs, 20) - 10.0f;
    char t[40];
    for (n = 0; n < LP_NSONG && lpp.song[n] < LP_NSCN; n++)
        bars += lp_scene_bars(lpp.song[n]) * sy_clampi(lpp.srep[n], 1, 16);
    ui_title("SONG", UI_TITLE_X);
    ui_chip(cx, 24.0f, cs, lpp.song_on, ui_hot(3));
    ui_H(3, cx - 18.0f, 27.0f);
    for (i = 0; i < LP_NSONG; i++) {
        float x = 14.0f + 26.5f * (float)(i % 8u), y = 58.0f + 46.0f * (float)(i / 8u);
        int in = i < n;
        if (i == sl)
            lk_frame(x + 1.0f, y + 1.0f, 22.5f, 32.0f, 2.0f, ui_hot(0) ? sk.hot : sk.ink);
        else
            lk_frame(x + 0.5f, y + 0.5f, 23.5f, 33.0f, 1.0f, in ? sk.mid : sk.line);
        if (in) {
            char l[2] = {ui_l_scn(lpp.song[i]), 0};
            lk_text_c(x + 12.25f, y + 5.0f, T12B, l, i == sl && (ui_hot(1) || ui_hot(0)) ? sk.hot : sk.ink, sk.ground, 0, 0);
            if (lpp.srep[i] > 1u) {
                sp_fix(sp_put(t, "x"), lpp.srep[i], 0u, 0);
                lk_text_c(x + 12.25f, y + 21.0f, T9, t, i == sl && ui_hot(2) ? sk.hot : sk.lab, sk.ground, 0, 0);
            }
        } else if (i == n) {
            lk_text_c(x + 12.25f, y + 12.0f, T9, "END", sk.lab, sk.ground, 0, 0);
        }
        if (lpp.song_on && cht.run && i == lpx.sslot)
            lk_tri(x + 8.75f, y - 7.0f, x + 15.75f, y - 7.0f, x + 12.25f, y - 2.0f, sk.hot);
    }
    if (!lpp.song_on)
        sp_put(t, "OFF: THE SCENES AS LAUNCHED");
    else if (!n)
        sp_put(t, "SLOT 1 HAS NO SCENE");
    else
        sp_put(sp_fix(t, (int32_t)bars, 0u, 0), bars == 1u ? " BAR, ROUND AGAIN" : " BARS, ROUND AGAIN");
    lk_text_c(120.0f, 156.0f, T9, t, sk.lab, sk.ground, 20, 0);
    ui_l_row(0u, IDS);
}

/* PROJECT: the four (the one loaded filled, the one picked framed; each kept, empty, or in the way of old presets);
 * the events in use */
/* the launch pad (SEQ held): the keys, what each does (the white: the tracks' clips in the scene picked, the scenes;
 * the black: MUTE, STOP, SOLO, UNDO), the state of each */
static void ui_p_pad(void)
{
    static const char *const CMD[3] = {"ST", "SO", "UN"};
    uint32_t k;
    lk_kb_t kb;
    char t[40], *e;
    lk_kb(&kb, 8.0f, 52.0f, 224.0f, 112.0f);
    for (k = 0; k < 27u; k++) {
        int32_t r = ui_pad_of(k);
        float cx = lk_kb_cx(&kb, k);
        if (r >= 16) {                              /* (the black keys: their words over them) */
            if (r < 24) {
                sp_fix(t, r - 15, 0u, 0);
                lk_text_c(cx, 34.0f, T9B, t, lpp.trk[r - 16].mute ? sk.hot : sk.lab, sk.ground, 0, 0);
            } else {
                lk_text_c(cx, 34.0f, T9B, CMD[r - 24], r == 25 && lpp.trk[lpp.sel % LP_NTRK].solo ? sk.hot : sk.ink,
                          sk.ground, 0, 0);
            }
            lk_disc(cx, 112.0f, 2.0f, sk.ground);
            continue;
        }
        if (r < 8) {
            uint32_t s = lpp.scn % LP_NSCN;
            sp_fix(t, r + 1, 0u, 0);
            lk_text_c(cx, 168.0f, r == (int32_t)lpp.sel ? T9B : T9, t, r == (int32_t)lpp.sel ? sk.ink : sk.lab, sk.ground,
                      0, 0);
            if (lpr.st != LR_OFF && lpr.trk == (uint32_t)r && lpr.scn == s)
                lk_disc(cx, 150.0f, 3.0f, sk.hot);
            else if (lps[r].play == s && cht.run)
                lk_disc(cx, 150.0f, 3.0f, sk.ink);
            else if (lps[r].next == s)
                lk_ring(cx, 150.0f, 3.0f, 1.5f, sk.hot);
            else if (lpp.clip[r][s].len)
                lk_ring(cx, 150.0f, 2.0f, 1.0f, sk.mid);
        } else {
            char l[2] = {ui_l_scn((uint32_t)r - 8u), 0};
            lk_text_c(cx, 168.0f, (uint32_t)r - 8u == lpp.scn ? T9B : T9, l, (uint32_t)r - 8u == lpp.scn ? sk.ink : sk.lab,
                      sk.ground, 0, 0);
            if ((uint32_t)r - 8u == lpp.scn)
                lk_disc(cx, 150.0f, 3.0f, sk.ink);
        }
    }
    e = ui_l_trk(t, lpp.sel);
    sp_put(sp_put(e, " \xB7 SCENE "), (char[2]){ui_l_scn(lpp.scn), 0});
    lk_text_c(120.0f, 186.0f, T14B, t, sk.ink, sk.ground, 20, 17);
    lk_text_c(120.0f, 206.0f, T9, "1-8 MUTE \xB7 ST STOP \xB7 SO SOLO \xB7 UN UNDO", sk.lab, sk.ground, 20, 0);
}
