/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* STRATA's looper (V0.8; docs/LOOPER.md, docs/CONTROLS.md "The looper"): eight tracks, each a synth (the sound playing:
 * V0.9 gives each its own) or MIDI only (USB, its channel), with SCALE, the chords and the ARP of its own (sy_chord.c:
 * a side a track); eight scenes, A to H: each track a clip a scene; SONG, a chain of scenes.
 *   - a clip: notes and knob moves at free times (96 ticks a quarter note), looped at its length in whole bars (1 ..
 *     16); its notes play as keys do into its track's chords (ids LP_ID0 + a slot), its moves hold their parameters
 *     (sy_param.c sp_ovr_set) while it plays, the patch itself untouched;
 *   - recording (REC, the selected track, the scene picked): an empty clip's first take starts on a bar (stopped:
 *     after a bar of COUNT-IN), the next REC ends it, rounded to whole bars, and it loops; a clip with notes: REC
 *     overdubs, pass after pass (UNDO PASS takes the last back, again puts it back), until REC again. What is played
 *     (the panel's keys as SCALE fits them, MIDI in) goes in at its time (QUANTIZE: on the grid), as long as it was
 *     held; a knob turned (a parameter that can be modulated) writes its moves, the ones it passes replaced;
 *   - launching (the launch pad, SEQ held; CLIPS' knobs): a clip, a scene or a stop waits for the next bar (LAUNCH:
 *     BAR, BEAT, NOW); stopped, a launch starts the transport; SONG plays its scenes in turn, each its repeats;
 *   - the mix: each track's VOLUME and PAN, MUTE and SOLO, as its voices' gains (sy_voice.c sy_tgain);
 *   - the click: on the beats while recording (or always, CLICK ON), the downbeat higher; COUNT-IN's bar of it.
 * The clips' events live in one pool of 2,048 (8 bytes each: a list a clip, by time). The project (the tracks, clips,
 * chords, SONG) is kept in the flash, four of them (sy_store.c, sy_app.c). What plays or records runs in the audio
 * interrupt, a block at a time before the chords' (sy_loop_block); the UI asks (lpr.req, a track's next) or edits
 * with the interrupt held a moment (LP_LOCK). Needs sy_chord.c (cht, chd, ch_d, sy_chord_key_t, ch_use), the engine
 * (sy_note_on / off, sy_ntrk, sy_tgain, sp_ovr_set / drop, SP, sy_tempo, sy_bar_q, sy_patch), SY_MIDI_OUT and fm1_ms. */

#ifndef LP_LOCK
#define LP_LOCK() irq_save()                     /* (the main loop's edits: the audio interrupt held a moment) */
#define LP_UNLOCK(f) irq_restore(f)
#endif

/* ----------------------------------------------------------------------------------------- the data --- */
#define LP_NTRK CH_NTRK                          /* tracks */
#define LP_NSCN 8u                               /* scenes (A .. H): a clip each a track */
#define LP_PPQ 96u                               /* ticks a quarter note */
#define LP_MAXBAR 16u                            /* a clip's bars at most */
#define LP_MAXT 32767u                           /* .. its ticks at most (bit 15 of an event's time: a move) */
#define LP_NEV 2048u                             /* events a project */
#define LP_END 0xFFFFu                           /* no event */
#define LP_MOVE 0x8000u                          /* an event's time: a knob's move, not a note */
#define LP_FRESH 0x80u                           /* a note's velocity: recorded ahead of the playhead (not played as
                                                  * it comes this pass: its key played it) */
#define LP_NOSCN 0xFFu                           /* no scene */
#define LP_STOP 0xFEu                            /* a track's next: stop */
#define LP_NSONG 16u                             /* SONG's slots */
#define LP_NPRJ 4u                               /* projects */
#define LP_ID0 64u                               /* sy_chord_key's ids for a clip's notes: LP_ID0 + a slot */
#define LP_NON 16u                               /* .. a track's sounding at most */
#define LP_NOPEN 8u                              /* notes held while recording, their lengths still to come */
#define LP_NPASS 256u                            /* a pass's notes and moves UNDO PASS takes back at most */
enum { LT_SYNTH, LT_MIDI, LT_DRUM, LT_N };       /* a track's TYPE (V0.85: DRUMS, the kit: sy_drum.c) */
/* V0.85, a drum track's notes are hits (their notes DK_NOTE's, GM's drums): their length is no length but the hit's
 * own, LP_HIT marking it: ACCENT, CHANCE (0 always, 1 .. 7: 7/8 .. 1/8 of the times it comes), RATCHET (0 .. 3: 1 .. 4
 * hits in its sixteenth). They stay so whatever the track's TYPE: a synth or MIDI track plays a hit as a sixteenth
 * (lp_len), a drum track plays a note of a length as a hit of the defaults; a note's own length stays under LP_HIT */
#define LP_HIT 0x4000u
#define LP_ACC 0x0020u
#define LP_CHN(len) (((len) >> 2) & 7u)
#define LP_RAT(len) ((len) & 3u)
#define LP_STEP 24u                              /* the grid's step: a sixteenth */
#define LP_NRAT 8u                               /* ratchets' hits still to come at most */
enum { LR_OFF, LR_COUNT, LR_WAIT, LR_TAKE, LR_DUB };   /* recording: off, counting in, waiting for a bar, the first
                                                       * take, overdubbing */
enum { LL_BAR, LL_BEAT, LL_NOW, LL_N };          /* LAUNCH: when a launch plays */
enum { LC_OFF, LC_REC, LC_ON, LC_N };            /* CLICK */
enum { LQ_OFF, LQ_32, LQ_16, LQ_8, LQ_4, LQ_N }; /* QUANTIZE: the grid */
enum { LS_1, LS_HALF, LS_2, LS_N };              /* V0.86, a clip's SPEED: as recorded, half as fast, twice */
static const char *const LT_NAME[LT_N] = {"SYNTH", "MIDI", "DRUMS"};
static const char *const LL_NAME[LL_N] = {"BAR", "BEAT", "NOW"};
static const char *const LC_NAME[LC_N] = {"OFF", "REC", "ON"};
static const char *const LQ_NAME[LQ_N] = {"OFF", "1/32", "1/16", "1/8", "1/4"};
static const uint8_t LQ_TICKS[LQ_N] = {0, 12, 24, 48, 96};

typedef struct {
    uint16_t t;                                  /* its tick in the clip (LP_MOVE: a move) */
    uint16_t len;                                /* a note: its length in ticks; a move: its parameter */
    uint16_t next;                               /* the clip's next (LP_END: the last); free: the next free */
    uint8_t a, b;                                /* a note: the note, its velocity (LP_FRESH); a move: its value */
} lp_ev_t;
typedef struct {
    uint16_t head;                               /* its first event (LP_END none) */
    uint16_t len;                                /* its length in ticks (0: empty, no clip) */
    uint16_t n;                                  /* its events */
    uint16_t rsv;
} lp_clip_t;
typedef struct {
    uint8_t type;                                /* LT_* */
    uint8_t ch, cin, out;                        /* MIDI: its channel out (0 .. 15), in (0: none, the selected track
                                                  * takes the rest; 1 .. 16), a synth's notes out over USB too */
    uint8_t vol, pan, mute, solo;                /* VOLUME (0 .. 127, 100 as recorded), PAN (0 .. 127, 64 the
                                                  * centre), MUTE, SOLO */
    uint8_t scale, root, fit, kvel;              /* SCALE's: the keys' scale, root, fit, velocity (the UI's ui.*
                                                  * while it is the track selected) */
    uint8_t rsv[4];
} lp_trk_t;
_Static_assert(sizeof(lp_ev_t) == 8u && sizeof(lp_trk_t) == 16u, "the looper's records");
static lp_ev_t lp_ev[LP_NEV] SY_POOL;            /* (16 KB in the pool: set at power-on, lp_boot) */
static uint16_t lp_free, lp_nfree;               /* the free events: a list, its count */
static struct {
    lp_trk_t trk[LP_NTRK];
    lp_clip_t clip[LP_NTRK][LP_NSCN];
    uint8_t song[LP_NSONG], srep[LP_NSONG];      /* SONG: a slot's scene (LP_NOSCN: the song ends before it), its
                                                  * repeats (1 .. 16) */
    uint8_t song_on;
    uint8_t speed[LP_NTRK][LP_NSCN];             /* V0.86: each clip's SPEED (LS_*: x1, x1/2, x2) */
    volatile uint8_t sel;                        /* the track selected: the panel's keys play it, the menus edit it */
    volatile uint8_t scn;                        /* the scene picked (CLIPS' cursor): REC records there, the pages
                                                  * show its clip */
} lpp;
/* the settings (sy_ui.c: the flash's settings record): QUANTIZE, COUNT-IN, CLICK and its LEVEL, LAUNCH, the
 * project */
static struct {
    uint8_t quant, cin, click, clvl, launch, prj;
} lpg = {LQ_OFF, 1, LC_REC, 6, LL_BAR, 0};

/* ------------------------------------------------------------------------------------- the audio side --- */
static struct {
    volatile uint8_t play;                       /* the scene playing (LP_NOSCN none) */
    volatile uint8_t next;                       /* launched: a scene, LP_STOP, LP_NOSCN none (the UI writes it) */
    volatile uint8_t resync;                     /* its clip's list edited: the next event looked up again */
    float start;                                 /* the transport's place (quarter notes) of the clip's tick 0 */
    uint16_t cur, pt;                            /* the next event to play; the tick the last block reached */
    volatile uint16_t pos;                       /* .. as the UI reads it */
    volatile uint16_t passes;                    /* passes played (the UI's) */
    struct {
        uint8_t used, note;
        uint32_t off;                            /* .. when it ends (samples) */
    } on[LP_NON];
} lps[LP_NTRK];
static struct {
    uint8_t run;                                 /* the transport was running a block before */
    int32_t bar, beat;                           /* the bar and the beat the last block was in */
    float lastq;                                 /* the transport's place a block before */
    uint8_t keytrk[27];                          /* the track each panel key went to (its release goes there too) */
    uint8_t mtrk[128];                           /* .. each MIDI note */
    uint8_t keynote[27];                         /* the note each panel key played (its release comes without it) */
    uint8_t sslot, sleft;                        /* SONG: the slot playing, its bars still to go */
    float cq;                                    /* COUNT-IN: quarter notes counted */
    int32_t cbeat;                               /* .. the beat */
    uint32_t ck_n, ck_ph, ck_inc;                /* the click sounding: samples left, phase, increment */
    float ck_a;                                  /* .. its level */
    volatile uint16_t seq;                       /* something the UI shows changed (a launch, a record, an edit) */
    uint32_t rnd;                                /* V0.85: CHANCE's dice */
    struct {
        uint8_t row, trk, vel, acc, left;        /* a RATCHET's hits to come: the drum, its track, how, how many more */
        uint32_t at, step;                       /* .. the next one's sample (cht.t), the samples between */
    } rat[LP_NRAT];
} lpx;
static struct {
    volatile uint8_t req;                        /* the UI's: LQ_REQ_* */
    volatile uint8_t st;                         /* LR_* */
    uint8_t trk, scn, dub;                       /* the clip recording; LR_WAIT for an overdub */
    volatile uint8_t full;                       /* the events ran out (the UI says so) */
    float t0;                                    /* the first take's start (the transport's place) */
    struct {
        uint8_t used, note;
        uint16_t ev;
        float q;                                 /* .. where it came (the transport's place) */
    } open[LP_NOPEN];
    uint16_t pass[LP_NPASS], npass;              /* this pass's events */
    uint16_t last[LP_NPASS], nlast;              /* the pass before's (UNDO PASS takes them back) */
    uint16_t undo[LP_NPASS], nundo;              /* taken back: their events kept until the next take (again: back) */
    uint8_t utrk, uscn;                          /* .. their clip */
    volatile uint16_t touch;                     /* a knob turning: its parameter (0xFFFF none), the UI's */
    volatile uint32_t touch_ms;                  /* .. when it last turned */
    uint16_t mv_id, mv_t;                        /* the move recorded last: its parameter, its tick */
    int16_t mv_v;                                /* .. its value */
} lpr;                                           /* (V0.81: set in lp_boot, not .data's 1.6 KB of the image) */
/* V0.87 (F09): TAKE, REC twice: what was just played on the selected track, kept as it comes (the newest LP_NTAKE
 * notes and hits, 2 KB), made a clip on REC twice (lp_take_make). Each note's place in quarter notes of its clock:
 * the transport's while it runs (its bars then the clip's), else a free one at the tempo */
