// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// The synth voice ACID plays (SynthV, the PolyBLEP oscillators, the saturating ladder), and what is left of GRAND.
// 2026-11-25 (user): WAVE (src/wave.h) took GRAND's place as Track.type 1; GRAND's DSP, DEEP page, PATCH bay and
// presets are gone. Its parameters stay here as names and ranges only: an old project's GRAND pad (Track.sp,
// Song.gx) is read once and becomes a WAVE sound (wv_from_grand in main.c).
// main.c includes it too: the guard keeps one copy when tests join both files.
#ifndef BP_SYNTH_H
#define BP_SYNTH_H
#include <math.h>
#include <string.h>
enum { SP_OSC1, SP_OSC2, SP_TUNE2, SP_DETUNE, SP_MIX, SP_NOISE, SP_CUTOFF, SP_RESO, SP_ENV, SP_ATTACK, SP_DECAY,
       SP_SUSTAIN, SP_RELEASE, SP_GLIDE, SP_N, SP_MARK = 15 };   // TUNE2 -24..24 semis, DETUNE 0..50 cents, ENV -100..100
#define SP_INIT 0x5A                     // sp[SP_MARK]: the parameters were set up once
// GRAND DEEP (was Y on GRAND's panel): Song.gx[slot][GX_*].
enum { GX_LRATE, GX_LSYNC, GX_LWAVE, GX_LPITCH, GX_LCUT, GX_LPWM, GX_O1OCT, GX_O2OCT, GX_SYNC, GX_HP, GX_KBT, GX_VCA,
       GX_SPRING, GX_ARP, GX_AMODE, GX_AOCT, GX_ARATE, GX_AGATE, GX_ATTEN, GX_N, GX_MARK = 31 };   // ATTEN -100..100 (PATCH)   // ARP on, UP DOWN ORDER UP/DOWN, octaves 1..3, a division, gate %   // SPRING: the spring reverb's MIX (0 off)   // LSYNC 0 FREE else a division (NDIV); LWAVE TRI SQR SAW RAMP S&H; O1OCT 32' 16' 8' 4'; O2OCT 16' 8' 4' 2'; KBT OFF HALF FULL; VCA ENV GATE
#define GX_INIT 0x5F
static const int8_t gx_def[32] = {50, 0, 0, 0, 0, 0, 2, 1, 0, 0, 1, 0, 0, 0, 0, 0, 2, 50, [GX_MARK] = GX_INIT};
static void gx_defaults(int8_t *gx) { memcpy(gx, gx_def, 32); }
static void gx_range(int k, int *lo, int *hi) {
    *lo = 0; *hi = 100;
    if (k == GX_LSYNC) *hi = 12; else if (k == GX_LWAVE) *hi = 4; else if (k == GX_O1OCT || k == GX_O2OCT) *hi = 3;
    else if (k == GX_SYNC || k == GX_ARP) *hi = 1; else if (k == GX_VCA) *hi = 2; /* VCA ENV GATE DRONE */ else if (k == GX_KBT || k == GX_AOCT) *hi = 2; else if (k == GX_AMODE) *hi = 3;
    else if (k == GX_ARATE) *hi = 11; else if (k == GX_AGATE) *lo = 10; else if (k == GX_ATTEN) *lo = -100;
}
static void sp_range(int k, int *lo, int *hi) {
    *lo = 0; *hi = 100;
    if (k == SP_OSC1 || k == SP_OSC2) *hi = 3;
    else if (k == SP_TUNE2) { *lo = -24; *hi = 24; } else if (k == SP_DETUNE) *hi = 50; else if (k == SP_ENV) *lo = -100;
}

