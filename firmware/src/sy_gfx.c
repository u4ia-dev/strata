/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA's drawing kit on gfx.c's canvas (docs/ARCHITECTURE.md §17; the screens of the design canvas):
 *   - the palettes (SETTINGS > COLOR): thirteen base colours each, by role (ground, display, raised, text, label,
 *     SOUND: waves and curves, NOW: the current frame, playheads, voices, VALUE: a knob's value, MOD: modulation,
 *     MOD -: negative, OTHER: a source not picked, the MOD banner and its ink), the rest derived as tints of them.
 *     gfx.c's own tokens follow (its text ramps, keycaps and hints);
 *   - the anti-aliased shapes the dials and graphs need, blended over what the canvas holds: a pixel, a disc, a
 *     ring's arc with round ends, a segment of any width, a curve plotted column by column (a wave, a response,
 *     an envelope) and the fill under one. Coverage is geometric, the distance to the shape's edge over one
 *     pixel; blended in RGB565. No libm: a square root by Newton steps, angles by cross products.
 * Needs gfx.c and sy_core.h. */

/* -------------------------------------------------------------------------------------- palettes --- */
#define SY_NPAL 8u
enum { PR_GROUND, PR_DISPLAY, PR_RAISED, PR_TEXT, PR_LABEL, PR_SOUND, PR_NOW, PR_VALUE, PR_MOD, PR_MODNEG, PR_MODOFF,
       PR_BANNER, PR_BINK, PR_N };
static const struct { char name[8]; uint32_t c[PR_N]; } SY_PAL[SY_NPAL] = {   /* RGB888 as designed */
    {"STRATA", {0x131418, 0x0b0c0f, 0x1e2026, 0xeceae6, 0x979ba4, 0xffab4a, 0xf5f8ff, 0xd9dee7, 0x3b8bff, 0x9fc8ff,
                0x5b616c, 0x1f5fd0, 0xffffff}},
    {"CLASSIC", {0x111418, 0x0a0d10, 0x1b2129, 0xe8ecf1, 0x8b95a3, 0x4be39b, 0xffd24a, 0xd5e6f7, 0x2f8cff, 0x8cc3ff,
                 0x566070, 0x1d63cc, 0xffffff}},
    {"ICE", {0x0f151c, 0x090d12, 0x1a2430, 0xe6eef6, 0x8ea0b3, 0x7ccfff, 0xffd98a, 0xcfe0f0, 0xb47cff, 0xdcc0ff,
             0x59647a, 0x6b3fc4, 0xffffff}},
    {"VIOLET", {0x15121c, 0x0d0b12, 0x221d2e, 0xefeaf7, 0x9d94b0, 0xc69bff, 0xfff1a8, 0xe2d8f2, 0x4cc9ff, 0xa6e3ff,
                0x615a72, 0x1f6db0, 0xffffff}},
    {"PAPER", {0xece7dd, 0xf7f4ee, 0xddd6c8, 0x1e1f22, 0x5f5b53, 0x22262e, 0xd0312d, 0x3a3f48, 0x1f6fe0, 0x78a6f0,
               0xa8a194, 0x1f6fe0, 0xffffff}},
    {"NIGHT", {0x0b0807, 0x060403, 0x1a1210, 0xdcae9e, 0x8c695e, 0xff6a4d, 0xffd08a, 0xc49a8c, 0xb07cff, 0xd8bdff,
               0x5d4842, 0x6a35b8, 0xffffff}},
    {"HI-CON", {0x000000, 0x000000, 0x1a1a1a, 0xffffff, 0xd0d0d0, 0xffe600, 0xff4fd8, 0xffffff, 0x00d8ff, 0x9ff0ff,
                0x808080, 0x00d8ff, 0x000000}},
    {"MONO", {0x111111, 0x0a0a0a, 0x1d1d1d, 0xededed, 0x9a9a9a, 0xc4c4c4, 0xffffff, 0xe0e0e0, 0x8c8c8c, 0x5e5e5e,
              0x454545, 0xe8e8e8, 0x000000}},                  /* greys only: the roles by lightness */
};
static struct {
    uint16_t ground, display, raised, text, label, sound, now, value, mod, modneg, modoff, banner, bink;   /* stored */
    uint16_t rule, dline, mline, grid, grid2, rline, rline2, track, disc, dl, sub, dim, faint, text2, ghost;
    uint16_t wfar, wmid, wfill, wtint, dfar, dnear, mtint, mtext, mfar, mnear, bsub, chipl, pdim, mband;   /* derived */
    uint8_t idx;
} sk;

