/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* V0.85: the drum synth (firmware/src/sy_drum.c) through sy_block, as the audio interrupt runs it:
 *   build/host/sy_drum_test tests/sy_drum_golden.txt [DEMODIR]       GOLDEN_UPDATE=1 rewrites the golden file
 * every lane sounds, stays under its bounds and ends; DECAY is the time to -60 dB; TUNE the KICK's pitch; CLAP's four
 * bursts SPREAD apart; HAT CL chokes HAT OP; ACCENT's lift, VELOCITY's share; PAN and the track's gains (a muted
 * track silent); the WT lanes on OSC A's and B's tables (none built: no hit); the duck's AMOUNT, ATTACK, RELEASE and
 * SHAPEs, on the synth's sound and never on the drums; 40 random kits healthy; the renders twice the same and as
 * recorded; the cost on the host. V0.86 (the drum editor): each drum's SHAPE, DRIVE (its four TYPEs, its MIX) and EQ
 * (its three bands where they should act, and not elsewhere), the kit's COMP, the duck's SOURCE and HOLD, ACCENT's
 * TONE, the eight factory kits, 40 random kits with every effect, the cost with every effect on. */
#define SY_ENGINE 1
#include "sy_host.h"

#define NMAX (44118u * 9u)
static float L[NMAX], R[NMAX];
static const char *demo;

/* n samples of sy_block from L/R's place at */
static uint32_t run(uint32_t n, uint32_t at)
{
    int32_t o[2u * SY_BLK];
    uint32_t i, k;
    for (k = 0; k < n && at + SY_BLK <= NMAX; k += SY_BLK, at += SY_BLK) {
        sy_block(o);
        for (i = 0; i < SY_BLK; i++) {
            L[at + i] = (float)o[2u * i] * (1.0f / 32767.0f);
            R[at + i] = (float)o[2u * i + 1u] * (1.0f / 32767.0f);
        }
    }
    return at;
}
static uint32_t secs(float s) { return (uint32_t)(s * SY_FS); }
static float pkr(const float *x, uint32_t a, uint32_t b)
{
    float p = 0.0f;
    for (; a < b; a++)
        p = fmaxf(p, fabsf(x[a]));
    return p;
}
static int finite_all(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (!isfinite(L[i]) || !isfinite(R[i]))
            return 0;
    return 1;
}
/* the time (s) after which |x| stays under p x 10^(db / 20) */
static float fall_time(const float *x, uint32_t n, float p, float db)
{
    float th = p * powf(10.0f, db / 20.0f);
    uint32_t i = n;
    while (i > 0 && fabsf(x[i - 1u]) < th)
        i--;
    return (float)i / SY_FS;
}
static void fresh(void)
{
    sy_init();
    sy_tables_now();
}

static void t_lanes(void)
{
    uint32_t l, n, ok = 1, ends = 1, fin = 1, wt2 = 1;
    for (l = 0; l < DK_N; l++) {
        float p, t60;
        fresh();
        sy_drum_hit(l, 0, 127, 0);
        t60 = dk_t60(l, dkk.l[l].decay);
        n = run(secs(t60 * 1.4f + 0.15f), 0);
        p = fmaxf(pkr(L, 0, n), pkr(R, 0, n));
        printf("drums:   %-7s peak %.3f  -60 dB at %.3f s (DECAY %.3f s)\n", DK_NAME[l], p,
               fall_time(L, n, pkr(L, 0, n), -60.0f), t60);
        ok &= p > 0.05f && p < 0.8f;
        ends &= !dkv[l].on;
        fin &= finite_all(n);
        if (demo) {
            char path[256];
            snprintf(path, sizeof path, "%s/drum_%s.wav", demo, DK_NAME[l]);
            for (char *c = path + strlen(demo); *c; c++)
                if (*c == ' ')
                    *c = '_';
            wav_write(path, L, R, n);
        }
    }
    check("every lane at velocity 127: heard, peaks under 0.8 (-2 dB), every sample finite", ok && fin);
    check("every lane's voice ends within its DECAY (a hit, nothing after)", ends);
    fresh();                                        /* (a WT lane's table: none built, or being built: no hit) */
    {
        uint8_t n1 = wt_tab[1].n;
        wt_tab[1].n = 0;
        sy_drum_hit(DK_WT2, 0, 127, 0);
        wt2 = !dkv[DK_WT2].on;
        wt_tab[1].n = n1;
        wt_tab[0].busy = 1;
        sy_drum_hit(DK_WT1, 0, 127, 0);
        wt2 &= !dkv[DK_WT1].on;
        wt_tab[0].busy = 0;
        sy_drum_hit(DK_WT1, 0, 127, 0);
        wt2 &= dkv[DK_WT1].on;
    }
    check("WT 1, WT 2: no hit while their table is none (never built) or being built; heard once it is", wt2);
}

