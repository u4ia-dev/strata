/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA's filter and effects pages (the Strata Screen Design canvas, "Filter and effects, redone"): each draws what it
 * does, from the values as the DSP reads them, in sy_ui_draw.c's display window:
 *   FILTER: the response over 20 Hz .. 20 kHz (each type's prototype at the SVF's warped frequency: what plays, the
 *     drive aside), the range modulation sweeps the cutoff, VAR's two sides; MORE: the cutoff following the key (the
 *     curves at C2, C4 and C6);
 *   the effects, under the rack (the six, the one shown outlined, each lit when on): DRIVE its curve and a sine
 *     through it; CHORUS its four taps sweeping (live); DELAY its echoes (ping-pong: left above, right below) and their
 *     fall, on TONE what the loop's filters leave of each pass; REVERB its decay (the early reflections, the tail, the
 *     highs dying first); OTT its three bands, their levels live and which way each is pushed; EQ its curve, each
 *     band's handle;
 *   a knob turning (sy_ui.c ui_act): the part of the graph it moves in NOW, its value in a flag, the shape before the
 *     turn dashed behind (ui_ghost); a modulated value: the range it sweeps in MOD.
 * Needs sy_ui.c, sy_ui_draw.c (WX WY WW WH, ui_label, ui_text_c), sy_gfx.c and the engine (sy_filter.c, sy_fx.c). */

static float ui_v(const int16_t *p, uint32_t id) { return sp_norm_of(id, p[id]); }   /* a value as 0..1 */

/* a flag: the value a knob moves, beside what it moves, kept inside the window */
static void ui_flag(int32_t x, int32_t y, const char *s)
{
    int32_t w = text_w(&AF_XS, s) + 8;
    x = sy_clampi(x, WX + 2, WX + WW - 2 - w);
    y = sy_clampi(y, WY + 2, WY + WH - 15);
    cv_rrect(x, y, w, 13, 3, sk.now, sk.display);
    cv_rrect(x + 1, y + 1, w - 2, 11, 2, sk.raised, sk.now);
    cv_text_on(x + 4, y + CAP_IN(XS, 13), &AF_XS, s, sk.now, sk.raised);
}

/* the parameter the knob turning sets, if it is one of this graph's (ids lo .. hi, or extra); -1 none */
static int32_t ui_act_in(int32_t lo, int32_t hi)
{
    int32_t id = ui_act_id();
    return id >= lo && id <= hi ? id : -1;
}

/* the first source moving parameter id (SRC_*), -1 none */
static int32_t ui_mod_first(uint32_t id)
{
    uint32_t s;
    for (s = 0; s < NSLOT; s++) {
        int32_t src = sy_patch.p[SP_M1_SRC + 3u * s];
        if (src > SRC_NONE && src < SRC_COUNT && sy_patch.p[SP_M1_DST + 3u * s] == (int32_t)id &&
            sy_patch.p[SP_M1_AMT + 3u * s])
            return src;
    }
    return -1;
}

/* the value of parameter id as its knob shows it, into t */
static char *ui_val(char *t, int32_t id)
{
    sp_text((uint32_t)id, sy_patch.p[id], t);
    return t;
}

/* a time for a ruler: "250 ms", "1 s", "1.5 s" */
static void ui_tick_text(char *t, float s)
{
    if (s < 0.9995f)
        sp_real(t, s * 1000.0f, 0u, " ms");
    else
        sp_real(t, s, s - (float)sy_floori(s + 0.0005f) < 0.001f ? 0u : 1u, " s");
}

/* a ruler's step: 1, 2 or 5 x 10^n s, at most `most` of them across span */
static float ui_nice(float span, uint32_t most)
{
    float s = 0.001f;
    uint32_t k = 0;
    while (span / s > (float)most)
        s *= k++ % 3u == 1u ? 2.5f : 2.0f;
    return s;
}

/* the chain across the top of the window, in its order: each module's short name, the one shown outlined, each lit
 * when in (bypassed: dim) */
static void ui_rack(void)
{
    uint32_t i, nc = sp_chain_n(sy_patch.p);
    int32_t cur = ui_fx_inst();
    for (i = 0; i < nc; i++) {
        uint32_t e = (uint32_t)sp_chain_at(sy_patch.p, i);
        const char *nm = SEC_SHORT[SEC_DRIVE + e];
        int on = sy_patch.p[FXI_ON[e]] != 0, here = (int32_t)e == cur;
        int32_t x = WX + 3 + (int32_t)i * 28, lx = x + (22 - text_w(&AF_XS, nm)) / 2;
        uint16_t bg = here ? sk.raised : sk.display, bd = here ? sk.now : on ? sk.wmid : sk.rule;
        cv_rrect(x, WY + 3, 26, 13, 3, bd, sk.display);
        cv_rrect(x + 1, WY + 4, 24, 11, 2, bg, bd);
        cv_text_on(lx + 2, WY + 3 + CAP_IN(XS, 13), &AF_XS, nm, here ? sk.text : on ? sk.text2 : sk.faint, bg);
    }
}
/* the knob turning's parameter, if one of this module's lo .. hi (its first's ids: the module's moved back); -1 none */
static int32_t ui_act_fx(int32_t lo, int32_t hi)
{
    int32_t id = ui_act_id(), o = (int32_t)ui_fx_off();
    return id >= lo + o && id <= hi + o ? id - o : -1;
}
static uint32_t ui_fx_nth(void) { return ui_fx_inst() >= 0 ? fx_nth((uint32_t)ui_fx_inst()) : 0u; }

/* ------------------------------------------------------------------------------------- frequency --- */
#define UI_FN 211u                                  /* a response's columns: x 8 .. 218 of the window */
static float ui_col_hz(uint32_t i) { return 20.0f * sy_exp2f((float)i * (9.96578f / 210.0f)); }
static float ui_hz_x(float hz) { return (float)(WX + 8) + sy_log2f(hz * 0.05f) * (210.0f / 9.96578f); }

/* the frequency grid from row y0 to y1 (50 200 500 2k 5k faint, 100 1k 10k stronger and named under it) */
static void ui_freq_grid(int32_t y0, int32_t y1)
{
    static const uint16_t F[8] = {50, 200, 500, 2000, 5000, 100, 1000, 10000};
    static const char *const T[3] = {"100", "1k", "10k"};
    uint32_t i;
    for (i = 0; i < 8u; i++)
        cv_rect(sy_floori(ui_hz_x((float)F[i]) + 0.5f), y0, 1, y1 - y0, i < 5u ? sk.grid : sk.grid2);
    for (i = 0; i < 3u; i++)
        ui_text_c(sy_floori(ui_hz_x((float)F[5u + i]) + 0.5f) - 12, WY + WH - 12, 24, &AF_XS, T[i], sk.faint,
                  sk.display);
}

/* a complex number, for the responses */
typedef struct {
    float r, i;
} ui_cx;
static ui_cx cx_mul(ui_cx a, ui_cx b)
{
    ui_cx c;
    c.r = a.r * b.r - a.i * b.i;
    c.i = a.r * b.i + a.i * b.r;
    return c;
}
static ui_cx cx_div(ui_cx a, ui_cx b)
{
    float d = 1.0f / (b.r * b.r + b.i * b.i);
    ui_cx c;
    c.r = (a.r * b.r + a.i * b.i) * d;
    c.i = (a.i * b.r - a.r * b.i) * d;
    return c;
}

/* ---------------------------------------------------------------------------------------- FILTER --- */
/* one SVF stage (sy_filter.c's outputs FR_*) at w, the frequency over its corner, warped (tan(pi f / fs) / g) */
static ui_cx ui_svf_h(uint32_t rs, float k, float w)
{
    ui_cx d, n;
    d.r = 1.0f - w * w;
    d.i = k * w;
    n.r = rs == FR_HP ? -w * w : rs == FR_BP ? 0.0f : rs == FR_LP ? 1.0f : d.r;
    n.i = rs == FR_BP ? k * w : rs == FR_PEAK ? 4.0f * k * w : 0.0f;
    return cx_div(n, d);
}

/* the filter's response in dB at f for values p, its cutoff at hz (one side: VAR moves it for each), MIX in: each
 * type's analog prototype at the SVF's warped frequency, as sy_filter.c plays it (DRIVE aside) */
static float ui_flt_db(const int16_t *p, float hz, float f)
{
    uint32_t t = (uint32_t)p[SP_F_TYPE], b;
    float res = ui_v(p, SP_F_RES), mix = ui_v(p, SP_F_MIX);
    float tf = sy_tanpi(sy_minf(f, 0.468f * SY_FS) * (1.0f / SY_FS)), w = tf / flt_g(hz);
    ui_cx h = {0.0f, 0.0f};
    if (t == FT_FORMANT) {                          /* three band-passes on a vowel's formants (VAR: their shift) */
        float pos = sy_clampf((sy_log2f(hz) - 4.3219f) * (4.0f / 9.9658f), 0.0f, 4.0f), x;
        float sh = sy_exp2f(2.0f * ui_v(p, SP_F_VAR) - 1.0f), k = flt_k(res, 0.5f);
        uint32_t v = (uint32_t)sy_floori(pos);
        if (v > 3u)
            v = 3u;
        x = pos - (float)v;
        for (b = 0; b < 3u; b++) {
            float fh = FLT_VOW[v][b][0] * sy_exp2f(x * sy_log2f(FLT_VOW[v + 1u][b][0] / FLT_VOW[v][b][0])) * sh;
            float gn = 2.0f * (FLT_VOW[v][b][1] + (FLT_VOW[v + 1u][b][1] - FLT_VOW[v][b][1]) * x);
            ui_cx s = ui_svf_h(FR_BP, k, tf / flt_g(fh));
            h.r += gn * s.r;
            h.i += gn * s.i;
        }
    } else if (t == FT_LADDER) {                    /* (1 + k / 2) / ((1 + s)^4 + k), the clip aside */
        float k = 4.0f * res, w2 = w * w;
        ui_cx n = {1.0f + 0.5f * k, 0.0f}, d;
        d.r = 1.0f - 6.0f * w2 + w2 * w2 + k;
        d.i = 4.0f * w * (1.0f - w2);
        h = cx_div(n, d);
    } else if (t == FT_LP24 || t == FT_HP24 || t == FT_BP24) {
        h = cx_mul(ui_svf_h(FLT_RESP[t], 1.848f, w), ui_svf_h(FLT_RESP[t], flt_k(res, 0.765f), w));
    } else if (t < FT_LADDER) {
        h = ui_svf_h(FLT_RESP[t], flt_k(res, 1.414f), w);
    }
    h.r = 1.0f + (h.r - 1.0f) * mix;
    h.i *= mix;
    return 3.0103f * sy_log2f(h.r * h.r + h.i * h.i + 1e-12f);
}

