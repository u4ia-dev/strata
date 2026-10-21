/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* The STRATA UI's filter, effects and chain pages (the design's EDIT FILTER and FX boards): each draws what it does from
 * the values as the DSP reads them (the responses: each filter type's analog prototype at the SVF's warped frequency,
 * the EQ's bands as fx_eq plays them, the comb's and the phaser's at their sweep's middle), as one drawing whose parts
 * the knobs' handles sit on; a knob turning: its part HOT, the shape before the turn dashed. Every effect with a mix
 * shares the MIX row at the bottom.
 *   The chain view: the modules as boxes on a cable in the order they play (the one picked filled, a bypassed one
 * dashed; the place to add one at the end), the type to add as a tag.
 * Needs sy_ui.c, sy_ui_draw.c (pg, ui_H, ui_hot, ui_gh, ui_v), sy_gfx.c and the engine (sy_filter.c, sy_fx.c). */

static uint32_t ui_fx_nth(void) { return ui_fx_inst() >= 0 ? fx_nth((uint32_t)ui_fx_inst()) : 0u; }

/* a module's rate as its knob shows it: Hz, or with SYNC FIXED the note value */
static char *ui_fx_rate(char *w, uint32_t rate, uint32_t sync, uint32_t note)
{
    if (sy_patch.p[sync])
        return sp_put(w, NOTE_NAMES[sy_patch.p[note] < (int16_t)SY_NNOTE ? sy_patch.p[note] : 0]);
    sp_text(rate, sy_patch.p[rate], w);
    return w;
}

/* the frequency across: 20 Hz .. 20 kHz from x0, w px (the design's: three decades); and back */
static float ui_fx_x(float f, float x0, float w) { return x0 + w * sy_log2f(f * 0.05f) * (1.0f / 9.96578f); }
static float ui_fx_f(float x, float x0, float w) { return 20.0f * sy_exp2f((x - x0) / w * 9.96578f); }

