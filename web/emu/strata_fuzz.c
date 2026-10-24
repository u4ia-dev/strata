/* SPDX-License-Identifier: GPL-3.0-only */
/* The firmware (web/emu/strata_web.c, natively) driven at random under the sanitizers, its invariants checked after
 * each step: the panel's buttons (taps, holds, chords with OCT), the keys, the knobs, MIDI in (notes on every channel,
 * controllers, clock, start and stop), the looper's own (launches, scenes, REC, UNDO PASS, notes and moves put in,
 * the tracks' settings, the pages, projects loaded), the audio rendered between. Checked: the event pool (every event
 * in one clip's list, the free list or UNDO's, none twice; the counts), each clip (its list sorted, as long as its
 * count, inside its length), each track's place (its scenes, its next event in its clip), the recording's lists, the
 * chords' side the UI's (the selected track's), the voices' tracks, the parameters held. A failure prints the step
 * and exits 1.
 *   cc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -ffp-contract=off -w \
 *      -Ibuild/gen -Ifirmware/src -o build/emu/strata_fuzz web/emu/strata_fuzz.c -lm
 *   build/emu/strata_fuzz [seed] [steps] */
#include <stdio.h>
#include <stdlib.h>
#define WEB_NO_LINT 1
#include "strata_web.c"

static uint32_t rng = 0x12345678u, step_no, verbose;
#define TRACE(...) (verbose ? printf(__VA_ARGS__) : 0)
static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static uint32_t rn(uint32_t n) { return n ? rnd() % n : 0u; }
static void run_ms(uint32_t ms)
{
    uint32_t n = ms * 441u / 10u, k;
    for (k = 0; k < n; k += 128u)
        web_render(128u);
}
static void fail(const char *what, int a, int b)
{
    printf("FAIL step %u: %s (%d, %d)\n", step_no, what, a, b);
    exit(1);
}