#define UI_FY(db) ((float)WY + 14.0f + (18.0f - sy_clampf((db), -42.0f, 18.0f)) * (4.0f / 3.0f))   /* +18 .. -42 dB */
static void ui_flt_curve(const int16_t *p, float hz, float *ys)
{
    uint32_t i;
    for (i = 0; i < UI_FN; i++)
        ys[i] = UI_FY(ui_flt_db(p, hz, ui_col_hz(i)));
}

/* the cutoff the values p give at a note n semitones from C4 (KEYTRACK), one side of VAR (s -1, 0, +1) */
static float ui_flt_hz(const int16_t *p, float n, float s)
{
    float hz = sp_cut_hz(ui_v(p, SP_F_CUT)) * sy_exp2f(ui_v(p, SP_F_KEY) * n * (1.0f / 12.0f));
    return p[SP_F_TYPE] == FT_FORMANT ? hz : hz * sy_exp2f(s * ui_v(p, SP_F_VAR));
}

static void ui_g_filter(void)
{
    static const char *const SLOPE[FT_COUNT] = {"12 dB/OCT", "24 dB/OCT", "12 dB/OCT", "24 dB/OCT", "6 dB/OCT",
                                                "12 dB/OCT", "", "+12 dB", "24 dB/OCT", ""};
    const int16_t *p = sy_patch.p;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_in(SP_F_ON, SP_F_MIX);
    int more = UI_PG[ui.page].id[0] == SP_F_KEY, spread = p[SP_F_TYPE] != FT_FORMANT && p[SP_F_VAR] > 0;
    int moving = act == SP_F_TYPE || act == SP_F_CUT || act == SP_F_RES || act == SP_F_MIX || act == SP_F_VAR;
    uint32_t t = (uint32_t)p[SP_F_TYPE], i, top = 0;
    float ys[UI_FN], y2[UI_FN], cut = ui_v(p, SP_F_CUT), hz = ui_flt_hz(p, 0.0f, 0.0f), olo, ohi, plo, phi;
    char w[32], *e;
    ui_freq_grid(WY + 6, WY + 94);
    cv_rect(WX + 8, sy_floori(UI_FY(-24.0f)), 211, 1, sk.grid);
    cv_rect(WX + 8, sy_floori(UI_FY(0.0f)), 211, 1, sk.grid2);
    ui_mod_span(SP_F_CUT, SRC_COUNT, &olo, &ohi, &plo, &phi);
    if (!more && ohi > olo) {                       /* the range modulation sweeps the cutoff */
        int16_t q[SP_COUNT];
        memcpy(q, p, sizeof q);
        q[SP_F_CUT] = (int16_t)sy_floori(sy_clampf(cut + olo, 0.0f, 1.0f) * 1000.0f + 0.5f);
        ui_flt_curve(q, ui_flt_hz(q, 0.0f, 0.0f), ys);
        q[SP_F_CUT] = (int16_t)sy_floori(sy_clampf(cut + ohi, 0.0f, 1.0f) * 1000.0f + 0.5f);
        ui_flt_curve(q, ui_flt_hz(q, 0.0f, 0.0f), y2);
        cv_band(WX + 8, ys, y2, UI_FN, sk.mband);
    }
    if (more) {                                     /* KEYTRACK: the cutoff at C2, C4, C6 */
        static const char *const NOTE[3] = {"C2", "C4", "C6"};
        int keys = act == SP_F_KEY;
        ui_flt_curve(p, hz, ys);
        cv_under(WX + 8, ys, UI_FN, (float)(WY + 94), sk.wfill);
        if (gh && keys) {
            ui_flt_curve(gh->p, ui_flt_hz(gh->p, -24.0f, 0.0f), y2);
            cv_curve_dash(WX + 8, y2, UI_FN, 1.0f, sk.ghost, 3u, 2u);
            ui_flt_curve(gh->p, ui_flt_hz(gh->p, 24.0f, 0.0f), y2);
            cv_curve_dash(WX + 8, y2, UI_FN, 1.0f, sk.ghost, 3u, 2u);
        }
        if (p[SP_F_KEY] > 0)
            for (i = 0; i < 2u; i++) {
                ui_flt_curve(p, ui_flt_hz(p, i ? 24.0f : -24.0f, 0.0f), y2);
                cv_curve(WX + 8, y2, UI_FN, 1.2f, keys ? sk.now : sk.wmid);
            }
        cv_curve(WX + 8, ys, UI_FN, 1.6f, moving ? sk.now : sk.sound);
        for (i = 0; i < 3u; i++) {                  /* (C2 and C6 named when apart from C4) */
            float f = ui_flt_hz(p, (float)i * 24.0f - 24.0f, 0.0f);
            if (i != 1u && sy_absf(ui_hz_x(f) - ui_hz_x(hz)) < 18.0f)
                continue;
            ui_text_c(sy_floori(ui_hz_x(f) + 0.5f) - 10, WY + 18, 20, &AF_XS, NOTE[i],
                      i == 1u ? sk.sound : keys ? sk.now : sk.dim, sk.display);
        }
        ui_label(0, 1, "CUTOFF FOLLOWS THE KEY", sk.dim);
    } else {
        if (gh && moving) {                         /* the shape before the turn */
            uint32_t s;
            for (s = 0; s < (gh->p[SP_F_TYPE] != FT_FORMANT && gh->p[SP_F_VAR] > 0 ? 2u : 1u); s++) {
                ui_flt_curve(gh->p, ui_flt_hz(gh->p, 0.0f, gh->p[SP_F_VAR] > 0 ? (s ? 1.0f : -1.0f) : 0.0f), y2);
                cv_curve_dash(WX + 8, y2, UI_FN, 1.0f, sk.ghost, 3u, 2u);
            }
        }
        if (spread) {                               /* VAR: the left's cutoff down, the right's up */
            ui_flt_curve(p, ui_flt_hz(p, 0.0f, -1.0f), ys);
            ui_flt_curve(p, ui_flt_hz(p, 0.0f, 1.0f), y2);
            for (i = 0; i < UI_FN; i++)
                y2[i] = sy_minf(ys[i], y2[i]);
            cv_under(WX + 8, y2, UI_FN, (float)(WY + 94), sk.wfill);
            ui_flt_curve(p, ui_flt_hz(p, 0.0f, 1.0f), y2);
            cv_curve(WX + 8, y2, UI_FN, 1.2f, moving ? sk.now : sk.sound);
            cv_curve(WX + 8, ys, UI_FN, 1.6f, moving ? sk.now : sk.sound);
        } else {
            ui_flt_curve(p, hz, ys);
            cv_under(WX + 8, ys, UI_FN, (float)(WY + 94), sk.wfill);
            if (act == SP_F_CUT)                    /* the cutoff, marked */
                cv_vdash(sy_floori(ui_hz_x(hz) + 0.5f), WY + 30, WY + 94, 2, 2, sk.now);
            cv_curve(WX + 8, ys, UI_FN, 1.6f, moving ? sk.now : sk.sound);
        }
        for (i = 1; i < UI_FN; i++)                 /* the peak */
            if (ys[i] < ys[top])
                top = i;
        if (moving) {
            cv_disc((float)(WX + 8 + (int32_t)top) + 0.5f, ys[top], 3.6f, sk.display);
            cv_disc((float)(WX + 8 + (int32_t)top) + 0.5f, ys[top], 2.6f, sk.now);
        }
        e = sp_put(w, FT_NAMES[t < FT_COUNT ? t : 0]);
        if (t == FT_FORMANT) {                      /* the vowel at the cutoff */
            float pos = sy_clampf((sy_log2f(hz) - 4.3219f) * (4.0f / 9.9658f), 0.0f, 4.0f);
            char v[2] = {"AEIOU"[sy_floori(pos + 0.5f)], 0};
            sp_put(sp_put(e, " \xB7 VOWEL "), v);
        } else if (SLOPE[t][0]) {
            sp_put(sp_put(e, " \xB7 "), SLOPE[t]);
        }
        ui_label(0, 1, w, sk.dim);
        if (ohi > olo && ui_mod_first(SP_F_CUT) >= 0) {   /* the source, and how far it takes the cutoff */
            e = sp_put(sp_put(w, SRC_NAMES[ui_mod_first(SP_F_CUT)]), ": ");
            sp_freq(e, sp_cut_hz(sy_clampf(cut + (ohi > 0.0f ? ohi : olo), 0.0f, 1.0f)), 1u);
            ui_label(-1, 1, w, sk.mod);
        }
    }
    if (act >= 0) {                                 /* its value, beside what it moves */
        int32_t x = WX + 6, y = WY + 17;
        if (act == SP_F_CUT) {
            x = sy_floori(ui_hz_x(hz)) - 24;
        } else if (act == SP_F_RES && !more) {
            x = WX + 8 + (int32_t)top - 20;
            y = sy_floori(ys[top]) > WY + 36 ? sy_floori(ys[top]) - 18 : sy_floori(ys[top]) + 6;
        } else if (act == SP_F_KEY) {
            x = WX + 166;
            y = WY + 4;
        }
        ui_flag(x, y, ui_val(w, act));
    }
}

