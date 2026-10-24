/* SPDX-License-Identifier: GPL-3.0-only */
/* STRATA's workbench: the synth's DSP (firmware/src/sy_*) as WebAssembly for the browser page web/strata/index.html.
 * Two instances of it run there: the AudioWorklet's plays (sw_render a render quantum at a time, the notes, the
 * parameters the page sends, a patch load faded as on the FM-1), and the page's own answers what the page needs to
 * know (the parameter table, a value as text, the test sounds, a patch's code). No HAL, no UI of the FM-1: that is
 * web/emu's. tests/sy_wasm.sh checks this DSP as WebAssembly; web/strata/wb_test.mjs checks this file's exports.
 *   web/strata/build.sh  ->  build/strata/strata_workbench.html (one file: the module and the worklet inside) */
#include <stdint.h>
#include <string.h>

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
#include "../../firmware/src/sy_sounds.h"

#define EXPORT __attribute__((used, visibility("default")))
#define SW_QUANTUM 128u                  /* the worklet's render quantum */

static float sw_l[SW_QUANTUM], sw_r[SW_QUANTUM];
static int32_t sw_blk[2u * SY_BLK];
static char sw_txt[SP_TEXT_N];
static uint8_t sw_code[1024];            /* a patch packed (sp_pack): in and out */
static int16_t sw_vals[SP_COUNT];        /* a patch's values: in and out */
static char sw_name[17];
static sp_draw_t sw_draw[2];             /* .. its LFOs' drawn shapes (kept through a code; drawn on the FM-1) */
static float sw_peak;

/* ------------------------------------------------------------------------------------------- audio --- */
EXPORT void sw_init(void)
{
    sy_init();
    sy_tables_now();
}
EXPORT float sw_rate(void) { return SY_FS; }
EXPORT float *sw_out_l(void) { return sw_l; }
EXPORT float *sw_out_r(void) { return sw_r; }

/* n frames (a multiple of 32, at most 128), then a step of a table being built (the FM-1's main loop's work) */
EXPORT void sw_render(uint32_t n)
{
    uint32_t b, i;
    for (b = 0; b + SY_BLK <= n && b < SW_QUANTUM; b += SY_BLK) {
        sy_block(sw_blk);
        for (i = 0; i < SY_BLK; i++) {
            float l = (float)sw_blk[2u * i] * (1.0f / 32768.0f), r = (float)sw_blk[2u * i + 1u] * (1.0f / 32768.0f);
            sw_l[b + i] = l;
            sw_r[b + i] = r;
            sw_peak = sy_maxf(sw_peak, sy_maxf(sy_absf(l), sy_absf(r)));
        }
    }
    sy_service();
}
EXPORT float sw_take_peak(void)
{
    float p = sw_peak;
    sw_peak = 0.0f;
    return p;
}
EXPORT void sw_note(uint32_t note, uint32_t vel)
{
    if (vel)
        sy_note_on(note & 127u, vel > 127u ? 127u : vel);
    else
        sy_note_off(note & 127u);
}
EXPORT void sw_bend(int32_t v14) { sy_bend(v14); }
EXPORT void sw_cc(uint32_t cc, uint32_t v) { sy_cc(cc, v); }
EXPORT void sw_panic(void) { sy_all_sound_off(); }
EXPORT void sw_tempo(float bpm) { sy_tempo = sy_clampf(bpm, 20.0f, 300.0f); }
EXPORT uint32_t sw_voices(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < SY_NVOICE; i++)
        n += sy_v[i].on != 0;
    return n;
}
EXPORT uint32_t sw_instances(void) { return sy_instances(); }

