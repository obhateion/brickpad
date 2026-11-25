// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// BrickPad — UI with tabs (F1/F2), M8-style controls:
// arrows cursor · EDIT (B) act, EDIT+arrows value · OPTION (A) listen, OPTION+EDIT delete,
// OPTION+<> slot · patterns on the pattern field of SEQ+INST · SHIFT+OPTION copy, SHIFT+EDIT paste · PLAY transport.
#include "bp.h"
#include "synth.h"                       // ACID's voice
#include "wave.h"
#include "acid.h"
#include "cloud.h"
#include "fx.h"
#include "presets.h"
#include "wpresets.h"
_Static_assert(sizeof(Song) == 301612 + NSLOT * NPAT * 6 + NSLOT * NLFO * 8 + NSLOT * NPAT * NAUTO * (2 + NSTEP) + NSLOT * 16 + NSLOT * (1 + 32 + 16) + NSLOT * NPAT * NAUTO * NSTEP + NSLOT * NINS * 6 + NSLOT * 16 + NSLOT * 8 + NSLOT * 8 + NSLOT * 16 + NSUB * 20 + NJMP * 12 + NSLOT * 256, "Song changed size: bump STATE_MAGIC and convert the previous format in load_state");
static int len_lo(void) { return 1; }      // MIN/MAX LENGTH are gone (2026-11-09): a clip may be 1..NSTEP steps
static int len_hi(void) { return NSTEP; }
#define GROW_MAX 64                      // v3: the cursor grows a clip a bar at a time up to here (LENGTH still goes to NSTEP)
static int ui_qbar;                       // the quick bar at the bottom (defined with the other ui.cfg fields below)
#include <SDL2/SDL.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <time.h>
#include <dirent.h>
#include <utime.h>
#include <stdint.h>
#include "../third_party/font8x8_basic.h"

#include <math.h>
#define logf bp_logf   // math.h declares a logf of its own
#define W 1024
#define H 768
#define STATE "state.bin"

enum { K_UP, K_DOWN, K_LEFT, K_RIGHT, K_OPT, K_EDIT, K_X, K_Y, K_L1, K_L2, K_R1, K_R2, K_MENU, K_PLAY, K_SHIFT, K_F1, K_F2, K_N };
static const char *pad_label[K_N] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L1", "L2", "R1", "R2"};
static const int cell_key[NSLOT] = {K_L2, K_L1, K_R1, K_R2, K_LEFT, K_UP, K_Y, K_X, K_DOWN, K_RIGHT, K_EDIT, K_OPT};
#include "input.h"

typedef struct { Uint8 r, g, b; } Col;
// Colours come from the current theme. C_WHITE is the text colour and C_ORANGE the accent, whatever they are in a theme.
// UI v2 tokens (docs/UI-V2.md): the first ten keep the order of the .thm files of 1.2 (a file is numbers only,
// one colour a line), the engine and category colours follow; a file without them takes COSMIC's.
enum { TC_BG, TC_CELL, TC_LINE, TC_GREY, TC_DIM, TC_TEXT, TC_ACCENT, TC_ALERT, TC_SEL, TC_PLAY,
       TC_SMP, TC_GRD, TC_ACD, TC_CLD, TC_PAT, TC_SND, TC_FX, TC_MOD, TC_MIX, TC_N };
enum { TH_PRESETS = 5 };
#define RGB(x) {(x) >> 16 & 255, (x) >> 8 & 255, (x) & 255}
static const Col th_preset[TH_PRESETS][TC_N] = {
    {RGB(0x0B0C10), RGB(0x161820), RGB(0x2A2E3A), RGB(0x3D4250), RGB(0x80879A), RGB(0xE8E8E8), RGB(0xFFD23F), RGB(0xFF5A5A), RGB(0x23263A), RGB(0x7CFF6B),
     RGB(0x3FE0D0), RGB(0xFF4FA3), RGB(0xFFD23F), RGB(0x5AA0FF), RGB(0x7CFF6B), RGB(0xFFD23F), RGB(0xFF4FA3), RGB(0x5AA0FF), RGB(0xFF8A3D)},   // COSMIC: the mockups
    {RGB(0x000802), RGB(0x041408), RGB(0x0A3014), RGB(0x145024), RGB(0x288C46), RGB(0xAAFFB4), RGB(0x5AFF78), RGB(0xE6FFE6), RGB(0x082C12), RGB(0x78FF96),
     RGB(0x3CDC6E), RGB(0x8CFFAA), RGB(0x64F082), RGB(0x28B45A), RGB(0x78FF96), RGB(0x5AFF78), RGB(0x8CFFAA), RGB(0x28B45A), RGB(0x64F082)},   // MATRIX: green monochrome
    {RGB(0x1A1A1B), RGB(0x262628), RGB(0x3C3C3F), RGB(0x58585C), RGB(0x8C8C90), RGB(0xDCDCDE), RGB(0xDC3238), RGB(0xF02828), RGB(0x3C1418), RGB(0xC8C8CC),
     RGB(0xB4B4B8), RGB(0x96141E), RGB(0xD23C3C), RGB(0x6E6E74), RGB(0xC8C8CC), RGB(0xDC3238), RGB(0x96141E), RGB(0x6E6E74), RGB(0xD23C3C)},   // BLOOD CONCRETE: grey and dark red
    {RGB(0x0E0800), RGB(0x1E1204), RGB(0x402608), RGB(0x643C0C), RGB(0xAA6414), RGB(0xFFBE5A), RGB(0xFF9614), RGB(0xFFEBC8), RGB(0x3C2004), RGB(0xFFC83C),
     RGB(0xFFAA28), RGB(0xFF780A), RGB(0xFFD26E), RGB(0xC86E14), RGB(0xFFC83C), RGB(0xFF9614), RGB(0xFF780A), RGB(0xC86E14), RGB(0xFFD26E)},   // AMBER: orange monochrome
    {RGB(0x80913A), RGB(0x748536), RGB(0x5F6E32), RGB(0x536234), RGB(0x3C563C), RGB(0x192E19), RGB(0x2B442B), RGB(0x192E19), RGB(0x687635), RGB(0x3C563C),
     RGB(0x192E19), RGB(0x3C563C), RGB(0x2B442B), RGB(0x495D39), RGB(0x3C563C), RGB(0x2B442B), RGB(0x3C563C), RGB(0x495D39), RGB(0x2B442B)},   // GAMEBOY: light olive, half saturated (user 2026-11-14: "too loud")
};
static Col th_cur[TC_N] = {RGB(0x0B0C10), RGB(0x161820), RGB(0x2A2E3A), RGB(0x3D4250), RGB(0x80879A), RGB(0xE8E8E8), RGB(0xFFD23F), RGB(0xFF5A5A), RGB(0x23263A), RGB(0x7CFF6B),
     RGB(0x3FE0D0), RGB(0xFF4FA3), RGB(0xFFD23F), RGB(0x5AA0FF), RGB(0x7CFF6B), RGB(0xFFD23F), RGB(0xFF4FA3), RGB(0x5AA0FF), RGB(0xFF8A3D)};
#define C_ENG(t) th_cur[TC_SMP + ((t) & 3)]   // an engine by Track.type: SAMPLER WAVE ACID CLOUD
#define C_PLAY th_cur[TC_PLAY]
#define C_BG th_cur[TC_BG]
#define C_CELL th_cur[TC_CELL]
#define C_LINE th_cur[TC_LINE]
#define C_GREY th_cur[TC_GREY]
#define C_DIM th_cur[TC_DIM]
#define C_WHITE th_cur[TC_TEXT]
#define C_ORANGE th_cur[TC_ACCENT]
#define C_RED th_cur[TC_ALERT]
#define C_SELBG th_cur[TC_SEL]
static Col mix(Col a, Col b, int t) {    // t 0..255: a to b
    return (Col){(Uint8)(a.r + (b.r - a.r) * t / 255), (Uint8)(a.g + (b.g - a.g) * t / 255), (Uint8)(a.b + (b.b - a.b) * t / 255)};
}

enum { T_GRID, T_SEQ, T_PAT, T_FX, T_MIX, T_PRJ, T_N };
static const char *tab_name[T_N] = {"PADS", "INSTRUMENT", "PATTERNS", "FX", "MIXER", "PROJECT"};

static SDL_Renderer *R;
static FILE *logf;
static int koct, key_on[NSLOT];
static int rec;
static int scn_lock, rec_replace, ex_resample; static uint8_t rec_wiped[NSLOT];   // the quick drawer (2026-11-21): SCENE LOCK, REPLACE, RESAMPLE
static Uint32 combo_t; static double combo_pos;   // MENU+PLAY pressed then, waiting for a second PLAY
#define COMBO_MS 300
#define X_TWICE_MS 350               // two X taps this close double the clip
static Uint32 x_tap_t;                // the first of them, 0 = none
enum { ICON_COLS = 12, ICOL_N = 1 + 9 * 3 };   // ICOL_N: AUTO + the icon colours (icon_col)
static int iconing, ic_cur;
static int ducking;                      // the DUCK window is open (below, with the mixer)
static int tap_bpm(Uint32 *t, int *n);
static void naming_pad(void);
static int tab = T_GRID, browsing, slicing, theming, perform, sel = 5, undo_armed;
static int hub_cur, ui_cat = -1;   // UI v2: the pad hub is on screen; the category opened from it (-1 none), ui.h
enum { PC_PATTERN, PC_SOUND, PC_FX, PC_MOD, PC_MIX, PC_N };   // the categories inside a pad
static void cat_open(int c);              // ui.h: a pad's section (v3: the only way into a pad)
static int scening;                      // HOME > SCENES (A02) is on screen, over the pads
static int v2_unit(void), v2_mix(void), v2_mixer(void), v2_fx(void), v2_lfo(void), v2_auto(void), mixer2_tap(void); static void mixer2_arrow(int k);   // UI v2 step 5 pages (pages.h)
static int v2_steps(void);               // the rebuilt STEPS page is the screen now (below, before seq_clamp)
static Uint32 quit_down, flash[NSLOT];   // quit_down: SHIFT+PLAY held since then, 3 s quits (v3, 2026-11-20)
static char msg[80]; static Uint32 msg_t;

static void logmsg(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(logf, fmt, ap); va_end(ap); fputc('\n', logf); fflush(logf);
}
// A time mark is SDL_GetTicks() | 1, so it is never 0 but may be 1 ms ahead of "now": compare only through this.
static int ms_since(Uint32 now, Uint32 mark) { return (Sint32)(now - mark); }
// 2026-11-24 (user: "^v <> are unclear, draw them"): the keys' arrows as arrow glyphs (6 up, 7 down, 5 left, 4 right).
// keys: also a lone v < > (or after a +) in a key name; text: only ^v, <> and ^ (a v in a word stays a letter).
static void arrows(char *s, int keys) {
    char o[256]; int n = 0;
    for (int i = 0; s[i] && n < 250; i++) {
        char c = s[i], nx = s[i + 1], pv = i ? s[i - 1] : ' '; int lone = pv == ' ' || pv == '+' || pv == '(';
        if (keys && !strncmp(s + i, "ARROWS", 6) && (lone || pv == '+')) { o[n++] = 6; o[n++] = 7; o[n++] = 5; o[n++] = 4; i += 5; }   // the four arrows
        else if (keys && !strncmp(s + i, "UP/DOWN", 7)) { o[n++] = 6; o[n++] = 7; i += 6; }
        else if (c == '^' && nx == 'v') { o[n++] = 6; o[n++] = 7; i++; }
        else if (c == '<' && nx == '>') { o[n++] = 5; o[n++] = 4; i++; }
        else if (c == '^') o[n++] = 6;
        else if (keys && lone && (nx == ' ' || !nx || nx == ')') && (c == 'v' || c == '<' || c == '>')) o[n++] = c == 'v' ? 7 : c == '<' ? 5 : 4;
        else o[n++] = c;
    }
    o[n] = 0; memcpy(s, o, (size_t)n + 1);
}
static int ftext_w(int f, const char *s);
static void fit(char *b, int f, int maxw) {   // 2026-11-24 (audit): too long for its place: cut, and say so with ".."
    if (ftext_w(f, b) <= maxw) return; int l = (int)strlen(b);
    while (l > 2 && ftext_w(f, b) > maxw) { b[--l] = 0; if (l >= 2) { b[l - 2] = '.'; b[l - 1] = '.'; } }
}
static void units(char *s) {             // 2026-11-24 (audit): "12 DB" "0.02HZ" "300MS" read as dB, Hz, ms
    for (char *q = s; *q; q++) { int num = q > s && ((q[-1] >= '0' && q[-1] <= '9') || q[-1] == ' ');
        if (!num) continue;
        if (!strncmp(q, "DB", 2) && !(q[2] >= 'A' && q[2] <= 'Z')) { q[1] = 'B'; q[0] = 'd'; }
        else if (!strncmp(q, "KHZ", 3)) { q[0] = 'k'; q[1] = 'H'; q[2] = 'z'; }
        else if (!strncmp(q, "HZ", 2)) { q[1] = 'z'; }
        else if (!strncmp(q, "MS", 2) && !(q[2] >= 'A' && q[2] <= 'Z')) { q[0] = 'm'; q[1] = 's'; } }
}
static void toast(const char *s) { snprintf(msg, sizeof msg, "%s", s); arrows(msg, 0); msg_t = SDL_GetTicks(); }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float clampf1(float v) { return v < -1 ? -1 : v > 1 ? 1 : v; }

// ---------- drawing ----------
// Text (UI v2): DejaVu Sans Mono baked into the cells of the old 8x8 font (src/fontmono.h, tools/mkfont.py), so
// every layout keeps its places; scale 4 and up stays the dot-matrix of 8x8 (big readouts).
#include "fontmono.h"
static SDL_Texture *font_sc[4];          // white glyphs, alpha from the font, at scales 1..3
#include "fontv2.h"
static SDL_Texture *font_v2[FT_N];      // the mockups' sizes, natural spacing (ftext in ui.h)
static SDL_Texture *font_dig;            // the big digits (fdig in ui.h)
static void font_init(void) {
    for (int sc = 1; sc <= 3; sc++) {
        int c = 8 * sc, w = 95 * c; Uint32 *px = calloc((size_t)w * c, 4); const unsigned char *g = fm_glyph[sc];
        for (int ch = 0; ch < 95; ch++) for (int i = 0; i < c * c; i++) {
            int a = g[(ch * c * c + i) / 2] >> ((i & 1) ? 0 : 4) & 15;
            px[(i / c) * w + ch * c + i % c] = (Uint32)(a * 17) << 24 | 0xFFFFFF;
        }
        font_sc[sc] = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, c);
        SDL_UpdateTexture(font_sc[sc], NULL, px, w * 4);
        SDL_SetTextureBlendMode(font_sc[sc], SDL_BLENDMODE_BLEND);
        free(px);
    }
    for (int f = 0; f < FT_N; f++) {     // UI v2 fonts: 103 glyphs in a row
        const FontV2 *v = &fv2[f]; int w = 103 * v->w; Uint32 *px = calloc((size_t)w * v->h, 4);
        for (int g = 0; g < 103; g++) for (int i = 0; i < v->w * v->h; i++) {
            int a = v->px[g * v->per + i / 2] >> ((i & 1) ? 0 : 4) & 15;
            px[(i / v->w) * w + g * v->w + i % v->w] = (Uint32)(a * 17) << 24 | 0xFFFFFF;
        }
        font_v2[f] = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, v->h);
        SDL_UpdateTexture(font_v2[f], NULL, px, w * 4); SDL_SetTextureBlendMode(font_v2[f], SDL_BLENDMODE_BLEND); free(px);
    }
    { int w = 11 * FD_W; Uint32 *px = calloc((size_t)w * FD_H, 4);
      for (int g = 0; g < 11; g++) for (int i = 0; i < FD_W * FD_H; i++) {
          int a = fd_px[g * FD_PER + i / 2] >> ((i & 1) ? 0 : 4) & 15;
          px[(i / FD_W) * w + g * FD_W + i % FD_W] = (Uint32)(a * 17) << 24 | 0xFFFFFF;
      }
      font_dig = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, FD_H);
      SDL_UpdateTexture(font_dig, NULL, px, w * 4); SDL_SetTextureBlendMode(font_dig, SDL_BLENDMODE_BLEND); free(px); }
}
static void text(int x, int y, int sc, Col c, const char *s) {
    if (sc >= 4) {                       // big text is dot-matrix
        SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255);
        for (; *s; s++, x += 8 * sc) for (int b = 0; b < 8; b++) for (int a = 0; a < 8; a++)
            if (font8x8_basic[*s & 127][b] >> a & 1) { SDL_Rect d = {x + a * sc, y + b * sc, sc - 1, sc - 1}; SDL_RenderFillRect(R, &d); }
        return;
    }
    if (sc < 1) sc = 1;
    SDL_Texture *t = font_sc[sc]; int cl = 8 * sc;
    SDL_SetTextureColorMod(t, c.r, c.g, c.b);
    for (; *s; s++, x += cl) {
        int ch = *s & 127; if (ch <= 32 || ch > 126) continue;
        SDL_Rect src = {(ch - 32) * cl, 0, cl, cl}, dst = {x, y, cl, cl};
        SDL_RenderCopy(R, t, &src, &dst);
    }
}
static void rect(int x, int y, int w, int h, Col c, int fill) {
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255);
    SDL_Rect r = {x, y, w, h};
    if (fill) SDL_RenderFillRect(R, &r);
    else for (int i = 0; i < 3; i++) { SDL_RenderDrawRect(R, &r); r.x++; r.y++; r.w -= 2; r.h -= 2; }
}

static void frame(int x, int y, int w, int h, int t, Col c) {   // an outline t pixels thick, inside the box
    rect(x, y, w, t, c, 1); rect(x, y + h - t, w, t, c, 1); rect(x, y + t, t, h - 2 * t, c, 1); rect(x + w - t, y + t, t, h - 2 * t, c, 1);
}
enum { CAT_OTHER, CAT_KICK, CAT_SNARE, CAT_CLAP, CAT_HAT, CAT_OHAT, CAT_CYM, CAT_TOM, CAT_PERC, CAT_LOOP, CAT_BASS, CAT_MEL, CAT_VOX, CAT_FX, CAT_N };
#include "icons.h"

// ---------- song helpers ----------
#define UNDO_N 10
static Song undo_buf[UNDO_N]; static int undo_n, undo_top;
#define REDO_N 4                         // REDO (quick bar, 2026-11-21): what UNDO took back, until the next edit
static Song redo_buf[REDO_N]; static int redo_n, redo_top;
static void push_undo(void) { redo_n = 0; undo_buf[undo_top] = song; undo_top = (undo_top + 1) % UNDO_N; if (undo_n < UNDO_N) undo_n++; }
static void edit_begin(void) { if (!undo_armed) { push_undo(); undo_armed = 1; } }

// ---------- library index: what is known about the files, kept in library.idx ----------
enum { IDX_NONE, IDX_OK, IDX_BAD };
#define IDX_FILE "library.idx"
#define IDX_CAP 32768
typedef struct { char *path; int st; long size; } IdxE;
static IdxE idx[IDX_CAP]; static int idx_n; static volatile int idx_dirty; static SDL_mutex *idx_mx;
static IdxE *idx_find(const char *p) {
    unsigned h = 2166136261u; for (const char *c = p; *c; c++) h = (h ^ (unsigned char)*c) * 16777619u;
    for (;; h++) { IdxE *e = &idx[h % IDX_CAP]; if (!e->path || !strcmp(e->path, p)) return e; }
}
static int idx_get(const char *p, long *size) {
    SDL_LockMutex(idx_mx);
    IdxE *e = idx_find(p); int st = e->path ? e->st : IDX_NONE;
    if (size) *size = e->path ? e->size : -1;
    SDL_UnlockMutex(idx_mx);
    return st;
}
static void idx_put(const char *p, int st, long size) {
    SDL_LockMutex(idx_mx);
    IdxE *e = idx_find(p);
    if (e->path || idx_n < IDX_CAP * 3 / 4) {
        if (!e->path) { e->path = strdup(p); idx_n++; }
        if (e->st != st || e->size != size) idx_dirty = 1;
        e->st = st; e->size = size;
    }
    SDL_UnlockMutex(idx_mx);
}
static long file_size(const char *p) { struct stat st; return stat(p, &st) ? -1 : (long)st.st_size; }
static void idx_save(void) {
    SDL_LockMutex(idx_mx);
    FILE *f = fopen(IDX_FILE ".tmp", "w");
    if (f) {
        for (int i = 0; i < IDX_CAP; i++) if (idx[i].path) fprintf(f, "%d %ld %s\n", idx[i].st, idx[i].size, idx[i].path);
        fclose(f); rename(IDX_FILE ".tmp", IDX_FILE); idx_dirty = 0;
    }
    SDL_UnlockMutex(idx_mx);
}
static void idx_load(void) {
    if (!idx_mx) idx_mx = SDL_CreateMutex();
    FILE *f = fopen(IDX_FILE, "r"); if (!f) return;
    char line[600];
    while (fgets(line, sizeof line, f)) {
        int st, off = 0; long size;
        if (sscanf(line, "%d %ld %n", &st, &size, &off) < 2 || !off) continue;
        char *e = line + strlen(line); while (e > line && (e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
        if (line[off]) idx_put(line + off, st, size);
    }
    fclose(f); idx_dirty = 0;
}
static int idx_bad(const char *p) { return idx_get(p, NULL) == IDX_BAD; }

// CHECK LIBRARY: decodes every file once, slowly (a fast scan of the card can freeze the Brick), and resumes where it stopped.
static SDL_Thread *lib_thread; static volatile int lib_stop, lib_state, lib_seen, lib_badn;   // state 0 off, 1 running, 2 done
static void lib_walk(const char *dir, int depth) {
    Entry *e = malloc(sizeof(Entry) * 2048); if (!e) return;
    int n = list_dir(dir, e, 2048);
    for (int i = 0; i < n && !lib_stop; i++) {
        char p[1024]; snprintf(p, sizeof p, "%s/%s", dir, e[i].name);
        if (strlen(p) >= 500) continue;
        if (e[i].dir) { if (depth < 8) lib_walk(p, depth + 1); continue; }
        while (engine_playing() && !lib_stop) SDL_Delay(200);    // never while the music plays
        long known, sz = file_size(p); int st = idx_get(p, &known);
        if (st == IDX_NONE || known != sz) {
            st = sample_verify(p) ? IDX_OK : IDX_BAD;
            idx_put(p, st, sz);
            if (st == IDX_BAD) { logmsg("broken sample: %s", p); idx_save(); }
            SDL_Delay(30);
        } else SDL_Delay(3);
        lib_seen++; if (st == IDX_BAD) lib_badn++;
        if (lib_seen % 200 == 0 && idx_dirty) idx_save();
    }
    free(e);
}
static int lib_main(void *u) {
    (void)u;
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
    lib_walk("samples", 0);
    idx_save();
    if (!lib_stop) lib_state = 2;
    return 0;
}
static void lib_end(void) {
    if (lib_thread) { lib_stop = 1; SDL_WaitThread(lib_thread, NULL); lib_thread = NULL; }
}
static void lib_toggle(void) {
    if (lib_state == 1) { lib_end(); lib_state = 0; return; }
    lib_end();
    lib_stop = 0; lib_seen = lib_badn = 0; lib_state = 1;
    lib_thread = SDL_CreateThread(lib_main, "library", NULL);
    if (!lib_thread) lib_state = 0;
}

static Uint32 rng;
static Uint32 rnd(void) {
    if (!rng) rng = SDL_GetTicks() * 2654435761u | 1;
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}
static int slot_bad[NSLOT];              // this slot's file was broken and got replaced
static void frz_reset(void);
static char slot_want[NSLOT][256];       // the project's file that does not load: song keeps its path, the sound is a stand-in or none
static void norm_new(int s) { song.mix.pad[s].spare[PM_NORM] = 1; }   // a sample the user loads: NORMALIZE PEAK (see norm_name)
static int set_slot_raw(int s, const char *path) {   // 0 = the file does not play
    Sample *smp = path[0] ? sample_load(path) : NULL;
    if (path[0]) {
        long sz = file_size(path);
        if (!smp) { logmsg("broken sample: %s", path); if (sz >= 0) { idx_put(path, IDX_BAD, sz); idx_save(); } return 0; }
        idx_put(path, IDX_OK, sz);
    }
    snprintf(song.t[s].path, sizeof song.t[s].path, "%s", path);
    engine_set_sample(s, smp); slot_bad[s] = 0; slot_want[s][0] = 0;
    return 1;
}
static void set_slot_file(int s, const char *path) {   // a broken file gives way to a random working one from its folder
    if (set_slot_raw(s, path)) return;
    char d[512], b[120]; snprintf(d, sizeof d, "%s", path);
    char *sl = strrchr(d, '/'); const char *name = strrchr(path, '/'); name = name ? name + 1 : path;
    snprintf(b, sizeof b, "BROKEN > %.40s", name);
    if (!sl) { toast(b); return; }
    *sl = 0;
    Entry *e = malloc(sizeof(Entry) * 2048); if (!e) return;
    int n = list_dir(d, e, 2048);
    for (int i = n - 1; i > 0; i--) { int j = rnd() % (Uint32)(i + 1); Entry t = e[i]; e[i] = e[j]; e[j] = t; }
    for (int i = 0; i < n; i++) {
        char p[1024]; snprintf(p, sizeof p, "%s/%s", d, e[i].name);
        if (e[i].dir || strlen(p) >= 256 || idx_bad(p) || !set_slot_raw(s, p)) continue;
        slot_bad[s] = 1; toast(b); free(e); return;
    }
    free(e);
    snprintf(b, sizeof b, "BROKEN, NO WORKING FILE IN FOLDER"); toast(b);
}
static const char *file_base(const char *p) { const char *b = strrchr(p, '/'); return b ? b + 1 : p; }
static int notes_deg;                    // the loaded project has scale degrees in its steps: they become pitches once its samples are in
static void notes_to_midi(void) {        // as the old engine played them: the same sound
    for (int s = 0; s < NSLOT; s++) for (int q = 0; q < NPAT; q++) { Pattern *p = &song.t[s].pat[q];
        for (int i = 0; i < NSTEP; i++) if (p->note[i] != NOTE_NONE) p->note[i] = (int8_t)deg_midi(s, p->note[i] < -60 ? -60 : p->note[i] > 60 ? 60 : p->note[i]); }
    notes_deg = 0;
}
static void sync_samples(void) {  // reload slots whose path differs from what is loaded
    for (int s = 0; s < NSLOT; s++) {
        const Sample *x = engine_sample(s);
        if (slot_want[s][0] && !strcmp(slot_want[s], song.t[s].path)) continue;   // known, and undo must not change the stand-in
        if (strcmp(x ? x->path : "", song.t[s].path)) {
            char p[256]; snprintf(p, sizeof p, "%s", song.t[s].path); slot_bad[s] = 0; set_slot_file(s, p);
            x = engine_sample(s);
            if (x && strcmp(x->path, p) && !slot_bad[s]) { engine_set_sample(s, NULL); x = NULL; }   // no stand-in: not the old sound under a new name
            if (strcmp(x ? x->path : "", p)) {   // saved or pasted path stays, so the file comes back when it returns
                snprintf(song.t[s].path, sizeof song.t[s].path, "%s", p);
                snprintf(slot_want[s], sizeof slot_want[s], "%s", p); slot_bad[s] = 1;
                logmsg("slot %d: %s does not load, playing %s", s + 1, p, x ? x->path : "nothing");
            }
        }
    }
    if (notes_deg) notes_to_midi();
}
static void undo(void) {
    if (!undo_n) { toast("NOTHING TO UNDO"); return; }
    undo_top = (undo_top + UNDO_N - 1) % UNDO_N; undo_n--;
    Song cur = song; song = undo_buf[undo_top];
    redo_buf[redo_top] = cur; redo_top = (redo_top + 1) % REDO_N; if (redo_n < REDO_N) redo_n++;
    for (int s = 0; s < NSLOT; s++) { song.t[s].on = cur.t[s].on; song.t[s].queued = cur.t[s].queued; }
    sync_samples(); toast("UNDO");
}
static void redo(void) {
    if (!redo_n) { toast("NOTHING TO REDO"); return; }
    redo_top = (redo_top + REDO_N - 1) % REDO_N; redo_n--;
    undo_buf[undo_top] = song; undo_top = (undo_top + 1) % UNDO_N; if (undo_n < UNDO_N) undo_n++;   // as push_undo, keeping the redo
    Song cur = song; song = redo_buf[redo_top];
    for (int s = 0; s < NSLOT; s++) { song.t[s].on = cur.t[s].on; song.t[s].queued = cur.t[s].queued; }
    sync_samples(); toast("REDO");
}
static Pattern *cur_pat(int s) { return &song.t[s].pat[song.t[s].cur]; }
// ---- SUB cells (2026-11-20): the pool in Song.sub (bp.h). An entry exists only while a cell differs from the step.
static int pat_owner(const Pattern *p, int *s, int *q) {   // which pad and pattern of this scene p is
    for (int a = 0; a < NSLOT; a++) { ptrdiff_t k = p - song.t[a].pat; if (k >= 0 && k < NPAT) { *s = a; *q = (int)k; return 1; } }
    return 0;
}
static SubStep *sub_find(int s, int q, int st) {
    for (int i = 0; i < NSUB; i++) { SubStep *e = &song.sub[i]; if (e->slot == s + 1 && e->pat == q && e->step == st) return e; }
    return NULL;
}
static SubStep *sub_get(int s, int q, int st) {   // its entry, a new one if none (NULL: the pool is full)
    SubStep *e = sub_find(s, q, st); if (e) return e;
    for (int i = 0; i < NSUB; i++) if (!song.sub[i].slot) { e = &song.sub[i]; memset(e, 0, sizeof *e); e->slot = (uint8_t)(s + 1); e->pat = (uint8_t)q; e->step = (uint8_t)st; return e; }
    return NULL;
}
static void sub_tidy(SubStep *e) { if (!e->off) { for (int k = 0; k < NCELL; k++) if (e->vel[k] || e->note[k]) return; e->slot = 0; } }   // all as the step: no entry
static JumpStep *jmp_find(int s, int q, int st) {   // the step's PRB mode entry (NULL: NOTE)
    for (int i = 0; i < NJMP; i++) { JumpStep *e = &song.jmp[i]; if (e->slot == s + 1 && e->pat == q && e->step == st) return e; }
    return NULL;
}
static JumpStep *jmp_get(int s, int q, int st) {
    JumpStep *e = jmp_find(s, q, st); if (e) return e;
    for (int i = 0; i < NJMP; i++) if (!song.jmp[i].slot) { e = &song.jmp[i]; memset(e, 0, sizeof *e); e->slot = (uint8_t)(s + 1); e->pat = (uint8_t)q; e->step = (uint8_t)st;
        e->a[0] = e->a[1] = e->a[2] = -1; e->ch[0] = 50; e->ch[1] = e->ch[2] = 25; return e; }
    return NULL;
}
static void sub_drop(int s, int q, int st) { SubStep *e = sub_find(s, q, st); if (e) e->slot = 0; JumpStep *j = jmp_find(s, q, st); if (j) j->slot = 0; }   // the cells and the PRB mode
static void sub_drop_pat(int s, int q) { for (int i = 0; i < NSUB; i++) if (song.sub[i].slot == s + 1 && song.sub[i].pat == q) song.sub[i].slot = 0;
    for (int i = 0; i < NJMP; i++) if (song.jmp[i].slot == s + 1 && song.jmp[i].pat == q) song.jmp[i].slot = 0; }
static void sub_drop_p(const Pattern *p, int st) { int s, q; if (pat_owner(p, &s, &q)) { if (st < 0) sub_drop_pat(s, q); else sub_drop(s, q, st); } }   // st -1: the whole pattern
static int pat_used(const Pattern *p) {
    if (p->mode) { for (int i = 0; i < NNOTE; i++) if (p->roll[i].len) return 1; return 0; }
    for (int i = 0; i < NSTEP; i++) if (p->trig[i]) return 1;
    return 0;
}

// Writes a copy first; the old file stays unless the whole new one reached the card.
static int write_whole(const char *path, const void *d, size_t n) {
    char tmp[64]; snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb"); if (!f) return 0;
    int ok = fwrite(d, 1, n, f) == n && fflush(f) == 0 && fsync(fileno(f)) == 0;
    if (fclose(f)) ok = 0;
    if (ok && rename(tmp, path)) ok = 0;
    if (!ok) { remove(tmp); logmsg("could not write %s", path); }
    return ok;
}
// A file we cannot use whole is moved aside before autosave replaces it: state.bin.damaged, then .damaged2...
static void keep_damaged(void) {
    char p[64]; snprintf(p, sizeof p, STATE ".damaged");
    for (int i = 2; i < 10 && access(p, F_OK) == 0; i++) snprintf(p, sizeof p, STATE ".damaged%d", i);
    if (!rename(STATE, p)) logmsg("kept %s", p);
}
static void scene_save(void);
static void save_state(void) {
    scene_save();
    static Song saved;                   // the file is big now: write only when something changed
    if (!memcmp(&saved, &song, sizeof song)) return;
    if (write_whole(STATE, &song, sizeof song)) saved = song;
}
// Whatever is in the file, every value the code indexes with or divides by ends up in range.
#define KEEP(v, lo, hi) do { if ((v) < (lo)) (v) = (lo); else if ((v) > (hi)) (v) = (hi); } while (0)
static void song_sanitize(void) {
    KEEP(song.tempo, 40, 300); KEEP(song.swing, 0, 75); KEEP(song.launch, 0, 4); KEEP(song.metro, 0, 1); KEEP(song.master, 0, 100);
    KEEP(song.key, 0, 11); KEEP(song.scale, 0, NSCALE - 1); KEEP(song.count_in, 0, 8); song.len_min = song.len_max = 0;   /* MIN/MAX LENGTH are gone (2026-11-09): no limits */ KEEP(song.rec_quant, 0, 4); KEEP(song.snap, 0, 1); KEEP(song.rec_len, 0, 3);
    if (song.beats < 1 || song.beats > 12) song.beats = 4;
    for (int s = 0; s < NSLOT; s++) {
        Track *t = &song.t[s]; Cuts *c = &song.cut[s];
        KEEP(t->cur, 0, NPAT - 1); KEEP(t->mute, 0, 1); t->on = 0; t->queued = -1;
        KEEP(t->pitch, -24, 24); KEEP(t->fine, -50, 50); KEEP(t->vol, 0, 100); KEEP(t->pan, -50, 50);
        if (t->start < 0 || t->end > 1000 || t->start >= t->end) { t->start = 0; t->end = 1000; }
        KEEP(t->rev, 0, 1); KEEP(t->voice, 0, 1); KEEP(t->root, -1, 11); KEEP(t->warp, 0, 5); KEEP(t->type, 0, 3);
        if (t->type == 2 && t->ap[AP_MARK] != AP_INIT) acid_defaults(t->ap);
        if (t->type == 3 && t->cp[CP_MARK] != CP_INIT) cl_defaults(t->cp);
        if (song.cx[s][CX_MARK] != CX_INIT) cx_defaults(song.cx[s]);
        if (song.ax[s][AX_MARK] != AX_INIT) ax_defaults(song.ax[s]);
        if (song.wv[s][WP_MARK] == WP_INIT) for (int k = 0; k < WP_N - 1; k++) { int lo, hi; wp_range(k, &lo, &hi); KEEP(song.wv[s][k], lo, hi); }   // a spare byte stays 0
        KEEP(song.lnote[s], -48, 48);
        for (int k = 0; k < AX_N; k++) { int lo, hi; ax_range(k, &lo, &hi); KEEP(song.ax[s][k], lo, hi); }
        if (s == 0) { mix_sanitize(&song.mix); memset(song.sub, 0, sizeof song.sub);   // 2026-11-24: SUB is gone, its cells with it (user: not kept)
            for (int i = 0; i < NJMP; i++) { JumpStep *j = &song.jmp[i]; if (j->slot > NSLOT || j->pat >= NPAT || j->step >= NSTEP || j->mode >= PM_N || j->mode == PM_SUB || j->kind > 1 || j->form > 1) j->slot = 0; } }
        for (int q = 0; q < NPAT; q++) { uint8_t *fo = song.fol[s][q]; if (fo[FO_AFTER] > 64) fo[FO_AFTER] = 64; if (fo[FO_A] >= FA_N) fo[FO_A] = 0; if (fo[FO_B] >= FA_N) fo[FO_B] = 0;
            if (fo[FO_BCH] > 100) fo[FO_BCH] = 100; if (fo[FO_JUMP] >= NPAT) fo[FO_JUMP] = 0; fo[FO_SPARE] = 0; }
        for (int j = 0; j < NLFO; j++) { Lfo *L = &song.lfo[s][j];
            if (L->target >= LT_ENG + LT_ENGN || (L->target > LT_SENDB && L->target < LT_INS1) || (L->target >= LT_INS1 + 6 && L->target < LT_INS2) || (L->target >= LT_INS2 + 6 && L->target < LT_ENG)) L->target = 0;
            if (L->shape >= LS_N) L->shape = 0; L->sync = L->sync ? 1 : 0; if (L->sync ? L->rate >= LFO_NSYNC : L->rate > 100) L->rate = L->sync ? 8 : 50;
            KEEP(L->depth, -100, 100); if (L->retrig > NSLOT + 1) L->retrig = 0; if (L->phase > 100) L->phase = 0; L->spare &= 1; }   // spare bit 0: UNIPOLAR
        for (int q = 0; q < NPAT; q++) for (int a = 0; a < NAUTO; a++) { AutoLane *al = &song.aut[s][q][a];
            if (al->target >= LT_ENG + LT_ENGN || (al->target > LT_SENDB && al->target < LT_INS1) || (al->target >= LT_INS1 + 6 && al->target < LT_INS2) || (al->target >= LT_INS2 + 6 && al->target < LT_ENG)) al->target = 0;
            al->smooth = al->smooth ? 1 : 0; for (int i = 0; i < NSTEP; i++) { if (al->v[i] != AUTO_EMPTY) KEEP(al->v[i], 0, 100); KEEP(song.acurve[s][q][a][i], -100, 100); } }   // a file from before CLOUD DEEP: the defaults, the same sound
        for (int k = 0; k < CX_N; k++) { int lo, hi; cx_range(k, &lo, &hi); KEEP(song.cx[s][k], lo, hi); }
        for (int k = 0; k < CP_N; k++) KEEP(t->cp[k], 0, 100);
        for (int i = 0; i < NINS; i++) for (int k = 0; k < 6; k++) KEEP(song.mbx[s][i][k], -24, 24);
        KEEP(song.mix.pad[s].spare[DK_SRC], 0, NSLOT); KEEP(song.mix.pad[s].spare[DK_AMT], 0, 100);   // DUCK
        KEEP(song.mix.pad[s].spare[PM_NORM], 0, 2); KEEP(song.mix.pad[s].spare[PM_FRZ], 0, 1);
        song.pname[s][15] = 0; for (int k = 0; k < SX_N; k++) KEEP(song.smx[s][k], 0, 100);
        if (song.dkx[s][DX_MARK] == DX_INIT) { int8_t *dx = song.dkx[s]; KEEP(dx[DX_MODE], 0, DM_N - 1); KEEP(dx[DX_LEN], 0, DK_NLEN - 1); KEEP(dx[DX_GRID], 0, DK_NLEN - 1); KEEP(dx[DX_SMOOTH], 0, 100); KEEP(dx[DX_THRESH], 0, 100);
            for (int k = 0; k < 16; k++) if (song.dkc[s][k] > 100) song.dkc[s][k] = 100; }
        for (int k = 0; k < AP_N; k++) { int lo, hi; ap_range(k, &lo, &hi); KEEP(t->ap[k], lo, hi); }
        KEEP(t->bars, 0, 16);
        if (t->bpm < 0 || t->bpm > 300) t->bpm = 0;
        KEEP(t->grain, 0, 120); KEEP(t->flux, 0, 100); KEEP(t->scale, 0, NSCALE); KEEP(t->key, 0, 12); track_set_icon(t, clampi(track_icon(t), 0, IC_N));
        t->path[sizeof t->path - 1] = 0;
        KEEP(c->n, 0, 32);
        for (int k = 1; k < 32; k++) if (c->pos[k] < 0) c->pos[k] = 0;
        if (t->slices < 0 || t->slices > 6 || (t->slices == 6 && c->n < 1)) t->slices = 0;
        for (int q = 0; q < NPAT; q++) {
            Pattern *p = &t->pat[q];
            if (p->len < 1 || p->len > NSTEP) p->len = 16;
            if (p->div > 5) p->div = 1;
            KEEP(p->mode, 0, 2); p->spare = 0;
            for (int i = 0; i < NNOTE; i++) {
                Note *n = &p->roll[i];
                if (n->len && n->start >= NSTEP * 32 * RT) n->len = 0;
                if (n->len > NSTEP * 32 * RT) n->len = NSTEP * 32 * RT;
                if (n->len) { KEEP(n->note, 0, 127); KEEP(n->vel, 1, 127); KEEP(n->prob, 0, 100); n->flags &= NF_ACC; }   // an empty slot is left as it is (it made a saved project differ after each load)
            }
            for (int i = 0; i < NSTEP; i++) {
                KEEP(p->trig[i], 0, 1); KEEP(p->vel[i], 0, 127); KEEP(p->prob[i], 0, 100); p->rep[i] = 1; KEEP(p->slice[i], -1, 127);   // rep: SUB is gone
            }
        }
    }
}
static void wave_fix(Song *g) {          // a WAVE pad without its block (never set up) starts from the first preset
    for (int s = 0; s < NSLOT; s++) if (g->t[s].type == 1 && g->wv[s][WP_MARK] != WP_INIT) { wpreset_apply(g->wv[s], 0); g->t[s].voice = g->wv[s][WP_POLY] > 1; }
}
static size_t file_song_sz = sizeof(Song);   // where a project file's scenes begin (after the playing scene)
static void load_from(const char *path) {   // state.bin or a project file; 2026-11-25 (user): only the current format opens
    song_defaults();
    FILE *f = fopen(path, "rb");
    static Song tmp; memset(&tmp, 0, sizeof tmp);    // zero: fields a cut file does not have
    size_t got = f ? fread(&tmp, 1, sizeof tmp, f) : 0;
    int short_file = tmp.magic == song.magic && got < sizeof(Song);   // a cut file: what is there
    if (f) { fclose(f); f = NULL; }
    if (got >= offsetof(Song, t) && tmp.magic == song.magic) {
        song = tmp;
        wave_fix(&song);
        song_sanitize();
        if (short_file && !strcmp(path, STATE)) keep_damaged();        // a cut file: load what is there, keep the original too
        if (short_file && got < offsetof(Song, cut)) toast("PROJECT FILE WAS CUT, COPY KEPT");
        logmsg("state loaded%s", short_file ? " (short)" : "");
    } else {
        if (got && strcmp(path, STATE)) toast(tmp.magic != song.magic && (tmp.magic & 0xFFFFFF00u) == 0xB2B20000u ? "OLD PROJECT FORMAT: NOT OPENED" : "PROJECT FILE UNREADABLE");
        else if (got) { keep_damaged(); toast("PROJECT FILE UNREADABLE, COPY KEPT"); }
        static Entry e[64]; int n = list_dir("samples/808", e, 64), k = 0;
        for (int i = 0; i < n && k < NSLOT; i++)
            if (!e[i].dir) snprintf(song.t[k++].path, 256, "samples/808/%s", e[i].name);
        logmsg("default kit: %d files", k);
    }
    sync_samples(); frz_reset();
}
static void load_state(void) { load_from(STATE); }

static int roll_add(Pattern *p, int start, int len, int note, int vel); static void roll_del(Pattern *p, int i);   // piano roll, below
static int roll_base(int s); static int roll_ticks(const Pattern *p);
// ---------- recording ----------
// Clips with notes overdub in place. Empty clips collect the hits and get their length when the take ends.
typedef struct { int8_t slot, note, slice; double pos, end; int16_t tag; } RecEv;   // end: where its key was let go (1/32), -1 still held; tag 0 = a one-shot hit
static int rec_starting;                 // the first hit of an armed take started the count; until the engine runs, more hits (a chord) belong to step one
static int rec_tag_now;                  // the engine tag of the key making the hit being recorded (KEYS 200+, PADS 100+), 0 none
static struct { int16_t tag; int8_t s, pat, k, g; double pos; } rec_open[32];   // overdubbed roll notes still held: their length comes at release
static RecEv rec_ev[4096]; static int rec_n, rec_free[NSLOT], rec_pat[NSLOT]; static double rec_from;
static double rec_end = -1;              // a take that ends on the next launch quantum: there (1/32), -1 none
static int bar32(void) { return (song.beats > 0 ? song.beats : 4) * 8; }
static int rec_grid(int d) {             // quantize grid in 1/32 notes, never finer than the clip's own step
    static const int qg[5] = {0, 1, 2, 4, 8};
    int g = qg[song.rec_quant % 5];
    return g > d ? g : d;
}
static void rec_put(Pattern *p, int st, int note, int slice) {
    p->trig[st] = 1; p->vel[st] = 127; p->prob[st] = 100; p->rep[st] = 1; p->note[st] = note; p->slice[st] = slice;
}
static int take_length(double dur, int d, int up) {   // in 1/32 notes, rounded the way PROJECT says; up: keeps the last hit
    int u = bar32(); (void)d;            // REC LENGTH is gone (2026-11-09): to the bar
    int l = up ? ((int)(dur / u) + 1) * u : (int)(dur / u + 0.5) * u;
    return l < u ? u : l;
}
static int rec_midi(int s, int note) { return note == NOTE_NONE ? clampi(roll_base(s), 0, 127) : note; }   // hits carry MIDI pitches
static int rec_overlap(int s) {          // two notes of this slot sound at once: a chord or legato, steps cannot hold it
    for (int i = 0; i < rec_n; i++) if (rec_ev[i].slot == s && rec_ev[i].tag)
        for (int j = 0; j < rec_n; j++) if (j != i && rec_ev[j].slot == s && rec_ev[j].pos >= rec_ev[i].pos &&
            (rec_ev[i].end < 0 || rec_ev[j].pos < rec_ev[i].end - 0.25) && (j > i || rec_ev[j].pos > rec_ev[i].pos)) return 1;
    return 0;
}
static int build_clip(int s, Pattern *p, double dur, int up, double base, int unit) {   // rec_ev hits of this slot -> clip; returns its length
    int d = engine_step32(p), l = unit > 0 ? (int)floor(dur / unit + 0.5) * unit : take_length(dur, d, up);   // unit: the nearest whole quantum
    if (unit > 0 && l < unit) l = unit;
    while (l % d) d /= 2;                    // the length must be whole steps
    while (l / d > len_hi() && d < 32) d *= 2;   // longer than the clip may be at this step: coarser steps
    if (l / d > len_hi()) l = len_hi() * d;
    if (l / d < len_lo()) l = len_lo() * d;      // shorter than MIN LENGTH: the loop gets that long
    l -= l % d;
    int dv = 0; while ((1 << dv) < d) dv++;
    p->div = dv; p->len = l / d;
    int g = rec_grid(d);
    const Track *tr = &song.t[s];        // ROLL (user, 2026-11-08): the pattern is a roll already, a POLY WAVE, CLOUD, or notes overlap
    if (!track_nslices(s) && (p->mode || (tr->type == 1 && tr->voice == 1) || tr->type == 3 || rec_overlap(s))) {
        p->mode = 1; for (int k = 0; k < NNOTE; k++) roll_del(p, k);
        for (int i = 0; i < rec_n; i++) if (rec_ev[i].slot == s) {
            int qp = (int)((rec_ev[i].pos - base) / g + 0.5) * g; if (qp < 0) qp = 0;
            if (unit > 0 && qp >= l) continue;
            double e = !rec_ev[i].tag ? rec_ev[i].pos : rec_ev[i].end >= 0 ? rec_ev[i].end : base + dur;   // a one-shot: one grid
            int ln = (int)((e - rec_ev[i].pos) / g + 0.5) * g; if (ln < g) ln = g;
            roll_add(p, (qp % l) * RT, ln * RT, rec_midi(s, rec_ev[i].note), 127);
        }
        song.t[s].on = 1;
        if (engine_playing()) engine_set_anchor(s, (uint32_t)(base + 0.5));
        return l;
    }
    for (int i = 0; i < rec_n; i++) if (rec_ev[i].slot == s) {
        int qp = (int)((rec_ev[i].pos - base) / g + 0.5) * g;   // from the clip's step one, which is at base
        if (qp < 0) qp = 0;
        if (unit > 0 && qp >= l) continue;   // past the rounded end (often the "one" played again while stopping): not in the loop
        rec_put(p, (qp / d) % p->len, rec_ev[i].note, rec_ev[i].slice);
    }
    song.t[s].on = 1;
    if (engine_playing()) engine_set_anchor(s, (uint32_t)(base + 0.5));   // the loop goes on from the take's start without a jump
    return l;
}
static void toast_length(const char *what, int l) {
    char b[48]; int t = l * 100 / bar32();
    snprintf(b, sizeof b, "%s %d.%02d BARS", what, t / 100, t % 100); toast(b);
}
static void rec_finish(double end) {
    double dur = end - rec_from;
    int shown = 0;
    for (int s = 0; s < NSLOT; s++) {
        if (!rec_free[s]) continue;
        int any = 0; for (int i = 0; i < rec_n; i++) if (rec_ev[i].slot == s) any = 1;
        if (any) { int q = engine_quant32(); shown = build_clip(s, &song.t[s].pat[rec_pat[s]], dur, 0, rec_from, q ? q : bar32()); }   // the pattern the take began on, even if another one is on now
    }
    rec = 0;
    if (shown) toast_length("LENGTH", shown); else toast("REC OFF");
}
static void rec_hit(int s, int slice, int note);

// ---------- transport ----------
static int play_solo;                    // the count was started from inside a pad: it stops with the last clip
static int rec_wait;                     // a take is armed with the transport stopped: the first hit starts it
static void transport_toggle(void);
static void scn_unwait(void);
// As in Ableton: with the count running a clip starts (from its step one) or stops on the next launch quantum
// (PROJECT LAUNCH: 1/2/4 BARS, NONE = the next 1/32); pressing again before that cancels. Stopped: at once.
static void clip_toggle(int s) {
    Track *t = &song.t[s];
    if (engine_playing()) {
        if (engine_want(s) >= 0) engine_launch(s, -1);   // waiting: cancel
        else engine_launch(s, !t->on);
        return;
    }
    t->on = !t->on;
    if (t->on && engine_counting() < 0) engine_transport(1);
}
static void rec_tick(void) {             // a take that ends on a quantum: finish it there
    if (rec && rec_end >= 0 && engine_posf() >= rec_end) { double e = rec_end; rec_end = -1; rec_finish(e); }
}
static Uint32 last_input;                // the last button or d-pad event (defined again below)
static void play_hold_tick(void);
// 2026-11-11 (user: no transport of its own, as in Ableton): the count went on after the last clip stopped.
// 2026-11-19 (user: "the MIDI clock and the metronome must stop when every clip is stopped, else a live set cannot move
// the start of the bar"): the count stops as soon as no clip plays or waits to start; the next clip starts it again
// from the bar's beginning at the press. The sounds ring out (no stop of the voices). With CLOCK IN the other device
// leads, so it is left alone.
static int midi_clk_in;
static void launch_tick(void) {
    play_hold_tick();
    if (!engine_playing() || rec || rec_wait || midi_clk_in || engine_stop_pending()) return;
    for (int i = 0; i < NSLOT; i++) if (song.t[i].on || song.t[i].queued >= 0 || engine_want(i) == 1) return;
    play_solo = 0; engine_transport(0);
}
static void transport_toggle(void) {
    play_solo = 0; rec_wait = 0; rec_starting = 0;
    if (engine_playing() || engine_counting() >= 0) {
        if (rec) {
            if (engine_counting() >= 0) rec = 0;
            else if (rec_wait) rec = 0;          // armed, nothing played: nothing to keep
            else rec_finish(engine_posf());
        }
        rec_end = -1;
        engine_transport(0); engine_stop_all();
        for (int s = 0; s < NSLOT; s++) { song.t[s].on = 0; song.t[s].queued = -1; }
    } else {
        for (int s = 0; s < NSLOT; s++) song.t[s].on = pat_used(cur_pat(s));   // capture and recording: every clip with notes
        engine_transport(1);
    }
}
// The PLAY button. Stopped: inside a pad (SEQ+INST) it starts only that pad's clip, anywhere else only the count,
// the project's time, so clips launched later fall into it (a step is the global count modulo the clip).
// Running: inside a pad it starts or stops this pad's clip as EDIT does on GRID, the count goes on; elsewhere it stops.
// 2026-11-11: PLAY launches and stops the selected pad's clip, the same as EDIT on GRID, on every screen. The first
// clip started is the clock: the count begins with it. During a take PLAY still ends everything (the take is kept);
// on an armed take it starts the count. PLAY has no hold (2026-11-12, user: the old PLAY everywhere, no hold).
static int has_sound(int s);
// 2026-11-12 (user): PLAY has no hold on any screen. A tap acts at once on the press: GRID all on / all off, other
// screens the selected pad's clip. play_hold_tick and play_release stay as no-ops for the call sites.
static Uint32 play_down;
static int play_pend;                    // a PLAY tap waits for the release (2026-11-12): a hold then only stops everything, it never toggles first
static void play_release(void) { if (!play_pend) return; play_pend = 0; if (has_sound(sel)) clip_toggle(sel); }
static void play_hold_tick(void) {
    play_down = 0;                       // no hold any more: nothing to time
}
// PADS (2026-11-11, the user: no separate LIVE mode, PADS is both): a pad whose pattern has notes is a clip, its button
// starts and stops it on the quantum (a clip slot in Ableton, a cued chain in the M8's live mode); a pad with an empty
// pattern is a sound, its button plays it. SHIFT+button does the other of the two; while a take records every button
// plays by hand. SHIFT+F1/F2 choose a SCENE = one pattern number for every pad: pads with notes in it play it (from
// the next quantum), the others stop, as a scene row does. PLAY as everywhere: tap = the selected pad's clip, hold = stop all.
// (2026-11-15: a scene is no longer a pattern number: it is a whole set of pads, see SCENES below.)
// GRID selection (2026-11-11, user: "select pads as on the M8 and launch them in batches"): SHIFT+arrows stretch a
// rectangle from the pad the cursor stood on (gsel_a) to the cursor; a plain arrow drops it. PLAY and EDIT then work on
// every selected pad that has a clip: if one of them is not playing, those start and the rest go on; if all play, all
// stop; all on the same quantum. Y mutes them together the same way. The selection lives on GRID only.
static int gsel_a = -1, gsel_b;          // the corner it started from; the cursor it was stretched to (another cursor: no selection)
static int gsel_on(void) { return tab == T_GRID && !perform && gsel_a >= 0 && gsel_b == sel && gsel_a != sel; }
static int gsel_in(int i) {
    if (!gsel_on()) return 0;
    int c0 = gsel_a % 4, c1 = sel % 4, r0 = gsel_a / 4, r1 = sel / 4, c = i % 4, r = i / 4;
    return c >= (c0 < c1 ? c0 : c1) && c <= (c0 > c1 ? c0 : c1) && r >= (r0 < r1 ? r0 : r1) && r <= (r0 > r1 ? r0 : r1);
}
static int clip_goes(int s) {            // plays, or will from the next quantum
    int w = engine_playing() ? engine_want(s) : -1;
    return w >= 0 ? w : song.t[s].on;
}
static int gsel_launch(void) {           // 1 = the selection took the key
    if (!gsel_on()) return 0;
    int n = 0, off = 0, run = engine_playing(); char b[32];
    for (int s = 0; s < NSLOT; s++) if (gsel_in(s) && has_sound(s) && pat_used(cur_pat(s))) { n++; off += !clip_goes(s); }
    if (!n) { toast("NO CLIPS IN THE SELECTION"); return 1; }
    for (int s = 0; s < NSLOT; s++) { Track *t = &song.t[s];
        if (!gsel_in(s) || !has_sound(s) || !pat_used(cur_pat(s)) || clip_goes(s) == (off > 0)) continue;
        if (!run) t->on = off > 0;
        else if (engine_want(s) >= 0) engine_launch(s, -1);   // it was waiting to do the opposite: cancel that
        else engine_launch(s, off > 0);
    }
    if (!run && off && engine_counting() < 0) engine_transport(1);
    snprintf(b, sizeof b, "%d CLIPS %s", off ? off : n, off ? "START" : "STOP"); toast(b);
    return 1;
}
static int gsel_mute(void) {
    if (!gsel_on()) return 0;
    int n = 0, open = 0; char b[32];
    for (int s = 0; s < NSLOT; s++) if (gsel_in(s) && has_sound(s)) { n++; open += !song.t[s].mute; }
    if (!n) return 1;
    for (int s = 0; s < NSLOT; s++) if (gsel_in(s) && has_sound(s)) song.t[s].mute = open > 0;
    snprintf(b, sizeof b, "%d PADS %s", n, open ? "MUTED" : "MUTE OFF"); toast(b);
    return 1;
}
static int exporting;
// ---------- FREEZE (2026-11-11, user: "freeze tracks as in Ableton, with their automation and effects") ----------
// A frozen pad plays a recording of each of its patterns (one whole loop; engine, notes, inserts, modulation inside)
// and costs almost nothing; VOL PAN sends MUTE DUCK and the follow actions stay live. PadMix.spare[PM_FRZ] is the
// wish (saved with the project); frz_have = rendered and playing. frz_tick renders what is wished and not there yet
// (after a load too) and thaws a pad as soon as anything the recording depends on changes (frz_sum): editing a
// frozen pad just works, it is live again ("UNFROZEN"). What is chance in a pattern is fixed as it fell in the render.
static uint8_t frz_have[NSLOT]; static uint32_t frz_hash[NSLOT]; static int frz_wait; static float frz_mb;
static uint32_t fnv(uint32_t h, const void *d, size_t n) { const uint8_t *b = d; while (n--) { h ^= *b++; h *= 16777619u; } return h; }
static uint32_t frz_sum(int s) {         // everything the recording of pad s was made from
    static Track tt; tt = song.t[s]; tt.on = tt.queued = tt.cur = tt.mute = 0; tt.vol = tt.pan = 0; tt.icon = tt.icon_hi = 0;
    uint32_t h = fnv(2166136261u, &tt, sizeof tt);
    h = fnv(h, &song.cut[s], sizeof song.cut[s]); h = fnv(h, song.cx[s], sizeof song.cx[s]); h = fnv(h, song.ax[s], sizeof song.ax[s]);
    h = fnv(h, song.wv[s], sizeof song.wv[s]); h = fnv(h, song.lfo[s], sizeof song.lfo[s]);
    h = fnv(h, song.aut[s], sizeof song.aut[s]); h = fnv(h, song.acurve[s], sizeof song.acurve[s]); h = fnv(h, song.mbx[s], sizeof song.mbx[s]);
    h = fnv(h, song.mix.pad[s].ins, sizeof song.mix.pad[s].ins); h = fnv(h, &song.mix.pad[s].spare[PM_NORM], 1); h = fnv(h, song.smx[s], sizeof song.smx[s]);
    uint8_t sp = engine_sample(s) != NULL; float tm = tempo_mul;   // (the sample itself is its path, in the track: the sum must be the same after a restart)
    h = fnv(h, &sp, sizeof sp); h = fnv(h, &song.tempo, sizeof song.tempo); h = fnv(h, &tm, sizeof tm);
    h = fnv(h, &song.swing, 1); h = fnv(h, &song.key, 1); h = fnv(h, &song.scale, 1); h = fnv(h, &song.beats, 1);
    return h;
}
static void frz_drop(int s) {            // the recordings go, the pad plays live
    engine_lock(1); engine_freeze_on(s, 0);
    for (int q = 0; q < NPAT; q++) free(engine_freeze_buf(s, q, NULL, 0));
    engine_lock(0); frz_have[s] = 0;
}
static void frz_reset(void) { for (int s = 0; s < NSLOT; s++) frz_drop(s); frz_wait = 0; }   // another project: its frozen pads are rendered anew
static long mem_avail_mb(void) { long v = 4096; FILE *f = fopen("/proc/meminfo", "r"); char l[96]; if (!f) return v; while (fgets(l, sizeof l, f)) if (sscanf(l, "MemAvailable: %ld", &v) == 1) { v /= 1024; break; } fclose(f); return v; }
static int frz_render(int s) {           // 1 = pad s now plays its recordings
    static Song keep; static float z[256]; int made = 0; frz_mb = 0;
    engine_pause(1); keep = song;
    engine_transport(0); engine_stop_all(); for (int i = 0; i < 8; i++) engine_render(z, 128);
    engine_freeze_on(s, 0); for (int q = 0; q < NPAT; q++) free(engine_freeze_buf(s, q, NULL, 0));
    double f32 = SR * 60.0 / (keep.tempo * tempo_mul < 20 ? 20 : keep.tempo * tempo_mul) / 8;
    for (int q = 0; q < NPAT; q++) {
        if (!pat_used(&keep.t[s].pat[q])) continue;
        const Pattern *p = &keep.t[s].pat[q];
        double lf = (double)(p->len ? p->len : 16) * engine_step32(p) * f32; long n = (long)ceil(lf), a = (long)(lf + 0.5), need = a + n + 256;
        if (mem_avail_mb() < 150 + (long)((need + n) * 8 / 1048576)) { logmsg("freeze: pad %d pattern %d: not enough memory", s + 1, q + 1); made = -1; break; }
        float *tmp = malloc(sizeof(float) * 2 * (size_t)need), *d = malloc(sizeof(float) * 2 * (size_t)n);
        if (!tmp || !d) { free(tmp); free(d); made = -1; break; }
        song = keep;
        for (int x = 0; x < NSLOT; x++) { song.t[x].on = 0; song.t[x].queued = -1; }
        for (int i = 0; i < NINS; i++) { int kp = song.mbx[s][i][5]; const FxSlot *f = &song.mix.pad[s].ins[i];   // a keyed compressor hears its key pad while it is recorded
            if (fx_keyed(f->type) && f->on && kp > 0 && kp <= NSLOT && kp - 1 != s) song.t[kp - 1].on = (int8_t)pat_used(cur_pat(kp - 1)); }
        song.t[s].on = 1; song.t[s].cur = (int8_t)q; song.t[s].mute = 0; song.metro = 0; memset(song.fol[s], 0, sizeof song.fol[s]);   // this pattern, round and round
        engine_stop_all(); for (int i = 0; i < 200; i++) engine_render(z, 128);   // what rang before dies away
        engine_transport(1); engine_capture(s, tmp, need);
        while (engine_captured() < need) engine_render(z, 128);   // two loops: the second holds the tails of the first, so it closes on itself
        engine_capture(-1, NULL, 0);
        memcpy(d, tmp + 2 * a, sizeof(float) * 2 * (size_t)n); free(tmp);
        free(engine_freeze_buf(s, q, d, (int)n)); made++; frz_mb += n * 8 / 1048576.f;
        engine_transport(0); engine_render(z, 128);
    }
    song = keep; engine_transport(0); engine_stop_all(); for (int i = 0; i < 8; i++) engine_render(z, 128);
    if (made > 0) engine_freeze_on(s, 1); else for (int q = 0; q < NPAT; q++) free(engine_freeze_buf(s, q, NULL, 0));
    engine_pause(0);
    return made > 0;
}
// The recordings are kept on the card (2026-11-12, user: opening a project with frozen pads took too long): a file
// frozen/HHHHHHHH.frz named by frz_sum, so a pad frozen as it was is read back instead of rendered; any change gives
// another sum and a new file. The folder is kept under FRZ_KEEP_MB: the files unused for longest go first.
#define FRZ_DIR "frozen"
#define FRZ_KEEP_MB 300
static int frz_renders, frz_loads;        // counted for the log and the tests
static void frz_file(uint32_t h, char *p, int n) { snprintf(p, n, FRZ_DIR "/%08X.frz", h); }
static long frz_len(int s, int q) { const Pattern *p = &song.t[s].pat[q]; double f32 = SR * 60.0 / (song.tempo * tempo_mul < 20 ? 20 : song.tempo * tempo_mul) / 8; return (long)ceil((double)(p->len ? p->len : 16) * engine_step32(p) * f32); }
static void frz_prune(void) {             // oldest first, until the folder fits
    DIR *d = opendir(FRZ_DIR); if (!d) return;
    static struct { char n[24]; long sz; time_t t; } e[512]; int n = 0; long tot = 0; struct dirent *de; struct stat st; char p[64];
    while ((de = readdir(d)) && n < 512) { if (!strstr(de->d_name, ".frz") || strlen(de->d_name) > 20) continue; snprintf(p, sizeof p, FRZ_DIR "/%s", de->d_name); if (stat(p, &st)) continue;
        snprintf(e[n].n, sizeof e[n].n, "%s", de->d_name); e[n].sz = (long)(st.st_size >> 10); e[n].t = st.st_mtime; tot += e[n].sz; n++; }
    closedir(d);
    while (tot > FRZ_KEEP_MB * 1024L && n > 1) { int o = 0; for (int i = 1; i < n; i++) if (e[i].t < e[o].t) o = i;
        snprintf(p, sizeof p, FRZ_DIR "/%s", e[o].n); remove(p); tot -= e[o].sz; e[o] = e[--n]; }
}
static void frz_store(int s, uint32_t h) {
    char p[64], tp[72]; frz_file(h, p, sizeof p); snprintf(tp, sizeof tp, "%s.part", p); mkdir(FRZ_DIR, 0777);
    FILE *f = fopen(tp, "wb"); if (!f) return;
    uint32_t hd[3] = {0x5A524642u, h, 0}; int ok = 1;
    for (int q = 0; q < NPAT; q++) { int n; if (engine_freeze_get(s, q, &n)) hd[2]++; }
    ok &= fwrite(hd, sizeof hd, 1, f) == 1;
    for (int q = 0; q < NPAT && ok; q++) { int n; const float *d = engine_freeze_get(s, q, &n); if (!d) continue; uint32_t e[2] = {(uint32_t)q, (uint32_t)n};
        ok &= fwrite(e, sizeof e, 1, f) == 1 && fwrite(d, sizeof(float) * 2, (size_t)n, f) == (size_t)n; }
    ok &= fclose(f) == 0;
    if (ok) { rename(tp, p); frz_prune(); } else remove(tp);
}
static int frz_load(int s, uint32_t h) {  // 1 = the pad plays the recordings read from the card
    char p[64]; frz_file(h, p, sizeof p); FILE *f = fopen(p, "rb"); if (!f) return 0;
    uint32_t hd[3]; float *got[NPAT] = {0}; int gn[NPAT] = {0}, ok = fread(hd, sizeof hd, 1, f) == 1 && hd[0] == 0x5A524642u && hd[1] == h && hd[2] <= NPAT, want = 0;
    for (int q = 0; q < NPAT; q++) want += pat_used(&song.t[s].pat[q]);
    ok = ok && (int)hd[2] == want && want > 0;
    for (uint32_t i = 0; ok && i < hd[2]; i++) { uint32_t e[2];
        ok = fread(e, sizeof e, 1, f) == 1 && e[0] < NPAT && !got[e[0]] && pat_used(&song.t[s].pat[e[0]]) && (long)e[1] == frz_len(s, (int)e[0]) && mem_avail_mb() > 150 + (long)(e[1] >> 17);
        if (!ok) break;
        float *d = malloc(sizeof(float) * 2 * (size_t)e[1]); ok = d && fread(d, sizeof(float) * 2, e[1], f) == e[1];
        if (ok) { got[e[0]] = d; gn[e[0]] = (int)e[1]; } else free(d); }
    fclose(f);
    if (!ok) { for (int q = 0; q < NPAT; q++) free(got[q]); return 0; }
    engine_lock(1); engine_freeze_on(s, 0);
    for (int q = 0; q < NPAT; q++) free(engine_freeze_buf(s, q, got[q], gn[q]));
    engine_freeze_on(s, 1); engine_lock(0);
    utime(p, NULL);                          // used now: the last to be pruned
    return 1;
}
static int frozen(int s) { return song.mix.pad[s].spare[PM_FRZ] != 0; }
static void freeze_set(int s, int on) {  // the wish; frz_tick does the work on the next frames, so the words are seen first
    if (on && !has_sound(s)) return;
    song.mix.pad[s].spare[PM_FRZ] = (int8_t)(on ? 1 : 0);
    if (!on && frz_have[s]) frz_drop(s);
    if (on) frz_wait = 0;
}
static void freeze_key(void) {           // SHIFT+Y on GRID: the pad, or every selected pad; one not frozen -> they freeze, all frozen -> they thaw
    int n = 0, cold = 0; char b[40];
    for (int s = 0; s < NSLOT; s++) if ((gsel_on() ? gsel_in(s) : s == sel) && has_sound(s)) { n++; cold += frozen(s); }
    if (!n) { toast("EMPTY PAD: NOTHING TO FREEZE"); return; }
    for (int s = 0; s < NSLOT; s++) if ((gsel_on() ? gsel_in(s) : s == sel) && has_sound(s)) freeze_set(s, cold < n);
    if (cold < n) toast("FREEZING..."); else { snprintf(b, sizeof b, n > 1 ? "%d PADS UNFROZEN" : "UNFROZEN", n); toast(b); }
}
static void frz_tick(void) {
    static Uint32 last; Uint32 now = SDL_GetTicks(); char b[48];
    if (rec || exporting) return;
    for (int s = 0; s < NSLOT; s++) if (frz_have[s] && !frozen(s)) frz_drop(s);          // undone, or cleared
    for (int s = 0; s < NSLOT; s++) if (frozen(s) && !frz_have[s]) {                      // wished: render it, one pad a frame
        if (!has_sound(s)) { song.mix.pad[s].spare[PM_FRZ] = 0; continue; }
        uint32_t h = frz_sum(s);
        if (frz_load(s, h)) { frz_have[s] = 1; frz_hash[s] = h; frz_loads++; frz_wait = 0; snprintf(b, sizeof b, "PAD %d FROZEN (FROM THE CARD)", s + 1); toast(b); return; }   // as it was saved: no render
        if (frz_wait++ < 2) { if (frz_wait == 1) { snprintf(b, sizeof b, "FREEZING PAD %d...", s + 1); toast(b); } return; }
        frz_wait = 0;
        if (frz_render(s)) { frz_have[s] = 1; frz_hash[s] = frz_sum(s); frz_renders++; frz_store(s, frz_hash[s]); snprintf(b, sizeof b, "PAD %d FROZEN  %.1f MB", s + 1, frz_mb); toast(b); logmsg("freeze: pad %d, %.1f MB", s + 1, frz_mb); }
        else { song.mix.pad[s].spare[PM_FRZ] = 0; toast("COULD NOT FREEZE: NO NOTES OR NO MEMORY"); }
        last_input = SDL_GetTicks(); return;
    }
    if (ms_since(now, last) < 250) return; last = now;
    for (int s = 0; s < NSLOT; s++) if (frz_have[s] && frz_sum(s) != frz_hash[s]) {       // it was edited: live again
        frz_drop(s); song.mix.pad[s].spare[PM_FRZ] = 0; snprintf(b, sizeof b, "PAD %d UNFROZEN", s + 1); toast(b); }
}
static void play_key(void) {
    play_down = SDL_GetTicks() | 1;
    if (rec && rec_wait && !engine_playing() && engine_counting() < 0) {   // an armed take starts with the count
        for (int s = 0; s < NSLOT; s++) song.t[s].queued = -1;
        rec_wait = 0; engine_transport(1); return;
    }
    if (rec) { transport_toggle(); return; }
    if (engine_counting() >= 0) { transport_toggle(); return; }           // a count-in: PLAY cancels it
    if (gsel_launch()) return;
    if (tab == T_GRID && !perform && !browsing && !iconing) {   // GRID (2026-11-12, user: "the old PLAY back on GRID: everything on, everything off")
        int any = 0; for (int s = 0; s < NSLOT; s++) any |= song.t[s].on || engine_want(s) == 1;
        play_down = 0;
        if (engine_stop_pending()) { engine_queue_stop(0); toast("STOP CANCELLED"); return; }
        if (engine_playing() && !any) {      // the count runs on with no clip: every clip with notes joins on the quantum
            int n = 0; for (int s = 0; s < NSLOT; s++) if (has_sound(s) && pat_used(cur_pat(s))) { engine_launch(s, 1); n++; }
            toast(n ? "ALL CLIPS START" : "NO CLIPS TO PLAY"); return; }
        if (engine_playing()) {
            scn_unwait();
            engine_queue_stop(1);
            toast("STOP QUEUED");
        } else transport_toggle();
        return;
    }
    if (!has_sound(sel)) { toast("EMPTY PAD: NOTHING TO PLAY"); return; }
    clip_toggle(sel);                        // the pad's clip on or off at once, on the launch quantum
}
static void listen(int s) { engine_trig(s, 1.f, 0, -1); flash[s] = SDL_GetTicks(); }

// ---------- capture: what was played without recording, turned into clips afterwards ----------
typedef struct { Uint32 ms; double pos; int8_t slot, note, slice; float len; int16_t tag; } CapEv;   // pos < 0: transport was stopped; len (1/32) set at release, -1 held
#define CAP_N 2048
static CapEv cap[CAP_N]; static int cap_n;   // ring, cap_n hits so far
static void cap_log(int s, int slice, int note) {
    cap[cap_n % CAP_N] = (CapEv){SDL_GetTicks(), engine_playing() ? engine_posf() : -1, (int8_t)s, (int8_t)note, (int8_t)slice, -1, (int16_t)rec_tag_now};
    cap_n++;
}
static void capture(void) {
    if (rec) { toast("REC IS ON"); return; }
    if (!cap_n) { toast("NOTHING TO CAPTURE"); return; }
    double tempo = song.tempo * tempo_mul, ms32 = 60000.0 / tempo / 8, gap = ms32 * bar32() * 2;   // two bars of silence end a phrase
    int oldest = cap_n > CAP_N ? cap_n - CAP_N : 0, first = cap_n - 1;
    const CapEv *last = &cap[first % CAP_N];
    int stopped = last->pos < 0;
    while (first > oldest) {
        const CapEv *a = &cap[(first - 1) % CAP_N], *b = &cap[first % CAP_N];
        if ((a->pos < 0) != stopped || b->ms - a->ms > gap || (!stopped && a->pos > b->pos)) break;
        first--;
    }
    const CapEv *f = &cap[first % CAP_N];
    rec_n = 0;
    for (int i = first; i < cap_n && rec_n < (int)(sizeof rec_ev / sizeof *rec_ev); i++) {
        const CapEv *e = &cap[i % CAP_N];
        double ps = stopped ? (e->ms - f->ms) / ms32 : e->pos;
        rec_ev[rec_n++] = (RecEv){e->slot, e->note, e->slice, ps, e->len >= 0 ? ps + e->len : -1, e->tag};
    }
    double span = rec_ev[rec_n - 1].pos - rec_ev[0].pos;
    push_undo();
    int shown = 0, full = 0;
    for (int s = 0; s < NSLOT; s++) {
        int any = 0; for (int i = 0; i < rec_n; i++) if (rec_ev[i].slot == s) any = 1;
        if (!any) continue;
        Track *t = &song.t[s]; int q = t->cur, k = 0;   // never over a clip with notes: the next empty pattern
        while (k < NPAT && pat_used(&t->pat[q])) { q = (q + 1) % NPAT; k++; }
        if (k == NPAT) { full = 1; continue; }
        t->cur = q; t->queued = -1;
        shown = build_clip(s, &t->pat[q], span, 1, 0, 0);
    }
    cap_n = 0;
    if (shown && stopped) transport_toggle();
    if (shown) toast_length(full ? "SOME SLOTS FULL, CAPTURED" : "CAPTURED", shown);
    else toast("NO FREE PATTERN");
}

static void auto_setup(int s) {   // a loop gets 16 slices on the grid (WARP BEATS), anything else plays whole
    const Sample *x = engine_sample(s); Track *t = &song.t[s];
    t->bars = 0; t->root = -1; t->bpm = 0; song.cut[s].n = 0;   // points belong to the old sample
    if (x && x->loop) { t->slices = 3; t->warp = 1; relayout(s); toast("LOOP: 16 SLICES, WARP BEATS"); }
    else { t->slices = 0; t->warp = 0; }
}

// ---------- browser (inside SEQ+INST) ----------
static char bdir[512] = "samples";
static Entry bent[2048]; static int bn, bcur, btop;
#define PRJ_DIR "projects"
static int bmode;                         // 0 samples, 1 projects (files *.bpp under projects/)
static int fmenu, fconfirm;               // the browser's file menu (X) and its DELETE question
static void draw_fmenu(void); static int fm_key(int k);
static void browse_open(const char *dir) {
    snprintf(bdir, sizeof bdir, "%s", dir);
    bn = list_dir_ext(bdir, bent, 2048, bmode ? ".bpp" : NULL); bcur = 0; btop = 0;
    const char *cur = song.t[sel].path, *slash = strrchr(cur, '/');
    if (slash && (size_t)(slash - cur) == strlen(bdir) && !strncmp(cur, bdir, slash - cur))
        for (int i = 0; i < bn; i++) if (!strcmp(bent[i].name, slash + 1)) bcur = i;
    browsing = 1;
}
static void browse_close(void) { engine_set_sample(PREVIEW, NULL); browsing = 0; fmenu = fconfirm = 0; }
// LISTEN in the browser (2026-11-19, user: "in a live set the sample I am choosing plays into the master"): R2 turns
// it off, the arrows are silent then and OPTION still listens on purpose. Kept in listen.cfg beside the binary.
static int browse_listen = 1;
static void browse_listen_set(int on) { browse_listen = on; write_whole("listen.cfg", on ? "1\n" : "0\n", 2); if (!on) engine_set_sample(PREVIEW, NULL); }
static void browse_listen_load(void) { FILE *f = fopen("listen.cfg", "r"); if (f) { int v = 1; if (fscanf(f, "%d", &v) == 1) browse_listen = v != 0; fclose(f); } }
static void browse_preview(void) {
    if (bmode || bcur >= bn || bent[bcur].dir) return;
    char p[1024]; snprintf(p, sizeof p, "%s/%s", bdir, bent[bcur].name);
    Sample *s = sample_load(p);
    long sz = file_size(p);
    if (!s && sz >= 0) { logmsg("broken sample: %s", p); idx_put(p, IDX_BAD, sz); idx_save(); toast("BROKEN FILE"); }
    else if (s) idx_put(p, IDX_OK, sz);
    engine_set_sample(PREVIEW, s);
    if (s) engine_trig(PREVIEW, 1.f, 0, -1);
}
// A whole pack: the files of a folder and its subfolders go on the 12 slots, laid out like a drum kit.
// A bigger pack gives a new random kit each time. The type comes from the name, then from the nearest folder.
static char pk[4096][256]; static signed char pk_cat[4096], pk_used[4096]; static int pk_n;
static int has_sub(const char *s, const char *const *w) { for (; *w; w++) if (strstr(s, *w)) return 1; return 0; }
static int has_tok(const char *s, const char *const *w) {   // a whole word, an s at the end allowed
    for (; *w; w++) {
        size_t l = strlen(*w);
        for (const char *p = s; (p = strstr(p, *w)); p++) {
            char a = p > s ? p[-1] : 0, b = p[l]; if (b == 's') b = p[l + 1];
            if (!(a >= 'a' && a <= 'z') && !(b >= 'a' && b <= 'z')) return 1;
        }
    }
    return 0;
}
static int word_cat(const char *s) {     // lower case
    static const char *const loop[] = {"loop", "break", "groove", "bpm", 0}, *const fill[] = {"fill", "beat", "top", "drum", 0},
        *const kick[] = {"kick", "kik", "bassdrum", 0}, *const kick2[] = {"bd", 0},
        *const snare[] = {"snare", "snr", "rimshot", 0}, *const snare2[] = {"sd", 0},
        *const clap[] = {"clap", "snap", "clp", 0},
        *const ohat[] = {"openhat", "open hat", "open-hat", "open_hat", "hat open", "hat_open", "ophat", 0}, *const ohat2[] = {"ohh", "oh", "ohat", 0},
        *const hat[] = {"hihat", "hi-hat", "hi hat", "closedhat", 0}, *const hat2[] = {"hat", "chh", "hh", "ch", "chat", 0},
        *const cym[] = {"cymbal", "crash", 0}, *const cym2[] = {"ride", "china", "splash", 0},
        *const tom[] = {"lotom", "midtom", "hitom", "lowtom", "hightom", "floortom", 0}, *const tom2[] = {"tom", 0},
        *const perc[] = {"perc", "cowbell", "conga", "bongo", "shaker", "clave", "tambourine", "woodblock", 0}, *const perc2[] = {"rim", "tamb", "block", "cowb", 0},
        *const bass[] = {"bassline", "reese", 0}, *const bass2[] = {"bass", "sub", "boom", 0},
        *const vox[] = {"vocal", "acapella", "choir", 0}, *const vox2[] = {"vox", "voice", 0},
        *const fx[] = {"sfx", "riser", "impact", "sweep", "noise", "texture", "ambien", "atmo", "drone", "downlifter", "uplifter", 0}, *const fx2[] = {"fx", 0},
        *const mel[] = {"synth", "chord", "piano", "melod", "stab", "pluck", 0}, *const mel2[] = {"lead", "pad", "arp", "key", "bell", "seq", 0};
    if (has_sub(s, loop)) return CAT_LOOP;
    if (has_sub(s, ohat) || has_tok(s, ohat2)) return CAT_OHAT;
    if (has_sub(s, hat) || has_tok(s, hat2)) { if (strstr(s, "open")) return CAT_OHAT; return CAT_HAT; }
    if (has_sub(s, kick) || has_tok(s, kick2)) return CAT_KICK;
    if (has_sub(s, snare) || has_tok(s, snare2)) return CAT_SNARE;
    if (has_sub(s, clap)) return CAT_CLAP;
    if (has_sub(s, cym) || has_tok(s, cym2)) return CAT_CYM;
    if (has_sub(s, tom) || has_tok(s, tom2)) return CAT_TOM;
    if (has_sub(s, perc) || has_tok(s, perc2)) return CAT_PERC;
    if (has_sub(s, bass) || has_tok(s, bass2)) return CAT_BASS;
    if (has_sub(s, vox) || has_tok(s, vox2)) return CAT_VOX;
    if (has_sub(s, fx) || has_tok(s, fx2)) return CAT_FX;
    if (has_sub(s, mel) || has_tok(s, mel2)) return CAT_MEL;
    if (has_tok(s, fill)) return CAT_LOOP;
    return CAT_OTHER;
}
static int file_cat(const char *path) {
    char low[256]; int i;
    for (i = 0; path[i] && i < 255; i++) low[i] = (char)(path[i] >= 'A' && path[i] <= 'Z' ? path[i] + 32 : path[i]);
    low[i] = 0;
    char *sl = strrchr(low, '/');
    char *dot = strrchr(low, '.'); if (dot && (!sl || dot > sl)) *dot = 0;
    int c = word_cat(sl ? sl + 1 : low);
    if (c != CAT_OTHER) return c;
    float bpm; int root; sample_name_info(path, &bpm, &root);
    while (sl) {                         // nearest folder first
        *sl = 0;
        char *up = strrchr(low, '/');
        c = word_cat(up ? up + 1 : low);
        if (c != CAT_OTHER) return c;
        sl = up;
    }
    return bpm > 0 ? CAT_LOOP : CAT_OTHER;
}
static void pack_scan(const char *dir, int depth) {
    Entry *e = malloc(sizeof(Entry) * 2048); if (!e) return;
    int n = list_dir(dir, e, 2048);
    for (int pass = 0; pass < 2; pass++)         // the folder's own files first, then subfolders
        for (int i = 0; i < n; i++) {
            char p[1024]; snprintf(p, sizeof p, "%s/%s", dir, e[i].name);
            if (pass == 0 && !e[i].dir && strlen(p) < 256 && pk_n < 4096 && !idx_bad(p)) { snprintf(pk[pk_n], 256, "%s", p); pk_cat[pk_n] = file_cat(p); pk_used[pk_n] = 0; pk_n++; }
            if (pass == 1 && e[i].dir && depth < 6 && strlen(p) < 500) pack_scan(p, depth + 1);
        }
    free(e);
}
static int fits_project(int i) {         // loops near the tempo, melodic parts in the scale
    float bpm; int root; sample_name_info(pk[i], &bpm, &root);
    if (pk_cat[i] == CAT_LOOP) {
        if (bpm <= 0) return 0;
        float r = bpm / song.tempo;
        return (r > 0.87f && r < 1.15f) || (r > 0.435f && r < 0.575f) || (r > 1.74f && r < 2.3f);   // half and double time fit too
    }
    if (root < 0) return 0;
    int d = ((root - song.key) % 12 + 12) % 12, n = scale_len(song.scale);
    for (int k = 0; k < n; k++) if (deg_semis(song.scale, k) % 12 == d) return 1;
    return 0;
}
static int pack_take(int cat, int slot) {   // cat -1 = anything; a broken file just makes way for the next
    for (int fit = 1; fit >= 0; fit--)
        for (int i = 0; i < pk_n; i++) {
            if (pk_used[i] || (cat >= 0 && pk_cat[i] != cat)) continue;
            if (fit && !((pk_cat[i] == CAT_LOOP || pk_cat[i] == CAT_MEL || pk_cat[i] == CAT_BASS) && fits_project(i))) continue;
            pk_used[i] = 1;
            if (set_slot_raw(slot, pk[i])) { norm_new(slot); return 1; }
            pk_used[i] = 2;
        }
    return 0;
}
static void draw(void);
static void load_pack(const char *dir, int random) {   // random: any files on any slots, repeated if fewer than 12 (as in version 1)
    pk_n = 0; pack_scan(dir, 0);
    if (!pk_n) { toast("NO SAMPLES IN THIS FOLDER"); return; }
    toast("LOADING PACK..."); draw();
    push_undo();
    for (int i = pk_n - 1; i > 0; i--) {          // shuffled: every pick below is random within its kind
        int j = rnd() % (Uint32)(i + 1); char t[256]; signed char c = pk_cat[i];
        memcpy(t, pk[i], 256); memcpy(pk[i], pk[j], 256); memcpy(pk[j], t, 256);
        pk_cat[i] = pk_cat[j]; pk_cat[j] = c;
    }
    int full[NSLOT] = {0}, placed = 0;
    if (random) {
        int good[NSLOT], ng = 0;
        for (int i = 0; i < pk_n && ng < NSLOT; i++) {   // already shuffled
            pk_used[i] = 1;
            if (set_slot_raw(ng, pk[i])) { norm_new(ng); good[ng] = i; full[ng] = 1; ng++; } else pk_used[i] = 2;
        }
        placed = ng;
        for (int s = ng; ng && s < NSLOT; s++) if (set_slot_raw(s, pk[good[rnd() % (Uint32)ng]])) { norm_new(s); full[s] = 1; }
    }
    // the kit layout of version 1: slot numbers are L2 L1 R1 R2 / LEFT UP Y X / DOWN RIGHT B A
    static const struct { signed char slot, want[4]; } role[NSLOT] = {
        {11, {CAT_KICK, -1, -1, -1}},                {10, {CAT_SNARE, CAT_CLAP, -1, -1}},
        {7, {CAT_HAT, CAT_OHAT, -1, -1}},            {6, {CAT_CLAP, CAT_SNARE, CAT_PERC, -1}},
        {2, {CAT_OHAT, CAT_HAT, CAT_CYM, -1}},       {9, {CAT_PERC, CAT_TOM, -1, -1}},
        {8, {CAT_LOOP, -1, -1, -1}},                 {4, {CAT_BASS, CAT_MEL, -1, -1}},
        {1, {CAT_MEL, CAT_BASS, CAT_OTHER, -1}},     {3, {CAT_CYM, CAT_OHAT, CAT_FX, -1}},
        {5, {CAT_TOM, CAT_PERC, -1, -1}},            {0, {CAT_VOX, CAT_FX, CAT_OTHER, CAT_MEL}},
    };
    for (int r = 0; r < NSLOT && !random; r++)
        for (int w = 0; w < 4 && role[r].want[w] >= 0 && !full[role[r].slot]; w++)
            if (pack_take(role[r].want[w], role[r].slot)) { full[role[r].slot] = 1; placed++; }
    for (int r = 0; r < NSLOT && !random; r++)   // what the pack has no such sound for: whatever is left
        if (!full[role[r].slot] && pack_take(-1, role[r].slot)) { full[role[r].slot] = 1; placed++; }
    if (!placed) { toast("NO WORKING FILES IN THIS FOLDER"); return; }
    for (int s = 0; s < NSLOT; s++) {
        if (!full[s]) set_slot_raw(s, "");
        song.t[s].start = 0; song.t[s].end = 1000; auto_setup(s);
    }
    browse_close();
    int broken = 0; for (int i = 0; i < pk_n; i++) if (pk_used[i] == 2) broken++;
    char b[64];
    const char *what = random ? "RANDOM" : "KIT";
    if (broken) snprintf(b, sizeof b, "%s: %d OF %d, %d BROKEN SKIPPED", what, placed, pk_n, broken);
    else snprintf(b, sizeof b, "%s: %d OF %d FILES", what, placed, pk_n);
    toast(b);
}
static Uint32 browse_hold; static int browse_rand;   // EDIT is held on a folder since then: 3 s load it (with SHIFT at the press: at random)
#define HOLD_MS 3000
static void browse_tick(Uint32 now) {
    if (!browse_hold || ms_since(now, browse_hold) < HOLD_MS) return;
    browse_hold = 0;
    if (browsing && bcur < bn && bent[bcur].dir) {
        char p[1024]; snprintf(p, sizeof p, "%s/%s", bdir, bent[bcur].name);
        if (strlen(p) < 500) load_pack(p, browse_rand);
    }
}
static void prj_open_req(const char *path);
static char blast[256];                  // the folder the last sound came from: an empty pad's browser opens there
static void browse_key(int k) {
    if (k == K_UP || k == K_DOWN) {
        if (!bn) return;
        bcur = clampi(bcur + (k == K_DOWN ? 1 : -1), 0, bn - 1); if (browse_listen) browse_preview();
    } else if (k == K_LEFT) {
        char *sl = strrchr(bdir, '/');
        if (sl && strcmp(bdir, bmode ? PRJ_DIR : "samples")) {   // never above the root; copy first (browse_open writes bdir)
            char from[200], up[512]; snprintf(from, sizeof from, "%s", sl + 1);
            snprintf(up, sizeof up, "%.*s", (int)(sl - bdir), bdir); browse_open(up);
            for (int i = 0; i < bn; i++) if (!strcmp(bent[i].name, from)) bcur = i; }
    } else if ((k == K_EDIT || k == K_RIGHT) && bn) {
        char p[1024]; snprintf(p, sizeof p, "%s/%s", bdir, bent[bcur].name);
        if (bent[bcur].dir) { if (strlen(p) < 500) browse_open(p); }
        else if (k == K_EDIT && bmode) prj_open_req(p);   // a project: open it (asks first when this one has changes)
        else if (k == K_EDIT && strlen(p) < 256) { push_undo(); set_slot_file(sel, p); norm_new(sel); auto_setup(sel); snprintf(blast, sizeof blast, "%s", bdir); browse_close();
            if (tab == T_GRID && ui_cat < 0 && !perform) cat_open(PC_SOUND); }   // v3: a pad that just got its sound opens on it
    } else if (k == K_OPT) browse_close();
}
static void neighbour_file(int pad, int dir) {
    char d[256]; snprintf(d, sizeof d, "%s", song.t[pad].path);
    char *sl = strrchr(d, '/'); if (!sl) return; *sl = 0;
    static Entry e[2048]; int n = list_dir(d, e, 2048), i;
    for (i = 0; i < n && strcmp(e[i].name, sl + 1); i++);
    for (int j = i + dir; j >= 0 && j < n; j += dir)
        if (!e[j].dir) { char p[1024]; snprintf(p, sizeof p, "%s/%s", d, e[j].name);
            if (strlen(p) >= 256 || idx_bad(p) || !set_slot_raw(pad, p)) continue;   // broken files are stepped over
            norm_new(pad); auto_setup(pad); listen(pad); return; }
}

// ---------- SEQ+INST ----------
enum { L_TRIG, L_NOTE, L_VEL, L_PROB, L_REP, L_SLICE, L_ACC, L_SLD, L_A1, L_A2, L_A3, L_A4, L_N };
static const char *lane_name[L_N] = {"NOTE", "NOT", "VEL", "PRB", "SUB", "SLC", "ACC", "SLD", "A1", "A2", "A3", "A4"};   // short: PAGE, MODE and the lanes share the row
static int is_synth(int s);
static int lane_list(int *l) {           // the lanes this pad has: a sampler slices, a synth does not, ACID has ACCENT and SLIDE
    // 2026-11-24 (user): no SUB lane any more; the roll (TRIPLET / DOTTED / FREE grids) places notes anywhere
    static const int smp_l[] = {L_TRIG, L_VEL, L_PROB, L_SLICE}, gb_l[] = {L_TRIG, L_VEL, L_PROB},   // TRIG holds the note too (2026-11-09)
                     ab_l[] = {L_TRIG, L_VEL, L_PROB, L_ACC, L_SLD};
    static const int sls_l[] = {L_SLICE, L_TRIG, L_VEL, L_PROB};   // a sliced sample: SLC first (2026-11-12, user)
    int ty = song.t[sel].type; const int *src = ty == 2 ? ab_l : ty ? gb_l : track_nslices(sel) > 1 ? sls_l : smp_l; int n = ty == 2 ? 5 : ty ? 3 : track_nslices(sel) > 1 ? 4 : 3;   // UI v2: no SLICE lane without slices (2026-11-25: the counts had kept SUB's place and read past the lists)
    if (song.t[sel].pat[song.t[sel].cur].mode) n = 1;   // ROLL: the roll itself (NOTE) and the automation lanes   // CLOUD: as WAVE
    for (int i = 0; i < n; i++) l[i] = src[i];
    for (int a = 0; a < NAUTO; a++) l[n++] = L_A1 + a;   // automation lanes on every pad
    return n;
}
static int is_auto(int ln) { return ln >= L_A1 && ln <= L_A4; }
static const char *lane_nm(int ln) { return ln == L_TRIG && song.t[sel].type == 0 ? "PITCH" : lane_name[ln]; }   // the sampler's note lane is its PITCH (2026-11-12, user)
static int lane_first(void) { int l[L_N]; lane_list(l); return l[0]; }
static int lane; static int lfo_targets(int s, int *out); static void lfo_tname(int s, int tg, char *b);
static int learning, learn_lane, learn_lfo = -1;   // LEARN: a lane (or LFO learn_lfo, from the MOD page) waits for a knob; EDIT on the panel binds it
static int8_t *aut_bend(void);
static int aut_point_back(void);
static int cur_step(void); static Pattern *cur_pat(int s);
static AutoLane *cur_aut(void) { return &song.aut[sel][song.t[sel].cur][(lane - L_A1) & 3]; }
static int8_t *aut_bend(void) { return song.acurve[sel][song.t[sel].cur][(lane - L_A1) & 3]; }
static int aut_point_back(void) {        // the point the segment under the cursor starts from (-1 none)
    const AutoLane *al = cur_aut(); const Pattern *p = cur_pat(sel); int len = p->len ? p->len : 16, k = cur_step() % len;
    for (int i = 0; i < len; i++) { int j = (k - i + len) % len; if (al->v[j] != AUTO_EMPTY) return j; }
    return -1;
}
static int last_vel[NSLOT], last_prob[NSLOT], last_rep[NSLOT], last_slc[NSLOT];   // the last value set on each lane, +1 (slice +2), 0 none: a new step starts from it (2026-11-12, user)
static int last_note[NSLOT];             // the last note set on a pad's steps, MIDI + 1 (0 none): a new step gets it
static int in_scale(int s, int m) {      // a pitch belongs to the pad's scale
    int pc = ((m - track_key(s)) % 12 + 12) % 12, sc = track_scale(s), n = scale_len(sc);
    for (int k = 0; k < n; k++) if (deg_semis(sc, k) % 12 == pc) return 1;
    return 0;
}
static void note_str(int m, char *b) { snprintf(b, 8, "%s%d", note_name[m % 12], m / 12 - 1); }
static const char *div_name[6] = {"1/32", "1/16", "1/8", "1/4", "1/2", "1/1"};
enum { P_SAMPLE, P_PITCH, P_FINE, P_VOL, P_PAN, P_START, P_END, P_DIR, P_VOICE, P_ROOT, P_SLICES, P_WARP, P_BARS, P_BPM, P_GRAIN, P_FLUX, P_TYPE,
       P_SRC, P_NORM, P_SX, P_AC = P_SX + SX_N, P_CL = P_AC + AP_N, P_CX = P_CL + CP_N, P_AX = P_CX + CX_N, P_WV = P_AX + AX_N, P_SLWAY = P_WV + WP_N, P_WENV, P_WLFO, P_WTGT, P_N };   // P_WENV / P_WLFO: which ENV / LFO WAVE's page shows; P_WTGT: where that LFO goes (the matrix)   // P_WV + k: WAVE's wp[k] (Song.wv, 2026-11-25: in GRAND's place, P_SY / P_GX / P_PT)   // P_AX + AX_*: ACID DEEP (Song.ax)   // P_CX + CX_*: CLOUD DEEP (Song.cx)   // P_SRC: CLOUD's sample; P_CL + CP_*: CLOUD; P_AC + AP_*: ACID; P_SX + SX_*: SAMPLER DEEP
static const char *par_name[P_N] = {"SAMPLE", "PITCH", "FINE", "VOLUME", "PAN", "START", "END", "DIR", "VOICE", "ROOT", "SLICES", "WARP", "BARS", "BPM", "GRAIN", "FLUX", "TYPE", "SOURCE", "NORM", "FADE IN", "FADE OUT", "XFADE",
    "WAVE", "TUNING", "CUTOFF", "RESO", "ENV MOD", "DECAY", "ACCENT", "DIST", "DRIVE", "TONE", "MIX",
    "POSITION", "SPRAY", "SIZE", "DENSITY", "SHIMMER", "MOTION",
    "SIZE RND", "WINDOW", "REVERSE", "PITCH", "FINE", "PITCH RND", "INTERVALS", "SYNC", "TIME RND", "WIDTH", "SCAN", "FREEZE", "ATTACK", "RELEASE", "TONE",
    "SUB", "SUB OCT", "SUB WAVE", "PULSE W", "NOISE", "ATTACK", "RELEASE", "SLIDE", "RANGE", "TRACK", "RESO HP", "BASS", "ACC ATK", "ACC DEC", "CLIP",
    [P_SLWAY] = "METHOD", [P_WENV] = "ENVELOPE", [P_WLFO] = "LFO", [P_WTGT] = "TARGET"};   // P_WV + k: wv_names() fills them from wave.h
__attribute__((constructor)) static void wv_names(void) {   // (before main: every screen reads par_name)             // WAVE's card names: wp_name, shortened where a card is narrow
    static char nb[WP_N][16];
    for (int k = 0; k < WP_N; k++) { const char *n = wp_name(k); if (!n) { par_name[P_WV + k] = ""; continue; }
        if (k < WP_SUB_LEVEL && (k & 15) != WO_ON) n += 2;   // OSC A / B: the page says which ("A POS" -> "POS")
        else if (k >= WP_ENV1 && k < WP_LFO1) n += 5;        // ENV1 ATT -> ATT (the tab says which envelope)
        else if (k >= WP_LFO1 && k < WP_POLY) n += 5;        // LFO2 RATE -> RATE
        else if (k >= WP_MTX && k < WP_MTX + 64) n += (k - WP_MTX) / 4 >= 9 ? 4 : 3;   // M12 AMT -> AMT
        snprintf(nb[k], sizeof nb[k], "%s", n); par_name[P_WV + k] = nb[k]; }
    par_name[P_WV + WP_SUB_LEVEL] = "SUB"; par_name[P_WV + WP_SUB_SHAPE] = "SHAPE"; par_name[P_WV + WP_SUB_OCT] = "OCTAVE"; par_name[P_WV + WP_SUB_PAN] = "SUB PAN";
    par_name[P_WV + WP_SUB_DIRECT] = "SUB DIRECT"; par_name[P_WV + WP_NOISE_LEVEL] = "NOISE"; par_name[P_WV + WP_NOISE_TYPE] = "COLOUR"; par_name[P_WV + WP_NOISE_DIRECT] = "NS DIRECT";
    par_name[P_WV + WP_A + WO_ON] = "OSC A"; par_name[P_WV + WP_B + WO_ON] = "OSC B"; par_name[P_WV + WP_F_TYPE] = "TYPE";
    par_name[P_WV + WP_ARP] = "ARP"; par_name[P_WV + WP_ARP_MODE] = "MODE"; par_name[P_WV + WP_ARP_OCT] = "OCTAVES"; par_name[P_WV + WP_ARP_RATE] = "RATE"; par_name[P_WV + WP_ARP_GATE] = "GATE";
}
static const char *dist_name[4] = {"DIODE", "TRANSISTOR", "TUBE", "FUZZ"};
static const char *type_name[4] = {"SAMPLER", "WAVE", "ACID", "CLOUD"};
static int is_synth(int s) { return song.t[s].type != 0; }   // either synth: no sample, a drawn panel
static int has_sound(int s) { return engine_sample(s) || is_synth(s); }
static const char *slot_name(int s) { const Sample *x = engine_sample(s);   // the pad's own name (SHIFT+X on GRID), else its instrument or sample
    return song.pname[s][0] && (is_synth(s) || x) ? song.pname[s] : is_synth(s) ? type_name[song.t[s].type % 4] : x ? x->name : NULL; }
static const char *warp_name[6] = {"OFF", "BEATS", "REPITCH", "TONES", "TEXTURE", "COMPLEX"};
#define PROWS 9
static int srow = 1, scol = 0, lane;
static int hear_on = 1;                  // the headphones (2026-11-19, user, as in Ableton): a note being put or moved sounds; R2 in STEPS and the roll
static int edit_put;                     // the step under the cursor was put by this EDIT press (its release must not take it away)
static int paint_anchor = -1, paint_val; static int8_t paint_snap[NSTEP];   // rows: -1 name, 0 header(2), 1-2 steps(16), 3+ params(2)
static int spage;                        // the steps are shown 32 at a time
static int cur_step(void) { return spage * 32 + (srow - 1) * 16 + scol; }
static void set_step(int st) { spage = st / 32; srow = 1 + (st % 32) / 16; scol = st % 16; }
static int last_page(void) { int l = song.t[sel].pat[song.t[sel].cur].len; return (l > 0 ? l - 1 : 0) / 32; }
static int last_page(void);
static int prbing; static void prb_open(int st); static void prb_key(int k, int down); static void draw_prb(void);   // PRB modes (prb.h)
static int chording; static void chord_open(void); static void chord_key(int k, int down); static void draw_chords(void); static void chord_leave(void);   // CHORDS (chords.h)
static int rolling; static const int rl_d32[6];   // the piano roll (below, before key_event)
static int picking; static void pick_open(void); static void pick_close(void); static void draw_pick(void); static void pick_key(int k, int down);
static int randing; static int presetting; static void preset_open(void); static void preset_close(int keep);
static void roll_mode(void); static void roll_open(void); static void roll_del(Pattern *p, int i);
static int roll_add(Pattern *p, int start, int len, int note, int vel); static void draw_roll(void); static void roll_leave(void); static int roll_base(int s); static int roll_ticks(const Pattern *p);
static void pat_switch(int np); static void pat_open(void);
static int seq_cols(int r) { return r < 0 ? 2 : r == 1 || r == 2 ? 16 : r == 0 ? (is_auto(lane) ? 5 : 4) : 2; }   // row -1: the name and the pattern; row 0: LENGTH STEP PAGE MODE

static void wave_new(int s);             // a pad becomes WAVE (below, with the presets)
static int wv_env, wv_lfo;               // WAVE: the envelope (0..2) and LFO (0..3) the ENV and LFO pages show
static int wtbing;                       // WAVE's TABLE browser is open (wvui.h)
static int learn_wv = -1;                // LEARN for WAVE's matrix: 0..15 the slot whose DEST is pointed at, 16 + j: LFO j's TARGET (a new slot)
static int wv_is_pair(int p) {           // a WAVE switch (two states): a tap flips it, UP / DOWN pick a side
    int k = p - P_WV; if (p < P_WV || p >= P_WV + WP_N) return 0;
    return k == WP_A + WO_ON || k == WP_B + WO_ON || k == WP_SUB_DIRECT || k == WP_NOISE_DIRECT || k == WP_LEGATO || k == WP_ARP || k == WP_NOISE_TYPE || k == WP_QUALITY || k == WP_SUB_OCT;
}
static int wv_is_list(int p) {           // a WAVE value that is a name from a long list (a card shows the name, big)
    int k = p - P_WV; if (p < P_WV || p >= P_WV + WP_N) return 0;
    int o = k < WP_SUB_LEVEL ? k & 15 : -1, l = k >= WP_LFO1 && k < WP_POLY ? (k - WP_LFO1) % WL_N : -1;
    return o == WO_TABLE || o == WO_WARP || k == WP_F_TYPE || l == WL_SHAPE || l == WL_SYNC || k == WP_ARP_RATE;
}
static const char *const slway_name[3] = {"EQUAL", "TRANSIENTS", "BY HAND"};   // P_SLWAY: how SLICES cuts (A09)
static int par_visible(int p) {          // hide what the current mode does not use
    const Track *t = &song.t[sel];
    int warp = track_bpm(sel) > 0 ? t->warp : 0;   // without a tempo the engine plays the sample plainly
    switch (p) {
    case P_DIR: return warp < 3;                   // TONES/TEXTURE/COMPLEX play forward only
    case P_GRAIN: return warp == 3 || warp == 4;
    case P_FLUX: return warp == 4;
    case P_BARS: case P_BPM: return t->warp >= 2 || t->slices;
    }
    return 1;
}
// Fixed places: the right column is the WARP block, its parameters only appear below WARP, nothing moves.
#define WARP_ROWS 5
static const int8_t par_col_sampler[2][PROWS] = {
    {P_SAMPLE, P_PITCH, P_FINE, P_VOL, P_PAN, P_VOICE, P_ROOT, P_SLICES, P_DIR},
    {P_WARP, P_BARS, P_BPM, P_GRAIN, P_FLUX, -1, -1, -1, -1},
};
#define par_col par_col_sampler
// The 1.2 panels are lists of controls (the cursor and LFO learn walk them; the v2 SOUND pages draw the cards).
// srow 3 = the panel, gb_cur = the control. WAVE (2026-11-25) has no drawn panel: its list is its main cards.
enum { GB_KNOBC, GB_WAVE, GB_SLIDER, GB_TOGGLE, GB_SELECT };
typedef struct { int16_t p, x, y; int8_t kind, r; } GbCtl;
static const GbCtl gb_ctl[] = {
    {P_SAMPLE, 208, 273, GB_SELECT, 0},
    {P_WV + WP_A + WO_TABLE, 80, 420, GB_SELECT, 0}, {P_WV + WP_A + WO_POS, 240, 420, GB_KNOBC, 26}, {P_WV + WP_B + WO_POS, 400, 420, GB_KNOBC, 26},
    {P_WV + WP_F_CUT, 560, 420, GB_KNOBC, 26}, {P_WV + WP_F_RES, 720, 420, GB_KNOBC, 26}, {P_WV + WP_F_ENV, 880, 420, GB_KNOBC, 26},
    {P_WV + WP_ENV1 + WE_ATT, 80, 600, GB_KNOBC, 22}, {P_WV + WP_ENV1 + WE_DEC, 240, 600, GB_KNOBC, 22}, {P_WV + WP_ENV1 + WE_SUS, 400, 600, GB_KNOBC, 22},
    {P_WV + WP_ENV1 + WE_REL, 560, 600, GB_KNOBC, 22}, {P_WV + WP_GLIDE, 720, 600, GB_KNOBC, 22}, {P_VOL, 880, 600, GB_KNOBC, 22},
};
#define GB_N_GB ((int)(sizeof gb_ctl / sizeof gb_ctl[0]))
// ACID's panel: OSC | FILTER (the five knobs of the original) | DISTORTION (black, with its lamp) | OUTPUT
#define AB_SX0 16
#define AB_SX1 (AB_SX0 + 154)               // FILTER
#define AB_SX2 (AB_SX1 + 424)               // DISTORTION
#define AB_SX3 (AB_SX2 + 284)               // OUTPUT, to 1008
static const GbCtl ab_ctl[] = {
    {P_SAMPLE, 208, 273, GB_SELECT, 0},
    {P_AC + AP_WAVE, AB_SX0 + 75, 384, GB_TOGGLE, 0}, {P_AC + AP_TUNE, AB_SX0 + 75, 560, GB_KNOBC, 28},
    {P_AC + AP_CUTOFF, AB_SX1 + 75, 476, GB_KNOBC, 30}, {P_AC + AP_RESO, AB_SX1 + 210, 476, GB_KNOBC, 30}, {P_AC + AP_ENVMOD, AB_SX1 + 345, 476, GB_KNOBC, 30},
    {P_AC + AP_DECAY, AB_SX1 + 140, 608, GB_KNOBC, 26}, {P_AC + AP_ACCENT, AB_SX1 + 280, 608, GB_KNOBC, 26},
    {P_AC + AP_DTYPE, AB_SX2 + 74, 392, GB_WAVE, 0},
    {P_AC + AP_DRIVE, AB_SX2 + 50, 596, GB_KNOBC, 24}, {P_AC + AP_DTONE, AB_SX2 + 140, 596, GB_KNOBC, 24}, {P_AC + AP_DMIX, AB_SX2 + 230, 596, GB_KNOBC, 24},
    {P_VOL, AB_SX3 + 65, 476, GB_KNOBC, 28},
};
// The sampler's panel (rack graphite): the sample on a wide screen, then TUNE, PLAY, WARP, OUTPUT
#define SM_S1 290                           // PLAY
#define SM_S2 534                           // WARP
#define SM_S3 876                           // OUTPUT, to 1008
static const GbCtl sm_ctl[] = {
    {P_SAMPLE, 208, 273, GB_SELECT, 0},
    {P_PITCH, 82, 572, GB_KNOBC, 28}, {P_FINE, 210, 572, GB_KNOBC, 28}, {P_ROOT, 150, 654, GB_SELECT, 0},
    {P_VOICE, SM_S1 + 52, 566, GB_TOGGLE, 0}, {P_DIR, SM_S1 + 168, 566, GB_TOGGLE, 0}, {P_SLICES, SM_S1 + 120, 654, GB_SELECT, 0},
    {P_WARP, SM_S2 + 70, 590, GB_WAVE, 0}, {P_GRAIN, SM_S2 + 196, 560, GB_KNOBC, 22}, {P_FLUX, SM_S2 + 288, 560, GB_KNOBC, 22},
    {P_BARS, SM_S2 + 242, 634, GB_SELECT, 0}, {P_BPM, SM_S2 + 242, 664, GB_SELECT, 0},
    {P_VOL, SM_S3 + 66, 552, GB_KNOBC, 24}, {P_PAN, SM_S3 + 66, 642, GB_KNOBC, 18},
    {P_NORM, 912, 308, GB_SELECT, 0},        // NORMALIZE: a plate in the SAMPLE section's head
};
// ACID DEEP (Y on ACID's panel): SUB, VOICE, FILTER, ACCENT, OUTPUT
#define AD_S0 16
#define AD_S1 216                           // VOICE
#define AD_S2 466                           // FILTER
#define AD_S3 716                           // ACCENT
#define AD_S4 866                           // OUTPUT, to 1008
static const GbCtl ad_ctl[] = {
    {P_SAMPLE, 208, 273, GB_SELECT, 0},
    {P_AX + AX_SUB, AD_S0 + 56, 396, GB_KNOBC, 24}, {P_AX + AX_SUBOCT, AD_S0 + 150, 396, GB_TOGGLE, 0}, {P_AX + AX_SUBWAVE, AD_S0 + 100, 560, GB_WAVE, 0},
    {P_AX + AX_PW, AD_S1 + 66, 396, GB_KNOBC, 24}, {P_AX + AX_NOISE, AD_S1 + 186, 396, GB_KNOBC, 24},
    {P_AX + AX_VATK, AD_S1 + 66, 518, GB_KNOBC, 24}, {P_AX + AX_VREL, AD_S1 + 186, 518, GB_KNOBC, 24}, {P_AX + AX_SLIDE, AD_S1 + 126, 632, GB_KNOBC, 22},
    {P_AX + AX_RANGE, AD_S2 + 66, 396, GB_KNOBC, 24}, {P_AX + AX_TRACK, AD_S2 + 186, 396, GB_KNOBC, 24},
    {P_AX + AX_RESHP, AD_S2 + 66, 518, GB_KNOBC, 24}, {P_AX + AX_BASS, AD_S2 + 186, 518, GB_KNOBC, 24},
    {P_AX + AX_ACCATK, AD_S3 + 75, 396, GB_KNOBC, 24}, {P_AX + AX_ACCDEC, AD_S3 + 75, 518, GB_KNOBC, 24},
    {P_AX + AX_CLIP, AD_S4 + 71, 396, GB_KNOBC, 24},
};
// CLOUD's panel: SOURCE, the sample with the cloud on it, six knobs and VOLUME in a row
static const GbCtl cl_ctl[] = {
    {P_SAMPLE, 208, 273, GB_SELECT, 0}, {P_SRC, 208, 318, GB_SELECT, 0},
    {P_CL + CP_POS, 16 + 71, 590, GB_KNOBC, 30}, {P_CL + CP_SPRAY, 16 + 212, 590, GB_KNOBC, 30}, {P_CL + CP_SIZE, 16 + 354, 590, GB_KNOBC, 30},
    {P_CL + CP_DENSITY, 16 + 496, 590, GB_KNOBC, 30}, {P_CL + CP_SHIMMER, 16 + 638, 590, GB_KNOBC, 30}, {P_CL + CP_MOTION, 16 + 779, 590, GB_KNOBC, 30},
    {P_VOL, 16 + 921, 590, GB_KNOBC, 26},
    {P_NORM, 944, 317, GB_SELECT, 0},        // NORMALIZE: a plate at the end of the SOURCE row
};
// CLOUD DEEP (Y on CLOUD's panel): five columns GRAIN PITCH TIME SPACE SHAPE
#define CD_X(c) (16 + 198 * (c) + 99)
static const GbCtl cd_ctl[] = {
    {P_SAMPLE, 208, 273, GB_SELECT, 0},
    {P_CX + CX_SIZERND, CD_X(0), 394, GB_KNOBC, 20}, {P_CX + CX_WINDOW, CD_X(0), 482, GB_KNOBC, 20}, {P_CX + CX_REVERSE, CD_X(0), 570, GB_KNOBC, 20},
    {P_CX + CX_PITCH, CD_X(1), 394, GB_KNOBC, 20}, {P_CX + CX_FINE, CD_X(1), 482, GB_KNOBC, 20}, {P_CX + CX_PITCHRND, CD_X(1), 570, GB_KNOBC, 20},
    {P_CX + CX_SHMODE, CD_X(1), 652, GB_SELECT, 0},
    {P_CX + CX_SYNC, CD_X(2), 394, GB_KNOBC, 20}, {P_CX + CX_TIMERND, CD_X(2), 482, GB_KNOBC, 20},
    {P_CX + CX_WIDTH, CD_X(3), 394, GB_KNOBC, 20}, {P_CX + CX_SCAN, CD_X(3), 482, GB_KNOBC, 20}, {P_CX + CX_FREEZE, CD_X(3), 600, GB_TOGGLE, 0},
    {P_CX + CX_ATTACK, CD_X(4), 394, GB_KNOBC, 20}, {P_CX + CX_RELEASE, CD_X(4), 482, GB_KNOBC, 20}, {P_CX + CX_TONE, CD_X(4), 570, GB_KNOBC, 20},
};
// SAMPLER DEEP (Y on the sampler's panel, 2026-11-11): the fades
#define SD_X(c) (16 + 332 * (c) + 164)
static const GbCtl sd_ctl[] = {
    {P_SAMPLE, 208, 273, GB_SELECT, 0},
    {P_SX + SX_FIN, SD_X(0), 578, GB_KNOBC, 32}, {P_SX + SX_FOUT, SD_X(1), 578, GB_KNOBC, 32}, {P_SX + SX_XFADE, SD_X(2), 578, GB_KNOBC, 32},
};
static int cl_deep;                       // CLOUD shows CLOUD DEEP instead of its panel
static const GbCtl *pctl(int *n) {        // the controls of the open pad's panel
    if (song.t[sel].type == 0 && cl_deep) { *n = (int)(sizeof sd_ctl / sizeof sd_ctl[0]); return sd_ctl; }
    if (song.t[sel].type == 3 && cl_deep) { *n = (int)(sizeof cd_ctl / sizeof cd_ctl[0]); return cd_ctl; }
    if (song.t[sel].type == 3) { *n = (int)(sizeof cl_ctl / sizeof cl_ctl[0]); return cl_ctl; }
    if (song.t[sel].type == 2 && cl_deep) { *n = (int)(sizeof ad_ctl / sizeof ad_ctl[0]); return ad_ctl; }
    if (song.t[sel].type == 2) { *n = (int)(sizeof ab_ctl / sizeof ab_ctl[0]); return ab_ctl; }
    if (song.t[sel].type == 0) { *n = (int)(sizeof sm_ctl / sizeof sm_ctl[0]); return sm_ctl; }
    *n = GB_N_GB; return gb_ctl;
}
static int pn(void) { int n; pctl(&n); return n; }
#define GB_N pn()
static int gb_cur, gb_heard;            // gb_heard: B holds a note on the panel
#define GB_TAG 300                       // the engine tag of that note
static void gb_audition(void) {          // B on the panel: a note while B is held, again after every change
    if (engine_playing() && song.t[sel].on) return;   // the pad's clip plays: the change is heard in it, no extra note
    engine_release_key(GB_TAG); engine_trig_key(sel, 1.f, song.lnote[sel], -1, GB_TAG); flash[sel] = SDL_GetTicks(); gb_heard = 1;
}
static int gb_find(int p) { int n; const GbCtl *c = pctl(&n); for (int i = 0; i < n; i++) if (c[i].p == p) return i; return -1; }
static int gb_nav(int k) {               // the nearest control that way, -1 none
    int n; const GbCtl *c = pctl(&n); if (gb_cur >= n) gb_cur = 0;
    int cx = c[gb_cur].x, cy = c[gb_cur].y, best = -1, bs = 1 << 30;
    for (int i = 0; i < n; i++) {
        if (i == gb_cur || !par_visible(c[i].p)) continue;
        int dx = c[i].x - cx, dy = c[i].y - cy, along, side;
        if (k == K_RIGHT) { along = dx; side = dy; } else if (k == K_LEFT) { along = -dx; side = dy; }
        else if (k == K_DOWN) { along = dy; side = dx; } else { along = -dy; side = dx; }
        if (along <= 12) continue;
        int sc = along + 3 * abs(side);
        if (sc < bs) { bs = sc; best = i; }
    }
    return best;
}
static int par_at(int c, int r) {        // parameter at this place, -1 if empty or hidden
    if (c < 0 || c > 1 || r < 0 || r >= PROWS) return -1;
    int p = par_col[c][r];
    return p >= 0 && par_visible(p) ? p : -1;
}
static int par_row_near(int c, int r) {  // nearest filled row at or above r
    while (r > 0 && par_at(c, r) < 0) r--;
    return r;
}
static int v2_sound(void); static int sound_par(void); static void sound_sync(void); static void sound_move(int k); static int sound_edit_arrow(int k);   // sound.h
static int cur_par(void) { if (v2_sound()) return sound_par();   // UI v2: the card under the cursor
    if (srow >= 3) { int n; const GbCtl *c = pctl(&n); return c[clampi(gb_cur, 0, n - 1)].p; } return par_at(scol, par_row_near(scol, srow - 3)); }
static void par_focus(int p) {
    { int i = gb_find(p); if (i >= 0) { if (par_visible(p)) { gb_cur = i; srow = 3; scol = 0; } return; } }   // every pad has a panel; a hidden control keeps the cursor where it is
    for (int c = 0; c < 2; c++) for (int r = 0; r < PROWS; r++) if (par_col[c][r] == p) { scol = c; srow = 3 + par_row_near(c, r); return; }
}
static int v2_steps(void) { return ui_cat == PC_PATTERN && tab == T_SEQ && !perform && !cur_pat(sel)->mode && !is_auto(lane); }
static void seq_clamp(void) {            // slot change or undo may hide the parameter under the cursor
    if (ui_cat == PC_SOUND && tab == T_SEQ) sound_sync();   // SOUND (UI v2): its page and card exist
    if (spage > last_page()) spage = last_page();   // first: the panel returns early below
    if (v2_steps() && (srow < 1 || srow > 2)) { srow = 1; scol = 0; }   // STEPS (UI v2): only the steps
    if (v2_steps() && cur_step() >= cur_pat(sel)->len) set_step(cur_pat(sel)->len ? cur_pat(sel)->len - 1 : 0);
    if (srow >= 1 && srow <= 2 && cur_pat(sel)->mode && !is_auto(lane)) { srow = 1; scol = 0; }   // ROLL: the mini roll is one place (its automation lanes are steps)
    if (srow >= 3) { srow = 3; scol = 0; gb_cur = clampi(gb_cur, 0, GB_N - 1);   // scol has no meaning on the panel
        { int n; const GbCtl *c = pctl(&n); if (!par_visible(c[gb_cur].p)) gb_cur = gb_find(P_WARP) >= 0 ? gb_find(P_WARP) : 0; }   // a control the mode hides
        return; }
    if (srow >= 3) srow = 3 + par_row_near(scol, srow - 3);
    if (spage > last_page()) spage = last_page();
}

// NORMALIZE (2026-11-11, user: "in case the sample is too quiet"): per pad, for the engines that play a sample
// (SAMPLER, CLOUD). The file is not touched: the engine multiplies the pad's sound (Sample.npk / nld, src/sample.c).
// A sample the user loads gets PEAK; pads of older projects stay OFF and sound as they did.
static const char *const norm_name[3] = {"OFF", "PEAK", "LOUD"};
static float norm_gain(int s) {          // what the pad's sample is multiplied by now
    const Sample *x = engine_sample(s); int m = song.mix.pad[s].spare[PM_NORM];
    float g = x && m > 0 ? (m == 1 ? x->npk : x->nld) : 1.f; return g > 0 ? g : 1.f;
}
static void wv_time_str(float sec, char *b, int n) { if (sec < 0.1f) snprintf(b, n, "%.1fMS", sec * 1000); else if (sec < 1) snprintf(b, n, "%dMS", (int)(sec * 1000 + 0.5f)); else snprintf(b, n, "%.2fS", sec); }
static void wv_par_str(int pad, int k, int v, char *b, int n) {   // WAVE's values as the cards read them
    const int8_t *wp = song.wv[pad]; int lf = k >= WP_LFO1 && k < WP_POLY ? (k - WP_LFO1) % WL_N : -1, o = k < WP_SUB_LEVEL ? k & 15 : -1;
    if (k >= WP_ENV1 && k < WP_LFO1 && (k - WP_ENV1) % WE_N != WE_SUS && (k - WP_ENV1) % WE_N != WE_CURVE) wv_time_str(wv_time(v), b, n);
    else if (k == WP_F_CUT) { float hz = wv_cut_hz(v); if (hz < 1000) snprintf(b, n, "%dHZ", (int)hz); else snprintf(b, n, "%.1fKHZ", hz / 1000); }
    else if (k == WP_GLIDE) { if (!v) snprintf(b, n, "OFF"); else wv_time_str(wv_glide_s(v), b, n); }
    else if (lf == WL_RATE) { int sy = wp[k - WL_RATE + WL_SYNC]; if (sy > 0 && sy < WV_NSYNC) snprintf(b, n, "%s", wv_sync_names[sy]); else { float hz = wv_lfo_hz(v); snprintf(b, n, hz < 1 ? "%.2fHZ" : "%.1fHZ", hz); } }
    else if (k == WP_ARP_RATE) snprintf(b, n, "%s", div_label[v % NDIV]);
    else if (o == WO_SEMI) snprintf(b, n, v ? "%+d ST" : "0 ST", v);
    else if (o == WO_POS || o == WO_LEVEL || o == WO_DET || o == WO_WIDTH || o == WO_WAMT || o == WO_RAND || k == WP_F_DRIVE || k == WP_F_KEY || k == WP_SUB_LEVEL || k == WP_NOISE_LEVEL || k == WP_VELSENS) snprintf(b, n, "%d%%", v);
    else if (k == WP_F_ENV || k == WP_F_RES || k == WP_MASTER) snprintf(b, n, k == WP_F_ENV && v ? "%+d" : "%d", v);
    else wp_str(k, v, b, n, (const WaveStore *)engine_wstore());
}
static void par_str(int pad, int p, char *b, int n) {
    Track *t = &song.t[pad]; const Sample *s = engine_sample(pad);
    if (p == P_SLWAY) { snprintf(b, n, "%s", t->slices ? slway_name[t->slices == 5 ? 1 : t->slices == 6 ? 2 : 0] : "--"); return; }   // past P_WV
    switch (p) {
    case P_SAMPLE: snprintf(b, n, "%.20s", is_synth(pad) ? type_name[t->type % 4] : s ? s->name : "--"); break;
    case P_PITCH: snprintf(b, n, t->pitch ? "%+d" : "0", t->pitch); break;
    case P_FINE: snprintf(b, n, t->fine ? "%+d" : "0", t->fine); break;
    case P_VOL: snprintf(b, n, "%d", t->vol); break;
    case P_PAN: snprintf(b, n, t->pan ? "%c%d" : "C", t->pan < 0 ? 'L' : 'R', abs(t->pan)); break;
    case P_START: snprintf(b, n, "%d.%d%%", t->start / 10, t->start % 10); break;
    case P_END: snprintf(b, n, "%d.%d%%", t->end / 10, t->end % 10); break;
    case P_DIR: snprintf(b, n, t->rev ? "REV" : "FWD"); break;
    case P_VOICE: snprintf(b, n, t->voice ? "POLY" : is_synth(pad) ? "MONO" : "GATE"); break;
    case P_TYPE: snprintf(b, n, "%s", type_name[t->type % 4]); break;
    case P_SRC: snprintf(b, n, "%.20s", s ? s->name : "NO SAMPLE"); break;
    case P_NORM: snprintf(b, n, "%s", norm_name[song.mix.pad[pad].spare[PM_NORM] % 3]); break;
    case P_SX + SX_FIN: case P_SX + SX_FOUT: case P_SX + SX_XFADE: { int v = song.smx[pad][p - P_SX]; float ms = v * v / 5.f;
        if (!v) snprintf(b, n, "OFF"); else if (ms < 10) snprintf(b, n, "%.1f MS", ms); else if (ms < 1000) snprintf(b, n, "%d MS", (int)ms); else snprintf(b, n, "%.1f S", ms / 1000); break; }
    case P_ROOT: { int r = track_root(pad);
        if (t->root >= 0) snprintf(b, n, "%s", note_name[t->root]);
        else if (s && s->drum) snprintf(b, n, "AUTO DRUMS");
        else if (s && s->root >= 0) snprintf(b, n, "AUTO %s%s", note_name[s->root], s->mode == 2 ? "m" : "");   // the sample's own key; a minor one fits a major project as its relative
        else snprintf(b, n, "AUTO %s", r >= 0 ? note_name[r] : "--");
        break; }
    case P_SLICES: if (!t->slices) snprintf(b, n, "OFF"); else snprintf(b, n, "%d", track_nslices(pad)); break;   // the count; the way: P_SLWAY
    case P_WARP: snprintf(b, n, "%s", warp_name[t->warp % 6]); break;
    case P_GRAIN: if (t->warp < 3 || t->warp > 4) snprintf(b, n, "--"); else if (t->grain) snprintf(b, n, "%dMS", t->grain);
        else snprintf(b, n, "AUTO %dMS", t->warp == 3 ? 30 : 90); break;
    case P_FLUX: if (t->warp != 4) snprintf(b, n, "--"); else snprintf(b, n, "%d%%", t->flux ? t->flux : 40); break;
    case P_BPM: { float bpm = track_bpm(pad);
        if (t->bpm) snprintf(b, n, "%d", t->bpm); else if (bpm > 0) snprintf(b, n, "AUTO %d", (int)(bpm + 0.5f));
        else snprintf(b, n, "AUTO --"); break; }
    case P_BARS: { float bpm = track_bpm(pad);
        if (bpm > 0) snprintf(b, n, "%s%d", t->bars ? "" : "AUTO ", track_bars(pad));
        else snprintf(b, n, t->bars ? "%d" : "AUTO", t->bars); break; }
    }
    if (p == P_WENV) { snprintf(b, n, "ENV%d", wv_env + 1); return; } if (p == P_WLFO) { snprintf(b, n, "LFO%d", wv_lfo + 1); return; }
    if (p == P_WTGT) {                   // where the LFO goes: its matrix slots' destinations
        const int8_t *wp = song.wv[pad]; int k = 0; b[0] = 0;
        for (int i = 0; i < WM_SLOTS; i++) { const int8_t *m = wp + WP_MTX + 4 * i; if (m[WM_SRC] != WS_LFO1 + wv_lfo || !m[WM_DST] || !m[WM_AMT]) continue;
            int l = (int)strlen(b); snprintf(b + l, n - l, "%s%s", k++ ? ", " : "", wp_name(m[WM_DST] - 1)); }
        if (!k) snprintf(b, n, "NONE: EDIT"); return; }
    if (p >= P_WV && p < P_SLWAY) { wv_par_str(pad, p - P_WV, song.wv[pad][p - P_WV], b, n); return; }
    if (p >= P_AX) {                     // ACID DEEP
        const int8_t *ax = song.ax[pad]; int k = p - P_AX, v = ax[k];
        static const char *const sw[3] = {"SQUARE", "SINE", "TRIANGLE"};
        switch (k) {
        case AX_SUB: case AX_NOISE: case AX_BASS: case AX_CLIP: case AX_TRACK: if (v) snprintf(b, n, "%d%%", v); else snprintf(b, n, "OFF"); break;
        case AX_SUBOCT: snprintf(b, n, v ? "-2 OCT" : "-1 OCT"); break;
        case AX_SUBWAVE: snprintf(b, n, "%s", sw[v % 3]); break;
        case AX_PW: if (!song.t[pad].ap[AP_WAVE]) snprintf(b, n, "SQUARE ONLY"); else snprintf(b, n, "%d%%", 50 + (v - 50) * 8 / 10); break;
        case AX_VATK: snprintf(b, n, "%.1fMS", ax_vatk_s(ax) * 1000); break;
        case AX_VREL: snprintf(b, n, "%.1fMS", ax_vrel_s(ax) * 1000); break;
        case AX_SLIDE: snprintf(b, n, "%dMS", (int)(ax_slide_s(ax) * 3000 + 0.5f)); break;
        case AX_ACCATK: snprintf(b, n, "%dMS", (int)(ax_accatk_s(ax) * 1000 + 0.5f)); break;
        case AX_ACCDEC: snprintf(b, n, "%dMS", (int)(ax_accdec_s(ax) * 1000 + 0.5f)); break;
        case AX_RANGE: snprintf(b, n, v == 50 ? "0 OCT" : "%+.1f OCT", (v - 50) / 50.f * 1.5f); break;
        case AX_RESHP: snprintf(b, n, "%dHZ", (int)(ax_reshp_hz(ax) + 0.5f)); break;
        default: snprintf(b, n, "%d", v);
        }
        return;
    }
    if (p >= P_CX) {
        int k = p - P_CX, v = song.cx[pad][k];
        static const char *const wn[5] = {"HANN", "TRIANGLE", "TUKEY", "PERC", "REV PERC"}, *const im[3] = {"OCT+5TH", "OCTAVES", "SCALE"},
                          *const sy[5] = {"FREE", "1/4", "1/8", "1/16", "1/32"}, *const sc[3] = {"FWD", "REV", "PING-PONG"};
        switch (k) {
        case CX_WINDOW: snprintf(b, n, "%s", wn[v % 5]); break;
        case CX_SHMODE: snprintf(b, n, "%s", im[v % 3]); break;
        case CX_SYNC: snprintf(b, n, "%s", sy[v % 5]); break;
        case CX_SCAN: snprintf(b, n, "%s", sc[v % 3]); break;
        case CX_FREEZE: snprintf(b, n, v ? "ON" : "OFF"); break;
        case CX_PITCH: snprintf(b, n, v ? "%+d ST" : "0 ST", v); break;
        case CX_FINE: snprintf(b, n, v ? "%+d CT" : "0 CT", v); break;
        case CX_PITCHRND: snprintf(b, n, "+-%d CT", v); break;
        case CX_WIDTH: if (v) snprintf(b, n, "%d", v); else snprintf(b, n, "AUTO (SPRAY)"); break;
        case CX_ATTACK: case CX_RELEASE: { float sec = k == CX_ATTACK ? cx_attack_s(v) : cx_release_s(v);
            if (sec < 1) snprintf(b, n, "%dMS", (int)(sec * 1000 + 0.5f)); else snprintf(b, n, "%.1fS", sec); break; }
        case CX_TONE: if (!v) snprintf(b, n, "FLAT"); else snprintf(b, n, v < 0 ? "DARK %d" : "BRIGHT %d", abs(v)); break;
        default: snprintf(b, n, "%d%%", v); break;   // SIZE RND, REVERSE, TIME RND
        }
        return;
    }
    if (p >= P_CL) {
        int k = p - P_CL, v = t->cp[k];
        if (k == CP_SIZE) snprintf(b, n, "%dMS", (int)(cl_size_s(v) * 1000 + 0.5f));
        else if (k == CP_DENSITY) snprintf(b, n, "%.1f/S", cl_density(v));
        else if (k == CP_MOTION) { if (!v) snprintf(b, n, "STILL"); else snprintf(b, n, "%.1f BARS", cl_motion_bars(v)); }
        else snprintf(b, n, "%d", v);
        return;
    }
    if (p >= P_AC) {
        int k = p - P_AC, v = t->ap[k];
        if (k == AP_WAVE) snprintf(b, n, v ? "SQUARE" : "SAW");
        else if (k == AP_TUNE) snprintf(b, n, v ? "%+d" : "0", v);
        else if (k == AP_DTYPE) snprintf(b, n, "%s", dist_name[v & 3]);
        else if (k == AP_DECAY) snprintf(b, n, "%dMS", 200 + v * 20);
        else snprintf(b, n, "%d", v);
        return;
    }
}
#include "params.h"
static void par_adjust(int pad, int p, int d, int big) {
    Track *t = &song.t[pad];
    if (p == P_SAMPLE) return;            // v3: the engine is switched on the section strip (EDIT held on SOUND), never by an arrow here
    if (p == P_TYPE) {
        int nt = ((t->type + d) % 4 + 4) % 4; if (nt == t->type) return;   // SAMPLER, WAVE, ACID, CLOUD
        edit_begin(); t->type = (int8_t)nt;
        if (nt == 1) wave_new(pad);       // the first time: a sound that plays at once
        if (nt == 2 && t->ap[AP_MARK] != AP_INIT) acid_defaults(t->ap);
        if (nt == 3 && t->cp[CP_MARK] != CP_INIT) cl_defaults(t->cp);
        return;
    }
    if (p == P_SRC) { if (!big) { edit_begin(); neighbour_file(pad, d); } return; }   // CLOUD's sample: the next file
    if (p == P_NORM) { edit_begin(); int8_t *v = &song.mix.pad[pad].spare[PM_NORM]; *v = (int8_t)((*v + (d > 0 ? 1 : 2)) % 3); return; }
    edit_begin();
    if (p == P_SLWAY) { int lo, hi, st, bg; par_limits(pad, p, &lo, &hi, &st, &bg); int v = par_get(pad, p) + d; par_put(pad, p, v > hi ? lo : v < lo ? hi : v);   // a short list: round
        if (d > 0 && !song.cut[pad].n && par_get(pad, p) == 1) toast("BY HAND: EDIT ON THE PICTURE MAKES THE POINTS"); return; }
    if (p == P_SLICES && (t->slices == 5 || t->slices == 6)) {   // a count on TRANSIENTS / BY HAND: back to EQUAL slices
        int k = track_nslices(pad), e = k <= 4 ? 1 : k <= 8 ? 2 : k <= 16 ? 3 : 4; t->slices = (int8_t)clampi(e + (d > 0) - (d < 0), 1, 4); relayout(pad); toast("EQUAL SLICES"); return; }
    switch (p) {                          // what is not a plain number in a range
    case P_DIR: t->rev = !t->rev; return;
    case P_VOICE: t->voice = !t->voice; if (t->type == 1) song.wv[pad][WP_POLY] = t->voice ? WV_POLY : 1; return;   // WAVE: VOICES 8 or MONO
    case P_GRAIN: { int g = t->grain ? t->grain : t->warp == 4 ? 90 : 30; t->grain = clampi(g + d * (big ? 10 : 1), 5, 120); return; }
    case P_FLUX: { int f = t->flux ? t->flux : 40; t->flux = clampi(f + d * (big ? 10 : 1), 1, 100); return; }
    case P_BPM: { int cur = t->bpm ? t->bpm : (int)(track_bpm(pad) + 0.5f);
        if (!cur) cur = song.tempo;
        int nb = t->bpm ? cur + d * (big ? 10 : 1) : cur;   // first touch takes the detected value
        t->bpm = nb < 40 ? 0 : clampi(nb, 40, 300); relayout(pad); return; }
    case P_BARS: { static const int bv[6] = {0, 1, 2, 4, 8, 16}; int i = 0;
        while (i < 5 && bv[i] != t->bars) i++;
        t->bars = bv[clampi(i + d, 0, 5)]; relayout(pad); return; }
    }
    int lo, hi, st, bg; par_limits(pad, p, &lo, &hi, &st, &bg);
    par_put(pad, p, clampi(par_get(pad, p) + d * (big ? bg : st), lo, hi));
}
static void par_default(int pad, int p) {
    Track *t = &song.t[pad]; edit_begin();
    switch (p) {
    case P_NORM: song.mix.pad[pad].spare[PM_NORM] = 1; break;
    case P_SX + SX_FIN: case P_SX + SX_FOUT: case P_SX + SX_XFADE: song.smx[pad][p - P_SX] = 0; break;
    case P_PITCH: t->pitch = 0; break; case P_FINE: t->fine = 0; break; case P_VOL: t->vol = 75; break;
    case P_PAN: t->pan = 0; break; case P_START: t->start = 0; break; case P_END: t->end = 1000; break;
    case P_DIR: t->rev = 0; break; case P_VOICE: t->voice = 0; break;
    case P_ROOT: t->root = -1; break; case P_SLICES: case P_SLWAY: t->slices = 0; relayout(pad); break; case P_WARP: t->warp = 0; break;
    case P_BARS: t->bars = 0; relayout(pad); break;
    case P_BPM: t->bpm = 0; relayout(pad); break;
    case P_GRAIN: t->grain = 0; break; case P_FLUX: t->flux = 0; break;
    case P_SAMPLE: if (is_synth(pad)) t->type = 0; else set_slot_file(pad, ""); break;   // a synth goes back to the sampler
    case P_TYPE: t->type = 0; break;
    }
    if (p >= P_WV + WP_MTX && p < P_WV + WP_MTX + 64) memset(song.wv[pad] + WP_MTX + (p - P_WV - WP_MTX) / 4 * 4, 0, 4);   // MATRIX: the whole slot
    else if (p >= P_WV && p < P_SLWAY) { int8_t d0[WP_N]; wp_defaults(d0); par_put(pad, p, d0[p - P_WV]); }
    else if (p >= P_WV) return;          // SLWAY (above) and WAVE's selectors: nothing more (2026-11-25: SLWAY wrote past GRAND DEEP's bytes)
    else if (p >= P_AX) song.ax[pad][p - P_AX] = ax_def[p - P_AX];
    else if (p >= P_CX) { int8_t d0[16]; cx_defaults(d0); song.cx[pad][p - P_CX] = d0[p - P_CX]; }
    else if (p >= P_CL) { int8_t d0[7]; cl_defaults(d0); t->cp[p - P_CL] = d0[p - P_CL]; }
    else if (p >= P_AC) { int8_t d0[12]; acid_defaults(d0); t->ap[p - P_AC] = d0[p - P_AC]; }
}
static void step_audition(int st);
static void step_reset(Pattern *p, int st);
static void step_seed(Pattern *p, int st);
static void step_hear(int st) {          // an edit sounds, unless the pad's clip plays it anyway
    if (hear_on && !(engine_playing() && song.t[sel].on)) step_audition(st);
}
static void step_adjust(int st, int d, int big) {
    Pattern *p = cur_pat(sel); edit_begin();
    if (!is_auto(lane) && !p->trig[st]) step_seed(p, st);
    if (is_auto(lane)) { AutoLane *al = cur_aut(); int v = al->v[st] == AUTO_EMPTY ? 50 : al->v[st]; al->v[st] = (int8_t)clampi(v + d * (big ? 10 : 1), 0, 100); return; }
    switch (lane) {
    case L_TRIG: case L_NOTE: {          // the step's pitch: a semitone (SCALE SNAP: the next note of the scale), an octave
        int cur = p->note[st] == NOTE_NONE ? clampi(roll_base(sel), 0, 127) : p->note[st], n = cur + (big ? 12 * d : d);
        if (!big && song.snap) while (n > 0 && n < 127 && !in_scale(sel, n)) n += d;
        if (!big && song.snap && !in_scale(sel, n)) n = cur;
        n = clampi(n, 0, 127); p->note[st] = (int8_t)n; last_note[sel] = n + 1; break; }
    case L_SLICE: { int ns = track_nslices(sel);
        if (!ns) { p->slice[st] = clampi(p->slice[st] + d * (big ? 4 : 1), -1, 31); break; }
        int aut = st % ns, cur = p->slice[st] < 0 ? aut : p->slice[st] % ns;   // count from the number shown on the step
        int nv = clampi(cur + d * (big ? 4 : 1), 0, ns - 1);
        p->slice[st] = nv == aut ? -1 : nv; break; }                           // back on its own number: automatic again
    case L_VEL: p->vel[st] = clampi(p->vel[st] + d * (big ? 16 : 1), 1, 127); break;
    case L_PROB: p->prob[st] = clampi(p->prob[st] + d * (big ? 10 : 1), 0, 100); break;
    case L_REP: p->rep[st] = clampi(p->rep[st] + d, 1, NCELL); break;
    case L_ACC: case L_SLD: { int bit = lane == L_ACC ? ACID_ACCENT : ACID_SLIDE, f = p->slice[st] < 0 ? 0 : p->slice[st];
        f = d > 0 ? f | bit : f & ~bit; p->slice[st] = (int8_t)(f ? f : -1); break; }   // RIGHT/UP sets, LEFT/DOWN clears
    }
    p->trig[st] = 1;
    if (lane == L_VEL) last_vel[sel] = p->vel[st] + 1; else if (lane == L_PROB) last_prob[sel] = p->prob[st] + 1;
    else if (lane == L_REP) last_rep[sel] = p->rep[st] + 1; else if (lane == L_SLICE) last_slc[sel] = p->slice[st] + 2;
    if (lane == L_TRIG || lane == L_NOTE || lane == L_SLICE) step_hear(st);   // the new note or slice sounds
}
static void seq_edit_arrow(int k) {     // EDIT + arrows
    if (v2_sound() && sound_edit_arrow(k)) return;
    Pattern *p = cur_pat(sel);
    int d = (k == K_RIGHT || k == K_UP) ? 1 : -1, big = k == K_UP || k == K_DOWN;
    if (srow < 0 && scol == 1) { Track *t = &song.t[sel]; pat_switch((t->queued >= 0 ? t->queued : t->cur) + d); return; }   // the pattern field: the next one
    if (srow < 0) { if ((k == K_LEFT || k == K_RIGHT) && !is_synth(sel)) { edit_begin(); neighbour_file(sel, d); } return; }   // the name row: the next file
    if (srow == 0) {
        edit_begin();
        if (scol == 4) { AutoLane *al = cur_aut();   // the lane's target (<>), jump or glide (^v)
            if (big) return;                      // (JUMP/GLIDE are bends of each segment now: Y + arrows on the steps)
            int t[64], n = lfo_targets(sel, t), i = 0; while (i < n && t[i] != al->target) i++; al->target = (uint8_t)t[((i < n ? i : 0) + d + n) % n]; return; }
        if (scol == 3) { roll_mode(); return; }   // MODE: STEPS or ROLL
        if (scol == 2) { spage = clampi(spage + d, 0, last_page()); return; }   // PAGE: the next or previous 32 steps
        if (scol == 0) p->len = clampi(p->len + d * (big ? 4 : 1), len_lo(), len_hi());
        else p->div = clampi(p->div + d, 0, 5);
    } else if (srow <= 2) {
        if (p->mode && !is_auto(lane)) return;   // the mini roll: EDIT opens the editor
        int st = cur_step();
        if (st >= p->len) { toast("PAST THE CLIP LENGTH"); return; }
        if (lane == L_TRIG && paint_anchor >= 0) {   // drag: fill (or erase) anchor..cursor, both directions
            edit_begin();
            int c = st + (k == K_RIGHT) - (k == K_LEFT) + (v2_steps() ? 8 : 16) * ((k == K_DOWN) - (k == K_UP));
            c = clampi(c, 0, p->len - 1); set_step(c);
            int lo = paint_anchor < c ? paint_anchor : c, hi = paint_anchor < c ? c : paint_anchor;
            for (int i = 0; i < NSTEP; i++) { p->trig[i] = i >= lo && i <= hi ? paint_val : paint_snap[i];
                if (i >= lo && i <= hi && paint_val && !paint_snap[i]) step_seed(p, i); }   // a painted step starts from the lanes' last values   // painted steps get the last note
        } else step_adjust(st, d, big);
    } else {
        int pr = cur_par();
        if (big && (pr == P_AC + AP_DTYPE || pr == P_WARP || pr == P_AX + AX_SUBWAVE || wv_is_list(pr))) { d = -d; big = 0; }   // the lists run down: UP the one above
        if (big && wv_is_pair(pr)) { d = k == K_DOWN ? 1 : -1; big = 0; }   // levers: UP the first, DOWN the second
        if (big && pr == P_AX + AX_SUBOCT) { d = k == K_DOWN ? 1 : -1; big = 0; }   // the lists run down: UP goes to the top one
        if (big && pr == P_DIR) {                                    // the sampler's lever: UP FWD, DOWN REV
            if (song.t[sel].rev != (k == K_DOWN)) { edit_begin(); song.t[sel].rev = k == K_DOWN; }
        } else if (big && pr == P_VOICE) {                           // the lever: UP MONO (GATE), DOWN POLY
            if (song.t[sel].voice != (k == K_DOWN)) { edit_begin(); song.t[sel].voice = k == K_DOWN; }
        } else if (big && pr == P_CX + CX_FREEZE) {                  // CLOUD DEEP's FREEZE lever: UP ON, DOWN OFF
            if (song.cx[sel][CX_FREEZE] != (k == K_UP)) { edit_begin(); song.cx[sel][CX_FREEZE] = k == K_UP; }
        } else if (big && pr == P_AC + AP_WAVE) {                    // ACID's lever: UP SAW, DOWN SQUARE
            if (song.t[sel].ap[AP_WAVE] != (k == K_DOWN)) { edit_begin(); song.t[sel].ap[AP_WAVE] = k == K_DOWN; }
        } else par_adjust(sel, pr, d, big);
        par_focus(pr);
        if (is_synth(sel) && srow >= 3 && held[K_EDIT] && pr != P_SAMPLE && is_synth(sel)) gb_audition();
    }
}
static int v2_auto(void); static void auto2_move(int k);
static void seq_move(int k) {
    if (v2_sound()) { sound_move(k); return; }
    if (v2_auto()) { auto2_move(k); return; }
    if (v2_steps()) {                    // STEPS (UI v2): two rows of eight, the pages follow the cursor
        // 2026-11-24 (user): <> stay in the row of eight, their edges lead to the side slots; v from the bottom row is the
        // next bar (past the end the clip grows a whole bar), ^ from the top row the bar before
        Pattern *p = cur_pat(sel); int len = p->len, c = cur_step(), d = k == K_RIGHT ? 1 : k == K_LEFT ? -1 : k == K_DOWN ? 8 : -8, n = c + d;
        if ((k == K_LEFT && c % 8 == 0) || (k == K_RIGHT && c % 8 == 7)) return;
        if (k == K_DOWN && n >= len && c % 16 >= 8 && len < GROW_MAX && !ui_qbar) {   // the bar at the bottom: DOWN reaches it (LEN/GRID grows)
            edit_begin(); p->len = (uint8_t)(len / 16 * 16 + 16 > GROW_MAX ? GROW_MAX : len / 16 * 16 + 16); toast_length("LENGTH", p->len);
            if (n >= p->len) n = p->len - 1;
        }
        int bar = (len - 1) / 16 * 16;   // leaving an empty last bar backwards: it goes again
        if (len > 16 && cur_step() >= bar && n >= 0 && n < bar) { int used_ = 0; for (int i = bar; i < len; i++) used_ |= p->trig[i];
            if (!used_) { edit_begin(); p->len = (uint8_t)bar; toast_length("LENGTH", p->len); } }
        if (n >= 0 && n < p->len) set_step(n);
        return;
    }
    if (srow >= 1 && srow <= 2 && cur_pat(sel)->mode && !is_auto(lane)) {   // the mini roll: up to MODE, down to the parameters
        if (k == K_UP) { srow = 0; scol = 3; }
        else if (k == K_DOWN) { srow = 3; scol = 0; gb_cur = 0; seq_clamp(); }
        return;
    }
    if (srow >= 3) {                     // the panel: the nearest control that way; up past the top: the steps
        int n = gb_nav(k);
        {                                     // the DEEP pages lie below the panel, as the next screen down (the sampler's too, 2026-11-12)
            int nc; const GbCtl *c = pctl(&nc); int x = c[clampi(gb_cur, 0, nc - 1)].x;
            if (cl_deep < 1 && song.t[sel].type != 1 && k == K_DOWN && n < 0) {   // (WAVE has no DEEP page: its pages are the tabs)
                cl_deep++; const GbCtl *d = pctl(&nc); int best = 1, bd = 1 << 30;
                for (int i = 1; i < nc; i++) if (d[i].y < 420) { int dd = abs(d[i].x - x); if (dd < bd) { bd = dd; best = i; } }
                gb_cur = best; return;
            }
            if (cl_deep && k == K_UP && (n < 0 || c[n].p == P_SAMPLE)) {
                cl_deep--; const GbCtl *m = pctl(&nc); int best = 0, bd = 1 << 30;
                for (int i = 0; i < nc; i++) if (m[i].kind == GB_KNOBC || m[i].kind == GB_WAVE) { int dd = abs(m[i].x - x); if (dd < bd) { bd = dd; best = i; } }
                gb_cur = best; return;
            }
        }
        if (n >= 0) gb_cur = n;
        else if (k == K_UP) { int n; const GbCtl *c = pctl(&n); srow = 2; scol = clampi((c[clampi(gb_cur, 0, n - 1)].x - 16) / 62, 0, 15); }
        return;
    }
    if (srow == 2 && k == K_DOWN) { srow = 3; scol = 0; gb_cur = 0; return; }
    if ((k == K_LEFT || k == K_RIGHT) && srow >= 1 && srow <= 2) {   // steps: one line through the pages
        set_step(clampi(cur_step() + (k == K_RIGHT ? 1 : -1), 0, cur_pat(sel)->len - 1));
        return;
    }
    if (k == K_LEFT || k == K_RIGHT) {
        int nc = clampi(scol + (k == K_RIGHT ? 1 : -1), 0, seq_cols(srow) - 1);
        if (srow >= 3 && nc != scol) srow = 3 + par_row_near(nc, srow - 3);
        scol = nc;
        return;
    }
    int d = k == K_DOWN ? 1 : -1;
    if (srow >= 3) {                     // parameters: skip empty places
        int r = srow - 3 + d;
        while (r >= 0 && r < PROWS && par_at(scol, r) < 0) r += d;
        if (r >= PROWS) return;
        if (r >= 0) { srow = 3 + r; return; }
    }
    int nr = clampi(srow + d, -1, 3);
    scol = scol * seq_cols(nr) / seq_cols(srow);
    srow = nr;
}
static void seq_delete(void) {
    if (srow < 0) return;
    Pattern *p = cur_pat(sel); edit_begin();
    if (srow == 0 && scol == 4 && is_auto(lane)) { char b[32]; cur_aut()->target = 0; snprintf(b, sizeof b, "%s UNLINKED", lane_name[lane]); toast(b); return; }   // 2026-11-12, user: the points stay
    if (srow == 0) { int l = p->len, dv = p->div, md = p->mode; pattern_clear(p); sub_drop_p(p, -1); p->len = l; p->div = dv; p->mode = md; toast("CLIP CLEARED"); }
    else if (srow >= 3) { int pr = cur_par(); par_default(sel, pr); par_focus(pr); }   // a SOUND card: its default (2026-11-25: on a roll pad this cleared the roll)
    else if (p->mode) { for (int i = 0; i < NNOTE; i++) roll_del(p, i); toast("ROLL CLEARED"); }
    else if (srow <= 2 && is_auto(lane)) cur_aut()->v[cur_step()] = AUTO_EMPTY;
    else if (srow <= 2 && lane != L_TRIG) { char b[32]; snprintf(b, sizeof b, "ERASE ON THE %s LANE", lane_nm(L_TRIG)); toast(b); }
    else if (srow <= 2) { int st = cur_step();
        p->trig[st] = 0; p->note[st] = NOTE_NONE; p->vel[st] = 100; p->prob[st] = 100; p->rep[st] = 1; p->slice[st] = -1; }
    else { int pr = cur_par(); par_default(sel, pr); par_focus(pr); }
}
static void browse_slot(void) {          // the browser, in the folder of the slot's sample
    const char *pth = song.t[sel].path, *sl = strrchr(pth, '/');
    char d[256]; snprintf(d, sizeof d, "%.*s", sl ? (int)(sl - pth) : 7, sl ? pth : "samples");
    if (!sl && blast[0]) snprintf(d, sizeof d, "%s", blast);
    bmode = 0; browse_open(d);
}
static void move_cursor(int k);
static int auto_bind_target(int p) {     // the LT_* code a panel control is automated through, -1 none
    int ty = song.t[sel].type, tg = -1;
    if (p == P_PITCH) tg = LT_PITCH; else if (p == P_VOL) tg = LT_VOL; else if (p == P_PAN) tg = LT_PAN;
    else if (ty == 1 && p >= P_WV && p < P_WV + LT_ENGN) tg = LT_ENG + p - P_WV;   // WAVE: wp 0..55 (OSC A B, SUB, NOISE, FILTER, ENV1)
    else if (ty == 2 && p >= P_AC && p < P_AC + AP_N) tg = LT_ENG + p - P_AC;
    else if (ty == 3 && p >= P_CL && p < P_CL + CP_N) tg = LT_ENG + p - P_CL;
    else if (ty == 3 && p >= P_CX && p < P_CX + CX_N) tg = LT_ENG + CP_N + p - P_CX;
    else if (ty == 2 && p >= P_AX && p < P_AX + AX_N) tg = LT_ENG + LT_DEEP + p - P_AX;   // ACID DEEP
    if (tg < 0) return -1;
    int t[256], n = lfo_targets(sel, t); for (int i = 0; i < n; i++) if (t[i] == tg) return tg;   // only what the engine can move
    return -1;
}
static void learn_start(void) {
    learning = 1; learn_lane = lane; int tg = cur_aut()->target, n; const GbCtl *c;
    srow = 3; scol = 0; gb_cur = 1; ui_cat = PC_SOUND; toast("POINT AT A PARAMETER: EDIT TAKES IT");   // UI v2: on the cards of SOUND
    c = pctl(&n); for (int i = 0; i < n; i++) if (tg && auto_bind_target(c[i].p) == tg) { gb_cur = i; break; }
    if (gb_cur >= n) gb_cur = 0;
    seq_clamp();
}
static int fx_page; static int lcol, lrow; static int fx_unit; static void set_tab(int t);
static void wv_learn_back(int m);       // below sound.h: back on the page the pointing began from
static int wv_seq_tap(void);             // below sound.h: EDIT on WAVE's pictures, TARGET, a matrix DEST; 1 = done
static void snd_page_go(int i);
static void learn_end(void) {
    if (learn_wv >= 0) { int m = learn_wv; learn_wv = -1; learning = 0; wv_learn_back(m); return; }   // WAVE
    ui_cat = PC_MOD;                     // UI v2: the cards of SOUND were shown for the pointing
    if (learn_lfo >= 0) { int j = learn_lfo; learn_lfo = -1; set_tab(T_FX); fx_unit = sel; fx_page = 1; lcol = j; lrow = 0; learning = 0; return; }   // back on the MOD page
    learning = 0; lane = learn_lane; srow = 0; scol = 4;
}
static void wv_learn_start(int m) {     // WAVE: EDIT on a matrix slot's DEST (m = slot) or an LFO's TARGET (m = 16 + j): point at a card
    learning = 1; learn_wv = m; learn_lfo = -1; learn_lane = lane; snd_page_go(0); toast("POINT AT A CARD: EDIT TAKES IT, Y CANCELS");
}
static void wv_learn_take(int p) {
    int k = p - P_WV; char b[48];
    if (p < P_WV || k >= WP_NP || !wp_info[k].mod) { toast("THAT ONE CANNOT BE MODULATED: ANOTHER CARD"); return; }
    int8_t *wp = song.wv[sel]; edit_begin(); int sl = learn_wv;
    if (sl >= 16) {                      // an LFO's TARGET: a new slot (or the one this LFO already has to it)
        int src = WS_LFO1 + (sl - 16), fr = -1; sl = -1;
        for (int i = 0; i < WM_SLOTS; i++) { int8_t *m = wp + WP_MTX + 4 * i; if (m[WM_SRC] == src && m[WM_DST] == k + 1) { sl = i; break; } if (fr < 0 && !m[WM_SRC] && !m[WM_DST]) fr = i; }
        if (sl < 0) sl = fr;
        if (sl < 0) { toast("THE MATRIX IS FULL: CLEAR A SLOT"); learn_end(); return; }
        int8_t *m = wp + WP_MTX + 4 * sl; m[WM_SRC] = (int8_t)src; if (!m[WM_AMT]) m[WM_AMT] = 25; m[WM_MODE] = 1;
    }
    wp[WP_MTX + 4 * sl + WM_DST] = (int8_t)(k + 1);
    snprintf(b, sizeof b, "M%d %s -> %s", sl + 1, wv_src_names[wp[WP_MTX + 4 * sl + WM_SRC] % WS_N], wp_name(k)); learn_end(); toast(b);
}
static void learn_lfo_start(int u, int j) {   // EDIT on an LFO's TARGET: point at a knob on the pad's panel
    sel = u; set_tab(T_SEQ); ui_cat = PC_SOUND; toast("POINT AT A PARAMETER: EDIT TAKES IT"); learn_lane = lane; learning = 1; learn_lfo = j; int tg = song.lfo[u][j].target, n;
    srow = 3; scol = 0; gb_cur = 1; const GbCtl *c = pctl(&n); for (int i = 0; i < n; i++) if (tg && auto_bind_target(c[i].p) == tg) { gb_cur = i; break; }
    if (gb_cur >= n) gb_cur = 0;
    seq_clamp();
}
static int learn_key(int k, int down) {   // keys while LEARN waits; 1 = used
    if (tab != T_SEQ || browsing || slicing || rolling) { learning = 0; return 0; }
    if (k == K_OPT || k == K_Y) { if (down) { used[k] = 1; learn_end(); toast("NO TARGET TAKEN"); } return 1; }
    if (k == K_EDIT) {
        if (!down && !used[K_EDIT] && learn_wv >= 0) { wv_learn_take(cur_par()); return 1; }   // WAVE's matrix
        if (!down && !used[K_EDIT]) {
            int pr = cur_par(), tg = auto_bind_target(pr); char b[48];
            if (tg < 0) { snprintf(b, sizeof b, "%s CANNOT BE AUTOMATED YET", pr >= 0 ? par_name[pr] : "THIS"); toast(b); return 1; }
            char tn[24]; lfo_tname(sel, tg, tn); edit_begin();
            if (learn_lfo >= 0) { Lfo *L = &song.lfo[sel][learn_lfo]; L->target = (int8_t)tg; if (!L->depth) L->depth = 50; snprintf(b, sizeof b, "LFO %d -> %s", learn_lfo + 1, tn); }
            else { lane = learn_lane; cur_aut()->target = (int8_t)tg; snprintf(b, sizeof b, "%s -> %s", lane_name[lane], tn); }
            learn_end(); toast(b);
        }
        return 1;
    }
    if (down && (k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT)) { move_cursor(k); if (srow < 3) { learn_end(); toast("NO TARGET TAKEN"); } return 1; }
    return 0;                            // L1/R1 (the pages), PLAY and the rest as usual
}
static void seq_tap(void) {
    if (srow == 0 && scol == 3) { roll_mode(); return; }
    if (srow == 0 && scol == 4) { learn_start(); return; }   // pick the target by pointing at a knob
    if (srow >= 1 && srow <= 2 && is_auto(lane)) {   // a tap: a value (the last one before it, else 50) or none
        int st = cur_step(); Pattern *p = cur_pat(sel); if (st >= p->len) { toast("PAST THE CLIP LENGTH"); return; }
        AutoLane *al = cur_aut(); edit_begin();
        if (al->v[st] != AUTO_EMPTY) al->v[st] = AUTO_EMPTY;
        else { int v = 50; for (int i = st - 1; i >= 0; i--) if (al->v[i] != AUTO_EMPTY) { v = al->v[i]; break; } al->v[st] = (int8_t)v; }
        if (!al->target) toast("CHOOSE A TARGET: UP TO THE HEADER");
        return;
    }
    if (srow >= 1 && srow <= 2 && cur_pat(sel)->mode) { if (cur_pat(sel)->mode == 2) chord_open(); else roll_open(); return; }
    if (srow == 0 && scol == 2) { spage = last_page() ? (spage + 1) % (last_page() + 1) : 0; return; }   // EDIT on PAGE: the next one, round
    if (srow >= 1 && srow <= 2) {
        Pattern *p = cur_pat(sel); int st = cur_step();
        if (st >= p->len) { toast("PAST THE CLIP LENGTH"); return; }
        edit_begin();
        if (lane == L_ACC || lane == L_SLD) {   // ACID: a tap toggles the step's ACCENT or SLIDE (and puts the step on)
            int bit = lane == L_ACC ? ACID_ACCENT : ACID_SLIDE, f = p->slice[st] < 0 ? 0 : p->slice[st];
            if (!p->trig[st]) { p->trig[st] = 1; f |= bit; } else f ^= bit;
            p->slice[st] = (int8_t)(f ? f : -1); return;   // it sounded when B went down
        }
        if (edit_put) { edit_put = 0; return; }   // put when EDIT went down: the release leaves it on
        if (p->trig[st] && lane == L_PROB) { prb_open(st); return; }  // PRB: its mode NOTE / SUB / JUMP
        if (p->trig[st] && lane != L_TRIG) { char b[32]; snprintf(b, sizeof b, "ERASE ON THE %s LANE", lane_nm(L_TRIG)); toast(b); return; }   // 2026-11-12, user
        if (!p->trig[st]) step_seed(p, st);
        p->trig[st] = !p->trig[st];
        // 2026-11-11 (user): a new step is the tonic of the pad's key (NOTE_NONE plays exactly that and follows the key), and its name shows at once
        if (!p->trig[st]) step_reset(p, st);   // (a new step sounded when B went down)           // a removed step forgets its note, velocity, chance, repeats, slice
    } else if (song.t[sel].type == 1 && wv_seq_tap()) return;   // WAVE: the tables, TARGET, a matrix DEST
    else if (srow >= 3 && cur_par() == P_SAMPLE && is_synth(sel)) preset_open();   // EDIT on the instrument: its presets
    else if (srow >= 3 && cur_par() == P_SRC) browse_slot();   // CLOUD: EDIT on SOURCE opens the browser
    else if (srow >= 3 && song.t[sel].type == 0 && cur_par() == P_BPM) {   // the loop's tempo by tapping (besides AUTO and EDIT+arrows)
        static Uint32 tt[8]; static int tn; int b = tap_bpm(tt, &tn); char m[32];
        if (b) { edit_begin(); song.t[sel].bpm = (int16_t)clampi(b, 40, 300); relayout(sel); snprintf(m, sizeof m, "LOOP TAP  %d BPM", song.t[sel].bpm); } else snprintf(m, sizeof m, "TAP: GO ON IN TIME");
        toast(m); }
    else if (srow >= 3 && is_synth(sel) && cur_par() == P_AC + AP_WAVE) { edit_begin(); song.t[sel].ap[AP_WAVE] ^= 1; }   // a tap flips the lever
    else if (srow >= 3 && is_synth(sel) && cur_par() == P_CX + CX_FREEZE) { edit_begin(); song.cx[sel][CX_FREEZE] ^= 1; }
    else if (srow >= 3 && is_synth(sel) && wv_is_pair(cur_par())) { int lo, hi, st, bg; par_limits(sel, cur_par(), &lo, &hi, &st, &bg); edit_begin(); par_put(sel, cur_par(), par_get(sel, cur_par()) == lo ? hi : lo); }   // WAVE: a tap flips a switch
    else if (srow >= 3 && is_synth(sel) && cur_par() == P_AX + AX_SUBOCT) { edit_begin(); song.ax[sel][AX_SUBOCT] ^= 1; song.ax[sel][AX_MARK] = AX_INIT; }   // a tap flips the lever
    else if (srow >= 3 && is_synth(sel) && cur_par() != P_VOICE) return;   // B on a knob plays it, the value stays
    else if (srow < 0 && scol == 1) pat_open();   // EDIT on the pattern: into the pattern window
    else if (srow < 0 && !has_sound(sel)) pick_open();   // an empty pad: which instrument first
    else if (srow < 0 || (srow >= 3 && cur_par() == P_SAMPLE)) browse_slot();
    else if (srow >= 3 && cur_par() == P_SLICES) {    // EDIT on SLICES: the slice editor
        if (engine_sample(sel)) slicing = 1; else toast("NO SAMPLE");
    } else if (srow >= 3 && cur_par() == P_ROOT) {      // EDIT on ROOT: make it the project key
        int r = track_root(sel); if (r >= 0) { edit_begin(); song.key = r; toast("PROJECT KEY SET"); }
    } else if (srow >= 3) { int pr = cur_par(); par_adjust(sel, pr, 1, 0); par_focus(pr); }
}

// ---------- PATTERNS ----------
static int pcur;
static int pat_last;                      // the pattern chosen in the window by EDIT (not where the cursor passed): OK and Y go there
static void pat_switch(int np) {          // the pad's pattern: queued for the next quantum while its clip plays, else at once
    Track *t = &song.t[sel]; np = ((np % NPAT) + NPAT) % NPAT;
    if (np == t->cur) { t->queued = -1; return; }   // already this one: nothing to restart
    if (engine_quant32() && engine_playing() && t->on) t->queued = (int8_t)np; else { t->cur = (int8_t)np; t->queued = -1; }
}
static int pfol;                  // SELECT: the cursor is on the cards (not on the grid); pfcol: the old window's field
static int sl_r, sl_c;                   // SELECT (UI v2): rows 0-1 the grid 2 x 8, 2 LENGTH DIVISION MODE, 3-4 REPEAT THEN / CHANCE ELSE
static void select_value(int d, int big);
static const char *const fa_name[FA_N] = {"LOOP", "NEXT", "PREV", "FIRST", "RANDOM", "OTHER", "JUMP", "STOP"};
static int fol_pat(void) { return pat_last >= 0 && pat_last < NPAT ? pat_last : pcur % NPAT; }   // FOLLOW is about the chosen pattern (SEL)
static void pat_open(void) { Track *t = &song.t[sel]; pcur = pat_last = t->queued >= 0 ? t->queued : t->cur; pfol = 0; tab = T_PAT; sl_r = pcur / 8; sl_c = pcur % 8; }
static void pat_close(int take) {         // OK and Y both leave on the chosen pattern (the one EDIT played or marked)
    (void)take; pat_switch(pat_last);
    if (ui_cat == PC_PATTERN) { cat_open(PC_PATTERN); return; }   // v3: back to the layout
    tab = T_SEQ; srow = -1; scol = 1;
}
static void pat_tap(void) {
    Track *t = &song.t[sel];
    if (sl_r >= 2) { select_value(1, 0); return; }   // EDIT on a card: the next value
    if (pcur >= NPAT) { pat_close(1); return; }   // the OK button
    if (pat_last != pcur) { pat_last = pcur; return; }   // the first EDIT only chooses it (OK and Y will leave on it): no sound (2026-11-11, user)
    if (engine_quant32() && engine_playing()) t->queued = pcur;   // EDIT again on the chosen one plays it: at the next quantum
    else { t->cur = pcur; if (!t->on) clip_toggle(sel); }
}
static void pat_play(void) {             // PLAY in the window: the pattern under the cursor, chosen and played at once; the one that plays: the clip stops
    Track *t = &song.t[sel]; play_down = SDL_GetTicks() | 1;
    if (pfol || pcur >= NPAT) { play_key(); return; }
    pat_last = pcur;
    if (t->on && t->cur == pcur && t->queued < 0) { clip_toggle(sel); return; }
    if (engine_quant32() && engine_playing()) t->queued = pcur;
    else { t->cur = pcur; if (!t->on) clip_toggle(sel); }
}

// ---------- PROJECT ----------
enum { J_TEMPO, J_SWING, J_KEY, J_SCALE, J_LAUNCH, J_METRO, J_BEATS, J_COUNT, J_QUANT, J_MASTER, J_THEME, J_LIB, J_NEW, J_SNAP, J_PROJ, J_SAVE, J_SAVEAS, J_EXPORT, J_HINT, J_STRIP, J_VIEW, J_LOCK, J_NAV, J_DRAWER, J_MXFX, J_MXLINK, J_MPORT, J_MCLK, J_MNOTE, J_MCH, J_MCLKIN, J_TRIM, J_ABOUT, J_MSND, J_MVIB, J_MVOL, J_MTONE, J_MWHEN, J_MDIV, J_MVOFF, J_QBAR, J_SECS, J_QBTN, J_N };
// The sections of PROJECT (2026-11-09): the left column chooses one, the middle shows its settings
// 2026-11-14 (user): INTERFACE holds how BrickPad looks and moves, ABOUT every key of it by topic (the sheet SHIFT used to show)
// 2026-11-20 (user: "too many tabs, sort them by what they are for"): eight sections by task, the metronome its own.
enum { JS_FILE, JS_TEMPO, JS_METRO, JS_PLAY, JS_OUT, JS_MIDI, JS_SET, JS_HELP, JS_N };
#define JS_ROWS 10                       // settings in a section, at most
enum { AB_N = 11 }; static int ab_pg;   // ABOUT: its topics, the one shown
static const char *const prj_sec_name[JS_N] = {"FILE", "TEMPO", "METRONOME", "PLAY / REC", "OUTPUT", "MIDI", "SETTINGS", "HELP"};
static const int8_t prj_sec[JS_N][JS_ROWS] = {{J_PROJ, J_SAVE, J_SAVEAS, J_EXPORT, J_NEW, -1}, {J_TEMPO, J_SWING, J_BEATS, J_KEY, J_SCALE, J_SNAP, -1},
                                        {J_MSND, J_MVIB, J_MVOL, J_MTONE, J_MWHEN, J_MDIV, J_MVOFF, -1}, {J_LAUNCH, J_COUNT, J_QUANT, -1},
                                        {J_MASTER, J_TRIM, J_DRAWER, J_MXFX, J_MXLINK, -1}, {J_MPORT, J_MCLKIN, J_MCLK, J_MNOTE, J_MCH, -1},
                                        {J_VIEW, J_STRIP, J_SECS, J_QBAR, J_QBTN, J_HINT, J_LOCK, J_THEME, J_LIB, -1}, {J_ABOUT, -1}};
static const char *const prj_help[J_N] = {"THE PROJECT'S SPEED. EDIT TAPS IT, L1/R1 HELD NUDGE WHILE PLAYING", "LATE EVEN 1/16 STEPS: A SHUFFLE",
    "THE TONIC EVERY PAD PLAYS IN (A PAD CAN HAVE ITS OWN)", "WHICH NOTES THE DEGREES, KEYS AND RANDOM USE",
    "A CLIP STARTS OR STOPS ON THIS GRID, AS IN ABLETON", "A CLICK ON EVERY BEAT WHILE PLAYING", "BEATS IN A BAR: LAUNCH AND RECORDING COUNT THEM",
    "BEATS COUNTED IN BEFORE A TAKE FROM STOP", "WHERE RECORDED HITS LAND ON THE STEPS", "THE WHOLE MIX, THE SAME FADER AS IN MIXER",
    "COLOURS OF BRICKPAD, KEPT FOR EVERY PROJECT", "CHECKS EVERY SAMPLE ONCE, SLOWLY, MARKS BROKEN FILES", "EDIT TWICE: AN EMPTY PROJECT (UNDO BRINGS IT BACK)", "ON: EDIT+<> ON A STEP WALKS THE PAD'S SCALE ONLY, AS ON THE M8", "THE OPEN PROJECT; * = CHANGES NOT SAVED. EDIT: THE PROJECTS", "EDIT: WRITE THE PROJECT TO ITS FILE", "EDIT: TYPE A NAME, THEN SAVE UNDER IT", "EDIT: RENDER BARS OF THE PROJECT TO exports/NAME.wav", "THE KEYS OF THE SCREEN IN THE FOOTER. OFF: A CLEAN FOOTER. EVERY KEY IS IN PROJECT > HELP",
    "THE TWELVE PADS UNDER THE PATH INSIDE A PAD: HIDDEN, THIN BARS OR BIG TILES WITH NAMES", "ADVANCED: EVERYTHING. SIMPLE: PLAY, NOTES, SOUND, LARGE (F1/F2). HOLD F1+F2 TO LEAVE IT",
    "SIMPLE VIEW ONLY, FOR A CHILD: NOTES CANNOT BE ERASED AND THE DICE IS OFF. THE ADVANCED VIEW IGNORES IT",
    "INSIDE A PAD: A LIST OF ITS CATEGORIES, OR A TREE WITH EVERY PAGE (<> LEVEL, ^v CHOOSE, EDIT OPEN)",
    "THE MIXER'S DRAWER WITH THE RETURNS, THE MASTER AND THE PAD'S EFFECTS: THE SIDE IT SLIDES OUT OF",
    "EFFECTS AND SENDS IN SMALL ON EVERY STRIP. OFF: A CLEANER MIXER. THE DRAWER ALWAYS SHOWS THEM IN FULL",
    "THE SAME SOUND ON THE SAME PAD IN ANOTHER SCENE: ONE MIX FOR EVERY SCENE, OR ITS OWN IN EACH. ANOTHER SOUND ALWAYS KEEPS ITS OWN",
    "THE MIDI DEVICE ON USB (THE M8, A KEYBOARD, A CONTROLLER). EDIT+<> CHOOSES, EDIT LOOKS AGAIN", "ON: BRICKPAD SENDS START, STOP AND 24 CLOCKS A BEAT: THE OTHER DEVICE FOLLOWS ITS TEMPO",
    "KEYS: NOTES PLAY THE OPEN PAD AT THEIR PITCH. PADS: NOTES 36-47 (C1-B1) PLAY PADS 1-12", "THE MIDI CHANNEL NOTES ARE TAKEN FROM",
    "ON: BRICKPAD FOLLOWS THE OTHER DEVICE: ITS TEMPO, START AND STOP. KNOBS: R2 ON A SOUND CARD, THEN TURN ONE",
    "LOUDER OUT OF THE BRICK, AFTER THE LIMITER AND THE CLIP (MAY DISTORT). EXPORTS STAY AS THEY ARE", NULL,
    "THE CLICK YOU HEAR. MENU+X ANYWHERE: OFF, SOUND, SOUND + VIBRATION, VIBRATION", "THE BRICK BUZZES ON THE BEAT: ALONE IT IS A SILENT METRONOME (THE BAR'S FIRST BUZZES LONGER)",
    "HOW LOUD THE CLICK IS (IT NEVER GOES INTO AN EXPORT OR A FREEZE)", "CLICK: A SHORT TICK. WOOD: A KNOCK. BEEP: A LONGER TONE",
    "ALWAYS WHILE PLAYING, OR ONLY WHILE RECORDING AND ITS COUNT-IN", "A CLICK ON EVERY BEAT, EVERY 1/8 OR EVERY 1/16 (THE BEAT IS LOUDER)",
    "THE BUZZ EARLIER (-) OR LATER (+) IN MS, UNTIL IT LANDS WITH THE CLICK",
    "WHERE THE QUICK BAR SITS: AT THE TOP OR THE BOTTOM. ARROW PAST THE EDGE OF A PAGE REACHES IT",
    "THE PAD'S SECTIONS UNDER THE PADS: PATTERN SOUND FX MOD MIX LOOK", "EDIT: PICK UP TO 12 BUTTONS FOR THE QUICK BAR. THE REST ARE IN ITS DRAWER"};
static const char *prj_name[J_N] = {"TEMPO", "SWING", "KEY", "SCALE", "CLIP LAUNCH", "METRONOME", "BEATS/BAR", "COUNT-IN", "REC QUANT", "MASTER VOL", "THEME", "CHECK LIBRARY", "NEW PROJECT", "SCALE SNAP", "PROJECT", "SAVE", "SAVE AS", "EXPORT WAV", "FOOTER HINTS", "PAD STRIP", "VIEW", "KID LOCK", "PAD MENU", "MIXER DRAWER", "STRIP EFFECTS", "SCENE MIX", "MIDI PORT", "CLOCK OUT", "MIDI NOTES", "MIDI CHANNEL", "CLOCK IN", "OUTPUT TRIM", "ALL THE KEYS", "SOUND", "VIBRATION", "VOLUME", "SOUND OF IT", "WHEN", "EVERY", "VIBRATION TIMING", "QUICK BAR", "SECTION STRIP", "BAR BUTTONS"};
static int ui_strip = 1, ui_simple, ui_lock, ui_qbar, ui_secs = 1;   // v3: ui_qbar the quick bar 0 top 1 bottom, ui_secs the section strip on
// PROJECT > INTERFACE: the pad strip (0 hidden, 1 thin, 2 big), the simple view, its kid lock (ui.cfg beside the binary)
static int ui_tree, ui_drawer_left;          // the pad menu as a tree (A23) instead of the list; the mixer's drawer on the left
static int ui_mxlink = 1;                    // PROJECT > MIXER > SCENE MIX (2026-11-15, user): one mix for the same sound in every scene
static int ui_mxfx = 1;                      // PROJECT > MIXER > STRIP EFFECTS (2026-11-14, user): the strips show their effects and sends too
static int lst_on;                        // LISTEN STEP (side slot, 2026-11-24): a step or a roll note sounds when the cursor lands on it
static void ui_cfg_save(void) { char b[64]; int n = snprintf(b, sizeof b, "%d %d %d %d %d %d %d %d %d %d\n", ui_strip, ui_simple, ui_lock, ui_tree, ui_drawer_left, ui_mxfx, ui_mxlink, ui_qbar, ui_secs, lst_on); write_whole("ui.cfg", b, n); }
static void ui_cfg_load(void) { FILE *f = fopen("ui.cfg", "r"); if (f) { int a = 1, b = 0, c = 0, d = 0, e = 0, g = 1, l = 1, q = 0, sc = 1, ls = 0;
    if (fscanf(f, "%d %d %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &g, &l, &q, &sc, &ls) >= 1) { lst_on = ls != 0; ui_qbar = q != 0; ui_secs = sc != 0; ui_strip = clampi(a, 0, 2); ui_simple = b != 0; ui_lock = c != 0; ui_tree = d != 0; ui_drawer_left = e != 0; ui_mxfx = g != 0; ui_mxlink = l != 0; } fclose(f); } }
static int trim_db;                        // OUTPUT TRIM 0..24 dB (trim.cfg beside the binary: a device setting, not the project's)
static void trim_apply(void) { out_trim = powf(10, trim_db / 20.f); }
static void trim_save(void) { char b[16]; int n = snprintf(b, sizeof b, "%d\n", trim_db); write_whole("trim.cfg", b, n); trim_apply(); }
static void trim_load(void) { FILE *f = fopen("trim.cfg", "r"); if (f) { int v = 0; if (fscanf(f, "%d", &v) == 1) trim_db = clampi(v, 0, 24); fclose(f); } trim_apply(); }
// METRONOME (2026-11-20, user): the sound and the vibration apart, both device settings (metro.cfg beside the binary).
// The vibration motor is the Brick's rumble, GPIO 227 (as /usr/bin/knulli-rumble drives it): on, a pulse, off. It cannot
// be weaker, only shorter: the bar's first buzzes longest. Its own thread waits for each beat the audio thread posts.
static int vib_off;                        // VIBRATION TIMING, ms: the motor needs a moment to spin up
static int vib_fd = -1;
static void metro_save(void) { char b[64]; int n = snprintf(b, sizeof b, "%d %d %d %d %d %d %d\n", metro.snd, metro.vib, metro.vol, metro.tone, metro.when, metro.div, vib_off); write_whole("metro.cfg", b, n); }
static void metro_load(void) { FILE *f = fopen("metro.cfg", "r"); int v[7] = {0, 0, 70, 0, 0, 0, 0};
    if (f) { for (int i = 0; i < 7 && fscanf(f, "%d", &v[i]) == 1; i++) {} fclose(f); }
    metro.snd = v[0] != 0; metro.vib = v[1] != 0; metro.vol = (int8_t)clampi(v[2], 0, 100); metro.tone = (int8_t)clampi(v[3], 0, 2);
    metro.when = v[4] != 0; metro.div = (int8_t)clampi(v[5], 0, 2); vib_off = clampi(v[6], -80, 80); }
static void vib_set(int on) {
    if (vib_fd < 0) {
        if (access("/sys/class/gpio/gpio227", F_OK)) { int fd = open("/sys/class/gpio/export", O_WRONLY); if (fd >= 0) { if (write(fd, "227", 3) < 0) {} close(fd); } }   // sysfs: straight writes, no .tmp
        { int fd = open("/sys/class/gpio/gpio227/direction", O_WRONLY); if (fd >= 0) { if (write(fd, "out", 3) < 0) {} close(fd); } }
        vib_fd = open("/sys/class/gpio/gpio227/value", O_WRONLY); if (vib_fd < 0) return; }
    if (write(vib_fd, on ? "1" : "0", 1) < 0) {}
}
static volatile int vib_quit; static SDL_Thread *vib_th;
static void ns_sleep_until(int64_t t) { struct timespec ts = {t / 1000000000LL, t % 1000000000LL}; while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR) {} }
static int64_t ns_now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000000000LL + ts.tv_nsec; }
static int vib_main(void *arg) {
    (void)arg; uint32_t r = __atomic_load_n(&vib_ring.n, __ATOMIC_ACQUIRE);
    while (!vib_quit) {
        uint32_t n = __atomic_load_n(&vib_ring.n, __ATOMIC_ACQUIRE);
        if (r == n) { usleep(1000); continue; }
        if (n - r > 16) r = n - 16;          // fell behind: the newest only
        int64_t due = vib_ring.due[r % 16] + (int64_t)vib_off * 1000000; int kind = vib_ring.kind[r % 16]; r++;
        if (due < ns_now() - 30000000LL) continue;   // long past: skipped, never late
        ns_sleep_until(due); vib_set(1);
        ns_sleep_until(due + (kind == 2 ? 70 : kind ? 40 : 22) * 1000000LL); vib_set(0);
    }
    if (vib_fd >= 0) vib_set(0);
    return 0;
}
static const char *const metro_tone_name[3] = {"CLICK", "WOOD", "BEEP"}, *const metro_div_name[3] = {"1/4", "1/8", "1/16"};
static void metro_cycle(void) {            // MENU+X anywhere: OFF > SOUND > SOUND + VIBRATION > VIBRATION > OFF
    int st = metro.snd ? (metro.vib ? 2 : 1) : metro.vib ? 3 : 0; st = (st + 1) % 4;
    metro.snd = st == 1 || st == 2; metro.vib = st >= 2; metro_save();
    static const char *const nm[4] = {"METRONOME OFF", "METRONOME: SOUND", "METRONOME: SOUND + VIBRATION", "METRONOME: VIBRATION ONLY"}; toast(nm[st]);
}
static int hint_on = 1;                    // HOVER HINTS (hints.cfg beside the binary: "0" = off)
static void hint_set(int on) { hint_on = on; write_whole("hints.cfg", on ? "1\n" : "0\n", 2); }
#include "midi.h"
static void hint_load(void) { FILE *f = fopen("hints.cfg", "r"); if (f) { int v = 1; if (fscanf(f, "%d", &v) == 1) hint_on = v != 0; fclose(f); } }
static Uint32 new_armed;                 // NEW PROJECT asked once, waiting for the second press
// ---- SCENES (2026-11-15, user: "a scene is not tied to a pattern; a new scene is new empty pads"). A scene is a whole
// Song of its own: twelve pads with their sounds, effects, patterns and chains. `song` is the scene that plays; the others
// wait in scn[] with their samples loaded, and the engine takes the next one in on a launch quantum (engine_scene_swap).
// Tempo, key, the master and the returns are the project's (scene_carry). Files: state.bin is the playing scene,
// scenes.bin the others (a slot each); a project file is the playing scene, then a head and all of them.
#define NSCENE 16
#define SCN_FILE "scenes.bin"
#define SCN_MAGIC 0x314E4353u
typedef struct { uint32_t magic; int32_t n, cur, spare; } ScnHead;
static Song *scn[NSCENE]; static Sample *scn_smp[NSCENE][NSLOT];   // scn[scn_cur] only receives the scene when it leaves
static int scn_n = 1, scn_cur, scn_next = -1, scn_play; static uint8_t scn_ready[NSCENE], scn_dirty[NSCENE], scn_hdr;
static void scn_rest(Song *g) { for (int s = 0; s < NSLOT; s++) { g->t[s].on = 0; g->t[s].queued = -1; } }
static Sample *scn_sample(const char *path) {   // one file is held once, however many scenes play it
    if (!path[0]) return NULL;
    for (int s = 0; s < NSLOT; s++) { Sample *x = (Sample *)engine_sample(s); if (x && !strcmp(x->path, path)) { x->ref++; return x; } }
    for (int i = 0; i < scn_n; i++) for (int s = 0; s < NSLOT; s++) { Sample *x = scn_smp[i][s]; if (x && !strcmp(x->path, path)) { x->ref++; return x; } }
    return sample_load(path);
}
static void scn_unload(int i) { for (int s = 0; s < NSLOT; s++) { sample_free(scn_smp[i][s]); scn_smp[i][s] = NULL; } scn_ready[i] = 0; }
static void scn_preload(int i) {          // reads files: a moment
    if (scn_ready[i] || i == scn_cur || !scn[i]) return;
    for (int s = 0; s < NSLOT; s++) scn_smp[i][s] = scn_sample(scn[i]->t[s].path);
    scn_ready[i] = 1;
}
// SCENE MIX SHARED: a pad that has the same sound in another scene (the same file; a synth: its name, else its sound) takes
// the open scene's mix: fader, pan, inserts, sends, duck, normalize. Another sound on the pad keeps that scene's own.
static int scn_same(const Song *a, const Song *b, int s) {
    const Track *x = &a->t[s], *y = &b->t[s];
    if (x->type != y->type) return 0;
    if (x->type == 0 || x->type == 3) return x->path[0] && !strcmp(x->path, y->path);
    if (a->pname[s][0] || b->pname[s][0]) return !strcmp(a->pname[s], b->pname[s]);
    return x->type == 1 ? !memcmp(a->wv[s], b->wv[s], sizeof a->wv[s]) : !memcmp(x->ap, y->ap, sizeof x->ap);
}
static int scn_mix_copy(Song *d, const Song *g, int s) {   // 1 = something changed
    Track *t = &d->t[s]; PadMix *p = &d->mix.pad[s], *q = (PadMix *)&g->mix.pad[s];
    int ch = t->vol != g->t[s].vol || t->pan != g->t[s].pan || memcmp(p->ins, q->ins, sizeof p->ins) || p->send_a != q->send_a || p->send_b != q->send_b
        || p->spare[DK_SRC] != q->spare[DK_SRC] || p->spare[DK_AMT] != q->spare[DK_AMT] || p->spare[PM_NORM] != q->spare[PM_NORM]
        || memcmp(d->mbx[s], g->mbx[s], sizeof d->mbx[s]) || memcmp(d->dkx[s], g->dkx[s], sizeof d->dkx[s]) || memcmp(d->dkc[s], g->dkc[s], sizeof d->dkc[s]);
    if (!ch) return 0;
    t->vol = g->t[s].vol; t->pan = g->t[s].pan; memcpy(p->ins, q->ins, sizeof p->ins); p->send_a = q->send_a; p->send_b = q->send_b;
    p->spare[DK_SRC] = q->spare[DK_SRC]; p->spare[DK_AMT] = q->spare[DK_AMT]; p->spare[PM_NORM] = q->spare[PM_NORM];
    memcpy(d->mbx[s], g->mbx[s], sizeof d->mbx[s]); memcpy(d->dkx[s], g->dkx[s], sizeof d->dkx[s]); memcpy(d->dkc[s], g->dkc[s], sizeof d->dkc[s]);
    return 1;
}
static void scn_mix_sync(void) {          // the open scene's mix goes to the same sounds elsewhere
    if (!ui_mxlink) return;
    for (int i = 0; i < scn_n; i++) if (i != scn_cur && scn[i]) for (int s = 0; s < NSLOT; s++)
        if (scn_same(&song, scn[i], s) && scn_mix_copy(scn[i], &song, s)) scn_dirty[i] = 1;
}
static void scene_kill_tick(void);
static void scene_poll(void) {            // every frame: the engine has taken the next scene in
    Sample *old[NSLOT];
    if (scn_next < 0) scene_kill_tick();   // a deleted open scene goes once the next one plays
    if (scn_next < 0 || !engine_scene_done(old)) return;
    int a = scn_cur, b = scn_next;
    memcpy(scn_smp[a], old, sizeof old); scn_ready[a] = 1; scn_dirty[a] = 1;
    memset(scn_smp[b], 0, sizeof scn_smp[b]); scn_ready[b] = 0;      // the engine holds them now
    scn_cur = b; scn_next = -1; scn_hdr = 1;
    undo_n = undo_top = 0; cap_n = 0; memset(slot_want, 0, sizeof slot_want); memset(slot_bad, 0, sizeof slot_bad); frz_reset();   // undo does not cross scenes
    if (scn_play) { scn_play = 0; if (!engine_playing() && engine_counting() < 0) engine_transport(1); }
    char t[24]; snprintf(t, sizeof t, "SCENE %d", b + 1); toast(t);
}
static void scn_unwait(void) {            // a scene that waits for the bar gives way
    scene_poll(); if (scn_next < 0) return;
    engine_lock(1); int c = engine_scene_cancel(); engine_lock(0);
    if (c) { scn_next = -1; scn_play = 0; } else scene_poll();
}
static void scenes_clear(void) {          // another project: one scene, the playing one
    scn_unwait();
    for (int i = 0; i < NSCENE; i++) { scn_unload(i); free(scn[i]); scn[i] = NULL; scn_dirty[i] = 0; }
    scn_n = 1; scn_cur = 0; scn_hdr = 1; scn[0] = calloc(1, sizeof(Song));
}
static void scene_go(int n) {             // launch: on the quantum while playing, at once when stopped
    scn_unwait();
    if (n < 0 || n >= scn_n || n == scn_cur || scn_next >= 0 || !scn[n] || !scn[scn_cur]) return;
    engine_queue_stop(0);
    scn_mix_sync(); scn_preload(n);
    static Song in; in = *scn[n]; int any = 0;
    for (int s = 0; s < NSLOT; s++) { Track *t = &in.t[s]; t->queued = -1; t->cur = (int8_t)clampi(t->cur, 0, NPAT - 1);
        t->on = (t->type || scn_smp[n][s]) && pat_used(&t->pat[t->cur]); any |= t->on; }
    scn_play = any && !engine_playing() && engine_counting() < 0;
    scn_next = n; engine_scene_swap(&in, scn[scn_cur], scn_smp[n]);
}
static void scene_step(int d) { scene_go(clampi((scn_next >= 0 ? scn_next : scn_cur) + d, 0, scn_n - 1)); }
static void scene_new(int src) {          // src < 0: empty pads; else a copy of that scene
    if (scn_n >= NSCENE) { toast("16 SCENES AT MOST"); return; }
    if (mem_avail_mb() < 120) { toast("NOT ENOUGH MEMORY"); return; }
    if (src >= 0 && src != scn_cur) scn_preload(src);
    Song *g = malloc(sizeof *g); if (!g) return;
    int i = scn_n; memset(scn_smp[i], 0, sizeof scn_smp[i]);
    if (src < 0) { static Song keep; engine_lock(1); keep = song; song_defaults(); *g = song; song = keep; engine_lock(0); scene_carry(g, &song); }
    else { *g = src == scn_cur || !scn[src] ? song : *scn[src]; scene_carry(g, &song);
        for (int s = 0; s < NSLOT; s++) { Sample *x = src == scn_cur ? (Sample *)engine_sample(s) : scn_smp[src][s]; if (x) x->ref++; scn_smp[i][s] = x; } }
    scn_rest(g); scn[i] = g; scn_ready[i] = 1; scn_dirty[i] = 1; scn_hdr = 1; scn_n++;
    char t[32]; if (src < 0) snprintf(t, sizeof t, "SCENE %d \003 EMPTY", i + 1); else snprintf(t, sizeof t, "SCENE %d \003 COPY OF %d", i + 1, src + 1); toast(t);
}
static void scene_del(int i) {
    scn_unwait();
    if (i < 0 || i >= scn_n) return;
    if (i == scn_cur || scn_n < 2) { toast("THIS SCENE IS OPEN"); return; }
    scn_unload(i); free(scn[i]);
    for (int k = i; k < scn_n - 1; k++) { scn[k] = scn[k + 1]; memcpy(scn_smp[k], scn_smp[k + 1], sizeof scn_smp[k]); scn_ready[k] = scn_ready[k + 1]; }
    scn_n--; scn[scn_n] = NULL; memset(scn_smp[scn_n], 0, sizeof scn_smp[scn_n]); scn_ready[scn_n] = 0;
    if (scn_cur > i) scn_cur--;
    memset(scn_dirty, 1, sizeof scn_dirty); scn_hdr = 1; toast("SCENE DELETED");
}
static void scene_save(void) {            // only what changed: every scene has its own place in the file
    scn_mix_sync();
    int any = scn_hdr; for (int i = 0; i < scn_n; i++) any |= scn_dirty[i] && i != scn_cur;
    if (!any) return;
    FILE *f = fopen(SCN_FILE, "r+b"); if (!f) f = fopen(SCN_FILE, "w+b"); if (!f) return;
    ScnHead h = {SCN_MAGIC, scn_n, scn_cur, (int32_t)sizeof(Song)}; int ok = fwrite(&h, sizeof h, 1, f) == 1;
    for (int i = 0; i < scn_n && ok; i++) if (scn_dirty[i] && i != scn_cur && scn[i])
        ok = !fseek(f, (long)sizeof h + (long)i * (long)sizeof(Song), SEEK_SET) && fwrite(scn[i], sizeof(Song), 1, f) == 1;
    if (fflush(f) || fsync(fileno(f))) ok = 0; fclose(f);
    if (ok) { memset(scn_dirty, 0, sizeof scn_dirty); scn_hdr = 0; }
}
static int scene_up(Song *g, long one) { (void)one; wave_fix(g); return 0; }   // always 0 (older formats do not open since 2026-11-25)
static void scenes_read(const char *path, long off) {   // after the playing scene was loaded: the others, from this file
    scenes_clear();
    FILE *f = fopen(path, "rb"); if (!f) return;
    ScnHead h;
    if (fseek(f, off, SEEK_SET) || fread(&h, sizeof h, 1, f) != 1 || h.magic != SCN_MAGIC || h.n < 1 || h.n > NSCENE || h.cur < 0 || h.cur >= h.n) { fclose(f); return; }
    free(scn[0]); scn[0] = NULL; int n = 0;
    long one = h.spare;                  // spare: the size of a scene
    if (one != (long)sizeof(Song)) { fclose(f); return; }
    for (; n < h.n; n++) {
        if (!(scn[n] = calloc(1, sizeof(Song)))) break;
        if (n == h.cur) continue;
        if (fseek(f, off + (long)sizeof h + (long)n * one, SEEK_SET) || fread(scn[n], (size_t)one, 1, f) != 1 || scene_up(scn[n], one) || scn[n]->magic != song.magic) {
            if (n > h.cur) { free(scn[n]); scn[n] = NULL; } else { h.cur = n; n++; }   // a cut file: the scenes before it stay
            break; }
        scene_carry(scn[n], &song); scn_rest(scn[n]);
    }
    fclose(f);
    if (n < 1) { scn[0] = calloc(1, sizeof(Song)); n = 1; h.cur = 0; }
    scn_n = n; scn_cur = h.cur < n ? h.cur : n - 1; scn_hdr = 1;
    if (off || one != (long)sizeof(Song)) memset(scn_dirty, 1, sizeof scn_dirty);     // a project's scenes become the working copy's; an old file is written again whole
    for (int i = 0; i < scn_n; i++) if (mem_avail_mb() > 200) scn_preload(i);
    logmsg("scenes: %d, open %d", scn_n, scn_cur + 1);
}
static char prj_cur[32];                  // the open project's name ("" = not saved under a name yet)
static void prj_cfg_write(void); static void prj_mark_saved_empty(void);
static void new_project(void) {
    int real = 0;                                // an empty project must not replace the copy of the last real one
    for (int s = 0; s < NSLOT; s++) if (song.t[s].path[0]) real = 1;
    if (real) write_whole(STATE ".prev", &song, sizeof song);   // the only copy of the old project besides undo
    push_undo();
    rec = 0; combo_t = 0;
    if (engine_playing() || engine_counting() >= 0) transport_toggle();
    engine_stop_all();
    cap_n = 0; spage = 0; sel = 0; memset(slot_want, 0, sizeof slot_want); memset(slot_bad, 0, sizeof slot_bad);   // nothing of the old project is missed in the new one
    int master = song.master;
    song_defaults(); song.master = master;       // same loudness as before
    scenes_clear();
    sync_samples(); frz_reset();
    save_state();
    prj_cur[0] = 0; prj_cfg_write(); prj_mark_saved_empty();
    toast("NEW PROJECT");
}
static const char *quant_name[5] = {"CLIP STEP", "1/32", "1/16", "1/8", "1/4"};
static int prj_dirty(void);
static int jcur, jsec, jcol;   // jcol 0: the sections, 1: the settings of jsec

static void leave_screen(void) {         // what a key held across a screen change must not carry over
    tempo_mul = 1.f; memset(key_on, 0, sizeof key_on); browse_hold = 0;
    if (held[K_EDIT]) used[K_EDIT] = 1;
}
// ---------- themes: a setting of the Brick, not of the project (theme.cfg), so NEW PROJECT and undo leave it alone ----------
#define THEME_FILE "theme.cfg"
#define THEME_DIR "themes"
// Themes (2026-11-11, user): the built-in ones are read-only; the user's own are files themes/NAME.thm, as many as
// wanted, made with SAVE / SAVE AS in the colour editor (the M8 name editor); DELETE removes one. theme.cfg keeps the
// name of the theme in use. Colours changed but not saved stay on the screen until another theme is chosen.
static const char *th_preset_name[TH_PRESETS] = {"COSMIC", "MATRIX", "BLOOD CONCRETE", "AMBER", "GAMEBOY"};
static const char *th_color_name[TC_N] = {"BACKGROUND", "PANEL", "LINE", "FAINT", "DIM TEXT", "TEXT", "ACCENT", "WARN", "SELECTION", "PLAYING",
    "SAMPLER", "WAVE", "ACID", "CLOUD", "PATTERN", "SOUND", "FX", "MOD", "MIX"};
#define TH_MAXU 64
typedef struct { char name[20]; Col c[TC_N]; } UserTheme;
static UserTheme th_usr[TH_MAXU]; static int th_nu, th_sel, th_dirty, th_mod;   // th_mod: the colours on screen differ from the saved theme
static int ted_row, ted_col; static Uint32 ted_armed;
static int th_count(void) { return TH_PRESETS + th_nu; }
static const char *theme_name(int i) { return i < TH_PRESETS ? th_preset_name[i] : th_usr[i - TH_PRESETS].name; }
static const Col *theme_colors(int i) { return i < TH_PRESETS ? th_preset[i] : th_usr[i - TH_PRESETS].c; }
static int th_lum(Col c) { return (c.r * 3 + c.g * 6 + c.b) / 10; }
static int theme_legible(const Col *t) {   // the screen must stay readable, or there is no way back from the screen itself
    int bg = th_lum(t[TC_BG]);
    return abs(th_lum(t[TC_TEXT]) - bg) >= 64 && abs(th_lum(t[TC_ACCENT]) - bg) >= 64;
}
static void theme_apply(void) {
    const Col *t = theme_colors(th_sel);
    memcpy(th_cur, t, sizeof th_cur); th_mod = 0;
}
static int th_cmp(const void *a, const void *b) { return strcmp(((const UserTheme *)a)->name, ((const UserTheme *)b)->name); }
static int theme_read(const char *path, Col *c) {   // a file from before a colour was added keeps COSMIC's for it
    FILE *f = fopen(path, "r"); if (!f) return 0; int got = 0, v[3];
    memcpy(c, th_preset[0], sizeof(Col) * TC_N);
    for (; got < TC_N && fscanf(f, "%d %d %d", &v[0], &v[1], &v[2]) == 3; got++) c[got] = (Col){(Uint8)clampi(v[0], 0, 255), (Uint8)clampi(v[1], 0, 255), (Uint8)clampi(v[2], 0, 255)};
    fclose(f); return got >= 9 && theme_legible(c);
}
static void theme_scan(void) {             // the .thm files of one small folder
    th_nu = 0; DIR *d = opendir(THEME_DIR); if (!d) return; struct dirent *e;
    while ((e = readdir(d)) && th_nu < TH_MAXU) {
        const char *x = strrchr(e->d_name, '.'); if (!x || strcmp(x, ".thm") || x == e->d_name || x - e->d_name > 19) continue;
        char p[300]; snprintf(p, sizeof p, THEME_DIR "/%s", e->d_name); UserTheme *u = &th_usr[th_nu];
        if (!theme_read(p, u->c)) continue;
        snprintf(u->name, sizeof u->name, "%.*s", (int)(x - e->d_name), e->d_name); th_nu++;
    }
    closedir(d); qsort(th_usr, (size_t)th_nu, sizeof th_usr[0], th_cmp);
}
static int theme_find(const char *name) { for (int i = 0; i < th_count(); i++) if (!strcmp(theme_name(i), name)) return i; return -1; }
static int theme_write(const char *name, const Col *c) {
    mkdir(THEME_DIR, 0777); char p[300], b[400]; int n = 0; snprintf(p, sizeof p, THEME_DIR "/%s.thm", name);
    for (int i = 0; i < TC_N; i++) n += snprintf(b + n, sizeof b - n, "%d %d %d\n", c[i].r, c[i].g, c[i].b);
    return write_whole(p, b, (size_t)n);
}
static void theme_load(void) {
    theme_scan(); th_sel = 0;
    FILE *f = fopen(THEME_FILE, "r");
    if (f) {
        char line[64] = ""; if (!fgets(line, sizeof line, f)) line[0] = 0;
        if (!strncmp(line, "name ", 5)) { line[strcspn(line, "\r\n")] = 0; int i = theme_find(line + 5); if (i >= 0) th_sel = i; }
        else {                           // before 2026-11-11: "theme N" and three "user USED 27 numbers": the used ones become files; the first built-in is the default
            for (int u = 0; u < 3; u++) {
                int used = 0, v[27], ok = fscanf(f, " user %d", &used) == 1;
                for (int i = 0; ok && i < 27; i++) ok = fscanf(f, "%d", &v[i]) == 1 && v[i] >= 0 && v[i] <= 255;
                if (!ok) break; if (!used) continue;
                Col c[TC_N]; memcpy(c, th_preset[0], sizeof c); for (int i = 0; i < 9; i++) c[i] = (Col){(Uint8)v[i * 3], (Uint8)v[i * 3 + 1], (Uint8)v[i * 3 + 2]};
                char nm[20]; snprintf(nm, sizeof nm, "USER %d", u + 1); if (theme_legible(c)) theme_write(nm, c);
            }
            theme_scan(); th_dirty = 1;
        }
        fclose(f);
    }
    theme_apply();
}
static void theme_save(void) {             // which theme is in use
    if (!th_dirty) return;
    char b[64]; int n = snprintf(b, sizeof b, "name %s\n", theme_name(th_sel));
    if (write_whole(THEME_FILE, b, (size_t)n)) th_dirty = 0;
}
static void theme_pick(int d) { th_sel = (th_sel + d + th_count()) % th_count(); theme_apply(); th_dirty = 1; }
static void theme_change(int d) {          // the colours on screen change; SAVE keeps them
    Col *c = &th_cur[ted_row], old = *c;
    Uint8 *v = ted_col == 0 ? &c->r : ted_col == 1 ? &c->g : &c->b;
    *v = (Uint8)clampi(*v + d, 0, 255);
    if (!theme_legible(th_cur)) { *c = old; toast("TEXT WOULD DISAPPEAR"); return; }
    th_mod = 1;
}
static void theme_save_as(const char *name) {
    char nm[20]; snprintf(nm, sizeof nm, "%.19s", name);
    for (int i = 0; i < TH_PRESETS; i++) if (!strcmp(nm, th_preset_name[i])) { toast("A BUILT-IN NAME: SAVE AS ANOTHER"); return; }
    Col c[TC_N]; memcpy(c, th_cur, sizeof c);
    if (theme_find(nm) < 0 && th_nu >= TH_MAXU) { toast("64 THEMES ALREADY: DELETE ONE"); return; }
    if (!theme_write(nm, c)) { toast("COULD NOT SAVE THE THEME"); return; }
    sync(); theme_scan(); int i = theme_find(nm); if (i >= 0) th_sel = i;
    memcpy(th_cur, c, sizeof c); th_mod = 0; th_dirty = 1; toast("THEME SAVED");
}
static void naming_theme(void);
static void theme_action(int a) {          // the editor's bottom row: 0 SAVE, 1 SAVE AS, 2 DELETE
    if (a == 0 && th_sel >= TH_PRESETS) { char nm[20]; snprintf(nm, sizeof nm, "%s", theme_name(th_sel)); theme_save_as(nm); return; }
    if (a <= 1) { naming_theme(); return; }
    if (th_sel < TH_PRESETS) { toast("BUILT-IN THEMES STAY"); return; }
    Uint32 now = SDL_GetTicks();
    if (!ted_armed || ms_since(now, ted_armed) >= 3000) { ted_armed = now | 1; toast("EDIT AGAIN: DELETE THIS THEME"); return; }
    ted_armed = 0; char p[300]; snprintf(p, sizeof p, THEME_DIR "/%s.thm", theme_name(th_sel)); unlink(p); sync();
    theme_scan(); th_sel = 0; theme_apply(); th_dirty = 1; toast("THEME DELETED");
}
static void theme_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT] && ted_row == TC_N) theme_action(ted_col); return; }
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) {   // this colour as it is saved
        used[K_EDIT] = used[K_OPT] = 1;
        if (ted_row < TC_N) { th_cur[ted_row] = theme_colors(th_sel)[ted_row]; th_mod = memcmp(th_cur, theme_colors(th_sel), sizeof th_cur) != 0; toast("THE SAVED COLOUR AGAIN"); }
        return;
    }
    if (k == K_Y) { if (th_mod) toast("NOT SAVED: THESE COLOURS LAST UNTIL ANOTHER THEME"); leave_screen(); theming = 0; return; }
    if (k == K_PLAY) { play_key(); return; }
    if (k > K_RIGHT) return;
    if (held[K_EDIT] && ted_row < TC_N) { used[K_EDIT] = 1; theme_change(k == K_LEFT ? -1 : k == K_RIGHT ? 1 : k == K_UP ? 16 : -16); }
    else if (held[K_EDIT]) used[K_EDIT] = 1;
    else if (k == K_UP || k == K_DOWN) { ted_row = clampi(ted_row + (k == K_DOWN ? 1 : -1), 0, TC_N); ted_armed = 0; }
    else ted_col = clampi(ted_col + (k == K_RIGHT ? 1 : -1), 0, 2);
}

static void prj_goto(int j) {           // a setting by its id: its section, the cursor on it
    for (int k = 0; k < JS_N; k++) for (int i = 0; i < JS_ROWS && prj_sec[k][i] >= 0; i++) if (prj_sec[k][i] == j) { jsec = k; jcol = 1; jcur = j; return; }
}
static int prj_rows(int sc, int *r) { int n = 0; while (n < JS_ROWS && prj_sec[sc][n] >= 0) { r[n] = prj_sec[sc][n]; n++; } return n; }
static void prj_move(int k) {
    int r[JS_ROWS], n = prj_rows(jsec, r), i = 0; while (i < n && r[i] != jcur) i++; if (i >= n) { i = 0; jcur = r[0]; }
    if (!jcol) {                         // the sections: UP/DOWN choose, RIGHT goes in
        if (k == K_UP || k == K_DOWN) { jsec = clampi(jsec + (k == K_DOWN ? 1 : -1), 0, JS_N - 1); prj_rows(jsec, r); jcur = r[0]; }
        else if (k == K_RIGHT) jcol = 1;
        return;
    }
    if (jsec == JS_HELP && k == K_UP && ab_pg > 0) { ab_pg--; return; }   // ABOUT: the arrows turn its topics
    if (jsec == JS_HELP && k == K_DOWN) { if (ab_pg < AB_N - 1) ab_pg++; return; }
    if (k == K_LEFT) jcol = 0;
    else if (k == K_UP && i > 0) jcur = r[i - 1];
    else if (k == K_DOWN && i < n - 1) jcur = r[i + 1];
    else if (k == K_UP && jsec > 0) { jsec--; n = prj_rows(jsec, r); jcur = r[n - 1]; }    // past the top: the section above
    else if (k == K_DOWN && jsec < JS_N - 1) { jsec++; prj_rows(jsec, r); jcur = r[0]; }   // past the bottom: the one below
}
static int mcol;
#include "actions.h"
static void prj_edit_arrow(int k) {
    int d = (k == K_RIGHT || k == K_UP) ? 1 : -1, big = k == K_UP || k == K_DOWN;
    if (!jcol) return;                   // on the sections: the arrows only move
    if (jcur == J_LIB || jcur == J_NEW || jcur == J_ABOUT || (jcur >= J_PROJ && jcur < J_HINT)) return;   // actions, not values (SCALE SNAP is a value)
    if (jcur == J_NAV) { ui_tree = !ui_tree; ui_cfg_save(); return; }
    if (jcur == J_DRAWER) { ui_drawer_left = !ui_drawer_left; ui_cfg_save(); return; }
    if (jcur == J_MXFX) { ui_mxfx = !ui_mxfx; ui_cfg_save(); return; }
    if (jcur == J_MXLINK) { ui_mxlink = !ui_mxlink; ui_cfg_save(); return; }
    if (jcur == J_MPORT) { midi_pick(d); return; }
    if (jcur == J_MCLK) { midi_clk_out = !midi_clk_out; midi_cfg_save(); return; }
    if (jcur == J_MCLKIN) { midi_clk_in = !midi_clk_in; if (!midi_clk_in) tempo_mul = 1.f; midi_cfg_save(); return; }
    if (jcur == J_TRIM) { trim_db = clampi(trim_db + d * (big ? 6 : 1), 0, 24); trim_save(); return; }
    if (jcur >= J_MSND) {                // METRONOME: device settings, no undo step
        switch (jcur) {
        case J_MSND: metro.snd = !metro.snd; break; case J_MVIB: metro.vib = !metro.vib; break;
        case J_MVOL: metro.vol = (int8_t)clampi(metro.vol + d * (big ? 10 : 5), 0, 100); break;
        case J_MTONE: metro.tone = (int8_t)((metro.tone + d + 3) % 3); break; case J_MWHEN: metro.when = !metro.when; break;
        case J_MDIV: metro.div = (int8_t)((metro.div + d + 3) % 3); break; case J_MVOFF: vib_off = clampi(vib_off + d * (big ? 10 : 2), -80, 80); break;
        }
        metro_save(); return; }
    if (jcur == J_MNOTE) { midi_notes = !midi_notes; midi_cfg_save(); return; }
    if (jcur == J_MCH) { midi_ch = (midi_ch + d * (big ? 4 : 1) + 17) % 17; midi_cfg_save(); return; }
    if (jcur == J_STRIP) { ui_strip = (ui_strip + 1) % 3; ui_cfg_save(); return; }
    if (jcur == J_QBAR) { ui_qbar = !ui_qbar; ui_cfg_save(); return; }
    if (jcur == J_SECS) { ui_secs = !ui_secs; ui_cfg_save(); return; }
    if (jcur == J_VIEW) { ui_simple = !ui_simple; ui_cfg_save(); return; }
    if (jcur == J_LOCK) { ui_lock = !ui_lock; ui_cfg_save(); return; }
    if (jcur == J_HINT) { hint_set(!hint_on); return; }
    if (jcur == J_THEME) { theme_pick(d); return; }
    edit_begin();
    switch (jcur) {
    case J_TEMPO: act_tempo_step(d * (big ? 10 : 1)); break;
    case J_SWING: act_swing_step(d * (big ? 10 : 1)); break;
    case J_LAUNCH: act_launch_step(d); break;
    case J_METRO: act_metro_toggle(); break;
    case J_SNAP: act_snap_toggle(); break;
    case J_HINT: hint_set(!hint_on); break;
    case J_STRIP: ui_strip = (ui_strip + (d > 0 ? 1 : 2)) % 3; ui_cfg_save(); break;
    case J_QBAR: ui_qbar = !ui_qbar; ui_cfg_save(); break;
    case J_SECS: ui_secs = !ui_secs; ui_cfg_save(); break;
    case J_VIEW: ui_simple = !ui_simple; ui_cfg_save(); break;
    case J_LOCK: ui_lock = !ui_lock; ui_cfg_save(); break;
    case J_KEY: act_key_step(d * (big ? 7 : 1)); break;
    case J_SCALE: act_scale_step(d); break;
    case J_BEATS: act_beats_step(d); break;
    case J_COUNT: act_count_in_step(d); break;
    case J_QUANT: act_rec_quant_step(d); break;
    case J_MASTER: act_master_step(d * (big ? 10 : 1)); break;
    }
}
static void export_open(void);
static int prj_dirty(void); static int prj_changed(void); static void prj_browse(void); static void prj_save(const char *name); static void naming_open(int then); static void ask_open(int act, const char *path);
// TAP TEMPO (2026-11-12, as on the M8): EDIT taps in time. 2026-11-19 (user: "tap is not exact, average over more"):
// up to 12 taps, the tempo is the straight line through all of them (least squares), so one late tap moves it little.
// A pause over 2 s, or a gap far off the others (under half or over twice the mean), starts again from that tap.
#define TAP_N 12
static void qp_open(void); static void qb_ready(void); static void qb_defaults(void); static void qb_save(void); static int qb_n;
static int tap_bpm(Uint32 *t, int *n) {
    Uint32 now = SDL_GetTicks();
    if (*n) { Uint32 g = now - t[*n - 1]; double mean = *n > 1 ? (double)(t[*n - 1] - t[0]) / (*n - 1) : g;
        if (g > 2000 || (*n > 1 && (g < mean * 0.5 || g > mean * 2))) *n = 0; }
    if (*n == TAP_N) { memmove(t, t + 1, (TAP_N - 1) * sizeof *t); *n = TAP_N - 1; }
    t[(*n)++] = now;
    if (*n < 3) return 0;               // two gaps at least
    double sx = 0, sy = 0, sxx = 0, sxy = 0; int m = *n;
    for (int i = 0; i < m; i++) { double y = (double)(t[i] - t[0]); sx += i; sy += y; sxx += (double)i * i; sxy += i * y; }
    double per = (m * sxy - sx * sy) / (m * sxx - sx * sx);   // ms a beat
    return per > 0 ? (int)lrint(60000.0 / per) : 0;
}
static void prj_tap(void) {
    if (!jcol) { jcol = 1; return; }     // EDIT on a section: into its settings
    if (jcur == J_TEMPO) { static Uint32 tt[TAP_N]; static int tn; int b = tap_bpm(tt, &tn); char m[40];
        if (b) { edit_begin(); song.tempo = (int16_t)clampi(b, 40, 300); snprintf(m, sizeof m, "TAP  %d BPM  (%d TAPS)", song.tempo, tn); } else snprintf(m, sizeof m, "TAP: GO ON IN TIME");
        toast(m); return; }
    if (jcur == J_ABOUT) { ab_pg = (ab_pg + 1) % AB_N; return; }
    if (jcur == J_PROJ) { prj_browse(); return; }
    if (jcur == J_SAVE) { if (prj_cur[0]) prj_save(prj_cur); else naming_open(0); return; }
    if (jcur == J_SAVEAS) { naming_open(0); return; }
    if (jcur == J_QBTN) { qp_open(); return; }
    if (jcur == J_EXPORT) { export_open(); return; }
    if (jcur == J_LIB) { lib_toggle(); return; }
    if (jcur == J_THEME) { theming = 1; ted_armed = 0; return; }
    if (jcur == J_NEW) {
        Uint32 now = SDL_GetTicks();
        if (new_armed && ms_since(now, new_armed) < 3000) { new_armed = 0; if (prj_changed()) ask_open(2, ""); else new_project(); }
        else { new_armed = now | 1; toast("EDIT AGAIN: ERASE THIS PROJECT"); }
        return;
    }
    if (jcur == J_MSND || jcur == J_MVIB || jcur == J_MTONE || jcur == J_MWHEN || jcur == J_MDIV || jcur == J_LAUNCH || jcur == J_METRO || jcur == J_COUNT || jcur == J_QUANT || jcur == J_SNAP || jcur == J_HINT || jcur == J_STRIP || jcur == J_VIEW || jcur == J_LOCK || jcur == J_NAV || jcur == J_DRAWER || jcur == J_MXFX || jcur == J_MXLINK || jcur == J_MCLK || jcur == J_MCLKIN || jcur == J_MNOTE || jcur == J_MCH) prj_edit_arrow(K_RIGHT);
    else if (jcur == J_MPORT) { if (midi_want[0] && midi_open()) toast("MIDI: CONNECTED"); else if (midi_want[0]) toast("MIDI: THE DEVICE IS NOT HERE"); else { midi_pick(1); toast(midi_want[0] ? "MIDI: CONNECTED" : "NO MIDI DEVICE FOUND"); } }
}

// ---------- SLICE EDITOR ----------
// Markers: 0 START, 1..cnt-1 slice points, cnt END. The first change of a point makes the slicing hand-edited (EDIT).
#define WAVE_W 992
static int ed_m, ed_zoom, ed_view, opt_gate;   // opt_gate: a listen that lasts while OPTION is held
static int ed_cnt(void) { int n = track_nslices(sel); return n < 1 ? 1 : n; }
static int ed_start(const Sample *s) { return (int)((double)s->n * song.t[sel].start / 1000); }
static int ed_end(const Sample *s) { return (int)((double)(s->n - 1) * song.t[sel].end / 1000); }
static int ed_pos(const Sample *s, int m) { return m <= 0 ? ed_start(s) : m >= ed_cnt() ? ed_end(s) : track_slice_pos(sel, m); }
static int ed_span(const Sample *s) { int sp = s->n >> ed_zoom; if (sp < 256) sp = 256; return sp > s->n ? s->n : sp; }
static void cuts_own(const Sample *s) {          // current slicing -> hand-edited points
    Track *t = &song.t[sel]; Cuts *c = &song.cut[sel];
    if (t->slices == 6) return;
    int n = track_nslices(sel), a = ed_start(s), b = ed_end(s), m = 1;
    c->pos[0] = 0;
    for (int k = 1; k < n; k++) { int p = track_slice_pos(sel, k); if (p > a + 64 && p < b - 64) c->pos[m++] = p; }
    c->n = m; t->slices = 6;
}
static void pats_explicit(int n) {               // steps "in order" keep playing the same slice when the numbers shift
    Track *t = &song.t[sel];
    for (int q = 0; q < NPAT; q++) for (int st = 0; st < NSTEP; st++)
        if (t->pat[q].trig[st] && t->pat[q].slice[st] < 0) t->pat[q].slice[st] = st % n;
}
static void ed_split(const Sample *s) {          // EDIT tap: new point in the middle of the slice at the cursor
    Track *t = &song.t[sel]; Cuts *c = &song.cut[sel];
    int k = clampi(ed_m, 0, ed_cnt() - 1), a = ed_pos(s, k), b = ed_pos(s, k + 1);
    if (b - a < 128) { toast("SLICE TOO SHORT"); return; }
    edit_begin();
    cuts_own(s);
    if (c->n >= 32) { toast("32 SLICES MAX"); return; }
    k = clampi(k, 0, c->n - 1); a = ed_pos(s, k); b = ed_pos(s, k + 1);
    pats_explicit(c->n);
    for (int q = 0; q < NPAT; q++) for (int st = 0; st < NSTEP; st++) if (t->pat[q].slice[st] > k) t->pat[q].slice[st]++;
    uint32_t mask = (uint32_t)c->pos[0], low = mask & ((1u << (k + 1)) - 1), high = k + 2 < 32 ? (mask >> (k + 1)) << (k + 2) : 0;
    for (int i = c->n - 1; i > k; i--) c->pos[i + 1] = c->pos[i];
    c->pos[k + 1] = a + (b - a) / 2;
    c->pos[0] = (int32_t)(low | high | (1u << (k + 1)));   // the new part plays right after its parent: no step needed, rhythm kept
    c->n++;
    ed_m = k + 1;
    if (!pat_used(cur_pat(sel))) relayout(sel);
}
static void ed_delete(const Sample *s) {         // OPTION+EDIT: remove a point (its slice joins the previous one), reset START/END
    Track *t = &song.t[sel]; Cuts *c = &song.cut[sel];
    int m = ed_m;
    edit_begin();
    if (m <= 0) { t->start = 0; return; }
    if (m >= ed_cnt()) { t->end = 1000; return; }
    cuts_own(s);
    if (m >= c->n) return;
    pats_explicit(c->n);
    for (int q = 0; q < NPAT; q++) for (int st = 0; st < NSTEP; st++) {
        Pattern *p = &t->pat[q];
        if (p->slice[st] == m) { p->trig[st] = 0; p->slice[st] = -1; }   // the previous slice now plays through it
        else if (p->slice[st] > m) p->slice[st]--;
    }
    uint32_t mask = (uint32_t)c->pos[0], low = mask & ((1u << m) - 1), high = m < 31 ? (mask >> (m + 1)) << m : 0;
    for (int i = m; i + 1 < c->n; i++) c->pos[i] = c->pos[i + 1];
    c->pos[0] = (int32_t)(low | high);
    c->n--; ed_m = m - 1;
}
static void ed_move(const Sample *s, int dir, int big) {   // EDIT+arrows: one screen pixel, or 16 with up/down
    Track *t = &song.t[sel]; Cuts *c = &song.cut[sel];
    int cnt = ed_cnt(), px = ed_span(s) / WAVE_W, step = (px < 1 ? 1 : px) * (big ? 16 : 1);
    edit_begin();
    if (ed_m <= 0 || ed_m >= cnt) {              // START/END: per mille of the sample
        if (t->slices == 5) { cuts_own(s); cnt = ed_cnt(); ed_m = ed_m <= 0 ? 0 : cnt; }
        int dm = (int)((double)step * 1000 / s->n), own = t->slices == 6 && cnt > 1;
        if (dm < 1) dm = 1;
        if (ed_m == 0) t->start = clampi(t->start + dir * dm, 0, own ? (int)((double)c->pos[1] * 1000 / s->n) : t->end - 5);
        else t->end = clampi(t->end + dir * dm, own ? (int)((double)c->pos[cnt - 1] * 1000 / s->n) + 1 : t->start + 5, 1000);
    } else {
        int m = ed_m;
        cuts_own(s);
        cnt = ed_cnt(); if (m >= cnt) { ed_m = cnt; return; }
        c->pos[m] = clampi(c->pos[m] + dir * step, ed_pos(s, m - 1) + 64, ed_pos(s, m + 1) - 64);
    }
}
static void slice_key(int k, int down) {
    const Sample *s = engine_sample(sel);
    if (!s) { slicing = 0; return; }
    int cnt = ed_cnt(); ed_m = clampi(ed_m, 0, cnt);
    if (!down) {
        if (k == K_EDIT) { if (!used[K_EDIT]) ed_split(s); undo_armed = 0; }
        return;
    }
    if (held[K_SHIFT] && k == K_L2) { undo(); return; }
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) { used[K_EDIT] = used[K_OPT] = 1; ed_delete(s); return; }
    if (k == K_OPT) { int sl = track_nslices(sel) ? clampi(ed_m, 0, cnt - 1) : -1; engine_trig_held(sel, 1.f, 0, sl); opt_gate = 1; flash[sel] = SDL_GetTicks(); rec_hit(sel, sl, NOTE_NONE); return; }
    if (k == K_Y) { leave_screen(); slicing = 0; return; }
    if (k == K_PLAY) { play_key(); return; }
    if (k > K_RIGHT) return;
    if (held[K_EDIT]) { used[K_EDIT] = 1; ed_move(s, k == K_RIGHT || k == K_UP ? 1 : -1, k == K_UP || k == K_DOWN); return; }
    if (k == K_LEFT || k == K_RIGHT) ed_m = clampi(ed_m + (k == K_RIGHT ? 1 : -1), 0, cnt);
    else ed_zoom = clampi(ed_zoom + (k == K_UP ? 1 : -1), 0, 10);
}

// ---------- generic M8-style actions per tab ----------
static void mix_move(int k); static void fx_move(int k); static void mix_arrow(int k); static void fx_value(int d, int big);   // MIXER and FX, below
static void mix_tap(void); static void fx_tap(void); static void mix_delete(void); static void fx_delete(void);
static void move_cursor(int k) {
    switch (tab) {
    case T_GRID:
        if (k == K_LEFT && sel % 4) sel--; else if (k == K_RIGHT && sel % 4 < 3) sel++;
        else if (k == K_UP && sel >= 4) sel -= 4; else if (k == K_DOWN && sel < 8) sel += 4;
        break;
    case T_SEQ: seq_move(k); break;
    case T_PAT: {                        // SELECT (UI v2): the grid 2 x 8, LENGTH DIVISION MODE, the four FOLLOW cards
        static const int cols[5] = {8, 8, 3, 2, 2}; int oc = cols[sl_r];
        if (k == K_LEFT) { if (sl_c) sl_c--; } else if (k == K_RIGHT) { if (sl_c < oc - 1) sl_c++; }
        else { int nr = sl_r + (k == K_DOWN ? 1 : -1); if (nr < 0 || nr > 4) break;
               sl_c = sl_c * cols[nr] / oc; sl_r = nr; }   // the card under or over this one
        if (sl_r < 2) pcur = sl_r * 8 + sl_c;
        pfol = sl_r >= 2;
        break; }
    case T_PRJ: prj_move(k); break;
    case T_MIX: mix_move(k); break;
    case T_FX: fx_move(k); break;
    }
}
static void select_value(int d, int big) {   // SELECT: EDIT+arrows on a card (<> small, ^v big), about the chosen pattern
    Track *t = &song.t[sel]; int pt = fol_pat(); const uint8_t *fo = song.fol[sel][pt];
    if (sl_r == 2) {
        if (sl_c == 0) act_pattern_len_step(sel, pt, d * (big ? 4 : 1));
        else if (sl_c == 1) act_pattern_div_step(sel, pt, d);
        else {                           // MODE: the pad must be on this pattern
            if (pt != t->cur && engine_playing() && t->on) { toast("PLAY THIS PATTERN FIRST: EDIT ON IT TWICE"); return; }
            if (pt != t->cur) pat_switch(pt);
            roll_mode();
        }
        return;
    }
    switch ((sl_r - 3) * 2 + sl_c) {     // REPEAT THEN / CHANCE ELSE
    case 0: act_follow_repeat_step(sel, pt, d * (big ? 4 : 1)); break;
    case 1: if (big && fo[FO_A] == FA_JUMP) act_follow_jump_step(sel, pt, d); else act_follow_action_step(sel, pt, 0, d); break;
    case 2: act_follow_else_chance_step(sel, pt, -d * (big ? 10 : 5)); break;   // CHANCE of THEN: the rest goes to ELSE
    case 3: if (big && fo[FO_B] == FA_JUMP) act_follow_jump_step(sel, pt, d); else act_follow_action_step(sel, pt, 1, d); break;
    }
}
static void edit_arrow(int k) {
    if (tab == T_SEQ) seq_edit_arrow(k);
    else if (tab == T_PRJ) prj_edit_arrow(k);
    else if (tab == T_MIX) mix_arrow(k);
    else if (tab == T_PAT) { if (pfol) select_value(k == K_RIGHT || k == K_UP ? 1 : -1, k == K_UP || k == K_DOWN); }
    else if (tab == T_FX) fx_value(k == K_RIGHT || k == K_UP ? 1 : -1, k == K_UP || k == K_DOWN);
    else if (tab == T_GRID && (k == K_LEFT || k == K_RIGHT)) {   // slot volume
        act_pad_volume_step(sel, k == K_RIGHT ? 5 : -5);
        char b[32]; snprintf(b, sizeof b, "VOLUME %d", song.t[sel].vol); toast(b);
    }
}
static void hub_open(void);
static void edit_tap(void) {
    switch (tab) {
    case T_GRID: if (has_sound(sel)) cat_open(hub_cur < PC_N ? hub_cur : PC_SOUND); else pick_open(); break;   // v3: EDIT opens the pad on its last section (no pad menu); an empty one asks for an engine
    case T_SEQ: seq_tap(); break;
    case T_PAT: pat_tap(); break;
    case T_PRJ: prj_tap(); break;
    case T_MIX: mix_tap(); break;
    case T_FX: fx_tap(); break;
    }
}
// ---- OPT+EDIT on a whole thing asks first (2026-11-17, user): a pad on HOME, a scene, a card inside the pad, a PROJECT
// section. YES puts it back as in an empty project (undo brings it back; a deleted scene does not come back).
enum { PP_PAT = 1, PP_SOUND = 2, PP_FX = 4, PP_LFO = 8, PP_AUTO = 16, PP_DUCK = 32, PP_MIX = 64, PP_LOOK = 128, PP_NAME = 256, PP_ALL = 511, PP_STEPS = 512, PP_NOUNDO = 1024 };
static const Song *song_zero(void) {     // an empty project's song, made once
    static Song *z; if (z) return z;
    static Song keep; z = malloc(sizeof *z); if (!z) return NULL;
    engine_lock(1); keep = song; song_defaults(); *z = song; song = keep; engine_lock(0);
    return z;
}
static void pad_reset(int s, int m) {    // the parts m of pad s as in an empty project; its clip goes on playing (as a paste)
    const Song *dz = song_zero(); if (!dz || s < 0 || s >= NSLOT) return;
    Track *t = &song.t[s]; const Track *z = &dz->t[s]; PadMix *pm = &song.mix.pad[s]; const PadMix *pz = &dz->mix.pad[s];
    int frz = (m & (PP_MIX | PP_SOUND)) && pm->spare[PM_FRZ];
    if (!(m & PP_NOUNDO)) push_undo();
    engine_lock(1);
    if (m & PP_STEPS) { t->pat[clampi(t->cur, 0, NPAT - 1)] = z->pat[0]; memset(song.fol[s][clampi(t->cur, 0, NPAT - 1)], 0, 6); }
    if (m & PP_PAT) { memcpy(t->pat, z->pat, sizeof t->pat); t->cur = 0; t->queued = -1; memcpy(song.fol[s], dz->fol[s], sizeof song.fol[s]); }
    if (m & PP_SOUND) {
        static Track k; k = *t; *t = *z;
        memcpy(t->pat, k.pat, sizeof t->pat); t->cur = k.cur; t->queued = k.queued; t->on = k.on; t->mute = k.mute;
        t->vol = k.vol; t->pan = k.pan; t->icon = k.icon; t->icon_hi = k.icon_hi;
        song.cut[s] = dz->cut[s]; memcpy(song.cx[s], dz->cx[s], sizeof song.cx[s]); memcpy(song.ax[s], dz->ax[s], sizeof song.ax[s]);
        memcpy(song.wv[s], dz->wv[s], sizeof song.wv[s]); memcpy(song.smx[s], dz->smx[s], sizeof song.smx[s]);
        song.lnote[s] = 0; pm->spare[PM_NORM] = pz->spare[PM_NORM]; pm->spare[PM_FRZ] = 0;
    }
    if (m & PP_FX) { memcpy(pm->ins, pz->ins, sizeof pm->ins); memcpy(song.mbx[s], dz->mbx[s], sizeof song.mbx[s]); }
    if (m & PP_LFO) memcpy(song.lfo[s], dz->lfo[s], sizeof song.lfo[s]);
    if (m & PP_AUTO) { memcpy(song.aut[s], dz->aut[s], sizeof song.aut[s]); memcpy(song.acurve[s], dz->acurve[s], sizeof song.acurve[s]); }
    if (m & PP_DUCK) { pm->spare[DK_SRC] = pz->spare[DK_SRC]; pm->spare[DK_AMT] = pz->spare[DK_AMT]; memcpy(song.dkx[s], dz->dkx[s], 8); memcpy(song.dkc[s], dz->dkc[s], 16); }
    if (m & PP_MIX) { t->vol = z->vol; t->pan = z->pan; t->mute = 0; pm->send_a = pz->send_a; pm->send_b = pz->send_b; pm->solo = pz->solo; pm->flags = pz->flags; pm->spare[PM_FRZ] = 0; }
    if (m & PP_LOOK) { t->icon = z->icon; t->icon_hi = z->icon_hi; }
    if (m & PP_NAME) memset(song.pname[s], 0, sizeof song.pname[s]);
    engine_lock(0);
    if (frz) frz_drop(s);
    if (m & (PP_PAT | PP_SOUND)) { last_note[s] = last_vel[s] = last_prob[s] = last_rep[s] = last_slc[s] = 0; }
    if (m & PP_SOUND) { slot_want[s][0] = 0; slot_bad[s] = 0; sync_samples(); }
}
static void prj_reset_row(int j) {       // one PROJECT setting back to its default (actions have none)
    const Song *dz = song_zero(); if (!dz) return;
    switch (j) {
    case J_TEMPO: song.tempo = dz->tempo; break; case J_SWING: song.swing = dz->swing; break;
    case J_KEY: song.key = dz->key; break; case J_SCALE: song.scale = dz->scale; break;
    case J_SNAP: song.snap = dz->snap; break; case J_BEATS: song.beats = dz->beats; break;
    case J_LAUNCH: song.launch = dz->launch; break; case J_METRO: song.metro = dz->metro; break; case J_MASTER: song.master = dz->master; break;
    case J_COUNT: song.count_in = dz->count_in; break; case J_QUANT: song.rec_quant = dz->rec_quant; song.rec_len = dz->rec_len; break;
    case J_VIEW: ui_simple = 0; break; case J_STRIP: ui_strip = 1; break; case J_QBAR: ui_qbar = 0; break; case J_QBTN: qb_defaults(); qb_save(); break; case J_SECS: ui_secs = 1; break; case J_NAV: ui_tree = 0; break; case J_LOCK: ui_lock = 0; break;
    case J_HINT: if (!hint_on) hint_set(1); break;
    case J_DRAWER: ui_drawer_left = 0; break; case J_MXFX: ui_mxfx = 1; break; case J_MXLINK: ui_mxlink = 1; break;
    case J_MPORT: midi_want[0] = 0; midi_close(); midi_cfg_save(); break; case J_MCLK: midi_clk_out = 0; midi_cfg_save(); break;
    case J_MNOTE: midi_notes = 0; midi_cfg_save(); break; case J_MCH: midi_ch = 0; midi_cfg_save(); break;
    case J_MCLKIN: midi_clk_in = 0; tempo_mul = 1.f; midi_cfg_save(); break;
    case J_TRIM: trim_db = 0; trim_save(); break;
    case J_MSND: metro.snd = 0; metro_save(); break; case J_MVIB: metro.vib = 0; metro_save(); break; case J_MVOL: metro.vol = 70; metro_save(); break;
    case J_MTONE: metro.tone = 0; metro_save(); break; case J_MWHEN: metro.when = 0; metro_save(); break; case J_MDIV: metro.div = 0; metro_save(); break;
    case J_MVOFF: vib_off = 0; metro_save(); break;
    case J_THEME: if (th_sel) { th_sel = 0; theme_apply(); th_dirty = 1; } break;
    }
}
static int prj_row_resets(int j) { return j < J_LIB || j == J_SNAP || (j >= J_HINT && j <= J_TRIM) || j >= J_MSND; }   // J_THEME < J_LIB too
static int prj_sec_resets(int sc) { int r[JS_ROWS], n = prj_rows(sc, r); for (int i = 0; i < n; i++) if (prj_row_resets(r[i])) return 1; return 0; }
static void prj_reset_sec(int sc) {     // sc -1: PATTERN's RECORD card, the take's two settings
    int r[JS_ROWS] = {J_COUNT, J_QUANT}, n = sc < 0 ? 2 : prj_rows(sc, r); push_undo();
    for (int i = 0; i < n; i++) prj_reset_row(r[i]);
    if (sc == JS_SET || sc == JS_OUT) ui_cfg_save();
}
enum { CF_PAD = 1, CF_SCENE, CF_PART, CF_PRJ, CF_CHORD };
static void pat_mode_set(int m, int clear);
static int cfm, cfm_arg, cfm_part, cfm_cur; static char cfm_title[48], cfm_sub[64];
static int sc_kill = -1;                 // the open scene to delete once another one plays
static void cfm_open(int act, int arg, int part, const char *title, const char *sub) {
    cfm = act; cfm_arg = arg; cfm_part = part; cfm_cur = 1;   // the cursor on NO
    snprintf(cfm_title, sizeof cfm_title, "%s", title); snprintf(cfm_sub, sizeof cfm_sub, "%s", sub);
}
static void cfm_pad(int s, int part, const char *what) {   // a part of pad s, or all of it
    char t[48]; int n = 0; for (int i = 0; i < NSLOT; i++) n += s < 0 && gsel_in(i);
    if (part != PP_ALL) snprintf(t, sizeof t, "CLEAR %s?", what);
    else if (s < 0) snprintf(t, sizeof t, "CLEAR %d PADS?", n); else snprintf(t, sizeof t, "CLEAR PAD %d?", s + 1);
    cfm_open(part == PP_ALL ? CF_PAD : CF_PART, s, part, t, part == PP_ALL ? "sound, effects, patterns, mix: all of it, on this scene"
                                                                          : "as on an empty pad  \003  SHIFT+L2 undo");
}
static void cfm_scene(int i) {
    if (i < 0 || i >= scn_n) return;
    if (scn_n < 2) { toast("THE ONLY SCENE STAYS"); return; }
    if (sc_kill >= 0 || scn_next >= 0) { toast("WAIT FOR THE SCENE TO CHANGE"); return; }
    char t[48]; snprintf(t, sizeof t, "DELETE SCENE %d?", i + 1);
    cfm_open(CF_SCENE, i, 0, t, i == scn_cur ? "it plays: the next one takes over first  \003  no undo" : "its twelve pads go  \003  no undo");
}
static void cfm_prj(int sc) {
    if (sc >= 0 && !prj_sec_resets(sc)) { toast("NOTHING TO RESET HERE"); return; }
    char t[48]; snprintf(t, sizeof t, "RESET %s?", sc < 0 ? "RECORD" : prj_sec_name[sc]);
    cfm_open(CF_PRJ, sc, 0, t, sc == JS_SET ? "every setting here, the theme back to BRICK" : sc < 0 ? "COUNT-IN and REC QUANT back to their defaults" : "every setting here back to its default");
}
static void cfm_do(void) {
    char b[48];
    switch (cfm) {
    case CF_PAD:
        if (cfm_arg >= 0) { pad_reset(cfm_arg, PP_ALL); snprintf(b, sizeof b, "PAD %d CLEARED", cfm_arg + 1); toast(b); break; }
        { int n = 0; push_undo(); for (int i = 0; i < NSLOT; i++) if (gsel_in(i)) { pad_reset(i, PP_ALL | PP_NOUNDO); n++; }   // one undo step for them all
          snprintf(b, sizeof b, "%d PADS CLEARED", n); toast(b); }
        break;
    case CF_PART: pad_reset(cfm_arg, cfm_part); toast("CLEARED"); break;
    case CF_CHORD: pat_mode_set(2, 1); if (ui_cat == PC_PATTERN && !chording) chord_open(); break;   // YES: the notes go, the chords start on an empty clip (and show)
    case CF_PRJ: prj_reset_sec(cfm_arg); snprintf(b, sizeof b, "%s RESET", cfm_arg < 0 ? "RECORD" : prj_sec_name[cfm_arg]); toast(b); break;
    case CF_SCENE:
        if (cfm_arg != scn_cur) { scene_del(cfm_arg); break; }
        scene_go(cfm_arg + 1 < scn_n ? cfm_arg + 1 : cfm_arg - 1);   // the next one takes over (scene_go polls: the mark only after it)
        if (scn_next < 0) toast("THE SCENE CANNOT CHANGE NOW"); else sc_kill = cfm_arg;
        break;
    }
    cfm = 0;
}
static void scene_kill_tick(void) {      // the open scene that was deleted: gone as soon as the next one plays
    if (sc_kill < 0 || scn_next >= 0) return;
    int i = sc_kill; sc_kill = -1;
    if (i != scn_cur) scene_del(i);
}
static void cfm_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) { if (cfm_cur == 0) cfm_do(); else { if (cfm == CF_CHORD) { pat_mode_set(2, 0); if (ui_cat == PC_PATTERN && !chording) chord_open(); } cfm = 0; } } return; }   // CHORDS, NO: over the notes
    if (k != K_EDIT) used[k] = 1;   // EDIT acts on its release
    if (k == K_LEFT || k == K_RIGHT || k == K_UP || k == K_DOWN) cfm_cur = !cfm_cur;
    else if (k == K_Y || k == K_OPT) cfm = 0;
}
static void cx_resample(void);
static void delete_action(void) {   // OPTION + EDIT
    switch (tab) {
    case T_GRID: if (!gsel_on() && !has_sound(sel)) { cx_resample(); break; }   // an empty pad: the mix rendered into it (2026-11-24)
        cfm_pad(gsel_on() ? -1 : sel, PP_ALL, ""); break;   // the pad, or every pad of the selection
    case T_PRJ:
        if (!jcol) { cfm_prj(jsec); break; }
        if (!prj_row_resets(jcur)) break;
        { int ui = jcur >= J_HINT && jcur <= J_MCLKIN; if (!ui && jcur != J_THEME) push_undo(); prj_reset_row(jcur); if (ui) ui_cfg_save(); toast("DEFAULT"); }
        break;
    case T_SEQ: seq_delete(); break;
    case T_PAT: if (sl_r == 2) break;
        if (pfol) { act_follow_loop(sel, fol_pat()); break; }
        if (pcur >= NPAT) break; act_pattern_clear(sel, pcur); break;
    case T_MIX: mix_delete(); break;
    case T_FX: fx_delete(); break;
    }
}
static void clip_double(void) { act_clip_double(sel); }   // X twice on SEQ+INST
static void copy_paste(int paste) {   // SHIFT+OPTION copy, SHIFT+EDIT paste: what is under the cursor
    if (tab == T_GRID) { if (!paste) act_pad_copy(sel); else act_pad_paste(sel); }
    else if ((tab == T_MIX && mcol < NSLOT) || (tab == T_FX && fx_unit < NSLOT)) {
        int s = tab == T_MIX ? mcol : fx_unit;
        if (!paste) act_strip_copy(s); else act_strip_paste(s);
    } else if (tab == T_SEQ || tab == T_PAT) {
        if (tab == T_PAT && pcur >= NPAT) return;
        Pattern *p = tab == T_PAT ? &song.t[sel].pat[pcur] : cur_pat(sel);
        if (!paste) act_pattern_copy(p); else act_pattern_paste(p);
    }
}

// Knulli's battery saver mutes the sound and dims the screen after 5 minutes without a key press. A file
// in its folder pauses it: kept only while the music plays, so an idle BrickPad still saves the battery.
#define SAVER_PAUSE "/var/run/battery-saver/brickpad.pause"
static void saver_pause(int on) {
    static int was = -1;
    if (on == was) return;
    was = on;
    if (on) { FILE *f = fopen(SAVER_PAUSE, "w"); if (f) fclose(f); }   // no folder: no saver, nothing to do
    else remove(SAVER_PAUSE);
}

// ---------- key dispatch ----------
static void step_reset(Pattern *p, int st) { p->note[st] = NOTE_NONE; p->vel[st] = 100; p->prob[st] = 100; p->rep[st] = 1; p->slice[st] = -1; sub_drop_p(p, st); }
static void step_seed(Pattern *p, int st) {   // an empty step about to be put: every lane starts from its last value on this pad
    step_reset(p, st);
    if (last_note[sel]) p->note[st] = (int8_t)(last_note[sel] - 1);
    if (last_vel[sel]) p->vel[st] = (int8_t)(last_vel[sel] - 1);
    if (last_prob[sel]) p->prob[st] = (int8_t)(last_prob[sel] - 1);
    if (last_rep[sel]) p->rep[st] = (int8_t)(last_rep[sel] - 1);
    if (last_slc[sel] && song.t[sel].type == 0) p->slice[st] = (int8_t)(last_slc[sel] - 2);
}
static void step_audition(int st) {      // an edited step sounds once; editing is not playing: no take, no capture
    Pattern *p = cur_pat(sel); int ns = track_nslices(sel), nt = p->note[st];
    (void)last_note;   // a step about to be put: the note it will have
    float semis = nt == NOTE_NONE ? 0 : (float)note_semis(sel, (uint8_t)nt);
    int sl = ns ? (p->slice[st] < 0 ? st : p->slice[st]) % ns : -1;
    if (song.t[sel].type == 2) sl = p->slice[st] >= 0 ? p->slice[st] & ACID_ACCENT : -1;   // ACID: the accent is heard (a slide needs a next note)
    engine_trig_chain(sel, p->vel[st] / 127.f, semis, sl);
    flash[sel] = SDL_GetTicks();
}
static void step_listen(int st) {        // only this step's slice, with its note and velocity (and what follows the slice)
    Pattern *p = cur_pat(sel); int ns = track_nslices(sel);
    float semis = p->note[st] == NOTE_NONE ? 0 : (float)note_semis(sel, (uint8_t)p->note[st]);
    int sl = ns ? (p->slice[st] < 0 ? st : p->slice[st]) % ns : -1, note = p->note[st];
    engine_trig_chain(sel, p->vel[st] / 127.f, semis, sl);
    flash[sel] = SDL_GetTicks();
    rec_hit(sel, sl, note);
}
static void set_tab(int t) { leave_screen(); if (browsing) browse_close(); slicing = 0; roll_leave(); randing = 0; learning = 0; learn_lfo = -1; if (picking) pick_close(); if (presetting) preset_close(1); theming = 0; iconing = 0; ducking = 0; prbing = 0; if (chording) chord_leave(); perform = 0; tab = t; scening = 0; if (t == T_GRID || t == T_PRJ) ui_cat = -1; }
// KEYS: the pad gets its own tonality, the project's stays. Coming back to the project's value means following the project again.
static void pad_tonality(int k) {
    Track *t = &song.t[sel]; char b[48];
    if (k == K_LEFT || k == K_RIGHT) { int v = (track_scale(sel) + (k == K_RIGHT ? 1 : NSCALE - 1)) % NSCALE; t->scale = v == song.scale ? 0 : v + 1; }
    else { int v = (track_key(sel) + (k == K_R1 ? 1 : 11)) % 12; t->key = v == song.key ? 0 : v + 1; }
    snprintf(b, sizeof b, "%s %s%s", note_name[track_key(sel)], scale_name[track_scale(sel)], t->scale || t->key ? "" : "  PROJECT"); toast(b);
}
// The icon palette of a pad: LOOK in the pad's hub (X on GRID until 2026-11-14).
static int icon_group(int i) { int g = 0; while (g + 1 < ICON_GRP_N && i >= icon_grp_at[g + 1]) g++; return g; }
static int ic_pal = -1;                  // the colour strip under the icons: the colour under the cursor, -1 = in the icons
static void icon_open(void) {
    iconing = 1; ic_cur = 0; ic_pal = -1;
    for (int i = 0; i < ICON_PAL_N; i++) if (icon_pal[i] == track_icon(&song.t[sel]) - 1) ic_cur = i;
}
static void icon_key(int k, int down) {
    if (!down) return;
    used[k] = 1;                         // the release must not act again
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_OPT) { listen(sel); return; }
    if (k == K_Y || k == K_X) { leave_screen(); iconing = 0; return; }
    if (ic_pal >= 0) {                   // the colours: <> choose, EDIT takes one (the window stays), UP back to the icons
        if (k == K_LEFT || k == K_RIGHT) ic_pal = clampi(ic_pal + (k == K_RIGHT ? 1 : -1), 0, ICOL_N - 1);
        else if (k == K_UP) ic_pal = -1;
        else if (k == K_EDIT) { push_undo(); track_set_icol(&song.t[sel], ic_pal); toast(ic_pal ? "ICON COLOUR SET" : "ICON COLOUR AUTO"); }
        return; }
    if (k == K_EDIT) { push_undo(); track_set_icon(&song.t[sel], icon_pal[ic_cur] + 1);
        char b[32]; snprintf(b, sizeof b, "ICON %s", ic_cur ? icon_tab[icon_pal[ic_cur]].name : "AUTO"); toast(b); leave_screen(); iconing = 0; return; }
    int g = icon_group(ic_cur), a = icon_grp_at[g], b = icon_grp_at[g + 1], col = (ic_cur - a) % ICON_COLS;
    if (k == K_L1 || k == K_R1) ic_cur = icon_grp_at[(g + (k == K_R1 ? 1 : ICON_GRP_N - 1)) % ICON_GRP_N];
    if (k == K_LEFT || k == K_RIGHT) ic_cur = clampi(ic_cur + (k == K_RIGHT ? 1 : -1), 0, ICON_PAL_N - 1);   // the ends of a group lead to the next one
    if (k == K_DOWN) {
        if (ic_cur + ICON_COLS < b) ic_cur += ICON_COLS;
        else if ((ic_cur - a) / ICON_COLS < (b - a - 1) / ICON_COLS) ic_cur = b - 1;
        else ic_pal = track_icol(&song.t[sel]);   // under the last row: the colours (L1/R1 change the group)
    }
    if (k == K_UP) {
        if (ic_cur - ICON_COLS >= a) ic_cur -= ICON_COLS;
        else if (g > 0) { int pa = icon_grp_at[g - 1]; ic_cur = clampi(pa + (a - pa - 1) / ICON_COLS * ICON_COLS + col, 0, a - 1); }
    }
}
static int fx_unit;                      // what the FX tab shows: a pad 0..11, 12 RETURN A, 13 RETURN B, 14 MASTER
static void tab_step(int d) {            // F1 left, F2 right, round: GRID SEQ+INST FX MIXER (user, 2026-11-09); PROJECT goes back
    static const int order[4] = {T_GRID, T_SEQ, T_FX, T_MIX};
    {                                    // UI v2: HOME is PADS, SCENES, MIXER, PROJECT (also F1+F2); the pad's pages are inside the pad
        // PROJECT left the circle (user, 2026-11-14): only F1+F2 opens it, from anywhere, and comes back to that very screen
        int at = tab == T_MIX || v2_unit() ? 2 : tab == T_GRID && scening ? 1 : 0, to = (at + (d > 0 ? 1 : 2)) % 3;   // RETURN A/B, MASTER are the MIXER's
        set_tab(to == 2 ? T_MIX : T_GRID); scening = to == 1; if (to == 2 && sel < NSLOT) mcol = sel;
        return;
    }
    int i = 0, cur = tab == T_PAT ? T_SEQ : tab; while (i < 4 && order[i] != cur) i++;
    int nt = order[((i < 4 ? i : 0) + (d > 0 ? 1 : 3)) % 4];
    if (nt == T_FX) fx_unit = sel;
    set_tab(nt);
}
static void roll_open(void); static void duck_open(int s); static int mx_pg;
static struct { int tab, ui_cat, hub_cur, scening, perform, srow, scol, lane, fx_unit, fx_page, mcol, mx_pg, rolling, ducking; } prj_from;
static void prj_leave(void) {            // back where F1+F2 was pressed: the same pad, category, page and cursor
    set_tab(prj_from.tab); ui_cat = prj_from.ui_cat; hub_cur = prj_from.hub_cur;
    scening = prj_from.scening; perform = prj_from.perform; srow = prj_from.srow;
    scol = prj_from.scol; lane = prj_from.lane; fx_unit = prj_from.fx_unit; fx_page = prj_from.fx_page; mcol = prj_from.mcol; mx_pg = prj_from.mx_pg;
    if (prj_from.rolling && cur_pat(sel)->mode) { if (cur_pat(sel)->mode == 2) chord_open(); else roll_open(); }
    if (prj_from.ducking) duck_open(sel);
}
static void tab_project(void) {          // F1+F2: in and out of PROJECT (its only way in since 2026-11-14)
    if (tab == T_PRJ) { prj_leave(); return; }
    prj_from.tab = tab == T_PRJ ? T_GRID : tab; prj_from.ui_cat = ui_cat; prj_from.hub_cur = hub_cur;
    prj_from.scening = scening; prj_from.perform = perform; prj_from.srow = srow;
    prj_from.scol = scol; prj_from.lane = lane; prj_from.fx_unit = fx_unit; prj_from.fx_page = fx_page; prj_from.mcol = mcol; prj_from.mx_pg = mx_pg;
    prj_from.rolling = rolling; prj_from.ducking = ducking;
    set_tab(T_PRJ);
}
static void rec_store(int s, int slice, int note, double pos) {
    Track *t = &song.t[s]; Pattern *p = cur_pat(s);
    if (pos < rec_from) pos = rec_from;  // rounding can put a hit a hair before the take's start
    if (rec_free[s]) {
        if (rec_n < (int)(sizeof rec_ev / sizeof *rec_ev)) rec_ev[rec_n++] = (RecEv){(int8_t)s, (int8_t)note, (int8_t)slice, pos, -1, (int16_t)rec_tag_now};
        return;
    }
    if (rec_replace && !rec_wiped[s]) {  // REPLACE: the take's first note wipes the clip, its length stays
        rec_wiped[s] = 1; for (int i = 0; i < NSTEP; i++) p->trig[i] = 0; for (int i = 0; i < NNOTE; i++) p->roll[i].len = 0; }
    int d = engine_step32(p), g = rec_grid(d), len = p->len ? p->len : 16;
    long rel = (long)((pos - engine_anchor(s)) / g + 0.5) * g;   // steps count from where the clip began
    if (rel < 0) rel += (long)len * d * (1 + (-rel) / ((long)len * d));
    int qp = (int)(engine_anchor(s) + rel), q = (int)(rel / d), st = q % len;
    if (p->mode) {                       // a ROLL clip: a note, one grid long until its key is let go
        int k = roll_add(p, (int)(rel % ((long)len * d)) * RT, g * RT, rec_midi(s, note), 127);
        if (k < 0) return;
        if ((uint32_t)qp >= engine_pos32()) p->roll[k].flags |= NF_SKIP;   // still ahead: it was just heard
        t->on = 1;
        if (rec_tag_now) for (int i = 0; i < 32; i++) if (!rec_open[i].tag) {
            rec_open[i].tag = (int16_t)rec_tag_now; rec_open[i].s = (int8_t)s; rec_open[i].pat = t->cur; rec_open[i].k = (int8_t)k; rec_open[i].g = (int8_t)g; rec_open[i].pos = pos; break; }
        return;
    }
    rec_put(p, st, note, slice);
    t->on = 1;
    if ((uint32_t)qp >= engine_pos32()) engine_skip(s, st);   // that step is still ahead: it was just heard, do not double it
}
// Recording (the user's algorithm, 2026-11-08): MENU+PLAY arms, whatever the count does; the first hit starts the
// take right there and is step one (stopped: the count starts with it); MENU+PLAY again ends it at once; then the
// length is rounded to the nearest quantum (PROJECT LAUNCH, 1 bar if NONE), back or forward, and the clip loops on
// from the take's start. A count-in set in PROJECT still counts in first when stopped.
static void rec_toggle(double pos, Uint32 pressed) {   // MENU+PLAY: pos and time of the press, it fires a moment later
    if (rec && engine_counting() >= 0) { transport_toggle(); toast("REC CANCELLED"); return; }
    if (rec && rec_wait) { rec = 0; rec_wait = 0; toast("REC CANCELLED"); return; }
    if (rec) { rec_finish(pos); return; }
    engine_queue_stop(0);
    push_undo(); memset(rec_wiped, 0, sizeof rec_wiped);
    int playing = engine_playing();
    rec_n = 0; rec_end = -1; rec_from = 0;
    for (int s = 0; s < NSLOT; s++) { rec_free[s] = !pat_used(cur_pat(s)); rec_pat[s] = song.t[s].cur; }
    if (!playing && song.count_in) {
        for (int s = 0; s < NSLOT; s++) song.t[s].on = pat_used(cur_pat(s));
        engine_count(song.count_in);
        rec = 1; toast("REC ON"); return;
    }
    rec = 1; rec_wait = 1; toast("REC ARMED: PLAY A NOTE");
    if (playing) {                       // a hit made while MENU+PLAY was still being decided is the first one
        int oldest = cap_n > CAP_N ? cap_n - CAP_N : 0, i = cap_n;
        while (i > oldest && cap[(i - 1) % CAP_N].ms >= pressed && cap[(i - 1) % CAP_N].pos >= 0) i--;
        if (i < cap_n) {
            rec_wait = 0; rec_from = floor(cap[i % CAP_N].pos + 0.5); toast("REC ON");
            for (int k = i; k < cap_n; k++) { const CapEv *e = &cap[k % CAP_N]; rec_store(e->slot, e->slice, e->note, e->pos); }
            cap_n = i;
        }
    }
}
static void combo_tick(Uint32 now) {     // no second PLAY came: it was a single press
    if (combo_t && ms_since(now, combo_t) >= COMBO_MS) { Uint32 t = combo_t; combo_t = 0; rec_toggle(combo_pos, t); }
}
static void rec_hit(int s, int slice, int note) {   // a played note goes on the nearest step of the slot's clip
    if (!rec) { cap_log(s, slice, note); return; }
    if (rec_wait && engine_playing()) {  // the count runs: the take starts at this hit, it is step one
        rec_wait = 0; rec_from = floor(engine_posf() + 0.5);
        rec_store(s, slice, note, rec_from);
        toast("REC ON");
        return;
    }
    if (rec_wait) {                      // the first hit of an armed take: the count starts here, this is step one
        rec_wait = 0; play_solo = 0; rec_from = 0;
        for (int i = 0; i < NSLOT; i++) { song.t[i].on = pat_used(cur_pat(i)); song.t[i].queued = -1; }
        rec_store(s, slice, note, 0); rec_starting = 1;
        engine_transport_skip(rec_free[s] ? -1 : s, 0);   // the hit already sounds: step one of a clip with notes does not play it again
        toast("REC ON");
        return;
    }
    double pos = engine_posf();
    if (engine_playing()) rec_starting = 0;
    else if (rec_starting) pos = 0;              // pressed together with the first hit: a chord on step one
    else {                                       // a hit just ahead of the first beat belongs to it
        if (engine_counting() < 0 || engine_count_left() > rec_grid(engine_step32(cur_pat(s))) / 2.0) return;
        pos = 0;
    }
    rec_store(s, slice, note, pos);
}
static void rec_release(int tag) {       // a held key let go: the note it started gets its length (take, overdub, capture)
    double now = engine_playing() ? engine_posf() : -1;
    double ms32 = 60000.0 / (song.tempo * tempo_mul) / 8;
    for (int i = cap_n - 1; i >= 0 && i >= cap_n - CAP_N; i--) { CapEv *e = &cap[i % CAP_N];
        if (e->tag == tag && e->len < 0) { e->len = e->pos < 0 || now < 0 ? (float)((SDL_GetTicks() - e->ms) / ms32) : (float)(now - e->pos); if (e->len < 0) e->len = 0; break; } }
    for (int i = 0; i < 32; i++) if (rec_open[i].tag == tag) {
        Pattern *p = &song.t[rec_open[i].s].pat[rec_open[i].pat]; Note *n = &p->roll[rec_open[i].k]; int g = rec_open[i].g;
        rec_open[i].tag = 0;
        if (!n->len || now < 0) continue;
        int ln = (int)((now - rec_open[i].pos) / g + 0.5) * g; if (ln < g) ln = g; ln *= RT;
        int T = roll_ticks(p); if (n->start + ln > T) ln = T - n->start;
        if (ln >= 1) n->len = (uint16_t)ln;
    }
    if (!rec) return;
    for (int i = rec_n - 1; i >= 0; i--) if (rec_ev[i].tag == tag && rec_ev[i].end < 0) { rec_ev[i].end = now >= 0 ? now : rec_ev[i].pos; break; }
}

// ---------- piano roll (Pattern.mode 1): notes with a length, chords; the step lanes are kept but silent ----------
static int rolling;                      // the note editor of the pad's current pattern is open
static int rl_t, rl_n = 60, rl_v0, rl_n0 = 50, rl_snap = 24, rl_len = 24, rl_heard, rl_chord;   // cursor (tick, pitch), view, grid and last length in ticks
static const int rl_snaps[6] = {3, 6, 12, 24, 48, 96};   // 1/128 .. 1/4 in 1/384 ticks
// 2026-11-24 (user, instead of SUB): the roll's GRID is STRAIGHT, TRIPLET (2/3), DOTTED (3/2) or FREE (the cursor walks the
// straight grid, OPT+EDIT+<> moves a note one tick, 1/384). RHYTHM: the same notes on one line, the height their velocity.
enum { RG_STRAIGHT, RG_TRIPLET, RG_DOTTED, RG_FREE, RG_N };
static const char *const rg_name[RG_N] = {"STRAIGHT", "TRIPLET", "DOTTED", "FREE"};
static int rl_gtype, rl_gi = 3, rl_rhythm;
static int rl_snap_of(int t, int g) { int s = rl_snaps[g]; return t == RG_TRIPLET ? s * 2 / 3 : t == RG_DOTTED ? s * 3 / 2 : s; }
static const int rl_d32[6] = {1, 2, 4, 8, 16, 32};
enum { RL_COLS = 32, RL_ROWS = 14, RL_TAG = 301, RL_CHORD_TAG = 310 };
static int rl_snap; static int roll_ticks(const Pattern *p);
static int rl_cols(void) {               // 2026-11-24: the roll shows the clip, no dead half: as many columns as it is long (8..32)
    int T = roll_ticks(cur_pat(sel)), c = rl_snap > 0 ? (T + rl_snap - 1) / rl_snap : RL_COLS; return c < 8 ? 8 : c > RL_COLS ? RL_COLS : c;
}
static int roll_ticks(const Pattern *p) { return (p->len ? p->len : 16) * rl_d32[p->div % 6] * RT; }
static int roll_any(const Pattern *p) { for (int i = 0; i < NNOTE; i++) if (p->roll[i].len) return 1; return 0; }
static int roll_find(const Pattern *p, int t, int n) {   // the note of pitch n sounding at tick t (the latest to start)
    int best = -1;
    for (int i = 0; i < NNOTE; i++) { const Note *x = &p->roll[i];
        if (x->len && x->note == n && x->start <= t && t < x->start + x->len && (best < 0 || x->start > p->roll[best].start)) best = i; }
    return best;
}
static int roll_add(Pattern *p, int start, int len, int note, int vel) {
    for (int i = 0; i < NNOTE; i++) if (!p->roll[i].len) {
        int T = roll_ticks(p); if (start < T && start + len > T) len = T - start;   // a note ends with the clip
        Note *x = &p->roll[i]; x->start = (uint16_t)start; x->note = (uint8_t)clampi(note, 0, 127); x->vel = (uint8_t)clampi(vel, 1, 127); x->prob = 100; x->flags = 0;
        __atomic_store_n(&x->len, (uint16_t)clampi(len, 1, NSTEP * 32 * RT), __ATOMIC_RELEASE);   // last: the audio thread sees a whole note or none
        return i;
    }
    toast("ROLL IS FULL (96 NOTES)"); return -1;
}
static void roll_del(Pattern *p, int i) { __atomic_store_n(&p->roll[i].len, 0, __ATOMIC_RELEASE); }
static int roll_base(int s) { return 60 - note_semis(s, 60); }   // the pitch that plays the pad as it is
static void roll_hear(int n) {           // the note while EDIT is held; editing is not playing: no take
    if (!hear_on || (engine_playing() && song.t[sel].on)) return;
    engine_release_key(RL_TAG); engine_trig_key(sel, 1.f, (float)note_semis(sel, n), -1, RL_TAG); flash[sel] = SDL_GetTicks(); rl_heard = 1;
}
static void roll_view(void) {            // the cursor stays on screen
    int w = rl_cols() * rl_snap;
    if (rl_t < rl_v0) rl_v0 = rl_t; if (rl_t >= rl_v0 + w) rl_v0 = rl_t - w + rl_snap; if (rl_v0 < 0) rl_v0 = 0;
    if (rl_n < rl_n0) rl_n0 = rl_n; if (rl_n >= rl_n0 + RL_ROWS) rl_n0 = rl_n - RL_ROWS + 1; rl_n0 = clampi(rl_n0, 0, 128 - RL_ROWS);
}
static void roll_open(void) {
    Pattern *p = cur_pat(sel); int d = rl_d32[p->div % 6] * RT;
    rl_gi = 0; while (rl_gi < 5 && rl_snaps[rl_gi] < d) rl_gi++; if (rl_gtype == RG_DOTTED && !rl_gi) rl_gi = 1;
    rl_snap = rl_snap_of(rl_gtype, rl_gi); rl_len = d; rl_t = 0; rl_v0 = 0; rl_n = roll_base(sel);
    for (int i = 0; i < NNOTE; i++) if (p->roll[i].len) { rl_n = p->roll[i].note; break; }
    rl_n0 = clampi(rl_n - RL_ROWS / 2, 0, 128 - RL_ROWS); rolling = 1;
}
// MODE: STEPS > PIANO ROLL > CHORDS, round (2026-11-20, user: chosen on PATTERN's first card with EDIT + the arrows).
// The roll and the chords are the same notes (mode 2 plays as the roll); an empty roll takes the steps as notes; both are kept.
static const char *const pat_mode_name[3] = {"STEPS", "PIANO ROLL", "CHORDS"};
// 2026-11-20 (user): a clip with notes asks first whether CHORDS start on an empty clip (YES) or over the notes (NO); Y stays.
static void pat_mode_set(int m, int clear) {
    Pattern *p = cur_pat(sel); edit_begin();
    if (clear) for (int i = 0; i < NNOTE; i++) p->roll[i].len = 0;   // the roll's notes only: the steps stay for STEPS
    else if (!p->mode && m && !roll_any(p)) {
        int d = rl_d32[p->div % 6] * RT, base = roll_base(sel), acid = song.t[sel].type == 2;
        for (int st = 0; st < p->len; st++) if (p->trig[st]) {
            int n = p->note[st] == NOTE_NONE ? base : p->note[st], fl = acid && p->slice[st] >= 0 ? p->slice[st] : 0;
            int k = roll_add(p, st * d, (fl & ACID_SLIDE) ? d + d / 2 : d, n, p->vel[st]);
            if (k < 0) break;
            p->roll[k].prob = (uint8_t)p->prob[st]; if (fl & ACID_ACCENT) p->roll[k].flags |= NF_ACC;
        }
    }
    p->mode = (uint8_t)m; toast(pat_mode_name[m]);
}
static void pat_mode_step(int d) {
    Pattern *p = cur_pat(sel); int m = ((p->mode + d) % 3 + 3) % 3, steps = 0;
    if (m && track_nslices(sel)) { toast("SLICED PAD: STEPS ONLY"); return; }
    for (int st = 0; st < p->len; st++) steps |= p->trig[st];
    if (m == 2 && (roll_any(p) || (!p->mode && steps))) {
        char t[48]; snprintf(t, sizeof t, "CLEAR P%d FOR CHORDS?", song.t[sel].cur + 1);
        cfm_open(CF_CHORD, sel, 0, t, "YES: an empty clip  \003  NO: chords over the notes"); return; }
    pat_mode_set(m, 0);
}
static void roll_mode(void) { pat_mode_step(1); }
static int roll_in_scale(int n) {
    int sc = track_scale(sel), key = track_key(sel), pc = ((n - key) % 12 + 12) % 12;
    for (int d = 0; d < scale_len(sc); d++) if (deg_semis(sc, d) % 12 == pc) return 1;
    return 0;
}
static int roll_up(int n, int steps) {   // the scale tone `steps` above n (CHROMATIC: a major triad's thirds)
    if (track_scale(sel) == 0) return n + (steps == 2 ? 4 : steps == 4 ? 7 : steps);
    while (steps > 0 && n < 127) { n++; if (roll_in_scale(n)) steps--; }
    return n;
}
static void roll_chord_off(void) { for (int i = 0; i < rl_chord; i++) engine_release_key(RL_CHORD_TAG + i); rl_chord = 0; }
static void roll_leave(void) {           // out of the editor: no note it started keeps sounding
    if (rl_heard) { engine_release_key(RL_TAG); rl_heard = 0; }
    roll_chord_off(); rolling = 0;
}
static void rnd_open(void);
static int rl_odel, rl_nudge = -1;       // OPT+EDIT went down on the roll: delete on release; the note being nudged
static void roll_triad(Pattern *p, int i) {   // a triad on the cursor, in the scale (ACID: the note's ACCENT)
    used[K_EDIT] = 1;
    if (song.t[sel].type == 2) { if (i >= 0) { push_undo(); p->roll[i].flags ^= NF_ACC; } return; }
    push_undo();
    int st = i >= 0 ? p->roll[i].start : rl_t, ln = i >= 0 ? p->roll[i].len : rl_len, v = i >= 0 ? p->roll[i].vel : 100;
    if (i < 0) roll_add(p, st, ln, rl_n, v);
    for (int s2 = 2; s2 <= 4; s2 += 2) { int n = roll_up(rl_n, s2); if (n <= 127 && roll_find(p, st, n) < 0) roll_add(p, st, ln, n, v); }
}
static void rhythm_sync(const Pattern *p) {   // RHYTHM: the pitch under the cursor is that of the note at its time, else the pad's own
    int best = -1; for (int j = 0; j < NNOTE; j++) { const Note *x = &p->roll[j]; if (x->len && x->start <= rl_t && rl_t < x->start + x->len && (best < 0 || x->start > p->roll[best].start)) best = j; }
    rl_n = best >= 0 ? p->roll[best].note : roll_base(sel);
}
static void roll_key(int k, int down) {
    Pattern *p = cur_pat(sel); int T = roll_ticks(p);
    if (rl_rhythm) { rhythm_sync(p);       // one line: no pitch to walk; EDIT+^v is the velocity
        if (down && (k == K_UP || k == K_DOWN) && !held[K_OPT] && !held[K_SHIFT]) {
            if (held[K_EDIT]) { int i = roll_find(p, rl_t, rl_n); used[K_EDIT] = 1; if (i >= 0) { edit_begin(); p->roll[i].vel = (uint8_t)clampi(p->roll[i].vel + (k == K_UP ? 8 : -8), 1, 127); } }
            return; } }
    if (k == K_R2) { if (down && !held[K_SHIFT]) { hear_on = !hear_on; toast(hear_on ? "HEAR NOTES ON" : "HEAR NOTES OFF"); } return; }
    if (!down && (k == K_EDIT || k == K_OPT)) rl_nudge = -1;
    if (!down && rl_odel && (k == K_EDIT || k == K_OPT)) { int j = roll_find(p, rl_t, rl_n); rl_odel = 0; if (j >= 0) { push_undo(); roll_del(p, j); } return; }
    if (!down) {
        if (k == K_EDIT) {
            if (rl_heard) { rl_heard = 0; engine_release_key(RL_TAG); }
            if (!used[K_EDIT]) { int i = roll_find(p, rl_t, rl_n); push_undo();
                if (i >= 0) roll_del(p, i); else roll_add(p, rl_t, rl_len, rl_n, 100); }
        }
        if (k == K_OPT) roll_chord_off();
        if (k == K_X || k == K_OPT) undo_armed = 0;   // each combo its own undo step
        return;
    }
    if (k == K_Y) { roll_leave(); return; }
    if (held[K_SHIFT] && k == K_X) { used[K_SHIFT] = used[K_X] = 1; rnd_open(); return; }   // RANDOM, over the roll (Y comes back here)
    if (k == K_PLAY) { play_key(); return; }
    if (held[K_SHIFT] && k == K_L2) {
        undo(); p = cur_pat(sel);
        if (!p->mode) { roll_leave(); return; }   // the undo took the roll away
        rl_t = clampi(rl_t, 0, (roll_ticks(p) - 1) / rl_snap * rl_snap); return;
    }
    if (k == K_L1 || k == K_R1) {        // the grid's size: 1/128 .. 1/4 (of its kind; DOTTED from 1/64)
        rl_gi = clampi(rl_gi + (k == K_R1 ? 1 : -1), rl_gtype == RG_DOTTED ? 1 : 0, 5); rl_snap = rl_snap_of(rl_gtype, rl_gi);
        rl_t = rl_t / rl_snap * rl_snap; rl_v0 = rl_v0 / rl_snap * rl_snap; roll_view(); return;
    }
    int i = roll_find(p, rl_t, rl_n);
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) {   // OPT+EDIT: the note goes when they are let go, unless UP made a triad
        used[K_EDIT] = used[K_OPT] = 1; roll_chord_off(); rl_odel = 1; return;
    }
    if (k == K_UP && held[K_OPT] && held[K_EDIT]) { rl_odel = 0; roll_triad(p, i); return; }
    if ((k == K_LEFT || k == K_RIGHT) && held[K_OPT] && held[K_EDIT]) {   // OPT+EDIT+<>: the note moves (FREE: a tick, else the grid)
        rl_odel = 0; if (i < 0 && rl_nudge >= 0 && p->roll[rl_nudge].len && p->roll[rl_nudge].note == rl_n) i = rl_nudge;   // FREE: the cursor stayed on the grid
        if (i < 0) return; int st = rl_gtype == RG_FREE ? 1 : rl_snap, ns = p->roll[i].start + (k == K_RIGHT ? st : -st);
        if (ns < 0 || ns >= roll_ticks(p)) return; push_undo(); p->roll[i].start = (uint16_t)ns; rl_nudge = i; if (rl_gtype != RG_FREE) rl_t = ns; roll_view(); return; }   // 2026-11-24: OPT+EDIT+UP a triad (ACID: ACCENT); was X+EDIT
    if (k == K_EDIT && held[K_X]) { used[K_X] = 1; roll_triad(p, i); return; }
    if (k == K_EDIT) { roll_hear(i >= 0 ? p->roll[i].note : rl_n); return; }
    if (k == K_OPT) {                    // the chord under the cursor's time, while held
        roll_chord_off();
        if (engine_playing() && song.t[sel].on) return;
        for (int j = 0; j < NNOTE && rl_chord < 8; j++) { const Note *x = &p->roll[j];
            if (x->len && x->start <= rl_t && rl_t < x->start + x->len) engine_trig_key(sel, x->vel / 127.f, (float)note_semis(sel, x->note), -1, RL_CHORD_TAG + rl_chord++); }
        if (!rl_chord) { engine_trig_key(sel, 1.f, (float)note_semis(sel, rl_n), -1, RL_CHORD_TAG); rl_chord = 1; }
        flash[sel] = SDL_GetTicks(); return;
    }
    if (k > K_RIGHT) return;
    int dx = (k == K_RIGHT) - (k == K_LEFT), dy = (k == K_UP) - (k == K_DOWN);
    if (held[K_EDIT]) {                  // EDIT+<> length (a new note on an empty cell), EDIT+^v pitch
        used[K_EDIT] = 1; edit_begin();
        if (dx) {
            if (i < 0) { if (dx > 0) { int j = roll_add(p, rl_t, rl_snap, rl_n, 100); if (j >= 0) rl_len = rl_snap; } return; }
            int nl = p->roll[i].len + dx * rl_snap; if (nl < 1) nl = 1; if (p->roll[i].start + nl > T) nl = T - p->roll[i].start;
            if (nl >= 1) { p->roll[i].len = (uint16_t)nl; rl_len = nl; }
        } else if (i >= 0) { int nn = clampi(p->roll[i].note + dy, 0, 127); p->roll[i].note = (uint8_t)nn; rl_n = nn; roll_hear(nn); roll_view(); }
        return;
    }
    if (held[K_X]) {                     // X+^v velocity, X+<> chance
        used[K_X] = 1; if (i < 0) return; edit_begin();
        if (dy) p->roll[i].vel = (uint8_t)clampi(p->roll[i].vel + dy * 8, 1, 127);
        if (dx) p->roll[i].prob = (uint8_t)clampi(p->roll[i].prob + dx * 10, 0, 100);
        return;
    }
    if (held[K_OPT]) {                   // OPT+<> the next note, OPT+^v an octave
        used[K_OPT] = 1; roll_chord_off();
        if (dx) { int best = -1;
            for (int j = 0; j < NNOTE; j++) { const Note *x = &p->roll[j]; if (!x->len || x->start >= T) continue;
                int a = x->start * 128 + x->note, c = rl_t * 128 + rl_n;
                if (dx > 0 ? a > c && (best < 0 || a < p->roll[best].start * 128 + p->roll[best].note) : a < c && (best < 0 || a > p->roll[best].start * 128 + p->roll[best].note)) best = j; }
            if (best >= 0) { rl_t = p->roll[best].start; rl_n = p->roll[best].note; if (rl_t % rl_snap) rl_snap = 1; roll_hear(rl_n); engine_release_key(RL_TAG); rl_heard = 0; }
        } else if (i >= 0) { edit_begin(); int nn = clampi(p->roll[i].note + 12 * dy, 0, 127); p->roll[i].note = (uint8_t)nn; rl_n = nn; }
        else rl_n = clampi(rl_n + 12 * dy, 0, 127);
        roll_view(); return;
    }
    int last = (T - 1) / rl_snap * rl_snap;
    if (held[K_SHIFT]) { rl_t = clampi(rl_t + dx * rl_cols() * rl_snap, 0, last); rl_n = clampi(rl_n + 12 * dy, 0, 127);
        rl_v0 += dx * rl_cols() * rl_snap; rl_n0 += 12 * dy; roll_view(); return; }
    // 2026-11-24: past the end the arrow goes onto the side slots (as on STEPS); the clip grows on LEN/GRID (EDIT+^v a bar)
    if (dx < 0 && p->len > 16) {         // back out of an empty last bar: it goes
        int bt = roll_ticks(p) * ((p->len - 1) / 16 * 16) / p->len, any = 0;
        for (int j = 0; j < NNOTE; j++) if (p->roll[j].len && p->roll[j].start + p->roll[j].len > bt) any = 1;
        if (!any && rl_t >= bt && rl_t - rl_snap < bt) { edit_begin(); p->len = (uint8_t)((p->len - 1) / 16 * 16); toast_length("LENGTH", p->len); T = roll_ticks(p); last = (T - 1) / rl_snap * rl_snap; }
    }
    rl_t = clampi(rl_t + dx * rl_snap, 0, last); rl_n = clampi(rl_n + dy, 0, 127); roll_view();
}

// ---------- MIXER and FX tabs (2026-11-09; layout and keys from PLAN-mixer-fx.md, F1/F2 and FX as a tab by the user) ----------
enum { MX_INS1, MX_INS2, MX_SA, MX_SB, MX_DUCK, MX_PAN, MX_VOL, MX_MUTE, MX_SOLO, MX_FRZ, MX_ROWS };   // DUCK (2026-11-11): the sidechain row
static int mcol, mrow = MX_VOL;          // mixer cursor: strip 0..14 (12 pads, RET A, RET B, MASTER), row
static int fx_sub;                       // MULTIBAND: 1 = the cursor in the right half (Song.mbx gains), rows 2..6
static int fcol, frow;                   // FX tab cursor: pads: column 0/1 (insert), row 0 TYPE 1 ON 2..7 params, 8 the bottom row (fcol 0..3)
#define MIX_FXX 732                      // "FX nn%": small, in the header row between the pattern and the OUT meter
#define MIX_FXY 58
static int fx_budget_used(void) { int u = 0; for (int s = 0; s < NSLOT; s++) for (int i = 0; i < NINS; i++) { const FxSlot *f = &song.mix.pad[s].ins[i]; if (f->on && f->type) u += fx_weight[f->type]; } return u; }
static int ui_fx_live(int s0, int i0) {   // the same budget walk as the engine, in pad order: is this slot within it?
    int b = FX_BUDGET;
    for (int s = 0; s < NSLOT; s++) for (int i = 0; i < NINS; i++) {
        const FxSlot *f = &song.mix.pad[s].ins[i]; int on = f->on && f->type && f->type < FX_N;
        if (on && fx_weight[f->type] > b) on = 0; else if (on) b -= fx_weight[f->type];
        if (s == s0 && i == i0) return on;
    }
    return 0;
}
static void fx_turn_on(FxSlot *f) {      // on, unless the budget is full
    f->on = 0; if (!f->type) return;
    if (fx_budget_used() + fx_weight[f->type] > FX_BUDGET) { toast("FX LIMIT (40% OF CPU): TURN ANOTHER OFF"); return; }
    f->on = 1;
}
static void mbx_clear(const FxSlot *f) {   // a new type in an insert: MULTIBAND's gains back to 0 dB
    for (int s = 0; s < NSLOT; s++) for (int i = 0; i < NINS; i++) if (f == &song.mix.pad[s].ins[i]) memset(song.mbx[s][i], 0, 6);
}
static int8_t *fx_side(int c, int r, int *lo, int *hi) {   // the right half of an insert's row, if it has one (Song.mbx)
    if (fx_unit >= NSLOT) return NULL; int ty = song.mix.pad[fx_unit].ins[c].type;
    if (ty == FX_MULTI && r >= 2 && r < 2 + MB_N) { *lo = -24; *hi = 24; return &song.mbx[fx_unit][c][r - 2]; }   // MULTIBAND gains
    if (fx_keyed(ty) && r == 2) { *lo = 0; *hi = NSLOT; return &song.mbx[fx_unit][c][5]; }                     // COMPRESSOR KEY: 0 itself, n = pad n (sidechain); RING SOURCE: 0 its oscillator
    return NULL;
}
static int fx_multi_row(int c, int r) { int lo, hi; return fx_side(c, r, &lo, &hi) != NULL; }
static void fx_cycle(FxSlot *f, int d) {   // the next type; FLANGER and PHASER are modes of MODULATION (CHORUS's stop)
    int t = fx_is_mod(f->type) ? FX_CHORUS : f->type;
    do t = ((t + d) % FX_N + FX_N) % FX_N; while (t == FX_FLANGER || t == FX_PHASER);
    edit_begin(); fx_set_type(f, t); mbx_clear(f); fx_turn_on(f); }
static int8_t *unit_param(int unit, int k, int *lo, int *hi, int *def) {   // returns, master: their parameter k
    static const int8_t rvd[8] = {50, 40, 50, 10, 100, 20, 80, 0}, dld[8] = {7, 35, 70, 20, 0, 100, 80, 0}, msd[12] = {0, 70, 20, 30, 40, 0, 1, 89, 30, 0, 0, 30};
    if (unit == NSLOT) { rv_range(k, lo, hi); *def = rvd[k]; return &song.mix.rv[k]; }
    if (unit == NSLOT + 1) { dl_range(k, lo, hi); *def = dld[k]; return &song.mix.dl[k]; }
    ms_range(k, lo, hi); *def = msd[k]; return &song.mix.ms[k];
}
static int unit_nparams(int unit) { return unit == NSLOT + 2 ? 9 : 8; }   // master: the duck rows come later
static int unit_split(int u) { return u == NSLOT + 2 ? 6 : (unit_nparams(u) + 1) / 2; }   // the left column: MASTER keeps COMP's six there, LIMITER's three on the right (2026-11-24)
// ---------- DUCK window (2026-11-11, user: "more flexible, a window like Kickstart 2 / ShaperBox 3") ----------
// EDIT on a strip's DK row. The curve: 16 points of level along the dip (Song.dkc), filled from a SHAPE or set point by
// point. The fields: TRIGGER (NOTE of the source pad, its SOUND passing a threshold, GRID on the tempo), SOURCE, AMOUNT,
// LENGTH of the curve, GRID step or THRESHOLD, SMOOTH, SHAPE. Opening it gives an old duck the same sound as before.
static int ducking, dk_row = 1, dk_col, dk_pt;     // row 0 the curve (dk_pt), row 1 the fields (dk_col)
enum { DF_MODE, DF_SRC, DF_AMT, DF_LEN, DF_RATE, DF_SMOOTH, DF_SHAPE, DF_N };
static const char *const dk_len_name[DK_NLEN] = {"1/16", "1/8", "1/4", "1/2", "1 BAR", "2 BARS"};
static const char *const dk_mode_name[DM_N] = {"NOTE", "SOUND", "GRID"};
#define DK_NSHAPE 8
static const char *const dk_shape_name[DK_NSHAPE] = {"CLASSIC", "FAST", "SLOW", "HOLD", "GATE", "LINEAR", "PUMP", "LATE"};
static const uint8_t dk_shape[DK_NSHAPE][16] = {
    {0, 13, 25, 36, 46, 56, 64, 72, 78, 84, 89, 93, 96, 98, 100, 100},            // the duck as it was: back up fast, then gently
    {0, 35, 58, 74, 85, 92, 96, 98, 100, 100, 100, 100, 100, 100, 100, 100},      // out of the way for a moment only
    {0, 2, 5, 9, 14, 20, 27, 35, 44, 53, 63, 73, 82, 90, 96, 100},                // comes back late
    {0, 0, 0, 0, 0, 0, 10, 25, 42, 58, 72, 83, 91, 96, 99, 100},                  // stays down, then up
    {0, 0, 0, 0, 0, 0, 0, 0, 100, 100, 100, 100, 100, 100, 100, 100},             // off for half, on for half
    {0, 7, 13, 20, 27, 33, 40, 47, 53, 60, 67, 73, 80, 87, 93, 100},
    {0, 1, 4, 10, 20, 33, 48, 63, 76, 86, 93, 97, 99, 100, 100, 100},             // the pumping S
    {100, 100, 100, 100, 100, 100, 100, 100, 60, 25, 5, 0, 10, 40, 75, 100},      // dips in the second half: before the next hit
};
static void line(int x0, int y0, int x1, int y1, int t, Col c); static void draw_footer(const char *hint); 
static void duck_init(int s) {            // the window's values for a pad that has none: exactly the duck it had
    int8_t *dx = song.dkx[s]; if (dx[DX_MARK] == DX_INIT) return;
    memset(dx, 0, 8); dx[DX_MODE] = DM_NOTE; dx[DX_LEN] = 2; dx[DX_GRID] = 2; dx[DX_THRESH] = 40; dx[DX_SHAPE] = 0; dx[DX_MARK] = DX_INIT;
    memcpy(song.dkc[s], dk_shape[0], 16);
}
static void dk_make_pts(int s) {          // the first change of the curve turns it into points (dkx[DX_SPARE] = DK_PTS)
    int8_t *dx = song.dkx[s]; if (dx[DX_SPARE] == DK_PTS) return;
    dk_table_to_pts(song.dkc[s], song.dkc[s]); dx[DX_SPARE] = DK_PTS; dx[DX_SHAPE] = DK_USER;
}
static const uint8_t *dk_view_pts(int s) { static uint8_t t[16]; if (song.dkx[s][DX_SPARE] == DK_PTS) return song.dkc[s]; dk_table_to_pts(song.dkc[s], t); return t; }
static void duck_open(int s) { ducking = 1; sel = s; dk_row = 1; dk_col = DF_MODE; dk_pt = 0; duck_init(s); }
static void duck_field(int f, int d, int big) {   // EDIT+arrows on a field
    PadMix *pm = &song.mix.pad[sel]; int8_t *dx = song.dkx[sel];
    switch (f) {
    case DF_MODE: dx[DX_MODE] = (int8_t)((dx[DX_MODE] + d + DM_N) % DM_N); if (dx[DX_MODE] == DM_GRID && pm->spare[DK_AMT] <= 0) pm->spare[DK_AMT] = 50; break;
    case DF_SRC: { int v = pm->spare[DK_SRC]; do v = (v + d + NSLOT + 1) % (NSLOT + 1); while (v - 1 == sel); pm->spare[DK_SRC] = (int8_t)v; if (v && pm->spare[DK_AMT] <= 0) pm->spare[DK_AMT] = 50; break; }
    case DF_AMT: pm->spare[DK_AMT] = (int8_t)clampi(pm->spare[DK_AMT] + d * (big ? 10 : 1), 0, 100); break;
    case DF_LEN: dx[DX_LEN] = (int8_t)clampi(dx[DX_LEN] + d, 0, DK_NLEN - 1); break;
    case DF_RATE: if (dx[DX_MODE] == DM_GRID) dx[DX_GRID] = (int8_t)clampi(dx[DX_GRID] + d, 0, DK_NLEN - 1); else if (dx[DX_MODE] == DM_AUDIO) dx[DX_THRESH] = (int8_t)clampi(dx[DX_THRESH] + d * (big ? 10 : 1), 0, 100); break;
    case DF_SMOOTH: dx[DX_SMOOTH] = (int8_t)clampi(dx[DX_SMOOTH] + d * (big ? 10 : 1), 0, 100); break;
    case DF_SHAPE: { int v = dx[DX_SHAPE] == DK_USER ? (d > 0 ? 0 : DK_NSHAPE - 1) : (dx[DX_SHAPE] + d + DK_NSHAPE) % DK_NSHAPE; dx[DX_SHAPE] = (int8_t)v; memcpy(song.dkc[sel], dk_shape[v], 16); dx[DX_SPARE] = 0; break; }
    }
}
static void duck_key(int k, int down) {
    static int combo, xcombo;                // EDIT (X) was used with another button: its release is no tap
    if (!down) { if (k == K_EDIT && !combo && dk_row == 1) { edit_begin(); duck_field(dk_col, 1, 0); }   // a tap on a field: its next value
        if (k == K_X && !xcombo && dk_row == 0) {   // X: a new point after this one, half way to the next
            uint8_t *c = song.dkc[sel]; dk_make_pts(sel); int n = dk_npts(c);
            if (n >= DK_NPT) { toast("8 POINTS: THE MOST"); return; }
            DkPt a = dk_pt_get(c, dk_pt), b = dk_pt < n - 1 ? dk_pt_get(c, dk_pt + 1) : (DkPt){1, 31, a.y, 0};
            if (b.x - a.x < 2) { toast("NO ROOM: MOVE A POINT"); return; }
            edit_begin(); DkPt m = {1, (a.x + b.x) / 2, (int)(dk_pts_eval(c, (a.x + b.x) / 2 / 31.f) * 63 / 100 + 0.5f), 0};
            for (int i = n; i > dk_pt + 1; i--) dk_pt_put(c, i, dk_pt_get(c, i - 1));
            dk_pt_put(c, dk_pt + 1, m); dk_pt++; }
        return; }
    if (k == K_X) { xcombo = 0; return; }
    if (held[K_OPT] && !held[K_EDIT] && (k == K_UP || k == K_DOWN || k == K_RIGHT) && dk_row == 0) {   // 2026-11-24 (M8): OPT+> a new point, OPT+^v the bend (were X)
        used[K_OPT] = 1;
        if (k == K_RIGHT) { xcombo = 0; duck_key(K_X, 0); return; }
        held[K_X] = 1; duck_key(k, 1); held[K_X] = 0; return; }
    used[k] = 1;
    if (k == K_EDIT && !held[K_OPT]) { combo = 0; return; }
    if (held[K_EDIT] || k == K_EDIT) combo = 1;
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_OPT && !held[K_EDIT]) { listen(sel); return; }
    if (k == K_Y || k == K_MENU) { leave_screen(); ducking = 0; return; }
    PadMix *pm = &song.mix.pad[sel]; int8_t *dx = song.dkx[sel]; uint8_t *cv = song.dkc[sel];
    { int n = dk_npts(dk_view_pts(sel)); if (dk_pt >= n) dk_pt = n ? n - 1 : 0; }   // another pad may have fewer points
    int arrow = k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT;
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) {      // OPT+EDIT: the point back up, the field's default
        edit_begin();
        if (dk_row == 0) { dk_make_pts(sel); int n = dk_npts(cv);   // the point goes (two stay)
            if (n <= 2) { toast("TWO POINTS AT LEAST"); return; }
            for (int i = dk_pt; i < n - 1; i++) dk_pt_put(cv, i, dk_pt_get(cv, i + 1));
            dk_pt_put(cv, n - 1, (DkPt){0, 0, 0, 0}); if (dk_pt >= n - 1) dk_pt = n - 2; }
        else switch (dk_col) { case DF_MODE: dx[DX_MODE] = DM_NOTE; break; case DF_SRC: pm->spare[DK_SRC] = 0; break; case DF_AMT: pm->spare[DK_AMT] = 0; break; case DF_LEN: dx[DX_LEN] = 2; break;
            case DF_RATE: dx[DX_GRID] = 2; dx[DX_THRESH] = 40; break; case DF_SMOOTH: dx[DX_SMOOTH] = 0; break; case DF_SHAPE: dx[DX_SHAPE] = 0; memcpy(cv, dk_shape[0], 16); break; }
        return;
    }
    if (k == K_L1 || k == K_R1) { sel = (sel + (k == K_R1 ? 1 : NSLOT - 1)) % NSLOT; mcol = sel; duck_init(sel); return; }   // the next pad's duck
    if (arrow && held[K_EDIT]) {
        edit_begin(); int d = k == K_RIGHT || k == K_UP ? 1 : -1;
        if (dk_row == 1) { duck_field(dk_col, d, k == K_UP || k == K_DOWN); return; }
        dk_make_pts(sel); int n = dk_npts(cv); DkPt p = dk_pt_get(cv, dk_pt);
        if (k == K_UP || k == K_DOWN) p.y = clampi(p.y + d * 2, 0, 63);   // the level
        else { int lo = dk_pt > 0 ? dk_pt_get(cv, dk_pt - 1).x : 0, hi = dk_pt < n - 1 ? dk_pt_get(cv, dk_pt + 1).x : 31; p.x = clampi(p.x + d, lo, hi); }   // along, between its neighbours
        dk_pt_put(cv, dk_pt, p); return;
    }
    if (arrow && held[K_X] && dk_row == 0 && (k == K_UP || k == K_DOWN)) {   // X+^v: the bend of the curve to the next point
        xcombo = 1; edit_begin(); dk_make_pts(sel); DkPt p = dk_pt_get(cv, dk_pt);
        if (dk_pt >= dk_npts(cv) - 1) { toast("THE LAST POINT HAS NO CURVE AFTER IT"); return; }
        p.bend = clampi(p.bend + (k == K_UP ? 1 : -1), -7, 7); dk_pt_put(cv, dk_pt, p); return; }
    if (!arrow) return;
    if (dk_row == 1) {                   // UI v2 (A20): the fields as cards in two columns
        if (k == K_LEFT) { if (dk_col & 1) dk_col--; } else if (k == K_RIGHT) { if (!(dk_col & 1) && dk_col + 1 < DF_N) dk_col++; }
        else if (k == K_DOWN) { if (dk_col + 2 < DF_N) dk_col += 2; } else if (dk_col >= 2) dk_col -= 2; else dk_row = 0;
        return; }
    if (k == K_UP) dk_row = 0; else if (k == K_DOWN) dk_row = 1;
    else if (dk_row == 0) { int n = dk_npts(dk_view_pts(sel)); dk_pt = clampi(dk_pt + (k == K_RIGHT ? 1 : -1), 0, n > 0 ? n - 1 : 0); }
    else dk_col = (dk_col + (k == K_RIGHT ? 1 : DF_N - 1)) % DF_N;
}
static void mix_arrow(int k) {           // EDIT+arrows on the mixer
    if (v2_mixer()) { mixer2_arrow(k); return; }
    int d = k == K_RIGHT || k == K_UP ? 1 : -1, big = k == K_UP || k == K_DOWN;
    if (mcol < NSLOT) {
        switch (mrow) {
        case MX_INS1: case MX_INS2: if (big) act_insert_onoff(mcol, mrow); else act_insert_type_step(mcol, mrow, d); break;
        case MX_SA: act_send_step(mcol, 0, d * (big ? 5 : 1)); break;
        case MX_SB: act_send_step(mcol, 1, d * (big ? 5 : 1)); break;
        case MX_DUCK:                          // EDIT+<> which pad's notes duck this one, EDIT+^v how much
            if (big) act_duck_amount_step(mcol, d * 10); else act_duck_source_step(mcol, d);
            break;
        case MX_PAN: act_pad_pan_step(mcol, d * (big ? 10 : 1)); break;
        case MX_VOL: act_pad_volume_step(mcol, d * (big ? 5 : 1)); break;
        }
        return;
    }
    if (mcol < NSLOT + 2) {              // returns: VOL = LEVEL, PAN = WIDTH, MUTE
        if (mrow == MX_VOL) act_unit_param_add(mcol, mcol == NSLOT ? RV_LEVEL : DL_LEVEL, d * (big ? 5 : 1));
        else if (mrow == MX_PAN) act_unit_param_add(mcol, mcol == NSLOT ? RV_WIDTH : DL_WIDTH, d * (big ? 5 : 1));
        return;
    }
    if (mrow == MX_VOL) act_master_step(d * (big ? 5 : 1));
    else if (mrow == MX_INS1 || mrow == MX_INS2) act_master_fx_toggle(mrow == MX_INS1 ? MS_COMP : MS_LIM);
}
static void mix_tap(void) {
    if (v2_mixer() && mixer2_tap()) return;
    if (mcol < NSLOT) {
        Track *t = &song.t[mcol]; PadMix *pm = &song.mix.pad[mcol];
        if (mrow == MX_MUTE) { t->mute = !t->mute; return; }                 // live moves: no undo step
        if (mrow == MX_SOLO) { pm->solo = !pm->solo; return; }
        if (mrow == MX_DUCK) { duck_open(mcol); return; }
        if (mrow == MX_FRZ) { if (!has_sound(mcol)) { toast("EMPTY PAD: NOTHING TO FREEZE"); return; } freeze_set(mcol, !frozen(mcol)); if (!frozen(mcol)) toast("UNFROZEN"); return; }
        if (mrow == MX_INS1 || mrow == MX_INS2) { fx_unit = mcol; sel = mcol; fcol = mrow; frow = 0; set_tab(T_FX); return; }
        return;
    }
    if (mcol < NSLOT + 2 && mrow == MX_MUTE) { if (mcol == NSLOT) song.mix.rv[RV_MUTE] = !song.mix.rv[RV_MUTE]; else song.mix.dl[DL_MUTE] = !song.mix.dl[DL_MUTE]; return; }
    if (mrow == MX_INS1 || mrow == MX_INS2) { fx_unit = mcol; fcol = 0; frow = 0; set_tab(T_FX); }
}
static void mix_delete(void) {           // OPT+EDIT: back to the default
    if (mcol >= NSLOT) { if (mcol == NSLOT + 2 && mrow == MX_VOL) { edit_begin(); song.master = 80; } return; }
    Track *t = &song.t[mcol]; PadMix *pm = &song.mix.pad[mcol]; edit_begin();
    switch (mrow) {
    case MX_INS1: case MX_INS2: fx_set_type(&pm->ins[mrow], FX_NONE); break;
    case MX_SA: pm->send_a = 0; break; case MX_SB: pm->send_b = 0; break;
    case MX_DUCK: pm->spare[DK_SRC] = 0; if (song.dkx[mcol][DX_MARK] == DX_INIT && song.dkx[mcol][DX_MODE] == DM_GRID) pm->spare[DK_AMT] = 0; break;   // off (on the grid: no amount)
    case MX_PAN: t->pan = 0; break; case MX_VOL: t->vol = 75; break;
    }
}
static int v2_mix(void); static void mix2_move(int k); static int v2_mixer(void); static void mixer2_move(int k); static void mixer2_arrow(int k); static int mixer2_tap(void);
static void mix_move(int k) {
    if (v2_mix()) { mix2_move(k); return; }
    if (v2_mixer()) { mixer2_move(k); return; }
    if (k == K_LEFT) mcol = (mcol + NSLOT + 2) % (NSLOT + 3); else if (k == K_RIGHT) mcol = (mcol + 1) % (NSLOT + 3);
    else mrow = clampi(mrow + (k == K_DOWN ? 1 : -1), 0, MX_ROWS - 1);
    if (mcol < NSLOT) sel = mcol;
}
// FX tab
static int v2_fx(void); static void fx2_move(int k); static int v2_lfo(void); static void lfo2_move(int k);
static void fx_move(int k) {
    if (v2_fx()) { fx2_move(k); return; }
    if (v2_lfo()) { lfo2_move(k); return; }
    if (fx_unit < NSLOT && fx_page) { if (k == K_LEFT || k == K_RIGHT) lcol = (lcol + (k == K_RIGHT ? 1 : NLFO - 1)) % NLFO; else lrow = clampi(lrow + (k == K_DOWN ? 1 : -1), 0, 7); return; }   // 7 rows (LR_N, below)
    if (fx_unit < NSLOT) {
        if (frow == 8) { if (k == K_UP) { frow = 7; fcol = fcol >= 2; } else if (k == K_LEFT || k == K_RIGHT) fcol = clampi(fcol + (k == K_RIGHT ? 1 : -1), 0, 3); return; }
        if (k == K_RIGHT) { if (!fx_sub && fx_multi_row(fcol, frow)) fx_sub = 1; else if (fcol == 0) { fcol = 1; fx_sub = 0; } return; }
        if (k == K_LEFT) { if (fx_sub) fx_sub = 0; else if (fcol == 1) { fcol = 0; fx_sub = fx_multi_row(0, frow); } return; }
        frow = clampi(frow + (k == K_DOWN ? 1 : -1), 0, 8);
        if (fx_sub && !fx_multi_row(fcol, frow)) fx_sub = 0;
        if (frow == 8) fcol = fcol * 2;
        return;
    }
    int n = unit_nparams(fx_unit), half = unit_split(fx_unit);   // two columns of parameters
    if (k == K_LEFT || k == K_RIGHT) { fcol = k == K_RIGHT; if (fcol && frow >= n - half) frow = n - half - 1; return; }
    frow = clampi(frow + (k == K_DOWN ? 1 : -1), 0, (fcol ? n - half : half) - 1);
}
static void lfo_value(int d, int big);
static void fx_value(int d, int big) {
    if (fx_unit < NSLOT && fx_page) { lfo_value(d, big); return; }
    if (fx_unit >= NSLOT) {
        int half = unit_split(fx_unit), kk = fcol * half + frow, lo, hi, def;
        unit_param(fx_unit, kk, &lo, &hi, &def); act_unit_param_add(fx_unit, kk, d * (big && hi > 10 ? 10 : 1)); return;
    }
    PadMix *pm = &song.mix.pad[fx_unit];
    if (frow == 8) {
        int dd = d * (big ? 10 : 1);
        switch (fcol) { case 0: act_send_step(fx_unit, 0, dd); break; case 1: act_send_step(fx_unit, 1, dd); break;
                        case 2: act_pad_pan_step(fx_unit, dd); break; default: act_pad_volume_step(fx_unit, dd); }
        return;
    }
    FxSlot *f = &pm->ins[fcol];
    if (frow == 9 && fx_is_mod(f->type)) { edit_begin(); fx_mod_mode(f, f->type - FX_CHORUS + d); return; }   // MODULATION's MODE
    if (frow == 0) { act_insert_type_step(fx_unit, fcol, d); return; }
    if (frow == 1) { act_insert_on(fx_unit, fcol, d > 0); return; }
    if (!f->type) return;
    if (fx_sub && fx_multi_row(fcol, frow)) { int lo2, hi2; int8_t *g = fx_side(fcol, frow, &lo2, &hi2); edit_begin(); *g = (int8_t)clampi(*g + d * (big && hi2 > 12 ? 6 : 1), lo2, hi2); return; }
    act_insert_param_step(fx_unit, fcol, frow - 2, d, big);
}
static void fx_tap(void) {
    if (fx_unit < NSLOT && fx_page) { if (lrow == 0) learn_lfo_start(fx_unit, lcol); else lfo_value(1, 0); return; }   // TARGET: point at a knob
    if (fx_unit >= NSLOT) { int half = unit_split(fx_unit), kk = fcol * half + frow, lo, hi, def; int8_t *v = unit_param(fx_unit, kk, &lo, &hi, &def);
        if (hi == 1) { edit_begin(); *v = !*v; } return; }
    if (frow == 8) return;
    FxSlot *f = &song.mix.pad[fx_unit].ins[fcol];
    if (frow == 9 && fx_is_mod(f->type)) { edit_begin(); fx_mod_mode(f, (f->type - FX_CHORUS + 1) % 3); }   // EDIT: the next mode, round
    else if (frow == 0) fx_cycle(f, 1);
    else if (frow == 1) { edit_begin(); if (f->on) f->on = 0; else fx_turn_on(f); }
    else if (frow >= 2 && frow - 2 < 8) { int lo, hi; fx_range(f->type, frow - 2, &lo, &hi); if (hi - lo == 1) { edit_begin(); f->p[frow - 2] = (int8_t)(f->p[frow - 2] == lo ? hi : lo); } }   // a switch: EDIT flips it
}
static void fx_delete(void) {
    if (fx_unit < NSLOT && fx_page && lrow == 0) { char b[24]; edit_begin(); song.lfo[fx_unit][lcol].target = 0; snprintf(b, sizeof b, "LFO %d UNLINKED", lcol + 1); toast(b); return; }   // 2026-11-12, user
    if (fx_unit < NSLOT && fx_page) { edit_begin(); memset(&song.lfo[fx_unit][lcol], 0, sizeof(Lfo)); toast("LFO OFF"); return; }
    if (fx_unit >= NSLOT) { int half = unit_split(fx_unit), kk = fcol * half + frow, lo, hi, def; int8_t *v = unit_param(fx_unit, kk, &lo, &hi, &def); edit_begin(); *v = (int8_t)def; return; }
    PadMix *pm = &song.mix.pad[fx_unit]; edit_begin();
    if (frow == 8) { if (fcol == 0) pm->send_a = 0; else if (fcol == 1) pm->send_b = 0; else if (fcol == 2) song.t[fx_unit].pan = 0; else song.t[fx_unit].vol = 75; return; }
    FxSlot *f = &pm->ins[fcol];
    if (fx_sub && fx_multi_row(fcol, frow)) { int lo2, hi2; *fx_side(fcol, frow, &lo2, &hi2) = 0; return; }
    if (frow == 9) { if (fx_is_mod(f->type)) fx_mod_mode(f, 0); return; }
    if (frow <= 1) { fx_set_type(f, FX_NONE); mbx_clear(f); } else if (f->type) f->p[frow - 2] = fx_def[f->type][frow - 2];
}
// ---------- MOD page of the FX tab: 4 LFOs per pad (2026-11-09; user: in the FX tab for now, Y switches FX/MOD) ----------
static int lcol, lrow;                   // the LFO column and its row
enum { LR_TARGET, LR_SHAPE, LR_SYNC, LR_RATE, LR_DEPTH, LR_TRIG, LR_PHASE, LR_POL, LR_N };   // POL (2026-11-25, Serum): BIPOLAR / UNIPOLAR
static const char *const lr_name[LR_N] = {"TARGET", "SHAPE", "CLOCK", "RATE", "DEPTH", "START", "PHASE", "POLARITY"};
static const char *const ls_name[LS_N] = {"SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "S&H", "SMOOTH", "DUCK"};
static const char *const ab_pn[11] = {"WAVE", "TUNE", "CUTOFF", "RESO", "ENV MOD", "DECAY", "ACCENT", "DIST TYPE", "DRIVE", "DIST TONE", "DIST MIX"};
static const char *const cl_pn[21] = {"POSITION", "SPRAY", "SIZE", "DENSITY", "SHIMMER", "MOTION", "SIZE RND", "WINDOW", "REVERSE", "GRAIN PITCH", "GRAIN FINE", "PITCH RND", "INTERVALS", "SYNC", "TIME RND", "WIDTH", "SCAN", "FREEZE", "ATTACK", "RELEASE", "TONE"};
static int lfo_targets(int s, int *out) {   // what an LFO on this pad can move, in order
    int n = 0, ty = song.t[s].type; out[n++] = LT_NONE;
    if (ty != 3) out[n++] = LT_PITCH;
    out[n++] = LT_VOL; out[n++] = LT_PAN; out[n++] = LT_SENDA; out[n++] = LT_SENDB;
    int ne = ty == 2 ? AP_N : ty == 3 ? CP_N + CX_N : 0;
    for (int k = 0; k < ne; k++) out[n++] = LT_ENG + k;
    if (ty == 1) for (int k = 0; k < LT_ENGN; k++) if (wp_info[k].name && wp_info[k].mod) out[n++] = LT_ENG + k;   // WAVE: what its matrix may move, OSC A .. ENV1
    int nd = ty == 2 ? AX_N : 0;   // the DEEP page's knobs
    for (int k = 0; k < nd; k++) out[n++] = LT_ENG + LT_DEEP + k;
    for (int i = 0; i < NINS; i++) { const FxSlot *f = &song.mix.pad[s].ins[i];
        if (f->type) for (int k = 0; k < 6; k++) if (fx_pname[f->type][k][0]) out[n++] = (i ? LT_INS2 : LT_INS1) + k; }
    return n;
}
static void lfo_tname(int s, int tg, char *b) {
    int ty = song.t[s].type;
    if (tg == LT_NONE) snprintf(b, 24, "OFF"); else if (tg == LT_PITCH) snprintf(b, 24, "PITCH"); else if (tg == LT_VOL) snprintf(b, 24, "VOLUME");
    else if (tg == LT_PAN) snprintf(b, 24, "PAN"); else if (tg == LT_SENDA) snprintf(b, 24, "SEND A"); else if (tg == LT_SENDB) snprintf(b, 24, "SEND B");
    else if (tg >= LT_INS1 && tg < LT_ENG) { int i = tg >= LT_INS2, k = tg - (i ? LT_INS2 : LT_INS1); const FxSlot *f = &song.mix.pad[s].ins[i];
        snprintf(b, 24, "%s %s", fx_short[f->type], f->type ? fx_pname[f->type][k] : "?"); }
    else if (ty == 1 && tg >= LT_ENG) { const char *nm = wp_name(tg - LT_ENG); snprintf(b, 24, "%s", nm ? nm : "?"); }   // WAVE: "A POS", "CUTOFF"
    else if (tg >= LT_ENG + LT_DEEP) { int k = tg - LT_ENG - LT_DEEP; snprintf(b, 24, "%s", ty == 2 && k < AX_N ? par_name[P_AX + k] : "?"); }
    else { int k = tg - LT_ENG; snprintf(b, 24, "%s", ty == 2 && k < 11 ? ab_pn[k] : ty == 3 && k < 21 ? cl_pn[k] : "?"); }
}
static void lfo_value(int d, int big) {
    Lfo *L = &song.lfo[fx_unit][lcol]; edit_begin();
    switch (lrow) {
    case LR_TARGET: { int t[64], n = lfo_targets(fx_unit, t), i = 0; while (i < n && t[i] != L->target) i++;
        L->target = (uint8_t)t[((i < n ? i : 0) + d + n) % n];
        if (L->target && !L->depth) { L->depth = 50; if (!L->sync && !L->rate) { L->sync = 1; L->rate = 8; } } break; }
    case LR_SHAPE: L->shape = (uint8_t)((L->shape + d + LS_N) % LS_N); break;
    case LR_SYNC: L->sync = !L->sync; L->rate = L->sync ? 8 : 50; break;
    case LR_RATE: L->rate = (uint8_t)(L->sync ? clampi(L->rate + d, 0, LFO_NSYNC - 1) : clampi(L->rate + d * (big ? 10 : 1), 0, 100)); break;
    case LR_DEPTH: L->depth = (int8_t)clampi(L->depth + d * (big ? 10 : 1), -100, 100); break;
    case LR_TRIG: L->retrig = (uint8_t)((L->retrig + (d < 0 ? -1 : 1) + NSLOT + 2) % (NSLOT + 2)); break;   // FREE RUN, EACH NOTE, NOTE OF PAD 1..12
    case LR_PHASE: L->phase = (uint8_t)((L->phase + d * (big ? 10 : 1) + 101) % 101); break;
    case LR_POL: L->spare ^= 1; break;
    }
}
static void lfo_str(int s, int j, int row, char *b) {
    const Lfo *L = &song.lfo[s][j];
    switch (row) {
    case LR_TARGET: lfo_tname(s, L->target, b); break;
    case LR_SHAPE: snprintf(b, 24, "%s", ls_name[L->shape % LS_N]); break;
    case LR_SYNC: snprintf(b, 24, "%s", L->sync ? "TEMPO" : "FREE"); break;
    case LR_RATE: if (L->sync) snprintf(b, 24, "%s", L->rate < NDIV ? div_label[L->rate] : L->rate == NDIV ? "2 BARS" : "4 BARS"); else snprintf(b, 24, "%.2fHZ", 0.02 * pow(1000, L->rate / 100.0)); break;
    case LR_DEPTH: snprintf(b, 24, "%+d", L->depth); break;
    case LR_POL: snprintf(b, 24, "%s", L->spare & 1 ? "UNIPOLAR" : "BIPOLAR"); break;
    case LR_TRIG: if (L->retrig >= 2) snprintf(b, 24, "PAD %d NOTE", L->retrig - 1); else snprintf(b, 24, "%s", L->retrig ? "EACH NOTE" : "FREE RUN"); break;
    default: snprintf(b, 24, "%d", L->phase);
    }
}
static void fx_unit_step(int d) { fx_unit = (fx_unit + d + NSLOT + 3) % (NSLOT + 3); if (fx_unit < NSLOT) sel = fx_unit; frow = 0; fcol = 0; }

// ---------- PRESETS (2026-11-09): factory catalogue (src/presets.h from tools/mkpresets.py) + the user's own (presets.cfg) ----------
#define PRESET_FILE "presets.cfg"
#define NUSERP 96
typedef struct { char name[16]; uint8_t engine; int8_t v[WP_N]; } UserPreset;   // v: PRESET_N values, or a WAVE block (WP_N)
#define PR_MAX (NPRESET + NWPRESET + NUSERP + 1)   // the rows a list may have: SAVE CURRENT, the user's, the factory's
static UserPreset upre[NUSERP]; static int nupre;
static int presetting, pr_cur, pr_top, pr_heard;   // the window, cursor, first row shown, OPTION note
static Track pr_keep; static int8_t pr_keep_cx[16], pr_keep_x[WP_N];   // the pad as it was when the window opened: Y puts it back
static void presets_load(void) {
    nupre = 0; FILE *f = fopen(PRESET_FILE, "r"); if (!f) return;
    char line[640];
    while (nupre < NUSERP && fgets(line, sizeof line, f)) {
        UserPreset *u = &upre[nupre]; int e, n = 0, pos = 0; char nm[32];
        if (sscanf(line, "%d %31[^|]|%n", &e, nm, &pos) < 2 || e < 1 || e > 3) continue;
        memset(u, 0, sizeof *u); u->engine = (uint8_t)e; snprintf(u->name, sizeof u->name, "%s", nm);
        const char *p = line + pos; int v, k, wave = e == 1 && *p == 'W';   // "W": a WAVE block (2026-11-25); engine 1 without it: GRAND's
        if (wave) p++;
        while (n < (wave ? WP_N : PRESET_N) && sscanf(p, "%d%n", &v, &k) == 1) { u->v[n++] = (int8_t)clampi(v, -128, 127); p += k; }
        if (e == 1 && !wave) continue;   // an old GRAND preset: gone with GRAND (2026-11-25)
        if (e == 1) u->v[WP_MARK] = WP_INIT;
        nupre++;
    }
    fclose(f);
}
static void presets_save(void) {
    static char b[NUSERP * 1100]; int n = 0;
    for (int i = 0; i < nupre; i++) {
        n += snprintf(b + n, sizeof b - n, "%d %s|%s", upre[i].engine, upre[i].name, upre[i].engine == 1 ? "W" : "");
        int last = upre[i].engine == 1 ? WP_N - 1 : PRESET_N; while (last > 24 && !upre[i].v[last - 1]) last--;   // a preset with no DEEP data stays short (WAVE: the mark is set on load)
        for (int k = 0; k < last; k++) n += snprintf(b + n, sizeof b - n, " %d", upre[i].v[k]);
        n += snprintf(b + n, sizeof b - n, "\n");
    }
    write_whole(PRESET_FILE, b, (size_t)n);
}
static void preset_apply(int s, const int8_t *v) {   // values are clamped to the engine's ranges: a hand-edited file stays safe
    Track *t = &song.t[s]; int lo, hi;
    if (t->type == 1) { int8_t *wp = song.wv[s]; for (int k = 0; k < WP_N - 1; k++) { wp_range(k, &lo, &hi); wp[k] = (int8_t)clampi(v[k], lo, hi); }   // WAVE: the whole block
        wp[WP_MARK] = WP_INIT; t->voice = wp[WP_POLY] > 1; }
    else if (t->type == 2) { for (int k = 0; k < AP_N; k++) { ap_range(k, &lo, &hi); t->ap[k] = (int8_t)clampi(v[k], lo, hi); } t->ap[AP_MARK] = AP_INIT; ax_defaults(song.ax[s]);
        if (v[24 + AX_MARK] == AX_INIT) for (int k = 0; k < AX_N; k++) { ax_range(k, &lo, &hi); song.ax[s][k] = (int8_t)clampi(v[24 + k], lo, hi); } }
    else if (t->type == 3) { for (int k = 0; k < CP_N; k++) t->cp[k] = (int8_t)clampi(v[k], 0, 100); t->cp[CP_MARK] = CP_INIT;
        for (int k = 0; k < CX_N; k++) { cx_range(k, &lo, &hi); song.cx[s][k] = (int8_t)clampi(v[6 + k], lo, hi); } song.cx[s][CX_MARK] = CX_INIT; }
}
static void preset_capture(int s, int8_t *v) {
    const Track *t = &song.t[s]; memset(v, 0, PRESET_N);
    if (t->type == 1) memcpy(v, song.wv[s], WP_N);   // v holds WP_N here
    else if (t->type == 2) { memcpy(v, t->ap, AP_N); memcpy(v + 24, song.ax[s], 16); }
    else if (t->type == 3) { memcpy(v, t->cp, CP_N); memcpy(v + 6, song.cx[s], CX_N); }
}
// The list for this pad's engine: row 0 = SAVE CURRENT, then the user's presets, then the factory ones
static int pr_list(int *idx, int max) {
    int e = song.t[sel].type, n = 0; idx[n++] = -1;
    for (int i = 0; i < nupre && n < max; i++) if (upre[i].engine == e) idx[n++] = 1000 + i;
    for (int i = 0; i < NPRESET && n < max; i++) if (presets[i].engine == e) idx[n++] = i;
    if (e == 1) for (int i = 0; i < NWPRESET && n < max; i++) idx[n++] = 2000 + i;   // WAVE's factory presets (wpresets.h)
    return n;
}
static const char *pr_name(int i) { return i >= 2000 ? wpresets[i - 2000].name : i >= 1000 ? upre[i - 1000].name : i >= 0 ? presets[i].name : ""; }
static void wave_new(int s) {            // a pad becomes WAVE: its own block if it had one, else the INIT preset (two saws)
    if (song.wv[s][WP_MARK] != WP_INIT) wpreset_apply(song.wv[s], 0);
    song.t[s].voice = song.wv[s][WP_POLY] > 1;
}
static void pr_restore(void) { Track *t = &song.t[sel]; memcpy(t->ap, pr_keep.ap, 12); memcpy(t->cp, pr_keep.cp, 7); t->voice = pr_keep.voice; memcpy(song.cx[sel], pr_keep_cx, 16);
    if (t->type == 1) memcpy(song.wv[sel], pr_keep_x, WP_N); else if (t->type == 2) memcpy(song.ax[sel], pr_keep_x + 24, 16); }
static void pr_try(void) {               // the row under the cursor sounds on the pad at once (Y gives the old sound back)
    int idx[PR_MAX], n = pr_list(idx, PR_MAX); pr_cur = clampi(pr_cur, 0, n - 1);
    int i = idx[pr_cur];
    if (i >= 2000) { wpreset_apply(song.wv[sel], i - 2000); song.t[sel].voice = song.wv[sel][WP_POLY] > 1; }
    else if (i >= 1000) preset_apply(sel, upre[i - 1000].v); else if (i >= 0) preset_apply(sel, presets[i].v);
    else pr_restore();                   // SAVE CURRENT: the sound it had when opened
}
static void preset_open(void) {
    if (!is_synth(sel)) return;
    push_undo(); pr_keep = song.t[sel]; memcpy(pr_keep_cx, song.cx[sel], 16); preset_capture(sel, pr_keep_x);
    presetting = 1; pr_cur = 0; pr_top = 0;
}
static void preset_close(int keep) {
    if (pr_heard) { engine_release_key(302); pr_heard = 0; }
    if (!keep) pr_restore();
    presetting = 0;
}
static void preset_key(int k, int down) {
    int idx[PR_MAX], n = pr_list(idx, PR_MAX);
    if (!down) {
        if (k == K_OPT && pr_heard) { engine_release_key(302); pr_heard = 0; }
        if (k == K_EDIT && !used[K_EDIT]) {
            int i = idx[clampi(pr_cur, 0, n - 1)];
            if (i == -1) {               // SAVE CURRENT: the sound the pad had when the window opened
                if (nupre >= NUSERP) { toast("USER PRESETS FULL (96)"); return; }
                UserPreset *u = &upre[nupre]; memset(u, 0, sizeof *u); u->engine = (uint8_t)song.t[sel].type;
                int num = 1; for (int j = 0; j < nupre; j++) if (upre[j].engine == u->engine) num++;
                snprintf(u->name, sizeof u->name, "USER %02d", num);
                pr_restore(); preset_capture(sel, u->v);
                nupre++; presets_save(); char b[40]; snprintf(b, sizeof b, "SAVED AS %s", u->name); toast(b); pr_cur = 1; pr_try();
                return;
            }
            const char *nm = pr_name(i); char b[40]; snprintf(b, sizeof b, "PRESET %s", nm); toast(b);
            preset_close(1);
        }
        return;
    }
    if (k == K_Y) { preset_close(0); toast("PRESET: AS BEFORE"); return; }
    if (k == K_PLAY) { play_key(); return; }
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) {   // OPT+EDIT on a user preset: delete it
        used[K_EDIT] = used[K_OPT] = 1; if (pr_heard) { engine_release_key(302); pr_heard = 0; }
        int i = idx[clampi(pr_cur, 0, n - 1)];
        if (i >= 1000 && i < 2000) { int j = i - 1000; char b[40]; snprintf(b, sizeof b, "DELETED %s", upre[j].name);
            memmove(&upre[j], &upre[j + 1], sizeof *upre * (size_t)(nupre - j - 1)); nupre--; presets_save(); toast(b); pr_try(); }
        else toast("ONLY USER PRESETS CAN BE DELETED");
        return;
    }
    if (k == K_OPT) { if (!(engine_playing() && song.t[sel].on)) { engine_release_key(302); engine_trig_key(sel, 1.f, 0, -1, 302); pr_heard = 1; flash[sel] = SDL_GetTicks(); } return; }
    if (k == K_UP || k == K_DOWN || k == K_L1 || k == K_R1) {
        int d = k == K_DOWN ? 1 : k == K_UP ? -1 : 0;
        if (k == K_L1 || k == K_R1) {    // UI v2 (A12): L1/R1 between the two tabs, FACTORY and MINE
            int ic = idx[clampi(pr_cur, 0, n - 1)], mine = ic < 0 || (ic >= 1000 && ic < 2000), j = 0;
            if (mine) { while (j < n && (idx[j] < 0 || (idx[j] >= 1000 && idx[j] < 2000))) j++; if (j >= n) j = 0; }
            pr_cur = j; pr_try(); return;
        }
        if (0) {
        } else pr_cur = clampi(pr_cur + d, 0, n - 1);
        pr_try();
        if (pr_heard) { engine_release_key(302); engine_trig_key(sel, 1.f, 0, -1, 302); }   // OPTION held: the new sound at once
    }
}

// ---------- RANDOM: rhythm and melody in the pad's key (2026-11-09, user: a window with settings, SHIFT+X in SEQ+INST) ----------
static int randing;                      // the window is open
enum { RN_WHAT, RN_DENS, RN_FEEL, RN_ACC, RN_SLIDE, RN_RANGE, RN_MOVE, RN_HOLD, RN_GO, RN_ROWS };
static int rn_v[RN_GO] = {2, 50, 0, 25, 15, 1, 0, 20}, rn_row = RN_GO;
static const char *const rn_name[RN_ROWS] = {"MAKE", "DENSITY", "FEEL", "ACCENTS", "SLIDES", "RANGE", "MELODY", "LONG NOTES", "GENERATE"};
static void rn_range(int k, int *lo, int *hi) { *lo = 0; *hi = k == RN_WHAT || k == RN_FEEL ? 2 : k == RN_MOVE ? 1 : k == RN_RANGE ? 3 : 100; if (k == RN_RANGE) *lo = 1; }
static void rn_str(int k, char *b) {
    static const char *const what[3] = {"RHYTHM", "MELODY", "BOTH"}, *const feel[3] = {"STRAIGHT", "SYNCOPATED", "BROKEN"}, *const mv[2] = {"STEPS", "LEAPS"};
    if (k == RN_WHAT) snprintf(b, 16, "%s", what[rn_v[k]]); else if (k == RN_FEEL) snprintf(b, 16, "%s", feel[rn_v[k]]);
    else if (k == RN_MOVE) snprintf(b, 16, "%s", mv[rn_v[k]]); else if (k == RN_RANGE) snprintf(b, 16, "%d OCT", rn_v[k]);
    else if (k == RN_GO) b[0] = 0; else snprintf(b, 16, "%d%%", rn_v[k]);
}
static int rn_chance(int pct) { return rand() % 100 < pct; }
static void rnd_generate(void) {
    Pattern *p = cur_pat(sel); push_undo();
    int len = p->len ? p->len : 16, sl = scale_len(track_scale(sel)), acid = song.t[sel].type == 2;
    int rhythm = rn_v[RN_WHAT] != 1, melody = rn_v[RN_WHAT] != 0, d = rl_d32[p->div % 6] * RT;
    static uint8_t on[NSTEP]; static int8_t deg[NSTEP]; float w[NSTEP];
    for (int i = 0; i < len; i++) {      // where the hits fall: the feel weighs the steps
        w[i] = rn_v[RN_FEEL] == 0 ? (i % 4 == 0 ? 1.6f : i % 2 == 0 ? 1.0f : 0.5f)
             : rn_v[RN_FEEL] == 1 ? (i % 4 == 0 ? 1.0f : i % 4 == 2 ? 0.6f : 1.35f) : 0.3f + (rand() % 140) / 100.f;
        if (!rhythm) on[i] = p->mode ? 0 : p->trig[i];
        else on[i] = rand() % 1000 < (int)(1000 * (1 - powf(1 - rn_v[RN_DENS] / 100.f, w[i])));   // 100% fills every step, the feel shapes the rest
    }
    if (!rhythm && p->mode) for (int k = 0; k < NNOTE; k++) if (p->roll[k].len && p->roll[k].start / d < len) on[p->roll[k].start / d] = 1;
    if (rhythm && rn_v[RN_DENS] > 0) on[0] = 1;   // the one is always there
    int cur = 0, top = rn_v[RN_RANGE] * sl - 1;   // the melody: a walk on the scale's degrees from the tonic
    for (int i = 0; i < len; i++) {
        if (!on[i]) continue;
        if (i % 16 == 0 && rn_chance(35)) cur = 0;
        else if (rn_v[RN_MOVE]) cur += rand() % (2 * sl + 1) - sl;
        else { static const int mv[7] = {-2, -1, -1, 0, 1, 1, 2}; cur += mv[rand() % 7]; }
        if (cur < 0) cur = -cur; if (cur > top) cur = top - (cur - top); if (cur < 0) cur = 0;
        deg[i] = (int8_t)cur;
    }
    if (!p->mode) {
        for (int i = 0; i < len; i++) {
            if (rhythm) { p->trig[i] = on[i]; p->prob[i] = 100; p->rep[i] = 1; p->slice[i] = -1; if (!on[i]) p->note[i] = NOTE_NONE; }
            if (!on[i]) continue;
            int accent = rn_chance(rn_v[RN_ACC]);
            if (rhythm) p->vel[i] = (int8_t)(accent ? 127 : 70 + rand() % 30);
            p->note[i] = melody ? (int8_t)deg_midi(sel, deg[i]) : rhythm ? NOTE_NONE : p->note[i];
            if (acid && rhythm) { int f = (accent ? ACID_ACCENT : 0) | (rn_chance(rn_v[RN_SLIDE]) ? ACID_SLIDE : 0); p->slice[i] = (int8_t)(f ? f : -1); }
        }
    } else {
        if (rhythm) { for (int k = 0; k < NNOTE; k++) roll_del(p, k); }
        if (!rhythm && melody) {         // the notes keep their place and length, they get new pitches
            for (int k = 0; k < NNOTE; k++) if (p->roll[k].len) { int st = p->roll[k].start / d; if (st < len) p->roll[k].note = (uint8_t)clampi(roll_base(sel) + hit_semis(sel, deg[st]), 0, 127); }
        } else for (int i = 0; i < len; i++) {
            if (!on[i]) continue;
            int next = i + 1; while (next < len && !on[next]) next++;
            int ln = rn_chance(rn_v[RN_HOLD]) ? (next - i) + (rn_chance(rn_v[RN_SLIDE]) ? 1 : 0) : 1;   // long: to the next note (or into it: legato)
            int accent = rn_chance(rn_v[RN_ACC]);
            int k = roll_add(p, i * d, ln * d, melody ? roll_base(sel) + hit_semis(sel, deg[i]) : roll_base(sel), accent ? 127 : 70 + rand() % 30);
            if (k >= 0 && accent && acid) p->roll[k].flags |= NF_ACC;
        }
    }
    toast("GENERATED: SHIFT+L2 UNDOES");
}
static void rnd_open(void) {
    int drum = !is_synth(sel) && track_root(sel) < 0;
    if (drum && rn_v[RN_WHAT] != 0) rn_v[RN_WHAT] = 0;   // drums: rhythm by default (a melody would pitch them)
    randing = 1; rn_row = RN_GO;
}
static void rnd_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) { if (rn_row == RN_GO) rnd_generate(); else { int lo, hi; rn_range(rn_row, &lo, &hi); rn_v[rn_row] = rn_v[rn_row] >= hi ? lo : rn_v[rn_row] + (hi > 3 ? 10 : 1); } } return; }
    if (k == K_Y) { randing = 0; return; }
    if (k == K_PLAY) { play_key(); return; }
    if (held[K_SHIFT] && k == K_L2) { undo(); return; }
    if (k > K_RIGHT) return;
    if (held[K_EDIT] && rn_row < RN_GO) {
        used[K_EDIT] = 1; int lo, hi, dd = k == K_RIGHT || k == K_UP ? 1 : -1, big = k == K_UP || k == K_DOWN;
        rn_range(rn_row, &lo, &hi); rn_v[rn_row] = clampi(rn_v[rn_row] + dd * (big && hi > 3 ? 10 : big || hi <= 3 ? 1 : 5), lo, hi); return;
    }
    if (k == K_UP) rn_row = (rn_row + RN_ROWS - 1) % RN_ROWS; else if (k == K_DOWN) rn_row = (rn_row + 1) % RN_ROWS;
}

static void listen_name(int s, char *b) {   // the listening note by name where the pad has a pitch, else in semitones
    int n = song.lnote[s];
    if (is_synth(s) || track_root(s) >= 0) { int m = roll_base(s) + n; if (m < 0) m = 0; if (m > 127) m = 127; snprintf(b, 8, "%s%d", note_name[m % 12], m / 12 - 1); }
    else snprintf(b, 8, n ? "%+d ST" : "0 ST", n);
}
static int asking, naming, exporting; static void ask_key(int k, int down); static void nm_key(int k, int down); static void export_key(int k, int down);
static void draw_roll_mini(const Pattern *p, int x, int y, int w, int h, int st, int on); static void draw_chord_mini(const Pattern *p, int x, int y, int w, int h, int on);
#include "ui.h"
#include "sound.h"
static void icon(int id, int x, int y, Col c); static int slot_icon(int i); static Col pad_icol(int s, Col dflt);   // pages.h: the pads' icons and meters, drawn further down
static float mx_show[NSLOT + 3]; static void mx_meters(void); static void meter_bar(int x, int y, int w, int h, float v);
#include "pages.h"
#include "simple.h"
#include "ctx.h"
static void xy_key(int k);
static void wv_learn_back(int m) { if (m < 16) { snd_page_go(snd_page_of(GF_WMTX)); sp_cur = 4 * m + WM_DST; } else { snd_page_go(snd_page_of(GF_WLFO)); sp_cur = 6; } }
static int wv_seq_tap(void) {
    if (!v2_sound()) return 0;
    const SndPage *pg = snd_page(); int p = cur_par();
    if (sp_cur < 0 && (pg->gfx == GF_WTA || pg->gfx == GF_WTB)) { wtb_open(pg->gfx == GF_WTB); return 1; }   // the picture of an oscillator: its tables
    if (p == P_WTGT) { wv_learn_start(16 + wv_lfo); return 1; }
    if (pg->gfx == GF_WMTX) { if (sp_cur >= 0 && sp_cur % 4 == WM_DST) wv_learn_start(sp_cur / 4); return 1; }   // the other fields: EDIT+arrows
    return 0;
}
static void key_event1(int k, int down) {
    if (k < 0) return;
    if ((k == K_X || k == K_Y) && !sim_on && !perform && !ui_simple) { if (down) xy_key(k); return; }   // 2026-11-21 (user): Y back everywhere, X the quick bar and its drawer
    if (k == K_PLAY && !down) play_release();
    input_key(k, down);
    if (k == K_OPT && !down && opt_gate) { opt_gate = 0; engine_release(); }
    if (k == K_EDIT && !down) undo_armed = 0;
    if (k == K_EDIT && !down && paint_anchor >= 0 && tab == T_SEQ) {   // a drag that erased steps: they forget their values too
        Pattern *p = cur_pat(sel); for (int i = 0; i < NSTEP; i++) if (paint_snap[i] && !p->trig[i]) step_reset(p, i);
    }
    if (k == K_EDIT && !down && gb_heard) { gb_heard = 0; engine_release_key(GB_TAG); }
    if (k == K_EDIT && down) paint_anchor = -1;
    input_combo(k, down);
    if (k == K_SHIFT && !down) quit_down = 0;
    if (k == K_PLAY && !down) quit_down = 0;
    if (k == K_PLAY && held[K_SHIFT]) { if (down) { quit_down = SDL_GetTicks() | 1; used[K_SHIFT] = 1; } return; }   // v3: SHIFT+PLAY held 3 s quits
    if (k == K_PLAY && held[K_EDIT] && tab == T_GRID && ui_cat < 0 && !perform && !ui_over() && !qb_focus) { if (down) { used[K_EDIT] = 1; sim_key(K_X, -1); } return; }   // EDIT+PLAY: this pad's clip
    if (x_latch_key(k, down)) return;
    if (qpicking) { qp_key(k, down); return; }   // PROJECT > SETTINGS > BAR BUTTONS
    if (held[K_MENU] && k == K_UP && y_back_here()) { if (down) { used[K_MENU] = 1; sim_key(K_Y, -1); } return; }   // v3: Y's way back
    if ((k == K_L1 || k == K_R1 || k == K_L2 || k == K_R2) && !sim_on && !perform && !ui_simple && !ui_over() && !naming && !exporting && !asking && !cfm && !learning
        && !held[K_SHIFT] && tab != T_PRJ) {   // SHIFT+L2 undo, SHIFT+L1/R1 the grid, PROJECT's held nudge stay theirs
        if (down) { int d = k == K_R1 || k == K_R2 ? 1 : -1; qb_focus = strip_focus = tab_focus = 0;   // v3: L/R page as F1/F2 do
            if (in_pad()) cat_step(d); else { ui_cat = -1; tab_step(d); } }
        return; }
    if (down && k != K_X) x_tap_t = 0;   // X, another key, X is not a double tap
    seq_clamp();
    if (learning && learn_key(k, down)) return;
    if (exporting) { export_key(k, down); return; }
    if (asking) { ask_key(k, down); return; }
    if (cfm) { cfm_key(k, down); return; }
    if (naming) { nm_key(k, down); return; }
    if (ui_simple && simple_key(k, down)) return;   // the simple view: its own keys (NOTES lets the steps' keys through)
    if (k == K_EDIT && down && !browsing && !slicing && !rolling && !theming && !iconing && !perform && tab == T_SEQ && srow >= 3 && is_synth(sel) && cur_par() != P_SAMPLE) gb_audition();
    if (k == K_EDIT && down && !browsing && !slicing && !rolling && !theming && !iconing && !perform && tab == T_SEQ && srow >= 1 && srow <= 2 && !cur_pat(sel)->mode) {
        Pattern *p = cur_pat(sel); int st = cur_step();
        if (st < p->len && !is_auto(lane) && !p->trig[st]) step_seed(p, st);   // it sounds as it will be put
        if (st < p->len && !is_auto(lane) && (!p->trig[st] || lane == L_ACC)) step_hear(st);   // the step sounds as B goes down (it is put on at release)
    }
    if (k == K_EDIT && down && !browsing && !slicing && !rolling && tab == T_SEQ && srow >= 1 && srow <= 2 && lane == L_TRIG && !cur_pat(sel)->mode) {
        Pattern *p = cur_pat(sel); int st = cur_step();
        paint_anchor = -1; edit_put = 0;   // 2026-11-19 (user): no drag-painting any more
        if (st < p->len && !p->trig[st]) { edit_begin(); p->trig[st] = 1; edit_put = 1; }   // the note is put as EDIT goes down; held, the arrows change it
        if (st < p->len) last_note[sel] = p->note[st] == NOTE_NONE ? 0 : p->note[st] + 1;
    }
    if (k == K_F1 || k == K_F2) {        // tabs on release, so that F1+F2 does not move first
        int other = k == K_F1 ? K_F2 : K_F1;
        if (down && held[other]) { used[K_F1] = used[K_F2] = 1; tab_project(); adv_hold = SDL_GetTicks() | 1; }   // a tap: PROJECT; held 3 s: the simple view
        else if (!down && !used[k]) { if (perform == 1 && held[K_SHIFT]) act_scene(k == K_F2 ? 1 : -1); else if (in_pad() && !perform) cat_step(k == K_F2 ? 1 : -1); else { ui_cat = -1; tab_step(k == K_F2 ? 1 : -1); } }
        return;
    }
    if (k == K_PLAY && down && held[K_MENU]) {   // once: rec, twice: capture
        if (combo_t) { combo_t = 0; capture(); }
        else { combo_t = SDL_GetTicks() | 1; combo_pos = engine_posf(); }
        return;
    }
    if (k == K_MENU) {
        if (down) browse_hold = 0;
        return;                          // v3: MENU alone does nothing; PADS and KEYS are on the quick bar
    }
    if (held[K_MENU] && k <= K_RIGHT) { if (down) { used[K_MENU] = 1; qb_focus = strip_focus = 0; if (!ui_over()) menu_nav(k); } return; }   // v3: the screen map
    if (strip_focus && !strips_here()) strip_focus = 0;
    if (qd_open && qd_key(k, down)) return;   // the quick drawer
    if (qb_focus && qbar_key(k, down)) return;
    if (strip_focus && strip_key(k, down)) return;
    if (tab_focus && !page_tabs()) tab_focus = 0;
    if (tab_focus && tabs_key(k, down)) return;
    if (perform && k == K_OPT && held[K_SHIFT]) { if (down) { used[K_SHIFT] = 1; perform = 0; toast("PLAY MODE OFF"); } return; }   // v3: the way out of PADS and KEYS
    if (perform) {                       // PADS: 12 slots; KEYS: selected slot on 12 scale degrees
        int i; for (i = 0; i < NSLOT && cell_key[i] != k; i++);
        if (k == K_PLAY && down) play_key();
        if (i >= NSLOT || !down) {
            if (i < NSLOT && perform == 2) key_on[i] = 0;
            if (i < NSLOT) { engine_release_key(perform == 2 ? 200 + i : 100 + i); rec_release(perform == 2 ? 200 + i : 100 + i); }   // a synth note ends with its key
            return;
        }
        if (perform == 2) {
            if (held[K_SHIFT] && (k == K_UP || k == K_DOWN)) { koct = clampi(koct + (k == K_UP ? 1 : -1), -3, 3); return; }
            if (held[K_SHIFT] && (k == K_LEFT || k == K_RIGHT || k == K_L1 || k == K_R1)) { pad_tonality(k); return; }
            int deg = i + koct * scale_len(track_scale(sel)), n = track_nslices(sel);
            if (n) { int sl = ((i + koct * 12) % n + n) % n; engine_trig(sel, 1.f, 0, sl); rec_hit(sel, sl, NOTE_NONE); }
            else if (is_synth(sel) || cur_pat(sel)->mode) { engine_trig_key(sel, 1.f, (float)hit_semis(sel, deg), -1, 200 + i); rec_tag_now = 200 + i; rec_hit(sel, -1, deg_midi(sel, deg)); rec_tag_now = 0; }   // ROLL: a sample holds too
            else { engine_trig(sel, 1.f, (float)hit_semis(sel, deg), -1); rec_tag_now = 200 + i; rec_hit(sel, -1, deg_midi(sel, deg)); rec_tag_now = 0; }   // a one-shot, but the take knows how long the key was held (chords)
            key_on[i] = 1; flash[sel] = SDL_GetTicks();
        } else { int clip = !rec && pat_used(cur_pat(i)); if (held[K_SHIFT]) clip = !clip;   // notes in its pattern: the button is the clip; none (or a take running): the sound; SHIFT: the other
                 if (clip) act_clip_toggle(i);
                 else { if (is_synth(i)) { engine_trig_key(i, 1.f, 0, -1, 100 + i); flash[i] = SDL_GetTicks(); rec_tag_now = 100 + i; } else act_listen(i); rec_hit(i, -1, NOTE_NONE); rec_tag_now = 0; }
                 act_select_pad(i); }
        return;
    }
    if (tab == T_PRJ && !theming && (k == K_L1 || k == K_R1)) { tempo_mul = down ? (k == K_L1 ? 0.94f : 1.06f) : 1.f; return; }
    if (browsing) {
        if (k == K_EDIT && !down && (fmenu || fconfirm)) { if (!used[K_EDIT]) fm_key(K_EDIT); return; }   // the menu acts on the release
        if (k == K_EDIT && down && (fmenu || fconfirm)) return;
        if (k == K_EDIT && !down) {      // a short press on a folder opens it, on release
            if (browse_hold) { browse_hold = 0; browse_key(K_EDIT); }
            return;
        }
        if (down) {
            used[k] = 1;                 // the release must not act again
            if (k == K_EDIT && bcur < bn && bent[bcur].dir && !bmode) { browse_hold = SDL_GetTicks() | 1; browse_rand = held[K_SHIFT]; return; }
            if (k == K_SHIFT) return;    // SHIFT may come and go while EDIT is held
            browse_hold = 0;             // anything else cancels the hold
            if (k == K_PLAY) { play_key(); return; }
            if (fm_key(k)) return;           // the file menu (X) and its question
            if (k == K_Y) { browse_close(); return; }   // Y leaves here as in the editors
            if (k == K_OPT && !bmode) { browse_preview(); return; }   // UI v2 (A11): OPTION listens again
            if (k == K_R2 && !bmode) { browse_listen_set(!browse_listen); toast(browse_listen ? "LISTEN ON" : "LISTEN OFF: OPTION HEARS ONE"); return; }
            browse_key(k);
        }
        return;
    }
    if (slicing) { slice_key(k, down); return; }
    if (randing) { rnd_key(k, down); return; }   // before ROLL: RANDOM opens over a roll pattern too
    if (prbing) { prb_key(k, down); return; }
    if (chording) { chord_key(k, down); return; }
    if (rolling) { roll_key(k, down); return; }
    if (picking) { pick_key(k, down); return; }
    if (wtbing) { wtb_key(k, down); return; }
    if (presetting) { preset_key(k, down); return; }
    if (theming) { theme_key(k, down); return; }
    if (iconing) { icon_key(k, down); return; }
    if (ducking) { duck_key(k, down); return; }
    if (jump_ok() && held[K_SHIFT] && k <= K_RIGHT) { if (down) { used[K_SHIFT] = 1; if (k == K_LEFT || k == K_RIGHT) pad_step(k == K_RIGHT ? 1 : -1); } return; }   // v3: SHIFT+<> the next pad
    if (scening) { scene_key(k, down); return; }
    if (!down) {                         // taps fire on release when no combo was used
        if (k == K_EDIT) { if (!used[K_EDIT]) edit_tap(); undo_armed = 0; }
        if (k == K_X && !used[K_X] && tab == T_SEQ && !held[K_SHIFT]) {
            Uint32 now = SDL_GetTicks();
            if (x_tap_t && ms_since(now, x_tap_t) < X_TWICE_MS) { x_tap_t = 0; clip_double(); }
            else x_tap_t = now | 1;
        }
        return;
    }
    int arrow = k <= K_RIGHT;
    if (arrow && (held[K_Y] || held[K_X] || held[K_OPT]) && tab == T_SEQ && srow >= 1 && srow <= 2 && is_auto(lane) && !browsing) {   // BEND the segment under the cursor (OPT+arrows since 2026-11-24)
        used[K_Y] = used[K_X] = used[K_OPT] = 1; int back = aut_point_back();
        if (back < 0) { toast("NO POINT: EDIT PUTS ONE"); return; }
        edit_begin(); int8_t *bc = aut_bend(); int d = k == K_RIGHT ? 10 : k == K_LEFT ? -10 : k == K_UP ? 50 : -50;
        act_bend(bc, back, d);
        char b[32]; snprintf(b, sizeof b, bc[back] >= 100 ? "BEND +100 HOLD" : bc[back] <= -100 ? "BEND -100 JUMP" : bc[back] ? "BEND %+d" : "BEND 0 STRAIGHT", bc[back]); toast(b); return;
    }
    if (arrow && held[K_OPT] && tab == T_SEQ && srow >= 3 && !browsing && !slicing && !rolling) {   // OPTION + arrows on the panel: the listening note
        used[K_OPT] = 1; int d = k == K_RIGHT ? 1 : k == K_LEFT ? -1 : k == K_UP ? 12 : -12;
        act_listen_note(sel, d);
        engine_release(); engine_trig_held(sel, 1.f, song.lnote[sel], -1); opt_gate = 1; flash[sel] = SDL_GetTicks();
        char b[40], nn[8]; listen_name(sel, nn); snprintf(b, sizeof b, "LISTEN %s", nn); toast(b); return;
    }
    if (held[K_SHIFT] && k == K_L2) { act_undo(); return; }
    if (k == K_R2 && !held[K_SHIFT] && tab == T_SEQ && srow >= 3 && !browsing && !slicing && !rolling) {   // MIDI LEARN on a SOUND card (2026-11-19)
        int pr = cur_par(), i = pr >= 0 ? mmap_find(sel, pr) : -1;
        if (mlearn_par >= 0) { mlearn_par = -1; toast("MIDI LEARN CANCELLED"); }
        else if (pr < 0 || !par_is_number(pr)) toast("NOT A KNOB: NO MIDI HERE");
        else if (i >= 0) { mmap_[i] = mmap_[--mmap_n]; mmap_save(); toast("MIDI KNOB UNLINKED"); }
        else if (midi_fd < 0) toast("NO MIDI PORT: PROJECT > MIDI");
        else { mlearn_pad = sel; mlearn_par = pr; toast("TURN A KNOB ON THE MIDI CONTROLLER"); }
        return; }
    if (k == K_R2 && !held[K_SHIFT] && tab == T_SEQ && srow >= 1 && srow <= 2 && !is_auto(lane)) { hear_on = !hear_on; toast(hear_on ? "HEAR NOTES ON" : "HEAR NOTES OFF"); return; }   // the headphones
    if (held[K_SHIFT] && k == K_OPT) { used[K_OPT] = 1; copy_paste(0); return; }
    if (held[K_SHIFT] && k == K_EDIT) { used[K_EDIT] = 1; copy_paste(1); return; }
    if (held[K_SHIFT] && k == K_X && tab == T_SEQ) { used[K_X] = 1; rnd_open(); return; }   // RANDOM
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) { used[K_EDIT] = used[K_OPT] = 1; delete_action(); return; }
    if (k == K_OPT) return;              // v3: OPTION alone opens the menu on release (key_event); LISTEN is an item there
    if (arrow && held[K_SHIFT] && tab == T_SEQ && srow >= 1 && srow <= 2 && !cur_pat(sel)->mode && (k == K_LEFT || k == K_RIGHT)) {   // a page at once
        spage = clampi(spage + (k == K_RIGHT ? 1 : -1), 0, last_page());
        return;
    }
    if (arrow && held[K_SHIFT] && tab == T_GRID) {   // stretch the selection
        if (gsel_a < 0 || gsel_b != sel) gsel_a = sel;
        move_cursor(k); gsel_b = sel; if (gsel_a == sel) gsel_a = -1;
        return;
    }
    if (arrow && held[K_EDIT]) { used[K_EDIT] = 1; edit_arrow(k); return; }
    if (held[K_X] && tab == T_SEQ && (k == K_LEFT || k == K_RIGHT)) { act_select_step(k == K_RIGHT ? 1 : -1); seq_clamp(); pat_sync(); return; }
    if (held[K_X] && tab == T_FX && (k == K_LEFT || k == K_RIGHT)) { used[K_X] = 1; act_select_step(k == K_RIGHT ? 1 : -1); fx_unit = sel; return; }   // another pad from any row
    if (arrow && held[K_OPT] && ((tab == T_GRID && ui_cat < 0 && !perform) || (v2_mixer() && mcol < NSLOT))) {   // 2026-11-24 (user, M8): the pad under the cursor
        used[K_OPT] = 1; int s = v2_mixer() ? mcol : sel;
        if (k == K_DOWN) { if (!gsel_mute()) { act_mute_toggle(s); toast(song.t[s].mute ? "MUTE ON" : "MUTE OFF"); } }
        else if (k == K_UP) { act_solo_toggle(s); toast(song.mix.pad[s].solo ? "SOLO ON" : "SOLO OFF"); }
        else if (k == K_RIGHT && has_sound(s)) { int o = sel; sel = s; sim_key(K_Y, K_SHIFT); sel = o; }   // FREEZE / LIVE, as SHIFT+Y did
        return;
    }
    if (arrow && held[K_OPT]) {          // OPTION+<> slot (patterns only on the pattern field since 2026-11-08: OPTION+^v does nothing)
        used[K_OPT] = 1;
        if (k == K_LEFT || k == K_RIGHT) {
            if (tab == T_SEQ && srow >= 1 && srow <= 2 && !cur_pat(sel)->mode) {   // walk the steps, hearing each like in the browser
                int st = cur_step(), nx = clampi(st + (k == K_RIGHT ? 1 : -1), 0, cur_pat(sel)->len - 1);
                if (nx != st) { set_step(nx); step_listen(nx); }
            } else if (tab != T_SEQ || srow < 0) act_select_step(k == K_RIGHT ? 1 : -1);   // SEQ: only on the name
        }
        return;
    }
    if (arrow) { if (tab == T_GRID) gsel_a = -1; move_cursor(k); return; }
    if (k == K_PLAY && tab == T_PAT) { pat_play(); return; }
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_Y && tab == T_SEQ && srow >= 3) {   // Y opens and closes the DEEP page (the sampler's too, 2026-11-11)
        cl_deep = song.t[sel].type == 1 ? 0 : !cl_deep; gb_cur = cl_deep ? 1 : 0; return; }   // WAVE: no DEEP page
    if (tab == T_PAT && k == K_Y) { pat_close(0); return; }   // the pattern window: Y goes back
    if (tab == T_GRID && k == K_Y && held[K_SHIFT]) { freeze_key(); return; }
    if (tab == T_PRJ && k == K_Y) { if (jcol) jcol = 0; else prj_leave(); return; }   // Y at the top: back where PROJECT was opened   // UI v2: Y is back, always
    if (v2_mixer() && mx_pg && !(mx_pg == 1 && mcol < NSLOT) && k == K_Y) { mx_pg = 0; mcol = sel; mrow = MX_VOL; return; }   // the drawer, FREEZE: back to the faders (on a strip Y mutes)
    if (tab == T_GRID && k == K_Y && gsel_mute()) return;
    if (tab == T_GRID && k == K_Y) { act_mute_toggle(sel); toast(song.t[sel].mute ? "MUTE ON" : "MUTE OFF"); return; }   // a live move: no undo step
    if (tab == T_GRID && k == K_X && held[K_SHIFT]) { naming_pad(); return; }
    if (tab == T_GRID && k == K_X) {     // 2026-11-14 (user): X starts or stops this pad's clip, as a pad does in PADS; no notes yet: its sound
        if (has_sound(sel)) { if (pat_used(cur_pat(sel))) act_clip_toggle(sel); else act_listen(sel); }
        return; }
    if (tab == T_MIX && k == K_Y && mcol < NSLOT && held[K_SHIFT]) { used[K_SHIFT] = 1; act_solo_toggle(mcol); return; }   // A21: SHIFT+Y solo
    if (v2_fx() && k == K_X) { act_insert_onoff(fx_unit, fx2_slot); return; }   // A17: the insert on or off (the tab dims)
    if (tab == T_MIX && k == K_Y && mcol < NSLOT) { act_mute_toggle(mcol); return; }   // live moves: no undo step
    if (tab == T_MIX && k == K_X && mcol < NSLOT) { act_solo_toggle(mcol); return; }
    if (v2_lfo() && (k == K_L1 || k == K_R1)) { lcol = (lcol + (k == K_R1 ? 1 : NLFO - 1)) % NLFO; return; }   // UI v2 (A18): the next LFO
    if (v2_auto() && (k == K_L1 || k == K_R1)) { lane = L_A1 + (lane - L_A1 + (k == K_R1 ? 1 : NAUTO - 1)) % NAUTO; return; }   // A19: the next lane
    if (v2_fx() && (k == K_L1 || k == K_R1)) { fx2_slot ^= 1; fx2_cur = -1; fx2_sync(); return; }   // UI v2 (A17): the other insert
    if (v2_unit() && (k == K_L1 || k == K_R1)) { mix_goto((mix_at() + (k == K_R1 ? 1 : MXP_N - 1)) % MXP_N); return; }   // the mixer's tab row
    if (tab == T_FX && (k == K_L1 || k == K_R1)) { fx_unit_step(k == K_R1 ? 1 : -1); return; }
    if (tab == T_FX && k == K_Y && fx_unit < NSLOT) { fx_page = !fx_page; return; }   // FX <> MOD
    if (v2_mix() && (k == K_L1 || k == K_R1)) return;   // MIX of a pad: one page
    if (v2_unit() && k == K_Y) { mix_goto(MXP_PADS); return; }   // 2026-11-24: back to the mixer's first tab, a level up
    if (v2_mixer() && (k == K_L1 || k == K_R1)) { mix_goto((mix_at() + (k == K_R1 ? 1 : MXP_N - 1)) % MXP_N); return; }   // 2026-11-24: PADS RETURN A RETURN B MASTER FREEZE
    if (tab == T_MIX && (k == K_L1 || k == K_R1)) { mcol = k == K_R1 ? (mcol < NSLOT ? (mcol / 4 + 1) * 4 : 0) : (mcol < 4 ? NSLOT : mcol <= NSLOT ? (mcol - 1) / 4 * 4 : NSLOT); if (mcol > NSLOT + 2) mcol = 0; if (mcol < NSLOT) sel = mcol; return; }
    if (ui_cat == PC_SOUND && tab == T_SEQ && (k == K_L1 || k == K_R1)) { sound_page_step(k == K_R1 ? 1 : -1); return; }   // SOUND: the page
    if (tab == T_SEQ && (k == K_L1 || k == K_R1) && held[K_SHIFT] && srow >= 1 && srow <= 2 && ui_cat != PC_SOUND) {   // the grid (2026-11-19, user: "a grid size in NOTE mode")
        used[K_SHIFT] = 1; act_pattern_div_step(sel, song.t[sel].cur, k == K_R1 ? 1 : -1);
        char b[32]; snprintf(b, sizeof b, "GRID %s", div_name[cur_pat(sel)->div % 6]); toast(b); return; }
    if (tab == T_SEQ && (k == K_L1 || k == K_R1)) {   // the pad's own lanes, round (PATTERN: without A1-A4, they are MOD's)
        int l[L_N], n = lane_list(l), i = 0; if (ui_cat == PC_PATTERN) n -= NAUTO; while (i < n && l[i] != lane) i++;
        lane = l[((i < n ? i : 0) + (k == K_R1 ? 1 : n - 1)) % n]; return; }
}
// v3: an arrow towards the quick bar that moved nothing on the page (its edge) puts the cursor on the bar. The cursors of
// every page are compared before and after; windows keep their keys.
static int ch_row, ch_col, ch_slot;      // chords.h
static int qb_edge(int k, int down) {
    if (!down || qb_focus || strip_focus || tab_focus || x_latch || v_latch || !(k == K_UP && (!ui_qbar || strips_here() || page_tabs()) || k == K_DOWN && ui_qbar) || perform || ui_over() || ui_simple || learning || exporting || asking || cfm || naming) return 0;
    for (int i = K_OPT; i < K_N; i++) if (held[i] && i != k) return 0;
    return 1;
}
static void cur_snap(int *a) {
    int v[] = {tab, sel, srow, scol, lane, gb_cur, mrow, mcol, mx_r, mx_pg, hub_cur, fx2_slot, fx_unit, fx_page,
               lcol, lrow, jcur, jcol, jsec, mix2_cur, dk_row, dk_col, dk_pt, sc_cur, scn_cur, ui_cat,
               scening, spage, sl_r, sl_c, ted_row, ted_col, pcur, ab_pg, cl_deep, rolling, rl_t, rl_n, rl_v0, rl_n0, sp_cur, sp_pg[0], sp_pg[1], sp_pg[2], sp_pg[3], fx2_cur, mx_fz, frow, fcol, ch_row, ch_col};
    memcpy(a, v, sizeof v); a[63] = (int)(sizeof v / sizeof *v);
}
static int side_key(int k, int down); static int side_page(void); static int side_enter(int k); static int side_land_mark(void); static void side_land(int before);   // side.h
static void key_event2(int k, int down);
static void step_audition(int st);
static void opt_listen(void) {            // OPTION tapped: the step under the cursor on STEPS, else the pad
    if (v2_steps()) { const Pattern *p = cur_pat(sel); int st = cur_step(); if (st < p->len && p->trig[st]) { step_audition(st); return; } }
    if (has_sound(sel)) listen(sel);
}
static int side_edge(int k, int down) {   // a plain arrow across on a page with slots: its edge may lead onto them
    if (!down || (k != K_LEFT && k != K_RIGHT) || !side_page() || x_latch || v_latch || qb_focus || qd_open || qpicking || strip_focus || tab_focus
        || ui_over() || perform || ui_simple || learning || cfm || asking || naming || exporting) return 0;   // a window or the drawer keeps its arrows
    for (int i = K_OPT; i < K_N; i++) if (held[i] && i != k) return 0;
    return 1;
}
static void key_event(int k, int down) {   // 2026-11-24: the side slots first, then the page; LISTEN STEP after
    if (side_key(k, down)) return;
    int lm = side_land_mark();
    if (side_edge(k, down)) {
        int a[64], b[64], c0[3] = {ch_row, ch_col, ch_slot}; cur_snap(a); key_event2(k, down); cur_snap(b);
        if (!memcmp(a, b, a[63] * sizeof *a) && c0[0] == ch_row && c0[1] == ch_col && c0[2] == ch_slot) side_enter(k);
    } else key_event2(k, down);
    side_land(lm);
}
static void key_event2(int k, int down) {
    if (k == K_OPT && !down && !used[K_OPT] && ctx_ok()) { key_event1(k, down); opt_listen(); return; }   // 2026-11-24 (user, M8): OPTION tapped alone listens; no menu
    if (!qb_edge(k, down)) { key_event1(k, down); return; }
    int a[64], b[64]; cur_snap(a); key_event1(k, down); cur_snap(b);
    if (memcmp(a, b, a[63] * sizeof *a)) return;
    if (k == K_UP && page_tabs()) tab_focus = 1;   // the page's top: its own tabs, then the strips
    else if (k == K_UP && strips_here()) { strip_focus = sec_on() ? 1 : 2; sec_cur = ui_cat >= 0 ? ui_cat : hub_cur; }
    else qb_focus = 1;
}

// ---------- screens ----------
static void line(int x0, int y0, int x1, int y1, int t, Col c);
static void step_matrix(const Pattern *p, int x, int y, int h, int play, int quiet) {   // the clip as a block of pixels, 16 in a row, against the bottom
    int rows = (p->len + 15) / 16, rh = h / (rows ? rows : 1); if (rh > 34) rh = 34; int b = rh > 2 ? rh - 2 : 1; y += h - rows * rh;
    static uint8_t lit[NSTEP]; int d = rl_d32[p->div % 6] * RT;
    for (int k = 0; k < p->len; k++) lit[k] = p->mode ? 0 : p->trig[k];
    if (p->mode) for (int i = 0; i < NNOTE; i++) if (p->roll[i].len) for (int k = p->roll[i].start / d; k <= (p->roll[i].start + p->roll[i].len - 1) / d && k < p->len; k++) lit[k] = 1;
    for (int k = 0; k < p->len; k++) { int c = k % 16;
        rect(x + c * 13 + (c / 4) * 2, y + (k / 16) * rh, 12, b, k == play ? C_PLAY : lit[k] ? (quiet ? C_GREY : C_ORANGE) : rows > 2 ? C_LINE : C_CELL, 1); }
}
static void icon_sc(int id, int x, int y, int sc, Col c) {
    static SDL_Rect d[160]; int n = 0;
    for (int r = 0; r < 16; r++) for (int a = 0; a < 16;) {
        if (icon_tab[id].px[r][a] != 'X') { a++; continue; }
        int e = a; while (e < 16 && icon_tab[id].px[r][e] == 'X') e++;
        if (n < 160) d[n++] = (SDL_Rect){x + a * sc, y + r * sc, (e - a) * sc, sc};
        a = e;
    }
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255); SDL_RenderFillRects(R, d, n);
}
static void icon(int id, int x, int y, Col c) { icon_sc(id, x, y, 2, c); }
// The icons' colours (2026-11-14): taken from the theme itself, so they fit any theme, a custom one too: nine of its colours,
// each as it is, lighter (toward the text) and deeper (toward the background). 0 = AUTO (the screen's own ink).
static Col icon_col(int c, Col dflt) {
    static const int8_t tok[9] = {TC_TEXT, TC_ACCENT, TC_ALERT, TC_PLAY, TC_SMP, TC_GRD, TC_CLD, TC_MIX, TC_DIM};
    if (c <= 0 || c >= ICOL_N) return dflt;
    Col t = th_cur[tok[(c - 1) / 3]]; int v = (c - 1) % 3;
    return v == 0 ? t : v == 1 ? mix(t, th_cur[TC_TEXT], 100) : mix(t, th_cur[TC_BG], 90);
}
static Col pad_icol(int s, Col dflt) { return icon_col(track_icol(&song.t[s]), dflt); }
static int slot_icon(int i) {            // the type comes from the path: asked only when the path changes
    static char seen[NSLOT][256]; static int cat[NSLOT]; const char *p = song.t[i].path;
    if (track_icon(&song.t[i])) return track_icon(&song.t[i]) - 1;   // the one picked with X (ICON), else by the sound
    if (song.t[i].type == 1) return IC_M_SYNTH;
    if (song.t[i].type == 2) return IC_M_BASS_BOX;
    if (song.t[i].type == 3) return IC_M_GRAINS;
    if (!p[0]) return IC_EMPTY;
    if (strncmp(seen[i], p, 255)) { snprintf(seen[i], 256, "%s", p); cat[i] = file_cat(p); }
    return cat[i];
}
static const char *side_hint(void);
static void draw_footer(const char *hint) {
    if (side_hint()) hint = side_hint();   // the cursor on a side slot: its keys
    snprintf(ui_hint, sizeof ui_hint, "%s", hint);
    ui2_footer(hint);
}
// HOME > PADS (UI v2, mockup A01): twelve cards, the engine's stripe, number, name, engine, the clip's bar; the pattern's
// route and FRZ / M / ! marks small (user, 2026-11-13). EDIT opens the pad, an empty one asks for an engine; single clips
// start in PADS (MENU).
static void draw_keys2(void);
static void draw_grid(void) {
    Uint32 now = SDL_GetTicks();
    if (perform == 2) { draw_keys2(); return; }
    enum { GX = 26, GY = 86, CW = 230, CH = 193, DX = 247, DY = 210 };
    static const char *const eng_tag[4] = {"SMP", "WAVE", "ACID", "CLOUD"};   // as the mockup writes them
    for (int i = 0; i < NSLOT; i++) {
        int x = GX + (i % 4) * DX, y = GY + (i / 4) * DY;
        Track *t = &song.t[i]; Uint32 age = now - flash[i]; int snd = has_sound(i);
        Col ec = C_ENG(t->type), ink = t->mute ? C_DIM : C_WHITE;
        if (!snd) {                      // empty: a dark place with a plus, or the file it misses
            if (i == sel) rframe(x - 4, y - 4, CW + 8, CH + 8, 11, 3, perform ? C_DIM : C_WHITE, C_BG);
            rframe(x, y, CW, CH, 8, 2, C_LINE, C_BG);
            if (slot_want[i][0]) { char nn[8]; snprintf(nn, sizeof nn, "%02d", i + 1); ftext(x + 25, y + 12, FT_M, C_DIM, nn); ftext(x + 25, y + 60, FT_MB, C_RED, "MISSING"); char b[64]; snprintf(b, sizeof b, "%s", file_base(slot_want[i])); fit(b, FT_S, CW - 40); ftext(x + 25, y + 92, FT_S, C_DIM, b); }
            else ftext(x + CW / 2 - fv2[FT_LB].w / 2, y + CH / 2 - fv2[FT_LB].h / 2, FT_LB, C_GREY, "+");
        } else {
            if (i == sel) rframe(x - 4, y - 4, CW + 8, CH + 8, 11, 3, perform ? C_DIM : C_WHITE, C_BG);   // the selected card: a white rounded outline
            Col bg = flash[i] && age < 180 ? mix(C_CELL, ec, 120 - (int)age * 120 / 180) : gsel_in(i) && i != sel ? C_SELBG : C_CELL;
            rrect(x, y, CW, CH, 8, bg);
            rect(x, y, 9, CH, t->mute ? mix(ec, C_BG, 150) : ec, 1);   // the engine's stripe
            char b[24]; snprintf(b, sizeof b, "%02d", i + 1); ftext(x + 25, y + 12, FT_M, C_DIM, b);
            { char nb[64]; snprintf(nb, sizeof nb, "%s", slot_name(i)); fit(nb, FT_XB, CW - 40); ftext(x + 25, y + 42, FT_XB, ink, nb); }
            ftext(x + 25, y + 94, FT_M, t->mute ? C_DIM : ec, eng_tag[t->type & 3]);
            icon(slot_icon(i), x + CW - 46, y + 86, t->mute ? C_DIM : pad_icol(i, ink));   // the pad's icon (X), by the engine's name
            { char r[24]; const Pattern *p0 = &t->pat[t->cur]; (void)p0;   // where the pattern goes, small
              int tg = t->queued >= 0 ? t->queued : -9; if (tg >= 0) snprintf(r, sizeof r, "P%d \177 P%d", t->cur + 1, tg + 1); else snprintf(r, sizeof r, "P%d", t->cur + 1);
              ftext(x + 25, y + 132, FT_S, t->on ? C_WHITE : C_DIM, r); }
            int fx = x + CW - 14;            // marks, top right
            if (slot_bad[i]) { fx -= 22; rrect(fx, y + 12, 22, 24, 4, C_RED); ftext(fx + 6, y + 13, FT_S, C_BG, "!"); fx -= 6; }
            if (t->mute) { fx -= 22; rrect(fx, y + 12, 22, 24, 4, C_RED); ftext(fx + 6, y + 13, FT_S, C_BG, "M"); fx -= 6; }
            if (frozen(i)) { fx -= 42; rrect(fx, y + 12, 42, 24, 4, frz_have[i] ? th_cur[TC_CLD] : C_LINE); ftext(fx + 6, y + 13, FT_S, C_BG, "FRZ"); }
            { int w = engine_playing() ? engine_want(i) : -1;   // the clip plays (blinks while it waits for the quantum)
              if ((t->on && w != 0) || (w >= 0 && (now / 250) % 2)) ftext(x + CW - 34, y + 128, FT_M, C_PLAY, "\1"); }
            rect(x + 25, y + 160, 180, 9, C_LINE, 1);   // the clip's progress
            const Pattern *p = cur_pat(i); int st = t->on ? engine_step(i) : -1;
            if (st >= 0 && p->len > 0) rect(x + 25, y + 160, 180 * (st % p->len + 1) / p->len, 9, t->mute ? C_DIM : ec, 1);
        }
    }
    if (gsel_on()) {                         // one frame round the whole selection
        int c0 = gsel_a % 4, c1 = sel % 4, r0 = gsel_a / 4, r1 = sel / 4, tt;
        if (c0 > c1) { tt = c0; c0 = c1; c1 = tt; } if (r0 > r1) { tt = r0; r0 = r1; r1 = tt; }
        frame(GX + c0 * DX - 6, GY + r0 * DY - 6, (c1 - c0) * DX + CW + 12, (r1 - r0) * DY + CH + 12, 2, C_ORANGE);
    }
    char missing[96]; snprintf(missing, sizeof missing, "%s  OPT+EDIT CLEAR", engine_sample(sel) ? "EDIT OPEN" : "EDIT LOAD");   // the card says MISSING and the file
    draw_footer(perform ? "PAD CLIP/HIT  SHIFT+OPT LEAVE  PLAY PAD CLIP  SHIFT+PAD OTHER  SHIFT+F1/F2 SCENE"
                        : gsel_on() ? "PLAY ALL SELECTED  OPT+v MUTE  ARROW ENDS IT"
                        : slot_want[sel][0] ? missing
                        : has_sound(sel) ? "EDIT OPEN  EDIT+PLAY CLIP  OPT+v MUTE  OPT+^ SOLO  OPT+> FREEZE"
                                             : "EDIT CHOOSE ENGINE  MENU PADS");
}
static void slice_label(char *b, const Pattern *p, int i, int ns) {   // "16+": the step also plays what follows slice 16
    int k = (p->slice[i] < 0 ? i : p->slice[i]) % (ns ? ns : NSTEP);
    snprintf(b, 8, "%d%s", k + 1, track_follows(sel, k + 1) ? "+" : "");
}

// ---------- drawing helpers (the 1.2 panels' parts: rings, lines) ----------
static int ring_cut = 1 << 20, ring_top = -(1 << 20);            // gb_ring draws rows down to this (a knob's ring stays open at the bottom, over its label)
static void gb_ring(int cx, int cy, int r, int t, Col c) {
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255);
    for (int y = -r; y <= r && y <= ring_cut; y++) { if (y < ring_top) continue;
        int wo = (int)sqrtf((float)(r * r - y * y)), ri = r - t, wi = abs(y) < ri ? (int)sqrtf((float)(ri * ri - y * y)) : -1;
        if (wi < 0) SDL_RenderDrawLine(R, cx - wo, cy + y, cx + wo, cy + y);
        else { SDL_RenderDrawLine(R, cx - wo, cy + y, cx - wi, cy + y); SDL_RenderDrawLine(R, cx + wi, cy + y, cx + wo, cy + y); }
    }
}
static void line(int x0, int y0, int x1, int y1, int t, Col c) {   // t pixels thick
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255);
    for (int a = -(t / 2); a <= (t - 1) / 2; a++) for (int b = -(t / 2); b <= (t - 1) / 2; b++) SDL_RenderDrawLine(R, x0 + a, y0 + b, x1 + a, y1 + b);
}



#include "hints.h"
// ---------- piano roll drawing ----------
static void roll_name(char *b, int n) { snprintf(b, 8, "%s%d", note_name[n % 12], n / 12 - 1); }
static void draw_roll_mini(const Pattern *p, int x, int y, int w, int h, int st, int on) {   // the clip's notes in the place of the steps
    int T = roll_ticks(p), lo = 127, hi = 0, beat = 8 * RT, bar = beat * (song.beats > 0 ? song.beats : 4);
    for (int i = 0; i < NNOTE; i++) if (p->roll[i].len) { if (p->roll[i].note < lo) lo = p->roll[i].note; if (p->roll[i].note > hi) hi = p->roll[i].note; }
    if (lo > hi) { lo = roll_base(sel) - 6; hi = lo + 11; }
    while (hi - lo < 11) { if (lo > 0) lo--; if (hi - lo < 11 && hi < 127) hi++; }
    int rows = hi - lo + 1, rh = (h - 4) / rows; if (rh < 1) rh = 1;
    rect(x, y, w, h, C_CELL, 0);
    for (int t = 0; t < T; t += beat) rect(x + (int)((int64_t)t * w / T), y, 1, h, t % bar ? C_CELL : C_LINE, 1);
    for (int i = 0; i < NNOTE; i++) { const Note *n = &p->roll[i]; if (!n->len || n->start >= T) continue;
        int nx = x + (int)((int64_t)n->start * w / T), nw = (int)((int64_t)n->len * w / T); if (nw < 3) nw = 3; if (nx + nw > x + w) nw = x + w - nx;
        int ny = y + 2 + (hi - n->note) * rh;
        rect(nx, ny, nw, rh > 2 ? rh - 1 : 1, mix(C_DIM, C_ORANGE, 120 + n->vel), 1); }
    if (!roll_any(p)) text(x + w / 2 - 160, y + h / 2 - 8, 2, C_GREY, "EMPTY: EDIT TO WRITE");
    if (st >= 0) rect(x + (int)((int64_t)st * rl_d32[p->div % 6] * RT * w / T), y, 2, h, C_WHITE, 1);
    if (on) { frame(x - 1, y - 1, w + 2, h + 2, 3, C_ORANGE); }
}
// PATTERN > ROLL (UI v2, mockup A06): piano keys on the left, fourteen rows, the notes in the engine's colour.
static void draw_rhythm(void) {          // RHYTHM (2026-11-24, user's mockup): the clip's notes on one line, the height their velocity
    Pattern *p = cur_pat(sel); int T = roll_ticks(p), st = song.t[sel].on ? engine_step(sel) : -1; Col ec = C_ENG(song.t[sel].type); char b[80];
    int beat = 8 * RT, bar = beat * (song.beats > 0 ? song.beats : 4), cols = rl_cols(), w = cols * rl_snap;
    roll_view(); if (follow_ph && st >= 0) { int pt = st * rl_d32[p->div % 6] * RT; rl_v0 = pt / w * w; }
    rhythm_sync(p); int cur = roll_find(p, rl_t, rl_n);
    const int X0 = wx0, X1 = wx0 + ww, Y0 = 118, Y1 = 520, IX = X0 + 10, IW = X1 - X0 - 20, TY = Y0 + 44, BY = Y1 - 12;
    rrect(X0, Y0, X1 - X0, Y1 - Y0, 8, C_CELL);
    for (int c = 0; c <= cols; c++) { int t = rl_v0 + c * rl_snap, x = IX + (int)((int64_t)(t - rl_v0) * IW / w);   // the grid, bars bright
        if (t > T) break;
        if (t % bar == 0) { rect(x, TY - 6, 2, BY - TY + 6, C_GREY, 1); snprintf(b, sizeof b, "%d", t / bar + 1); if (x + 4 + ftext_w(FT_MB, b) <= X1 - 4) ftext(x + 4, Y0 + 8, FT_MB, C_WHITE, b); }   // the clip's end line: no number past the card
        else if (t % beat == 0) { rect(x, TY, 1, BY - TY, C_LINE, 1); snprintf(b, sizeof b, "%d.%d", t / bar + 1, t % bar / beat + 1); if (x + 3 + ftext_w(FT_S, b) <= X1 - 4) ftext(x + 3, Y0 + 10, FT_S, C_DIM, b); }
        else rect(x, TY + (BY - TY) / 2, 1, (BY - TY) / 2, mix(C_LINE, C_CELL, 120), 1); }
    if (T < rl_v0 + w) { int ex = IX + (int)((int64_t)(T - rl_v0) * IW / w); rect(ex, TY - 6, IX + IW - ex, BY - TY + 6, C_BG, 1); }   // past the clip's end
    int fx0 = -1, fx1 = 0, ftop = 0; Col fc = C_CELL;   // the cursor's note: its frame over its neighbours, drawn last (2026-11-25 QA)
    for (int j = 0; j < NNOTE; j++) { const Note *n = &p->roll[j]; if (!n->len) continue;
        int a = n->start - rl_v0, e = a + n->len; if (e <= 0 || a >= w) continue;
        int x0 = IX + (int)((int64_t)(a < 0 ? 0 : a) * IW / w), x1 = IX + (int)((int64_t)(e > w ? w : e) * IW / w) - 2; if (x1 - x0 < 6) x1 = x0 + 6;
        int top = BY - (BY - TY) * n->vel / 127; Col c = n->start >= T ? C_GREY : mix(mix(ec, C_BG, 110), ec, n->vel * 2);
        rrect(x0, top, x1 - x0, BY - top, 4, c);
        if (j == cur) { fx0 = x0; fx1 = x1; ftop = top; fc = c; }
        if (n->start % rl_snap) ftext(x0, top - 22, FT_S, C_ORANGE, "");   // off the grid
    }
    if (fx0 >= 0) rframe(fx0 - 3, ftop - 3, fx1 - fx0 + 6, BY - ftop + 6, 6, 3, C_WHITE, fc);
    { int cx = IX + (int)((int64_t)(rl_t - rl_v0) * IW / w); rect(cx, TY - 8, 2, BY - TY + 8, C_WHITE, 1); }
    if (st >= 0) { int t = st * rl_d32[p->div % 6] * RT - rl_v0; if (t >= 0 && t < w) rect(IX + (int)((int64_t)t * IW / w), TY, 2, BY - TY, C_PLAY, 1); }
    // the whole clip, small: the view's frame on it
    rrect(X0, 536, X1 - X0, 64, 8, C_CELL);
    for (int j = 0; j < NNOTE; j++) { const Note *n = &p->roll[j]; if (!n->len || n->start >= T) continue;
        int x = IX + (int)((int64_t)n->start * IW / T); rect(x, 592 - 44 * n->vel / 127, 3, 44 * n->vel / 127, ec, 1); }
    { int vx0 = IX + (int)((int64_t)rl_v0 * IW / T), vx1 = IX + (int)((int64_t)(rl_v0 + w < T ? rl_v0 + w : T) * IW / T); rframe(vx0, 540, vx1 - vx0, 56, 4, 2, C_WHITE, C_CELL);
      for (int j = 0; j < NNOTE; j++) { const Note *n = &p->roll[j]; if (!n->len || n->start >= T) continue;   // the notes again, over the frame's inside
        int x = IX + (int)((int64_t)n->start * IW / T); rect(x, 592 - 44 * n->vel / 127, 3, 44 * n->vel / 127, ec, 1); } }
    snprintf(b, sizeof b, "%s 1/%d%s \3 bar %d.%d", rl_gtype == RG_FREE ? "FREE" : "grid", 384 / rl_snaps[rl_gi], rl_gtype == RG_TRIPLET ? "T" : rl_gtype == RG_DOTTED ? "D" : "", rl_t / bar + 1, rl_t % bar / beat + 1);
    ftext(wx0, 612, FT_M, C_GREY, b);
    if (cur >= 0) { const Note *n = &p->roll[cur]; int spn = rl_d32[p->div % 6] * RT; snprintf(b, sizeof b, "len %g st \3 vel %d \3 chc %d", (double)((int)(n->len * 10.f / spn + 0.5f)) / 10, n->vel, n->prob); }
    else snprintf(b, sizeof b, "EDIT puts a note");
    ftext(wx0 + ww - ftext_w(FT_M, b), 612, FT_M, C_WHITE, b);
    draw_footer(cur >= 0 ? "EDIT DELETE  EDIT+^v VELOCITY  EDIT+<> LENGTH" : "EDIT NOTE  OPT+EDIT+<> MOVE  SHIFT+<> PAGE");
}
static void draw_roll(void) {
    if (rl_rhythm) { draw_rhythm(); return; }
    Pattern *p = cur_pat(sel); int T = roll_ticks(p), st = song.t[sel].on ? engine_step(sel) : -1;
    enum { KW = 64, GY = 76, RH = 44 }; const int KX = wx0, GX = wx0 + 70, CW = (ww - 70) / rl_cols();   // the work area (side slots)
    int beat = 8 * RT, bar = beat * (song.beats > 0 ? song.beats : 4), i = roll_find(p, rl_t, rl_n);
    Col ec = C_ENG(song.t[sel].type), key_w = mix(C_WHITE, C_BG, 30), key_b = C_LINE;
    roll_view();
    if (follow_ph && st >= 0) { int w = rl_cols() * rl_snap, pt = st * rl_d32[p->div % 6] * RT; rl_v0 = pt / w * w; }   // FOLLOW: the view shows the playhead
    char b[80], nm[8];
    for (int r = 0; r < RL_ROWS; r++) {  // rows, high pitches on top: white and black keys; the pad's scale has a dot, its tonic the accent
        int n = rl_n0 + RL_ROWS - 1 - r, y = GY + r * RH, pc = n % 12, black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        int ton = ((n - track_key(sel)) % 12 + 12) % 12 == 0;
        rect(GX, y, rl_cols() * CW, RH - 1, black ? mix(C_BG, C_CELL, 110) : C_CELL, 1);
        rect(KX, y, KW, RH - 1, black ? key_b : key_w, 1);
        if (pc == 0 || n == rl_n) { roll_name(nm, n); ftext(KX + 6, y + 2, FT_S, black ? C_WHITE : C_BG, nm); }   // readable on either key
        if (n == rl_n) rect(KX + KW - 4, y, 4, RH - 1, ec, 1);   // the cursor's row: a bar on its key
        if (roll_in_scale(n)) rect(KX + KW - 10, y + RH / 2 - 3, 6, 6, ton ? C_ORANGE : black ? C_DIM : C_GREY, 1);
    }
    for (int c = 0; c <= rl_cols(); c++) { int t = rl_v0 + c * rl_snap;   // lines: beats faint, bars clear
        if (t % bar == 0) rect(GX + c * CW, GY, 2, RL_ROWS * RH, C_GREY, 1);
        else if (t % beat == 0) rect(GX + c * CW, GY, 1, RL_ROWS * RH, C_LINE, 1); }
    int end = T - rl_v0; if (end < rl_cols() * rl_snap) { int ex = GX + (end > 0 ? end * CW / rl_snap : 0); rect(ex, GY, GX + rl_cols() * CW - ex, RL_ROWS * RH, C_BG, 1); }   // past the end of the pattern
    for (int j = 0; j < NNOTE; j++) { const Note *n = &p->roll[j];
        if (!n->len || n->note < rl_n0 || n->note >= rl_n0 + RL_ROWS) continue;
        int a = n->start - rl_v0, e = a + n->len; if (e <= 0 || a >= rl_cols() * rl_snap) continue;
        int x0 = GX + (a < 0 ? 0 : a * CW / rl_snap), x1 = GX + (e > rl_cols() * rl_snap ? rl_cols() * rl_snap : e) * CW / rl_snap;
        int y = GY + (rl_n0 + RL_ROWS - 1 - n->note) * RH;
        if (x1 - x0 < 8) x1 = x0 + 8;
        Col c = n->start >= T ? C_GREY : mix(mix(ec, C_BG, 110), ec, n->vel * 2);
        if (j == i) rframe(x0, y + 2, x1 - x0, RH - 5, 7, 3, C_WHITE, c); else rrect(x0 + 2, y + 4, x1 - x0 - 3, RH - 9, 5, c);
        if (n->flags & NF_ACC) { if (x1 - x0 >= 18) ftext(x0 + 5, y + 10, FT_S, C_BG, "A"); else rect(x0 + 2, y + 4, x1 - x0 - 3, 4, C_WHITE, 1); }   // a short note: a stripe
    }
    if (st >= 0) { int t = st * rl_d32[p->div % 6] * RT - rl_v0; if (t >= 0 && t < rl_cols() * rl_snap) rect(GX + t * CW / rl_snap, GY, 2, RL_ROWS * RH, C_WHITE, 1); }
    if (i < 0) { int cx = GX + (rl_t - rl_v0) * CW / rl_snap, cy = GY + (rl_n0 + RL_ROWS - 1 - rl_n) * RH;   // the cursor on an empty place
        rect(cx, cy + 2, CW, 2, C_WHITE, 1); rect(cx, cy + RH - 5, CW, 2, C_WHITE, 1); rect(cx, cy + 2, 2, RH - 5, C_WHITE, 1); rect(cx + CW - 2, cy + 2, 2, RH - 5, C_WHITE, 1); }
    else if (rl_t - rl_v0 >= 0 && rl_t - rl_v0 < rl_cols() * rl_snap) { int cx = GX + (rl_t - rl_v0) * CW / rl_snap, cy = GY + (rl_n0 + RL_ROWS - 1 - rl_n) * RH;   // on a note: its cell
        rect(cx + 3, cy + RH - 11, CW - 6, 3, C_WHITE, 1); }   // 2026-11-25 QA: <> inside a long note showed nothing
    snprintf(b, sizeof b, "grid 1/%d%s \3 bar %d.%d", 384 / rl_snaps[rl_gi], rl_gtype == RG_TRIPLET ? "T" : rl_gtype == RG_DOTTED ? "D" : rl_gtype == RG_FREE ? " FREE" : "", rl_t / bar + 1, rl_t % bar / beat + 1); ftext(wx0, 694, FT_S, C_GREY, b);
    roll_name(nm, rl_n);
    if (i >= 0) { const Note *n = &p->roll[i]; int sp = rl_d32[p->div % 6] * RT;   // the length in the clip's steps
        snprintf(b, sizeof b, "%s \3 len %g st \3 vel %d \3 chc %d%s", nm, (double)((int)(n->len * 10.f / sp + 0.5f)) / 10, n->vel, n->prob, n->flags & NF_ACC ? " \3 acc" : ""); }
    else snprintf(b, sizeof b, "%s", nm);
    ftext(wx0 + ww - ftext_w(FT_S, b), 694, FT_S, C_WHITE, b);
    if (ui_qbar) draw_phones(850, 16);   // v3: MON on the quick bar at the top
    draw_footer(i >= 0 ? "EDIT DELETE  EDIT+ARROWS LENGTH/PITCH  OPT+<> NEXT" : "EDIT NOTE  OPT+<> NEXT  SHIFT+ARROWS PAGE");
}
#include "chords.h"
#include "prb.h"
#include "side.h"
static void slice_card(int x, int y, int w, const char *lb, const char *v, Col ec, int on) {   // UI v2: a name and a value, as on the SOUND cards
    if (on) rframe(x, y, w, 96, 8, 4, ec, C_CELL); else rrect(x, y, w, 96, 8, C_CELL);
    ftext(x + 20, y + 12, FT_M, C_DIM, lb); ftext(x + w - 20 - ftext_w(FT_XB, v), y + 10, FT_XB, C_WHITE, v);
}
static void draw_slice(void) {
    const Sample *s = engine_sample(sel);
    if (!s || s->n < 2) { slicing = 0; return; }
    static float pk[WAVE_W][2]; static const Sample *pk_s; static int pk_a = -1, pk_sp;
    const Track *t = &song.t[sel];
    int cnt = ed_cnt(); ed_m = clampi(ed_m, 0, cnt);
    int span = ed_span(s), mpos = ed_pos(s, ed_m);
    if (mpos < ed_view || mpos >= ed_view + span) ed_view = mpos - span / 2;   // keep the marker on screen
    ed_view = clampi(ed_view, 0, s->n - span);
    if (pk_s != s || pk_a != ed_view || pk_sp != span) {   // peaks only when the view changes
        pk_s = s; pk_a = ed_view; pk_sp = span;
        for (int x = 0; x < WAVE_W; x++) {
            int i0 = ed_view + (int)((int64_t)x * span / WAVE_W), i1 = ed_view + (int)((int64_t)(x + 1) * span / WAVE_W);
            if (i1 <= i0) i1 = i0 + 1;
            int st = (i1 - i0) / 48; if (st < 1) st = 1;
            float lo = 0, hi = 0;
            for (int i = i0; i < i1 && i < s->n; i += st) {
                float l = s->d[2 * i], r = s->d[2 * i + 1];
                if (l < lo) lo = l; if (l > hi) hi = l; if (r < lo) lo = r; if (r > hi) hi = r;
            }
            pk[x][0] = lo; pk[x][1] = hi;
        }
    }
    int a = ed_start(s), b = ed_end(s), k = clampi(ed_m, 0, cnt - 1), ka = ed_pos(s, k), kb = ed_pos(s, k + 1);
    enum { WY = 118, WH = 396, CY = WY + WH / 2 };
    {                                    // UI v2 (after A09)                         // UI v2 (after A09): the wave on a panel, the points as lines with numbers, cards below
        Col ec = C_ENG(song.t[sel].type), yel = C_ORANGE; enum { PX = 26, PY = 80, PW = 971, PH = 400, IX = PX + 12, IW = PW - 24, MY = PY + PH / 2 };
        rrect(PX, PY, PW, PH, 10, C_CELL);
        for (int x = 0; x < IW; x += 3) { int px = x * WAVE_W / IW, f = ed_view + (int)((int64_t)px * span / WAVE_W);
            int y0 = MY - (int)(clampf1(pk[px][1]) * (PH / 2 - 16)), y1 = MY - (int)(clampf1(pk[px][0]) * (PH / 2 - 16));
            rect(IX + x, y0, 2, y1 - y0 + 1, f < a || f > b ? mix(ec, C_CELL, 190) : f >= ka && f < kb ? ec : mix(ec, C_CELL, 110), 1); }
        for (int m = 0; m <= cnt; m++) {
            int p = ed_pos(s, m); if (p < ed_view || p > ed_view + span) continue;
            int x = IX + (int)((int64_t)(p - ed_view) * (IW - 1) / span), on = m == ed_m, edge = m == 0 || m == cnt;
            rect(x - (on ? 1 : 0), PY + 8, on ? 3 : 2, PH - 16, on ? yel : edge ? C_WHITE : mix(C_WHITE, C_CELL, 60), 1);
            char nb[8]; snprintf(nb, sizeof nb, "%s", m == 0 ? "S" : m == cnt ? "E" : ""); if (!nb[0]) snprintf(nb, sizeof nb, "%d", m + 1);
            if (m < cnt || m == cnt) ftext(m == cnt ? x - 8 - ftext_w(FT_S, nb) : x + 8, PY + 10, FT_S, on ? yel : C_DIM, nb);
        }
        if (span < s->n) { rect(PX, PY + PH + 8, PW, 6, C_CELL, 1); rect(PX + (int)((int64_t)ed_view * PW / s->n), PY + PH + 8, (int)((int64_t)span * PW / s->n) + 2, 6, C_DIM, 1); }
        char v1[32], v2[32], v3[32];
        par_str(sel, P_SLICES, v1, sizeof v1);
        if (ed_m == 0) snprintf(v2, sizeof v2, "START %d.%d%%", t->start / 10, t->start % 10);
        else if (ed_m == cnt) snprintf(v2, sizeof v2, "END %d.%d%%", t->end / 10, t->end % 10);
        else snprintf(v2, sizeof v2, "%d  %.3f S", ed_m + 1, mpos / (double)SR);
        snprintf(v3, sizeof v3, "%d  %d MS", k + 1, (kb - ka) / (SR / 1000));
        slice_card(32, 500, 473, "SLICES", v1, ec, 0); slice_card(518, 500, 473, "POINT", v2, ec, 1);
        slice_card(32, 608, 473, "SLICE", v3, ec, 0);
        snprintf(v1, sizeof v1, "X%d", span < s->n ? s->n / span : 1); slice_card(518, 608, 473, "ZOOM", v1, ec, 0);
        draw_footer("EDIT SPLIT  EDIT+<> MOVE  Y BACK  <> POINT  ^v ZOOM  OPT LISTEN  OPT+EDIT DELETE");
        return;
    }
}
// PATTERN > SELECT (UI v2, mockup A07): the sixteen patterns, the chosen one's LENGTH DIVISION MODE (user, 2026-11-13) and
// what plays after it: REPEAT xN, THEN (how often: CHANCE), ELSE.
static void sel_card(int x, int y, int w, int h, const char *label, const char *val, int bar, int on) {   // bar 0..100, -1 none
    Col cc = C_CAT(PC_PATTERN);
    if (on) rframe(x, y, w, h, 8, 4, cc, C_CELL); else rrect(x, y, w, h, 8, C_CELL);
    ftext(x + 20, y + 12, FT_S, C_DIM, label);
    ftext(x + w - 20 - ftext_w(FT_XB, val), y + 6, FT_XB, C_WHITE, val);
    if (bar >= 0) { rect(x + 20, y + h - 24, w - 40, 8, C_LINE, 1); rect(x + 20, y + h - 24, (w - 40) * clampi(bar, 0, 100) / 100, 8, cc, 1); }
}
static void draw_pat(void) {
    const Track *t = &song.t[sel]; int pt = fol_pat(); const Pattern *p = &t->pat[pt]; const uint8_t *fo = song.fol[sel][pt];
    Col cc = C_CAT(PC_PATTERN); char b[48], v[24];
    for (int i = 0; i < NPAT; i++) {
        int x = 29 + (i % 8) * 243 / 2, y = 80 + (i / 8) * 84, used = pat_used(&t->pat[i]) || roll_any(&t->pat[i]);
        if (sl_r < 2 && i == pcur) rframe(x - 4, y - 4, 119, 78, 11, 3, C_WHITE, C_BG);
        rrect(x, y, 111, 70, 8, used ? mix(C_BG, cc, 50) : C_CELL);
        if (i == pt) { rframe(x, y, 111, 70, 8, 2, cc, used ? mix(C_BG, cc, 50) : C_CELL); }   // the chosen one: the cards below are about it
        snprintf(b, sizeof b, "P%02d", i + 1); ftext(x + 55 - ftext_w(FT_MB, b) / 2, y + 23, FT_MB, used ? cc : C_DIM, b);
        if (i == t->cur && t->on) ftext(x + 8, y + 4, FT_S, cc, "\1");   // it plays
        else if (i == t->queued && SDL_GetTicks() / 250 % 2) ftext(x + 8, y + 4, FT_S, C_WHITE, "\1");   // it waits for the quantum
    }
    snprintf(b, sizeof b, "P%02d", pt + 1); ftext(26, 254, FT_S, C_DIM, b);
    snprintf(v, sizeof v, "%d", p->len); sel_card(32, 290, 310, 120, "LENGTH", v, p->len * 100 / NSTEP, sl_r == 2 && sl_c == 0);
    sel_card(357, 290, 310, 120, "DIVISION", div_name[p->div % 6], p->div * 20, sl_r == 2 && sl_c == 1);
    sel_card(682, 290, 310, 120, "MODE", p->mode == 2 ? "CHORDS" : p->mode ? "ROLL" : "STEPS", -1, sl_r == 2 && sl_c == 2);
    snprintf(b, sizeof b, "AFTER P%02d", pt + 1); ftext(26, 428, FT_S, C_DIM, b);
    int rp = fo[FO_AFTER] ? fo[FO_AFTER] : 1, loop = fo[FO_A] == FA_NONE && (fo[FO_B] == FA_NONE || !fo[FO_BCH]);
    snprintf(v, sizeof v, "x%d", rp); sel_card(32, 462, 474, 116, "REPEAT", loop ? "ALWAYS" : v, loop ? -1 : (rp > 16 ? 16 : rp) * 100 / 16, sl_r == 3 && sl_c == 0);
    if (fo[FO_A] == FA_JUMP) snprintf(v, sizeof v, "P%02d", fo[FO_JUMP] % NPAT + 1); else snprintf(v, sizeof v, "%s", fa_name[fo[FO_A] % FA_N]);
    sel_card(522, 462, 474, 116, "THEN", v, -1, sl_r == 3 && sl_c == 1);
    snprintf(v, sizeof v, "%d%%", 100 - fo[FO_BCH]); sel_card(32, 590, 474, 116, "CHANCE", v, 100 - fo[FO_BCH], sl_r == 4 && sl_c == 0);
    if (fo[FO_B] == FA_JUMP) snprintf(v, sizeof v, "P%02d", fo[FO_JUMP] % NPAT + 1); else snprintf(v, sizeof v, "%s", fa_name[fo[FO_B] % FA_N]);
    sel_card(522, 590, 474, 116, "ELSE", fo[FO_BCH] ? v : "-", -1, sl_r == 4 && sl_c == 1);
    draw_footer(sl_r < 2 ? "EDIT CHOOSE  OPT+EDIT CLEAR  Y BACK  PLAY PLAY IT  SHIFT+OPT COPY  SHIFT+EDIT PASTE"
              : sl_r == 2 ? "EDIT+ARROWS VALUE  Y BACK" : "EDIT+ARROWS VALUE  OPT+EDIT LOOP  Y BACK  EDIT+UP/DOWN JUMP TARGET");
}
static void prj_value(int i, char *b) {
    switch (i) {
    case J_TEMPO: snprintf(b, 32, "%d BPM", song.tempo); break;
    case J_TRIM: snprintf(b, 32, "%+d DB", trim_db); break;
    case J_SWING: snprintf(b, 32, "%d%%", song.swing); break;
    case J_LAUNCH: { static const char *const ln[5] = {"1 BAR", "1 BAR", "2 BARS", "4 BARS", "NONE"}; snprintf(b, 32, "%s", ln[song.launch % 5]); break; }
    case J_KEY: snprintf(b, 32, "%s", note_name[song.key]); break;
    case J_SCALE: snprintf(b, 32, "%s", scale_name[song.scale]); break;
    case J_METRO: snprintf(b, 32, song.metro ? "ON" : "OFF"); break;
    case J_SNAP: snprintf(b, 32, song.snap ? "ON" : "OFF"); break;
    case J_HINT: snprintf(b, 32, hint_on ? "ON" : "OFF"); break;
    case J_STRIP: snprintf(b, 32, "%s", ui_strip == 2 ? "BIG" : ui_strip ? "THIN" : "HIDDEN"); break;
    case J_QBAR: snprintf(b, 32, ui_qbar ? "BOTTOM" : "TOP"); break;
    case J_QBTN: qb_ready(); snprintf(b, 32, "%d OF 12", qb_n); break;
    case J_SECS: snprintf(b, 32, ui_secs ? "ON" : "HIDDEN"); break;
    case J_VIEW: snprintf(b, 32, ui_simple ? "SIMPLE" : "ADVANCED"); break;
    case J_LOCK: snprintf(b, 32, ui_lock ? "ON" : "OFF"); break;
    case J_NAV: snprintf(b, 32, ui_tree ? "TREE" : "LIST"); break;
    case J_DRAWER: snprintf(b, 32, ui_drawer_left ? "LEFT" : "RIGHT"); break;
    case J_MXFX: snprintf(b, 32, ui_mxfx ? "ON" : "OFF"); break;
    case J_MXLINK: snprintf(b, 32, ui_mxlink ? "SHARED" : "OWN"); break;
    case J_MPORT: if (!midi_want[0]) snprintf(b, 32, "OFF"); else snprintf(b, 32, "%.20s%s", midi_want, midi_fd >= 0 && midi_run ? "" : " (NOT HERE)"); break;
    case J_MCLK: snprintf(b, 32, midi_clk_out ? "ON" : "OFF"); break;
    case J_MCLKIN: if (midi_clk_in && mclk_per_ns) snprintf(b, 32, "ON  %.1f BPM", 60e9 / (mclk_per_ns * 24.0)); else snprintf(b, 32, midi_clk_in ? "ON" : "OFF"); break;
    case J_MNOTE: snprintf(b, 32, midi_notes ? "PADS" : "KEYS"); break;
    case J_MCH: if (midi_ch) snprintf(b, 32, "%d", midi_ch); else snprintf(b, 32, "ALL"); break;
    case J_ABOUT: b[0] = 0; break;
    case J_MSND: snprintf(b, 32, metro.snd ? "ON" : "OFF"); break; case J_MVIB: snprintf(b, 32, metro.vib ? "ON" : "OFF"); break;
    case J_MVOL: snprintf(b, 32, "%d", metro.vol); break; case J_MTONE: snprintf(b, 32, "%s", metro_tone_name[metro.tone % 3]); break;
    case J_MWHEN: snprintf(b, 32, metro.when ? "RECORDING" : "ALWAYS"); break; case J_MDIV: snprintf(b, 32, "%s", metro_div_name[metro.div % 3]); break;
    case J_MVOFF: snprintf(b, 32, "%+d MS", vib_off); break;
    case J_PROJ: snprintf(b, 32, "%.20s%s", prj_cur[0] ? prj_cur : "UNTITLED", prj_dirty() ? " *" : ""); break;
    case J_SAVE: snprintf(b, 32, prj_dirty() ? "UNSAVED CHANGES" : prj_cur[0] ? "SAVED" : "NOT SAVED YET"); break;
    case J_SAVEAS: b[0] = 0; break;
    case J_EXPORT: b[0] = 0; break;
    case J_NEW: b[0] = 0; break;
    case J_THEME: snprintf(b, 32, "%s", theme_name(th_sel)); break;
    case J_LIB: if (lib_state == 1) snprintf(b, 32, "%d, %d BAD", lib_seen, lib_badn);
        else if (lib_state == 2) snprintf(b, 32, "DONE %d, %d BAD", lib_seen, lib_badn);
        else b[0] = 0; break;
    case J_BEATS: snprintf(b, 32, "%d/4", song.beats); break;
    case J_COUNT: if (song.count_in) snprintf(b, 32, "%d BEATS", song.count_in); else snprintf(b, 32, "OFF"); break;
    case J_QUANT: snprintf(b, 32, "%s", quant_name[song.rec_quant % 5]); break;
    default: snprintf(b, 32, "%d", song.master); break;
    }
}
static void prj_info_draw(const char *path);
static double draw_build_ms, draw_present_ms;   // summed over the frames since the last log line
static Uint32 last_input;                // the last button or d-pad event
static int frame_rate(Uint32 now) {      // frames per second for what is happening now
    int keys = 0; for (int k = 0; k < K_N; k++) keys |= held[k];
    if (keys || (last_input && ms_since(now, last_input) < 500) || browse_hold || quit_down || (anim_busy && ms_since(now, anim_busy) < 100)) return 30;   // UI v2: never over 30
    if (engine_playing() || engine_counting() >= 0 || engine_voices() > 0 || (msg_t && now - msg_t < 1600)) return 30;
    for (int i = 0; i < NSLOT; i++) if (flash[i] && now - flash[i] < 200) return 30;
    return 10;
}
// ---------- MIXER and FX screens ----------
static float mx_show[NSLOT + 3];         // meters as drawn: peaks with a fall
static void mx_meters(void) {
    static Uint32 last; Uint32 now = SDL_GetTicks() | 1;
    float m[NSLOT + 3]; engine_meters(m);
    if (!last || ms_since(now, last) > 400) { memset(m, 0, sizeof m); memset(mx_show, 0, sizeof mx_show); }   // the engine holds its peaks until they are read: after a
    last = now;                          // pause the first read is whatever sounded since, and the bars flicked on opening the mixer (2026-11-14, user)
    for (int i = 0; i < NSLOT + 3; i++) mx_show[i] = m[i] > mx_show[i] ? m[i] : mx_show[i] * 0.85f;
}
static void meter_bar(int x, int y, int w, int h, float v) {   // segments from the bottom; the top ones orange, red past the limiter
    int seg = h / 8; float db = v > 1e-5f ? 20 * log10f(v) : -100;
    int lit = (int)((db + 48) / 48 * seg + 0.5f); if (lit < 0) lit = 0; if (lit > seg) lit = seg;
    for (int i = 0; i < seg; i++) {
        Col c = i >= lit ? C_CELL : i >= seg - 2 ? C_RED : i >= seg - 6 ? C_ORANGE : C_PLAY;
        rect(x, y + h - (i + 1) * 8 + 2, w, 6, c, 1);
    }
}
static void fx_pstr(int type, int k, int v, char *b) {   // a parameter as the user reads it
    switch (type) {
    case FX_DIST: if (k == 0) { snprintf(b, 16, "%s", dist_mode[v & 3]); return; } break;
    case FX_FILTER: if (k == 0) { snprintf(b, 16, "%s", filt_mode[v % FILT_N]); return; } if (k == 4) { snprintf(b, 16, "%s", v ? div_label[(v - 1) % NDIV] : "OFF"); return; }
        if (k == 2) { snprintf(b, 16, "%.1f", 1 / (2 - 2 * (v / 100.f) * 0.98f)); return; } break;   // Q as fxdsp.h makes it: 0.5 .. 25
    case FX_PHASER: if (k == 4) { snprintf(b, 16, "%d", 4 + 2 * v); return; } break;
    case FX_EQ: { float hz = k == 2 ? eq_mid_hz(v) : k == 4 ? eq_lc_hz(v) : k == 5 ? eq_hc_hz(v) : 0;
        if ((k == 4 && !v) || (k == 5 && v >= 100)) { snprintf(b, 16, "OFF"); return; }
        if (hz > 0) { if (hz < 1000) snprintf(b, 16, "%.0fHZ", hz); else snprintf(b, 16, "%.1fKHZ", hz / 1000); return; }
        snprintf(b, 16, "%+.1fDB", eq_db(v)); return; }
    case FX_DELAY: if (k == 0) { snprintf(b, 16, "%s", div_label[v % NDIV]); return; } if (k == 3) { snprintf(b, 16, "%s", v ? "ON" : "OFF"); return; } if (k == 5) { snprintf(b, 16, "%d%%", v); return; } break;
    case FX_REVERB: if (k == 3) { snprintf(b, 16, "%dMS", v * 2); return; } if (k == 5) { snprintf(b, 16, "%d%%", v); return; } break;
    case FX_AUTOPAN: if (k == 0) { snprintf(b, 16, "%s", div_label[v % NDIV]); return; } if (k == 2) { snprintf(b, 16, "%s", pan_shape[v % 3]); return; } break;
    case FX_RING: if (k == 0) { snprintf(b, 16, "%.0fHZ", 20 * exp2f(v / 100.f * 8)); return; } if (k == 1) { snprintf(b, 16, "%+dC", v); return; } break;
    case FX_MULTI: if (k == 4 || k == 5) { float hz = k == 4 ? mb_xlow(v) : mb_xhigh(v); if (hz < 1000) snprintf(b, 16, "%.0fHZ", hz); else snprintf(b, 16, "%.1fKHZ", hz / 1000); return; }
        if (k == 1) { snprintf(b, 16, "%.1fX", mb_tscale(v)); return; } snprintf(b, 16, "%d%%", v); return;
    }
    snprintf(b, 16, "%d", v);
}
static void unit_pstr(int unit, int k, int v, char *b) {
    if (unit == NSLOT + 1 && k == DL_TIME) { snprintf(b, 16, "%s", div_label[v % NDIV]); return; }
    if ((unit == NSLOT && k == RV_MUTE) || (unit == NSLOT + 1 && (k == DL_PING || k == DL_MUTE)) || (unit == NSLOT + 2 && (k == MS_COMP || k == MS_LIM))) { snprintf(b, 16, "%s", v ? "ON" : "OFF"); return; }
    if (unit == NSLOT + 2 && k == MS_DUCKSRC) { if (v) snprintf(b, 16, "PAD %d", v); else snprintf(b, 16, "OFF"); return; }
    snprintf(b, 16, "%d", v);
}


// ---------- the instrument picker: EDIT on an empty pad (2026-11-09) ----------
// Four tiles, each with a miniature of the instrument's real SEQ+INST page, drawn once when the window opens:
// the pad is set to each instrument for a moment (only while its clip is off, so the engine plays nothing of it).
static const char *const pick_name[4] = {"SAMPLER", "WAVE", "ACID", "CLOUD"};
static const char *const pick_what[4] = {"SAMPLES, LOOPS, SLICES", "WAVETABLES, UNISON, MOD MATRIX", "303-STYLE BASS, ACCENT, SLIDE", "GRAINS FROM A SAMPLE"};
static int pk_cur; static SDL_Texture *pk_tex[4];
enum { PK_SY = 250, PK_SH = 694 - 250 };   // what a miniature shows: the panel below the steps
static int pk_change;                    // v3: the picker switches the engine of a pad that has one
static void pick_open(void) { picking = 1; pk_cur = 0; pk_change = 0; }
static void engine_pick(void) { pick_open(); pk_change = 1; pk_cur = song.t[sel].type & 3; }   // UI v2 (A03): a list, no panel pictures (they took 12 MB)
static void pick_close(void) {
    picking = 0;
    for (int i = 0; i < 4; i++) if (pk_tex[i]) { SDL_DestroyTexture(pk_tex[i]); pk_tex[i] = NULL; }   // 3 MB each
}
static void hub_open(void);
static void pick_take(int i) {
    pick_close();
    if (pk_change) {                      // v3: another engine for this pad; each engine keeps its own settings
        pk_change = 0; if (i == (song.t[sel].type & 3)) { cat_open(PC_SOUND); return; }
        Track *t = &song.t[sel]; edit_begin(); t->type = (int8_t)i;
        if (i == 1) wave_new(sel);
        if (i == 2 && t->ap[AP_MARK] != AP_INIT) acid_defaults(t->ap);
        if (i == 3 && t->cp[CP_MARK] != CP_INIT) cl_defaults(t->cp);
        cat_open(PC_SOUND); sound_sync();
        if ((i == 0 || i == 3) && !engine_sample(sel)) browse_slot();   // a sampler or grains without a sample: the browser
        return;
    }
    if (i == 0) { browse_slot(); return; }
    Track *t = &song.t[sel]; edit_begin(); t->type = (int8_t)i;
    if (i == 1) wave_new(sel);
    if (i == 2 && t->ap[AP_MARK] != AP_INIT) acid_defaults(t->ap);
    if (i == 3 && t->cp[CP_MARK] != CP_INIT) cl_defaults(t->cp);
    cl_deep = 0; cat_open(PC_SOUND); sound_sync();   // v3: the engine first, so its sound comes next
    if (i == 3) browse_slot();           // grains need a sample: the browser first
    else toast(i == 1 ? "WAVE IS ON THE PAD: OPTION PLAYS IT" : "ACID IS ON THE PAD: OPTION PLAYS IT");
}
static void pick_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) pick_take(pk_cur); return; }
    if (k == K_Y || k == K_OPT) { pick_close(); return; }
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_UP || k == K_DOWN) pk_cur = (pk_cur + (k == K_DOWN ? 1 : 3)) % 4;   // a list now, round
}
// The empty pad (UI v2, mockup A03): "what goes here?", the four engines as cards in their colours.
static void draw_pick(void) {
    static const char *const what[4] = {"sample or loop", "synth: chords, pads", "bass line 303", "granular textures"};
    const char *q = pk_change ? "Another engine for this pad?" : "Empty pad. What goes here?";
    ftext(W / 2 - ftext_w(FT_LB, q) / 2, 84, FT_LB, C_WHITE, q);
    enum { CX = 122, CWD = 778, CHT = 126, DY = 142, Y0 = 130 };
    for (int i = 0; i < 4; i++) {
        int y = Y0 + i * DY, on = i == pk_cur; Col ec = C_ENG(i);
        rrect(CX, y, CWD, CHT, 8, on ? mix(C_SELBG, C_DIM, 40) : C_CELL);
        rect(CX, y, 9, CHT, ec, 1);
        ftext(CX + 38, y + 10, FT_LB, ec, pick_name[i]);
        ftext(CX + CWD - 26 - ftext_w(FT_M, what[i]), y + 14, FT_M, on ? C_WHITE : C_DIM, what[i]);
    }
    draw_footer("EDIT CHOOSE  Y BACK");
}
// ---------- projects (2026-11-09) ----------
// state.bin stays the working copy (autosave). A project is projects/NAME.bpp, the same bytes; project.cfg remembers
// the open one. "Changes" = the song differs from what was last loaded or saved (clips playing do not count).
#define PRJ_CFG "project.cfg"
static Song prj_saved, prj_tmp;
static void prj_norm(Song *d, const Song *s) { *d = *s; for (int i = 0; i < NSLOT; i++) { d->t[i].on = 0; d->t[i].queued = -1; } }
static void prj_mark_saved(void) { prj_norm(&prj_saved, &song); }
static void prj_mark_saved_empty(void) {   // an untitled project counts as changed as soon as it has anything in it
    static Song keep; keep = song; song_defaults(); prj_norm(&prj_saved, &song); song = keep; }
static int prj_dirty(void) {
    static Uint32 t; static int v;
    Uint32 now = SDL_GetTicks();
    if (!t || ms_since(now, t) > 400) { t = now | 1; prj_norm(&prj_tmp, &song); v = memcmp(&prj_tmp, &prj_saved, sizeof prj_tmp) != 0; }
    return v;
}
static int prj_changed(void) { prj_norm(&prj_tmp, &song); return memcmp(&prj_tmp, &prj_saved, sizeof prj_tmp) != 0; }   // now, for decisions (prj_dirty is for drawing)
static void prj_cfg_write(void) { FILE *f = fopen(PRJ_CFG, "w"); if (f) { fprintf(f, "%s\n", prj_cur); fclose(f); } }
static void prj_path(const char *name, char *p, int n) { snprintf(p, n, PRJ_DIR "/%s.bpp", name); }
static void prj_save(const char *name) {
    mkdir(PRJ_DIR, 0777);
    char p[96]; prj_path(name, p, sizeof p);
    scene_poll(); scn_mix_sync();
    size_t one = sizeof(Song), sz = one + (scn_n > 1 ? sizeof(ScnHead) + (size_t)scn_n * one : 0); char *buf = malloc(sz);   // the playing scene, then all of them
    if (!buf) { toast("COULD NOT SAVE"); return; }
    memcpy(buf, &song, one);
    if (scn_n > 1) { ScnHead h = {SCN_MAGIC, scn_n, scn_cur, (int32_t)sizeof(Song)}; memcpy(buf + one, &h, sizeof h);
        for (int i = 0; i < scn_n; i++) memcpy(buf + one + sizeof h + (size_t)i * one, i == scn_cur || !scn[i] ? &song : scn[i], one); }
    int ok = write_whole(p, buf, sz); free(buf);
    if (!ok) { toast("COULD NOT SAVE"); return; }
    snprintf(prj_cur, sizeof prj_cur, "%s", name); prj_cfg_write(); prj_mark_saved(); save_state(); sync();
    char b[48]; snprintf(b, sizeof b, "SAVED %s", name); toast(b);
}
static void prj_load(const char *path) {
    { FILE *f = fopen(path, "rb"); uint32_t m = 0; if (f) { if (fread(&m, sizeof m, 1, f) != 1) m = 0; fclose(f); }   // 2026-11-25 (user): only the current format;
      if (m != STATE_MAGIC) { toast((m & 0xFFFFFF00u) == 0xB2B20000u ? "OLD PROJECT FORMAT: NOT OPENED" : "PROJECT FILE UNREADABLE"); return; } }   // the open project stays
    rec = 0; combo_t = 0;
    if (engine_playing() || engine_counting() >= 0) transport_toggle();
    engine_stop_all(); cap_n = 0; spage = 0; undo_n = undo_top = 0;   // undo does not cross projects
    load_from(path); scenes_read(path, (long)file_song_sz); save_state();
    const char *b = strrchr(path, '/'); b = b ? b + 1 : path; int l = (int)strlen(b); if (l > 4) l -= 4; if (l > 31) l = 31;
    snprintf(prj_cur, sizeof prj_cur, "%.*s", l, b); prj_cfg_write(); prj_mark_saved();
    if (browsing) browse_close();
    char t[48]; snprintf(t, sizeof t, "OPENED %s", prj_cur); toast(t);
}
static void prj_startup(void) {          // the open project's name, and what its file holds (to know what changed)
    prj_cur[0] = 0; FILE *f = fopen(PRJ_CFG, "r");
    if (f) { char l[64] = ""; if (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; snprintf(prj_cur, sizeof prj_cur, "%s", l); } fclose(f); }
    if (prj_cur[0]) {
        char p[96]; prj_path(prj_cur, p, sizeof p); FILE *g = fopen(p, "rb"); static Song fs; size_t got = 0;
        if (g) { got = fread(&fs, 1, sizeof fs, g); fclose(g); }
        static Song ws; size_t wg = 0; FILE *w = fopen(STATE, "rb"); if (w) { wg = fread(&ws, 1, sizeof ws, w); fclose(w); }
        if (got == sizeof fs && fs.magic == STATE_MAGIC && wg == sizeof ws) {   // both written raw: the same bytes = nothing changed since the save
            static Song a2, b2; prj_norm(&a2, &fs); prj_norm(&b2, &ws);
            if (!memcmp(&a2, &b2, sizeof a2)) prj_mark_saved(); else prj_saved = a2;
        } else prj_mark_saved();
    } else prj_mark_saved_empty();
}
static void prj_browse(void) {
    mkdir(PRJ_DIR, 0777); bmode = 1; browse_open(PRJ_DIR);
    for (int i = 0; i < bn; i++) { char nm[40]; snprintf(nm, sizeof nm, "%s.bpp", prj_cur); if (!strcmp(bent[i].name, nm)) bcur = i; }
}
// the info of a project file, read once per file (a few small reads, never the whole file)
static struct { char path[512]; long mt; int ok, tempo, key, scale; int8_t type[NSLOT]; char nm[NSLOT][24]; } pin;
static void prj_info_draw2(void);
static void prj_info_draw(const char *path) {
    struct stat st; long mt = !stat(path, &st) ? (long)st.st_mtime : 0;
    if (strcmp(pin.path, path) || pin.mt != mt) {
        memset(&pin, 0, sizeof pin); snprintf(pin.path, sizeof pin.path, "%s", path); pin.mt = mt;
        FILE *f = fopen(path, "rb");
        if (f) { static Song h; memset(&h, 0, offsetof(Song, t));
            if (fread(&h, 1, offsetof(Song, t), f) == offsetof(Song, t) && (h.magic & 0xFFFFFF00u) == 0xB2B20000u) { pin.ok = 1; pin.tempo = h.tempo; pin.key = h.key; pin.scale = h.scale; }
            for (int s = 0; s < NSLOT && pin.ok; s++) { Track tr; long o = (long)offsetof(Song, t) + (long)s * (long)sizeof(Track);
                if (fseek(f, o + (long)offsetof(Track, type), SEEK_SET) || fread(&tr.type, 1, 1, f) != 1) break;
                char pp[256] = ""; if (fseek(f, o + (long)offsetof(Track, path), SEEK_SET) || fread(pp, 1, sizeof pp, f) != sizeof pp) break; pp[255] = 0;
                pin.type[s] = tr.type; const char *bn2 = strrchr(pp, '/'); snprintf(pin.nm[s], 24, "%.23s", tr.type ? type_name[tr.type % 4] : pp[0] ? (bn2 ? bn2 + 1 : pp) : ""); }
            fclose(f); }
    }
    prj_info_draw2();
}
// SAVE / DISCARD / CANCEL before opening another project or starting a new one
static int ask_act, ask_cur; static char ask_path[512];
static void ask_do(void) { if (ask_act == 1) prj_load(ask_path); else if (ask_act == 2) new_project(); ask_act = 0; }
static void ask_open(int act, const char *path) { asking = 1; ask_act = act; ask_cur = 0; snprintf(ask_path, sizeof ask_path, "%s", path); }
static void prj_open_req(const char *path) { if (prj_changed()) ask_open(1, path); else prj_load(path); }
static void ask_key(int k, int down) {
    if (!down) { if (k == K_EDIT && !used[K_EDIT]) {
        asking = 0;
        if (ask_cur == 0) { if (prj_cur[0]) { prj_save(prj_cur); ask_do(); } else naming_open(ask_act); }   // SAVE (a name first if it has none)
        else if (ask_cur == 1) ask_do();                                                                      // DISCARD
        else ask_act = 0; }                                                                                   // CANCEL
        return; }
    if (k != K_EDIT) used[k] = 1;   // EDIT acts on its release
    if (k == K_LEFT) ask_cur = (ask_cur + 2) % 3; else if (k == K_RIGHT) ask_cur = (ask_cur + 1) % 3;
    else if (k == K_Y || k == K_OPT) { asking = 0; ask_act = 0; }
}
// the name, typed as on the M8: LEFT/RIGHT the place, EDIT+UP/DOWN the character, OPTION deletes, EDIT saves
// the name editor, as on the M8: LEFT/RIGHT the place, EDIT+UP/DOWN the character (EDIT+<> by 10), OPTION deletes,
// EDIT confirms, Y cancels. Modes: a project's name (then save), a new folder, a new name for a file or folder.
#define NM_MAX 60                         // characters; the editor shows 30 places and scrolls
enum { NM_PROJECT, NM_FOLDER, NM_RENAME, NM_THEME, NM_PAD };
static int nm_pos, nm_then, nm_mode; static char nm_buf[NM_MAX + 2], nm_ext[16], nm_old[1024];
static void naming_open(int then) {
    naming = 1; nm_then = then; nm_mode = NM_PROJECT; nm_ext[0] = 0;
    if (prj_cur[0]) snprintf(nm_buf, sizeof nm_buf, "%s", prj_cur);
    else for (int i = 1; i < 1000; i++) { char p[96]; snprintf(nm_buf, sizeof nm_buf, "SONG %03d", i); prj_path(nm_buf, p, sizeof p); if (access(p, F_OK)) break; }
    nm_pos = (int)strlen(nm_buf); if (nm_pos > NM_MAX - 1) nm_pos = NM_MAX - 1;
}
static void naming_file(int mode, const char *name, int keep_ext) {   // a folder (NM_FOLDER) or a rename of bdir/name
    naming = 1; nm_then = 0; nm_mode = mode; nm_ext[0] = 0;
    if (mode == NM_RENAME) {
        snprintf(nm_old, sizeof nm_old, "%s/%s", bdir, name);
        const char *e = keep_ext ? strrchr(name, '.') : NULL;
        if (e && e != name && strlen(e) < sizeof nm_ext) { snprintf(nm_ext, sizeof nm_ext, "%s", e); snprintf(nm_buf, sizeof nm_buf, "%.*s", (int)(e - name) > NM_MAX ? NM_MAX : (int)(e - name), name); }
        else snprintf(nm_buf, sizeof nm_buf, "%.*s", NM_MAX, name);
    } else snprintf(nm_buf, sizeof nm_buf, "NEW FOLDER");
    nm_pos = (int)strlen(nm_buf); if (nm_pos > NM_MAX - 1) nm_pos = NM_MAX - 1;
}
static void naming_pad(void) {            // SHIFT+X on GRID: the pad's own name; an empty one gives the instrument's name back
    if (!has_sound(sel)) { toast("EMPTY PAD: NOTHING TO NAME"); return; }
    naming = 1; nm_then = 0; nm_mode = NM_PAD; nm_ext[0] = 0;
    snprintf(nm_buf, sizeof nm_buf, "%.15s", song.pname[sel][0] ? song.pname[sel] : slot_name(sel) ? slot_name(sel) : "");
    nm_pos = (int)strlen(nm_buf); if (nm_pos > NM_MAX - 1) nm_pos = NM_MAX - 1;
}
static void naming_theme(void) {          // SAVE AS in the theme editor
    naming = 1; nm_then = 0; nm_mode = NM_THEME; nm_ext[0] = 0;
    snprintf(nm_buf, sizeof nm_buf, "%s", th_sel >= TH_PRESETS ? theme_name(th_sel) : "MY THEME");
    nm_pos = (int)strlen(nm_buf); if (nm_pos > NM_MAX - 1) nm_pos = NM_MAX - 1;
}
static void browse_reopen(const char *select) {   // the folder again, the cursor on this name
    char d[512]; snprintf(d, sizeof d, "%s", bdir); browse_open(d);
    if (select) for (int i = 0; i < bn; i++) if (!strcmp(bent[i].name, select)) bcur = i;
}
static void naming_ok(void) {
    int l = (int)strlen(nm_buf); while (l && nm_buf[l - 1] == ' ') nm_buf[--l] = 0;
    int s0 = 0; while (nm_buf[s0] == ' ' || nm_buf[s0] == '.') s0++; if (s0) memmove(nm_buf, nm_buf + s0, strlen(nm_buf + s0) + 1);   // never a hidden file
    if (nm_mode == NM_PAD) { naming = 0; push_undo(); snprintf(song.pname[sel], sizeof song.pname[sel], "%.15s", nm_buf); toast(nm_buf[0] ? "PAD NAMED" : "NAME CLEARED: THE INSTRUMENT'S NAME"); return; }
    if (!nm_buf[0]) { toast("TYPE A NAME FIRST"); return; }
    if (nm_mode == NM_PROJECT) { naming = 0; prj_save(nm_buf); if (nm_then) { ask_act = nm_then; ask_do(); } return; }
    if (nm_mode == NM_THEME) { naming = 0; theme_save_as(nm_buf); return; }
    char nn[300], np[1024]; snprintf(nn, sizeof nn, "%s%s", nm_buf, nm_ext); snprintf(np, sizeof np, "%s/%s", bdir, nn);
    if (nm_mode == NM_FOLDER) {
        if (!access(np, F_OK)) { toast("THAT NAME IS TAKEN"); return; }
        if (mkdir(np, 0777)) { toast("COULD NOT MAKE THE FOLDER"); return; }
        sync(); naming = 0; browse_reopen(nn); toast("FOLDER MADE"); return;
    }
    if (!strcmp(np, nm_old)) { naming = 0; return; }   // the same name: nothing to do
    if (!access(np, F_OK)) { toast("THAT NAME IS TAKEN"); return; }
    if (rename(nm_old, np)) { toast("COULD NOT RENAME"); return; }
    sync(); naming = 0;
    size_t ol = strlen(nm_old);
    for (int s2 = 0; s2 < NSLOT; s2++) {                // pads using it (or anything inside a renamed folder) follow
        char *tp = song.t[s2].path;
        if (!strncmp(tp, nm_old, ol) && (tp[ol] == 0 || tp[ol] == '/')) { char q[1024]; snprintf(q, sizeof q, "%s%s", np, tp + ol); if (strlen(q) < 256) snprintf(tp, 256, "%s", q); slot_want[s2][0] = 0; }
    }
    if (bmode) { char cp[96]; prj_path(prj_cur, cp, sizeof cp); if (prj_cur[0] && !strcmp(cp, nm_old) && strlen(nm_buf) < sizeof prj_cur) { snprintf(prj_cur, sizeof prj_cur, "%s", nm_buf); prj_cfg_write(); } }
    browse_reopen(nn); toast("RENAMED");
}
// The name editor's keyboard (2026-11-12, user: typing by scrolling each letter was "extremely inconvenient"):
// a grid of keys under the name. Arrows choose a key, EDIT types it at the cursor, OPT deletes the character before
// the cursor, L1/R1 move the cursor along the name, X upper/lower case, PLAY or the OK key finishes, Y cancels.
// The cursor starts on OK: one EDIT keeps the offered name.
#define NK_COLS 10
#define NK_ROWS 5                         // four rows of characters, the last row: DEL, aA, ( ), OK
static const char nk_keys[4][NK_COLS + 1] = {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-_. "};
enum { NK_DEL, NK_CASE, NK_LP, NK_RP, NK_OK, NK_NB };
static const int8_t nk_bx[NK_NB] = {0, 3, 5, 6, 7}, nk_bw[NK_NB] = {3, 2, 1, 1, 3};   // the bottom row: first column and width of each button
static int nk_r = NK_ROWS - 1, nk_c = 7, nk_low;
static int nk_btn(void) { for (int b = NK_NB - 1; b >= 0; b--) if (nk_c >= nk_bx[b]) return b; return 0; }
static void nk_reset(void) { nk_r = NK_ROWS - 1; nk_c = nk_bx[NK_OK]; }
static void nk_del(void) { int l = (int)strlen(nm_buf); if (nm_pos > 0) { memmove(nm_buf + nm_pos - 1, nm_buf + nm_pos, (size_t)(l - nm_pos + 1)); nm_pos--; } else if (l > 0) memmove(nm_buf, nm_buf + 1, (size_t)l); }
static void nk_type(char ch) {
    int l = (int)strlen(nm_buf); if (l >= NM_MAX) { toast("THE NAME IS FULL"); return; }
    if (nm_pos > l) nm_pos = l;
    memmove(nm_buf + nm_pos + 1, nm_buf + nm_pos, (size_t)(l - nm_pos + 1)); nm_buf[nm_pos++] = ch;
}
static void nm_key(int k, int down) {
    static Uint32 opened; if (!naming) return;
    if (!down) return;
    used[k] = 1;
    if (k == K_Y) { naming = 0; nm_then = 0; ask_act = 0; nk_reset(); return; }
    if (k == K_PLAY) { nk_reset(); naming_ok(); return; }
    if (k == K_OPT) { nk_del(); return; }
    if (k == K_SHIFT || k == K_X) { nk_low = !nk_low; return; }   // v3: SHIFT the case (X is gone)
    int l = (int)strlen(nm_buf); (void)opened;
    if (k == K_L1) { nm_pos = nm_pos > 0 ? nm_pos - 1 : 0; return; }
    if (k == K_R1) { nm_pos = nm_pos < l ? nm_pos + 1 : l; return; }
    if (k == K_UP) nk_r = nk_r > 0 ? nk_r - 1 : 0;
    else if (k == K_DOWN) nk_r = nk_r < NK_ROWS - 1 ? nk_r + 1 : NK_ROWS - 1;
    else if (k == K_LEFT || k == K_RIGHT) {
        int d = k == K_RIGHT ? 1 : -1;
        if (nk_r < NK_ROWS - 1) nk_c = (nk_c + d + NK_COLS) % NK_COLS;
        else { int b = (nk_btn() + d + NK_NB) % NK_NB; nk_c = nk_bx[b]; }       // the bottom row goes button by button
    } else if (k == K_EDIT) {
        if (nk_r < NK_ROWS - 1) { char ch = nk_keys[nk_r][nk_c]; if (nk_low && ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32); nk_type(ch); return; }
        switch (nk_btn()) {
        case NK_DEL: nk_del(); break; case NK_CASE: nk_low = !nk_low; break;
        case NK_LP: nk_type('('); break; case NK_RP: nk_type(')'); break;
        case NK_OK: { int was = naming; naming_ok(); if (was && !naming) nk_reset(); break; }
        }
    }
}
// ---------- the file menu of the browsers (X): NEW FOLDER, RENAME, DUPLICATE, DELETE (2026-11-09) ----------
// Only the open folder is touched; deleting a folder walks only that folder (never a scan of the card).
static int fm_cur, fc_files, fc_used; static char fc_path[1024];
static const char *const fm_name[6] = {"NEW FOLDER", "RENAME", "DUPLICATE", "CUT", "PASTE HERE", "DELETE"};
static char fm_cut[1024];                 // CUT: what PASTE HERE moves
static int count_files(const char *dir, int depth) {   // files inside a folder (to say what DELETE takes), capped
    if (depth > 8) return 0; DIR *d = opendir(dir); if (!d) return 0; int n = 0; struct dirent *e;
    while ((e = readdir(d)) && n < 100000) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char p[1024]; snprintf(p, sizeof p, "%s/%s", dir, e->d_name); struct stat st;
        if (!lstat(p, &st) && S_ISDIR(st.st_mode)) n += count_files(p, depth + 1); else n++; }
    closedir(d); return n;
}
static int remove_tree(const char *path, int depth) {   // 0 ok
    struct stat st; if (lstat(path, &st)) return -1;
    if (!S_ISDIR(st.st_mode)) return unlink(path);
    if (depth > 8) return -1;
    DIR *d = opendir(path); if (!d) return -1; struct dirent *e; int bad = 0;
    while ((e = readdir(d))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char p[1024]; snprintf(p, sizeof p, "%s/%s", path, e->d_name); if (remove_tree(p, depth + 1)) bad = 1; }
    closedir(d); return bad ? -1 : rmdir(path);
}
static int copy_file(const char *from, const char *to) {   // 0 ok
    FILE *a = fopen(from, "rb"); if (!a) return -1; FILE *b = fopen(to, "wb"); if (!b) { fclose(a); return -1; }
    static char buf[1 << 16]; size_t n; int bad = 0;
    while ((n = fread(buf, 1, sizeof buf, a)) > 0) if (fwrite(buf, 1, n, b) != n) { bad = 1; break; }
    fclose(a); if (fclose(b)) bad = 1; if (bad) unlink(to); return bad ? -1 : 0;
}
static int copy_tree(const char *from, const char *to, int depth) {   // a file, or a folder with all in it; 0 ok
    struct stat st; if (stat(from, &st)) return -1;
    if (!S_ISDIR(st.st_mode)) return copy_file(from, to);
    if (depth > 8 || mkdir(to, 0777)) return -1;
    DIR *d = opendir(from); if (!d) return -1; struct dirent *e; int bad = 0;
    while ((e = readdir(d))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char a[1024], b2[1024]; snprintf(a, sizeof a, "%s/%s", from, e->d_name); snprintf(b2, sizeof b2, "%s/%s", to, e->d_name); if (copy_tree(a, b2, depth + 1)) bad = 1; }
    closedir(d); return bad ? -1 : 0;
}
static void paths_follow(const char *old, const char *nw) {   // pads using a moved file (or anything in a moved folder) follow it
    size_t ol = strlen(old);
    for (int s2 = 0; s2 < NSLOT; s2++) { char *tp = song.t[s2].path;
        if (!strncmp(tp, old, ol) && (tp[ol] == 0 || tp[ol] == '/')) { char q[1024]; snprintf(q, sizeof q, "%s%s", nw, tp + ol); if (strlen(q) < 256) snprintf(tp, 256, "%s", q); slot_want[s2][0] = 0; } }
}
static void fmenu_open(void) { fmenu = 1; fm_cur = fm_cut[0] ? 4 : bn ? 1 : 0; }
static void fm_do(int i) {
    fmenu = 0;
    if (i == 0) { naming_file(NM_FOLDER, "", 0); return; }
    if (i == 4) {                         // PASTE HERE: the cut file or folder moves into the open folder
        if (!fm_cut[0]) { toast("NOTHING CUT: X, CUT ON A FILE FIRST"); return; }
        const char *nm = strrchr(fm_cut, '/'); nm = nm ? nm + 1 : fm_cut; char np[1024]; size_t cl = strlen(fm_cut);
        snprintf(np, sizeof np, "%s/%s", bdir, nm);
        if (!strcmp(np, fm_cut)) { fm_cut[0] = 0; toast("ALREADY HERE"); return; }
        if (!strncmp(bdir, fm_cut, cl) && (bdir[cl] == 0 || bdir[cl] == '/')) { toast("NOT INTO ITSELF"); return; }
        if (!access(np, F_OK)) { toast("THAT NAME IS TAKEN HERE"); return; }
        if (rename(fm_cut, np)) { toast("COULD NOT MOVE"); return; }
        paths_follow(fm_cut, np); fm_cut[0] = 0; sync(); browse_reopen(nm); toast("MOVED"); return;
    }
    if (!bn || bcur >= bn) { toast("NOTHING HERE"); return; }
    const Entry *e = &bent[bcur]; char p[1024]; snprintf(p, sizeof p, "%s/%s", bdir, e->name);
    if (i == 1) { const char *x = e->dir ? NULL : strrchr(e->name, '.'); int bl = x ? (int)(x - e->name) : (int)strlen(e->name);
        if (bl > NM_MAX) { toast("NAME LONGER THAN 60: RENAME IT ON A COMPUTER"); return; }
        naming_file(NM_RENAME, e->name, !e->dir); return; }
    if (i == 3) { snprintf(fm_cut, sizeof fm_cut, "%s", p); toast("CUT: OPEN A FOLDER, X, PASTE HERE"); return; }
    if (i == 2) {
        const char *x = e->dir ? NULL : strrchr(e->name, '.'); int bl = x ? (int)(x - e->name) : (int)strlen(e->name); char nn[300], np[1024];
        for (int k = 1; k < 100; k++) {
            if (k == 1) snprintf(nn, sizeof nn, "%.*s COPY%s", bl, e->name, x ? x : ""); else snprintf(nn, sizeof nn, "%.*s COPY %d%s", bl, e->name, k, x ? x : "");
            snprintf(np, sizeof np, "%s/%s", bdir, nn); if (access(np, F_OK)) break; }
        if (copy_tree(p, np, 0)) { sync(); browse_reopen(nn); toast("COULD NOT COPY ALL OF IT"); return; }
        sync(); browse_reopen(nn); toast("DUPLICATED"); return;
    }
    snprintf(fc_path, sizeof fc_path, "%s", p); fc_files = e->dir ? count_files(p, 0) : 1; fc_used = 0;
    size_t pl = strlen(p);
    for (int s2 = 0; s2 < NSLOT; s2++) { const char *tp = song.t[s2].path; if (!strncmp(tp, p, pl) && (tp[pl] == 0 || tp[pl] == '/')) fc_used++; }
    fconfirm = 1; fm_cur = 1;            // the cursor on CANCEL: deleting takes a deliberate move
}
static void fc_delete(void) {
    fconfirm = 0; struct stat st; int dir = !stat(fc_path, &st) && S_ISDIR(st.st_mode);
    if (remove_tree(fc_path, 0)) toast(dir ? "SOME FILES COULD NOT BE DELETED" : "COULD NOT DELETE");
    else toast("DELETED");
    sync();
    if (bmode && prj_cur[0]) { char cp[96]; prj_path(prj_cur, cp, sizeof cp); if (!strcmp(cp, fc_path)) { prj_cur[0] = 0; prj_cfg_write(); prj_mark_saved_empty(); } }   // the open project's file: it is untitled now
    int keep = bcur; browse_reopen(NULL); bcur = clampi(keep, 0, bn ? bn - 1 : 0);
}
static int fm_key(int k) {               // keys while the file menu or the question is open; 1 = used
    if (fconfirm) {
        if (k == K_LEFT || k == K_RIGHT) fm_cur ^= 1;
        else if (k == K_EDIT) { if (fm_cur == 0) fc_delete(); else fconfirm = 0; }
        else if (k == K_Y || k == K_OPT || k == K_X) fconfirm = 0;
        return 1;
    }
    if (fmenu) {
        if (k == K_UP) fm_cur = (fm_cur + 5) % 6; else if (k == K_DOWN) fm_cur = (fm_cur + 1) % 6;
        else if (k == K_EDIT) fm_do(fm_cur);
        else if (k == K_Y || k == K_OPT || k == K_X) fmenu = 0;
        return 1;
    }
    if (k == K_X) { fmenu_open(); return 1; }
    return 0;
}
static void draw_fmenu2(void);
static void draw_fmenu(void) {
    draw_fmenu2();
}
// ---------- EXPORT WAV (2026-11-09) ----------
// The window: BARS, CLIPS (the ones on, or all with notes), TAIL, EXPORT. Rendering pauses the audio device and runs
// callback() from the UI thread, a slice per frame, into exports/NAME.wav (48 kHz, 16 bit, stereo).
static const int ex_bars_v[7] = {1, 2, 4, 8, 16, 32, 64}, ex_tail_v[4] = {0, 2, 4, 8};
// 2026-11-19 (user: "an export menu with settings, as in Ableton"): WHAT (the mix, a file per pad, both, the open pad),
// SCENES (all in turn or the open one), AS LOOP (the tail folded onto the start), NORMALIZE (-0.3 dB peak), FORMAT
// (16 / 24 bit, 32 float), MONO. The render goes to a float file (exports/.render), the WAV is written from it at the end.
enum { EX_BARS, EX_FROM, EX_CLIPS, EX_TAIL, EX_STEMS, EX_SCN, EX_LOOP, EX_NORM, EX_FMT, EX_MONO, EX_GO, EX_ROWS };
static int ex_row, ex_bars = 3, ex_from = 1, ex_all, ex_tail = 1, ex_stems, ex_run, ex_pad; static long ex_done, ex_total, ex_body, ex_skip; static FILE *ex_f;
static int ex_what, ex_scnall = 1, ex_loop, ex_norm, ex_fmt, ex_mono, ex_mixed;   // WHAT 0 MIX 1 PADS 2 MIX+PADS 3 THIS PAD; ex_mixed: the mix of MIX+PADS is done
static char ex_path[160]; static int8_t ex_on[NSLOT], ex_solo[NSLOT]; static float ex_peak;
static int ex_scn, ex_k, ex_home, ex_all0; static long ex_seg;   // 2026-11-15 (user): ONE MIX of a project with scenes = every scene in turn, BARS each
#define EX_TMP "exports/.render"
static void ex_scene(int n) {            // the device is paused: the scene comes in on the next rendered step
    float z[256]; scene_go(n); scn_play = 0; engine_scene_force(); if (!engine_playing()) engine_render(z, 128); scene_poll(); }
static void export_open(void) {
    exporting = 1; ex_resample = 0; ex_row = EX_GO; ex_run = 0;
    int any = 0; for (int s = 0; s < NSLOT; s++) if (song.t[s].on) any = 1;
    ex_all = !any;                        // nothing on: every clip with notes
}
static void wav_header(FILE *f, long frames) {
    uint16_t ch = ex_mono ? 1 : 2, bits = ex_fmt == 2 ? 32 : ex_fmt == 1 ? 24 : 16, fmt = ex_fmt == 2 ? 3 : 1, ba = (uint16_t)(ch * bits / 8);
    uint32_t data = (uint32_t)(frames * ba), riff = 36 + data, sr = SR, br = SR * ba, n16 = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&n16, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
}
static int ex_pads(void) { return ex_what == 1 || ex_what == 3 || (ex_what == 2 && ex_mixed); }   // this pass writes pads, not the mix
static int ex_used(int s) { return ex_all ? has_sound(s) && (pat_used(cur_pat(s)) || roll_any(cur_pat(s))) : ex_on[s]; }
static int ex_pass(void) {               // one file: the mix, or pad ex_pad alone; 0 when there is no more to do
    ex_stems = ex_pads();
    if (ex_stems) { if (ex_what == 3) { if (ex_pad > sel) return 0; ex_pad = sel; } else while (ex_pad < NSLOT && !ex_used(ex_pad)) ex_pad++; if (ex_pad >= NSLOT) return 0; }
    const char *nm = prj_cur[0] ? prj_cur : "UNTITLED"; char base[96];
    if (ex_stems) snprintf(base, sizeof base, "%s PAD%02d", nm, ex_pad + 1); else snprintf(base, sizeof base, "%s", nm);
    snprintf(ex_path, sizeof ex_path, "exports/%s.wav", base);
    for (int i = 2; i < 100 && !access(ex_path, F_OK); i++) snprintf(ex_path, sizeof ex_path, "exports/%s %d.wav", base, i);
    ex_f = fopen(EX_TMP, "w+b"); if (!ex_f) { toast("COULD NOT WRITE exports/"); return 0; }
    for (int s = 0; s < NSLOT; s++) { song.t[s].on = (int8_t)(ex_used(s) || (ex_what == 3 && s == sel && has_sound(s))); song.t[s].queued = -1; song.mix.pad[s].solo = (uint8_t)(ex_stems && s == ex_pad); }
    engine_stop_all(); { float z[256]; for (int i = 0; i < 8; i++) engine_render(z, 128); }
    if (ex_scn && scn_cur) ex_scene(0);
    engine_transport(1);
    double bar_s = (song.beats > 0 ? song.beats : 4) * 60.0 / song.tempo;
    ex_skip = (long)(bar_s * (ex_from - 1) * SR + 0.5);   // FROM BAR: the bars before it are played, not written
    ex_body = (long)(bar_s * ex_bars_v[ex_bars] * SR + 0.5); ex_seg = ex_body; ex_k = 0;
    if (ex_scn) { ex_skip = 0; ex_body = ex_seg * scn_n; }
    ex_total = ex_body + (long)ex_tail_v[ex_tail] * SR; ex_done = 0; ex_peak = 0;
    return 1;
}
static void export_start(void) {
    mkdir("exports", 0777);
    rec = 0; combo_t = 0; rec_wait = 0;
    if (engine_playing() || engine_counting() >= 0) transport_toggle();
    for (int s = 0; s < NSLOT; s++) { ex_on[s] = song.t[s].on; ex_solo[s] = (int8_t)song.mix.pad[s].solo; }
    engine_pause(1); ex_pad = 0; ex_mixed = 0;
    scene_poll(); ex_scn = ex_scnall && ex_what != 3 && scn_n > 1; ex_home = scn_cur; ex_all0 = ex_all;
    if (ex_scn) { if (scn_cur) ex_scene(0); ex_all = 1; }
    if (ex_pass()) ex_run = 1; else { if (ex_scn) { ex_all = ex_all0; if (scn_cur != ex_home) ex_scene(ex_home); ex_scn = 0; } engine_pause(0); toast(ex_stems ? "NO PAD TO EXPORT" : "COULD NOT EXPORT"); }
}
static int ex_write(void) {              // the float render -> the WAV as asked: loop fold, mono, normalize, format
    long T = ex_done > ex_body ? ex_done - ex_body : 0, out = ex_loop ? (ex_done < ex_body ? ex_done : ex_body) : ex_done;
    float *tail = NULL; if (ex_loop && T) { tail = malloc((size_t)T * 8); if (!tail) return 0; fseek(ex_f, ex_body * 8, SEEK_SET); if (fread(tail, 8, (size_t)T, ex_f) != (size_t)T) { free(tail); return 0; } }
    FILE *o = fopen(ex_path, "wb"); if (!o) { free(tail); return 0; }
    wav_header(o, out); float peak = 0, g = 1; static float fb[2 * 4096]; static uint8_t ob[4 * 2 * 4096];
    for (int pass = ex_norm ? 0 : 1; pass < 2; pass++) {
        fseek(ex_f, 0, SEEK_SET);
        for (long at = 0; at < out; ) {
            int n = out - at > 4096 ? 4096 : (int)(out - at); if (fread(fb, 8, (size_t)n, ex_f) != (size_t)n) { fclose(o); free(tail); return 0; }
            for (int i = 0; i < n; i++) { long f = at + i;
                if (tail) for (long w = f; w < T; w += out) { fb[2 * i] += tail[2 * w]; fb[2 * i + 1] += tail[2 * w + 1]; }   // AS LOOP: what rang past the end, over the start
                if (ex_mono) fb[2 * i] = fb[2 * i + 1] = (fb[2 * i] + fb[2 * i + 1]) * 0.5f; }
            if (!pass) { for (int i = 0; i < 2 * n; i++) if (fabsf(fb[i]) > peak) peak = fabsf(fb[i]); at += n; continue; }
            int ch = ex_mono ? 1 : 2, k = 0;
            for (int i = 0; i < n; i++) for (int c = 0; c < ch; c++) { float v = fb[2 * i + c] * g;
                if (ex_fmt == 2) { memcpy(ob + k, &v, 4); k += 4; continue; }
                v = v > 1 ? 1 : v < -1 ? -1 : v;
                if (ex_fmt == 1) { int32_t x = (int32_t)lrintf(v * 8388607); ob[k++] = (uint8_t)x; ob[k++] = (uint8_t)(x >> 8); ob[k++] = (uint8_t)(x >> 16); }
                else { int16_t x = (int16_t)lrintf(v * 32767); memcpy(ob + k, &x, 2); k += 2; } }
            if (fwrite(ob, 1, (size_t)k, o) != (size_t)k) { fclose(o); free(tail); return 0; }
            at += n; }
        if (!pass) g = peak > 1e-6f ? 0.966f / peak : 1;   // -0.3 dB
    }
    free(tail); return !fclose(o);
}
static void export_finish(int keep) {
    engine_transport(0); { float z[256]; engine_render(z, 128); } engine_stop_all();
    if (ex_f) { if (keep && !ex_write()) { toast("COULD NOT WRITE THE WAV"); keep = 0; } fclose(ex_f); ex_f = NULL; unlink(EX_TMP); if (!keep) unlink(ex_path); sync(); }
    if (keep && ex_what == 2 && !ex_mixed) { ex_mixed = 1; ex_pad = 0; if (ex_pass()) return; }   // MIX + PADS: now the pads
    else if (keep && ex_stems && ex_what != 3) { ex_pad++; if (ex_pass()) return; }   // the next pad
    if (ex_scn) { ex_all = ex_all0; if (scn_cur != ex_home) ex_scene(ex_home); ex_scn = 0; }   // back in the scene that was open
    for (int s = 0; s < NSLOT; s++) { song.t[s].on = ex_on[s]; song.mix.pad[s].solo = (uint8_t)ex_solo[s]; }
    engine_pause(0); ex_run = 0;
    if (keep) { char b[200]; snprintf(b, sizeof b, ex_what == 1 || ex_what == 2 ? "FILES IN exports/" : "EXPORTED %s", ex_path); toast(b); exporting = 0;
        if (ex_resample && !ex_stems) {   // RESAMPLE: the render goes onto the first pad with no sound
            int s = 0; while (s < NSLOT && has_sound(s)) s++; char ap[512];
            if (s >= NSLOT) toast("NO EMPTY PAD: THE FILE IS IN exports/");
            else if (realpath(ex_path, ap)) { push_undo(); song.t[s].type = 0; set_slot_file(s, ap); snprintf(b, sizeof b, "RESAMPLED INTO PAD %d", s + 1); toast(b); }
        }
        ex_resample = 0; } else toast("EXPORT CANCELLED");
}
static void export_tick(void) {          // a slice of the render per frame: about a second of audio
    if (!ex_run) return;
    static float buf[2 * 128];
    for (int n = 0; n < 375; n++) {
        if (ex_skip > 0) { engine_render(buf, 128); ex_skip -= 128; continue; }   // before FROM BAR
        if (ex_done >= ex_total) break;
        if (ex_done < ex_body && ex_done + 128 >= ex_body) engine_transport(0);   // the bars are done: the tail rings out
        else if (ex_scn && ex_k + 1 < scn_n && ex_done + 256 >= ex_seg * (ex_k + 1)) { ex_k++; scene_go(ex_k); scn_play = 0; engine_scene_force(); }   // the next scene on the bar
        engine_render(buf, 128); if (ex_scn) scene_poll();
        int fr = ex_total - ex_done < 128 ? (int)(ex_total - ex_done) : 128;
        for (int i = 0; i < 2 * fr; i++) if (fabsf(buf[i]) > ex_peak) ex_peak = fabsf(buf[i]);
        if (fwrite(buf, 8, fr, ex_f) != (size_t)fr) { toast("DISK FULL?"); export_finish(0); return; }
        ex_done += fr;
    }
    if (ex_skip <= 0 && ex_done >= ex_total) export_finish(1);
}
static void export_key(int k, int down) {
    if (ex_run) { if (down && (k == K_Y || k == K_OPT)) export_finish(0); return; }   // rendering: only cancel
    if (!down) { if (k == K_EDIT && !used[K_EDIT] && ex_row == EX_GO) export_start(); return; }
    if (k == K_Y || k == K_OPT) { exporting = 0; return; }
    if (k == K_UP) ex_row = (ex_row + EX_ROWS - 1) % EX_ROWS; else if (k == K_DOWN) ex_row = (ex_row + 1) % EX_ROWS;
    else if (k == K_LEFT || k == K_RIGHT) { int d = k == K_RIGHT ? 1 : -1; if (held[K_EDIT]) used[K_EDIT] = 1;
        switch (ex_row) {
        case EX_BARS: ex_bars = clampi(ex_bars + d, 0, 6); break; case EX_FROM: ex_from = clampi(ex_from + d, 1, 128); break;
        case EX_CLIPS: ex_all = !ex_all; break; case EX_TAIL: ex_tail = clampi(ex_tail + d, 0, 3); break;
        case EX_STEMS: ex_what = (ex_what + d + 4) % 4; break; case EX_SCN: ex_scnall = !ex_scnall; break;
        case EX_LOOP: ex_loop = !ex_loop; break; case EX_NORM: ex_norm = !ex_norm; break;
        case EX_FMT: ex_fmt = (ex_fmt + d + 3) % 3; break; case EX_MONO: ex_mono = !ex_mono; break; } }
}
#include "windows.h"
static void lcd_mask(void) {             // a dark gap after every 3 px, both ways, as on the DMG screen: one texture, made once
    static SDL_Texture *m; static int failed;
    if (!m && !failed) {
        Uint32 *px = malloc((size_t)W * H * 4); if (!px) { failed = 1; return; }
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) px[y * W + x] = (x % 4 == 3 || y % 4 == 3) ? 0x38000000u : 0;   // ARGB: black at 22 %
        m = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, W, H);
        if (m) { SDL_UpdateTexture(m, NULL, px, W * 4); SDL_SetTextureBlendMode(m, SDL_BLENDMODE_BLEND); } else failed = 1;
        free(px);
    }
    if (m) SDL_RenderCopy(R, m, NULL, NULL);
}
static void draw(void) {
    Uint64 t_draw0 = SDL_GetPerformanceCounter();
    SDL_SetRenderDrawColor(R, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(R);
    uf_name[0] = 0;                      // the pages tell ui_popup their card
    if (ui_simple && !exporting && !naming && !asking) { draw_simple(); goto shown; }
    v2_normalize(); side_area();
    // PAD STRIP BIG: the page is drawn into a texture, then set a little smaller under the big strip; the footer stays 1:1
    static SDL_Texture *body; int big = ui_strip == STRIP_BIG && strip_on(), secs = sec_on();   // v3: the sections push the page down the same way
    int bigs = big; big = big || secs;
    if (big && !body) { body = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H); if (!body) logmsg("strip: no render target, BIG drawn as THIN"); }
    if (big && body && !SDL_SetRenderTarget(R, body)) { SDL_SetRenderDrawColor(R, C_BG.r, C_BG.g, C_BG.b, 255); SDL_RenderClear(R); } else big = 0;
    if (bigs) { ui_strip = STRIP_THIN; ui2_header(); ui_strip = STRIP_BIG; } else ui2_header();   // the page's own header: cut off below
    cur_dim = qb_focus || qd_open;        // 2026-11-25 QA: the bar has the cursor; the page's white outline goes dim (one cursor)
    if (exporting) draw_export2();
    else if (naming) draw_naming2();
    else if (browsing) { if (bmode) draw_prjbrowse2(); else draw_browse2(); }
    else if (slicing) draw_slice();
    else if (randing) draw_rnd2();
    else if (prbing) draw_prb();
    else if (chording) draw_chords();
    else if (rolling) draw_roll();
    else if (picking) draw_pick();
    else if (wtbing) draw_wtb();
    else if (presetting) draw_presets2();
    else if (theming) draw_theme2();
    else if (iconing) draw_icons2();
    else if (ducking) draw_duck2();
    else if (scening) draw_scenes();
    else if (v2_steps()) draw_steps2();
    else if (v2_sound()) draw_sound2();
    else if (v2_fx()) draw_fx2();
    else if (v2_lfo()) draw_lfo2();
    else if (v2_auto()) draw_auto2();
    else if (v2_mix()) draw_mix2();
    else if (v2_unit()) draw_unit2();
    else if (v2_mixer()) draw_mixer2();
    else switch (tab) {
    case T_GRID: draw_grid(); break; case T_PAT: draw_pat(); break; case T_PRJ: draw_prj2(); break;
    default: draw_grid(); break;      // not reached: v2_normalize gives every state a screen
    }
    side_draw();                          // the slots beside the page (side.h), scaled with it
    cur_dim = 0;
    if (big && tab_focus) tabs_frame();   // on the page, before it is set under the strips
    if (big) { SDL_SetRenderTarget(R, NULL);
        int SY = strip_on() ? 69 : V2_HEAD, sy = bigs ? 122 : SY + 4, DY = secs ? sy + 36 : sy, sh = V2_FOOT - SY, dh = V2_FOOT - DY, dw = W * dh / sh;
        rect(0, SY, W, DY - SY, C_BG, 1);
        SDL_Rect s1 = {0, SY, W, sh}, d1 = {(W - dw) / 2, DY, dw, dh}, s2 = {0, V2_FOOT, W, H - V2_FOOT};
        SDL_RenderCopy(R, body, &s1, &d1); SDL_RenderCopy(R, body, &s2, &s2);
        if (bigs) ui2_header(); else { int st = ui_strip; if (strip_on()) ui_strip = STRIP_THIN; ui2_header(); ui_strip = st; }
        if (secs) ui2_secs(sy); }
    else if (tab_focus) tabs_frame();
    qd_draw(); qp_draw();
    ui_popup();
    if (asking) draw_ask2();
    if (cfm) draw_cfm2();
shown:;
    ui_quit_hold(); adv_hold_draw();
    if (th_sel == 4) lcd_mask();           // GAMEBOY: the LCD's pixel grid over everything (user, 2026-11-14)
    Uint64 tp = SDL_GetPerformanceCounter(); draw_build_ms += (tp - t_draw0) * 1000.0 / SDL_GetPerformanceFrequency();
    SDL_RenderPresent(R);
    draw_present_ms += (SDL_GetPerformanceCounter() - tp) * 1000.0 / SDL_GetPerformanceFrequency();
}

static void screenshot(const char *path) {
    SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(R, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
    SDL_SaveBMP(sf, path); SDL_FreeSurface(sf);
}

// MIDI in (midi.h reads the port): notes play like KEYS (the open pad, at their pitch) or PADS (36..47 = pads 1-12),
// with their velocity, into takes and capture like the Brick's own keys. Tags 700+note (KEYS) and 100+pad (PADS).
static void midi_follow(void) {          // CLOCK IN: the tempo from the clocks, and the phase pulled to theirs
    if (!midi_clk_in || !mclk_per_ns) return;
    int64_t now = mono_ns(), per = mclk_per_ns, last = mclk_last_ns;
    if (now - last > 500000000) return;  // no clock for half a second: stopped or unplugged, keep the tempo
    double bpm = 60e9 / (per * 24.0);
    if (bpm < 40 || bpm > 300) return;
    int bt = (int)(bpm + 0.5); if (bt != song.tempo && abs((int)(bpm * 10) - song.tempo * 10) > 6) song.tempo = (int16_t)bt;   // whole BPM in the project; the rest in tempo_mul
    double mul = bpm / song.tempo;
    if (engine_playing() && mclk_count > 0) {   // where they are, in 1/32 (3 clocks), against where we are
        double ext = (mclk_count - 1) / 3.0 + (double)(now - last) / per / 3.0, err = ext - engine_posf();
        if (fabs(err) < 4) mul *= 1 + (err > 0.5 ? 0.5 : err < -0.5 ? -0.5 : err) * 0.04;   // up to 2 % faster or slower until they meet
    }
    tempo_mul = (float)mul;
}
static void midi_poll(Uint32 now) {
    static Uint32 retry;
    midi_follow();
    if (midi_want[0] && (midi_fd < 0 || !midi_run) && ms_since(now, retry) > 2000) { retry = now | 1; midi_open(); }   // plugged in later, or again
    while (midi_rd != midi_wr) {
        unsigned r = midi_rd; uint8_t st = midi_ring[r & 255][0], n = midi_ring[r & 255][1], v = midi_ring[r & 255][2];
        __atomic_store_n(&midi_rd, r + 1, __ATOMIC_RELEASE);
        if (st == 0xFA || st == 0xFB || st == 0xFC) {   // CLOCK IN: the other device starts and stops us
            if (!midi_clk_in) continue;
            int run = engine_playing() || engine_counting() >= 0;
            if (st == 0xFC) { if (run) transport_toggle(); continue; }
            if (run && st == 0xFA) transport_toggle();
            if (!engine_playing()) transport_toggle();
            continue; }
        int ty = st & 0xF0, ch = (st & 15) + 1, on = ty == 0x90 && v > 0;
        if (ty == 0xB0) {                // a knob: MIDI LEARN takes it, or it turns what it was bound to
            if (mlearn_par >= 0) { int i = mmap_find(mlearn_pad, mlearn_par); if (i < 0 && mmap_n < MMAP_N) i = mmap_n++;
                if (i >= 0) { for (int j = 0; j < mmap_n; j++) if (j != i && mmap_[j].ch == ch && mmap_[j].cc == n) { mmap_[j] = mmap_[--mmap_n]; if (i == mmap_n) i = j; break; }   // one knob, one parameter
                    mmap_[i] = (MidiMap){(uint8_t)ch, n, (uint8_t)mlearn_pad, (int16_t)mlearn_par}; mmap_save();
                    char b[48]; snprintf(b, sizeof b, "CC %d -> %s", n, par_name[mlearn_par]); toast(b); }
                mlearn_par = -1; continue; }
            for (int i = 0; i < mmap_n; i++) if (mmap_[i].cc == n && mmap_[i].ch == ch) {
                int pad = mmap_[i].pad, pr = mmap_[i].par, lo, hi, st2, bg; par_limits(pad, pr, &lo, &hi, &st2, &bg);
                int val = lo + (int)((hi - lo) * v / 127.0 + 0.5); if (val != par_get(pad, pr)) { edit_begin(); par_put(pad, pr, val); } }
            continue; }
        if ((ty != 0x80 && ty != 0x90) || (midi_ch && ch != midi_ch)) continue;   // CC comes with the second step
        last_input = now | 1;
        if (midi_notes) {                // PADS
            if (n < 36 || n > 47) continue; int i = n - 36;
            if (!on) { engine_release_key(100 + i); rec_release(100 + i); continue; }
            if (is_synth(i)) engine_trig_key(i, v / 127.f, 0, -1, 100 + i); else engine_trig(i, v / 127.f, 0, -1);
            flash[i] = now; rec_tag_now = 100 + i; rec_hit(i, -1, NOTE_NONE); rec_tag_now = 0;
        } else {                         // KEYS: the open pad
            int tag = 700 + n;
            if (!on) { engine_release_key(tag); rec_release(tag); continue; }
            int ns = track_nslices(sel);
            if (ns) { int sl = (n - 36 + 120) % ns; engine_trig(sel, v / 127.f, 0, sl); rec_hit(sel, sl, NOTE_NONE); }
            else { if (is_synth(sel) || cur_pat(sel)->mode) engine_trig_key(sel, v / 127.f, (float)note_semis(sel, n), -1, tag); else engine_trig(sel, v / 127.f, (float)note_semis(sel, n), -1);   // a sample: a one-shot, as KEYS
                   rec_tag_now = tag; rec_hit(sel, -1, n); rec_tag_now = 0; }
            flash[sel] = now;
        }
    }
}
// A crash leaves its trace (2026-11-19: the Brick went down once and the log said nothing): the signal and the call
// stack go into brickpad.log at once; the next start keeps that log as brickpad.log.1.
#include <execinfo.h>
#include <signal.h>
static void crash_handler(int sig) {
    char b[64]; int n = snprintf(b, sizeof b, "CRASH: signal %d (%s)\n", sig, sig == SIGSEGV ? "SEGV" : sig == SIGBUS ? "BUS" : sig == SIGABRT ? "ABORT" : sig == SIGFPE ? "FPE" : "ILL");
    if (vib_fd >= 0 && write(vib_fd, "0", 1) < 0) {}   // the motor never stays on
    int fd = logf ? fileno(logf) : 2; if (write(fd, b, (size_t)n) < 0) {}
    void *bt[32]; int k = backtrace(bt, 32); backtrace_symbols_fd(bt, k, fd); fsync(fd);
    signal(sig, SIG_DFL); raise(sig);
}
int main(int argc, char **argv) {
    srand((unsigned)time(NULL));         // figures, random files and kits differ from run to run
    rename("brickpad.log", "brickpad.log.1");   // the last run's log, kept (a crash or a freeze is read there)
    logf = fopen("brickpad.log", "w"); if (!logf) logf = stderr;
    { time_t t = time(NULL); char d[32]; strftime(d, sizeof d, "%F %T", localtime(&t)); logmsg("start %s", d); }
    { int sg[5] = {SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL}; for (int i = 0; i < 5; i++) signal(sg[i], crash_handler); }
    int test_secs = 0, buffer = 128;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--test") && i + 1 < argc) test_secs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--buffer") && i + 1 < argc) buffer = atoi(argv[++i]);
    }
    for (int i = 1; i + 1 < argc; i++) if (!strcmp(argv[i], "--analyze")) {   // print what the loader detects
        static Entry e[2048]; int n = list_dir(argv[i + 1], e, 2048);
        for (int k = 0; k < n; k++) {
            if (e[k].dir) continue;
            char pth[1024]; snprintf(pth, sizeof pth, "%s/%s", argv[i + 1], e[k].name);
            Sample *x = sample_load(pth);
            if (!x) { printf("BROKEN %s\n", e[k].name); continue; }
            printf("%-40.40s %5.2fs root %-2s%s %-5s %.2f %s%s bpm %5.1f bars %d onsets %d\n", x->name, x->n / (float)SR,
                   x->root >= 0 ? note_name[x->root] : "-", x->mode == 2 ? "m" : x->mode == 1 ? "M" : " ",
                   x->root_by == 1 ? "NAME" : x->root_by == 2 ? "SOUND" : "", x->root_by == 2 ? key_last_corr : 0, x->drum ? "DRUM " : "", x->loop ? "LOOP" : "", x->bpm, x->bars, x->nons);
            sample_free(x);
        }
        return 0;
    }
    char *loop_arg = NULL;
    for (int i = 1; i + 1 < argc; i++) if (!strcmp(argv[i], "--loop")) loop_arg = argv[i + 1];
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) { logmsg("SDL: %s", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("BrickPad", 0, 0, W, H, SDL_WINDOW_FULLSCREEN);
    // Every presented frame costs ~6 ms of CPU in this GPU driver, and with vsync it waits another ~9 ms spinning:
    // 60 fps with vsync took ~43 % of a core while drawing took 1.5 ms. So no vsync, and the frame rate follows
    // what happens (frame_rate): 60 right after a key, 30 while sound plays, 10 at rest. A file "bp_fps" with a
    // number beside the binary forces a rate (for measuring).
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
    int pace = 0; { FILE *pf = fopen("bp_fps", "r"); if (pf) { if (fscanf(pf, "%d", &pace) != 1) pace = 0; fclose(pf); } }
    R = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_ShowCursor(0);
    { SDL_RendererInfo ri; if (!SDL_GetRendererInfo(R, &ri)) logmsg("render: %s%s%s", ri.name, ri.flags & SDL_RENDERER_ACCELERATED ? " accelerated" : " software", ri.flags & SDL_RENDERER_PRESENTVSYNC ? " vsync" : ""); }
    font_init();
    SDL_Joystick *js = SDL_NumJoysticks() > 0 ? SDL_JoystickOpen(0) : NULL;
    logmsg("joystick: %s", js ? SDL_JoystickName(js) : "none");
    engine_wave_init(); wt_boot();       // WAVE: the built-in tables, then the user's (wavetables/slots.cfg), before the device
    if (engine_start(buffer)) { logmsg("audio: %s", SDL_GetError()); return 1; }
    logmsg("audio: %s", SDL_GetCurrentAudioDriver());
    idx_load();
    load_state(); scenes_read(SCN_FILE, 0); prj_startup();
    theme_load(); presets_load(); hint_load(); trim_load(); metro_load(); browse_listen_load(); ui_cfg_load(); midi_cfg_load(); mmap_load(); midi_open();
    vib_th = SDL_CreateThread(vib_main, "vibration", NULL);
    if (test_secs) {  // demo beat for automated checks
        song.tempo = 128; song.swing = 20;
        for (int i = 0; i < 16; i++) {
            song.t[0].pat[0].trig[i] = i % 4 == 0; song.t[5].pat[0].trig[i] = i % 2 == 0;
            song.t[2].pat[0].trig[i] = i % 8 == 4; song.t[9].pat[0].trig[i] = i % 3 == 0;
            song.t[9].pat[0].note[i] = (int8_t)deg_midi(9, (i * 3) % 8);
        }
        song.t[9].pat[0].len = 12;
        if (loop_arg) { set_slot_file(11, loop_arg); auto_setup(11); song.t[11].warp = 3; song.tempo = 100; }
        transport_toggle(); sel = loop_arg ? 11 : 9; lane = loop_arg ? L_SLICE : L_NOTE; tab = T_SEQ; srow = 4;
    }
    Uint32 start = SDL_GetTicks(), frames = 0, last_save = start; int hat = 0, run = 1;
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: run = 0; break;
            case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP: last_input = SDL_GetTicks() | 1;
                key_event(map_button(e.jbutton.button), e.type == SDL_JOYBUTTONDOWN); break;
            case SDL_JOYHATMOTION: { last_input = SDL_GetTicks() | 1;
                static const int bit[4] = {SDL_HAT_UP, SDL_HAT_DOWN, SDL_HAT_LEFT, SDL_HAT_RIGHT};
                for (int d = 0; d < 4; d++)
                    if ((e.jhat.value ^ hat) & bit[d]) {
                        int dn = !!(e.jhat.value & bit[d]);
                        input_arrow(d, dn, SDL_GetTicks());
                        key_event(K_UP + d, dn);
                    }
                hat = e.jhat.value; break;
            }
            }
        }
        Uint32 now = SDL_GetTicks();
        for (int d = 0; d < 4; d++)   // arrow auto-repeat (not in PERFORM: arrows are pads there)
            if (!perform && input_repeat(d, now)) key_event1(K_UP + d, 1);   // a repeat never leaves the page for the quick bar
        if (quit_down && now - quit_down > 3000) run = 0;
        combo_tick(now);
        browse_tick(now);
        metro.rec = (int8_t)(rec || rec_wait); scene_poll(); launch_tick(); rec_tick(); export_tick(); frz_tick(); midi_poll(now);
        { int stuck = engine_watchdog(now); if (stuck) logmsg("audio: a callback hung %d ms, real-time priority dropped", stuck); }
        if (!test_secs) saver_pause(engine_playing() || engine_counting() >= 0);
        if (!test_secs && now - last_save > 10000) { save_state(); theme_save(); if (idx_dirty && lib_state != 1) idx_save(); last_save = now; }
        if (test_secs) {
            if (now - start > (Uint32)test_secs * 1000 - 3000 && tab == T_SEQ) { screenshot("/tmp/bp1.bmp"); tab = T_GRID; }
            if (now - start > (Uint32)test_secs * 1000) run = 0;
        }
        { static Uint32 st0; static int nf;
          draw(); nf++;
          if (!st0) st0 = now;
          if (!test_secs && ms_since(now, st0) >= 30000) {   // a line every 30 s: frames per second and the cost of a frame
              { float amx, agap; int aov, an; engine_stats(&amx, &aov, &agap, &an);
              logmsg("frames: %.1f fps, build %.2f ms, present %.2f ms, tab %d%s | audio: load avg %d%% max %d%%, %d of %d calls over time, longest wait %.1f ms, voices %d | free %ld MB", nf * 1000.0 / ms_since(now, st0), draw_build_ms / nf, draw_present_ms / nf, tab, is_synth(sel) && tab == T_SEQ ? " synth" : "", (int)(engine_load() * 100), (int)(amx * 100), aov, an, agap, engine_voices(), mem_avail_mb()); }
              st0 = now; nf = 0; draw_build_ms = draw_present_ms = 0; } }
        engine_collect_garbage();
        { static Uint64 next; int fps = pace ? pace : frame_rate(now);
          Uint64 fq = SDL_GetPerformanceFrequency(), nw = SDL_GetPerformanceCounter(), per = fq / (fps > 0 ? fps : 60);
          if (!next || nw > next + per) next = nw;
          next += per;
          while (next > nw) {               // sleep in short bits: a key wakes the loop within 5 ms
              Uint32 ms = (Uint32)((next - nw) * 1000 / fq); if (ms > 5) ms = 5;
              if (SDL_HasEvents(SDL_FIRSTEVENT, SDL_LASTEVENT) && fps < 60) { next = nw; break; }
              SDL_Delay(ms ? ms : 1); nw = SDL_GetPerformanceCounter();
              if (midi_wr != midi_rd) midi_poll(SDL_GetTicks());   // a MIDI note does not wait for the frame
          } }
        frames++;
    }
    if (test_secs) {
        float secs = (SDL_GetTicks() - start) / 1000.f;
        printf("fps %.1f, dsp load %.1f%%, buffer %d\n", frames / secs, engine_load() * 100, engine_frames());
        screenshot("/tmp/bp2.bmp");
    } else { save_state(); theme_save(); saver_pause(0); }
    vib_quit = 1; SDL_WaitThread(vib_th, NULL);
    midi_close(); logmsg("quit");
    lib_end();
    if (idx_dirty) idx_save();
    engine_stop();
    SDL_Quit();
    return 0;
}
