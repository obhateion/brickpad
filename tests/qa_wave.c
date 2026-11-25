// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// WAVE (src/wave.h) checks: pitch, aliasing, unison width, warps, envelopes, the matrix, the WAV loader, voices,
// NaN / denormals. Build: cc -O2 -std=gnu11 -Isrc tests/qa_wave.c -o qa_wave -lm && ./qa_wave  (prints ALL OK)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "wave.h"

static WaveStore ws; static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static const char *tmpdir(void) { const char *t = getenv("TMPDIR"); return t && *t ? t : "/tmp"; }

// n frames of the instrument into L and R (128-frame buffers, as the Brick's callback).
static void render(WaveInst *w, const int8_t *wp, float *L, float *R, int n) {
    float buf[256];
    for (int i = 0; i < n; i += 128) {
        int m = n - i < 128 ? n - i : 128; memset(buf, 0, sizeof buf);
        wv_render(w, wp, &ws, buf, m);
        for (int k = 0; k < m; k++) { if (L) L[i + k] = buf[2 * k]; if (R) R[i + k] = buf[2 * k + 1]; }
    }
}
static void plain(int8_t *wp) {          // one sine, no filter, the envelope full at once and held
    wp_defaults(wp); wp[WP_F_TYPE] = WF_OFF; wp[WP_A + WO_RAND] = 0;
    wp[WP_ENV1 + WE_SUS] = 100; wp[WP_ENV1 + WE_CURVE] = 0; wp[WP_VELSENS] = 0; wp[WP_MASTER] = 100;
}
static double *re_, *im_;
// The magnitude spectrum of x (n a power of two) with a window: 0 Hann, 1 Blackman-Harris. mag[0..n/2].
static void spectrum(const float *x, int n, int win, double *mag) {
    re_ = realloc(re_, n * sizeof(double)); im_ = realloc(im_, n * sizeof(double));
    for (int i = 0; i < n; i++) {
        double t = 2 * M_PI * i / n, w = win ? 0.35875 - 0.48829 * cos(t) + 0.14128 * cos(2 * t) - 0.01168 * cos(3 * t) : 0.5 - 0.5 * cos(t);
        re_[i] = x[i] * w; im_[i] = 0;
    }
    wt_fft(re_, im_, n, 0);
    for (int i = 0; i <= n / 2; i++) mag[i] = sqrt(re_[i] * re_[i] + im_[i] * im_[i]) + 1e-30;
}
static double peak_hz(const float *x, int n) {   // the strongest frequency, Gaussian-interpolated between bins
    double *m = malloc((n / 2 + 1) * sizeof(double)); spectrum(x, n, 0, m);
    int k = 2; for (int i = 2; i < n / 2 - 1; i++) if (m[i] > m[k]) k = i;
    double a = log(m[k - 1]), b = log(m[k]), c = log(m[k + 1]), d = 0.5 * (a - c) / (a - 2 * b + c);
    free(m); return (k + d) * SR / n;
}
static double rms(const float *x, int n) { double s = 0; for (int i = 0; i < n; i++) s += (double)x[i] * x[i]; return sqrt(s / (n > 0 ? n : 1)); }
static int finite_all(const float *x, int n, float lim) { for (int i = 0; i < n; i++) if (!isfinite(x[i]) || fabsf(x[i]) > lim) return 0; return 1; }
static double note_of(double hz) { return 69 + 12 * log2(hz / 440); }

