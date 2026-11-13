// The UI v2 frame (step 2): the path on top with the page on the right, the pad strip under it, the footer with at
// most three hints, the cheat sheet while SHIFT is held alone and the value popup while EDIT turns a panel control.
// The screens of 1.2 are drawn inside it until steps 3-5 rebuild them. main.c includes this before draw_header.
#pragma once

enum { STRIP_HIDDEN, STRIP_THIN, STRIP_BIG };
enum { PC_PATTERN, PC_SOUND, PC_FX, PC_MOD, PC_MIX, PC_N };   // inside a pad (the hub, below)
static const char *const cat_name[PC_N] = {"PATTERN", "SOUND", "FX", "MOD", "MIX"};
static void draw_footer(const char *hint);
static int ui_strip = STRIP_THIN;        // PROJECT > SYSTEM > INTERFACE (step 6)
static char ui_hint[256];                // this frame's whole footer hint: the cheat sheet shows all of it

static int ui_window(void) {             // a window over the screens: no popup, the path names it
    return exporting || naming || browsing || slicing || rolling || randing || picking || presetting || theming || iconing || ducking;
}
static void ui_path(char *b, int n) {    // NEON > INSTRUMENT > 06 GRAND > DEEP
    int l = snprintf(b, n, "%s", prj_cur[0] ? prj_cur : "UNTITLED");
#define UI_ADD(...) (l += snprintf(b + l, l < n ? n - l : 0, " > " __VA_ARGS__))
    int inpad = hubbing || ui_cat >= 0;
    if (perform) UI_ADD("%s", perform == 2 ? "KEYS" : "PADS");
    else if (!inpad) UI_ADD("%s", tab == T_GRID ? "PADS" : tab_name[tab == T_PAT ? T_SEQ : tab]);
    int pad = inpad || perform || tab == T_SEQ || tab == T_PAT ? sel : tab == T_FX && fx_unit < NSLOT ? fx_unit : -1;
    if (pad >= 0) UI_ADD("%02d %.14s", pad + 1, slot_name(pad) ? slot_name(pad) : "EMPTY");
    else if (tab == T_FX) UI_ADD("%s", fx_unit == NSLOT ? "RETURN A" : fx_unit == NSLOT + 1 ? "RETURN B" : "MASTER");
    if (exporting) UI_ADD("EXPORT"); else if (naming) UI_ADD("NAME"); else if (browsing) UI_ADD("FILES");
    else if (slicing) UI_ADD("SLICES"); else if (rolling) UI_ADD("ROLL"); else if (randing) UI_ADD("RANDOM");
    else if (picking) UI_ADD("ENGINE"); else if (presetting) UI_ADD("PRESETS"); else if (theming) UI_ADD("THEME");
    else if (iconing) UI_ADD("ICON"); else if (ducking) UI_ADD("DUCK");
    else if (ui_cat >= 0 && !(tab == T_PAT) && !(ui_cat == PC_SOUND && cl_deep)) UI_ADD("%s", cat_name[ui_cat]);
    else if (tab == T_PAT) UI_ADD("PATTERN > PATTERNS");
    else if (tab == T_SEQ && srow >= 3 && cl_deep) UI_ADD("%s%s", ui_cat == PC_SOUND ? "SOUND > " : "", song.t[sel].type == 1 && cl_deep == 2 ? "PATCH" : "DEEP");
    else if (tab == T_FX && fx_page && fx_unit < NSLOT) UI_ADD("MOD");
#undef UI_ADD
}
static void ui_top(void) {               // the first header row: the path, the page, the pad strip
    char b[160]; ui_path(b, sizeof b);
    int max = 52, len = (int)strlen(b); const char *s = len > max ? b + len - max : b;   // too long: the end counts
    int x = 16;
    for (const char *p = s; *p; ) {      // the last part bright, the separators faint
        const char *e = strstr(p, " > "); int n = e ? (int)(e - p) : (int)strlen(p); char w[64];
        snprintf(w, sizeof w, "%.*s", n, p); text(x, 10, 2, e ? C_DIM : C_WHITE, w); x += n * 16;
        if (!e) break;
        text(x + 8, 10, 2, C_GREY, ">"); x += 32; p = e + 3;
    }
    int scr = tab == T_GRID ? 1 : tab == T_SEQ || tab == T_PAT ? 2 : tab == T_FX ? 3 : tab == T_MIX ? 4 : 5;
    textf(1008 - 3 * 16, 10, 2, C_ORANGE, "%d/5", scr);
    if (ui_strip == STRIP_HIDDEN) return;
    for (int i = 0; i < NSLOT; i++) {    // 12 pads: the engine's colour, how far the clip has played, the selected one
        int px = 16 + i * 83, pw = 79, on = song.t[i].on && engine_playing();
        Col c = has_sound(i) ? C_ENG(song.t[i].type) : C_LINE, dim = mix(c, C_BG, 170);
        if (song.t[i].mute) c = dim = mix(c, C_BG, 200);
        rect(px, 34, pw, 6, has_sound(i) ? dim : C_CELL, 1);
        if (has_sound(i) && on) { int len = cur_pat(i)->len, st = engine_step(i); if (len > 0 && st >= 0) rect(px, 34, pw * (st % len + 1) / len, 6, c, 1); }
        if (i == sel) frame(px - 2, 31, pw + 4, 12, 2, C_WHITE);
    }
}

