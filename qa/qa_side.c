// 2026-11-24: the side slots (side.h): the cursor goes onto them at the page's edges and back where it was, the slots
// do what OPTION's items did, LISTEN STEP plays on landing, the grid's DOWN grows a bar. Pictures of every page with slots.
// Build and run as qa_subshots (qseq.sh side), in /tmp/qx.
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
#define ctxing 0   // 2026-11-24: the OPTION menu is gone (M8: OPTION is a modifier)
#undef main
static SDL_Surface *sf; static float buf[2 * 128]; static int fails;
static void pump(int n) { while (n--) { callback(0, (Uint8 *)buf, sizeof buf); engine_collect_garbage(); scene_poll(); } }
static void ev(int k, int down) { fake_ms += 40; key_event(k, down); pump(1); }
static void tap(int k) { ev(k, 1); ev(k, 0); }
static void combo(int m, int k) { ev(m, 1); tap(k); ev(m, 0); }
static void shot(const char *f) { pump(2); fake_ms += 2000; draw(); SDL_SaveBMP(sf, f); printf("shot %s\n", f); }
#define CHECK(c, m) do { if (!(c)) { printf("FAIL %s\n", m); fails++; } else printf("ok   %s\n", m); } while (0)
int main(void) {
    srand(1); sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1);
    prj_load("projects/DEMO/NEON.bpp"); pump(4); lst_on = 0;
    int s = -1;
    for (int i = 0; i < NSLOT && s < 0; i++) { Pattern *p = cur_pat(i); if (has_sound(i) && !p->mode && pat_used(p) && p->len == 16) s = i; }
    if (s < 0) { printf("FAILED no pad\n"); return 1; }
    sel = s; set_tab(T_SEQ); cat_open(PC_PATTERN); lane = L_TRIG; set_step(0); pump(1);
    CHECK(side_page() == SD_STEPS, "STEPS has slots");
    for (int i = 0; i < 7; i++) tap(K_RIGHT);
    CHECK(cur_step() == 7 && !side_foc, "RIGHT stays in the row of eight");
    tap(K_RIGHT); CHECK(side_foc == 5, "RIGHT at the edge: RECORD");
    shot("side_1_record.bmp");
    tap(K_DOWN); CHECK(side_foc == 6, "DOWN: LISTEN");
    tap(K_EDIT); CHECK(lst_on == 1, "EDIT: LISTEN ON");
    tap(K_LEFT); CHECK(!side_foc && cur_step() == 7, "LEFT: back on step 8");
    tap(K_DOWN); CHECK(cur_step() == 15, "DOWN: the bottom row");
    tap(K_RIGHT); CHECK(side_foc == 7, "RIGHT from the bottom row: DOUBLE");
    tap(K_LEFT); for (int i = 0; i < 7; i++) tap(K_LEFT);
    CHECK(cur_step() == 8 && !side_foc, "LEFT along the bottom row");
    tap(K_LEFT); CHECK(side_foc == 3, "LEFT at the edge: PATTERN");
    tap(K_UP); CHECK(side_foc == 2, "UP: LEN/GRID");
    { Pattern *p = cur_pat(sel); int d0 = p->div;
      combo(K_EDIT, K_UP); CHECK(p->len == 32, "EDIT+UP: a bar longer");
      combo(K_EDIT, K_DOWN); CHECK(p->len == 16, "EDIT+DOWN: a bar shorter");
      combo(K_EDIT, K_RIGHT); CHECK(p->div != d0, "EDIT+RIGHT: the grid");
      combo(K_EDIT, K_LEFT); CHECK(p->div == d0, "EDIT+LEFT: the grid back"); }
    shot("side_2_len.bmp");
    tap(K_RIGHT); CHECK(!side_foc && cur_step() == 8, "RIGHT: back on step 9");
    tap(K_DOWN); CHECK(cur_pat(sel)->len == 32 && cur_step() == 16, "DOWN from the bottom row grows a bar");
    tap(K_UP); CHECK(cur_pat(sel)->len == 16 && cur_step() == 8, "UP: the empty bar goes again");
    tap(K_OPT); CHECK(!ctxing, "OPTION: no menu (M8: a modifier, a tap listens)");
    set_step(0); tap(K_LEFT); CHECK(side_foc == 1, "LEFT from step 1: LAYOUT");
    combo(K_EDIT, K_DOWN); pump(2);
    CHECK(rolling && cur_pat(sel)->mode == 1 && side_foc == 1, "EDIT+DOWN on LAYOUT: PIANO ROLL, the cursor stays on it");
    shot("side_3_roll.bmp");
    tap(K_RIGHT); CHECK(!side_foc, "RIGHT: back in the roll");
    combo(K_EDIT, K_UP); pump(2);   // as the roll: EDIT+UP on an empty cell is nothing
    side_foc = 1; combo(K_EDIT, K_UP); pump(2); CHECK(!rolling && cur_pat(sel)->mode == 0, "EDIT+UP on LAYOUT: STEPS again");
    // 2026-11-24: the roll's GRID kinds, a note nudged, RHYTHM
    lay_to(1); pump(2); CHECK(rolling && !rl_rhythm, "LAYOUT: ROLL");
    side_foc = 3; tap(K_EDIT); CHECK(rl_gtype == RG_TRIPLET && rl_snap == rl_snaps[rl_gi] * 2 / 3, "GRID: TRIPLET");
    side_foc = 3; tap(K_EDIT); CHECK(rl_gtype == RG_DOTTED && rl_snap == rl_snaps[rl_gi] * 3 / 2, "GRID: DOTTED");
    side_foc = 3; tap(K_EDIT); CHECK(rl_gtype == RG_FREE, "GRID: FREE"); side_foc = 0;
    { Pattern *p = cur_pat(sel); for (int i = 0; i < NNOTE; i++) p->roll[i].len = 0; rl_t = 0; rl_n = 60; roll_add(p, 0, 24, 60, 100);
      ev(K_OPT, 1); ev(K_EDIT, 1); tap(K_RIGHT); tap(K_RIGHT); ev(K_EDIT, 0); ev(K_OPT, 0); int i = roll_find(p, 2, 60);
      CHECK(i >= 0 && p->roll[i].start == 2 && p->roll[i].len, "FREE: OPT+EDIT+> moves the note a tick each, and keeps it"); }
    side_foc = 3; tap(K_EDIT); CHECK(rl_gtype == RG_STRAIGHT, "GRID: STRAIGHT again"); side_foc = 0;
    lay_to(2); pump(2); CHECK(rolling && rl_rhythm, "LAYOUT: RHYTHM"); shot("side_r_rhythm.bmp");
    { Pattern *p = cur_pat(sel); rl_t = 0; int v0 = p->roll[roll_find(p, 2, 60) >= 0 ? roll_find(p, 2, 60) : 0].vel; rl_t = 2;
      ev(K_EDIT, 1); tap(K_UP); ev(K_EDIT, 0); int i = roll_find(p, 2, 60); CHECK(i >= 0 && p->roll[i].vel == v0 + 8, "RHYTHM: EDIT+UP velocity"); }
    lay_to(0); pump(2); CHECK(!rolling && cur_pat(sel)->mode == 0, "LAYOUT: STEPS");
    cat_open(PC_SOUND); pump(1); CHECK(side_page() == SD_SOUND, "SOUND has slots");
    sp_cur = 0; printf("before sp_cur %d foc %d\n", sp_cur, side_foc); tap(K_LEFT); printf("after sp_cur %d foc %d srow %d\n", sp_cur, side_foc, srow); CHECK(side_foc == 2, "LEFT from the first knob row: MIDI LEARN");
    shot("side_4_sound.bmp"); side_foc = 0;
    tap(K_OPT); CHECK(!ctxing, "SOUND: OPTION opens no menu");
    cat_open(PC_FX); pump(1); CHECK(side_page() == SD_FX, "FX has slots");
    { int on = song.mix.pad[sel].ins[fx2_slot].on, ty = song.mix.pad[sel].ins[fx2_slot].type; fx2_cur = 0;
      tap(K_LEFT); tap(K_LEFT); tap(K_LEFT); tap(K_LEFT); tap(K_LEFT); tap(K_LEFT); tap(K_LEFT); tap(K_LEFT);
      CHECK(side_foc == 1, "FX: LEFT at the edge: INSERT");
      tap(K_EDIT); if (ty) CHECK(song.mix.pad[sel].ins[fx2_slot].on != on, "INSERT: EDIT toggles");
      shot("side_5_fx.bmp"); side_foc = 0; }
    cat_open(PC_MOD); pump(1); CHECK(side_page() == SD_LFO, "MOD > LFO has slots");
    shot("side_6_lfo.bmp");
    side_foc = 1; tap(K_EDIT); pump(1); CHECK(side_page() == SD_AUTO, "AUTOMATION slot opens A1-A4");
    shot("side_7_auto.bmp");
    cat_open(PC_PATTERN); pump(1);
    { Pattern *p = cur_pat(sel); for (int i = 0; i < NSTEP; i++) p->trig[i] = 0; for (int i = 0; i < NNOTE; i++) p->roll[i].len = 0; }   // an empty clip: CHORDS asks nothing
    lay_go(2); pump(2); printf("mode %d cfm %d chording %d rolling %d\n", cur_pat(sel)->mode, cfm, chording, rolling);
    if (chording) { CHECK(side_page() == SD_CHORDS, "CHORDS has slots"); shot("side_8_chords.bmp");
        ch_col = 6; tap(K_RIGHT); CHECK(side_foc == 5, "CHORDS: RIGHT at the edge: RECORD"); }
    else printf("FAIL no chords\n"), fails++;
    set_tab(T_GRID); ui_cat = -1; side_foc = 0; qp_open(); qp_cur = 4; for (int i = 0; i < 12; i++) { fake_ms += 40; draw(); } shot("side_9_picker.bmp"); qpicking = 0;
    if (fails) printf("FAILED %d\n", fails); else printf("ALL OK (0 fails)\n");
    return fails != 0;
}