static void t_pitch(void) {
    int N = 1 << 17; float *L = malloc((N + 4096) * sizeof(float)); int8_t wp[WP_N]; double worst = 0;
    for (int hq = 0; hq < 2; hq++)
        for (int nt = 33; nt <= 117; nt += 12) {
            plain(wp); wp[WP_QUALITY] = hq;
            WaveInst w; wv_init(&w); wv_note_on(&w, wp, nt, 1, -1, 0);
            render(&w, wp, L, NULL, N + 4096);
            double f = peak_hz(L + 4096, N), c = 1200 * log2(f / wv_hz(nt));
            if (fabs(c) > worst) worst = fabs(c);
            CHECK(fabs(c) < 1, "pitch note %d hq %d: %.3f Hz, %.3f cents", nt, hq, f, c);
        }
    // OCT, SEMI and FINE together: 57 + 12 + 7 + 0.5
    plain(wp); wp[WP_A + WO_OCT] = 1; wp[WP_A + WO_SEMI] = 7; wp[WP_A + WO_FINE] = 50;
    WaveInst w; wv_init(&w); wv_note_on(&w, wp, 57, 1, -1, 0); render(&w, wp, L, NULL, N + 4096);
    double c = 1200 * log2(peak_hz(L + 4096, N) / wv_hz(57 + 19.5)); CHECK(fabs(c) < 1, "OCT/SEMI/FINE off by %.3f cents", c);
    printf("pitch: worst %.4f cents over 8 octaves (ECO and HIGH), tuning %.4f cents\n", worst, fabs(c));
    free(L);
}
// A one-frame naive saw (2x - 1, full of harmonics that fold) as a user table: played from 100 Hz to 10 kHz, nothing
// that is not a harmonic may come within 60 dB of the fundamental below 20 kHz.
static void t_alias(void) {
    static float fr[WT_LEN]; for (int i = 0; i < WT_LEN; i++) fr[i] = 2.f * i / WT_LEN - 1;
    CHECK(wt_from_frames(&ws.t[WT_NBUILT], fr, 1, "SAW", 0) == 1, "saw table");
    int N = 16384; float *L = malloc((N + 4096) * sizeof(float)); double *m = malloc((N / 2 + 1) * sizeof(double)); int8_t wp[WP_N];
    double worst = -999, wf = 0;
    for (int hq = 0; hq < 2; hq++) {
        double notes[64]; int nn = 0;
        for (double nt = note_of(100); nt < note_of(10000); nt += 1.7) notes[nn++] = nt;
        notes[nn++] = note_of(10000);
        for (int q = 0; q < nn; q++) {
            plain(wp); wp[WP_QUALITY] = hq; wp[WP_A + WO_TABLE] = WT_NBUILT;
            WaveInst w; wv_init(&w); wv_note_on(&w, wp, (float)notes[q], 1, -1, 0);
            render(&w, wp, L, NULL, N + 4096); spectrum(L + 4096, N, 1, m);
            double f = wv_hz((float)notes[q]), bin = (double)SR / N, fund = 0, bad = 0;
            for (int k = (int)(f / bin) - 4; k <= (int)(f / bin) + 4; k++) if (m[k] > fund) fund = m[k];
            for (int k = (int)(20 / bin); k <= (int)(20000 / bin); k++) {
                double hz = k * bin, h = floor(hz / f + 0.5);
                if (h >= 1 && fabs(hz - h * f) < 7 * bin) continue;   // a harmonic and its window's main lobe
                if (m[k] > bad) bad = m[k];
            }
            double db = 20 * log10(bad / fund);
            if (db > worst) { worst = db; wf = f; }
            CHECK(db < -60, "alias at %.0f Hz (hq %d): %.1f dB", f, hq, db);
        }
    }
    printf("alias: worst non-harmonic %.1f dB (at %.0f Hz), saw 100 Hz .. 10 kHz, below 20 kHz\n", worst, wf);
    free(L); free(m);
}
static double corr(const float *a, const float *b, int n) {
    double ab = 0, aa = 0, bb = 0; for (int i = 0; i < n; i++) { ab += (double)a[i] * b[i]; aa += (double)a[i] * a[i]; bb += (double)b[i] * b[i]; }
    return ab / sqrt(aa * bb + 1e-30);
}
static void t_unison(void) {
    int N = 48000; float *L = malloc(N * sizeof(float)), *R = malloc(N * sizeof(float)); int8_t wp[WP_N]; double cs[3];
    int wid[3] = {0, 50, 100};
    for (int i = 0; i < 3; i++) {
        plain(wp); wp[WP_A + WO_TABLE] = 0; wp[WP_A + WO_POS] = 67; wp[WP_A + WO_UNI] = 4; wp[WP_A + WO_DET] = 30; wp[WP_A + WO_RAND] = 100; wp[WP_A + WO_WIDTH] = wid[i];
        WaveInst w; wv_init(&w); wv_note_on(&w, wp, 48, 1, -1, 0); render(&w, wp, L, R, N);
        cs[i] = corr(L + 4800, R + 4800, N - 4800);
        if (!i) { float d = 0; for (int k = 0; k < N; k++) if (fabsf(L[k] - R[k]) > d) d = fabsf(L[k] - R[k]); CHECK(d == 0, "width 0 must be mono, L-R %g", d); }
    }
    CHECK(cs[0] > 0.9999 && cs[1] < cs[0] && cs[2] < cs[1] && cs[2] < 0.6, "unison width: L/R correlation %.3f %.3f %.3f", cs[0], cs[1], cs[2]);
    // the same unison at 8 voices: still in range, louder than one voice but not 8 times
    plain(wp); wp[WP_A + WO_UNI] = 8; wp[WP_A + WO_DET] = 40; WaveInst w; wv_init(&w); wv_note_on(&w, wp, 48, 1, -1, 0); render(&w, wp, L, R, N);
    CHECK(finite_all(L, N, 4) && finite_all(R, N, 4), "unison 8 out of range");
    printf("unison: L/R correlation at WIDTH 0/50/100 = %.4f %.3f %.3f\n", cs[0], cs[1], cs[2]);
    free(L); free(R);
}
static void t_warp(void) {
    int N = 12000; float *L = malloc(N * sizeof(float)), *R = malloc(N * sizeof(float)), *L0 = malloc(N * sizeof(float)); int8_t wp[WP_N];
    static const int amts[4] = {0, 25, 60, 100}, notes[3] = {30, 60, 100}; float top = 0;
    for (int o = 0; o < 2; o++) for (int wm = 0; wm < WW_N; wm++) for (int hq = 0; hq < 2; hq++) for (int ni = 0; ni < 3; ni++) for (int ai = 0; ai < 4; ai++) {
        plain(wp); wp[WP_QUALITY] = hq; wp[WP_B + WO_ON] = 1; wp[WP_B + WO_TABLE] = 7; wp[WP_B + WO_POS] = 40; wp[WP_B + WO_SEMI] = 7;
        wp[WP_A + WO_TABLE] = 0; wp[WP_A + WO_POS] = 50; wp[WP_A + WO_UNI] = 2;
        wp[o * 16 + WO_WARP] = wm; wp[o * 16 + WO_WAMT] = amts[ai];
        WaveInst w; wv_init(&w); wv_note_on(&w, wp, notes[ni], 1, -1, 0); render(&w, wp, L, R, N);
        CHECK(finite_all(L, N, 4) && finite_all(R, N, 4), "warp %s osc %d amt %d note %d: out of range", wv_warp_names[wm], o, amts[ai], notes[ni]);
        CHECK(rms(L + 2000, N - 2000) > 1e-3, "warp %s osc %d amt %d note %d: silent", wv_warp_names[wm], o, amts[ai], notes[ni]);
        for (int i = 0; i < N; i++) if (fabsf(L[i]) > top) top = fabsf(L[i]);
        if (!ai) memcpy(L0, L, N * sizeof(float));
        else if (wm && ai == 3) { double d = 0; for (int i = 0; i < N; i++) d += fabs(L[i] - L0[i]); CHECK(d / N > 1e-3, "warp %s osc %d does nothing", wv_warp_names[wm], o); }
    }
    printf("warp: %d modes x 2 osc x 4 amounts x 3 notes x 2 qualities finite, peak %.2f\n", WW_N, top);
    free(L); free(R); free(L0);
}
static void t_env(void) {
    const float dt = (float)WV_BLK / SR; int8_t wp[WP_N];
    int vals[4] = {10, 40, 60, 85}; float curv[3] = {0, 0.6f, -0.6f}; double worst = 0;
    for (int vi = 0; vi < 4; vi++) for (int ci = 0; ci < 3; ci++) {
        float a = wv_time(vals[vi]), d = wv_time(vals[(vi + 1) & 3]), r = wv_time(vals[(vi + 2) & 3]), s = 0.4f, c = curv[ci];
        WvEnv e = {0}; WvEnvK K; wv_envk(&K, a, d, s, r, c, dt); wv_env_go(&e); double t = 0; int n = 0;
        while (e.st == 1 && n < 10000000) { wv_env_step(&e, &K); t += dt; n++; }
        double ta = t; while (e.st == 2 && n < 10000000) { wv_env_step(&e, &K); t += dt; n++; }
        double td = t; for (int k = 0; k < 100; k++) wv_env_step(&e, &K);
        CHECK(fabsf(e.lvl - s) < 1e-6f, "sustain level %f", e.lvl);
        wv_env_rel(&e); t = 0; while (e.st == 4 && n < 20000000) { wv_env_step(&e, &K); t += dt; n++; }
        // the attack ends inside the block that crosses a; decay and release likewise (a block = 0.33 ms)
        double ea = fabs(ta - a), ed = fabs(td - a - d), er = fabs(t - r);
        double m = ea > ed ? ea : ed; if (er > m) m = er; if (m > worst) worst = m;
        CHECK(ea <= dt * 1.01 && ed <= dt * 2.01 && er <= dt * 1.01, "env times a %.4f/%.4f d %.4f/%.4f r %.4f/%.4f", ta, a, td - ta, d, t, r);
    }
    // through the voice: a linear attack of 100 ms (ATT 54.97 -> nearest step), the release ends the voice
    plain(wp); wp[WP_ENV1 + WE_ATT] = 55; wp[WP_ENV1 + WE_REL] = 45; float A = wv_time(55), Rt = wv_time(45);
    int N = (int)((A + 0.2f) * SR); float *L = malloc(N * sizeof(float));
    WaveInst w; wv_init(&w); wv_note_on(&w, wp, 93, 1, -1, 0); render(&w, wp, L, NULL, N);
    double pk = 0; for (int i = N - 4800; i < N; i++) if (fabs(L[i]) > pk) pk = fabs(L[i]);
    int hit = 0; for (int i = 0; i + 48 <= N; i += 48) { double m = 0; for (int k = 0; k < 48; k++) if (fabs(L[i + k]) > m) m = fabs(L[i + k]); if (m >= 0.99 * pk) { hit = i + 48; break; } }
    CHECK(fabs(hit / (double)SR - A) < 0.003, "attack heard at %.4f s, set %.4f s", hit / (double)SR, A);
    wv_note_off(&w, wp, 93); int fr = 0; while (wv_active(&w) && fr < SR * 20) { render(&w, wp, L, NULL, 128); fr += 128; }
    CHECK(fabs(fr / (double)SR - Rt) < 0.004, "voice ended %.4f s after release, set %.4f s", fr / (double)SR, Rt);
    printf("env: stage times within %.2f ms (block %.2f ms); heard attack %.1f ms (set %.1f), release end %.1f ms (set %.1f)\n",
           worst * 1000, dt * 1000, hit * 1000.0 / SR, A * 1000, fr * 1000.0 / SR, Rt * 1000);
    free(L);
}
static void mtx(int8_t *wp, int slot, int src, int dst, int amt, int bi) { int8_t *m = wp + WP_MTX + slot * 4; m[WM_SRC] = src; m[WM_DST] = dst + 1; m[WM_AMT] = amt; m[WM_MODE] = bi; }
static void t_matrix(void) {
    int N = 1 << 16; float *L = malloc((N + 4096) * sizeof(float)); int8_t wp[WP_N];
    struct { int src, dst, amt, bi, macro, mw; double semis; } cs[] = {
        {WS_MACRO1, WP_A + WO_SEMI, 25, 0, 100, 0, 12}, {WS_MACRO1, WP_A + WO_SEMI, 25, 1, 0, 0, -12},
        {WS_MW, WP_A + WO_SEMI, -25, 0, 0, 50, -6}, {WS_MACRO2, WP_A + WO_OCT, 25, 1, 100, 0, 24}, {WS_NOTE, WP_A + WO_FINE, 10, 1, 0, 0, 0},
    };
    for (unsigned q = 0; q < sizeof cs / sizeof cs[0]; q++) {
        plain(wp); mtx(wp, 3, cs[q].src, cs[q].dst, cs[q].amt, cs[q].bi);
        wp[cs[q].src == WS_MACRO2 ? WP_MACRO2 : WP_MACRO1] = cs[q].macro; wp[WP_MODWHEEL] = cs[q].mw;
        WaveInst w; wv_init(&w); wv_note_on(&w, wp, 60, 1, -1, 0); render(&w, wp, L, NULL, N + 4096);
        double exp = cs[q].src == WS_NOTE ? 0 : cs[q].semis, c = 1200 * log2(peak_hz(L + 4096, N) / wv_hz(60)) - exp * 100;
        CHECK(fabs(c) < 1, "matrix %s -> %s %+d %s: off by %.2f cents", wv_src_names[cs[q].src], wp_name(cs[q].dst), cs[q].amt, cs[q].bi ? "BI" : "UNI", c);
    }
    // NOTE -> FINE: an octave up (+48 / 48 = +1 in BI) adds 10 % of 200 cents
    plain(wp); mtx(wp, 0, WS_NOTE, WP_A + WO_FINE, 10, 1);
    { WaveInst w; wv_init(&w); wv_note_on(&w, wp, 72, 1, -1, 0); render(&w, wp, L, NULL, N + 4096);
      double c = 1200 * log2(peak_hz(L + 4096, N) / wv_hz(72)); CHECK(fabs(c - 5) < 1, "NOTE -> FINE gave %.2f cents, want 5", c); }
    // LFO1 (square, 4 Hz) -> A LEVEL +100 BI: the level swings between full and nothing
    plain(wp); wp[WP_LFO1 + WL_SHAPE] = WLS_SQUARE; wp[WP_LFO1 + WL_SYNC] = 7; wp[WP_LFO1 + WL_TRIG] = 1; mtx(wp, 0, WS_LFO1, WP_A + WO_LEVEL, 100, 1);
    { WaveInst w; wv_init(&w); w.bpm = 240; wv_note_on(&w, wp, 69, 1, -1, 0); render(&w, wp, L, NULL, SR);
      double a = rms(L + 1200, 3600), b = rms(L + 7200, 3600);   // 4 Hz: high in 0..125 ms, low 125..250 ms
      CHECK(a > 0.3 && b < 0.02 * a, "LFO -> LEVEL: %.3f then %.4f", a, b); }
    // ENV2 -> CUTOFF (on the panel's own ENV AMT and through the matrix): bright at the start, dark after the decay
    for (int via = 0; via < 2; via++) {
        plain(wp); wp[WP_A + WO_POS] = 67; wp[WP_F_TYPE] = WF_LP24; wp[WP_F_CUT] = 30; wp[WP_F_RES] = 0;
        wp[WP_ENV2 + WE_DEC] = 70; wp[WP_ENV2 + WE_SUS] = 0;
        if (via) mtx(wp, 9, WS_ENV2, WP_F_CUT, 60, 0); else wp[WP_F_ENV] = 75;
        WaveInst w; wv_init(&w); wv_note_on(&w, wp, 45, 1, -1, 0); render(&w, wp, L, NULL, SR);
        double *m = malloc(2049 * sizeof(double)), hi0 = 0, hi1 = 0; spectrum(L, 4096, 1, m); for (int k = 200; k < 2048; k++) hi0 += m[k];
        spectrum(L + SR - 4096, 4096, 1, m); for (int k = 200; k < 2048; k++) hi1 += m[k]; free(m);
        CHECK(hi0 > 20 * hi1, "ENV2 -> CUTOFF (%s): highs %.3g early, %.3g late", via ? "matrix" : "ENV AMT", hi0, hi1);
    }
    // VELOCITY -> MASTER: soft notes quieter when routed, the same when not
    double lv[2][2];
    for (int r = 0; r < 2; r++) for (int vi = 0; vi < 2; vi++) {
        plain(wp); wp[WP_MASTER] = 50; if (r) mtx(wp, 15, WS_VEL, WP_MASTER, 50, 0);
        WaveInst w; wv_init(&w); wv_note_on(&w, wp, 69, vi ? 1.f : 0.f, -1, 0); render(&w, wp, L, NULL, 9600); lv[r][vi] = rms(L + 4800, 4800);
    }
    CHECK(fabs(lv[0][0] - lv[0][1]) < 1e-6 && lv[1][1] > 3.5 * lv[1][0], "VELO -> MASTER: %.3f %.3f routed %.3f %.3f", lv[0][0], lv[0][1], lv[1][0], lv[1][1]);
    printf("matrix: macro/modwheel/note pitch routes within 1 cent, LFO->level, ENV2->cutoff, velo->master ok\n");
    free(L);
}
static void put32(FILE *f, uint32_t v) { uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)}; fwrite(b, 1, 4, f); }
static void put16(FILE *f, unsigned v) { uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; fwrite(b, 1, 2, f); }
static void wav16(const char *p, const float *x, int n, int ch, int clm) {   // a plain 16-bit WAV, a 'clm ' chunk when clm
    FILE *f = fopen(p, "wb"); char c[16]; int cn = 0; if (clm) cn = snprintf(c, sizeof c, "<!>%d", clm) + 1, cn += cn & 1;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + n * ch * 2 + (clm ? 8 + cn : 0)); fwrite("WAVEfmt ", 1, 8, f); put32(f, 16); put16(f, 1); put16(f, ch);
    put32(f, SR); put32(f, SR * 2 * ch); put16(f, 2 * ch); put16(f, 16);
    if (clm) { fwrite("clm ", 1, 4, f); put32(f, cn); char z[16] = {0}; memcpy(z, c, strlen(c)); fwrite(z, 1, cn, f); }
    fwrite("data", 1, 4, f); put32(f, n * ch * 2);
    for (int i = 0; i < n; i++) for (int k = 0; k < ch; k++) put16(f, (unsigned)(int16_t)lrintf(x[i] * 32767));
    fclose(f);
}
static void t_wav(void) {
    int nf = 8; float *x = malloc(nf * WT_LEN * sizeof(float)); char p[512]; double worst[4] = {0};
    for (int f = 0; f < nf; f++) for (int i = 0; i < WT_LEN; i++) { double t = 2 * M_PI * i / WT_LEN, y = 0; for (int h = 1; h <= 5; h++) y += 0.17 * sin(t * (h * (f + 1) % 900 + 1) + f * h); x[f * WT_LEN + i] = (float)y; }
    static WaveTable a, b; CHECK(wt_from_frames(&a, x, nf, "ORIG", 0) == nf, "from frames");
    int bits[4] = {16, 24, 32, 32}, flt[4] = {0, 0, 0, 1}; double tol[4] = {2e-4, 2e-6, 2e-6, 2e-6};
    for (int k = 0; k < 4; k++) {
        snprintf(p, sizeof p, "%s/qa_wave_%d%s.wav", tmpdir(), bits[k], flt[k] ? "f" : "");
        CHECK(wt_save_wav(&a, p, bits[k], flt[k]), "save %s", p);
        int r = wt_load_wav(&b, p); CHECK(r == nf, "load %s gave %d", p, r);
        if (r == nf) { for (size_t i = 0; i < (size_t)nf * WT_STRIDE; i++) { double d = fabs(a.d[i] - b.d[i]); if (d > worst[k]) worst[k] = d; }
            CHECK(worst[k] < tol[k], "round trip %d-bit%s: max error %g", bits[k], flt[k] ? " float" : "", worst[k]); }
        remove(p);
    }
    // 3 frames, stereo, no 'clm ' chunk: 2048 a frame
    float *y = malloc(3 * WT_LEN * sizeof(float)); for (int i = 0; i < 3 * WT_LEN; i++) y[i] = 0.5f * sinf(2 * (float)M_PI * 3 * i / WT_LEN);
    snprintf(p, sizeof p, "%s/qa_wave_st.wav", tmpdir()); wav16(p, y, 3 * WT_LEN, 2, 0); CHECK(wt_load_wav(&b, p) == 3, "stereo 3 frames");
    CHECK(!strcmp(b.name, "qa_wave_st"), "name '%s'", b.name);
    // not whole frames: refused
    wav16(p, y, 2 * WT_LEN + 5, 1, 0); CHECK(wt_load_wav(&b, p) == -4, "a part frame must be refused");
    // 'clm ' says 1024: four frames, each resampled to 2048 by its harmonics
    for (int i = 0; i < 4096; i++) y[i] = 0.5f * sinf(2 * (float)M_PI * 3 * i / 1024);
    wav16(p, y, 4096, 1, 1024); int r = wt_load_wav(&b, p); CHECK(r == 4, "clm 1024 gave %d", r);
    if (r == 4) { double e = 0; for (int i = 0; i < WT_LEN; i++) { double d = fabs(b.d[3 * WT_STRIDE + 1 + i] - 0.5 * sin(2 * M_PI * 3 * i / WT_LEN)); if (d > e) e = d; } CHECK(e < 1e-3, "clm 1024 resample error %g", e); }
    remove(p); wt_free(&a); wt_free(&b); free(x); free(y);
    CHECK(wt_load_wav(&b, "/nonexistent/x.wav") == -1, "missing file");
    printf("wav: round trip max error 16-bit %.2g, 24-bit %.2g, 32-bit %.2g, float %.2g; stereo, clm 1024, bad length ok\n", worst[0], worst[1], worst[2], worst[3]);
}
static void t_voices(void) {
    int8_t wp[WP_N]; float *L = malloc(48000 * sizeof(float)), *R = malloc(48000 * sizeof(float));
    plain(wp); WaveInst w; wv_init(&w);
    for (int k = 0; k < 10; k++) { wv_note_on(&w, wp, 48 + k, 0.8f, -1, 0); render(&w, wp, L, R, 256); }
    int n = 0, has0 = 0, has9 = 0; for (int i = 0; i < WV_POLY; i++) if (w.v[i].on) { n++; if (fabsf(w.v[i].key - 48) < .01f) has0 = 1; if (fabsf(w.v[i].key - 57) < .01f) has9 = 1; }
    CHECK(n == 8 && !has0 && has9, "poly 8: %d voices, oldest gone %d, newest there %d", n, !has0, has9);
    render(&w, wp, L, R, 48000); CHECK(finite_all(L, 48000, 8), "10 notes on 8 voices");
    for (int k = 0; k < 10; k++) wv_note_off(&w, wp, 48 + k);
    for (int k = 0; k < 40 && wv_active(&w); k++) render(&w, wp, L, R, 4800);
    CHECK(!wv_active(&w), "all voices end after note off");
    // POLY 3 caps the voices
    wp[WP_POLY] = 3; wv_init(&w); for (int k = 0; k < 6; k++) { wv_note_on(&w, wp, 60 + k, 1, -1, 0); render(&w, wp, L, R, 128); }
    CHECK(wv_active(&w) == 3, "POLY 3 has %d voices", wv_active(&w));
    // MONO LEGATO with GLIDE: no new attack, the pitch slides, back to the held key on release
    plain(wp); wp[WP_POLY] = 1; wp[WP_LEGATO] = 1; wp[WP_GLIDE] = 30; wp[WP_ENV1 + WE_SUS] = 60; wv_init(&w);
    wv_note_on(&w, wp, 60, 1, -1, 0); render(&w, wp, L, R, 24000);
    int st = w.v[0].e[0].st; wv_note_on(&w, wp, 67, 1, -1, 0); render(&w, wp, L, R, 1024);
    CHECK(st == 3 && w.v[0].e[0].st == 3 && w.v[0].note > 60.1f && w.v[0].note < 66.9f, "legato: env %d -> %d, pitch %.2f", st, w.v[0].e[0].st, w.v[0].note);
    render(&w, wp, L, R, 48000); CHECK(fabsf(w.v[0].note - 67) < 0.01f, "glide arrives: %.3f", w.v[0].note);
    wv_note_off(&w, wp, 67); render(&w, wp, L, R, 48000); CHECK(w.v[0].on && !w.v[0].rel && fabsf(w.v[0].note - 60) < 0.01f, "back to the held key: %.2f", w.v[0].note);
    wv_note_off(&w, wp, 60); CHECK(w.v[0].rel, "released");
    // MONO, not legato: a new attack from where the envelope is (no jump down to 0)
    wp[WP_LEGATO] = 0; wp[WP_GLIDE] = 0; wv_init(&w); wv_note_on(&w, wp, 60, 1, -1, 0); render(&w, wp, L, R, 24000);
    float lv = w.v[0].e[0].lvl; wv_note_on(&w, wp, 64, 1, -1, 0); render(&w, wp, L, R, 16);
    CHECK(w.v[0].e[0].st == 1 && w.v[0].e[0].lvl >= lv - 1e-6f && fabsf(w.v[0].note - 64) < 1e-3f, "mono retrigger: st %d lvl %.3f (was %.3f)", w.v[0].e[0].st, w.v[0].e[0].lvl, lv);
    // a gate in frames releases on its own; a delayed start waits
    plain(wp); wv_init(&w); wv_note_on(&w, wp, 60, 1, 4800, 100); render(&w, wp, L, R, 128);
    float z = 0; for (int i = 0; i < 100; i++) z += fabsf(L[i]); CHECK(z == 0 && fabsf(L[110]) > 0, "delay 100 frames");
    render(&w, wp, L, R, 4800); CHECK(w.v[0].rel, "gate of 4800 frames released");
    printf("voices: stealing, POLY cap, mono legato + glide, retrigger, gate and delay ok\n");
    free(L); free(R);
}
static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static int sub_state(const WaveInst *w) {
    for (int i = 0; i < WV_POLY; i++) { const WaveV *v = &w->v[i]; if (!v->on) continue;
        for (int c = 0; c < 2; c++) { for (int k = 0; k < 4; k++) if (fpclassify(v->fs[c][k]) == FP_SUBNORMAL) return 1; if (fpclassify(v->fy[c]) == FP_SUBNORMAL) return 1; }
        for (int k = 0; k < 6; k++) if (fpclassify(v->pk[k]) == FP_SUBNORMAL) return 1;
        if (fpclassify(v->amp) == FP_SUBNORMAL) return 1; }
    return 0;
}
static void t_nan(void) {
    int8_t wp[WP_N]; float *L = malloc(SR * sizeof(float)), *R = malloc(SR * sizeof(float)); uint32_t rs = 77; int runs = 0; float xpk = 0;
    for (int ft = 0; ft < WF_N; ft++) for (int ext = 0; ext < 4; ext++) for (int hq = 0; hq < 2; hq++) {
        wp_defaults(wp); wp[WP_F_TYPE] = ft; wp[WP_F_RES] = 100; wp[WP_F_DRIVE] = ext & 1 ? 100 : 0; wp[WP_F_CUT] = ext & 2 ? 100 : 0; wp[WP_QUALITY] = hq;
        wp[WP_B + WO_ON] = 1; wp[WP_A + WO_UNI] = wp[WP_B + WO_UNI] = 8; wp[WP_A + WO_DET] = wp[WP_B + WO_DET] = 100;
        wp[WP_A + WO_WARP] = (int)(wt_rng(&rs) % WW_N); wp[WP_B + WO_WARP] = (int)(wt_rng(&rs) % WW_N); wp[WP_A + WO_WAMT] = wp[WP_B + WO_WAMT] = 100;
        wp[WP_A + WO_TABLE] = (int)(wt_rng(&rs) % WT_NBUILT); wp[WP_B + WO_TABLE] = (int)(wt_rng(&rs) % WT_NBUILT);
        wp[WP_SUB_LEVEL] = 100; wp[WP_SUB_SHAPE] = ext; wp[WP_NOISE_LEVEL] = 100; wp[WP_NOISE_TYPE] = ext & 1; wp[WP_F_ENV] = ext & 2 ? -100 : 100;
        for (int j = 0; j < 4; j++) { wp[WP_LFO1 + j * WL_N + WL_RATE] = 100; wp[WP_LFO1 + j * WL_N + WL_SHAPE] = (int)(wt_rng(&rs) % WLS_N); }
        for (int s = 0; s < WM_SLOTS; s++) { int d; do d = (int)(wt_rng(&rs) % WP_NP); while (!wp_info[d].mod); mtx(wp, s, 1 + (int)(wt_rng(&rs) % (WS_N - 1)), d, (wt_rng(&rs) & 1) ? 100 : -100, (int)(wt_rng(&rs) & 1)); }
        wp[WP_MACRO1] = 100; wp[WP_MODWHEEL] = 100;
        WaveInst w; wv_init(&w); w.bend = 1;
        for (int k = 0; k < 8; k++) wv_note_on(&w, wp, 20 + k * 13, 1, -1, 0);
        render(&w, wp, L, R, SR); runs++;
        float pk = 0; int fin = 1; for (int i = 0; i < SR; i++) { if (!isfinite(L[i]) || !isfinite(R[i])) fin = 0; else { if (fabsf(L[i]) > pk) pk = fabsf(L[i]); if (fabsf(R[i]) > pk) pk = fabsf(R[i]); } }
        if (pk > xpk) xpk = pk;
        CHECK(fin && pk < 20, "extreme filter %s ext %d hq %d: finite %d, peak %.1f", wv_filt_names[ft], ext, hq, fin, pk);
    }
    // a 10 s release through a resonant filter: the tail must not go denormal, nor get slower than the note itself
    plain(wp); wp[WP_F_TYPE] = WF_LP24; wp[WP_F_CUT] = 20; wp[WP_F_RES] = 90; wp[WP_ENV1 + WE_REL] = 100; wp[WP_ENV1 + WE_CURVE] = 100;
    wp[WP_A + WO_UNI] = 4; wp[WP_POLY] = 8; WaveInst w; wv_init(&w); for (int k = 0; k < 8; k++) wv_note_on(&w, wp, 40 + k * 5, 1, -1, 0);
    double t0 = now_s(); render(&w, wp, L, R, SR); double t_note = now_s() - t0; int sub = 0;
    for (int k = 0; k < 8; k++) wv_note_off(&w, wp, 40 + k * 5);
    double t_tail = 0, tmax = 0; int secs = 0;
    while (wv_active(&w) && secs < 12) {
        for (int b = 0; b < SR / 128; b++) { t0 = now_s(); render(&w, wp, L, R, 128); double d = now_s() - t0; t_tail += d; if (d > tmax) tmax = d; sub |= sub_state(&w); }
        secs++;
    }
    CHECK(!sub, "denormal state in the tail");
    CHECK(t_tail / secs < 2 * t_note, "tail %.2f ms/s vs note %.2f ms/s", t_tail / secs * 1e3, t_note * 1e3);
    CHECK(!wv_active(&w), "the tail ends");
    printf("nan/denormal: %d extreme setups finite (8 voices, peak %.1f); tail %.2f ms per second vs %.2f ms while held, no subnormal state\n", runs, xpk, t_tail / secs * 1e3, t_note * 1e3);
    free(L); free(R);
}
static void t_params(void) {
    int8_t wp[WP_N]; wp_defaults(wp); int named = 0; char b[32];
    for (int k = 0; k < WP_N; k++) { const char *n = wp_name(k); if (!n) continue; named++; int lo, hi; wp_range(k, &lo, &hi);
        CHECK(lo <= hi && wp[k] >= lo && wp[k] <= hi, "param %d %s default %d outside %d..%d", k, n, wp[k], lo, hi); wp_str(k, wp[k], b, sizeof b, &ws); CHECK(b[0], "param %d text", k); }
    CHECK(named == 91 + 5 + 64, "named parameters %d", named);   // + the host's ARP bytes (2026-11-25)
    CHECK(wp[WP_MARK] == WP_INIT, "mark");
    for (int m = 0; m < WT_NLV; m++) { CHECK(wt_lsz[m] == 1 << wt_lbits[m], "level %d size", m); CHECK(m == 0 || wt_loff[m] == wt_loff[m - 1] + wt_lsz[m - 1] + 3, "level %d offset", m); }
    CHECK(wt_loff[WT_NLV - 1] + wt_lsz[WT_NLV - 1] + 3 == WT_STRIDE, "stride");
    for (int k = 0; k < WT_NBUILT; k++) { const WaveTable *t = &ws.t[k]; float pk = 0; int bad = 0;
        for (size_t i = 0; i < (size_t)t->nf * WT_STRIDE; i++) { if (!isfinite(t->d[i])) bad = 1; if (fabsf(t->d[i]) > pk) pk = fabsf(t->d[i]); }
        CHECK(!bad && pk > 0.9f && pk < 1.5f, "table %s peak %.3f", t->name, pk); }
    printf("params: %d named, defaults in range; %d built-in tables, %.1f MB\n", named, WT_NBUILT, ({ size_t s = 0; for (int k = 0; k < WT_NBUILT; k++) s += (size_t)ws.t[k].nf * WT_STRIDE * 4; s / 1048576.0; }));
}
int main(void) {
    double t0 = now_s(); if (!wt_build_all(&ws)) { printf("out of memory\n"); return 1; }
    printf("built %d tables in %.0f ms\n", WT_NBUILT, (now_s() - t0) * 1e3);
    t_params(); t_pitch(); t_alias(); t_unison(); t_warp(); t_env(); t_matrix(); t_wav(); t_voices(); t_nan();
    wt_store_free(&ws); free(re_); free(im_);
    if (fails) { printf("%d FAILED\n", fails); return 1; }
    printf("ALL OK\n");
    return 0;
}