#define LP_NTAKE 256u
static struct {
    float q[LP_NTAKE];                           /* each one's start */
    uint8_t note[LP_NTAKE], vel[LP_NTAKE];       /* .. its note (a hit: its drum's GM note), velocity (bit 7: the */
                                                 /* transport's clock) */
    uint16_t len[LP_NTAKE];                      /* .. ticks (0: still held); a hit LP_HIT, LP_ACC */
    uint16_t head, n;
    uint8_t trk;
    float fq;                                    /* the free clock: quarter notes, every block */
} lpk;
enum { LP_REQ_NONE, LP_REQ_TAP, LP_REQ_UNDO, LP_REQ_STOPALL };

/* ------------------------------------------------------------------------------------- the event pool --- */
static void lp_pool_init(void)
{
    uint32_t i;
    for (i = 0; i < LP_NEV; i++)
        lp_ev[i].next = (uint16_t)(i + 1u < LP_NEV ? i + 1u : LP_END);
    lp_free = 0;
    lp_nfree = LP_NEV;
}
static uint32_t lp_alloc(void)                   /* a free event (LP_END: none) */
{
    uint32_t i = lp_free;
    if (i == LP_END)
        return LP_END;
    lp_free = lp_ev[i].next;
    lp_nfree--;
    lp_ev[i].next = LP_END;
    return i;
}
static void lp_release(uint32_t i)
{
    lp_ev[i].next = lp_free;
    lp_free = (uint16_t)i;
    lp_nfree++;
}
static uint32_t lp_et(uint32_t i) { return lp_ev[i].t & LP_MAXT; }   /* an event's tick */
static uint32_t lp_len(uint32_t i) { return lp_ev[i].len & LP_HIT ? LP_STEP : lp_ev[i].len; }   /* a note's length (a hit: a */
                                                                                                 /* sixteenth) */
/* event i into clip c, by its tick (after those of the same tick) */
static void lp_insert(lp_clip_t *c, uint32_t i)
{
    uint32_t t = lp_et(i), k = c->head, prev = LP_END;
    while (k != LP_END && lp_et(k) <= t) {
        prev = k;
        k = lp_ev[k].next;
    }
    lp_ev[i].next = (uint16_t)k;
    if (prev == LP_END)
        c->head = (uint16_t)i;
    else
        lp_ev[prev].next = (uint16_t)i;
    c->n++;
}
static int lp_unlink(lp_clip_t *c, uint32_t i)   /* event i out of clip c: 1 (0: not in it) */
{
    uint32_t k = c->head, prev = LP_END;
    while (k != LP_END && k != i) {
        prev = k;
        k = lp_ev[k].next;
    }
    if (k == LP_END)
        return 0;
    if (prev == LP_END)
        c->head = lp_ev[i].next;
    else
        lp_ev[prev].next = lp_ev[i].next;
    lp_ev[i].next = LP_END;
    c->n--;
    return 1;
}
static void lp_clip_empty(lp_clip_t *c)          /* its events freed, it gone */
{
    while (c->head != LP_END) {
        uint32_t i = c->head;
        c->head = lp_ev[i].next;
        lp_release(i);
    }
    c->n = 0;
    c->len = 0;
}
/* the clip's events whose ticks are at or past its length moved round into it (a first take rounded down), sorted */
static void lp_clip_wrap(lp_clip_t *c)
{
    uint32_t k = c->head, len = c->len;
    if (!len)
        return;
    c->head = LP_END;
    c->n = 0;
    while (k != LP_END) {
        uint32_t nx = lp_ev[k].next;
        lp_ev[k].t = (uint16_t)((lp_ev[k].t & LP_MOVE) | (lp_et(k) % len));
        lp_insert(c, k);
        k = nx;
    }
}

/* ------------------------------------------------------------------------------------------ helpers --- */
static uint32_t lp_bar_ticks(void)               /* a bar in ticks (the time signature's) */
{
    uint32_t b = (uint32_t)(sy_bar_q * (float)LP_PPQ + 0.5f);
    return b < LP_PPQ / 4u ? LP_PPQ / 4u : b > LP_MAXT ? LP_MAXT : b;
}
static uint32_t lp_max_bars(void)                /* a clip's bars at most at this time signature */
{
    uint32_t m = LP_MAXT / lp_bar_ticks();
    return m < 1u ? 1u : m > LP_MAXBAR ? LP_MAXBAR : m;
}
static float lp_tick_smp(void) { return SY_FS * 60.0f / ((sy_tempo > 1.0f ? sy_tempo : 120.0f) * (float)LP_PPQ); }
/* V0.86: clip (t, s)'s SPEED as a factor on its ticks a quarter note (x1, x1/2, x2) */
static const float LS_K[LS_N] = {1.0f, 0.5f, 2.0f};
static float lp_speed(uint32_t t, uint32_t sc) { return sc < LP_NSCN ? LS_K[lpp.speed[t % LP_NTRK][sc] % LS_N] : 1.0f; }
static int lp_any_solo(void)
{
    uint32_t t;
    for (t = 0; t < LP_NTRK; t++)
        if (lpp.trk[t].solo)
            return 1;
    return 0;
}
static int lp_muted(uint32_t t)                  /* MUTE, or another track's SOLO */
{
    t %= LP_NTRK;
    return lpp.trk[t].mute || (!lpp.trk[t].solo && lp_any_solo());
}
/* each track's gains (a block at a time): VOLUME's square law (100: 1), PAN's balance */
static void lp_gains(void)
{
    uint32_t t;
    int solo = lp_any_solo();
    for (t = 0; t < LP_NTRK; t++) {
        const lp_trk_t *k = &lpp.trk[t];
        float g = 0.0f, p = ((float)k->pan - 64.0f) * (1.0f / 63.0f);
        if (!k->mute && (k->solo || !solo))
            g = (float)k->vol * (float)k->vol * (1.0f / 10000.0f);
        p = sy_clampf(p, -1.0f, 1.0f);
        sy_tgain[t][0] = g * sy_minf(1.0f, 1.0f - p);
        sy_tgain[t][1] = g * sy_minf(1.0f, 1.0f + p);
    }
}
/* a track's notes out (sy_chord.c's way out): a synth's to the voices (tagged with it; a muted track starts none) and,
 * with OUT, over USB too; a MIDI track's over USB on its channel (part: the chord's 0, the bass's 1, both there) */
static void lp_out_on(uint32_t t, uint32_t note, uint32_t vel, uint32_t part)
{
    const lp_trk_t *k = &lpp.trk[t % LP_NTRK];
    (void)part;
    if (lp_muted(t))
        return;
    if (k->type == LT_SYNTH) {
        sy_ntrk = (uint8_t)(t % LP_NTRK);
        sy_note_on(note, vel);
        sy_ntrk = SY_NOTRK;
    }
    if (k->type == LT_MIDI || k->out)
        SY_MIDI_OUT(0x09u | (0x90u | (k->ch & 15u)) << 8 | note << 16 | vel << 24);
}
static void lp_out_off(uint32_t t, uint32_t note, uint32_t part)
{
    const lp_trk_t *k = &lpp.trk[t % LP_NTRK];
    (void)part;
    if (k->type == LT_SYNTH) {
        sy_ntrk = (uint8_t)(t % LP_NTRK);
        sy_note_off(note);
        sy_ntrk = SY_NOTRK;
    }
    if (k->type == LT_MIDI || k->out)
        SY_MIDI_OUT(0x08u | (0x80u | (k->ch & 15u)) << 8 | note << 16);
}

/* ------------------------------------------------------------------------------------------ the drums --- */
/* V0.85: a note's drum row (GM's drums onto the eight; C1 the DUCK row), DK_ROWS none */
static uint32_t lp_drum_row(uint32_t note)
{
    switch (note) {
    case 35: case 36: return DK_KICK;
    case 37: case 38: case 40: return DK_SNARE;
    case 39: return DK_CLAP;
    case 42: case 44: return DK_HATC;
    case 46: return DK_HATO;
    case 49: case 51: case 52: case 53: case 55: case 57: case 59: return DK_CYM;
    case 41: case 43: case 45: case 47: return DK_WT1;
    case 48: case 50: return DK_WT2;
    case 24: return DK_DUCK;
    default: return DK_ROWS;
    }
}
/* a panel key's drum on a drum track: white keys 1-8 the eight, 9-16 the same accented; the first black key DUCK */
static uint32_t lp_drum_key(uint32_t key, int *acc)
{
    static const uint8_t BLK[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};   /* (from F: sy_gfx.c LK_BLACK) */
    uint32_t i, w = 0;
    *acc = 0;
    if (key >= 27u || BLK[key % 12u])
        return key == 1u ? DK_DUCK : DK_ROWS;
    for (i = 0; i < key; i++)
        w += !BLK[i % 12u];
    *acc = w >= DK_N;
    return w % DK_N;
}
/* a drum hit now for track t (muted: none); a ratchet's rat more in the sixteenth put off (lp_ratchets) */
static void lp_drum_play(uint32_t t, uint32_t row, uint32_t vel, int acc, uint32_t rat)
{
    uint32_t i;
    if (row >= DK_ROWS || lp_muted(t))
        return;
    sy_drum_hit(row, t, vel, acc);
    if (!rat || !cht.run)
        return;
    for (i = 0; i < LP_NRAT && lpx.rat[i].left; i++)
        ;
    if (i == LP_NRAT)
        return;                                  /* (eight ratchets under way: this one's repeats none) */
    lpx.rat[i].row = (uint8_t)row;
    lpx.rat[i].trk = (uint8_t)t;
    lpx.rat[i].vel = (uint8_t)vel;
    lpx.rat[i].acc = (uint8_t)(acc != 0);
    lpx.rat[i].left = (uint8_t)rat;
    lpx.rat[i].step = (uint32_t)((float)LP_STEP / (float)(rat + 1u) * lp_tick_smp() / lp_speed(t, lps[t].play));
    lpx.rat[i].at = cht.t + lpx.rat[i].step;
}
static void lp_ratchets(void)                    /* a block: the ratchets' hits due */
{
    uint32_t i;
    for (i = 0; i < LP_NRAT; i++)
        while (lpx.rat[i].left && (int32_t)(cht.t - lpx.rat[i].at) >= 0) {
            if (!lp_muted(lpx.rat[i].trk))
                sy_drum_hit(lpx.rat[i].row, lpx.rat[i].trk, lpx.rat[i].vel, lpx.rat[i].acc);
            lpx.rat[i].left--;
            lpx.rat[i].at += lpx.rat[i].step;
        }
}
/* a drum track's hit as its clip plays: CHANCE's dice, then the hit and its ratchet */
static void lp_drum_fire(uint32_t t, const lp_ev_t *e)
{
    uint32_t len = e->len, hit = (len & LP_HIT) != 0u, ch = hit ? LP_CHN(len) : 0u;
    if (ch && (sy_rand(&lpx.rnd) >> 29) < ch)    /* (CHANCE k: heard 8 - k times in 8) */
        return;
    lp_drum_play(t, lp_drum_row(e->a), e->b & 0x7Fu, hit && (len & LP_ACC), hit ? LP_RAT(len) : 0u);
}

/* a track's TYPE about to change (the main loop's): what it sounds ends first, the old way */
static void lp_notes_off(uint32_t t);
static void lp_out_off_all(uint32_t t)
{
    uint32_t fl = LP_LOCK(), ti = ch_ti;
    ch_d_t *d = ch_dp;
    ch_s_t *s = ch_sp;
    lp_notes_off(t % LP_NTRK);
    ch_use(t);
    sy_chord_reset();
    ch_dp = d, ch_sp = s, ch_ti = (uint8_t)ti;
    LP_UNLOCK(fl);
}

