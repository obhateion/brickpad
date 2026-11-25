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

// 2026-11-20: X on HOME starts / stops the open pad's clip (the night run once saw it do nothing); CHORDS asks YES / NO.
static void ev(int k, int down) { fake_ms += 40; key_event(k, down); }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL L%d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void tap(int k) { ev(k, 1); ev(k, 0); pump(1); }
int main(void) {
    srand(1); sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1);
    sim_on = 1;   // v3: X and Y come from OPTION's menu (sim_key); the actions behind them are what is tested here
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    set_tab(T_GRID); song.launch = 1;
    int tested = 0;
    for (int s = 0; s < NSLOT; s++) {
        if (!has_sound(s) || !pat_used(cur_pat(s))) continue;
        sel = s; int was = song.t[s].on;
        tap(K_X); pump(1600);                           // past a bar: the launch has landed
        CHECK(song.t[s].on == !was, "pad %d: X did not %s its clip (on %d, playing %d)", s + 1, was ? "stop" : "start", song.t[s].on, playing);
        tap(K_X); pump(1600);
        CHECK(song.t[s].on == was, "pad %d: second X did not undo it", s + 1);
        tested++;
    }
    CHECK(tested > 0, "no pad with a clip in NEON");
    printf("X tested on %d pads\n", tested);
    // CHORDS on a clip with notes: YES clears the roll, NO keeps it, Y stays in the mode it was
    int s = -1; for (int i = 0; i < NSLOT; i++) if (!track_nslices(i) && pat_used(cur_pat(i))) { s = i; break; }
    CHECK(s >= 0, "no unsliced pad with notes");
    if (s >= 0) {
        sel = s; Pattern *p = cur_pat(s); p->mode = 1; if (!roll_any(p)) roll_add(p, 0, 8, 60, 100);
        pat_mode_step(1); CHECK(cfm == CF_CHORD && p->mode == 1, "no question before CHORDS (cfm %d mode %d)", cfm, p->mode);
        tap(K_Y); CHECK(!cfm && p->mode == 1 && roll_any(p), "Y: should stay as it was");
        pat_mode_step(1); tap(K_EDIT); CHECK(!cfm && p->mode == 2 && roll_any(p), "NO: chords over the notes (mode %d)", p->mode);
        p->mode = 1; pat_mode_step(1); tap(K_LEFT); tap(K_EDIT); CHECK(!cfm && p->mode == 2 && !roll_any(p), "YES: empty clip (mode %d any %d)", p->mode, roll_any(p));
        p->mode = 1; pat_mode_step(1); CHECK(!cfm && p->mode == 2, "empty clip: no question");
    }
    if (!access("projects/DEMO/SWITCHBACK.bpp", F_OK)) {   // the 1.1 Beta demo (older format) opens whole in 1.2
        prj_load("projects/DEMO/SWITCHBACK.bpp"); pump(4); int n = 0; for (int i = 0; i < NSLOT; i++) n += has_sound(i) && pat_used(cur_pat(i));
        CHECK(n >= 4 && scn_n >= 4, "SWITCHBACK: %d pads with clips, %d scenes", n, scn_n); printf("SWITCHBACK: %d pads, %d scenes\n", n, scn_n); }
    printf(fails ? "FAILED (%d fails)\n" : "ALL OK (%d fails)\n", fails);
    return fails != 0;
}
