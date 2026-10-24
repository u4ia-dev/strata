/* SPDX-License-Identifier: GPL-3.0-only
 * After Felucca's src/console.c (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0):
 * its line editor, number parsing, memr, flr, inp, crash and uboot are kept as they were. */
/* STRATA's serial console on the CDC-ACM function (FELUCCA_CDC=1): read-only diagnostics, in the main loop
 * (cdc_task; usb_poll moves the bytes). Nothing here writes memory or flash; `uboot yes` does what the SysEx soft key
 * does. `status` and `bench` are what a hardware test reports (docs/FLASHING.md): the web flasher's DEVICE REPORT
 * reads them over Web Serial. V0.87.1: `uboot yes a|b|c` picks the way into the USB boot (sy_main.c sy_uboot), and
 * `tproj` loads, stops and follows the test projects (sy_testproj.c: into RAM, the flash not written, auto-save off
 * while one is loaded); one line a reply */
#define CON_LINE 64u

static struct {
    char line[CON_LINE];
    uint32_t len;
    uint8_t dtr_seen, stalled;   /* stalled: the host stopped reading, drop the rest of this reply */
} con;

static void con_putc(char c)
{
    uint32_t t0 = fm1_ms;
    while (co_w - co_r >= CO_N) {                      /* full: wait for usb_poll, briefly */
        if (!cdc.dtr || con.stalled || fm1_ms - t0 > 20u) {
            con.stalled = 1;
            return;
        }
        fm1_wdt_feed();
    }
    cdc_out[co_w % CO_N] = (uint8_t)c;
    RING_PUBLISH();
    co_w++;
}

static void con_puts(const char *s)
{
    while (*s)
        con_putc(*s++);
}

static void con_hex(uint32_t v, uint32_t digits)
{
    while (digits--)
        con_putc("0123456789ABCDEF"[(v >> (digits * 4u)) & 15u]);
}

static void con_dec(int32_t v)
{
    char b[12];
    uint32_t n = 0, u = v < 0 ? (uint32_t)-v : (uint32_t)v;
    if (v < 0)
        con_putc('-');
    do
        b[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) != 0 && n < sizeof b);
    while (n)
        con_putc(b[--n]);
}

static void con_kv(const char *k, int32_t v)            /* "key value\r\n" */
{
    con_puts(k);
    con_putc(' ');
    con_dec(v);
    con_puts("\r\n");
}

static void con_kx(const char *k, uint32_t v)
{
    con_puts(k);
    con_puts(" 0x");
    con_hex(v, 8);
    con_puts("\r\n");
}

static void con_ks(const char *k, const char *s)        /* "key text\r\n" */
{
    con_puts(k);
    con_putc(' ');
    con_puts(s);
    con_puts("\r\n");
}

static uint32_t con_num(const char **p, int *ok)        /* decimal or 0x hex (V0.87.1: over 32 bits, not ok) */
{
    const char *s = *p;
    uint32_t v = 0, base = 10, d, n = 0, over = 0;
    while (*s == ' ')
        s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2, base = 16;
    for (;; s++, n++) {
        char c = *s;
        if (c >= '0' && c <= '9')
            d = (uint32_t)(c - '0');
        else if (base == 16 && c >= 'a' && c <= 'f')
            d = (uint32_t)(c - 'a' + 10);
        else if (base == 16 && c >= 'A' && c <= 'F')
            d = (uint32_t)(c - 'A' + 10);
        else
            break;
        if (v > (0xFFFFFFFFu - d) / base)
            over = 1;
        v = v * base + d;
    }
    *ok = n > 0 && !over;
    *p = s;
    return v;
}

static int con_word(const char **p, const char *w)       /* match a whole word */
{
    const char *s = *p;
    while (*s == ' ')
        s++;
    while (*w && *s == *w)
        s++, w++;
    if (*w || (*s && *s != ' '))
        return 0;
    *p = s;
    return 1;
}

static void con_memr(const char *p)
{
    int ok, ok2;
    uint32_t a = con_num(&p, &ok), n = con_num(&p, &ok2), i;
    if (!ok) {
        con_puts("usage: memr ADDR [LEN<=256]\r\n");
        return;
    }
    if (!ok2 || !n)
        n = 64;
    if (n > 256u)
        n = 256;
    if (!fm1_mem_readable(a, n)) {
        con_puts("only RAM 01C00000-01C80000 and XIP 02000000-02100000\r\n");
        return;
    }
    for (i = 0; i < n; i++) {
        if (i % 16u == 0) {
            con_hex(a + i, 8);
            con_putc(':');
        }
        con_putc(' ');
        con_hex(fm1_peek8(a + i), 2);
        if (i % 16u == 15u || i + 1u == n)
            con_puts("\r\n");
    }
}

