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
static void fx2_move(int k) {            // knobs in two rows; UP from the first row: the picture
    FxCard c[16]; int n = fx2_cards(c);
    if (fx2_cur < 0) { if (k == K_DOWN && n) fx2_cur = 0; }
    else kg_move(k, n, &fx2_cur);          // the knobs in kg_cols columns (ui.h)
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
    if (on) ui_focus(lb, v, hi - lo > 3 ? (val - lo) * 100 / (hi - lo) : -1, ec);
    if (hi - lo == 1 && cd.row >= 2 && !cd.side) { toggle_draw(x, y, w, h, lb, NULL, NULL, val != lo, on, ec); return; }   // ON / OFF: a switch
    if (cd.row == 9) { if (on) ui_focus(lb, v, -1, ec); seg_draw(x, y, w, h, lb, v, (const char *const *)mod_mode_name, 3, clampi(val, 0, 2), on, ec); return; }   // MODE: three names
    if (cd.side && (f->type == FX_COMP || f->type == FX_RING)) { if (on) ui_focus(lb, v, -1, ec); val_draw(x, y, w, h, lb, v, on, ec); return; }   // KEY / SOURCE: a pad
    int cx, cy, r; float fz = lo < 0 && hi > 0 ? (float)-lo / (hi - lo) : 0;   // v3: a knob, as on SOUND
    float f0 = hi > lo ? (float)clampi(val - lo, 0, hi - lo) / (hi - lo) : 0; int tg = cd.row >= 2 && cd.row - 2 < 6 && !cd.side ? (fx2_slot ? LT_INS2 : LT_INS1) + cd.row - 2 : -1;   // 2026-11-25: modulated, Serum's way
    knob_draw(x, y, w, h, lb, v, mod_live(fx_unit, tg, f0), fz, on, ec, &cx, &cy, &r); mod_rings(fx_unit, tg, f0, cx, cy, r);
}
static void draw_fx2(void) {
    PadMix *pm = &song.mix.pad[fx_unit]; Col ec = C_CAT(PC_FX); fx2_sync();
    for (int s = 0; s < 2; s++) {        // the inserts as tabs
        const FxSlot *f = &pm->ins[s]; char b[32]; int hw = (ww - 13) / 2, x = wx0 + s * (hw + 13), on = s == fx2_slot;
        if (fx_is_mod(f->type)) snprintf(b, sizeof b, "%d \003 MODULATION \003 %s", s + 1, fx_name[f->type]);
        else snprintf(b, sizeof b, "%d \003 %s", s + 1, f->type ? fx_name[f->type] : "EMPTY");
        rrect(x, 76, hw, 56, 6, on ? ec : C_CELL);
        ftext(x + hw / 2 - ftext_w(FT_MB, b) / 2, 92, FT_MB, on ? C_BG : f->type && f->on ? C_WHITE : C_DIM, b);
    }
    const FxSlot *f = &pm->ins[fx2_slot]; FxCard c[16]; int n = fx2_cards(c);
    int ph = (n + kg_cols(n) - 1) / kg_cols(n) <= 1 ? 290 : 186, y0 = 153 + ph + 16;   // every knob at once; one row: a taller picture
    if (fx2_cur < 0) rframe(wx0, 153, ww, ph, 8, 4, ec, C_CELL); else rrect(wx0, 153, ww, ph, 8, C_CELL);
    fx2_picture(wx0 + 20, 153 + 40, ww - 40, ph - 46, f, ec);   // under the FX LOAD label (2026-11-24: it covered the wave)
    fx2_live(wx0 + 20, 153, ww - 40, ph, f, ec);
    for (int i = 0; i < n; i++) { int x, y, w, h; kg_cell(n, i, wx0, ww, y0, 700, &x, &y, &w, &h); fx2_card(x, y, w, h, f, c[i], i == fx2_cur, ec); }
    if (fx2_cur < 0) draw_footer("EDIT CHANGE  EDIT+<> TYPE  ^ TABS");
    else draw_footer("EDIT+ARROWS VALUE  ^ TABS  OPT+EDIT DEFAULT");
}

// ---- MOD: its list (LFO, AUTOMATION, DUCK), as PATTERN's ----

// ---- MOD > LFO (A18): LFO 1-4 as tabs (L1/R1), its shape, seven cards (the 1.2 rows: lrow) ----
static int v2_lfo(void) { return ui_cat == PC_MOD && tab == T_FX && fx_unit < NSLOT && fx_page; }
static const int8_t lfo2_ord[LR_N] = {LR_TARGET, LR_SHAPE, LR_RATE, LR_DEPTH, LR_SYNC, LR_POL, LR_TRIG, LR_PHASE};   // A18's four first; POL by CLOCK
static int lfo2_pos(void) { for (int i = 0; i < LR_N; i++) if (lfo2_ord[i] == lrow) return i; return 0; }
static void lfo2_move(int k) {
    int p = lfo2_pos(); if (k == K_UP && p < kg_cols(LR_N)) return;   // the knobs in kg_cols columns (ui.h)
    kg_move(k, LR_N, &p); lrow = lfo2_ord[p];
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
    for (int j = 0; j < NLFO; j++) { int tp = (ww + 10) / NLFO, x = wx0 + j * tp, on = j == lcol; const Lfo *M = &song.lfo[fx_unit][j]; snprintf(b, sizeof b, "LFO %d", j + 1);
        if (on) rrect(x, 76, tp - 10, 38, 8, ec);
        ftext(x + (tp - 10) / 2 - ftext_w(FT_MB, b) / 2, 83, FT_MB, on ? C_BG : M->target && M->depth ? C_WHITE : C_DIM, b); }
    rrect(wx0, 127, ww, 200, 8, C_CELL);
    { int px = 0, py = 0; unsigned rng = 7u + lcol; float hold = 0; int act = L->target && L->depth;
      int gw = ww - 40;                    // the shape across the work area (side slots)
      for (int i = 0; i <= gw && L->target; i += 3) { float t = fmodf(i * 2.f / gw + L->phase / 100.f, 1.f), v;   // no target: only the note
          if (L->shape % LS_N == 5) { if (i % 117 < 3) hold = lfo2_shape(5, t, &rng); v = hold; } else v = lfo2_shape(L->shape, t, &rng);
          int x = wx0 + 20 + i, y = 227 - (int)(v * 80);
          if (i) line(px, py, x, y, 3, act ? ec : C_DIM);
          px = x; py = y; }
      if (act) { float v = engine_lfo(fx_unit, lcol); disc2(wx0 + 20 + gw, 227 - (int)(v * 80), 6, C_WHITE); }   // where it is now
      else if (!L->target) ui_empty(wx0 + 20, 140, ww - 40, 174, "NO TARGET YET", "TARGET: EDIT, then EDIT on a SOUND card"); }   // empty state (TZ)
    for (int i = 0; i < LR_N; i++) {     // v3: every setting as a knob at once
        int r = lfo2_ord[i], x, y, kw, kh, on = r == lrow, lo = 0, hi = 0, v = 0, cx, cy, kr; kg_cell(LR_N, i, wx0, ww, 343, 700, &x, &y, &kw, &kh);
        lfo_str(fx_unit, lcol, r, b);
        if (r == LR_DEPTH) { lo = -100; hi = 100; v = L->depth; } else if (r == LR_PHASE) { hi = 100; v = L->phase; } else if (r == LR_RATE && !L->sync) { hi = 100; v = L->rate; }
        if (on) ui_focus(lr_name[r], b, hi > lo ? (v - lo) * 100 / (hi - lo) : -1, ec);
        if (r == LR_SYNC) { toggle_draw(x, y, kw, kh, lr_name[r], "FREE", "TEMPO", L->sync != 0, on, ec); if (on) ui_focus(lr_name[r], b, -1, ec); continue; }   // two states: a switch
        if (r == LR_POL) { toggle_draw(x, y, kw, kh, lr_name[r], "BI", "UNI", (L->spare & 1) != 0, on, ec); if (on) ui_focus(lr_name[r], b, -1, ec); continue; }
        if (r == LR_TARGET || r == LR_SHAPE || r == LR_TRIG) { val_draw(x, y, kw, kh, lr_name[r], b, on, ec); if (on) ui_focus(lr_name[r], b, -1, ec); continue; }   // names: a value card
        knob_draw(x, y, kw, kh, lr_name[r], b, hi > lo ? (float)(v - lo) / (hi - lo) : 0, lo < 0 ? 0.5f : 0, on, ec, &cx, &cy, &kr);
    }
    draw_footer(lrow == LR_TARGET ? "EDIT PICK ON SOUND  EDIT+<> TARGET  ^ TABS" : "EDIT+ARROWS VALUE  ^ TABS  OPT+EDIT LFO OFF");
}

