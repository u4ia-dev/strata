/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* The STRATA UI's screen (docs/UI.md; the design: the "Strata UI Overhaul" gallery; V0.67: nothing under 9 px, the
 * labels in LAB, a module's VALUES pages): a page is one picture of 240 x 240, drawn with sy_gfx.c's kit:
 *   - its top line (the module's name and its dot, the module dots or the chain's squares, a word at the right) or a
 *     layer's banner; its body (what the module does, drawn from the values as the DSP reads them, with the four
 *     knobs' numbered handles on what each moves; VALUES: the four as figures, ui_p_values); its bottom row (the
 *     section's pages as zones, the one shown inverted; a word for a moment instead: the toast);
 *   - a knob turning: its handle HOT, its value in a flag beside it, the shape before the turn dashed (ui_ghost) until
 *     the gesture ends; MOMENTARY (LFO held): the shape it goes back to, dashed, the whole time;
 *   - HOME: the sound's name, the bloom (the scope's two sides, 2 x 212 points as they are captured, sown as a
 *     sunflower's seeds: opening with the sound, swirling at each note), the four macros; the countdowns of the long
 *     holds (a ring that empties), the layers (MOD, PERFORM, MACROS, MOMENTARY, JUMP), the SETTINGS, SAFE START.
 * The frame (sy_ui_frame): a page is drawn in three strips of 80 rows on gfx.c's canvas (240 x 80; V0.66's was 240 x
 * 114), each strip's rows hashed and only the rows that changed sent; a live part (the bloom, an LFO's dial, the
 * tape's reels, the metronome, a countdown's ring) is the page drawn into its own box, sent alone. The bloom's box is
 * a disc: each of its rows sends only the part of it inside the circle, so the dial's ticks around it are drawn with
 * the page, never with the bloom. The LEDs (sy_ui_leds). */

static void ui_p_filter(void);                      /* sy_ui_fx.c: the filter's and the effects' pages, the chain view */
static void ui_p_drive(void);
static void ui_p_chorus(void);
static void ui_p_delay(void);
static void ui_p_reverb(void);
static void ui_p_ott(void);
static void ui_p_eq(void);
static void ui_p_chain(void);
static void ui_p_flanger(void);
static void ui_p_phaser(void);
static void ui_p_ffilt(void);
static void ui_p_crush(void);
static void ui_p_trem(void);
static void ui_p_ring(void);
static void ui_p_tape(void);
static void ui_tape_wins(void);
static int ui_eq_modded(void);
static void ui_p_chord(void);                       /* sy_ui_chord.c: SCL's chord pages (V0.7) */
static void ui_p_cperf(void);
static void ui_p_cbass(void);
static void ui_p_cmidi(void);
static const char *ui_chord_nv(uint32_t id, char *v);
static void ui_p_steps(void);                       /* .. ARP's (V0.75) */
static void ui_p_ratch(void);
static void ui_p_groove(void);
static void ui_p_save(void);                        /* sy_ui_save.c: NAME IT, the browser */
static void ui_p_browse(void);
static void ui_save_flag(int32_t id, char *t);
static void ui_save_label(char *t);
static int ui_save_renaming(void);

/* V0.75: GLOBAL's BPM, or MIDI's clock's while the tempo follows it (CLOCK IN MIDI), in TS-NOTE beats a minute */
static uint32_t ui_bpm_now(void)
{
    return sy_app_clock() ? (uint32_t)sy_floori(sy_tempo * (float)UI_TSN_VAL[ui.tsn % 5u] / 4.0f + 0.5f) : ui.bpm;
}
/* .. the transport's place in TS-NOTE beats (the audio side's: on MIDI's clock too) */
static float ui_beats(void) { return chs.tq * (float)UI_TSN_VAL[ui.tsn % 5u] / 4.0f; }

/* sound playing: a voice on, or the output still over -70 dBFS (a delay's or a reverb's tail). The live parts move
 * only while it does; otherwise they stand still and nothing is redrawn */
static int ui_sounding(void)
{
    uint32_t i;
    for (i = 0; i < SY_NVOICE; i++)
        if (sy_v[i].on)
            return 1;
    return sy_out_pk > 10u;
}
static uint32_t ui_nvoices(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < SY_NVOICE; i++)
        n += sy_v[i].on != 0;
    return n;
}

static float ui_v(const int16_t *p, uint32_t id) { return sp_norm_of(id, p[id]); }   /* a value as 0..1 */
static char *ui_pct(char *b, int32_t v) { return sp_put(sp_fix(b, v, 1u, 1), "%"); }   /* a depth: "+20.0%" */

/* a slot's depth of source src on parameter id (0: none) */
static int32_t ui_depth(uint32_t src, uint32_t id)
{
    uint32_t s;
    for (s = 0; s < NSLOT; s++)
        if (sy_patch.p[SP_M1_SRC + 3u * s] == (int32_t)src && sy_patch.p[SP_M1_DST + 3u * s] == (int32_t)id)
            return sy_patch.p[SP_M1_AMT + 3u * s];
    return 0;
}
/* the first source on parameter id, as "LFO 1 +20.0%" (0: none) */
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

/* ------------------------------------------------------------------------------------------ page --- */
/* the page being drawn: where each knob's handle sits (has: bit k placed), the values to dash (before the turn, or
 * MOMENTARY's way back: mom, drawn whatever turns) */
static struct {
    float hx[4], hy[4];
    uint8_t has, mom;
    uint8_t figs;                                   /* the page shows each knob's value as a figure (no flag: V0.75) */
    const sy_patch_t *gh;
    float live_lo, live_hi;                         /* the rows its live parts take (voices, levels): ui_live */
} pg;
static void ui_live(float y0, float y1)             /* a live part on rows y0 .. y1 (a live frame redraws only those) */
{
    pg.live_lo = sy_minf(pg.live_lo, y0);
    pg.live_hi = sy_maxf(pg.live_hi, y1);
}
static void ui_H(uint32_t k, float x, float y)      /* knob k's handle at (x, y), its box's corner (kept on the screen) */
{
    pg.hx[k] = sy_clampf(x, 2.0f, 228.0f);
    pg.hy[k] = sy_clampf(y, 18.0f, 206.0f);
    pg.has |= (uint8_t)(1u << k);
}
/* the curves a page computes (responses, envelopes), kept for its frame's other strips: a frame draws the page once
 * a strip, its values the same (in the spare SRAM: 2.6 KB) */
#define UI_NC 3u
#define UI_CN 217u
static float ui_cache[UI_NC][UI_CN] SY_SPARE;
static uint32_t ui_cache_at[UI_NC], ui_frame_n = 1u;
static float *ui_curve(uint32_t k, int *fresh)      /* curve k's columns; fresh: not yet this frame's (fill them) */
{
    *fresh = ui_cache_at[k] != ui_frame_n;
    ui_cache_at[k] = ui_frame_n;
    return ui_cache[k];
}
static int32_t ui_turning(void) { return ui.act_k < 4u && !ui.mod && !ui.lay ? (int32_t)ui.act_k : -1; }
static int ui_hot(uint32_t k) { return ui_turning() == (int32_t)k; }   /* knob k turning: what it moves HOT */
/* the values to dash for a shape the knob turning moves (moving: it moves it), else 0 */
static const sy_patch_t *ui_gh(int moving) { return pg.gh && (moving || pg.mom) ? pg.gh : 0; }
static uint16_t ui_ink(int hot) { return hot ? sk.hot : sk.ink; }

/* the section's top line: its name, its dot, where it is in its family */
#define UI_TITLE_X 26.0f                         /* (past the handles at x 8) */
static void ui_title(const char *s, float x)        /* a module's word at the top of its body: 14 px bold */
{
    lk_text(x, 24.0f, T14B, s, sk.ink, sk.ground, 0, 17);
}
static void ui_sub(float x, const char *a, const char *b)   /* .. under it: a word in LAB, a value in INK (10 px) */
{
    float e = lk_text(x, 42.0f, T10, a, sk.lab, sk.ground, 20, 0);
    if (b)
        lk_text(e + lk_tw(T10, " ", 20), 42.0f, T10B, b, sk.ink, sk.ground, 20, 0);
}

/* knob k's name, its value into v ("DETUNE", "35.0%"); 0: none, or SAVE's and the browser's (ui_save_flag) */
static const char *ui_knob_nv(uint32_t k, char *v)
{
    static const char *const EQPT[3] = {"LOW", "MID", "HIGH"};
    int32_t it = UI_PG[ui.page].id[k], id = ui_virt(it);
    const char *n = "";
    v[0] = 0;
    if (id < 0)
        return 0;
    if (id < UI_ITEM) {
        int32_t e = ui_eff(id);
        n = id >= SP_MAC1 && id <= SP_MAC4 ? sy_patch.macro[id - SP_MAC1]
            : it == UI_EQF ? "FREQ" : it == UI_EQG ? "GAIN" : it == UI_EQQ ? "Q" : SP[id].name;
        sp_text((uint32_t)e, sy_patch.p[e], v);
    } else if (id >= UI_LPT && id <= UI_LC) {
        uint32_t lk = ui_lfo(), i;
        sp_draw_t tp;
        const sp_draw_t *d = ui_pts(lk, &tp);
        i = ui_lpt(lk, d);
        n = id == UI_LPT ? "POINT" : id == UI_LX ? "TIME" : id == UI_LY ? "LEVEL" : "CURVE";
        if (id == UI_LPT)
            sp_fix(sp_put(sp_fix(v, (int32_t)i + 1, 0u, 0), " OF "), d->n, 0u, 0);
        else
            sp_put(sp_fix(v, id == UI_LX ? d->x[i] : id == UI_LY ? d->y[i] : d->c[i + 1u < d->n ? i : i - 1u], 1u,
                          id != UI_LX), "%");
    } else if (id == UI_BPM) {
        n = "BPM";
        sp_fix(v, (int32_t)ui_bpm_now(), 0u, 0);
    } else if (id == UI_CLOCK) {
        n = "CLOCK IN";
        sp_put(v, chd.clock ? "MIDI" : "INTERNAL");
    } else if (id == UI_TSB || id == UI_TSN) {
        n = "TIME SIG";
        sp_fix(sp_put(sp_fix(v, ui.tsb, 0u, 0), "/"), UI_TSN_VAL[ui.tsn], 0u, 0);
    } else if (id == UI_EQPT) {
        n = "POINT";
        sp_put(v, EQPT[ui.eqpt < 3u ? ui.eqpt : 0u]);
    } else if (id == UI_MSLOT) {
        n = "SLOT";
        sp_fix(v, (int32_t)ui.mslot + 1, 0u, 0);
    } else if (id == UI_SCALE) {
        n = "SCALE";
        sp_put(v, UI_SCALE_NAME[ui.scale % UI_NSCALE]);
    } else if (id == UI_ROOT) {
        n = "ROOT";
        sp_put(v, UI_NOTE_NAME[ui.root % 12u]);
    } else if (id == UI_FIT) {
        n = "FIT";
        sp_put(v, UI_FIT_NAME[ui.fit % UI_NFIT]);
    } else if (id == UI_KVEL) {
        n = "VELOCITY";
        sp_fix(v, ui.kvel, 0u, 0);
    } else if (id == UI_COLOR) {
        n = "COLOR";
        sp_put(v, SY_PAL[ui.pal].name);
    } else if (id == UI_HELP || id == UI_LEARN || id == UI_MPC || id == UI_ACCEL) {
        n = id == UI_HELP ? "HELP" : id == UI_LEARN ? "LEARN" : id == UI_MPC ? "PROGRAM" : "ACCEL";
        sp_put(v, (id == UI_HELP ? ui.help : id == UI_LEARN ? ui.learn : id == UI_MPC ? ui.mpc : ui.accel) ? "ON" : "OFF");
    } else if (id == UI_GLOW) {
        n = "GLOW";
        sp_put(v, ui.glow ? "LOW" : "HIGH");
    } else if (id == UI_HOLD) {
        n = "HOLD";
        sp_put(sp_fix(v, HOLD_MS[settings_hold % 4u], 0u, 0), " MS");
    } else if (id == UI_MCH) {
        n = "CHANNEL";
        if (ui.mch)
            sp_fix(v, ui.mch, 0u, 0);
        else
            sp_put(v, "ANY");
    } else if (id == UI_MCLR) {
        n = "CLEAR";
        sp_put(sp_fix(v, (int32_t)ml_count(), 0u, 0), " CC");
    } else if (id == UI_BRUN) {
        n = "RUN";
        sp_put(v, sy_bench.run == 1u ? "..." : "TIMED");
    } else if (id == UI_BCLR) {
        n = "PEAKS";
        sp_put(v, "CLEARED");
    } else if (id == UI_CTYPE) {
        n = "ADD";
        sp_put(v, FXT_NAMES[ui.ctype % FXT_N]);
    } else if (id == UI_CSLOT || id == UI_CMOVE) {
        uint32_t at = ui_cslot();
        n = id == UI_CSLOT ? "SLOT" : "MOVED TO";
        if (at < ui_cn())
            sp_fix(v, (int32_t)at + 1, 0u, 0);
        else
            sp_put(v, "THE END");
    } else if (id >= UI_HMODE && id <= UI_SSLD) {
        n = ui_chord_nv((uint32_t)id, v);           /* (SCL's chords, V0.7; ARP's, V0.75) */
    } else if (id >= UI_SCUR && id <= UI_BBANK) {
        return 0;                                   /* (SAVE's, the browser's: ui_save_flag) */
    }
    return n;
}
/* .. as its flag says it: "DETUNE 35.0%" */
static void ui_flag_text(uint32_t k, char *t)
{
    int32_t id = ui_virt(UI_PG[ui.page].id[k]);
    char v[SP_TEXT_N + 8];
    const char *n;
    t[0] = 0;
    if (id >= UI_SCUR && id <= UI_BBANK) {
        ui_save_flag(id, t);
        return;
    }
    n = ui_knob_nv(k, v);
    if (n)
        sp_put(sp_put(sp_put(t, n), n[0] ? " " : ""), v);
}

/* ------------------------------------------------------------------------------------------- HOME --- */
/* the bloom (V0.67): the scope's capture kept here (sy_engine.c sy_scope: three periods of the newest voice's note,
 * 212 points a side) sown as a sunflower's seeds round (166, 116): seed j (L's the even, R's the odd, the oldest
 * sample at the middle, the newest at the rim) at j golden angles and k sqrt(j) out, moved out or in by its sample
 * (2.5 px at the capture's peak, at most x4 for a quiet one), larger and brighter (MID to INK) the higher it is: a
 * sine's periods as rings, a saw's as a spiral's grain, a chord's as their interference; the newest HOT. It opens with the sound (the capture's level: a bud of 22 px in silence, 44 px at -6 dBFS; quick to
 * open, slow to close), swirls at each new note (the golden angle bent: the seeds wind into spiral arms, up the
 * keys one way, down them the other) and unwinds back into the sunflower in about a second, and turns once in 48
 * s. Each seed's angle from the last by a rotation, its radius by a reciprocal root: no tables; 424 dots, the
 * old trace's 424 segments */
#define UI_BX 166.0f
#define UI_BY 116.0f
#define UI_BLOOM_R 49.5f                            /* (the disc the live box sends: the bloom stays inside it, the */
                                                    /* ticks outside, from 50) */
static int16_t ui_scope[2][SY_SCOPE_N];
static uint8_t ui_scope_ok;
static float ui_spin;                               /* the bloom's turn (turns) */
static int16_t ui_open_q, ui_twist_q;               /* how open (0 .. 1), its swirl (-2.5 .. 2.5): x 4096 */
static uint32_t ui_bl;                              /* the newest note it swirled for: its age << 8, its key (0 .. 127);
                                                     * bit 7: the bloom still opening, closing or unwinding */
#define ui_open ((float)ui_open_q * (1.0f / 4096.0f))
#define ui_twist ((float)ui_twist_q * (1.0f / 4096.0f))

/* the capture's peak (0: none) */
static int32_t ui_scope_pk(void)
{
    uint32_t i;
    int32_t pk = 0;
    for (i = 0; ui_scope_ok && i < 2u * SY_SCOPE_N; i++) {
        int32_t v = ui_scope[i / SY_SCOPE_N][i % SY_SCOPE_N];
        pk = v > pk ? v : -v > pk ? -v : pk;
    }
    return pk;
}
/* the bloom's motion over dt ms (snd: sound playing): a new note's swirl, the level it opens to, the swirl
 * unwinding, the turn; 1 while it still moves */
static int ui_bloom_step(uint32_t dt, int snd)
{
    const sy_voice_t *nv = 0;
    float d = (float)dt, tgt = 0.0f, op = ui_open, tw = ui_twist;
    uint32_t i;
    int32_t pk = snd ? ui_scope_pk() : 0;
    int mv;
    for (i = 0; i < SY_NVOICE; i++)
        if (sy_v[i].on && (!nv || sy_v[i].age > nv->age))
            nv = &sy_v[i];
    if (nv && (nv->age & 0xFFFFFFu) != ui_bl >> 8) {   /* (up the keys: one way; down: the other) */
        uint32_t key = (uint32_t)sy_clampi(sy_floori(nv->target + 0.5f), 0, 127);
        tw = sy_clampf(tw + (key >= (ui_bl & 127u) ? 1.0f : -1.0f), -2.5f, 2.5f);
        ui_bl = (nv->age & 0xFFFFFFu) << 8 | key;
    }
    if (pk > 2048)                                  /* (-42 dBFS: a bud; -6: open) */
        tgt = sy_clampf((6.0206f * sy_log2f((float)pk * (1.0f / 32768.0f)) + 42.0f) * (1.0f / 36.0f), 0.0f, 1.0f);
    op += (tgt - op) * (1.0f - sy_exp2f(-d * 1.4427f / (tgt > op ? 70.0f : 450.0f)));
    tw *= sy_exp2f(-d * 1.4427f / 600.0f);
    ui_spin += d * (1.0f / 48000.0f + 0.00004f * tw);
    ui_spin -= (float)sy_floori(ui_spin);
    ui_open_q = (int16_t)sy_floori(op * 4096.0f + 0.5f);
    ui_twist_q = (int16_t)sy_floori(tw * 4096.0f + (tw < 0.0f ? -0.5f : 0.5f));
    mv = sy_absf(tw) > 0.02f || sy_absf(tgt - op) > 0.01f;
    ui_bl = (ui_bl & ~128u) | (uint32_t)mv << 7;
    return mv;
}

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

static void ui_bloom(void)
{
    uint32_t j;
    int32_t pk = ui_scope_pk();
    float g = sy_minf(1.0f / (float)(pk > 2048 ? pk : 2048), 4.0f / 32768.0f), ga = 0.381966f + 0.0042f * ui_twist;
    float k = (22.0f + 22.0f * ui_open) * (1.0f / 20.58f), sw = 0.4f + 0.6f * ui_open;   /* (the last seed: 22 .. 44 px) */
    float cs = sy_cos2pi(ga), sn = sy_sin2pi(ga), vx = sy_cos2pi(ui_spin), vy = sy_sin2pi(ui_spin), x = UI_BX, y = UI_BY;
    for (j = 0; j < 2u * SY_SCOPE_N; j++) {         /* seed j: L's sample j / 2 (j even), R's (odd) */
        float jj = (float)j + 0.5f, v = ui_scope_ok ? (float)ui_scope[j & 1u][j >> 1] * g : 0.0f, u = 0.5f + 0.5f * v;
        float r = k * jj * lk_rsqrt(jj) + 2.5f * v * sw, nx;
        x = UI_BX + r * vx;
        y = UI_BY + r * vy;
        lk_disc(x, y, 0.55f + 0.95f * u * sw, sy_mix565(sk.mid, sk.ink, (uint32_t)(u * 256.0f)));
        nx = vx * cs - vy * sn;
        vy = vx * sn + vy * cs;
        vx = nx;
    }
    lk_disc(x, y, 2.4f, sk.hot);                    /* the newest sample: the rim's */
}

/* the tempo's dot on HOME's top line: HOT, pulsing on the beat while PLAY runs or sound plays (its level in
 * eighths); still otherwise (silence: nothing redrawn) */
