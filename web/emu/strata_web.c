/* SPDX-License-Identifier: GPL-3.0-only
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
 *   - the flash: a 1 MiB image (erased, or the page's saved one), written through storage_hw.c's hooks with the
 *     firmware's allow-list (Strata's presets, settings and sound: sy_store.c), kept by the page across reloads.
 * Not here: USB, TRS MIDI, the console, the battery. Web MIDI's channel messages go to sy_app.c's MIDI in.
 *   web/emu/build_strata.sh  ->  build/emu_strata/{index.html, worklet.js, strata.wasm} */
#include <stdint.h>
#include <stdio.h>
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
#define FM1_NENC 7u                                  /* (V0.87.1: KEY SCAN names the encoders' lines: as hal/fm1_input.h) */
static const uint8_t FM1_ENC[FM1_NENC][4] = {
    {0, 0, 1, 0}, {2, 0, 3, 0}, {8, 1, 9, 1}, {8, 0, 9, 0}, {6, 0, 7, 0}, {4, 0, 5, 0}, {0, 5, 1, 5},
};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_breath[FM1_NCOL], fm1_led_mid[FM1_NCOL];
static struct { volatile uint32_t notes, buttons; uint8_t raw[FM1_NCOL]; } fm1_in;   /* (raw: the matrix, BENCH > KEY SCAN) */
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
static uint32_t web_draws, web_px;              /* blits, and the pixels they sent (the SPI's load on the FM-1) */
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = p[j * w + i];
    web_draws++;
    web_px += w * h;
}
static void lcd_sync(void) {}
static uint32_t web_lcd_off;                     /* (V0.86, F34: SCREEN OFF: the panel dark until it wakes) */
static void lcd_power(uint32_t s)
{
    if (s == 0u) {
        web_lcd_off = 1;
        memset(web_fb, 0, sizeof web_fb);
    } else if (s == 2u) {
        web_lcd_off = 0;
    }
}
static int32_t web_batt = 600, web_usb;          /* (V0.86, F31: the battery's reading, USB power, as the tests set them) */
static int sy_usb_power(void) { return web_usb; }

/* ------------------------------------------------------------------------------------------- the flash --- */
/* storage_hw.c's hooks on the simulated NOR: an erase or a program only inside the firmware's allow-list
 * (hal/fm1_flash_map.h FL_STORE_OK, as storage_hw.c checks it; the RAM driver's FL_RANGE_OK is the device's second
 * guard), a program only clearing bits, no page wrapping; what was refused is counted (the tests) */
#include "../../firmware/hal/fm1_flash_map.h"
static uint32_t web_flash_writes, web_flash_refused;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off >= sizeof nor || n > sizeof nor - off)
        return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (!FL_STORE_OK(off, 4096u) || (off & 4095u)) {
        web_flash_refused++;
        return -8;
    }
    memset(nor + off, 0xFF, 4096);
    web_flash_writes++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (!FL_STORE_OK(off, n) || (off & 0xFFu) + n > 256u) {
        web_flash_refused++;
        return -8;
    }
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    web_flash_writes++;
    return 0;
}

/* ------------------------------------------------------------------------------ the app and the synth --- */
#define CV_MAX (240u * 114u)                       /* (strata.c's: the canvas Strata's largest strip needs) */
#include "../../firmware/src/gfx.c"                /* (its fonts, palettes and keycaps: build/gen) */
static uint8_t fx_lowcut;                        /* (panel.c's settings_init sets Felucca's low cut: not Strata's) */
static uint32_t web_dim_lo = 3;
static void fm1_led_level(uint32_t lv) { web_dim_lo = lv & 3u; }   /* (V0.87, the HAL's: LIGHTS 0 OFF .. 3 HIGH) */
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
#include "../../firmware/src/sy_drum.c"
#include "../../firmware/src/sy_engine.c"
#include "../../firmware/src/sy_text.c"
#include "../../firmware/src/sy_store.c"
static uint32_t web_mout[256], web_mout_t[256], web_mout_n;   /* (V0.7: the chords' USB MIDI out, kept for the tests
                                                               * and the page; V0.75: each one's time, samples) */
#define SY_MIDI_OUT(pkt) (web_mout_t[web_mout_n % 256u] = cht.t, web_mout[web_mout_n++ % 256u] = (pkt))
#include "../../firmware/src/sy_chord.c"
#define LP_LOCK() 0u                               /* (one thread here: the audio runs between the page's calls) */
#define LP_UNLOCK(f) ((void)(f))
#include "../../firmware/src/sy_loop.c"
#include "../../firmware/src/sy_c2.c"                 /* (one core here: CORE 2's page says so) */
#include "../../firmware/src/sy_sounds.h"
#define STRATA_VERSION "V0.87.1"                        /* (the boot screen's: the page names the build) */
#ifndef WEB_NO_LINT                               /* (web_lint: the screens' check; strata_bench.c: none) */
#define LK_LINT 1
#endif
#include "../../firmware/src/sy_gfx.c"
#include "../../firmware/src/sy_ui.c"
#include "../../firmware/src/sy_ui_draw.c"
#include "../../firmware/src/sy_ui_fx.c"
#include "../../firmware/src/sy_ui_save.c"
#include "../../firmware/src/sy_ui_chord.c"
#include "../../firmware/src/sy_ui_loop.c"
#include "../../firmware/src/sy_ui_drum.c"
#define SY_LOAD_KNOWN 0                          /* (web_ticks moves a millisecond at a time, never within a half) */
#define SY_KEYS_ISR 1                             /* (the keys from "TIMER5", as the FM-1: web_ms) */
#include "../../firmware/src/sy_app.c"
#define sy_tp_ms sy_tp_ms_real                   /* (V0.87.1: the renders and checks of DEBUG > TESTS: a test shown loaded, */
#define sy_tp_active sy_tp_active_real           /* with a time that moves, whatever sy_testproj.c does) */
#include "../../firmware/src/sy_testproj.c"    /* (V0.87.1: the test projects) */
#undef sy_tp_ms
#undef sy_tp_active
static int32_t web_tp_fake = -1;                /* (>= 0: the test shown loaded; -1: the project's own) */
static uint32_t web_tp_fake_t0;
static uint32_t sy_tp_ms(uint32_t k) { return web_tp_fake >= 0 ? fm1_ms - web_tp_fake_t0 : sy_tp_ms_real(k); }
static int sy_tp_active(void) { return web_tp_fake >= 0 ? web_tp_fake + 1 : sy_tp_active_real(); }

