// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// WAVE's screens (2026-11-25, docs/WAVE.md section 10 on the v3 SOUND pages): the pictures on top of its pages, the
// MATRIX page (the macros, then the 16 slots as a list) and the TABLE browser (the built-in tables, then the WAVs in
// wavetables/ beside the binary). sound.h includes it once.
#pragma once

static const WaveStore *wv_ws(void) { return (const WaveStore *)engine_wstore(); }
static const float *wv_frame(int tab, float pos, float *mix_to, float *fr) {   // the frames either side of POS, and how far between
    const WaveStore *ws = wv_ws(); const WaveTable *t = tab >= 0 && tab < WT_SLOTS ? &ws->t[tab] : NULL;
    if (!t || !t->d || t->nf < 1) return NULL;
    float f = pos / 100.f * (t->nf - 1); int i = (int)f; if (i > t->nf - 1) i = t->nf - 1; if (i < 0) i = 0;
    *fr = f - i; *mix_to = 0;
    if (i + 1 < t->nf) *mix_to = 1;
    return t->d + (size_t)i * WT_STRIDE + wt_loff[0] + 1;
}
static float wv_frame_at(const float *f0, float fr, int has_next, float x) {   // the morphed frame's value at phase x (0..1)
    int k = (int)(x * WT_LEN) & (WT_LEN - 1); float a = f0[k];
    if (has_next && fr > 0) a += (f0[WT_STRIDE + k] - a) * fr;
    return a;
}
static void wv_draw_cycle(int tab, float pos, int x, int y, int w, int h, Col c, int th) {   // one cycle of the frame at POS
    float nx, fr; const float *f = wv_frame(tab, pos, &nx, &fr); if (!f) return;
    int px = x, py = y + h / 2;
    for (int i = 0; i <= w; i += 2) { float v = wv_frame_at(f, fr, nx > 0, (float)i / (w + 1)); if (v > 1) v = 1; if (v < -1) v = -1;
        int qx = x + i, qy = y + h / 2 - (int)(v * (h / 2 - 3)); if (i) line(px, py, qx, qy, th, c); px = qx; py = qy; }
}
static void wv_pic_osc(int o, int x, int y, int w, int h, Col ec) {   // OSC A / B: the frame at POS big, the table's frames as a strip, POS on it
    const int8_t *q = song.wv[sel] + 16 * o; int tab = q[WO_TABLE], on = q[WO_ON]; const WaveTable *t = &wv_ws()->t[clampi(tab, 0, WT_SLOTS - 1)];
    Col c = on ? ec : mix(ec, C_CELL, 170); char b[48], nm[32];
    rect(x + 12, y + 10, w - 24, h - 20, C_BG, 1);
    wp_str(WP_A + WO_TABLE, tab, nm, sizeof nm, wv_ws());
    int sh = 56, cy = y + 34, ch = h - 34 - sh - 22;   // the big cycle, and the strip under it
    rect(x + 24, cy + ch / 2, w - 48, 1, C_LINE, 1);
    if (!t->d) { snprintf(b, sizeof b, "%s: NOT LOADED (EDIT: THE TABLES)", nm); ftext(x + w / 2 - ftext_w(FT_M, b) / 2, cy + ch / 2 - 12, FT_M, C_DIM, b); }
    else wv_draw_cycle(tab, q[WO_POS], x + 24, cy, w - 48, ch, c, 3);
    snprintf(b, sizeof b, "%s  %s", o ? "OSC B" : "OSC A", nm); ftext(x + 22, y + 14, FT_S, on ? C_WHITE : C_DIM, b);
    if (t->d) { snprintf(b, sizeof b, "%d FRAMES", t->nf); ftext(x + w - 22 - ftext_w(FT_S, b), y + 14, FT_S, C_DIM, b); }
    if (!on) ftext(x + w / 2 - ftext_w(FT_MB, "OFF") / 2, y + 14, FT_MB, C_DIM, "OFF");
    if (!t->d) return;
    int sy = y + h - sh - 14, nth = t->nf < 24 ? t->nf : 24, tw = (w - 48) / nth;   // the strip: up to 24 frames, the one at POS lit
    for (int i = 0; i < nth; i++) { float fp = nth > 1 ? i * 100.f / (nth - 1) : 0; int tx = x + 24 + i * tw;
        int lit = fabsf(fp - q[WO_POS]) <= 50.f / (nth > 1 ? nth - 1 : 1);
        rrect(tx + 1, sy, tw - 2, sh, 4, lit ? mix(ec, C_BG, 120) : C_CELL);
        wv_draw_cycle(tab, fp, tx + 4, sy + 4, tw - 8, sh - 8, lit ? C_WHITE : mix(C_GREY, C_CELL, 60), 1); }
    int mx = x + 24 + (int)((w - 48 - tw) * q[WO_POS] / 100.f) + tw / 2; rect(mx - 1, sy - 6, 3, sh + 12, C_WHITE, 1);   // POS as set
    float lvl[8], env[3], fc, pos[2] = {-1, -1};
    if (engine_wave_live(sel, lvl, env, &fc, pos) && pos[o] >= 0 && t->nf > 1) {   // POS as it plays (the matrix and the pad LFOs move it)
        int lx = x + 24 + (int)((w - 48 - tw) * pos[o] / (t->nf - 1)) + tw / 2; rect(lx - 1, sy - 6, 3, sh + 12, C_ORANGE, 1); }
}
static void wv_pic_uni(int x, int y, int w, int h, Col ec) {   // UNISON: each oscillator's voices across pitch (DETUNE) and the stereo field (WIDTH)
    const int8_t *wp = song.wv[sel];
    for (int o = 0; o < 2; o++) { const int8_t *q = wp + 16 * o; int bx = x + 12 + o * (w - 12) / 2, bw = (w - 36) / 2, n = q[WO_UNI] < 1 ? 1 : q[WO_UNI];
        Col c = q[WO_ON] ? ec : mix(ec, C_CELL, 170); char b[40];
        rect(bx, y + 10, bw, h - 20, C_BG, 1); rect(bx + bw / 2, y + 40, 1, h - 70, C_LINE, 1);
        snprintf(b, sizeof b, "%s  %d VOICE%s", o ? "OSC B" : "OSC A", n, n > 1 ? "S" : ""); ftext(bx + 10, y + 14, FT_S, q[WO_ON] ? C_WHITE : C_DIM, b);
        for (int u = 0; u < n; u++) { float sp = n > 1 ? (float)u / (n - 1) * 2 - 1 : 0, det = sp * q[WO_DET] * 0.5f, pan = sp * q[WO_WIDTH] / 100.f + q[WO_PAN] / 50.f;
            int vx = bx + bw / 2 + (int)(det / 50.f * (bw / 2 - 20)), vy = y + h / 2 + (int)(clampi((int)(pan * 100), -100, 100) * (h / 2 - 50) / 100);
            rect(vx - 1, y + 44, 3, h - 78, mix(c, C_BG, 150), 1); disc2(vx, vy, 9, c); }
        ftext(bx + 10, y + h - 30, FT_S, C_DIM, "PITCH \004   STEREO \003"); }
}
static void wv_pic_sub(int x, int y, int w, int h, Col ec) {   // SUB / NOISE: the sub's wave an octave or two down, the noise's colour
    const int8_t *wp = song.wv[sel]; int bw = (w - 36) / 2; char b[32];
    for (int o = 0; o < 2; o++) rect(x + 12 + o * (bw + 12), y + 10, bw, h - 20, C_BG, 1);
    int sl = wp[WP_SUB_LEVEL]; Col c = sl ? ec : mix(ec, C_CELL, 170), cs[4] = {c, c, c, c}; (void)cs;
    { int sx = x + 24, sw = bw - 24, cy = y + h / 2, amp = (h / 2 - 40) * (sl ? 20 + sl * 80 / 100 : 30) / 100, per = wp[WP_SUB_OCT] >= 2 ? 1 : 2, px = sx, py = cy;
      for (int i = 0; i <= sw; i += 2) { float ph = fmodf(i * (float)per / sw, 1.f), v;
          switch (wp[WP_SUB_SHAPE] & 3) { case 1: v = 1 - 4 * fabsf(ph - 0.5f); break; case 2: v = ph < 0.5f ? 1 : -1; break; case 3: v = 2 * ph - 1; break; default: v = sinf(ph * 6.2831853f); }
          int qx = sx + i, qy = cy - (int)(v * amp); if (i) line(px, py, qx, qy, 3, c); px = qx; py = qy; }
      snprintf(b, sizeof b, "SUB  %s  -%d OCT%s", wv_sub_names[wp[WP_SUB_SHAPE] & 3], wp[WP_SUB_OCT], wp[WP_SUB_DIRECT] ? "  DIRECT" : ""); ftext(x + 22, y + 14, FT_S, sl ? C_WHITE : C_DIM, b); }
    { int nx = x + 24 + bw + 12, nw = bw - 24, nl = wp[WP_NOISE_LEVEL], pink = wp[WP_NOISE_TYPE]; Col c2 = nl ? (pink ? mix(ec, C_ORANGE, 120) : C_WHITE) : mix(C_GREY, C_CELL, 80);
      unsigned r = (unsigned)(SDL_GetTicks() / 90) * 2654435761u; float lp = 0;
      for (int i = 0; i < nw; i += 3) { r = r * 1103515245u + 12345u; float v = ((r >> 9) & 1023) / 511.5f - 1; lp += (v - lp) * (pink ? 0.25f : 1.f); float u = pink ? lp * 1.8f : v;
          int hh = (int)(u * (h / 2 - 44) * (nl ? 0.3f + nl * 0.7f / 100 : 0.2f)); rect(nx + i, y + h / 2 - (hh > 0 ? hh : 0), 2, abs(hh) + 1, c2, 1); }
      snprintf(b, sizeof b, "NOISE  %s%s", pink ? "PINK" : "WHITE", wp[WP_NOISE_DIRECT] ? "  DIRECT" : ""); ftext(x + 24 + bw, y + 14, FT_S, nl ? C_WHITE : C_DIM, b); }
}
static float wv_resp(int ft, float f, float fc, float res) {   // the filter's level at f (a drawing, from the textbook forms)
    float w = f / fc, q = 0.5f + res * 12.f, q2 = 0.7071f, lp = 1 / sqrtf((1 - w * w) * (1 - w * w) + w * w / (q * q));
    switch (ft) {
    case WF_LP12: return lp;
    case WF_LP24: return lp / sqrtf((1 - w * w) * (1 - w * w) + w * w / (q2 * q2));
    case WF_HP12: return lp * w * w;
    case WF_BP: return lp * w / q;
    case WF_NOTCH: return lp * fabsf(1 - w * w);
    case WF_LADDER: { float k = res * 3.9f, re = (1 - w * w) * (1 - w * w) - 4 * w * w + k, im = 4 * w * (1 - w * w); return (1 + k * 0.35f) / sqrtf(re * re + im * im) * (1 + k) / (1 + k * 0.35f); }   // 1 / ((1 + jw)^4 + k), level kept
    }
    return 1;
}
static void wv_pic_filter(int x, int y, int w, int h, Col ec) {   // the response on 20 Hz .. 20 kHz, the cutoff as set, the cutoff now
    const int8_t *wp = song.wv[sel]; int ft = wp[WP_F_TYPE] % WF_N, ix = x + 24, iw = w - 48, top = y + 34, bot = y + h - 30, px = ix, py = bot; char b[48];
    rect(x + 12, y + 10, w - 24, h - 20, C_BG, 1);
    float fc = wv_cut_hz(wp[WP_F_CUT]), res = wp[WP_F_RES] / 100.f;
    for (int i = 0; i <= iw; i += 2) { float f = 20.f * powf(1000.f, (float)i / iw), a = ft == WF_OFF ? 1 : wv_resp(ft, f, fc, res), db = 20 * log10f(a > 1e-4f ? a : 1e-4f);
        int qy = top + (int)((12 - db) / 60.f * (bot - top)); qy = clampi(qy, top, bot);
        rect(ix + i, qy, 2, bot - qy, mix(C_CELL, ec, 60), 1); if (i) line(px, py, ix + i, qy, 2, ec); px = ix + i; py = qy; }
    int cx = ix + (int)(log10f(fc / 20.f) / 3 * iw); rect(clampi(cx, ix, ix + iw), top, 1, bot - top, C_DIM, 1);
    float lv[8], env[3], fl, pos[2]; if (ft && engine_wave_live(sel, lv, env, &fl, pos) && fl > 20) {   // the cutoff now: ENV2, KEY, the matrix
        int lx = ix + (int)(log10f(fl / 20.f) / 3 * iw); rect(clampi(lx, ix, ix + iw), top, 3, bot - top, C_ORANGE, 1); }
    snprintf(b, sizeof b, "%s", wv_filt_names[ft]); ftext(x + 22, y + 14, FT_S, C_WHITE, b);
    ftext(ix, bot + 4, FT_S, C_DIM, "20HZ"); ftext(ix + iw / 2 - 20, bot + 4, FT_S, C_DIM, "630HZ"); ftext(ix + iw - 50, bot + 4, FT_S, C_DIM, "20KHZ");
}
static void wv_pic_env(int x, int y, int w, int h, Col ec) {   // the envelope the selector shows, its level now
    const int8_t *e = song.wv[sel] + WP_ENV1 + wv_env * WE_N; char b[48];
    rect(x + 12, y + 10, w - 24, h - 20, C_BG, 1);
    snd_env(x + 24, y + 30, w - 48, h - 50, e[WE_ATT], e[WE_DEC], e[WE_SUS], e[WE_REL], ec);
    snprintf(b, sizeof b, "ENV%d  %s", wv_env + 1, wv_env == 0 ? "THE VOLUME" : wv_env == 1 ? "THE FILTER (ENV AMT) AND THE MATRIX" : "THE MATRIX"); ftext(x + 22, y + 14, FT_S, C_WHITE, b);
    float lv[8], env[3], fc, pos[2]; if (engine_wave_live(sel, lv, env, &fc, pos)) { int top = y + 40, bot = y + h - 30, ly = bot - (int)(env[wv_env] * (bot - top));
        rect(x + 24, ly, w - 48, 1, mix(C_ORANGE, C_CELL, 120), 1); disc2(x + w - 40, ly, 7, C_ORANGE); }
}
static void wv_pic_lfo(int x, int y, int w, int h, Col ec) {   // the LFO the selector shows: two cycles of its shape, from its PHASE
    const int8_t *l = song.wv[sel] + WP_LFO1 + wv_lfo * WL_N; int ix = x + 24, iw = w - 48, iy = y + 34, ih = h - 60, px = ix, py = iy + ih / 2; char b[48], rt[24];
    rect(x + 12, y + 10, w - 24, h - 20, C_BG, 1); rect(ix, iy + ih / 2, iw, 1, C_LINE, 1);
    unsigned r = 11; float hold = 0, prev = 0, nxt = 0; int lc = -1;
    for (int i = 0; i <= iw; i += 2) { float t = (float)i * 2 / iw + l[WL_PHASE] / 100.f, ph = t - floorf(t), v; int cyc = (int)(t * 4);
        if (cyc != lc) { lc = cyc; r = r * 1103515245u + 12345u; prev = nxt; nxt = ((r >> 16) & 255) / 127.5f - 1; hold = nxt; }
        switch (l[WL_SHAPE] % WLS_N) { case WLS_TRI: v = 1 - 4 * fabsf(ph - 0.5f); break; case WLS_SAWUP: v = 2 * ph - 1; break; case WLS_SAWDN: v = 1 - 2 * ph; break;
            case WLS_SQUARE: v = ph < 0.5f ? 1 : -1; break; case WLS_SH: v = hold; break; case WLS_SMOOTH: { float u = t * 4 - floorf(t * 4); v = prev + (nxt - prev) * (0.5f - 0.5f * cosf(u * 3.14159f)); break; }
            default: v = sinf(ph * 6.2831853f); }
        int qx = ix + i, qy = iy + ih / 2 - (int)(v * (ih / 2 - 4)); if (i) line(px, py, qx, qy, 3, ec); px = qx; py = qy; }
    wv_par_str(sel, WP_LFO1 + wv_lfo * WL_N + WL_RATE, l[WL_RATE], rt, sizeof rt);
    snprintf(b, sizeof b, "LFO%d  %s  %s  %s", wv_lfo + 1, wv_lfo_names[l[WL_SHAPE] % WLS_N], rt, wv_trig_names[l[WL_TRIG] % 3]); ftext(x + 22, y + 14, FT_S, C_WHITE, b);
}
static void wv_pic_voice(int x, int y, int w, int h, Col ec) {   // VOICE: the 8 voices, their level now; the ones VOICES leaves out dim
    const int8_t *wp = song.wv[sel]; float lv[8] = {0}, env[3], fc, pos[2]; int nv = engine_wave_live(sel, lv, env, &fc, pos), poly = clampi(wp[WP_POLY], 1, WV_POLY), bw = (w - 48) / WV_POLY; char b[48];
    rect(x + 12, y + 10, w - 24, h - 20, C_BG, 1);
    for (int i = 0; i < WV_POLY; i++) { int bx = x + 24 + i * bw, top = y + 40, bot = y + h - 24, bh = (int)(fminf(1.f, lv[i] * 1.4f) * (bot - top));
        rrect(bx + 4, top, bw - 8, bot - top, 6, i < poly ? C_CELL : mix(C_CELL, C_BG, 160)); if (bh > 2) rrect(bx + 4, bot - bh, bw - 8, bh, 6, ec); }
    snprintf(b, sizeof b, "%s  %d SOUNDING%s", poly == 1 ? "MONO" : "POLY", nv, wp[WP_QUALITY] ? "  HIGH" : "  ECO"); ftext(x + 22, y + 14, FT_S, C_WHITE, b);
}
static void wv_picture(int gfx, int x, int y, int w, int h, Col ec) {
    switch (gfx) {
    case GF_WTA: wv_pic_osc(0, x, y, w, h, ec); break; case GF_WTB: wv_pic_osc(1, x, y, w, h, ec); break;
    case GF_WUNI: wv_pic_uni(x, y, w, h, ec); break; case GF_WSUB: wv_pic_sub(x, y, w, h, ec); break;
    case GF_WFILT: wv_pic_filter(x, y, w, h, ec); break; case GF_WENV: wv_pic_env(x, y, w, h, ec); break;
    case GF_WLFO: wv_pic_lfo(x, y, w, h, ec); break; case GF_WVOICE: wv_pic_voice(x, y, w, h, ec); break;
    }
}
// ---- MATRIX: the macros on top, 8 slots of the 16 a page (the cursor's half), SRC DEST AMT MODE a row ----
static void draw_matrix2(Col ec) {
    const int8_t *wp = song.wv[sel]; char b[48]; int y = 127, cw = ww / 5;
    for (int m = 0; m < 5; m++) { int x = wx0 + m * cw, on = sp_cur == -1 - m, v = wp[WP_MACRO1 + m];   // MACRO 1-4, MODWHEEL
        if (on) rframe(x + 2, y, cw - 4, 66, 8, 3, ec, C_CELL); else rrect(x + 2, y, cw - 4, 66, 8, C_CELL);
        ftext(x + 12, y + 6, FT_S, on ? C_WHITE : C_DIM, m < 4 ? wp_name(WP_MACRO1 + m) : "MODWHEEL");
        rrect(x + 12, y + 38, cw - 24, 14, 5, C_BG); if (v) rrect(x + 12, y + 38, (cw - 24) * v / 100, 14, 5, ec);
        snprintf(b, sizeof b, "%d", v); ftext(x + cw - 14 - ftext_w(FT_S, b), y + 6, FT_S, C_WHITE, b);
        if (on) { snprintf(b, sizeof b, "%d", v); ui_focus(m < 4 ? wp_name(WP_MACRO1 + m) : "MODWHEEL", b, v, ec); } }
    int half = sp_cur >= 32 ? 8 : 0, y0 = y + 80, rh = 58, colx[5] = {0, 70, 290, 560, 820}; static const char *const hd[4] = {"SOURCE", "DESTINATION", "AMOUNT", "MODE"};
    for (int c = 0; c < 4; c++) ftext(wx0 + colx[c + 1] * ww / 971 + 14, y0, FT_S, C_DIM, hd[c]);
    for (int r = 0; r < 8; r++) { int sl = half + r, ry = y0 + 24 + r * rh; const int8_t *m = wp + WP_MTX + 4 * sl; int live = m[WM_SRC] && m[WM_DST] && m[WM_AMT];
        rrect(wx0, ry, ww, rh - 6, 8, sp_cur >= 0 && sp_cur / 4 == sl ? C_SELBG : C_CELL);
        snprintf(b, sizeof b, "M%d", sl + 1); ftext(wx0 + 12, ry + 14, FT_MB, live ? ec : C_DIM, b);
        for (int c = 0; c < 4; c++) { int k = WP_MTX + 4 * sl + c, x = wx0 + colx[c + 1] * ww / 971, x1 = wx0 + (c < 3 ? colx[c + 2] * ww / 971 : ww), on = sp_cur == 4 * sl + c;
            wp_str(k, wp[k], b, sizeof b, wv_ws()); fit(b, FT_MB, x1 - x - 24);
            if (on) rframe(x + 2, ry + 4, x1 - x - 8, rh - 14, 6, 3, ec, C_SELBG);
            if (c == WM_AMT) { int bw2 = x1 - x - 110, mid = x + 12 + bw2 / 2, a = m[WM_AMT] * (bw2 / 2) / 100;   // a bar from the middle
                rrect(x + 12, ry + 20, bw2, 12, 5, C_BG); if (a) rrect(a > 0 ? mid : mid + a, ry + 20, abs(a), 12, 5, ec);
                ftext(x1 - 90, ry + 14, FT_MB, C_WHITE, b); }
            else ftext(x + 14, ry + 14, FT_MB, m[WM_SRC] || c == WM_SRC ? C_WHITE : C_DIM, b);
            if (on) ui_focus(wp_mnames[4 * sl + c], b, -1, ec); } }
    int c = sp_cur >= 0 ? sp_cur % 4 : -1;
    draw_footer(learning ? "EDIT TAKE THIS PARAMETER  Y CANCEL" : c == WM_DST ? "EDIT POINT AT A CARD  EDIT+ARROWS DEST  OPT+EDIT CLEAR THE SLOT"
                : c >= 0 ? "EDIT+ARROWS VALUE  ^v SLOT  <> COLUMN  OPT+EDIT CLEAR THE SLOT" : "EDIT+ARROWS VALUE  v THE SLOTS  ^ TABS");
}