/* DECAY as the time to -60 dB (the peak's), the models whose amplitude is one envelope */
static void t_decay(void)
{
    static const uint8_t LANES[5] = {DK_KICK, DK_HATC, DK_HATO, DK_CYM, DK_WT1};
    static const uint8_t D[3] = {20, 50, 80};
    uint32_t i, j, ok = 1;
    float worst = 0.0f;
    for (i = 0; i < 5u; i++)
        for (j = 0; j < 3u; j++) {
            uint32_t l = LANES[i], n;
            float t, want, p;
            fresh();
            dkk.l[l].decay = D[j];
            if (l == DK_KICK)
                dkk.l[l].tone = 0;                 /* (no click: the sine's own envelope) */
            want = dk_t60(l, D[j]);
            sy_drum_hit(l, 0, 127, 0);
            n = run(secs(want * 1.5f + 0.05f), 0);
            p = pkr(L, 0, n);
            t = fall_time(L, n, p, -60.0f);
            worst = fmaxf(worst, fabsf(t / want - 1.0f));
            ok &= fabsf(t / want - 1.0f) < 0.3f;
        }
    printf("drums:   DECAY: the time to -60 dB within %.0f %% of its setting at worst\n", 100.0f * worst);
    check("DECAY: KICK, HAT CL, HAT OP, CYM, WT 1 fall 60 dB in DECAY's time (within 30 %, DECAY 20, 50, 80)", ok);
}

/* TUNE: the KICK's pitch once swept (zero crossings 0.3 .. 0.6 s after the hit) */
static void t_tune(void)
{
    static const int8_t T[3] = {-12, 0, 12};
    uint32_t j, ok = 1;
    for (j = 0; j < 3u; j++) {
        uint32_t n, i, zc = 0, a = secs(0.3f), b = secs(0.6f);
        float hz, want = 48.0f * powf(2.0f, (float)T[j] / 12.0f);
        fresh();
        dkk.l[DK_KICK].tune = T[j];
        dkk.l[DK_KICK].decay = 100;
        dkk.l[DK_KICK].tone = 0;
        sy_drum_hit(DK_KICK, 0, 127, 0);
        n = run(b + SY_BLK, 0);
        for (i = a + 1u; i < b && i < n; i++)
            zc += (L[i - 1u] < 0.0f) != (L[i] < 0.0f);
        hz = (float)zc * 0.5f / ((float)(b - a) / SY_FS);
        printf("drums:   KICK TUNE %+d: %.1f Hz (%.1f wanted)\n", T[j], hz, want);
        ok &= fabsf(hz / want - 1.0f) < 0.03f;
    }
    check("TUNE: the KICK at 48 Hz x 2^(TUNE / 12) once swept (-12, 0, +12: within 3 %)", ok);
}

/* CLAP: four bursts SPREAD apart (each a peak of the envelope near its time), then the tail */
static void t_clap(void)
{
    uint32_t n, k, ok = 1;
    float s;
    fresh();
    dkk.l[DK_CLAP].chr = 100;                       /* (SPREAD 20 ms) */
    sy_drum_hit(DK_CLAP, 0, 127, 0);
    s = 0.02f * SY_FS;
    n = run(secs(0.3f), 0);
    for (k = 1; k < 4u; k++) {                      /* (louder just after burst k than just before it) */
        uint32_t at = (uint32_t)(s * (float)k);
        float before = pkr(L, at - 120u, at - 20u), after = pkr(L, at, at + 100u);
        ok &= after > 2.0f * before;
    }
    check("CLAP: four bursts SPREAD apart (20 ms), each louder at its start than the end of the one before", ok && n);
}

/* HAT CL ends a HAT OP ringing: off within 60 ms */
static void t_choke(void)
{
    uint32_t at, ok;
    fresh();
    dkk.l[DK_HATO].decay = 100;                     /* (2 s) */
    sy_drum_hit(DK_HATO, 0, 127, 0);
    at = run(secs(0.1f), 0);
    ok = dkv[DK_HATO].on;
    sy_drum_hit(DK_HATC, 0, 127, 0);
    run(secs(0.06f), at);
    check("the choke: a HAT CL hit ends a HAT OP ringing (2 s) within 60 ms", ok && !dkv[DK_HATO].on);
}

