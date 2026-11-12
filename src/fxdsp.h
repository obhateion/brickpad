// BrickPad effect DSP (engine.c only): insert slots, the reverb (send A) and delay (send B) returns, master
// compressor and limiter. Audio thread only, no allocation; parameters are bytes from Song.mix read once per buffer.
#pragma once
#include "fx.h"

#define FXD 2048                          // modulated delay line per insert (chorus, flanger, tape): 42 ms
typedef struct {
    float xf; uint8_t type;               // crossfade to the effect (0 = dry, the effect is not run), the type the state is for
    int8_t p[6];                          // that type's parameters: a slot changing type fades the old effect out with its own
    float z[2][4];                        // filters per channel
    float ap[2][8], last[2];              // phaser allpass states, feedback
    float env, gr, g;                     // compressor: detector, gain reduction (dB), smoothed gain
    double lfo;                           // 0..1
    float hold[2]; int cnt; uint32_t rng; // bitcrusher
    float rc, rs;                         // ring modulator phasor
    int wp; float dl[2][FXD];             // delay line
    float mb[2][8][2], mbe[3], mbg[3];    // MULTIBAND: 8 filter states per channel, band detectors, band gains (linear)
    float mbs[MB_N];                      // MULTIBAND gains IN LOW MID HIGH OUT, smoothed (linear; 0 = not set yet)
    float view[6];                        // for the screen: COMP in dB and GR; FILTER cutoff; MULTIBAND levels and gains
} FxState;

static inline float fx_tanh(float x) { if (x > 3) return 1; if (x < -3) return -1; float x2 = x * x; return x * (27 + x2) / (27 + 9 * x2); }
static inline float fx_rand(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return (float)(*s >> 8) / 8388608.f - 1.f; }
static inline float fx_read(const float *d, float pos) {   // pos frames behind... d is a ring of FXD, pos already wrapped
    int a = (int)pos; float f = pos - a; int b = (a + 1) & (FXD - 1);
    return d[a & (FXD - 1)] + (d[b] - d[a & (FXD - 1)]) * f;
}
// SPRING (GRAND's spring reverb, 2026-11-10): two springs, a delay each with 12 allpasses in the loop
#define SPN 4096
typedef struct { float d[2][SPN]; int w; float ap[2][12], lp[2], dc[2]; } SpringState;
static void spring_run(SpringState *s, float *b, int n, int mix) {
    static const int len[2] = {1777, 1999};   // 37 and 42 ms
    float m = mix / 100.f, fb = 0.72f, a = 0.62f, lpa = 1 - expf(-2 * (float)M_PI * 3500.f / SR);
    for (int k = 0; k < n; k++) {
        float x = 0.5f * (b[2 * k] + b[2 * k + 1]), o[2];
        for (int c = 0; c < 2; c++) {
            float y = s->d[c][(s->w - len[c]) & (SPN - 1)];
            for (int j = 0; j < 12; j++) { float z = s->ap[c][j], q = -a * y + z; s->ap[c][j] = y + a * q; y = q; }
            s->lp[c] += (y - s->lp[c]) * lpa; y = s->lp[c];
            s->dc[c] += (y - s->dc[c]) * 0.002f; y -= s->dc[c];   // no slow drift in the loop
            s->d[c][s->w] = fx_tanh(x + fb * y);
            o[c] = y;
        }
        s->w = (s->w + 1) & (SPN - 1);
        b[2 * k] = b[2 * k] * (1 - 0.4f * m) + o[0] * m * 1.3f; b[2 * k + 1] = b[2 * k + 1] * (1 - 0.4f * m) + o[1] * m * 1.3f;
    }
}
static inline float fx_lfo_sin(double ph) { return sinf((float)(ph * 2 * M_PI)); }
static inline void fx_svf(float *z, float g, float k, float x, float *lp, float *hp) {   // TPT state variable, 12 dB
    float a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2, v3 = x - z[1], v1 = a1 * z[0] + a2 * v3, v2 = z[1] + a2 * z[0] + a3 * v3;
    z[0] = 2 * v1 - z[0]; z[1] = 2 * v2 - z[1]; *lp = v2; *hp = x - k * v1 - v2;
}

