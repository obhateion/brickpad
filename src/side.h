// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// Side slots (2026-11-24, user: "what should be at hand is hidden in OPTIONS"): four buttons a side next to the work
// area of a pad's page, the page narrower by the columns (wx0/ww). An arrow that moves nothing at the page's left or
// right edge goes onto the nearest slot that way; ^v walk the four; the arrow back returns to the page where it was
// (its cursor never moved). EDIT presses a slot, EDIT+arrows turn its value. An empty place keeps its column, no frame.
// main.c includes this after chords.h; key_event and draw() call it.
#pragma once

enum { SD_STEPS = 1, SD_ROLL, SD_CHORDS, SD_SOUND, SD_FX, SD_LFO, SD_AUTO };
enum { SD_N = 8, SD_W = 112, SD_GAP = 12, SD_LX = 16, SD_RX = W - 16 - SD_W, SD_Y0 = 127, SD_Y1 = 644 };
typedef struct { const char *name; char v1[16], v2[16]; Col vc; int lamp; Col lc; const char *hint; } SideSlot;

static int side_page(void) {              // the page under the slots: the order of draw()'s chain
    if (exporting || naming || browsing || slicing || randing || prbing || subbing || picking || presetting || theming || iconing || ducking || scening || recpage) return 0;
    if (chording) return SD_CHORDS;
    if (rolling) return SD_ROLL;
    if (v2_steps()) return SD_STEPS;
    if (v2_sound()) return SD_SOUND;
    if (v2_fx()) return SD_FX;
    if (v2_lfo()) return SD_LFO;
    if (v2_auto()) return SD_AUTO;
    return 0;
}
static int side_pat(int pg) { return pg == SD_STEPS || pg == SD_ROLL || pg == SD_CHORDS; }