/* ACCENT's lift and VELOCITY's share, as peaks of the KICK (no click: the same shape at every gain) */
static float kick_peak(uint32_t vel, int acc)
{
    uint32_t n;
    fresh();
    dkk.l[DK_KICK].tone = 0;
    dkk.l[DK_KICK].level = 80;
    sy_drum_hit(DK_KICK, 0, vel, acc);
    n = run(secs(0.15f), 0);
    return pkr(L, 0, n);
}
static void t_gains(void)
{
    float p, pa, lo, hi;
    uint32_t n;
    int ok;
    p = kick_peak(127, 0);
    pa = kick_peak(127, 1);
    check("ACCENT 50 %: an accented hit 1.5 x a plain one (within 2 %)", fabsf(pa / p - 1.5f) < 0.03f);
    fresh();
    dkk.vsens = 0;
    dkk.l[DK_KICK].tone = 0;
    sy_drum_hit(DK_KICK, 0, 30, 0);
    n = run(secs(0.15f), 0);
    lo = pkr(L, 0, n);
    fresh();
    dkk.vsens = 0;
    dkk.l[DK_KICK].tone = 0;
    sy_drum_hit(DK_KICK, 0, 127, 0);
    n = run(secs(0.15f), 0);
    hi = pkr(L, 0, n);
    ok = fabsf(lo / hi - 1.0f) < 0.01f;
    fresh();
    dkk.vsens = 100;
    dkk.l[DK_KICK].tone = 0;
    sy_drum_hit(DK_KICK, 0, 64, 0);
    n = run(secs(0.15f), 0);
    lo = pkr(L, 0, n);
    fresh();
    dkk.vsens = 100;
    dkk.l[DK_KICK].tone = 0;
    sy_drum_hit(DK_KICK, 0, 127, 0);
    n = run(secs(0.15f), 0);
    hi = pkr(L, 0, n);
    check("VELOCITY 0: velocity 30 as loud as 127; 100: velocity 64 at (64 / 127)^2 (within 3 %)",
          ok && fabsf(lo / hi / powf(64.0f / 127.0f, 2.0f) - 1.0f) < 0.03f);
    fresh();
    dkk.l[DK_SNARE].pan = 1;
    sy_drum_hit(DK_SNARE, 0, 127, 0);
    n = run(secs(0.2f), 0);
    ok = pkr(L, 0, n) > 0.05f && pkr(R, 0, n) < 1e-4f;
    fresh();
    sy_tgain[3][0] = sy_tgain[3][1] = 0.0f;         /* (track 4 muted: the looper's gains) */
    sy_drum_hit(DK_SNARE, 3, 127, 0);
    n = run(secs(0.2f), 0);
    check("PAN hard left: nothing right; a hit for a muted track (its gains 0): silent",
          ok && pkr(L, 0, n) < 1e-4f && pkr(R, 0, n) < 1e-4f);
}

/* the duck: the synth's held note against itself without it (the gain, sample by sample: AMOUNT, ATTACK, RELEASE,
 * SHAPE); a drum hit at once never ducked */
static float duck_gain[NMAX / SY_BLK];
static uint8_t duck_st[NMAX / SY_BLK];
static uint32_t duck_run(uint32_t shape, int duck, int drum)
{
    uint32_t at = 0, b;
    fresh();
    dkk.amount = 75;
    dkk.attack = 10;
    dkk.release = 50;                               /* (20 ms x 50^0.5: 141 ms) */
    dkk.shape = (uint8_t)shape;
    sy_note_on(48, 100);
    at = run(secs(0.3f), at);
    if (duck)
        sy_drum_hit(DK_DUCK, 0, 100, 0);
    if (drum)
        sy_drum_hit(DK_KICK, 0, 127, 0);
    for (b = 0; b < secs(0.4f) / SY_BLK; b++) {
        at = run(SY_BLK, at);
        duck_gain[b] = dkd.g;
        duck_st[b] = dkd.st;
    }
    return at;
}
static float LA[NMAX];
static void t_duck(void)
{
    uint32_t n, i, s, a0 = secs(0.3f), ok = 1, okc = 1, rest = 1, ncmp = 0;
    float rel = dk_rel_s(), lo = 1.0f;
    static float G[DS_N][3];
    for (s = 0; s < DS_N; s++) {
        uint32_t b, att_b = 0, half_b, end_b = 0;
        n = duck_run(s, 0, 0);
        memcpy(LA, L, n * sizeof L[0]);
        n = duck_run(s, 1, 0);
        rel = dk_rel_s();
        for (b = 0; b < secs(0.4f) / SY_BLK; b++) {
            if (!att_b && duck_gain[b] <= 0.2501f)
                att_b = b + 1u;
            if (att_b && !end_b && !duck_st[b])
                end_b = b + 1u;
        }
        lo = 1.0f;
        for (b = 0; b < secs(0.4f) / SY_BLK; b++)
            lo = fminf(lo, duck_gain[b]);
        half_b = att_b + (uint32_t)(0.5f * rel * SY_FS / (float)SY_BLK);
        G[s][0] = lo;
        G[s][1] = (float)(att_b * SY_BLK) / SY_FS;
        G[s][2] = half_b < secs(0.4f) / SY_BLK ? duck_gain[half_b] : 0.0f;
        printf("drums:   duck %-6s: down to %.3f in %.1f ms, at half RELEASE %.3f, back in %.1f ms (RELEASE %.1f)\n",
               DS_NAME[s], lo, 1000.0f * G[s][1], G[s][2], 1000.0f * (float)((end_b - att_b) * SY_BLK) / SY_FS,
               1000.0f * rel);
        ok &= fabsf(lo - 0.25f) < 0.005f && fabsf(G[s][1] - 0.010f) < 0.0015f;
        ok &= end_b && fabsf((float)((end_b - att_b) * SY_BLK) / SY_FS - rel) < 0.003f;
        for (i = a0 + secs(0.002f); i < n; i++)     /* (the output: the synth's x the gain, ramped a block; the */
            if (fabsf(LA[i]) > 0.1f) {              /* master's DC blocker after it remembers ~32 ms: a few LSBs) */
                float g = L[i] / LA[i];
                ncmp++;
                if (g < lo - 0.03f || g > 1.0f + 0.01f) {
                    if (okc)
                        printf("drums:   duck %s: at %.4f s the ratio %.4f (%.4f / %.4f)\n", DS_NAME[s],
                               (double)((float)(i - a0) / SY_FS), (double)g, (double)L[i], (double)LA[i]);
                    okc = 0;
                }
            }
        if (!(dkd.st == 0u && dkd.g == 1.0f))
            printf("drums:   duck %s at its end: state %u, gain %.6f\n", DS_NAME[s], dkd.st, (double)dkd.g);
        rest &= dkd.st == 0u && dkd.g == 1.0f;
    }
    check("the duck: AMOUNT 75 % (the gain to 0.25), ATTACK 10 ms, back in RELEASE's time, every SHAPE", ok);
    check("SHAPE at half RELEASE: LINEAR 1 - 0.75 x 0.5, EXP sooner back (0.125 deep), PUMP later (0.875)",
          fabsf(G[DS_LIN][2] - (1.0f - 0.75f * 0.5f)) < 0.03f && fabsf(G[DS_EXP][2] - (1.0f - 0.75f * 0.125f)) < 0.03f &&
              fabsf(G[DS_PUMP][2] - (1.0f - 0.75f * 0.875f)) < 0.03f);
    printf("drums:   duck: %u samples over 0.1 compared\n", ncmp);
    check("the synth's sound is the unducked one x the duck's gain; the duck at rest after (gain exactly 1)",
          okc && rest && ncmp > 10000u);
    {                                               /* (a KICK alone, then with a duck at the same moment: the */
        uint32_t m, i2, same = 1;                   /* duck takes the synth's sound only, the kick is the same) */
        fresh();
        sy_drum_hit(DK_KICK, 0, 127, 0);
        n = run(secs(0.3f), 0);
        memcpy(LA, L, n * sizeof L[0]);
        fresh();
        dkk.amount = 100;
        sy_drum_hit(DK_KICK, 0, 127, 0);
        sy_drum_hit(DK_DUCK, 0, 127, 0);
        m = run(secs(0.3f), 0);
        for (i2 = 0; i2 < n && i2 < m; i2++)
            same &= L[i2] == LA[i2];
        check("the drums are never ducked: a KICK with a full duck at once is the KICK alone, bit for bit",
              same && m == n && pkr(L, 0, m) > 0.05f);
    }
}

