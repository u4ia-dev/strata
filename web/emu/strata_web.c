/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone
 * The simulated FM-1 below follows Felucca's web/emu/felucca_web.c (Copyright (C) 2026 Leo Kuroshita (@kurogedelic),
 * Hügelton Instruments, GPL-3.0), itself after X0X's browser build (charlesvestal/fm1-x0x, GPL-3.0). */
/* STRATA in the browser on a simulated FM-1: the app (firmware/src/sy_app.c, the sy_* engine, gfx.c, panel.c)
 * compiled to WebAssembly against a simulated FM-1, with the exports Felucca's page and worklet (web/emu/index.html,
 * worklet.js) use, so the same page plays it (web/emu/build_strata.sh brands it):
 *   - the clock: TIMER4 ticks (24 MHz) and fm1_ms advance with the audio the worklet asks for; the audio interrupt
 *     (sy_app.c fm1_alnk0_irq) renders each I2S half as the DMA would ask for it; the main loop's millisecond
 *     (sy_app_ms) every millisecond, its frame (sy_app_frame) every 15 ms;
 *   - the panel: buttons and encoders by their printed labels (panel.c maps them to the matrix), the keys as
 *     fm1_in.notes, the LEDs read back through the key matrix; MASTER as the output's level;
 *   - the screen: lcd_fill / lcd_blit into a 240 x 240 RGB565 frame (the panel's byte order);
 *   - the flash: an erased 1 MiB image for the page's save and restore (Strata writes nothing to it yet).
 * Not here: USB, TRS MIDI, the console, the battery. Web MIDI's channel messages go to sy_app.c's MIDI in.
 *   web/emu/build_strata.sh  ->  build/emu_strata/{index.html, worklet.js, strata.wasm} */
#include <stdint.h>
#include <string.h>

#define __attribute__(x)                         /* (no sections, no alignment pragmas on the host) */

static uint8_t nor[0x100000];                    /* the SPI NOR (erased: 0xFF) */
static volatile uint32_t fm1_ms;

/* ------------------------------------------------------------------------------------------- the clock --- */
#define FM1_TICKS_PER_US 24u                     /* TIMER4: 24 MHz */
static uint32_t web_ticks;
static uint32_t fm1_ticks(void) { return web_ticks; }

/* ---------------------------------------------------------------------------------------- audio (ALNK0) --- */
static uint32_t web_half;                        /* the half the next render fills */
#define FM1_AUDIO_HALF 1u
static uint8_t fm1_audio_pending(void) { return FM1_AUDIO_HALF; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return web_half; }
static void fm1_audio_ack_half(void) {}
static void fm1_audio_init(int32_t *buf, uint32_t words, void (*isr)(void), uint32_t prio)
{
    (void)buf; (void)words; (void)isr; (void)prio;
}
void isr_alnk0(void) {}

/* -------------------------------------------------------------------------------------------- the panel --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {      /* as hal/fm1_input.h: key id at (column, row bit) */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL];
static struct { volatile uint32_t notes, buttons; } fm1_in;
static uint32_t web_pressed, web_released;
static int32_t web_enc_steps[7];
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = web_pressed;
    if (released)
        *released = web_released;
    web_pressed = web_released = 0;
    return p;
}
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = web_enc_steps[e % 7u];
    web_enc_steps[e % 7u] = 0;
    return s;
}

/* ------------------------------------------------------------------------------------------- the screen --- */
static uint16_t web_fb[240 * 240];               /* RGB565 as the panel gets it (high byte first) */
static uint32_t web_draws;
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = p[j * w + i];
    web_draws++;
}
static void lcd_sync(void) {}

/* ------------------------------------------------------------------------------ the app and the synth --- */
#include "../../firmware/src/gfx.c"                /* (its fonts, palettes and keycaps: build/gen) */
static uint8_t fx_lowcut;                        /* (panel.c's settings_init sets Felucca's low cut: not Strata's) */
#include "../../firmware/src/panel.c"
#include "../../firmware/src/sy_core.h"
#include "../../firmware/src/sy_wt.c"
#include "../../firmware/src/sy_wtgen.c"
#include "../../firmware/src/sy_param.c"
#include "../../firmware/src/sy_osc.c"
#include "../../firmware/src/sy_env.c"
#include "../../firmware/src/sy_lfo.c"
#include "../../firmware/src/sy_mod.c"
#include "../../firmware/src/sy_filter.c"
#include "../../firmware/src/sy_voice.c"
#include "../../firmware/src/sy_fx.c"
#include "../../firmware/src/sy_engine.c"
#include "../../firmware/src/sy_text.c"
#include "../../firmware/src/sy_sounds.h"
#include "../../firmware/src/sy_gfx.c"
#include "../../firmware/src/sy_ui.c"
#include "../../firmware/src/sy_ui_draw.c"
#define SY_LOAD_KNOWN 0                          /* (web_ticks moves a millisecond at a time, never within a half) */
#include "../../firmware/src/sy_app.c"