/* ----------------------------------------------------------------------------------------- DRIVE --- */
/* the drive's curve: out for a level x (-1..1) in, as fx_drive plays it (TONE aside) */
static float ui_drv_out(uint32_t type, float amt, float mix, float x)
{
    float g = 1.0f + 31.0f * amt * amt, v = x * g, y;
    switch (type) {
    case DRV_TUBE: y = sy_softclip(v + 0.3f) - 0.296f; break;
    case DRV_HARD: y = sy_clampf(v, -1.0f, 1.0f); break;
    case DRV_FOLD: y = sy_sin2pi(0.25f * v); break;
    default: y = sy_softclip(v); break;
    }
    return x + (y * sy_exp2f(-0.3f * sy_log2f(g)) - x) * mix;
}
static void ui_drv_curve(const int16_t *p, float amt, float *ys)   /* the box's 81 columns, x -1 .. 1 */
{
    uint32_t i;
    for (i = 0; i < 81u; i++)
        ys[i] = (float)(WY + 62) - sy_clampf(ui_drv_out((uint32_t)p[SP_DR_TYPE], amt, ui_v(p, SP_DR_MIX),
                                                         (float)((int32_t)i - 40) * 0.025f), -1.0f, 1.0f) * 40.0f;
}

static void ui_g_drive(void)
{
    uint32_t o = ui_fx_off();
    const int16_t *p = sy_patch.p + o;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_fx(SP_DR_ON, SP_DR_MIX);
    int moving = act == SP_DR_TYPE || act == SP_DR_AMT || act == SP_DR_MIX;
    float ys[117], y2[117], amt = ui_v(p, SP_DR_AMT), olo, ohi, plo, phi;
    uint32_t i, type = (uint32_t)p[SP_DR_TYPE];
    char w[24];
    ui_rack();
    cv_frame(WX + 10, WY + 22, 81, 81, sk.grid2);   /* the curve's box: in across, out up */
    cv_rect(WX + 11, WY + 62, 79, 1, sk.grid);
    cv_rect(WX + 50, WY + 23, 1, 79, sk.grid);
    cv_rect(WX + 100, WY + 62, 120, 1, sk.grid);
    for (i = 0; i < 81u; i++)
        ys[i] = (float)(WY + 102 - (int32_t)i);
    cv_curve_dash(WX + 10, ys, 81u, 1.0f, sk.ghost, 2u, 3u);   /* (straight through) */
    ui_mod_span(o + SP_DR_AMT, SRC_COUNT, &olo, &ohi, &plo, &phi);
    if (ohi > olo) {
        ui_drv_curve(p, sy_clampf(amt + olo, 0.0f, 1.0f), ys);
        ui_drv_curve(p, sy_clampf(amt + ohi, 0.0f, 1.0f), y2);
        cv_band(WX + 10, ys, y2, 81u, sk.mband);
    }
    if (gh && moving) {
        ui_drv_curve((gh->p + o), ui_v((gh->p + o), SP_DR_AMT), y2);
        cv_curve_dash(WX + 10, y2, 81u, 1.0f, sk.wmid, 3u, 2u);
    }
    ui_drv_curve(p, amt, ys);
    cv_curve(WX + 10, ys, 81u, 1.6f, moving ? sk.now : sk.sound);
    for (i = 0; i < 117u; i++) {                    /* a sine through it, a cycle and a half */
        float v = 0.8f * sy_sin2pi(1.5f * (float)i / 116.0f);
        ys[i] = (float)(WY + 62) - v * 34.0f;
        y2[i] = (float)(WY + 62) - sy_clampf(ui_drv_out(type, amt, ui_v(p, SP_DR_MIX), v), -1.0f, 1.0f) * 34.0f;
    }
    cv_curve(WX + 102, ys, 117u, 1.0f, sk.ghost);
    cv_curve(WX + 102, y2, 117u, 1.6f, act == SP_DR_MIX ? sk.now : sk.sound);
    cv_text_on(WX + 104, WY + 18, &AF_XS, "IN", sk.ghost, sk.display);
    cv_text_on(WX + 104 + text_w(&AF_XS, "IN") + 6, WY + 18, &AF_XS, "OUT", sk.sound, sk.display);
    sp_put(w, "TONE ");
    ui_val(w + 5, (int32_t)(SP_DR_TONE + o));
    ui_label(-1, 0, w, act == SP_DR_TONE ? sk.now : sk.faint);
    if (act >= 0)
        ui_flag(act == SP_DR_MIX ? WX + 160 : act == SP_DR_TONE ? WX + 150 : WX + 13,
                act == SP_DR_MIX ? WY + 18 : act == SP_DR_TONE ? WY + 80 : WY + 25, ui_val(w, act + (int32_t)o));
}

/* ---------------------------------------------------------------------------------------- CHORUS --- */
#define UI_MS_X(ms) ((float)(WX + 30) + (ms) * (186.0f / 20.0f))   /* 0 .. 20 ms of the line */
static void ui_cho_box(float c, float h, int32_t y, uint16_t bd, int dashed)   /* a tap's swing: c +- h ms */
{
    int32_t x0 = sy_floori(UI_MS_X(c - h) + 0.5f), x1 = sy_floori(UI_MS_X(c + h) + 0.5f);
    if (x1 - x0 < 4)
        x0 -= 2, x1 += 2;
    if (dashed) {
        cv_dash(x0, x1, y - 3, 2, 2, bd);
        cv_dash(x0, x1, y + 3, 2, 2, bd);
        cv_vdash(x0, y - 3, y + 4, 2, 2, bd);
        cv_vdash(x1 - 1, y - 3, y + 4, 2, 2, bd);
        return;
    }
    cv_rrect(x0, y - 3, x1 - x0, 7, 3, bd, sk.display);
    cv_rrect(x0 + 1, y - 2, x1 - x0 - 2, 5, 2, sk.wfill, bd);
}

static void ui_g_chorus(void)
{
    static const char *const ROW[4] = {"L1", "L2", "R1", "R2"};
    uint32_t o = ui_fx_off();
    const int16_t *p = sy_patch.p + o;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_fx(SP_CH_ON, SP_CH_MIX);
    int sweep = act == SP_CH_DEPTH || act == SP_CH_DELAY;
    float base = 2.0f + 13.0f * ui_v(p, SP_CH_DELAY), depth = 3.0f * ui_v(p, SP_CH_DEPTH), bm = base + 0.5f * depth;
    float ph = (float)(fx.cho[ui_fx_nth()].ph >> 8) * (1.0f / 16777216.0f), sn = sy_sin2pi(ph), cs = sy_sin2pi(ph + 0.25f);
    float pos[4], olo, ohi, plo, phi, dlo, dhi;
    uint32_t i;
    char w[24];
    pos[0] = sn;
    pos[1] = cs;
    pos[2] = -sn;
    pos[3] = -cs;
    ui_rack();
    for (i = 1; i < 4u; i++)                        /* 5, 10, 15 ms */
        cv_rect(sy_floori(UI_MS_X(5.0f * (float)i) + 0.5f), WY + 20, 1, 72, sk.grid);
    ui_mod_span(o + SP_CH_DEPTH, SRC_COUNT, &olo, &ohi, &plo, &phi);
    ui_mod_span(o + SP_CH_DELAY, SRC_COUNT, &dlo, &dhi, &plo, &phi);
    for (i = 0; i < 4u; i++) {
        int32_t y = WY + 30 + (int32_t)i * 16;
        cv_rect(WX + 30, y, 186, 1, sk.grid2);
        cv_text_on(WX + 7, y - 6, &AF_XS, ROW[i], sk.dim, sk.display);
        if (ohi > olo || dhi > dlo) {               /* the farthest modulation takes the taps */
            float lo = 2.0f + 13.0f * sy_clampf(ui_v(p, SP_CH_DELAY) + dlo, 0.0f, 1.0f);
            float hi = 2.0f + 13.0f * sy_clampf(ui_v(p, SP_CH_DELAY) + dhi, 0.0f, 1.0f) +
                       3.0f * sy_clampf(ui_v(p, SP_CH_DEPTH) + ohi, 0.0f, 1.0f);
            int32_t x0 = sy_floori(UI_MS_X(lo) + 0.5f);
            cv_rect(x0, y + 5, sy_floori(UI_MS_X(hi) + 0.5f) - x0, 1, sk.mod);
        }
        if (gh && sweep) {
            float gb = 2.0f + 13.0f * ui_v((gh->p + o), SP_CH_DELAY), gd = 3.0f * ui_v((gh->p + o), SP_CH_DEPTH);
            ui_cho_box(gb + 0.5f * gd, 0.5f * gd, y, sk.ghost, 1);
        }
        ui_cho_box(bm, 0.5f * depth, y, sweep ? sk.now : sk.wmid, 0);
        cv_vdash(sy_floori(UI_MS_X(bm) + 0.5f), y - 6, y + 7, 1, 2, sk.label);
        cv_disc(UI_MS_X(bm + 0.5f * depth * pos[i]), (float)y + 0.5f, 2.5f, sk.sound);
    }
    for (i = 0; i < 5u; i++) {                      /* the ruler */
        int32_t x = sy_floori(UI_MS_X(5.0f * (float)i) + 0.5f);
        sp_fix(w, (int32_t)(5u * i), 0u, 0);
        if (i == 4u)
            cv_text_r(x + 4, WY + 94, &AF_XS, "20 ms", sk.faint, sk.display);
        else
            ui_text_c(x - 8, WY + 94, 16, &AF_XS, w, sk.faint, sk.display);
    }
    if (act >= 0) {
        if (act == SP_CH_DEPTH) {                   /* the swing, in ms */
            w[0] = (char)0xB1;
            sp_put(sp_real(w + 1, 0.5f * depth, 2u, ""), " ms");
        } else if (act == SP_CH_DELAY) {
            sp_real(w, bm, 1u, " ms");
        } else {
            ui_val(w, act + (int32_t)o);
        }
        ui_flag(act == SP_CH_DELAY ? sy_floori(UI_MS_X(bm)) - 18 : WX + 150, WY + 17, w);
    }
}

/* ----------------------------------------------------------------------------------------- DELAY --- */
static float ui_dly_secs(const int16_t *p)         /* the echo's distance as fx_delay sets it (750 ms at most) */
{
    float s = p[SP_DL_SYNC] ? sy_note_q((uint32_t)p[SP_DL_NOTE]) * 60.0f / sy_tempo : sp_dly_s(ui_v(p, SP_DL_TIME));
    return sy_clampf(s, 2.0f / (0.5f * SY_FS), (float)(FX_DLY_N - 4u) / (0.5f * SY_FS));
}