// ---- what each place holds on this page ----
static int side_fill(int pg, int i, SideSlot *s) {   // 0: the place is empty
    memset(s, 0, sizeof *s); s->vc = C_WHITE;
    const Pattern *p = cur_pat(sel);
    if (side_pat(pg)) switch (i) {
    case 0: s->name = "LAYOUT"; snprintf(s->v1, 16, "%s", p->mode == 1 ? "ROLL" : p->mode == 2 ? "CHORDS" : "STEPS"); s->vc = C_CAT(PC_PATTERN);
        s->hint = "EDIT NEXT  EDIT+^v PICK"; return 1;
    case 1: s->name = "LEN/GRID"; snprintf(s->v1, 16, "%d", p->len); snprintf(s->v2, 16, "%s", div_name[p->div % 6]);
        s->hint = "EDIT+^v LENGTH  EDIT+<> GRID  EDIT FINE"; return 1;
    case 2: { const Track *t = &song.t[sel]; s->name = "PATTERN"; snprintf(s->v1, 16, "P%d", t->cur + 1);
        if (t->queued >= 0) snprintf(s->v2, 16, "\177 P%d", t->queued + 1); else snprintf(s->v2, 16, "%s", eng_nofollow ? "FOL OFF" : "FOLLOW");
        s->hint = "EDIT PATTERNS / FOLLOW"; return 1; }
    case 3:
        if (pg == SD_STEPS) { s->name = "HEAR"; snprintf(s->v1, 16, "%s", hear_on ? "ON" : "OFF"); s->vc = hear_on ? C_ORANGE : C_DIM; s->lamp = 1; s->lc = hear_on ? C_ORANGE : C_LINE;
            s->hint = "EDIT ON/OFF: A NOTE YOU PUT SOUNDS"; return 1; }   // 2026-11-24: SELECT had no step selection behind it
        if (pg == SD_ROLL) { s->name = song.t[sel].type == 2 ? "ACCENT" : "TRIAD"; snprintf(s->v1, 16, "%s", song.t[sel].type == 2 ? "ACC" : "+3"); s->hint = song.t[sel].type == 2 ? "EDIT ACCENT ON THE NOTE" : "EDIT TRIAD ON THE CURSOR"; return 1; }
        s->name = "CHORD LEN"; snprintf(s->v1, 16, "%s", ch_len_name[ch_len]); s->hint = "EDIT NEXT LENGTH"; return 1;
    case 4: { int cnt = rec ? engine_counting() : -1, wait = rec && rec_wait && cnt < 0;
        s->name = "RECORD"; snprintf(s->v1, 16, "%s", !rec ? "OFF" : wait || cnt >= 0 ? "ARMED" : "REC");
        s->vc = rec ? C_RED : C_DIM; s->lamp = 1; s->lc = !rec ? mix(C_RED, C_BG, 150) : wait && SDL_GetTicks() / 400 % 2 ? mix(C_RED, C_BG, 120) : C_RED;
        s->hint = rec ? "EDIT STOP" : "EDIT ARM"; return 1; }
    case 5: s->name = "LISTEN"; snprintf(s->v1, 16, "%s", lst_on ? "ON" : "OFF"); s->vc = lst_on ? C_ORANGE : C_DIM; s->lamp = 1; s->lc = lst_on ? C_ORANGE : C_LINE;
        s->hint = pg == SD_STEPS ? "EDIT ON/OFF: A STEP SOUNDS UNDER THE CURSOR" : "EDIT ON/OFF: THE NOTE UNDER THE CURSOR SOUNDS"; return 1;
    case 6:
        if (pg == SD_ROLL) { s->name = "VEL/CHNC"; snprintf(s->v1, 16, "%s", x_latch ? "ON" : "ARROWS"); s->vc = x_latch ? C_ORANGE : C_WHITE; s->hint = "EDIT ARROWS: ^v VELOCITY <> CHANCE"; return 1; }
        if (pg == SD_CHORDS) { s->name = "SLOT"; snprintf(s->v1, 16, "%d/%d", ch_slot + 1, ch_nslots(cur_pat(sel))); s->hint = "EDIT NEXT SLOT  EDIT+<> SLOT"; return 1; }
        s->name = "DOUBLE"; snprintf(s->v1, 16, "x2"); s->hint = "EDIT DOUBLE THE CLIP"; return 1;
    case 7:
        if (pg == SD_CHORDS) { s->name = "INVERSION"; snprintf(s->v1, 16, "%d", ch_inv); s->hint = "EDIT NEXT INVERSION"; return 1; }
        s->name = "RANDOM"; snprintf(s->v1, 16, "FILL"); s->hint = "EDIT RANDOM"; return 1;
    }
    if (pg == SD_SOUND) switch (i) {
    case 0: s->name = "LISTEN"; snprintf(s->v1, 16, "PLAY"); s->hint = "EDIT LISTEN TO THE PAD"; return 1;
    case 1: s->name = "MIDI LRN"; snprintf(s->v1, 16, "%s", mlearn_par >= 0 ? "ON" : "OFF"); s->vc = mlearn_par >= 0 ? C_ORANGE : C_DIM; s->lamp = 1; s->lc = mlearn_par >= 0 ? C_ORANGE : C_LINE;
        s->hint = "EDIT MIDI LEARN"; return 1;
    }
    if (pg == SD_FX && i == 0) { const FxSlot *f = &song.mix.pad[fx_unit].ins[fx2_slot]; s->name = "INSERT";
        snprintf(s->v1, 16, "%s", !f->type ? "EMPTY" : f->on ? "ON" : "OFF"); s->vc = f->type && f->on ? C_ORANGE : C_DIM; s->lamp = 1; s->lc = f->type && f->on ? C_ORANGE : C_LINE;
        s->hint = "EDIT ON/OFF"; return 1; }
    if (pg == SD_LFO || pg == SD_AUTO) switch (i) {
    case 0: if (pg == SD_LFO) { s->name = "AUTO"; snprintf(s->v1, 16, "A1-A4"); s->hint = "EDIT OPEN AUTOMATION"; }
            else { s->name = "LFO"; snprintf(s->v1, 16, "1-4"); s->hint = "EDIT OPEN LFO"; } return 1;
    case 1: { const PadMix *pm = &song.mix.pad[sel]; s->name = "DUCK";
        if (pm->spare[DK_SRC]) snprintf(s->v1, 16, "PAD %d", pm->spare[DK_SRC]); else snprintf(s->v1, 16, "OFF");
        s->vc = pm->spare[DK_SRC] ? C_WHITE : C_DIM; s->hint = "EDIT OPEN DUCK"; return 1; }
    }
    return 0;
}
static int side_has(int pg, int i) { SideSlot s; return side_fill(pg, i, &s); }
static void side_sync(void);

// ---- the work area: the pages lay themselves out in wx0 .. wx0 + ww ----
static void side_area(void) {
    int pg = side_page(), l = 0, r = 0;
    for (int i = 0; pg && i < 4; i++) { l |= side_has(pg, i); r |= side_has(pg, 4 + i); }
    wx0 = l ? SD_LX + SD_W + SD_GAP : 26; ww = (r ? SD_RX - SD_GAP : 997) - wx0;   // a side with no slot gives its width back
}

