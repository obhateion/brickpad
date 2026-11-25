// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// PAD > SOUND (UI v2, step 4; mockups A08-A16): an engine's parameters as pages of four to six cards under a picture of
// what they shape. L1/R1 the page, arrows the card, EDIT+arrows the value; rare parameters on the last page, MORE.
// The pages are data: a name, a picture, the P_* ids. The cursor's parameter goes through cur_par(), so everything the
// 1.2 panel did with a control (EDIT+arrows, EDIT, OPT+EDIT, hearing it, LFO learn) works on a card.
// main.c includes this once, after ui.h.
#pragma once

static void gb_ring(int cx, int cy, int r, int t, Col c);
static const char *par_hint(int p);      // hints.h: what a parameter does, one sentence
static void wv_picture(int gfx, int x, int y, int w, int h, Col ec);   // wvui.h
static void draw_matrix2(Col ec);
enum { GF_NONE, GF_WAVE, GF_SLICES, GF_OSC, GF_FILTER, GF_ENV, GF_CLOUD, GF_ALL, GF_WTA, GF_WTB, GF_WUNI, GF_WSUB, GF_WFILT, GF_WENV, GF_WLFO, GF_WMTX, GF_WVOICE };   // GF_W*: WAVE (2026-11-25)
// v3 (2026-11-20, user: "five pages of MORE is no way to jam"): pages by topic, at most ten knobs each, all on one screen;
// MAIN shows the main curves side by side with the knobs that shape them. The engine is switched on the section strip
// (EDIT held on SOUND), not from a page.
typedef struct { const char *name; int8_t n, gfx; int16_t p[10]; } SndPage;
static const SndPage snd_smp[] = {
    {"SAMPLE", 7, GF_WAVE, {P_PITCH, P_ROOT, P_VOICE, P_DIR, P_START, P_END, P_FINE}},
    // 2026-11-24 (audit): SLICES had 2 cards, WARP often 2 shown: one page for both, the cut and the time
    {"SLICE/WARP", 8, GF_SLICES, {P_SLICES, P_SLWAY, P_WARP, P_NORM, P_BARS, P_BPM, P_GRAIN, P_FLUX}},
    {"AMP", 5, GF_WAVE, {P_SX + SX_FIN, P_SX + SX_FOUT, P_SX + SX_XFADE, P_VOL, P_PAN}},
};
// WAVE (2026-11-25, docs/WAVE.md section 10 on the v3 pages): the oscillators with their table on top, the unison spread,
// SUB and NOISE, the filter's curve, ENV and LFO with a selector card (1..3, 1..4: the cards follow it), the MATRIX as a
// list of its 16 slots, the voices, the ARP. Cards: P_WV + wp index (ENV / LFO pages: ENV1 / LFO1's, moved by the selector).
#define WVA(k) (P_WV + WP_A + WO_##k)
#define WVB(k) (P_WV + WP_B + WO_##k)
#define WVP(k) (P_WV + (k))
static const SndPage snd_wav[] = {
    {"OSC A", 10, GF_WTA, {WVA(ON), WVA(TABLE), WVA(POS), WVA(WARP), WVA(WAMT), WVA(LEVEL), WVA(OCT), WVA(SEMI), WVA(FINE), WVA(UNI)}},
    {"OSC B", 10, GF_WTB, {WVB(ON), WVB(TABLE), WVB(POS), WVB(WARP), WVB(WAMT), WVB(LEVEL), WVB(OCT), WVB(SEMI), WVB(FINE), WVB(UNI)}},
    {"UNISON", 10, GF_WUNI, {WVA(DET), WVA(WIDTH), WVA(PAN), WVA(PHASE), WVA(RAND), WVB(DET), WVB(WIDTH), WVB(PAN), WVB(PHASE), WVB(RAND)}},
    {"SUB", 8, GF_WSUB, {WVP(WP_SUB_LEVEL), WVP(WP_SUB_SHAPE), WVP(WP_SUB_OCT), WVP(WP_SUB_PAN), WVP(WP_SUB_DIRECT), WVP(WP_NOISE_LEVEL), WVP(WP_NOISE_TYPE), WVP(WP_NOISE_DIRECT)}},
    {"FILTER", 10, GF_WFILT, {WVP(WP_F_TYPE), WVP(WP_F_CUT), WVP(WP_F_RES), WVP(WP_F_DRIVE), WVP(WP_F_KEY), WVP(WP_F_ENV), WVP(WP_ENV2 + WE_ATT), WVP(WP_ENV2 + WE_DEC), WVP(WP_ENV2 + WE_SUS), WVP(WP_ENV2 + WE_REL)}},
    {"ENV", 6, GF_WENV, {P_WENV, WVP(WP_ENV1 + WE_ATT), WVP(WP_ENV1 + WE_DEC), WVP(WP_ENV1 + WE_SUS), WVP(WP_ENV1 + WE_REL), WVP(WP_ENV1 + WE_CURVE)}},
    {"LFO", 7, GF_WLFO, {P_WLFO, WVP(WP_LFO1 + WL_SHAPE), WVP(WP_LFO1 + WL_RATE), WVP(WP_LFO1 + WL_SYNC), WVP(WP_LFO1 + WL_TRIG), WVP(WP_LFO1 + WL_PHASE), P_WTGT}},
    {"MATRIX", 0, GF_WMTX, {0}},
    {"VOICE", 9, GF_WVOICE, {WVP(WP_POLY), WVP(WP_LEGATO), WVP(WP_GLIDE), WVP(WP_BEND), WVP(WP_VELSENS), WVP(WP_MASTER), WVP(WP_QUALITY), P_PITCH, P_VOL}},
    {"ARP", 5, GF_NONE, {WVP(WP_ARP), WVP(WP_ARP_MODE), WVP(WP_ARP_OCT), WVP(WP_ARP_RATE), WVP(WP_ARP_GATE)}},
};
#undef WVA
#undef WVB
#undef WVP
static const SndPage snd_acd[] = {
    {"MAIN", 10, GF_ALL, {P_AC + AP_WAVE, P_AC + AP_TUNE, P_AC + AP_CUTOFF, P_AC + AP_RESO, P_AC + AP_ENVMOD, P_AC + AP_DECAY, P_AC + AP_ACCENT,
        P_AC + AP_DRIVE, P_AC + AP_DMIX, P_VOL}},
    {"OSC", 7, GF_OSC, {P_AC + AP_WAVE, P_AC + AP_TUNE, P_AX + AX_SUB, P_AX + AX_SUBOCT, P_AX + AX_SUBWAVE, P_AX + AX_PW, P_AX + AX_NOISE}},
    {"FILTER", 8, GF_FILTER, {P_AC + AP_CUTOFF, P_AC + AP_RESO, P_AC + AP_ENVMOD, P_AC + AP_DECAY, P_AC + AP_ACCENT, P_AX + AX_TRACK, P_AX + AX_RESHP, P_AX + AX_BASS}},
    {"DIST", 5, GF_NONE, {P_AC + AP_DTYPE, P_AC + AP_DRIVE, P_AC + AP_DTONE, P_AC + AP_DMIX, P_AX + AX_CLIP}},
    {"VOICE", 7, GF_NONE, {P_AX + AX_VATK, P_AX + AX_VREL, P_AX + AX_SLIDE, P_AX + AX_RANGE, P_AX + AX_ACCATK, P_AX + AX_ACCDEC, P_VOL}},
};
static const SndPage snd_cld[] = {
    {"GRAINS", 8, GF_CLOUD, {P_CL + CP_POS, P_CL + CP_SPRAY, P_CL + CP_SIZE, P_CL + CP_DENSITY, P_CX + CX_SIZERND, P_CX + CX_WINDOW, P_CX + CX_TIMERND, P_CX + CX_REVERSE}},
    {"TONE", 7, GF_CLOUD, {P_CX + CX_PITCH, P_CX + CX_FINE, P_CX + CX_TONE, P_CL + CP_SHIMMER, P_CX + CX_SHMODE, P_CX + CX_PITCHRND, P_CX + CX_WIDTH}},
    {"ENV", 6, GF_CLOUD, {P_CX + CX_ATTACK, P_CX + CX_RELEASE, P_CL + CP_MOTION, P_CX + CX_FREEZE, P_CX + CX_SYNC, P_CX + CX_SCAN}},
    {"OUT", 3, GF_NONE, {P_VOL, P_NORM, P_SAMPLE}},
};
static int sp_pg[4], sp_cur;             // the page of each engine, kept; the card under the cursor, -1 = the picture