/* ------------------------------------------------------------------------------------ the parameters --- */
EXPORT uint32_t sw_param_count(void) { return SP_COUNT; }
EXPORT uint32_t sw_section_count(void) { return SEC_COUNT; }
EXPORT const char *sw_section_name(uint32_t s) { return s < SEC_COUNT ? SEC_NAMES[s] : ""; }
EXPORT const char *sw_param_name(uint32_t id) { return SP[id].name; }
EXPORT uint32_t sw_param_section(uint32_t id) { return SP[id].sec; }
EXPORT uint32_t sw_param_unit(uint32_t id) { return SP[id].unit; }
EXPORT uint32_t sw_param_flags(uint32_t id) { return SP[id].flags; }
EXPORT int32_t sw_param_lo(uint32_t id) { return SP[id].lo; }
EXPORT int32_t sw_param_hi(uint32_t id) { return SP[id].hi; }
EXPORT int32_t sw_param_def(uint32_t id) { return SP[id].def; }
EXPORT uint32_t sw_param_cc(uint32_t id) { return SP[id].cc; }
/* how the page shows it: 0 a slider, 1 a list (names), 2 a switch; sw_param_dst: a matrix destination (its list:
 * NONE and the modulatable parameters) */
EXPORT uint32_t sw_param_kind(uint32_t id)
{
    uint32_t u = SP[id].unit;
    return u == U_ONOFF ? 2u : (u == U_LIST || u == U_TABLE || u == U_SRC || u == U_DST || u == U_SYNC) ? 1u : 0u;
}
EXPORT uint32_t sw_param_dst(uint32_t id) { return SP[id].unit == U_DST; }
EXPORT uint32_t sw_param_mod(uint32_t id) { return id < SP_COUNT && (SP[id].flags & F_MOD) != 0; }
EXPORT const char *sw_text(uint32_t id, int32_t v)
{
    sp_text(id, v, sw_txt);
    return sw_txt;
}
EXPORT int32_t sw_clamp(uint32_t id, int32_t v) { return sp_clamp(id, v); }
/* the effects chain as the page has it: a module whose ON the page turns on joins the chain's end (the FM-1 adds
 * and takes them out on its FX page; here the panels' switches do) */
static void sw_chain_on(int16_t *p)
{
    uint32_t i, n = sp_chain_n(p);
    for (i = 0; i < FXI_N && n < NFXC; i++)
        if (p[FXI_ON[i]] && sp_chain_find(p, i) < 0)
            p[SP_FXC1 + n++] = (int16_t)(i + 1u);
}

/* the playing instance: a value the page set (the smoother glides the continuous ones, as on the FM-1) */
EXPORT void sw_set(uint32_t id, int32_t v)
{
    if (id < SP_COUNT) {
        sy_patch.p[id] = sp_clamp(id, v);
        sw_chain_on(sy_patch.p);
    }
}

/* --------------------------------------------------------------------------------------- patches --- */
EXPORT int16_t *sw_values(void) { return sw_vals; }
EXPORT char *sw_patch_name(void) { return sw_name; }
EXPORT uint8_t *sw_code_buf(void) { return sw_code; }
EXPORT uint32_t sw_code_size(void) { return sizeof sw_code; }

static void sw_from(const sy_patch_t *p)
{
    memcpy(sw_vals, p->p, sizeof sw_vals);
    memcpy(sw_name, p->name, sizeof sw_name);
    sw_name[16] = 0;
    memcpy(sw_draw, p->draw, sizeof sw_draw);
}

static void sw_to(sy_patch_t *p)
{
    uint32_t i;
    sp_defaults(p);
    for (i = 0; i < SP_COUNT; i++)
        p->p[i] = sp_clamp(i, sw_vals[i]);
    sp_chain_fix(p->p);
    sw_chain_on(p->p);
    memcpy(p->name, sw_name, sizeof p->name);
    p->name[16] = 0;
    for (i = 0; i < 2u; i++)
        if (sw_draw[i].n >= 2u) {                /* (none yet: the default's) */
            p->draw[i] = sw_draw[i];
            sp_draw_fix(&p->draw[i]);
        }
}

