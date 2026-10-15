/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA core: sizes, the sample rate, the pool section, and the few math functions the DSP needs.
 * The FM-1 has no libm and its doubles are software: everything here is single precision, written for the
 * pi32v2 FPU (-mcpu=r3 -mfprev1), and every float constant carries its f. Every sy_* DSP file
 * (docs/ARCHITECTURE.md §2) builds three ways: the FM-1 image (JieLi clang), the host harness (tests/sy_*,
 * gcc) and the browser emulator (emcc); none of them uses the HAL. Identifiers are sy_ / SY_ (storage.c
 * already has st_). */
#ifndef SY_CORE_H
#define SY_CORE_H
#include <stdint.h>

/* ------------------------------------------------------------ sizes --- */
#define SY_FS 44117.647f          /* the codec's rate: 24 MHz / 544 (audio.c DAC_TICKS); A4 is 440 Hz on the unit */
#define SY_BLK 32u                /* samples a control block: events, envelopes, LFOs, the matrix (Felucca's CTL) */
#define SY_NVOICE 8u              /* voices */
#define SY_NOSC 2u                /* wavetable oscillators a voice: A, B */
#define SY_NUNI 16u               /* unison voices an oscillator */
#define SY_NFRAME 16u             /* frames (keyframes) a table at most */

#if defined(__PI32V2__)
#define SY_POOL __attribute__((section(".pool")))   /* app.ld's 360 KiB POOL (not cleared at boot: init it) */
#else
#define SY_POOL
#endif

/* ------------------------------------------------------------- math --- */
static inline float sy_absf(float x) { return x < 0.0f ? -x : x; }
static inline float sy_minf(float a, float b) { return a < b ? a : b; }
static inline float sy_maxf(float a, float b) { return a > b ? a : b; }
static inline float sy_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline int32_t sy_clampi(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float sy_lerpf(float a, float b, float t) { return a + (b - a) * t; }

/* a kernel kept whole wherever it is called (one copy in the image, for the instruction cache; tools/costprobe
 * counts it exactly) */
#define SY_NOINLINE __attribute__((noinline))

/* zeroing and copying in the audio path: the firmware's memset and memcpy go a byte at a time (src/libc.c, built
 * -fno-builtin: 3 or 4 instructions a byte on the target), these a word at a time */
static inline void sy_zerof(float *x, uint32_t n)
{
    for (; n >= 4u; n -= 4u, x += 4)
        x[0] = x[1] = x[2] = x[3] = 0.0f;
    while (n--)
        *x++ = 0.0f;
}
static inline void sy_zeroi(int32_t *x, uint32_t n)
{
    for (; n >= 4u; n -= 4u, x += 4)
        x[0] = x[1] = x[2] = x[3] = 0;
    while (n--)
        *x++ = 0;
}
static inline void sy_zero16(int16_t *x, uint32_t n) { while (n--) *x++ = 0; }
static inline void sy_copyf(float *restrict d, const float *restrict s, uint32_t n)
{
    for (; n >= 4u; n -= 4u, d += 4, s += 4) {
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        d[3] = s[3];
    }
    while (n--)
        *d++ = *s++;
}

static inline int32_t sy_floori(float x)          /* floor for |x| < 2^31 */
{
    int32_t i = (int32_t)x;
    return (float)i > x ? i - 1 : i;
}

/* 2^x (relative error < 2e-7 for -126 < x < 128; 0 below): the integer part into the exponent, the fraction
 * by a degree-5 polynomial fitted on [0, 1) */
static inline float sy_exp2f(float x)
{
    union { float f; uint32_t u; } e;
    int32_t i;
    float f;
    if (x < -126.0f)
        return 0.0f;
    if (x > 127.0f)
        x = 127.0f;
    i = sy_floori(x);
    f = x - (float)i;
    e.u = (uint32_t)(i + 127) << 23;
    return e.f * (1.0f + f * (0.69315246557f + f * (0.24015283328f + f * (0.055835901595f +
                                                         f * (0.0089733694275f + f * 0.0018853123172f)))));
}

/* log2(x), x > 0 (absolute error < 3e-6): the exponent, and log2 of the mantissa 1..2 by a degree-6 polynomial */
static inline float sy_log2f(float x)
{
    union { float f; uint32_t u; } m;
    int32_t e;
    float t;
    if (!(x > 0.0f))
        return -127.0f;
    m.f = x;
    e = (int32_t)((m.u >> 23) & 255u) - 127;
    m.u = (m.u & 0x007FFFFFu) | 0x3F800000u;
    t = m.f - 1.0f;
    return (float)e + t * (1.4425543558f + t * (-0.71829762318f + t * (0.45833905635f + t * (-0.27966974375f +
                                                        t * (0.12356784478f + t * -0.026495975012f)))));
}

/* sin(2 pi t) for t in turns, |t| <= 0.25 (absolute error < 2e-7): an odd degree-9 polynomial */
static inline float sy_sin_q(float t)
{
    float t2 = t * t;
    return t * (6.2831851602f + t2 * (-41.341655048f + t2 * (81.601005028f + t2 * (-76.549802650f +
                                                                             t2 * 39.536850791f))));
}

/* sin(2 pi t), any t in turns: reduced to [-0.25, 0.25] */
static inline float sy_sin2pi(float t)
{
    t -= (float)sy_floori(t);                      /* 0..1 */
    if (t > 0.75f)
        t -= 1.0f;                                 /* -0.25..0.75 */
    else if (t > 0.25f)
        t = 0.5f - t;                              /* sin(pi - x) = sin(x) */
    return sy_sin_q(t);
}

static inline float sy_cos2pi(float t) { return sy_sin2pi(t + 0.25f); }

/* tan(pi x) for 0 <= x < 0.5: sin / cos, both by sy_sin_q (the filters' prewarp) */
static inline float sy_tanpi(float x)
{
    return sy_sin_q(0.5f * x) / sy_sin_q(0.25f - 0.5f * x);
}

/* soft clip: x - 4 x^3 / 27 up to |x| = 1.5 (slope 0 there), +-1 beyond; slope 1 at 0 */
static inline float sy_softclip(float x)
{
    if (x >= 1.5f)
        return 1.0f;
    if (x <= -1.5f)
        return -1.0f;
    return x - 0.14814815f * x * x * x;
}

/* a recursive state that has decayed under 1e-15 becomes exact 0: no denormals (their speed on the pi32v2 FPU
 * is unknown; x86 slows down on them). Once per block, never per sample */
#define SY_FLUSH(x) do { if (sy_absf(x) < 1e-15f) (x) = 0.0f; } while (0)

/* xorshift32: noise, random phases, note-on random (state never 0) */
static inline uint32_t sy_rand(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

/* a uniform float in [-1, 1) from a sy_rand value */
static inline float sy_randf(uint32_t r) { return (float)(int32_t)r * (1.0f / 2147483648.0f); }

#endif
