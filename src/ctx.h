// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// v3 (2026-11-20, user): OPTION tapped alone opens the actions of what is under the cursor. X and Y are not used any more:
// their jobs are items here. An item either calls a function, presses an old combo for the screen that knows it (the
// screens keep their code), or latches X so that the arrows do what X+arrows did until another key.
// main.c includes this right after key_event1.
#pragma once

typedef struct { const char *name; int key, mod, latch; void (*fn)(void); } CtxItem;
enum { CTX_MAX = 14 };
static CtxItem ctx_it[CTX_MAX]; static int ctx_n, ctx_cur, ctxing, sim_on, x_latch, v_latch;   // v_latch: arrows change 1 LENGTH, 2 GRID

static void ctx_add(const char *n, int key, int mod, int latch, void (*fn)(void)) {
    if (ctx_n < CTX_MAX) ctx_it[ctx_n++] = (CtxItem){n, key, mod, latch, fn};
}
static void cx_listen(void) { listen(sel); }
static void cx_step(void) { step_listen(cur_step()); }
static void cx_mute(void) { int s = tab == T_MIX && mcol < NSLOT ? mcol : sel; act_mute_toggle(s); toast(song.t[s].mute ? "MUTE ON" : "MUTE OFF"); }
static void cx_solo(void) { int s = tab == T_MIX && mcol < NSLOT ? mcol : sel; act_solo_toggle(s); toast(song.mix.pad[s].solo ? "SOLO ON" : "SOLO OFF"); }
static void cx_copy(void) { copy_paste(0); }
static void cx_paste(void) { copy_paste(1); }
static void chord_open(void);
static void lay_go(int m) {             // v3 (user): the layout of the clip: STEPS, PIANO ROLL, CHORDS
    Pattern *p = cur_pat(sel); if (p->mode == m) return;
    pat_mode_step((m - p->mode + 3) % 3);
    if (cfm) return;                      // CHORDS over notes: the question first
    cat_open(PC_PATTERN); if (cur_pat(sel)->mode == 2 && !chording) chord_open();
}
static void lay_steps(void) { lay_go(0); }
static void lay_roll(void) { lay_go(1); }
static void lay_chords(void) { lay_go(2); }
static void cx_patterns(void) { if (rolling) roll_leave(); if (chording) chord_leave(); pat_open(); }
static void cx_record(void) { cat_open(PC_PATTERN); if (rolling) roll_leave(); if (chording) chord_leave(); recpage = 1; }
static void cx_lfo(void) { cat_open(PC_MOD); }
static void cx_auto(void) { set_tab(T_SEQ); ui_cat = hub_cur = PC_MOD; if (!is_auto(lane)) lane = L_A1; set_step(0); seq_clamp(); }
static void cx_duck(void) { duck_open(sel); }
static void lat_len(void) { v_latch = 1; toast("ARROWS: LENGTH (^v A BAR)"); }
static void lat_grid(void) { v_latch = 2; toast("ARROWS: GRID"); }
static void ctx_layout(void) {
    int m = cur_pat(sel)->mode;
    if (m != 0) ctx_add("LAYOUT: STEPS", 0, 0, 0, lay_steps);
    if (m != 1) ctx_add("LAYOUT: PIANO ROLL", 0, 0, 0, lay_roll);
    if (m != 2) ctx_add("LAYOUT: CHORDS", 0, 0, 0, lay_chords);
    ctx_add("LENGTH", 0, 0, 0, lat_len); ctx_add("GRID", 0, 0, 0, lat_grid);
    ctx_add("PATTERNS / FOLLOW", 0, 0, 0, cx_patterns); ctx_add("RECORD", 0, 0, 0, cx_record);
}
static void cx_undo(void) { act_undo(); }
static void ctx_common(void) { ctx_add("UNDO", 0, 0, 0, cx_undo); ctx_add("COPY", 0, 0, 0, cx_copy); ctx_add("PASTE", 0, 0, 0, cx_paste); ctx_add("CLEAR", 0, 0, 0, delete_action); }