static uint32_t ui_pulse_lv(void)
{
    uint32_t beat = 60000u / (ui_bpm_now() ? ui_bpm_now() : 120u);
    float ph = ui.run ? ui_beats() - (float)sy_floori(ui_beats()) : (float)(fm1_ms % beat) / (float)beat;
    if (!ui.run && !ui_sounding())
        return 8u;
    return (uint32_t)sy_floori(8.0f * (1.0f - 0.7f * (0.5f - 0.5f * sy_cos2pi(ph))) + 0.5f);
}
static float ui_pulse_x(void)
{
    char t[16];
    sp_put(sp_fix(t, (int32_t)ui_bpm_now(), 0u, 0), " BPM");
    return 232.0f - lk_tw(T10B, t, 30) - 7.0f;
}
static void ui_pulse(void) { lk_disc(ui_pulse_x(), 9.0f, 2.0f, sy_mix565(sk.ground, sk.hot, ui_pulse_lv() * 32u)); }

/* a macro's dial at (cx, cy), r 14, at v (0..1): its track, its value, the pointer, the inner ring */
static void ui_dial(float cx, float cy, float v, int hot)
{
    float a = 0.375f + 0.75f * v;
    lk_dial_arc(cx, cy, 14.0f, 3.0f, 0.0f, 1.0f, sk.line);
    lk_dial_arc(cx, cy, 14.0f, 3.0f, 0.0f, sy_maxf(v, 0.008f), ui_ink(hot));
    lk_seg(cx, cy, cx + 8.0f * sy_cos2pi(a), cy + 8.0f * sy_sin2pi(a), 1.5f, ui_ink(hot));
    lk_ring(cx, cy, 8.0f, 1.0f, sk.mid);
}

static void ui_p_home(void)
{
    static const char *const ROW[3] = {"TABLE", "UNISON", "FILTER"};
    uint32_t i, num, cnt, nl;
    const char *bank = sy_app_sound_bank(&num, &cnt), *nm = sy_patch.name;
    const wt_table_t *wt = &wt_tab[0];
    char t[40], v[32], *e;
    for (nl = 0; nm[nl]; nl++)
        ;
    nl = nl <= 8u ? T26B : nl <= 12u ? T18B : T14B; /* (the name's size by its length) */
    lk_text(8.0f, 18.0f, nl, lk_fit(t, sizeof t, nm, nl, -20, 224.0f), sk.ink, sk.ground, -20, 28);
    e = sp_put(sp_put(t, CAT_NAMES[sy_patch.cat < CAT_COUNT ? sy_patch.cat : CAT_OTHER]), " \xB7 ");
    e = sp_put(e, bank);
    if (num) {
        e = sp_put(e, " ");
        if (num < 10u)
            e = sp_put(e, "0");
        sp_fix(e, (int32_t)num, 0u, 0);
    }
    lk_text(9.0f, 50.0f, T10, t, sk.lab, sk.ground, 20, 0);
    for (i = 0; i < 3u; i++) {                      /* TABLE, UNISON, FILTER: what the sound is made of, a word over */
        float y = 72.0f + (float)(i * 27u);         /* its value (the bloom's ticks are from x 110) */
        if (i == 0u) {
            float cur = (float)sy_patch.p[SP_A_POS] * 0.001f * (float)(wt->n > 1u ? wt->n - 1u : 0u);
            sp_fix(sp_put(sp_put(v, WT_NAMES[wt->id]), " "), sy_floori((cur + 1.0f) * 10.0f + 0.5f), 1u, 0);
        } else if (i == 1u) {
            e = sp_put(sp_fix(v, sy_patch.p[SP_A_UNI], 0u, 0), " \xB7 ");
            sp_put(sp_fix(e, (sy_patch.p[SP_A_DETUNE] + 5) / 10, 0u, 0), "%");
        } else if (sy_patch.p[SP_F_ON]) {             /* "LP24 2.4K": the type without its space, the cutoff short */
            float hz = sp_cut_hz(ui_v(sy_patch.p, SP_F_CUT));
            const char *ft = FT_NAMES[(uint32_t)sy_patch.p[SP_F_TYPE] % FT_COUNT];
            for (e = v; *ft; ft++)
                if (*ft != ' ')
                    *e++ = *ft;
            *e++ = ' ';
            if (hz < 999.5f)
                sp_fix(e, sy_floori(hz + 0.5f), 0u, 0);
            else
                sp_put(sp_fix(e, sy_floori(hz * 0.01f + 0.5f), 1u, 0), "K");
        } else {
            sp_put(v, "OFF");
        }
        lk_text(9.0f, y, T9, ROW[i], sk.lab, sk.ground, 40, 0);
        lk_text(9.0f, y + 11.0f, T10B, lk_fit(t, sizeof t, v, T10B, 0, 96.0f), sk.ink, sk.ground, 0, 0);
    }
    {                                               /* the dial's 48 ticks round the bloom (the top, and each 90 */
        float vx = 1.0f, vy = 0.0f, cs = sy_cos2pi(1.0f / 48.0f), sn = sy_sin2pi(1.0f / 48.0f);   /* degrees, longer) */
        for (i = 0; i < 48u; i++) {
            float r0 = i % 12u ? 51.5f : 50.0f, nx;
            lk_seg(UI_BX + r0 * vx, UI_BY + r0 * vy, UI_BX + 55.5f * vx, UI_BY + 55.5f * vy, 1.0f, i % 12u ? sk.line : sk.mid);
            nx = vx * cs - vy * sn;
            vy = vx * sn + vy * cs;
            vx = nx;
        }
    }
    ui_bloom();
    for (i = 0; i < 4u; i++) {                      /* the macros */
        float cx = 36.0f + (float)(i * 56u);
        uint32_t id = SP_MAC1 + i;
        ui_dial(cx, 190.0f, ui_v(sy_patch.p, id), ui_hot(i));
        if (!ui.toast[0]) {
            lk_text_c(cx, 205.0f, T9, lk_fit(t, sizeof t, sy_patch.macro[i], T9, 20, 54.0f), sk.lab, sk.ground, 20, 0);
            sp_put(sp_fix(t, (sy_patch.p[id] + 5) / 10, 0u, 0), "%");
            lk_text_c(cx, 217.0f, T10B, t, ui_ink(ui_hot(i)), sk.ground, 0, 12);
        }
    }
}

/* ----------------------------------------------------------------------------------------- EDIT --- */
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
/* table t at frame position cur (between two frames) at phase p */
static float ui_wt_at(const wt_table_t *t, float cur, float p)
{
    uint32_t k0 = (uint32_t)sy_floori(cur), k1 = k0 + 1u < t->n ? k0 + 1u : k0;
    float a = ui_wave_at(wt_frame(t, k0, 2u), p);
    return a + (ui_wave_at(wt_frame(t, k1, 2u), p) - a) * (cur - (float)k0);
}
/* the phases a warp reads across n columns (sy_osc.c osc_warp_q: what the oscillator plays; FM is no map: as it is) */
static void ui_warp_cols(uint32_t mode, float amt, float *q, uint32_t n)
{
    uint32_t j;
    float slope;
    for (j = 0; j < n; j++)
        q[j] = mode == WARP_OFF || mode == WARP_FM ? (float)j / (float)(n - 1u)
                                                   : osc_warp_q(mode, amt, (float)j / (float)(n - 1u), &slope);
}

/* a source turned off: its name, how it comes back; nothing else of it but a faint drawing (no handles) */
static void ui_off(const char *what)
{
    char t[24];
    sp_put(sp_put(t, what), " IS OFF");
    lk_text(20.0f, 26.0f, T14B, t, sk.ink, sk.ground, 0, 14);
    lk_text(20.0f, 42.0f, T10, "EDIT + OCT+ TURNS IT ON", sk.lab, sk.ground, 60, 0);
}

static float ui_frame_norm(const wt_table_t *t, float cur);   /* (below) */

/* OSC WAVE: the table as strata (16 of its frames, each through the warp as it plays, the ones near the frame playing
 * in MID), the frame playing bold (between two), a small bloom of it, the warp's amount */
#define UI_WN 105u                                  /* a frame's columns */
static void ui_p_wave(void)
{
    uint32_t o = ui_osc(), b = UI_OB(o), n, k, j, nk, mode = (uint32_t)sy_patch.p[SP_A_WARP + b];
    const wt_table_t *t = &wt_tab[o];
    const sy_patch_t *gh = ui_gh(ui_hot(2) || ui_hot(3));
    float q[UI_WN], ys[UI_WN], amt = ui_v(sy_patch.p, SP_A_WAMT + b), cur, kc, x0c, bc, ga;
    int on = sy_patch.p[SP_A_ON + b] != 0;
    char w[32];
    if (t->busy || !t->n) {
        lk_text(20.0f, 100.0f, T10, "BUILDING THE TABLE", sk.lab, sk.ground, 80, 0);
        return;
    }
    n = t->n;
    nk = n < 16u ? n : 16u;
    ui_warp_cols(mode, amt, q, UI_WN);
    cur = ui_v(sy_patch.p, SP_A_POS + b) * (float)(n - 1u);
    kc = n > 1u ? cur * 15.0f / (float)(n - 1u) : 0.0f;
    ga = 12.0f * sy_minf(ui_frame_norm(t, cur), 4.0f);   /* (the frames at the level of the one playing: 12 px, x4 at */
                                                    /* most; neighbours overlap, as strata do) */
    for (k = on ? nk : (nk * 10u + 15u) / 16u; k-- > 0;) {   /* back to front (off: the nearer ten, faint) */
        float dk = nk > 1u ? (float)k * 15.0f / (float)(nk - 1u) : 0.0f, base = 170.0f - dk * 7.0f;
        int32_t x0 = sy_floori(20.0f + dk * 4.4f + 0.5f);
        const int16_t *f = wt_frame(t, nk > 1u ? (uint32_t)sy_floori(dk * (float)(n - 1u) / 15.0f + 0.5f) : 0u, 2u);
        if (!lk_vis((float)x0, base - 9.0f, (float)x0 + 105.0f, base + 9.0f))
            continue;
        for (j = 0; j < UI_WN; j++)
            ys[j] = base - ui_wave_at(f, q[j]) * ga;
        lk_curve(x0, ys, UI_WN, 1.0f, on && sy_absf(dk - kc) <= 3.0f ? sk.mid : sk.line, 0);
    }
    if (!on) {
        ui_off(SEC_NAMES[ui_sec()]);
        return;
    }
    x0c = 20.0f + kc * 4.4f;
    bc = 170.0f - kc * 7.0f;
    if (gh) {                                       /* (before the turn) */
        float g[UI_WN];
        ui_warp_cols((uint32_t)gh->p[SP_A_WARP + b], ui_v(gh->p, SP_A_WAMT + b), g, UI_WN);
        for (j = 0; j < UI_WN; j++)
            ys[j] = bc - ui_wt_at(t, cur, g[j]) * ga;
        lk_curve(sy_floori(x0c + 0.5f), ys, UI_WN, 1.0f, sk.mid, LK_DASH(2, 2));
    }
    for (j = 0; j < UI_WN; j++)                     /* the frame playing */
        ys[j] = bc - ui_wt_at(t, cur, q[j]) * ga;
    lk_curve(sy_floori(x0c + 0.5f), ys, UI_WN, 2.2f, ui_ink(ui_hot(0)), 0);
    if (ui_nvoices())                               /* (sounding: a HOT dot on it) */
        lk_disc((float)sy_floori(x0c + 0.5f) + 73.5f, ys[73], 2.5f, sk.hot);
    ui_H(0, (float)sy_floori(x0c + 0.5f) + 52.5f - 5.0f, ys[52] - 5.0f);
    if (!ui.learn) {                                /* a small bloom of the frame playing (the banner: none) */
        float vx = 0.0f, vy = -1.0f, cs = sy_cos2pi(1.0f / 120.0f), sn = sy_sin2pi(1.0f / 120.0f), px = 0.0f, py = 0.0f;
        float mx = 0.02f;
        lk_ring_dash(200.0f, 30.0f, 16.0f, 1.0f, 3.0f, sk.line);
        for (j = 0; j < UI_WN; j++)
            mx = sy_maxf(mx, sy_absf(bc - ys[j]) / ga);
        for (j = 0; j <= 120u; j++) {
            float r = 10.0f + 3.0f * ui_wt_at(t, cur, q[(j * 3u % 120u) * (UI_WN - 1u) / 120u]) / mx, x, y, nx;
            x = 200.0f + r * vx;
            y = 30.0f + r * vy;
            if (j)
                lk_wu(px, py, x, y, 1.0f, sk.ink);
            px = x;
            py = y;
            nx = vx * cs - vy * sn;
            vy = vx * sn + vy * cs;
            vx = nx;
        }
    }
    {                                               /* AMOUNT: the warp's, up from 170 */
        float yt = 170.0f - 112.0f * amt;
        lk_seg(222.0f, 58.0f, 222.0f, 170.0f, 3.0f, sk.line);
        if (amt > 0.004f)
            lk_seg(222.0f, yt, 222.0f, 170.0f, 3.0f, ui_ink(ui_hot(3)));
        lk_label(211.0f, 176.0f, "AMT", 60);
        ui_H(3, 217.0f, yt - 16.0f);
    }
    ui_title(WT_NAMES[t->id], 26.0f);
    sp_put(w, WARP_NAMES[mode < WARP_COUNT ? mode : 0]);
    ui_sub(26.0f, "WARP", w);
    ui_H(1, 8.0f, 27.0f);
    ui_H(2, 8.0f, 43.0f);
}

/* the 7 waves' frame: the oscillator's playing, normalised (UNISON, SPREAD) */
static float ui_frame_norm(const wt_table_t *t, float cur)
{
    uint32_t j;
    float m = 0.02f;
    for (j = 0; j < 64u; j++)
        m = sy_maxf(m, sy_absf(ui_wt_at(t, cur, (float)j / 64.0f)));
    return 1.0f / m;
}

/* OSC UNISON: the voices fanned like cards (DETUNE their spread, BLEND the side ones' weight), the bracket of the
 * detune, the stereo WIDTH under them */
static void ui_p_unison(void)
{
    uint32_t o = ui_osc(), b = UI_OB(o), nv = (uint32_t)sy_patch.p[SP_A_UNI + b], umode = (uint32_t)sy_patch.p[SP_A_UMODE + b];
    uint32_t i, j;
    const wt_table_t *t = &wt_tab[o];
    float det = ui_v(sy_patch.p, SP_A_DETUNE + b), blend = ui_v(sy_patch.p, SP_A_BLEND + b), wid = ui_v(sy_patch.p, SP_A_WIDTH + b);
    float ys[121], cur, g, half = 16.0f + 70.0f * det, lx;
    char w[24], v[16];
    if (!sy_patch.p[SP_A_ON + b]) {
        ui_off(SEC_NAMES[ui_sec()]);
        return;
    }
    if (!t->busy && t->n) {
        cur = ui_v(sy_patch.p, SP_A_POS + b) * (float)(t->n - 1u);
        g = ui_frame_norm(t, cur) * 20.0f;
        for (i = 0; i < nv; i++) {                  /* the side voices first, the middle one last, bold */
            float q = nv > 1u ? (float)(2u * i) / (float)(nv - 1u) - 1.0f : 0.0f, dx, dy;
            int mid = nv & 1u ? i == nv / 2u : 0;
            if (umode)
                q *= 0.3f + 0.7f * q * q;
            if (mid || sy_absf(q) < 0.001f)
                continue;
            dx = q * (6.0f + 26.0f * det);
            dy = -q * 6.0f;
            for (j = 0; j < 121u; j++)
                ys[j] = 112.0f + dy - ui_wt_at(t, cur, (float)j / 120.0f) * g;
            lk_curve(sy_floori(54.0f + dx + 0.5f), ys, 121u, 1.0f,
                     sy_absf(q) > 0.85f || blend < 0.25f ? sk.line : sk.mid, 0);
        }
        for (j = 0; j < 121u; j++)
            ys[j] = 112.0f - ui_wt_at(t, cur, (float)j / 120.0f) * g;
        lk_curve(54, ys, 121u, 2.2f, ui_ink(ui_hot(0)), 0);
    }
    {                                               /* DETUNE's bracket */
        uint16_t c = ui_hot(1) ? sk.hot : sk.mid;
        lk_seg(114.0f - half, 160.0f, 114.0f + half, 160.0f, ui_hot(1) ? 1.5f : 1.0f, c);
        lk_seg(114.0f - half, 156.0f, 114.0f - half, 164.0f, ui_hot(1) ? 1.5f : 1.0f, c);
        lk_seg(114.0f + half, 156.0f, 114.0f + half, 164.0f, ui_hot(1) ? 1.5f : 1.0f, c);
        ui_H(1, 109.0f, 166.0f);
    }
    lk_seg(32.0f, 197.0f, 208.0f, 197.0f, 1.0f, sk.line);   /* WIDTH: the stereo field */
    lx = 120.0f - 86.0f * wid;
    if (wid > 0.01f)
        lk_seg(lx, 197.0f, 240.0f - lx, 197.0f, 1.5f, ui_ink(ui_hot(3)));
    lk_text(20.0f, 192.0f, T9, "L", sk.lab, sk.ground, 0, 0);
    lk_text(214.0f, 192.0f, T9, "R", sk.lab, sk.ground, 0, 0);
    ui_H(3, sy_maxf(lx, 34.0f) - 5.0f, 184.0f);
    sp_put(sp_fix(w, (int32_t)nv, 0u, 0), nv == 1u ? " VOICE" : " VOICES");
    ui_title(w, UI_TITLE_X);
    sp_put(sp_put(w, UMODE_NAMES[umode & 1u]), " \xB7 BLEND");
    sp_put(sp_fix(v, (sy_patch.p[SP_A_BLEND + b] + 5) / 10, 0u, 0), "%");
    ui_sub(UI_TITLE_X, w, v);
    ui_H(0, 8.0f, 27.0f);
    ui_H(2, 8.0f, 43.0f);
}

/* OSC SPREAD: each voice's own frame (WT SPRD: across the table), the phase clock (each voice's start: PHASE where
 * they start, RAND their scatter) */
static void ui_p_spread(void)
{
    uint32_t o = ui_osc(), b = UI_OB(o), i, j, umode = (uint32_t)sy_patch.p[SP_A_UMODE + b];
    const wt_table_t *t = &wt_tab[o];
    float spr = ui_v(sy_patch.p, SP_A_SPREAD + b), rnd = ui_v(sy_patch.p, SP_A_RAND + b);
    float ph = (float)sy_patch.p[SP_A_PHASE + b] * (1.0f / 360.0f), ys[101];
    uint32_t seed = 7u;
    char w[SP_TEXT_N + 8];
    if (!sy_patch.p[SP_A_ON + b]) {
        ui_off(SEC_NAMES[ui_sec()]);
        return;
    }
    if (!t->busy && t->n)
        for (i = 0; i < 7u; i++) {                  /* the frames the seven voices read */
            float cur = sy_clampf(ui_v(sy_patch.p, SP_A_POS + b) + ((float)i - 3.0f) / 3.0f * 0.5f * spr, 0.0f, 1.0f) *
                        (float)(t->n - 1u), g = 6.0f * ui_frame_norm(t, cur), y = 72.0f + (float)(i * 16u);
            if (!lk_vis(24.0f, y - 8.0f, 125.0f, y + 8.0f))
                continue;
            for (j = 0; j < 101u; j++)
                ys[j] = y - ui_wt_at(t, cur, (float)j / 100.0f) * g;
            lk_curve(24, ys, 101u, i == 3u ? 1.5f : 1.0f, i == 3u ? ui_ink(ui_hot(1)) : sk.mid, 0);
        }
    lk_seg(136.0f, 64.0f, 140.0f, 64.0f, 1.0f, sk.mid);     /* the bracket: the table's span the voices read */
    lk_seg(140.0f, 64.0f, 140.0f, 176.0f, 1.0f, sk.mid);
    lk_seg(140.0f, 176.0f, 136.0f, 176.0f, 1.0f, sk.mid);
    lk_ring(184.0f, 122.0f, 30.0f, 1.0f, sk.line);          /* the phase clock */
    lk_seg(184.0f, 88.0f, 184.0f, 94.0f, 1.0f, sk.mid);
    for (i = 0; i < 7u; i++) {
        float j1 = (float)(sy_rand(&seed) >> 8) * (1.0f / 16777216.0f) - 0.5f;
        float j2 = (float)(sy_rand(&seed) >> 8) * (1.0f / 16777216.0f) - 0.5f;
        float a = ph - 0.25f + ((float)i - 3.0f) * (22.0f / 360.0f) + j1 * rnd * (60.0f / 360.0f), r = 30.0f + j2 * 6.0f * rnd;
        float x = 184.0f + r * sy_cos2pi(a), y = 122.0f + r * sy_sin2pi(a);
        if (i == 3u)
            lk_disc(x, y, 2.3f, ui_ink(ui_hot(2)));
        else
            lk_hollow(x, y, 2.3f);
    }
    ui_title(UMODE_NAMES[umode & 1u], UI_TITLE_X);
    sp_text(SP_A_SPREAD + b, sy_patch.p[SP_A_SPREAD + b], w);
    ui_sub(UI_TITLE_X, "WT SPREAD", w);
    lk_label(170.0f, 162.0f, "PHASE", 20);
    ui_H(0, 8.0f, 27.0f);
    ui_H(1, 143.0f, 114.0f);
    ui_H(2, 179.0f, 70.0f);
    ui_H(3, 220.0f, 134.0f);
}

