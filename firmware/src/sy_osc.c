/* SPDX-License-Identifier: GPL-3.0-only */
/* STRATA wavetable oscillators (docs/ARCHITECTURE.md §7): the kernels, the warps, unison.
 * A unison voice reads two adjacent frames of its table at one mip level, interpolates each linearly by the
 * phase, crossfades them by the position's fraction, and adds itself into the oscillator's left and right
 * (int32, Q15 gains). The increment, the crossfade and the gains ramp linearly across the block (pitch, position
 * and unison modulation stay smooth); a position that crosses a frame boundary inside the block splits the block
 * there. Warps change the phase that is read: SYNC BEND PWM ASYM MIRROR through a 65-point map of the phase,
 * built when the amount changes (piecewise linear, rebuilt a block only while the amount moves); FM adds the
 * other oscillator's output to the phase (phase modulation). The level comes from the increment times the warp's
 * steepest slope: a warp never makes a table alias. Levels change with hysteresis (a semitone), and a change
 * crossfades the two levels over the block. Integer only inside the loops (the most repeated code: voices x 2 x
 * unison), int16 tables, uint32 phases (2^32 = a cycle). */
#define OSC_NMAP 64u                       /* warp map segments */
#define OSC_SMAX 15.0f                     /* a warp's steepest slope (a segment must span < half a cycle) */
enum { WARP_OFF, WARP_SYNC, WARP_BENDP, WARP_BENDM, WARP_PWM, WARP_ASYMP, WARP_ASYMM, WARP_MIRROR, WARP_FM,
       WARP_COUNT };
static const char *const WARP_NAMES[WARP_COUNT] = {"OFF", "SYNC", "BEND+", "BEND-", "PWM", "ASYM+", "ASYM-",
                                                   "MIRROR", "FM"};

typedef struct {                           /* a kernel call: one unison voice over n samples */
    const int16_t *a;                      /* the level in frame k (frame k + 1's is WT_FRAME further on) */
    uint32_t bits;                         /* log2 of the level's samples */
    uint32_t ph, inc;                      /* phase, increment */
    int32_t dinc;                          /* the increment's step a sample */
    int32_t xf, dxf;                       /* the crossfade k -> k + 1 (Q15, 0..32768) and its step */
    int32_t gl, gr, dgl, dgr;              /* gains (Q15) and their steps */
    const uint32_t *map;                   /* warp map (OSC_NMAP + 1 phases) */
    const int32_t *pm;                     /* FM: the modulator (Q15, every other int32: an interleaved buffer's left) */
    uint32_t pmd;                          /* FM: phase units per Q15 unit of the modulator */
    uint32_t wt, wk;                       /* SYNC window: its width (phase >> 17), 32767 / width (Q15) */
    int32_t wd;                            /* SYNC window: depth, Q15 */
} osc_job_t;

/* ---------------------------------------------------------------------------------------- kernels --- */
/* The pi32v2 has 16 registers and these loops need most of them (the target listing: tools/costprobe). So: the
 * output is one interleaved buffer (left, right: one pointer), one shift gives the index and the fraction
 * together, frame k + 1 is frame k's pointer plus WT_FRAME (frames are contiguous in the pool; a table of one
 * frame reads the pool's next frame with a crossfade of 0), and each kernel comes four times: gains held (the usual
 * block) or ramping (_g: unison, level and fade changes), and two frames crossfaded or one (1: the position on a
 * frame for the whole run, as at either end, at a keyframe or in a table of one frame: what the crossfade of 0 or 1
 * gives exactly, for 7 instructions less), one a line (tools/costprobe counts by line). */
#define OSC_SETUP                                                                                         \
    const int16_t *a = j->a, *b = a + WT_FRAME;                                                           \
    uint32_t shf = 17u - j->bits, ph = j->ph, inc = j->inc;                                               \
    int32_t dinc = j->dinc, xf = j->xf, dxf = j->dxf, gl = j->gl, gr = j->gr
#define OSC_READ(q)                                                                                       \
    uint32_t x = (q) >> shf, i = x >> 15;                                                                 \
    int32_t fr = (int32_t)(x & 0x7FFFu), a0 = a[i];                                                       \
    int32_t s = a0 + (((a[i + 1u] - a0) * fr) >> 15);                                                     \
    if (f2) {                                                                                             \
        int32_t b0 = b[i], sb = b0 + (((b[i + 1u] - b0) * fr) >> 15);                                     \
        s += ((sb - s) * xf) >> 15;                                                                       \
    }
#define OSC_OUT                                                                                           \
    o[0] += (s * gl) >> 15;                                                                               \
    o[1] += (s * gr) >> 15;                                                                               \
    o += 2;                                                                                               \
    ph += inc;                                                                                            \
    inc += (uint32_t)dinc;                                                                                \
    xf += dxf