/* 40 random kits, every lane: finite, bounded, ended */
static void t_health(void)
{
    uint32_t seed = 12345u, k, l, ok = 1, ends = 1;
    float worst = 0.0f;
    for (k = 0; k < 40u; k++) {
        fresh();
        for (l = 0; l < DK_N; l++) {
            dk_lane_t *d = &dkk.l[l];
            d->tune = (int8_t)((int32_t)(sy_rand(&seed) % 49u) - 24);
            d->decay = (uint8_t)(sy_rand(&seed) % 101u);
            d->tone = (uint8_t)(sy_rand(&seed) % 101u);
            d->chr = (uint8_t)(sy_rand(&seed) % 101u);
            d->level = (uint8_t)(sy_rand(&seed) % 128u);
            d->pan = (uint8_t)(1u + sy_rand(&seed) % 127u);
        }
        dkk.accent = (uint8_t)(sy_rand(&seed) % 101u);
        dkk.vsens = (uint8_t)(sy_rand(&seed) % 101u);
        for (l = 0; l < DK_N; l++) {
            uint32_t n;
            float p;
            sy_drum_hit(l, sy_rand(&seed) % 8u, 1u + sy_rand(&seed) % 127u, (int)(sy_rand(&seed) & 1u));
            n = run(secs(dk_t60(l, dkk.l[l].decay) * 1.4f + 0.15f), 0);   /* (off at -80 dB: 1.33 x DECAY) */
            ok &= finite_all(n);
            p = fmaxf(pkr(L, 0, n), pkr(R, 0, n));
            worst = fmaxf(worst, p);
            ends &= !dkv[l].on;
        }
    }
    printf("drums:   40 random kits: the loudest hit %.3f\n", worst);
    check("40 random kits, every lane (random velocity, accent, track): finite, every voice ended", ok && ends);
}


/* ---------------------------------------------------------------------------------------- V0.86 --- */
/* a drum's effects on a sound x of n samples, a block at a time, as the hit set them (lane's dkk.fx): DRIVE and EQ,
 * then SHAPE's gain stepped over the block as sy_drum_block steps it */
