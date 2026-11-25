// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// WAVE: a wavetable synth voice (the DSP core; not wired into engine.c / main.c yet, see docs/WAVE.md).
// Two wavetable oscillators (2048-sample frames, up to 256 per table, POSITION morphs between neighbours, played
// band-limited from per-octave mipmaps), each with UNISON 1..8 (detune, stereo width) and a WARP mode; a SUB
// oscillator and NOISE; one filter per voice (SVF LP12 LP24 HP12 BP NOTCH, or a 4-pole ladder); 3 ADSR envelopes
// (ENV1 = the amp), 4 LFOs and a 16-slot mod matrix. 8 voices with stealing, or mono / legato with GLIDE.
// Everything here is our own code: the FFT, the tables (made from formulas at load time), the filters (textbook
// TPT forms), the WAV reader. Parameters are a plain int8_t array (WP_*, 256 bytes: Song can keep one per pad).
// Audio side (wv_render and what it calls): no allocation, no locks, no file access, deterministic.
// Load side (wt_*): allocates and may read files; call it from the UI / loader thread only.
#ifndef BP_WAVE_H
#define BP_WAVE_H
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SR
#define SR 48000
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------- wavetables ----------
// A table is nf frames; each frame is kept at WT_NLV mipmap levels. Level m holds harmonics 1 .. 1024 >> m (level 0:
// 1..1023) in wt_lsz[m] samples (never fewer than 32 per harmonic, so a straight-line read stays clean), with one
// guard sample before and two after (wrap copies) for the cubic read. Frame f, level m starts at
// d + f * WT_STRIDE + wt_loff[m]; its first real sample is one further on.
#define WT_LEN 2048
#define WT_MAXF 256
#define WT_NLV 11
#define WT_STRIDE 12289
#define WT_SLOTS 32                      // table slots in a store: 0..WT_NBUILT-1 built in, the rest for user WAVs
#define WT_NBUILT 12
#define WV_FLIM 28000.f                  // the top harmonic may reach this: what folds back lands above 20 kHz
static const int wt_lsz[WT_NLV] = {2048, 2048, 2048, 2048, 2048, 1024, 512, 256, 128, 64, 32};
static const int wt_lbits[WT_NLV] = {11, 11, 11, 11, 11, 10, 9, 8, 7, 6, 5};
static const int wt_loff[WT_NLV] = {0, 2051, 4102, 6153, 8204, 10255, 11282, 11797, 12056, 12187, 12254};
typedef struct { char name[24]; int nf; float *d; } WaveTable;
typedef struct { WaveTable t[WT_SLOTS]; } WaveStore;
static const char *const wt_bnames[WT_NBUILT] = {"BASIC", "PWM", "VOWEL", "HARMS", "NOISE", "DIGITAL", "SYNC", "FM",
                                                  "FOLD", "ORGAN", "COMB", "RESO"};
static const int wt_bnf[WT_NBUILT] = {31, 32, 17, 32, 16, 16, 32, 16, 16, 9, 16, 32};   // frames of each built-in

