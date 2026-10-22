/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* V0.85: the drum synth (firmware/src/sy_drum.c) through sy_block, as the audio interrupt runs it:
 *   build/host/sy_drum_test tests/sy_drum_golden.txt [DEMODIR]       GOLDEN_UPDATE=1 rewrites the golden file
 * every lane sounds, stays under its bounds and ends; DECAY is the time to -60 dB; TUNE the KICK's pitch; CLAP's four
 * bursts SPREAD apart; HAT CL chokes HAT OP; ACCENT's lift, VELOCITY's share; PAN and the track's gains (a muted
 * track silent); the WT lanes on OSC A's and B's tables (none built: no hit); the duck's AMOUNT, ATTACK, RELEASE and
 * SHAPEs, on the synth's sound and never on the drums; 40 random kits healthy; the renders twice the same and as
 * recorded; the cost on the host. */
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
    t_golden(argc > 1 ? argv[1] : "tests/sy_drum_golden.txt");
    t_cost();
    return t_done();
}
