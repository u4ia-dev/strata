/* SPDX-License-Identifier: GPL-3.0-only */
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
 *    changed, bend jumps): the largest step between samples near the event against the steady sine's (<= 2x); the
 *    hush for a SAVE's flash write: out and back without a click, hushed only once the last two halves are silent;
 * 6. health: 40 random patches, notes and knob changes for 3 s each: finite, no denormal filter state (a held
 *    chord's windowed DC printed: slow envelopes and LFOs pass the blockers as slow movement); TUBE drive on a held
 *    chord: no DC at the output (-80 dBFS); offsets: the pulse sub's mean, PWM's, ASYM's and SYNC's over whole
 *    cycles taken out, and the infrasonic thump of a note's attack and release printed for each;
 * 7. determinism and golden renders: eight test sounds play a phrase; the same twice in a run, and as recorded
 *    in tests/sy_golden.txt (a change must be intended: GOLDEN_UPDATE=1); WAVs into DEMODIR;
 * 8. effects: OTT's bands sum back to the input; the synced delay's echo at 1/4 of 120 BPM, TIME's modulation
 *    stepping its note value; every module bounded and finite on full-scale noise; the limiter holds peaks under 1;
 *    the EQ: flat it is its input bit for bit, each band's gain measured against its design and against the curve the
 *    screen draws, bounded with every frequency jumping, its states 0 after silence; FX ORDER: every code an order,
 *    the order set the order played, a new one mid-note without a click;
 * 9. cost: host time of a half (128 frames) for a light and two heavy cases, against real time. */
#define SY_TICKS() 0u              /* (the admission's clock: none here, so nothing is measured; a block's ticks as the */
#define SY_TICKS_BLK 17408u        /* FM-1's, for the admission's test, which sets what it would have measured) */
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
static void set(uint32_t id, int32_t v)
{
    int32_t fi = sp_inst_of(id);
    if (id >= SP_COUNT)
        return;
    sy_patch.p[id] = sp_clamp(id, v);
    if (id == SP_FX_ORDER || (fi >= 0 && fi < (int32_t)SP_NFX && id == FXI_ON[fi]))
        sp_chain_from_old(&sy_patch);                 /* (the six's ON and FX ORDER: the chain as before it) */
}
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
/* a patch as a load leaves it: its chain whole, the effects out of the chain and the unused at their defaults */
static void canon(sy_patch_t *a)
{
    uint32_t i;
    sp_chain_fix(a->p);
    for (i = 0; i < SP_COUNT; i++)
        if ((SP[i].flags & F_OLD) || (sp_inst_of(i) >= 0 && sp_chain_find(a->p, (uint32_t)sp_inst_of(i)) < 0))
            a->p[i] = SP[i].def;
}
static void t_params(void)
{
    uint32_t i, j, tail;
    int ok = 1, lists = 1, ccs = 1;
    static uint8_t buf[SP_PACK_MAX + 64u];
    int same_v1;
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
        uint32_t s = 7, nc = 0, core[SP_COUNT];
        int same = 1;
        for (i = 0; i < SP_COUNT; i++)
            if (sp_core(i))
                core[nc++] = i;
        sp_defaults(&a);
        for (i = 0; i < SP_COUNT; i++)
            a.p[i] = sp_clamp(i, SP[i].lo + (int32_t)(sy_rand(&s) % (uint32_t)(SP[i].hi - SP[i].lo + 1)));
        for (i = 0; i < NFXC; i++)                        /* a chain of five: DRIVE 2, EQ, FILTER, TAPE 2, DELAY */
            a.p[SP_FXC1 + i] = 0;
        a.p[SP_FXC1] = FXI_DRIVE2 + 1;
        a.p[SP_FXC2] = FXI_EQ + 1;
        a.p[SP_FXC3] = FXI_FFILT + 1;
        a.p[SP_FXC4] = FXI_TAPE2 + 1;
        a.p[SP_FXC5] = FXI_DELAY + 1;
        canon(&a);
        memcpy(a.name, "TEST PATCH 12345", 17);
        a.cat = CAT_PAD;
        sp_shape_pts(LFO_STAIRS, &a.draw[0]);             /* drawn shapes: 16 points; 5, moved and curved */
        a.draw[1].y[1] = 640;
        a.draw[1].c[2] = -777;
        a.draw[1].x[3] = 801;
        j = sp_pack(&a, buf);
        same = j <= SP_PACK_MAX && sp_unpack(&b, buf, j) && !memcmp(a.p, b.p, sizeof a.p) &&
               !memcmp(a.name, b.name, 16) && a.cat == b.cat && draw_same(&a.draw[0], &b.draw[0]) &&
               draw_same(&a.draw[1], &b.draw[1]);
        printf("engine: a patch of %u core values and a chain of 5 packs into %u bytes\n", (unsigned)nc, (unsigned)j);
        check("a patch packed and unpacked is the same (its chain, its modules' values, its drawn shapes)", same &&
              nc == SP_NCORE);
        tail = j - 6u - 2u * nc;                          /* the name, category, macros, drawn shapes, the chain */
        buf[4] = (uint8_t)(nc - 10u);                     /* an older build: 10 core values fewer */
        memmove(buf + 6u + 2u * (nc - 10u), buf + 6u + 2u * nc, tail);
        same = sp_unpack(&b, buf, j - 20u);
        for (i = nc - 10u; i < nc; i++)                   /* (the chain's slots: the chain's own part sets them) */
            same &= b.p[core[i]] == (core[i] >= SP_FXC1 && core[i] <= SP_FXC8 ? a.p[core[i]] : SP[core[i]].def);
        for (i = 0; i < nc - 10u; i++)
            same &= b.p[core[i]] == a.p[core[i]];
        same &= draw_same(&a.draw[1], &b.draw[1]);
        check("a patch with fewer values: the rest take their defaults", same);
        sp_defaults(&def);
        j = sp_pack(&a, buf);
        same = sp_unpack(&b, buf, 6u + 2u * nc + 49u) && draw_same(&b.draw[0], &def.draw[0]) &&
               draw_same(&b.draw[1], &def.draw[1]);
        for (i = 0; i < nc; i++)
            same &= b.p[core[i]] == a.p[core[i]];
        check("a patch without drawn shapes (an older build): the default's", same);
        buf[6u + 2u * nc + 50u] = SP_NPT + 1u;            /* LFO 1 with 17 points: neither is read */
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
    {                                                     /* the largest patch: eight modules of the most values, */
        uint32_t k, n;                                    /* both LFOs drawn with 16 points */
        static const uint8_t BIG[NFXC] = {FXI_EQ, FXI_EQ2, FXI_PHASER, FXI_PHASER2, FXI_DELAY, FXI_REVERB,
                                          FXI_FLANGER, FXI_FLANGER2};
        sp_defaults(&a);
        for (k = 0; k < NFXC; k++)
            a.p[SP_FXC1 + k] = (int16_t)(BIG[k] + 1u);
        sp_shape_pts(LFO_STAIRS, &a.draw[0]);
        sp_shape_pts(LFO_STAIRS, &a.draw[1]);
        n = sp_pack(&a, buf);
        printf("engine: the largest patch packs into %u bytes (at most %u; a user slot holds 766)\n", (unsigned)n,
               (unsigned)SP_PACK_MAX);
        check("the largest patch (eight modules of ten values, two 16-point shapes) fits SP_PACK_MAX and a user slot",
              n <= SP_PACK_MAX && SP_PACK_MAX <= 766u && sp_unpack(&b, buf, n) && sp_chain_n(b.p) == NFXC);
    }
    {                                                     /* a preset from before the chain (version 1: every value */
        static uint8_t v1[6u + 2u * 181u + 49u + 1u + 2u];   /* of the 181 then, in id order) */
        uint32_t n = 0, k;
        sp_defaults(&a);
        a.p[SP_CH_ON] = 1;
        a.p[SP_CH_RATE] = 123;
        a.p[SP_RV_ON] = 1;
        a.p[SP_RV_DECAY] = 777;
        a.p[SP_DL_ON] = 0;
        a.p[SP_DL_MIX] = 999;
        a.p[SP_FX_ORDER] = 595;                           /* OTT EQ REVERB DRIVE DELAY CHORUS */
        a.p[SP_M1_SRC] = SRC_LFO1;
        a.p[SP_M1_DST] = SP_RV_MIX;
        a.p[SP_M1_AMT] = 500;
        v1[n++] = 'S', v1[n++] = 'Y', v1[n++] = 1, v1[n++] = 0, v1[n++] = 181, v1[n++] = 0;
        for (k = 0; k < 181u; k++)
            v1[n++] = (uint8_t)a.p[k], v1[n++] = (uint8_t)((uint16_t)a.p[k] >> 8);
        memcpy(v1 + n, "OLD ONE\0\0\0\0\0\0\0\0\0", 16);
        n += 16u;
        v1[n++] = CAT_KEYS;
        for (k = 0; k < 32u; k++)
            v1[n++] = (uint8_t)a.macro[k / 8u][k % 8u];
        same_v1 = sp_unpack(&b, v1, n) && sp_chain_n(b.p) == 2u && b.p[SP_FXC1] == FXI_REVERB + 1 &&
                  b.p[SP_FXC2] == FXI_CHORUS + 1 && b.p[SP_CH_RATE] == 123 && b.p[SP_RV_DECAY] == 777 &&
                  b.p[SP_M1_DST] == SP_RV_MIX && b.cat == CAT_KEYS;
        check("a preset from before the chain: its effects that were ON the chain, in FX ORDER's order, values kept",
              same_v1);
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
    {                                                     /* every smoothed value glides to a change (V0.6 kept the */
        uint32_t i, nsm = 0, hi = 0, bad = 0, first = SP_COUNT;   /* moving ones' ids in bytes: from 256 up, stuck) */
        eng_reset();
        snap();
        for (i = 0; i < SP_COUNT; i++)                    /* (written as the knobs and MIDI write: no patch load) */
            if (SP[i].flags & F_SMOOTH) {
                sy_patch.p[i] = (int16_t)(sy_patch.p[i] == SP[i].hi ? SP[i].lo : SP[i].hi);
                nsm++;
                hi += i >= 256u;
            }
        run(blocks(0.5f), 0);
        for (i = 0; i < SP_COUNT; i++)
            if ((SP[i].flags & F_SMOOTH) && sp_n[i] != sp_norm_of(i, sy_patch.p[i]))
                bad++, first = first < i ? first : i;
        if (bad)
            printf("    %u smoothed values short of a change, the first %s %s\n", (unsigned)bad,
                   SEC_NAMES[SP[first].sec], SP[first].name);
        check("every smoothed value (166, the 34 from id 256 up: FX FILTER, BITCRUSH, TREMOLO, RING MOD, TAPE, both) "
              "glides to a change while a sound plays, without a patch load", nsm >= 160u && hi >= 30u && !bad);
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
    {                                                     /* V0.87: POLARITY, a bit a slot (the source turned) */
        float o0[3], o1[3];
        uint32_t k;
        static const float LV[3] = {-1.0f, 0.0f, 1.0f}, EV[3] = {0.0f, 0.5f, 1.0f};
        eng_reset();
        set(SP_M1_SRC, SRC_LFO1);
        set(SP_M1_DST, SP_F_CUT);
        set(SP_M1_AMT, 400);
        set(SP_M2_SRC, SRC_ENV2);
        set(SP_M2_DST, SP_F_RES);
        set(SP_M2_AMT, 400);
        snap();
        for (k = 0; k < 3u; k++) {                        /* (as they are, then both turned) */
            vs[SRC_LFO1] = LV[k], vs[SRC_ENV2] = EV[k];
            set(SP_M_POL, 0);
            mod_map();
            mod_offsets(off, vs, 0);
            o0[k] = off[mod_idx[SP_F_CUT]] + 10.0f * off[mod_idx[SP_F_RES]];
            set(SP_M_POL, 3);
            mod_map();
            mod_offsets(off, vs, 0);
            o1[k] = off[mod_idx[SP_F_CUT]];
            o1[k] += 10.0f * off[mod_idx[SP_F_RES]];
        }
        check("matrix POLARITY: 0 every source as it is (LFO -1 .. 1 x 40 %: -0.4 .. 0.4; ENV 0 .. 1: 0 .. 0.4)",
              fabsf(o0[0] - (-0.4f)) < 1e-6f && fabsf(o0[2] - (0.4f + 4.0f)) < 1e-5f);
        check("matrix POLARITY turned: the LFO 0 .. 1 (never under the knob: 0 .. 0.4), the ENV -1 .. 1 (-0.4 .. 0.4)",
              fabsf(o1[0] - (0.0f - 4.0f)) < 1e-5f && fabsf(o1[1] - (0.2f + 0.0f)) < 1e-5f &&
              fabsf(o1[2] - (0.4f + 4.0f)) < 1e-5f);
    }
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
    for (i = 0; i < 20u; i++)                             /* (V0.87.1: the render at 80 %, the whole time too: */
        sy_cpu(205, 205);                                 /* V0.87's guard stepped at 70 % of the render) */
    check("CPU guard (V0.87.1): 80 % of the half, the render and its whole time alike: nothing", sy_uni_budget == 48u &&
          !fx_lo && nvoices() == 4u);
    k = (uint32_t)sy_cpu(200, 250) + (uint32_t)sy_cpu(200, 250);
    run(1, 0);
    ok = 0;
    for (i = 0; i < SY_NVOICE; i++)
        ok |= sy_v[i].steal != 0;
    check("CPU guard: two halves over 92 % of their whole time (the render under it) shed a voice (faded)",
          k == 1u && (ok || nvoices() < 4u));
    for (i = 0; i < 20u; i++)
        sy_cpu(190, 222);
    check("CPU guard: over 85 % of the whole time the unison budget steps down to 16", sy_uni_budget == 16u);
    for (i = 0; i < 20u; i++)
        sy_cpu(190, 230);
    check("CPU guard: over 88 % the effects get cheaper", fx_lo == 1);
    for (i = 0; i < 345u * 6u; i++)
        sy_cpu(100, 185);
    check("CPU guard: under 75 % for a second, one step back at a time, all the way", sy_uni_budget == 48u && !fx_lo);
    {                                                     /* near the deadline: at once */
        uint32_t b0, b1, sh;
        int f1;
        eng_reset();
        for (i = 0; i < 4u; i++) {
            sy_note_on(60 + i, 100);
            run(1, 0);
        }
        sh = (uint32_t)sy_cpu_now(250);                   /* (V0.87.1: 98 % in the half its voices started: */
        f1 = sy_g.shed == 0u && !sh;                      /* a start's peak, nothing shed at once) */
        eng_reset();
        for (i = 0; i < 4u; i++) {
            sy_note_on(60 + i, 100);
            run(1, 0);
        }
        sy_adm_half(0);                                   /* (a half later: none started since) */
        check("CPU guard (V0.87.1): 98 % in the half that started its voices sheds none at once (a start's peak)", f1);
        b0 = sy_uni_budget;
        k = (uint32_t)sy_cpu_now(225);                    /* (88 %: nothing) */
        b1 = sy_uni_budget;
        k += (uint32_t)sy_cpu_now(242);                   /* (95 %: a step, the effects cheaper) */
        f1 = fx_lo;
        sh = (uint32_t)sy_cpu_now(250);                   /* (98 %: a voice shed now) */
        check("CPU guard: a half near its deadline (the nested interrupts in): over 90 % a step at once, over 94 % the "
              "effects cheaper, over 96 % a voice shed at once",
              b0 == 48u && b1 == 48u && !k && f1 == 1 && sy_uni_budget < 40u && sh == 1u);
    }
    {                                                     /* admission (V0.66): a voice only where its time fits */
        uint32_t on1, on2, ref;
        eng_reset();
        set(SP_V_POLY, 8);
        snap();
        sy_adm.vt = SY_TICKS_BLK / 8u;                    /* (as measured: a voice 1/8 of a block, the rest 1/4) */
        sy_adm.bt = SY_TICKS_BLK / 4u;
        for (i = 0; i < 8u; i++)
            sy_note_on(48 + 3 * i, 100);
        on1 = nvoices();
        ref = sy_adm.refused;
        eng_reset();                                      /* (not measured yet: a new sound) */
        set(SP_V_POLY, 8);
        snap();
        for (i = 0; i < 8u; i++)
            sy_note_on(48 + 3 * i, 100);
        on2 = nvoices();
        printf("engine: admission: 8 notes, a voice 1/8 of a block: %u voices, %u refused a free one (stealing); "
               "the cost not measured yet: %u voices\n", (unsigned)on1, (unsigned)ref, (unsigned)on2);
        check("admission: a chord's voices only as many as the block's time holds under 84 % (1/4 + 4 x 1/8), the "
              "others steal; with the cost not measured yet (a new sound), every one", on1 == 4u && ref == 4u && on2 == 8u);
    }
    {                                                     /* V0.87.1: the unison gives way before a note steals */
        uint32_t on1, ref, tr, b1;
        eng_reset();
        set(SP_V_POLY, 8);
        set(SP_A_UNI, 7);
        snap();
        sy_adm.vt = SY_TICKS_BLK / 100u * 12u;            /* (a voice of 7 unison voices 12 % of a block: its own 5 %, */
        sy_adm.bt = SY_TICKS_BLK / 4u;                    /* each unison voice 1 %; the rest 25 %) */
        ref = sy_adm.refused;
        tr = sy_adm.traded;
        for (i = 0; i < 8u; i++)
            sy_note_on(48 + 3 * i, 100);
        on1 = nvoices();
        b1 = sy_uni_budget;
        run(1, 0);
        printf("engine: admission: 8 notes of 7 unison voices (a voice 12 %% of a block, 7 of it its unison): %u voices, "
               "%u refused, %u took unison from the others, the budget %u (%u instances after a block)\n", (unsigned)on1,
               (unsigned)(sy_adm.refused - ref), (unsigned)(sy_adm.traded - tr), (unsigned)b1, (unsigned)sy_instances());
        check("admission (V0.87.1): a chord of 7-unison voices that the time does not hold whole: every note a voice, the "
              "unison budget stepped down for them (to 16: 2 each), none refused",
              on1 == 8u && sy_adm.refused == ref && sy_adm.traded > tr && b1 == 16u && sy_instances() <= 16u);
        eng_reset();
        set(SP_V_POLY, 8);
        set(SP_A_UNI, 7);
        snap();
        sy_adm.vt = SY_TICKS_BLK / 100u * 12u;
        sy_adm.bt = SY_TICKS_BLK / 2u;                    /* (the rest half a block: even the floor does not hold 8) */
        ref = sy_adm.refused;
        for (i = 0; i < 8u; i++)
            sy_note_on(48 + 3 * i, 100);
        check("admission (V0.87.1): no time even with the budget at its floor (a trade held under 80 %), a note steals as "
              "before", nvoices() < 8u && sy_adm.refused > ref);
    }
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

/* the hush for the flash's writes (sy_app.c): out and back in without a click; hushed only once the last two
 * halves (256 samples: the ring the DMA loops while an erase holds the interrupts off) are silent; the voices play
 * on under it */
static void t_hush(void)
{
    uint32_t at, e0, e1, k, i, loud = 0, w5 = (uint32_t)(0.05 * (double)SY_FS), w15 = (uint32_t)(0.15 * (double)SY_FS);
    double r0, r1;
    click_patch();
    sy_note_on(57, 100);
    at = e0 = run(blocks(0.3f), 0);
    sy_hush_req = 1;
    for (k = 0; k < 64u && !sy_hushed; k++) {
        at = run(1, at);
        if (sy_hushed)
            for (i = at - 256u; i < at; i++)
                loud += L[i] != 0.0f || R[i] != 0.0f;
    }
    at = run(blocks(0.1f), at);
    sy_hush_req = 0;
    e1 = at;
    at = run(blocks(0.3f), at);
    r0 = click_ratio(e0 - 64u, e0 + w5, e0 - 64u, e0 + w15, e0 + w15);
    r1 = click_ratio(e1 - 64u, e1 + w5, e1 - 64u, e1 + w15, at);
    printf("engine: the hush: hushed after %u blocks (the last 256 samples: %u not silent); out %.2f x, back %.2f x the "
           "steady sine's step\n", k, loud, r0, r1);
    check("the hush (a SAVE's flash write): out and back without a click (<= 2x), hushed only with the ring silent",
          k <= 2u * SY_HUSH_BLKS && loud == 0u && r0 <= 2.0 && r1 <= 2.0 && nvoices() == 1u);
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
            if (!isfinite(fx.dly_lp[0]) || !isfinite(fx.rv_lp[0]) || !isfinite(fx.ott[0].env[0]) || !isfinite(fx.lim_g))
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
        fx_chorus(l, r, FXI_CHORUS);
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
        fx_delay(l, r, FXI_DELAY);
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
    memset(fx.eq[0].s, 0, sizeof fx.eq[0].s);
    fx.eq[0].run = 0;
    fx.mix[5] = 0.0f;
    for (b = 0; b < blocks(0.6f); b++) {
        for (i = 0; i < SY_BLK; i++, ph += hz / (double)SY_FS)
            x[i] = l[i] = r[i] = (float)(0.25 * sin(2.0 * M_PI * ph));
        sp_block();
        mod_map();
        mod_offsets(sy_goff, 0, 0);
        fx_eq(l, r, FXI_EQ);
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
        fx_eq(l, r, FXI_EQ);
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
        c = fx_eq_db(t, sp_cut_hz(sp_n[hz]), (float)PT[k].db * 0.1f,
                     sp_q(sp_n[t == EQ_LS ? SP_EQ_LQ : t == EQ_PK ? SP_EQ_Q : SP_EQ_HQ]), (float)PT[k].f);
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
        fx_eq(l, r, FXI_EQ);
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
        fx_eq(l, r, FXI_EQ);
    }
    for (b = 0; b < 3u; b++)
        for (i = 0; i < 2u; i++)
            rest &= fx.eq[0].s[b][i][0] == 0.0f && fx.eq[0].s[b][i][1] == 0.0f;
    printf("engine: EQ at +18 dB with every frequency jumping 20 Hz <-> 20 kHz each block: largest output %.2f\n",
           (double)top);
    check("EQ: frequencies jumping each block at +18 dB: finite and bounded (noise at 0.1 under 4)", finite && top < 4.0f);
    check("EQ: after silence its states are exactly 0 (no denormals)", rest);
}

/* the effects' order: every code an order and back; a set order is the order the modules play in (against calling
 * them so); a new order while a sine plays through DRIVE and OTT takes no click (the largest step between samples
 * near it against the steady output's) */
static void t_fx_order(void)
{
    static void (*const RUN[SP_NFX])(float *, float *, uint32_t) = {fx_drive, fx_chorus, fx_delay, fx_reverb, fx_ott,
                                                                    fx_eq};
    static const uint8_t ON[SP_NFX] = {SP_DR_ON, SP_CH_ON, SP_DL_ON, SP_RV_ON, SP_OT_ON, SP_EQ_ON};
    static float a[2][4096], bo[2][4096];
    uint8_t o[SP_NFX];
    uint32_t c, i, k, b, s;
    int all = 1, same = 1;
    double ph = 0, st = 0, st2 = 0, near = 0;
    for (c = 0; c < 720u; c++) {
        uint32_t seen = 0;
        sp_order((int32_t)c, o);
        for (i = 0; i < SP_NFX; i++)
            seen |= 1u << o[i];
        all &= seen == 63u && sp_order_code(o) == (int16_t)c;
    }
    check("FX ORDER: each of the 720 codes is an order of the six, and comes back the same", all);
    sp_order(595, o);                                   /* OTT, EQ, REVERB, DRIVE, DELAY, CHORUS */
    for (k = 0; k < 2u; k++) {                          /* fx_block in that order, and the modules called so */
        eng_reset();
        for (i = 0; i < SP_NFX; i++)
            set(ON[i], 1);
        set(SP_EQ_MID, 60);
        set(SP_FX_ORDER, 595);
        snap();
        mod_map();                                      /* (the chain from none: taken at once) */
        for (s = 3, b = 0; b < 4096u / SY_BLK; b++) {
            float l[SY_BLK], r[SY_BLK];
            for (i = 0; i < SY_BLK; i++)
                l[i] = r[i] = 0.4f * sy_randf(sy_rand(&s));
            mod_offsets(sy_goff, 0, 0);
            if (!k) {
                fx_block(l, r);
            } else {
                for (i = 0; i < SP_NFX; i++)
                    RUN[o[i]](l, r, o[i]);
                fx_limit(l, r);
            }
            for (i = 0; i < SY_BLK; i++) {
                (k ? bo : a)[0][b * SY_BLK + i] = l[i];
                (k ? bo : a)[1][b * SY_BLK + i] = r[i];
            }
        }
    }
    for (i = 0; i < 4096u; i++)
        same &= a[0][i] == bo[0][i] && a[1][i] == bo[1][i];
    check("FX ORDER: the modules play in the patch's order (OTT EQ REVERB DRIVE DELAY CHORUS: as called so, bit for bit)",
          same);
    eng_reset();                                        /* a new order mid-note: a dip, no click */
    set(SP_DR_ON, 1);
    set(SP_DR_AMT, 600);
    set(SP_OT_ON, 1);
    snap();
    mod_map();
    for (b = 0; b < blocks(1.0f); b++) {
        float l[SY_BLK], r[SY_BLK], prev = 0.0f;
        if (b == blocks(0.6f))
            set(SP_FX_ORDER, 595);
        for (i = 0; i < SY_BLK; i++, ph += 220.0 / (double)SY_FS)
            l[i] = r[i] = (float)(0.3 * sin(2.0 * M_PI * ph));
        sp_block();
        mod_offsets(sy_goff, 0, 0);
        fx_block(l, r);
        for (i = 0; i < SY_BLK; i++) {
            double d = fabs((double)l[i] - (double)(i ? l[i - 1u] : prev));
            if (b > blocks(0.3f) && b < blocks(0.6f) - 1u)
                st = fmax(st, d);
            else if (b >= blocks(0.6f) - 1u && b < blocks(0.6f) + 8u)
                near = fmax(near, d);
            else if (b > blocks(0.8f))
                st2 = fmax(st2, d);
        }
        prev = l[SY_BLK - 1u];
    }
    printf("engine: FX ORDER changed under a sine through DRIVE and OTT: largest step %.4f near it; steady %.4f before, "
           "%.4f after\n", near, st, st2);
    check("FX ORDER: a new order mid-note dips the wet parts, no click (the largest step under 2x the steady output's)",
          near < 2.0 * fmax(st, st2) && fx.n == 2u && fx.ch[0] == FXI_OTT && fx.ch[1] == FXI_DRIVE);
}

static void stack_poison(void)                            /* the stack below its caller full of NaN: an unset */
{                                                         /* local there reads one */
    volatile float junk[8192];
    uint32_t i;
    for (i = 0; i < 8192u; i++)
        junk[i] = NAN;
    (void)junk[8191];
}

static void t_fx(void)
{
    {                                                     /* the reverb on 4 lines (PLATE, or HALL under the CPU */
        float l[SY_BLK], r[SY_BLK];                       /* guard) reads its 4 only (it once added a 5th, never */
        uint32_t b2, j;                                   /* set: the stack's leftovers, a NaN at worst) */
        int quiet = 1;
        eng_reset();
        set(SP_RV_ON, 1);
        set(SP_RV_TYPE, 0);
        set(SP_RV_MIX, 1000);
        snap();
        mod_map();
        for (b2 = 0; b2 < 200u; b2++) {
            sy_zerof(l, SY_BLK);
            sy_zerof(r, SY_BLK);
            mod_offsets(sy_goff, 0, 0);
            stack_poison();
            fx_reverb(l, r, FXI_REVERB);
            for (j = 0; j < SY_BLK; j++)
                quiet &= l[j] == 0.0f && r[j] == 0.0f;
        }
        check("REVERB on 4 lines (PLATE): silence in, silence out with the stack full of NaN (no unset line read)",
              quiet);
    }
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
            fx_chorus(z[0], z[1], FXI_CHORUS);
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
        fx_ott(l, r, FXI_OTT);
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

/* ------------------------------------------------------------------------------------- the chain --- */
/* the chain set to these instances (each ON), the rest of the slots empty */
static void chain(const uint8_t *ins, uint32_t n)
{
    uint32_t k;
    for (k = 0; k < NFXC; k++)
        sy_patch.p[SP_FXC1 + k] = (int16_t)(k < n ? ins[k] + 1u : 0u);
    for (k = 0; k < n; k++)
        sy_patch.p[FXI_ON[ins[k]]] = 1;
}

/* a block of the chain on l / r as the engine runs it (the values followed, the matrix's offsets) */
static void fx_run(float *l, float *r)
{
    sp_block();
    mod_map();
    mod_offsets(sy_goff, 0, 0);
    fx_block(l, r);
}

/* a sine at hz (0.05: +18 dB stays under the limiter) through the chain as set: its gain in dB (power out over in,
 * after 0.4 s); the left */
static double chain_gain_db(double hz)
{
    float l[SY_BLK], r[SY_BLK], x[SY_BLK];
    double ph = 0, pi = 0, po = 0;
    uint32_t b, i;
    for (b = 0; b < blocks(0.8f); b++) {
        for (i = 0; i < SY_BLK; i++, ph += hz / (double)SY_FS)
            x[i] = l[i] = r[i] = (float)(0.05 * sin(2.0 * M_PI * ph));
        fx_run(l, r);
        if (b >= blocks(0.4f))
            for (i = 0; i < SY_BLK; i++) {
                pi += (double)x[i] * (double)x[i];
                po += (double)l[i] * (double)l[i];
            }
    }
    return 10.0 * log10(po / pi + 1e-30);
}

/* a 220 Hz sine through the chain for 1 s, the chain changed by `change` at 0.6 s: the largest step between samples
 * near the change against the steady output's (before and after); the deepest dip asked for (fx.dip) */
static double chain_step(void (*change)(void), double *steady, uint32_t *dip)
{
    double ph = 0, st = 0, st2 = 0, near = 0;
    float prev = 0.0f;
    uint32_t b, i;
    *dip = 0;
    for (b = 0; b < blocks(1.0f); b++) {
        float l[SY_BLK], r[SY_BLK];
        if (b == blocks(0.6f))
            change();
        for (i = 0; i < SY_BLK; i++, ph += 220.0 / (double)SY_FS)
            l[i] = r[i] = (float)(0.3 * sin(2.0 * M_PI * ph));
        fx_run(l, r);
        if (fx.dip > *dip)
            *dip = fx.dip;
        for (i = 0; i < SY_BLK; i++) {
            double d = fabs((double)l[i] - (double)(i ? l[i - 1u] : prev));
            if (b > blocks(0.3f) && b < blocks(0.6f) - 1u)
                st = fmax(st, d);
            else if (b >= blocks(0.6f) - 1u && b < blocks(0.6f) + 24u)   /* (the dip: 16 blocks) */
                near = fmax(near, d);
            else if (b > blocks(0.8f))
                st2 = fmax(st2, d);
        }
        prev = l[SY_BLK - 1u];
    }
    *steady = fmax(st, st2);
    return near;
}
static void ch_add_eq(void)                               /* DRIVE -> DRIVE, EQ (MID +6 dB) */
{
    static const uint8_t C[2] = {FXI_DRIVE, FXI_EQ};
    sy_patch.p[SP_EQ_MID] = 60;
    chain(C, 2u);
}
static void ch_take_ott(void)                             /* DRIVE, OTT -> DRIVE */
{
    static const uint8_t C[1] = {FXI_DRIVE};
    chain(C, 1u);
}
static void ch_swap(void)                                 /* DRIVE, OTT -> OTT, DRIVE */
{
    static const uint8_t C[2] = {FXI_OTT, FXI_DRIVE};
    chain(C, 2u);
}

static void t_chain(void)
{
    static const uint8_t DO[2] = {FXI_DRIVE, FXI_OTT}, D[1] = {FXI_DRIVE};
    double near, st;
    uint32_t dip, b, i, s = 21;
    eng_reset();
    sy_patch.p[SP_DR_AMT] = 600;
    chain(D, 1u);
    snap();
    near = chain_step(ch_add_eq, &st, &dip);
    printf("engine: chain: EQ added under a sine through DRIVE: largest step %.4f near it, steady %.4f; dip %u\n",
           near, st, (unsigned)dip);
    check("chain: a module added joins at once, its mix from 0 (no dip, no click: under 2x the steady step)",
          near < 2.0 * st && dip == 0u && fx.n == 2u && fx.ch[1] == FXI_EQ);
    eng_reset();
    sy_patch.p[SP_DR_AMT] = 600;
    chain(DO, 2u);
    snap();
    near = chain_step(ch_take_ott, &st, &dip);
    printf("engine: chain: OTT taken out under a sine through DRIVE and OTT: largest step %.4f near it, steady %.4f\n",
           near, st);
    check("chain: a module taken out fades out (over 8 blocks), then is gone (no dip, no click)",
          near < 2.0 * st && dip == 0u && fx.n == 1u && fx.ch[0] == FXI_DRIVE && !fx.out);
    eng_reset();
    sy_patch.p[SP_DR_AMT] = 600;
    chain(DO, 2u);
    snap();
    near = chain_step(ch_swap, &st, &dip);
    printf("engine: chain: DRIVE and OTT swapped under a sine: largest step %.4f near it, steady %.4f; dip %u\n", near,
           st, (unsigned)dip);
    check("chain: modules moved: a dip of the wet parts, then the new order (no click)",
          near < 2.0 * st && dip >= 3u && fx.n == 2u && fx.ch[0] == FXI_OTT && fx.ch[1] == FXI_DRIVE);
    {                                                     /* bypassed: as if not there, once faded */
        static const uint8_t F[1] = {FXI_FLANGER};
        float l[SY_BLK], r[SY_BLK], x[SY_BLK];
        int exact = 1;
        eng_reset();
        chain(F, 1u);
        sy_patch.p[SP_FL_MIX] = 1000;
        snap();
        for (b = 0; b < 40u; b++) {
            for (i = 0; i < SY_BLK; i++)
                x[i] = l[i] = r[i] = 0.3f * sy_randf(sy_rand(&s));
            if (b == 20u)
                sy_patch.p[SP_FL_ON] = 0;
            fx_run(l, r);
            for (i = 0; b > 29u && i < SY_BLK; i++)           /* (switched out over 8 blocks) */
                exact &= l[i] == x[i] && r[i] == x[i];
        }
        check("chain: a module bypassed (its ON off, the FX-held keys') is the input bit for bit once faded", exact);
    }
    {                                                     /* two of a type: each its own values, in order */
        static const uint8_t DD[2] = {FXI_DRIVE, FXI_DRIVE2};
        static float a[2][2048], c[2][2048];
        int same = 1;
        uint32_t k;
        for (k = 0; k < 2u; k++) {
            eng_reset();
            chain(DD, 2u);
            sy_patch.p[SP_DR_TYPE] = DRV_FOLD;
            sy_patch.p[SP_DR_AMT] = 400;
            sy_patch.p[SP_DR2_TYPE] = DRV_HARD;
            sy_patch.p[SP_DR2_AMT] = 900;
            sy_patch.p[SP_DR2_TONE] = -600;
            snap();
            mod_map();
            for (s = 5, b = 0; b < 2048u / SY_BLK; b++) {
                float l[SY_BLK], r[SY_BLK];
                for (i = 0; i < SY_BLK; i++)
                    l[i] = r[i] = 0.4f * sy_randf(sy_rand(&s));
                mod_offsets(sy_goff, 0, 0);
                if (!k) {
                    fx_block(l, r);
                } else {
                    fx_drive(l, r, FXI_DRIVE);
                    fx_drive(l, r, FXI_DRIVE2);
                    fx_limit(l, r);
                }
                for (i = 0; i < SY_BLK; i++)
                    (k ? c : a)[0][b * SY_BLK + i] = l[i], (k ? c : a)[1][b * SY_BLK + i] = r[i];
            }
        }
        for (i = 0; i < 2048u; i++)
            same &= a[0][i] == c[0][i] && a[1][i] == c[1][i];
        check("chain: DRIVE and DRIVE 2, each its own values, in the chain's order (as called so, bit for bit)", same);
    }
    {                                                     /* the slots made whole as the audio side takes them */
        float l[SY_BLK], r[SY_BLK];
        eng_reset();
        sy_patch.p[SP_FXC1] = FXI_TAPE + 1;
        sy_patch.p[SP_FXC2] = FXI_TAPE + 1;               /* (twice: once) */
        sy_patch.p[SP_FXC3] = FXI_RING + 1;
        sy_patch.p[SP_FXC4] = 0;
        sy_patch.p[SP_FXC5] = FXI_EQ + 1;                 /* (after a gap: not in it) */
        sy_zerof(l, SY_BLK);
        sy_zerof(r, SY_BLK);
        fx_run(l, r);
        check("chain: the slots as the audio side takes them: an instance once, none after the first empty slot",
              fx.n == 2u && fx.ch[0] == FXI_TAPE && fx.ch[1] == FXI_RING);
    }
}

/* the fades (V0.66): every type at full MIX (and DEPTH) under a 220 Hz sine, changed at 0.6 s: added (op 0), taken
 * out (1), bypassed (2), back in (3), moved past DRIVE (4). The burst a change makes: the energy of the output's second
 * difference (the high frequencies a quick ramp throws) over 25 blocks from the change, against the same over 25
 * steady blocks before it and after it (the larger) */
static double fade_burst(uint32_t inst, uint32_t op)
{
    uint8_t ch[2] = {(uint8_t)inst, FXI_DRIVE};
    double ph = 0, e[3] = {0, 0, 0};
    float p1 = 0.0f, p2 = 0.0f;
    uint32_t b, i, c = blocks(0.6f), k;
    eng_reset();
    for (k = 0; k < SP_COUNT; k++)
        if (sp_inst_of(k) == (int32_t)inst && (!strcmp(SP[k].name, "MIX") || !strcmp(SP[k].name, "DEPTH")))
            sy_patch.p[k] = SP[k].hi;
    sy_patch.p[SP_DR_AMT] = 600;
    if (op == 0u)
        chain(ch, 0u);
    else
        chain(ch, op == 4u ? 2u : 1u);
    if (op == 3u)
        sy_patch.p[FXI_ON[inst]] = 0;
    snap();
    for (b = 0; b < blocks(1.6f); b++) {
        float l[SY_BLK], r[SY_BLK];
        if (b == c) {
            if (op == 0u)
                chain(ch, 1u);
            else if (op == 1u)
                chain(ch, 0u);
            else if (op == 4u) {
                uint8_t sw[2] = {FXI_DRIVE, (uint8_t)inst};
                chain(sw, 2u);
            } else
                sy_patch.p[FXI_ON[inst]] = op == 3u;
        }
        for (i = 0; i < SY_BLK; i++, ph += 220.0 / (double)SY_FS)
            l[i] = r[i] = (float)(0.3 * sin(2.0 * M_PI * ph));
        fx_run(l, r);
        for (i = 0; i < SY_BLK; i++) {
            double d = (double)l[i] - 2.0 * (double)p1 + (double)p2;
            int w = b + 31u >= c && b + 5u < c ? 0 : b + 1u >= c && b < c + 24u ? 1 : b >= c + 40u && b < c + 65u ? 2 : -1;
            p2 = p1;
            p1 = l[i];
            if (w >= 0)
                e[w] += d * d;
        }
    }
    return e[1] / fmax(fmax(e[0], e[2]), 1e-12);
}

static void t_fades(void)
{
    static const char *const OP[5] = {"added", "taken out", "bypassed", "back in", "moved"};
    double worst = 0, w2 = 0;
    uint32_t t, op, wt = 0, wo = 0;
    for (t = 0; t < FXT_N; t++)
        for (op = 0; op < 5u; op++) {
            double r = fade_burst(FXT_INST[t][0], op);
            if (getenv("FADES"))
                printf("    %-10s %-10s %8.2f\n", FXT_NAMES[t], OP[op], r);
            if (r > worst)
                worst = r, wt = t, wo = op;
            if (op == 4u && r > w2)
                w2 = r;
        }
    printf("engine: fades: the largest burst %.2fx the steady output's (%s %s); moved, at most %.2fx\n", worst,
           FXT_NAMES[wt], OP[wo], w2);
    check("the fades: every module type at full MIX added, taken out, bypassed, back in and moved under a sine: no "
          "burst over 1.5x the steady output's (V0.6's one-block switches and 2.9 ms dip: see TESTING.md)", worst < 1.5);
}

/* ---------------------------------------------------------------------------------- the new modules --- */
static int flt_ok(const float *x, uint32_t n)             /* finite, none subnormal */
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (!isfinite(x[i]) || fpclassify(x[i]) == FP_SUBNORMAL)
            return 0;
    return 1;
}

/* instance i alone in the chain, its values at random corners (each at its low or high end), full-scale noise for
 * 2 s then silence for 4 s: finite, under the limiter's 1, every float state flushed (none subnormal) */
static int mod_extremes(uint32_t i, uint32_t seed, float *top)
{
    uint16_t ids[SP_FX_MAXP];
    uint32_t n = sp_inst_ids(i, ids), k, b, j, s = seed;
    int ok = 1;
    uint8_t one[1];
    one[0] = (uint8_t)i;
    eng_reset();
    chain(one, 1u);
    for (k = 1; k < n; k++)
        sy_patch.p[ids[k]] = (sy_rand(&s) & 1u) ? SP[ids[k]].hi : SP[ids[k]].lo;
    snap();
    *top = 0.0f;
    for (b = 0; b < blocks(6.0f); b++) {
        float l[SY_BLK], r[SY_BLK];
        for (j = 0; j < SY_BLK; j++) {
            l[j] = b < blocks(2.0f) ? sy_randf(sy_rand(&s)) : 0.0f;
            r[j] = b < blocks(2.0f) ? sy_randf(sy_rand(&s)) : 0.0f;
        }
        fx_run(l, r);
        for (j = 0; j < SY_BLK; j++) {
            ok &= isfinite(l[j]) && isfinite(r[j]);
            *top = fmaxf(*top, fmaxf(fabsf(l[j]), fabsf(r[j])));
        }
    }
    for (k = 0; k < 2u; k++) {                            /* (the float states that recurse: none subnormal) */
        ok &= flt_ok(fx.phs[k].s[0], 24u) && flt_ok(fx.phs[k].fb, 2u) && flt_ok(fx.ff[k].s[0], 8u);
        ok &= flt_ok(fx.ff[k].f.s[0], 12u) && flt_ok(fx.tap[k].lp, 2u) && flt_ok(fx.ott[k].env, 3u);
        ok &= flt_ok(fx.ott[k].s[0][0], 8u) && flt_ok(fx.eq[k].s[0][0], 12u) && flt_ok(fx.drv_lp[k], 2u);
    }
    return ok && *top <= 1.0f;
}

/* the left of a 1 kHz sine (0.5) times nothing but the chain, its power at hz (a bin of a 2^15 spectrum), dB */
static double chain_bin_db(double fin, const double *hz, double *out, uint32_t nh)
{
    static float o[1u << 15];
    double *p = malloc(((1u << 14) + 1u) * sizeof *p), ph = 0;
    uint32_t b, i, n = 0, k;
    for (b = 0; n < (1u << 15); b++) {
        float l[SY_BLK], r[SY_BLK];
        for (i = 0; i < SY_BLK; i++, ph += fin / (double)SY_FS)
            l[i] = r[i] = (float)(0.5 * sin(2.0 * M_PI * ph));
        fx_run(l, r);
        for (i = 0; i < SY_BLK && b >= 64u && n < (1u << 15); i++)
            o[n++] = l[i];
    }
    power_spectrum(o, 1u << 15, p);
    for (k = 0; k < nh; k++) {
        uint32_t c = (uint32_t)(hz[k] * (double)(1u << 15) / (double)SY_FS + 0.5), j;
        double e = 0;
        for (j = c - 2u; j <= c + 2u; j++)
            e += p[j];
        out[k] = 10.0 * log10(e + 1e-30);
    }
    free(p);
    return 0;
}

static void t_fx_new(void)
{
    static const uint8_t NEW[] = {FXI_FLANGER, FXI_FLANGER2, FXI_PHASER, FXI_PHASER2, FXI_FFILT, FXI_FFILT2,
                                  FXI_CRUSH, FXI_CRUSH2, FXI_TREM, FXI_TREM2, FXI_RING, FXI_RING2, FXI_TAPE,
                                  FXI_TAPE2, FXI_DRIVE2, FXI_CHORUS2, FXI_OTT2, FXI_EQ2};
    uint32_t k, t, seed = 1, bad = 0;
    float top, worst = 0.0f;
    for (k = 0; k < sizeof NEW; k++)
        for (t = 0; t < 6u; t++, seed += 7u)
            if (!mod_extremes(NEW[k], seed, &top))
                bad++, printf("engine: %s at its corners (seed %u): unbounded or not flushed (peak %.3f)\n",
                              SEC_NAMES[SEC_DRIVE + NEW[k]], (unsigned)seed, (double)top);
            else
                worst = fmaxf(worst, top);
    check("the new modules and the second instances at their corners on full-scale noise: finite, under 1, flushed",
          !bad);
    for (t = 0, bad = 0; t < FF_COUNT; t++) {             /* the FX filter's every type, the same */
        uint32_t c;
        for (c = 0; c < 4u; c++) {
            static const uint8_t F[1] = {FXI_FFILT};
            uint32_t b, j, s = 77u + c;
            int ok = 1;
            eng_reset();
            chain(F, 1u);
            sy_patch.p[SP_FF_TYPE] = (int16_t)t;
            sy_patch.p[SP_FF_CUT] = (int16_t)(c & 1u ? 1000 : 0);
            sy_patch.p[SP_FF_RES] = (int16_t)(c & 2u ? 1000 : 0);
            sy_patch.p[SP_FF_DRIVE] = (int16_t)(c == 3u ? 1000 : 0);
            snap();
            for (b = 0; b < blocks(3.0f); b++) {
                float l[SY_BLK], r[SY_BLK];
                for (j = 0; j < SY_BLK; j++)
                    l[j] = r[j] = b < blocks(1.5f) ? sy_randf(sy_rand(&s)) : 0.0f;
                if ((b & 15u) == 0u)                      /* the cutoff jumping 20 Hz <-> 20 kHz */
                    sy_patch.p[SP_FF_CUT] = (int16_t)(1000 - sy_patch.p[SP_FF_CUT]);
                fx_run(l, r);
                for (j = 0; j < SY_BLK; j++)
                    ok &= isfinite(l[j]) && fabsf(l[j]) <= 1.0f;
            }
            if (!ok)
                bad++, printf("engine: FX filter %s (corner %u): unbounded\n", FF_NAMES[t], (unsigned)c);
        }
    }
    check("the FX filter's 21 types at their corners, the cutoff jumping 20 Hz <-> 20 kHz: finite, under 1", !bad);
    {                                                     /* the FX filter's responses */
        static const uint8_t F[1] = {FXI_FFILT};
        double lp6, hp6, shelf, dj, cp, cq;
        eng_reset();
        chain(F, 1u);
        sy_patch.p[SP_FF_TYPE] = FF_LP6;
        sy_patch.p[SP_FF_CUT] = 566;                      /* (1.00 kHz) */
        snap();
        lp6 = chain_gain_db(1000.0);
        sy_patch.p[SP_FF_TYPE] = FF_HP6;
        snap();
        hp6 = chain_gain_db(1000.0);
        sy_patch.p[SP_FF_TYPE] = FF_LSHELF;
        sy_patch.p[SP_FF_RES] = 1000;                     /* (+18 dB) */
        snap();
        shelf = chain_gain_db(40.0);
        sy_patch.p[SP_FF_TYPE] = FF_DJ;
        sy_patch.p[SP_FF_CUT] = 500;                      /* (the middle: open) */
        sy_patch.p[SP_FF_RES] = 0;
        snap();
        dj = chain_gain_db(1000.0);
        sy_patch.p[SP_FF_TYPE] = FF_COMBP;
        sy_patch.p[SP_FF_CUT] = 456;                      /* (441 Hz) */
        sy_patch.p[SP_FF_RES] = 900;
        snap();
        cp = chain_gain_db(sp_cut_hz(0.456f));
        cq = chain_gain_db(1.5 * (double)sp_cut_hz(0.456f));
        printf("engine: FX filter at 1 kHz: LP 6 %+.2f dB, HP 6 %+.2f dB; LO SHELF +18 dB at 40 Hz %+.2f dB; DJ open "
               "%+.2f dB; COMB + %+.1f dB at its frequency, %+.1f dB halfway\n", lp6, hp6, shelf, dj, cp, cq);
        check("FX filter: LP 6 and HP 6 -3 dB at the cutoff (+-0.5), LO SHELF its gain (+-1), DJ open flat (+-0.2), "
              "COMB + a peak at its frequency (10 dB over halfway)",
              fabs(lp6 + 3.01) < 0.5 && fabs(hp6 + 3.01) < 0.5 && fabs(shelf - 18.0) < 1.0 && fabs(dj) < 0.2 &&
              cp > cq + 10.0);
    }
    {                                                     /* PHASER: two stages, still, at 1 kHz, half dry: a notch */
        static const uint8_t P[1] = {FXI_PHASER};
        double at, lo;
        eng_reset();
        chain(P, 1u);
        sy_patch.p[SP_PH_STAGES] = 0;
        sy_patch.p[SP_PH_DEPTH] = 0;
        sy_patch.p[SP_PH_CENTER] = 566;
        sy_patch.p[SP_PH_FDBK] = 0;
        sy_patch.p[SP_PH_MIX] = 500;
        snap();
        at = chain_gain_db(1000.0);
        lo = chain_gain_db(60.0);
        printf("engine: PHASER, two stages at 1 kHz, still, half dry: %+.1f dB at 1 kHz, %+.2f dB at 60 Hz\n", at, lo);
        check("PHASER: two still stages and the dry make a notch at CENTER (under -30 dB), flat far under it (-1 dB)",
              at < -30.0 && lo > -1.0);
    }
    {                                                     /* FLANGER: still at 0.3 ms, half dry: a notch at 1.67 kHz */
        static const uint8_t F[1] = {FXI_FLANGER};
        double at, lo, f0 = 1.0 / (2.0 * 0.0003);
        eng_reset();
        chain(F, 1u);
        sy_patch.p[SP_FL_DEPTH] = 0;
        sy_patch.p[SP_FL_DELAY] = 0;
        sy_patch.p[SP_FL_FDBK] = 0;
        sy_patch.p[SP_FL_MIX] = 500;
        snap();
        at = chain_gain_db(f0);
        lo = chain_gain_db(60.0);
        printf("engine: FLANGER, still at 0.3 ms, half dry: %+.1f dB at %.0f Hz, %+.2f dB at 60 Hz\n", at, f0, lo);
        check("FLANGER: still, half dry: its first notch at 1 / (2 x delay) (under -20 dB), flat far under it",
              at < -20.0 && lo > -1.0);
    }
    {                                                     /* RING MOD: 1 kHz times 300 Hz: 700 and 1300 Hz, no 1 kHz */
        static const uint8_t G[1] = {FXI_RING};
        static const double HZ[3] = {1000.0, 700.0, 1300.0};
        double db[3];
        eng_reset();
        chain(G, 1u);
        sy_patch.p[SP_RG_FREQ] = 392;                     /* (300 Hz) */
        sy_patch.p[SP_RG_MIX] = 1000;
        snap();
        chain_bin_db(1000.0, HZ, db, 3u);
        printf("engine: RING MOD, 1 kHz times 300 Hz: %.1f dB at 1 kHz, %.1f and %.1f dB at 700 and 1300 Hz\n", db[0],
               db[1], db[2]);
        check("RING MOD: the sum and the difference (700, 1300 Hz), the input gone (40 dB under them)",
              db[1] > db[0] + 40.0 && db[2] > db[0] + 40.0 && fabs(db[1] - db[2]) < 1.0);
    }
    {                                                     /* TREMOLO and AUTOPAN */
        static const uint8_t T[1] = {FXI_TREM};
        float lo = 1.0f, hi = 0.0f, cl = 0.0f, cr = 0.0f;
        double corr = 0;
        uint32_t b, i, m;
        for (m = 0; m < 2u; m++) {
            eng_reset();
            chain(T, 1u);
            sy_patch.p[SP_TR_MODE] = (int16_t)m;
            sy_patch.p[SP_TR_DEPTH] = 1000;
            snap();
            for (b = 0; b < blocks(2.0f); b++) {
                float l[SY_BLK], r[SY_BLK];
                for (i = 0; i < SY_BLK; i++)
                    l[i] = r[i] = 0.5f;
                fx_run(l, r);
                if (b < blocks(0.5f))
                    continue;
                if (!m) {
                    lo = fminf(lo, l[0]);
                    hi = fmaxf(hi, l[0]);
                } else {
                    cl = l[0] - 0.5f;
                    cr = r[0] - 0.5f;
                    corr += (double)cl * (double)cr;
                }
            }
        }
        printf("engine: TREMOLO at DEPTH 100 %% on 0.5: %.3f .. %.3f; AUTOPAN: the sides' swings %s\n", (double)lo,
               (double)hi, corr < 0.0 ? "opposite" : "together");
        check("TREMOLO: the level from 0 to all of it at full DEPTH; AUTOPAN: the sides swing opposite",
              lo < 0.02f && hi > 0.48f && corr < 0.0);
    }
    {                                                     /* TAPE, still and clean: a 200 Hz sine 4 ms later */
        static const uint8_t T[1] = {FXI_TAPE};
        static float ou[1u << 14];
        uint32_t b, i, n = 0;
        double xy = 0, xx = 0, yy = 0, ph = 0;
        eng_reset();
        chain(T, 1u);
        sy_patch.p[SP_TP_WOW] = sy_patch.p[SP_TP_FLUT] = sy_patch.p[SP_TP_SAT] = sy_patch.p[SP_TP_TONE] = 0;
        snap();
        for (b = 0; n < (1u << 14); b++) {
            float l[SY_BLK], r[SY_BLK];
            for (i = 0; i < SY_BLK; i++, ph += 200.0 / (double)SY_FS)
                l[i] = r[i] = (float)(0.3 * sin(2.0 * M_PI * ph));
            fx_run(l, r);
            for (i = 0; i < SY_BLK && n < (1u << 14); i++)
                ou[n++] = l[i];
        }
        for (i = 4096u; i < (1u << 14); i++) {           /* (against the sine as it was 4 ms before) */
            double x = 0.3 * sin(2.0 * M_PI * 200.0 * ((double)i / (double)SY_FS - 0.004));
            xy += x * (double)ou[i];
            xx += x * x;
            yy += (double)ou[i] * (double)ou[i];
        }
        printf("engine: TAPE still and clean: the output against the input 4 ms before, correlation %.4f\n",
               xy / sqrt(xx * yy));
        check("TAPE: no wow, flutter or saturation: the input 4 ms later (correlation over 0.999)", xy / sqrt(xx * yy) > 0.999);
    }
    {                                                     /* BITCRUSH: one bit, three levels (the module alone) */
        uint32_t b, i, s = 9;
        int ok = 1;
        eng_reset();
        sy_patch.p[SP_CR_ON] = 1;
        sy_patch.p[SP_CR_BITS] = 1;
        sy_patch.p[SP_CR_MIX] = 1000;
        snap();
        for (b = 0; b < 200u; b++) {
            float l[SY_BLK], r[SY_BLK];
            for (i = 0; i < SY_BLK; i++)
                l[i] = r[i] = 0.9f * sy_randf(sy_rand(&s));
            sp_block();
            mod_map();
            mod_offsets(sy_goff, 0, 0);
            fx_crush(l, r, FXI_CRUSH);
            for (i = 0; b > 2u && i < SY_BLK; i++)
                ok &= fabsf(l[i] + 1.0f) < 1e-6f || fabsf(l[i]) < 1e-6f || fabsf(l[i] - 1.0f) < 1e-6f;
        }
        check("BITCRUSH at 1 bit: three levels", ok);
    }
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
    t_hush();
    t_health();
    t_dc_block();
    t_dc_sources();
    t_golden(argc > 1 ? argv[1] : "tests/sy_golden.txt");
    t_fx();
    t_fx_order();
    t_eq();
    t_chain();
    t_fades();
    t_fx_new();
    t_cost();
    return t_done();
}