static const SndPage *snd_pages(int type, int *n) {
    switch (type & 3) {
    case 1: *n = (int)(sizeof snd_wav / sizeof *snd_wav); return snd_wav;
    case 2: *n = (int)(sizeof snd_acd / sizeof *snd_acd); return snd_acd;
    case 3: *n = (int)(sizeof snd_cld / sizeof *snd_cld); return snd_cld;
    }
    *n = (int)(sizeof snd_smp / sizeof *snd_smp); return snd_smp;
}
static int snd_page_n(void) { int n; snd_pages(song.t[sel].type, &n); return n; }   // the pages by number: the tree lists and opens them
static const char *snd_page_nm(int i) { int n; const SndPage *pg = snd_pages(song.t[sel].type, &n); return pg[clampi(i, 0, n - 1)].name; }
static const SndPage *snd_page(void) { int n; const SndPage *pg = snd_pages(song.t[sel].type, &n); return &pg[clampi(sp_pg[song.t[sel].type & 3], 0, n - 1)]; }
static int snd_cards(const SndPage *pg, int *out) {   // the cards on show: what the mode does not use is left out
    int c = 0; for (int i = 0; i < pg->n; i++) { int p = pg->p[i];
        if (pg->gfx == GF_WENV && p >= P_WV + WP_ENV1 && p < P_WV + WP_ENV2) p += wv_env * WE_N;   // the selector moves the cards
        if (pg->gfx == GF_WLFO && p >= P_WV + WP_LFO1 && p < P_WV + WP_LFO2) p += wv_lfo * WL_N;
        if (par_visible(p)) out[c++] = p; }
    return c;
}
static int v2_sound(void) { return ui_cat == PC_SOUND && tab == T_SEQ && !perform && srow >= 3; }
// MATRIX (WAVE): sp_cur = 4 * slot + column (SRC DEST AMT MODE), slots 0..15 in two pages of 8; -1 - k: MACRO k (0..4, the 5th the MODWHEEL)
#define MX_CELLS (WM_SLOTS * 4)
static int sound_par(void) {             // the parameter under the cursor; the picture stands for the file (or the presets)
    const SndPage *pg = snd_page(); int l[20], c = snd_cards(pg, l);
    if (pg->gfx == GF_WMTX) return sp_cur < 0 ? P_WV + WP_MACRO1 + clampi(-1 - sp_cur, 0, 4) : P_WV + WP_MTX + clampi(sp_cur, 0, MX_CELLS - 1);
    if (sp_cur >= 0 && sp_cur < c) return l[sp_cur];
    return pg->gfx == GF_SLICES ? P_SLICES : song.t[sel].type == 3 ? P_SRC : P_SAMPLE;
}
static void sound_sync(void) {           // after any change of pad, engine or mode: the page and the cursor exist
    int n; const SndPage *pgs = snd_pages(song.t[sel].type, &n); int *pp = &sp_pg[song.t[sel].type & 3];
    *pp = clampi(*pp, 0, n - 1); srow = 3; scol = 0;
    const SndPage *pg = &pgs[*pp]; int l[20], c = snd_cards(pg, l);
    cl_deep = 0;
    if (pg->gfx == GF_WMTX) { sp_cur = clampi(sp_cur, -5, MX_CELLS - 1); return; }
    if (sp_cur >= c) sp_cur = c - 1;
    if (sp_cur < 0 && pg->gfx == GF_NONE) sp_cur = 0;
    if (sp_cur < -1) sp_cur = -1;
}
static void sound_page_step(int d) {     // L1/R1
    int n; snd_pages(song.t[sel].type, &n); int *pp = &sp_pg[song.t[sel].type & 3];
    *pp = (*pp + d + n) % n; sp_cur = 0;
    sound_sync();
}
static void snd_page_go(int i) {         // a page by its number (the tree)
    int n; snd_pages(song.t[sel].type, &n); sp_pg[song.t[sel].type & 3] = clampi(i, 0, n - 1); sp_cur = 0; sound_sync();
}
static void sound_move(int k) {          // the knobs; UP from the first row: the picture
    const SndPage *pg = snd_page(); int l[20], c = snd_cards(pg, l);
    if (pg->gfx == GF_WMTX) {            // the macros' row on top, then a slot a row: up / down the slot, left / right the column
        if (sp_cur < 0) { int m = -1 - sp_cur; if (k == K_LEFT && m > 0) m--; else if (k == K_RIGHT && m < 4) m++; else if (k == K_DOWN) { sp_cur = 0; return; } sp_cur = -1 - m; return; }
        int sl = sp_cur / 4, col = sp_cur % 4;
        if (k == K_UP) { if (sl == 0 || sl == 8) { sp_cur = -1 - clampi(col, 0, 4); return; } sl--; } else if (k == K_DOWN && sl < WM_SLOTS - 1) sl++;
        else if (k == K_LEFT && col > 0) col--; else if (k == K_RIGHT && col < 3) col++;
        sp_cur = 4 * sl + col; return;
    }
    if (sp_cur < 0) { if (k == K_DOWN && c) sp_cur = 0; return; }   // the knobs in kg_cols columns (ui.h)
    if (k == K_UP && sp_cur < kg_cols(c) && pg->gfx == GF_NONE) return;   // no picture above
    kg_move(k, c, &sp_cur);
}
static int sound_edit_arrow(int k) {     // EDIT+arrows on the picture: the sampler's next file; nothing else changes there
    if (sp_cur >= 0 || snd_page()->gfx == GF_WMTX) return 0;   // (MATRIX: below 0 are its macros, not a picture)
    if (snd_page()->gfx == GF_SLICES) return 0;   // SLICES: the count, as on its card
    if (song.t[sel].type == 3) return 0;          // CLOUD: its source's next file (P_SRC)
    if (!is_synth(sel) && (k == K_LEFT || k == K_RIGHT)) { edit_begin(); neighbour_file(sel, k == K_RIGHT ? 1 : -1); }
    if (song.t[sel].type == 1 && (snd_page()->gfx == GF_WTA || snd_page()->gfx == GF_WTB) && (k == K_LEFT || k == K_RIGHT)) {   // WAVE: the next table
        int p = P_WV + (snd_page()->gfx == GF_WTB ? WP_B : WP_A) + WO_TABLE; par_adjust(sel, p, k == K_RIGHT ? 1 : -1, 0); }
    return 1;
}

