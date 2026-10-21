/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia
 * Its audio interrupt follows Felucca's src/audio.c (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton
 * Instruments, GPL-3.0). */
/* STRATA on the FM-1: the app around the synth (docs/ARCHITECTURE.md §3).
 *   - The audio interrupt (fm1_alnk0_irq): each I2S half, 128 frames, is four engine blocks; before each block the
 *     events (notes, bend, controllers) from two rings, one the main loop fills from the panel and one MIDI fills, so
 *     only the interrupt ever touches the voices; MASTER's level (a ramp a half), each block to the platform's tap
 *     (the USB audio input); then Q15 -> 24 bit, and the half's load into the CPU guard and BENCH (sy_bench).
 *   - The main loop: sy_app_ms every millisecond (the keys -> notes, the UI's buttons and knobs (sy_ui.c), a step of a
 *     wavetable build), sy_app_frame about 60 times a second (the screen where it changed, the LEDs: sy_ui_draw.c).
 *     A value a knob turns is a 16-bit store into the patch: the engine's smoother takes it at its next block.
 *   - The flash (sy_store.c), from the main loop only: a SAVE behind the engine's hush, the settings, the sound
 *     playing and (V0.8) the sequencer's patterns once 3 s have gone by without sound (the patterns: the transport
 *     stopped too); the user presets' index and the patterns read at power-on.
 * Needs the HAL (or the emulator's), panel.c, gfx.c, the sy_* engine, sy_text.c, sy_sounds.h, sy_gfx.c, sy_store.c
 * and the UI (sy_ui.c, sy_ui_draw.c, sy_ui_fx.c, sy_ui_save.c, which call back the sy_app_* below). */

#define SY_HALF 128u                             /* an I2S half: four engine blocks */
#define SY_HALF_WORDS (SY_HALF * 2u)
#define SY_OUT_SHIFT 7                           /* Q15 -> 24 bit, a -6 dBFS ceiling (Felucca's audio.c OUT_SHIFT) */
#define SY_NKEY 27u                              /* F3 .. G5 */
#define SY_KEY0 53                               /* F3 */
#ifndef SY_LOAD_KNOWN
#define SY_LOAD_KNOWN 1                          /* 0: the clock does not run within a half (the emulator's) */
#endif
_Static_assert(SY_HALF % SY_BLK == 0u, "a half is whole blocks");
_Static_assert(SY_HALF == 128u, "MASTER's ramp steps a half in 128ths (sy_master_blk)");
_Static_assert(SY_HALF == C2_HALF, "CORE 2's test renders a half (sy_c2.c)");

static int32_t sy_abuf[2u * SY_HALF_WORDS] __attribute__((aligned(4)));

/* the platform's hooks into the audio interrupt (the FM-1's in sy_main.c; none in the emulator): its entry and exit
 * (TIMER5 nests in it), a half's start and each block as the DAC gets it, Q15 (the USB audio input), and the TIMER4
 * ticks TIMER5 spent nested in it (taken off the load) */
#ifndef SY_ISR_ENTER
#define SY_ISR_ENTER()
#define SY_ISR_EXIT()
#define SY_HALF_START()
#define SY_BLOCK_TAP(o, n)
#define SY_NESTED_TICKS() 0u
#endif

/* MASTER: the level the platform reads from the pot, Q12 (4096: as rendered; the emulator's page sets its own and
 * leaves this), ramped over each half from the one before (no steps) */
static volatile int32_t sy_master_q12 = 4096;
static int32_t sy_master_now = 4096;


/* ------------------------------------------------------------------------------------------ events --- */
/* a ring with one producer and the audio interrupt as its consumer: the panel's (main loop) and MIDI's */
enum { SY_EV_NOTE, SY_EV_BEND, SY_EV_CC, SY_EV_AT, SY_EV_PANIC,
       SY_EV_KEY, SY_EV_MKEY,                    /* (V0.7, the chords' (sy_chord.c): a key down or up (a its note as
                                                  * SCALE fits it, 0xFF none; b the key, 0x80 down), a MIDI note as a root
                                                  * (a the note, b its velocity, 0 up)) */
       SY_EV_RUN, SY_EV_CLK };                   /* (V0.75: the transport (a 1 start, 2 continue, 0 stop), MIDI's clock) */
typedef struct { uint8_t type, a, b, c; } sy_ev_t;   /* (c: V0.8, a MIDI note's channel) */
#define SY_EVQ 64u
typedef struct {
    sy_ev_t q[SY_EVQ];
    volatile uint32_t w, r;
} sy_ring_t;
static sy_ring_t sy_ev_ui, sy_ev_midi;
static sy_ring_t sy_ev_key;                      /* the keys' notes (V0.66): from TIMER5 while the keys only play, else
                                                  * from the main loop (one of them at a time: sy_keys_isr) */
#define SY_STARTS_BLK 2u                         /* note-ons a block at most (V0.66): a chord's voices start over a few
                                                  * blocks (8 notes: 2.2 ms), no block pays for all their starts, and the
                                                  * first two measure a new sound's cost (sy_engine.c admission) */
static uint32_t sy_ev_starts;                    /* .. this block's so far */

/* the interrupts held off (V0.66: sy_hw.c's flash work, BENCH's RUN): the halves that come late after it are
 * counted apart (the audio was hushed: nothing was heard), and the event log sees the flash's work */
static volatile uint8_t sy_irq_held;             /* halves left that count as after it */
static volatile uint32_t sy_flash_ops;           /* erases and programs so far */
static volatile uint32_t sy_late_ms, sy_late_us; /* the last late half: when, its render */

