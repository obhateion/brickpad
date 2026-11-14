// UI v2, step 5: the pad's FX (A17), MOD (A18-A20) and MIX pages, the MIXER (A21-A23), SCENES (A02), PROJECT (A24).
// Each page draws after its mockup and drives the 1.2 state underneath (frow/fcol, lcol/lrow, mcol/mrow...), so the
// actions, undo and the engine stay as they were. main.c includes this once, after sound.h.
#pragma once
static void fx_pstr(int type, int k, int v, char *b); static void unit_pstr(int unit, int k, int v, char *b);

// ---- FX (A17): the two inserts as tabs (L1/R1), a picture of the effect (EDIT: the next type), its parameters as cards ----
static int fx2_slot, fx2_cur = -1;       // the insert shown; the card under the cursor, -1 = the picture (the type)
static int v2_fx(void) { return ui_cat == PC_FX && tab == T_FX && fx_unit < NSLOT && !fx_page; }
typedef struct { int8_t row, side; } FxCard;   // a card is a 1.2 row of the slot (1 ON, 2..7 the parameters), side = its right half
static int fx2_cards(FxCard *c) {
    const FxSlot *f = &song.mix.pad[fx_unit].ins[fx2_slot]; int n = 0;
    if (!f->type) return 0;              // ON/OFF: X (the tab dims), as A17 has no card for it
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
    if (on) { rframe(x, y, w, h, 8, 4, ec, C_CELL); ui_focus(lb, v, hi - lo > 3 ? (val - lo) * 100 / (hi - lo) : -1, ec); } else rrect(x, y, w, h, 8, C_CELL);
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
    int rows = (n + 1) / 2, ph = 186, h = 162, dy = 174, y0 = 153 + ph + 16;   // four cards, the rest by pages
    if (fx2_cur < 0) rframe(26, 153, 971, ph, 8, 4, ec, C_CELL); else rrect(26, 153, 971, ph, 8, C_CELL);
    fx2_picture(46, 153, 931, ph, f, ec);
    int vis = 2, first = (fx2_cur < 0 ? 0 : fx2_cur / 2) / 2 * 2;
    for (int i = first * 2; i < n && i < (first + vis) * 2; i++)
        fx2_card(i & 1 ? 518 : 32, y0 + (i / 2 - first) * dy, 473, h, f, c[i], i == fx2_cur, ec);
    ui_cards_more(1004, y0, vis * dy - 8, first, rows);
    if (fx2_cur < 0) draw_footer("EDIT CHANGE  L1/R1 SLOT  Y BACK  EDIT+<> TYPE  X ON/OFF  OPT+EDIT REMOVE");
    else draw_footer("EDIT+ARROWS VALUE  L1/R1 SLOT  Y BACK  X ON/OFF  OPT+EDIT DEFAULT");
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
static const int8_t lfo2_ord[LR_N] = {LR_TARGET, LR_SHAPE, LR_RATE, LR_DEPTH, LR_SYNC, LR_TRIG, LR_PHASE};   // A18's four first
static int lfo2_pos(void) { for (int i = 0; i < LR_N; i++) if (lfo2_ord[i] == lrow) return i; return 0; }
static void lfo2_move(int k) {
    int p = lfo2_pos();
    if (k == K_LEFT) { if (p & 1) p--; } else if (k == K_RIGHT) { if (!(p & 1) && p + 1 < LR_N) p++; }
    else if (k == K_DOWN) { if (p + 2 < LR_N) p += 2; else if ((p | 1) + 1 < LR_N) p = LR_N - 1; }
    else if (p >= 2) p -= 2;
    lrow = lfo2_ord[p];
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
    int cp = lfo2_pos(), first = cp / 2 / 2 * 2;
    for (int i = first * 2; i < LR_N && i < first * 2 + 4; i++) {
        int r = lfo2_ord[i], x = i & 1 ? 518 : 32, y = 343 + (i / 2 - first) * 178, on = r == lrow, lo = 0, hi = 0, v = 0;
        lfo_str(fx_unit, lcol, r, b);
        if (on) rframe(x, y, 473, 166, 8, 4, ec, C_CELL); else rrect(x, y, 473, 166, 8, C_CELL);
        ftext(x + 20, y + 12, FT_M, C_DIM, lr_name[r]);
        ftext(x + 453 - ftext_w(FT_XB, b) > x + 140 ? x + 453 - ftext_w(FT_XB, b) : x + 140, y + 10, ftext_w(FT_XB, b) < 300 ? FT_XB : FT_M, C_WHITE, b);
        if (r == LR_DEPTH) { lo = -100; hi = 100; v = L->depth; } else if (r == LR_PHASE) { hi = 100; v = L->phase; } else if (r == LR_RATE && !L->sync) { hi = 100; v = L->rate; }
        if (hi > lo) { rect(x + 20, y + 140, 433, 8, C_LINE, 1); rect(x + 20, y + 140, 433 * (v - lo) / (hi - lo), 8, ec, 1); }
        if (on) ui_focus(lr_name[r], b, hi > lo ? (v - lo) * 100 / (hi - lo) : -1, ec);
    }
    ui_cards_more(1004, 343, 348, first, (LR_N + 1) / 2);
    draw_footer(lrow == LR_TARGET ? "EDIT PICK ON SOUND  EDIT+<> TARGET  L1/R1 LFO  Y BACK  OPT+EDIT UNLINK" : "EDIT+ARROWS VALUE  L1/R1 LFO  Y BACK  OPT+EDIT LFO OFF");
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
    draw_footer(srow == 0 ? "EDIT+<> TARGET  EDIT PICK ON SOUND  Y BACK" : "EDIT POINT  X+^v BEND  Y BACK  EDIT+^v VALUE  L1/R1 LANE  OPT+EDIT CLEAR");
}

// ---- MIX of a pad: its strip of the mixer as cards (the 1.2 rows: mrow, the column mcol = the pad) ----
static const int8_t mix2_rows[8] = {MX_VOL, MX_PAN, MX_SA, MX_SB, MX_FRZ, MX_DUCK, MX_MUTE, MX_SOLO};
static int mix2_cur;
static int v2_mix(void) { return ui_cat == PC_MIX && tab == T_MIX && mcol < NSLOT && !ducking; }
static void mix2_move(int k) {
    if (k == K_LEFT) { if (mix2_cur & 1) mix2_cur--; } else if (k == K_RIGHT) { if (!(mix2_cur & 1)) mix2_cur++; }
    else if (k == K_DOWN) { if (mix2_cur + 2 < 8) mix2_cur += 2; } else if (mix2_cur >= 2) mix2_cur -= 2;
    mrow = mix2_rows[mix2_cur];
}
static void draw_mix2(void) {
    Col ec = C_CAT(PC_MIX); const Track *t = &song.t[mcol]; const PadMix *pm = &song.mix.pad[mcol]; mrow = mix2_rows[mix2_cur & 7];
    static const char *const nm[8] = {"VOLUME", "PAN", "SEND A", "SEND B", "FREEZE", "DUCK", "MUTE", "SOLO"};
    int first = mix2_cur / 4 * 2;           // four cards a page: VOLUME PAN SENDS, then FREEZE DUCK MUTE SOLO
    for (int i = first * 2; i < first * 2 + 4; i++) { char v[24]; int x = i & 1 ? 518 : 32, y = 86 + (i / 2 - first) * 312, on = i == mix2_cur, val = -1, lo = 0, hi = 100;
        switch (i) { case 0: val = t->vol; snprintf(v, sizeof v, "%d", t->vol); break;
            case 1: val = t->pan; lo = -50; hi = 50; if (t->pan) snprintf(v, sizeof v, "%c%d", t->pan < 0 ? 'L' : 'R', abs(t->pan)); else snprintf(v, sizeof v, "C"); break;
            case 2: val = pm->send_a; snprintf(v, sizeof v, "%d", pm->send_a); break;
            case 3: val = pm->send_b; snprintf(v, sizeof v, "%d", pm->send_b); break;
            case 4: snprintf(v, sizeof v, "%s", frozen(mcol) ? "FROZEN" : "LIVE"); break;
            case 5: if (pm->spare[DK_SRC]) snprintf(v, sizeof v, "PAD %d %d%%", pm->spare[DK_SRC], pm->spare[DK_AMT]); else snprintf(v, sizeof v, "OFF"); break;
            case 6: snprintf(v, sizeof v, "%s", t->mute ? "ON" : "OFF"); break;
            default: snprintf(v, sizeof v, "%s", pm->solo ? "ON" : "OFF"); }
        if (on) { rframe(x, y, 473, 296, 8, 4, ec, C_CELL); ui_focus(nm[i], v, val >= 0 ? (val - lo) * 100 / (hi - lo) : -1, ec); } else rrect(x, y, 473, 296, 8, C_CELL);
        ftext(x + 20, y + 12, FT_M, C_DIM, nm[i]); ftext(x + 453 - ftext_w(FT_XB, v), y + 10, FT_XB, (i >= 4 && (i == 4 ? frozen(mcol) : i == 6 ? t->mute : i == 7 ? pm->solo : pm->spare[DK_SRC] != 0)) ? ec : C_WHITE, v);
        if (val >= 0 || i == 1) { int by = y + 296 - 30; rect(x + 20, by, 433, 8, C_LINE, 1);
            if (i == 1) { int cx = x + 20 + 433 / 2, w = 433 * t->pan / 100; if (w > 0) rect(cx, by, w, 8, ec, 1); else rect(cx + w, by, -w, 8, ec, 1); rect(cx, by - 4, 2, 16, C_WHITE, 1); }
            else rect(x + 20, by, 433 * clampi(val - lo, 0, hi - lo) / (hi - lo), 8, ec, 1); } }
    draw_footer(mix2_cur == 4 ? "EDIT FREEZE/LIVE  Y BACK" : mix2_cur == 5 ? "EDIT OPEN DUCK  EDIT+<> SOURCE  Y BACK  EDIT+^v DEPTH" : mix2_cur >= 6 ? "EDIT ON/OFF  Y BACK" : "EDIT+ARROWS VALUE  Y BACK  OPT+EDIT DEFAULT");
}

// ---- MIXER (HOME): L1/R1 the page: PADS (A21, faders), RETURNS (A22: A, B, MASTER), FREEZE (A23, and EXPORT) ----
static int mx_pg, mx_r, mx_fz;            // the page; RETURNS: the row in a column (0 level, 1-2 two parameters); FREEZE: the row
static int v2_mixer(void) { return ui_cat < 0 && tab == T_MIX && !ducking && !perform; }
static const int8_t mx_ret_k[3][2] = {{RV_SIZE, RV_DECAY}, {DL_TIME, DL_FB}, {MS_COMP, MS_LIM}};
static void mixer2_fix(void) {            // the cursor belongs to the page: pads on PADS, a unit on RETURNS
    if (mx_pg == 1 && (mcol < NSLOT || mcol > NSLOT + 2)) mcol = NSLOT; if (mx_pg != 1 && mcol >= NSLOT) mcol = sel; mx_r = clampi(mx_r, 0, 2);
}
static void mixer2_page(int d) { mx_pg = (mx_pg + d + 3) % 3; if (mx_pg == 0) { mcol = sel; mrow = MX_VOL; } else if (mx_pg == 1) { mcol = NSLOT; mx_r = 0; mrow = MX_VOL; } else mx_fz = 0; }
static void mixer2_move(int k) {
    mixer2_fix();
    if (mx_pg == 0) { if (k == K_LEFT) mcol = (mcol + NSLOT - 1) % NSLOT; else if (k == K_RIGHT) mcol = (mcol + 1) % NSLOT; sel = mcol; mrow = MX_VOL; return; }
    if (mx_pg == 1) { if (k == K_LEFT && mcol > NSLOT) mcol--; else if (k == K_RIGHT && mcol < NSLOT + 2) mcol++; else if (k == K_UP && mx_r) mx_r--; else if (k == K_DOWN && mx_r < 2) mx_r++;
        mrow = mx_r ? MX_PAN : MX_VOL; return; }
    int n = 0; for (int s = 0; s < NSLOT; s++) n += has_sound(s);
    if (k == K_UP && mx_fz) mx_fz--; else if (k == K_DOWN && mx_fz < n) mx_fz++;
}
static int mixer2_fz_pad(void) { int n = 0; for (int s = 0; s < NSLOT; s++) if (has_sound(s)) { if (n == mx_fz) return s; n++; } return -1; }   // -1: EXPORT
static void mixer2_arrow(int k) {         // EDIT+arrows
    int d = k == K_RIGHT || k == K_UP ? 1 : -1, big = k == K_UP || k == K_DOWN; mixer2_fix();
    if (mx_pg == 0) { act_pad_volume_step(mcol, d * (big ? 5 : 1)); return; }
    if (mx_pg == 1) { int u = mcol - NSLOT;
        if (mx_r == 0) { if (u == 2) act_master_step(d * (big ? 5 : 1)); else act_unit_param_add(mcol, u ? DL_LEVEL : RV_LEVEL, d * (big ? 5 : 1)); }
        else if (u == 2) act_master_fx_toggle(mx_ret_k[2][mx_r - 1]);
        else act_unit_param_add(mcol, mx_ret_k[u][mx_r - 1], d * (big && !(u == 1 && mx_r == 1) ? 5 : 1)); }
}
static int mixer2_tap(void) {             // EDIT: RETURNS opens the unit's every parameter; FREEZE freezes, or exports
    mixer2_fix();
    if (mx_pg == 1) { fx_unit = mcol; frow = fcol = 0; set_tab(T_FX); return 1; }
    if (mx_pg == 2) { int s = mixer2_fz_pad(); if (s < 0) { export_open(); return 1; } freeze_set(s, !frozen(s)); if (!frozen(s)) toast("UNFROZEN"); return 1; }
    return 0;
}
static void draw_mixer2(void) {
    mixer2_fix();
    if (mx_pg == 0) {                     // A21: twelve faders in the pads' engine colours
        for (int s = 0; s < NSLOT; s++) { int cx = 66 + s * 81, on = s == mcol, top = 86, bot = 640, h = (bot - top) * song.t[s].vol / 100;
            Col c = has_sound(s) ? (on ? C_ORANGE : C_ENG(song.t[s].type)) : C_LINE; char b[4];
            if (on) rframe(cx - 38, top - 10, 76, 638, 8, 3, C_WHITE, C_BG);
            if (on) { char vv[8]; snprintf(vv, sizeof vv, "%d", song.t[s].vol); char nn[24]; snprintf(nn, sizeof nn, "%02d VOLUME", s + 1); ui_focus(nn, vv, song.t[s].vol, c); }
            rect(cx - 8, top, 16, bot - top, C_CELL, 1); if (has_sound(s)) { rect(cx - 8, bot - h, 16, h, c, 1); rect(cx - 22, bot - h - 5, 44, 10, C_WHITE, 1); }
            snprintf(b, sizeof b, "%02d", s + 1); ftext(cx - ftext_w(FT_MB, b) / 2, 652, FT_MB, on ? C_WHITE : C_DIM, b);
            if (song.t[s].mute) ftext(cx - 6, 678, FT_MB, C_RED, "M"); else if (song.mix.pad[s].solo) ftext(cx - 6, 678, FT_MB, C_ORANGE, "S"); }
        draw_footer("EDIT+^v LEVEL  Y MUTE  SHIFT+Y SOLO  L1/R1 RETURNS, FREEZE");
    } else if (mx_pg == 1) {              // A22: the returns and the master, a fader and two bars each
        static const char *const cn[3] = {"A \003 REVERB", "B \003 DELAY", "MASTER"}, *const pn[3][2] = {{"SIZE", "DECAY"}, {"TIME", "FEEDBACK"}, {"COMP", "LIMITER"}};
        for (int u = 0; u < 3; u++) { int x = 32 + u * 324, on = mcol == NSLOT + u, lo, hi, def; Col c = u == 2 ? th_cur[TC_SMP] : C_CAT(PC_MIX);
            if (on) rframe(x, 76, 310, 626, 8, 4, C_CAT(PC_MIX), C_CELL); else rrect(x, 76, 310, 626, 8, C_CELL);
            ftext(x + 20, 92, FT_MB, C_WHITE, cn[u]);
            int lv = u == 2 ? song.master : *unit_param(NSLOT + u, u ? DL_LEVEL : RV_LEVEL, &lo, &hi, &def), fh = 380 * lv / 100;
            rect(x + 142, 168, 26, 380, C_LINE, 1); rect(x + 142, 548 - fh, 26, fh, c, 1);
            if (on && mx_r == 0) rframe(x + 134, 160, 42, 396, 6, 3, C_WHITE, C_CELL), rect(x + 142, 168, 26, 380, C_LINE, 1), rect(x + 142, 548 - fh, 26, fh, c, 1);
            for (int r = 0; r < 2; r++) { int k = mx_ret_k[u][r], y = 570 + r * 62, v; char b[16];
                if (u == 2) { v = song.mix.ms[k] ? 100 : 0; snprintf(b, sizeof b, "%s", song.mix.ms[k] ? "ON" : "OFF"); }
                else { v = *unit_param(NSLOT + u, k, &lo, &hi, &def); unit_pstr(NSLOT + u, k, v, b); v = hi > lo ? (v - lo) * 100 / (hi - lo) : 0; }
                ftext(x + 20, y, FT_M, on && mx_r == r + 1 ? C_WHITE : C_DIM, pn[u][r]); ftext(x + 290 - ftext_w(FT_M, b), y, FT_M, on && mx_r == r + 1 ? C_WHITE : C_DIM, b);
                rect(x + 20, y + 30, 270, 8, C_LINE, 1); rect(x + 20, y + 30, 270 * v / 100, 8, on && mx_r == r + 1 ? C_WHITE : C_CAT(PC_MIX), 1); } }
        draw_footer("EDIT+^v VALUE  Y BACK  EDIT ALL PARAMETERS  L1/R1 PAGE");
    } else {                              // A23: the pads with a sound, live or frozen; EXPORT last; the DSP load
        int n = 0, ss[NSLOT]; for (int s = 0; s < NSLOT; s++) if (has_sound(s)) ss[n++] = s;
        int top = clampi(mx_fz - 3, 0, n + 1 > 6 ? n + 1 - 6 : 0);
        for (int r = 0; r < 6 && top + r <= n; r++) { int i = top + r, y = 78 + r * 96, on = i == mx_fz; char b[40];
            rrect(26, y, 971, 80, 8, on ? C_SELBG : C_CELL);
            if (i == n) { ftext(48, y + 26, FT_LB, C_WHITE, "EXPORT"); ftext(974 - ftext_w(FT_M, "song \003 WAV"), y + 28, FT_M, C_DIM, "song \003 WAV"); continue; }
            int s = ss[i]; snprintf(b, sizeof b, "%02d %.16s", s + 1, slot_name(s) ? slot_name(s) : "EMPTY"); ftext(48, y + 26, FT_LB, C_WHITE, b);
            if (frozen(s)) { const char *f = "\002 frozen"; ftext(974 - ftext_w(FT_M, f), y + 28, FT_M, C_ENG(3), f); } else ftext(974 - ftext_w(FT_M, "live"), y + 28, FT_M, C_DIM, "live"); }
        int ld = (int)(engine_load() * 100); char b[8]; snprintf(b, sizeof b, "%d%%", ld);
        ftext(26, 660, FT_M, C_DIM, "CPU"); rect(102, 668, 780, 8, C_LINE, 1); rect(102, 668, 780 * clampi(ld, 0, 100) / 100, 8, ld >= 85 ? C_RED : th_cur[TC_SMP], 1);
        ftext(997 - ftext_w(FT_MB, b), 658, FT_MB, C_WHITE, b);
        draw_footer(mixer2_fz_pad() < 0 ? "EDIT EXPORT  Y BACK  L1/R1 PAGE" : "EDIT FREEZE  Y BACK  L1/R1 PAGE");
    }
}

// ---- MOD > DUCK (A20): the curve as sixteen bars, the fields as cards (keys: duck_key, the 1.2 window's) ----
static void draw_duck2(void) {
    const PadMix *pm = &song.mix.pad[sel]; const int8_t *dx = song.dkx[sel]; const uint8_t *cv = song.dkc[sel];
    int mode = dx[DX_MODE] % DM_N, amt = pm->spare[DK_AMT], src = pm->spare[DK_SRC], onn = mode == DM_GRID ? amt > 0 : src > 0;
    Col ec = C_CAT(PC_MIX); enum { PX = 26, PY = 80, PW = 971, PH = 280 };
    if (dk_row == 0) rframe(PX, PY, PW, PH, 8, 4, ec, C_CELL); else rrect(PX, PY, PW, PH, 8, C_CELL);
    for (int i = 0; i < 16; i++) { int bw = (PW - 28) / 16, x = PX + 14 + i * bw, h = (PH - 28) * cv[i] / 100, on = dk_row == 0 && i == dk_pt;
        int rh = (PH - 28) * (100 - amt * (100 - cv[i]) / 100) / 100;   // what the level really does, with the depth
        rect(x, PY + PH - 14 - h, bw - 6, h > 1 ? h : 1, on ? C_WHITE : onn ? ec : C_DIM, 1);
        rect(x, PY + PH - 14 - rh, bw - 6, 3, mix(C_WHITE, ec, 120), 1); }
    { float g = 1, v = engine_duck_view(sel, &g); if (v >= 0) rect(PX + 14 + (int)((PW - 28) * v), PY + 6, 2, PH - 12, C_WHITE, 1); }
    if (dk_row == 0) { char nn[16], vv[8]; snprintf(nn, sizeof nn, "POINT %d", dk_pt + 1); snprintf(vv, sizeof vv, "%d%%", cv[dk_pt]); ui_focus(nn, vv, cv[dk_pt], ec); }
    static const char *const fn2[DF_N] = {"TRIGGER", "SOURCE", "DEPTH", "LENGTH", "", "SMOOTH", "SHAPE"};
    int first = dk_row == 1 ? dk_col / 2 / 2 * 2 : 0;
    for (int c = first * 2; c < DF_N && c < first * 2 + 4; c++) { char v[24]; int x = c & 1 ? 518 : 32, y = PY + PH + 16 + (c / 2 - first) * 170, on = dk_row == 1 && dk_col == c, use = c != DF_SRC || mode != DM_GRID;
        switch (c) { case DF_MODE: snprintf(v, sizeof v, "%s", mode == DM_NOTE ? "NOTES" : dk_mode_name[mode]); break;
            case DF_SRC: if (src) snprintf(v, sizeof v, "%02d %.10s", src, slot_name(src - 1) ? slot_name(src - 1) : "EMPTY"); else snprintf(v, sizeof v, "-"); break;
            case DF_AMT: snprintf(v, sizeof v, "%d%%", amt); break; case DF_LEN: snprintf(v, sizeof v, "%s", dk_len_name[dx[DX_LEN] % DK_NLEN]); break;
            case DF_RATE: if (mode == DM_GRID) snprintf(v, sizeof v, "%s", dk_len_name[dx[DX_GRID] % DK_NLEN]); else if (mode == DM_AUDIO) snprintf(v, sizeof v, "%dDB", (int)((dx[DX_THRESH] - 100) * 0.6f)); else { snprintf(v, sizeof v, "-"); use = 0; } break;
            case DF_SMOOTH: if (dx[DX_SMOOTH]) snprintf(v, sizeof v, "%dMS", dx[DX_SMOOTH] / 2 > 0 ? dx[DX_SMOOTH] / 2 : 1); else snprintf(v, sizeof v, "OFF"); break;
            default: snprintf(v, sizeof v, "%s", dx[DX_SHAPE] == DK_USER ? "USER" : dk_shape_name[dx[DX_SHAPE] % DK_NSHAPE]); }
        if (on) { rframe(x, y, 473, 156, 8, 4, ec, C_CELL); ui_focus(c == DF_RATE ? (mode == DM_GRID ? "EVERY" : "THRESHOLD") : fn2[c], v, c == DF_AMT ? amt : -1, ec); } else rrect(x, y, 473, 156, 8, C_CELL);
        ftext(x + 20, y + 12, FT_M, C_DIM, c == DF_RATE ? (mode == DM_GRID ? "EVERY" : "THRESHOLD") : fn2[c]);
        ftext(x + 453 - ftext_w(FT_XB, v), y + 10, FT_XB, use ? C_WHITE : C_DIM, v);
        if (c == DF_AMT || c == DF_LEN) { int f = c == DF_AMT ? amt : (dx[DX_LEN] % DK_NLEN) * 100 / (DK_NLEN - 1); rect(x + 20, y + 130, 433, 8, C_LINE, 1); rect(x + 20, y + 130, 433 * f / 100, 8, ec, 1); } }
    ui_cards_more(1004, PY + PH + 16, 332, first, (DF_N + 1) / 2);
    draw_footer(dk_row == 0 ? "EDIT+^v LEVEL  EDIT+<> DRAW  Y BACK  OPT+EDIT POINT UP  L1/R1 PAD" : "EDIT SHAPE  Y BACK  EDIT+ARROWS VALUE  OPT+EDIT DEFAULT  L1/R1 PAD");
    return;
}

// ---- SCENES (A02, HOME): a scene = one pattern number for every pad; a row each, a dot per pad that has notes in it ----
static int sc_cur;
static int scene_count(void) {           // up to the highest pattern any pad uses
    int top = 0; for (int s = 0; s < NSLOT; s++) if (has_sound(s)) for (int q = NPAT - 1; q > top; q--) if (pat_used(&song.t[s].pat[q])) { top = q; break; }
    return top + 1;
}
static void scene_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) act_scene(sc_cur - live_scene); return; }
    if (k == K_Y) { scening = 0; return; }
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_UP || k == K_DOWN) sc_cur = clampi(sc_cur + (k == K_DOWN ? 1 : -1), 0, scene_count() - 1);
}
static void draw_scenes(void) {
    int n = scene_count(); sc_cur = clampi(sc_cur, 0, n - 1);
    int top = clampi(sc_cur - 2, 0, n > 6 ? n - 6 : 0);
    for (int r = 0; r < 6 && top + r < n; r++) { int q = top + r, y = 80 + r * 104, on = q == sc_cur; char b[16];
        rrect(26, y, 971, 90, 8, on ? C_SELBG : C_CELL);
        snprintf(b, sizeof b, "SCENE %d", q + 1); ftext(48, y + 30, FT_LB, on ? C_WHITE : C_DIM, b);
        if (q == live_scene) ftext(22 + 4, y + 32, FT_MB, th_cur[TC_SMP], "\001");
        for (int s = 0; s < NSLOT; s++) { if (!has_sound(s)) continue; int x = 344 + s * 52, cy = y + 45, u = pat_used(&song.t[s].pat[q]);
            disc2(x, cy, u ? 8 : 7, u ? C_ENG(song.t[s].type) : C_LINE); } }
    if (n > 6) { int th = 6 * 104 - 14, tw = th * 6 / n; rect(1004, 80, 4, th, C_CELL, 1); rect(1004, 80 + (th - tw) * top / (n - 6), 4, tw, C_DIM, 1); }
    draw_footer("EDIT LAUNCH  Y BACK  F1/F2 HOME");
}

