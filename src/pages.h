// UI v2, step 5: the pad's FX (A17), MOD (A18-A20) and MIX pages, the MIXER (A21-A23), SCENES (A02), PROJECT (A24).
// Each page draws after its mockup and drives the 1.2 state underneath (frow/fcol, lcol/lrow, mcol/mrow...), so the
// actions, undo and the engine stay as they were. main.c includes this once, after sound.h.
#pragma once
static void fx_pstr(int type, int k, int v, char *b);

// ---- FX (A17): the two inserts as tabs (L1/R1), a picture of the effect (EDIT: the next type), its parameters as cards ----
static int fx2_slot, fx2_cur = -1;       // the insert shown; the card under the cursor, -1 = the picture (the type)
static int v2_fx(void) { return ui_cat == PC_FX && tab == T_FX && fx_unit < NSLOT && !fx_page; }
typedef struct { int8_t row, side; } FxCard;   // a card is a 1.2 row of the slot (1 ON, 2..7 the parameters), side = its right half
static int fx2_cards(FxCard *c) {
    const FxSlot *f = &song.mix.pad[fx_unit].ins[fx2_slot]; int n = 0;
    if (!f->type) return 0;
    c[n++] = (FxCard){1, 0};
    for (int k = 0; k < 6; k++) if (fx_pname[f->type][k][0]) c[n++] = (FxCard){(int8_t)(2 + k), 0};
    for (int r = 2; r < 8; r++) if (fx_multi_row(fx2_slot, r)) c[n++] = (FxCard){(int8_t)r, 1};
    return n;
}
static void fx2_sync(void) {             // the 1.2 cursor follows the card: fx_value, fx_tap and fx_delete act on it
    FxCard c[16]; int n = fx2_cards(c); fx2_slot &= 1;
    if (fx2_cur >= n) fx2_cur = n - 1;
    fcol = fx2_slot;
    if (fx2_cur < 0) { frow = 0; fx_sub = 0; } else { frow = c[fx2_cur].row; fx_sub = c[fx2_cur].side; }
}
static void fx2_move(int k) {            // two columns of cards; UP from the first row: the picture
    FxCard c[16]; int n = fx2_cards(c);
    if (fx2_cur < 0) { if (k == K_DOWN && n) fx2_cur = 0; }
    else if (k == K_LEFT) { if (fx2_cur & 1) fx2_cur--; }
    else if (k == K_RIGHT) { if (!(fx2_cur & 1) && fx2_cur + 1 < n) fx2_cur++; }
    else if (k == K_DOWN) { if (fx2_cur + 2 < n) fx2_cur += 2; else if ((fx2_cur | 1) + 1 < n) fx2_cur = n - 1; }
    else fx2_cur = fx2_cur >= 2 ? fx2_cur - 2 : -1;
    fx2_sync();
}
static void fx2_picture(int x, int y, int w, int h, const FxSlot *f, Col c) {   // what the effect does, as one line
    int px = x, py = y + h / 2, ty = f->type; float p0 = f->p[0] / 100.f, p1 = f->p[1] / 100.f;
    if (!ty) { const char *m = "no effect: EDIT picks one"; ftext(x + w / 2 - ftext_w(FT_M, m) / 2, y + h / 2 - 12, FT_M, C_DIM, m); return; }
    for (int i = 0; i <= w; i += 3) {
        float t = (float)i / w, v;          // v -1..1
        switch (ty) {
        case FX_DIST: case FX_TAPE: case FX_COMP: case FX_MULTI: {   // a transfer curve: soft to hard as the drive goes up
            float in = t * 2 - 1, d = 1 + (ty == FX_DIST ? p1 : ty == FX_TAPE ? p0 : 0.6f) * 8; v = tanhf(in * d) / tanhf(d); break; }
        case FX_CRUSH: { int steps = 2 + f->p[0] / 8; v = floorf(sinf(t * 6.2832f * 2) * steps) / steps; break; }
        case FX_FILTER: { float fc = 0.15f + p1 * 0.7f, d = (t - fc) / 0.08f; v = (t < fc ? 0.4f : 0.4f - (t - fc) * 3) + f->p[2] / 100.f * expf(-d * d) * 0.6f; if (v < -0.9f) v = -0.9f; break; }
        case FX_RING: v = sinf(t * 6.2832f * 3) * sinf(t * 6.2832f * (3 + p0 * 12)); break;
        case FX_AUTOPAN: v = f->p[2] == 2 ? (fmodf(t * 4, 1) < 0.5f ? 0.8f : -0.8f) : f->p[2] == 1 ? 1.6f * fabsf(fmodf(t * 4, 1) - 0.5f) * 2 - 0.8f : 0.8f * sinf(t * 6.2832f * 2); break;
        default: v = (0.3f + 0.6f * p1) * sinf(t * 6.2832f * (1 + p0 * 3));   // CHORUS FLANGER PHASER: the sweep
        }
        int nx = x + i, ny = y + h / 2 - (int)(v * (h / 2 - 14));
        if (i) line(px, py, nx, ny, 3, f->on ? c : C_DIM);
        px = nx; py = ny;
    }
}
static void fx2_card(int x, int y, int w, int h, const FxSlot *f, FxCard cd, int on, Col ec) {
    char v[24]; const char *lb; int val, lo = 0, hi = 1;
    if (cd.row == 1) { lb = "ON"; snprintf(v, sizeof v, "%s", f->on ? "ON" : "OFF"); val = f->on; }
    else if (cd.side) { int8_t *g = fx_side(fx2_slot, cd.row, &lo, &hi); val = *g;
        if (f->type == FX_COMP) { lb = "KEY"; if (val) snprintf(v, sizeof v, "PAD %d", val); else snprintf(v, sizeof v, "SELF"); }
        else { lb = mb_name[cd.row - 2]; snprintf(v, sizeof v, "%+d DB", val); } }
    else { int k = cd.row - 2; lb = fx_pname[f->type][k]; val = f->p[k]; fx_range(f->type, k, &lo, &hi); fx_pstr(f->type, k, val, v); }
    if (on) rframe(x, y, w, h, 8, 4, ec, C_CELL); else rrect(x, y, w, h, 8, C_CELL);
    ftext(x + 20, y + 12, FT_M, C_DIM, lb);
    ftext(x + w - 20 - ftext_w(FT_XB, v), y + 10, FT_XB, C_WHITE, v);
    if (hi - lo > 3) { int by = y + h - 26; rect(x + 20, by, w - 40, 8, C_LINE, 1); rect(x + 20, by, (w - 40) * clampi(val - lo, 0, hi - lo) / (hi - lo), 8, ec, 1); }
}
static void draw_fx2(void) {
    PadMix *pm = &song.mix.pad[fx_unit]; Col ec = C_CAT(PC_FX); fx2_sync();
    for (int s = 0; s < 2; s++) {        // the inserts as tabs
        const FxSlot *f = &pm->ins[s]; char b[32]; int x = 26 + s * 492, on = s == fx2_slot;
        snprintf(b, sizeof b, "%d \003 %s", s + 1, f->type ? fx_name[f->type] : "EMPTY");
        rrect(x, 76, 479, 56, 6, on ? ec : C_CELL);
        ftext(x + 240 - ftext_w(FT_MB, b) / 2, 92, FT_MB, on ? C_BG : f->type && f->on ? C_WHITE : C_DIM, b);
    }
    const FxSlot *f = &pm->ins[fx2_slot]; FxCard c[16]; int n = fx2_cards(c);
    int rows = (n + 1) / 2, tall = rows <= 2, ph = tall ? 186 : 136, h = tall ? 162 : 96, dy = tall ? 174 : 104, y0 = 153 + ph + 16;
    if (fx2_cur < 0) rframe(26, 153, 971, ph, 8, 4, ec, C_CELL); else rrect(26, 153, 971, ph, 8, C_CELL);
    fx2_picture(46, 153, 931, ph, f, ec);
    int vis = (700 - y0) / dy, first = clampi((fx2_cur < 0 ? 0 : fx2_cur / 2) - vis / 2, 0, rows > vis ? rows - vis : 0);
    for (int i = first * 2; i < n && i < (first + vis) * 2; i++)
        fx2_card(i & 1 ? 518 : 32, y0 + (i / 2 - first) * dy, 473, h, f, c[i], i == fx2_cur, ec);
    if (rows > vis) { int th = vis * dy - 8, tw = th * vis / rows; rect(1004, y0, 4, th, C_CELL, 1); rect(1004, y0 + (th - tw) * first / (rows - vis), 4, tw, C_DIM, 1); }
    if (fx2_cur < 0) draw_footer("EDIT CHANGE  L1/R1 SLOT  Y BACK  EDIT+<> TYPE  OPT+EDIT REMOVE");
    else if (c[fx2_cur].row == 1) draw_footer("EDIT ON/OFF  L1/R1 SLOT  Y BACK");
    else draw_footer("EDIT+ARROWS VALUE  L1/R1 SLOT  Y BACK  OPT+EDIT DEFAULT");
}

