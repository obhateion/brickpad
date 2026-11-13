// The UI v2 frame (step 2): the path on top with the page on the right, the pad strip under it, the footer with at
// most three hints, the cheat sheet while SHIFT is held alone and the value popup while EDIT turns a panel control.
// The screens of 1.2 are drawn inside it until steps 3-5 rebuild them. main.c includes this before draw_header.
#pragma once

enum { STRIP_HIDDEN, STRIP_THIN, STRIP_BIG };
static const char *const cat_name[PC_N] = {"PATTERN", "SOUND", "FX", "MOD", "MIX"};
static void draw_footer(const char *hint);
static int ui_strip = STRIP_THIN;        // PROJECT > SYSTEM > INTERFACE (step 6)
static char ui_hint[256];                // this frame's whole footer hint: the cheat sheet shows all of it
static int ui_v2;                        // this frame draws a rebuilt screen: the mockups' header and footer (draw())

// ---- the kit of the mockups: text in their sizes, rounded cards, the header and the footer ----
enum { V2_HEAD = 60, V2_FOOT = 718 };    // the header ends, the footer starts
static int ftext(int x, int y, int f, Col c, const char *s) {   // y: the top of the line; returns the width
    const FontV2 *v = &fv2[f]; SDL_Texture *t = font_v2[f]; int x0 = x;
    SDL_SetTextureColorMod(t, c.r, c.g, c.b);
    for (; *s; s++, x += v->w) {
        int ch = (unsigned char)*s, g = ch >= 32 && ch <= 127 ? ch - 32 : ch == 1 ? 96 : ch == 2 ? 97 : -1;
        if (ch == 3) g = 98;             // the middle dot
        if (ch == 4) g = 99;             // the arrow
        if (g <= 0) continue;
        SDL_Rect src = {g * v->w, 0, v->w, v->h}, dst = {x, y, v->w, v->h}; SDL_RenderCopy(R, t, &src, &dst);
    }
    return x - x0;
}
static int ftext_w(int f, const char *s) { return (int)strlen(s) * fv2[f].w; }
static void rrect(int x, int y, int w, int h, int r, Col c) {   // a filled rounded box
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255);
    for (int i = 0; i < h; i++) {
        int d = i < r ? r - i : i >= h - r ? i - (h - r - 1) : 0, in = 0;
        if (d) { float e = r - sqrtf((float)(r * r - (d - 0.5f) * (d - 0.5f))); in = (int)(e + 0.5f); }
        SDL_RenderDrawLine(R, x + in, y + i, x + w - 1 - in, y + i);
    }
}
static void rframe(int x, int y, int w, int h, int r, int t, Col c, Col inside) {   // a rounded outline t thick
    rrect(x, y, w, h, r, c); rrect(x + t, y + t, w - 2 * t, h - 2 * t, r > t ? r - t : 1, inside);
}
static void ui_path(char *b, int n); static const char *snd_page_name(void); static void snd_page_count(char *b, int n);   // sound.h
static void ui2_header(void) {           // NEON > PADS on the left, the transport and the tempo on the right
    rect(0, 0, W, V2_HEAD, C_CELL, 1);
    char b[160]; ui_path(b, sizeof b);
    int x = 20, y = (V2_HEAD - fv2[FT_M].h) / 2;
    for (const char *p = b; *p; ) {
        const char *e = strstr(p, " > "); int n = e ? (int)(e - p) : (int)strlen(p); char w[64];
        snprintf(w, sizeof w, "%.*s", n, p);
        x += ftext(x, y, e ? FT_M : FT_MB, e ? C_DIM : C_WHITE, w);
        if (!e) break;
        x += ftext(x, y, FT_M, C_DIM, " \177 "); p = e + 3;
    }
    char t[16];
    if (rec) {                           // a take: the count, waiting for the first note, or how long it runs (A04: REC)
        if (engine_counting() >= 0) snprintf(t, sizeof t, "REC %d", engine_counting());
        else if (rec_wait) snprintf(t, sizeof t, "REC WAIT");
        else { int q = (int)((engine_posf() - rec_from) * 10 / bar32()); snprintf(t, sizeof t, "REC %d.%d", q / 10, q % 10); }
        ftext(W - 20 - ftext_w(FT_MB, t), y, FT_MB, C_RED, t); return;
    }
    if (tab == T_PAT && !perform) return;   // A07: nothing on the right
    if (browsing || presetting || slicing) return;   // A11, A12: nothing on the right
    if (v2_sound()) { snd_page_count(t, sizeof t); ftext(W - 20 - ftext_w(FT_MB, t), y, FT_MB, C_ENG(song.t[sel].type), t); return; }   // A08: 1/5
    if (v2_steps() || rolling) { snprintf(t, sizeof t, "P%02d", song.t[sel].cur + 1); ftext(W - 20 - ftext_w(FT_MB, t), y, FT_MB, C_ORANGE, t); return; }   // A05: the pattern
    snprintf(t, sizeof t, "%c %d", engine_playing() ? 1 : 2, (int)(song.tempo * tempo_mul + 0.5f));
    ftext(W - 20 - ftext_w(FT_MB, t), y, FT_MB, engine_playing() ? th_cur[TC_SMP] : C_DIM, t);
}
static void ui2_footer(const char *h) {  // at most three key chips: the key in the accent, what it does after it
    rect(0, V2_FOOT, W, H - V2_FOOT, C_CELL, 1);
    int x = 18, y = V2_FOOT + 10, left = 3;
    if (msg_t && SDL_GetTicks() - msg_t < 1500) { int w = ftext_w(FT_MB, msg) + 24; rrect(x, y, w, 32, 6, C_ORANGE); ftext(x + 12, y + 4, FT_MB, C_BG, msg); return; }
    while (*h && left--) {
        const char *e = strstr(h, "  "); int n = e ? (int)(e - h) : (int)strlen(h), k = (int)strcspn(h, " "); char b[64];
        if (k > n) k = n;
        snprintf(b, sizeof b, "%.*s", k, h); int kw = ftext_w(FT_MB, b) + 24;
        rrect(x, y, kw, 32, 6, C_SELBG); ftext(x + 12, y + 4, FT_MB, C_ORANGE, b); x += kw + 10;
        snprintf(b, sizeof b, "%.*s", n - k > 0 ? n - k - 1 : 0, h + k + (n > k));
        for (char *q = b; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32;   // the mockups write what a key does in small letters
        x += ftext(x, y + 4, FT_M, mix(C_WHITE, C_DIM, 70), b) + 36;
        if (!e) break;
        h = e; while (*h == ' ') h++;
    }
}

static int ui_window(void) {             // a window over the screens: no popup, the path names it
    return exporting || naming || browsing || slicing || rolling || randing || picking || presetting || theming || iconing || ducking;
}
static void ui_path(char *b, int n) {    // NEON > INSTRUMENT > 06 GRAND > DEEP
    int l = snprintf(b, n, "%s", prj_cur[0] ? prj_cur : "UNTITLED");
#define UI_ADD(...) (l += snprintf(b + l, l < n ? n - l : 0, " > " __VA_ARGS__))
    if (picking) { snprintf(b + l, l < n ? n - l : 0, " > %02d -", sel + 1); return; }   // the empty pad (A03)
    int inpad = hubbing || ui_cat >= 0;
    if (perform) UI_ADD("%s", perform == 2 ? "KEYS" : "PADS");
    else if (!inpad) UI_ADD("%s", tab == T_GRID ? "PADS" : tab_name[tab == T_PAT ? T_SEQ : tab]);
    int pad = inpad || perform || tab == T_SEQ || tab == T_PAT ? sel : tab == T_FX && fx_unit < NSLOT ? fx_unit : -1;
    if (pad >= 0) UI_ADD("%02d %.14s", pad + 1, slot_name(pad) ? slot_name(pad) : "EMPTY");
    else if (tab == T_FX) UI_ADD("%s", fx_unit == NSLOT ? "RETURN A" : fx_unit == NSLOT + 1 ? "RETURN B" : "MASTER");
    if (exporting) UI_ADD("EXPORT"); else if (naming) UI_ADD("NAME"); else if (browsing) UI_ADD("%s", bmode ? "FILES" : "SOUND > FILE");
    else if (slicing) UI_ADD("SOUND > SLICES > EDIT"); else if (rolling) UI_ADD("PATTERN > ROLL"); else if (randing) UI_ADD("RANDOM");
    else if (picking) UI_ADD("ENGINE"); else if (presetting) UI_ADD("SOUND > PRESETS"); else if (theming) UI_ADD("THEME");
    else if (iconing) UI_ADD("ICON"); else if (ducking) UI_ADD("DUCK");
    else if (recpage) UI_ADD("PATTERN > RECORD");
    else if (v2_sound()) UI_ADD("SOUND > %s", snd_page_name());
    else if (v2_steps()) UI_ADD("PATTERN > STEPS");
    else if (ui_cat >= 0 && !(tab == T_PAT) && !(ui_cat == PC_SOUND && cl_deep)) UI_ADD("%s", cat_name[ui_cat]);
    else if (tab == T_PAT) UI_ADD("PATTERN > SELECT");
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
    if (!held[K_EDIT] || !used[K_EDIT] || tab != T_SEQ || srow < 3 || perform || ui_window() || ui_v2) return;   // the cards show their values
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
static void patmenu_open(void) { set_tab(T_SEQ); ui_cat = hub_cur = PC_PATTERN; patmenu = 1; }
static void cat_open(int c) {
    set_tab(c == PC_PATTERN || c == PC_SOUND ? T_SEQ : c == PC_MIX ? T_MIX : T_FX);
    hub_cur = c; ui_cat = c;
    if (c == PC_PATTERN && (srow < 1 || srow > 2)) { srow = 1; scol = 0; }
    if (c == PC_PATTERN && pm_cur == 1) pat_open();   // the page it was left on
    else if (c == PC_PATTERN && pm_cur == 2) recpage = 1;
    else if (c == PC_PATTERN && cur_pat(sel)->mode) roll_open();
    if (c == PC_SOUND) { srow = 3; scol = 0; cl_deep = 0; gb_cur = 0; }
    if (c == PC_FX || c == PC_MOD) { fx_unit = sel; fx_page = c == PC_MOD; }
    if (c == PC_MIX) mcol = sel;
    seq_clamp();
}
static int in_pad(void) { return hubbing || ui_cat >= 0; }
static void pat_sync(void) {              // another pad on PATTERN's first page: its steps or its roll
    if (ui_cat != PC_PATTERN || tab != T_SEQ || patmenu || recpage) return;
    int m = cur_pat(sel)->mode;
    if (m) roll_open(); else if (rolling) roll_leave();
}
static void pad_step(int d) {             // F1/F2 inside a pad: the next pad, the same page
    act_select_step(d); seq_clamp(); pat_sync();
    if (tab == T_FX && fx_unit < NSLOT) fx_unit = sel;
    if (tab == T_MIX && mcol < NSLOT) mcol = sel;
}
static int jump_cat(int k) { return k == K_UP ? PC_PATTERN : k == K_RIGHT ? PC_SOUND : k == K_DOWN ? PC_FX : PC_MIX; }
static void hub_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) { if (hub_cur == PC_PATTERN) patmenu_open(); else cat_open(hub_cur); } return; }
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
static void draw_hub(void) {              // the cards of A03: stripe, name in the category's colour, what is inside
    enum { CX = 122, CWD = 778, CHT = 104, DY = 122, Y0 = 82 };
    static const char *const jump[PC_N] = {"SHIFT+UP", "SHIFT+RIGHT", "SHIFT+DOWN", "", "SHIFT+LEFT"};
    for (int c = 0; c < PC_N; c++) {
        int y = Y0 + c * DY, on = c == hub_cur; Col cc = C_CAT(c); char b[96];
        rrect(CX, y, CWD, CHT, 8, on ? mix(C_SELBG, C_DIM, 40) : C_CELL);
        rect(CX, y, 9, CHT, cc, 1);
        ftext(CX + 38, y + 12, FT_LB, cc, cat_name[c]);
        ftext(CX + CWD - 26 - ftext_w(FT_S, jump[c]), y + 16, FT_S, C_DIM, jump[c]);
        hub_line(c, b, sizeof b); ftext(CX + 38, y + 56, FT_M, on ? C_WHITE : C_DIM, b);
    }
    draw_footer("EDIT OPEN  F1/F2 PAD  Y BACK");
}

