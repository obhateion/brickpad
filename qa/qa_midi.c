// Golden run for the UI v2 refactor: the same input script on two source trees must give the same hashes.
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
int main(void) {   // MIDI part 2: CLOCK IN tempo, Start/Stop from outside, a CC learnt and turning a knob
    sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888); R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1); prj_load("projects/DEMO/NEON.bpp"); pump(4);
    int fails = 0; midi_clk_in = 1;
    mclk_per_ns = (int64_t)(60e9 / (128.4 * 24)); mclk_last_ns = mono_ns(); midi_follow();
    printf("tempo %d mul %.4f\n", song.tempo, tempo_mul); if (song.tempo != 128 || fabsf(tempo_mul * song.tempo - 128.4f) > 0.05f) { fails++; printf("FAIL tempo\n"); }
    midi_push(0xFA, 0, 0); midi_poll(SDL_GetTicks()); pump(2); if (!engine_playing()) { fails++; printf("FAIL start\n"); }
    midi_push(0xFC, 0, 0); midi_poll(SDL_GetTicks()); pump(2); if (engine_playing()) { fails++; printf("FAIL stop\n"); }
    sel = 0; mlearn_pad = 0; mlearn_par = P_PITCH; midi_push(0xB0, 74, 10); midi_poll(SDL_GetTicks());
    if (mmap_n != 1 || mmap_[0].cc != 74) { fails++; printf("FAIL learn\n"); }
    midi_push(0xB0, 74, 127); midi_poll(SDL_GetTicks()); int lo, hi, a, b; par_limits(0, P_PITCH, &lo, &hi, &a, &b);
    printf("pitch %d (hi %d)\n", par_get(0, P_PITCH), hi); if (par_get(0, P_PITCH) != hi) { fails++; printf("FAIL cc\n"); }
    midi_push(0xB0, 74, 0); midi_poll(SDL_GetTicks()); if (par_get(0, P_PITCH) != lo) { fails++; printf("FAIL cc0\n"); }
    printf("%s\n", fails ? "FAILED" : "ALL OK"); return fails;
}
