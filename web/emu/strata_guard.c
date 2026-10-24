/* SPDX-License-Identifier: GPL-3.0-only */
/* The CPU guard's transitions on the host (V0.87.1, DECISIONS item 13): web/emu/strata_web.c built natively, a starter
 * sound held (8 notes at velocity 127, or a chord let go so that only its tails ring), then one of the guard's steps
 * forced at time T as the guard takes it (sy_engine.c sy_g_shed, sy_g_step, sy_g_fx): a voice shed, the unison
 * budget a step down (or four, a half apart), the effects at low cost and back. The process forks at T: the child
 * takes the step, the parent does not, both render on, and the two outputs (the master's Q15, before MASTER) are
 * compared over the 200 ms after T:
 *   JUMP   the largest step between two samples after T against the largest in the second before (x; above 1: a
 *          jump the music had not made)
 *   HF+    the high frequencies the step added: in 3 ms frames, the energy of the second difference (a high pass, +12
 *          dB an octave) of B less A's, the largest, against A's mean over the 200 ms (dB; "-" none added: a fade
 *          smoother than the sound; near 0 dB or above, a click as loud as the music's own highs)
 *   SPIKE  B's 1 ms frames of the second difference, each against the median of the eleven around it (5 a side): the
 *          one that stands out most, less the same of A (dB): a click stands out of its neighbours, a level that rises
 *          or falls over several milliseconds does not (for a step that adds sound, the effects back: HF+ counts what
 *          it adds, SPIKE whether it comes as a click)
 *   DIFF   the change's peak (B - A) against the output's (dB)
 * Each case's WAV (the stepped run, 0.5 s before T to 1 s after) into build/guard/.
 *   cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/strata_guard web/emu/strata_guard.c -lm
 *   build/emu/strata_guard */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#define WEB_NO_LINT 1
#include "strata_web.c"

static const uint8_t CH8[8] = { 48, 55, 60, 64, 67, 71, 74, 79 };   /* C3 G3 C4 E4 G4 B4 D5 G5 */
#define FS 44118u
#define PRE (FS)                                  /* the second before T (the steady output) */
#define WIN (FS / 5u)                             /* the 200 ms after */
#define WAV_PRE (FS / 2u)
#define WAV_POST (FS)

enum { GE_SHED, GE_STEP, GE_STEP4, GE_FXLO, GE_FXHI, GE_FXNOW, GE_N };
static const char *const GE_NAME[GE_N] = {"voice shed", "budget -1 step", "budget -4 steps", "fx_lo on", "fx_lo off",
                                          "fx_lo at once"};

static void guard_step(uint32_t e)               /* the guard's step, as it takes it */
{
    if (e == GE_SHED)
        sy_g_shed();
    else if (e == GE_STEP || e == GE_STEP4)
        sy_g_step();
    else
        sy_g_fx(e == GE_FXLO || e == GE_FXNOW, e == GE_FXNOW);
}

static void run_ms(uint32_t ms)
{
    while (ms--)
        web_ms();
}
/* n samples of the master's output (Q15 as float, left and right) into x; the halves as they render; e >= 0: the
 * guard's step at the first half (four: one a half) */
static void render(float *x, uint32_t n, int32_t e)
{
    uint32_t k = 0, halves = 0, done = 0xFFFFFFFFu;
    while (k < n) {
        uint32_t h = web_half;
        if (e >= 0 && done != halves && (halves == 0u || (e == GE_STEP4 && halves < 4u))) {
            guard_step((uint32_t)e);
            done = halves;
        }
        web_ms();
        if (web_half != h) {
            const int32_t *o = &sy_abuf[h * SY_HALF_WORDS];
            uint32_t j;
            for (j = 0; j < SY_HALF && k < n; j++, k++) {
                x[2u * k] = (float)(o[2u * j] >> SY_OUT_SHIFT) * (1.0f / 32768.0f);
                x[2u * k + 1u] = (float)(o[2u * j + 1u] >> SY_OUT_SHIFT) * (1.0f / 32768.0f);
            }
            halves++;
        }
    }
}

static void wav(const char *path, const float *x, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, d = n * 4u, v;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f);
    v = 36u + d; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f);
    v = 1u | 2u << 16; fwrite(&v, 4, 1, f);
    v = FS; fwrite(&v, 4, 1, f);
    v = FS * 4u; fwrite(&v, 4, 1, f);
    v = 4u | 16u << 16; fwrite(&v, 4, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&d, 4, 1, f);
    for (i = 0; i < 2u * n; i++) {
        float s = x[i] * 32768.0f;
        int16_t q = (int16_t)(s > 32767.0f ? 32767 : s < -32768.0f ? -32768 : (int32_t)s);
        fwrite(&q, 2, 1, f);
    }
    fclose(f);
}