// ---- PROJECT (A24): the sections on the left, the settings of the one chosen as cards (the 1.2 page's jsec/jcol/jcur) ----
static void prj_value(int i, char *b);
static void draw_prj2(void) {
    Col ec = th_cur[TC_SMP]; int r[6], n = prj_rows(jsec, r);
    { int ok = 0; for (int i = 0; i < n; i++) if (r[i] == jcur) ok = 1; if (!ok) jcur = r[0]; }
    for (int k = 0; k < JS_N; k++) { int y = 76 + k * 70, on = k == jsec;
        rrect(26, y, 249, 60, 8, on ? (jcol ? mix(C_SELBG, C_CELL, 128) : C_SELBG) : C_CELL);
        ftext(48, y + 16, FT_MB, on ? C_WHITE : C_DIM, prj_sec_name[k]); }
    int ch = 144, dy = 156, cj = 0; for (int j = 0; j < n; j++) if (r[j] == jcur) cj = j;
    int first = jcol ? cj / 4 * 4 : 0;   // four cards a page, as A24
    if (n > 4) { rect(1000, 82, 4, 4 * dy - 12, C_CELL, 1); rect(1000, 82 + (first / 4) * (4 * dy - 12) / ((n + 3) / 4), 4, (4 * dy - 12) / ((n + 3) / 4), C_DIM, 1); }
    for (int j = first; j < n && j < first + 4; j++) { int i = r[j], y = 82 + (j - first) * dy, on = jcol && i == jcur; char b[32]; prj_value(i, b);
        if (on) { rframe(300, y, 690, ch, 8, 4, ec, C_CELL); ui_focus(prj_name[i], b, i == J_MASTER ? song.master : -1, ec); } else rrect(300, y, 690, ch, 8, C_CELL);
        ftext(320, y + 14, FT_M, C_DIM, prj_name[i]);
        int f = ftext_w(FT_XB, b) <= 640 - ftext_w(FT_M, prj_name[i]) ? FT_XB : FT_M;
        ftext(970 - ftext_w(f, b), y + 10, f, i == J_PROJ && prj_dirty() ? C_ORANGE : C_WHITE, b);
        if (i == J_TEMPO || i == J_SWING) { int v = i == J_TEMPO ? (song.tempo - 40) * 100 / 260 : song.swing * 100 / 75; rect(320, y + ch - 26, 650, 8, C_LINE, 1); rect(320, y + ch - 26, 650 * clampi(v, 0, 100) / 100, 8, ec, 1); }
        if (i == J_MASTER && ch >= 70) { rect(320, y + ch - 26, 650, 8, C_LINE, 1); rect(320, y + ch - 26, 650 * song.master / 100, 8, ec, 1); } }
    draw_footer(!jcol ? "EDIT ENTER  Y BACK  ^v SECTION" : "EDIT+<> VALUE  EDIT DO IT  Y BACK  < SECTIONS");
}

