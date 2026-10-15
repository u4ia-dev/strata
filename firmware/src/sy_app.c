/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone
 * Its audio interrupt follows Felucca's src/audio.c (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton
 * Instruments, GPL-3.0). */
/* STRATA on the FM-1: the app around the synth (docs/ARCHITECTURE.md §3).
 *   - The audio interrupt (fm1_alnk0_irq): each I2S half, 128 frames, is four engine blocks; before each block the
 *     events (notes, bend, controllers) from two rings, one the main loop fills from the panel and one MIDI fills, so
 *     only the interrupt ever touches the voices; then Q15 -> 24 bit, and the half's load into the CPU guard.
 *   - The main loop: sy_app_ms every millisecond (the panel: keys -> notes, the four knobs on the page, SELECT the
 *     page, PRESETS the sound, the buttons by family, OCT- / OCT+; and a step of a wavetable build), sy_app_frame
 *     about 60 times a second (the LEDs, the screen when something changed). A value a knob turns is a 16-bit store
 *     into the patch: the engine's smoother takes it at its next block.
 * Phase 3: a skeleton that plays in the emulator and on the host; its pages are the parameter table four at a
 * time. The UI proper (layers, the menu, MIDI learn, presets in flash) is Phase 4's, the device's bring-up Phase 6's.
 * Nothing here writes the flash. Needs the HAL (or the emulator's), panel.c, gfx.c, the sy_* engine, sy_text.c and
 * sy_sounds.h. */

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
static uint32_t sy_load_q8;                      /* the last halves' load, 256 = all of a half (shown, the guard's) */

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
static uint8_t sy_dirty;
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
            sy_dirty = 1;
        } else {
            sy_ev_push(&sy_ev_midi, SY_EV_CC, d1, d2);
        }
    }
}

/* ------------------------------------------------------------------------------------------- pages --- */
/* the parameter table, section by section, four to a page; a button shows its family's sections in turn */
#define SY_PGMAX 64u
typedef struct {
    uint8_t sec, n, of, id[4];                   /* section, parameters, its page's number in the section (1..) */
} sy_page_t;
static sy_page_t sy_pg[SY_PGMAX];
static uint8_t sy_secpages[SEC_COUNT];
static uint32_t sy_npg, sy_page;

static void sy_pages(void)
{
    uint32_t s, id;
    sy_npg = 0;
    for (s = 0; s < SEC_COUNT; s++) {
        sy_secpages[s] = 0;
        for (id = 0; id < SP_COUNT; id++)
            if (SP[id].sec == s) {
                if (!sy_secpages[s] || sy_pg[sy_npg - 1u].n == 4u) {
                    if (sy_npg == SY_PGMAX)
                        return;
                    sy_pg[sy_npg].sec = (uint8_t)s;
                    sy_pg[sy_npg].n = 0;
                    sy_pg[sy_npg].of = ++sy_secpages[s];
                    sy_npg++;
                }
                sy_pg[sy_npg - 1u].id[sy_pg[sy_npg - 1u].n++] = (uint8_t)id;
            }
    }
}

static const uint8_t SY_FAM_SECS[][7] = {         /* the sections of each button's family (0xFF: the end) */
    {SEC_MACRO, 0xFF},                                                  /* HOME */
    {SEC_OSCA, SEC_OSCB, SEC_SUB, SEC_NOISE, SEC_FILTER, 0xFF},        /* EDIT */
    {SEC_ENV1, SEC_ENV2, 0xFF},                                         /* ENV */
    {SEC_LFO1, SEC_LFO2, 0xFF},                                         /* LFO */
    {SEC_MATRIX, 0xFF},                                                 /* SEQ */
    {SEC_DRIVE, SEC_CHORUS, SEC_DELAY, SEC_REVERB, SEC_OTT, 0xFF},     /* FX */
    {SEC_VOICE, 0xFF},                                                  /* GLO */
};
static const uint8_t SY_FAM_BTN[] = {B_HOME, B_EDIT, B_ENV, B_LFO, B_SEQ, B_FX, B_GLO};

static uint32_t sy_first_page(uint32_t sec)
{
    uint32_t p;
    for (p = 0; p < sy_npg; p++)
        if (sy_pg[p].sec == sec)
            return p;
    return 0;
}

static uint32_t sy_family(uint32_t sec)         /* the family a section is in (the button that shows it) */
{
    uint32_t f, k;
    for (f = 0; f < sizeof SY_FAM_BTN; f++)
        for (k = 0; SY_FAM_SECS[f][k] != 0xFF; k++)
            if (SY_FAM_SECS[f][k] == sec)
                return f;
    return 0;
}

/* a family's button: its first section, or (already there) its next one */
static void sy_family_press(uint32_t f)
{
    uint32_t k, n = 0, at = 0xFF, sec = sy_pg[sy_page].sec;
    while (SY_FAM_SECS[f][n] != 0xFF) {
        if (SY_FAM_SECS[f][n] == sec)
            at = n;
        n++;
    }
    k = at == 0xFF ? 0u : (at + 1u) % n;
    sy_page = sy_first_page(SY_FAM_SECS[f][k]);
    sy_dirty = 1;
}

