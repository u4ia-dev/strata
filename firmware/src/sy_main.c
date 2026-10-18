/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone
 * Power-on, the guards, the UBOOT entries, the update entry, the TIMER5 tick and the crash screen follow Felucca's
 * src/main.c, MASTER its src/master.c (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments,
 * GPL-3.0). */
/* STRATA on the FM-1: power-on and the main loop (docs/ARCHITECTURE.md §3).
 * Power-on (fm1_cstart, then fm1_main), as Felucca's: the watchdog and the boot-loop guard first (two boots in a row
 * that die in their first 30 s go to UBOOT, the chip's own USB boot, where a full flash backup is written back); the
 * fault vectors and the hardware guards; RAM. Then the flash (the expected part, else none of it is touched; the
 * update loader's staging, if left over, cleared); the screen; the panel, polled before any interrupt: HOME held, or
 * a boot before this one that died, and the sound that was playing is not restored; the app (sy_app.c); USB, TRS
 * MIDI, the TIMER5 tick; the vectors write-protected; the interrupts on.
 * The main loop: the watchdog; the update entry (the M-UPGRADE handshake answers FELUCCA_ID; the upgrade command runs
 * ota.c's session behind the engine's hush); the UBOOT entries (OCT- + OCT+ held 5 s, a countdown in the footer from
 * 2 s; the SysEx soft key; the console's `uboot yes`); MIDI in, USB and TRS, to sy_app.c; sy_app_ms once each
 * millisecond, sy_app_frame every 15 ms; MASTER; the console; BENCH's timings and its RUN. */
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];
extern char _ustack_top[];

#define SY_CRASH_BG 0x5000u                      /* the crash screen's: fixed, outside the palettes */
#define SY_CRASH_INK 0xFFFFu

/* ------------------------------------------------------------------------------------------- TIMER5 --- */
/* 10 kHz, above the audio's priority (Felucca's main.c): the key / LED scan and the milliseconds always; the USB and
 * the TRS polls at 2 kHz when not nested in the audio's render; nested, only the USB audio input's packet (it cannot
 * wait for a render), and the time spent there is taken off the render's load */
void fm1_timer5_irq(void)
{
    static uint32_t sub, owed;
    uint32_t t0 = fm1_ticks(), usb_due = sub % 5u == 0u;
    fm1_timer5_ack();
    fm1_input_tick();
    {   /* milliseconds from the 24 MHz TIMER4 (robust to a late tick) */
        static uint32_t last, acc;
        acc += t0 - last;
        last = t0;
        while (acc >= 1000u * FM1_TICKS_PER_US) {
            acc -= 1000u * FM1_TICKS_PER_US;
            fm1_ms++;
        }
    }
    if (usb_due)
        owed |= 1u;
    if (sub % 5u == 2u)
        owed |= 2u;
    if (++sub == 10u)
        sub = 0;
    if (sy_in_audio) {
#if FELUCCA_UAC
        if (usb_due)
            uac_service();
#endif
        t5_nested_ticks += fm1_ticks() - t0;
        return;
    }
    if (owed & 1u)
        usb_poll();
#if FELUCCA_UART
    if (owed & 2u)
        uart_midi_poll();
#endif
    owed = 0;
}
extern void isr_timer5(void);

/* ----------------------------------------------------------------------------------------- the crash --- */
static void sy_hex(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}

/* a fault (hal/fm1_irq.h): the record on the screen for 4 s, then a restart (a second one in the first 30 s: UBOOT) */
static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t t0;
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, SY_CRASH_BG);
    draw_text_line(0, 8, 240, &AF_M, "STRATA CRASH", SY_CRASH_INK, SY_CRASH_BG, 1);
    sy_hex(b, c->vec);
    draw_text_line(10, 40, 220, &AF_M, b, SY_CRASH_INK, SY_CRASH_BG, 0);
    sy_hex(b, c->pc);
    draw_text_line(10, 60, 220, &AF_M, b, SY_CRASH_INK, SY_CRASH_BG, 0);
    sy_hex(b, c->emu);
    draw_text_line(10, 84, 220, &AF_M, b, SY_CRASH_INK, SY_CRASH_BG, 0);
    sy_hex(b, c->dbg);
    draw_text_line(10, 102, 220, &AF_M, b, SY_CRASH_INK, SY_CRASH_BG, 0);
    sy_hex(b, c->rets);
    draw_text_line(10, 120, 220, &AF_M, b, SY_CRASH_INK, SY_CRASH_BG, 0);
    draw_text_line(0, 160, 240, &AF_S, "A PHOTO OF THIS HELPS", SY_CRASH_INK, SY_CRASH_BG, 1);
    draw_text_line(0, 180, 240, &AF_S, "RESTARTING IN 4 S", SY_CRASH_INK, SY_CRASH_BG, 1);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