// ---- MOD: its list (LFO, AUTOMATION, DUCK), as PATTERN's ----
static void modmenu_open(void) { set_tab(T_SEQ); ui_cat = hub_cur = PC_MOD; modmenu = 1; }
static void modmenu_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) {
        if (mm_cur == 0) { modmenu = 0; set_tab(T_FX); ui_cat = PC_MOD; fx_unit = sel; fx_page = 1; }
        else if (mm_cur == 1) { modmenu = 0; set_tab(T_SEQ); ui_cat = PC_MOD; if (!is_auto(lane)) lane = L_A1; set_step(0); seq_clamp(); }
        else duck_open(sel); }       // the DUCK window over the list: Y there comes back here
        return; }
    if (k == K_Y) { modmenu = 0; hub_open(); hub_cur = PC_MOD; return; }
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_OPT) { act_listen(sel); return; }
    if (k == K_UP || k == K_LEFT) mm_cur = (mm_cur + 2) % 3;
    if (k == K_DOWN || k == K_RIGHT) mm_cur = (mm_cur + 1) % 3;
}
static void draw_modmenu(void) {
    enum { CX = 122, CWD = 778, CHT = 104, DY = 122, Y0 = 82 };
    static const char *const nm[3] = {"LFO", "AUTOMATION", "DUCK"};
    for (int c = 0; c < 3; c++) {
        int y = Y0 + c * DY, on = c == mm_cur; Col cc = C_CAT(PC_MOD); char b[96], t[24]; b[0] = 0;
        rrect(CX, y, CWD, CHT, 8, on ? mix(C_SELBG, C_DIM, 40) : C_CELL);
        rect(CX, y, 9, CHT, cc, 1);
        ftext(CX + 38, y + 12, FT_LB, cc, nm[c]);
        for (int j = 0; j < (c == 2 ? 0 : 4); j++) { int l = (int)strlen(b);
            if (c == 0) { const Lfo *L = &song.lfo[sel][j]; lfo_tname(sel, L->target, t); snprintf(b + l, sizeof b - l, "%s%d %s", j ? " \3 " : "", j + 1, L->target && L->depth ? t : "-"); }
            else { const AutoLane *al = &song.aut[sel][song.t[sel].cur][j]; lfo_tname(sel, al->target, t); snprintf(b + l, sizeof b - l, "%sA%d %s", j ? " \3 " : "", j + 1, al->target ? t : "-"); } }
        if (c == 2) { const PadMix *pm = &song.mix.pad[sel]; (void)pm; snprintf(b, sizeof b, "the pad ducks under another one: curve, source, depth, length"); }
        ftext(CX + 38, y + 56, FT_M, on ? C_WHITE : C_DIM, b);
    }
    draw_footer("EDIT OPEN  F1/F2 PAD  Y BACK");
}