// One insert slot in place on a stereo bus of n frames. tempo in BPM. on: whether the slot should sound now.
static const float *fx_keybuf;             // COMPRESSOR KEY: another pad's sound for the detector (NULL: its own); set around a call
static void fx_insert_ex(FxState *st, const FxSlot *f, const int8_t *mbx, int on, float *b, int n, double tempo);
static void fx_insert(FxState *st, const FxSlot *f, int on, float *b, int n, double tempo) { fx_insert_ex(st, f, NULL, on, b, n, tempo); }
static void fx_insert_ex(FxState *st, const FxSlot *f, const int8_t *mbx, int on, float *b, int n, double tempo) {
    int want = on && f->type != FX_NONE;
    if (f->type != st->type) {            // a new type: fade the old one out first, then start clean
        if (st->xf <= 0) { uint8_t t = f->type; memset(st, 0, sizeof *st); st->type = t; st->rng = 0x9E3779B9u; st->rc = 1; st->g = 1; }
        else want = 0;
    }
    if (!want && st->xf <= 0) return;     // dry: nothing to do, exactly the input
    float step = 1.f / 256, xf = st->xf;
    if (f->type == st->type) memcpy(st->p, f->p, 6);
    const int8_t *p = st->p; int ty = st->type;
    float mix = 1;
    // per-buffer coefficients
    float c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0;
    double lfo_inc = 0;
    switch (ty) {
    case FX_COMP: c0 = -40 + 40 * p[0] / 100.f; c1 = 1 + 19 * (p[1] / 100.f) * (p[1] / 100.f);
        c2 = 1 - expf(-1.f / (0.0001f * powf(1000, p[2] / 100.f) * SR)); c3 = 1 - expf(-1.f / (0.01f * powf(100, p[3] / 100.f) * SR));
        c4 = 24 * p[4] / 100.f; mix = p[5] / 100.f; break;
    case FX_DIST: c0 = 1 + (p[1] / 100.f) * (p[1] / 100.f) * 40; c1 = 1 - expf(-2 * (float)M_PI * 500 * exp2f(p[2] / 100.f * 5) / SR);
        c2 = p[3] / 50.f * 0.8f / (c0 > 4 ? 1 : 1); mix = p[4] / 100.f; break;
    case FX_CHORUS: lfo_inc = 0.05 * pow(100, p[0] / 100.0) / SR; c0 = (5 + p[2] * 0.25f) * SR / 1000; c1 = p[1] / 100.f * 5 * SR / 1000;
        c2 = p[3] / 100.f * 0.7f; c3 = p[4] / 100.f * 0.25f; mix = p[5] / 100.f; break;
    case FX_FLANGER: lfo_inc = 0.02 * pow(150, p[0] / 100.0) / SR; c0 = (0.1f + p[2] / 100.f * 5) * SR / 1000; c1 = p[1] / 100.f * 5 * SR / 1000;
        c2 = p[3] / 100.f * 0.95f; c3 = p[4] / 100.f * 0.25f; mix = p[5] / 100.f; break;
    case FX_PHASER: lfo_inc = 0.02 * pow(200, p[0] / 100.0) / SR; c0 = 200 * exp2f(p[2] / 100.f * 5); c1 = p[1] / 100.f * 2;
        c2 = p[3] / 100.f * 0.9f; c3 = 4 + 2 * p[4]; mix = p[5] / 100.f; break;
    case FX_FILTER: c0 = 20 * exp2f(p[1] / 100.f * 10); c1 = 2 - 2 * (p[2] / 100.f) * 0.98f; c2 = 1 + p[3] / 100.f * 8;
        lfo_inc = p[4] ? tempo / 60.0 / div_beats[p[4] - 1] / SR : 0; c3 = p[5] / 100.f * 4; break;
    case FX_CRUSH: c0 = exp2f(15 - p[0] / 100.f * 14); c1 = 1 + p[1] / 100.f * 31; c2 = p[2] / 100.f;
        c3 = 1 - expf(-2 * (float)M_PI * 400 * exp2f(p[3] / 100.f * 5.6f) / SR); mix = p[4] / 100.f; break;
    case FX_TAPE: c0 = 1 + p[0] / 100.f * 4; c1 = p[1] / 100.f * 0.002f * SR; c2 = p[2] / 100.f * 0.0003f * SR;
        c3 = 1 - expf(-2 * (float)M_PI * 1500 * exp2f(p[3] / 100.f * 3.7f) / SR); c4 = p[4] / 100.f * 0.004f; mix = p[5] / 100.f; break;
    case FX_AUTOPAN: lfo_inc = tempo / 60.0 / div_beats[p[0] % NDIV] / SR; c0 = p[1] / 100.f; break;
    case FX_MULTI: c0 = tanf((float)M_PI * mb_xlow(p[4]) / SR); c1 = tanf((float)M_PI * (mb_xhigh(p[5]) < SR * 0.45f ? mb_xhigh(p[5]) : SR * 0.45f) / SR);
        c2 = 1 - expf(-1.f / (0.002f * mb_tscale(p[1]) * SR)); c3 = 1 - expf(-1.f / (0.06f * mb_tscale(p[1]) * SR)); break;
    case FX_RING: { float hz = 20 * exp2f(p[0] / 100.f * 8 + p[1] / 1200.f); c0 = 2 * (float)M_PI * hz / SR;
        lfo_inc = 0.05 * pow(200, p[2] / 100.0) / SR; c1 = p[3] / 100.f * 2; mix = p[4] / 100.f; break; }
    }
    for (int k = 0; k < n; k++) {
        xf += want ? step : -step; if (xf > 1) xf = 1; if (xf < 0) xf = 0;
        float dl = b[2 * k], dr = b[2 * k + 1], wl = dl, wr = dr;
        switch (ty) {
        case FX_COMP: {
            float kl = fx_keybuf ? fx_keybuf[2 * k] : dl, kr = fx_keybuf ? fx_keybuf[2 * k + 1] : dr;
            float a = fabsf(kl) > fabsf(kr) ? fabsf(kl) : fabsf(kr);
            st->env += (a - st->env) * (a > st->env ? c2 : c3);
            if ((k & 7) == 0) { float lv = 20 * log10f(st->env + 1e-9f), over = lv - c0; st->gr = over > 0 ? over * (1 - 1 / c1) : 0; st->g = powf(10, (c4 - st->gr) / 20); if (k == 0) { st->view[0] = lv; st->view[1] = st->gr; } }
            wl = dl * st->g; wr = dr * st->g; break; }
        case FX_DIST: for (int c = 0; c < 2; c++) {
                float x = (c ? dr : dl) * c0, y;
                switch (p[0]) { case 1: y = x > 1 ? 1 : x < -1 ? -1 : x; break;
                                case 2: y = sinf(x * 0.5f * (float)M_PI); break;
                                case 3: y = x > 0 ? fx_tanh(x) : fx_tanh(x * 0.4f) * 0.8f; break;
                                default: y = fx_tanh(x); }
                st->z[c][0] += (y - st->z[c][0]) * c1;
                if (c) wr = st->z[c][0] * c2; else wl = st->z[c][0] * c2;
            } break;
        case FX_CHORUS: case FX_FLANGER: {
            float ml = fx_lfo_sin(st->lfo), mr = fx_lfo_sin(st->lfo + c3);
            float tl = c0 + c1 * (ty == FX_FLANGER ? 0.5f + 0.5f * ml : ml), tr = c0 + c1 * (ty == FX_FLANGER ? 0.5f + 0.5f * mr : mr);
            if (tl < 1) tl = 1; if (tr < 1) tr = 1;
            float rl = fx_read(st->dl[0], (float)st->wp - tl + FXD), rr = fx_read(st->dl[1], (float)st->wp - tr + FXD);
            st->dl[0][st->wp] = dl + rl * c2; st->dl[1][st->wp] = dr + rr * c2; st->wp = (st->wp + 1) & (FXD - 1);
            wl = rl; wr = rr; break; }
        case FX_PHASER: {
            if ((k & 15) == 0) { for (int c = 0; c < 2; c++) {
                float fc = c0 * exp2f(c1 * fx_lfo_sin(st->lfo + c * 0.25)); if (fc > SR * 0.45f) fc = SR * 0.45f;
                float t = tanf((float)M_PI * fc / SR); st->z[c][3] = (t - 1) / (t + 1); } }
            for (int c = 0; c < 2; c++) {
                float x = (c ? dr : dl) + st->last[c] * c2, a = st->z[c][3];
                for (int i = 0; i < (int)c3; i++) { float y = a * x + st->ap[c][i]; st->ap[c][i] = x - a * y; x = y; }
                st->last[c] = x; if (c) wr = x; else wl = x;
            } break; }
        case FX_FILTER: {
            float fc = c0 * (lfo_inc ? exp2f(c3 * fx_lfo_sin(st->lfo)) : 1); if (fc > SR * 0.45f) fc = SR * 0.45f;
            if ((k & 15) == 0 || k == 0) { float g = tanf((float)M_PI * fc / SR); st->z[0][3] = g; if (k == 0) st->view[0] = fc; }
            float g = st->z[0][3], a1 = 1 / (1 + g * (g + c1)), a2 = g * a1, a3 = g * a2;
            for (int c = 0; c < 2; c++) {
                float x = c2 > 1.01f ? fx_tanh((c ? dr : dl) * c2) / fx_tanh(c2) : (c ? dr : dl);
                float *z = st->z[c]; float v3 = x - z[1], v1 = a1 * z[0] + a2 * v3, v2 = z[1] + a2 * z[0] + a3 * v3;
                z[0] = 2 * v1 - z[0]; z[1] = 2 * v2 - z[1];
                float lp = v2, bp = v1, hp = x - c1 * v1 - v2, y = p[0] == 1 ? hp : p[0] == 2 ? bp : p[0] == 3 ? lp + hp : lp;
                if (c) wr = y; else wl = y;
            } break; }
        case FX_CRUSH: {
            if (--st->cnt <= 0) { st->hold[0] = floorf(dl * c0 + 0.5f) / c0; st->hold[1] = floorf(dr * c0 + 0.5f) / c0;
                st->cnt = (int)(c1 * (1 + c2 * 0.5f * fx_rand(&st->rng)) + 0.5f); if (st->cnt < 1) st->cnt = 1; }
            st->z[0][0] += (st->hold[0] - st->z[0][0]) * c3; st->z[1][0] += (st->hold[1] - st->z[1][0]) * c3;
            wl = st->z[0][0]; wr = st->z[1][0]; break; }
        case FX_TAPE: {
            float w = c1 * (0.5f + 0.5f * fx_lfo_sin(st->lfo)) + c2 * (0.5f + 0.5f * fx_lfo_sin(st->lfo * 14)) + 0.004f * SR;
            for (int c = 0; c < 2; c++) st->dl[c][st->wp] = fx_tanh((c ? dr : dl) * c0) / fx_tanh(c0);
            float rl = fx_read(st->dl[0], (float)st->wp - w + FXD), rr = fx_read(st->dl[1], (float)st->wp - w + FXD);
            st->wp = (st->wp + 1) & (FXD - 1);
            st->z[0][0] += (rl - st->z[0][0]) * c3; st->z[1][0] += (rr - st->z[1][0]) * c3;
            wl = st->z[0][0] + c4 * fx_rand(&st->rng); wr = st->z[1][0] + c4 * fx_rand(&st->rng); break; }
        case FX_AUTOPAN: {
            double ph = st->lfo; float m = p[2] == 1 ? (float)(4 * fabs(ph - floor(ph + 0.5)) - 1) : p[2] == 2 ? fx_tanh(8 * fx_lfo_sin(ph)) : fx_lfo_sin(ph);
            m = -m;
            float pan = c0 * m, a = (pan + 1) * (float)M_PI / 4, mono = 0.5f * (dl + dr);
            (void)mono; wl = dl * cosf(a) * 1.41421f; wr = dr * sinf(a) * 1.41421f; break; }
        case FX_MULTI: {                  // three bands (Linkwitz-Riley 24 dB crossovers), each compressed down above and up below its threshold
            float band[3][2]; const float K = 1.41421356f;
            if (k == 0) for (int q = 0; q < MB_N; q++) if (st->mbs[q] <= 0) st->mbs[q] = mbx ? mb_lin(mbx[q]) : 1;
            if (mbx) for (int q = 0; q < MB_N; q++) { float tg = mb_lin(mbx[q]); if (st->mbs[q] != tg) { st->mbs[q] += (tg - st->mbs[q]) * 0.002f; if (fabsf(st->mbs[q] - tg) < 1e-5f) st->mbs[q] = tg; } }
            for (int c = 0; c < 2; c++) { float x = (c ? dr : dl) * st->mbs[MB_IN], l1, h1, l2, h2, t;
                fx_svf(st->mb[c][0], c0, K, x, &l1, &t); fx_svf(st->mb[c][1], c0, K, l1, &l1, &t);       // low: LP LP at X LOW
                fx_svf(st->mb[c][2], c0, K, x, &t, &h1); fx_svf(st->mb[c][3], c0, K, h1, &t, &h1);       // the rest: HP HP
                fx_svf(st->mb[c][4], c1, K, h1, &l2, &t); fx_svf(st->mb[c][5], c1, K, l2, &l2, &t);      // mid: LP LP at X HIGH
                fx_svf(st->mb[c][6], c1, K, h1, &t, &h2); fx_svf(st->mb[c][7], c1, K, h2, &t, &h2);      // high: HP HP
                band[0][c] = l1; band[1][c] = l2; band[2][c] = h2; }
            if (k == 0) for (int bnd = 0; bnd < 3; bnd++) if (st->mbg[bnd] <= 0) st->mbg[bnd] = 1;
            float depth = p[0] / 100.f; wl = wr = 0;
            for (int bnd = 0; bnd < 3; bnd++) {
                float a = fabsf(band[bnd][0]) > fabsf(band[bnd][1]) ? fabsf(band[bnd][0]) : fabsf(band[bnd][1]);
                st->mbe[bnd] += (a - st->mbe[bnd]) * (a > st->mbe[bnd] ? c2 : c3);
                if ((k & 7) == 0) {
                    float lv = 20 * log10f(st->mbe[bnd] + 1e-9f), th = mb_thresh[bnd], gd = 0;
                    if (lv > th) gd -= (lv - th) * (1 - 1 / 66.f) * p[3] / 100.f;                   // DOWNWARD: 66:1 above
                    else if (lv > -72) { float up = (th - lv) * (1 - 1 / 4.f) * p[2] / 100.f; gd += up > 30 ? 30 : up; }   // UPWARD: 4:1 below, not from silence
                    gd += mb_makeup[bnd];
                    float gl = 1 + depth * (powf(10, gd / 20) - 1); st->mbg[bnd] += (gl - st->mbg[bnd]) * 0.25f;
                    if (k == 0) { st->view[bnd] = lv; st->view[3 + bnd] = 20 * log10f(st->mbg[bnd] + 1e-9f); }
                }
                float bg = st->mbg[bnd] * st->mbs[MB_LOW + bnd]; wl += band[bnd][0] * bg; wr += band[bnd][1] * bg;
            }
            wl *= st->mbs[MB_OUT]; wr *= st->mbs[MB_OUT];
            break; }
        case FX_RING: {
            float w = c0 * (lfo_inc && c1 ? exp2f(c1 * fx_lfo_sin(st->lfo)) : 1), cw = cosf(w), sw = sinf(w);
            float nc = st->rc * cw - st->rs * sw, ns = st->rc * sw + st->rs * cw; st->rc = nc; st->rs = ns;
            wl = dl * st->rc; wr = dr * st->rc; break; }
        }
        st->lfo += lfo_inc ? lfo_inc : (ty == FX_TAPE ? 0.5 / SR : 0); if (st->lfo >= 1) st->lfo -= 1;
        wl = dl + (wl - dl) * mix; wr = dr + (wr - dr) * mix;
        b[2 * k] = dl + (wl - dl) * xf; b[2 * k + 1] = dr + (wr - dr) * xf;
    }
    if (ty == FX_RING) { float m = sqrtf(st->rc * st->rc + st->rs * st->rs); if (m > 0) { st->rc /= m; st->rs /= m; } else { st->rc = 1; st->rs = 0; } }
    st->xf = xf;
}