/* the phase through the warp map: 64 segments, a 16-bit fraction (the segment's span >> 16: 1.5e-5 cycle) */
#define OSC_MAP(q)                                                                                        \
    uint32_t sg = ph >> 26, w0 = map[sg];                                                                 \
    uint32_t q = w0 + (uint32_t)(((int32_t)(map[sg + 1u] - w0) >> 16) * (int32_t)((ph >> 10) & 0xFFFFu))
#define OSC_KERNEL(NAME, RAMP, F2, DECL, BODY)                                                            \
    static uint32_t NAME(int32_t *restrict o, uint32_t n, const osc_job_t *j)                             \
    {                                                                                                     \
        const int f2 = F2;                                                                                \
        int32_t dgl = j->dgl, dgr = j->dgr;                                                               \
        DECL;                                                                                             \
        OSC_SETUP;                                                                                        \
        do {                                                                                              \
            BODY;                                                                                         \
            OSC_OUT;                                                                                      \
            if (RAMP) {                                                                                   \
                gl += dgl;                                                                                \
                gr += dgr;                                                                                \
            }                                                                                             \
        } while (--n);                                                                                    \
        return ph;                                                                                        \
    }

/* the phase as it is */
#define OSC_D_PLAIN do {} while (0)
#define OSC_B_PLAIN OSC_READ(ph)
OSC_KERNEL(osc_k_plain, 0, 1, OSC_D_PLAIN, OSC_B_PLAIN)
OSC_KERNEL(osc_k_plain_g, 1, 1, OSC_D_PLAIN, OSC_B_PLAIN)
OSC_KERNEL(osc_k_plain1, 0, 0, OSC_D_PLAIN, OSC_B_PLAIN)
OSC_KERNEL(osc_k_plain1_g, 1, 0, OSC_D_PLAIN, OSC_B_PLAIN)

/* through the warp map */
#define OSC_D_MAP const uint32_t *map = j->map
#define OSC_B_MAP OSC_MAP(q); OSC_READ(q)
OSC_KERNEL(osc_k_map, 0, 1, OSC_D_MAP, OSC_B_MAP)
OSC_KERNEL(osc_k_map_g, 1, 1, OSC_D_MAP, OSC_B_MAP)
OSC_KERNEL(osc_k_map1, 0, 0, OSC_D_MAP, OSC_B_MAP)
OSC_KERNEL(osc_k_map1_g, 1, 0, OSC_D_MAP, OSC_B_MAP)

/* SYNC: the map, and a window that dips the output where the master resets (deep only as far as the slave is
 * from a whole cycle there: at a whole ratio the reset is already continuous) */
#define OSC_D_SYNC const uint32_t *map = j->map; uint32_t wt = j->wt, wk = j->wk; int32_t wd = j->wd
#define OSC_B_SYNC                                                                                        \
    uint32_t e = (ph < 0x80000000u ? ph : 0u - ph) >> 17;                                                 \
    OSC_MAP(q);                                                                                           \
    OSC_READ(q);                                                                                          \
    if (e < wt)                                                                                           \
        s = (s * (32767 - ((wd * (32767 - (int32_t)((e * wk) >> 15))) >> 15))) >> 15
OSC_KERNEL(osc_k_sync, 0, 1, OSC_D_SYNC, OSC_B_SYNC)
OSC_KERNEL(osc_k_sync_g, 1, 1, OSC_D_SYNC, OSC_B_SYNC)
OSC_KERNEL(osc_k_sync1, 0, 0, OSC_D_SYNC, OSC_B_SYNC)
OSC_KERNEL(osc_k_sync1_g, 1, 0, OSC_D_SYNC, OSC_B_SYNC)

/* FM: phase modulation by the other oscillator */
#define OSC_D_PM const int32_t *pm = j->pm; uint32_t pmd = j->pmd
#define OSC_B_PM uint32_t q = ph + (uint32_t)*pm * pmd; pm += 2; OSC_READ(q)
OSC_KERNEL(osc_k_pm, 0, 1, OSC_D_PM, OSC_B_PM)
OSC_KERNEL(osc_k_pm_g, 1, 1, OSC_D_PM, OSC_B_PM)
OSC_KERNEL(osc_k_pm1, 0, 0, OSC_D_PM, OSC_B_PM)
OSC_KERNEL(osc_k_pm1_g, 1, 0, OSC_D_PM, OSC_B_PM)

