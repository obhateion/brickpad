// PAD > SOUND (UI v2, step 4; mockups A08-A16): an engine's parameters as pages of four to six cards under a picture of
// what they shape. L1/R1 the page, arrows the card, EDIT+arrows the value; rare parameters on the last page, MORE.
// The pages are data: a name, a picture, the P_* ids. The cursor's parameter goes through cur_par(), so everything the
// 1.2 panel did with a control (EDIT+arrows, EDIT, OPT+EDIT, hearing it, LFO learn) works on a card.
// main.c includes this once, after ui.h.
#pragma once

enum { GF_NONE, GF_WAVE, GF_SLICES, GF_OSC, GF_FILTER, GF_ENV, GF_CLOUD, GF_PATCH };
typedef struct { const char *name; int8_t n, gfx; int16_t p[20]; } SndPage;
static const SndPage snd_smp[] = {
    {"SAMPLE", 4, GF_WAVE, {P_PITCH, P_ROOT, P_VOICE, P_DIR}},
    {"SLICES", 3, GF_SLICES, {P_SLICES, P_START, P_END}},
    {"WARP", 6, GF_WAVE, {P_WARP, P_BARS, P_BPM, P_NORM, P_GRAIN, P_FLUX}},
    {"ENV", 3, GF_WAVE, {P_SX + SX_FIN, P_SX + SX_FOUT, P_SX + SX_XFADE}},
    {"MORE...", 4, GF_NONE, {P_FINE, P_VOL, P_PAN, P_TYPE}},
};
static const SndPage snd_grd[] = {
    {"OSC", 6, GF_OSC, {P_SY + SP_OSC1, P_SY + SP_OSC2, P_SY + SP_TUNE2, P_SY + SP_DETUNE, P_SY + SP_MIX, P_SY + SP_NOISE}},
    {"FILTER", 5, GF_FILTER, {P_SY + SP_CUTOFF, P_SY + SP_RESO, P_SY + SP_ENV, P_GX + GX_HP, P_GX + GX_KBT}},
    {"ENV", 6, GF_ENV, {P_SY + SP_ATTACK, P_SY + SP_DECAY, P_SY + SP_SUSTAIN, P_SY + SP_RELEASE, P_SY + SP_GLIDE, P_VOICE}},
    {"PATCH", 0, GF_PATCH, {0}},
    {"MORE...", 20, GF_NONE, {P_PITCH, P_VOL, P_GX + GX_LRATE, P_GX + GX_LSYNC, P_GX + GX_LWAVE, P_GX + GX_LPITCH, P_GX + GX_LCUT, P_GX + GX_LPWM,
        P_GX + GX_O1OCT, P_GX + GX_O2OCT, P_GX + GX_SYNC, P_GX + GX_VCA, P_GX + GX_SPRING, P_GX + GX_ARP, P_GX + GX_AMODE, P_GX + GX_AOCT,
        P_GX + GX_ARATE, P_GX + GX_AGATE, P_GX + GX_ATTEN, P_TYPE}},
};
static const SndPage snd_acd[] = {
    {"OSC", 6, GF_OSC, {P_AC + AP_WAVE, P_AC + AP_TUNE, P_AX + AX_SUB, P_AX + AX_SUBOCT, P_AX + AX_SUBWAVE, P_AX + AX_PW}},
    {"FILTER", 6, GF_FILTER, {P_AC + AP_CUTOFF, P_AC + AP_RESO, P_AC + AP_ENVMOD, P_AC + AP_DECAY, P_AC + AP_ACCENT, P_AX + AX_TRACK}},
    {"DIST", 4, GF_NONE, {P_AC + AP_DTYPE, P_AC + AP_DRIVE, P_AC + AP_DTONE, P_AC + AP_DMIX}},
    {"MORE...", 12, GF_NONE, {P_VOL, P_AX + AX_NOISE, P_AX + AX_VATK, P_AX + AX_VREL, P_AX + AX_SLIDE, P_AX + AX_RANGE, P_AX + AX_RESHP, P_AX + AX_BASS,
        P_AX + AX_ACCATK, P_AX + AX_ACCDEC, P_AX + AX_CLIP, P_TYPE}},
};
static const SndPage snd_cld[] = {
    {"GRAINS", 4, GF_CLOUD, {P_CL + CP_POS, P_CL + CP_SPRAY, P_CL + CP_SIZE, P_CL + CP_DENSITY}},
    {"TONE", 6, GF_CLOUD, {P_CX + CX_PITCH, P_CX + CX_FINE, P_CX + CX_TONE, P_CL + CP_SHIMMER, P_CX + CX_SHMODE, P_CX + CX_PITCHRND}},
    {"ENV", 4, GF_CLOUD, {P_CX + CX_ATTACK, P_CX + CX_RELEASE, P_CL + CP_MOTION, P_CX + CX_FREEZE}},
    {"MORE...", 11, GF_NONE, {P_VOL, P_NORM, P_CX + CX_SIZERND, P_CX + CX_WINDOW, P_CX + CX_REVERSE, P_CX + CX_SYNC, P_CX + CX_TIMERND, P_CX + CX_WIDTH,
        P_CX + CX_SCAN, P_SAMPLE, P_TYPE}},
};
static int sp_pg[4], sp_cur;             // the page of each engine, kept; the card under the cursor, -1 = the picture

