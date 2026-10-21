/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* The STRATA UI's sequencer (V0.8; the sequencer itself: sy_seq.c), the SEQ button's menu:
 *   - SEQ: the steps, sixteen at a time (the window holding the step picked, KNOB 1): NOTES (a piano roll: each note
 *     a bar from where NUDGE puts it, as long as LENGTH, as bright as its velocity; NOTE moves the step's notes, from
 *     the lowest a step can go one more: emptied), FEEL (NUDGE's ticks, CHANCE's meters; OCTAVE), MOTION (the lane
 *     picked: its parameter, a step's value as a bar; VALUE turned below the lowest: none there);
 *   - PATTERN: PATTERN (the sixteen, the one shown, the one next, those that hold notes), its LENGTH as cells, a bar
 *     of its steps at RATE, SWING moving the odd ones; PLAY+REC: TRANSPOSE, DIRECTION, RECORD, QUANTIZE;
 *   - CHAIN: its sixteen slots, each a pattern and its repeats, the one playing marked; CHAIN on, off;
 *   - PATTERNS (SEQ held): the keys, the pattern each picks (the white ones), COPY, PASTE and CLEAR (the black ones
 *     at the left).
 * The step picked is sqd.cursor (step recording moves it on); every value is on its page (the one turning HOT: no
 * value in the bottom row). Needs sy_ui.c, sy_ui_draw.c (pg, ui_H, ui_hot, ui_ink, ui_title, UI_TITLE_X),
 * sy_ui_chord.c (ui_chip) and sy_gfx.c. */

/* ------------------------------------------------------------------------------------------ turning --- */
static sq_pat_t *ui_qp(void) { return &sq_pat[sqd.cur % SQ_NPAT]; }
static uint32_t ui_qc(void) { return sqd.cursor % sq_len(ui_qp()); }   /* the step picked, inside LENGTH */
/* a step's notes: their count, the lowest and highest */
static uint32_t ui_q_span(const sq_pat_t *p, uint32_t s, int32_t *lo, int32_t *hi)
{
    uint32_t i, n = 0;
    *lo = 127, *hi = 0;
    for (i = 0; i < SQ_NNOTE; i++)
        if (p->note[s][i] != SQ_NONE) {
            n++;
            *lo = p->note[s][i] < *lo ? p->note[s][i] : *lo;
            *hi = p->note[s][i] > *hi ? p->note[s][i] : *hi;
        }
    return n;
}
static uint32_t ui_q_first(const sq_pat_t *p, uint32_t s)   /* its first note's slot (SQ_NNOTE: none) */
{
    uint32_t i;
    for (i = 0; i < SQ_NNOTE && p->note[s][i] == SQ_NONE; i++)
        ;
    return i;
}
/* a note for an empty step: the nearest one's before it (its first), else C4 */
static uint8_t ui_q_near(const sq_pat_t *p, uint32_t s)
{
    uint32_t j, i;
    for (j = 1; j <= SQ_NSTEP; j++) {
        i = ui_q_first(p, (s + SQ_NSTEP - j) % SQ_NSTEP);
        if (i < SQ_NNOTE)
            return p->note[(s + SQ_NSTEP - j) % SQ_NSTEP][i];
    }
    return 60;
}
/* MOTION's PARAMETER: the next (s > 0) or an earlier one a lane can hold (an effect's: of a module in the chain);
 * before the first, none */
static uint16_t ui_q_param(uint32_t id, int32_t s)
{
    int32_t v = id < SP_COUNT ? (int32_t)id : -1, n = s > 0 ? s : -s, dir = s > 0 ? 1 : -1;
    while (n--) {
        int32_t w = v;
        do
            w += dir;
        while (w >= 0 && w < (int32_t)SP_COUNT &&
               (!sq_lane_ok((uint32_t)w) ||
                (sp_inst_of((uint32_t)w) >= 0 && sp_chain_find(sy_patch.p, (uint32_t)sp_inst_of((uint32_t)w)) < 0)));
        if (w >= (int32_t)SP_COUNT)
            break;
        v = w;
        if (v < 0)
            break;
    }
    return v < 0 ? (uint16_t)SQ_NOID : (uint16_t)v;
}
/* a turn of the sequencer's knobs (sy_ui.c ui_item_turn): the step picked's, the pattern shown's, the chain's; the
 * audio side reads them as they stand (from the next step) */
static void ui_seq_turn(int32_t item, int32_t s, uint32_t e)
{
    sq_pat_t *p = ui_qp();
    uint32_t c = ui_qc(), L = sq_len(p), i, n;
    int32_t lo, hi, d;
    switch (item) {
    case UI_QSTEP: sqd.cursor = (uint8_t)(((int32_t)c + s % (int32_t)L + (int32_t)L) % (int32_t)L); break;
    case UI_QNOTE:                                  /* (the step's notes together; empty: one, as the step before's) */
        n = ui_q_span(p, c, &lo, &hi);
        if (!n) {
            if (s > 0) {
                p->note[c][0] = ui_q_near(p, c);
                p->vel[c][0] = 100;
            }
            break;
        }
        if (lo == 0 && s < 0) {                     /* (below the lowest: emptied) */
            for (i = 0; i < SQ_NNOTE; i++)
                p->note[c][i] = SQ_NONE;
            break;
        }
        d = sy_clampi(ui_accel(e, s, 127), -lo, 127 - hi);
        for (i = 0; i < SQ_NNOTE; i++)
            if (p->note[c][i] != SQ_NONE)
                p->note[c][i] = (uint8_t)(p->note[c][i] + d);
        break;
    case UI_QVEL:
        if ((i = ui_q_first(p, c)) < SQ_NNOTE) {
            uint8_t v = (uint8_t)sy_clampi(p->vel[c][i] + ui_accel(e, s, 126), 1, 127);
            for (i = 0; i < SQ_NNOTE; i++)
                p->vel[c][i] = v;
        }
        break;
    case UI_QLEN: p->len[c] = (uint8_t)sy_clampi(p->len[c] + ui_accel(e, s, 254), 1, 255); break;
    case UI_QCHN: p->chance[c] = (uint8_t)sy_clampi(p->chance[c] + 5 * s, 0, 100); break;
    case UI_QNUD: p->nudge[c] = (int8_t)sy_clampi(p->nudge[c] + ui_accel(e, s, 100), -50, 50); break;
    case UI_QOCT:
        if (ui_q_span(p, c, &lo, &hi) && lo + 12 * s >= 0 && hi + 12 * s <= 127)
            for (i = 0; i < SQ_NNOTE; i++)
                if (p->note[c][i] != SQ_NONE)
                    p->note[c][i] = (uint8_t)(p->note[c][i] + 12 * s);
        break;
    case UI_QLANE: sqd.lane = s > 0; break;
    case UI_QVAL: {                                 /* (none: from the parameter's own value; below 0: none) */
        uint32_t l = sqd.lane % SQ_NLANE, id = p->lane_id[l], x = p->lane[l][c];
        if (!sq_lane_ok(id)) {
            ui_toast("KNOB 4: ITS PARAMETER FIRST");
            return;
        }
        if (x == SQ_NONE)
            x = s > 0 ? sq_lane_x(id, sy_patch.p[id]) : SQ_NONE;
        else
            x = (uint32_t)((d = (int32_t)x + ui_accel(e, s, 254)) < 0 ? SQ_NONE : sy_clampi(d, 0, 254));
        p->lane[l][c] = (uint8_t)x;
        break;
    }
    case UI_QPAR: p->lane_id[sqd.lane % SQ_NLANE] = ui_q_param(p->lane_id[sqd.lane % SQ_NLANE], s); break;
    case UI_QPAT: ui_seq_pick((ui.run && sqd.next < SQ_NPAT ? sqd.next : sqd.cur) + (uint32_t)(s % 16 + 16)); return;
    case UI_QPLEN:
        p->length = (uint8_t)sy_clampi(p->length + ui_accel(e, s, 63), 1, SQ_NSTEP);
        if (sqd.cursor >= p->length)
            sqd.cursor = (uint8_t)(p->length - 1u);
        break;
    case UI_QRATE: p->rate = (uint8_t)sy_clampi(p->rate + s, 0, SQ_NRATE - 1); break;
    case UI_QSWG: p->swing = (uint8_t)sy_clampi(p->swing + s, 50, 75); break;
    case UI_QTRN: p->transpose = (int8_t)sy_clampi(p->transpose + s, -24, 24); break;
    case UI_QDIR: p->dir = (uint8_t)sy_clampi(p->dir + s, 0, SQ_NDIR - 1); break;
    case UI_QRMD: sqd.recmode = s > 0; break;
    case UI_QQNT: sqd.quant = s > 0; break;
    case UI_QSLOT: ui.qslot = (uint8_t)sy_clampi(ui.qslot + s, 0, SQ_NCHAIN - 1); break;
    case UI_QCPAT: {                                /* (END, then the patterns) */
        int32_t v = sqd.cpat[ui.qslot % SQ_NCHAIN] < SQ_NPAT ? sqd.cpat[ui.qslot % SQ_NCHAIN] : -1;
        v = sy_clampi(v + s, -1, SQ_NPAT - 1);
        sqd.cpat[ui.qslot % SQ_NCHAIN] = v < 0 ? SQ_NONE : (uint8_t)v;
        break;
    }
    case UI_QCREP: sqd.crep[ui.qslot % SQ_NCHAIN] = (uint8_t)sy_clampi(sqd.crep[ui.qslot % SQ_NCHAIN] + s, 1, 16); break;
    default: sqd.chain = s > 0; break;
    }
    ui.dirty |= D_KNOBS | D_DISP | D_HEAD;
}

/* PATTERNS (SEQ held): key k's pattern (0 .. 15: the white keys), -2 COPY (F#3), -3 PASTE (G#3), -4 CLEAR (A#3), -1
 * none */
static int32_t ui_seq_key_pat(uint32_t k)
{
    uint32_t i, wi = 0;
    if (k >= 27u)
        return -1;
    if (LK_BLACK[k % 12u])
        return k == 1u ? -2 : k == 3u ? -3 : k == 5u ? -4 : -1;
    for (i = 0; i < k; i++)
        wi += !LK_BLACK[i % 12u];
    return (int32_t)wi;
}
static int ui_seq_key(uint32_t k)                   /* .. its press (1: taken, no note) */
{
    int32_t pt = ui_seq_key_pat(k);
    char t[40], *e;
    ui.bt_chord |= (uint16_t)(1u << B_SEQ);         /* (a key used: no reset at 5 s) */
    ui_cd_stop();
    if (pt >= 0) {
        ui_seq_pick((uint32_t)pt);
        return 1;
    }
    e = sp_fix(sp_put(t, "PATTERN "), (int32_t)sqd.cur % SQ_NPAT + 1, 0u, 0);
    if (pt == -2) {
        sq_copy();
        sp_put(e, " COPIED");
    } else if (pt == -3) {
        if (sq_paste())
            sp_put(e, ": PASTED");
        else
            sp_put(t, "NOTHING COPIED YET");
    } else if (pt == -4) {
        sq_clear();
        sp_put(e, " CLEARED: PASTE UNDOES");
    } else {
        return 1;
    }
    ui_toast(t);
    ui.dirty = D_ALL;
    return 1;
}

/* ------------------------------------------------------------------------------------------- values --- */
static char *ui_q_note(char *b, uint32_t n)         /* a note's name: "C4" (60), "F#-1" */
{
    return sp_fix(sp_put(b, UI_NOTE_NAME[n % 12u]), (int32_t)(n / 12u) - 1, 0u, 0);
}
static char *ui_q_len(char *b, uint32_t q)          /* quarter steps: "1/2 STEP", "1 1/4 STEPS" */
{
    static const char *const F[4] = {"", "1/4", "1/2", "3/4"};
    char *e = b;
    if (q / 4u)
        e = sp_fix(e, (int32_t)(q / 4u), 0u, 0);
    if (q % 4u)
        e = sp_put(sp_put(e, q / 4u ? " " : ""), F[q % 4u]);
    return sp_put(e, q > 4u ? " STEPS" : " STEP");
}
static uint32_t ui_q_nchain(void)                   /* the chain's slots before its first without a pattern */
{
    uint32_t n;
    for (n = 0; n < SQ_NCHAIN && sqd.cpat[n] < SQ_NPAT; n++)
        ;
    return n;
}
/* knob k's name and value on the sequencer's pages (sy_ui_draw.c ui_knob_nv) */
static const char *ui_seq_nv(uint32_t id, char *v)
{
    const sq_pat_t *p = ui_qp();
    uint32_t c = ui_qc(), f = ui_q_first(p, c), l = sqd.lane % SQ_NLANE, lid = p->lane_id[l];
    int32_t lo, hi;
    uint32_t n = ui_q_span(p, c, &lo, &hi);
    v[0] = 0;
    switch (id) {
    case UI_QSTEP: sp_fix(sp_put(sp_fix(v, (int32_t)c + 1, 0u, 0), " OF "), (int32_t)sq_len(p), 0u, 0); return "STEP";
    case UI_QNOTE:
        if (!n)
            sp_put(v, "\x97");
        else if (n == 1u)
            ui_q_note(v, p->note[c][f]);
        else
            sp_fix(sp_put(ui_q_note(v, (uint32_t)lo), " +"), (int32_t)n - 1, 0u, 0);
        return "NOTE";
    case UI_QVEL:
        if (f < SQ_NNOTE)
            sp_fix(v, p->vel[c][f], 0u, 0);
        else
            sp_put(v, "\x97");
        return "VELOCITY";
    case UI_QLEN: ui_q_len(v, p->len[c]); return "LENGTH";
    case UI_QCHN: sp_put(sp_fix(v, p->chance[c], 0u, 0), "%"); return "CHANCE";
    case UI_QNUD: sp_put(sp_fix(v, p->nudge[c], 0u, 1), "%"); return "NUDGE";
    case UI_QOCT:
        if (n)
            sp_fix(sp_put(v, "OCT "), lo / 12 - 1, 0u, 0);
        else
            sp_put(v, "\x97");
        return "OCTAVE";
    case UI_QLANE: sp_fix(v, (int32_t)l + 1, 0u, 0); return "LANE";
    case UI_QVAL:
        if (sq_lane_ok(lid) && p->lane[l][c] != SQ_NONE)
            sp_text(lid, sq_lane_v(lid, p->lane[l][c]), v);
        else
            sp_put(v, "\x97");
        return "VALUE";
    case UI_QPAR:                                   /* (its section: MOTION's line under the title) */
        sp_put(v, sq_lane_ok(lid) ? SP[lid].name : "NONE");
        return "PARAMETER";
    case UI_QPAT: {
        char *e = sp_fix(v, (int32_t)sqd.cur % SQ_NPAT + 1, 0u, 0);
        if (sqd.next < SQ_NPAT)
            sp_fix(sp_put(e, ", NEXT "), (int32_t)sqd.next + 1, 0u, 0);
        return "PATTERN";
    }
    case UI_QPLEN: sp_put(sp_fix(v, (int32_t)sq_len(p), 0u, 0), sq_len(p) == 1u ? " STEP" : " STEPS"); return "LENGTH";
    case UI_QRATE: sp_put(v, NOTE_NAMES[CH_RATE_NOTE[p->rate % SQ_NRATE]]); return "RATE";
    case UI_QSWG: sp_put(sp_fix(v, sy_clampi(p->swing, 50, 75), 0u, 0), "%"); return "SWING";
    case UI_QTRN: sp_fix(v, p->transpose, 0u, 1); return "TRANSPOSE";
    case UI_QDIR: sp_put(v, SQ_DIR_NAME[p->dir % SQ_NDIR]); return "DIRECTION";
    case UI_QRMD: sp_put(v, SQ_RMODE_NAME[sqd.recmode % SQ_NRMODE]); return "RECORD";
    case UI_QQNT: sp_put(v, sqd.quant ? "ON" : "OFF"); return "QUANTIZE";
    case UI_QSLOT: sp_fix(v, (int32_t)ui.qslot % SQ_NCHAIN + 1, 0u, 0); return "SLOT";
    case UI_QCPAT:
        if (sqd.cpat[ui.qslot % SQ_NCHAIN] < SQ_NPAT)
            sp_fix(v, sqd.cpat[ui.qslot % SQ_NCHAIN] + 1, 0u, 0);
        else
            sp_put(v, "END");
        return "PATTERN";
    case UI_QCREP:
        if (sqd.crep[ui.qslot % SQ_NCHAIN] <= 1u)
            sp_put(v, "ONCE");
        else
            sp_put(sp_fix(v, sqd.crep[ui.qslot % SQ_NCHAIN], 0u, 0), " TIMES");
        return "REPEATS";
    case UI_QCHAIN: sp_put(v, sqd.chain ? "ON" : "OFF"); return "CHAIN";
    default: return "";
    }
}

/* ------------------------------------------------------------------------------------------- pages --- */
/* the step pages' sixteen columns across 34 .. 226 (the left: the notes' names, the lanes' marks), rows 64 .. 164 */
#define UI_QX0 34.0f
#define UI_QCW 12.0f
#define UI_QY0 64.0f
#define UI_QY1 164.0f
static float ui_qx(uint32_t i) { return UI_QX0 + UI_QCW * (float)i; }
static int32_t ui_q_play(void)                      /* the step playing in the pattern shown (-1 none) */
{
    return sqs.run && sqs.at && sqs.spat == sqd.cur % SQ_NPAT ? (int32_t)sqs.at - 1 : -1;
}
/* a page's three knobs in a row under its graph (knobs k0 .. k0 + 2: a word over the value each) */
static void ui_q_row(uint32_t k0, const int32_t *ids)
{
    static const float CX[3] = {26.0f, 104.0f, 182.0f};
    char v[40], f[40];
    uint32_t i;
    for (i = 0; i < 3u; i++) {
        const char *nm = ui_seq_nv((uint32_t)ids[i], v);
        lk_text(CX[i], 176.0f, T9, nm, sk.lab, sk.ground, 20, 0);
        lk_text(CX[i], 188.0f, T10B, lk_fit(f, sizeof f, v, T10B, 0, i < 2u ? 74.0f : 50.0f), ui_ink(ui_hot(k0 + i)),
                sk.ground, 0, 0);
        ui_H(k0 + i, CX[i] - 18.0f, 185.0f);
    }
    pg.figs = 0xFu;
}
/* the step pages' frame: "STEP 5" (KNOB 1), the line under it, the window's place among the pattern's (four boxes
 * at the right: the one shown filled, those past LENGTH in LINE, the one playing in marked under), the columns' beat
 * ticks, LENGTH's end dashed, the step picked framed, the one playing marked over it; the window's first step */
static uint32_t ui_q_frame(const sq_pat_t *p, const char *sub, int subhot)
{
    uint32_t c = ui_qc(), w0 = c / 16u * 16u, L = sq_len(p), i;
    int32_t pl = ui_q_play();
    char t[16];
    sp_fix(sp_put(t, "STEP "), (int32_t)c + 1, 0u, 0);
    lk_text(UI_TITLE_X, 24.0f, T14B, t, ui_ink(ui_hot(0)), sk.ground, 0, 17);
    ui_H(0, 8.0f, 27.0f);
    if (sub)
        lk_text(UI_TITLE_X, 42.0f, T10, sub, subhot ? sk.hot : sk.lab, sk.ground, 20, 12);
    for (i = 0; i < 4u; i++) {
        float x = 174.0f + 14.0f * (float)i;
        if (i == w0 / 16u)
            lk_box(x, 26.0f, x + 11.0f, 35.0f, ui_ink(ui_hot(0)));
        else
            lk_frame(x + 0.5f, 26.5f, 10.0f, 8.0f, 1.0f, i * 16u < L ? sk.mid : sk.line);
        if (pl >= 0 && (uint32_t)pl / 16u == i)
            lk_box(x + 3.0f, 38.0f, x + 8.0f, 40.0f, sk.hot);
    }
    for (i = 0; i <= 16u; i++)
        lk_seg(ui_qx(i), UI_QY1 + 1.0f, ui_qx(i), UI_QY1 + ((w0 + i) % 4u ? 3.0f : 6.0f), 1.0f,
               (w0 + i) % 4u ? sk.line : sk.mid);
    if (L > w0 && L < w0 + 16u)
        lk_seg_dash(ui_qx(L - w0), UI_QY0, ui_qx(L - w0), UI_QY1, sk.mid, 1.0f, 2.0f);
    lk_frame(ui_qx(c - w0) + 0.5f, UI_QY0 + 0.5f, UI_QCW - 1.0f, UI_QY1 - UI_QY0 - 1.0f, sqd.steprec ? 2.0f : 1.0f,
             ui_hot(0) || sqd.steprec ? sk.hot : sk.ink);   /* (step recording: the step the keys fill, HOT) */
    if (pl >= (int32_t)w0 && pl < (int32_t)w0 + 16) {
        float cx = ui_qx((uint32_t)pl - w0) + 0.5f * UI_QCW;
        lk_tri(cx - 3.5f, UI_QY0 - 8.0f, cx + 3.5f, UI_QY0 - 8.0f, cx, UI_QY0 - 3.0f, sk.hot);
    }
    return w0;
}
/* "16 STEPS OF 1/16"; step recording, "REC: THE KEYS FILL STEP 5" */
static char *ui_q_sub(char *t, const sq_pat_t *p)
{
    if (sqd.steprec)
        sp_fix(sp_put(t, "REC: THE KEYS FILL STEP "), (int32_t)ui_qc() + 1, 0u, 0);
    else
        sp_put(sp_put(sp_fix(t, (int32_t)sq_len(p), 0u, 0), sq_len(p) == 1u ? " STEP OF " : " STEPS OF "),
               NOTE_NAMES[CH_RATE_NOTE[p->rate % SQ_NRATE]]);
    return t;
}

/* NOTES: the window's notes as a piano roll (the range they span, an octave at least; the Cs named at the left) */
static void ui_p_qnotes(void)
{
    static const int32_t IDS[3] = {UI_QNOTE, UI_QVEL, UI_QLEN};
    const sq_pat_t *p = ui_qp();
    uint32_t c = ui_qc(), L = sq_len(p), w0, i, k;
    int32_t lo = 127, hi = 0, r, x;
    float h;
    char t[32];
    w0 = ui_q_frame(p, ui_q_sub(t, p), sqd.steprec);
    for (i = w0; i < w0 + 16u; i++)
        for (k = 0; k < SQ_NNOTE; k++)
            if (p->note[i][k] != SQ_NONE) {
                x = sy_clampi(p->note[i][k] + p->transpose, 0, 127);
                lo = x < lo ? x : lo;
                hi = x > hi ? x : hi;
            }
    if (lo > hi)
        lo = 60, hi = 71;
    if (hi - lo < 11) {                             /* (an octave at least, round the notes) */
        lo -= (11 - (hi - lo)) / 2;
        hi = lo + 11;
    }
    if (lo < 0)
        hi -= lo, lo = 0;
    if (hi > 127)
        lo -= hi - 127, hi = 127;
    r = hi - lo + 1;
    h = (UI_QY1 - UI_QY0 - 2.0f) / (float)r;
    for (x = lo; x <= hi; x++)                      /* (the Cs: a line, the name at the left) */
        if (x % 12 == 0) {
            float y = UI_QY1 - 1.0f - h * ((float)(x - lo) + 0.5f);
            lk_seg_dash(UI_QX0, y, UI_QX0 + 16.0f * UI_QCW, y, sk.line, 1.0f, 3.0f);
            ui_q_note(t, (uint32_t)x);
            lk_text_r(UI_QX0 - 3.0f, y - 5.0f, T9, t, sk.lab, sk.ground, 0, 0);
        }
    for (i = w0; i < w0 + 16u; i++)
        for (k = 0; k < SQ_NNOTE; k++) {
            uint32_t n = p->note[i][k];
            float xa, xb, y;
            uint16_t col;
            if (n == SQ_NONE)
                continue;
            x = sy_clampi((int32_t)n + p->transpose, 0, 127);
            y = UI_QY1 - 1.0f - h * ((float)(x - lo) + 0.5f);
            xa = ui_qx(i - w0) + 1.0f + UI_QCW * (float)p->nudge[i] * 0.01f;
            xb = sy_minf(xa + sy_maxf(UI_QCW * (float)p->len[i] * 0.25f - 2.0f, 2.0f), UI_QX0 + 16.0f * UI_QCW);
            xa = sy_maxf(xa, UI_QX0);
            col = i >= L ? sk.line
                  : i == c && (ui_hot(1) || ui_hot(2) || ui_hot(3)) ? sk.hot
                  : sy_mix565(sk.mid, sk.ink, (uint32_t)p->vel[i][k] * 2u);
            lk_box(xa, y - sy_maxf(0.5f * h - 0.75f, 1.0f), xb, y + sy_maxf(0.5f * h - 0.75f, 1.0f), col);
        }
    ui_q_row(1u, IDS);
}

/* FEEL: NUDGE (each step's notes as a tick where it puts them, round the column's middle) over CHANCE (a meter a
 * step) */
static void ui_p_qfeel(void)
{
    static const int32_t IDS[3] = {UI_QCHN, UI_QNUD, UI_QOCT};
    const sq_pat_t *p = ui_qp();
    uint32_t c = ui_qc(), L = sq_len(p), w0, i;
    char t[32];
    w0 = ui_q_frame(p, ui_q_sub(t, p), sqd.steprec);
    lk_arrow(16.0f, 82.0f, 28.0f, 82.0f, 3.0f, 1.5f, sk.lab);   /* (the lanes' marks: earlier, later; %) */
    lk_arrow(28.0f, 82.0f, 16.0f, 82.0f, 3.0f, 1.5f, sk.lab);
    lk_text_c(22.0f, 128.0f, T10B, "%", sk.lab, sk.ground, 0, 0);
    lk_seg(UI_QX0, 108.0f, UI_QX0 + 16.0f * UI_QCW, 108.0f, 1.0f, sk.line);
    for (i = w0; i < w0 + 16u; i++) {
        float x0 = ui_qx(i - w0), cx = x0 + 0.5f * UI_QCW, nx = cx + UI_QCW * (float)p->nudge[i] * 0.01f;
        int on = sq_step_used(p, i), sel = i == c;
        uint16_t ink = i >= L ? sk.line : sk.ink;
        lk_seg(cx, 74.0f, cx, 90.0f, 1.0f, sk.line);
        if (on) {
            lk_box(nx - 1.5f, 70.0f, nx + 1.5f, 94.0f, sel && ui_hot(2) ? sk.hot : ink);
            lk_box(x0 + 2.0f, 156.0f - 40.0f * (float)p->chance[i] / 100.0f, x0 + UI_QCW - 2.0f, 156.0f,
                   sel && ui_hot(1) ? sk.hot : ink);
        } else {
            lk_box(x0 + 3.0f, 153.0f, x0 + UI_QCW - 3.0f, 156.0f, sk.line);
        }
    }
    ui_q_row(1u, IDS);
}

/* MOTION: the lane picked, its values a bar a step (none: a stub), its parameter under the title */
static void ui_p_qmotion(void)
{
    static const int32_t IDS[3] = {UI_QLANE, UI_QVAL, UI_QPAR};
    const sq_pat_t *p = ui_qp();
    uint32_t c = ui_qc(), L = sq_len(p), l = sqd.lane % SQ_NLANE, id = p->lane_id[l], w0, i;
    char t[40], *e = sp_fix(sp_put(t, "LANE "), (int32_t)l + 1, 0u, 0);
    if (sq_lane_ok(id))
        sp_put(sp_put(sp_put(sp_put(e, ": "), SEC_SHORT[SP[id].sec]), " "), SP[id].name);
    else
        sp_put(e, ": NO PARAMETER YET");
    w0 = ui_q_frame(p, t, 0);
    for (i = w0; i < w0 + 16u; i++) {
        float x0 = ui_qx(i - w0);
        uint32_t x = p->lane[l][i];
        if (sq_lane_ok(id) && x != SQ_NONE)
            lk_box(x0 + 2.0f, 160.0f - 90.0f * (float)x / 254.0f - 1.0f, x0 + UI_QCW - 2.0f, 160.0f,
                   i >= L ? sk.line : i == c && ui_hot(2) ? sk.hot : sk.ink);
        else
            lk_box(x0 + 3.0f, 157.0f, x0 + UI_QCW - 3.0f, 160.0f, sk.line);
    }
    ui_q_row(1u, IDS);
}

/* PATTERN: the sixteen (the one shown filled, the one next HOT, those holding notes framed), LENGTH's cells (a step
 * holding notes in INK, an empty one in MID), a bar of the steps at RATE (SWING moving the odd ones) */
static void ui_p_qpat(void)
{
    static const int32_t IDS[3] = {UI_QPLEN, UI_QRATE, UI_QSWG};
    const sq_pat_t *p = ui_qp();
    uint32_t i, L = sq_len(p), cur = sqd.cur % SQ_NPAT, nb;
    float q = sq_q(p), bar = sy_bar_q > 0.0f ? sy_bar_q : 4.0f;
    const char *st = sqd.next < SQ_NPAT ? "NEXT UP" : sqs.run ? "PLAYING" : "STOPPED";
    char t[16];
    sp_fix(sp_put(t, "PATTERN "), (int32_t)cur + 1, 0u, 0);
    lk_text(UI_TITLE_X, 24.0f, T14B, t, ui_ink(ui_hot(0)), sk.ground, 0, 17);
    ui_H(0, 8.0f, 27.0f);
    lk_chip(230.0f - lk_tw(T10B, st, 20) - 10.0f, 24.0f, st, sqs.run != 0);
    for (i = 0; i < SQ_NPAT; i++) {                 /* the sixteen */
        float x = 14.0f + 27.0f * (float)(i % 4u), y = 50.0f + 27.0f * (float)(i / 4u);
        int used = sq_pat_used(&sq_pat[i]);
        sp_fix(t, (int32_t)i + 1, 0u, 0);
        if (i == cur) {
            lk_box(x, y, x + 24.0f, y + 24.0f, ui_ink(ui_hot(0)));
            lk_text_c(x + 12.0f, y + 6.0f, T10B, t, sk.ground, ui_ink(ui_hot(0)), 0, 0);
        } else {
            lk_frame(x + 0.5f, y + 0.5f, 23.0f, 23.0f, i == sqd.next ? 2.0f : 1.0f,
                     i == sqd.next ? sk.hot : used ? sk.ink : sk.line);
            lk_text_c(x + 12.0f, y + 6.0f, T10B, t, used ? sk.ink : sk.lab, sk.ground, 0, 0);
        }
    }
    lk_label(130.0f, 50.0f, "LENGTH", 20);          /* LENGTH: 64 cells */
    for (i = 0; i < SQ_NSTEP; i++) {
        float x = 130.0f + 6.25f * (float)(i % 16u), y = 64.0f + 10.0f * (float)(i / 16u);
        if (i < L)
            lk_box(x, y, x + 5.0f, y + 7.0f, ui_hot(1) ? sk.hot : sq_step_used(p, i) ? sk.ink : sk.mid);
        else
            lk_frame(x + 0.5f, y + 0.5f, 4.0f, 6.0f, 1.0f, sk.line);
    }
    lk_label(130.0f, 112.0f, "A BAR", 20);          /* a bar of steps at RATE (32 at most), the odd ones SWING's */
    nb = (uint32_t)sy_clampf(bar / q + 0.5f, 1.0f, 32.0f);
    lk_seg(130.0f, 146.0f, 230.0f, 146.0f, 1.0f, sk.line);
    for (i = 0; i < nb; i++) {
        float x = 130.0f + 100.0f * ((float)i + sq_swing(p, i)) / (float)nb;
        lk_seg(x, 128.0f, x, 146.0f, nb > 16u ? 1.0f : 2.0f, (i & 1u) && p->swing > 50u ? ui_ink(ui_hot(3))
                                                            : ui_ink(ui_hot(2)));
    }
    ui_q_row(1u, IDS);
}

/* PLAY+REC: TRANSPOSE (the title, a ruler -24 .. +24), DIRECTION's, RECORD's and QUANTIZE's chips */
static void ui_p_qplay(void)
{
    static const char *const DIR[SQ_NDIR] = {"FWD", "BACK", "PING", "RANDOM"};
    const sq_pat_t *p = ui_qp();
    uint32_t i;
    float x;
    char t[24];
    pg.figs = 0xFu;
    sp_fix(sp_put(t, "TRANSPOSE "), p->transpose, 0u, 1);
    lk_text(UI_TITLE_X, 24.0f, T14B, t, ui_ink(ui_hot(0)), sk.ground, 0, 17);
    for (i = 0; i <= 48u; i++) {                    /* (a tick a semitone, the octaves longer) */
        float xt = 34.0f + 4.0f * (float)i;
        lk_seg(xt, i % 12u ? 50.0f : 46.0f, xt, 56.0f, 1.0f, i == 24u ? sk.mid : i % 12u ? sk.line : sk.mid);
    }
    lk_disc(34.0f + 4.0f * (float)(p->transpose + 24), 53.0f, 3.2f, ui_ink(ui_hot(0)));
    lk_label(UI_TITLE_X, 70.0f, "DIRECTION", 20);
    for (i = 0, x = UI_TITLE_X; i < SQ_NDIR; i++)
        x = ui_chip(x, 84.0f, DIR[i], i == p->dir % SQ_NDIR, ui_hot(1));
    lk_label(UI_TITLE_X, 110.0f, "RECORD", 20);
    for (i = 0, x = UI_TITLE_X; i < SQ_NRMODE; i++)
        x = ui_chip(x, 124.0f, SQ_RMODE_NAME[i], i == sqd.recmode % SQ_NRMODE, ui_hot(2));
    lk_label(UI_TITLE_X, 150.0f, "QUANTIZE", 20);
    x = ui_chip(UI_TITLE_X, 164.0f, "OFF", !sqd.quant, ui_hot(3));
    ui_chip(x, 164.0f, "ON", sqd.quant, ui_hot(3));
    lk_text(UI_TITLE_X, 190.0f, T9, sqd.quant ? "NOTES ON THE STEPS" : "NUDGE KEEPS WHERE THEY CAME", sk.lab, sk.ground,
            20, 0);
    ui_H(0, 8.0f, 27.0f);
    ui_H(1, 8.0f, 87.0f);
    ui_H(2, 8.0f, 127.0f);
    ui_H(3, 8.0f, 167.0f);
}

/* CHAIN: its sixteen slots (each its pattern, its repeats; the slot picked framed, the one playing marked over it;
 * past the first without a pattern in LINE), what it plays in words; CHAIN's chip */
static void ui_p_qchain(void)
{
    static const int32_t IDS[3] = {UI_QSLOT, UI_QCPAT, UI_QCREP};
    uint32_t i, n = ui_q_nchain(), sl = ui.qslot % SQ_NCHAIN;
    const char *cs = sqd.chain ? "CHAIN ON" : "CHAIN OFF";
    float cx = 230.0f - lk_tw(T10B, cs, 20) - 10.0f;
    char t[40];
    ui_title("CHAIN", UI_TITLE_X);
    ui_chip(cx, 24.0f, cs, sqd.chain, ui_hot(3));
    ui_H(3, cx - 18.0f, 27.0f);
    for (i = 0; i < SQ_NCHAIN; i++) {
        float x = 14.0f + 26.5f * (float)(i % 8u), y = 58.0f + 46.0f * (float)(i / 8u);
        int in = i < n;
        if (i == sl)
            lk_frame(x + 1.0f, y + 1.0f, 22.5f, 32.0f, 2.0f, ui_hot(0) ? sk.hot : sk.ink);
        else
            lk_frame(x + 0.5f, y + 0.5f, 23.5f, 33.0f, 1.0f, in ? sk.mid : sk.line);
        if (in) {
            sp_fix(t, sqd.cpat[i] + 1, 0u, 0);
            lk_text_c(x + 12.25f, y + 5.0f, T12B, t, i == sl && (ui_hot(1) || ui_hot(0)) ? sk.hot : sk.ink, sk.ground,
                      0, 0);
            if (sqd.crep[i] > 1u) {
                sp_fix(sp_put(t, "x"), sqd.crep[i], 0u, 0);
                lk_text_c(x + 12.25f, y + 21.0f, T9, t, i == sl && ui_hot(2) ? sk.hot : sk.lab, sk.ground, 0, 0);
            }
        } else if (i == n) {
            lk_text_c(x + 12.25f, y + 12.0f, T9, "END", sk.lab, sk.ground, 0, 0);
        }
        if (sqd.chain && sqs.run && i == sqs.cslot)
            lk_tri(x + 8.75f, y - 7.0f, x + 15.75f, y - 7.0f, x + 12.25f, y - 2.0f, sk.hot);
    }
    if (!sqd.chain)
        sp_put(sp_fix(sp_put(t, "OFF: PATTERN "), (int32_t)sqd.cur % SQ_NPAT + 1, 0u, 0), " LOOPS");
    else if (!n)
        sp_put(t, "SLOT 1 HAS NO PATTERN");
    else
        sp_put(sp_fix(sp_put(t, "THE "), (int32_t)n, 0u, 0), n == 1u ? " SLOT, ROUND AGAIN" : " SLOTS, ROUND AGAIN");
    lk_text_c(120.0f, 156.0f, T9, t, sk.lab, sk.ground, 20, 0);
    ui_q_row(0u, IDS);
}

/* PATTERNS (SEQ held): the keys, the pattern each white one picks (the one shown filled, the one next HOT, those
 * holding notes ringed), COPY PASTE CLEAR over the black ones at the left; the pattern shown, the copy's state */
static void ui_p_patterns(void)
{
    static const char *const CMD[3] = {"CP", "PA", "CL"};
    uint32_t k;
    lk_kb_t kb;
    char t[40], *e;
    lk_kb(&kb, 8.0f, 52.0f, 224.0f, 112.0f);
    for (k = 0; k < 27u; k++) {
        int32_t pt = ui_seq_key_pat(k);
        float cx = lk_kb_cx(&kb, k);
        if (pt <= -2) {
            lk_text_c(cx, 34.0f, T9B, CMD[-pt - 2], sk.ink, sk.ground, 0, 0);
            lk_disc(cx, 112.0f, 2.0f, sk.ground);
            continue;
        }
        if (pt < 0)
            continue;
        sp_fix(t, pt + 1, 0u, 0);
        lk_text_c(cx, 168.0f, pt == (int32_t)sqd.cur ? T9B : T9, t, pt == (int32_t)sqd.cur ? sk.ink : sk.lab, sk.ground,
                  0, 0);
        if (pt == (int32_t)sqd.cur)
            lk_disc(cx, 150.0f, 3.0f, sk.ink);
        else if (pt == (int32_t)sqd.next)
            lk_ring(cx, 150.0f, 3.0f, 1.5f, sk.hot);
        else if (sq_pat_used(&sq_pat[pt]))
            lk_ring(cx, 150.0f, 2.0f, 1.0f, sk.mid);
    }
    e = sp_fix(sp_put(t, "PATTERN "), (int32_t)sqd.cur % SQ_NPAT + 1, 0u, 0);
    if (sqd.next < SQ_NPAT)
        sp_fix(sp_put(e, ", NEXT "), (int32_t)sqd.next + 1, 0u, 0);
    lk_text_c(120.0f, 186.0f, T14B, t, sk.ink, sk.ground, 20, 17);
    lk_text_c(120.0f, 206.0f, T9, sq_clip_ok ? "CP COPY \xB7 PA PASTE \xB7 CL CLEAR" : "CP COPY \xB7 CL CLEAR (PASTE UNDOES)",
              sk.lab, sk.ground, 20, 0);
}