/* ----------------------------------------------------------------------------------------- playing --- */
static void lp_note_end(uint32_t t, uint32_t i)
{
    if (!lps[t].on[i].used)
        return;
    lps[t].on[i].used = 0;
    sy_chord_key_t(t, LP_ID0 + i, lps[t].on[i].note, 0u, 0);
}
/* a clip's note on until off (the same note of it already on ends first; all in use: the one ending first goes) */
static void lp_note_on(uint32_t t, uint32_t note, uint32_t vel, uint32_t off)
{
    uint32_t i, j = 0;
    for (i = 0; i < LP_NON; i++)
        if (lps[t].on[i].used && lps[t].on[i].note == note)
            lp_note_end(t, i);
    for (i = 0; i < LP_NON && lps[t].on[i].used; i++)
        if ((int32_t)(lps[t].on[i].off - lps[t].on[j].off) < 0)
            j = i;
    if (i == LP_NON) {
        lp_note_end(t, j);
        i = j;
    }
    lps[t].on[i].used = 1;
    lps[t].on[i].note = (uint8_t)note;
    lps[t].on[i].off = off;
    sy_chord_key_t(t, LP_ID0 + i, note, vel, 1);
}
static void lp_ends(uint32_t t)                  /* a track's clip notes due to end */
{
    uint32_t i;
    for (i = 0; i < LP_NON; i++)
        if (lps[t].on[i].used && (int32_t)(cht.t - lps[t].on[i].off) >= 0)
            lp_note_end(t, i);
}
static void lp_notes_off(uint32_t t)
{
    uint32_t i;
    for (i = 0; i < LP_NON; i++)
        lp_note_end(t, i);
}
static void lp_stop_trk(uint32_t t)              /* a track's clip stopped: its notes end, its moves let go */
{
    lp_notes_off(t);
    sp_ovr_drop(t);
    lps[t].play = LP_NOSCN;
    lpx.seq++;
}
static uint32_t lp_seek(const lp_clip_t *c, uint32_t tick)   /* the first event at or after a tick */
{
    uint32_t k = c->head;
    while (k != LP_END && lp_et(k) < tick)
        k = lp_ev[k].next;
    return k;
}
/* scene s of track t from the transport's place q (its tick 0 there) */
static void lp_play_from(uint32_t t, uint32_t s, float q)
{
    lp_notes_off(t);
    if (lps[t].play != s)
        sp_ovr_drop(t);
    lps[t].play = (uint8_t)s;
    lps[t].start = q;
    lps[t].pt = 0;
    lps[t].cur = lpp.clip[t][s].head;
    lps[t].resync = 0;
    lps[t].passes = 0;
    lpx.seq++;
}
static int lp_touched(uint32_t id)               /* a knob turning holds it (its own value heard) */
{
    return id == lpr.touch && fm1_ms - lpr.touch_ms < 400u;
}
static void lp_fire(uint32_t t, lp_ev_t *e)
{
    if (e->t & LP_MOVE) {
        if (!lp_touched(e->len))
            sp_ovr_set(e->len, t, (int16_t)(e->a | (uint16_t)e->b << 8));
        return;
    }
    if (e->b & LP_FRESH) {                       /* (recorded just now, ahead: its key plays it) */
        e->b &= (uint8_t)~LP_FRESH;
        return;
    }
    if (lpp.trk[t].type == LT_DRUM) {            /* (V0.85: a hit, past the chords) */
        lp_drum_fire(t, e);
        return;
    }
    {
        float s = (float)(e->len & LP_HIT ? LP_STEP : e->len ? e->len : 1u) * lp_tick_smp() / lp_speed(t, lps[t].play);
        uint32_t n = s < (float)SY_BLK ? SY_BLK : s > 2.0e9f ? 2000000000u : (uint32_t)s;
        lp_note_on(t, e->a, e->b, cht.t + n);
    }
}
static void lp_walk(uint32_t t, const lp_clip_t *c, uint32_t from, uint32_t to)   /* the events of [from, to) */
{
    while (lps[t].cur != LP_END) {
        lp_ev_t *e = &lp_ev[lps[t].cur];
        uint32_t et = e->t & LP_MAXT;
        if (et >= to || et >= c->len)
            break;
        if (et >= from)
            lp_fire(t, e);
        lps[t].cur = e->next;
    }
}
/* a pass of a clip ended: what was recorded ahead and not reached plays from now on; an overdub's pass is the one
 * before now (UNDO PASS's) */
static void lp_pass_wrap(uint32_t t, lp_clip_t *c)
{
    uint32_t k;
    for (k = c->head; k != LP_END; k = lp_ev[k].next)
        if (!(lp_ev[k].t & LP_MOVE))
            lp_ev[k].b &= (uint8_t)~LP_FRESH;
    lps[t].passes++;
    if (lpr.st == LR_DUB && lpr.trk == t && lpr.npass) {
        memcpy(lpr.last, lpr.pass, lpr.npass * sizeof lpr.pass[0]);
        lpr.nlast = lpr.npass;
        lpr.npass = 0;
    }
}
static uint32_t lp_tick_now(uint32_t t, const lp_clip_t *c)   /* the clip's tick now (V0.86: at its SPEED) */
{
    float q = (cht.tq - lps[t].start) * lp_speed(t, lps[t].play);
    uint32_t k = q > 0.0f ? (uint32_t)(q * (float)LP_PPQ) : 0u;
    return c->len ? k % c->len : 0u;
}
static void lp_track_block(uint32_t t)
{
    lp_clip_t *c;
    uint32_t nt;
    lp_ends(t);
    if (lps[t].play >= LP_NSCN)
        return;
    c = &lpp.clip[t][lps[t].play];
    if ((lpr.st == LR_TAKE && lpr.trk == t) || !c->len) {
        lps[t].pos = 0;
        return;                                  /* (the first take recording: it plays as it is played) */
    }
    if (cht.tq < lps[t].start)
        return;
    nt = lp_tick_now(t, c) + 1u;                 /* (the ticks up to and with the one now: pt, the next to play) */
    if (lps[t].resync) {
        lps[t].resync = 0;
        lps[t].cur = (uint16_t)lp_seek(c, lps[t].pt);
    }
    if (nt >= lps[t].pt) {
        lp_walk(t, c, lps[t].pt, nt);
    } else {
        lp_walk(t, c, lps[t].pt, c->len);
        lp_pass_wrap(t, c);
        lps[t].cur = c->head;
        lp_walk(t, c, 0, nt);
    }
    lps[t].pt = (uint16_t)nt;
    lps[t].pos = (uint16_t)(nt - 1u);
}

/* ------------------------------------------------------------------------------------------ the click --- */
static void lp_click(int down)                   /* a click from now: the downbeat higher */
{
    lpx.ck_n = (uint32_t)(SY_FS * 0.03f);
    lpx.ck_ph = 0;
    lpx.ck_inc = (uint32_t)((down ? 1760.0f : 1175.0f) / SY_FS * 4294967296.0f);
    lpx.ck_a = (float)sy_clampi(lpg.clvl, 0, 10) * (down ? 1400.0f : 1000.0f);
}
static int lp_click_on(void)                     /* CLICK: always (running), or while counting in and recording */
{
    return lpg.click == LC_ON || (lpg.click == LC_REC && lpr.st != LR_OFF);
}
/* the click into a block of the output (Q15, interleaved), before MASTER */
static void lp_click_block(int32_t *o)
{
    uint32_t i;
    if (!lpx.ck_n)
        return;
    for (i = 0; i < SY_BLK && lpx.ck_n; i++) {
        float s = sy_sin2pi((float)lpx.ck_ph * (1.0f / 4294967296.0f)) * lpx.ck_a;
        o[2u * i] += (int32_t)s;
        o[2u * i + 1u] += (int32_t)s;
        lpx.ck_ph += lpx.ck_inc;
        lpx.ck_a *= 0.9985f;
        lpx.ck_n--;
    }
}

/* ---------------------------------------------------------------------------------------- recording --- */
static void lp_pass_add(uint32_t i)
{
    if (lpr.npass < LP_NPASS)
        lpr.pass[lpr.npass++] = (uint16_t)i;
}
static void lp_undo_drop(void)                   /* what UNDO PASS took back, gone for good */
{
    uint32_t k;
    for (k = 0; k < lpr.nundo; k++)
        lp_release(lpr.undo[k]);
    lpr.nundo = 0;
}
static void lp_pass_begin(void)                  /* a take or an overdub starts: nothing to take back yet */
{
    lp_undo_drop();
    lpr.npass = lpr.nlast = 0;
    lpr.mv_id = 0xFFFFu;
}
static uint32_t lp_quant(uint32_t tk, uint32_t len)   /* a tick on QUANTIZE's grid (len: round it in, 0 none) */
{
    uint32_t g = LQ_TICKS[lpg.quant % LQ_N];
    if (g)
        tk = (tk + g / 2u) / g * g;
    if (len)
        tk %= len;
    return tk > LP_MAXT ? LP_MAXT : tk;
}
static uint32_t lp_rec_tick(lp_clip_t *c)       /* where a note played now goes in the clip recording */
{
    if (lpr.st == LR_TAKE) {
        float d = (cht.tq - lpr.t0) * (float)LP_PPQ;
        return d <= 0.0f ? 0u : d >= (float)LP_MAXT ? LP_MAXT - 1u : (uint32_t)d;
    }
    return lp_tick_now(lpr.trk, c);
}
static void lp_rec_close(void)                   /* the notes still held: their lengths now */
{
    uint32_t i;
    lp_clip_t *c = &lpp.clip[lpr.trk][lpr.scn];
    for (i = 0; i < LP_NOPEN; i++)
        if (lpr.open[i].used) {
            float d = (cht.tq - lpr.open[i].q) * (float)LP_PPQ;
            uint32_t m = c->len && c->len < LP_HIT ? c->len : LP_HIT - 1u, l = d < 1.0f ? 1u : d > (float)m ? m : (uint32_t)(d + 0.5f);
            lp_ev[lpr.open[i].ev].len = (uint16_t)l;
            lpr.open[i].used = 0;
        }
}
/* a note played into track t (down or up) while it records */
static void lp_rec_input(uint32_t t, uint32_t note, uint32_t vel, int down)
{
    lp_clip_t *c;
    uint32_t i;
    if (note > 127u || t != lpr.trk || (lpr.st != LR_TAKE && lpr.st != LR_DUB))
        return;
    c = &lpp.clip[t][lpr.scn];
    if (!down) {
        for (i = 0; i < LP_NOPEN; i++)
            if (lpr.open[i].used && lpr.open[i].note == note) {
                float d = (cht.tq - lpr.open[i].q) * (float)LP_PPQ;
                uint32_t m = c->len && c->len < LP_HIT ? c->len : LP_HIT - 1u, l = d < 1.0f ? 1u : d > (float)m ? m : (uint32_t)(d + 0.5f);
                lp_ev[lpr.open[i].ev].len = (uint16_t)l;
                lpr.open[i].used = 0;
                lpx.seq++;
                return;
            }
        return;
    }
    {
        uint32_t raw = lp_rec_tick(c), tk = lp_quant(raw, lpr.st == LR_DUB ? c->len : 0u), e = lp_alloc();
        if (e == LP_END) {
            lpr.full = 1;
            return;
        }
        if (lpr.nundo)
            lp_undo_drop();
        lp_ev[e].t = (uint16_t)tk;
        lp_ev[e].len = 1;
        lp_ev[e].a = (uint8_t)note;
        lp_ev[e].b = (uint8_t)sy_clampi((int32_t)vel, 1, 127);
        if (lpr.st == LR_DUB && (tk >= lps[t].pt || tk + c->len / 2u < raw))
            lp_ev[e].b |= LP_FRESH;              /* (ahead of the playhead, or round to the start: not played again
                                                  * as it comes) */
        lp_insert(c, e);
        lps[t].resync = 1;
        lp_pass_add(e);
        for (i = 0; i < LP_NOPEN && lpr.open[i].used; i++)
            ;
        if (i == LP_NOPEN)
            i = 0;
        lpr.open[i].used = 1;
        lpr.open[i].note = (uint8_t)note;
        lpr.open[i].ev = (uint16_t)e;
        lpr.open[i].q = cht.tq;
        lpx.seq++;
    }
}
/* V0.85: a drum hit played into track t while it records (a hit: no length to wait for; LP_HIT's own, ACCENT if
 * accented) */