static int sy_ev_push3(sy_ring_t *g, uint32_t type, uint32_t a, uint32_t b, uint32_t c)
{
    sy_ev_t *e;
    if (g->w - g->r >= SY_EVQ)
        return 0;
    e = &g->q[g->w % SY_EVQ];
    e->type = (uint8_t)type;
    e->a = (uint8_t)a;
    e->b = (uint8_t)b;
    e->c = (uint8_t)c;
    g->w = g->w + 1u;                            /* (the event first, then the index the consumer reads) */
    return 1;
}
static int sy_ev_push(sy_ring_t *g, uint32_t type, uint32_t a, uint32_t b) { return sy_ev_push3(g, type, a, b, 0xFFu); }

static void sy_ev_drain(sy_ring_t *g)
{
    while (g->r != g->w) {
        const sy_ev_t *e = &g->q[g->r % SY_EVQ];
        switch (e->type) {
        case SY_EV_NOTE:                         /* (V0.8: a note as MIDI's, to the selected track) */
            sy_loop_key(0xFFu, e->a, e->b, e->b != 0u, 0xFFu);
            break;
        case SY_EV_BEND: sy_bend((int32_t)(e->a | (uint32_t)e->b << 7)); break;
        case SY_EV_CC: sy_cc(e->a, e->b); break;
        case SY_EV_AT: sy_at = e->a; break;
        case SY_EV_KEY:                          /* (V0.8: the looper's track, its chords; recorded) */
            sy_loop_key(e->b & 127u, e->a, 0u, e->b >> 7, 0xFFu);
            break;
        case SY_EV_MKEY:
            sy_loop_key(0xFFu, e->a, e->b, e->b != 0u, e->c);
            break;
        case SY_EV_RUN: sy_chord_run(e->a); break;
        case SY_EV_CLK: sy_chord_clock(); break;
        default:
            sy_chord_reset_all();
            sy_all_sound_off();
            break;
        }
        g->r = g->r + 1u;
    }
}

/* ------------------------------------------------------------------------------------------- audio --- */
static uint32_t sy_load_q8;                      /* the last halves' load, 256 = all of a half (the guard's) */
static int32_t sy_app_load(void) { return SY_LOAD_KNOWN ? (int32_t)(sy_g.load_q8 * 100u / 256u) : -1; }   /* (HOME) */

/* MASTER on a block of the half: from the level the last half ended at to the one asked for, b its first frame */
static void sy_master_blk(int32_t *o, int32_t g0, int32_t g1, uint32_t b)
{
    uint32_t i;
    if (g0 == 4096 && g1 == 4096)
        return;
    for (i = 0; i < SY_BLK; i++) {
        int32_t g = g0 + (((g1 - g0) * (int32_t)(b + i)) >> 7);
        o[2u * i] = o[2u * i] * g >> 12;
        o[2u * i + 1u] = o[2u * i + 1u] * g >> 12;
    }
}

void fm1_alnk0_irq(void)                         /* via isr_alnk0 (hal/fm1_isr.S), or the emulator's clock */
{
    uint8_t p;
    uint32_t t0;
    SY_ISR_ENTER();
    p = fm1_audio_pending();
    t0 = fm1_ticks();
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half(), b, i, us, c2t;
        int32_t *o = &sy_abuf[half * SY_HALF_WORDS], g0 = sy_master_now, g1 = sy_master_q12;
        SY_HALF_START();
        c2_half_start();                         /* (V0.8 BENCH: CORE 2's TEST hands core 1 its work, sy_c2.c) */
        for (b = 0; b < SY_HALF; b += SY_BLK) {
            sy_ev_starts = 0;
            sy_ev_drain(&sy_ev_ui);
            sy_ev_drain(&sy_ev_key);
            sy_ev_drain(&sy_ev_midi);
            sy_loop_block();                     /* (V0.8, the looper: its clips, as keys into the tracks' chords) */
            sy_chord_block(&sy_ev_starts, SY_STARTS_BLK);   /* (the chords: their steps, their notes' starts) */
            sy_block(o + 2u * b);
            lp_click_block(o + 2u * b);          /* (the looper's click, before MASTER) */
            sy_master_blk(o + 2u * b, g0, g1, b);
            SY_BLOCK_TAP(o + 2u * b, SY_BLK);
        }
        sy_master_now = g1;
        c2t = c2_half_end(fm1_ticks() - t0 - SY_NESTED_TICKS());   /* (.. waits for it: its part not the sound's) */
        for (i = 0; i < 2u * SY_HALF; i++)
            o[i] *= 1 << SY_OUT_SHIFT;
        fm1_audio_ack_half();
        us = fm1_ticks() - t0 - SY_NESTED_TICKS();
        us = (us > c2t ? us - c2t : 0u) / FM1_TICKS_PER_US;
        sy_load_q8 = us * 256u / (SY_HALF * 1000000u / 44118u);
        sy_cpu(sy_load_q8);
        sy_cpu_now((fm1_ticks() - t0) / FM1_TICKS_PER_US * 256u / (SY_HALF * 1000000u / 44118u));
        sy_bench.half_us = us;
        if (us > sy_bench.half_max_us)
            sy_bench.half_max_us = us;
        if (fm1_audio_free_half() != half) {    /* (the DMA moved on while this half rendered) */
            if (sy_irq_held)
                sy_bench.late_held++;            /* (after the interrupts were held: hushed, unheard) */
            else
                sy_bench.late++, sy_late_ms = fm1_ms, sy_late_us = us;
        }
        if (sy_irq_held)
            sy_irq_held--;
    }
    SY_ISR_EXIT();
}
extern void isr_alnk0(void);

