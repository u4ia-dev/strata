/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* The voice filter alone (src/sy_filter.c: flt_block):
 *   build/host/sy_filter_test
 * 1. responses (sines at -20 dBFS, the gain after 0.2 s): LP12 / HP12 flat far from the cutoff and -3 dB at it
 *    (+-0.5 dB, 100 Hz .. 10 kHz: the prewarp holds the tuning); 12 dB an octave above / below (+-1.5 dB at 4x);
 *    LP24 / HP24 -3 dB at the cutoff, 24 dB an octave; BP12 0 dB at the cutoff (normalised); NOTCH under -30 dB
 *    at it; PEAK +12 dB at it; LADDER a low pass falling 24 dB an octave; resonance at 100 % peaks the LP12 by
 *    over +30 dB; FORMANT puts its peaks near the vowel's first two formants;
 * 2. stability: every type at cutoffs 20 Hz .. 20 kHz, resonance 0, 50, 100 %, drive 0 and 100 %: a full-scale
 *    square for 0.5 s, then the cutoff jumping between its extremes every block for 0.5 s, then silence: never a
 *    NaN or Inf, bounded (|y| < 32), never a denormal state at a block's end, and after 40 s of silence every
 *    state is exactly 0 (the flush; a Q of 50 at 20 Hz rings for half a minute; the ladder at 100 % resonance
 *    sings on: bounded is enough there);
 * 3. VAR spreads the cutoff between left and right; MIX 0 is the input exactly;
 * 4. cost: host ns a stereo sample for each type. */
#define SY_ENGINE 1
#include "sy_host.h"

static flt_t F;

/* the filter's gain (dB) at hz for a sine of amplitude 0.1, settled */
static double gain_at(uint32_t type, float cut, float res, float hz, float var, int right)
{
    flt_par_t p;
    float l[SY_BLK], r[SY_BLK];
    uint32_t b, i, n = 0;
    double ein = 0, eout = 0, ph = 0, dph = (double)hz / (double)SY_FS;
    flt_note(&F);
    p.type = (uint8_t)type;
    p.hz = cut;
    p.res = res;
    p.drive = 0.0f;
    p.var = var;
    p.mix = 1.0f;
    for (b = 0; b < (uint32_t)(0.4 * (double)SY_FS) / SY_BLK; b++) {
        for (i = 0; i < SY_BLK; i++, ph += dph)
            l[i] = r[i] = (float)(0.1 * sin(2 * M_PI * ph));
        if (b * SY_BLK > 0.2 * (double)SY_FS)
            for (i = 0; i < SY_BLK; i++)
                ein += (double)l[i] * (double)l[i];
        flt_block(&F, &p, l, r);
        if (b * SY_BLK > 0.2 * (double)SY_FS)
            for (i = 0; i < SY_BLK; i++, n++)
                eout += right ? (double)r[i] * (double)r[i] : (double)l[i] * (double)l[i];
    }
    return 10 * log10(eout / ein);
}

static int all_zero(const flt_t *f)
{
    uint32_t c, i;
    for (c = 0; c < 2u; c++)
        for (i = 0; i < 6u; i++)
            if (f->s[c][i] != 0.0f)
                return 0;
    return 1;
}

