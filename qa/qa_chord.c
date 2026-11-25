// 2026-11-19: CHORDS: L2 round, chords into the slots, names, back to STEPS.
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
    int s = -1; for (int i = 0; i < NSLOT; i++) if (is_synth(i)) { s = i; break; }
    CHECK(s >= 0, "a synth pad"); sel = s; song.t[s].key = 1; song.t[s].scale = 2;   // C MAJOR on this pad
    Pattern *p = cur_pat(s); p->mode = 0; for (int i = 0; i < NNOTE; i++) p->roll[i].len = 0;
    set_tab(T_SEQ); ui_cat = PC_PATTERN; srow = 1; pump(2);
    lay_go(1); pump(1); CHECK(p->mode == 1, "LAYOUT: PIANO ROLL (%d)", p->mode);   // v3: OPTION's LAYOUT items
    lay_go(2); pump(1); CHECK(cfm == CF_CHORD, "the notes of the steps: CHORDS asks first"); tap(K_LEFT); tap(K_EDIT);   // YES: an empty clip (2026-11-20)
    CHECK(p->mode == 2 && !roll_any(p), "LAYOUT: CHORDS (%d)", p->mode);
    if (rolling) roll_leave(); if (!chording) chord_open(); CHECK(chording && !rolling, "CHORDS open");
    char b[32]; ch_name(0, 0, b, sizeof b); printf("I = %s", b); CHECK(!strcmp(b, "C"), "I is %s", b);
    ch_name(0, 4, b, sizeof b); printf(", V = %s", b); CHECK(!strcmp(b, "G"), "V is %s", b);
    ch_name(1, 4, b, sizeof b); printf(", V7 = %s", b); CHECK(!strcmp(b, "G7"), "V7 is %s", b);
    ch_name(0, 6, b, sizeof b); printf(", VII = %s\n", b); CHECK(!strcmp(b, "Bdim"), "VII is %s", b);
    tap(K_EDIT); CHECK(ch_slot == 1, "EDIT goes to the next slot");
    for (int i = 0; i < 4; i++) tap(K_RIGHT);
    tap(K_EDIT);
    int r, c; CHECK(ch_slot_match(p, 0, &r, &c) == 1 && c == 0, "slot 1 holds I"); CHECK(ch_slot_match(p, 1, &r, &c) == 1 && c == 4, "slot 2 holds V");
    int cnt = 0; for (int i = 0; i < NNOTE; i++) cnt += p->roll[i].len > 0; CHECK(cnt == 6, "6 notes (%d)", cnt);
    ev(K_OPT, 1); ev(K_EDIT, 1); ev(K_EDIT, 0); ev(K_OPT, 0); pump(1);
    CHECK(ch_slot_match(p, ch_slot, &r, &c) == 0, "OPT+EDIT clears the slot");
    draw(); SDL_SaveBMP(sf, "chords.bmp");
    sim_on = 1; tap(K_Y); sim_on = 0; CHECK(!chording, "Y (MENU+UP): CHORDS closes");
    draw(); SDL_SaveBMP(sf, "patmenu2.bmp");
    lay_go(0); pump(1); CHECK(p->mode == 0, "LAYOUT: STEPS (%d)", p->mode);   // v3: no pattern menu, OPTION > LAYOUT
    printf("%s (%d fails)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails != 0;
}
