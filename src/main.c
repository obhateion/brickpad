// BrickPad — UI with tabs (F1/F2), M8-style controls:
// arrows cursor · EDIT (B) act, EDIT+arrows value · OPTION (A) listen, OPTION+EDIT delete,
// OPTION+<> slot · patterns on the pattern field of SEQ+INST · SHIFT+OPTION copy, SHIFT+EDIT paste · PLAY transport.
#include "bp.h"
#include "synth.h"
#include "acid.h"
#include "cloud.h"
#include "fx.h"
#include "presets.h"
_Static_assert(sizeof(Song) == 301612 + NSLOT * NPAT * 6 + NSLOT * NLFO * 8 + NSLOT * NPAT * NAUTO * (2 + NSTEP) + NSLOT * 16 + NSLOT * (1 + 32 + 16) + NSLOT * NPAT * NAUTO * NSTEP + NSLOT * NINS * 6 + NSLOT * 16 + NSLOT * 8 + NSLOT * 8 + NSLOT * 16, "Song changed size: bump STATE_MAGIC and convert the previous format in load_state");
static int len_lo(void) { return 1; }      // MIN/MAX LENGTH are gone (2026-11-09): a clip may be 1..NSTEP steps
static int len_hi(void) { return NSTEP; }
#include <SDL2/SDL.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
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
    {RGB(0x0B0C10), RGB(0x161820), RGB(0x2A2E3A), RGB(0x3D4250), RGB(0x626877), RGB(0xE8E8E8), RGB(0xFFD23F), RGB(0xFF5A5A), RGB(0x23263A), RGB(0x7CFF6B),
     RGB(0x3FE0D0), RGB(0xFF4FA3), RGB(0xFFD23F), RGB(0x5AA0FF), RGB(0x7CFF6B), RGB(0xFFD23F), RGB(0xFF4FA3), RGB(0x5AA0FF), RGB(0xFF8A3D)},   // COSMIC: the mockups
    {RGB(0x000802), RGB(0x041408), RGB(0x0A3014), RGB(0x145024), RGB(0x288C46), RGB(0xAAFFB4), RGB(0x5AFF78), RGB(0xE6FFE6), RGB(0x082C12), RGB(0x78FF96),
     RGB(0x3CDC6E), RGB(0x8CFFAA), RGB(0x64F082), RGB(0x28B45A), RGB(0x78FF96), RGB(0x5AFF78), RGB(0x8CFFAA), RGB(0x28B45A), RGB(0x64F082)},   // MATRIX: green monochrome
    {RGB(0x1A1A1B), RGB(0x262628), RGB(0x3C3C3F), RGB(0x58585C), RGB(0x8C8C90), RGB(0xDCDCDE), RGB(0xDC3238), RGB(0xF02828), RGB(0x3C1418), RGB(0xC8C8CC),
     RGB(0xB4B4B8), RGB(0x96141E), RGB(0xD23C3C), RGB(0x6E6E74), RGB(0xC8C8CC), RGB(0xDC3238), RGB(0x96141E), RGB(0x6E6E74), RGB(0xD23C3C)},   // BLOOD CONCRETE: grey and dark red
    {RGB(0x0E0800), RGB(0x1E1204), RGB(0x402608), RGB(0x643C0C), RGB(0xAA6414), RGB(0xFFBE5A), RGB(0xFF9614), RGB(0xFFEBC8), RGB(0x3C2004), RGB(0xFFC83C),
     RGB(0xFFAA28), RGB(0xFF780A), RGB(0xFFD26E), RGB(0xC86E14), RGB(0xFFC83C), RGB(0xFF9614), RGB(0xFF780A), RGB(0xC86E14), RGB(0xFFD26E)},   // AMBER: orange monochrome
    {RGB(0x9BBC0F), RGB(0x8BAC0F), RGB(0x6E8C14), RGB(0x5A781E), RGB(0x306230), RGB(0x0F380F), RGB(0x1E501E), RGB(0x0F380F), RGB(0x7A9614), RGB(0x306230),
     RGB(0x0F380F), RGB(0x306230), RGB(0x1E501E), RGB(0x466E28), RGB(0x306230), RGB(0x1E501E), RGB(0x306230), RGB(0x466E28), RGB(0x1E501E)},   // GAMEBOY: light olive
};
static Col th_cur[TC_N] = {RGB(0x0B0C10), RGB(0x161820), RGB(0x2A2E3A), RGB(0x3D4250), RGB(0x626877), RGB(0xE8E8E8), RGB(0xFFD23F), RGB(0xFF5A5A), RGB(0x23263A), RGB(0x7CFF6B),
     RGB(0x3FE0D0), RGB(0xFF4FA3), RGB(0xFFD23F), RGB(0x5AA0FF), RGB(0x7CFF6B), RGB(0xFFD23F), RGB(0xFF4FA3), RGB(0x5AA0FF), RGB(0xFF8A3D)};
#define C_ENG(t) th_cur[TC_SMP + ((t) & 3)]   // an engine by Track.type: SAMPLER GRAND ACID CLOUD
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
// GRAND's panel keeps the colours of the original's sections in every theme (pale blue oscillators,
// green filter, the rest black): fixed on purpose, the only colours outside the theme. Ink is their text.
static const Col GB_OSC = {150, 198, 228}, GB_FILTER = {72, 158, 92}, GB_INK = {16, 16, 16},
    GB_BLACK = {24, 24, 24}, GB_LEGEND = {232, 230, 222}, GB_KNOB = {30, 30, 30}, GB_RIM = {84, 84, 84},
    GB_LED = {235, 45, 30}, GB_LCD = {8, 10, 8},
    CL_PANEL = {26, 30, 40}, CL_HEAD = {44, 50, 66}, CL_INK = {176, 214, 228},   // CLOUD: night slate, pale sky letters
    AB_PANEL = {196, 198, 201}, AB_HEAD = {164, 166, 170}, AB_INK = {20, 20, 22}, AB_GLOW = {255, 150, 40},   // ACID: brushed aluminium, a warm lamp
    SM_PANEL = {44, 46, 49}, SM_HEAD = {22, 23, 25}, SM_EDGE = {70, 73, 77}, SM_INK = {226, 228, 224},   // the sampler: rack graphite,
    SM_LCD = {6, 20, 11}, SM_GLOW = {96, 255, 146}, SM_DIM = {34, 92, 56};                                 // a green glowing screen
static Col mix(Col a, Col b, int t) {    // t 0..255: a to b
    return (Col){(Uint8)(a.r + (b.r - a.r) * t / 255), (Uint8)(a.g + (b.g - a.g) * t / 255), (Uint8)(a.b + (b.b - a.b) * t / 255)};
}

enum { T_GRID, T_SEQ, T_PAT, T_FX, T_MIX, T_PRJ, T_N };
static const char *tab_name[T_N] = {"PADS", "INSTRUMENT", "PATTERNS", "FX", "MIXER", "PROJECT"};

static SDL_Renderer *R;
static FILE *logf;
static int koct, key_on[NSLOT];
static int rec, prj_back = T_GRID;
static Uint32 combo_t; static double combo_pos;   // MENU+PLAY pressed then, waiting for a second PLAY
#define COMBO_MS 300
#define X_TWICE_MS 350               // two X taps this close double the clip
static Uint32 x_tap_t;                // the first of them, 0 = none
enum { ICON_COLS = 12 };
static int iconing, ic_cur, fig_open;
static int ducking;                      // the DUCK window is open (below, with the mixer)
static int tap_bpm(Uint32 *t, int *n);
static void naming_pad(void);
static int tab = T_GRID, browsing, slicing, theming, perform, sel = 5, undo_armed;
static int hubbing, hub_cur, ui_cat = -1;   // UI v2: the pad hub is on screen; the category opened from it (-1 none), ui.h
enum { PC_PATTERN, PC_SOUND, PC_FX, PC_MOD, PC_MIX, PC_N };   // the categories inside a pad
static int scening;                      // HOME > SCENES (A02) is on screen, over the pads
static int modmenu, mm_cur;              // MOD's own list (LFO, AUTOMATION, DUCK), as PATTERN's
static int patmenu, pm_cur;              // PATTERN's own list (STEPS or ROLL, SELECT, RECORD) is on screen; its cursor, kept
static int recpage;                      // PATTERN > RECORD is on screen
static int v2_unit(void), v2_mix(void), v2_mixer(void), v2_fx(void), v2_lfo(void), v2_auto(void), mixer2_tap(void); static void mixer2_arrow(int k);   // UI v2 step 5 pages (pages.h)
static int v2_steps(void);               // the rebuilt STEPS page is the screen now (below, before seq_clamp)
static Uint32 menu_down, flash[NSLOT];
static char msg[80]; static Uint32 msg_t;

static void logmsg(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(logf, fmt, ap); va_end(ap); fputc('\n', logf); fflush(logf);
}
// A time mark is SDL_GetTicks() | 1, so it is never 0 but may be 1 ms ahead of "now": compare only through this.
static int ms_since(Uint32 now, Uint32 mark) { return (Sint32)(now - mark); }
static void toast(const char *s) { snprintf(msg, sizeof msg, "%s", s); msg_t = SDL_GetTicks(); }
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
    for (int f = 0; f < FT_N; f++) {     // UI v2 fonts: 101 glyphs in a row
        const FontV2 *v = &fv2[f]; int w = 101 * v->w; Uint32 *px = calloc((size_t)w * v->h, 4);
        for (int g = 0; g < 101; g++) for (int i = 0; i < v->w * v->h; i++) {
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
static void textf(int x, int y, int sc, Col c, const char *fmt, ...) {
    char b[160]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); text(x, y, sc, c, b);
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
// technical GUI details: static, only lines
static void ticks(int x, int y, int w, int h, int l, Col c) {   // corner brackets
    rect(x, y, l, 1, c, 1); rect(x, y, 1, l, c, 1); rect(x + w - l, y, l, 1, c, 1); rect(x + w - 1, y, 1, l, c, 1);
    rect(x, y + h - 1, l, 1, c, 1); rect(x, y + h - l, 1, l, c, 1); rect(x + w - l, y + h - 1, l, 1, c, 1); rect(x + w - 1, y + h - l, 1, l, c, 1);
}
static void plus(int x, int y, int r, Col c) { rect(x - r, y, 2 * r + 1, 1, c, 1); rect(x, y - r, 1, 2 * r + 1, c, 1); }
static void ruler(int x, int y, int w, int step, int major, Col c) {   // scale marks under a line
    for (int i = 0, k = 0; i <= w; i += step, k++) rect(x + i, y, 1, k % major ? 3 : 7, c, 1);
}
static void dotfield(int x, int y, int w, int h, int step, Col c) {
    static SDL_Rect d[2048]; int n = 0;
    for (int b = 0; b <= h - 2; b += step) for (int a = 0; a <= w - 2 && n < 2048; a += step) d[n++] = (SDL_Rect){x + a, y + b, 2, 2};
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255); SDL_RenderFillRects(R, d, n);
}
#include "figures.h"
enum { CAT_OTHER, CAT_KICK, CAT_SNARE, CAT_CLAP, CAT_HAT, CAT_OHAT, CAT_CYM, CAT_TOM, CAT_PERC, CAT_LOOP, CAT_BASS, CAT_MEL, CAT_VOX, CAT_FX, CAT_N };
#include "icons.h"

// ---------- song helpers ----------
#define UNDO_N 10
static Song undo_buf[UNDO_N]; static int undo_n, undo_top;
static void push_undo(void) { undo_buf[undo_top] = song; undo_top = (undo_top + 1) % UNDO_N; if (undo_n < UNDO_N) undo_n++; }
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
    for (int s = 0; s < NSLOT; s++) { song.t[s].on = cur.t[s].on; song.t[s].queued = cur.t[s].queued; }
    sync_samples(); toast("UNDO");
}
static Pattern *cur_pat(int s) { return &song.t[s].pat[song.t[s].cur]; }
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
static void save_state(void) {
    static Song saved;                   // the file is big now: write only when something changed
    if (!memcmp(&saved, &song, sizeof song)) return;
    if (write_whole(STATE, &song, sizeof song)) saved = song;
}
// State format 0xB2B20005: patterns without mode and roll. Everything else has the same layout.
typedef struct { int8_t trig[NSTEP], note[NSTEP], vel[NSTEP], prob[NSTEP], rep[NSTEP], slice[NSTEP]; uint8_t len, div; } OldPattern;
typedef struct { OldPattern pat[NPAT]; char rest[sizeof(Track) - offsetof(Track, cur)]; } OldTrack;
typedef struct { char head[offsetof(Song, t)]; OldTrack t[NSLOT]; char tail[sizeof(Song) - offsetof(Song, cut)]; } OldSong;
static void convert_old(const OldSong *o, size_t got) {
    song_defaults();
    memcpy(&song, o->head, sizeof o->head);
    song.magic = STATE_MAGIC;
    for (int s = 0; s < NSLOT; s++) {
        memcpy(&song.t[s].cur, o->t[s].rest, sizeof o->t[s].rest);
        for (int q = 0; q < NPAT; q++) memcpy(&song.t[s].pat[q], &o->t[s].pat[q], sizeof(OldPattern));   // mode STEPS, empty roll
    }
    size_t tail = got > offsetof(OldSong, tail) ? got - offsetof(OldSong, tail) : 0;
    if (tail > sizeof o->tail) tail = sizeof o->tail;
    memcpy(song.cut, o->tail, tail);
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
        if (t->type == 1 && t->sp[SP_MARK] != SP_INIT) synth_defaults(t->sp);
        if (t->type == 2 && t->ap[AP_MARK] != AP_INIT) acid_defaults(t->ap);
        if (t->type == 3 && t->cp[CP_MARK] != CP_INIT) cl_defaults(t->cp);
        if (song.cx[s][CX_MARK] != CX_INIT) cx_defaults(song.cx[s]);
        if (song.ax[s][AX_MARK] != AX_INIT) ax_defaults(song.ax[s]);
        if (song.gx[s][GX_MARK] != GX_INIT) gx_defaults(song.gx[s]);
        for (int k = 0; k < GX_N; k++) { int lo, hi; gx_range(k, &lo, &hi); KEEP(song.gx[s][k], lo, hi); }
        for (int c = 0; c < NCABLE; c++) if (song.gp[s][c][0] >= JO_N || song.gp[s][c][1] >= JI_N) song.gp[s][c][0] = song.gp[s][c][1] = 0;
        KEEP(song.lnote[s], -48, 48);
        for (int k = 0; k < AX_N; k++) { int lo, hi; ax_range(k, &lo, &hi); KEEP(song.ax[s][k], lo, hi); }
        if (s == 0) mix_sanitize(&song.mix);
        for (int q = 0; q < NPAT; q++) { uint8_t *fo = song.fol[s][q]; if (fo[FO_AFTER] > 64) fo[FO_AFTER] = 64; if (fo[FO_A] >= FA_N) fo[FO_A] = 0; if (fo[FO_B] >= FA_N) fo[FO_B] = 0;
            if (fo[FO_BCH] > 100) fo[FO_BCH] = 100; if (fo[FO_JUMP] >= NPAT) fo[FO_JUMP] = 0; fo[FO_SPARE] = 0; }
        for (int j = 0; j < NLFO; j++) { Lfo *L = &song.lfo[s][j];
            if (L->target >= LT_ENG + LT_ENGN || (L->target > LT_SENDB && L->target < LT_INS1) || (L->target >= LT_INS1 + 6 && L->target < LT_INS2) || (L->target >= LT_INS2 + 6 && L->target < LT_ENG)) L->target = 0;
            if (L->shape >= LS_N) L->shape = 0; L->sync = L->sync ? 1 : 0; if (L->sync ? L->rate >= LFO_NSYNC : L->rate > 100) L->rate = L->sync ? 8 : 50;
            KEEP(L->depth, -100, 100); if (L->retrig > NSLOT + 1) L->retrig = 0; if (L->phase > 100) L->phase = 0; L->spare = 0; }
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
        for (int k = 0; k < SP_N; k++) { int lo, hi; sp_range(k, &lo, &hi); KEEP(t->sp[k], lo, hi); } KEEP(t->bars, 0, 16);
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
            KEEP(p->mode, 0, 1); p->spare = 0;
            for (int i = 0; i < NNOTE; i++) {
                Note *n = &p->roll[i];
                if (n->len && n->start >= NSTEP * 32 * RT) n->len = 0;
                if (n->len > NSTEP * 32 * RT) n->len = NSTEP * 32 * RT;
                if (n->len) { KEEP(n->note, 0, 127); KEEP(n->vel, 1, 127); KEEP(n->prob, 0, 100); n->flags &= NF_ACC; }   // an empty slot is left as it is (it made a saved project differ after each load)
            }
            for (int i = 0; i < NSTEP; i++) {
                KEEP(p->trig[i], 0, 1); KEEP(p->vel[i], 0, 127); KEEP(p->prob[i], 0, 100); KEEP(p->rep[i], 1, 4); KEEP(p->slice[i], -1, 127);
            }
        }
    }
}
static void load_from(const char *path) {   // state.bin or a project file
    song_defaults();
    FILE *f = fopen(path, "rb");
    static Song tmp; memset(&tmp, 0, sizeof tmp);    // zero: fields a shorter file does not have
    size_t got = f ? fread(&tmp, 1, sizeof tmp, f) : 0;
    {   // a file from before the mixer (6) or before follow actions (7): the same layout, shorter; the rest starts at zero
        size_t end = tmp.magic == 0xB2B20006 ? offsetof(Song, mix) : tmp.magic == 0xB2B20007 ? offsetof(Song, fol) : tmp.magic == 0xB2B20008 ? offsetof(Song, lfo) : tmp.magic == 0xB2B20009 ? offsetof(Song, aut) : tmp.magic == 0xB2B2000A ? offsetof(Song, ax) : tmp.magic == 0xB2B2000B ? offsetof(Song, lnote) : tmp.magic == 0xB2B2000C ? offsetof(Song, acurve) : tmp.magic == 0xB2B2000D ? offsetof(Song, acurve) : tmp.magic == 0xB2B2000E ? offsetof(Song, pname) : 0;   // 0E: before pad names, SAMPLER DEEP and the DUCK window (only appended)
        if (end && got >= offsetof(Song, t) && got <= end) { notes_deg = tmp.magic <= 0xB2B2000C;   // files before 0x0D have degrees in their steps
            memset((char *)&tmp + end, 0, sizeof tmp - end);
            if (tmp.magic != 0xB2B2000E)     // (a 0E file has its bends; it only lacks what was appended, and stays where it is)
            for (int s2 = 0; s2 < NSLOT; s2++) for (int q = 0; q < NPAT; q++) for (int a = 0; a < NAUTO; a++)   // bends: JUMP lanes hold, GLIDE lanes are straight
                memset(tmp.acurve[s2][q][a], tmp.aut[s2][q][a].smooth ? 0 : 100, NSTEP);
            if (f) { fclose(f); f = NULL; } if (tmp.magic != 0xB2B2000E) { char vp[600]; snprintf(vp, sizeof vp, "%s.v%d", path, (int)(tmp.magic & 0xff)); rename(path, vp); } if (0) rename(STATE, tmp.magic == 0xB2B20006 ? STATE ".v6" : tmp.magic == 0xB2B20007 ? STATE ".v7" : tmp.magic == 0xB2B20008 ? STATE ".v8" : tmp.magic == 0xB2B20009 ? STATE ".v9" : tmp.magic == 0xB2B2000A ? STATE ".v10" : tmp.magic == 0xB2B2000B ? STATE ".v11" : tmp.magic == 0xB2B2000C ? STATE ".v12" : STATE ".v13");
            if (end <= offsetof(Song, aut)) for (int s2 = 0; s2 < NSLOT; s2++) for (int q = 0; q < NPAT; q++) for (int a = 0; a < NAUTO; a++) { tmp.aut[s2][q][a].target = tmp.aut[s2][q][a].smooth = 0; memset(tmp.aut[s2][q][a].v, AUTO_EMPTY, NSTEP); }   // empty lanes
            tmp.magic = STATE_MAGIC; got = sizeof tmp;   // whole now: mix_sanitize gives the mixer's defaults (mark 0)
        }
    }
    int old = got >= offsetof(OldSong, tail) && tmp.magic == 0xB2B20005;
    if (old) {
        convert_old((const OldSong *)&tmp, got);
        fclose(f); f = NULL;
        { char vp[600]; snprintf(vp, sizeof vp, "%s.v5", path); rename(path, vp); }   // kept, in case the conversion ever needs a second look
    }
    int short_file = !old && tmp.magic == song.magic && got < offsetof(Song, len_min);   // a file from before MIN/MAX LENGTH is whole
    if (f) { fclose(f); f = NULL; }
    if (old || (got >= offsetof(Song, t) && tmp.magic == song.magic)) {
        if (!old) song = tmp;
        song_sanitize();
        if (short_file && !strcmp(path, STATE)) keep_damaged();        // an older format or a cut file: load what is there, keep the original too
        if (short_file && got < offsetof(Song, cut)) toast("PROJECT FILE WAS CUT, COPY KEPT");
        logmsg("state loaded%s", short_file ? " (short)" : "");
    } else {
        if (got && strcmp(path, STATE)) toast("PROJECT FILE UNREADABLE");
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
    const Track *tr = &song.t[s];        // ROLL (user, 2026-11-08): the pattern is a roll already, a POLY GRAND, CLOUD, or notes overlap
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
// 2026-11-11 (user: no transport of its own, as in Ableton): the count goes on after the last clip stops, so the next
// clip falls into the same grid. Only to spare the battery it stops by itself after a minute with no clip, no sound
// and no key (the screen saver is held off while it runs).
static void launch_tick(void) {
    static Uint32 idle_t; Uint32 now = SDL_GetTicks();
    play_hold_tick();
    if (!engine_playing() || rec) { idle_t = 0; return; }
    for (int i = 0; i < NSLOT; i++) if (song.t[i].on || engine_want(i) == 1) { idle_t = 0; return; }
    if (engine_voices() > 0 || (last_input && ms_since(now, last_input) < 60000)) { idle_t = 0; return; }
    if (!idle_t) { idle_t = now | 1; return; }
    if (ms_since(now, idle_t) > 60000) { idle_t = 0; transport_toggle(); }
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
static int live_scene;
static void live_scene_go(int d) {
    int top = 0; for (int s = 0; s < NSLOT; s++) if (has_sound(s)) for (int q = NPAT - 1; q > top; q--) if (pat_used(&song.t[s].pat[q])) { top = q; break; }
    int n = clampi(live_scene + d, 0, top), run = engine_playing(), any = 0;
    live_scene = n;
    for (int s = 0; s < NSLOT; s++) { Track *t = &song.t[s]; if (!has_sound(s)) continue;
        if (pat_used(&t->pat[n])) {
            any = 1;
            if (run && t->on && engine_want(s) != 0) { if (t->cur != n) t->queued = (int8_t)n; }   // playing: the pattern changes on the quantum
            else { t->cur = (int8_t)n; t->queued = -1; if (run) { if (engine_want(s) == 0) engine_launch(s, -1); else if (!t->on && engine_want(s) != 1) engine_launch(s, 1); } else t->on = 1; }
        } else if (run) { if (engine_want(s) == 1) engine_launch(s, -1); else if (t->on && engine_want(s) != 0) engine_launch(s, 0); }
        else t->on = 0;
    }
    if (!run && any && engine_counting() < 0) engine_transport(1);
    char b[24]; snprintf(b, sizeof b, "SCENE %d", n + 1); toast(b);
}
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
    h = fnv(h, song.gx[s], sizeof song.gx[s]); h = fnv(h, song.gp[s], sizeof song.gp[s]); h = fnv(h, song.lfo[s], sizeof song.lfo[s]);
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
            if (f->type == FX_COMP && f->on && kp > 0 && kp <= NSLOT && kp - 1 != s) song.t[kp - 1].on = (int8_t)pat_used(cur_pat(kp - 1)); }
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
        play_down = 0;                       // no hold here: the tap already is the stop
        if (engine_playing() && !any) {      // the count runs on with no clip: every clip with notes joins on the quantum
            int n = 0; for (int s = 0; s < NSLOT; s++) if (has_sound(s) && pat_used(cur_pat(s))) { engine_launch(s, 1); n++; }
            toast(n ? "ALL CLIPS START" : "NO CLIPS TO PLAY"); return; }
        transport_toggle(); return;
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
        bcur = clampi(bcur + (k == K_DOWN ? 1 : -1), 0, bn - 1); browse_preview();
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
            if (tab == T_GRID && ui_cat < 0 && !perform) { hubbing = 1; hub_cur = PC_PATTERN; } }   // UI v2: a pad that just got its sound opens (its list)
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
static const char *lane_name[L_N] = {"NOTE", "NOT", "VEL", "PRB", "REP", "SLC", "ACC", "SLD", "A1", "A2", "A3", "A4"};   // short: PAGE, MODE and the lanes share the row
static int is_synth(int s);
static int lane_list(int *l) {           // the lanes this pad has: a sampler slices, a synth does not, ACID has ACCENT and SLIDE
    static const int smp_l[] = {L_TRIG, L_VEL, L_PROB, L_REP, L_SLICE}, gb_l[] = {L_TRIG, L_VEL, L_PROB, L_REP},   // TRIG holds the note too (2026-11-09)
                     ab_l[] = {L_TRIG, L_VEL, L_PROB, L_REP, L_ACC, L_SLD};
    static const int sls_l[] = {L_SLICE, L_TRIG, L_VEL, L_PROB, L_REP};   // a sliced sample: SLC first (2026-11-12, user)
    int ty = song.t[sel].type; const int *src = ty == 2 ? ab_l : ty ? gb_l : track_nslices(sel) > 1 ? sls_l : smp_l; int n = ty == 2 ? 6 : ty ? 4 : track_nslices(sel) > 1 ? 5 : 4;   // UI v2: no SLICE lane without slices
    if (song.t[sel].pat[song.t[sel].cur].mode) n = 1;   // ROLL: the roll itself (NOTE) and the automation lanes   // CLOUD: as GRAND
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
static int lane_ok(int ln) { int l[L_N], n = lane_list(l); for (int i = 0; i < n; i++) if (l[i] == ln) return 1; return 0; }
static const char *div_name[6] = {"1/32", "1/16", "1/8", "1/4", "1/2", "1/1"};
enum { P_SAMPLE, P_PITCH, P_FINE, P_VOL, P_PAN, P_START, P_END, P_DIR, P_VOICE, P_ROOT, P_SLICES, P_WARP, P_BARS, P_BPM, P_GRAIN, P_FLUX, P_TYPE,
       P_SRC, P_NORM, P_SX, P_SY = P_SX + SX_N, P_AC = P_SY + SP_N, P_CL = P_AC + AP_N, P_CX = P_CL + CP_N, P_AX = P_CX + CX_N, P_GX = P_AX + AX_N, P_PT = P_GX + GX_N, P_N = P_PT + 32 };   // P_PT + jack: PATCH (outputs 1.., inputs 16 + ..)   // P_GX + GX_*: GRAND DEEP (Song.gx)   // P_AX + AX_*: ACID DEEP (Song.ax)   // P_CX + CX_*: CLOUD DEEP (Song.cx)   // P_SRC: CLOUD's sample; P_CL + CP_*: CLOUD (Track.cp)   // P_SY + SP_*: GRAND (Track.sp); P_AC + AP_*: ACID (Track.ap)
static const char *par_name[P_N] = {"SAMPLE", "PITCH", "FINE", "VOLUME", "PAN", "START", "END", "DIR", "VOICE", "ROOT", "SLICES", "WARP", "BARS", "BPM", "GRAIN", "FLUX", "TYPE", "SOURCE", "NORM", "FADE IN", "FADE OUT", "XFADE",
    "OSC 1", "OSC 2", "TUNE 2", "DETUNE", "MIX", "NOISE", "CUTOFF", "RESO", "ENV", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "GLIDE",
    "WAVE", "TUNING", "CUTOFF", "RESO", "ENV MOD", "DECAY", "ACCENT", "DIST", "DRIVE", "TONE", "MIX",
    "POSITION", "SPRAY", "SIZE", "DENSITY", "SHIMMER", "MOTION",
    "SIZE RND", "WINDOW", "REVERSE", "PITCH", "FINE", "PITCH RND", "INTERVALS", "SYNC", "TIME RND", "WIDTH", "SCAN", "FREEZE", "ATTACK", "RELEASE", "TONE",
    "SUB", "SUB OCT", "SUB WAVE", "PULSE W", "NOISE", "ATTACK", "RELEASE", "SLIDE", "RANGE", "TRACK", "RESO HP", "BASS", "ACC ATK", "ACC DEC", "CLIP",
    "RATE", "SYNC", "WAVE", "PITCH AMT", "CUTOFF AMT", "PWM AMT", "OSC 1 RANGE", "OSC 2 RANGE", "HARD SYNC", "HP CUTOFF", "KB TRACK", "VCA", "SPRING", "ARP", "MODE", "OCTAVES", "ARP RATE", "ARP GATE", "ATTENUATOR",
    "", "LFO OUT", "S&H OUT", "OSC 1 OUT", "OSC 2 OUT", "NOISE OUT", "MIX OUT", "FILTER OUT", "ENV OUT", "VCA OUT", "KB PITCH", "KB GATE", "VELOCITY", "ATTEN OUT", "", "",
    "", "OSC 1 PITCH", "OSC 2 PITCH", "PWM", "LIN FM", "MIX OSC 1", "MIX OSC 2", "MIX NOISE", "FILTER IN", "CUTOFF", "RESO", "VCA IN", "VCA AMT", "ATTEN IN", "LFO RATE", ""};
static const char *dist_name[4] = {"DIODE", "TRANSISTOR", "TUBE", "FUZZ"};
static const char *type_name[4] = {"SAMPLER", "GRAND", "ACID", "CLOUD"}, *wave_name[4] = {"TRI", "SAW", "SQR", "PULSE"};
static int is_synth(int s) { return song.t[s].type != 0; }   // either synth: no sample, a drawn panel
static int has_sound(int s) { return engine_sample(s) || is_synth(s); }
static const char *slot_name(int s) { const Sample *x = engine_sample(s);   // the pad's own name (SHIFT+X on GRID), else its instrument or sample
    return song.pname[s][0] && (is_synth(s) || x) ? song.pname[s] : is_synth(s) ? type_name[song.t[s].type % 4] : x ? x->name : NULL; }
static Col par_section(int p) {        // a GRAND parameter's panel colour, C_BG when it has none
    int k = p - P_SY;
    if (p < P_SY) return C_BG;
    if (k <= SP_DETUNE) return GB_OSC;
    if (k >= SP_CUTOFF && k <= SP_ENV) return GB_FILTER;
    return C_BG;
}
static const char *warp_name[6] = {"OFF", "BEATS", "REPITCH", "TONES", "TEXTURE", "COMPLEX"};
#define PROWS 9
static int srow = 1, scol = 0, lane;
static int paint_anchor = -1, paint_val; static int8_t paint_snap[NSTEP];   // rows: -1 name, 0 header(2), 1-2 steps(16), 3+ params(2)
static int spage;                        // the steps are shown 32 at a time
static int cur_step(void) { return spage * 32 + (srow - 1) * 16 + scol; }
static void set_step(int st) { spage = st / 32; srow = 1 + (st % 32) / 16; scol = st % 16; }
static int last_page(void) { int l = song.t[sel].pat[song.t[sel].cur].len; return (l > 0 ? l - 1 : 0) / 32; }
static int last_page(void);
static int rolling; static const int rl_d32[6];   // the piano roll (below, before key_event)
static int picking; static void pick_open(void); static void pick_close(void); static void draw_pick(void); static void pick_key(int k, int down);
static int randing; static void draw_rnd(void); static int presetting; static void draw_presets(void); static void preset_open(void); static void preset_close(int keep);
static void roll_mode(void); static void roll_open(void); static void roll_del(Pattern *p, int i);
static int roll_add(Pattern *p, int start, int len, int note, int vel); static void draw_roll(void); static void roll_leave(void); static int roll_base(int s); static int roll_ticks(const Pattern *p);
static void pat_switch(int np); static void pat_open(void);
static int seq_cols(int r) { return r < 0 ? 2 : r == 1 || r == 2 ? 16 : r == 0 ? (is_auto(lane) ? 5 : 4) : 2; }   // row -1: the name and the pattern; row 0: LENGTH STEP PAGE MODE

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
// GRAND has its own page in the order of the original's panel: oscillators and mixer on the left,
// filter and envelope on the right. TYPE keeps its place in both.
#define WARP_ROWS 5
static const int8_t par_col_sampler[2][PROWS] = {
    {P_SAMPLE, P_PITCH, P_FINE, P_VOL, P_PAN, P_VOICE, P_ROOT, P_SLICES, P_DIR},
    {P_WARP, P_BARS, P_BPM, P_GRAIN, P_FLUX, -1, -1, -1, -1},
};
#define par_col par_col_sampler
// GRAND's page is a drawn panel (draw_gb): the controls sit where the panel shows them, the arrows go to
// the nearest one in their direction. srow 3 = the panel, gb_cur = the control. The SAMPLE place is the
// instrument switch on both pages (EDIT+UP/DOWN), so TYPE has no row of its own.
enum { GB_KNOBC, GB_WAVE, GB_SLIDER, GB_TOGGLE, GB_SELECT };
typedef struct { int16_t p, x, y; int8_t kind, r; } GbCtl;
#define GB_SX0 16
#define GB_SX1 (GB_SX0 + 304)               // MIXER
#define GB_SX2 (GB_SX1 + 124)               // FILTER
#define GB_SX3 (GB_SX2 + 224)               // ENVELOPE
#define GB_SX4 (GB_SX3 + 204)               // OUTPUT, to 1008
static const GbCtl gb_ctl[] = {
    {P_SAMPLE, 208, 273, GB_SELECT, 0},
    {P_SY + SP_OSC1, GB_SX0 + 75, 470, GB_WAVE, 0}, {P_SY + SP_OSC2, GB_SX0 + 225, 470, GB_WAVE, 0},
    {P_SY + SP_TUNE2, GB_SX0 + 75, 596, GB_KNOBC, 26}, {P_SY + SP_DETUNE, GB_SX0 + 225, 596, GB_KNOBC, 26},
    {P_SY + SP_MIX, GB_SX1 + 60, 420, GB_KNOBC, 26}, {P_SY + SP_NOISE, GB_SX1 + 60, 580, GB_KNOBC, 26},
    {P_SY + SP_CUTOFF, GB_SX2 + 110, 470, GB_KNOBC, 34}, {P_SY + SP_RESO, GB_SX2 + 55, 604, GB_KNOBC, 24}, {P_SY + SP_ENV, GB_SX2 + 165, 604, GB_KNOBC, 24},
    {P_SY + SP_ATTACK, GB_SX3 + 25, 540, GB_SLIDER, 0}, {P_SY + SP_DECAY, GB_SX3 + 75, 540, GB_SLIDER, 0},
    {P_SY + SP_SUSTAIN, GB_SX3 + 125, 540, GB_SLIDER, 0}, {P_SY + SP_RELEASE, GB_SX3 + 175, 540, GB_SLIDER, 0},
    {P_VOICE, GB_SX4 + 68, 372, GB_TOGGLE, 0}, {P_SY + SP_GLIDE, GB_SX4 + 68, 460, GB_KNOBC, 20},
    {P_PITCH, GB_SX4 + 68, 544, GB_KNOBC, 20}, {P_VOL, GB_SX4 + 68, 628, GB_KNOBC, 20},
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
// GRAND DEEP (Y on GRAND's panel): MODULATION, OSCILLATORS, FILTER, VCA
#define GD_SA 320                           // ARP
#define GD_S1 524                           // OSCILLATORS
#define GD_S2 728                           // FILTER
#define GD_S3 868                           // VCA, to 1008
static const GbCtl gd_ctl[] = {
    {P_SAMPLE, 208, 273, GB_SELECT, 0},
    {P_GX + GX_LRATE, 60, 396, GB_KNOBC, 24}, {P_GX + GX_LWAVE, 160, 400, GB_WAVE, 0},
    {P_GX + GX_LPITCH, 66, 560, GB_KNOBC, 22}, {P_GX + GX_LCUT, 166, 560, GB_KNOBC, 22}, {P_GX + GX_LPWM, 266, 560, GB_KNOBC, 22},
    {P_GX + GX_LSYNC, 166, 660, GB_SELECT, 0},
    {P_GX + GX_ARP, GD_SA + 44, 400, GB_TOGGLE, 0}, {P_GX + GX_AMODE, GD_SA + 152, 400, GB_WAVE, 0},
    {P_GX + GX_ARATE, GD_SA + 50, 566, GB_KNOBC, 22}, {P_GX + GX_AGATE, GD_SA + 150, 566, GB_KNOBC, 22}, {P_GX + GX_AOCT, GD_SA + 100, 660, GB_SELECT, 0},
    {P_GX + GX_O1OCT, GD_S1 + 64, 400, GB_WAVE, 0}, {P_GX + GX_O2OCT, GD_S1 + 154, 400, GB_WAVE, 0}, {P_GX + GX_SYNC, GD_S1 + 100, 590, GB_TOGGLE, 0},
    {P_GX + GX_HP, GD_S2 + 68, 410, GB_KNOBC, 26}, {P_GX + GX_KBT, GD_S2 + 68, 590, GB_WAVE, 0},
    {P_GX + GX_VCA, GD_S3 + 56, 400, GB_TOGGLE, 0}, {P_GX + GX_SPRING, GD_S3 + 70, 600, GB_KNOBC, 26},
};
// PATCH (page 3 of GRAND): the jacks by module, as the columns of a patch bay
#define PJ(o) (P_PT + (o))
#define PI_(i) (P_PT + 16 + (i))
static const GbCtl pt_ctl[] = {
    {P_SAMPLE, 208, 273, GB_SELECT, 0},
    {PJ(JO_LFO), 80, 372, GB_SELECT, 16}, {PJ(JO_SH), 80, 456, GB_SELECT, 16}, {PI_(JI_LRATE), 80, 540, GB_SELECT, 16},
    {PI_(JI_P1), 200, 372, GB_SELECT, 16}, {PI_(JI_P2), 200, 456, GB_SELECT, 16}, {PI_(JI_PWM), 200, 540, GB_SELECT, 16}, {PI_(JI_FM), 200, 624, GB_SELECT, 16},
    {PJ(JO_OSC1), 320, 372, GB_SELECT, 16}, {PJ(JO_OSC2), 320, 456, GB_SELECT, 16}, {PJ(JO_NOISE), 320, 540, GB_SELECT, 16},
    {PI_(JI_M1), 440, 372, GB_SELECT, 16}, {PI_(JI_M2), 440, 456, GB_SELECT, 16}, {PI_(JI_MN), 440, 540, GB_SELECT, 16}, {PJ(JO_MIX), 440, 624, GB_SELECT, 16},
    {PI_(JI_FIN), 560, 372, GB_SELECT, 16}, {PI_(JI_CUT), 560, 456, GB_SELECT, 16}, {PI_(JI_RES), 560, 540, GB_SELECT, 16}, {PJ(JO_FILT), 560, 624, GB_SELECT, 16},
    {PI_(JI_VIN), 680, 372, GB_SELECT, 16}, {PI_(JI_VAMT), 680, 456, GB_SELECT, 16}, {PJ(JO_ENV), 680, 540, GB_SELECT, 16}, {PJ(JO_VCA), 680, 624, GB_SELECT, 16},
    {PJ(JO_KBP), 800, 372, GB_SELECT, 16}, {PJ(JO_GATE), 800, 456, GB_SELECT, 16}, {PJ(JO_VEL), 800, 540, GB_SELECT, 16},
    {PI_(JI_ATT), 930, 372, GB_SELECT, 16}, {P_GX + GX_ATTEN, 930, 480, GB_KNOBC, 22}, {PJ(JO_ATT), 930, 600, GB_SELECT, 16},
};
static int pt_pick;                       // PATCH: the output a cable is being carried from (0 none)
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
    if (song.t[sel].type == 1 && cl_deep == 2) { *n = (int)(sizeof pt_ctl / sizeof pt_ctl[0]); return pt_ctl; }
    if (song.t[sel].type == 1 && cl_deep) { *n = (int)(sizeof gd_ctl / sizeof gd_ctl[0]); return gd_ctl; }
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
static int v2_steps(void) { return ui_cat == PC_PATTERN && tab == T_SEQ && !hubbing && !patmenu && !perform && !cur_pat(sel)->mode && !is_auto(lane); }
static void seq_clamp(void) {            // slot change or undo may hide the parameter under the cursor
    if (ui_cat == PC_SOUND && tab == T_SEQ && !hubbing && !patmenu) sound_sync();   // SOUND (UI v2): its page and card exist
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
static void par_str(int pad, int p, char *b, int n) {
    Track *t = &song.t[pad]; const Sample *s = engine_sample(pad);
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
    case P_SLICES: { int k = track_nslices(pad);
        if (!t->slices) snprintf(b, n, "OFF"); else if (t->slices == 5) snprintf(b, n, "AUTO %d", k);
        else if (t->slices == 6) snprintf(b, n, "EDIT %d", k); else snprintf(b, n, "%d", k); break; }
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
    if (p >= P_PT) {                     // a jack: where its cables go
        int j = p - P_PT, isin = j >= 16, id = isin ? j - 16 : j, k = 0; b[0] = 0;
        for (int c = 0; c < NCABLE; c++) { const uint8_t *g = song.gp[pad][c]; if (!g[0] || !g[1]) continue;
            if (isin ? g[1] == id : g[0] == id) { int l = (int)strlen(b); snprintf(b + l, n - l, "%s%s", k++ ? ", " : isin ? "FROM " : "TO ", par_name[isin ? P_PT + g[0] : P_PT + 16 + g[1]]); } }
        if (!k) snprintf(b, n, isin ? (id == JI_M1 ? "NORMAL: OSC 1" : id == JI_M2 ? "NORMAL: OSC 2" : id == JI_MN ? "NORMAL: NOISE" : id == JI_FIN ? "NORMAL: MIX" : id == JI_VIN ? "NORMAL: FILTER" : id == JI_VAMT ? "NORMAL: ENV" : id == JI_ATT ? "NORMAL: +1" : "FREE") : "FREE");
        return;
    }
    if (p >= P_GX) {                     // GRAND DEEP
        int k = p - P_GX, v = song.gx[pad][k];
        static const char *const wn[5] = {"TRIANGLE", "SQUARE", "SAW", "RAMP", "S&H"}, *const o1[4] = {"32'", "16'", "8'", "4'"}, *const o2[4] = {"16'", "8'", "4'", "2'"}, *const kb[3] = {"OFF", "HALF", "FULL"};
        switch (k) {
        case GX_LRATE: { float hz = gx_lfo_hz(v); if (song.gx[pad][GX_LSYNC]) snprintf(b, n, "SYNCED"); else if (hz < 1) snprintf(b, n, "%.2fHZ", hz); else snprintf(b, n, "%.1fHZ", hz); break; }
        case GX_LSYNC: snprintf(b, n, "%s", v ? div_label[(v - 1) % NDIV] : "FREE"); break;
        case GX_LWAVE: snprintf(b, n, "%s", wn[v % 5]); break;
        case GX_O1OCT: snprintf(b, n, "%s", o1[v & 3]); break;
        case GX_O2OCT: snprintf(b, n, "%s", o2[v & 3]); break;
        case GX_SYNC: snprintf(b, n, v ? "ON" : "OFF"); break;
        case GX_KBT: snprintf(b, n, "%s", kb[v % 3]); break;
        case GX_VCA: snprintf(b, n, v == 2 ? "DRONE" : v ? "GATE" : "ENV"); break;
        case GX_HP: if (v) snprintf(b, n, "%.0fHZ", 20 * powf(100.f, v / 100.f)); else snprintf(b, n, "OFF"); break;
        case GX_LPITCH: snprintf(b, n, v ? "+-%.1f ST" : "OFF", v * 0.12f); break;
        case GX_ARP: snprintf(b, n, v ? "ON" : "OFF"); break;
        case GX_AMODE: { static const char *const am[4] = {"UP", "DOWN", "ORDER", "UP/DOWN"}; snprintf(b, n, "%s", am[v & 3]); break; }
        case GX_AOCT: snprintf(b, n, "%d OCT", v % 3 + 1); break;
        case GX_ARATE: snprintf(b, n, "%s", div_label[v % NDIV]); break;
        case GX_AGATE: snprintf(b, n, "%d%%", v); break;
        case GX_ATTEN: snprintf(b, n, "%+d%%", v); break;
        case GX_LCUT: snprintf(b, n, v ? "+-%.1f OCT" : "OFF", v * 0.03f); break;
        default: if (v) snprintf(b, n, "%d%%", v); else snprintf(b, n, "OFF");
        }
        return;
    }
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
    if (p >= P_SY) {
        int k = p - P_SY, v = t->sp[k];
        if (k == SP_OSC1 || k == SP_OSC2) snprintf(b, n, "%s", wave_name[v & 3]);
        else if (k == SP_TUNE2 || k == SP_ENV) snprintf(b, n, v ? "%+d" : "0", v);
        else if (k == SP_DETUNE) snprintf(b, n, "%d CT", v);
        else if (k == SP_ATTACK || k == SP_DECAY || k == SP_RELEASE || k == SP_GLIDE) {
            float ms = k == SP_GLIDE ? v * v * 0.2f : sy_time(v) * 1000;   // GLIDE: (v/100)^2 * 2 s
            if (k == SP_GLIDE && t->voice) snprintf(b, n, "%s MONO ONLY", v ? "ON" : "OFF");   // POLY: glide does nothing
        else if (k == SP_GLIDE && !v) snprintf(b, n, "OFF");
            else if (ms < 1000) snprintf(b, n, "%dMS", (int)(ms + 0.5f)); else snprintf(b, n, "%.1fS", ms / 1000);
        } else snprintf(b, n, "%d", v);
    }
}
#include "params.h"
static void par_adjust(int pad, int p, int d, int big) {
    Track *t = &song.t[pad];
    if (p == P_SAMPLE) p = P_TYPE;        // the instrument switch: every arrow, round (RIGHT/UP next, LEFT/DOWN back)
    if (p == P_TYPE) {
        int nt = ((t->type + d) % 4 + 4) % 4; if (nt == t->type) return;   // SAMPLER, GRAND, ACID, CLOUD
        edit_begin(); t->type = (int8_t)nt;
        if (nt == 1 && t->sp[SP_MARK] != SP_INIT) synth_defaults(t->sp);   // the first time: a sound that plays at once
        if (nt == 2 && t->ap[AP_MARK] != AP_INIT) acid_defaults(t->ap);
        if (nt == 3 && t->cp[CP_MARK] != CP_INIT) cl_defaults(t->cp);
        return;
    }
    if (p == P_SRC) { if (!big) { edit_begin(); neighbour_file(pad, d); } return; }   // CLOUD's sample: the next file
    if (p == P_NORM) { edit_begin(); int8_t *v = &song.mix.pad[pad].spare[PM_NORM]; *v = (int8_t)((*v + (d > 0 ? 1 : 2)) % 3); return; }
    edit_begin();
    if (p >= P_PT) return;                // jacks are plugged, not turned
    switch (p) {                          // what is not a plain number in a range
    case P_DIR: t->rev = !t->rev; return;
    case P_VOICE: t->voice = !t->voice; return;
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
    case P_ROOT: t->root = -1; break; case P_SLICES: t->slices = 0; break; case P_WARP: t->warp = 0; break;
    case P_BARS: t->bars = 0; relayout(pad); break;
    case P_BPM: t->bpm = 0; relayout(pad); break;
    case P_GRAIN: t->grain = 0; break; case P_FLUX: t->flux = 0; break;
    case P_SAMPLE: if (is_synth(pad)) t->type = 0; else set_slot_file(pad, ""); break;   // a synth goes back to the sampler
    case P_TYPE: t->type = 0; break;
    }
    if (p >= P_PT) { int j = p - P_PT, isin = j >= 16, id = isin ? j - 16 : j;   // OPT+EDIT on a jack: its cables out
        for (int c = 0; c < NCABLE; c++) { uint8_t *g = song.gp[pad][c]; if (isin ? g[1] == id : g[0] == id) g[0] = g[1] = 0; } pt_pick = 0; }
    else if (p >= P_GX) song.gx[pad][p - P_GX] = gx_def[p - P_GX];
    else if (p >= P_AX) song.ax[pad][p - P_AX] = ax_def[p - P_AX];
    else if (p >= P_CX) { int8_t d0[16]; cx_defaults(d0); song.cx[pad][p - P_CX] = d0[p - P_CX]; }
    else if (p >= P_CL) { int8_t d0[7]; cl_defaults(d0); t->cp[p - P_CL] = d0[p - P_CL]; }
    else if (p >= P_AC) { int8_t d0[12]; acid_defaults(d0); t->ap[p - P_AC] = d0[p - P_AC]; }
    else if (p >= P_SY) { int8_t d0[16]; synth_defaults(d0); t->sp[p - P_SY] = d0[p - P_SY]; }
}
static void step_audition(int st);
static void step_reset(Pattern *p, int st);
static void step_seed(Pattern *p, int st);
static void step_hear(int st) {          // an edit sounds, unless the pad's clip plays it anyway
    if (!(engine_playing() && song.t[sel].on)) step_audition(st);
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
    case L_REP: p->rep[st] = clampi(p->rep[st] + d, 1, 4); break;
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
        if (big && (pr == P_SY + SP_OSC1 || pr == P_SY + SP_OSC2 || pr == P_AC + AP_DTYPE || pr == P_WARP || pr == P_AX + AX_SUBWAVE || pr == P_GX + GX_LWAVE || pr == P_GX + GX_O1OCT || pr == P_GX + GX_O2OCT || pr == P_GX + GX_KBT || pr == P_GX + GX_AMODE)) { d = -d; big = 0; }
        if (big && (pr == P_GX + GX_SYNC || pr == P_GX + GX_VCA || pr == P_GX + GX_ARP)) { d = k == K_DOWN ? 1 : -1; big = 0; }   // levers: UP the first, DOWN the second
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
        int len = cur_pat(sel)->len, d = k == K_RIGHT ? 1 : k == K_LEFT ? -1 : k == K_DOWN ? 8 : -8, n = cur_step() + d;
        if (n >= 0 && n < len) set_step(n);
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
            if (cl_deep < (song.t[sel].type == 1 ? 2 : 1) && k == K_DOWN && n < 0) {
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
    if (srow == 0) { int l = p->len, dv = p->div, md = p->mode; pattern_clear(p); p->len = l; p->div = dv; p->mode = md; toast("CLIP CLEARED"); }
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
    else if (ty == 1 && p >= P_SY && p < P_SY + SP_N) tg = LT_ENG + p - P_SY;
    else if (ty == 2 && p >= P_AC && p < P_AC + AP_N) tg = LT_ENG + p - P_AC;
    else if (ty == 3 && p >= P_CL && p < P_CL + CP_N) tg = LT_ENG + p - P_CL;
    else if (ty == 3 && p >= P_CX && p < P_CX + CX_N) tg = LT_ENG + CP_N + p - P_CX;
    else if (ty == 2 && p >= P_AX && p < P_AX + AX_N) tg = LT_ENG + LT_DEEP + p - P_AX;   // ACID DEEP
    else if (ty == 1 && p >= P_GX && p < P_GX + GX_N) tg = LT_ENG + LT_DEEP + p - P_GX;   // GRAND DEEP
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
static void learn_end(void) {
    ui_cat = PC_MOD;                     // UI v2: the cards of SOUND were shown for the pointing
    if (learn_lfo >= 0) { int j = learn_lfo; learn_lfo = -1; set_tab(T_FX); fx_unit = sel; fx_page = 1; lcol = j; lrow = 0; learning = 0; return; }   // back on the MOD page
    learning = 0; lane = learn_lane; srow = 0; scol = 4;
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
static void pt_tap(int j) {               // PATCH: EDIT on an output carries a cable from it; on an input plugs it (or picks its cable up)
    uint8_t (*gp)[2] = song.gp[sel];
    if (j < 16) { pt_pick = pt_pick == j ? 0 : j; if (pt_pick) toast("CABLE: EDIT ON AN INPUT PLUGS IT, Y DROPS IT"); return; }
    int id = j - 16;
    if (!pt_pick) {                      // an input with a cable: take it out, carry it on
        for (int c = 0; c < NCABLE; c++) if (gp[c][1] == id && gp[c][0]) { edit_begin(); pt_pick = gp[c][0]; gp[c][0] = gp[c][1] = 0; toast("CABLE TAKEN OUT: PLUG IT ELSEWHERE OR Y"); return; }
        toast("AN OUTPUT FIRST: EDIT ON IT, THEN HERE"); return;
    }
    edit_begin(); int free = -1;
    for (int c = 0; c < NCABLE; c++) if (gp[c][1] == id) { gp[c][0] = gp[c][1] = 0; }   // one cable per input: the new one replaces it
    for (int c = 0; c < NCABLE; c++) if (!gp[c][0] || !gp[c][1]) { free = c; break; }
    if (free < 0) { toast("8 CABLES ALREADY: TAKE ONE OUT"); return; }
    gp[free][0] = (uint8_t)pt_pick; gp[free][1] = (uint8_t)id; pt_pick = 0;
    char b[64]; snprintf(b, sizeof b, "%s -> %s", par_name[P_PT + gp[free][0]], par_name[P_PT + 16 + id]); toast(b);
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
    if (srow >= 1 && srow <= 2 && cur_pat(sel)->mode) { roll_open(); return; }
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
        if (p->trig[st] && lane != L_TRIG) { char b[32]; snprintf(b, sizeof b, "ERASE ON THE %s LANE", lane_nm(L_TRIG)); toast(b); return; }   // 2026-11-12, user
        if (!p->trig[st]) step_seed(p, st);
        p->trig[st] = !p->trig[st];
        // 2026-11-11 (user): a new step is the tonic of the pad's key (NOTE_NONE plays exactly that and follows the key), and its name shows at once
        if (!p->trig[st]) step_reset(p, st);   // (a new step sounded when B went down)           // a removed step forgets its note, velocity, chance, repeats, slice
    } else if (srow >= 3 && cur_par() == P_SAMPLE && is_synth(sel)) preset_open();   // EDIT on the instrument: its presets
    else if (srow >= 3 && cur_par() == P_SRC) browse_slot();   // CLOUD: EDIT on SOURCE opens the browser
    else if (srow >= 3 && song.t[sel].type == 0 && cur_par() == P_BPM) {   // the loop's tempo by tapping (besides AUTO and EDIT+arrows)
        static Uint32 tt[8]; static int tn; int b = tap_bpm(tt, &tn); char m[32];
        if (b) { edit_begin(); song.t[sel].bpm = (int16_t)clampi(b, 40, 300); relayout(sel); snprintf(m, sizeof m, "LOOP TAP  %d BPM", song.t[sel].bpm); } else snprintf(m, sizeof m, "TAP: GO ON IN TIME");
        toast(m); }
    else if (srow >= 3 && is_synth(sel) && cur_par() == P_AC + AP_WAVE) { edit_begin(); song.t[sel].ap[AP_WAVE] ^= 1; }   // a tap flips the lever
    else if (srow >= 3 && is_synth(sel) && cur_par() == P_CX + CX_FREEZE) { edit_begin(); song.cx[sel][CX_FREEZE] ^= 1; }
    else if (srow >= 3 && cur_par() >= P_PT && cur_par() < P_PT + 32) pt_tap(cur_par() - P_PT);
    else if (srow >= 3 && is_synth(sel) && (cur_par() == P_GX + GX_SYNC || cur_par() == P_GX + GX_VCA || cur_par() == P_GX + GX_ARP)) { edit_begin(); { int8_t *gv = &song.gx[sel][cur_par() - P_GX]; *gv = cur_par() == P_GX + GX_VCA ? (*gv + 1) % 3 : *gv ^ 1; } song.gx[sel][GX_MARK] = GX_INIT; }
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
static int pfol, pfcol;                  // SELECT: the cursor is on the cards (not on the grid); pfcol: the old window's field
static int sl_r, sl_c;                   // SELECT (UI v2): rows 0-1 the grid 2 x 8, 2 LENGTH DIVISION MODE, 3-4 REPEAT THEN / CHANCE ELSE
static void select_value(int d, int big); static void patmenu_open(void);
static const char *const fa_name[FA_N] = {"LOOP", "NEXT", "PREV", "FIRST", "RANDOM", "OTHER", "JUMP", "STOP"};
static int fol_pat(void) { return pat_last >= 0 && pat_last < NPAT ? pat_last : pcur % NPAT; }   // FOLLOW is about the chosen pattern (SEL)
static void fol_value(int d, int big) {  // EDIT+arrows on a FOLLOW field
    uint8_t *fo = song.fol[sel][fol_pat()]; edit_begin();
    switch (pfcol) {
    case 0: fo[FO_AFTER] = (uint8_t)clampi((fo[FO_AFTER] ? fo[FO_AFTER] : 1) + d * (big ? 4 : 1), 1, 64); break;
    case 1: fo[FO_A] = (uint8_t)((fo[FO_A] + d + FA_N) % FA_N); break;
    case 2: fo[FO_B] = (uint8_t)((fo[FO_B] + d + FA_N) % FA_N); break;
    case 3: fo[FO_BCH] = (uint8_t)clampi(fo[FO_BCH] + d * (big ? 10 : 5), 0, 100); break;
    case 4: fo[FO_JUMP] = (uint8_t)((fo[FO_JUMP] + d + NPAT) % NPAT); break;
    }
}
static void pat_open(void) { Track *t = &song.t[sel]; pcur = pat_last = t->queued >= 0 ? t->queued : t->cur; pfol = 0; tab = T_PAT; sl_r = pcur / 8; sl_c = pcur % 8; }
static void pat_close(int take) {         // OK and Y both leave on the chosen pattern (the one EDIT played or marked)
    (void)take; pat_switch(pat_last);
    if (ui_cat == PC_PATTERN) { patmenu_open(); return; }   // UI v2: back to PATTERN's list
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
enum { J_TEMPO, J_SWING, J_KEY, J_SCALE, J_LAUNCH, J_METRO, J_BEATS, J_COUNT, J_QUANT, J_MASTER, J_THEME, J_LIB, J_NEW, J_SNAP, J_PROJ, J_SAVE, J_SAVEAS, J_EXPORT, J_HINT, J_STRIP, J_VIEW, J_LOCK, J_N };
// The sections of PROJECT (2026-11-09): the left column chooses one, the middle shows its settings
enum { JS_SONG, JS_PLAY, JS_REC, JS_FILE, JS_SYS, JS_N };
static const char *const prj_sec_name[JS_N] = {"SONG", "PLAY", "RECORD", "FILE", "SYSTEM"};
static const char *const prj_sec_hint[JS_N] = {"TEMPO KEY SCALE", "LAUNCH CLICK MASTER", "COUNT-IN QUANTISE", "OPEN SAVE NEW", "THEME LIBRARY"};
static const int8_t prj_sec[JS_N][6] = {{J_TEMPO, J_SWING, J_KEY, J_SCALE, J_SNAP, J_BEATS}, {J_LAUNCH, J_METRO, J_MASTER, -1},
                                        {J_COUNT, J_QUANT, -1}, {J_PROJ, J_SAVE, J_SAVEAS, J_EXPORT, J_NEW, -1}, {J_VIEW, J_STRIP, J_HINT, J_LOCK, J_THEME, J_LIB}};
static const char *const prj_help[J_N] = {"THE PROJECT'S SPEED. EDIT TAPS IT, L1/R1 HELD NUDGE WHILE PLAYING", "LATE EVEN 1/16 STEPS: A SHUFFLE",
    "THE TONIC EVERY PAD PLAYS IN (A PAD CAN HAVE ITS OWN)", "WHICH NOTES THE DEGREES, KEYS AND RANDOM USE",
    "A CLIP STARTS OR STOPS ON THIS GRID, AS IN ABLETON", "A CLICK ON EVERY BEAT WHILE PLAYING", "BEATS IN A BAR: LAUNCH AND RECORDING COUNT THEM",
    "BEATS COUNTED IN BEFORE A TAKE FROM STOP", "WHERE RECORDED HITS LAND ON THE STEPS", "THE WHOLE MIX, THE SAME FADER AS IN MIXER",
    "COLOURS OF BRICKPAD, KEPT FOR EVERY PROJECT", "CHECKS EVERY SAMPLE ONCE, SLOWLY, MARKS BROKEN FILES", "EDIT TWICE: AN EMPTY PROJECT (UNDO BRINGS IT BACK)", "ON: EDIT+<> ON A STEP WALKS THE PAD'S SCALE ONLY, AS ON THE M8", "THE OPEN PROJECT; * = CHANGES NOT SAVED. EDIT: THE PROJECTS", "EDIT: WRITE THE PROJECT TO ITS FILE", "EDIT: TYPE A NAME, THEN SAVE UNDER IT", "EDIT: RENDER BARS OF THE PROJECT TO exports/NAME.wav", "THE KEYS OF THE SCREEN IN THE FOOTER. OFF: A CLEAN FOOTER, SHIFT STILL SHOWS THEM ALL",
    "THE TWELVE PADS UNDER THE PATH WHILE A PAD IS OPEN: ENGINE, CLIP, WHICH ONE", "ADVANCED: EVERYTHING. SIMPLE: PLAY, NOTES, SOUND, LARGE (MENU+ARROW). HOLD F1+F2 TO LEAVE IT",
    "IN THE SIMPLE VIEW NOTHING CAN BE DELETED OR OVERWRITTEN"};
static const char *prj_name[J_N] = {"TEMPO", "SWING", "KEY", "SCALE", "CLIP LAUNCH", "METRONOME", "BEATS/BAR", "COUNT-IN", "REC QUANT", "MASTER VOL", "THEME", "CHECK LIBRARY", "NEW PROJECT", "SCALE SNAP", "PROJECT", "SAVE", "SAVE AS", "EXPORT WAV", "FOOTER HINTS", "PAD STRIP", "VIEW", "KID LOCK"};
static int ui_strip = 1, ui_simple, ui_lock;   // PROJECT > SYSTEM: the pad strip (0 hidden, 1 thin), the simple view, its kid lock (ui.cfg beside the binary)
static void ui_cfg_save(void) { char b[32]; int n = snprintf(b, sizeof b, "%d %d %d\n", ui_strip, ui_simple, ui_lock); write_whole("ui.cfg", b, n); }
static void ui_cfg_load(void) { FILE *f = fopen("ui.cfg", "r"); if (f) { int a = 1, b = 0, c = 0; if (fscanf(f, "%d %d %d", &a, &b, &c) >= 1) { ui_strip = a != 0; ui_simple = b != 0; ui_lock = c != 0; } fclose(f); } }
static int hint_on = 1;                    // HOVER HINTS (hints.cfg beside the binary: "0" = off)
static void hint_set(int on) { hint_on = on; write_whole("hints.cfg", on ? "1\n" : "0\n", 2); }
static void hint_load(void) { FILE *f = fopen("hints.cfg", "r"); if (f) { int v = 1; if (fscanf(f, "%d", &v) == 1) hint_on = v != 0; fclose(f); } }
static Uint32 new_armed;                 // NEW PROJECT asked once, waiting for the second press
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
    sync_samples(); frz_reset();
    save_state();
    prj_cur[0] = 0; prj_cfg_write(); prj_mark_saved_empty();
    toast("NEW PROJECT");
}
static const char *rlen_name[4] = {"BAR", "1/2 BAR", "BEAT", "STEP"};
static const char *quant_name[5] = {"CLIP STEP", "1/32", "1/16", "1/8", "1/4"};
static int prj_dirty(void);
static int jcur, jsec, jcol; static Uint32 taps[5]; static int ntaps;   // jcol 0: the sections, 1: the settings of jsec

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
    "SAMPLER", "GRAND", "ACID", "CLOUD", "PATTERN", "SOUND", "FX", "MOD", "MIX"};
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
    if (memcmp(&t[TC_TEXT], &th_cur[TC_TEXT], sizeof(Col)) || memcmp(&t[TC_ACCENT], &th_cur[TC_ACCENT], sizeof(Col))) fig_gen++;   // figures carry these two
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
    if (ted_row == TC_TEXT || ted_row == TC_ACCENT) fig_gen++;
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
        if (ted_row < TC_N) { th_cur[ted_row] = theme_colors(th_sel)[ted_row]; th_mod = memcmp(th_cur, theme_colors(th_sel), sizeof th_cur) != 0; fig_gen++; toast("THE SAVED COLOUR AGAIN"); }
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
    for (int k = 0; k < JS_N; k++) for (int i = 0; i < 6 && prj_sec[k][i] >= 0; i++) if (prj_sec[k][i] == j) { jsec = k; jcol = 1; jcur = j; return; }
}
static int prj_rows(int sc, int *r) { int n = 0; while (n < 6 && prj_sec[sc][n] >= 0) { r[n] = prj_sec[sc][n]; n++; } return n; }
static void prj_move(int k) {
    int r[6], n = prj_rows(jsec, r), i = 0; while (i < n && r[i] != jcur) i++; if (i >= n) { i = 0; jcur = r[0]; }
    if (!jcol) {                         // the sections: UP/DOWN choose, RIGHT goes in
        if (k == K_UP || k == K_DOWN) { jsec = clampi(jsec + (k == K_DOWN ? 1 : -1), 0, JS_N - 1); prj_rows(jsec, r); jcur = r[0]; }
        else if (k == K_RIGHT) jcol = 1;
        return;
    }
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
    if (jcur == J_LIB || jcur == J_NEW || (jcur >= J_PROJ && jcur < J_HINT)) return;   // actions, not values (SCALE SNAP is a value)
    if (jcur == J_STRIP) { ui_strip = !ui_strip; ui_cfg_save(); return; }
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
    case J_STRIP: ui_strip = !ui_strip; ui_cfg_save(); break;
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
// TAP TEMPO (2026-11-12, as on the M8): EDIT taps in time; the mean of the last four gaps, a pause over 2 s starts again
static int tap_bpm(Uint32 *t, int *n) {
    Uint32 now = SDL_GetTicks();
    if (*n && now - t[(*n - 1) % 8] > 2000) *n = 0;
    t[*n % 8] = now; (*n)++;
    if (*n < 2) return 0;
    int k = *n - 1 < 4 ? *n - 1 : 4; Uint32 a = t[(*n - 1 - k) % 8];
    return now > a ? (int)lrint(60000.0 * k / (now - a)) : 0;
}
static void prj_tap(void) {
    if (!jcol) { jcol = 1; return; }     // EDIT on a section: into its settings
    if (jcur == J_TEMPO) { static Uint32 tt[8]; static int tn; int b = tap_bpm(tt, &tn); char m[32];
        if (b) { edit_begin(); song.tempo = (int16_t)clampi(b, 40, 300); snprintf(m, sizeof m, "TAP  %d BPM", song.tempo); } else snprintf(m, sizeof m, "TAP: GO ON IN TIME");
        toast(m); return; }
    if (jcur == J_PROJ) { prj_browse(); return; }
    if (jcur == J_SAVE) { if (prj_cur[0]) prj_save(prj_cur); else naming_open(0); return; }
    if (jcur == J_SAVEAS) { naming_open(0); return; }
    if (jcur == J_EXPORT) { export_open(); return; }
    if (jcur == J_LIB) { lib_toggle(); return; }
    if (jcur == J_THEME) { theming = 1; ted_armed = 0; return; }
    if (jcur == J_NEW) {
        Uint32 now = SDL_GetTicks();
        if (new_armed && ms_since(now, new_armed) < 3000) { new_armed = 0; if (prj_changed()) ask_open(2, ""); else new_project(); }
        else { new_armed = now | 1; toast("EDIT AGAIN: ERASE THIS PROJECT"); }
        return;
    }
    if (jcur == J_TEMPO) {   // tap tempo
        Uint32 now = SDL_GetTicks();
        if (ntaps && now - taps[ntaps - 1] > 2000) ntaps = 0;
        if (ntaps == 5) { memmove(taps, taps + 1, 4 * sizeof *taps); ntaps = 4; }
        taps[ntaps++] = now;
        if (ntaps >= 2) { edit_begin(); song.tempo = clampi((int)(60000.0 * (ntaps - 1) / (now - taps[0]) + 0.5), 40, 300); }
    } else if (jcur == J_LAUNCH || jcur == J_METRO || jcur == J_COUNT || jcur == J_QUANT || jcur == J_SNAP || jcur == J_HINT || jcur == J_STRIP || jcur == J_VIEW || jcur == J_LOCK) prj_edit_arrow(K_RIGHT);
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
static void draw_mixer(void); static void draw_fx(void);
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
    case T_GRID: if (has_sound(sel)) hub_open(); else pick_open(); break;   // UI v2: EDIT opens the pad, an empty one asks for an engine
    case T_SEQ: seq_tap(); break;
    case T_PAT: pat_tap(); break;
    case T_PRJ: prj_tap(); break;
    case T_MIX: mix_tap(); break;
    case T_FX: fx_tap(); break;
    }
}
static void delete_action(void) {   // OPTION + EDIT
    switch (tab) {
    case T_GRID: act_steps_clear(sel); break;
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
static void step_reset(Pattern *p, int st) { p->note[st] = NOTE_NONE; p->vel[st] = 100; p->prob[st] = 100; p->rep[st] = 1; p->slice[st] = -1; }
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
static void set_tab(int t) { leave_screen(); if (browsing) browse_close(); slicing = 0; roll_leave(); randing = 0; learning = 0; learn_lfo = -1; pt_pick = 0; if (picking) pick_close(); if (presetting) preset_close(1); theming = 0; iconing = 0; ducking = 0; perform = 0; tab = t; hubbing = 0; patmenu = 0; modmenu = 0; scening = 0; recpage = 0; if (t == T_GRID || t == T_PRJ) ui_cat = -1; }
// KEYS: the pad gets its own tonality, the project's stays. Coming back to the project's value means following the project again.
static void pad_tonality(int k) {
    Track *t = &song.t[sel]; char b[48];
    if (k == K_LEFT || k == K_RIGHT) { int v = (track_scale(sel) + (k == K_RIGHT ? 1 : NSCALE - 1)) % NSCALE; t->scale = v == song.scale ? 0 : v + 1; }
    else { int v = (track_key(sel) + (k == K_R1 ? 1 : 11)) % 12; t->key = v == song.key ? 0 : v + 1; }
    snprintf(b, sizeof b, "%s %s%s", note_name[track_key(sel)], scale_name[track_scale(sel)], t->scale || t->key ? "" : "  PROJECT"); toast(b);
}
// The icon palette of a pad: X on GRID.
static int icon_group(int i) { int g = 0; while (g + 1 < ICON_GRP_N && i >= icon_grp_at[g + 1]) g++; return g; }
static void icon_open(void) {
    iconing = 1; ic_cur = 0;
    for (int i = 0; i < ICON_PAL_N; i++) if (icon_pal[i] == track_icon(&song.t[sel]) - 1) ic_cur = i;
}
static void icon_key(int k, int down) {
    if (!down) return;
    used[k] = 1;                         // the release must not act again
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_OPT) { listen(sel); return; }
    if (k == K_Y || k == K_X) { leave_screen(); iconing = 0; return; }
    if (k == K_EDIT) { push_undo(); track_set_icon(&song.t[sel], icon_pal[ic_cur] + 1);
        char b[32]; snprintf(b, sizeof b, "ICON %s", ic_cur ? icon_tab[icon_pal[ic_cur]].name : "AUTO"); toast(b); leave_screen(); iconing = 0; return; }
    int g = icon_group(ic_cur), a = icon_grp_at[g], b = icon_grp_at[g + 1], col = (ic_cur - a) % ICON_COLS;
    if (k == K_L1 || k == K_R1) ic_cur = icon_grp_at[(g + (k == K_R1 ? 1 : ICON_GRP_N - 1)) % ICON_GRP_N];
    if (k == K_LEFT || k == K_RIGHT) ic_cur = clampi(ic_cur + (k == K_RIGHT ? 1 : -1), 0, ICON_PAL_N - 1);   // the ends of a group lead to the next one
    if (k == K_DOWN) {
        if (ic_cur + ICON_COLS < b) ic_cur += ICON_COLS;
        else if ((ic_cur - a) / ICON_COLS < (b - a - 1) / ICON_COLS) ic_cur = b - 1;
        else if (g + 1 < ICON_GRP_N) ic_cur = clampi(b + col, 0, icon_grp_at[g + 2] - 1);
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
        int at = tab == T_PRJ ? 3 : tab == T_MIX ? 2 : tab == T_GRID && scening ? 1 : 0, to = (at + (d > 0 ? 1 : 3)) % 4;
        if (to == 3) prj_back = T_GRID;
        set_tab(to == 3 ? T_PRJ : to == 2 ? T_MIX : T_GRID); scening = to == 1; if (to == 2 && sel < NSLOT) mcol = sel;
        return;
    }
    int i = 0, cur = tab == T_PAT ? T_SEQ : tab; while (i < 4 && order[i] != cur) i++;
    int nt = order[((i < 4 ? i : 0) + (d > 0 ? 1 : 3)) % 4];
    if (nt == T_FX) fx_unit = sel;
    set_tab(nt);
}
static void tab_project(void) {          // F1+F2: in and out of PROJECT
    if (tab == T_PRJ) set_tab(prj_back); else { prj_back = tab; set_tab(T_PRJ); }
}
static void rec_store(int s, int slice, int note, double pos) {
    Track *t = &song.t[s]; Pattern *p = cur_pat(s);
    if (pos < rec_from) pos = rec_from;  // rounding can put a hit a hair before the take's start
    if (rec_free[s]) {
        if (rec_n < (int)(sizeof rec_ev / sizeof *rec_ev)) rec_ev[rec_n++] = (RecEv){(int8_t)s, (int8_t)note, (int8_t)slice, pos, -1, (int16_t)rec_tag_now};
        return;
    }
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
    push_undo();
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
static int rl_t, rl_n = 60, rl_v0, rl_n0 = 50, rl_snap = 8, rl_len = 8, rl_heard, rl_chord;   // cursor (tick, pitch), view, grid and last length in ticks
static const int rl_snaps[6] = {1, 2, 4, 8, 16, 32};   // 1/128 .. 1/4
static const int rl_d32[6] = {1, 2, 4, 8, 16, 32};
enum { RL_COLS = 32, RL_ROWS = 14, RL_TAG = 301, RL_CHORD_TAG = 310 };
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
    if (engine_playing() && song.t[sel].on) return;
    engine_release_key(RL_TAG); engine_trig_key(sel, 1.f, (float)note_semis(sel, n), -1, RL_TAG); flash[sel] = SDL_GetTicks(); rl_heard = 1;
}
static void roll_view(void) {            // the cursor stays on screen
    int w = RL_COLS * rl_snap;
    if (rl_t < rl_v0) rl_v0 = rl_t; if (rl_t >= rl_v0 + w) rl_v0 = rl_t - w + rl_snap; if (rl_v0 < 0) rl_v0 = 0;
    if (rl_n < rl_n0) rl_n0 = rl_n; if (rl_n >= rl_n0 + RL_ROWS) rl_n0 = rl_n - RL_ROWS + 1; rl_n0 = clampi(rl_n0, 0, 128 - RL_ROWS);
}
static void roll_open(void) {
    Pattern *p = cur_pat(sel); int d = rl_d32[p->div % 6] * RT;
    rl_snap = d > 32 ? 32 : d; rl_len = d; rl_t = 0; rl_v0 = 0; rl_n = roll_base(sel);
    for (int i = 0; i < NNOTE; i++) if (p->roll[i].len) { rl_n = p->roll[i].note; break; }
    rl_n0 = clampi(rl_n - RL_ROWS / 2, 0, 128 - RL_ROWS); rolling = 1;
}
static void roll_mode(void) {            // MODE: STEPS <> ROLL. An empty roll takes the steps as notes; both are kept
    Pattern *p = cur_pat(sel);
    if (!p->mode && track_nslices(sel)) { toast("SLICED PAD: STEPS ONLY"); return; }
    edit_begin();
    if (!p->mode && !roll_any(p)) {
        int d = rl_d32[p->div % 6] * RT, base = roll_base(sel), acid = song.t[sel].type == 2;
        for (int st = 0; st < p->len; st++) if (p->trig[st]) {
            int n = p->note[st] == NOTE_NONE ? base : p->note[st], fl = acid && p->slice[st] >= 0 ? p->slice[st] : 0;
            int k = roll_add(p, st * d, (fl & ACID_SLIDE) ? d + d / 2 : d, n, p->vel[st]);
            if (k < 0) break;
            p->roll[k].prob = (uint8_t)p->prob[st]; if (fl & ACID_ACCENT) p->roll[k].flags |= NF_ACC;
        }
    }
    p->mode = !p->mode; toast(p->mode ? "PIANO ROLL" : "STEPS");
}
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
static void roll_key(int k, int down) {
    Pattern *p = cur_pat(sel); int T = roll_ticks(p);
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
    if (k == K_Y) { roll_leave(); if (ui_cat == PC_PATTERN) patmenu_open(); return; }   // UI v2: back to PATTERN's list
    if (k == K_PLAY) { play_key(); return; }
    if (held[K_SHIFT] && k == K_L2) {
        undo(); p = cur_pat(sel);
        if (!p->mode) { roll_leave(); return; }   // the undo took the roll away
        rl_t = clampi(rl_t, 0, (roll_ticks(p) - 1) / rl_snap * rl_snap); return;
    }
    if (k == K_L1 || k == K_R1) {        // the grid: 1/128 .. 1/4
        int g = 0; while (g < 5 && rl_snaps[g] < rl_snap) g++;
        g = clampi(g + (k == K_R1 ? 1 : -1), 0, 5); rl_snap = rl_snaps[g]; rl_t = rl_t / rl_snap * rl_snap; rl_v0 = rl_v0 / rl_snap * rl_snap; roll_view(); return;
    }
    int i = roll_find(p, rl_t, rl_n);
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) {
        used[K_EDIT] = used[K_OPT] = 1; roll_chord_off();
        if (i >= 0) { push_undo(); roll_del(p, i); } return;
    }
    if (k == K_EDIT && held[K_X]) {      // X+EDIT: a triad on the cursor (ACID: ACCENT)
        used[K_EDIT] = used[K_X] = 1;
        if (song.t[sel].type == 2) { if (i >= 0) { push_undo(); p->roll[i].flags ^= NF_ACC; } return; }
        push_undo();
        int st = i >= 0 ? p->roll[i].start : rl_t, ln = i >= 0 ? p->roll[i].len : rl_len, v = i >= 0 ? p->roll[i].vel : 100;
        if (i < 0) roll_add(p, st, ln, rl_n, v);
        for (int s2 = 2; s2 <= 4; s2 += 2) { int n = roll_up(rl_n, s2); if (n <= 127 && roll_find(p, st, n) < 0) roll_add(p, st, ln, n, v); }
        return;
    }
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
    if (held[K_SHIFT]) { rl_t = clampi(rl_t + dx * RL_COLS * rl_snap, 0, last); rl_n = clampi(rl_n + 12 * dy, 0, 127);
        rl_v0 += dx * RL_COLS * rl_snap; rl_n0 += 12 * dy; roll_view(); return; }
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
    if (ty == FX_COMP && r == 2) { *lo = 0; *hi = NSLOT; return &song.mbx[fx_unit][c][5]; }                     // COMPRESSOR KEY: 0 itself, n = pad n (sidechain)
    return NULL;
}
static int fx_multi_row(int c, int r) { int lo, hi; return fx_side(c, r, &lo, &hi) != NULL; }
static void fx_cycle(FxSlot *f, int d) { edit_begin(); fx_set_type(f, ((f->type + d) % FX_N + FX_N) % FX_N); mbx_clear(f); fx_turn_on(f); }
static int8_t *unit_param(int unit, int k, int *lo, int *hi, int *def) {   // returns, master: their parameter k
    static const int8_t rvd[8] = {50, 40, 50, 10, 100, 20, 80, 0}, dld[8] = {7, 35, 70, 20, 0, 100, 80, 0}, msd[12] = {0, 70, 20, 30, 40, 0, 1, 89, 30, 0, 0, 30};
    if (unit == NSLOT) { rv_range(k, lo, hi); *def = rvd[k]; return &song.mix.rv[k]; }
    if (unit == NSLOT + 1) { dl_range(k, lo, hi); *def = dld[k]; return &song.mix.dl[k]; }
    ms_range(k, lo, hi); *def = msd[k]; return &song.mix.ms[k];
}
static int unit_nparams(int unit) { return unit == NSLOT + 2 ? 9 : 8; }   // master: the duck rows come later
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
static void line(int x0, int y0, int x1, int y1, int t, Col c); static void draw_footer(const char *hint); static void ctext(int cx, int y, int sc, Col c, const char *s);
static void duck_init(int s) {            // the window's values for a pad that has none: exactly the duck it had
    int8_t *dx = song.dkx[s]; if (dx[DX_MARK] == DX_INIT) return;
    memset(dx, 0, 8); dx[DX_MODE] = DM_NOTE; dx[DX_LEN] = 2; dx[DX_GRID] = 2; dx[DX_THRESH] = 40; dx[DX_SHAPE] = 0; dx[DX_MARK] = DX_INIT;
    memcpy(song.dkc[s], dk_shape[0], 16);
}
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
    case DF_SHAPE: { int v = dx[DX_SHAPE] == DK_USER ? (d > 0 ? 0 : DK_NSHAPE - 1) : (dx[DX_SHAPE] + d + DK_NSHAPE) % DK_NSHAPE; dx[DX_SHAPE] = (int8_t)v; memcpy(song.dkc[sel], dk_shape[v], 16); break; }
    }
}
static void duck_key(int k, int down) {
    static int combo;                        // EDIT was used with another button: its release is no tap
    if (!down) { if (k == K_EDIT && !combo && dk_row == 1) { edit_begin(); duck_field(dk_col, 1, 0); } return; }   // a tap on a field: its next value
    used[k] = 1;
    if (k == K_EDIT && !held[K_OPT]) { combo = 0; return; }
    if (held[K_EDIT] || k == K_EDIT) combo = 1;
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_OPT && !held[K_EDIT]) { listen(sel); return; }
    if (k == K_Y || k == K_MENU) { leave_screen(); ducking = 0; return; }
    PadMix *pm = &song.mix.pad[sel]; int8_t *dx = song.dkx[sel]; uint8_t *cv = song.dkc[sel];
    int arrow = k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT;
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) {      // OPT+EDIT: the point back up, the field's default
        edit_begin();
        if (dk_row == 0) { cv[dk_pt] = 100; dx[DX_SHAPE] = DK_USER; }
        else switch (dk_col) { case DF_MODE: dx[DX_MODE] = DM_NOTE; break; case DF_SRC: pm->spare[DK_SRC] = 0; break; case DF_AMT: pm->spare[DK_AMT] = 0; break; case DF_LEN: dx[DX_LEN] = 2; break;
            case DF_RATE: dx[DX_GRID] = 2; dx[DX_THRESH] = 40; break; case DF_SMOOTH: dx[DX_SMOOTH] = 0; break; case DF_SHAPE: dx[DX_SHAPE] = 0; memcpy(cv, dk_shape[0], 16); break; }
        return;
    }
    if (k == K_L1 || k == K_R1) { sel = (sel + (k == K_R1 ? 1 : NSLOT - 1)) % NSLOT; mcol = sel; duck_init(sel); return; }   // the next pad's duck
    if (arrow && held[K_EDIT]) {
        edit_begin(); int d = k == K_RIGHT || k == K_UP ? 1 : -1;
        if (dk_row == 1) { duck_field(dk_col, d, k == K_UP || k == K_DOWN); return; }
        if (k == K_UP || k == K_DOWN) cv[dk_pt] = (uint8_t)clampi(cv[dk_pt] + d * 5, 0, 100);
        else { int v = cv[dk_pt]; dk_pt = clampi(dk_pt + d, 0, 15); cv[dk_pt] = (uint8_t)v; }   // drawn sideways: the next point takes this level
        dx[DX_SHAPE] = DK_USER; return;
    }
    if (!arrow) return;
    if (dk_row == 1) {                   // UI v2 (A20): the fields as cards in two columns
        if (k == K_LEFT) { if (dk_col & 1) dk_col--; } else if (k == K_RIGHT) { if (!(dk_col & 1) && dk_col + 1 < DF_N) dk_col++; }
        else if (k == K_DOWN) { if (dk_col + 2 < DF_N) dk_col += 2; } else if (dk_col >= 2) dk_col -= 2; else dk_row = 0;
        return; }
    if (k == K_UP) dk_row = 0; else if (k == K_DOWN) dk_row = 1;
    else if (dk_row == 0) dk_pt = clampi(dk_pt + (k == K_RIGHT ? 1 : -1), 0, 15);
    else dk_col = (dk_col + (k == K_RIGHT ? 1 : DF_N - 1)) % DF_N;
}
static void draw_duck(void) {
    const PadMix *pm = &song.mix.pad[sel]; const int8_t *dx = song.dkx[sel]; const uint8_t *cv = song.dkc[sel];
    int mode = dx[DX_MODE] % DM_N, amt = pm->spare[DK_AMT], src = pm->spare[DK_SRC], onn = mode == DM_GRID ? amt > 0 : src > 0;
    enum { X0 = 16, Y0 = 122, BW = 992, BH = 360, PX = 84, PW = 880 };   // the box; the points run from PX over PW
    textf(16, 92, 2, C_WHITE, "DUCK"); textf(96, 92, 2, C_DIM, "PAD %02d %.15s", sel + 1, slot_name(sel) ? slot_name(sel) : "EMPTY");
    if (!onn) text(600, 92, 2, C_GREY, mode == DM_GRID ? "OFF: AMOUNT IS 0" : "OFF: CHOOSE A SOURCE");
    rect(X0, Y0, BW, BH, C_CELL, 1); frame(X0, Y0, BW, BH, 1, dk_row == 0 ? C_ORANGE : C_LINE);
    for (int q = 1; q < 4; q++) rect(X0 + 1, Y0 + BH * q / 4, BW - 2, 1, C_LINE, 1);
    text(X0 + 6, Y0 + 6, 1, C_GREY, "FULL"); text(X0 + 6, Y0 + BH - 14, 1, C_GREY, "SILENT");
    int px[16], py[16], ry[16];
    for (int i = 0; i < 16; i++) { px[i] = PX + PW * i / 15; py[i] = Y0 + BH - 14 - (BH - 28) * cv[i] / 100; ry[i] = Y0 + BH - 14 - (BH - 28) * (100 - amt * (100 - cv[i]) / 100) / 100; }
    for (int i = 0; i < 16; i++) rect(px[i], py[i], 1, Y0 + BH - 14 - py[i], C_LINE, 1);
    for (int i = 0; i < 15; i++) line(px[i], ry[i], px[i + 1], ry[i + 1], 2, onn ? C_PLAY : C_GREY);      // what the pad's level really does (with AMOUNT)
    for (int i = 0; i < 15; i++) line(px[i], py[i], px[i + 1], py[i + 1], 2, C_WHITE);                   // the curve itself
    for (int i = 0; i < 16; i++) { int on = dk_row == 0 && i == dk_pt; rect(px[i] - (on ? 7 : 4), py[i] - (on ? 7 : 4), on ? 15 : 9, on ? 15 : 9, on ? C_ORANGE : C_WHITE, 1); }
    if (dk_row == 0) { char b[8]; snprintf(b, sizeof b, "%d", cv[dk_pt]); int tx = px[dk_pt] - (int)strlen(b) * 8, ty = py[dk_pt] - 30; if (ty < Y0 + 4) ty = py[dk_pt] + 14; text(tx < X0 + 4 ? X0 + 4 : tx, ty, 2, C_ORANGE, b); }
    { float g = 1, v = engine_duck_view(sel, &g);          // live: where the dip is now, and the level
      if (v >= 0) { int x = PX + (int)(PW * v); rect(x, Y0 + 2, 2, BH - 4, C_PLAY, 1); }
      int gh = (int)((BH - 4) * (g < 0 ? 0 : g > 1 ? 1 : g)); rect(X0 + BW - 14, Y0 + 2, 10, BH - 4, C_BG, 1); rect(X0 + BW - 14, Y0 + BH - 2 - gh, 10, gh, C_PLAY, 1); }
    // the fields
    static const char *const fn[DF_N] = {"TRIGGER", "SOURCE", "AMOUNT", "LENGTH", "", "SMOOTH", "SHAPE"};
    char v[DF_N][12]; int used_f[DF_N] = {1, mode != DM_GRID, 1, 1, mode != DM_NOTE, 1, 1};
    snprintf(v[DF_MODE], 12, "%s", dk_mode_name[mode]);
    if (src) snprintf(v[DF_SRC], 12, "PAD %d", src); else snprintf(v[DF_SRC], 12, "--");
    snprintf(v[DF_AMT], 12, "%d%%", amt); snprintf(v[DF_LEN], 12, "%s", dk_len_name[dx[DX_LEN] % DK_NLEN]);
    if (mode == DM_GRID) snprintf(v[DF_RATE], 12, "%s", dk_len_name[dx[DX_GRID] % DK_NLEN]); else if (mode == DM_AUDIO) snprintf(v[DF_RATE], 12, "%dDB", (int)((dx[DX_THRESH] - 100) * 0.6f)); else snprintf(v[DF_RATE], 12, "--");
    if (dx[DX_SMOOTH]) snprintf(v[DF_SMOOTH], 12, "%dMS", dx[DX_SMOOTH] / 2 > 0 ? dx[DX_SMOOTH] / 2 : 1); else snprintf(v[DF_SMOOTH], 12, "OFF");
    snprintf(v[DF_SHAPE], 12, "%s", dx[DX_SHAPE] == DK_USER ? "USER" : dk_shape_name[dx[DX_SHAPE] % DK_NSHAPE]);
    for (int c = 0; c < DF_N; c++) {
        int x = 16 + c * 142, y = Y0 + BH + 12, on = dk_row == 1 && dk_col == c;
        rect(x, y, 138, 72, on ? C_ORANGE : C_CELL, 1);
        text(x + 8, y + 10, 1, on ? C_BG : C_DIM, c == DF_RATE ? (mode == DM_GRID ? "EVERY" : mode == DM_AUDIO ? "THRESHOLD" : "(NOTES)") : fn[c]);
        text(x + 8, y + 34, 2, on ? C_BG : used_f[c] ? C_WHITE : C_GREY, v[c]);
    }
    static const char *const help[DF_N] = {
        "NOTE: EVERY NOTE OF THE SOURCE PAD STARTS THE DIP.  SOUND: ITS SOUND PASSING THE THRESHOLD.  GRID: THE TEMPO, NO SOURCE NEEDED.",
        "THE PAD WHOSE NOTES OR SOUND START THE DIP (NOT USED ON THE GRID).",
        "HOW DEEP THE DIP GOES. THE BLUE LINE SHOWS THE LEVEL THAT RESULTS.",
        "HOW LONG THE CURVE LASTS, FROM ITS START TO ITS END.",
        "GRID: HOW OFTEN THE DIP STARTS.  SOUND: HOW LOUD THE SOURCE MUST GET TO START IT.",
        "ROUNDS THE CORNERS OF THE DIP, SO A STEEP CURVE DOES NOT CLICK.",
        "FILLS THE CURVE FROM A READY SHAPE. MOVING A POINT MAKES IT YOUR OWN (USER).",
    };
    const char *h = dk_row == 0 ? "THE CURVE: 16 POINTS OF LEVEL ALONG THE DIP, LEFT = ITS START. TOP = FULL LEVEL, BOTTOM = SILENT." : help[dk_col];
    { char l1[124] = "", l2[72] = ""; int n = (int)strlen(h), cut = n; if (n > 120) { cut = 120; while (cut > 0 && h[cut] != ' ') cut--; }   // two lines at the most
      snprintf(l1, sizeof l1, "%.*s", cut, h); if (cut < n) snprintf(l2, sizeof l2, "%.71s", h + cut + 1);
      text(16, Y0 + BH + 96, 1, C_GREY, l1); text(16, Y0 + BH + 110, 1, C_GREY, l2); }
    draw_footer(dk_row == 0 ? "<> POINT  EDIT+^v LEVEL  EDIT+<> DRAW  OPT+EDIT FULL  Y BACK" : "EDIT+ARROWS CHANGE  UP CURVE  L1/R1 PAD  OPT+EDIT DEFAULT");
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
    if (fx_unit < NSLOT && fx_page) { if (k == K_LEFT || k == K_RIGHT) lcol = (lcol + (k == K_RIGHT ? 1 : NLFO - 1)) % NLFO; else lrow = clampi(lrow + (k == K_DOWN ? 1 : -1), 0, 6); return; }   // 7 rows (LR_N, below)
    if (fx_unit < NSLOT) {
        if (frow == 8) { if (k == K_UP) { frow = 7; fcol = fcol >= 2; } else if (k == K_LEFT || k == K_RIGHT) fcol = clampi(fcol + (k == K_RIGHT ? 1 : -1), 0, 3); return; }
        if (k == K_RIGHT) { if (!fx_sub && fx_multi_row(fcol, frow)) fx_sub = 1; else if (fcol == 0) { fcol = 1; fx_sub = 0; } return; }
        if (k == K_LEFT) { if (fx_sub) fx_sub = 0; else if (fcol == 1) { fcol = 0; fx_sub = fx_multi_row(0, frow); } return; }
        frow = clampi(frow + (k == K_DOWN ? 1 : -1), 0, 8);
        if (fx_sub && !fx_multi_row(fcol, frow)) fx_sub = 0;
        if (frow == 8) fcol = fcol * 2;
        return;
    }
    int n = unit_nparams(fx_unit), half = (n + 1) / 2;   // two columns of parameters
    if (k == K_LEFT || k == K_RIGHT) { fcol = k == K_RIGHT; if (fcol && frow >= n - half) frow = n - half - 1; return; }
    frow = clampi(frow + (k == K_DOWN ? 1 : -1), 0, (fcol ? n - half : half) - 1);
}
static void lfo_value(int d, int big);
static void fx_value(int d, int big) {
    if (fx_unit < NSLOT && fx_page) { lfo_value(d, big); return; }
    if (fx_unit >= NSLOT) {
        int half = (unit_nparams(fx_unit) + 1) / 2, kk = fcol * half + frow, lo, hi, def;
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
    if (frow == 0) { act_insert_type_step(fx_unit, fcol, d); return; }
    if (frow == 1) { act_insert_on(fx_unit, fcol, d > 0); return; }
    if (!f->type) return;
    if (fx_sub && fx_multi_row(fcol, frow)) { int lo2, hi2; int8_t *g = fx_side(fcol, frow, &lo2, &hi2); edit_begin(); *g = (int8_t)clampi(*g + d * (big && hi2 > 12 ? 6 : 1), lo2, hi2); return; }
    act_insert_param_step(fx_unit, fcol, frow - 2, d, big);
}
static void fx_tap(void) {
    if (fx_unit < NSLOT && fx_page) { if (lrow == 0) learn_lfo_start(fx_unit, lcol); else lfo_value(1, 0); return; }   // TARGET: point at a knob
    if (fx_unit >= NSLOT) { int half = (unit_nparams(fx_unit) + 1) / 2, kk = fcol * half + frow, lo, hi, def; int8_t *v = unit_param(fx_unit, kk, &lo, &hi, &def);
        if (hi == 1) { edit_begin(); *v = !*v; } return; }
    if (frow == 8) return;
    FxSlot *f = &song.mix.pad[fx_unit].ins[fcol];
    if (frow == 0) fx_cycle(f, 1);
    else if (frow == 1) { edit_begin(); if (f->on) f->on = 0; else fx_turn_on(f); }
}
static void fx_delete(void) {
    if (fx_unit < NSLOT && fx_page && lrow == 0) { char b[24]; edit_begin(); song.lfo[fx_unit][lcol].target = 0; snprintf(b, sizeof b, "LFO %d UNLINKED", lcol + 1); toast(b); return; }   // 2026-11-12, user
    if (fx_unit < NSLOT && fx_page) { edit_begin(); memset(&song.lfo[fx_unit][lcol], 0, sizeof(Lfo)); toast("LFO OFF"); return; }
    if (fx_unit >= NSLOT) { int half = (unit_nparams(fx_unit) + 1) / 2, kk = fcol * half + frow, lo, hi, def; int8_t *v = unit_param(fx_unit, kk, &lo, &hi, &def); edit_begin(); *v = (int8_t)def; return; }
    PadMix *pm = &song.mix.pad[fx_unit]; edit_begin();
    if (frow == 8) { if (fcol == 0) pm->send_a = 0; else if (fcol == 1) pm->send_b = 0; else if (fcol == 2) song.t[fx_unit].pan = 0; else song.t[fx_unit].vol = 75; return; }
    FxSlot *f = &pm->ins[fcol];
    if (fx_sub && fx_multi_row(fcol, frow)) { int lo2, hi2; *fx_side(fcol, frow, &lo2, &hi2) = 0; return; }
    if (frow <= 1) { fx_set_type(f, FX_NONE); mbx_clear(f); } else if (f->type) f->p[frow - 2] = fx_def[f->type][frow - 2];
}
// ---------- MOD page of the FX tab: 4 LFOs per pad (2026-11-09; user: in the FX tab for now, Y switches FX/MOD) ----------
static int lcol, lrow;                   // the LFO column and its row
enum { LR_TARGET, LR_SHAPE, LR_SYNC, LR_RATE, LR_DEPTH, LR_TRIG, LR_PHASE, LR_N };
static const char *const lr_name[LR_N] = {"TARGET", "SHAPE", "CLOCK", "RATE", "DEPTH", "START", "PHASE"};
static const char *const ls_name[LS_N] = {"SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "S&H", "SMOOTH", "DUCK"};
static const char *const gb_pn[14] = {"OSC 1", "OSC 2", "TUNE 2", "DETUNE", "MIX", "NOISE", "CUTOFF", "RESO", "ENV", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "GLIDE"};
static const char *const ab_pn[11] = {"WAVE", "TUNE", "CUTOFF", "RESO", "ENV MOD", "DECAY", "ACCENT", "DIST TYPE", "DRIVE", "DIST TONE", "DIST MIX"};
static const char *const cl_pn[21] = {"POSITION", "SPRAY", "SIZE", "DENSITY", "SHIMMER", "MOTION", "SIZE RND", "WINDOW", "REVERSE", "GRAIN PITCH", "GRAIN FINE", "PITCH RND", "INTERVALS", "SYNC", "TIME RND", "WIDTH", "SCAN", "FREEZE", "ATTACK", "RELEASE", "TONE"};
static int lfo_targets(int s, int *out) {   // what an LFO on this pad can move, in order
    int n = 0, ty = song.t[s].type; out[n++] = LT_NONE;
    if (ty != 3) out[n++] = LT_PITCH;
    out[n++] = LT_VOL; out[n++] = LT_PAN; out[n++] = LT_SENDA; out[n++] = LT_SENDB;
    int ne = ty == 1 ? SP_N : ty == 2 ? AP_N : ty == 3 ? CP_N + CX_N : 0;
    for (int k = 0; k < ne; k++) out[n++] = LT_ENG + k;
    int nd = ty == 1 ? GX_N : ty == 2 ? AX_N : 0;   // the DEEP page's knobs
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
    else if (tg >= LT_ENG + LT_DEEP) { int k = tg - LT_ENG - LT_DEEP; snprintf(b, 24, "%s", ty == 1 && k < GX_N ? par_name[P_GX + k] : ty == 2 && k < AX_N ? par_name[P_AX + k] : "?"); }
    else { int k = tg - LT_ENG; snprintf(b, 24, "%s", ty == 1 && k < 14 ? gb_pn[k] : ty == 2 && k < 11 ? ab_pn[k] : ty == 3 && k < 21 ? cl_pn[k] : "?"); }
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
    case LR_TRIG: if (L->retrig >= 2) snprintf(b, 24, "PAD %d NOTE", L->retrig - 1); else snprintf(b, 24, "%s", L->retrig ? "EACH NOTE" : "FREE RUN"); break;
    default: snprintf(b, 24, "%d", L->phase);
    }
}
static void fx_unit_step(int d) { fx_unit = (fx_unit + d + NSLOT + 3) % (NSLOT + 3); if (fx_unit < NSLOT) sel = fx_unit; frow = 0; fcol = 0; }

// ---------- PRESETS (2026-11-09): factory catalogue (src/presets.h from tools/mkpresets.py) + the user's own (presets.cfg) ----------
#define PRESET_FILE "presets.cfg"
#define NUSERP 96
typedef struct { char name[16]; uint8_t engine; int8_t v[PRESET_N]; } UserPreset;
static UserPreset upre[NUSERP]; static int nupre;
static int presetting, pr_cur, pr_top, pr_heard;   // the window, cursor, first row shown, OPTION note
static Track pr_keep; static int8_t pr_keep_cx[16], pr_keep_x[PRESET_N];   // the pad as it was when the window opened: Y puts it back
static void presets_load(void) {
    nupre = 0; FILE *f = fopen(PRESET_FILE, "r"); if (!f) return;
    char line[640];
    while (nupre < NUSERP && fgets(line, sizeof line, f)) {
        UserPreset *u = &upre[nupre]; int e, n = 0, pos = 0; char nm[32];
        if (sscanf(line, "%d %31[^|]|%n", &e, nm, &pos) < 2 || e < 1 || e > 3) continue;
        memset(u, 0, sizeof *u); u->engine = (uint8_t)e; snprintf(u->name, sizeof u->name, "%s", nm);
        const char *p = line + pos; int v, k;
        while (n < PRESET_N && sscanf(p, "%d%n", &v, &k) == 1) { u->v[n++] = (int8_t)clampi(v, -128, 127); p += k; }
        nupre++;
    }
    fclose(f);
}
static void presets_save(void) {
    static char b[NUSERP * 400]; int n = 0;
    for (int i = 0; i < nupre; i++) {
        n += snprintf(b + n, sizeof b - n, "%d %s|", upre[i].engine, upre[i].name);
        int last = PRESET_N; while (last > 24 && !upre[i].v[last - 1]) last--;   // a preset with no DEEP data stays short
        for (int k = 0; k < last; k++) n += snprintf(b + n, sizeof b - n, " %d", upre[i].v[k]);
        n += snprintf(b + n, sizeof b - n, "\n");
    }
    write_whole(PRESET_FILE, b, (size_t)n);
}
static void preset_apply(int s, const int8_t *v) {   // values are clamped to the engine's ranges: a hand-edited file stays safe
    Track *t = &song.t[s]; int lo, hi;
    if (t->type == 1) { for (int k = 0; k < SP_N; k++) { sp_range(k, &lo, &hi); t->sp[k] = (int8_t)clampi(v[k], lo, hi); } t->voice = v[14] ? 1 : 0; t->sp[SP_MARK] = SP_INIT; gx_defaults(song.gx[s]); memset(song.gp[s], 0, sizeof song.gp[s]);
        if (v[24 + GX_MARK] == GX_INIT) { for (int k = 0; k < GX_N; k++) { gx_range(k, &lo, &hi); song.gx[s][k] = (int8_t)clampi(v[24 + k], lo, hi); }   // its GRAND DEEP and cables
            for (int c = 0; c < 8; c++) { int o = (uint8_t)v[56 + 2 * c], i = (uint8_t)v[57 + 2 * c]; if (o && i && o < 32 && i < 32) { song.gp[s][c][0] = (uint8_t)o; song.gp[s][c][1] = (uint8_t)i; } } } }
    else if (t->type == 2) { for (int k = 0; k < AP_N; k++) { ap_range(k, &lo, &hi); t->ap[k] = (int8_t)clampi(v[k], lo, hi); } t->ap[AP_MARK] = AP_INIT; ax_defaults(song.ax[s]);
        if (v[24 + AX_MARK] == AX_INIT) for (int k = 0; k < AX_N; k++) { ax_range(k, &lo, &hi); song.ax[s][k] = (int8_t)clampi(v[24 + k], lo, hi); } }
    else if (t->type == 3) { for (int k = 0; k < CP_N; k++) t->cp[k] = (int8_t)clampi(v[k], 0, 100); t->cp[CP_MARK] = CP_INIT;
        for (int k = 0; k < CX_N; k++) { cx_range(k, &lo, &hi); song.cx[s][k] = (int8_t)clampi(v[6 + k], lo, hi); } song.cx[s][CX_MARK] = CX_INIT; }
}
static void preset_capture(int s, int8_t *v) {
    const Track *t = &song.t[s]; memset(v, 0, PRESET_N);
    if (t->type == 1) { memcpy(v, t->sp, SP_N); v[14] = t->voice; memcpy(v + 24, song.gx[s], 32); memcpy(v + 56, song.gp[s], 16); }
    else if (t->type == 2) { memcpy(v, t->ap, AP_N); memcpy(v + 24, song.ax[s], 16); }
    else if (t->type == 3) { memcpy(v, t->cp, CP_N); memcpy(v + 6, song.cx[s], CX_N); }
}
// The list for this pad's engine: row 0 = SAVE CURRENT, then the user's presets, then the factory ones
static int pr_list(int *idx, int max) {
    int e = song.t[sel].type, n = 0; idx[n++] = -1;
    for (int i = 0; i < nupre && n < max; i++) if (upre[i].engine == e) idx[n++] = 1000 + i;
    for (int i = 0; i < NPRESET && n < max; i++) if (presets[i].engine == e) idx[n++] = i;
    return n;
}
static void pr_restore(void) { Track *t = &song.t[sel]; memcpy(t->sp, pr_keep.sp, 16); memcpy(t->ap, pr_keep.ap, 12); memcpy(t->cp, pr_keep.cp, 7); t->voice = pr_keep.voice; memcpy(song.cx[sel], pr_keep_cx, 16);
    if (t->type == 1) { memcpy(song.gx[sel], pr_keep_x + 24, 32); memcpy(song.gp[sel], pr_keep_x + 56, 16); } else if (t->type == 2) memcpy(song.ax[sel], pr_keep_x + 24, 16); }
static void pr_try(void) {               // the row under the cursor sounds on the pad at once (Y gives the old sound back)
    int idx[NPRESET + NUSERP + 1], n = pr_list(idx, NPRESET + NUSERP + 1); pr_cur = clampi(pr_cur, 0, n - 1);
    int i = idx[pr_cur];
    if (i >= 1000) preset_apply(sel, upre[i - 1000].v); else if (i >= 0) preset_apply(sel, presets[i].v);
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
    int idx[NPRESET + NUSERP + 1], n = pr_list(idx, NPRESET + NUSERP + 1);
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
            const char *nm = i >= 1000 ? upre[i - 1000].name : presets[i].name; char b[40]; snprintf(b, sizeof b, "PRESET %s", nm); toast(b);
            preset_close(1);
        }
        return;
    }
    if (k == K_Y) { preset_close(0); toast("PRESET: AS BEFORE"); return; }
    if (k == K_PLAY) { play_key(); return; }
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) {   // OPT+EDIT on a user preset: delete it
        used[K_EDIT] = used[K_OPT] = 1; if (pr_heard) { engine_release_key(302); pr_heard = 0; }
        int i = idx[clampi(pr_cur, 0, n - 1)];
        if (i >= 1000) { int j = i - 1000; char b[40]; snprintf(b, sizeof b, "DELETED %s", upre[j].name);
            memmove(&upre[j], &upre[j + 1], sizeof *upre * (size_t)(nupre - j - 1)); nupre--; presets_save(); toast(b); pr_try(); }
        else toast("ONLY USER PRESETS CAN BE DELETED");
        return;
    }
    if (k == K_OPT) { if (!(engine_playing() && song.t[sel].on)) { engine_release_key(302); engine_trig_key(sel, 1.f, 0, -1, 302); pr_heard = 1; flash[sel] = SDL_GetTicks(); } return; }
    if (k == K_UP || k == K_DOWN || k == K_L1 || k == K_R1) {
        int d = k == K_DOWN ? 1 : k == K_UP ? -1 : 0;
        if (k == K_L1 || k == K_R1) {    // UI v2 (A12): L1/R1 between the two tabs, FACTORY and MINE
            int mine = idx[clampi(pr_cur, 0, n - 1)] < 0 || idx[clampi(pr_cur, 0, n - 1)] >= 1000, j = 0;
            if (mine) { while (j < n && (idx[j] < 0 || idx[j] >= 1000)) j++; if (j >= n) j = 0; }
            pr_cur = j; pr_try(); return;
        }
        if (0) {
            int i0 = idx[clampi(pr_cur, 0, n - 1)], c0 = i0 >= 0 && i0 < 1000 ? presets[i0].cat : -1, j = pr_cur;
            if (k == K_R1) { while (j < n - 1 && (idx[j] < 0 || idx[j] >= 1000 || presets[idx[j]].cat == c0)) j++; }
            else { j--; while (j > 0 && idx[j] >= 0 && idx[j] < 1000 && presets[idx[j]].cat == presets[idx[clampi(j - 1, 0, n - 1)]].cat && idx[j - 1] >= 0 && idx[j - 1] < 1000) j--; if (j < 0) j = 0; }
            pr_cur = j;
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
#include "ui.h"
#include "sound.h"
static void icon(int id, int x, int y, Col c); static int slot_icon(int i);   // pages.h: the pads' icons and meters, drawn further down
static float mx_show[NSLOT + 3]; static void mx_meters(void); static void meter_bar(int x, int y, int w, int h, float v);
#include "pages.h"
#include "simple.h"
static void key_event(int k, int down) {
    if (k < 0) return;
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
    if (down && k != K_X) x_tap_t = 0;   // X, another key, X is not a double tap
    seq_clamp();
    if (learning && learn_key(k, down)) return;
    if (exporting) { export_key(k, down); return; }
    if (asking) { ask_key(k, down); return; }
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
        if (st < p->len && p->trig[st]) { paint_anchor = -1; last_note[sel] = p->note[st] == NOTE_NONE ? 0 : p->note[st] + 1; }   // a lit step: EDIT+arrows change its note
        else { memcpy(paint_snap, p->trig, NSTEP); paint_anchor = st; paint_val = 1; }                 // an empty one: they paint
    }
    if (k == K_F1 || k == K_F2) {        // tabs on release, so that F1+F2 does not move first
        int other = k == K_F1 ? K_F2 : K_F1;
        if (down && held[other]) { used[K_F1] = used[K_F2] = 1; tab_project(); }
        else if (!down && !used[k]) { if (perform == 1 && held[K_SHIFT]) act_scene(k == K_F2 ? 1 : -1); else if (in_pad() && !perform) pad_step(k == K_F2 ? 1 : -1); else { ui_cat = -1; tab_step(k == K_F2 ? 1 : -1); } }
        return;
    }
    if (k == K_PLAY && held[K_SHIFT]) return;   // SHIFT = BTN_MODE: Knulli kills the app on a double press of this combo, leave it unbound
    if (k == K_PLAY && down && held[K_MENU]) {   // once: rec, twice: capture
        menu_down = 0;
        if (combo_t) { combo_t = 0; capture(); }
        else { combo_t = SDL_GetTicks() | 1; combo_pos = engine_posf(); }
        return;
    }
    if (k == K_MENU) {
        if (down) browse_hold = 0;
        if (down) menu_down = held[K_SHIFT] ? 0 : SDL_GetTicks();   // SHIFT+MENU is the system's d-pad toggle, not ours
        else { if (menu_down && SDL_GetTicks() - menu_down < 600) { int pm = perform; set_tab(T_GRID); perform = (pm + 1) % 3; if (perform == 1) live_scene = song.t[sel].cur; }   // GRID > PADS > KEYS
            menu_down = 0; }
        return;
    }
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
            browse_key(k);
        }
        return;
    }
    if (slicing) { slice_key(k, down); return; }
    if (rolling) { roll_key(k, down); return; }
    if (randing) { rnd_key(k, down); return; }
    if (picking) { pick_key(k, down); return; }
    if (presetting) { preset_key(k, down); return; }
    if (theming) { theme_key(k, down); return; }
    if (iconing) { icon_key(k, down); return; }
    if (ducking) { duck_key(k, down); return; }
    if (in_pad() && down && held[K_SHIFT] && k <= K_RIGHT) { used[K_SHIFT] = 1; cat_open(jump_cat(k)); return; }   // the jump
    if (hubbing) { hub_key(k, down); return; }
    if (patmenu) { patmenu_key(k, down); return; }
    if (modmenu) { modmenu_key(k, down); return; }
    if (scening) { scene_key(k, down); return; }
    if (recpage) { record_key(k, down); return; }
    if (ui_cat >= 0 && k == K_Y && tab != T_PAT && !pt_pick) {   // UI v2: Y goes back to the pad's hub, on release (Y+arrows BEND)
        if (!down && !used[K_Y]) { int c = ui_cat; if (c == PC_PATTERN) patmenu_open(); else if (c == PC_MOD) modmenu_open(); else { hub_open(); hub_cur = c; } return; }
        if (!down || !(tab == T_SEQ && srow >= 1 && srow <= 2)) return;
    }
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
    if (arrow && (held[K_Y] || held[K_X]) && tab == T_SEQ && srow >= 1 && srow <= 2 && is_auto(lane) && !browsing) {   // BEND the segment under the cursor (A19: X+arrows)
        used[K_Y] = used[K_X] = 1; int back = aut_point_back();
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
    if (held[K_SHIFT] && k == K_OPT) { used[K_OPT] = 1; copy_paste(0); return; }
    if (held[K_SHIFT] && k == K_EDIT) { used[K_EDIT] = 1; copy_paste(1); return; }
    if (held[K_SHIFT] && k == K_X && tab == T_SEQ) { used[K_X] = 1; rnd_open(); return; }   // RANDOM
    if ((k == K_EDIT && held[K_OPT]) || (k == K_OPT && held[K_EDIT])) { used[K_EDIT] = used[K_OPT] = 1; delete_action(); return; }
    if (k == K_OPT) {                    // sound on press, not on release
        if (tab == T_SEQ && srow >= 1 && srow <= 2 && !cur_pat(sel)->mode) step_listen(cur_step());
        else if (tab == T_GRID) { listen(sel); rec_hit(sel, -1, NOTE_NONE); }
        else { engine_trig_held(sel, 1.f, tab == T_SEQ && srow >= 3 ? song.lnote[sel] : 0, -1); opt_gate = 1; flash[sel] = SDL_GetTicks(); rec_hit(sel, -1, NOTE_NONE); }   // parameters etc.: only while held (the panel: its listening note)
        return;
    }
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
        if (pt_pick) { pt_pick = 0; toast("CABLE DROPPED"); return; }
        cl_deep = song.t[sel].type == 1 ? (cl_deep + 1) % 3 : !cl_deep; gb_cur = cl_deep ? 1 : 0; return; }
    if (tab == T_PAT && k == K_Y) { pat_close(0); return; }   // the pattern window: Y goes back
    if (tab == T_GRID && k == K_Y && held[K_SHIFT]) { freeze_key(); return; }
    if (tab == T_PRJ && k == K_Y) { if (jcol) jcol = 0; else { ui_cat = -1; set_tab(T_GRID); } return; }   // UI v2: Y is back, always
    if (v2_mixer() && mx_pg && k == K_Y) { mx_pg = 0; mcol = sel; mrow = MX_VOL; return; }                 // RETURNS, FREEZE: back to the faders
    if (tab == T_GRID && k == K_Y && gsel_mute()) return;
    if (tab == T_GRID && k == K_Y) { act_mute_toggle(sel); toast(song.t[sel].mute ? "MUTE ON" : "MUTE OFF"); return; }   // a live move: no undo step
    if (tab == T_GRID && k == K_X && held[K_SHIFT]) { naming_pad(); return; }
    if (tab == T_GRID && k == K_X) { icon_open(); return; }
    if (tab == T_MIX && k == K_Y && mcol < NSLOT && held[K_SHIFT]) { used[K_SHIFT] = 1; act_solo_toggle(mcol); return; }   // A21: SHIFT+Y solo
    if (v2_fx() && k == K_X) { act_insert_onoff(fx_unit, fx2_slot); return; }   // A17: the insert on or off (the tab dims)
    if (tab == T_MIX && k == K_Y && mcol < NSLOT) { act_mute_toggle(mcol); return; }   // live moves: no undo step
    if (tab == T_MIX && k == K_X && mcol < NSLOT) { act_solo_toggle(mcol); return; }
    if (v2_lfo() && (k == K_L1 || k == K_R1)) { lcol = (lcol + (k == K_R1 ? 1 : NLFO - 1)) % NLFO; return; }   // UI v2 (A18): the next LFO
    if (v2_auto() && (k == K_L1 || k == K_R1)) { lane = L_A1 + (lane - L_A1 + (k == K_R1 ? 1 : NAUTO - 1)) % NAUTO; return; }   // A19: the next lane
    if (v2_fx() && (k == K_L1 || k == K_R1)) { fx2_slot ^= 1; fx2_cur = -1; fx2_sync(); return; }   // UI v2 (A17): the other insert
    if (tab == T_FX && (k == K_L1 || k == K_R1)) { fx_unit_step(k == K_R1 ? 1 : -1); return; }
    if (tab == T_FX && k == K_Y && fx_unit < NSLOT) { fx_page = !fx_page; return; }   // FX <> MOD
    if (v2_mix() && (k == K_L1 || k == K_R1)) return;   // MIX of a pad: one page
    if (v2_unit() && (k == K_L1 || k == K_R1)) { fx_unit = NSLOT + (fx_unit - NSLOT + (k == K_R1 ? 1 : 2)) % 3; frow = fcol = 0; return; }
    if (v2_unit() && k == K_Y) { int u = fx_unit; set_tab(T_MIX); mx_pg = 1; mcol = u; mx_r = 0; mrow = MX_VOL; return; }   // back to RETURNS
    if (v2_mixer() && (k == K_L1 || k == K_R1)) { mixer2_page(k == K_R1 ? 1 : -1); return; }   // UI v2: PADS, RETURNS, FREEZE
    if (tab == T_MIX && (k == K_L1 || k == K_R1)) { mcol = k == K_R1 ? (mcol < NSLOT ? (mcol / 4 + 1) * 4 : 0) : (mcol < 4 ? NSLOT : mcol <= NSLOT ? (mcol - 1) / 4 * 4 : NSLOT); if (mcol > NSLOT + 2) mcol = 0; if (mcol < NSLOT) sel = mcol; return; }
    if (ui_cat == PC_SOUND && tab == T_SEQ && (k == K_L1 || k == K_R1)) { sound_page_step(k == K_R1 ? 1 : -1); return; }   // SOUND: the page
    if (tab == T_SEQ && (k == K_L1 || k == K_R1)) {   // the pad's own lanes, round (PATTERN: without A1-A4, they are MOD's)
        int l[L_N], n = lane_list(l), i = 0; if (ui_cat == PC_PATTERN) n -= NAUTO; while (i < n && l[i] != lane) i++;
        lane = l[((i < n ? i : 0) + (k == K_R1 ? 1 : n - 1)) % n]; return; }
}

// ---------- screens ----------
// The look: an instrument panel. Cells share their borders, the active thing is a solid plate with dark text, states are
// plates, hatching and chevrons, a sample is a small pixel icon. Figures are rare and quiet: only PROJECT and the theme preview.
static void hatch(int x, int y, int w, int h, int step, int band, Col c) {   // diagonal stripes inside the box
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255);
    for (int d = 0; d < w + h - 1; d += step) for (int k = 0; k < band && d + k < w + h - 1; k++) {
        int v = d + k, xa = v < w ? v : w - 1, yb = v < h ? v : h - 1;
        SDL_RenderDrawLine(R, x + xa, y + v - xa, x + v - yb, y + yb);
    }
}
static void line(int x0, int y0, int x1, int y1, int t, Col c);
static void route_arrow(int x, int y, Col c) { line(x, y, x + 14, y, 2, c); line(x + 14, y, x + 9, y - 5, 2, c); line(x + 14, y, x + 9, y + 5, 2, c); }
static void route_loop(int cx, int cy, Col c) {   // a closed arrow: the pattern plays on and on
    line(cx - 6, cy - 2, cx - 6, cy + 3, 2, c); line(cx - 6, cy + 3, cx - 3, cy + 6, 2, c); line(cx - 3, cy + 6, cx + 3, cy + 6, 2, c);
    line(cx + 3, cy + 6, cx + 6, cy + 3, 2, c); line(cx + 6, cy + 3, cx + 6, cy - 3, 2, c); line(cx + 6, cy - 3, cx + 3, cy - 6, 2, c);
    line(cx + 3, cy - 6, cx - 4, cy - 6, 2, c); line(cx - 4, cy - 6, cx - 1, cy - 9, 2, c); line(cx - 4, cy - 6, cx - 1, cy - 3, 2, c);
}
static int route_target(int s, int act, int cur) {   // what a follow action leads to, as far as it can be known: -1 stop, -2 chance, -3 itself
    const Track *t = &song.t[s]; int first = -1;
    for (int q = 0; q < NPAT; q++) if (pat_used(&t->pat[q])) { first = q; break; }
    switch (act) {
    case FA_STOP: return -1;
    case FA_RANDOM: case FA_OTHER: return -2;
    case FA_JUMP: return song.fol[s][cur][FO_JUMP] % NPAT;
    case FA_FIRST: return first >= 0 ? first : -3;
    case FA_NEXT: for (int k = 1; k <= NPAT; k++) { int q = (cur + k) % NPAT; if (pat_used(&t->pat[q])) return q == cur ? -3 : q; } return -3;
    case FA_PREV: for (int k = 1; k <= NPAT; k++) { int q = (cur - k + NPAT) % NPAT; if (pat_used(&t->pat[q])) return q == cur ? -3 : q; } return -3;
    }
    return -3;
}
static int route_token(int x, int y, int tg, Col c) {   // P2, P? or a stop square; returns the x after it
    char b[8];
    if (tg == -1) { rect(x + 2, y + 2, 12, 12, c, 1); return x + 18; }
    if (tg == -3) { route_loop(x + 8, y + 8, c); return x + 20; }
    if (tg == -2) snprintf(b, 8, "P?"); else snprintf(b, 8, "P%d", tg + 1);
    text(x, y, 2, c, b); return x + (int)strlen(b) * 16;
}
// The pattern a pad plays and where it goes: P1 then a loop sign (it plays on), P1 2/3 -> P2 (the follow action and how
// many loops are left before it), P1 -> P2 in the accent colour (queued by hand). full: also the OR action with its chance.
static int draw_route(int x, int y, int s, Col c, Col hi, int full) {
    const Track *t = &song.t[s]; char b[16]; snprintf(b, sizeof b, "P%d", t->cur + 1);
    text(x, y, 2, c, b); x += (int)strlen(b) * 16 + 4;
    if (t->queued >= 0) { route_arrow(x, y + 8, hi); return route_token(x + 20, y, t->queued, hi); }
    const uint8_t *fo = song.fol[s][t->cur];
    if (!fo[FO_A] && !fo[FO_B]) { route_loop(x + 8, y + 8, c); return x + 20; }
    int after = fo[FO_AFTER] ? fo[FO_AFTER] : 1;
    if (after > 1 && full) { char n[12]; if (t->on && engine_playing()) snprintf(n, sizeof n, "%d/%d", engine_fol_n(s) % after + 1, after); else snprintf(n, sizeof n, "X%d", after);
        text(x, y + 4, 1, c, n); x += (int)strlen(n) * 8 + 4; }
    route_arrow(x, y + 8, c); x = route_token(x + 20, y, fo[FO_A] ? route_target(s, fo[FO_A], t->cur) : -3, c);
    if (full && fo[FO_B] && fo[FO_BCH]) { char o[16]; snprintf(o, sizeof o, "OR %d%%", fo[FO_BCH]); text(x + 10, y + 4, 1, c, o); x = route_token(x + 14 + (int)strlen(o) * 8, y, route_target(s, fo[FO_B], t->cur), c); }
    return x;
}
static void chevrons(int x, int y, int h, int n, Col c) {   // >>> the clip plays
    for (int i = 0; i < n; i++, x += h / 2 + 4) for (int r = 0; r < h; r++)
        rect(x + (r < h / 2 ? r : h - 1 - r), y + r, 3, 1, c, 1);
}
static void plate(int x, int y, int w, int h, int cut, Col c) {   // a solid label, its upper right corner cut
    for (int r = 0; r < cut && r < h; r++) rect(x, y + r, w - (cut - r), 1, c, 1);
    if (h > cut) rect(x, y + cut, w, h - cut, c, 1);
}
static int tag(int x, int y, Col bg, Col fg, const char *s) {     // words on a plate, returns its width
    int w = (int)strlen(s) * 16 + 12; plate(x, y, w, 24, 5, bg); text(x + 6, y + 4, 2, fg, s); return w;
}
static void barcode(int x, int y, int w, int h, unsigned seed, Col c) {
    for (int a = 0; a < w;) { seed = seed * 1103515245u + 12345u; int bar = 1 + (seed >> 16) % 4, gap = 1 + (seed >> 20) % 3; if (a + bar > w) break; rect(x + a, y, bar, h, c, 1); a += bar + gap; }
}
static void cell_lines(int x, int y, int cw, int ch, int nx, int ny) {   // the panel: one line between two cells
    for (int i = 0; i <= nx; i++) rect(i == nx ? x + i * cw - 1 : x + i * cw, y, 1, ny * ch, C_LINE, 1);
    for (int j = 0; j <= ny; j++) rect(x, y + j * ch, nx * cw, 1, C_LINE, 1);
}
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
static void draw_header(void) {
    ui_top();
    rect(16, 44, 992, 1, C_LINE, 1);
    const Sample *s = engine_sample(sel); char nb[8]; snprintf(nb, sizeof nb, "%02d", sel + 1);
    int name_on = tab == T_SEQ && srow < 0 && scol == 0 && !browsing && !slicing && !perform;
    if (name_on) rect(16, 50, 376, 26, C_ORANGE, 1);
    else plate(16, 51, 40, 24, 5, C_WHITE);
    text(22, 55, 2, C_BG, nb);
    (void)s; textf(64, 55, 2, name_on ? C_BG : C_WHITE, "%.18s", slot_name(sel) ? slot_name(sel) : "--");
    if (slot_bad[sel]) text(370, 55, 2, name_on ? C_BG : C_RED, "!");
    {   // the pattern: a place of its own on the name row (SEQ+INST, RIGHT from the name)
        int pon = (tab == T_SEQ && srow < 0 && scol == 1 && !browsing && !slicing && !perform) || tab == T_PAT;
        if (pon) rect(392, 50, 396, 26, tab == T_PAT ? C_DIM : C_ORANGE, 1);   // the whole field, up to the meter
        int ex; if (perform) { char pb[8]; snprintf(pb, 8, "P%d", song.t[sel].cur + 1); text(400, 55, 2, C_DIM, pb); ex = 400 + (int)strlen(pb) * 16; }   // PADS/KEYS: the tag needs the row
        else ex = draw_route(400, 55, sel, pon ? C_BG : C_DIM, pon ? C_BG : C_ORANGE, !rec);   // the pattern and where it goes (short during REC)
        int w = engine_playing() ? engine_want(sel) : -1;
        if (!perform && ((song.t[sel].on && w != 0) || (w >= 0 && SDL_GetTicks() / 250 % 2))) chevrons(ex + 10, 57, 12, 2, pon ? C_BG : C_PLAY);
    }
    if (perform) tag(476, 51, C_ORANGE, C_BG, perform == 2 ? "KEYS" : "PADS");
    if (perform == 1 && !rec) {          // PADS: the tonality of the selected pad
        textf(568, 48, 2, C_DIM, "%s %s", note_name[track_key(sel)], scale_name[track_scale(sel)]);
        text(568, 67, 1, song.t[sel].scale || song.t[sel].key ? C_ORANGE : C_GREY, song.t[sel].scale || song.t[sel].key ? "PAD" : "PROJECT");
    }
    if (rec) { tag(568, 51, C_RED, C_BG, "REC"); }
    if (rec && engine_counting() >= 0) textf(640, 55, 2, C_RED, "COUNT %d", song.count_in - engine_counting());
    else if (rec_wait) textf(640, 55, 2, C_RED, "WAITING");   // armed: the first hit starts the take
    else if (rec) { int t = (int)((engine_posf() - rec_from) * 10 / bar32()); textf(640, 55, 2, C_RED, "%d.%d BARS", t / 10, t % 10); }
    static float meter; float pk = engine_peak(); meter = pk > meter ? pk : meter * 0.9f;
    text(800, 59, 1, C_DIM, "OUT");
    for (int i = 0, lit = (int)(22 * (meter > 1 ? 1 : meter) + 0.5f); i < 22; i++)    // a meter of segments
        rect(832 + i * 8, 56, 6, 14, i >= lit ? C_LINE : meter > 0.9f ? C_RED : C_PLAY, 1);
    rect(16, 82, 992, 1, C_LINE, 1);
    if (menu_down && SDL_GetTicks() - menu_down > 600) rect(0, 80, (int)((SDL_GetTicks() - menu_down) * W / 3000), 3, C_ORANGE, 1);
}
static int is_key_word(const char *w, int n) {   // the first word of a hint: a key gets a key cap
    static const char *const pre[] = {"EDIT", "OPT", "SHIFT", "MENU", "F1", "L1", "HOLD", 0}, *const whole[] = {"Y", "X", "XX", "X+<>", "<", "<>", "UP/DOWN", "UP:", "PADS", "ARROWS", 0};
    for (int i = 0; pre[i]; i++) if (!strncmp(w, pre[i], strlen(pre[i]))) return 1;
    for (int i = 0; whole[i]; i++) if ((int)strlen(whole[i]) == n && !strncmp(w, whole[i], n)) return 1;
    return 0;
}
static void draw_hint(const char *h) {   // same width as plain text: the cap grows into the double space; at most three
    int x = 20, y = H - 64, left = 3, more = ui_pairs(h) > 3;
    while (*h && left--) {
        const char *e = strstr(h, "  "); int n = e ? (int)(e - h) : (int)strlen(h), w = (int)strcspn(h, " ");
        if (w > n) w = n;
        char b[96]; int key = is_key_word(h, w);
        if (key) { snprintf(b, sizeof b, "%.*s", w, h); rect(x - 4, y - 4, w * 16 + 8, 24, C_LINE, 1); text(x, y, 2, C_WHITE, b); }
        else w = 0;
        snprintf(b, sizeof b, "%.*s", n - w, h + w); text(x + w * 16, y, 2, C_DIM, b);
        x += n * 16 + 32; h += n; while (*h == ' ') h++;
    }
    if (more) { rect(1008 - 160 - 4, y - 4, 5 * 16 + 8, 24, C_LINE, 1); text(1008 - 160, y, 2, C_WHITE, "SHIFT"); text(1008 - 80, y, 2, C_DIM, " MORE"); }
}
static void draw_footer(const char *hint) {
    snprintf(ui_hint, sizeof ui_hint, "%s", hint);
    if (ui_v2) { ui2_footer(hint); return; }   // a rebuilt screen: the mockups' footer
    Uint32 now = SDL_GetTicks();
    rect(16, H - 73, 992, 32, C_BG, 1);   // a window drawn over a screen replaces its hints, never mixes with them
    rect(16, H - 74, 992, 1, C_LINE, 1);
    if (msg_t && now - msg_t < 1500) { plate(16, H - 69, (int)strlen(msg) * 16 + 16, 26, 6, C_ORANGE); text(24, H - 64, 2, C_BG, msg); }
    else draw_hint(hint);
    textf(16, H - 32, 2, engine_load() >= 0.85f ? C_RED : C_GREY, "DSP %d%%  V%02d", (int)(engine_load() * 100), engine_voices());   // red: close to gaps in the sound
    // which screen of five: the path on top shows it now (UI v2)
    char b[48]; snprintf(b, sizeof b, "%d BPM%s", (int)(song.tempo * tempo_mul + 0.5f), song.metro ? " M" : "");
    int tx = 1008 - (int)strlen(b) * 16;     // right edge: state plate, beat lights, tempo
    text(tx, H - 32, 2, C_WHITE, b);
    int nb = song.beats > 0 ? song.beats : 4, beat = engine_playing() ? (int)(engine_pos32() / 8 % nb) : -1, bw = 84 / nb;
    for (int i = 0; i < nb; i++) rect(tx - 16 - (nb - i) * bw, H - 30, bw > 6 ? bw - 6 : bw, 12, i == beat ? C_ORANGE : C_LINE, 1);
    int sx = tx - 16 - nb * bw - 12 - 76;
    if (engine_playing()) tag(sx, H - 36, C_PLAY, C_BG, "PLAY"); else { frame(sx, H - 36, 76, 24, 1, C_GREY); text(sx + 6, H - 32, 2, C_DIM, "STOP"); }
}
static void draw_keys(void) {
    int n = track_nslices(sel), ts = track_scale(sel), tk = track_key(sel), sl = scale_len(ts);
    cell_lines(16, 90, 248, 150, 4, 3);
    for (int i = 0; i < NSLOT; i++) {
        int x = 16 + (i % 4) * 248, y = 90 + (i / 4) * 150, deg = i + koct * sl, tonic = ((deg % sl) + sl) % sl == 0;
        int pc = (tk + deg_semis(ts, deg) + 120) % 12;
        if (key_on[i]) rect(x + 1, y + 1, 247, 149, C_ORANGE, 1);
        else if (tonic) rect(x + 1, y + 1, 247, 28, C_DIM, 1);      // the tonic has a plate
        text(x + 12, y + 7, 2, key_on[i] || tonic ? C_BG : C_DIM, pad_label[cell_key[i]]);
        if (n) textf(x + 12, y + 56, 4, key_on[i] ? C_BG : C_WHITE, "S%d", ((i + koct * 12) % n + n) % n + 1);
        else { textf(x + 12, y + 56, 4, key_on[i] ? C_BG : C_WHITE, "%s", note_name[pc]);
            textf(x + 214, y + 7, 2, key_on[i] || tonic ? C_BG : C_DIM, "%d", ((deg % sl) + sl) % sl + 1); }
    }
    for (int pc = 0; pc < 12; pc++) {   // keyboard strip: scale notes light, tonic orange, sounding white
        int in = 0, on = 0;
        for (int d = 0; d < sl; d++) if ((tk + deg_semis(ts, d)) % 12 == pc) in = 1;
        for (int i = 0; i < NSLOT; i++) if (key_on[i] && (tk + deg_semis(ts, i + koct * sl) + 120) % 12 == pc) on = 1;
        int x = 16 + pc * 83;
        rect(x, 548, 79, 62, on ? C_WHITE : pc == tk ? C_ORANGE : in ? C_DIM : C_CELL, 1);
        text(x + 8, 586, 2, on || in || pc == tk ? C_BG : C_GREY, note_name[pc]);
    }
    { char b[32]; snprintf(b, sizeof b, "%s %s", note_name[tk], scale_name[ts]);   // the pad's tonality, and whose it is
      text(16, 626, 3, C_WHITE, b);
      int own = song.t[sel].scale || song.t[sel].key, x = 16 + (int)strlen(b) * 24 + 16;
      x += tag(x, 626, own ? C_ORANGE : C_DIM, C_BG, own ? "PAD" : "PROJECT") + 24;
      textf(x, 630, 2, C_DIM, "OCT %d", koct);
      text(1008 - 9 * 16, 630, 2, C_DIM, "MENU EXIT"); }
    draw_footer("PADS PLAY  SHIFT+<> SCALE  SHIFT+L1/R1 KEY  SHIFT+^v OCT");
}
// HOME > PADS (UI v2, mockup A01): twelve cards, the engine's stripe, number, name, engine, the clip's bar; the pattern's
// route and FRZ / M / ! marks small (user, 2026-11-13). EDIT opens the pad, an empty one asks for an engine; single clips
// start in PADS (MENU).
static void draw_keys2(void);
static void draw_grid(void) {
    Uint32 now = SDL_GetTicks();
    if (perform == 2) { draw_keys2(); return; }
    enum { GX = 26, GY = 86, CW = 230, CH = 193, DX = 247, DY = 210 };
    static const char *const eng_tag[4] = {"SMP", "GRAND", "ACID", "CLOUD"};   // as the mockup writes them
    for (int i = 0; i < NSLOT; i++) {
        int x = GX + (i % 4) * DX, y = GY + (i / 4) * DY;
        Track *t = &song.t[i]; Uint32 age = now - flash[i]; int snd = has_sound(i);
        Col ec = C_ENG(t->type), ink = t->mute ? C_DIM : C_WHITE;
        if (!snd) {                      // empty: a dark place with a plus, or the file it misses
            if (i == sel) rframe(x - 4, y - 4, CW + 8, CH + 8, 11, 3, perform ? C_DIM : C_WHITE, C_BG);
            rframe(x, y, CW, CH, 8, 2, C_LINE, C_BG);
            if (slot_want[i][0]) { ftext(x + 25, y + 60, FT_MB, C_RED, "MISSING"); char b[16]; snprintf(b, sizeof b, "%.12s", file_base(slot_want[i])); ftext(x + 25, y + 92, FT_S, C_DIM, b); }
            else ftext(x + CW / 2 - fv2[FT_LB].w / 2, y + CH / 2 - fv2[FT_LB].h / 2, FT_LB, C_GREY, "+");
        } else {
            if (i == sel) rframe(x - 4, y - 4, CW + 8, CH + 8, 11, 3, perform ? C_DIM : C_WHITE, C_BG);   // the selected card: a white rounded outline
            Col bg = flash[i] && age < 180 ? mix(C_CELL, ec, 120 - (int)age * 120 / 180) : gsel_in(i) && i != sel ? C_SELBG : C_CELL;
            rrect(x, y, CW, CH, 8, bg);
            rect(x, y, 9, CH, t->mute ? mix(ec, C_BG, 150) : ec, 1);   // the engine's stripe
            char b[24]; snprintf(b, sizeof b, "%02d", i + 1); ftext(x + 25, y + 12, FT_M, C_DIM, b);
            snprintf(b, sizeof b, "%.10s", slot_name(i)); ftext(x + 25, y + 42, FT_XB, ink, b);
            ftext(x + 25, y + 94, FT_M, t->mute ? C_DIM : ec, eng_tag[t->type & 3]);
            icon(slot_icon(i), x + CW - 46, y + 86, t->mute ? C_DIM : ink);   // the pad's icon (X), by the engine's name
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
    char missing[96]; snprintf(missing, sizeof missing, "MISSING %.32s  %s", file_base(slot_want[sel]), engine_sample(sel) ? "EDIT OPEN" : "EDIT LOAD");
    draw_footer(perform ? "PAD CLIP/HIT  SHIFT+PAD OTHER  PLAY PAD CLIP"
                        : gsel_on() ? "PLAY ALL SELECTED  Y MUTE  SHIFT+Y FREEZE  ARROW ENDS IT"
                        : slot_want[sel][0] ? missing
                        : has_sound(sel) ? "EDIT OPEN  PLAY START  SHIFT MORE  Y MUTE  SHIFT+Y FREEZE  SHIFT+DPAD SEL"
                                             : "EDIT CHOOSE ENGINE  X ICON  MENU PADS");
}
static void slice_label(char *b, const Pattern *p, int i, int ns) {   // "16+": the step also plays what follows slice 16
    int k = (p->slice[i] < 0 ? i : p->slice[i]) % (ns ? ns : NSTEP);
    snprintf(b, 8, "%d%s", k + 1, track_follows(sel, k + 1) ? "+" : "");
}

// ---------- GRAND panel ----------
static void disc(int cx, int cy, int r, Col c) {
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255);
    for (int y = -r; y <= r; y++) { int w = (int)sqrtf((float)(r * r - y * y)); SDL_RenderDrawLine(R, cx - w, cy + y, cx + w, cy + y); }
}
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
static void ctext(int cx, int y, int sc, Col c, const char *s) { text(cx - (int)strlen(s) * 4 * sc, y, sc, c, s); }
// A Moog-style knob: scale marks from 7 to 5 o'clock, a black cap with a light rim, a white pointer.
static void gb_knob(int cx, int cy, int r, float f, int bipolar, Col ink, int on) {
    for (int i = 0; i <= 10; i++) {
        float a = (float)M_PI * (0.75f + 1.5f * i / 10);
        int l = i == 0 || i == 10 || (bipolar && i == 5) ? 9 : 5;
        line(cx + (int)(cosf(a) * (r + 4)), cy + (int)(sinf(a) * (r + 4)), cx + (int)(cosf(a) * (r + 4 + l)), cy + (int)(sinf(a) * (r + 4 + l)), 2, ink);
    }
    if (on) { ring_cut = (r + 18) * 7 / 10; ring_top = -(r + 18) * 3 / 4; gb_ring(cx, cy, r + 18, 4, C_ORANGE); ring_cut = 1 << 20; ring_top = -(1 << 20); }   // open at the bottom (its label) and the top (the label above)   // open where the scale is: the label below stays clear
    disc(cx, cy, r, GB_RIM); disc(cx, cy, r - 3, GB_KNOB);
    float a = (float)M_PI * (0.75f + 1.5f * f);
    line(cx + (int)(cosf(a) * r * 0.25f), cy + (int)(sinf(a) * r * 0.25f), cx + (int)(cosf(a) * (r - 4)), cy + (int)(sinf(a) * (r - 4)), 3, GB_LEGEND);
}
static float gb_frac(int p) {           // where a control's value sits, 0..1
    const Track *t = &song.t[sel];
    if (p >= P_PT) return 0;
    if (p >= P_GX) { int lo, hi; gx_range(p - P_GX, &lo, &hi); return (song.gx[sel][p - P_GX] - lo) / (float)(hi - lo); }
    if (p >= P_AX) { int lo, hi; ax_range(p - P_AX, &lo, &hi); return (song.ax[sel][p - P_AX] - lo) / (float)(hi - lo); }
    if (p >= P_CX) { int lo, hi; cx_range(p - P_CX, &lo, &hi); return (song.cx[sel][p - P_CX] - lo) / (float)(hi - lo); }
    if (p >= P_CL) return t->cp[p - P_CL] / 100.f;
    if (p >= P_AC) { int lo, hi; ap_range(p - P_AC, &lo, &hi); return (t->ap[p - P_AC] - lo) / (float)(hi - lo); }
    if (p >= P_SY) { int lo, hi; sp_range(p - P_SY, &lo, &hi); return (t->sp[p - P_SY] - lo) / (float)(hi - lo); }
    if (p >= P_SX && p < P_SX + SX_N) return song.smx[sel][p - P_SX] / 100.f;
    if (p == P_PITCH) return (t->pitch + 24) / 48.f;
    if (p == P_VOL) return t->vol / 100.f;
    return 0;
}
static void gb_wave_glyph(int x, int y, int w, int h, int wave, Col c) {   // one cycle, drawn
    int px = x, py = y + h / 2;
    for (int i = 0; i <= w; i++) {
        double ph = (double)i / w; float v = wave == 0 ? (float)(ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph) : wave == 1 ? (float)(2 * ph - 1) : (ph < (wave == 2 ? 0.5 : 0.15) ? 1.f : -1.f);
        int qy = y + h / 2 - (int)(v * h / 2);
        if (i) line(px, py, x + i, qy, 2, c);
        px = x + i; py = qy;
    }
}
static void gb_screen(int x, int y, int w, int h) { rect(x, y, w, h, GB_LCD, 1); frame(x, y, w, h, 2, GB_RIM); if (h > 40) for (int i = 1; i < 4; i++) rect(x + 2, y + h * i / 4, w - 4, 1, mix(GB_LCD, C_ORANGE, 30), 1); }
static void gb_section(int x, int w, Col bg, Col ink, const char *name) {
    rect(x, 296, w, 394, bg, 1);
    rect(x, 296, w, 26, mix(bg, ink, 40), 1);
    ctext(x + w / 2, 301, 2, ink, name);
}
static void draw_gb(int focus) {
    const Track *t = &song.t[sel]; const int8_t *sp = t->sp;
    int fp = focus >= 0 ? gb_ctl[focus].p : -1;
    // top strip: the instrument switch and a readout of the control under the cursor
    { int on = fp == P_SAMPLE;
      rect(16, 256, 384, 34, on ? C_ORANGE : GB_BLACK, 1);
      text(26, 265, 2, on ? C_BG : C_DIM, "INSTRUMENT");
      text(196, 265, 2, on ? C_BG : GB_LEGEND, "GRAND");
      if (!on) { text(366, 265, 2, C_DIM, "<>"); } }
    gb_screen(410, 256, 598, 34);
    if (fp >= 0 && fp != P_SAMPLE) { char b[64]; par_str(sel, fp, b, sizeof b); textf(424, 265, 2, C_ORANGE, "%-8s %s", par_name[fp], b); }
    else textf(424, 265, 2, mix(GB_LCD, C_ORANGE, 160), "%s", "EDIT+ARROWS: INSTRUMENT  Y: DEEP");
    // OSCILLATORS: two wave switches, TUNE 2 and DETUNE, a screen with the mixed wave
    gb_section(GB_SX0, 300, GB_OSC, GB_INK, "OSCILLATORS");
    { int sx = GB_SX0 + 12, sy = 332, sw = 276, sh = 64; gb_screen(sx, sy, sw, sh);
      double r2 = pow(2.0, (sp[SP_TUNE2] + sp[SP_DETUNE] / 100.0) / 12.0); float m = sp[SP_MIX] / 100.f; unsigned rn = 7;
      int px = 0, py = 0;
      for (int i = 0; i <= sw - 8; i++) {
          double ph = (double)i / (sw - 8) * 2, p2 = fmod(ph * r2, 1.0);
          rn = rn * 1103515245u + 12345u; float nz = ((int)(rn >> 16 & 0x7fff) / 16384.f - 1) * sp[SP_NOISE] / 100.f;
          float v = (1 - m) * sy_osc(sp[SP_OSC1], fmod(ph, 1.0), 1e-9) + m * sy_osc(sp[SP_OSC2], p2, 1e-9) + nz;
          v = v < -1.4f ? -1.4f : v > 1.4f ? 1.4f : v;
          int qx = sx + 4 + i, qy = sy + sh / 2 - (int)(v * (sh / 2 - 6) / 1.4f);
          if (i) line(px, py, qx, qy, 2, C_ORANGE);
          px = qx; py = qy;
      } }
    for (int o = 0; o < 2; o++) {          // wave switches: an LED by the chosen wave
        const GbCtl *g = &gb_ctl[gb_find(P_SY + (o ? SP_OSC2 : SP_OSC1))]; int on = fp == g->p, w = sp[o ? SP_OSC2 : SP_OSC1] & 3;
        ctext(g->x, 408, 2, GB_INK, o ? "OSC 2" : "OSC 1");
        if (on) frame(g->x - 62, 430, 132, 92, 3, C_ORANGE);
        for (int i = 0; i < 4; i++) {
            int y = 436 + i * 21;
            disc(g->x - 40, y + 8, 6, i == w ? GB_LED : mix(GB_OSC, GB_INK, 120));
            gb_wave_glyph(g->x - 24, y + 1, 30, 14, i, GB_INK);
            text(g->x + 16, y + 1, 2, GB_INK, i == 3 ? "PLS" : wave_name[i]);
        }
    }
    // MIXER
    gb_section(GB_SX1, 120, GB_BLACK, GB_LEGEND, "MIXER");
    { const GbCtl *g = &gb_ctl[gb_find(P_SY + SP_MIX)]; text(g->x - 52, g->y + 30, 2, GB_LEGEND, "1"); text(g->x + 38, g->y + 30, 2, GB_LEGEND, "2"); }   // OSC 1 .. OSC 2
    // FILTER: CUTOFF, RESO, ENV and the response
    gb_section(GB_SX2, 220, GB_FILTER, GB_INK, "FILTER");
    { int sx = GB_SX2 + 10, sy = 332, sw = 200, sh = 64; gb_screen(sx, sy, sw, sh);
      float fc = 20.f * exp2f(sp[SP_CUTOFF] / 100.f * 9.8f), k = sp[SP_RESO] / 100.f * 4.2f; int px = 0, py = 0;
      for (int i = 0; i <= sw - 8; i++) {
          float f = 20.f * powf(1000.f, (float)i / (sw - 8)), x = f / fc;   // 20 Hz .. 20 kHz
          float re = 1 - 6 * x * x + x * x * x * x + k, im = 4 * x - 4 * x * x * x;   // (1 + jx)^4 + k
          float db = 20 * log10f((1 + k) / sqrtf(re * re + im * im) + 1e-6f); db = db < -48 ? -48 : db > 18 ? 18 : db;
          int qx = sx + 4 + i, qy = sy + 6 + (int)((18 - db) / 66 * (sh - 12));
          if (i) line(px, py, qx, qy, 2, C_ORANGE);
          px = qx; py = qy;
      }
      }
    // ENVELOPE: four sliders and the shape
    gb_section(GB_SX3, 200, GB_BLACK, GB_LEGEND, "ENVELOPE");
    { int sx = GB_SX3 + 10, sy = 332, sw = 180, sh = 64; gb_screen(sx, sy, sw, sh);
      float ta = 0.15f + sp[SP_ATTACK] / 100.f, td = 0.15f + sp[SP_DECAY] / 100.f, tr = 0.15f + sp[SP_RELEASE] / 100.f, hold = 0.6f, tot = ta + td + hold + tr;
      float su = sp[SP_SUSTAIN] / 100.f; int x0 = sx + 4, w = sw - 8, b = sy + sh - 6, top = sy + 6, hgt = b - top;
      int xa = x0 + (int)(w * ta / tot), xd = xa + (int)(w * td / tot), xs = xd + (int)(w * hold / tot), xr = x0 + w;
      line(x0, b, xa, top, 2, C_ORANGE); line(xa, top, xd, b - (int)(su * hgt), 2, C_ORANGE);
      line(xd, b - (int)(su * hgt), xs, b - (int)(su * hgt), 2, C_ORANGE); line(xs, b - (int)(su * hgt), xr, b, 2, C_ORANGE);
      }
    for (int i = 0; i < 4; i++) {
        const GbCtl *g = &gb_ctl[gb_find(P_SY + SP_ATTACK + i)]; int on = fp == g->p;
        int top = 426, h = 200, v = sp[SP_ATTACK + i], cy = top + h - v * h / 100;
        if (on) frame(g->x - 22, top - 10, 44, h + 20, 3, C_ORANGE);
        for (int m = 0; m <= 10; m++) rect(g->x - 16, top + m * h / 10, m % 5 ? 6 : 10, 1, GB_LEGEND, 1);
        rect(g->x - 3, top, 6, h, GB_INK, 1); frame(g->x - 3, top, 6, h, 1, GB_RIM);
        rect(g->x - 13, cy - 7, 26, 14, GB_RIM, 1); rect(g->x - 13, cy - 1, 26, 2, GB_LEGEND, 1);
        ctext(g->x, 642, 2, on ? C_ORANGE : GB_LEGEND, (const char *[]){"A", "D", "S", "R"}[i]);
    }
    // OUTPUT: the voice lever, GLIDE, PITCH, VOLUME
    gb_section(GB_SX4, 136, GB_BLACK, GB_LEGEND, "OUTPUT");
    { int on = fp == P_VOICE, up = !t->voice, lx = GB_SX4 + 26;   // a lever: up MONO, down POLY
      if (on) frame(GB_SX4 + 4, 330, 128, 86, 3, C_ORANGE);
      text(lx + 24, 340, 2, up ? GB_LEGEND : C_GREY, "MONO"); text(lx + 24, 390, 2, !up ? GB_LEGEND : C_GREY, "POLY");
      rect(lx - 7, 344, 14, 56, GB_INK, 1); frame(lx - 7, 344, 14, 56, 1, GB_RIM);
      line(lx, 372, lx, up ? 348 : 396, 6, GB_LEGEND); disc(lx, up ? 348 : 396, 7, GB_LEGEND); }
    // the knobs and their names
    for (int i = 0; i < GB_N; i++) {
        const GbCtl *g = &gb_ctl[i]; if (g->kind != GB_KNOBC) continue;
        int on = i == focus, colour = g->p >= P_SY && (g->p - P_SY <= SP_DETUNE || (g->p - P_SY >= SP_CUTOFF && g->p - P_SY <= SP_ENV));
        Col ink = colour ? GB_INK : GB_LEGEND;
        int idle = g->p == P_SY + SP_GLIDE && t->voice;               // GLIDE in POLY: greyed out, it does nothing there
        if (idle) ink = C_GREY;
        int bip = g->p == P_SY + SP_TUNE2 || g->p == P_SY + SP_ENV || g->p == P_PITCH;
        gb_knob(g->x, g->y, g->r, gb_frac(g->p), bip, ink, on);
        if (idle) { disc(g->x, g->y, g->r - 3, mix(GB_KNOB, GB_BLACK, 255)); hatch(g->x - g->r + 4, g->y - g->r + 4, 2 * g->r - 8, 2 * g->r - 8, 5, 1, GB_RIM); }
        const char *nm = g->p == P_VOL ? "VOLUME" : g->p == P_SY + SP_TUNE2 ? "TUNE 2" : par_name[g->p];
        ctext(g->x, g->y + g->r + 16, 2, on ? (colour ? GB_INK : C_ORANGE) : ink, nm);
        if (on && colour) rect(g->x - (int)strlen(nm) * 8 - 4, g->y + g->r + 33, (int)strlen(nm) * 16 + 8, 3, C_ORANGE, 1);
    }
}

static void ab_section(int x, int w, Col bg, Col head, Col ink, const char *name) {
    rect(x, 296, w, 394, bg, 1); rect(x, 296, w, 26, head, 1); ctext(x + w / 2, 301, 2, ink, name);
}
static void draw_ab(int focus) {
    const Track *t = &song.t[sel]; const int8_t *ap = t->ap;
    int n; const GbCtl *c = pctl(&n); int fp = focus >= 0 && focus < n ? c[focus].p : -1;
    { int on = fp == P_SAMPLE;            // top strip: the instrument switch and a readout
      rect(16, 256, 384, 34, on ? C_ORANGE : GB_BLACK, 1);
      text(26, 265, 2, on ? C_BG : C_DIM, "INSTRUMENT"); text(196, 265, 2, on ? C_BG : GB_LEGEND, "ACID");
      if (!on) text(366, 265, 2, C_DIM, "<>"); }
    gb_screen(410, 256, 598, 34);
    if (fp >= 0 && fp != P_SAMPLE) { char b[64]; par_str(sel, fp, b, sizeof b); textf(424, 265, 2, C_ORANGE, "%-8s %s", fp == P_VOL ? "VOLUME" : par_name[fp], b); }
    else textf(424, 265, 2, mix(GB_LCD, C_ORANGE, 160), "%s", "EDIT+ARROWS: INSTRUMENT  Y: DEEP");
    // OSC: the wave lever and TUNING
    ab_section(AB_SX0, 150, AB_PANEL, AB_HEAD, AB_INK, "OSC");
    { const GbCtl *g = &c[gb_find(P_AC + AP_WAVE)]; int on = fp == g->p, up = !ap[AP_WAVE], lx = AB_SX0 + 30;
      if (on) frame(AB_SX0 + 4, 336, 142, 92, 3, C_ORANGE);
      text(lx + 24, 350, 2, up ? AB_INK : mix(AB_PANEL, AB_INK, 110), "SAW"); text(lx + 24, 404, 2, !up ? AB_INK : mix(AB_PANEL, AB_INK, 110), "SQR");
      gb_wave_glyph(lx + 80, 350, 30, 14, 1, AB_INK); gb_wave_glyph(lx + 80, 404, 30, 14, 2, AB_INK);
      rect(lx - 7, 354, 14, 60, AB_INK, 1); line(lx, 384, lx, up ? 358 : 410, 6, GB_LEGEND); disc(lx, up ? 358 : 410, 8, GB_RIM);
      ctext(AB_SX0 + 75, 438, 2, on ? C_ORANGE : AB_INK, "WAVE"); }
    // FILTER: the response, five knobs below
    ab_section(AB_SX1, 420, AB_PANEL, AB_HEAD, AB_INK, "FILTER");
    { int sx = AB_SX1 + 12, sy = 332, sw = 396, sh = 60; gb_screen(sx, sy, sw, sh);
      float fc = acid_cutoff(ap, song.ax[sel], 0, 0), k = ap[AP_RESO] / 100.f * 4.4f; int px = 0, py = 0;
      for (int i = 0; i <= sw - 8; i++) {
          float f = 20.f * powf(1000.f, (float)i / (sw - 8)), x = f / fc;
          float re = 1 - 6 * x * x + x * x * x * x + k, im = 4 * x - 4 * x * x * x;
          float hp = f / sqrtf(f * f + 150.f * 150.f);   // the resonance loop's high-pass thins the bottom
          float db = 20 * log10f((1 + k) * (k > 0.1f ? hp : 1) / sqrtf(re * re + im * im) + 1e-6f); db = db < -48 ? -48 : db > 18 ? 18 : db;
          int qx = sx + 4 + i, qy = sy + 6 + (int)((18 - db) / 66 * (sh - 12));
          if (i) line(px, py, qx, qy, 2, C_ORANGE);
          px = qx; py = qy;
      } }
    // DISTORTION: black, the type with LEDs, the lamp, DRIVE TONE MIX
    ab_section(AB_SX2, 280, GB_BLACK, mix(GB_BLACK, GB_LEGEND, 40), GB_LEGEND, "DISTORTION");
    { const GbCtl *g = &c[gb_find(P_AC + AP_DTYPE)]; int on = fp == g->p, ty = ap[AP_DTYPE] & 3;
      static const char *const nm[4] = {"DIODE", "TRANS", "TUBE", "FUZZ"};
      if (on) frame(AB_SX2 + 4, 334, 136, 118, 3, C_ORANGE);
      for (int i = 0; i < 4; i++) { int y = 342 + i * 26; disc(AB_SX2 + 22, y + 8, 6, i == ty ? GB_LED : GB_RIM); text(AB_SX2 + 38, y + 1, 2, i == ty ? GB_LEGEND : C_GREY, nm[i]); }
      // the lamp: a glass bulb on a socket, its filament glowing with DRIVE
      int cx = AB_SX2 + 208, cy = 392; float glow = ap[AP_DRIVE] / 100.f;
      rect(cx - 22, cy + 40, 44, 22, GB_RIM, 1); for (int i = 0; i < 3; i++) rect(cx - 22, cy + 44 + i * 6, 44, 2, GB_KNOB, 1);
      disc(cx, cy, 42, mix(GB_BLACK, AB_GLOW, (int)(glow * 70))); gb_ring(cx, cy, 42, 3, GB_RIM);
      if (glow > 0) disc(cx, cy + 4, 18, mix(GB_BLACK, AB_GLOW, (int)(60 + glow * 160)));
      Col fil = glow > 0 ? mix(GB_RIM, AB_GLOW, (int)(80 + glow * 175)) : GB_RIM;
      for (int i = 0; i < 4; i++) line(cx - 12 + i * 6, cy + 14, cx - 9 + i * 6, cy - 6, 2, fil);
      line(cx - 12, cy + 14, cx - 12, cy + 38, 2, GB_RIM); line(cx + 12, cy + 14, cx + 12, cy + 38, 2, GB_RIM);
      ctext(AB_SX2 + 72, 460, 2, on ? C_ORANGE : GB_LEGEND, "TYPE"); }
    // OUTPUT
    ab_section(AB_SX3, 130, AB_PANEL, AB_HEAD, AB_INK, "OUTPUT");
    for (int i = 0; i < n; i++) {         // the knobs and their names
        const GbCtl *g = &c[i]; if (g->kind != GB_KNOBC) continue;
        int on = i == focus, dark = g->p >= P_AC + AP_DRIVE && g->p <= P_AC + AP_DMIX;
        Col ink = dark ? GB_LEGEND : AB_INK;
        int bip = g->p == P_AC + AP_TUNE;
        gb_knob(g->x, g->y, g->r, gb_frac(g->p), bip, ink, on);
        const char *nm = g->p == P_VOL ? "VOLUME" : par_name[g->p];
        ctext(g->x, g->y + g->r + 16, 2, on && dark ? C_ORANGE : ink, nm);
        if (on && !dark) rect(g->x - (int)strlen(nm) * 8 - 4, g->y + g->r + 33, (int)strlen(nm) * 16 + 8, 3, C_ORANGE, 1);
    }
}

static void draw_abdeep(int focus) {     // ACID DEEP: the same aluminium, five sections
    int n; const GbCtl *c = pctl(&n); int fp = focus >= 0 && focus < n ? c[focus].p : -1; const int8_t *ax = song.ax[sel], *ap = song.t[sel].ap;
    { int on = fp == P_SAMPLE;
      rect(16, 256, 384, 34, on ? C_ORANGE : GB_BLACK, 1);
      text(26, 265, 2, on ? C_BG : C_DIM, "INSTRUMENT"); text(196, 265, 2, on ? C_BG : GB_LEGEND, "ACID");
      if (!on) text(366, 265, 2, C_DIM, "<>"); }
    gb_screen(410, 256, 598, 34);
    if (fp >= 0 && fp != P_SAMPLE) { char b[64]; par_str(sel, fp, b, sizeof b); textf(424, 265, 2, C_ORANGE, "%-8s %s", par_name[fp], b); }
    else textf(424, 265, 2, mix(GB_LCD, C_ORANGE, 160), "%s", "ACID DEEP   Y BACK");
    ab_section(AD_S0, AD_S1 - AD_S0, AB_PANEL, AB_HEAD, AB_INK, "SUB");
    ab_section(AD_S1, AD_S2 - AD_S1, AB_PANEL, AB_HEAD, AB_INK, "VOICE");
    ab_section(AD_S2, AD_S3 - AD_S2, AB_PANEL, AB_HEAD, AB_INK, "FILTER");
    ab_section(AD_S3, AD_S4 - AD_S3, AB_PANEL, AB_HEAD, AB_INK, "ACCENT");
    ab_section(AD_S4, 1008 - AD_S4, GB_BLACK, mix(GB_BLACK, GB_LEGEND, 40), GB_LEGEND, "OUTPUT");
    { static const int sx[4] = {AD_S1, AD_S2, AD_S3, AD_S4}; for (int i = 0; i < 4; i++) rect(sx[i] - 1, 296, 2, 394, AB_HEAD, 1); }   // the seams
    {   // a screen in ACCENT: the filter's decay after a plain note and after an accent
        int sx = AD_S3 + 10, sy = 600, sw = 130, sh = 70, b0 = sy + sh - 6; gb_screen(sx, sy, sw, sh);
        float dn = 0.2f + ap[AP_DECAY] / 100.f * 2.f, da = ax_accdec_s(ax), span = 1.2f; int px = 0, py = 0, qx = 0, qy = 0;
        for (int i = 0; i <= sw - 8; i++) { float t = span * i / (sw - 8); int x = sx + 4 + i;
            int y1 = b0 - (int)(expf(-t / (dn * 0.35f)) * (sh - 14)), y2 = b0 - (int)(expf(-t / (da * 0.35f)) * (sh - 14));
            if (i) { line(px, py, x, y1, 2, GB_LEGEND); line(qx, qy, x, y2, 2, C_ORANGE); } px = qx = x; py = y1; qy = y2; }
        text(sx + 4, sy + 4, 1, C_ORANGE, "ACC"); text(sx + 36, sy + 4, 1, GB_LEGEND, "NOTE"); }
    for (int i = 0; i < n; i++) {
        const GbCtl *g = &c[i]; int on = i == focus; if (g->p == P_SAMPLE) continue;
        int dark = g->p == P_AX + AX_CLIP; Col ink = dark ? GB_LEGEND : AB_INK;
        if (g->p == P_AX + AX_SUBOCT) {     // a lever: up -1 octave, down -2
            int up = !ax[AX_SUBOCT], lx = g->x - 22, y0 = g->y - 30;
            if (on) frame(g->x - 46, y0 - 10, 92, 92, 3, C_ORANGE);
            text(lx + 18, y0 - 4, 2, up ? AB_INK : mix(AB_PANEL, AB_INK, 110), "-1"); text(lx + 18, y0 + 46, 2, !up ? AB_INK : mix(AB_PANEL, AB_INK, 110), "-2");
            rect(lx - 7, y0, 14, 60, AB_INK, 1); line(lx, y0 + 30, lx, up ? y0 + 4 : y0 + 56, 6, GB_LEGEND); disc(lx, up ? y0 + 4 : y0 + 56, 8, GB_RIM);
            ctext(g->x, g->y + 56, 2, on ? C_ORANGE : AB_INK, "OCT"); continue;
        }
        if (g->p == P_AX + AX_SUBWAVE) {    // three LEDs
            static const char *const nm[3] = {"SQUARE", "SINE", "TRI"}; int x = g->x - 70, y0 = g->y - 40;
            if (on) frame(x - 8, y0 - 6, 156, 3 * 26 + 10, 3, C_ORANGE);
            for (int m = 0; m < 3; m++) { int y = y0 + m * 26; disc(x + 6, y + 8, 6, m == ax[AX_SUBWAVE] ? GB_LED : GB_RIM); text(x + 20, y + 1, 2, m == ax[AX_SUBWAVE] ? AB_INK : mix(AB_PANEL, AB_INK, 110), nm[m]); }
            ctext(g->x, y0 + 3 * 26 + 8, 2, on ? C_ORANGE : AB_INK, "WAVE"); continue;
        }
        int idle = g->p == P_AX + AX_PW && !ap[AP_WAVE];   // PULSE WIDTH does nothing on SAW
        int bip = g->p == P_AX + AX_RANGE || g->p == P_AX + AX_PW;
        gb_knob(g->x, g->y, g->r, gb_frac(g->p), bip, idle ? mix(AB_PANEL, AB_INK, 90) : ink, on);
        if (idle) hatch(g->x - g->r + 4, g->y - g->r + 4, 2 * g->r - 8, 2 * g->r - 8, 5, 1, GB_RIM);
        const char *nm = par_name[g->p];
        ctext(g->x, g->y + g->r + 14, 2, on ? C_ORANGE : idle ? mix(AB_PANEL, AB_INK, 110) : ink, nm);
    }
}
static void gd_leds(int cx, int cy, const char *const *nm, int n, int cur, int on, Col ink, Col bg, const char *label) {   // a switch with LEDs
    int x = cx - 56, y0 = cy - n * 13, mw = 0; for (int m = 0; m < n; m++) { int l = (int)strlen(nm[m]) * 16; if (l > mw) mw = l; }
    if (on) frame(x - 8, y0 - 6, 20 + mw + 12, n * 26 + 8, 3, C_ORANGE);   // as wide as its names: never over the next switch
    for (int m = 0; m < n; m++) { int y = y0 + m * 26; disc(x + 6, y + 8, 6, m == cur ? GB_LED : mix(bg, ink, 120)); text(x + 20, y + 1, 2, m == cur ? ink : mix(bg, ink, 110), nm[m]); }
    ctext(cx, y0 + n * 26 + 8, 2, on ? C_ORANGE : ink, label);
}
static void gd_lever(int cx, int cy, int up, const char *a, const char *b, int on, Col ink, Col bg, const char *label) {
    int lx = cx - 26, y0 = cy - 30, mw = (int)(strlen(a) > strlen(b) ? strlen(a) : strlen(b)) * 16;
    if (on) frame(lx - 14, y0 - 10, 14 + 20 + mw + 10, 92, 3, C_ORANGE);   // as wide as its words
    text(lx + 20, y0 - 4, 2, up ? ink : mix(bg, ink, 110), a); text(lx + 20, y0 + 46, 2, !up ? ink : mix(bg, ink, 110), b);
    rect(lx - 7, y0, 14, 60, GB_INK, 1); frame(lx - 7, y0, 14, 60, 1, GB_RIM);
    line(lx, y0 + 30, lx, up ? y0 + 4 : y0 + 56, 6, GB_LEGEND); disc(lx, up ? y0 + 4 : y0 + 56, 7, GB_LEGEND);
    ctext(cx, cy + 58, 2, on ? C_ORANGE : ink, label);
}
static void draw_gbdeep(int focus) {     // GRAND DEEP: the original's colours, section by section
    int n; const GbCtl *c = pctl(&n); int fp = focus >= 0 && focus < n ? c[focus].p : -1; const int8_t *gx = song.gx[sel];
    { int on = fp == P_SAMPLE;
      rect(16, 256, 384, 34, on ? C_ORANGE : GB_BLACK, 1);
      text(26, 265, 2, on ? C_BG : C_DIM, "INSTRUMENT"); text(196, 265, 2, on ? C_BG : GB_LEGEND, "GRAND");
      if (!on) text(366, 265, 2, C_DIM, "<>"); }
    gb_screen(410, 256, 598, 34);
    if (fp >= 0 && fp != P_SAMPLE) { char b[64]; par_str(sel, fp, b, sizeof b); textf(424, 265, 2, C_ORANGE, "%-11s %s", par_name[fp], b); }
    else textf(424, 265, 2, mix(GB_LCD, C_ORANGE, 160), "%s", "GRAND DEEP   Y BACK");
    gb_section(16, GD_SA - 16 - 4, GB_BLACK, GB_LEGEND, "MODULATION");
    { Col yel = {226, 192, 72}; gb_section(GD_SA, GD_S1 - GD_SA - 4, yel, GB_INK, "ARP"); }   // yellow, as the original's sequencer
    gb_section(GD_S1, GD_S2 - GD_S1 - 4, GB_OSC, GB_INK, "OSCILLATORS");
    gb_section(GD_S2, GD_S3 - GD_S2 - 4, GB_FILTER, GB_INK, "FILTER");
    gb_section(GD_S3, 1008 - GD_S3, GB_BLACK, GB_LEGEND, "VCA");
    { Col red = {168, 44, 38}; rect(GD_S3, 512, 1008 - GD_S3, 178, red, 1); rect(GD_S3, 512, 1008 - GD_S3, 26, mix(red, GB_LEGEND, 40), 1); ctext(GD_S3 + (1008 - GD_S3) / 2, 517, 2, GB_LEGEND, "SPRING"); }   // red, as on the original
    {   // the LFO's shape, the live dot drawn over it each frame
        int sx = 222, sy = 334, sw = 88, sh = 60, px = 0, py = 0; gb_screen(sx, sy, sw, sh);
        for (int i = 0; i <= sw - 8; i++) { double ph = (double)i / (sw - 8); float y;
            switch (gx[GX_LWAVE] % 5) { case 0: y = (float)(ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph); break; case 1: y = ph < 0.5 ? 1.f : -1.f; break; case 2: y = (float)(1 - 2 * ph); break; case 3: y = (float)(2 * ph - 1); break; default: y = ((i / 14) * 7919 % 200) / 100.f - 1; }
            int qx = sx + 4 + i, qy = sy + sh / 2 - (int)(y * (sh / 2 - 6)); if (i) line(px, py, qx, qy, 2, C_ORANGE); px = qx; py = qy; } }
    static const char *const wn[5] = {"TRI", "SQR", "SAW", "RAMP", "S&H"}, *const o1[4] = {"32'", "16'", "8'", "4'"}, *const o2[4] = {"16'", "8'", "4'", "2'"}, *const kb[3] = {"OFF", "HALF", "FULL"};
    for (int i = 0; i < n; i++) {
        const GbCtl *g = &c[i]; int on = i == focus, k = g->p - P_GX; if (g->p == P_SAMPLE) continue;
        int blue = k == GX_O1OCT || k == GX_O2OCT || k == GX_SYNC, green = k == GX_HP || k == GX_KBT, yellow = k >= GX_ARP && k <= GX_AGATE;
        Col yel = {226, 192, 72}, ink = blue || green || yellow ? GB_INK : GB_LEGEND, bg = blue ? GB_OSC : green ? GB_FILTER : yellow ? yel : GB_BLACK;
        switch (k) {
        case GX_LWAVE: gd_leds(g->x, g->y, wn, 5, gx[k] % 5, on, ink, bg, "WAVE"); break;
        case GX_O1OCT: gd_leds(g->x, g->y, o1, 4, gx[k] & 3, on, ink, bg, "OSC1"); break;
        case GX_O2OCT: gd_leds(g->x, g->y, o2, 4, gx[k] & 3, on, ink, bg, "OSC2"); break;
        case GX_KBT: gd_leds(g->x, g->y, kb, 3, gx[k] % 3, on, ink, bg, "KB TRACK"); break;
        case GX_SYNC: gd_lever(g->x, g->y, !gx[k], "OFF", "SYNC", on, ink, bg, "HARD SYNC"); break;
        case GX_VCA: gd_lever(g->x, g->y, !gx[k], "ENV", gx[k] == 2 ? "DRONE" : "GATE", on, ink, bg, "VCA"); break;
        case GX_ARP: gd_lever(g->x, g->y, !gx[k], "OFF", "ON", on, ink, bg, "ARP"); break;
        case GX_AMODE: { static const char *const am[4] = {"UP", "DOWN", "ORDER", "U-D"}; gd_leds(g->x, g->y, am, 4, gx[k] & 3, on, ink, bg, "MODE"); break; }
        case GX_AOCT: { char b[16]; snprintf(b, sizeof b, "%d OCT", gx[k] % 3 + 1); int x = g->x - 90;
            rect(x, g->y - 12, 180, 26, on ? C_ORANGE : GB_LCD, 1); frame(x, g->y - 12, 180, 26, 1, on ? C_ORANGE : GB_RIM);
            text(x + 6, g->y - 4, 1, on ? C_BG : GB_LEGEND, "RANGE"); text(x + 174 - (int)strlen(b) * 16, g->y - 7, 2, on ? C_BG : C_ORANGE, b); break; }
        case GX_LSYNC: { char b[16]; par_str(sel, g->p, b, sizeof b); int x = g->x - 120;
            rect(x, g->y - 12, 240, 26, on ? C_ORANGE : GB_LCD, 1); frame(x, g->y - 12, 240, 26, 1, on ? C_ORANGE : GB_RIM);
            text(x + 6, g->y - 4, 1, on ? C_BG : GB_LEGEND, "SYNC"); text(x + 234 - (int)strlen(b) * 16, g->y - 7, 2, on ? C_BG : C_ORANGE, b); break; }
        default: {
            int idle = k == GX_LRATE && gx[GX_LSYNC];   // RATE does nothing while synced
            gb_knob(g->x, g->y, g->r, gb_frac(g->p), 0, idle ? GB_RIM : ink, on);
            if (idle) hatch(g->x - g->r + 4, g->y - g->r + 4, 2 * g->r - 8, 2 * g->r - 8, 5, 1, GB_RIM);
            const char *nm = k == GX_LPITCH ? "PITCH" : k == GX_LCUT ? "CUTOFF" : k == GX_LPWM ? "PWM" : k == GX_HP ? "HP" : k == GX_SPRING ? "MIX" : k == GX_ARATE ? "RATE" : k == GX_AGATE ? "GATE" : par_name[g->p];
            ctext(g->x, g->y + g->r + 14, 2, on ? C_ORANGE : idle ? C_GREY : ink, nm);
        } }
    }
    text(30, 470, 1, mix(GB_BLACK, GB_LEGEND, 120), "AMOUNT TO");
}
static void draw_gbdeep_live(void) {     // where the LFO is now
    float v = engine_glfo(sel); if (v == 0 && !gx_lfo_on(song.gx[sel])) return;
    int sx = 222, sy = 334, sh = 60; disc(sx + 44, sy + sh / 2 - (int)(v * (sh / 2 - 6)), 5, GB_LEGEND);
}
static void pt_jack_pos(int j, int *x, int *y) {   // a jack's centre on the PATCH page
    *x = *y = 0; for (unsigned i = 0; i < sizeof pt_ctl / sizeof pt_ctl[0]; i++) if (pt_ctl[i].p == P_PT + j) { *x = pt_ctl[i].x; *y = pt_ctl[i].y; return; } }
static void pt_cable(int x0, int y0, int x1, int y1, Col c) {   // a cable: a sagging curve, dark edge and colour
    int sag = 50 + abs(x1 - x0) / 6, px = x0, py = y0;
    for (int i = 1; i <= 24; i++) { float t = i / 24.f, u = 1 - t; float cx = (x0 + x1) / 2.f, cy = (y0 > y1 ? y0 : y1) + sag;
        int qx = (int)(u * u * x0 + 2 * u * t * cx + t * t * x1), qy = (int)(u * u * y0 + 2 * u * t * cy + t * t * y1);
        line(px, py, qx, qy, 6, GB_BLACK); line(px, py, qx, qy, 3, c); px = qx; py = qy; }
    disc(x0, y0, 7, c); disc(x1, y1, 7, c);
}
static void draw_gbpatch(int focus) {     // PATCH: jacks by module, outputs boxed, the cables over them
    int n; const GbCtl *c = pctl(&n); int fp = focus >= 0 && focus < n ? c[focus].p : -1;
    { int on = fp == P_SAMPLE;
      rect(16, 256, 384, 34, on ? C_ORANGE : GB_BLACK, 1);
      text(26, 265, 2, on ? C_BG : C_DIM, "INSTRUMENT"); text(196, 265, 2, on ? C_BG : GB_LEGEND, "GRAND");
      if (!on) text(366, 265, 2, C_DIM, "<>"); }
    gb_screen(410, 256, 598, 34);
    if (fp >= 0 && fp != P_SAMPLE) { char b[96]; par_str(sel, fp, b, sizeof b); textf(424, 265, 2, C_ORANGE, "%.11s %.24s", par_name[fp], b); }
    else textf(424, 265, 2, mix(GB_LCD, C_ORANGE, 160), "%s", "PATCH   Y BACK TO THE PANEL");
    rect(16, 296, 992, 394, GB_BLACK, 1);
    static const struct { int x; const char *nm; Col bg; } col[8] = {{80, "MOD", {24, 24, 24}}, {200, "OSC IN", {150, 198, 228}}, {320, "OSC OUT", {150, 198, 228}}, {440, "MIXER", {24, 24, 24}},
        {560, "FILTER", {72, 158, 92}}, {680, "ENV VCA", {24, 24, 24}}, {800, "KEYS", {24, 24, 24}}, {930, "UTIL", {24, 24, 24}}};
    for (int i = 0; i < 8; i++) { int x = col[i].x - 56; rect(x, 300, 112, 386, col[i].bg, 1); int dark = col[i].bg.r > 100;
        rect(x, 300, 112, 24, mix(col[i].bg, dark ? GB_INK : GB_LEGEND, 40), 1); ctext(col[i].x, 305, 1, dark ? GB_INK : GB_LEGEND, col[i].nm); }
    for (int i = 0; i < n; i++) {
        const GbCtl *g = &c[i]; int on = i == focus; if (g->p == P_SAMPLE) continue;
        if (g->p == P_GX + GX_ATTEN) { gb_knob(g->x, g->y, g->r, gb_frac(g->p), 1, GB_LEGEND, on); ctext(g->x, g->y + g->r + 12, 1, on ? C_ORANGE : GB_LEGEND, "AMOUNT"); continue; }
        int j = g->p - P_PT, isin = j >= 16, dark = g->x == 200 || g->x == 320 || g->x == 560;
        Col ink = dark ? GB_INK : GB_LEGEND;
        if (!isin) rect(g->x - 26, g->y - 26, 52, 52, dark ? mix(GB_INK, GB_LEGEND, 60) : mix(GB_BLACK, GB_LEGEND, 60), 1);   // outputs: a box, as on the original
        if (on) gb_ring(g->x, g->y, 28, 4, C_ORANGE);
        disc(g->x, g->y, 16, GB_RIM); disc(g->x, g->y, 12, GB_KNOB); disc(g->x, g->y, 6, GB_BLACK);
        if (pt_pick == j && !isin) gb_ring(g->x, g->y, 22, 3, C_ORANGE);
        const char *nm = par_name[g->p]; char sh[16]; snprintf(sh, sizeof sh, "%.12s", nm);
        ctext(g->x, g->y + 30, 1, on ? C_ORANGE : ink, sh);
    }
    static const Col cc[NCABLE] = {{235, 60, 50}, {250, 190, 40}, {60, 160, 235}, {90, 210, 110}, {240, 120, 200}, {250, 140, 40}, {200, 200, 200}, {150, 110, 240}};
    for (int k = 0; k < NCABLE; k++) { const uint8_t *g = song.gp[sel][k]; if (!g[0] || !g[1]) continue;
        int x0, y0, x1, y1; pt_jack_pos(g[0], &x0, &y0); pt_jack_pos(16 + g[1], &x1, &y1); if (x0 && x1) pt_cable(x0, y0, x1, y1, cc[k]); }
    if (pt_pick && focus >= 0 && focus < n) { int x0, y0; pt_jack_pos(pt_pick, &x0, &y0); if (x0) pt_cable(x0, y0, c[focus].x, c[focus].y + 2, C_ORANGE); }   // the carried cable follows the cursor
}
static float cl_peaks[992]; static const Sample *cl_peak_s;   // the sample's outline, rebuilt when the sample changes
static void norm_plate(const GbCtl *g, int h, int on, Col bg, Col edge, Col ink, Col glow);
static void draw_cl(int focus) {
    const Track *t = &song.t[sel]; const Sample *smpl = engine_sample(sel);
    int n; const GbCtl *c = pctl(&n); int fp = focus >= 0 && focus < n ? c[focus].p : -1;
    { int on = fp == P_SAMPLE;
      rect(16, 256, 384, 34, on ? C_ORANGE : GB_BLACK, 1);
      text(26, 265, 2, on ? C_BG : C_DIM, "INSTRUMENT"); text(196, 265, 2, on ? C_BG : GB_LEGEND, "CLOUD");
      if (!on) text(366, 265, 2, C_DIM, "<>"); }
    gb_screen(410, 256, 598, 34);
    if (fp >= 0 && fp != P_SAMPLE) { char b[64]; par_str(sel, fp, b, sizeof b); textf(424, 265, 2, C_ORANGE, "%-8s %s", fp == P_VOL ? "VOLUME" : par_name[fp], b); }
    else textf(424, 265, 2, mix(GB_LCD, C_ORANGE, 160), "%s", "EDIT+ARROWS: INSTRUMENT");
    rect(16, 296, 992, 394, CL_PANEL, 1);
    { int on = fp == P_SRC;             // SOURCE: the sample the grains come from
      rect(16, 300, 640, 34, on ? C_ORANGE : CL_HEAD, 1);
      text(26, 309, 2, on ? C_BG : CL_INK, "SOURCE"); textf(140, 309, 2, on ? C_BG : GB_LEGEND, "%.30s", smpl ? smpl->name : "NO SAMPLE: EDIT HERE");
      text(670, 309, 2, mix(CL_PANEL, CL_INK, 150), on ? "<> NEXT FILE" : "Y: DEEP"); }
    for (int i = 0; i < n; i++) if (c[i].p == P_NORM) norm_plate(&c[i], 26, i == focus, CL_HEAD, CL_INK, CL_INK, GB_LEGEND);
    // the sample, with where the grains read (POSITION and SPRAY); the moving cloud is drawn live on top
    { int sx = 16 + 8, sy = 344, sw = 976, sh = 150; gb_screen(sx, sy, sw, sh);
      if (smpl && smpl->n > 0) {
          if (cl_peak_s != smpl) { cl_peak_s = smpl;
              for (int i = 0; i < 976 - 8; i++) { long a = (long)smpl->n * i / (976 - 8), b = (long)smpl->n * (i + 1) / (976 - 8); float m = 0;
                  for (long k = a; k < b; k += 16) { float v = fabsf(smpl->d[2 * k]); if (v > m) m = v; } cl_peaks[i] = m; } }
          float ng = norm_gain(sel);
          for (int i = 0; i < sw - 8; i++) { int h = (int)(cl_peaks[i] * ng * (sh / 2 - 8)); if (h > sh / 2 - 8) h = sh / 2 - 8; rect(sx + 4 + i, sy + sh / 2 - h, 1, 2 * h + 1, mix(GB_LCD, CL_INK, 110), 1); }
      } else ctext(sx + sw / 2, sy + sh / 2 - 8, 2, CL_INK, "LOAD A SAMPLE ON SOURCE");
    }
    for (int i = 0; i < n; i++) {         // the knobs, pale on slate
        const GbCtl *g = &c[i]; if (g->kind != GB_KNOBC) continue;
        int on = i == focus;
        gb_knob(g->x, g->y, g->r, gb_frac(g->p), 0, CL_INK, on);
        ctext(g->x, g->y + g->r + 16, 2, on ? C_ORANGE : CL_INK, g->p == P_VOL ? "VOLUME" : par_name[g->p]);
    }
    (void)t;
}

static void draw_cldeep(int focus) {
    cl_tables();                          // the windows are drawn from the engine's tables
    int n; const GbCtl *c = pctl(&n); int fp = focus >= 0 && focus < n ? c[focus].p : -1;
    { int on = fp == P_SAMPLE;
      rect(16, 256, 384, 34, on ? C_ORANGE : GB_BLACK, 1);
      text(26, 265, 2, on ? C_BG : C_DIM, "INSTRUMENT"); text(196, 265, 2, on ? C_BG : GB_LEGEND, "CLOUD");
      if (!on) text(366, 265, 2, C_DIM, "<>"); }
    gb_screen(410, 256, 598, 34);
    if (fp >= 0 && fp != P_SAMPLE) { char b[64]; par_str(sel, fp, b, sizeof b); textf(424, 265, 2, C_ORANGE, "%-9s %s", par_name[fp], b); }
    else textf(424, 265, 2, mix(GB_LCD, C_ORANGE, 160), "%s", "CLOUD DEEP   Y BACK");
    rect(16, 296, 992, 394, CL_PANEL, 1);
    static const char *const sec[5] = {"GRAIN", "PITCH", "TIME", "SPACE", "SHAPE"};
    for (int k = 0; k < 5; k++) {
        int x = 16 + 198 * k; rect(x + 2, 300, 194, 26, CL_HEAD, 1); ctext(x + 99, 305, 2, CL_INK, sec[k]);
        if (k) rect(x, 300, 1, 386, CL_HEAD, 1);
    }
    { const int8_t *cx = song.cx[sel]; int x0 = CD_X(0) - 60, y0 = 332, w = 120, h = 28;   // the grain window, drawn
      gb_screen(x0, y0, w, h); const float *wt = cl_win[cx[CX_WINDOW] % 5]; int px = 0, py = 0;
      for (int i = 0; i <= w - 8; i++) { int qx = x0 + 4 + i, qy = y0 + h - 4 - (int)(wt[i * CL_WIN / (w - 8)] * (h - 8)); if (i) line(px, py, qx, qy, 2, C_ORANGE); px = qx; py = qy; } }
    for (int i = 0; i < n; i++) {
        const GbCtl *g = &c[i]; int on = i == focus;
        if (g->kind == GB_SELECT && g->p == P_CX + CX_SHMODE) {   // INTERVALS: a word plate, it is a choice not an amount
            char b[24]; par_str(sel, g->p, b, sizeof b);
            ctext(g->x, g->y - 22, 2, on ? C_ORANGE : CL_INK, "INTERVALS");
            rect(g->x - 90, g->y - 2, 180, 24, on ? C_ORANGE : CL_HEAD, 1); ctext(g->x, g->y + 2, 2, on ? C_BG : GB_LEGEND, b);
            continue;
        }
        if (g->kind == GB_TOGGLE && g->p == P_CX + CX_FREEZE) {   // FREEZE: a lever, up ON, down OFF
            int up = song.cx[sel][CX_FREEZE] != 0, lx = g->x - 30;
            if (on) frame(g->x - 70, g->y - 36, 140, 90, 3, C_ORANGE);
            text(lx + 22, g->y - 30, 2, up ? GB_LEGEND : mix(CL_PANEL, CL_INK, 90), "ON"); text(lx + 22, g->y + 10, 2, !up ? GB_LEGEND : mix(CL_PANEL, CL_INK, 90), "OFF");
            rect(lx - 7, g->y - 28, 14, 56, GB_INK, 1); frame(lx - 7, g->y - 28, 14, 56, 1, GB_RIM);
            line(lx, g->y, lx, up ? g->y - 24 : g->y + 24, 6, GB_LEGEND); disc(lx, up ? g->y - 24 : g->y + 24, 7, up ? C_ORANGE : GB_LEGEND);
            ctext(g->x, g->y + 36, 2, on ? C_ORANGE : CL_INK, "FREEZE");   // y 600: below SCAN's name
            continue;
        }
        if (g->kind != GB_KNOBC) continue;
        int bip = g->p == P_CX + CX_PITCH || g->p == P_CX + CX_FINE || g->p == P_CX + CX_TONE;
        gb_knob(g->x, g->y, g->r, gb_frac(g->p), bip, CL_INK, on);
        ctext(g->x, g->y + g->r + 14, 2, on ? C_ORANGE : CL_INK, par_name[g->p]);
    }
}
static void draw_cl_live(void) {          // the cloud now: where POSITION and SPRAY point, and every grain sounding
    const Track *t = &song.t[sel]; const Sample *smpl = engine_sample(sel);
    int sx = 16 + 8, sy = 344, sw = 976, sh = 150, w = sw - 8;
    if (!smpl) return;
    float pos[CL_GRAINS], env = 0, scan = 0; int ng = engine_cloud_grains(sel, pos, CL_GRAINS, &env, &scan);
    float c0 = t->cp[CP_POS] / 100.f + scan, sp = t->cp[CP_SPRAY] / 100.f;   // MOTION carries the band along
    c0 -= floorf(c0);
    int a = sx + 4 + (int)((c0 - sp / 2) * w), b = sx + 4 + (int)((c0 + sp / 2) * w);
    for (int x = a; x <= b; x += 3) { int xx = x; while (xx < sx + 4) xx += w; while (xx >= sx + 4 + w) xx -= w; rect(xx, sy + 4, 1, sh - 8, mix(GB_LCD, C_ORANGE, 70), 1); }
    rect(sx + 4 + (int)(c0 * w), sy + 3, 2, sh - 6, C_ORANGE, 1);
    // each grain inside the band, wherever MOTION has taken it
    for (int i = 0; i < ng; i++) { float p = c0 + pos[i]; p -= floorf(p); int x = sx + 4 + (int)(p * w), y = sy + 20 + (int)((i * 37 % 100) / 100.f * (sh - 40));
        disc(x, y, 5, mix(GB_LCD, GB_LEGEND, (int)(120 + env * 135))); }
}
// ---------- the sampler's panel: rack graphite (2026-11-09) ----------
static void sm_section(int x, int y, int w, int h, const char *name) {
    rect(x, y, w, h, SM_PANEL, 1); rect(x, y, w, 24, SM_HEAD, 1);
    text(x + 10, y + 4, 2, SM_INK, name);
    rect(x, y + h - 1, w, 1, SM_EDGE, 1);
    disc(x + w - 12, y + 12, 4, SM_EDGE); line(x + w - 15, y + 12, x + w - 9, y + 12, 1, SM_HEAD);   // a rack screw
}
static void sm_plate(const GbCtl *g, int w, int on, int idle, const char *label) {   // a value on a dark plate, its name small beside it
    char b[24]; par_str(sel, g->p, b, sizeof b);
    int x = g->x - w / 2;
    rect(x, g->y - 12, w, 26, on ? C_ORANGE : SM_LCD, 1); frame(x, g->y - 12, w, 26, 1, on ? C_ORANGE : SM_EDGE);
    const char *v = idle ? "--" : b; int vw = (int)strlen(v) * 16, lw = (int)strlen(label) * 8;
    if (idle && !on) {                    // hatched = not in use; the words on clear ground over it, so they stay readable
        hatch(x + 2, g->y - 10, w - 4, 22, 6, 1, SM_EDGE);
        rect(x + 3, g->y - 6, lw + 6, 12, SM_LCD, 1); rect(x + w - 9 - vw, g->y - 9, vw + 6, 20, SM_LCD, 1);
    }
    text(x + 6, g->y - 4, 1, on ? C_BG : idle ? C_GREY : SM_INK, label);
    text(x + w - 6 - vw, g->y - 7, 2, on ? C_BG : idle ? SM_DIM : SM_GLOW, v);
}
static void norm_plate(const GbCtl *g, int h, int on, Col bg, Col edge, Col ink, Col glow) {   // NORM and OFF / PEAK / LOUD, 120 wide
    char b[8]; par_str(sel, P_NORM, b, sizeof b); int w = 120, x = g->x - w / 2, y = g->y - h / 2;
    rect(x, y, w, h, on ? C_ORANGE : bg, 1); frame(x, y, w, h, 1, on ? C_ORANGE : edge);
    text(x + 6, g->y - 4, 1, on ? C_BG : ink, "NORM");
    text(x + w - 6 - (int)strlen(b) * 16, g->y - 8, 2, on ? C_BG : song.mix.pad[sel].spare[PM_NORM] ? glow : ink, b);
}
static void sm_lever(const GbCtl *g, int on, int up, const char *a, const char *b, const char *name) {
    int lx = g->x - 30, y0 = g->y - 30, mw = (int)(strlen(a) > strlen(b) ? strlen(a) : strlen(b)) * 16;
    if (on) frame(lx - 20, y0 - 10, 20 + 22 + mw + 8, 90, 3, C_ORANGE);   // as wide as its words
    text(lx + 22, y0 - 4, 2, up ? SM_INK : C_GREY, a); text(lx + 22, y0 + 44, 2, !up ? SM_INK : C_GREY, b);
    rect(lx - 7, y0, 14, 60, SM_HEAD, 1); frame(lx - 7, y0, 14, 60, 1, SM_EDGE);
    line(lx, y0 + 30, lx, up ? y0 + 4 : y0 + 56, 6, SM_INK); disc(lx, up ? y0 + 4 : y0 + 56, 7, up ? SM_INK : SM_GLOW);
    ctext(g->x, g->y + 56, 2, on ? C_ORANGE : SM_INK, name);
}
static float sm_peaks[968]; static const Sample *sm_peak_s;
static void draw_sm(int focus) {
    const Track *t = &song.t[sel]; const Sample *s = engine_sample(sel);
    int n; const GbCtl *c = pctl(&n); int fp = focus >= 0 && focus < n ? c[focus].p : -1;
    { int on = fp == P_SAMPLE;            // top strip: the instrument switch and a readout, as on the other panels
      rect(16, 256, 384, 34, on ? C_ORANGE : SM_HEAD, 1);
      text(26, 265, 2, on ? C_BG : C_DIM, "INSTRUMENT"); text(196, 265, 2, on ? C_BG : SM_INK, "SAMPLER");
      if (!on) text(366, 265, 2, C_DIM, "<>"); }
    rect(410, 256, 598, 34, SM_LCD, 1); frame(410, 256, 598, 34, 2, SM_EDGE);
    if (fp >= 0 && fp != P_SAMPLE) { char b[64]; par_str(sel, fp, b, sizeof b); textf(424, 265, 2, SM_GLOW, "%-8s %s", par_name[fp], b); }
    else textf(424, 265, 2, s ? SM_GLOW : SM_DIM, "%.36s", s ? s->name : "NO SAMPLE: EDIT TO BROWSE");
    // SAMPLE: the whole sample, START..END bright, the slices, where it plays now (live, on top)
    sm_section(16, 296, 992, 190, "SAMPLE");
    { int sx = 24, sy = 324, sw = 976, sh = 154, w = sw - 8; rect(sx, sy, sw, sh, SM_LCD, 1); frame(sx, sy, sw, sh, 2, SM_EDGE);
      for (int i = 1; i < 4; i++) rect(sx + 2, sy + sh * i / 4, sw - 4, 1, mix(SM_LCD, SM_GLOW, 18), 1);
      if (s && s->n > 1) {
          if (sm_peak_s != s) { sm_peak_s = s;
              for (int i = 0; i < w; i++) { long a = (long)s->n * i / w, b = (long)s->n * (i + 1) / w; float m = 0;
                  for (long k = a; k < b; k += 16) { float v = fabsf(s->d[2 * k]); if (v > m) m = v; } sm_peaks[i] = m; } }
          int a = sx + 4 + (int)((long)ed_start(s) * w / s->n), b = sx + 4 + (int)((long)ed_end(s) * w / s->n);
          float ng = norm_gain(sel);        // the picture shows the level it plays at
          for (int i = 0; i < w; i++) { int x = sx + 4 + i, h = (int)(sm_peaks[i] * ng * (sh / 2 - 22)); if (h > sh / 2 - 22) h = sh / 2 - 22;
              rect(x, sy + sh / 2 + 6 - h, 1, 2 * h + 1, x >= a && x <= b ? SM_GLOW : SM_DIM, 1); }
          int ns = track_nslices(sel);
          for (int m = 1; m < ns; m++) { int x = sx + 4 + (int)((long)ed_pos(s, m) * w / s->n); for (int y = sy + 22; y < sy + sh - 4; y += 4) rect(x, y, 1, 2, SM_INK, 1); }
          if (ns > 1) for (int m = 0; m < ns && ns <= 32; m++) {   // numbers over the slices, when there is room
              int x0 = sx + 4 + (int)((long)ed_pos(s, m) * w / s->n), x1 = sx + 4 + (int)((long)ed_pos(s, m + 1) * w / s->n); char b[4]; snprintf(b, 4, "%d", m + 1);
              if (x1 - x0 > 20) text(x0 + 3, sy + sh - 14, 1, SM_GLOW, b); }
          rect(a, sy + 4, 2, sh - 8, SM_INK, 1); rect(b - 1, sy + 4, 2, sh - 8, SM_INK, 1);
          char inf[64]; float sec = s->n / 48000.f, bpm = track_bpm(sel); int r = track_root(sel);
          int k = snprintf(inf, sizeof inf, "%.1fS", sec);
          if (r >= 0) k += snprintf(inf + k, sizeof inf - k, "  %s", note_name[r]);
          if (bpm > 0) snprintf(inf + k, sizeof inf - k, "  %d BPM", (int)(bpm + 0.5f));
          text(sx + 10, sy + 8, 2, SM_GLOW, inf);
          if (ns) { char b[24]; par_str(sel, P_SLICES, b, sizeof b); textf(sx + sw - 10 - (8 + (int)strlen(b)) * 8, sy + 12, 1, SM_GLOW, "SLICES %s", b); }
      } else ctext(sx + sw / 2, sy + sh / 2 - 8, 2, SM_DIM, "NO SAMPLE  -  EDIT ON INSTRUMENT TO BROWSE");
    }
    // the four sections below
    sm_section(16, 490, SM_S1 - 16 - 4, 200, "TUNE");
    sm_section(SM_S1, 490, SM_S2 - SM_S1 - 4, 200, "PLAY");
    sm_section(SM_S2, 490, SM_S3 - SM_S2 - 4, 200, "WARP");
    sm_section(SM_S3, 490, 1008 - SM_S3, 200, "OUTPUT");
    int warp = track_bpm(sel) > 0 ? t->warp : 0;
    for (int i = 0; i < n; i++) {
        const GbCtl *g = &c[i]; int on = i == focus, idle = !par_visible(g->p);
        switch (g->p) {
        case P_SAMPLE: break;
        case P_NORM: norm_plate(g, 24, on, SM_LCD, SM_EDGE, SM_INK, SM_GLOW); break;
        case P_ROOT: sm_plate(g, 256, on, 0, "ROOT"); break;
        case P_SLICES: sm_plate(g, 224, on, 0, "SLICES"); break;
        case P_BARS: sm_plate(g, 172, on, idle, "BARS"); break;
        case P_BPM: sm_plate(g, 172, on, idle, "BPM"); break;
        case P_VOICE: sm_lever(g, on, !t->voice, "GATE", "POLY", "VOICE"); break;
        case P_DIR: sm_lever(g, on, !t->rev, "FWD", "REV", idle ? "FWD ONLY" : "DIR"); break;
        case P_WARP: {                       // six LEDs, the mode in use lit; with no tempo found it plays plainly
            int x = g->x - 58, y0 = 522;
            if (on) frame(x - 8, y0 - 6, 148, 6 * 26 + 8, 3, C_ORANGE);   // clear of the longest name (REPITCH)
            for (int m = 0; m < 6; m++) { int y = y0 + m * 26, lit = m == t->warp % 6;
                disc(x + 6, y + 8, 6, lit ? (m == warp ? SM_GLOW : SM_DIM) : SM_HEAD);
                text(x + 20, y + 1, 2, lit ? SM_INK : C_GREY, warp_name[m]); }
            break; }
        default: {                          // the knobs: PITCH FINE GRAIN FLUX VOLUME PAN
            float f = g->p == P_FINE ? (t->fine + 50) / 100.f : g->p == P_PAN ? (t->pan + 50) / 100.f
                    : g->p == P_GRAIN ? ((t->grain ? t->grain : t->warp == 4 ? 90 : 30) - 5) / 115.f
                    : g->p == P_FLUX ? (t->flux ? t->flux : 40) / 100.f : gb_frac(g->p);
            int bip = g->p == P_PITCH || g->p == P_FINE || g->p == P_PAN;
            gb_knob(g->x, g->y, g->r, f, bip, idle ? SM_EDGE : SM_INK, on);
            if (idle) { disc(g->x, g->y, g->r - 3, SM_HEAD); hatch(g->x - g->r + 4, g->y - g->r + 4, 2 * g->r - 8, 2 * g->r - 8, 5, 1, SM_EDGE); }
            ctext(g->x, g->y + g->r + (g->p == P_PAN ? 10 : 14), 2, on ? C_ORANGE : idle ? C_GREY : SM_INK, par_name[g->p]);
        } }
    }
}
// SAMPLER DEEP: the sample with its fades drawn on it, and the three knobs
static void draw_smdeep(int focus) {
    const Sample *s = engine_sample(sel); const int8_t *sx = song.smx[sel];
    int n; const GbCtl *c = pctl(&n); int fp = focus >= 0 && focus < n ? c[focus].p : -1;
    { int on = fp == P_SAMPLE;
      rect(16, 256, 384, 34, on ? C_ORANGE : SM_HEAD, 1);
      text(26, 265, 2, on ? C_BG : C_DIM, "INSTRUMENT"); text(196, 265, 2, on ? C_BG : SM_INK, "SAMPLER");
      if (!on) text(366, 265, 2, C_DIM, "<>"); }
    rect(410, 256, 598, 34, SM_LCD, 1); frame(410, 256, 598, 34, 2, SM_EDGE);
    if (fp >= 0 && fp != P_SAMPLE) { char b[64]; par_str(sel, fp, b, sizeof b); textf(424, 265, 2, SM_GLOW, "%-8s %s", par_name[fp], b); }
    else text(424, 265, 2, SM_GLOW, "SAMPLER DEEP: FADES");
    sm_section(16, 296, 992, 190, "FADES");
    { int sx0 = 24, sy = 324, sw = 976, sh = 154, w = sw - 8; rect(sx0, sy, sw, sh, SM_LCD, 1); frame(sx0, sy, sw, sh, 2, SM_EDGE);
      if (s && s->n > 1) {
          if (sm_peak_s != s) { sm_peak_s = s;
              for (int i = 0; i < w; i++) { long a = (long)s->n * i / w, b = (long)s->n * (i + 1) / w; float m = 0;
                  for (long k = a; k < b; k += 16) { float v = fabsf(s->d[2 * k]); if (v > m) m = v; } sm_peaks[i] = m; } }
          float ng = norm_gain(sel);
          for (int i = 0; i < w; i++) { int x = sx0 + 4 + i, h = (int)(sm_peaks[i] * ng * (sh / 2 - 22)); if (h > sh / 2 - 22) h = sh / 2 - 22;
              rect(x, sy + sh / 2 + 6 - h, 1, 2 * h + 1, SM_DIM, 1); }
          // the level each part plays at: up over FADE IN, down over FADE OUT before its end, or past its end over XFADE
          int ns = track_nslices(sel); if (ns < 1) ns = 1;
          int fi = sx_frames(sx[SX_FIN]), fo = sx_frames(sx[SX_FOUT]), xf = sx_frames(sx[SX_XFADE]), top = sy + 24, bot = sy + sh - 6;
          for (int m = 0; m < ns && ns <= 32; m++) {
              long a = ns > 1 ? ed_pos(s, m) : ed_start(s), b = ns > 1 ? ed_pos(s, m + 1) : ed_end(s);
              int xa = sx0 + 4 + (int)(a * w / s->n), xb = sx0 + 4 + (int)(b * w / s->n);
              int in = (int)((long)(m && xf > fi ? xf : fi) * w / s->n), out = (int)((long)(xf > fo ? xf : fo) * w / s->n), ext = (int)((long)xf * w / s->n);
              int xe = xb + ext; if (xe > sx0 + sw - 4) xe = sx0 + sw - 4;                 // XFADE: it plays on past its end
              int x1 = xa + in > xe ? xe : xa + in, x2 = xe - out < x1 ? x1 : xe - out;
              line(xa, in ? bot : top, x1, top, 2, C_ORANGE); line(x1, top, x2, top, 2, C_ORANGE); line(x2, top, xe, out ? bot : top, 2, C_ORANGE);
              if (ns > 1 && m) for (int y = top; y < bot; y += 6) rect(xa, y, 1, 3, SM_INK, 1);
          }
          char inf[48]; snprintf(inf, sizeof inf, "%.1fS%s", s->n / 48000.f, ns > 1 ? "  SLICES" : ""); text(sx0 + 10, sy + 8, 1, SM_GLOW, inf);
      } else ctext(sx0 + sw / 2, sy + sh / 2 - 8, 2, SM_DIM, "NO SAMPLE");
    }
    static const char *const what[SX_N][2] = {{"THE NOTE COMES IN SOFTLY", "0 = AT ONCE"}, {"IT FADES BEFORE ITS END", "AND WHEN IT IS CUT"}, {"SLICES AND NOTES OVERLAP", "EACH FADES INTO THE NEXT"}};
    for (int k = 0; k < SX_N; k++) sm_section(16 + 332 * k, 490, k == 2 ? 328 : 328, 200, par_name[P_SX + k]);
    for (int i = 0; i < n; i++) {
        const GbCtl *g = &c[i]; if (g->kind != GB_KNOBC) continue;
        int on = i == focus, k = g->p - P_SX; char b[24]; par_str(sel, g->p, b, sizeof b);
        gb_knob(g->x - 104, g->y, g->r, gb_frac(g->p), 0, SM_INK, on);
        text(g->x - 50, g->y - 18, 3, on ? C_ORANGE : SM_GLOW, b);
        text(g->x - 50, g->y + 16, 1, SM_INK, what[k][0]); text(g->x - 50, g->y + 30, 1, C_GREY, what[k][1]);
    }
}
static void draw_sm_live(void) {          // where the sounding voices read the sample now
    const Sample *s = engine_sample(sel); if (!s) return;
    float pos[8]; int k = engine_sample_live(sel, pos, 8);
    for (int i = 0; i < k; i++) { int x = 24 + 4 + (int)(pos[i] * 968); rect(x, 328, 2, 146, C_ORANGE, 1); }
}
// What the drawn panel depends on: when none of it changed, the last picture is copied (it cost ~60 % of a core
// to draw every frame). The live marks (cutoff, envelope level) go on top each frame.
typedef struct { int sel, focus; int8_t sp[16], ap[12], cp[7], cx[16], ax[16], gx[32], gp[16], pick, type, voice, pitch, fine, vol, deep, norm, sx[8], sm[24]; int cuts; Col th[TC_N]; const void *smp; } GbKey;
static SDL_Texture *gb_tex; static GbKey gb_key; static int gb_valid, gb_draws, gb_direct;   // gb_direct: draw now, into whatever target is set
static void draw_gb_live(void) {
    if (song.t[sel].type == 0) { draw_sm_live(); return; }
    if (song.t[sel].type == 3) { if (!cl_deep) draw_cl_live(); return; }
    float env, fc;
    if (!engine_synth_live(sel, &env, &fc)) return;
    if (song.t[sel].type == 2 && cl_deep) return;
    if (song.t[sel].type == 1 && cl_deep == 2) return;
    if (song.t[sel].type == 1 && cl_deep) { draw_gbdeep_live(); return; }
    if (song.t[sel].type == 2) {          // ACID: where the filter is now, on its response
        int sx = AB_SX1 + 12, sy = 332, sw = 396, sh = 60, lx = sx + 4 + (int)(log10f(fc / 20.f) / 3 * (sw - 8));
        if (lx > sx + 2 && lx < sx + sw - 2) rect(lx, sy + 3, 2, sh - 6, GB_LEGEND, 1);
        return;
    }
    { int sx = GB_SX2 + 10, sy = 332, sw = 200, sh = 64, lx = sx + 4 + (int)(log10f(fc / 20.f) / 3 * (sw - 8));
      if (lx > sx + 2 && lx < sx + sw - 2) rect(lx, sy + 3, 2, sh - 6, GB_LEGEND, 1); }
    { int sx = GB_SX3 + 10, sy = 332, sh = 64, b = sy + sh - 6, hgt = b - (sy + 6);
      disc(sx + 12, b - (int)(env * hgt), 5, GB_LEGEND); }
}
static void draw_auto_tags(void) {        // A1..A4 by the knobs this pattern's automation moves
    int n; const GbCtl *c = pctl(&n); const AutoLane *al = song.aut[sel][song.t[sel].cur];
    for (int i = 0; i < n; i++) {
        int tg = auto_bind_target(c[i].p), k = 0; if (tg < 0) continue;
        for (int a = 0; a < NAUTO; a++) if (al[a].target == tg) {
            int r = c[i].r ? c[i].r : 14, x = c[i].x + r + 2 + k * 30, y = c[i].y - r - 16; char b[4]; snprintf(b, 4, "A%d", a + 1);
            plate(x, y, 28, 16, 3, learning && a == learn_lane - L_A1 ? C_WHITE : C_ORANGE); text(x + 6, y + 4, 1, C_BG, b); k++; }
        for (int j = 0; j < NLFO; j++) if (song.lfo[sel][j].target == tg && song.lfo[sel][j].depth) {
            int r = c[i].r ? c[i].r : 14, x = c[i].x + r + 2 + k * 30, y = c[i].y - r - 16; char b[4]; snprintf(b, 4, "L%d", j + 1);
            plate(x, y, 28, 16, 3, C_WHITE); text(x + 6, y + 4, 1, C_BG, b); k++; }
    }
}
#include "hints.h"
static int marq_live;                     // a hint is scrolling: keep the frame rate up
static void draw_marquee(void) {          // after 10 s on one control, its line scrolls in the readout, right of the name
    static int hp = -2, hdeep = -1, hsel = -1; static Uint32 ht;
    int p = cur_par(); Uint32 now = SDL_GetTicks(); marq_live = 0;
    if (p != hp || cl_deep != hdeep || sel != hsel) { hp = p; hdeep = cl_deep; hsel = sel; ht = now | 1; }
    if (!hint_on || srow < 3 || p <= 0 || ms_since(now, ht) < 10000) return;   // only on the panel, not the steps above it
    const char *h = par_hint(p); if (!h || !h[0]) return;
    marq_live = 1;
    int x0 = 412, x1 = 1006, nx = 424 + ((int)strlen(par_name[p]) + 1) * 16; if (nx > 700) nx = 700;
    rect(x0, 258, x1 - x0, 30, GB_LCD, 1); text(424, 265, 2, C_ORANGE, par_name[p]);
    SDL_Rect clip = {nx, 258, x1 - 8 - nx, 30}; SDL_RenderSetClipRect(R, &clip);
    int len = (int)strlen(h) * 16 + 160, off = (int)((ms_since(now, ht) - 10000) / 20) % len;   // 50 px a second, then again
    text(nx + clip.w - off, 265, 2, mix(GB_LCD, C_WHITE, 200), h);
    SDL_RenderSetClipRect(R, NULL);
}
static void draw_gb_cached(int focus) {
    enum { Y0 = 252, HH = 694 - 252 };
    GbKey k; memset(&k, 0, sizeof k);
    const Track *t = &song.t[sel];
    k.sel = sel; k.focus = focus; k.norm = song.mix.pad[sel].spare[PM_NORM]; memcpy(k.sx, song.smx[sel], 8); memcpy(k.sp, t->sp, 16); memcpy(k.ap, t->ap, 12); memcpy(k.cp, t->cp, 7); memcpy(k.cx, song.cx[sel], 16); memcpy(k.ax, song.ax[sel], 16); memcpy(k.gx, song.gx[sel], 32); memcpy(k.gp, song.gp[sel], 16); k.pick = (int8_t)pt_pick; k.deep = (int8_t)cl_deep; k.type = t->type; k.smp = engine_sample(sel); k.voice = t->voice; k.pitch = t->pitch; k.fine = t->fine; k.vol = t->vol; memcpy(k.th, th_cur, sizeof k.th);
    memcpy(k.sm, &t->pitch, offsetof(Track, scale) - offsetof(Track, pitch)); k.cuts = song.cut[sel].n * 7919 + song.cut[sel].pos[1];   // the sampler's values
    if (!gb_tex && SDL_RenderTargetSupported(R)) {
        gb_tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
        gb_valid = 0;
    }
    if (!gb_tex || gb_direct) { if (t->type == 3 && cl_deep) draw_cldeep(focus); else if (t->type == 3) draw_cl(focus); else if (t->type == 2 && cl_deep) draw_abdeep(focus); else if (t->type == 2) draw_ab(focus); else if (t->type == 1 && cl_deep == 2) draw_gbpatch(focus); else if (t->type == 1 && cl_deep) draw_gbdeep(focus); else if (t->type == 1) draw_gb(focus); else if (cl_deep) draw_smdeep(focus); else draw_sm(focus); draw_gb_live(); draw_marquee(); return; }   // no render targets: every frame
    if (!gb_valid || memcmp(&k, &gb_key, sizeof k)) {
        SDL_SetRenderTarget(R, gb_tex);
        SDL_SetRenderDrawColor(R, C_BG.r, C_BG.g, C_BG.b, 255); SDL_RenderClear(R);
        if (t->type == 3 && cl_deep) draw_cldeep(focus); else if (t->type == 3) draw_cl(focus); else if (t->type == 2 && cl_deep) draw_abdeep(focus); else if (t->type == 2) draw_ab(focus); else if (t->type == 1 && cl_deep == 2) draw_gbpatch(focus); else if (t->type == 1 && cl_deep) draw_gbdeep(focus); else if (t->type == 1) draw_gb(focus); else if (cl_deep) draw_smdeep(focus); else draw_sm(focus);
        SDL_SetRenderTarget(R, NULL);
        gb_key = k; gb_valid = 1; gb_draws++;
    }
    SDL_Rect a = {0, Y0, W, HH};
    SDL_RenderCopy(R, gb_tex, &a, &a);
    draw_gb_live(); draw_auto_tags(); draw_marquee();
}
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
static void draw_presets(void) {
    int idx[NPRESET + NUSERP + 1], n = pr_list(idx, NPRESET + NUSERP + 1); char b[64];
    pr_cur = clampi(pr_cur, 0, n - 1);
    static const char *const en[4] = {"", "GRAND", "ACID", "CLOUD"};
    snprintf(b, sizeof b, "PRESETS  %s  %d", en[song.t[sel].type % 4], n - 1); text(16, 92, 2, C_WHITE, b);
    rect(16, 120, 992, 1, C_LINE, 1);
    enum { ROWS = 13, RH = 42 };
    if (pr_cur < pr_top) pr_top = pr_cur; if (pr_cur >= pr_top + ROWS) pr_top = pr_cur - ROWS + 1;
    for (int r = 0; r < ROWS && pr_top + r < n; r++) {
        int j = pr_top + r, i = idx[j], y = 126 + r * RH, on = j == pr_cur;
        const char *nm = i < 0 ? "+ SAVE CURRENT" : i >= 1000 ? upre[i - 1000].name : presets[i].name;
        const char *cat = i < 0 ? "" : i >= 1000 ? "USER" : preset_cat[presets[i].cat % NPRESET_CAT];
        int newcat = i >= 0 && i < 1000 && (j == 0 || idx[j - 1] < 0 || idx[j - 1] >= 1000 || presets[idx[j - 1]].cat != presets[i].cat);
        if (newcat && !on) rect(16, y - 2, 600, 1, C_LINE, 1);
        if (on) rect(16, y, 600, RH - 4, C_ORANGE, 1);
        text(28, y + 9, 2, on ? C_BG : i < 0 ? C_DIM : C_WHITE, nm);
        text(604 - (int)strlen(cat) * 16, y + 9, 2, on ? C_BG : i >= 1000 ? C_ORANGE : C_GREY, cat);
    }
    if (n > ROWS) { int h = 13 * RH * ROWS / n, y = 126 + 13 * RH * pr_top / n; rect(622, 126, 3, 13 * RH, C_CELL, 1); rect(622, y, 3, h, C_ORANGE, 1); }
    // the sound as it is now, short
    int i = idx[pr_cur]; const char *nm = i < 0 ? "SAVE CURRENT" : i >= 1000 ? upre[i - 1000].name : presets[i].name;
    text(648, 132, 3, C_WHITE, nm);
    text(648, 172, 2, C_DIM, i < 0 ? "THE SOUND YOU HAD" : i >= 1000 ? "USER" : preset_cat[presets[i].cat % NPRESET_CAT]);
    text(648, 220, 1, C_GREY, "UP/DOWN: HEAR IT ON THE PAD");
    text(648, 236, 1, C_GREY, "HOLD OPTION: PLAY A NOTE");
    text(648, 252, 1, C_GREY, "EDIT: KEEP IT   Y: AS BEFORE");
    text(648, 268, 1, C_GREY, "L1/R1: CATEGORY");
    draw_footer(i < 0 ? "EDIT SAVE THE SOUND YOU HAD  Y BACK" : i >= 1000 ? "EDIT KEEP  OPT LISTEN  OPT+EDIT DELETE  Y BACK" : "EDIT KEEP  OPT LISTEN  L1/R1 CATEGORY  Y BACK");
}
static void draw_rnd(void) {
    Pattern *p = cur_pat(sel); char b[48];
    snprintf(b, sizeof b, "RANDOM  P%d  %s %s", song.t[sel].cur + 1, note_name[track_key(sel)], scale_name[track_scale(sel)]); text(16, 92, 2, C_WHITE, b);
    rect(16, 120, 992, 1, C_LINE, 1);
    int acid = song.t[sel].type == 2;
    for (int r = 0; r < RN_GO; r++) {
        int y = 128 + r * 46, on = rn_row == r, lo, hi; rn_range(r, &lo, &hi); rn_str(r, b);
        int dim = (r == RN_SLIDE && !acid && !p->mode) || (r == RN_HOLD && !p->mode) || ((r == RN_RANGE || r == RN_MOVE) && rn_v[RN_WHAT] == 0) || ((r == RN_DENS || r == RN_FEEL) && rn_v[RN_WHAT] == 1);
        if (on) rect(16, y, 560, 42, C_ORANGE, 1);
        text(26, y + 6, 2, on ? C_BG : dim ? C_GREY : C_DIM, rn_name[r]);
        text(566 - (int)strlen(b) * 16, y + 6, 2, on ? C_BG : dim ? C_GREY : C_WHITE, b);
        if (hi > 3) { rect(26, y + 32, 530, 4, on ? mix(C_BG, C_ORANGE, 120) : C_CELL, 1); rect(26, y + 32, 530 * rn_v[r] / 100, 4, on ? C_BG : dim ? C_GREY : C_ORANGE, 1); }
    }
    { int y = 128 + RN_GO * 46 + 10, on = rn_row == RN_GO; rect(16, y, 560, 690 - y - 8, on ? C_ORANGE : C_CELL, 1); ctext(296, y + (690 - y) / 2 - 22, 4, on ? C_BG : C_WHITE, "GENERATE"); }
    // the pattern as it is now
    int st = song.t[sel].on ? engine_step(sel) : -1;
    text(600, 128, 1, C_GREY, p->mode ? "PIANO ROLL" : "STEPS");
    if (p->mode) draw_roll_mini(p, 600, 144, 408, 200, st, 0);
    else { rect(600, 144, 408, 200, C_CELL, 0); step_matrix(p, 640, 150, 180, st, 0); }
    text(600, 360, 1, C_GREY, "NOTES FROM THE PAD'S KEY AND SCALE");
    text(600, 376, 1, C_GREY, "EVERY GENERATE: A NEW TAKE, SHIFT+L2 UNDOES");
    draw_footer(rn_row == RN_GO ? "EDIT GENERATE  SHIFT+L2 UNDO  PLAY  Y BACK" : "EDIT+<> CHANGE  EDIT NEXT  UP/DOWN ROW  Y BACK");
}
// PATTERN > ROLL (UI v2, mockup A06): piano keys on the left, fourteen rows, the notes in the engine's colour.
static void draw_roll(void) {
    Pattern *p = cur_pat(sel); int T = roll_ticks(p), st = song.t[sel].on ? engine_step(sel) : -1;
    enum { KX = 26, KW = 64, GX = 96, GY = 76, CW = 28, RH = 44 };
    int beat = 8 * RT, bar = beat * (song.beats > 0 ? song.beats : 4), i = roll_find(p, rl_t, rl_n);
    Col ec = C_ENG(song.t[sel].type), key_w = mix(C_WHITE, C_BG, 30), key_b = C_LINE;
    roll_view();
    char b[80], nm[8];
    for (int r = 0; r < RL_ROWS; r++) {  // rows, high pitches on top: white and black keys; the pad's scale has a dot, its tonic the accent
        int n = rl_n0 + RL_ROWS - 1 - r, y = GY + r * RH, pc = n % 12, black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        int ton = ((n - track_key(sel)) % 12 + 12) % 12 == 0;
        rect(GX, y, RL_COLS * CW, RH - 1, black ? mix(C_BG, C_CELL, 110) : C_CELL, 1);
        rect(KX, y, KW, RH - 1, black ? key_b : key_w, 1);
        if (pc == 0 || n == rl_n) { roll_name(nm, n); ftext(KX + 6, y + 2, FT_S, n == rl_n && pc ? ec : black ? C_WHITE : C_BG, nm); }
        if (roll_in_scale(n)) rect(KX + KW - 10, y + RH / 2 - 3, 6, 6, ton ? C_ORANGE : black ? C_DIM : C_GREY, 1);
    }
    for (int c = 0; c <= RL_COLS; c++) { int t = rl_v0 + c * rl_snap;   // lines: beats faint, bars clear
        if (t % bar == 0) rect(GX + c * CW, GY, 2, RL_ROWS * RH, C_GREY, 1);
        else if (t % beat == 0) rect(GX + c * CW, GY, 1, RL_ROWS * RH, C_LINE, 1); }
    int end = T - rl_v0; if (end < RL_COLS * rl_snap) { int ex = GX + (end > 0 ? end * CW / rl_snap : 0); rect(ex, GY, GX + RL_COLS * CW - ex, RL_ROWS * RH, C_BG, 1); }   // past the end of the pattern
    for (int j = 0; j < NNOTE; j++) { const Note *n = &p->roll[j];
        if (!n->len || n->note < rl_n0 || n->note >= rl_n0 + RL_ROWS) continue;
        int a = n->start - rl_v0, e = a + n->len; if (e <= 0 || a >= RL_COLS * rl_snap) continue;
        int x0 = GX + (a < 0 ? 0 : a * CW / rl_snap), x1 = GX + (e > RL_COLS * rl_snap ? RL_COLS * rl_snap : e) * CW / rl_snap;
        int y = GY + (rl_n0 + RL_ROWS - 1 - n->note) * RH;
        if (x1 - x0 < 8) x1 = x0 + 8;
        Col c = n->start >= T ? C_GREY : mix(mix(ec, C_BG, 110), ec, n->vel * 2);
        if (j == i) rframe(x0, y + 2, x1 - x0, RH - 5, 7, 3, C_WHITE, c); else rrect(x0 + 2, y + 4, x1 - x0 - 3, RH - 9, 5, c);
        if (n->flags & NF_ACC) ftext(x0 + 7, y + 10, FT_S, C_BG, "A");
    }
    if (st >= 0) { int t = st * rl_d32[p->div % 6] * RT - rl_v0; if (t >= 0 && t < RL_COLS * rl_snap) rect(GX + t * CW / rl_snap, GY, 2, RL_ROWS * RH, C_WHITE, 1); }
    if (i < 0) { int cx = GX + (rl_t - rl_v0) * CW / rl_snap, cy = GY + (rl_n0 + RL_ROWS - 1 - rl_n) * RH;   // the cursor on an empty place
        rect(cx, cy + 2, CW, 2, C_WHITE, 1); rect(cx, cy + RH - 5, CW, 2, C_WHITE, 1); rect(cx, cy + 2, 2, RH - 5, C_WHITE, 1); rect(cx + CW - 2, cy + 2, 2, RH - 5, C_WHITE, 1); }
    snprintf(b, sizeof b, "grid 1/%d \3 bar %d.%d", 128 / rl_snap, rl_t / bar + 1, rl_t % bar / beat + 1); ftext(26, 694, FT_S, C_DIM, b);
    roll_name(nm, rl_n);
    if (i >= 0) { const Note *n = &p->roll[i]; snprintf(b, sizeof b, "%s \3 len %d/128 \3 vel %d \3 chance %d%s", nm, n->len, n->vel, n->prob, n->flags & NF_ACC ? " \3 acc" : ""); }
    else snprintf(b, sizeof b, "%s", nm);
    ftext(W - 28 - ftext_w(FT_S, b), 694, FT_S, C_WHITE, b);
    draw_footer(i >= 0 ? "EDIT DELETE  EDIT+ARROWS LENGTH/PITCH  Y BACK  X+ARROWS VEL/CHANCE  L1/R1 GRID  OPT+<> NEXT"
                       : "EDIT NOTE  L1/R1 GRID  Y BACK  OPT+<> NEXT  X+EDIT CHORD  SHIFT+ARROWS PAGE");
}
static void draw_seq(void) {
    Pattern *p = cur_pat(sel); int st = song.t[sel].on ? engine_step(sel) : -1;
    const char *hdr[2] = {"LEN", "STEP"}; int x = 16;   // LEN, STEP, PAGE, MODE from the left; the lanes from the right edge
    for (int c = 0; c < 2; c++) {
        int on = srow == 0 && scol == c; char v[16], b[32];
        if (c == 0) snprintf(v, sizeof v, "%d", p->len); else snprintf(v, sizeof v, "%s", div_name[p->div % 6]);
        snprintf(b, sizeof b, "%s %s", hdr[c], v);
        int w = (int)strlen(b) * 16 + 12;
        if (on) plate(x, 89, w, 24, 5, C_ORANGE);
        else { text(x + 6, 93, 2, C_DIM, hdr[c]); text(x + 6 + ((int)strlen(hdr[c]) + 1) * 16, 93, 2, C_WHITE, v); }
        if (on) text(x + 6, 93, 2, C_BG, b);
        x += w + 12;
    }
    {                                    // PAGE: a place of its own in the row, EDIT+<> turns the pages (grey: one page only)
        char b[16]; snprintf(b, sizeof b, "PAGE %d/%d", spage + 1, last_page() + 1); int w = (int)strlen(b) * 16 + 12;
        if (srow == 0 && scol == 2) { plate(x, 89, w, 24, 5, C_ORANGE); text(x + 6, 93, 2, C_BG, b); }
        else text(x + 6, 93, 2, last_page() ? C_ORANGE : C_GREY, b);
        x += w + 12;
    }
    {                                    // MODE: STEP or ROLL
        const char *m = p->mode ? "ROLL" : "STEP"; int w = (int)strlen(m) * 16 + 12;
        if (srow == 0 && scol == 3) { plate(x, 89, w, 24, 5, C_ORANGE); text(x + 6, 93, 2, C_BG, m); }
        else text(x + 6, 93, 2, p->mode ? C_ORANGE : C_WHITE, m);
        x += w + 12;
    }
    const int hx = x;                    // where the left half of the row ends: nothing on the right may start before it
    if (!lane_ok(lane)) lane = lane_first();
    if (learning) lane = learn_lane;
    if (learning && learn_lfo >= 0) {   // an LFO learns: no lanes, the question
        char b2[40]; snprintf(b2, sizeof b2, "LFO %d < PICK A KNOB", learn_lfo + 1); int w = (int)strlen(b2) * 16 + 12, x = 1008 - w, on = SDL_GetTicks() / 300 % 2;
        if (on) plate(x, 89, w, 24, 5, C_ORANGE); text(x + 6, 93, 2, on ? C_BG : C_ORANGE, b2);
    } else if (is_auto(lane)) {      // an automation lane: its target and JUMP/GLIDE instead of the lane list
        AutoLane *al = cur_aut(); char tn[24], b2[40]; lfo_tname(sel, al->target, tn);
        { int back = aut_point_back(); char bb[16]; if (back >= 0) snprintf(bb, sizeof bb, "BEND %+d", aut_bend()[back]); else snprintf(bb, sizeof bb, "NO POINTS");
          snprintf(b2, sizeof b2, "%s %s %s", lane_name[lane], al->target ? tn : "TARGET?", srow >= 1 && srow <= 2 ? bb : ""); }
        int fit = (1008 - hx - 12) / 16; if (fit < 4) fit = 4; if ((int)strlen(b2) > fit) b2[fit] = 0;   // a long target name is cut, never drawn over MODE
        if (learning) snprintf(b2, sizeof b2, "%s < PICK A KNOB", lane_name[learn_lane]);
        int on = (srow == 0 && scol == 4) || (learning && SDL_GetTicks() / 300 % 2), w = (int)strlen(b2) * 16 + 12, x = 1008 - w;
        if (on) plate(x, 89, w, 24, 5, C_ORANGE); text(x + 6, 93, 2, on ? C_BG : al->target ? C_ORANGE : C_RED, b2);
    } else {                             // the pad's lanes against the right edge, the current one a plate; A1..A4 show as one AUT
      int l[L_N], n = lane_list(l), m = 0; for (int i = 0; i < n; i++) if (!is_auto(l[i]) || l[i] == L_A1) l[m++] = l[i];
      int w = 0; for (int i = 0; i < m; i++) w += (is_auto(l[i]) ? 3 : (int)strlen(lane_nm(l[i]))) * 16 + 10;
      int x = 1008 - w; if (x < hx) x = hx;
      if (!last_page() && x - 52 > hx) text(x - 48, 97, 1, C_GREY, "L1/R1");   // with pages the footer says it
      for (int i = 0; i < m; i++) { const char *nm = is_auto(l[i]) ? "AUT" : lane_nm(l[i]); int lw = (int)strlen(nm) * 16 + 10;
          if (l[i] == lane) plate(x, 89, lw, 24, 5, C_ORANGE);
          text(x + 5, 93, 2, l[i] == lane ? C_BG : C_DIM, nm); x += lw; } }
    enum { SX = 16, SY = 118, SC = 62 };
    if (p->mode && !is_auto(lane)) draw_roll_mini(p, SX, SY, 992, 124, st, srow >= 1 && srow <= 2);
    else cell_lines(SX, SY, SC, SC, 16, 2);
    for (int v = 0; v < 32 && (!p->mode || is_auto(lane)); v++) {
        int i = spage * 32 + v, x = SX + (v % 16) * SC, y = SY + (v / 16) * SC, in = i < p->len;
        if (p->trig[i] && in && !is_auto(lane)) rect(x + 1, y + 1, 61, 61, lane == L_TRIG ? C_ORANGE : mix(C_BG, C_ORANGE, 90), 1);
        if (is_auto(lane)) {              // automation: an envelope (drawn below over the row), the trig steps faint behind it
            const AutoLane *al = cur_aut(); int v = al->v[i];
            if (p->trig[i] && in) rect(x + 1, y + 1, 61, 61, mix(C_BG, C_ORANGE, 40), 1);
            if (v != AUTO_EMPTY) { char b[8]; snprintf(b, 8, "%d", v); text(x + 31 - (int)strlen(b) * 4, y + 4, 1, in ? C_WHITE : C_GREY, b); }
        } else if (lane != L_TRIG && p->trig[i]) {
            char b[8];
            switch (lane) {
            case L_NOTE: note_str((uint8_t)(p->note[i] == NOTE_NONE ? clampi(roll_base(sel), 0, 127) : p->note[i]), b); break;
            case L_SLICE: slice_label(b, p, i, track_nslices(sel)); break;
            case L_VEL: snprintf(b, 8, "%d", p->vel[i]); break;
            case L_PROB: snprintf(b, 8, "%d", p->prob[i]); break;
            case L_ACC: case L_SLD: { int bit = lane == L_ACC ? ACID_ACCENT : ACID_SLIDE, on = p->slice[i] >= 0 && (p->slice[i] & bit);
                if (on && in) rect(x + 1, y + 1, 61, 61, C_ORANGE, 1);   // a set flag lights the step
                snprintf(b, 8, "%s", on ? (lane == L_ACC ? "ACC" : "SLD") : "-"); break; }
            default: snprintf(b, 8, "x%d", p->rep[i]); break;
            }
            int lit = (lane == L_ACC || lane == L_SLD) && p->slice[i] >= 0 && (p->slice[i] & (lane == L_ACC ? ACID_ACCENT : ACID_SLIDE));
            text(x + 31 - (int)strlen(b) * 8, y + 23, 2, !in ? C_GREY : lit ? C_BG : lane == L_SLICE && p->slice[i] < 0 ? C_DIM : C_WHITE, b);
        }
        if (lane == L_TRIG && p->trig[i]) {   // the step's note by name; a step never changed plays the tonic: say so
            char nb[8]; note_str((uint8_t)(p->note[i] == NOTE_NONE ? clampi(roll_base(sel), 0, 127) : p->note[i]), nb); int sl2 = track_nslices(sel);
            if (sl2) text(x + 5, y + 50, 1, in ? C_BG : C_GREY, nb); else text(x + 31 - (int)strlen(nb) * 8, y + 23, 2, in ? C_BG : C_GREY, nb);
        }
        int nsl = track_nslices(sel);
        if (nsl && p->trig[i] && in && lane != L_SLICE) {   // slice number on every sliced step
            char b[8]; slice_label(b, p, i, nsl);
            if (lane == L_TRIG) text(x + 31 - (int)strlen(b) * 8, y + 23, 2, C_BG, b);
            else text(x + 5, y + 50, 1, C_DIM, b);
        }
        if (!in) hatch(x + 1, y + 1, 61, 61, 10, 1, C_LINE);     // past the end of the clip
        if (i % 4 == 0) rect(x + 1, y + 1, 12, 4, in && p->trig[i] && lane == L_TRIG ? C_BG : C_GREY, 1);       // a beat starts here
        if (i == st) { frame(x, y, 63, 63, 3, C_PLAY); frame(x + 3, y + 3, 57, 57, 2, C_BG); }   // the dark line: visible on a lit step in any theme
        if (srow >= 1 && srow <= 2 && i == cur_step()) {     // orange outside, a dark line inside: visible on a lit step too
            frame(x - 1, y - 1, 65, 65, 3, C_ORANGE);
            frame(x + 2, y + 2, 59, 59, 2, i == st ? C_PLAY : C_BG);
        }
    }
    if (is_auto(lane)) {   // the envelope: a line through the points, each segment with its BEND
        const AutoLane *al = cur_aut(); const int8_t *bc = aut_bend(); int len = p->len ? p->len : 16, any = 0;
        for (int i = 0; i < len; i++) if (al->v[i] != AUTO_EMPTY) any = 1;
        if (any) for (int r = 0; r < 2; r++) {
            int s0 = spage * 32 + r * 16, y0 = SY + r * SC, px = -1, py = 0;
            for (int xx = 0; xx <= 16 * SC; xx += 2) {
                double cell = s0 + xx / (double)SC, sf = cell - 0.5; if (cell >= len) { px = -1; continue; } if (sf < 0) sf += len;   // the loop: before step 1 comes its end
                int pt = (int)floor(sf), pxs = SX + (int)((pt + 0.5 - s0) * SC);   // a point between the last sample and this one: through its vertex, so jumps stand upright
                if (px >= 0 && pt >= 0 && pt < len && al->v[pt] != AUTO_EMPTY && pxs > px && pxs <= SX + xx) {
                    int vy = y0 + SC - 6 - al->v[pt] * (SC - 18) / 100; float vb = auto_value(al, bc, len, pt - 1e-4 < 0 ? pt - 1e-4 + len : pt - 1e-4);
                    int by = y0 + SC - 6 - (int)(vb * (SC - 18) / 100); line(px, py, pxs, by, 2, C_ORANGE); line(pxs, by, pxs, vy, 2, C_ORANGE); px = pxs; py = vy; }
                float v = auto_value(al, bc, len, sf); int qx = SX + xx, qy = y0 + SC - 6 - (int)(v * (SC - 18) / 100);
                if (px >= 0) line(px, py, qx, qy, 2, C_ORANGE); px = qx; py = qy;
            }
            for (int c = 0; c < 16; c++) { int i = s0 + c; if (i >= len || al->v[i] == AUTO_EMPTY) continue;
                int cx = SX + c * SC + SC / 2, cy = y0 + SC - 6 - al->v[i] * (SC - 18) / 100, on = srow >= 1 && srow <= 2 && i == cur_step();
                rect(cx - 5, cy - 5, 10, 10, on ? C_WHITE : C_ORANGE, 1); }
        }
    }
    seq_clamp();
    enum { PY = 252, PH = 49 };
    rect(16, PY - 1, 992, 1, C_LINE, 1);
    if (1) draw_gb_cached(srow >= 3 ? gb_cur : -1);   // every pad: a drawn panel (the sampler since 2026-11-09)
    else {
    rect(510, PY - 1, 1, H - 74 - PY + 1, C_LINE, 1);
    for (int r = 1; r <= PROWS - 1; r++) rect(16, PY + r * PH - 1, 992, 1, C_CELL, 1);
    rect(511, PY + WARP_ROWS * PH - 1, 497, 1, C_LINE, 1);   // the WARP block has its own edge
    for (int c = 0; c < 2; c++) for (int r = 0; r < PROWS; r++) {
        int i = par_at(c, r); if (i < 0) continue;
        int x = 28 + c * 496, y = PY + r * PH + 8, on = srow == 3 + r && scol == c;
        char b[64]; par_str(sel, i, b, sizeof b);
        Col sec = par_section(i); int has_sec = i >= P_SY && memcmp(&sec, &C_BG, sizeof sec);
        if (on) rect(c ? 511 : 16, y - 8, c ? 497 : 494, PH - 1, C_ORANGE, 1);
        else if (has_sec) rect(c ? 511 : 16, y - 8, 124, PH - 1, sec, 1);   // the section's colour, as on the panel
        text(x, y + 8, 2, on ? C_BG : has_sec ? GB_INK : C_DIM, par_name[i]);
        text(x + 136, i == P_SAMPLE ? y + 8 : y + 4, i == P_SAMPLE ? 2 : 3, on ? C_BG : C_WHITE, b);
    }
    }
    int fp = srow >= 3 ? cur_par() : -1;
    draw_footer(srow < 0 && scol == 1 ? "EDIT+<> PATTERN  EDIT ALL PATTERNS  OPT+<> SLOT"
                : srow < 0 ? (is_synth(sel) ? "OPT+<> SLOT  OPT LISTEN  RIGHT: PATTERN" : "EDIT BROWSE  EDIT+<> FILE  OPT+<> SLOT  RIGHT: PATTERN")
                : learning ? "ARROWS TO A KNOB  EDIT BINDS IT  Y DEEP PAGE  OPT CANCEL"
                : srow == 0 && scol == 4 ? "EDIT LEARN: POINT AT A KNOB  EDIT+<> TARGET  L1/R1 LANE"
                : srow >= 1 && srow <= 2 && is_auto(lane) ? "EDIT POINT  EDIT+ARROWS VALUE  Y+ARROWS BEND  OPT+EDIT CLEAR"
                : srow == 0 && scol == 3 ? "EDIT STEPS/ROLL  XX DOUBLE  X+<> PAD"
                : srow >= 1 && srow <= 2 && p->mode ? "EDIT OPEN ROLL  OPT+EDIT CLEAR  SHIFT+X RANDOM"
                : srow == 0 && scol == 2 ? (last_page() ? "EDIT+<> PAGE  EDIT NEXT PAGE  XX DOUBLE  X+<> PAD" : "ONE PAGE: A LONGER CLIP HAS MORE  XX DOUBLE")
                : srow == 0 ? "EDIT+ARROWS CHANGE  XX DOUBLE  SHIFT+X RANDOM"
                : srow <= 2 && lane == L_TRIG ? (song.snap ? "EDIT STEP  EDIT+<> NOTE IN SCALE  EDIT+^v OCTAVE  L1/R1 LANE" : "EDIT STEP  EDIT+<> NOTE  EDIT+^v OCTAVE  L1/R1 LANE  XX DOUBLE")
                : srow <= 2 ? (last_page() ? "EDIT STEP  SHIFT+<> PAGE  L1/R1 LANE  XX DOUBLE  X+<> PAD" : "EDIT STEP  EDIT+ARROWS VALUE  L1/R1 LANE  XX DOUBLE  X+<> PAD")
                : fp == P_SLICES ? "EDIT SLICE EDITOR  EDIT+ARROWS CHANGE  OPT+EDIT DEFAULT"
                : fp == P_SAMPLE && is_synth(sel) ? "EDIT+ARROWS INSTRUMENT  OPT+EDIT SAMPLER"
                : song.t[sel].type == 1 && fp >= P_PT ? (pt_pick ? "EDIT ON AN INPUT: PLUG  Y DROP THE CABLE" : fp >= P_PT + 16 ? "EDIT TAKE ITS CABLE  OPT+EDIT PULL IT  Y PANEL" : "EDIT TAKE A CABLE  OPT+EDIT PULL ITS CABLES  Y PANEL")
                : song.t[sel].type == 1 && fp >= P_GX ? "Y PATCH  EDIT+ARROWS CHANGE  OPT+EDIT DEFAULT  HOLD EDIT HEAR"
                : song.t[sel].type == 2 && fp >= P_AX ? "Y BACK  EDIT+ARROWS CHANGE  OPT+EDIT DEFAULT"
                : song.t[sel].type == 2 && fp != P_SAMPLE ? "Y DEEP  EDIT+ARROWS CHANGE  OPT+EDIT DEFAULT  HOLD EDIT HEAR"
                : song.t[sel].type == 3 && fp >= P_CX ? "Y BACK  EDIT+ARROWS CHANGE  OPT+EDIT DEFAULT"
                : song.t[sel].type == 3 && fp != P_SRC ? "Y DEEP  EDIT+ARROWS CHANGE  OPT+EDIT DEFAULT"
                : song.t[sel].type == 0 && fp >= P_SX && fp < P_SX + SX_N ? "Y BACK  EDIT+ARROWS CHANGE  OPT+EDIT OFF"
                : fp == P_SAMPLE ? "EDIT BROWSE  EDIT+ARROWS INSTRUMENT  OPT+EDIT EMPTY"
                : fp == P_BPM ? "EDIT TAP IN TIME  EDIT+ARROWS CHANGE  OPT+EDIT AUTO"
                : fp == P_ROOT ? "EDIT SET AS PROJECT KEY  EDIT+ARROWS CHANGE  OPT+EDIT AUTO"
                : song.t[sel].type == 0 ? "Y DEEP (FADES)  EDIT+ARROWS CHANGE  OPT+EDIT DEFAULT"
                               : "EDIT+ARROWS CHANGE  OPT+EDIT DEFAULT");
}
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
    if (ui_v2) {                         // UI v2 (after A09): the wave on a panel, the points as lines with numbers, cards below
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
    dotfield(16, WY, WAVE_W, WH, 31, C_GREY);
    rect(16, CY, WAVE_W, 1, C_LINE, 1);
    frame(16, WY, WAVE_W, WH, 1, C_LINE);
    ruler(16, WY + WH + 1, WAVE_W - 1, WAVE_W / 40, 10, C_GREY);
    for (int x = 0; x < WAVE_W; x++) {
        int f = ed_view + (int)((int64_t)x * span / WAVE_W);
        int y0 = CY - (int)(clampf1(pk[x][1]) * (WH / 2 - 6)), y1 = CY - (int)(clampf1(pk[x][0]) * (WH / 2 - 6));
        rect(16 + x, y0, 1, y1 - y0 + 1, f < a || f > b ? C_GREY : f >= ka && f < kb ? C_ORANGE : C_DIM, 1);
    }
    for (int m = 0; m <= cnt; m++) {             // markers
        int p = ed_pos(s, m); if (p < ed_view || p > ed_view + span) continue;
        int x = 16 + (int)((int64_t)(p - ed_view) * (WAVE_W - 1) / span), on = m == ed_m, edge = m == 0 || m == cnt;
        rect(x - (on ? 1 : 0), WY, on ? 3 : edge ? 2 : 1, WH, on ? C_ORANGE : edge ? C_WHITE : C_GREY, 1);
        if (m == 0) text(x + 4, WY + WH - 20, 2, on ? C_ORANGE : C_WHITE, "S");
        else if (m == cnt) text(x - 20, WY + WH - 20, 2, on ? C_ORANGE : C_WHITE, "E");
        if (m < cnt && track_nslices(sel) && x + 36 < 16 + WAVE_W) textf(x + 4, WY + 4, cnt > 16 ? 1 : 2, m == k ? C_ORANGE : track_follows(sel, m) ? C_DIM : C_WHITE, "%d", m + 1);
    }
    char b1[32]; par_str(sel, P_SLICES, b1, sizeof b1);
    tag(16, 90, C_DIM, C_BG, "SLICES"); text(134, 94, 2, C_WHITE, b1);
    if (ed_zoom && span < s->n) textf(320, 94, 2, C_DIM, "ZOOM X%d", s->n / span);
    if (ed_m == 0) textf(640, 94, 2, C_ORANGE, "START %d.%d%%", t->start / 10, t->start % 10);
    else if (ed_m == cnt) textf(640, 94, 2, C_ORANGE, "END %d.%d%%", t->end / 10, t->end % 10);
    else textf(640, 94, 2, C_ORANGE, "POINT %d  %.3f S", ed_m + 1, mpos / (double)SR);
    rect(16, 526, WAVE_W, 8, C_LINE, 1);         // where the view is in the whole sample
    rect(16 + (int)((int64_t)ed_view * WAVE_W / s->n), 526, (int)((int64_t)span * WAVE_W / s->n) + 1, 8, C_DIM, 1);
    textf(16, 552, 2, C_WHITE, "START %d.%d%%   END %d.%d%%", t->start / 10, t->start % 10, t->end / 10, t->end % 10);
    textf(640, 552, 2, C_WHITE, "SLICE %d  %d MS", k + 1, (kb - ka) / (SR / 1000));
    text(16, 600, 2, C_DIM, "UP/DOWN ZOOM  EDIT+UP/DOWN MOVE MORE  Y BACK");
    draw_footer("<> POINT  EDIT+<> MOVE  EDIT SPLIT  OPT LISTEN  OPT+EDIT DEL");
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
    sel_card(682, 290, 310, 120, "MODE", p->mode ? "ROLL" : "STEPS", -1, sl_r == 2 && sl_c == 2);
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
    case J_SWING: snprintf(b, 32, "%d%%", song.swing); break;
    case J_LAUNCH: { static const char *const ln[5] = {"1 BAR", "1 BAR", "2 BARS", "4 BARS", "NONE"}; snprintf(b, 32, "%s", ln[song.launch % 5]); break; }
    case J_KEY: snprintf(b, 32, "%s", note_name[song.key]); break;
    case J_SCALE: snprintf(b, 32, "%s", scale_name[song.scale]); break;
    case J_METRO: snprintf(b, 32, song.metro ? "ON" : "OFF"); break;
    case J_SNAP: snprintf(b, 32, song.snap ? "ON" : "OFF"); break;
    case J_HINT: snprintf(b, 32, hint_on ? "ON" : "OFF"); break;
    case J_STRIP: snprintf(b, 32, ui_strip ? "THIN" : "HIDDEN"); break;
    case J_VIEW: snprintf(b, 32, ui_simple ? "SIMPLE" : "ADVANCED"); break;
    case J_LOCK: snprintf(b, 32, ui_lock ? "ON" : "OFF"); break;
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
static void draw_prj(void) {
    enum { SX = 16, SW = 220, MX = 248, MW = 468 };   // sections, settings, then the information panel
    { int r[6]; prj_rows(jsec, r); int ok = 0; for (int i = 0; i < 6 && prj_sec[jsec][i] >= 0; i++) if (r[i] == jcur) ok = 1; if (!ok) jcur = r[0]; }
    for (int k = 0; k < JS_N; k++) {     // the sections
        int y = 90 + k * 76, on = k == jsec;
        if (on && !jcol) plate(SX, y, SW, 68, 6, C_ORANGE);
        else if (on) { rect(SX, y, SW, 68, C_CELL, 1); rect(SX, y, 5, 68, C_ORANGE, 1); }
        else frame(SX, y, SW, 68, 1, C_LINE);
        text(SX + 16, y + 10, 3, on && !jcol ? C_BG : on ? C_WHITE : C_DIM, prj_sec_name[k]);
        text(SX + 16, y + 46, 1, on && !jcol ? C_BG : C_GREY, prj_sec_hint[k]);
    }
    int r[6], n = prj_rows(jsec, r);
    for (int j = 0; j < n; j++) {        // the settings of the section
        int i = r[j], y = 90 + j * 76, on = jcol && i == jcur; char b[32]; prj_value(i, b);
        if (on) rect(MX, y, MW, 68, C_ORANGE, 1); else frame(MX, y, MW, 68, 1, C_LINE);
        text(MX + 14, y + 8, 2, on ? C_BG : C_DIM, prj_name[i]);
        if (i == J_MASTER) {             // a fader, as on the mixer's master strip
            int fx = MX + 14, fw = MW - 120, fy = y + 44, cx = fx + fw * song.master / 100;
            rect(fx, fy, fw, 3, on ? mix(C_ORANGE, C_BG, 120) : C_LINE, 1); rect(fx, fy, cx - fx, 3, on ? C_BG : C_ORANGE, 1);
            for (int tk = 0; tk <= 4; tk++) rect(fx + fw * tk / 4, fy - 6, 1, 15, on ? C_BG : C_GREY, 1);
            plate(cx - 7, fy - 12, 14, 28, 3, on ? C_BG : C_WHITE);
            text(MX + MW - 14 - (int)strlen(b) * 24, y + 22, 3, on ? C_BG : C_WHITE, b);
        } else if (b[0]) text(MX + 14, y + 32, strlen(b) > 18 ? 2 : 3, on ? C_BG : i == J_PROJ && prj_dirty() ? C_ORANGE : C_WHITE, b);
    }
    if (jcol) {                          // what the setting under the cursor does
        int y = 90 + n * 76 + 10; rect(MX, y, MW, 1, C_LINE, 1);
        const char *h = prj_help[jcur]; int w = MW / 8 - 1, len = (int)strlen(h), line = 0;
        while (len > 0 && line < 4) { int c = len > w ? w : len; if (c < len) { while (c > 0 && h[c] != ' ') c--; if (!c) c = w; }
            char seg[80]; snprintf(seg, sizeof seg, "%.*s", c, h); text(MX + 2, y + 14 + line * 16, 1, C_GREY, seg); h += c; len -= c; while (*h == ' ') { h++; len--; } line++; }
    }
    draw_footer(!jcol ? "UP/DOWN SECTION  RIGHT OR EDIT: ITS SETTINGS"
                : jcur == J_TEMPO ? "EDIT+ARROWS TEMPO  EDIT TAP  HOLD L1/R1 NUDGE"
                : jcur == J_LIB ? (lib_state == 1 ? "EDIT STOP  SLOW ON PURPOSE, WAITS WHILE PLAYING" : "EDIT CHECK ALL FILES, RESUMES WHERE IT STOPPED")
                : jcur == J_THEME ? "EDIT+<> CHOOSE  EDIT COLOURS"
                : jcur == J_NEW ? "EDIT TWICE: EMPTY PROJECT  SHIFT+L2 UNDO"
                : jcur == J_PROJ ? "EDIT THE PROJECTS  LEFT: SECTIONS" : jcur == J_SAVE ? "EDIT SAVE  LEFT: SECTIONS" : jcur == J_SAVEAS ? "EDIT TYPE A NAME AND SAVE" : jcur == J_EXPORT ? "EDIT EXPORT OPTIONS"
                : jcur == J_MASTER ? "EDIT+<> 1  EDIT+UP/DN 10  LEFT: SECTIONS" : "EDIT+ARROWS CHANGE  LEFT: SECTIONS");
    enum { PX = 728, PW = 280, PT = 90, PB = 690 };   // information panel: the one quiet figure of the program
    frame(PX, PT, PW, PB - PT, 1, C_LINE);
    plate(PX, PT, 148, 24, 5, C_DIM); text(PX + 6, PT + 4, 2, C_BG, "SYSTEM");
    hatch(PX + 160, PT + 6, 108, 12, 8, 3, C_LINE);
    fig_put(fig_open, FIG_MID, PX + 40, PT + 44, 90);
    rect(PX, PT + 260, PW, 1, C_LINE, 1);
    text(PX + 16, PT + 276, 3, C_WHITE, "BRICKPAD");
    text(PX + 16, PT + 310, 1, C_DIM, "SAMPLER / SEQUENCER / V1.2");
    barcode(PX + 16, PT + 330, PW - 32, 28, 2u, C_GREY);
    rect(PX, PT + 374, PW, 1, C_LINE, 1);
    int used_slots = 0, playing = 0; for (int i = 0; i < NSLOT; i++) { if (has_sound(i)) used_slots++; if (song.t[i].on) playing++; }
    int iy = PT + 390;
    textf(PX + 16, iy - 4, 1, C_DIM, "PROJECT"); textf(PX + 80, iy - 4, 1, prj_dirty() ? C_ORANGE : C_WHITE, "%.20s%s", prj_cur[0] ? prj_cur : "UNTITLED", prj_dirty() ? " *" : ""); iy += 14;
    textf(PX + 16, iy, 2, C_DIM, "SLOTS");  textf(PX + 120, iy, 2, C_WHITE, "%d/%d", used_slots, NSLOT); iy += 30;
    textf(PX + 16, iy, 2, C_DIM, "CLIPS");  textf(PX + 120, iy, 2, C_WHITE, "%d ON", playing); iy += 30;
    textf(PX + 16, iy, 2, C_DIM, "KEY");    textf(PX + 120, iy, 2, C_WHITE, "%s", note_name[song.key]); iy += 30;
    textf(PX + 16, iy, 2, C_DIM, "THEME");  textf(PX + 120, iy, 2, C_ORANGE, "%.9s", theme_name(th_sel));
    hatch(PX + 1, PB - 40, PW - 2, 39, 12, 4, C_LINE);
}
static void draw_theme(void) {
    static const char *chan[3] = {"R", "G", "B"}, *act[3] = {"SAVE", "SAVE AS", "DELETE"};
    tag(16, 90, C_DIM, C_BG, "THEME"); textf(120, 94, 2, C_WHITE, "%s%s", theme_name(th_sel), th_mod ? " *" : "");
    text(500, 94, 2, C_GREY, th_mod ? "CHANGED, NOT SAVED" : th_sel < TH_PRESETS ? "BUILT-IN" : "YOURS");
    for (int c = 0; c < 3; c++) text(380 + c * 112 + 24, 122, 1, ted_col == c && ted_row < TC_N ? C_ORANGE : C_DIM, chan[c]);
    enum { LW = 724, VR = 11 };          // 19 colours, 11 rows on screen: the list scrolls with the cursor
    int first = ted_row < TC_N ? clampi(ted_row - VR / 2, 0, TC_N - VR) : TC_N - VR;
    if (first > 0) text(LW - 40, 122, 1, C_DIM, "^ MORE");
    if (first + VR < TC_N) text(LW - 40, 140 + VR * 44 - 6, 1, C_DIM, "v MORE");
    for (int r = first; r < first + VR; r++) {
        int y = 140 + (r - first) * 44, on = r == ted_row; Col col = th_cur[r];
        if (on) rect(16, y - 4, LW, 40, C_ORANGE, 1);
        text(28, y + 4, 3, on ? C_BG : C_DIM, th_color_name[r]);
        rect(288, y, 60, 32, col, 1); frame(288, y, 60, 32, 1, on ? C_BG : C_GREY);
        int v[3] = {col.r, col.g, col.b};
        for (int c = 0; c < 3; c++) {
            int x = 380 + c * 112, sel_c = on && c == ted_col;
            if (sel_c) rect(x - 6, y - 2, 84, 36, C_BG, 1);
            textf(x, y + 4, 3, sel_c ? C_ORANGE : on ? C_BG : C_WHITE, "%3d", v[c]);
        }
    }
    for (int a2 = 0; a2 < 3; a2++) {      // the bottom row: what to do with these colours
        int x = 16 + a2 * 244, y = 140 + VR * 44 + 6, on = ted_row == TC_N && ted_col == a2, dim = a2 == 2 && th_sel < TH_PRESETS;
        if (on) rect(x, y, 236, 44, C_ORANGE, 1); else frame(x, y, 236, 44, 1, C_LINE);
        ctext(x + 118, y + 10, 3, on ? C_BG : dim ? C_GREY : a2 == 2 ? C_RED : C_WHITE, act[a2]);
    }
    enum { PX = 752, PW = 256 };         // what the colours give: the same plates as everywhere
    frame(PX, 120, PW, 570, 1, C_LINE);
    tag(PX, 120, C_DIM, C_BG, "PREVIEW");
    fig_put(fig_open, FIG_MID, PX + 28, 160, 255);
    tag(PX + 16, 380, C_ORANGE, C_BG, "ACCENT"); tag(PX + 136, 380, C_PLAY, C_BG, "PLAY"); tag(PX + 16, 416, C_RED, C_BG, "ALERT");
    text(PX + 16, 456, 2, C_WHITE, "TEXT"); text(PX + 16, 480, 2, C_DIM, "DIM"); text(PX + 16, 504, 2, C_GREY, "GREY");
    rect(PX + 16, 536, PW - 32, 40, C_SELBG, 1); text(PX + 28, 548, 2, C_ORANGE, "SELECT");
    rect(PX + 16, 588, PW - 32, 30, C_CELL, 1); frame(PX + 16, 588, PW - 32, 30, 1, C_LINE);
    hatch(PX + 1, 636, PW - 2, 53, 12, 4, C_LINE);
    draw_footer(ted_row == TC_N ? "<> CHOOSE  EDIT DO IT  Y BACK" : "EDIT+<> 1  EDIT+^v 16  OPT+EDIT SAVED COLOUR  Y BACK");
}
static void prj_info_draw(const char *path);
static void draw_browse(void) {
    int bw = bmode ? 600 : 980;          // projects: the list, then what the project is
    plate(16, 89, 76, 24, 5, C_DIM); text(22, 93, 2, C_BG, bmode ? "PROJ" : "PATH");
    { int bl = (int)strlen(bdir);             // the deepest folder is the one that matters
        if (bl > 54) textf(104, 93, 2, C_WHITE, "...%s", bdir + bl - 51); else text(104, 93, 2, C_WHITE, bdir); }
    if (bn) textf(1008 - 16 * 11, 93, 2, C_GREY, "%5d/%-5d", bcur + 1, bn);
    int rows = 13;
    if (bcur < btop) btop = bcur;
    if (bcur >= btop + rows) btop = bcur - rows + 1;
    rect(16, 116, 992, 1, C_LINE, 1);
    for (int i = 0; i < rows && btop + i < bn; i++) {
        int k = btop + i, y = 124 + i * 43, on = k == bcur;
        if (on) rect(16, y - 6, bw, 42, C_ORANGE, 1);
        else rect(16, y + 36, bw, 1, C_CELL, 1);
        char fp[1024]; snprintf(fp, sizeof fp, "%s/%s", bdir, bent[k].name);
        int bad = !bent[k].dir && idx_bad(fp), fill = on && browse_hold && ms_since(SDL_GetTicks(), browse_hold) > 150;
        if (fill) rect(16, y + 30, clampi(ms_since(SDL_GetTicks(), browse_hold), 0, HOLD_MS) * bw / HOLD_MS, 6, browse_rand ? C_WHITE : C_BG, 1);
        icon(bad ? IC_BAD : bent[k].dir ? IC_DIR : file_cat(fp), 26, y - 1, on ? C_BG : bad ? C_RED : bent[k].dir ? C_DIM : C_GREY);
        char nm[48]; int nl = (int)strlen(bent[k].name);   // files of a pack often differ only at the end of the name
        if (bmode && !bent[k].dir && nl > 4) nl -= 4;   // projects: no .bpp
        if (nl > (bmode ? 22 : 36)) snprintf(nm, sizeof nm, bmode ? "%.12s..%.8s" : "%.20s..%s", bent[k].name, bent[k].name + nl - (bmode ? 8 : 14));
        else if (bmode) snprintf(nm, sizeof nm, "%.*s", nl, bent[k].name);
        else snprintf(nm, sizeof nm, "%s", bent[k].name);
        text(72, y + 3, 3, on ? C_BG : bad ? C_RED : bent[k].dir ? C_WHITE : C_DIM, nm);
    }
    if (bn > rows) {                     // where we are in a long folder
        int th = rows * 43, tw = th * rows / bn; if (tw < 16) tw = 16;
        rect(16 + bw + 6, 118, 6, th, C_LINE, 1);
        rect(16 + bw + 6, 118 + (th - tw) * btop / (bn - rows), 6, tw, C_DIM, 1);
    }
    if (!bn) { hatch(16, 124, bw, 60, 16, 5, C_LINE); tag(28, 142, C_DIM, C_BG, bmode ? "NO PROJECTS YET: SAVE ONE IN PROJECT > FILE" : "EMPTY"); }
    if (bmode) { if (bcur < bn && !bent[bcur].dir) { char fp[1024]; snprintf(fp, sizeof fp, "%s/%s", bdir, bent[bcur].name); prj_info_draw(fp); }
        if (!fmenu && !fconfirm) draw_footer("EDIT OPEN  X FILE MENU  < UP  OPT BACK"); draw_fmenu(); return; }
    if (!fmenu && !fconfirm) draw_footer("EDIT LOAD/OPEN  X FILES  HOLD KIT  < UP  OPT BACK");
    draw_fmenu();
}
static void draw_icons(void) {           // the palette, one solid cell under the cursor
    enum { GX = 16, GY = 118, C = 66, PX = 816, PW = 192 };
    enum { rows = 8 };                   // the largest group fits: mkicons_music.py checks it
    int now = track_icon(&song.t[sel]) - 1, g = icon_group(ic_cur), a = icon_grp_at[g], n = icon_grp_at[g + 1] - a;
    for (int i = 0, x = 16; i < ICON_GRP_N; i++) {      // the groups, L1/R1
        if (i == g) x += tag(x, 89, C_ORANGE, C_BG, icon_grp_name[i]);
        else { text(x + 6, 93, 2, C_DIM, icon_grp_name[i]); x += (int)strlen(icon_grp_name[i]) * 16 + 12; }
        x += 6;
    }
    cell_lines(GX, GY, C, C, ICON_COLS, rows);
    for (int c = 0; c < ICON_COLS * rows; c++) {
        int x = GX + (c % ICON_COLS) * C, y = GY + (c / ICON_COLS) * C;
        if (c >= n) { hatch(x + 1, y + 1, C - 1, C - 1, 10, 1, C_LINE); continue; }
        int i = a + c, on = i == ic_cur, id = icon_pal[i];
        if (on) rect(x, y, C + 1, C + 1, C_ORANGE, 1);
        if (id < 0) text(x + 17, y + 29, 1, on ? C_BG : C_DIM, "AUTO");
        else icon_sc(id, x + 9, y + 9, 3, on ? C_BG : C_WHITE);
        if (id == now) { rect(x + 4, y + 4, 8, 8, on ? C_BG : C_ORANGE, 1); }   // what the pad has now
    }
    int id = icon_pal[ic_cur], show = id >= 0 ? id : slot_icon(sel);
    frame(PX, GY, PW, rows * C + 1, 1, C_LINE);
    tag(PX, GY, C_DIM, C_BG, "PAD"); text(PX + 72, GY + 4, 2, C_WHITE, pad_label[cell_key[sel]]);
    icon_sc(show, PX + 32, GY + 48, 8, C_ORANGE);
    const char *nm = id >= 0 ? icon_tab[id].name : "AUTO";
    text(PX + 96 - (int)strlen(nm) * 8, GY + 196, 2, C_WHITE, nm);
    if (id < 0) text(PX + 16, GY + 224, 1, C_DIM, "BY SOUND TYPE");
    rect(PX, GY + 244, PW, 1, C_LINE, 1);
    barcode(PX + 16, GY + 260, PW - 32, 24, (unsigned)ic_cur + 3u, C_GREY);
    textf(PX + 16, GY + 300, 2, C_DIM, "%03d/%03d", ic_cur + 1, ICON_PAL_N);
    hatch(PX + 1, GY + rows * C - 40, PW - 2, 40, 12, 4, C_LINE);
    draw_footer("ARROWS CHOOSE  L1/R1 GROUP  EDIT SET  OPT LISTEN  Y BACK");
}
static double draw_build_ms, draw_present_ms;   // summed over the frames since the last log line
static Uint32 last_input;                // the last button or d-pad event
static int frame_rate(Uint32 now) {      // frames per second for what is happening now
    int keys = 0; for (int k = 0; k < K_N; k++) keys |= held[k];
    if (keys || (last_input && ms_since(now, last_input) < 500) || browse_hold || menu_down) return 30;   // UI v2: never over 30
    if (engine_playing() || engine_counting() >= 0 || engine_voices() > 0 || (msg_t && now - msg_t < 1600) || marq_live) return 30;
    for (int i = 0; i < NSLOT; i++) if (flash[i] && now - flash[i] < 200) return 30;
    return 10;
}
// ---------- MIXER and FX screens ----------
static float mx_show[NSLOT + 3];         // meters as drawn: peaks with a fall
static void mx_meters(void) {
    float m[NSLOT + 3]; engine_meters(m);
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
static void mx_cell(int x, int y, int w, int h, int on) { if (on) rect(x, y, w, h, C_ORANGE, 1); }
static void draw_mixer(void) {
    mx_meters();
    enum { PW = 62, RW = 76, TY = 88, RY = 146 };
    static const int ry[MX_ROWS] = {146, 176, 210, 242, 274, 306, 338, 588, 620, 652}, rh[MX_ROWS] = {28, 28, 30, 30, 30, 30, 246, 30, 30, 30};   // FRZ (2026-11-11) took 32 px of the fader
    for (int c = 0; c < NSLOT + 3; c++) {
        int x = c < NSLOT ? 16 + c * PW : 16 + NSLOT * PW + 16 + (c - NSLOT) * RW, w = c < NSLOT ? PW : RW;
        rect(x + w - 1, TY, 1, 594, C_LINE, 1);
        // the head: number and icon, or the return's name
        char nb[8];
        if (c < NSLOT) { snprintf(nb, 8, "%02d", c + 1); int cur = c == sel;
            if (cur) plate(x + 4, TY, 36, 22, 4, C_WHITE); text(x + 6, TY + 3, 2, cur ? C_BG : C_DIM, nb);
            icon(slot_icon(c), x + 14, TY + 24, has_sound(c) ? (song.t[c].mute ? C_GREY : C_WHITE) : C_LINE); }
        else text(x + 6, TY + 3, 2, C_DIM, c == NSLOT ? "A" : c == NSLOT + 1 ? "B" : "MST");
        if (c >= NSLOT) text(x + 6, TY + 30, 1, C_GREY, c == NSLOT ? "REVERB" : c == NSLOT + 1 ? "DELAY" : "MASTER");
        for (int r = 0; r < MX_ROWS; r++) {
            int y = ry[r], h = rh[r], on = mcol == c && mrow == r; char b[16] = ""; Col tc = on ? C_BG : C_WHITE;
            if (r != MX_VOL) mx_cell(x + 2, y, w - 5, h - 2, on);
            if (c < NSLOT) {
                const Track *t = &song.t[c]; const PadMix *pm = &song.mix.pad[c];
                switch (r) {
                case MX_INS1: case MX_INS2: { const FxSlot *f = &pm->ins[r];
                    int live = ui_fx_live(c, r);
                    text(x + 7, y + 6, 2, on ? C_BG : !f->type ? C_LINE : !f->on ? C_GREY : live ? C_WHITE : C_RED, fx_short[f->type]);
                    if (f->type && f->on && !live && !on) hatch(x + 2, y, w - 5, h - 2, 8, 1, C_RED);
                    break; }
                case MX_SA: case MX_SB: { int v = r == MX_SA ? pm->send_a : pm->send_b;
                    rect(x + 6, y + 18, w - 13, 6, on ? mix(C_BG, C_ORANGE, 120) : C_CELL, 1);
                    rect(x + 6, y + 18, (w - 13) * v / 100, 6, on ? C_BG : v ? C_ORANGE : C_CELL, 1);
                    text(x + 6, y + 4, 1, on ? C_BG : C_DIM, r == MX_SA ? "A" : "B"); snprintf(b, 8, "%d", v); text(x + w - 8 - (int)strlen(b) * 8, y + 4, 1, tc, b); break; }
                case MX_DUCK: { int grid = song.dkx[c][DX_MARK] == DX_INIT && song.dkx[c][DX_MODE] == DM_GRID, src = grid ? 99 : pm->spare[DK_SRC], am = src ? pm->spare[DK_AMT] : 0;   // "DK" and the pad that ducks this one; the bar: how much
                    rect(x + 6, y + 18, w - 13, 6, on ? mix(C_BG, C_ORANGE, 120) : C_CELL, 1);
                    rect(x + 6, y + 18, (w - 13) * am / 100, 6, on ? C_BG : am ? C_PLAY : C_CELL, 1);
                    text(x + 6, y + 4, 1, on ? C_BG : C_DIM, "DK"); if (src == 99) snprintf(b, 8, "GRD"); else if (src) snprintf(b, 8, "<%02d", src); else snprintf(b, 8, "--"); text(x + w - 8 - (int)strlen(b) * 8, y + 4, 1, src ? tc : on ? C_BG : C_GREY, b); break; }
                case MX_PAN: { int cx = x + w / 2 - 2, px = cx + t->pan * (w / 2 - 8) / 50;
                    rect(x + 6, y + 14, w - 13, 2, on ? C_BG : C_LINE, 1); rect(cx, y + 8, 2, 14, on ? C_BG : C_GREY, 1); rect(px - 3, y + 8, 7, 14, on ? C_BG : C_ORANGE, 1); break; }
                case MX_MUTE: if (t->mute && !on) plate(x + 2, y, w - 5, h - 2, 4, C_GREY); text(x + w / 2 - 8, y + 7, 2, on || t->mute ? C_BG : C_DIM, "M"); break;
                case MX_SOLO: if (pm->solo && !on) plate(x + 2, y, w - 5, h - 2, 4, C_ORANGE); text(x + w / 2 - 8, y + 7, 2, on || pm->solo ? C_BG : C_DIM, "S"); break;
                case MX_FRZ: { int fz = frozen(c); if (fz && !on) plate(x + 2, y, w - 5, h - 2, 4, frz_have[c] ? C_PLAY : C_GREY); text(x + w / 2 - 8, y + 7, 2, on || fz ? C_BG : C_DIM, "F"); break; }
                }
            } else if (c < NSLOT + 2) {
                const int8_t *pr = c == NSLOT ? song.mix.rv : song.mix.dl;
                switch (r) {
                case MX_INS1: text(x + 7, y + 6, 2, tc, c == NSLOT ? "REV" : "DLY"); break;
                case MX_SA: case MX_SB: case MX_DUCK: case MX_INS2: case MX_SOLO: case MX_FRZ: hatch(x + 2, y, w - 5, h - 2, 10, 1, C_LINE); break;
                case MX_PAN: snprintf(b, 16, "W%d", pr[c == NSLOT ? RV_WIDTH : DL_WIDTH]); text(x + 7, y + 8, 2, tc, b); break;
                case MX_MUTE: { int m = pr[c == NSLOT ? RV_MUTE : DL_MUTE]; if (m && !on) plate(x + 2, y, w - 5, h - 2, 4, C_GREY); text(x + w / 2 - 8, y + 7, 2, on || m ? C_BG : C_DIM, "M"); break; }
                }
            } else {
                switch (r) {
                case MX_INS1: case MX_INS2: { int v = song.mix.ms[r == MX_INS1 ? MS_COMP : MS_LIM]; text(x + 7, y + 6, 2, on ? C_BG : v ? C_WHITE : C_GREY, r == MX_INS1 ? "CMP" : "LIM"); break; }
                case MX_PAN: { float gr = engine_master_gr(); snprintf(b, 16, "%.1f", gr); text(x + 6, y + 4, 1, on ? C_BG : C_DIM, "GR dB"); text(x + 6, y + 15, 1, tc, b); break; }
                default: if (r != MX_VOL) hatch(x + 2, y, w - 5, h - 2, 10, 1, C_LINE);   // the fader row stays clear: its value is read there
                }
            }
            if (r == MX_VOL) {           // the fader and the meter beside it
                int v = c < NSLOT ? song.t[c].vol : c == NSLOT ? song.mix.rv[RV_LEVEL] : c == NSLOT + 1 ? song.mix.dl[DL_LEVEL] : song.master;
                int fx = x + 10, fy = y + 6, fh = h - 46, cy = fy + fh - fh * v / 100;
                if (on) rect(x + 2, y, w - 5, h - 2, mix(C_BG, C_ORANGE, 60), 1);
                rect(fx + 8, fy, 3, fh, C_LINE, 1);
                for (int tk = 0; tk <= 4; tk++) rect(fx + 2, fy + fh * tk / 4, 15, 1, C_GREY, 1);
                plate(fx, cy - 6, 20, 12, 3, on ? C_ORANGE : C_WHITE);
                meter_bar(x + w - 22, fy, 12, fh, mx_show[c < NSLOT ? c : c == NSLOT + 2 ? NSLOT + 2 : c]);
                snprintf(b, 8, "%d", v); text(x + w / 2 - (int)strlen(b) * 8 - 2, y + h - 30, 2, on ? C_ORANGE : C_WHITE, b);
            }
        }
    }
    { int u = fx_budget_used(); char b[40]; snprintf(b, sizeof b, "FX %d%%", (u + 5) / 10); text(MIX_FXX, MIX_FXY, 1, u >= FX_BUDGET ? C_RED : C_GREY, b); }   // what the inserts take of the processor when all sound (the limit is 40 %)
    const char *h = mcol < NSLOT ? (mrow == MX_DUCK ? "EDIT DUCK WINDOW  EDIT+<> SOURCE  EDIT+^v AMOUNT  OPT+EDIT OFF" : mrow <= MX_INS2 ? "EDIT FX  EDIT+<> TYPE  EDIT+^v ON  Y MUTE  X SOLO"
                                   : mrow >= MX_MUTE ? "EDIT TOGGLE  Y MUTE  X SOLO  L1/R1 GROUP"
                                   : "EDIT+ARROWS VALUE  OPT+EDIT RESET  Y MUTE  X SOLO")
                  : mcol < NSLOT + 2 ? "EDIT FX PAGE  EDIT+ARROWS LEVEL/WIDTH  L1/R1 GROUP" : "EDIT+ARROWS VOL  EDIT+<> CMP/LIM  EDIT FX PAGE";
    draw_footer(h);
}
static void fx_pstr(int type, int k, int v, char *b) {   // a parameter as the user reads it
    switch (type) {
    case FX_DIST: if (k == 0) { snprintf(b, 16, "%s", dist_mode[v & 3]); return; } break;
    case FX_FILTER: if (k == 0) { snprintf(b, 16, "%s", filt_mode[v & 3]); return; } if (k == 4) { snprintf(b, 16, "%s", v ? div_label[(v - 1) % NDIV] : "OFF"); return; } break;
    case FX_PHASER: if (k == 4) { snprintf(b, 16, "%d", 4 + 2 * v); return; } break;
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
static void param_row(int x, int y, int w, const char *name, const char *val, int v, int lo, int hi, int on, int bar) {
    if (on) rect(x, y, w, 44, C_ORANGE, 1);
    text(x + 10, y + 6, 2, on ? C_BG : C_DIM, name);
    text(x + w - 12 - (int)strlen(val) * 16, y + 6, 2, on ? C_BG : C_WHITE, val);
    if (bar && hi > lo) { rect(x + 10, y + 32, w - 20, 4, on ? mix(C_BG, C_ORANGE, 120) : C_CELL, 1); rect(x + 10, y + 32, (w - 20) * (v - lo) / (hi - lo), 4, on ? C_BG : C_ORANGE, 1); }
}
static void draw_mod(void) {
    char b[48]; int u = fx_unit;
    snprintf(b, sizeof b, "MOD %02d %.14s", u + 1, slot_name(u) ? slot_name(u) : "--"); text(16, 92, 2, C_WHITE, b);
    text(1008 - 6 * 16, 92, 2, C_DIM, "Y: FX");
    rect(16, 120, 992, 1, C_LINE, 1);
    for (int j = 0; j < NLFO; j++) {
        int x = 16 + j * 248, w = 240; const Lfo *L = &song.lfo[u][j]; int act = L->target && L->depth;
        snprintf(b, sizeof b, "LFO %d", j + 1); text(x + 8, 128, 2, act ? C_ORANGE : C_GREY, b);
        {   // its value now: a bar from the middle
            float v = act ? engine_lfo(u, j) : 0; int cx = x + w / 2, bw = (int)(v * (w / 2 - 10));
            rect(x + 10, 154, w - 20, 8, C_CELL, 1); rect(cx, 152, 1, 12, C_GREY, 1);
            if (bw > 0) rect(cx, 154, bw, 8, C_ORANGE, 1); else if (bw < 0) rect(cx + bw, 154, -bw, 8, C_ORANGE, 1);
        }
        for (int r = 0; r < LR_N; r++) {
            int y = r == 0 ? 172 : 172 + 64 + (r - 1) * 50, h = r == 0 ? 60 : 46, on = lcol == j && lrow == r;
            if (on) rect(x, y, w, h, C_ORANGE, 1);
            lfo_str(u, j, r, b);
            if (r == 0) { text(x + 8, y + 6, 1, on ? C_BG : C_DIM, lr_name[r]); text(x + 8, y + 24, 2, on ? C_BG : L->target ? C_WHITE : C_GREY, b); }
            else { text(x + 8, y + 4, 1, on ? C_BG : C_DIM, lr_name[r]); text(x + 8, y + 18, 2, on ? C_BG : act ? C_WHITE : C_GREY, b); }
        }
        if (j) rect(x - 4, 124, 1, 560, C_LINE, 1);
    }
    draw_footer(lrow == LR_TARGET ? "EDIT LEARN  EDIT+<> TARGET  OPT+EDIT OFF  Y FX" : "EDIT+<> 1  EDIT+^v 10  OPT+EDIT OFF  Y FX");
}
// The screen under an insert (2026-11-09): what the effect does to the sound, live where the engine says.
static void fx_screen(int x, int y, int w, int h, int slot, int i) {
    const FxSlot *f = &song.mix.pad[slot].ins[i]; float v[6]; engine_fx_view(slot, i, v);
    rect(x, y, w, h, C_CELL, 1); frame(x, y, w, h, 1, C_LINE);
    int live = f->on && ui_fx_live(slot, i) && engine_playing();
    switch (f->type) {
    case FX_COMP: {                      // input dB (-60..0) to output dB: the curve, the threshold, where the signal is
        float th = -40 + 40 * f->p[0] / 100.f, ra = 1 + 19 * (f->p[1] / 100.f) * (f->p[1] / 100.f), mk = 24 * f->p[4] / 100.f;
        int gx = x + 8, gy = y + 6, gw = h - 12, gh = h - 12, px = 0, py = 0;
        rect(gx, gy, gw, gh, C_BG, 1); for (int q = 1; q < 4; q++) { rect(gx + gw * q / 4, gy, 1, gh, C_LINE, 1); rect(gx, gy + gh * q / 4, gw, 1, C_LINE, 1); }
        line(gx, gy + gh, gx + gw, gy, 1, C_GREY);   // 1:1
        for (int s2 = 0; s2 <= gw; s2++) { float in = -60 + 60.f * s2 / gw, out = (in > th ? th + (in - th) / ra : in) + mk;
            int qy = gy + gh - (int)((out + 60) / 60 * gh); if (qy < gy) qy = gy; if (qy > gy + gh) qy = gy + gh;
            if (s2) line(px, py, gx + s2, qy, 2, C_ORANGE); px = gx + s2; py = qy; }
        int tx = gx + (int)((th + 60) / 60 * gw); for (int yy = gy; yy < gy + gh; yy += 4) rect(tx, yy, 1, 2, C_WHITE, 1);
        if (live && v[0] > -60) { float in = v[0] > 0 ? 0 : v[0], out = in - v[1] + mk; int dx = gx + (int)((in + 60) / 60 * gw), dy = gy + gh - (int)((out + 60) / 60 * gh);
            if (dy < gy) dy = gy; disc(dx, dy, 5, C_WHITE); }
        int bx = gx + gw + 16; char b[32];
        snprintf(b, sizeof b, "THRESH %.0f DB  RATIO %.1f:1", th, ra); text(bx, y + 8, 1, C_DIM, b);
        text(bx, y + 26, 1, C_GREY, "GAIN REDUCTION");
        { int bw = w - (bx - x) - 12; float gr = live ? v[1] : 0; if (gr > 24) gr = 24; rect(bx, y + 40, bw, 12, C_BG, 1); rect(bx + bw - (int)(bw * gr / 24), y + 40, (int)(bw * gr / 24), 12, C_ORANGE, 1);
          snprintf(b, sizeof b, "-%.1f DB", gr); text(bx, y + 58, 2, live && gr > 0.1f ? C_ORANGE : C_GREY, b); }
        break; }
    case FX_FILTER: {                    // the response, 20 Hz .. 20 kHz, -36..+18 dB; the line: the cutoff now (the LFO moves it)
        float fc = 20 * exp2f(f->p[1] / 100.f * 10), kk = 2 - 2 * (f->p[2] / 100.f) * 0.98f; int px = 0, py = 0, mode = f->p[0] & 3;
        for (int s2 = 0; s2 <= w - 16; s2++) { float hz = 20 * powf(1000, (float)s2 / (w - 16)), u = hz / fc, re = 1 - u * u, im = kk * u, d = re * re + im * im, m2;
            m2 = mode == 1 ? u * u * u * u / d : mode == 2 ? u * u / d : mode == 3 ? re * re / d : 1 / d;
            float db = 10 * log10f(m2 + 1e-9f); if (db > 18) db = 18; if (db < -36) db = -36;
            int qx = x + 8 + s2, qy = y + 6 + (int)((18 - db) / 54 * (h - 12)); if (s2) line(px, py, qx, qy, 2, C_ORANGE); px = qx; py = qy; }
        float now = live && v[0] > 0 ? v[0] : fc; int lx = x + 8 + (int)(log10f(now / 20) / 3 * (w - 16));
        if (lx > x + 8 && lx < x + w - 8) rect(lx, y + 4, 2, h - 8, C_WHITE, 1);
        char b[24]; if (now < 1000) snprintf(b, sizeof b, "%.0f HZ", now); else snprintf(b, sizeof b, "%.1f KHZ", now / 1000); text(x + w - 8 - (int)strlen(b) * 8, y + 6, 1, C_DIM, b);
        break; }
    case FX_MULTI: {                     // three bands: the level in (bar), the gain now (up orange, down white), the crossovers
        static const char *const bn[3] = {"LOW", "MID", "HIGH"}; int bw = (w - 16) / 3;
        for (int bnd = 0; bnd < 3; bnd++) {
            int bx = x + 8 + bnd * bw, cy = y + h / 2 + 6; char b[24];
            text(bx + 4, y + 6, 1, C_DIM, bn[bnd]);
            float lv = live ? v[bnd] : -90, g = live ? v[3 + bnd] : 0; if (lv < -72) lv = -72; if (lv > 0) lv = 0;
            int lw = (int)((lv + 72) / 72 * (bw - 20)); rect(bx + 4, y + 20, bw - 20, 8, C_BG, 1); rect(bx + 4, y + 20, lw, 8, C_GREY, 1);
            int tx = bx + 4 + (int)((mb_thresh[bnd] + 72) / 72 * (bw - 20)); rect(tx, y + 18, 1, 12, C_WHITE, 1);
            if (g > 24) g = 24; if (g < -24) g = -24; int gh = (int)(fabsf(g) / 24 * (h / 2 - 14));
            rect(bx + 4, cy, bw - 20, 1, C_LINE, 1);
            if (g >= 0) rect(bx + bw / 2 - 12, cy - gh, 16, gh, C_ORANGE, 1); else rect(bx + bw / 2 - 12, cy, 16, gh, C_WHITE, 1);
            snprintf(b, sizeof b, "%+.0f DB", g); text(bx + bw / 2 + 10, cy + 4, 1, C_DIM, b);   // below the 0 dB line, not on it
            if (bnd) { for (int yy = y + 4; yy < y + h - 4; yy += 4) rect(bx - 1, yy, 1, 2, C_LINE, 1); }
        }
        char b[48], l1[16], l2[16]; fx_pstr(FX_MULTI, 4, f->p[4], l1); fx_pstr(FX_MULTI, 5, f->p[5], l2); snprintf(b, sizeof b, "SPLIT %s / %s", l1, l2);
        text(x + w - 8 - (int)strlen(b) * 8, y + h - 14, 1, C_GREY, b);
        break; }
    default: ctext(x + w / 2, y + h / 2 - 4, 1, C_GREY, f->type ? "" : "NO EFFECT"); break;
    }
}
static void draw_fx(void) {
    mx_meters(); char b[48];
    int u = fx_unit;
    if (u < NSLOT && fx_page) { draw_mod(); return; }
    if (u < NSLOT) {
        snprintf(b, sizeof b, "FX %02d %.14s", u + 1, slot_name(u) ? slot_name(u) : "--"); text(16, 92, 2, C_WHITE, b);
        const PadMix *pm = &song.mix.pad[u];
        {   // the chain, right
            char ch[48]; snprintf(ch, sizeof ch, "IN > %s > %s > OUT", pm->ins[0].on ? fx_short[pm->ins[0].type] : "---", pm->ins[1].on ? fx_short[pm->ins[1].type] : "---");
            text(1008 - (int)strlen(ch) * 16, 92, 2, C_DIM, ch); text(380, 92, 2, C_GREY, "Y: MOD");
        }
        for (int c = 0; c < NINS; c++) {
            int x = 16 + c * 500, w = 488; const FxSlot *f = &pm->ins[c]; int live = ui_fx_live(u, c);
            rect(x, 120, w, 1, C_LINE, 1);
            int on = fcol == c && frow == 0;
            if (on) rect(x, 124, w, 48, C_ORANGE, 1);
            snprintf(b, sizeof b, "%d", c + 1); text(x + 10, 128, 2, on ? C_BG : C_GREY, b);
            text(x + 40, 128, 3, on ? C_BG : f->type ? C_WHITE : C_GREY, fx_name[f->type]);
            on = fcol == c && frow == 1;
            if (on) rect(x, 174, w, 36, C_ORANGE, 1);
            text(x + 10, 182, 2, on ? C_BG : C_DIM, "ON");
            { const char *st = !f->type ? "--" : !f->on ? "BYPASS" : live ? "ON" : "OVER BUDGET"; text(x + w - 12 - (int)strlen(st) * 16, 182, 2, on ? C_BG : f->on && live ? C_ORANGE : f->on ? C_RED : C_GREY, st); }
            for (int k = 0; k < 6; k++) {
                int y = 214 + k * 48; on = fcol == c && frow == 2 + k;
                if (!f->type || !fx_pname[f->type][k][0]) { if (on) rect(x, y, w, 44, mix(C_BG, C_ORANGE, 60), 1); continue; }
                int lo, hi; fx_range(f->type, k, &lo, &hi); fx_pstr(f->type, k, f->p[k], b);
                if (f->type == FX_COMP && k == 0) {   // THRESH left, KEY right: what the compressor listens to (another pad = sidechain)
                    param_row(x, y, w / 2 - 4, fx_pname[f->type][k], b, f->p[k], lo, hi, on && !fx_sub, 1);
                    int kp = song.mbx[u][c][5]; if (kp > 0 && kp <= NSLOT) snprintf(b, sizeof b, "PAD %d", kp); else snprintf(b, sizeof b, "SELF");
                    param_row(x + w / 2 + 4, y, w / 2 - 4, "SRC", b, kp, 0, NSLOT, on && fx_sub, 0); continue;
                }
                if (f->type == FX_MULTI) {      // two halves: the dynamics left, the gains right (Song.mbx)
                    param_row(x, y, w / 2 - 4, fx_pname[f->type][k], b, f->p[k], lo, hi, on && !fx_sub, 1);
                    if (k < MB_N) { int g = song.mbx[u][c][k]; snprintf(b, sizeof b, g ? "%+d DB" : "0 DB", g); param_row(x + w / 2 + 4, y, w / 2 - 4, mb_name[k], b, g + 24, 0, 48, on && fx_sub, 1); }
                    continue;
                }
                param_row(x, y, w, fx_pname[f->type][k], b, f->p[k], lo, hi, on, 1);
            }
        }
        rect(16, 506, 992, 1, C_LINE, 1);
        static const char *bn[4] = {"SEND A", "SEND B", "PAN", "VOL"};
        for (int i = 0; i < 4; i++) {
            int v = i == 0 ? pm->send_a : i == 1 ? pm->send_b : i == 2 ? song.t[u].pan : song.t[u].vol, lo = i == 2 ? -50 : 0;
            snprintf(b, 16, "%d", v); param_row(16 + i * 248, 512, 240, bn[i], b, v, lo, i == 2 ? 50 : 100, frow == 8 && fcol == i, 1);
        }
        meter_bar(16, 568, 992, 0, 0);
        { int mw = (int)(mx_show[u] * 900); if (mw > 980) mw = 980; rect(16, 572, 992, 8, C_CELL, 1); rect(16, 572, mw, 8, mx_show[u] > 0.89f ? C_RED : C_PLAY, 1); text(16, 588, 1, C_GREY, "PAD OUT"); }
        for (int c = 0; c < NINS; c++) fx_screen(16 + c * 500, 602, 488, 86, u, c);   // what each insert does
        draw_footer(frow == 0 ? "EDIT/EDIT+<> TYPE  OPT+EDIT NONE  X+<> PAD  L1/R1 UNIT" : frow == 1 ? "EDIT ON/BYPASS  X+<> PAD  L1/R1 UNIT  F1/F2 TABS"
                    : "EDIT+<> 1  EDIT+^v 10  OPT+EDIT DEFAULT  X+<> PAD  L1/R1 UNIT");
        return;
    }
    const char *nm = u == NSLOT ? "RETURN A  REVERB" : u == NSLOT + 1 ? "RETURN B  DELAY" : "MASTER  COMP + LIMITER";
    text(16, 92, 2, C_WHITE, nm);
    rect(16, 120, 992, 1, C_LINE, 1);
    int n = unit_nparams(u), half = (n + 1) / 2;
    for (int kk = 0; kk < n; kk++) {
        int c = kk / half, r = kk % half, lo, hi, def; int8_t *v = unit_param(u, kk, &lo, &hi, &def);
        const char *pn = u == NSLOT ? rv_name[kk] : u == NSLOT + 1 ? dl_name[kk] : ms_name[kk];
        unit_pstr(u, kk, *v, b); param_row(16 + c * 500, 128 + r * 52, 488, pn, b, *v, lo, hi, fcol == c && frow == r, hi > 1);
    }
    { int mi = u == NSLOT + 2 ? NSLOT + 2 : u; int mw = (int)(mx_show[mi] * 900); if (mw > 980) mw = 980; rect(16, 572, 992, 8, C_CELL, 1); rect(16, 572, mw, 8, mx_show[mi] > 0.89f ? C_RED : C_PLAY, 1); text(16, 588, 1, C_GREY, u == NSLOT + 2 ? "MASTER OUT" : "RETURN OUT"); }
    if (u == NSLOT + 2) { snprintf(b, sizeof b, "GAIN REDUCTION %.1f DB", engine_master_gr()); text(16, 610, 2, C_DIM, b); }
    draw_footer("EDIT+<> 1  EDIT+^v 10  EDIT ON/OFF  L1/R1 UNIT");
}


// ---------- the instrument picker: EDIT on an empty pad (2026-11-09) ----------
// Four tiles, each with a miniature of the instrument's real SEQ+INST page, drawn once when the window opens:
// the pad is set to each instrument for a moment (only while its clip is off, so the engine plays nothing of it).
static const char *const pick_name[4] = {"SAMPLER", "GRAND", "ACID", "CLOUD"};
static const char *const pick_what[4] = {"SAMPLES, LOOPS, SLICES", "TWO OSCILLATORS, LADDER FILTER", "303-STYLE BASS, ACCENT, SLIDE", "GRAINS FROM A SAMPLE"};
static int pk_cur; static SDL_Texture *pk_tex[4];
enum { PK_SY = 250, PK_SH = 694 - 250 };   // what a miniature shows: the panel below the steps
static void pick_thumbs(void) {
    if (!SDL_RenderTargetSupported(R) || song.t[sel].on) return;
    static Track keep; keep = song.t[sel];
    int ks = srow, kc = scol, kp = spage, kl = lane, kd = cl_deep, kg = gb_cur;
    gb_direct = 1;
    for (int i = 0; i < 4; i++) {
        if (!pk_tex[i]) pk_tex[i] = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
        if (!pk_tex[i]) break;
        Track *t = &song.t[sel]; t->type = (int8_t)i; t->on = 0;
        if (i == 1) synth_defaults(t->sp); if (i == 2) acid_defaults(t->ap); if (i == 3) cl_defaults(t->cp);
        srow = 3; scol = 0; spage = 0; lane = lane_first(); cl_deep = 0; gb_cur = 0;
        SDL_SetRenderTarget(R, pk_tex[i]);
        SDL_SetRenderDrawColor(R, C_BG.r, C_BG.g, C_BG.b, 255); SDL_RenderClear(R);
        draw_seq();
    }
    SDL_SetRenderTarget(R, NULL);
    gb_direct = 0; song.t[sel] = keep;
    srow = ks; scol = kc; spage = kp; lane = kl; cl_deep = kd; gb_cur = kg;
}
static void pick_open(void) { picking = 1; pk_cur = 0; }   // UI v2 (A03): a list, no panel pictures (they took 12 MB)
static void pick_close(void) {
    picking = 0;
    for (int i = 0; i < 4; i++) if (pk_tex[i]) { SDL_DestroyTexture(pk_tex[i]); pk_tex[i] = NULL; }   // 3 MB each
}
static void hub_open(void);
static void pick_take(int i) {
    pick_close();
    if (i == 0) { browse_slot(); return; }
    Track *t = &song.t[sel]; edit_begin(); t->type = (int8_t)i;
    if (i == 1 && t->sp[SP_MARK] != SP_INIT) synth_defaults(t->sp);
    if (i == 2 && t->ap[AP_MARK] != AP_INIT) acid_defaults(t->ap);
    if (i == 3 && t->cp[CP_MARK] != CP_INIT) cl_defaults(t->cp);
    cl_deep = 0; hub_open(); hub_cur = PC_PATTERN;   // UI v2: the pad is open now, its list first (PATTERN: the notes come next)
    if (i == 3) browse_slot();           // grains need a sample: the browser first
    else toast(i == 1 ? "GRAND IS ON THE PAD: OPTION PLAYS IT" : "ACID IS ON THE PAD: OPTION PLAYS IT");
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
    const char *q = "Empty pad. What goes here?";
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
    if (!write_whole(p, &song, sizeof song)) { toast("COULD NOT SAVE"); return; }
    snprintf(prj_cur, sizeof prj_cur, "%s", name); prj_cfg_write(); prj_mark_saved(); save_state(); sync();
    char b[48]; snprintf(b, sizeof b, "SAVED %s", name); toast(b);
}
static void prj_load(const char *path) {
    rec = 0; combo_t = 0;
    if (engine_playing() || engine_counting() >= 0) transport_toggle();
    engine_stop_all(); cap_n = 0; spage = 0; undo_n = undo_top = 0;   // undo does not cross projects
    load_from(path); save_state();
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
    if (ui_v2) { prj_info_draw2(); return; }
    int x = 640, y = 118, w = 368; frame(x, y, w, 560, 1, C_LINE);
    plate(x, y, 120, 24, 5, C_DIM); text(x + 6, y + 4, 2, C_BG, "INFO");
    if (!pin.ok) { text(x + 16, y + 44, 2, C_RED, "NOT A PROJECT FILE"); return; }
    char b[64]; struct tm tmv; time_t tt = (time_t)pin.mt; localtime_r(&tt, &tmv); strftime(b, sizeof b, "%Y-%m-%d %H:%M", &tmv);
    text(x + 16, y + 40, 2, C_WHITE, b);
    snprintf(b, sizeof b, "%d BPM", pin.tempo); text(x + 16, y + 72, 3, C_WHITE, b);
    snprintf(b, sizeof b, "%s %s", note_name[pin.key % 12], scale_name[pin.scale % NSCALE]); text(x + 16, y + 110, 2, C_DIM, b);
    rect(x, y + 140, w, 1, C_LINE, 1);
    for (int s = 0; s < NSLOT; s++) { int yy = y + 152 + s * 33; snprintf(b, sizeof b, "%02d", s + 1);
        text(x + 16, yy, 2, pin.nm[s][0] ? C_DIM : C_LINE, b); text(x + 64, yy, 2, pin.type[s] ? C_ORANGE : pin.nm[s][0] ? C_WHITE : C_LINE, pin.nm[s][0] ? pin.nm[s] : "--"); }
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
static void draw_ask(void) {
    int x = 152, y = 250, w = 720, h = 250;
    rect(x - 4, y - 4, w + 8, h + 8, C_BG, 1); frame(x, y, w, h, 3, C_ORANGE);
    char b[64]; snprintf(b, sizeof b, "%s HAS UNSAVED CHANGES", prj_cur[0] ? prj_cur : "UNTITLED"); ctext(x + w / 2, y + 30, 2, C_WHITE, b);
    ctext(x + w / 2, y + 62, 1, C_GREY, ask_act == 2 ? "BEFORE A NEW PROJECT" : "BEFORE OPENING ANOTHER PROJECT");
    static const char *const bt[3] = {"SAVE", "DISCARD", "CANCEL"};
    for (int i = 0; i < 3; i++) { int bx = x + 30 + i * 226, on = i == ask_cur;
        if (on) rect(bx, y + 120, 206, 70, C_ORANGE, 1); else frame(bx, y + 120, 206, 70, 1, C_LINE);
        ctext(bx + 103, y + 143, 3, on ? C_BG : C_WHITE, bt[i]); }
    draw_footer("<> CHOOSE  EDIT OK  Y CANCEL");
}
// the name, typed as on the M8: LEFT/RIGHT the place, EDIT+UP/DOWN the character, OPTION deletes, EDIT saves
// the name editor, as on the M8: LEFT/RIGHT the place, EDIT+UP/DOWN the character (EDIT+<> by 10), OPTION deletes,
// EDIT confirms, Y cancels. Modes: a project's name (then save), a new folder, a new name for a file or folder.
#define NM_MAX 60                         // characters; the editor shows 30 places and scrolls
enum { NM_PROJECT, NM_FOLDER, NM_RENAME, NM_THEME, NM_PAD };
static int nm_pos, nm_then, nm_mode; static char nm_buf[NM_MAX + 2], nm_ext[16], nm_old[1024];
static const char nm_chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.()";
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
    if (k == K_X) { nk_low = !nk_low; return; }
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
static void draw_naming(void) {
    static const char *const ttl[5] = {"SAVE AS: NAME OF THE PROJECT", "NEW FOLDER: ITS NAME", "RENAME", "SAVE THE THEME AS (UP TO 19 CHARACTERS)", "THE PAD'S NAME"};
    text(16, 92, 2, C_WHITE, ttl[nm_mode % 5]); rect(16, 120, 992, 1, C_LINE, 1);
    int l = (int)strlen(nm_buf), cw = 32, x0 = 16 + (992 - 30 * cw) / 2, y = 136, off = nm_pos > 29 ? nm_pos - 29 : 0;
    if (off) text(x0 - 14, y + 22, 2, C_DIM, "<");
    for (int c0 = 0; c0 < 30; c0++) { int i = off + c0, x = x0 + c0 * cw, on = i == nm_pos; char c[2] = {i < l ? nm_buf[i] : ' ', 0};
        rect(x, y + 56, cw - 4, 4, i < l ? C_DIM : C_LINE, 1);
        if (on) rect(x - 3, y + 4, 3, 50, C_ORANGE, 1);     // the cursor stands before this character: what is typed goes in here
        ctext(x + (cw - 4) / 2, y + 18, 3, C_WHITE, c); }
    if (nm_ext[0]) text(x0 + 30 * cw - (int)strlen(nm_ext) * 16, y + 76, 2, C_DIM, nm_ext);
    char p[1024];
    if (nm_mode == NM_PAD) { snprintf(p, sizeof p, "PAD %d: UP TO 15 CHARACTERS, EMPTY = THE INSTRUMENT'S NAME", sel + 1); if (strlen(nm_buf) > 15) text(x0, y + 120, 2, C_ORANGE, "ONLY THE FIRST 15 ARE KEPT"); }
    else if (nm_mode == NM_PROJECT) { prj_path(nm_buf, p, sizeof p); if (!access(p, F_OK) && strcmp(nm_buf, prj_cur)) text(x0, y + 120, 2, C_ORANGE, "A PROJECT WITH THIS NAME EXISTS: SAVING REPLACES IT"); }
    else if (nm_mode == NM_THEME) { snprintf(p, sizeof p, THEME_DIR "/%.19s.thm", nm_buf); if (!access(p, F_OK)) text(x0, y + 120, 2, C_ORANGE, "A THEME WITH THIS NAME EXISTS: SAVING REPLACES IT"); }
    else { snprintf(p, sizeof p, "%s/%s%s", bdir, nm_buf, nm_ext); if (!access(p, F_OK) && strcmp(p, nm_old)) text(x0, y + 120, 2, C_ORANGE, "THAT NAME IS TAKEN HERE"); }
    text(x0, y + 100, 1, C_GREY, p);
    {   // the keyboard
        enum { KX = 82, KY = 296, KW = 86, KH = 68 };
        for (int r = 0; r < 4; r++) for (int c0 = 0; c0 < NK_COLS; c0++) {
            int x = KX + c0 * KW, yy = KY + r * KH, on = r == nk_r && c0 == nk_c; char ch = nk_keys[r][c0], t[3] = {ch, 0, 0};
            if (nk_low && ch >= 'A' && ch <= 'Z') t[0] = (char)(ch + 32);
            rect(x, yy, KW - 6, KH - 6, on ? C_ORANGE : C_CELL, 1);
            if (ch == ' ') ctext(x + (KW - 6) / 2, yy + 22, 2, on ? C_BG : C_DIM, "SPACE"); else ctext(x + (KW - 6) / 2, yy + 16, 4, on ? C_BG : C_WHITE, t);
        }
        static const char *const bn[NK_NB] = {"DELETE", "aA", "(", ")", "OK"};
        for (int b = 0; b < NK_NB; b++) { int x = KX + nk_bx[b] * KW, yy = KY + 4 * KH, w = nk_bw[b] * KW - 6, on = nk_r == NK_ROWS - 1 && nk_btn() == b;
            rect(x, yy, w, KH - 6, on ? C_ORANGE : b == NK_OK ? C_SELBG : C_CELL, 1);
            ctext(x + w / 2, yy + 18, 3, on ? C_BG : C_WHITE, b == NK_CASE ? (nk_low ? "abc" : "ABC") : bn[b]); }
    }
    draw_footer("ARROWS KEY  EDIT TYPE  OPT DEL  L1/R1 PLACE  PLAY OK  Y BACK");
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
    if (ui_v2) { draw_fmenu2(); return; }
    if (fmenu) {
        int x = 600, y = 120, w = 392, h = 6 * 56 + 52;
        rect(x - 4, y - 4, w + 8, h + 8, C_BG, 1); frame(x, y, w, h, 3, C_ORANGE);
        char t[64]; snprintf(t, sizeof t, "%.22s", bn && bcur < bn ? bent[bcur].name : bdir); text(x + 16, y + 14, 1, C_GREY, t);
        for (int i = 0; i < 6; i++) { int yy = y + 40 + i * 56, on = i == fm_cur, dim = i == 4 ? !fm_cut[0] : i && !bn;
            if (on) rect(x + 8, yy, w - 16, 48, C_ORANGE, 1);
            text(x + 24, yy + 12, 3, on ? C_BG : dim ? C_GREY : i == 5 ? C_RED : C_WHITE, fm_name[i]); }
        draw_footer("^v CHOOSE  EDIT DO IT  X/Y CLOSE");
    }
    if (fconfirm) {
        int x = 152, y = 250, w = 720, h = 260; const char *nm = strrchr(fc_path, '/'); nm = nm ? nm + 1 : fc_path;
        rect(x - 4, y - 4, w + 8, h + 8, C_BG, 1); frame(x, y, w, h, 3, C_RED);
        char b[96]; snprintf(b, sizeof b, "DELETE %.30s?", nm); ctext(x + w / 2, y + 26, 2, C_WHITE, b);
        struct stat st; int dir = !stat(fc_path, &st) && S_ISDIR(st.st_mode);
        if (dir) { snprintf(b, sizeof b, "A FOLDER WITH %d FILE%s IN IT", fc_files, fc_files == 1 ? "" : "S"); ctext(x + w / 2, y + 60, 1, C_ORANGE, b); }
        if (fc_used) { snprintf(b, sizeof b, "%d PAD%s USE IT: THEY WILL MISS IT", fc_used, fc_used == 1 ? "" : "S"); ctext(x + w / 2, y + 78, 1, C_RED, b); }
        ctext(x + w / 2, y + 96, 1, C_GREY, "IT CANNOT BE UNDONE");
        static const char *const bt[2] = {"DELETE", "CANCEL"};
        for (int i = 0; i < 2; i++) { int bx = x + 60 + i * 320, on = i == fm_cur;
            if (on) rect(bx, y + 140, 280, 70, i ? C_ORANGE : C_RED, 1); else frame(bx, y + 140, 280, 70, 1, C_LINE);
            ctext(bx + 140, y + 163, 3, on ? C_BG : C_WHITE, bt[i]); }
        draw_footer("<> CHOOSE  EDIT OK  Y CANCEL");
    }
}
// ---------- EXPORT WAV (2026-11-09) ----------
// The window: BARS, CLIPS (the ones on, or all with notes), TAIL, EXPORT. Rendering pauses the audio device and runs
// callback() from the UI thread, a slice per frame, into exports/NAME.wav (48 kHz, 16 bit, stereo).
static const int ex_bars_v[7] = {1, 2, 4, 8, 16, 32, 64}, ex_tail_v[3] = {0, 2, 4};
enum { EX_BARS, EX_FROM, EX_CLIPS, EX_TAIL, EX_STEMS, EX_GO, EX_ROWS };
static int ex_row, ex_bars = 3, ex_from = 1, ex_all, ex_tail = 1, ex_stems, ex_run, ex_pad; static long ex_done, ex_total, ex_body, ex_skip; static FILE *ex_f;
static char ex_path[160]; static int8_t ex_on[NSLOT], ex_solo[NSLOT]; static float ex_peak;
static void export_open(void) {
    exporting = 1; ex_row = EX_GO; ex_run = 0;
    int any = 0; for (int s = 0; s < NSLOT; s++) if (song.t[s].on) any = 1;
    ex_all = !any;                        // nothing on: every clip with notes
}
static void wav_header(FILE *f, long frames) {
    uint32_t data = (uint32_t)(frames * 4), riff = 36 + data, sr = SR, br = SR * 4, n16 = 16; uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&n16, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
}
static int ex_used(int s) { return ex_all ? has_sound(s) && (pat_used(cur_pat(s)) || roll_any(cur_pat(s))) : ex_on[s]; }
static int ex_pass(void) {               // one file: the mix, or (STEMS) pad ex_pad alone; 0 when there is no more to do
    if (ex_stems) { while (ex_pad < NSLOT && !ex_used(ex_pad)) ex_pad++; if (ex_pad >= NSLOT) return 0; }
    const char *nm = prj_cur[0] ? prj_cur : "UNTITLED"; char base[96];
    if (ex_stems) snprintf(base, sizeof base, "%s PAD%02d", nm, ex_pad + 1); else snprintf(base, sizeof base, "%s", nm);
    snprintf(ex_path, sizeof ex_path, "exports/%s.wav", base);
    for (int i = 2; i < 100 && !access(ex_path, F_OK); i++) snprintf(ex_path, sizeof ex_path, "exports/%s %d.wav", base, i);
    ex_f = fopen(ex_path, "wb"); if (!ex_f) { toast("COULD NOT WRITE exports/"); return 0; }
    wav_header(ex_f, 0);
    for (int s = 0; s < NSLOT; s++) { song.t[s].on = (int8_t)ex_used(s); song.t[s].queued = -1; song.mix.pad[s].solo = (uint8_t)(ex_stems && s == ex_pad); }
    engine_stop_all(); { float z[256]; for (int i = 0; i < 8; i++) engine_render(z, 128); }
    engine_transport(1);
    double bar_s = (song.beats > 0 ? song.beats : 4) * 60.0 / song.tempo;
    ex_skip = (long)(bar_s * (ex_from - 1) * SR + 0.5);   // FROM BAR: the bars before it are played, not written
    ex_body = (long)(bar_s * ex_bars_v[ex_bars] * SR + 0.5); ex_total = ex_body + (long)ex_tail_v[ex_tail] * SR; ex_done = 0; ex_peak = 0;
    return 1;
}
static void export_start(void) {
    mkdir("exports", 0777);
    rec = 0; combo_t = 0; rec_wait = 0;
    if (engine_playing() || engine_counting() >= 0) transport_toggle();
    for (int s = 0; s < NSLOT; s++) { ex_on[s] = song.t[s].on; ex_solo[s] = (int8_t)song.mix.pad[s].solo; }
    engine_pause(1); ex_pad = 0;
    if (ex_pass()) ex_run = 1; else { engine_pause(0); toast(ex_stems ? "NO PAD TO EXPORT" : "COULD NOT EXPORT"); }
}
static void export_finish(int keep) {
    engine_transport(0); { float z[256]; engine_render(z, 128); } engine_stop_all();
    if (ex_f) { if (keep) { fseek(ex_f, 0, SEEK_SET); wav_header(ex_f, ex_done); } fclose(ex_f); ex_f = NULL; if (!keep) unlink(ex_path); sync(); }
    if (keep && ex_stems) { ex_pad++; if (ex_pass()) return; }   // STEMS: the next pad
    for (int s = 0; s < NSLOT; s++) { song.t[s].on = ex_on[s]; song.mix.pad[s].solo = (uint8_t)ex_solo[s]; }
    engine_pause(0); ex_run = 0;
    if (keep) { char b[200]; snprintf(b, sizeof b, ex_stems ? "STEMS IN exports/" : "EXPORTED %s", ex_path); toast(b); exporting = 0; } else toast("EXPORT CANCELLED");
}
static void export_tick(void) {          // a slice of the render per frame: about a second of audio
    if (!ex_run) return;
    static float buf[2 * 128]; static int16_t pcm[2 * 128];
    for (int n = 0; n < 375; n++) {
        if (ex_skip > 0) { engine_render(buf, 128); ex_skip -= 128; continue; }   // before FROM BAR
        if (ex_done >= ex_total) break;
        if (ex_done < ex_body && ex_done + 128 >= ex_body) engine_transport(0);   // the bars are done: the tail rings out
        engine_render(buf, 128);
        int fr = ex_total - ex_done < 128 ? (int)(ex_total - ex_done) : 128;
        for (int i = 0; i < 2 * fr; i++) { float v = buf[i]; if (fabsf(v) > ex_peak) ex_peak = fabsf(v); v = v > 1 ? 1 : v < -1 ? -1 : v; pcm[i] = (int16_t)lrintf(v * 32767); }
        if (fwrite(pcm, 4, fr, ex_f) != (size_t)fr) { toast("DISK FULL?"); export_finish(0); return; }
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
        if (ex_row == EX_BARS) ex_bars = clampi(ex_bars + d, 0, 6); else if (ex_row == EX_FROM) ex_from = clampi(ex_from + d, 1, 128);
        else if (ex_row == EX_CLIPS) ex_all = !ex_all; else if (ex_row == EX_TAIL) ex_tail = clampi(ex_tail + d, 0, 2); else if (ex_row == EX_STEMS) ex_stems = !ex_stems; }
}
static void draw_export(void) {
    text(16, 92, 2, C_WHITE, "EXPORT WAV"); rect(16, 120, 992, 1, C_LINE, 1);
    char b[96]; double bar_s = (song.beats > 0 ? song.beats : 4) * 60.0 / song.tempo, sec = bar_s * ex_bars_v[ex_bars] + ex_tail_v[ex_tail];
    static const char *const rn[5] = {"BARS", "FROM BAR", "CLIPS", "TAIL", "FILES"};
    for (int r = 0; r < 5; r++) { int y = 130 + r * 70, on = ex_row == r && !ex_run;
        if (on) rect(16, y, 600, 64, C_ORANGE, 1); else frame(16, y, 600, 64, 1, C_LINE);
        text(30, y + 6, 2, on ? C_BG : C_DIM, rn[r]);
        if (r == EX_BARS) snprintf(b, sizeof b, "%d", ex_bars_v[ex_bars]); else if (r == EX_FROM) snprintf(b, sizeof b, "%d", ex_from);
        else if (r == EX_CLIPS) snprintf(b, sizeof b, ex_all ? "ALL WITH NOTES" : "THE ONES PLAYING"); else if (r == EX_TAIL) snprintf(b, sizeof b, ex_tail_v[ex_tail] ? "%d S" : "NONE", ex_tail_v[ex_tail]);
        else snprintf(b, sizeof b, ex_stems ? "ONE PER PAD (STEMS)" : "ONE MIX");
        text(30, y + 28, 3, on ? C_BG : C_WHITE, b); }
    { int y = 130 + 5 * 70, on = ex_row == EX_GO && !ex_run; rect(16, y, 600, 80, on ? C_ORANGE : C_CELL, 1); ctext(316, y + 24, 4, on ? C_BG : C_WHITE, ex_run ? "RENDERING" : "EXPORT"); }
    const char *nm = prj_cur[0] ? prj_cur : "UNTITLED";
    snprintf(b, sizeof b, ex_stems ? "exports/%s PADnn.wav" : "exports/%s.wav", nm); text(640, 150, 1, C_GREY, ex_run ? ex_path : b);
    snprintf(b, sizeof b, "%.1f S  %d BPM  48 KHZ 16 BIT", sec, song.tempo); text(640, 170, 1, C_GREY, b);
    if (ex_run) {
        int pw = 992, pc = ex_total ? (int)(ex_done * 100 / ex_total) : 0; rect(16, 600, pw, 24, C_CELL, 1); rect(16, 600, pw * pc / 100, 24, C_ORANGE, 1);
        if (ex_stems) snprintf(b, sizeof b, "PAD %d  %d%%  PEAK %.2f", ex_pad + 1, pc, ex_peak); else snprintf(b, sizeof b, "%d%%  PEAK %.2f", pc, ex_peak);
        if (!ex_stems) snprintf(b, sizeof b, "%d%%  PEAK %.2f", pc, ex_peak);
        text(16, 632, 2, ex_peak > 1 ? C_RED : C_DIM, b);
        draw_footer("Y CANCEL");
    } else draw_footer("^v ROW  <> CHANGE  EDIT ON EXPORT: GO  Y BACK");
}
#include "windows.h"
static void draw(void) {
    Uint64 t_draw0 = SDL_GetPerformanceCounter();
    { static int last; int scr = theming ? 2 : !iconing && !browsing && !slicing && !perform && tab == T_PRJ ? 1 : 0;   // every opening gets another figure
      if (scr && scr != last) fig_open = (fig_open + 1 + rand() % (FIG_N - 1)) % FIG_N;
      last = scr; }
    SDL_SetRenderDrawColor(R, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(R);
    ui_v2 = 1; uf_name[0] = 0;           // every screen is a rebuilt one; the pages tell ui_popup their card
    if (ui_simple && !exporting && !naming && !asking) { draw_simple(); goto shown; }
    v2_normalize(); ui2_header();
    if (exporting) draw_export2();
    else if (naming) draw_naming2();
    else if (browsing) { if (bmode) draw_prjbrowse2(); else draw_browse2(); }
    else if (slicing) draw_slice();
    else if (rolling) draw_roll();
    else if (randing) draw_rnd2();
    else if (picking) draw_pick();
    else if (presetting) draw_presets2();
    else if (theming) draw_theme2();
    else if (iconing) draw_icons2();
    else if (ducking) { if (ui_v2) draw_duck2(); else draw_duck(); }
    else if (hubbing) draw_hub();
    else if (patmenu) draw_patmenu();
    else if (modmenu) draw_modmenu();
    else if (scening) draw_scenes();
    else if (recpage) draw_record();
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
    default: draw_hub(); break;       // not reached: v2_normalize gives every state a screen
    }
    ui_popup(); ui_cheat();
    if (asking) draw_ask2();
shown:;
    ui_quit_hold();
    Uint64 tp = SDL_GetPerformanceCounter(); draw_build_ms += (tp - t_draw0) * 1000.0 / SDL_GetPerformanceFrequency();
    SDL_RenderPresent(R);
    draw_present_ms += (SDL_GetPerformanceCounter() - tp) * 1000.0 / SDL_GetPerformanceFrequency();
}

static void screenshot(const char *path) {
    SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(R, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
    SDL_SaveBMP(sf, path); SDL_FreeSurface(sf);
}

int main(int argc, char **argv) {
    srand((unsigned)time(NULL));         // figures, random files and kits differ from run to run
    logf = fopen("brickpad.log", "w"); if (!logf) logf = stderr;
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
    if (engine_start(buffer)) { logmsg("audio: %s", SDL_GetError()); return 1; }
    logmsg("audio: %s", SDL_GetCurrentAudioDriver());
    idx_load();
    load_state(); prj_startup();
    theme_load(); presets_load(); hint_load(); ui_cfg_load();
    if (test_secs) {  // demo beat for automated checks
        song.tempo = 128; song.swing = 20;
        for (int i = 0; i < 16; i++) {
            song.t[0].pat[0].trig[i] = i % 4 == 0; song.t[5].pat[0].trig[i] = i % 2 == 0;
            song.t[2].pat[0].trig[i] = i % 8 == 4; song.t[9].pat[0].trig[i] = i % 3 == 0;
            song.t[9].pat[0].note[i] = (int8_t)deg_midi(9, (i * 3) % 8);
        }
        song.t[5].pat[0].rep[6] = 3; song.t[9].pat[0].len = 12;
        if (loop_arg) { set_slot_file(11, loop_arg); auto_setup(11); song.t[11].warp = 3; song.tempo = 100; }
        transport_toggle(); sel = loop_arg ? 11 : 9; lane = loop_arg ? L_SLICE : L_NOTE; tab = T_SEQ; srow = 4;
    }
    Uint32 start = SDL_GetTicks(), frames = 0, last_save = start; int hat = 0, run = 1;
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: run = 0; break;
            case SDL_RENDER_TARGETS_RESET: case SDL_RENDER_DEVICE_RESET: gb_valid = 0; break;   // the cached panel is gone
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
            if (!perform && input_repeat(d, now)) key_event(K_UP + d, 1);
        if (menu_down && now - menu_down > 3000) run = 0;
        combo_tick(now);
        browse_tick(now);
        launch_tick(); rec_tick(); export_tick(); frz_tick();
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
              logmsg("frames: %.1f fps, build %.2f ms, present %.2f ms, tab %d%s | audio: load avg %d%% max %d%%, %d of %d calls over time, longest wait %.1f ms, voices %d", nf * 1000.0 / ms_since(now, st0), draw_build_ms / nf, draw_present_ms / nf, tab, is_synth(sel) && tab == T_SEQ ? " synth" : "", (int)(engine_load() * 100), (int)(amx * 100), aov, an, agap, engine_voices()); }
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
          } }
        frames++;
    }
    if (test_secs) {
        float secs = (SDL_GetTicks() - start) / 1000.f;
        printf("fps %.1f, dsp load %.1f%%, buffer %d\n", frames / secs, engine_load() * 100, engine_frames());
        screenshot("/tmp/bp2.bmp");
    } else { save_state(); theme_save(); saver_pause(0); }
    lib_end();
    if (idx_dirty) idx_save();
    engine_stop();
    SDL_Quit();
    return 0;
}