static float FX[NMAX];
static void fx_run(uint32_t lane, float *x, uint32_t n)
{
    dk_voice_t v;
    uint32_t i, j;
    memset(&v, 0, sizeof v);
    dk_fx_hit(&v, lane);
    for (i = 0; i + SY_BLK <= n; i += SY_BLK) {
        if (v.fx_dr | v.fx_eq)
            dk_fx_block(&v, x + i);
        if (v.fx_sh) {
            float g0, g1, dg;
            dk_sh_gains(&v, &g0, &g1);
            dg = (g1 - g0) * (1.0f / (float)SY_BLK);
            for (j = 0; j < SY_BLK; j++, g0 += dg)
                x[i + j] *= g0;
        }
    }
}
static uint32_t sine(float hz, float a, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        FX[i] = a * sinf(6.2831853f * hz * (float)i / SY_FS);
    return n;
}
static float eq_db(uint32_t lo, uint32_t mid, uint32_t hi, uint32_t freq, float hz)
{
    uint32_t n = sine(hz, 0.25f, secs(0.4f) / SY_BLK * SY_BLK);
    fresh();
    dkk.fx[DK_KICK].lo = (uint8_t)lo, dkk.fx[DK_KICK].mid = (uint8_t)mid, dkk.fx[DK_KICK].hi = (uint8_t)hi;
    dkk.fx[DK_KICK].freq = (uint8_t)freq;
    fx_run(DK_KICK, FX, n);
    return 20.0f * log10f(pkr(FX, n / 2u, n) / 0.25f);
}
static void t_fx(void)
{
    uint32_t n, i, k, ok = 1, bounded = 1, steps = 1, nd = 0;
    float a, b, c, d, e, f, base[4], peak = 0.0f, last = 2.0f;
    /* SHAPE on a steady sound: ATTACK the start's gain falling to SUSTAIN's */
    fresh();
    dkk.fx[DK_SNARE].att = 90, dkk.fx[DK_SNARE].sus = 20;
    n = secs(0.2f) / SY_BLK * SY_BLK;
    for (i = 0; i < n; i++)
        FX[i] = 0.1f;
    fx_run(DK_SNARE, FX, n);
    a = FX[0] / 0.1f, b = FX[n - 1u] / 0.1f;
    printf("drums:   SHAPE ATTACK 90, SUSTAIN 20: the start x %.2f (+9.6 dB wanted x 3.02), the body x %.3f (x 0.437)\n", a, b);
    check("SHAPE: ATTACK's gain at the hit, SUSTAIN's on the body (within 2 %), the start's share falling between",
          fabsf(a - 3.02f) < 0.06f && fabsf(b - 0.437f) < 0.009f && FX[secs(0.005f)] / 0.1f < a && FX[secs(0.005f)] / 0.1f > b);
    /* DRIVE: each TYPE bounded at its out level; CRUSH at 100 % on 2 bits; MIX 50 % half way */
    for (k = 0; k < DD_N; k++) {
        fresh();
        dkk.fx[DK_SNARE].dtype = (uint8_t)k, dkk.fx[DK_SNARE].drive = 100, dkk.fx[DK_SNARE].dmix = 100;
        n = sine(220.0f, 0.5f, 4096u);
        fx_run(DK_SNARE, FX, n);
        peak = pkr(FX, 0, n);
        bounded &= peak <= (k == DD_CRUSH ? 0.51f : 0.401f);
        if (k == DD_CRUSH) {                       /* (its values: steps of a quarter) */
            for (i = 0; i < n; i++)
                steps &= fabsf(FX[i] * 4.0f - roundf(FX[i] * 4.0f)) < 1e-5f;
        }
        base[k] = peak;
    }
    fresh();
    dkk.fx[DK_SNARE].dtype = DD_HARD, dkk.fx[DK_SNARE].drive = 100, dkk.fx[DK_SNARE].dmix = 50;
    n = sine(220.0f, 0.5f, 4096u);
    fx_run(DK_SNARE, FX, n);
    for (i = 0; i < n; i++) {
        float x = 0.5f * sinf(6.2831853f * 220.0f * (float)i / SY_FS), y = fminf(fmaxf(x * 16.0f, -1.0f), 1.0f) * 0.4f;
        ok &= fabsf(FX[i] - 0.5f * (x + y)) < 1e-4f;
    }
    printf("drums:   DRIVE 100 %%: SOFT %.3f, HARD %.3f, FOLD %.3f, CRUSH %.3f at most (0.5 in)\n", base[0], base[1], base[2],
           base[3]);
    check("DRIVE: SOFT, HARD, FOLD bounded by its out level, CRUSH on 2 bits (steps of 1/4); MIX 50 % half the way",
          bounded && steps && ok);
    /* EQ: each band where it acts, and not far off elsewhere */
    a = eq_db(100, 50, 50, 50, 80.0f), b = eq_db(100, 50, 50, 50, 6000.0f);
    c = eq_db(50, 100, 50, 50, 200.0f * powf(2.0f, 5.3219281f * 0.5f)), d = eq_db(50, 100, 50, 50, 80.0f);
    e = eq_db(50, 50, 0, 50, 10000.0f), f = eq_db(50, 50, 0, 50, 80.0f);
    printf("drums:   EQ LOW +12: %+.1f dB at 80 Hz, %+.1f at 6 kHz; MID +12 (1.27 kHz): %+.1f there, %+.1f at 80 Hz; HIGH -12: %+.1f at 10 kHz, %+.1f at 80 Hz\n",
           a, b, c, d, e, f);
    check("EQ: LOW +12 dB over 8 dB at 80 Hz and within 1.5 dB at 6 kHz; MID +12 over 10 dB at FREQ; HIGH -12 under -9 dB at 10 kHz",
          a > 8.0f && fabsf(b) < 1.5f && c > 10.0f && fabsf(d) < 3.0f && e < -9.0f && fabsf(f) < 1.5f);
    (void)nd, (void)last;
}
/* COMP: the beat louder (its make-up) and its peaks held together: the peak over the loudness less */
static float crest(uint32_t a, uint32_t b)
{
    double s = 0.0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += (double)L[i] * L[i];
    return pkr(L, a, b) / (float)sqrt(s / (double)(b - a));
}
static void t_comp(void)
{
    uint32_t n, l, at;
    float c0, c1, p0, p1;
    for (l = 0; l < 2u; l++) {
        fresh();
        dkk.comp[0] = l ? 80 : 0, dkk.comp[1] = 10, dkk.comp[2] = 10, dkk.comp[3] = 100;   /* (RELEASE 29 ms) */
        at = 0;
        for (n = 0; n < 8u; n++) {
            sy_drum_hit(n & 1u ? DK_SNARE : DK_KICK, 0, 127, 0);
            sy_drum_hit(DK_HATC, 0, 100, 0);
            at = run(secs(0.25f), at);
        }
        if (l)
            c1 = crest(secs(0.5f), at), p1 = pkr(L, secs(0.5f), at);
        else
            c0 = crest(secs(0.5f), at), p0 = pkr(L, secs(0.5f), at);
    }
    printf("drums:   COMP 80 %%: the beat's crest %.2f -> %.2f, its peak %.3f -> %.3f\n", c0, c1, p0, p1);
    check("COMP: the beat's crest (peak over RMS) lower with it on; nothing over full scale", c1 < c0 * 0.85f && p1 < 1.0f);
}
/* the duck's SOURCE (its row, the KICK, both) and HOLD */
static void t_duck2(void)
{
    uint32_t src, ok = 1, b, held = 0, down = 0;
    for (src = 0; src < DSRC_N; src++) {
        int row, kick;
        fresh();
        dkk.dsrc = (uint8_t)src;
        sy_drum_hit(DK_DUCK, 0, 100, 0);
        row = dkd.st != 0u;
        sy_drum_off();
        sy_drum_hit(DK_KICK, 0, 100, 0);
        kick = dkd.st != 0u;
        ok &= row == (src != DSRC_KICK) && kick == (src != DSRC_ROW);
    }
    check("the duck's SOURCE: ROW its row's hits, KICK the KICK's, BOTH both", ok);
    for (b = 0; b < 2u; b++) {                      /* (HOLD 40: 100 ms down, then the release; 0: the release at once) */
        uint32_t k;
        fresh();
        dkk.amount = 75, dkk.attack = 0, dkk.release = 50, dkk.dhold = (uint8_t)(b ? 40 : 0);
        sy_note_on(48, 100);
        run(secs(0.1f), 0);
        sy_drum_hit(DK_DUCK, 0, 100, 0);
        for (k = 0; k < secs(0.08f) / SY_BLK; k++)
            run(SY_BLK, 0);
        if (b)
            held = fabsf(dkd.g - 0.25f) < 1e-4f;
        else
            down = dkd.g > 0.26f;
    }
    check("the duck's HOLD 100 ms: still at the bottom 80 ms after it got there; without: on its way back by then", held && down);
}
/* ACCENT's TONE: a soft hit darker (the HAT CL's high-pass lower) than at TONE 0, an accented one brighter */
static float bright(uint32_t vel, int acc, uint32_t at)
{
    uint32_t n, i;
    double d = 0.0, e = 0.0;
    fresh();
    dkk.atone = (uint8_t)at;
    sy_drum_hit(DK_HATC, 0, vel, acc);
    n = run(secs(0.05f), 0);
    for (i = 1; i < n; i++) {
        d += (double)(L[i] - L[i - 1u]) * (L[i] - L[i - 1u]);
        e += (double)L[i] * L[i];
    }
    return (float)(d / (e + 1e-20));
}
static void t_atone(void)
{
    float s0 = bright(30, 0, 0), s1 = bright(30, 0, 100), a0 = bright(127, 1, 0), a1 = bright(127, 1, 100);
    printf("drums:   ACCENT's TONE 100: a soft HAT CL's brightness %.3f -> %.3f, an accented one's %.3f -> %.3f\n", s0, s1, a0,
           a1);
    check("ACCENT's TONE: a soft hit darker than at 0, an accented one brighter", s1 < s0 * 0.95f && a1 > a0);
}
/* the factory kits: each loads whole, every lane heard, healthy and ended */
static void t_kits(void)
{
    uint32_t k, l, ok = 1, ends = 1, named = 1;
    float worst = 0.0f, kw = 0.0f;
    for (k = 0; k < DK_NKIT; k++) {
        fresh();
        dk_kit_load(k);
        named &= dkk.kit == k && dkk.name[0] != 0 && strcmp(dkk.name, DK_KITS[k].name) == 0;
        for (l = 0; l < DK_N; l++) {
            uint32_t n;
            sy_drum_hit(l, 0, 127, 1);
            n = run(secs(dk_t60(l, dkk.l[l].decay) * 2.2f + 0.3f), 0);
            ok &= finite_all(n);
            worst = fmaxf(worst, fmaxf(pkr(L, 0, n), pkr(R, 0, n)));
            ends &= !dkv[l].on;
            kw = fmaxf(kw, fmaxf(pkr(L, 0, n), pkr(R, 0, n)));
            if (fmaxf(pkr(L, 0, n), pkr(R, 0, n)) > 0.75f)
                printf("drums:     %s %s: %.3f\n", DK_KITS[k].name, DK_NAME[l], fmaxf(pkr(L, 0, n), pkr(R, 0, n)));
        }
        printf("drums:   %-10s its loudest accented hit %.3f\n", DK_KITS[k].name, kw);
        kw = 0.0f;
    }
    printf("drums:   the %u factory kits: the loudest accented hit %.3f\n", DK_NKIT, worst);
    check("the factory kits: each loads whole (its name, its number); every lane accented finite, under 0.8 (-2 dB), ended",
          ok && ends && named && worst < 0.8f);
}
static void t_health2(void)
{
    uint32_t seed = 54321u, k, l, ok = 1, ends = 1;
    float worst = 0.0f;
    for (k = 0; k < 40u; k++) {
        fresh();
        for (l = 0; l < DK_N; l++) {
            dk_fx_t *f = &dkk.fx[l];
            f->dtype = (uint8_t)(sy_rand(&seed) % DD_N), f->drive = (uint8_t)(sy_rand(&seed) % 101u);
            f->dmix = (uint8_t)(sy_rand(&seed) % 101u);
            f->lo = (uint8_t)(sy_rand(&seed) % 101u), f->mid = (uint8_t)(sy_rand(&seed) % 101u);
            f->hi = (uint8_t)(sy_rand(&seed) % 101u), f->freq = (uint8_t)(sy_rand(&seed) % 101u);
            f->att = (uint8_t)(sy_rand(&seed) % 101u), f->sus = (uint8_t)(sy_rand(&seed) % 101u);
        }
        dkk.dsrc = (uint8_t)(sy_rand(&seed) % DSRC_N), dkk.dhold = (uint8_t)(sy_rand(&seed) % 101u);
        dkk.atone = (uint8_t)(sy_rand(&seed) % 101u);
        for (l = 0; l < 4u; l++)
            dkk.comp[l] = (uint8_t)(sy_rand(&seed) % 101u);
        for (l = 0; l < DK_N; l++) {
            uint32_t n;
            sy_drum_hit(l, sy_rand(&seed) % 8u, 1u + sy_rand(&seed) % 127u, (int)(sy_rand(&seed) & 1u));
            n = run(secs(dk_t60(l, dkk.l[l].decay) * 2.2f + 0.3f), 0);
            ok &= finite_all(n);
            worst = fmaxf(worst, fmaxf(pkr(L, 0, n), pkr(R, 0, n)));
            ends &= !dkv[l].on;
        }
    }
    printf("drums:   40 random kits with every effect: the loudest hit %.3f\n", worst);
    check("40 random kits with every effect (DRIVE, EQ, SHAPE, COMP, the duck's SOURCE and HOLD): finite, every voice ended",
          ok && ends);
}

