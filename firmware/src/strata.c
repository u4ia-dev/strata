/* SPDX-License-Identifier: GPL-3.0-only
 * After Felucca's src/felucca.c (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0). */
/* STRATA: one compilation unit, as Felucca is built (the HAL is header-only; each file uses what the files above it
 * define, so the order matters). tools/build.py --strata compiles this one in place of felucca.c.
 * From Felucca's platform (docs/ARCHITECTURE.md §2), unchanged: the HAL, libc.c, lcd.c, gfx.c, panel.c, usb.c
 * (USB-MIDI, the USB audio input, the console's CDC, the SysEx frames), midi_uart.c (TRS MIDI IN), the flash driver
 * and ota.c (the M-UPGRADE update entry). Strata's: the sy_* engine and UI, sy_app.c, and the FM-1's glue: sy_hw.c
 * (the flash's and the update's hooks), sy_main.c (power-on, the guards, the main loop), sy_console.c. */

/* ---------------------------------------------------- build options --- */
#define FELUCCA_FLASH 1          /* the flash driver: Strata's presets, settings and the sound playing (sy_store.c) */
#define FELUCCA_OTA 1            /* the M-UPGRADE update entry: always, it is the way back to Felucca or the stock
                                  * firmware */
#define FELUCCA_OTA_DRYRUN 0
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1            /* the USB serial console (sy_console.c) */
#endif
#ifndef FELUCCA_UAC
#define FELUCCA_UAC 1            /* the USB audio input: what the DAC plays, 16-bit stereo, to the computer */
#endif
#define FELUCCA_UAC_TONE 0
#ifndef FELUCCA_UART
#define FELUCCA_UART 1           /* TRS MIDI IN on UART1 / PH8 */
#endif
#ifndef FELUCCA_ID
#define FELUCCA_ID "FM-1_900"    /* the package identity (build.py: the .fwsc's marker string): Felucca's, kept (§16) */
#endif
#define FELUCCA_CPU2 0           /* (Felucca's parts on the second core: none in Strata) */
#ifndef STRATA_CPU2
#define STRATA_CPU2 1            /* the second core built in (V0.8 BENCH: SETTINGS > CORE 2, OFF by default, sy_c2.c);
                                  * build.py STRATA_CPU2=0: none of it (no stack, waiting loop or mailbox) */
#endif
#define FM1_CPU1_USED STRATA_CPU2   /* (hal/fm1_sys.h: every reset holds it first) */
#define FM1_LED_Q 1              /* V0.87 (U9): the LED scan's tick in quarters, steady levels (hal/fm1_input.h) */
#define FM1_T5_PRD_V 150u        /* .. TIMER5 at 40 kHz, the tick every fourth (sy_main.c fm1_timer5_irq) */
#ifndef STRATA_VERSION
#define STRATA_VERSION "V0.875"                 /* the splash and the console (V0.8: the looper, CORE 2; V0.85 its drums;
                                                   * V0.86 the interface, V0.87 its changes); build.py --strata --release X.Y */
#endif
#if !FELUCCA_OTA || !FELUCCA_FLASH
#error "Strata keeps the update entry and the flash: they are the way back"
#endif

/* ------------------------------------------------------------- HAL --- */
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "fm1_flash.h"
#if STRATA_CPU2
#include "fm1_cpu1.h"                            /* (Felucca's mailbox, from X0X, Melodee and Jangada: sy_c2.c) */
#endif
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* a ring's slot stored before its index moves */
static volatile uint32_t fm1_ms;                 /* milliseconds (sy_main.c's TIMER5); usb.c declares it too */

/* ----------------------------------------------------- base, display --- */
#include "libc.c"
static void sy_lcd_idle(void);                   /* sy_main.c: the keys and MIDI read while a screen strip goes out */
#define LCD_IDLE() sy_lcd_idle()
#define LCD_BAUD 0u                              /* the panel's SPI: lsb / (BAUD + 1), 0 = 60 MHz (~44 MHz measured by
                                                  * fm1-amb, fm1-ctl and fm1-fomni, the FM-1 forks that run it so;
                                                  * ST7789 <= ~62 MHz). Felucca's 4 (12 MHz): a full redraw ~77 ms of
                                                  * SPI, the frames of V0.65's test 90 ms (V0.66; needs a hardware test) */
#include "lcd.c"
#define CV_MAX (240u * 80u)      /* the canvas: the STRATA UI draws its screen in three strips of 80 rows (sy_ui_draw.c;
                                  * V0.66's largest strip, the display's, was 240 x 114: 16,320 bytes of the pool more) */
#include "gfx.c"
static uint8_t fx_lowcut;                        /* (panel.c's Felucca settings hook: not Strata's) */
#include "panel.c"