// ---- the keys ----
static int side_row(int pg) {             // the place the cursor reaches from the page: its height there, 0..3
    switch (pg) {
    case SD_STEPS: return cur_step() % 16 >= 8 ? 2 : 0;
    case SD_ROLL: return clampi((rl_n0 + RL_ROWS - 1 - rl_n) * 4 / RL_ROWS, 0, 3);
    case SD_CHORDS: return ch_row;
    case SD_SOUND: return sp_cur < 0 ? 0 : 1 + sp_cur / 5;
    }
    return 0;
}
static int side_enter(int k) {            // an arrow that moved nothing at the page's edge: onto a slot that way (1: done)
    int pg = side_page(), base = k == K_RIGHT ? 4 : 0, r = side_row(pg), best = -1;
    if (!pg) return 0;
    for (int d = 0; d < 4 && best < 0; d++) {   // the nearest place with a slot, from the cursor's height
        if (r - d >= 0 && side_has(pg, base + r - d)) best = base + r - d;
        else if (r + d < 4 && side_has(pg, base + r + d)) best = base + r + d;
    }
    if (best < 0) return 0;
    side_foc = best + 1; return 1;
}
static void side_press(int pg, int i) {   // EDIT on a slot
    Pattern *p = cur_pat(sel);
    if (side_pat(pg)) switch (i) {
    case 0: lay_go((p->mode + 1) % 3); return;
    case 1: v_latch = 1; side_foc = 0; toast("ARROWS: LENGTH (^v A BAR)"); return;   // the fine length, as OPTION's LENGTH was
    case 2: side_foc = 0; cx_patterns(); return;
    case 3: if (pg == SD_STEPS) { hear_on = !hear_on; toast(hear_on ? "HEAR NOTES ON" : "HEAR NOTES OFF"); }
            else if (pg == SD_ROLL) sim_key(K_EDIT, K_X);
            else sim_key(K_X, -1);
            return;
    case 4: rec_toggle(engine_posf(), SDL_GetTicks() | 1); return;
    case 5: lst_on = !lst_on; ui_cfg_save(); toast(lst_on ? "LISTEN ON" : "LISTEN OFF"); return;
    case 6: if (pg == SD_CHORDS) { ch_slot = (ch_slot + 1) % ch_nslots(p); return; }
            if (pg == SD_ROLL) { side_foc = 0; x_latch = 1; held[K_X] = used[K_X] = 1; toast("ARROWS: ^v VELOCITY <> CHANCE"); } else { clip_double(); toast_length("LENGTH", cur_pat(sel)->len); } return;
    case 7: if (pg == SD_CHORDS) { ch_inv = (ch_inv + 1) % 3; ch_hear(ch_row, ch_col); return; }
            side_foc = 0; sim_key(K_X, K_SHIFT); return;   // RANDOM's window (STEPS and the roll)
    }
    if (pg == SD_SOUND) { if (i == 0) listen(sel); else { side_foc = 0; sim_key(K_R2, -1); } return; }
    if (pg == SD_FX) { sim_key(K_X, -1); return; }
    if (pg == SD_LFO || pg == SD_AUTO) { side_foc = 0; if (i == 1) cx_duck(); else if (pg == SD_LFO) cx_auto(); else cx_lfo(); }
}
static void side_turn(int pg, int i, int k) {   // EDIT+arrows on a slot
    Pattern *p = cur_pat(sel); int c = song.t[sel].cur;
    if (!side_pat(pg)) return;
    if (i == 0 && (k == K_UP || k == K_DOWN)) lay_go((p->mode + (k == K_DOWN ? 1 : 2)) % 3);
    else if (i == 6 && pg == SD_CHORDS && (k == K_LEFT || k == K_RIGHT)) { int n = ch_nslots(p); ch_slot = (ch_slot + (k == K_RIGHT ? 1 : n - 1)) % n; }
    else if (i == 1 && pg == SD_ROLL && (k == K_LEFT || k == K_RIGHT)) sim_key(k == K_RIGHT ? K_R1 : K_L1, -1);   // the roll's own grid (snap)
    else if (i == 1) {
        if (k == K_UP || k == K_DOWN) { act_pattern_len_step(sel, c, k == K_UP ? 16 : -16); toast_length("LENGTH", cur_pat(sel)->len); }
        else { act_pattern_div_step(sel, c, k == K_RIGHT ? 1 : -1); char b[32]; snprintf(b, sizeof b, "GRID %s", div_name[cur_pat(sel)->div % 6]); toast(b); }
        seq_clamp();
    }
}
static int side_key(int k, int down) {    // 1: the key was the slot's
    side_sync(); int pg = side_page();
    if (!side_foc) return 0;
    if (!pg || ui_over() || ctxing || cfm || asking) { if (!pg) side_foc = 0; return 0; }
    int i = side_foc - 1, right = i >= 4;
    if (qb_focus || strip_focus || tab_focus) { side_foc = 0; return 0; }
    if (k == K_PLAY || k == K_SHIFT || k == K_MENU || k == K_F1 || k == K_F2 || k == K_L1 || k == K_R1 || k == K_L2 || k == K_R2 || k == K_X || k == K_OPT) return 0;
    if (k <= K_RIGHT && (held[K_MENU] || held[K_SHIFT])) return 0;   // MENU+arrows (the map), SHIFT+<> (the next pad) stay theirs
    if (k == K_Y) { if (down) side_foc = 0; return 1; }   // X and Y never go through input_key (key_event1): nothing to hold
    input_key(k, down); input_combo(k, down);   // the slot takes the key: held[] and used[] as key_event1 keeps them
    if (!down) {
        if (k == K_EDIT && !used[K_EDIT] && !held[K_OPT]) side_press(pg, i);   // OPT+EDIT (delete / default) presses nothing
        return k == K_EDIT || k <= K_RIGHT;
    }
    if (k > K_RIGHT) return k == K_EDIT;
    if (held[K_EDIT]) { used[K_EDIT] = 1; side_turn(pg, i, k); return 1; }
    if (k == (right ? K_LEFT : K_RIGHT)) { side_foc = 0; return 1; }   // back to the page, where it was
    if (k == K_UP || k == K_DOWN) {           // the next slot this side; past the end nothing
        int d = k == K_DOWN ? 1 : -1, j = i % 4 + d;
        while (j >= 0 && j < 4 && !side_has(pg, (right ? 4 : 0) + j)) j += d;
        if (j >= 0 && j < 4) side_foc = (right ? 4 : 0) + j + 1;
    }
    return 1;
}
// LISTEN STEP: after a key, the cursor landed somewhere new
static int side_land_mark(void) {
    switch (side_page()) {
    case SD_STEPS: return 1 + cur_step() + 1000 * sel;
    case SD_ROLL: return 1 + rl_t * 128 + rl_n;
    }
    return 0;
}
static void side_land(int before) {
    int pg = side_page(), now = side_land_mark();
    if (!lst_on || side_foc || !now || now == before || (engine_playing() && song.t[sel].on)) return;
    Pattern *p = cur_pat(sel);
    if (pg == SD_STEPS) { int st = cur_step(); if (st < p->len && p->trig[st]) step_audition(st); }
    else if (pg == SD_ROLL) { int i = roll_find(p, rl_t, rl_n); if (i >= 0) { engine_trig_chain(sel, p->roll[i].vel / 127.f, (float)note_semis(sel, p->roll[i].note), -1); flash[sel] = SDL_GetTicks(); } }
}