static void sy_audio_init(void)
{
    uint32_t i;
    for (i = 0; i < 2u * SY_HALF_WORDS; i++)
        sy_abuf[i] = 0;
    fm1_audio_init(sy_abuf, SY_HALF_WORDS, isr_alnk0, 3);
}

/* the UI's performance: the panel's bend and a controller (GLO's PERFORM), as MIDI's would come */
static void sy_app_bend(uint32_t v) { sy_ev_push(&sy_ev_ui, SY_EV_BEND, v & 127u, (v >> 7) & 127u); }
static void sy_app_cc(uint32_t cc, uint32_t v) { sy_ev_push(&sy_ev_ui, SY_EV_CC, cc & 127u, v & 127u); }
static void sy_app_run(uint32_t on) { sy_ev_push(&sy_ev_ui, SY_EV_RUN, on ? 1u : 0u, 0u); }       /* (PLAY: V0.75) */

/* -------------------------------------------------------------------------------------------- MIDI --- */
/* a channel message on the SETTINGS' channel (any): notes, bend, aftertouch and the engine's controllers into the
 * MIDI ring; a controller MIDI LEARN set, or else a parameter has as its default CC, sets that parameter (a 16-bit
 * store; the table's 0 is no CC); CC 0 is the bank (0 the factory's, else the user's) a program change loads from,
 * when PROGRAM is on (the main loop loads it) */
static volatile uint8_t sy_midi_bank;
static volatile int32_t sy_pc_want = -1;         /* a program change's preset (sy_app_entry_load's id), -1 none */
static void sy_midi_in(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t t = st & 0xF0u, id;
    int32_t l;
    d1 &= 127u;
    d2 &= 127u;
    if (t >= 0x80u && t < 0xF0u && !sy_ui_midi_ch(st & 15u) &&
        !((t == 0x90u || t == 0x80u) && lp_trk_in(st & 15u) != lpp.sel % LP_NTRK))
        return;                                  /* (V0.8: a track's MIDI IN channel always comes in) */
    if (t == 0x90u || t == 0x80u)                /* (V0.8: to the looper's track: its IN's, else the selected) */
        sy_ev_push3(&sy_ev_midi, SY_EV_MKEY, d1, t == 0x90u ? d2 : 0u, st & 15u);
    else if (t == 0xE0u)
        sy_ev_push(&sy_ev_midi, SY_EV_BEND, d1, d2);
    else if (t == 0xD0u)
        sy_ev_push(&sy_ev_midi, SY_EV_AT, d1, 0u);
    else if (t == 0xC0u && sy_ui_pc_on())
        sy_pc_want = sy_midi_bank ? (int32_t)(SY_USER + d1) : (int32_t)d1;
    else if (t == 0xB0u) {
        if (!d1) {
            sy_midi_bank = d2 != 0u;
            return;
        }
        if (sy_ui_learn_cc(d1))                  /* (LEARN listening: this CC is the picked one's now) */
            return;
        if ((l = sy_ui_learned(d1)) >= 0 && l < (int32_t)SP_COUNT)
            id = (uint32_t)l;
        else
            for (id = 0; id < SP_COUNT; id++)
                if (SP[id].cc == d1)
                    break;
        if (id < SP_COUNT) {
            sy_patch.p[id] = sp_clamp(id, SP[id].lo + (int32_t)((SP[id].hi - SP[id].lo) * (int32_t)d2 / 127));
            sy_ui_touch();
        } else {
            sy_ev_push(&sy_ev_midi, SY_EV_CC, d1, d2);
        }
    }
}

/* V0.75: MIDI's clock, start, continue and stop (realtime bytes, with the time each came: usb.c's stamp), taken with
 * CLOCK IN MIDI (GLOBAL) alone: the tempo from the clocks' times (24 a quarter note; over the last 48, two quarter
 * notes; a gap over 250 ms counts again), each clock to the audio side (the beat's place), start and stop the
 * transport (PLAY's state too). sy_app_clock_lost: the clock gone a second (or CLOCK IN off), once */
#define SY_CLK_N 49u
static uint32_t sy_clk_ms[SY_CLK_N], sy_clk_n;   /* the last clocks' times; how many in a row */
static uint8_t sy_clk_follow;                    /* the tempo is MIDI's clock's */
static void sy_midi_rt(uint32_t b, uint32_t ms)
{
    if (!ch_clock)
        return;
    if (b == 0xF8u) {
        if (sy_clk_n && ms - sy_clk_ms[(sy_clk_n - 1u) % SY_CLK_N] > 250u)
            sy_clk_n = 0;
        sy_clk_ms[sy_clk_n % SY_CLK_N] = ms;
        sy_clk_n++;
        if (sy_clk_n >= 25u) {                   /* (a quarter note's clocks at least) */
            uint32_t m = sy_clk_n > SY_CLK_N ? SY_CLK_N - 1u : sy_clk_n - 1u;
            uint32_t span = ms - sy_clk_ms[(sy_clk_n - 1u - m) % SY_CLK_N];
            if (span)
                sy_tempo = sy_clampf(2500.0f * (float)m / (float)span, 20.0f, 400.0f);   /* (quarter notes a minute) */
            sy_clk_follow = 1;
        }
        sy_ev_push(&sy_ev_midi, SY_EV_CLK, 0u, 0u);
    } else if (b == 0xFAu || b == 0xFBu || b == 0xFCu) {
        sy_ev_push(&sy_ev_midi, SY_EV_RUN, b == 0xFAu ? 1u : b == 0xFBu ? 2u : 0u, 0u);
        sy_ui_run(b != 0xFCu, b == 0xFAu);
    }
}
static int sy_app_clock(void) { return sy_clk_follow; }
static int sy_app_clock_lost(void)
{
    if (!sy_clk_follow || (ch_clock && fm1_ms - sy_clk_ms[(sy_clk_n - 1u) % SY_CLK_N] <= 1000u))
        return 0;
    sy_clk_follow = 0;
    sy_clk_n = 0;
    return 1;
}