/* ---------------------------------------------------------------------------------------- invariants --- */
static uint8_t seen[LP_NEV];
static void check(void)
{
    uint32_t t, s, k, n, nfree = 0, inclips = 0;
    memset(seen, 0, sizeof seen);
    for (k = lp_free; k != LP_END; k = lp_ev[k].next) {
        if (k >= LP_NEV)
            fail("free list: an index out of range", (int)k, 0);
        if (seen[k])
            fail("free list: an event twice (a cycle)", (int)k, seen[k]);
        seen[k] = 1;
        nfree++;
    }
    if (nfree != lp_nfree)
        fail("free count", (int)nfree, lp_nfree);
    for (t = 0; t < LP_NTRK; t++)
        for (s = 0; s < LP_NSCN; s++) {
            lp_clip_t *c = &lpp.clip[t][s];
            uint32_t last = 0;
            n = 0;
            if (lpp.speed[t][s] >= LS_N)          /* (V0.86: a clip's SPEED one of three) */
                fail("a clip's SPEED out of range", (int)(t * 8 + s), lpp.speed[t][s]);
            for (k = c->head; k != LP_END; k = lp_ev[k].next) {
                if (k >= LP_NEV)
                    fail("a clip: an index out of range", (int)(t * 8 + s), (int)k);
                if (seen[k])
                    fail("an event in two lists (or a cycle)", (int)k, seen[k]);
                seen[k] = 2;
                if (lp_et(k) < last)
                    fail("a clip not sorted", (int)(t * 8 + s), (int)k);
                last = lp_et(k);
                if (!(lp_ev[k].t & LP_MOVE) && (lp_ev[k].a > 127u || !(lp_ev[k].b & 0x7Fu) || !lp_ev[k].len))
                    fail("a note out of range", lp_ev[k].a, lp_ev[k].b);
                if ((lp_ev[k].t & LP_MOVE) && lp_ev[k].len >= SP_COUNT)
                    fail("a move of no parameter", lp_ev[k].len, 0);
                n++;
            }
            if (n != c->n)
                fail("a clip's count", (int)n, c->n);
            if (!c->len && n && !(lpr.st == LR_TAKE && lpr.trk == t && lpr.scn == s))
                fail("events in a clip of no length (not a take under way)", (int)(t * 8 + s), (int)n);
            if (c->len > LP_MAXT)
                fail("a clip too long", c->len, 0);
            inclips += n;
        }
    for (k = 0; k < lpr.nundo; k++) {
        uint32_t i = lpr.undo[k];
        if (i >= LP_NEV || seen[i])
            fail("UNDO's events: free or in a clip", (int)i, i < LP_NEV ? seen[i] : -1);
        seen[i] = 3;
    }
    if (nfree + inclips + lpr.nundo != LP_NEV)
        fail("events lost", (int)(nfree + inclips + lpr.nundo), LP_NEV);
    for (k = 0; k < lpr.npass; k++)
        if (lpr.pass[k] >= LP_NEV)
            fail("a pass's event out of range", lpr.pass[k], 0);
    for (k = 0; k < lpr.nlast; k++)
        if (lpr.last[k] >= LP_NEV)
            fail("the last pass's event out of range", lpr.last[k], 0);
    for (t = 0; t < LP_NTRK; t++) {
        uint32_t p = lps[t].play, x = lps[t].next;
        if (p != LP_NOSCN && p >= LP_NSCN)
            fail("a track's scene", (int)t, (int)p);
        if (x != LP_NOSCN && x != LP_STOP && x >= LP_NSCN)
            fail("a track's next", (int)t, (int)x);
        if (p < LP_NSCN && !lps[t].resync && lps[t].cur != LP_END) {
            for (k = lpp.clip[t][p].head; k != LP_END && k != lps[t].cur; k = lp_ev[k].next)
                ;
            if (k == LP_END)
                fail("a track's next event not in its clip", (int)t, lps[t].cur);
        }
        if (lpp.trk[t].type >= LT_N || lpp.trk[t].ch > 15u || lpp.trk[t].cin > 16u)
            fail("a track's settings", (int)t, lpp.trk[t].type);
    }
    if (lpp.sel >= LP_NTRK || lpp.scn >= LP_NSCN)
        fail("the selection", lpp.sel, lpp.scn);
    if (ch_dp != &ch_d[lpp.sel] || ch_sp != &ch_s[lpp.sel] || ch_ti != lpp.sel)
        fail("the chords' side in use is not the selected track's", (int)(ch_dp - ch_d), lpp.sel);
    for (k = 0; k < SY_NVOICE; k++)
        if (sy_v[k].trk > SY_NOTRK)
            fail("a voice's track", (int)k, sy_v[k].trk);
    for (k = 0; k < SP_COUNT; k++)
        if (sp_ovr_on[k] > LP_NTRK)
            fail("a parameter held by no track", (int)k, sp_ovr_on[k]);
    if (lpr.st > LR_DUB)
        fail("the recording's state", lpr.st, 0);
    for (t = 0; t < CH_NTRK; t++)
        if (ch_s[t].nheld > CH_MAXN || ch_s[t].non > 16u || ch_s[t].nq > 16u || ch_s[t].ndq > CH_NDQ ||
            ch_s[t].nan > CH_NAN)
            fail("a chords' side's counts", (int)t, ch_s[t].nheld);
    if (sy_ntrk != SY_NOTRK)
        fail("sy_ntrk left set", sy_ntrk, 0);
    for (k = 0; k < LP_NRAT; k++)                 /* (V0.85: the ratchets to come, the kit in its ranges, the duck) */
        if (lpx.rat[k].left > 3u || lpx.rat[k].row >= DK_ROWS || lpx.rat[k].trk >= LP_NTRK)
            fail("a ratchet's hits to come", (int)k, lpx.rat[k].left);
    {
        dk_kit_t was = dkk;
        dk_sane();
        if (memcmp(&was, &dkk, sizeof dkk))
            fail("the kit out of its ranges", 0, 0);
    }
    if (dkd.st > 3u || !(dkd.g >= 0.0f && dkd.g <= 1.0f) || !(dkd.d >= 0.0f && dkd.d <= 1.0f)
        || dkd.h > (int32_t)(100.0f * DK_HOLD_MS * 0.001f * SY_FS))   /* (3: V0.86's HOLD) */
        fail("the duck's state", dkd.st, (int)(dkd.g * 1000.0f));
    if (!(dkc.e >= 0.0f && dkc.e < 1e3f) || !(dkc.g > 0.0f && dkc.g <= 3.0f))   /* (COMP: its make-up, +9 dB at most) */
        fail("COMP's state", (int)(dkc.e * 1000.0f), (int)(dkc.g * 1000.0f));
    for (k = 0; k < DK_N; k++)
        if (dkv[k].on && (!(dkv[k].a == dkv[k].a) || dkv[k].trk > SY_NOTRK))
            fail("a drum voice", (int)k, dkv[k].trk);
}