static void con_flr(const char *p)                  /* flash read over SPI (no XIP decryption) */
{
    static uint8_t b[256];
    int ok, ok2;
    uint32_t a = con_num(&p, &ok), n = con_num(&p, &ok2), i;
    if (!ok || a < 0x93000u || a >= 0x100000u) {
        con_puts("usage: flr OFFSET [LEN<=256]   (0x93000..0xFFFFF)\r\n");
        return;
    }
    if (!ok2 || !n)
        n = 64;
    if (n > 256u)
        n = 256;
    if (a + n > 0x100000u)
        n = 0x100000u - a;
    if (!flash_ok || st_read(a, b, n)) {
        con_puts("flash not available\r\n");
        return;
    }
    for (i = 0; i < n; i++) {
        if (i % 16u == 0) {
            con_hex(a + i, 6);
            con_putc(':');
        }
        con_putc(' ');
        con_hex(b[i], 2);
        if (i % 16u == 15u || i + 1u == n)
            con_puts("\r\n");
    }
}

#if FELUCCA_UAC
static void con_uac(void)                              /* USB audio input: stream state and glitches */
{
    uint32_t p = uac.pkts, base = uac.r48 ? 48u : 44u;
    con_kv("uac_alt", uac.alt);
    con_kv("uac_hz", uac.r48 ? 48000 : 44100);
    con_kv("uac_pkts", (int32_t)p);
    con_kv("uac_rate_hz", p ? (int32_t)base * 1000 + (int32_t)(uac.frames - base * p) * 1000 / (int32_t)p : 0);
    con_kv("uac_underruns", (int32_t)uac.underruns);
    con_kv("uac_overruns", (int32_t)uac.overruns);
    con_kv("uac_missed", (int32_t)uac.missed);
    con_kv("uac_stalls", (int32_t)uac.stalls);
    con_kv("uac_fill_lo", uac.fill_lo == 0xFFFFFFFFu ? -1 : (int32_t)uac.fill_lo);
    con_kv("uac_fill_hi", (int32_t)uac.fill_hi);
}
#endif

/* the state a hardware test reports: the build, the audio and its guard, the sound, USB, MIDI, the flash */
static void con_status(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < SY_NVOICE; i++)
        n += sy_v[i].on != 0;
    con_puts("strata ");
    con_puts(STRATA_VERSION);
    con_puts(" (");
    con_puts(FELUCCA_ID);
    con_puts(")\r\n");
    con_kv("uptime_ms", (int32_t)fm1_ms);
    con_kv("cpu_pct", sy_app_load());
    con_kv("half_us", (int32_t)sy_bench.half_us);
    con_kv("half_max_us", (int32_t)sy_bench.half_max_us);
    con_kv("halves_late", (int32_t)sy_bench.late);
    con_kv("guard_level", sy_g.level);
    con_kv("guard_fx_lo", sy_g.fx_lo);
    con_kv("voices_shed", (int32_t)sy_g.shed);
    con_kv("voices", (int32_t)n);
    con_ks("sound", sy_patch.name);
    con_kv("user_slot", sy_slot);
    con_kv("master_q12", sy_master_q12);
    con_kv("master_adc", sy_master_adc);         /* (V0.87.1: the pot's last reading, before the dead band) */
    con_kv("safe_start", sy_boot_safe);
    con_kv("bootguard_failed", (int32_t)bootguard.failed);
    con_kv("usb_resets", (int32_t)usb.resets);
    con_kv("usb_sof", (int32_t)usb.sof_seen);
    con_kv("usb_suspends", (int32_t)usb.suspends);
    con_kv("midi_rx_pkts", (int32_t)usb.rx_pkts);
#if FELUCCA_UAC
    con_uac();
#endif
#if FELUCCA_UART
    con_kv("uart_rx_bytes", (int32_t)um.bytes);
    con_kv("uart_rx_msgs", (int32_t)um.msgs);
    con_kv("uart_rx_drops", (int32_t)um.drops);
