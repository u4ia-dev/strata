/* SPDX-License-Identifier: GPL-3.0-only */
/* The STRATA UI's chord pages (V0.7, SCL's family after SCALE; the chords themselves: sy_chord.c):
 *   - CHORD: the chord playing named large ("Cm7/G"), a row a knob: MODE's chips, TYPE's (KEY: SHAPE's) strip from
 *     the one before the current (its place in the list at the right), VOICING's AUTO chip and ruler, SPREAD's chips;
 *     the chord's notes and the bass on three octaves of keys. The current choice in each row HOT while its knob
 *     turns, no flag (V0.75);
 *   - STRUM (V0.75): the chord as STRUM starts it, a row a note, each from its start, as loud as TAPER and HUMAN
 *     leave it;
 *   - PERFORM (V0.75: ARP held, ARP's family): the style, the ARP button's state, a bar of what it plays (each note
 *     a row, its time across: a strum's stagger, an arpeggio's steps, a pattern's stabs; HARP: the note each key
 *     plays; the step playing marked under it), RATE, MOVE and FEEL; STEPS, RATCHET and GROOVE (V0.75, below);
 *   - BASS: on or off, its octave, the chord and the bass on four octaves of keys, its level;
 *   - MIDI: where the chords come from and go (the keys, MIDI in, the sound, USB out), the two channels.
 * Each draws from chd (the settings) and chs (the chord as the audio side last built it), read as they stand. Needs
 * sy_ui.c, sy_ui_draw.c (pg, ui_H, ui_hot, ui_title, ui_sub) and sy_gfx.c. */

/* a chip (sy_gfx.c lk_chip) whose current one is HOT while its knob turns (V0.75: the value turning shown in place) */
static float ui_chip(float x, float y, const char *s, int cur, int hot)
{
    float w = lk_tw(T10B, s, 20) + 10.0f;
    if (lk_vis(x, y, x + w, y + LK_CHIP_H)) {
        uint16_t bg = cur ? (hot ? sk.hot : sk.ink) : sk.ground;
        if (cur)
            lk_box(x, y, x + w, y + LK_CHIP_H, bg);
        else
            lk_frame(x + 0.5f, y + 0.5f, w - 1.0f, LK_CHIP_H - 1.0f, 1.0f, sk.mid);
        lk_text(x + 5.0f, y + 2.0f, T10B, s, cur ? sk.ground : sk.lab, bg, 20, 12);
    }
    return x + w + 4.0f;
}
/* knob k's name and value on SCL's chord pages (sy_ui_draw.c ui_knob_nv) */
static const char *ui_chord_nv(uint32_t id, char *v)
{
    uint32_t st = chd.style % PF_N, a = ui.astep % AR_NSTEP;
    v[0] = 0;
    switch (id) {
    case UI_HMODE: sp_put(v, CH_MODE_NAME[chd.mode % CH_NMODE]); return "MODE";
    case UI_HTYPE:
        if (chd.mode == CH_KEY) {
            sp_put(v, CH_KT_NAME[chd.ktype % CH_KT_N]);
            return "SHAPE";
        }
        sp_put(v, CH_FT[chd.ftype % CH_FT_N].name);
        return "TYPE";
    case UI_HVOI:
        if (!chd.voicing)
            sp_put(v, "AUTO");
        else
            sp_fix(v, (int32_t)chd.voicing - 4, 0u, 1);
        return "VOICING";
    case UI_HSPR: sp_put(v, CH_SPREAD_NAME[chd.spread % CH_NSPREAD]); return "SPREAD";
    case UI_PSTY: sp_put(v, PF_NAME[st]); return "STYLE";
    case UI_PRATE:
        if (st == PF_HARP)
            sp_put(v, "\x97");
        else if (chd.rate[st] < 7u)
            sp_put(sp_fix(v, CH_RATE_MS[chd.rate[st]], 0u, 0), " MS");
        else
            sp_put(v, NOTE_NAMES[CH_RATE_NOTE[(chd.rate[st] - 7u) % (CH_NRATE - 7u)]]);
        return "RATE";
    case UI_PMOVE:
        if (st == PF_ARP)
            sp_put(v, AR_NAME[chd.move[st] % AR_N]);
        else if (st == PF_PATTERN)
            sp_fix(v, (int32_t)chd.move[st] % 16 + 1, 0u, 0);
        else
            sp_put(v, "\x97");
        return ch_move_name(st);
    case UI_PFEEL:
        if (st == PF_ARP)
            sp_put(sp_fix(v, chd.feel[st], 0u, 0), " OCT");
        else if (st == PF_HARP)
            sp_put(v, "\x97");
        else
            sp_put(sp_fix(v, chd.feel[st], 0u, 0), "%");
        return ch_feel_name(st);
    case UI_BON: sp_put(v, chd.bass ? "ON" : "OFF"); return "BASS";
    case UI_BOCT: sp_put(sp_fix(v, -(int32_t)chd.boct, 0u, 1), chd.boct == 1u ? " OCTAVE" : " OCTAVES"); return "OCTAVE";
    case UI_BLVL: sp_put(sp_fix(v, chd.blvl, 0u, 0), "%"); return "LEVEL";
    case UI_AGATE: sp_put(sp_fix(v, chd.gate, 0u, 0), "%"); return "GATE";
    case UI_ASWING: sp_put(sp_fix(v, chd.swing, 0u, 0), "%"); return "SWING";
    case UI_ALEN: sp_put(sp_fix(v, chd.len, 0u, 0), chd.len == 1u ? " STEP" : " STEPS"); return "LENGTH";
    case UI_ALATCH: sp_put(v, chd.latch ? "ON" : "OFF"); return "LATCH";
    case UI_SSTEP: sp_fix(sp_put(sp_fix(v, (int32_t)a + 1, 0u, 0), " OF "), chd.len, 0u, 0); return "STEP";
    case UI_STYPE: sp_put(v, AS_NAME[chd.st[a] % AS_N]); return "KIND";
    case UI_SLVL: sp_put(sp_fix(v, chd.lvl[a], 0u, 0), "%"); return "LEVEL";
    case UI_SOCT:
        if (chd.oct[a] == 2u)
            sp_put(v, "AS PLAYED");
        else
            sp_put(sp_fix(v, (int32_t)chd.oct[a] - 2, 0u, 1), " OCT");
        return "OCTAVE";
    case UI_SRAT: sp_put(sp_fix(v, chd.rat[a], 0u, 0), chd.rat[a] == 1u ? " HIT" : " HITS"); return "RATCHET";
    case UI_SCHN: sp_put(sp_fix(v, chd.chn[a], 0u, 0), "%"); return "CHANCE";
    case UI_SSLD: sp_put(v, chd.sld[a] ? "ON" : "OFF"); return "SLIDE";
    case UI_STRUM:
        if (!chd.strum)
            sp_put(v, "OFF");
        else
            sp_put(sp_fix(v, CH_STRUM_MS[chd.strum % CH_NSTRUM], 0u, 0), " MS");
        return "STRUM";
    case UI_SDIR: sp_put(v, CH_SDIR_NAME[chd.sdir % CH_NSDIR]); return "DIRECTION";
    case UI_SHUM: sp_put(sp_fix(v, chd.human, 0u, 0), "%"); return "HUMAN";
    case UI_STAP: sp_put(sp_fix(v, chd.taper, 0u, 0), "%"); return "TAPER";
    default: return "";
    }
}

