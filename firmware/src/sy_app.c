/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone
 * Its audio interrupt follows Felucca's src/audio.c (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton
 * Instruments, GPL-3.0). */
/* STRATA on the FM-1: the app around the synth (docs/ARCHITECTURE.md §3).
 *   - The audio interrupt (fm1_alnk0_irq): each I2S half, 128 frames, is four engine blocks; before each block the
 *     events (notes, bend, controllers) from two rings, one the main loop fills from the panel and one MIDI fills, so
 *     only the interrupt ever touches the voices; then Q15 -> 24 bit, and the half's load into the CPU guard.
 *   - The main loop: sy_app_ms every millisecond (the keys -> notes, the UI's buttons and knobs (sy_ui.c), a step of a
 *     wavetable build), sy_app_frame about 60 times a second (the screen where it changed, the LEDs: sy_ui_draw.c).
 *     A value a knob turns is a 16-bit store into the patch: the engine's smoother takes it at its next block.
 * Nothing here writes the flash. Needs the HAL (or the emulator's), panel.c, gfx.c, the sy_* engine, sy_text.c,
 * sy_sounds.h, sy_gfx.c and the UI (sy_ui.c, sy_ui_draw.c, which call back the sy_app_* below). */

#define SY_HALF 128u                             /* an I2S half: four engine blocks */
#define SY_HALF_WORDS (SY_HALF * 2u)
#define SY_OUT_SHIFT 7                           /* Q15 -> 24 bit, a -6 dBFS ceiling (Felucca's audio.c OUT_SHIFT) */
#define SY_PANEL_VEL 100u                        /* the keys have no velocity */
#define SY_NKEY 27u                              /* F3 .. G5 */
#define SY_KEY0 53                               /* F3 */
#ifndef SY_LOAD_KNOWN
#define SY_LOAD_KNOWN 1                          /* 0: the clock does not run within a half (the emulator's) */
#endif
_Static_assert(SY_HALF % SY_BLK == 0u, "a half is whole blocks");

static int32_t sy_abuf[2u * SY_HALF_WORDS] __attribute__((aligned(4)));

/* ------------------------------------------------------------------------------------------ events --- */
/* a ring with one producer and the audio interrupt as its consumer: the panel's (main loop) and MIDI's */
enum { SY_EV_NOTE, SY_EV_BEND, SY_EV_CC, SY_EV_PANIC };
typedef struct { uint8_t type, a, b; } sy_ev_t;
#define SY_EVQ 64u
typedef struct {
    sy_ev_t q[SY_EVQ];
    volatile uint32_t w, r;
} sy_ring_t;
static sy_ring_t sy_ev_ui, sy_ev_midi;

static int sy_ev_push(sy_ring_t *g, uint32_t type, uint32_t a, uint32_t b)
{
    sy_ev_t *e;
    if (g->w - g->r >= SY_EVQ)
        return 0;
    e = &g->q[g->w % SY_EVQ];
    e->type = (uint8_t)type;
    e->a = (uint8_t)a;
    e->b = (uint8_t)b;
    g->w = g->w + 1u;                            /* (the event first, then the index the consumer reads) */
    return 1;
}

static void sy_ev_drain(sy_ring_t *g)
{
    while (g->r != g->w) {
        const sy_ev_t *e = &g->q[g->r % SY_EVQ];
        switch (e->type) {
        case SY_EV_NOTE:
            if (e->b)
                sy_note_on(e->a, e->b);
            else
                sy_note_off(e->a);
            break;
        case SY_EV_BEND: sy_bend((int32_t)(e->a | (uint32_t)e->b << 7)); break;
        case SY_EV_CC: sy_cc(e->a, e->b); break;
        default: sy_all_sound_off(); break;
        }
        g->r = g->r + 1u;
    }
}

/* ------------------------------------------------------------------------------------------- audio --- */
static uint32_t sy_load_q8;                      /* the last halves' load, 256 = all of a half (the guard's) */
static int32_t sy_app_load(void) { return SY_LOAD_KNOWN ? (int32_t)(sy_g.load_q8 * 100u / 256u) : -1; }   /* (HOME) */

void fm1_alnk0_irq(void)                         /* via isr_alnk0 (hal/fm1_isr.S), or the emulator's clock */
{
    uint8_t p = fm1_audio_pending();
    uint32_t t0 = fm1_ticks();
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half(), b, i, us;
        int32_t *o = &sy_abuf[half * SY_HALF_WORDS];
        for (b = 0; b < SY_HALF; b += SY_BLK) {
            sy_ev_drain(&sy_ev_ui);
            sy_ev_drain(&sy_ev_midi);
            sy_block(o + 2u * b);
        }
        for (i = 0; i < 2u * SY_HALF; i++)
            o[i] *= 1 << SY_OUT_SHIFT;
        fm1_audio_ack_half();
        us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
        sy_load_q8 = us * 256u / (SY_HALF * 1000000u / 44118u);
        sy_cpu(sy_load_q8);
    }
}
extern void isr_alnk0(void);