typedef uint32_t (*osc_kernel_t)(int32_t *restrict, uint32_t, const osc_job_t *);
enum { OSK_PLAIN, OSK_MAP, OSK_SYNC, OSK_PM };
static const osc_kernel_t OSC_KS[4][2][2] = {      /* [kind][gains ramping][two frames] */
    {{osc_k_plain1, osc_k_plain}, {osc_k_plain1_g, osc_k_plain_g}}, {{osc_k_map1, osc_k_map}, {osc_k_map1_g, osc_k_map_g}},
    {{osc_k_sync1, osc_k_sync}, {osc_k_sync1_g, osc_k_sync_g}}, {{osc_k_pm1, osc_k_pm}, {osc_k_pm1_g, osc_k_pm_g}}};

/* -------------------------------------------------------------------------------------------- state --- */
typedef struct {                           /* an oscillator of a voice */
    uint32_t ph[SY_NUNI];                  /* unison phases */
    uint32_t ratio[SY_NUNI];               /* detune ratios, Q30 */
    float x[SY_NUNI];                      /* each voice's place in the spread, -1..1 */
    int16_t gl[SY_NUNI], gr[SY_NUNI];      /* gains at the end of the last block (Q15) */
    int16_t tl[SY_NUNI], tr[SY_NUNI];      /* the gains BLEND and WIDTH give (Q15) */
    uint8_t lev[SY_NUNI];                  /* the mip level each plays (WT_NLEV: silent, too high) */
    uint8_t n;                             /* unison voices sounding */
    uint8_t c_n, c_mode, c_wmode, c_gn;    /* what ratio / x, the map and tl / tr were made for */
    float c_det, c_wamt, c_blend, c_width;
    uint32_t inc;                          /* the centre increment at the end of the last block */
    float pos;                             /* the position at the end of the last block (frames) */
    uint32_t map[OSC_NMAP + 1u];           /* the warp map */
    float mslope;                          /* its steepest slope */
    float c_dc, c_dcpos;                   /* the warped cycle's mean (int16 units), for this position .. */
    uint8_t c_dclv;                        /* .. and level (0xFF: to be worked out) */
    float slope;                           /* this block's harmonic stretch (the level's) */
    uint8_t fresh;                         /* the first block of a note: no ramps from old values */
} osc_v_t;

typedef struct {                           /* what a block of an oscillator plays (sy_voice.c fills it) */
    uint32_t inc0, inc1;                   /* the centre increment at the block's start (a new note) and end */
    float pos;                             /* position 0..1 (modulated), at the block's end */
    uint8_t warp;
    float wamt;                            /* warp amount 0..1 */
    uint8_t nuni, umode;                   /* unison voices (after the budget), LINEAR / SUPER */
    float det, blend, width, spread;       /* 0..1 */
    const int32_t *pm;                     /* FM: the other oscillator's output (Q15), or 0 */
    uint32_t pm_inc;                       /* .. and its increment (for the level) */
} osc_par_t;

/* a phase 0..1 (1 included) as a uint32 phase: no float -> unsigned conversion above 2^31 (the pi32v2 has none) */
static inline uint32_t osc_phase(float p)
{
    if (p >= 1.0f)
        return 0u;
    return (uint32_t)(int32_t)((p - 0.5f) * 4294967296.0f) + 0x80000000u;
}

/* --------------------------------------------------------------------------------------- unison --- */
/* detune ratios and places: N voices over -1..1 (LINEAR even; SUPER denser near the centre), +-DETUNE cents
 * at the ends (150 cents at 100 %, 9 at 25 %: the knob's square) */
static void osc_uni_detune(osc_v_t *o, uint32_t n, uint32_t mode, float det)
{
    float cents = 150.0f * det * det;
    uint32_t j;
    for (j = 0; j < n; j++) {
        float x = n > 1u ? (float)(2u * j) / (float)(n - 1u) - 1.0f : 0.0f;
        if (mode)
            x *= 0.3f + 0.7f * x * x;
        o->x[j] = x;
        o->ratio[j] = (uint32_t)(int32_t)(sy_exp2f(cents * x * (1.0f / 1200.0f)) * 1073741824.0f);
    }
    o->c_n = (uint8_t)n;
    o->c_mode = (uint8_t)mode;
    o->c_det = det;
    o->c_gn = 0;                               /* the gains follow the places */
}

/* the unison voices' gains: the centre voice (two with an even N) at 1, the others at BLEND, equal power over
 * all of them; WIDTH spreads them over the stereo field, from the centre out in pairs alternating left / right */
