/* SPDX-License-Identifier: GPL-3.0-only */
/* The test projects (V0.87.1): four built-in, read-only projects, each stressing one part of Strata (VOICES, FX,
 * DRUMS, LOOPER), loaded from DEBUG > TESTS or the console's `tproj N`. Loading one saves nothing and keeps the
 * projects' auto-save off while it is loaded; loading any project or a restart ends it. Each stops by itself.
 * (The interface the UI and the console use; the projects themselves: to come.) */
#define SY_TP_N 4u
static const char *const SY_TP_NAME[SY_TP_N] = {"VOICES", "FX", "DRUMS", "LOOPER"};
static uint8_t sy_tp_on;                         /* the test project loaded + 1, 0 none */

static const char *sy_tp_name(uint32_t k) { return k < SY_TP_N ? SY_TP_NAME[k] : ""; }
static int sy_tp_active(void) { return sy_tp_on; }
static uint32_t sy_tp_ms(uint32_t k) { (void)k; return 0u; }
static int sy_tp_load(uint32_t k) { (void)k; return 0; }
static void sy_tp_stop(void) { }
