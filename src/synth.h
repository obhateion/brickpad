// GRAND: a two-oscillator mono/poly synth voice in the manner of a semi-modular classic, included by
// engine.c only. OSC 1 and 2 (TRI SAW SQR PULSE, band-limited with PolyBLEP) -> MIXER (+ white noise) ->
// 4-pole ladder low-pass (24 dB, resonance, tanh stages, 2x oversampled) -> VCA. One ADSR drives both the
// filter (ENV amount, either sign) and the VCA, as on the original. GLIDE works in GATE (mono) voice mode.
// Parameters live in Track.sp[] (see SP_*), 0..100 unless noted. Runs in the audio callback: no allocation.
// main.c includes it too (names, ranges, defaults): the guard keeps one copy when tests join both files.
#ifndef BP_SYNTH_H
#define BP_SYNTH_H
#include <math.h>
#include <string.h>
enum { SP_OSC1, SP_OSC2, SP_TUNE2, SP_DETUNE, SP_MIX, SP_NOISE, SP_CUTOFF, SP_RESO, SP_ENV, SP_ATTACK, SP_DECAY,
       SP_SUSTAIN, SP_RELEASE, SP_GLIDE, SP_N, SP_MARK = 15 };   // TUNE2 -24..24 semis, DETUNE 0..50 cents, ENV -100..100
#define SP_INIT 0x5A                     // sp[SP_MARK]: the parameters were set up once
// GRAND DEEP (Y on the panel): Song.gx[slot][GX_*]. Defaults give the panel's sound exactly.
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
static inline float gx_lfo_hz(int v) { return 0.05f * powf(1000.f, v / 100.f); }   // 0.05 .. 50 Hz
static inline int gx_lfo_on(const int8_t *gx) { return gx && gx[GX_MARK] == GX_INIT && (gx[GX_LPITCH] || gx[GX_LCUT] || gx[GX_LPWM]); }
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
    float src[16];                       // GRAND PATCH: last sample of each audio output (a cable is one sample late)
    float lri;                           // PATCH: what feeds LFO RATE (the pad LFO reads the newest voice per buffer)
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
// Ladder cutoff in Hz: the knob, the envelope (+-6 octaves) and half key tracking. The panel draws it too.
static inline float sy_cutoff(const int8_t *sp, float env, float freq) {
    float oct = (sp[SP_CUTOFF] / 100.f) * 9.8f + sp[SP_ENV] / 100.f * 6.f * env + 0.5f * log2f(freq / 261.6f);
    float fc = 20.f * exp2f(oct); return fc > SR * 0.45f ? SR * 0.45f : fc;
}
static inline float sy_cutoff_kb(const int8_t *sp, float env, float freq, float kb, float oct_add) {   // KB TRACK and the LFO
    float oct = (sp[SP_CUTOFF] / 100.f) * 9.8f + sp[SP_ENV] / 100.f * 6.f * env + kb * log2f(freq / 261.6f) + oct_add;
    float fc = 20.f * exp2f(oct); return fc > SR * 0.45f ? SR * 0.45f : fc < 10 ? 10 : fc;
}
static inline float sy_pulse(double ph, double dt, double pw) {   // a pulse of any width (PWM)
    double q = ph + (1 - pw); if (q >= 1) q -= 1;
    return (ph < pw ? 1.f : -1.f) + sy_blep(ph, dt) - sy_blep(q, dt);
}
static inline float sy_time(int x) { return 0.001f * powf(10000.f, x / 100.f); }   // 0..100 -> 1 ms .. 10 s

