// 2026-11-19: the count stops with the last clip; a clip from stop starts the bar at once; TAP over 12 taps.
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
int main(void) {
    srand(1); sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1);
    prj_load("projects/DEMO/NEON.bpp"); pump(4); song.launch = 1; song.beats = 4; set_tab(T_GRID);
    int a = -1, b = -1; for (int i = 0; i < NSLOT; i++) if (has_sound(i) && pat_used(cur_pat(i))) { if (a < 0) a = i; else if (b < 0) b = i; }
    CHECK(a >= 0 && b >= 0, "two clips with notes");
    clip_toggle(a); pump(2); CHECK(playing, "a clip from stop starts the count");
    clip_toggle(b); pump(400); launch_tick(); pump(2); CHECK(playing, "two clips play");
    engine_launch(a, 0); for (int k = 0; k < 5000 && song.t[a].on; k++) pump(1);
    launch_tick(); pump(2); CHECK(playing, "one clip left: the count goes on");
    engine_launch(b, 0); for (int k = 0; k < 5000 && song.t[b].on; k++) pump(1);
    CHECK(!song.t[b].on, "b stopped");
    launch_tick(); pump(2); CHECK(!playing, "no clip: the count stopped");
    pump(37); clip_toggle(a); pump(2); CHECK(playing && pos32 < 4, "a new start from the bar's beginning (pos %u)", pos32);
    // TAP: 128 BPM = 468.75 ms, with +-15 ms of jitter
    { Uint32 t[TAP_N]; int n = 0, r = 0; unsigned base = 50000; static const int j[14] = {0, 12, -9, 15, -14, 3, 8, -11, 6, -2, 13, -15, 4, 0};
      for (int i = 0; i < 14; i++) { fake_ms = base + (unsigned)(i * 468.75) + j[i]; r = tap_bpm(t, &n); }
      printf("tap: %d BPM from %d taps\n", r, n); CHECK(r == 128, "tap gave %d", r); }
    printf("%s (%d fails)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails != 0;
}