// ---- TABLE browser: EDIT on an oscillator's picture. The built-ins, then wavetables/*.wav (loaded into slots 12..31) ----
#define WT_DIR "wavetables"
#define WT_CFG WT_DIR "/slots.cfg"
static int wtb_osc, wtb_cur, wtb_n; static Entry wtb_e[128];   // (wtbing: main.c)
static char wt_file[WT_SLOTS][200];      // the file each user slot holds (UI side; slots.cfg keeps it from run to run)
static void wt_cfg_save(void) {
    char b[WT_SLOTS * 210]; int n = 0;
    for (int k = WT_NBUILT; k < WT_SLOTS; k++) if (wt_file[k][0]) n += snprintf(b + n, sizeof b - n, "%d %s\n", k, wt_file[k]);
    mkdir(WT_DIR, 0755); write_whole(WT_CFG, b, (size_t)n);
}
static void wt_boot(void) {              // start-up (UI thread, before the device): the user tables the last run had
    FILE *f = fopen(WT_CFG, "r"); if (!f) return; char ln[260], nm[200]; int k;
    while (fgets(ln, sizeof ln, f)) if (sscanf(ln, "%d %199[^\n]", &k, nm) == 2 && k >= WT_NBUILT && k < WT_SLOTS) {
        char p[260]; snprintf(p, sizeof p, WT_DIR "/%s", nm); if (!engine_wave_table(k, p)) snprintf(wt_file[k], sizeof wt_file[k], "%s", nm); }
    fclose(f);
}
static int wt_take(const char *nm) {     // a WAV from wavetables/ into a slot (the one it is in already, else a free one, else the last); -1 failed
    int k = -1; for (int i = WT_NBUILT; i < WT_SLOTS; i++) if (!strcmp(wt_file[i], nm)) return i;
    for (int i = WT_NBUILT; i < WT_SLOTS && k < 0; i++) if (!wt_file[i][0]) k = i;
    if (k < 0) k = WT_SLOTS - 1;
    char p[260]; snprintf(p, sizeof p, WT_DIR "/%s", nm); int r = engine_wave_table(k, p);
    if (r) { char b[64]; snprintf(b, sizeof b, r == -4 ? "NOT WHOLE FRAMES OF 2048" : "THIS WAV DOES NOT LOAD (%d)", r); toast(b); return -1; }
    snprintf(wt_file[k], sizeof wt_file[k], "%s", nm); wt_cfg_save(); return k;
}
static void wtb_open(int o) {
    wtbing = 1; wtb_osc = o; wtb_n = list_dir_ext(WT_DIR, wtb_e, 128, ".wav"); int t = song.wv[sel][16 * o + WO_TABLE]; wtb_cur = t < WT_NBUILT ? t : 0;
    if (t >= WT_NBUILT) for (int i = 0; i < wtb_n; i++) if (!wtb_e[i].dir && !strcmp(wtb_e[i].name, wt_file[t])) wtb_cur = WT_NBUILT + i;
}
static void wtb_apply(void) {            // the row under the cursor on the oscillator, at once (heard with OPTION)
    int t = -1;
    if (wtb_cur < WT_NBUILT) t = wtb_cur; else { int i = wtb_cur - WT_NBUILT; if (i < wtb_n && !wtb_e[i].dir) t = wt_take(wtb_e[i].name); }
    if (t < 0) return;
    edit_begin(); song.wv[sel][16 * wtb_osc + WO_TABLE] = (int8_t)t; song.wv[sel][16 * wtb_osc + WO_ON] = 1;
}
static void wtb_key(int k, int down) {
    if (!down && k == K_OPT) { engine_release_key(GB_TAG); return; }
    if (!down && k != K_EDIT) return;
    int n = WT_NBUILT + wtb_n;
    if (k == K_Y) { wtbing = 0; return; }
    if (k == K_EDIT) { if (down) return;   // on the release, as the presets: the press that opened it is not this one
        wtb_apply(); wtbing = 0; char b[48], nm[32]; wp_str(WP_A + WO_TABLE, song.wv[sel][16 * wtb_osc + WO_TABLE], nm, sizeof nm, wv_ws()); snprintf(b, sizeof b, "OSC %c: %s", 'A' + wtb_osc, nm); toast(b); return; }
    if (k == K_OPT) { wtb_apply(); engine_release_key(GB_TAG); engine_trig_key(sel, 1.f, song.lnote[sel], -1, GB_TAG); return; }
    if (k == K_UP || k == K_DOWN) wtb_cur = clampi(wtb_cur + (k == K_DOWN ? 1 : -1), 0, n - 1);
    if (k == K_L1 || k == K_R1) wtb_cur = k == K_R1 && wtb_n ? WT_NBUILT : 0;   // the two groups
}
static void draw_wtb(void) {
    Col ec = C_ENG(1); int n = WT_NBUILT + wtb_n, rows = 9, rh = 61, top = clampi(wtb_cur - rows / 2, 0, n > rows ? n - rows : 0); char b[64];
    snprintf(b, sizeof b, "OSC %c  TABLES", 'A' + wtb_osc); ftext(26, 80, FT_MB, ec, b);
    int lw = 560;   // the window is full width: the side slots are hidden while it is open (side.h)
    for (int r = 0; r < rows && top + r < n; r++) { int i = top + r, y = 130 + r * rh, on = i == wtb_cur;
        if (on) rrect(26, y, lw, 50, 8, C_SELBG);
        if (i < WT_NBUILT) { snprintf(b, sizeof b, "%s", wt_bnames[i]); ftext(48, y + 12, on ? FT_LB : FT_M, C_WHITE, b); ftext(26 + lw - 20 - ftext_w(FT_S, "BUILT-IN"), y + 16, FT_S, C_DIM, "BUILT-IN"); }
        else { const Entry *e = &wtb_e[i - WT_NBUILT]; snprintf(b, sizeof b, "%s%s", e->name, e->dir ? "/" : ""); fit(b, on ? FT_LB : FT_M, lw - 140); ftext(48, y + 12, on ? FT_LB : FT_M, e->dir ? C_DIM : C_WHITE, b);
            ftext(26 + lw - 20 - ftext_w(FT_S, "WAVETABLES/"), y + 16, FT_S, C_DIM, "WAVETABLES/"); } }
    if (!wtb_n) ftext(26, 130 + WT_NBUILT * 0 + rows * rh + 4, FT_S, C_DIM, "PUT WAVETABLE WAVS IN wavetables/ BESIDE THE APP");
    int px = 26 + lw + 20, pw = 997 - px, ph = 260;   // the preview: a built-in's first, middle and last frames
    rrect(px, 130, pw, ph, 8, C_CELL);
    if (wtb_cur < WT_NBUILT) for (int j = 0; j < 3; j++) wv_draw_cycle(wtb_cur, j * 50.f, px + 16, 150 + j * 76, pw - 32, 70, j == 1 ? ec : mix(ec, C_CELL, 120), 2);
    else ftext(px + 16, 150, FT_S, C_DIM, "EDIT LOADS IT");
    draw_footer("EDIT TAKE  OPT LISTEN  L1/R1 BUILT-IN / FILES  Y BACK");
}
