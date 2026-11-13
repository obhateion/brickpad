// UI v2, step 7: the simple view (PROJECT > SYSTEM > VIEW; mockups P1-P4). Three large screens over the same song and the
// same actions: PLAY (the pads as tiles), NOTES (the open pad's steps), SOUND (four knobs with plain names).
// MENU + UP / RIGHT / DOWN (or MENU alone: the next one) changes the screen, F1/F2 the pad, F1+F2 held 3 s leaves the view.
// KID LOCK: nothing is deleted or overwritten here (notes stay, the dice is off). main.c includes this once, after pages.h.
#pragma once
static int sv_mode, sv_knob; static Uint32 sv_exit;   // the screen; SOUND: the knob the arrows chose; when F1+F2 went down together
static const char *const sv_name[3] = {"PLAY", "NOTES", "SOUND"};
static Col sv_col(int m) { return m == 0 ? C_PLAY : m == 1 ? th_cur[TC_SMP] : C_ENG(1); }
typedef struct { const char *nm; int p; } SvKnob;      // in the order UP RIGHT LEFT DOWN
static void sv_knobs(int s, SvKnob *k) {
    switch (song.t[s].type & 3) {
    case 1: k[0] = (SvKnob){"BRIGHT", P_SY + SP_CUTOFF}; k[1] = (SvKnob){"SCREAM", P_SY + SP_RESO}; k[2] = (SvKnob){"TAIL", P_SY + SP_RELEASE}; k[3] = (SvKnob){"DIRT", P_SY + SP_NOISE}; break;
    case 2: k[0] = (SvKnob){"BRIGHT", P_AC + AP_CUTOFF}; k[1] = (SvKnob){"SCREAM", P_AC + AP_RESO}; k[2] = (SvKnob){"TAIL", P_AC + AP_DECAY}; k[3] = (SvKnob){"DIRT", P_AC + AP_DRIVE}; break;
    case 3: k[0] = (SvKnob){"BRIGHT", P_CX + CX_TONE}; k[1] = (SvKnob){"SHIMMER", P_CL + CP_SHIMMER}; k[2] = (SvKnob){"TAIL", P_CX + CX_RELEASE}; k[3] = (SvKnob){"SPRAY", P_CL + CP_SPRAY}; break;
    default: k[0] = (SvKnob){"PITCH", P_PITCH}; k[1] = (SvKnob){"START", P_START}; k[2] = (SvKnob){"TAIL", P_SX + SX_FOUT}; k[3] = (SvKnob){"LOUD", P_VOL};
    }
}
static void simple_sync(void) {          // the state under the simple screens: HOME for PLAY and SOUND, the pad's steps for NOTES
    hubbing = patmenu = modmenu = scening = recpage = 0; perform = 0; if (rolling) roll_leave();
    if (sv_mode == 1) { tab = T_SEQ; ui_cat = PC_PATTERN; lane = lane_first(); if (srow < 1 || srow > 2) { srow = 1; scol = 0; } }
    else { tab = T_GRID; ui_cat = -1; }
}
static void sv_pad(int d) { for (int i = 1; i <= NSLOT; i++) { int s = (sel + d * i + NSLOT * 2) % NSLOT; if (has_sound(s)) { act_select_pad(s); return; } } }
static void sv_dice(void) {              // SHIFT+X on SOUND: the four knobs at random
    SvKnob kn[4]; sv_knobs(sel, kn);
    for (int i = 0; i < 4; i++) { if (kn[i].p == P_VOL) continue; int n = rand() % 11 - 5; for (int j = 0; j < abs(n); j++) par_adjust(sel, kn[i].p, n > 0 ? 1 : -1, 1); }
    act_listen(sel); toast("DICE: A NEW SOUND, SHIFT+L2 IN THE ADVANCED VIEW UNDOES");
}
static int simple_key(int k, int down) {  // 1 = the key is used up here; 0 = NOTES lets the steps' own keys through
    simple_sync();
    if (k == K_F1 || k == K_F2) { int o = k == K_F1 ? K_F2 : K_F1;
        if (down && held[o]) { used[K_F1] = used[K_F2] = 1; sv_exit = SDL_GetTicks() | 1; }
        else if (!down) { sv_exit = 0; if (!used[k]) sv_pad(k == K_F2 ? 1 : -1); }
        return 1; }
    if (k == K_MENU) { menu_down = down && !held[K_SHIFT] ? SDL_GetTicks() : 0; if (!down && !used[K_MENU]) { sv_mode = (sv_mode + 1) % 3; simple_sync(); } return 1; }
    if (held[K_MENU] && k <= K_RIGHT) { if (down) { used[K_MENU] = 1; if (k != K_LEFT) sv_mode = k == K_UP ? 0 : k == K_RIGHT ? 1 : 2; simple_sync(); } return 1; }
    if (k == K_PLAY) { if (down && !held[K_SHIFT]) play_key(); return 1; }
    if (!has_sound(sel) && sv_mode) return 1;
    if (sv_mode == 1) {
        if (cur_pat(sel)->mode) return 1;                    // a piano roll: shown as it is, edited in the advanced view
        if (held[K_SHIFT] || k == K_SHIFT || k == K_Y || k == K_X || k == K_L1 || k == K_R1 || k == K_L2 || k == K_R2) return 1;
        if (ui_lock) { if ((k == K_OPT && held[K_EDIT]) || (k == K_EDIT && held[K_OPT])) return 1;
            if (k == K_EDIT && !down && !used[K_EDIT]) { const Pattern *p = cur_pat(sel); int st = cur_step(); if (st < p->len && p->trig[st]) { toast("KID LOCK: THE NOTE STAYS"); return 1; } } }
        return 0;
    }
    if (sv_mode == 0) {
        if (!down) { if (k == K_EDIT && !used[K_EDIT]) { if (!has_sound(sel)) toast("AN EMPTY PAD"); else if (pat_used(cur_pat(sel))) act_clip_toggle(sel); else act_listen(sel); } return 1; }
        if (k <= K_RIGHT && held[K_EDIT]) { used[K_EDIT] = 1; if (k == K_UP || k == K_DOWN) act_pad_volume_step(sel, k == K_UP ? 5 : -5); return 1; }
        if (k == K_LEFT && sel % 4) sel--; else if (k == K_RIGHT && sel % 4 < 3) sel++; else if (k == K_UP && sel >= 4) sel -= 4; else if (k == K_DOWN && sel < 8) sel += 4;
        else if (k == K_Y) act_mute_toggle(sel); else if (k == K_L1 || k == K_R1) act_scene(k == K_R1 ? 1 : -1); else if (k == K_OPT) act_listen(sel);
        return 1;
    }
    if (!down) return 1;
    SvKnob kn[4]; sv_knobs(sel, kn);
    if (k == K_X && held[K_SHIFT]) { used[K_SHIFT] = 1; if (ui_lock) toast("KID LOCK: THE SOUND STAYS"); else sv_dice(); }
    else if (k <= K_RIGHT && held[K_EDIT]) { used[K_EDIT] = 1; par_adjust(sel, kn[sv_knob].p, k == K_RIGHT || k == K_UP ? 1 : -1, 1); }
    else if (k <= K_RIGHT) sv_knob = k == K_UP ? 0 : k == K_RIGHT ? 1 : k == K_LEFT ? 2 : 3;
    else if (k == K_OPT) act_listen(sel);
    return 1;
}
static void sv_arc(int cx, int cy, int r, float f0, float f1, Col c) {   // a knob's arc from 7 to 5 o'clock, the part f0..f1 of it
    for (float f = f0; f <= f1; f += 0.012f) { float a = (float)M_PI * (0.75f + 1.5f * f); disc2(cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r), 4, c); }
}
static void draw_simple(void) {
    simple_sync(); Uint32 now = SDL_GetTicks(); char b[64]; Col mc = sv_col(sv_mode);
    rect(0, 0, W, V2_HEAD, C_CELL, 1); rect(0, 0, 190, V2_HEAD, mc, 1);       // the header: the screen's name on its colour, three dots, the tempo or the pad
    ftext(95 - ftext_w(FT_MB, sv_name[sv_mode]) / 2, 18, FT_MB, C_BG, sv_name[sv_mode]);
    for (int i = 0; i < 3; i++) disc2(218 + i * 26, 30, 6, i == sv_mode ? mc : C_LINE);
    if (sv_mode == 0) snprintf(b, sizeof b, "%c %d", engine_playing() ? 1 : 2, (int)(song.tempo * tempo_mul + 0.5f));
    else snprintf(b, sizeof b, "%02d %.12s", sel + 1, slot_name(sel) ? slot_name(sel) : "EMPTY");
    ftext(W - 24 - ftext_w(FT_MB, b), 18, FT_MB, C_WHITE, b);
    if (sv_mode == 0) {                  // P1: the pads as tiles; a playing clip lights its tile, the bright part is its volume
        enum { GX = 26, GY = 80, CW = 232, CH = 196, DX = 247, DY = 210 };
        for (int i = 0; i < NSLOT; i++) { int x = GX + (i % 4) * DX, y = GY + (i / 4) * DY; const Track *t = &song.t[i]; Col ec = C_ENG(t->type);
            if (i == sel) rframe(x - 5, y - 5, CW + 10, CH + 10, 12, 4, C_WHITE, C_BG);
            if (!has_sound(i)) { rframe(x, y, CW, CH, 10, 2, C_LINE, C_BG); ftext(x + CW / 2 - 7, y + CH / 2 - 14, FT_LB, C_LINE, "+"); continue; }
            int on = t->on && engine_playing() && !t->mute, hit = flash[i] && now - flash[i] < 180;
            if (on || hit) { int top = 52 + (CH - 52) * t->vol / 100; rrect(x, y, CW, CH, 10, mix(ec, C_BG, 130)); rrect(x, y, CW, top, 10, ec); if (top < CH - 10) rect(x, y + top - 10, CW, 10, ec, 1); }
            else rrect(x, y, CW, CH, 10, C_CELL);
            snprintf(b, sizeof b, "%.11s", slot_name(i)); ftext(x + 18, y + 16, FT_LB, on || hit ? C_BG : t->mute ? C_DIM : C_WHITE, b);
            if (t->mute) ftext(x + CW - 76, y + CH - 40, FT_M, C_RED, "quiet");
            else if (!on && pat_used(cur_pat(i))) rect(x + 18, y + CH - 22, (CW - 36) * t->vol / 100, 6, mix(ec, C_BG, 90), 1); }
        draw_footer("EDIT PLAY  EDIT+^v VOLUME  Y QUIET  L1/R1 SCENE  MENU SCREEN  F1+F2-HOLD LEAVE");
    } else if (!has_sound(sel)) {
        const char *m = "this pad is empty: F1/F2 goes to the next one"; ftext(512 - ftext_w(FT_M, m) / 2, 340, FT_M, C_DIM, m); draw_footer("F1/F2 PAD  MENU SCREEN");
    } else if (sv_mode == 1) {           // P2: sixteen steps, the notes on them
        const Pattern *p = cur_pat(sel); Col ec = C_ENG(song.t[sel].type); int nsl = track_nslices(sel), cs = cur_step(), page = cs / 16, st = song.t[sel].on ? engine_step(sel) : -1;
        if (p->mode) { const char *m = "this pattern is a piano roll: it plays, the advanced view edits it"; ftext(512 - ftext_w(FT_M, m) / 2, 340, FT_M, C_DIM, m); draw_footer("F1/F2 PAD  MENU SCREEN"); }
        else {
            for (int c = 0; c < 16; c++) { int i = page * 16 + c, x = 28 + (c % 8) * 122, y = 78 + (c / 8) * 288, in = i < p->len, lit = in && p->trig[i];
                Col fill = lit ? mix(C_CELL, ec, 70) : C_CELL;
                if (!in) { rframe(x, y, 112, 274, 10, 1, C_LINE, C_BG); continue; }
                if (i == cs) rframe(x - 3, y - 3, 118, 280, 12, 3, C_WHITE, fill); else rrect(x, y, 112, 274, 10, fill);
                snprintf(b, sizeof b, "%d", i + 1); ftext(x + 10, y + 8, FT_S, C_DIM, b);
                if (lit) { char nb[12]; step_value(p, i, lane, nsl, nb); ftext(x + 56 - ftext_w(FT_LB, nb) / 2, y + 120, FT_LB, ec, nb); } else ftext(x + 50, y + 122, FT_M, C_DIM, "\003");
                if (i == st) rect(x + 6, y + 260, 100, 8, C_WHITE, 1); }
            snprintf(b, sizeof b, "%d steps \003 %s", p->len, div_name[p->div % 6]); ftext(28, 668, FT_M, C_DIM, b);
            if (cs < p->len && p->trig[cs]) { char nb[12]; step_value(p, cs, lane, nsl, nb); snprintf(b, sizeof b, "step %d \003 %s \003 vel %d", cs + 1, nb, p->vel[cs]); } else snprintf(b, sizeof b, "step %d", cs + 1);
            ftext(W - 28 - ftext_w(FT_M, b), 668, FT_M, C_WHITE, b);
            draw_footer("EDIT NOTE  EDIT+^v HIGHER/LOWER  F1/F2 PAD  MENU SCREEN");
        }
    } else {                             // P3: four knobs on the arrows, the dice
        SvKnob kn[4]; sv_knobs(sel, kn); static const char *const ar[4] = {"^", ">", "<", "v"}; static const int ord[4] = {0, 1, 2, 3};
        const char *t = "four knobs on the arrows"; ftext(512 - ftext_w(FT_M, t) / 2, 84, FT_M, C_DIM, t);
        for (int j = 0; j < 4; j++) { int i = ord[j], cx = 152 + j * 240, cy = 270, lo, hi, st, bg, on = i == sv_knob; Col c = j == 0 ? th_cur[TC_SMP] : j == 1 ? C_ENG(1) : j == 2 ? C_PLAY : C_ENG(2);
            par_limits(sel, kn[i].p, &lo, &hi, &st, &bg); float f = hi > lo ? (float)(par_get(sel, kn[i].p) - lo) / (hi - lo) : 0; if (f < 0) f = 0; if (f > 1) f = 1;
            if (on) rframe(cx - 108, cy - 112, 216, 300, 12, 3, C_WHITE, C_BG);
            sv_arc(cx, cy, 74, 0, 1, C_LINE); sv_arc(cx, cy, 74, 0, f, c);
            float a = (float)M_PI * (0.75f + 1.5f * f); line(cx, cy, cx + (int)(cosf(a) * 52), cy + (int)(sinf(a) * 52), 5, c);
            snprintf(b, sizeof b, "%s %s", ar[i], kn[i].nm); ftext(cx - ftext_w(FT_M, b) / 2, cy + 104, FT_M, on ? C_WHITE : C_DIM, b);
            par_str(sel, kn[i].p, b, 20); ftext(cx - ftext_w(FT_MB, b) / 2, cy + 136, FT_MB, C_WHITE, b); }
        rrect(26, 488, 971, 190, 10, C_CELL);
        ftext(512 - ftext_w(FT_LB, "DICE") / 2, 536, FT_LB, ui_lock ? C_DIM : C_ENG(1), "DICE");
        t = ui_lock ? "the kid lock keeps this sound" : "SHIFT+X: a random sound"; ftext(512 - ftext_w(FT_M, t) / 2, 592, FT_M, C_DIM, t);
        draw_footer("EDIT+<> TURN  ARROW KNOB  F1/F2 PAD  SHIFT+X DICE  MENU SCREEN");
    }
    if (sv_exit && held[K_F1] && held[K_F2]) {       // P4: the way out is a hold, with a ring that fills
        int el = (int)ms_since(now, sv_exit);
        if (el >= 3000) { ui_simple = 0; sv_exit = 0; ui_cfg_save(); used[K_F1] = used[K_F2] = 1; set_tab(T_GRID); ui_cat = -1; return; }
        if (el > 250) { ui_shade(215); sv_arc(512, 300, 84, 0, 1, C_LINE); for (float f = 0; f <= el / 3000.f; f += 0.004f) { float a = (float)M_PI * (-0.5f + 2 * f); disc2(512 + (int)(cosf(a) * 84), 300 + (int)(sinf(a) * 84), 7, C_PLAY); }
            snprintf(b, sizeof b, "%d", 3 - el / 1000); ftext(512 - ftext_w(FT_XB, b) / 2, 280, FT_XB, C_WHITE, b);
            snprintf(b, sizeof b, "hold F1+F2 for %d more second%s", 3 - el / 1000, 3 - el / 1000 == 1 ? "" : "s"); ftext(512 - ftext_w(FT_MB, b) / 2, 420, FT_MB, C_WHITE, b);
            const char *t = "to leave the simple view"; ftext(512 - ftext_w(FT_MB, t) / 2, 452, FT_MB, C_WHITE, t);
            if (ui_lock) { t = "kid lock: deleting and overwriting are off"; ftext(512 - ftext_w(FT_S, t) / 2, 500, FT_S, C_DIM, t); } }
    }
}