// PATTERN (UI v2): its pages as a list, STEPS (the roll when the pattern is one) and SELECT; RECORD comes with A04.
static void patmenu_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) { patmenu = 0;
        if (pm_cur == 1) pat_open(); else if (pm_cur == 2) recpage = 1; else if (cur_pat(sel)->mode) roll_open(); else { srow = 1; scol = 0; seq_clamp(); } } return; }
    if (k == K_Y) { hub_open(); hub_cur = PC_PATTERN; return; }
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_OPT) { act_listen(sel); return; }
    if (k == K_UP || k == K_LEFT) pm_cur = (pm_cur + 2) % 3;
    if (k == K_DOWN || k == K_RIGHT) pm_cur = (pm_cur + 1) % 3;
}
static void draw_patmenu(void) {
    enum { CX = 122, CWD = 778, CHT = 104, DY = 122, Y0 = 82 };
    const Track *t = &song.t[sel]; const Pattern *p = cur_pat(sel); int used = 0; for (int q = 0; q < NPAT; q++) used += pat_used(&t->pat[q]);
    for (int c = 0; c < 3; c++) {
        int y = Y0 + c * DY, on = c == pm_cur; Col cc = C_CAT(PC_PATTERN); char b[96];
        rrect(CX, y, CWD, CHT, 8, on ? mix(C_SELBG, C_DIM, 40) : C_CELL);
        rect(CX, y, 9, CHT, cc, 1);
        ftext(CX + 38, y + 12, FT_LB, cc, c == 2 ? "RECORD" : c ? "SELECT" : p->mode ? "ROLL" : "STEPS");
        if (c == 2) snprintf(b, sizeof b, "count-in %d \3 quantize %s \3 play it on KEYS or PADS (MENU)", song.count_in, quant_name[song.rec_quant % 5]);
        else if (c) snprintf(b, sizeof b, "P%02d of %d used \3 length \3 division \3 what plays after", t->cur + 1, used);
        else snprintf(b, sizeof b, "%d steps \3 %s", p->len, div_name[p->div % 6]);
        ftext(CX + 38, y + 56, FT_M, on ? C_WHITE : C_DIM, b);
    }
    draw_footer("EDIT OPEN  F1/F2 PAD  Y BACK");
}