#endif
    con_kv("flash", flash_ok);
    {                                            /* (V0.87.1: the test project playing, 1 .. SY_TP_N, 0 none) */
        uint32_t k = (uint32_t)sy_tp_active();
        con_kv("tproj", (int32_t)k);
        con_kv("tproj_ms", k && k <= SY_TP_N ? (int32_t)sy_tp_ms(k - 1u) : 0);
    }
}

/* BENCH's figures (SETTINGS > BENCH), and RUN's loop counts (their instructions: build/strata.dis) */
static void con_bench(void)
{
    con_kv("boot_ms", (int32_t)sy_bench.boot_ms);
    con_kv("half_us", (int32_t)sy_bench.half_us);
    con_kv("half_max_us", (int32_t)sy_bench.half_max_us);
    con_kv("half_budget_us", (int32_t)(SY_HALF * 1000000u / 44118u));
    con_kv("halves_late", (int32_t)sy_bench.late);
    con_kv("halves_late_held", (int32_t)sy_bench.late_held);
    con_kv("voices_refused", (int32_t)sy_adm.refused);
    con_kv("frame_us", (int32_t)sy_bench.frame_us);
    con_kv("frame_max_us", (int32_t)sy_bench.frame_max_us);
    con_puts("frame_max_page ");
    con_puts(ui_sec_name(UI_PG[sy_bench.frame_max_page].sec));
    con_putc(' ');
    con_puts(UI_PG[sy_bench.frame_max_page].name);
    con_puts("\r\n");
    con_kv("wait_max_us", (int32_t)sy_bench.gap_max_us);
    con_kv("wait_save_max_us", (int32_t)sy_bench.gap_save_max_us);
    con_kv("keys_from_timer5", sy_keys_isr);
    con_kv("timer5_gap_max_us", (int32_t)sy_bench.t5_gap_max_us);
    con_kv("build_step_max_us", (int32_t)sy_bench.step_max_us);
    con_kv("build_max_ms", (int32_t)sy_bench.build_max_ms);
    con_kv("flash_erases", (int32_t)sy_bench.erase_n);
    con_kv("flash_erase_max_us", (int32_t)sy_bench.erase_max_us);
    con_kv("flash_page_max_us", (int32_t)sy_bench.prog_max_us);
    con_kv("stack_irq_used", (int32_t)sy_bench.stack_isr);
    con_kv("stack_irq_size", (int32_t)(_sstack_top - _sstack_lo));
    con_kv("stack_main_used", (int32_t)sy_bench.stack_main);
    con_kv("stack_main_size", (int32_t)(_ustack_top - _ustack_lo));
    con_kv("spare_bytes", (int32_t)sy_bench.spare);
    con_kv("spare_changed_bytes", (int32_t)sy_bench.spare_hit);
    con_kv("run", sy_bench.run);
    con_kv("run_int_loops", (int32_t)SY_RUN_INT);
    con_kv("run_int_us", (int32_t)sy_bench.run_int_us);
    con_kv("run_flt_loops", (int32_t)SY_RUN_FLT);
    con_kv("run_flt_us", (int32_t)sy_bench.run_flt_us);
    con_kv("key_press_max_us", (int32_t)(fm1_in_stat.press_max / FM1_TICKS_PER_US));
    con_ks("c2_state", C2_STATE[c2.state % C2_NSTATE]);   /* (V0.8 BENCH: CORE 2, sy_c2.c) */
    con_kv("c2_on", c2.on);
    con_kv("c2_load_voices", c2.load);
    con_kv("c2_test_done", c2.done);
    con_kv("c2_jobs", (int32_t)c2.j1_n);
    con_kv("c2_bad", (int32_t)c2_bad());
    con_kv("c2_core1_us", (int32_t)c2_c1_us());
    con_kv("c2_core1_max_us", (int32_t)c2_c1_max_us());
    con_kv("c2_core0_us", (int32_t)c2_c0_us());
    con_kv("c2_render_alone_us", (int32_t)c2_alone_us());
    con_kv("c2_render_with_us", (int32_t)c2_with_us());
    con_kv("c2_wait_max_us", (int32_t)(c2.wait_max / FM1_TICKS_PER_US));
    con_kv("c2_stack_used", (int32_t)c2_stack());
    {                                                   /* the event log (sy_main.c), oldest first: ev MS WHAT ARG */
        uint32_t i = sy_nlog > SY_NLOG ? sy_nlog - SY_NLOG : 0u;
        con_kv("events", (int32_t)sy_nlog);
        for (; i < sy_nlog; i++) {
            con_puts("ev ");
            con_dec((int32_t)sy_log[i % SY_NLOG].ms);
            con_putc(' ');
            con_puts(EV_NAMES[sy_log[i % SY_NLOG].type]);
            con_putc(' ');
            con_dec(sy_log[i % SY_NLOG].arg);
            con_puts("\r\n");
        }
    }
}