/* ----------------------------------------------------------------------------------------- the stacks --- */
/* BENCH: both stacks painted at power-on (below where this runs, with room), their unpainted depth read later */
#define SY_PAINT 0x57A7A57Au
static void sy_paint(uint32_t *p, const char *top, const char *sp)
{
    const char *stop = sp > (const char *)p && sp <= top ? sp - 512 : top;   /* (the stack this runs on: below it) */
    for (; (const char *)p < stop; p++)
        *p = SY_PAINT;
}
static void sy_stack_paint(void)
{
    uint32_t mark;
    sy_paint((uint32_t *)(void *)_ustack_lo, _ustack_top, (const char *)&mark);
    sy_paint((uint32_t *)(void *)_sstack_lo, _sstack_top, (const char *)&mark);
}
static uint32_t sy_stack_used(const char *lo, const char *top)
{
    const uint32_t *p = (const uint32_t *)(const void *)lo;
    while ((const char *)p < top && *p == SY_PAINT)
        p++;
    return (uint32_t)(top - (const char *)p);
}

/* the SRAM Strata leaves unused (strata.ld: _spare0, RAMTEXT after the RAM code and Felucca's RAM-run debug room;
 * _spare1, NOINIT after .noinit), painted at power-on: BENCH counts the bytes anything wrote there since (none
 * expected: the ROM is not running, every DMA buffer is in .bss or the pool), so that a later version can take them */
extern uint32_t _spare0_lo[], _spare0_hi[], _spare1_lo[], _spare1_hi[];
#define SY_SPARE 0xA55A5AA5u
static void sy_spare_paint(void)
{
    uint32_t *p;
    for (p = _spare0_lo; p < _spare0_hi; p++)
        *p = SY_SPARE;
    for (p = _spare1_lo; p < _spare1_hi; p++)
        *p = SY_SPARE;
    sy_bench.spare = 4u * (uint32_t)((_spare0_hi - _spare0_lo) + (_spare1_hi - _spare1_lo));
}
static uint32_t sy_spare_hit(void)               /* the spare words not as painted, in bytes */
{
    const uint32_t *p;
    uint32_t n = 0;
    for (p = _spare0_lo; p < _spare0_hi; p++)
        n += *p != SY_SPARE;
    for (p = _spare1_lo; p < _spare1_hi; p++)
        n += *p != SY_SPARE;
    return 4u * n;
}

/* ------------------------------------------------------------------------------------------- MASTER --- */
/* the pot, read before the audio starts (the first block at its level), then smoothed a frame at a time; Felucca's
 * curve: the pot's square (1023: 4088 of 4096) */
static int32_t sy_master_knob = 512 * 16;
static int32_t sy_master_of(uint32_t k10) { return (int32_t)((k10 * k10) >> 8); }
static void sy_master_poll(void)
{
    int32_t a = fm1_adc_read(FM1_ADC_MASTER);
    if (a >= 0) {
        sy_master_knob += (a * 16 - sy_master_knob) / 8;
        sy_master_q12 = sy_master_of((uint32_t)(sy_master_knob / 16));
    }
}
static void sy_master_boot(void)
{
    uint32_t i;
    (void)fm1_adc_read(FM1_ADC_MASTER);          /* (the first conversion after the pin setup: dropped) */
    for (i = 0; i < 3u; i++) {
        int32_t a = fm1_adc_read(FM1_ADC_MASTER);
        if (a >= 0) {
            sy_master_knob = a * 16;
            sy_master_q12 = sy_master_now = sy_master_of((uint32_t)a);
            return;
        }
    }
}

