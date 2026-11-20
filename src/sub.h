// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// SUB (2026-11-20, user's plan of 2026-11-13): EDIT on a step of the SUB lane (was REP) opens its cells. The step is cut
// into N (1..8) cells; each is on or off, has its velocity and a note (a sliced pad: a slice) over the step's.
// The data is the pool in Song.sub (bp.h): an entry only while a cell differs from the step.
static int sb_step, sb_cell, sb_combo, sb_heard;
enum { SB_TAG = 340 };
static void sub_open(int st) { subbing = 1; sb_step = st; sb_cell = 0; }
static void sub_hear(void) {
    Pattern *p = cur_pat(sel); SubStep *e = sub_find(sel, song.t[sel].cur, sb_step); int k = sb_cell;
    int v = e && e->vel[k] > 0 ? e->vel[k] : p->vel[sb_step], dn = e ? e->note[k] : 0, n = track_nslices(sel);
    float semis = p->note[sb_step] == NOTE_NONE ? 0 : (float)note_semis(sel, p->note[sb_step]);
    int sl = n ? (p->slice[sb_step] < 0 ? sb_step : p->slice[sb_step]) % n : -1;
    if (sl >= 0 && dn) sl = ((sl + dn) % n + n) % n; else if (sl < 0) semis += dn;
    if (sb_heard) engine_release_key(SB_TAG);
    engine_trig_key(sel, v / 127.f, semis, sl, SB_TAG); sb_heard = 1;
}
static void sub_quiet(void) { if (sb_heard) { engine_release_key(SB_TAG); sb_heard = 0; } }
static void sub_key(int k, int down) {
    Pattern *p = cur_pat(sel); int q = song.t[sel].cur, n = p->rep[sb_step] < 1 ? 1 : p->rep[sb_step];
    if (!down) {
        if (k == K_EDIT && !sb_combo && !held[K_OPT]) {   // a tap: the cell on or off
            SubStep *e = sub_get(sel, q, sb_step); if (!e) { toast("512 SUB STEPS: THE MOST"); return; }
            push_undo(); edit_begin(); e->off ^= (uint8_t)(1 << sb_cell); sub_tidy(e); }
        if (k == K_OPT) sub_quiet();
        return;
    }
    used[k] = 1;
    if (k == K_EDIT) { sb_combo = 0; if (held[K_OPT]) { SubStep *e = sub_find(sel, q, sb_step); sb_combo = 1;   // OPT+EDIT: the cell as the step
            if (e) { push_undo(); edit_begin(); e->off &= (uint8_t)~(1 << sb_cell); e->vel[sb_cell] = 0; e->note[sb_cell] = 0; sub_tidy(e); } }
        return; }
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_Y || k == K_MENU) { sub_quiet(); subbing = 0; return; }
    if (k == K_OPT) { sub_hear(); return; }
    if (k == K_L1 || k == K_R1) { push_undo(); edit_begin(); p->rep[sb_step] = (int8_t)clampi(n + (k == K_R1 ? 1 : -1), 1, NCELL); if (sb_cell >= p->rep[sb_step]) sb_cell = p->rep[sb_step] - 1; return; }   // how many cells
    if (held[K_EDIT] && (k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT)) {
        SubStep *e = sub_get(sel, q, sb_step); if (!e) { toast("512 SUB STEPS: THE MOST"); return; }
        sb_combo = 1; edit_begin(); int d = k == K_UP || k == K_RIGHT ? 1 : -1;
        if (k == K_UP || k == K_DOWN) { int v = e->vel[sb_cell] > 0 ? e->vel[sb_cell] : p->vel[sb_step]; v = clampi(v + d * 8, 1, 127); e->vel[sb_cell] = (int8_t)(v == p->vel[sb_step] ? 0 : v); }
        else e->note[sb_cell] = (int8_t)clampi(e->note[sb_cell] + d, -24, 24);
        sub_tidy(e); if (hear_on) sub_hear(); return; }
    if (k == K_LEFT) sb_cell = (sb_cell + n - 1) % n; else if (k == K_RIGHT) sb_cell = (sb_cell + 1) % n;
}
static void draw_sub(void) {
    Pattern *p = cur_pat(sel); const SubStep *e = sub_find(sel, song.t[sel].cur, sb_step); Col ec = C_CAT(PC_PATTERN); char b[32];
    int n = p->rep[sb_step] < 1 ? 1 : p->rep[sb_step], sl = track_nslices(sel); if (sb_cell >= n) sb_cell = n - 1;
    snprintf(b, sizeof b, "STEP %d   %d CELL%s", sb_step + 1, n, n > 1 ? "S" : ""); ftext(32, 84, FT_MB, C_WHITE, b);
    int gw = 971, cw = gw / n, top = 140, ch = 420;
    for (int k = 0; k < n; k++) {
        int x = 26 + k * cw, on = !(e && (e->off >> k & 1)), cur = k == sb_cell;
        int v = e && e->vel[k] > 0 ? e->vel[k] : p->vel[sb_step], dn = e ? e->note[k] : 0;
        if (cur) rframe(x + 3, top, cw - 6, ch, 8, 4, C_WHITE, C_CELL); else rrect(x + 3, top, cw - 6, ch, 8, C_CELL);
        int bh = (ch - 120) * v / 127;   // the velocity as a bar, the cell's colour when it plays
        rrect(x + 16, top + ch - 60 - bh, cw - 32, bh > 4 ? bh : 4, 6, on ? (e && e->vel[k] > 0 ? ec : mix(C_CELL, ec, 150)) : C_LINE);
        snprintf(b, sizeof b, "%d", k + 1); ftext(x + cw / 2 - ftext_w(FT_M, b) / 2, top + 12, FT_M, cur ? C_WHITE : C_DIM, b);
        if (!on) snprintf(b, sizeof b, "OFF"); else if (dn) snprintf(b, sizeof b, "%+d%s", dn, sl ? " SL" : ""); else snprintf(b, sizeof b, "-");
        if (ftext_w(FT_M, b) < cw - 12) ftext(x + cw / 2 - ftext_w(FT_M, b) / 2, top + ch - 44, FT_M, on ? (dn ? C_ORANGE : C_GREY) : C_DIM, b);
    }
    { int k = sb_cell, v = e && e->vel[k] > 0 ? e->vel[k] : p->vel[sb_step], dn = e ? e->note[k] : 0; char nn[16], vv[32];
      snprintf(nn, sizeof nn, "CELL %d", k + 1);
      if (e && (e->off >> k & 1)) snprintf(vv, sizeof vv, "OFF"); else snprintf(vv, sizeof vv, "VEL %d  %s %+d", v, sl ? "SLICE" : "NOTE", dn);
      ui_focus(nn, vv, v * 100 / 127, ec); }
    ftext(32, top + ch + 30, FT_M, C_GREY, sl ? "NOTE: SLICES OVER THE STEP'S SLICE" : "NOTE: SEMITONES OVER THE STEP'S NOTE");
    draw_footer("EDIT ON/OFF  L1/R1 CELLS  OPT HEAR  EDIT+^v VELOCITY  EDIT+<> NOTE  OPT+EDIT AS THE STEP  Y BACK");
}
