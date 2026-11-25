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
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    int lq[4] = {1, 2, 3, 4}, unit[4] = {32, 64, 128, 1};   // 1 BAR, 2 BARS, 4 BARS, NONE (a 1/32)
    for (int c = 0; c < 4; c++) {
        song.launch = lq[c]; song.beats = 4; set_tab(T_GRID);
        if (playing) { tap(K_PLAY); run_until_stop(20000); }
        tap(K_PLAY); pump(300);                         // all clips start
        CHECK(playing, "launch %d: did not start", c);
        for (int i = 0; i < 37; i++) pump(1);           // somewhere inside a bar
        uint32_t at = pos32; tap(K_PLAY);
        CHECK(playing || unit[c] == 1, "launch %d: stopped at once (pos %u)", c, at);
        uint32_t end = run_until_stop(40000);
        CHECK(!playing, "launch %d: never stopped", c);
        CHECK(end % unit[c] == 0 || unit[c] == 1, "launch %d: stop at 1/32 #%u, not on the grid of %d", c, end, unit[c]);   // end: the tick the stop happened on (2026-11-20: was counted one late)
        printf("launch %d: press at %u, stopped at %u\n", c, at, end + 1);
        int on = 0; for (int s = 0; s < NSLOT; s++) on |= song.t[s].on; CHECK(!on, "launch %d: a clip is still on", c);
    }
    // cancel: press, press again before the bar
    song.launch = 1; tap(K_PLAY); pump(300); pump(10); tap(K_PLAY); CHECK(engine_stop_pending(), "no stop queued");
    tap(K_PLAY); CHECK(!engine_stop_pending(), "cancel failed");
    pump(3000); CHECK(playing, "cancelled stop still stopped");
    // a pad stop inside the pad (PLAY toggles this pad's clip on the grid)
    int s = -1; for (int i = 0; i < NSLOT; i++) if (song.t[i].on) { s = i; break; }
    CHECK(s >= 0, "no clip plays");
    if (s >= 0) { sel = s; set_tab(T_GRID); pump(5); uint32_t a = pos32; engine_launch(s, 0); pump(1);
        printf("after X: on %d want %d tab %d hub %d\n", song.t[s].on, engine_want(s), tab, 0); CHECK(song.t[s].on, "pad %d stopped at once", s);
        int k = 0; while (song.t[s].on && k++ < 5000) pump(1);
        printf("pad %d: press at %u, off by %u\n", s + 1, a, pos32);
        CHECK(!song.t[s].on, "pad never stopped"); CHECK(playing, "the count stopped with the pad"); }
    printf("%s (%d fails)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails != 0;
}
