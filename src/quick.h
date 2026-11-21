// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// The quick bar and its drawer (v3; 2026-11-21, user: "variant C", "a drawer between a phone's control centre and our mixer").
// The bar holds 12 buttons as icons in the header row (or at the bottom, PROJECT > SETTINGS > QUICK BAR); the 13th place is
// the handle "+N" that pulls the drawer down: cards TEMPO, RECORD, EDIT, SOUND, SCENE, DEVICE with tiles, and at the right
// three strips as on the mixer (MASTER VOLUME with its meter, FILTER while EDIT is held, TEMPO). Arrows move to the nearest
// tile that way, EDIT presses, EDIT+^v moves a strip, OPTION (or ^ past the top) closes. Every drawer slides (anim).
// ui.h includes this.
#pragma once

// ---- the slide of every drawer: 0 closed .. 1 open, eased; any value between keeps the frame rate up ----
typedef struct { float v; Uint32 t; } Anim;
static Uint32 anim_busy;
static Anim ctx_anim, mx_anim; static float ctx_e;   // the OPTION menu and the mixer's drawer slide too
static float anim(Anim *a, int on, int ms) {
    Uint32 now = SDL_GetTicks(), dt = a->t ? now - a->t : 0; a->t = now;
    if (dt > 34) dt = 34;                 // after a quiet spell the first frame does not jump to the end
    float d = dt / (float)ms; a->v = on ? fminf(1, a->v + d) : fmaxf(0, a->v - d);
    if (a->v > 0 && a->v < 1) anim_busy = now | 1;
    float x = 1 - a->v; return 1 - x * x * x;
}

static const char *const qi_px[][16] = {   // icons the pad set has no use for
 {"................","....X...........","...XX...........","..XXXXXXXXXX....",".XXXXXXXXXXXX...","..XXXXXXXXXXXX..","...XX.......XX..","....X........XX.",".............XX.",".............XX.","............XX..","...........XX...","....XXXXXXXX....","....XXXXXXX.....","................","................"},   // 0 UNDO
 {"................","......XX........","......XX........","......XX........","......XX........","......XXXXXXXX..","......XXXXXXXXX.","..XX..XXXXXXXXX.","..XXX.XXXXXXXXX.","...XXXXXXXXXXXX.","....XXXXXXXXXXX.",".....XXXXXXXXX..","......XXXXXXX...","................",".XX..XX....XX..X","................"},   // 1 TAP
 {"................","..........XX....",".........XXX....","........XXXX....",".......XX.XX....","......XX..XX....",".....XXXXXXXXX..",".....XXXXXXXXX..","..........XX....","..........XX....","................","................","XX..XX..XX..XX..","XX..XX..XX..XX..","................","................"},   // 2 COUNT-IN
 {"................","XXXXXXXXXXXXXXXX","X.......X......X","X.......X......X","X.......X......X","XXXXXXXXXXXXXXXX","................","......XX........","......XX........","....XXXXXX......",".....XXXX.......","......XX........","................","XX..XX..XX..XX..","XX..XX..XX..XX..","................"},   // 3 LAUNCH QUANT
 {"................","...XXXXXXXXX....","..XXXXXXXXXXX...",".XX.........XX..",".XX..XXXX...XX..",".XX..XXXX...XX..",".XX..XXXX..XXXXX",".XX..XXXX...XXX.",".XX..........X..",".XX.............","..XXXXXXXXXXXX..","...XXXXXXXXXX...","................","................","................","................"},   // 4 SCENE LOCK
 {"................","..XXX....XXX....","..XXX....XXX....","..XXX....XXX....","..XXX....XXX....","..XXX....XXX....","..XXX....XXX....","..XXX....XXX....","................","XXXXXXXXXXX.....","XXXXXXXXXXXX....","...........XX...","XXXXXXXXXXXX....","XXXXXXXXXXX.....","................","................"},   // 5 FOLLOW OFF
 {"................","....XXXXXX......","..XXXXXXXXXX....","..XXXXXXXXXX....",".XXXXXXXXXXXX...",".XXXXXXXXXXXX...","..XXXXXXXXXX....","..XXXXXXXXXX....","....XXXXXX..XX..","............XX..","..........XXXXXX","..........XXXXXX","............XX..","............XX..","................","................"},   // 6 OVERDUB
 {"................","..XXXXXX........",".XXXXXXXX.......",".XXXXXXXX.......",".XXXXXXXX.......","..XXXXXX........","................","X...X...X...X...","X...X...X...X...","X...X...X...X...","XXXXXXXXXXXXXXXX","X...X...X...X...","X...X...X...X...","X...X...X...X...","................","................"},   // 7 REC QUANT
 {"................","XXX.............","XXX....X........","XXX...XXX.......","XXX..XXXXX......","XXX....X........","XXX....X........","XXX....XXXXXXX..","XXX..........X..","XXX....XXXXXXXXX","XXX....X.......X","XXX....X.XXXXX.X","XXX....X.XXXXX.X","XXX....X.......X","XXX....XXXXXXXXX","................"},   // 8 RESAMPLE
 {"................","...XXXXXXXXXX...","..XX........XX..",".XX..XXXXXX..XX.",".X..XX....XX..X.",".X..X......X..X.",".X..X......X..X.",".X..X......X..X.",".X..XX....XX..X.",".XX..XXXXXX..XX.","..XX........XX..","...XXXXXXXXXX...","................","XXXXXXXXXXXXXXXX","................","................"},   // 9 FX BYPASS
 {"................","..XXXXXXXX......",".XX......XX.....",".XX.............","..XXXXXXX.......","........XX......",".XX......XX.....","..XXXXXXXX......","................","........XX....XX",".........XX..XX.","..........XXXX..","..........XXXX..",".........XX..XX.","........XX....XX","................"},   // 10 SOLO CLEAR
 {"................","XXXXXXXXX.......","X.......X.......","X.......X.......","X...XXXXXXXXX...","X...X.......X...","X...X.......X...","X...X.......X...","XXXXX.......X...","....X.......X...","....X.......X...","....X.......X...","....XXXXXXXXX...","................","................","................"},   // 11 COPY
 {"................",".....XXXXX......","..XXXX...XXXX...","..X.XXXXXXX.X...","..X.........X...","..X..XXXXXXXXXX.","..X..X........X.","..X..X.XXXXXX.X.","..X..X........X.","..X..X.XXXXXX.X.","..X..X........X.","..XXXX.XXXX...X.",".....X........X.",".....XXXXXXXXXX.","................","................"},   // 12 PASTE
 {"................","XXXXXXXXXXXXXXXX","X..............X","X.....X........X","X....XX........X","X...XXXXXXXXX..X","X..XXXXXXXXXX..X","X...XXXXXXXXX..X","X....XX........X","X.....X........X","X..............X","XXXXXXXXXXXXXXXX","................","XXX..XXX..XXX...","XXX..XXX..XXX...","................"},   // 13 SCENE <
};
enum { QI_UNDO = 1000, QI_TAP, QI_COUNT, QI_LQ, QI_SLOCK, QI_FOLOFF, QI_OD, QI_RECQ, QI_RESAMPLE, QI_BYPASS, QI_SOLOCLR, QI_COPY, QI_PASTE, QI_SCNPREV,
       QI_REDO = 2000 + 0, QI_SCNNEXT = 2000 + 13 };   // 2000+: the 1000+ icon mirrored