typedef struct {
    double ph1, ph2;                     // oscillator phases 0..1
    float freq, target;                  // Hz now and where GLIDE goes
    float y[4], env;                     // ladder stages, envelope level
    int stage;                           // 0 attack, 1 decay, 2 sustain, 3 release
    int gate;                            // frames until the key is let go, -1 held (until release)
    float vel;
    unsigned rng;
    float pf, pv; int pg, pat;           // a legato note waiting: frequency, velocity, gate, frames until it (-1 none)
    float menv, accap, hp, tlp, bst;     // ACID: filter envelope, accent sweep, resonance high-pass, distortion tone, BASS
    int accent, slide, pacc, pslide;     // ACID: this note's accent and slide; the waiting note's
    // 2026-11-11: what costs a pow or an exp is worked out every 16 frames (knobs only: sy_ctl, acid_ctl) or every 8
    // with a straight line between (filter coefficient, LFO pitch), not on every frame: about half the CPU of a voice
    float th[4];                         // the saturated ladder stages, kept from frame to frame (2026-11-11: 5 saturations per pass, not 8; the same numbers)
    unsigned cn; float c_att, c_dec, c_rel, c_tatk, c_trel, c_hp, c_sl, c_adec, c_x[4], c_g, c_dg; double c_tune, c_r1, c_r2, c_pm, c_dpm;
} SynthV;

static inline float sy_tanh(float x) {   // a smooth saturator, close to tanh where it matters
    if (x > 3) return 1; if (x < -3) return -1;
    float x2 = x * x; return x * (27 + x2) / (27 + 9 * x2);
}
// One pass of the ladder: th[i] is always sy_tanh(y[i]), so each stage's old value is not worked out again.
#define SY_LADDER(v, IN, g) do { float in_ = sy_tanh(IN); \
    (v)->y[0] += (g) * (in_ - (v)->th[0]); (v)->th[0] = sy_tanh((v)->y[0]); \
    (v)->y[1] += (g) * ((v)->th[0] - (v)->th[1]); (v)->th[1] = sy_tanh((v)->y[1]); \
    (v)->y[2] += (g) * ((v)->th[1] - (v)->th[2]); (v)->th[2] = sy_tanh((v)->y[2]); \
    (v)->y[3] += (g) * ((v)->th[2] - (v)->th[3]); (v)->th[3] = sy_tanh((v)->y[3]); } while (0)
static inline float sy_blep(double t, double dt) {
    if (t < dt) { t /= dt; return (float)(t + t - t * t - 1); }
    if (t > 1 - dt) { t = (t - 1) / dt; return (float)(t * t + t + t + 1); }
    return 0;
}
static inline float sy_osc(int w, double ph, double dt) {
    switch (w & 3) {
    case 0: return (float)(ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph);                 // TRI
    case 1: return (float)(2 * ph - 1) - sy_blep(ph, dt);                        // SAW
    default: {                                                                     // SQR 50 %, PULSE 10 %
        double pw = (w & 3) == 2 ? 0.5 : 0.1, q = ph + (1 - pw); if (q >= 1) q -= 1;
        return (ph < pw ? 1.f : -1.f) + sy_blep(ph, dt) - sy_blep(q, dt);
    }
    }
}
static inline float sy_time(int x) { return 0.001f * powf(10000.f, x / 100.f); }   // 0..100 -> 1 ms .. 10 s (GRAND's times, for the conversion)

// GRAND's defaults (a warm bass-lead): an old GRAND pad that was never touched converts from these.
static void synth_defaults(int8_t *sp) {
    memset(sp, 0, 16);
    sp[SP_OSC1] = 1; sp[SP_OSC2] = 2; sp[SP_TUNE2] = -12; sp[SP_DETUNE] = 6; sp[SP_MIX] = 40; sp[SP_NOISE] = 0;
    sp[SP_CUTOFF] = 45; sp[SP_RESO] = 25; sp[SP_ENV] = 45; sp[SP_ATTACK] = 0; sp[SP_DECAY] = 62; sp[SP_SUSTAIN] = 50;
    sp[SP_RELEASE] = 50; sp[SP_GLIDE] = 0; sp[SP_MARK] = SP_INIT;
}

#endif