// ---- the pictures ----
static void snd_wave(int x, int y, int w, int h, Col on, Col off, int slices) {   // the whole sample, START..END bright
    static float pk[960]; static const Sample *ps; static int pw;
    const Sample *s = engine_sample(sel);
    if (!s || s->n < 2) { const char *m = "no sample: EDIT on the picture loads one"; ftext(x + w / 2 - ftext_w(FT_M, m) / 2, y + h / 2 - 12, FT_M, C_DIM, m); return; }
    if (w > 960) w = 960;
    if (ps != s || pw != w) { ps = s; pw = w;
        for (int i = 0; i < w; i++) { long a = (long)s->n * i / w, b = (long)s->n * (i + 1) / w; float m = 0;
            for (long k = a; k < b; k += 16) { float v = fabsf(s->d[2 * k]); if (v > m) m = v; } pk[i] = m; } }
    int a = x + (int)((long)ed_start(s) * w / s->n), b = x + (int)((long)ed_end(s) * w / s->n), half = h / 2 - 8; float ng = norm_gain(sel);
    for (int i = 0; i < w; i += 3) { int hh = (int)(pk[i] * ng * half); if (hh > half) hh = half; if (hh < 1) hh = 1;
        rect(x + i, y + h / 2 - hh, 2, 2 * hh, x + i >= a && x + i <= b ? on : off, 1); }
    rect(a, y + 4, 3, h - 8, C_PLAY, 1); rect(b - 2, y + 4, 3, h - 8, C_RED, 1);   // START and END
    if (slices) { int ns = track_nslices(sel); char nb[4];
        for (int m = 0; m < ns; m++) { int sx = x + (int)((long)ed_pos(s, m) * w / s->n);
            if (m) rect(sx, y + 4, 1, h - 8, C_WHITE, 1);
            if (ns <= 32) { snprintf(nb, 4, "%d", m + 1); ftext(sx + 4, y + 2, FT_S, C_DIM, nb); } } }
}
static void snd_shape(int x, int y, int w, int h, int wave, Col c) {   // two periods of TRI SAW SQR PULSE
    int px = x, py = y + h / 2;
    for (int i = 0; i <= w; i += 2) { float ph = fmodf(i * 2.f / w, 1.f), v;
        switch (wave) { case 0: v = ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph; break; case 1: v = 2 * ph - 1; break;
                        case 2: v = ph < 0.5f ? 1 : -1; break; default: v = ph < 0.25f ? 1 : -1; }
        int nx = x + i, ny = y + h / 2 - (int)(v * (h / 2 - 6));
        if (i) line(px, py, nx, ny, 2, c);
        px = nx; py = ny; }
}
static void snd_filter(int x, int y, int w, int h, int cut, int res, Col c) {   // a low-pass: flat, the peak at the cutoff, the slope
    int fx = x + w * (8 + cut * 80 / 100) / 100, base = y + h * 55 / 100, px = x, py = base;
    for (int i = 0; i <= w; i += 2) { int xx = x + i; float d = (float)(xx - fx) / (w * 0.06f), v;
        float peak = res / 100.f * expf(-d * d);
        if (xx <= fx) v = 0.45f + peak * 0.5f; else { float f = (float)(xx - fx) / (w * 0.22f); v = (0.45f + peak * 0.5f) * (f < 1 ? 1 - f : 0); if (v < 0.02f) v = 0.02f; }
        int yy = y + h - 8 - (int)(v * (h - 20));
        rect(xx, yy, 2, y + h - 8 - yy, mix(C_CELL, c, 60), 1);
        if (i) line(px, py, xx, yy, 2, c);
        px = xx; py = yy; }
    rect(fx, y + 6, 1, h - 12, C_DIM, 1);
    (void)base;
}
static void snd_env(int x, int y, int w, int h, int a, int d, int s, int r, Col c) {   // attack, decay to the sustain level, release
    int seg = w / 4, x1 = x + 4 + seg * a / 100, x2 = x1 + 4 + seg * d / 100, x3 = x2 + seg, x4 = x3 + 4 + seg * r / 100;
    int top = y + 10, bot = y + h - 10, sy = bot - (bot - top) * s / 100;
    line(x, bot, x1, top, 2, c); line(x1, top, x2, sy, 2, c); line(x2, sy, x3, sy, 2, c); line(x3, sy, x4, bot, 2, c);
}
static void snd_picture(const SndPage *pg, int x, int y, int w, int h, Col ec) {
    const Track *t = &song.t[sel]; const Sample *s = engine_sample(sel);
    switch (pg->gfx) {
    case GF_WAVE: case GF_SLICES:
        snd_wave(x + 12, y + 8, w - 24, h - 36, ec, mix(ec, C_CELL, 170), pg->gfx == GF_SLICES);
        if (s) ftext(x + 20, y + h - 28, FT_S, C_DIM, s->name);
        break;
    case GF_CLOUD:
        snd_wave(x + 12, y + 8, w - 24, h - 36, mix(ec, C_CELL, 150), mix(ec, C_CELL, 200), 0);
        if (s && s->n > 1) { int ww = w - 24 > 960 ? 960 : w - 24, px = x + 12 + ww * t->cp[CP_POS] / 100, sw = ww * t->cp[CP_SPRAY] / 200;   // where the grains come from
            int x0 = px - sw < x + 12 ? x + 12 : px - sw, x1 = px + sw > x + 12 + ww ? x + 12 + ww : px + sw;
            float gp[CL_GRAINS], genv = 0, scan = 0; int ng = engine_cloud_grains(sel, gp, CL_GRAINS, &genv, &scan);
            int rad = 3 + t->cp[CP_SIZE] / 14, hh = h - 52;
            if (ng > 0) {                 // sounding: every grain where it reads now, MOTION carrying the band along
                float c0 = t->cp[CP_POS] / 100.f + scan; c0 -= floorf(c0); px = x + 12 + (int)(c0 * ww);
                for (int i = 0; i < ng; i++) { float p = c0 + gp[i]; p -= floorf(p);
                    disc2(x + 12 + (int)(p * ww), y + 16 + (i * 37 % 100) * hh / 100, rad, mix(ec, C_WHITE, (int)(60 + genv * 160))); }
                rect(px, y + 8, 2, h - 36, C_WHITE, 1); if (s) ftext(x + 20, y + h - 28, FT_S, C_DIM, s->name); break; }
            unsigned r = (unsigned)(SDL_GetTicks() / 140) * 2654435761u;   // silent, A16: the grains as dots round the position, a new cloud every 140 ms
            int n = 10 + t->cp[CP_DENSITY] * 30 / 100;
            for (int i = 0; i < n; i++) { r = r * 1103515245u + 12345u; int gx = x0 + (int)((r >> 8) % (unsigned)(x1 - x0 + 1)); r = r * 1103515245u + 12345u; int gy = y + 16 + (int)((r >> 8) % (unsigned)hh);
                disc2(gx, gy, rad, mix(ec, C_WHITE, (int)((r >> 4) % 90))); }
            rect(px, y + 8, 2, h - 36, C_WHITE, 1); }
        if (s) ftext(x + 20, y + h - 28, FT_S, C_DIM, s->name);
        break;
    case GF_OSC:
        if (t->type == 2) { rect(x + 12, y + 10, w - 24, h - 20, C_BG, 1); snd_shape(x + 24, y + 22, w - 48, h - 44, t->ap[AP_WAVE] ? 2 : 1, ec); ftext(x + 22, y + 14, FT_S, C_DIM, "OSC"); }
        break;
    case GF_FILTER:
        snd_filter(x + 12, y + 8, w - 24, h - 16, t->ap[AP_CUTOFF], t->ap[AP_RESO], ec);
        break;
    case GF_ALL: {                        // v3 MAIN: the wave, the filter and the envelope side by side
        int pw = (w - 48) / 3;
        for (int i = 0; i < 3; i++) { int px = x + 12 + i * (pw + 12); rect(px, y + 10, pw, h - 20, C_BG, 1);
            ftext(px + 10, y + 14, FT_S, C_DIM, i == 0 ? "OSC" : i == 1 ? "FILTER" : "ENV"); }
        snd_shape(x + 24, y + 40, pw - 24, h - 66, t->ap[AP_WAVE] ? 2 : 1, ec);
        snd_filter(x + 24 + pw, y + 34, pw - 24, h - 50, t->ap[AP_CUTOFF], t->ap[AP_RESO], ec);
        snd_env(x + 40 + 2 * pw, y + 34, pw - 32, h - 50, 0, t->ap[AP_DECAY], 0, t->ap[AP_DECAY] / 2, ec);
        break; }
    default: wv_picture(pg->gfx, x, y, w, h, ec); break;   // WAVE's pages
    }
}
static void snd_live(const SndPage *pg, int x, int y, int w, int h) {   // what sounds now, over the picture (1.2 had it on its panels)
    const Track *t = &song.t[sel]; float env, fc;
    if ((pg->gfx == GF_WAVE || pg->gfx == GF_SLICES) && t->type == 0 && engine_sample(sel)) {   // where each voice reads the sample
        float pos[8]; int k = engine_sample_live(sel, pos, 8), ww = w - 24 > 960 ? 960 : w - 24;
        for (int i = 0; i < k; i++) rect(x + 12 + (int)(pos[i] * ww), y + 8, 2, h - 36, C_ORANGE, 1);
    } else if ((pg->gfx == GF_FILTER || pg->gfx == GF_ENV) && t->type == 2 && engine_synth_live(sel, &env, &fc)) {
        if (pg->gfx == GF_FILTER) {       // the cutoff now, on the same scale as the picture's (20 Hz .. 20 kHz over 8..88 %)
            float f = log10f(fc > 20 ? fc / 20.f : 1.f) / 3; if (f > 1) f = 1;
            rect(x + 12 + (w - 24) * (8 + (int)(f * 80)) / 100, y + 14, 3, h - 28, C_ORANGE, 1);
        } else { int top = y + 18, bot = y + h - 18, ly = bot - (int)(env * (bot - top));   // the envelope's level now
            rect(x + 16, ly, w - 32, 1, mix(C_ORANGE, C_CELL, 120), 1); disc2(x + w - 30, ly, 7, C_ORANGE); }
    }
}
static int snd_tags(int p, char tg[][4]) {   // the automation lanes (A1..) and LFOs (L1..) moving this parameter now
    int b = auto_bind_target(p), n = 0; if (b < 0) return 0;
    const AutoLane *al = song.aut[sel][song.t[sel].cur];
    for (int a = 0; a < NAUTO && n < 4; a++) if (al[a].target == b) snprintf(tg[n++], 4, "A%d", a + 1);
    for (int j = 0; j < NLFO && n < 4; j++) if (song.lfo[sel][j].target == b && song.lfo[sel][j].depth) snprintf(tg[n++], 4, "L%d", j + 1);
    return n;
}
static const char *snd_label(int p) {
    switch (p) { case P_VOICE: return "MODE"; case P_DIR: return "DIRECTION"; case P_TYPE: return "ENGINE"; case P_VOL: return "VOLUME";
                 case P_SAMPLE: return "PRESETS"; case P_BPM: return "LOOP BPM"; case P_SLWAY: return "METHOD"; }
    if (p == P_WENV) return "ENVELOPE"; if (p == P_WLFO) return "LFO"; if (p == P_WTGT) return "TARGET";
    if (p >= P_WV && p < P_WV + WP_SUB_LEVEL) { const char *pn = snd_page()->name;   // OSC A / B's cards: the page says which, elsewhere the name does
        if (strcmp(pn, "OSC A") && strcmp(pn, "OSC B")) return wp_name(p - P_WV); }
    if (p >= P_WV + WP_ENV2 && p < P_WV + WP_ENV3 && snd_page()->gfx == GF_WFILT) return wp_name(p - P_WV);   // FILTER: "ENV2 ATT"
    return par_name[p];
}
// v3 (2026-11-20, user: "round encoders like Serum"): a knob per parameter. The arc shows the value (from the middle for
// what goes both ways), the outer ring what moves it: an LFO as the span it swings over, automation as a dot.
static void knob_arc(int cx, int cy, int r, int t, float a0, float a1, Col c) {   // angles in degrees, 0 = right, clockwise
    if (a1 < a0) { float s = a0; a0 = a1; a1 = s; }
    for (float a = a0; a <= a1 + 0.01f; a += 3) disc2(cx + (int)(r * cosf(a * 0.0174533f)), cy + (int)(r * sinf(a * 0.0174533f)), t / 2, c);
}
// The knob alone, for any page: f = where the value sits 0..1, z = where its arc starts (0, or the middle for +/- values).
// Returns the angle of the value, for rings drawn round it.
// 2026-11-25 (user, Serum): a modulated knob. The needle shows the value NOW (its LFOs and automation move it on screen);
// a notch on the rim keeps the set value; each LFO draws its range as an arc outside the knob in the MOD colour: from the
// set value one way (UNIPOLAR, the depth's sign gives the way) or both ways (BIPOLAR); automation puts its dot on the rim.
static float mod_live(int pad, int tg, float f) {   // f: the set value 0..1; the value the engine plays now
    if (tg < 0) return f;
    if (song.t[pad].on || engine_playing()) { const AutoLane *al = song.aut[pad][song.t[pad].cur];
        for (int k = 0; k < NAUTO; k++) if (al[k].target == tg) { float v = engine_auto(pad, k); if (v >= 0) f = v / 100.f; break; } }
    for (int j = 0; j < NLFO; j++) { const Lfo *L = &song.lfo[pad][j]; if (L->target == tg && L->depth) f += engine_lfo(pad, j) * L->depth / 100.f; }
    return f < 0 ? 0 : f > 1 ? 1 : f;
}
static void mod_rings(int pad, int tg, float f, int cx, int cy, int r) {
    if (tg < 0) return; float a = 135 + 270 * f; int any = 0; Col mc = C_CAT(PC_MOD);
    for (int j = 0, ring = 0; j < NLFO; j++) { const Lfo *L = &song.lfo[pad][j]; if (L->target != tg || !L->depth) continue;
        float sp = 270.f * abs(L->depth) / 100, a0, a1, rr = r + 8 + ring++ * 5; any = 1;
        if (L->spare & 1) { if (L->depth > 0) { a0 = a; a1 = a + sp; } else { a0 = a - sp; a1 = a; } } else { a0 = a - sp; a1 = a + sp; }
        if (a0 < 135) a0 = 135; if (a1 > 405) a1 = 405; if (a1 > a0) knob_arc(cx, cy, (int)rr, 4, a0, a1, mc); }
    const AutoLane *al = song.aut[pad][song.t[pad].cur];
    for (int k = 0; k < NAUTO; k++) if (al[k].target == tg) { any = 1; float v = engine_auto(pad, k); float av = v >= 0 ? 135 + 270 * v / 100 : a;
        disc2(cx + (int)((r + 9) * cosf(av * 0.0174533f)), cy + (int)((r + 9) * sinf(av * 0.0174533f)), 5, C_ORANGE); break; }
    if (any) { float c = cosf(a * 0.0174533f), sn = sinf(a * 0.0174533f);   // the set value: a notch on the rim
        line(cx + (int)((r - 2) * c), cy + (int)((r - 2) * sn), cx + (int)((r + 6) * c), cy + (int)((r + 6) * sn), 3, C_WHITE); }
}
static float knob_draw(int x, int y, int w, int h, const char *lb, char *v, float f, float z, int on, Col ec, int *kcx, int *kcy, int *kr) {
    if (on) rframe(x, y, w, h, 8, 3, ec, C_CELL); else rrect(x, y, w, h, 8, C_CELL);
    char l2[16]; snprintf(l2, sizeof l2, "%.13s", lb);
    ftext(x + (w - ftext_w(FT_S, l2)) / 2, y + 8, FT_S, on ? C_WHITE : C_DIM, l2);
    int cx = x + w / 2, cy = y + 30 + (h - 70) / 2, r = (h - 70) / 2 - 8; if (r > 72) r = 72; if (r > w / 2 - 24) r = w / 2 - 24;   // the knob grows with its card; room for its mod ring
    float a = 135 + 270 * f, a0 = 135 + 270 * z;
    knob_arc(cx, cy, r, 6, 135, 405, C_LINE);
    if (fabsf(a - a0) > 1) knob_arc(cx, cy, r, 6, a0, a, ec);
    disc2(cx, cy, r - 12 > 4 ? r - 12 : 4, mix(C_CELL, C_BG, 128));
    line(cx + (int)((r - 26) * cosf(a * 0.0174533f) * 0.2f), cy + (int)((r - 26) * sinf(a * 0.0174533f) * 0.2f),
         cx + (int)((r - 14) * cosf(a * 0.0174533f)), cy + (int)((r - 14) * sinf(a * 0.0174533f)), 4, C_WHITE);
    units(v); int fw = ftext_w(FT_MB, v) <= w - 12 ? FT_MB : FT_S; fit(v, fw, w - 12);
    ftext(cx - ftext_w(fw, v) / 2, y + h - 30, fw, C_WHITE, v);
    *kcx = cx; *kcy = cy; *kr = r; return a;
}
static void val_draw(int x, int y, int w, int h, const char *lb, const char *v, int on, Col ec) {   // a value with many names (TARGET, SHAPE): big, no needle
    if (on) rframe(x, y, w, h, 8, 3, ec, C_CELL); else rrect(x, y, w, h, 8, C_CELL);
    char l2[16]; snprintf(l2, sizeof l2, "%.13s", lb); ftext(x + (w - ftext_w(FT_S, l2)) / 2, y + 8, FT_S, on ? C_WHITE : C_DIM, l2);
    char b[32]; snprintf(b, sizeof b, "%s", v); int f = ftext_w(FT_LB, b) <= w - 16 ? FT_LB : FT_MB; fit(b, f, w - 16);
    rrect(x + 12, y + h / 2 - 26, w - 24, 52, 10, C_SELBG); ftext(x + w / 2 - ftext_w(f, b) / 2, y + h / 2 - fv2[f].h / 2, f, C_WHITE, b);
}
// 2026-11-24 (user): a value with two states is a switch, not a knob. Named pair (a, b): two halves, the one in use filled;
// a, b NULL: one lamp button ON / OFF (as MIX's M and S). The same card as a knob, the value on the same line.
static void toggle_draw(int x, int y, int w, int h, const char *lb, const char *a, const char *b, int st, int on, Col ec) {
    if (on) rframe(x, y, w, h, 8, 3, ec, C_CELL); else rrect(x, y, w, h, 8, C_CELL);
    char l2[16]; snprintf(l2, sizeof l2, "%.13s", lb); ftext(x + (w - ftext_w(FT_S, l2)) / 2, y + 8, FT_S, on ? C_WHITE : C_DIM, l2);
    int bw = w - 28 > 200 ? 200 : w - 28, bh = clampi((h - 40) * 45 / 100, 44, 96), bx = x + (w - bw) / 2, by = y + 30 + (h - 36 - bh) / 2;
    const char *v;
    if (a) { int hw = bw / 2;
        for (int i = 0; i < 2; i++) { const char *t = i ? b : a; int lit = st == i, f = ftext_w(FT_MB, t) <= hw - 10 ? FT_MB : FT_S;
            rrect(bx + i * hw + i, by, hw - 1, bh, 8, lit ? ec : C_SELBG);
            if (ftext_w(f, t) <= hw - 4) ftext(bx + i * hw + hw / 2 - ftext_w(f, t) / 2, by + bh / 2 - fv2[f].h / 2, f, lit ? C_BG : mix(C_WHITE, C_GREY, 120), t); }
        v = st ? b : a;
    } else {
        rrect(bx, by, bw, bh, 12, st ? ec : C_SELBG); disc2(bx + bw - 18, by + 18, 6, st ? C_WHITE : mix(ec, C_BG, 150));
        v = st ? "ON" : "OFF"; ftext(bx + bw / 2 - ftext_w(FT_LB, v) / 2, by + bh / 2 - fv2[FT_LB].h / 2, FT_LB, st ? C_BG : C_DIM, v);
    }
    (void)v;                              // the switch shows its state: no value line under it (2026-11-24, audit)
}
static int snd_pair(int p, const char **a, const char **b) {   // a two-state SOUND parameter: 1, and its names (NULL: OFF / ON)
    *a = *b = NULL;
    if (p == P_DIR) { *a = "FWD"; *b = "REV"; return 1; }
    if (p == P_VOICE) { *a = song.t[sel].type == 0 ? "GATE" : "MONO"; *b = "POLY"; return 1; }
    if (p == P_AC + AP_WAVE) { *a = "SAW"; *b = "SQUARE"; return 1; }
    if (p == P_AX + AX_SUBOCT) { *a = "-1 OCT"; *b = "-2 OCT"; return 1; }
    if (p == P_WV + WP_NOISE_TYPE) { *a = "WHITE"; *b = "PINK"; return 1; }
    if (p == P_WV + WP_QUALITY) { *a = "ECO"; *b = "HIGH"; return 1; }
    if (p == P_WV + WP_SUB_OCT) { *a = "-1 OCT"; *b = "-2 OCT"; return 1; }
    return wv_is_pair(p) || p == P_CX + CX_FREEZE;
}
// 2026-11-24 (audit): a choice among 3..6 named values is a row of segments, the chosen one filled, its name on the value line
static void seg_draw(int x, int y, int w, int h, const char *lb, const char *v, const char *const *nm, int n, int cur, int on, Col ec) {
    if (on) rframe(x, y, w, h, 8, 3, ec, C_CELL); else rrect(x, y, w, h, 8, C_CELL);
    char l2[16]; snprintf(l2, sizeof l2, "%.13s", lb); ftext(x + (w - ftext_w(FT_S, l2)) / 2, y + 8, FT_S, on ? C_WHITE : C_DIM, l2);
    int bx = x + 10, bw = w - 20, sw = bw / n, row = 1;   // the names: in a row when each fits its segment, else one under another
    for (int i = 0; i < n; i++) if (ftext_w(FT_S, nm[i]) > sw - 6) row = 0;
    if (row) { int bh = clampi((h - 40) / 2, 28, 56), by = y + 34 + (h - 40 - bh) / 2;
        for (int i = 0; i < n; i++) { rrect(bx + i * sw + 1, by, sw - 3, bh, 6, i == cur ? ec : C_SELBG);
            ftext(bx + i * sw + sw / 2 - ftext_w(FT_S, nm[i]) / 2, by + bh / 2 - fv2[FT_S].h / 2, FT_S, i == cur ? C_BG : mix(C_WHITE, C_GREY, 120), nm[i]); }
        return; }
    int rh = (h - 40) / n; if (rh > 30) rh = 30; int y0 = y + 32 + (h - 36 - rh * n) / 2;
    for (int i = 0; i < n; i++) { int ry = y0 + i * rh; char t[24]; snprintf(t, sizeof t, "%s", nm[i]); fit(t, FT_S, bw - 12);
        if (i == cur) rrect(bx, ry + 1, bw, rh - 2, 6, ec);
        ftext(x + w / 2 - ftext_w(FT_S, t) / 2, ry + rh / 2 - fv2[FT_S].h / 2, FT_S, i == cur ? C_BG : mix(C_WHITE, C_GREY, 120), t); }
    (void)v;
}
static const char *const *snd_names(int p) {   // the choices of a list parameter, as short as they read
    static const char *const nrm[] = {"OFF", "PEAK", "LOUD"}, *const wrp[] = {"OFF", "BEATS", "REPITCH", "TONES", "TEXTURE", "COMPLEX"},
        *const slc[] = {"OFF", "4", "8", "16", "32"}, *const way[] = {"EQUAL", "TRANS", "BY HAND"}, *const brs[] = {"AUTO", "1", "2", "4", "8", "16"},
        *const am[] = {"UP", "DOWN", "ORDER", "UP/DN"}, *const ao[] = {"1", "2", "3"}, *const dt[] = {"DIODE", "TRANS", "TUBE", "FUZZ"},
        *const sub[] = {"SINE", "TRI", "SQR", "SAW"}, *const trg[] = {"FREE", "RETRIG", "ONE"}, *const en[] = {"ENV1", "ENV2", "ENV3"}, *const lf[] = {"LFO1", "LFO2", "LFO3", "LFO4"},
        *const sw[] = {"SQR", "SINE", "TRI"}, *const wn[] = {"HANN", "TRI", "TUKEY", "PERC", "REV P"}, *const im[] = {"OCT+5", "OCT", "SCALE"},
        *const sy[] = {"FREE", "1/4", "1/8", "1/16", "1/32"}, *const sc[] = {"FWD", "REV", "PING"};
    switch (p) { case P_NORM: return nrm; case P_WARP: return wrp; case P_SLICES: return slc; case P_SLWAY: return way; case P_BARS: return brs; }
    if (p == P_WV + WP_ARP_MODE) return am; if (p == P_WV + WP_ARP_OCT) return ao; if (p == P_AC + AP_DTYPE) return dt;
    if (p == P_WV + WP_SUB_SHAPE) return sub; if (p == P_WENV) return en; if (p == P_WLFO) return lf;
    if (p >= P_WV + WP_LFO1 && p < P_WV + WP_POLY && (p - P_WV - WP_LFO1) % WL_N == WL_TRIG) return trg;
    if (p == P_AX + AX_SUBWAVE) return sw; if (p == P_CX + CX_WINDOW) return wn; if (p == P_CX + CX_SHMODE) return im; if (p == P_CX + CX_SYNC) return sy;
    if (p == P_CX + CX_SCAN) return sc;
    return NULL;
}
static int snd_list(int p, int *n, int *cur) {   // a parameter whose values are names: how many and which (BARS: AUTO 1 2 4 8 16)
    int lo, hi, st, bg, v; par_limits(sel, p, &lo, &hi, &st, &bg); v = par_get(sel, p);
    if (p == P_BARS) { static const int bv[6] = {0, 1, 2, 4, 8, 16}; *n = 6; *cur = 0; for (int i = 0; i < 6; i++) if (bv[i] == v) *cur = i; return 1; }
    static const int ls[] = {P_NORM, P_WARP, P_SLICES, P_SLWAY, P_WV + WP_ARP_MODE, P_WV + WP_ARP_OCT, P_WV + WP_SUB_SHAPE, P_WENV, P_WLFO,
        P_AC + AP_DTYPE, P_AX + AX_SUBWAVE, P_CX + CX_WINDOW, P_CX + CX_SHMODE, P_CX + CX_SYNC, P_CX + CX_SCAN};
    for (unsigned i = 0; i < sizeof ls / sizeof *ls; i++) if (ls[i] == p && hi - lo >= 1 && hi - lo <= 5) { *n = hi - lo + 1; *cur = clampi(v - lo, 0, hi - lo); return 1; }
    if (p >= P_WV + WP_LFO1 && p < P_WV + WP_POLY && (p - P_WV - WP_LFO1) % WL_N == WL_TRIG) { *n = 3; *cur = clampi(v, 0, 2); return 1; }
    return 0;
}
static void snd_knob(int x, int y, int w, int h, int p, int on, Col ec) {
    { const char *a, *b; if (snd_pair(p, &a, &b)) { int lo, hi, st, bg; par_limits(sel, p, &lo, &hi, &st, &bg); int v = par_get(sel, p) - lo;
        toggle_draw(x, y, w, h, snd_label(p), a, b, v != 0, on, ec); if (on) ui_focus(snd_label(p), v ? (b ? b : "ON") : (a ? a : "OFF"), -1, ec); return; } }
    { int n, cur; if (snd_list(p, &n, &cur) && snd_names(p)) { char v[48]; par_str(sel, p, v, sizeof v); seg_draw(x, y, w, h, snd_label(p), v, snd_names(p), n, cur, on, ec); if (on) ui_focus(snd_label(p), v, -1, ec); return; } }
    if (wv_is_list(p) || p == P_WTGT) { char v[48]; par_str(sel, p, v, sizeof v); val_draw(x, y, w, h, snd_label(p), v, on, ec); if (on) ui_focus(snd_label(p), v, -1, ec); return; }   // WAVE: TABLE, WARP, FILTER TYPE, SHAPE, SYNC... as names
    char v[96]; if (p == P_SAMPLE) snprintf(v, sizeof v, "OPEN"); else if (p == P_DIR) snprintf(v, sizeof v, "%s", song.t[sel].rev ? "\005" : "\004");
    else par_str(sel, p, v, sizeof v);
    const char *lb = snd_label(p);
    int lo = 0, hi = 1, st, bg; float f = 0; if (p != P_SAMPLE) { par_limits(sel, p, &lo, &hi, &st, &bg); if (hi > lo) f = (float)clampi(par_get(sel, p) - lo, 0, hi - lo) / (hi - lo); }
    if (p == P_SAMPLE) {                  // a button, not a knob
        if (on) rframe(x, y, w, h, 8, 3, ec, C_CELL); else rrect(x, y, w, h, 8, C_CELL);
        ftext(x + (w - ftext_w(FT_S, lb)) / 2, y + 8, FT_S, on ? C_WHITE : C_DIM, lb);
        rrect(x + 20, y + h / 2 - 18, w - 40, 36, 8, C_SELBG); ftext(x + w / 2 - ftext_w(FT_MB, v) / 2, y + h / 2 - fv2[FT_MB].h / 2, FT_MB, C_WHITE, v);
    } else {
        int b = auto_bind_target(p); float fl = mod_live(sel, b, f);   // the knob turns with its LFOs and automation, as on Serum
        int cx, cy, r; knob_draw(x, y, w, h, lb, v, fl, lo < 0 && hi > 0 ? (float)-lo / (hi - lo) : 0, on, ec, &cx, &cy, &r);
        mod_rings(sel, b, f, cx, cy, r);
    }
    char tg[4][4]; int nt = snd_tags(p, tg), tx = x + w - 6 - nt * 30;   // A1 / L1 badges in the top corner, off the ring (2026-11-24)
    for (int i = 0; i < nt; i++) { Col bc = tg[i][0] == 'A' ? C_CAT(PC_MOD) : C_WHITE; rrect(tx + i * 30, y + 6, 26, 18, 4, bc); ftext(tx + i * 30 + 13 - ftext_w(FT_S, tg[i]) / 2, y + 6, FT_S, C_BG, tg[i]); }
    if (on) ui_focus(lb, v, par_is_number(p) && hi > lo ? (int)(f * 100) : -1, ec);
}
#include "wvui.h"                        // WAVE's pictures, the MATRIX list and the TABLE browser
static int pic_h(const SndPage *pg) { return pg->gfx != GF_NONE; }
// 2026-11-24 (user: no empty half screens): the pages without a picture of their own get one drawn from their values (the
// cursor never stops on it: EDIT has nothing to open there). LFO its wave, ARP its steps, DIST its curve, VOICE its envelope,
// OUT the level.
static void snd_deco(const SndPage *pg, int x, int y, int w, int h, Col ec) {
    const Track *t = &song.t[sel]; const int8_t *wa = song.wv[sel], *ax = song.ax[sel]; int ix = x + 24, iw = w - 48, iy = y + 20, ih = h - 40, px = 0, py = 0;
    rrect(x, y, w, h, 8, C_CELL);
    if (!strcmp(pg->name, "ARP")) {        // the steps it plays: UP DOWN ORDER UP/DOWN over 1..3 octaves, the gate as the bar's width
        int oct = wa[WP_ARP_OCT] % 3 + 1, n = 4 * oct, mode = wa[WP_ARP_MODE] % 4, steps = mode == 3 ? 2 * n - 2 : n, gate = clampi(wa[WP_ARP_GATE], 5, 100), on = wa[WP_ARP];
        if (steps > 24) steps = 24; int sw = iw / (steps ? steps : 1);
        for (int i = 0; i < steps; i++) { int k = mode == 1 ? n - 1 - i : mode == 3 ? (i < n ? i : 2 * n - 2 - i) : mode == 2 ? (i * 5 + 1) % n : i;
            int bh = (k + 1) * (ih - 10) / n; rrect(ix + i * sw, iy + ih - bh, sw * gate / 100 > 4 ? sw * gate / 100 - 2 : 4, bh, 4, on ? ec : mix(ec, C_CELL, 170)); }
        if (!on) ftext(x + w / 2 - ftext_w(FT_MB, "ARP OFF") / 2, y + 12, FT_MB, C_DIM, "ARP OFF");
    } else if (!strcmp(pg->name, "DIST")) { // the transfer curve with DRIVE, the dry line under it
        line(ix, iy + ih, ix + iw, iy, 1, C_LINE); float dg = 1 + t->ap[AP_DRIVE] / 100.f * 30.f;
        for (int i = 0; i <= iw; i += 3) { float xv = i * 2.f / iw - 1, v = acid_shape(t->ap[AP_DTYPE], xv * dg) * 0.9f; v = v > 1 ? 1 : v < -1 ? -1 : v;
            int qx = ix + i, qy = iy + ih / 2 - (int)(v * (ih / 2 - 4)); if (i) line(px, py, qx, qy, 3, ec); px = qx; py = qy; }
    } else if (!strcmp(pg->name, "VOICE")) { // the amplitude: its attack and release
        snd_env(ix, iy, iw, ih, clampi(ax[AX_VATK], 0, 100), 0, 100, clampi(ax[AX_VREL], 0, 100), ec);
    } else {                               // OUT: the level, big
        int v = clampi(t->vol, 0, 100); char b[16]; snprintf(b, sizeof b, "%d", v);
        rrect(ix, iy + ih / 2 - 14, iw, 28, 8, C_BG); rrect(ix, iy + ih / 2 - 14, iw * v / 100, 28, 8, ec);
        ftext(ix, iy, FT_S, C_DIM, "LEVEL"); ftext(ix + iw - ftext_w(FT_XB, b), iy + ih / 2 - 60, FT_XB, C_WHITE, b);
        mx_meters(); float m = mx_show[sel]; int mw = (int)(iw * fminf(1.f, m));   // what the pad plays now (2026-11-24: the picture was the VOLUME card again)
        ftext(ix, iy + ih / 2 + 30, FT_S, C_DIM, "NOW"); rrect(ix, iy + ih / 2 + 56, iw, 12, 5, C_BG); if (mw > 0) rrect(ix, iy + ih / 2 + 56, mw, 12, 5, m > 0.98f ? C_RED : C_PLAY);
    }
}
static void draw_sound2(void) {
    int n; const SndPage *pgs = snd_pages(song.t[sel].type, &n), *pg = snd_page(); int cur = (int)(pg - pgs);
    Col ec = C_ENG(song.t[sel].type); int l[20], c = snd_cards(pg, l);
    for (int i = 0; i < n; i++) {        // the pages as tabs
        int tw = ww / n, cx = wx0 + tw * i + tw / 2;
        if (i == cur) rrect(wx0 + tw * i, 73, tw - 10 > 184 ? 184 : tw - 10, 38, 8, ec);
        int px = i == cur ? wx0 + tw * i + (tw - 10 > 184 ? 184 : tw - 10) / 2 : cx - 5;
        int tf = ftext_w(FT_MB, pgs[i].name) <= tw - 12 ? FT_MB : FT_S;   // WAVE has ten pages: a smaller name where it must
        ftext(px - ftext_w(tf, pgs[i].name) / 2, 80 + (tf == FT_S ? 4 : 0), tf, i == cur ? C_BG : C_DIM, pgs[i].name);
    }
    if (pg->gfx == GF_WMTX) { draw_matrix2(ec); return; }
    int y0 = 127, ph = (c + kg_cols(c) - 1) / kg_cols(c) <= 1 ? 300 : 210;   // one row of knobs: a taller picture
    if (pg->gfx != GF_NONE) {            // the picture; the cursor on it: the file (the sampler, CLOUD) or the presets
        if (sp_cur < 0) rframe(wx0, 127, ww, ph, 8, 4, ec, C_CELL); else rrect(wx0, 127, ww, ph, 8, C_CELL);
        snd_picture(pg, wx0, 127, ww, ph, ec); snd_live(pg, wx0, 127, ww, ph);
        y0 = 127 + ph + 15;
    } else { snd_deco(pg, wx0, 127, ww, ph, ec); y0 = 127 + ph + 15; }   // no picture of its own: one from its values
                                          // every knob of the page at once, in the grid that fits their count (ui.h kg_cell)
    for (int i = 0; i < c; i++) { int x, y, w, h; kg_cell(c, i, wx0, ww, y0, 700, &x, &y, &w, &h); snd_knob(x, y, w, h, l[i], i == sp_cur, ec); }
    if (learning) draw_footer("EDIT TAKE THIS PARAMETER  Y CANCEL");   // 2026-11-24: only keys that still work (L1/R1 page the sections)
    else if (sp_cur < 0) draw_footer(pg->gfx == GF_SLICES ? "EDIT SLICE EDITOR  EDIT+ARROWS SLICES  ^ TABS"
                              : (pg->gfx == GF_WTA || pg->gfx == GF_WTB) ? "EDIT TABLES  EDIT+<> NEXT TABLE  ^ TABS  OPT LISTEN"
                              : is_synth(sel) && song.t[sel].type != 3 ? "EDIT PRESETS  ^ TABS  OPT LISTEN"
                              : "EDIT FILE  EDIT+<> NEXT FILE  ^ TABS");
    else { int p = sound_par();          // the cards with a key of their own say so
        draw_footer(p == P_BPM ? "EDIT TAP IN TIME  EDIT+ARROWS VALUE  ^ TABS"
                  : p == P_ROOT ? "EDIT SET AS PROJECT KEY  EDIT+ARROWS VALUE  ^ TABS"
                  : p == P_WTGT ? "EDIT POINT AT A CARD: THIS LFO MOVES IT  ^ TABS"
                  : "EDIT+ARROWS VALUE  ^ TABS  OPT+EDIT DEFAULT"); }
}
static const char *snd_page_name(void) { return snd_page()->name; }
static int snd_page_of(int gfx) { int n; const SndPage *pg = snd_pages(song.t[sel].type, &n); for (int i = 0; i < n; i++) if (pg[i].gfx == gfx) return i; return 0; }
static void snd_page_count(char *b, int n) { int k; const SndPage *pgs = snd_pages(song.t[sel].type, &k); snprintf(b, n, "%d/%d", (int)(snd_page() - pgs) + 1, k); }

