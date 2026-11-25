// 2026-11-20: automation lane drawn as DUCK (points, bends): a picture to look at.
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
static void pumpd(int n) { while (n--) { callback((void *)1, (Uint8 *)buf, sizeof buf); engine_collect_garbage(); scene_poll(); } }   // as the device
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL L%d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
// 2026-11-20: METRONOME sound / vibration apart, its subdivisions and WHEN; PROJECT regrouped (every setting once).
static uint32_t buzzes_in_bar(void) {     // from the bar's start, one bar: how many beats were posted to the vibration
    uint32_t n0 = vib_ring.n; double bar = SR * 60.0 / song.tempo * song.beats; pumpd((int)(bar / 128) - 1); return vib_ring.n - n0;
}
int main(void) {
    srand(1); sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pumpd(1);
    int seen[J_N] = {0};
    for (int k = 0; k < JS_N; k++) for (int i = 0; i < JS_ROWS && prj_sec[k][i] >= 0; i++) seen[prj_sec[k][i]]++;
    for (int j = 0; j < J_N; j++) CHECK(seen[j] == (j == J_METRO || j == J_NAV ? 0 : 1), "%s is in %d sections", prj_name[j], seen[j]);
    for (int j = 0; j < J_N; j++) if (j != J_ABOUT) CHECK(prj_help[j] != NULL, "%s has no help", prj_name[j]);
    song.tempo = 120; song.beats = 4; song.t[0].on = 1;
    metro.snd = 0; metro.vib = 1; metro.when = 0; metro.div = 0; engine_transport(1); pumpd(2);
    while (engine_pos32() % 32) pumpd(1);
    uint32_t b = buzzes_in_bar(); CHECK(b == 4, "1/4: %u buzzes a bar", b);
    metro.div = 1; while (engine_pos32() % 32) pumpd(1); b = buzzes_in_bar(); CHECK(b == 8, "1/8: %u", b);
    metro.div = 2; while (engine_pos32() % 32) pumpd(1); b = buzzes_in_bar(); CHECK(b == 16, "1/16: %u", b);
    int bars = 0; for (uint32_t k = vib_ring.n - 16; k != vib_ring.n; k++) bars += vib_ring.kind[k % 16] == 2; CHECK(bars == 1, "the bar's first: %d long buzzes", bars);
    metro.when = 1; metro.rec = 0; while (engine_pos32() % 32) pumpd(1); b = buzzes_in_bar(); CHECK(b == 0, "WHEN RECORDING, not recording: %u", b);
    metro.rec = 1; metro.div = 0; while (engine_pos32() % 32) pumpd(1); b = buzzes_in_bar(); CHECK(b == 4, "recording: %u", b);
    metro.vib = 0; while (engine_pos32() % 32) pumpd(1); b = buzzes_in_bar(); CHECK(b == 0, "vibration off: %u", b);
    // a freeze / export (not the device) never clicks
    metro.snd = 1; metro.when = 0; float o[2 * 128]; double mx = 0; while (engine_pos32() % 32) pumpd(1);
    for (int i = 0; i < 40; i++) { callback(0, (Uint8 *)o, sizeof o); for (int k = 0; k < 256; k++) if (fabs(o[k]) > mx) mx = fabs(o[k]); }
    CHECK(mx < 1e-4, "an export clicks (%f)", mx);
    metro_cycle(); CHECK(metro.snd && metro.vib, "MENU+X: sound -> sound + vibration");
    metro_cycle(); CHECK(!metro.snd && metro.vib, "-> vibration"); metro_cycle(); CHECK(!metro.snd && !metro.vib, "-> off"); metro_cycle(); CHECK(metro.snd && !metro.vib, "-> sound");
    metro.snd = 1; metro.vib = 1; metro.div = 1; engine_transport(0); pumpd(2);   // pictures of the new PROJECT
    set_tab(T_PRJ); jsec = JS_METRO; jcol = 1; jcur = J_MSND; pumpd(1); draw(); SDL_SaveBMP(sf, "shot_prj_metro.bmp");
    jcur = J_MDIV; draw(); SDL_SaveBMP(sf, "shot_prj_metro2.bmp");
    jsec = JS_SET; jcur = J_VIEW; draw(); SDL_SaveBMP(sf, "shot_prj_settings.bmp");
    jsec = JS_TEMPO; jcol = 0; jcur = J_TEMPO; draw(); SDL_SaveBMP(sf, "shot_prj_tempo.bmp");
    remove("metro.cfg");
    printf(fails ? "FAILED (%d fails)\n" : "ALL OK (%d fails)\n", fails);
    return fails != 0;
}
