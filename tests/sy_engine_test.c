/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* The whole engine (src/sy_engine.c and everything under it) through sy_block, as the audio ISR will run it:
 *   build/host/sy_engine_test tests/sy_golden.txt [DEMODIR]       GOLDEN_UPDATE=1 rewrites the golden file
 * 1. parameters: every row's range, default and name; the lists' names match their ranges; MIDI CCs unique;
 *    a patch packed and unpacked is the same (its drawn shapes too); an older patch (fewer values, no drawn
 *    shapes) takes the defaults of the rest; values out of range are clamped, a malformed drawn shape not read or
 *    made whole; a foreign blob is refused;
 * 2. matrix: a slot moves its destination by amount x source over the whole range; two slots on one destination
 *    add; a slot into a parameter that is not a destination does nothing; macros feed macros; global destinations
 *    take the newest voice's sources;
 * 3. voices: POLY's eight voices, the ninth note steals (not the newest, not the lowest), its fade, then the new
 *    note; VOICES limits them; the same note takes its own voice again; releases end; sustain holds and lets go;
 *    MONO's stack (back to the held note, no restart), LEGATO (no restart while held), MONO restarts; glide's
 *    time; the unison budget trims the oldest voice; the CPU guard steps the budget down and back, sheds a voice;
 * 4. envelopes and LFOs: attack and release times; a FREE LFO's rate; a synced LFO at 120 BPM (SYNC FIXED); RATE's
 *    modulation steps the note value; the time signature's bars, BPM in TS-NOTE beats; the built-in shapes as
 *    points (on their shapes); a drawn shape plays, its edits reach the audio whole, a patch load's too;
 * 5. clicks: a sine patch, every event that could step the output (note on / off, steal, retrigger, MONO, table
 *    swap, patch change, knob and CC jumps, unison count, sources and the filter switched, filter type and warp
 *    changed, bend jumps): the largest step between samples near the event against the steady sine's (<= 2x);
 * 6. health: 40 random patches, notes and knob changes for 3 s each: finite, no denormal filter state (a held
 *    chord's windowed DC printed: slow envelopes and LFOs pass the blockers as slow movement); TUBE drive on a held
 *    chord: no DC at the output (-80 dBFS); offsets: the pulse sub's mean, PWM's, ASYM's and SYNC's over whole
 *    cycles taken out, and the infrasonic thump of a note's attack and release printed for each;
 * 7. determinism and golden renders: eight test sounds play a phrase; the same twice in a run, and as recorded
 *    in tests/sy_golden.txt (a change must be intended: GOLDEN_UPDATE=1); WAVs into DEMODIR;
 * 8. effects: OTT's bands sum back to the input; the synced delay's echo at 1/4 of 120 BPM, TIME's modulation
 *    stepping its note value; every module bounded and finite on full-scale noise; the limiter holds peaks under 1;
 *    the EQ: flat it is its input bit for bit, each band's gain measured against its design and against the curve the
 *    screen draws, bounded with every frequency jumping, its states 0 after silence;
 * 9. cost: host time of a half (128 frames) for a light and two heavy cases, against real time. */
#define SY_ENGINE 1
#include "sy_host.h"
#include "../firmware/src/sy_sounds.h"

#define MAXS (12u * 44118u)
static float L[MAXS], R[MAXS];
static const char *demo;

static void eng_reset(void)
{
    sy_init();
    sy_tables_now();
}

/* n blocks into L / R from offset at; the main loop's service between blocks */
static uint32_t run(uint32_t nb, uint32_t at)
{
    int32_t out[2u * SY_BLK];
    uint32_t b, i;
    for (b = 0; b < nb; b++) {
        sy_block(out);
        sy_service();
        for (i = 0; i < SY_BLK && at < MAXS; i++, at++) {
            L[at] = (float)out[2u * i] * (1.0f / 32768.0f);
            R[at] = (float)out[2u * i + 1u] * (1.0f / 32768.0f);
        }
    }
    return at;
}

static uint32_t blocks(float secs) { return (uint32_t)(secs * SY_FS) / SY_BLK; }
static void set(uint32_t id, int32_t v) { sy_patch.p[id] = sp_clamp(id, v); }
static void snap(void) { sp_snap(); }

static uint32_t nvoices(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < SY_NVOICE; i++)
        n += sy_v[i].on;
    return n;
}

/* the DC of n samples (n a power of two), Blackman-Harris windowed: an infrasonic tone does not leak into it */
#define DC_N 131072u
static double dc_est(const float *x, uint32_t n)
{
    double s = 0, w = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double h = 0.35875 - 0.48829 * cos(2 * M_PI * i / n) + 0.14128 * cos(4 * M_PI * i / n) -
                   0.01168 * cos(6 * M_PI * i / n);
        s += (double)x[i] * h;
        w += h;
    }
    return s / w;
}

static uint64_t hash_out(uint32_t n)
{
    uint64_t h = 1469598103934665603ull;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = (int32_t)(L[i] * 32768.0f), b = (int32_t)(R[i] * 32768.0f);
        h = (h ^ (uint32_t)a) * 1099511628211ull;
        h = (h ^ (uint32_t)b) * 1099511628211ull;
    }
    return h;
}

static int draw_same(const sp_draw_t *a, const sp_draw_t *b)
{
    uint32_t i;
    if (a->n != b->n)
        return 0;
    for (i = 0; i < a->n; i++)
        if (a->x[i] != b->x[i] || a->y[i] != b->y[i] || a->c[i] != b->c[i])
            return 0;
    return 1;
}