/* --------------------------------------------------------------------------------------- the sound --- */
static uint32_t sy_sound;                        /* the starter sound playing (sy_sounds.h; past the last: INIT) */
static int32_t sy_sound_want = -1;               /* one asked for while a load was still fading */

static void sy_sound_patch(uint32_t k, sy_patch_t *p)
{
    uint32_t i, n;
    sp_defaults(p);
    if (k >= sizeof SOUNDS / sizeof SOUNDS[0])
        return;
    for (i = 0; SOUNDS[k].kv[i][0] >= 0; i++)
        p->p[SOUNDS[k].kv[i][0]] = sp_clamp((uint32_t)SOUNDS[k].kv[i][0], SOUNDS[k].kv[i][1]);
    sp_chain_from_old(p);                        /* (their effects as the six were: those ON, the chain) */
    p->cat = SOUNDS[k].cat;
    for (n = 0; n < 16u && SOUNDS[k].name[n]; n++)
        p->name[n] = (char)(SOUNDS[k].name[n] >= 'a' && SOUNDS[k].name[n] <= 'z' ? SOUNDS[k].name[n] - 32
                                                                                  : SOUNDS[k].name[n]);
    p->name[n] = 0;
}

/* a starter sound, faded in as a preset load is (sy_patch_load); with a load in flight, after it */
static void sy_sound_load(uint32_t k)
{
    sy_patch_t p;
    if (sy_next_st) {                            /* (sy_patch_load would wait for the audio) */
        sy_sound_want = (int32_t)k;
        return;
    }
    sy_sound_patch(k, &p);
    ui_hist_push();                              /* (SAVE + OCT- takes the sound before back) */
    ui.edit_t = 0;
    sy_patch_load(&p);
    sy_sound = k;
    sy_sound_want = -1;
    sy_ui_touch();
}

/* ------------------------------------------------------------------------------------- the presets --- */
/* the presets the browser and PRESETS go through: the factory's (the starter sounds and INIT, until Phase 5's) then the
 * user's slots that hold one (sy_store.c), an id each: k (factory) or SY_USER + slot. The user slots' names and
 * categories are read once at power-on (and kept as saves change them) */
static struct {
    char name[17];
    uint8_t cat, used;
} sy_uidx[SS_NUSER];
static int16_t sy_slot = -1;                     /* the user slot the sound playing came from or went to (-1 none) */

static void sy_uidx_set(uint32_t s, const uint8_t *pk, uint32_t len)
{
    sy_patch_t p;
    sy_uidx[s].used = len && sp_unpack(&p, pk, len);
    if (!sy_uidx[s].used)
        return;
    memcpy(sy_uidx[s].name, p.name, sizeof sy_uidx[s].name);
    sy_uidx[s].cat = p.cat;
}
static void sy_uidx_scan(void)                   /* (power-on: each pair read once, its five slots from it) */
{
    uint32_t pr, k;
    memset(sy_uidx, 0, sizeof sy_uidx);
    for (pr = 0; pr < SS_NPAIRS; pr++) {
        int n = ss_load(SS_USER0 + pr, ss_buf, SS_PAYLOAD_MAX);
        for (k = 0; n > 0 && k < SS_SLOTS && pr * SS_SLOTS + k < SS_NUSER; k++) {
            uint32_t at = k * SS_SLOT_BYTES, len = (uint32_t)ss_buf[at] | (uint32_t)ss_buf[at + 1u] << 8;
            if (len && len <= SS_SLOT_BYTES - 2u && at + 2u + len <= (uint32_t)n)
                sy_uidx_set(pr * SS_SLOTS + k, ss_buf + at + 2u, len);
        }
    }
}

/* a preset's name and category (0: none such) */
static const char *sy_app_entry(uint32_t id, uint8_t *cat)
{
    static const char INIT[] = "INIT";
    if (id >= SY_USER)
        return id - SY_USER < SS_NUSER && sy_uidx[id - SY_USER].used ? (*cat = sy_uidx[id - SY_USER].cat,
                                                                          sy_uidx[id - SY_USER].name) : 0;
    if (id + 1u < SY_NFACT) {
        *cat = SOUNDS[id].cat;
        return SOUNDS[id].name;
    }
    *cat = CAT_OTHER;
    return id + 1u == SY_NFACT ? INIT : 0;
}

/* the presets of a bank (0 all, 1 factory, 2 user) and category (0 all, else CAT_* + 1), in order, into ids: their
 * count */
static uint32_t sy_app_list(uint32_t bank, uint32_t cat, uint16_t *ids)
{
    uint32_t k, n = 0;
    uint8_t c;
    for (k = 0; bank != 2u && k < SY_NFACT; k++)
        if (sy_app_entry(k, &c) && (!cat || c + 1u == cat))
            ids[n++] = (uint16_t)k;
    for (k = 0; bank != 1u && k < SS_NUSER; k++)
        if (sy_uidx[k].used && (!cat || sy_uidx[k].cat + 1u == cat))
            ids[n++] = (uint16_t)(SY_USER + k);
    return n;
}
static uint32_t sy_app_entry_now(void) { return sy_slot >= 0 ? SY_USER + (uint32_t)sy_slot : sy_sound; }
static void sy_app_entry_mark(uint32_t id)       /* undo, redo: the sound is preset id again (its values: the history's) */
{
    if (id >= SY_USER && id - SY_USER < SS_NUSER) {
        sy_slot = (int16_t)(id - SY_USER);
        return;
    }
    sy_slot = -1;
    sy_sound = id < SY_NFACT ? id : 0u;
}
static int32_t sy_app_slot(void) { return sy_slot; }
static const char *sy_app_slot_name(uint32_t s) { return s < SS_NUSER && sy_uidx[s].used ? sy_uidx[s].name : 0; }
static int32_t sy_app_slot_free(void)            /* the first empty slot, -1 all full */
{
    uint32_t s;
    for (s = 0; s < SS_NUSER; s++)
        if (!sy_uidx[s].used)
            return (int32_t)s;
    return -1;
}

