/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* The STRATA UI's drawing kit (the strata-ui build; the design: the "Strata UI Overhaul" gallery, docs/UI.md):
 *   - the palettes (SETTINGS > SCREEN, twelve since V0.67): six colours each, by role: GROUND, INK (the drawing and
 *     the values), LAB (the labels' text, V0.67), MID (the lesser lines), LINE (tracks, the faint), HOT (the one
 *     accent: what is live: a knob turning, the voices, a countdown); the stipple's tint from them; gfx.c's tokens
 *     follow (its text ramps);
 *   - the canvas as a window onto the screen: lk_begin places gfx.c's canvas there (a strip of the screen, or a live
 *     part's box) and every shape takes the screen's coordinates, drawing what of it falls inside; a shape wholly
 *     outside costs its test only (sy_ui_draw.c draws a page once per strip);
 *   - the shapes, anti-aliased (coverage: the distance to the shape's edge over one pixel), blended over what the
 *     canvas holds in RGB565: boxes and outlines with fractional edges, discs, rings and arcs (each row's span of the
 *     ring found by a root a row; an arc's ends by cross products), segments, polylines (column by column along the
 *     major axis, each segment half-open so a joint is drawn once), curves (a y a column, the stroke widened with
 *     the slope, solid or dashed along its length), the ground under a curve, dots, a triangle. No libm, no tables:
 *     a square root by Newton steps, angles by sy_sin2pi and rotations;
 *   - text in Space Mono (gen_aa_font.py --preset space-mono) with the design's letter-spacing, on the baseline its
 *     CSS line box puts it (half the line height, plus (ascent - descent) / 2 of the face: 0.3795 em), aligned in a
 *     box;
 *   - the parts every screen shares: a knob's numbered handle, chips, the zones' row, the toast, the
 *     top line (the module's name, its dot, the module dots or the chain's squares, the right word), a layer's
 *     banner, the mix row, a fader, the FM-1's 27 keys, the effects' glyphs (10 x 7, a bit a pixel: 9 bytes each).
 * Needs gfx.c and sy_core.h. */

/* -------------------------------------------------------------------------------------- palettes --- */
#define SY_NPAL 12u
enum { PR_GROUND, PR_INK, PR_LAB, PR_MID, PR_LINE, PR_HOT, PR_N };
static const struct { char name[8]; uint32_t c[PR_N]; } SY_PAL[SY_NPAL] = {   /* RGB888: LAB (the labels' text), MID
                                                                                 * and LINE INK over GROUND at ~76 %,
                                                                                 * ~47 % and ~14 % */
    {"STRATA", {0x0B0B0B, 0xF4F1EA, 0xBAB7B1, 0x7A766E, 0x2C2A27, 0xFF4D2E}},
    {"CLASSIC", {0x101820, 0xE8E8E8, 0xB4B6B8, 0x767A7E, 0x2E353C, 0xF2A900}},
    {"ICE", {0x0A1420, 0xDDF1FF, 0xAABCCB, 0x6D7C89, 0x27333F, 0x5CC8FF}},
    {"VIOLET", {0x140C1E, 0xEDE3FF, 0xB7ADC6, 0x7A7188, 0x322A3D, 0xB57CFF}},
    {"PAPER", {0xEFEBE3, 0x151515, 0x4B4A47, 0x857F75, 0xD3CDC2, 0xE2401F}},
    {"NIGHT", {0x000000, 0x8A8A8A, 0x7A7A7A, 0x414141, 0x1C1C1C, 0xC2410C}},   /* (dim: LAB 88 %, LINE 20 %) */
    {"HI-CON", {0x000000, 0xFFFFFF, 0xD0D0D0, 0x999999, 0x404040, 0xFFD400}},  /* (LAB 82 %, MID 60 %, LINE 25 %) */
    {"MONO", {0x000000, 0xFFFFFF, 0xC2C2C2, 0x787878, 0x242424, 0xFFFFFF}},    /* (the accent is the ink) */
    {"MOSS", {0x0C110C, 0xE4EDD8, 0xB0B8A7, 0x72786C, 0x2A3029, 0xB8E04A}},    /* (V0.67's four) */
    {"DUSK", {0x1A1020, 0xF6E4E8, 0xC1B1B8, 0x81747E, 0x392E3C, 0xFF6F91}},
    {"SAND", {0xE9DFC9, 0x2A2118, 0x584F42, 0x8F8676, 0xCEC4B0, 0x0A6E70}},    /* (the second light one) */
    {"NEON", {0x06070B, 0xC9FFF1, 0x9AC3BA, 0x617B77, 0x20292A, 0xFF3FD2}},
};
static struct {
    uint16_t ground, ink, lab, mid, line, hot;      /* stored */
    uint16_t stip;                                  /* derived: the stipple (MID at 55 % over GROUND) */
    uint8_t idx;
} sk;

static uint16_t sy_rgb(uint32_t c) { return RGB((c >> 16) & 255u, (c >> 8) & 255u, c & 255u); }

/* a + (b - a) t, t 0..256, per channel */
static inline uint16_t sy_mix565(uint16_t a, uint16_t b, uint32_t t)
{
    uint32_t u = 256u - t;
    uint32_t r = ((uint32_t)(a >> 11) * u + (uint32_t)(b >> 11) * t + 128u) >> 8;
    uint32_t g = ((uint32_t)((a >> 5) & 63u) * u + (uint32_t)((b >> 5) & 63u) * t + 128u) >> 8;
    uint32_t bl = ((uint32_t)(a & 31u) * u + (uint32_t)(b & 31u) * t + 128u) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}

/* palette i: its roles, the stipple, and gfx.c's tokens from them */
static void sy_palette(uint32_t i)
{
    const uint32_t *c;
    i %= SY_NPAL;
    c = SY_PAL[i].c;
    ux.mono = 0;                                      /* (ux_mix blends every channel) */
    ux.style = ST_FLAT;
    sk.idx = (uint8_t)i;
    sk.ground = sy_rgb(c[PR_GROUND]);
    sk.ink = sy_rgb(c[PR_INK]);
    sk.lab = sy_rgb(c[PR_LAB]);
    sk.mid = sy_rgb(c[PR_MID]);
    sk.line = sy_rgb(c[PR_LINE]);
    sk.hot = sy_rgb(c[PR_HOT]);
    sk.stip = sy_mix565(sk.ground, sk.mid, 141u);
    ux.bg = sk.ground;                                /* gfx.c's tokens: its text ramps (light or dark), keycaps */
    ux.surf = sk.line;
    ux.text = sk.ink;
    ux.theme = sk.ink;
    ux.accent = sk.hot;
    ux.mid = sk.lab;
    ux.dim = sk.lab;
    ux.line = sk.line;
    ux.sel = sk.line;
    ux.tint = sk.line;
    ux.ink = sk.ground;
    ux.raise = sk.line;
    ux.key = sk.mid;
    ux.lane = sk.line;
    ux.grid = sk.line;
    ux.light = ux_luma(sk.ground) > 128u;
    ux.rec = ux.light ? UI_REC_LIGHT : UI_REC_DARK;
    ux.gen++;
}

/* ---------------------------------------------------------------------------------------- window --- */
static int32_t lk_x0, lk_y0, lk_x1, lk_y1;         /* the canvas on the screen: columns x0 .. x1 - 1, rows y0 .. y1 - 1 */
static uint8_t lk_live;                             /* a live part's window: a page may leave its still parts out */

/* the canvas as the screen's box (x, y, w, h), as it is (lk_begin: cleared); gfx.c's own drawing (cv_alpha's rows)
 * by cv_oy */
static void lk_place(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    if (w * h > CV_MAX)
        h = CV_MAX / w;
    lcd_sync();                                     /* (the last blit may still read cv_px) */
    GFX_HOOK_BEGIN();
    cv_w = w;
    cv_h = h;
    cv_cy0 = 0;
    cv_cy1 = (int16_t)h;
    cv_bg = sk.ground;
    lk_x0 = x;
    lk_y0 = y;
    lk_x1 = x + (int32_t)cv_w;
    lk_y1 = y + (int32_t)cv_h;
    cv_oy = -y;
}
/* .. cleared to GROUND, two pixels a store */
static void lk_begin(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    uint16_t *p = cv_px, s = swap16(sk.ground);
    uint32_t n, i, v = (uint32_t)s | (uint32_t)s << 16;
    lk_place(x, y, w, h);
    n = cv_w * cv_h;
    if (((uintptr_t)p & 3u) && n) {
        *p++ = s;
        n--;
    }
    for (i = 0; i < n / 2u; i++)
        ((uint32_t *)(void *)p)[i] = v;
    if (n & 1u)
        p[n - 1u] = s;
}
/* the box x0 .. x1, y0 .. y1 touches the window */
static int lk_vis(float x0, float y0, float x1, float y1)
{
    return x1 >= (float)lk_x0 && x0 < (float)lk_x1 && y1 >= (float)lk_y0 && y0 < (float)lk_y1;
}

/* c over the screen's pixel (x, y) at coverage a (0..256) */
static inline void lk_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *p;
    if (!a || x < lk_x0 || x >= lk_x1 || y < lk_y0 || y >= lk_y1)
        return;
    p = &cv_px[(uint32_t)(y - lk_y0) * cv_w + (uint32_t)(x - lk_x0)];
    *p = a >= 256u ? swap16(c) : swap16(sy_mix565(swap16(*p), c, a));
}
static inline void lk_cover(int32_t x, int32_t y, uint16_t c, float cov)
{
    if (cov > 0.004f)
        lk_blend(x, y, c, cov >= 1.0f ? 256u : (uint32_t)(cov * 256.0f));
}
/* the window's columns (rows) a .. b - 1 of the span lo .. hi */
static inline void lk_cols(float lo, float hi, int32_t *a, int32_t *b)
{
    *a = sy_floori(lo);
    *b = sy_floori(hi) + 1;
    if (*a < lk_x0)
        *a = lk_x0;
    if (*b > lk_x1)
        *b = lk_x1;
}
static inline void lk_rows(float lo, float hi, int32_t *a, int32_t *b)
{
    *a = sy_floori(lo);
    *b = sy_floori(hi) + 1;
    if (*a < lk_y0)
        *a = lk_y0;
    if (*b > lk_y1)
        *b = lk_y1;
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
static inline float lk_rsqrt(float x)           /* 1 / sqrt(x), x > 0: the halved exponent and a Newton step (0.2 %) */
{
    union { float f; uint32_t u; } v;
    v.f = x;
    v.u = 0x5F3759DFu - (v.u >> 1);
    return v.f * (1.5f - 0.5f * x * v.f * v.f);
}
static inline float lk_tanh(float x)            /* tanh: (e^2x - 1) / (e^2x + 1), by sy_exp2f */
{
    float e;
    x = sy_clampf(x, -9.0f, 9.0f);
    e = sy_exp2f(2.8853900818f * x);
    return (e - 1.0f) / (e + 1.0f);
}

/* --------------------------------------------------------------------------------------- shapes --- */
/* a box with fractional edges, filled */
static void lk_box(float x0, float y0, float x1, float y1, uint16_t c)
{
    int32_t x, y, xa, xb, ya, yb;
    if (x1 <= x0 || y1 <= y0)
        return;
    lk_cols(x0, x1 - 0.0001f, &xa, &xb);
    lk_rows(y0, y1 - 0.0001f, &ya, &yb);
    for (y = ya; y < yb; y++) {
        float oy = sy_minf((float)y + 1.0f, y1) - sy_maxf((float)y, y0);
        for (x = xa; x < xb; x++)
            lk_cover(x, y, c, oy * (sy_minf((float)x + 1.0f, x1) - sy_maxf((float)x, x0)));
    }
}
/* a filled box of whole pixels */
static void lk_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { lk_box((float)x, (float)y, (float)(x + w), (float)(y + h), c); }

/* the outline of the box (x, y, w, h) (its edges' middles), stroke sw */
static void lk_frame(float x, float y, float w, float h, float sw, uint16_t c)
{
    float s = 0.5f * sw;
    lk_box(x - s, y - s, x + w + s, y + s, c);
    lk_box(x - s, y + h - s, x + w + s, y + h + s, c);
    lk_box(x - s, y + s, x + s, y + h - s, c);
    lk_box(x + w - s, y + s, x + w + s, y + h - s, c);
}
/* .. dashed (on px drawn, off px left, from its top left corner clockwise), stroke 1 */
static void lk_frame_dash(float x, float y, float w, float h, float on, float off, uint16_t c)
{
    float e[4] = {w, h, w, h}, ph = 0.0f, p = on + off;
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        float t;
        for (t = 0.0f; t < e[k]; t += 0.25f) {     /* (a quarter pixel a step: the dash's ends kept) */
            float s0 = t, s1 = sy_minf(t + 0.25f, e[k]);
            if (sy_floori(ph / p) * p + on > ph) {
                float a0 = s0, a1 = s1;
                if (k == 0u)
                    lk_box(x + a0, y - 0.5f, x + a1, y + 0.5f, c);
                else if (k == 1u)
                    lk_box(x + w - 0.5f, y + a0, x + w + 0.5f, y + a1, c);
                else if (k == 2u)
                    lk_box(x + w - a1, y + h - 0.5f, x + w - a0, y + h + 0.5f, c);
                else
                    lk_box(x - 0.5f, y + h - a1, x + 0.5f, y + h - a0, c);
            }
            ph += s1 - s0;
        }
    }
}

