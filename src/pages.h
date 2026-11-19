// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
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
    if (fx_is_mod(f->type)) c[n++] = (FxCard){9, 0};   // MODULATION: its MODE first
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
        case FX_FILTER: {                   // its MODE (2026-11-15, user: the curve stayed low-pass): LP, HP, BP, NOTCH
            // the state-variable response at w = 2^((t - fc) * 10) octaves around the cutoff, Q from RESO as fxdsp.h; 0 dB at 0.35, 12 dB per unit
            float fc = 0.15f + p1 * 0.7f, k = 2 - 2 * (f->p[2] / 100.f) * 0.98f, w = exp2f((t - fc) * 10), w2 = w * w;
            int md = f->p[0] % FILT_N, m = md == FILT_LP24 || md == FILT_LADDER ? 0 : md == FILT_HP24 ? 1 : md == FILT_BP24 ? 2 : md < 4 ? md : 0;
            if (md == FILT_DJ) { float dj = (f->p[1] - 50) / 50.f; m = dj < 0 ? 0 : 1; fc = dj < 0 ? 0.95f + dj * 0.85f : 0.05f + dj * 0.85f; if (fabsf(dj) < 0.02f) fc = -5; w = exp2f((t - fc) * 10); w2 = w * w; }
            float den = (1 - w2) * (1 - w2) + k * k * w2, num = m == 1 ? w2 * w2 : m == 2 ? k * k * w2 : m == 3 ? (1 - w2) * (1 - w2) : 1;
            float db = 10 * log10f(num / den + 1e-9f); if (md >= FILT_LP24 && md <= FILT_BP24) db *= 2; if (md == FILT_LADDER) db = db * 2 - 3;
            if (md == FILT_PEAK) db = 20 * log10f(1 + 3 * k * w / sqrtf(den));   // 1 + 3 x the unity band-pass
            if (md == FILT_COMB) db = 9 * cosf((t - fc) * 6.2832f * 6) * (0.4f + (f->p[2] / 100.f) * 0.6f);
            if (md == FILT_VOWEL) { float c0 = 0.12f + p1 * 0.1f, c1 = 0.4f + p1 * 0.1f, c2 = 0.62f; db = -24 + 30 * (expf(-powf((t - c0) * 30, 2)) + 0.7f * expf(-powf((t - c1) * 30, 2)) + 0.45f * expf(-powf((t - c2) * 30, 2))); }
            v = 0.35f + db / 48; if (v > 0.95f) v = 0.95f; if (v < -0.9f) v = -0.9f; break; }
        case FX_RING: v = sinf(t * 6.2832f * 3) * sinf(t * 6.2832f * (3 + p0 * 12)); break;
        case FX_EQ: { static float ec[5][5]; static int en; if (i == 0) en = eq_coefs(f->p, ec, 48000);   // the real response, 20 Hz .. 20 kHz, +-18 dB
            v = eq_mag_db(ec, en, 20 * exp2f(t * 9.97f), 48000) / 18; if (v > 0.95f) v = 0.95f; if (v < -0.95f) v = -0.95f; break; }
        case FX_AUTOPAN: v = f->p[2] == 2 ? (fmodf(t * 4, 1) < 0.5f ? 0.8f : -0.8f) : f->p[2] == 1 ? 1.6f * fabsf(fmodf(t * 4, 1) - 0.5f) * 2 - 0.8f : 0.8f * sinf(t * 6.2832f * 2); break;
        case FX_REVERB: { float e = expf(-t * (6 - p1 * 5)); v = e * 0.85f * sinf(t * 6.2832f * 23) * sinf(t * 6.2832f * 7.3f); break; }   // a tail that dies away, longer with DECAY
        case FX_DELAY: { int n = 2 + f->p[0] / 2; float ph = fmodf(t * n, 1), e = powf(0.15f + 0.8f * p1, floorf(t * n)); v = ph < 0.12f ? e * 0.85f * sinf(ph / 0.12f * 3.1416f) : 0; break; }   // echoes, each quieter by FEEDBACK
        default: v = (0.3f + 0.6f * p1) * sinf(t * 6.2832f * (1 + p0 * 3));   // CHORUS FLANGER PHASER: the sweep
        }
        int nx = x + i, ny = y + h / 2 - (int)(v * (h / 2 - 14));
        if (i) line(px, py, nx, ny, 3, f->on ? c : C_DIM);
        px = nx; py = ny;
    }
}
static void fx2_live(int x, int y, int w, int h, const FxSlot *f, Col c) {   // what the insert does now (1.2's FX screen), over the picture
    char b[48]; int u = fx_budget_used();       // what the inserts take of the processor when all sound; the limit is 40 %
    snprintf(b, sizeof b, "FX LOAD %d%% / %d%%", (u + 5) / 10, FX_BUDGET / 10); rrect(x - 6, y + 6, ftext_w(FT_S, b) + 12, 28, 4, C_CELL); ftext(x, y + 10, FT_S, u >= FX_BUDGET ? C_RED : C_DIM, b);
    if (!f->type) return;
    if (f->on && !ui_fx_live(fx_unit, fx2_slot)) { const char *m = "OVER BUDGET: NOT RUNNING"; int mw = ftext_w(FT_MB, m) + 24;
        rrect(x + w - mw, y + 8, mw, 32, 6, C_RED); ftext(x + w - mw + 12, y + 12, FT_MB, C_BG, m); return; }
    int live = f->on && engine_playing(); if (!live) return;
    float v[6]; engine_fx_view(fx_unit, fx2_slot, v);
    if (f->type == FX_COMP) {                    // the gain reduction now
        float gr = v[1] > 24 ? 24 : v[1] < 0 ? 0 : v[1]; int bw = 300, bx = x + w - bw;
        snprintf(b, sizeof b, "GAIN REDUCTION %.1f DB", gr > 0.05f ? -gr : 0.f); rrect(bx - 8, y + 4, bw + 16, 50, 4, C_CELL); ftext(bx, y + 10, FT_S, gr > 0.1f ? C_ORANGE : C_DIM, b);
        rect(bx, y + 36, bw, 10, C_LINE, 1); rect(bx + bw - (int)(bw * gr / 24), y + 36, (int)(bw * gr / 24), 10, C_ORANGE, 1);
    } else if (f->type == FX_FILTER && v[0] > 0) {   // the cutoff now (the LFO moves it), on the picture's scale
        float p = log2f(v[0] / 20) / 10; if (p < 0) p = 0; if (p > 1) p = 1;
        int lx = x + (int)((0.15f + p * 0.7f) * w); rect(lx, y + 12, 3, h - 24, C_WHITE, 1);
        if (v[0] < 1000) snprintf(b, sizeof b, "%.0f HZ", v[0]); else snprintf(b, sizeof b, "%.1f KHZ", v[0] / 1000);
        rrect(x + w - ftext_w(FT_S, b) - 8, y + 6, ftext_w(FT_S, b) + 12, 28, 4, C_CELL); ftext(x + w - ftext_w(FT_S, b), y + 10, FT_S, C_WHITE, b);
    } else if (f->type == FX_MULTI) {            // each band: its level in, the gain it gets now
        static const char *const bn[3] = {"LOW", "MID", "HIGH"}; int bw = 200;
        for (int k = 0; k < 3; k++) { int bx = x + w - (3 - k) * (bw + 16); float lv = v[k] < -72 ? -72 : v[k] > 0 ? 0 : v[k], g = v[3 + k];
            snprintf(b, sizeof b, "%s %+.0f DB", bn[k], g); rrect(bx - 6, y + 4, bw + 12, 48, 4, C_CELL); ftext(bx, y + 10, FT_S, g > 0.5f ? C_ORANGE : C_DIM, b);
            rect(bx, y + 36, bw, 8, C_LINE, 1); rect(bx, y + 36, (int)((lv + 72) / 72 * bw), 8, c, 1); }
    }
}
static void fx2_card(int x, int y, int w, int h, const FxSlot *f, FxCard cd, int on, Col ec) {
    char v[24]; const char *lb; int val, lo = 0, hi = 1;
    if (cd.row == 9) { lb = "MODE"; val = f->type - FX_CHORUS; hi = 2; snprintf(v, sizeof v, "%s", mod_mode_name[clampi(val, 0, 2)]); }
    else if (cd.row == 1) { lb = "ON"; snprintf(v, sizeof v, "%s", f->on ? "ON" : "OFF"); val = f->on; }
    else if (cd.side) { int8_t *g = fx_side(fx2_slot, cd.row, &lo, &hi); val = *g;
        if (f->type == FX_COMP) { lb = "KEY"; if (val) snprintf(v, sizeof v, "PAD %d", val); else snprintf(v, sizeof v, "SELF"); }
        else if (f->type == FX_RING) { lb = "SOURCE"; if (val) snprintf(v, sizeof v, "PAD %d", val); else snprintf(v, sizeof v, "OSC"); }   // 2026-11-19 (user): any sound modulates
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
        if (fx_is_mod(f->type)) snprintf(b, sizeof b, "%d \003 MODULATION \003 %s", s + 1, fx_name[f->type]);
        else snprintf(b, sizeof b, "%d \003 %s", s + 1, f->type ? fx_name[f->type] : "EMPTY");
        rrect(x, 76, 479, 56, 6, on ? ec : C_CELL);
        ftext(x + 240 - ftext_w(FT_MB, b) / 2, 92, FT_MB, on ? C_BG : f->type && f->on ? C_WHITE : C_DIM, b);
    }
    const FxSlot *f = &pm->ins[fx2_slot]; FxCard c[16]; int n = fx2_cards(c);
    int rows = (n + 1) / 2, ph = 186, h = 162, dy = 174, y0 = 153 + ph + 16;   // four cards, the rest by pages
    if (fx2_cur < 0) rframe(26, 153, 971, ph, 8, 4, ec, C_CELL); else rrect(26, 153, 971, ph, 8, C_CELL);
    fx2_picture(46, 153, 931, ph, f, ec);
    fx2_live(46, 153, 931, ph, f, ec);
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
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) { used[K_EDIT] = used[K_OPT] = 1;   // the four LFOs, the automation of every pattern, the duck
        static const int part[3] = {PP_LFO, PP_AUTO, PP_DUCK}; static const char *const what[3] = {"THE 4 LFOS", "ALL AUTOMATION", "THE DUCK"};
        cfm_pad(sel, part[clampi(mm_cur, 0, 2)], what[clampi(mm_cur, 0, 2)]); return; }
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
    draw_footer("EDIT OPEN  F1/F2 PAD  Y BACK  OPT+EDIT CLEAR");
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
      if (act) { float v = engine_lfo(fx_unit, lcol); disc2(986, 227 - (int)(v * 80), 6, C_WHITE); }   // where it is now
      else if (!L->target) ui_empty(206, 160, 611, 134, "NO TARGET YET", "TARGET: EDIT, then EDIT on a SOUND card"); }   // empty state (TZ)
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