/* a keyboard of oct octaves from note lo (a C) across w from x0, h high: a dot on each of the chord's notes (on a
 * black key in GROUND), the bass's HOT (bass 0xFF: none) */
static float ui_ch_kx(float x0, float ww, int32_t lo, uint32_t note, int *black)
{
    static const int8_t WI[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    int32_t r = (int32_t)note - lo, o = r / 12, pc = r % 12;
    *black = WI[pc] < 0;
    return *black ? x0 + (float)(o * 7 + WI[pc - 1] + 1) * ww : x0 + ((float)(o * 7 + WI[pc]) + 0.5f) * ww;
}
static void ui_ch_kb(float x0, float y0, float w, float h, int32_t lo, uint32_t oct, const uint8_t *nt, uint32_t n,
                     uint32_t bass, int hot)
{
    static const uint8_t BK[5] = {1, 3, 6, 8, 10};
    float ww = w / (float)(7u * oct), bw = ww * 0.62f, bh = h * 0.6f;
    uint32_t i, o;
    int black;
    if (!lk_vis(x0 - 1.0f, y0 - 1.0f, x0 + w + 1.0f, y0 + h + 1.0f))
        return;
    for (i = 0; i < 7u * oct; i++)
        lk_frame(x0 + (float)i * ww, y0, ww, h, 1.0f, sk.ink);
    for (o = 0; o < oct; o++)
        for (i = 0; i < 5u; i++) {
            float x = ui_ch_kx(x0, ww, lo, (uint32_t)lo + 12u * o + BK[i], &black);
            lk_box(x - 0.5f * bw - 0.5f, y0 - 0.5f, x + 0.5f * bw + 0.5f, y0 + bh + 0.5f, sk.ink);
        }
    for (i = 0; i <= n; i++) {                      /* (the chord's notes, then the bass) */
        uint32_t x = i < n ? nt[i] : bass;
        float cx;
        if (x > 127u || (int32_t)x < lo || (int32_t)x >= lo + 12 * (int32_t)oct)
            continue;
        cx = ui_ch_kx(x0, ww, lo, x, &black);
        if (i == n)
            lk_disc(cx, black ? y0 + bh - 6.0f : y0 + h - 7.0f, 3.2f, sk.hot);
        else
            lk_disc(cx, black ? y0 + bh - 6.0f : y0 + h - 7.0f, 2.6f, black ? sk.ground : ui_ink(hot));
    }
}
/* the lowest C at or under a chord's notes and its bass, for a keyboard of oct octaves */
static int32_t ui_ch_lo(const uint8_t *nt, uint32_t n, uint32_t bass, uint32_t oct)
{
    int32_t lo = 127, hi = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        lo = nt[i] < lo ? nt[i] : lo, hi = nt[i] > hi ? nt[i] : hi;
    if (bass <= 127u)
        lo = (int32_t)bass < lo ? (int32_t)bass : lo;
    if (!n)
        return 48;
    lo -= lo % 12;
    if (hi >= lo + 12 * (int32_t)oct && (hi - 12 * (int32_t)oct + 12) / 12 * 12 > lo)   /* (too wide: its top) */
        lo = (hi - 12 * (int32_t)oct + 12) / 12 * 12;
    return lo;
}
/* the chord to show: the one built (a C major one on C4 before any) */
static uint32_t ui_ch_now(uint8_t *nt, uint32_t *bass, uint32_t *root)
{
    uint32_t n = chs.n;
    if (!chs.built || !n) {
        static const uint8_t C[3] = {60, 64, 67};
        memcpy(nt, C, 3u);
        *bass = chd.bass ? 48u - 12u * (chd.boct - 1u) : 0xFFu;
        *root = 60u;
        return 3u;
    }
    memcpy(nt, chs.note, n);
    *bass = chd.bass ? chs.bass : 0xFFu;
    *root = chs.root;
    return n;
}

/* CHORD: the chord named large, MODE's, TYPE's and the extensions' chips, VOICING, the notes on the keys */
static void ui_p_chord(void)
{
    uint8_t nt[CH_MAXN];
    uint32_t bass, root, n = ui_ch_now(nt, &bass, &root), i, key = chd.mode == CH_KEY, on = chd.mode != CH_OFF;
    uint32_t cnt = key ? CH_KT_N : CH_FT_N, cur = key ? chd.ktype % CH_KT_N : chd.ftype % CH_FT_N, first;
    char t[24];
    float x;
    pg.figs = 0xFu;                                 /* (every value is on the page: the one turning HOT, no flag) */
    if (!on && !chd.perf && !chd.bass) {
        lk_text(26.0f, 22.0f, T26B, "OFF", sk.lab, sk.ground, -20, 28);
    } else if (!chs.built || !chs.n) {
        lk_text(26.0f, 22.0f, T26B, "\x97", sk.lab, sk.ground, -20, 28);
    } else {
        ch_name_of(t, nt, n, root);
        lk_text(26.0f, 22.0f, T26B, t, sk.ink, sk.ground, -20, 28);
    }
    x = 26.0f;                                      /* MODE */
    for (i = 0; i < CH_NMODE; i++)
        x = ui_chip(x, 58.0f, CH_MODE_NAME[i], i == chd.mode, ui_hot(0));
    first = cur ? cur - 1u : 0u;                    /* TYPE (KEY: SHAPE): a strip from the one before the current, */
    x = 26.0f;                                      /* its place in the list at the right */
    for (i = first; i < cnt; i++) {
        const char *s = key ? CH_KT_NAME[i] : CH_FT[i].name;
        if (x + lk_tw(T10B, s, 20) + 10.0f > 196.0f)
            break;
        x = ui_chip(x, 80.0f, s, on && i == cur, ui_hot(1));
    }
    sp_fix(sp_put(sp_fix(t, (int32_t)cur + 1, 0u, 0), "/"), (int32_t)cnt, 0u, 0);
    lk_text_r(232.0f, 82.0f, T9, t, sk.lab, sk.ground, 0, 0);
    {                                               /* VOICING: AUTO's chip, then -3 .. +6 on a ruler (0 longer) */
        float x0 = 132.0f, dx = 10.0f;
        lk_label(26.0f, 105.0f, "VOICING", 20);
        ui_chip(80.0f, 102.0f, "AUTO", !chd.voicing, ui_hot(2));
        for (i = 0; i < CH_NVOIC - 1u; i++)
            lk_seg(x0 + dx * (float)i, i == 3u ? 104.0f : 107.0f, x0 + dx * (float)i, 115.0f, 1.0f, i == 3u ? sk.mid : sk.line);
        if (chd.voicing)
            lk_disc(x0 + dx * (float)(chd.voicing - 1u), 110.0f, 3.2f, ui_ink(ui_hot(2)));
    }
    x = 26.0f;                                      /* SPREAD */
    for (i = 0; i < CH_NSPREAD; i++)
        x = ui_chip(x, 124.0f, CH_SPREAD_NAME[i], on && i == chd.spread % CH_NSPREAD, ui_hot(3));
    ui_ch_kb(8.0f, 148.0f, 224.0f, 56.0f, ui_ch_lo(nt, n, bass, 3u), 3u, nt, n, bass, 0);
    ui_H(0, 8.0f, 61.0f);
    ui_H(1, 8.0f, 83.0f);
    ui_H(2, 8.0f, 105.0f);
    ui_H(3, 8.0f, 127.0f);
}

/* PERFORM's bar of what it plays for the chord shown: each note a row, its time across (a strum's stagger, an
 * arpeggio's steps, a pattern's stabs; SLOP's looseness from a fixed seed). V0.75: ARP's and PATTERN's sixteen steps
 * as ch_step plays them (in steps: SWING, GATE, ARP's steps, its RANDOM and CHANCE from a seed of the page's own) */
typedef struct { float t0, t1; uint8_t n, v, hollow; } ui_pev_t;   /* (v: a level, %; hollow: CHANCE under 100 %) */
static uint32_t ui_ch_events(const uint8_t *nt, const uint8_t *od, uint32_t n, ui_pev_t *ev, uint32_t max, float *win)
{
    uint32_t st = chd.style % PF_N, k = 0, i, seed = 7u;
    float g = (float)sy_clampi(chd.gate, 5, 200) / 100.0f;
    int32_t feel = chd.feel[st];
    *win = 16.0f;                                   /* (sixteen steps, in steps) */
    if (st == PF_ARP) {                             /* (as ch_step plays them: RANDOM, WALK and CHANCE from a seed) */
        uint32_t len = (uint32_t)sy_clampi(chd.len, 1, AR_NSTEP), ni = 0, s, j, h, m, r, ties;
        uint8_t walk = 0, out[CH_MAXN];
        for (i = 0; i < 16u && k < max && n; i++) {
            float t0 = (float)i + ch_swing(i), gap, last;
            s = i % len;
            if (chd.st[s] != AS_PLAY)
                continue;
            m = ar_notes(ni++, nt, od, n, out, &walk, &seed);
            r = (uint32_t)sy_clampi(chd.rat[s], 1, 4);
            for (j = 1, ties = 0; j < len && chd.st[(s + j) % len] == AS_TIE; j++)
                ties++;
            gap = 1.0f / (float)r;
            if (chd.sld[s])
                last = (float)(1u + ties) + ch_swing(i + 1u + ties) - ch_swing(i) - (float)(r - 1u) * gap + 0.05f;
            else if (ties)
                last = (float)ties + ch_swing(i + ties) - ch_swing(i) - (float)(r - 1u) * gap + g;
            else
                last = gap * g;
            for (h = 0; h < r; h++)
                for (j = 0; j < m && k < max; j++) {
                    ev[k].n = ch_fold(out[j] + 12 * ((int32_t)sy_clampi(chd.oct[s], 0, 4) - 2));
                    ev[k].t0 = t0 + (float)h * gap;
                    ev[k].t1 = ev[k].t0 + sy_maxf(h + 1u == r ? last : gap * g, 0.05f);
                    ev[k].v = (uint8_t)sy_clampi(chd.lvl[s], 5, 100);
                    ev[k++].hollow = chd.chn[s] < 100u;
                }
        }
    } else if (st == PF_PATTERN) {
        for (i = 0; i < 16u; i++)
            if ((CH_PAT[chd.move[st] % 16u] >> i) & 1u) {
                uint32_t j;
                for (j = 0; j < n && k < max; j++) {
                    ev[k].n = nt[j];
                    ev[k].t0 = (float)i + ch_swing(i);
                    ev[k].t1 = ev[k].t0 + g;
                    ev[k].hollow = 0;
                    ev[k++].v = (uint8_t)(i % 4u ? 100 - sy_clampi(feel, 0, 100) : 100);
                }
            }
    }
    return k;
}
static void ui_p_cperf(void)
{
    uint8_t nt[CH_MAXN], od[CH_MAXN];
    uint32_t bass, root, n = ui_ch_now(nt, &bass, &root), st = chd.style % PF_N, i, k;
    char v[24];
    const float X0 = 14.0f, W = 212.0f, Y0 = 62.0f, H = 104.0f;
    memcpy(od, chs.built && chs.n ? chs.ord : nt, n);
    pg.figs = 0xFu;                                 /* (the values in the row, the style the title: no flag) */
    lk_text(UI_TITLE_X, 24.0f, T14B, PF_NAME[st], ui_ink(ui_hot(0)), sk.ground, 0, 17);
    lk_chip(184.0f, 24.0f, chd.perf ? "ARP ON" : "ARP OFF", chd.perf);
    lk_frame(X0, Y0, W, H, 1.0f, sk.line);
    if (st == PF_HARP) {                            /* HARP: the note each key plays, a step a key */
        uint32_t nstr = 27u, pcs = 0;
        float px = 0.0f, py = 0.0f;
        if (chd.mode == CH_OFF)
            pcs = ((uint32_t)chd.mask << chd.root | (uint32_t)chd.mask >> (12u - chd.root)) & 0xFFFu;
        else
            for (i = 0; i < n; i++)
                pcs |= 1u << (nt[i] % 12u);
        for (k = 0; k < nstr && pcs; k++) {
            int32_t at = 48 + (int32_t)(root % 12u) + (int32_t)(k * 48u / nstr), x;
            float cx = X0 + 6.0f + (W - 12.0f) * (float)k / (float)(nstr - 1u), cy;
            for (x = at; x >= 0 && !((pcs >> (uint32_t)(x % 12)) & 1u); x--)
                ;
            cy = Y0 + H - 8.0f - (H - 16.0f) * (float)(x - 48 - (int32_t)(root % 12u)) / 48.0f;
            if (k)
                lk_seg(px, py, cx, py, 1.0f, sk.mid), lk_seg(cx, py, cx, cy, 1.0f, sk.mid);
            lk_disc(cx, cy, 2.4f, ui_ink(ui_hot(0)));
            px = cx;
            py = cy;
        }
    } else {
        ui_pev_t ev[64];
        float win = 1.0f;
        uint8_t rows[32];
        uint32_t ne = ui_ch_events(nt, od, n, ev, 64u, &win), nr = 0;
        int hot = ui_hot(1) || ui_hot(2) || ui_hot(3);
        uint32_t len = st == PF_ARP ? (uint32_t)sy_clampi(chd.len, 1, AR_NSTEP) : 16u;
        for (i = 0; i < ne; i++) {                  /* the rows: each note once, the lowest at the bottom */
            for (k = 0; k < nr && rows[k] != ev[i].n; k++)
                ;
            if (k == nr && nr < 32u)
                rows[nr++] = ev[i].n;
        }
        ch_sort(rows, nr);
        for (i = 0; i <= 16u; i++)                  /* (the steps: a tick each, the beats longer; ARP's LENGTH) */
            lk_seg(X0 + W * (float)i / 16.0f, Y0 + H, X0 + W * (float)i / 16.0f, Y0 + H + (i % 4u ? 3.0f : 6.0f), 1.0f,
                   i % 4u ? sk.line : sk.mid);
        if (len < 16u)
            lk_seg_dash(X0 + W * (float)len / 16.0f, Y0 + 1.0f, X0 + W * (float)len / 16.0f, Y0 + H - 1.0f, sk.mid,
                        1.0f, 2.0f);
        if (chd.perf && chs.at)                     /* (the step playing: where it falls in the sixteen) */
            for (i = (uint32_t)(chs.at - 1u) % len; i < 16u; i += len) {
                float cx = X0 + W * ((float)i + 0.5f) / 16.0f;
                lk_tri(cx - 3.5f, Y0 + H + 8.0f, cx + 3.5f, Y0 + H + 8.0f, cx, Y0 + H + 3.0f, sk.hot);
            }
        for (i = 0; i < ne; i++) {
            float rh = sy_minf(12.0f, (H - 8.0f) / (float)(nr ? nr : 1u)), y, xa, xb, hh;
            uint16_t c;
            if (ev[i].t0 >= win)
                continue;
            for (k = 0; k < nr && rows[k] != ev[i].n; k++)
                ;
            y = Y0 + H - 4.0f - rh * ((float)k + 0.5f);
            xa = X0 + 2.0f + (W - 4.0f) * ev[i].t0 / win;
            xb = X0 + 2.0f + (W - 4.0f) * sy_minf(ev[i].t1, win) / win;
            xb = sy_maxf(xb - 1.0f, xa + 2.0f);     /* (a pixel apart from the next) */
            hh = sy_minf(3.5f, 0.32f * rh);
            c = sy_mix565(sk.mid, ui_ink(hot), (uint32_t)sy_clampi(ev[i].v, 0, 100) * 255u / 100u);
            if (ev[i].hollow)
                lk_frame(xa, y - hh - 0.5f, xb - xa, 2.0f * hh + 1.0f, 1.0f, c);
            else
                lk_box(xa, y - hh, xb, y + hh, c);
        }
    }
    for (i = 0; i < 3u; i++) {                      /* RATE, MOVE, FEEL: a word over the value each */
        static const float CX[3] = {26.0f, 104.0f, 182.0f};
        const char *nm = ui_chord_nv(UI_PRATE + i, v);
        lk_text(CX[i], 176.0f, T9, nm, sk.lab, sk.ground, 20, 0);
        lk_text(CX[i], 188.0f, T10B, v, ui_ink(ui_hot(1u + i)), sk.ground, 0, 0);
        ui_H(1u + i, CX[i] - 18.0f, 185.0f);
    }
    ui_H(0, 8.0f, 27.0f);
}

/* BASS: on or off, the octave, the chord and the bass on four octaves of keys (an arrow down to it), the level */
static void ui_p_cbass(void)
{
    uint8_t nt[CH_MAXN];
    uint32_t bass, root, n = ui_ch_now(nt, &bass, &root);
    int32_t lo;
    char v[16];
    float e;
    if (chd.bass && bass > 127u) {                  /* (no chord yet: where the bass would be) */
        int32_t b = (int32_t)root - 12 * (int32_t)chd.boct;
        bass = b >= 12 ? (uint32_t)b : 0xFFu;
    }
    pg.figs = 0xFu;                                 /* (on, the octave, the level: on the page, HOT while turning) */
    lk_text(UI_TITLE_X, 24.0f, T14B, chd.bass ? "BASS ON" : "BASS OFF", ui_ink(ui_hot(0)), sk.ground, 0, 17);
    sp_put(sp_fix(v, -(int32_t)chd.boct, 0u, 1), chd.boct == 1u ? " OCTAVE" : " OCTAVES");
    e = lk_text(UI_TITLE_X, 42.0f, T10, "UNDER THE CHORD", sk.lab, sk.ground, 20, 0);
    lk_text(e + lk_tw(T10, " ", 20), 42.0f, T10B, v, ui_ink(ui_hot(1)), sk.ground, 20, 0);
    lo = ui_ch_lo(nt, n, chd.bass ? bass : 0xFFu, 4u);
    ui_ch_kb(8.0f, 74.0f, 224.0f, 64.0f, lo, 4u, nt, n, chd.bass ? bass : 0xFFu, 0);
    if (chd.bass && bass <= 127u && n) {            /* (the root to the bass: an arrow under the keys) */
        int black;
        float ww = 224.0f / 28.0f, xb = ui_ch_kx(8.0f, ww, lo, bass, &black), xr = 0.0f;
        uint32_t i;
        for (i = 0; i < n; i++)
            if (nt[i] % 12u == root % 12u) {
                xr = ui_ch_kx(8.0f, ww, lo, nt[i], &black);
                break;
            }
        if (xr > xb + 4.0f)
            lk_arrow(xr, 150.0f, xb, 150.0f, 3.0f, 1.5f, ui_ink(ui_hot(1)));
        lk_seg(xb, 140.0f, xb, 156.0f, 1.0f, sk.hot);
    }
    {                                               /* LEVEL */
        float x = 80.0f + 140.0f * (float)chd.blvl / 100.0f;
        lk_label(26.0f, 190.0f, "LEVEL", 20);
        lk_seg(80.0f, 196.0f, 220.0f, 196.0f, 3.0f, sk.line);
        lk_seg(80.0f, 196.0f, x, 196.0f, 3.0f, chd.bass ? ui_ink(ui_hot(2)) : sk.mid);
        sp_put(sp_fix(v, chd.blvl, 0u, 0), "%");
        lk_text_r(232.0f, 176.0f, T10B, v, ui_ink(ui_hot(2)), sk.ground, 0, 0);
        ui_H(2, 8.0f, 191.0f);
    }
    ui_H(0, 8.0f, 27.0f);
    ui_H(1, 8.0f, 43.0f);
}

/* ------------------------------------------------------------------------------------------- ARP (V0.75) --- */
/* ARP's pages (ARP held): STEPS and RATCHET, its sixteen steps a column each across 14 .. 226; GROOVE, two beats of
 * steps where SWING puts them, as long as GATE makes them, and LENGTH's sixteen cells */
#define UI_AX0 14.0f
#define UI_AW (212.0f / 16.0f)
/* the chip at the right of the title: ARP on or off (another style: its name, ARP's steps waiting for it) */
static void ui_arp_chip(void)
{
    char t[32];
    const char *s = t;
    if (chd.style % PF_N == PF_ARP)
        s = chd.perf ? "ARP ON" : "ARP OFF";
    else
        sp_put(sp_put(t, "STYLE: "), PF_NAME[chd.style % PF_N]);
    lk_chip(230.0f - lk_tw(T10B, s, 20) - 10.0f, 24.0f, s, chd.perf && chd.style % PF_N == PF_ARP);
}
/* the steps: OCTAVE a tick over each (on the 0 line), LEVEL a bar (RATCHET's hits side by side; CHANCE under 100 %:
 * hollow there; V0.87.1: on STEPS a fader's cap on a rail in each step, up at 100 %, down at 0), TIE the note before held on across it, REST a dash at the foot, SLIDE a slope up into the next; past
 * LENGTH in LINE; the step picked framed, the one playing marked over it, the beats ticked under. rat: RATCHET's page
 * (the hits, CHANCE's meter under each and SLIDE in INK; the levels and octaves in MID) */
static void ui_steps_lanes(int rat)
{
    const float YO = 66.0f, YB = 154.0f, HB = 66.0f, Y0 = 54.0f, Y1 = 164.0f;
    uint32_t len = (uint32_t)sy_clampi(chd.len, 1, AR_NSTEP), a = ui.astep % AR_NSTEP, i, h;
    float held = 0.0f;                              /* (the top of the note a TIE holds on; 0 none) */
    lk_seg(UI_AX0, YO, UI_AX0 + 212.0f, YO, 1.0f, sk.line);   /* (OCTAVE 0) */
    for (i = 0; i < AR_NSTEP; i++) {
        float x0 = UI_AX0 + UI_AW * (float)i, bw = UI_AW - 4.0f, xl = x0 + 2.0f, xr = xl + bw;
        float top = YB - HB * (float)sy_clampi(chd.lvl[i], 5, 100) / 100.0f, y;
        int live = i < len, sel = i == a;
        uint16_t ink = live ? sk.ink : sk.line, soft = live ? sk.mid : sk.line;
        uint16_t lvc = rat ? soft : sel && ui_hot(2) ? sk.hot : ink, hitc = rat && sel && ui_hot(1) ? sk.hot : ink;
        y = YO - 5.0f * ((float)sy_clampi(chd.oct[i], 0, 4) - 2.0f);
        lk_box(xl + 1.0f, y - 1.5f, xr - 1.0f, y + 1.5f, rat ? soft : sel && ui_hot(3) ? sk.hot : ink);
        if (!rat && chd.st[i] == AS_PLAY)           /* (V0.87.1: CHANCE's fader: its rail, the bar over it) */
            lk_seg(0.5f * (xl + xr), YB - HB, 0.5f * (xl + xr), YB, 1.0f, live ? sk.mid : sk.line);
        if (chd.st[i] == AS_PLAY) {
            uint32_t r = (uint32_t)sy_clampi(chd.rat[i], 1, 4);
            float hw = (bw - (float)(r - 1u)) / (float)r;
            for (h = 0; h < r; h++) {               /* (each hit: a bar of its own) */
                float ha = xl + (hw + 1.0f) * (float)h;
                if (rat && chd.chn[i] < 100u)
                    lk_frame(ha + 0.5f, top + 0.5f, hw - 1.0f, YB - top - 1.0f, 1.0f, rat ? hitc : lvc);
                else
                    lk_box(ha, top, ha + hw, YB, rat ? hitc : lvc);
            }
            if (chd.sld[i])                         /* (SLIDE: up into the next step) */
                lk_seg(xr - 1.0f, top + 1.0f, xr + 5.0f, top - 5.0f, 1.5f,
                       !rat ? soft : sel && ui_hot(3) ? sk.hot : ink);
            held = top;
            if (!rat) {                             /* (V0.87.1: CHANCE a fader's cap, up at 100 %, down at 0: a gap of */
                float cy = YB - HB * (float)sy_clampi(chd.chn[i], 0, 100) / 100.0f;   /* ground round it keeps it clear of the bar) */
                lk_box(xl - 1.0f, cy - 3.0f, xr + 1.0f, cy + 3.0f, sk.ground);
                lk_box(xl, cy - 2.0f, xr, cy + 2.0f, live ? ink : sk.line);
            }
        } else if (chd.st[i] == AS_TIE && held > 0.0f) {
            lk_box(x0 - 2.0f, held, xr, YB, soft);  /* (the note before, on across this step) */
        } else {
            lk_box(xl + 1.0f, YB - 3.0f, xr - 1.0f, YB, soft);   /* (REST: a stub) */
            held = 0.0f;
        }
        if (rat && chd.st[i] == AS_PLAY) {          /* RATCHET's page: CHANCE's meter under the step */
            lk_box(xl, YB + 4.0f, xr, YB + 7.0f, sk.line);
            lk_box(xl, YB + 4.0f, xl + bw * (float)chd.chn[i] / 100.0f, YB + 7.0f, sel && ui_hot(2) ? sk.hot : ink);
        }
        lk_seg(x0, Y1 + 1.0f, x0, Y1 + (i % 4u ? 3.0f : 6.0f), 1.0f, i % 4u ? sk.line : sk.mid);
    }
    if (len < AR_NSTEP)
        lk_seg_dash(UI_AX0 + UI_AW * (float)len, Y0, UI_AX0 + UI_AW * (float)len, Y1, sk.mid, 1.0f, 2.0f);
    lk_frame(UI_AX0 + UI_AW * (float)a + 0.5f, Y0 + 0.5f, UI_AW - 1.0f, Y1 - Y0 - 1.0f, 1.0f,
             ui_hot(0) ? sk.hot : sk.ink);
    if (chd.perf && chd.style % PF_N == PF_ARP && chs.at) {   /* (the step playing) */
        float cx = UI_AX0 + UI_AW * ((float)((chs.at - 1u) % AR_NSTEP) + 0.5f);
        lk_tri(cx - 3.5f, 46.0f, cx + 3.5f, 46.0f, cx, 51.0f, sk.hot);
    }
}
/* a page's three knobs in a row under its graph: a word over the value each (knobs k0 .. k0 + 2) */
static void ui_arp_row(uint32_t k0, const int32_t *ids)
{
    static const float CX[3] = {26.0f, 104.0f, 182.0f};
    uint32_t i;
    char v[24];
    for (i = 0; i < 3u; i++) {
        const char *nm = ui_chord_nv((uint32_t)ids[i], v);
        lk_text(CX[i], 176.0f, T9, nm, sk.lab, sk.ground, 20, 0);
        lk_text(CX[i], 188.0f, T10B, v, ui_ink(ui_hot(k0 + i)), sk.ground, 0, 0);
        ui_H(k0 + i, CX[i] - 18.0f, 185.0f);
    }
}
/* the title of STEPS and RATCHET: the step picked (KNOB 1's figure: HOT while it turns; the pages show every value,
 * no flag) */
static void ui_step_title(void)
{
    char t[16];
    sp_fix(sp_put(t, "STEP "), (int32_t)(ui.astep % AR_NSTEP) + 1, 0u, 0);
    lk_text(UI_TITLE_X, 24.0f, T14B, t, ui_ink(ui_hot(0)), sk.ground, 0, 17);
    pg.figs = 0xFu;
}
/* STEPS: the step picked (KNOB 1), its KIND, LEVEL, OCTAVE */
static void ui_p_steps(void)
{
    static const int32_t IDS[3] = {UI_STYPE, UI_SLVL, UI_SOCT};
    ui_step_title();
    ui_arp_chip();
    ui_steps_lanes(0);
    ui_arp_row(1u, IDS);
    ui_H(0, 8.0f, 27.0f);
}
/* RATCHET: the step picked (KNOB 1), its RATCHET, CHANCE, SLIDE */
static void ui_p_ratch(void)
{
    static const int32_t IDS[3] = {UI_SRAT, UI_SCHN, UI_SSLD};
    ui_step_title();
    ui_arp_chip();
    ui_steps_lanes(1);
    ui_arp_row(1u, IDS);
    ui_H(0, 8.0f, 27.0f);
}
/* GROOVE: two beats of steps (the straight places dashed, the beat solid), each note where SWING puts it (an arrow
 * from its straight place) and as long as GATE makes it (past 100 % into the next: a gap marks where that starts);
 * LENGTH's sixteen cells; LATCH's chip */
static void ui_p_groove(void)
{
    static const int32_t IDS[3] = {UI_AGATE, UI_ASWING, UI_ALEN};
    const float X0 = 14.0f, W = 212.0f, Y0 = 58.0f, Y1 = 132.0f, YN = 100.0f, SL = 212.0f / 8.0f;
    float g = (float)sy_clampi(chd.gate, 5, 200) / 100.0f, cw = W / 16.0f;
    uint32_t i, len = (uint32_t)sy_clampi(chd.len, 1, AR_NSTEP);
    const char *ls = chd.latch ? "LATCH ON" : "LATCH OFF";
    float lx = 230.0f - lk_tw(T10B, ls, 20) - 10.0f;
    ui_title("GROOVE", UI_TITLE_X);
    ui_sub(UI_TITLE_X, "ARP AND PATTERN", 0);
    pg.figs = 0xFu;                                    /* (the values in the row, LATCH its chip: no flag) */
    lk_chip(lx, 24.0f, ls, chd.latch);
    for (i = 0; i <= 8u; i++) {
        float x = X0 + SL * (float)i;
        if (i % 4u)
            lk_seg_dash(x, Y0, x, Y1, sk.line, 1.0f, 3.0f);
        else
            lk_seg(x, Y0, x, Y1, 1.0f, sk.mid);
    }
    for (i = 0; i < 8u; i++) {
        float xs = X0 + SL * (float)i, xa = xs + SL * ch_swing(i), xb = sy_minf(xa + SL * g, X0 + W);
        lk_box(xa + 1.0f, YN - 8.0f, sy_maxf(xb - 1.0f, xa + 3.0f), YN + 8.0f, ui_ink(ui_hot(0)));
        if (i)
            lk_seg(xa + 0.5f, YN - 8.0f, xa + 0.5f, YN + 8.0f, 1.0f, sk.ground);   /* (where this one starts) */
        if (xa - xs > 5.0f)
            lk_arrow(xs + 1.0f, 74.0f, xa, 74.0f, 3.0f, 1.5f, ui_ink(ui_hot(1)));
    }
    for (i = 0; i < AR_NSTEP; i++) {                /* LENGTH */
        float x = X0 + cw * (float)i;
        if (i < len)
            lk_box(x + 1.0f, 146.0f, x + cw - 1.0f, 158.0f, ui_ink(ui_hot(2)));
        else
            lk_frame(x + 1.5f, 146.5f, cw - 3.0f, 11.0f, 1.0f, sk.line);
    }
    ui_arp_row(0u, IDS);
    ui_H(3, lx - 18.0f, 27.0f);
}

/* STRUM (V0.75): the chord shown as STRUM starts it: a row a note (the lowest at the bottom), each from its start on
 * (DIRECTION's order, STRUM apart, HUMAN's looseness from a seed of the page's own; ALTERNATE drawn up), a disc at its
 * start, as bright as TAPER and HUMAN leave its level; the time across (ticks each 10 ms, or 50) */
static void ui_p_strum(void)
{
    static const int32_t IDS[3] = {UI_SDIR, UI_SHUM, UI_STAP};
    const float X0 = 14.0f, W = 212.0f, Y0 = 54.0f, H = 108.0f;
    uint8_t nt[CH_MAXN], l[CH_MAXN];
    uint32_t bass, root, n = ui_ch_now(nt, &bass, &root), i, k, seed = 7u, dir = chd.sdir % CH_NSDIR;
    float gap = (float)CH_STRUM_MS[chd.strum % CH_NSTRUM], hu = (float)sy_clampi(chd.human, 0, 100) / 100.0f;
    float tp = (float)sy_clampi(chd.taper, 0, 100) / 100.0f, t0[CH_MAXN], lv[CH_MAXN], win = 0.0f, tick;
    int hot = ui_hot(0) || ui_hot(1) || ui_hot(2) || ui_hot(3);
    char t[24];
    pg.figs = 0xFu;                                 /* (the values: the title's and the row's, HOT while turning) */
    if (!chd.strum)
        sp_put(t, "STRUM OFF");
    else
        sp_put(sp_fix(sp_put(t, "STRUM "), CH_STRUM_MS[chd.strum % CH_NSTRUM], 0u, 0), " MS");
    lk_text(UI_TITLE_X, 24.0f, T14B, t, ui_ink(ui_hot(0)), sk.ground, 0, 17);
    memcpy(l, nt, n);
    if (dir == 1u) {
        for (i = 0; i < n / 2u; i++) {
            uint8_t x = l[i];
            l[i] = l[n - 1u - i];
            l[n - 1u - i] = x;
        }
    } else if (dir == 3u) {
        for (i = n; i > 1u; i--) {
            uint32_t j = sy_rand(&seed) % i;
            uint8_t x = l[i - 1u];
            l[i - 1u] = l[j];
            l[j] = x;
        }
    }
    for (i = 0; i < n; i++) {                       /* (as ch_strum: the times, ms; the levels, 0 .. 1) */
        t0[i] = gap * (float)i;
        lv[i] = 1.0f - (n > 1u ? tp * (float)i / (float)(n - 1u) * 0.5f : 0.0f);
        if (hu > 0.0f) {
            t0[i] += (float)(sy_rand(&seed) % 1000u) * hu * 0.012f;
            lv[i] *= 1.0f + ((float)(sy_rand(&seed) % 101u) - 50.0f) * hu / 200.0f;
        }
        win = sy_maxf(win, t0[i]);
    }
    win = sy_maxf(60.0f, win * 1.3f + 20.0f);
    lk_frame(X0, Y0, W, H, 1.0f, sk.line);
    tick = win > 300.0f ? 50.0f : 10.0f;
    for (i = 0; (float)i * tick <= win; i++) {      /* (the time: a tick each 10 ms, or 50; each 50 ms, or 250, longer) */
        float x = X0 + 2.0f + (W - 4.0f) * (float)i * tick / win;
        int big = (uint32_t)((float)i * tick) % (tick > 10.0f ? 250u : 50u) == 0u;
        lk_seg(x, Y0 + H, x, Y0 + H + (big ? 6.0f : 3.0f), 1.0f, big ? sk.mid : sk.line);
    }
    for (i = 0; i < n; i++) {
        float rh = sy_minf(14.0f, (H - 8.0f) / (float)(n ? n : 1u)), y, xa;
        uint16_t c = sy_mix565(sk.mid, ui_ink(hot), (uint32_t)(sy_clampf(lv[i], 0.0f, 1.0f) * 255.0f));
        for (k = 0; k < n && nt[k] != l[i]; k++)
            ;
        y = Y0 + H - 4.0f - rh * ((float)k + 0.5f);
        xa = X0 + 2.0f + (W - 4.0f) * t0[i] / win;
        lk_box(xa, y - 1.5f, X0 + W - 3.0f, y + 1.5f, c);
        lk_disc(xa, y, 3.0f, c);
    }
    ui_arp_row(1u, IDS);
    ui_H(0, 8.0f, 27.0f);
}