static int qi_on(int id, int r, int a) {
    if (id >= 2000) { id -= 1000; a = 15 - a; if (id == QI_SCNPREV && r >= 12) a = 15 - a; }   // the scene's dots stay
    return id >= 1000 ? qi_px[id - 1000][r][a] == 'X' : icon_tab[id].px[r][a] == 'X';
}
static void icon_fit(int id, int x, int y, int sz, Col c) {   // a 16x16 icon at any size (pixel edges rounded)
    SDL_Rect d[200]; int n = 0;
    for (int r = 0; r < 16; r++) for (int a = 0; a < 16;) {
        if (!qi_on(id, r, a)) { a++; continue; }
        int e = a; while (e < 16 && qi_on(id, r, e)) e++;
        int y0 = y + r * sz / 16, y1 = y + (r + 1) * sz / 16, x0 = x + a * sz / 16, x1 = x + e * sz / 16;
        if (n < 200) d[n++] = (SDL_Rect){x0, y0, x1 - x0, y1 - y0};
        a = e;
    }
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255); SDL_RenderFillRects(R, d, n);
}

// ---- the actions: buttons first (the bar's 12, then the drawer's tiles), then what only the drawer shows ----
enum { QA_MET, QA_TAP, QA_COUNT, QA_LQ, QA_REC, QA_UNDO, QA_REDO, QA_SAVE, QA_CPU, QA_MON, QA_PADS, QA_KEYS,
       QA_SLOW, QA_FAST, QA_SLOCK, QA_FOLOFF, QA_OD, QA_RECQ, QA_RESAMPLE, QA_CAP, QA_FOLLOW, QA_SNAP, QA_COPY, QA_PASTE, QA_RND,
       QA_BYPASS, QA_MUTEALL, QA_SOLOCLR, QA_SCNPREV, QA_SCNNEXT, QA_LOCK, QA_MIDI, QA_N,
       QA_VOL = QA_N, QA_FILT, QA_TEMPO, QD_N };
