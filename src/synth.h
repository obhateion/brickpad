// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// The synth voice ACID plays (SynthV, the PolyBLEP oscillators, the saturating ladder).
// 2026-11-25 (user): WAVE (src/wave.h) took GRAND's place as Track.type 1; GRAND is gone, old projects do not open.
// main.c includes it too: the guard keeps one copy when tests join both files.
#ifndef BP_SYNTH_H
#define BP_SYNTH_H
#include <math.h>
#include <string.h>
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
    float dcx, dcy;                      // ACID: the DC blocker after the distortion (2026-11-25)
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
#endif
