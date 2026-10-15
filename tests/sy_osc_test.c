/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* The wavetable oscillator alone (src/sy_osc.c: osc_block, no voice, filter or effects):
 *   build/host/sy_osc_test [quick]
 * 1. aliasing: every factory table at positions 0, 1/2, 1, notes C0..C8 every 6 semitones: the power away from
 *    the harmonics of the note (a 65,536-point Blackman-Harris FFT, harmonics +-6 bins masked), in dB under the
 *    total. Each table's worst is printed. The limits are today's measured state, held as regression limits:
 *    ALIAS_SMOOTH for the tables whose spectra fall towards the top, ALIAS_BRIGHT for the TAPER ones (flat up to
 *    the top). docs/ARCHITECTURE.md §18's targets (-60 / -40 dBc) are met above E3 by the smooth tables but not
 *    below it: levels 0 and 1 are 4x oversampled (RAM), their images reach -46..-50 dBc on saws (docs/TESTING.md);
 * 2. warps: SYNC BEND+ BEND- PWM ASYM+ ASYM- MIRROR at 30 % and 100 % on a saw and a vowel, the same measure,
 *    printed; held to WARP_LIMIT at 30 % up to C5. The warps' corners and SYNC's reset are not band-limited yet
 *    (polyBLEP / polyBLAMP corrections are the planned fix): strong warps on high notes alias;
 * 3. pitch: a sine's frequency (zero crossings over a second) against 440 * 2^((n - 69) / 12) at the codec's
 *    rate, notes 21..108: within 0.05 cent;
 * 4. unison: the power of 1..16 voices with random phases within +-1.5 dB of one voice (equal power), the
 *    detune ratios symmetric, the gains mirrored for WIDTH;
 * 5. levels: a vibrato of +-30 cents across a level boundary changes the level once, not with every cycle
 *    (hysteresis);
 * 6. cost: host ns per unison voice a sample (plain kernel and each warp's). */
#define SY_ENGINE 1
#include "sy_host.h"

#define NFFT 65536u
#define ALIAS_SMOOTH -40.0                       /* regression limits: today's worst, rounded (see above) */
#define ALIAS_BRIGHT -27.0
#define WARP_LIMIT -20.0
static float xl[NFFT + 8192], xr[NFFT + 8192];
static osc_v_t ov;
static uint32_t built = 0xFFFF;

static void use_table(uint32_t id)
{
    if (built == id)
        return;
    wg_begin(id, wt_pool[0]);
    while (!wg_step())
        ;
    wt_tab[0].f = wt_pool[0];
    wt_tab[0].n = wg.n;
    wt_tab[0].id = (uint8_t)id;
    built = id;
}

/* n samples of one oscillator (fresh note, phase 0 unless rand) into l / r as float */
static void render(uint32_t table, float pos, uint32_t warp, float wamt, float note, uint32_t nuni, float det,
                   float width, float rand, float *l, float *r, uint32_t n)
{
    static int32_t ilr[2u * SY_BLK];
    osc_par_t p;
    uint32_t b, i;
    static uint32_t seed = 12345;
    use_table(table);
    memset(&ov, 0, sizeof ov);
    osc_note(&ov, 1, 0.0f, rand, &seed);
    memset(&p, 0, sizeof p);
    p.inc0 = p.inc1 = voice_inc(note);
    p.pos = pos;
    p.warp = (uint8_t)warp;
    p.wamt = wamt;
    p.nuni = (uint8_t)nuni;
    p.det = det;
    p.blend = 1.0f;
    p.width = width;
    for (b = 0; b < n; b += SY_BLK) {
        memset(ilr, 0, sizeof ilr);
        osc_block(&ov, &wt_tab[0], &p, ilr, UNITY);
        for (i = 0; i < SY_BLK && b + i < n; i++) {
            l[b + i] = (float)ilr[2u * i] * (1.0f / 32768.0f);
            r[b + i] = (float)ilr[2u * i + 1u] * (1.0f / 32768.0f);
        }
    }
}

static double *pw;
/* the inharmonic power of a table at a note, dB under the total */
static double alias_db(uint32_t table, float pos, uint32_t warp, float wamt, float note)
{
    double f0;
    render(table, pos, warp, wamt, note, 1, 0.0f, 0.0f, 0.0f, xl, xr, NFFT + 4096u);
    f0 = (double)voice_inc(note) * (double)SY_FS / 4294967296.0;
    power_spectrum(xl + 4096, NFFT, pw);
    return inharmonic_db(pw, NFFT, (double)SY_FS, f0, 6);
}

/* a bright table: its spectrum stays flat up to the top (gen_wavetables.py marks it TAPER) */
static int bright(uint32_t t) { return (WT_RECIPES[t].flags & WT_TAPER) != 0; }

/* the frequency of a signal from its rising zero crossings (interpolated), Hz */
static double freq_zc(const float *x, uint32_t n)
{
    double first = -1, last = -1;
    uint32_t i, cnt = 0;
    for (i = 1; i < n; i++)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
            double t = (i - 1) + (double)x[i - 1] / ((double)x[i - 1] - (double)x[i]);
            if (first < 0)
                first = t;
            last = t;
            cnt++;
        }
    return cnt > 1 ? (cnt - 1) * (double)SY_FS / (last - first) : 0;
}