static void osc_uni_gains(osc_v_t *o, uint32_t n, float blend, float width)
{
    uint32_t j, nc = (n & 1u) ? 1u : 2u, half = n >> 1;
    float bs = n <= 2u ? 1.0f : blend, norm;
    norm = sy_exp2f(-0.5f * sy_log2f((float)nc + (float)(n - nc) * bs * bs));
    for (j = 0; j < n; j++) {
        uint32_t q = (n & 1u) ? (j > half ? j - half : half - j) : (j >= half ? j - half : half - 1u - j);
        int centre = (n & 1u) ? j == half : q == 0u;
        float x = o->x[j], g = (centre ? 1.0f : bs) * norm;
        float pan = width * sy_absf(x) * (((x > 0.0f) ^ (q & 1u)) ? 1.0f : -1.0f);
        float th = (pan + 1.0f) * 0.125f;      /* turns: 0 .. 1/4, equal power */
        o->tl[j] = (int16_t)(g * sy_cos2pi(th) * 32767.0f);
        o->tr[j] = (int16_t)(g * sy_sin2pi(th) * 32767.0f);
    }
    o->c_gn = (uint8_t)n;
    o->c_blend = blend;
    o->c_width = width;
}

/* ---------------------------------------------------------------------------------------- warps --- */
/* the phase a warp reads at output phase p (0..1), and its steepest slope: what the oscillators' map holds
 * (osc_warp_map) and what the screen's 2D view draws (sy_ui_graph.c). FM is no map (q = p) */
static float osc_warp_q(uint32_t mode, float amt, float p, float *slope)
{
    switch (mode) {
    case WARP_SYNC: {                                  /* the slave at r times the master: 1 .. 15 */
        float r = sy_exp2f(3.9069f * amt), x = p * r;
        *slope = r;
        return x - (float)sy_floori(x);
    }
    case WARP_BENDP:
    case WARP_BENDM: {                                 /* symmetric about the centre: slow there, fast at the ends */
        float g = sy_exp2f(3.0f * amt), k = mode == WARP_BENDP ? g : 1.0f / g;
        float u = sy_absf(2.0f * p - 1.0f), v = u / (u + (1.0f - u) * k);
        *slope = g;
        return 0.5f + (p < 0.5f ? -0.5f : 0.5f) * v;
    }
    case WARP_PWM: {                                   /* the cycle squeezed into w, its last sample held */
        float w = 1.0f - (1.0f - 1.0f / OSC_SMAX) * amt;
        *slope = 1.0f / w;
        return sy_minf(p / w, 1.0f);
    }
    case WARP_ASYMP:
    case WARP_ASYMM: {
        float g = sy_exp2f(3.0f * amt), k = mode == WARP_ASYMP ? g : 1.0f / g;
        *slope = g;
        return p / (p + (1.0f - p) * k);
    }
    case WARP_MIRROR: {                                /* forward to the fold point, back after it */
        float m = 0.5f - (0.5f - 1.0f / OSC_SMAX) * amt;
        *slope = 1.0f / m;
        return p < m ? p / m : (1.0f - p) / (1.0f - m);
    }
    }
    *slope = 1.0f;
    return p;
}

static void osc_warp_map(osc_v_t *o, uint32_t mode, float amt)
{
    uint32_t i;
    float slope = 1.0f;
    for (i = 0; i <= OSC_NMAP; i++) {
        if (mode == WARP_SYNC) {                       /* (in fixed point: the wraps exact) */
            float r = sy_exp2f(3.9069f * amt);
            uint32_t rq = (uint32_t)(r * 65536.0f);
            o->map[i] = (uint32_t)(((uint64_t)i << 26) * rq >> 16);
            slope = r;
            continue;
        }
        o->map[i] = osc_phase(osc_warp_q(mode, amt, (float)i * (1.0f / (float)OSC_NMAP), &slope));
    }
    o->mslope = slope;
    o->c_dclv = 0xFF;
    o->c_wmode = (uint8_t)mode;
    o->c_wamt = amt;
}

/* ------------------------------------------------------------------------------------------ notes --- */
/* a note starts on a voice: phases (retrig: PHASE plus RAND's share of a random cycle; else as they are),
 * levels, no ramps from the voice's last note */
static void osc_note(osc_v_t *o, int retrig, float phase, float rand, uint32_t *seed)
{
    uint32_t j;
    for (j = 0; j < SY_NUNI; j++) {
        if (retrig)
            o->ph[j] = osc_phase(phase) + (uint32_t)(int32_t)(rand * (float)(sy_rand(seed) >> 9)) * 512u;
        o->lev[j] = 0xFF;
        o->gl[j] = o->gr[j] = 0;
    }
    o->fresh = 1;
}

/* ------------------------------------------------------------------------------------------ block --- */
/* the level for an increment through a warp of slope s: the warp's harmonics never alias (a strong warp on a
 * high note falls to level 8, the sine); a fundamental too high for level 8 is silent (WT_NLEV) */