// ---- MOD > AUTOMATION (A19): A1-A4 as tabs (L1/R1; UP: the tab, EDIT+<> its target), the lane over the whole pattern ----
static int v2_auto(void) { return ui_cat == PC_MOD && tab == T_SEQ && is_auto(lane) && !perform; }
static void auto2_move(int k) {
    int len = cur_pat(sel)->len ? cur_pat(sel)->len : 16;
    if (srow == 0) { if (k == K_DOWN) { srow = 1; scol = 0; } return; }
    if (k == K_UP) { srow = 0; scol = 4; return; }
    if (k == K_LEFT || k == K_RIGHT) set_step(clampi(cur_step() + (k == K_RIGHT ? 1 : -1), 0, len - 1));
}
static void draw_auto2(void) {
    Col ec = C_CAT(PC_MOD); const Pattern *p = cur_pat(sel); int len = p->len ? p->len : 16, cur = lane - L_A1;
    for (int a = 0; a < NAUTO; a++) { const AutoLane *al = &song.aut[sel][song.t[sel].cur][a]; char t[24], b[40]; int tp = (ww + 10) / NAUTO, x = wx0 + a * tp, on = a == cur;
        lfo_tname(sel, al->target, t); snprintf(b, sizeof b, "A%d %.12s", a + 1, al->target ? t : "-");
        if (on) { rrect(x, 76, tp - 10, 38, 8, ec); if (srow == 0) rframe(x - 4, 72, tp - 2, 46, 10, 3, C_WHITE, ec); }
        ftext(x + (tp - 10) / 2 - ftext_w(FT_MB, b) / 2, 83, FT_MB, on ? C_BG : al->target ? C_WHITE : C_DIM, b); }
    enum { PY = 130, PH = 520, TOP = PY + 30, BOT = PY + PH - 30 }; const int PX = wx0, PW = ww, IX = PX + 14, IW = PW - 28;   // the work area (side slots)
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
        // 2026-11-20 (user: "the same everywhere curves are edited", as DUCK): a dot half way along each stretch for its bend
        // (orange when bent), hollow points, the one under the cursor filled
        for (int i = 0; i < len; i++) if (al->v[i] != AUTO_EMPTY) { int j = i + 1; while (j < len && al->v[j] == AUTO_EMPTY) j++; if (j >= len) break;
            double mf = (i + j) / 2.0; disc2(IX + (int)(mf * IW / len), BOT - (int)(auto_value(al, bc, len, mf) * (BOT - TOP) / 100), held[K_X] && i == aut_point_back() ? 7 : 5, bc[i] ? C_ORANGE : C_GREY); }
        for (int i = 0; i < len; i++) if (al->v[i] != AUTO_EMPTY) { int cx = IX + i * IW / len, cy = BOT - al->v[i] * (BOT - TOP) / 100;
            if (i == st) disc2(cx, cy, 12, C_WHITE); else { disc2(cx, cy, 10, ec); disc2(cx, cy, 6, C_CELL); } } }
    else { const char *m = al->target ? "no points: EDIT puts one on the step" : "no target: UP, then EDIT+<> picks one"; ftext(PX + PW / 2 - ftext_w(FT_M, m) / 2, PY + PH / 2 - 12, FT_M, C_DIM, m); }
    char b[48]; int back = aut_point_back();
    if (back >= 0) snprintf(b, sizeof b, "bend %+d%%", bc[back]); else snprintf(b, sizeof b, "bend -");
    ftext(wx0, 664, FT_M, C_DIM, b);
    if (st >= 0) { if (al->v[st] != AUTO_EMPTY) snprintf(b, sizeof b, "step %d \004 %d%%", st + 1, al->v[st]); else snprintf(b, sizeof b, "step %d", st + 1);
        ftext(wx0 + ww - ftext_w(FT_M, b), 664, FT_M, C_WHITE, b); }
    draw_footer(srow == 0 ? "EDIT+<> TARGET  EDIT PICK ON SOUND  v POINTS" : "EDIT POINT  EDIT+^v VALUE  ^ TABS");
}