/* input scan (hal/fm1_input.h fm1_in_stat) since the last `inp`, then cleared: tick gaps (= LED
 * on-time of a column), per-column LED share in 1/1000, the tick cost, the dim pulse (its average width and the
 * bits of the 595 shift it spans), encoder transitions */
static void con_inp(void)
{
    static uint32_t t_last;
    uint32_t now = fm1_ticks(), win = now - t_last, i, k;
    uint32_t win_us = win / FM1_TICKS_PER_US;
    t_last = now;
    con_kv("window_ms", (int32_t)(win_us / 1000u));
    con_kv("ticks_per_s", (int32_t)(win_us >= 1000u ? fm1_in_stat.ticks * 1000u / (win_us / 1000u) : 0u));
    con_kv("gap_max_us", (int32_t)(fm1_in_stat.gap_max / FM1_TICKS_PER_US));
    con_puts("gap_hist <.15 .3 .6 1.2 2.4 4.8 9.6 more ms:");
    for (i = 0; i < FM1_GAP_BINS; i++) {
        con_putc(' ');
        con_dec((int32_t)fm1_in_stat.gap_hist[i]);
    }
    con_puts("\r\n");
    con_kv("tick_cost_avg_ns", (int32_t)(fm1_in_stat.ticks ? fm1_in_stat.cost_sum / fm1_in_stat.ticks * 1000u / FM1_TICKS_PER_US : 0u));
    con_kv("tick_cost_max_us", (int32_t)(fm1_in_stat.cost_max / FM1_TICKS_PER_US));
    con_kv("tick_cpu_permille", (int32_t)(win >= 1000u ? fm1_in_stat.cost_sum / (win / 1000u) : 0u));
    con_puts("enc moves/lost/detent state:");
    for (k = 0; k < FM1_NENC; k++) {
        con_putc(' ');
        con_dec((int32_t)fm1_in_stat.enc_moves[k]);
        con_putc('/');
        con_dec((int32_t)fm1_in_stat.enc_lost[k]);
        con_putc('/');
        con_dec(fm1_in.enc_rest[k]);
    }
    con_puts("\r\n");
    {
        uint32_t n = fm1_in_stat.press_n;
        con_kv("key_presses", (int32_t)n);
        con_kv("key_press_avg_us", (int32_t)(n ? fm1_in_stat.press_sum / n / FM1_TICKS_PER_US : 0u));
        con_kv("key_press_max_us", (int32_t)(fm1_in_stat.press_max / FM1_TICKS_PER_US));
    }
    {
        uint32_t k2 = fm1__lock();
        memset((void *)&fm1_in_stat.gap_max, 0, sizeof fm1_in_stat - sizeof fm1_in_stat.last);
        fm1__unlock(k2);
    }
}

static void con_crash(void)
{
    if (fm1_crash.magic != FM1_CRASH_MAGIC) {
        con_puts("no crash record\r\n");
        return;
    }
    con_kv("count", (int32_t)fm1_crash.count);
    con_kx("vec", fm1_crash.vec);
    con_kx("pc", fm1_crash.pc);
    con_kx("rets", fm1_crash.rets);
    con_kx("emu", fm1_crash.emu);
    con_kx("sp", fm1_crash.sp);
    con_kx("psr", fm1_crash.psr);
    con_kv("uptime_ms", (int32_t)fm1_crash.uptime_ms);
    con_kv("early", (int32_t)fm1_crash.early);
}

static void con_params(void)                            /* the sound playing: every parameter, id=value */
{
    uint32_t i;
    for (i = 0; i < SP_COUNT; i++) {
        con_dec((int32_t)i);
        con_putc('=');
        con_dec(sy_patch.p[i]);
        con_puts(i % 8u == 7u || i + 1u == SP_COUNT ? "\r\n" : " ");
    }
}

/* V0.87.1: the test projects (sy_testproj.c), a line each: `tproj N` (1 .. SY_TP_N) loads and starts N, "tproj N NAME
 * ok", or "tproj N busy" when it can't (a load in flight); `tproj stop`, "tproj stop ok"; `tproj` alone, "tproj K NAME
 * MS" while K is loaded (MS its running time), else "tproj none" */