static int ctx_ok(void) {
    return !perform && !qb_focus && !strip_focus && !ui_simple && !learning && !exporting && !asking && !cfm && !naming && !ctxing
        && (!ui_window() || ((rolling || chording || ducking) && !randing && !presetting)) && tab != T_PRJ && !hubbing && !patmenu && !modmenu && !recpage;
}
static void ctx_open(void) {
    ctx_n = ctx_cur = 0;
    if (chording) { ctx_layout(); ctx_add("CHORD LENGTH", K_X, -1, 0, NULL); }
    else if (rolling) { ctx_layout(); ctx_add("TRIAD", K_EDIT, K_X, 0, NULL); ctx_add("VELOCITY / CHANCE", 0, 0, 1, NULL); ctx_add("RANDOM", K_X, K_SHIFT, 0, NULL); }
    else if (ducking) { ctx_add("NEW POINT", K_X, -1, 0, NULL); ctx_add("BEND", 0, 0, 1, NULL); }
    else if (scening) ctx_add("NEW COPY", K_X, -1, 0, NULL);
    else if (tab == T_GRID && ui_cat < 0) {
        ctx_add("CLIP START / STOP", K_X, -1, 0, NULL); ctx_add("LISTEN", 0, 0, 0, cx_listen); ctx_add("MUTE", 0, 0, 0, cx_mute);
        ctx_add("SOLO", 0, 0, 0, cx_solo); ctx_add("NAME", K_X, K_SHIFT, 0, NULL); ctx_add("FREEZE", K_Y, K_SHIFT, 0, NULL); ctx_common();
    } else if (v2_steps()) {
        if (!is_auto(lane)) ctx_layout();
        if (is_auto(lane)) ctx_add("BEND", 0, 0, 1, NULL);
        else { ctx_add("LISTEN STEP", 0, 0, 0, cx_step); ctx_add("SELECT STEPS", 0, 0, 1, NULL); }
        ctx_add("DOUBLE CLIP", 0, 0, 0, clip_double); ctx_add("RANDOM", K_X, K_SHIFT, 0, NULL); ctx_common();
    } else if (v2_fx()) { ctx_add("INSERT ON / OFF", K_X, -1, 0, NULL); ctx_common(); }
    else if (tab == T_MIX && mcol < NSLOT) { ctx_add("MUTE", 0, 0, 0, cx_mute); ctx_add("SOLO", 0, 0, 0, cx_solo); ctx_common(); }
    else if (v2_lfo() || v2_auto()) {    // MOD: its three kinds (the list that was MOD's menu)
        if (!v2_lfo()) ctx_add("LFO", 0, 0, 0, cx_lfo);
        if (!v2_auto()) ctx_add("AUTOMATION", 0, 0, 0, cx_auto);
        ctx_add("DUCK", 0, 0, 0, cx_duck);
        if (v2_auto()) ctx_add("BEND", 0, 0, 0, NULL), ctx_it[ctx_n - 1].latch = 1;
        ctx_common(); }
    else if (v2_sound()) { ctx_add("LISTEN", 0, 0, 0, cx_listen); ctx_add("MIDI LEARN", K_R2, -1, 0, NULL); ctx_common(); }
    else { ctx_add("LISTEN", 0, 0, 0, cx_listen); ctx_common(); }
    ctxing = ctx_n > 0;
}
static void key_event1(int k, int down);
static void sim_key(int key, int mod) {   // an old combo, pressed for the screen that knows it
    sim_on = 1;
    if (mod >= 0) { held[mod] = 1; used[mod] = 0; }
    key_event1(key, 1); key_event1(key, 0);
    if (mod >= 0) { held[mod] = 0; used[mod] = 1; }
    sim_on = 0;
}
static void ctx_run(const CtxItem *c) {
    ctxing = 0;
    if (c->fn) c->fn();
    else if (c->latch) { x_latch = 1; held[K_X] = used[K_X] = 1; toast("ARROWS NOW: ANY OTHER KEY ENDS IT"); }
    else sim_key(c->key, c->mod ? c->mod : -1);
}
static void ctx_key(int k, int down) {
    if (!down) return;
    if (k == K_UP || k == K_DOWN) ctx_cur = (ctx_cur + (k == K_DOWN ? 1 : ctx_n - 1)) % ctx_n;
    else if (k == K_EDIT) { used[K_EDIT] = 1; ctx_run(&ctx_it[ctx_cur]); }
    else if (k == K_OPT || k == K_MENU) { used[k] = 1; ctxing = 0; }
}
static int x_latch_key(int k, int down) {  // 1: the key ended the latch and is spent
    if (v_latch) {                        // LENGTH / GRID of the clip from inside its layout
        if (!down) return k <= K_RIGHT;
        if (k > K_RIGHT) { v_latch = 0; used[k] = 1; toast("DONE"); return 1; }
        int d = k == K_RIGHT ? 1 : k == K_LEFT ? -1 : k == K_UP ? 16 : -16, c = song.t[sel].cur;
        if (v_latch == 1) { act_pattern_len_step(sel, c, d); toast_length("LENGTH", cur_pat(sel)->len); }
        else { act_pattern_div_step(sel, c, d > 0 ? 1 : -1); char b[32]; snprintf(b, sizeof b, "GRID %s", div_name[cur_pat(sel)->div % 6]); toast(b); }
        seq_clamp(); return 1;
    }
    if (!x_latch) return 0;
    if (k <= K_RIGHT) { held[K_X] = used[K_X] = 1; return 0; }
    if (!down) return 0;
    x_latch = 0; held[K_X] = 0; used[K_X] = 1; toast("DONE"); return 1;
}
// v3: the page's own tabs are a row the cursor reaches (UP from the page's top); <> turn them as L1/R1 used to.
static int page_tabs(void) { return !ui_over() && !perform && (v2_sound() || v2_steps() || v2_fx() || v2_lfo() || v2_auto() || v2_mixer() || v2_unit()); }
static int tabs_key(int k, int down) {
    if (sim_on) return 0;                 // its own L1/R1 go on to the page
    if (k == K_PLAY || k == K_SHIFT || k == K_MENU || k == K_F1 || k == K_F2) return 0;
    if (!down) return 1;
    if (k == K_LEFT || k == K_RIGHT) sim_key(k == K_RIGHT ? K_R1 : K_L1, -1);
    else if (k == K_UP) { tab_focus = 0; if (strips_here()) { strip_focus = sec_on() ? 1 : 2; sec_cur = ui_cat >= 0 ? ui_cat : hub_cur; } else if (!ui_qbar) qb_focus = 1; }
    else if (k == K_DOWN || k == K_OPT || k == K_EDIT) tab_focus = 0;
    return 1;
}
static void tabs_frame(void) {          // an outline round the tab row (draw() puts it on the page before the page goes under the strips)
    rect(14, 66, W - 28, 3, C_WHITE, 1); rect(14, 115, W - 28, 3, C_WHITE, 1); rect(14, 66, 3, 52, C_WHITE, 1); rect(W - 17, 66, 3, 52, C_WHITE, 1);
}
static int y_back_here(void) {            // windows and sub-pages where Y was the way back: MENU+UP now
    return ui_over() || naming || exporting || asking || cfm || scening || recpage || tab == T_PAT || v2_unit() || (tab == T_PRJ && jcol)
        || (v2_mixer() && mx_pg && !(mx_pg == 1 && mcol < NSLOT));
}
static void draw_ctx(void) {
    enum { PW = 460 }; int RH = ctx_n > 10 ? 38 : 46, ph = 70 + ctx_n * RH, px = (W - PW) / 2, py = (H - ph) / 2;
    ui_shade(150); rframe(px, py, PW, ph, 12, 3, C_ORANGE, C_CELL);
    ftext(px + 24, py + 18, FT_M, C_DIM, "OPTIONS");
    for (int i = 0; i < ctx_n; i++) {
        int y = py + 56 + i * RH, on = i == ctx_cur;
        if (on) rrect(px + 12, y, PW - 24, RH - 6, 8, C_SELBG);
        ftext(px + 28, y + (RH - 6 - fv2[FT_MB].h) / 2, FT_MB, on ? C_WHITE : C_DIM, ctx_it[i].name);
        if (ctx_it[i].latch) ftext(px + PW - 28 - ftext_w(FT_S, "ARROWS"), y + (RH - 6 - fv2[FT_S].h) / 2, FT_S, C_DIM, "ARROWS");
    }
}