/* ---------------------------------------------------------------------------------------------- MIDI --- */
/* USB's and TRS's events (usb.c's ring, 4-byte USB-MIDI packets) to sy_app.c, a millisecond at a time. A ring that
 * overflowed (a TRS stream broken) stops every sound, as Felucca releases its notes. Clock, start and stop wait for
 * the rest of integration (Phase 6) */
static void sy_midi_take(void)
{
    while (mi_r != mi_w) {
        uint32_t pkt, cin;
        RING_PUBLISH();
        pkt = midi_in_q[mi_r % MQ];
        RING_PUBLISH();
        mi_r++;
        cin = pkt & 15u;
        if (cin >= 8u && cin <= 14u)
            sy_midi_in((pkt >> 8) & 0xFFu, (pkt >> 16) & 0x7Fu, (pkt >> 24) & 0x7Fu);
    }
    if (midi_in_overflow) {
        midi_in_overflow = 0;
        sy_ev_push(&sy_ev_midi, SY_EV_PANIC, 0, 0);
    }
}

/* the keys and MIDI between the main loop's millisecond passes: a full redraw holds the loop for up to ~90 ms (BENCH
 * on V0.5: the keys waited 91 ms), most of it waiting for a strip's DMA to the panel. lcd.c calls this before each
 * such wait: once a millisecond, MIDI's ring taken and, when the keys only play (no layer, no screen that types:
 * sy_ui_keys_plain), the keys read, so a note waits a strip at most. BENCH's KEYS row: the longest between two
 * readings of the keys, either here or the millisecond pass */
static uint8_t sy_loop_on;                       /* the main loop running (the splash draws before it) */
static uint32_t sy_keys_t;                       /* TIMER4 at the keys' last reading */
static void sy_keys_read(void)
{
    uint32_t t = fm1_ticks(), d;
    if (sy_keys_t && (d = (t - sy_keys_t) / FM1_TICKS_PER_US) > sy_bench.gap_max_us)
        sy_bench.gap_max_us = d;
    sy_keys_t = t;
}
static void sy_lcd_idle(void)
{
    static uint32_t last;
    if (!sy_loop_on || fm1_ms == last)
        return;
    last = fm1_ms;
    sy_midi_take();
    if (sy_ui_keys_plain()) {
        sy_keys_read();
        sy_keys();
    }
}

/* ---------------------------------------------------------------------------------- UBOOT, the update --- */
static void sy_uboot(const char *why)            /* the chip's USB boot, RAM as it is (debug, a flash backup) */
{
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, sk.ground);
    draw_text_line(0, 100, 240, &AF_M, why, sk.text, sk.ground, 1);
    draw_text_line(0, 126, 240, &AF_XS, "THE CHIP'S USB BOOT: FOR FLASH TOOLS", sk.label, sk.ground, 1);
    draw_text_line(0, 140, 240, &AF_XS, "POWER OFF AND ON TO LEAVE IT", sk.label, sk.ground, 1);
    fm1_delay_ms(20);
    usb_detach();
    fm1_delay_ms(30);
    bootguard.pending = 0;                       /* (meant: no failed boot) */
    fm1_enter_uboot();
}

/* OCT- + OCT+ held 5 s: UBOOT, a countdown in the footer from 2 s; letting go of either stops it */
static void sy_uboot_chord(void)
{
    static uint32_t t0;
    static uint8_t shown;
    uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]), held;
    if ((fm1_in.buttons & both) != both) {
        if (shown) {
            shown = 0;
            ui_toast("UBOOT STOPPED");
        }
        t0 = fm1_ms;
        return;
    }
    held = fm1_ms - t0;
    if (held > 5000u) {
        sy_uboot("UBOOT");
    } else if (held > 2000u) {
        uint32_t left = (5000u - held + 999u) / 1000u;
        if (left != shown) {
            char t[32];
            shown = (uint8_t)left;
            sp_put(sp_fix(sp_put(t, "HOLD: UBOOT IN "), (int32_t)left, 0u, 0), ", LET GO TO STOP");
            ui_toast(t);
        }
    }
}

/* the engine hushed (its notes stopped, the ring silent) for what holds the interrupts off; then back as it was (a
 * SAVE that waits for the hush keeps it) */