#undef __attribute__
#define EXPORT __attribute__((used, visibility("default")))

/* ------------------------------------------------------------------------------------------- the device --- */
static uint32_t web_booted, web_boot_ms, web_last_frame, web_master_adc = 700;

#define RING 4096u                               /* the output: stereo float, filled a half at a time */
static float ring[RING * 2];
static uint32_t ring_w, ring_r, web_samples_due;

/* one millisecond of the device: the clock, the audio halves due by now, the main loop */
static void web_ms(void)
{
    fm1_ms++;
    web_ticks += 1000u * FM1_TICKS_PER_US;
    web_samples_due += 44118u;
    while (web_samples_due >= 1000u * SY_HALF) {          /* an I2S half: the audio interrupt renders it */
        const int32_t *o = &sy_abuf[web_half * SY_HALF_WORDS];
        float g = (float)web_master_adc * (1.0f / 1023.0f);
        uint32_t k;
        web_samples_due -= 1000u * SY_HALF;
        fm1_alnk0_irq();
        g *= g;                                           /* MASTER: a level, about as a pot sounds */
        for (k = 0; k < SY_HALF; k++) {
            uint32_t i = (ring_w++ % RING) * 2u;
            ring[i] = (float)o[2u * k] / 8388608.0f * g;
            ring[i + 1] = (float)o[2u * k + 1u] / 8388608.0f * g;
        }
        web_half ^= 1u;
    }
    if (!web_booted || (int32_t)(fm1_ms - web_boot_ms) < 0)
        return;
    sy_app_ms();
    if (fm1_ms - web_last_frame >= 15u) {                 /* a frame: ~60 a second, as the FM-1's main loop */
        web_last_frame = fm1_ms;
        sy_app_frame();
    }
}

static float out_l[1024], out_r[1024];
EXPORT float *web_out_l(void) { return out_l; }
EXPORT float *web_out_r(void) { return out_r; }
/* n frames (<= 1024) into out_l / out_r: the device runs until they exist */
EXPORT void web_render(uint32_t n)
{
    uint32_t k;
    if (n > 1024u)
        n = 1024u;
    while (ring_w - ring_r < n)
        web_ms();
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_r++ % RING) * 2u;
        out_l[k] = ring[i];
        out_r[k] = ring[i + 1];
    }
}

/* the flash: kept for the page's save and restore; nothing writes it yet */
EXPORT uint8_t *web_nor(void) { return nor; }
EXPORT uint32_t web_nor_size(void) { return sizeof nor; }
EXPORT void web_nor_erase(void) { memset(nor, 0xFF, sizeof nor); }
EXPORT uint32_t web_flash_writes_count(void) { return 0; }
EXPORT void web_boot(void)
{
    if (web_booted)
        return;
    sy_app_boot();
    web_boot_ms = fm1_ms + 30u;
    web_booted = 1;
}

/* input, by the printed labels: buttons bit b = B_FX .. B_OCTUP (panel.c), keys bit k = key k (F3 .. G5),
 * encoders by role (EN_SELECT .. EN_K4), MASTER 0..1023 */
EXPORT void web_buttons(uint32_t labels)
{
    uint32_t m = 0, b, old = fm1_in.buttons;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            m |= 1u << panel.btn[b];
    web_pressed |= m & ~old;
    web_released |= old & ~m;
    fm1_in.buttons = m;
}
EXPORT void web_keys(uint32_t mask) { fm1_in.notes = mask & ((1u << 27) - 1u); }
EXPORT void web_enc(uint32_t role, int32_t n)
{
    if (role < NE)
        web_enc_steps[panel.enc[role] % 7u] += n * panel.dir[role];
}
EXPORT void web_master(uint32_t v) { web_master_adc = v > 1023u ? 1023u : v; }
EXPORT uint32_t web_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    if (status >= 0x80u && status < 0xF0u)
        sy_midi_in(status, d1, d2);
    return 1;
}