/* OSC PITCH: the octave on its ladder, the semitone on an octave of keys (from C to the dot), the cents on a gauge,
 * the phase mode (RETRIG: each note from the start; FREE) */
static void ui_p_pitch(void)
{
    uint32_t o = ui_osc(), b = UI_OB(o), i;
    int32_t oct = sy_patch.p[SP_A_OCT + b], semi = sy_patch.p[SP_A_SEMI + b], pc = (semi % 12 + 12) % 12;
    float fine = (float)sy_patch.p[SP_A_FINE + b], na = 0.75f + (fine / 100.0f) * (70.0f / 360.0f);
    int free = sy_patch.p[SP_A_PHMODE + b] != 0;
    static const int8_t WK[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};   /* C .. B: a white key's index */
    float kx;
    if (!sy_patch.p[SP_A_ON + b]) {
        ui_off(SEC_NAMES[ui_sec()]);
        return;
    }
    for (i = 0; i < 9u; i++) {                      /* the ladder, -4 .. +4 */
        int32_t k = (int32_t)i - 4;
        float y = 100.0f - (float)k * 15.0f;
        lk_seg(22.0f, y, 38.0f, y, 3.0f, k == oct ? ui_ink(ui_hot(0)) : sk.line);
    }
    lk_seg(22.0f, 34.0f, 22.0f, 166.0f, 1.0f, sk.line);
    lk_seg(38.0f, 34.0f, 38.0f, 166.0f, 1.0f, sk.line);
    lk_text(43.0f, 35.0f, T9, "+4", sk.lab, sk.ground, 0, 0);
    lk_text(43.0f, 95.0f, T9, "0", sk.lab, sk.ground, 0, 0);
    lk_text(43.0f, 155.0f, T9, "-4", sk.lab, sk.ground, 0, 0);
    for (i = 0; i < 7u; i++)                        /* an octave of keys, C .. B */
        lk_frame(78.0f + (float)(i * 20u), 46.0f, 20.0f, 54.0f, 1.0f, sk.ink);
    for (i = 0; i < 5u; i++) {
        static const uint8_t BK[5] = {0, 1, 3, 4, 5};
        float x = 78.0f + (float)((BK[i] + 1u) * 20u);
        lk_box(x - 6.5f, 45.5f, x + 6.5f, 78.5f, sk.ink);
    }
    lk_hollow(88.0f, 92.0f, 2.5f);                  /* C, and the semitone's key */
    if (WK[pc] >= 0) {
        kx = 78.0f + (float)(WK[pc] * 20) + 10.0f;
        lk_disc(kx, 92.0f, 2.5f, ui_ink(ui_hot(1)));
    } else {
        kx = 78.0f + (float)((WK[pc - 1] + 1) * 20);
        lk_disc(kx, 70.0f, 2.5f, ui_hot(1) ? sk.hot : sk.ground);
    }
    if (pc)
        lk_arrow(88.0f, 40.0f, kx, 40.0f, 3.0f, 1.5f, ui_ink(ui_hot(1)));
    if (semi < 0 || semi >= 12) {                   /* (past the octave: which way) */
        char t[8];
        sp_fix(t, semi < 0 ? -12 : 12, 0u, 1);
        lk_text(kx < 150.0f ? 196.0f : 60.0f, 104.0f, T9, t, sk.lab, sk.ground, 0, 0);
    }
    lk_arc(100.0f, 182.0f, 30.0f, 1.0f, 200.0f / 360.0f, 340.0f / 360.0f, sk.line, 0);   /* FINE: the gauge */
    lk_seg(100.0f, 152.0f, 100.0f, 157.0f, 1.0f, sk.mid);
    lk_seg(100.0f, 182.0f, 100.0f + 26.0f * sy_cos2pi(na), 182.0f + 26.0f * sy_sin2pi(na), 1.5f, ui_ink(ui_hot(2)));
    lk_disc(100.0f, 182.0f, 2.5f, ui_ink(ui_hot(2)));
    lk_text_r(68.0f, 170.0f, T9, "-100", sk.lab, sk.ground, 0, 0);
    lk_text(132.0f, 170.0f, T9, "+100", sk.lab, sk.ground, 0, 0);
    {                                               /* PH MODE: two boxes, the one it is in INK, its name under */
        float ys[25];
        lk_frame(160.5f, 142.5f, 32.0f, 28.0f, 1.0f, free ? ui_ink(ui_hot(3)) : sk.line);
        lk_frame(200.5f, 142.5f, 32.0f, 28.0f, 1.0f, free ? sk.line : ui_ink(ui_hot(3)));
        for (i = 0; i < 19u; i++)
            ys[i] = 156.5f - 6.0f * sy_sin2pi((float)i / 18.0f + 0.3f);
        lk_curve(168, ys, 19u, 1.0f, free ? sk.ink : sk.mid, 0);
        for (i = 0; i < 19u; i++)
            ys[i] = 156.5f - 6.0f * sy_sin2pi((float)i / 18.0f);
        lk_curve(208, ys, 19u, 1.0f, free ? sk.mid : sk.ink, 0);
        lk_seg(208.0f, 148.5f, 208.0f, 164.5f, 1.0f, free ? sk.mid : sk.ink);
        lk_text_c(176.5f, 174.0f, free ? T9B : T9, "FREE", free ? sk.ink : sk.lab, sk.ground, 0, 0);
        lk_text_c(216.5f, 174.0f, free ? T9 : T9B, "RETRIG", free ? sk.lab : sk.ink, sk.ground, 0, 0);
    }
    ui_H(0, 56.0f, 100.0f - (float)oct * 15.0f - 5.0f);   /* (past the ladder's numbers) */
    ui_H(1, kx - 5.0f, 21.0f);
    ui_H(2, 95.0f, 134.0f);
    ui_H(3, 199.0f, 125.0f);
}

/* OUTPUT (OSC, SUB, NOISE): LEVEL (the oscillators': a fader), PAN on its arc, ROUTE (through the filter, or around
 * it: the other way dashed) */
static void ui_p_out(void)
{
    uint32_t sec = ui_sec(), k = 0;
    int32_t lv = sec == SEC_OSCA ? SP_A_LEVEL : sec == SEC_OSCB ? SP_B_LEVEL : -1;
    int32_t pan = sec == SEC_SUB ? SP_SUB_PAN : sec == SEC_NOISE ? SP_NZ_PAN : sec == SEC_OSCB ? SP_B_PAN : SP_A_PAN;
    int32_t rt = sec == SEC_SUB ? SP_SUB_ROUTE : sec == SEC_NOISE ? SP_NZ_ROUTE : sec == SEC_OSCB ? SP_B_ROUTE : SP_A_ROUTE;
    int dir = sy_patch.p[rt] != 0;
    float pa = 0.75f + (float)sy_patch.p[pan] * 0.001f * (70.0f / 360.0f);
    uint16_t on, off;
    char src[12];
    if (ui_sec_power(sec) >= 0 && !sy_patch.p[ui_sec_power(sec)]) {
        ui_off(SEC_NAMES[sec]);
        return;
    }
    if (lv >= 0) {
        float y = lk_fader(30.0f, 46.0f, 180.0f, ui_v(sy_patch.p, (uint32_t)lv));
        ui_H(k++, 42.0f, y - 5.0f);
    }
    lk_arc(130.0f, 92.0f, 40.0f, 1.0f, 200.0f / 360.0f, 340.0f / 360.0f, sk.line, 0);
    lk_seg(130.0f, 49.0f, 130.0f, 55.0f, 1.0f, sk.mid);
    lk_disc(130.0f + 40.0f * sy_cos2pi(pa), 92.0f + 40.0f * sy_sin2pi(pa), 3.0f, ui_ink(ui_hot(k)));
    lk_text(84.0f, 84.0f, T9, "L", sk.lab, sk.ground, 0, 0);
    lk_text(172.0f, 84.0f, T9, "R", sk.lab, sk.ground, 0, 0);
    ui_H(k++, 125.0f, 30.0f);
    on = ui_ink(ui_hot(k));
    off = sk.mid;
    lk_frame(60.5f, 138.5f, 43.0f, 17.0f, 1.0f, sk.ink);    /* the source, the filter, around it */
    if (dir) {
        lk_frame_dash(148.5f, 128.5f, 55.0f, 15.0f, 2.0f, 2.0f, off);
        lk_frame(148.5f, 160.5f, 55.0f, 15.0f, 1.0f, on);
    } else {
        lk_frame(148.5f, 128.5f, 55.0f, 15.0f, 1.0f, on);
        lk_frame_dash(148.5f, 160.5f, 55.0f, 15.0f, 2.0f, 2.0f, off);
    }
    {
        static const float TOP[10] = {104, 147, 120, 147, 136, 136, 148, 136, 0, 0};
        static const float BOT[10] = {104, 147, 120, 147, 136, 168, 148, 168, 0, 0};
        float a[8], c[8];
        uint32_t i;
        for (i = 0; i < 8u; i++) {
            a[i] = TOP[i];
            c[i] = BOT[i];
        }
        if (dir) {
            lk_poly_dash(a + 2, 3u, 1.0f, off, 2.0f, 2.0f);
            lk_poly(c, 4u, 1.5f, on);
        } else {
            lk_poly(a, 4u, 1.5f, on);
            lk_poly_dash(c + 2, 3u, 1.0f, off, 2.0f, 2.0f);
        }
        {
            float ta[6] = {204, 136, 212, 136, 226, 147}, tb[6] = {204, 168, 212, 168, 226, 147};
            if (dir) {
                lk_poly_dash(ta, 3u, 1.0f, off, 2.0f, 2.0f);
                lk_poly(tb, 3u, 1.5f, on);
            } else {
                lk_poly(ta, 3u, 1.5f, on);
                lk_poly_dash(tb, 3u, 1.0f, off, 2.0f, 2.0f);
            }
        }
    }
    lk_disc(226.0f, 147.0f, 2.5f, sk.ink);
    sp_put(src, SEC_NAMES[sec]);
    lk_text(64.0f, 142.0f, T9B, src, sk.ink, sk.ground, 0, 0);
    lk_text(152.0f, 131.0f, dir ? T9 : T9B, "FILTER", dir ? sk.lab : sk.ink, sk.ground, 0, 0);
    lk_text(152.0f, 163.0f, dir ? T9B : T9, "DIRECT", dir ? sk.ink : sk.lab, sk.ground, 0, 0);
    lk_label(213.0f, 120.0f, "OUT", 0);
    ui_H(k, 77.0f, 162.0f);
}

/* the sub's shape at phase x (sy_voice.c's: SINE TRIANGLE SAW SQUARE PULSE ROUND), its width w (0..1) */
static float ui_sub_at(uint32_t shape, float w, float x)
{
    x -= (float)sy_floori(x);
    switch (shape) {
    case 1: return x < 0.5f ? 4.0f * x - 1.0f : 3.0f - 4.0f * x;
    case 2: return 2.0f * x - 1.0f;
    case 3: return x < 0.5f + 0.45f * w ? 1.0f : -1.0f;
    case 4: return x < 0.25f * (1.0f - 0.9f * w) ? 1.0f : -1.0f;
    case 5: return lk_tanh(3.0f * sy_sin2pi(x)) * 1.0049f;
    default: return sy_sin2pi(x);
    }
}

/* SUB: its shape under the oscillator (an octave or more down, the cycles it makes against the oscillator's two),
 * its LEVEL, its WIDTH */
static void ui_p_sub(void)
{
    uint32_t i, shape = (uint32_t)sy_patch.p[SP_SUB_SHAPE];
    int32_t oct = sy_patch.p[SP_SUB_OCT];
    const wt_table_t *t = &wt_tab[0];
    float ys[161], cyc = 2.0f * sy_exp2f((float)oct), pw = ui_v(sy_patch.p, SP_SUB_PW), y;
    char w[16];
    if (!sy_patch.p[SP_SUB_ON]) {
        ui_off("SUB");
        return;
    }
    if (!t->busy && t->n) {                         /* the oscillator above, two cycles */
        float cur = ui_v(sy_patch.p, SP_A_POS) * (float)(t->n - 1u), g = 12.0f * ui_frame_norm(t, cur);
        for (i = 0; i < 161u; i++)
            ys[i] = 74.0f - ui_wt_at(t, cur, (float)i / 80.0f) * g;
        lk_curve(20, ys, 161u, 1.0f, sk.mid, 0);
    }
    for (i = 0; i < 161u; i++)
        ys[i] = 144.0f - 34.0f * ui_sub_at(shape, pw, (float)i / 160.0f * cyc);
    lk_curve(17, ys, 161u, 1.0f, sk.line, 0);       /* (its width: the copies each side) */
    lk_curve(23, ys, 161u, 1.0f, sk.line, 0);
    lk_curve(20, ys, 161u, 2.2f, ui_ink(ui_hot(0)), 0);
    for (i = 0; i < 3u; i++)
        lk_seg_dash(20.0f + (float)(i * 80u), 60.0f, 20.0f + (float)(i * 80u), 182.0f, sk.line, 1.0f, 3.0f);
    y = lk_fader(214.0f, 50.0f, 180.0f, ui_v(sy_patch.p, SP_SUB_LEVEL));
    lk_arrow(70.0f, 197.0f, 70.0f - 10.0f - 20.0f * pw, 197.0f, 3.0f, 1.0f, ui_ink(ui_hot(3)));
    lk_arrow(130.0f, 197.0f, 130.0f + 10.0f + 20.0f * pw, 197.0f, 3.0f, 1.0f, ui_ink(ui_hot(3)));
    ui_title(SUB_NAMES[shape < SUB_NSHAPE ? shape : 0], UI_TITLE_X);
    sp_fix(w, oct, 0u, 1);
    ui_sub(UI_TITLE_X, "OCT", w);
    lk_label(84.0f, 192.0f, "WIDTH", 20);
    ui_H(0, 8.0f, 27.0f);
    ui_H(1, 8.0f, 43.0f);
    ui_H(2, 192.0f, y - 5.0f);
    ui_H(3, 166.0f, 192.0f);
}

/* NOISE: two fields of grain (L, R: STEREO apart; the same when 0), the colour as a tilt across the spectrum, LEVEL */
static void ui_p_noise(void)
{
    uint32_t f, i;
    float c = (float)sy_patch.p[SP_NZ_COLOR] * 0.001f, y;
    if (!sy_patch.p[SP_NZ_ON]) {
        ui_off("NOISE");
        return;
    }
    for (f = 0; f < 2u; f++) {
        uint32_t seed = f && sy_patch.p[SP_NZ_STEREO] > 30 ? 11u : 3u;
        float y0 = f ? 108.0f : 50.0f;
        if (!lk_vis(24.0f, y0, 182.0f, y0 + 48.0f))
            continue;
        for (i = 0; i < 300u; i++) {
            float x = 24.0f + (float)(sy_rand(&seed) >> 8) * (156.0f / 16777216.0f);
            lk_stip(x, y0 + (float)(sy_rand(&seed) >> 8) * (46.0f / 16777216.0f));
        }
    }
    lk_seg(24.0f, 177.0f, 180.0f, 177.0f, 1.0f, sk.line);
    lk_seg(24.0f, 177.0f + 9.0f * c, 180.0f, 177.0f - 9.0f * c, 1.5f, ui_ink(ui_hot(1)));
    lk_hollow(102.0f, 177.0f, 2.5f);
    y = lk_fader(222.0f, 50.0f, 180.0f, ui_v(sy_patch.p, SP_NZ_LEVEL));
    ui_title(NZ_NAMES[(uint32_t)sy_patch.p[SP_NZ_TYPE] % 3u], UI_TITLE_X);
    lk_text(10.0f, 69.0f, T9, "L", sk.lab, sk.ground, 0, 0);
    lk_text(10.0f, 127.0f, T9, "R", sk.lab, sk.ground, 0, 0);
    lk_label(24.0f, 192.0f, "LOW", 60);
    lk_label(160.0f, 192.0f, "HIGH", 60);
    ui_H(0, 8.0f, 27.0f);
    ui_H(1, 184.0f, 167.0f - 9.0f * c);
    ui_H(2, 9.0f, 97.0f);
    ui_H(3, 200.0f, y - 5.0f);
}

