// 2026-11-19: REVERB and DELAY inserts: a tail after the notes, the pool of 4, dry when full.
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
static double tail_energy(int s, int type) {   // play pad s alone for 1 bar, stop, then the energy of the next 1.5 s
    FxSlot *f = &song.mix.pad[s].ins[0]; f->type = (uint8_t)type; f->on = type != 0; memcpy(f->p, fx_def[type], 6); if (type) f->p[5] = 100;
    song.mix.pad[s].ins[1].on = 0; pump(300);
    for (int i = 0; i < NSLOT; i++) song.t[i].on = 0;
    song.t[s].on = 1; engine_transport(1); pump(4 * 48000 * 60 / song.tempo / 128);
    engine_transport(0); engine_stop_all(); song.t[s].on = 0; pump(20);
    double e = 0; for (int i = 0; i < 48000 * 3 / 2 / 128; i++) { pump(1); for (int k = 0; k < 256; k++) e += buf[k] * buf[k]; }
    return e;
}
int main(void) {
    srand(1); sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1);
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    for (int i = 0; i < NSLOT; i++) { song.mix.pad[i].send_a = song.mix.pad[i].send_b = 0; }
    int s = -1; for (int i = 0; i < NSLOT; i++) if (has_sound(i) && pat_used(cur_pat(i)) && !is_synth(i)) { s = i; break; }
    CHECK(s >= 0, "a sample pad with notes");
    double e0 = tail_energy(s, 0), er = tail_energy(s, FX_REVERB), ed = tail_energy(s, FX_DELAY);
    printf("pad %d tail energy: dry %.4g  reverb %.4g  delay %.4g\n", s + 1, e0, er, ed);
    CHECK(er > e0 * 10 + 1e-3, "reverb has no tail"); CHECK(ed > e0 * 10 + 1e-3, "delay has no tail");
    // six pads with REVERB: four lines, the others dry, nothing blows up
    int n = 0; for (int i = 0; i < NSLOT && n < 6; i++) if (has_sound(i)) { FxSlot *f = &song.mix.pad[i].ins[0]; f->type = FX_REVERB; f->on = 1; memcpy(f->p, fx_def[FX_REVERB], 6); song.t[i].on = pat_used(cur_pat(i)); n++; }
    engine_transport(1); pump(2000); int held = 0; for (int i = 0; i < FX_SPACES; i++) held += sp_own[0][i] != NULL;
    double pk = 0; for (int i = 0; i < 200; i++) { pump(1); for (int k = 0; k < 256; k++) if (fabs(buf[k]) > pk) pk = fabs(buf[k]); }
    printf("reverb lines in use %d of %d, peak %.3f\n", held, FX_SPACES, pk); CHECK(held == FX_SPACES, "pool not used"); CHECK(isfinite(pk) && pk > 0.001, "silent or broken");
    printf("%s (%d fails)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails != 0;
}
