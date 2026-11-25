// 2026-11-19: DUCK points: table -> points, add/delete/move/bend keys, the engine follows the points.
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
    sim_on = 1;   // v3: X and Y come from OPTION's menu (sim_key); the actions behind them are what is tested here
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    int s = 0; while (s < NSLOT && !has_sound(s)) s++;
    set_tab(T_FX); duck_open(s); pump(2);
    uint8_t tab[16]; memcpy(tab, song.dkc[s], 16);
    { uint8_t pt[16]; dk_table_to_pts(tab, pt); float worst = 0;
      for (int i = 0; i <= 31; i++) { float x = i / 31.f, f = x * 15; int j = (int)f; float a = tab[j] + (tab[j < 15 ? j + 1 : 15] - tab[j]) * (f - j), b = dk_pts_eval(pt, x); if (fabsf(a - b) > worst) worst = fabsf(a - b); }
      printf("table -> points: worst %.1f %%\n", worst); CHECK(worst < 12, "points far from the table (%.1f)", worst); }
    dk_row = 0; dk_pt = 2;
    tap(K_X); CHECK(dk_npts(song.dkc[s]) == 8 && song.dkx[s][DX_SPARE] == DK_PTS, "X on 8 points: still 8, now points");
    ev(K_OPT, 1); ev(K_EDIT, 1); ev(K_EDIT, 0); ev(K_OPT, 0); pump(1); CHECK(dk_npts(song.dkc[s]) == 7, "OPT+EDIT deletes (%d)", dk_npts(song.dkc[s]));
    tap(K_X); CHECK(dk_npts(song.dkc[s]) == 8, "X adds (%d)", dk_npts(song.dkc[s]));
    DkPt p0 = dk_pt_get(song.dkc[s], dk_pt);
    ev(K_EDIT, 1); tap(K_UP); tap(K_UP); tap(K_RIGHT); ev(K_EDIT, 0); pump(1);
    DkPt p1 = dk_pt_get(song.dkc[s], dk_pt); CHECK(p1.y == (p0.y + 4 > 63 ? 63 : p0.y + 4), "EDIT+UP level %d -> %d", p0.y, p1.y); CHECK(p1.x >= p0.x, "EDIT+RIGHT moves");
    ev(K_X, 1); tap(K_UP); tap(K_UP); ev(K_X, 0); pump(1); CHECK(dk_pt_get(song.dkc[s], dk_pt).bend == 2, "X+UP bends (%d)", dk_pt_get(song.dkc[s], dk_pt).bend);
    CHECK(dk_npts(song.dkc[s]) == 8, "X after a bend adds nothing");
    for (int i = 0; i + 1 < dk_npts(song.dkc[s]); i++) CHECK(dk_pt_get(song.dkc[s], i).x <= dk_pt_get(song.dkc[s], i + 1).x, "points out of order at %d", i);
    // engine: GRID duck, 100 %, a V of points: down at the start, up at the end
    uint8_t *c = song.dkc[s]; memset(c, 0, 16); dk_pt_put(c, 0, (DkPt){1, 0, 0, 0}); dk_pt_put(c, 1, (DkPt){1, 31, 63, 0});
    song.dkx[s][DX_SPARE] = DK_PTS; song.dkx[s][DX_MODE] = DM_GRID; song.dkx[s][DX_GRID] = 2; song.dkx[s][DX_LEN] = 2; song.mix.pad[s].spare[DK_AMT] = 100; song.dkx[s][DX_SMOOTH] = 0;
    engine_transport(1); float lo = 2, hi = -1; for (int i = 0; i < 800; i++) { pump(1); if (dk_g[s] < lo) lo = dk_g[s]; if (dk_g[s] > hi) hi = dk_g[s]; }
    printf("engine gain with points: %.2f .. %.2f\n", lo, hi); CHECK(lo < 0.1f && hi > 0.9f, "the engine does not follow the points");
    dk_pt_put(c, 1, (DkPt){1, 16, 40, 5}); dk_pt_put(c, 2, (DkPt){1, 31, 63, 0}); dk_row = 0; dk_pt = 1; draw(); SDL_SaveBMP(sf, "duck.bmp");
    printf("%s (%d fails)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails != 0;
}
