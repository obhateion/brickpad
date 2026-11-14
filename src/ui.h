// The UI v2 frame (step 2): the path on top with the page on the right, the pad strip under it, the footer with at
// most three hints, the cheat sheet while SHIFT is held alone and the value popup while EDIT turns a panel control.
// The screens of 1.2 are drawn inside it until steps 3-5 rebuild them. main.c includes this before draw_header.
#pragma once

enum { STRIP_HIDDEN, STRIP_THIN, STRIP_BIG };
#define C_CAT(c) th_cur[TC_PAT + (c)]
static const char *const cat_name[PC_N] = {"PATTERN", "SOUND", "FX", "MOD", "MIX"};
static void draw_footer(const char *hint);
static char ui_hint[256];                // this frame's whole footer hint: the cheat sheet shows all of it

// ---- the kit of the mockups: text in their sizes, rounded cards, the header and the footer ----
enum { V2_HEAD = 60, V2_FOOT = 718 };    // the header ends, the footer starts
static int ftext(int x, int y, int f, Col c, const char *s) {   // y: the top of the line; returns the width
    const FontV2 *v = &fv2[f]; SDL_Texture *t = font_v2[f]; int x0 = x;
    SDL_SetTextureColorMod(t, c.r, c.g, c.b);
    for (; *s; s++, x += v->w) {
        int ch = (unsigned char)*s, g = ch >= 32 && ch <= 127 ? ch - 32 : ch == 1 ? 96 : ch == 2 ? 97 : -1;
        if (ch == 3) g = 98;             // the middle dot
        if (ch == 4) g = 99;             // the arrow
        if (ch == 5) g = 100;            // the arrow to the left
        if (g <= 0) continue;
        SDL_Rect src = {g * v->w, 0, v->w, v->h}, dst = {x, y, v->w, v->h}; SDL_RenderCopy(R, t, &src, &dst);
    }
    return x - x0;
}
static int ftext_w(int f, const char *s) { return (int)strlen(s) * fv2[f].w; }
static void ftext_wrap(int x, int y, int w, int lines, int f, Col c, const char *s) {   // words over up to `lines` lines of width w
    int per = w / fv2[f].w; char b[160];
    for (int l = 0; l < lines && *s; l++) { int n = (int)strlen(s);
        if (n > per) { n = per; while (n > 0 && s[n] != ' ') n--; if (!n) n = per; }
        snprintf(b, sizeof b, "%.*s", n, s); ftext(x, y + l * (fv2[f].h + 4), f, c, b); s += n; while (*s == ' ') s++; }
}
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
static int v2_fx(void); static int fx2_slot; static int scene_count(void); static int v2_mixer(void); static int mx_pg; static int v2_lfo(void); static int v2_auto(void); static void modmenu_open(void); static void ui_path(char *b, int n); static const char *snd_page_name(void); static void snd_page_count(char *b, int n);   // sound.h
static int in_pad(void); static int ui_window(void);
static int scening_window(void) { return picking || exporting || naming || theming || iconing || browsing || tab == T_FX || tab == T_SEQ || tab == T_PAT; }   // a window over a HOME screen
static int strip_on(void) { return ui_strip != STRIP_HIDDEN && in_pad() && !picking; }   // inside a pad: the pads under the path (L2)
static void ui2_strip_big(void) {        // BIG (2026-11-14, mockup strip3): twelve tiles, number, name, engine, how far the clip has played
    for (int i = 0; i < NSLOT; i++) {
        int px = 12 + i * 84, pw = 78, y = 54, has = has_sound(i), on = song.t[i].on && engine_playing(); char b[8];
        Col c = has ? C_ENG(song.t[i].type) : C_LINE; if (song.t[i].mute) c = mix(c, C_BG, 170);
        if (i == sel) rframe(px - 4, y - 4, pw + 8, 68, 7, 2, C_WHITE, C_BG);
        snprintf(b, sizeof b, "%02d", i + 1);
        if (!has) { rframe(px, y, pw, 60, 5, 1, C_LINE, C_BG); ftext(px + 8, y + 4, FT_S, C_LINE, b); continue; }
        rrect(px, y, pw, 60, 5, C_CELL); rect(px, y, 5, 60, c, 1);
        ftext(px + 12, y + 2, FT_S, C_DIM, b);
        snprintf(b, sizeof b, "%.6s", slot_name(i) ? slot_name(i) : ""); ftext(px + 12, y + 20, FT_S, song.t[i].mute ? C_DIM : C_WHITE, b);
        rect(px + 12, y + 47, pw - 20, 5, C_LINE, 1);
        if (on) { int len = cur_pat(i)->len, st = engine_step(i); if (len > 0 && st >= 0) rect(px + 12, y + 47, (pw - 20) * (st % len + 1) / len, 5, c, 1); }
    }
}
static void ui2_header(void) {           // NEON > PADS on the left, the transport and the tempo on the right
    int strip = strip_on(), hh = strip ? 48 : V2_HEAD;
    rect(0, 0, W, hh, C_CELL, 1);
    if (strip && ui_strip == STRIP_BIG) ui2_strip_big();
    else if (strip) for (int i = 0; i < NSLOT; i++) {   // 12 pads: the engine's colour, how far its clip has played, the open one framed
        int px = 22 + i * 82, pw = 72, on = song.t[i].on && engine_playing(), has = has_sound(i);
        Col c = has ? C_ENG(song.t[i].type) : C_LINE; if (song.t[i].mute) c = mix(c, C_BG, 170);
        if (i == sel) rframe(px - 5, 50, pw + 10, 19, 5, 2, C_WHITE, C_CELL);
        if (!has) { rframe(px, 53, pw, 13, 4, 1, C_LINE, C_BG); continue; }
        rect(px, 53, pw, 4, c, 1);
        if (on) { int len = cur_pat(i)->len, st = engine_step(i); if (len > 0 && st >= 0) rect(px, 61, pw * (st % len + 1) / len, 4, c, 1); }
        else if (pat_used(cur_pat(i))) rect(px, 61, pw / 4, 4, mix(c, C_BG, 150), 1);
    }
    char b[160]; ui_path(b, sizeof b);
    int x = 20, y = (hh - fv2[FT_M].h) / 2;
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
    if (browsing || presetting || slicing || ducking || tab == T_PRJ) return;
    if (scening) { snprintf(t, sizeof t, "%d/%d", live_scene + 1, scene_count()); ftext(W - 20 - ftext_w(FT_MB, t), y, FT_MB, th_cur[TC_SMP], t); return; }   // A11, A12: nothing on the right
    if (v2_sound()) { snd_page_count(t, sizeof t); ftext(W - 20 - ftext_w(FT_MB, t), y, FT_MB, C_ENG(song.t[sel].type), t); return; }   // A08: 1/5
    if (v2_steps() || rolling) { snprintf(t, sizeof t, "P%02d", song.t[sel].cur + 1); ftext(W - 20 - ftext_w(FT_MB, t), y, FT_MB, C_ORANGE, t); return; }   // A05: the pattern
    if (in_pad() || tab != T_GRID || scening) return;   // the mockups: the tempo on HOME only (A01)
    snprintf(t, sizeof t, "%c %d", engine_playing() ? 1 : 2, (int)(song.tempo * tempo_mul + 0.5f));
    ftext(W - 20 - ftext_w(FT_MB, t), y, FT_MB, engine_playing() ? th_cur[TC_SMP] : C_DIM, t);
}
static void ui2_footer(const char *h) {  // at most three key chips: the key in the accent, what it does after it
    rect(0, V2_FOOT, W, H - V2_FOOT, C_CELL, 1);
    int x = 18, y = V2_FOOT + 10, left = 3;
    if (msg_t && SDL_GetTicks() - msg_t < 1500) { int w = ftext_w(FT_MB, msg) + 24; rrect(x, y, w, 32, 6, C_ORANGE); ftext(x + 12, y + 4, FT_MB, C_BG, msg); return; }
    if (!hint_on) return;                // PROJECT > SYSTEM > FOOTER HINTS off: SHIFT still shows every key
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
    else if (!inpad) UI_ADD("%s", tab == T_GRID ? (scening ? "SCENES" : "PADS") : tab == T_FX ? "MIXER" : tab_name[tab == T_PAT ? T_SEQ : tab]);
    int pad = inpad || perform || tab == T_SEQ || tab == T_PAT || (browsing && !bmode) || presetting || slicing || iconing ? sel : tab == T_FX && fx_unit < NSLOT ? fx_unit : -1;
    if (pad >= 0) UI_ADD("%02d %.14s", pad + 1, slot_name(pad) ? slot_name(pad) : "EMPTY");
    else if (tab == T_FX) UI_ADD("%s", fx_unit == NSLOT ? "RETURN A" : fx_unit == NSLOT + 1 ? "RETURN B" : "MASTER");
    if (exporting) UI_ADD("EXPORT"); else if (naming) UI_ADD("NAME"); else if (browsing) UI_ADD("%s", bmode ? "FILES" : "SOUND > FILE");
    else if (slicing) UI_ADD("SOUND > SLICES > EDIT"); else if (rolling) UI_ADD("PATTERN > ROLL"); else if (randing) UI_ADD("RANDOM");
    else if (picking) UI_ADD("ENGINE"); else if (presetting) UI_ADD("SOUND > PRESETS"); else if (theming) UI_ADD("THEME");
    else if (iconing) UI_ADD("ICON"); else if (ducking) UI_ADD("MOD > DUCK");
    else if (recpage) UI_ADD("PATTERN > RECORD");
    else if (v2_sound()) UI_ADD("SOUND > %s", snd_page_name());
    else if (v2_steps()) UI_ADD("PATTERN > STEPS");
    else if (tab == T_PRJ && !theming) UI_ADD("%s", prj_sec_name[jsec]);
    else if (v2_mixer() && mx_pg) UI_ADD("%s", mx_pg == 1 ? "RETURNS" : "FREEZE");
    else if (v2_lfo()) UI_ADD("MOD > LFO");
    else if (v2_auto()) UI_ADD("MOD > AUTOMATION");
    else if (v2_fx()) { const FxSlot *f = &song.mix.pad[fx_unit].ins[fx2_slot]; UI_ADD("FX > %s", f->type ? fx_name[f->type] : "EMPTY"); }
    else if (ui_cat >= 0 && !(tab == T_PAT) && !(ui_cat == PC_SOUND && cl_deep)) UI_ADD("%s", cat_name[ui_cat]);
    else if (tab == T_PAT) UI_ADD("PATTERN > SELECT");
    else if (tab == T_SEQ && srow >= 3 && cl_deep) UI_ADD("%s%s", ui_cat == PC_SOUND ? "SOUND > " : "", song.t[sel].type == 1 && cl_deep == 2 ? "PATCH" : "DEEP");
    else if (tab == T_FX && fx_page && fx_unit < NSLOT) UI_ADD("MOD");
#undef UI_ADD
}