/* ------------------------------------------------------------------------------------- 1 parameters --- */
static void t_params(void)
{
    uint32_t i, j, tail;
    int ok = 1, lists = 1, ccs = 1;
    static uint8_t buf[SP_PACK_MAX];
    sy_patch_t a, b, def;
    printf("engine: %u parameters, a patch packs into %u bytes\n", (unsigned)SP_COUNT, (unsigned)SP_PACK_MAX);
    for (i = 0; i < SP_COUNT; i++) {
        const sp_desc_t *d = &SP[i];
        if (!(d->lo < d->hi) || d->def < d->lo || d->def > d->hi || strlen(d->name) > 8u || d->sec >= SEC_COUNT) {
            printf("engine: parameter %u (%s) malformed\n", i, d->name);
            ok = 0;
        }
        for (j = 0; j < i; j++)
            if (d->cc && SP[j].cc == d->cc)
                ccs = 0;
    }
    lists &= SP[SP_A_WARP].hi == WARP_COUNT - 1 && SP[SP_B_WARP].hi == WARP_COUNT - 1;
    lists &= SP[SP_SUB_SHAPE].hi == SUB_NSHAPE - 1 && SP[SP_NZ_TYPE].hi == 2 && SP[SP_F_TYPE].hi == FT_COUNT - 1;
    lists &= SP[SP_L1_SHAPE].hi == LFO_NSHAPE - 1 && SP[SP_L1_SYNC].hi == 1 && SP[SP_DL_SYNC].hi == 1;
    lists &= SP[SP_L1_NOTE].hi == (int16_t)(SY_NNOTE - 1u) && SP[SP_DL_NOTE].hi == (int16_t)(SY_NNOTE - 1u);
    lists &= SP[SP_DR_TYPE].hi == 3;
    lists &= SP[SP_M1_SRC].hi == SRC_COUNT - 1 && SP[SP_A_TABLE].hi == (int16_t)(WT_NFACT - 1);
    check("every parameter: lo < hi, default in range, name <= 8, a section", ok);
    check("list parameters span their lists (warps, shapes, types, sources, tables)", lists);
    check("default MIDI CCs: each on one parameter", ccs);
    {
        uint32_t s = 7;
        int same = 1;
        sp_defaults(&a);
        for (i = 0; i < SP_COUNT; i++)
            a.p[i] = sp_clamp(i, SP[i].lo + (int32_t)(sy_rand(&s) % (uint32_t)(SP[i].hi - SP[i].lo + 1)));
        memcpy(a.name, "TEST PATCH 12345", 17);
        a.cat = CAT_PAD;
        sp_shape_pts(LFO_STAIRS, &a.draw[0]);             /* drawn shapes: 16 points; 5, moved and curved */
        a.draw[1].y[1] = 640;
        a.draw[1].c[2] = -777;
        a.draw[1].x[3] = 801;
        j = sp_pack(&a, buf);
        same = sp_unpack(&b, buf, j) && !memcmp(a.p, b.p, sizeof a.p) && !memcmp(a.name, b.name, 16) &&
               a.cat == b.cat && draw_same(&a.draw[0], &b.draw[0]) && draw_same(&a.draw[1], &b.draw[1]);
        check("a patch packed and unpacked is the same (its drawn shapes too)", same);
        tail = j - 6u - 2u * SP_COUNT;                    /* the name, category, macros, drawn shapes */
        buf[4] = (uint8_t)(SP_COUNT - 10u);               /* an older build: 10 values fewer */
        memmove(buf + 6u + 2u * (SP_COUNT - 10u), buf + 6u + 2u * SP_COUNT, tail);
        same = sp_unpack(&b, buf, j - 20u);
        for (i = SP_COUNT - 10u; i < SP_COUNT; i++)
            same &= b.p[i] == SP[i].def;
        for (i = 0; i < SP_COUNT - 10u; i++)
            same &= b.p[i] == a.p[i];
        same &= draw_same(&a.draw[1], &b.draw[1]);
        check("a patch with fewer values: the rest take their defaults", same);
        sp_defaults(&def);
        j = sp_pack(&a, buf);
        same = sp_unpack(&b, buf, 6u + 2u * SP_COUNT + 49u) && draw_same(&b.draw[0], &def.draw[0]) &&
               draw_same(&b.draw[1], &def.draw[1]) && !memcmp(a.p, b.p, sizeof a.p);
        check("a patch without drawn shapes (an older build): the default's", same);
        buf[6u + 2u * SP_COUNT + 50u] = SP_NPT + 1u;      /* LFO 1 with 17 points: neither is read */
        same = sp_unpack(&b, buf, j) && draw_same(&b.draw[0], &def.draw[0]) && draw_same(&b.draw[1], &def.draw[1]);
        b = a;                                            /* LFO 2's out of order and range: made whole */
        b.draw[1].x[0] = 50;
        b.draw[1].x[2] = 900;
        b.draw[1].x[4] = 990;
        b.draw[1].y[0] = 5000;
        b.draw[1].c[4] = -3000;
        j = sp_pack(&b, buf);
        same &= sp_unpack(&b, buf, j) && b.draw[1].n == 5u && b.draw[1].x[0] == 0 && b.draw[1].x[2] == 900 &&
                b.draw[1].x[3] == 900 && b.draw[1].x[4] == 1000 && b.draw[1].y[0] == 1000 && b.draw[1].c[4] == -1000;
        check("a drawn shape malformed: too many points, not read; out of order or range, made whole", same);
        j = sp_pack(&a, buf);
        buf[6] = 0xFF;                                    /* A ON = 32767 */
        buf[7] = 0x7F;
        same = sp_unpack(&b, buf, j) && b.p[0] == SP[0].hi;
        check("values out of range are clamped", same);
        buf[0] = 'X';
        check("a foreign blob is refused", !sp_unpack(&b, buf, j));
    }
    {                                                     /* the values read as stored follow a change at once */
        static const uint16_t IDS[] = {SP_V_GLIDE, SP_V_VEL, SP_NZ_STEREO, SP_RV_PRE, SP_L1_DELAY, SP_L1_RISE,
                                       SP_L1_SMOOTH, SP_L2_DELAY, SP_L2_RISE, SP_L2_SMOOTH};
        uint32_t k, tested = 0;
        int ok = 1;
        eng_reset();
        snap();
        for (k = 0; k < sizeof IDS / sizeof IDS[0]; k++)  /* (no patch load after: the UI and MIDI write so) */
            set(IDS[k], SP[IDS[k]].lo + (SP[IDS[k]].def - SP[IDS[k]].lo + 333) % (SP[IDS[k]].hi - SP[IDS[k]].lo + 1));
        run(1, 0);
        for (k = 0; k < sizeof IDS / sizeof IDS[0]; k++)
            if (!(SP[IDS[k]].flags & F_SMOOTH)) {
                ok &= sp_n[IDS[k]] == sp_norm_of(IDS[k], sy_patch.p[IDS[k]]);
                tested++;
            }
        check("values read as stored (GLIDE, VEL AMP, STEREO, PREDELAY, the LFOs' DELAY, RISE, SMOOTH) take a change "
              "at once, without a patch load", ok && tested >= 8u);
    }
    {                                                     /* every value as text (sy_text.c) */
        static const struct {
            uint16_t id;
            int16_t v;
            const char *t;
        } SPOT[] = {{SP_F_CUT, 1000, "20.0 kHz"}, {SP_F_CUT, 0, "20.0 Hz"}, {SP_E1_ATK, 0, "1.0 ms"},
                    {SP_E1_ATK, 1000, "30.0 s"}, {SP_E1_HOLD, 0, "OFF"}, {SP_V_VOL, 600, "0.0 dB"}, {SP_V_VOL, 0, "OFF"},
                    {SP_A_POS, 750, "75.0%"}, {SP_A_PAN, -250, "-25.0%"}, {SP_A_WARP, WARP_SYNC, "SYNC"},
                    {SP_F_TYPE, FT_LP24, "LP 24"}, {SP_L1_RATE, 0, "0.01 Hz"}, {SP_DL_TIME, 1000, "750 ms"},
                    {SP_DL_TIME, 0, "10 ms"}, {SP_OT_LOW, 30, "+3.0 dB"}, {SP_M1_DST, SP_F_CUT, "FLT CUTOFF"},
                    {SP_M1_DST, SP_A_ON, "NONE"}, {SP_A_FINE, -7, "-7 ct"}, {SP_V_BUP, 12, "12 st"},
                    {SP_EQ_LFREQ, 292, "150 Hz"}, {SP_EQ_MFREQ, 567, "1.00 kHz"}, {SP_EQ_HFREQ, 767, "4.00 kHz"},
                    {SP_EQ_Q, 400, "1.00"}, {SP_EQ_MID, -180, "-18.0 dB"}, {SP_M1_DST, SP_EQ_MID, "EQ MID"}};
        char t[SP_TEXT_N + 4u];
        uint32_t id, k, nl;
        int lists = 1, fits = 1, spot = 1;
        int32_t v;
        for (id = 0; id < SP_COUNT; id++) {
            if (SP[id].unit == U_LIST)
                lists &= sp_list(id, &nl) != 0 && SP[id].lo == 0 && nl == (uint32_t)SP[id].hi + 1u;
            for (v = SP[id].lo; v <= SP[id].hi; v++) {
                memset(t, 0x55, sizeof t);
                sp_text(id, v, t);
                fits &= t[0] != 0 && strlen(t) < SP_TEXT_N && !strchr(t, '?') && (uint8_t)t[SP_TEXT_N] == 0x55u;
            }
        }
        for (k = 0; k < sizeof SPOT / sizeof SPOT[0]; k++) {
            sp_text(SPOT[k].id, SPOT[k].v, t);
            if (strcmp(t, SPOT[k].t)) {
                spot = 0;
                printf("engine: %s %d as \"%s\", not \"%s\"\n", SP[SPOT[k].id].name, SPOT[k].v, t, SPOT[k].t);
            }
        }
        check("every list parameter names each of its values", lists);
        check("every value of every parameter as text: set, under 16 characters, no unknown", fits);
        check("values as text: cutoff, times, levels, lists, destinations as expected", spot);
    }
}

/* ----------------------------------------------------------------------------------------- 2 matrix --- */
static void t_matrix(void)
{
    float vs[SRC_VOICE_LAST + 1u] = {0}, off[NSLOT];
    eng_reset();
    set(SP_M1_SRC, SRC_LFO1);
    set(SP_M1_DST, SP_F_CUT);
    set(SP_M1_AMT, 500);
    set(SP_M2_SRC, SRC_ENV2);
    set(SP_M2_DST, SP_F_CUT);
    set(SP_M2_AMT, -250);
    set(SP_M3_SRC, SRC_LFO1);
    set(SP_M3_DST, SP_A_UNI);                             /* not a destination: refused by sp_clamp -> 0 */
    set(SP_M3_AMT, 1000);
    snap();
    mod_map();
    vs[SRC_LFO1] = 0.4f;
    vs[SRC_ENV2] = 1.0f;
    mod_offsets(off, vs, 0);
    check("matrix: 50 % of LFO 0.4 and -25 % of ENV 1.0 add on the cutoff",
          fabsf(mod_get(off, SP_F_CUT) - sy_clampf(sp_n[SP_F_CUT] + 0.2f - 0.25f, 0.0f, 1.0f)) < 1e-6f);
    check("matrix: a slot into a parameter that is not a destination does nothing",
          sy_patch.p[SP_M3_DST] == 0 && mod_idx[SP_A_UNI] == 0xFF);
    vs[SRC_LFO1] = 1.0f;
    vs[SRC_ENV2] = 0.0f;
    set(SP_M1_AMT, 1000);
    mod_map();
    mod_offsets(off, vs, 0);
    check("matrix: 100 % moves across the whole range (clamped at the top)", mod_get(off, SP_F_CUT) == 1.0f);
    eng_reset();
    set(SP_MAC1, 1000);
    set(SP_M1_SRC, SRC_MAC1);
    set(SP_M1_DST, SP_MAC2);
    set(SP_M1_AMT, 500);
    set(SP_M2_SRC, SRC_MAC2);
    set(SP_M2_DST, SP_DL_MIX);
    set(SP_M2_AMT, 400);
    snap();
    run(2, 0);
    check("matrix: MACRO 1 -> MACRO 2 (50 %) -> DELAY MIX (40 %): +0.2", fabsf(mod_get(sy_goff, SP_DL_MIX) -
          sy_clampf(sp_n[SP_DL_MIX] + 0.2f, 0.0f, 1.0f)) < 1e-5f);
    eng_reset();
    set(SP_M1_SRC, SRC_VEL);
    set(SP_M1_DST, SP_RV_MIX);
    set(SP_M1_AMT, 1000);
    snap();
    sy_note_on(60, 127);
    run(1, 0);
    sy_note_on(64, 32);
    run(2, 0);
    check("matrix: a global destination takes the newest voice's sources (velocity 32)",
          fabsf(mod_get(sy_goff, SP_RV_MIX) - sy_clampf(sp_n[SP_RV_MIX] + 32.0f / 127.0f, 0.0f, 1.0f)) < 1e-5f);
}