// ---- MIXER (HOME): the faders (A21) and a drawer at the side: returns, master, the pad's effects. L1/R1: closed, open, FREEZE (A23) ----
static int mx_pg, mx_r, mx_fz;            // 0 the drawer closed, 1 open, 2 FREEZE; in the drawer: the row of a unit (0 level, 1-2 two parameters); FREEZE: the row
static int v2_mixer(void) { return ui_cat < 0 && tab == T_MIX && !ducking && !perform; }
static const int8_t mx_ret_k[3][2] = {{RV_SIZE, RV_DECAY}, {DL_TIME, DL_FB}, {MS_COMP, MS_LIM}};
static void mixer2_fix(void) {            // the cursor is on a pad, or on a unit of the open drawer
    if (mcol > NSLOT + 2 || (mx_pg != 1 && mcol >= NSLOT)) mcol = sel; mx_r = clampi(mx_r, 0, 2);
}
static void mixer2_page(int d) { mx_pg = (mx_pg + d + 3) % 3; mx_r = 0; mrow = MX_VOL; if (mx_pg == 1) mcol = ui_drawer_left ? NSLOT + 2 : NSLOT; else if (mx_pg == 0) mcol = sel; else mx_fz = 0; }
static void mixer2_move(int k) {
    mixer2_fix();
    if (mx_pg != 2) {                     // one line: the twelve pads and, at the drawer's side, its three units; stepping past the last pad opens it
        if (k == K_LEFT || k == K_RIGHT) { int p = (ui_drawer_left ? (mcol >= NSLOT ? mcol - NSLOT : mcol + 3) : mcol) + (k == K_RIGHT ? 1 : -1);
            if (p < 0 || p > NSLOT + 2) return;
            mcol = ui_drawer_left ? (p < 3 ? p + NSLOT : p - 3) : p;
            if (mcol < NSLOT) sel = mcol; else if (mx_pg == 0) { mx_pg = 1; mx_r = 0; } }
        else if (mcol >= NSLOT) { if (k == K_UP && mx_r) mx_r--; else if (k == K_DOWN && mx_r < 2) mx_r++; }
        mrow = mcol >= NSLOT && mx_r ? MX_PAN : MX_VOL; return; }
    int n = 0; for (int s = 0; s < NSLOT; s++) n += has_sound(s);
    if (k == K_UP && mx_fz) mx_fz--; else if (k == K_DOWN && mx_fz < n) mx_fz++;
}
static int mixer2_fz_pad(void) { int n = 0; for (int s = 0; s < NSLOT; s++) if (has_sound(s)) { if (n == mx_fz) return s; n++; } return -1; }   // -1: EXPORT
static void mixer2_arrow(int k) {         // EDIT+arrows
    int d = k == K_RIGHT || k == K_UP ? 1 : -1, big = k == K_UP || k == K_DOWN; mixer2_fix();
    if (mx_pg != 2 && mcol < NSLOT) { act_pad_volume_step(mcol, d * (big ? 5 : 1)); return; }
    if (mx_pg == 1) { int u = mcol - NSLOT;
        if (mx_r == 0) { if (u == 2) act_master_step(d * (big ? 5 : 1)); else act_unit_param_add(mcol, u ? DL_LEVEL : RV_LEVEL, d * (big ? 5 : 1)); }
        else if (u == 2) act_master_fx_toggle(mx_ret_k[2][mx_r - 1]);
        else act_unit_param_add(mcol, mx_ret_k[u][mx_r - 1], d * (big && !(u == 1 && mx_r == 1) ? 5 : 1)); }
}
static int mixer2_tap(void) {             // EDIT: on a pad opens or closes the drawer (2026-11-14, user), on a unit its every parameter; FREEZE freezes, or exports
    mixer2_fix();
    if (mx_pg != 2 && mcol < NSLOT) { mx_pg = !mx_pg; mx_r = 0; mrow = MX_VOL; return 1; }
    if (mx_pg == 1 && mcol >= NSLOT) { fx_unit = mcol; frow = fcol = 0; set_tab(T_FX); return 1; }
    if (mx_pg == 2) { int s = mixer2_fz_pad(); if (s < 0) { export_open(); return 1; } freeze_set(s, !frozen(s)); if (!frozen(s)) toast("UNFROZEN"); return 1; }
    return 0;
}
static void mx_db(char *b, float lin) { if (lin < 0.001f) snprintf(b, 8, "-INF"); else snprintf(b, 8, "%+.1f", 20 * log10f(lin)); }   // b: 8 chars
static void mx_ctext(int cx, int y, int f, Col c, const char *s) { ftext(cx - ftext_w(f, s) / 2, y, f, c, s); }
static void mx_strip(int s, int cx, int hw) {   // a pad: icon, its two inserts, sends A B (PROJECT > MIXER > STRIP EFFECTS), the fader and the meter, the fader in dB, the level now in dB
    PadMix *pm = &song.mix.pad[s]; int on = s == mcol, has = has_sound(s), fx = ui_mxfx, top = fx ? 208 : 128, bot = 598, h = (bot - top) * clampi(song.t[s].vol, 0, 100) / 100; char b[8];
    Col c = has ? (on ? C_ORANGE : C_ENG(song.t[s].type)) : C_LINE;
    if (on) rframe(cx - hw, 76, hw * 2, 638, 8, 3, C_WHITE, C_BG);
    rect(cx - 10, top, 14, bot - top, C_CELL, 1);
    snprintf(b, sizeof b, "%02d", s + 1); mx_ctext(cx, 650, FT_MB, on ? C_WHITE : C_DIM, b);
    if (!has) return;
    icon(slot_icon(s), cx - 16, 84, song.t[s].mute ? C_DIM : pad_icol(s, on ? C_WHITE : C_GREY));
    for (int i = 0; i < 2 && fx; i++) { int ty = pm->ins[i].type < FX_N ? pm->ins[i].type : 0; mx_ctext(cx, 120 + i * 20, FT_S, ty && pm->ins[i].on ? C_CAT(PC_FX) : C_DIM, fx_short[ty]); }
    for (int i = 0; i < 2 && fx; i++) { int v = clampi(i ? pm->send_b : pm->send_a, 0, 100), y = 164 + i * 20, w = hw * 2 - 28;
        ftext(cx - hw + 6, y, FT_S, v ? C_WHITE : C_DIM, i ? "B" : "A"); rect(cx - hw + 20, y + 8, w, 5, C_LINE, 1); rect(cx - hw + 20, y + 8, w * v / 100, 5, C_CAT(PC_MIX), 1); }
    rect(cx - 10, bot - h, 14, h, c, 1); rect(cx - 18, bot - h - 5, 28, 10, C_WHITE, 1);
    meter_bar(cx + 13, top, 7, bot - top, mx_show[s]);
    mx_db(b, song.t[s].vol / 100.f); mx_ctext(cx, 606, FT_S, on ? C_WHITE : C_GREY, b);
    if (on) { char nn[24]; snprintf(nn, sizeof nn, "%02d VOLUME DB", s + 1); ui_focus(nn, b, song.t[s].vol, c); }
    if (mx_show[s] >= 0.001f) { mx_db(b, mx_show[s]); mx_ctext(cx, 626, FT_S, C_PLAY, b); }
    if (song.t[s].mute) mx_ctext(cx, 678, FT_MB, C_RED, "M"); else if (pm->solo) mx_ctext(cx, 678, FT_MB, C_ORANGE, "S");
}
static void mx_tab(void) {                // the closed drawer: a tab at the edge with the master's meter
    int tx = ui_drawer_left ? -8 : 980, ox = ui_drawer_left ? 2 : 982; char b[8];
    rframe(tx, 296, 52, 270, 8, 2, C_ORANGE, C_BG);
    ftext(ox + 14, 302, FT_S, C_ORANGE, ui_drawer_left ? "\004" : "\005");
    meter_bar(ox + 12, 332, 14, 184, mx_show[NSLOT + 2]);
    ftext(ox + 4, 520, FT_S, C_WHITE, "MST");
    if (mx_show[NSLOT + 2] >= 0.001f) { snprintf(b, sizeof b, "%+d", (int)lrintf(20 * log10f(mx_show[NSLOT + 2]))); ftext(ox + 4, 540, FT_S, C_PLAY, b); }
}
static void mx_drawer(int dx) {           // open: REVERB, DELAY, MASTER as faders, two parameters each, and what the pad under the cursor has
    static const char *const cn[3] = {"REVERB", "DELAY", "MASTER"}, *const sn[3] = {"A", "B", "MST"}, *const pn[3][2] = {{"SIZE", "DECAY"}, {"TIME", "FEEDBACK"}, {"COMP", "LIMITER"}};
    int lo, hi, def, tx = ui_drawer_left ? dx + 318 : dx - 22; char b[48];
    rframe(tx, 330, 24, 90, 6, 2, C_ORANGE, C_BG); ftext(tx + 7, 364, FT_S, C_ORANGE, ui_drawer_left ? "\005" : "\004");
    rframe(dx, 70, 320, 646, 8, 2, C_ORANGE, C_BG);
    for (int u = 0; u < 3; u++) { int cx = dx + 56 + u * 104, on = mcol == NSLOT + u, top = 126, bot = 380; Col c = u == 2 ? th_cur[TC_SMP] : C_CAT(PC_MIX);
        int lv = u == 2 ? song.master : *unit_param(NSLOT + u, u ? DL_LEVEL : RV_LEVEL, &lo, &hi, &def), fh = (bot - top) * clampi(lv, 0, 100) / 100;
        if (on && mx_r == 0) rframe(cx - 48, 76, 96, 356, 6, 3, C_WHITE, C_BG);
        mx_ctext(cx, 82, FT_S, on ? C_WHITE : C_GREY, cn[u]); mx_ctext(cx, 102, FT_S, C_DIM, sn[u]);
        rect(cx - 14, top, 18, bot - top, C_CELL, 1); rect(cx - 14, bot - fh, 18, fh, c, 1); rect(cx - 22, bot - fh - 5, 34, 10, C_WHITE, 1);
        meter_bar(cx + 16, top, 8, bot - top, mx_show[NSLOT + u]);
        mx_db(b, lv / 100.f); mx_ctext(cx, 386, FT_S, C_WHITE, b);
        if (on && mx_r == 0) ui_focus(cn[u], b, clampi(lv, 0, 100), c);
        if (mx_show[NSLOT + u] >= 0.001f) { mx_db(b, mx_show[NSLOT + u]); mx_ctext(cx, 406, FT_S, C_PLAY, b); }
        for (int r = 0; r < 2; r++) { int k = mx_ret_k[u][r], x = dx + 16 + r * 148, y = 442 + u * 24, cur = on && mx_r == r + 1; char pv[16];
            if (u == 2) snprintf(pv, sizeof pv, "%s", song.mix.ms[k] ? "ON" : "OFF"); else unit_pstr(NSLOT + u, k, *unit_param(NSLOT + u, k, &lo, &hi, &def), pv);
            snprintf(b, sizeof b, "%s %s", pn[u][r], pv); if (cur) { rrect(x - 6, y - 1, ftext_w(FT_S, b) + 12, 23, 4, C_SELBG); ui_focus(pn[u][r], pv, -1, c); }
            ftext(x, y, FT_S, cur ? C_WHITE : C_DIM, b); } }
    if (engine_master_gr() > 0.05f) { snprintf(b, sizeof b, "GR %.1f DB", -engine_master_gr()); ftext(dx + 304 - ftext_w(FT_S, b), 512, FT_S, C_DIM, b); }   // the limiter/comp now
    rect(dx + 12, 538, 296, 1, C_LINE, 1);
    int s = mcol < NSLOT ? mcol : sel; PadMix *pm = &song.mix.pad[s];
    snprintf(b, sizeof b, "%02d %.12s", s + 1, has_sound(s) ? (slot_name(s) ? slot_name(s) : "") : "EMPTY"); int w = ftext(dx + 16, 546, FT_MB, C_ORANGE, b);
    if (!has_sound(s)) return;
    ftext(dx + 28 + w, 549, FT_S, C_DIM, type_name[song.t[s].type]);
    for (int i = 0; i < 5; i++) { static const char *const rn[5] = {"INSERT 1", "INSERT 2", "SEND A", "SEND B", "DUCK"}; int y = 578 + i * 26; Col vc = C_DIM;
        if (i < 2) { int ty = pm->ins[i].type < FX_N ? pm->ins[i].type : 0; snprintf(b, sizeof b, "%s", ty ? fx_name[ty] : "---"); if (ty && pm->ins[i].on) vc = C_CAT(PC_FX); else if (ty) strcat(b, " OFF"); }
        else if (i < 4) { int v = i == 2 ? pm->send_a : pm->send_b; snprintf(b, sizeof b, "%d%%", v); if (v) vc = C_CAT(PC_MIX);
            rect(dx + 112, y + 8, 130, 5, C_LINE, 1); rect(dx + 112, y + 8, 130 * clampi(v, 0, 100) / 100, 5, C_CAT(PC_MIX), 1); }
        else if (pm->spare[DK_SRC]) { snprintf(b, sizeof b, "PAD %d %d%%", pm->spare[DK_SRC], pm->spare[DK_AMT]); vc = C_ORANGE; } else snprintf(b, sizeof b, "OFF");
        ftext(dx + 16, y, FT_S, C_GREY, rn[i]); ftext(dx + 304 - ftext_w(FT_S, b), y, FT_S, vc, b); }
}
static void draw_mixer2(void) {
    mixer2_fix();
    mx_meters();
    if (mx_pg == 0 && !any_sound()) { ui_empty(26, 200, 971, 300, "NOTHING TO MIX YET", "PADS: EDIT on an empty pad picks an engine"); draw_footer("Y BACK  L1/R1 DRAWER, FREEZE"); return; }
    if (mx_pg != 2) {                     // the drawer lies over the strips (2026-11-14, user: nothing narrows); they slide so the pad stays in view
        static int off; int open = mx_pg == 1, cs = mcol < NSLOT ? mcol : sel, vis = open ? 8 : NSLOT;
        if (cs < off) off = cs; if (cs >= off + vis) off = cs - vis + 1; off = clampi(off, 0, NSLOT - vis);
        int x0 = open && ui_drawer_left ? 387 : ui_drawer_left ? 90 : 50;
        for (int s = 0; s < NSLOT; s++) mx_strip(s, x0 + (s - off) * 78, 37);
        if (open) mx_drawer(ui_drawer_left ? 0 : 704); else mx_tab();
        draw_footer(!open ? "EDIT DRAWER  EDIT+^v LEVEL  Y MUTE  X SOLO  L1/R1 FREEZE" : mcol < NSLOT ? "EDIT CLOSE  EDIT+^v LEVEL  Y MUTE  X SOLO"
                    : "EDIT ALL PARAMETERS  EDIT+^v VALUE  Y CLOSE  <> UNIT, PADS");
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

// ---- SCENES (A02, HOME): a row per scene, each its own twelve pads (a dot per pad with a sound, bigger with notes); the last row makes one ----
static int sc_cur;
static int scene_count(void) { return scn_n; }
static void scene_key(int k, int down) {
    int rows = scn_n + (scn_n < NSCENE);
    if (!down) {
        if (k == K_EDIT && !used[K_EDIT]) {
            if (sc_cur >= scn_n) { scene_new(-1); sc_cur = scn_n - 1; }
            else if (sc_cur == scn_cur && scn_next < 0) toast("THIS SCENE IS OPEN");
            else scene_go(sc_cur); }
        return; }
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) {   // the scene goes, after YES (2026-11-17: was twice)
        used[K_EDIT] = used[K_OPT] = 1; if (sc_cur < scn_n) cfm_scene(sc_cur); return; }
    if (k == K_X) { scene_new(sc_cur >= scn_n ? scn_cur : sc_cur); sc_cur = scn_n - 1; return; }   // a copy: of this one, or of the open one
    if (k == K_Y) { scening = 0; return; }
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_UP || k == K_DOWN) sc_cur = clampi(sc_cur + (k == K_DOWN ? 1 : -1), 0, rows - 1);
}
static void draw_scenes(void) {
    int n = scn_n, rows = n + (n < NSCENE); sc_cur = clampi(sc_cur, 0, rows - 1);
    int top = clampi(sc_cur - 2, 0, rows > 6 ? rows - 6 : 0);
    for (int r = 0; r < 6 && top + r < rows; r++) { int q = top + r, y = 80 + r * 104, on = q == sc_cur; char b[24];
        rrect(26, y, 971, 90, 8, on ? C_SELBG : C_CELL);
        if (q >= n) { const char *h = "empty pads"; ftext(48, y + 30, FT_LB, on ? C_WHITE : C_DIM, "+ NEW SCENE"); ftext(974 - ftext_w(FT_M, h), y + 32, FT_M, C_DIM, h); continue; }
        const Song *g = q == scn_cur ? &song : scn[q];
        snprintf(b, sizeof b, "SCENE %d", q + 1); ftext(48, y + 30, FT_LB, on ? C_WHITE : C_DIM, b);
        if (q == scn_cur) ftext(22 + 4, y + 32, FT_MB, th_cur[TC_SMP], "\001");
        else if (q == scn_next && (SDL_GetTicks() / 250) % 2) ftext(22 + 4, y + 32, FT_MB, C_ORANGE, "\001");   // waits for the bar
        for (int s = 0; s < NSLOT && g; s++) { int has = q == scn_cur ? has_sound(s) : g->t[s].type || (scn_ready[q] ? scn_smp[q][s] != NULL : g->t[s].path[0] != 0), x = 344 + s * 52, cy = y + 45;
            int u = has && pat_used(&g->t[s].pat[clampi(g->t[s].cur, 0, NPAT - 1)]);
            disc2(x, cy, u ? 9 : has ? 6 : 3, has ? C_ENG(g->t[s].type) : C_LINE); } }
    if (rows > 6) { int th = 6 * 104 - 14, tw = th * 6 / rows; rect(1004, 80, 4, th, C_CELL, 1); rect(1004, 80 + (th - tw) * top / (rows - 6), 4, tw, C_DIM, 1); }
    draw_footer(sc_cur >= n ? "EDIT NEW EMPTY  X COPY OF THE OPEN ONE  Y BACK" : "EDIT LAUNCH  X COPY  OPT+EDIT DELETE  Y BACK  F1/F2 HOME");
}