static void lp_rec_hit(uint32_t t, uint32_t row, uint32_t vel, int acc)
{
    lp_clip_t *c;
    uint32_t raw, tk, e;
    if (row >= DK_ROWS || t != lpr.trk || (lpr.st != LR_TAKE && lpr.st != LR_DUB))
        return;
    c = &lpp.clip[t][lpr.scn];
    raw = lp_rec_tick(c);
    tk = lp_quant(raw, lpr.st == LR_DUB ? c->len : 0u);
    e = lp_alloc();
    if (e == LP_END) {
        lpr.full = 1;
        return;
    }
    if (lpr.nundo)
        lp_undo_drop();
    lp_ev[e].t = (uint16_t)tk;
    lp_ev[e].len = (uint16_t)(LP_HIT | (acc ? LP_ACC : 0u));
    lp_ev[e].a = DK_NOTE[row];
    lp_ev[e].b = (uint8_t)sy_clampi((int32_t)vel, 1, 127);
    if (lpr.st == LR_DUB && (tk >= lps[t].pt || tk + c->len / 2u < raw))
        lp_ev[e].b |= LP_FRESH;                  /* (as a note's: its key played it) */
    lp_insert(c, e);
    lps[t].resync = 1;
    lp_pass_add(e);
    lpx.seq++;
}
/* the first take ended: its length the bars it took, rounded (1 .. 16), its events round into it; it plays on */
static void lp_take_end(void)
{
    lp_clip_t *c = &lpp.clip[lpr.trk][lpr.scn];
    float b = (cht.tq - lpr.t0) / (sy_bar_q > 0.0f ? sy_bar_q : 4.0f);
    uint32_t bars = (uint32_t)sy_clampi(sy_floori(b + 0.5f), 1, (int32_t)lp_max_bars());
    c->len = (uint16_t)(bars * lp_bar_ticks());
    lp_clip_wrap(c);
    lps[lpr.trk].play = lpr.scn;
    lps[lpr.trk].start = lpr.t0;
    lps[lpr.trk].pt = (uint16_t)(lp_tick_now(lpr.trk, c) + 1u);
    lps[lpr.trk].cur = (uint16_t)lp_seek(c, lps[lpr.trk].pt);
    lps[lpr.trk].passes = 0;
    lpr.nlast = lpr.npass;                       /* (the take is the pass UNDO PASS takes back) */
    memcpy(lpr.last, lpr.pass, lpr.npass * sizeof lpr.pass[0]);
    lpr.npass = 0;
    lpx.seq++;
}
static void lp_rec_stop(void)                    /* recording ends however it was */
{
    if (lpr.st == LR_TAKE || lpr.st == LR_DUB) {
        if (lpr.st == LR_TAKE)
            lp_take_end();
        lp_rec_close();
    }
    lpr.st = LR_OFF;
    lpx.seq++;
}
/* REC: on the selected track and the scene picked, an empty clip's first take (stopped: COUNT-IN first, or at once;
 * running: from the next bar), a clip's overdub (it launched if it is not the one playing); recording, it ends */
static void lp_rec_tap(void)
{
    uint32_t t = lpp.sel % LP_NTRK, s = lpp.scn % LP_NSCN;
    lp_clip_t *c = &lpp.clip[t][s];
    if (lpr.st != LR_OFF) {
        lp_rec_stop();
        return;
    }
    lpr.trk = (uint8_t)t;
    lpr.scn = (uint8_t)s;
    lpr.dub = c->len != 0u;
    if (!lpr.dub)
        lpp.speed[t][s] = LS_1;                  /* (V0.86: a first take as played, its SPEED x1) */
    lp_pass_begin();
    if (lpr.dub && cht.run && lps[t].play == s) {
        lpr.st = LR_DUB;
    } else if (!cht.run) {
        if (lpr.dub)
            lps[t].next = (uint8_t)s;
        if (lpg.cin) {
            lpr.st = LR_COUNT;
            lpx.cq = 0.0f;
            lpx.cbeat = -1;
        } else {
            sy_chord_run(1);
            lpr.st = LR_WAIT;
        }
    } else {
        if (lpr.dub)
            lps[t].next = (uint8_t)s;
        lpr.st = LR_WAIT;
    }
    lpx.seq++;
}
/* UNDO PASS: the pass recording (or, none yet, the one before) taken out of its clip; again (nothing recorded since):
 * put back */
static void lp_undo(void)
{
    lp_clip_t *c;
    uint32_t k;
    uint16_t *l;
    if (lpr.nundo) {                             /* (again: back) */
        c = &lpp.clip[lpr.utrk][lpr.uscn];
        for (k = 0; k < lpr.nundo; k++)
            lp_insert(c, lpr.undo[k]);
        memcpy(lpr.last, lpr.undo, lpr.nundo * sizeof lpr.undo[0]);
        lpr.nlast = lpr.nundo;
        lpr.nundo = 0;
        lps[lpr.utrk].resync = 1;
        lpx.seq++;
        return;
    }
    if (lpr.st == LR_TAKE)
        return;
    lp_rec_close();
    l = lpr.npass ? lpr.pass : lpr.last;
    {
        uint32_t cnt = lpr.npass ? lpr.npass : lpr.nlast, m = 0;
        c = &lpp.clip[lpr.trk][lpr.scn];
        for (k = 0; k < cnt; k++)
            if (lp_unlink(c, l[k]))
                lpr.undo[m++] = l[k];
        lpr.nundo = (uint16_t)m;
        lpr.utrk = lpr.trk;
        lpr.uscn = lpr.scn;
        if (lpr.npass)
            lpr.npass = 0;
        else
            lpr.nlast = 0;
    }
    lp_notes_off(lpr.trk);
    lps[lpr.trk].resync = 1;
    lpx.seq++;
}
/* a block of knob moves (recording, a synth track, a knob turning): its value at the tick now, the old moves of it
 * the playhead passed taken out (the turn replaces them) */
static void lp_rec_moves(void)
{
    lp_clip_t *c;
    uint32_t id = lpr.touch, tk, k, prev = LP_END;
    int16_t v;
    if ((lpr.st != LR_TAKE && lpr.st != LR_DUB) || lpp.trk[lpr.trk].type != LT_SYNTH || !lp_touched(id) ||
        id >= SP_COUNT)
        return;
    c = &lpp.clip[lpr.trk][lpr.scn];
    tk = lp_rec_tick(c);
    v = sy_patch.p[id];
    if (lpr.mv_id == id && lpr.st == LR_DUB) {   /* (the moves passed since the last block's: out) */
        uint32_t a = lpr.mv_t, b = tk;
        for (k = c->head; k != LP_END;) {
            uint32_t nx = lp_ev[k].next, et = lp_et(k);
            int in = a <= b ? et > a && et < b : et > a || et < b;
            if ((lp_ev[k].t & LP_MOVE) && lp_ev[k].len == id && in) {
                if (prev == LP_END)
                    c->head = (uint16_t)nx;
                else
                    lp_ev[prev].next = (uint16_t)nx;
                c->n--;
                lp_release(k);
            } else {
                prev = k;
            }
            k = nx;
        }
        lps[lpr.trk].resync = 1;
    }
    if (lpr.mv_id == id && (lpr.mv_v == v || (tk >= lpr.mv_t && tk - lpr.mv_t < LP_PPQ / 16u))) {
        lpr.mv_t = (uint16_t)tk;                 /* (the same value, or under a 1/64 since the last: not again) */
        return;
    }
    k = lp_alloc();
    if (k == LP_END) {
        lpr.full = 1;
        return;
    }
    if (lpr.nundo)
        lp_undo_drop();
    lp_ev[k].t = (uint16_t)(LP_MOVE | tk);
    lp_ev[k].len = (uint16_t)id;
    lp_ev[k].a = (uint8_t)((uint16_t)v & 0xFFu);
    lp_ev[k].b = (uint8_t)((uint16_t)v >> 8);
    lp_insert(c, k);
    lp_pass_add(k);
    lps[lpr.trk].resync = 1;
    lpr.mv_id = (uint16_t)id;
    lpr.mv_t = (uint16_t)tk;
    lpr.mv_v = v;
    lpx.seq++;
}

/* ----------------------------------------------------------------------------------------- the block --- */
static uint32_t lp_scene_bars(uint32_t s)       /* a scene's length: its longest clip's bars (1 at least) */
{
    uint32_t t, m = 1, bt = lp_bar_ticks();
    for (t = 0; t < LP_NTRK; t++) {             /* (V0.86: at half speed twice its bars, at double half) */
        float k = lp_speed(t, s % LP_NSCN);
        uint32_t b = (uint32_t)sy_floori((float)lpp.clip[t][s % LP_NSCN].len / ((float)bt * k) + 0.999f);
        m = b > m ? b : m;
    }
    return m;
}
static void lp_scene_next(uint32_t s)            /* scene s launched: each track its clip there (none: stops) */
{
    uint32_t t;
    for (t = 0; t < LP_NTRK; t++)
        lps[t].next = lpp.clip[t][s % LP_NSCN].len ? (uint8_t)(s % LP_NSCN) : LP_STOP;
}
static void lp_launches(float q)                 /* the launches waiting play from q */
{
    uint32_t t;
    for (t = 0; t < LP_NTRK; t++) {
        uint32_t n = lps[t].next;
        if (n == LP_NOSCN)
            continue;
        lps[t].next = LP_NOSCN;
        if (n < LP_NSCN && lpp.clip[t][n].len && !(lpr.st == LR_TAKE && lpr.trk == t))
            lp_play_from(t, n, q);
        else if (n == LP_STOP || n < LP_NSCN)
            lp_stop_trk(t);
    }
}
static void lp_song_step(float q, int first)     /* SONG at a bar's start: its next slot when this one is done */
{
    uint32_t n;
    if (!lpp.song_on)
        return;
    if (!first && lpx.sleft > 1u) {
        lpx.sleft--;
        return;
    }
    n = first ? 0u : lpx.sslot + 1u;
    if (n >= LP_NSONG || lpp.song[n] >= LP_NSCN)
        n = 0;
    if (lpp.song[n] >= LP_NSCN)
        return;
    lpx.sslot = (uint8_t)n;
    lpx.sleft = (uint8_t)sy_clampi((int32_t)(lp_scene_bars(lpp.song[n]) * sy_clampi(lpp.srep[n], 1, 16)), 1, 255);
    lpp.scn = lpp.song[n];
    lp_scene_next(lpp.song[n]);
    lp_launches(q);
}
static void lp_bar_line(float q, int first)      /* a bar starts at q */
{
    lp_song_step(q, first);
    if (lpg.launch == LL_BAR)
        lp_launches(q);
    if (lpr.st == LR_WAIT && !lpr.dub) {         /* (the first take starts here) */
        uint32_t t = lpr.trk;
        if (lps[t].play < LP_NSCN)
            lp_stop_trk(t);
        lps[t].next = LP_NOSCN;
        lps[t].play = lpr.scn;
        lps[t].cur = LP_END;                     /* (nothing played until the take ends: lp_take_end) */
        lps[t].pt = 0;
        lpr.t0 = q;
        lpr.st = LR_TAKE;
        lpx.seq++;
    }
}
static void lp_transport_started(void)          /* PLAY: what is on plays from its start; the launches wait */
{
    uint32_t t;
    for (t = 0; t < LP_NTRK; t++)
        if (lps[t].play < LP_NSCN && lps[t].next == LP_NOSCN && lpp.clip[t][lps[t].play].len)
            lp_play_from(t, lps[t].play, 0.0f);
    lpx.bar = lpx.beat = -1;
    lpx.run = 1;
    lpx.lastq = 0.0f;
}
static void lp_transport_stopped(void)          /* stopped: the notes end, the moves let go, recording ends */
{
    uint32_t t;
    if (lpr.st != LR_COUNT)
        lp_rec_stop();
    for (t = 0; t < LP_NTRK; t++)
        lp_notes_off(t);
    sp_ovr_drop(0xFFu);
    memset(lpx.rat, 0, sizeof lpx.rat);          /* (V0.85: no ratchet's hits after) */
    lpx.run = 0;
    lpx.seq++;
}
/* COUNT-IN: a bar of clicks on the panel's tempo, then the transport runs (and the take waits for its first bar) */
static void lp_count_block(void)
{
    float beat = sy_beat_q > 0.0f ? sy_beat_q : 1.0f;
    int32_t b = sy_floori(lpx.cq / beat);
    if (b != lpx.cbeat) {
        lpx.cbeat = b;
        if (lpx.cq < sy_bar_q)
            lp_click(b == 0);
    }
    lpx.cq += (float)SY_BLK * (sy_tempo > 1.0f ? sy_tempo : 120.0f) / (60.0f * SY_FS);
    if (lpx.cq >= sy_bar_q) {
        lpr.st = LR_WAIT;
        sy_chord_run(1);
    }
}
/* a block (the audio interrupt, before the chords'): the UI's asks, COUNT-IN, the bars and beats (the launches, SONG,
 * the click, a take's start), each track's clip, the moves recorded */