EXPORT uint32_t sw_sound_count(void) { return sizeof SOUNDS / sizeof SOUNDS[0]; }
EXPORT const char *sw_sound_name(uint32_t k) { return k < sw_sound_count() ? SOUNDS[k].name : ""; }

/* the values of a test sound (k) or of INIT (k past the last) into sw_values / sw_patch_name */
EXPORT void sw_sound(uint32_t k)
{
    sy_patch_t p;
    uint32_t i, n;
    sp_defaults(&p);
    if (k < sw_sound_count()) {
        for (i = 0; SOUNDS[k].kv[i][0] >= 0; i++)
            p.p[SOUNDS[k].kv[i][0]] = sp_clamp((uint32_t)SOUNDS[k].kv[i][0], SOUNDS[k].kv[i][1]);
        sp_chain_from_old(&p);                   /* (their effects as the six were: those ON, the chain) */
        for (n = 0; n < 16u && SOUNDS[k].name[n]; n++)
            p.name[n] = (char)(SOUNDS[k].name[n] >= 'a' && SOUNDS[k].name[n] <= 'z' ? SOUNDS[k].name[n] - 32
                                                                                      : SOUNDS[k].name[n]);
        p.name[n] = 0;
    }
    sw_from(&p);
}

/* sw_values / sw_patch_name packed into sw_code_buf: its size */
EXPORT uint32_t sw_pack(void)
{
    sy_patch_t p;
    sw_to(&p);
    return sp_pack(&p, sw_code);
}

/* sw_code_buf (n bytes) unpacked into sw_values / sw_patch_name: 0 if it is not a patch */
EXPORT int sw_unpack(uint32_t n)
{
    sy_patch_t p;
    if (n > sizeof sw_code || !sp_unpack(&p, sw_code, n))
        return 0;
    sw_from(&p);
    return 1;
}

/* the playing instance: sw_values / sw_patch_name become the sound (faded, tables rebuilt as on the FM-1).
 * 0: a load still in flight (the page asks again next quantum; sy_patch_load would wait for the audio) */
EXPORT int sw_load(void)
{
    sy_patch_t p;
    if (sy_next_st)
        return 0;
    sw_to(&p);
    sy_patch_load(&p);
    return 1;
}

/* the playing instance's values now (after a load has swapped: what the page shows) */
EXPORT void sw_current(void) { sw_from(&sy_patch); }
EXPORT int sw_loading(void) { return sy_next_st != 0; }

/* ----------------------------------------------------------------------------------- for wb_test.mjs --- */
/* a test sound at once, as the engine test plays it (sy_init, the values, the tables built): no fade */
EXPORT void sw_test_sound(uint32_t k)
{
    uint32_t i;
    sy_init();
    sy_tables_now();
    for (i = 0; k < sw_sound_count() && SOUNDS[k].kv[i][0] >= 0; i++)
        sy_patch.p[SOUNDS[k].kv[i][0]] = sp_clamp((uint32_t)SOUNDS[k].kv[i][0], SOUNDS[k].kv[i][1]);
    sp_chain_from_old(&sy_patch);
    sp_snap();
    sy_tables_now();
}
EXPORT void sw_test_bend(int32_t v14) { sy_bend(v14); }
/* LFO k's drawn shape in sw_values' patch: point i's x, y or curve (f 0..2), set to v first (set) */
EXPORT int32_t sw_test_point(uint32_t k, uint32_t i, uint32_t f, int32_t v, uint32_t set)
{
    sp_draw_t *d = &sw_draw[k & 1u];
    int16_t *a = f == 0u ? d->x : f == 1u ? d->y : d->c;
    if (i >= SP_NPT)
        return 0;
    if (set)
        a[i] = (int16_t)v;
    return a[i];
}
/* the engine test's blocks(secs), the same float arithmetic */
EXPORT uint32_t sw_test_blocks(float secs) { return (uint32_t)(secs * SY_FS) / SY_BLK; }