/* ----------------------------------------------------------------------------------------- 3 voices --- */
static void t_voices(void)
{
    uint32_t i, k;
    int ok;
    eng_reset();
    set(SP_E1_REL, 300);
    snap();
    for (i = 0; i < 8u; i++) {
        sy_note_on(48 + 3 * i, 100);
        run(1, 0);
    }
    check("POLY: eight notes, eight voices", nvoices() == 8u);
    sy_note_on(80, 100);
    {
        sy_voice_t *st = 0;
        for (i = 0; i < SY_NVOICE; i++)
            if (sy_v[i].steal)
                st = &sy_v[i];
        check("the ninth note steals a voice: the oldest that is not the lowest note (49 -> the second oldest)",
              st && st->note == 51);
        run(VOICE_STEAL_BLKS + 1u, 0);
        ok = 0;
        for (i = 0; i < SY_NVOICE; i++)
            ok |= sy_v[i].on && sy_v[i].note == 80;
        check("after the steal's fade the new note plays", ok && nvoices() == 8u);
    }
    eng_reset();
    set(SP_V_POLY, 4);
    snap();
    for (i = 0; i < 6u; i++) {
        sy_note_on(60 + i, 100);
        run(VOICE_STEAL_BLKS + 2u, 0);
    }
    ok = 1;
    for (i = 4; i < SY_NVOICE; i++)
        ok &= !sy_v[i].on;
    check("VOICES 4: four voices at most", ok && nvoices() == 4u);
    eng_reset();
    sy_note_on(60, 100);
    run(2, 0);
    sy_note_on(60, 100);
    run(2, 0);
    check("the same note again takes its own voice", nvoices() == 1u);
    sy_note_off(60);
    run(blocks(0.6f), 0);
    check("a release ends (the default 15 ms)", nvoices() == 0u);
    eng_reset();
    sy_note_on(60, 100);
    run(2, 0);
    sy_cc(64, 127);
    sy_note_off(60);
    run(blocks(0.5f), 0);
    check("sustain holds a released note", nvoices() == 1u && sy_v[0].gate);
    sy_cc(64, 0);
    run(blocks(0.5f), 0);
    check("sustain off lets it go", nvoices() == 0u);

    eng_reset();
    set(SP_V_MODE, 1);                                    /* MONO */
    snap();
    sy_note_on(60, 100);
    run(blocks(0.3f), 0);
    sy_note_on(64, 100);
    run(1, 0);
    check("MONO: the new note restarts the envelope", sy_v[0].env[0].st == ENV_ATK || sy_v[0].env[0].st == ENV_DEC);
    run(blocks(0.3f), 0);
    sy_note_off(64);
    run(2, 0);
    check("MONO: back to the note still held, without a restart",
          sy_v[0].target == 60.0f && sy_v[0].env[0].st != ENV_ATK && sy_v[0].env[0].st != ENV_IDLE);
    sy_note_off(60);
    run(blocks(0.5f), 0);
    check("MONO: the last note off releases", nvoices() == 0u);
    eng_reset();
    set(SP_V_MODE, 2);                                    /* LEGATO */
    snap();
    sy_note_on(60, 100);
    run(blocks(0.3f), 0);
    sy_note_on(64, 100);
    run(1, 0);
    check("LEGATO: a note while one is held moves the pitch only",
          sy_v[0].target == 64.0f && sy_v[0].env[0].st != ENV_ATK && sy_v[0].env[0].st != ENV_IDLE);
    eng_reset();
    set(SP_V_MODE, 1);
    set(SP_V_GLIDE, 447);                                 /* 100 ms */
    snap();
    sy_note_on(60, 100);
    run(blocks(0.2f), 0);
    sy_note_on(72, 100);
    for (k = 0; k < blocks(0.5f) && sy_v[0].pitch < 71.76f; k++)
        run(1, 0);
    printf("engine: glide 100 ms: 98 %% of an octave in %.0f ms\n", (double)k * SY_BLK * 1000.0 / (double)SY_FS);
    check("glide: 98 % of the way in GLIDE's time (+-30 %)", k * SY_BLK > 0.07 * (double)SY_FS && k * SY_BLK < 0.13 * (double)SY_FS);

    eng_reset();
    set(SP_A_UNI, 16);
    set(SP_B_ON, 1);
    set(SP_B_UNI, 16);
    snap();
    sy_note_on(60, 100);
    run(2, 0);
    sy_note_on(64, 100);
    run(2, 0);
    printf("engine: unison 2 x 16 on two voices: %u instances (budget %u), the older voice capped at %u\n",
           sy_instances(), sy_uni_budget, sy_v[0].uni_cap);
    check("unison budget: two voices of 2 x 16 stay within 48 instances, the older voice gives way",
          sy_instances() <= 48u && sy_v[0].uni_cap < 16u && sy_v[1].uni_cap == 16u);

    eng_reset();
    for (i = 0; i < 4u; i++) {
        sy_note_on(60 + i, 100);
        run(1, 0);
    }
    k = (uint32_t)sy_cpu(250) + (uint32_t)sy_cpu(250);
    run(1, 0);
    ok = 0;
    for (i = 0; i < SY_NVOICE; i++)
        ok |= sy_v[i].steal != 0;
    check("CPU guard: two halves over 85 % shed a voice (faded)", k == 1u && (ok || nvoices() < 4u));
    for (i = 0; i < 20u; i++)
        sy_cpu(190);
    check("CPU guard: over 70 % the unison budget steps down to 16", sy_uni_budget == 16u);
    for (i = 0; i < 20u; i++)
        sy_cpu(210);
    check("CPU guard: over 78 % the effects get cheaper", fx_lo == 1);
    for (i = 0; i < 345u * 6u; i++)
        sy_cpu(100);
    check("CPU guard: under 55 % for a second, one step back at a time, all the way", sy_uni_budget == 48u && !fx_lo);
}