// ---- MIX of a pad: its strip of the mixer as cards (the 1.2 rows: mrow, the column mcol = the pad) ----
static const int8_t mix2_rows[8] = {MX_VOL, MX_PAN, MX_SA, MX_SB, MX_FRZ, MX_DUCK, MX_MUTE, MX_SOLO};
static int mix2_cur;
static int v2_mix(void) { return ui_cat == PC_MIX && tab == T_MIX && mcol < NSLOT && !ducking; }
static void mix2_move(int k) {
    if (k == K_LEFT) { if (mix2_cur % 4) mix2_cur--; } else if (k == K_RIGHT) { if (mix2_cur % 4 < 3) mix2_cur++; }   // v3: four a row
    else if (k == K_DOWN) { if (mix2_cur < 4) mix2_cur += 4; } else if (mix2_cur >= 4) mix2_cur -= 4;
    mrow = mix2_rows[mix2_cur];
}
static void draw_mix2(void) {
    Col ec = C_CAT(PC_MIX); const Track *t = &song.t[mcol]; const PadMix *pm = &song.mix.pad[mcol]; mrow = mix2_rows[mix2_cur & 7];
    static const char *const nm[8] = {"VOLUME", "PAN", "SEND A", "SEND B", "FREEZE", "DUCK", "MUTE", "SOLO"};
    for (int i = 0; i < 8; i++) { char v[24]; int x = 30 + (i % 4) * 242, y = 90 + (i / 4) * 300, on = i == mix2_cur, val = -1, lo = 0, hi = 100;   // v3: all eight at once
        switch (i) { case 0: val = t->vol; snprintf(v, sizeof v, "%d", t->vol); break;
            case 1: val = t->pan; lo = -50; hi = 50; if (t->pan) snprintf(v, sizeof v, "%c%d", t->pan < 0 ? 'L' : 'R', abs(t->pan)); else snprintf(v, sizeof v, "C"); break;
            case 2: val = pm->send_a; snprintf(v, sizeof v, "%d", pm->send_a); break;
            case 3: val = pm->send_b; snprintf(v, sizeof v, "%d", pm->send_b); break;
            case 4: snprintf(v, sizeof v, "%s", frozen(mcol) ? "FROZEN" : "LIVE"); break;
            case 5: if (pm->spare[DK_SRC]) snprintf(v, sizeof v, "PAD %d %d%%", pm->spare[DK_SRC], pm->spare[DK_AMT]); else snprintf(v, sizeof v, "OFF"); break;
            case 6: snprintf(v, sizeof v, "%s", t->mute ? "ON" : "OFF"); break;
            default: snprintf(v, sizeof v, "%s", pm->solo ? "ON" : "OFF"); }
        if (on) ui_focus(nm[i], v, i < 4 ? (val - lo) * 100 / (hi - lo) : -1, ec);
        if (i < 4) { int cx, cy, kr; knob_draw(x, y, 234, 290, nm[i], v, (float)clampi(val - lo, 0, hi - lo) / (hi - lo), lo < 0 ? 0.5f : 0, on, ec, &cx, &cy, &kr); continue; }
        int lit = i == 4 ? frozen(mcol) : i == 6 ? t->mute : i == 7 ? pm->solo : pm->spare[DK_SRC] != 0;   // the switches: a lamp button (M, S as drawn buttons)
        if (on) rframe(x, y, 234, 290, 8, 3, ec, C_CELL); else rrect(x, y, 234, 290, 8, C_CELL);
        ftext(x + 117 - ftext_w(FT_S, nm[i]) / 2, y + 8, FT_S, on ? C_WHITE : C_DIM, nm[i]);
        const char *big = i == 6 ? "M" : i == 7 ? "S" : i == 4 ? "FRZ" : i == 5 ? "DK" : NULL;
        rrect(x + 47, y + 90, 140, 90, 12, lit ? ec : C_SELBG);
        if (big) ftext(x + 117 - ftext_w(FT_XB, big) / 2, y + 135 - fv2[FT_XB].h / 2, FT_XB, lit ? C_BG : C_DIM, big);
        ftext(x + 117 - ftext_w(FT_MB, v) / 2, y + 290 - 40, FT_MB, lit ? ec : C_WHITE, v); }
    draw_footer(mix2_cur == 4 ? "EDIT FREEZE/LIVE  Y BACK" : mix2_cur == 5 ? "EDIT OPEN DUCK  EDIT+<> SOURCE  Y BACK  EDIT+^v DEPTH" : mix2_cur >= 6 ? "EDIT ON/OFF  Y BACK" : "EDIT+ARROWS VALUE  Y BACK  OPT+EDIT DEFAULT");
}

