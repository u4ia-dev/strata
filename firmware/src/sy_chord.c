/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA's chords (V0.7; docs/CONTROLS.md "Chords"), SCL's family after the Orchid's ideas (our own design and code),
 * and the arpeggiator (V0.75; docs/CONTROLS.md "The ARP"):
 *   - CHORD: KEY, a key plays its in-key chord (stacked thirds of SCALE's scale from its root); FREE, the keys F3 ..
 *     C4 are eight chord buttons (MAJ MIN SUS DIM, 6 m7 M7 9: a tap latches, buttons tapped together combine; a
 *     tap of extensions alone keeps the triad) and C#4 .. G5 the roots; OFF, the keys as they are (PERFORM and BASS
 *     still act on what is held). VOICING: the chord through inversions and octaves (-3 .. +6), or AUTO, the
 *     inversion nearest the last chord;
 *   - PERFORM (the ARP button; ARP held: its pages): STRUM, STRUM 2 OCT, ARP, PATTERN, HARP, SLOP, each with its own
 *     RATE (ms or a note value of GLOBAL's tempo), MOVE and FEEL. ARP (V0.75) is a step arpeggiator: thirteen MODEs
 *     over the chord (or the keys held) across 1-4 OCTAVES, and sixteen STEPS (LENGTH of them play), each PLAY, TIE
 *     (the note before held on through it) or REST, with its LEVEL, OCTAVE, RATCHET (1-4 hits), CHANCE and SLIDE (on
 *     into the next note: legato). GROOVE: GATE and SWING (ARP's and PATTERN's), LATCH (what the keys held keeps
 *     playing until the next press after all were let go; every style but HARP, and with PERFORM off);
 *   - the transport: PLAY or MIDI's start runs it (sy_chord_run); ARP and PATTERN then step on its beat, their
 *     pattern from its bar's start, a key's first note at the next step. CLOCK IN MIDI (GLOBAL): MIDI's clock is the
 *     beat (sy_chord_clock: 24 a quarter note; the platform sets the tempo from it);
 *   - BASS: the chord's root one or two octaves under it, on the same sound;
 *   - MIDI: the chords (their channel) and the bass (theirs) out over USB; MIDI notes in as roots, if asked.
 * The panel's keys (the main loop or TIMER5) and MIDI (its interrupt) send key events into the audio interrupt's
 * rings (sy_app.c); everything below runs there, a block at a time before the engine's, so only the interrupt
 * touches the voices. The settings (chd) are the UI's, a byte each, read here as they stand (the arpeggio's live,
 * from step to step); the chord buttons write the type and extensions back. chs.seq tells the UI the chord changed,
 * chs.at the step that played. Needs the engine (sy_note_on / off, sy_tempo, sy_note_q, NOTE_NAMES) and SY_MIDI_OUT
 * (the platform's USB MIDI out; none: nothing sent). */

#ifndef SY_MIDI_OUT
#define SY_MIDI_OUT(pkt) ((void)(pkt))
#endif

/* ------------------------------------------------------------------------------------------ settings --- */
#define CH_NBTN 8u                               /* FREE's chord buttons: the keys F3 .. C4 (0 .. 7) */
#define CH_MAXN 8u                               /* a chord's notes at most, the bass apart */
#define CH_NVOIC 11u                             /* VOICING: AUTO, then -3 .. +6 */
#define CH_NRATE 19u                             /* RATE: 7 times in ms, then 12 note values */
#define AR_NSTEP 16u                             /* ARP's steps */
enum { CH_OFF, CH_KEY, CH_FREE, CH_NMODE };
enum { PF_STRUM, PF_STRUM2, PF_ARP, PF_PATTERN, PF_HARP, PF_SLOP, PF_N };
enum { AR_UP, AR_DOWN, AR_UPDN, AR_DNUP, AR_UPDN2, AR_PLAYED, AR_RANDOM, AR_WALK, AR_OUTIN, AR_INOUT, AR_PINKY,
       AR_THUMB, AR_CHORD, AR_N };
enum { AS_PLAY, AS_TIE, AS_REST, AS_N };         /* a step: its note; the note before held on through it; silence */
static struct {
    uint8_t mode, type, ktype, ext, voicing;     /* CHORD: CH_*, FREE's type and KEY's (0 .. 3 each), the extensions
                                                  * (bits: 6 m7 M7 9), 0 AUTO / 1 .. 10 */
    uint8_t perf, style;                         /* PERFORM: on (the ARP button), PF_* */
    uint8_t rate[PF_N], move[PF_N], feel[PF_N];  /* .. each style's own */
    uint8_t bass, boct, blvl;                    /* BASS: on, octaves under (1, 2), level (% of the chord's) */
    uint8_t min, mout, och, bch;                 /* MIDI: roots from MIDI in, out over USB, the chords' and the bass's
                                                  * channels (0 .. 15) */
    uint8_t gate, swing, latch, len, clock;      /* GROOVE: ARP's and PATTERN's note (% of a step, 5 .. 200), SWING (50
                                                  * straight .. 75: the odd steps half a step late), LATCH, ARP's
                                                  * LENGTH (1 .. 16); CLOCK IN (GLOBAL's: 0 the panel's tempo, 1 MIDI's) */
    uint8_t st[AR_NSTEP], lvl[AR_NSTEP], oct[AR_NSTEP];   /* ARP's steps: AS_*, LEVEL (%), OCTAVE (0 .. 4: -2 .. +2) */
    uint8_t rat[AR_NSTEP], chn[AR_NSTEP], sld[AR_NSTEP];  /* .. RATCHET (1 .. 4), CHANCE (%), SLIDE */
    uint8_t root, vel;                           /* SCALE's root (0 C .. 11 B), the keys' velocity */
    uint16_t mask;                               /* SCALE's scale: a bit a semitone from its root */
    volatile uint8_t gen;                        /* bumped by the UI when it changed what makes the chord (the mode,
                                                  * its type, voicing, PERFORM, its style, BASS, MIDI) */
} chd;
static const char *const CH_MODE_NAME[CH_NMODE] = {"OFF", "KEY", "FREE"};
static const char *const CH_TYPE_NAME[2][4] = {{"MAJ", "MIN", "SUS", "DIM"}, {"IN KEY", "SUS2", "SUS4", "POWER"}};
static const char *const CH_EXT_NAME[4] = {"6", "m7", "M7", "9"};
static const char *const PF_NAME[PF_N] = {"STRUM", "STRUM 2 OCT", "ARP", "PATTERN", "HARP", "SLOP"};
static const char *const AR_NAME[AR_N] = {"UP", "DOWN", "UP-DOWN", "DOWN-UP", "UP & DOWN", "AS PLAYED", "RANDOM",
                                          "WALK", "OUTSIDE IN", "INSIDE OUT", "PINKY", "THUMB", "CHORD"};
static const char *const AS_NAME[AS_N] = {"PLAY", "TIE", "REST"};
static const char *const ST_NAME[3] = {"UP", "DOWN", "ALTERNATE"};
/* RATE: a gap (STRUM, SLOP: between notes) or a step (ARP, PATTERN): milliseconds, then note values (NOTE_NAMES),
 * shortest first: 1/32 1/16T 1/16 1/8T 1/16D 1/8 1/4T 1/8D 1/4 1/4D 1/2 1 BAR */
static const uint8_t CH_RATE_MS[7] = {5, 10, 20, 30, 50, 80, 120};
static const uint8_t CH_RATE_NOTE[CH_NRATE - 7u] = {16, 15, 14, 12, 13, 11, 9, 10, 8, 7, 5, 3};
/* PATTERN: sixteen rhythms of stabs, a step a bit (the first the lowest), sixteen steps */
static const uint16_t CH_PAT[16] = {0x1111, 0x5555, 0xFFFF, 0x4444, 0x4949, 0x1449, 0x94A5, 0x5451,
                                    0xDDDD, 0x9999, 0x3333, 0x1041, 0x2929, 0xD6D6, 0x34B1, 0x9595};

/* a style's MOVE: its count of choices; FEEL: its range and step */
static uint32_t ch_moves(uint32_t s) { return s == PF_ARP ? AR_N : s == PF_PATTERN ? 16u : s == PF_HARP ? 1u : 3u; }
static void ch_feel_range(uint32_t s, int32_t *lo, int32_t *hi, int32_t *step)
{
    *lo = 0, *hi = 100, *step = 5;               /* STRUM's taper, SLOP's looseness, PATTERN's accent (%) */
    if (s == PF_ARP)
        *lo = 1, *hi = 4, *step = 1;             /* octaves */
    else if (s == PF_HARP)
        *lo = *hi = 0;
}
static const char *ch_feel_name(uint32_t s)
{
    return s == PF_ARP ? "OCTAVES" : s == PF_PATTERN ? "ACCENT" : s == PF_HARP ? "FEEL" : s == PF_SLOP ? "LOOSE" : "TAPER";
}
static const char *ch_move_name(uint32_t s) { return s == PF_ARP ? "MODE" : s == PF_PATTERN ? "PATTERN" : "DIRECTION"; }
static int ch_stepped(uint32_t s) { return s == PF_ARP || s == PF_PATTERN; }   /* (a step at RATE: GROOVE's) */

static void ch_steps_clear(void)                 /* ARP's steps: each a note, as played, in its octave, once, always */
{
    uint32_t i;
    for (i = 0; i < AR_NSTEP; i++) {
        chd.st[i] = AS_PLAY;
        chd.lvl[i] = 100;
        chd.oct[i] = 2;
        chd.rat[i] = 1;
        chd.chn[i] = 100;
        chd.sld[i] = 0;
    }
}
static void ch_defaults(void)
{
    static const uint8_t RATE[PF_N] = {3, 2, 9, 9, 0, 4}, FEEL[PF_N] = {0, 0, 1, 20, 0, 50};
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
    chd.gate = 50;                               /* half a step, straight, no latch, all sixteen steps, the panel's */
    chd.swing = 50;                              /* tempo */
    chd.latch = 0;
    chd.len = AR_NSTEP;
    chd.clock = 0;
    ch_steps_clear();
    chd.vel = 100;
    chd.mask = 0xFFF;
    chd.gen++;
}
/* the keys go through here (else straight to the voices, as before V0.7) */
static int sy_chord_active(void) { return chd.mode != CH_OFF || chd.perf || chd.bass || chd.latch; }

/* -------------------------------------------------------------------------------- the audio side --- */
typedef struct { uint8_t id, note, vel, down; } ch_held_t;
#define CH_NAN 16u                               /* the stepped styles' notes sounding at most */
static struct {
    ch_held_t held[CH_MAXN];                     /* roots held, in the order pressed (the last one plays); id: a key
                                                  * 0 .. 26, a MIDI note 128 + it; down: held now (LATCH keeps the
                                                  * ones let go) */
    uint8_t nheld;
    uint8_t btn, gest_t, gest_e;                 /* FREE: the buttons down now; this gesture's triad (0xFF none), its
                                                  * extensions */
    uint8_t n, note[CH_MAXN], ord[CH_MAXN];      /* the chord as voiced, its notes ascending; their built order */
    uint8_t root, bass, vel, built;              /* its root (root position), the bass note (0xFF none), the velocity;
                                                  * a chord was built (HARP's) */
    uint8_t on[16][2], non;                      /* notes this side has on: the note, its part (0 the chord, 1 the bass) */
    uint8_t q[16][3], nq;                        /* note-ons waiting for a block's starts (SY_STARTS_BLK): note, vel, part */
    uint8_t harp[27];                            /* HARP: the note each key plays (0xFF none) */
    uint8_t gen, mode, perf, style, latch;       /* chd's as last seen */
    uint8_t pf, pf_dir;                          /* PERFORM: under way; ALTERNATE's way */
    uint16_t pf_i;                               /* STRUM's, SLOP's: the notes sounded */
    uint32_t t, pf_next;                         /* samples since power-on (a block a step); STRUM's next note */
    uint32_t seed;
    /* ARP and PATTERN: the steps */
    float ph;                                    /* the place in steps (free: since the start; on the beat: the
                                                  * transport's) */
    uint32_t k;                                  /* the next step (its count: the odd ones swing) */
    uint8_t s;                                   /* .. its place in the pattern (ARP: of LENGTH; PATTERN: of 16) */
    uint8_t walk;                                /* WALK's place */
    uint32_t ni;                                 /* ARP: the next note of MODE's order (PLAY steps count) */
    uint8_t h_left, h_n, h_vel, h_note[CH_MAXN]; /* the step under way: its hits still to come (RATCHET), its notes */
    uint32_t h_next, h_gap, h_len, h_last;       /* .. the next hit's time, between hits, a hit's length, the last's */
    uint8_t an_note[CH_NAN], nan;                /* the notes the steps have sounding, */
    uint32_t an_off[CH_NAN];                     /* .. when each ends */
    float qb;                                    /* quarter notes counted (the beats) */
    /* the transport */
    uint8_t run, clk_seen;                       /* running (PLAY, MIDI's start); a MIDI clock came */
    int32_t pulses;                              /* MIDI's clocks since its start (-1: the next is the downbeat) */
    uint32_t clk_t;                              /* the last clock's time */
    volatile float tq;                           /* quarter notes since the start (the UI's beat too) */
    volatile uint8_t at;                         /* the step that played last + 1 (0 none: the UI's) */
    volatile uint16_t atseq, beat;               /* steps played, beats passed (the UI's redraw, the ARP LED) */
    volatile uint16_t seq;                       /* the chord changed: the UI's redraw */
} chs = {.bass = 0xFF, .seed = 0x9E3779B9u};

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
    chs.nan = 0;
    chs.h_left = 0;
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
static float ch_rate_q(void)                    /* .. in quarter notes (0: a time in ms, off the beat) */
{
    uint32_t r = chd.rate[chd.style % PF_N] % CH_NRATE;
    return r < 7u ? 0.0f : sy_note_q(CH_RATE_NOTE[r - 7u]);
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
static uint8_t ch_fold(int32_t x)                /* a note into 0 .. 127 by octaves */
{
    while (x > 127)
        x -= 12;
    while (x < 0)
        x += 12;
    return (uint8_t)x;
}
/* ARP's notes for its i-th note of MODE's order over the chord nt (ascending; od as built or played), n notes, across
 * OCTAVES: into out, their count (CHORD: the whole chord, an octave up each time through OCTAVES; else one). walk,
 * seed: WALK's place and RANDOM's state (the UI draws with its own) */
static uint32_t ar_notes(uint32_t i, const uint8_t *nt, const uint8_t *od, uint32_t n, uint8_t *out, uint8_t *walk,
                         uint32_t *seed)
{
    uint32_t oct = (uint32_t)sy_clampi(chd.feel[PF_ARP], 1, 4), L = n * oct, m = chd.move[PF_ARP] % AR_N, cyc, j, k;
    const uint8_t *src = m == AR_PLAYED ? od : nt;
    if (!n)
        return 0;
    if (m == AR_CHORD) {
        for (j = 0, k = i % oct; j < n; j++)
            out[j] = ch_fold(nt[j] + 12 * (int32_t)k);
        return n;
    }
    cyc = L > 1u ? 2u * L - 2u : 1u;
    k = i % cyc;
    switch (m) {
    case AR_DOWN: j = L - 1u - i % L; break;
    case AR_UPDN: j = k < L ? k : cyc - k; break;
    case AR_DNUP: j = k < L ? L - 1u - k : k - (L - 1u); break;
    case AR_UPDN2: k = i % (2u * L), j = k < L ? k : 2u * L - 1u - k; break;   /* (the ends twice) */
    case AR_RANDOM: j = sy_rand(seed) % L; break;
    case AR_WALK:                                /* (a step up or down at random, off the ends) */
        j = *walk;
        if (!i)
            j = 0;
        else if (j >= L)
            j = L - 1u;
        else if (L > 1u)
            j = !j ? 1u : j == L - 1u ? j - 1u : (sy_rand(seed) >> 7) & 1u ? j + 1u : j - 1u;
        *walk = (uint8_t)j;
        break;
    case AR_OUTIN:                               /* (the outer notes first, inward: C G E) */
    case AR_INOUT:                               /* (the inner first, outward: E G C) */
        k = i % L;
        if (m == AR_INOUT)
            k = L - 1u - k;
        j = (k & 1u) ? L - 1u - k / 2u : k / 2u;
        break;
    case AR_PINKY: j = L < 2u ? 0u : (k & 1u) ? L - 1u : k / 2u; break;      /* (the top between the others) */
    case AR_THUMB: j = L < 2u ? 0u : (k & 1u) ? 1u + k / 2u : 0u; break;     /* (the bottom between) */
    default: j = i % L; break;                   /* (UP, AS PLAYED) */
    }
    out[0] = ch_fold(src[j % n] + 12 * (int32_t)(j / n));
    return 1;
}
/* SWING: step k's place after its beat, in steps (the odd ones late: 75, half a step) */
static float ch_swing(uint32_t k) { return (k & 1u) ? (float)((int32_t)sy_clampi(chd.swing, 50, 75) - 50) / 50.0f : 0.0f; }
/* samples from step k's start to step k + m's (SWING moves them) */
static uint32_t ch_span(uint32_t k, uint32_t m, uint32_t smp)
{
    float d = (float)m + ch_swing(k + m) - ch_swing(k);
    return d > 0.0f ? (uint32_t)(d * (float)smp) : 0u;
}
/* a stepped style's note on until off (a note the steps have on already: its voice again, its old end dropped) */
static void ch_an_on(uint32_t note, uint32_t vel, uint32_t off)
{
    uint32_t i;
    for (i = 0; i < chs.nan;)
        if (chs.an_note[i] == note) {
            chs.an_note[i] = chs.an_note[chs.nan - 1u];
            chs.an_off[i] = chs.an_off[chs.nan - 1u];
            chs.nan--;
        } else {
            i++;
        }
    if (chs.nan == CH_NAN) {                     /* (full: the first one ends) */
        ch_off(chs.an_note[0]);
        chs.an_note[0] = chs.an_note[chs.nan - 1u];
        chs.an_off[0] = chs.an_off[chs.nan - 1u];
        chs.nan--;
    }
    ch_on(note, vel, 0u);
    chs.an_note[chs.nan] = (uint8_t)note;
    chs.an_off[chs.nan] = off;
    chs.nan++;
}
static void ch_an_tick(void)                     /* the steps' notes due to end */
{
    uint32_t i;
    for (i = 0; i < chs.nan;)
        if ((int32_t)(chs.t - chs.an_off[i]) >= 0) {
            ch_off(chs.an_note[i]);
            chs.an_note[i] = chs.an_note[chs.nan - 1u];
            chs.an_off[i] = chs.an_off[chs.nan - 1u];
            chs.nan--;
        } else {
            i++;
        }
}
static void ch_hits(void)                        /* the step under way: its hits due (RATCHET's after the first) */
{
    while (chs.h_left && (int32_t)(chs.t - chs.h_next) >= 0) {
        uint32_t len = chs.h_left == 1u ? chs.h_last : chs.h_len, j;
        for (j = 0; j < chs.h_n; j++)
            ch_an_on(chs.h_note[j], chs.h_vel, chs.t + (len < SY_BLK ? SY_BLK : len));
        chs.h_left--;
        chs.h_next += chs.h_gap;
    }
}
/* step k of ARP or PATTERN (smp: a step's samples) */
static void ch_step(uint32_t k, uint32_t smp)
{
    uint32_t st = chd.style % PF_N, s = chs.s, len = st == PF_ARP ? (uint32_t)sy_clampi(chd.len, 1, AR_NSTEP) : 16u, j;
    uint32_t g = (uint32_t)sy_clampi(chd.gate, 5, 200);
    chs.s = (uint8_t)((s + 1u) % len);
    chs.at = (uint8_t)(s + 1u);
    chs.atseq++;
    if (st == PF_PATTERN) {                      /* PATTERN: a stab where its step's bit is (off the beat ACCENT softer) */
        if (!((CH_PAT[chd.move[PF_PATTERN] % 16u] >> s) & 1u) || !chs.n)
            return;
        chs.h_n = chs.n;
        memcpy(chs.h_note, chs.note, chs.n);
        chs.h_vel = (uint8_t)(chs.vel * (s % 4u ? 100u - (uint32_t)sy_clampi(chd.feel[PF_PATTERN], 0, 100) : 100u) / 100u);
        chs.h_left = 1;
        chs.h_gap = smp;
        chs.h_len = chs.h_last = smp * g / 100u;
    } else {                                     /* ARP: the step's note (TIE, REST: none) */
        uint32_t r = (uint32_t)sy_clampi(chd.rat[s], 1, 4), ties = 0, i;
        int32_t o = 12 * ((int32_t)sy_clampi(chd.oct[s], 0, 4) - 2);
        if (chd.st[s] != AS_PLAY)
            return;
        i = chs.ni++;
        if (sy_rand(&chs.seed) % 100u >= (uint32_t)chd.chn[s])   /* (CHANCE: a gap, its note passed over) */
            return;
        chs.h_n = (uint8_t)ar_notes(i, chs.note, chs.ord, chs.n, chs.h_note, &chs.walk, &chs.seed);
        for (j = 0; j < chs.h_n; j++)
            chs.h_note[j] = ch_fold(chs.h_note[j] + o);
        for (j = 1; j < len && chd.st[(s + j) % len] == AS_TIE; j++)
            ties++;                              /* (TIE after it: the note held on to the last one's gate) */
        chs.h_vel = (uint8_t)sy_clampi((int32_t)(chs.vel * (uint32_t)chd.lvl[s] / 100u), 1, 127);
        chs.h_left = (uint8_t)r;
        chs.h_gap = smp / r;
        chs.h_len = chs.h_gap * g / 100u;
        if (chd.sld[s] || ties) {                /* SLIDE: on into the next note (two blocks over it); TIE: to the */
            int32_t e = chd.sld[s] ? (int32_t)ch_span(k, 1u + ties, smp) + 2 * (int32_t)SY_BLK   /* tied one's gate */
                                   : (int32_t)ch_span(k, ties, smp) + (int32_t)(smp * g / 100u);
            e -= (int32_t)((r - 1u) * chs.h_gap);   /* (from the last hit) */
            chs.h_last = e > (int32_t)chs.h_len ? (uint32_t)e : chs.h_len;
        } else {
            chs.h_last = chs.h_len;
        }
    }
    chs.h_next = chs.t;
    ch_hits();
}
/* ARP and PATTERN start: from the next step on the beat (the transport running, a note value: the pattern's place as
 * the bar has it; a key just after a step's start plays it at once), else at once */
static void ch_step_start(void)
{
    float q = ch_rate_q();
    uint32_t wrap = chd.style == PF_ARP ? (uint32_t)sy_clampi(chd.len, 1, AR_NSTEP) : 16u;
    chs.ni = 0;
    chs.walk = 0;
    chs.h_left = 0;
    if (chs.run && q > 0.0f) {
        float ph = chs.pulses < 0 ? 0.0f : chs.tq / q;
        uint32_t k = (uint32_t)ph;
        chs.k = ph - (float)k - ch_swing(k) < 0.1f ? k : k + 1u;   /* (still to come, or just gone: now) */
        chs.ph = ph;
    } else {                                     /* (free; the keys themselves (OFF): gathered 15 ms first, so a */
        chs.k = 0;                               /* chord's notes are all in the first step, whichever came first) */
        chs.ph = chd.mode == CH_OFF && !(chd.style == PF_ARP && chd.move[PF_ARP] % AR_N == AR_PLAYED)
                     ? -sy_minf(0.9f, 0.015f * SY_FS / (float)ch_rate_smp()) : 0.0f;
    }
    chs.s = (uint8_t)(chs.k % wrap);
}
/* a block of ARP or PATTERN: the steps due (on the beat: the transport's place; free: their own) */
static void ch_steps(void)
{
    float q = ch_rate_q(), qn;
    uint32_t smp = ch_rate_smp();
    int beat = chs.run && q > 0.0f;
    if (beat && chs.pulses < 0)
        return;                                  /* (MIDI's start: waiting for its first clock) */
    if (beat)
        chs.ph = chs.tq / q;
    if (chs.ph >= 0.0f && (chs.ph + 2.0f < (float)chs.k || chs.ph > (float)chs.k + 2.0f)) {   /* (the rate, the */
        uint32_t wrap = chd.style == PF_ARP ? (uint32_t)sy_clampi(chd.len, 1, AR_NSTEP) : 16u, k0 = (uint32_t)chs.ph;
        chs.k = chs.ph < (float)k0 + ch_swing(k0) ? k0 : k0 + 1u;   /* transport moved: from the next step to come) */
        if (beat)
            chs.s = (uint8_t)(chs.k % wrap);
    }
    while (chs.ph >= (float)chs.k + ch_swing(chs.k)) {
        ch_step(chs.k, smp);
        chs.k++;
    }
    qn = beat ? chs.tq : q > 0.0f ? chs.ph * q : chs.ph * 0.25f;   /* (the beats: a quarter; ms: four steps) */
    if ((int32_t)qn != (int32_t)chs.qb)
        chs.beat++;
    chs.qb = qn;
    if (!beat) {
        chs.ph += (float)SY_BLK / (float)smp;
        if (chs.k >= 4096u) {                    /* (an even count off both: the swing's places kept) */
            chs.k -= 4096u;
            chs.ph -= 4096.0f;
        }
    }
}
static void ch_perf_start(void)
{
    chs.pf = 1;
    chs.pf_i = 0;
    chs.pf_next = chs.t;
    chs.pf_dir = (uint8_t)!chs.pf_dir;
    chs.at = 0;
    if (ch_stepped(chd.style % PF_N))
        ch_step_start();
}
/* a block of PERFORM: what is due now */
static void ch_perform(void)
{
    uint32_t st = chd.style % PF_N, rate;
    int32_t feel = chd.feel[st];
    if (ch_stepped(st)) {
        ch_an_tick();
        if (chs.n)
            ch_steps();
        ch_hits();
        return;
    }
    if (!chs.n || st == PF_HARP)
        return;
    {                                            /* STRUM, STRUM 2 OCT, SLOP */
        uint8_t l[CH_MAXN];
        uint32_t n = ch_strum_list(l);
        rate = ch_rate_smp();
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
    }
}

/* ------------------------------------------------------------------------------------------- playing --- */
/* the chord for what is held now: KEY and FREE, the last root's chord, voiced; OFF, the notes held. restart: a new
 * chord (everything it plays starts again, PERFORM from its start); else (OFF) only what changed (ARP and PATTERN:
 * their notes end as their steps say) */
static void ch_play(int restart)
{
    uint8_t nt[CH_MAXN], od[CH_MAXN], root = 0;
    uint32_t n = 0, i, bass = 0xFFu;
    int steps = chd.perf && ch_stepped(chd.style % PF_N);
    if (!chs.nheld) {
        ch_all_off();
        chs.pf = 0;
        chs.at = 0;
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
            if ((chs.on[i][1] == 0u && k == n && !steps) || (chs.on[i][1] == 1u && x != bass))
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
/* LATCH let go: the roots no longer held go (what is left plays on) */
static void ch_unlatch(void)
{
    uint32_t i, k = 0, top = chs.nheld ? chs.held[chs.nheld - 1u].id : 0xFFu;
    for (i = 0; i < chs.nheld; i++)
        if (chs.held[i].down)
            chs.held[k++] = chs.held[i];
    if (k == chs.nheld)
        return;
    chs.nheld = (uint8_t)k;
    ch_play(chd.mode != CH_OFF && k && chs.held[k - 1u].id != top);
}

/* ------------------------------------------------------------------------------------- the interface --- */
/* a key (0 .. 26) or a MIDI note (128 + it) down or up: note, the note it plays as SCALE fits it (0xFF none) */
static void sy_chord_key(uint32_t id, uint32_t note, uint32_t vel, int down)
{
    uint32_t i, any = 0;
    if (chd.mode == CH_FREE && id < CH_NBTN) {
        ch_button(id, down);
        return;
    }
    if (chd.perf && chd.style == PF_HARP && id < 27u) {
        ch_harp(id, down);
        return;
    }
    for (i = 0; i < chs.nheld; i++)
        any |= chs.held[i].down;
    if (chd.latch && down && !any)
        chs.nheld = 0;                           /* (LATCH: a press after all were let go, what is held anew) */
    for (i = 0; i < chs.nheld && chs.held[i].id != id; i++)
        ;
    if (chd.latch && !down) {                    /* (LATCH: let go, it stays) */
        if (i < chs.nheld)
            chs.held[i].down = 0;
        return;
    }
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
    chs.held[chs.nheld].down = 1;
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
    chs.at = 0;
    for (i = 0; i < 27u; i++)
        chs.harp[i] = 0xFF;
    chs.seq++;
}
/* the transport: a 1 start (from the top: MIDI's clock following, at its next clock), 2 continue, 0 stop. ARP and
 * PATTERN under way go on: from the start, on its beat from the top; stopped, on their own from where they are */
static void sy_chord_run(uint32_t a)
{
    if (a == 1u) {
        chs.tq = 0.0f;
        chs.pulses = chd.clock && chs.clk_seen && chs.t - chs.clk_t < (uint32_t)(SY_FS * 0.5f) ? -1 : 0;
        chs.run = 1;
        if (chs.pf && ch_stepped(chd.style % PF_N)) {
            chs.k = 0;
            chs.s = 0;
            chs.ph = 0.0f;
        }
    } else if (a == 2u) {
        if (chs.pulses < 0)
            chs.pulses = 0;
        chs.run = 1;
    } else {
        chs.run = 0;
    }
}
/* MIDI's clock (CLOCK IN MIDI, 24 a quarter note): the transport's place on it */
static void sy_chord_clock(void)
{
    if (!(chs.clk_seen && chs.t - chs.clk_t < (uint32_t)(SY_FS * 0.5f)) && chs.run && chs.pulses >= 0)
        chs.pulses = (int32_t)(chs.tq * 24.0f); /* (the first clock in a while: counted from where the transport is) */
    chs.clk_t = chs.t;
    chs.clk_seen = 1;
    if (!chs.run)
        return;
    chs.pulses++;
    if (chs.pulses >= 0 && chs.tq < (float)chs.pulses / 24.0f)
        chs.tq = (float)chs.pulses / 24.0f;
}
/* a block of the transport: its place on (between MIDI's clocks no further than the next one) */
static void ch_transport(void)
{
    if (!chs.run || chs.pulses < 0)
        return;
    chs.tq += (float)SY_BLK * sy_tempo / (60.0f * SY_FS);
    if (chd.clock && chs.clk_seen && chs.t - chs.clk_t < (uint32_t)(SY_FS * 0.5f)) {
        float cap = ((float)chs.pulses + 1.0f) / 24.0f - 0.0001f;
        if (chs.tq > cap)
            chs.tq = cap;
    }
    if (chs.tq >= 3840.0f) {                     /* (960 bars of 4/4: back by them, the beat's places kept) */
        chs.tq -= 3840.0f;
        chs.pulses = chs.pulses >= 3840 * 24 ? chs.pulses - 3840 * 24 : 0;
    }
}
/* a block: the settings' changes, PERFORM's steps, the note-ons queued (starts: this block's so far, max of them) */
static void sy_chord_block(uint32_t *starts, uint32_t max)
{
    if (chd.gen != chs.gen) {                    /* (the mode, HARP: everything again, the keys too (press them */
        int all = chd.mode != chs.mode || (chd.perf && chd.style == PF_HARP && (chd.perf != chs.perf ||   /* again); */
                                                                                chd.style != chs.style));
        int perf = chd.perf != chs.perf || chd.style != chs.style;   /* PERFORM on, off, its style: what is held */
        chs.gen = chd.gen;                       /* plays on the new way (V0.75: a chord held, then ARP); else the */
        chs.mode = chd.mode;                     /* chord voiced again) */
        chs.perf = chd.perf;
        chs.style = chd.style;
        if (all) {
            sy_chord_reset();
        } else if (perf) {
            uint32_t i;
            ch_all_off();
            chs.pf = 0;
            chs.at = 0;
            for (i = 0; i < 27u; i++)
                chs.harp[i] = 0xFF;
            if (chs.nheld)
                ch_play(1);
            chs.seq++;
        } else if (chs.nheld) {
            ch_play(1);
        }
    }
    if (chd.latch != chs.latch) {                /* (LATCH off: what was let go goes) */
        chs.latch = chd.latch;
        if (!chd.latch)
            ch_unlatch();
    }
    if (chs.pf)
        ch_perform();
    ch_starts(starts, max);
    ch_transport();
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
