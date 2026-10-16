/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone
 * After Felucca's src/storage_hw.c and src/ota_hw.c (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton
 * Instruments, GPL-3.0); their checks are kept as they were. */
/* STRATA on the FM-1: the flash's hooks and the update entry's.
 *   sy_store.c's (st_read, st_erase, st_prog): reads in 256-byte windows with the interrupts off, so the audio keeps
 *   up; erases and writes only inside the storage areas (FL_STORE_OK here, FL_RANGE_OK again in the RAM driver,
 *   hal/fm1_flash.h); the audio's ring zeroed first (the interrupts stay off for an erase, ~17 ms, and the DMA loops
 *   the ring: silence, not a buzz) and the LED lines dark (the scan stops meanwhile). Each erase and each page's
 *   program is timed, interrupts off to on, for BENCH.
 *   ota.c's: the clock and the watchdog; erases and writes only inside the update loader's staging area; the
 *   progress on the screen; the commit (the record into RAM, the core reset, the boot guard told it is meant).
 * Nothing is read, erased or written unless the flash is the expected part (flash_ok, sy_main.c). */

/* the boot-loop guard (sy_main.c): two boots in a row that die in their first 30 s -> UBOOT. Here, where the
 * update's commit first needs it; Felucca's layout and magic (core.h), so neither firmware misreads the other's */
#define BOOTGUARD_MAGIC 0x42475244u
struct { uint32_t magic, failed, pending; } bootguard __attribute__((section(".noinit")));

static uint8_t flash_ok;                         /* the expected 1 MiB part (its JEDEC id), checked at power-on */

static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte windows with the interrupts off */
{
    uint8_t *d = dst;
    if (!flash_ok)
        return -2;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}

static void audio_silence(void)                  /* (the interrupts off: the DMA loops the ring) */
{
    uint32_t i;
    for (i = 0; i < 2u * SY_HALF_WORDS; i++)
        sy_abuf[i] = 0;
}

static void st_timed(uint32_t t0, uint32_t *max)  /* BENCH: an interrupts-off window, us */
{
    uint32_t us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    if (us > *max)
        *max = us;
}

static int st_erase(uint32_t off)
{
    uint32_t took, f, t0;
    int rc;
    if (!flash_ok)
        return -2;
    if (!FL_STORE_OK(off, 0x1000u))              /* only inside the storage areas */
        return -8;
    f = irq_save();
    t0 = fm1_ticks();
    audio_silence();
    fm1__led_lines(0);
    rc = FL_FAR(fl_erase4k_ram)(off, &took);
    st_timed(t0, &sy_bench.erase_max_us);
    irq_restore(f);
    sy_bench.erase_n++;
    return rc;
}

static int st_prog(uint32_t off, const void *src, uint32_t n)   /* a page at a time, as fl_write */
{
    const uint8_t *s = src;
    uint32_t took;
    if (!flash_ok)
        return -2;
    if (!FL_STORE_OK(off, n))                    /* only inside the storage areas */
        return -8;
    while (n) {
        uint32_t chunk = 256u - (off & 0xFFu), f, t0;
        int rc;
        if (chunk > n)
            chunk = n;
        f = irq_save();
        t0 = fm1_ticks();
        rc = FL_FAR(fl_prog_ram)(off, s, chunk, &took);
        st_timed(t0, &sy_bench.prog_max_us);
        irq_restore(f);
        if (rc)
            return rc;
        off += chunk;
        s += chunk;
        n -= chunk;
    }
    return 0;
}

/* ------------------------------------------------------------------------------------- the update entry --- */
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { fm1_wdt_feed(); }
static int ota_in_area(uint32_t off, uint32_t n) { return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN); }
static int ota_erase(uint32_t off)
{
    uint32_t took;
    if (!flash_ok || !ota_in_area(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    audio_silence();
    return fl_erase4k(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!flash_ok || !ota_in_area(off, n))
        return -8;
    return fl_write(off, p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }

/* the update's steps on the screen, in the palette playing; a refusal or a failure with its code (ota.c's) for 1.5 s,
 * then the UI again */
static void ota_show(uint32_t step, int32_t code)
{
    static const char *const STEP[] = {"", "READING THE PACKAGE", "CHECKING ITS HEAD", "STAGING ITS LOADER",
                                       "WAITING FOR THE HOST", "RESTARTING INTO THE LOADER"};
    char b[24], *e;
    lcd_fill(0, 0, 240, 240, sk.ground);
    draw_text_line(0, 92, 240, &AF_M, "UPDATE", sk.text, sk.ground, 1);
    if (step < 9u) {
        draw_text_line(0, 124, 240, &AF_S, STEP[step < 6u ? step : 0], sk.label, sk.ground, 1);
        return;
    }
    e = code == 1 ? sp_put(b, "DRY RUN OK") : sp_fix(sp_put(b, "STOPPED: "), code, 0u, 0);
    (void)e;
    draw_text_line(0, 124, 240, &AF_S, b, sk.now, sk.ground, 1);
    draw_text_line(0, 148, 240, &AF_XS, "NOTHING WAS WRITTEN: STRATA GOES ON", sk.label, sk.ground, 1);
    fm1_delay_ms(1500);
}

static void ota_commit(const uint8_t *parm)
{
    bootguard.pending = 0;                       /* (meant: no failed boot) */
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);                      /* the record into RAM, the core reset (fm1_sys.h) */
}
