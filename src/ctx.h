// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// v3 (2026-11-20): what the OPTION menu was built on. Since 2026-11-24 (user, M8) OPTION is a modifier and the menu is
// gone; left here: sim_key (an old combo pressed for the screen that knows it, used by the side slots), the X latch and
// the LENGTH / GRID latch, the page's tab row, the way back (Y) and the quick bar (X).
// main.c includes this right after key_event1.
#pragma once

static int sim_on, x_latch, v_latch;     // sim_key running; X latched for the arrows; v_latch: arrows change 1 LENGTH, 2 GRID
static void export_open(void); static int ex_resample;
static void cx_resample(void) { export_open(); ex_resample = 1; toast("RESAMPLE: EDIT RENDERS INTO AN EMPTY PAD"); }
static void chord_open(void);
static void lay_go(int m) {             // v3 (user): the layout of the clip: STEPS, PIANO ROLL, CHORDS
    Pattern *p = cur_pat(sel); if (p->mode == m) return;
    pat_mode_step((m - p->mode + 3) % 3);
    if (cfm) return;                      // CHORDS over notes: the question first
    cat_open(PC_PATTERN); if (cur_pat(sel)->mode == 2 && !chording) chord_open();
}
static void cx_patterns(void) { if (rolling) roll_leave(); if (chording) chord_leave(); pat_open(); }
static void cx_lfo(void) { cat_open(PC_MOD); }
static void cx_auto(void) { set_tab(T_SEQ); ui_cat = hub_cur = PC_MOD; if (!is_auto(lane)) lane = L_A1; set_step(0); seq_clamp(); }
static void cx_duck(void) { duck_open(sel); }

static int ctx_ok(void) {
    return !perform && !qb_focus && !strip_focus && !ui_simple && !learning && !exporting && !asking && !cfm && !naming
        && (!ui_window() || ((rolling || chording || ducking) && !randing && !presetting)) && tab != T_PRJ;
}
static void key_event1(int k, int down);
static void sim_key(int key, int mod) {   // an old combo, pressed for the screen that knows it
    sim_on = 1;
    if (mod >= 0) { held[mod] = 1; used[mod] = 0; }
    key_event1(key, 1); key_event1(key, 0);
    if (mod >= 0) { held[mod] = 0; used[mod] = 1; }
    sim_on = 0;
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
static int page_tabs(void) { return !ui_over() && !perform && (v2_sound() || v2_steps() || v2_fx() || v2_lfo() || v2_auto() || v2_mixer() || v2_unit()); }   // each draws its tab row (MIXER: mx_row)
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
    return ui_over() || naming || exporting || asking || cfm || scening || tab == T_PAT || v2_unit() || (tab == T_PRJ && jcol)
        || (v2_mixer() && mx_pg == 2);
}

// 2026-11-21 (user): Y is the way back everywhere, X reaches the quick bar, X again its drawer, X once more closes it.
// The old jobs of X and Y stay in OPTION's menu; PADS, KEYS and the simple view keep their own X and Y.
static void xy_key(int k) {
    if (k == K_Y) {
        if (qpicking) { qpicking = 0; return; }
        if (x_latch || v_latch) { x_latch = v_latch = 0; held[K_X] = 0; used[K_X] = 1; return; }   // X is let go with the latch
        if (qd_open) { qd_toggle(0); return; }
        if (qb_focus || strip_focus || tab_focus) { qb_focus = strip_focus = tab_focus = 0; if (!midi_clk_in) tempo_mul = 1.f; return; }
        if (pt_pick) { pt_pick = 0; toast("CABLE DROPPED"); return; }   // PATCH: the carried cable only
        if (mlearn_par >= 0) { mlearn_par = -1; toast("MIDI LEARN CANCELLED"); return; }
        if (v2_mixer() && mx_pg == 1) { mx_pg = 0; if (mcol >= NSLOT) mcol = sel; return; }   // the drawer closes
        if (learning || y_back_here()) { sim_key(K_Y, -1); return; }   // windows and sub-pages: the old way back
        if (v2_mixer()) { set_tab(T_GRID); ui_cat = -1; return; }   // MIXER, as SCENES: back to PADS
        if (tab == T_PRJ) { menu_nav(K_DOWN); return; }   // back where PROJECT was opened
        if (in_pad()) { menu_nav(K_UP); return; }        // a pad's section: HOME
        return;
    }
    if (browsing && !naming) { sim_key(K_X, -1); return; }   // the browsers: X is their FILES menu
    if (naming || exporting || asking || cfm || learning || qpicking || ui_over()) return;   // a window is open: it answers first
    if (x_latch || v_latch) { x_latch = v_latch = 0; held[K_X] = 0; }   // a latch ends when the bar opens
    if (qd_open) { qd_toggle(0); return; }
    if (qb_focus) { qd_toggle(1); return; }
    strip_focus = tab_focus = 0; qb_focus = 1; qb_ready(); if (qb_cur > QB_HANDLE) qb_cur = QB_HANDLE;
}
