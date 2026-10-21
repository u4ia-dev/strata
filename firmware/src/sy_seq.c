/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* STRATA's sequencer (V0.8; docs/CONTROLS.md "The sequencer"): sixteen patterns, each up to 64 steps of up to four
 * notes, played on the transport (PLAY, or MIDI's start and clock with CLOCK IN MIDI):
 *   - a step: its notes (each with its velocity), LENGTH (quarter steps: 1/4 .. 64 steps; past the next step: held
 *     on), CHANCE (it plays, or not), NUDGE (half a step early .. half a step late); two MOTION lanes a pattern, each
 *     a parameter of the sound and a value at the steps that hold one (the parameter there; elsewhere the knob's own);
 *   - a pattern: LENGTH (1 .. 64 steps), RATE (a note value of GLOBAL's tempo), SWING (the odd steps late),
 *     DIRECTION (FORWARD, BACKWARD, PING-PONG, RANDOM), TRANSPOSE (semitones);
 *   - CHAIN: up to sixteen slots, each a pattern and its repeats, played in turn and round again (a slot without a
 *     pattern ends it); a pattern picked while one plays waits for its end (then the chain goes on);
 *   - recording (REC arms it): while the transport runs, the notes played (the panel's keys as SCALE fits them, MIDI
 *     in) go into the step nearest (its NUDGE how far off, unless QUANTIZE), as long as they were held; OVERDUB adds
 *     them, REPLACE clears each step as the playhead passes it first; a knob turned (a parameter that can be
 *     modulated) writes its lane at each step it passes while it turns. Stopped, with SEQ's steps shown (NOTES, FEEL,
 *     MOTION), the keys fill the step picked (a chord: the keys held together) and move it on.
 * The notes play as keys do (sy_chord_key, ids SQ_ID0 + a slot): SCALE's chords, the ARP, STRUM, BASS and USB MIDI
 * out act on them. The motion lanes hold their parameters through sy_param.c's override (sp_ovr_*), the patch itself
 * untouched: the knob, a save and undo keep the sound's own values. Everything here runs in the audio interrupt, a
 * block at a time before the chords' (sy_seq_block), but the UI's edits (the main loop: sq_pat and sqd, a byte at a
 * time) and the flash's (sy_app.c, stopped only). Needs sy_chord.c (chd, chs, sy_chord_key, CH_RATE_NOTE), the
 * engine (sy_patch, SP, sp_ovr_*, sy_note_q, sy_tempo) and sy_store.c (ss_buf, ss_crc32). */

/* ---------------------------------------------------------------------------------------- patterns --- */
#define SQ_NPAT 16u                              /* patterns */
#define SQ_NSTEP 64u                             /* .. their steps at most */
#define SQ_NNOTE 4u                              /* a step's notes at most */
#define SQ_NLANE 2u                              /* MOTION's lanes a pattern */
#define SQ_NCHAIN 16u                            /* CHAIN's slots */
#define SQ_NONE 0xFFu                            /* no note, no lane value, no pattern */
#define SQ_NOID 0xFFFFu                          /* a lane without its parameter */
#define SQ_NRATE (CH_NRATE - 7u)                 /* RATE: the ARP's note values (CH_RATE_NOTE: 1/32 .. 1 BAR) */
#define SQ_RATE0 2u                              /* .. 1/16 */
#define SQ_LEN0 2u                               /* a step's LENGTH as it starts: half a step (quarter steps) */
enum { SQ_FWD, SQ_BACK, SQ_PING, SQ_RANDOM, SQ_NDIR };
enum { SQ_OVERDUB, SQ_REPLACE, SQ_NRMODE };
typedef struct {
    uint8_t note[SQ_NSTEP][SQ_NNOTE];            /* a step's notes (MIDI; SQ_NONE none), as recorded */
    uint8_t vel[SQ_NSTEP][SQ_NNOTE];             /* .. their velocities (1 .. 127) */
    uint8_t len[SQ_NSTEP];                       /* LENGTH: quarter steps (1 .. 255) */
    uint8_t chance[SQ_NSTEP];                    /* CHANCE: % (0 .. 100) */
    int8_t nudge[SQ_NSTEP];                      /* NUDGE: % of a step (-50 .. +50) */
    uint8_t lane[SQ_NLANE][SQ_NSTEP];            /* MOTION: a step's value, 0 .. 254 of its parameter's range */
    uint16_t lane_id[SQ_NLANE];                  /* .. the parameters (SQ_NOID none) */
    uint8_t length, rate, swing, dir;            /* LENGTH (1 .. 64), RATE (SQ_NRATE's), SWING (50 .. 75), SQ_* */
    int8_t transpose;                            /* TRANSPOSE: semitones (-24 .. +24) */
    uint8_t rsv[3];
} sq_pat_t;
_Static_assert(sizeof(sq_pat_t) == 844u, "a pattern's bytes, as the flash keeps them");
static sq_pat_t sq_pat[SQ_NPAT] SY_POOL;         /* (13.2 KB in the pool: set at power-on, sq_defaults) */
static sq_pat_t sq_clip SY_POOL;                 /* PATTERNS' COPY (and what CLEAR took: PASTE puts it back) */
static uint8_t sq_clip_ok;
static const char *const SQ_DIR_NAME[SQ_NDIR] = {"FORWARD", "BACKWARD", "PING-PONG", "RANDOM"};
static const char *const SQ_RMODE_NAME[SQ_NRMODE] = {"OVERDUB", "REPLACE"};

/* the settings: the UI's (a byte each), read here as they stand */
static struct {
    volatile uint8_t cur;                        /* the pattern shown and edited; while the transport runs, the one
                                                  * playing (this side moves it on: CHAIN, a pattern picked) */
    volatile uint8_t next;                       /* a pattern picked while one plays: from that one's end (SQ_NONE) */
    uint8_t chain;                               /* CHAIN on */
    uint8_t cpat[SQ_NCHAIN], crep[SQ_NCHAIN];    /* .. its slots: a pattern (SQ_NONE: the chain ends before it), its
                                                  * repeats (1 .. 16) */
    uint8_t recmode, quant;                      /* RECORD: OVERDUB, REPLACE; QUANTIZE: the steps' places alone (else
                                                  * NUDGE keeps where the notes came) */
    uint8_t arm;                                 /* REC: armed */
    uint8_t steprec;                             /* the UI's: the keys fill the step picked (stopped, armed, SEQ's steps
                                                  * shown) */
    volatile uint8_t cursor;                     /* the step picked (0 .. 63): step recording moves it on */
    uint8_t lane;                                /* MOTION: the lane picked */
    volatile uint8_t ltouch;                     /* the lanes whose knob turns now (bit a lane): the knob heard, not the
                                                  * lane (recording: the lane written at each step meanwhile) */
} sqd = {.next = SQ_NONE};

/* --------------------------------------------------------------------------------- the audio side --- */
#define SQ_ID0 64u                               /* sy_chord_key's ids for the sequencer's notes: SQ_ID0 + a slot */
#define SQ_NON 16u                               /* .. its notes sounding at most */
#define SQ_NREC 8u                               /* notes held while recording, their lengths still to come */
static struct {
    uint8_t run;                                 /* a pattern under way (the transport running) */
    uint8_t pat, spat;                           /* the pattern the next step is of; the last step's */
    uint8_t s, ns;                               /* the last step played (in spat), the next one (in pat) */
    uint8_t pass_n;                              /* steps played of this pass (PING-PONG: there and back) */
    uint8_t cslot, crep;                         /* CHAIN: the slot playing, its passes still to go */
    uint32_t k;                                  /* the next step's count from the start (the odd ones swing) */
    float tq;                                    /* the transport's place a block before (back: from there again) */
    uint32_t seed;
    struct {
        uint8_t used, note;
        uint32_t off;                            /* .. when it ends */
    } on[SQ_NON];
    uint8_t keynote[27];                         /* the note each panel key went in as (its release: SQ_NONE none) */
    struct {
        uint8_t used, note, pat, step, slot;
        float ph;                                /* .. where it came (in steps) */
    } rec[SQ_NREC];
    uint8_t fresh[SQ_NSTEP];                     /* notes recorded this pass just ahead of the playhead (a bit each):
                                                  * not played again as it comes (the key plays them already) */
    uint32_t sbits[4];                           /* step recording: the notes held (a bit each) */
    uint8_t sstep;                               /* .. the step they go to */
    volatile uint8_t at;                         /* the step played last + 1 (0 none): the UI's */
    volatile uint16_t atseq, seq;                /* steps played; a step recorded (the UI's redraws) */
} sqs = {.seed = 0x2545F491u};

/* a pattern's step in quarter notes; SWING: step k's place after its beat, in steps; step k's place */
static float sq_q(const sq_pat_t *p) { return sy_note_q(CH_RATE_NOTE[p->rate % SQ_NRATE]); }
static float sq_swing(const sq_pat_t *p, uint32_t k)
{
    return (k & 1u) ? (float)(sy_clampi(p->swing, 50, 75) - 50) / 50.0f : 0.0f;
}
static float sq_grid(const sq_pat_t *p, uint32_t k) { return (float)k + sq_swing(p, k); }
static uint32_t sq_len(const sq_pat_t *p) { return (uint32_t)sy_clampi(p->length, 1, SQ_NSTEP); }
/* a pass's steps (PING-PONG: there and back, the ends once); the i-th step of a pass */
static uint32_t sq_passlen(const sq_pat_t *p)
{
    uint32_t L = sq_len(p);
    return p->dir % SQ_NDIR == SQ_PING && L > 1u ? 2u * L - 2u : L;
}
static uint32_t sq_pos(const sq_pat_t *p, uint32_t i)
{
    uint32_t L = sq_len(p);
    switch (p->dir % SQ_NDIR) {
    case SQ_BACK: return L - 1u - i % L;
    case SQ_PING: i %= sq_passlen(p); return i < L ? i : 2u * L - 2u - i;
    case SQ_RANDOM: return sy_rand(&sqs.seed) % L;
    default: return i % L;
    }
}
/* a lane's value (0 .. 254) as its parameter's, and back */
static int16_t sq_lane_v(uint32_t id, uint32_t x)
{
    int32_t lo = SP[id].lo, hi = SP[id].hi;
    return (int16_t)(lo + ((hi - lo) * (int32_t)x + 127) / 254);
}
static uint8_t sq_lane_x(uint32_t id, int32_t v)
{
    int32_t lo = SP[id].lo, hi = SP[id].hi;
    return (uint8_t)(hi > lo ? sy_clampi(((v - lo) * 254 + (hi - lo) / 2) / (hi - lo), 0, 254) : 0);
}
static int sq_lane_ok(uint32_t id) { return id < SP_COUNT && (SP[id].flags & F_MOD) && !(SP[id].flags & F_OLD); }

/* a note of the sequencer's on until off (the same note already on from it: that one ends first, so it starts
 * again; all in use: the one ending first goes) */
static void sq_note_end(uint32_t i)
{
    if (!sqs.on[i].used)
        return;
    sqs.on[i].used = 0;
    sy_chord_key(SQ_ID0 + i, sqs.on[i].note, 0u, 0);
}
static void sq_note_on(uint32_t note, uint32_t vel, uint32_t off)
{
    uint32_t i, j = 0;
    for (i = 0; i < SQ_NON; i++)
        if (sqs.on[i].used && sqs.on[i].note == note)
            sq_note_end(i);
    for (i = 0; i < SQ_NON && sqs.on[i].used; i++)
        if ((int32_t)(sqs.on[i].off - sqs.on[j].off) < 0)
            j = i;
    if (i == SQ_NON) {
        sq_note_end(j);
        i = j;
    }
    sqs.on[i].used = 1;
    sqs.on[i].note = (uint8_t)note;
    sqs.on[i].off = off;
    sy_chord_key(SQ_ID0 + i, note, vel, 1);
}
static void sq_ends(void)                        /* the notes due to end */
{
    uint32_t i;
    for (i = 0; i < SQ_NON; i++)
        if (sqs.on[i].used && (int32_t)(chs.t - sqs.on[i].off) >= 0)
            sq_note_end(i);
}

/* the lanes at step s: a value there holds its parameter (its knob turning: the knob's, recording it there) */
static void sq_lanes(sq_pat_t *p, uint32_t s, int rec)
{
    uint32_t l;
    for (l = 0; l < SQ_NLANE; l++) {
        uint32_t id = p->lane_id[l], x = p->lane[l][s];
        if (!sq_lane_ok(id)) {
            sp_ovr_id[l] = SQ_NOID;
            continue;
        }
        if ((sqd.ltouch >> l) & 1u) {
            if (rec)
                p->lane[l][s] = sq_lane_x(id, sy_patch.p[id]);
            sp_ovr_id[l] = SQ_NOID;
        } else if (x != SQ_NONE) {
            sp_ovr_v[l] = sq_lane_v(id, x);
            sp_ovr_id[l] = (uint16_t)id;
        } else {
            sp_ovr_id[l] = SQ_NOID;
        }
    }
}

/* a pass's end: a pattern picked plays next (once; the chain then goes on), else the chain's next (its repeats
 * first; round to its first slot at the first without a pattern) */
static void sq_pass_end(void)
{
    uint32_t i;
    for (i = 0; i < SQ_NSTEP; i++)
        sqs.fresh[i] = 0;
    if (sqd.next < SQ_NPAT) {
        sqs.pat = sqd.next;
        sqd.next = SQ_NONE;
        sqs.crep = 1;
    } else if (sqd.chain) {
        if (sqs.crep > 1u) {
            sqs.crep--;
        } else {
            uint32_t n = sqs.cslot + 1u;
            if (n >= SQ_NCHAIN || sqd.cpat[n] >= SQ_NPAT)
                n = 0;
            sqs.cslot = (uint8_t)n;
            if (sqd.cpat[n] < SQ_NPAT)
                sqs.pat = sqd.cpat[n];
            sqs.crep = (uint8_t)sy_clampi(sqd.crep[n], 1, 16);
        }
    }
    sqd.cur = sqs.pat;
}

/* step sqs.ns of the pattern playing (smp: a step's samples): REPLACE recording clears it first, the lanes, its notes
 * (CHANCE, TRANSPOSE; LENGTH their end), then the next step's place */
static void sq_fire(uint32_t smp)
{
    sq_pat_t *p = &sq_pat[sqs.pat % SQ_NPAT];
    uint32_t s = sqs.ns % SQ_NSTEP, i, len;
    int rec = sqd.arm != 0;
    if (rec && sqd.recmode == SQ_REPLACE)
        for (i = 0; i < SQ_NNOTE; i++)
            if (!((sqs.fresh[s] >> i) & 1u))
                p->note[s][i] = SQ_NONE;
    sq_lanes(p, s, rec);
    if (sy_rand(&sqs.seed) % 100u < (uint32_t)p->chance[s]) {
        len = smp / 4u * p->len[s];             /* (1 BAR at 20 BPM in 16/1, 255 quarter steps: under 2^32) */
        for (i = 0; i < SQ_NNOTE; i++)
            if (p->note[s][i] != SQ_NONE && !((sqs.fresh[s] >> i) & 1u))
                sq_note_on((uint32_t)sy_clampi(p->note[s][i] + p->transpose, 0, 127),
                           (uint32_t)sy_clampi(p->vel[s][i], 1, 127), chs.t + (len < SY_BLK ? SY_BLK : len));
    }
    sqs.fresh[s] = 0;
    sqs.spat = sqs.pat;
    sqs.s = (uint8_t)s;
    sqs.at = (uint8_t)(s + 1u);
    sqs.atseq++;
    if (++sqs.pass_n >= sq_passlen(p)) {
        sqs.pass_n = 0;
        sq_pass_end();
    }
    sqs.ns = (uint8_t)sq_pos(&sq_pat[sqs.pat % SQ_NPAT], sqs.pass_n);
}

/* the transport started (from its top, or back to it: CHAIN from its first slot), or went on (from where it is) */
static void sq_start(void)
{
    sq_pat_t *p;
    float ph;
    uint32_t k, i;
    sqs.crep = 1;
    if (chs.tq < 0.001f && sqd.chain && sqd.cpat[0] < SQ_NPAT) {
        sqs.cslot = 0;
        sqs.pat = sqd.cpat[0];
        sqs.crep = (uint8_t)sy_clampi(sqd.crep[0], 1, 16);
    } else {
        sqs.pat = (uint8_t)(sqd.cur % SQ_NPAT);
    }
    sqd.cur = sqs.pat;
    sqd.next = SQ_NONE;
    p = &sq_pat[sqs.pat];
    ph = chs.tq / sq_q(p);
    k = (uint32_t)ph;
    sqs.k = ph - sq_grid(p, k) < 0.1f ? k : k + 1u;   /* (a step just gone: now) */
    sqs.pass_n = (uint8_t)(sqs.k % sq_passlen(p));
    sqs.ns = (uint8_t)sq_pos(p, sqs.pass_n);
    sqs.s = sqs.ns;
    sqs.spat = sqs.pat;
    sqs.at = 0;
    sqs.run = 1;
    for (i = 0; i < SQ_NSTEP; i++)
        sqs.fresh[i] = 0;
}
/* the transport stopped: the notes end, the lanes let go, a pattern picked is the one now */
static void sq_stop(void)
{
    uint32_t i;
    for (i = 0; i < SQ_NON; i++)
        sq_note_end(i);
    for (i = 0; i < SQ_NREC; i++)
        sqs.rec[i].used = 0;
    sp_ovr_id[0] = sp_ovr_id[1] = SQ_NOID;
    sqs.run = 0;
    sqs.at = 0;
    if (sqd.next < SQ_NPAT) {
        sqd.cur = sqd.next;
        sqd.next = SQ_NONE;
    }
}

/* a block: the notes due to end; running, the steps due (each at its place, SWING's and its NUDGE's) */
static void sy_seq_block(void)
{
    sq_pat_t *p;
    float ph;
    uint32_t smp, n = 0;
    sq_ends();
    if (!sqd.steprec)
        sqs.sbits[0] = sqs.sbits[1] = sqs.sbits[2] = sqs.sbits[3] = 0;
    if (!chs.run) {
        if (sqs.run)
            sq_stop();
        return;
    }
    if (chs.pulses < 0)
        return;                                  /* (MIDI's start: waiting for its first clock) */
    if (!sqs.run || chs.tq + 0.001f < sqs.tq)
        sq_start();
    sqs.tq = chs.tq;
    p = &sq_pat[sqs.pat % SQ_NPAT];
    ph = chs.tq / sq_q(p);
    if (ph + 2.0f < (float)sqs.k || ph > (float)sqs.k + 2.0f) {   /* (RATE changed, or the transport moved: */
        uint32_t k0 = (uint32_t)ph;                               /* from the next step to come) */
        sqs.k = ph < sq_grid(p, k0) ? k0 : k0 + 1u;
    }
    smp = (uint32_t)(sq_q(p) * SY_FS * 60.0f / (sy_tempo > 1.0f ? sy_tempo : 120.0f));
    while (n++ < 4u && ph >= sq_grid(p, sqs.k) + (float)p->nudge[sqs.ns % SQ_NSTEP] * 0.01f) {
        uint32_t was = sqs.pat;
        sq_fire(smp);
        sqs.k++;
        if (sqs.pat != was)
            break;                               /* (another pattern: its RATE from the next block) */
    }
}

/* ------------------------------------------------------------------------------------- recording --- */
/* note into step s of pattern p (at its velocity; already there: its velocity now; the step full: its last one
 * replaced): its slot; empty: the step had no notes */
static uint32_t sq_put(sq_pat_t *p, uint32_t s, uint32_t note, uint32_t vel, int *empty)
{
    uint32_t i, at = SQ_NNOTE;
    *empty = 1;
    for (i = 0; i < SQ_NNOTE; i++)
        if (p->note[s][i] != SQ_NONE)
            *empty = 0;
    for (i = 0; i < SQ_NNOTE; i++)
        if (p->note[s][i] == note) {
            p->vel[s][i] = (uint8_t)sy_clampi((int32_t)vel, 1, 127);
            return i;
        }
    for (i = 0; i < SQ_NNOTE && at == SQ_NNOTE; i++)
        if (p->note[s][i] == SQ_NONE)
            at = i;
    if (at == SQ_NNOTE)
        at = SQ_NNOTE - 1u;
    p->note[s][at] = (uint8_t)note;
    p->vel[s][at] = (uint8_t)sy_clampi((int32_t)vel, 1, 127);
    return at;
}
static uint32_t sq_untransposed(const sq_pat_t *p, uint32_t note)
{
    return (uint32_t)sy_clampi((int32_t)note - p->transpose, 0, 127);
}
/* a note played while recording: into the step nearest (the one played last, or the next: then not played again as
 * it comes), its NUDGE how far off (QUANTIZE, or a step with notes already: as it was) */
static void sq_rec_on(uint32_t note, uint32_t vel)
{
    sq_pat_t *p = &sq_pat[sqs.pat % SQ_NPAT];
    float ph = chs.tq / sq_q(p), gn = sq_grid(p, sqs.k), d;
    uint32_t s, i, slot;
    int ahead = !sqs.at || !sqs.k || gn - ph < ph - sq_grid(p, sqs.k - 1u), empty;
    if (ahead) {
        s = sqs.ns % SQ_NSTEP;
        d = ph - gn;
    } else {
        p = &sq_pat[sqs.spat % SQ_NPAT];
        s = sqs.s % SQ_NSTEP;
        d = ph - sq_grid(p, sqs.k - 1u);
    }
    note = sq_untransposed(p, note);
    slot = sq_put(p, s, note, vel, &empty);
    if (empty)
        p->nudge[s] = (int8_t)(sqd.quant ? 0 : sy_clampi(sy_floori(d * 100.0f + 0.5f), -50, 50));
    if (ahead)
        sqs.fresh[s] |= (uint8_t)(1u << slot);
    for (i = 0; i < SQ_NREC && sqs.rec[i].used; i++)
        ;
    if (i < SQ_NREC) {
        sqs.rec[i].used = 1;
        sqs.rec[i].note = (uint8_t)note;
        sqs.rec[i].pat = (uint8_t)(p - sq_pat);
        sqs.rec[i].step = (uint8_t)s;
        sqs.rec[i].slot = (uint8_t)slot;
        sqs.rec[i].ph = ph;
    }
    sqs.seq++;
}
/* .. let go: its step as long as it was held (a quarter step at least) */
static void sq_rec_off(uint32_t note)
{
    uint32_t i;
    for (i = 0; i < SQ_NREC; i++)
        if (sqs.rec[i].used) {
            sq_pat_t *p = &sq_pat[sqs.rec[i].pat % SQ_NPAT];
            if (sq_untransposed(p, note) != sqs.rec[i].note)
                continue;
            sqs.rec[i].used = 0;
            if (p->note[sqs.rec[i].step][sqs.rec[i].slot] == sqs.rec[i].note)
                p->len[sqs.rec[i].step] =
                    (uint8_t)sy_clampi(sy_floori((chs.tq / sq_q(p) - sqs.rec[i].ph) * 4.0f + 0.5f), 1, 255);
            sqs.seq++;
            return;
        }
}
/* step recording: the first key of a gesture clears the step picked, each key held with it joins; all let go, the
 * next step is picked */
static void sq_step_rec(uint32_t note, uint32_t vel, int down)
{
    sq_pat_t *p = &sq_pat[sqd.cur % SQ_NPAT];
    uint32_t i, b = 1u << (note & 31u), *w = &sqs.sbits[note >> 5];
    int empty;
    if (!down) {
        if (!(*w & b))
            return;
        *w &= ~b;
        if (!(sqs.sbits[0] | sqs.sbits[1] | sqs.sbits[2] | sqs.sbits[3])) {
            sqd.cursor = (uint8_t)((sqs.sstep + 1u) % sq_len(p));
            sqs.seq++;
        }
        return;
    }
    if (!(sqs.sbits[0] | sqs.sbits[1] | sqs.sbits[2] | sqs.sbits[3])) {
        sqs.sstep = (uint8_t)(sqd.cursor % sq_len(p));
        for (i = 0; i < SQ_NNOTE; i++)
            p->note[sqs.sstep][i] = SQ_NONE;
        p->nudge[sqs.sstep] = 0;
    }
    *w |= b;
    sq_put(p, sqs.sstep, sq_untransposed(p, note), vel, &empty);
    sqs.seq++;
}
/* a note in (sy_app.c sy_ev_drain: the panel's keys and MIDI's, as they play): key, the panel's key it is (its
 * release comes without its note; 0xFF: a note's own); armed, recorded */
static void sy_seq_input(uint32_t key, uint32_t note, uint32_t vel, int down)
{
    if (key < 27u) {
        if (down) {
            sqs.keynote[key] = (uint8_t)note;
        } else {
            note = sqs.keynote[key];
            sqs.keynote[key] = SQ_NONE;
        }
    }
    if (note > 127u || !sqd.arm)
        return;
    if (sqs.run) {
        if (down)
            sq_rec_on(note, vel);
        else
            sq_rec_off(note);
    } else if (sqd.steprec) {
        sq_step_rec(note, vel, down);
    }
}

/* ---------------------------------------------------------------------------------------- the UI's --- */
/* a pattern's steps cleared (its settings kept); a pattern as it starts; the chain as it starts */
static void sq_clear_steps(sq_pat_t *p)
{
    uint32_t s, l;
    memset(p->note, SQ_NONE, sizeof p->note);
    memset(p->vel, 100, sizeof p->vel);
    for (s = 0; s < SQ_NSTEP; s++) {
        p->len[s] = SQ_LEN0;
        p->chance[s] = 100;
        p->nudge[s] = 0;
    }
    for (l = 0; l < SQ_NLANE; l++) {
        memset(p->lane[l], SQ_NONE, SQ_NSTEP);
        p->lane_id[l] = SQ_NOID;
    }
}
static void sq_pat_defaults(sq_pat_t *p)
{
    sq_clear_steps(p);
    p->length = 16;
    p->rate = SQ_RATE0;
    p->swing = 50;
    p->dir = SQ_FWD;
    p->transpose = 0;
    p->rsv[0] = p->rsv[1] = p->rsv[2] = 0;
}
static void sq_chain_defaults(void)
{
    uint32_t i;
    sqd.chain = 0;
    for (i = 0; i < SQ_NCHAIN; i++) {
        sqd.cpat[i] = i ? SQ_NONE : 0u;
        sqd.crep[i] = 1;
    }
}
static void sq_defaults(void)
{
    uint32_t i;
    for (i = 0; i < SQ_NPAT; i++)
        sq_pat_defaults(&sq_pat[i]);
    sq_chain_defaults();
    sqd.cur = 0;
    sqd.next = SQ_NONE;
    sqd.recmode = SQ_OVERDUB;
    sqd.quant = 0;
    sqd.arm = 0;
    sqd.cursor = 0;
    sqd.lane = 0;
    for (i = 0; i < 27u; i++)
        sqs.keynote[i] = SQ_NONE;
    sq_clip_ok = 0;
}
/* a pattern read from the flash (or any): each field kept in its range */
static void sq_pat_fix(sq_pat_t *p)
{
    uint32_t s, i, l;
    for (s = 0; s < SQ_NSTEP; s++) {
        for (i = 0; i < SQ_NNOTE; i++) {
            if (p->note[s][i] > 127u)
                p->note[s][i] = SQ_NONE;
            p->vel[s][i] = (uint8_t)sy_clampi(p->vel[s][i], 1, 127);
        }
        p->len[s] = (uint8_t)(p->len[s] ? p->len[s] : 1u);
        p->chance[s] = (uint8_t)(p->chance[s] > 100u ? 100u : p->chance[s]);
        p->nudge[s] = (int8_t)sy_clampi(p->nudge[s], -50, 50);
    }
    for (l = 0; l < SQ_NLANE; l++)
        if (!sq_lane_ok(p->lane_id[l]))
            p->lane_id[l] = SQ_NOID;
    p->length = (uint8_t)sy_clampi(p->length, 1, SQ_NSTEP);
    p->rate = (uint8_t)(p->rate % SQ_NRATE);
    p->swing = (uint8_t)sy_clampi(p->swing, 50, 75);
    p->dir = (uint8_t)(p->dir % SQ_NDIR);
    p->transpose = (int8_t)sy_clampi(p->transpose, -24, 24);
}
static int sq_step_used(const sq_pat_t *p, uint32_t s)   /* a step holds a note */
{
    uint32_t i;
    for (i = 0; i < SQ_NNOTE; i++)
        if (p->note[s][i] != SQ_NONE)
            return 1;
    return 0;
}
static int sq_pat_used(const sq_pat_t *p)        /* a pattern holds a note or a lane's value */
{
    uint32_t s, l;
    for (s = 0; s < SQ_NSTEP; s++) {
        if (sq_step_used(p, s))
            return 1;
        for (l = 0; l < SQ_NLANE; l++)
            if (p->lane_id[l] != SQ_NOID && p->lane[l][s] != SQ_NONE)
                return 1;
    }
    return 0;
}
/* a knob turned parameter id (the main loop's: sy_ui.c ui_knob): its lane in the pattern playing let go while it
 * turns (the knob heard); recording, its value at the step playing (a lane taken for it if it has none: the first
 * without a parameter, or without values). 0: no lane free for it */
static int sy_seq_knob(uint32_t id, uint32_t *lane)
{
    sq_pat_t *p = &sq_pat[sqd.cur % SQ_NPAT];
    uint32_t l, s, rec = sqd.arm && sqs.run;
    if (!sq_lane_ok(id))
        return 1;
    for (l = 0; l < SQ_NLANE && p->lane_id[l] != id; l++)
        ;
    if (l == SQ_NLANE && !rec)
        return 1;
    if (l == SQ_NLANE) {                         /* (a lane for it: one free, or one without values) */
        for (l = 0; l < SQ_NLANE && p->lane_id[l] != SQ_NOID; l++)
            ;
        for (s = 0; l == SQ_NLANE && s < SQ_NLANE; s++) {
            uint32_t i;
            for (i = 0; i < SQ_NSTEP && p->lane[s][i] == SQ_NONE; i++)
                ;
            if (i == SQ_NSTEP)
                l = s;
        }
        if (l == SQ_NLANE)
            return 0;
        memset(p->lane[l], SQ_NONE, SQ_NSTEP);
        p->lane_id[l] = (uint16_t)id;
    }
    sqd.ltouch |= (uint8_t)(1u << l);
    *lane = l;
    if (rec)
        p->lane[l][sqs.s % SQ_NSTEP] = sq_lane_x(id, sy_patch.p[id]);
    return 1;
}
/* PATTERNS: the pattern shown copied, its copy put in it, it cleared (its steps into the copy first: PASTE puts
 * them back) */
static void sq_copy(void)
{
    sq_clip = sq_pat[sqd.cur % SQ_NPAT];
    sq_clip_ok = 1;
}
static int sq_paste(void)
{
    if (!sq_clip_ok)
        return 0;
    sq_pat[sqd.cur % SQ_NPAT] = sq_clip;
    return 1;
}
static void sq_clear(void)
{
    sq_copy();
    sq_clear_steps(&sq_pat[sqd.cur % SQ_NPAT]);
}

/* ------------------------------------------------------------------------------------------ flash --- */
/* the patterns as the flash keeps them (sy_app.c): four an object (SS_SEQ0 + o), "SQ", a version, the count, then
 * the patterns' bytes */
#define SQ_PER_OBJ 4u
#define SQ_NOBJ (SQ_NPAT / SQ_PER_OBJ)
#define SQ_OBJ_LEN (4u + SQ_PER_OBJ * (uint32_t)sizeof(sq_pat_t))
_Static_assert(SQ_OBJ_LEN <= SS_PAYLOAD_MAX, "four patterns fit an object");
static uint32_t sq_obj_crc(uint32_t o) { return ss_crc32(&sq_pat[o * SQ_PER_OBJ], SQ_PER_OBJ * sizeof(sq_pat_t)); }
static uint32_t sq_obj_pack(uint32_t o)          /* into ss_buf: its length */
{
    ss_buf[0] = 'S', ss_buf[1] = 'Q', ss_buf[2] = 1, ss_buf[3] = SQ_PER_OBJ;
    memcpy(ss_buf + 4, &sq_pat[o * SQ_PER_OBJ], SQ_PER_OBJ * sizeof(sq_pat_t));
    return SQ_OBJ_LEN;
}
static int sq_obj_take(uint32_t o, const uint8_t *b, uint32_t n)   /* an object's bytes into its patterns: 1 */
{
    uint32_t i;
    if (n != SQ_OBJ_LEN || b[0] != 'S' || b[1] != 'Q' || b[2] != 1u || b[3] != SQ_PER_OBJ)
        return 0;
    memcpy(&sq_pat[o * SQ_PER_OBJ], b + 4, SQ_PER_OBJ * sizeof(sq_pat_t));
    for (i = 0; i < SQ_PER_OBJ; i++)
        sq_pat_fix(&sq_pat[o * SQ_PER_OBJ + i]);
    return 1;
}