static void sy_audio_init(void)
{
    uint32_t i;
    for (i = 0; i < 2u * SY_HALF_WORDS; i++)
        sy_abuf[i] = 0;
    fm1_audio_init(sy_abuf, SY_HALF_WORDS, isr_alnk0, 3);
}

/* -------------------------------------------------------------------------------------------- MIDI --- */
/* a channel message (any channel): notes, bend and the engine's controllers into the MIDI ring; a controller a
 * parameter has as its default CC sets that parameter (a 16-bit store; the table's 0 is no CC, so CC 0, bank
 * select, sets nothing) */
static void sy_midi_in(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t t = st & 0xF0u, id;
    d1 &= 127u;
    d2 &= 127u;
    if (t == 0x90u || t == 0x80u)
        sy_ev_push(&sy_ev_midi, SY_EV_NOTE, d1, t == 0x90u ? d2 : 0u);
    else if (t == 0xE0u)
        sy_ev_push(&sy_ev_midi, SY_EV_BEND, d1, d2);
    else if (t == 0xB0u) {
        for (id = 0; id < SP_COUNT; id++)
            if (d1 && SP[id].cc == d1)
                break;
        if (id < SP_COUNT) {
            sy_patch.p[id] = sp_clamp(id, SP[id].lo + (int32_t)((SP[id].hi - SP[id].lo) * (int32_t)d2 / 127));
            sy_ui_touch();
        } else {
            sy_ev_push(&sy_ev_midi, SY_EV_CC, d1, d2);
        }
    }
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

/* the sound playing as HOME shows it: its bank, its number (from 1) and the bank's count; INIT: none (0) */
static const char *sy_app_sound_bank(uint32_t *i, uint32_t *n)
{
    *n = sizeof SOUNDS / sizeof SOUNDS[0];
    *i = sy_sound + 1u;
    if (sy_sound < *n)
        return "STARTER";
    *i = *n = 0;
    return "INIT";
}

/* PRESETS: the next or an earlier starter sound (INIT after the last) */
static void sy_app_sound_step(int32_t s)
{
    uint32_t n = sizeof SOUNDS / sizeof SOUNDS[0] + 1u;
    sy_sound_load((uint32_t)((int32_t)(sy_sound_want >= 0 ? (uint32_t)sy_sound_want : sy_sound) + s % (int32_t)n +
                             (int32_t)n) % n);
}

/* -------------------------------------------------------------------------------------------- keys --- */
static uint32_t sy_keyprev;
static uint8_t sy_keynote[SY_NKEY];             /* the note each key started (0xFF: none): released as it was */
static int32_t sy_oct;                           /* OCT- / OCT+: -3 .. +3 */

static int sy_app_held(uint32_t k) { return k < SY_NKEY && sy_keynote[k] != 0xFF; }
static int32_t sy_app_oct(void) { return sy_oct; }
static void sy_app_octave(int32_t d) { sy_oct = sy_clampi(sy_oct + d, -3, 3); }

/* the keys: a press plays (or, in REC's MOD layer, picks a source: sy_ui_key), a release ends what it started */
static void sy_keys(void)
{
    uint32_t now = fm1_in.notes & ((1u << SY_NKEY) - 1u), chg = now ^ sy_keyprev, k;
    for (k = 0; k < SY_NKEY; k++) {
        if (!((chg >> k) & 1u))
            continue;
        if ((now >> k) & 1u) {
            int32_t n = SY_KEY0 + (int32_t)k + 12 * sy_oct;
            if (sy_ui_key(k))
                continue;
            if (n >= 0 && n <= 127 && sy_ev_push(&sy_ev_ui, SY_EV_NOTE, (uint32_t)n, SY_PANEL_VEL))
                sy_keynote[k] = (uint8_t)n;
        } else if (sy_keynote[k] != 0xFF) {
            if (sy_ev_push(&sy_ev_ui, SY_EV_NOTE, sy_keynote[k], 0))
                sy_keynote[k] = 0xFF;
            else
                now |= 1u << k;                  /* (the ring full: the release again next time) */
        }
    }
    sy_keyprev = now;
}

/* --------------------------------------------------------------------------------------- main loop --- */
/* power-on: the panel, the engine with the first starter sound (no fade: nothing plays yet), the UI, the audio */
static void sy_app_boot(void)
{
    sy_patch_t p;
    uint32_t k;
    panel_init();
    sy_init();
    sy_sound_patch(0, &p);
    sy_patch = p;
    sp_snap();
    sy_tables_now();
    sy_sound = 0;
    for (k = 0; k < SY_NKEY; k++)
        sy_keynote[k] = 0xFF;
    sy_ui_boot();
    sy_audio_init();
}

/* every millisecond: the keys, the UI's buttons and knobs, a step of a wavetable build, a sound asked for while
 * one was loading */
static void sy_app_ms(void)
{
    sy_keys();
    sy_ui_ms();
    sy_service();
    if (sy_sound_want >= 0 && !sy_next_st)
        sy_sound_load((uint32_t)sy_sound_want);
}

/* a frame (~60 a second): the screen where it changed, the LEDs */
static void sy_app_frame(void) { sy_ui_frame(); }
