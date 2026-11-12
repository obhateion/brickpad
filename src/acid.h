// ACID: a one-oscillator bass line voice in the manner of the classic 1981 bass machine, with a built-in
// distortion. Included by engine.c and main.c (names, ranges, defaults); the guard keeps one copy.
// OSC (SAW or SQUARE, PolyBLEP) -> 4-pole ladder with a high-pass in its resonance path (thin, squelchy) -> VCA
// (gate held, short release) -> DISTORTION (DIODE, TRANSISTOR, TUBE, FUZZ; DRIVE, TONE, MIX). The filter has its own
// decay envelope (DECAY); an accented step is louder, its envelope is short and opens the filter more, and
// accents in a row build up a sweep (the "wow"). A step with SLIDE glides 60 ms into the next one without a new
// attack. Parameters live in Track.ap[] (AP_*), 0..100 unless noted. Runs in the audio callback: no allocation.
#ifndef BP_ACID_H
#define BP_ACID_H
#include <math.h>
#include <string.h>
enum { AP_WAVE, AP_TUNE, AP_CUTOFF, AP_RESO, AP_ENVMOD, AP_DECAY, AP_ACCENT, AP_DTYPE, AP_DRIVE, AP_DTONE, AP_DMIX,
       AP_N, AP_MARK = 11 };   // WAVE 0 SAW 1 SQUARE, TUNE -12..12 semis, DTYPE 0 DIODE 1 TRANSISTOR 2 TUBE 3 FUZZ
#define AP_INIT 0x5B
// ACID DEEP (Y on the panel): Song.ax[slot][AX_*], 0..100 unless noted. At their defaults the voice is exactly as
// before: times and frequencies are 2^((v - 50) / 50 * span) around the old fixed values, the additions start at 0.
enum { AX_SUB, AX_SUBOCT, AX_SUBWAVE, AX_PW, AX_NOISE, AX_VATK, AX_VREL, AX_SLIDE, AX_RANGE, AX_TRACK, AX_RESHP, AX_BASS,
       AX_ACCATK, AX_ACCDEC, AX_CLIP, AX_N, AX_MARK = 15 };   // SUBOCT 0 -1 oct, 1 -2 oct; SUBWAVE 0 SQUARE 1 SINE 2 TRIANGLE
#define AX_INIT 0x5E
static const int8_t ax_def[16] = {0, 0, 0, 50, 0, 50, 50, 50, 50, 0, 50, 0, 50, 50, 0, AX_INIT};
static void ax_defaults(int8_t *ax) { memcpy(ax, ax_def, 16); }
static void ax_range(int k, int *lo, int *hi) { *lo = 0; *hi = k == AX_SUBOCT ? 1 : k == AX_SUBWAVE ? 2 : 100; }
static inline float ax_exp(const int8_t *ax, int k, float span) { return exp2f((ax[k] - 50) / 50.f * span); }
static inline float ax_vatk_s(const int8_t *ax) { return 0.003f * ax_exp(ax, AX_VATK, 3); }     // 0.4 .. 24 ms
static inline float ax_vrel_s(const int8_t *ax) { return 0.008f * ax_exp(ax, AX_VREL, 4); }     // 0.5 .. 128 ms
static inline float ax_slide_s(const int8_t *ax) { return 0.02f * ax_exp(ax, AX_SLIDE, 2); }    // time constant; ~3x is heard
static inline float ax_accatk_s(const int8_t *ax) { return 0.06f * ax_exp(ax, AX_ACCATK, 2); }  // how fast the sweep builds
static inline float ax_accdec_s(const int8_t *ax) { return 0.2f * ax_exp(ax, AX_ACCDEC, 2); }   // an accent's filter decay
static inline float ax_reshp_hz(const int8_t *ax) { return 150.f * ax_exp(ax, AX_RESHP, 2.5f); } // 26 .. 850 Hz
#define ACID_LEVEL 0.38f         // output level: peaks like GRAND at the defaults
#define ACID_ACCENT 1                    // Pattern.slice of an ACID step: these bits (-1 = none)
#define ACID_SLIDE 2
static void ap_range(int k, int *lo, int *hi) {
    *lo = 0; *hi = 100;
    if (k == AP_WAVE) *hi = 1; else if (k == AP_TUNE) { *lo = -12; *hi = 12; } else if (k == AP_DTYPE) *hi = 3;
}
static void acid_defaults(int8_t *ap) {
    memset(ap, 0, 12);
    ap[AP_WAVE] = 0; ap[AP_TUNE] = 0; ap[AP_CUTOFF] = 30; ap[AP_RESO] = 70; ap[AP_ENVMOD] = 60; ap[AP_DECAY] = 40;
    ap[AP_ACCENT] = 60; ap[AP_DTYPE] = 0; ap[AP_DRIVE] = 0; ap[AP_DTONE] = 70; ap[AP_DMIX] = 100; ap[AP_MARK] = AP_INIT;
}
static inline float acid_shape(int type, float x) {   // the distortion's transfer curve, about unity for small x
    switch (type & 3) {
    case 0: return x >= 0 ? 1 - expf(-x) : -(1 - expf(1.6f * x)) / 1.6f;        // DIODE: soft, asymmetric
    case 1: return x / sqrtf(sqrtf(1 + x * x * x * x));                          // TRANSISTOR: a harder knee
    case 2: return tanhf(x + 0.35f) - tanhf(0.35f);                              // TUBE: biased, even harmonics
    default: { float a = fabsf(x); return copysignf(1 - expf(-3 * a), x) * (a > 0.02f ? 1 : a / 0.02f); }   // FUZZ
    }
}
static inline float acid_cutoff(const int8_t *ap, const int8_t *ax, float menv, float accap) {   // Hz; the panel draws it too
    float range = ax && ax[AX_MARK] == AX_INIT ? (ax[AX_RANGE] - 50) / 50.f * 1.5f : 0;   // ACID DEEP RANGE: +-1.5 octaves
    float oct = range + 4.9f + ap[AP_CUTOFF] / 100.f * 8.5f + ap[AP_ENVMOD] / 100.f * 5.f * menv * (1 + 0.6f * ap[AP_ACCENT] / 100.f * (accap > 0.01f))
              + ap[AP_ACCENT] / 100.f * 3.f * accap;   // an accent opens it further, and its sweep builds up
    float fc = exp2f(oct); return fc > 20000 ? 20000 : fc;
}