/* the LEDs: bit b = the button labelled b lit, keys bit k; the dim glow alike; no breath, no sweep yet */
static uint32_t led_on(const uint8_t *l, uint32_t id)
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id)
                return (l[c] >> r) & 1u;
    return 0;
}
static uint32_t led_buttons(const uint8_t *l)
{
    uint32_t b, m = 0;
    for (b = 0; b < NB; b++)
        m |= led_on(l, panel.btn[b]) << b;
    return m;
}
static uint32_t led_keys(const uint8_t *l)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        m |= led_on(l, 14u + k) << k;
    return m;
}
EXPORT uint32_t web_lit_buttons(void) { return led_buttons(fm1_led); }
EXPORT uint32_t web_lit_keys(void) { return led_keys(fm1_led); }
EXPORT uint32_t web_dim_buttons(void) { return led_buttons(fm1_led_dim); }
EXPORT uint32_t web_dim_keys(void) { return led_keys(fm1_led_dim); }
EXPORT uint32_t web_dim_level(void) { return 0; }
EXPORT uint32_t web_breath_buttons(void) { return 0; }
EXPORT uint32_t web_breath_keys(void) { return 0; }
EXPORT uint32_t web_mid_keys(void) { return 0; }
EXPORT uint8_t *web_anim_levels(void) { return 0; }

EXPORT uint16_t *web_screen(void) { return web_fb; }
EXPORT uint32_t web_screen_draws(void) { return web_draws; }
EXPORT uint32_t web_now_ms(void) { return fm1_ms; }
EXPORT uint32_t web_playing(void) { return 0; }
EXPORT uint32_t web_sample_rate(void) { return 44118u; }

/* for the tests (web/emu/strata_emu_test.mjs, strata_shot.mjs): the voices sounding, the page shown, a parameter's
 * value, the MOD layer, a matrix slot, the palette, the keys' octave, an LFO's drawn points and the one picked */
EXPORT uint32_t web_test_voices(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < SY_NVOICE; i++)
        n += sy_v[i].on != 0;
    return n;
}
EXPORT uint32_t web_test_page(void) { return ui.page; }
EXPORT uint32_t web_test_page_section(void) { return UI_PG[ui.page].sec; }
EXPORT const char *web_test_page_name(void) { return UI_PG[ui.page].name; }
EXPORT int32_t web_test_page_param(uint32_t k) { return k < 4u ? UI_PG[ui.page].id[k] : -1; }
EXPORT int32_t web_test_value(uint32_t id) { return id < SP_COUNT ? sy_patch.p[id] : 0; }
EXPORT void web_test_set(uint32_t id, int32_t v)
{
    if (id < SP_COUNT) {
        sy_patch.p[id] = sp_clamp(id, v);
        sy_ui_touch();
    }
}
EXPORT const char *web_test_patch_name(void) { return sy_patch.name; }
EXPORT uint32_t web_test_sound(void) { return sy_sound; }
EXPORT uint32_t web_test_mod(void) { return ui.mod; }
EXPORT uint32_t web_test_mod_src(void) { return ui.mod_src; }
EXPORT int32_t web_test_slot(uint32_t s, uint32_t field) { return s < NSLOT && field < 3u ? sy_patch.p[SP_M1_SRC + 3u * s + field] : 0; }
EXPORT uint32_t web_test_palette(int32_t i)
{
    if (i >= 0) {
        ui.pal = (uint8_t)(i % (int32_t)SY_NPAL);
        sy_palette(ui.pal);
        ui.dirty = D_ALL;
    }
    return ui.pal;
}
EXPORT uint32_t web_test_dirty(void) { return ui.dirty; }
EXPORT int32_t web_test_oct(void) { return sy_app_oct(); }
/* LFO k's drawn shape: f 0 its count of points, 1 .. 3 point i's x, y, curve */
EXPORT int32_t web_test_draw(uint32_t k, uint32_t i, uint32_t f)
{
    const sp_draw_t *d = &sy_patch.draw[k & 1u];
    if (f == 0u)
        return d->n;
    if (i >= SP_NPT)
        return 0;
    return f == 1u ? d->x[i] : f == 2u ? d->y[i] : d->c[i];
}
EXPORT uint32_t web_test_lpt(uint32_t k) { return ui.lpt[k & 1u]; }
/* GLOBAL: f 0 BPM, 1 TS-BEAT, 2 TS-NOTE; the engine's quarter notes a minute x 100 (f 3) */
EXPORT int32_t web_test_tempo(uint32_t f)
{
    return f == 0u ? ui.bpm : f == 1u ? ui.tsb : f == 2u ? UI_TSN_VAL[ui.tsn] : (int32_t)(sy_tempo * 100.0f + 0.5f);
}
EXPORT const char *web_test_toast(void) { return ui.toast; }