/* ---------------------------------------------------------------------------------------- ENV --- */
/* envelope e of values p, s seconds into a note held sus_t at its sustain */
static float ui_env_level(const int16_t *p, uint32_t e, float s, float sus_t)
{
    uint32_t b = e ? (uint32_t)(SP_E2_ATK - SP_E1_ATK) : 0u;
    float a = sp_time_s(ui_v(p, SP_E1_ATK + b)), h = p[SP_E1_HOLD + b] ? sp_time_s(ui_v(p, SP_E1_HOLD + b)) : 0.0f;
    float d = sp_time_s(ui_v(p, SP_E1_DEC + b)), r = sp_time_s(ui_v(p, SP_E1_REL + b)), S = ui_v(p, SP_E1_SUS + b);
    float ca = (float)p[SP_E1_ACRV + b] * 0.001f, cd = (float)p[SP_E1_DCRV + b] * 0.001f, cr = (float)p[SP_E1_RCRV + b] * 0.001f;
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
typedef struct { float a, h, d, s, r, sus_t, total; } ui_env_t;
static void ui_env_times(const int16_t *p, uint32_t e, ui_env_t *t)
{
    uint32_t b = e ? (uint32_t)(SP_E2_ATK - SP_E1_ATK) : 0u;
    t->a = sp_time_s(ui_v(p, SP_E1_ATK + b));
    t->h = p[SP_E1_HOLD + b] ? sp_time_s(ui_v(p, SP_E1_HOLD + b)) : 0.0f;
    t->d = sp_time_s(ui_v(p, SP_E1_DEC + b));
    t->r = sp_time_s(ui_v(p, SP_E1_REL + b));
    t->s = ui_v(p, SP_E1_SUS + b);
    t->sus_t = 0.3f * (t->a + t->h + t->d + t->r) + 0.05f;
    t->total = t->a + t->h + t->d + t->sus_t + t->r;
}
#define UI_EX(s, T) (14.0f + 212.0f * (s) / (T))
#define UI_EY(v) (194.0f - (v) * 150.0f)

/* ENV: the envelope fills the screen (its own time across), hatched under; ADSR: the other envelope dotted, the
 * voices riding it (HOT, their trails behind), the handles on its four corners; SHAPE: the handles on the plateau
 * and the three curves */
static void ui_p_env(void)
{
    uint32_t e = ui_sec() == SEC_ENV2, i, nv = 0;
    int shape = UI_PG[ui.page].id[0] == SP_E1_HOLD || UI_PG[ui.page].id[0] == SP_E2_HOLD;
    const sy_patch_t *gh = ui_gh(ui_turning() >= 0);
    ui_env_t t;
    float *ys, T;
    int fresh;
    char w[32];
    ui_env_times(sy_patch.p, e, &t);
    T = t.total;
    ys = ui_curve(0, &fresh);
    for (i = 0; fresh && i <= 212u; i++)
        ys[i] = UI_EY(ui_env_level(sy_patch.p, e, (float)i / 212.0f * T, t.sus_t));
    for (i = 0; i <= 212u; i += 4u)                 /* the hatch under it */
        if (ys[i] < 190.0f)
            lk_seg(14.0f + (float)i, ys[i] + 4.0f, 14.0f + (float)i, 194.0f, 1.0f, sk.line);
    lk_seg(12.0f, 194.0f, 228.0f, 194.0f, 1.0f, sk.mid);
    if (!shape) {                                   /* the other envelope, dotted */
        float *y2 = ui_curve(1, &fresh);
        for (i = 0; fresh && i <= 212u; i++)
            y2[i] = UI_EY(ui_env_level(sy_patch.p, !e, (float)i / 212.0f * T, t.sus_t));
        lk_curve(14, y2, 213u, 1.0f, sk.line, LK_DASH(1, 3));
    }
    if (gh) {                                       /* before the turn */
        float *y2 = ui_curve(2, &fresh);
        ui_env_t g;
        ui_env_times(gh->p, e, &g);
        for (i = 0; fresh && i <= 212u; i++)
            y2[i] = UI_EY(ui_env_level(gh->p, e, (float)i / 212.0f * T, g.sus_t));
        lk_curve(14, y2, 213u, 1.0f, sk.mid, LK_DASH(2, 2));
    }
    lk_curve(14, ys, 213u, 2.2f, sk.ink, 0);
    if (!shape) {
        for (i = 0; i < SY_NVOICE; i++) {           /* the voices: where each is (a value the audio writes), trails */
            const env_t *ev = &sy_v[i].env[e];
            float s;
            uint32_t k;
            if (!sy_v[i].on || ev->st == ENV_IDLE)
                continue;
            switch (ev->st) {
            case ENV_ATK: s = ev->t * t.a; break;
            case ENV_HOLD: s = t.a + ev->t * t.h; break;
            case ENV_DEC: s = t.a + t.h + ev->t * t.d; break;
            case ENV_SUS: s = t.a + t.h + t.d + t.sus_t * (0.2f + 0.6f * (float)i / (float)SY_NVOICE); break;
            default: s = t.a + t.h + t.d + t.sus_t + ev->t * t.r; break;
            }
            for (k = 1; k <= 3u && ev->st != ENV_SUS; k++) {
                float q = s - 0.035f * (float)k * T, qy;
                if (q > 0.0f) {
                    qy = UI_EY(ui_env_level(sy_patch.p, e, q, t.sus_t));
                    lk_disc(UI_EX(q, T), qy, 1.9f - 0.4f * (float)k, sk.mid);
                    ui_live(qy - 3.0f, qy + 3.0f);
                }
            }
            ui_live(UI_EY(ev->v) - 5.0f, UI_EY(ev->v) + 5.0f);
            lk_disc(UI_EX(s, T), UI_EY(ev->v), 3.75f, sk.hot);
            lk_disc(UI_EX(s, T), UI_EY(ev->v), 2.25f, sk.ground);
            nv++;
        }
        {
            float pts[4][2] = {{t.a + t.h, 1.0f}, {t.a + t.h + t.d, t.s}, {t.a + t.h + t.d + t.sus_t, t.s}, {T, 0.0f}};
            for (i = 0; i < 4u; i++)
                ui_H(i, UI_EX(pts[i][0], T) - 5.0f, UI_EY(pts[i][1]) - (i == 3u ? 18.0f : 16.0f));
        }
        if (nv) {
            lk_disc(11.0f, 205.0f, 2.5f, sk.hot);
            sp_put(sp_put(sp_fix(w, (int32_t)nv, 0u, 0), nv == 1u ? " VOICE \xB7 " : " VOICES \xB7 "),
                   e ? "ENV 1 DOTTED" : "ENV 2 DOTTED");
            lk_text(17.0f, 200.0f, T9, w, sk.lab, sk.ground, 20, 0);
        } else {
            lk_text(8.0f, 200.0f, T9, e ? "ENV 1 DOTTED" : "ENV 2 DOTTED", sk.lab, sk.ground, 20, 0);
        }
    } else {
        float sa = 0.5f * t.a, sd = t.a + t.h + 0.18f * t.d, sr = t.a + t.h + t.d + t.sus_t + 0.25f * t.r;
        ui_H(0, UI_EX(t.a + 0.5f * t.h, T) - 5.0f, UI_EY(1.0f) - 16.0f);
        ui_H(1, UI_EX(sa, T) - 14.0f, UI_EY(ui_env_level(sy_patch.p, e, sa, t.sus_t)) - 5.0f);
        ui_H(2, UI_EX(sd, T) + 2.0f, UI_EY(ui_env_level(sy_patch.p, e, sd, t.sus_t)) - 14.0f);
        ui_H(3, UI_EX(sr, T) + 4.0f, UI_EY(ui_env_level(sy_patch.p, e, sr, t.sus_t)) - 14.0f);
    }
}

/* ----------------------------------------------------------------------------------------- LFO --- */
/* LFO k's shape as the audio side plays it, for drawing: its settings and its drawn points */
typedef struct {
    lfo_draw_t dr;
    lfo_par_t lp;
    lfo_t l0;
    int drawn;
} ui_lfo_t;
static void ui_lfo_of(uint32_t k, ui_lfo_t *u)
{
    uint32_t b = UI_LB(k), shape = (uint32_t)sy_patch.p[SP_L1_SHAPE + b];
    sp_draw_t tp;
    lfo_draw_of(&u->dr, ui_pts(k, &tp));
    memset(&u->lp, 0, sizeof u->lp);
    memset(&u->l0, 0, sizeof u->l0);
    u->lp.shape = (uint8_t)shape;
    u->lp.dr = &u->dr;
    u->lp.sync = (uint8_t)(sy_patch.p[SP_L1_SYNC + b] != 0);
    u->lp.note = (uint8_t)sy_patch.p[k ? SP_L2_NOTE : SP_L1_NOTE];
    u->lp.rate = ui_v(sy_patch.p, SP_L1_RATE + b);
    u->l0.r0 = 0.6f;
    u->l0.r1 = -0.4f;
    u->drawn = ui_lfo_rnd(shape) || shape == LFO_DRAW;
}
static float ui_lfo_at(const ui_lfo_t *u, float x)  /* a cycle, x 0..1 (S&H and SMOOTH: as their points) */
{
    x -= (float)sy_floori(x);
    return u->drawn ? lfo_draw_at(&u->dr, x) : lfo_shape(&u->l0, &u->lp, sy_minf(x, 0.99999f));
}
/* the LFO's phase now (its dial): the newest voice's in RETRIG and ENV, the shared one in FREE */
static float ui_lfo_phase(uint32_t k)
{
    const lfo_t *l = &sy_glfo[k];
    uint32_t i, mode = (uint32_t)sy_patch.p[SP_L1_MODE + UI_LB(k)], age = 0;
    if (mode != LFOM_FREE)
        for (i = 0; i < SY_NVOICE; i++)
            if (sy_v[i].on && sy_v[i].age >= age) {
                age = sy_v[i].age;
                l = &sy_v[i].lfo[k];
            }
    return (float)(l->ph >> 8) * (1.0f / 16777216.0f);
}
/* every LFO page: the dial at the top right, its hand turning at the rate (live while sound plays) */
static void ui_orbit(uint32_t k)
{
    float a = ui_lfo_phase(k) - 0.25f, dx = sy_cos2pi(a), dy = sy_sin2pi(a);
    lk_ring(206.0f, 34.0f, 10.0f, 1.0f, sk.line);
    lk_seg(206.0f, 22.0f, 206.0f, 26.0f, 1.0f, sk.mid);
    lk_seg(206.0f, 34.0f, 206.0f + 9.0f * dx, 34.0f + 9.0f * dy, 1.0f, sk.line);
    lk_disc(206.0f + 10.0f * dx, 34.0f + 10.0f * dy, 2.5f, sk.hot);
}
static float ui_lfo_hz(uint32_t k)
{
    ui_lfo_t u;
    ui_lfo_of(k, &u);
    return lfo_hz(&u.lp);
}

#define UI_LX(u) (14.0f + (u) * 212.0f)
#define UI_LY(v) (112.0f - (v) * 70.0f)
/* LFO POINTS: the drawn shape (its points; the one picked filled, its moves as arrows; the segment KNOB 4 curves
 * thick under it), hatched to its middle; the voices on it (HOT) */
static void ui_p_lfo_points(void)
{
    uint32_t k = ui_lfo(), i, p, g;
    sp_draw_t tp;
    const sp_draw_t *d = ui_pts(k, &tp);
    ui_lfo_t u;
    float *ys, px, py, c0, c1, mx;
    int fresh;
    char w[40];
    ui_lfo_of(k, &u);
    ys = ui_curve(0, &fresh);
    for (i = 0; fresh && i <= 212u; i++)
        ys[i] = UI_LY(ui_lfo_at(&u, (float)i / 212.0f));
    lk_seg(14.0f, 112.0f, 226.0f, 112.0f, 1.0f, sk.line);
    for (i = 0; i <= 212u; i += 4u)
        if (sy_absf(ys[i] - 112.0f) > 1.0f)
            lk_seg(14.0f + (float)i, 112.0f, 14.0f + (float)i, ys[i], 1.0f, sk.line);
    p = ui_lpt(k, d);
    g = p + 1u < d->n ? p : p - 1u;
    c0 = (float)d->x[g] * 0.212f;
    c1 = (float)d->x[g + 1u] * 0.212f;
    if (c1 > c0 + 1.0f)                             /* the segment KNOB 4 curves */
        lk_curve(14 + sy_floori(c0 + 0.5f), ys + sy_floori(c0 + 0.5f), (uint32_t)(sy_floori(c1 + 0.5f) - sy_floori(c0 + 0.5f) + 1),
                 3.0f, ui_hot(3) ? sk.hot : sk.line, 0);
    lk_curve(14, ys, 213u, 2.2f, sk.ink, 0);
    for (i = 0; i < d->n; i++)
        if (i != p)
            lk_hollow_sq(UI_LX((float)d->x[i] * 0.001f), UI_LY((float)d->y[i] * 0.001f), 2.0f);
    px = UI_LX((float)d->x[p] * 0.001f);
    py = UI_LY((float)d->y[p] * 0.001f);
    lk_box(px - 3.5f, py - 3.5f, px + 3.5f, py + 3.5f, ui_ink(ui_hot(0)));
    if (p && p + 1u < d->n) {                       /* (the ends stay at the cycle's ends) */
        lk_arrow(px - 2.0f, py - 10.0f, px - 22.0f, py - 10.0f, 3.0f, 1.0f, ui_hot(1) ? sk.hot : sk.mid);
        lk_arrow(px + 2.0f, py - 10.0f, px + 22.0f, py - 10.0f, 3.0f, 1.0f, ui_hot(1) ? sk.hot : sk.mid);
    }
    lk_arrow(px + 10.0f, py + 22.0f, px + 10.0f, py - 2.0f, 3.0f, 1.0f, ui_hot(2) ? sk.hot : sk.mid);
    if (!ui_lfo_rnd((uint32_t)sy_patch.p[SP_L1_SHAPE + UI_LB(k)])) {   /* the voices on it */
        uint32_t mode = (uint32_t)sy_patch.p[SP_L1_MODE + UI_LB(k)];
        float ph = (float)sy_patch.p[SP_L1_PHASE + UI_LB(k)] * (1.0f / 360.0f);
        for (i = 0; i < SY_NVOICE; i++) {
            const lfo_t *l = mode == LFOM_FREE ? &sy_glfo[k] : &sy_v[i].lfo[k];
            float x = (float)(l->ph >> 8) * (1.0f / 16777216.0f);
            if (!sy_v[i].on)
                continue;
            if (mode == LFOM_FREE)
                x += ph, x -= (float)sy_floori(x);
            lk_disc(UI_LX(x), UI_LY(ui_lfo_at(&u, x)), 3.0f, sk.hot);
            ui_live(UI_LY(ui_lfo_at(&u, x)) - 5.0f, UI_LY(ui_lfo_at(&u, x)) + 5.0f);
            if (mode == LFOM_FREE)
                break;
        }
    }
    mx = 14.0f + 0.5f * (c0 + c1);
    ui_H(0, px - 18.0f, py + 4.0f);
    ui_H(1, px + 25.0f, py - 15.0f);
    ui_H(2, px + 13.0f, py + 18.0f);
    ui_H(3, mx + 4.0f, ys[sy_clampi(sy_floori(0.5f * (c0 + c1) + 0.5f), 0, 212)] - 4.0f);
    sp_fix(sp_put(sp_fix(sp_put(w, "POINT "), (int32_t)p + 1, 0u, 0), " OF "), d->n, 0u, 0);
    lk_text(8.0f, 200.0f, T10, w, sk.lab, sk.ground, 20, 0);
    ui_orbit(k);
}

/* the cycles across 212 px at the rate: about 4 s of it (1 .. 12) */
static float ui_lfo_cycles(uint32_t k) { return sy_clampf(ui_lfo_hz(k) * 4.0f, 1.0f, 12.0f); }

/* LFO SHAPE: the shapes to pick from (the one picked inverted), the shape at its rate (one cycle bracketed), SYNC, MODE */
static void ui_p_lfo_shape(void)
{
    uint32_t k = ui_lfo(), b = UI_LB(k), cur = (uint32_t)sy_patch.p[SP_L1_SHAPE + b], first, i, j;
    uint32_t mode = (uint32_t)sy_patch.p[SP_L1_MODE + b], sync = sy_patch.p[SP_L1_SYNC + b] != 0;
    ui_lfo_t u;
    float *ys, cyc = ui_lfo_cycles(k), xe, x;
    int fresh;
    char w[16];
    ui_lfo_of(k, &u);
    first = cur > 2u ? cur - 2u : 0u;
    if (first > LFO_NSHAPE - 6u)
        first = LFO_NSHAPE - 6u;
    for (i = 0; i < 6u; i++) {                      /* six of the shapes, the one picked among them */
        float bx = 24.0f + (float)(i * 34u), m[23];
        ui_lfo_t s = u;
        int on = first + i == cur;
        s.lp.shape = (uint8_t)(first + i);
        s.drawn = ui_lfo_rnd(first + i) || first + i == LFO_DRAW;
        if (on)
            lk_box(bx, 52.0f, bx + 30.0f, 72.0f, ui_ink(ui_hot(0)));
        else
            lk_frame(bx + 0.5f, 52.5f, 29.0f, 19.0f, 1.0f, sk.line);
        for (j = 0; j < 23u; j++)
            m[j] = 62.0f - 6.0f * ui_lfo_at(&s, (float)j / 22.0f);
        lk_curve(sy_floori(bx) + 4, m, 23u, 1.0f, on ? sk.ground : sk.mid, 0);
    }
    ys = ui_curve(0, &fresh);
    for (i = 0; fresh && i <= 212u; i++)
        ys[i] = 122.0f - 24.0f * ui_lfo_at(&u, (float)i / 212.0f * cyc);
    lk_seg(14.0f, 122.0f, 226.0f, 122.0f, 1.0f, sk.line);
    lk_curve(14, ys, 213u, 2.2f, sk.ink, 0);
    xe = 14.0f + 212.0f / cyc;                      /* one cycle */
    lk_seg(14.0f, 84.0f, 14.0f, 92.0f, 1.0f, sk.mid);
    lk_seg(14.0f, 88.0f, xe, 88.0f, 1.0f, ui_hot(2) ? sk.hot : sk.mid);
    lk_seg(xe, 84.0f, xe, 92.0f, 1.0f, sk.mid);
    sp_put(w, NOTE_NAMES[u.lp.note < SY_NNOTE ? u.lp.note : 0]);
    x = lk_chip(24.0f, 160.0f, "HZ", !sync);
    x = lk_chip(x, 160.0f, w, sync);
    lk_label(x + 4.0f, 163.0f, "SYNC", 20);
    x = 24.0f;
    for (i = 0; i < 3u; i++)
        x = lk_chip(x, 182.0f, LFOM_NAMES[i], i == mode);
    lk_label(x + 4.0f, 185.0f, "MODE", 20);
    ui_H(0, 8.0f, 57.0f);
    ui_H(1, 8.0f, 163.0f);
    ui_H(2, sy_minf(xe + 5.0f, 214.0f), 81.0f);
    ui_H(3, 8.0f, 185.0f);
    ui_orbit(k);
}

/* LFO TIME: the delay (flat), the rise (dashed: the depth fading in), the start phase (the dot), SMOOTH (the corners) */
static void ui_p_lfo_time(void)
{
    uint32_t k = ui_lfo(), b = UI_LB(k), i;
    ui_lfo_t u;
    float dl = sy_patch.p[SP_L1_DELAY + b] ? sp_time_s(ui_v(sy_patch.p, SP_L1_DELAY + b)) : 0.0f;
    float rs = sy_patch.p[SP_L1_RISE + b] ? sp_time_s(ui_v(sy_patch.p, SP_L1_RISE + b)) : 0.0f;
    float hz = ui_lfo_hz(k), T = sy_clampf(sy_maxf(1.6f * (dl + rs), 3.0f / sy_maxf(hz, 0.01f)), 1.0f, 30.0f);
    float ph = (float)sy_patch.p[SP_L1_PHASE + b] * (1.0f / 360.0f), sm = ui_v(sy_patch.p, SP_L1_SMOOTH + b);
    float xd = 14.0f + 212.0f * dl / T, xr = xd + 212.0f * rs / T, ys[213], y = 104.0f, C = 104.0f, A = 34.0f;
    ui_lfo_of(k, &u);
    lk_seg(14.0f, C, 226.0f, C, 1.0f, sk.line);
    for (i = 0; i <= 212u; i++) {
        float x = 14.0f + (float)i, e, v = 0.0f;
        if (x >= xd) {
            e = xr > xd ? sy_minf(1.0f, (x - xd) / (xr - xd)) : 1.0f;
            v = e * ui_lfo_at(&u, (x - xd) / 212.0f * T * hz + ph);
        }
        y += ((C - A * v) - y) * (1.0f - 0.92f * sm);   /* (SMOOTH: a pole across the columns) */
        ys[i] = i ? y : C;
    }
    lk_curve(14, ys, 213u, 2.2f, sk.ink, 0);
    {
        float a[6] = {xd, C, xr, C - A, 226.0f, C - A}, c[6] = {xd, C, xr, C + A, 226.0f, C + A};
        lk_poly_dash(a, 3u, 1.0f, ui_hot(1) ? sk.hot : sk.mid, 2.0f, 2.0f);
        lk_poly_dash(c, 3u, 1.0f, ui_hot(1) ? sk.hot : sk.mid, 2.0f, 2.0f);
    }
    lk_hollow(xd, C, 3.0f);
    lk_seg(14.0f, 156.0f, 14.0f, 164.0f, 1.0f, sk.mid);     /* the delay's and the rise's brackets */
    lk_seg(14.0f, 160.0f, xd, 160.0f, 1.0f, ui_hot(0) ? sk.hot : sk.mid);
    lk_seg(xd, 156.0f, xd, 164.0f, 1.0f, sk.mid);
    lk_seg(xr, 156.0f, xr, 164.0f, 1.0f, sk.mid);
    lk_seg(xd, 160.0f, xr, 160.0f, 1.0f, ui_hot(1) ? sk.hot : sk.mid);
    {                                               /* (their words under their brackets, kept apart and on the screen) */
        float dc = sy_maxf(0.5f * (14.0f + xd), 8.0f + 0.5f * lk_tw(T9, "DELAY", 60)), rc = sy_maxf(0.5f * (xd + xr), dc + 34.0f);
        lk_text_c(dc, 166.0f, T9, "DELAY", sk.lab, sk.ground, 60, 0);
        lk_text_c(rc, 166.0f, T9, "RISE", sk.lab, sk.ground, 60, 0);
        ui_H(0, dc - 5.0f, 180.0f);
        ui_H(1, rc - 5.0f, 180.0f);
    }
    ui_H(2, xd - 12.0f, C - 20.0f);
    ui_H(3, sy_minf(xr + 36.0f, 214.0f), C - A - 16.0f);
    ui_orbit(k);
}

/* ---------------------------------------------------------------------------- VOICE, PLAY, MATRIX --- */
/* VOICE: MODE, the voices as pills (HOT: playing; past VOICES: LINE), GLIDE's curve, GL MODE */
static void ui_p_voice(void)
{
    uint32_t i, np = (uint32_t)sy_patch.p[SP_V_POLY], nv = 0;
    float gl = ui_v(sy_patch.p, SP_V_GLIDE), ys[65], x;
    char w[16];
    ui_live(48.0f, 160.0f);                         /* (the pills, the count playing) */
    for (i = 0; i < SY_NVOICE; i++) {
        float bx = 20.0f + (float)(i * 16u);
        lk_frame(bx + 0.5f, 50.5f, 9.0f, 89.0f, 1.0f, i < np ? ui_ink(ui_hot(1)) : sk.line);
        if (sy_v[i].on) {
            lk_box(bx + 2.5f, 52.5f, bx + 7.5f, 137.5f, sk.hot);
            nv++;
        }
    }
    ui_title(VMODE_NAMES[(uint32_t)sy_patch.p[SP_V_MODE] % 3u], UI_TITLE_X);
    if (nv) {
        lk_disc(22.5f, 152.0f, 2.0f, sk.hot);
        sp_put(sp_fix(w, (int32_t)nv, 0u, 0), " PLAYING");
        lk_text(28.0f, 148.0f, T9, w, sk.lab, sk.ground, 60, 0);
    }
    lk_seg_dash(156.0f, 150.0f, 228.0f, 150.0f, sk.line, 1.0f, 3.0f);
    lk_seg_dash(156.0f, 70.0f, 228.0f, 70.0f, sk.line, 1.0f, 3.0f);
    for (i = 0; i <= 64u; i++)                      /* GLIDE: from one note to the next */
        ys[i] = 150.0f - 80.0f * (1.0f - sy_exp2f(-(float)i / 64.0f * 7.2f / (0.08f + 1.6f * gl)));
    lk_curve(160, ys, 65u, 2.2f, ui_ink(ui_hot(2)), 0);
    lk_hollow(160.0f, 150.0f, 3.0f);
    lk_disc(224.0f, 70.5f, 3.0f, sk.ink);
    x = lk_chip(24.0f, 176.0f, GLMODE_NAMES[0], sy_patch.p[SP_V_GLMODE] == 0);
    x = lk_chip(x, 176.0f, GLMODE_NAMES[1], sy_patch.p[SP_V_GLMODE] != 0);
    lk_label(x + 4.0f, 178.0f, "GLIDE", 60);
    ui_H(0, 8.0f, 27.0f);
    ui_H(1, 150.0f, 90.0f);
    ui_H(2, 188.0f, 100.0f);
    ui_H(3, 8.0f, 177.0f);
}

/* PLAY: BEND UP and DOWN as semitones on a ladder, VEL AMP as a slope (velocity to level), VOLUME */
static void ui_p_play(void)
{
    int32_t s, up = sy_patch.p[SP_V_BUP], dn = sy_patch.p[SP_V_BDN];
    float vel = ui_v(sy_patch.p, SP_V_VEL), y;
    for (s = -24; s <= 24; s += 2) {
        float yy = 112.0f - (float)s * 2.5f;
        int big = s % 12 == 0;
        lk_seg(big ? 40.0f : 44.0f, yy, big ? 60.0f : 56.0f, yy, 1.0f, s ? sk.line : sk.mid);
    }
    if (up)
        lk_seg(50.0f, 112.0f, 50.0f, 112.0f - 2.5f * (float)up, 3.0f, ui_ink(ui_hot(0)));
    if (dn)
        lk_seg(50.0f, 112.0f, 50.0f, 112.0f + 2.5f * (float)dn, 3.0f, ui_ink(ui_hot(1)));
    lk_arrow(50.0f, 112.0f - 2.5f * (float)up + 6.0f, 50.0f, 112.0f - 2.5f * (float)up - 2.0f, 3.0f, 1.5f, ui_ink(ui_hot(0)));
    lk_arrow(50.0f, 112.0f + 2.5f * (float)dn - 6.0f, 50.0f, 112.0f + 2.5f * (float)dn + 2.0f, 3.0f, 1.5f, ui_ink(ui_hot(1)));
    lk_disc(50.0f, 112.0f, 3.0f, sk.ink);
    lk_label(8.0f, 22.0f, "BEND", 20);
    lk_frame(100.0f, 70.0f, 70.0f, 70.0f, 1.0f, sk.line);
    lk_seg(100.0f, 70.0f + 70.0f * vel, 170.0f, 70.0f, 2.2f, ui_ink(ui_hot(2)));
    lk_text(100.0f, 146.0f, T9, "VEL \x86 LEVEL", sk.lab, sk.ground, 20, 0);
    y = lk_fader(214.0f, 50.0f, 180.0f, ui_v(sy_patch.p, SP_V_VOL));
    ui_H(0, 62.0f, 112.0f - 2.5f * (float)up - 10.0f);
    ui_H(1, 62.0f, 112.0f + 2.5f * (float)dn - 2.0f);
    ui_H(2, 174.0f, 58.0f);
    ui_H(3, 192.0f, y - 5.0f);
}

/* MATRIX: the eight slots (source -> destination, the amount from the middle), the one picked outlined, and at the
 * bottom the one picked as KNOB 2-4 set it */
static void ui_p_matrix(void)
{
    uint32_t s, ms = ui.mslot < NSLOT ? ui.mslot : 0u, g0 = ms & ~3u, i;
    char t[SP_TEXT_N + 8], f[40];
    for (i = 0; i < 4u && g0 + i < NSLOT; i++) {   /* the four slots of the one picked: 1-4 or 5-8 */
        float y = 24.0f + (float)(i * 34u), x;
        int32_t src, amt;
        s = g0 + i;
        src = sy_patch.p[SP_M1_SRC + 3u * s];
        amt = sy_patch.p[SP_M1_AMT + 3u * s];
        sp_fix(t, (int32_t)s + 1, 0u, 0);
        lk_text(22.0f, y + 2.0f, T12B, t, s == ms ? sk.ink : sk.lab, sk.ground, 0, 0);
        if (src <= SRC_NONE || src >= (int32_t)SRC_COUNT) {
            lk_text(40.0f, y + 4.0f, T10, "EMPTY", sk.lab, sk.ground, 20, 0);
            continue;
        }
        x = lk_text(40.0f, y, T10B, SRC_NAMES[src], sk.ink, sk.ground, 0, 0) + 4.0f;
        lk_arrow(x, y + 6.5f, x + 12.0f, y + 6.5f, 2.5f, 1.0f, sk.mid);
        sp_text(SP_M1_DST + 3u * s, sy_patch.p[SP_M1_DST + 3u * s], t);
        lk_text(x + 17.0f, y, T10B, lk_fit(f, sizeof f, t, T10B, 0, 228.0f - (x + 17.0f)), sk.ink, sk.ground, 0, 0);
        lk_seg(40.0f, y + 21.0f, 184.0f, y + 21.0f, 1.0f, sk.line);   /* the amount, from the middle */
        lk_seg(112.0f, y + 17.0f, 112.0f, y + 25.0f, 1.0f, sk.mid);
        if (amt)
            lk_seg(112.0f, y + 21.0f, 112.0f + (float)amt * 0.072f, y + 21.0f, 3.0f, s == ms && ui_hot(3) ? sk.hot : sk.ink);
        ui_pct(t, amt);
        lk_text_r(228.0f, y + 15.0f, T9B, t, sk.ink, sk.ground, 0, 0);
    }
    lk_frame(16.5f, 24.0f + (float)((ms - g0) * 34u) - 3.5f, 216.0f, 32.0f, 1.0f, ui_ink(ui_hot(0)));
    {                                               /* the slot picked: its source, where it goes, how much */
        int32_t src = sy_patch.p[SP_M1_SRC + 3u * ms];
        float x, w;
        const char *sn = src > SRC_NONE && src < (int32_t)SRC_COUNT ? SRC_NAMES[src] : "NONE";
        w = lk_tw(T12B, sn, 0) + 12.0f;
        lk_frame(16.5f, 184.5f, w - 1.0f, 20.0f, 1.0f, ui_ink(ui_hot(1)));
        lk_text(22.5f, 187.0f, T12B, sn, sk.ink, sk.ground, 0, 15);
        x = 16.0f + w + 5.0f;
        lk_arrow(x, 194.5f, x + 12.0f, 194.5f, 3.0f, 1.5f, sk.ink);
        sp_text(SP_M1_DST + 3u * ms, sy_patch.p[SP_M1_DST + 3u * ms], t);
        lk_fit(f, sizeof f, t, T12B, 0, 196.0f - (x + 23.0f));
        w = lk_tw(T12B, f, 0) + 12.0f;
        lk_frame(x + 17.5f, 184.5f, w - 1.0f, 20.0f, 1.0f, ui_ink(ui_hot(2)));
        lk_text(x + 23.5f, 187.0f, T12B, f, sk.ink, sk.ground, 0, 15);
        lk_dial_arc(222.0f, 194.5f, 8.0f, 3.0f, 0.0f, 1.0f, sk.line);
        {
            float a = (float)sy_patch.p[SP_M1_AMT + 3u * ms] * 0.0005f;
            if (a > 0.004f)
                lk_arc(222.0f, 194.5f, 8.0f, 3.0f, 0.75f, 0.75f + a * 0.75f, ui_ink(ui_hot(3)), 1);
            else if (a < -0.004f)
                lk_arc(222.0f, 194.5f, 8.0f, 3.0f, 0.75f + a * 0.75f, 0.75f, ui_ink(ui_hot(3)), 1);
        }
        ui_H(1, 22.0f, 165.0f);
        ui_H(2, x + 23.0f, 165.0f);
    }
    ui_H(0, 2.0f, 24.0f + (float)((ms - g0) * 34u) + 3.0f);
    ui_H(3, 217.0f, 165.0f);
}

/* ----------------------------------------------------------------------------------- GLOBAL, KEYS --- */
/* the metronome's arm: still at the middle, swinging on the beat while PLAY runs (22 degrees each way); its weight
 * up for a slow tempo */
static float ui_swing(void)
{
    float b = ui_beats();
    if (!ui.run)
        return 0.0f;
    return 0.0611f * sy_cos2pi(0.5f * (b - 2.0f * (float)sy_floori(0.5f * b)));
}
static uint32_t ui_beat_now(void)                   /* (V0.75: the audio side's beat, MIDI's clock's too) */
{
    return ui.run ? (uint32_t)sy_floori(ui_beats()) % (ui.tsb ? ui.tsb : 4u) : 0u;
}
static void ui_p_tempo(void)
{
    uint32_t i, n = ui.tsb ? ui.tsb : 4u, now = ui_beat_now();
    uint32_t bpm = (uint32_t)sy_clampi((int32_t)ui_bpm_now(), 20, 300);
    float a = ui_swing() - 0.25f, dx = sy_cos2pi(a), dy = sy_sin2pi(a), wy = 46.0f + (float)(300u - bpm) * (80.0f / 280.0f);
    float gap = n > 1u ? sy_minf(24.0f, 80.0f / (float)(n - 1u)) : 0.0f, r = sy_minf(6.0f, 0.4f * gap + 1.5f);
    float body[10] = {40, 190, 56, 50, 84, 50, 100, 190, 40, 190};
    char t[16];
    lk_poly(body, 5u, 1.5f, sk.ink);
    for (i = 0; i < 10u; i++)
        lk_seg(77.0f, 70.0f + (float)(i * 10u), 81.0f, 70.0f + (float)(i * 10u), 1.0f, sk.line);
    lk_seg(70.0f, 170.0f, 70.0f + 112.0f * dx, 170.0f + 112.0f * dy, 1.5f, sk.ink);
    {                                               /* the weight, along the arm */
        float l = 170.0f - wy - 5.0f, cx = 70.0f + l * dx, cy = 170.0f + l * dy;
        lk_box(cx - 6.0f, cy - 5.0f, cx + 6.0f, cy + 5.0f, ui_ink(ui_hot(0)));
        ui_H(0, 84.0f, wy - 4.0f);
    }
    lk_disc(70.0f, 170.0f, 3.0f, sk.ink);
    sp_fix(sp_put(sp_fix(t, (int32_t)n, 0u, 0), "/"), UI_TSN_VAL[ui.tsn], 0u, 0);
    lk_text(146.0f, 44.0f, T14B, t, ui_hot(1) || ui_hot(2) ? sk.hot : sk.ink, sk.ground, 0, 17);
    for (i = 0; i < n; i++) {                       /* the bar's beats, the one now filled */
        float cx = 152.0f + (float)i * gap;
        if (i == now)
            lk_disc(cx, 84.0f, r, sk.ink);
        else
            lk_hollow(cx, 84.0f, r - 0.6f);
    }
    ui_H(1, 128.0f, 47.0f);
    ui_H(2, 146.0f + lk_tw(T14B, t, 0) + 8.0f, 47.0f);
    sp_fix(t, (int32_t)ui_bpm_now(), 0u, 0);
    lk_text(144.0f, 104.0f, T26B, t, ui_ink(ui_hot(0)), sk.ground, -20, 28);
    lk_label(146.0f, 140.0f, sy_app_clock() ? "BPM: MIDI" : "BPM", 40);
    {                                               /* CLOCK IN (V0.75): the panel's tempo, or MIDI's clock */
        float x = lk_chip(146.0f, 172.0f, "INT", !chd.clock);
        lk_label(146.0f, 158.0f, "CLOCK IN", 20);
        lk_chip(x, 172.0f, "MIDI", chd.clock);
        ui_H(3, 128.0f, 175.0f);
    }
}

/* KEYS: the scale on the 27 keys (dots: in the scale; ringed: the root; chevrons: where SNAP moves a key), its name,
 * the root, FIT, the keys' velocity */
static void ui_p_keys(void)
{
    uint32_t mask = UI_SCALE_MASK[ui.scale % UI_NSCALE], k;
    lk_kb_t kb;
    float x;
    char t[8];
    lk_kb(&kb, 8.0f, 110.0f, 224.0f, 96.0f);
    for (k = 0; k < 27u; k++) {
        int32_t n = sy_ui_key_note(k, 0), own = n == UI_KEY0 + (int32_t)k;
        int32_t pc = n >= 0 ? (n - ui.root + 120) % 12 : -1;
        int in = pc >= 0 && ((mask >> (uint32_t)pc) & 1u), black = LK_BLACK[k % 12u];
        float cx = lk_kb_cx(&kb, k);
        if (n < 0)
            continue;
        if (black) {
            if (in && own)
                lk_disc(cx, 110.0f + kb.bh - 8.0f, 2.0f, sk.ground);
            continue;
        }
        if (ui.fit == UI_FIT_SNAP && !own)          /* (SNAP: a key off the scale plays the note below) */
            lk_poly((const float[6]){cx + 2.0f, 179.0f, cx - 2.0f, 182.0f, cx + 2.0f, 185.0f}, 3u, 1.0f, sk.mid);
        else if (in)
            lk_disc(cx, 194.0f, 2.5f, ui_ink(ui_hot(0)));
        if (pc == 0)
            lk_ring(cx, 194.0f, 4.5f, 1.0f, ui_ink(ui_hot(1)));
    }
    ui_title(UI_SCALE_LONG[ui.scale % UI_NSCALE], UI_TITLE_X);
    ui_sub(UI_TITLE_X, "ROOT", UI_NOTE_NAME[ui.root % 12u]);
    x = 26.0f;
    for (k = 0; k < UI_NFIT; k++)
        x = lk_chip(x, 62.0f, UI_FIT_NAME[k], k == ui.fit);
    lk_label(x + 4.0f, 65.0f, "FIT", 20);
    lk_label(26.0f, 85.0f, "VELOCITY", 20);
    lk_seg(80.0f, 91.0f, 192.0f, 91.0f, 3.0f, sk.line);
    lk_seg(80.0f, 91.0f, 80.0f + 112.0f * (float)ui.kvel / 127.0f, 91.0f, 3.0f, ui_ink(ui_hot(3)));
    sp_fix(t, ui.kvel, 0u, 0);
    lk_text_r(218.0f, 85.0f, T9B, t, ui_ink(ui_hot(3)), sk.ground, 0, 0);
    ui_H(0, 8.0f, 27.0f);
    ui_H(1, 8.0f, 43.0f);
    ui_H(2, 8.0f, 65.0f);
    ui_H(3, 224.0f, 86.0f);
}

/* ------------------------------------------------------------------------------------- SETTINGS --- */
/* a toggle: its track, the knob at the right (on: filled) or the left */
static void ui_toggle(float x, float y, int on, int hot)
{
    lk_capsule(x, y, 26.0f, 10.0f, sk.ink);
    if (on)
        lk_disc(x + 21.0f, y + 5.0f, 3.5f, ui_ink(hot));
    else
        lk_ring(x + 5.0f, y + 5.0f, 3.0f, 1.0f, hot ? sk.hot : sk.mid);
}

/* SCREEN: the eight themes (each its ground, its ink, its accent as the border), HELP, GLOW */
static void ui_p_color(void)
{
    uint32_t i;
    for (i = 0; i < SY_NPAL; i++) {                 /* the twelve themes: four a row */
        float x = 22.0f + (float)((i % 4u) * 54u), y = 22.0f + (float)((i / 4u) * 46u);
        uint16_t gr = sy_rgb(SY_PAL[i].c[PR_GROUND]), in = sy_rgb(SY_PAL[i].c[PR_INK]), ac = sy_rgb(SY_PAL[i].c[PR_HOT]);
        lk_box(x, y, x + 46.0f, y + 26.0f, gr);
        lk_frame(x + 0.5f, y + 0.5f, 45.0f, 25.0f, 1.0f, ac);
        lk_text_c(x + 23.0f, y + 5.0f, T12B, "Aa", in, gr, 0, 15);
        lk_text_c(x + 23.0f, y + 29.0f, i == ui.pal ? T9B : T9, SY_PAL[i].name, i == ui.pal ? sk.ink : sk.lab, sk.ground, 0, 0);
        if (i == ui.pal)
            lk_frame(x - 2.5f, y - 2.5f, 51.0f, 31.0f, 1.5f, ui_ink(ui_hot(0)));
    }
    lk_text(22.0f, 166.0f, T10, "HELP", sk.lab, sk.ground, 20, 0);
    ui_toggle(56.0f, 167.5f, ui.help, ui_hot(1));
    lk_text(122.0f, 166.0f, T10, "GLOW", sk.lab, sk.ground, 20, 0);
    {                                               /* the LEDs' glow: a sun, its rays shorter when LOW */
        float vx = 1.0f, vy = 0.0f, cs = sy_cos2pi(0.125f), sn = sy_sin2pi(0.125f), r1 = ui.glow ? 7.0f : 9.0f;
        lk_disc(168.0f, 172.5f, 3.5f, ui_ink(ui_hot(2)));
        for (i = 0; i < 8u; i++) {
            float nx;
            lk_seg(168.0f + 6.0f * vx, 172.5f + 6.0f * vy, 168.0f + r1 * vx, 172.5f + r1 * vy, 1.0f, ui_ink(ui_hot(2)));
            nx = vx * cs - vy * sn;
            vy = vx * sn + vy * cs;
            vx = nx;
        }
        lk_text(182.0f, 167.0f, T9B, ui.glow ? "LOW" : "HIGH", sk.ink, sk.ground, 0, 0);
    }
    ui_H(0, 2.0f, 22.0f + (float)((ui.pal % SY_NPAL / 4u) * 46u) + 8.0f);
    ui_H(1, 88.0f, 168.0f);
    ui_H(2, 214.0f, 168.0f);
}

/* PANEL: ACCEL (the speed a knob needs to step faster; OFF: flat), HOLD (300 .. 600 ms before a hold) */
static void ui_p_panel(void)
{
    uint32_t i;
    float ys[91], x = 140.0f + (float)(HOLD_MS[settings_hold % 4u] - 300u) * (88.0f / 300.0f);
    char t[8];
    lk_text(20.0f, 28.0f, T10, "ACCEL", sk.lab, sk.ground, 80, 0);
    lk_frame(20.0f, 50.0f, 90.0f, 90.0f, 1.0f, sk.line);
    for (i = 0; i <= 90u; i++)
        ys[i] = 138.0f - 86.0f * sy_exp2f(2.4f * sy_log2f(sy_maxf((float)i / 90.0f, 1e-4f)));
    if (ui.accel) {
        lk_seg_dash(20.0f, 131.0f, 110.0f, 131.0f, sk.mid, 2.0f, 2.0f);
        lk_curve(20, ys, 91u, 2.2f, ui_ink(ui_hot(0)), 0);
    } else {
        lk_curve(20, ys, 91u, 1.0f, sk.mid, LK_DASH(2, 2));
        lk_seg(20.0f, 131.0f, 110.0f, 131.0f, 2.2f, ui_ink(ui_hot(0)));
    }
    lk_text(20.0f, 146.0f, T9, "SLOW", sk.lab, sk.ground, 40, 0);
    lk_text_r(110.0f, 146.0f, T9, "FAST", sk.lab, sk.ground, 40, 0);
    lk_text(140.0f, 28.0f, T10, "HOLD", sk.lab, sk.ground, 80, 0);
    lk_frame(150.0f, 52.0f, 30.0f, 16.0f, 1.0f, sk.ink);
    lk_arrow(165.0f, 40.0f, 165.0f, 49.0f, 3.0f, 1.5f, sk.ink);
    lk_seg(140.0f, 104.0f, 228.0f, 104.0f, 1.0f, sk.line);
    for (i = 0; i < 4u; i++)
        lk_seg(140.0f + (float)(i * 100u) * (88.0f / 300.0f), 101.0f, 140.0f + (float)(i * 100u) * (88.0f / 300.0f),
               107.0f, 1.0f, sk.mid);
    lk_seg(140.0f, 104.0f, x, 104.0f, 3.0f, ui_ink(ui_hot(1)));
    lk_box(x - 2.0f, 98.0f, x + 2.0f, 110.0f, ui_ink(ui_hot(1)));
    lk_text(132.0f, 112.0f, T9, "300", sk.lab, sk.ground, 0, 0);
    lk_text(216.0f, 112.0f, T9, "600", sk.lab, sk.ground, 0, 0);
    sp_fix(t, HOLD_MS[settings_hold % 4u], 0u, 0);
    lk_text(190.0f, 90.0f, T9, t, sk.ink, sk.ground, 0, 0);
    lk_text(205.0f, 90.0f, T9, " MS", sk.lab, sk.ground, 0, 0);
    ui_H(0, 114.0f, 44.0f);
    ui_H(1, 164.0f, 82.0f);
}

/* MIDI: the channel (ANY or 1 .. 16), LEARN, the CCs learned (32 at most), PROGRAM; the CCs every sound answers */
static void ui_p_midi(void)
{
    static const char *const CC[9][2] = {{"74", "CUTOFF"}, {"71", "RESO"}, {"5", "GLIDE"}, {"7", "VOLUME"},
                                         {"1", "MOD WHEEL"}, {"16-19", "MACROS"}, {"91", "REVERB MIX"},
                                         {"93", "CHORUS MIX"}, {"94", "DELAY MIX"}};   /* (sy_param.c's cc column) */
    uint32_t i, n = ml_count();
    char t[40];
    for (i = 0; i < 16u; i++) {
        float x = 22.0f + (float)((i % 4u) * 22u), y = 24.0f + (float)((i / 4u) * 18u);
        int on = ui.mch == i + 1u;
        sp_fix(t, (int32_t)i + 1, 0u, 0);
        if (on)
            lk_box(x, y, x + 20.0f, y + 15.0f, ui_ink(ui_hot(0)));
        else
            lk_frame(x + 0.5f, y + 0.5f, 19.0f, 14.0f, 1.0f, sk.line);
        lk_text_c(x + 10.0f, y, on ? T9B : T9, t, on ? sk.ground : sk.lab, on ? ui_ink(ui_hot(0)) : sk.ground, 0, 15);
    }
    if (!ui.mch)
        lk_box(22.0f, 96.0f, 106.0f, 111.0f, ui_ink(ui_hot(0)));
    else
        lk_frame(22.5f, 96.5f, 83.0f, 14.0f, 1.0f, sk.line);
    lk_text_c(64.0f, 96.0f, T9B, "ANY", ui.mch ? sk.lab : sk.ground, ui.mch ? sk.ground : ui_ink(ui_hot(0)), 0, 15);
    lk_text(124.0f, 24.0f, T10, "LEARN", sk.lab, sk.ground, 20, 0);
    ui_toggle(166.0f, 25.5f, ui.learn, ui_hot(1));
    for (i = 0; i < ML_N; i++) {
        float x = 124.0f + (float)((i % 8u) * 8u), y = 46.0f + (float)((i / 8u) * 8u);
        if (i < n)
            lk_box(x, y, x + 5.0f, y + 5.0f, ui_ink(ui_hot(2)));
        else
            lk_frame(x + 0.5f, y + 0.5f, 4.0f, 4.0f, 1.0f, sk.line);
    }
    sp_put(sp_fix(t, (int32_t)n, 0u, 0), " OF 32 LEARNED");
    lk_text(124.0f, 78.0f, T9, t, sk.lab, sk.ground, 0, 0);
    lk_text(124.0f, 98.0f, T10, "PROGRAM", sk.lab, sk.ground, 20, 0);
    ui_toggle(176.0f, 99.5f, ui.mpc, ui_hot(3));
    if (ui.learn) {
        if (ui.ml_pick >= 0)
            sp_put(sp_put(sp_put(sp_put(t, "LEARN: "), SEC_SHORT[SP[ui.ml_pick].sec]), " "), SP[ui.ml_pick].name);
        else
            sp_put(t, "LEARN: TURN A KNOB");
        lk_text(22.0f, 124.0f, T10B, t, sk.ink, sk.ground, 0, 0);
    } else {                                        /* the CCs every sound answers */
        lk_label(22.0f, 124.0f, "EVERY SOUND'S CCS", 20);
        for (i = 0; i < 9u; i++) {
            float x = i < 5u ? 22.0f : 124.0f, y = 140.0f + (float)((i < 5u ? i : i - 5u) * 14u);
            lk_text_r(x + 28.0f, y, T9B, CC[i][0], sk.ink, sk.ground, 0, 0);
            lk_text(x + 33.0f, y, T9, CC[i][1], sk.lab, sk.ground, 0, 0);
        }
    }
    ui_H(0, 2.0f, ui.mch ? 24.0f + (float)(((ui.mch - 1u) / 4u) * 18u) + 3.0f : 99.0f);
    ui_H(1, 196.0f, 26.0f);
    ui_H(2, 196.0f, 52.0f);
    ui_H(3, 206.0f, 100.0f);
}

/* BENCH: what the platform measured (sy_bench), a row each: the bar the last, the tick the peak since CLEAR; RUN */
static char *ui_ms1(char *e, uint32_t us) { return sp_put(sp_fix(e, (int32_t)((us + 50u) / 100u), 1u, 0), " MS"); }
static char *ui_msv(char *e, uint32_t us)          /* ms: a decimal under 10, none above */
{
    return us < 9950u ? sp_fix(e, (int32_t)((us + 50u) / 100u), 1u, 0) : sp_fix(e, (int32_t)((us + 500u) / 1000u), 0u, 0);
}
static void ui_bench_row(uint32_t i, float *v, float *pk, char *t)   /* row i: its bar (0..1), its peak, its figure */
{
    *v = *pk = 0.0f;
    t[0] = 0;
    if (!sy_bench.known) {
        sp_put(t, "\x97");
        return;
    }
    switch (i) {
    case 0:                                         /* a half's render against its 2.9 ms */
        *v = (float)sy_bench.half_us / 2902.0f;
        *pk = (float)sy_bench.half_max_us / 2902.0f;
        {
            char *e = sp_put(sp_fix(t, (int32_t)(sy_bench.half_us * 100u / 2902u), 0u, 0), "%");
            if (sy_bench.late)                      /* (the halves the DMA had left: "38% L3") */
                sp_fix(sp_put(e, " L"), (int32_t)sy_bench.late, 0u, 0);
        }
        break;
    case 1:
        *v = (float)sy_app_load() * 0.01f;
        sp_put(sp_fix(t, sy_app_load(), 0u, 0), "%");
        break;
    case 2:
        *v = (float)sy_bench.frame_us / 50000.0f;
        *pk = (float)sy_bench.frame_max_us / 50000.0f;
        ui_ms1(t, sy_bench.frame_us);
        break;
    case 3:
        *v = *pk = (float)sy_bench.gap_max_us / 20000.0f;
        ui_ms1(t, sy_bench.gap_max_us);
        break;
    case 4:
        *v = *pk = (float)sy_bench.step_max_us / 10000.0f;
        ui_ms1(t, sy_bench.step_max_us);
        break;
    case 5:
        *v = *pk = (float)sy_bench.erase_max_us / 50000.0f;
        ui_ms1(t, sy_bench.erase_max_us);
        break;
    case 6:
        *v = (float)sy_bench.stack_main / 12288.0f;
        *pk = (float)sy_bench.stack_isr / 4096.0f;
        sp_put(sp_fix(t, (int32_t)(sy_bench.stack_main * 100u / 12288u), 0u, 0), sy_bench.spare_hit ? "% HIT" : "%");
        break;
    default:
        if (sy_bench.run == 2u)                     /* "3.9/2.9 MS": the integer loop's, the float loop's */
            sp_put(ui_msv(sp_put(ui_msv(t, sy_bench.run_int_us), "/"), sy_bench.run_flt_us), " MS");
        else
            sp_put(t, sy_bench.run ? "..." : "READY");
        break;
    }
}
static void ui_p_bench(void)
{
    static const char *const ROW[8] = {"AUDIO", "CPU", "SCREEN", "KEYS", "TABLES", "FLASH", "STACKS", "RUN"};
    uint32_t i;
    char t[24];
    float x;
    for (i = 0; i < 8u; i++) {
        float y = 24.0f + (float)(i * 18u), v, pk;
        ui_bench_row(i, &v, &pk, t);
        lk_text(8.0f, y, T9, ROW[i], sk.lab, sk.ground, 40, 0);
        if (i < 7u) {
            lk_seg(54.0f, y + 5.0f, 156.0f, y + 5.0f, 1.0f, sk.line);
            if (v > 0.004f)
                lk_seg(54.0f, y + 5.0f, 54.0f + 102.0f * sy_minf(v, 1.0f), y + 5.0f, 3.0f, sk.ink);
            if (pk > 0.004f)
                lk_seg(54.0f + 102.0f * sy_minf(pk, 1.0f), y + 1.0f, 54.0f + 102.0f * sy_minf(pk, 1.0f), y + 9.0f, 1.0f, sk.ink);
        }
        lk_text(164.0f, y, T9B, t, sk.ink, sk.ground, 0, 0);
    }
    x = lk_chip(20.0f, 176.0f, "RUN", ui_hot(0));
    (void)x;
    lk_chip(120.0f, 176.0f, "PEAKS: CLEAR", ui_hot(1));
    lk_text(20.0f, 196.0f, T9, sy_bench.known ? "TICK: THE PEAK SINCE CLEAR" : "THE EMULATOR TIMES NOTHING", sk.lab,
            sk.ground, 40, 0);
    ui_H(0, 6.0f, 177.0f);
    ui_H(1, 106.0f, 177.0f);
}

/* ----------------------------------------------------------------------------------------- layers --- */
/* the MOD layer (REC held; ENV for ENV 2): the page's four parameters with the source's depth on each, the keys and
 * the sources they pick (REC's) */
static void ui_p_mod(void)
{
    static const char *const SRC2[16] = {"E1", "E2", "L1", "L2", "M1", "M2", "M3", "M4", "VL", "NT", "RD", "AL", "MW",
                                         "BD", "AT", "EX"};
    uint32_t k, wi;
    lk_kb_t kb;
    char t[SP_TEXT_N + 24], f[32];
    sp_put(sp_put(sp_put(sp_put(t, "ON "), ui_sec_name(ui_sec())), " \xB7 "), UI_PG[ui.page].name);
    lk_text(8.0f, 22.0f, T10, t, sk.lab, sk.ground, 20, 0);
    for (k = 0; k < 4u; k++) {                      /* the page's parameters, the source's depth on each */
        float y = 42.0f + (float)(k * 18u);
        int32_t id = ui_virt(UI_PG[ui.page].id[k]);
        int can = id >= 0 && id < UI_ITEM && (SP[id].flags & F_MOD);
        int32_t a = can ? ui_depth(ui.mod_src, (uint32_t)id) : 0;
        lk_text(24.0f, y, T10B, lk_fit(f, sizeof f, id >= 0 && id < UI_ITEM ? SP[id].name : "\x97", T10B, 0, 74.0f),
                can ? sk.ink : sk.lab, sk.ground, 0, 0);
        lk_seg(104.0f, y + 6.5f, 184.0f, y + 6.5f, 1.0f, sk.line);
        lk_seg(144.0f, y + 2.5f, 144.0f, y + 10.5f, 1.0f, sk.mid);
        if (a)
            lk_seg(144.0f, y + 6.5f, 144.0f + (float)a * 0.04f, y + 6.5f, 3.0f, ui.mod_last == id ? sk.hot : sk.ink);
        if (can && a)
            ui_pct(t, a);
        else
            sp_put(t, "\x97");
        lk_text_r(232.0f, y + 1.0f, T9B, t, can ? sk.ink : sk.lab, sk.ground, 0, 0);
        ui_H(k, 6.0f, y + 1.0f);
    }
    lk_kb(&kb, 8.0f, 118.0f, 224.0f, 66.0f);
    for (k = 0, wi = 0; k < 27u; k++) {             /* the sources: each white key's, under it */
        float cx;
        uint32_t src;
        int on;
        if (LK_BLACK[k % 12u])
            continue;
        cx = lk_kb_cx(&kb, k);
        src = UI_KEY_SRC[k];
        on = src == ui.mod_src;
        lk_text_c(cx, 187.0f, on ? T9B : T9, SRC2[wi], on ? sk.ink : sk.lab, sk.ground, 0, 0);
        if (on)
            lk_disc(cx, 172.0f, 2.5f, sk.ink);
        else if (src && ui_src_used(src))
            lk_ring(cx, 172.0f, 2.0f, 1.0f, sk.mid);
        wi++;
    }
}

/* PERFORM (GLO held): the bend (16 detents to an end; back to the middle when let go), the mod wheel, GLIDE, VOLUME */
static void ui_p_perf(void)
{
    static const char *const N[4] = {"BEND", "WHEEL", "GLIDE", "VOLUME"};
    static const float X[4] = {34, 92, 150, 208};
    float by = 100.0f - (float)ui.bend * (52.0f / 8192.0f), wy = 156.0f - 4.0f - (float)ui.wheel * (108.0f / 127.0f), gl;
    uint32_t i;
    float ys[17];
    lk_frame(30.0f, 44.0f, 8.0f, 112.0f, 1.0f, sk.line);
    lk_seg(24.0f, 100.0f, 44.0f, 100.0f, 1.0f, sk.mid);
    lk_box(26.0f, by - 4.0f, 42.0f, by + 4.0f, sk.ink);
    lk_frame(82.0f, 44.0f, 20.0f, 112.0f, 1.0f, sk.ink);
    for (i = 0; i < 18u; i++)
        lk_seg(85.0f, 50.0f + (float)(i * 6u), 99.0f, 50.0f + (float)(i * 6u), 1.0f, sk.line);
    lk_box(82.0f, wy, 102.0f, wy + 4.0f, sk.ink);
    gl = ui_v(sy_patch.p, SP_V_GLIDE);
    lk_dial_arc(X[2], 100.0f, 18.0f, 3.0f, 0.0f, 1.0f, sk.line);
    lk_dial_arc(X[2], 100.0f, 18.0f, 3.0f, 0.0f, sy_maxf(gl, 0.008f), sk.ink);
    for (i = 0; i <= 16u; i++)
        ys[i] = 106.0f - 12.0f * (1.0f - sy_exp2f(-(float)i / 16.0f * 5.77f));
    lk_curve(142, ys, 17u, 1.5f, sk.ink, 0);
    lk_fader(X[3], 44.0f, 156.0f, ui_v(sy_patch.p, SP_V_VOL));
    for (i = 0; i < 4u; i++) {
        lk_text_c(X[i], 166.0f, T10B, N[i], sk.lab, sk.ground, 20, 0);
        ui_H(i, X[i] - 5.0f, 24.0f);
    }
}

/* MACROS (FX held): MACRO 1-4 as dials, the chain's modules (F3 .. F4 bypass them or take them back) */
static void ui_p_mac(void)
{
    static const char *const KEY[NFXC] = {"F3", "G3", "A3", "B3", "C4", "D4", "E4", "F4"};
    uint32_t i;
    char t[SP_TEXT_N + 4];
    for (i = 0; i < 4u; i++) {
        float cx = 36.0f + (float)(i * 56u);
        ui_dial(cx, 56.0f, ui_v(sy_patch.p, SP_MAC1 + i), 0);
        lk_text_c(cx, 74.0f, T9, lk_fit(t, sizeof t, sy_patch.macro[i], T9, 20, 54.0f), sk.lab, sk.ground, 20, 0);
        sp_put(sp_fix(t, (sy_patch.p[SP_MAC1 + i] + 5) / 10, 0u, 0), "%");
        lk_text_c(cx, 86.0f, T10B, t, sk.ink, sk.ground, 0, 0);
        ui_H(i, cx - 5.0f, 24.0f);
    }
    for (i = 0; i < NFXC; i++) {                    /* the chain's modules, each its key under it */
        float x = 8.0f + (float)(i * 28u);
        int32_t in = sp_chain_at(sy_patch.p, i);
        int on = in >= 0 && sy_patch.p[FXI_ON[in]];
        if (in < 0)
            lk_frame(x + 0.5f, 112.5f, 25.0f, 49.0f, 1.0f, sk.line);
        else if (!on)
            lk_frame_dash(x + 0.5f, 112.5f, 25.0f, 49.0f, 2.0f, 2.0f, sk.mid);
        else
            lk_frame(x + 0.5f, 112.5f, 25.0f, 49.0f, 1.0f, sk.ink);
        sp_fix(t, (int32_t)i + 1, 0u, 0);
        lk_text(x + 4.0f, 115.0f, T9, t, sk.lab, sk.ground, 0, 0);
        if (in >= 0)
            lk_icon(FXI_TYPE[in], x + 8.0f, 142.0f, on ? sk.ink : sk.mid);
        lk_text_c(x + 13.0f, 166.0f, in >= 0 ? T9B : T9, KEY[i], in >= 0 ? sk.ink : sk.lab, sk.ground, 0, 0);
    }
    lk_label(8.0f, 186.0f, "DASHED: BYPASSED", 20);
}

/* JUMP (EDIT held): the keys, the section each opens (its code; the one shown filled), the legend */
static void ui_p_jump(void)
{
    static const char *const W[16] = {"OA", "OB", "SB", "NS", "FL", "E1", "E2", "L1", "L2", "MX", "1", "2", "3", "4",
                                      "5", "6"};
    uint32_t k, wi, cur = ui_sec();
    lk_kb_t kb;
    char t[24];
    lk_kb(&kb, 8.0f, 52.0f, 224.0f, 112.0f);
    for (k = 0, wi = 0; k < 27u; k++) {
        uint32_t sec = ui_jump_sec(k);
        float cx = lk_kb_cx(&kb, k);
        int on = sec == cur && sec != 0xFFu;
        if (LK_BLACK[k % 12u]) {                    /* (a black key's word over it) */
            uint32_t c = UI_KEY_JUMP[k];
            const char *s = c == SEC_MACRO ? "HM" : c == SEC_VOICE ? "VC" : c == SEC_GLOBAL ? "GL" : c == SEC_KEYS ? "KY"
                            : c == SEC_MENU ? "ST" : c == SEC_CHAIN ? "FX" : 0;
            if (s) {
                lk_text_c(cx, 34.0f, on ? T9B : T9, s, on ? sk.ink : sk.lab, sk.ground, 0, 0);
                lk_disc(cx, 112.0f, on ? 2.5f : 2.0f, sk.ground);
            }
            continue;
        }
        lk_text_c(cx, 168.0f, on ? T9B : T9, W[wi], on ? sk.ink : sk.lab, sk.ground, 0, 0);   /* (a white key's under it) */
        if (on)
            lk_disc(cx, 150.0f, 2.5f, sk.ink);
        else if (sec != 0xFFu)
            lk_ring(cx, 150.0f, 2.0f, 1.0f, sk.mid);
        wi++;
    }
    sp_put(t, cur == SEC_MACRO ? "HOME" : ui_sec_name(cur));
    lk_text_c(120.0f, 188.0f, T14B, t, sk.ink, sk.ground, 20, 17);
}

/* --------------------------------------------------------------------------------- countdowns --- */
/* a long hold counting down: its seconds left, the ring's part left, what it does (0: none) */
static int ui_cd_view(uint32_t *n, float *frac, char *title, const char **sub, const char **sub2, const char **label)
{
    uint32_t held, dur;
    *sub2 = 0;
    if (ui.ub_t0) {                                 /* OCT- + OCT+ (sy_main.c): the chip's USB boot */
        held = fm1_ms - ui.ub_t0;
        dur = 5000u;
        sp_put(title, "USB BOOT");
        *sub = "BOTH OCT HELD \xB7 LET GO TO STOP";
        *label = "USB BOOT";
    } else if (ui.cd_btn != NB) {
        uint32_t b = ui.cd_btn;
        held = fm1_ms - ui.bt_t[b];
        dur = b == B_OCTDN ? UI_DEL_MS : UI_LONG_MS;
        *sub = "LET GO TO STOP";
        if (b == B_LFO) {
            uint32_t k = ui_lfo();
            sp_draw_t t;
            sp_fix(sp_put(title, "DELETE POINT "), (int32_t)ui_lpt(k, ui_pts(k, &t)) + 1, 0u, 0);
            *label = SEC_NAMES[ui_sec()];
        } else if (b == B_SAVE) {
            sp_fix(sp_put(title, "SAVE OVER USER "), sy_app_slot() + 1, 0u, 0);
            *sub = "LET GO, OCT, A KNOB OR A BUTTON STOP IT";
            *label = "SAVE";
        } else if (b == B_OCTDN) {
            sp_fix(sp_put(title, "DELETE USER "), sy_app_slot() + 1, 0u, 0);
            *sub2 = "SAVE + OCT- STRAIGHT AFTER PUTS IT BACK";
            *label = "BROWSER";
        } else {
            sp_put(title, "SETTINGS");
            *sub = "KEEP HOLDING HOME";
            *label = "HOME";
        }
    } else {
        return 0;
    }
    if (held >= dur)
        held = dur - 1u;
    *n = (dur - held + 999u) / 1000u;
    *frac = (float)(dur - held) / (float)dur;
    return 1;
}
static void ui_p_countdown(void)
{
    uint32_t n;
    float frac;
    char title[32], t[4];
    const char *sub, *sub2, *label;
    if (!ui_cd_view(&n, &frac, title, &sub, &sub2, &label))
        return;
    lk_top(label, -1, 0, 0, 0);
    lk_ring(120.0f, 100.0f, 40.0f, 1.0f, sk.line);
    lk_arc(120.0f, 100.0f, 40.0f, 3.0f, -0.25f, -0.25f + 0.9997f * frac, sk.hot, 1);
    sp_fix(t, (int32_t)n, 0u, 0);
    lk_text_c(120.0f, 86.0f, T26B, t, sk.ink, sk.ground, 0, 28);
    lk_text_c(120.0f, 154.0f, T12B, title, sk.ink, sk.ground, 40, 11);
    lk_text_c(120.0f, 170.0f, T9, sub, sk.lab, sk.ground, 40, 0);
    if (sub2)
        lk_text_c(120.0f, 180.0f, T9, sub2, sk.lab, sk.ground, 40, 0);
}

/* SAFE START: the sound before was not brought back (a crash, a hang or a failed boot came before; or HOME was held
 * at power-on), until a button, a knob or a key, or 6 s */
static void ui_p_safe(void)
{
    lk_ring(120.0f, 76.0f, 30.0f, 1.0f, sk.line);
    lk_ring_dash(120.0f, 76.0f, 20.0f, 2.0f, 2.0f, sk.mid);
    lk_disc(120.0f, 76.0f, 3.0f, sk.ink);
    lk_text_c(120.0f, 120.0f, T14B, "SAFE START", sk.ink, sk.ground, 60, 14);
    lk_text_c(120.0f, 144.0f, T9, "THE LAST SOUND WAS NOT BROUGHT BACK", sk.lab, sk.ground, 20, 0);
    lk_text_c(120.0f, 155.0f, T9, ui.safe == 2u ? "(HOME HELD AT POWER-ON)" : "(AFTER A CRASH OR A HANG)", sk.lab,
              sk.ground, 20, 0);
    lk_text_c(120.0f, 166.0f, T9, "INIT IS PLAYING \xB7 PRESETS PICKS A SOUND", sk.ink, sk.ground, 20, 0);
}

/* power-on: two blooms turning against each other (still here: the splash is drawn once), the dial's ticks, the name */
static float ui_boot_f(float x, int k)             /* a three-petalled frame, of two kinds */
{
    return k ? 0.7f * sy_sin2pi(3.0f * x) + 0.3f * sy_sin2pi(6.0f * x + 0.15f)
             : 0.6f * sy_sin2pi(3.0f * x + 0.1f) + 0.25f * sy_sin2pi(9.0f * x) + 0.15f * sy_sin2pi(12.0f * x + 0.3f);
}
static void ui_p_boot(void)
{
    uint32_t i, k;
    float vx = 1.0f, vy = 0.0f, cs = sy_cos2pi(1.0f / 48.0f), sn = sy_sin2pi(1.0f / 48.0f);
    char t[40], f[48];
    for (i = 0; i < 48u; i++) {
        float r0 = i % 12u ? 55.0f : 52.0f, nx;
        lk_seg(120.0f + r0 * vx, 92.0f + r0 * vy, 120.0f + 57.0f * vx, 92.0f + 57.0f * vy, 1.0f, i % 12u ? sk.line : sk.mid);
        nx = vx * cs - vy * sn;
        vy = vx * sn + vy * cs;
        vx = nx;
    }
    for (k = 0; k < 2u; k++) {
        float px = 0.0f, py = 0.0f, ux, uy, c2 = sy_cos2pi(1.0f / 160.0f), s2 = sy_sin2pi(1.0f / 160.0f);
        ux = sy_cos2pi(k ? 0.08f : -0.25f);
        uy = sy_sin2pi(k ? 0.08f : -0.25f);
        for (i = 0; i <= 160u; i++) {
            float r = 30.0f + 14.0f * ui_boot_f((float)i / 160.0f, (int)k), x = 120.0f + r * ux, y = 92.0f + r * uy, nx;
            if (i)
                lk_wu(px, py, x, y, k ? 1.0f : 1.5f, k ? sk.mid : sk.ink);
            px = x;
            py = y;
            nx = ux * c2 - uy * s2;
            uy = ux * s2 + uy * c2;
            ux = nx;
        }
    }
    lk_text_c(120.0f, 160.0f, T26B, "STRATA", sk.ink, sk.ground, 120, 28);
    sp_put(sp_put(t, "WAVETABLE \xB7 FM-1 \xB7 "), STRATA_VERSION);
    lk_text_c(120.0f, 194.0f, T9, lk_fit(f, sizeof f, t, T9, 100, 224.0f), sk.lab, sk.ground, 100, 0);
}

/* ------------------------------------------------------------------------------------------ chrome --- */
/* the section's top line: its name (an LFO's rate and mode with it; the chain view's and a module's place), its dot,
 * its family's dots or the chain's squares, the right word */
static void ui_top(void)
{
    uint32_t sec = ui_sec(), f = ui_fam_of(sec), n = 0, cur = 0, i;
    uint8_t secs[NFXC];
    int32_t pw = ui_sec_power(sec);
    char t[40], r[16], *e;
    const char *right = 0;
    if (ui.learn && sec != SEC_MENU) {              /* MIDI LEARN, listening: its banner on every page */
        lk_banner("MIDI LEARN", "OCT+ ENDS IT");
        return;
    }
    e = sp_put(t, sec == SEC_MACRO ? "HOME" : ui_sec_name(sec));
    if (sec == SEC_ENV1)
        sp_put(e, " \xB7 AMP");
    else if (sec == SEC_LFO1 || sec == SEC_LFO2) {
        uint32_t b = UI_LB(sec == SEC_LFO2), mode = (uint32_t)sy_patch.p[SP_L1_MODE + b];
        e = sp_put(e, " \xB7 ");
        if (sy_patch.p[SP_L1_SYNC + b])
            e = sp_put(e, NOTE_NAMES[sy_patch.p[sec == SEC_LFO2 ? SP_L2_NOTE : SP_L1_NOTE] % SY_NNOTE]);
        else {
            sp_text(SP_L1_RATE + b, sy_patch.p[SP_L1_RATE + b], e);
            while (*e)
                e++;
        }
        sp_put(sp_put(e, " \xB7 "), LFOM_NAMES[mode % 3u]);
    } else if (sec == SEC_MATRIX) {
        for (n = 0, i = 0; i < NSLOT; i++)
            n += sy_patch.p[SP_M1_SRC + 3u * i] > SRC_NONE && sy_patch.p[SP_M1_AMT + 3u * i] != 0;
        sp_put(sp_fix(sp_put(e, " \xB7 "), (int32_t)n, 0u, 0), " OF 8");
    } else if (sec == SEC_CHAIN) {                  /* "FX · 5 OF 8 · CPU 31%" (the cost model's) */
        uint32_t cost = 0, nc = sp_chain_n(sy_patch.p);
        for (i = 0; i < nc; i++)
            cost += FXT_COST[FXI_TYPE[sp_chain_at(sy_patch.p, i)]];
        e = sp_put(t, "FX \xB7 ");
        e = sp_put(sp_fix(e, (int32_t)nc, 0u, 0), " OF 8 \xB7 CPU ");
        sp_put(sp_fix(e, (int32_t)cost, 0u, 0), "%");
    } else if (ui_is_fx(sec)) {                     /* "FX 3 · DELAY" */
        int32_t at = sp_chain_find(sy_patch.p, sec - SEC_DRIVE);
        e = sp_put(t, "FX ");
        e = at >= 0 ? sp_fix(e, at + 1, 0u, 0) : sp_put(e, "-");
        sp_put(sp_put(e, " \xB7 "), SEC_NAMES[sec]);
    } else if (sec == SEC_SAVE || sec == SEC_BROWSE) {
        ui_save_label(t);
    }
    if (sec == SEC_MACRO) {
        sp_put(sp_fix(r, (int32_t)ui_bpm_now(), 0u, 0), " BPM");
        right = r;
    } else if (sec == SEC_KEYS || sec == SEC_ARP) { /* (V0.7: the chord playing, by name; else the octave) */
        if (chd.mode != CH_OFF && chs.built && chs.n)
            sy_chord_name(r);
        else
            sp_fix(sp_put(r, "OCT "), sy_app_oct(), 0u, 1);
        right = r;
    }
    n = 0;
    if (f == FAM_EDIT || f == FAM_ENV || f == FAM_LFO || f == FAM_GLO) {
        n = ui_fam_secs(f, secs);
        for (i = 0; i < n; i++)
            if (secs[i] == sec)
                cur = i;
        if (n < 2u)
            n = 0;
    }
    lk_top(t, pw >= 0 ? sy_patch.p[pw] != 0 : -1, n, cur, right);
    if (sec == SEC_MACRO)
        ui_pulse();
    if (ui_is_fx(sec)) {                            /* the chain's squares */
        uint32_t nc = sp_chain_n(sy_patch.p), byp = 0;
        for (i = 0; i < nc; i++)
            byp |= (uint32_t)(!sy_patch.p[FXI_ON[sp_chain_at(sy_patch.p, i)]]) << i;
        lk_chain_sq(nc, sp_chain_find(sy_patch.p, sec - SEC_DRIVE), byp);
    }
}
/* the bottom row: the toast, MIDI LEARN's word, or the section's pages as zones (HOME, the browser, NAME IT: none) */
static void ui_bottom(void)
{
    uint32_t sec = ui_sec(), p, n = 0, cur = 0, nv = 0, vat = 0, vi = 0;
    const char *z[8];
    char vz[16];
    if (ui.toast[0]) {
        lk_toast(ui.toast);
        return;
    }
    if (ui.learn && sec != SEC_MENU) {
        char t[40];
        if (ui.ml_pick >= 0)
            sp_put(sp_put(sp_put(sp_put(sp_put(t, "PICKED: "), SEC_SHORT[SP[ui.ml_pick].sec]), " "), SP[ui.ml_pick].name),
                   " \xB7 MOVE A CC");
        else
            sp_put(t, "TURN A KNOB TO PICK ITS PARAMETER");
        lk_toast(t);
        return;
    }
    if (sec == SEC_MACRO || sec == SEC_SAVE || sec == SEC_BROWSE)
        return;
    for (p = 0; p < UI_NPG; p++) {                  /* the pages, the VALUES pages as one zone */
        if (UI_PG[p].sec != sec)
            continue;
        if (UI_PG[p].graph == G_VALUES) {
            if (!nv++ && n < 8u) {
                vi = n;
                z[n++] = "VALUES";
            }
            if (p == ui.page) {
                vat = nv;
                cur = vi;
            }
            continue;
        }
        if (p == ui.page)
            cur = n;
        if (n < 8u)
            z[n++] = UI_PG[p].name;
    }
    if (vat && nv > 1u) {                           /* ("VALUES 2/5"; among six zones "VAL 2/5": the others keep */
        sp_fix(sp_put(sp_fix(sp_put(vz, n >= 6u ? "VAL " : "VALUES "), (int32_t)vat, 0u, 0), "/"), (int32_t)nv, 0u, 0);   /* three letters) */
        z[vi] = vz;
    }
    lk_zones(z, n, cur);
}

/* ------------------------------------------------------------------------------------------- pages --- */
static void ui_p_lfo(void)
{
    int32_t k0 = UI_PG[ui.page].id[0];
    if (k0 == UI_LPT)
        ui_p_lfo_points();
    else if (k0 == SP_L1_SHAPE || k0 == SP_L2_SHAPE)
        ui_p_lfo_shape();
    else
        ui_p_lfo_time();
}
static void ui_p_none(void) {}

/* VALUES (V0.67): the page's four knobs as figures, a row each: its name, its value large, its place on its range
 * (a bipolar one's from the middle), the range its modulation takes it and the source's word; the knobs here step
 * the finest unit at any speed (sy_ui.c ui_knob), its figure HOT while one turns (no flag: the figure is it) */
static void ui_p_values(void)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        float y = 22.0f + (float)(k * 49u);
        char v[SP_TEXT_N + 8], f[40], m[32];
        const char *n = ui_knob_nv(k, v);
        int32_t id = ui_virt(UI_PG[ui.page].id[k]);
        int hot = ui_hot(k);
        if (id < 0 || !n)
            continue;
        lk_text(26.0f, y, T10B, lk_fit(f, sizeof f, n, T10B, 20, 120.0f), sk.lab, sk.ground, 20, 0);
        lk_text(26.0f, y + 12.0f, T18B, lk_fit(f, sizeof f, v, T18B, 0, 206.0f), ui_ink(hot), sk.ground, 0, 22);
        if (id < UI_ITEM) {                         /* its place on its range (bipolar: from the middle), the range */
            int32_t e = ui_eff(id);                 /* its modulation takes it */
            const sp_desc_t *d = &SP[e];
            float u = ui_v(sy_patch.p, (uint32_t)e), x = 26.0f + 206.0f * u, z = 26.0f, olo, ohi, plo, phi;
            if (d->lo < 0 && d->hi > 0)
                z = 26.0f + 206.0f * sp_norm_of((uint32_t)e, 0);
            lk_seg(26.0f, y + 39.0f, 232.0f, y + 39.0f, 1.0f, sk.line);
            if (z != 26.0f)
                lk_seg(z, y + 36.0f, z, y + 42.0f, 1.0f, sk.mid);
            if (sy_absf(x - z) > 0.5f)
                lk_seg(sy_minf(x, z), y + 39.0f, sy_maxf(x, z), y + 39.0f, 3.0f, ui_ink(hot));
            lk_box(x - 1.5f, y + 34.5f, x + 1.5f, y + 43.5f, ui_ink(hot));
            if (ui_mod_span((uint32_t)e, SRC_COUNT, &olo, &ohi, &plo, &phi) && ohi > olo) {
                float xa = 26.0f + 206.0f * sy_clampf(u + olo, 0.0f, 1.0f), xb = 26.0f + 206.0f * sy_clampf(u + ohi, 0.0f, 1.0f);
                lk_seg(xa, y + 45.0f, xb, y + 45.0f, 1.0f, sk.mid);
                lk_seg(xa, y + 43.0f, xa, y + 47.0f, 1.0f, sk.mid);
                lk_seg(xb, y + 43.0f, xb, y + 47.0f, 1.0f, sk.mid);
            }
            if (ui_mod_word((uint32_t)e, m))
                lk_text_r(232.0f, y + 1.0f, T9, m, sk.lab, sk.ground, 0, 0);
        }
        ui_H(k, 8.0f, y + 1.0f);
    }
}
/* each G_*'s page, called through this table: called from a switch, every one of them was inlined into the caller and
 * their locals did not share the stack (V0.6: one 17,284-byte frame); through the table, the deepest is one page's */
