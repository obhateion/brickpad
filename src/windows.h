// UI v2: the windows over the screens in the mockups' look: EXPORT, the questions, the name keyboard, the file menu,
// RANDOM, the icons, the theme editor, the project list. Keys and state are their own (export_key, nm_key, fm_key,
// rnd_key, icon_key, theme_key, browse_key); only the drawing lives here. main.c includes this once, before draw().
#pragma once

static void w_row(int x, int y, int w, int h, const char *lb, const char *v, int on, Col ec) {   // a setting: its name left, the value right
    if (on) rframe(x, y, w, h, 8, 4, ec, C_CELL); else rrect(x, y, w, h, 8, C_CELL);
    ftext(x + 20, y + (h - fv2[FT_M].h) / 2, FT_M, C_DIM, lb);
    int f = ftext_w(FT_LB, v) < w - 60 - ftext_w(FT_M, lb) ? FT_LB : FT_M;
    ftext(x + w - 20 - ftext_w(f, v), y + (h - fv2[f].h) / 2, f, C_WHITE, v);
}
static void w_btn(int x, int y, int w, int h, const char *t, int on, Col c, int dim) {   // a button: filled under the cursor
    if (on) rrect(x, y, w, h, 8, c); else rframe(x, y, w, h, 8, 2, C_LINE, C_CELL);
    ftext(x + w / 2 - ftext_w(FT_LB, t) / 2, y + (h - fv2[FT_LB].h) / 2, FT_LB, on ? C_BG : dim ? C_DIM : C_WHITE, t);
}
static void w_box(int x, int y, int w, int h, Col c) { ui_shade(200); rframe(x, y, w, h, 12, 3, c, C_CELL); }   // a question over the screen
static void w_center(int cx, int y, int f, Col c, const char *s) { ftext(cx - ftext_w(f, s) / 2, y, f, c, s); }