// ---- MIXER (HOME): the faders (A21) and a drawer at the side: returns, master, the pad's effects. L1/R1: closed, open, FREEZE (A23) ----
enum { MXP_PADS, MXP_FREEZE = 4, MXP_N };   // the mixer's tab row (2026-11-24): PADS, RETURN A, RETURN B, MASTER, FREEZE
static int mx_pg, mx_r, mx_fz;            // 0 the drawer closed, 1 open, 2 FREEZE; in the drawer: the row of a unit (0 level, 1-2 two parameters); FREEZE: the row
static int v2_mixer(void) { return ui_cat < 0 && tab == T_MIX && !ducking && !perform; }
static const int8_t mx_ret_k[3][2] = {{RV_SIZE, RV_DECAY}, {DL_TIME, DL_FB}, {MS_COMP, MS_LIM}};
static void mixer2_fix(void) {            // the cursor is on a pad, or on a unit of the open drawer
    if (mcol > NSLOT + 2 || (mx_pg != 1 && mcol >= NSLOT)) mcol = sel; mx_r = clampi(mx_r, 0, 2);
    if (mcol < NSLOT && !has_sound(mcol) && any_sound()) { int s = mcol; while (!has_sound(s)) s = (s + 1) % NSLOT; mcol = sel = s; }   // no outline on an empty strip
}
static void mixer2_page(int d) { mx_pg = (mx_pg + d + 3) % 3; mx_r = 0; mrow = MX_VOL; if (mx_pg == 1) mcol = ui_drawer_left ? NSLOT + 2 : NSLOT; else if (mx_pg == 0) mcol = sel; else mx_fz = 0; }
static void mixer2_move(int k) {
    mixer2_fix();
    if (mx_pg != 2) {                     // one line: the twelve pads and, at the drawer's side, its three units; stepping past the last pad opens it
        if (k == K_LEFT || k == K_RIGHT) { int p = (ui_drawer_left ? (mcol >= NSLOT ? mcol - NSLOT : mcol + 3) : mcol) + (k == K_RIGHT ? 1 : -1);
            if (p < 0 || p > NSLOT + 2) return;
            mcol = ui_drawer_left ? (p < 3 ? p + NSLOT : p - 3) : p;
            if (mcol < NSLOT && !has_sound(mcol) && any_sound()) {   // 2026-11-24: an empty strip is passed over (a loop: the recursion ran away at the edge, cloud QA)
                int d = k == K_RIGHT ? 1 : -1, q = mcol; while (q >= 0 && q < NSLOT && !has_sound(q)) q += d;
                if (q >= 0 && q < NSLOT) mcol = q; else if (d > 0) { mcol = ui_drawer_left ? sel : NSLOT; if (mx_pg == 0 && mcol >= NSLOT) { mx_pg = 1; mx_r = 0; } } else mcol = sel; }
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
    PadMix *pm = &song.mix.pad[s]; int on = s == mcol, has = has_sound(s), fx = ui_mxfx, top = fx ? 246 : 164, bot = 598, h = (bot - top) * clampi(song.t[s].vol, 0, 100) / 100; char b[8];
    Col c = has ? (on ? C_ORANGE : C_ENG(song.t[s].type)) : C_LINE;
    if (on) rframe(cx - hw, 118, hw * 2, 596, 8, 3, C_WHITE, C_BG);   // 2026-11-24: below the tab row (mx_row)
    rect(cx - 10, top, 14, bot - top, C_CELL, 1);
    snprintf(b, sizeof b, "%02d", s + 1); mx_ctext(cx, 650, FT_MB, on ? C_WHITE : C_DIM, b);
    if (!has) return;
    icon(slot_icon(s), cx - 16, 124, song.t[s].mute ? C_DIM : pad_icol(s, on ? C_WHITE : C_GREY));
    for (int i = 0; i < 2 && fx; i++) { int ty = pm->ins[i].type < FX_N ? pm->ins[i].type : 0; mx_ctext(cx, 158 + i * 20, FT_S, ty && pm->ins[i].on ? C_CAT(PC_FX) : C_DIM, fx_short[ty]); }
    for (int i = 0; i < 2 && fx; i++) { int v = clampi(i ? pm->send_b : pm->send_a, 0, 100), y = 200 + i * 20, w = hw * 2 - 28;
        ftext(cx - hw + 6, y, FT_S, v ? C_WHITE : C_DIM, i ? "B" : "A"); rect(cx - hw + 20, y + 8, w, 5, C_LINE, 1); rect(cx - hw + 20, y + 8, w * v / 100, 5, C_CAT(PC_MIX), 1); }
    rect(cx - 10, bot - h, 14, h, c, 1); rect(cx - 18, bot - h - 5, 28, 10, C_WHITE, 1);
    meter_bar(cx + 13, top, 7, bot - top, mx_show[s]);
    mx_db(b, song.t[s].vol / 100.f); mx_ctext(cx, 606, FT_S, on ? C_WHITE : C_GREY, b);
    if (on) { char nn[24]; snprintf(nn, sizeof nn, "%02d VOLUME DB", s + 1); ui_focus(nn, b, song.t[s].vol, c); }
    if (mx_show[s] >= 0.001f) { mx_db(b, mx_show[s]); mx_ctext(cx, 626, FT_S, C_PLAY, b); }
    if (song.t[s].mute) mx_ctext(cx, 678, FT_MB, C_RED, "M"); else if (pm->solo) mx_ctext(cx, 678, FT_MB, C_ORANGE, "S");
}
static void mx_tab(void) {                // the closed drawer: a tab at the edge with the master's meter
    int tx = ui_drawer_left ? -8 : 966, ox = ui_drawer_left ? 2 : 968; char b[8];   // the whole frame on the screen
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
    rframe(dx, 114, 320, 602, 8, 2, C_ORANGE, C_BG);
    for (int u = 0; u < 3; u++) { int cx = dx + 56 + u * 104, on = mcol == NSLOT + u, top = 162, bot = 380; Col c = u == 2 ? th_cur[TC_SMP] : C_CAT(PC_MIX);
        int lv = u == 2 ? song.master : *unit_param(NSLOT + u, u ? DL_LEVEL : RV_LEVEL, &lo, &hi, &def), fh = (bot - top) * clampi(lv, 0, 100) / 100;
        if (on && mx_r == 0) rframe(cx - 48, 118, 96, 314, 6, 3, C_WHITE, C_BG);
        mx_ctext(cx, 122, FT_S, on ? C_WHITE : C_GREY, cn[u]); mx_ctext(cx, 140, FT_S, C_DIM, sn[u]);
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
// 2026-11-24 (user: a hidden page is a dead end): the mixer's pages as a tab row on each of them, as everywhere else
static int mix_at(void) { return v2_unit() ? 1 + fx_unit - NSLOT : mx_pg == 2 ? MXP_FREEZE : MXP_PADS; }
static void mix_goto(int i) {
    if (i >= 1 && i <= 3) { mcol = fx_unit = NSLOT + i - 1; frow = fcol = 0; set_tab(T_FX); ui_cat = -1; return; }
    set_tab(T_MIX); ui_cat = -1; mx_r = 0; mrow = MX_VOL;
    if (i == MXP_FREEZE) { mx_pg = 2; mx_fz = 0; } else { mx_pg = 0; mcol = sel; }
}
static void mx_row(void) {
    static const char *const nm[MXP_N] = {"PADS", "RETURN A", "RETURN B", "MASTER", "FREEZE"}; int at = mix_at(), tw = 971 / MXP_N;
    for (int i = 0; i < MXP_N; i++) { int x = 26 + i * tw, on = i == at; Col ec = i == 3 ? th_cur[TC_SMP] : C_CAT(PC_MIX);
        if (on) rrect(x, 72, tw - 10, 36, 8, ec);
        ftext(x + (tw - 10) / 2 - ftext_w(FT_MB, nm[i]) / 2, 78, FT_MB, on ? C_BG : C_DIM, nm[i]); }
}
static void draw_mixer2(void) {
    mixer2_fix();
    mx_meters(); mx_row();
    if (mx_pg == 0 && !any_sound()) { ui_empty(26, 200, 971, 300, "NOTHING TO MIX YET", "PADS: EDIT on an empty pad picks an engine"); draw_footer("^ PAGES  Y BACK"); return; }
    if (mx_pg != 2) {                     // the drawer lies over the strips (2026-11-14, user: nothing narrows); they slide so the pad stays in view
        static int off; int open = mx_pg == 1, cs = mcol < NSLOT ? mcol : sel, vis = open ? 8 : NSLOT;
        if (cs < off) off = cs; if (cs >= off + vis) off = cs - vis + 1; off = clampi(off, 0, NSLOT - vis);
        float e = anim(&mx_anim, open, 200);   // the drawer slides in from its side (2026-11-21, user: every drawer moves)
        int xc = ui_drawer_left ? 90 : 50, xo = ui_drawer_left ? 387 : 50, x0 = xc + (int)((xo - xc) * e);
        for (int s = 0; s < NSLOT; s++) if (x0 + (s - off) * 78 - 37 >= 0) mx_strip(s, x0 + (s - off) * 78, 37);   // none half off the left edge
        if (e > 0) mx_drawer(ui_drawer_left ? -(int)((1 - e) * 340) : 704 + (int)((1 - e) * 340)); else mx_tab();
        draw_footer(!open ? "EDIT+^v LEVEL  OPT+v MUTE  OPT+^ SOLO" : mcol < NSLOT ? "EDIT+^v LEVEL  OPT+v MUTE  OPT+^ SOLO"
                    : "EDIT ALL PARAMETERS  EDIT+^v VALUE  Y CLOSE  <> UNIT, PADS");
    } else {                              // A23: the pads with a sound, live or frozen; EXPORT last; the DSP load
        int n = 0, ss[NSLOT]; for (int s = 0; s < NSLOT; s++) if (has_sound(s)) ss[n++] = s;
        int top = clampi(mx_fz - 3, 0, n + 1 > 6 ? n + 1 - 6 : 0);
        for (int r = 0; r < 6 && top + r <= n; r++) { int i = top + r, y = 128 + r * 86, on = i == mx_fz; char b[40];   // below the tab row
            rrect(26, y, 971, 76, 8, on ? C_SELBG : C_CELL);
            if (i == n) { ftext(48, y + 26, FT_LB, C_WHITE, "EXPORT"); ftext(974 - ftext_w(FT_M, "song \003 WAV"), y + 28, FT_M, C_DIM, "song \003 WAV"); continue; }
            int s = ss[i]; snprintf(b, sizeof b, "%02d %s", s + 1, slot_name(s) ? slot_name(s) : "EMPTY"); fit(b, FT_LB, 700); ftext(48, y + 26, FT_LB, C_WHITE, b);
            if (frozen(s)) { const char *f = "\002 frozen"; ftext(974 - ftext_w(FT_M, f), y + 28, FT_M, C_ENG(3), f); } else ftext(974 - ftext_w(FT_M, "live"), y + 28, FT_M, C_DIM, "live"); }
        int ld = (int)(engine_load() * 100); char b[8]; snprintf(b, sizeof b, "%d%%", ld);
        ftext(26, 660, FT_M, C_DIM, "CPU"); rect(102, 668, 780, 8, C_LINE, 1); rect(102, 668, 780 * clampi(ld, 0, 100) / 100, 8, ld >= 85 ? C_RED : th_cur[TC_SMP], 1);
        ftext(997 - ftext_w(FT_MB, b), 658, FT_MB, C_WHITE, b);
        draw_footer(mixer2_fz_pad() < 0 ? "EDIT EXPORT  ^ PAGES  Y BACK" : "EDIT FREEZE  ^ PAGES  Y BACK");
    }
}

// ---- MOD > DUCK (A20): the curve as sixteen bars, the fields as cards (keys: duck_key, the 1.2 window's) ----
static void draw_duck2(void) {
    const PadMix *pm = &song.mix.pad[sel]; const int8_t *dx = song.dkx[sel]; const uint8_t *cv = song.dkc[sel];
    int mode = dx[DX_MODE] % DM_N, amt = pm->spare[DK_AMT], src = pm->spare[DK_SRC], onn = mode == DM_GRID ? amt > 0 : src > 0;
    Col ec = C_CAT(PC_MIX); enum { PX = 26, PY = 80, PW = 971, PH = 280 };
    if (dk_row == 0) rframe(PX, PY, PW, PH, 8, 4, ec, C_CELL); else rrect(PX, PY, PW, PH, 8, C_CELL);
    // 2026-11-19 (user, as ShaperBox): the pad's sound under the curve as it passes, the curve through up to 8 points,
    // hollow ones and the chosen one filled, a dot half way along each stretch for its bend (orange when bent)
    (void)cv; const uint8_t *pt = dk_view_pts(sel); int npt = dk_npts(pt), ix = PX + 14, iy = PY + 14, iw = PW - 28, ih = PH - 28;
    enum { WB = 120 }; static float wh[NSLOT][WB];
    { float g = 1, v = engine_duck_view(sel, &g); mx_meters();
      for (int i = 0; i < WB; i++) wh[sel][i] *= 0.997f;
      if (v >= 0) { int bi = clampi((int)(v * WB), 0, WB - 1); float l = sqrtf(mx_show[sel]); wh[sel][bi] = wh[sel][bi] * 0.5f + l * 0.5f; }
      for (int i = 0; i < WB; i++) { int hh = (int)(ih * 0.9f * (wh[sel][i] > 1 ? 1 : wh[sel][i])); if (hh > 1) rect(ix + i * iw / WB, iy + ih - hh, iw / WB - 1, hh, mix(C_CELL, ec, 70), 1); }
      if (v >= 0) rect(ix + (int)(iw * v), PY + 6, 2, PH - 12, C_WHITE, 1); }
    { int lx = 0, ly = 0, rx = 0, ry = 0;
      for (int i = 0; i <= 96; i++) { float L = dk_pts_eval(pt, i / 96.f), R = 100 - amt * (100 - L) / 100;
          int x = ix + iw * i / 96, y = iy + ih - (int)(ih * L / 100), yr = iy + ih - (int)(ih * R / 100);
          if (i) { if (amt < 100) line(rx, ry, x, yr, 1, mix(C_WHITE, ec, 120)); line(lx, ly, x, y, 3, onn ? ec : C_DIM); }   // the curve; what the level really does, with the depth
          lx = x; ly = y; rx = x; ry = yr; } }
    for (int i = 0; i + 1 < npt; i++) { DkPt a = dk_pt_get(pt, i), b2 = dk_pt_get(pt, i + 1); if (b2.x <= a.x) continue;
        float mx = (a.x + b2.x) / 2.f / 31; int cx = ix + (int)(iw * mx), cy = iy + ih - (int)(ih * dk_pts_eval(pt, mx) / 100);
        disc2(cx, cy, held[K_X] && dk_row == 0 && i == dk_pt ? 7 : 5, a.bend ? C_ORANGE : C_GREY); }
    for (int i = 0; i < npt; i++) { DkPt q = dk_pt_get(pt, i); int cx = ix + iw * q.x / 31, cy = iy + ih - ih * q.y / 63, on = dk_row == 0 && i == dk_pt;
        if (on) disc2(cx, cy, 12, C_WHITE); else { disc2(cx, cy, 10, onn ? ec : C_DIM); disc2(cx, cy, 6, C_CELL); } }
    if (dk_row == 0 && npt) { char nn[16], vv[24]; DkPt q = dk_pt_get(pt, clampi(dk_pt, 0, npt - 1)); snprintf(nn, sizeof nn, "POINT %d/%d", dk_pt + 1, npt);
        snprintf(vv, sizeof vv, q.bend && dk_pt < npt - 1 ? "%d%%  BEND %+d" : "%d%%", q.y * 100 / 63, q.bend); ui_focus(nn, vv, q.y * 100 / 63, ec); }
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
    draw_footer(dk_row == 0 ? "EDIT+ARROWS LEVEL/MOVE  X NEW POINT, X+^v BEND  OPT+EDIT DELETE  <> POINT  Y BACK  L1/R1 PAD" : "EDIT SHAPE  Y BACK  EDIT+ARROWS VALUE  OPT+EDIT DEFAULT  L1/R1 PAD");
    return;
}

// ---- SCENES (A02, HOME): a row per scene, each its own twelve pads (a dot per pad with a sound, bigger with notes); the last row makes one ----
static int sc_cur;
static int scene_count(void) { return scn_n; }
static void scene_key(int k, int down) {
    int rows = scn_n + (scn_n < NSCENE) * 2;   // the scenes, + NEW SCENE, + COPY OF THE OPEN ONE (2026-11-24: was X)
    if (!down) {
        if (k == K_EDIT && !used[K_EDIT]) {
            if (sc_cur == scn_n + 1) { scene_new(scn_cur); sc_cur = scn_n - 1; }
            else if (sc_cur >= scn_n) { scene_new(-1); sc_cur = scn_n - 1; }
            else if (sc_cur == scn_cur && scn_next < 0) toast("THIS SCENE IS OPEN");
            else if (scn_lock) toast("SCENE LOCK IS ON");
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
    int n = scn_n, rows = n + (n < NSCENE) * 2; sc_cur = clampi(sc_cur, 0, rows - 1);
    int top = clampi(sc_cur - 2, 0, rows > 6 ? rows - 6 : 0);
    for (int r = 0; r < 6 && top + r < rows; r++) { int q = top + r, y = 80 + r * 104, on = q == sc_cur; char b[24];
        rrect(26, y, 971, 90, 8, on ? C_SELBG : C_CELL);
        if (q >= n) { char t[32]; const char *h = q == n ? "empty pads" : "the same pads"; if (q == n) snprintf(t, sizeof t, "+ NEW SCENE"); else snprintf(t, sizeof t, "+ COPY OF SCENE %d", scn_cur + 1);
            ftext(48, y + 30, FT_LB, on ? C_WHITE : C_DIM, t); ftext(974 - ftext_w(FT_M, h), y + 32, FT_M, C_DIM, h); continue; }
        const Song *g = q == scn_cur ? &song : scn[q];
        snprintf(b, sizeof b, "SCENE %d", q + 1); ftext(62, y + 30, FT_LB, on ? C_WHITE : C_DIM, b);
        if (q == scn_cur) ftext(38, y + 32, FT_MB, th_cur[TC_SMP], "\001");
        else if (q == scn_next && (SDL_GetTicks() / 250) % 2) ftext(38, y + 32, FT_MB, C_ORANGE, "\001");   // waits for the bar
        for (int s = 0; s < NSLOT && g; s++) { int has = q == scn_cur ? has_sound(s) : g->t[s].type || (scn_ready[q] ? scn_smp[q][s] != NULL : g->t[s].path[0] != 0), x = 344 + s * 52, cy = y + 45;
            int u = has && pat_used(&g->t[s].pat[clampi(g->t[s].cur, 0, NPAT - 1)]);
            if (has) disc2(x, cy, u ? 9 : 6, C_ENG(g->t[s].type)); else { disc2(x, cy, 6, C_GREY); disc2(x, cy, 4, C_CELL); } } }   // an empty pad: a ring
    if (rows > 6) { int th = 6 * 104 - 14, tw = th * 6 / rows; rect(1004, 80, 4, th, C_CELL, 1); rect(1004, 80 + (th - tw) * top / (rows - 6), 4, tw, C_DIM, 1); }
    if (rows <= 4) {                      // 2026-11-24 (audit): room below: the scene under the cursor, its twelve pads named
        int q = sc_cur < n ? sc_cur : scn_cur; const Song *g = q == scn_cur ? &song : scn[q]; int y0 = 80 + rows * 104 + 8, ch = (700 - y0 - 8) / 2 - 6;
        if (g && ch >= 60) for (int s2 = 0; s2 < NSLOT; s2++) { int x = 26 + s2 % 6 * 163, y = y0 + s2 / 6 * (ch + 8), has = q == scn_cur ? has_sound(s2) : g->t[s2].type || g->t[s2].path[0];
            rrect(x, y, 155, ch, 8, C_CELL); if (!has) { ftext(x + 12, y + 8, FT_S, C_DIM, "--"); continue; }
            rect(x, y, 6, ch, C_ENG(g->t[s2].type), 1); char b[40], nm[40];
            const char *pth = g->t[s2].path, *bn = strrchr(pth, '/'); static const char *const en[4] = {"SAMPLER", "GRAND", "ACID", "CLOUD"};
            snprintf(nm, sizeof nm, "%s", g->t[s2].type ? en[g->t[s2].type & 3] : bn ? bn + 1 : pth); fit(nm, FT_MB, 135);
            snprintf(b, sizeof b, "%02d", s2 + 1); ftext(x + 14, y + 6, FT_S, C_DIM, b); ftext(x + 14, y + 30, FT_MB, C_WHITE, nm);
            snprintf(b, sizeof b, "P%d", g->t[s2].cur + 1); ftext(x + 14, y + ch - 26, FT_S, C_GREY, b); } }
    draw_footer(sc_cur >= n ? (sc_cur == n + 1 ? "EDIT COPY THE OPEN ONE  Y BACK" : "EDIT NEW EMPTY  Y BACK") : "EDIT LAUNCH  OPT+EDIT DELETE  Y BACK");
}

// ---- PROJECT (A24): the sections on the left, the settings of the one chosen as cards (the 1.2 page's jsec/jcol/jcur) ----
// PROJECT > ABOUT: every key of BrickPad by topic, the sheet SHIFT held alone used to show (2026-11-14, user: SHIFT is the
// cross of the jump now). A row is "KEY what it does", a row that starts with # is a sub-title. UP/DOWN or EDIT turn the topics.
static const char *const ab_name[AB_N] = {"EVERYWHERE", "HOME", "INSIDE A PAD", "STEPS", "PIANO ROLL", "SOUND", "FX AND MOD", "MIXER", "PADS, KEYS, RECORD", "PROJECT AND FILES", "SIMPLE VIEW"};
static const char *const ab_rows[AB_N][19] = {
    {"PLAY play / stop", "Y back", "EDIT open, do it", "EDIT+ARROWS change the value (held: shown big)", "OPT listen to the pad or the step", "OPT+EDIT delete, or the default value",
     "F1/F2 the next pad (on HOME: the next screen)", "F1+F2 PROJECT, again: back where you were", "F1+F2-3S the simple view (hold)", "SHIFT+ARROW inside a pad: straight to a category",
     "SHIFT+L2 undo", "SHIFT+OPT copy", "SHIFT+EDIT paste", "X the quick bar, again: its drawer", "MENU+PLAY record, twice: capture what was played", "MENU+ARROWS the screen map",
     "SHIFT+PLAY-3S quit BrickPad (hold)", "MENU+X metronome: off, sound, +vibration, vibration", "#BRICKPAD 1.2 BETA  (C) 2026 TIMOTHY GURTO (OBHATEION)"},
    {"EDIT open the pad (an empty one: choose its engine)", "EDIT+PLAY start / stop the clip of this pad", "PLAY all clips", "OPT+v mute", "OPT+^ solo", "OPT+> freeze the pad",
     "LOOK (inside the pad) its name and icon", "SHIFT+DPAD select several pads", "OPT+EDIT clear the pad (YES / NO); an empty one: RESAMPLE here", "F1/F2 PADS, SCENES, MIXER", "#SCENES", "EDIT launch the scene (its own twelve pads)", "+ COPY OF SCENE: a copy of the open one", "OPT+EDIT delete it (YES / NO)", "+ NEW SCENE: empty pads",
     "#UP TO 16 SCENES; TEMPO, KEY, MASTER, RETURNS ARE SHARED"},
    {"EDIT open the category or the page", "ARROWS choose", "Y back up: page, category, HOME", "F1/F2 the next pad, the same page",
     "SHIFT+UP PATTERN", "SHIFT+RIGHT SOUND", "SHIFT+DOWN FX", "SHIFT+LEFT MIX", "OPT+EDIT on a card or a list: clear it (YES / NO)", "#LOOK: THE ICON AND ITS COLOUR", "EDIT set", "L1/R1 the group of icons",
     "DOWN from the last row: the colours", "#PAD MENU AS A TREE (PROJECT > SETTINGS)", "<> the level: HOME, pad, category, pages", "^v choose", "EDIT open it"},
    {"EDIT a note on the step, or off", "EDIT+ARROWS the value of the step", "L1/R1 the lane: note, velocity, chance...", "OPT listen", "OPT+<> walk and listen",
     "X+<> the next pad", "OPT+EDIT delete the step, or its default", "XX (twice) double the clip", "SHIFT+X random", "SHIFT+L1/R1 the grid (NOTE lane)", "EDIT on SUB / PRB: the cells / the mode", "#PATTERN CARD: EDIT+ARROWS STEPS, PIANO ROLL, CHORDS", "#SELECT: THE 16 PATTERNS OF THE PAD",
     "EDIT choose the pattern", "OPT+EDIT clear it", "SHIFT+OPT copy", "SHIFT+EDIT paste", "EDIT+ARROWS on THEN: what plays after it"},
    {"EDIT a note here; on a note: delete it", "EDIT+ARROWS on a note: its length and pitch", "X+ARROWS velocity and chance", "X+EDIT a chord", "L1/R1 the grid",
     "OPT+<> the next note", "SHIFT+ARROWS a page, an octave (the jump is not here)", "SHIFT+X random", "Y back"},
    {"L1/R1 the page", "EDIT+ARROWS the value", "EDIT hear it", "OPT listen", "OPT+ARROWS listen to another note", "OPT+EDIT the default value",
     "#ON THE PICTURE", "EDIT the file, the presets or the slice editor", "EDIT+<> the next file", "#PATCH (GRAND)", "EDIT take a cable, then EDIT on an input",
     "OPT+EDIT pull the cables", "Y drop the cable", "#FILES", "EDIT load, or open the folder", "OPT listen", "< up a folder", "X the file menu"},
    {"#FX", "L1/R1 insert 1 / 2", "EDIT change the effect", "EDIT+<> its type", "EDIT+ARROWS the value", "X on / off", "OPT+EDIT remove, or the default",
     "#LFO", "L1/R1 LFO 1-4", "EDIT on TARGET: pick it on SOUND", "OPT+EDIT unlink, LFO off", "#AUTOMATION", "EDIT a point", "X+^v bend", "EDIT+^v its value",
     "OPT+EDIT clear", "#DUCK", "X a point, X+^v its bend", "EDIT+ARROWS level / move, L1/R1 the pad"},
    {"<> the pad", "EDIT+^v its level (EDIT+<> fine)", "Y mute", "X solo", "SHIFT+Y solo", "L1/R1 the drawer, FREEZE", "#THE DRAWER: RETURNS, MASTER, THE PAD'S EFFECTS",
     "EDIT on a pad: open, close (or R1, or past the last pad)", "<> reverb, delay, master", "^v level and two parameters", "EDIT+^v the value", "EDIT all the parameters of the unit",
     "Y close it", "#FREEZE", "EDIT freeze the pad, or export", "#PROJECT > OUTPUT: STRIP EFFECTS, SCENE MIX SHARED / OWN"},
    {"#PADS (MENU)", "BUTTONS the clip or the hit of a pad", "SHIFT+BUTTON the other of the two", "PLAY the pad's clip", "SHIFT+F1/F2 the scene",
     "#KEYS (MENU AGAIN)", "BUTTONS notes", "SHIFT+^v octave", "SHIFT+<> scale", "SHIFT+L1/R1 key", "MENU back", "#RECORD (PATTERN > RECORD)",
     "EDIT arm, stop", "MENU+PLAY arm from anywhere", "OPT a hit", "Y cancel the count",
     "#COUNT-IN IS IN PROJECT > PLAY / REC: OFF WAITS FOR THE FIRST NOTE"},
    {"^v the section, the setting", "> into the section", "< back to the sections", "EDIT+<> the value", "EDIT do it (TEMPO: tap it)", "Y back",
     "#A NAME", "EDIT type the letter", "PLAY ok", "OPT delete", "L1/R1 the place", "#PRESETS", "EDIT load", "L1/R1 factory / yours", "OPT listen", "OPT+EDIT delete",
     "#FILES", "X the menu: new folder, rename, cut, delete"},
    {"F1/F2 the screen: PLAY, NOTES, SOUND", "L1/R1 the pad (on PLAY: the scene)", "F1+F2-3S leave the simple view (hold)", "MENU-3S quit BrickPad (hold)",
     "#PLAY", "EDIT play the pad", "EDIT+^v volume", "Y quiet", "#NOTES", "EDIT a note", "EDIT+^v higher / lower", "#SOUND", "<> choose the knob", "^v turn it",
     "SHIFT+X dice: a random sound", "#KID LOCK (PROJECT > SETTINGS): NOTHING IS ERASED"},
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
    Col ec = th_cur[TC_SMP]; int r[JS_ROWS], n = prj_rows(jsec, r);
    { int ok = 0; for (int i = 0; i < n; i++) if (r[i] == jcur) ok = 1; if (!ok) jcur = r[0]; }
    for (int k = 0; k < JS_N; k++) { int y = 76 + k * 70, on = k == jsec;
        rrect(26, y, 249, 60, 8, on ? (jcol ? mix(C_SELBG, C_CELL, 128) : C_SELBG) : C_CELL);
        ftext(48, y + 16, FT_MB, on ? C_WHITE : C_DIM, prj_sec_name[k]); }
    if (jsec == JS_HELP) { draw_about(); draw_footer(jcol ? "^v TOPIC  EDIT NEXT  Y BACK  < SECTIONS" : "EDIT ENTER  Y BACK  ^v SECTION"); return; }
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
    draw_footer(!jcol ? "EDIT ENTER  OPT+EDIT RESET IT  ^v SECTION" : jcur == J_TEMPO ? "EDIT TAP IN TIME  EDIT+<> VALUE  Y BACK  L1/R1 HELD NUDGE  < SECTIONS  OPT+EDIT DEFAULT"
                                                    : "EDIT+<> VALUE  EDIT DO IT  Y BACK  < SECTIONS  OPT+EDIT DEFAULT");
}

// ---- MIXER > RETURN A / RETURN B / MASTER: every parameter of the unit as cards (the 1.2 cursor: fcol the column, frow the row) ----
static int v2_unit(void) { return tab == T_FX && fx_unit >= NSLOT; }
static void draw_unit2(void) {
    int u = fx_unit, n = unit_nparams(u), half = unit_split(u); Col ec = u == NSLOT + 2 ? th_cur[TC_SMP] : C_CAT(PC_MIX);
    mx_row();                            // 2026-11-24: the mixer's tab row (PADS RETURN A RETURN B MASTER FREEZE)
    int gr = u == NSLOT + 2, rows[2] = {half, n - half + gr};   // MASTER: a meter of the gain reduction under LIMITER
    int rm = rows[0] > rows[1] ? rows[0] : rows[1], hh = (700 - 126 - (rm - 1) * 12) / rm; if (hh > 136) hh = 136;   // both columns one height: the rows line up
    for (int k = 0; k < n; k++) { int c = k >= half, r = c ? k - half : k, x = c ? 518 : 32, h = hh, y = 126 + r * (h + 12), on = !tab_focus && fcol == c && frow == r, lo, hi, def; char b[16];
        int v = *unit_param(u, k, &lo, &hi, &def); unit_pstr(u, k, v, b); units(b);
        const char *nm = u == NSLOT ? rv_name[k] : u == NSLOT + 1 ? dl_name[k] : ms_name[k];
        if (on) { rframe(x, y, 473, h, 8, 4, ec, C_CELL); ui_focus(nm, b, hi - lo > 1 ? (v - lo) * 100 / (hi - lo) : -1, ec); } else rrect(x, y, 473, h, 8, C_CELL);
        ftext(x + 20, y + 12, FT_M, C_DIM, nm);
        if (hi - lo == 1) { int lw = 140, lh = h - 24 > 70 ? 70 : h - 24, lx = x + 453 - lw, ly = y + (h - lh) / 2;   // ON / OFF: a lamp switch (2026-11-24)
            rrect(lx, ly, lw, lh, 10, v ? ec : C_SELBG); ftext(lx + lw / 2 - ftext_w(FT_LB, b) / 2, ly + lh / 2 - fv2[FT_LB].h / 2, FT_LB, v ? C_BG : C_DIM, b); continue; }
        ftext(x + 453 - ftext_w(FT_XB, b), y + 10, FT_XB, C_WHITE, b);
        if (hi - lo > 1) { rect(x + 20, y + h - 26, 433, 8, C_LINE, 1); rect(x + 20, y + h - 26, 433 * clampi(v - lo, 0, hi - lo) / (hi - lo), 8, ec, 1); } }
    if (gr) { int h = hh, y = 126 + (rows[1] - 1) * (h + 12); float g = engine_master_gr(); char b[24];
        rrect(518, y, 473, h, 8, C_CELL); ftext(538, y + 12, FT_M, C_DIM, "GAIN REDUCTION"); snprintf(b, sizeof b, "%.1f dB", g > 0.05f ? -g : 0.f);
        ftext(971 - ftext_w(FT_XB, b), y + 10, FT_XB, g > 0.1f ? C_ORANGE : C_WHITE, b);
        rect(538, y + h - 26, 433, 8, C_LINE, 1); rect(538 + 433 - (int)(433 * fminf(g, 24) / 24), y + h - 26, (int)(433 * fminf(g, 24) / 24), 8, C_ORANGE, 1); }
    draw_footer("EDIT+ARROWS VALUE  ^ PAGES  Y BACK  EDIT ON/OFF  OPT+EDIT DEFAULT");
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
    draw_footer("BUTTONS PLAY  SHIFT+OPT LEAVE  SHIFT+^v OCTAVE  SHIFT+<> SCALE  SHIFT+L1/R1 KEY  MENU+PLAY RECORD");
}