/* a preset loaded, faded in as any (sy_patch_load); with a load in flight, after it */
static void sy_app_entry_load(uint32_t id)
{
    uint8_t pk[SP_PACK_MAX];
    sy_patch_t p;
    int n;
    if (id < SY_USER) {
        sy_sound_load(id);
        sy_slot = -1;
        return;
    }
    if (sy_next_st || (n = ss_user_get(id - SY_USER, pk, sizeof pk)) <= 0 || !sp_unpack(&p, pk, (uint32_t)n))
        return;
    ui_hist_push();
    ui.edit_t = 0;
    sy_patch_load(&p);
    sy_slot = (int16_t)(id - SY_USER);
    sy_ui_touch();
}

/* the sound playing as HOME shows it: its bank, its number (from 1) and the bank's count (none for the user's) */
static const char *sy_app_sound_bank(uint32_t *i, uint32_t *n)
{
    *n = SY_NFACT - 1u;
    *i = sy_sound + 1u;
    if (sy_slot >= 0) {
        *i = (uint32_t)sy_slot + 1u;
        *n = 0;
        return "USER";
    }
    if (sy_sound + 1u < SY_NFACT)
        return "STARTER";
    *i = *n = 0;
    return "INIT";
}

/* PRESETS: the next or an earlier preset, factory then user, round */
static void sy_app_sound_step(int32_t s)
{
    uint16_t ids[SY_NFACT + SS_NUSER];
    uint32_t n = sy_app_list(0, 0, ids), k, now = sy_sound_want >= 0 ? (uint32_t)sy_sound_want : sy_app_entry_now();
    for (k = 0; k < n && ids[k] != now; k++)
        ;
    if (k == n)
        k = 0;
    sy_app_entry_load(ids[(uint32_t)(((int32_t)k + s % (int32_t)n + (int32_t)n) % (int32_t)n)]);
}

/* -------------------------------------------------------------------------------------------- flash --- */
/* the main loop's flash work (sy_store.c). A SAVE: the audio hushed first (an erase holds the audio's interrupt off
 * for ~17 ms, a page's program for up to 3 ms: storage_hw.c; the DMA loops the ring meanwhile, so the engine fades
 * out over 5.8 ms and says hushed once the whole ring is silent; back in over 5.8 ms), then the slot written. The
 * settings and the sound playing (back at power-on): only after 3 s without sound (the ring under -70 dBFS, nothing
 * to hush), when they differ from what the flash has */
#define SY_QUIET_MS 3000u
enum { SY_FL_SAVE = 1, SY_FL_DEL, SY_FL_BACK };   /* a slot's write: a SAVE, a delete, a delete put back */
static struct {
    uint8_t busy;                                /* a write waiting for the hush (SY_FL_*) */
    uint16_t slot;
    uint8_t pk[SP_PACK_MAX];                     /* the patch to write; after a delete, the one deleted (put back) */
    uint32_t len, quiet_t, check_t, edit_crc, set_crc;
    uint32_t prj_crc[LP_NOBJ];                   /* V0.8: the project's objects as the flash has them */
    int8_t prj_want;                             /* .. a project to load (-1 none): the one loaded saved first */
    uint8_t prj_told;                            /* .. its sectors hold older presets: said once */
} sy_fl;
static void sy_ui_project(int ok, int blocked);  /* sy_ui.c: a project loaded (or not); its place in the way */
static void sy_ui_deleted(int rc, uint32_t s);   /* sy_ui_save.c: a delete or its putting back done */
static void sy_ui_restored(int rc, uint32_t s);

/* a SAVE asked for: the sound playing into user slot s, named and filed so; 0 queued (sy_ui_saved tells the end) */
static int sy_app_save(uint32_t s, const char *name, uint32_t cat)
{
    uint32_t i;
    if (sy_fl.busy || s >= SS_NUSER || sy_next_st)
        return -1;
    for (i = 0; i < 16u && name[i]; i++)
        sy_patch.name[i] = name[i];
    sy_patch.name[i] = 0;
    sy_patch.cat = (uint8_t)(cat < CAT_COUNT ? cat : CAT_OTHER);
    sy_fl.len = sp_pack(&sy_patch, sy_fl.pk);
    sy_fl.slot = (uint16_t)s;
    sy_fl.busy = SY_FL_SAVE;
    sy_hush_req = 1;
    ui.undel = 0;                                /* (the bytes of a delete gone with it) */
    return 0;
}
/* user slot s emptied (its bytes kept: sy_app_undelete puts them back); 0 queued (sy_ui_deleted tells the end) */
static int sy_app_delete(uint32_t s)
{
    int n;
    if (sy_fl.busy || s >= SS_NUSER || !sy_uidx[s].used || (n = ss_user_get(s, sy_fl.pk, sizeof sy_fl.pk)) <= 0)
        return -1;
    sy_fl.len = (uint32_t)n;
    sy_fl.slot = (uint16_t)s;
    sy_fl.busy = SY_FL_DEL;
    sy_hush_req = 1;
    return 0;
}
/* the preset deleted last back in its slot (taken meanwhile: the first free one); 0 queued (sy_ui_restored) */
static int sy_app_undelete(void)
{
    int32_t s = sy_fl.slot;
    if (sy_fl.busy || !sy_fl.len)
        return -1;
    if (s >= (int32_t)SS_NUSER || sy_uidx[s].used)
        s = sy_app_slot_free();
    if (s < 0)
        return -1;
    sy_fl.slot = (uint16_t)s;
    sy_fl.busy = SY_FL_BACK;
    sy_hush_req = 1;
    return 0;
}