static void (*const UI_PAGE[])(void) = {
    [G_LIST] = ui_p_none, [G_WAVE] = ui_p_wave, [G_UNI] = ui_p_unison, [G_SPREAD] = ui_p_spread,
    [G_PITCH] = ui_p_pitch, [G_OUT] = ui_p_out, [G_SUB] = ui_p_sub, [G_NOISE] = ui_p_noise, [G_ENV] = ui_p_env,
    [G_LFO] = ui_p_lfo, [G_TEMPO] = ui_p_tempo, [G_COLOR] = ui_p_color, [G_HOME] = ui_p_home,
    [G_FILTER] = ui_p_filter, [G_DRIVE] = ui_p_drive, [G_CHORUS] = ui_p_chorus, [G_DELAY] = ui_p_delay,
    [G_REVERB] = ui_p_reverb, [G_OTT] = ui_p_ott, [G_EQ] = ui_p_eq, [G_SAVE] = ui_p_save, [G_BROWSE] = ui_p_browse,
    [G_MATRIX] = ui_p_matrix, [G_KEYS] = ui_p_keys, [G_PANEL] = ui_p_panel, [G_MIDI] = ui_p_midi,
    [G_BENCH] = ui_p_bench, [G_CHAIN] = ui_p_chain, [G_FLANGER] = ui_p_flanger, [G_PHASER] = ui_p_phaser,
    [G_FFILT] = ui_p_ffilt, [G_CRUSH] = ui_p_crush, [G_TREM] = ui_p_trem, [G_RING] = ui_p_ring, [G_TAPE] = ui_p_tape,
    [G_VOICE] = ui_p_voice, [G_PLAY] = ui_p_play, [G_VALUES] = ui_p_values, [G_CHORD] = ui_p_chord,
    [G_PERF] = ui_p_cperf, [G_BASS] = ui_p_cbass, [G_CMIDI] = ui_p_cmidi, [G_STEPS] = ui_p_steps, [G_RATCH] = ui_p_ratch,
    [G_GROOVE] = ui_p_groove,
};
_Static_assert(sizeof UI_PAGE / sizeof UI_PAGE[0] == G_N, "a page for every G_*");

