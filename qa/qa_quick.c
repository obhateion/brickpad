// 2026-11-21: the quick bar (12 icons + handle) and its drawer: open/close slide, tiles, strips, TAP, REDO, FILTER, pictures.
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
static SDL_Surface *sf; static float buf[2 * 128];
static void pump(int n) { while (n--) { callback(0, (Uint8 *)buf, sizeof buf); engine_collect_garbage(); scene_poll(); } }
static void ev(int k, int down) { fake_ms += 40; key_event(k, down); }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL L%d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void tap(int k) { ev(k, 1); ev(k, 0); pump(1); }
static void frames(int n) { while (n--) { fake_ms += 40; draw(); } }
static void shot(const char *f) { draw(); SDL_SaveBMP(sf, f); }
int main(void) {
    srand(1); sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1);
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    sel = 4; set_tab(T_SEQ); ui_cat = hub_cur = PC_PATTERN; frames(3);
    qb_focus = 1; qb_cur = QB_HANDLE; frames(2); shot("q0.bmp");
    tap(K_EDIT); CHECK(qd_open && !qb_focus, "EDIT on the handle opens the drawer");   // 2026-11-24: DOWN leaves the bar
    CHECK(qd_cur == QA_RESAMPLE, "the cursor starts on the MORE card's first tile (%d)", qd_cur);   // 2026-11-24: merged bar, one MORE card
    fake_ms += 40; draw(); fake_ms += 40; draw(); SDL_SaveBMP(sf, "q1.bmp");   // half way down
    CHECK(qd_anim.v > 0 && qd_anim.v < 1, "it slides (%.2f)", qd_anim.v);
    frames(10); CHECK(qd_anim.v == 1, "and arrives"); shot("q2.bmp");
    tap(K_RIGHT); CHECK(qd_cur == QA_FOLLOW, "RIGHT: FOLLOW (%d)", qd_cur);
    { int f0 = follow_ph; tap(K_EDIT); CHECK(follow_ph != f0, "EDIT toggles FOLLOW"); tap(K_EDIT); CHECK(follow_ph == f0, "and back"); }
    tap(K_RIGHT); CHECK(qd_cur == QA_SOLOCLR, "RIGHT: CLEAR SOLO (%d)", qd_cur);
    for (int i = 0; i < 6 && qd_cur != QA_VOL; i++) tap(K_RIGHT); CHECK(qd_cur == QA_VOL, "RIGHT reaches the VOLUME strip (%d)", qd_cur);
    int m0 = song.master; ev(K_EDIT, 1); tap(K_DOWN); tap(K_DOWN); ev(K_EDIT, 0); CHECK(song.master == m0 - 4, "EDIT+v twice: master %d -> %d", m0, song.master);
    tap(K_RIGHT); CHECK(qd_cur == QA_FILT, "FILTER (%d)", qd_cur);
    ev(K_EDIT, 1); for (int i = 0; i < 5; i++) tap(K_DOWN);
    CHECK(eng_sweep < -0.45f, "EDIT+v sweeps low-pass (%.2f)", eng_sweep);
    engine_transport(1); pump(200); float pk = 0; for (int i = 0; i < 256; i++) { if (!isfinite(buf[i])) pk = 99; if (fabsf(buf[i]) > pk) pk = fabsf(buf[i]); }
    CHECK(pk < 1.5f, "filtered output finite (%.2f)", pk);
    shot("q3.bmp");
    ev(K_EDIT, 0); CHECK(eng_sweep == 0, "EDIT let go: the filter springs back"); pump(50);
    tap(K_RIGHT); CHECK(qd_cur == QA_TEMPO, "TEMPO (%d)", qd_cur);
    int t0 = song.tempo; ev(K_EDIT, 1); tap(K_UP); ev(K_EDIT, 0); CHECK(song.tempo == t0 + 1, "tempo %d -> %d", t0, song.tempo);
    undo(); CHECK(song.tempo == t0, "UNDO the tempo (%d)", song.tempo); redo(); CHECK(song.tempo == t0 + 1, "REDO brings it (%d)", song.tempo);
    redo(); CHECK(song.tempo == t0 + 1, "nothing more to redo");
    for (int i = 0; i < 4; i++) { fake_ms += 500; qa_press(QA_TAP); } CHECK(song.tempo == 120, "TAP every 500 ms = 120 (%d)", song.tempo);
    int mu[NSLOT]; for (int s = 0; s < NSLOT; s++) mu[s] = song.t[s].mute;
    qa_press(QA_MUTEALL); int all = 1; for (int s = 0; s < NSLOT; s++) all &= song.t[s].mute; CHECK(all, "MUTE ALL");
    qa_press(QA_MUTEALL); int same = 1; for (int s = 0; s < NSLOT; s++) same &= song.t[s].mute == mu[s]; CHECK(same, "and the mutes come back");
    qd_cur = QA_FOLLOW; tap(K_UP); CHECK(!qd_open && qb_focus && qb_cur == QB_HANDLE, "UP past the top: closed, on the handle");
    frames(2); SDL_SaveBMP(sf, "q4.bmp"); frames(10); CHECK(qd_anim.v == 0, "slid away");
    tap(K_EDIT); CHECK(qd_open, "EDIT on the handle opens"); tap(K_OPT); CHECK(!qd_open, "OPTION closes");
    // the merged buttons on the bar (default: TEMPO METRO REC UNDO SCENE LAUNCH MUTEALL BYPASS PLAY SAVE)
    CHECK(qb_n == 10 && qb_top[0] == QA_TAP && qb_top[8] == QA_PADS, "the default bar (%d)", qb_n);
    qb_focus = 1; qb_cur = 1; { int c0 = song.count_in; ev(K_EDIT, 1); tap(K_UP); ev(K_EDIT, 0); CHECK(song.count_in != c0, "METRO: EDIT+^ the count-in (%d)", song.count_in);
      int m1 = metro.snd; CHECK(m1 == metro.snd, "EDIT+^ did not click the metronome"); tap(K_EDIT); CHECK(metro.snd != m1, "METRO: EDIT the click"); tap(K_EDIT); }
    { int s0 = scn_lock, f0 = eng_nofollow; qb_focus = 1; qb_cur = 4; tap(K_EDIT); CHECK(scn_lock && !eng_nofollow, "SCENE: LOCK"); tap(K_EDIT); CHECK(scn_lock && eng_nofollow, "SCENE: HOLD ALL");
      tap(K_EDIT); CHECK(!scn_lock && !eng_nofollow, "SCENE: FREE"); (void)s0; (void)f0; }
    qb_focus = 1; qb_cur = 8; ev(K_EDIT, 1); tap(K_RIGHT); ev(K_EDIT, 0); CHECK(perform == 2, "PLAY MODE: EDIT+> KEYS (%d)", perform);
    ev(K_SHIFT, 1); tap(K_OPT); ev(K_SHIFT, 0); CHECK(!perform, "SHIFT+OPTION leaves");
    qb_focus = 1; qb_cur = 8; tap(K_EDIT); CHECK(perform == 2, "PLAY MODE: EDIT enters the one used last (%d)", perform); ev(K_SHIFT, 1); tap(K_OPT); ev(K_SHIFT, 0);
    qd_open = 0;
    CHECK(!ctxing, "OPTION closing the drawer opens no menu"); qb_focus = 1; qb_cur = 2; tap(K_EDIT); fake_ms += 1000; combo_tick(fake_ms); CHECK(rec || rec_wait, "REC on the bar arms"); tap(K_EDIT); fake_ms += 1000; combo_tick(fake_ms); pump(2);
    // 2026-11-21: Y back everywhere, X the bar and its drawer
    qb_focus = 0; qd_open = 0; set_tab(T_SEQ); ui_cat = hub_cur = PC_PATTERN; frames(2);
    tap(K_X); CHECK(qb_focus && !qd_open, "X: the quick bar");
    tap(K_X); CHECK(qd_open && !qb_focus, "X again: the drawer");
    tap(K_Y); CHECK(!qd_open && !qb_focus, "Y: the drawer closes");
    tap(K_X); tap(K_X); tap(K_X); CHECK(!qd_open, "X X X: open and closed");
    tap(K_X); tap(K_Y); CHECK(!qb_focus && in_pad(), "Y leaves the bar, the page stays");
    tap(K_Y); CHECK(tab == T_GRID && ui_cat < 0, "Y in a pad's section: HOME (tab %d cat %d)", tab, ui_cat);
    set_tab(T_SEQ); ui_cat = hub_cur = PC_PATTERN; tap(K_OPT); CHECK(in_pad(), "OPTION: no menu, the page stays");
    perform = 1; int on0 = song.t[0].on; tap(K_X); CHECK(!qb_focus, "PADS: X stays a pad"); perform = 0; (void)on0; frames(2);
    // PROJECT > SETTINGS > BAR BUTTONS
    unlink("quickbar2.cfg"); qb_n = -1; qb_ready(); CHECK(qb_n == 10 && qb_top[0] == QA_TAP, "default ten");
    qb_focus = 0; set_tab(T_PRJ); prj_goto(J_QBTN); frames(2); tap(K_EDIT); if (!qpicking) printf("dbg tab %d jsec %d jcol %d jcur %d\n", tab, jsec, jcol, jcur); CHECK(qpicking, "EDIT on BAR BUTTONS opens the picker");
    frames(10); shot("p0.bmp");
    { int ids[QA_N], nv = qp_vis(ids); CHECK(nv == 13, "13 buttons to pick from (%d)", nv);   // visible: MET TEMPO LQ REC UNDO SAVE PLAY SCENE RESAMPLE FOLLOW BYPASS MUTEALL NOSOLO
      int pm = -1, pf = -1, pr = -1, ps = -1; for (int j = 0; j < nv; j++) { if (ids[j] == QA_MET) pm = j; if (ids[j] == QA_FOLLOW) pf = j; if (ids[j] == QA_RESAMPLE) pr = j; if (ids[j] == QA_SOLOCLR) ps = j; }
      qp_cur = pm; tap(K_EDIT); CHECK(qb_n == 9 && !qb_on(QA_MET), "EDIT takes METRONOME off the bar");
      qp_cur = pf; tap(K_EDIT); CHECK(qb_n == 10 && qb_on(QA_FOLLOW) == 10, "FOLLOW goes on at the end");
      qp_cur = pr; tap(K_EDIT); qp_cur = ps; tap(K_EDIT); CHECK(qb_n == 12, "twelve");
      qp_cur = pm; tap(K_EDIT); CHECK(qb_n == 12 && !qb_on(QA_MET), "a full bar takes no more");
      qp_cur = pf; ev(K_EDIT, 1); tap(K_LEFT); tap(K_LEFT); ev(K_EDIT, 0); pump(1); CHECK(qb_on(QA_FOLLOW) == 8 && qb_n == 12, "EDIT+<< moves it two places (%d)", qb_on(QA_FOLLOW)); }
    shot("p1.bmp");
    tap(K_Y); CHECK(!qpicking && !ctxing, "Y closes the picker");
    qb_n = -1; qb_ready(); CHECK(qb_n == 12 && qb_on(QA_FOLLOW) == 8 && !qb_on(QA_MET), "the choice comes back from quickbar2.cfg");
    set_tab(T_SEQ); frames(3); qd_toggle(1); frames(10);
    CHECK(qd_item(QA_MET) && !qd_item(QA_FOLLOW) && !qd_item(QA_COPY), "the drawer has METRONOME now, not FOLLOW, never COPY"); shot("q8.bmp"); qd_toggle(0); frames(10);
    qb_defaults(); qb_save();
    // the mixer's drawer and the OPTION menu slide too
    qb_focus = 0; set_tab(T_MIX); ui_cat = -1; mx_pg = 0; frames(8); mx_pg = 1; mcol = NSLOT; fake_ms += 40; draw(); fake_ms += 40; draw(); SDL_SaveBMP(sf, "q5.bmp");
    CHECK(mx_anim.v > 0 && mx_anim.v < 1, "mixer drawer slides (%.2f)", mx_anim.v); frames(10); shot("q6.bmp");
    printf("%s (%d fails)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails != 0;
}
