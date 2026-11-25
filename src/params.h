// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// A pad's sound parameters as data (UI v2, step 1): every P_* id that is a number has a value, a range and two steps
// (EDIT+<> small, EDIT+^v big). Screens and the simple view read and set them through these, by pad and id.
// What is not a plain number stays special in par_adjust: TYPE/SAMPLE/SOURCE (files), NORM, DIR/VOICE (toggles),
// GRAIN/FLUX (0 = per mode), BPM (first touch takes the detected tempo), BARS (a list). WAVE: P_WV + wp index (2026-11-25).
// main.c includes this once, right before par_adjust.
#pragma once

static int par_get(int pad, int p) {     // the stored value; 0 for what is not a number
    const Track *t = &song.t[pad];
    if (p == P_SLWAY) return t->slices == 5 ? 1 : t->slices == 6 ? 2 : 0;
    if (p == P_WENV) return wv_env; if (p == P_WLFO) return wv_lfo; if (p == P_WTGT) return 0;
    if (p >= P_WV) return song.wv[pad][p - P_WV];
    if (p >= P_AX) return song.ax[pad][p - P_AX];
    if (p >= P_CX) return song.cx[pad][p - P_CX];
    if (p >= P_CL) return t->cp[p - P_CL];
    if (p >= P_AC) return t->ap[p - P_AC];
    if (p >= P_SX) return song.smx[pad][p - P_SX];
    switch (p) {
    case P_PITCH: return t->pitch; case P_FINE: return t->fine; case P_VOL: return t->vol; case P_PAN: return t->pan;
    case P_START: return t->start; case P_END: return t->end; case P_DIR: return t->rev; case P_VOICE: return t->voice;
    case P_ROOT: return t->root; case P_SLICES: return t->slices; case P_WARP: return t->warp;
    case P_GRAIN: return t->grain; case P_FLUX: return t->flux; case P_BPM: return t->bpm; case P_BARS: return t->bars;
    case P_TYPE: return t->type; case P_NORM: return song.mix.pad[pad].spare[PM_NORM];
    }
    return 0;
}
static void par_limits(int pad, int p, int *lo, int *hi, int *step, int *big) {   // range and the two steps
    const Track *t = &song.t[pad]; *lo = 0; *hi = 100; *step = 1; *big = 10;
    if (p == P_SLWAY) { *hi = song.cut[pad].n ? 2 : 1; *big = 1; return; }   // BY HAND once the slice editor has made points
    if (p == P_WENV || p == P_WLFO || p == P_WTGT) { *hi = p == P_WENV ? 2 : p == P_WLFO ? 3 : 0; *big = 1; return; }
    if (p >= P_WV) { int k = p - P_WV; wp_range(k, lo, hi); *big = *hi - *lo <= 24 ? (k < WP_SUB_LEVEL && (k & 15) == WO_SEMI ? 12 : 1) : 10; return; }   // SEMI: an octave
    if (p >= P_AX) { int k = p - P_AX; ax_range(k, lo, hi); *big = *hi > 4 ? 10 : 1; return; }
    if (p >= P_CX) { int k = p - P_CX; cx_range(k, lo, hi); *big = k == CX_PITCH ? 12 : *hi - *lo <= 4 ? 1 : 10; return; }
    if (p >= P_CL) return;
    if (p >= P_AC) { int k = p - P_AC; ap_range(k, lo, hi); *big = k == AP_TUNE ? 12 : k == AP_WAVE || k == AP_DTYPE ? 1 : 10; return; }
    if (p >= P_SX) return;
    switch (p) {
    case P_PITCH: *lo = -24; *hi = 24; *big = 12; break;
    case P_FINE: *lo = -50; *hi = 50; break;
    case P_PAN: *lo = -50; *hi = 50; break;
    case P_START: *hi = t->end - 5; *step = 5; *big = 50; break;
    case P_END: *lo = t->start + 5; *hi = 1000; *step = 5; *big = 50; break;
    case P_DIR: case P_VOICE: *hi = 1; *big = 1; break;
    case P_ROOT: *lo = -1; *hi = 11; *big = 1; break;
    case P_SLICES: *hi = 4; *big = 1; break;   // OFF 4 8 16 32 (equal); TRANSIENTS and BY HAND are P_SLWAY
    case P_WARP: *hi = 5; *big = 1; break;
    case P_GRAIN: *lo = 5; *hi = 120; break;
    case P_FLUX: *lo = 1; break;
    case P_BPM: *hi = 300; break;           // 0 AUTO, else 40..300
    case P_BARS: *hi = 16; *big = 1; break;  // 0 AUTO, 1 2 4 8 16
    case P_TYPE: *hi = 3; *big = 1; break;
    case P_NORM: *hi = 2; *big = 1; break;
    }
}
static void par_put(int pad, int p, int v) {   // stores a value already in range, with what has to follow it
    Track *t = &song.t[pad];
    if (p == P_SLWAY) { static int8_t eq[NSLOT]; if (t->slices >= 1 && t->slices <= 4) eq[pad] = t->slices;   // the equal count, kept for the way back
        t->slices = (int8_t)(v == 1 ? 5 : v == 2 ? 6 : eq[pad] ? eq[pad] : 2); relayout(pad); return; }
    if (p == P_WENV) { wv_env = v; return; } if (p == P_WLFO) { wv_lfo = v; return; } if (p == P_WTGT) return;
    if (p >= P_WV) { int k = p - P_WV; song.wv[pad][k] = (int8_t)v; if (k == WP_POLY) t->voice = v > 1; return; }   // VOICES: the pad's MONO / POLY follows (the roll)
    if (p >= P_AX) { song.ax[pad][p - P_AX] = (int8_t)v; song.ax[pad][AX_MARK] = AX_INIT; return; }
    if (p >= P_CX) { song.cx[pad][p - P_CX] = (int8_t)v; return; }
    if (p >= P_CL) { t->cp[p - P_CL] = (int8_t)v; return; }
    if (p >= P_AC) { t->ap[p - P_AC] = (int8_t)v; return; }
    if (p >= P_SX) { song.smx[pad][p - P_SX] = (int8_t)v; return; }
    switch (p) {
    case P_PITCH: t->pitch = (int8_t)v; break; case P_FINE: t->fine = (int8_t)v; break;
    case P_VOL: t->vol = (int8_t)v; break; case P_PAN: t->pan = (int8_t)v; break;
    case P_START: t->start = (int16_t)v; break; case P_END: t->end = (int16_t)v; break;
    case P_DIR: t->rev = (int8_t)v; break; case P_VOICE: t->voice = (int8_t)v; break;
    case P_ROOT: t->root = (int8_t)v; break; case P_WARP: t->warp = (int8_t)v; break;
    case P_SLICES: t->slices = (int8_t)v; relayout(pad); break;
    case P_GRAIN: t->grain = (int8_t)v; break; case P_FLUX: t->flux = (int8_t)v; break;
    case P_BPM: t->bpm = (int16_t)v; relayout(pad); break;
    case P_BARS: t->bars = (int8_t)v; relayout(pad); break;
    case P_NORM: song.mix.pad[pad].spare[PM_NORM] = (int8_t)v; break;
    }
}
static int par_is_number(int p) { return p != P_SAMPLE && p != P_TYPE && p != P_SRC && p != P_WENV && p != P_WLFO && p != P_WTGT; }
