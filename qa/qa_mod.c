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
static void shot(const char *n) { for (int i = 0; i < NSLOT; i++) flash[i] = 0; msg_t = 0; draw(); SDL_SaveBMP(sf, n); }
int main(void) {   // MODULATION: the three modes on one card, the type list skips FLANGER/PHASER, shared knobs kept
    sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888); R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1); prj_load("projects/DEMO/NEON.bpp"); pump(4);
    int fails = 0; FxSlot *f = &song.mix.pad[5].ins[0];
    fx_set_type(f, FX_DIST); fx_cycle(f, 1); if (f->type != FX_CHORUS) { fails++; printf("FAIL dist+1 = %d\n", f->type); }
    fx_cycle(f, 1); if (f->type != FX_FILTER) { fails++; printf("FAIL mod+1 = %d\n", f->type); }
    fx_cycle(f, -1); if (f->type != FX_CHORUS) { fails++; printf("FAIL filter-1 = %d\n", f->type); }
    f->p[0] = 77; f->p[5] = 33; fx_mod_mode(f, 2); if (f->type != FX_PHASER || f->p[0] != 77 || f->p[5] != 33 || f->p[4] != fx_def[FX_PHASER][4]) { fails++; printf("FAIL mode\n"); }
    sel = 5; cat_open(PC_FX); fx2_slot = 0; fx2_cur = 0; shot("m0.bmp");
    fx_tap(); shot("m1.bmp"); if (f->type != FX_CHORUS) { fails++; printf("FAIL tap round %d\n", f->type); }
    fx_value(1, 0); shot("m2.bmp"); if (f->type != FX_FLANGER) { fails++; printf("FAIL arrow %d\n", f->type); }
    pump(400); printf("%s\n", fails ? "FAILED" : "ALL OK"); return fails;
}