/* the loop's filters at f (one pass): the low pass and the high pass at the half rate, as fx_delay runs them (dB) */
static float ui_dly_db(const int16_t *p, float f)
{
    float kl = sy_minf(1.0f, 6.2831853f * sp_cut_hz(ui_v(p, SP_DL_LP)) * 2.0f / SY_FS);
    float kh = sy_minf(1.0f, 6.2831853f * sp_cut_hz(ui_v(p, SP_DL_HP)) * 2.0f / SY_FS);
    float c = sy_cos2pi(f * 2.0f / SY_FS), al = 1.0f - kl, ah = 1.0f - kh;
    float lp = kl * kl / (1.0f - 2.0f * al * c + al * al), hp = ah * ah * 2.0f * (1.0f - c) / (1.0f - 2.0f * ah * c + ah * ah);
    return 3.0103f * sy_log2f(lp * hp + 1e-12f);
}

static void ui_g_delay(void)
{
    const int16_t *p = sy_patch.p;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_in(SP_DL_ON, SP_DL_MIX), na = ui_act_in(SP_DL_NOTE, SP_DL_NOTE);
    float fb = 0.95f * ui_v(p, SP_DL_FDBK), secs = ui_dly_secs(p), olo, ohi, plo, phi;
    uint32_t i, n;
    char w[32], *e;
    if (na >= 0)
        act = na;
    ui_rack();
    if (UI_PG[ui.page].id[0] == SP_DL_LP) {         /* TONE: what each pass through the loop leaves */
        float ys[UI_FN], y2[UI_FN], h[UI_FN], fdb = 6.0206f * sy_log2f(sy_maxf(fb, 1e-3f));
        int moving = act == SP_DL_LP || act == SP_DL_HP;
        uint32_t k, nf = 0;
#define UI_DY(db) ((float)WY + 31.0f + sy_minf(-(db), 48.0f) * (64.0f / 48.0f))
        while (nf < UI_FN && ui_col_hz(nf) <= 0.24f * SY_FS)
            nf++;                                   /* (the line at half rate: nothing above ~10.6 kHz) */
        ui_freq_grid(WY + 20, WY + 95);
        for (k = 0; k < 4u; k++)
            cv_rect(WX + 8, sy_floori(UI_DY(-12.0f * (float)k)), 211, 1, k ? sk.grid : sk.grid2);
        for (i = 0; i < nf; i++) {                  /* the first echo: the filters once */
            h[i] = ui_dly_db(p, ui_col_hz(i));
            y2[i] = UI_DY(h[i]);
        }
        cv_under(WX + 8, y2, nf, UI_DY(-48.0f), sk.wfill);
        if (gh && moving) {
            for (i = 0; i < nf; i++)
                ys[i] = UI_DY(ui_dly_db(gh->p, ui_col_hz(i)));
            cv_curve_dash(WX + 8, ys, nf, 1.0f, sk.ghost, 3u, 2u);
        }
        for (k = 3u; k >= 1u; k--) {                /* echoes 4 .. 2: the filters k + 1 times, the feedback k */
            for (i = 0; i < nf; i++)
                ys[i] = UI_DY((float)(k + 1u) * h[i] + (float)k * fdb);
            cv_curve(WX + 8, ys, nf, 1.0f, sy_mix565(sk.sound, sk.display, 60u * k));
        }
        cv_curve(WX + 8, y2, nf, 1.6f, moving ? sk.now : sk.sound);
        cv_text_on(WX + 6, WY + 18, &AF_XS, "EACH ECHO, DARKER", sk.dim, sk.display);
        if (act >= 0) {
            float f = act == SP_DL_LP ? sp_cut_hz(ui_v(p, SP_DL_LP)) : sp_cut_hz(ui_v(p, SP_DL_HP));
            ui_flag(act == SP_DL_LP || act == SP_DL_HP ? sy_floori(ui_hz_x(f)) - 20 : WX + 150, WY + 32, ui_val(w, act));
        }
#undef UI_DY
        return;
    }
    {                                               /* DELAY: the echoes and their fall */
        float T = 6.4f * secs, step = ui_nice(T, 5u), tk, env[2][203];
        int pp = p[SP_DL_MODE] != 0, fall = act == SP_DL_FDBK;
#define UI_SX(s) ((float)(WX + 16) + (s) / T * 202.0f)
        for (tk = step; tk < T; tk += step)
            cv_rect(sy_floori(UI_SX(tk) + 0.5f), WY + 26, 1, 76, sk.grid);
        cv_rect(WX + 16, WY + 62, 202, 1, sk.grid2);
        cv_rect(WX + 15, WY + 26, 2, 72, sk.text2);  /* the sound in */
        ui_mod_span(SP_DL_FDBK, SRC_COUNT, &olo, &ohi, &plo, &phi);
        for (n = 0; n < 3u; n++) {                  /* the fall: now, before the turn (dashed), modulated */
            float f = n == 0u ? fb : n == 1u ? (gh && fall ? 0.95f * ui_v(gh->p, SP_DL_FDBK) : -1.0f)
                                           : (ohi > 0.0f ? 0.95f * sy_clampf(ui_v(p, SP_DL_FDBK) + ohi, 0.0f, 1.0f) : -1.0f);
            if (f < 0.0f)
                continue;
            for (i = 0; i < 203u; i++) {
                float a = 36.0f * sy_exp2f(sy_log2f(sy_maxf(f, 1e-4f)) * ((float)i / 202.0f * T) / secs);
                env[0][i] = (float)(WY + 62) - a;
                env[1][i] = (float)(WY + 62) + a;
            }
            for (i = 0; i < 2u; i++) {
                if (n == 1u)
                    cv_curve_dash(WX + 16, env[i], 203u, 1.0f, sk.ghost, 3u, 2u);
                else
                    cv_curve(WX + 16, env[i], 203u, 1.0f, n == 2u ? sk.mod : fall ? sk.now : sk.wmid);
            }
        }
        for (n = 1; (float)n * secs < T; n++) {     /* the echoes (ping-pong: left, right, left ..) */
            float h = sy_maxf(1.0f, 36.0f * sy_exp2f(sy_log2f(sy_maxf(fb, 1e-4f)) * (float)n));
            int32_t x = sy_floori(UI_SX((float)n * secs) + 0.5f);
            uint16_t c = sy_mix565(sk.sound, sk.display, (uint32_t)(sy_minf(0.6f, 0.1f * (float)n) * 256.0f));
            if (!pp || (n & 1u))
                cv_rect(x - 2, sy_floori((float)(WY + 62) - h + 0.5f), 4, sy_floori(h + 0.5f), c);
            if (!pp || !(n & 1u))
                cv_rect(x - 2, WY + 63, 4, sy_floori(h + 0.5f), c);
            cv_rect(x, WY + 60, 1, 5, sk.label);
        }
        cv_text_on(WX + 4, WY + 28, &AF_XS, "L", sk.dim, sk.display);
        cv_text_on(WX + 4, WY + 82, &AF_XS, "R", sk.dim, sk.display);
        e = w;
        if (p[SP_DL_SYNC])
            e = sp_put(sp_put(w, NOTE_NAMES[p[SP_DL_NOTE] < (int16_t)SY_NNOTE ? p[SP_DL_NOTE] : 0]), " \xB7 ");
        sp_real(e, secs * 1000.0f, 0u, " ms");
        cv_text_on(WX + 22, WY + 18, &AF_XS, w, sk.dim, sk.display);
        ui_label(-1, 0, DLY_NAMES[pp], sk.faint);
        if (act >= 0) {
            if (fall) {                             /* the echoes above -40 dB */
                for (n = 0; n < 99u && sy_exp2f(sy_log2f(sy_maxf(fb, 1e-4f)) * (float)(n + 1u)) >= 0.01f; n++)
                    ;
                for (e = ui_val(w, act); *e; e++)   /* (its end: the firmware has no strlen) */
                    ;
                e = sp_put(e, " \xB7 ");
                sp_put(sp_fix(e, (int32_t)n, 0u, 0), n == 1u ? " ECHO" : " ECHOES");
            } else {
                ui_val(w, act);
            }
            ui_flag(act == SP_DL_TIME || act == SP_DL_NOTE ? sy_floori(UI_SX(secs)) - 12 : WX + 118, WY + 30, w);
        }
#undef UI_SX
    }
}

/* ---------------------------------------------------------------------------------------- REVERB --- */
static float ui_rv_rt(const int16_t *p) { return 0.3f * sy_exp2f(6.0589f * ui_v(p, SP_RV_DECAY)); }   /* RT60, s */
/* the highs' RT60 (8 kHz): the damping's loss each pass through a line (half rate, ~1250 x SIZE samples) */
static float ui_rv_rth(const int16_t *p, float rt)
{
    float k = 1.0f - 0.85f * ui_v(p, SP_RV_DAMP), a = 1.0f - k, c = sy_cos2pi(8000.0f * 2.0f / SY_FS);
    float g2 = k * k / (1.0f - 2.0f * a * c + a * a), size = 0.45f + 0.55f * ui_v(p, SP_RV_SIZE);
    float loss = -3.0103f * sy_log2f(g2) * (0.5f * SY_FS) / (1262.0f * size);   /* dB a second */
    return 60.0f / (60.0f / rt + loss);
}

