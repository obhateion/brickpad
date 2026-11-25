// 2026-11-25 (user: only the new format): a project of an older format opened once and saved as the new one, for the qa fixtures.
// Usage in a folder with samples/: mkfix IN.bpp NAME -> projects/NAME.bpp
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned fake_ms = 1000;
#define SDL_GetTicks fake_ticks
#define SDL_GetPerformanceCounter fake_pc
#include <SDL2/SDL.h>
Uint32 fake_ticks(void) { return fake_ms; }
static Uint64 pc_n; Uint64 fake_pc(void) { return ++pc_n; }
#include "engine.c"
#define logf bp_logf
#define main bp_main
#include "main.c"
#undef main
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888); R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup();
    prj_load(argv[1]); prj_save(argv[2]);
    printf("magic %08x scenes %d\n", song.magic, scn_n); return 0;
}