static int ui_pairs(const char *h) {     // how many "KEY action" pairs a hint has (they are split by two spaces)
    int n = 0; while (*h) { n++; const char *e = strstr(h, "  "); if (!e) break; h = e; while (*h == ' ') h++; }
    return n;
}

// The cheat sheet: every combination of this screen (its whole hint) and the ones that work everywhere.
static const char *const ui_everywhere[] = {"PLAY PLAY / STOP", "F1/F2 SCREENS", "F1+F2 PROJECT", "MENU PADS / KEYS",
    "MENU+PLAY RECORD", "MENU 3 S QUIT", "SHIFT+L2 UNDO", "SHIFT+OPT COPY", "SHIFT+EDIT PASTE", "OPT+EDIT DELETE", 0};
static const char *const ui_more_grid[] = {"Y MUTE", "X ICON", "SHIFT+X NAME", "SHIFT+Y FREEZE", "SHIFT+ARROWS SELECT", "EDIT+<> VOLUME", "OPT LISTEN", 0};
static const char *const ui_more_seq[] = {"L1/R1 LANE", "X+<> PAD", "XX DOUBLE CLIP", "SHIFT+X RANDOM", "SHIFT+<> PAGE", "Y DEEP", "OPT+ARROWS NOTE", 0};
static const char *const ui_more_fx[] = {"L1/R1 UNIT", "X+<> PAD", "Y FX / MOD", 0};
static const char *const ui_more_mix[] = {"Y MUTE", "X SOLO", "L1/R1 GROUP", 0};
static const char *const ui_more_prj[] = {"L1/R1 TEMPO NUDGE", 0};
static void ui_pair(int x, int y, const char *p, int n) {   // a key cap and its action
    int w = (int)strcspn(p, " "); if (w > n) w = n; char b[64];
    snprintf(b, sizeof b, "%.*s", w, p); rect(x - 4, y - 4, w * 16 + 8, 24, C_LINE, 1); text(x, y, 2, C_WHITE, b);
    snprintf(b, sizeof b, "%.*s", n - w, p + w); text(x + w * 16, y, 2, C_DIM, b);
}
static void ui_cheat(void) {
    if (!held[K_SHIFT] || used[K_SHIFT] || !shift_t || ms_since(SDL_GetTicks(), shift_t) < 350) return;
    enum { X0 = 32, Y0 = 92, BW = 960, BH = H - 92 - 84 };
    rect(X0, Y0, BW, BH, C_CELL, 1); frame(X0, Y0, BW, BH, 2, C_ORANGE);
    text(X0 + 20, Y0 + 16, 2, C_ORANGE, "THIS SCREEN"); text(X0 + 500, Y0 + 16, 2, C_ORANGE, "EVERYWHERE");
    int y = Y0 + 52;
    for (const char *h = ui_hint; *h && y < Y0 + BH - 28; y += 32) {
        const char *e = strstr(h, "  "); int n = e ? (int)(e - h) : (int)strlen(h);
        ui_pair(X0 + 24, y, h, n > 28 ? 28 : n);
        if (!e) { y += 32; break; }
        h = e; while (*h == ' ') h++;
    }
    const char *const *more = perform ? NULL : tab == T_GRID ? ui_more_grid : tab == T_SEQ ? ui_more_seq : tab == T_FX ? ui_more_fx : tab == T_MIX ? ui_more_mix : tab == T_PRJ ? ui_more_prj : NULL;
    for (int i = 0; more && more[i] && y < Y0 + BH - 28; i++) if (!strstr(ui_hint, more[i])) { ui_pair(X0 + 24, y, more[i], (int)strlen(more[i])); y += 32; }   // not twice
    y = Y0 + 52;
    for (int i = 0; ui_everywhere[i]; i++, y += 32) ui_pair(X0 + 504, y, ui_everywhere[i], (int)strlen(ui_everywhere[i]));
    text(X0 + 20, Y0 + BH - 24, 1, C_DIM, "LET GO OF SHIFT TO CLOSE");
}

// The value popup: while EDIT+arrows turn a control of the instrument panel, its name, value and where it sits.
static void ui_popup(void) {
    if (!held[K_EDIT] || !used[K_EDIT] || tab != T_SEQ || srow < 3 || perform || ui_window()) return;
    int p = cur_par(); if (p < 0 || p == P_SAMPLE) return;
    char v[96]; par_str(sel, p, v, sizeof v);
    enum { PW = 440, PH = 92, PX = (W - PW) / 2, PY = 92 };
    rect(PX, PY, PW, PH, C_CELL, 1); frame(PX, PY, PW, PH, 2, C_ENG(song.t[sel].type));
    text(PX + 16, PY + 12, 2, C_DIM, par_name[p]);
    textf(PX + 16, PY + 36, 3, C_WHITE, "%.17s", v);
    if (par_is_number(p)) {
        int lo, hi, st, bg; par_limits(sel, p, &lo, &hi, &st, &bg);
        if (hi > lo) { int f = (par_get(sel, p) - lo) * (PW - 32) / (hi - lo); rect(PX + 16, PY + 72, PW - 32, 6, C_LINE, 1); rect(PX + 16, PY + 72, clampi(f, 0, PW - 32), 6, C_ENG(song.t[sel].type), 1); }
    }
}