static void ui_g_reverb(void)
{
    static const uint16_t EARLY[6] = {1499, 1153, 1741, 853, 1321, 1031};   /* sy_fx.c's lines */
    static const int8_t EDB[6] = {-7, -9, -8, -12, -11, -14};
    const int16_t *p = sy_patch.p;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_in(SP_RV_ON, SP_RV_MIX);
    int tail = act == SP_RV_DECAY || act == SP_RV_SIZE || act == SP_RV_PRE;
    float rt = ui_rv_rt(p), rth = ui_rv_rth(p, rt), pre = 0.05f * ui_v(p, SP_RV_PRE), size = 0.45f + 0.55f * ui_v(p, SP_RV_SIZE);
    float T = sy_maxf(1.0f, (pre + rt) * 1.25f), step = ui_nice(T, 4u), tk, x0, x1;
    uint32_t i;
    char w[32];
#define UI_RX(s) ((float)(WX + 22) + (s) / T * 196.0f)
#define UI_RY(db) ((float)WY + 28.0f + (-(db)) * (70.0f / 60.0f))
    ui_rack();
    for (i = 1; i < 3u; i++)
        cv_rect(WX + 22, sy_floori(UI_RY(-20.0f * (float)i)), 196, 1, sk.grid);
    for (tk = step; tk < T; tk += step) {
        int32_t x = sy_floori(UI_RX(tk) + 0.5f);
        cv_rect(x, WY + 28, 1, 70, sk.grid);
        ui_tick_text(w, tk);
        ui_text_c(x - 16, WY + 97, 32, &AF_XS, w, sk.faint, sk.display);
    }
    cv_rect(WX + 22, WY + 98, 196, 1, sk.grid2);
    {                                               /* the tail: -10 dB after the early part, to -60 at RT60 */
        float ys[197], y2[197];
        for (i = 0; i < 197u; i++) {
            float s = (float)i / 196.0f * T - pre - 0.08f * size;
            ys[i] = s < 0.0f ? UI_RY(-60.0f) : UI_RY(sy_maxf(-60.0f, -10.0f - 50.0f * s / rt));
            y2[i] = s < 0.0f ? UI_RY(-60.0f) : UI_RY(sy_maxf(-60.0f, -10.0f - 50.0f * s / rth));
        }
        cv_under(WX + 22, ys, 197u, UI_RY(-60.0f), sk.wfill);
        if (gh && tail) {
            float grt = ui_rv_rt(gh->p), gpre = 0.05f * ui_v(gh->p, SP_RV_PRE);
            float gs = 0.45f + 0.55f * ui_v(gh->p, SP_RV_SIZE);
            for (i = 0; i < 197u; i++) {
                float s = (float)i / 196.0f * T - gpre - 0.08f * gs;
                y2[i] = s < 0.0f ? UI_RY(-60.0f) : UI_RY(sy_maxf(-60.0f, -10.0f - 50.0f * s / grt));
            }
            cv_curve_dash(WX + 22, y2, 197u, 1.0f, sk.ghost, 3u, 2u);
            for (i = 0; i < 197u; i++) {
                float s = (float)i / 196.0f * T - pre - 0.08f * size;
                y2[i] = s < 0.0f ? UI_RY(-60.0f) : UI_RY(sy_maxf(-60.0f, -10.0f - 50.0f * s / rth));
            }
        }
        cv_curve(WX + 22, y2, 197u, 1.2f, act == SP_RV_DAMP ? sk.now : sk.wmid);
        cv_curve(WX + 22, ys, 197u, 1.6f, tail ? sk.now : sk.sound);
        x1 = UI_RX(pre + 0.08f * size + rth * 0.84f);
        cv_text_on(sy_clampi(sy_floori(x1) - 14, WX + 30, WX + WW - 40), sy_floori(UI_RY(-52.0f)) - 12, &AF_XS,
                   "HIGHS", act == SP_RV_DAMP ? sk.now : sk.wmid, sk.display);
    }
    for (i = 0; i < 6u; i++) {                      /* the early reflections: the lines' first returns */
        float s = pre + (float)EARLY[i] * size / (0.5f * SY_FS);
        x0 = UI_RX(s);
        cv_rect(sy_floori(x0 + 0.5f), sy_floori(UI_RY((float)EDB[i]) + 0.5f), 1,
                WY + 98 - sy_floori(UI_RY((float)EDB[i]) + 0.5f), act == SP_RV_SIZE || act == SP_RV_PRE ? sk.now : sk.sound);
    }
    cv_rect(WX + 22, WY + 28, 2, 70, sk.text2);      /* the sound in */
    cv_text_on(WX + 11, WY + 24, &AF_XS, "0", sk.faint, sk.display);
    cv_text_on(WX + 2, WY + 88, &AF_XS, "-60", sk.faint, sk.display);
    sp_put(sp_put(w, RV_NAMES[p[SP_RV_TYPE] ? 1 : 0]), " \xB7 EARLY, THEN THE TAIL");
    cv_text_on(WX + 26, WY + 17, &AF_XS, w, sk.dim, sk.display);
    if (act >= 0) {
        int32_t fx = WX + 150, fy = WY + 40;
        if (act == SP_RV_DECAY) {
            sp_real(sp_put(w, "DECAY "), rt, rt < 9.95f ? 1u : 0u, " s");
            fx = sy_floori(UI_RX(pre + rt)) - 50;
            fy = WY + 58;
        } else if (act == SP_RV_DAMP) {
            sp_real(sp_put(w, "HIGHS "), rth, rth < 9.95f ? 2u : 1u, " s");
            fx = sy_floori(x1) - 10;
            fy = WY + 70;
        } else if (act == SP_RV_PRE) {
            sp_real(sp_put(w, "PRE "), pre * 1000.0f, 1u, " ms");
            fx = sy_floori(UI_RX(pre)) + 4;
        } else {
            ui_val(w, act);
        }
        ui_flag(fx, fy, w);
    }
#undef UI_RX
#undef UI_RY
}

/* ------------------------------------------------------------------------------------------- OTT --- */
/* the three bands: each one's level in (its follower, live), the thresholds (down above -12 dB, up below -40), its
 * level out (the gain the audio side applies, DEPTH in) and which way it is pushed */
static void ui_g_ott(void)
{
    static const char *const NAME[3] = {"LOW", "MID", "HIGH"};
    uint32_t o = ui_fx_off();
    const int16_t *p = sy_patch.p + o;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_fx(SP_OT_ON, SP_OT_HIGH);
    float depth = ui_v(p, SP_OT_DEPTH);
    uint32_t b;
    char w[24];
#define UI_LV(db) ((float)WY + 32.0f + sy_clampf(-(db), 0.0f, 60.0f) * (58.0f / 60.0f))
    ui_rack();
    for (b = 0; b < 3u; b++) {
        int32_t x = WX + 8 + (int32_t)b * 72, ya, yb, cx = x + 33;
        float in = 6.0206f * sy_log2f(sy_maxf(fx.ott[ui_fx_nth()].env[b], 1e-6f));
        float g = sy_maxf(1.0f + (fx.ott[ui_fx_nth()].g[b] - 1.0f) * depth, 1e-6f), out = in + 6.0206f * sy_log2f(g);
        int band = act == SP_OT_LOW + (int32_t)b, push = act >= SP_OT_DEPTH && act <= SP_OT_DOWN;
        cv_rect(x + 18, WY + 32, 30, 58, sk.grid);
        ya = sy_floori(UI_LV(in) + 0.5f);
        cv_rect(x + 18, ya, 30, WY + 90 - ya, sk.wmid);
        cv_rect(x + 14, sy_floori(UI_LV(-12.0f) + 0.5f), 38, 1, sk.label);   /* down above, up below */
        cv_dash(x + 14, x + 52, sy_floori(UI_LV(-40.0f) + 0.5f), 2, 2, sk.label);
        if (gh && push) {
            float g0 = sy_maxf(1.0f + (fx.ott[ui_fx_nth()].g[b] - 1.0f) * ui_v((gh->p + o), SP_OT_DEPTH), 1e-6f);
            cv_dash(x + 14, x + 52, sy_floori(UI_LV(in + 6.0206f * sy_log2f(g0)) + 0.5f), 2, 2, sk.ghost);
        }
        yb = sy_floori(UI_LV(out) + 0.5f);
        cv_rect(x + 14, yb - 1, 38, 2, band ? sk.now : sk.sound);
        if (yb - ya > 5 || ya - yb > 5) {           /* the push: an arrow from in to out */
            int32_t h = yb > ya ? yb - 4 : yb + 4;
            uint16_t c = push || band ? sk.now : sk.text2;
            cv_seg((float)cx + 0.5f, (float)ya, (float)cx + 0.5f, (float)yb, 1.6f, c);
            cv_seg((float)cx - 2.5f, (float)h, (float)cx + 0.5f, (float)yb, 1.6f, c);
            cv_seg((float)cx + 3.5f, (float)h, (float)cx + 0.5f, (float)yb, 1.6f, c);
        }
        cv_text_on(x + 2, WY + 94, &AF_XS, NAME[b], band ? sk.now : sk.dim, sk.display);
        ui_val(w, (int32_t)(SP_OT_LOW + b + o));
        cv_text_r(x + 64, WY + 94, &AF_XS, w, band ? sk.now : sk.faint, sk.display);
    }
    cv_text_on(WX + 61, sy_floori(UI_LV(-12.0f)) - 6, &AF_XS, "-12", sk.label, sk.display);
    cv_text_on(WX + 61, sy_floori(UI_LV(-40.0f)) - 6, &AF_XS, "-40", sk.label, sk.display);
    if (act >= 0) {
        int32_t k = act >= SP_OT_LOW ? act - SP_OT_LOW : -1;
        ui_val(sp_put(sp_put(w, SP[act].name), " "), act + (int32_t)o);
        ui_flag(k >= 0 ? WX + 8 + k * 72 + 4 : WX + 60, k >= 0 ? WY + 17 : WY + 17, w);
    }
#undef UI_LV
}

/* -------------------------------------------------------------------------------------------- EQ --- */
/* the EQ's nine values: each point's gain, frequency and Q (LOW, MID, HIGH) */
static const uint8_t UI_EQ_ID[9] = {SP_EQ_LOW, SP_EQ_MID, SP_EQ_HIGH, SP_EQ_LFREQ, SP_EQ_MFREQ, SP_EQ_HFREQ,
                                    SP_EQ_LQ, SP_EQ_Q, SP_EQ_HQ};
#define UI_QY(db) ((float)WY + 61.0f - sy_clampf((db), -20.0f, 20.0f) * 2.0f)   /* +-18 dB: 2 px a dB */

/* the nine as 0..1, into v: of values p each moved by d (a modulation's range end; 0: none); live: as the audio side
 * has them now (smoothed, modulated) */