/* ------------------------------------------------------------------------------- 4 envelopes, LFOs --- */
static void t_env_lfo(void)
{
    uint32_t k;
    eng_reset();
    set(SP_E1_ATK, 447);                                  /* 100 ms */
    set(SP_E1_REL, 447);
    snap();
    sy_note_on(60, 127);
    for (k = 0; k < blocks(1.0f) && sy_v[0].src[SRC_ENV1] < 1.0f; k++)
        run(1, 0);
    printf("engine: ATTACK 100 ms: at the top in %.1f ms\n", (double)k * SY_BLK * 1000.0 / (double)SY_FS);
    check("attack 100 ms reaches 1 in 100 ms (+-2 blocks)", fabs(k * SY_BLK - 0.1 * (double)SY_FS) <= 2.0 * SY_BLK);
    sy_note_off(60);
    for (k = 0; k < blocks(1.0f) && sy_v[0].on; k++)
        run(1, 0);
    printf("engine: RELEASE 100 ms: silent in %.1f ms\n", (double)k * SY_BLK * 1000.0 / (double)SY_FS);
    check("release 100 ms ends in 100 ms (+-2 blocks)", fabs(k * SY_BLK - 0.1 * (double)SY_FS) <= 2.0 * SY_BLK);
    {
        uint32_t cyc = 0, b;
        float last = 0.0f, hz;
        eng_reset();
        set(SP_L1_MODE, LFOM_FREE);
        set(SP_L1_SHAPE, LFO_SAWUP);
        set(SP_L1_RATE, 600);                             /* 0.01 * 4000^0.6 = 1.45 Hz */
        snap();
        for (b = 0; b < blocks(10.0f); b++) {
            run(1, 0);
            if (sy_glfo_v[0] < last - 1.0f)
                cyc++;
            last = sy_glfo_v[0];
        }
        hz = sp_lfo_hz(0.6f);
        printf("engine: LFO at %.3f Hz: %u cycles in 10 s\n", (double)hz, cyc);
        check("a FREE LFO's rate (cycles in 10 s, +-1)", fabs(cyc - 10.0 * (double)hz) <= 1.0);
        eng_reset();
        set(SP_L1_MODE, LFOM_FREE);
        set(SP_L1_SHAPE, LFO_SAWUP);
        set(SP_L1_SYNC, 1);                               /* FIXED, 1/4 */
        set(SP_L1_NOTE, 8);
        snap();
        cyc = 0;
        last = 0.0f;
        for (b = 0; b < blocks(10.0f); b++) {
            run(1, 0);
            if (sy_glfo_v[0] < last - 1.0f)
                cyc++;
            last = sy_glfo_v[0];
        }
        check("a synced LFO (1/4 at 120 BPM: 2 Hz): 20 cycles in 10 s (+-1)", cyc >= 19u && cyc <= 21u);
        eng_reset();                                      /* RATE's modulation steps the note value */
        set(SP_L1_MODE, LFOM_RETRIG);
        set(SP_L1_SHAPE, LFO_SAWUP);
        set(SP_L1_SYNC, 1);
        set(SP_L1_NOTE, 8);                               /* 1/4 */
        set(SP_M1_SRC, SRC_MAC1);
        set(SP_M1_DST, SP_L1_RATE);
        set(SP_M1_AMT, 1000);
        set(SP_MAC1, 250);                                /* 25 % of the 17 values: 4 on, 1/8T */
        snap();
        sy_note_on(60, 100);
        cyc = 0;
        last = 0.0f;
        for (b = 0; b < blocks(10.0f); b++) {
            const sy_voice_t *v = &sy_v[0];
            uint32_t i;
            run(1, 0);
            for (i = 0; i < SY_NVOICE; i++)
                if (sy_v[i].on)
                    v = &sy_v[i];
            if (v->src[SRC_LFO1] < last - 1.0f)
                cyc++;
            last = v->src[SRC_LFO1];
        }
        printf("engine: FIXED 1/4, RATE + 25 %%: %u cycles in 10 s\n", cyc);
        check("FIXED: RATE's modulation steps the note value (1/4 + 25 %: 1/8T, 6 Hz): 60 cycles in 10 s (+-1)",
              cyc >= 59u && cyc <= 61u);
    }
    {                                                     /* the tempo and the time signature (GLOBAL) */
        uint32_t cyc = 0, b;
        float last = 0.0f, bar34, bar68, q68, bar716;
        eng_reset();
        sy_set_tempo(120, 3, 4);
        bar34 = sy_note_q(3);
        sy_set_tempo(120, 6, 8);
        bar68 = sy_note_q(3);
        q68 = sy_tempo;
        sy_set_tempo(120, 7, 16);
        bar716 = sy_note_q(3);
        sy_set_tempo(120, 6, 8);
        set(SP_L1_MODE, LFOM_FREE);
        set(SP_L1_SHAPE, LFO_SAWUP);
        set(SP_L1_SYNC, 1);
        set(SP_L1_NOTE, 8);                               /* 1/4: 1 s when a beat is an eighth at 120 */
        snap();
        for (b = 0; b < blocks(10.0f); b++) {
            run(1, 0);
            if (sy_glfo_v[0] < last - 1.0f)
                cyc++;
            last = sy_glfo_v[0];
        }
        printf("engine: 1 BAR in quarter notes: 3/4 %.2f, 6/8 %.2f, 7/16 %.2f; 120 BPM in 6/8: a quarter at %.0f a "
               "minute, a FIXED 1/4 LFO %u cycles in 10 s\n", (double)bar34, (double)bar68, (double)bar716, (double)q68,
               cyc);
        check("time signature: 1 BAR is TS-BEAT beats of TS-NOTE (3/4: 3, 6/8: 3, 7/16: 1.75 quarter notes)",
              bar34 == 3.0f && bar68 == 3.0f && bar716 == 1.75f);
        check("BPM counts TS-NOTE beats (120 in 6/8: a quarter note a second; a FIXED 1/4 LFO 10 cycles in 10 s +-1)",
              q68 == 60.0f && cyc >= 9u && cyc <= 11u);
        sy_set_tempo(120, 4, 4);
    }
    {                                                     /* the built-in shapes as points (DRAWN starts from them) */
        uint32_t s, j, len = 0;
        int whole = 1, on = 1;
        double worst[LFO_NSHAPE] = {0};
        lfo_t l;
        lfo_par_t p;
        lfo_draw_t dr;
        sp_draw_t d, f;
        memset(&l, 0, sizeof l);
        memset(&p, 0, sizeof p);
        for (s = 0; s < SP_NSHAPE_PTS; s++) {
            sp_shape_pts(s, &d);
            f = d;
            sp_draw_fix(&f);
            whole &= d.n >= 2u && draw_same(&d, &f);
            len += 1u + 3u * d.n;
            if (s == LFO_SH || s == LFO_SMOOTH)           /* (random: sample levels) */
                continue;
            lfo_draw_of(&dr, &d);
            p.shape = (uint8_t)s;
            for (j = 0; j < 1000u; j++) {
                float x = ((float)j + 0.5f) * 0.001f;
                worst[s] = fmax(worst[s], fabs((double)(lfo_draw_at(&dr, x) - lfo_shape(&l, &p, x))));
            }
            on &= worst[s] <= (s == LFO_SINE ? 0.06 : s == LFO_BOUNCE ? 0.11 : 0.001);
        }
        whole &= len == sizeof SP_SHAPE_PTS / sizeof SP_SHAPE_PTS[0];
        printf("engine: the shapes as points: SINE within %.4f, BOUNCE %.4f, the rest %.4f\n", worst[LFO_SINE],
               worst[LFO_BOUNCE], fmax(fmax(worst[LFO_TRI], worst[LFO_STEP4]), fmax(worst[LFO_STAIRS], worst[LFO_TRAPEZ])));
        check("the built-in shapes as points: each whole, the table read to its end", whole);
        check("the points on their shapes: exact (0.001), SINE's curves within 0.06, BOUNCE's 0.11", on);
    }
    {                                                     /* a drawn shape plays; its edits reach the audio whole */
        static const int16_t X[4] = {0, 300, 300, 1000}, Y[4] = {1000, 1000, -1000, -1000};
        static sy_patch_t lp;
        uint32_t b, hi = 0, n = blocks(10.0f);
        int held;
        eng_reset();
        set(SP_L1_MODE, LFOM_FREE);
        set(SP_L1_SHAPE, LFO_DRAW);
        set(SP_L1_RATE, 800);                             /* 7.6 Hz: 76 cycles in 10 s */
        sy_draw_edit();
        sy_patch.draw[0].n = 4;
        for (b = 0; b < 4u; b++) {
            sy_patch.draw[0].x[b] = X[b];
            sy_patch.draw[0].y[b] = Y[b];
            sy_patch.draw[0].c[b] = 0;
        }
        sy_draw_done();
        snap();
        for (b = 0; b < n; b++) {
            run(1, 0);
            hi += sy_glfo_v[0] > 0.0f;
        }
        printf("engine: a drawn pulse, high 30 %% of its cycle: high %.1f %% of 10 s\n", 100.0 * hi / n);
        check("a drawn LFO plays its points (a pulse high 30 % of its cycle: 30 +- 1.5 %)",
              fabs(100.0 * hi / n - 30.0) <= 1.5);
        sy_draw_edit();                                   /* an edit under way: the last shape plays on */
        sy_patch.draw[0].y[0] = sy_patch.draw[0].y[1] = -1000;
        run(4, 0);
        held = lfo_dr[0].y[0] > 0.5f;
        sy_draw_done();
        run(1, 0);
        check("an edit of a drawn shape: not taken while under way, taken at the next block when done",
              held && lfo_dr[0].y[0] < -0.5f);
        lp = sy_patch;
        sp_shape_pts(LFO_SAWDN, &lp.draw[0]);
        sy_patch_load(&lp);
        run(8, 0);
        check("a patch load takes its drawn shapes", lfo_dr[0].n == 2u && lfo_dr[0].y[0] > 0.99f &&
                                                     lfo_dr[0].y[1] < -0.99f && !sy_next_st);
    }
}

/* ----------------------------------------------------------------------------------------- 5 clicks --- */
static double steady_step;
static void click_patch(void)
{
    eng_reset();
    set(SP_A_TABLE, 5);                                   /* SINE TO TRI at 0: a sine */
    set(SP_A_POS, 0);
    set(SP_E1_ATK, 186);                                  /* 10 ms */
    set(SP_E1_REL, 334);                                  /* 50 ms */
    set(SP_A_RAND, 0);
    set(SP_A_LEVEL, 700);
    snap();
    sy_tables_now();
}

/* the largest sample step from a to b against the steady signal's: the larger of the 0.2 s before (up to s) and
 * the 0.15 s after (from e), never under the plain sine's */
static double click_ratio(uint32_t a, uint32_t b, uint32_t s, uint32_t e, uint32_t end)
{
    double m = fmax(max_step(L + a, b - a), max_step(R + a, b - a)), ref = steady_step;
    uint32_t w = (uint32_t)(0.2 * (double)SY_FS);
    if (s > w)
        ref = fmax(ref, fmax(max_step(L + s - w, w), max_step(R + s - w, w)));
    if (end > e)
        ref = fmax(ref, fmax(max_step(L + e, end - e), max_step(R + e, end - e)));
    return m / ref;
}

