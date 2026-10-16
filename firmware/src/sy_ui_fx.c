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

/* the rack: the six effects across the top of the window, the one shown outlined, each lit when on */
static void ui_rack(void)
{
    static const char *const N[UI_NFX] = {"DRV", "CHO", "DLY", "REV", "OTT", "EQ"};
    uint32_t i, cur = ui_sec() - SEC_DRIVE;
    for (i = 0; i < UI_NFX; i++) {
        int on = sy_patch.p[ui_sec_power(SEC_DRIVE + i)] != 0, here = i == cur;
        int32_t x = WX + 3 + (int32_t)i * 37, lx = x + (26 - text_w(&AF_XS, N[i])) / 2;
        uint16_t bg = here ? sk.raised : sk.display, bd = here ? sk.now : on ? sk.wmid : sk.rule;
        cv_rrect(x, WY + 3, 34, 13, 3, bd, sk.display);
        cv_rrect(x + 1, WY + 4, 32, 11, 2, bg, bd);
        cv_rect(lx, WY + 8, 4, 4, on ? sk.sound : sk.faint);
        if (!on)
            cv_rect(lx + 1, WY + 9, 2, 2, bg);
        cv_text_on(lx + 8, WY + 3 + CAP_IN(XS, 13), &AF_XS, N[i], here ? sk.text : on ? sk.text2 : sk.faint, bg);
    }
}

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
    const int16_t *p = sy_patch.p;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_in(SP_DR_ON, SP_DR_MIX);
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
    ui_mod_span(SP_DR_AMT, SRC_COUNT, &olo, &ohi, &plo, &phi);
    if (ohi > olo) {
        ui_drv_curve(p, sy_clampf(amt + olo, 0.0f, 1.0f), ys);
        ui_drv_curve(p, sy_clampf(amt + ohi, 0.0f, 1.0f), y2);
        cv_band(WX + 10, ys, y2, 81u, sk.mband);
    }
    if (gh && moving) {
        ui_drv_curve(gh->p, ui_v(gh->p, SP_DR_AMT), y2);
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
    ui_val(w + 5, SP_DR_TONE);
    ui_label(-1, 0, w, act == SP_DR_TONE ? sk.now : sk.faint);
    if (act >= 0)
        ui_flag(act == SP_DR_MIX ? WX + 160 : act == SP_DR_TONE ? WX + 150 : WX + 13,
                act == SP_DR_MIX ? WY + 18 : act == SP_DR_TONE ? WY + 80 : WY + 25, ui_val(w, act));
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
    const int16_t *p = sy_patch.p;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_in(SP_CH_ON, SP_CH_MIX);
    int sweep = act == SP_CH_DEPTH || act == SP_CH_DELAY;
    float base = 2.0f + 13.0f * ui_v(p, SP_CH_DELAY), depth = 3.0f * ui_v(p, SP_CH_DEPTH), bm = base + 0.5f * depth;
    float ph = (float)(fx.cho_ph >> 8) * (1.0f / 16777216.0f), sn = sy_sin2pi(ph), cs = sy_sin2pi(ph + 0.25f);
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
    ui_mod_span(SP_CH_DEPTH, SRC_COUNT, &olo, &ohi, &plo, &phi);
    ui_mod_span(SP_CH_DELAY, SRC_COUNT, &dlo, &dhi, &plo, &phi);
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
            float gb = 2.0f + 13.0f * ui_v(gh->p, SP_CH_DELAY), gd = 3.0f * ui_v(gh->p, SP_CH_DEPTH);
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
            ui_val(w, act);
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
                e = sp_put(ui_val(w, act) + strlen(w), " \xB7 ");
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
    const int16_t *p = sy_patch.p;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_in(SP_OT_ON, SP_OT_HIGH);
    float depth = ui_v(p, SP_OT_DEPTH);
    uint32_t b;
    char w[24];
#define UI_LV(db) ((float)WY + 32.0f + sy_clampf(-(db), 0.0f, 60.0f) * (58.0f / 60.0f))
    ui_rack();
    for (b = 0; b < 3u; b++) {
        int32_t x = WX + 8 + (int32_t)b * 72, ya, yb, cx = x + 33;
        float in = 6.0206f * sy_log2f(sy_maxf(fx.ott_env[b], 1e-6f));
        float g = sy_maxf(1.0f + (fx.ott_g[b] - 1.0f) * depth, 1e-6f), out = in + 6.0206f * sy_log2f(g);
        int band = act == SP_OT_LOW + (int32_t)b, push = act >= SP_OT_DEPTH && act <= SP_OT_DOWN;
        cv_rect(x + 18, WY + 32, 30, 58, sk.grid);
        ya = sy_floori(UI_LV(in) + 0.5f);
        cv_rect(x + 18, ya, 30, WY + 90 - ya, sk.wmid);
        cv_rect(x + 14, sy_floori(UI_LV(-12.0f) + 0.5f), 38, 1, sk.label);   /* down above, up below */
        cv_dash(x + 14, x + 52, sy_floori(UI_LV(-40.0f) + 0.5f), 2, 2, sk.label);
        if (gh && push) {
            float g0 = sy_maxf(1.0f + (fx.ott_g[b] - 1.0f) * ui_v(gh->p, SP_OT_DEPTH), 1e-6f);
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
        ui_val(w, SP_OT_LOW + b);
        cv_text_r(x + 64, WY + 94, &AF_XS, w, band ? sk.now : sk.faint, sk.display);
    }
    cv_text_on(WX + 61, sy_floori(UI_LV(-12.0f)) - 6, &AF_XS, "-12", sk.label, sk.display);
    cv_text_on(WX + 61, sy_floori(UI_LV(-40.0f)) - 6, &AF_XS, "-40", sk.label, sk.display);
    if (act >= 0) {
        int32_t k = act >= SP_OT_LOW ? act - SP_OT_LOW : -1;
        sp_put(sp_put(w, SP[act].name), " ");
        ui_val(w + strlen(w), act);
        ui_flag(k >= 0 ? WX + 8 + k * 72 + 4 : WX + 60, k >= 0 ? WY + 17 : WY + 17, w);
    }
#undef UI_LV
}

/* -------------------------------------------------------------------------------------------- EQ --- */
static const uint8_t UI_EQ_DB[3] = {SP_EQ_LOW, SP_EQ_MID, SP_EQ_HIGH}, UI_EQ_HZ[3] = {SP_EQ_LFREQ, SP_EQ_MFREQ, SP_EQ_HFREQ};
#define UI_QY(db) ((float)WY + 61.0f - sy_clampf((db), -20.0f, 20.0f) * 2.0f)   /* +-18 dB: 2 px a dB */

/* the EQ's gain at f in dB for values p (each band's gain, frequency and the peak's Q; off-set by o: the
 * modulation's range, as parts of each range) */
static float ui_eq_db(const int16_t *p, const float *o, float f)
{
    float db = 0.0f, q = sp_q(sy_clampf(ui_v(p, SP_EQ_Q) + (o ? o[6] : 0.0f), 0.0f, 1.0f));
    uint32_t b;
    for (b = 0; b < 3u; b++) {
        float g = sp_lin(UI_EQ_DB[b], sy_clampf(ui_v(p, UI_EQ_DB[b]) + (o ? o[b] : 0.0f), 0.0f, 1.0f)) * 0.1f;
        if (sy_absf(g) >= 0.01f)
            db += fx_eq_db(b, sp_cut_hz(sy_clampf(ui_v(p, UI_EQ_HZ[b]) + (o ? o[3u + b] : 0.0f), 0.0f, 1.0f)), g, q, f);
    }
    return db;
}
static void ui_eq_curve(const int16_t *p, const float *o, float *ys)
{
    uint32_t i;
    for (i = 0; i < UI_FN; i++)
        ys[i] = UI_QY(ui_eq_db(p, o, ui_col_hz(i)));
}

static void ui_g_eq(void)
{
    static const uint8_t IDS[7] = {SP_EQ_LOW, SP_EQ_MID, SP_EQ_HIGH, SP_EQ_LFREQ, SP_EQ_MFREQ, SP_EQ_HFREQ, SP_EQ_Q};
    const int16_t *p = sy_patch.p;
    const sy_patch_t *gh = ui_ghost();
    int32_t act = ui_act_in(SP_EQ_ON, SP_EQ_HFREQ);
    int band = act == SP_EQ_LOW || act == SP_EQ_LFREQ ? 0 : act == SP_EQ_MID || act == SP_EQ_MFREQ || act == SP_EQ_Q ? 1
             : act == SP_EQ_HIGH || act == SP_EQ_HFREQ ? 2 : -1;
    float ys[UI_FN], y2[UI_FN], lo[7], hi[7], plo, phi;
    uint32_t k, flat = 1, modded = 0;
    char w[32];
    ui_rack();
    ui_freq_grid(WY + 20, WY + 100);
    for (k = 0; k < 5u; k++)                        /* -12 -6 0 +6 +12 dB */
        cv_rect(WX + 8, sy_floori(UI_QY(6.0f * (float)k - 12.0f) + 0.5f), 211, 1, k == 2u ? sk.grid2 : sk.grid);
    for (k = 0; k < 7u; k++)
        modded |= ui_mod_span(IDS[k], SRC_COUNT, &lo[k], &hi[k], &plo, &phi) > 0u;
    if (modded) {                                   /* the range modulation sweeps: the curve at each end */
        ui_eq_curve(p, lo, ys);
        ui_eq_curve(p, hi, y2);
        cv_band(WX + 8, ys, y2, UI_FN, sk.mband);
    }
    if (gh && act >= 0) {
        ui_eq_curve(gh->p, 0, y2);
        cv_curve_dash(WX + 8, y2, UI_FN, 1.0f, sk.ghost, 3u, 2u);
    }
    ui_eq_curve(p, 0, ys);
    cv_under(WX + 8, ys, UI_FN, UI_QY(0.0f), sk.wfill);
    cv_curve(WX + 8, ys, UI_FN, 1.6f, act >= 0 && act != SP_EQ_ON ? sk.now : sk.sound);
    for (k = 0; k < 3u; k++) {                      /* each band's handle: its frequency, its gain */
        int32_t x = sy_floori(ui_hz_x(sp_cut_hz(ui_v(p, UI_EQ_HZ[k]))) + 0.5f);
        int32_t y = sy_floori(UI_QY((float)p[UI_EQ_DB[k]] * 0.1f) + 0.5f);
        flat &= p[UI_EQ_DB[k]] == 0;
        if ((int)k == band) {
            cv_rect(x - 3, y - 3, 7, 7, sk.now);
        } else {
            cv_rect(x - 2, y - 2, 5, 5, sk.sound);
            cv_rect(x - 1, y - 1, 3, 3, sk.display);
        }
    }
    cv_text_on(WX + 4, WY + 21, &AF_XS, "+18", sk.faint, sk.display);
    cv_text_on(WX + 4, WY + 86, &AF_XS, "-18", sk.faint, sk.display);
    if (flat)
        ui_text_c(WX, WY + 67, WW, &AF_XS, "FLAT: EVERY BAND AT 0 dB", sk.faint, sk.display);
    if (act >= 0) {
        int32_t x, y;
        if (band >= 0) {
            x = sy_floori(ui_hz_x(sp_cut_hz(ui_v(p, UI_EQ_HZ[band])))) - 24;
            y = sy_floori(UI_QY((float)p[UI_EQ_DB[band]] * 0.1f));
            y = y > WY + 50 ? y - 20 : y + 8;
        } else {
            x = WX + 150;
            y = WY + 20;
        }
        sp_put(sp_put(w, SP[act].name), " ");
        ui_val(w + strlen(w), act);
        ui_flag(x, y, w);
    }
}
