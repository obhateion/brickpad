// 2026-11-19: RING SOURCE = another pad: sound while that pad plays, silence when it does not.
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
static double energy(int a, int b, int bon) {
    for (int i = 0; i < NSLOT; i++) song.t[i].on = 0;
    song.t[a].on = 1; song.t[b].on = (int8_t)bon; engine_transport(1); double e = 0;
    for (int i = 0; i < 1500; i++) { pump(1); if (i > 300) for (int k = 0; k < 256; k++) e += buf[k] * buf[k]; }
    engine_transport(0); engine_stop_all(); pump(400); return e;
}
int main(void) {
    srand(1); sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1);
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    for (int i = 0; i < NSLOT; i++) { song.mix.pad[i].send_a = song.mix.pad[i].send_b = 0; song.mix.pad[i].ins[0].on = song.mix.pad[i].ins[1].on = 0; }
    int a = -1, b = -1; for (int i = 0; i < NSLOT; i++) if (has_sound(i) && pat_used(cur_pat(i))) { if (a < 0) a = i; else if (b < 0) b = i; }
    FxSlot *f = &song.mix.pad[a].ins[0]; f->type = FX_RING; f->on = 1; memcpy(f->p, fx_def[FX_RING], 6); f->p[4] = 100;
    song.mbx[a][0][5] = (int8_t)(b + 1); song.t[b].vol = 0; pump(50);
    double e1 = energy(a, b, 1), e0 = energy(a, b, 0);
    song.mbx[a][0][5] = 0; double eo = energy(a, b, 0);
    printf("pad %d ring by pad %d: playing %.4g, silent %.4g, oscillator %.4g\n", a + 1, b + 1, e1, e0, eo);
    CHECK(e1 > 1e-3, "no sound with the source playing"); CHECK(e0 < e1 * 1e-3, "sound without the source"); CHECK(eo > 1e-3, "oscillator mode silent");
    printf("%s (%d fails)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails != 0;
}