static void t_clicks(void)
{
    uint32_t at, ev, i;
    double worst = 0;
    const char *wname = "";
    int ok = 1;
    click_patch();
    sy_note_on(57, 100);
    at = run(blocks(0.5f), 0);
    steady_step = fmax(max_step(L + at / 2, at / 2), max_step(R + at / 2, at / 2));
    printf("engine: clicks: the steady sine (220 Hz) steps %.5f a sample at most\n", steady_step);
    for (ev = 0; ev < 17u; ev++) {
        static const char *const NAMES[17] = {"note on", "note off", "steal", "retrigger", "MONO note",
                                              "table swap", "patch change", "cutoff jump", "macro jump",
                                              "unison 1 -> 7", "OSC B on / off", "SUB on / off", "filter on / off",
                                              "filter type", "warp change", "bend jump", "level jump"};
        double r;
        uint32_t e0;
        click_patch();
        if (ev == 4u)
            set(SP_V_MODE, 1), snap();
        if (ev == 7u || ev == 12u || ev == 13u)
            set(SP_F_ON, 1), set(SP_F_CUT, 400), snap();
        if (ev == 8u)
            set(SP_M1_SRC, SRC_MAC1), set(SP_M1_DST, SP_A_LEVEL), set(SP_M1_AMT, -1000), snap();
        if (ev == 10u)
            set(SP_B_TABLE, 5), snap(), sy_tables_now();
        if (ev == 11u)
            set(SP_SUB_SHAPE, SUB_SINE), set(SP_SUB_LEVEL, 500), snap();
        if (ev == 14u)
            set(SP_A_WAMT, 500), snap();
        if (ev == 2u) {                                   /* eight voices sounding, then a ninth note */
            for (i = 0; i < 8u; i++)
                sy_note_on(45 + 2 * i, 100);
        } else if (ev != 0u) {
            sy_note_on(57, 100);
        }
        at = run(blocks(0.3f), 0);
        e0 = at;
        switch (ev) {
        case 0: sy_note_on(57, 100); break;
        case 1: sy_note_off(57); break;
        case 2: sy_note_on(70, 100); break;
        case 3: sy_note_on(57, 100); break;
        case 4: sy_note_on(60, 100); break;
        case 5: set(SP_A_TABLE, 36); break;              /* FM 1:1 at 0: a sine too */
        case 6: {
            static sy_patch_t np;
            np = sy_patch;
            np.p[SP_A_TABLE] = 36;
            sy_patch_load(&np);
            break;
        }
        case 7: set(SP_F_CUT, 1000); break;
        case 8: sy_patch.p[SP_MAC1] = 1000; break;
        case 9: set(SP_A_UNI, 7); break;
        case 10: set(SP_B_ON, 1); break;
        case 11: set(SP_SUB_ON, 1); break;
        case 12: set(SP_F_ON, 0); break;
        case 13: set(SP_F_TYPE, FT_HP12); break;
        case 14: set(SP_A_WARP, WARP_BENDP); break;
        case 15: sy_bend(16383); break;
        case 16: set(SP_A_LEVEL, 100); break;
        }
        at = run(blocks(0.3f), at);
        {
            uint32_t w5 = (uint32_t)(0.05 * (double)SY_FS), w15 = (uint32_t)(0.15 * (double)SY_FS);
            r = click_ratio(e0 > 64u ? e0 - 64u : 0u, e0 + w5, e0 > 64u ? e0 - 64u : 0u, e0 + w15, at);
            if (ev == 10u || ev == 11u || ev == 12u) {    /* and back */
                uint32_t e1 = at;
                if (ev == 10u)
                    set(SP_B_ON, 0);
                if (ev == 11u)
                    set(SP_SUB_ON, 0);
                if (ev == 12u)
                    set(SP_F_ON, 1);
                at = run(blocks(0.3f), at);
                r = fmax(r, click_ratio(e1 - 64u, e1 + w5, e1 - 64u, e1 + w15, at));
            }
        }
        printf("engine:   %-16s largest step %.2f x the steady sine's\n", NAMES[ev], r);
        if (r > worst)
            worst = r, wname = NAMES[ev];
        ok &= r <= 2.0;
        if (demo && ev == 6u) {
            char path[256];
            snprintf(path, sizeof path, "%s/click_patch_change.wav", demo);
            wav_write(path, L, R, at);
        }
    }
    printf("engine: clicks: the worst %s at %.2f x\n", wname, worst);
    check("clicks: no event steps the output more than twice the steady sine", ok);
}

/* ----------------------------------------------------------------------------------------- 6 health --- */
static void t_health(void)
{
    uint32_t p, i, b, s = 12345, at = 0, denorm = 0;
    int finite = 1;
    double dc = 0, dcw = 0;
    uint64_t n = 0;
    for (p = 0; p < 40u; p++) {
        eng_reset();
        for (i = 0; i < SP_COUNT; i++) {
            if (i == SP_V_VOL)
                continue;
            set(i, SP[i].lo + (int32_t)(sy_rand(&s) % (uint32_t)(SP[i].hi - SP[i].lo + 1)));
        }
        snap();
        for (b = 0; b < blocks(3.0f); b++) {
            int32_t out[2u * SY_BLK];
            uint32_t r = sy_rand(&s);
            if ((r & 63u) == 0u)
                sy_note_on(36 + (r >> 8) % 60u, 1 + (r >> 16) % 127u);
            if ((r & 63u) == 1u)
                sy_note_off(36 + (r >> 8) % 60u);
            if ((r & 127u) == 2u) {                       /* a knob */
                uint32_t id = (r >> 8) % SP_COUNT;
                if (id != SP_V_VOL)
                    set(id, SP[id].lo + (int32_t)((r >> 12) % (uint32_t)(SP[id].hi - SP[id].lo + 1)));
            }
            if ((r & 255u) == 3u)
                sy_bend((int32_t)((r >> 8) & 16383u));
            sy_block(out);
            sy_service();
            for (i = 0; i < 2u * SY_BLK; i++) {
                float x = (float)out[i] * (1.0f / 32768.0f);
                dc += (double)x;
                n++;
            }
            for (i = 0; i < SY_NVOICE; i++) {
                uint32_t c, k;
                const sy_voice_t *v = &sy_v[i];
                if (!isfinite(v->amp) || !isfinite(v->pitch))
                    finite = 0;
                for (c = 0; c < 2u; c++)
                    for (k = 0; k < 6u; k++) {
                        if (!isfinite(v->flt.s[c][k]))
                            finite = 0;
                        if (fpclassify(v->flt.s[c][k]) == FP_SUBNORMAL)
                            denorm++;
                    }
            }
            if (!isfinite(fx.dly_lp[0]) || !isfinite(fx.rv_lp[0]) || !isfinite(fx.ott_env[0]) || !isfinite(fx.lim_g))
                finite = 0;
        }
        sy_all_notes_off();                               /* then a chord held 4 s, without the noise (its mean */
        set(SP_NZ_ON, 0);                                 /* over a window is not an offset): the last 3 s's DC, */
        run(blocks(0.5f), 0);                             /* windowed (no infrasonic tone leaks into it) */
        sy_note_on(48, 100);
        sy_note_on(55, 100);
        sy_note_on(64, 100);
        at = run(blocks(4.0f), 0);
        {
            double d = fmax(fabs(dc_est(L + at - DC_N, DC_N)), fabs(dc_est(R + at - DC_N, DC_N)));
            if (d > 1e-3 || getenv("VERBOSE"))
                printf("engine:   patch %2u: offset %.2e (A %s oct %d semi %d warp %s, B %s oct %d warp %s, SUB %s %s oct %d, "
                       "NOISE %s, filter %s %s, LFO1 %.2f Hz)\n", p, d, sy_patch.p[SP_A_ON] ? "on" : "off",
                       sy_patch.p[SP_A_OCT], sy_patch.p[SP_A_SEMI], WARP_NAMES[sy_patch.p[SP_A_WARP]],
                       sy_patch.p[SP_B_ON] ? "on" : "off", sy_patch.p[SP_B_OCT], WARP_NAMES[sy_patch.p[SP_B_WARP]],
                       sy_patch.p[SP_SUB_ON] ? "on" : "off", SUB_NAMES[sy_patch.p[SP_SUB_SHAPE]], sy_patch.p[SP_SUB_OCT],
                       sy_patch.p[SP_NZ_ON] ? "on" : "off", sy_patch.p[SP_F_ON] ? "on" : "off",
                       FT_NAMES[sy_patch.p[SP_F_TYPE]], (double)sp_lfo_hz(sp_n[SP_L1_RATE]));
            dcw = fmax(dcw, d);
        }
    }
    dc /= (double)n;
    printf("engine: health: 40 random patches x 3 s: the output's mean %.2e; a held chord's worst DC %.2e\n", dc, dcw);
    check("health: every state finite through random patches, notes and knobs", finite);
    check("health: no denormal filter state", denorm == 0);
    (void)dcw;                                            /* (slow envelopes and LFOs move through the blockers: */
}                                                         /* printed, not held to a limit) */

