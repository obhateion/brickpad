// 2026-11-20: PRB modes: NOTE gates, SUB splits at its chance, JUMP takes another step's note; the panel keys; copy and paste.
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
static int run_step(int s, int st, double *rate) {   // one step through the engine: how many voices it starts, the first one's rate
    memset(voices, 0, sizeof voices); Track *t = &song.t[s]; const Pattern *p = &t->pat[t->cur];
    t->on = 1; t->queued = -1; want[s] = -1; anchor[s] = 0; anchor_set[s] = 0; skip_step[s] = 0; playing = 1;
    tick((uint32_t)(st * div32[p->div % 6]), 0, SR * 60.0 / song.tempo / 8);
    int n = 0; *rate = 0; for (int i = 0; i < NVOICE; i++) if (voices[i].on && voices[i].slot == s) { if (!n) *rate = voices[i].rate; n++; }
    return n;
}
int main(void) {
    srand(1); sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1);
    sim_on = 1;   // v3: X and Y come from OPTION's menu (sim_key); the actions behind them are what is tested here
    prj_load("projects/DEMO/NEON.bpp"); pump(4); engine_transport(0); pump(4);
    int s = -1; for (int i = 0; i < NSLOT; i++) if (has_sound(i) && song.t[i].type == 0 && !track_nslices(i)) { s = i; break; }
    CHECK(s >= 0, "a plain sample pad");
    Pattern *p = cur_pat(s); p->mode = 0; int A = 2, B = 5; double r0, r1;
    p->trig[A] = p->trig[B] = 1; p->note[A] = 60; p->note[B] = 67; p->rep[A] = 1; p->rep[B] = 1;   // 2026-11-24: no SUB cells any more p->prob[A] = 100; p->prob[B] = 100;
    int nb = run_step(s, B, &r1); CHECK(nb >= 1, "step B plays");
    CHECK(run_step(s, A, &r0) == 1, "A: one hit");
    p->prob[A] = 0; CHECK(run_step(s, A, &r0) == 0, "NOTE at 0%%: silent");
    // the panel: EDIT on the PRB lane opens it, EDIT+> makes JUMP (2026-11-24: SUB is gone, NOTE or JUMP)
    sel = s; set_tab(T_SEQ); ui_cat = PC_PATTERN; srow = 1; lane = L_PROB; spage = 0; scol = A; pump(1);
    prb_open(A); CHECK(prbing, "PRB opens"); p->prob[A] = 100;
    ev(K_EDIT, 1); tap(K_RIGHT); ev(K_EDIT, 0); pump(1);
    JumpStep *j = jmp_find(s, song.t[s].cur, A); CHECK(j && j->mode == PM_JUMP, "EDIT+> : JUMP");
    tap(K_DOWN); tap(K_DOWN); tap(K_DOWN); tap(K_DOWN); CHECK(pr_row == PR_A1, "row SOURCE 1 (%d)", pr_row);
    ev(K_EDIT, 1); for (int i = 0; i <= B; i++) tap(K_RIGHT); for (int i = 0; i < 10; i++) tap(K_UP); ev(K_EDIT, 0); pump(1);
    CHECK(j->a[0] == B && j->ch[0] == 100, "source 1 = step %d at %d%% (want %d, 100)", j->a[0] + 1, j->ch[0], B + 1);
    int na = run_step(s, A, &r0); printf("JUMP: %d voices, rate %.4f (step B %.4f)\n", na, r0, r1);
    CHECK(na == 1 && fabs(r0 - r1) < 1e-6, "JUMP plays B's note");
    // RANGE of NOTE degrees, chance 100: a note of the scale
    j->kind = 1; j->form = 1; j->a[0] = 0; j->a[1] = 6; CHECK(run_step(s, A, &r0) == 1, "RANGE plays");
    // copy and paste keep the modes; an erased step forgets them
    act_pattern_copy(p); int q2 = (song.t[s].cur + 1) % NPAT; act_pattern_paste(&song.t[s].pat[q2]);
    CHECK(jmp_find(s, q2, A) != NULL, "paste keeps the PRB mode");
    tap(K_Y); CHECK(!prbing, "Y leaves");
    step_reset(p, A); CHECK(!jmp_find(s, song.t[s].cur, A), "an erased step drops its PRB mode");
    draw(); prb_open(B); ev(K_EDIT, 1); tap(K_RIGHT); tap(K_RIGHT); ev(K_EDIT, 0); pump(1); draw(); SDL_SaveBMP(sf, "prb.bmp");
    printf("%s (%d fails)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails != 0;
}
