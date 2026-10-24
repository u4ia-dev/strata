/* SPDX-License-Identifier: GPL-3.0-only */
/* The wavetable generator (src/sy_wtgen.c) against tools/gen_wavetables.py's reference renders, and the format
 * (src/sy_wt.c):
 *   build/host/sy_wt_test build/gen/strata_wt_ref.bin
 * 1. every factory table built in C equals the script's double-precision render: max difference <= 4 LSB and
 *    RMS difference <= 0.5 LSB over all frames and levels (single precision, the device's own sin / exp2 / log2);
 * 2. format: every level band-limited (its FFT: nothing above its top harmonic but int16 rounding, < 1 LSB RMS),
 *    no DC (< 1 LSB mean), the guard sample a copy of sample 0, the table's peak 0.95 of full scale (+-1 LSB);
 * 3. levels match each other: a frame's levels differ only by the harmonics they drop (level L against level 0
 *    band-limited the same way: < 1 LSB RMS apart);
 * 4. wt_level: the top harmonic at or under 21 kHz at every increment, and the next level down would exceed it;
 * 5. the build time per table on the host (the device's: the BENCH page). */
#include "sy_host.h"

static int16_t tab[SY_NFRAME][WT_FRAME];

int main(int argc, char **argv)
{
    FILE *f;
    char hdr[8];
    uint16_t nt, fs, frames;
    uint32_t t, k, l, i;
    int worst = 0, all_bl = 1, all_dc = 1, all_guard = 1, all_peak = 1, all_lev = 1;
    double worst_rms = 0, worst_bl = 0, worst_lev = 0, ms_total = 0, ms_max = 0;
    t_name = "wt";
    (void)wt_tab;                                  /* (the oscillators' tables: not used here) */
    (void)wt_pool;
    if (argc < 2 || !(f = fopen(argv[1], "rb"))) {
        fprintf(stderr, "usage: sy_wt_test build/gen/strata_wt_ref.bin\n");
        return 2;
    }
    if (fread(hdr, 1, 8, f) != 8 || memcmp(hdr, "SYWT", 4)) {
        fprintf(stderr, "sy_wt_test: not a reference file\n");
        return 2;
    }
    memcpy(&nt, hdr + 4, 2);
    memcpy(&fs, hdr + 6, 2);
    check("reference: as many tables as WT_NFACT, frames of WT_FRAME", nt == WT_NFACT && fs == WT_FRAME);
    wg_init();
    for (t = 0; t < nt; t++) {
        static int16_t ref[SY_NFRAME][WT_FRAME];
        int md = 0, pk = 0;
        double se = 0, ms;
        uint64_t t0;
        if (fread(&frames, 2, 1, f) != 1 || frames > SY_NFRAME ||
            fread(ref, sizeof ref[0], frames, f) != frames) {
            check("reference readable", 0);
            return t_done();
        }
        t0 = t_now_ns();
        wg_begin(t, tab[0]);
        while (!wg_step())
            ;
        ms = (t_now_ns() - t0) / 1e6;
        ms_total += ms;
        ms_max = fmax(ms_max, ms);
        if (frames != WT_RECIPES[t].frames)
            check("frames as the recipe", 0);
        for (k = 0; k < frames; k++)
            for (i = 0; i < WT_FRAME; i++) {
                int d = abs(tab[k][i] - ref[k][i]);
                md = d > md ? d : md;
                se += (double)d * d;
                pk = abs(tab[k][i]) > pk ? abs(tab[k][i]) : pk;
            }
        se = sqrt(se / (frames * WT_FRAME));
        if (md > 4 || se > 0.5)
            printf("wt: %-12s max diff %d LSB, rms %.3f LSB\n", WT_NAMES[t], md, se);
        worst = md > worst ? md : worst;
        worst_rms = fmax(worst_rms, se);
        if (abs(pk - 31129) > 1) {
            printf("wt: %-12s peak %d\n", WT_NAMES[t], pk);
            all_peak = 0;
        }
        /* format: band limits, DC, guards; and the levels against level 0 band-limited the same way */
        for (k = 0; k < frames; k++)
            for (l = 0; l < WT_NLEV; l++) {
                uint32_t n = WT_N(l), h = 256u >> l, j;
                const int16_t *s = tab[k] + WT_OFF[l];
                double re[1024], im[1024], out = 0, dc = 0;
                for (j = 0; j < n; j++) {
                    re[j] = s[j];
                    im[j] = 0;
                    dc += s[j];
                }
                if (s[n] != s[0])
                    all_guard = 0;
                if (fabs(dc / n) > 1.0)
                    all_dc = 0;
                dfft(re, im, n);
                for (j = h + 1u; j <= n / 2; j++)  /* what lies above the top harmonic, as an RMS in LSB */
                    out += (re[j] * re[j] + im[j] * im[j]) / ((double)n * n / 4) / 2;
                worst_bl = fmax(worst_bl, sqrt(out));
                if (sqrt(out) > 1.0)
                    all_bl = 0;
                if (l > 0) {                       /* level 0's harmonics 1..h against level l's (a tapered */
                    double r0[1024], i0[1024], e = 0;   /* table: 1..h/2, under both tapers): RMS in LSB */
                    uint32_t hc = (WT_RECIPES[t].flags & WT_TAPER) && h >= 4u ? h / 2u : h;
                    for (j = 0; j < 1024; j++) {
                        r0[j] = tab[k][j];
                        i0[j] = 0;
                    }
                    dfft(r0, i0, 1024);
                    for (j = 1; j <= hc; j++) {
                        double ar = r0[j] / 512, ai = i0[j] / 512, br = re[j] / (n / 2), bi = im[j] / (n / 2);
                        e += ((ar - br) * (ar - br) + (ai - bi) * (ai - bi)) / 2;
                    }
                    worst_lev = fmax(worst_lev, sqrt(e));
                    if (sqrt(e) > 1.0)
                        all_lev = 0;
                }
            }
    }
    fclose(f);
    printf("wt: worst max difference %d LSB, rms %.3f LSB; above the band %.2f LSB rms; levels apart %.2f LSB rms\n",
           worst, worst_rms, worst_bl, worst_lev);
    check("every factory table as the script renders it (max <= 4 LSB, rms <= 0.5 LSB)", worst <= 4 && worst_rms <= 0.5);
    check("each level band-limited to its harmonics (under 1 LSB RMS above them: rounding only)", all_bl);
    check("no DC in any level (mean < 1 LSB)", all_dc);
    check("guard samples are sample 0", all_guard);
    check("each table's peak is 0.95 of full scale (+-1 LSB)", all_peak);
    check("a frame's levels agree on the harmonics they share (under 1 LSB RMS apart)", all_lev);
    {
        int ok = 1;
        uint32_t inc;
        for (inc = 1000; inc < 0x80000000u; inc += inc / 97u + 1u) {
            uint32_t lv = wt_level(inc);
            double f = inc * (double)SY_FS / 4294967296.0;
            if (lv < WT_NLEV && (256u >> lv) * f > 21000.0 + 1e-3)
                ok = 0;                          /* aliasing harmonic */
            if (lv > 0 && lv < WT_NLEV && (256u >> (lv - 1u)) * f <= 21000.0 - 1.0)
                ok = 0;                          /* a lower level would have done */
            if (lv == WT_NLEV && f <= 21000.0 - 1.0)
                ok = 0;
        }
        check("wt_level: top harmonic <= 21 kHz, and the lowest level that keeps it", ok);
    }
    printf("wt: build on the host: %.2f ms a table on average, %.2f ms the slowest\n", ms_total / nt, ms_max);
    return t_done();
}