/* -------------------------------------------------------- USB, MIDI --- */
/* usb.c, midi_uart.c and ota.c are built at -Os inside this -O2 unit (tools/strata_drivers.py), as Felucca and every
 * other Felucca-family firmware build them: built at -O2, Strata's V0.4 (2026-10-16) and OberVave (2026-10-13, its
 * tools/build.py) sent nothing usable on the bulk IN endpoints after any chip reset, while EP0 and both OUT endpoints
 * worked (docs/FLASHING.md, section 9). And usb.c's two SIE accesses (a CON1 write, then its MC_ACK) stay out of line,
 * Felucca's functions instruction for instruction, whatever the inliner decides: at -O2 they were inlined back to back
 * into the TIMER5 interrupt. These declarations give usb.c's definitions the attributes; usb.c stays Felucca's */
static void sie_wr(uint32_t r, uint32_t v) __attribute__((noinline, minsize));
static uint32_t sie_rd(uint32_t r) __attribute__((noinline, minsize));
#define HALF_FRAMES 128          /* usb.c's USB audio input: the frames a render writes (sy_app.c SY_HALF) */
#define UA_PRIME 168u            /* .. its ring's band, 64 frames (1.45 ms) more than Felucca's 104 / 80 / 128 (V0.66: */
#define UA_LO 144u               /* V0.65's test, 14 underruns while the load test ran: a render started late found */
#define UA_HI 192u               /* the ring empty); the USB recording 1.45 ms later, the band as wide */
#define UA_PRIME48 183u          /* .. at 48 kHz, the same in 48 kHz frames (Felucca's 113 / 87 / 139, + 70) */
#define UA_LO48 157u
#define UA_HI48 209u
#define FELUCCA_USB_PID 0x5354u /* Strata (u4ia): its USB id 1209:5354, its strings (usb.c; docs/RELEASE.md) */
#define STRATA_USB 1
#include "usb.c"
#if FELUCCA_UART
#include "midi_uart.c"
#endif

/* ---------------------------------------------------------- the synth --- */
static int st_read(uint32_t off, void *dst, uint32_t n);   /* the flash's hooks (sy_hw.c, below: they need the */
static int st_erase(uint32_t off);                          /* audio's ring and BENCH) */
static int st_prog(uint32_t off, const void *src, uint32_t n);
#define SY_TICKS() fm1_ticks()   /* the engine's admission (sy_engine.c): TIMER4, 24 MHz; a sample 544 ticks */
#define SY_TICKS_BLK (SY_BLK * 544u)   /* (44,117.6 Hz = 24 MHz / 544): a block, 17,408 */
#define SY_KEYS_ISR 1            /* the keys from TIMER5 while they only play (sy_app.c, sy_main.c; V0.66) */
#include "sy_core.h"
#include "sy_wt.c"
#include "sy_wtgen.c"
#include "sy_param.c"
#include "sy_osc.c"
#include "sy_env.c"
#include "sy_lfo.c"
#include "sy_mod.c"
#include "sy_filter.c"
#include "sy_voice.c"
#include "sy_fx.c"
#include "sy_drum.c"
#include "sy_engine.c"
#include "sy_text.c"
#include "sy_store.c"
#define SY_MIDI_OUT(pkt) midi_out_event(pkt)   /* the chords out over USB MIDI (usb.c's ring, the audio ISR its producer) */
#define SY_MIDI_POLL() midi_out_recover()      /* (V0.81: the ring's lost notes ended, a half) */
#include "sy_chord.c"
#include "sy_loop.c"                             /* (V0.8: the looper, on the chords' keys: a side a track) */
#include "sy_c2.c"                               /* (V0.8 BENCH: the second core measured) */
#include "sy_sounds.h"

/* --------------------------------------------------------------- UI --- */
#include "sy_gfx.c"
#include "sy_ui.c"
#include "sy_ui_draw.c"
#include "sy_ui_fx.c"
#include "sy_ui_save.c"
#include "sy_ui_chord.c"
#include "sy_ui_loop.c"
#include "sy_ui_drum.c"                           /* (V0.85: the drums' pages) */

/* -------------------------------------------------------------- app --- */
static volatile uint8_t sy_in_audio;             /* the audio interrupt is rendering (sy_main.c's TIMER5 nests in it) */
static volatile uint32_t t5_nested_ticks;        /* .. TIMER4 ticks TIMER5 spent nested in this half's render */
#define SY_ISR_ENTER() (sy_in_audio = 1u, t5_nested_ticks = 0u)
#define SY_ISR_EXIT() (sy_in_audio = 0u)
#if FELUCCA_UAC
#define SY_HALF_START() uac_render_start()
#define SY_BLOCK_TAP(o, n) uac_tap(o, n)
#else
#define SY_HALF_START()
#define SY_BLOCK_TAP(o, n)
#endif
#define SY_NESTED_TICKS() t5_nested_ticks
#include "sy_app.c"
#include "sy_testproj.c"                        /* (V0.87.1: the test projects) */

/* ------------------------------------------------- update, the FM-1 --- */
#include "ota.c"
#include "sy_hw.c"
#if FELUCCA_CDC
static void cdc_task(void);                      /* sy_console.c */
#endif
#include "sy_main.c"
#if FELUCCA_CDC
#include "sy_console.c"
#endif