static void sy_loop_block(void)
{
    uint32_t t;
    lp_gains();
    lpk.fq += (float)SY_BLK * sy_tempo / (60.0f * SY_FS);   /* (V0.87: TAKE's free clock) */
    if (lpk.fq > 1e6f)
        lpk.fq = 0.0f, lpk.n = lpk.head = 0;
    if (lpr.req) {
        uint32_t r = lpr.req;
        lpr.req = LP_REQ_NONE;
        if (r == LP_REQ_TAP)
            lp_rec_tap();
        else if (r == LP_REQ_UNDO)
            lp_undo();
        else if (r == LP_REQ_STOPALL)
            for (t = 0; t < LP_NTRK; t++)
                lps[t].next = LP_STOP;
    }
    if (lpr.st == LR_COUNT && !cht.run)
        lp_count_block();
    if (!cht.run || cht.pulses < 0) {
        if (lpx.run && !cht.run)
            lp_transport_stopped();
        for (t = 0; t < LP_NTRK; t++) {
            lp_ends(t);
            if (!cht.run && lps[t].next != LP_NOSCN && lps[t].next != LP_STOP && lps[t].next < LP_NSCN) {
                if (lps[t].play != lps[t].next)  /* (stopped: what is launched is on, from the start when it runs) */
                    sp_ovr_drop(t);
                lps[t].play = lps[t].next;
                lps[t].next = LP_NOSCN;
                lps[t].cur = lpp.clip[t][lps[t].play].head;
                lps[t].pt = 0;
                lps[t].resync = 0;
                lpx.seq++;
            } else if (!cht.run && lps[t].next == LP_STOP) {
                lps[t].play = LP_NOSCN;
                lps[t].next = LP_NOSCN;
                lpx.seq++;
            }
        }
        return;
    }
    if (lpr.st == LR_COUNT)
        lpr.st = LR_WAIT;                        /* (PLAY during the count: the take waits for its bar) */
    if (!lpx.run)
        lp_transport_started();
    else if (cht.tq + 1000.0f < lpx.lastq)       /* (the transport's place went back by its 3840: the clips' too) */
        for (t = 0; t < LP_NTRK; t++)
            lps[t].start -= 3840.0f;
    {
        float bq = sy_bar_q > 0.0f ? sy_bar_q : 4.0f, beat = sy_beat_q > 0.0f ? sy_beat_q : 1.0f;
        int32_t bar = sy_floori(cht.tq / bq), bt = sy_floori(cht.tq / beat);
        int first = lpx.bar < 0;
        if (bar != lpx.bar) {
            lpx.bar = bar;
            lp_bar_line((float)bar * bq, first);
        }
        if (bt != lpx.beat) {
            lpx.beat = bt;
            if (lpg.launch == LL_BEAT)
                lp_launches((float)bt * beat);
            if (lp_click_on())
                lp_click((float)bt * beat - (float)bar * bq < 0.5f * beat);
        }
        if (lpg.launch == LL_NOW)
            lp_launches(cht.tq);
    }
    if (lpr.st == LR_WAIT && lpr.dub && lps[lpr.trk].play == lpr.scn && lps[lpr.trk].next == LP_NOSCN)
        lpr.st = LR_DUB;                         /* (the overdub's clip launched: it records from now) */
    for (t = 0; t < LP_NTRK; t++)
        lp_track_block(t);
    lp_ratchets();                               /* (V0.85: the drums' ratchets due) */
    if (lpr.st == LR_TAKE && (cht.tq - lpr.t0) * (float)LP_PPQ >= (float)(lp_max_bars() * lp_bar_ticks()))
        lp_rec_stop();                           /* (sixteen bars: the take ends itself) */
    lp_rec_moves();
    lpx.lastq = cht.tq;
}

/* V0.87: TAKE's memory (the audio side): a note or a hit played into track t (another track's: it starts again) */
static void lp_take_in(uint32_t t, uint32_t note, uint32_t vel, int down, uint32_t hit)
{
    uint32_t i, k;
    if (note > 127u)
        return;
    if (t != lpk.trk) {
        lpk.trk = (uint8_t)t;
        lpk.n = lpk.head = 0;
    }
    if (down) {
        i = lpk.head;
        lpk.q[i] = cht.run ? cht.tq : lpk.fq;
        lpk.note[i] = (uint8_t)note;
        lpk.vel[i] = (uint8_t)(sy_clampi((int32_t)vel, 1, 127) | (cht.run ? 128 : 0));
        lpk.len[i] = (uint16_t)hit;
        lpk.head = (uint16_t)((i + 1u) % LP_NTAKE);
        if (lpk.n < LP_NTAKE)
            lpk.n++;
        return;
    }
    for (k = 0; k < lpk.n; k++) {                /* (its end: the newest of that note still held) */
        i = (lpk.head + LP_NTAKE - 1u - k) % LP_NTAKE;
        if (lpk.note[i] == note && !lpk.len[i]) {
            float d = ((lpk.vel[i] & 128u ? cht.tq : lpk.fq) - lpk.q[i]) * (float)LP_PPQ;
            lpk.len[i] = (uint16_t)(d < 1.0f ? 1u : d > (float)(LP_HIT - 1u) ? LP_HIT - 1u : (uint32_t)(d + 0.5f));
            return;
        }
    }
}
static uint32_t lp_take_len(uint32_t i)          /* .. note i's length in ticks (still held: to now) */
{
    float d;
    if (lpk.len[i])
        return lpk.len[i];
    d = ((lpk.vel[i] & 128u ? cht.tq : lpk.fq) - lpk.q[i]) * (float)LP_PPQ;
    return d < 1.0f ? 1u : d > (float)(LP_HIT - 1u) ? LP_HIT - 1u : (uint32_t)(d + 0.5f);
}
/* .. REC twice (the main loop): what TAKE kept, of the clock the newest was played on and of the last lp_max_bars()
 * bars of it, a clip on the selected track in the scene picked (an empty one): from the bar it starts in (the
 * transport's) or the quarter (the free clock), the bars it takes; its notes still held end now. 0: none kept; -1
 * that scene has a clip; -2 the events ran out */
static int lp_take_make(void)
{
    uint32_t t = lpp.sel % LP_NTRK, s = lpp.scn % LP_NSCN, k, i, n = 0, bars, fl, run;
    float bq = sy_bar_q > 0.0f ? sy_bar_q : 4.0f, last, first, end, start;
    lp_clip_t *c = &lpp.clip[t][s];
    if (!lpk.n || lpk.trk != t)
        return 0;
    if (c->len)
        return -1;
    run = lpk.vel[(lpk.head + LP_NTAKE - 1u) % LP_NTAKE] & 128u;
    last = lpk.q[(lpk.head + LP_NTAKE - 1u) % LP_NTAKE];
    first = end = last;
    for (k = 0; k < lpk.n; k++) {                /* (the newest back, its clock's, within the bars a clip holds) */
        i = (lpk.head + LP_NTAKE - 1u - k) % LP_NTAKE;
        if ((lpk.vel[i] & 128u) != run || last - lpk.q[i] >= bq * (float)lp_max_bars() - 1e-3f)
            break;
        first = lpk.q[i];
        n++;
    }
    start = run ? (float)sy_floori(first / bq) * bq : (float)sy_floori(first);
    for (k = 0; k < n; k++) {
        i = (lpk.head + LP_NTAKE - 1u - k) % LP_NTAKE;
        end = sy_maxf(end, lpk.q[i] + (lpk.len[i] & LP_HIT ? 0.25f : (float)lp_take_len(i) / (float)LP_PPQ));
    }
    bars = (uint32_t)sy_clampi(sy_floori((end - start) / bq - 1e-3f) + 1, 1, (int32_t)lp_max_bars());
    fl = LP_LOCK();
    c->len = (uint16_t)(bars * lp_bar_ticks());
    LP_UNLOCK(fl);
    for (k = n; k-- > 0;) {                      /* (oldest first: as they were played) */
        uint32_t e, tk;
        i = (lpk.head + LP_NTAKE - 1u - k) % LP_NTAKE;
        tk = (uint32_t)sy_clampi(sy_floori((lpk.q[i] - start) * (float)LP_PPQ + 0.5f), 0, (int32_t)c->len - 1);
        fl = LP_LOCK();
        if ((e = lp_alloc()) == LP_END) {
            LP_UNLOCK(fl);
            return -2;
        }
        lp_ev[e].t = (uint16_t)tk;
        lp_ev[e].len = (uint16_t)(lpk.len[i] & LP_HIT ? lpk.len[i] : lp_take_len(i));
        lp_ev[e].a = lpk.note[i];
        lp_ev[e].b = (uint8_t)(lpk.vel[i] & 127u);
        lp_insert(c, e);
        LP_UNLOCK(fl);
    }
    lpk.n = lpk.head = 0;
    lpx.seq++;
    return (int)n;
}

/* ---------------------------------------------------------------------------------------- the input --- */
/* a panel key (0 .. 26) or a MIDI note (key 0xFF, chan its channel) in: to its track (a key: the selected track's,
 * its release to the same; a MIDI note: the track whose IN is its channel, else the selected), through its chords,
 * recorded if it records */
static uint32_t lp_trk_in(uint32_t chan)
{
    uint32_t t;
    for (t = 0; t < LP_NTRK; t++)
        if (lpp.trk[t].cin == chan + 1u)
            return t;
    return lpp.sel % LP_NTRK;
}
static void sy_loop_key(uint32_t key, uint32_t note, uint32_t vel, int down, uint32_t chan)
{
    uint32_t t, id;
    if (key < 27u) {
        if (down) {
            t = lpp.sel % LP_NTRK;
            lpx.keytrk[key] = (uint8_t)t;
            lpx.keynote[key] = (uint8_t)note;
        } else {
            t = lpx.keytrk[key] % LP_NTRK;
            note = lpx.keynote[key];
            lpx.keynote[key] = 0xFF;
        }
        id = key;
        vel = down ? ch_d[t].vel : 0u;
    } else {
        note &= 127u;
        if (down) {
            t = lp_trk_in(chan);
            lpx.mtrk[note] = (uint8_t)t;
        } else {
            t = lpx.mtrk[note] % LP_NTRK;
        }
        id = 128u + note;
    }
    if (lpp.trk[t].type == LT_DRUM) {            /* (V0.85: a drum track: a key's drum or a note's, a hit now, past */
        int acc = 0;                             /* the chords; a release nothing) */
        uint32_t row = key < 27u ? lp_drum_key(key, &acc) : lp_drum_row(note);
        if (down && row < DK_ROWS) {
            lp_drum_play(t, row, vel ? vel : 100u, acc, 0u);
            lp_rec_hit(t, row, vel ? vel : 100u, acc);
            lp_take_in(t, DK_NOTE[row], vel ? vel : 100u, 1, LP_HIT | (acc ? LP_ACC : 0u));   /* (V0.87: TAKE's) */
        }
        return;
    }
    sy_chord_key_t(t, id, note, vel, down);
    lp_rec_input(t, note, vel, down);
    lp_take_in(t, note, vel, down, 0u);          /* (V0.87: TAKE's) */
}