// ---------- send A: reverb (predelay, 4 allpass diffusers, 8-line feedback delay network) ----------
#define RV_LINE 8192
#define RV_PREBUF 9600   // the predelay line (200 ms); RV_PRE itself is the PREDELAY knob (bp.h) - this name once hid it (fixed 2026-11-10)
typedef struct {
    float pre[RV_PREBUF]; int pw;
    float ap[4][512]; int apw[4];
    float line[8][RV_LINE]; int lw;
    float damp[8], lc, quiet;
} Reverb;
static const int rv_ap_len[4] = {155, 116, 413, 302}, rv_len0[8] = {1123, 1289, 1447, 1621, 1789, 1987, 2203, 2459};
static void reverb_run(Reverb *r, const int8_t *pr, const float *in, float *out, int n) {   // out += return
    float size = 0.5f + pr[RV_SIZE] / 100.f * 1.5f, t60 = 0.3f * powf(30, pr[RV_DECAY] / 100.f), damp = pr[RV_DAMP] / 100.f * 0.7f;
    int pre = (int)(pr[RV_PRE] / 100.f * 0.2f * SR); if (pre >= RV_PREBUF) pre = RV_PREBUF - 1; if (pre < 0) pre = 0;
    float lcut = 1 - expf(-2 * (float)M_PI * (20 * powf(50, pr[RV_LOWCUT] / 100.f)) / SR), w = pr[RV_WIDTH] / 100.f;
    float lvl = pr[RV_MUTE] ? 0 : pr[RV_LEVEL] / 100.f;
    int len[8]; float g[8];
    for (int i = 0; i < 8; i++) { len[i] = (int)(rv_len0[i] * size); if (len[i] >= RV_LINE) len[i] = RV_LINE - 1; g[i] = powf(10, -3.f * len[i] / (t60 * SR)); }
    float ein = 0; for (int k = 0; k < n; k++) ein += fabsf(in[2 * k]) + fabsf(in[2 * k + 1]);
    if (ein < 1e-7f) { if (r->quiet > t60 * SR + SR) return; r->quiet += n; } else r->quiet = 0;   // nothing in, tail gone: skip
    for (int k = 0; k < n; k++) {
        float x = 0.5f * (in[2 * k] + in[2 * k + 1]);
        r->lc += (x - r->lc) * lcut; x -= r->lc;                       // low cut
        r->pre[r->pw] = x; int rp = r->pw - pre; if (rp < 0) rp += RV_PREBUF; x = r->pre[rp]; r->pw = (r->pw + 1) % RV_PREBUF;
        for (int a = 0; a < 4; a++) {                                  // diffusers
            int L = rv_ap_len[a], rd = r->apw[a] - L; if (rd < 0) rd += 512;
            float d = r->ap[a][rd], y = -0.6f * x + d; r->ap[a][r->apw[a]] = x + 0.6f * y; r->apw[a] = (r->apw[a] + 1) & 511; x = y;
        }
        float v[8];
        for (int i = 0; i < 8; i++) { int rd = r->lw - len[i]; if (rd < 0) rd += RV_LINE; v[i] = r->line[i][rd]; }
        float ol = v[0] - v[1] + v[2] - v[3] + v[4] - v[5] + v[6] - v[7], orr = v[0] + v[1] - v[2] - v[3] + v[4] + v[5] - v[6] - v[7];
        // Hadamard 8 (butterflies), normalised
        for (int s = 1; s < 8; s <<= 1) for (int i = 0; i < 8; i += 2 * s) for (int j = i; j < i + s; j++) { float a = v[j], c = v[j + s]; v[j] = a + c; v[j + s] = a - c; }
        for (int i = 0; i < 8; i++) {
            float y = v[i] * 0.35355339f * g[i];
            r->damp[i] += (y - r->damp[i]) * (1 - damp); y = r->damp[i];
            r->line[i][r->lw] = y + x * 0.5f;
        }
        r->lw = (r->lw + 1) % RV_LINE;
        ol *= 0.3f; orr *= 0.3f; float m = 0.5f * (ol + orr);
        out[2 * k] += (m + (ol - m) * w) * lvl; out[2 * k + 1] += (m + (orr - m) * w) * lvl;
    }
}

