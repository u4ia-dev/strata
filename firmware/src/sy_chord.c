/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA's chords (V0.7; docs/CONTROLS.md "Chords"), SCL's family after the Orchid's ideas (our own design and code):
 *   - CHORD: KEY, a key plays its in-key chord (stacked thirds of SCALE's scale from its root); FREE, the keys F3 ..
 *     C4 are eight chord buttons (MAJ MIN SUS DIM, 6 m7 M7 9: a tap latches, buttons tapped together combine; a
 *     tap of extensions alone keeps the triad) and C#4 .. G5 the roots; OFF, the keys as they are (PERFORM and BASS
 *     still act on what is held). VOICING: the chord through inversions and octaves (-3 .. +6), or AUTO, the
 *     inversion nearest the last chord;
 *   - PERFORM (the ARP button): STRUM, STRUM 2 OCT, ARP, PATTERN, HARP, SLOP, each with its own RATE (ms or a note
 *     value of GLOBAL's tempo), MOVE and FEEL;
 *   - BASS: the chord's root one or two octaves under it, on the same sound;
 *   - MIDI: the chords (their channel) and the bass (theirs) out over USB; MIDI notes in as roots, if asked.
 * The panel's keys (the main loop or TIMER5) and MIDI (its interrupt) send key events into the audio interrupt's
 * rings (sy_app.c); everything below runs there, a block at a time before the engine's, so only the interrupt
 * touches the voices. The settings (chd) are the UI's, a byte each, read here as they stand; the chord buttons write
 * the type and extensions back. chs.seq tells the UI the chord changed. Needs the engine (sy_note_on / off, sy_tempo,
 * sy_note_q, NOTE_NAMES) and SY_MIDI_OUT (the platform's USB MIDI out; none: nothing sent). */

#ifndef SY_MIDI_OUT
#define SY_MIDI_OUT(pkt) ((void)(pkt))
#endif

/* ------------------------------------------------------------------------------------------ settings --- */
#define CH_NBTN 8u                               /* FREE's chord buttons: the keys F3 .. C4 (0 .. 7) */
#define CH_MAXN 8u                               /* a chord's notes at most, the bass apart */
#define CH_NVOIC 11u                             /* VOICING: AUTO, then -3 .. +6 */
#define CH_NRATE 16u
enum { CH_OFF, CH_KEY, CH_FREE, CH_NMODE };
enum { PF_STRUM, PF_STRUM2, PF_ARP, PF_PATTERN, PF_HARP, PF_SLOP, PF_N };
enum { AR_UP, AR_DOWN, AR_UPDN, AR_RANDOM, AR_PLAYED, AR_N };
static struct {
    uint8_t mode, type, ktype, ext, voicing;     /* CHORD: CH_*, FREE's type and KEY's (0 .. 3 each), the extensions
                                                  * (bits: 6 m7 M7 9), 0 AUTO / 1 .. 10 */
    uint8_t perf, style;                         /* PERFORM: on (the ARP button), PF_* */
    uint8_t rate[PF_N], move[PF_N], feel[PF_N];  /* .. each style's own */
    uint8_t bass, boct, blvl;                    /* BASS: on, octaves under (1, 2), level (% of the chord's) */
    uint8_t min, mout, och, bch;                 /* MIDI: roots from MIDI in, out over USB, the chords' and the bass's
                                                  * channels (0 .. 15) */
    uint8_t root, vel;                           /* SCALE's root (0 C .. 11 B), the keys' velocity */
    uint16_t mask;                               /* SCALE's scale: a bit a semitone from its root */
    volatile uint8_t gen;                        /* bumped by the UI when it changed any of the above but the last three */
} chd;
static const char *const CH_MODE_NAME[CH_NMODE] = {"OFF", "KEY", "FREE"};
static const char *const CH_TYPE_NAME[2][4] = {{"MAJ", "MIN", "SUS", "DIM"}, {"IN KEY", "SUS2", "SUS4", "POWER"}};
static const char *const CH_EXT_NAME[4] = {"6", "m7", "M7", "9"};
static const char *const PF_NAME[PF_N] = {"STRUM", "STRUM 2 OCT", "ARP", "PATTERN", "HARP", "SLOP"};
static const char *const AR_NAME[AR_N] = {"UP", "DOWN", "UP-DOWN", "RANDOM", "AS PLAYED"};
static const char *const ST_NAME[3] = {"UP", "DOWN", "ALTERNATE"};
/* RATE: a gap (STRUM, SLOP: between notes) or a step (ARP, PATTERN): milliseconds, then note values (NOTE_NAMES) */
static const uint8_t CH_RATE_MS[7] = {5, 10, 20, 30, 50, 80, 120};
static const uint8_t CH_RATE_NOTE[CH_NRATE - 7u] = {16, 15, 14, 12, 11, 9, 8, 5, 3};   /* 1/32 .. 1 BAR */
/* PATTERN: sixteen rhythms of stabs, a step a bit (the first the lowest), sixteen steps */
static const uint16_t CH_PAT[16] = {0x1111, 0x5555, 0xFFFF, 0x4444, 0x4949, 0x1449, 0x94A5, 0x5451,
                                    0xDDDD, 0x9999, 0x3333, 0x1041, 0x2929, 0xD6D6, 0x34B1, 0x9595};

/* a style's MOVE: its count of choices; FEEL: its range and step */
static uint32_t ch_moves(uint32_t s) { return s == PF_ARP ? AR_N : s == PF_PATTERN ? 16u : s == PF_HARP ? 1u : 3u; }
static void ch_feel_range(uint32_t s, int32_t *lo, int32_t *hi, int32_t *step)
{
    *lo = 0, *hi = 100, *step = 5;               /* STRUM's taper, SLOP's looseness (%) */
    if (s == PF_ARP)
        *lo = 1, *hi = 3, *step = 1;             /* octaves */
    else if (s == PF_PATTERN)
        *lo = 10, *step = 10;                    /* a stab's length (% of a step) */
    else if (s == PF_HARP)
        *lo = *hi = 0;
}
static const char *ch_feel_name(uint32_t s)
{
    return s == PF_ARP ? "OCTAVES" : s == PF_PATTERN ? "GATE" : s == PF_HARP ? "FEEL" : s == PF_SLOP ? "LOOSE" : "TAPER";
}
static const char *ch_move_name(uint32_t s) { return s == PF_ARP ? "ORDER" : s == PF_PATTERN ? "PATTERN" : "DIRECTION"; }

static void ch_defaults(void)
{
    static const uint8_t RATE[PF_N] = {3, 2, 9, 9, 0, 4}, FEEL[PF_N] = {0, 0, 1, 50, 0, 50};
    uint32_t s;
    chd.mode = CH_OFF;
    chd.type = 0;
    chd.ktype = 0;
    chd.ext = 0;
    chd.voicing = 0;                             /* AUTO */
    chd.perf = 0;
    chd.style = PF_STRUM;
    for (s = 0; s < PF_N; s++) {
        chd.rate[s] = RATE[s];                   /* STRUM 30 ms, STRUM 2 OCT 20 ms, ARP and PATTERN 1/16, SLOP 50 ms */
        chd.move[s] = 0;
        chd.feel[s] = FEEL[s];
    }
    chd.bass = 0;
    chd.boct = 1;
    chd.blvl = 80;
    chd.min = 0;
    chd.mout = 0;
    chd.och = 0;                                 /* channel 1, the bass on 2 */
    chd.bch = 1;
    chd.vel = 100;
    chd.mask = 0xFFF;
    chd.gen++;
}
/* the keys go through here (else straight to the voices, as before V0.7) */
static int sy_chord_active(void) { return chd.mode != CH_OFF || chd.perf || chd.bass; }

/* -------------------------------------------------------------------------------- the audio side --- */
typedef struct { uint8_t id, note, vel; } ch_held_t;
static struct {
    ch_held_t held[CH_MAXN];                     /* roots held, in the order pressed (the last one plays); id: a key
                                                  * 0 .. 26, a MIDI note 128 + it */
    uint8_t nheld;
    uint8_t btn, gest_t, gest_e;                 /* FREE: the buttons down now; this gesture's triad (0xFF none), its
                                                  * extensions */
    uint8_t n, note[CH_MAXN], ord[CH_MAXN];      /* the chord as voiced, its notes ascending; their built order */
    uint8_t root, bass, vel, built;              /* its root (root position), the bass note (0xFF none), the velocity;
                                                  * a chord was built (HARP's) */
    uint8_t on[16][2], non;                      /* notes this side has on: the note, its part (0 the chord, 1 the bass) */
    uint8_t q[16][3], nq;                        /* note-ons waiting for a block's starts (SY_STARTS_BLK): note, vel, part */
    uint8_t harp[27];                            /* HARP: the note each key plays (0xFF none) */
    uint8_t gen, mode, perf, style;              /* chd's as last seen */
    uint8_t pf, pf_dir, pf_cur, pf_stab;         /* PERFORM: under way; ALTERNATE's way; ARP's note on (0xFF); a stab on */
    uint16_t pf_i;
    uint32_t t, pf_next, pf_off;                 /* samples since power-on (a block a step); the next step, an off due */
    uint32_t seed;
    volatile uint16_t seq;                       /* the chord changed: the UI's redraw */
} chs = {.bass = 0xFF, .pf_cur = 0xFF, .seed = 0x9E3779B9u};

static void ch_midi(int on, uint32_t note, uint32_t vel, uint32_t part)
{
    uint32_t c = (part ? chd.bch : chd.och) & 15u;
    if (chd.mout)
        SY_MIDI_OUT((on ? 0x09u | (0x90u | c) << 8 : 0x08u | (0x80u | c) << 8) | note << 16 | (on ? vel : 0u) << 24);
}
static void ch_on(uint32_t note, uint32_t vel, uint32_t part)   /* queued for the next starts */
{
    if (note > 127u || chs.nq >= 16u)
        return;
    chs.q[chs.nq][0] = (uint8_t)note;
    chs.q[chs.nq][1] = (uint8_t)sy_clampi((int32_t)vel, 1, 127);
    chs.q[chs.nq][2] = (uint8_t)part;
    chs.nq++;
}
static void ch_off(uint32_t note)                /* now (a queued start of it dropped) */
{
    uint32_t i, j;
    for (i = 0; i < chs.nq;)
        if (chs.q[i][0] == note) {
            for (j = i; j + 1u < chs.nq; j++)
                memcpy(chs.q[j], chs.q[j + 1u], 3u);
            chs.nq--;
        } else {
            i++;
        }
    for (i = 0; i < chs.non; i++)
        if (chs.on[i][0] == note) {
            sy_note_off(note);
            ch_midi(0, note, 0u, chs.on[i][1]);
            chs.on[i][0] = chs.on[chs.non - 1u][0];
            chs.on[i][1] = chs.on[chs.non - 1u][1];
            chs.non--;
            return;
        }
}
static void ch_part_off(uint32_t part)
{
    uint32_t i;
    for (i = chs.non; i-- > 0;)
        if (chs.on[i][1] == part)
            ch_off(chs.on[i][0]);
    for (i = chs.nq; i-- > 0;)
        if (chs.q[i][2] == part)
            ch_off(chs.q[i][0]);
}
static void ch_all_off(void)
{
    chs.nq = 0;
    while (chs.non)
        ch_off(chs.on[chs.non - 1u][0]);
    chs.pf_cur = 0xFF;
    chs.pf_stab = 0;
}
static int ch_is_on(uint32_t note)
{
    uint32_t i;
    for (i = 0; i < chs.non; i++)
        if (chs.on[i][0] == note)
            return 1;
    return 0;
}
/* the queued note-ons, as many as this block's starts allow (shared with the panel's and MIDI's) */
static void ch_starts(uint32_t *starts, uint32_t max)
{
    while (chs.nq && *starts < max) {
        uint32_t n = chs.q[0][0], v = chs.q[0][1], p = chs.q[0][2], i;
        (*starts)++;
        for (i = 0; i + 1u < chs.nq; i++)
            memcpy(chs.q[i], chs.q[i + 1u], 3u);
        chs.nq--;
        if (!ch_is_on(n) && chs.non < 16u) {
            chs.on[chs.non][0] = (uint8_t)n;
            chs.on[chs.non][1] = (uint8_t)p;
            chs.non++;
        }
        sy_note_on(n, v);                        /* (a note already on: its voice again) */
        ch_midi(1, n, v, p);
    }
}

/* ------------------------------------------------------------------------------------------- chords --- */
static const uint8_t CH_TRIAD[4][3] = {{0, 4, 7}, {0, 3, 7}, {0, 5, 7}, {0, 3, 6}};
static const uint8_t CH_EXT_ST[4] = {9, 10, 11, 14};

/* SCALE's pitch classes from its root, ascending (at least one); their count */
static uint32_t ch_scale(uint8_t *s)
{
    uint32_t i, m = 0;
    for (i = 0; i < 12u; i++)
        if ((chd.mask >> i) & 1u)
            s[m++] = (uint8_t)i;
    if (!m)
        s[m++] = 0;
    return m;
}
static void ch_sort(uint8_t *a, uint32_t n)
{
    uint32_t i, j;
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && a[j - 1u] > a[j]; j--) {
            uint8_t t = a[j];
            a[j] = a[j - 1u];
            a[j - 1u] = t;
        }
}
/* the chord of root r (a key's note) by CHORD's settings, root position: its notes into nt (ascending), the order
 * they were built in into od; its count. KEY: the root moved down onto the scale first */
static uint32_t ch_build(int32_t r, uint8_t *nt, uint8_t *od, uint8_t *root)
{
    int32_t iv[CH_MAXN];
    uint32_t n = 0, i, j;
    if (chd.mode == CH_KEY) {
        static const uint8_t DEG[4][3] = {{0, 2, 4}, {0, 1, 4}, {0, 3, 4}, {0, 4, 7}};
        uint8_t s[12];
        uint32_t m = ch_scale(s), k, t = chd.ktype & 3u;
        int32_t rel = ((r - chd.root) % 12 + 12) % 12;
        for (k = 0; k < 12u && !((chd.mask >> (uint32_t)rel) & 1u) && chd.mask; k++, r--)
            rel = (rel + 11) % 12;
        for (k = 0; k < m && s[k] != rel; k++)
            ;
        if (k == m)
            k = 0;
#define CH_DEG(dd) ((int32_t)s[(k + (dd)) % m] - (int32_t)s[k] + 12 * (int32_t)((k + (dd)) / m))
        if (t == 3u) {                           /* POWER: the root, its fifth, the octave */
            iv[n++] = 0;
            iv[n++] = 7;
            iv[n++] = 12;
        } else {
            for (i = 0; i < 3u; i++)
                iv[n++] = CH_DEG(DEG[t][i]);
        }
        if (chd.ext & 1u)                        /* the in-key 6th, 7th (m7 or M7), 9th */
            iv[n++] = CH_DEG(5u);
        if (chd.ext & 6u)
            iv[n++] = CH_DEG(6u);
        if (chd.ext & 8u)
            iv[n++] = CH_DEG(8u);
#undef CH_DEG
    } else {
        for (i = 0; i < 3u; i++)
            iv[n++] = CH_TRIAD[chd.type & 3u][i];
        for (i = 0; i < 4u; i++)
            if ((chd.ext >> i) & 1u)
                iv[n++] = CH_EXT_ST[i];
    }
    *root = (uint8_t)sy_clampi(r, 0, 127);
    for (i = j = 0; i < n; i++) {                /* (in range, each pitch once) */
        int32_t x = r + iv[i];
        uint32_t k;
        while (x > 127)
            x -= 12;
        for (k = 0; k < j && od[k] != x; k++)
            ;
        if (x >= 0 && k == j)
            od[j++] = (uint8_t)x;
    }
    memcpy(nt, od, j);
    ch_sort(nt, j);
    return j;
}
/* the chord's notes moved v inversions up (each its lowest an octave up) or down (each its highest an octave down) */
static void ch_invert(uint8_t *nt, uint32_t n, int32_t v)
{
    for (; v > 0 && n > 1u; v--) {
        if (nt[0] + 12u > 127u)
            break;
        nt[0] = (uint8_t)(nt[0] + 12u);
        ch_sort(nt, n);
    }
    for (; v < 0 && n > 1u; v++) {
        if (nt[n - 1u] < 12u)
            break;
        nt[n - 1u] = (uint8_t)(nt[n - 1u] - 12u);
        ch_sort(nt, n);
    }
}
/* AUTO: of the voicings -3 .. +3, the one whose notes move least from the chord before's (and stays nearest it) */
static void ch_auto(uint8_t *nt, uint32_t n)
{
    uint8_t best[CH_MAXN], t[CH_MAXN];
    int32_t v, bc = 0x7FFFFFFF;
    if (!chs.n || n < 2u)
        return;
    for (v = -3; v <= 3; v++) {
        int32_t c = 0, lo = 0, hi = 0;
        uint32_t i, m = n < chs.n ? n : chs.n;
        memcpy(t, nt, n);
        ch_invert(t, n, v);
        for (i = 0; i < m; i++)
            c += t[i] > chs.note[i] ? t[i] - chs.note[i] : chs.note[i] - t[i];
        for (i = 0; i < n; i++)
            lo += t[i];
        for (i = 0; i < chs.n; i++)
            hi += chs.note[i];
        lo = lo / (int32_t)n - hi / (int32_t)chs.n;
        c = 4 * c + 3 * (lo < 0 ? -lo : lo) + (v < 0 ? -v : v);
        if (c < bc) {
            bc = c;
            memcpy(best, t, n);
        }
    }
    memcpy(nt, best, n);
}

/* ------------------------------------------------------------------------------------------- perform --- */
static uint32_t ch_rate_smp(void)               /* the style's RATE in samples (a block at least) */
{
    uint32_t r = chd.rate[chd.style % PF_N] % CH_NRATE;
    float s = r < 7u ? (float)CH_RATE_MS[r] * (SY_FS / 1000.0f)
                     : sy_note_q(CH_RATE_NOTE[r - 7u]) * SY_FS * 60.0f / (sy_tempo > 1.0f ? sy_tempo : 120.0f);
    return s < (float)SY_BLK ? SY_BLK : (uint32_t)s;
}
/* STRUM's and SLOP's notes in the order they sound (STRUM 2 OCT: the chord and an octave up, 8 at most); their count */
static uint32_t ch_strum_list(uint8_t *l)
{
    uint32_t n = chs.n, i, up;
    memcpy(l, chs.note, n);
    if (chd.style == PF_STRUM2)
        for (i = 0; i < chs.n && n < CH_MAXN; i++)
            if (chs.note[i] + 12u <= 127u)
                l[n++] = (uint8_t)(chs.note[i] + 12u);
    ch_sort(l, n);
    up = chd.move[chd.style] % 3u == 0u || (chd.move[chd.style] % 3u == 2u && !chs.pf_dir);
    if (!up)
        for (i = 0; i < n / 2u; i++) {
            uint8_t t = l[i];
            l[i] = l[n - 1u - i];
            l[n - 1u - i] = t;
        }
    return n;
}
/* ARP's step i: its note (the chord over its octaves in the order MOVE picks) */
static uint32_t ch_arp_note(uint32_t i)
{
    uint32_t oct = (uint32_t)sy_clampi(chd.feel[PF_ARP], 1, 3), n = chs.n * oct, k, mv = chd.move[PF_ARP] % AR_N, x;
    const uint8_t *src = mv == AR_PLAYED ? chs.ord : chs.note;
    if (!n)
        return 0xFFu;
    if (mv == AR_UPDN && n > 1u) {
        k = i % (2u * n - 2u);
        k = k < n ? k : 2u * n - 2u - k;
    } else if (mv == AR_RANDOM) {
        k = sy_rand(&chs.seed) % n;
    } else {
        k = i % n;
        if (mv == AR_DOWN)
            k = n - 1u - k;
    }
    x = src[k % chs.n] + 12u * (k / chs.n);
    return x > 127u ? src[k % chs.n] : x;
}
static void ch_perf_start(void)
{
    chs.pf = 1;
    chs.pf_i = 0;
    chs.pf_next = chs.t;
    chs.pf_off = 0;
    chs.pf_cur = 0xFF;
    chs.pf_stab = 0;
    chs.pf_dir = (uint8_t)!chs.pf_dir;
}
/* a block of PERFORM: what is due now */
static void ch_perform(void)
{
    uint32_t st = chd.style % PF_N, rate = ch_rate_smp(), i;
    int32_t feel = chd.feel[st];
    if (!chs.n || st == PF_HARP)
        return;
    if (st == PF_STRUM || st == PF_STRUM2 || st == PF_SLOP) {
        uint8_t l[CH_MAXN];
        uint32_t n = ch_strum_list(l);
        while (chs.pf_i < n && (int32_t)(chs.t - chs.pf_next) >= 0) {
            int32_t v = chs.vel;
            if (st == PF_SLOP) {                 /* (LOOSE 100: a gap of 15 .. 100 % of RATE, the level +-50 %) */
                v += v * ((int32_t)(sy_rand(&chs.seed) % 101u) - 50) * feel / 10000;
                chs.pf_next += rate * (100u - (uint32_t)feel * (sy_rand(&chs.seed) % 86u) / 100u) / 100u;
            } else {                             /* (TAPER: the last note at 100 - TAPER/2 %) */
                v -= n > 1u ? v * feel * chs.pf_i / (int32_t)(n - 1u) / 200 : 0;
                chs.pf_next += rate;
            }
            ch_on(l[chs.pf_i], (uint32_t)v, 0u);
            chs.pf_i++;
        }
        return;
    }
    if (chs.pf_off && (int32_t)(chs.t - chs.pf_off) >= 0) {   /* the step's note or stab ends */
        if (st == PF_ARP && chs.pf_cur != 0xFF)
            ch_off(chs.pf_cur);
        else if (chs.pf_stab)
            ch_part_off(0u);
        chs.pf_cur = 0xFF;
        chs.pf_stab = 0;
        chs.pf_off = 0;
    }
    if ((int32_t)(chs.t - chs.pf_next) < 0)
        return;
    if (st == PF_ARP) {                          /* (half a step long) */
        uint32_t x = ch_arp_note(chs.pf_i);
        if (chs.pf_cur != 0xFF)
            ch_off(chs.pf_cur);
        if (x <= 127u) {
            ch_on(x, chs.vel, 0u);
            chs.pf_cur = (uint8_t)x;
            chs.pf_off = (chs.t + rate / 2u) | 1u;
        }
    } else {                                     /* PATTERN: a stab where its step's bit is (the beats louder) */
        uint32_t step = chs.pf_i % 16u, v = chs.vel * (step % 4u ? 80u : 100u) / 100u;
        if ((CH_PAT[chd.move[PF_PATTERN] % 16u] >> step) & 1u) {
            ch_part_off(0u);
            for (i = 0; i < chs.n; i++)
                ch_on(chs.note[i], v, 0u);
            chs.pf_stab = 1;
            chs.pf_off = (chs.t + rate * (uint32_t)sy_clampi(feel, 10, 100) / 100u) | 1u;
        }
    }
    chs.pf_i++;
    chs.pf_next += rate;
}

/* ------------------------------------------------------------------------------------------- playing --- */
/* the chord for what is held now: KEY and FREE, the last root's chord, voiced; OFF, the notes held. restart: a new
 * chord (everything it plays starts again, PERFORM from its start); else (OFF) only what changed */
static void ch_play(int restart)
{
    uint8_t nt[CH_MAXN], od[CH_MAXN], root = 0;
    uint32_t n = 0, i, bass = 0xFFu;
    if (!chs.nheld) {
        ch_all_off();
        chs.pf = 0;
        chs.seq++;
        return;
    }
    if (chd.mode == CH_OFF) {
        for (i = 0; i < chs.nheld; i++)
            od[n++] = chs.held[i].note;
        memcpy(nt, od, n);
        ch_sort(nt, n);
        root = nt[0];
    } else {
        n = ch_build(chs.held[chs.nheld - 1u].note, nt, od, &root);
        if (chd.voicing % CH_NVOIC)
            ch_invert(nt, n, (int32_t)(chd.voicing % CH_NVOIC) - 4);
        else if (restart)
            ch_auto(nt, n);
    }
    if (chd.bass && n) {                         /* the root under the chord's lowest note, one or two octaves */
        int32_t b = root - 12 * (int32_t)sy_clampi(chd.boct, 1, 2);
        while (b >= nt[0] - 12 * ((int32_t)sy_clampi(chd.boct, 1, 2) - 1))
            b -= 12;
        if (b >= 12)
            bass = (uint32_t)b;
    }
    if (restart) {
        ch_all_off();
    } else {
        for (i = chs.non; i-- > 0;) {            /* (OFF: what was let go ends; the bass if it moved) */
            uint32_t x = chs.on[i][0], k;
            for (k = 0; k < n && nt[k] != x; k++)
                ;
            if ((chs.on[i][1] == 0u && k == n && (!chd.perf || chd.style != PF_ARP || x != chs.pf_cur)) ||
                (chs.on[i][1] == 1u && x != bass))
                ch_off(x);
        }
    }
    memcpy(chs.note, nt, n);
    memcpy(chs.ord, od, n);
    chs.n = (uint8_t)n;
    chs.root = root;
    chs.vel = chs.held[chs.nheld - 1u].vel;
    chs.built = 1;
    if (bass <= 127u && (restart || chs.bass != bass || !ch_is_on(bass)))
        ch_on(bass, chs.vel * sy_clampi(chd.blvl, 1, 100) / 100u, 1u);
    chs.bass = (uint8_t)bass;
    if (chd.perf && chd.style != PF_HARP) {
        if (restart || !chs.pf)
            ch_perf_start();
        else if (chd.style == PF_STRUM || chd.style == PF_STRUM2 || chd.style == PF_SLOP)
            for (i = 0; i < n; i++)              /* (OFF: a note held anew joins at once) */
                if (!ch_is_on(nt[i]))
                    ch_on(nt[i], chs.vel, 0u);
    } else {
        for (i = 0; i < n; i++)
            if (restart || !ch_is_on(nt[i]))
                ch_on(nt[i], chs.vel, 0u);
    }
    chs.seq++;
}
/* FREE's chord buttons: a press starts a gesture or adds to it; the gesture is the chord (no triad in it: the one
 * before kept) */
static void ch_button(uint32_t b, int down)
{
    if (!down) {
        chs.btn &= (uint8_t)~(1u << b);
        return;
    }
    if (!chs.btn) {
        chs.gest_t = 0xFF;
        chs.gest_e = 0;
    }
    chs.btn |= (uint8_t)(1u << b);
    if (b < 4u)
        chs.gest_t = (uint8_t)b;
    else
        chs.gest_e |= (uint8_t)(1u << (b - 4u));
    if (chs.gest_t != 0xFF)
        chd.type = chs.gest_t;
    chd.ext = chs.gest_e;
    if (chs.nheld)
        ch_play(1);
    else if (chs.built && chd.perf && chd.style == PF_HARP)
        chs.n = (uint8_t)ch_build(chs.root, chs.note, chs.ord, &chs.root);
    chs.seq++;
}
/* HARP: the keys are strings over four octaves from the chord's root's in octave 3: each plays the chord's note at
 * or under its place (OFF: the scale's); a key on the note just played adds nothing */
static void ch_harp(uint32_t k, int down)
{
    uint32_t first = chd.mode == CH_FREE ? CH_NBTN : 0u, nstr = 27u - first, s, pcs = 0, i;
    int32_t at, x;
    if (k >= 27u)
        return;
    if (!down) {
        if (chs.harp[k] <= 127u) {
            uint32_t x2 = chs.harp[k], j, still = 0;
            chs.harp[k] = 0xFF;
            for (j = 0; j < 27u; j++)
                still |= chs.harp[j] == x2;
            if (!still)
                ch_off(x2);
        }
        return;
    }
    if (k < first)
        return;
    if (!chs.built) {                            /* (nothing played yet: the chord on SCALE's root at C4) */
        chs.n = (uint8_t)ch_build(60 + chd.root, chs.note, chs.ord, &chs.root);
        chs.built = 1;
    }
    if (chd.mode == CH_OFF)
        pcs = (uint32_t)chd.mask << chd.root | (uint32_t)chd.mask >> (12u - chd.root);
    else
        for (i = 0; i < chs.n; i++)
            pcs |= 1u << (chs.note[i] % 12u);
    pcs &= 0xFFFu;
    if (!pcs)
        return;
    s = k - first;
    at = 48 + (int32_t)(chs.root % 12u) + (int32_t)(s * 48u / nstr);
    for (x = at; x >= 0 && !((pcs >> (uint32_t)(x % 12)) & 1u); x--)
        ;
    if (x < 0)
        return;
    chs.harp[k] = (uint8_t)x;
    ch_on((uint32_t)x, chd.vel, 0u);
    chs.seq++;
}

/* ------------------------------------------------------------------------------------- the interface --- */
/* a key (0 .. 26) or a MIDI note (128 + it) down or up: note, the note it plays as SCALE fits it (0xFF none) */
static void sy_chord_key(uint32_t id, uint32_t note, uint32_t vel, int down)
{
    uint32_t i;
    if (chd.mode == CH_FREE && id < CH_NBTN) {
        ch_button(id, down);
        return;
    }
    if (chd.perf && chd.style == PF_HARP && id < 27u) {
        ch_harp(id, down);
        return;
    }
    for (i = 0; i < chs.nheld && chs.held[i].id != id; i++)
        ;
    if (i < chs.nheld) {                         /* (out of the list: pressed again, or let go) */
        int top = i + 1u == chs.nheld;
        for (; i + 1u < chs.nheld; i++)
            chs.held[i] = chs.held[i + 1u];
        chs.nheld--;
        if (!down) {
            ch_play(chd.mode != CH_OFF && top && chs.nheld);
            return;
        }
    }
    if (!down || note > 127u)
        return;
    if (chs.nheld == CH_MAXN) {
        for (i = 0; i + 1u < CH_MAXN; i++)
            chs.held[i] = chs.held[i + 1u];
        chs.nheld--;
    }
    chs.held[chs.nheld].id = (uint8_t)id;
    chs.held[chs.nheld].note = (uint8_t)note;
    chs.held[chs.nheld].vel = (uint8_t)sy_clampi((int32_t)vel, 1, 127);
    chs.nheld++;
    ch_play(chd.mode != CH_OFF || chs.nheld == 1u);
}
/* SCL held + a key while HARP plays: the chord's root (the scale's otherwise: the UI's) */
static void sy_chord_root(uint32_t note)
{
    chs.n = (uint8_t)ch_build((int32_t)note, chs.note, chs.ord, &chs.root);
    chs.built = 1;
    chs.seq++;
}
/* everything this side plays ends; nothing held (a panic, a sound's load) */
static void sy_chord_reset(void)
{
    uint32_t i;
    ch_all_off();
    chs.nheld = 0;
    chs.btn = 0;
    chs.pf = 0;
    for (i = 0; i < 27u; i++)
        chs.harp[i] = 0xFF;
    chs.seq++;
}
/* a block: the settings' changes, PERFORM's steps, the note-ons queued (starts: this block's so far, max of them) */
static void sy_chord_block(uint32_t *starts, uint32_t max)
{
    if (chd.gen != chs.gen) {                    /* (the mode, PERFORM or its style: everything again; else the */
        int all = chd.mode != chs.mode || chd.perf != chs.perf || chd.style != chs.style;   /* chord voiced again) */
        chs.gen = chd.gen;
        chs.mode = chd.mode;
        chs.perf = chd.perf;
        chs.style = chd.style;
        if (all)
            sy_chord_reset();
        else if (chs.nheld)
            ch_play(1);
    }
    if (chs.pf)
        ch_perform();
    ch_starts(starts, max);
    chs.t += SY_BLK;
}

/* ---------------------------------------------------------------------------------------- the name --- */
/* the chord's name from its notes and root: "Cm7/G" (a sharp's root "C#"; a bass under it other than the root: after
 * the slash), "" none */
static const char *const CH_PC[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static char *ch_name_of(char *b, const uint8_t *nt, uint32_t n, uint32_t root)
{
    uint32_t m = 0, i, th, fi, sv;
    const char *q = "", *x = "";
    char *e = b;
    *b = 0;
    if (!n)
        return b;
    for (i = 0; i < n; i++)
        m |= 1u << ((nt[i] + 12u - root % 12u) % 12u);
    th = m & 16u ? 4u : m & 8u ? 3u : 0u;
    fi = m & 128u ? 7u : m & 64u ? 6u : m & 256u ? 8u : 0u;
    sv = m & 1024u ? 10u : m & 2048u ? 11u : 0u;
    if (th == 4u)
        q = fi == 8u ? "+" : "";
    else if (th == 3u)
        q = fi == 6u ? "dim" : "m";
    else
        q = m & 32u ? "sus4" : m & 4u ? "sus2" : fi == 7u && n <= 3u ? "5" : "";
    if (th == 3u && fi == 6u && sv == 10u)
        q = "m7b5", sv = 0u;
    else if (th == 3u && fi == 6u && (m & 512u) && !sv)
        q = "dim7", m &= ~512u;
    if (sv == 10u)
        x = th && (m & 4u) ? "9" : "7";
    else if (sv == 11u)
        x = m & 4u && th ? "maj9" : "maj7";
    else if (m & 512u)
        x = m & 4u && th ? "6/9" : "6";
    else if (m & 4u && th)
        x = "add9";
    e = sp_put(e, CH_PC[root % 12u]);
    if (sv && th == 0u && (q[0] == 's'))         /* (C7sus4: the seventh before the sus) */
        e = sp_put(sp_put(e, x), q);
    else
        e = sp_put(sp_put(e, q), x);
    if (nt[0] % 12u != root % 12u)
        sp_put(sp_put(e, "/"), CH_PC[nt[0] % 12u]);
    return b;
}
static char *sy_chord_name(char *b) { return ch_name_of(b, chs.note, chs.n, chs.root); }
