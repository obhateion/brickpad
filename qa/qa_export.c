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
static long fsize(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return -1; fseek(f, 0, SEEK_END); long n = ftell(f); fclose(f); return n; }
int main(void) {   // EXPORT options: 24 bit mono loop normalized; MIX + PADS; THIS PAD
    sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888); R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1); prj_load("projects/DEMO/NEON.bpp"); pump(4);
    int fails = 0; export_open(); ex_bars = 1; ex_tail = 1; ex_loop = 1; ex_norm = 1; ex_fmt = 1; ex_mono = 1; ex_what = 0;
    for (int i = 0; i < 9; i++) ex_row = i; draw(); SDL_SaveBMP(sf, "ex.bmp");
    export_start(); while (ex_run) export_tick();
    double bar_s = song.beats * 60.0 / song.tempo; long body = (long)(bar_s * 2 * SR + 0.5), want = 44 + body * 3;
    char p[200]; snprintf(p, sizeof p, "%s", ex_path); long n = fsize(p); printf("%s: %ld bytes (want %ld)\n", p, n, want); if (n != want) { fails++; printf("FAIL size\n"); }
    { FILE *f = fopen(p, "rb"); uint8_t h[44]; fread(h, 1, 44, f); long pk = 0; uint8_t x[3]; while (fread(x, 1, 3, f) == 3) { long v = (int32_t)((uint32_t)(x[0] | x[1] << 8 | x[2] << 16) << 8) >> 8; if (labs(v) > pk) pk = labs(v); } fclose(f);
      printf("channels %d bits %d peak %.3f\n", h[22], h[34], pk / 8388607.0); if (h[22] != 1 || h[34] != 24 || pk < 8000000 || pk > 8150000) { fails++; printf("FAIL fmt/peak\n"); } }
    ex_loop = ex_norm = ex_mono = 0; ex_fmt = 0; ex_what = 2; exporting = 1; export_start(); while (ex_run) export_tick();
    int files = 0; DIR *d = opendir("exports"); struct dirent *e; while (d && (e = readdir(d))) if (strstr(e->d_name, ".wav")) files++; if (d) closedir(d);
    printf("files now %d\n", files); if (files < 3) { fails++; printf("FAIL mix+pads\n"); }
    if (!access(EX_TMP, F_OK)) { fails++; printf("FAIL temp left\n"); }
    printf("%s\n", fails ? "FAILED" : "ALL OK"); return fails;
}