static void draw_export2(void) {
    Col ec = th_cur[TC_SMP]; char b[96]; double bar_s = (song.beats > 0 ? song.beats : 4) * 60.0 / song.tempo, sec = bar_s * ex_bars_v[ex_bars] + ex_tail_v[ex_tail];
    static const char *const rn[5] = {"BARS", "FROM BAR", "CLIPS", "TAIL", "FILES"};
    for (int r = 0; r < 5; r++) {
        if (r == EX_BARS) snprintf(b, sizeof b, "%d", ex_bars_v[ex_bars]); else if (r == EX_FROM) snprintf(b, sizeof b, "%d", ex_from);
        else if (r == EX_CLIPS) snprintf(b, sizeof b, ex_all ? "ALL WITH NOTES" : "THE ONES PLAYING"); else if (r == EX_TAIL) snprintf(b, sizeof b, ex_tail_v[ex_tail] ? "%d S" : "NONE", ex_tail_v[ex_tail]);
        else snprintf(b, sizeof b, ex_stems ? "ONE PER PAD" : "ONE MIX");
        w_row(26, 80 + r * 82, 971, 72, rn[r], b, ex_row == r && !ex_run, ec); }
    w_btn(26, 496, 971, 84, ex_run ? "RENDERING" : "EXPORT", ex_row == EX_GO || ex_run, ec, 0);
    const char *nm = prj_cur[0] ? prj_cur : "UNTITLED";
    snprintf(b, sizeof b, ex_stems ? "exports/%s PADnn.wav" : "exports/%s.wav", nm); ftext(26, 596, FT_S, C_DIM, ex_run ? ex_path : b);
    snprintf(b, sizeof b, "%.1f s \003 %d BPM \003 48 kHz 16 bit", sec, song.tempo); ftext(997 - ftext_w(FT_S, b), 596, FT_S, C_DIM, b);
    if (ex_run) {
        int pc = ex_total ? (int)(ex_done * 100 / ex_total) : 0; rect(26, 640, 971, 10, C_LINE, 1); rect(26, 640, 971 * pc / 100, 10, ec, 1);
        if (ex_stems) snprintf(b, sizeof b, "pad %d \003 %d%% \003 peak %.2f", ex_pad + 1, pc, ex_peak); else snprintf(b, sizeof b, "%d%% \003 peak %.2f", pc, ex_peak);
        ftext(26, 664, FT_M, ex_peak > 1 ? C_RED : C_DIM, b);
        draw_footer("Y CANCEL");
    } else draw_footer(ex_row == EX_GO ? "EDIT EXPORT  ^v ROW  Y BACK" : "<> CHANGE  ^v ROW  Y BACK");
}
static void draw_ask2(void) {
    int x = 152, y = 240, w = 720, h = 270; char b[64]; w_box(x, y, w, h, C_ORANGE);
    snprintf(b, sizeof b, "%.20s has unsaved changes", prj_cur[0] ? prj_cur : "UNTITLED"); w_center(x + w / 2, y + 34, FT_LB, C_WHITE, b);
    w_center(x + w / 2, y + 78, FT_M, C_DIM, ask_act == 2 ? "before a new project" : "before opening another project");
    static const char *const bt[3] = {"SAVE", "DISCARD", "CANCEL"};
    for (int i = 0; i < 3; i++) w_btn(x + 30 + i * 226, y + 150, 206, 76, bt[i], i == ask_cur, i == 1 ? C_RED : C_ORANGE, 0);
    draw_footer("EDIT OK  <> CHOOSE  Y CANCEL");
}
static void draw_naming2(void) {
    static const char *const ttl[5] = {"the project's name", "the new folder's name", "the new name", "the theme's name (up to 19 characters)", "the pad's name (up to 15 characters, empty: the instrument's)"};
    Col ec = th_cur[TC_SMP]; ftext(26, 78, FT_M, C_DIM, ttl[nm_mode % 5]);
    int l = (int)strlen(nm_buf), cw = fv2[FT_XB].w + 4, vis = 900 / cw, off = nm_pos > vis - 1 ? nm_pos - vis + 1 : 0, x0 = 46;
    rrect(26, 110, 971, 84, 8, C_CELL);
    for (int c0 = 0; c0 < vis; c0++) { int i = off + c0, x = x0 + c0 * cw; char c[2] = {i < l ? nm_buf[i] : 0, 0};
        if (i == nm_pos) rect(x - 3, 124, 3, 56, ec, 1);     // the cursor stands before this character
        if (c[0]) ftext(x, 132, FT_XB, C_WHITE, c); }
    if (nm_ext[0]) ftext(977 - ftext_w(FT_M, nm_ext), 142, FT_M, C_DIM, nm_ext);
    char p[1024]; const char *warn = NULL;
    if (nm_mode == NM_PAD) { p[0] = 0; if (strlen(nm_buf) > 15) warn = "only the first 15 are kept"; }
    else if (nm_mode == NM_PROJECT) { prj_path(nm_buf, p, sizeof p); if (!access(p, F_OK) && strcmp(nm_buf, prj_cur)) warn = "a project with this name exists: saving replaces it"; }
    else if (nm_mode == NM_THEME) { snprintf(p, sizeof p, THEME_DIR "/%.19s.thm", nm_buf); if (!access(p, F_OK)) warn = "a theme with this name exists: saving replaces it"; }
    else { snprintf(p, sizeof p, "%s/%s%s", bdir, nm_buf, nm_ext); if (!access(p, F_OK) && strcmp(p, nm_old)) warn = "that name is taken here"; }
    if (warn) ftext(26, 204, FT_M, C_ORANGE, warn); else { int pl = (int)strlen(p); ftext(26, 206, FT_S, C_DIM, pl > 90 ? p + pl - 90 : p); }
    enum { KX = 84, KY = 250, KW = 86, KH = 86 };
    for (int r = 0; r < 4; r++) for (int c0 = 0; c0 < NK_COLS; c0++) {
        int x = KX + c0 * KW, yy = KY + r * KH, on = r == nk_r && c0 == nk_c; char ch = nk_keys[r][c0], t[2] = {ch, 0};
        if (nk_low && ch >= 'A' && ch <= 'Z') t[0] = (char)(ch + 32);
        rrect(x, yy, KW - 8, KH - 8, 8, on ? ec : C_CELL);
        if (ch == ' ') w_center(x + (KW - 8) / 2, yy + 28, FT_S, on ? C_BG : C_DIM, "SPACE"); else w_center(x + (KW - 8) / 2, yy + 22, FT_XB, on ? C_BG : C_WHITE, t);
    }
    static const char *const bn2[NK_NB] = {"DELETE", "aA", "(", ")", "OK"};
    for (int b = 0; b < NK_NB; b++) { int x = KX + nk_bx[b] * KW, yy = KY + 4 * KH, w = nk_bw[b] * KW - 8, on = nk_r == NK_ROWS - 1 && nk_btn() == b;
        rrect(x, yy, w, KH - 8, 8, on ? ec : b == NK_OK ? C_SELBG : C_CELL);
        w_center(x + w / 2, yy + 26, FT_LB, on ? C_BG : C_WHITE, b == NK_CASE ? (nk_low ? "abc" : "ABC") : bn2[b]); }
    draw_footer("EDIT TYPE  PLAY OK  Y BACK  OPT DELETE  L1/R1 PLACE");
}
static void draw_fmenu2(void) {
    if (fmenu) {
        int x = 560, y = 96, w = 436, h = 6 * 62 + 64; char t[64]; w_box(x, y, w, h, C_ORANGE);
        snprintf(t, sizeof t, "%.30s", bn && bcur < bn ? bent[bcur].name : bdir); ftext(x + 24, y + 16, FT_S, C_DIM, t);
        for (int i = 0; i < 6; i++) { int yy = y + 50 + i * 62, on = i == fm_cur, dim = i == 4 ? !fm_cut[0] : i && !bn;
            if (on) rrect(x + 12, yy, w - 24, 54, 8, C_SELBG);
            ftext(x + 32, yy + 14, FT_LB, dim ? C_DIM : i == 5 ? C_RED : C_WHITE, fm_name[i]); }
        draw_footer("EDIT DO IT  ^v CHOOSE  Y CLOSE");
    }
    if (fconfirm) {
        int x = 152, y = 240, w = 720, h = 280; const char *nm = strrchr(fc_path, '/'); nm = nm ? nm + 1 : fc_path; char b[96]; w_box(x, y, w, h, C_RED);
        snprintf(b, sizeof b, "delete %.30s?", nm); w_center(x + w / 2, y + 30, FT_LB, C_WHITE, b);
        struct stat st; int dir = !stat(fc_path, &st) && S_ISDIR(st.st_mode), yy = y + 72;
        if (dir) { snprintf(b, sizeof b, "a folder with %d file%s in it", fc_files, fc_files == 1 ? "" : "s"); w_center(x + w / 2, yy, FT_M, C_ORANGE, b); yy += 28; }
        if (fc_used) { snprintf(b, sizeof b, "%d pad%s use it: they will miss it", fc_used, fc_used == 1 ? "" : "s"); w_center(x + w / 2, yy, FT_M, C_RED, b); yy += 28; }
        w_center(x + w / 2, yy, FT_M, C_DIM, "it cannot be undone");
        w_btn(x + 60, y + 176, 280, 76, "DELETE", fm_cur == 0, C_RED, 0); w_btn(x + 380, y + 176, 280, 76, "CANCEL", fm_cur == 1, C_ORANGE, 0);
        draw_footer("EDIT OK  <> CHOOSE  Y CANCEL");
    }
}
static void draw_rnd2(void) {
    Pattern *p = cur_pat(sel); char b[48]; Col ec = C_CAT(PC_PATTERN); int acid = song.t[sel].type == 2;
    for (int r = 0; r < RN_GO; r++) {
        int y = 78 + r * 66, on = rn_row == r, lo, hi; rn_range(r, &lo, &hi); rn_str(r, b);
        int dim = (r == RN_SLIDE && !acid && !p->mode) || (r == RN_HOLD && !p->mode) || ((r == RN_RANGE || r == RN_MOVE) && rn_v[RN_WHAT] == 0) || ((r == RN_DENS || r == RN_FEEL) && rn_v[RN_WHAT] == 1);
        if (on) rframe(26, y, 560, 58, 8, 4, ec, C_CELL); else rrect(26, y, 560, 58, 8, C_CELL);
        ftext(46, y + 10, FT_M, C_DIM, rn_name[r]); ftext(566 - ftext_w(FT_MB, b), y + 10, FT_MB, dim ? C_DIM : C_WHITE, b);
        if (hi > 3) { rect(46, y + 42, 520, 6, C_LINE, 1); rect(46, y + 42, 520 * rn_v[r] / 100, 6, dim ? C_DIM : ec, 1); }
    }
    w_btn(26, 78 + RN_GO * 66 + 4, 560, 700 - (78 + RN_GO * 66 + 4), "GENERATE", rn_row == RN_GO, ec, 0);
    int st = song.t[sel].on ? engine_step(sel) : -1;       // the pattern as it is now
    rrect(604, 78, 393, 300, 8, C_CELL); snprintf(b, sizeof b, "P%d \003 %s \003 %s %s", song.t[sel].cur + 1, p->mode ? "ROLL" : "STEPS", note_name[track_key(sel)], scale_name[track_scale(sel)]);
    ftext(624, 90, FT_S, C_DIM, b);
    if (p->mode) draw_roll_mini(p, 620, 124, 361, 236, st, 0); else step_matrix(p, 640, 130, 180, st, 0);
    ftext(604, 396, FT_S, C_DIM, "notes come from the pad's key and scale");
    ftext(604, 424, FT_S, C_DIM, "every GENERATE is a new take");
    draw_footer(rn_row == RN_GO ? "EDIT GENERATE  Y BACK  SHIFT+L2 UNDO  PLAY LISTEN" : "EDIT+<> CHANGE  ^v ROW  Y BACK");
}
static void draw_icons2(void) {
    enum { GX = 26, GY = 126, C = 66, PX = 826, PW = 171, rows = 8 };
    int now = track_icon(&song.t[sel]) - 1, g = icon_group(ic_cur), a = icon_grp_at[g], n = icon_grp_at[g + 1] - a; Col ec = C_ENG(song.t[sel].type);
    for (int i = 0, x = 26; i < ICON_GRP_N; i++) { int w = ftext_w(FT_MB, icon_grp_name[i]) + 28;      // the groups, L1/R1
        if (i == g) rrect(x, 76, w, 36, 8, ec);
        ftext(x + 14, 82, FT_MB, i == g ? C_BG : C_DIM, icon_grp_name[i]); x += w + 6; }
    for (int c = 0; c < ICON_COLS * rows && c < n; c++) {
        int x = GX + (c % ICON_COLS) * C, y = GY + (c / ICON_COLS) * C, i = a + c, on = i == ic_cur, id = icon_pal[i];
        rrect(x, y, C - 6, C - 6, 6, on ? ec : C_CELL);
        if (id < 0) ftext(x + 6, y + 20, FT_S, on ? C_BG : C_DIM, "AUTO"); else icon_sc(id, x + 6, y + 6, 3, on ? C_BG : C_WHITE);
        if (id == now) rect(x + 2, y + 2, 8, 8, on ? C_BG : ec, 1);   // what the pad has now
    }
    int id = icon_pal[ic_cur], show = id >= 0 ? id : slot_icon(sel); char b[24];
    rrect(PX, GY, PW, rows * C - 6, 8, C_CELL);
    snprintf(b, sizeof b, "PAD %02d", sel + 1); ftext(PX + 20, GY + 14, FT_M, C_DIM, b);
    icon_sc(show, PX + 22, GY + 60, 8, ec);
    const char *nm = id >= 0 ? icon_tab[id].name : "AUTO"; w_center(PX + PW / 2, GY + 210, FT_MB, C_WHITE, nm);
    if (id < 0) w_center(PX + PW / 2, GY + 240, FT_S, C_DIM, "by the sound's kind");
    snprintf(b, sizeof b, "%d / %d", ic_cur + 1, ICON_PAL_N); w_center(PX + PW / 2, GY + rows * C - 44, FT_S, C_DIM, b);
    draw_footer("EDIT SET  L1/R1 GROUP  Y BACK  OPT LISTEN");
}
static void draw_theme2(void) {
    static const char *chan[3] = {"R", "G", "B"}, *act[3] = {"SAVE", "SAVE AS", "DELETE"}; char b[48]; Col ec = th_cur[TC_SMP];
    snprintf(b, sizeof b, "%s%s", theme_name(th_sel), th_mod ? " *" : ""); int x = 26 + ftext(26, 76, FT_LB, C_WHITE, b) + 20;
    ftext(x, 80, FT_M, C_DIM, th_mod ? "changed, not saved" : th_sel < TH_PRESETS ? "built-in" : "yours");
    for (int c = 0; c < 3; c++) ftext(430 + c * 100 + 24, 82, FT_S, ted_col == c && ted_row < TC_N ? ec : C_DIM, chan[c]);
    enum { LW = 700, VR = 10, RH = 52 };
    int first = ted_row < TC_N ? clampi(ted_row - VR / 2, 0, TC_N - VR) : TC_N - VR;
    for (int r = first; r < first + VR; r++) {
        int y = 110 + (r - first) * RH, on = r == ted_row; Col col = th_cur[r]; int v[3] = {col.r, col.g, col.b};
        rrect(26, y, LW, RH - 6, 8, on ? C_SELBG : C_CELL);
        ftext(46, y + 12, FT_M, on ? C_WHITE : C_DIM, th_color_name[r]);
        rrect(330, y + 8, 70, RH - 22, 5, col); 
        for (int c = 0; c < 3; c++) { int cx = 430 + c * 100, sc = on && c == ted_col; snprintf(b, sizeof b, "%3d", v[c]);
            if (sc) rframe(cx - 8, y + 4, 76, RH - 14, 6, 3, ec, C_SELBG);
            ftext(cx, y + 11, FT_MB, sc ? C_WHITE : on ? C_WHITE : C_DIM, b); }
    }
    if (TC_N > VR) { int th = VR * RH - 6, tw = th * VR / TC_N; rect(26 + LW + 6, 110, 4, th, C_CELL, 1); rect(26 + LW + 6, 110 + (th - tw) * first / (TC_N - VR), 4, tw, C_DIM, 1); }
    for (int a2 = 0; a2 < 3; a2++) w_btn(26 + a2 * 236, 110 + VR * RH + 4, 228, 60, act[a2], ted_row == TC_N && ted_col == a2, a2 == 2 ? C_RED : ec, a2 == 2 && th_sel < TH_PRESETS);
    enum { PX = 752, PW = 245 };          // what the colours give
    rrect(PX, 76, PW, 624, 8, C_CELL); ftext(PX + 20, 90, FT_S, C_DIM, "PREVIEW");
    static const char *const en[4] = {"SMP", "GRD", "ACD", "CLD"};
    for (int i = 0; i < 4; i++) { int y = 126 + i * 44; rect(PX + 20, y, 9, 34, C_ENG(i), 1); ftext(PX + 44, y + 6, FT_MB, C_ENG(i), en[i]); }
    for (int i = 0; i < PC_N; i++) { int y = 310 + i * 40; rect(PX + 20, y, 9, 30, C_CAT(i), 1); ftext(PX + 44, y + 4, FT_MB, C_CAT(i), cat_name[i]); }
    ftext(PX + 20, 522, FT_MB, C_WHITE, "TEXT"); ftext(PX + 90, 522, FT_M, C_DIM, "dim"); 
    rrect(PX + 20, 560, 90, 32, 6, C_SELBG); ftext(PX + 32, 564, FT_MB, C_ORANGE, "EDIT"); ftext(PX + 124, 564, FT_M, C_WHITE, "key");
    rrect(PX + 20, 606, 96, 30, 6, C_PLAY); ftext(PX + 32, 610, FT_S, C_BG, "PLAY"); rrect(PX + 126, 606, 96, 30, 6, C_RED); ftext(PX + 138, 610, FT_S, C_BG, "ALERT");
    rect(PX + 20, 656, PW - 40, 8, C_LINE, 1); rect(PX + 20, 656, (PW - 40) / 2, 8, ec, 1);
    draw_footer(ted_row == TC_N ? "EDIT DO IT  <> CHOOSE  Y BACK" : "EDIT+<> 1  EDIT+^v 16  Y BACK  OPT+EDIT SAVED COLOUR");
}
// PROJECT > FILE > OPEN: the projects on the left, what the one under the cursor is on the right (pin: read by prj_info_draw)
static void prj_info_draw2(void) {
    enum { x = 612, y = 76, w = 385 }; char b[64];
    rrect(x, y, w, 624, 8, C_CELL);
    if (!pin.ok) { ftext(x + 20, y + 20, FT_MB, C_RED, "not a project file"); return; }
    struct tm tmv; time_t tt = (time_t)pin.mt; localtime_r(&tt, &tmv); strftime(b, sizeof b, "%Y-%m-%d %H:%M", &tmv); ftext(x + 20, y + 16, FT_S, C_DIM, b);
    snprintf(b, sizeof b, "%d BPM", pin.tempo); ftext(x + 20, y + 46, FT_XB, C_WHITE, b);
    snprintf(b, sizeof b, "%s %s", note_name[pin.key % 12], scale_name[pin.scale % NSCALE]); ftext(x + 20, y + 92, FT_M, C_DIM, b);
    for (int s = 0; s < NSLOT; s++) { int yy = y + 136 + s * 39; snprintf(b, sizeof b, "%02d", s + 1);
        if (pin.nm[s][0]) rect(x + 20, yy + 2, 5, 22, C_ENG(pin.type[s]), 1);
        ftext(x + 38, yy, FT_M, C_DIM, b); snprintf(b, sizeof b, "%.20s", pin.nm[s][0] ? pin.nm[s] : "-"); ftext(x + 82, yy, FT_M, pin.nm[s][0] ? C_WHITE : C_LINE, b); }
}
static void draw_prjbrowse2(void) {
    enum { ROWS = 10, RH = 62 };
    if (bcur < btop) btop = bcur; if (bcur >= btop + ROWS) btop = bcur - ROWS + 1;
    for (int i = 0; i < ROWS && btop + i < bn; i++) { int k = btop + i, y = 76 + i * RH, on = k == bcur; char nm[40]; int nl = (int)strlen(bent[k].name);
        if (!bent[k].dir && nl > 4) nl -= 4;   // no .bpp
        if (nl > 30) snprintf(nm, sizeof nm, "%.20s..%.8s", bent[k].name, bent[k].name + nl - 8); else snprintf(nm, sizeof nm, "%.*s%s", nl, bent[k].name, bent[k].dir ? "/" : "");
        rrect(26, y, 572, RH - 10, 8, on ? C_SELBG : C_CELL);
        ftext(48, y + 14, on ? FT_MB : FT_M, bent[k].dir ? C_DIM : C_WHITE, nm);
        if (!bent[k].dir && !strncmp(bent[k].name, prj_cur, (size_t)nl) && (int)strlen(prj_cur) == nl) ftext(578 - ftext_w(FT_S, "open"), y + 16, FT_S, th_cur[TC_SMP], "open"); }
    if (bn > ROWS) { int th = ROWS * RH - 10, tw = th * ROWS / bn; rect(602, 76, 4, th, C_CELL, 1); rect(602, 76 + (th - tw) * btop / (bn - ROWS), 4, tw, C_DIM, 1); }
    if (!bn) ftext(48, 96, FT_M, C_DIM, "no projects yet: PROJECT > FILE > SAVE AS makes one");
    if (bcur < bn && !bent[bcur].dir) { char fp[1024]; snprintf(fp, sizeof fp, "%s/%s", bdir, bent[bcur].name); prj_info_draw(fp); }
    else rrect(612, 76, 385, 624, 8, C_CELL);
    if (!fmenu && !fconfirm) draw_footer("EDIT OPEN  X FILES  Y BACK  < UP");
    draw_fmenu();
}