static void side_sync(void) {             // another kind of page (not another layout of PATTERN): the cursor is on the page
    static int last; int pg = side_page();
    if (pg != last && !(side_pat(pg) && side_pat(last))) side_foc = 0;
    last = pg;
}

// ---- the picture ----
static void side_draw(void) {
    side_sync();
    int pg = side_page(); if (!pg) { side_foc = 0; return; }
    if (side_foc && !side_has(pg, side_foc - 1)) side_foc = 0;
    int sh = (SD_Y1 - SD_Y0 - 3 * SD_GAP) / 4;
    for (int i = 0; i < SD_N; i++) {
        SideSlot s; if (!side_fill(pg, i, &s)) continue;
        int x = i < 4 ? SD_LX : SD_RX, y = SD_Y0 + (i % 4) * (sh + SD_GAP), on = side_foc == i + 1;
        if (on) rframe(x - 4, y - 4, SD_W + 8, sh + 8, 12, 3, C_WHITE, C_BG);
        rrect(x, y, SD_W, sh, 10, C_CELL);
        ftext(x + 12, y + 10, FT_S, on ? C_WHITE : C_DIM, s.name);
        if (s.lamp) disc2(x + SD_W - 16, y + 20, 6, s.lc);
        if (s.v2[0]) { ftext(x + SD_W / 2 - ftext_w(FT_MB, s.v1) / 2, y + 42, FT_MB, s.vc, s.v1); ftext(x + SD_W / 2 - ftext_w(FT_MB, s.v2) / 2, y + 74, FT_MB, s.vc, s.v2); }
        else { int f = ftext_w(FT_LB, s.v1) <= SD_W - 12 ? FT_LB : FT_MB; ftext(x + SD_W / 2 - ftext_w(f, s.v1) / 2, y + 56, f, s.vc, s.v1); }
    }
}
static const char *side_hint(void) {      // the footer while the cursor is on a slot
    static char b[96]; SideSlot s; int pg = side_page();
    if (!side_foc || !pg || !side_fill(pg, side_foc - 1, &s)) return NULL;
    const char *e = strstr(s.hint, "  ");   // the slot's first key, the way back, then the rest
    if (e) snprintf(b, sizeof b, "%.*s  %s BACK%s", (int)(e - s.hint), s.hint, side_foc > 4 ? "<" : ">", e);
    else snprintf(b, sizeof b, "%s  %s BACK", s.hint, side_foc > 4 ? "<" : ">");
    return b;
}