// ---- MIXER > RETURN A / RETURN B / MASTER: every parameter of the unit as cards (the 1.2 cursor: fcol the column, frow the row) ----
static int v2_unit(void) { return tab == T_FX && fx_unit >= NSLOT; }
static void draw_unit2(void) {
    int u = fx_unit, n = unit_nparams(u), half = (n + 1) / 2; Col ec = u == NSLOT + 2 ? th_cur[TC_SMP] : C_CAT(PC_MIX);
    static const char *const un[3] = {"A \003 REVERB", "B \003 DELAY", "MASTER"};
    for (int i = 0; i < 3; i++) { int x = 26 + i * 324, on = NSLOT + i == u; if (on) rrect(x, 70, 314, 34, 8, ec);
        ftext(x + 157 - ftext_w(FT_MB, un[i]) / 2, 75, FT_MB, on ? C_BG : C_DIM, un[i]); }
    int h = half <= 4 ? 136 : 108, dy = h + 12;
    for (int k = 0; k < n; k++) { int c = k >= half, r = c ? k - half : k, x = c ? 518 : 32, y = 118 + r * dy, on = fcol == c && frow == r, lo, hi, def; char b[16];
        int v = *unit_param(u, k, &lo, &hi, &def); unit_pstr(u, k, v, b);
        const char *nm = u == NSLOT ? rv_name[k] : u == NSLOT + 1 ? dl_name[k] : ms_name[k];
        if (on) { rframe(x, y, 473, h, 8, 4, ec, C_CELL); ui_focus(nm, b, hi - lo > 1 ? (v - lo) * 100 / (hi - lo) : -1, ec); } else rrect(x, y, 473, h, 8, C_CELL);
        ftext(x + 20, y + 12, FT_M, C_DIM, nm); ftext(x + 453 - ftext_w(FT_XB, b), y + 10, FT_XB, C_WHITE, b);
        if (hi - lo > 1) { rect(x + 20, y + h - 26, 433, 8, C_LINE, 1); rect(x + 20, y + h - 26, 433 * clampi(v - lo, 0, hi - lo) / (hi - lo), 8, ec, 1); } }
    draw_footer("EDIT+ARROWS VALUE  L1/R1 UNIT  Y BACK  EDIT ON/OFF  OPT+EDIT DEFAULT");
}