/* what the screen shows now: a countdown, SAFE START, a layer, or the page */
enum { V_PAGE, V_CD, V_SAFE, V_MOD, V_PERF, V_MAC, V_JUMP };
static uint32_t ui_view(void)
{
    if (ui.ub_t0 || (ui.cd_btn != NB && fm1_ms - ui.bt_t[ui.cd_btn] >= HOLD_MS[settings_hold % 4u]))
        return V_CD;
    if (ui.safe)
        return V_SAFE;
    if (ui.mod)
        return V_MOD;
    return ui.lay == LAY_PERF ? V_PERF : ui.lay == LAY_MAC ? V_MAC : ui.lay == LAY_JUMP ? V_JUMP : V_PAGE;
}

/* MOMENTARY (LFO held): the page, with the values it goes back to as its shape's dashes (their patch, ~0.9 KB, on
 * this path's stack only) */
static SY_NOINLINE void ui_page_mom(uint32_t g)
{
    sy_patch_t mom;
    char t[24];
    if (ui.mom_n) {
        uint32_t i;
        mom = sy_patch;
        for (i = 0; i < ui.mom_n; i++)
            mom.p[ui.mom_id[i]] = ui.mom_v[i];
        pg.gh = &mom;
        pg.mom = 1;
        sp_put(sp_put(sp_fix(sp_put(t, "LET GO: "), ui.mom_n, 0u, 0), ui.mom_n == 1u ? " GOES" : " GO"), " BACK");
    } else {
        sp_put(t, "A KNOB: FOR NOW");
    }
    lk_banner("MOMENTARY", t);
    UI_PAGE[g < G_N ? g : G_LIST]();
    if (ui.toast[0])
        lk_toast(ui.toast);
    pg.gh = 0;                                      /* (mom goes with this frame) */
}