static const struct { const char *name, *tile; int icon; } qa_tab[QD_N] = {
    {"METRONOME", "METRO", IC_M_METRONOME}, {"TAP TEMPO", "TAP", QI_TAP}, {"COUNT-IN", "COUNT", QI_COUNT}, {"LAUNCH QUANT", "LAUNCH", QI_LQ},
    {"RECORD", "REC", IC_M_REC}, {"UNDO", "UNDO", QI_UNDO}, {"REDO", "REDO", QI_REDO}, {"SAVE", "SAVE", IC_FLOPPY},
    {"CPU LOAD", "CPU", IC_M_CHIP}, {"HEAR NOTES", "HEAR", IC_PHONES}, {"PADS", "PADS", IC_M_PAD_GRID}, {"KEYS", "KEYS", IC_M_PIANO},
    {"TEMPO DOWN (HOLD)", "NUDGE-", IC_M_BACK}, {"TEMPO UP (HOLD)", "NUDGE+", IC_M_FWD}, {"SCENE LOCK", "SC LOCK", QI_SLOCK}, {"FOLLOW ACTIONS OFF", "FOL OFF", QI_FOLOFF},
    {"OVERDUB / REPLACE", "OVERDUB", QI_OD}, {"REC QUANT", "REC Q", QI_RECQ}, {"RESAMPLE", "RESAMPL", QI_RESAMPLE}, {"CAPTURE", "CAPTURE", IC_TAPE},
    {"FOLLOW PLAYHEAD", "FOLLOW", IC_EYE}, {"SNAP", "SNAP", IC_M_STEP_SEQ}, {"COPY", "COPY", QI_COPY}, {"PASTE", "PASTE", QI_PASTE}, {"RANDOM", "RANDOM", IC_DICE},
    {"MASTER FX BYPASS", "FX BYPASS", QI_BYPASS}, {"MUTE ALL", "MUTE ALL", IC_M_MUTE}, {"CLEAR SOLO", "SOLO CLR", QI_SOLOCLR},
    {"SCENE <", "SCENE <", QI_SCNPREV}, {"SCENE >", "SCENE >", QI_SCNNEXT}, {"KID LOCK", "KID LOCK", IC_LOCK}, {"MIDI LEARN", "MIDI", IC_M_MIDI_PORT},
    {"MASTER VOLUME", "VOLUME", 0}, {"FILTER (HOLD EDIT)", "FILTER", 0}, {"TEMPO", "TEMPO", 0},
};
static const int8_t qb_top[] = {QA_MET, QA_TAP, QA_COUNT, QA_LQ, QA_REC, QA_UNDO, QA_REDO, QA_SAVE, QA_CPU, QA_MON, QA_PADS, QA_KEYS};
enum { QB_N = sizeof qb_top / sizeof *qb_top, QB_HANDLE = QB_N, QB_IC = 24, QB_W = QB_IC + 12 };
static int qb_focus, qb_cur, qd_open, qd_cur, qd_edit, follow_ph;
static int8_t mute_keep[NSLOT]; static int muted_all;
static Uint32 tap_t[4]; static int tap_n;
static int sec_on(void) { return ui_secs && in_pad() && !ui_over() && !picking; }
static int strips_here(void) { return in_pad() && !ui_over() && !picking && !perform && (sec_on() || strip_on()); }
static void capture(void); static void metro_save(void); static void undo(void); static void redo(void); static int prj_dirty(void);
static void prj_save(const char *name); static void naming_open(int then); static void rnd_generate(void); static void export_open(void);
static void sim_key(int key, int mod); static int v2_sound(void); static void ui_cfg_save(void); static void disc2(int cx, int cy, int r, Col c);
static int batt_pct(int *charging) {     // the battery, read every 10 s
    static Uint32 at; static int pct = -1, chg; Uint32 now = SDL_GetTicks();
    if (!at || now - at > 10000) { at = now | 1; FILE *f = fopen("/sys/class/power_supply/axp2202-battery/capacity", "r"); if (f) { if (fscanf(f, "%d", &pct) != 1) pct = -1; fclose(f); }
        f = fopen("/sys/class/power_supply/axp2202-battery/status", "r"); if (f) { char b[16] = ""; if (fscanf(f, "%15s", b) == 1) chg = !strcmp(b, "Charging") || !strcmp(b, "Full"); fclose(f); } }
    if (charging) *charging = chg; return pct;
}
static int qa_lit(int i) {
    switch (i) {
    case QA_MET: return metro.snd; case QA_COUNT: return song.count_in > 0; case QA_MON: return hear_on;
    case QA_PADS: return perform == 1; case QA_KEYS: return perform == 2;
    case QA_SLOW: return tempo_mul < 0.9995f; case QA_FAST: return tempo_mul > 1.0005f;
    case QA_SLOCK: return scn_lock; case QA_FOLOFF: return eng_nofollow; case QA_OD: return rec_replace; case QA_FOLLOW: return follow_ph;
    case QA_SNAP: return song.snap; case QA_BYPASS: return eng_mbypass; case QA_MUTEALL: return muted_all; case QA_LOCK: return ui_lock;
    case QA_MIDI: return midi_fd >= 0;
    }
    return 0;
}
static const char *qa_txt(int i) {       // a small value in the corner
    static const char *const lq[] = {"", "1B", "2B", "4B", "--"}, *const rq[] = {"ST", "32", "16", "8", "4"};
    switch (i) {
    case QA_LQ: return lq[song.launch % 5]; case QA_RECQ: return rq[song.rec_quant % 5]; case QA_OD: return rec_replace ? "RP" : "OD";
    }
    return "";
}
static void qa_press(int i) {
    char b[48];
    switch (i) {
    case QA_MET: metro.snd = !metro.snd; metro_save(); toast(metro.snd ? "METRONOME SOUND ON" : "METRONOME SOUND OFF"); break;
    case QA_TAP: {
        if (midi_clk_in) { toast("CLOCK IN: THE TEMPO COMES FROM MIDI"); break; }
        Uint32 now = SDL_GetTicks();
        if (tap_n && now - tap_t[(tap_n - 1) % 4] > 2000) tap_n = 0;   // a pause starts again
        tap_t[tap_n % 4] = now; tap_n++;
        if (tap_n < 2) { toast("TAP AGAIN IN TIME"); break; }
        int m = tap_n < 4 ? tap_n : 4; float iv = (now - tap_t[(tap_n - m) % 4]) / (float)(m - 1);
        if (tap_n == 2) push_undo();
        song.tempo = (int16_t)clampi((int)lrintf(60000.f / iv), 40, 300); snprintf(b, sizeof b, "TAP %d BPM", song.tempo); toast(b); break; }
    case QA_COUNT: { static int last = 4; edit_begin(); if (song.count_in) { last = song.count_in; song.count_in = 0; } else song.count_in = (int8_t)last;
        if (song.count_in) snprintf(b, sizeof b, "COUNT-IN %d BEATS", song.count_in); else snprintf(b, sizeof b, "COUNT-IN OFF"); toast(b); break; }
    case QA_LQ: { static const char *const n[] = {"", "1 BAR", "2 BARS", "4 BARS", "NONE"}; act_launch_step(1); snprintf(b, sizeof b, "LAUNCH QUANT %s", n[song.launch % 5]); toast(b); break; }
    case QA_REC: rec_toggle(engine_posf(), SDL_GetTicks()); break;
    case QA_UNDO: undo(); break; case QA_REDO: redo(); break;
    case QA_SAVE: if (prj_cur[0]) prj_save(prj_cur); else { qd_open = 0; qb_focus = 0; naming_open(0); } break;
    case QA_CPU: snprintf(b, sizeof b, "DSP %d%%  VOICES %d", (int)(engine_load() * 100), engine_voices()); toast(b); break;
    case QA_MON: hear_on = !hear_on; toast(hear_on ? "HEAR NOTES ON" : "HEAR NOTES OFF"); break;
    case QA_PADS: case QA_KEYS: qd_open = 0; qb_focus = 0; set_tab(T_GRID); ui_cat = -1; perform = i == QA_PADS ? 1 : 2; toast(i == QA_PADS ? "PADS: SHIFT+OPTION LEAVES" : "KEYS: SHIFT+OPTION LEAVES"); break;
    case QA_SLOW: case QA_FAST: if (midi_clk_in) toast("CLOCK IN: THE TEMPO COMES FROM MIDI"); else tempo_mul = i == QA_SLOW ? 0.94f : 1.06f; break;
    case QA_SLOCK: scn_lock = !scn_lock; toast(scn_lock ? "SCENE LOCK ON: THE SCENE STAYS" : "SCENE LOCK OFF"); break;
    case QA_FOLOFF: eng_nofollow = !eng_nofollow; toast(eng_nofollow ? "FOLLOW ACTIONS OFF: CLIPS LOOP" : "FOLLOW ACTIONS ON"); break;
    case QA_OD: rec_replace = !rec_replace; toast(rec_replace ? "REPLACE: A TAKE WIPES THE CLIP FIRST" : "OVERDUB: NEW NOTES OVER THE OLD"); break;
    case QA_RECQ: act_rec_quant_step(1); snprintf(b, sizeof b, "REC QUANT %s", quant_name[song.rec_quant % 5]); toast(b); break;
    case QA_RESAMPLE: qd_open = 0; qb_focus = 0; export_open(); ex_resample = 1; toast("RESAMPLE: EDIT RENDERS INTO AN EMPTY PAD"); break;
    case QA_CAP: capture(); break;
    case QA_FOLLOW: follow_ph = !follow_ph; toast(follow_ph ? "THE PAGE FOLLOWS THE PLAYHEAD" : "THE PAGE FOLLOWS THE CURSOR"); break;
    case QA_SNAP: act_snap_toggle(); toast(song.snap ? "SNAP ON" : "SNAP OFF"); break;
    case QA_COPY: copy_paste(0); break; case QA_PASTE: copy_paste(1); break;
    case QA_RND: rnd_generate(); break;
    case QA_BYPASS: eng_mbypass = !eng_mbypass; toast(eng_mbypass ? "MASTER FX BYPASSED" : "MASTER FX ON"); break;
    case QA_MUTEALL:
        if (!muted_all) { for (int s = 0; s < NSLOT; s++) { mute_keep[s] = song.t[s].mute; song.t[s].mute = 1; } muted_all = 1; toast("ALL MUTED"); }
        else { for (int s = 0; s < NSLOT; s++) song.t[s].mute = mute_keep[s]; muted_all = 0; toast("MUTES BACK"); }
        break;
    case QA_SOLOCLR: for (int s = 0; s < NSLOT; s++) song.mix.pad[s].solo = 0; toast("SOLO CLEARED"); break;
    case QA_SCNPREV: case QA_SCNNEXT: if (scn_lock) toast("SCENE LOCK IS ON"); else scene_step(i == QA_SCNNEXT ? 1 : -1); break;
    case QA_LOCK: ui_lock = !ui_lock; ui_cfg_save(); toast(ui_lock ? "KID LOCK ON" : "KID LOCK OFF"); break;
    case QA_MIDI:
        if (v2_sound()) { qd_open = 0; qb_focus = 0; sim_key(K_R2, -1); }
        else { snprintf(b, sizeof b, midi_fd >= 0 ? "MIDI: %.20s  LEARN ON A SOUND PAGE" : "NO MIDI DEVICE%.0s", midi_open_name); toast(b); }
        break;
    }
}
static void qa_release(int i) { if ((i == QA_SLOW || i == QA_FAST) && !midi_clk_in) tempo_mul = 1.f; }