static int sy_hush_wait(void)
{
    uint32_t t0 = fm1_ms;
    sy_ev_push(&sy_ev_ui, SY_EV_PANIC, 0, 0);
    sy_hush_req = 1;
    while (!sy_hushed && fm1_ms - t0 < 200u)
        fm1_wdt_feed();
    return sy_hushed;
}
static void sy_hush_end(void) { sy_hush_req = sy_fl.busy; }

/* the M-UPGRADE upgrade command: ota.c's session (it returns only when nothing was committed), then the UI again */
static void sy_update(void)
{
    sy_hush_wait();
    if (flash_ok)
        ota_session();
    sy_hush_end();
    ui.dirty = D_ALL;
}

/* ------------------------------------------------------------------------------------------ BENCH RUN --- */
/* two loops, timed with the interrupts off behind the hush: their instructions are counted in the listing
 * (build/strata.dis: sy_run_int, sy_run_flt), so their times give the core's rate, integer and float */
#define SY_RUN_INT 100000u
#define SY_RUN_FLT 50000u
static SY_NOINLINE uint32_t sy_run_int(uint32_t n)
{
    uint32_t a = 0x12345678u, b = 0x9ABCDEF0u;
    while (n--) {
        a = a * 1664525u + 1013904223u;
        b += a ^ (b >> 13);
    }
    return a ^ b;
}
static SY_NOINLINE float sy_run_flt(uint32_t n)
{
    float x = 0.5f, y = 0.25f;
    while (n--) {
        x = x * 0.999f + y * 0.001f;
        y = y * 0.998f - x * 0.002f + 0.0001f;
    }
    return x + y;
}
static void sy_bench_run(void)
{
    static volatile uint32_t sink;
    static volatile float fsink;
    uint32_t f, t0;
    if (sy_fl.busy || !sy_hush_wait()) {
        sy_hush_end();
        return;                                  /* (a SAVE first: the next frame asks again) */
    }
    f = irq_save();
    audio_silence();
    fm1__led_lines(0);
    t0 = fm1_ticks();
    sink = sy_run_int(SY_RUN_INT);
    sy_bench.run_int_us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    t0 = fm1_ticks();
    fsink = sy_run_flt(SY_RUN_FLT);
    sy_bench.run_flt_us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    irq_restore(f);
    sy_hush_end();
    sy_bench.run = 2;
    ui.dirty |= D_DISP | D_KNOBS;
}

/* ------------------------------------------------------------------------------------------ power-on --- */
static void sy_splash(void)
{
    sy_palette(0);
    lcd_fill(0, 0, 240, 240, sk.ground);
    draw_text_line(0, 88, 240, &AF_L, "STRATA", sk.sound, sk.ground, 1);
    draw_text_line(0, 128, 240, &AF_S, STRATA_VERSION, sk.text, sk.ground, 1);
    draw_text_line(0, 206, 240, &AF_XS, "A WAVETABLE SYNTH \xB7 A FELUCCA FORK", sk.label, sk.ground, 1);
}

static uint32_t crash_seen __attribute__((section(".noinit")));   /* fm1_crash.count at the last boot */
static uint32_t sy_t_boot;                       /* TIMER4 at fm1_cstart (it runs on through warm resets) */