/* the screen, into the window lk_begin set (a strip, a live part's box) */
static void ui_page(void)
{
    uint32_t v = ui_view(), g = UI_PG[ui.page].graph, k;
    pg.has = 0;
    pg.figs = 0;
    pg.live_lo = 1e9f;
    pg.live_hi = -1e9f;
    pg.mom = 0;
    pg.gh = ui_ghost();
    switch (v) {
    case V_CD: ui_p_countdown(); return;
    case V_SAFE: ui_p_safe(); return;
    case V_MOD: {
        char t[24];
        sp_put(sp_put(t, "MOD \xB7 "), SRC_NAMES[ui.mod_src]);
        lk_banner(t, ui.mod == 2u ? (ui.mod_btn == B_REC ? "TAP REC: CLOSES" : "TAP ENV: CLOSES") : "LET GO: CLOSES");
        ui_p_mod();
        break;
    }
    case V_PERF: lk_banner("PERFORM", "LET GO: BEND RETURNS"); ui_p_perf(); break;
    case V_MAC: lk_banner("MACROS", "LET GO: CLOSES"); ui_p_mac(); break;
    case V_JUMP: lk_banner("JUMP", "A KEY OPENS IT"); ui_p_jump(); break;
    default:
        if (ui.lay == LAY_MOM) {
            ui_page_mom(g);
            break;
        }
        ui_top();
        UI_PAGE[g < G_N ? g : G_LIST]();
        ui_bottom();
        break;
    }
    for (k = 1; k < 4u; k++) {                      /* (a handle on another's place: moved along the row, clear of it) */
        uint32_t j, n = 0;
        for (j = 0; j < k && n < 4u; j++)
            if ((pg.has >> k & 1u) && (pg.has >> j & 1u) && sy_absf(pg.hx[k] - pg.hx[j]) < 15.0f &&
                sy_absf(pg.hy[k] - pg.hy[j]) < 15.0f) {
                pg.hx[k] = pg.hx[j] + (pg.hx[j] < 210.0f ? 16.0f : -16.0f);
                j = (uint32_t)-1;                   /* (and again against all of them) */
                n++;
            }
    }
    for (k = 0; k < 4u; k++)                        /* the handles, the knob turning's HOT, its value in a flag */
        if (pg.has >> k & 1u) {
            int32_t id = ui_virt(UI_PG[ui.page].id[k]);
            int hot = ui_hot(k) || (v == V_PAGE && ui.learn && ui.ml_pick >= 0 && id >= 0 && ui_eff(id) == ui.ml_pick) ||
                      (v == V_MOD && ui.mod_last >= 0 && id == ui.mod_last);
            lk_handle(pg.hx[k], pg.hy[k], k + 1u, hot);
        }
    if (v == V_PAGE && g != G_VALUES && !pg.figs && ui_turning() >= 0 && (pg.has >> ui.act_k & 1u)) {
        char t[48];
        ui_flag_text(ui.act_k, t);
        if (t[0])
            lk_flag(pg.hx[ui.act_k], pg.hy[ui.act_k], t);
    }
}

