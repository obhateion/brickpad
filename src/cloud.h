// CLOUD: a granular texture voice for backgrounds, kept simple on purpose (six knobs, no envelope to set).
// Included by engine.c and main.c (names, ranges, defaults); the guard keeps one copy.
// The pad's sample is read in short Hann-windowed grains: POSITION where, SPRAY how scattered (place and stereo),
// SIZE how long (20 ms .. 1 s), DENSITY how many (1 .. 60 a second), SHIMMER how many go an octave up or down or a
// fifth up (always consonant), MOTION how fast the cloud travels through the sample, in bars of the project's
// tempo (0 = still). Pitch follows the played degree like the sampler: the sample's root is moved to the project
// key. A cloud fades in over 0.4 s and out over 1.2 s. Parameters live in Track.cp[] (CP_*), 0..100.
#ifndef BP_CLOUD_H
#define BP_CLOUD_H
#include <math.h>
#include <string.h>
enum { CP_POS, CP_SPRAY, CP_SIZE, CP_DENSITY, CP_SHIMMER, CP_MOTION, CP_N, CP_MARK = 6 };
#define CP_INIT 0x5C
#define CL_GRAINS 16
static void cl_defaults(int8_t *cp) {
    memset(cp, 0, 7);
    cp[CP_POS] = 30; cp[CP_SPRAY] = 25; cp[CP_SIZE] = 55; cp[CP_DENSITY] = 50; cp[CP_SHIMMER] = 20; cp[CP_MOTION] = 30;
    cp[CP_MARK] = CP_INIT;
}
static inline float cl_size_s(int v) { return 0.02f * powf(50.f, v / 100.f); }        // 20 ms .. 1 s
static inline float cl_density(int v) { return powf(60.f, v / 100.f); }               // 1 .. 60 grains a second
static inline float cl_motion_bars(int v) { return v ? 64.f * powf(1 / 64.f, (v - 1) / 99.f) : 0; }   // the whole sample in 64 .. 1 bars

// CLOUD DEEP, the hidden page: finer control, Song.cx[slot][CX_*] (appended to Song; CX_MARK set once). Its defaults
// give exactly the sound of the six knobs alone.
enum { CX_SIZERND, CX_WINDOW, CX_REVERSE, CX_PITCH, CX_FINE, CX_PITCHRND, CX_SHMODE, CX_SYNC, CX_TIMERND, CX_WIDTH,
       CX_SCAN, CX_FREEZE, CX_ATTACK, CX_RELEASE, CX_TONE, CX_N, CX_MARK = 15 };
#define CX_INIT 0x5D
static void cx_range(int k, int *lo, int *hi) {
    *lo = 0; *hi = 100;
    switch (k) {
    case CX_WINDOW: *hi = 4; break;      // HANN TRIANGLE TUKEY PERC REV PERC
    case CX_PITCH: *lo = -24; *hi = 24; break;
    case CX_FINE: *lo = -50; *hi = 50; break;
    case CX_SHMODE: *hi = 2; break;      // OCT+5TH, OCTAVES, SCALE
    case CX_SYNC: *hi = 4; break;        // FREE, 1/4, 1/8, 1/16, 1/32
    case CX_SCAN: *hi = 2; break;        // FWD, REV, PING-PONG
    case CX_FREEZE: *hi = 1; break;
    case CX_TONE: *lo = -100; *hi = 100; break;   // below 0 darker (low-pass), above brighter (high-pass)
    }
}
static void cx_defaults(int8_t *cx) {
    memset(cx, 0, 16);
    cx[CX_ATTACK] = 66; cx[CX_RELEASE] = 68; cx[CX_MARK] = CX_INIT;   // 0.4 s in, 1.2 s out: as before
}
static inline float cx_attack_s(int v) { return 0.005f * powf(800.f, v / 100.f); }   // 5 ms .. 4 s
static inline float cx_release_s(int v) { return 0.02f * powf(400.f, v / 100.f); }   // 20 ms .. 8 s to silence

typedef struct { double pos; float rate, gl, gr, off, wst; int ph, len, rev; } Grain;   // wst: window table steps per frame   // len 0: free; off: its place in the SPRAY band (from the centre)
typedef struct {
    Grain g[CL_GRAINS];
    double scan;                         // how far MOTION has carried the cloud, 0..1 of the sample
    float env, next, rate, vel;          // level, frames to the next grain, pitch rate, velocity
    int stage, gate;                     // 0 fade in, 2 on, 3 fade out; frames until the key is let go (-1 held)
    unsigned rng;
    int scan_dir;                        // PING-PONG: -1 on the way back
    float tl[2], th[2];                  // TONE: low-pass and high-pass states, left and right
    unsigned cn; float c_att, c_rel, c_dens, c_gain, c_ta, c_mb; int c_len0;   // knob-only values, worked out every 16 frames (2026-11-11: they cost a pow each, on every frame)
} CloudV;