// ---- SOUND > FILES (A11): the folders on the left, the files on the right ----
// The 1.2 browser's keys stay: UP/DOWN the list (folders first, then files), RIGHT/EDIT into a folder, LEFT up, hold EDIT a kit.
// A folder under the cursor shows what is in it on the right; in a folder without folders the left side shows its neighbours.
static const char *const fcat_name[CAT_N] = {"", "Kick", "Snare", "Clap", "Hat", "Open hat", "Cymbal", "Tom", "Perc", "Loop", "Bass", "Melodic", "Vox", "FX"};
static int fl_list(const char *dir, Entry **out) {   // a folder's entries, kept until another folder is asked for (two at once)
    static Entry *e[2]; static int n[2], next; static char key[2][512];
    for (int i = 0; i < 2; i++) if (e[i] && !strcmp(key[i], dir)) { *out = e[i]; return n[i]; }
    int i = next; next ^= 1;
    if (!e[i]) e[i] = malloc(sizeof(Entry) * 512);
    if (!e[i]) { *out = NULL; return 0; }
    snprintf(key[i], sizeof key[i], "%s", dir); n[i] = list_dir(dir, e[i], 512); *out = e[i]; return n[i];
}
static void fl_name(char *b, int n, const char *s, int max) {   // too long: the start and the end, the middle as ..
    int l = (int)strlen(s); if (l <= max) snprintf(b, n, "%s", s); else snprintf(b, n, "%.*s..%s", max - 8, s, s + l - 6);
}
static void draw_browse2(void) {
    enum { LX = 26, LY = 76, LW = 293, LH = 625, RX = 338, RW = 659, RH = 61, ROWS = 10 };
    Col ec = C_ENG(song.t[sel].type); char b[64];
    int nd = 0; while (nd < bn && bent[nd].dir) nd++;
    rrect(LX, LY, LW, LH, 10, C_CELL);
    // the left side: this folder's folders (the cursor may be on one), else its neighbours with this one marked
    const Entry *le = bent; int ln = nd, lon = bcur < nd ? bcur : -1, act = bcur < nd; Entry *pe = NULL;
    if (!nd) { const char *sl = strrchr(bdir, '/');
        if (sl) { char up[512]; snprintf(up, sizeof up, "%.*s", (int)(sl - bdir), bdir); int pn = fl_list(up, &pe); ln = 0; le = pe;
            while (ln < pn && pe[ln].dir) { if (!strcmp(pe[ln].name, sl + 1)) lon = ln; ln++; } } }
    { static int ltop; if (lon >= 0) { if (lon < ltop) ltop = lon; if (lon >= ltop + 10) ltop = lon - 9; } if (ltop > (ln > 10 ? ln - 10 : 0)) ltop = ln > 10 ? ln - 10 : 0;
      for (int i = 0; i < 10 && ltop + i < ln; i++) { int k = ltop + i, y = LY + 12 + i * 57, on = k == lon;
        if (on) rrect(LX + 10, y, LW - 20, 48, 8, act ? C_SELBG : mix(C_SELBG, C_CELL, 128));
        fl_name(b, sizeof b, le[k].name, 13); for (char *q = b; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
        ftext(LX + 28, y + 12, on ? FT_MB : FT_M, on ? C_WHITE : C_DIM, b); }
      if (!ln) ftext(LX + 28, LY + 24, FT_MB, C_WHITE, "SAMPLES"); }
    // the right side: the files here, or what is in the folder under the cursor
    const Entry *re = bent + nd; int rn = bn - nd, rcur = bcur - nd; char rdir[512]; snprintf(rdir, sizeof rdir, "%s", bdir);
    if (act) { char p[1024]; snprintf(p, sizeof p, "%s/%s", bdir, bent[bcur].name); snprintf(rdir, sizeof rdir, "%.511s", p); Entry *fe; rn = fl_list(rdir, &fe); re = fe; rcur = -1; }
    static int rtop; if (rcur >= 0) { if (rcur < rtop) rtop = rcur; if (rcur >= rtop + ROWS) rtop = rcur - ROWS + 1; } else if (act) rtop = 0;
    if (rtop > (rn > ROWS ? rn - ROWS : 0)) rtop = rn > ROWS ? rn - ROWS : 0;
    for (int i = 0; i < ROWS && rtop + i < rn; i++) { int k = rtop + i, y = LY + i * RH, on = k == rcur;
        char fp[1024]; snprintf(fp, sizeof fp, "%s/%s", rdir, re[k].name);
        int bad = !re[k].dir && idx_bad(fp), fill = on && browse_hold && ms_since(SDL_GetTicks(), browse_hold) > 150;
        if (on) rrect(RX, y, RW, 50, 8, C_SELBG);
        fl_name(b, sizeof b, re[k].name, 40); if (re[k].dir) strcat(b, "/");
        ftext(RX + 23, y + 13, on ? FT_MB : FT_M, bad ? C_RED : act ? C_DIM : C_WHITE, b);
        const char *tg = ""; char t2[24];
        if (re[k].dir) tg = "folder";
        else if (bad) tg = "broken";
        else { int c = file_cat(fp); float bpm; int root; sample_name_info(fp, &bpm, &root);
            if (c == CAT_LOOP && bpm > 0) { snprintf(t2, sizeof t2, "%d BPM", (int)(bpm + 0.5f)); tg = t2; } else tg = fcat_name[c % CAT_N]; }
        ftext(RX + RW - 23 - ftext_w(FT_M, tg), y + 13, FT_M, on ? ec : C_DIM, tg);
        if (fill) rect(RX + 12, y + 44, clampi(ms_since(SDL_GetTicks(), browse_hold), 0, HOLD_MS) * (RW - 24) / HOLD_MS, 4, browse_rand ? C_WHITE : ec, 1);
    }
    if (rn > ROWS) { int th = ROWS * RH - 11, tw = th * ROWS / rn; rect(RX + RW + 4, LY, 4, th, C_CELL, 1); rect(RX + RW + 4, LY + (th - tw) * rtop / (rn - ROWS), 4, tw, C_DIM, 1); }
    if (!rn) ftext(RX + 23, LY + 13, FT_M, C_DIM, act ? "empty folder" : "no sounds here");
    if (!fmenu && !fconfirm) draw_footer(act ? "EDIT OPEN  HOLD KIT  Y BACK  < UP  X FILES" : browse_listen ? "EDIT LOAD  R2 LISTEN: ON  OPTION HEAR  Y BACK  < UP  X FILES" : "EDIT LOAD  R2 LISTEN: OFF  OPTION HEAR  Y BACK  < UP  X FILES");
    draw_fmenu();
}

// ---- SOUND > PRESETS (A12): the categories as tabs, the presets of the one under the cursor ----
static void draw_presets2(void) {
    int idx[PR_MAX], n = pr_list(idx, PR_MAX); Col ec = C_ENG(song.t[sel].type);
    pr_cur = clampi(pr_cur, 0, n - 1);
    #define PR_CAT(j) (idx[j] < 0 || (idx[j] >= 1000 && idx[j] < 2000) ? 1 : 0)   // A12: two tabs, FACTORY and MINE (SAVE CURRENT and the user's)
    int cur = PR_CAT(pr_cur);
    for (int t = 0; t < 2; t++) { int x = 26 + t * 492, on = t == cur; const char *nm = t ? "MINE" : "FACTORY";
        if (on) rrect(x, 76, 475, 38, 8, ec);
        ftext(x + 237 - ftext_w(FT_MB, nm) / 2, 83, FT_MB, on ? C_BG : C_DIM, nm); }
    int a = 0; while (a < n && PR_CAT(a) != cur) a++;
    int e = a; while (e < n && PR_CAT(e) == cur) e++;
    enum { ROWS = 9, RH = 61 }; int top = clampi(pr_cur - a - ROWS / 2, 0, e - a > ROWS ? e - a - ROWS : 0);
    static const char *const en[4] = {"SAMPLER", "WAVE", "ACID", "CLOUD"};
    for (int r = 0; r < ROWS && a + top + r < e; r++) { int j = a + top + r, i = idx[j], y = 130 + r * RH, on = j == pr_cur;
        const char *nm = i < 0 ? "+ SAVE CURRENT" : pr_name(i);
        if (on) rrect(26, y, 971, 50, 8, C_SELBG);
        ftext(48, y + 12, on ? FT_LB : FT_M, i < 0 ? C_DIM : C_WHITE, nm);
        const char *tg = i < 0 ? "" : i >= 1000 && i < 2000 ? "USER" : en[song.t[sel].type & 3];
        ftext(974 - ftext_w(FT_M, tg), y + 13, FT_M, on ? ec : C_DIM, tg); }
    if (e - a > ROWS) { int th = ROWS * RH - 11, h = th * ROWS / (e - a); rect(1004, 130, 4, th, C_CELL, 1); rect(1004, 130 + (th - h) * top / (e - a - ROWS), 4, h, C_DIM, 1); }
    int i = idx[pr_cur];
    draw_footer(i < 0 ? "EDIT SAVE THE SOUND YOU HAD  L1/R1 FACTORY  Y BACK" : i >= 1000 ? "EDIT LOAD  L1/R1 FACTORY  Y BACK  OPT LISTEN  OPT+EDIT DELETE"
                : "EDIT LOAD  L1/R1 MINE  Y BACK  OPT LISTEN");
    #undef PR_CAT
}