static uint16_t sy_rgb(uint32_t c) { return RGB((c >> 16) & 255u, (c >> 8) & 255u, c & 255u); }

/* palette i: its roles, the tints, and gfx.c's tokens from them */
static void sy_palette(uint32_t i)
{
    const uint32_t *c;
    i %= SY_NPAL;
    c = SY_PAL[i].c;
    ux.mono = 0;                                      /* (ux_mix blends every channel) */
    ux.style = ST_FLAT;
    sk.idx = (uint8_t)i;
    sk.ground = sy_rgb(c[PR_GROUND]);
    sk.display = sy_rgb(c[PR_DISPLAY]);
    sk.raised = sy_rgb(c[PR_RAISED]);
    sk.text = sy_rgb(c[PR_TEXT]);
    sk.label = sy_rgb(c[PR_LABEL]);
    sk.sound = sy_rgb(c[PR_SOUND]);
    sk.now = sy_rgb(c[PR_NOW]);
    sk.value = sy_rgb(c[PR_VALUE]);
    sk.mod = sy_rgb(c[PR_MOD]);
    sk.modneg = sy_rgb(c[PR_MODNEG]);
    sk.modoff = sy_rgb(c[PR_MODOFF]);
    sk.banner = sy_rgb(c[PR_BANNER]);
    sk.bink = sy_rgb(c[PR_BINK]);
    sk.rule = ux_mix(sk.ground, sk.text, 7);
    sk.dline = ux_mix(sk.display, sk.text, 12);
    sk.mline = ux_mix(sk.display, sk.mod, 30);
    sk.grid = ux_mix(sk.display, sk.text, 5);
    sk.grid2 = ux_mix(sk.display, sk.text, 9);
    sk.rline = ux_mix(sk.raised, sk.text, 13);
    sk.rline2 = ux_mix(sk.raised, sk.text, 22);
    sk.track = ux_mix(sk.ground, sk.text, 10);
    sk.disc = ux_mix(sk.ground, sk.text, 6);
    sk.dl = ux_mix(sk.ground, sk.text, 17);
    sk.sub = ux_mix(sk.label, sk.ground, 22);
    sk.dim = ux_mix(sk.label, sk.ground, 38);
    sk.faint = ux_mix(sk.label, sk.ground, 58);
    sk.text2 = ux_mix(sk.text, sk.ground, 18);
    sk.ghost = ux_mix(sk.display, sk.label, 32);
    sk.wfar = ux_mix(sk.display, sk.sound, 22);
    sk.wmid = ux_mix(sk.display, sk.sound, 48);
    sk.wfill = ux_mix(sk.display, sk.sound, 10);
    sk.wtint = ux_mix(sk.ground, sk.sound, 13);
    sk.dfar = ux_mix(sk.display, sk.sound, 10);
    sk.dnear = ux_mix(sk.display, sk.sound, 20);
    sk.mtint = ux_mix(sk.ground, sk.mod, 16);
    sk.mtext = ux_mix(sk.mod, sk.text, 60);
    sk.mfar = ux_mix(sk.display, sk.mod, 40);
    sk.mnear = ux_mix(sk.mod, sk.text, 20);
    sk.bsub = ux_mix(sk.banner, sk.bink, 80);
    sk.chipl = ux_mix(sk.ground, sk.text, 7);
    sk.pdim = ux_mix(sk.ground, sk.raised, 40);
    sk.mband = ux_mix(sk.display, sk.mod, 22);
    ux.bg = sk.ground;                                /* gfx.c's tokens: its text ramps (light or dark), keycaps */
    ux.surf = sk.raised;
    ux.text = sk.text;
    ux.theme = sk.sound;
    ux.accent = sk.now;
    ux.mid = sk.label;
    ux.dim = sk.dim;
    ux.line = sk.track;
    ux.sel = sk.raised;
    ux.tint = sk.wtint;
    ux.ink = sk.ground;
    ux.raise = sk.rline;
    ux.key = sk.label;
    ux.lane = sk.wtint;
    ux.grid = sk.grid;
    ux.light = ux_luma(sk.ground) > 128u;
    ux.rec = ux.light ? UI_REC_LIGHT : UI_REC_DARK;
    ux.gen++;
}