static uint32_t osc_level(uint32_t inc, float s)
{
    float e = (float)inc * s;
    uint32_t l;
    if (inc > WT_INC_L0 << (WT_NLEV - 1u))                 /* (wt_level(inc) == WT_NLEV) */
        return WT_NLEV;
    if (e >= 2147483648.0f)
        return WT_NLEV - 1u;
    l = wt_level((uint32_t)(int32_t)e);
    return l < WT_NLEV ? l : WT_NLEV - 1u;
}

/* one unison voice over a block at one level: position p .. p + dp SY_BLK (frames), split where it crosses a
 * frame; jb holds the phase, increments and gains at the block's start. Returns the phase at its end */
static uint32_t osc_run(uint32_t kind, osc_job_t *jb, const wt_table_t *t, uint32_t lv, float p, float dp,
                        int32_t *lr)
{
    uint32_t done = 0, nf = t->n, two;
    int32_t inc0 = (int32_t)jb->inc, gl0 = jb->gl, gr0 = jb->gr;
    const int32_t *pm0 = jb->pm;
    jb->bits = WT_BITS[lv];
    while (done < SY_BLK) {
        uint32_t m = SY_BLK - done, kf = 0;
        float xf0 = 0.0f, xf1 = 0.0f;
        if (nf >= 2u) {
            int32_t ki = sy_floori(p);
            if (dp < 0.0f && (float)ki == p)
                ki--;                                  /* moving down from a frame: the pair below it */
            ki = sy_clampi(ki, 0, (int32_t)nf - 2);
            if (dp > 0.0f && ki < (int32_t)nf - 2 && p + dp * (float)m > (float)(ki + 1))
                m = (uint32_t)(((float)(ki + 1) - p) / dp) + 1u;
            else if (dp < 0.0f && ki > 0 && p + dp * (float)m < (float)ki)
                m = (uint32_t)((p - (float)ki) / -dp) + 1u;
            if (m > SY_BLK - done)
                m = SY_BLK - done;
            kf = (uint32_t)ki;
            xf0 = sy_clampf(p - (float)ki, 0.0f, 1.0f);
            xf1 = sy_clampf(p + dp * (float)m - (float)ki, 0.0f, 1.0f);
        }
        jb->xf = (int32_t)(xf0 * 32768.0f);
        jb->dxf = (int32_t)((xf1 - xf0) * 32768.0f) / (int32_t)m;
        two = jb->dxf != 0 || (jb->xf != 0 && jb->xf != 32768);     /* (else the position is on frame kf or kf + 1) */
        jb->a = wt_frame(t, kf + (!two && jb->xf != 0), lv);
        jb->ph = OSC_KS[kind][(jb->dgl | jb->dgr) != 0][two](lr + 2u * done, m, jb);
        done += m;
        jb->inc = (uint32_t)(inc0 + jb->dinc * (int32_t)done);
        jb->gl = gl0 + jb->dgl * (int32_t)done;
        jb->gr = gr0 + jb->dgr * (int32_t)done;
        jb->pm = pm0 ? pm0 + 2u * done : 0;
        p += dp * (float)m;
    }
    jb->pm = pm0;
    return jb->ph;
}

/* a warp's amount held to what the note allows: its steepest slope times the note (its highest unison voice)
 * stays at or under 21 kHz, so the stretched harmonics never pass it. A strong warp on a high note gives way
 * gradually instead of aliasing */
static float osc_amt_cap(uint32_t mode, float amt, uint32_t inc)
{
    float maxs = 2044404224.0f / ((float)(inc ? inc : 1u) * 1.0905f), cap;   /* (+150 cents of detune) */
    switch (mode) {
    case WARP_SYNC: cap = maxs > 1.0f ? sy_log2f(maxs) * (1.0f / 3.9069f) : 0.0f; break;
    case WARP_PWM: cap = (1.0f - 1.0f / sy_maxf(maxs, 1.0f)) / (1.0f - 1.0f / OSC_SMAX); break;
    case WARP_MIRROR: cap = (0.5f - 1.0f / sy_maxf(maxs, 1.0f)) / (0.5f - 1.0f / OSC_SMAX); break;
    default: cap = maxs > 1.0f ? sy_log2f(maxs) * (1.0f / 3.0f) : 0.0f; break;   /* BEND, ASYM */
    }
    return sy_clampf(sy_minf(amt, cap), 0.0f, 1.0f);
}

/* the mean of a warped cycle (int16 units): a warp spends more time on some parts of the wave than others, and
 * its output takes an offset an envelope would turn into a thump. BEND ASYM MIRROR: the 64 map phases read on a
 * coarse level (6 or above: a few harmonics, whose warped copy stays under 64 harmonics, so a 64-point mean is
 * close); SYNC: 1/r of the cycle's integral up to frac(r). The frames crossfaded at pos. Kept until the map, the
 * position (by 1/64 frame) or the level changes: a warp that is not moving costs nothing here */