#undef __attribute__
#define EXPORT __attribute__((used, visibility("default")))

/* ------------------------------------------------------------------------------------------- the device --- */
static uint32_t web_booted, web_boot_ms, web_last_frame, web_master_adc = 700;

#define RING 4096u                               /* the output: stereo float, filled a half at a time */
static float ring[RING * 2];
static uint32_t ring_w, ring_r, web_samples_due;

#ifndef WEB_FRAME_BEGIN
#define WEB_FRAME_BEGIN() ((void)0)              /* (web/emu/strata_bench.c: a frame's drawing timed) */
#define WEB_FRAME_END() ((void)0)
#endif
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
    sy_keys_tick();                                       /* (TIMER5's: the keys while they only play) */
    sy_app_ms();
    if (fm1_ms - web_last_frame >= 15u) {                 /* a frame: ~60 a second, as the FM-1's main loop */
        web_last_frame = fm1_ms;
        WEB_FRAME_BEGIN();
        sy_app_frame();
        WEB_FRAME_END();
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

/* the flash: kept for the page's save and restore (it saves the image when the count of writes moves) */
EXPORT uint8_t *web_nor(void) { return nor; }
EXPORT uint32_t web_nor_size(void) { return sizeof nor; }
EXPORT void web_nor_erase(void) { memset(nor, 0xFF, sizeof nor); }
EXPORT uint32_t web_flash_writes_count(void) { return web_flash_writes; }
/* for the tests: an erase where the allow-list says no (the hook refuses it: -8), the refusals so far */
EXPORT int32_t web_test_erase(uint32_t off) { return st_erase(off); }
EXPORT uint32_t web_test_refused(void) { return web_flash_refused; }
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
    else if (status >= 0xF8u)                   /* (V0.75: clock, start, continue, stop, as they come) */
        sy_midi_rt(status, fm1_ms);
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
EXPORT uint32_t web_lit_buttons(void)            /* (bit 14: PLAY's green LED, as Felucca's page reads it) */
{
    return led_buttons(fm1_led) | ((fm1_led[UI_LED_PLAY_GREEN >> 3] >> (UI_LED_PLAY_GREEN & 7u)) & 1u) << 14;
}
EXPORT uint32_t web_lit_keys(void) { return led_keys(fm1_led); }
EXPORT uint32_t web_dim_buttons(void) { return led_buttons(fm1_led_dim); }
EXPORT uint32_t web_dim_keys(void) { return led_keys(fm1_led_dim); }
EXPORT uint32_t web_dim_level(void) { return web_dim_lo; }
EXPORT uint32_t web_breath_buttons(void) { return led_buttons(fm1_led_breath); }   /* (a layer's button) */
EXPORT uint32_t web_breath_keys(void) { return led_keys(fm1_led_breath); }
EXPORT uint32_t web_mid_keys(void) { return led_keys(fm1_led_mid); }   /* (SCL: the root's keys) */
EXPORT uint8_t *web_anim_levels(void) { return 0; }

EXPORT uint16_t *web_screen(void) { return web_fb; }
EXPORT uint32_t web_screen_draws(void) { return web_draws; }
EXPORT uint32_t web_screen_px(void) { return web_px; }
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
/* SCL: f 0 the scale, 1 the root, 2 the fit, 3 the keys' velocity; the last note a key started, its velocity */
EXPORT int32_t web_test_scl(uint32_t f) { return f == 0u ? ui.scale : f == 1u ? ui.root : f == 2u ? ui.fit : ui.kvel; }
EXPORT int32_t web_test_key_note(void) { return sy_key_note; }
EXPORT int32_t web_test_key_vel(void) { return sy_key_vel; }
/* the hold layers: the one open (LAY_*); PERFORM's bend as the engine has it (x 1000, -1000 .. 1000) and its wheel */
EXPORT uint32_t web_test_lay(void) { return ui.lay; }
/* PLAY: the transport running; a FREE LFO's phase (its top 16 bits) */
EXPORT uint32_t web_test_run(void) { return ui.run; }
/* the SETTINGS: f 0 HELP, 1 GLOW, 2 HOLD, 3 MIDI's channel, 4 PROGRAM, 5 LEARN, 6 the learned CCs, 7 the parameter
 * LEARN picked (v >= 0: HELP set to v first); the parameter a CC is learned to (-1 none) */
EXPORT int32_t web_test_set_get(uint32_t f, int32_t v)
{
    if (f == 0u && v >= 0)
        ui.help = v != 0;
    return f == 0u ? ui.help : f == 1u ? ui.glow : f == 2u ? settings_hold : f == 3u ? ui.mch : f == 4u ? ui.mpc
           : f == 5u ? ui.learn : f == 6u ? (int32_t)ml_count() : f == 7u ? ui.batt_show : f == 8u ? ui.scr_off : ui.ml_pick;
}
EXPORT int32_t web_test_learned(uint32_t cc) { return sy_ui_learned(cc & 127u); }
EXPORT uint32_t web_test_lfo_phase(uint32_t k) { return sy_glfo[k & 1u].ph >> 16; }
EXPORT int32_t web_test_bend(void) { return (int32_t)(sy_bend_v * 1000.0f + (sy_bend_v < 0.0f ? -0.5f : 0.5f)); }
EXPORT uint32_t web_test_wheel(void) { return sy_wheel; }
/* the screens the panel cannot reach here (sy_main.c's, which the emulator does not run): k 1, 2 SAFE START (after a
 * crash, HOME held), 3 the power-on's (drawn at once), 4 OCT- + OCT+ held 3 s (USB BOOT's countdown), 0 none */
EXPORT void web_test_screen(uint32_t k)
{
    if (k == 1u || k == 2u) {
        sy_ui_safe(k);
    } else if (k == 3u) {
        uint32_t s;
        for (s = 0; s < 240u; s += 80u) {
            lk_begin(0, (int32_t)s, 240u, 80u);
            ui_p_boot();
            lcd_blit(0, s, 240u, 80u, cv_px);
        }
    } else {
        ui.ub_t0 = k == 4u ? (fm1_ms - 3000u) | 1u : 0u;
        ui.dirty = D_ALL;
    }
}
/* the countdown shown (a long hold, sy_ui_draw.c): its title and its seconds, "SETTINGS 4"; "" none */
/* DEBUG > TESTS (V0.87.1): the stub's test k loaded (-1 none), its time (the stub's own is 0) moving from ms on; the
 * test picked, the question asked */
EXPORT int32_t web_test_tp(uint32_t f, int32_t v)
{
    if (f == 1u) {
        web_tp_fake = v;
        web_tp_fake_t0 = fm1_ms;
        ui.dirty = D_ALL;
    }
    return f == 0u ? ui.tp_k : f == 2u ? sy_tp_active() : f == 3u ? (int32_t)sy_tp_ms(0)
         : f == 4u ? (sy_tp_active_real() ? (int32_t)sy_tp_ms_real((uint32_t)sy_tp_active_real() - 1u) : -1)   /* (V0.87.1: the real one's time) */
         : -1;
}
EXPORT int32_t web_test_ks_enc(uint32_t r, uint32_t c) { return ui_ks_enc(r, c); }   /* (V0.87.1: KEY SCAN: the encoder at a cell) */
EXPORT const char *web_test_cd(void)
{
    static char t[48];
    uint32_t n;
    float frac;
    const char *sub, *sub2, *label;
    t[0] = 0;
    if (ui_view() == V_HELP) {                      /* (V0.875: the help map as long as HOME is held, on every page) */
        memcpy(t, "HELP: LET GO", 13);
    } else if (ui_view() == V_CD && ui_cd_view(&n, &frac, t, &sub, &sub2, &label))
        sp_fix(sp_put(t + strlen(t), " "), (int32_t)n, 0u, 0);
    else
        t[0] = 0;
    return t;
}
/* effect instance i in (1) or bypassed (0), as FX held + its key sets it */
EXPORT void web_test_fx_on(uint32_t i, uint32_t on)
{
    if (i < FXI_N) {
        sy_patch.p[FXI_ON[i]] = on != 0;
        sy_ui_touch();
    }
}
/* page p shown at once (not SAVE's, the browser's: they open from SAVE); the pages' count */
EXPORT uint32_t web_test_go(uint32_t p)
{
    if (p < UI_NPG && UI_PG[p].sec != SEC_SAVE && UI_PG[p].sec != SEC_BROWSE)
        ui_go(p);
    return UI_NPG;
}
EXPORT uint32_t web_test_page_section(void) { return UI_PG[ui.page].sec; }
EXPORT const char *web_test_page_name(void) { return UI_PG[ui.page].name; }
EXPORT int32_t web_test_page_param(uint32_t k) { return k < 4u ? UI_PG[ui.page].id[k] : -1; }
EXPORT int32_t web_test_page_virt(uint32_t k) { return k < 4u ? ui_virt(UI_PG[ui.page].id[k]) : -1; }   /* (the EQ point picked's, the matrix slot's) */
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
EXPORT int32_t web_test_win(uint32_t f)   /* (V0.87's windows: assign, SAVE, a question) */
{
    return f == 0u ? ui.asg : f == 1u ? ui.sw : f == 2u ? ui.sw_pos : f == 3u ? ui.qk : f == 4u ? ui.asg_src : f == 5u ? ui.asg_k
         : f == 6u ? sy_patch.p[SP_M_POL] : -1;
}
EXPORT int32_t web_test_pl_id(uint32_t pos)   /* (the presets' list: its entry at pos; 0xFFFF: INIT's id) */
{
    uint16_t ids[SY_NFACT + SS_NUSER];
    uint32_t n = sy_app_list(0, 0, ids);
    return pos == 0xFFFFu ? (int32_t)(SY_NFACT - 1u) : pos < n ? ids[pos] : -1;
}
EXPORT int32_t web_test_pl_user(uint32_t slot)   /* (.. where user slot sits in it) */
{
    uint16_t ids[SY_NFACT + SS_NUSER];
    uint32_t n = sy_app_list(0, 0, ids), i;
    for (i = 0; i < n; i++)
        if (ids[i] == SY_USER + slot)
            return (int32_t)i;
    return -1;
}
EXPORT void web_test_sound_set(uint32_t k)   /* (V0.87: the tests start on sound k, the boot's INIT before) */
{
    sy_patch_t p;
    sy_sound_patch(k, &p);
    sy_patch_load(&p);
    sy_sound = k;
    ui.edited = 0;
    sy_ui_touch();
}
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
EXPORT const char *web_test_palette_name(void) { return SY_PAL[ui.pal].name; }
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
/* SAVE: the name typed, the slot it goes to; the user slot the sound playing is from (-1 none); a user slot's name
 * ("" empty) and one of its patch's values (-32768: empty) */
EXPORT const char *web_test_save_name(void) { return ui.sv_name; }
EXPORT uint32_t web_test_save_slot(void) { return ui.sv_slot; }
EXPORT int32_t web_test_user_slot(void) { return sy_slot; }
EXPORT const char *web_test_slot_name(uint32_t s)
{
    const char *n = sy_app_slot_name(s);
    return n ? n : "";
}
EXPORT int32_t web_test_slot_value(uint32_t s, uint32_t id)
{
    uint8_t pk[SP_PACK_MAX];
    sy_patch_t p;
    int n = ss_user_get(s, pk, sizeof pk);
    return n > 0 && sp_unpack(&p, pk, (uint32_t)n) && id < SP_COUNT ? p.p[id] : -32768;
}
/* BENCH's row i (the emulator draws none: no clock of its own; 8 .. 15: RUN not yet run) at the widest figures a unit
 * shows: its width, in pixels, less the room for it (x 164 to the margin at 236; above 0: it runs past); sy_bench as it
 * was after */
EXPORT int32_t web_test_bench_fit(uint32_t i)
{
    char t[48];
    int32_t w;
    __typeof__(sy_bench) was = sy_bench;
    __typeof__(sy_g.shed) shed = sy_g.shed;
    sy_g.shed = 9999u;
    sy_bench.known = 1;
    sy_bench.half_us = sy_bench.half_max_us = 9999u;   /* (a half: 2.9 ms at most) */
    sy_bench.late = 99999u;
    sy_bench.frame_us = sy_bench.frame_max_us = 999999u;
    sy_bench.gap_max_us = 999999u;
    sy_bench.step_max_us = 99999u;
    sy_bench.build_max_ms = 99999u;
    sy_bench.erase_max_us = sy_bench.prog_max_us = 99999u;
    sy_bench.stack_isr = 7936u;
    sy_bench.stack_main = 12032u;
    sy_bench.spare_hit = 4u;
    sy_bench.run = (uint8_t)(i >= 8u ? 0u : 2u);
    sy_bench.run_int_us = sy_bench.run_flt_us = 99999u;
    sy_bench.gap_save_max_us = 999999u;
    {
        float v, pk;
        ui_bench_row(i % 8u, &v, &pk, t);
        w = sy_floori(lk_tw(T9B, t, 0) + 0.999f) - 72;
    }
    sy_bench = was;
    sy_g.shed = shed;
    return w;
}
/* the pages whose name does not end within its field (the header would run on into the knobs' ids), 0 none */
EXPORT uint32_t web_test_page_names_bad(void)
{
    uint32_t p, k, n = 0;
    for (p = 0; p < UI_NPG; p++) {
        for (k = 0; k < sizeof UI_PG[p].name && UI_PG[p].name[k]; k++)
            ;
        n += k == sizeof UI_PG[p].name;
    }
    return n;
}
/* the FX chain: slot k's instance + 1 (0 none); the chain view's slot and type picked */
EXPORT int32_t web_test_chain(uint32_t k) { return k < NFXC ? sy_patch.p[SP_FXC1 + k] : 0; }
EXPORT uint32_t web_test_cslot(void) { return ui_cslot(); }
EXPORT uint32_t web_test_ctype(void) { return ui.ctype; }
/* the knob turning (0xFF none), whether a shape before the turn is kept (the ghost), HOME's scope captures taken */
EXPORT uint32_t web_test_act(void) { return ui.act_k; }
EXPORT uint32_t web_test_ghost(void) { return ui_ghost() != 0; }
EXPORT uint32_t web_test_scope(void) { return sy_scope.frames; }

#ifdef LK_LINT
/* ------------------------------------------------------------------------------------------ the check --- */
/* web_lint: the screen as it is now, drawn twice into buffers of its own (nothing sent): with its text and without.
 * Every pixel the text changed is checked against what lies under it without the text, and its neighbours: only the
 * ground, the text's own ground (a chip's, a flag's) or a blend of the two may be there; the texts' ink boxes against
 * each other; a text past the screen's edges (2 px); a text in MID or LINE (V0.67: labels in LAB). The handles'
 * numbers are their own parts (not checked). web_lint_report: each finding, a line */
typedef struct {
    int16_t x0, y0, x1, y1;
    uint16_t fg, bg;
    uint8_t part, bad;
    char s[24];
} web_lt_t;
static web_lt_t web_lt[400];
static uint32_t web_lt_n, web_lt_on;
static uint16_t web_lint_full[240 * 240], web_lint_bare[240 * 240];
static uint8_t web_lint_mask[240 * 240];
static char web_lint_txt[4096];
static void lk_lint_text(const int32_t *b, uint32_t t, const char *s, uint16_t fg, uint16_t bg)
{
    uint32_t i;
    web_lt_t *r;
    (void)t;
    if (!web_lt_on)
        return;
    for (i = 0; i < web_lt_n; i++)                  /* (a text across two strips: drawn twice) */
        if (web_lt[i].x0 == b[0] && web_lt[i].y0 == b[1] && web_lt[i].x1 == b[2] && web_lt[i].y1 == b[3] &&
            !strncmp(web_lt[i].s, s, sizeof web_lt[i].s - 1u))
            return;
    if (web_lt_n >= sizeof web_lt / sizeof web_lt[0])
        return;
    r = &web_lt[web_lt_n++];
    r->x0 = (int16_t)b[0];
    r->y0 = (int16_t)b[1];
    r->x1 = (int16_t)b[2];
    r->y1 = (int16_t)b[3];
    r->fg = fg;
    r->bg = bg;
    r->part = lk_lint_part;
    r->bad = 0;
    strncpy(r->s, s, sizeof r->s - 1u);
    r->s[sizeof r->s - 1u] = 0;
}
static void web_lint_draw(uint16_t *to)
{
    uint32_t s;
    for (s = 0; s < 240u; s += 80u) {
        lk_begin(0, (int32_t)s, 240u, 80u);
        ui_page();
        memcpy(to + s * 240u, cv_px, 240u * 80u * 2u);
    }
}
static int web_lint_ok(uint16_t q, uint16_t g, uint16_t b)   /* q: g, b or a blend of the two (RGB565, swapped) */
{
    int32_t c, lo = 0, hi = 256;
    q = swap16(q);
    for (c = 0; c < 3; c++) {
        int32_t sh = c == 0 ? 11 : c == 1 ? 5 : 0, m = c == 1 ? 63 : 31;
        int32_t qv = (q >> sh) & m, gv = (g >> sh) & m, bv = (b >> sh) & m, d = bv - gv, t0, t1;
        if (d == 0) {
            if (qv < gv - 1 || qv > gv + 1)
                return 0;
            continue;
        }
        t0 = ((qv - gv) * 256 - 256 * (d > 0 ? 1 : -1)) / d;   /* (one step of the channel either way) */
        t1 = ((qv - gv) * 256 + 256 * (d > 0 ? 1 : -1)) / d;
        if (t0 > t1) {
            int32_t x = t0;
            t0 = t1;
            t1 = x;
        }
        lo = t0 > lo ? t0 : lo;
        hi = t1 < hi ? t1 : hi;
    }
    return lo <= hi + 24;
}
EXPORT uint32_t web_lint(void)
{
    uint32_t p, i, j, over = 0, tt = 0, edge = 0, mid = 0;
    char *o = web_lint_txt;
    web_lint_txt[0] = 0;
    memset(web_lint_mask, 0, sizeof web_lint_mask);
    web_lt_n = 0;
    ui_frame_n++;
    web_lt_on = 1;
    web_lint_draw(web_lint_full);
    web_lt_on = 0;
    lk_lint_notext = 1;
    web_lint_draw(web_lint_bare);
    lk_lint_notext = 0;
    for (p = 0; p < 240u * 240u; p++) {
        int32_t x = (int32_t)(p % 240u), y = (int32_t)(p / 240u), dx, dy, k = -1, bad = 0;
        if (web_lint_full[p] == web_lint_bare[p])
            continue;
        for (i = web_lt_n; i-- > 0;)                /* (the last text drawn there) */
            if (x >= web_lt[i].x0 && x < web_lt[i].x1 && y >= web_lt[i].y0 && y < web_lt[i].y1) {
                k = (int32_t)i;
                break;
            }
        if (k < 0 || web_lt[k].part)
            continue;
        for (dy = -1; dy <= 1 && !bad; dy++)
            for (dx = -1; dx <= 1 && !bad; dx++) {
                int32_t qx = x + dx, qy = y + dy;
                if (qx < 0 || qy < 0 || qx >= 240 || qy >= 240)
                    continue;
                bad = !web_lint_ok(web_lint_bare[qy * 240 + qx], sk.ground, web_lt[k].bg);
            }
        if (bad) {
            web_lint_mask[p] = 1;
            web_lt[k].bad = 1;
            over++;
        }
    }
    for (i = 0; i < web_lt_n; i++) {
        const web_lt_t *a = &web_lt[i];
        if (a->bad && o < web_lint_txt + sizeof web_lint_txt - 64u)
            o += sprintf(o, "OVER  '%s' at %d,%d\n", a->s, a->x0, a->y0);
        if (!a->part && (a->x0 < 2 || a->x1 > 238 || a->y0 < 0 || a->y1 > 240)) {
            edge++;
            if (o < web_lint_txt + sizeof web_lint_txt - 64u)
                o += sprintf(o, "EDGE  '%s' %d..%d, %d..%d\n", a->s, a->x0, a->x1, a->y0, a->y1);
        }
        if (!a->part && (a->fg == sk.mid || a->fg == sk.line) && a->fg != sk.ink) {
            mid++;
            if (o < web_lint_txt + sizeof web_lint_txt - 64u)
                o += sprintf(o, "DIM   '%s' at %d,%d\n", a->s, a->x0, a->y0);
        }
        for (j = i + 1u; j < web_lt_n; j++) {
            const web_lt_t *b = &web_lt[j];
            int32_t x0 = a->x0 > b->x0 ? a->x0 : b->x0, x1 = a->x1 < b->x1 ? a->x1 : b->x1;
            int32_t y0 = a->y0 > b->y0 ? a->y0 : b->y0, y1 = a->y1 < b->y1 ? a->y1 : b->y1, x, y;
            if (x0 >= x1 || y0 >= y1 || (a->part && b->part))
                continue;
            tt++;
            for (y = y0; y < y1; y++)
                for (x = x0; x < x1; x++)
                    web_lint_mask[y * 240 + x] = 2;
            if (o < web_lint_txt + sizeof web_lint_txt - 80u)
                o += sprintf(o, "TEXT  '%s' x '%s' at %d,%d\n", a->s, b->s, x0, y0);
        }
    }
    for (i = 0; i < 4u; i++)                        /* the handles against each other (their discs: 7 px) */
        for (j = i + 1u; j < 4u; j++)
            if ((pg.has >> i & 1u) && (pg.has >> j & 1u) && sy_absf(pg.hx[i] - pg.hx[j]) < 15.0f &&
                sy_absf(pg.hy[i] - pg.hy[j]) < 15.0f) {
                tt++;
                if (o < web_lint_txt + sizeof web_lint_txt - 64u)
                    o += sprintf(o, "KNOBS %u and %u at %d,%d\n", i + 1u, j + 1u, (int)pg.hx[i], (int)pg.hy[i]);
            }
    return over + 1000u * tt + 100000u * edge + 10000000u * mid;
}
EXPORT const char *web_lint_report(void) { return web_lint_txt; }
EXPORT uint8_t *web_lint_px(void) { return web_lint_mask; }
EXPORT uint16_t *web_lint_screen(void) { return web_lint_full; }
#endif
/* HOME's bloom: f 0 how open, 1 its swirl (thousandths) */
EXPORT int32_t web_test_bloom(uint32_t f) { return sy_floori((f ? ui_twist : ui_open) * 1000.0f + 0.5f); }
/* V0.7, the chords: f 0 its count of notes, 1 its root, 2 the bass (255 none), 3 HARP's / the latched chord built, 10 + i
 * note i; the chord's name; a setting set (sy_chord.c chd: 0 mode, 1 FREE's type, 2 SPREAD, 3 voicing, 4 the ARP
 * on, 5 its style (0 ARP, 1 PATTERN, 2 HARP), 6 the style's rate, 7 its move, 8 its feel, 9 bass, 10 its octaves, 11
 * its level, 12 MIDI in, 13 out, 14 the chords' channel, 15 the bass's, 16 KEY's shape, 17 GATE, 18 SWING, 19 LATCH,
 * 20 LENGTH, 21 CLOCK IN, 22 STRUM, 23 its DIRECTION, 24 HUMAN, 25 TAPER) and read back; the USB MIDI sent (its count, packet i); voice i's note (-1 off
 * or released) */
EXPORT int32_t web_test_chord(uint32_t f)
{
    return f == 0u ? chs.n : f == 1u ? chs.root : f == 2u ? chs.bass : f == 3u ? chs.built
           : f >= 10u && f < 10u + CH_MAXN ? chs.note[f - 10u] : -1;
}
EXPORT const char *web_test_chord_name(void)
{
    static char t[24];
    return sy_chord_name(t);
}
EXPORT int32_t web_test_chd(uint32_t f, int32_t v)
{
    uint8_t *p[26] = {&chd.mode, &chd.ftype, &chd.spread, &chd.voicing, &chd.perf, &chd.style, 0, 0, 0, &chd.bass,
                      &chd.boct, &chd.blvl, 0, 0, 0, 0, &chd.ktype,   /* (12-15: SCL's MIDI until V0.87) */
                      &chd.gate, &chd.swing, &chd.latch, &chd.len, &ch_clock, &chd.strum, &chd.sdir, &chd.human,
                      &chd.taper};
    uint8_t *q = f == 6u ? &chd.rate[chd.style % PF_N] : f == 7u ? &chd.move[chd.style % PF_N]
                 : f == 8u ? &chd.feel[chd.style % PF_N] : f == 13u ? &lpp.trk[lpp.sel % LP_NTRK].out
                 : f == 14u ? &lpp.trk[lpp.sel % LP_NTRK].ch : f < 26u ? p[f] : 0;   /* (V0.8: USB OUT and its channel: the
                                                                                     * selected track's) */
    if (!q)
        return -1;
    if (v >= 0) {
        *q = (uint8_t)v;
        chd.gen++;
        ui.dirty = D_ALL;
    }
    return *q;
}
EXPORT uint32_t web_test_mout_n(void) { return web_mout_n; }
EXPORT uint32_t web_test_mout(uint32_t i) { return web_mout[i % 256u]; }
EXPORT uint32_t web_test_mout_t(uint32_t i) { return web_mout_t[i % 256u]; }
/* V0.75, ARP's step i: f 0 its kind (PLAY TIE REST), 1 LEVEL, 2 OCTAVE (0 .. 4: -2 .. +2), 3 RATCHET, 4 CHANCE, 5
 * SLIDE; v >= 0 sets it; read back. The transport: f 0 running, 1 its place (thousandths of a quarter note), 2 MIDI's
 * clocks counted, 3 the step that played + 1, 4 the beats, 5 the tempo (hundredths of a quarter note a minute), 6 the
 * tempo from MIDI's clock, 7 the step picked (STEPS), 8 the audio side's time (samples) */
EXPORT int32_t web_test_step(uint32_t i, uint32_t f, int32_t v)
{
    uint8_t *q = f == 0u ? chd.st : f == 1u ? chd.lvl : f == 2u ? chd.oct : f == 3u ? chd.rat : f == 4u ? chd.chn : chd.sld;
    if (i >= AR_NSTEP || f > 5u)
        return -1;
    if (v >= 0) {
        q[i] = (uint8_t)v;
        ui.dirty = D_ALL;
    }
    return q[i];
}
EXPORT int32_t web_test_arp(uint32_t f)
{
    return f == 0u ? cht.run : f == 1u ? sy_floori(cht.tq * 1000.0f) : f == 2u ? cht.pulses : f == 3u ? chs.at
           : f == 4u ? chs.beat : f == 5u ? sy_floori(sy_tempo * 100.0f + 0.5f) : f == 6u ? sy_app_clock()
           : f == 7u ? ui.astep : f == 8u ? (int32_t)cht.t : -1;
}
EXPORT int32_t web_test_voice_note(uint32_t i)
{
    return i < SY_NVOICE && sy_v[i].on && sy_v[i].key ? sy_v[i].note : -1;
}
/* V0.8, the looper (sy_loop.c): f 0 the track selected (set: lp_select), 1 the scene picked, 2 QUANTIZE, 3 COUNT-IN,
 * 4 CLICK, 5 its LEVEL, 6 LAUNCH, 7 the project, 8 SONG on (v >= 0 sets them); 10 recording (LR_*), 11 the track
 * recording, 12 events free, 13 the UI's change count, 14 the click sounding (samples left), 15 the transport running,
 * 16 the bar it is in, 17 UNDO's events kept, 18 this pass's, 19 the pass before's, 20 SONG's slot playing; read
 * back */
/* V0.86: the battery's reading and USB power (v < 0: as they are); the screen: 0 on .. 4 (SCR_*), 1 dark */
EXPORT int32_t web_test_power(int32_t batt, int32_t usbp)
{
    if (batt >= 0)
        sy_batt_raw = batt;
    if (usbp >= 0)
        web_usb = usbp;
    return ui_batt();
}
EXPORT uint32_t web_test_scr(void) { return ui.scr_st; }
/* PRESETS' list (V0.86): f 0 open, 1 the one picked (its place), 2 SAVE + OCT-'s delete asked (the user slot + 1) */
EXPORT int32_t web_test_pl(uint32_t f)   /* (3: the kits' list) */
{
    return f == 0u ? ui.pl_open : f == 1u ? ui.pl_pos : f == 2u ? ui.pl_ask : f == 3u ? ui.pl_kind : f == 4u ? ui.pl_osc : f == 5u ? ui.pl_dcat : -1;
}
EXPORT const char *web_test_kit_name(void) { return dkk.name; }
/* V0.87 (step 2): row r's model (f 0), variant (1), preset (2: dk_pid); the user's drum presets (3), kits (4) used;
 * the selected clip's hits on row r (5) */
EXPORT int32_t web_test_dkm(uint32_t r, uint32_t f)
{
    uint32_t v, m = dk_model(r % DK_N, &v), i, n = 0;
    if (f == 0u || f == 1u)
        return (int32_t)(f ? v : m);
    if (f == 2u)
        return *dk_pid(r);
    if (f == 3u || f == 4u) {
        for (i = 0; i < (f == 3u ? DK_NUP : DK_NUK); i++)
            n += (f == 3u ? dk_up_name(i) : dk_uk_name(i)) != 0;
        return (int32_t)n;
    }
    if (f == 5u) {
        const lp_clip_t *c = lp_cur_clip();
        for (i = 0; c->len && i < c->len / LP_STEP; i++)
            n += lp_grid_at(c, r, i) != LP_END;
        return (int32_t)n;
    }
    return -1;
}
EXPORT const char *web_test_dk_name(uint32_t r) { return dk_rname(r); }
EXPORT uint32_t web_test_lcd_off(void) { return web_lcd_off; }
EXPORT int32_t web_test_lp(uint32_t f, int32_t v)
{
    if (v >= 0 && f <= 8u) {
        if (f == 0u) lp_select((uint32_t)v);
        else if (f == 1u) lpp.scn = (uint8_t)(v % LP_NSCN);
        else if (f == 2u) lpg.quant = (uint8_t)(v % LQ_N);
        else if (f == 3u) lpg.cin = v != 0;
        else if (f == 4u) lpg.click = (uint8_t)(v % LC_N);
        else if (f == 5u) lpg.clvl = (uint8_t)sy_clampi(v, 0, 10);
        else if (f == 6u) lpg.launch = (uint8_t)(v % LL_N);
        else if (f == 7u) lpg.prj = (uint8_t)(v % LP_NPRJ);
        else lpp.song_on = v != 0;
        ui.dirty = D_ALL;
    }
    switch (f) {
    case 0: return lpp.sel;
    case 1: return lpp.scn;
    case 2: return lpg.quant;
    case 3: return lpg.cin;
    case 4: return lpg.click;
    case 5: return lpg.clvl;
    case 6: return lpg.launch;
    case 7: return lpg.prj;
    case 8: return lpp.song_on;
    case 10: return lpr.st;
    case 11: return lpr.trk;
    case 12: return lp_nfree;
    case 13: return lpx.seq;
    case 14: return (int32_t)lpx.ck_n;
    case 15: return cht.run;
    case 16: return lpx.bar;
    case 17: return lpr.nundo;
    case 18: return lpr.npass;
    case 19: return lpr.nlast;
    case 20: return lpx.sslot;
    default: return -1;
    }
}
/* track t: f 0 TYPE, 1 CHANNEL (out), 2 IN, 3 OUT, 4 VOLUME, 5 PAN, 6 MUTE, 7 SOLO (v >= 0 sets them); 10 the scene
 * playing (255 none), 11 the one launched (255 none, 254 stop), 12 its place in its clip (ticks), 13 passes played,
 * 14 its gain left x 1000, 15 right; read back */
EXPORT int32_t web_test_lp_trk(uint32_t t, uint32_t f, int32_t v)
{
    lp_trk_t *k = &lpp.trk[t % LP_NTRK];
    uint8_t *b = f == 0u ? &k->type : f == 1u ? &k->ch : f == 2u ? &k->cin : f == 3u ? &k->out : f == 4u ? &k->vol
                 : f == 5u ? &k->pan : f == 6u ? &k->mute : f == 7u ? &k->solo : 0;
    t %= LP_NTRK;
    if (f == 0u && v >= 0) {                        /* (V0.85: TYPE as its knob sets it: DRUMS turns the clips' notes) */
        lp_type_set(t, (uint32_t)v % LT_N, k->out);
        ui_l_follow();
        ui.dirty = D_ALL;
        return k->type;
    }
    if (b) {
        if (v >= 0) {
            *b = (uint8_t)v;
            ui.dirty = D_ALL;
        }
        return *b;
    }
    return f == 10u ? lps[t].play : f == 11u ? lps[t].next : f == 12u ? lps[t].pos : f == 13u ? lps[t].passes
           : f == 14u ? (int32_t)(sy_tgain[t][0] * 1000.0f + 0.5f) : f == 15u ? (int32_t)(sy_tgain[t][1] * 1000.0f + 0.5f)
           : -1;
}
/* clip t, s: f 0 its length in ticks (v >= 0 sets it), 1 its events, 2 its notes, 3 the parameters it moves, 4 its
 * SPEED (V0.86: LS_*, v >= 0 sets it); read */
EXPORT int32_t web_test_lp_clip(uint32_t t, uint32_t s, uint32_t f, int32_t v)
{
    lp_clip_t *c = &lpp.clip[t % LP_NTRK][s % LP_NSCN];
    uint16_t ids[8];
    if (f == 0u && v >= 0) {
        c->len = (uint16_t)v;
        lps[t % LP_NTRK].resync = 1;
    }
    if (f == 4u && v >= 0)
        lp_speed_set(t, s, (uint32_t)v);
    return f == 0u ? c->len : f == 1u ? c->n : f == 2u ? (int32_t)lp_note_n(c) : f == 3u ? (int32_t)lp_moves_of(c, ids, 8u)
           : f == 4u ? lpp.speed[t % LP_NTRK][s % LP_NSCN] : -1;
}
/* clip t, s's note i (in time): f 0 its note, 1 velocity (its FRESH bit too: + 128), 2 tick, 3 length; -1 none */
EXPORT int32_t web_test_lp_note(uint32_t t, uint32_t s, uint32_t i, uint32_t f)
{
    uint32_t k = lp_note_at(&lpp.clip[t % LP_NTRK][s % LP_NSCN], i);
    if (k == LP_END)
        return -1;
    return f == 0u ? lp_ev[k].a : f == 1u ? lp_ev[k].b : f == 2u ? (int32_t)lp_et(k) : f == 3u ? lp_ev[k].len : -1;
}
/* clip t, s's moves of parameter id: the i-th's tick (f 0) or value (f 1); -1 none */
EXPORT int32_t web_test_lp_move(uint32_t t, uint32_t s, uint32_t id, uint32_t i, uint32_t f)
{
    uint32_t k;
    for (k = lpp.clip[t % LP_NTRK][s % LP_NSCN].head; k != LP_END; k = lp_ev[k].next)
        if ((lp_ev[k].t & LP_MOVE) && lp_ev[k].len == id && !i--)
            return f ? (int16_t)(lp_ev[k].a | (uint16_t)lp_ev[k].b << 8) : (int32_t)lp_et(k);
    return -1;
}
/* a note into clip t, s (an empty one made a bar long first): its tick, length, note, velocity; its event (-1: full) */
EXPORT int32_t web_test_lp_put(uint32_t t, uint32_t s, uint32_t tick, uint32_t len, uint32_t note, uint32_t vel)
{
    lp_clip_t *c = &lpp.clip[t % LP_NTRK][s % LP_NSCN];
    uint32_t e = lp_alloc();
    if (e == LP_END)
        return -1;
    if (!c->len)
        c->len = (uint16_t)lp_bar_ticks();
    lp_ev[e].t = (uint16_t)(tick & LP_MAXT);
    lp_ev[e].len = (uint16_t)len;
    lp_ev[e].a = (uint8_t)note;
    lp_ev[e].b = (uint8_t)vel;
    lp_insert(c, e);
    lps[t % LP_NTRK].resync = 1;
    lpx.seq++;
    ui.dirty = D_ALL;
    return (int32_t)e;
}
/* a move into clip t, s (an empty one made a bar long first): its tick, the parameter, its value; its event */
EXPORT int32_t web_test_lp_mput(uint32_t t, uint32_t s, uint32_t tick, uint32_t id, int32_t v)
{
    lp_clip_t *c = &lpp.clip[t % LP_NTRK][s % LP_NSCN];
    uint32_t e = lp_alloc();
    if (e == LP_END)
        return -1;
    if (!c->len)
        c->len = (uint16_t)lp_bar_ticks();
    lp_ev[e].t = (uint16_t)(LP_MOVE | (tick & LP_MAXT));
    lp_ev[e].len = (uint16_t)id;
    lp_ev[e].a = (uint8_t)((uint16_t)v & 0xFFu);
    lp_ev[e].b = (uint8_t)((uint16_t)v >> 8);
    lp_insert(c, e);
    lps[t % LP_NTRK].resync = 1;
    ui.dirty = D_ALL;
    return (int32_t)e;
}
/* SONG's slot i: f 0 its scene (255: the song ends there), 1 its repeats (v >= 0 sets them); read back */
EXPORT int32_t web_test_lp_song(uint32_t i, uint32_t f, int32_t v)
{
    uint8_t *b = f ? &lpp.srep[i % LP_NSONG] : &lpp.song[i % LP_NSONG];
    if (v >= 0) {
        *b = (uint8_t)v;
        ui.dirty = D_ALL;
    }
    return *b;
}
EXPORT void web_test_lp_req(uint32_t r) { lpr.req = (uint8_t)r; }   /* 1 REC tapped, 2 UNDO PASS, 3 STOP ALL */
EXPORT void web_test_lp_launch(uint32_t t, uint32_t s) { lp_launch(t, s); }   /* s: a scene, 254 stop */
/* V0.85, the kit (sy_drum.c): f 0 .. 5 drum lane's TUNE (+ 24: 0 .. 48), DECAY, TONE, its own, LEVEL, PAN; 10 ACCENT,
 * 11 VELOCITY, 12 the duck's AMOUNT, 13 ATTACK, 14 RELEASE, 15 SHAPE (v >= 0 sets them); 20 lane sounding, 21 the
 * duck's gain x 1000, 22 the last lane hit (255 none), 23 the duck's state, 24 row lane's hits (0 .. 8); read back */
EXPORT int32_t web_test_kit(uint32_t lane, uint32_t f, int32_t v)
{
    dk_lane_t *k = &dkk.l[lane % DK_N];
    uint8_t *b = f == 1u ? &k->decay : f == 2u ? &k->tone : f == 3u ? &k->chr : f == 4u ? &k->level : f == 5u ? &k->pan
                 : f == 10u ? &dkk.accent : f == 11u ? &dkk.vsens : f == 12u ? &dkk.amount : f == 13u ? &dkk.attack
                 : f == 14u ? &dkk.release : f == 15u ? &dkk.shape : 0;
    if (f == 0u) {
        if (v >= 0) {
            k->tune = (int8_t)sy_clampi(v - 24, -24, 24);
            ui.dirty = D_ALL;
        }
        return k->tune + 24;
    }
    if (b) {
        if (v >= 0) {
            *b = (uint8_t)v;
            dk_sane();
            ui.dirty = D_ALL;
        }
        return *b;
    }
    if (f >= 30u && f <= 38u) {                     /* (V0.86, step 2: lane's DRIVE TYPE, DRIVE, MIX, EQ LOW, MID, HIGH, */
        uint8_t *x = &dkk.fx[lane % DK_N].dtype + (f - 30u);   /* FREQ, SHAPE ATTACK, SUSTAIN; the kit's SOURCE, */
        if (v >= 0) {                                         /* HOLD, ACCENT's TONE, COMP's four, the factory kit */
            *x = (uint8_t)v;                                  /* it came from, edited since) */
            dk_sane();
        }
        return *x;
    }
    if (f >= 40u && f <= 46u) {
        uint8_t *x = f == 40u ? &dkk.dsrc : f == 41u ? &dkk.dhold : f == 42u ? &dkk.atone : &dkk.comp[f - 43u];
        if (v >= 0) {
            *x = (uint8_t)v;
            dk_sane();
        }
        return *x;
    }
    return f == 20u ? dkv[lane % DK_N].on : f == 21u ? (int32_t)(dkd.g * 1000.0f + 0.5f) : f == 22u ? dk_last
           : f == 23u ? dkd.st : f == 24u ? (int32_t)dk_nhit[lane % DK_ROWS] : f == 47u ? dkk.kit : f == 48u ? ui.kedited
           : f == 49u ? (int32_t)ui.krow : -1;   /* (24: row lane's hits so far) */
}
EXPORT void web_test_drum_hit(uint32_t lane, uint32_t trk, uint32_t vel, uint32_t acc) { sy_drum_hit(lane, trk, vel, (int)acc); }
/* the drum grid of the selected clip (sy_loop.c): f 0 step s of row toggled (1 in, 0 out, -1 neither), 1 .. 4 its
 * hit's VELOCITY, ACCENT, CHANCE, RATCHET moved by v (0: read; -1 no hit), 5 a hit there (1, 0) */
EXPORT int32_t web_test_grid(uint32_t f, uint32_t row, uint32_t s, int32_t v)
{
    int32_t r;
    if (f == 0u)
        r = lp_grid_toggle(row, s, v > 0 ? (uint32_t)v : 100u);
    else if (f <= 4u)
        r = lp_grid_attr(row, s, f - 1u, v);
    else
        r = lp_grid_at(lp_cur_clip(), row, s) != LP_END;
    ui.dirty = D_ALL;
    return r;
}
EXPORT int32_t web_test_ovr(uint32_t id) { return id < SP_COUNT && sp_ovr_on[id] ? sp_ovr_v[id] : -32768; }
EXPORT int32_t web_test_voice_trk(uint32_t i) { return i < SY_NVOICE && sy_v[i].on ? sy_v[i].trk : -1; }
EXPORT const char *web_test_knob_name(uint32_t k)   /* V0.86: knob k's name on the page shown ("" none; the docs' table) */
{
    static char n[40];
    char v[SP_TEXT_N + 8];
    const char *s = k < 4u && UI_PG[ui.page].id[k] >= 0 ? ui_knob_nv(k, v) : "";
    uint32_t i;
    for (i = 0; s && s[i] && i + 1u < sizeof n; i++)
        n[i] = s[i];
    n[i] = 0;
    return n;
}
EXPORT void web_test_knob_touch(uint32_t id) { lp_knob(id); }   /* (sy_ui.c ui_knob's: a knob turned) */
EXPORT int32_t web_test_follow(uint32_t id) { return id < SP_COUNT ? sp_last.v[id] : -1; }
/* V0.8 BENCH, CORE 2 (sy_c2.c; the emulator has one core): f 0 the state, 1 ON, 2 LOAD, 3 TEST's figures as a unit
 * might show them after a test (v: core 1's microseconds a half; the rest made from it: for the page's render and its
 * check, never measured here), 4 the platform's clock (sy_bench.known: the page's footer). v < 0 reads; the value
 * after */
EXPORT int32_t web_test_c2(uint32_t f, int32_t v)
{
    if (f == 0) {
        if (v >= 0)
            c2.state = (uint8_t)((uint32_t)v % C2_NSTATE);
        return c2.state;
    }
    if (f == 1) {
        if (v >= 0)
            c2.on = v != 0;
        return c2.on;
    }
    if (f == 2) {
        if (v >= 0)
            c2.load = (uint8_t)v;
        return c2.load;
    }
    if (f == 3) {
        if (v >= 0) {
            uint32_t n = C2_SECS * C2_HALVES_S / C2_NPH, us = (uint32_t)v, t = FM1_TICKS_PER_US;
            c2.j1_n = c2.j0_n = n;
            c2.j1_sum = n * us * t, c2.j1_max = (us + us / 8u) * t;
            c2.j0_sum = n * (us - us / 20u) * t;
            c2.r_n[C2_IDLE] = c2.r_n[C2_SOLO] = c2.r_n[C2_PAR] = n;
            c2.r_sum[C2_IDLE] = c2.r_sum[C2_SOLO] = n * 1180u * t, c2.r_sum[C2_PAR] = n * 1215u * t;
            c2.wait_max = 140u * t;
            c2.halves = C2_SECS * C2_HALVES_S, c2.test = 0, c2.done = 1;
        }
        return (int32_t)c2_c1_us();
    }
    if (f == 4) {
        if (v >= 0)
            sy_bench.known = v != 0;
        return (int32_t)sy_bench.known;
    }
    return -1;
}