int main(void)
{
    uint32_t t;
    int ok;
    t_name = "filter";
    {                                          /* 1. responses */
        static const float CUT[4] = {100.0f, 1000.0f, 5000.0f, 10000.0f};
        double worst3 = 0, w12 = 0;
        uint32_t c;
        for (c = 0; c < 4u; c++) {
            double g = gain_at(FT_LP12, CUT[c], 0.0f, CUT[c], 0.0f, 0), h = gain_at(FT_HP12, CUT[c], 0.0f, CUT[c], 0.0f, 0);
            worst3 = fmax(worst3, fmax(fabs(g + 3.01), fabs(h + 3.01)));
        }
        printf("filter: LP12 / HP12 at the cutoff (100 Hz .. 10 kHz): within %.2f dB of -3 dB\n", worst3);
        check("LP12 / HP12: -3 dB at the cutoff, 100 Hz .. 10 kHz (+-0.5 dB)", worst3 < 0.5);
        {
            double lp = gain_at(FT_LP12, 1000.0f, 0.0f, 4000.0f, 0.0f, 0), hp = gain_at(FT_HP12, 1000.0f, 0.0f, 250.0f, 0.0f, 0);
            double lp0 = gain_at(FT_LP12, 1000.0f, 0.0f, 50.0f, 0.0f, 0);
            w12 = fmax(fabs(lp + 24.1), fabs(hp + 24.1));
            printf("filter: LP12 two octaves up %.1f dB, HP12 two down %.1f dB, LP12 far below %.2f dB\n", lp, hp, lp0);
            check("12 dB an octave (two octaves out: -24 dB +-1.5) and flat in the passband", w12 < 1.5 && fabs(lp0) < 0.1);
        }
        {
            double a = gain_at(FT_LP24, 1000.0f, 0.0f, 1000.0f, 0.0f, 0), b = gain_at(FT_LP24, 1000.0f, 0.0f, 4000.0f, 0.0f, 0);
            double c2 = gain_at(FT_HP24, 1000.0f, 0.0f, 1000.0f, 0.0f, 0), d = gain_at(FT_HP24, 1000.0f, 0.0f, 250.0f, 0.0f, 0);
            printf("filter: LP24 at the cutoff %.2f dB, two octaves up %.1f dB; HP24 %.2f, %.1f dB\n", a, b, c2, d);
            check("LP24 / HP24: -3 dB at the cutoff (+-0.7), 24 dB an octave (two octaves: -48 +-3)",
                  fabs(a + 3.0) < 0.7 && fabs(c2 + 3.0) < 0.7 && fabs(b + 48.2) < 3.0 && fabs(d + 48.2) < 3.0);
        }
        {
            double bp = gain_at(FT_BP12, 1000.0f, 0.5f, 1000.0f, 0.0f, 0);
            double nt = gain_at(FT_NOTCH, 1000.0f, 0.0f, 1000.0f, 0.0f, 0);
            double pk = gain_at(FT_PEAK, 1000.0f, 0.3f, 1000.0f, 0.0f, 0);
            double rs = gain_at(FT_LP12, 1000.0f, 1.0f, 1000.0f, 0.0f, 0);
            printf("filter: BP12 at the cutoff %.2f dB, NOTCH %.1f dB, PEAK %.2f dB, LP12 at 100 %% resonance %.1f dB\n",
                   bp, nt, pk, rs);
            check("BP12 0 dB at the cutoff (+-0.3), NOTCH under -30 dB there, PEAK +12 dB (+-0.5)",
                  fabs(bp) < 0.3 && nt < -30.0 && fabs(pk - 12.04) < 0.5);
            check("resonance 100 %: the LP12 peaks over +30 dB at the cutoff", rs > 30.0);
        }
        {
            double a = gain_at(FT_LADDER, 1000.0f, 0.0f, 100.0f, 0.0f, 0), b = gain_at(FT_LADDER, 1000.0f, 0.0f, 4000.0f, 0.0f, 0);
            double c2 = gain_at(FT_LADDER, 1000.0f, 0.0f, 8000.0f, 0.0f, 0);
            printf("filter: LADDER passband %.2f dB, two octaves up %.1f dB, three %.1f dB\n", a, b, c2);
            check("LADDER: a low pass, about 24 dB an octave above the cutoff (-20..-28 dB from 2 to 3 octaves)",
                  fabs(a) < 1.0 && b < -20.0 && c2 - b < -20.0 && c2 - b > -28.0);
        }
        {                                      /* FORMANT at A: peaks near 600 and 1040 Hz, a dip between */
            float cut = 20.0f;                 /* (CUTOFF at its bottom: vowel A) */
            double p1 = gain_at(FT_FORMANT, cut, 0.5f, 600.0f, 0.5f, 0), p2 = gain_at(FT_FORMANT, cut, 0.5f, 1040.0f, 0.5f, 0);
            double dip = gain_at(FT_FORMANT, cut, 0.5f, 3500.0f, 0.5f, 0);
            printf("filter: FORMANT (A) at 600 Hz %.1f dB, 1040 Hz %.1f dB, 3.5 kHz %.1f dB\n", p1, p2, dip);
            check("FORMANT: the vowel's first formants stand above its upper range (by 10 dB)", p1 > dip + 10.0 && p2 > dip + 10.0);
        }
        {
            double l = gain_at(FT_LP12, 1000.0f, 0.0f, 1000.0f, 1.0f, 0), r = gain_at(FT_LP12, 1000.0f, 0.0f, 1000.0f, 1.0f, 1);
            printf("filter: VAR 100 %%: left %.1f dB, right %.1f dB at the cutoff\n", l, r);
            check("VAR: the left cutoff an octave down, the right an octave up", l < -8.0 && r > -1.5);
        }
    }
    {                                          /* 2. stability */
        static const float CUTS[5] = {20.0f, 200.0f, 2000.0f, 10000.0f, 20000.0f};
        static const float RES[3] = {0.0f, 0.5f, 1.0f};
        double top = 0;
        int finite = 1, quiet = 1, denorm = 0;
        uint32_t c, r, d, b, i;
        for (t = 0; t < FT_COUNT; t++)
            for (c = 0; c < 5u; c++)
                for (r = 0; r < 3u; r++)
                    for (d = 0; d < 2u; d++) {
                        flt_par_t p;
                        float l[SY_BLK], rr[SY_BLK];
                        flt_note(&F);
                        p.type = (uint8_t)t;
                        p.res = RES[r];
                        p.drive = (float)d;
                        p.var = 0.3f;
                        p.mix = 1.0f;
                        for (b = 0; b < 42u * 1378u; b++) {
                            p.hz = b < 689u ? CUTS[c] : b < 1378u ? (b & 1u ? 20000.0f : 20.0f) : CUTS[c];
                            for (i = 0; i < SY_BLK; i++)
                                l[i] = rr[i] = b < 1378u ? (((b * SY_BLK + i) / 50u) & 1u ? 1.0f : -1.0f) : 0.0f;
                            flt_block(&F, &p, l, rr);
                            for (i = 0; i < SY_BLK; i++) {
                                if (!isfinite(l[i]) || !isfinite(rr[i]))
                                    finite = 0;
                                top = fmax(top, fmax(fabs((double)l[i]), fabs((double)rr[i])));
                            }
                            for (i = 0; i < 12u; i++)
                                if (fpclassify(F.s[i / 6u][i % 6u]) == FP_SUBNORMAL)
                                    denorm++;
                        }
                        if (!(t == FT_LADDER && r == 2u) && !all_zero(&F)) {
                            quiet = 0;
                            printf("filter: %s cut %.0f res %.1f drive %u: states not 0 after 40 s of silence\n",
                                   FT_NAMES[t], (double)CUTS[c], (double)RES[r], d);
                        }
                    }
        printf("filter: stability: largest output %.2f over every type and corner\n", top);
        check("stability: no NaN / Inf at any type, cutoff, resonance, drive, with the cutoff jumping", finite);
        check("stability: bounded (|y| < 32)", top < 32.0);
        check("never a denormal state at a block's end", denorm == 0);
        check("after the input stops every state decays to exactly 0 (40 s; the ladder at 100 % aside)", quiet);
    }
    {                                          /* 3. MIX 0 */
        flt_par_t p;
        float l[SY_BLK], r[SY_BLK], x[SY_BLK];
        uint32_t i;
        flt_note(&F);
        p.type = FT_LP24;
        p.hz = 300.0f;
        p.res = 0.5f;
        p.drive = 0.0f;
        p.var = 0.0f;
        p.mix = 0.0f;
        for (i = 0; i < SY_BLK; i++)
            x[i] = l[i] = r[i] = sinf((float)i * 0.7f);
        flt_block(&F, &p, l, r);
        ok = 1;
        for (i = 0; i < SY_BLK; i++)
            ok &= l[i] == x[i] && r[i] == x[i];
        check("MIX 0: the input exactly", ok);
    }
    for (t = 0; t < FT_COUNT; t++) {           /* 4. cost */
        flt_par_t p;
        float l[SY_BLK], r[SY_BLK];
        uint32_t b, i;
        uint64_t t0;
        flt_note(&F);
        p.type = (uint8_t)t;
        p.res = 0.5f;
        p.drive = 0.0f;
        p.var = 0.2f;
        p.mix = 1.0f;
        for (i = 0; i < SY_BLK; i++)
            l[i] = r[i] = 0.1f * sinf((float)i);
        t0 = t_now_ns();
        for (b = 0; b < 20000u; b++) {
            p.hz = 500.0f + (float)(b & 63u) * 30.0f;
            flt_block(&F, &p, l, r);
            l[0] += 0.001f;
        }
        printf("filter: host cost %-8s %.2f ns a stereo sample\n", FT_NAMES[t], (double)(t_now_ns() - t0) / (20000.0 * SY_BLK));
    }
    return t_done();
}