/* the DC blockers: TUBE drive (asymmetric, an offset by design) on a held chord: no DC at the output */
static void t_dc_block(void)
{
    uint32_t at;
    double d;
    eng_reset();
    set(SP_A_TABLE, 1);
    set(SP_A_POS, 1000);
    set(SP_DR_ON, 1);
    set(SP_DR_TYPE, DRV_TUBE);
    set(SP_DR_AMT, 1000);
    snap();
    sy_tables_now();
    sy_note_on(48, 100);
    sy_note_on(55, 100);
    sy_note_on(64, 100);
    at = run(blocks(4.0f), 0);
    d = fmax(fabs(dc_est(L + at - DC_N, DC_N)), fabs(dc_est(R + at - DC_N, DC_N)));
    printf("engine: TUBE drive at 100 %% on a held chord: DC at the output %.2e\n", d);
    check("no DC at the output: TUBE drive's offset taken out (held chord, windowed: under 1e-4, -80 dBFS)", d < 1e-4);
}

/* the infrasonic part of a note's life (four one-poles at 20 Hz: its fundamental, 262 Hz, under -80 dB): the
 * thump an offset makes when the envelope opens and closes on it */
static double thump(uint32_t n)
{
    double s[2][4] = {{0}}, a = 1.0 - exp(-2.0 * M_PI * 20.0 / (double)SY_FS), top = 0;
    uint32_t i, c, k;
    for (i = 0; i < n; i++)
        for (c = 0; c < 2u; c++) {
            double x = c ? (double)R[i] : (double)L[i];
            for (k = 0; k < 4u; k++)
                x = s[c][k] += (x - s[c][k]) * a;
            top = fmax(top, fabs(x));
        }
    return top;
}

/* the sources that would carry an offset: a narrow pulse sub, PWM's hold (both taken out at the source), the
 * other warps (not yet: printed) */
static void t_dc_sources(void)
{
    static const struct { const char *name; int16_t kv[8][2]; } C[6] = {
        {"plain saw", {{SP_A_TABLE, 1}, {SP_A_POS, 1000}, {-1, 0}}},
        {"SUB PULSE 5 %", {{SP_A_ON, 0}, {SP_SUB_ON, 1}, {SP_SUB_SHAPE, SUB_PULSE}, {SP_SUB_PW, 1000}, {SP_SUB_OCT, 0}, {-1, 0}}},
        {"PWM 80 % on a pulse", {{SP_A_TABLE, 0}, {SP_A_POS, 1000}, {SP_A_WARP, WARP_PWM}, {SP_A_WAMT, 800}, {-1, 0}}},
        {"ASYM+ 100 % on a saw", {{SP_A_TABLE, 1}, {SP_A_POS, 1000}, {SP_A_WARP, WARP_ASYMP}, {SP_A_WAMT, 1000}, {-1, 0}}},
        {"MIRROR 60 % on a saw", {{SP_A_TABLE, 1}, {SP_A_POS, 1000}, {SP_A_WARP, WARP_MIRROR}, {SP_A_WAMT, 600}, {-1, 0}}},
        {"SYNC 45 % on a saw", {{SP_A_TABLE, 1}, {SP_A_POS, 1000}, {SP_A_WARP, WARP_SYNC}, {SP_A_WAMT, 450}, {-1, 0}}},
    };
    uint32_t c, i, at;
    double t[6];
    for (c = 0; c < 6u; c++) {
        eng_reset();
        for (i = 0; C[c].kv[i][0] >= 0; i++)
            set((uint32_t)C[c].kv[i][0], C[c].kv[i][1]);
        snap();
        sy_tables_now();
        at = run(blocks(0.2f), 0);
        sy_note_on(60, 127);
        at = run(blocks(0.5f), at);
        sy_note_off(60);
        at = run(blocks(0.5f), at);
        t[c] = thump(at);
        printf("engine: offsets: %-22s infrasonic peak %.4f (%.0f dBFS) over a note's attack and release\n", C[c].name,
               t[c], db(t[c]));
    }
    {                                                     /* the sources' own means, over whole cycles */
        static int32_t ilr[2u * SY_BLK];
        static float x[SY_BLK];
        double m, pk;
        uint32_t b, w;
        sy_voice_t *v = &sy_v[0];
        eng_reset();
        m = 0;
        for (b = 0; b < 400u; b++) {                      /* the pulse sub, 5 %: 128 samples a cycle */
            voice_sub(v, 1u << 25, SUB_PULSE, 1.0f, x);
            for (i = 0; i < SY_BLK; i++)
                m += (double)x[i];
        }
        m /= 400.0 * SY_BLK;
        printf("engine: offsets: the pulse sub at 5 %%: mean %.2e over 100 cycles\n", m);
        check("offsets: the pulse sub's mean is taken out (under 1e-4 over whole cycles)", fabs(m) < 1e-4);
        for (w = 0; w < 3u; w++) {                        /* PWM, ASYM+, SYNC alone: 128 samples a cycle */
            static const uint8_t WM[3] = {WARP_PWM, WARP_ASYMP, WARP_SYNC};
            static const uint8_t TB[3] = {0, 1, 1};
            static const float AM[3] = {0.8f, 1.0f, 0.45f};
            osc_par_t p;
            static uint32_t seed = 3;
            osc_v_t *o = &v->osc[0];
            uint32_t id = TB[w];
            wg_begin(id, wt_pool[0]);
            while (!wg_step())
                ;
            wt_tab[0].f = wt_pool[0];
            wt_tab[0].n = wg.n;
            memset(o, 0, sizeof *o);
            osc_note(o, 1, 0.0f, 0.0f, &seed);
            memset(&p, 0, sizeof p);
            p.inc0 = p.inc1 = 1u << 25;
            p.pos = 1.0f;
            p.warp = WM[w];
            p.wamt = AM[w];
            p.nuni = 1;
            p.blend = 1.0f;
            m = 0;
            pk = 0;
            for (b = 0; b < 404u; b++) {
                memset(ilr, 0, sizeof ilr);
                osc_block(o, &wt_tab[0], &p, ilr, UNITY);
                if (b >= 4u)
                    for (i = 0; i < SY_BLK; i++) {
                        m += (double)ilr[2u * i];
                        pk = fmax(pk, fabs((double)ilr[2u * i]));
                    }
            }
            m /= 400.0 * SY_BLK;
            printf("engine: offsets: %-6s %3.0f %% alone: mean %.4f of its peak over 100 cycles\n", WARP_NAMES[WM[w]],
                   (double)(AM[w] * 100.0f), fabs(m) / pk);
            check(w == 0u ? "offsets: PWM's hold is taken out (mean under 1 % of the peak)"
                          : w == 1u ? "offsets: ASYM's offset is taken out (mean under 2 % of the peak)"
                                    : "offsets: SYNC's offset is taken out (mean under 2 % of the peak)",
                  fabs(m) / pk < (w == 0u ? 0.01 : 0.02));
        }
    }
    (void)t;
}

/* ------------------------------------------------------------------------------------- 7 golden --- */
/* the test sounds: firmware/src/sy_sounds.h (the app and the workbench offer them too) */

/* a sound plays a phrase: a chord, a line, a release; returns the frames */
static uint32_t play(const test_sound_t *ts)
{
    static const uint8_t LINE[8] = {48, 55, 60, 63, 67, 72, 70, 67};
    uint32_t i, at = 0;
    eng_reset();
    for (i = 0; ts->kv[i][0] >= 0; i++)
        set((uint32_t)ts->kv[i][0], ts->kv[i][1]);
    snap();
    sy_tables_now();
    sy_note_on(48, 100);
    sy_note_on(55, 90);
    sy_note_on(63, 80);
    at = run(blocks(0.8f), at);
    sy_note_off(48);
    sy_note_off(55);
    sy_note_off(63);
    for (i = 0; i < 8u; i++) {
        sy_note_on(LINE[i], 70 + 7 * i);
        at = run(blocks(0.15f), at);
        sy_note_off(LINE[i]);
        at = run(blocks(0.05f), at);
    }
    sy_bend(12000);
    sy_note_on(60, 100);
    at = run(blocks(0.3f), at);
    sy_note_off(60);
    sy_bend(8192);
    return run(blocks(1.0f), at);
}