static double db(double a, double b) { return 20.0 * log10((a > 1e-12 ? a : 1e-12) / (b > 1e-12 ? b : 1e-12)); }

typedef struct { int8_t sound; uint8_t notes, tail, fxlo; uint8_t e; const char *name; int16_t vol; } case_t;
static const case_t CASES[] = {
    {2, 8, 0, 0, GE_SHED, "pad8_shed"},          {0, 8, 0, 0, GE_SHED, "supersaw8_shed"},
    {6, 2, 0, 0, GE_SHED, "vowelpad2_shed"},     {2, 1, 0, 0, GE_SHED, "pad1_shed"},
    {0, 8, 0, 0, GE_STEP, "supersaw8_step"},     {2, 8, 0, 0, GE_STEP, "pad8_step"},
    {0, 8, 0, 0, GE_STEP4, "supersaw8_step4"},   {2, 8, 0, 0, GE_STEP4, "pad8_step4"},
    {6, 8, 0, 0, GE_STEP4, "vowelpad8_step4"},
    {2, 8, 0, 0, GE_FXLO, "pad8_fxlo"},          {2, 8, 1, 0, GE_FXLO, "padtail_fxlo"},
    {0, 8, 1, 0, GE_FXLO, "supersawtail_fxlo"},  {6, 4, 0, 0, GE_FXLO, "vowelpad4_fxlo"},
    {7, 4, 0, 0, GE_FXLO, "ottgrowl4_fxlo"},     {2, 8, 1, 1, GE_FXHI, "padtail_fxhi"},
    {0, 8, 1, 1, GE_FXHI, "supersawtail_fxhi"},
    {2, 8, 1, 0, GE_FXNOW, "padtail_fxnow"},     {0, 8, 1, 0, GE_FXNOW, "supersawtail_fxnow"},
    {7, 1, 0, 0, GE_FXNOW, "ottgrowlquiet_fxnow", 250},
    {7, 1, 0, 0, GE_FXLO, "ottgrowlquiet_fxlo", 250}, {7, 1, 0, 1, GE_FXHI, "ottgrowlquiet_fxhi", 250},
    {6, 4, 0, 0, GE_FXLO, "vowelpadquiet_fxlo", 250}, {6, 4, 0, 1, GE_FXHI, "vowelpadquiet_fxhi", 250},
};
#define NCASE (sizeof CASES / sizeof CASES[0])