// ---- the bar ----
static int qbar_w(void) { return (QB_N + 1) * (QB_W + 5) - 5; }
static void qb_button(int x, int y, int w, int h, int i, int cur, int sz) {
    int lit = qa_lit(i);
    if (cur) rframe(x - 3, y - 3, w + 6, h + 6, 8, 2, C_WHITE, C_BG);
    rrect(x, y, w, h, 6, lit ? C_ORANGE : C_SELBG);
    Col c = lit ? C_BG : cur ? C_WHITE : C_DIM; if (i == QA_REC && (rec || rec_wait)) c = C_RED;
    icon_fit(qa_tab[i].icon, x + (w - sz) / 2, y + (h - sz) / 2, sz, c);
    if (i == QA_SAVE && prj_dirty()) disc2(x + w - 6, y + 6, 3, C_ORANGE);
    if (i == QA_CPU) { int ld = clampi((int)(engine_load() * 100), 0, 100); rect(x + 4, y + h - 5, w - 8, 3, C_LINE, 1); rect(x + 4, y + h - 5, (w - 8) * ld / 100, 3, ld >= 85 ? C_RED : th_cur[TC_SMP], 1); }
}
static void qbar_draw(int x, int y, int h) {
    int sz = h - 8 < QB_IC ? h - 8 : QB_IC;
    for (int i = 0; i < QB_N; i++) { qb_button(x, y, QB_W, h, qb_top[i], qb_focus && i == qb_cur, sz); x += QB_W + 5; }
    int cur = qb_focus && qb_cur == QB_HANDLE;
    if (cur) rframe(x - 3, y - 3, QB_W + 6, h + 6, 8, 2, C_WHITE, C_BG);
    rrect(x, y, QB_W, h, 6, qd_open ? C_ORANGE : C_SELBG);
    Col c = qd_open ? C_BG : cur ? C_WHITE : C_DIM; int cx = x + QB_W / 2, cy = y + h / 2 - 3;
    for (int k = 0; k < 6; k++) { int dy = qd_open ? 5 - k : k; rect(cx - 11 + k * 2, cy + dy, 3, 3, c, 1); rect(cx + 8 - k * 2, cy + dy, 3, 3, c, 1); }
}
static int qd_item(int i);
static void qd_toggle(int on) { qd_open = on; qd_edit = 0; eng_sweep = 0; if (on) { qb_focus = 0; if (!qd_item(qd_cur)) qd_cur = QA_SLOW; } }
static int qbar_key(int k, int down) {    // the cursor on the bar: 1 = the key was the bar's
    if (k == K_EDIT) {
        if (qb_cur == QB_HANDLE) { if (down) { qd_toggle(!qd_open); if (!qd_open) qb_focus = 1; } return 1; }
        if (down) qa_press(qb_top[qb_cur]); else qa_release(qb_top[qb_cur]); return 1; }
    if (k == K_PLAY || k == K_SHIFT || k == K_MENU || k == K_F1 || k == K_F2) return 0;
    if (!down) return 1;
    if (k == K_LEFT || k == K_RIGHT) { if (qb_cur < QB_N) qa_release(qb_top[qb_cur]); qb_cur = clampi(qb_cur + (k == K_RIGHT ? 1 : -1), 0, QB_HANDLE); }
    else if (!ui_qbar && k == K_DOWN && qb_cur == QB_HANDLE) qd_toggle(1);   // down from the handle pulls the drawer
    else if (k == K_OPT || k == (ui_qbar ? K_UP : K_DOWN)) {   // back: down to the strips at the top, OPTION straight to the page
        qb_focus = 0; if (!midi_clk_in) tempo_mul = 1.f; if (k == K_OPT) used[K_OPT] = 1;
        if (k != K_OPT && !ui_qbar && strips_here()) { strip_focus = strip_on() ? 2 : 1; sec_cur = ui_cat >= 0 ? ui_cat : hub_cur; }
    }
    return 1;
}