static void con_tproj(const char *p)
{
    int ok;
    uint32_t n, k;
    if (con_word(&p, "stop")) {
        sy_tp_stop();
        con_puts("tproj stop ok\r\n");
        return;
    }
    while (*p == ' ')
        p++;
    if (!*p) {
        k = (uint32_t)sy_tp_active();
        if (!k || k > SY_TP_N) {
            con_puts("tproj none\r\n");
            return;
        }
        con_puts("tproj ");
        con_dec((int32_t)k);
        con_putc(' ');
        con_puts(sy_tp_name(k - 1u));
        con_putc(' ');
        con_dec((int32_t)sy_tp_ms(k - 1u));
        con_puts("\r\n");
        return;
    }
    n = con_num(&p, &ok);                          /* (not ok: no digits, or over 32 bits) */
    while (*p == ' ')
        p++;
    if (!ok || *p || n < 1u || n > SY_TP_N) {
        con_puts("usage: tproj [1..4|stop]\r\n");
        return;
    }
    con_puts("tproj ");
    con_dec((int32_t)n);
    con_putc(' ');
    if (sy_tp_load(n - 1u)) {
        con_puts(sy_tp_name(n - 1u));
        con_puts(" ok\r\n");
    } else {
        con_puts("busy\r\n");
    }
}

static void con_exec(const char *p)
{
    if (con_word(&p, "help") || con_word(&p, "?"))
        con_puts("status  bench [run]  inp  crash  params  memr ADDR [LEN]  flr OFF [LEN]  tproj [1..4|stop]  "
                 "uboot yes [a|b|c]\r\n");
    else if (con_word(&p, "status"))
        con_status();
    else if (con_word(&p, "bench")) {
        if (con_word(&p, "run")) {                     /* RUN: the main loop times its loops (a moment's silence) */
            sy_bench.run = 1;
            con_puts("RUN asked: 'bench' again in a second\r\n");
        } else {
            con_bench();
        }
    }
    else if (con_word(&p, "crash"))
        con_crash();
    else if (con_word(&p, "inp"))
        con_inp();
    else if (con_word(&p, "params"))
        con_params();
    else if (con_word(&p, "memr"))
        con_memr(p);
    else if (con_word(&p, "flr"))
        con_flr(p);
    else if (con_word(&p, "tproj"))
        con_tproj(p);
    else if (con_word(&p, "uboot")) {                  /* (V0.87.1: way a, b or c, sy_main.c sy_uboot; none: b) */
        uint32_t way = 0;
        if (con_word(&p, "yes")) {
            if (con_word(&p, "a"))
                way = 'a';
            else if (con_word(&p, "b"))
                way = 'b';
            else if (con_word(&p, "c"))
                way = 'c';
            while (*p == ' ')
                p++;
            if (*p)
                way = 0;
            else if (!way)
                way = SY_UBOOT_WAY;
        }
        if (way) {
            con_puts("entering UBOOT ");
            con_putc((char)way);
            con_puts("\r\n");
            sy_uboot_way = (uint8_t)way;
            usb.uboot_req = 1;                         /* main loop: same path as the SysEx key */
        } else {
            con_puts("type 'uboot yes' (or 'uboot yes a|b|c')\r\n");
        }
    } else if (*p)
        con_puts("? (help)\r\n");
}

static void cdc_task(void)                              /* main loop */
{
    if (cdc.dtr && !con.dtr_seen) {
        con_puts("\r\nStrata ");
        con_puts(STRATA_VERSION);
        con_puts(" console - 'help'\r\n> ");
    }
    con.dtr_seen = cdc.dtr;
    while (ci_r != ci_w) {
        char c;
        RING_PUBLISH();                                /* usb_poll (producer) can preempt us */
        c = (char)cdc_in[ci_r % CI_N];
        RING_PUBLISH();
        ci_r++;
        if (c == '\r' || c == '\n') {
            if (c == '\n' && con.len == 0)
                continue;                              /* the LF of a CR LF */
            con_puts("\r\n");
            con.line[con.len] = 0;
            con.stalled = 0;
            con_exec(con.line);
            con.len = 0;
            con_puts("> ");
        } else if (c == 0x7F || c == 0x08) {
            if (con.len) {
                con.len--;
                con_puts("\b \b");
            }
        } else if (c >= ' ' && c < 0x7F && con.len + 1u < CON_LINE) {
            con.line[con.len++] = c;
            con_putc(c);
        }
    }
}