static void t_golden(const char *gpath)
{
    uint32_t i;
    int upd = getenv("GOLDEN_UPDATE") != 0, same = 1, match = 1, found = 0;
    FILE *g = upd ? 0 : fopen(gpath, "r"), *w = upd ? fopen(gpath, "w") : 0;
    char line[256];
    for (i = 0; i < 8u; i++) {
        uint32_t n = play(&SOUNDS[i]);
        uint64_t h = hash_out(n), h2;
        double pk = fmax(peak(L, n), peak(R, n)), rm = rms(L, n);
        if (demo) {
            char path[256];
            snprintf(path, sizeof path, "%s/%s.wav", demo, SOUNDS[i].name);
            for (char *c = path + strlen(demo); *c; c++)
                if (*c == ' ')
                    *c = '_';
            wav_write(path, L, R, n);
        }
        if (getenv("GOLDEN_RAW")) {                       /* the render as float (L R interleaved): how far an
                                                           * intended change moves it (tests/sy_golden_diff.py) */
            const char *raw = getenv("GOLDEN_RAW");
            char path[256];
            FILE *rf;
            uint32_t k;
            snprintf(path, sizeof path, "%s/%s.f32", raw, SOUNDS[i].name);
            for (char *c = path + strlen(raw); *c; c++)
                if (*c == ' ')
                    *c = '_';
            if ((rf = fopen(path, "wb")) != 0) {
                for (k = 0; k < n; k++) {
                    fwrite(&L[k], sizeof L[k], 1, rf);
                    fwrite(&R[k], sizeof R[k], 1, rf);
                }
                fclose(rf);
            }
        }
        n = play(&SOUNDS[i]);
        h2 = hash_out(n);
        same &= h == h2;
        printf("engine:   %-12s %016llx  peak %.3f  rms %.3f%s\n", SOUNDS[i].name, (unsigned long long)h, pk, rm,
               h == h2 ? "" : "  <- differs run to run");
        if (w)
            fprintf(w, "%s %016llx\n", SOUNDS[i].name, (unsigned long long)h);
        if (g) {
            int hit = 0;
            rewind(g);
            while (fgets(line, sizeof line, g)) {
                char nm[64];
                unsigned long long gh;
                char *sp = strrchr(line, ' ');
                if (!sp)
                    continue;
                memcpy(nm, line, (size_t)(sp - line));
                nm[sp - line] = 0;
                gh = strtoull(sp + 1, 0, 16);
                if (!strcmp(nm, SOUNDS[i].name)) {
                    hit = 1;
                    found++;
                    if (gh != h) {
                        match = 0;
                        printf("engine:   %-12s differs from tests/sy_golden.txt\n", SOUNDS[i].name);
                    }
                }
            }
            if (!hit)
                match = 0;
        }
        if (pk > 1.0 || !(rm > 1e-4))
            match = 0, printf("engine:   %-12s level out of bounds\n", SOUNDS[i].name);
    }
    if (w)
        fclose(w), printf("engine: golden renders written to %s\n", gpath);
    if (g)
        fclose(g);
    check("determinism: every test sound twice in a run, bit for bit", same);
    check(upd ? "golden renders recorded (GOLDEN_UPDATE)" : "golden renders as recorded in tests/sy_golden.txt",
          upd ? 1 : g && match && found == 8);
}

/* -------------------------------------------------------------------------------------- 8 effects --- */
/* a 1 kHz sine (0.5) through the chorus alone, fully wet, for 1.5 s: its power away from 1 kHz (dB under the
 * total; the line's int16 and the interpolation's own error stay under -60 dB) */
static double chorus_noise(void)
{
    static float out[1u << 16];
    float l[SY_BLK], r[SY_BLK];
    double *p = malloc(((1u << 15) + 1u) * sizeof *p), ph = 0, db;
    uint32_t b, i, n = 0;
    for (b = 0; n < (1u << 16); b++) {
        for (i = 0; i < SY_BLK; i++, ph += 1000.0 / (double)SY_FS)
            l[i] = r[i] = (float)(0.5 * sin(2.0 * M_PI * ph));
        mod_offsets(sy_goff, 0, 0);
        fx_chorus(l, r);
        for (i = 0; i < SY_BLK && b >= 64u && n < (1u << 16); i++)
            out[n++] = l[i];
    }
    power_spectrum(out, 1u << 16, p);
    db = inharmonic_db(p, 1u << 16, SY_FS, 1000.0, 40);
    free(p);
    return db;
}

/* the delay as set, an impulse through it: its echo's distance in ms (the matrix mapped, MACRO 1 a source) */
static double delay_echo_ms(void)
{
    uint32_t at = 10u * SY_BLK, k, b, i, best = 0;
    float bp = 0.0f, l[SY_BLK], r[SY_BLK];
    memset(L, 0, sizeof L);
    for (b = 0; b < blocks(1.0f); b++) {
        for (i = 0; i < SY_BLK; i++)
            l[i] = r[i] = (b == 10u && i == 0u) ? 1.0f : 0.0f;
        sp_block();
        mod_map();
        sy_src_g[SRC_MAC1] = sp_n[SP_MAC1];
        mod_offsets(sy_goff, 0, 0);
        fx_delay(l, r);
        for (i = 0; i < SY_BLK; i++)
            L[b * SY_BLK + i] = l[i];
    }
    for (k = at + 1000u; k < blocks(1.0f) * SY_BLK; k++)
        if (fabsf(L[k]) > bp)
            bp = fabsf(L[k]), best = k;
    return (double)(best - at) * 1000.0 / (double)SY_FS;
}

/* a sine at hz (0.25) through the EQ alone as set: its gain in dB (power out over in, after 0.3 s) */
static double eq_gain_db(double hz)
{
    float l[SY_BLK], r[SY_BLK], x[SY_BLK];
    double ph = 0, pi = 0, po = 0;
    uint32_t b, i;
    memset(fx.eq_s, 0, sizeof fx.eq_s);
    fx.eq_run = 0;
    fx.mix[5] = 0.0f;
    for (b = 0; b < blocks(0.6f); b++) {
        for (i = 0; i < SY_BLK; i++, ph += hz / (double)SY_FS)
            x[i] = l[i] = r[i] = (float)(0.25 * sin(2.0 * M_PI * ph));
        sp_block();
        mod_map();
        mod_offsets(sy_goff, 0, 0);
        fx_eq(l, r);
        if (b >= blocks(0.3f))
            for (i = 0; i < SY_BLK; i++) {
                pi += (double)x[i] * (double)x[i];
                po += (double)l[i] * (double)l[i];
            }
    }
    return 10.0 * log10(po / pi);
}

/* the EQ: flat it is the input bit for bit; each band's gain, measured, against its curve (fx_eq_db: what the screen
 * draws) and against its design; a sweep of every frequency at once, at +18 dB, stays bounded; silence decays to 0 */
static void t_eq(void)
{
    static const struct { uint16_t on; int16_t db; double f, want, tol; } PT[] = {
        {SP_EQ_MID, 120, 1004.7, 12.0, 0.15}, {SP_EQ_MID, 120, 100.0, 0.0, 0.3}, {SP_EQ_MID, -120, 1004.7, -12.0, 0.15},
        {SP_EQ_LOW, 120, 30.0, 12.0, 0.5}, {SP_EQ_LOW, 120, 5000.0, 0.0, 0.3}, {SP_EQ_LOW, 120, 150.3, 6.0, 0.3},
        {SP_EQ_HIGH, -120, 16000.0, -12.0, 0.6}, {SP_EQ_HIGH, -120, 300.0, 0.0, 0.3}, {SP_EQ_HIGH, -120, 3999.7, -6.0, 0.3},
    };
    uint32_t i, b, k, s = 5;
    float l[SY_BLK], r[SY_BLK], x[SY_BLK], top = 0.0f;
    double worst = 0, wcurve = 0;
    int exact = 1, finite = 1, rest = 1;
    eng_reset();
    set(SP_EQ_ON, 1);
    snap();
    for (b = 0; b < blocks(1.0f); b++) {                  /* flat: on, every band at 0 dB */
        for (i = 0; i < SY_BLK; i++)
            x[i] = l[i] = r[i] = 0.5f * sy_randf(sy_rand(&s));
        sp_block();
        mod_map();
        mod_offsets(sy_goff, 0, 0);
        fx_eq(l, r);
        for (i = 0; i < SY_BLK; i++)
            exact &= l[i] == x[i] && r[i] == x[i];
    }
    check("EQ on and flat: the input bit for bit", exact);
    for (k = 0; k < sizeof PT / sizeof PT[0]; k++) {
        uint32_t t = PT[k].on == SP_EQ_LOW ? EQ_LS : PT[k].on == SP_EQ_MID ? EQ_PK : EQ_HS;
        uint32_t hz = PT[k].on == SP_EQ_LOW ? SP_EQ_LFREQ : PT[k].on == SP_EQ_MID ? SP_EQ_MFREQ : SP_EQ_HFREQ;
        double g, c;
        eng_reset();
        set(SP_EQ_ON, 1);
        set(PT[k].on, PT[k].db);
        snap();
        g = eq_gain_db(PT[k].f);
        c = fx_eq_db(t, sp_cut_hz(sp_n[hz]), (float)PT[k].db * 0.1f, sp_q(sp_n[SP_EQ_Q]), (float)PT[k].f);
        printf("engine: EQ %-4s %+5.1f dB at %7.1f Hz: %+6.2f dB (its curve %+6.2f, the design %+5.1f)\n",
               SP[PT[k].on].name, PT[k].db * 0.1, PT[k].f, g, c, PT[k].want);
        worst = fmax(worst, fabs(g - PT[k].want) - PT[k].tol);
        wcurve = fmax(wcurve, fabs(g - c));
    }
    check("EQ: each band's gain as designed (the peak's centre, the shelves' far sides, halfway at FREQ, flat away)",
          worst <= 0.0);
    check("EQ: the curve the screen draws is what plays (within 0.05 dB)", wcurve < 0.05);
    eng_reset();
    set(SP_EQ_ON, 1);
    set(SP_EQ_LOW, 180);
    set(SP_EQ_MID, 180);
    set(SP_EQ_HIGH, 180);
    set(SP_EQ_Q, 1000);
    snap();
    for (b = 0; b < blocks(3.0f); b++) {                  /* every frequency jumping 20 Hz <-> 20 kHz each block */
        int32_t f = (b & 1u) ? 1000 : 0;
        set(SP_EQ_LFREQ, f);
        set(SP_EQ_MFREQ, 1000 - f);
        set(SP_EQ_HFREQ, f);
        snap();
        mod_map();
        for (i = 0; i < SY_BLK; i++)
            l[i] = r[i] = 0.1f * sy_randf(sy_rand(&s));
        mod_offsets(sy_goff, 0, 0);
        fx_eq(l, r);
        for (i = 0; i < SY_BLK; i++) {
            finite &= isfinite(l[i]) && isfinite(r[i]);
            top = fmaxf(top, fmaxf(fabsf(l[i]), fabsf(r[i])));
        }
    }
    set(SP_EQ_LFREQ, SP[SP_EQ_LFREQ].def);                /* then silence, at the default frequencies and Q (a Q */
    set(SP_EQ_MFREQ, SP[SP_EQ_MFREQ].def);                /* of 22 at 20 Hz rings on for minutes, far under */
    set(SP_EQ_HFREQ, SP[SP_EQ_HFREQ].def);                /* hearing and never into denormals: the flush at 1e-15 */
    set(SP_EQ_Q, SP[SP_EQ_Q].def);                        /* a block) */
    snap();
    for (b = 0; b < blocks(10.0f); b++) {
        sy_zerof(l, SY_BLK);
        sy_zerof(r, SY_BLK);
        mod_offsets(sy_goff, 0, 0);
        fx_eq(l, r);
    }
    for (b = 0; b < 3u; b++)
        for (i = 0; i < 2u; i++)
            rest &= fx.eq_s[b][i][0] == 0.0f && fx.eq_s[b][i][1] == 0.0f;
    printf("engine: EQ at +18 dB with every frequency jumping 20 Hz <-> 20 kHz each block: largest output %.2f\n",
           (double)top);
    check("EQ: frequencies jumping each block at +18 dB: finite and bounded (noise at 0.1 under 4)", finite && top < 4.0f);
    check("EQ: after silence its states are exactly 0 (no denormals)", rest);
}