/* --------------------------------------------------------------------------------------- shapes --- */
/* a + (b - a) t, t 0..256, per channel */
static inline uint16_t sy_mix565(uint16_t a, uint16_t b, uint32_t t)
{
    uint32_t u = 256u - t;
    uint32_t r = ((uint32_t)(a >> 11) * u + (uint32_t)(b >> 11) * t + 128u) >> 8;
    uint32_t g = ((uint32_t)((a >> 5) & 63u) * u + (uint32_t)((b >> 5) & 63u) * t + 128u) >> 8;
    uint32_t bl = ((uint32_t)(a & 31u) * u + (uint32_t)(b & 31u) * t + 128u) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}

/* c over the canvas pixel (x, y) at coverage a (0..256) */
static void cv_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint32_t i;
    y += cv_oy;
    if (!a || (uint32_t)x >= cv_w || (uint32_t)y >= cv_h)
        return;
    i = (uint32_t)y * cv_w + (uint32_t)x;
    cv_px[i] = a >= 256u ? swap16(c) : swap16(sy_mix565(swap16(cv_px[i]), c, a));
}
static inline void cv_cover(int32_t x, int32_t y, uint16_t c, float cov)
{
    if (cov > 0.0f)
        cv_blend(x, y, c, cov >= 1.0f ? 256u : (uint32_t)(cov * 256.0f));
}

static inline float sy_sqrtf(float x)            /* x >= 0: two Newton steps from the halved exponent, < 1e-5 */
{
    union { float f; uint32_t u; } v;
    float y;
    if (x <= 0.0f)
        return 0.0f;
    v.f = x;
    v.u = 0x1FBD1DF5u + (v.u >> 1);
    y = v.f;
    y = 0.5f * (y + x / y);
    return 0.5f * (y + x / y);
}

/* a dial's turn t (0..1: its 270 degrees, from 7:30 clockwise) as a unit vector, y down */
static void sy_dial_dir(float t, float *x, float *y)
{
    float a = 0.75f * t - 0.375f;
    *x = sy_sin2pi(a);
    *y = -sy_cos2pi(a);
}

/* a filled disc */
static void cv_disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x, y, x0 = sy_floori(cx - r - 1.0f), x1 = sy_floori(cx + r + 1.0f);
    int32_t y0 = sy_floori(cy - r - 1.0f), y1 = sy_floori(cy + r + 1.0f);
    float ro = (r + 1.0f) * (r + 1.0f);
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d2 = dx * dx + dy * dy;
            if (d2 < ro)
                cv_cover(x, y, c, r + 0.5f - sy_sqrtf(d2));
        }
}

/* an arc of a ring: centre (cx, cy), radius r (the stroke's middle), width w, from dial turn t0 to t1, round ends */
static void cv_arc(float cx, float cy, float r, float w, float t0, float t1, uint16_t c)
{
    float ax, ay, bx, by, pax, pay, pbx, pby, hw = 0.5f * w, ro = r + hw + 1.0f, ri = r - hw - 1.0f;
    int32_t x, y, x0, x1, y0, y1;
    int big;
    if (t1 < t0) {
        float s = t0;
        t0 = t1;
        t1 = s;
    }
    t0 = sy_clampf(t0, 0.0f, 1.0f);
    t1 = sy_clampf(t1, 0.0f, 1.0f);
    if (t1 - t0 < 0.002f)
        return;
    sy_dial_dir(t0, &ax, &ay);
    sy_dial_dir(t1, &bx, &by);
    pax = cx + r * ax;
    pay = cy + r * ay;
    pbx = cx + r * bx;
    pby = cy + r * by;
    big = 0.75f * (t1 - t0) > 0.5f;                   /* over half a turn: in = not inside the rest */
    x0 = sy_floori(cx - ro);
    x1 = sy_floori(cx + ro);
    y0 = sy_floori(cy - ro);
    y1 = sy_floori(cy + ro);
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d2 = dx * dx + dy * dy, cov;
            int in;
            if (d2 > ro * ro || (ri > 0.0f && d2 < ri * ri))
                continue;
            if (big)                                  /* (cross products, y down: > 0 = clockwise of) */
                in = !((bx * dy - by * dx) > 0.0f && (dx * ay - dy * ax) > 0.0f);
            else
                in = (ax * dy - ay * dx) >= 0.0f && (dx * by - dy * bx) >= 0.0f;
            if (in) {
                cov = hw + 0.5f - sy_absf(sy_sqrtf(d2) - r);
            } else {                                  /* the round ends */
                float ex = (float)x + 0.5f - pax, ey = (float)y + 0.5f - pay;
                float fx = (float)x + 0.5f - pbx, fy = (float)y + 0.5f - pby;
                float da = ex * ex + ey * ey, db = fx * fx + fy * fy;
                cov = hw + 0.5f - sy_sqrtf(da < db ? da : db);
            }
            cv_cover(x, y, c, cov);
        }
}