/* a filled disc: each pixel by (ro^2 - d^2) / 2 ro, its edge's distance to first order (no root a pixel; under 0.02
 * off at the edge of a handle's ring); each row's span by a reciprocal root, clipped to the window once (its pixels
 * stored straight) */
static void lk_disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x, y, xa, xb, ya, yb;
    float ro = r + 0.5f, ro2 = ro * ro, iro = 0.5f / ro;
    uint16_t sc = swap16(c);
    if (!lk_vis(cx - ro, cy - ro, cx + ro, cy + ro))
        return;
    lk_rows(cy - ro, cy + ro, &ya, &yb);
    for (y = ya; y < yb; y++) {
        float dy = (float)y + 0.5f - cy, e = ro2 - dy * dy, xs;
        uint16_t *p;
        if (e <= 0.0f)
            continue;
        xs = ro < 2.5f ? ro : e * lk_rsqrt(e);    /* (a small one: its box; the pixels past it come to nothing) */
        lk_cols(cx - xs, cx + xs, &xa, &xb);
        p = &cv_px[(uint32_t)(y - lk_y0) * cv_w + (uint32_t)(xa - lk_x0)];
        for (x = xa; x < xb; x++, p++) {
            float dx = (float)x + 0.5f - cx, cov = (e - dx * dx) * iro;
            if (cov > 0.004f)
                *p = cov >= 1.0f ? sc : swap16(sy_mix565(swap16(*p), c, (uint32_t)(cov * 256.0f)));
        }
    }
}