static void fm1_main(void)
{
    uint32_t i, ms_last, frame_t, build_t = 0, stack_t = 0, clean;
    {   /* the flash: the expected part, else none of it is touched; the update loader's staging left over: gone */
        uint32_t f = irq_save();
        flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;
        irq_restore(f);
        if (flash_ok) {
            fl_plain_window_init();
            ota_boot_cleanup();
        }
    }
    clean = !(fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != crash_seen) && !bootguard.failed &&
            !(fm1_boot.p3_rst & 4u);           /* no crash, failed boot or watchdog reset before this one */
    crash_seen = fm1_crash.count;
    lcd_init();
    sy_splash();
    fm1_input_init();
    fm1_adc_init();
    for (i = 0; i < 12u; i++)                    /* the panel polled (~7 ms, no interrupts yet): HOME held */
        fm1_input_scan();                        /* (sy_app.c sy_flash_boot reads it) */
    sy_boot_safe = !clean;
    sy_stack_paint();
    sy_spare_paint();
    sy_bench.known = 1;
    sy_master_boot();
    sy_app_boot();                               /* (the engine, the UI, the flash's; the audio's interrupt attached) */
    usb_start();
#if FELUCCA_UART
    uart_midi_init();
#endif
    fm1_timer5_start(isr_timer5, 4);             /* above the audio (3): the scan nests into the render */
    fm1_guard_lock_top();                        /* (every vector attached) */
    fm1_irq_enable_all();
    while (fm1_ticks() - sy_t_boot < 600000u * FM1_TICKS_PER_US) {   /* the splash: 0.6 s from the start at least */
        fm1_delay_ms(10);
        sy_master_poll();
    }
    ms_last = fm1_ms;
    frame_t = fm1_ms - 15u;
    sy_loop_on = 1;
    for (;;) {
        uint32_t t;
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (fm1_ms > 30000u && bootguard.pending) {   /* a crash or hang in the first 30 s counts */
            bootguard.pending = 0;
            bootguard.failed = 0;
        }
        sy_uboot_chord();
#if FELUCCA_OTA
        ota_service();                           /* the M-UPGRADE handshake (other SysEx: dropped) */
        if (usb.ota_req) {                       /* the upgrade command */
            usb.ota_req = 0;
            sy_update();
        }
#endif
        if (usb.uboot_req)                       /* SysEx F0 22 24 35 7D F7, or the console's `uboot yes` */
            sy_uboot("UBOOT (USB)");
#if FELUCCA_CDC
        cdc_task();
#endif
        if (sy_bench.run == 1u)
            sy_bench_run();
        sy_midi_take();
        if (fm1_ms != ms_last) {                 /* a millisecond: the keys, the knobs, a build step, the flash */
            uint32_t d, building = wt_build_o != 0xFFu;
            t = fm1_ticks();
            sy_keys_read();                      /* (sy_app_ms reads them first) */
            ms_last = fm1_ms;
            sy_app_ms();
            d = (fm1_ticks() - t) / FM1_TICKS_PER_US;
            if (building && d > sy_bench.step_max_us)
                sy_bench.step_max_us = d;
            if (wt_build_o != 0xFFu && !build_t)
                build_t = fm1_ms | 1u;
            else if (wt_build_o == 0xFFu && build_t) {
                if (fm1_ms - build_t > sy_bench.build_max_ms)
                    sy_bench.build_max_ms = fm1_ms - build_t;
                build_t = 0;
            }
        }
        if (fm1_ms - frame_t >= 15u) {           /* a frame: ~60 a second at most */
            frame_t = fm1_ms;
            sy_master_poll();
            t = fm1_ticks();
            sy_app_frame();
            sy_bench.frame_us = (fm1_ticks() - t) / FM1_TICKS_PER_US;
            if (sy_bench.frame_us > sy_bench.frame_max_us)
                sy_bench.frame_max_us = sy_bench.frame_us;
            if (!sy_bench.boot_ms)
                sy_bench.boot_ms = (fm1_ticks() - sy_t_boot) / (1000u * FM1_TICKS_PER_US);
        }
        if (fm1_ms - stack_t >= 500u) {
            stack_t = fm1_ms;
            sy_bench.stack_isr = sy_stack_used(_sstack_lo, _sstack_top);
            sy_bench.stack_main = sy_stack_used(_ustack_lo, _ustack_top);
            sy_bench.spare_hit = sy_spare_hit();
        }
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt, t_boot;
    fm1_time_init();
    t_boot = fm1_ticks();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);                           /* ~8 s, fed by the main loop */
    if (bootguard.magic != BOOTGUARD_MAGIC) {
        bootguard.magic = BOOTGUARD_MAGIC;
        bootguard.failed = 0;
        bootguard.pending = 0;
    }
    if (bootguard.pending)
        bootguard.failed++;
    bootguard.pending = 1;
    if (bootguard.failed >= 2u) {                /* two boots in a row died in their first 30 s */
        bootguard.failed = 0;
        bootguard.pending = 0;
        fm1_enter_uboot();
    }
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;                                 /* the flash driver's code that runs from RAM */
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    sy_t_boot = t_boot;
    fm1_main();
    for (;;)
        ;
}