/* the edit buffer's bytes: the user slot (2), the starter sound (1), the packed patch */
static uint32_t sy_edit_pack(uint8_t *b)
{
    b[0] = (uint8_t)sy_slot;
    b[1] = (uint8_t)((uint16_t)sy_slot >> 8);
    b[2] = (uint8_t)sy_sound;
    return 3u + sp_pack(&sy_patch, b + 3);
}

/* V0.8, the looper's project (sy_loop.c): its objects that changed saved (one: the first that did; else all of them),
 * unless older presets hold its sectors (said once). 0: nothing left to save */
static int sy_prj_save(int one)
{
    uint32_t o, left = 0;
    lp_scale_keep();
    for (o = 0; o < LP_NOBJ; o++) {
        uint32_t n = lp_pack(o), c = n ? ss_crc32(ss_buf, n) : 0u, obj = SS_PRJ0 + lpg.prj % LP_NPRJ * SS_PRJ_OBJS + o;
        if (c == sy_fl.prj_crc[o])
            continue;
        if (!n) {                                /* (past the stream's end: what is there is never read) */
            sy_fl.prj_crc[o] = c;
            continue;
        }
        if (ss_foreign(obj)) {
            if (!sy_fl.prj_told)
                sy_ui_project(1, 1);
            sy_fl.prj_told = 1;
            ui_prj_state[lpg.prj % LP_NPRJ] = 2;
            return 0;
        }
        if (!ss_save(obj, ss_buf, n)) {
            sy_fl.prj_crc[o] = c;
            ui_prj_state[lpg.prj % LP_NPRJ] = 1;
        } else {
            left = 1;
        }
        if (one)
            return 1;
    }
    return (int)left;
}
/* the project p as the flash keeps it loaded (none: a new one); the crcs as read (nothing written for it). 1: read */
static int sy_prj_load(uint32_t p)
{
    uint32_t o;
    int ok;
    lpg.prj = (uint8_t)(p % LP_NPRJ);
    lp_load_begin();
    for (o = 0; o < LP_NOBJ; o++) {
        int m = ss_load(SS_PRJ0 + lpg.prj * SS_PRJ_OBJS + o, ss_buf, SS_PAYLOAD_MAX);
        if (m > 0)
            lp_load(o, ss_buf, (uint32_t)m);
    }
    ok = lp_load_end();
    for (o = 0; o < LP_NOBJ; o++) {
        uint32_t n = lp_pack(o);
        sy_fl.prj_crc[o] = n ? ss_crc32(ss_buf, n) : 0u;
    }
    sy_fl.prj_told = 0;
    return ok;
}
static void sy_prj_scan(void)                    /* each project kept, empty, or where older presets are */
{
    uint32_t p;
    for (p = 0; p < LP_NPRJ; p++) {
        ss_hdr_t h;
        uint32_t obj = SS_PRJ0 + p * SS_PRJ_OBJS;
        ui_prj_state[p] = (uint8_t)(ss_current(obj, &h) >= 0 ? 1u : ss_foreign(obj) ? 2u : 0u);
    }
}
/* PROJECT's LOAD (the UI's): the transport stopped, then (hushed) the one loaded saved and p loaded; 1: under way */
static int sy_app_project(uint32_t p)
{
    if (p >= LP_NPRJ || sy_fl.busy || sy_fl.prj_want >= 0)
        return 0;
    if (cht.run)
        sy_app_run(0);
    sy_fl.prj_want = (int8_t)p;
    return 1;
}
static void sy_app_project_new(void)             /* PROJECT's NEW: the one loaded cleared (its tracks, chords, clips) */
{
    uint32_t t;
    if (cht.run)
        sy_app_run(0);
    lp_project_clear();
    for (t = 0; t < CH_NTRK; t++) {
        ch_use(t);
        ch_defaults();
    }
    ch_use(0);
    sy_ui_project(1, 0);
}