// ---- MOD > LFO (A18): LFO 1-4 as tabs (L1/R1), its shape, seven cards (the 1.2 rows: lrow) ----
static int v2_lfo(void) { return ui_cat == PC_MOD && tab == T_FX && fx_unit < NSLOT && fx_page && !modmenu; }
static void lfo2_move(int k) {
    if (k == K_LEFT) { if (lrow & 1) lrow--; } else if (k == K_RIGHT) { if (!(lrow & 1) && lrow + 1 < LR_N) lrow++; }
    else if (k == K_DOWN) { if (lrow + 2 < LR_N) lrow += 2; else if ((lrow | 1) + 1 < LR_N) lrow = LR_N - 1; }
    else if (lrow >= 2) lrow -= 2;
}
static float lfo2_shape(int sh, float t, unsigned *rng) {   // one period, -1..1
    switch (sh % LS_N) {
    case 0: return sinf(t * 6.2832f);
    case 1: return t < 0.25f ? 4 * t : t < 0.75f ? 2 - 4 * t : 4 * t - 4;
    case 2: return 2 * t - 1;
    case 3: return 1 - 2 * t;
    case 4: return t < 0.5f ? 1 : -1;
    case 5: { *rng = *rng * 1103515245u + 12345u; return ((*rng >> 16) & 255) / 127.5f - 1; }
    case 6: return sinf(t * 6.2832f) * 0.6f + sinf(t * 12.566f + 1) * 0.3f;
    default: return 1 - 2 * (1 - (1 - t) * (1 - t));   // DUCK: down at once, back up
    }
}
static void draw_lfo2(void) {
    Col ec = C_CAT(PC_MOD); const Lfo *L = &song.lfo[fx_unit][lcol]; char b[32];
    for (int j = 0; j < NLFO; j++) { int x = 26 + j * 243, on = j == lcol; const Lfo *M = &song.lfo[fx_unit][j]; snprintf(b, sizeof b, "LFO %d", j + 1);
        if (on) rrect(x, 76, 233, 38, 8, ec);
        ftext(x + 116 - ftext_w(FT_MB, b) / 2, 83, FT_MB, on ? C_BG : M->target && M->depth ? C_WHITE : C_DIM, b); }
    rrect(26, 127, 971, 200, 8, C_CELL);
    { int px = 0, py = 0; unsigned rng = 7u + lcol; float hold = 0; int act = L->target && L->depth;
      for (int i = 0; i <= 931; i += 3) { float t = fmodf(i / 465.5f + L->phase / 100.f, 1.f), v;
          if (L->shape % LS_N == 5) { if (i % 117 < 3) hold = lfo2_shape(5, t, &rng); v = hold; } else v = lfo2_shape(L->shape, t, &rng);
          int x = 46 + i, y = 227 - (int)(v * 80);
          if (i) line(px, py, x, y, 3, act ? ec : C_DIM);
          px = x; py = y; }
      if (act) { float v = engine_lfo(fx_unit, lcol); disc2(986, 227 - (int)(v * 80), 6, C_WHITE); } }   // where it is now
    for (int r = 0; r < LR_N; r++) {
        int x = r & 1 ? 518 : 32, y = 343 + (r / 2) * 88, on = r == lrow, lo = 0, hi = 0, v = 0;
        lfo_str(fx_unit, lcol, r, b);
        if (on) rframe(x, y, 473, 80, 8, 4, ec, C_CELL); else rrect(x, y, 473, 80, 8, C_CELL);
        ftext(x + 20, y + 10, FT_M, C_DIM, lr_name[r]);
        ftext(x + 453 - ftext_w(FT_LB, b), y + 8, FT_LB, C_WHITE, b);
        if (r == LR_DEPTH) { lo = -100; hi = 100; v = L->depth; } else if (r == LR_PHASE) { hi = 100; v = L->phase; } else if (r == LR_RATE && !L->sync) { hi = 100; v = L->rate; }
        if (hi > lo) { rect(x + 20, y + 58, 433, 8, C_LINE, 1); rect(x + 20, y + 58, 433 * (v - lo) / (hi - lo), 8, ec, 1); }
    }
    draw_footer(lrow == LR_TARGET ? "EDIT POINT AT A KNOB  EDIT+<> TARGET  L1/R1 LFO  Y BACK  OPT+EDIT UNLINK" : "EDIT+ARROWS VALUE  L1/R1 LFO  Y BACK  OPT+EDIT LFO OFF");
}