// No 1.2 screen is left to fall back on: whatever state the keys left, it is brought to the nearest screen of the ladder.
static void v2_normalize(void) {
    if (exporting || naming || browsing || slicing || randing || picking || presetting || theming || iconing || ducking || hubbing || patmenu || modmenu || recpage || rolling) return;
    if (tab == T_PAT) { if (ui_cat < 0) ui_cat = PC_PATTERN; return; }
    if (tab == T_SEQ) {
        if (ui_cat < 0 || ui_cat == PC_FX || ui_cat == PC_MIX) { int c = ui_cat < 0 ? PC_PATTERN : ui_cat; tab = T_GRID; hubbing = 1; hub_cur = c; ui_cat = -1; return; }
        if (ui_cat == PC_PATTERN) { if (is_auto(lane)) lane = lane_first(); if (cur_pat(sel)->mode) { roll_open(); return; } if (srow < 1 || srow > 2) { srow = 1; scol = 0; } }
        else if (ui_cat == PC_SOUND) { if (srow < 3) sound_sync(); }
        else if (ui_cat == PC_MOD && !is_auto(lane)) { lane = L_A1; if (srow > 2) set_step(0); }
        return;
    }
    if (tab == T_FX && fx_unit < NSLOT) { sel = fx_unit; if (ui_cat != PC_FX && ui_cat != PC_MOD) ui_cat = fx_page ? PC_MOD : PC_FX; if ((ui_cat == PC_MOD) != (fx_page != 0)) fx_page = ui_cat == PC_MOD; return; }
    if (tab == T_MIX && ui_cat >= 0) { ui_cat = PC_MIX; if (mcol >= NSLOT) mcol = sel; }
}