/* a segment of width w, round ends */
static void cv_seg(float xa, float ya, float xb, float yb, float w, uint16_t c)
{
    float hw = 0.5f * w, dx = xb - xa, dy = yb - ya, l2 = dx * dx + dy * dy;
    int32_t x, y, x0 = sy_floori(sy_minf(xa, xb) - hw - 1.0f), x1 = sy_floori(sy_maxf(xa, xb) + hw + 1.0f);
    int32_t y0 = sy_floori(sy_minf(ya, yb) - hw - 1.0f), y1 = sy_floori(sy_maxf(ya, yb) + hw + 1.0f);
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float px = (float)x + 0.5f - xa, py = (float)y + 0.5f - ya;
            float t = l2 > 0.0f ? sy_clampf((px * dx + py * dy) / l2, 0.0f, 1.0f) : 0.0f;
            float ex = px - t * dx, ey = py - t * dy;
            cv_cover(x, y, c, hw + 0.5f - sy_sqrtf(ex * ex + ey * ey));
        }
}

/* a curve as its y at each column's centre (ys[i]: column x0 + i): in each column its span between the
 * neighbours' midpoints, widened by w / 2, the ends anti-aliased */
static void cv_curve(int32_t x0, const float *ys, uint32_t n, float w, uint16_t c)
{
    uint32_t i;
    float hw = 0.5f * w;
    for (i = 0; i < n; i++) {
        float ya = i ? 0.5f * (ys[i - 1u] + ys[i]) : ys[i], yb = i + 1u < n ? 0.5f * (ys[i] + ys[i + 1u]) : ys[i];
        float lo = sy_minf(sy_minf(ya, yb), ys[i]) - hw, hi = sy_maxf(sy_maxf(ya, yb), ys[i]) + hw;
        int32_t y, ylo = sy_floori(lo), yhi = sy_floori(hi);
        for (y = ylo; y <= yhi; y++)
            cv_cover(x0 + (int32_t)i, y, c, sy_minf((float)y + 1.0f, hi) - sy_maxf((float)y, lo));
    }
}

/* the area between a curve and the row y = base, in c */
static void cv_under(int32_t x0, const float *ys, uint32_t n, float base, uint16_t c)
{
    uint32_t i;
    int32_t yb = sy_floori(base + 0.5f);
    for (i = 0; i < n; i++) {
        int32_t yt = sy_floori(ys[i] + 0.5f);
        if (yt < yb)
            cv_rect(x0 + (int32_t)i, yt, 1, yb - yt, c);
        else if (yt > yb)
            cv_rect(x0 + (int32_t)i, yb, 1, yt - yb, c);
    }
}

/* a dashed row: on px drawn, off px left, from x0 to x1 */
static void cv_dash(int32_t x0, int32_t x1, int32_t y, int32_t on, int32_t off, uint16_t c)
{
    int32_t x;
    for (x = x0; x < x1; x += on + off)
        cv_rect(x, y, x + on > x1 ? x1 - x : on, 1, c);
}

/* .. a dashed column, from y0 to y1 */
static void cv_vdash(int32_t x, int32_t y0, int32_t y1, int32_t on, int32_t off, uint16_t c)
{
    int32_t y;
    for (y = y0; y < y1; y += on + off)
        cv_rect(x, y, 1, y + on > y1 ? y1 - y : on, c);
}

/* a curve dashed: on columns drawn, off left (the shape before a knob's turn) */
static void cv_curve_dash(int32_t x0, const float *ys, uint32_t n, float w, uint16_t c, uint32_t on, uint32_t off)
{
    uint32_t i;
    for (i = 0; i < n; i += on + off)
        cv_curve(x0 + (int32_t)i, ys + i, i + on <= n ? on : n - i, w, c);
}

/* the area between two curves (the range a modulation sweeps), in c */
static void cv_band(int32_t x0, const float *ya, const float *yb, uint32_t n, uint16_t c)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = sy_floori(sy_minf(ya[i], yb[i]) + 0.5f), b = sy_floori(sy_maxf(ya[i], yb[i]) + 0.5f);
        if (b > a)
            cv_rect(x0 + (int32_t)i, a, 1, b - a, c);
    }
}