/* --------------------------------------------------------------------------------------- the UI's --- */
/* (the main loop's: the audio side asked, or edited with it held a moment) */
/* a clip launched (s: a scene, LP_STOP); a scene; stopped, the transport starts with it */
static void lp_launch(uint32_t t, uint32_t s)
{
    lps[t % LP_NTRK].next = (uint8_t)s;
    lpx.seq++;
}
static void lp_launch_scene(uint32_t s)
{
    lp_scene_next(s);
    lpp.scn = (uint8_t)(s % LP_NSCN);
    lpx.seq++;
}
static int lp_running(void) { return cht.run != 0; }
/* the clip of track t at scene s: its notes and moves, its bars (0: none) */
static uint32_t lp_clip_bars(uint32_t t, uint32_t s)
{
    uint32_t bt = lp_bar_ticks();
    return (lpp.clip[t % LP_NTRK][s % LP_NSCN].len + bt - 1u) / bt;
}
static lp_clip_t *lp_cur_clip(void) { return &lpp.clip[lpp.sel % LP_NTRK][lpp.scn % LP_NSCN]; }
static int lp_cur_rec(void)                      /* the selected clip is the one recording (or about to) */
{
    return lpr.st != LR_OFF && lpr.trk == lpp.sel && lpr.scn == lpp.scn;
}
/* a clip's notes, by time: the i-th's event (LP_END none); their count */
static uint32_t lp_note_n(const lp_clip_t *c)
{
    uint32_t k, n = 0;
    for (k = c->head; k != LP_END; k = lp_ev[k].next)
        n += !(lp_ev[k].t & LP_MOVE);
    return n;
}
static uint32_t lp_note_at(const lp_clip_t *c, uint32_t i)
{
    uint32_t k;
    for (k = c->head; k != LP_END; k = lp_ev[k].next)
        if (!(lp_ev[k].t & LP_MOVE) && !i--)
            return k;
    return LP_END;
}
/* an event deleted (V0.85): out of the recording's pass lists too, so that UNDO PASS never takes an event that went
 * and came back as another */
static void lp_forget(uint32_t k)
{
    uint32_t i, n;
    for (i = n = 0; i < lpr.npass; i++)
        if (lpr.pass[i] != k)
            lpr.pass[n++] = lpr.pass[i];
    lpr.npass = (uint16_t)n;
    for (i = n = 0; i < lpr.nlast; i++)
        if (lpr.last[i] != k)
            lpr.last[n++] = lpr.last[i];
    lpr.nlast = (uint16_t)n;
}
/* NOTES' edits of note i of the selected clip (f: 0 PITCH, 1 START (ticks), 2 LENGTH (ticks), 3 VELOCITY: by d;
 * 4 deleted). Its index after (a move in time reorders them; -1 gone) */
static int32_t lp_note_edit(uint32_t i, uint32_t f, int32_t d)
{
    lp_clip_t *c = lp_cur_clip();
    uint32_t k = lp_note_at(c, i), fl = LP_LOCK(), j, n;
    int32_t r = (int32_t)i;
    if (k == LP_END || !c->len) {
        LP_UNLOCK(fl);
        return -1;
    }
    if (f == 0u) {
        lp_ev[k].a = (uint8_t)sy_clampi(lp_ev[k].a + d, 0, 127);
    } else if (f == 1u) {
        int32_t t = ((int32_t)lp_et(k) + d) % (int32_t)c->len;
        lp_unlink(c, k);
        lp_ev[k].t = (uint16_t)(t < 0 ? t + (int32_t)c->len : t);
        lp_insert(c, k);
        for (j = c->head, n = 0; j != LP_END && j != k; j = lp_ev[j].next)
            n += !(lp_ev[j].t & LP_MOVE);
        r = (int32_t)n;
    } else if (f == 2u) {
        if (lpp.trk[lpp.sel % LP_NTRK].type != LT_DRUM)   /* (a hit's length is its own: the grid's; on a synth track */
            lp_ev[k].len = (uint16_t)sy_clampi((int32_t)lp_len(k) + d, 1,   /* a hit is a sixteenth, a length from it) */
                                               (int32_t)(c->len < LP_HIT ? c->len : LP_HIT - 1u));
    } else if (f == 3u) {
        lp_ev[k].b = (uint8_t)(sy_clampi((lp_ev[k].b & 0x7F) + d, 1, 127));
    } else {
        lp_unlink(c, k);
        lp_forget(k);
        lp_release(k);
        n = lp_note_n(c);
        r = n ? (int32_t)(i < n ? i : n - 1u) : -1;
    }
    lps[lpp.sel % LP_NTRK].resync = 1;
    LP_UNLOCK(fl);
    lpx.seq++;
    return r;
}
/* CLIP's: its LENGTH (bars: an empty clip made one, of no notes), QUANTIZE (its notes onto the grid g ticks), its
 * moves of a parameter cleared (0xFFFF: all of them), cleared (the clip gone) */
static void lp_clip_bars_set(uint32_t bars)
{
    lp_clip_t *c = lp_cur_clip();
    uint32_t fl = LP_LOCK();
    c->len = (uint16_t)(sy_clampi((int32_t)bars, 1, (int32_t)lp_max_bars()) * lp_bar_ticks());
    lps[lpp.sel % LP_NTRK].resync = 1;
    LP_UNLOCK(fl);
    lpx.seq++;
}
static void lp_clip_quantize(uint32_t g)
{
    lp_clip_t *c = lp_cur_clip();
    uint32_t fl, k, nx;
    if (!g || !c->len)
        return;
    fl = LP_LOCK();
    k = c->head;
    c->head = LP_END;
    c->n = 0;
    for (; k != LP_END; k = nx) {
        nx = lp_ev[k].next;
        if (!(lp_ev[k].t & LP_MOVE))
            lp_ev[k].t = (uint16_t)((lp_et(k) + g / 2u) / g * g % c->len);
        lp_insert(c, k);
    }
    lps[lpp.sel % LP_NTRK].resync = 1;
    LP_UNLOCK(fl);
    lpx.seq++;
}
static void lp_moves_clear(uint32_t id)
{
    lp_clip_t *c = lp_cur_clip();
    uint32_t fl = LP_LOCK(), k = c->head, prev = LP_END;
    while (k != LP_END) {
        uint32_t nx = lp_ev[k].next;
        if ((lp_ev[k].t & LP_MOVE) && (id == 0xFFFFu || lp_ev[k].len == id)) {
            if (prev == LP_END)
                c->head = (uint16_t)nx;
            else
                lp_ev[prev].next = (uint16_t)nx;
            c->n--;
            lp_release(k);
        } else {
            prev = k;
        }
        k = nx;
    }
    lps[lpp.sel % LP_NTRK].resync = 1;
    if (lps[lpp.sel % LP_NTRK].play == lpp.scn)
        for (k = 0; k < SP_COUNT; k++)
            if (id == 0xFFFFu || k == id)
                if (sp_ovr_on[k] == lpp.sel % LP_NTRK + 1u)
                    sp_ovr_set(k, 0xFFu, 0);
    LP_UNLOCK(fl);
    lpx.seq++;
}
/* the parameters a clip moves, the first n of them into ids; their count */
static uint32_t lp_moves_of(const lp_clip_t *c, uint16_t *ids, uint32_t max)
{
    uint32_t k, n = 0, j;
    for (k = c->head; k != LP_END; k = lp_ev[k].next) {
        if (!(lp_ev[k].t & LP_MOVE))
            continue;
        for (j = 0; j < n && ids[j] != lp_ev[k].len; j++)
            ;
        if (j == n && n < max)
            ids[n++] = lp_ev[k].len;
    }
    return n;
}
static void lp_clip_clear(uint32_t t, uint32_t s)   /* (UNDO PASS's kept events gone too, should they be its) */
{
    uint32_t fl = LP_LOCK();
    t %= LP_NTRK, s %= LP_NSCN;
    if (lpr.st != LR_OFF && lpr.trk == t && lpr.scn == s) {
        lpr.st = LR_OFF;
        lpr.npass = lpr.nlast = 0;
        memset(lpr.open, 0, sizeof lpr.open);
    }
    if (lpr.nundo && lpr.utrk == t && lpr.uscn == s)
        lp_undo_drop();
    if (lpr.nlast && lpr.trk == t && lpr.scn == s)
        lpr.nlast = 0;
    lp_clip_empty(&lpp.clip[t][s]);
    if (lps[t].play == s) {
        lp_notes_off(t);
        sp_ovr_drop(t);
        lps[t].play = LP_NOSCN;
    }
    if (lps[t].next == s)
        lps[t].next = LP_NOSCN;
    LP_UNLOCK(fl);
    lpx.seq++;
}
/* a knob turned (sy_ui.c ui_knob: a parameter that can be modulated): heard while it turns (its moves let go),
 * recorded if a synth track records */
static void lp_knob(uint32_t id)
{
    if (id >= SP_COUNT || !(SP[id].flags & F_MOD) || (SP[id].flags & F_OLD))
        return;
    lpr.touch_ms = fm1_ms;
    lpr.touch = (uint16_t)id;
    if (sp_ovr_on[id]) {
        uint32_t fl = LP_LOCK();
        sp_ovr_set(id, 0xFFu, 0);
        LP_UNLOCK(fl);
    }
}

/* a track's TYPE set (V0.85; TRACK's knob): what it sounded ends the old way. Its clips stay as they are (a hit plays
 * as a sixteenth on a synth or MIDI track, a note as a hit on a drum track: a turn there and back loses nothing) */
static void lp_type_set(uint32_t t, uint32_t type, uint32_t out)
{
    lp_trk_t *k;
    uint32_t fl;
    t %= LP_NTRK;
    k = &lpp.trk[t];
    lp_out_off_all(t);
    fl = LP_LOCK();
    k->type = (uint8_t)(type % LT_N);
    k->out = (uint8_t)(out != 0u && type == LT_SYNTH);
    LP_UNLOCK(fl);
    lpx.seq++;
}

/* the drum grid (V0.85, CLIP's GRID and HIT on a drum track): the selected clip in sixteenths, a row a drum (and
 * DUCK). Row r's first hit in step s (LP_END none) */
static uint32_t lp_grid_at(const lp_clip_t *c, uint32_t row, uint32_t s)
{
    uint32_t k, t0 = s * LP_STEP, t1 = t0 + LP_STEP;
    for (k = c->head; k != LP_END && lp_et(k) < t1; k = lp_ev[k].next)
        if (!(lp_ev[k].t & LP_MOVE) && lp_et(k) >= t0 && lp_drum_row(lp_ev[k].a) == row)
            return k;
    return LP_END;
}
/* step s of row: a hit put in (velocity vel; an empty clip made one bar first, launched when its track plays
 * nothing), or what is there taken out. 1 put in, 0 taken out, -1 neither (a take recording it, past its end, full) */
static int32_t lp_grid_toggle(uint32_t row, uint32_t s, uint32_t vel)
{
    lp_clip_t *c = lp_cur_clip();
    uint32_t t = lpp.sel % LP_NTRK, fl, k;
    int32_t r;
    if (row >= DK_ROWS || (lp_cur_rec() && lpr.st == LR_TAKE))
        return -1;
    fl = LP_LOCK();
    if (!c->len) {
        c->len = (uint16_t)lp_bar_ticks();
        if (lps[t].play >= LP_NSCN && lps[t].next == LP_NOSCN)
            lps[t].next = lpp.scn;
    }
    if (s * LP_STEP >= c->len) {
        r = -1;
    } else if (lp_grid_at(c, row, s) != LP_END) {
        while ((k = lp_grid_at(c, row, s)) != LP_END) {
            lp_unlink(c, k);
            lp_forget(k);
            lp_release(k);
        }
        r = 0;
    } else if ((k = lp_alloc()) == LP_END) {
        lpr.full = 1;
        r = -1;
    } else {
        lp_ev[k].t = (uint16_t)(s * LP_STEP);
        lp_ev[k].len = LP_HIT;
        lp_ev[k].a = DK_NOTE[row];
        lp_ev[k].b = (uint8_t)sy_clampi((int32_t)vel, 1, 127);
        lp_insert(c, k);
        r = 1;
    }
    lps[t].resync = 1;
    LP_UNLOCK(fl);
    lpx.seq++;
    return r;
}
/* the hit of row in step s: f 0 VELOCITY (1 .. 127), 1 ACCENT (0, 1), 2 CHANCE (0 .. 7), 3 RATCHET (0 .. 3), moved
 * by d (0: as it is); its value (-1: no hit there) */
