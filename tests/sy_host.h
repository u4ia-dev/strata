/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* The STRATA DSP on the host: the firmware's sy_* sources compiled as one unit with gcc (tests/sy_tests.sh),
 * and what the tests share: checks, WAV output, a double-precision FFT, level and spectrum measures. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../firmware/src/sy_core.h"
#include "../firmware/src/sy_wt.c"
#include "../firmware/src/sy_wtgen.c"
#ifdef SY_ENGINE
#include "../firmware/src/sy_param.c"
#include "../firmware/src/sy_osc.c"
#include "../firmware/src/sy_env.c"
#include "../firmware/src/sy_lfo.c"
#include "../firmware/src/sy_mod.c"
#include "../firmware/src/sy_filter.c"
#include "../firmware/src/sy_voice.c"
#include "../firmware/src/sy_fx.c"
#include "../firmware/src/sy_drum.c"
#include "../firmware/src/sy_engine.c"
#include "../firmware/src/sy_text.c"
static const float UNITY[4] = {1.0f, 1.0f, 1.0f, 1.0f};   /* osc_block's source gains for a test of it alone */
#endif

static int t_bad, t_n;
static const char *t_name = "sy";
static void check(const char *what, int ok)
{
    printf("%s: %-96s %s\n", t_name, what, ok ? "ok" : "FAIL");
    t_bad += !ok;
    t_n++;
}
static int t_done(void)
{
    printf("%s: %d checks, %d failed\n", t_name, t_n, t_bad);
    return t_bad != 0;
}

/* ------------------------------------------------------------------------------------------- WAV --- */
static void wav_write(const char *path, const float *l, const float *r, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, v;
    uint16_t a = 1, ch = r ? 2 : 1, ba = (uint16_t)(2 * ch), bits = 16;
    uint32_t sr = 44118, br = sr * ba;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f); v = 36 + n * ba; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = n * ba; fwrite(&v, 4, 1, f);
    for (i = 0; i < n; i++) {
        float x = l[i] * 32767.0f;
        int16_t s = (int16_t)(x > 32767.0f ? 32767 : x < -32768.0f ? -32768 : x);
        fwrite(&s, 2, 1, f);
        if (r) {
            x = r[i] * 32767.0f;
            s = (int16_t)(x > 32767.0f ? 32767 : x < -32768.0f ? -32768 : x);
            fwrite(&s, 2, 1, f);
        }
    }
    fclose(f);
}

/* ------------------------------------------------------------------------------------------- FFT --- */
/* in-place complex FFT (double), n a power of two up to 2^20; re / im separate */
#define T_FFT_MAX (1u << 20)
static void dfft(double *re, double *im, uint32_t n)
{
    static double *tc, *ts;
    uint32_t i, j, k, len;
    if (!tc) {                                   /* twiddles of the largest size; smaller ones stride */
        tc = malloc(T_FFT_MAX / 2 * sizeof *tc);
        ts = malloc(T_FFT_MAX / 2 * sizeof *ts);
        for (i = 0; i < T_FFT_MAX / 2; i++) {
            tc[i] = cos(-2.0 * M_PI * i / T_FFT_MAX);
            ts[i] = sin(-2.0 * M_PI * i / T_FFT_MAX);
        }
    }
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        uint32_t st = T_FFT_MAX / len, h = len / 2;
        for (i = 0; i < n; i += len)
            for (k = 0; k < h; k++) {
                double c = tc[k * st], s = ts[k * st];
                double *ar = re + i + k, *ai = im + i + k, *br = re + i + k + h, *bi = im + i + k + h;
                double tr = *br * c - *bi * s, ti = *br * s + *bi * c;
                *br = *ar - tr; *bi = *ai - ti;
                *ar += tr; *ai += ti;
            }
    }
}

/* power spectrum of n samples (n a power of two) with a 4-term Blackman-Harris window: p[0 .. n/2] */
static void power_spectrum(const float *x, uint32_t n, double *p)
{
    double *re = calloc(n, sizeof *re), *im = calloc(n, sizeof *im);
    uint32_t i;
    for (i = 0; i < n; i++) {
        double w = 0.35875 - 0.48829 * cos(2 * M_PI * i / n) + 0.14128 * cos(4 * M_PI * i / n) -
                   0.01168 * cos(6 * M_PI * i / n);
        re[i] = (double)x[i] * w;
    }
    dfft(re, im, n);
    for (i = 0; i <= n / 2; i++)
        p[i] = re[i] * re[i] + im[i] * im[i];
    free(re);
    free(im);
}

/* the share of a spectrum's power away from the harmonics of f0 (Hz): bins further than `guard` bins from any
 * k f0 count as inharmonic (aliases, images). Returns dB under the total; -200 for none */
static double inharmonic_db(const double *p, uint32_t n, double fs, double f0, uint32_t guard)
{
    double tot = 0, bad = 0, binhz = fs / n;
    uint32_t i;
    for (i = 2; i <= n / 2; i++) {             /* (DC and the first bin: the window's skirt) */
        double f = i * binhz, h = f / f0, d = fabs(h - floor(h + 0.5)) * f0 / binhz;
        tot += p[i];
        if (d > guard && h > 0.5)
            bad += p[i];
    }
    return bad > 0 ? 10 * log10(bad / tot) : -200.0;
}

static double rms(const float *x, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)x[i] * (double)x[i];
    return sqrt(s / (n ? n : 1));
}

static double peak(const float *x, uint32_t n)
{
    double m = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        m = fmax(m, fabs(x[i]));
    return m;
}

static double mean(const float *x, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)x[i];
    return s / (n ? n : 1);
}

/* the largest step between two samples */
static double max_step(const float *x, uint32_t n)
{
    double m = 0;
    uint32_t i;
    for (i = 1; i < n; i++)
        m = fmax(m, fabs((double)x[i] - (double)x[i - 1]));
    return m;
}

static double db(double x) { return x > 0 ? 20 * log10(x) : -200.0; }

static uint64_t t_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}
