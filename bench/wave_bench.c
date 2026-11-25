// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// WAVE CPU: ns per voice per 128-frame buffer (2 oscillators, LP24, position between frames: the morph read),
// for unison 1, 4, 8 at ECO and HIGH, a warp case, and the target scene (8 voices x 2 osc x unison 4).
// Build: cc -O2 -std=gnu11 -Isrc bench/wave_bench.c -o wave_bench -lm && ./wave_bench [A53 slowdown, default 7]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "wave.h"

static WaveStore ws;
static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static volatile float sink;
// ns per buffer for the whole instrument with nv voices held (best of 5 runs of nb buffers).
static double run(const int8_t *wp, int nv, int nb) {
    double best = 1e30; static float out[256];
    for (int rep = 0; rep < 5; rep++) {
        WaveInst w; wv_init(&w);
        for (int k = 0; k < nv; k++) wv_note_on(&w, wp, 40 + k * 5, 0.9f, -1, 0);
        for (int b = 0; b < 64; b++) { memset(out, 0, sizeof out); wv_render(&w, wp, &ws, out, 128); }   // past the attack
        double t0 = now_s();
        for (int b = 0; b < nb; b++) { memset(out, 0, sizeof out); wv_render(&w, wp, &ws, out, 128); sink += out[0]; }
        double t = (now_s() - t0) / nb * 1e9; if (t < best) best = t;
        if (wv_active(&w) != nv) { printf("voices dropped\n"); exit(1); }
    }
    return best;
}
static void setup(int8_t *wp, int uni, int hq) {
    wp_defaults(wp);
    wp[WP_A + WO_TABLE] = 0; wp[WP_A + WO_POS] = 50; wp[WP_B + WO_ON] = 1; wp[WP_B + WO_TABLE] = 1; wp[WP_B + WO_POS] = 37; wp[WP_B + WO_SEMI] = 7;
    wp[WP_A + WO_UNI] = wp[WP_B + WO_UNI] = uni; wp[WP_F_TYPE] = WF_LP24; wp[WP_F_CUT] = 60; wp[WP_F_RES] = 30; wp[WP_ENV1 + WE_SUS] = 80;
    wp[WP_QUALITY] = hq;
}
int main(int argc, char **argv) {
    double slow = argc > 1 ? atof(argv[1]) : 7, buf_ns = 128.0 / SR * 1e9;
    if (!wt_build_all(&ws)) return 1;
    int8_t wp[WP_N]; int nb = 20000;
    printf("WAVE bench, x86 -O2, 128-frame buffers (%.0f us of audio), 2 osc + LP24, ns per voice per buffer\n", buf_ns / 1e3);
    printf("%-34s %10s %10s\n", "", "ECO", "HIGH");
    int us[3] = {1, 4, 8};
    for (int i = 0; i < 3; i++) {
        double r[2];
        for (int hq = 0; hq < 2; hq++) { setup(wp, us[i], hq); r[hq] = run(wp, 1, nb); }
        printf("unison %d (%2d table reads a frame)     %10.0f %10.0f\n", us[i], 2 * us[i], r[0], r[1]);
    }
    { setup(wp, 4, 0); wp[WP_A + WO_WARP] = WW_FM; wp[WP_A + WO_WAMT] = 40; wp[WP_B + WO_WARP] = WW_BENDP; wp[WP_B + WO_WAMT] = 50;
      double e = run(wp, 1, nb); wp[WP_QUALITY] = 1; printf("unison 4, warps FM + BEND+             %10.0f %10.0f\n", e, run(wp, 1, nb)); }
    { setup(wp, 4, 0); wp[WP_F_TYPE] = WF_LADDER; double e = run(wp, 1, nb); wp[WP_QUALITY] = 1; printf("unison 4, LADDER                       %10.0f %10.0f\n", e, run(wp, 1, nb)); }
    { setup(wp, 4, 0); wp[WP_SUB_LEVEL] = 50; wp[WP_NOISE_LEVEL] = 20; wp[WP_NOISE_TYPE] = 1; double e = run(wp, 1, nb); wp[WP_QUALITY] = 1; printf("unison 4, + SUB + pink NOISE           %10.0f %10.0f\n", e, run(wp, 1, nb)); }
    // the target: 8 voices x 2 osc x unison 4, LP24, four matrix routes moving position, cutoff and pitch
    printf("\ntarget scene: 8 voices x 2 osc x unison 4, LP24, 4 matrix routes (LFO1->A POS, LFO2->CUTOFF, ENV2->B POS, LFO3->A FINE)\n");
    for (int hq = 0; hq < 2; hq++) {
        setup(wp, 4, hq);
        int8_t *m = wp + WP_MTX;
        m[0] = WS_LFO1; m[1] = WP_A + WO_POS + 1; m[2] = 40; m[3] = 1;
        m[4] = WS_LFO2; m[5] = WP_F_CUT + 1; m[6] = 20; m[7] = 1;
        m[8] = WS_ENV2; m[9] = WP_B + WO_POS + 1; m[10] = 50; m[11] = 0;
        m[12] = WS_LFO3; m[13] = WP_A + WO_FINE + 1; m[14] = 3; m[15] = 1;
        wp[WP_LFO2 + WL_RATE] = 70; wp[WP_LFO3 + WL_RATE] = 60;
        double t = run(wp, 8, nb / 2);
        printf("%s: %.1f us per buffer = %.1f %% of one x86 core; A53 estimate (x%.0f) %.0f %% of one core\n",
               hq ? "HIGH" : "ECO ", t / 1e3, 100 * t / buf_ns, slow, 100 * t * slow / buf_ns);
    }
    wt_store_free(&ws);
    return 0;
}
