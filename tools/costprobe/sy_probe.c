/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* The Strata DSP for the target listing (tools/costprobe/sy_cost.sh): the firmware's sy_* sources compiled as one
 * unit by the JieLi clang, each kernel reached from an exported function so the compiler keeps its loop as the
 * firmware will have it. Not linked: only counted. */
#include <stdint.h>
void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);

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
#include "../../firmware/src/sy_engine.c"

uint32_t probe_osc(uint32_t k, uint32_t g, uint32_t f, int32_t *lr, const osc_job_t *j)
{
    return OSC_KS[k][g][f](lr, SY_BLK, j);
}
void probe_svf(uint32_t rs, float *x, float *st, const float *c0, const float *c1) { FLT_SVF[rs](x, st, c0, c1); }
void probe_ladder(float *x, float *st) { flt_ladder(x, st, 0.1f, 0.2f, 1.0f, 2.0f); }
void probe_sub(sy_voice_t *v, uint32_t shape, float *x) { voice_sub(v, 1u << 25, shape, 0.3f, x); }
void probe_noise(sy_voice_t *v, uint32_t type, float *l, float *r) { voice_noise(v, type, 0.2f, 0.5f, l, r); }
void probe_mix(float *bl, float *br, const float *x, const float *y, const float *g0, const float *g1)
{
    voice_mix(bl, br, x, y, g0, g1);
}
void probe_fx(float *l, float *r) { fx_block(l, r); }
void probe_voice(sy_voice_t *v, float *l, float *r) { voice_block(v, l, r); }
void probe_engine(int32_t *out) { sy_block(out); }
void probe_init(void)
{
    sy_init();
    sy_tables_now();
}
int probe_service(void)
{
    sy_service();
    return wg_step();
}