int main(int argc, char **argv)
{
    int quick = argc > 1 && !strcmp(argv[1], "quick");
    uint32_t t, k, nbad = 0;
    double worst_s = -300, worst_b = -300;
    t_name = "osc";
    pw = malloc((NFFT / 2 + 1) * sizeof *pw);
    wg_init();
    memset(wt_tab, 0, sizeof wt_tab);

    /* 1. aliasing of every table */
    printf("osc: aliasing, worst over C0..C8 and positions 0, 1/2, 1 (dB under the total)\n");
    for (t = 0; t < WT_NFACT; t++) {
        double w = -300;
        float wn = 0, wp = 0;
        static const float POS[3] = {0.0f, 0.5f, 1.0f};
        uint32_t p;
        for (p = 0; p < 3u; p++)
            for (k = 12; k <= 108u; k += quick ? 24u : 6u) {
                double a = alias_db(t, POS[p], WARP_OFF, 0.0f, (float)k);
                if (a > w) {
                    w = a;
                    wn = (float)k;
                    wp = POS[p];
                }
            }
        printf("osc:   %2u %-12s %-6s %7.1f dB  (note %2.0f, pos %.1f)%s\n", t, WT_NAMES[t], bright(t) ? "bright" : "smooth",
               w, (double)wn, (double)wp, w > (bright(t) ? ALIAS_BRIGHT : ALIAS_SMOOTH) ? "  <- over" : "");
        if (bright(t))
            worst_b = fmax(worst_b, w);
        else
            worst_s = fmax(worst_s, w);
        nbad += w > (bright(t) ? ALIAS_BRIGHT : ALIAS_SMOOTH);
    }
    printf("osc: worst smooth table %.1f dB, worst bright table %.1f dB\n", worst_s, worst_b);
    check("aliasing: smooth tables under -40 dBc, bright ones under -27 dBc, C0..C8 (regression limits)", nbad == 0);

    /* 2. warps */
    {
        static const uint32_t TABS[2] = {1, 27};          /* SAW, VOWELS */
        static const float POSW[2] = {1.0f, 0.5f};
        double ww = -300;
        uint32_t m, a, j;
        static const float AMT[2] = {0.3f, 1.0f};
        printf("osc: warps (worst over notes C1..C7)\n");
        for (m = WARP_SYNC; m <= WARP_MIRROR; m++)
            for (j = 0; j < 2u; j++)
                for (a = 0; a < 2u; a++) {
                    double w = -300, w5 = -300;
                    float wn = 0;
                    for (k = 24; k <= 96u; k += quick ? 24u : 12u) {
                        double d = alias_db(TABS[j], POSW[j], m, AMT[a], (float)k);
                        if (d > w)
                            w = d, wn = (float)k;
                        if (k <= 72u)
                            w5 = fmax(w5, d);
                    }
                    printf("osc:   %-7s %3.0f %% %-8s %7.1f dB up to C5, %7.1f dB up to C7 (note %.0f)\n", WARP_NAMES[m],
                           (double)(AMT[a] * 100.0f), WT_NAMES[TABS[j]], w5, w, (double)wn);
                    if (a == 0u)
                        ww = fmax(ww, w5);
                }
        check("warps at 30 % on a saw and a vowel under -20 dBc up to C5 (regression limit)", ww <= WARP_LIMIT);
    }

    /* 3. pitch */
    {
        double worst = 0;
        for (k = 21; k <= 108u; k += 3u) {
            double f, want = 440.0 * pow(2.0, (k - 69.0) / 12.0), c;
            render(5, 0.0f, WARP_OFF, 0.0f, (float)k, 1, 0.0f, 0.0f, 0.0f, xl, xr, 44118u);
            f = freq_zc(xl, 44118u);
            c = 1200.0 * log2(f / want);
            worst = fmax(worst, fabs(c));
        }
        printf("osc: pitch, worst %.4f cent (A0..C8)\n", worst);
        check("pitch within 0.05 cent of equal temperament at the codec's rate, A0..C8", worst < 0.05);
    }

    /* 4. unison */
    {
        double p1 = 0, worst = 0;
        uint32_t nn;
        int sym = 1;
        for (nn = 1; nn <= SY_NUNI; nn++) {
            double pw2 = 0;
            uint32_t i;
            render(1, 1.0f, WARP_OFF, 0.0f, 45.0f, nn, 0.4f, 1.0f, 1.0f, xl, xr, 44118u);
            for (i = 4410; i < 44118u; i++)
                pw2 += (double)xl[i] * (double)xl[i] + (double)xr[i] * (double)xr[i];
            if (nn == 1)
                p1 = pw2;
            worst = fmax(worst, fabs(10 * log10(pw2 / p1)));
            for (i = 0; i < nn; i++)               /* voice i and its mirror: +x and -x cents */
                if (fabs(log2(ov.ratio[i] / 1073741824.0) + log2(ov.ratio[nn - 1 - i] / 1073741824.0)) > 1e-5)
                    sym = 0;
        }
        printf("osc: unison 1..16 voices, power within %.2f dB of one voice\n", worst);
        check("unison: 1..16 voices with random phases keep the power within 1.5 dB", worst < 1.5);
        check("unison: detune ratios symmetric about the note", sym);
    }

    /* 5. levels: hysteresis */
    {
        static int32_t ilr[2u * SY_BLK];
        osc_par_t p;
        uint32_t b, changes = 0, last;
        static uint32_t seed = 1;
        float base;
        use_table(1);
        memset(&ov, 0, sizeof ov);
        osc_note(&ov, 1, 0.0f, 0.0f, &seed);
        memset(&p, 0, sizeof p);
        p.pos = 1.0f;
        p.nuni = 1;
        /* the boundary between level 2 and 3 (WT_INC_L0 << 2): its note */
        base = 69.0f + 12.0f * log2f((float)(WT_INC_L0 << 2) / SY_INC_A4);
        last = 0xFF;
        for (b = 0; b < 3u * 1378u; b++) {
            float note = base + 0.30f * sinf(6.2831853f * 5.0f * (float)b / 1378.0f);
            p.inc1 = voice_inc(note);
            p.inc0 = p.inc1;
            osc_block(&ov, &wt_tab[0], &p, ilr, UNITY);
            if (b > 2u && ov.lev[0] != last)
                changes++;
            last = ov.lev[0];
        }
        printf("osc: a 5 Hz vibrato of +-30 cent across a level boundary: %u level changes in 3 s\n", changes);
        check("levels: a vibrato across a boundary changes the level at most once (hysteresis)", changes <= 1u);
    }

    /* 6. cost on the host */
    {
        static const uint32_t W[5] = {WARP_OFF, WARP_SYNC, WARP_BENDP, WARP_PWM, WARP_MIRROR};
        uint32_t m;
        for (m = 0; m < 5u; m++) {
            uint64_t t0 = t_now_ns();
            render(1, 0.5f, W[m], 0.5f, 57.0f, 16, 0.3f, 1.0f, 1.0f, xl, xr, 65536u);
            printf("osc: host cost, %-6s %.2f ns per unison voice a sample\n", WARP_NAMES[W[m]],
                   (double)(t_now_ns() - t0) / (65536.0 * 16.0));
        }
    }
    return t_done();
}