/* an arc of the ring at (cx, cy), radius r (the stroke's middle), stroke sw, from turn a0 clockwise to a1 (turns: 0
 * east, 0.25 south; a whole turn or more: the ring), its ends round (caps) or cut along the radius */
static void lk_arc(float cx, float cy, float r, float sw, float a0, float a1, uint16_t c, int caps)
{
    float hw = 0.5f * sw, ro = r + hw + 0.5f, ri = r - hw - 0.5f, span = a1 - a0, ax, ay, bx, by;
    int32_t x, y, xa, xb, ya, yb, half;
    int full = span >= 0.9999f, big = span > 0.5f;
    if (span <= 0.0005f || !lk_vis(cx - ro, cy - ro, cx + ro, cy + ro))
        return;
    ax = sy_cos2pi(a0);
    ay = sy_sin2pi(a0);
    bx = sy_cos2pi(a1);
    by = sy_sin2pi(a1);
    lk_rows(cy - ro, cy + ro, &ya, &yb);
    for (y = ya; y < yb; y++) {
        float dy = (float)y + 0.5f - cy, xo, xi;
        if (sy_absf(dy) >= ro)
            continue;
        xo = sy_sqrtf(ro * ro - dy * dy);
        xi = ri > 0.0f && sy_absf(dy) < ri ? sy_sqrtf(ri * ri - dy * dy) : -1.0f;
        for (half = 0; half < 2; half++) {          /* the row's two spans of the ring (one, past the hole) */
            if (xi < 0.0f && half)
                break;
            if (xi < 0.0f)
                lk_cols(cx - xo, cx + xo, &xa, &xb);
            else if (!half)
                lk_cols(cx - xo, cx - xi, &xa, &xb);
            else
                lk_cols(cx + xi, cx + xo, &xa, &xb);
            for (x = xa; x < xb; x++) {
                float dx = (float)x + 0.5f - cx, d = sy_sqrtf(dx * dx + dy * dy), cov = hw + 0.5f - sy_absf(d - r);
                if (cov <= 0.0f)
                    continue;
                if (!full) {                         /* (cross products, y down: > 0 = clockwise of) */
                    float e0 = ax * dy - ay * dx, e1 = dx * by - dy * bx;
                    int in = big ? !(e0 < 0.0f && e1 < 0.0f) : e0 >= 0.0f && e1 >= 0.0f;
                    if (!in || (!big && caps)) {
                        if (caps) {                  /* the round ends */
                            float px = (float)x + 0.5f, py = (float)y + 0.5f;
                            float ex = px - cx - r * ax, ey = py - cy - r * ay, fx = px - cx - r * bx, fy = py - cy - r * by;
                            float de = sy_minf(ex * ex + ey * ey, fx * fx + fy * fy);
                            float ce = hw + 0.5f - sy_sqrtf(de);
                            if (!in)
                                cov = ce;
                        } else if (!big) {           /* the ends cut along the radius, their edge anti-aliased */
                            cov *= sy_clampf(0.5f + sy_minf(e0, e1) / (d > 0.5f ? d : 0.5f) * r, 0.0f, 1.0f);
                        } else {
                            cov = 0.0f;
                        }
                    } else if (!big) {
                        cov *= sy_clampf(0.5f + sy_minf(e0, e1) / (d > 0.5f ? d : 0.5f) * r, 0.0f, 1.0f);
                    }
                }
                lk_cover(x, y, c, cov);
            }
        }
    }
}
static void lk_ring(float cx, float cy, float r, float sw, uint16_t c) { lk_arc(cx, cy, r, sw, 0.0f, 1.0f, c, 0); }
/* a dial's arc (the design's 135 .. 405 degrees, 7:30 clockwise to 4:30) from t0 to t1 (0..1 of it), round ends */
static void lk_dial_arc(float cx, float cy, float r, float sw, float t0, float t1, uint16_t c)
{
    lk_arc(cx, cy, r, sw, 0.375f + 0.75f * t0, 0.375f + 0.75f * t1, c, 1);
}
/* a dashed ring, stroke 1: on px of every on + off around (its dashes' places by a rotation a step) */
static void lk_ring_dash(float cx, float cy, float r, float on, float off, uint16_t c)
{
    uint32_t n = (uint32_t)sy_floori(6.2831853f * r / (on + off) + 0.5f), i;
    float vx = 0.0f, vy = -1.0f, cs, sn, ux, uy, ds;
    if (!n || !lk_vis(cx - r - 1.0f, cy - r - 1.0f, cx + r + 1.0f, cy + r + 1.0f))
        return;
    cs = sy_cos2pi(1.0f / (float)n);
    sn = sy_sin2pi(1.0f / (float)n);
    ds = on / r;                                    /* (a dash's turn, in radians: its far end by the tangent) */
    for (i = 0; i < n; i++) {
        float px = cx + r * vx, py = cy + r * vy;
        ux = -vy;                                   /* the tangent, clockwise */
        uy = vx;
        if (on <= 1.0f)
            lk_box(px - 0.5f, py - 0.5f, px + 0.5f, py + 0.5f, c);
        else {
            float qx = px + ux * on, qy = py + uy * on;
            int32_t x, y, xa, xb, ya, yb;          /* a short segment: its pixels by their distance to it */
            lk_cols(sy_minf(px, qx) - 1.0f, sy_maxf(px, qx) + 1.0f, &xa, &xb);
            lk_rows(sy_minf(py, qy) - 1.0f, sy_maxf(py, qy) + 1.0f, &ya, &yb);
            for (y = ya; y < yb; y++)
                for (x = xa; x < xb; x++) {
                    float ex = (float)x + 0.5f - px, ey = (float)y + 0.5f - py, t = sy_clampf((ex * ux + ey * uy) / on, 0.0f, 1.0f);
                    float fx = ex - t * ux * on, fy = ey - t * uy * on;
                    lk_cover(x, y, c, 1.0f - sy_sqrtf(fx * fx + fy * fy));
                }
        }
        {                                           /* the next: rotated by 1 / n of a turn */
            float nx = vx * cs - vy * sn;
            vy = vx * sn + vy * cs;
            vx = nx;
        }
    }
    (void)ds;
}