/* ------------------------------------------------------------------------------------------ frame --- */
/* the panel's rows as they are, a hash each (0: not known: the row goes) */
#define UI_STRIP 80u
static uint32_t ui_rowh[240];

/* rows y0 .. y1 - 1 of the screen, in strips; only the rows that changed go (fresh: a new frame, its curves
 * computed again) */
static float ui_live_lo = 1e9f, ui_live_hi = -1e9f;   /* the rows the live parts were drawn on last */
static void ui_band(uint32_t y0, uint32_t y1, int fresh)
{
    uint32_t s, y, x;
    if (fresh)
        ui_frame_n++;
    for (s = y0; s < y1; s += UI_STRIP) {
        uint32_t h = y1 - s < UI_STRIP ? y1 - s : UI_STRIP, lo = 240u, hi = 0u;
        lk_begin(0, (int32_t)s, 240u, h);
        ui_page();
        for (y = 0; y < cv_h; y++) {
            const uint16_t *p = &cv_px[y * cv_w];
            uint32_t hs = 2166136261u;              /* (FNV-1a, two pixels a step) */
            for (x = 0; x + 1u < cv_w; x += 2u)
                hs = (hs ^ ((uint32_t)p[x] | (uint32_t)p[x + 1u] << 16)) * 16777619u;
            hs |= 1u;
            if (hs != ui_rowh[s + y]) {
                ui_rowh[s + y] = hs;
                lo = y < lo ? y : lo;
                hi = y;
            }
        }
        if (lo <= hi)
            lcd_blit(0, s + lo, cv_w, hi - lo + 1u, cv_px + lo * cv_w);
    }
    ui_live_lo = pg.live_lo;
    ui_live_hi = pg.live_hi;
}
/* a live frame (voices on an envelope or an LFO, OTT's levels, the pills): where the live parts are now found by
 * the page placed in an empty window (nothing drawn), then only the rows they take now or took before redrawn */
static void ui_live_band(void)
{
    float lo, hi;
    ui_frame_n++;
    lk_begin(0, 0, 240u, 0u);
    ui_page();
    lo = sy_minf(pg.live_lo, ui_live_lo) - 2.0f;
    hi = sy_maxf(pg.live_hi, ui_live_hi) + 2.0f;
    if (hi >= lo)
        ui_band((uint32_t)sy_clampi(sy_floori(lo), 0, 240), (uint32_t)sy_clampi(sy_floori(hi) + 1, 0, 240), 0);
}
/* a live part: the screen redrawn in the box (x, y, w, h) and sent (disc: only each row's part inside the circle of
 * radius UI_BLOOM_R round (UI_BX, UI_BY): the bloom's); its rows' hashes forgotten (the next strip sends them) */
static void ui_win(int32_t x, int32_t y, int32_t w, int32_t h, int disc)
{
    int16_t ca[112], cb[112];                       /* (the disc: each row's span, cleared and sent) */
    int32_t r;
    ui_frame_n++;
    lk_live = 1;
    if (!disc) {
        lk_begin(x, y, (uint32_t)w, (uint32_t)h);
        ui_page();
    } else {
        lk_place(x, y, (uint32_t)w, (uint32_t)(h < 112 ? h : 112));   /* (only the disc cleared: its corners unsent) */
        for (r = 0; r < (int32_t)cv_h; r++) {
            float dy = (float)(y + r) + 0.5f - UI_BY, hw;
            int32_t a = 0, b = 0, i;
            if (sy_absf(dy) < UI_BLOOM_R) {
                hw = sy_sqrtf(UI_BLOOM_R * UI_BLOOM_R - dy * dy);
                a = sy_clampi(sy_floori(UI_BX - hw + 0.5f), x, x + (int32_t)cv_w);
                b = sy_clampi(sy_floori(UI_BX + hw + 0.5f), x, x + (int32_t)cv_w);
            }
            ca[r] = (int16_t)a;
            cb[r] = (int16_t)b;
            for (i = a; i < b; i++)
                cv_px[(uint32_t)r * cv_w + (uint32_t)(i - x)] = swap16(sk.ground);
        }
        ui_bloom();
    }
    lk_live = 0;
    for (r = 0; r < (int32_t)cv_h; r++) {
        int32_t sy = y + r;
        if (sy >= 0 && sy < 240)
            ui_rowh[sy] = 0;
        if (disc && cb[r] > ca[r])
            lcd_blit((uint32_t)ca[r], (uint32_t)sy, (uint32_t)(cb[r] - ca[r]), 1u,
                     cv_px + (uint32_t)r * cv_w + (uint32_t)(ca[r] - x));
    }
    if (!disc)
        lcd_blit((uint32_t)x, (uint32_t)y, cv_w, cv_h, cv_px);
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
        } else if (ui.lay == LAY_MAC && UI_KEY_FX[k] >= 0) {   /* MACROS: the modules' keys, lit when in, */
            int32_t in = sp_chain_at(sy_patch.p, (uint32_t)UI_KEY_FX[k]);   /* glowing when bypassed */
            if (in >= 0)
                ui_led(sy_patch.p[FXI_ON[in]] ? fm1_led : fm1_led_dim, 14u + k);
        } else if (ui.lay == LAY_JUMP && ui_jump_sec(k) != 0xFFu) {   /* JUMP: the keys that open a section, */
            ui_led(ui_jump_sec(k) == ui_sec() ? fm1_led : fm1_led_dim, 14u + k);   /* the one shown lit */
        } else if (sy_app_held(k)) {
            ui_led(fm1_led, 14u + k);
        } else if (chd.mode == CH_FREE && k < CH_NBTN && ui_sec() != SEC_SAVE) {   /* FREE's chord buttons: the */
            int lit = k < 4u ? k == (chd.type & 3u) : ((chd.ext >> (k - 4u)) & 1u) != 0u;   /* latched lit, the rest */
            ui_led(lit ? fm1_led : fm1_led_dim, 14u + k);                                     /* glowing (V0.7) */
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
    if (chd.perf && !(chs.pf && ui.abeat_t && fm1_ms - ui.abeat_t < 70u))   /* ARP: PERFORM on (V0.7); ARP's and */
        ui_led(fm1_led, panel.btn[B_ARP]);                                    /* PATTERN's beats dark a moment */
    if (ui.run) {                                   /* PLAY: its green LED while running, its own each beat (a */
        float b = ui_beats();                       /* quarter of it lit; V0.75: the audio side's beat) */
        fm1_led[UI_LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (UI_LED_PLAY_GREEN & 7u));
        if (b - (float)sy_floori(b) < 0.25f)
            ui_led(fm1_led, panel.btn[B_PLAY]);
    }
    if (ui.mod == 2u)                               /* a latched MOD layer's button, a hold layer's: breathing */
        ui_led_breathe(panel.btn[ui.mod_btn]);
    else if (ui.lay)
        ui_led_breathe(panel.btn[ui.lay_btn]);
}

/* ------------------------------------------------------------------------------------------ frame --- */
/* a frame (~60 a second): the knob turning let go, what is live (each part at its own pace, only while it moves),
 * the screen if anything changed, then the LEDs */
static void sy_ui_frame(void)
{
    uint32_t g = UI_PG[ui.page].graph, v = ui_view(), nv = ui_nvoices(), live = 0;
    int snd = ui_sounding();
    static uint8_t was_live, was_busy, was_nv, was_view, pulse, bloom, band;
    static uint32_t load_t, orbit_t, swing_t, cd_t, reel_t, beat;
    if ((ui.dirty & D_ALL) == D_ALL)                /* (everything redrawn: everything sent, whatever the panel had) */
        memset(ui_rowh, 0, sizeof ui_rowh);
    if (ui.act_k < 4u && fm1_ms - ui.act_t >= UI_GESTURE_MS) {   /* (the handle, the shape as it rests) */
        ui.dirty |= D_DISP;
        ui.act_k = 0xFF;
    }
    if (ui.safe && (int32_t)(fm1_ms - ui.safe_t) >= 0) {
        ui.safe = 0;
        ui.dirty |= D_ALL;
    }
    if (v != was_view) {
        was_view = (uint8_t)v;
        ui.dirty |= D_DISP;
    }
    if (v == V_CD) {                                /* a countdown: its ring as it empties, its seconds */
        if (fm1_ms - cd_t >= 40u) {
            cd_t = fm1_ms;
            ui.dirty |= D_LIVE;
        }
    } else if (v == V_PAGE) {
        if (g == G_ENV || (g == G_LFO && UI_PG[ui.page].id[0] == UI_LPT))   /* the voices on an envelope, an LFO */
            live = nv != 0;
        else if (g == G_OTT)                        /* OTT's levels (the module in) */
            live = sy_patch.p[ui_sec_power(ui_sec())] && snd;
        else if (g == G_EQ)
            live = snd && ui_eq_modded();
        else if (g == G_VOICE)                      /* the voices playing */
            live = nv != was_nv;
        if ((live || was_live) && fm1_ms - ui.live_t >= 70u) {
            ui.live_t = fm1_ms;
            band = 1;                               /* (their rows alone: ui_live_band) */
            was_live = (uint8_t)live;
        }
        if ((g == G_ENV || g == G_LFO) && nv != was_nv)
            ui.dirty |= D_DISP;                     /* (the voices' count, in words) */
        if (g == G_HOME) {                          /* the bloom: while sound plays, and once after (silence, flat), */
            static uint8_t fresh, flat = 1;         /* a capture kept (drawn when it differs), the next asked for */
            uint32_t i;
            if (sy_scope.st == 2u) {
                SY_CBAR();
                if (!ui_scope_ok || memcmp(ui_scope, sy_scope.v, sizeof ui_scope)) {
                    memcpy(ui_scope, sy_scope.v, sizeof ui_scope);
                    fresh = 1;
                }
                for (flat = 1, i = 0; i < 2u * SY_SCOPE_N; i++) {   /* (a capture under -66 dB: flat on the screen) */
                    int32_t s = ui_scope[i / SY_SCOPE_N][i % SY_SCOPE_N];
                    flat &= s > -16 && s < 16;
                }
                ui_scope_ok = 1;
                sy_scope.st = 0;
            }
            if (sy_scope.st == 0u && (snd || !flat))
                sy_scope_arm(ui_scope_step());
            if ((fresh || (ui_bl & 128u)) && fm1_ms - ui.live_t >= 50u) {   /* (20 a second: V0.66's scope 25; */
                                                                             /* the bloom still moving) */
                uint32_t dt = fm1_ms - ui.live_t;   /* (since the last: after a pause, a frame's step) */
                ui_bloom_step(dt < 200u ? dt : 50u, snd);
                ui.live_t = fm1_ms;
                ui.dirty |= D_LIVE;
                bloom = 1;
                fresh = 0;
            }
            if (ui_pulse_lv() != pulse)             /* the tempo's dot */
                ui.dirty |= D_LIVE;
            if (nv != was_nv)
                ui.dirty |= D_DISP;
        } else {
            if (sy_scope.st == 1u)
                sy_scope.st = 0;                    /* (another page: no capture) */
            ui_open_q = ui_twist_q = 0;             /* (the bloom a bud again, still: HOME shows it so in silence) */
            ui_bl = (sy_age & 0xFFFFFFu) << 8;
        }
        if (g == G_LFO && snd && fm1_ms - orbit_t >= 33u) {   /* an LFO's dial */
            orbit_t = fm1_ms;
            ui.dirty |= D_LIVE;
        }
        if (g == G_TEMPO && ui.run) {               /* the metronome: the arm, the beat */
            if (fm1_ms - swing_t >= 33u) {
                swing_t = fm1_ms;
                ui.dirty |= D_LIVE;
            }
            if (ui_beat_now() != beat) {
                beat = ui_beat_now();
                ui.dirty |= D_DISP;
            }
        }
        if (g == G_TAPE && snd && sy_patch.p[ui_sec_power(ui_sec())] && fm1_ms - reel_t >= 50u) {   /* the reels */
            reel_t = fm1_ms;
            ui.dirty |= D_LIVE;
        }
        if (g == G_BENCH && fm1_ms - load_t >= 250u) {   /* BENCH: its figures, four times a second when they move */
            static uint32_t sum;
            const uint32_t *w = (const uint32_t *)(const void *)&sy_bench;
            uint32_t s = (uint32_t)sy_app_load() * 31u + sy_g.level + sy_g.fx_lo * 7u + sy_g.shed * 131u, i;
            for (i = 0; i < sizeof sy_bench / 4u; i++)
                s = s * 33u + w[i];
            load_t = fm1_ms;
            if (s != sum) {
                sum = s;
                ui.dirty |= D_DISP;
            }
        }
        if (g == G_WAVE || g == G_UNI || g == G_SPREAD || g == G_SUB) {
            uint8_t busy = wt_tab[g == G_SUB ? 0u : ui_osc()].busy;
            if (busy != was_busy)
                ui.dirty |= D_DISP;                 /* (a table built: its frames) */
            was_busy = busy;
        }
        if (g == G_WAVE && (nv != 0) != (was_nv != 0))
            ui.dirty |= D_DISP;                     /* (the frame playing's dot) */
    }
    was_nv = (uint8_t)nv;
    if (ui.dirty & D_ALL) {                         /* anything: the screen, its changed rows sent; a knob's turn */
        uint32_t y0 = 0u, y1 = 240u;                /* alone: the body (its top line and its zones as they were; */
        if (!(ui.dirty & (D_HEAD | D_FOOT)) && v == V_PAGE && g != G_HOME)   /* HOME's macros reach row 228) */
            y0 = 16u, y1 = 222u;
        ui.dirty = 0;
        ui_band(y0, y1, 1);
        pulse = (uint8_t)ui_pulse_lv();
        bloom = 0;
        band = 0;
    } else if (band) {
        band = 0;
        ui_live_band();
    } else if (ui.dirty & D_LIVE) {
        ui.dirty &= (uint8_t)~D_LIVE;
        if (v == V_CD) {
            ui_win(76, 56, 89, 89, 0);              /* (the ring and its seconds) */
        } else if (g == G_HOME) {
            if (bloom)
                ui_win((int32_t)UI_BX - 50, (int32_t)UI_BY - 50, 100, 100, 1);
            bloom = 0;
            if (ui_pulse_lv() != pulse) {           /* the tempo's dot alone: nothing else in its box */
                uint32_t r;
                pulse = (uint8_t)ui_pulse_lv();
                lk_begin(sy_floori(ui_pulse_x()) - 4, 5, 9u, 9u);
                ui_pulse();
                lcd_blit((uint32_t)lk_x0, 5u, 9u, 9u, cv_px);
                for (r = 5; r < 14u; r++)
                    ui_rowh[r] = 0;
            }
        } else if (g == G_LFO) {
            ui_win(193, 21, 27, 27, 0);
        } else if (g == G_TEMPO) {
            ui_win(24, 50, 92, 126, 0);
        } else if (g == G_TAPE) {
            ui_tape_wins();
        }
    }
    sy_ui_leds();
}