static const SndPage *snd_pages(int type, int *n) {
    switch (type & 3) {
    case 1: *n = (int)(sizeof snd_grd / sizeof *snd_grd); return snd_grd;
    case 2: *n = (int)(sizeof snd_acd / sizeof *snd_acd); return snd_acd;
    case 3: *n = (int)(sizeof snd_cld / sizeof *snd_cld); return snd_cld;
    }
    *n = (int)(sizeof snd_smp / sizeof *snd_smp); return snd_smp;
}
static const SndPage *snd_page(void) { int n; const SndPage *pg = snd_pages(song.t[sel].type, &n); return &pg[clampi(sp_pg[song.t[sel].type & 3], 0, n - 1)]; }
static int snd_cards(const SndPage *pg, int *out) {   // the cards on show: what the mode does not use is left out
    int c = 0; for (int i = 0; i < pg->n; i++) if (par_visible(pg->p[i])) out[c++] = pg->p[i];
    return c;
}
static int v2_sound(void) { return ui_cat == PC_SOUND && tab == T_SEQ && !hubbing && !patmenu && !perform && srow >= 3 && snd_page()->gfx != GF_PATCH; }
static int sound_par(void) {             // the parameter under the cursor; the picture stands for the file (or the presets)
    const SndPage *pg = snd_page(); int l[20], c = snd_cards(pg, l);
    if (sp_cur >= 0 && sp_cur < c) return l[sp_cur];
    return pg->gfx == GF_SLICES ? P_SLICES : song.t[sel].type == 3 ? P_SRC : P_SAMPLE;
}
static void sound_sync(void) {           // after any change of pad, engine or mode: the page and the cursor exist
    int n; const SndPage *pgs = snd_pages(song.t[sel].type, &n); int *pp = &sp_pg[song.t[sel].type & 3];
    *pp = clampi(*pp, 0, n - 1); srow = 3; scol = 0;
    const SndPage *pg = &pgs[*pp]; int l[20], c = snd_cards(pg, l);
    cl_deep = pg->gfx == GF_PATCH ? 2 : 0;
    if (sp_cur >= c) sp_cur = c - 1;
    if (sp_cur < 0 && pg->gfx == GF_NONE) sp_cur = 0;
    if (sp_cur < -1) sp_cur = -1;
}
static void sound_page_step(int d) {     // L1/R1
    int n; const SndPage *pgs = snd_pages(song.t[sel].type, &n); int *pp = &sp_pg[song.t[sel].type & 3];
    *pp = (*pp + d + n) % n; sp_cur = 0;
    if (pgs[*pp].gfx == GF_PATCH) gb_cur = 1;   // the 1.2 patch bay until it is rebuilt (A14)
    sound_sync();
}
static void sound_move(int k) {          // two columns of cards; UP from the first row: the picture
    const SndPage *pg = snd_page(); int l[20], c = snd_cards(pg, l);
    if (sp_cur < 0) { if (k == K_DOWN && c) sp_cur = 0; return; }
    if (k == K_LEFT) { if (sp_cur & 1) sp_cur--; }
    else if (k == K_RIGHT) { if (!(sp_cur & 1) && sp_cur + 1 < c) sp_cur++; }
    else if (k == K_DOWN) { if (sp_cur + 2 < c) sp_cur += 2; else if ((sp_cur | 1) + 1 < c) sp_cur = c - 1; }
    else if (sp_cur >= 2) sp_cur -= 2;
    else if (pg->gfx != GF_NONE) sp_cur = -1;
}
static int sound_edit_arrow(int k) {     // EDIT+arrows on the picture: the sampler's next file; nothing else changes there
    if (sp_cur >= 0) return 0;
    if (snd_page()->gfx == GF_SLICES) return 0;   // SLICES: the count, as on its card
    if (song.t[sel].type == 3) return 0;          // CLOUD: its source's next file (P_SRC)
    if (!is_synth(sel) && (k == K_LEFT || k == K_RIGHT)) { edit_begin(); neighbour_file(sel, k == K_RIGHT ? 1 : -1); }
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
            for (int yy = y + 10; yy < y + h - 30; yy += 6) rect(x0, yy, x1 - x0 + 2, 2, mix(C_CELL, ec, 110), 1);
            rect(px, y + 8, 2, h - 36, C_WHITE, 1); }
        if (s) ftext(x + 20, y + h - 28, FT_S, C_DIM, s->name);
        break;
    case GF_OSC:
        if (t->type == 2) { rect(x + 12, y + 10, w - 24, h - 20, C_BG, 1); snd_shape(x + 24, y + 22, w - 48, h - 44, t->ap[AP_WAVE] ? 2 : 1, ec); ftext(x + 22, y + 14, FT_S, C_DIM, "OSC"); }
        else for (int o = 0; o < 2; o++) { int bx = x + 12 + o * (w - 12) / 2, bw = (w - 36) / 2;
            rect(bx, y + 10, bw, h - 20, C_BG, 1); snd_shape(bx + 12, y + 30, bw - 24, h - 56, t->sp[o ? SP_OSC2 : SP_OSC1] & 3, ec);
            ftext(bx + 10, y + 14, FT_S, C_DIM, o ? "OSC 2" : "OSC 1"); }
        break;
    case GF_FILTER:
        if (t->type == 2) snd_filter(x + 12, y + 8, w - 24, h - 16, t->ap[AP_CUTOFF], t->ap[AP_RESO], ec);
        else snd_filter(x + 12, y + 8, w - 24, h - 16, t->sp[SP_CUTOFF], t->sp[SP_RESO], ec);
        break;
    case GF_ENV: snd_env(x + 16, y + 8, w - 32, h - 16, t->sp[SP_ATTACK], t->sp[SP_DECAY], t->sp[SP_SUSTAIN], t->sp[SP_RELEASE], ec); break;
    }
}
static const char *snd_label(int p) {
    switch (p) { case P_VOICE: return "MODE"; case P_DIR: return "DIRECTION"; case P_TYPE: return "ENGINE"; case P_VOL: return "VOLUME";
                 case P_SAMPLE: return "PRESETS"; case P_BPM: return "LOOP BPM"; }
    if (p == P_GX + GX_LRATE) return "LFO RATE"; if (p == P_GX + GX_LSYNC) return "LFO SYNC"; if (p == P_GX + GX_LWAVE) return "LFO WAVE";
    return par_name[p];
}
static void snd_card(int x, int y, int w, int h, int p, int on, Col ec) {
    char v[96]; if (p == P_SAMPLE) snprintf(v, sizeof v, "OPEN"); else par_str(sel, p, v, sizeof v);
    if (on) rframe(x, y, w, h, 8, 4, ec, C_CELL); else rrect(x, y, w, h, 8, C_CELL);
    const char *lb = snd_label(p); ftext(x + 20, y + 12, FT_M, C_DIM, lb);
    int room = w - 52 - ftext_w(FT_M, lb), f = ftext_w(FT_XB, v) <= room ? FT_XB : ftext_w(FT_LB, v) <= room ? FT_LB : FT_M;
    if (ftext_w(f, v) > room) v[room / fv2[f].w > 0 ? room / fv2[f].w : 0] = 0;
    ftext(x + w - 20 - ftext_w(f, v), y + 10 + (fv2[FT_XB].h - fv2[f].h) / 2, f, C_WHITE, v);
    if (par_is_number(p)) { int lo, hi, st, bg; par_limits(sel, p, &lo, &hi, &st, &bg);   // a bar for what is turned, none for what is chosen
        if (bg > 1 && hi > lo) { int by = y + h - 26; rect(x + 20, by, w - 40, 8, C_LINE, 1); rect(x + 20, by, (w - 40) * clampi(par_get(sel, p) - lo, 0, hi - lo) / (hi - lo), 8, ec, 1); } }
}
static void draw_sound2(void) {
    int n; const SndPage *pgs = snd_pages(song.t[sel].type, &n), *pg = snd_page(); int cur = (int)(pg - pgs);
    Col ec = C_ENG(song.t[sel].type); int l[20], c = snd_cards(pg, l);
    for (int i = 0; i < n; i++) {        // the pages as tabs
        int tw = 971 / n, cx = 26 + tw * i + tw / 2;
        if (i == cur) rrect(26 + tw * i, 73, tw - 10 > 184 ? 184 : tw - 10, 38, 8, ec);
        int px = i == cur ? 26 + tw * i + (tw - 10 > 184 ? 184 : tw - 10) / 2 : cx - 5;
        ftext(px - ftext_w(FT_MB, pgs[i].name) / 2, 80, FT_MB, i == cur ? C_BG : C_DIM, pgs[i].name);
    }
    int y0 = 127;
    if (pg->gfx != GF_NONE) {            // the picture; the cursor on it: the file (the sampler, CLOUD) or the presets
        if (sp_cur < 0) rframe(26, 127, 971, 243, 8, 4, ec, C_CELL); else rrect(26, 127, 971, 243, 8, C_CELL);
        snd_picture(pg, 26, 127, 971, 243, ec);
        y0 = 390;
    }
    int rows = (c + 1) / 2, tall = pg->gfx != GF_NONE && rows <= 2, h = tall ? 148 : 96, dy = tall ? 162 : 104;
    int vis = pg->gfx != GF_NONE ? (tall ? 2 : 3) : 5, first = clampi((sp_cur < 0 ? 0 : sp_cur / 2) - vis / 2, 0, rows > vis ? rows - vis : 0);
    for (int i = first * 2; i < c && i < (first + vis) * 2; i++)
        snd_card(i & 1 ? 518 : 32, y0 + (i / 2 - first) * dy, 473, h, l[i], i == sp_cur, ec);
    if (rows > vis) { int th = (vis * dy - 8) * vis / rows, ty = y0 + (vis * dy - 8 - th) * first / (rows - vis); rect(1004, y0, 4, vis * dy - 8, C_CELL, 1); rect(1004, ty, 4, th, C_DIM, 1); }   // more below
    if (sp_cur < 0) draw_footer(pg->gfx == GF_SLICES ? "EDIT SLICE EDITOR  L1/R1 PAGE  Y BACK  EDIT+ARROWS SLICES  OPT LISTEN"
                              : is_synth(sel) && song.t[sel].type != 3 ? "EDIT PRESETS  L1/R1 PAGE  Y BACK  OPT LISTEN"
                              : "EDIT FILE  L1/R1 PAGE  Y BACK  EDIT+<> NEXT FILE  OPT LISTEN");
    else draw_footer("EDIT+ARROWS VALUE  L1/R1 PAGE  Y BACK  OPT LISTEN  OPT+EDIT DEFAULT  OPT+ARROWS LISTEN NOTE  EDIT HEAR IT");
}
static const char *snd_page_name(void) { return snd_page()->name; }
static void snd_page_count(char *b, int n) { int k; const SndPage *pgs = snd_pages(song.t[sel].type, &k); snprintf(b, n, "%d/%d", (int)(snd_page() - pgs) + 1, k); }