static void sy_flash_ms(void)
{
    if (sy_fl.prj_want >= 0 && !sy_fl.busy && !cht.run && lpr.st == LR_OFF) {   /* (PROJECT's LOAD: hushed, the one */
        sy_hush_req = 1;                                                         /* loaded saved, then the next read) */
        if (sy_hushed) {
            int ok = !sy_prj_save(0) || ui_prj_state[lpg.prj % LP_NPRJ] == 2u;
            if (ok)
                sy_prj_load((uint32_t)sy_fl.prj_want);
            sy_fl.prj_want = -1;
            sy_hush_req = 0;
            sy_ui_project(ok, 0);
        }
        return;
    }
    if (sy_fl.busy && sy_hushed) {               /* a SAVE, a delete, its putting back: hushed, the slot written, */
        uint32_t job = sy_fl.busy;               /* back */
        int rc = ss_user_put(sy_fl.slot, sy_fl.pk, job == SY_FL_DEL ? 0u : sy_fl.len);
        sy_hush_req = 0;
        sy_fl.busy = 0;
        if (job == SY_FL_DEL) {
            if (!rc) {
                sy_uidx[sy_fl.slot].used = 0;
                if (sy_slot == (int16_t)sy_fl.slot)
                    sy_slot = -1;                /* (the sound plays on, from no slot) */
            }
            sy_ui_deleted(rc, sy_fl.slot);
        } else if (job == SY_FL_BACK) {
            if (!rc) {
                sy_uidx_set(sy_fl.slot, sy_fl.pk, sy_fl.len);
                sy_fl.len = 0;
            }
            sy_ui_restored(rc, sy_fl.slot);
        } else {
            if (!rc) {
                sy_uidx_set(sy_fl.slot, sy_fl.pk, sy_fl.len);
                sy_slot = (int16_t)sy_fl.slot;
            }
            sy_ui_saved(rc, sy_fl.slot);
        }
    }
    if (ui_sounding())
        sy_fl.quiet_t = fm1_ms;
    if (fm1_ms - sy_fl.check_t < 500u || fm1_ms - sy_fl.quiet_t < SY_QUIET_MS || sy_fl.busy || sy_next_st)
        return;
    sy_fl.check_t = fm1_ms;
    {                                            /* (quiet: the settings, the sound, when changed) */
        uint8_t b[SS_SETTINGS_LEN + SP_PACK_MAX + 3u];
        uint32_t n = sy_ui_settings(b, 0), c = ss_crc32(b, n);
        if (c != sy_fl.set_crc && !ss_save(SS_SETTINGS, b, n))
            sy_fl.set_crc = c;
        n = sy_edit_pack(b);
        c = ss_crc32(b, n);
        if (c != sy_fl.edit_crc && !ss_save(SS_EDIT, b, n))
            sy_fl.edit_crc = c;
    }
    if (!cht.run && lpr.st == LR_OFF)            /* (V0.8: the project's objects changed, one a check: stopped, so */
        sy_prj_save(1);                          /* nothing records meanwhile) */
}

/* power-on: the user slots' index; the settings; the sound that was playing (unless HOME is held, or the platform
 * says the boot before this one died, sy_boot_safe: a way back to a start, should a sound ever stop the unit) */
static uint8_t sy_boot_safe;                     /* the platform's: a crash, a hang or a failed boot came before */
static uint8_t sy_boot_skipped;                  /* .. so the sound was not restored (the UI says so) */
static void sy_flash_boot(void)
{
    uint8_t b[SS_SETTINGS_LEN + SP_PACK_MAX + 3u];
    sy_patch_t p;
    int n;
    memset(&sy_fl, 0, sizeof sy_fl);
    sy_fl.prj_want = -1;
    sy_uidx_scan();
    if ((n = ss_load(SS_SETTINGS, b, sizeof b)) > 0 && sy_ui_settings(b, (uint32_t)n))
        sy_fl.set_crc = ss_crc32(b, (uint32_t)n);
    sy_prj_scan();                               /* (V0.8: the project the settings name; none kept: a new one, */
    sy_ui_project(sy_prj_load(lpg.prj) ? 1 : 2, 0);   /* written once changed, its tracks the settings' SCALE) */
    if (sy_boot_safe || (fm1_in.buttons & (1u << panel.btn[B_HOME]))) {
        sy_boot_skipped = 1;
        return;
    }
    n = ss_load(SS_EDIT, b, sizeof b);
    if (n > 3 && sp_unpack(&p, b + 3, (uint32_t)n - 3u)) {
        sy_slot = (int16_t)(b[0] | b[1] << 8);
        if (sy_slot < -1 || sy_slot >= (int16_t)SS_NUSER || (sy_slot >= 0 && !sy_uidx[sy_slot].used))
            sy_slot = -1;
        sy_sound = b[2] < SY_NFACT ? b[2] : 0u;
        sy_patch = p;
        sy_fl.edit_crc = ss_crc32(b, (uint32_t)n);
    }
}

/* -------------------------------------------------------------------------------------------- keys --- */
_Static_assert(SY_KEY0 == UI_KEY0, "sy_ui.c's keys start where these do");
static uint32_t sy_keyprev;
static int32_t sy_key_note = -1, sy_key_vel;     /* the last note a key started, its velocity (the tests) */
static uint8_t sy_keynote[SY_NKEY];             /* the note each key started (0xFF: none; 0xFE: a key the chords
                                                  * took, V0.7): released as it was */
static int32_t sy_oct;                           /* OCT- / OCT+: -3 .. +3 */

static int sy_app_held(uint32_t k) { return k < SY_NKEY && sy_keynote[k] != 0xFF; }
static int32_t sy_app_oct(void) { return sy_oct; }
static void sy_app_octave(int32_t d) { sy_oct = sy_clampi(sy_oct + d, -3, 3); }

/* the keys: a press plays (or, in REC's MOD layer, picks a source: sy_ui_key), a release ends what it started.
 * V0.66: while the keys only play (no layer, no screen that types, SCL not held: sy_ui_keys_plain), TIMER5 plays them
 * at 10 kHz (sy_keys_tick), whatever the main loop is drawing or saving; else the main loop does (sy_keys), the UI
 * first. sy_keys_isr says which, set by the main loop every millisecond (SY_KEYS_ISR: the platform's; 0, the main
 * loop always). Their notes go into one ring either way (sy_ev_key), from one of them at a time: the main loop sets
 * the flag only between its own readings, and a TIMER5 tick runs to its end before the main loop goes on */
