// 2026-11-19: FILTER modes: gains of sines below/at/above the cutoff, all finite.
// Build: sh build.sh SRC_DIR OUT. Run in a scratch folder in /tmp with samples/ and projects/DEMO/NEON.bpp (run.sh does it).
// Prints one line per step: step, key, song hash, screen hash; then the hash of a 4-bar WAV export of NEON.
// Time is faked (SDL_GetTicks advances 40 ms per event), rand() is seeded, the library thread is never started.
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned fake_ms = 1000;
#define SDL_GetTicks fake_ticks
#define SDL_GetPerformanceCounter fake_pc
#include <SDL2/SDL.h>
Uint32 fake_ticks(void) { return fake_ms; }
static Uint64 pc_n; Uint64 fake_pc(void) { return ++pc_n; }   // DSP load reads as 0: no timing on screen
#include "engine.c"
#define logf bp_logf
#define main bp_main
#include "main.c"
#undef main
#include <time.h>


#ifdef OLD                                // 1.2: the pad is the selected one
#define PAR_ADJUST(pad, p, d, big) par_adjust(p, d, big)
#define PAR_STR(pad, p, b, n) par_str(p, b, n)
#else
#define PAR_ADJUST par_adjust
#define PAR_STR par_str
#endif
static SDL_Surface *sf; static float buf[2 * 128];
static void pump(int n) { while (n--) { callback(0, (Uint8 *)buf, sizeof buf); engine_collect_garbage(); scene_poll(); } }
// Codex's quantized stops (1.1 Beta): a global stop and a pad stop land on the LAUNCH grid, a second press cancels.
static void ev(int k, int down) { fake_ms += 40; key_event(k, down); }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL L%d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void tap(int k) { ev(k, 1); ev(k, 0); pump(1); }
static uint32_t run_until_stop(int max) { uint32_t last = 0; for (int i = 0; i < max && playing; i++) { last = pos32; pump(1); } return last; }
static double gain(int mode, int cut, int q, float hz) {   // steady gain of a sine through FILTER
    static FxState st; memset(&st, 0, sizeof st); FxSlot f = {FX_FILTER, 1, {0}}; memcpy(f.p, fx_def[FX_FILTER], 6);
    f.p[0] = (int8_t)mode; f.p[1] = (int8_t)cut; f.p[2] = (int8_t)q; float b[256]; double ei = 0, eo = 0; long ph = 0;
    for (int blk = 0; blk < 400; blk++) { for (int k = 0; k < 128; k++, ph++) b[2 * k] = b[2 * k + 1] = 0.3f * sinf(2 * (float)M_PI * hz * ph / SR);
        float in[256]; memcpy(in, b, sizeof in); fx_insert(&st, &f, 1, b, 128, 120);
        if (blk > 200) for (int k = 0; k < 256; k++) { ei += in[k] * in[k]; eo += b[k] * b[k]; if (!isfinite(b[k])) return -999; } }
    return 10 * log10(eo / ei + 1e-12);
}
int main(void) {
    // CUTOFF 60 = 20 * 2^6 = 1280 Hz
    for (int m = 0; m < FILT_N; m++) { double lo = gain(m, 60, 30, 160), at = gain(m, 60, 30, 1280), hi = gain(m, 60, 30, 10240);
        printf("%-8s 160 Hz %6.1f  1280 Hz %6.1f  10 kHz %6.1f dB\n", filt_mode[m], lo, at, hi);
        CHECK(lo > -900 && at > -900 && hi > -900, "%s not finite", filt_mode[m]); }
    CHECK(gain(FILT_LP24, 60, 30, 10240) < gain(0, 60, 30, 10240) - 15, "LP24 not steeper than LP12");
    CHECK(gain(FILT_HP24, 60, 30, 160) < gain(1, 60, 30, 160) - 15, "HP24 not steeper");
    CHECK(fabs(gain(FILT_DJ, 50, 30, 160)) < 0.5 && fabs(gain(FILT_DJ, 50, 30, 10240)) < 0.5, "DJ in the middle is not flat");
    CHECK(gain(FILT_DJ, 20, 30, 10240) < -12, "DJ left does not cut the highs"); CHECK(gain(FILT_DJ, 80, 30, 160) < -12, "DJ right does not cut the lows");
    CHECK(gain(FILT_PEAK, 60, 30, 1280) > 8, "PEAK does not boost"); CHECK(gain(FILT_LADDER, 60, 30, 10240) < -20, "LADDER does not cut");
    printf("%s (%d fails)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails != 0;
}