// In-place radix-2 FFT (n a power of two). inv: e^{+i}, no 1/n. Load time and tests only.
static void wt_fft(double *re, double *im, int n, int inv) {
    for (int i = 1, j = 0; i < n; i++) {
        int b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double a = (inv ? 2 : -2) * M_PI / len, wr = cos(a), wi = sin(a);
        for (int i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (int k = 0; k < len / 2; k++) {
                int p = i + k, q = p + len / 2;
                double tr = re[q] * cr - im[q] * ci, ti = re[q] * ci + im[q] * cr;
                re[q] = re[p] - tr; im[q] = im[p] - ti; re[p] += tr; im[p] += ti;
                double nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}
static double wt_re[4096], wt_im[4096];  // scratch (the load side runs on one thread)
static void wt_free(WaveTable *t) { free(t->d); memset(t, 0, sizeof *t); }
static int wt_alloc(WaveTable *t, int nf, const char *name) {
    float *d = (float *)calloc((size_t)nf * WT_STRIDE, sizeof(float));
    if (!d) return 0;
    free(t->d); t->d = d; t->nf = nf; snprintf(t->name, sizeof t->name, "%s", name ? name : "");
    return 1;
}
// One frame from its harmonics: x(t) = sum a[h] cos(2 pi h t) + b[h] sin(2 pi h t), h = 1..H. norm: peak of level 0 -> 1.
static void wt_frame_spec(WaveTable *t, int f, const double *a, const double *b, int H, int norm) {
    float *fr = t->d + (size_t)f * WT_STRIDE; double pk = 0;
    for (int m = 0; m < WT_NLV; m++) {
        int N = wt_lsz[m], hm = 1024 >> m; if (hm > N / 2 - 1) hm = N / 2 - 1; if (hm > H) hm = H;
        memset(wt_re, 0, N * sizeof(double)); memset(wt_im, 0, N * sizeof(double));
        for (int h = 1; h <= hm; h++) { wt_re[h] = a[h] * 0.5; wt_im[h] = -b[h] * 0.5; wt_re[N - h] = wt_re[h]; wt_im[N - h] = -wt_im[h]; }
        wt_fft(wt_re, wt_im, N, 1);
        float *x = fr + wt_loff[m] + 1;
        for (int i = 0; i < N; i++) { x[i] = (float)wt_re[i]; if (!m && fabs(wt_re[i]) > pk) pk = fabs(wt_re[i]); }
        x[-1] = x[N - 1]; x[N] = x[0]; x[N + 1] = x[1];
    }
    if (norm && pk > 1e-9) { float s = (float)(1 / pk); for (int i = 0; i < WT_STRIDE; i++) fr[i] *= s; }
}
// One frame from S time samples (S a power of two, 32..4096): its harmonics up to min(S/2 - 1, 1023) are kept.
static void wt_frame_time(WaveTable *t, int f, const float *x, int S, int norm) {
    static double a[1025], b[1025];
    for (int i = 0; i < S; i++) { wt_re[i] = x[i]; wt_im[i] = 0; }
    wt_fft(wt_re, wt_im, S, 0);
    int H = S / 2 - 1; if (H > 1023) H = 1023;
    for (int h = 1; h <= H; h++) { a[h] = 2 * wt_re[h] / S; b[h] = -2 * wt_im[h] / S; }
    wt_frame_spec(t, f, a, b, H, norm);
}
static uint32_t wt_rng(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }
static double wt_rnd(uint32_t *s) { return wt_rng(s) / 16777216.0; }
// The built-in tables, all from formulas. Fills a/b (harmonics, returns H) or x (2048 samples, returns 0).
static int wt_gen(int k, int f, int nf, double *a, double *b, float *x) {
    double u = nf > 1 ? (double)f / (nf - 1) : 0, P = M_PI;
    memset(a, 0, 1025 * sizeof(double)); memset(b, 0, 1025 * sizeof(double));
    switch (k) {
    case 0: {                            // BASIC: sine -> triangle -> saw -> square, a third of the frames each
        double s = u * 3; int i = s >= 3 ? 2 : (int)s; double w = s - i;
        for (int h = 1; h <= 1023; h++) {
            double c[4] = {h == 1, h & 1 ? 8 / (P * P * h * h) * (((h - 1) / 2) & 1 ? -1 : 1) : 0, -2 / (P * h), h & 1 ? 4 / (P * h) : 0};
            b[h] = (1 - w) * c[i] + w * c[i + 1];
        }
        return 1023; }
    case 1: {                            // PWM: a pulse from 50 % down to 3 % (a saw minus a shifted saw)
        double w = 0.5 - 0.47 * u;
        for (int h = 1; h <= 1023; h++) { double s = -2 / (P * h); a[h] = s * sin(2 * P * h * w); b[h] = s * (1 - cos(2 * P * h * w)); }
        return 1023; }
    case 2: {                            // VOWEL: a 110 Hz voice through A E I O U formants (averaged textbook values)
        static const double F[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
        static const double A[3] = {1, 0.5, 0.25}, B[3] = {80, 90, 120};
        double s = u * 4; int i = s >= 4 ? 3 : (int)s; double w = s - i;
        for (int h = 1; h <= 1023; h++) {
            double fq = h * 110.0, e = 0.01; if (fq > 9000) break;
            for (int j = 0; j < 3; j++) { double fj = (1 - w) * F[i][j] + w * F[i + 1][j], z = (fq - fj) / B[j]; e += A[j] * exp(-0.5 * z * z); }
            b[h] = e / sqrt(h);
        }
        return 1023; }
    case 3:                              // HARMS: harmonics come in one by one (1 .. 32), saw weights
        for (int h = 1; h <= f + 1; h++) b[h] = 1.0 / h;
        return 1023;
    case 4: {                            // NOISE: random spectra, dark to bright
        uint32_t r = 12345u + (uint32_t)f * 7919u; double tilt = 1.5 - 1.2 * u;
        for (int h = 1; h <= 1023; h++) { double m = (0.2 + wt_rnd(&r)) / pow(h, tilt), ph = 2 * P * wt_rnd(&r); a[h] = m * cos(ph); b[h] = m * sin(ph); }
        return 1023; }
    case 5: {                            // DIGITAL: a sine through fewer levels and fewer steps
        int lev = 32 >> (f / 4), hold = 32 << (f % 4);
        for (int i = 0; i < WT_LEN; i++) { int j = i / hold * hold; double y = sin(2 * P * j / WT_LEN) + 0.3 * sin(6 * P * j / WT_LEN); x[i] = (float)(floor(y * lev * 0.77 + 0.5) / lev); }
        return 0; }
    case 6: {                            // SYNC: a saw hard-synced at 1 .. 8 times the cycle
        double r = 1 + 7 * u;
        for (int i = 0; i < WT_LEN; i++) { double t = (double)i / WT_LEN * r; x[i] = (float)(2 * (t - floor(t)) - 1); }
        return 0; }
    case 7: {                            // FM: a sine phase-modulated by its octave, index 0 .. 5
        for (int i = 0; i < WT_LEN; i++) { double t = (double)i / WT_LEN; x[i] = (float)sin(2 * P * t + 5 * u * sin(4 * P * t)); }
        return 0; }
    case 8: {                            // FOLD: a sine folded back on itself, gain 1 .. 8
        double g = 1 + 7 * u;
        for (int i = 0; i < WT_LEN; i++) x[i] = (float)sin(P / 2 * g * sin(2 * P * i / WT_LEN));
        return 0; }
    case 9: {                            // ORGAN: drawbars (16' as the fundamental) pulled out one more per frame
        static const int hb[9] = {1, 2, 3, 4, 6, 8, 10, 12, 16};
        for (int j = 0; j <= f && j < 9; j++) b[hb[j]] += 1 - j * 0.07;
        return 1023; }
    case 10: {                           // COMB: a saw through a moving comb (a phaser-like sweep of notches)
        double d = 0.02 + 0.28 * u;
        for (int h = 1; h <= 1023; h++) b[h] = -2 / (P * h) * fabs(cos(P * h * d));
        return 1023; }
    default: {                           // RESO: a decaying burst at 1 .. 16 times the cycle (a resonant sweep)
        double r = 1 + 15 * u;
        for (int i = 0; i < WT_LEN; i++) { double t = (double)i / WT_LEN; x[i] = (float)((1 - t) * sin(2 * P * r * t)); }
        return 0; }
    }
}
// Builds built-in table k into its slot (about 0.6 MB a table at 16 frames). Returns 0 when out of memory.
static int wt_build(WaveStore *ws, int k) {
    static double a[1025], b[1025]; static float x[WT_LEN];
    WaveTable *t = &ws->t[k];
    if (t->d) return 1;
    if (!wt_alloc(t, wt_bnf[k], wt_bnames[k])) return 0;
    for (int f = 0; f < t->nf; f++) {
        int H = wt_gen(k, f, t->nf, a, b, x);
        if (H) wt_frame_spec(t, f, a, b, H, 1); else wt_frame_time(t, f, x, WT_LEN, 1);
    }
    return 1;
}
static int wt_build_all(WaveStore *ws) { int ok = 1; for (int k = 0; k < WT_NBUILT; k++) ok &= wt_build(ws, k); return ok; }
static void wt_store_free(WaveStore *ws) { for (int k = 0; k < WT_SLOTS; k++) wt_free(&ws->t[k]); }
// A table from raw frames of 2048 samples (no normalising unless norm; a peak over 1 is always brought to 1).
static int wt_from_frames(WaveTable *t, const float *x, int nf, const char *name, int norm) {
    if (nf < 1) return 0;
    if (nf > WT_MAXF) nf = WT_MAXF;
    if (!wt_alloc(t, nf, name)) return 0;
    for (int f = 0; f < nf; f++) wt_frame_time(t, f, x + (size_t)f * WT_LEN, WT_LEN, norm);
    float pk = 0; for (size_t i = 0; i < (size_t)nf * WT_STRIDE; i++) if (fabsf(t->d[i]) > pk) pk = fabsf(t->d[i]);
    if (pk > 1) for (size_t i = 0; i < (size_t)nf * WT_STRIDE; i++) t->d[i] /= pk;
    return nf;
}
static uint32_t wt_u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static unsigned wt_u16(const uint8_t *p) { return p[0] | p[1] << 8; }
// A user WAV: mono (stereo is mixed down), PCM 8/16/24/32-bit or float 32/64. The frame size is 2048, or what a
// 'clm ' chunk says ("<!>" then the size), or the whole file when it is one power-of-two cycle shorter than 2048.
// Returns the frames loaded, or < 0: -1 file, -2 not a WAV, -3 format, -4 length not whole frames, -5 memory.
static int wt_load_wav(WaveTable *t, const char *path) {
    FILE *fp = fopen(path, "rb"); if (!fp) return -1;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    if (sz < 44 || sz > (64L << 20)) { fclose(fp); return -2; }
    uint8_t *m = (uint8_t *)malloc(sz); if (!m) { fclose(fp); return -5; }
    if (fread(m, 1, sz, fp) != (size_t)sz) { fclose(fp); free(m); return -1; }
    fclose(fp);
    int rc = -2, fmt = 0, ch = 0, bits = 0, clm = 0; const uint8_t *dat = NULL; uint32_t dn = 0;
    if (!memcmp(m, "RIFF", 4) && !memcmp(m + 8, "WAVE", 4)) {
        for (long p = 12; p + 8 <= sz;) {
            uint32_t n = wt_u32(m + p + 4); const uint8_t *c = m + p + 8;
            if (n > (uint32_t)(sz - p - 8)) n = (uint32_t)(sz - p - 8);
            if (!memcmp(m + p, "fmt ", 4) && n >= 16) { fmt = wt_u16(c); ch = wt_u16(c + 2); bits = wt_u16(c + 14); if (fmt == 0xFFFE && n >= 26) fmt = wt_u16(c + 24); }
            else if (!memcmp(m + p, "data", 4)) { dat = c; dn = n; }
            else if (!memcmp(m + p, "clm ", 4)) {   // "<!>" then the frame size in digits
                for (uint32_t i = 0; i + 3 < n; i++) if (c[i] == '<' && c[i + 1] == '!' && c[i + 2] == '>') {
                    int v = 0; for (uint32_t j = i + 3; j < n && j < i + 9 && c[j] >= '0' && c[j] <= '9'; j++) v = v * 10 + c[j] - '0';
                    clm = v; break; }
            }
            p += 8 + n + (n & 1);
        }
        rc = -3;
        int bps = bits / 8;
        if (dat && ch >= 1 && ch <= 8 && ((fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && (bits == 32 || bits == 64)))) {
            long len = dn / (bps * ch); int S = clm ? clm : WT_LEN;
            if (!clm && len < WT_LEN && len >= 32 && !(len & (len - 1))) S = (int)len;
            rc = -4;
            if (S >= 32 && S <= 4096 && !(S & (S - 1)) && len >= S && len % S == 0) {
                int nf = (int)(len / S); if (nf > WT_MAXF) nf = WT_MAXF;
                float *x = (float *)malloc((size_t)nf * S * sizeof(float)); rc = -5;
                if (x) {
                    for (long i = 0; i < (long)nf * S; i++) {
                        double s = 0;
                        for (int k = 0; k < ch; k++) {
                            const uint8_t *q = dat + (i * ch + k) * bps;
                            if (fmt == 3) { if (bits == 32) { float v; memcpy(&v, q, 4); s += v; } else { double v; memcpy(&v, q, 8); s += v; } }
                            else if (bits == 8) s += (q[0] - 128) / 128.0;
                            else if (bits == 16) s += (int16_t)wt_u16(q) / 32768.0;
                            else if (bits == 24) s += (double)((int32_t)((uint32_t)q[0] << 8 | (uint32_t)q[1] << 16 | (uint32_t)q[2] << 24) >> 8) / 8388608.0;
                            else s += (int32_t)wt_u32(q) / 2147483648.0;
                        }
                        x[i] = (float)(s / ch);
                    }
                    const char *nm = strrchr(path, '/'); nm = nm ? nm + 1 : path;
                    char name[24]; snprintf(name, sizeof name, "%s", nm); char *dot = strrchr(name, '.'); if (dot) *dot = 0;
                    if (wt_alloc(t, nf, name)) {
                        for (int f = 0; f < nf; f++) wt_frame_time(t, f, x + (size_t)f * S, S, 0);
                        float pk = 0; for (size_t i = 0; i < (size_t)nf * WT_STRIDE; i++) if (fabsf(t->d[i]) > pk) pk = fabsf(t->d[i]);
                        if (pk > 1) for (size_t i = 0; i < (size_t)nf * WT_STRIDE; i++) t->d[i] /= pk;
                        rc = nf;
                    }
                    free(x);
                }
            }
        }
    }
    free(m);
    return rc;
}
// Writes a table's full-band frames as a WAV with a 'clm ' chunk. bits 16, 24, 32 (PCM) or flt 1 (float 32).
static int wt_save_wav(const WaveTable *t, const char *path, int bits, int flt) {
    if (!t->d) return 0;
    if (flt) bits = 32;
    FILE *fp = fopen(path, "wb"); if (!fp) return 0;
    uint32_t n = (uint32_t)t->nf * WT_LEN, bps = bits / 8, dn = n * bps; const char clm[] = "<!>2048 BrickPad WAVE  ";   // 24 bytes with the NUL
    uint8_t h[12 + 24 + 8 + 24 + 8];
#define WT_P32(p, v) do { uint32_t v_ = (v); (p)[0] = v_; (p)[1] = v_ >> 8; (p)[2] = v_ >> 16; (p)[3] = v_ >> 24; } while (0)
    memcpy(h, "RIFF", 4); WT_P32(h + 4, 4 + 24 + 8 + 24 + 8 + dn); memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4); WT_P32(h + 16, 16); h[20] = flt ? 3 : 1; h[21] = 0; h[22] = 1; h[23] = 0;
    WT_P32(h + 24, SR); WT_P32(h + 28, SR * bps); h[32] = (uint8_t)bps; h[33] = 0; h[34] = (uint8_t)bits; h[35] = 0;
    memcpy(h + 36, "clm ", 4); WT_P32(h + 40, 24); memcpy(h + 44, clm, 24);
    memcpy(h + 68, "data", 4); WT_P32(h + 72, dn);
    fwrite(h, 1, sizeof h, fp);
    for (int f = 0; f < t->nf; f++) {
        const float *x = t->d + (size_t)f * WT_STRIDE + 1;
        for (int i = 0; i < WT_LEN; i++) {
            uint8_t q[4]; double v = x[i]; if (v > 1) v = 1; if (v < -1) v = -1;
            if (flt) { float y = (float)v; memcpy(q, &y, 4); }
            else if (bits == 16) { long s = lrint(v * 32767); q[0] = (uint8_t)s; q[1] = (uint8_t)(s >> 8); }
            else if (bits == 24) { long s = lrint(v * 8388607); q[0] = (uint8_t)s; q[1] = (uint8_t)(s >> 8); q[2] = (uint8_t)(s >> 16); }
            else { int64_t s = llrint(v * 2147483647.0); WT_P32(q, (uint32_t)s); }
            fwrite(q, 1, bps, fp);
        }
    }
#undef WT_P32
    return fclose(fp) == 0;
}

// ---------- parameters (int8_t wp[WP_N], kept like Track.sp / Song.gx) ----------
// OSC A at 0, OSC B at 16, each WO_* from there. Ranges, defaults and names: wp_info[] below.
enum { WO_ON, WO_TABLE, WO_POS, WO_OCT, WO_SEMI, WO_FINE, WO_LEVEL, WO_PAN, WO_PHASE, WO_RAND, WO_UNI, WO_DET,
       WO_WIDTH, WO_WARP, WO_WAMT, WO_N };
enum { WE_ATT, WE_DEC, WE_SUS, WE_REL, WE_CURVE, WE_N };
enum { WL_SHAPE, WL_RATE, WL_SYNC, WL_TRIG, WL_PHASE, WL_N };
enum { WP_A = 0, WP_B = 16,
       WP_SUB_LEVEL = 32, WP_SUB_SHAPE, WP_SUB_OCT, WP_SUB_PAN, WP_SUB_DIRECT, WP_NOISE_LEVEL, WP_NOISE_TYPE, WP_NOISE_DIRECT,
       WP_F_TYPE = 40, WP_F_CUT, WP_F_RES, WP_F_DRIVE, WP_F_KEY, WP_F_ENV,
       WP_ENV1 = 48, WP_ENV2 = 53, WP_ENV3 = 58,
       WP_LFO1 = 64, WP_LFO2 = 69, WP_LFO3 = 74, WP_LFO4 = 79,
       WP_POLY = 84, WP_LEGATO, WP_GLIDE, WP_BEND, WP_VELSENS, WP_MASTER, WP_QUALITY, WP_MACRO1, WP_MACRO2, WP_MACRO3,
       WP_MACRO4, WP_MODWHEEL, WP_NP,     // 96: the sound's own parameters (mod destinations come from these)
       WP_MTX = 128,                     // 16 slots x {SRC, DST, AMT, MODE}
       WP_MARK = 255, WP_N = 256 };
#define WP_INIT 0x57
enum { WM_SRC, WM_DST, WM_AMT, WM_MODE };   // DST = parameter index + 1 (0 none); AMT -100..100 of its range; MODE 0 UNI 1 BI
#define WM_SLOTS 16
enum { WS_NONE, WS_ENV1, WS_ENV2, WS_ENV3, WS_LFO1, WS_LFO2, WS_LFO3, WS_LFO4, WS_VEL, WS_NOTE, WS_MACRO1, WS_MACRO2,
       WS_MACRO3, WS_MACRO4, WS_MW, WS_N };
enum { WW_OFF, WW_SYNC, WW_BENDP, WW_BENDM, WW_PWM, WW_ASYMP, WW_ASYMM, WW_FLIP, WW_MIRROR, WW_QUANT, WW_FM, WW_AM, WW_RM, WW_N };
enum { WF_OFF, WF_LP12, WF_LP24, WF_HP12, WF_BP, WF_NOTCH, WF_LADDER, WF_N };
enum { WLS_SINE, WLS_TRI, WLS_SAWUP, WLS_SAWDN, WLS_SQUARE, WLS_SH, WLS_SMOOTH, WLS_N };
#define WV_NSYNC 13
static const float wv_sync_beats[WV_NSYNC] = {0, 0.125f, 1 / 6.f, 0.25f, 1 / 3.f, 0.5f, 2 / 3.f, 1, 2, 4, 8, 16, 32};
static const char *const wv_sync_names[WV_NSYNC] = {"FREE", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 BAR", "2 BAR", "4 BAR", "8 BAR"};
static const char *const wv_warp_names[WW_N] = {"OFF", "SYNC", "BEND+", "BEND-", "PWM", "ASYM+", "ASYM-", "FLIP", "MIRROR", "QUANT", "FM", "AM", "RM"};
static const char *const wv_filt_names[WF_N] = {"OFF", "LP12", "LP24", "HP12", "BP", "NOTCH", "LADDER"};
static const char *const wv_sub_names[4] = {"SINE", "TRI", "SQUARE", "SAW"};
static const char *const wv_lfo_names[WLS_N] = {"SINE", "TRI", "SAW UP", "SAW DN", "SQUARE", "S&H", "SMOOTH"};
static const char *const wv_trig_names[3] = {"FREE", "RETRIG", "ONE"};
static const char *const wv_src_names[WS_N] = {"--", "ENV1", "ENV2", "ENV3", "LFO1", "LFO2", "LFO3", "LFO4", "VELO", "NOTE", "MACRO1", "MACRO2", "MACRO3", "MACRO4", "MODWHL"};
typedef struct { const char *name; int8_t lo, hi, def, mod; } WpInfo;   // mod: may be a matrix destination
#define WPI(k, n, l, h, d, m) [k] = {n, l, h, d, m}
#define WP_OSC(B, X, on) WPI(B + WO_ON, X " ON", 0, 1, on, 0), WPI(B + WO_TABLE, X " TABLE", 0, WT_SLOTS - 1, 0, 0), \
    WPI(B + WO_POS, X " POS", 0, 100, 0, 1), WPI(B + WO_OCT, X " OCT", -4, 4, 0, 1), WPI(B + WO_SEMI, X " SEMI", -24, 24, 0, 1), \
    WPI(B + WO_FINE, X " FINE", -100, 100, 0, 1), WPI(B + WO_LEVEL, X " LEVEL", 0, 100, 75, 1), WPI(B + WO_PAN, X " PAN", -50, 50, 0, 1), \
    WPI(B + WO_PHASE, X " PHASE", 0, 100, 0, 0), WPI(B + WO_RAND, X " RAND", 0, 100, 100, 0), WPI(B + WO_UNI, X " UNISON", 1, 8, 1, 0), \
    WPI(B + WO_DET, X " DETUNE", 0, 100, 25, 1), WPI(B + WO_WIDTH, X " WIDTH", 0, 100, 80, 1), WPI(B + WO_WARP, X " WARP", 0, WW_N - 1, 0, 0), \
    WPI(B + WO_WAMT, X " W.AMT", 0, 100, 0, 1)
#define WP_ENV(B, X, s) WPI(B + WE_ATT, X " ATT", 0, 100, 0, 1), WPI(B + WE_DEC, X " DEC", 0, 100, 50, 1), \
    WPI(B + WE_SUS, X " SUS", 0, 100, s, 1), WPI(B + WE_REL, X " REL", 0, 100, 35, 1), WPI(B + WE_CURVE, X " CURVE", -100, 100, 40, 1)
#define WP_LFO(B, X) WPI(B + WL_SHAPE, X " SHAPE", 0, WLS_N - 1, 0, 0), WPI(B + WL_RATE, X " RATE", 0, 100, 50, 1), \
    WPI(B + WL_SYNC, X " SYNC", 0, WV_NSYNC - 1, 0, 0), WPI(B + WL_TRIG, X " TRIG", 0, 2, 0, 0), WPI(B + WL_PHASE, X " PHASE", 0, 100, 0, 0)
static const WpInfo wp_info[WP_NP] = {
    WP_OSC(WP_A, "A", 1), WP_OSC(WP_B, "B", 0),
    WPI(WP_SUB_LEVEL, "SUB LEVEL", 0, 100, 0, 1), WPI(WP_SUB_SHAPE, "SUB SHAPE", 0, 3, 0, 0), WPI(WP_SUB_OCT, "SUB OCT", 1, 2, 1, 0),
    WPI(WP_SUB_PAN, "SUB PAN", -50, 50, 0, 1), WPI(WP_SUB_DIRECT, "SUB DIRECT", 0, 1, 0, 0),
    WPI(WP_NOISE_LEVEL, "NOISE LEVEL", 0, 100, 0, 1), WPI(WP_NOISE_TYPE, "NOISE TYPE", 0, 1, 0, 0), WPI(WP_NOISE_DIRECT, "NOISE DIRECT", 0, 1, 0, 0),
    WPI(WP_F_TYPE, "FILTER", 0, WF_N - 1, WF_LP24, 0), WPI(WP_F_CUT, "CUTOFF", 0, 100, 75, 1), WPI(WP_F_RES, "RESO", 0, 100, 15, 1),
    WPI(WP_F_DRIVE, "DRIVE", 0, 100, 0, 1), WPI(WP_F_KEY, "KEY TRK", 0, 100, 0, 1), WPI(WP_F_ENV, "ENV AMT", -100, 100, 0, 1),
    WP_ENV(WP_ENV1, "ENV1", 70), WP_ENV(WP_ENV2, "ENV2", 30), WP_ENV(WP_ENV3, "ENV3", 30),
    WP_LFO(WP_LFO1, "LFO1"), WP_LFO(WP_LFO2, "LFO2"), WP_LFO(WP_LFO3, "LFO3"), WP_LFO(WP_LFO4, "LFO4"),
    WPI(WP_POLY, "VOICES", 1, 8, 8, 0), WPI(WP_LEGATO, "LEGATO", 0, 1, 0, 0), WPI(WP_GLIDE, "GLIDE", 0, 100, 0, 1),
    WPI(WP_BEND, "BEND", 0, 24, 2, 0), WPI(WP_VELSENS, "VELO", 0, 100, 60, 1), WPI(WP_MASTER, "MASTER", 0, 100, 70, 1),
    WPI(WP_QUALITY, "QUALITY", 0, 1, 0, 0), WPI(WP_MACRO1, "MACRO 1", 0, 100, 0, 0), WPI(WP_MACRO2, "MACRO 2", 0, 100, 0, 0),
    WPI(WP_MACRO3, "MACRO 3", 0, 100, 0, 0), WPI(WP_MACRO4, "MACRO 4", 0, 100, 0, 0), WPI(WP_MODWHEEL, "MODWHEEL", 0, 100, 0, 0),
};
#define WM_NAMES(n) "M" #n " SRC", "M" #n " DEST", "M" #n " AMT", "M" #n " MODE"
static const char *const wp_mnames[WM_SLOTS * 4] = {WM_NAMES(1), WM_NAMES(2), WM_NAMES(3), WM_NAMES(4), WM_NAMES(5), WM_NAMES(6),
    WM_NAMES(7), WM_NAMES(8), WM_NAMES(9), WM_NAMES(10), WM_NAMES(11), WM_NAMES(12), WM_NAMES(13), WM_NAMES(14), WM_NAMES(15), WM_NAMES(16)};
// A parameter's name for the UI (NULL: a spare byte).
static const char *wp_name(int k) {
    if (k >= 0 && k < WP_NP) return wp_info[k].name;
    if (k >= WP_MTX && k < WP_MTX + WM_SLOTS * 4) return wp_mnames[k - WP_MTX];
    return NULL;
}
static void wp_range(int k, int *lo, int *hi) {
    *lo = *hi = 0;
    if (k >= 0 && k < WP_NP) { if (wp_info[k].name) { *lo = wp_info[k].lo; *hi = wp_info[k].hi; } return; }
    if (k >= WP_MTX && k < WP_MTX + WM_SLOTS * 4) switch ((k - WP_MTX) & 3) {
        case WM_SRC: *hi = WS_N - 1; break; case WM_DST: *hi = WP_NP; break; case WM_AMT: *lo = -100; *hi = 100; break; default: *hi = 1; }
}
static void wp_defaults(int8_t *wp) {
    memset(wp, 0, WP_N);
    for (int k = 0; k < WP_NP; k++) if (wp_info[k].name) wp[k] = wp_info[k].def;
    wp[WP_MARK] = WP_INIT;
}
// A value as the UI shows it. ws may be NULL (table numbers then).
static void wp_str(int k, int v, char *b, int n, const WaveStore *ws) {
    int o = k < WP_SUB_LEVEL ? k & 15 : -1;
    if (o == WO_TABLE) { if (ws && v >= 0 && v < WT_SLOTS && ws->t[v].d && ws->t[v].name[0]) snprintf(b, n, "%s", ws->t[v].name); else if (v < WT_NBUILT) snprintf(b, n, "%s", wt_bnames[v]); else snprintf(b, n, "USER %d", v - WT_NBUILT + 1); }
    else if (o == WO_WARP) snprintf(b, n, "%s", wv_warp_names[v % WW_N]);
    else if (o == WO_ON) snprintf(b, n, v ? "ON" : "OFF");
    else if (o == WO_OCT) snprintf(b, n, "%+d OCT", v);
    else if (o == WO_FINE) snprintf(b, n, "%+d CT", v);
    else if (k == WP_F_TYPE) snprintf(b, n, "%s", wv_filt_names[v % WF_N]);
    else if (k == WP_SUB_SHAPE) snprintf(b, n, "%s", wv_sub_names[v & 3]);
    else if (k == WP_SUB_OCT) snprintf(b, n, "-%d OCT", v);
    else if (k == WP_NOISE_TYPE) snprintf(b, n, v ? "PINK" : "WHITE");
    else if (k == WP_SUB_DIRECT || k == WP_NOISE_DIRECT || k == WP_LEGATO) snprintf(b, n, v ? "ON" : "OFF");
    else if (k == WP_QUALITY) snprintf(b, n, v ? "HIGH" : "ECO");
    else if (k == WP_POLY) { if (v <= 1) snprintf(b, n, "MONO"); else snprintf(b, n, "%d", v); }
    else if (k >= WP_LFO1 && k < WP_POLY && (k - WP_LFO1) % WL_N == WL_SHAPE) snprintf(b, n, "%s", wv_lfo_names[v % WLS_N]);
    else if (k >= WP_LFO1 && k < WP_POLY && (k - WP_LFO1) % WL_N == WL_SYNC) snprintf(b, n, "%s", wv_sync_names[v % WV_NSYNC]);
    else if (k >= WP_LFO1 && k < WP_POLY && (k - WP_LFO1) % WL_N == WL_TRIG) snprintf(b, n, "%s", wv_trig_names[v % 3]);
    else if (k >= WP_MTX && k < WP_MTX + WM_SLOTS * 4) {
        int f = (k - WP_MTX) & 3;
        if (f == WM_SRC) snprintf(b, n, "%s", wv_src_names[v % WS_N]);
        else if (f == WM_DST) snprintf(b, n, "%s", v > 0 && v <= WP_NP && wp_info[v - 1].mod ? wp_info[v - 1].name : "--");
        else if (f == WM_MODE) snprintf(b, n, v ? "BI" : "UNI");
        else snprintf(b, n, "%+d", v);
    }
    else snprintf(b, n, "%d", v);
}
static inline float wv_time(float x) { return 0.0005f * powf(20000.f, x * 0.01f); }      // 0..100 -> 0.5 ms .. 10 s
static inline float wv_lfo_hz(float x) { return 0.02f * powf(2500.f, x * 0.01f); }    // 0..100 -> 0.02 .. 50 Hz
static inline float wv_glide_s(float x) { x *= 0.01f; return x * x * 2; }            // 0..100 -> 0 .. 2 s
static inline float wv_cut_hz(float c) { return 20.f * exp2f(c * 0.1f); }            // 0..100 -> 20 Hz .. 20.5 kHz

// ---------- the voice ----------
#ifndef WV_BLK
#define WV_BLK 16                        // control rate: envelopes, LFOs, the matrix and coefficients every 16 frames
#endif
#define WV_UNI 8
#define WV_POLY 8
typedef struct { uint8_t st; float t, from, lvl; } WvEnv;   // st 0 idle 1 attack 2 decay 3 sustain 4 release
typedef struct { float ph, sh, prev, out; } WvLfo;
// An envelope's settings worked out for one block: rates are dt / time; k and kn give the curve.
typedef struct { float ra, rd, rr, s, k, kn; } WvEnvK;

typedef struct {
    const float *f0, *f1;                // frame i0 and i0 + 1 at the level in use (f1 NULL: a one-frame table)
    int sh, i0, nu, warp;                // 32 - log2(level size); unison voices (0 = the oscillator is off)
    uint32_t ph[WV_UNI], inc[WV_UNI];
    float gl[WV_UNI], gr[WV_UNI];        // each unison voice's pan gains
    float mf, dmf, lg, dlg;              // morph between the two frames and its ramp; level and its ramp
    float wa, w1, w2;                    // warp amount 0..1 and what it works out to
    float cdet, cpan, cwid; int cnu;     // what the cached increments and pans were made for
    float ratio[WV_UNI];
} WvOsc;
typedef struct {
    uint8_t on, rel, pend, fade;         // fade: blocks left fading out a stolen voice, then the pending note starts
    int delay, gate, bpos;               // gate: frames until release, < 0 held until note off
    unsigned age;
    float note, tnote, key, vel;         // pitch now (semitones, glides), its target, the key that lets it go, 0..1
    float pnote, pvel; int pgate;        // the note waiting behind a fade
    uint32_t rng;
    WvEnv e[3]; WvLfo l[4];
    WvOsc o[2];
    float om[2][WV_BLK];                 // each oscillator's last output (FM AM RM read the other one)
    int order, hq, ft, sshape, sdir, ndir, ntype;
    uint32_t sph, sinc;
    float sl, sr, dsl, dsr, cspan, spl, spr, nl, dnl, pk[6];
    float fg, fk, fkg, fd, fd1, fG, fdg, fcomp, fs[2][4], fy[2], cfc, cres, fcl, fcb, fch; int cft;
    float amp, damp;
} WaveV;
typedef struct { uint8_t src, dst, bi, nat; float amt; } WvMod;
typedef struct {
    WaveV v[WV_POLY];
    float bend, bpm;                     // the pitch wheel -1..1 (times BEND semitones); tempo for synced LFOs
    float lph[4];                        // free-running LFO phases: a new voice joins them
    unsigned age; uint32_t rng;
    float held[16]; int nheld;           // mono: keys still down, newest last
    float last;                          // the last note played: GLIDE starts there
    float base[WP_NP]; WvMod mods[WM_SLOTS]; int nmods;   // worked out once per buffer:
    uint8_t md[WP_NP];                   // the parameters some matrix slot moves (only those are worked out per voice)
    WvEnvK ek[3]; float lhz[4];          // the envelopes and LFO rates as the knobs set them
} WaveInst;
static void wv_init(WaveInst *w) { memset(w, 0, sizeof *w); w->bpm = 120; w->last = -1000; w->rng = 0x2545F491u; }

static inline float wv_sat(float x) { if (x > 3) return 1; if (x < -3) return -1; float x2 = x * x; return x * (27 + x2) / (27 + 9 * x2); }
static inline float wv_sin1(float x) {   // sin(2 pi x), x in [0, 1): folded to a quarter wave, then a Taylor series (err < 2e-4)
    float q = x < 0.25f ? 4 * x : x < 0.75f ? 2 - 4 * x : 4 * x - 4, q2 = q * q;
    return q * (1.5707963f - q2 * (0.6459641f - q2 * (0.0796926f - q2 * 0.0046818f)));
}
static inline float wv_blep(float t, float dt) {   // the polynomial correction at a jump (PolyBLEP)
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}
static void wv_envk(WvEnvK *K, float a, float d, float s, float r, float c, float dt) {
    K->ra = dt / a; K->rd = dt / d; K->rr = dt / r; K->s = s;
    K->k = c > -0.01f && c < 0.01f ? 0 : c * 6; K->kn = K->k ? 1 / (1 - expf(-K->k)) : 1;
}
static inline float wv_curve(float t, const WvEnvK *K) {   // segments: curve > 0 starts fast (as an RC), < 0 starts slow
    return K->k ? (1 - expf(-K->k * t)) * K->kn : t;
}
static void wv_env_step(WvEnv *e, const WvEnvK *K) {
    float s = K->s;
    switch (e->st) {
    case 1: e->t += K->ra;
        if (e->t < 1) { e->lvl = e->from + (1 - e->from) * wv_curve(e->t, K); break; }
        e->t = (e->t - 1) * K->rd / K->ra; e->st = 2;   // the time left over goes on into the decay
        if (e->t >= 1) { e->st = 3; e->lvl = s; } else e->lvl = 1 - (1 - s) * wv_curve(e->t, K);
        break;
    case 2: e->t += K->rd;
        if (e->t >= 1) { e->st = 3; e->lvl = s; } else e->lvl = 1 - (1 - s) * wv_curve(e->t, K);
        break;
    case 3: e->lvl = s; break;
    case 4: e->t += K->rr;
        if (e->t >= 1) { e->st = 0; e->lvl = 0; } else e->lvl = e->from * (1 - wv_curve(e->t, K));
        break;
    default: e->lvl = 0;
    }
}
static inline void wv_env_go(WvEnv *e) { e->from = e->lvl; e->t = 0; e->st = 1; }
static inline void wv_env_rel(WvEnv *e) { if (e->st) { e->from = e->lvl; e->t = 0; e->st = 4; } }
static float wv_lfo_out(const WvLfo *l, int shape, float off) {
    float p = l->ph + off; p -= floorf(p);
    switch (shape) {
    case WLS_TRI: return 1 - 4 * fabsf(p - 0.5f);
    case WLS_SAWUP: return 2 * p - 1;
    case WLS_SAWDN: return 1 - 2 * p;
    case WLS_SQUARE: return p < 0.5f ? 1.f : -1.f;
    case WLS_SH: return l->sh;
    case WLS_SMOOTH: return l->prev + (l->sh - l->prev) * (0.5f - 0.5f * cosf((float)M_PI * p));
    default: return wv_sin1(p);
    }
}
static inline float wv_rnd(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return (float)(*s >> 8) * (1.f / 16777216.f); }
static inline uint32_t wv_inc(float hz) { double d = hz / (double)SR; if (d > 0.49) d = 0.49; if (d < 0) d = 0; return (uint32_t)(d * 4294967296.0); }
static inline float wv_hz(float note) { return 440.f * exp2f((note - 69) * (1.f / 12)); }

// The values one oscillator needs for the next block (p: the parameters with modulation).
static void wv_osc_ctl(WaveV *v, int o, const float *p, const int8_t *wp, const WaveStore *ws, float note) {
    WvOsc *c = &v->o[o]; const float *q = p + o * 16;
    int t = wp[o * 16 + WO_TABLE]; const WaveTable *tb = t >= 0 && t < WT_SLOTS ? &ws->t[t] : NULL;
    if (!wp[o * 16 + WO_ON] || !tb || !tb->d) { c->nu = 0; return; }
    int nu = wp[o * 16 + WO_UNI]; if (nu < 1) nu = 1; if (nu > WV_UNI) nu = WV_UNI;
    float f = wv_hz(note + q[WO_OCT] * 12 + q[WO_SEMI] + q[WO_FINE] * 0.01f), det = nu > 1 ? q[WO_DET] * 0.5f : 0;   // DETUNE 100 = +-50 cents
    int warp = wp[o * 16 + WO_WARP]; float wa = q[WO_WAMT] * 0.01f, wf = 1;
    if (wa <= 0) warp = WW_OFF;
    switch (warp) {                      // what the warp does to the top harmonic: the level is chosen for it
    case WW_SYNC: c->w1 = 1 + 15 * wa; wf = c->w1; break;
    case WW_BENDP: case WW_BENDM: c->w1 = (warp == WW_BENDP ? 0.9f : -0.9f) * wa / (2 * (float)M_PI); wf = 1 + 0.9f * wa; break;
    case WW_PWM: c->w1 = 1 - 0.95f * wa; c->w2 = 1 / c->w1; wf = c->w2; break;
    case WW_ASYMP: case WW_ASYMM: c->w1 = 0.5f + (warp == WW_ASYMP ? -0.45f : 0.45f) * wa;
        c->w2 = 0.5f / c->w1; wf = 0.5f / (c->w1 < 0.5f ? c->w1 : 1 - c->w1); break;
    case WW_FLIP: c->w1 = 1 - wa; break;
    case WW_QUANT: c->w1 = exp2f(1 + 7 * (1 - wa)); c->w2 = 1 / c->w1; break;
    case WW_FM: c->w1 = wa; wf = 1 + 2 * wa; break;
    default: break;
    }
    c->warp = warp; c->wa = wa;
    float top = f * exp2f(det * (1.f / 1200)) * wf * (1024.f / WV_FLIM);
    int m = top <= 1 ? 0 : (int)ceilf(log2f(top)); if (m > WT_NLV - 1) m = WT_NLV - 1;
    c->sh = 32 - wt_lbits[m];
    int nf = tb->nf, i0 = 0; float mf = 0;
    if (nf > 1) { float fp = q[WO_POS] * 0.01f * (nf - 1); i0 = (int)fp; if (i0 > nf - 2) i0 = nf - 2; mf = fp - i0; }
    if (i0 != c->i0 || !c->nu) { c->i0 = i0; c->mf = mf; c->dmf = 0; } else c->dmf = (mf - c->mf) * (1.f / WV_BLK);
    c->f0 = tb->d + (size_t)i0 * WT_STRIDE + wt_loff[m] + 1; c->f1 = nf > 1 ? c->f0 + WT_STRIDE : NULL;
    float lg = q[WO_LEVEL] * 0.01f / sqrtf((float)nu);
    if (!c->nu) c->lg = lg;
    c->dlg = (lg - c->lg) * (1.f / WV_BLK);
    if (nu != c->cnu || det != c->cdet) {   // the detune ratios: only when they change
        for (int u = 0; u < nu; u++) { float d = nu > 1 ? 2.f * u / (nu - 1) - 1 : 0; c->ratio[u] = exp2f(d * det * (1.f / 1200)); }
        c->cdet = det;
    }
    for (int u = 0; u < nu; u++) c->inc[u] = wv_inc(f * c->ratio[u]);
    float pan = q[WO_PAN] * 0.02f, wid = q[WO_WIDTH] * 0.01f;
    if (nu != c->cnu || pan != c->cpan || wid != c->cwid) {   // the unison pans likewise
        for (int u = 0; u < nu; u++) {
            float d = nu > 1 ? 2.f * u / (nu - 1) - 1 : 0, x = pan + d * wid; if (x < -1) x = -1; if (x > 1) x = 1;
            float a = (x + 1) * (float)M_PI / 4; c->gl[u] = cosf(a) * 1.4142136f; c->gr[u] = sinf(a) * 1.4142136f;
        }
        c->cpan = pan; c->cwid = wid;
    }
    c->cnu = nu; c->nu = nu;
}
// One stretch (n <= WV_BLK) of one oscillator, added into L/R; mono gets its plain output (before level and pan).
static inline float wv_rd(const float *t, int i, float fr, int hq) {
    if (!hq) return t[i] + (t[i + 1] - t[i]) * fr;
    float xm = t[i - 1], x0 = t[i], x1 = t[i + 1], x2 = t[i + 2];   // 4-point cubic Hermite (Catmull-Rom)
    float c1 = 0.5f * (x1 - xm), c2 = xm - 2.5f * x0 + 2 * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
    return ((c3 * fr + c2) * fr + c1) * fr + x0;
}
static void wv_osc_run(WvOsc *c, int n, int hq, float *L, float *R, float *mono, const float *oth) {
    const float *a = c->f0, *b = c->f1; int sh = c->sh, N = 1 << (32 - sh); uint32_t msk = 0xFFFFFFFFu >> (32 - sh);
    float fs = ldexpf(1.f, -sh), mf0 = c->mf, dmf = c->dmf, lg0 = c->lg, dlg = c->dlg, inu = 1.f / c->nu;
    for (int i = 0; i < n; i++) mono[i] = 0;
    if (b && mf0 == 0 && dmf == 0) b = NULL;   // sitting on a frame: one read
    for (int u = 0; u < c->nu; u++) {
        uint32_t ph = c->ph[u], inc = c->inc[u]; float gl = c->gl[u], gr = c->gr[u];
        if (c->warp == WW_OFF) {
            if (!hq && !b) for (int i = 0; i < n; i++) {
                int j = (int)(ph >> sh); float fr = (float)(ph & msk) * fs, y = a[j] + (a[j + 1] - a[j]) * fr;
                ph += inc; mono[i] += y; y *= lg0 + dlg * i; L[i] += y * gl; R[i] += y * gr;
            }
            else if (!hq) for (int i = 0; i < n; i++) {
                int j = (int)(ph >> sh); float fr = (float)(ph & msk) * fs, y0 = a[j] + (a[j + 1] - a[j]) * fr, y1 = b[j] + (b[j + 1] - b[j]) * fr;
                float y = y0 + (y1 - y0) * (mf0 + dmf * i);
                ph += inc; mono[i] += y; y *= lg0 + dlg * i; L[i] += y * gl; R[i] += y * gr;
            }
            else for (int i = 0; i < n; i++) {
                int j = (int)(ph >> sh); float fr = (float)(ph & msk) * fs, y = wv_rd(a, j, fr, 1);
                if (b) y += (wv_rd(b, j, fr, 1) - y) * (mf0 + dmf * i);
                ph += inc; mono[i] += y; y *= lg0 + dlg * i; L[i] += y * gl; R[i] += y * gr;
            }
        } else for (int i = 0; i < n; i++) {   // WARP: the phase is bent (or the output changed) sample by sample
            float x = (float)(ph >> 8) * (1.f / 16777216.f), x2 = -1, mf = mf0 + dmf * i;
            switch (c->warp) {
            case WW_SYNC: x *= c->w1; x -= (int)x; break;
            case WW_BENDP: case WW_BENDM: x -= c->w1 * wv_sin1(x); if (x < 0) x = 0; if (x >= 1) x = 0.9999999f; break;
            case WW_PWM: x = x < c->w1 ? x * c->w2 : 0; break;
            case WW_ASYMP: case WW_ASYMM: x = x < c->w1 ? x * c->w2 : 0.5f + (x - c->w1) * (0.5f / (1 - c->w1)); break;
            case WW_FM: x += c->w1 * oth[i]; x -= floorf(x); break;
            case WW_MIRROR: x2 = 1 - x; if (x2 >= 1) x2 = 0; break;
            default: break;
            }
            float fi = x * N; int j = (int)fi; float fr = fi - j; j &= N - 1;
            float y = wv_rd(a, j, fr, hq); if (b) y += (wv_rd(b, j, fr, hq) - y) * mf;
            if (x2 >= 0) { fi = x2 * N; j = (int)fi; fr = fi - j; j &= N - 1;
                float z = wv_rd(a, j, fr, hq); if (b) z += (wv_rd(b, j, fr, hq) - z) * mf; y += (z - y) * c->wa; }
            switch (c->warp) {
            case WW_FLIP: if (x >= c->w1) y = -y; break;
            case WW_QUANT: y = floorf(y * c->w1 + 0.5f) * c->w2; break;
            case WW_AM: y *= 1 - c->wa * 0.5f * (1 - oth[i]); break;
            case WW_RM: y *= 1 - c->wa + c->wa * oth[i]; break;
            default: break;
            }
            ph += inc; mono[i] += y; y *= lg0 + dlg * i; L[i] += y * gl; R[i] += y * gr;
        }
        c->ph[u] = ph;
    }
    for (int i = 0; i < n; i++) mono[i] *= inu;
    c->mf = mf0 + dmf * n; c->lg = lg0 + dlg * n;
}

// A voice starts: fresh (from silence) or again (envelopes from where they are, phases go on).
static void wv_vstart(WaveInst *w, WaveV *v, const int8_t *wp, float note, float vel, int gate, int fresh) {
    if (fresh) {
        memset(v, 0, sizeof *v);
        w->rng = w->rng * 1664525u + 1013904223u; v->rng = w->rng ^ 0x9E3779B9u;
        for (int o = 0; o < 2; o++) {
            float ph0 = wp[o * 16 + WO_PHASE] * 0.01f, rnd = wp[o * 16 + WO_RAND] * 0.01f;
            for (int u = 0; u < WV_UNI; u++) { float x = ph0 + rnd * wv_rnd(&v->rng); x -= floorf(x); v->o[o].ph[u] = (uint32_t)(x * 4294967295.0); }
            v->o[o].i0 = -1;
        }
        for (int j = 0; j < 4; j++) { v->l[j].ph = wp[WP_LFO1 + j * WL_N + WL_TRIG] ? 0 : w->lph[j]; v->l[j].sh = wv_rnd(&v->rng) * 2 - 1; v->l[j].prev = wv_rnd(&v->rng) * 2 - 1; }
        v->note = note; v->on = 1; v->sph = 0; v->cspan = 2;
    } else for (int j = 0; j < 4; j++) if (wp[WP_LFO1 + j * WL_N + WL_TRIG]) v->l[j].ph = 0;
    for (int j = 0; j < 3; j++) wv_env_go(&v->e[j]);
    if (wp[WP_GLIDE] <= 0) v->note = note;
    v->tnote = note; v->key = note; v->vel = vel; v->gate = gate; v->rel = 0; v->age = ++w->age;
}
static void wv_vrelease(WaveV *v) { if (v->on && !v->rel) { v->rel = 1; for (int j = 0; j < 3; j++) wv_env_rel(&v->e[j]); } }

// Once per block: modulation, envelopes, LFOs, glide, every coefficient. Returns 0 when the voice has finished.
static int wv_ctl(WaveV *v, WaveInst *w, const int8_t *wp, const WaveStore *ws) {
    if (v->pend && !v->fade) {           // the stolen voice is quiet now: its new note starts
        float pn = v->pnote, pv = v->pvel; int pg = v->pgate;
        wv_vstart(w, v, wp, pn, pv, pg, 1);
        if (wp[WP_GLIDE] > 0 && w->last > -999) v->note = w->last;
    }
    float p[WP_NP]; memcpy(p, w->base, sizeof p);
    if (w->nmods) {                      // the matrix, from the sources as the last block left them
        float s[WS_N];
        s[WS_NONE] = 0; s[WS_ENV1] = v->e[0].lvl; s[WS_ENV2] = v->e[1].lvl; s[WS_ENV3] = v->e[2].lvl;
        for (int j = 0; j < 4; j++) s[WS_LFO1 + j] = v->l[j].out;
        s[WS_VEL] = v->vel; s[WS_NOTE] = (v->key - 60) * (1.f / 48);
        for (int j = 0; j < 4; j++) s[WS_MACRO1 + j] = w->base[WP_MACRO1 + j] * 0.01f;
        s[WS_MW] = w->base[WP_MODWHEEL] * 0.01f;
        for (int i = 0; i < w->nmods; i++) {
            const WvMod *m = &w->mods[i]; float x = s[m->src];
            if (m->bi && !m->nat) x = 2 * x - 1; else if (!m->bi && m->nat) x = 0.5f * x + 0.5f;
            p[m->dst] += m->amt * x;
        }
        for (int i = 0; i < w->nmods; i++) { int k = w->mods[i].dst; if (p[k] < wp_info[k].lo) p[k] = wp_info[k].lo; if (p[k] > wp_info[k].hi) p[k] = wp_info[k].hi; }
    }
    const float dt = (float)WV_BLK / SR;
    for (int j = 0; j < 3; j++) {
        const float *e = p + WP_ENV1 + j * WE_N; const uint8_t *m = w->md + WP_ENV1 + j * WE_N; WvEnvK K; const WvEnvK *kp = &w->ek[j];
        if (m[0] | m[1] | m[2] | m[3] | m[4]) { wv_envk(&K, wv_time(e[WE_ATT]), wv_time(e[WE_DEC]), e[WE_SUS] * 0.01f, wv_time(e[WE_REL]), e[WE_CURVE] * 0.01f, dt); kp = &K; }
        wv_env_step(&v->e[j], kp);
    }
    for (int j = 0; j < 4; j++) {
        const float *q = p + WP_LFO1 + j * WL_N; WvLfo *l = &v->l[j]; int sy = wp[WP_LFO1 + j * WL_N + WL_SYNC], tr = wp[WP_LFO1 + j * WL_N + WL_TRIG];
        float hz = w->md[WP_LFO1 + j * WL_N + WL_RATE] && !(sy > 0 && sy < WV_NSYNC) ? wv_lfo_hz(q[WL_RATE]) : w->lhz[j];
        l->ph += hz * dt;
        if (l->ph >= 1) { if (tr == 2) l->ph = 0.99999f; else { l->ph -= floorf(l->ph); l->prev = l->sh; l->sh = wv_rnd(&v->rng) * 2 - 1; } }
        l->out = wv_lfo_out(l, wp[WP_LFO1 + j * WL_N + WL_SHAPE], q[WL_PHASE] * 0.01f);
    }
    if (v->note != v->tnote) {           // GLIDE: towards the target, about 95 % of the way in the set time
        float gt = wv_glide_s(p[WP_GLIDE]);
        if (gt < 0.001f) v->note = v->tnote;
        else { v->note += (v->tnote - v->note) * (1 - expf(-3 * dt / gt)); if (fabsf(v->note - v->tnote) < 0.001f) v->note = v->tnote; }
    }
    float atg;
    if (v->fade) { v->fade--; atg = 0; }
    else if (v->e[0].st == 0) { if (v->amp <= 1e-6f) return 0; atg = 0; }
    else { float vs = p[WP_VELSENS] * 0.01f, ms = p[WP_MASTER] * 0.01f; atg = v->e[0].lvl * (1 - vs + vs * v->vel) * ms * ms; }
    v->damp = (atg - v->amp) * (1.f / WV_BLK);
    v->hq = wp[WP_QUALITY] > 0;
    float note = v->note + w->bend * wp[WP_BEND];
    for (int o = 0; o < 2; o++) wv_osc_ctl(v, o, p, wp, ws, note);
    int wa = wp[WO_WARP]; v->order = v->o[0].nu && v->o[0].warp && (wa == WW_FM || wa == WW_AM || wa == WW_RM);   // A reads B: B goes first
    // SUB: follows the note, one or two octaves down
    float sg = p[WP_SUB_LEVEL] * 0.01f, sp = p[WP_SUB_PAN] * 0.02f;
    if (sp != v->cspan) { float a = (sp + 1) * (float)M_PI / 4; v->spl = cosf(a) * 1.4142136f; v->spr = sinf(a) * 1.4142136f; v->cspan = sp; }
    v->sinc = wv_inc(wv_hz(note) / (wp[WP_SUB_OCT] >= 2 ? 4 : 2)); v->sshape = wp[WP_SUB_SHAPE] & 3; v->sdir = wp[WP_SUB_DIRECT] > 0;
    v->dsl = (sg * v->spl - v->sl) * (1.f / WV_BLK); v->dsr = (sg * v->spr - v->sr) * (1.f / WV_BLK);
    v->dnl = (p[WP_NOISE_LEVEL] * 0.01f - v->nl) * (1.f / WV_BLK); v->ntype = wp[WP_NOISE_TYPE] > 0; v->ndir = wp[WP_NOISE_DIRECT] > 0;
    // FILTER
    v->ft = wp[WP_F_TYPE]; if (v->ft < 0 || v->ft >= WF_N) v->ft = WF_OFF;
    if (v->ft) {
        float fsr = v->hq ? 2.f * SR : (float)SR;
        float fc = wv_cut_hz(p[WP_F_CUT] + p[WP_F_KEY] * 0.01f * (v->key - 60) * (10.f / 12) + p[WP_F_ENV] * 0.8f * v->e[1].lvl);   // KEY 100 = 1:1, ENV 100 = +8 octaves
        fc = fc > 20000 ? 20000 : fc < 15 ? 15 : fc; if (fc > 0.45f * fsr) fc = 0.45f * fsr;
        float r = p[WP_F_RES] * 0.01f, g = v->fg; fc = fc / fsr;
        if (fc != v->cfc || r != v->cres || v->ft != v->cft) {   // tan() only when the cutoff moved
            if (fc != v->cfc) g = v->fg = tanf((float)M_PI * fc);
            v->cfc = fc; v->cres = r; v->cft = v->ft;
            if (v->ft == WF_LADDER) { v->fG = g / (1 + g); v->fk = r * 3.9f; v->fcomp = 1 + v->fk * 0.35f; }
            else { v->fk = 2 - 1.92f * r; v->fkg = v->fk + g; v->fd = 1 / (1 + v->fk * g + g * g); v->fd1 = 1 / (1 + 1.4142136f * g + g * g); }
            v->fcl = v->ft == WF_LP12 || v->ft == WF_LP24 || v->ft == WF_NOTCH; v->fcb = v->ft == WF_BP ? v->fk : 0; v->fch = v->ft == WF_HP12 || v->ft == WF_NOTCH;
        }
        v->fdg = p[WP_F_DRIVE] > 0 ? 1 + p[WP_F_DRIVE] * 0.07f : 0;
    }
    for (int c = 0; c < 2; c++) {        // tails die to zero, not to denormals
        for (int k = 0; k < 4; k++) if (fabsf(v->fs[c][k]) < 1e-15f) v->fs[c][k] = 0;
        if (fabsf(v->fy[c]) < 1e-15f) v->fy[c] = 0;
    }
    for (int k = 0; k < 6; k++) if (fabsf(v->pk[k]) < 1e-15f) v->pk[k] = 0;
    return 1;
}
// The filter over one stretch, in place, both sides.
// The filter over one stretch, in place. Both sides run in the same loop: two chains the CPU can overlap.
static void wv_filter(WaveV *v, float *L, float *R, int n) {
    int os = v->hq ? 2 : 1, ft = v->ft; float g = v->fg, dg = v->fdg, (*s)[4] = v->fs;
    if (ft == WF_LADDER) {
        float G = v->fG, k = v->fk, cp = v->fcomp, y0 = v->fy[0], y1 = v->fy[1];
        float a0 = s[0][0], a1 = s[0][1], a2 = s[0][2], a3 = s[0][3], b0 = s[1][0], b1 = s[1][1], b2 = s[1][2], b3 = s[1][3];
        for (int i = 0; i < n; i++) {
            float il = dg ? L[i] * dg : L[i], ir = dg ? R[i] * dg : R[i];
            for (int r = 0; r < os; r++) {
                float u = wv_sat(il - k * y0), w = wv_sat(ir - k * y1), t, q;
                t = (u - a0) * G; u = t + a0; a0 = u + t;  q = (w - b0) * G; w = q + b0; b0 = w + q;
                t = (u - a1) * G; u = t + a1; a1 = u + t;  q = (w - b1) * G; w = q + b1; b1 = w + q;
                t = (u - a2) * G; u = t + a2; a2 = u + t;  q = (w - b2) * G; w = q + b2; b2 = w + q;
                t = (u - a3) * G; u = t + a3; a3 = u + t;  q = (w - b3) * G; w = q + b3; b3 = w + q;
                y0 = u; y1 = w;
            }
            L[i] = y0 * cp; R[i] = y1 * cp;
        }
        v->fy[0] = y0; v->fy[1] = y1;
        s[0][0] = a0; s[0][1] = a1; s[0][2] = a2; s[0][3] = a3; s[1][0] = b0; s[1][1] = b1; s[1][2] = b2; s[1][3] = b3;
    } else if (ft == WF_LP24) {          // a gentle first stage, the resonant one after it
        float kg = v->fkg, d = v->fd, d1 = v->fd1, k1g = 1.4142136f + g;
        float la1 = s[0][0], la2 = s[0][1], lb1 = s[0][2], lb2 = s[0][3], ra1 = s[1][0], ra2 = s[1][1], rb1 = s[1][2], rb2 = s[1][3];
        for (int i = 0; i < n; i++) {
            float il = dg ? wv_sat(L[i] * dg) : L[i], ir = dg ? wv_sat(R[i] * dg) : R[i], ol = 0, orr = 0;
            for (int r = 0; r < os; r++) {
                float h = (il - k1g * lb1 - lb2) * d1, H = (ir - k1g * rb1 - rb2) * d1;
                float b = g * h + lb1, B = g * H + rb1; lb1 = g * h + b; rb1 = g * H + B;
                float u = g * b + lb2, U = g * B + rb2; lb2 = g * b + u; rb2 = g * B + U;
                h = (u - kg * la1 - la2) * d; H = (U - kg * ra1 - ra2) * d;
                b = g * h + la1; B = g * H + ra1; la1 = g * h + b; ra1 = g * H + B;
                ol = g * b + la2; orr = g * B + ra2; la2 = g * b + ol; ra2 = g * B + orr;
            }
            L[i] = ol; R[i] = orr;
        }
        s[0][0] = la1; s[0][1] = la2; s[0][2] = lb1; s[0][3] = lb2; s[1][0] = ra1; s[1][1] = ra2; s[1][2] = rb1; s[1][3] = rb2;
    } else {                             // one SVF: LP12 HP12 BP NOTCH as a mix of its outputs
        float kg = v->fkg, d = v->fd, cl = v->fcl, cb = v->fcb, ch = v->fch, la1 = s[0][0], la2 = s[0][1], ra1 = s[1][0], ra2 = s[1][1];
        for (int i = 0; i < n; i++) {
            float il = dg ? wv_sat(L[i] * dg) : L[i], ir = dg ? wv_sat(R[i] * dg) : R[i], ol = 0, orr = 0;
            for (int r = 0; r < os; r++) {
                float h = (il - kg * la1 - la2) * d, H = (ir - kg * ra1 - ra2) * d;
                float b = g * h + la1, B = g * H + ra1; la1 = g * h + b; ra1 = g * H + B;
                float l = g * b + la2, Lp = g * B + ra2; la2 = g * b + l; ra2 = g * B + Lp;
                ol = cl * l + cb * b + ch * h; orr = cl * Lp + cb * B + ch * H;
            }
            L[i] = ol; R[i] = orr;
        }
        s[0][0] = la1; s[0][1] = la2; s[1][0] = ra1; s[1][1] = ra2;
    }
}
// One stretch of a voice (n <= what is left of its block), added into out (interleaved stereo).
static void wv_chunk(WaveV *v, float *out, int n) {
    float L[WV_BLK] = {0}, R[WV_BLK] = {0}, DL[WV_BLK] = {0}, DR[WV_BLK] = {0};
    for (int k = 0; k < 2; k++) {
        int o = v->order ? 1 - k : k;
        if (v->o[o].nu) wv_osc_run(&v->o[o], n, v->hq, L, R, v->om[o], v->om[o ^ 1]);
        else for (int i = 0; i < n; i++) v->om[o][i] = 0;
    }
    if (v->sl != 0 || v->dsl != 0 || v->sr != 0 || v->dsr != 0) {   // SUB
        float *sl = v->sdir ? DL : L, *sr = v->sdir ? DR : R, dt = v->sinc * (1.f / 4294967296.f);
        for (int i = 0; i < n; i++) {
            float x = (float)(v->sph >> 8) * (1.f / 16777216.f), y;
            switch (v->sshape) {
            case 1: y = 1 - 4 * fabsf(x - 0.5f); break;
            case 2: { float q = x + 0.5f; if (q >= 1) q -= 1; y = (x < 0.5f ? 1.f : -1.f) + wv_blep(x, dt) - wv_blep(q, dt); break; }
            case 3: y = 2 * x - 1 - wv_blep(x, dt); break;
            default: y = wv_sin1(x);
            }
            v->sph += v->sinc; v->sl += v->dsl; v->sr += v->dsr;
            sl[i] += y * v->sl; sr[i] += y * v->sr;
        }
    }
    if (v->nl != 0 || v->dnl != 0) {     // NOISE: white, or pink from six one-poles an octave pair apart
        float *nl = v->ndir ? DL : L, *nr = v->ndir ? DR : R;
        static const float pa[6] = {0.0032672f, 0.0130069f, 0.0510153f, 0.1890183f, 0.5671670f, 1.f}, pg[6] = {0.2f, 0.1f, 0.05f, 0.025f, 0.0125f, 0.00625f};
        for (int i = 0; i < n; i++) {
            v->rng = v->rng * 1664525u + 1013904223u; float y = (float)((int32_t)v->rng >> 8) * (1.f / 8388608.f);
            if (v->ntype) { float s = 0; for (int k = 0; k < 6; k++) { v->pk[k] += pa[k] * (y - v->pk[k]); s += pg[k] * v->pk[k]; } y = s * 22.f; }   // about the loudness of the white
            v->nl += v->dnl; y *= v->nl; nl[i] += y; nr[i] += y;
        }
    }
    if (v->ft) wv_filter(v, L, R, n);
    float a = v->amp, da = v->damp;
    for (int i = 0; i < n; i++) { a += da; out[2 * i] += (L[i] + DL[i]) * a; out[2 * i + 1] += (R[i] + DR[i]) * a; }
    v->amp = a;
}
// One voice for n frames into out (+=). Returns 0 once it has finished.
static int wv_voice_run(WaveV *v, WaveInst *w, const int8_t *wp, const WaveStore *ws, float *out, int n) {
    int i = 0;
    if (v->delay > 0) { int d = v->delay < n ? v->delay : n; v->delay -= d; i = d; }
    while (i < n) {
        if (v->bpos == 0 && !wv_ctl(v, w, wp, ws)) { v->on = 0; return 0; }
        int m = WV_BLK - v->bpos; if (m > n - i) m = n - i;
        wv_chunk(v, out + 2 * i, m);
        v->bpos = (v->bpos + m) & (WV_BLK - 1); i += m;
        if (!v->rel && v->gate > 0 && (v->gate -= m) <= 0) wv_vrelease(v);
    }
    return 1;
}

// ---------- the instrument: 8 voices or one, notes, the buffer ----------
static void wv_note_on(WaveInst *w, const int8_t *wp, float note, float vel, int gate, int delay) {
    int poly = wp[WP_POLY]; if (poly < 1) poly = 1; if (poly > WV_POLY) poly = WV_POLY;
    if (gate < 0) { if (w->nheld == 16) { memmove(w->held, w->held + 1, 15 * sizeof(float)); w->nheld--; } w->held[w->nheld++] = note; }
    if (poly == 1) {
        WaveV *v = &w->v[0];
        for (int i = 1; i < WV_POLY; i++) wv_vrelease(&w->v[i]);
        if (v->on && !v->fade && !v->pend && !v->rel && wp[WP_LEGATO]) {   // LEGATO: the line goes on, no new attack
            v->tnote = note; v->key = note; v->gate = gate; if (wp[WP_GLIDE] <= 0) v->note = note;
        } else if (v->on && !v->pend) wv_vstart(w, v, wp, note, vel, gate, 0);
        else { wv_vstart(w, v, wp, note, vel, gate, 1); v->delay = delay; if (wp[WP_GLIDE] > 0 && w->last > -999) v->note = w->last; }
        w->last = note; return;
    }
    int best = -1;
    for (int i = 0; i < WV_POLY; i++) { WaveV *v = &w->v[i]; if (v->on && !v->fade && !v->pend && fabsf(v->key - note) < 0.01f) { best = i; break; } }
    if (best >= 0) { wv_vstart(w, &w->v[best], wp, note, vel, gate, 0); w->last = note; return; }   // the same key again: the same voice
    for (int i = 0; i < poly && best < 0; i++) if (!w->v[i].on) best = i;
    if (best < 0) { float lo = 2; for (int i = 0; i < poly; i++) { WaveV *v = &w->v[i]; if (v->rel && !v->pend && v->e[0].lvl < lo) { lo = v->e[0].lvl; best = i; } } }
    if (best < 0) { unsigned old = ~0u; for (int i = 0; i < poly; i++) if (w->v[i].age < old) { old = w->v[i].age; best = i; } }
    WaveV *v = &w->v[best];
    if (v->on) { v->pend = 1; v->fade = 1; v->pnote = note; v->pvel = vel; v->pgate = gate; v->key = note; v->rel = 1; v->age = ++w->age; }   // steal: one block down to 0, then the new note
    else { wv_vstart(w, v, wp, note, vel, gate, 1); v->delay = delay; if (wp[WP_GLIDE] > 0 && w->last > -999) v->note = w->last; }
    w->last = note;
}
static void wv_note_off(WaveInst *w, const int8_t *wp, float note) {
    for (int i = 0; i < w->nheld; i++) if (fabsf(w->held[i] - note) < 0.01f) { memmove(w->held + i, w->held + i + 1, (w->nheld - i - 1) * sizeof(float)); w->nheld--; break; }
    int poly = wp[WP_POLY];
    if (poly <= 1) {
        WaveV *v = &w->v[0];
        if (!v->on || v->rel || fabsf(v->key - note) >= 0.01f) return;
        if (w->nheld) { float n2 = w->held[w->nheld - 1]; v->tnote = n2; v->key = n2; if (wp[WP_GLIDE] <= 0) v->note = n2; }   // back to the key still held
        else wv_vrelease(v);
        return;
    }
    for (int i = 0; i < WV_POLY; i++) {
        WaveV *v = &w->v[i]; if (!v->on || fabsf(v->key - note) >= 0.01f) continue;
        if (v->pend) { if (v->pgate < 0) v->pgate = 1; } else if (!v->rel && v->gate < 0) wv_vrelease(v);
    }
}
static void wv_all_off(WaveInst *w) { for (int i = 0; i < WV_POLY; i++) { wv_vrelease(&w->v[i]); if (w->v[i].pend) w->v[i].pgate = 1; } w->nheld = 0; }
static int wv_active(const WaveInst *w) { int n = 0; for (int i = 0; i < WV_POLY; i++) n += w->v[i].on; return n; }
// The buffer: every sounding voice added into out (interleaved stereo, n frames). Audio thread.
static void wv_render(WaveInst *w, const int8_t *wp, const WaveStore *ws, float *out, int n) {
    for (int k = 0; k < WP_NP; k++) w->base[k] = wp[k];
    w->nmods = 0;
    for (int s = 0; s < WM_SLOTS; s++) {
        const int8_t *m = wp + WP_MTX + s * 4; int src = m[WM_SRC], dst = m[WM_DST] - 1;
        if (src <= 0 || src >= WS_N || dst < 0 || dst >= WP_NP || !m[WM_AMT] || !wp_info[dst].mod) continue;
        WvMod *d = &w->mods[w->nmods++];
        d->src = (uint8_t)src; d->dst = (uint8_t)dst; d->bi = m[WM_MODE] > 0; d->nat = (src >= WS_LFO1 && src <= WS_LFO4) || src == WS_NOTE;
        d->amt = m[WM_AMT] * 0.01f * (wp_info[dst].hi - wp_info[dst].lo);
    }
    memset(w->md, 0, sizeof w->md); for (int i = 0; i < w->nmods; i++) w->md[w->mods[i].dst] = 1;
    for (int j = 0; j < 3; j++) { const int8_t *e = wp + WP_ENV1 + j * WE_N;
        wv_envk(&w->ek[j], wv_time(e[WE_ATT]), wv_time(e[WE_DEC]), e[WE_SUS] * 0.01f, wv_time(e[WE_REL]), e[WE_CURVE] * 0.01f, (float)WV_BLK / SR); }
    for (int j = 0; j < 4; j++) {        // the free-running LFOs go on whether a voice listens or not
        int sy = wp[WP_LFO1 + j * WL_N + WL_SYNC];
        float hz = sy > 0 && sy < WV_NSYNC ? w->bpm / 60 / wv_sync_beats[sy] : wv_lfo_hz(wp[WP_LFO1 + j * WL_N + WL_RATE]);
        w->lhz[j] = hz; w->lph[j] += hz * n / SR; w->lph[j] -= floorf(w->lph[j]);
    }
    for (int i = 0; i < WV_POLY; i++) if (w->v[i].on) wv_voice_run(&w->v[i], w, wp, ws, out, n);
}
#endif