static float osc_warp_dc(osc_v_t *o, const wt_table_t *t, uint32_t mode, float pos, uint32_t lv)
{
    uint32_t nf = t->n, i, bits;
    int32_t kf;
    float xf, s = 0.0f;
    const int16_t *fa, *fb;
    if (lv < 6u)
        lv = 6u;
    if (lv >= WT_NLEV)
        lv = WT_NLEV - 1u;
    if (o->c_dclv == lv && sy_absf(pos - o->c_dcpos) < 0.015625f)
        return o->c_dc;
    kf = nf >= 2u ? sy_clampi(sy_floori(pos), 0, (int32_t)nf - 2) : 0;
    xf = nf >= 2u ? pos - (float)kf : 0.0f;
    fa = wt_frame(t, (uint32_t)kf, lv);
    fb = nf >= 2u ? wt_frame(t, (uint32_t)kf + 1u, lv) : fa;
    bits = WT_BITS[lv];
    if (mode == WARP_SYNC) {
        uint32_t n = 1u << bits, end = (uint32_t)((o->mslope - (float)sy_floori(o->mslope)) * (float)n);
        for (i = 0; i < end; i++)
            s += (float)fa[i] + (float)(fb[i] - fa[i]) * xf;
        s /= (float)n * o->mslope;
    } else {
        for (i = 0; i < OSC_NMAP; i++) {
            uint32_t q = o->map[i], ix = q >> (32u - bits);
            float fr = (float)((q << bits) >> 8) * (1.0f / 16777216.0f);
            float a = (float)fa[ix] + (float)(fa[ix + 1u] - fa[ix]) * fr, b = (float)fb[ix] + (float)(fb[ix + 1u] - fb[ix]) * fr;
            s += a + (b - a) * xf;
        }
        s *= 1.0f / (float)OSC_NMAP;
    }
    o->c_dc = s;
    o->c_dcpos = pos;
    o->c_dclv = (uint8_t)lv;
    return s;
}

/* the kernel and its settings for a block of an oscillator, playing warp (pp's, or FM's modulator's) */
static uint32_t osc_setup(osc_v_t *o, const osc_par_t *pp, uint32_t warp, osc_job_t *jb)
{
    float amt;
    jb->map = o->map;
    jb->pm = 0;
    jb->pmd = 0;
    o->slope = 1.0f;
    if (warp == WARP_OFF || (warp == WARP_FM && !pp->pm))
        return OSK_PLAIN;
    if (warp == WARP_FM) {                         /* +-2 cycles at 100 %, held under 21 kHz as the others */
        float dc = 2.0f * pp->wamt * pp->wamt, r = (float)pp->pm_inc / (float)(pp->inc1 ? pp->inc1 : 1u);
        float maxs = 2044404224.0f / ((float)(pp->inc1 ? pp->inc1 : 1u) * 1.0905f);
        dc = sy_minf(dc, sy_maxf(0.0f, (maxs - 1.0f) / (6.2831853f * sy_maxf(r, 1e-3f))));
        jb->pm = pp->pm;
        jb->pmd = (uint32_t)(dc * 131072.0f);
        o->slope = 1.0f + 6.2831853f * dc * r;
        return OSK_PM;
    }
    amt = osc_amt_cap(warp, pp->wamt, pp->inc1);
    if (warp != o->c_wmode || sy_absf(amt - o->c_wamt) > 1e-4f)
        osc_warp_map(o, warp, amt);
    o->slope = o->mslope;
    if (warp == WARP_SYNC) {
        float rr = o->mslope, fr = rr - (float)sy_floori(rr), d = 4.0f * sy_minf(fr, 1.0f - fr);
        uint32_t w = pp->inc1 < 0x20000000u ? pp->inc1 * 4u : 0x80000000u;   /* the dip: 4 samples or 2 %, */
        w = w > 85899346u ? w : 85899346u;                                        /* the longer */
        jb->wt = w >> 17;
        jb->wk = (32767u << 15) / jb->wt;
        jb->wd = (int32_t)(sy_minf(d, 1.0f) * 32767.0f);
        return OSK_SYNC;
    }
    return OSK_MAP;
}

/* one block of an oscillator of a voice, added into lr (left and right interleaved, Q15 in int32); sg: the source's
 * gains (level, pan, fades: left at the block's start and end, right), in its unison voices' */