// PATTERN > STEPS (mockup A05): the lanes as tabs (L1/R1), sixteen steps in two rows, the pages follow the cursor.
static void slice_label(char *b, const Pattern *p, int i, int ns);   // main.c, below
static const char *lane_full(int ln) {
    switch (ln) { case L_TRIG: return song.t[sel].type == 0 ? "PITCH" : "NOTE"; case L_VEL: return "VEL"; case L_PROB: return "CHANCE";
                  case L_REP: return "REPEAT"; case L_SLICE: return "SLICE"; case L_ACC: return "ACC"; case L_SLD: return "SLIDE"; }
    return lane_name[ln];
}
static void step_value(const Pattern *p, int i, int ln, int nsl, char *b) {   // what a lit step shows on this lane
    switch (ln) {
    case L_TRIG: if (nsl) slice_label(b, p, i, nsl); else note_str((uint8_t)(p->note[i] == NOTE_NONE ? clampi(roll_base(sel), 0, 127) : p->note[i]), b); break;
    case L_SLICE: slice_label(b, p, i, nsl); break;
    case L_VEL: snprintf(b, 8, "%d", p->vel[i]); break;
    case L_PROB: snprintf(b, 8, "%d%%", p->prob[i]); break;
    case L_REP: snprintf(b, 8, "x%d", p->rep[i]); break;
    case L_ACC: case L_SLD: snprintf(b, 8, "%s", p->slice[i] >= 0 && (p->slice[i] & (ln == L_ACC ? ACID_ACCENT : ACID_SLIDE)) ? (ln == L_ACC ? "ACC" : "SLD") : "-"); break;
    default: b[0] = 0;
    }
}
static void draw_steps2(void) {
    const Pattern *p = cur_pat(sel); Col ec = C_ENG(song.t[sel].type); int nsl = track_nslices(sel);
    int l[L_N], n = lane_list(l), nt = 0;
    for (int i = 0; i < n; i++) if (!is_auto(l[i])) {   // the tabs; A1-A4 follow them on L1/R1 (their own page comes with MOD)
        int cx = 90 + nt++ * 138, on = l[i] == lane; const char *nm = lane_full(l[i]);
        if (on) rrect(cx - 65, 74, 130, 38, 8, ec);
        ftext(cx - ftext_w(FT_MB, nm) / 2, 81, FT_MB, on ? C_BG : C_DIM, nm);
    }
    int cs = cur_step(), page = cs / 16, play = song.t[sel].on ? engine_step(sel) : -1; char b[64];
    for (int v = 0; v < 16; v++) {
        int i = page * 16 + v, x = 29 + (v % 8) * 243 / 2, y = 127 + (v / 8) * 265, on = i < p->len && p->trig[i];
        if (i >= p->len) { rframe(x, y, 108, 252, 8, 2, C_CELL, C_BG); continue; }   // past the end of the pattern
        if (i == cs) rframe(x - 4, y - 4, 116, 260, 11, 3, C_WHITE, C_BG);
        int flag = on && (lane == L_ACC || lane == L_SLD) && p->slice[i] >= 0 && (p->slice[i] & (lane == L_ACC ? ACID_ACCENT : ACID_SLIDE));
        rrect(x, y, 108, 252, 8, flag ? mix(C_BG, ec, 130) : on ? mix(C_BG, ec, 46) : C_CELL);
        snprintf(b, sizeof b, "%d", i + 1); ftext(x + 10, y + 6, FT_S, C_DIM, b);
        if (on) step_value(p, i, lane, nsl, b); else snprintf(b, sizeof b, "\3");
        ftext(x + 54 - ftext_w(FT_LB, b) / 2, y + 126 - fv2[FT_LB].h / 2, FT_LB, on ? ec : C_DIM, b);
        if (i == play) rect(x, y + 244, 108, 8, C_WHITE, 1);   // the step that plays
    }
    int pages = (p->len + 15) / 16;
    if (pages > 1) snprintf(b, sizeof b, "%d steps \3 %s \3 page %d/%d", p->len, div_name[p->div % 6], page + 1, pages);
    else snprintf(b, sizeof b, "%d steps \3 %s", p->len, div_name[p->div % 6]);
    ftext(26, 660, FT_M, C_DIM, b);
    if (cs < p->len && p->trig[cs]) { char nb[12]; step_value(p, cs, L_TRIG, nsl, nb); snprintf(b, sizeof b, "step %d \3 %s \3 vel %d", cs + 1, nb, p->vel[cs]); }
    else snprintf(b, sizeof b, "step %d", cs + 1);
    ftext(W - 28 - ftext_w(FT_M, b), 660, FT_M, C_WHITE, b);
    draw_footer(lane == L_TRIG || lane == L_SLICE ? "EDIT NOTE  L1/R1 LANE  Y BACK  EDIT+ARROWS VALUE  OPT LISTEN  OPT+EDIT DELETE  XX DOUBLE CLIP  SHIFT+X RANDOM"
                                                  : "EDIT+ARROWS VALUE  L1/R1 LANE  Y BACK  OPT LISTEN  OPT+EDIT DEFAULT  XX DOUBLE CLIP  SHIFT+X RANDOM");
}