static void ui_eq_vals(const int16_t *p, const float *d, int live, float *v)
{
    uint32_t k;
    for (k = 0; k < 9u; k++)
        v[k] = live ? mod_get(sy_goff, UI_EQ_ID[k] + ui_fx_off())
                    : sy_clampf(ui_v(p, UI_EQ_ID[k]) + (d ? d[k] : 0.0f), 0.0f, 1.0f);
}
static float ui_eq_gain(const float *v, uint32_t b) { return sp_lin(UI_EQ_ID[b], v[b]) * 0.1f; }   /* a point's dB */

/* the EQ's gain at f in dB for the nine values v: each point's band, as fx_eq plays it (fx_eq_db) */
static float ui_eq_db(const float *v, float f)
{
    float db = 0.0f;
    uint32_t b;
    for (b = 0; b < 3u; b++) {
        float g = ui_eq_gain(v, b);
        if (sy_absf(g) >= 0.01f)
            db += fx_eq_db(b, sp_cut_hz(v[3u + b]), g, sp_q(v[6u + b]), f);
    }
    return db;
}
static void ui_eq_curve(const float *v, float *ys)
{
    uint32_t i;
    for (i = 0; i < UI_FN; i++)
        ys[i] = UI_QY(ui_eq_db(v, ui_col_hz(i)));
}

/* any of the EQ's values modulated (its graph live while sound plays) */
static int ui_eq_modded(void)
{
    float lo, hi, plo, phi;
    uint32_t k;
    for (k = 0; k < 9u; k++)
        if (ui_mod_span(UI_EQ_ID[k] + ui_fx_off(), SRC_COUNT, &lo, &hi, &plo, &phi))
            return 1;
    return 0;
}

/* the EQ: its curve and its three points (each at its frequency and gain; on POINTS the one picked filled, the one a
 * knob turns in NOW); the range a modulation sweeps, the curve's as a band and each point's as bars (up and down: its
 * gain, across: its frequency); while sound plays, the curve and the points where the modulation has them now */
static void ui_g_eq(void)
{
    uint32_t o = ui_fx_off();
    const int16_t *p = sy_patch.p + o;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_id(), band = -1;
    int points = UI_PG[ui.page].id[0] == UI_EQPT, live;
    if (act >= 0 && act < UI_ITEM)
        act = (int32_t)sp_fx_base((uint32_t)act);   /* (EQ 2's: as the first's) */
    float ys[UI_FN], y2[UI_FN], lo[9], hi[9], v[9], vl[9], vh[9], plo, phi;
    uint32_t k, flat = 1, modded = 0;
    char w[32];
    for (k = 0; k < 9u; k++) {
        modded |= ui_mod_span(UI_EQ_ID[k] + o, SRC_COUNT, &lo[k], &hi[k], &plo, &phi) > 0u;
        if (act == (int32_t)UI_EQ_ID[k])
            band = (int32_t)(k % 3u);
    }
    if (band < 0 && act != SP_EQ_ON)
        act = -1;
    live = modded && ui_sounding();
    ui_rack();
    ui_freq_grid(WY + 20, WY + 100);
    for (k = 0; k < 5u; k++)                        /* -12 -6 0 +6 +12 dB */
        cv_rect(WX + 8, sy_floori(UI_QY(6.0f * (float)k - 12.0f) + 0.5f), 211, 1, k == 2u ? sk.grid2 : sk.grid);
    if (modded) {                                   /* the range modulation sweeps: the curve at each end */
        ui_eq_vals(p, lo, 0, vl);
        ui_eq_vals(p, hi, 0, vh);
        ui_eq_curve(vl, ys);
        ui_eq_curve(vh, y2);
        cv_band(WX + 8, ys, y2, UI_FN, sk.mband);
    }
    if (gh && band >= 0) {                          /* the curve before the turn */
        ui_eq_vals((gh->p + o), 0, 0, v);
        ui_eq_curve(v, y2);
        cv_curve_dash(WX + 8, y2, UI_FN, 1.0f, sk.ghost, 3u, 2u);
    }
    ui_eq_vals(p, 0, live, v);
    ui_eq_curve(v, ys);
    cv_under(WX + 8, ys, UI_FN, UI_QY(0.0f), sk.wfill);
    cv_curve(WX + 8, ys, UI_FN, 1.6f, band >= 0 ? sk.now : sk.sound);
    for (k = 0; k < 3u; k++) {                      /* the points */
        int32_t x0 = sy_floori(ui_hz_x(sp_cut_hz(ui_v(p, UI_EQ_ID[3u + k]))) + 0.5f);
        int32_t y0 = sy_floori(UI_QY((float)p[UI_EQ_ID[k]] * 0.1f) + 0.5f);
        int32_t x = sy_floori(ui_hz_x(sp_cut_hz(v[3u + k])) + 0.5f), y = sy_floori(UI_QY(ui_eq_gain(v, k)) + 0.5f);
        flat &= p[UI_EQ_ID[k]] == 0;
        if (hi[k] > lo[k]) {                        /* its gain's sweep */
            int32_t ya = sy_floori(UI_QY(ui_eq_gain(vh, k)) + 0.5f), yb = sy_floori(UI_QY(ui_eq_gain(vl, k)) + 0.5f);
            cv_rect(x0 - 1, ya < yb ? ya : yb, 2, (ya < yb ? yb - ya : ya - yb) + 1, sk.mod);
        }
        if (hi[3u + k] > lo[3u + k]) {              /* its frequency's sweep */
            int32_t xa = sy_floori(ui_hz_x(sp_cut_hz(vl[3u + k])) + 0.5f);
            int32_t xb = sy_floori(ui_hz_x(sp_cut_hz(vh[3u + k])) + 0.5f);
            cv_rect(xa < xb ? xa : xb, y0 - 1, (xa < xb ? xb - xa : xa - xb) + 1, 2, sk.mod);
        }
        if ((int32_t)k == band) {
            cv_rect(x - 3, y - 3, 7, 7, sk.now);
        } else if (points && k == ui.eqpt) {
            cv_rect(x - 3, y - 3, 7, 7, sk.sound);
        } else {
            cv_rect(x - 2, y - 2, 5, 5, sk.sound);
            cv_rect(x - 1, y - 1, 3, 3, sk.display);
        }
    }
    cv_text_on(WX + 4, WY + 21, &AF_XS, "+18", sk.faint, sk.display);
    cv_text_on(WX + 4, WY + 86, &AF_XS, "-18", sk.faint, sk.display);
    if (flat && !modded)
        ui_text_c(WX, WY + 67, WW, &AF_XS, "FLAT: EVERY BAND AT 0 dB", sk.faint, sk.display);
    if (act >= 0) {
        int32_t x = WX + 150, y = WY + 20;
        if (band >= 0) {
            x = sy_floori(ui_hz_x(sp_cut_hz(v[3u + (uint32_t)band]))) - 24;
            y = sy_floori(UI_QY(ui_eq_gain(v, (uint32_t)band)));
            y = y > WY + 50 ? y - 20 : y + 8;
        }
        ui_val(sp_put(sp_put(w, SP[act].name), " "), act + (int32_t)o);
        ui_flag(x, y, w);
    }
}

/* ------------------------------------------------------------------------------------ the chain view --- */
/* as the preset browser: the types at the left (the one KNOB 1 picked raised, each one's instances in the chain:
 * 1/2, IN; faint when none is left to add), the chain at the right (its modules in order, each lit when in, dim
 * when bypassed; the slot KNOB 2 picked marked; past the last, the place to add one) */
static void ui_g_chain(void)
{
    uint32_t nc = sp_chain_n(sy_patch.p), at = ui_cslot(), first, k;
    char t[24];
    first = ui.ctype > 4u ? ui.ctype - 4u : 0u;     /* nine rows of the 13, the one picked in view */
    if (first > FXT_N - 9u)
        first = FXT_N - 9u;
    for (k = first; k < first + 9u; k++) {
        int32_t y = WY + 3 + (int32_t)(k - first) * 11;
        uint32_t used = (FXT_INST[k][0] != 0xFF && sp_chain_find(sy_patch.p, FXT_INST[k][0]) >= 0) +
                        (FXT_INST[k][1] != 0xFF && sp_chain_find(sy_patch.p, FXT_INST[k][1]) >= 0);
        int on = k == ui.ctype, free = ui_cfree(k) >= 0 && nc < NFXC;
        uint16_t bg = on ? sk.raised : sk.display;
        if (on)
            cv_rrect(WX + 3, y, 84, 11, 2, sk.raised, sk.display);
        cv_text_on(WX + 7, y + CAP_IN(XS, 11), &AF_XS, FXT_NAMES[k], on ? sk.text : free ? sk.text2 : sk.faint, bg);
        if (used) {
            if (FXT_INST[k][1] == 0xFF)
                sp_put(t, "IN");
            else
                sp_put(sp_fix(t, (int32_t)used, 0u, 0), "/2");
            cv_text_r(WX + 84, y + CAP_IN(XS, 11), &AF_XS, t, on ? sk.label : sk.faint, bg);
        }
    }
    cv_rect(WX + 90, WY + 4, 1, 100, sk.dline);
    if (!nc)
        ui_text_c(WX + 92, WY + 40, 133, &AF_XS, "EMPTY: FX + OCT+ ADDS", sk.faint, sk.display);
    for (k = 0; k < nc + (nc < NFXC); k++) {
        int32_t y = WY + 3 + (int32_t)k * 11, in = sp_chain_at(sy_patch.p, k);
        int on = k == at, live = in >= 0 && sy_patch.p[FXI_ON[in]];
        uint16_t bg = on ? sk.raised : sk.display;
        if (!nc)
            break;
        if (on) {
            cv_rrect(WX + 94, y, 129, 11, 2, sk.raised, sk.display);
            cv_rect(WX + 96, y + 2, 2, 7, sk.now);
        }
        if (in < 0) {
            cv_text_on(WX + 102, y + CAP_IN(XS, 11), &AF_XS, "+ HERE (FX + OCT+)", on ? sk.label : sk.faint, bg);
            continue;
        }
        sp_fix(t, (int32_t)k + 1, 0u, 0);
        cv_text_on(WX + 102, y + CAP_IN(XS, 11), &AF_XS, t, sk.dim, bg);
        cv_text_on(WX + 112, y + CAP_IN(XS, 11), &AF_XS, SEC_NAMES[SEC_DRIVE + (uint32_t)in],
                   on ? sk.text : live ? sk.text2 : sk.faint, bg);
        if (!live)
            cv_text_r(WX + 220, y + CAP_IN(XS, 11), &AF_XS, "BYPASS", sk.faint, bg);
    }
}