// ---- MOD > AUTOMATION (A19): A1-A4 as tabs (L1/R1; UP: the tab, EDIT+<> its target), the lane over the whole pattern ----
static int v2_auto(void) { return ui_cat == PC_MOD && tab == T_SEQ && is_auto(lane) && !modmenu && !hubbing && !perform; }
static void auto2_move(int k) {
    int len = cur_pat(sel)->len ? cur_pat(sel)->len : 16;
    if (srow == 0) { if (k == K_DOWN) { srow = 1; scol = 0; } return; }
    if (k == K_UP) { srow = 0; scol = 4; return; }
    if (k == K_LEFT || k == K_RIGHT) set_step(clampi(cur_step() + (k == K_RIGHT ? 1 : -1), 0, len - 1));
}
static void draw_auto2(void) {
    Col ec = C_CAT(PC_MOD); const Pattern *p = cur_pat(sel); int len = p->len ? p->len : 16, cur = lane - L_A1;
    for (int a = 0; a < NAUTO; a++) { const AutoLane *al = &song.aut[sel][song.t[sel].cur][a]; char t[24], b[40]; int x = 26 + a * 243, on = a == cur;
        lfo_tname(sel, al->target, t); snprintf(b, sizeof b, "A%d %.12s", a + 1, al->target ? t : "-");
        if (on) { rrect(x, 76, 233, 38, 8, ec); if (srow == 0) rframe(x - 4, 72, 241, 46, 10, 3, C_WHITE, ec); }
        ftext(x + 116 - ftext_w(FT_MB, b) / 2, 83, FT_MB, on ? C_BG : al->target ? C_WHITE : C_DIM, b); }
    enum { PX = 26, PY = 130, PW = 971, PH = 520, IX = PX + 14, IW = PW - 28, TOP = PY + 30, BOT = PY + PH - 30 };
    rrect(PX, PY, PW, PH, 8, C_CELL);
    const AutoLane *al = cur_aut(); const int8_t *bc = aut_bend(); int st = srow >= 1 ? cur_step() : -1, any = 0;
    for (int i = 0; i <= len; i++) { int x = IX + i * IW / len; rect(x, PY + 8, 1, PH - 16, i % 4 ? mix(C_LINE, C_CELL, 100) : C_LINE, 1); }
    if (st >= 0) { int x = IX + st * IW / len; rect(x, PY + 8, IW / len > 2 ? IW / len : 2, PH - 16, mix(C_SELBG, C_CELL, 60), 1); }
    for (int i = 0; i < len; i++) if (al->v[i] != AUTO_EMPTY) any = 1;
    if (any) { int px = -1, py = 0;
        for (int xx = 0; xx <= IW; xx += 2) { double sf = xx * (double)len / IW; if (sf >= len) sf = len - 1e-4;
            int pt = (int)sf; if (px >= 0 && al->v[pt] != AUTO_EMPTY && IX + pt * IW / len > px) {   // a point: straight up or down to it
                int vx = IX + pt * IW / len, vy = BOT - al->v[pt] * (BOT - TOP) / 100; line(px, py, vx, py, 3, ec); line(vx, py, vx, vy, 3, ec); px = vx; py = vy; }
            float v = auto_value(al, bc, len, sf); int qx = IX + xx, qy = BOT - (int)(v * (BOT - TOP) / 100);
            if (px >= 0) line(px, py, qx, qy, 3, ec); px = qx; py = qy; }
        for (int i = 0; i < len; i++) if (al->v[i] != AUTO_EMPTY) disc2(IX + i * IW / len, BOT - al->v[i] * (BOT - TOP) / 100, i == st ? 9 : 7, i == st ? C_WHITE : mix(C_WHITE, ec, 60)); }
    else { const char *m = al->target ? "no points: EDIT puts one on the step" : "no target: UP, then EDIT+<> picks one"; ftext(512 - ftext_w(FT_M, m) / 2, PY + PH / 2 - 12, FT_M, C_DIM, m); }
    char b[48]; int back = aut_point_back();
    if (back >= 0) snprintf(b, sizeof b, "bend %+d%%", bc[back]); else snprintf(b, sizeof b, "bend -");
    ftext(26, 664, FT_M, C_DIM, b);
    if (st >= 0) { if (al->v[st] != AUTO_EMPTY) snprintf(b, sizeof b, "step %d \004 %d%%", st + 1, al->v[st]); else snprintf(b, sizeof b, "step %d", st + 1);
        ftext(997 - ftext_w(FT_M, b), 664, FT_M, C_WHITE, b); }
    draw_footer(srow == 0 ? "EDIT+<> TARGET  EDIT POINT AT A KNOB  Y BACK" : "EDIT POINT  Y+ARROWS BEND  Y BACK  EDIT+^v VALUE  L1/R1 LANE  OPT+EDIT CLEAR");
}