// Start a note on a voice. legato: the previous step slid into this one (no new attack, a 60 ms glide).
static void acid_start(SynthV *v, float freq, float vel, int gate, int accent, int legato) {
    if (!legato) { v->freq = freq; v->menv = 1; v->stage = 0; }   // a new attack from the level it is at; filter and phase go on: no click
    v->target = freq; v->vel = vel; v->gate = gate; v->accent = accent; v->slide = legato;
}
static inline void acid_ctl(SynthV *v, const int8_t *ap, const int8_t *ax) {   // the values that follow knobs only (every 16 frames)
    v->c_att = 1.f / (ax_vatk_s(ax) * SR); v->c_rel = (1 - expf(-1.f / (ax_vrel_s(ax) * SR)));
    v->c_dec = expf(-1.f / ((0.2f + ap[AP_DECAY] / 100.f * 2.f) * 0.35f * SR)); v->c_adec = expf(-1.f / (ax_accdec_s(ax) * 0.35f * SR));
    v->c_tatk = (1 - expf(-1.f / (ax_accatk_s(ax) * SR))); v->c_sl = 1 - expf(-1.f / (ax_slide_s(ax) * SR));
    v->c_tune = pow(2.0, ap[AP_TUNE] / 12.0);
    v->c_hp = 1 - expf(-2 * (float)M_PI * ax_reshp_hz(ax) / (2 * SR));
    v->c_x[0] = 1 - expf(-2 * (float)M_PI * 120.f / SR); v->c_x[1] = 1 + ax[AX_CLIP] / 100.f * 5; v->c_x[2] = 1 / sqrtf(v->c_x[1]);
    v->c_x[3] = 1 - expf(-2 * (float)M_PI * (800.f * exp2f(ap[AP_DTONE] / 100.f * 4.5f)) / SR);
}
static inline int acid_frame(SynthV *v, const int8_t *ap, const int8_t *ax, float *out) {
    if (!ax || ax[AX_MARK] != AX_INIT) ax = ax_def;
    unsigned cn = v->cn++; if ((cn & 15) == 0) acid_ctl(v, ap, ax);
    if (v->pat >= 0 && v->pat-- == 0) acid_start(v, v->pf, v->pv, v->pg, v->pacc, v->pslide);   // a note waiting for its frame
    if (v->gate > 0 && --v->gate == 0) v->stage = 3;
    // VCA: 3 ms attack, held with the gate, 8 ms release
    if (v->stage == 0) { v->env += v->c_att; if (v->env >= 1) { v->env = 1; v->stage = 2; } }
    else if (v->stage == 3) { v->env -= v->env * v->c_rel; if (v->env < 1e-4f) return 0; }
    // filter envelope: DECAY 200 ms .. 2.2 s; an accented note: 200 ms and the sweep builds up
    v->menv *= v->accent ? v->c_adec : v->c_dec;
    float target = v->accent ? v->menv : 0;
    v->accap += (target - v->accap) * v->c_tatk;
    // slide: 60 ms towards the next pitch
    if (v->freq != v->target) {
        v->freq *= powf(v->target / v->freq, v->c_sl);
        if (fabsf(v->freq - v->target) < 0.01f) v->freq = v->target;
    }
    double f = v->freq * v->c_tune, dt = f * (1.0 / SR);
    float o;
    if (ap[AP_WAVE] && ax[AX_PW] != 50) {   // PULSE WIDTH 10..90 %, the DC taken off (at 50 exactly the old square)
        double pw = 0.5 + (ax[AX_PW] - 50) / 100.0 * 0.8, q = v->ph1 + (1 - pw); if (q >= 1) q -= 1;
        o = (v->ph1 < pw ? 1.f : -1.f) + sy_blep(v->ph1, dt) - sy_blep(q, dt) + (float)(1 - 2 * pw);
    } else o = sy_osc(ap[AP_WAVE] ? 2 : 1, v->ph1, dt);
    if (ax[AX_SUB] > 0) {                   // SUB: one or two octaves down
        double sdt = dt / (ax[AX_SUBOCT] ? 4 : 2); float s;
        s = ax[AX_SUBWAVE] == 1 ? sinf(2 * (float)M_PI * (float)v->ph2) : sy_osc(ax[AX_SUBWAVE] == 2 ? 0 : 2, v->ph2, sdt);
        o += s * ax[AX_SUB] / 100.f * 0.8f; v->ph2 += sdt; if (v->ph2 >= 1) v->ph2 -= 1;
    }
    if (ax[AX_NOISE] > 0) { v->rng = v->rng * 1664525u + 1013904223u; o += ((int)(v->rng >> 9 & 0xffff) / 32768.f - 1) * ax[AX_NOISE] / 100.f * 0.6f; }
    v->ph1 += dt; if (v->ph1 >= 1) v->ph1 -= 1;
    if ((cn & 7) == 0) {                 // the filter coefficient: where it is heading every 8 frames, a straight line between
        float fc = acid_cutoff(ap, ax, v->menv, v->accap);
        if (ax[AX_TRACK] > 0) fc *= powf(v->freq / 110.f, ax[AX_TRACK] / 100.f);   // KEY TRACK: around A2
        if (fc > SR * 0.45f) fc = SR * 0.45f; if (fc < 20) fc = 20;
        float gt = 1 - expf(-2 * (float)M_PI * fc / (2 * SR));
        if (!cn) { v->c_g = gt; v->c_dg = 0; } else v->c_dg = (gt - v->c_g) / 8;
    }
    v->c_g += v->c_dg;
    float g = v->c_g, k = ap[AP_RESO] / 100.f * 4.4f;
    float hpa = v->c_hp;   // high-pass in the resonance path
    float x = o * 0.75f;
    for (int os = 0; os < 2; os++) {
        v->hp += (v->y[3] - v->hp) * hpa;
        SY_LADDER(v, x - k * (v->y[3] - v->hp), g);
    }
    float y = v->y[3] * (1.4f + 0.5f * k) * v->env * v->vel * (v->accent ? 1 + ap[AP_ACCENT] / 100.f * 0.7f : 1);
    if (ax[AX_BASS] > 0) { float ba = v->c_x[0]; v->bst += (y - v->bst) * ba; y += v->bst * ax[AX_BASS] / 100.f * 1.5f; }   // BASS boost
    if (ax[AX_CLIP] > 0) { y = sy_tanh(y * v->c_x[1]) * v->c_x[2]; }   // CLIP: the output stage driven
    // distortion: DRIVE into the curve, TONE a low-pass after it, MIX dry/wet
    if (ap[AP_DRIVE] > 0 && ap[AP_DMIX] > 0) {
        float dg = 1 + ap[AP_DRIVE] / 100.f * 30.f, w = acid_shape(ap[AP_DTYPE], y * dg) * 0.7f;
        float ta = v->c_x[3];
        v->tlp += (w - v->tlp) * ta;
        float m = ap[AP_DMIX] / 100.f; y = y * (1 - m) + v->tlp * m;
    }
    *out = y * ACID_LEVEL;   // 2026-11-09: was ~3x louder than GRAND and ran into the master limiter (crackle); after the distortion, so its sound stays
    return 1;
}
#endif