/* a module's rate as its knob shows it: Hz, or with SYNC FIXED the note value */
static char *ui_fx_rate(char *w, uint32_t rate, uint32_t sync, uint32_t note)
{
    if (sy_patch.p[sync])
        return sp_put(w, NOTE_NAMES[sy_patch.p[note] < (int16_t)SY_NNOTE ? sy_patch.p[note] : 0]);
    sp_text(rate, sy_patch.p[rate], w);
    return w;
}

/* the response in dB at f of a comb (d samples, feedback fb, mix m: the flanger's tap at rest at d) */
static float ui_comb_db(float d, float fb, float m, float f)
{
    float a = 6.2831853f * f * d * (1.0f / SY_FS), c = sy_cos2pi(a * 0.15915494f), s = sy_sin2pi(a * 0.15915494f);
    ui_cx e = {c, -s}, one = {1.0f, 0.0f}, den, w;
    den.r = 1.0f - fb * e.r;
    den.i = -fb * e.i;
    w = cx_div(e, den);
    w.r = 1.0f + (w.r - 1.0f) * m;                  /* out = x + (wet - x) mix */
    w.i *= m;
    (void)one;
    return 3.0103f * sy_log2f(w.r * w.r + w.i * w.i + 1e-12f);
}

#define UI_XY(db) ((float)WY + 24.0f + (12.0f - sy_clampf((db), -36.0f, 12.0f)) * (72.0f / 48.0f))   /* +12 .. -36 dB */
static void ui_xgrid(void)                          /* the frequency grid and 0 dB, -24 dB */
{
    ui_freq_grid(WY + 20, WY + 96);
    cv_rect(WX + 8, sy_floori(UI_XY(0.0f)), 211, 1, sk.grid2);
    cv_rect(WX + 8, sy_floori(UI_XY(-24.0f)), 211, 1, sk.grid);
}

/* FLANGER: its comb's response with the tap at the middle of its sweep, the sweep's ends as a band */
static void ui_g_flanger(void)
{
    uint32_t o = ui_fx_off(), i;
    const int16_t *p = sy_patch.p + o;
    int32_t act = ui_act_fx(SP_FL_ON, SP_FL_MIX);
    float lo = (0.3f + 4.7f * ui_v(p, SP_FL_DELAY)) * 0.001f * SY_FS, sw = 5.0f * ui_v(p, SP_FL_DEPTH) * 0.001f * SY_FS;
    float fb = 0.95f * (float)p[SP_FL_FDBK] * 0.001f, m = ui_v(p, SP_FL_MIX), ya[UI_FN], yb[UI_FN], ys[UI_FN];
    char w[32], *e;
    ui_rack();
    ui_xgrid();
    for (i = 0; i < UI_FN; i++) {
        float f = ui_col_hz(i);
        ya[i] = UI_XY(ui_comb_db(lo, fb, m, f));
        yb[i] = UI_XY(ui_comb_db(lo + sw, fb, m, f));
        ys[i] = UI_XY(ui_comb_db(lo + 0.5f * sw, fb, m, f));
    }
    if (sw > 0.5f)
        cv_band(WX + 8, ya, yb, UI_FN, sk.mband);
    cv_curve(WX + 8, ys, UI_FN, 1.6f, act >= 0 ? sk.now : sk.sound);
    e = sp_put(sp_real(sp_put(w, "TAP "), lo * 1000.0f / SY_FS, 1u, ""), " .. ");
    e = sp_put(sp_real(e, (lo + sw) * 1000.0f / SY_FS, 1u, " ms"), " \xB7 ");
    ui_fx_rate(e, SP_FL_RATE + o, SP_FL_SYNC + o, SP_FL_NOTE + o);
    cv_text_on(WX + 6, WY + 18, &AF_XS, w, sk.dim, sk.display);
    if (act >= 0)
        ui_flag(WX + 150, WY + 20, ui_val(w, act + (int32_t)o));
}

/* PHASER: its stages' and the dry's response at the sweep's middle, the sweep's ends as a band */
static float ui_phs_db(float cen, uint32_t ns, float fb, float m, float f)
{
    float t = sy_tanpi(sy_clampf(cen, 20.0f, 18000.0f) * (1.0f / SY_FS)), a = (t - 1.0f) / (t + 1.0f);
    float ph = f * (1.0f / SY_FS), c = sy_cos2pi(ph), s = sy_sin2pi(ph);
    ui_cx e = {c, -s}, num = {a + e.r, e.i}, den = {1.0f + a * e.r, a * e.i}, st = cx_div(num, den), y = {1.0f, 0.0f};
    uint32_t k;
    for (k = 0; k < ns; k++)
        y = cx_mul(y, st);
    {
        ui_cx d = {1.0f - fb * y.r, -fb * y.i};
        y = cx_div(y, d);
    }
    y.r = 1.0f + (y.r - 1.0f) * m;
    y.i *= m;
    return 3.0103f * sy_log2f(y.r * y.r + y.i * y.i + 1e-12f);
}
static void ui_g_phaser(void)
{
    static const uint8_t NST[6] = {2, 4, 6, 8, 10, 12};
    uint32_t o = ui_fx_off(), i, ns = NST[sy_clampi(sy_patch.p[SP_PH_STAGES + o], 0, 5)];
    const int16_t *p = sy_patch.p + o;
    int32_t act = ui_act_fx(SP_PH_ON, SP_PH_MIX);
    float cen = sp_cut_hz(ui_v(p, SP_PH_CENTER)), oct = 2.5f * ui_v(p, SP_PH_DEPTH);
    float fb = 0.9f * (float)p[SP_PH_FDBK] * 0.001f, m = ui_v(p, SP_PH_MIX), ya[UI_FN], yb[UI_FN], ys[UI_FN];
    char w[32], *e;
    ui_rack();
    ui_xgrid();
    for (i = 0; i < UI_FN; i++) {
        float f = ui_col_hz(i);
        ya[i] = UI_XY(ui_phs_db(cen * sy_exp2f(-oct), ns, fb, m, f));
        yb[i] = UI_XY(ui_phs_db(cen * sy_exp2f(oct), ns, fb, m, f));
        ys[i] = UI_XY(ui_phs_db(cen, ns, fb, m, f));
    }
    if (oct > 0.01f)
        cv_band(WX + 8, ya, yb, UI_FN, sk.mband);
    cv_curve(WX + 8, ys, UI_FN, 1.6f, act >= 0 ? sk.now : sk.sound);
    e = sp_put(sp_fix(w, (int32_t)ns, 0u, 0), " STAGES \xB7 ");
    ui_fx_rate(e, SP_PH_RATE + o, SP_PH_SYNC + o, SP_PH_NOTE + o);
    cv_text_on(WX + 6, WY + 18, &AF_XS, w, sk.dim, sk.display);
    if (act >= 0)
        ui_flag(act == SP_PH_CENTER ? sy_floori(ui_hz_x(cen)) - 20 : WX + 150, WY + 20, ui_val(w, act + (int32_t)o));
}

/* the FX filter: its type's response (the voice filter's types as the FILTER page draws them; LO-FI its low pass;
 * RING flat, the carrier named) */