// ---- KEYS (MENU twice): the open pad on twelve buttons, the scale's degrees; the tiles of HOME, the notes on them ----
static void draw_keys2(void) {
    enum { GX = 26, GY = 86, CW = 230, CH = 140, DX = 247, DY = 152 };
    int n = track_nslices(sel), ts = track_scale(sel), tk = track_key(sel), sl = scale_len(ts); Col ec = C_ENG(song.t[sel].type); char b[40];
    for (int i = 0; i < NSLOT; i++) {
        int x = GX + (i % 4) * DX, y = GY + (i / 4) * DY, deg = i + koct * sl, tonic = ((deg % sl) + sl) % sl == 0, pc = (tk + deg_semis(ts, deg) + 120) % 12, on = key_on[i];
        rrect(x, y, CW, CH, 8, on ? ec : C_CELL); if (tonic && !on) rect(x, y, 9, CH, ec, 1);
        ftext(x + 25, y + 12, FT_M, on ? C_BG : C_DIM, pad_label[cell_key[i]]);
        if (n) snprintf(b, sizeof b, "S%d", ((i + koct * 12) % n + n) % n + 1); else snprintf(b, sizeof b, "%s", note_name[pc]);
        ftext(x + 25, y + 56, FT_XB, on ? C_BG : C_WHITE, b);
        if (!n) { snprintf(b, sizeof b, "%d", ((deg % sl) + sl) % sl + 1); ftext(x + CW - 20 - ftext_w(FT_M, b), y + 12, FT_M, on ? C_BG : tonic ? ec : C_DIM, b); }
    }
    for (int pc = 0; pc < 12; pc++) {    // the octave: the scale's notes lit, the tonic in the engine's colour, what sounds white
        int in = 0, on = 0, x = GX + pc * 81;
        for (int d = 0; d < sl; d++) if ((tk + deg_semis(ts, d)) % 12 == pc) in = 1;
        for (int i = 0; i < NSLOT; i++) if (key_on[i] && (tk + deg_semis(ts, i + koct * sl) + 120) % 12 == pc) on = 1;
        rrect(x, 556, 75, 56, 6, on ? C_WHITE : pc == tk ? ec : in ? C_DIM : C_CELL);
        ftext(x + 10, 574, FT_MB, on || in || pc == tk ? C_BG : C_DIM, note_name[pc]);
    }
    int own = song.t[sel].scale || song.t[sel].key;
    snprintf(b, sizeof b, "%s %s", note_name[tk], scale_name[ts]); int x = 26 + ftext(26, 636, FT_LB, C_WHITE, b) + 20;
    x += ftext(x, 640, FT_M, own ? ec : C_DIM, own ? "of the pad" : "of the project") + 30;
    snprintf(b, sizeof b, "octave %+d", koct); ftext(x, 640, FT_M, C_DIM, b);
    draw_footer("BUTTONS PLAY  MENU BACK  SHIFT+^v OCTAVE  SHIFT+<> SCALE  SHIFT+L1/R1 KEY  MENU+PLAY RECORD");
}