/* a segment, stroke sw, round ends; a thin one along an axis on the pixels' middle (crisp) */
static void lk_seg(float xa, float ya, float xb, float yb, float sw, uint16_t c)
{
    float hw = 0.5f * sw, dx, dy, l2;
    int32_t x, y, x0, x1, y0, y1;
    if (sw <= 1.5f && xa == xb)
        xa = xb = (float)sy_floori(xa) + 0.5f;
    if (sw <= 1.5f && ya == yb)
        ya = yb = (float)sy_floori(ya) + 0.5f;
    dx = xb - xa;
    dy = yb - ya;
    l2 = dx * dx + dy * dy;
    if (!lk_vis(sy_minf(xa, xb) - hw - 1.0f, sy_minf(ya, yb) - hw - 1.0f, sy_maxf(xa, xb) + hw + 1.0f,
                sy_maxf(ya, yb) + hw + 1.0f))
        return;
    lk_cols(sy_minf(xa, xb) - hw - 0.5f, sy_maxf(xa, xb) + hw + 0.5f, &x0, &x1);
    lk_rows(sy_minf(ya, yb) - hw - 0.5f, sy_maxf(ya, yb) + hw + 0.5f, &y0, &y1);
    if ((xa == xb || ya == yb) && sw <= 1.5f) {      /* (along an axis: a box, its ends' halves cut square) */
        lk_box(sy_minf(xa, xb) - (xa == xb ? hw : 0.0f), sy_minf(ya, yb) - (ya == yb ? hw : 0.0f),
               sy_maxf(xa, xb) + (xa == xb ? hw : 0.0f), sy_maxf(ya, yb) + (ya == yb ? hw : 0.0f), c);
        return;
    }
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            float px = (float)x + 0.5f - xa, py = (float)y + 0.5f - ya;
            float t = l2 > 0.0f ? sy_clampf((px * dx + py * dy) / l2, 0.0f, 1.0f) : 0.0f;
            float ex = px - t * dx, ey = py - t * dy;
            lk_cover(x, y, c, hw + 0.5f - sy_sqrtf(ex * ex + ey * ey));
        }
}

/* a polyline's segment a .. b, stroke sw, half-open (b's column or row is the next segment's): column by column along
 * its major axis, each pixel by its distance across */
static void lk_wu(float xa, float ya, float xb, float yb, float sw, uint16_t c)
{
    float dx = xb - xa, dy = yb - ya, hw = 0.5f * sw;
    int xm = sy_absf(dx) >= sy_absf(dy);
    float lo, hi, m, k, ext;
    int32_t i, i0, i1, j, j0, j1;
    if (!lk_vis(sy_minf(xa, xb) - hw - 1.0f, sy_minf(ya, yb) - hw - 1.0f, sy_maxf(xa, xb) + hw + 1.0f,
                sy_maxf(ya, yb) + hw + 1.0f))
        return;
    if (xm) {
        if (dx == 0.0f)
            return;
        m = dy / dx;
        lo = sy_minf(xa, xb);
        hi = sy_maxf(xa, xb);
        lk_cols(lo, hi - 0.0001f, &i0, &i1);
    } else {
        m = dx / dy;
        lo = sy_minf(ya, yb);
        hi = sy_maxf(ya, yb);
        lk_rows(lo, hi - 0.0001f, &i0, &i1);
    }
    k = lk_rsqrt(1.0f + m * m);                     /* (the distance across from the distance along the minor axis) */
    ext = (hw + 0.5f) / k;
    {                                               /* wholly in the window (the bloom's, a curve's): no pixel's bounds */
        float a0 = (xm ? sy_minf(ya, yb) : sy_minf(xa, xb)) - ext, a1 = (xm ? sy_maxf(ya, yb) : sy_maxf(xa, xb)) + ext;
        int32_t f0 = sy_floori(lo), f1 = sy_floori(hi), g0 = sy_floori(a0), g1 = sy_floori(a1);
        int in = xm ? f0 >= lk_x0 && f1 < lk_x1 && g0 >= lk_y0 && g1 < lk_y1
                    : f0 >= lk_y0 && f1 < lk_y1 && g0 >= lk_x0 && g1 < lk_x1;
        if (in) {
            float hk = hw + 0.5f;
            uint16_t sc = swap16(c);
            for (i = i0; i < i1; i++) {
                float ov = sy_minf((float)i + 1.0f, hi) - sy_maxf((float)i, lo), u, v;
                if (ov <= 0.0f)
                    continue;
                u = sy_clampf((float)i + 0.5f, lo, hi);
                v = xm ? ya + (u - xa) * m : xa + (u - ya) * m;
                j0 = sy_floori(v - ext);
                j1 = sy_floori(v + ext) + 1;
                for (j = j0; j < j1; j++) {
                    float cov = sy_minf((hk - sy_absf((float)j + 0.5f - v) * k) * ov, ov);
                    uint16_t *p;
                    if (cov <= 0.004f)
                        continue;
                    p = xm ? &cv_px[(uint32_t)(j - lk_y0) * cv_w + (uint32_t)(i - lk_x0)]
                           : &cv_px[(uint32_t)(i - lk_y0) * cv_w + (uint32_t)(j - lk_x0)];
                    *p = cov >= 1.0f ? sc : swap16(sy_mix565(swap16(*p), c, (uint32_t)(cov * 256.0f)));
                }
            }
            return;
        }
    }
    for (i = i0; i < i1; i++) {
        float ov = sy_minf((float)i + 1.0f, hi) - sy_maxf((float)i, lo), u, v;
        if (ov <= 0.0f)
            continue;
        u = sy_clampf((float)i + 0.5f, lo, hi);
        v = xm ? ya + (u - xa) * m : xa + (u - ya) * m;
        if (xm)
            lk_rows(v - ext, v + ext, &j0, &j1);
        else
            lk_cols(v - ext, v + ext, &j0, &j1);
        for (j = j0; j < j1; j++) {
            float cov = (hw + 0.5f - sy_absf((float)j + 0.5f - v) * k) * ov;
            if (xm)
                lk_cover(i, j, c, sy_minf(cov, ov));
            else
                lk_cover(j, i, c, sy_minf(cov, ov));
        }
    }
}
/* a polyline through n points (x, y pairs), stroke sw */
static void lk_poly(const float *p, uint32_t n, float sw, uint16_t c)
{
    uint32_t i;
    for (i = 1; i < n; i++)
        lk_wu(p[2u * i - 2u], p[2u * i - 1u], p[2u * i], p[2u * i + 1u], sw, c);
}
/* a polyline dashed along its length (on px drawn, off px left, the pattern carried across its joints), stroke sw */
static void lk_poly_dash(const float *p, uint32_t n, float sw, uint16_t c, float on, float off)
{
    uint32_t i;
    float ph = 0.0f, per = on + off;
    for (i = 1; i < n; i++) {
        float xa = p[2u * i - 2u], ya = p[2u * i - 1u], dx = p[2u * i] - xa, dy = p[2u * i + 1u] - ya;
        float l = sy_sqrtf(dx * dx + dy * dy), s = 0.0f;
        if (l <= 0.0f)
            continue;
        while (s < l) {                             /* each dash (or the part of it) on this segment */
            float in = ph - per * (float)sy_floori(ph / per), e;
            if (in < on) {
                e = sy_minf(l, s + on - in);
                if (sw <= 1.5f && (dx == 0.0f || dy == 0.0f))
                    lk_seg(xa + dx * s / l, ya + dy * s / l, xa + dx * e / l, ya + dy * e / l, sw, c);
                else
                    lk_wu(xa + dx * s / l, ya + dy * s / l, xa + dx * e / l, ya + dy * e / l, sw, c);
            } else {
                e = sy_minf(l, s + per - in);
            }
            ph += e - s;
            s = e;
        }
    }
}
/* a dashed line a -> b, stroke 1 */
static void lk_seg_dash(float xa, float ya, float xb, float yb, uint16_t c, float on, float off)
{
    float p[4] = {xa, ya, xb, yb};
    lk_poly_dash(p, 2u, 1.0f, c, on, off);
}
/* an arrow a -> b: its shaft and a head of two strokes hd px long (the design's K.arrow), stroke sw */
static void lk_arrow(float xa, float ya, float xb, float yb, float hd, float sw, uint16_t c)
{
    float dx = xb - xa, dy = yb - ya, l = sy_sqrtf(dx * dx + dy * dy), ux, uy, c6 = 0.8253356f, s6 = 0.5646425f;
    if (l <= 0.0f)
        return;
    ux = dx / l;
    uy = dy / l;
    lk_seg(xa, ya, xb, yb, sw, c);
    lk_seg(xb, yb, xb - hd * (ux * c6 + uy * s6), yb - hd * (uy * c6 - ux * s6), sw, c);   /* (+-0.6 rad) */
    lk_seg(xb, yb, xb - hd * (ux * c6 - uy * s6), yb - hd * (uy * c6 + ux * s6), sw, c);
}