// PATCH (GRAND DEEP page 3): the jacks. gp[c][0] an output, gp[c][1] an input; 0 = no cable.
enum { JO_NONE, JO_LFO, JO_SH, JO_OSC1, JO_OSC2, JO_NOISE, JO_MIX, JO_FILT, JO_ENV, JO_VCA, JO_KBP, JO_GATE, JO_VEL, JO_ATT, JO_N };
enum { JI_NONE, JI_P1, JI_P2, JI_PWM, JI_FM, JI_M1, JI_M2, JI_MN, JI_FIN, JI_CUT, JI_RES, JI_VIN, JI_VAMT, JI_ATT, JI_LRATE, JI_N };   // LFO RATE: appended 2026-11-10 (the numbers are saved)
#define NCABLE 8
static inline int gp_any(const uint8_t (*gp)[2]) { for (int c = 0; c < NCABLE; c++) if (gp[c][0] && gp[c][1]) return 1; return 0; }
static inline int gp_uses(const uint8_t (*gp)[2], int out) { for (int c = 0; c < NCABLE; c++) if (gp[c][0] == out && gp[c][1]) return 1; return 0; }
// Defaults that sound at once: a warm bass-lead.
static void synth_defaults(int8_t *sp) {
    memset(sp, 0, 16);
    sp[SP_OSC1] = 1; sp[SP_OSC2] = 2; sp[SP_TUNE2] = -12; sp[SP_DETUNE] = 6; sp[SP_MIX] = 40; sp[SP_NOISE] = 0;
    sp[SP_CUTOFF] = 45; sp[SP_RESO] = 25; sp[SP_ENV] = 45; sp[SP_ATTACK] = 0; sp[SP_DECAY] = 62; sp[SP_SUSTAIN] = 50;
    sp[SP_RELEASE] = 50; sp[SP_GLIDE] = 0; sp[SP_MARK] = SP_INIT;
}

static void synth_start(SynthV *v, float freq, float vel, int gate, int legato) {
    if (!legato) {
        memset(v, 0, sizeof *v);
        v->rng = 0x9E3779B9u ^ (unsigned)(freq * 1000);
        v->freq = freq; v->pat = -1;
    }
    v->target = freq; v->vel = vel; v->gate = gate; v->stage = 0;   // the envelope restarts from where it is: no click
}