/* a knob's steps on a parameter: a hundredth of a long range a detent, one otherwise; a destination steps over
 * what cannot be modulated */
static void sy_knob(uint32_t id, int32_t steps)
{
    const sp_desc_t *d = &SP[id];
    int32_t v = sy_patch.p[id], span = d->hi - d->lo;
    if (d->unit == U_DST) {
        int32_t dir = steps > 0 ? 1 : -1, k;
        for (k = 0; k < (steps > 0 ? steps : -steps); k++) {
            int32_t w = v;
            do
                w += dir;
            while (w > 0 && w < (int32_t)SP_COUNT && !(SP[w].flags & F_MOD));
            if (w < 0 || w >= (int32_t)SP_COUNT)
                break;
            v = w;
        }
    } else {
        v += steps * (span > 200 ? span / 100 : 1);
    }
    sy_patch.p[id] = sp_clamp(id, v);
    sy_dirty = 1;
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
    sy_patch_load(&p);
    sy_sound = k;
    sy_sound_want = -1;
    sy_dirty = 1;
}

/* ------------------------------------------------------------------------------------------- panel --- */
static uint32_t sy_keyprev;
static uint8_t sy_keynote[SY_NKEY];             /* the note each key started (0xFF: none): released as it was */
static int32_t sy_oct;                           /* OCT- / OCT+: -3 .. +3 */

static void sy_keys(void)
{
    uint32_t now = fm1_in.notes & ((1u << SY_NKEY) - 1u), chg = now ^ sy_keyprev, k;
    for (k = 0; k < SY_NKEY; k++) {
        if (!((chg >> k) & 1u))
            continue;
        if ((now >> k) & 1u) {
            int32_t n = SY_KEY0 + (int32_t)k + 12 * sy_oct;
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

static void sy_panel(void)
{
    uint32_t rel, pressed = fm1_input_edges(&rel), b, f, i;
    int32_t s;
    (void)rel;
    for (b = 0; b < NB; b++) {
        if (!((pressed >> panel.btn[b]) & 1u))
            continue;
        if (b == B_OCTDN || b == B_OCTUP) {
            sy_oct += b == B_OCTUP ? 1 : -1;
            sy_oct = sy_oct < -3 ? -3 : sy_oct > 3 ? 3 : sy_oct;
            sy_dirty = 1;
            continue;
        }
        for (f = 0; f < sizeof SY_FAM_BTN; f++)
            if (SY_FAM_BTN[f] == b)
                sy_family_press(f);
    }
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0 && i < sy_pg[sy_page].n)
            sy_knob(sy_pg[sy_page].id[i], s);
    if ((s = panel_enc(EN_SELECT)) != 0) {
        sy_page = (uint32_t)((int32_t)sy_page + s % (int32_t)sy_npg + (int32_t)sy_npg) % sy_npg;
        sy_dirty = 1;
    }
    if ((s = panel_enc(EN_PRESET)) != 0) {
        uint32_t n = sizeof SOUNDS / sizeof SOUNDS[0] + 1u;
        sy_sound_load((uint32_t)((int32_t)(sy_sound_want >= 0 ? (uint32_t)sy_sound_want : sy_sound) +
                                 s % (int32_t)n + (int32_t)n) % n);
    }
    (void)panel_enc(EN_ALGO);                    /* (not used yet: Phase 4) */
}

/* -------------------------------------------------------------------------------------------- LEDs --- */
static void sy_led(uint8_t *l, uint32_t id)      /* matrix id: a button's (panel.btn), a key's 14 + k */
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id)
                l[c] |= (uint8_t)(1u << r);
}

static void sy_leds(void)                        /* the keys held lit, the page's button lit, the others dim */
{
    uint32_t c, k, f;
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = fm1_led_dim[c] = 0;
    for (k = 0; k < SY_NKEY; k++)
        if (sy_keynote[k] != 0xFF)
            sy_led(fm1_led, 14u + k);
    for (f = 0; f < sizeof SY_FAM_BTN; f++)
        sy_led(f == sy_family(sy_pg[sy_page].sec) ? fm1_led : fm1_led_dim, panel.btn[SY_FAM_BTN[f]]);
}

/* ------------------------------------------------------------------------------------------ screen --- */
/* a header (the section and its page, the sound), four knob cards (name, value as the FM-1 shows it, a gauge),
 * a footer (voices, load, octave); drawn whole in two canvas strips (gfx.c's canvas is 240 x 124) */