/* a curve as its y at each column's centre (ys[i]: column x0 + i), stroke sw kept across slopes; dash 0: solid, else
 * on | off << 8 (px drawn, px left, along its length from its start) */
#define LK_DASH(on, off) ((uint32_t)(on) | (uint32_t)(off) << 8)
static void lk_curve(int32_t x0, const float *ys, uint32_t n, float sw, uint16_t c, uint32_t dash)
{
    uint32_t i, on = dash & 255u, per = on + (dash >> 8);
    float hw = 0.5f * sw, lo = 1e9f, hi = -1e9f, s = 0.0f;
    for (i = 0; i < n; i++) {
        lo = sy_minf(lo, ys[i]);
        hi = sy_maxf(hi, ys[i]);
    }
    if (!n || !lk_vis((float)x0 - 1.0f, lo - sw - 1.0f, (float)(x0 + (int32_t)n) + 1.0f, hi + sw + 1.0f))
        return;
    for (i = 0; i < n; i++) {
        float ya = i ? 0.5f * (ys[i - 1u] + ys[i]) : ys[i], yb = i + 1u < n ? 0.5f * (ys[i] + ys[i + 1u]) : ys[i];
        float m = yb - ya, am, g, a, b, side = 0.0f, lo3 = sy_minf(sy_minf(ya, yb), ys[i]), hi3 = sy_maxf(sy_maxf(ya, yb), ys[i]);
        int32_t y, y0, y1, x = x0 + (int32_t)i, k;
        if (!per && (hi3 + 1.5f * hw + 1.0f < (float)lk_y0 || lo3 - 1.5f * hw - 1.0f >= (float)lk_y1))
            continue;                               /* (solid, outside the window's rows: nothing, not even its root) */
        am = sy_absf(m);
        g = sy_sqrtf(1.0f + m * m);
        s += g;
        if (per && (uint32_t)sy_floori(s) % per >= on)
            continue;
        if (x < lk_x0 - 1 || x > lk_x1)
            continue;
        /* the stroke's reach up and down past the column's span: its width over the slope's cosine for a gentle
         * one; a steep one's span is the line itself, its width goes across instead (into the columns each side) */
        a = sy_minf(sy_minf(ya, yb), ys[i]) - hw * sy_minf(g, 1.4142f);
        b = sy_maxf(sy_maxf(ya, yb), ys[i]) + hw * sy_minf(g, 1.4142f);
        if (am > 1.0f)
            side = sy_clampf(hw * sy_sqrtf(1.0f + 1.0f / (m * m)) - 0.5f, 0.0f, 1.0f);
        lk_rows(a, b, &y0, &y1);
        for (y = y0; y < y1; y++) {
            float cv = sy_minf((float)y + 1.0f, b) - sy_maxf((float)y, a);
            lk_cover(x, y, c, cv);
            if (side > 0.0f)
                for (k = -1; k <= 1; k += 2)
                    lk_cover(x + k, y, c, cv * side);
        }
    }
}
/* the ground between a curve (as lk_curve's) and the row `base`, in c */
static void lk_under(int32_t x0, const float *ys, uint32_t n, float base, uint16_t c)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        lk_box((float)(x0 + (int32_t)i), sy_minf(ys[i], base), (float)(x0 + (int32_t)i + 1), sy_maxf(ys[i], base), c);
}
/* a dot of the stipple at (x, y), its top left: a whole pixel in its tint (MID at 55 %: the design's 1.2 px dot,
 * crisp on the panel, a store and no blend) */
static void lk_stip(float x, float y)
{
    int32_t px = sy_floori(x + 0.1f), py = sy_floori(y + 0.1f);
    if (px >= lk_x0 && px < lk_x1 && py >= lk_y0 && py < lk_y1)
        cv_px[(uint32_t)(py - lk_y0) * cv_w + (uint32_t)(px - lk_x0)] = swap16(sk.stip);
}
/* the stipple under a curve (the design's): every third column, dots 5 px apart up from `base` to 4 px under it */
static void lk_stip_under(int32_t x0, const float *ys, uint32_t n, float base, int32_t step)
{
    uint32_t i;
    for (i = 0; i < n; i += (uint32_t)step) {
        float y;
        int32_t r = 0;
        if (!lk_vis((float)(x0 + (int32_t)i), ys[i], (float)(x0 + (int32_t)i) + 5.0f, base + 2.0f))
            continue;
        for (y = base; y > ys[i] + 4.0f; y -= 5.0f, r++) {
            if (y >= (float)lk_y1)                  /* (below the window: the next up; above it: done) */
                continue;
            if (y + 1.2f < (float)lk_y0)
                break;
            lk_stip((float)(x0 + (int32_t)i) + (r & 1 ? 3.0f : 0.0f), y);
        }
    }
}

/* a triangle a b c, filled: each pixel by its distance inside the nearest edge */
static void lk_tri(float ax, float ay, float bx, float by, float cx, float cy, uint16_t c)
{
    float p[6] = {ax, ay, bx, by, cx, cy}, nx[3], ny[3], nd[3], area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    int32_t x, y, x0, x1, y0, y1, k;
    float sg = area < 0.0f ? -1.0f : 1.0f;
    for (k = 0; k < 3; k++) {                       /* each edge's inward unit normal */
        float ex = p[(2 * k + 2) % 6] - p[2 * k], ey = p[(2 * k + 3) % 6] - p[2 * k + 1], l = sy_sqrtf(ex * ex + ey * ey);
        nx[k] = -ey / l * sg;
        ny[k] = ex / l * sg;
        nd[k] = nx[k] * p[2 * k] + ny[k] * p[2 * k + 1];
    }
    lk_cols(sy_minf(ax, sy_minf(bx, cx)) - 1.0f, sy_maxf(ax, sy_maxf(bx, cx)) + 1.0f, &x0, &x1);
    lk_rows(sy_minf(ay, sy_minf(by, cy)) - 1.0f, sy_maxf(ay, sy_maxf(by, cy)) + 1.0f, &y0, &y1);
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            float px = (float)x + 0.5f, py = (float)y + 0.5f, d = 1e9f;
            for (k = 0; k < 3; k++)
                d = sy_minf(d, nx[k] * px + ny[k] * py - nd[k]);
            lk_cover(x, y, c, d + 0.5f);
        }
}