#ifndef SY_KEYS_ISR
#define SY_KEYS_ISR 0
#endif
static volatile uint8_t sy_keys_isr;
/* V0.75: the chords just took the keys (PERFORM on, say, a chord held): the keys held as plain notes go over to them,
 * by whichever reads the keys now (sy_ev_key has one producer at a time) */
static volatile uint8_t sy_keys_hand;
static void sy_app_keys_hand(void) { sy_keys_hand = 1; }
static void sy_keys_handover(void)
{
    uint32_t k;
    if (!sy_keys_hand)
        return;
    sy_keys_hand = 0;
    for (k = 0; k < SY_NKEY; k++)
        if (sy_keynote[k] < 0xFEu && sy_ev_push(&sy_ev_key, SY_EV_NOTE, sy_keynote[k], 0))
            sy_keynote[k] = sy_ev_push(&sy_ev_key, SY_EV_KEY, sy_keynote[k], k | 0x80u) ? 0xFEu : 0xFFu;
}
static void sy_key_up(uint32_t k, uint32_t *now)        /* a key let go: the note it started ends */
{
    if (sy_keynote[k] == 0xFF)
        return;
    if (sy_keynote[k] == 0xFE ? sy_ev_push(&sy_ev_key, SY_EV_KEY, 0xFFu, k)
                              : sy_ev_push(&sy_ev_key, SY_EV_NOTE, sy_keynote[k], 0))
        sy_keynote[k] = 0xFF;
    else
        *now |= 1u << k;                     /* (the ring full: the release again next time) */
}
static void sy_key_down(uint32_t k, int32_t n, uint32_t v)
{
    if (sy_ev_push(&sy_ev_key, SY_EV_KEY, n >= 0 && n <= 127 ? (uint32_t)n : 0xFFu, k | 0x80u)) {
        sy_keynote[k] = 0xFE;                    /* (V0.8: every key to the selected track, through its chords) */
        sy_key_note = n;
        sy_key_vel = (int32_t)v;
    }
}
static void sy_keys(void)                        /* main loop */
{
    uint32_t now = fm1_in.notes & ((1u << SY_NKEY) - 1u), chg = now ^ sy_keyprev, k;
    if (sy_keys_isr)
        return;
    sy_keys_handover();
    for (k = 0; k < SY_NKEY; k++) {
        if (!((chg >> k) & 1u))
            continue;
        if ((now >> k) & 1u) {
            int32_t n = sy_ui_key_note(k, sy_oct);  /* (SCL's fit; -1 silent) */
            uint32_t v = sy_ui_key_vel();
            if (!sy_ui_key(k))
                sy_key_down(k, n, v);
        } else {
            sy_key_up(k, &now);
        }
    }
    sy_keyprev = now;
}
static void sy_keys_tick(void)                   /* TIMER5 (the emulator: its millisecond), the keys only playing */
{
    uint32_t now, chg, k;
    if (!sy_keys_isr)
        return;
    sy_keys_handover();
    now = fm1_in.notes & ((1u << SY_NKEY) - 1u);
    chg = now ^ sy_keyprev;
    for (k = 0; chg && k < SY_NKEY; k++)
        if ((chg >> k) & 1u) {
            if ((now >> k) & 1u)
                sy_key_down(k, sy_ui_key_note(k, sy_oct), sy_ui_key_vel());
            else
                sy_key_up(k, &now);
        }
    sy_keyprev = now;
}

/* --------------------------------------------------------------------------------------- main loop --- */
/* power-on: the panel, the engine with the first starter sound (no fade: nothing plays yet), the UI, then what the
 * flash keeps (the user presets' index, the settings, the sound that was playing), the audio. (Not inlined: its two
 * patches and the settings' buffer, ~2.6 KB, would stay in fm1_main's frame, under every frame the loop draws) */
static SY_NOINLINE void sy_app_boot(void)
{
    sy_patch_t p;
    uint32_t k;
    panel_init();
    sy_init();
    sy_sound_patch(0, &p);
    sy_patch = p;
    sy_sound = 0;
    sy_slot = -1;
    sy_ui_boot();
    sy_flash_boot();
    c2_boot(sy_boot_skipped);                    /* (V0.8 BENCH: core 1 started when CORE 2 is ON, before the audio) */
    if (sy_boot_skipped)
        sy_ui_safe(sy_boot_safe ? 1u : 2u);         /* (SAFE START's screen) */
    sp_snap();
    lfo_dr_seq = 1u;                             /* (the sound's drawn shapes: taken at the first block) */
    sy_tables_now();
    for (k = 0; k < SY_NKEY; k++)
        sy_keynote[k] = 0xFF;
    sy_chord_reset_all();
    sy_audio_init();
}

/* every millisecond: the keys, the UI's buttons and knobs, a step of a wavetable build, a sound asked for while
 * one was loading, the flash's work */
static void sy_app_ms(void)
{
    sy_keys_isr = (uint8_t)(SY_KEYS_ISR && sy_ui_keys_plain());
    sy_keys();
    sy_ui_ms();
    sy_service();
    if (sy_sound_want >= 0 && !sy_next_st)
        sy_sound_load((uint32_t)sy_sound_want);
    if (sy_pc_want >= 0 && !sy_next_st) {        /* a program change: a starter sound, INIT or a user slot that holds one */
        uint32_t id = (uint32_t)sy_pc_want;
        sy_pc_want = -1;
        if (id < SY_NFACT || (id >= SY_USER && id - SY_USER < SS_NUSER && sy_uidx[id - SY_USER].used))
            sy_app_entry_load(id);
    }
    sy_flash_ms();
}

/* a frame (~60 a second): the screen where it changed, the LEDs */
static void sy_app_frame(void) { sy_ui_frame(); }
