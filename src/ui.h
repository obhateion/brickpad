// The UI v2 frame (step 2): the path on top with the page on the right, the pad strip under it, the footer with at
// most three hints, the cheat sheet while SHIFT is held alone and the value popup while EDIT turns a panel control.
// The screens of 1.2 are drawn inside it until steps 3-5 rebuild them. main.c includes this before draw_header.
#pragma once

enum { STRIP_HIDDEN, STRIP_THIN, STRIP_BIG };
static int ui_strip = STRIP_THIN;        // PROJECT > SYSTEM > INTERFACE (step 6)
static char ui_hint[256];                // this frame's whole footer hint: the cheat sheet shows all of it

static int ui_window(void) {             // a window over the screens: no popup, the path names it
    return exporting || naming || browsing || slicing || rolling || randing || picking || presetting || theming || iconing || ducking;
}
static void ui_path(char *b, int n) {    // NEON > INSTRUMENT > 06 GRAND > DEEP
    int l = snprintf(b, n, "%s", prj_cur[0] ? prj_cur : "UNTITLED");
#define UI_ADD(...) (l += snprintf(b + l, l < n ? n - l : 0, " > " __VA_ARGS__))
    if (perform) UI_ADD("%s", perform == 2 ? "KEYS" : "PADS");
    else UI_ADD("%s", tab_name[tab == T_PAT ? T_SEQ : tab]);
    int pad = perform || tab == T_SEQ || tab == T_PAT ? sel : tab == T_FX && fx_unit < NSLOT ? fx_unit : -1;
    if (pad >= 0) UI_ADD("%02d %.14s", pad + 1, slot_name(pad) ? slot_name(pad) : "EMPTY");
    else if (tab == T_FX) UI_ADD("%s", fx_unit == NSLOT ? "RETURN A" : fx_unit == NSLOT + 1 ? "RETURN B" : "MASTER");
    if (exporting) UI_ADD("EXPORT"); else if (naming) UI_ADD("NAME"); else if (browsing) UI_ADD("FILES");
    else if (slicing) UI_ADD("SLICES"); else if (rolling) UI_ADD("ROLL"); else if (randing) UI_ADD("RANDOM");
    else if (picking) UI_ADD("ENGINE"); else if (presetting) UI_ADD("PRESETS"); else if (theming) UI_ADD("THEME");
    else if (iconing) UI_ADD("ICON"); else if (ducking) UI_ADD("DUCK");
    else if (tab == T_PAT) UI_ADD("PATTERNS");
    else if (tab == T_SEQ && srow >= 3 && cl_deep) UI_ADD("%s", song.t[sel].type == 1 && cl_deep == 2 ? "PATCH" : "DEEP");
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