/* a capsule's outline (a toggle's track: the design's h 10 with round ends), stroke 1, from (x, y) w x h */
static void lk_capsule(float x, float y, float w, float h, uint16_t c)
{
    float r = 0.5f * h, cy = y + r, xa = x + r, xb = x + w - r;
    int32_t px, py, x0, x1, y0, y1;
    if (!lk_vis(x - 1.0f, y - 1.0f, x + w + 1.0f, y + h + 1.0f))
        return;
    lk_cols(x - 1.0f, x + w + 1.0f, &x0, &x1);
    lk_rows(y - 1.0f, y + h + 1.0f, &y0, &y1);
    for (py = y0; py < y1; py++)
        for (px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f, fy = (float)py + 0.5f - cy, qx = sy_clampf(fx, xa, xb) - fx;
            float d = sy_sqrtf(qx * qx + fy * fy);
            lk_cover(px, py, c, 1.0f - sy_absf(d - r));
        }
}

/* ------------------------------------------------------------------------------------------ text --- */
/* the faces by size and weight (gen_aa_font.py space-mono; V0.67: nothing under 9 px): T9 labels and captions, T9B
 * a handle's number and a row's figure, T10 a module's line, T10B the top line, the zones, chips and the toast, T12B
 * flags and values, T14B a module's word, T18B and T26B (sparse: a name's, a value's characters) */
enum { T9, T9B, T10, T10B, T12B, T14B, T18B, T26B, T_N };
static const aafont_t *const LK_FONT[T_N] = {&AF_XS, &AF_XB, &AF_S, &AF_SB, &AF_F, &AF_M, &AF_N, &AF_L};
static const uint8_t LK_FS[T_N] = {9, 9, 10, 10, 12, 14, 18, 26};

/* the emulator's check of the screens (web/emu/strata_web.c web_lint): each text drawn, and a mode that leaves the
 * text out (what is left: the drawing it must not touch) */
#ifdef LK_LINT
static void lk_lint_text(const int32_t *box, uint32_t t, const char *s, uint16_t fg, uint16_t bg);
static uint8_t lk_lint_notext, lk_lint_part;
#endif

/* s's width in face t with letter-spacing ls (thousandths of an em, after every character, the last's too: CSS's),
 * in 1/16 px */
/* ch's glyph in f: its own when the face has it (N and L: the capitals, and the v of a version), else gfx.c's (a
 * small letter as its capital, the ellipsis as '.', else '?') */
static uint32_t lk_glyph(const aafont_t *f, uint32_t ch)
{
    int32_t k = glyph_at(f, ch);
    return k >= 0 ? (uint32_t)k << f->psh : glyph(f, fold(f, ch));
}
static int32_t lk_tw16(uint32_t t, const char *s, int32_t ls)   /* (a monospace face: its space's advance each) */
{
    const aafont_t *f = LK_FONT[t];
    int32_t n = 0;
    while (s[n])
        n++;
    return n * (f->g[lk_glyph(f, ' ')].adv + ls * (int32_t)LK_FS[t] * 16 / 1000);
}
static float lk_tw(uint32_t t, const char *s, int32_t ls) { return (float)lk_tw16(t, s, ls) * (1.0f / 16.0f); }

/* s in face t from x (its box's left), its CSS line box's top y and line height lh (0: the face's 1.25 em rounded),
 * letter-spacing ls; fg on bg (what lies under it: the ramp's other end). Returns the x after it */
static float lk_text(float x, float y, uint32_t t, const char *s, uint16_t fg, uint16_t bg, int32_t ls, int32_t lh)
{
    const aafont_t *f = LK_FONT[t];
    const int32_t step = 16 >> f->psh, fs = LK_FS[t], tr = ls * fs * 16 / 1000;
    int32_t pen = sy_floori(x * 16.0f + 0.5f), err = 0, base, top;
    const uint16_t *rv;
    if (!lh)
        lh = (fs * 5 + 2) / 4;
    base = sy_floori((y + 0.5f * (float)lh + 0.3795f * (float)fs) + 0.5f);   /* (the baseline's row) */
    top = base - f->asc;
#ifdef LK_LINT
    const char *s0 = s;
    int32_t lb[4] = {9999, 9999, -9999, -9999};     /* (its ink's box on the screen) */
    if (lk_lint_notext)
        return x + (float)lk_tw16(t, s, ls) * (1.0f / 16.0f);
#endif
    if (top >= lk_y1 || top + f->h <= lk_y0 || x >= (float)lk_x1)
        return x + (float)lk_tw16(t, s, ls) * (1.0f / 16.0f);
    rv = ramp(fg, bg);
    for (; *s; s++) {
        const aag_t *g = &f->g[lk_glyph(f, (uint8_t)*s)];
        int32_t pos;
        if (f->psh && g[1].off != g->off) {        /* (gfx.c text_run's placement: the phase nearer the error) */
            int32_t d0, d1;
            pos = pen - (pen & (step - 1));
            d0 = pos - pen - err;
            d1 = d0 + step;
            if ((d0 < 0 ? -d0 : d0) > (d1 < 0 ? -d1 : d1))
                pos += step;
            g += ((uint32_t)pos >> (4u - f->psh)) & ((1u << f->psh) - 1u);
        } else {
            pos = (pen + 8) & ~15;
        }
        err = pos - pen;
        if (g->bw) {
            int32_t gx = (pos >> 4) + g->bx, gy = top + g->by;
#ifdef LK_LINT
            lb[0] = gx < lb[0] ? gx : lb[0];
            lb[1] = gy < lb[1] ? gy : lb[1];
            lb[2] = gx + g->bw > lb[2] ? gx + g->bw : lb[2];
            lb[3] = gy + g->bh > lb[3] ? gy + g->bh : lb[3];
#endif
            if (gx + g->bw > lk_x0 && gx < lk_x1 && gy + g->bh > lk_y0 && gy < lk_y1) {
                if (f->hc)
                    cv_alpha_hc(gx - lk_x0, gy, g->bw, g->bh, f->data + g->off, f->hc, rv);
                else
                    cv_alpha(gx - lk_x0, gy, g->bw, g->bh, f->data + g->off, rv);
            }
        }
        pen += g->adv + tr;
    }
#ifdef LK_LINT
    if (lb[2] > lb[0])
        lk_lint_text(lb, t, s0, fg, bg);
#endif
    return (float)pen * (1.0f / 16.0f);
}
/* .. its box ending at xr (right-aligned), or centred on xc */
static void lk_text_r(float xr, float y, uint32_t t, const char *s, uint16_t fg, uint16_t bg, int32_t ls, int32_t lh)
{
    lk_text(xr - lk_tw(t, s, ls), y, t, s, fg, bg, ls, lh);
}
static void lk_text_c(float xc, float y, uint32_t t, const char *s, uint16_t fg, uint16_t bg, int32_t ls, int32_t lh)
{
    lk_text(xc - 0.5f * lk_tw(t, s, ls), y, t, s, fg, bg, ls, lh);
}
/* s fitted in w px of face t (its end an ellipsis when longer), into d (n bytes) */
static const char *lk_fit(char *d, uint32_t n, const char *s, uint32_t t, int32_t ls, float w)
{
    uint32_t k = 0;
    while (s[k] && k + 1u < n) {
        d[k] = s[k];
        k++;
    }
    d[k] = 0;
    while (k > 1u && lk_tw(t, d, ls) > w) {
        d[--k] = 0;
        d[k - 1u] = ELLIPSIS;
    }
    return d;
}