// ---- PROJECT > INTERFACE > PAD MENU = TREE (the mockup's map, A-04): the hub drawn as the whole app: HOME's screens, the pad,
// its categories, the pages of each, joined by curves. <> change the level, ^v choose on it, EDIT opens what the cursor is on:
// a page of level 3 opens straight, without its list. tr_lv: 0 HOME, 1 the pad, 2 the category (hub_cur), 3 its pages (tr_pg).
static const char *const tr_home[4] = {"PADS", "MIXER", "SCENES", "PROJECT"};
static int tree_pages(int c, const char *nm[8]) {
    switch (c) {
    case PC_PATTERN: nm[0] = cur_pat(sel)->mode ? "ROLL" : "STEPS"; nm[1] = "SELECT"; nm[2] = "RECORD"; return 3;
    case PC_SOUND: { int n = snd_page_n(); if (n > 8) n = 8; for (int i = 0; i < n; i++) nm[i] = snd_page_nm(i); return n; }
    case PC_FX: nm[0] = "INSERT 1"; nm[1] = "INSERT 2"; return 2;
    case PC_MOD: nm[0] = "LFO 1-4"; nm[1] = "AUTOMATION"; nm[2] = "DUCK"; return 3;
    case PC_MIX: nm[0] = "VOLUME"; nm[1] = "PAN"; nm[2] = "SEND A"; nm[3] = "SEND B"; nm[4] = "FREEZE"; return 5;
    }
    nm[0] = "ICON"; nm[1] = "COLOUR"; return 2;
}
static int tree_page_now(int c) {         // the page of a category that was open last
    const char *nm[8]; int n = tree_pages(c, nm), i = c == PC_PATTERN ? pm_cur : c == PC_SOUND ? sp_pg[song.t[sel].type & 3] : c == PC_FX ? fx2_slot : c == PC_MOD ? mm_cur : 0;
    return clampi(i, 0, n - 1);
}
static void tree_open(int c, int i) {
    switch (c) {
    case PC_PATTERN: pm_cur = i; cat_open(PC_PATTERN); break;
    case PC_SOUND: cat_open(PC_SOUND); snd_page_go(i); break;
    case PC_FX: cat_open(PC_FX); fx2_slot = i; fx2_cur = -1; fx2_sync(); break;
    case PC_MOD: mm_cur = i;
        if (i == 0) { set_tab(T_FX); ui_cat = hub_cur = PC_MOD; fx_unit = sel; fx_page = 1; }
        else if (i == 1) { set_tab(T_SEQ); ui_cat = hub_cur = PC_MOD; if (!is_auto(lane)) lane = L_A1; set_step(0); seq_clamp(); }
        else { modmenu_open(); duck_open(sel); }
        break;
    case PC_MIX: cat_open(PC_MIX); mix2_cur = i; mrow = mix2_rows[i]; break;
    default: icon_open(); if (i) ic_pal = track_icol(&song.t[sel]); break;
    }
}
static void tree_key(int k) {
    const char *nm[8]; int d = k == K_DOWN ? 1 : -1;
    tr_lv = clampi(tr_lv, 0, 3); hub_cur = clampi(hub_cur, 0, HUB_N - 1);
    if (k == K_LEFT) { if (tr_lv == 3 && tr_pg > 0) tr_pg--; else if (tr_lv > 0) tr_lv--; if (tr_lv == 0) tr_0 = 0; return; }
    if (k == K_RIGHT) {
        if (tr_lv == 3) { if (tr_pg < tree_pages(hub_cur, nm) - 1) tr_pg++; }
        else { if (tr_lv == 0) tr_0 = 0; tr_lv++; if (tr_lv == 3) tr_pg = tree_page_now(hub_cur); }   // only PADS leads on to the pad
        return; }
    if (tr_lv == 0) tr_0 = clampi(tr_0 + d, 0, 3);
    else if (tr_lv == 1) pad_step(d);
    else { hub_cur = (hub_cur + d + HUB_N) % HUB_N; tr_pg = tree_page_now(hub_cur); }
}
static void tree_tap(void) {
    if (tr_lv == 0) { tr_lv = 2;
        if (tr_0 == 3) { tab_project(); return; }   // F1+F2 or Y there comes back to the tree
        set_tab(tr_0 == 1 ? T_MIX : T_GRID); ui_cat = -1; scening = tr_0 == 2; if (tr_0 == 1) mcol = sel; }
    else if (tr_lv == 1) tr_lv = 2;
    else if (tr_lv == 2) hub_go();
    else tree_open(hub_cur, tr_pg);
}
static void tree_link(int x0, int y0, int x1, int y1, Col c) {   // a curve that leaves and arrives level
    int px = x0, py = y0;
    for (int i = 1; i <= 16; i++) { float t = i / 16.f, u = 1 - t, by = u * u * u * y0 + 3 * u * u * t * y0 + 3 * u * t * t * y1 + t * t * t * y1;
        int x = x0 + (x1 - x0) * i / 16, y = (int)(by + 0.5f); line(px, py, x, y, 2, c); px = x; py = y; }
}
static void tree_block(int x, int y, int w, int h, Col stripe, int cur, int way) {   // cur: the cursor is here; way: it is on the way to the cursor
    if (cur) rframe(x - 3, y - 3, w + 6, h + 6, 9, 3, C_WHITE, way ? mix(C_SELBG, C_DIM, 40) : C_CELL);
    else rrect(x, y, w, h, 7, way ? mix(C_SELBG, C_DIM, 40) : C_CELL);
    rect(x, y + 3, 6, h - 6, stripe, 1);
}
static void draw_tree(void) {
    enum { X0 = 18, W0 = 152, X1 = 216, W1 = 164, X2 = 424, W2 = 190, X3 = 656, W3 = 350, YR = 106, DY = 100, BH = 70, PY = 340, PH = 100 };
    static const char *const lv[4] = {"LEVEL 0 \003 HOME", "LEVEL 1 \003 PAD", "LEVEL 2 \003 CATEGORY", "LEVEL 3 \003 PAGES"}; static const int lx[4] = {X0, X1, X2, X3};
    const Track *t = &song.t[sel]; char b[64]; const char *nm[8];
    tr_lv = clampi(tr_lv, 0, 3); hub_cur = clampi(hub_cur, 0, HUB_N - 1);
    for (int i = 0; i < 4; i++) ftext(lx[i], 76, FT_S, i == tr_lv ? C_WHITE : C_DIM, lv[i]);
    // the links first, under the blocks: PADS to the pad, the pad to its categories, a category to its pages
    tree_link(X0 + W0, YR + BH / 2, X1, PY + PH / 2, tr_lv >= 1 || !tr_0 ? C_WHITE : C_GREY);
    for (int c = 0; c < HUB_N; c++) { int y = YR + c * DY + BH / 2, on = tr_lv >= 2 && c == hub_cur;
        if (!on) { tree_link(X1 + W1, PY + PH / 2, X2, y, C_GREY); line(X2 + W2, y, X3, y, 2, C_GREY); } }
    if (tr_lv >= 2) { int y = YR + hub_cur * DY + BH / 2; tree_link(X1 + W1, PY + PH / 2, X2, y, C_WHITE); line(X2 + W2, y, X3, y, 2, tr_lv == 3 ? C_WHITE : C_GREY); }
    for (int i = 0; i < 4; i++) { int y = YR + i * DY, cur = tr_lv == 0 && i == tr_0;
        tree_block(X0, y, W0, BH, i == 0 ? th_cur[TC_SMP] : i == 1 ? C_CAT(PC_MIX) : i == 2 ? C_PLAY : C_GREY, cur, !cur && i == 0 && tr_lv > 0);
        ftext(X0 + 18, y + 10, FT_MB, C_WHITE, tr_home[i]); }
    tree_block(X1, PY, W1, PH, C_ENG(t->type), tr_lv == 1, tr_lv > 1);
    snprintf(b, sizeof b, "%02d %.9s", sel + 1, slot_name(sel) ? slot_name(sel) : "EMPTY"); ftext(X1 + 18, PY + 10, FT_MB, C_WHITE, b);
    ftext(X1 + 18, PY + 38, FT_S, C_ENG(t->type), type_name[t->type & 3]);
    ftext(X1 + 18, PY + PH - 28, FT_S, C_DIM, "any of 12 pads");
    for (int c = 0; c < HUB_N; c++) { int y = YR + c * DY, on = c == hub_cur && tr_lv >= 2, n = tree_pages(c, nm), x = X3 + 18, yy = y + (n > 3 ? 11 : 24); Col cc = hub_col(c);
        tree_block(X2, y, W2, BH, cc, on && tr_lv == 2, on && tr_lv == 3);
        ftext(X2 + 18, y + 10, FT_MB, C_WHITE, hub_name(c));
        if (c == HUB_LOOK) icon(slot_icon(sel), X2 + W2 - 44, y + 19, pad_icol(sel, C_ENG(t->type)));
        tree_block(X3, y, W3, BH, cc, 0, on && tr_lv == 3);
        for (int i = 0; i < n; i++) { int w = ftext_w(FT_S, nm[i]), pick = on && tr_lv == 3 && i == tr_pg;
            if (x + w > X3 + W3 - 12) { x = X3 + 18; yy += 26; }
            if (pick) rrect(x - 6, yy - 2, w + 12, 25, 5, C_WHITE);
            ftext(x, yy, FT_S, pick ? C_BG : on ? C_WHITE : C_DIM, nm[i]); x += w;
            if (i < n - 1) x += ftext(x, yy, FT_S, C_DIM, " \003 "); } }
    draw_footer(tr_lv == 0 ? "EDIT OPEN  ^v SCREEN  > THE PAD  Y BACK" : tr_lv == 1 ? "^v PAD  <> LEVEL  EDIT CATEGORIES  Y BACK  F1/F2 PAD"
              : tr_lv == 2 ? "EDIT OPEN  <> LEVEL  ^v CATEGORY  Y BACK  F1/F2 PAD" : "EDIT OPEN THE PAGE  <> PAGE  ^v CATEGORY  Y BACK  F1/F2 PAD");
}