static int32_t lp_grid_attr(uint32_t row, uint32_t s, uint32_t f, int32_t d)
{
    lp_clip_t *c = lp_cur_clip();
    uint32_t fl = LP_LOCK(), k = lp_grid_at(c, row, s), len;
    int32_t v = -1;
    if (k != LP_END) {
        len = lp_ev[k].len & LP_HIT ? lp_ev[k].len : LP_HIT;
        if (f == 0u) {
            v = sy_clampi((lp_ev[k].b & 0x7F) + d, 1, 127);
            lp_ev[k].b = (uint8_t)((uint32_t)v | (lp_ev[k].b & LP_FRESH));
        } else if (f == 1u) {
            if (d)
                len = d > 0 ? len | LP_ACC : len & ~LP_ACC;
            v = (len & LP_ACC) != 0u;
        } else if (f == 2u) {
            v = sy_clampi((int32_t)LP_CHN(len) + d, 0, 7);
            len = (len & ~(7u << 2)) | ((uint32_t)v << 2);
        } else {
            v = sy_clampi((int32_t)LP_RAT(len) + d, 0, 3);
            len = (len & ~3u) | (uint32_t)v;
        }
        lp_ev[k].len = (uint16_t)len;
    }
    LP_UNLOCK(fl);
    if (d && k != LP_END)
        lpx.seq++;
    return v;
}

/* V0.86: clip (t, s)'s SPEED (the one playing goes on from where it is) */
static void lp_speed_set(uint32_t t, uint32_t s, uint32_t sp)
{
    uint32_t fl = LP_LOCK();
    lp_clip_t *c = &lpp.clip[t % LP_NTRK][s % LP_NSCN];
    t %= LP_NTRK, s %= LP_NSCN;
    if (cht.run && lps[t].play == s && c->len) {
        float k = (float)lp_tick_now(t, c);
        lpp.speed[t][s] = (uint8_t)(sp % LS_N);
        lps[t].start = cht.tq - k / ((float)LP_PPQ * lp_speed(t, s));
        lps[t].resync = 1;
    } else {
        lpp.speed[t][s] = (uint8_t)(sp % LS_N);
    }
    LP_UNLOCK(fl);
    lpx.seq++;
}
/* V0.86, CLIP > TOOLS: clip (st, ss) into (dt, ds), whole (bar < 0: its length and SPEED with it, what was there
 * gone) or its bar `bar` into the same bar there (that bar's notes and moves replaced). 0 done; -1 the events ran out
 * (nothing changed); -2 no such bar in one of them */
static int32_t lp_clip_copy(uint32_t st, uint32_t ss, uint32_t dt, uint32_t ds, int32_t bar)
{
    lp_clip_t *src = &lpp.clip[st % LP_NTRK][ss % LP_NSCN], *dst = &lpp.clip[dt % LP_NTRK][ds % LP_NSCN];
    uint32_t bt = lp_bar_ticks(), a = bar < 0 ? 0u : (uint32_t)bar * bt, b = bar < 0 ? src->len : a + bt, need = 0, back = 0;
    uint32_t k, fl;
    if (src == dst)
        return 0;
    if (bar >= 0 && (a >= src->len || a >= dst->len))
        return -2;
    for (k = src->head; k != LP_END; k = lp_ev[k].next)
        need += lp_et(k) >= a && lp_et(k) < b;
    for (k = dst->head; k != LP_END; k = lp_ev[k].next)
        back += bar < 0 || (lp_et(k) >= a && lp_et(k) < b);
    if (need > lp_nfree + back)
        return -1;
    fl = LP_LOCK();
    dt %= LP_NTRK, ds %= LP_NSCN;
    if (lps[dt].play == ds)
        lp_notes_off(dt);
    if (lpr.nundo && lpr.utrk == dt && lpr.uscn == ds)   /* (what UNDO PASS kept of it: gone, as its pass lists) */
        lp_undo_drop();
    if (lpr.trk == dt && lpr.scn == ds)
        lpr.npass = lpr.nlast = 0;
    if (bar < 0) {
        uint16_t len = src->len;
        lp_clip_empty(dst);
        dst->len = len;
        lpp.speed[dt][ds] = lpp.speed[st % LP_NTRK][ss % LP_NSCN];
    } else {
        uint32_t prev = LP_END;
        for (k = dst->head; k != LP_END;) {
            uint32_t nx = lp_ev[k].next;
            if (lp_et(k) >= a && lp_et(k) < b) {
                if (prev == LP_END)
                    dst->head = (uint16_t)nx;
                else
                    lp_ev[prev].next = (uint16_t)nx;
                dst->n--;
                lp_forget(k);
                lp_release(k);
            } else {
                prev = k;
            }
            k = nx;
        }
    }
    for (k = src->head; k != LP_END; k = lp_ev[k].next) {
        uint32_t e;
        if (lp_et(k) < a || lp_et(k) >= b)
            continue;
        e = lp_alloc();
        lp_ev[e] = lp_ev[k];
        if (!(lp_ev[e].t & LP_MOVE))
            lp_ev[e].b &= (uint8_t)~LP_FRESH;
        lp_insert(dst, e);
    }
    lps[dt].resync = 1;
    LP_UNLOCK(fl);
    lpx.seq++;
    return 0;
}
/* .. clip (t, s) twice as long, its notes and moves again in the second half (0; -1 the events ran out; -2 longer
 * than LP_MAXBAR bars) */
static int32_t lp_clip_double(uint32_t t, uint32_t s)
{
    lp_clip_t *c = &lpp.clip[t % LP_NTRK][s % LP_NSCN];
    uint32_t len = c->len, n = c->n, k, i, fl;
    if (!len || 2u * len > lp_max_bars() * lp_bar_ticks())
        return -2;
    if (n > lp_nfree)
        return -1;
    fl = LP_LOCK();
    for (k = c->head, i = 0; k != LP_END && i < n; k = lp_ev[k].next, i++) {   /* (the copies go in after every one */
        uint32_t e = lp_alloc();                                             /* of the first half: the walk stops at n) */
        lp_ev[e] = lp_ev[k];
        lp_ev[e].t = (uint16_t)((lp_ev[k].t & LP_MOVE) | (lp_et(k) + len));
        if (!(lp_ev[e].t & LP_MOVE))
            lp_ev[e].b &= (uint8_t)~LP_FRESH;
        lp_insert(c, e);
    }
    c->len = (uint16_t)(2u * len);
    lps[t % LP_NTRK].resync = 1;
    LP_UNLOCK(fl);
    lpx.seq++;
    return 0;
}
/* .. clip (t, s) half as long (in whole bars, one at least): its events past the new end gone; count only (do 0):
 * how many would go, -2 one bar already */
static int32_t lp_clip_halve(uint32_t t, uint32_t s, int go)
{
    lp_clip_t *c = &lpp.clip[t % LP_NTRK][s % LP_NSCN];
    uint32_t bt = lp_bar_ticks(), bars = c->len / bt, nl, k, prev = LP_END, gone = 0, fl;
    if (bars < 2u)
        return -2;
    nl = bars / 2u * bt;
    for (k = c->head; k != LP_END; k = lp_ev[k].next)
        gone += lp_et(k) >= nl;
    if (!go)
        return (int32_t)gone;
    fl = LP_LOCK();
    if (lpr.nundo && lpr.utrk == t % LP_NTRK && lpr.uscn == s % LP_NSCN)
        lp_undo_drop();
    for (k = c->head; k != LP_END;) {
        uint32_t nx = lp_ev[k].next;
        if (lp_et(k) >= nl) {
            if (prev == LP_END)
                c->head = (uint16_t)nx;
            else
                lp_ev[prev].next = (uint16_t)nx;
            c->n--;
            lp_forget(k);
            lp_release(k);
        } else {
            prev = k;
        }
        k = nx;
    }
    c->len = (uint16_t)nl;
    if (lps[t % LP_NTRK].play == s % LP_NSCN)
        lp_notes_off(t % LP_NTRK);
    lps[t % LP_NTRK].resync = 1;
    LP_UNLOCK(fl);
    lpx.seq++;
    return (int32_t)gone;
}

/* .. clip (t, s)'s notes (a drum track's hits) gone, its moves kept; how many went */
static uint32_t lp_notes_clear(uint32_t t, uint32_t s)
{
    lp_clip_t *c = &lpp.clip[t % LP_NTRK][s % LP_NSCN];
    uint32_t fl = LP_LOCK(), k, prev = LP_END, gone = 0;
    t %= LP_NTRK, s %= LP_NSCN;
    if (lpr.nundo && lpr.utrk == t && lpr.uscn == s)
        lp_undo_drop();
    for (k = c->head; k != LP_END;) {
        uint32_t nx = lp_ev[k].next;
        if (!(lp_ev[k].t & LP_MOVE)) {
            if (prev == LP_END)
                c->head = (uint16_t)nx;
            else
                lp_ev[prev].next = (uint16_t)nx;
            c->n--;
            lp_forget(k);
            lp_release(k);
            gone++;
        } else {
            prev = k;
        }
        k = nx;
    }
    if (lps[t].play == s)
        lp_notes_off(t);
    lps[t].resync = 1;
    LP_UNLOCK(fl);
    lpx.seq++;
    return gone;
}

/* ------------------------------------------------------------------------------------------ defaults --- */
static void lp_trk_defaults(uint32_t t)
{
    lp_trk_t *k = &lpp.trk[t % LP_NTRK];
    memset(k, 0, sizeof *k);
    k->type = LT_SYNTH;
    k->ch = (uint8_t)(t % LP_NTRK);
    k->cin = 0;
    k->vol = 100;
    k->pan = 64;
    k->scale = 0, k->root = 0, k->fit = 0, k->kvel = 100;
}
static void lp_song_defaults(void)
{
    uint32_t i;
    lpp.song_on = 0;
    for (i = 0; i < LP_NSONG; i++) {
        lpp.song[i] = i ? LP_NOSCN : 0u;
        lpp.srep[i] = 1;
    }
}
/* a project as it starts (the event pool's and the clips' state, every track's and SONG's; the chords' left as they
 * are: power-on's, or the project's) */
static void lp_project_clear(void)
{
    uint32_t t, s;
    lp_pool_init();
    for (t = 0; t < LP_NTRK; t++) {
        lp_trk_defaults(t);
        for (s = 0; s < LP_NSCN; s++) {
            lpp.clip[t][s].head = LP_END;
            lpp.clip[t][s].len = lpp.clip[t][s].n = lpp.clip[t][s].rsv = 0;
            lpp.speed[t][s] = LS_1;
        }
        memset(&lps[t], 0, sizeof lps[t]);
        lps[t].play = lps[t].next = LP_NOSCN;
    }
    lp_song_defaults();
    dk_defaults();                               /* (V0.85: the kit as a project starts) */
    lpp.sel = 0;
    lpp.scn = 0;
    memset(&lpr.open, 0, sizeof lpr.open);
    lpr.st = LR_OFF;
    lpr.npass = lpr.nlast = lpr.nundo = 0;
    lpx.seq++;
}
static void lp_boot(void)                       /* power-on (before the flash's project) */
{
    uint32_t i;
    lp_project_clear();
    lpr.touch = lpr.mv_id = 0xFFFFu;
    lpx.rnd = 0x9E3779B9u;                       /* (V0.85: CHANCE's dice, never 0) */
    for (i = 0; i < 27u; i++)
        lpx.keynote[i] = 0xFF;
    lpx.bar = lpx.beat = -1;
}