/* the renders: each lane, then a bar of a beat over a held chord (a duck on each beat), twice, against the file */
static uint32_t pattern(void)
{
    static const uint8_t BEAT[16][3] = {
        {DK_KICK, DK_HATC, DK_DUCK}, {DK_HATC, 0xFF, 0xFF}, {DK_HATO, 0xFF, 0xFF}, {DK_HATC, 0xFF, 0xFF},
        {DK_SNARE, DK_HATC, DK_DUCK}, {DK_HATC, 0xFF, 0xFF}, {DK_WT1, DK_HATO, 0xFF}, {DK_CLAP, 0xFF, 0xFF},
        {DK_KICK, DK_HATC, DK_DUCK}, {DK_KICK, 0xFF, 0xFF}, {DK_HATO, 0xFF, 0xFF}, {DK_HATC, 0xFF, 0xFF},
        {DK_SNARE, DK_CLAP, DK_DUCK}, {DK_HATC, 0xFF, 0xFF}, {DK_CYM, DK_WT1, 0xFF}, {DK_HATC, DK_KICK, 0xFF},
    };
    uint32_t at = 0, s, j;
    fresh();
    sy_note_on(48, 90);
    sy_note_on(55, 90);
    sy_note_on(63, 90);
    for (s = 0; s < 32u; s++) {
        for (j = 0; j < 3u; j++)
            if (BEAT[s % 16u][j] != 0xFF)
                sy_drum_hit(BEAT[s % 16u][j], 0, s % 4u ? 90u : 120u, s % 8u == 0u);
        at = run(secs(0.125f) / SY_BLK * SY_BLK, at);
    }
    return run(secs(0.8f), at);
}
static uint64_t hash(uint32_t n)
{
    uint64_t h = 1469598103934665603ull;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = (int32_t)lrintf(L[i] * 32767.0f), b = (int32_t)lrintf(R[i] * 32767.0f);
        h = (h ^ (uint32_t)a) * 1099511628211ull;
        h = (h ^ (uint32_t)b) * 1099511628211ull;
    }
    return h;
}
static void t_golden(const char *gpath)
{
    int upd = getenv("GOLDEN_UPDATE") != 0, same = 1, match = 1;
    uint32_t l, found = 0, want = DK_N + 1u;
    FILE *g = upd ? 0 : fopen(gpath, "r"), *w = upd ? fopen(gpath, "w") : 0;
    char line[256];
    for (l = 0; l <= DK_N; l++) {
        const char *nm = l < DK_N ? DK_NAME[l] : "BEAT";
        uint32_t n;
        uint64_t h, h2;
        fresh();
        if (l < DK_N) {
            sy_drum_hit(l, 0, 100, 0);
            n = run(secs(1.5f), 0);
        } else {
            n = pattern();
            if (demo) {
                char path[256];
                snprintf(path, sizeof path, "%s/drum_beat.wav", demo);
                wav_write(path, L, R, n);
            }
        }
        h = hash(n);
        if (l < DK_N) {
            fresh();
            sy_drum_hit(l, 0, 100, 0);
            n = run(secs(1.5f), 0);
        } else {
            n = pattern();
        }
        h2 = hash(n);
        same &= h == h2;
        printf("drums:   %-7s %016llx%s\n", nm, (unsigned long long)h, h == h2 ? "" : "  <- differs run to run");
        if (w)
            fprintf(w, "%s %016llx\n", nm, (unsigned long long)h);
        if (g) {
            int hit = 0;
            rewind(g);
            while (fgets(line, sizeof line, g)) {
                char gn[64];
                char *sp = strrchr(line, ' ');
                if (!sp || sp - line >= (long)sizeof gn)
                    continue;
                memcpy(gn, line, (size_t)(sp - line));
                gn[sp - line] = 0;
                if (!strcmp(gn, nm)) {
                    hit = 1;
                    found++;
                    if (strtoull(sp + 1, 0, 16) != h) {
                        match = 0;
                        printf("drums:   %-7s differs from %s\n", nm, gpath);
                    }
                }
            }
            match &= hit;
        }
    }
    if (w)
        fclose(w), printf("drums: golden renders written to %s\n", gpath);
    if (g)
        fclose(g);
    check("determinism: each lane and a beat over a chord twice, bit for bit", same);
    check(upd ? "golden renders recorded (GOLDEN_UPDATE)" : "golden renders as recorded in tests/sy_drum_golden.txt",
          upd ? 1 : g && match && found == want);
}

