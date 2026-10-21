/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* The STRATA UI's chord pages (V0.7, SCL's family after SCALE; the chords themselves: sy_chord.c):
 *   - CHORD: the chord playing named large ("Cm7/G"), MODE's, TYPE's and the extensions' chips (FREE: what the chord
 *     buttons latched), VOICING's place, the chord's notes and the bass on three octaves of keys;
 *   - PERFORM (V0.75: ARP held, ARP's family): the style, the ARP button's state, a bar of what it plays (each note
 *     a row, its time across: a strum's stagger, an arpeggio's steps, a pattern's stabs; HARP: the note each key
 *     plays; the step playing marked under it), RATE, MOVE and FEEL; STEPS, RATCHET and GROOVE (V0.75, below);
 *   - BASS: on or off, its octave, the chord and the bass on four octaves of keys, its level;
 *   - MIDI: where the chords come from and go (the keys, MIDI in, the sound, USB out), the two channels.
 * Each draws from chd (the settings) and chs (the chord as the audio side last built it), read as they stand. Needs
 * sy_ui.c, sy_ui_draw.c (pg, ui_H, ui_hot, ui_title, ui_sub) and sy_gfx.c. */

/* knob k's name and value on SCL's chord pages (sy_ui_draw.c ui_knob_nv) */
static const char *ui_chord_nv(uint32_t id, char *v)
{
    uint32_t st = chd.style % PF_N, i, a = ui.astep % AR_NSTEP;
    v[0] = 0;
    switch (id) {
    case UI_HMODE: sp_put(v, CH_MODE_NAME[chd.mode % CH_NMODE]); return "MODE";
    case UI_HTYPE: sp_put(v, chd.mode == CH_KEY ? CH_TYPE_NAME[1][chd.ktype & 3u] : CH_TYPE_NAME[0][chd.type & 3u]); return "TYPE";
    case UI_HEXT: {
        char *e = v;
        if (!chd.ext)
            sp_put(v, "NONE");
        for (i = 0; i < 4u; i++)
            if ((chd.ext >> i) & 1u) {
                if (chd.mode == CH_KEY && i == 2u && (chd.ext & 2u))
                    continue;                       /* (KEY: m7 and M7 are the in-key 7th) */
                if (e != v)
                    e = sp_put(e, "+");
                e = sp_put(e, chd.mode == CH_KEY && (i == 1u || i == 2u) ? "7" : CH_EXT_NAME[i]);
            }
        return "EXTENSION";
    }
    case UI_HVOI:
        if (!chd.voicing)
            sp_put(v, "AUTO");
        else
            sp_fix(v, (int32_t)chd.voicing - 4, 0u, 1);
        return "VOICING";
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
        else if (st == PF_HARP)
            sp_put(v, "\x97");
        else
            sp_put(v, ST_NAME[chd.move[st] % 3u]);
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
    case UI_CIN: sp_put(v, chd.min ? "ROOTS" : "AS SENT"); return "MIDI IN";
    case UI_COUT: sp_put(v, chd.mout ? "ON" : "OFF"); return "USB OUT";
    case UI_COCH: sp_fix(v, chd.och + 1, 0u, 0); return "CHORDS CH";
    case UI_CBCH: sp_fix(v, chd.bch + 1, 0u, 0); return "BASS CH";
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
    default: sp_put(v, chd.sld[a] ? "ON" : "OFF"); return "SLIDE";
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
    uint32_t bass, root, n = ui_ch_now(nt, &bass, &root), i, key = chd.mode == CH_KEY;
    char t[24];
    float x;
    if (chd.mode == CH_OFF && !chd.perf && !chd.bass) {
        lk_text(26.0f, 22.0f, T26B, "OFF", sk.lab, sk.ground, -20, 28);
    } else if (!chs.built || !chs.n) {
        lk_text(26.0f, 22.0f, T26B, "\x97", sk.lab, sk.ground, -20, 28);
    } else {
        ch_name_of(t, nt, n, root);
        lk_text(26.0f, 22.0f, T26B, t, sk.ink, sk.ground, -20, 28);
    }
    x = 26.0f;                                      /* MODE */
    for (i = 0; i < CH_NMODE; i++)
        x = lk_chip(x, 58.0f, CH_MODE_NAME[i], i == chd.mode);
    x = 26.0f;                                      /* TYPE (KEY's: of the scale) */
    for (i = 0; i < 4u; i++)
        x = lk_chip(x, 80.0f, CH_TYPE_NAME[key][i], chd.mode != CH_OFF && i == ((key ? chd.ktype : chd.type) & 3u));
    x = 26.0f;                                      /* the extensions (KEY: the in-key 6th, 7th, 9th) */
    for (i = 0; i < 4u; i++) {
        if (key && i == 2u)
            continue;
        x = lk_chip(x, 102.0f, key && i == 1u ? "7" : CH_EXT_NAME[i],
                    chd.mode != CH_OFF && (key && i == 1u ? (chd.ext & 6u) != 0u : ((chd.ext >> i) & 1u) != 0u));
    }
    {                                               /* VOICING: AUTO's chip, then -3 .. +6 on a ruler (0 longer) */
        float x0 = 124.0f, dx = 10.0f;
        lk_label(26.0f, 127.0f, "VOICING", 20);
        lk_chip(80.0f, 124.0f, "AUTO", !chd.voicing);
        for (i = 0; i < CH_NVOIC - 1u; i++)
            lk_seg(x0 + dx * (float)i, i == 3u ? 126.0f : 129.0f, x0 + dx * (float)i, 137.0f, 1.0f, i == 3u ? sk.mid : sk.line);
        if (chd.voicing)
            lk_disc(x0 + dx * (float)(chd.voicing - 1u), 132.0f, 3.2f, ui_ink(ui_hot(3)));
        ui_H(3, 222.0f, 127.0f);
    }
    ui_ch_kb(8.0f, 148.0f, 224.0f, 56.0f, ui_ch_lo(nt, n, bass, 3u), 3u, nt, n, bass, 0);
    ui_H(0, 8.0f, 61.0f);
    ui_H(1, 8.0f, 83.0f);
    ui_H(2, 8.0f, 105.0f);
}

/* PERFORM's bar of what it plays for the chord shown: each note a row, its time across (a strum's stagger, an
 * arpeggio's steps, a pattern's stabs; SLOP's looseness from a fixed seed). V0.75: ARP's and PATTERN's sixteen steps
 * as ch_step plays them (in steps: SWING, GATE, ARP's steps, its RANDOM and CHANCE from a seed of the page's own) */
typedef struct { float t0, t1; uint8_t n, v, hollow; } ui_pev_t;   /* (v: a level, %; hollow: CHANCE under 100 %) */
static uint32_t ui_ch_events(const uint8_t *nt, const uint8_t *od, uint32_t n, ui_pev_t *ev, uint32_t max, float *win)
{
    uint32_t st = chd.style % PF_N, k = 0, i, seed = 7u;
    float rate = (float)ch_rate_smp() / SY_FS, g = (float)sy_clampi(chd.gate, 5, 200) / 100.0f;
    int32_t feel = chd.feel[st];
    if (st == PF_STRUM || st == PF_STRUM2 || st == PF_SLOP) {
        uint8_t l[CH_MAXN];
        uint32_t m = n, up = chd.move[st] % 3u != 1u;
        float t = 0.0f;
        memcpy(l, nt, n);
        if (st == PF_STRUM2)
            for (i = 0; i < n && m < CH_MAXN; i++)
                l[m++] = (uint8_t)(nt[i] + 12u);
        ch_sort(l, m);
        for (i = 0; i < m && k < max; i++) {
            int32_t v = 100;
            ev[k].n = l[up ? i : m - 1u - i];
            ev[k].t0 = t;
            ev[k].hollow = 0;
            if (st == PF_SLOP) {
                v += v * ((int32_t)(sy_rand(&seed) % 101u) - 50) * feel / 10000;
                t += rate * (float)(100u - (uint32_t)feel * (sy_rand(&seed) % 86u) / 100u) / 100.0f;
            } else {
                v -= m > 1u ? v * feel * (int32_t)i / (int32_t)(m - 1u) / 200 : 0;
                t += rate;
            }
            ev[k++].v = (uint8_t)sy_clampi(v, 1, 127);
        }
        *win = t + 3.0f * rate;
        for (i = 0; i < k; i++)
            ev[i].t1 = *win;
        return k;
    }
    *win = 16.0f;                                   /* (ARP, PATTERN: sixteen steps, in steps) */
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
    static const char *const MV[PF_N] = {"DIRECTION", "DIRECTION", "MODE", "PATTERN", "\x97", "DIRECTION"};
    uint8_t nt[CH_MAXN], od[CH_MAXN];
    uint32_t bass, root, n = ui_ch_now(nt, &bass, &root), st = chd.style % PF_N, i, k;
    char v[24];
    const float X0 = 14.0f, W = 212.0f, Y0 = 62.0f, H = 104.0f;
    memcpy(od, chs.built && chs.n ? chs.ord : nt, n);
    pg.figs = 1;                                    /* (V0.75: the values in the row, the style the title: no flag) */
    lk_text(UI_TITLE_X, 24.0f, T14B, PF_NAME[st], ui_ink(ui_hot(0)), sk.ground, 0, 17);
    lk_chip(184.0f, 24.0f, chd.perf ? "ARP ON" : "ARP OFF", chd.perf);
    lk_frame(X0, Y0, W, H, 1.0f, sk.line);
    if (st == PF_HARP) {                            /* HARP: the note each key plays, a step a key */
        uint32_t first = chd.mode == CH_FREE ? CH_NBTN : 0u, nstr = 27u - first, pcs = 0;
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
        for (i = 0; i < ne; i++) {                  /* the rows: each note once, the lowest at the bottom */
            for (k = 0; k < nr && rows[k] != ev[i].n; k++)
                ;
            if (k == nr && nr < 32u)
                rows[nr++] = ev[i].n;
        }
        ch_sort(rows, nr);
        if (ch_stepped(st)) {                       /* (the steps: a tick each, the beats longer; ARP's LENGTH) */
            uint32_t len = st == PF_ARP ? (uint32_t)sy_clampi(chd.len, 1, AR_NSTEP) : 16u;
            for (i = 0; i <= 16u; i++)
                lk_seg(X0 + W * (float)i / 16.0f, Y0 + H, X0 + W * (float)i / 16.0f, Y0 + H + (i % 4u ? 3.0f : 6.0f), 1.0f,
                       i % 4u ? sk.line : sk.mid);
            if (len < 16u)
                lk_seg_dash(X0 + W * (float)len / 16.0f, Y0 + 1.0f, X0 + W * (float)len / 16.0f, Y0 + H - 1.0f, sk.mid,
                            1.0f, 2.0f);
            if (chd.perf && chs.at && chd.style == st)   /* (the step playing: where it falls in the sixteen) */
                for (i = (uint32_t)(chs.at - 1u) % len; i < 16u; i += len) {
                    float cx = X0 + W * ((float)i + 0.5f) / 16.0f;
                    lk_tri(cx - 3.5f, Y0 + H + 8.0f, cx + 3.5f, Y0 + H + 8.0f, cx, Y0 + H + 3.0f, sk.hot);
                }
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
        lk_text(CX[i], 176.0f, T9, i == 1u ? MV[st] : nm, sk.lab, sk.ground, 20, 0);
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
    if (chd.bass && bass > 127u) {                  /* (no chord yet: where the bass would be) */
        int32_t b = (int32_t)root - 12 * (int32_t)chd.boct;
        bass = b >= 12 ? (uint32_t)b : 0xFFu;
    }
    ui_title(chd.bass ? "BASS ON" : "BASS OFF", UI_TITLE_X);
    sp_put(sp_fix(v, -(int32_t)chd.boct, 0u, 1), chd.boct == 1u ? " OCTAVE" : " OCTAVES");
    ui_sub(UI_TITLE_X, "UNDER THE CHORD", v);
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
        ui_H(1, sy_clampf(0.5f * (xr + xb) - 5.0f, 8.0f, 214.0f), 156.0f);
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
    if (!(chd.bass && bass <= 127u && n))
        ui_H(1, 8.0f, 43.0f);
}

/* MIDI: the keys and MIDI in into the chords, the chords into the sound and out over USB, the channels */
static void ui_ch_box(float x, float y, float w, const char *s, int on)
{
    if (on)
        lk_frame(x + 0.5f, y + 0.5f, w - 1.0f, 19.0f, 1.0f, sk.ink);
    else
        lk_frame_dash(x + 0.5f, y + 0.5f, w - 1.0f, 19.0f, 2.0f, 2.0f, sk.mid);
    lk_text_c(x + 0.5f * w, y + 3.0f, T9B, s, on ? sk.ink : sk.lab, sk.ground, 20, 0);
}
static void ui_ch_wire(const float *p, uint32_t n, int on, int hot)
{
    if (on)
        lk_poly(p, n, 1.5f, ui_ink(hot));
    else
        lk_poly_dash(p, n, 1.0f, sk.mid, 2.0f, 2.0f);
}
static void ui_p_cmidi(void)
{
    char v[16];
    ui_ch_box(14.0f, 40.0f, 64.0f, "KEYS", 1);
    ui_ch_box(14.0f, 96.0f, 64.0f, "MIDI IN", 1);
    ui_ch_box(96.0f, 68.0f, 62.0f, "CHORDS", chd.mode != CH_OFF || chd.perf || chd.bass);
    ui_ch_box(176.0f, 40.0f, 56.0f, "SOUND", 1);
    ui_ch_box(176.0f, 96.0f, 56.0f, "USB OUT", chd.mout);
    {
        float a[6] = {78, 50, 87, 50, 96, 74}, b[6] = {78, 106, 87, 106, 96, 82}, c[6] = {158, 74, 167, 50, 176, 50};
        float d[6] = {158, 82, 167, 106, 176, 106}, e[4] = {78, 112, 176, 112};
        ui_ch_wire(a, 3u, 1, 0);
        ui_ch_wire(b, 3u, chd.min, ui_hot(0));
        ui_ch_wire(c, 3u, 1, 0);
        ui_ch_wire(d, 3u, chd.mout, ui_hot(1));
        if (!chd.min)                               /* (MIDI notes as sent: straight to the sound) */
            lk_seg_dash(e[0], e[1], e[2] - 4.0f, e[3], sk.line, 1.0f, 3.0f);
    }
    lk_label(26.0f, 136.0f, "MIDI IN", 20);
    lk_text(26.0f, 148.0f, T10B, chd.min ? "ROOTS OF CHORDS" : "AS SENT", ui_ink(ui_hot(0)), sk.ground, 0, 0);
    lk_label(144.0f, 136.0f, "USB OUT", 20);
    lk_text(144.0f, 148.0f, T10B, chd.mout ? "ON" : "OFF", ui_ink(ui_hot(1)), sk.ground, 0, 0);
    lk_label(26.0f, 176.0f, "CHORDS CH", 20);
    sp_fix(v, chd.och + 1, 0u, 0);
    lk_text(26.0f, 188.0f, T12B, v, chd.mout ? ui_ink(ui_hot(2)) : sk.lab, sk.ground, 0, 0);
    lk_label(144.0f, 176.0f, "BASS CH", 20);
    sp_fix(v, chd.bch + 1, 0u, 0);
    lk_text(144.0f, 188.0f, T12B, v, chd.mout ? ui_ink(ui_hot(3)) : sk.lab, sk.ground, 0, 0);
    ui_H(0, 8.0f, 145.0f);
    ui_H(1, 126.0f, 145.0f);
    ui_H(2, 8.0f, 185.0f);
    ui_H(3, 126.0f, 185.0f);
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
 * hollow), TIE the note before held on across it, REST a dash at the foot, SLIDE a slope up into the next; past
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
        if (chd.st[i] == AS_PLAY) {
            uint32_t r = (uint32_t)sy_clampi(chd.rat[i], 1, 4);
            float hw = (bw - (float)(r - 1u)) / (float)r;
            for (h = 0; h < r; h++) {               /* (each hit: a bar of its own) */
                float ha = xl + (hw + 1.0f) * (float)h;
                if (chd.chn[i] < 100u)
                    lk_frame(ha + 0.5f, top + 0.5f, hw - 1.0f, YB - top - 1.0f, 1.0f, rat ? hitc : lvc);
                else
                    lk_box(ha, top, ha + hw, YB, rat ? hitc : lvc);
            }
            if (chd.sld[i])                         /* (SLIDE: up into the next step) */
                lk_seg(xr - 1.0f, top + 1.0f, xr + 5.0f, top - 5.0f, 1.5f,
                       !rat ? soft : sel && ui_hot(3) ? sk.hot : ink);
            held = top;
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
    pg.figs = 1;
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
    pg.figs = 1;                                    /* (the values in the row, LATCH its chip: no flag) */
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