// The pad hub (UI v2): PATTERN SOUND FX MOD MIX of the selected pad. EDIT opens one, Y goes back up (category -> hub ->
// HOME), F1/F2 the next pad on the same page, SHIFT+arrow jumps: UP PATTERN, RIGHT SOUND, DOWN FX, LEFT MIX.
// Until steps 4-5 rebuild them, the categories open the screens of 1.2 for this pad.
#define C_CAT(c) th_cur[TC_PAT + (c)]
static void hub_open(void) { set_tab(T_GRID); hubbing = 1; }
static void cat_open(int c) {
    set_tab(c == PC_PATTERN || c == PC_SOUND ? T_SEQ : c == PC_MIX ? T_MIX : T_FX);
    hub_cur = c; ui_cat = c;
    if (c == PC_PATTERN && (srow < 1 || srow > 2)) { srow = 1; scol = 0; }
    if (c == PC_SOUND) { srow = 3; scol = 0; cl_deep = 0; gb_cur = 0; }
    if (c == PC_FX || c == PC_MOD) { fx_unit = sel; fx_page = c == PC_MOD; }
    if (c == PC_MIX) mcol = sel;
    seq_clamp();
}
static int in_pad(void) { return hubbing || ui_cat >= 0; }
static void pad_step(int d) {             // F1/F2 inside a pad: the next pad, the same page
    act_select_step(d); seq_clamp();
    if (tab == T_FX && fx_unit < NSLOT) fx_unit = sel;
    if (tab == T_MIX && mcol < NSLOT) mcol = sel;
}
static int jump_cat(int k) { return k == K_UP ? PC_PATTERN : k == K_RIGHT ? PC_SOUND : k == K_DOWN ? PC_FX : PC_MIX; }
static void hub_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) cat_open(hub_cur); return; }
    if (k == K_Y) { hubbing = 0; return; }   // back to HOME
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_OPT) { act_listen(sel); return; }
    if (k == K_UP || k == K_LEFT) hub_cur = (hub_cur + PC_N - 1) % PC_N;
    if (k == K_DOWN || k == K_RIGHT) hub_cur = (hub_cur + 1) % PC_N;
}
static void hub_line(int c, char *b, int n) {   // what is inside, in a few words
    const Track *t = &song.t[sel]; const Pattern *p = cur_pat(sel); const PadMix *pm = &song.mix.pad[sel];
    switch (c) {
    case PC_PATTERN: snprintf(b, n, "P%d  %d STEPS  %s%s", t->cur + 1, p->len, div_name[p->div % 6], p->mode ? "  ROLL" : ""); break;
    case PC_SOUND: snprintf(b, n, "%s  %.20s", type_name[t->type & 3], slot_name(sel) ? slot_name(sel) : "EMPTY"); break;
    case PC_FX: snprintf(b, n, "1 %s  2 %s", pm->ins[0].type ? fx_name[pm->ins[0].type] : "--", pm->ins[1].type ? fx_name[pm->ins[1].type] : "--"); break;
    case PC_MOD: snprintf(b, n, "LFO 1-4  AUTOMATION  DUCK"); break;
    case PC_MIX: snprintf(b, n, "VOL %d  PAN %d  SEND A %d  B %d%s", t->vol, t->pan, pm->send_a, pm->send_b, frozen(sel) ? "  FROZEN" : ""); break;
    }
}
static void draw_hub(void) {
    enum { CX = 120, CWD = 784, CHT = 96, GAP = 14, Y0 = 108 };
    for (int c = 0; c < PC_N; c++) {
        int y = Y0 + c * (CHT + GAP), on = c == hub_cur; Col cc = C_CAT(c); char b[96];
        rect(CX, y, CWD, CHT, on ? C_SELBG : C_CELL, 1);
        rect(CX, y, 8, CHT, cc, 1);
        if (on) frame(CX - 2, y - 2, CWD + 4, CHT + 4, 2, cc);
        text(CX + 32, y + 18, 3, cc, cat_name[c]);
        hub_line(c, b, sizeof b); text(CX + 32, y + 58, 2, on ? C_WHITE : C_DIM, b);
        static const char *const jump[PC_N] = {"SHIFT+UP", "SHIFT+RIGHT", "SHIFT+DOWN", "", "SHIFT+LEFT"};
        text(CX + CWD - 16 - (int)strlen(jump[c]) * 8, y + 24, 1, C_GREY, jump[c]);
    }
    draw_footer("EDIT OPEN  F1/F2 PAD  Y HOME");
}