// ---- PROJECT (A24): the sections on the left, the settings of the one chosen as cards (the 1.2 page's jsec/jcol/jcur) ----
// PROJECT > ABOUT: every key of BrickPad by topic, the sheet SHIFT held alone used to show (2026-11-14, user: SHIFT is the
// cross of the jump now). A row is "KEY what it does", a row that starts with # is a sub-title. UP/DOWN or EDIT turn the topics.
static const char *const ab_name[AB_N] = {"EVERYWHERE", "HOME", "INSIDE A PAD", "STEPS", "PIANO ROLL", "SOUND", "FX AND MOD", "MIXER", "PADS, KEYS, RECORD", "PROJECT AND FILES", "SIMPLE VIEW"};
static const char *const ab_rows[AB_N][19] = {
    {"PLAY play / stop", "Y back", "EDIT open, do it", "EDIT+ARROWS change the value (held: shown big)", "OPT listen to the pad", "OPT+EDIT delete, or the default value",
     "F1/F2 the next pad (on HOME: the next screen)", "F1+F2 PROJECT, again: back where you were", "F1+F2-3S the simple view (hold)", "SHIFT+ARROW inside a pad: straight to a category",
     "SHIFT+L2 undo", "SHIFT+OPT copy", "SHIFT+EDIT paste", "MENU pads to play, again: keys, again: back", "MENU+PLAY record, twice: capture what was played",
     "MENU-3S quit BrickPad (hold)", "SHIFT+PLAY never: the system's own", "#BRICKPAD (C) 2026 TIMOTHY GURTO (OBHATEION)"},
    {"EDIT open the pad (an empty one: choose its engine)", "X start / stop the clip of this pad", "PLAY all clips", "Y mute", "SHIFT+Y freeze the pad",
     "SHIFT+X name the pad", "SHIFT+DPAD select several pads", "OPT+EDIT clear the pad, or the selected ones (YES / NO)", "F1/F2 PADS, SCENES, MIXER", "#SCENES", "EDIT launch the scene (its own twelve pads)", "X a copy of it", "OPT+EDIT delete it (YES / NO)", "+ NEW SCENE: empty pads",
     "#UP TO 16 SCENES; TEMPO, KEY, MASTER, RETURNS ARE SHARED"},
    {"EDIT open the category or the page", "ARROWS choose", "Y back up: page, category, HOME", "F1/F2 the next pad, the same page",
     "SHIFT+UP PATTERN", "SHIFT+RIGHT SOUND", "SHIFT+DOWN FX", "SHIFT+LEFT MIX", "OPT+EDIT on a card or a list: clear it (YES / NO)", "#LOOK: THE ICON AND ITS COLOUR", "EDIT set", "L1/R1 the group of icons",
     "DOWN from the last row: the colours", "#PAD MENU AS A TREE (PROJECT > INTERFACE)", "<> the level: HOME, pad, category, pages", "^v choose", "EDIT open it"},
    {"EDIT a note on the step, or off", "EDIT+ARROWS the value of the step", "L1/R1 the lane: note, velocity, chance...", "OPT listen", "OPT+<> walk and listen",
     "X+<> the next pad", "OPT+EDIT delete the step, or its default", "XX (twice) double the clip", "SHIFT+X random", "#SELECT: THE 16 PATTERNS OF THE PAD",
     "EDIT choose the pattern", "OPT+EDIT clear it", "PLAY play it", "SHIFT+OPT copy", "SHIFT+EDIT paste", "#THEN / ELSE", "EDIT+ARROWS what plays after it, and how often"},
    {"EDIT a note here; on a note: delete it", "EDIT+ARROWS on a note: its length and pitch", "X+ARROWS velocity and chance", "X+EDIT a chord", "L1/R1 the grid",
     "OPT+<> the next note", "SHIFT+ARROWS a page, an octave (the jump is not here)", "SHIFT+X random", "Y back"},
    {"L1/R1 the page", "EDIT+ARROWS the value", "EDIT hear it", "OPT listen", "OPT+ARROWS listen to another note", "OPT+EDIT the default value",
     "#ON THE PICTURE", "EDIT the file, the presets or the slice editor", "EDIT+<> the next file", "#PATCH (GRAND)", "EDIT take a cable, then EDIT on an input",
     "OPT+EDIT pull the cables", "Y drop the cable", "#FILES", "EDIT load, or open the folder", "OPT listen", "< up a folder", "X the file menu"},
    {"#FX", "L1/R1 insert 1 / 2", "EDIT change the effect", "EDIT+<> its type", "EDIT+ARROWS the value", "X on / off", "OPT+EDIT remove, or the default",
     "#LFO", "L1/R1 LFO 1-4", "EDIT on TARGET: pick it on SOUND", "OPT+EDIT unlink, LFO off", "#AUTOMATION", "EDIT a point", "X+^v bend", "EDIT+^v its value",
     "OPT+EDIT clear", "#DUCK", "EDIT+^v level, EDIT+<> draw the curve", "L1/R1 the pad"},
    {"<> the pad", "EDIT+^v its level (EDIT+<> fine)", "Y mute", "X solo", "SHIFT+Y solo", "L1/R1 the drawer, FREEZE", "#THE DRAWER: RETURNS, MASTER, THE PAD'S EFFECTS",
     "EDIT on a pad: open, close (or R1, or past the last pad)", "<> reverb, delay, master", "^v level and two parameters", "EDIT+^v the value", "EDIT all the parameters of the unit",
     "Y close it", "#FREEZE", "EDIT freeze the pad, or export", "#PROJECT > MIXER: STRIP EFFECTS, SCENE MIX SHARED / OWN"},
    {"#PADS (MENU)", "BUTTONS the clip or the hit of a pad", "SHIFT+BUTTON the other of the two", "PLAY the pad's clip", "SHIFT+F1/F2 the scene",
     "#KEYS (MENU AGAIN)", "BUTTONS notes", "SHIFT+^v octave", "SHIFT+<> scale", "SHIFT+L1/R1 key", "MENU back", "#RECORD (PATTERN > RECORD)",
     "EDIT arm, stop", "MENU+PLAY arm from anywhere", "OPT a hit", "Y cancel the count",
     "#COUNT-IN IS IN PROJECT > RECORD: OFF WAITS FOR THE FIRST NOTE"},
    {"^v the section, the setting", "> into the section", "< back to the sections", "EDIT+<> the value", "EDIT do it (TEMPO: tap it)", "Y back",
     "#A NAME", "EDIT type the letter", "PLAY ok", "OPT delete", "L1/R1 the place", "#PRESETS", "EDIT load", "L1/R1 factory / yours", "OPT listen", "OPT+EDIT delete",
     "#FILES", "X the menu: new folder, rename, cut, delete"},
    {"F1/F2 the screen: PLAY, NOTES, SOUND", "L1/R1 the pad (on PLAY: the scene)", "F1+F2-3S leave the simple view (hold)", "MENU-3S quit BrickPad (hold)",
     "#PLAY", "EDIT play the pad", "EDIT+^v volume", "Y quiet", "#NOTES", "EDIT a note", "EDIT+^v higher / lower", "#SOUND", "<> choose the knob", "^v turn it",
     "SHIFT+X dice: a random sound", "#KID LOCK (PROJECT > INTERFACE): NOTHING IS ERASED"},
};
static void draw_about(void) {
    Col ec = th_cur[TC_SMP]; char b[16]; ab_pg = clampi(ab_pg, 0, AB_N - 1);
    if (jcol) rframe(300, 82, 690, 626, 8, 4, ec, C_CELL); else rrect(300, 82, 690, 626, 8, C_CELL);
    ftext(322, 98, FT_LB, C_WHITE, ab_name[ab_pg]);
    snprintf(b, sizeof b, "%d/%d", ab_pg + 1, AB_N); ftext(968 - ftext_w(FT_M, b), 100, FT_M, C_DIM, b);
    rect(1000, 82, 4, 626, C_CELL, 1); rect(1000, 82 + ab_pg * 626 / AB_N, 4, 626 / AB_N, C_DIM, 1);
    for (int i = 0, y = 142; i < 19 && ab_rows[ab_pg][i]; i++, y += 29) { const char *s = ab_rows[ab_pg][i];
        if (s[0] == '#') ftext(322, y + 4, FT_S, ec, s + 1); else ui_pair(322, y, s, (int)strlen(s)); }
}
static void prj_value(int i, char *b);
static void draw_prj2(void) {
    Col ec = th_cur[TC_SMP]; int r[6], n = prj_rows(jsec, r);
    { int ok = 0; for (int i = 0; i < n; i++) if (r[i] == jcur) ok = 1; if (!ok) jcur = r[0]; }
    for (int k = 0; k < JS_N; k++) { int y = 76 + k * 70, on = k == jsec;
        rrect(26, y, 249, 60, 8, on ? (jcol ? mix(C_SELBG, C_CELL, 128) : C_SELBG) : C_CELL);
        ftext(48, y + 16, FT_MB, on ? C_WHITE : C_DIM, prj_sec_name[k]); }
    if (jsec == JS_ABOUT) { draw_about(); draw_footer(jcol ? "^v TOPIC  EDIT NEXT  Y BACK  < SECTIONS" : "EDIT ENTER  Y BACK  ^v SECTION"); return; }
    int ch = 144, dy = 156, cj = 0; for (int j = 0; j < n; j++) if (r[j] == jcur) cj = j;
    int first = jcol ? cj / 4 * 4 : 0;   // four cards a page, as A24
    if (n > 4) { rect(1000, 82, 4, 4 * dy - 12, C_CELL, 1); rect(1000, 82 + (first / 4) * (4 * dy - 12) / ((n + 3) / 4), 4, (4 * dy - 12) / ((n + 3) / 4), C_DIM, 1); }
    for (int j = first; j < n && j < first + 4; j++) { int i = r[j], y = 82 + (j - first) * dy, on = jcol && i == jcur; char b[32]; prj_value(i, b);
        if (on) { rframe(300, y, 690, ch, 8, 4, ec, C_CELL); ui_focus(prj_name[i], b, i == J_MASTER ? song.master : -1, ec); } else rrect(300, y, 690, ch, 8, C_CELL);
        ftext(320, y + 14, FT_M, C_DIM, prj_name[i]);
        float nd = i == J_TEMPO ? (tempo_mul - 1) * 100 : 0; int nudging = fabsf(nd) >= 0.05f;
        if (nudging) {   // 2026-11-19 (user: "you cannot see how much the BPM is pushed or held back"): the shift and the BPM it gives
            char nb[64]; Col nc = nd > 0 ? C_PLAY : C_ORANGE;
            snprintf(nb, sizeof nb, "%s %+.1f%%   NOW %.1f BPM%s", nd > 0 ? "FASTER" : "SLOWER", nd, song.tempo * tempo_mul, midi_clk_in ? "  (MIDI CLOCK)" : "");
            ftext(320, y + 56, FT_M, nc, nb);
            int cx = 320 + 325, w = (int)(325 * clampi((int)fabsf(nd * 10), 0, 100) / 100);   // centre = no shift, the full half = 10 %
            rect(320, y + ch - 26, 650, 8, C_LINE, 1); rect(nd > 0 ? cx : cx - w, y + ch - 30, w, 16, nc, 1); rect(cx - 1, y + ch - 34, 3, 24, C_WHITE, 1);
        } else if (prj_help[i]) ftext_wrap(320, y + 56, 650, 2, FT_S, on ? mix(C_WHITE, C_CELL, 70) : C_DIM, prj_help[i]);   // what the setting does (1.2's help line)
        int f = ftext_w(FT_XB, b) <= 640 - ftext_w(FT_M, prj_name[i]) ? FT_XB : FT_M;
        ftext(970 - ftext_w(f, b), y + 10, f, i == J_PROJ && prj_dirty() ? C_ORANGE : C_WHITE, b);
        if ((i == J_TEMPO && !nudging) || i == J_SWING) { int v = i == J_TEMPO ? (song.tempo - 40) * 100 / 260 : song.swing * 100 / 75; rect(320, y + ch - 26, 650, 8, C_LINE, 1); rect(320, y + ch - 26, 650 * clampi(v, 0, 100) / 100, 8, ec, 1); }
        if (i == J_TRIM) { rect(320, y + ch - 26, 650, 8, C_LINE, 1); rect(320, y + ch - 26, 650 * trim_db / 24, 8, trim_db > 12 ? C_ORANGE : ec, 1); }
        if (i == J_MASTER && ch >= 70) { rect(320, y + ch - 26, 650, 8, C_LINE, 1); rect(320, y + ch - 26, 650 * song.master / 100, 8, ec, 1); } }
    draw_footer(!jcol ? "EDIT ENTER  Y BACK  ^v SECTION  OPT+EDIT RESET IT" : jcur == J_TEMPO ? "EDIT TAP IN TIME  EDIT+<> VALUE  Y BACK  L1/R1 HELD NUDGE  < SECTIONS  OPT+EDIT DEFAULT"
                                                    : "EDIT+<> VALUE  EDIT DO IT  Y BACK  < SECTIONS  OPT+EDIT DEFAULT");
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
