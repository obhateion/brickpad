// usage: zig cc ... -O2 -Isrc -DFX_BUDGET=9999 tools/fxcost.c src/sample.c (see build-seq.sh). What each insert effect costs: 12 busy sampler pads, both insert slots of every pad on one type (24 instances),
// against the same pads with no inserts. Prints ms per 128-frame buffer (2.667 ms is all there is) and the cost of
// one instance. Build without the budget: OPT=-O2 sh tests/qa_build.sh tools/fxcost.c /tmp/qa/fxcost -DFX_BUDGET=9999
#include <stdio.h>
#include <time.h>
#include "engine.c"
#define B 128
static Sample S; static float sd[2 * SR * 4];
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
static void reset(void) {
    engine_transport(0); { float o[2 * B]; callback(0, (Uint8 *)o, sizeof o); }
    memset(voices, 0, sizeof voices); memset(&song, 0, sizeof song); memset(fxs, 0, sizeof fxs); memset(&rvb, 0, sizeof rvb); memset(&dly, 0, sizeof dly);
    song.tempo = 120; song.master = 100; song.beats = 4; mix_defaults(&song.mix); song.mix.ms[MS_LIM] = 0;
    memset(mpk, 0, sizeof mpk); memset(strip_gl, 0, sizeof strip_gl); memset(strip_gr, 0, sizeof strip_gr); memset(send_ga, 0, sizeof send_ga); memset(send_gb, 0, sizeof send_gb);
    for (int s = 0; s < NSLOT; s++) { Track *t = &song.t[s]; for (int p = 0; p < NPAT; p++) pattern_clear(&t->pat[p]); t->vol = 100; t->end = 1000; t->root = -1; t->queued = -1; cx_defaults(song.cx[s]); }
}
static int ninst(int ty) { return ty == FX_REVERB || ty == FX_DELAY ? FX_SPACES : 24; }   // 2026-11-20: only 4 of each run at once
static void run(int ty, double *mean, double *mx) {
    reset(); for (int s = 0, k = 0; s < NSLOT; s++) { smp[s] = &S; for (int i = 0; i < 2 && ty; i++) if (k < ninst(ty)) { fx_set_type(&song.mix.pad[s].ins[i], ty); k++; } }
    double tot = 0; long nb = 0; static float o1[2 * B]; *mx = 0;
    for (int rep = 0; rep < 60; rep++) { for (int s = 0; s < NSLOT; s++) engine_trig(s, 0.5f, 0, -1);
        for (int i = 0; i < 70; i++) { double t0 = now_ms(); callback(0, (Uint8 *)o1, sizeof o1); double d = now_ms() - t0; if (rep >= 4) { tot += d; if (d > *mx) *mx = d; nb++; } } }
    *mean = tot / nb;
}
int main(void) {
    SDL_Init(SDL_INIT_TIMER);
    { struct sched_param sp = {.sched_priority = 20}; if (sched_setscheduler(0, SCHED_FIFO, &sp)) fprintf(stderr, "no real-time priority\n"); }
    S.d = sd; S.n = SR * 4; for (int i = 0; i < S.n; i++) sd[2 * i] = sd[2 * i + 1] = 0.5f * sinf(2 * (float)M_PI * 220 * i / SR);
    double base, bm, m, x; run(0, &base, &bm); run(0, &base, &bm);
    printf("BASE 12 pads, no inserts: mean %.3f ms  max %.3f\n", base, bm);
    for (int ty = 1; ty < FX_N; ty++) { run(ty, &m, &x);
        int n = ninst(ty); printf("TYPE %2d %-10s x%d: mean %.3f ms  max %.3f  one instance %.1f us = %.2f %% of a buffer (weight now %d, measured %.0f)\n", ty, fx_name[ty], n, m, x, (m - base) * 1000 / n, (m - base) / n / 2.667 * 100, fx_weight[ty], (m - base) / n / 2.667 * 1000); }
    return 0;
}