/* the cost on the host: all eight lanes sounding, against the synth's idle block */
static void t_cost(void)
{
    int32_t o[2u * SY_BLK];
    uint32_t b, l, nb = 4000u;
    clock_t c0, c1, c2;
    fresh();
    c0 = clock();
    for (b = 0; b < nb; b++)
        sy_block(o);
    c1 = clock();
    for (b = 0; b < nb; b++) {
        if (b % 64u == 0u)
            for (l = 0; l < DK_N; l++)
                sy_drum_hit(l, 0, 100, 0);
        sy_block(o);
    }
    c2 = clock();
    {
        double idle = (double)(c1 - c0) / CLOCKS_PER_SEC / nb * 1e6, all = (double)(c2 - c1) / CLOCKS_PER_SEC / nb * 1e6;
        double rt = (double)SY_BLK / SY_FS * 1e6;
        printf("drums:   host cost a block: idle %.2f us, all eight sounding %.2f us (%.1f %% of real time)\n", idle, all,
               100.0 * all / rt);
        check("the cost on the host: all eight lanes sounding under 10 % of real time", all / rt < 0.10);
    }
    {                                               /* (V0.86: every effect of every drum on, and COMP) */
        clock_t c3, c4;
        for (l = 0; l < DK_N; l++) {
            dk_fx_t *f = &dkk.fx[l];
            f->dtype = DD_SOFT, f->drive = 40, f->dmix = 80, f->lo = 60, f->mid = 40, f->hi = 60, f->att = 60, f->sus = 40;
        }
        dkk.comp[0] = 50;
        c3 = clock();
        for (b = 0; b < nb; b++) {
            if (b % 64u == 0u)
                for (l = 0; l < DK_N; l++)
                    sy_drum_hit(l, 0, 100, 0);
            sy_block(o);
        }
        c4 = clock();
        {
            double all = (double)(c4 - c3) / CLOCKS_PER_SEC / nb * 1e6, rt = (double)SY_BLK / SY_FS * 1e6;
            printf("drums:   host cost a block, every effect on: %.2f us (%.1f %% of real time)\n", all, 100.0 * all / rt);
            check("the cost on the host with every effect on: under 15 % of real time", all / rt < 0.15);
        }
    }
}

int main(int argc, char **argv)
{
    t_name = "drums";
    demo = argc > 2 ? argv[2] : 0;
    t_lanes();
    t_decay();
    t_tune();
    t_clap();
    t_choke();
    t_gains();
    t_duck();
    t_health();
    t_fx();
    t_comp();
    t_duck2();
    t_atone();
    t_kits();
    t_health2();
    t_golden(argc > 1 ? argv[1] : "tests/sy_drum_golden.txt");
    t_cost();
    return t_done();
}