// PATTERN > RECORD (UI v2, mockup A04): the take's state as one big circle. The logic is the one of MENU+PLAY: EDIT arms
// (or ends the take), the count-in runs, the first note starts it; notes come from KEYS or PADS (MENU) or OPTION here.
static void fdig(int cx, int cy, Col c, const char *s) {   // big digits, centred on cx, the figures' middle on cy
    int n = (int)strlen(s), x = cx - n * FD_W / 2, y = cy - FD_TOP - FD_CAP / 2;
    SDL_SetTextureColorMod(font_dig, c.r, c.g, c.b);
    for (; *s; s++, x += FD_W) { int g = *s == '.' ? 10 : *s >= '0' && *s <= '9' ? *s - '0' : -1; if (g < 0) continue;
        SDL_Rect src = {g * FD_W, 0, FD_W, FD_H}, dst = {x, y, FD_W, FD_H}; SDL_RenderCopy(R, font_dig, &src, &dst); }
}
static void disc2(int cx, int cy, int r, Col c) {
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255);
    for (int dy = -r; dy <= r; dy++) { int dx = (int)(sqrtf((float)(r * r - dy * dy)) + 0.5f); SDL_RenderDrawLine(R, cx - dx, cy + dy, cx + dx, cy + dy); }
}
static void record_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) rec_toggle(engine_posf(), SDL_GetTicks() | 1); return; }
    if (k == K_Y) {                      // the count or the wait: cancel; else back to the list (a running take goes on)
        if (rec && (engine_counting() >= 0 || rec_wait)) rec_toggle(engine_posf(), SDL_GetTicks() | 1);
        else { recpage = 0; patmenu_open(); }
        return;
    }
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_OPT) { listen(sel); rec_hit(sel, -1, NOTE_NONE); return; }   // a hit of the pad: it starts an armed take
}
static void draw_record(void) {
    enum { CX = W / 2, CY = 300, RR = 169 };
    int cnt = rec ? engine_counting() : -1, wait = rec && rec_wait && cnt < 0, run = rec && !wait && cnt < 0; char b[96], v[16];
    const char *line;
    if (!rec) { disc2(CX, CY, RR, mix(C_RED, C_BG, 200)); disc2(CX, CY, RR - 6, C_BG); ftext(CX - ftext_w(FT_XB, "REC") / 2, CY - 18, FT_XB, mix(C_RED, C_BG, 90), "REC"); line = "ready to record"; }
    else if (wait) { int on = SDL_GetTicks() / 400 % 2; disc2(CX, CY, RR, C_RED); disc2(CX, CY, RR - 6, on ? mix(C_RED, C_BG, 190) : C_BG); ftext(CX - ftext_w(FT_XB, "PLAY") / 2, CY - 18, FT_XB, C_RED, "PLAY"); line = "play a note: the take starts with it"; }
    else if (cnt >= 0) { disc2(CX, CY, RR, C_RED); snprintf(v, sizeof v, "%d", cnt > 0 ? cnt : 1); fdig(CX, CY, C_BG, v); line = "count-in before recording"; }
    else { disc2(CX, CY, RR, C_RED); int q = (int)((engine_posf() - rec_from) * 10 / bar32()); snprintf(v, sizeof v, "%d.%d", q / 10, q % 10); fdig(CX, CY, C_BG, v); line = "recording: bars"; }
    ftext(CX - ftext_w(FT_LB, line) / 2, 524, FT_LB, C_WHITE, line);
    snprintf(b, sizeof b, "into %02d %.14s \3 P%02d \3 quantize %s", sel + 1, slot_name(sel) ? slot_name(sel) : "EMPTY", song.t[sel].cur + 1, quant_name[song.rec_quant % 5]);
    ftext(CX - ftext_w(FT_M, b) / 2, 566, FT_M, C_DIM, b);
    draw_footer(run ? "EDIT STOP  OPT HIT  Y BACK  MENU KEYS / PADS  PLAY TRANSPORT" : rec ? "PLAY STOP  Y CANCEL  OPT HIT  MENU KEYS / PADS"
                    : "EDIT ARM  OPT HIT  Y BACK  MENU KEYS / PADS  MENU+PLAY ARM FROM ANYWHERE");
}