// ---------- send B: tempo delay, 2 s, crossfading heads when the time changes ----------
#define DL_LEN (2 * SR)
typedef struct { float buf[2][DL_LEN]; int w; float cur, nxt; int xfade; float tone[2], lcut[2], quiet; } Delay;
static void delay_run(Delay *d, const int8_t *pr, double tempo, const float *in, float *out, int n) {
    float want = (float)(SR * 60.0 / tempo * div_beats[pr[DL_TIME] % NDIV]); while (want > DL_LEN - 2) want *= 0.5f;
    float fb = pr[DL_FB] / 100.f, tc = 1 - expf(-2 * (float)M_PI * 800 * exp2f(pr[DL_TONE] / 100.f * 4.6f) / SR);
    float lc = 1 - expf(-2 * (float)M_PI * 20 * powf(25, pr[DL_LOWCUT] / 100.f) / SR), w = pr[DL_WIDTH] / 100.f;
    float lvl = pr[DL_MUTE] ? 0 : pr[DL_LEVEL] / 100.f; int ping = pr[DL_PING];
    float ein = 0; for (int k = 0; k < n; k++) ein += fabsf(in[2 * k]) + fabsf(in[2 * k + 1]);
    if (ein < 1e-7f) { if (d->quiet > SR * 12) return; d->quiet += n; } else d->quiet = 0;
    if (d->cur <= 0) d->cur = want;
    if (!d->xfade && fabsf(want - d->cur) > 1) { d->nxt = want; d->xfade = 2048; }
    for (int k = 0; k < n; k++) {
        float r[2];
        for (int c = 0; c < 2; c++) {
            float rp = d->w - d->cur; if (rp < 0) rp += DL_LEN; int a = (int)rp; float f = rp - a, *b = d->buf[c];
            float y = b[a] + (b[(a + 1) % DL_LEN] - b[a]) * f;
            if (d->xfade) { float rq = d->w - d->nxt; if (rq < 0) rq += DL_LEN; int q = (int)rq; float g = rq - q, z = b[q] + (b[(q + 1) % DL_LEN] - b[q]) * g, t = d->xfade / 2048.f; y = y * t + z * (1 - t); }
            r[c] = y;
        }
        if (d->xfade && --d->xfade == 0) d->cur = d->nxt;
        for (int c = 0; c < 2; c++) {
            float y = r[ping ? 1 - c : c];
            d->tone[c] += (y - d->tone[c]) * tc; y = d->tone[c];
            d->lcut[c] += (y - d->lcut[c]) * lc; y -= d->lcut[c];
            float x = ping ? (c == 0 ? 0.5f * (in[2 * k] + in[2 * k + 1]) : 0) : in[2 * k + c];
            d->buf[c][d->w] = x + fx_tanh(y * fb);
        }
        d->w = (d->w + 1) % DL_LEN;
        float m = 0.5f * (r[0] + r[1]);
        out[2 * k] += (m + (r[0] - m) * w) * lvl; out[2 * k + 1] += (m + (r[1] - m) * w) * lvl;
    }
}