// The cheat sheet: every combination of this screen (its whole hint) and the ones that work everywhere.
static void disc2(int cx, int cy, int r, Col c);
static void ui_cards_more(int x, int y, int h, int first, int rows) {   // four cards of more: a bar on the right, the page lit
    if (rows <= 2) return; int pages = (rows + 1) / 2, pg = first / 2, ph = h / pages;
    rect(x, y, 4, h, C_CELL, 1); rect(x, y + pg * ph, 4, ph, C_DIM, 1);
}
static void ui_shade(int a) {            // the screen under an overlay goes dark
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(R, C_BG.r, C_BG.g, C_BG.b, (Uint8)a);
    SDL_Rect r = {0, 0, W, V2_FOOT}; SDL_RenderFillRect(R, &r); SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
}
static int ui_pair(int x, int y, const char *p, int n) {   // a key chip and what it does, small; returns the width
    int w = (int)strcspn(p, " "); if (w > n) w = n; char b[64];
    snprintf(b, sizeof b, "%.*s", w, p); int kw = ftext_w(FT_S, b) + 16; rrect(x, y, kw, 24, 5, C_SELBG);
    { int lb = C_SELBG.r * 3 + C_SELBG.g * 6 + C_SELBG.b, lo = C_ORANGE.r * 3 + C_ORANGE.g * 6 + C_ORANGE.b, lw = C_WHITE.r * 3 + C_WHITE.g * 6 + C_WHITE.b;
      ftext(x + 8, y + 2, FT_S, abs(lo - lb) * 2 >= abs(lw - lb) ? C_ORANGE : C_WHITE, b); }   // small keys: the key colour unless a theme makes it fade (GAMEBOY)
    snprintf(b, sizeof b, "%.*s", n - w > 0 ? n - w - 1 : 0, p + w + (n > w)); for (char *q = b; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32;
    return kw + 8 + ftext(x + kw + 8, y + 2, FT_S, C_WHITE, b);
}
static void ui_jump_box(int x, int y, int c) {   // a category of the jump cross: the open one filled
    int on = ui_cat == c || (hubbing && hub_cur == c); Col cc = C_CAT(c);
    if (on) rrect(x, y, 118, 50, 8, cc); else rframe(x, y, 118, 50, 8, 3, cc, C_CELL);
    ftext(x + 59 - ftext_w(FT_MB, cat_name[c]) / 2, y + 14, FT_MB, on ? C_BG : cc, cat_name[c]);
}
// SHIFT held alone: every key of this screen (its whole hint), then the ones that work everywhere; inside a pad also the jump
// cross (L3), small, top right. Nothing is cut: the list flows into columns, under the cross when it gets there (2026-11-14:
// "every hidden action in the cheat sheet, on any screen").
static void ui_cheat(void) {
    if (!held[K_SHIFT] || used[K_SHIFT] || !shift_t || ms_since(SDL_GetTicks(), shift_t) < 250) return;
    int pad = in_pad() && !perform && !picking && !slicing && !rolling && !randing && !presetting && !browsing && !ducking && !theming && !iconing && !naming && !exporting;
    enum { Y0 = 96, CX = 640 };          // CX: where the cross starts; columns there begin under it
    ui_shade(215); rrect(14, Y0 - 34, 996, V2_FOOT - Y0 + 28, 10, C_CELL);
    ftext(26, Y0 - 30, FT_MB, C_ORANGE, "KEYS OF THIS SCREEN");
    if (pad) {
        ftext(CX + 6, Y0 - 30, FT_MB, C_WHITE, "SHIFT + arrow: jump");
        ui_jump_box(CX + 125, Y0 + 4, PC_PATTERN); ui_jump_box(CX + 2, Y0 + 62, PC_MIX); ui_jump_box(CX + 248, Y0 + 62, PC_SOUND); ui_jump_box(CX + 125, Y0 + 120, PC_FX);
        disc2(CX + 184, Y0 + 87, 24, C_SELBG); char b[8]; snprintf(b, sizeof b, "%02d", sel + 1); ftext(CX + 184 - ftext_w(FT_MB, b) / 2, Y0 + 75, FT_MB, C_WHITE, b);
        ftext(CX + 6, Y0 + 180, FT_S, C_DIM, "MOD: from the pad's list");
    }
    static const char *const every[] = {"PLAY play / stop", "Y back", "F1/F2 next pad, or screen of HOME", "F1+F2 PROJECT, again: back here",
        "EDIT+ARROWS value (held: shown big)", "OPT listen", "OPT+EDIT delete / default", "SHIFT+L2 undo", "SHIFT+OPT copy", "SHIFT+EDIT paste",
        "MENU pads / keys", "MENU+PLAY record, twice capture", "MENU-3S quit (hold)", "SHIFT this sheet (hold)", "SHIFT+PLAY never: the system's", 0};
    int x = 26, top = Y0 + 4, y = top, r = 0, colw = 0;
    for (int pass = 0; pass < 2; pass++) {
        const char *h = pass ? NULL : ui_hint;
        if (pass) { ftext(x, y, FT_S, C_DIM, "EVERYWHERE"); y += 30; }
        for (int i = 0; pass ? every[i] != NULL : *h != 0; i++) {
            const char *p = pass ? every[i] : h, *e = pass ? NULL : strstr(h, "  "); int n = e ? (int)(e - h) : (int)strlen(p);
            if (y + 26 > V2_FOOT - 6) { x += colw + 24; colw = 0; top = pad && x + 330 > CX ? Y0 + 214 : Y0 + 4; y = top; }
            if (x < 1000 - 120) { int w = ui_pair(x, y, p, n); if (w > colw) colw = w; y += 30; r++; }
            if (pass) continue;
            if (!e) break;
            h = e; while (*h == ' ') h++;
        }
        if (!pass) y += 12;              // a gap, then the keys that work everywhere
    }
}

// The value popup: while EDIT+arrows turn a control of the instrument panel, its name, value and where it sits.
static char uf_name[32], uf_val[48]; static int uf_f; static Col uf_col;   // the card under the cursor this frame (ui_focus)
static void ui_focus(const char *n, const char *v, int f, Col c) {   // f: 0..100 where the value sits, -1 a choice
    snprintf(uf_name, sizeof uf_name, "%s", n); snprintf(uf_val, sizeof uf_val, "%s", v); uf_f = f; uf_col = c;
}
static void ui_popup(void) {              // EDIT held and turning: the value large, over the page (ТЗ: always on)
    if (!held[K_EDIT] || !used[K_EDIT] || !uf_name[0] || perform) return;
    enum { PW = 560, PX = (W - PW) / 2, PY = 84 }; int PH = uf_f >= 0 ? 132 : 84;
    rframe(PX, PY, PW, PH, 12, 3, uf_col, C_CELL);
    ftext(PX + 24, PY + 16, FT_M, C_DIM, uf_name);
    ftext(PX + PW - 24 - ftext_w(FT_XB, uf_val), PY + 12, FT_XB, C_WHITE, uf_val);
    if (uf_f >= 0) { rect(PX + 24, PY + PH - 34, PW - 48, 10, C_LINE, 1); rect(PX + 24, PY + PH - 34, (PW - 48) * clampi(uf_f, 0, 100) / 100, 10, uf_col, 1); }
}
static void ui_empty(int x, int y, int w, int h, const char *what, const char *how) {   // an empty state: what is missing, the key that fixes it
    rrect(x, y, w, h, 10, C_CELL);
    ftext(x + w / 2 - ftext_w(FT_LB, what) / 2, y + h / 2 - 36, FT_LB, C_WHITE, what);
    ftext(x + w / 2 - ftext_w(FT_M, how) / 2, y + h / 2 + 8, FT_M, C_DIM, how);
}
static int any_sound(void) { for (int s = 0; s < NSLOT; s++) if (has_sound(s)) return 1; return 0; }
static void ui_quit_hold(void) {          // MENU held: how long until the app quits (3 s, main loop), so a long press is never a surprise
    if (!menu_down) return; Uint32 d = SDL_GetTicks() - menu_down; if (d < 400) return;
    enum { PW = 560, PX = (W - PW) / 2, PY = 300, PH = 132 }; int f = clampi((int)(d - 400) * 100 / 2600, 0, 100);
    rframe(PX, PY, PW, PH, 12, 3, C_RED, C_CELL);
    ftext(PX + 24, PY + 16, FT_MB, C_WHITE, "QUIT BRICKPAD");
    const char *r = "release MENU to stay"; ftext(PX + PW - 24 - ftext_w(FT_M, r), PY + 16, FT_M, C_DIM, r);
    rect(PX + 24, PY + PH - 46, PW - 48, 14, C_LINE, 1); rect(PX + 24, PY + PH - 46, (PW - 48) * f / 100, 14, C_RED, 1);
}

// The pad hub (UI v2): PATTERN SOUND FX MOD MIX of the selected pad. EDIT opens one, Y goes back up (category -> hub ->
// HOME), F1/F2 the next pad on the same page, SHIFT+arrow jumps: UP PATTERN, RIGHT SOUND, DOWN FX, LEFT MIX.
// Until steps 4-5 rebuild them, the categories open the screens of 1.2 for this pad.
static void hub_open(void) { set_tab(T_GRID); hubbing = 1; }
static void patmenu_open(void) { set_tab(T_SEQ); ui_cat = hub_cur = PC_PATTERN; patmenu = 1; if (is_auto(lane)) lane = lane_first(); }
static void cat_open(int c) {
    set_tab(c == PC_PATTERN || c == PC_SOUND ? T_SEQ : c == PC_MIX ? T_MIX : T_FX);
    hub_cur = c; ui_cat = c;
    if (c == PC_PATTERN && is_auto(lane)) lane = lane_first();
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
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) { if (hub_cur == PC_PATTERN) patmenu_open(); else if (hub_cur == PC_MOD) modmenu_open(); else cat_open(hub_cur); } return; }
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
        if (pm_cur == 1) pat_open(); else if (pm_cur == 2) recpage = 1; else if (cur_pat(sel)->mode) roll_open(); else { srow = 1; scol = 0; lane = lane_first(); seq_clamp(); } } return; }   // STEPS open on the notes
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
    if (!pat_used(p)) { const char *m = "empty: EDIT puts a note on the step"; ftext(512 - ftext_w(FT_M, m) / 2, 660, FT_M, C_ORANGE, m); }
    if (cs < p->len && p->trig[cs]) { char nb[12], ln[24]; step_value(p, cs, lane, nsl, nb); snprintf(ln, sizeof ln, "STEP %d %s", cs + 1, lane_full(lane)); ui_focus(ln, nb, -1, C_ENG(song.t[sel].type));
        step_value(p, cs, L_TRIG, nsl, nb); snprintf(b, sizeof b, "step %d \3 %s \3 vel %d", cs + 1, nb, p->vel[cs]); }
    else snprintf(b, sizeof b, "step %d", cs + 1);
    ftext(W - 28 - ftext_w(FT_M, b), 660, FT_M, C_WHITE, b);
    draw_footer(lane == L_TRIG || lane == L_SLICE ? "EDIT NOTE  L1/R1 LANE  Y BACK  EDIT+ARROWS VALUE  OPT LISTEN  OPT+<> WALK AND LISTEN  X+<> PAD  OPT+EDIT DELETE  XX DOUBLE CLIP  SHIFT+X RANDOM"
                                                  : "EDIT+ARROWS VALUE  L1/R1 LANE  Y BACK  OPT LISTEN  X+<> PAD  OPT+EDIT DEFAULT  XX DOUBLE CLIP  SHIFT+X RANDOM");
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