static void osc_block(osc_v_t *o, const wt_table_t *t, const osc_par_t *pp, int32_t *lr, const float *sg)
{
    uint32_t n = pp->nuni, nold = o->fresh ? n : o->n, nmax = n > nold ? n : nold, j, i, nf = t->n;
    osc_job_t jb;
    uint32_t k;
    float pos1 = pp->pos * (float)(nf - 1u), pos0 = o->fresh ? pos1 : o->pos, sp = pp->spread * 0.5f * (float)(nf - 1u);
    float dc[4] = {0.0f, 0.0f, 0.0f, 0.0f}, hold, wdc = 0.0f;   /* a warp's offset: left and right, start and end */
    uint32_t inc0 = o->fresh ? pp->inc0 : o->inc;
    if (nmax != o->c_n || pp->umode != o->c_mode || sy_absf(pp->det - o->c_det) > 1e-5f)
        osc_uni_detune(o, nmax, pp->umode, pp->det);    /* (voices leaving keep their places this block) */
    if (nmax != o->c_gn || sy_absf(pp->blend - o->c_blend) > 1e-4f || sy_absf(pp->width - o->c_width) > 1e-4f)
        osc_uni_gains(o, nmax, pp->blend, pp->width);
    k = osc_setup(o, pp, pp->warp, &jb);
    hold = pp->warp == WARP_PWM ? (1.0f - 1.0f / OSC_SMAX) * o->c_wamt : 0.0f;   /* the share of the cycle held */
    if (pp->warp != WARP_OFF && pp->warp != WARP_FM && pp->warp != WARP_PWM) {  /* the others' offset: the centre */
        uint32_t c = (nmax - 1u) / 2u, ic = (uint32_t)(((uint64_t)pp->inc1 * o->ratio[c]) >> 30);   /* voice's */
        wdc = osc_warp_dc(o, t, pp->warp, pos1, osc_level(ic, o->slope));
    }
    for (j = 0; j < nmax; j++) {
        uint32_t ia = (uint32_t)(((uint64_t)inc0 * o->ratio[j]) >> 30);
        uint32_t ib = (uint32_t)(((uint64_t)pp->inc1 * o->ratio[j]) >> 30), im = ia > ib ? ia : ib;
        uint32_t lv = osc_level(im, o->slope), old = o->lev[j];
        int32_t tl = j < n ? o->tl[j] : 0, tr = j < n ? o->tr[j] : 0, ql0, ql1, qr0, qr1;   /* (leaving: to 0) */
        int32_t g0l = o->fresh ? tl : j < nold ? o->gl[j] : 0, g0r = o->fresh ? tr : j < nold ? o->gr[j] : 0;
        float p0 = sy_clampf(pos0 + sp * o->x[j], 0.0f, (float)(nf - 1u));
        float p1 = sy_clampf(pos1 + sp * o->x[j], 0.0f, (float)(nf - 1u));
        float dp = (p1 - p0) * (1.0f / (float)SY_BLK);
        if (old < WT_NLEV && lv < old && osc_level((uint32_t)((float)im * 1.0595f), o->slope) >= old)
            lv = old;                                  /* hysteresis: a level down a semitone late */
        if (old >= WT_NLEV)
            g0l = o->fresh ? g0l : 0, g0r = o->fresh ? g0r : 0;   /* silent before: fade in */
        ql0 = (int32_t)((float)g0l * sg[0] + 0.5f);    /* the source's gains in (rounded): the kernels play these */
        ql1 = (int32_t)((float)tl * sg[1] + 0.5f);
        qr0 = (int32_t)((float)g0r * sg[2] + 0.5f);
        qr1 = (int32_t)((float)tr * sg[3] + 0.5f);
        jb.inc = ia;
        jb.dinc = ((int32_t)ib - (int32_t)ia) / (int32_t)SY_BLK;
        if (lv >= WT_NLEV) {                           /* too high: fade the old level out, or nothing */
            if (old < WT_NLEV && !o->fresh) {
                jb.ph = o->ph[j];
                jb.gl = ql0;
                jb.gr = qr0;
                jb.dgl = -ql0 / (int32_t)SY_BLK;
                jb.dgr = -qr0 / (int32_t)SY_BLK;
                o->ph[j] = osc_run(k, &jb, t, old, p0, dp, lr);
            } else {
                o->ph[j] += (ia >> 1) * SY_BLK + (ib >> 1) * SY_BLK;
            }
            lv = WT_NLEV;
            tl = tr = 0;
        } else if (old == lv || old >= WT_NLEV || o->fresh) {
            jb.ph = o->ph[j];
            jb.gl = ql0;
            jb.gr = qr0;
            jb.dgl = (ql1 - ql0) / (int32_t)SY_BLK;
            jb.dgr = (qr1 - qr0) / (int32_t)SY_BLK;
            o->ph[j] = osc_run(k, &jb, t, lv, p0, dp, lr);
        } else {                                       /* a level change: the old one fades out, the new in */
            uint32_t ph = o->ph[j];
            jb.ph = ph;
            jb.gl = ql0;
            jb.gr = qr0;
            jb.dgl = -ql0 / (int32_t)SY_BLK;
            jb.dgr = -qr0 / (int32_t)SY_BLK;
            osc_run(k, &jb, t, old, p0, dp, lr);
            jb.inc = ia;
            jb.ph = ph;
            jb.gl = 0;
            jb.gr = 0;
            jb.dgl = ql1 / (int32_t)SY_BLK;
            jb.dgr = qr1 / (int32_t)SY_BLK;
            o->ph[j] = osc_run(k, &jb, t, lv, p0, dp, lr);
        }
        if ((hold > 0.0f || wdc != 0.0f) && lv < WT_NLEV) {   /* PWM holds the cycle's first sample: its share is */
            float v0 = wdc;                                   /* an offset; the other warps': wdc */
            if (hold > 0.0f) {
                int32_t kf = sy_clampi(sy_floori(p1), 0, nf >= 2u ? (int32_t)nf - 2 : 0);
                const int16_t *fa = wt_frame(t, (uint32_t)kf, lv), *fb = wt_frame(t, nf >= 2u ? (uint32_t)kf + 1u : 0u, lv);
                v0 = hold * ((float)fa[0] + (float)(fb[0] - fa[0]) * (p1 - (float)kf));
            }
            v0 *= 1.0f / 32768.0f;
            dc[0] += v0 * (float)ql0;
            dc[1] += v0 * (float)ql1;
            dc[2] += v0 * (float)qr0;
            dc[3] += v0 * (float)qr1;
        }
        o->lev[j] = (uint8_t)lv;
        o->gl[j] = (int16_t)tl;
        o->gr[j] = (int16_t)tr;
    }
    if (hold > 0.0f || wdc != 0.0f)                   /* ... taken out, ramped as the gains are: a warp adds no DC */
        for (i = 0; i < SY_BLK; i++) {
            float a = (float)i * (1.0f / (float)SY_BLK);
            lr[2u * i] -= (int32_t)(dc[0] + (dc[1] - dc[0]) * a);
            lr[2u * i + 1u] -= (int32_t)(dc[2] + (dc[3] - dc[2]) * a);
        }
    if (nmax > n)                                     /* the voices that faded out are gone */
        osc_uni_detune(o, n, pp->umode, pp->det);
    o->n = (uint8_t)n;
    o->inc = pp->inc1;
    o->pos = pos1;
    o->fresh = 0;
}