// ---------- master: compressor and limiter ----------
typedef struct { float env, g, lg; } MasterState;
static float master_gr;                   // dB the master takes off now (comp + limiter), for the meter
static void master_run(MasterState *m, const int8_t *ms, float *o, int n) {
    float gr = 0;
    if (ms[MS_COMP]) {
        float th = -40 + 40 * ms[MS_THRESH] / 100.f, ra = 1 + 19 * (ms[MS_RATIO] / 100.f) * (ms[MS_RATIO] / 100.f), mk = 24 * ms[MS_MAKEUP] / 100.f;
        float at = 1 - expf(-1.f / (0.0001f * powf(1000, ms[MS_ATTACK] / 100.f) * SR)), rl = 1 - expf(-1.f / (0.01f * powf(100, ms[MS_RELEASE] / 100.f) * SR));
        if (m->g <= 0) m->g = 1;
        for (int k = 0; k < n; k++) {
            float a = fabsf(o[2 * k]) > fabsf(o[2 * k + 1]) ? fabsf(o[2 * k]) : fabsf(o[2 * k + 1]);
            m->env += (a - m->env) * (a > m->env ? at : rl);
            if ((k & 7) == 0) { float lv = 20 * log10f(m->env + 1e-9f), over = lv - th, r = over > 0 ? over * (1 - 1 / ra) : 0; m->g = powf(10, (mk - r) / 20); if (r > gr) gr = r; }
            o[2 * k] *= m->g; o[2 * k + 1] *= m->g;
        }
    }
    if (ms[MS_LIM]) {
        float ceil = ms[MS_CEIL] / 100.f; if (ceil < 0.1f) ceil = 0.1f;
        float rel = 1 - expf(-1.f / ((0.05f + ms[MS_LIMREL] / 100.f * 0.25f) * SR));
        if (m->lg <= 0) m->lg = 1;
        for (int k = 0; k < n; k++) {
            float a = fabsf(o[2 * k]) > fabsf(o[2 * k + 1]) ? fabsf(o[2 * k]) : fabsf(o[2 * k + 1]);
            float need = a * m->lg > ceil ? ceil / a : 1;
            if (need < m->lg) m->lg = need; else m->lg += (1 - m->lg) * rel;
            o[2 * k] *= m->lg; o[2 * k + 1] *= m->lg;
        }
        float lr = -20 * log10f(m->lg); if (lr > gr) gr = lr;
    }
    master_gr = gr;
}