static void sy_draw_at(int32_t oy)
{
    const sy_page_t *pg = &sy_pg[sy_page];
    char t[SP_TEXT_N + 8u], *e;
    uint32_t i, nv = 0;
    cv_oy = oy;
    e = sp_put(t, SEC_NAMES[pg->sec]);
    if (sy_secpages[pg->sec] > 1u) {
        e = sp_put(e, "  ");
        e = sp_fix(e, pg->of, 0u, 0);
        e = sp_put(e, "/");
        sp_fix(e, sy_secpages[pg->sec], 0u, 0);
    }
    cv_text(10, 5, &AF_M, t, T_TEXT);
    cv_text_r(230, 8, &AF_S, sy_patch.name, T_THEME, T_BG);
    cv_rect(0, 28, 240, 1, T_LINE);
    for (i = 0; i < 4u; i++) {
        int32_t x = 8 + (int32_t)(i % 2u) * 116, y = 36 + (int32_t)(i / 2u) * 90, w = 108, h = 82;
        char k[3] = {'K', (char)('1' + i), 0};
        cv_rrect(x, y, w, h, 8, T_SURF, T_BG);
        cv_text_r(x + w - 8, y + 5, &AF_S, k, T_DIM, T_SURF);
        if (i < pg->n) {
            uint32_t id = pg->id[i];
            const sp_desc_t *d = &SP[id];
            int32_t v = sy_patch.p[id];
            cv_text_on(x + 8, y + 5, &AF_S, d->name, T_MID, T_SURF);
            sp_text(id, v, t);
            cv_text_on(x + 8, y + 27, &AF_M, t, T_TEXT, T_SURF);
            if (d->hi - d->lo > 2 && d->unit != U_LIST && d->unit != U_TABLE && d->unit != U_SRC &&
                d->unit != U_DST && d->unit != U_SYNC) {
                int32_t gw = w - 16, z = d->lo < 0 ? (int32_t)((-d->lo) * gw / (d->hi - d->lo)) : 0;
                int32_t at = (int32_t)((v - d->lo) * gw / (d->hi - d->lo));
                cv_rect(x + 8, y + 62, gw, 4, T_LINE);
                cv_rect(x + 8 + (at < z ? at : z), y + 62, at < z ? z - at : at - z, 4, T_THEME);
                if (z)                                   /* (bipolar: its centre) */
                    cv_rect(x + 8 + z, y + 59, 1, 10, T_MID);
            }
        }
    }
    for (i = 0; i < SY_NVOICE; i++)
        nv += sy_v[i].on != 0;
    e = sp_fix(t, (int32_t)nv, 0u, 0);
    e = sp_put(e, nv == 1u ? " voice  " : " voices  ");
    if (SY_LOAD_KNOWN) {
        e = sp_fix(e, (int32_t)(sy_load_q8 * 100u / 256u), 0u, 0);
        sp_put(e, "% cpu");
    } else {
        sp_put(e, "cpu --");
    }
    cv_text(10, 219, &AF_S, t, T_MID);
    e = sp_put(t, "OCT ");
    sp_fix(e, sy_oct, 0u, 1);
    cv_text_r(230, 219, &AF_S, t, T_MID, T_BG);
}

static void sy_draw(void)
{
    cv_begin(240, 120, T_BG);
    sy_draw_at(0);
    cv_blit(0, 0);
    cv_begin(240, 120, T_BG);
    sy_draw_at(-120);
    cv_blit(0, 120);
    cv_oy = 0;
}

/* ------------------------------------------------------------------------------ panel.c's settings --- */
/* panel.c (Felucca's, shared) declares these: the palette its settings name, and their save (Phase 4: the settings
 * in flash; nothing is written yet) */
static void palette_apply(void) { palette_set(settings.palette < NPALETTES ? settings.palette : 3u); }
static void settings_save(void) {}

/* --------------------------------------------------------------------------------------- main loop --- */
static uint32_t sy_last_draw;

/* power-on: the panel, the screen's colours, the engine with the first starter sound (no fade: nothing plays yet),
 * the pages, the audio */
static void sy_app_boot(void)
{
    sy_patch_t p;
    uint32_t k;
    panel_init();
    palette_set(3u);                             /* ICE */
    sy_init();
    sy_sound_patch(0, &p);
    sy_patch = p;
    sp_snap();
    sy_tables_now();
    sy_sound = 0;
    sy_pages();
    sy_page = sy_first_page(SEC_OSCA);
    for (k = 0; k < SY_NKEY; k++)
        sy_keynote[k] = 0xFF;
    sy_audio_init();
    sy_dirty = 1;
}

/* every millisecond: the panel, a step of a wavetable build, a sound asked for while one was loading */
static void sy_app_ms(void)
{
    sy_keys();
    sy_panel();
    sy_service();
    if (sy_sound_want >= 0 && !sy_next_st)
        sy_sound_load((uint32_t)sy_sound_want);
}

/* a frame (~60 a second): the LEDs; the screen when something changed, or its footer's figures every half second */
static void sy_app_frame(void)
{
    sy_leds();
    if (sy_dirty || fm1_ms - sy_last_draw >= 500u) {
        sy_dirty = 0;
        sy_last_draw = fm1_ms;
        sy_draw();
    }
}