/* the frequency's names under a response: 20, 200, 2K, 20K at row y */
static void ui_fx_freqs(float x0, float w, float y)
{
    lk_text(x0 - 4.0f, y, T9, "20", sk.lab, sk.ground, 0, 0);
    lk_text_c(ui_fx_x(200.0f, x0, w), y, T9, "200", sk.lab, sk.ground, 0, 0);
    lk_text_c(ui_fx_x(2000.0f, x0, w), y, T9, "2K", sk.lab, sk.ground, 0, 0);
    lk_text_c(sy_minf(ui_fx_x(20000.0f, x0, w), 226.0f), y, T9, "20K", sk.lab, sk.ground, 0, 0);
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
/* the cutoff the values p give at a note n semitones from C4 (KEYTRACK), one side of VAR (s -1, 0, +1) */
static float ui_flt_hz(const int16_t *p, float n, float s)
{
    float hz = sp_cut_hz(ui_v(p, SP_F_CUT)) * sy_exp2f(ui_v(p, SP_F_KEY) * n * (1.0f / 12.0f));
    return p[SP_F_TYPE] == FT_FORMANT ? hz : hz * sy_exp2f(s * ui_v(p, SP_F_VAR));
}
/* the response across 197 columns from x 12 (196 px: 20 Hz .. 20 kHz), dB to rows by y0 - db * k, kept in lo .. hi */
#define UI_FC 197u
static void ui_flt_curve(const int16_t *p, float hz, float y0, float k, float lo, float hi, float *ys)
{
    uint32_t i;
    for (i = 0; i < UI_FC; i++)
        ys[i] = sy_clampf(y0 - k * ui_flt_db(p, hz, ui_fx_f(12.0f + (float)i, 12.0f, 196.0f)), lo, hi);
}
static uint32_t ui_peak(const float *ys, uint32_t n)   /* the column of the curve's top */
{
    uint32_t i, t = 0;
    for (i = 1; i < n; i++)
        if (ys[i] < ys[t])
            t = i;
    return t;
}
/* a drive's small meter at the right: its track, its amount up from 120, a knob under it */
static void ui_drive_meter(float v, int hot)
{
    lk_seg(222.0f, 60.0f, 222.0f, 120.0f, 3.0f, sk.line);
    if (v > 0.004f)
        lk_seg(222.0f, 120.0f - 60.0f * v, 222.0f, 120.0f, 3.0f, ui_ink(hot));
    lk_ring(222.0f, 130.0f, 7.0f, 1.5f, ui_ink(hot));
    lk_text_r(236.0f, 44.0f, T9, "DRIVE", sk.lab, sk.ground, 20, 0);   /* (over the meter, clear of the curves) */
}

/* FILTER TONE: the hill the sound passes over (VAR: the right side's thin), stippled under; the cutoff's post, the
 * range its modulation takes it; DRIVE. MORE: KEYTRACK (keys pulling the cutoff), VAR's side dashed, MIX */
static void ui_p_filter(void)
{
    static const char *const SLOPE[FT_COUNT] = {"12 DB/OCT", "24 DB/OCT", "12 DB/OCT", "24 DB/OCT", "6 DB/OCT",
                                                "12 DB/OCT", "", "+12 DB", "24 DB/OCT", ""};
    const int16_t *p = sy_patch.p;
    uint32_t t = (uint32_t)p[SP_F_TYPE] % FT_COUNT, pk, i;
    int more = UI_PG[ui.page].id[0] == SP_F_KEY, spread = p[SP_F_TYPE] != FT_FORMANT && p[SP_F_VAR] > 0;
    const sy_patch_t *gh = ui_gh(more ? ui_hot(0) || ui_hot(1) : ui_hot(0) || ui_hot(1) || ui_hot(2));
    float *ys, *y2, hz = ui_flt_hz(p, 0.0f, spread ? -1.0f : 0.0f), xc;
    int fresh;
    char w[32];
    if (!p[SP_F_ON]) {
        ui_off("FILTER");
        return;
    }
    if (more) {                                     /* MORE */
        lk_kb_t kb;
        static const uint8_t KEY[3] = {0, 7, 26};
        static const int8_t SEMI[3] = {-7, 0, 19};
        y2 = ui_curve(0, &fresh);
        if (fresh)
            ui_flt_curve(p, ui_flt_hz(p, 0.0f, 1.0f), 92.0f, 1.4f, 44.0f, 132.0f, y2);
        lk_curve(12, y2, UI_FC, 1.0f, ui_hot(1) ? sk.hot : sk.mid, LK_DASH(2, 2));
        if (gh) {
            ys = ui_curve(1, &fresh);
            if (fresh)
                ui_flt_curve(gh->p, ui_flt_hz(gh->p, 0.0f, -1.0f), 92.0f, 1.4f, 44.0f, 132.0f, ys);
            lk_curve(12, ys, UI_FC, 1.0f, sk.line, LK_DASH(2, 2));
        }
        ys = ui_curve(2, &fresh);
        if (fresh)
            ui_flt_curve(p, ui_flt_hz(p, 0.0f, -1.0f), 92.0f, 1.4f, 44.0f, 132.0f, ys);
        lk_curve(12, ys, UI_FC, 2.2f, sk.ink, 0);
        lk_seg(12.0f, 134.0f, 212.0f, 134.0f, 1.0f, sk.line);
        lk_kb(&kb, 20.0f, 150.0f, 192.0f, 30.0f);
        for (i = 0; i < 3u; i++) {                  /* F3, C4, G5: where the key takes the cutoff */
            float x = ui_fx_x(ui_flt_hz(p, (float)SEMI[i], -1.0f), 12.0f, 196.0f);
            x = sy_clampf(x, 12.0f, 208.0f);
            lk_seg_dash(lk_kb_cx(&kb, KEY[i]), 148.0f, x, 138.0f, ui_hot(0) ? sk.hot : sk.line, 1.0f, 3.0f);
            lk_seg(x, 134.0f, x, 139.0f, 1.0f, sk.ink);
        }
        pk = ui_peak(y2, UI_FC);
        lk_mix(ui_v(p, SP_F_MIX));
        ui_title_p(FT_NAMES[t], 8.0f, SP_F_TYPE);
        ui_H(0, 6.0f, 160.0f);
        ui_H(1, 12.0f + (float)pk - 5.0f, y2[pk] - 16.0f);
        ui_H(2, 32.0f + 196.0f * ui_v(p, SP_F_MIX) - 5.0f, 184.0f);
        return;
    }
    ys = ui_curve(0, &fresh);                       /* TONE */
    if (fresh)
        ui_flt_curve(p, hz, 110.0f, 2.2f, 56.0f, 194.0f, ys);
    if (gh) {
        y2 = ui_curve(1, &fresh);
        if (fresh)
            ui_flt_curve(gh->p, ui_flt_hz(gh->p, 0.0f, gh->p[SP_F_VAR] > 0 ? -1.0f : 0.0f), 110.0f, 2.2f, 56.0f, 194.0f, y2);
        lk_curve(12, y2, UI_FC, 1.0f, sk.mid, LK_DASH(2, 2));
    }
    if (spread) {                                   /* the right side, thin */
        y2 = ui_curve(2, &fresh);
        if (fresh)
            ui_flt_curve(p, ui_flt_hz(p, 0.0f, 1.0f), 110.0f, 2.2f, 56.0f, 194.0f, y2);
        lk_curve(12, y2, UI_FC, 1.0f, sk.mid, 0);
    }
    lk_stip_under(12, ys, UI_FC, 190.0f, 6);
    lk_curve(12, ys, UI_FC, 2.2f, sk.ink, 0);       /* (a knob turning: its part HOT, the post or the peak's handle) */
    pk = ui_peak(ys, UI_FC);
    xc = sy_clampf(ui_fx_x(hz, 12.0f, 196.0f), 12.0f, 208.0f);
    lk_seg_dash(xc, ys[(uint32_t)sy_clampi(sy_floori(xc - 12.0f), 0, UI_FC - 1)] + 6.0f, xc, 168.0f,
                ui_hot(1) ? sk.hot : sk.ink, 2.0f, 2.0f);
    {                                               /* the cutoff's modulation: its range, from the cutoff */
        float olo, ohi, plo, phi;
        ui_mod_span(SP_F_CUT, SRC_COUNT, &olo, &ohi, &plo, &phi);
        if (ohi > olo) {
            float c = ui_v(p, SP_F_CUT);
            float xa = ui_fx_x(sp_cut_hz(sy_clampf(c + olo, 0.0f, 1.0f)), 12.0f, 196.0f);
            float xb = ui_fx_x(sp_cut_hz(sy_clampf(c + ohi, 0.0f, 1.0f)), 12.0f, 196.0f);
            lk_seg(sy_clampf(xa, 12.0f, 208.0f), 198.0f, sy_clampf(xb, 12.0f, 208.0f), 198.0f, 1.0f, sk.mid);
            lk_hollow(sy_clampf(ohi > 0.0f ? xb : xa, 12.0f, 208.0f), 198.0f, 2.5f);
        }
    }
    ui_drive_meter(ui_v(p, SP_F_DRIVE), ui_hot(3));
    ui_title_p(FT_NAMES[t], UI_TITLE_X, SP_F_TYPE);
    if (!ui_mod_word(SP_F_CUT, w))
        sp_put(w, SLOPE[t]);
    lk_text(UI_TITLE_X, 42.0f, T10, w, sk.lab, sk.ground, 20, 0);
    ui_fx_freqs(12.0f, 196.0f, 204.0f);
    ui_H(0, 8.0f, 27.0f);
    ui_H(1, xc - 5.0f, 172.0f);
    ui_H(2, 12.0f + (float)pk - 5.0f, ys[pk] - 16.0f);
    ui_H(3, 198.0f, 118.0f);
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
/* the mix row, its handle (knob k) on its knob */
static void ui_mix_row(const int16_t *p, uint32_t id, uint32_t k)
{
    float x = lk_mix(ui_v(p, id));
    ui_H(k, x - 5.0f, 184.0f);
}

/* DRIVE: the transfer curve in its box (straight through dashed), a sine through it, TONE's tilt, MIX */
static void ui_p_drive(void)
{
    uint32_t o = ui_fx_off(), i, type;
    const int16_t *p = sy_patch.p + o;
    const sy_patch_t *gh = ui_gh(ui_hot(0) || ui_hot(1));
    float ys[111], amt = ui_v(p, SP_DR_AMT), mix = ui_v(p, SP_DR_MIX), tn = (float)p[SP_DR_TONE] * 0.001f;
    type = (uint32_t)p[SP_DR_TYPE] % 4u;
    lk_frame(20.0f, 52.0f, 80.0f, 80.0f, 1.0f, sk.line);
    lk_seg_dash(20.0f, 132.0f, 100.0f, 52.0f, sk.line, 1.0f, 3.0f);
    if (gh) {
        for (i = 0; i < 81u; i++)
            ys[i] = 92.0f - 40.0f * sy_clampf(ui_drv_out((uint32_t)gh->p[SP_DR_TYPE + o] % 4u, ui_v(gh->p + o, SP_DR_AMT), mix,
                                                         (float)((int32_t)i - 40) * 0.025f), -1.0f, 1.0f);
        lk_curve(20, ys, 81u, 1.0f, sk.mid, LK_DASH(2, 2));
    }
    for (i = 0; i < 81u; i++)
        ys[i] = 92.0f - 40.0f * sy_clampf(ui_drv_out(type, amt, mix, (float)((int32_t)i - 40) * 0.025f), -1.0f, 1.0f);
    lk_curve(20, ys, 81u, 2.2f, ui_ink(ui_hot(1)), 0);
    for (i = 0; i < 111u; i++)                      /* a sine in (dashed), and out */
        ys[i] = 92.0f - 34.0f * sy_sin2pi(2.0f * (float)i / 110.0f);
    lk_curve(116, ys, 111u, 1.0f, sk.mid, LK_DASH(2, 2));
    for (i = 0; i < 111u; i++)
        ys[i] = 92.0f - 34.0f * sy_clampf(ui_drv_out(type, amt, mix, sy_sin2pi(2.0f * (float)i / 110.0f)), -1.0f, 1.0f);
    lk_curve(116, ys, 111u, 2.2f, ui_ink(ui_hot(1)), 0);
    lk_seg(116.0f, 152.0f + 6.0f * tn, 226.0f, 152.0f - 6.0f * tn, 1.5f, ui_ink(ui_hot(2)));   /* TONE's tilt */
    lk_hollow(171.0f, 152.0f, 2.5f);
    ui_mix_row(p, SP_DR_MIX, 3);
    ui_title_p(DRV_NAMES[type], UI_TITLE_X, (int32_t)(SP_DR_TYPE + ui_fx_off()));
    lk_text(52.0f, 137.0f, T9, "IN", sk.lab, sk.ground, 60, 0);
    lk_text(116.0f, 164.0f, T9, "TONE", sk.lab, sk.ground, 60, 0);
    ui_H(0, 8.0f, 27.0f);
    ui_H(1, 104.0f, 48.0f);
    ui_H(2, 212.0f, 128.0f);
}

/* ---------------------------------------------------------------------------------------- CHORUS --- */
static float ui_cho_wave(float u) { return 0.75f * sy_sin2pi(u) + 0.25f * sy_sin2pi(3.0f * u + 0.2f); }
/* CHORUS: the sound and its two copies (DEPTH apart), the taps on their line (DELAY: where; DEPTH: their swing), the
 * rate's small wave; MORE: the feedback's loop */
static void ui_p_chorus(void)
{
    uint32_t o = ui_fx_off(), i, k;
    const int16_t *p = sy_patch.p + o;
    int more = UI_PG[ui.page].id[0] == (int16_t)(SP_CH_FDBK + o);
    float dep = ui_v(p, SP_CH_DEPTH), base = 2.0f + 13.0f * ui_v(p, SP_CH_DELAY), sw = 3.0f * dep, ys[195];
    float hz = sp_lfo_hz(ui_v(p, SP_CH_RATE)), cyc = sy_clampf(hz * 3.0f, 1.0f, 8.0f);
#define UI_TX(ms) (20.0f + (ms) * (110.0f / 20.0f))
    for (k = 0; k < 2u; k++) {
        float dx = (k ? 1.0f : -1.0f) * (1.0f + 5.0f * dep), dy = (k ? 3.0f : -3.0f) * dep;
        for (i = 0; i < 195u; i++)
            ys[i] = 84.0f + dy - 20.0f * ui_cho_wave(3.0f * (float)i / 194.0f);
        lk_curve(sy_floori(20.0f + dx + 0.5f), ys, 195u, 1.0f, sk.mid, 0);
    }
    for (i = 0; i < 195u; i++)
        ys[i] = 84.0f - 20.0f * ui_cho_wave(3.0f * (float)i / 194.0f);
    lk_curve(20, ys, 195u, 2.2f, sk.ink, 0);
    lk_seg(20.0f, 146.0f, 130.0f, 146.0f, 1.0f, sk.line);  /* the line: the sound at 0, the taps */
    lk_seg(20.0f, 140.0f, 20.0f, 152.0f, 1.0f, sk.ink);
    lk_seg(UI_TX(base), 140.0f, UI_TX(base), 152.0f, 1.5f, ui_ink(ui_hot(2)));
    lk_seg(UI_TX(base + sw), 140.0f, UI_TX(base + sw), 152.0f, 1.5f, ui_ink(ui_hot(2)));
    lk_seg(UI_TX(base) - 8.0f, 134.0f, UI_TX(base + sw) + 8.0f, 134.0f, 1.0f, ui_hot(1) ? sk.hot : sk.mid);
    lk_seg(UI_TX(base) - 8.0f, 131.0f, UI_TX(base) - 8.0f, 137.0f, 1.0f, sk.mid);
    lk_seg(UI_TX(base + sw) + 8.0f, 131.0f, UI_TX(base + sw) + 8.0f, 137.0f, 1.0f, sk.mid);
    {                                               /* the feedback: from the taps back to the start */
        float lp[22], xs = UI_TX(base + sw), fb = ui_v(p, SP_CH_FDBK);
        for (i = 0; i <= 10u; i++) {
            float u = (float)i / 10.0f;
            lp[2u * i] = xs + (20.0f - xs) * u;
            lp[2u * i + 1u] = 160.0f + 12.0f * sy_sin2pi(0.5f * u);
        }
        lk_poly_dash(lp, 11u, fb > 0.01f ? 1.5f : 1.0f, more && ui_hot(0) ? sk.hot : fb > 0.01f ? sk.ink : sk.mid, 2.0f, 2.0f);
        lk_arrow(20.0f, 164.0f, 20.0f, 156.0f, 3.0f, 1.0f, sk.mid);
    }
    for (i = 0; i < 77u; i++)                       /* RATE: the taps' swing */
        ys[i] = 146.0f - 7.0f * sy_sin2pi(cyc * (float)i / 76.0f);
    lk_curve(150, ys, 77u, 1.0f, ui_hot(0) && !more ? sk.hot : sk.mid, 0);
    ui_mix_row(p, SP_CH_MIX, 3);
    if (more) {
        ui_H(0, 48.0f, 174.0f);
    } else {
        ui_H(0, 183.0f, 124.0f);
        ui_H(1, UI_TX(base + 0.5f * sw) - 5.0f, 118.0f);
        ui_H(2, UI_TX(base) - 22.0f, 130.0f);
    }
#undef UI_TX
}

/* ----------------------------------------------------------------------------------------- DELAY --- */
static float ui_dly_secs(const int16_t *p)         /* the echo's distance as fx_delay sets it (750 ms at most) */
{
    float s = p[SP_DL_SYNC] ? sy_note_q((uint32_t)p[SP_DL_NOTE]) * 60.0f / sy_tempo : sp_dly_s(ui_v(p, SP_DL_TIME));
    return sy_clampf(s, 2.0f / (0.5f * SY_FS), (float)(FX_DLY_N - 4u) / (0.5f * SY_FS));
}
/* DELAY: the echoes falling by FEEDBACK (ping-pong: left up, right down), the time between them; TONE: the band the
 * loop's filters leave (HP at the left, LP at the right) */
static void ui_p_delay(void)
{
    const int16_t *p = sy_patch.p;
    float fb = 0.95f * ui_v(p, SP_DL_FDBK), secs = ui_dly_secs(p), sp = 8.0f + 44.0f * sy_minf(secs / 0.75f, 1.0f);
    float top[24], bot[24], xl, xh;
    int pp = p[SP_DL_MODE] != 0, tone = UI_PG[ui.page].id[0] == SP_DL_LP;
    uint32_t n = 0, nt = 0, nb = 0;
    char w[24];
    lk_seg(20.0f, 120.0f, 226.0f, 120.0f, 1.0f, sk.line);
    for (n = 0; n < 12u; n++) {
        float x = 28.0f + (float)n * sp, h = 52.0f * sy_exp2f(sy_log2f(sy_maxf(fb, 1e-4f)) * (float)n);
        int up = !pp || !(n & 1u);
        if (x > 226.0f)
            break;
        if (h >= 0.5f)
            lk_seg(x, 120.0f, x, up ? 120.0f - h : 120.0f + h, n ? 1.5f : 3.0f,
                   !tone && (ui_hot(3) || (ui_hot(2) && n == 1u)) ? sk.hot : sk.ink);
        if (up) {
            top[2u * nt] = x;
            top[2u * nt++ + 1u] = 120.0f - h;
        } else {
            bot[2u * nb] = x;
            bot[2u * nb++ + 1u] = 120.0f + h;
        }
    }
    lk_poly_dash(top, nt, 1.0f, sk.mid, 2.0f, 2.0f);
    lk_poly_dash(bot, nb, 1.0f, sk.mid, 2.0f, 2.0f);
    lk_seg(28.0f, 168.0f, 28.0f, 176.0f, 1.0f, sk.mid);     /* TIME: one echo's distance */
    lk_seg(28.0f, 172.0f, 28.0f + sp, 172.0f, 1.0f, !tone && ui_hot(2) ? sk.hot : sk.mid);
    lk_seg(28.0f + sp, 168.0f, 28.0f + sp, 176.0f, 1.0f, sk.mid);
    xh = sy_clampf(ui_fx_x(sp_cut_hz(ui_v(p, SP_DL_HP)), 130.0f, 90.0f), 150.0f, 196.0f);   /* the band */
    xl = sy_clampf(ui_fx_x(sp_cut_hz(ui_v(p, SP_DL_LP)), 130.0f, 90.0f), xh + 4.0f, 200.0f);
    {
        float b[8] = {xh - 20.0f, 186.0f, xh, 166.0f, xl, 166.0f, xl + 20.0f, 186.0f};
        lk_poly(b, 4u, 1.5f, tone && (ui_hot(0) || ui_hot(1)) ? sk.hot : sk.ink);
    }
    ui_mix_row(p, SP_DL_MIX, tone ? 2u : 4u);
    if (!tone)
        pg.has &= (uint8_t)~16u;
    ui_title_p(DLY_NAMES[pp], UI_TITLE_X, SP_DL_MODE);
    if (p[SP_DL_SYNC])
        sp_put(w, NOTE_NAMES[p[SP_DL_NOTE] < (int16_t)SY_NNOTE ? p[SP_DL_NOTE] : 0]);
    else
        sp_real(w, secs * 1000.0f, 0u, " MS");
    lk_text(UI_TITLE_X, 42.0f, T10, w, sk.lab, sk.ground, 20, 0);
    if (tone) {
        ui_H(0, xl + 6.0f, 150.0f);
        ui_H(1, xh - 16.0f, 150.0f);
    } else {
        ui_H(0, 8.0f, 27.0f);
        ui_H(1, 8.0f, 43.0f);
        ui_H(2, 28.0f + 0.5f * sp - 5.0f, 176.0f);
        ui_H(3, 28.0f + 2.0f * sp + 4.0f, 120.0f - 52.0f * fb * fb - 14.0f);
    }
}

/* ---------------------------------------------------------------------------------------- REVERB --- */
static float ui_rv_rt(const int16_t *p) { return 0.3f * sy_exp2f(6.0589f * ui_v(p, SP_RV_DECAY)); }   /* RT60, s */
/* REVERB: the room (SIZE: its walls), the source and its reflections; the gap (PREDELAY), the tail (DECAY: its
 * fall, DAMP its fine grain), WIDTH (the arrows), LOW CUT (its curve) */
static void ui_p_reverb(void)
{
    const int16_t *p = sy_patch.p;
    float size = ui_v(p, SP_RV_SIZE), rt = ui_rv_rt(p), pre = ui_v(p, SP_RV_PRE), wid = ui_v(p, SP_RV_WIDTH);
    float damp = ui_v(p, SP_RV_DAMP), lc = ui_v(p, SP_RV_LOCUT), s = 0.6f + 0.4f * size, x0 = 136.0f + 10.0f * pre, x;
    float tau = 6.0f + 60.0f * sy_minf(rt / 12.0f, 1.0f), ys[78], up[42], dn[42];
    uint32_t i, seed = 5u, n = 0;
    int tone = UI_PG[ui.page].id[0] == SP_RV_DAMP;
    lk_frame(70.0f - 50.0f * s, 101.0f - 49.0f * s, 100.0f * s, 98.0f * s, 1.0f, ui_ink(!tone && ui_hot(1)));
    lk_frame(70.0f - 36.0f * s, 101.0f - 35.0f * s, 72.0f * s, 70.0f * s, 1.0f, sk.mid);
    lk_frame(70.0f - 22.0f * s, 101.0f - 21.0f * s, 44.0f * s, 42.0f * s, 1.0f, sk.line);
    lk_disc(70.0f, 101.0f, 3.0f, sk.ink);
    for (i = 0; i < 26u; i++) {
        float rx = 70.0f + ((float)(sy_rand(&seed) >> 8) * (1.0f / 16777216.0f) - 0.5f) * 92.0f * s;
        float ry = 101.0f + ((float)(sy_rand(&seed) >> 8) * (1.0f / 16777216.0f) - 0.5f) * 90.0f * s;
        if (sy_absf(rx - 70.0f) < 24.0f * s && sy_absf(ry - 101.0f) < 23.0f * s)
            continue;
        lk_disc(rx, ry, 0.9f, sk.mid);
    }
    lk_seg(132.0f, 60.0f, 132.0f, 140.0f, 1.5f, ui_ink(tone && ui_hot(1)));
    for (x = x0 + 8.0f; x <= 226.0f; x += 2.0f) {   /* the tail */
        float e = sy_exp2f(-(x - x0 - 8.0f) / tau * 1.4427f), h;
        h = e * 40.0f * (0.4f + 0.6f * (1.0f - 0.7f * damp) * (float)(sy_rand(&seed) >> 8) * (1.0f / 16777216.0f) + 0.42f * damp);
        lk_seg(x, 100.0f - h, x, 100.0f + h, 1.0f, !tone && ui_hot(2) ? sk.hot : sk.mid);
        if (n < 21u && sy_floori(x - x0 - 8.0f) % 4 == 0) {
            up[2u * n] = dn[2u * n] = x;
            up[2u * n + 1u] = 100.0f - 40.0f * e;
            dn[2u * n + 1u] = 100.0f + 40.0f * e;
            n++;
        }
    }
    lk_poly_dash(up, n, 1.0f, ui_ink(tone && ui_hot(0)), 2.0f, 2.0f);
    lk_poly_dash(dn, n, 1.0f, ui_ink(tone && ui_hot(0)), 2.0f, 2.0f);
    lk_arrow(66.0f, 166.0f, 56.0f - 20.0f * wid, 166.0f, 3.0f, 1.0f, ui_ink(tone && ui_hot(2)));   /* WIDTH */
    lk_arrow(74.0f, 166.0f, 84.0f + 20.0f * wid, 166.0f, 3.0f, 1.0f, ui_ink(tone && ui_hot(2)));
    for (i = 0; i < 77u; i++) {                     /* LOW CUT: the lows let through, from its corner */
        float u = (float)i / 76.0f - 0.6f * lc;
        ys[i] = 178.0f - 18.0f * sy_minf(1.0f, u > 0.0f ? u * u * 6.76f : 0.0f);
    }
    lk_curve(150, ys, 77u, 1.5f, ui_ink(tone && ui_hot(3)), 0);
    ui_mix_row(p, SP_RV_MIX, 3);
    if (tone)
        pg.has &= (uint8_t)~8u;
    ui_title_p(RV_NAMES[p[SP_RV_TYPE] ? 1 : 0], UI_TITLE_X, SP_RV_TYPE);
    if (tone) {
        ui_H(0, 172.0f, 62.0f);
        ui_H(1, 127.0f, 146.0f);
        ui_H(2, 64.0f, 172.0f);
        ui_H(3, 138.0f, 166.0f);
    } else {
        ui_H(0, 8.0f, 27.0f);
        ui_H(1, 70.0f + 50.0f * s - 2.0f, 101.0f - 49.0f * s - 12.0f);
        ui_H(2, 214.0f, 62.0f);
    }
}

/* ------------------------------------------------------------------------------------------- OTT --- */
/* OTT: the three bands (each its level in, live while sound plays; the thresholds dashed: down above, up below; the
 * up and down pushes as arrows), DEPTH (the fader), TIME; BANDS: each band's gain */
static void ui_p_ott(void)
{
    static const char *const NAME[3] = {"LOW", "MID", "HIGH"};
    uint32_t o = ui_fx_off(), b;
    const int16_t *p = sy_patch.p + o;
    int bands = UI_PG[ui.page].id[0] == (int16_t)(SP_OT_LOW + o);
    float up = ui_v(p, SP_OT_UP), dn = ui_v(p, SP_OT_DOWN), tm = ui_v(p, SP_OT_TIME), y;
    if (!bands)
        ui_live(48.0f, 172.0f);                     /* (the levels, live) */
    for (b = 0; b < 3u; b++) {
        float cx = 56.0f + (float)(b * 56u), lv;
        if (bands) {
            lv = 0.5f + (float)p[SP_OT_LOW + b] * (0.5f / 240.0f);
        } else {
            float in = 6.0206f * sy_log2f(sy_maxf(fx.ott[ui_fx_nth()].env[b], 1e-6f));
            lv = sy_clampf((in + 60.0f) / 60.0f, 0.0f, 1.0f);
        }
        lk_frame(cx - 17.0f, 50.0f, 34.0f, 120.0f, 1.0f, sk.line);
        lk_seg_dash(cx - 17.0f, 80.0f, cx + 17.0f, 80.0f, sk.mid, 2.0f, 2.0f);
        lk_seg_dash(cx - 17.0f, 140.0f, cx + 17.0f, 140.0f, sk.mid, 2.0f, 2.0f);
        lk_box(cx - 4.0f, 170.0f - 120.0f * lv, cx + 4.0f, 170.0f, bands && ui_hot(b) ? sk.hot : sk.ink);
        lk_arrow(cx - 11.0f, 56.0f, cx - 11.0f, 58.0f + 18.0f * dn, 3.0f, 1.5f, ui_ink(!bands && ui_hot(3)));
        lk_arrow(cx + 11.0f, 164.0f, cx + 11.0f, 162.0f - 18.0f * up, 3.0f, 1.5f, ui_ink(!bands && ui_hot(2)));
        lk_text_c(cx, 174.0f, T9, NAME[b], sk.lab, sk.ground, 60, 0);
        if (bands)
            ui_H(b, cx - 5.0f, 36.0f);
    }
    y = lk_fader(214.0f, 50.0f, 170.0f, ui_v(p, SP_OT_DEPTH));
    {                                               /* TIME: the attack, quick or slow */
        float tp[6] = {188.0f, 42.0f, 188.0f + 4.0f + 8.0f * tm, 24.0f, 228.0f, 40.0f};
        lk_poly(tp, 3u, 1.5f, ui_ink(!bands && ui_hot(1)));
    }
    if (!bands) {
        ui_H(0, 192.0f, y - 5.0f);
        ui_H(1, 172.0f, 26.0f);
        ui_H(2, 22.0f, 136.0f);
        ui_H(3, 22.0f, 72.0f);
    }
}

/* -------------------------------------------------------------------------------------------- EQ --- */
/* the EQ's nine values: each point's gain, frequency and Q (LOW, MID, HIGH) */
static const uint8_t UI_EQ_ID[9] = {SP_EQ_LOW, SP_EQ_MID, SP_EQ_HIGH, SP_EQ_LFREQ, SP_EQ_MFREQ, SP_EQ_HFREQ,
                                    SP_EQ_LQ, SP_EQ_Q, SP_EQ_HQ};
/* the nine as 0..1, into v: of values p (live: as the audio side has them now, smoothed, modulated) */
static void ui_eq_vals(const int16_t *p, int live, float *v)
{
    uint32_t k;
    for (k = 0; k < 9u; k++)
        v[k] = live ? mod_get(sy_goff, UI_EQ_ID[k] + ui_fx_off()) : ui_v(p, UI_EQ_ID[k]);
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
/* EQ: its curve hatched to 0 dB, its three points (on POINTS the one picked filled, its moves as arrows and Q's width
 * as a bracket); while sound plays and something modulates it, where the modulation has it now */
static void ui_p_eq(void)
{
    uint32_t o = ui_fx_off(), k, i;
    const int16_t *p = sy_patch.p + o;
    int points = UI_PG[ui.page].id[0] == UI_EQPT, live = ui_sounding() && ui_eq_modded();
    uint32_t pick = ui.eqpt < 3u ? ui.eqpt : 1u;
    const sy_patch_t *gh = ui_gh(ui_turning() >= 0);
    float v[9], *ys, px[3], py[3];
    int fresh;
#define UI_EQY(db) sy_clampf(110.0f - (db) * 3.0f, 52.0f, 192.0f)
    if (live)
        ui_live(40.0f, 196.0f);                     /* (the curve where the modulation has it now) */
    ui_eq_vals(p, live, v);
    ys = ui_curve(0, &fresh);
    for (i = 0; fresh && i <= 216u; i++)
        ys[i] = UI_EQY(ui_eq_db(v, ui_fx_f(12.0f + (float)i, 12.0f, 216.0f)));
    lk_seg_dash(12.0f, 110.0f, 228.0f, 110.0f, sk.line, 1.0f, 3.0f);
    for (i = 0; i <= 216u; i += 4u)
        if (sy_absf(ys[i] - 110.0f) > 1.0f)
            lk_seg(12.0f + (float)i, 110.0f, 12.0f + (float)i, ys[i], 1.0f, sk.line);
    if (gh) {
        float g[9], *y2 = ui_curve(1, &fresh);
        ui_eq_vals(gh->p + o, 0, g);
        for (i = 0; fresh && i <= 216u; i++)
            y2[i] = UI_EQY(ui_eq_db(g, ui_fx_f(12.0f + (float)i, 12.0f, 216.0f)));
        lk_curve(12, y2, 217u, 1.0f, sk.mid, LK_DASH(2, 2));
    }
    lk_curve(12, ys, 217u, 2.2f, sk.ink, 0);
    for (k = 0; k < 3u; k++) {
        px[k] = sy_clampf(ui_fx_x(sp_cut_hz(v[3u + k]), 12.0f, 216.0f), 14.0f, 226.0f);
        py[k] = UI_EQY(ui_eq_gain(v, k));
        if (points && k == pick)
            lk_box(px[k] - 3.5f, py[k] - 3.5f, px[k] + 3.5f, py[k] + 3.5f, ui_ink(ui_turning() >= 1));
        else
            lk_hollow_sq(px[k], py[k], 2.5f);
        if (!points)
            ui_H(k, px[k] - 5.0f, k == 1u ? py[k] + 10.0f : py[k] - 16.0f);
    }
    if (points) {                                   /* the point picked: FREQ, GAIN, Q */
        float x = px[pick], qw = 8.0f + 24.0f * (1.0f - v[6u + pick]);
        lk_arrow(x - 4.0f, 150.0f, x - 24.0f, 150.0f, 3.0f, 1.0f, ui_hot(1) ? sk.hot : sk.mid);
        lk_arrow(x + 4.0f, 150.0f, x + 24.0f, 150.0f, 3.0f, 1.0f, ui_hot(1) ? sk.hot : sk.mid);
        lk_arrow(x + 14.0f, 124.0f, x + 14.0f, 104.0f, 3.0f, 1.0f, ui_hot(2) ? sk.hot : sk.mid);
        lk_seg(x - qw, 162.0f, x - qw, 170.0f, 1.0f, sk.mid);
        lk_seg(x - qw, 166.0f, x + qw, 166.0f, 1.0f, ui_hot(3) ? sk.hot : sk.mid);
        lk_seg(x + qw, 162.0f, x + qw, 170.0f, 1.0f, sk.mid);
        ui_H(0, x - 5.0f, py[pick] - 18.0f);
        ui_H(1, x + 27.0f, 145.0f);
        ui_H(2, x + 20.0f, 96.0f);
        ui_H(3, x + qw + 4.0f, 161.0f);
    }
    ui_fx_freqs(12.0f, 216.0f, 200.0f);
#undef UI_EQY
}

/* ------------------------------------------------------------------------------------ the chain view --- */
/* the modules on a cable in the order they play: eight places, a row left to right, the next right to left; each its
 * place, its name, its glyph, its mix; the one picked filled; a bypassed one dashed; the place to add one */
static const char *const UI_CHAIN_NAME[FXT_N] = {"DRIVE", "CHORUS", "DELAY", "REVERB", "OTT", "EQ", "FLANGER",
                                                  "PHASER", "FILTER", "CRUSH", "TREMOLO", "RING", "TAPE"};   /* (a box's) */
static void ui_p_chain(void)
{
    static const float POS[NFXC][2] = {{14, 48}, {68, 48}, {122, 48}, {176, 48}, {176, 116}, {122, 116}, {68, 116},
                                       {14, 116}};
    uint32_t nc = sp_chain_n(sy_patch.p), at = ui_cslot(), k, used = 0;
    char t[32];
    float tw, x;
    for (k = 0; k < nc + (nc < NFXC) && k < NFXC; k++) {
        float bx = POS[k][0], by = POS[k][1];
        int32_t in = sp_chain_at(sy_patch.p, k), mx = in >= 0 ? ui_cmix_of(in) : -1;
        int pick = k == at, on = in >= 0 && sy_patch.p[FXI_ON[in]];
        uint16_t bgc = pick ? (ui_hot(1) || ui_hot(2) ? sk.hot : sk.ink) : sk.ground, fg = pick ? sk.ground : sk.ink;
        if (pick)
            lk_box(bx, by, bx + 46.0f, by + 40.0f, bgc);
        else if (in < 0 || !on)
            lk_frame_dash(bx + 0.5f, by + 0.5f, 45.0f, 39.0f, 2.0f, 2.0f, sk.mid);
        else
            lk_frame(bx + 0.5f, by + 0.5f, 45.0f, 39.0f, 1.2f, sk.ink);
        sp_fix(t, (int32_t)k + 1, 0u, 0);
        lk_text(bx + 5.0f, by + 2.0f, T9, t, pick ? sk.ground : sk.lab, bgc, 0, 0);
        if (in < 0) {
            lk_text(bx + 5.0f, by + 16.0f, T9B, "+ HERE", pick ? sk.ground : sk.lab, bgc, 0, 0);
            continue;
        }
        lk_text(bx + 5.0f, by + 16.0f, T9B, lk_fit(t, sizeof t, UI_CHAIN_NAME[FXI_TYPE[in]], T9B, 0, 40.0f),
                pick ? sk.ground : on ? sk.ink : sk.lab, bgc, 0, 0);
        lk_icon(FXI_TYPE[in], bx + 32.0f, by + 5.0f, pick ? sk.ground : on ? sk.ink : sk.mid);
        if (mx >= 0) {
            float m = ui_v(sy_patch.p, (uint32_t)mx);
            lk_seg(bx + 5.0f, by + 33.0f, bx + 41.0f, by + 33.0f, 1.0f, pick ? sk.mid : sk.line);
            if (m > 0.01f)
                lk_seg(bx + 5.0f, by + 33.0f, bx + 5.0f + sy_maxf(1.0f, m * 36.0f), by + 33.0f, 3.0f,
                       pick ? sk.ground : on ? (ui_hot(3) && pick ? sk.hot : sk.ink) : sk.mid);
        }
        (void)fg;
    }
    {                                               /* the cable between them, the sound's way */
        static const float J[NFXC - 1u][4] = {{60, 68, 68, 68}, {114, 68, 122, 68}, {168, 68, 176, 68}, {0, 0, 0, 0},
                                              {176, 136, 168, 136}, {122, 136, 114, 136}, {68, 136, 60, 136}};
        uint32_t last = nc + (nc < NFXC) - 1u;
        lk_seg(4.0f, 68.0f, 14.0f, 68.0f, 1.0f, sk.mid);
        for (k = 0; k < last && k < NFXC - 1u; k++) {
            if (k == 3u) {                          /* (the turn at the right) */
                float c[12] = {222, 68, 226, 68, 229.5f, 70, 230, 72, 230, 132, 222, 136};
                lk_poly(c, 6u, 1.0f, sk.mid);
                lk_disc(230.0f, 102.0f, 2.0f, sk.hot);
                continue;
            }
            lk_seg(J[k][0], J[k][1], J[k][2], J[k][3], 1.0f, sk.mid);
            if (k == 0u || k == 1u || k == 5u)
                lk_disc(0.5f * (J[k][0] + J[k][2]), J[k][1], 2.0f, sk.hot);
        }
    }
    for (k = 0; k < 2u; k++)                        /* the type to add: its instances in the chain */
        used += FXT_INST[ui.ctype][k] != 0xFF && sp_chain_find(sy_patch.p, FXT_INST[ui.ctype][k]) >= 0;
    lk_text(8.0f, 22.0f, T10, "ADD", sk.lab, sk.ground, 20, 0);
    tw = lk_tw(T12B, FXT_NAMES[ui.ctype % FXT_N], 0) + 12.0f;
    lk_frame(34.5f, 19.5f, tw - 1.0f, 20.0f, 1.0f, ui_ink(ui_hot(0)));
    lk_text(40.5f, 22.0f, T12B, FXT_NAMES[ui.ctype % FXT_N], ui_ink(ui_hot(0)), sk.ground, 0, 15);
    sp_put(sp_fix(sp_put(sp_fix(t, (int32_t)used, 0u, 0), " OF "), FXT_INST[ui.ctype][1] == 0xFF ? 1 : 2, 0u, 0), " IN");
    x = lk_text(40.0f + tw, 24.0f, T9, t, sk.lab, sk.ground, 20, 0);
    {                                               /* the module picked, in words */
        int32_t in = at < nc ? sp_chain_at(sy_patch.p, at) : -1;
        char *e = sp_put(sp_fix(t, (int32_t)(at < NFXC ? at : 0u) + 1, 0u, 0), "  ");
        if (in < 0)
            sp_put(e, "EMPTY: FX+OCT+ ADDS HERE");
        else
            sp_put(sp_put(e, FXT_NAMES[FXI_TYPE[in]]), sy_patch.p[FXI_ON[in]] ? "" : " \xB7 BYPASSED");
        lk_text(8.0f, 186.0f, T10B, t, sk.ink, sk.ground, 0, 0);
    }
    {
        float bx = POS[at < NFXC ? at : 0][0], by = POS[at < NFXC ? at : 0][1];
        ui_H(0, x + 7.0f, 23.0f);
        ui_H(1, bx - 12.0f, by - 6.0f);
        if (at < nc) {
            ui_H(2, bx - 2.0f, by + 47.0f);
            ui_H(3, bx + 30.0f, by + 47.0f);
        }
    }
}

/* ----------------------------------------------------------------------------- the modulations --- */
/* the response in dB at f of a comb (d samples, feedback fb, mix m: the flanger's tap at rest at d) */
static float ui_comb_db(float d, float fb, float m, float f)
{
    float a = f * d * (1.0f / SY_FS), c = sy_cos2pi(a), s = sy_sin2pi(a);
    ui_cx e = {c, -s}, den, w;
    den.r = 1.0f - fb * e.r;
    den.i = -fb * e.i;
    w = cx_div(e, den);
    w.r = 1.0f + (w.r - 1.0f) * m;                  /* out = x + (wet - x) mix */
    w.i *= m;
    return 3.0103f * sy_log2f(w.r * w.r + w.i * w.i + 1e-12f);
}
/* the sound in, at the top left (the rate's small wave beside it in its chips' place) */
static void ui_fx_in(int hot)
{
    float ys[71];
    uint32_t i;
    for (i = 0; i < 71u; i++)
        ys[i] = 34.0f - 6.0f * sy_sin2pi(2.0f * (float)i / 70.0f);
    lk_curve(20, ys, 71u, 1.0f, hot ? sk.hot : sk.mid, 0);
}
/* FLANGER: the comb at its sweep's middle (SPREAD: the right side's dashed), hatched, across 0 .. 3 kHz (linear: its
 * teeth evenly spaced, as many as DELAY puts there); the sweep's width (DEPTH, the arrows), the comb's spacing
 * (DELAY), SYNC's chips, MIX */
#define UI_FLF(i) ((float)(i) * (3000.0f / 216.0f)) /* column i's frequency */
static void ui_p_flanger(void)
{
    uint32_t o = ui_fx_off(), i;
    const int16_t *p = sy_patch.p + o;
    int more = UI_PG[ui.page].id[0] == (int16_t)(SP_FL_FDBK + o);
    float lo = (0.3f + 4.7f * ui_v(p, SP_FL_DELAY)) * 0.001f * SY_FS, sw = 5.0f * ui_v(p, SP_FL_DEPTH) * 0.001f * SY_FS;
    float fb = 0.95f * (float)p[SP_FL_FDBK] * 0.001f, m = ui_v(p, SP_FL_MIX), spr = ui_v(p, SP_FL_SPREAD), *ys, dw;
    int fresh;
    char w[16];
#define UI_CY(db) (166.0f - 48.0f * sy_minf(sy_exp2f((db) * 0.1660964f), 2.0f))   /* (its magnitude, linear: round */
                                                                                      /* teeth, sharp notches) */
    ui_fx_in(!more && ui_hot(0));
    dw = 6.0f + 34.0f * ui_v(p, SP_FL_DEPTH);
    lk_arrow(150.0f, 52.0f, 150.0f - dw, 52.0f, 3.0f, 1.0f, ui_ink(!more && ui_hot(1)));
    lk_arrow(160.0f, 52.0f, 160.0f + dw, 52.0f, 3.0f, 1.0f, ui_ink(!more && ui_hot(1)));
    ys = ui_curve(0, &fresh);
    for (i = 0; fresh && i <= 216u; i++)
        ys[i] = UI_CY(ui_comb_db(lo + 0.5f * sw * (1.0f + spr * 0.6f), fb, m, UI_FLF(i)));
    lk_curve(12, ys, 217u, 1.0f, more && ui_hot(1) ? sk.hot : sk.mid, LK_DASH(2, 2));
    ys = ui_curve(1, &fresh);
    for (i = 0; fresh && i <= 216u; i++)
        ys[i] = UI_CY(ui_comb_db(lo + 0.5f * sw, fb, m, UI_FLF(i)));
    for (i = 0; i <= 216u; i += 4u)
        lk_seg(12.0f + (float)i, ys[i], 12.0f + (float)i, 166.0f, 1.0f, sk.line);
    lk_curve(12, ys, 217u, 2.2f, more && ui_hot(0) ? sk.hot : sk.ink, 0);
    dw = 10.0f + 26.0f * ui_v(p, SP_FL_DELAY);
    lk_seg(30.0f, 168.0f, 30.0f, 176.0f, 1.0f, sk.mid);
    lk_seg(30.0f, 172.0f, 30.0f + dw, 172.0f, 1.0f, !more && ui_hot(2) ? sk.hot : sk.mid);
    lk_seg(30.0f + dw, 168.0f, 30.0f + dw, 176.0f, 1.0f, sk.mid);
    ui_mix_row(p, SP_FL_MIX, 3);
    sp_put(w, NOTE_NAMES[p[SP_FL_NOTE] < (int16_t)SY_NNOTE ? p[SP_FL_NOTE] : 0]);
    lk_chip(lk_chip(150.0f, 22.0f, "FREE", !p[SP_FL_SYNC]), 22.0f, w, p[SP_FL_SYNC] != 0);
    if (more) {
        pg.has &= (uint8_t)~8u;
        ui_H(0, 43.0f, ys[0] - 16.0f > 60.0f ? 52.0f : 52.0f);
        ui_H(1, 176.0f, 54.0f);
        ui_H(2, 136.0f, 23.0f);
    } else {
        ui_H(0, 94.0f, 29.0f);
        ui_H(1, 160.0f + dw * 0.0f + 6.0f + 34.0f * ui_v(p, SP_FL_DEPTH) + 4.0f, 47.0f);
        ui_H(2, 30.0f + 0.5f * dw - 5.0f, 178.0f);
    }
#undef UI_CY
}

/* PHASER: its stages' and the dry's response at the sweep's middle (SPREAD: the right side's dashed), hatched; the
 * centre (the post), the sweep (DEPTH), the stages (the dots), SYNC's chips, MIX */
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
static void ui_p_phaser(void)
{
    static const uint8_t NST[6] = {2, 4, 6, 8, 10, 12};
    uint32_t o = ui_fx_off(), i, ns = NST[sy_clampi(sy_patch.p[SP_PH_STAGES + o], 0, 5)];
    const int16_t *p = sy_patch.p + o;
    int more = UI_PG[ui.page].id[0] == (int16_t)(SP_PH_FDBK + o);
    float cen = sp_cut_hz(ui_v(p, SP_PH_CENTER)), oct = 2.5f * ui_v(p, SP_PH_DEPTH), spr = ui_v(p, SP_PH_SPREAD);
    float fb = 0.9f * (float)p[SP_PH_FDBK] * 0.001f, m = ui_v(p, SP_PH_MIX), *ys, cx, x0, x1;
    int fresh;
    char w[16];
#define UI_PY(db) sy_clampf(120.0f - (db) * 2.4f, 66.0f, 166.0f)
    ui_fx_in(!more && ui_hot(0));
    for (i = 0; i < 12u; i++) {                     /* the stages */
        float dx = 134.0f + (float)(i * 7u);
        if (i < ns)
            lk_disc(dx, 27.0f, 2.0f, ui_ink(more && ui_hot(1)));
        else
            lk_ring(dx, 27.0f, 2.0f, 1.0f, sk.mid);
    }
    ys = ui_curve(0, &fresh);
    for (i = 0; fresh && i <= 216u; i++)
        ys[i] = UI_PY(ui_phs_db(cen * sy_exp2f(spr * 0.7f), ns, fb, m, ui_fx_f(12.0f + (float)i, 12.0f, 216.0f)));
    lk_curve(12, ys, 217u, 1.0f, more && ui_hot(2) ? sk.hot : sk.mid, LK_DASH(2, 2));
    ys = ui_curve(1, &fresh);
    for (i = 0; fresh && i <= 216u; i++)
        ys[i] = UI_PY(ui_phs_db(cen, ns, fb, m, ui_fx_f(12.0f + (float)i, 12.0f, 216.0f)));
    for (i = 0; i <= 216u; i += 4u)
        lk_seg(12.0f + (float)i, ys[i] + 3.0f, 12.0f + (float)i, 166.0f, 1.0f, sk.line);
    lk_curve(12, ys, 217u, 2.2f, more && ui_hot(0) ? sk.hot : sk.ink, 0);
    cx = sy_clampf(ui_fx_x(cen, 12.0f, 216.0f), 14.0f, 226.0f);
    lk_seg_dash(cx, 54.0f, cx, 160.0f, ui_ink(!more && ui_hot(2)), 2.0f, 2.0f);
    x0 = sy_clampf(ui_fx_x(cen * sy_exp2f(-oct), 12.0f, 216.0f), 12.0f, 228.0f);
    x1 = sy_clampf(ui_fx_x(cen * sy_exp2f(oct), 12.0f, 216.0f), 12.0f, 228.0f);
    lk_seg(x0, 164.0f, x0, 172.0f, 1.0f, sk.mid);
    lk_seg(x0, 168.0f, x1, 168.0f, 1.0f, !more && ui_hot(1) ? sk.hot : sk.mid);
    lk_seg(x1, 164.0f, x1, 172.0f, 1.0f, sk.mid);
    ui_mix_row(p, SP_PH_MIX, 3);
    sp_put(w, NOTE_NAMES[p[SP_PH_NOTE] < (int16_t)SY_NNOTE ? p[SP_PH_NOTE] : 0]);
    lk_chip(lk_chip(26.0f, 48.0f, "FREE", !p[SP_PH_SYNC]), 48.0f, w, p[SP_PH_SYNC] != 0);
    if (more) {
        uint32_t t = ui_peak(ys, 217u);
        pg.has &= (uint8_t)~8u;
        ui_H(0, 12.0f + (float)t - 5.0f, ys[t] - 16.0f);
        ui_H(1, 118.0f, 22.0f);
        ui_H(2, 12.0f + 216.0f * 0.71f - 5.0f, 150.0f);
        ui_H(3, 8.0f, 49.0f);
    } else {
        ui_H(0, 94.0f, 29.0f);
        ui_H(1, x1 + 4.0f, 163.0f);
        ui_H(2, cx - 5.0f, 40.0f);
    }
#undef UI_PY
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
/* FX FILTER: its type on a ruler (FF_COUNT of them), its response stippled under, the cutoff's post, DRIVE, MIX */
static void ui_p_ffilt(void)
{
    uint32_t o = ui_fx_off(), i, t = (uint32_t)sy_clampi(sy_patch.p[SP_FF_TYPE + o], 0, FF_COUNT - 1), pk;
    const int16_t *p = sy_patch.p + o;
    int more = UI_PG[ui.page].id[0] == (int16_t)(SP_FF_DRIVE + o);
    float hz = sp_cut_hz(ui_v(p, SP_FF_CUT)), res = ui_v(p, SP_FF_RES), mix = ui_v(p, SP_FF_MIX), *ys, xc;
    const sy_patch_t *gh = ui_gh(!more && (ui_hot(0) || ui_hot(1) || ui_hot(2)));
    int fresh;
    for (i = 0; i < FF_COUNT && i < 22u; i++) {
        float x = 100.0f + (float)(i * 6u);
        if (i == t)
            lk_seg(x, 24.0f, x, 40.0f, 1.5f, ui_ink(!more && ui_hot(0)));
        else
            lk_seg(x, 28.0f, x, 36.0f, 1.0f, sk.line);
    }
#define UI_FY2(db) sy_clampf(116.0f - (db) * 2.0f, 50.0f, 184.0f)
    if (gh) {
        uint32_t gt = (uint32_t)sy_clampi(gh->p[SP_FF_TYPE + o], 0, FF_COUNT - 1);
        ys = ui_curve(1, &fresh);
        for (i = 0; fresh && i < UI_FC; i++)
            ys[i] = UI_FY2(gt == FF_RING ? 0.0f : ui_ffx_db(gt, sp_cut_hz(ui_v(gh->p + o, SP_FF_CUT)), ui_v(gh->p + o, SP_FF_RES),
                                                              mix, ui_fx_f(12.0f + (float)i, 12.0f, 196.0f)));
        lk_curve(12, ys, UI_FC, 1.0f, sk.mid, LK_DASH(2, 2));
    }
    ys = ui_curve(0, &fresh);
    for (i = 0; fresh && i < UI_FC; i++)
        ys[i] = UI_FY2(t == FF_RING ? 0.0f : ui_ffx_db(t, hz, res, mix, ui_fx_f(12.0f + (float)i, 12.0f, 196.0f)));
    lk_stip_under(12, ys, UI_FC, 180.0f, 6);
    lk_curve(12, ys, UI_FC, 2.2f, !more && (ui_hot(1) || ui_hot(2)) ? sk.hot : sk.ink, 0);
    pk = ui_peak(ys, UI_FC);
    xc = sy_clampf(ui_fx_x(hz, 12.0f, 196.0f), 12.0f, 208.0f);
    lk_seg_dash(xc, ys[(uint32_t)sy_clampi(sy_floori(xc - 12.0f), 0, UI_FC - 1)] + 6.0f, xc, 170.0f, sk.ink, 2.0f, 2.0f);
    ui_drive_meter(ui_v(p, SP_FF_DRIVE), more && ui_hot(0));
    ui_mix_row(p, SP_FF_MIX, 3);
    ui_title_p(FF_NAMES[t], UI_TITLE_X, (int32_t)(SP_FF_TYPE + ui_fx_off()));
    if (more) {
        pg.has &= (uint8_t)~8u;
        ui_H(0, 198.0f, 118.0f);
    } else {
        ui_H(0, 8.0f, 27.0f);
        ui_H(1, xc - 15.0f, 160.0f);
        ui_H(2, 12.0f + (float)pk - 5.0f, ys[pk] - 16.0f);
    }
#undef UI_FY2
}

/* BITCRUSH: a sine (dashed) as it plays through: its holds (RATE: the ticks under) and its levels (BITS: at the right) */
static void ui_p_crush(void)
{
    uint32_t o = ui_fx_off(), i, bits = (uint32_t)sy_clampi(sy_patch.p[SP_CR_BITS + o], 1, 16);
    const int16_t *p = sy_patch.p + o;
    float step = sp_cut_hz(ui_v(p, SP_CR_RATE)) * (1.0f / SY_FS), qs = sy_exp2f(1.0f - (float)bits), m = ui_v(p, SP_CR_MIX);
    float ys[213], ph = 1.0f, hold = 0.0f, nl;
    for (i = 0; i <= 212u; i++)                     /* (150 samples across: 1.41 px each, a 441 Hz sine) */
        ys[i] = 110.0f - 56.0f * sy_sin2pi(1.5f * (float)i / 212.0f);
    lk_curve(14, ys, 213u, 1.0f, sk.mid, LK_DASH(2, 2));
    for (i = 0; i <= 212u; i++) {
        float x = sy_sin2pi(1.5f * (float)i / 212.0f), s0 = (float)i / 1.413f, s1 = (float)(i + 1u) / 1.413f;
        int32_t k;
        for (k = sy_floori(s0); k < sy_floori(s1); k++) {
            ph += step;
            if (ph >= 1.0f) {
                ph -= (float)sy_floori(ph);
                hold = sy_clampf((float)sy_floori(x / qs + 0.5f) * qs, -1.0f, 1.0f);
                if (i % 2u == 0u)
                    lk_seg(14.0f + (float)i, 176.0f, 14.0f + (float)i, 180.0f, 1.0f, ui_hot(1) ? sk.hot : sk.line);
            }
        }
        ys[i] = 110.0f - 56.0f * (x + (hold - x) * m);
    }
    lk_curve(14, ys, 213u, 2.2f, ui_ink(ui_hot(0) || ui_hot(1)), 0);
    nl = sy_minf((float)(1u << (bits < 4u ? bits : 4u)), 16.0f);   /* the levels (four bits and more: sixteen shown) */
    for (i = 0; i <= (uint32_t)nl; i++) {
        float y = 110.0f - 56.0f * (2.0f * (float)i / nl - 1.0f);
        lk_seg(230.0f, y, 234.0f, y, 1.0f, ui_hot(0) ? sk.hot : sk.mid);
    }
    ui_mix_row(p, SP_CR_MIX, 2);
    ui_H(0, 216.0f, 36.0f);
    ui_H(1, 20.0f, 183.0f);
}

/* TREMOLO: each side's level over the rate's cycles (AUTOPAN: L and R against each other), a cycle bracketed */
static void ui_p_trem(void)
{
    uint32_t o = ui_fx_off(), i, c, shape = (uint32_t)sy_clampi(sy_patch.p[SP_TR_SHAPE + o], 0, 3);
    const int16_t *p = sy_patch.p + o;
    int pan = p[SP_TR_MODE] != 0, more = UI_PG[ui.page].id[0] == (int16_t)(SP_TR_SYNC + o);
    float dep = ui_v(p, SP_TR_DEPTH), hz, cyc, ys[207], e1[207], e2[207];
    char w[16];
    {
        float s = p[SP_TR_SYNC] ? sy_note_q((uint32_t)p[SP_TR_NOTE]) * 60.0f / sy_tempo : 1.0f / sp_lfo_hz(ui_v(p, SP_TR_RATE));
        hz = 1.0f / sy_maxf(s, 0.01f);
    }
    cyc = sy_clampf(hz * 0.5f, 1.0f, 6.0f);
    for (c = 0; c < 2u; c++) {
        float y0 = c ? 150.0f : 84.0f, l = c && pan ? -1.0f : 1.0f;
        for (i = 0; i < 207u; i++) {
            float u = (float)i / 206.0f, v = fx_trm_lfo(shape, cyc * u - (float)sy_floori(cyc * u)), a;
            a = 1.0f - dep * (0.5f + 0.5f * l * v);
            ys[i] = y0 - 22.0f * a * sy_sin2pi(24.0f * u);
            e1[i] = y0 - 22.0f * a;
            e2[i] = y0 + 22.0f * a;
        }
        lk_curve(20, e1, 207u, 1.0f, sk.mid, LK_DASH(2, 2));
        lk_curve(20, e2, 207u, 1.0f, sk.mid, LK_DASH(2, 2));
        lk_curve(20, ys, 207u, 1.5f, ui_ink(ui_hot(3)), 0);
    }
    lk_text(8.0f, 80.0f, T9, "L", sk.lab, sk.ground, 0, 0);
    lk_text(8.0f, 146.0f, T9, "R", sk.lab, sk.ground, 0, 0);
    {
        float x1 = 20.0f + 206.0f / cyc;
        lk_seg(20.0f, 113.0f, 20.0f, 121.0f, 1.0f, sk.mid);
        lk_seg(20.0f, 117.0f, x1, 117.0f, 1.0f, ui_hot(2) && !more ? sk.hot : sk.mid);
        lk_seg(x1, 113.0f, x1, 121.0f, 1.0f, sk.mid);
        if (!more)
            ui_H(2, x1 + 4.0f, 112.0f);
    }
    ui_title_p(TRM_MODES[pan], UI_TITLE_X, (int32_t)(SP_TR_MODE + ui_fx_off()));
    sp_put(w, NOTE_NAMES[p[SP_TR_NOTE] < (int16_t)SY_NNOTE ? p[SP_TR_NOTE] : 0]);
    lk_chip(lk_chip(150.0f, 24.0f, "FREE", !p[SP_TR_SYNC]), 24.0f, w, p[SP_TR_SYNC] != 0);
    if (more) {
        ui_H(0, 136.0f, 25.0f);
    } else {
        ui_H(0, 8.0f, 27.0f);
        ui_H(1, 92.0f, 46.0f);
        ui_H(3, 40.0f, 46.0f);
    }
}

/* RING MOD: the sound in, times the carrier (FREQ: its cycles, SHAPE sine to square, SPREAD: the right side's
 * dashed), equals what comes out */
static void ui_p_ring(void)
{
    uint32_t o = ui_fx_off(), i;
    const int16_t *p = sy_patch.p + o;
    float shape = ui_v(p, SP_RG_SHAPE), spr = ui_v(p, SP_RG_SPREAD), cyc = 3.0f + 12.0f * ui_v(p, SP_RG_FREQ), ys[185];
#define UI_CAR(u, ph) (lk_tanh((1.0f + 9.0f * shape) * sy_sin2pi(cyc * (u) + (ph))) / lk_tanh(1.0f + 9.0f * shape))
    for (i = 0; i < 185u; i++)
        ys[i] = 56.0f - 14.0f * ui_cho_wave(3.0f * (float)i / 184.0f);
    lk_curve(26, ys, 185u, 1.0f, sk.ink, 0);
    for (i = 0; i < 185u; i++)
        ys[i] = 100.0f - 14.0f * UI_CAR((float)i / 184.0f, 0.12f * spr);
    lk_curve(26, ys, 185u, 1.0f, ui_hot(2) ? sk.hot : sk.mid, LK_DASH(2, 2));
    for (i = 0; i < 185u; i++)
        ys[i] = 100.0f - 14.0f * UI_CAR((float)i / 184.0f, 0.0f);
    lk_curve(26, ys, 185u, 1.5f, ui_ink(ui_hot(0) || ui_hot(1)), 0);
    for (i = 0; i < 185u; i++)
        ys[i] = 150.0f - 26.0f * ui_cho_wave(3.0f * (float)i / 184.0f) * UI_CAR((float)i / 184.0f, 0.0f);
    lk_curve(26, ys, 185u, 2.2f, sk.ink, 0);
#undef UI_CAR
    lk_text(11.0f, 94.0f, T12B, "\xD7", sk.lab, sk.ground, 0, 0);
    lk_text(11.0f, 144.0f, T12B, "=", sk.lab, sk.ground, 0, 0);
    ui_mix_row(p, SP_RG_MIX, 3);
    ui_H(0, 214.0f, 95.0f);
    ui_H(1, 100.0f, 73.0f);
    ui_H(2, 160.0f, 116.0f);
}

/* TAPE: the reels (turning while sound plays), the tape between them wobbling (WOW slow, FLUTTER quick), the heads,
 * SATURATE's curve, TONE's tilt, MIX */
static float ui_reel_a;                             /* the reels' turn */
static void ui_reel(float cx, float cy, float a)
{
    uint32_t i;
    lk_ring(cx, cy, 28.0f, 1.5f, sk.ink);
    lk_ring(cx, cy, 6.0f, 1.5f, sk.ink);
    for (i = 0; i < 3u; i++) {
        float t = a - 0.25f + (float)i / 3.0f;
        lk_ring(cx + 15.0f * sy_cos2pi(t), cy + 15.0f * sy_sin2pi(t), 5.0f, 1.5f, sk.ink);
    }
}
static void ui_tape_seg(float xa, float ya, float xb, float yb, float ph, float wow, float flt, uint16_t c)
{
    float pts[82], dx = xb - xa, dy = yb - ya, l = sy_sqrtf(dx * dx + dy * dy), nx = -dy / l, ny = dx / l;
    uint32_t i;
    for (i = 0; i <= 40u; i++) {
        float u = (float)i / 40.0f, o = sy_sin2pi(2.0f * u + ph) * 3.2f * wow + sy_sin2pi(14.0f * u) * 1.2f * flt;
        pts[2u * i] = xa + dx * u + nx * o;
        pts[2u * i + 1u] = ya + dy * u + ny * o;
    }
    lk_poly(pts, 41u, 1.5f, c);
}
static void ui_p_tape(void)
{
    uint32_t o = ui_fx_off(), i;
    const int16_t *p = sy_patch.p + o;
    int tone = UI_PG[ui.page].id[0] == (int16_t)(SP_TP_TONE + o);
    float wow = ui_v(p, SP_TP_WOW), flt = ui_v(p, SP_TP_FLUT), sat = ui_v(p, SP_TP_SAT), tn = ui_v(p, SP_TP_TONE), ys[27];
    ui_reel(56.0f, 74.0f, ui_reel_a);
    ui_reel(184.0f, 74.0f, ui_reel_a);
    ui_tape_seg(56.0f, 102.0f, 96.0f, 138.0f, 0.0f, wow, flt, ui_ink(!tone && (ui_hot(0) || ui_hot(1))));
    lk_seg(96.0f, 142.0f, 144.0f, 142.0f, 1.5f, sk.ink);
    ui_tape_seg(144.0f, 138.0f, 184.0f, 102.0f, 0.3f, wow, flt, ui_ink(!tone && (ui_hot(0) || ui_hot(1))));
    lk_ring(96.0f, 138.0f, 4.0f, 1.0f, sk.mid);
    lk_ring(144.0f, 138.0f, 4.0f, 1.0f, sk.mid);
    lk_frame(112.0f, 144.0f, 16.0f, 8.0f, 1.0f, sk.ink);
    lk_frame(196.0f, 140.0f, 30.0f, 30.0f, 1.0f, sk.line);  /* SATURATE */
    for (i = 0; i < 27u; i++) {
        float x = 2.0f * (float)i / 26.0f - 1.0f, g = 0.6f + 4.0f * sat;
        ys[i] = 155.0f - 13.0f * lk_tanh(g * x) / lk_tanh(g);
    }
    lk_curve(198, ys, 27u, 1.5f, ui_ink(!tone && ui_hot(2)), 0);
    lk_seg(20.0f, 160.0f + 6.0f * (1.0f - 2.0f * tn), 70.0f, 160.0f - 6.0f * (1.0f - 2.0f * tn), 1.5f, ui_ink(tone && ui_hot(0)));
    lk_hollow(45.0f, 160.0f, 2.5f);
    ui_mix_row(p, SP_TP_MIX, 3);
    if (tone) {
        pg.has &= (uint8_t)~8u;
        ui_H(0, 74.0f, 150.0f);
    } else {
        ui_H(0, 62.0f, 124.0f);
        ui_H(1, 168.0f, 124.0f);
        ui_H(2, 182.0f, 146.0f);
    }
}
/* the reels turning: each one's box redrawn, a turn in 5 s */
static void ui_tape_wins(void)
{
    static uint32_t t;
    uint32_t dt = fm1_ms - t;
    t = fm1_ms;
    if (dt < 200u)
        ui_reel_a += (float)dt * (1.0f / 5000.0f);
    ui_reel_a -= (float)sy_floori(ui_reel_a);
    ui_win(26, 44, 61, 61, 0);
    ui_win(154, 44, 61, 61, 0);
}
