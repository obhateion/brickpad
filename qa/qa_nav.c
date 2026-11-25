// 2026-11-24 (user: "no more dead ends"): a walk over every screen with the real keys. On each screen: the cursor goes up to
// the bar and comes back down, a tab outline only where a tab row is drawn, side slots only beside a page that has them,
// Y leads back towards HOME, and the screen can be reached again the same way. A picture of each screen (nav_*.bmp).
// Build and run as qa_side (qnav.sh), in /tmp/qx.
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
static SDL_Surface *sf; static float buf[2 * 128]; static int fails, nshot; static char where[64];
static void pump(int n) { while (n--) { callback(0, (Uint8 *)buf, sizeof buf); engine_collect_garbage(); scene_poll(); } }
static void inv(int k);
static void ev(int k, int down) { fake_ms += 40; key_event(k, down); pump(1); if (down) inv(k); }
static void tap(int k) { ev(k, 1); ev(k, 0); }
static void combo(int m, int k) { ev(m, 1); tap(k); ev(m, 0); }
static void frames(int n) { while (n--) { fake_ms += 40; draw(); } }
static void shot(const char *nm) { char f[96]; frames(12); snprintf(f, sizeof f, "nav_%02d_%s.bmp", ++nshot, nm); SDL_SaveBMP(sf, f); }
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL [%s] ", where); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void inv(int k) {                 // after every key: what is focused must be drawn
    CHECK(!tab_focus || page_tabs(), "key %d: the tab outline on a page without tabs (tab %d cat %d)", k, tab, ui_cat);
    CHECK(!side_foc || side_page(), "key %d: a side slot focused on a page without slots", k);
    CHECK(!(qb_focus && strip_focus) && !(qb_focus && tab_focus) && !(strip_focus && tab_focus), "key %d: two focuses at once", k);
}
static int focus_any(void) { return qb_focus || strip_focus || tab_focus || side_foc || qd_open || ctxing; }
static void up_down(void) {              // UP to the bar, then DOWN back onto the page (the roll: UP is pitch, X reaches the bar)
    int i; if (rolling && !chording) tap(K_X); else for (i = 0; i < 12 && !qb_focus; i++) tap(K_UP);
    CHECK(qb_focus, "UP never reaches the quick bar");
    for (i = 0; i < 12 && focus_any(); i++) tap(K_DOWN);
    CHECK(!focus_any(), "DOWN never comes back onto the page (qb %d strip %d tab %d side %d drawer %d)", qb_focus, strip_focus, tab_focus, side_foc, qd_open);
}
static void handle_down(void) {          // the bar's last place, the drawer's handle: DOWN leaves the bar (user, 2026-11-24)
    tap(K_X); CHECK(qb_focus, "X: the bar");
    for (int i = 0; i < 16; i++) tap(K_RIGHT);
    CHECK(qb_cur == QB_HANDLE, "RIGHT to the handle");
    tap(K_DOWN); CHECK(!qb_focus && !qd_open, "DOWN on the handle leaves the bar");
    tap(K_X); for (int i = 0; i < 16; i++) tap(K_RIGHT); tap(K_EDIT); CHECK(qd_open, "EDIT on the handle opens the drawer");
    tap(K_Y); CHECK(!qd_open && !qb_focus, "Y closes the drawer");
}
static void sides(void) {                // a page with slots: both edges lead onto them and back
    if (!side_page()) return;
    for (int d = 0; d < 2; d++) { int k = d ? K_RIGHT : K_LEFT, any = 0;
        for (int i = 0; i < 8; i++) if (side_has(side_page(), d * 4 + i % 4)) any = 1;
        if (!any) continue;
        int i; for (i = 0; i < 400 && !side_foc; i++) tap(k);
        CHECK(side_foc, "%s edge never reaches the slots", d ? "right" : "left");
        tap(d ? K_LEFT : K_RIGHT); CHECK(!side_foc, "the arrow back leaves the slots"); }
}
static int home(void) { return tab == T_GRID && ui_cat < 0 && !scening && !focus_any(); }
static void y_home(void) {               // Y, again and again: HOME within a few presses
    int i; for (i = 0; i < 6 && !home(); i++) { tap(K_Y); if (tab == T_MIX || (tab == T_GRID && scening)) break; }
    CHECK(home() || tab == T_MIX || scening, "Y does not lead back to HOME (tab %d cat %d scening %d)", tab, ui_cat, scening);
    if (!home()) { set_tab(T_GRID); ui_cat = -1; scening = 0; qb_focus = strip_focus = tab_focus = side_foc = 0; }
}
static void screen(const char *nm) { snprintf(where, sizeof where, "%s", nm); shot(nm); up_down(); sides(); }
int main(void) {
    srand(1); sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1);
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    set_tab(T_GRID); ui_cat = -1; sel = 0; pump(1);
    screen("home"); handle_down();
    // HOME's screens: L1/R1 round PADS, SCENES, MIXER
    tap(K_R1); screen("scenes"); CHECK(scening, "R1: SCENES"); handle_down();
    tap(K_R1); CHECK(v2_mixer(), "R1: MIXER"); screen("mixer");
    // the mixer's pages: UP onto its tab row, <> round all five, DOWN back
    { const char *nm[MXP_N] = {"mixer_pads", "mixer_ret_a", "mixer_ret_b", "mixer_master", "mixer_freeze"};
      tap(K_UP); CHECK(tab_focus, "UP from the faders: the tab row");
      for (int i = 1; i <= MXP_N; i++) { tap(K_RIGHT); CHECK(mix_at() == i % MXP_N, "RIGHT on the row: page %d (at %d)", i % MXP_N, mix_at());
          CHECK(tab_focus, "the row keeps the cursor"); if (i < MXP_N) { snprintf(where, sizeof where, "%s", nm[i]); shot(nm[i]); } }
      for (int i = 1; i < MXP_N; i++) { snprintf(where, sizeof where, "%s keys", nm[i]); mix_goto(i); tab_focus = 0; pump(1); up_down(); tap(K_Y);
          CHECK(v2_mixer() || v2_unit(), "Y on %s stays in the mixer", nm[i]); }
      mix_goto(0); tap(K_RIGHT); for (int i = 0; i < 13; i++) tap(K_RIGHT); CHECK(mx_pg == 1, "right past pad 12 opens the drawer");
      screen("mixer_drawer"); tap(K_EDIT); CHECK(v2_unit(), "EDIT on a unit in the drawer opens its page"); tap(K_Y); }
    tap(K_R1); CHECK(home(), "R1 from MIXER: PADS");
    // PROJECT: MENU+UP, and back
    combo(K_MENU, K_UP); CHECK(tab == T_PRJ, "MENU+UP: PROJECT"); screen("project");
    combo(K_MENU, K_DOWN); CHECK(tab == T_GRID, "MENU+DOWN: back from PROJECT");
    // every engine's pad: EDIT opens it, F1/F2 (and L1/R1) through its sections; on each one the bar and back, its slots, Y HOME
    int done[4] = {0};
    for (int s = 0; s < NSLOT; s++) {
        int ty = song.t[s].type & 3; if (!has_sound(s) || done[ty]) continue; done[ty] = 1;
        set_tab(T_GRID); ui_cat = -1; sel = s; pump(1);
        cat_open(PC_PATTERN); pump(1);
        for (int c = 0; c < PC_N; c++) { char nm[48]; static const char *const en[4] = {"smp", "grand", "acid", "cloud"};   // F2 from MIX goes round to PATTERN static const char *const en[4] = {"smp", "grand", "acid", "cloud"};
            snprintf(nm, sizeof nm, "%s_%s", en[ty], c < PC_N ? cat_name[c] : "LOOK");
            screen(nm);
            if (c == PC_SOUND) {        // every SOUND page of this engine
                int np; snd_pages(song.t[s].type, &np);
                for (int pg = 1; pg < np; pg++) { tap(K_UP); for (int u = 0; u < 4 && !tab_focus; u++) tap(K_UP); CHECK(tab_focus, "UP: the SOUND pages row");
                    tap(K_RIGHT); tab_focus = 0; char n2[48]; snprintf(n2, sizeof n2, "%s_p%d", nm, pg + 1); screen(n2); } }
            if (c == PC_PATTERN) {      // the layouts through the LAYOUT slot
                for (int l = 0; l < 2; l++) { set_step(0); for (int i = 0; i < 400 && !side_foc; i++) tap(K_LEFT); for (int i = 0; i < 3; i++) tap(K_UP);   // the slot at the cursor's height, then up to LAYOUT
                    if (side_foc != 1) { CHECK(0, "LAYOUT slot not reached"); break; }
                    combo(K_EDIT, K_DOWN); pump(2); if (cfm) { tap(K_Y); pump(1); break; }
                    tap(K_RIGHT); char n2[48]; snprintf(n2, sizeof n2, "%s_layout%d", nm, cur_pat(sel)->mode); screen(n2); }
                while (cur_pat(sel)->mode) { side_foc = 1; combo(K_EDIT, K_DOWN); pump(2); if (cfm) { tap(K_Y); break; } side_foc = 0; }
                side_foc = 0; }
            tap(K_F2); pump(1);
        }
        y_home();
    }
    // stuck keys (agent audit 2026-11-24): Y on a slot, Y ending a latch must not leave Y or X held
    snprintf(where, sizeof where, "stuck keys");
    { int s = 0; while (s < NSLOT && !(has_sound(s) && song.t[s].type == 0)) s++; if (s < NSLOT) { sel = s; set_tab(T_GRID); ui_cat = -1; cat_open(PC_PATTERN); pump(1);
        if (cur_pat(sel)->mode) { cur_pat(sel)->mode = 0; if (rolling) roll_leave(); if (chording) chord_leave(); }
        set_step(0); tap(K_LEFT); CHECK(side_foc, "LEFT: a slot"); tap(K_Y); CHECK(!side_foc && !held[K_Y], "Y on a slot: off it, Y not held");
        tap(K_LEFT); CHECK(side_foc, "the slots again after Y");
        side_foc = 0; v_latch = 1; tap(K_Y); CHECK(!v_latch && !held[K_X], "Y ends the LENGTH latch, X not held");
        x_latch = 1; held[K_X] = used[K_X] = 1; tap(K_Y); CHECK(!x_latch && !held[K_X], "Y ends the X latch, X let go");
        tap(K_LEFT); CHECK(side_foc, "the slots still reachable"); side_foc = 0; } }
    // OPTION's menu: opens, Y closes, on a pad's page and on HOME
    snprintf(where, sizeof where, "HOME combos");
    set_tab(T_GRID); ui_cat = -1; tap(K_OPT); CHECK(!ctxing, "OPTION on HOME: no menu");
    { int s = 0; while (s < NSLOT && !has_sound(s)) s++; sel = s; int m = song.t[s].mute, so = song.mix.pad[s].solo;
      ev(K_OPT, 1); tap(K_DOWN); ev(K_OPT, 0); CHECK(song.t[s].mute != m && !ctxing, "OPT+DOWN: mute");
      ev(K_OPT, 1); tap(K_DOWN); ev(K_OPT, 0); CHECK(song.t[s].mute == m, "and back");
      ev(K_OPT, 1); tap(K_UP); ev(K_OPT, 0); CHECK(song.mix.pad[s].solo != so, "OPT+UP: solo"); ev(K_OPT, 1); tap(K_UP); ev(K_OPT, 0);
      int on0 = song.t[s].on, q0 = song.t[s].queued; ev(K_EDIT, 1); tap(K_PLAY); ev(K_EDIT, 0); pump(2);
      CHECK(tab == T_GRID && ui_cat < 0 && (song.t[s].on != on0 || song.t[s].queued != q0), "EDIT+PLAY: the clip, the pad stays closed");
      int e = 0; while (e < NSLOT && has_sound(e)) e++;
      if (e < NSLOT) { sel = e; ev(K_OPT, 1); tap(K_EDIT); ev(K_OPT, 0); CHECK(exporting && ex_resample, "OPT+EDIT on an empty pad: RESAMPLE"); tap(K_Y); exporting = 0; ex_resample = 0; } }
    if (fails) printf("FAILED %d\n", fails); else printf("ALL OK (0 fails)\n");
    return fails != 0;
}