/* ------------------------------------------------------------------------------------------- flash --- */
/* a project as the flash keeps it (sy_app.c): a stream over LP_NOBJ objects of SS_PAYLOAD_MAX each: "LP", a version,
 * the selected track and scene, SONG; each track (16 bytes) and its chords (sy_chord.c ch_d_t); each clip's length
 * and count of events; (version 2, V0.85: the drum kit, sy_drum.c dk_kit_t; version 3, V0.86: the kit grown to its
 * V0.86 bytes, then each clip's SPEED, a byte;) then the events in clip order (time, length or parameter, two bytes: 6
 * each). Versions 1 and 2 read too: what they lack as a project starts */
#define LP_NOBJ 4u
#define LP_STREAM (LP_NOBJ * SS_PAYLOAD_MAX)
#define LP_HEAD1 (8u + 2u * LP_NSONG + LP_NTRK * (sizeof(lp_trk_t) + sizeof(ch_d_t)) + LP_NTRK * LP_NSCN * 4u)
#define LP_HEAD2 (LP_HEAD1 + DK_KIT2)
#define LP_HEAD (LP_HEAD1 + sizeof(dk_kit_t) + LP_NTRK * LP_NSCN)
_Static_assert(LP_HEAD + 6u * LP_NEV <= LP_STREAM, "a project fits its objects");
static uint8_t *lp_wb;                           /* the window being written (or read) */
static uint32_t lp_wlo, lp_wpos;                 /* .. its first byte's place in the stream; the place now */
static void lp_put(uint32_t b)
{
    if (lp_wpos >= lp_wlo && lp_wpos < lp_wlo + SS_PAYLOAD_MAX)
        lp_wb[lp_wpos - lp_wlo] = (uint8_t)b;
    lp_wpos++;
}
static void lp_put16(uint32_t v) { lp_put(v & 0xFFu); lp_put(v >> 8); }
static void lp_puts(const void *p, uint32_t n)
{
    const uint8_t *b = p;
    while (n--)
        lp_put(*b++);
}
/* object o of the project into ss_buf: its length (the stream's bytes in it; 0: none, the stream ended before it) */
static uint32_t lp_pack(uint32_t o)
{
    uint32_t t, s, k, fl;
    lp_wb = ss_buf;
    lp_wlo = o * SS_PAYLOAD_MAX;
    lp_wpos = 0;
    fl = LP_LOCK();
    lp_put('L'), lp_put('P'), lp_put(3), lp_put(lpp.sel), lp_put(lpp.scn), lp_put(lpp.song_on);
    {                                            /* (the stream's length) */
        uint32_t n = LP_HEAD, t2, s2;
        for (t2 = 0; t2 < LP_NTRK; t2++)
            for (s2 = 0; s2 < LP_NSCN; s2++)
                n += 6u * lpp.clip[t2][s2].n;
        lp_put16(n);
    }
    lp_puts(lpp.song, LP_NSONG);
    lp_puts(lpp.srep, LP_NSONG);
    for (t = 0; t < LP_NTRK; t++) {
        lp_puts(&lpp.trk[t], sizeof(lp_trk_t));
        lp_puts(&ch_d[t], sizeof(ch_d_t));
    }
    for (t = 0; t < LP_NTRK; t++)
        for (s = 0; s < LP_NSCN; s++) {
            lp_put16(lpp.clip[t][s].len);
            lp_put16(lpp.clip[t][s].n);
        }
    lp_puts(&dkk, sizeof dkk);                   /* (V0.85: the kit; V0.86 its grown bytes) */
    lp_puts(lpp.speed, sizeof lpp.speed);        /* (V0.86: the clips' SPEED) */
    for (t = 0; t < LP_NTRK; t++)
        for (s = 0; s < LP_NSCN; s++)
            for (k = lpp.clip[t][s].head; k != LP_END; k = lp_ev[k].next) {
                lp_put16(lp_ev[k].t);
                lp_put16(lp_ev[k].len);
                lp_put(lp_ev[k].a);
                lp_put(lp_ev[k].b & (uint8_t)(lp_ev[k].t & LP_MOVE ? 0xFFu : 0x7Fu));
            }
    LP_UNLOCK(fl);
    return lp_wpos <= lp_wlo ? 0u : lp_wpos - lp_wlo >= SS_PAYLOAD_MAX ? SS_PAYLOAD_MAX : lp_wpos - lp_wlo;
}
static uint32_t lp_pack_crc(uint32_t o)
{
    uint32_t n = lp_pack(o);
    return n ? ss_crc32(ss_buf, n) : 0u;
}
/* a project read back: its stream's objects in turn (lp_load_begin, lp_load(o, b, n) each, lp_load_end); a stream
 * that does not hold together: the project as it starts. 1: taken */
static uint8_t lp_tk[LP_HEAD] SY_POOL;          /* (the head, gathered: it may cross an object) */
static struct {
    uint32_t pos, t, s, left;                    /* the stream's place; the clip being read, its events still to come */
    uint32_t end;                                /* the stream's length (its head's) */
    uint32_t total, got;                         /* the events the head says, those read */
    uint32_t hlen;                               /* the head's length (its version's: LP_HEAD1, LP_HEAD) */
    uint8_t ev[6], ne;                           /* an event's bytes so far */
    uint8_t bad;
} lpt;
static void lp_load_begin(void)
{
    lp_project_clear();
    memset(&lpt, 0, sizeof lpt);
    lpt.hlen = LP_HEAD;
}
static void lp_load_head(void)                   /* the head read: the tracks, chords, SONG and the clips' sizes */
{
    const uint8_t *b = lp_tk;
    uint32_t t, s, i, cl = 8u + 2u * LP_NSONG + LP_NTRK * (sizeof(lp_trk_t) + sizeof(ch_d_t));
    lpt.end = b[6] | (uint32_t)b[7] << 8;
    if (b[0] != 'L' || b[1] != 'P' || b[2] < 1u || b[2] > 3u || lpt.end < lpt.hlen || lpt.end > LP_STREAM) {
        lpt.bad = 1;
        return;
    }
    if (b[2] >= 2u) {                            /* (V0.85: the kit, every value in its range; version 2's: the V0.85 */
        memcpy(&dkk, b + LP_HEAD1, b[2] == 2u ? DK_KIT2 : sizeof dkk);   /* part, the rest as a kit starts) */
        if (b[2] == 2u)
            dk_defaults3();
        dk_sane();
    }
    if (b[2] >= 3u)                              /* (V0.86: the clips' SPEED) */
        for (t = 0; t < LP_NTRK; t++)
            for (s = 0; s < LP_NSCN; s++)
                lpp.speed[t][s] = (uint8_t)(b[LP_HEAD1 + sizeof(dk_kit_t) + t * LP_NSCN + s] % LS_N);
    lpp.sel = (uint8_t)(b[3] % LP_NTRK);
    lpp.scn = (uint8_t)(b[4] % LP_NSCN);
    lpp.song_on = b[5] != 0;
    for (i = 0; i < LP_NSONG; i++) {
        lpp.song[i] = b[8u + i] < LP_NSCN ? b[8u + i] : LP_NOSCN;
        lpp.srep[i] = (uint8_t)sy_clampi(b[8u + LP_NSONG + i], 1, 16);
    }
    for (t = 0; t < LP_NTRK; t++) {
        const uint8_t *p = b + 8u + 2u * LP_NSONG + t * (sizeof(lp_trk_t) + sizeof(ch_d_t));
        lp_trk_t *k = &lpp.trk[t];
        memcpy(k, p, sizeof(lp_trk_t));
        k->type = (uint8_t)(k->type % LT_N), k->ch &= 15u, k->cin = (uint8_t)(k->cin > 16u ? 0u : k->cin);
        k->out = k->out != 0, k->vol = (uint8_t)(k->vol & 127u), k->pan = (uint8_t)(k->pan & 127u);
        k->mute = k->mute != 0, k->solo = k->solo != 0;
        memcpy(&ch_d[t], p + sizeof(lp_trk_t), sizeof(ch_d_t));
        ch_d[t].gen++;
    }
    for (t = 0; t < LP_NTRK; t++)
        for (s = 0; s < LP_NSCN; s++) {
            const uint8_t *p = b + cl + 4u * (t * LP_NSCN + s);
            uint32_t len = p[0] | (uint32_t)p[1] << 8, n = p[2] | (uint32_t)p[3] << 8;
            lpp.clip[t][s].len = (uint16_t)(len > LP_MAXT ? 0u : len);
            lpp.clip[t][s].rsv = (uint16_t)n;    /* (its events to come: counted down as they are read) */
            lpt.total += n;
        }
    lpt.t = lpt.s = 0;
    lpt.left = lpp.clip[0][0].rsv;
}
static void lp_load_ev(void)                     /* an event read: into its clip (the clips in order) */
{
    while (!lpt.left && lpt.t < LP_NTRK) {       /* (the next clip with events) */
        lpp.clip[lpt.t][lpt.s].rsv = 0;
        if (++lpt.s == LP_NSCN)
            lpt.s = 0, lpt.t++;
        if (lpt.t < LP_NTRK)
            lpt.left = lpp.clip[lpt.t][lpt.s].rsv;
    }
    if (lpt.t >= LP_NTRK) {
        lpt.bad = 1;
        return;
    }
    {
        lp_clip_t *c = &lpp.clip[lpt.t][lpt.s];
        uint32_t e = lp_alloc(), t = lpt.ev[0] | (uint32_t)lpt.ev[1] << 8;
        if (e == LP_END) {
            lpt.bad = 1;
            return;
        }
        lp_ev[e].t = (uint16_t)t;
        lp_ev[e].len = (uint16_t)(lpt.ev[2] | (uint32_t)lpt.ev[3] << 8);
        lp_ev[e].a = lpt.ev[4];
        lp_ev[e].b = lpt.ev[5];
        if (!(t & LP_MOVE))
            lp_ev[e].a &= 127u, lp_ev[e].b = (uint8_t)sy_clampi(lp_ev[e].b & 127, 1, 127),
            lp_ev[e].len = (uint16_t)(lp_ev[e].len ? lp_ev[e].len : 1u);
        if ((t & LP_MOVE) && (lp_ev[e].len >= SP_COUNT || !(SP[lp_ev[e].len].flags & F_MOD))) {
            lp_release(e);                       /* (a move of a parameter this build does not have) */
        } else {
            lp_insert(c, e);
        }
        lpt.left--;
        lpt.got++;
    }
}
static void lp_load(uint32_t o, const uint8_t *b, uint32_t n)
{
    uint32_t i;
    if (lpt.bad || lpt.pos != o * SS_PAYLOAD_MAX || (lpt.end && lpt.pos >= lpt.end))
        return;
    for (i = 0; i < n && !lpt.bad && (!lpt.end || lpt.pos < lpt.end); i++, lpt.pos++) {
        if (lpt.pos < lpt.hlen) {
            lp_tk[lpt.pos] = b[i];
            if (lpt.pos == 2u)                   /* (the version: its head's length) */
                lpt.hlen = b[i] == 1u ? LP_HEAD1 : b[i] == 2u ? LP_HEAD2 : LP_HEAD;
            if (lpt.pos + 1u == lpt.hlen)
                lp_load_head();
            continue;
        }
        lpt.ev[lpt.ne++] = b[i];
        if (lpt.ne == 6u) {
            lpt.ne = 0;
            lp_load_ev();
        }
    }
    if (lpt.pos < lpt.end && lpt.pos != (o + 1u) * SS_PAYLOAD_MAX)
        lpt.bad = 1;                             /* (an object shorter than the stream says) */
}
static int lp_load_end(void)
{
    uint32_t t, s;
    for (t = 0; t < LP_NTRK; t++)
        for (s = 0; s < LP_NSCN; s++)
            lpp.clip[t][s].rsv = 0;
    if (lpt.bad || lpt.pos < lpt.hlen || lpt.pos != lpt.end || lpt.got != lpt.total) {
        lp_project_clear();
        return 0;
    }
    for (t = 0; t < LP_NTRK; t++)
        for (s = 0; s < LP_NSCN; s++)
            if (!lpp.clip[t][s].len)
                lp_clip_empty(&lpp.clip[t][s]);  /* (events without a length: none) */
    return 1;
}