/* FM's modulator: this oscillator's centre unison voice alone (Q15 into m's left: an interleaved block), its warp
 * but no window or FM of its own; the phases advance only with advance (the oscillator is off and does not run) */
static void osc_modulator(osc_v_t *o, const wt_table_t *t, const osc_par_t *pp, int32_t *m, int advance)
{
    osc_job_t jb;
    uint32_t k;
    uint32_t c, lv, nf = t->n, ia, ib, ph;
    float pos1 = pp->pos * (float)(nf - 1u), pos0 = o->fresh ? pos1 : o->pos;
    uint32_t inc0 = o->fresh ? pp->inc0 : o->inc;
    sy_zeroi(m, 2u * SY_BLK);
    if (pp->nuni != o->c_n || pp->umode != o->c_mode || sy_absf(pp->det - o->c_det) > 1e-5f)
        osc_uni_detune(o, pp->nuni, pp->umode, pp->det);
    c = (pp->nuni - 1u) / 2u;
    k = osc_setup(o, pp, pp->warp == WARP_FM || pp->warp == WARP_SYNC ? WARP_OFF : pp->warp, &jb);
    ia = (uint32_t)(((uint64_t)inc0 * o->ratio[c]) >> 30);
    ib = (uint32_t)(((uint64_t)pp->inc1 * o->ratio[c]) >> 30);
    lv = osc_level(ia > ib ? ia : ib, o->slope);
    if (lv >= WT_NLEV)
        return;
    jb.inc = ia;
    jb.dinc = ((int32_t)ib - (int32_t)ia) / (int32_t)SY_BLK;
    jb.ph = o->ph[c];
    jb.gl = 32767;
    jb.gr = 0;
    jb.dgl = jb.dgr = 0;
    ph = osc_run(k, &jb, t, lv, pos0, (pos1 - pos0) * (1.0f / (float)SY_BLK), m);
    if (advance) {
        uint32_t j;
        for (j = 0; j < pp->nuni; j++)
            o->ph[j] = j == c ? ph : o->ph[j] + (ia >> 1) * SY_BLK + (ib >> 1) * SY_BLK;
        o->n = pp->nuni;
        o->inc = pp->inc1;
        o->pos = pos1;
        o->fresh = 0;
    }
}
