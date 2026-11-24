// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// CHORDS (2026-11-19, user: "a chord palette, as onemotion's chord player, a third way next to STEPS and PIANO ROLL").
// The pad's key and scale give seven chords (I..VII), as triads, sevenths and sus4. The clip is cut into slots of
// LENGTH; EDIT puts the chosen chord in the slot and goes to the next. The chords are piano-roll notes, so the roll
// edits them afterwards. The clip's MODE (PATTERN's first card, EDIT + arrows) chooses STEPS, PIANO ROLL or CHORDS.
enum { CH_COLS = 7, CH_ROWS = 3, CH_TAG = 330 };
static int ch_row, ch_col, ch_slot, ch_len = 2, ch_inv, ch_oct, ch_heard;
static const int ch_len32[4] = {8, 16, 32, 64};
static const char *const ch_len_name[4] = {"1/4", "1/2", "1 BAR", "2 BARS"};
static const int ch_offs[CH_ROWS][4] = {{0, 2, 4, -1}, {0, 2, 4, 6}, {0, 3, 4, -1}};   // degrees above the root: triad, seventh, sus4
static int ch_scale(void) { int sc = track_scale(sel); return sc == 0 ? 1 : sc; }   // CHROMATIC has no chords of its own: MAJOR's
static int ch_notes(int row, int col, int *n) {   // the chord's pitches, low to high, with the inversion and the octave
    int sc = ch_scale(), root = deg_midi(sel, 0) - deg_semis(track_scale(sel), 0) + 12 * ch_oct, c = 0;
    if (scale_len(sc) < 5 && row == 2) row = 0;
    for (int i = 0; i < 4 && ch_offs[row][i] >= 0; i++) n[c++] = root + deg_semis(sc, col + ch_offs[row][i]);
    for (int k = 0; k < ch_inv % c; k++) { int lo = n[0]; for (int i = 0; i < c - 1; i++) n[i] = n[i + 1]; n[c - 1] = lo + 12; }
    for (int i = 0; i < c; i++) n[i] = clampi(n[i], 0, 127);
    return c;
}
static void ch_name(int row, int col, char *b, int sz) {   // C, Dm, Bdim, G7, Cmaj7, Am7, Bm7b5, Dsus4
    int sc = ch_scale(), r = deg_semis(sc, col), t = deg_semis(sc, col + 2) - r, f = deg_semis(sc, col + 4) - r, s = deg_semis(sc, col + 6) - r;
    const char *nm = note_name[(track_key(sel) + r) % 12], *q;
    if (row == 2) q = "sus4";
    else if (row == 1) q = t == 4 && s == 11 ? "maj7" : t == 4 && f == 7 ? "7" : t == 3 && f == 6 ? (s == 9 ? "dim7" : "m7b5") : t == 3 ? (s == 11 ? "mmaj7" : "m7") : "7";
    else q = t == 4 && f == 7 ? "" : t == 3 && f == 7 ? "m" : t == 3 && f == 6 ? "dim" : t == 4 && f == 8 ? "aug" : "";
    snprintf(b, sz, "%s%s", nm, q);
}
static int ch_slot_ticks(void) { return ch_len32[ch_len] * RT; }
static int ch_nslots(const Pattern *p) { int n = roll_ticks(p) / ch_slot_ticks(); return n < 1 ? 1 : n; }
static void ch_hear(int row, int col) {
    int n[4], c = ch_notes(row, col, n);
    for (int i = 0; i < ch_heard; i++) engine_release_key(CH_TAG + i);
    for (int i = 0; i < c; i++) engine_trig_key(sel, 0.8f, (float)note_semis(sel, n[i]), -1, CH_TAG + i);
    ch_heard = c;
}
static void ch_quiet(void) { for (int i = 0; i < ch_heard; i++) engine_release_key(CH_TAG + i); ch_heard = 0; }
static void ch_clear_slot(Pattern *p, int slot) {
    int a = slot * ch_slot_ticks(), b = a + ch_slot_ticks();
    for (int i = 0; i < NNOTE; i++) if (p->roll[i].len && p->roll[i].start >= a && p->roll[i].start < b) roll_del(p, i);
}
static void ch_put(int row, int col) {
    Pattern *p = cur_pat(sel); int n[4], c = ch_notes(row, col, n), L = ch_slot_ticks(), a = ch_slot * L;
    push_undo(); edit_begin(); if (!p->mode) p->mode = 2;
    ch_clear_slot(p, ch_slot);
    for (int i = 0; i < c; i++) if (roll_add(p, a, L, n[i], 100) < 0) { toast("THE CLIP IS FULL OF NOTES"); break; }
    ch_slot = (ch_slot + 1) % ch_nslots(p);
}
static int ch_slot_match(const Pattern *p, int slot, int *row, int *col) {   // which palette chord fills a slot: 1 found, 0 empty, -1 other notes
    int a = slot * ch_slot_ticks(), b = a + ch_slot_ticks(), pcs = 0, any = 0;
    for (int i = 0; i < NNOTE; i++) if (p->roll[i].len && p->roll[i].start >= a && p->roll[i].start < b) { pcs |= 1 << (p->roll[i].note % 12); any = 1; }
    if (!any) return 0;
    for (int r = 0; r < CH_ROWS; r++) for (int c = 0; c < CH_COLS; c++) { int n[4], k = ch_notes(r, c, n), m = 0;
        for (int i = 0; i < k; i++) m |= 1 << (n[i] % 12);
        if (m == pcs) { *row = r; *col = c; return 1; } }
    return -1;
}
static void draw_chord_mini(const Pattern *p, int x, int y, int w, int h, int on) {   // PATTERN's card: the clip's slots and their chords
    int n = ch_nslots(p), sw = w / n; char b[16]; Col ec = C_CAT(PC_PATTERN);
    for (int i = 0; i < n; i++) { int r, c, m = ch_slot_match(p, i, &r, &c), bx = x + i * sw;
        rrect(bx + 1, y, sw - 2, h, 4, m > 0 ? mix(C_CELL, ec, on ? 110 : 70) : C_LINE);
        if (m > 0) ch_name(r, c, b, sizeof b); else snprintf(b, sizeof b, "%s", m < 0 ? "NOTES" : "-");
        int f = ftext_w(FT_M, b) < sw - 6 ? FT_M : FT_S; if (ftext_w(f, b) < sw - 4) ftext(bx + sw / 2 - ftext_w(f, b) / 2, y + h / 2 - fv2[f].h / 2, f, m > 0 ? C_WHITE : C_DIM, b); }
}
static void chord_open(void) { Pattern *p = cur_pat(sel); chording = 1; ch_slot = 0; ch_row = 0; ch_col = 0; if (ch_slot >= ch_nslots(p)) ch_slot = 0; toast("CHORDS"); }
static void chord_leave(void) { ch_quiet(); chording = 0; }
static void chord_key(int k, int down) {
    static int combo;
    Pattern *p = cur_pat(sel);
    if (!down) {
        if (k == K_EDIT) { if (!combo && !held[K_OPT]) ch_put(ch_row, ch_col); ch_quiet(); }
        if (k == K_OPT) ch_quiet();
        return;
    }
    used[k] = 1;
    if (k == K_EDIT) { combo = 0; if (held[K_OPT]) { push_undo(); edit_begin(); ch_clear_slot(p, ch_slot); toast("SLOT CLEARED"); combo = 1; } else ch_hear(ch_row, ch_col); return; }
    if (held[K_EDIT]) combo = 1;
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_Y) { chord_leave(); return; }
    if (k == K_OPT) { ch_hear(ch_row, ch_col); return; }
    if (k == K_X) { ch_len = (ch_len + 1) % 4; ch_slot = 0; return; }
    if (k == K_R2) { ch_inv = (ch_inv + 1) % 3; ch_hear(ch_row, ch_col); return; }
    if (k == K_L1 || k == K_R1) { int n = ch_nslots(p); ch_slot = (ch_slot + (k == K_R1 ? 1 : n - 1)) % n; return; }
    if (held[K_EDIT] && (k == K_UP || k == K_DOWN)) { ch_oct = clampi(ch_oct + (k == K_UP ? 1 : -1), -2, 2); ch_hear(ch_row, ch_col); return; }
    int r0 = ch_row, c0 = ch_col;          // 2026-11-24: no wrap across, the edges lead to the side slots
    if (k == K_LEFT) { if (ch_col) ch_col--; } else if (k == K_RIGHT) { if (ch_col < CH_COLS - 1) ch_col++; }
    else if (k == K_UP) { if (ch_row) ch_row--; } else if (k == K_DOWN) { if (ch_row < CH_ROWS - 1) ch_row++; }   // no wrap: UP at the top goes on to the strips
    else return;
    if (lst_on && (r0 != ch_row || c0 != ch_col)) { ch_hear(ch_row, ch_col); }   // the LISTEN slot
}
static void draw_chords(void) {
    Pattern *p = cur_pat(sel); Col ec = C_CAT(PC_PATTERN); char b[32];
    static const char *const rn[7] = {"I", "II", "III", "IV", "V", "VI", "VII"}, *const rown[CH_ROWS] = {"TRIADS", "SEVENTHS", "SUS4"};
    int sc = ch_scale();
    for (int r = 0; r < CH_ROWS; r++) {
        ftext(wx0 + 6, 80 + r * 146, FT_S, C_DIM, rown[r]);   // 2026-11-24: taller cards fill the page
        for (int c = 0; c < CH_COLS; c++) {
            int cp = (ww + 10) / CH_COLS, cw = cp - 10, x = wx0 + c * cp, y = 100 + r * 146, on = r == ch_row && c == ch_col;
            int t = deg_semis(sc, c + 2) - deg_semis(sc, c); char rb[8]; snprintf(rb, sizeof rb, "%s", rn[c]);
            if (t == 3) for (char *q = rb; *q; q++) *q += 32;   // minor and diminished in small numerals
            if (on) rframe(x, y, cw, 122, 8, 4, ec, C_SELBG); else rrect(x, y, cw, 122, 8, C_CELL);
            ftext(x + 12, y + 8, FT_S, on ? ec : C_DIM, rb);
            ch_name(r, c, b, sizeof b); int f = ftext_w(FT_MB, b) <= cw - 14 ? FT_MB : ftext_w(FT_M, b) <= cw - 8 ? FT_M : FT_S;
            ftext(x + cw / 2 - ftext_w(f, b) / 2, y + 58, f, on ? C_WHITE : C_GREY, b);
        }
    }
    int n = ch_nslots(p), tx = wx0, tw = ww, ty = 560, th = 92, sw = tw / n;   // the clip: a block per slot, the chord in it
    ftext(wx0 + 6, ty - 22, FT_S, C_DIM, "CLIP");
    for (int i = 0; i < n; i++) { int x = tx + i * sw, r, c, m = ch_slot_match(p, i, &r, &c), on = i == ch_slot;
        if (on) rframe(x + 2, ty, sw - 4, th, 8, 4, C_WHITE, m ? mix(C_CELL, ec, 90) : C_CELL); else rrect(x + 2, ty, sw - 4, th, 8, m ? mix(C_CELL, ec, 60) : C_CELL);
        if (m > 0) ch_name(r, c, b, sizeof b); else snprintf(b, sizeof b, "%s", m < 0 ? "NOTES" : "-");
        if (ftext_w(FT_M, b) < sw - 12) ftext(x + sw / 2 - ftext_w(FT_M, b) / 2, ty + 32, FT_M, m ? C_WHITE : C_DIM, b); }
    { int st = engine_playing() && song.t[sel].on ? (int)((engine_posf() - 0) * RT) % roll_ticks(p) : -1;   // where the clip plays
      if (st >= 0) rect(tx + (int)((int64_t)st * tw / roll_ticks(p)), ty - 6, 3, th + 12, C_WHITE, 1); }
    snprintf(b, sizeof b, "%s %s", note_name[track_key(sel)], scale_name[track_scale(sel)]);
    char info[96]; snprintf(info, sizeof info, "INVERSION %d   OCTAVE %+d   %s", ch_inv, ch_oct, b);   // LENGTH: its side slot
    ftext(wx0 + 6, ty + th + 24, FT_M, C_GREY, info);
    { char nn[24], vv[32]; ch_name(ch_row, ch_col, vv, sizeof vv); snprintf(nn, sizeof nn, "SLOT %d/%d", ch_slot + 1, n); ui_focus(nn, vv, -1, ec); }
    draw_footer("EDIT PUT  OPT HEAR  EDIT+^v OCTAVE  OPT+EDIT CLEAR SLOT");
}