int main(int argc, char **argv)
{
    static float pre[2u * PRE], a[2u * (WAV_POST)];
    float *b = mmap(0, sizeof(float) * 2u * WAV_POST, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    uint32_t c, i;
    int only = argc > 1 ? atoi(argv[1]) : -1;
    if (b == MAP_FAILED)
        return 1;
    system("mkdir -p build/guard");
    setvbuf(stdout, 0, _IONBF, 0);
    printf("%-20s %-16s %7s %8s %8s %8s\n", "case", "step", "JUMP", "HF+", "SPIKE", "DIFF");
    for (c = 0; c < NCASE; c++) {
        const case_t *k = &CASES[c];
        pid_t pid;
        double jpre = 0, jpost = 0, d2d = 0, d2a = 0, dpk = 0, apk = 0, spk[2] = {0, 0};
        if (only >= 0 && (uint32_t)only != c)
            continue;
        fflush(stdout);
        pid = fork();
        if (pid == 0) {                              /* (each case from power-on: the same events, the same sound) */
            pid_t p2;
            web_nor_erase();
            web_boot();
            run_ms(400);
            web_test_sound_set((uint32_t)k->sound);
            run_ms(400);
            if (k->vol)                              /* (VOLUME down: a quiet input, OTT's UPWARD at work) */
                web_test_set(SP_V_VOL, k->vol);
            for (i = 0; i < k->notes; i++)
                web_midi(0x90, CH8[i], 127);
            run_ms(1500);
            if (k->tail) {
                for (i = 0; i < k->notes; i++)
                    web_midi(0x80, CH8[i], 0);
                run_ms(300);
            }
            render(pre, PRE / 2u, -1);
            if (k->fxlo) {                           /* (on half a second before T: the emulator's guard, its load 0, */
                sy_g_fx(1, 0);                       /* steps back after a second of calm) */
                sy_g.calm = 0;
            }
            render(pre + PRE, PRE / 2u, -1);
            p2 = fork();
            if (p2 == 0) {
                render(b, WAV_POST, (int32_t)k->e);
                _exit(0);
            }
            render(a, WAV_POST, -1);
            waitpid(p2, 0, 0);
            for (i = 1; i < PRE; i++) {
                double d = fabs((double)pre[2u * i] - pre[2u * i - 2u]);
                jpre = d > jpre ? d : jpre;
            }
            for (i = 1; i < WIN; i++) {
                uint32_t ch;
                for (ch = 0; ch < 2u; ch++) {
                    double df = fabs((double)b[2u * i + ch] - a[2u * i + ch]);
                    if (ch == 0u && fabs((double)b[2u * i] - b[2u * i - 2u]) > jpost)
                        jpost = fabs((double)b[2u * i] - b[2u * i - 2u]);
                    dpk = df > dpk ? df : dpk;
                    apk = fabs(a[2u * i + ch]) > apk ? fabs(a[2u * i + ch]) : apk;
                }
            }
            {                                        /* (HF+: 3 ms frames of the second difference's energy) */
                const uint32_t fr = FS * 3u / 1000u;
                double ea_sum = 0, add = -1;
                uint32_t f0, nf = 0;
                for (f0 = 2; f0 + fr <= WIN; f0 += fr, nf++) {
                    double eb = 0, ea = 0;
                    uint32_t j, ch;
                    for (j = f0; j < f0 + fr; j++)
                        for (ch = 0; ch < 2u; ch++) {
                            double xb = b[2u * j + ch] - 2.0 * b[2u * j - 2u + ch] + b[2u * j - 4u + ch];
                            double xa = a[2u * j + ch] - 2.0 * a[2u * j - 2u + ch] + a[2u * j - 4u + ch];
                            eb += xb * xb, ea += xa * xa;
                        }
                    ea_sum += ea;
                    if (eb - ea > add)
                        add = eb - ea;
                }
                d2d = add, d2a = nf ? ea_sum / nf : 0;
            }
            {                                        /* (SPIKE: 1 ms frames, the loudest against the median) */
                const uint32_t fr = FS / 1000u, nfr = (WIN - 2u) / (FS / 1000u);
                static double fe[2][256];
                uint32_t f, j, ch, v;
                for (v = 0; v < 2u; v++) {
                    const float *x = v ? b : a;
                    double mx = 0, t;
                    for (f = 0; f < nfr && f < 256u; f++) {
                        double e = 0;
                        for (j = 2u + f * fr; j < 2u + (f + 1u) * fr; j++)
                            for (ch = 0; ch < 2u; ch++) {
                                double d = x[2u * j + ch] - 2.0 * x[2u * j - 2u + ch] + x[2u * j - 4u + ch];
                                e += d * d;
                            }
                        fe[v][f] = e;
                        mx = e > mx ? e : mx;
                    }
                    mx = 0;
                    for (f = 5; f + 5u < nfr; f++) {  /* (each against its neighbours' median) */
                        double w[11], r;
                        uint32_t q, p;
                        for (q = 0; q < 11u; q++)
                            w[q] = fe[v][f - 5u + q];
                        for (q = 1; q < 11u; q++)
                            for (p = q; p > 0 && w[p - 1u] > w[p]; p--)
                                t = w[p], w[p] = w[p - 1u], w[p - 1u] = t;
                        r = fe[v][f] / (w[5] > 1e-30 ? w[5] : 1e-30);
                        mx = r > mx ? r : mx;
                    }
                    spk[v] = 10.0 * log10(mx > 1e-30 ? mx : 1e-30);
                }
            }
            {
                static float w[2u * (WAV_PRE + WAV_POST)];
                char path[96];
                memcpy(w, pre + 2u * (PRE - WAV_PRE), sizeof(float) * 2u * WAV_PRE);
                memcpy(w + 2u * WAV_PRE, b, sizeof(float) * 2u * WAV_POST);
                snprintf(path, sizeof path, "build/guard/%s.wav", k->name);
                wav(path, w, WAV_PRE + WAV_POST);
            }
            if (d2d > 0.0 && d2a > 0.0)
                printf("%-20s %-16s %6.2fx %+7.1f %+7.1f %+7.1f\n", k->name, GE_NAME[k->e],
                       jpost / (jpre > 0 ? jpre : 1e-9), 10.0 * log10(d2d / d2a), spk[1] - spk[0], db(dpk, apk));
            else
                printf("%-20s %-16s %6.2fx %7s %+7.1f %+7.1f\n", k->name, GE_NAME[k->e], jpost / (jpre > 0 ? jpre : 1e-9),
                       "-", spk[1] - spk[0], db(dpk, apk));
            fflush(stdout);
            _exit(0);
        }
        waitpid(pid, 0, 0);
    }
    return 0;
}