/* ----------------------------------------------------------------------------------------- parts --- */
/* KNOB n's handle, its box's corner (x, y): a ring of 7 around (x + 5, y + 5) (V0.66 UI's of 5, grown about its
 * middle), the number in it, 9 px bold; hot (turning) filled */
#define LK_HR 7.0f
static void lk_handle(float x, float y, uint32_t n, int hot)
{
    char t[2] = {(char)('0' + n), 0};
    lk_disc(x + 5.0f, y + 5.0f, LK_HR, hot ? sk.hot : sk.ink);
    if (!hot)
        lk_disc(x + 5.0f, y + 5.0f, LK_HR - 1.2f, sk.ground);
#ifdef LK_LINT
    lk_lint_part = 1;
#endif
    lk_text_c(x + 5.5f, y - 1.0f, T9B, t, hot ? sk.ground : sk.ink, hot ? sk.hot : sk.ground, 0, 12);
#ifdef LK_LINT
    lk_lint_part = 0;
#endif
}
/* a chip at (x, y), 10 px bold in a box 16 high (5 px at the sides, a border): cur inverted. Returns the next one's x */
#define LK_CHIP_H 16.0f
static float lk_chip(float x, float y, const char *s, int cur)
{
    float tw = lk_tw(T10B, s, 20), w = tw + 10.0f;
    if (lk_vis(x, y, x + w, y + LK_CHIP_H)) {
        if (cur)
            lk_box(x, y, x + w, y + LK_CHIP_H, sk.ink);
        else
            lk_frame(x + 0.5f, y + 0.5f, w - 1.0f, LK_CHIP_H - 1.0f, 1.0f, sk.mid);
        lk_text(x + 5.0f, y + 2.0f, T10B, s, cur ? sk.ground : sk.lab, cur ? sk.ink : sk.ground, 20, 12);
    }
    return x + w + 4.0f;
}
/* a label: 9 px in LAB (V0.66 UI's 7 px in MID) */
static void lk_label(float x, float y, const char *s, int32_t ls) { lk_text(x, y, T9, s, sk.lab, sk.ground, ls, 0); }

/* the zones' row at the bottom (rows 223 .. 238): each zone's name, 10 px bold, the one shown inverted; where they do
 * not fit across 224 px, the others by their first three letters (then their first); five or more spread across */
#define LK_ZY 223.0f
static void lk_zones(const char *const *z, uint32_t n, uint32_t cur)
{
    float w[8], sum = 0.0f, gap, x = 8.0f;
    uint32_t i, cut = 0, k;
    char ab[8][4];
    if (!lk_vis(0.0f, LK_ZY, 240.0f, LK_ZY + 15.0f))
        return;
    for (k = 0; k < 3u; k++) {                      /* (k: whole names, three letters, one) */
        sum = 0.0f;
        cut = k == 0u ? 0u : k == 1u ? 3u : 1u;
        for (i = 0; i < n && i < 8u; i++) {
            uint32_t j;
            for (j = 0; j < 3u && z[i][j]; j++)
                ab[i][j] = z[i][j];
            ab[i][cut < j ? cut : j] = 0;
            sum += w[i] = lk_tw(T10B, i == cur || !cut ? z[i] : ab[i], 20) + 8.0f;
        }
        if (sum + 6.0f * (float)(n - 1u) <= 224.0f)
            break;
    }
    gap = n >= 5u ? (224.0f - sum) / (float)(n - 1u) : 8.0f;
    for (i = 0; i < n && i < 8u; i++) {
        if (i == cur)
            lk_box(x, LK_ZY, x + w[i], LK_ZY + 15.0f, sk.ink);
        lk_text(x + 4.0f, LK_ZY + 1.5f, T10B, i == cur || !cut ? z[i] : ab[i], i == cur ? sk.ground : sk.lab,
                i == cur ? sk.ink : sk.ground, 20, 12);
        x += w[i] + gap;
    }
}
/* a word for a moment in the zones' place: a HOT dot, then the word (10 px bold) */
static void lk_toast(const char *s)
{
    char t[48];
    if (!lk_vis(0.0f, LK_ZY, 240.0f, LK_ZY + 15.0f))
        return;
    lk_disc(11.0f, LK_ZY + 7.5f, 3.0f, sk.hot);
    lk_text(19.0f, LK_ZY + 1.5f, T10B, lk_fit(t, sizeof t, s, T10B, 0, 216.0f), sk.ink, sk.ground, 0, 12);
}
/* the top line: the label (on: -1 no dot, 0 off, 1 on: its dot before it), LAB, 10 px bold; at the right n module
 * dots (cur filled) or nothing; a right-aligned word */
static void lk_top(const char *label, int on, uint32_t n, uint32_t cur, const char *right)
{
    uint32_t i;
    if (!lk_vis(0.0f, 0.0f, 240.0f, 16.0f))
        return;
    if (on > 0)
        lk_disc(10.5f, 9.0f, 3.0f, sk.ink);
    else if (!on)
        lk_ring(10.5f, 9.0f, 2.5f, 1.0f, sk.lab);
    if (label)
        lk_text(on >= 0 ? 18.0f : 8.0f, 2.0f, T10B, label, sk.lab, sk.ground, 30, 13);
    for (i = 0; i < n; i++) {
        float x = 236.0f - (float)(n * 8u) + (float)(i * 8u) + 2.5f;
        if (i == cur)
            lk_disc(x, 9.0f, 3.0f, sk.ink);
        else
            lk_ring(x, 9.0f, 2.5f, 1.0f, sk.lab);
    }
    if (right)                                      /* (left of the dots, if any) */
        lk_text_r(n ? 232.0f - (float)(n * 8u) - 4.0f : 232.0f, 2.0f, T10B, right, sk.lab, sk.ground, 30, 13);
}
/* the chain's eight squares at the top right: the module shown filled, the others in the chain outlined (bypassed:
 * dashed), the places after LINE */
