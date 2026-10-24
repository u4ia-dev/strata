/* SPDX-License-Identifier: GPL-3.0-only */
/* V0.81: USB MIDI out's recovery (firmware/src/usb.c midi_out_event, midi_out_recover), as the official firmware's
 * MIDI router recovers: a note the full ring dropped marks its channel, nothing more goes in until the ring has room,
 * then an All Notes Off on each channel marked (in channel order) before the next packet; the half's poll sends them
 * with no packet to follow; nothing while USB is not configured; a ring with room is as before, packet for packet. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FELUCCA_OTA 0
#define FELUCCA_CDC 0
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"   /* SIE register macros (never touched here) */
#include "../firmware/src/usb.c"

static int fails, checks;
static void check(const char *what, int ok)
{
    printf("midi out: %-92s %s\n", what, ok ? "ok" : "FAIL");
    checks++;
    fails += !ok;
}
static uint32_t on(uint32_t ch, uint32_t n) { return 0x09u | (0x90u | ch) << 8 | n << 16 | 100u << 24; }
static uint32_t off(uint32_t ch, uint32_t n) { return 0x08u | (0x80u | ch) << 8 | n << 16; }
static uint32_t ano(uint32_t ch) { return 0x0Bu | (0xB0u | ch) << 8 | 123u << 16; }
static uint32_t at(uint32_t i) { return midi_out_q[i % MQ]; }
static void reset(void)
{
    mo_w = mo_r = 0;
    mo_lost = 0;
    usb.config = 1;
}

int main(void)
{
    uint32_t i, w;
    int ok;

    reset();
    for (i = 0; i < 10u; i++)
        midi_out_event(i & 1u ? off(2, 60 + i / 2u) : on(2, 60 + i / 2u));
    for (ok = mo_w == 10u, i = 0; i < 10u; i++)
        ok &= at(i) == (i & 1u ? off(2, 60 + i / 2u) : on(2, 60 + i / 2u));
    midi_out_recover();
    check("a ring with room: every packet in, in order, nothing added (the poll too)", ok && mo_w == 10u && !mo_lost);

    reset();
    for (i = 0; i < MQ; i++)
        midi_out_event(on(0, i & 127u));
    midi_out_event(off(3, 64));                    /* (full: lost) */
    midi_out_event(on(5, 65));
    midi_out_event(0x0Bu | 0xB7u << 8 | 7u << 16 | 90u << 24);   /* (a controller: lost, no channel marked) */
    check("full: the notes after it lost, their channels marked (3, 5), a controller's not",
          mo_w == MQ && mo_lost == ((1u << 3) | (1u << 5)));
    mo_r = 50;                                     /* (the host reads 50: 14 in the ring) */
    midi_out_event(on(1, 70));
    w = mo_w;
    check("room again: All Notes Off on 3, then on 5, then the packet", w == MQ + 3u && at(MQ) == ano(3) &&
          at(MQ + 1u) == ano(5) && at(MQ + 2u) == on(1, 70) && !mo_lost);
    midi_out_event(off(1, 70));
    check(".. and from then on as before", mo_w == w + 1u && at(w) == off(1, 70));

    reset();
    for (i = 0; i < MQ; i++)
        midi_out_event(on(0, i & 127u));
    midi_out_event(off(9, 36));
    mo_r = 5;                                      /* (5 free: under the 16 it waits for) */
    midi_out_event(on(9, 38));
    check("too little room: still nothing in (a note then would be cut by the All Notes Off after it)",
          mo_w == MQ && mo_lost == 1u << 9);
    mo_r = 48;
    midi_out_recover();                            /* (the half's poll: no packet needed) */
    check("the poll: All Notes Off on 9 with no packet after it", mo_w == MQ + 1u && at(MQ) == ano(9) && !mo_lost);

    reset();
    for (i = 0; i < MQ; i++)
        midi_out_event(on(0, i & 127u));
    midi_out_event(off(0, 1));
    usb.config = 0;
    mo_r = MQ;
    midi_out_event(on(0, 2));
    midi_out_recover();
    check("USB not configured: nothing in, the mark kept for when it is", mo_w == MQ && mo_lost == 1u);
    usb.config = 1;
    midi_out_recover();
    check(".. configured: its All Notes Off", mo_w == MQ + 1u && at(MQ) == ano(0) && !mo_lost);

    reset();
    for (i = 0; i < 16u; i++) {                    /* (every channel lost: 16 in a ring of 64 with room for 17) */
        midi_out_q[0] = 0;
        mo_w = mo_r + MQ;
        midi_out_event(off(i, 60));
    }
    mo_r = mo_w - (MQ - 17u);
    midi_out_event(on(15, 61));
    for (ok = 1, i = 0; i < 16u; i++)
        ok &= at(mo_w - 17u + i) == ano(i);
    check("all 16 channels: 16 All Notes Off and the packet fit the room asked for", ok &&
          at(mo_w - 1u) == on(15, 61) && mo_w - mo_r == MQ);

    printf("midi out: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