// ---- the drawer: where everything lies when it is open (it slides down from under the header) ----
typedef struct { int16_t x, y, w, h; } QR;
static const QR qd_at[QD_N] = {
    [QA_SLOW] = {30, 164, 74, 78}, [QA_FAST] = {111, 164, 74, 78}, [QA_SLOCK] = {192, 164, 74, 78}, [QA_FOLOFF] = {273, 164, 74, 78},
    [QA_OD] = {382, 84, 74, 78}, [QA_RECQ] = {463, 84, 74, 78}, [QA_RESAMPLE] = {544, 84, 74, 78}, [QA_CAP] = {625, 84, 74, 78},
    [QA_FOLLOW] = {30, 292, 78, 78}, [QA_SNAP] = {116, 292, 78, 78}, [QA_COPY] = {202, 292, 78, 78}, [QA_PASTE] = {288, 292, 78, 78}, [QA_RND] = {374, 292, 78, 78},
    [QA_BYPASS] = {30, 422, 96, 78}, [QA_MUTEALL] = {134, 422, 96, 78}, [QA_SOLOCLR] = {238, 422, 96, 78},
    [QA_SCNPREV] = {382, 422, 74, 78}, [QA_SCNNEXT] = {624, 422, 74, 78},
    [QA_LOCK] = {30, 552, 96, 78}, [QA_MIDI] = {134, 552, 96, 78},
    [QA_VOL] = {734, 84, 84, 548}, [QA_FILT] = {826, 84, 84, 548}, [QA_TEMPO] = {918, 84, 84, 548},
};
static int qd_item(int i) { return qd_at[i].w > 0; }
static void qd_nav(int dx, int dy) {     // the nearest tile that way
    const QR *c = &qd_at[qd_cur]; int cx = c->x + c->w / 2, cy = c->y + c->h / 2, best = -1; long bs = 0;
    for (int i = 0; i < QD_N; i++) if (qd_item(i) && i != qd_cur) {
        const QR *r = &qd_at[i]; int x = r->x + r->w / 2, y = r->y + r->h / 2;
        if (dx) { int ex = dx > 0 ? r->x - (c->x + c->w) : c->x - (r->x + r->w); if (ex < -10) continue;
            int oy = y < c->y ? c->y - y : y > c->y + c->h ? y - c->y - c->h : 0; if (r->h > 300) oy = 0;
            long s = (long)abs(x - cx) + 4L * oy; if (best < 0 || s < bs) { best = i; bs = s; } }
        else { int ey = dy > 0 ? r->y - (c->y + c->h) : c->y - (r->y + r->h); if (ey < -10) continue;
            long s = (long)abs(y - cy) + 3L * abs(x - cx); if (best < 0 || s < bs) { best = i; bs = s; } }
    }
    if (best >= 0) qd_cur = best;
    else if (dy < 0 && !ui_qbar) { qd_toggle(0); qb_focus = 1; qb_cur = QB_HANDLE; }   // ^ past the top: closed, back on the handle
}
static void qd_value(int d) {            // EDIT+^v on a strip
    switch (qd_cur) {
    case QA_VOL: edit_begin(); song.master = (int8_t)clampi(song.master + d * 2, 0, 100); break;
    case QA_FILT: eng_sweep = fmaxf(-1, fminf(1, eng_sweep + d * 0.1f)); if (fabsf(eng_sweep) < 0.05f) eng_sweep = 0; break;
    case QA_TEMPO: if (midi_clk_in) { toast("CLOCK IN: THE TEMPO COMES FROM MIDI"); break; } edit_begin(); song.tempo = (int16_t)clampi(song.tempo + d, 40, 300); break;
    }
}
static int qd_key(int k, int down) {     // the drawer open: 1 = the key was its
    if (k == K_PLAY || k == K_SHIFT || k == K_MENU || k == K_F1 || k == K_F2) return 0;
    int strip = qd_cur >= QA_N;
    if (k == K_EDIT) {
        if (strip) { qd_edit = down; if (!down && qd_cur == QA_FILT) eng_sweep = 0; return 1; }   // the filter springs back
        if (down) qa_press(qd_cur); else qa_release(qd_cur); return 1; }
    if (!down) return 1;
    if (k == K_OPT) { used[K_OPT] = 1; qa_release(qd_cur); qd_toggle(0); return 1; }   // its release opens no menu
    if (strip && held[K_EDIT] && (k == K_UP || k == K_DOWN)) { qd_value(k == K_UP ? 1 : -1); return 1; }
    if (k == K_LEFT || k == K_RIGHT || k == K_UP || k == K_DOWN) { qa_release(qd_cur); qd_nav(k == K_RIGHT ? 1 : k == K_LEFT ? -1 : 0, k == K_DOWN ? 1 : k == K_UP ? -1 : 0); }
    return 1;
}
static const char *qd_hint(void) {
    return qd_cur == QA_FILT ? "EDIT+^v SWEEP  ARROWS MOVE  OPTION CLOSE" : qd_cur >= QA_N ? "EDIT+^v VALUE  ARROWS MOVE  OPTION CLOSE" : "EDIT PRESS  ARROWS MOVE  OPTION CLOSE";
}
static void mx_meters(void); static void meter_bar(int x, int y, int w, int h, float v); static float mx_show[NSLOT + 3];
static void qd_card(int x, int y, int w, int h, const char *t) { rrect(x, y, w, h, 14, C_CELL); ftext(x + 14, y + 6, FT_S, C_DIM, t); }
static void qd_tile(int i, int oy) {
    const QR *r = &qd_at[i]; int x = r->x, y = r->y + oy, w = r->w, h = r->h, cur = i == qd_cur, lit = qa_lit(i);
    if (cur) rframe(x - 4, y - 4, w + 8, h + 8, 12, 3, C_WHITE, C_BG);
    rrect(x, y, w, h, 10, lit ? C_ORANGE : C_SELBG);
    icon_fit(qa_tab[i].icon, x + (w - 32) / 2, y + 9, 32, lit ? C_BG : cur ? C_WHITE : C_GREY);
    const char *t = i == QA_OD && rec_replace ? "REPLACE" : qa_tab[i].tile;
    ftext(x + (w - ftext_w(FT_S, t)) / 2, y + h - 26, FT_S, lit ? C_BG : C_DIM, t);
    const char *v = qa_txt(i); if (*v && i != QA_OD) { int tw = ftext_w(FT_S, v) + 8; rrect(x + w - tw - 4, y + 4, tw, 20, 4, C_BG); ftext(x + w - tw, y + 4, FT_S, C_ORANGE, v); }
}
static void qd_strip(int i, int oy, const char *val, float v, int bip, int meter) {
    const QR *r = &qd_at[i]; int x = r->x, y = r->y + oy, w = r->w, h = r->h, cur = i == qd_cur;
    if (cur) rframe(x - 4, y - 4, w + 8, h + 8, 12, 3, cur && qd_edit ? C_ORANGE : C_WHITE, C_BG);
    rrect(x, y, w, h, 12, C_SELBG);
    ftext(x + (w - ftext_w(FT_S, qa_tab[i].tile)) / 2, y + 6, FT_S, C_DIM, qa_tab[i].tile);
    int tx = x + 26, t0 = y + 36, t1 = y + h - 50, yv = t1 - (int)((t1 - t0) * v);
    rrect(tx, t0, 10, t1 - t0, 5, C_BG);
    if (bip) { int ym = (t0 + t1) / 2; rect(tx + 2, ym < yv ? ym : yv, 6, abs(ym - yv) + 1, C_ORANGE, 1); rect(tx - 6, ym, 22, 1, C_DIM, 1); }
    else rect(tx + 2, yv, 6, t1 - yv - 2, C_ORANGE, 1);
    rrect(tx - 10, yv - 6, 30, 12, 4, C_WHITE);
    if (meter) { meter_bar(x + 54, t0, 6, t1 - t0, mx_show[NSLOT + 2]); meter_bar(x + 63, t0, 6, t1 - t0, mx_show[NSLOT + 2]); }
    ftext(x + (w - ftext_w(FT_M, val)) / 2, y + h - 38, FT_M, C_WHITE, val);
}
static void qd_meter(int x, int y, int w, int h, const char *name, const char *txt, int pct, int bad) {
    rrect(x, y, w, h, 10, C_SELBG); ftext(x + 10, y + 6, FT_S, C_DIM, name); ftext(x + 10, y + 26, FT_MB, C_WHITE, txt);
    rrect(x + 10, y + h - 16, w - 20, 7, 3, C_BG); if (pct > 0) rrect(x + 10, y + h - 16, (w - 20) * clampi(pct, 0, 100) / 100, 7, 3, bad ? C_RED : th_cur[TC_SMP]);
}
static Anim qd_anim;
static void qd_draw(void) {
    float e = anim(&qd_anim, qd_open, 220); if (e <= 0) return;
    int oy = -(int)((1 - e) * 640);        // it comes down from under the header
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(R, 0, 0, 0, (Uint8)(150 * e));
    SDL_Rect sh = {0, 48, W, V2_FOOT - 48}; SDL_RenderFillRect(R, &sh); SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
    SDL_Rect clip = {0, 44, W, V2_FOOT - 44}; SDL_RenderSetClipRect(R, &clip);
    rframe(6, 40 + oy, W - 12, 622, 22, 2, C_LINE, C_BG);
    char b[64];
    qd_card(18, 56 + oy, 340, 196, "TEMPO");
    snprintf(b, sizeof b, "%d", song.tempo); ftext(34, 80 + oy, FT_XB, C_WHITE, b); ftext(34 + ftext_w(FT_XB, b) + 8, 92 + oy, FT_M, C_DIM, "BPM");
    snprintf(b, sizeof b, "SWING %d%%", song.swing); ftext(200, 80 + oy, FT_S, C_DIM, b);
    snprintf(b, sizeof b, "%d/4%s", song.beats > 0 ? song.beats : 4, tempo_mul != 1.f ? "  NUDGE" : ""); ftext(200, 104 + oy, FT_S, C_DIM, b);
    for (int i = QA_SLOW; i <= QA_FOLOFF; i++) qd_tile(i, oy);
    qd_card(370, 56 + oy, 340, 196, "RECORD");
    for (int i = QA_OD; i <= QA_CAP; i++) qd_tile(i, oy);
    ftext(384, 176 + oy, FT_S, C_DIM, rec_replace ? "REPLACE: A TAKE WIPES THE CLIP" : "OVERDUB: NEW NOTES OVER OLD");
    snprintf(b, sizeof b, "REC QUANT %s", quant_name[song.rec_quant % 5]); ftext(384, 200 + oy, FT_S, C_DIM, b);
    qd_card(18, 264 + oy, 692, 118, "EDIT");
    for (int i = QA_FOLLOW; i <= QA_RND; i++) qd_tile(i, oy);
    ftext(470, 300 + oy, FT_S, C_DIM, follow_ph ? "PAGE: PLAYHEAD" : "PAGE: CURSOR");
    snprintf(b, sizeof b, "SNAP %s", song.snap ? "ON" : "OFF"); ftext(470, 324 + oy, FT_S, C_DIM, b);
    qd_card(18, 394 + oy, 340, 118, "SOUND");
    for (int i = QA_BYPASS; i <= QA_SOLOCLR; i++) qd_tile(i, oy);
    qd_card(370, 394 + oy, 340, 118, "SCENE");
    qd_tile(QA_SCNPREV, oy); qd_tile(QA_SCNNEXT, oy);
    rrect(464, 422 + oy, 152, 78, 10, C_SELBG); snprintf(b, sizeof b, "%d SCENE%s%s", scn_n, scn_n > 1 ? "S" : "", scn_lock ? "  LOCK" : ""); ftext(476, 428 + oy, FT_S, C_DIM, b);
    snprintf(b, sizeof b, scn_next >= 0 ? "%d > %d" : "SCENE %d", scn_cur + 1, scn_next + 1); ftext(476, 454 + oy, FT_MB, C_WHITE, b);
    qd_card(18, 524 + oy, 692, 118, "DEVICE");
    qd_tile(QA_LOCK, oy); qd_tile(QA_MIDI, oy);
    { int chg, p = batt_pct(&chg); if (p >= 0) snprintf(b, sizeof b, "%d %%%s", p, chg ? "  CHARGING" : ""); else snprintf(b, sizeof b, "?");
      qd_meter(238, 552 + oy, 222, 78, "BATTERY", b, p, p >= 0 && p < 20); }
    { int ld = clampi((int)(engine_load() * 100), 0, 100); snprintf(b, sizeof b, "%d %%  %d VOICES", ld, engine_voices()); qd_meter(468, 552 + oy, 230, 78, "CPU", b, ld, ld >= 85); }
    qd_card(722, 56 + oy, 284, 586, "MASTER");
    mx_meters();
    snprintf(b, sizeof b, "%d", song.master); qd_strip(QA_VOL, oy, b, song.master / 100.f, 0, 1);
    float sw = eng_sweep; snprintf(b, sizeof b, sw < 0 ? "LP" : sw > 0 ? "HP" : "OFF"); qd_strip(QA_FILT, oy, b, (sw + 1) / 2, 1, 0);
    snprintf(b, sizeof b, "%d", song.tempo); qd_strip(QA_TEMPO, oy, b, (song.tempo - 40) / 260.f, 0, 0);
    rrect(W / 2 - 40, 650 + oy, 80, 6, 3, C_DIM);   // the handle
    SDL_RenderSetClipRect(R, NULL);
}