static void lk_chain_sq(uint32_t n, int32_t cur, uint32_t bypassed)
{
    uint32_t i;
    for (i = 0; i < 8u; i++) {
        float x = 172.0f + (float)(i * 8u);
        if ((int32_t)i == cur)
            lk_box(x, 6.0f, x + 5.0f, 11.0f, sk.ink);
        else if (i < n && (bypassed >> i & 1u))
            lk_frame_dash(x + 0.5f, 6.5f, 4.0f, 4.0f, 2.0f, 2.0f, sk.lab);
        else
            lk_frame(x + 0.5f, 6.5f, 4.0f, 4.0f, 1.0f, i < n ? sk.ink : sk.line);
    }
}
/* a layer's banner: INK across the top, its name and how it ends in GROUND */
static void lk_banner(const char *a, const char *b)
{
    if (!lk_vis(0.0f, 0.0f, 240.0f, 16.0f))
        return;
    lk_fill(0, 0, 240, 17, sk.ink);
    lk_text(8.0f, 2.0f, T10B, a, sk.ground, sk.ink, 30, 13);
    if (b)
        lk_text_r(232.0f, 2.5f, T9, b, sk.ground, sk.ink, 0, 13);
}
/* the mix row every effect shares: MIX at the left, the bar from 32 to 228 at y 200 (v 0..1); returns the knob's x */
static float lk_mix(float v)
{
    float x = 32.0f + 196.0f * sy_clampf(v, 0.0f, 1.0f);
    lk_seg(32.0f, 200.0f, 228.0f, 200.0f, 3.0f, sk.line);
    if (x > 32.5f)
        lk_seg(32.0f, 200.0f, x, 200.0f, 3.0f, sk.ink);
    lk_box(x - 2.5f, 197.5f, x + 2.5f, 202.5f, sk.ink);
    lk_label(4.0f, 194.5f, "MIX", 0);
    return x;
}
/* a fader at x from y0 (top) to y1, at v (0..1): its track, the fill below, a hollow cap; returns the cap's y */
static float lk_fader(float x, float y0, float y1, float v)
{
    float y = y1 - (y1 - y0) * sy_clampf(v, 0.0f, 1.0f);
    lk_seg(x, y0, x, y1, 3.0f, sk.line);
    lk_seg(x, y, x, y1, 3.0f, sk.ink);
    lk_box(x - 6.6f, y - 3.6f, x + 6.6f, y + 3.6f, sk.ink);
    lk_box(x - 5.4f, y - 2.4f, x + 5.4f, y + 2.4f, sk.ground);
    return y;
}
/* a small hollow mark: GROUND inside an INK ring (r, stroke 1.2): the design's points and joints */
static void lk_hollow(float cx, float cy, float r)
{
    lk_disc(cx, cy, r + 0.6f, sk.ink);
    lk_disc(cx, cy, r - 0.6f, sk.ground);
}
/* .. square */
static void lk_hollow_sq(float cx, float cy, float r)
{
    lk_box(cx - r - 0.6f, cy - r - 0.6f, cx + r + 0.6f, cy + r + 0.6f, sk.ink);
    lk_box(cx - r + 0.6f, cy - r + 0.6f, cx + r - 0.6f, cy + r - 0.6f, sk.ground);
}

/* the FM-1's 27 keys, F3 to G5, as the design's K.kb: 16 white keys across w from x0, the black ones 0.62 of a white
 * one wide and 0.6 of the height down. Key k (0 F3 .. 26 G5): black or white, its white index, its box */
static const uint8_t LK_BLACK[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};   /* from F */
typedef struct { float x0, y0, ww, bw, bh, h; } lk_kb_t;
static void lk_kb_at(const lk_kb_t *kb, uint32_t k, float *x, float *w, int *black)
{
    uint32_t i, wi = 0;
    for (i = 0; i < k; i++)
        wi += !LK_BLACK[i % 12u];
    *black = LK_BLACK[k % 12u];
    if (*black) {
        *x = kb->x0 + (float)wi * kb->ww - 0.5f * kb->bw;
        *w = kb->bw;
    } else {
        *x = kb->x0 + (float)wi * kb->ww;
        *w = kb->ww;
    }
}
static float lk_kb_cx(const lk_kb_t *kb, uint32_t k)   /* key k's middle */
{
    float x, w;
    int b;
    lk_kb_at(kb, k, &x, &w, &b);
    return x + 0.5f * w;
}
static void lk_kb(lk_kb_t *kb, float x0, float y0, float w, float h)
{
    uint32_t k;
    kb->x0 = x0;
    kb->y0 = y0;
    kb->ww = w / 16.0f;
    kb->bw = kb->ww * 0.62f;
    kb->bh = h * 0.6f;
    kb->h = h;
    if (!lk_vis(x0 - 1.0f, y0 - 1.0f, x0 + w + 1.0f, y0 + h + 1.0f))
        return;
    for (k = 0; k < 16u; k++)
        lk_frame(x0 + (float)k * kb->ww, y0, kb->ww, h, 1.0f, sk.ink);
    for (k = 0; k < 27u; k++) {
        float x, bw;
        int b;
        lk_kb_at(kb, k, &x, &bw, &b);
        if (b)
            lk_box(x - 0.5f, y0 - 0.5f, x + bw + 0.5f, y0 + kb->bh + 0.5f, sk.ink);
    }
}

/* the effects' glyphs (the design's K.icons), 10 x 7, a bit a pixel, row by row, the first bit the top left's: by the
 * effect's type (sy_param.c FXT_*); traced from its paths a pixel a point (Bresenham), CHORUS drawn by hand */
static const uint8_t LK_ICON[13][9] = {
    {0x00, 0x1E, 0x04, 0x82, 0x11, 0x04, 0x40, 0x90, 0x3C},   /* DRIVE */
    {0x00, 0x1C, 0x08, 0x84, 0x1E, 0x70, 0x22, 0x10, 0x78},   /* CHORUS */
    {0x40, 0x10, 0x04, 0x81, 0x20, 0x49, 0x12, 0x54, 0x94},   /* DELAY */
    {0x00, 0x00, 0x81, 0x00, 0x02, 0x00, 0x11, 0x01, 0x04},   /* REVERB */
    {0x00, 0x3F, 0xF4, 0x45, 0x11, 0x44, 0x51, 0x14, 0x44},   /* OTT */
    {0x00, 0x06, 0x13, 0xC4, 0x93, 0xE3, 0xA0, 0x00, 0x00},   /* EQ */
    {0x00, 0x00, 0x00, 0x03, 0x18, 0xA5, 0x29, 0x4B, 0xDC},   /* FLANGER */
    {0x00, 0x37, 0xB5, 0x29, 0xCE, 0x21, 0x08, 0x42, 0x10},   /* PHASER */
    {0x00, 0x00, 0x0F, 0xE0, 0x0C, 0x01, 0x00, 0x40, 0x0C},   /* FILTER */
    {0x01, 0xC0, 0x40, 0x70, 0x10, 0x3C, 0x08, 0x0E, 0x00},   /* BITCRUSH */
    {0x00, 0x30, 0x0E, 0x62, 0xB5, 0x38, 0xC6, 0x00, 0x00},   /* TREMOLO */
    {0x00, 0x08, 0x21, 0x13, 0xFF, 0x0E, 0x04, 0x42, 0x08},   /* RING MOD */
    {0x00, 0x3E, 0xF8, 0xA6, 0x29, 0x8A, 0x7E, 0xF0, 0xE0},   /* TAPE */
};
static void lk_icon(uint32_t type, float x, float y, uint16_t c)
{
    uint32_t i;
    if (type >= 13u || !lk_vis(x, y, x + 10.0f, y + 7.0f))
        return;
    for (i = 0; i < 70u; i++)
        if (LK_ICON[type][i >> 3] >> (7u - (i & 7u)) & 1u)
            lk_fill(sy_floori(x) + (int32_t)(i % 10u), sy_floori(y) + (int32_t)(i / 10u), 1, 1, c);
}