static float ui_ffx_db(uint32_t t, float hz, float res, float mix, float f)
{
    float tf = sy_tanpi(sy_minf(f, 0.468f * SY_FS) * (1.0f / SY_FS)), db = 0.0f;
    ui_cx h = {1.0f, 0.0f};
    if (FF_FLT[t] != 0xFF) {                        /* (the voice filter's: its own curve) */
        int16_t q[SP_COUNT];
        memset(q, 0, sizeof q);
        q[SP_F_TYPE] = FF_FLT[t];
        q[SP_F_RES] = (int16_t)(res * 1000.0f + 0.5f);
        q[SP_F_MIX] = (int16_t)(mix * 1000.0f + 0.5f);
        q[SP_F_VAR] = t == FF_FORMANT ? 500 : 0;
        return ui_flt_db(q, hz, f);
    }
    if (t == FF_LP6 || t == FF_HP6) {
        float w = tf / flt_g(hz);
        ui_cx d = {1.0f, w}, n = {t == FF_LP6 ? 1.0f : 0.0f, t == FF_LP6 ? 0.0f : w};
        h = cx_div(n, d);
    } else if (t == FF_DIRTY || t == FF_LOFI) {
        float w = tf / flt_g(t == FF_LOFI ? sy_minf(hz * 0.45f, 20000.0f) : hz);
        h = ui_svf_h(FR_LP, t == FF_LOFI ? 1.414f : flt_k(res, 1.414f), w);
    } else if (t == FF_COMBP || t == FF_COMBN) {
        float d = sy_clampf(SY_FS / hz, 2.0f, (float)(FX_LN - 4u)), fb = (t == FF_COMBP ? 0.97f : -0.97f) * res;
        float ph = f * d * (1.0f / SY_FS);
        ui_cx e = {sy_cos2pi(ph), -sy_sin2pi(ph)}, den = {1.0f - fb * e.r, -fb * e.i}, one = {1.0f - 0.5f * sy_absf(fb), 0.0f};
        h = cx_div(one, den);
    } else if (t == FF_LSHELF || t == FF_HSHELF || t == FF_TILT) {
        float g = (res - 0.5f) * 36.0f;
        db = t == FF_TILT ? fx_eq_db(EQ_LS, hz, -0.5f * g, 0.7071f, f) + fx_eq_db(EQ_HS, hz, 0.5f * g, 0.7071f, f)
                          : fx_eq_db(t == FF_LSHELF ? EQ_LS : EQ_HS, hz, g, 0.7071f, f);
        h.r = sy_exp2f(db * 0.1660964f);
        h.i = 0.0f;
    } else if (t == FF_DJ) {
        float cn = sy_log2f(hz * 0.05f) * (1.0f / 9.96578f);
        float lo = cn < 0.49f ? sp_cut_hz(cn / 0.49f) : 20000.0f, hi = cn > 0.51f ? sp_cut_hz((cn - 0.51f) / 0.49f) : 20.0f;
        float k = flt_k(res, 1.414f);
        h = cx_mul(ui_svf_h(FR_LP, k, tf / flt_g(lo)), ui_svf_h(FR_HP, k, tf / flt_g(hi)));
    }
    h.r = 1.0f + (h.r - 1.0f) * mix;
    h.i *= mix;
    return 3.0103f * sy_log2f(h.r * h.r + h.i * h.i + 1e-12f);
}
static void ui_g_ffilt(void)
{
    uint32_t o = ui_fx_off(), i, t = (uint32_t)sy_clampi(sy_patch.p[SP_FF_TYPE + o], 0, FF_COUNT - 1);
    const int16_t *p = sy_patch.p + o;
    int32_t act = ui_act_fx(SP_FF_ON, SP_FF_MIX);
    float hz = sp_cut_hz(ui_v(p, SP_FF_CUT)), res = ui_v(p, SP_FF_RES), mix = ui_v(p, SP_FF_MIX), ys[UI_FN];
    char w[32], *e;
    ui_rack();
    ui_xgrid();
    for (i = 0; i < UI_FN; i++)
        ys[i] = UI_XY(t == FF_RING ? 0.0f : ui_ffx_db(t, hz, res, mix, ui_col_hz(i)));
    if (t == FF_RING) {
        cv_curve_dash(WX + 8, ys, UI_FN, 1.2f, sk.wmid, 3u, 2u);
    } else {
        cv_under(WX + 8, ys, UI_FN, UI_XY(-36.0f), sk.wfill);
        cv_curve(WX + 8, ys, UI_FN, 1.6f, act >= 0 ? sk.now : sk.sound);
    }
    if (act == SP_FF_CUT)
        cv_vdash(sy_floori(ui_hz_x(hz) + 0.5f), WY + 30, WY + 96, 2, 2, sk.now);
    e = sp_put(w, FF_NAMES[t]);
    if (t == FF_RING || t == FF_LOFI) {
        e = sp_put(e, t == FF_RING ? " \xB7 TIMES " : " \xB7 HELD AT ");
        sp_freq(e, hz, 1u);
    } else if (t == FF_DJ) {
        sp_put(e, ui_v(p, SP_FF_CUT) < 0.49f ? " \xB7 LOW PASS" : ui_v(p, SP_FF_CUT) > 0.51f ? " \xB7 HIGH PASS" : " \xB7 OPEN");
    }
    cv_text_on(WX + 6, WY + 18, &AF_XS, w, sk.dim, sk.display);
    if (act >= 0)
        ui_flag(act == SP_FF_CUT ? sy_floori(ui_hz_x(hz)) - 24 : WX + 150, WY + 20, ui_val(w, act + (int32_t)o));
}

/* BITCRUSH: a cycle of a 441 Hz sine as it plays through: its levels (BITS) and its holds (RATE) */
static void ui_g_crush(void)
{
    uint32_t o = ui_fx_off(), i;
    const int16_t *p = sy_patch.p + o;
    int32_t act = ui_act_fx(SP_CR_ON, SP_CR_MIX);
    float step = sp_cut_hz(ui_v(p, SP_CR_RATE)) * (1.0f / SY_FS), qs = sy_exp2f(1.0f - (float)p[SP_CR_BITS]);
    float m = ui_v(p, SP_CR_MIX), ph = 1.0f, hold = 0.0f, ys[200], yc[200];
    char w[32], *e;
    ui_rack();
    cv_rect(WX + 14, WY + 60, 200, 1, sk.grid2);
    for (i = 0; i < 200u; i++) {                    /* (100 samples a cycle at 44.1 kHz, two columns each) */
        float x = 0.9f * sy_sin2pi((float)(i / 2u) * 0.01f);
        if (!(i & 1u)) {
            ph += step;
            if (ph >= 1.0f) {
                ph -= (float)sy_floori(ph);
                hold = sy_clampf((float)sy_floori(x / qs + 0.5f) * qs, -1.0f, 1.0f);
            }
        }
        ys[i] = (float)(WY + 60) - x * 36.0f;
        yc[i] = (float)(WY + 60) - (x + (hold - x) * m) * 36.0f;
    }
    cv_curve(WX + 14, ys, 200u, 1.0f, sk.ghost);
    cv_curve(WX + 14, yc, 200u, 1.6f, act >= 0 ? sk.now : sk.sound);
    e = sp_put(sp_fix(w, p[SP_CR_BITS], 0u, 0), p[SP_CR_BITS] == 1 ? " BIT \xB7 " : " BITS \xB7 ");
    sp_text(SP_CR_RATE + o, p[SP_CR_RATE], e);
    cv_text_on(WX + 6, WY + 18, &AF_XS, w, sk.dim, sk.display);
    if (act >= 0)
        ui_flag(WX + 150, WY + 20, ui_val(w, act + (int32_t)o));
}

/* TREMOLO: two cycles of its gain (AUTOPAN: each side's) */
static void ui_g_trem(void)
{
    uint32_t o = ui_fx_off(), i, c, shape = (uint32_t)sy_clampi(sy_patch.p[SP_TR_SHAPE + o], 0, 3);
    const int16_t *p = sy_patch.p + o;
    int32_t act = ui_act_fx(SP_TR_ON, SP_TR_DEPTH), pan = p[SP_TR_MODE] != 0;
    float dep = ui_v(p, SP_TR_DEPTH), ys[2][200];
    char w[32], *e;
    ui_rack();
    cv_rect(WX + 14, WY + 92, 200, 1, sk.grid2);
    cv_rect(WX + 14, WY + 26, 200, 1, sk.grid);
    for (i = 0; i < 200u; i++) {
        float v = fx_trm_lfo(shape, (float)(i % 100u) * 0.01f), g[2];
        if (pan) {
            float q = 0.5f + 0.5f * dep * v;
            g[0] = sy_sin2pi(0.25f * (1.0f - q));
            g[1] = sy_sin2pi(0.25f * q);
        } else {
            g[0] = g[1] = 1.0f - dep * (0.5f - 0.5f * v);
        }
        for (c = 0; c < 2u; c++)
            ys[c][i] = (float)(WY + 92) - g[c] * 66.0f;
    }
    if (pan)
        cv_curve(WX + 14, ys[1], 200u, 1.2f, sk.wmid);
    cv_curve(WX + 14, ys[0], 200u, 1.6f, act >= 0 ? sk.now : sk.sound);
    e = sp_put(w, pan ? "L SOLID, R THIN \xB7 " : "THE LEVEL \xB7 ");
    ui_fx_rate(e, SP_TR_RATE + o, SP_TR_SYNC + o, SP_TR_NOTE + o);
    cv_text_on(WX + 6, WY + 18, &AF_XS, w, sk.dim, sk.display);
    if (act >= 0)
        ui_flag(WX + 150, WY + 20, ui_val(w, act + (int32_t)o));
}

/* RING MOD: the carrier (sine to square), a few of its cycles */
static void ui_g_ring(void)
{
    uint32_t o = ui_fx_off(), i;
    const int16_t *p = sy_patch.p + o;
    int32_t act = ui_act_fx(SP_RG_ON, SP_RG_MIX);
    float shape = ui_v(p, SP_RG_SHAPE), ys[200];
    char w[32], *e;
    ui_rack();
    cv_rect(WX + 14, WY + 60, 200, 1, sk.grid2);
    for (i = 0; i < 200u; i++) {
        float v = sy_sin2pi((float)i * 0.02f), sq = v < 0.0f ? -1.0f : 1.0f;
        ys[i] = (float)(WY + 60) - (v + (sq - v) * shape) * 34.0f;
    }
    cv_curve(WX + 14, ys, 200u, 1.6f, act >= 0 ? sk.now : sk.sound);
    e = sp_put(w, "THE CARRIER \xB7 ");
    sp_text(SP_RG_FREQ + o, p[SP_RG_FREQ], e);
    cv_text_on(WX + 6, WY + 18, &AF_XS, w, sk.dim, sk.display);
    if (act >= 0)
        ui_flag(WX + 150, WY + 20, ui_val(w, act + (int32_t)o));
}

/* TAPE: two seconds of its pitch's drift (WOW and FLUTTER), in cents */
static void ui_g_tape(void)
{
    uint32_t o = ui_fx_off(), i;
    const int16_t *p = sy_patch.p + o;
    int32_t act = ui_act_fx(SP_TP_ON, SP_TP_MIX);
    float wow = ui_v(p, SP_TP_WOW) * 0.0025f * SY_FS, flt = ui_v(p, SP_TP_FLUT) * 0.0004f * SY_FS, ys[200], top = 0.0f;
    char w[32], *e;
    ui_rack();
    cv_rect(WX + 14, WY + 60, 200, 1, sk.grid2);
    for (i = 0; i < 200u; i++) {                    /* the tap's speed: 1 - its delay's slope (samples a sample) */
        float t = (float)i * 0.01f;
        float dd = wow * 6.2831853f * 0.6f * sy_cos2pi(0.6f * t) +
                   flt * 6.2831853f * (0.67f * 9.0f * sy_cos2pi(9.0f * t) + 0.33f * 13.0f * sy_cos2pi(13.0f * t));
        float ct = 1731.234f * (-dd / SY_FS);       /* (1200 / ln 2: cents for a small ratio) */
        top = sy_maxf(top, sy_absf(ct));
        ys[i] = (float)(WY + 60) - sy_clampf(ct, -40.0f, 40.0f) * 0.85f;
    }
    cv_curve(WX + 14, ys, 200u, 1.6f, act >= 0 ? sk.now : sk.sound);
    e = sp_put(w, "PITCH \xB1");
    sp_put(sp_real(e, top, 1u, " CT"), " OVER 2 S");
    cv_text_on(WX + 6, WY + 18, &AF_XS, w, sk.dim, sk.display);
    if (act >= 0)
        ui_flag(WX + 150, WY + 20, ui_val(w, act + (int32_t)o));
}