/* ---------------------------------------------------------------------------------------------- moves --- */
static uint32_t buttons;
static void act(void)
{
    uint32_t r = rn(100);
    if (r < 22) {                                 /* a button tapped, or held, alone or with OCT */
        uint32_t b = rn(14), hold = rn(10) < 7 ? 40u : rn(10) < 8 ? 700u : 5600u;
        TRACE("%u button %u hold %u\n", step_no, b, hold);
        if (b == 6u && hold > 4000u)
            hold = 700u;                          /* (HOME held 5 s: the SETTINGS; fine, but not too often) */
        buttons = 1u << b;
        web_buttons(buttons);
        run_ms(30);
        if (rn(4) == 0) {
            web_buttons(buttons | (1u << (12u + rn(2))));
            run_ms(40);
            web_buttons(buttons);
        }
        if (rn(3) == 0) {                         /* (a key while it is held: the pad, a root, a source) */
            web_keys(1u << rn(27));
            run_ms(40);
            web_keys(0);
        }
        run_ms(hold);
        buttons = 0;
        web_buttons(0);
        run_ms(rn(400));
    } else if (r < 40) {                          /* keys: a chord or a note, held a while */
        uint32_t m = 0, i, n = 1 + rn(4);
        for (i = 0; i < n; i++)
            m |= 1u << rn(27);
        web_keys(m);
        run_ms(5 + rn(300));
        web_keys(0);
    } else if (r < 52) {                          /* a knob */
        web_enc(rn(7), (int32_t)rn(9) - 4);
        run_ms(rn(60));
    } else if (r < 62) {                          /* MIDI: notes on any channel, a controller, the clock */
        uint32_t q = rn(10), ch = rn(16);
        if (q < 6) {
            uint32_t n = 36 + rn(60);
            web_midi(0x90u | ch, n, 1 + rn(127));
            run_ms(rn(200));
            web_midi(0x80u | ch, n, 0);
        } else if (q < 8) {
            web_midi(0xB0u | ch, rn(128), rn(128));
        } else if (q == 8) {
            web_midi(0xFA + rn(3) * 1u, 0, 0);    /* start, continue, stop */
        } else {
            uint32_t i;
            for (i = 0; i < 24u; i++) {
                web_midi(0xF8, 0, 0);
                run_ms(20);
            }
        }
    } else if (r < 70) {                          /* the looper's own (V0.85: the drums' too) */
        uint32_t q = rn(16), t = rn(8), s = rn(8);
        TRACE("%u looper %u t%u s%u\n", step_no, q, t, s);
        if (q == 0)
            web_test_lp_launch(t, rn(5) == 0 ? LP_STOP : s);
        else if (q == 1)
            web_test_lp_put(t, s, rn(1536), 1 + rn(400), rn(128), 1 + rn(127));
        else if (q == 2)
            web_test_lp_mput(t, s, rn(1536), rn(SP_COUNT), (int32_t)rn(2000) - 1000);
        else if (q == 3)
            web_test_lp_req(1 + rn(3));
        else if (q == 4)
            web_test_lp(0, (int32_t)t);
        else if (q == 5)
            web_test_lp(1, (int32_t)s);
        else if (q == 6) {                        /* (a setting in its range, as the UI keeps it) */
            static const uint8_t TOP[8] = {3, 16, 17, 2, 128, 128, 2, 2};
            uint32_t f = rn(8);
            web_test_lp_trk(t, f, (int32_t)rn(TOP[f]));
        } else if (q == 7) {                      /* (a clip's length in bars, as LENGTH sets it) */
            web_test_lp_clip(t, s, 0, (int32_t)((1u + rn(16)) * 384u));
        } else if (q == 8) {                      /* (V0.85: the grid of the selected clip: a step in or out, a hit's */
            uint32_t f = rn(6);                   /* VELOCITY, ACCENT, CHANCE, RATCHET moved, read) */
            TRACE("%u grid %u row %u step %u\n", step_no, f, t, s);
            web_test_grid(f, rn(9), rn(40), f ? (int32_t)rn(9) - 4 : (int32_t)rn(128));
        } else if (q == 9) {                      /* (the kit: any value, kept in range; V0.86: the drums' effects, */
            if (rn(12) == 0)                      /* the kit's, a factory kit loaded; V0.87 a row's model, any */
                dk_kit_load(rn(DK_NKIT));         /* byte, a factory drum preset into a row) */
            else if (rn(6) == 0)
                dkk.l[rn(8)].model = (uint8_t)rn(10), dkk.l[rn(8)].var = (uint8_t)rn(5), dk_sane();
            else if (rn(8) == 0)
                dk_row_set(rn(8), &DKP[rn(DK_NPRE)], 1u + rn(DK_NPRE));
            else if (rn(2))
                web_test_kit(rn(8), 30u + rn(17), (int32_t)rn(140));
            else
                web_test_kit(rn(8), rn(16), (int32_t)rn(140));
        } else if (q == 10) {                     /* (a hit straight in: any row, the duck's too) */
            web_test_drum_hit(rn(10), rn(9), rn(128), rn(2));
        } else if (lpr.st != LR_OFF && lpr.trk == t && lpr.scn == s) {
            ;                                     /* (V0.86's tools: never on the clip recording, as the UI keeps it) */
        } else if (q == 11) {                     /* (V0.86: TOOLS' PASTE, a clip or a bar of it, from another) */
            uint32_t st = rn(8), ss = rn(8);
            TRACE("%u copy t%u s%u -> t%u s%u\n", step_no, st, ss, t, s);
            if (!(lpr.st != LR_OFF && lpr.trk == st && lpr.scn == ss))
                lp_clip_copy(st, ss, t, s, rn(3) ? -1 : (int32_t)rn(4));
        } else if (q == 12) {                     /* (DOUBLE, HALVE) */
            if (rn(2))
                lp_clip_double(t, s);
            else
                lp_clip_halve(t, s, 1);
        } else if (q == 13) {                     /* (CLEAR's notes) */
            lp_notes_clear(t, s);
        } else if (q == 14) {                     /* (SPEED) */
            lp_speed_set(t, s, rn(LS_N));
        } else {                                  /* (the screen: SCREEN OFF AFTER 1 MIN, a rest past it) */
            if (rn(20) == 0) {
                ui.scr_off = 1;
                run_ms(61000);
                ui.scr_off = 0;
            }
        }
        run_ms(rn(100));
    } else if (r < 76) {                          /* a page */
        uint32_t pg = rn(UI_NPG);
        TRACE("%u page %u %s\n", step_no, pg, UI_PG[pg].name);
        web_test_go(pg);
        run_ms(20 + rn(80));
    } else if (r < 79) {                          /* PLAY */
        web_buttons(1u << 10);
        run_ms(30);
        web_buttons(0);
        run_ms(rn(500));
    } else if (r < 81) {                          /* MASTER */
        web_master(rn(1024));
    } else {                                      /* time */
        run_ms(10 + rn(800));
    }
}

int main(int argc, char **argv)
{
    uint32_t steps = argc > 2 ? (uint32_t)atoi(argv[2]) : 3000u;
    rng = argc > 1 ? (uint32_t)atoi(argv[1]) * 2654435761u + 1u : rng;
    verbose = argc > 3;
    web_nor_erase();
    web_master(700);
    web_boot();
    run_ms(300);
    check();
    for (step_no = 1; step_no <= steps; step_no++) {
        act();
        check();
    }
    printf("fuzz: %u steps clean (seed %s), events free %u, flash writes %u, ms %u\n", steps,
           argc > 1 ? argv[1] : "default", lp_nfree, web_flash_writes_count(), web_now_ms());
    return 0;
}