static inline void sy_ctl(SynthV *v, const int8_t *sp, const int8_t *gx) {   // GRAND: the values that follow knobs only
    v->c_att = 1.f / (sy_time(sp[SP_ATTACK]) * SR);
    v->c_dec = (1 - expf(-1.f / (sy_time(sp[SP_DECAY]) * SR * 0.3f)));
    v->c_rel = (1 - expf(-1.f / (sy_time(sp[SP_RELEASE]) * SR * 0.3f)));
    v->c_tatk = (1 - expf(-1.f / (0.002f * SR))); v->c_trel = (1 - expf(-1.f / (sy_time(sp[SP_RELEASE]) * 0.3f * SR)));
    v->c_tune = pow(2.0, (sp[SP_TUNE2] + sp[SP_DETUNE] / 100.0) / 12.0);
    v->c_r1 = gx[GX_O1OCT] == 2 ? 1 : exp2(gx[GX_O1OCT] - 2); v->c_r2 = gx[GX_O2OCT] == 1 ? 1 : exp2(gx[GX_O2OCT] - 1);
    v->c_x[0] = sp[SP_SUSTAIN] / 100.f; v->c_x[1] = sp[SP_MIX] / 100.f; v->c_x[2] = sp[SP_NOISE] / 100.f; v->c_x[3] = sp[SP_RESO] / 100.f * 4.2f;   // divisions, once per 16 frames
    v->c_hp = gx[GX_HP] ? 1 - expf(-2 * (float)M_PI * 20.f * powf(100.f, gx[GX_HP] / 100.f) / SR) : 0;
}
// One output frame; returns 0 once the release has died away.
static inline int synth_frame(SynthV *v, const int8_t *sp, const int8_t *gx, float lfo, float *out) {
    if (!gx) gx = gx_def;                // the caller checked the mark once per buffer
    unsigned cn = v->cn++; if ((cn & 15) == 0) sy_ctl(v, sp, gx);
    if (v->pat >= 0 && v->pat-- == 0) synth_start(v, v->pf, v->pv, v->pg, 1);   // the next note of a mono line
    // envelope
    if (v->gate > 0 && --v->gate == 0) v->stage = 3;
    float sus = v->c_x[0];
    switch (v->stage) {
    case 0: v->env += v->c_att; if (v->env >= 1) { v->env = 1; v->stage = 1; } break;
    case 1: v->env += (sus - v->env) * v->c_dec; if (fabsf(v->env - sus) < 1e-4f) v->stage = 2; break;
    case 2: v->env = sus; break;
    default: v->env -= v->env * v->c_rel; if (v->env < 1e-4f && (!gx[GX_VCA] || v->tlp < 1e-4f)) return 0;
    }
    if (gx[GX_VCA]) {                    // VCA GATE: full while the key is held, the envelope's release after (2 ms in)
        float tg = v->stage < 3 || (gx[GX_VCA] == 2 && v->stage == 3) ? 1.f : 0.f;
        v->tlp += (tg - v->tlp) * (tg > v->tlp ? v->c_tatk : v->c_trel);
    }
    // glide: exponential towards the target pitch
    if (v->freq != v->target) {
        float gt = sp[SP_GLIDE] / 100.f; gt = gt * gt * 2.f;              // up to 2 s
        if (gt <= 0.0005f) v->freq = v->target;
        else { v->freq *= powf(v->target / v->freq, 1 - expf(-1.f / (gt * SR * 0.3f))); if (fabsf(v->freq - v->target) < 0.01f) v->freq = v->target; }
    }
    if ((cn & 7) == 0) {                 // the LFO's pitch: where it is heading, every 8 frames
        double pt = lfo != 0 && gx[GX_LPITCH] ? exp2(lfo * gx[GX_LPITCH] / 100.0) : 1;
        if (!cn) { v->c_pm = pt; v->c_dpm = 0; } else v->c_dpm = (pt - v->c_pm) / 8;
    }
    v->c_pm += v->c_dpm;
    double pm = v->c_pm;   // MODULATION PITCH AMT: up to +-1 octave
    double r1 = v->c_r1, r2 = v->c_r2;   // RANGE: 8' is the note
    double f1 = v->freq * r1 * pm, f2 = v->freq * r2 * pm * v->c_tune;
    double dt1 = f1 * (1.0 / SR), dt2 = f2 * (1.0 / SR);
    float pwm = lfo != 0 ? lfo * gx[GX_LPWM] / 100.f * 0.4f : 0, o1, o2;   // PWM moves the width of SQR and PULSE
    if (pwm != 0 && sp[SP_OSC1] >= 2) { double w = (sp[SP_OSC1] == 2 ? 0.5 : 0.1) + pwm; o1 = sy_pulse(v->ph1, dt1, w < 0.05 ? 0.05 : w > 0.95 ? 0.95 : w); } else o1 = sy_osc(sp[SP_OSC1], v->ph1, dt1);
    if (pwm != 0 && sp[SP_OSC2] >= 2) { double w = (sp[SP_OSC2] == 2 ? 0.5 : 0.1) + pwm; o2 = sy_pulse(v->ph2, dt2, w < 0.05 ? 0.05 : w > 0.95 ? 0.95 : w); } else o2 = sy_osc(sp[SP_OSC2], v->ph2, dt2);
    int wrap = 0; v->ph1 += dt1; if (v->ph1 >= 1) { v->ph1 -= 1; wrap = 1; }
    if (wrap && gx[GX_SYNC]) v->ph2 = v->ph1 * (dt2 / dt1);   // HARD SYNC: OSC 2 starts again with OSC 1
    else { v->ph2 += dt2; if (v->ph2 >= 1) v->ph2 -= 1; }
    v->rng = v->rng * 1664525u + 1013904223u;
    float noise = ((int)(v->rng >> 8) - 8388608) / 8388608.f;
    float m = v->c_x[1], x = (1 - m) * o1 + m * o2 + noise * v->c_x[2];
    // ladder: cutoff from the knob, the envelope (+-6 octaves) and half key tracking
    float kb = gx[GX_KBT] == 1 ? 0.5f : gx[GX_KBT] == 2 ? 1.f : 0.f, lc = lfo != 0 ? lfo * gx[GX_LCUT] / 100.f * 3.f : 0;   // KB TRACK, CUTOFF AMT (+-3 oct)
    if ((cn & 7) == 0) {                 // the filter coefficient likewise
        float fc = kb == 0.5f && lc == 0 ? sy_cutoff(sp, v->env, v->freq) : sy_cutoff_kb(sp, v->env, v->freq, kb, lc);
        float gt = 1 - expf(-2 * (float)M_PI * fc / (2 * SR));
        if (!cn) { v->c_g = gt; v->c_dg = 0; } else v->c_dg = (gt - v->c_g) / 8;
    }
    v->c_g += v->c_dg;
    float g = v->c_g, k = v->c_x[3];
    x *= 0.8f * (1 + k * 0.3f);                                          // keeps the level as the resonance rises
    for (int os = 0; os < 2; os++) {
        SY_LADDER(v, x - k * v->y[3], g);
    }
    float y = v->y[3];
    if (gx[GX_HP]) { float a = v->c_hp; v->hp += (y - v->hp) * a; y -= v->hp; }   // HP: 20 Hz .. 2 kHz, 6 dB
    *out = y * (gx[GX_VCA] ? v->tlp : v->env) * v->vel;
    return 1;
}
// One frame of a patched GRAND voice. lfo/sh: the pad's LFO and S&H now. Audio outputs feed inputs one sample late.
static inline int synth_frame_patch(SynthV *v, const int8_t *sp, const int8_t *gx, const uint8_t (*gp)[2], float lfo, float sh, float *out) {
    if (!gx || gx[GX_MARK] != GX_INIT) gx = gx_def;
    { unsigned cn = v->cn++; if ((cn & 15) == 0) sy_ctl(v, sp, gx); }
    if (v->pat >= 0 && v->pat-- == 0) synth_start(v, v->pf, v->pv, v->pg, 1);
    if (v->gate > 0 && --v->gate == 0) v->stage = 3;
    float sus = v->c_x[0];
    switch (v->stage) {
    case 0: v->env += v->c_att; if (v->env >= 1) { v->env = 1; v->stage = 1; } break;
    case 1: v->env += (sus - v->env) * v->c_dec; if (fabsf(v->env - sus) < 1e-4f) v->stage = 2; break;
    case 2: v->env = sus; break;
    default: v->env -= v->env * v->c_rel; if (v->env < 1e-4f && (!gx[GX_VCA] || v->tlp < 1e-4f)) return 0;
    }
    if (gx[GX_VCA]) { float tg = v->stage < 3 || (gx[GX_VCA] == 2 && v->stage == 3) ? 1.f : 0.f; v->tlp += (tg - v->tlp) * (tg > v->tlp ? v->c_tatk : v->c_trel); }
    if (v->freq != v->target) {
        float gt = sp[SP_GLIDE] / 100.f; gt = gt * gt * 2.f;
        if (gt <= 0.0005f) v->freq = v->target;
        else { v->freq *= powf(v->target / v->freq, 1 - expf(-1.f / (gt * SR * 0.3f))); if (fabsf(v->freq - v->target) < 0.01f) v->freq = v->target; }
    }
    // the outputs known before this frame: control ones now, audio ones from the last frame
    float o[JO_N]; memcpy(o, v->src, sizeof o);
    o[JO_LFO] = lfo; o[JO_SH] = sh; o[JO_ENV] = v->env; o[JO_KBP] = log2f(v->freq / 130.81f); o[JO_GATE] = v->stage < 3; o[JO_VEL] = v->vel;
    int sr[JI_N] = {0}, att_from = 0;   // which output feeds each input; read when it is used: forward cables get this frame, backward ones the last
    for (int c = 0; c < NCABLE; c++) { int a = gp[c][0], b = gp[c][1]; if (!a || !b || a >= JO_N || b >= JI_N) continue; if (b == JI_ATT) att_from = a; }
    o[JO_ATT] = (att_from ? (att_from == JO_ATT ? v->src[JO_ATT] : o[att_from]) : 1.f) * gx[GX_ATTEN] / 100.f;   // ATTEN: its input (normalled to +1) times the knob
    for (int c = 0; c < NCABLE; c++) { int a = gp[c][0], b = gp[c][1]; if (!a || !b || a >= JO_N || b >= JI_N) continue; sr[b] = a; }
#define has(i) (sr[i] != 0)
#define IN(i) (o[sr[i]])
    double pm = lfo != 0 && gx[GX_LPITCH] ? exp2(lfo * gx[GX_LPITCH] / 100.0) : 1;
    double r1 = exp2(gx[GX_O1OCT] - 2), r2 = exp2(gx[GX_O2OCT] - 1);
    double f1 = v->freq * r1 * pm * (has(JI_P1) ? exp2(IN(JI_P1)) : 1);                      // OSC PITCH: 1.0 = an octave
    double f2 = v->freq * r2 * pm * v->c_tune * (has(JI_P2) ? exp2(IN(JI_P2)) : 1);
    if (has(JI_FM)) f2 *= 1 + 1.5 * IN(JI_FM);                                                // LIN FM (through zero)
    double dt1 = f1 * (1.0 / SR), dt2 = f2 * (1.0 / SR); if (dt1 > 0.45) dt1 = 0.45; if (dt2 > 0.45) dt2 = 0.45; if (dt2 < -0.45) dt2 = -0.45;
    float pwm = (lfo != 0 ? lfo * gx[GX_LPWM] / 100.f * 0.4f : 0) + (has(JI_PWM) ? IN(JI_PWM) * 0.45f : 0), o1, o2;
    if (pwm != 0 && sp[SP_OSC1] >= 2) { double w = (sp[SP_OSC1] == 2 ? 0.5 : 0.1) + pwm; o1 = sy_pulse(v->ph1, dt1, w < 0.05 ? 0.05 : w > 0.95 ? 0.95 : w); } else o1 = sy_osc(sp[SP_OSC1], v->ph1, dt1);
    if (pwm != 0 && sp[SP_OSC2] >= 2) { double w = (sp[SP_OSC2] == 2 ? 0.5 : 0.1) + pwm; o2 = sy_pulse(v->ph2, fabs(dt2), w < 0.05 ? 0.05 : w > 0.95 ? 0.95 : w); } else o2 = sy_osc(sp[SP_OSC2], v->ph2, fabs(dt2));
    int wrap = 0; v->ph1 += dt1; if (v->ph1 >= 1) { v->ph1 -= 1; wrap = 1; }
    if (wrap && gx[GX_SYNC]) v->ph2 = v->ph1 * (fabs(dt2) / dt1);
    else { v->ph2 += dt2; if (v->ph2 >= 1) v->ph2 -= 1; if (v->ph2 < 0) v->ph2 += 1; }
    v->rng = v->rng * 1664525u + 1013904223u;
    float noise = ((int)(v->rng >> 8) - 8388608) / 8388608.f;
    o[JO_OSC1] = o1; o[JO_OSC2] = o2; o[JO_NOISE] = noise;
    float m = sp[SP_MIX] / 100.f, a1 = has(JI_M1) ? IN(JI_M1) : o1, a2 = has(JI_M2) ? IN(JI_M2) : o2, an = has(JI_MN) ? IN(JI_MN) : noise;   // MIXER inputs, normalled
    float mixo = (1 - m) * a1 + m * a2 + an * sp[SP_NOISE] / 100.f;
    o[JO_MIX] = mixo;
    float x = has(JI_FIN) ? IN(JI_FIN) : mixo;                                                // FILTER IN, normalled from MIX
    float kb = gx[GX_KBT] == 1 ? 0.5f : gx[GX_KBT] == 2 ? 1.f : 0.f, lc = (lfo != 0 ? lfo * gx[GX_LCUT] / 100.f * 3.f : 0) + (has(JI_CUT) ? IN(JI_CUT) * 4.f : 0);
    float fc = sy_cutoff_kb(sp, v->env, v->freq, kb, lc);
    float g = 1 - expf(-2 * (float)M_PI * fc / (2 * SR)), k = sp[SP_RESO] / 100.f * 4.2f + (has(JI_RES) ? IN(JI_RES) * 4.2f : 0);
    if (k < 0) k = 0; if (k > 4.4f) k = 4.4f;
    x *= 0.8f * (1 + k * 0.3f);
    for (int os = 0; os < 2; os++) {
        SY_LADDER(v, x - k * v->y[3], g);
    }
    float y = v->y[3];
    if (gx[GX_HP]) { float a = v->c_hp; v->hp += (y - v->hp) * a; y -= v->hp; }
    o[JO_FILT] = y;
    float vin = has(JI_VIN) ? IN(JI_VIN) : y, va = has(JI_VAMT) ? IN(JI_VAMT) : 0, amt = has(JI_VAMT) ? (va < 0 ? 0 : va > 1.5f ? 1.5f : va) : gx[GX_VCA] ? v->tlp : v->env;
    float vo = vin * amt;
    v->src[JO_OSC1] = o1; v->src[JO_OSC2] = o2; v->src[JO_NOISE] = noise; v->src[JO_MIX] = mixo; v->src[JO_FILT] = y; v->src[JO_VCA] = vo; v->src[JO_ATT] = o[JO_ATT]; v->lri = has(JI_LRATE) ? IN(JI_LRATE) : 0;   // LFO RATE: the pad LFO reads it per buffer
    if (!isfinite(vo)) { memset(v->y, 0, sizeof v->y); memset(v->th, 0, sizeof v->th); vo = 0; }
    *out = vo * v->vel;
    return 1;
#undef has
#undef IN
}
#endif