static void t_fx(void)
{
    {                                                     /* the chorus as clean after 10 minutes as at the start */
        float z[2][SY_BLK];                               /* (its read position once came from a count of every */
        uint32_t b;                                       /* sample played: a float of it lost the fraction) */
        double n0, n1;
        eng_reset();
        set(SP_CH_ON, 1);
        set(SP_CH_MIX, 1000);
        set(SP_CH_FDBK, 0);
        snap();
        n0 = chorus_noise();
        for (b = 0; b < (uint32_t)(600.0 * (double)SY_FS) / SY_BLK; b++) {
            sy_zerof(z[0], SY_BLK);
            sy_zerof(z[1], SY_BLK);
            fx_chorus(z[0], z[1]);
        }
        n1 = chorus_noise();
        printf("engine: chorus on a 1 kHz sine, power away from it: %.1f dB at the start, %.1f dB after 10 minutes\n",
               n0, n1);
        check("the chorus as clean after 10 minutes of playing as at the start (under -60 dB, within 3 dB)",
              n0 < -60.0 && n1 < -60.0 && n1 < n0 + 3.0);
    }
    uint32_t i, b, s = 99;
    float l[SY_BLK], r[SY_BLK], x[SY_BLK];
    double err = 0, top = 0;
    int finite = 1;
    eng_reset();
    set(SP_OT_ON, 1);
    set(SP_OT_UP, 0);
    set(SP_OT_DOWN, 0);
    snap();
    run(2, 0);
    for (b = 0; b < 200u; b++) {
        for (i = 0; i < SY_BLK; i++)
            x[i] = l[i] = r[i] = 0.3f * sy_randf(sy_rand(&s));
        mod_offsets(sy_goff, 0, 0);
        fx_ott(l, r);
        if (b > 4u)
            for (i = 0; i < SY_BLK; i++)
                err = fmax(err, fabs((double)l[i] - (double)x[i]));
    }
    printf("engine: OTT flat (no compression): the bands sum back within %.2e\n", err);
    check("OTT: the three bands sum back to the input", err < 1e-5);
    {
        double e1, e2;
        eng_reset();
        set(SP_DL_ON, 1);
        set(SP_DL_SYNC, 1);                               /* FIXED 1/4 at 120 BPM: 0.5 s */
        set(SP_DL_NOTE, 8);
        set(SP_DL_MIX, 1000);
        set(SP_DL_FDBK, 0);
        set(SP_DL_LP, 1000);
        set(SP_DL_HP, 0);
        set(SP_DL_MODE, 0);
        snap();
        e1 = delay_echo_ms();
        set(SP_M1_SRC, SRC_MAC1);                         /* TIME's modulation, -25 %: 4 values shorter, 1/8T */
        set(SP_M1_DST, SP_DL_TIME);
        set(SP_M1_AMT, -1000);
        set(SP_MAC1, 250);
        snap();
        e2 = delay_echo_ms();
        printf("engine: delay FIXED 1/4 at 120 BPM: the echo %.2f ms after the impulse; TIME - 25 %%: %.2f ms\n", e1, e2);
        check("delay synced: the echo at 500 ms (+-1 ms)", fabs(e1 - 500.0) < 1.0);
        check("delay FIXED: TIME's modulation steps the note value (1/4 - 25 %: 1/8T, 166.7 ms +-1 ms)",
              fabs(e2 - 500.0 / 3.0) < 1.0);
    }
    eng_reset();
    set(SP_DR_ON, 1);
    set(SP_DR_AMT, 1000);
    set(SP_CH_ON, 1);
    set(SP_CH_FDBK, 1000);
    set(SP_DL_ON, 1);
    set(SP_DL_FDBK, 1000);
    set(SP_RV_ON, 1);
    set(SP_RV_DECAY, 1000);
    set(SP_RV_SIZE, 1000);
    set(SP_OT_ON, 1);
    set(SP_EQ_ON, 1);
    set(SP_EQ_LOW, 180);
    set(SP_EQ_MID, 180);
    set(SP_EQ_HIGH, 180);
    snap();
    for (b = 0; b < blocks(10.0f); b++) {
        for (i = 0; i < SY_BLK; i++)
            l[i] = r[i] = b < blocks(5.0f) ? sy_randf(sy_rand(&s)) : 0.0f;
        mod_offsets(sy_goff, 0, 0);
        sp_block();
        fx_block(l, r);
        for (i = 0; i < SY_BLK; i++) {
            if (!isfinite(l[i]) || !isfinite(r[i]))
                finite = 0;
            top = fmax(top, fmax(fabs((double)l[i]), fabs((double)r[i])));
        }
    }
    printf("engine: every effect at its extremes on full-scale noise: largest output %.3f\n", top);
    check("effects at their extremes: finite, and the limiter holds the peaks under 1", finite && top < 1.0);
}

/* ----------------------------------------------------------------------------------------- 9 cost --- */
static void t_cost(void)
{
    static const char *const NAMES[3] = {"1 voice, OSC A, no unison, no filter, no effects",
                                         "8 voices, A + B x 3 unison, LP24, every effect",
                                         "8 voices, A + B x 16 unison (budget 48), LADDER, every effect"};
    uint32_t c, i, b;
    for (c = 0; c < 3u; c++) {
        uint64_t t0;
        double us;
        eng_reset();
        if (c) {
            set(SP_B_ON, 1);
            set(SP_A_UNI, c == 1u ? 3 : 16);
            set(SP_B_UNI, c == 1u ? 3 : 16);
            set(SP_F_ON, 1);
            set(SP_F_TYPE, c == 1u ? FT_LP24 : FT_LADDER);
            set(SP_DR_ON, 1);
            set(SP_CH_ON, 1);
            set(SP_DL_ON, 1);
            set(SP_RV_ON, 1);
            set(SP_OT_ON, 1);
            set(SP_EQ_ON, 1);
            set(SP_EQ_LOW, 30);
            set(SP_EQ_MID, -30);
            set(SP_EQ_HIGH, 30);
            snap();
            for (i = 0; i < 8u; i++)
                sy_note_on(48 + 4 * i, 100);
        } else {
            sy_note_on(60, 100);
        }
        run(4, 0);
        t0 = t_now_ns();
        for (b = 0; b < 4000u; b++) {
            int32_t out[2u * SY_BLK];
            sy_block(out);
        }
        us = (double)(t_now_ns() - t0) / 1000.0 / 1000.0;     /* us a half (4 blocks) */
        printf("engine: host cost, %-62s %7.1f us a half (%.1f %% of real time)\n", NAMES[c], us,
               us / (128.0 / (double)SY_FS * 1e6) * 100.0);
    }
}

int main(int argc, char **argv)
{
    t_name = "engine";
    demo = argc > 2 ? argv[2] : 0;
    t_params();
    t_matrix();
    t_voices();
    t_env_lfo();
    t_clicks();
    t_health();
    t_dc_block();
    t_dc_sources();
    t_golden(argc > 1 ? argv[1] : "tests/sy_golden.txt");
    t_fx();
    t_eq();
    t_cost();
    return t_done();
}
