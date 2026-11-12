// BrickPad effects: names, ranges and defaults (shared by the UI and the engine), then the DSP (engine only,
// FX_DSP defined). Per pad 2 insert slots (FxSlot in bp.h), send A = REVERB, send B = DELAY, master comp + limiter.
// Every parameter is a byte, 0..100 unless its range says otherwise. Runs in the audio callback: no allocation.
#pragma once
#include "bp.h"

static const char *const fx_name[FX_N] = {"NONE", "COMPRESSOR", "DISTORTION", "CHORUS", "FLANGER", "PHASER", "FILTER", "BITCRUSHER", "TAPE", "AUTOPAN", "RING MOD", "MULTIBAND"};
static const char *const fx_short[FX_N] = {"---", "CMP", "DST", "CHO", "FLG", "PHS", "FLT", "BIT", "TAP", "PAN", "RNG", "MLT"};
static const char *const fx_pname[FX_N][6] = {
    {"", "", "", "", "", ""},
    {"THRESH", "RATIO", "ATTACK", "RELEASE", "MAKEUP", "MIX"},
    {"MODE", "DRIVE", "TONE", "LEVEL", "MIX", ""},
    {"RATE", "DEPTH", "DELAY", "FEEDBACK", "WIDTH", "MIX"},
    {"RATE", "DEPTH", "MANUAL", "FEEDBACK", "WIDTH", "MIX"},
    {"RATE", "DEPTH", "CENTER", "FEEDBACK", "STAGES", "MIX"},
    {"MODE", "CUTOFF", "RESO", "DRIVE", "LFO RATE", "LFO DEPTH"},
    {"BITS", "DOWNSAMPLE", "JITTER", "TONE", "MIX", ""},
    {"DRIVE", "WOW", "FLUTTER", "TONE", "AGE", "MIX"},
    {"RATE", "DEPTH", "SHAPE", "PHASE", "", ""},
    {"FREQ", "FINE", "LFO RATE", "LFO DEPTH", "MIX", ""},
    {"DEPTH", "TIME", "UPWARD", "DOWNWARD", "X LOW", "X HIGH"},
};
static const int8_t fx_def[FX_N][6] = {
    {0}, {60, 30, 30, 40, 0, 100}, {0, 40, 70, 50, 100, 0}, {30, 50, 40, 0, 100, 50}, {20, 60, 30, 50, 100, 50},
    {25, 70, 50, 40, 1, 50}, {0, 60, 20, 0, 0, 0}, {50, 30, 0, 100, 100, 0}, {30, 20, 20, 70, 0, 100},
    {8, 70, 0, 0, 0, 0}, {40, 0, 0, 0, 50, 0}, {60, 50, 100, 100, 23, 41},
};
// What an insert costs, measured on the Brick (tools/fxcost.c, 2026-11-11): tenths of a per cent of one audio buffer
// (2.667 ms) per running instance. FX_BUDGET = 40 % of a buffer for inserts in all: both slots of all 12 pads fit with
// any type but MULTIBAND (18 of those). The old weights were guesses, 3-5 times too careful (the cap of 24 units was
// about 9 % of a buffer). Silent pads cost nothing; the real load is the DSP figure in the footer.
static const int8_t fx_weight[FX_N] = {0, 4, 4, 10, 9, 9, 7, 3, 12, 9, 5, 22};
#ifndef FX_BUDGET
#define FX_BUDGET 400
#endif
// Tempo divisions for DELAY TIME, AUTOPAN RATE and FILTER LFO RATE: in beats, shortest first
#define NDIV 12
static const float div_beats[NDIV] = {0.125f, 1 / 6.f, 0.25f, 1 / 3.f, 0.375f, 0.5f, 2 / 3.f, 0.75f, 1, 1.5f, 2, 4};
static const char *const div_label[NDIV] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/4D", "1/2", "1/1"};
static const char *const dist_mode[4] = {"SOFT", "HARD", "FOLD", "ASYM"}, *const filt_mode[4] = {"LOW", "HIGH", "BAND", "NOTCH"};
static const char *const pan_shape[3] = {"SINE", "TRI", "SQUARE"};
// MULTIBAND: crossovers and times from the knobs (the UI shows them, the DSP uses them)
static inline float mb_xlow(int v) { return 40.f * exp2f(v / 100.f * 5); }     // 40 .. 1280 Hz (23: 88 Hz)
static inline float mb_xhigh(int v) { return 600.f * exp2f(v / 100.f * 5); }   // 600 .. 19200 Hz (41: 2.5 kHz)
static inline float mb_tscale(int v) { return exp2f((v - 50) / 25.f); }        // 0.25 .. 4 x (attack 2 ms, release 60 ms)
static const float mb_thresh[3] = {-40.8f, -41.8f, -35.5f}, mb_makeup[3] = {10.3f, 5.7f, 10.3f};   // per band, dB
// MULTIBAND's second half (Song.mbx): gains in dB, -24..+24, 0 = unity (the sound before them, to the bit)
enum { MB_IN, MB_LOW, MB_MID, MB_HIGH, MB_OUT, MB_N };
static const char *const mb_name[MB_N] = {"IN", "LOW", "MID", "HIGH", "OUT"};
static inline float mb_lin(int db) { return db ? powf(10, db / 20.f) : 1.f; }
static inline void fx_range(int type, int k, int *lo, int *hi) {
    *lo = 0; *hi = 100;
    if ((type == FX_DIST || type == FX_FILTER) && k == 0) *hi = 3;
    if (type == FX_PHASER && k == 4) *hi = 2;
    if (type == FX_FILTER && k == 4) *hi = NDIV;          // 0 = LFO off, else a division
    if (type == FX_AUTOPAN && k == 0) *hi = NDIV - 1;
    if (type == FX_AUTOPAN && k == 2) *hi = 2;
    if (type == FX_RING && k == 1) { *lo = -50; *hi = 50; }
}
static const char *const rv_name[8] = {"SIZE", "DECAY", "DAMP", "PREDELAY", "WIDTH", "LOW CUT", "LEVEL", "MUTE"};
static const char *const dl_name[8] = {"TIME", "FEEDBACK", "TONE", "LOW CUT", "PING-PONG", "WIDTH", "LEVEL", "MUTE"};
static const char *const ms_name[12] = {"COMP", "THRESH", "RATIO", "ATTACK", "RELEASE", "MAKEUP", "LIMITER", "CEILING", "LIM REL", "DUCK BY", "DUCK AMT", "DUCK REL"};
static inline void rv_range(int k, int *lo, int *hi) { *lo = 0; *hi = k == RV_MUTE ? 1 : 100; }
static inline void dl_range(int k, int *lo, int *hi) { *lo = 0; *hi = k == DL_TIME ? NDIV - 1 : k == DL_PING || k == DL_MUTE ? 1 : k == DL_FB ? 95 : 100; }
static inline void ms_range(int k, int *lo, int *hi) { *lo = 0; *hi = k == MS_COMP || k == MS_LIM ? 1 : k == MS_DUCKSRC ? NSLOT : 100; }
static inline void fx_set_type(FxSlot *f, int type) {    // a fresh type gets its defaults; the type byte is written last
    for (int k = 0; k < 6; k++) f->p[k] = fx_def[type][k];
    f->on = type != FX_NONE; f->type = (uint8_t)type;
}
static inline void mix_defaults(Mixer *m) {
    memset(m, 0, sizeof *m);
    static const int8_t rv[8] = {50, 40, 50, 10, 100, 20, 80, 0}, dl[8] = {7, 35, 70, 20, 0, 100, 80, 0},
                        ms[12] = {0, 70, 20, 30, 40, 0, 1, 89, 30, 0, 0, 30};
    memcpy(m->rv, rv, 8); memcpy(m->dl, dl, 8); memcpy(m->ms, ms, 12); m->mark = MIX_INIT;
}
static inline void mix_sanitize(Mixer *m) {
    if (m->mark != MIX_INIT) { mix_defaults(m); return; }
    for (int s = 0; s < NSLOT; s++) {
        PadMix *p = &m->pad[s];
        if (p->send_a > 100) p->send_a = 100; if (p->send_b > 100) p->send_b = 100; p->solo = 0; p->flags = 0;
        for (int i = 0; i < NINS; i++) {
            FxSlot *f = &p->ins[i]; if (f->type >= FX_N) { f->type = FX_NONE; } if (f->on > 1) f->on = 1;
            for (int k = 0; k < 6; k++) { int lo, hi; fx_range(f->type, k, &lo, &hi); if (f->p[k] < lo) f->p[k] = (int8_t)lo; if (f->p[k] > hi) f->p[k] = (int8_t)hi; }
        }
    }
    for (int k = 0; k < 8; k++) { int lo, hi; rv_range(k, &lo, &hi); if (m->rv[k] < lo) m->rv[k] = lo; if (m->rv[k] > hi) m->rv[k] = hi;
                                  dl_range(k, &lo, &hi); if (m->dl[k] < lo) m->dl[k] = lo; if (m->dl[k] > hi) m->dl[k] = hi; }
    for (int k = 0; k < 12; k++) { int lo, hi; ms_range(k, &lo, &hi); if (m->ms[k] < lo) m->ms[k] = lo; if (m->ms[k] > hi) m->ms[k] = hi; }
}