#define CL_WIN 1024
static float cl_win[5][CL_WIN + 1];      // the grain windows, filled once (cl_tables): a cosine per sample cost too much
static void cl_tables(void) {
    if (cl_win[0][CL_WIN / 2] != 0) return;
    for (int i = 0; i <= CL_WIN; i++) {
        float t = (float)i / CL_WIN;
        cl_win[0][i] = 0.5f - 0.5f * cosf(2 * (float)M_PI * t);                       // HANN
        cl_win[1][i] = 1 - fabsf(2 * t - 1);                                           // TRIANGLE
        cl_win[2][i] = t < 0.15f ? 0.5f - 0.5f * cosf((float)M_PI * t / 0.15f) : t > 0.85f ? 0.5f - 0.5f * cosf((float)M_PI * (1 - t) / 0.15f) : 1;   // TUKEY: flat
        cl_win[3][i] = t < 0.03f ? t / 0.03f : expf(-(t - 0.03f) * 5.f) * (1 - powf(t, 8));   // PERC: a hit, then it dies
        cl_win[4][CL_WIN - i] = cl_win[3][i];                                          // REV PERC (filled as PERC goes)
    }
}
static inline float cl_rand(CloudV *v) { v->rng = v->rng * 1664525u + 1013904223u; return (v->rng >> 8) / 16777216.f; }
static void cl_start(CloudV *v, float rate, float vel, int gate, unsigned seed) {
    cl_tables(); memset(v, 0, sizeof *v);
    v->rate = rate; v->vel = vel; v->gate = gate; v->rng = seed | 1; v->stage = 0; v->scan_dir = 1;
}
// One stereo frame; returns 0 once faded out with no grain left. bar_frames: frames per bar at the live tempo;
// cx: CLOUD DEEP; scale: the pad's scale (SHIMMER in SCALE mode).
static inline int cl_frame(CloudV *v, const Sample *s, const int8_t *cp, const int8_t *cx, int scale, double bar_frames, float *l, float *r) {
    if ((v->cn++ & 15) == 0) {
        v->c_att = 1.f / (cx_attack_s(cx[CX_ATTACK]) * SR); v->c_rel = (1 - expf(-1.f / (cx_release_s(cx[CX_RELEASE]) / 9.2f * SR)));
        v->c_mb = cl_motion_bars(cp[CP_MOTION]); v->c_len0 = (int)(cl_size_s(cp[CP_SIZE]) * SR); if (v->c_len0 < 64) v->c_len0 = 64;
        v->c_dens = cl_density(cp[CP_DENSITY]);
        float over = v->c_dens * cl_size_s(cp[CP_SIZE]) * 0.5f; v->c_gain = 1 / sqrtf(over > 1 ? over : 1);   // grains sounding at once, on average
        int tn = cx[CX_TONE];
        v->c_ta = tn < 0 ? 1 - expf(-2 * (float)M_PI * (20000.f * powf(300.f / 20000.f, -tn / 100.f)) / SR) : tn > 0 ? 1 - expf(-2 * (float)M_PI * (20.f * powf(100.f, tn / 100.f)) / SR) : 0;
    }
    if (v->gate > 0 && --v->gate == 0) v->stage = 3;
    if (v->stage == 0) { v->env += v->c_att; if (v->env >= 1) { v->env = 1; v->stage = 2; } }
    else if (v->stage == 3) v->env -= v->env * v->c_rel;
    float mb = v->c_mb;
    if (mb > 0 && bar_frames > 0 && !cx[CX_FREEZE]) {   // MOTION, its direction (SCAN), unless FROZEN
        double st = 1.0 / (mb * bar_frames);
        if (cx[CX_SCAN] == 1) { v->scan -= st; if (v->scan < 0) v->scan += 1; }
        else if (cx[CX_SCAN] == 2) { v->scan += st * v->scan_dir; if (v->scan >= 1) { v->scan = 2 - v->scan; v->scan_dir = -1; } if (v->scan < 0) { v->scan = -v->scan; v->scan_dir = 1; } }
        else { v->scan += st; if (v->scan >= 1) v->scan -= 1; }
    }
    int len0 = v->c_len0;
    if (v->stage != 3 && --v->next <= 0) {   // new grains
        int sync = cx[CX_SYNC], count = 1;
        float jit = cx[CX_TIMERND] / 100.f;
        if (sync && bar_frames > 0) {    // on the tempo's grid: 1/4 .. 1/32; DENSITY says how many per tick
            double per = bar_frames / (4 << (sync - 1)) / ((song.beats > 0 ? song.beats : 4) / 4.0);
            count = (int)(v->c_dens * per / SR + 0.5f); if (count < 1) count = 1; if (count > 4) count = 4;
            v->next += (float)per * (1 + (cl_rand(v) - 0.5f) * jit * 0.5f);
        } else v->next += SR / v->c_dens * (0.7f + 0.6f * cl_rand(v)) * (1 + (cl_rand(v) - 0.5f) * jit);
        for (int c = 0; c < count; c++)
        for (int i = 0; i < CL_GRAINS; i++) if (!v->g[i].len) {
            Grain *g = &v->g[i];
            float rt = v->rate * powf(2.f, (cx[CX_PITCH] + cx[CX_FINE] / 100.f + (cl_rand(v) - 0.5f) * 2 * cx[CX_PITCHRND] / 100.f) / 12.f);
            float sh = cp[CP_SHIMMER] / 100.f, u = cl_rand(v);
            if (u < sh) {
                float w = cl_rand(v);
                if (cx[CX_SHMODE] == 1) rt *= w < 0.5f ? 2.f : 0.5f;                                  // octaves only
                else if (cx[CX_SHMODE] == 2) { int dg = (int)(w * 15) - 7; rt *= powf(2.f, deg_semis(scale, dg) / 12.f); }   // the scale's degrees
                else rt *= w < 0.45f ? 2.f : w < 0.75f ? 0.5f : 1.4983f;                              // octave up, down, a fifth
            }
            int len = (int)(len0 * (1 + (cl_rand(v) - 0.5f) * 1.8f * cx[CX_SIZERND] / 100.f)); if (len < 64) len = 64;
            double span = s->n - 2 - (double)len * rt; if (span < 1) span = 1;
            float off = (cl_rand(v) - 0.5f) * cp[CP_SPRAY] / 100.f;
            double cc = cp[CP_POS] / 100.0 + v->scan + off;
            cc -= floor(cc);
            float wd = cx[CX_WIDTH] ? cx[CX_WIDTH] / 100.f : cp[CP_SPRAY] / 100.f;   // WIDTH 0: AUTO, follows SPRAY
            float p = (cl_rand(v) - 0.5f) * wd * 1.6f, a = (p + 1) * (float)M_PI / 4;
            g->pos = cc * span; g->off = off; g->rate = rt; g->ph = 0; g->len = len; g->wst = (float)CL_WIN / len; g->gl = cosf(a) * 1.41421f; g->gr = sinf(a) * 1.41421f;
            g->rev = cl_rand(v) * 100 < cx[CX_REVERSE];
            break;
        }
    }
    const float *wt = cl_win[cx[CX_WINDOW] % 5];
    float sl = 0, sr = 0; int live = 0;
    for (int i = 0; i < CL_GRAINS; i++) {
        Grain *g = &v->g[i]; if (!g->len) continue;
        live++;
        double p = g->pos + (g->rev ? g->len - 1 - g->ph : g->ph) * (double)g->rate;
        if (p >= 0 && p < s->n - 2) {
            float w = wt[(int)(g->ph * g->wst)], a, b;
            int k = (int)p; float f = (float)(p - k);
            a = s->d[2 * k] + (s->d[2 * k + 2] - s->d[2 * k]) * f; b = s->d[2 * k + 1] + (s->d[2 * k + 3] - s->d[2 * k + 1]) * f;
            sl += a * w * g->gl; sr += b * w * g->gr;
        }
        if (++g->ph >= g->len) g->len = 0;
    }
    if (v->stage == 3 && v->env < 1e-4f && !live) return 0;
    float gain = v->env * v->vel * v->c_gain;
    sl *= gain; sr *= gain;
    int tn = cx[CX_TONE];
    if (tn < 0) {                        // darker: a low-pass from 20 kHz down to 300 Hz
        float a = v->c_ta;
        v->tl[0] += (sl - v->tl[0]) * a; v->tl[1] += (sr - v->tl[1]) * a; sl = v->tl[0]; sr = v->tl[1];
    } else if (tn > 0) {                 // brighter: a high-pass from 20 Hz up to 2 kHz
        float a = v->c_ta;
        v->th[0] += (sl - v->th[0]) * a; v->th[1] += (sr - v->th[1]) * a; sl -= v->th[0]; sr -= v->th[1];
    }
    *l = sl; *r = sr;
    return 1;
}
#endif
