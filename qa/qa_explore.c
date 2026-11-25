// 2026-11-25 (cloud QA, "emulate a real user"): a walk over every screen and every element it can focus, with single
// presses and short combos. The SDL draw calls are hooked: the boxes, outlines and text of each frame are kept (the page
// drawn into a texture under the big strip is mapped back to the screen), and after every arrow it is checked that
// (1) the cursor moves when there is an element that way, and does not jump far; (2) a focus outline covers no text and
// no other element, text stays on the screen, inside its card and off other text; (3) nothing crashes or leads nowhere.
// Build: cc -O0 -g -std=gnu11 -w -Isrc qa/qa_explore.c src/sample.c -lSDL2 -lm -lpthread; run in a folder with NEON.
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
static unsigned fake_ms = 1000;
#define SDL_GetTicks fake_ticks
#define SDL_GetPerformanceCounter fake_pc
#include <SDL2/SDL.h>
Uint32 fake_ticks(void) { return fake_ms; }
static Uint64 pc_n; Uint64 fake_pc(void) { return ++pc_n; }
// ---- the hooks: every draw call of the app goes through these (defined after main.c, they see its textures) ----
static int hk_line(SDL_Renderer *r, int x1, int y1, int x2, int y2);
static int hk_fill(SDL_Renderer *r, const SDL_Rect *a);
static int hk_fills(SDL_Renderer *r, const SDL_Rect *a, int n);
static int hk_drect(SDL_Renderer *r, const SDL_Rect *a);
static int hk_copy(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *s, const SDL_Rect *d);
static int hk_target(SDL_Renderer *r, SDL_Texture *t);
static int hk_clear(SDL_Renderer *r);
static int hk_clip(SDL_Renderer *r, const SDL_Rect *c);
static int hk_color(SDL_Renderer *r, Uint8 a, Uint8 b, Uint8 c, Uint8 d);
#define SDL_RenderDrawLine hk_line
#define SDL_RenderFillRect hk_fill
#define SDL_RenderFillRects hk_fills
#define SDL_RenderDrawRect hk_drect
#define SDL_RenderCopy hk_copy
#define SDL_SetRenderTarget hk_target
#define SDL_RenderClear hk_clear
#define SDL_RenderSetClipRect hk_clip
#define SDL_SetRenderDrawColor hk_color
#include "engine.c"
#define logf bp_logf
#define main bp_main
#include "main.c"
#undef main
#undef SDL_RenderDrawLine
#undef SDL_RenderFillRect
#undef SDL_RenderFillRects
#undef SDL_RenderDrawRect
#undef SDL_RenderCopy
#undef SDL_SetRenderTarget
#undef SDL_RenderClear
#undef SDL_RenderSetClipRect
#undef SDL_SetRenderDrawColor

enum { O_BOX, O_FILL, O_FRAME, O_TEXT, O_ICON };
typedef struct { int k, x, y, w, h, t, tgt, mapped, ly, font, hid; Uint32 col, in; char s[100]; } Op;
#define NOPS 20000
static Op ops[NOPS]; static int nops, cur_tgt; static Uint32 dcol; static SDL_Rect clipr; static int clip_on;
static Uint32 rgb(Col c) { return (Uint32)c.r << 16 | c.g << 8 | c.b; }
static int clip(int *x, int *y, int *w, int *h) {
    if (!clip_on) return 1;
    int x0 = *x > clipr.x ? *x : clipr.x, y0 = *y > clipr.y ? *y : clipr.y;
    int x1 = *x + *w < clipr.x + clipr.w ? *x + *w : clipr.x + clipr.w, y1 = *y + *h < clipr.y + clipr.h ? *y + *h : clipr.y + clipr.h;
    if (x1 <= x0 || y1 <= y0) return 0; *x = x0; *y = y0; *w = x1 - x0; *h = y1 - y0; return 1;
}
static Op *op_new(int k, int x, int y, int w, int h, Uint32 c) {
    if (nops >= NOPS) return NULL; if (!clip(&x, &y, &w, &h)) return NULL;
    Op *o = &ops[nops++]; memset(o, 0, sizeof *o); o->k = k; o->x = x; o->y = y; o->w = w; o->h = h; o->col = c; o->tgt = cur_tgt; o->ly = y; return o;
}
static void frame_check(void) {          // the box before the last one, with the last one inset all round: an outline
    if (nops < 2) return; Op *a = &ops[nops - 2], *b = &ops[nops - 1];
    if (a->k != O_BOX || b->k != O_BOX || a->tgt != b->tgt) return;
    int t = b->x - a->x; if (t < 1 || t > 8) return;
    if (abs((b->y - a->y) - t) > 1 || abs((a->x + a->w) - (b->x + b->w) - t) > 1 || abs((a->y + a->h) - (b->y + b->h) - t) > 1) return;
    a->k = O_FRAME; a->t = t; a->in = b->col; nops--;
}
static int hk_color(SDL_Renderer *r, Uint8 a, Uint8 b, Uint8 c, Uint8 d) { dcol = (Uint32)a << 16 | b << 8 | c; return SDL_SetRenderDrawColor(r, a, b, c, d); }
static int hk_line(SDL_Renderer *r, int x1, int y1, int x2, int y2) {
    if (y1 == y2) {                      // rrect: a row of lines down the box
        int xa = x1 < x2 ? x1 : x2, xb = x1 < x2 ? x2 : x1; Op *l = nops ? &ops[nops - 1] : NULL;
        if (l && l->k == O_BOX && l->col == dcol && l->tgt == cur_tgt && y1 == l->ly + 1 && xa <= l->x + l->w && xb >= l->x) {
            int x0 = xa < l->x ? xa : l->x, xe = xb + 1 > l->x + l->w ? xb + 1 : l->x + l->w; l->x = x0; l->w = xe - x0; l->h = y1 + 1 - l->y; l->ly = y1;
        } else { frame_check(); op_new(O_BOX, xa, y1, xb - xa + 1, 1, dcol); }
    }
    return SDL_RenderDrawLine(r, x1, y1, x2, y2);
}
static int hk_fill(SDL_Renderer *r, const SDL_Rect *a) {
    frame_check(); if (a) op_new(O_FILL, a->x, a->y, a->w, a->h, dcol); else op_new(O_FILL, 0, 0, W, H, dcol);
    return SDL_RenderFillRect(r, a);
}
static int hk_fills(SDL_Renderer *r, const SDL_Rect *a, int n) {
    frame_check(); if (n > 0) { int x0 = 9999, y0 = 9999, x1 = -9999, y1 = -9999;
        for (int i = 0; i < n; i++) { if (a[i].x < x0) x0 = a[i].x; if (a[i].y < y0) y0 = a[i].y; if (a[i].x + a[i].w > x1) x1 = a[i].x + a[i].w; if (a[i].y + a[i].h > y1) y1 = a[i].y + a[i].h; }
        op_new(O_ICON, x0, y0, x1 - x0, y1 - y0, dcol); }
    return SDL_RenderFillRects(r, a, n);
}
static int hk_drect(SDL_Renderer *r, const SDL_Rect *a) { frame_check(); return SDL_RenderDrawRect(r, a); }
static int is_font(SDL_Texture *t, int *f) {
    for (int i = 0; i < FT_N; i++) if (t == font_v2[i]) { *f = i; return 1; }
    for (int i = 1; i < 4; i++) if (t == font_sc[i]) { *f = 100 + i; return 1; }
    if (t == font_dig) { *f = 200; return 1; }
    return 0;
}
static void ink(int f, int *top, int *h) {   // rows of A-Z 0-9 with any ink, once per font
    static int done[FT_N], t0[FT_N], t1[FT_N]; const FontV2 *v = &fv2[f];
    if (!done[f]) { done[f] = 1; t0[f] = v->h; t1[f] = 0;
        for (int ch = '0'; ch <= 'Z'; ch++) { if (ch > '9' && ch < 'A') continue; int g = ch - 32;
            for (int i = 0; i < v->w * v->h; i++) if (v->px[g * v->per + i / 2] >> ((i & 1) ? 0 : 4) & 15) { int r = i / v->w; if (r < t0[f]) t0[f] = r; if (r > t1[f]) t1[f] = r; } }
        if (t1[f] < t0[f]) { t0[f] = 0; t1[f] = v->h - 1; } }
    *top = t0[f]; *h = t1[f] - t0[f] + 1;
}
static SDL_Texture *body_tex;
static int hk_copy(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *s, const SDL_Rect *d) {
    int f; frame_check();
    if (is_font(t, &f) && s && d) {
        Uint8 cr, cg, cb; SDL_GetTextureColorMod(t, &cr, &cg, &cb); Uint32 c = (Uint32)cr << 16 | cg << 8 | cb;
        int gw = f < 100 ? fv2[f].w : f < 200 ? 8 * (f - 100) : FD_W, gi = s->x / (gw ? gw : 1);
        char ch = f < 100 ? (gi < 95 ? (char)(gi + 32) : '#') : f < 200 ? (char)(gi + 32) : (char)('0' + gi);
        Op *l = nops ? &ops[nops - 1] : NULL; int gap = l ? d->x - (l->x + l->w) : 0;
        if (l && l->k == O_TEXT && l->font == f && l->col == c && l->tgt == cur_tgt && l->ly == d->y && gap >= 0 && gap <= 2 * gw) {
            size_t n = strlen(l->s); for (int g = 0; g < gap / gw && n < sizeof l->s - 2; g++) l->s[n++] = ' ';
            if (n < sizeof l->s - 1) { l->s[n++] = ch; l->s[n] = 0; }
            int x = d->x, y = d->y, w = d->w, h = d->h; if (clip(&x, &y, &w, &h)) l->w = x + w - l->x;
        } else { int it = 0, ih = d->h; if (f < FT_N) ink(f, &it, &ih);   // the letters' ink, not the cell's padding
            Op *o = op_new(O_TEXT, d->x, d->y + it, d->w, ih, c); if (o) { o->font = f; o->s[0] = ch; o->ly = d->y; } }
    } else if (t == body_tex && s && d) {   // the page set under the strip: its ops to where they land
        for (int i = 0; i < nops; i++) { Op *o = &ops[i]; if (o->tgt != 1 || o->mapped) continue;
            int cx = o->x + o->w / 2, cy = o->y + o->h / 2; if (cx < s->x || cx >= s->x + s->w || cy < s->y || cy >= s->y + s->h) continue;
            int x0 = d->x + (o->x - s->x) * d->w / s->w, y0 = d->y + (o->y - s->y) * d->h / s->h;
            int x1 = d->x + (o->x + o->w - s->x) * d->w / s->w, y1 = d->y + (o->y + o->h - s->y) * d->h / s->h;
            o->x = x0; o->y = y0; o->w = x1 - x0; o->h = y1 - y0; o->tgt = 0; o->mapped = 1; }
    }
    return SDL_RenderCopy(r, t, s, d);
}
static int hk_target(SDL_Renderer *r, SDL_Texture *t) { frame_check(); cur_tgt = t != NULL; if (t) body_tex = t; return SDL_SetRenderTarget(r, t); }
static int hk_clear(SDL_Renderer *r) { frame_check(); if (!cur_tgt) nops = 0; else { int j = 0; for (int i = 0; i < nops; i++) if (ops[i].tgt != 1) ops[j++] = ops[i]; nops = j; } return SDL_RenderClear(r); }
static int hk_clip(SDL_Renderer *r, const SDL_Rect *c) { frame_check(); clip_on = c != NULL; if (c) clipr = *c; return SDL_RenderSetClipRect(r, c); }

// ---- the harness ----
static SDL_Surface *sf; static float buf[2 * 128]; static int fails, warns; static char where[96];
static void pump(int n) { while (n--) { callback(0, (Uint8 *)buf, sizeof buf); engine_collect_garbage(); scene_poll(); } }
static unsigned seen_px; static unsigned px_hash(void);   // the page's pixels at the last redraw
static void occlude(void) {              // what a later opaque box covers whole is not seen (a window over the page)
    for (int i = 0; i < nops; i++) { Op *a = &ops[i]; a->hid = a->tgt != 0; }
    for (int j = 0; j < nops; j++) { Op *b = &ops[j]; if (b->tgt || (b->k != O_BOX && b->k != O_FILL) || b->w < 40 || b->h < 40) continue;
        for (int i = 0; i < j; i++) { Op *a = &ops[i]; if (a->hid) continue;
            if (a->x >= b->x && a->y >= b->y && a->x + a->w <= b->x + b->w && a->y + a->h <= b->y + b->h) a->hid = 1; } }
    // a frame counts as its ring: cover the ring and it is gone
    for (int j = 0; j < nops; j++) { Op *b = &ops[j]; if (b->tgt || b->k != O_FRAME || b->w < 40 || b->h < 40) continue;
        for (int i = 0; i < j; i++) { Op *a = &ops[i]; if (a->hid) continue;
            int t = b->t; if (a->x >= b->x + t && a->y >= b->y + t && a->x + a->w <= b->x + b->w - t && a->y + a->h <= b->y + b->h - t) a->hid = 1; } }
}
static void redraw(void) {               // frames until the drawers have slid
    for (int i = 0; i < 20; i++) { fake_ms += 40; anim_busy = 0; draw(); frame_check(); if (!anim_busy && i >= 1) break; }
    occlude(); seen_px = px_hash();
}
static void dump(const char *nm) {
    printf("---- %s: %d ops\n", nm, nops);
    for (int i = 0; i < nops; i++) { Op *o = &ops[i]; if (o->tgt) continue;
        printf("%4d %s %4d %4d %4d %4d %06X", i, (const char *[]){"BOX ", "FILL", "FRM ", "TEXT", "ICON"}[o->k], o->x, o->y, o->w, o->h, o->col);
        if (o->k == O_FRAME) printf(" t%d in %06X", o->t, o->in); if (o->k == O_TEXT) printf(" f%d \"%s\"", o->font, o->s); printf("\n"); }
}

// ---- what each frame is checked for ----
static int isect(const Op *a, const Op *b, int m) {   // overlap of more than m pixels both ways
    int x0 = a->x > b->x ? a->x : b->x, x1 = a->x + a->w < b->x + b->w ? a->x + a->w : b->x + b->w;
    int y0 = a->y > b->y ? a->y : b->y, y1 = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;
    return x1 - x0 > m && y1 - y0 > m;
}
static int inside(const Op *a, const Op *b, int m) { return a->x >= b->x - m && a->y >= b->y - m && a->x + a->w <= b->x + b->w + m && a->y + a->h <= b->y + b->h + m; }
static int inside_t(const Op *a, const Op *b) {   // text in a card: a pixel across, its cell's padding up and down
    return a->x >= b->x - 1 && a->x + a->w <= b->x + b->w + 1 && a->y >= b->y - 1 && a->y + a->h <= b->y + b->h + 1;
}
static int is_focus(const Op *o) { return o->k == O_FRAME && o->t >= 2 && (o->col == rgb(C_WHITE) || o->col == rgb(C_ORANGE) || o->in == rgb(C_SELBG)); }
static int blank_text(const Op *o) { for (const char *c = o->s; *c; c++) if (*c != ' ') return 0; return 1; }
#define NSEEN 4000
static char seen[NSEEN][200]; static int nseen;
static void report(int fail, const char *fmt, ...) {   // each finding once (per screen and text)
    char b[200]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
    for (int i = 0; i < nseen; i++) if (!strcmp(seen[i], b)) return;
    if (nseen < NSEEN) snprintf(seen[nseen++], sizeof seen[0], "%s", b);
    static int nshot; char f[64] = ""; if (nshot < 99 && getenv("SHOTS")) { snprintf(f, sizeof f, "fail_%02d.bmp", ++nshot); SDL_SaveBMP(sf, f); }
    printf("%s %s%s%s\n", fail ? "FAIL" : "note", b, *f ? "  -> " : "", f); if (fail) fails++; else warns++;
}
static int noisy_text(const Op *o) { return o->font >= 200; }   // the big digits are drawn as art
static void check_frame(const char *key) {
    for (int i = 0; i < nops; i++) { Op *a = &ops[i]; if (a->hid || a->k != O_TEXT || blank_text(a)) continue;
        if (a->x < 0 || a->y < 0 || a->x + a->w > W || a->y + a->h > H) report(1, "[%s] text off the screen: \"%s\" at %d,%d w %d", where, a->s, a->x, a->y, a->w);
        for (int j = i + 1; j < nops; j++) { Op *b = &ops[j]; if (b->hid || b->k != O_TEXT || blank_text(b)) continue;
            if (isect(a, b, 3)) report(1, "[%s] text on text: \"%s\" (%d,%d) and \"%s\" (%d,%d)", where, a->s, a->x, a->y, b->s, b->x, b->y); }
        // its card: the last box drawn before it under its first letter
        int gx = a->x + 4, gy = a->y + a->h / 2, mx = a->x + a->w / 2, pass;
        for (pass = 0; pass < 2; pass++) for (int j = i - 1; j >= 0; j--) { Op *b = &ops[j]; if (b->hid || (b->k != O_BOX && b->k != O_FILL && b->k != O_FRAME)) continue;
            if (b->w < 16 || b->h < 12 || (b->w >= W - 4 && b->h >= H - 4)) continue;
            if (gx < b->x || gx >= b->x + b->w || gy < b->y || gy >= b->y + b->h) continue;
            if (!pass && (mx < b->x || mx >= b->x + b->w)) continue;   // first a card under the whole line
            if (b->col == rgb(C_BG) && b->w >= W - 4) break;   // the page itself
            if (!inside_t(a, b) && !noisy_text(a)) report(1, "[%s] text out of its card: \"%s\" at %d,%d w %d h %d, card %d,%d %dx%d", where, a->s, a->x, a->y, a->w, a->h, b->x, b->y, b->w, b->h);
            pass = 2; break; }
    }
    for (int i = 0; i < nops; i++) { Op *f = &ops[i]; if (f->hid || !is_focus(f)) continue;
        Op in = *f; in.x += f->t; in.y += f->t; in.w -= 2 * f->t; in.h -= 2 * f->t;
        for (int j = 0; j < nops; j++) { Op *b = &ops[j]; if (j == i || b->hid) continue;
            if (b->k == O_TEXT && blank_text(b)) continue;
            if (!isect(b, f, b->k == O_TEXT ? 1 : 3) || inside(b, &in, 0)) continue;   // inside the outline: what it frames
            if (b->k != O_TEXT && inside(f, b, 0)) continue;     // the card or panel it sits on
            if (b->k == O_FRAME && inside(b, f, 0)) continue;
            if (b->w <= 3 || b->h <= 3) continue;
            if (b->k == O_TEXT) report(1, "[%s] focus outline %d,%d %dx%d over text \"%s\" at %d,%d", where, f->x, f->y, f->w, f->h, b->s, b->x, b->y);
            else if (j > i || b->k == O_ICON || isect(b, &in, -1) == 0) report(1, "[%s] focus outline %d,%d %dx%d crosses a %s %d,%d %dx%d (%06X)%s", where, f->x, f->y, f->w, f->h,
                (const char *[]){"box", "fill", "frame", "text", "icon"}[b->k], b->x, b->y, b->w, b->h, b->col, j > i ? " drawn over it" : ""); }
    }
    if (qb_focus || qd_open) for (int i = 0; i < nops; i++) { Op *f = &ops[i];   // the bar has the cursor: no white outline left on the page
        if (f->hid || f->k != O_FRAME || f->t < 2 || f->col != rgb(C_WHITE) || f->y < 70 || f->h < 30) continue;
        if (qd_open && f->y < 660) continue;   // the drawer's own tiles
        report(1, "[%s] two cursors: the bar has it, and a white outline %d,%d %dx%d stays on the page", where, f->x, f->y, f->w, f->h); }
    (void)key;
}

// ---- the cursor: the outlines in the text colour or the accent; did an arrow move it, and how far ----
typedef struct { int n; Op f[8]; unsigned th; } Look;
static unsigned text_hash(void) { unsigned h = 2166136261u; for (int i = 0; i < nops; i++) if (!ops[i].hid && ops[i].k == O_TEXT && ops[i].y > V2_HEAD) {
    for (const char *c = ops[i].s; *c; c++) h = (h ^ (unsigned char)*c) * 16777619u; h = (h ^ (unsigned)ops[i].x) * 16777619u; h = (h ^ (unsigned)ops[i].y) * 16777619u; } return h; }
static unsigned px_hash(void) {          // what is seen of the page (the header's clock and meters left out)
    unsigned h = 2166136261u; const Uint32 *px = sf->pixels;
    for (int y = V2_HEAD; y < V2_FOOT; y++) for (int x = 0; x < W; x++) h = (h ^ px[y * (sf->pitch / 4) + x]) * 16777619u;
    return h;
}
static void look(Look *l) { l->n = 0; for (int i = 0; i < nops && l->n < 8; i++) if (!ops[i].hid && is_focus(&ops[i])) l->f[l->n++] = ops[i]; l->th = seen_px; }
static int same_look(const Look *a, const Look *b) {
    if (a->n != b->n) return 0;
    for (int i = 0; i < a->n; i++) if (a->f[i].x != b->f[i].x || a->f[i].y != b->f[i].y || a->f[i].w != b->f[i].w || a->f[i].h != b->f[i].h) return 0;
    return 1;
}
static const char *kname(int k) { static char b[8]; switch (k) { case K_UP: return "UP"; case K_DOWN: return "DOWN"; case K_LEFT: return "LEFT"; case K_RIGHT: return "RIGHT";
    case K_EDIT: return "EDIT"; case K_OPT: return "OPT"; case K_Y: return "Y"; case K_X: return "X"; case K_L1: return "L1"; case K_R1: return "R1"; case K_MENU: return "MENU"; case K_SHIFT: return "SHIFT"; } snprintf(b, sizeof b, "k%d", k); return b; }
static int is_arrow(int k) { return k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT; }
static int candidate(const Op *f, int k, Op *out) {   // an element like the focused one that way
    int cx = f->x + f->w / 2, cy = f->y + f->h / 2, best = 1 << 30;
    for (int i = 0; i < nops; i++) { const Op *b = &ops[i]; if (b->hid || (b->k != O_BOX && b->k != O_FILL && b->k != O_FRAME)) continue;
        if (b->w < f->w - 16 || b->w > f->w + 2 || b->h < f->h - 16 || b->h > f->h + 2 || isect(b, f, 0)) continue;
        if (b->y < V2_HEAD || b->y + b->h > V2_FOOT) continue;
        int bx = b->x + b->w / 2, by = b->y + b->h / 2, dx = bx - cx, dy = by - cy, ok = 0;
        switch (k) { case K_RIGHT: ok = dx > f->w / 2 && abs(dy) < f->h / 2; break; case K_LEFT: ok = -dx > f->w / 2 && abs(dy) < f->h / 2; break;
                     case K_DOWN: ok = dy > f->h / 2 && abs(dx) < f->w / 2; break; case K_UP: ok = -dy > f->h / 2 && abs(dx) < f->w / 2; break; }
        if (ok && abs(dx) + abs(dy) < best) { best = abs(dx) + abs(dy); *out = *b; } }
    return best < (1 << 30);
}
static int quiet;                         // a run of presses that changed nothing
static Op active;                         // the outline that moved last: the cursor
static void ev(int k, int down) { fake_ms += 40; key_event(k, down); pump(1); }
static void after_key(int k, const Look *b4) {
    redraw(); check_frame(kname(k)); Look a; look(&a);
    quiet = same_look(b4, &a) && a.th == b4->th ? quiet + 1 : 0;
    if (!is_arrow(k)) return;
    int top = qb_focus || strip_focus ? 0 : 70;   // the strip's lit pad is no cursor while the page has it (the theme's swatches: pictures)
    if (quiet && !theming) for (int i = 0; i < b4->n; i++) { const Op *f = &b4->f[i]; Op c; if (f->y < top) continue;
        if (b4->n > 1 && memcmp(&f->x, &active.x, 4 * sizeof(int))) continue;   // several outlines: the one that moved last is the cursor
        if (candidate(f, k, &c)) report(1, "[%s] STUCK: %s does nothing on %d,%d %dx%d although an element is that way at %d,%d %dx%d", where, kname(k), f->x, f->y, f->w, f->h, c.x, c.y, c.w, c.h); }
    int io = -1, in = -1, no = 0, nn = 0;      // the outline that went and the one that came
    for (int i = 0; i < b4->n; i++) { int m = 0; for (int j = 0; j < a.n; j++) m |= !memcmp(&b4->f[i].x, &a.f[j].x, 4 * sizeof(int)); if (!m) { io = i; no++; } }
    for (int j = 0; j < a.n; j++) { int m = 0; for (int i = 0; i < b4->n; i++) m |= !memcmp(&b4->f[i].x, &a.f[j].x, 4 * sizeof(int)); if (!m) { in = j; nn++; } }
    if (nn >= 1) active = a.f[in];
    if (no == 1 && nn == 1 && b4->f[io].y >= top && a.f[in].y >= top) {
        const Op *f = &b4->f[io], *g = &a.f[in]; int dx = (g->x + g->w / 2) - (f->x + f->w / 2), dy = (g->y + g->h / 2) - (f->y + f->h / 2);
        int back = (k == K_RIGHT && dx < -8) || (k == K_LEFT && dx > 8) || (k == K_DOWN && dy < -8) || (k == K_UP && dy > 8);
        Op c; int near = candidate(f, k, &c);
        if (back && near) report(1, "[%s] %s goes the other way (%d,%d -> %d,%d) though an element is that way at %d,%d", where, kname(k), f->x, f->y, g->x, g->y, c.x, c.y);
        else if (back) report(0, "[%s] %s wraps round (%d,%d -> %d,%d)", where, kname(k), f->x, f->y, g->x, g->y);
        else if (near && abs(dx) + abs(dy) > 2 * (abs(c.x + c.w / 2 - f->x - f->w / 2) + abs(c.y + c.h / 2 - f->y - f->h / 2)) + 40 && g->w == f->w && g->h == f->h)
            report(1, "[%s] %s jumps far: %d,%d -> %d,%d, the next element that way is at %d,%d", where, kname(k), f->x, f->y, g->x, g->y, c.x, c.y);
    }
}
static int npress; static void tap(int k) { Look b; look(&b); npress++; if (getenv("TRACE")) printf("  %s %s foc %d\n", where, kname(k), b.n); ev(k, 1); ev(k, 0); after_key(k, &b); }
static void combo(int m, int k) { Look b; look(&b); ev(m, 1); ev(k, 1); ev(k, 0); ev(m, 0); after_key(k + 1000, &b); }
static void clean(void) {                // every window shut, HOME
    exporting = naming = 0; asking = 0; cfm = 0; qpicking = 0; if (qd_open) qd_open = 0;
    set_tab(T_GRID); ui_cat = -1; scening = 0; qb_focus = strip_focus = tab_focus = side_foc = 0; perform = 0; pump(2);
}
typedef void (*Setup)(void);
static void walk(const char *nm, Setup su) {   // the rows: DOWN r times, then RIGHT along it and LEFT back; then UP to the bar
    if (!su || !*nm || (getenv("ONLY") && !strstr(nm, getenv("ONLY")))) return;
    snprintf(where, sizeof where, "%s", nm); clean(); su(); redraw(); check_frame("open"); memset(&active, 0, sizeof active);
    { char f[128]; snprintf(f, sizeof f, "ex_%s.bmp", nm); SDL_SaveBMP(sf, f); }
    for (int r = 0; r < 10; r++) {
        clean(); su(); redraw(); quiet = 0; memset(&active, 0, sizeof active);
        for (int i = 0; i < r && quiet < 2; i++) tap(K_DOWN);
        if (r && quiet >= 2) break;
        if (qb_focus || qd_open) break;
        quiet = 0; for (int i = 0; i < 30 && quiet < 2; i++) tap(K_RIGHT);
        quiet = 0; for (int i = 0; i < 30 && quiet < 2; i++) tap(K_LEFT);
    }
    clean(); su(); redraw(); quiet = 0; memset(&active, 0, sizeof active);
    for (int i = 0; i < 12 && !qb_focus && quiet < 3; i++) { tap(K_UP); if (getenv("TRACE")) printf("  UP: tab_focus %d strip %d qb %d side %d ducking %d v2_auto %d quiet %d\n", tab_focus, strip_focus, qb_focus, side_foc, ducking, v2_auto(), quiet); }
    if (!qb_focus && !ui_over() && !(rolling && !chording) && tab != T_PAT && !qpicking) report(1, "[%s] UP never reaches the quick bar", nm);
    for (int i = 0; i < 12 && (qb_focus || tab_focus || strip_focus); i++) tap(K_DOWN);
    if (qb_focus || tab_focus || strip_focus) report(1, "[%s] DOWN never comes back from the bar", nm);
}
static void key(int k) { ev(k, 1); ev(k, 0); }       // a press with no checks (to set a screen up)
static void kombo(int m, int k) { ev(m, 1); key(k); ev(m, 0); }
static int g_pad, g_pg, g_sec;           // the screen's pad, page, section
static void slot(int n) {                 // onto side slot n (1-4 left, 5-8 right) by the arrows, as a user gets there
    redraw(); int k = n > 4 ? K_RIGHT : K_LEFT;
    for (int i = 0; i < 40 && !side_foc; i++) key(k);
    for (int i = 0; i < 4; i++) key(K_UP);
    for (int i = 0; i < 4 && side_foc != n; i++) key(K_DOWN);
    if (side_foc != n) report(1, "[%s] side slot %d not reached by the arrows (at %d)", where, n, side_foc);
}
static void su_home(void) { }
static void su_scenes(void) { key(K_R1); }
static void su_mixer(void) { key(K_R1); key(K_R1); if (g_pg) { mix_goto(g_pg); tab_focus = 0; pump(1); } }
static void su_mxdrawer(void) { key(K_R1); key(K_R1); mix_goto(0); key(K_RIGHT); for (int i = 0; i < 13; i++) key(K_RIGHT); }
static void su_mxunit(void) { su_mxdrawer(); key(K_EDIT); }
static void su_prj(void) { kombo(K_MENU, K_UP); jsec = g_sec; jcol = 0; pump(1); }
static void su_cat(void) { sel = g_pad; cat_open(g_pg); pump(1); }
static void su_sound(void) { sel = g_pad; cat_open(PC_SOUND); snd_page_go(g_sec); pump(1); }
static void su_layout(void) {            // ROLL RHYTHM CHORDS by the LAYOUT slot: EDIT+DOWN from STEPS, and on
    sel = g_pad; cat_open(PC_PATTERN); pump(1); if (cur_pat(sel)->mode) lay_to(0); pump(1);
    set_step(0); slot(1);
    for (int i = 0; i < g_sec; i++) { kombo(K_EDIT, K_DOWN); pump(2); if (cfm) { key(K_EDIT); pump(2); } if (getenv("TRACE")) printf("  layout %d: foc %d mode %d rolling %d chording %d rh %d\n", i, side_foc, cur_pat(sel)->mode, rolling, chording, rl_rhythm); }
    key(K_RIGHT); side_foc = 0; pump(1);
}
static void su_auto(void) { sel = g_pad; cat_open(PC_MOD); pump(1); slot(1); key(K_EDIT); pump(1); }
static void su_duck(void) { sel = g_pad; cat_open(PC_MOD); pump(1); slot(2); key(K_EDIT); pump(1); }
static void su_slot(void) { sel = g_pad; cat_open(g_pg); pump(1); if (cur_pat(sel)->mode) lay_to(0); slot(g_sec); key(K_EDIT); pump(1); }   // a window behind a side slot
static void su_bar(void) { key(K_X); }
static void su_drawer(void) { key(K_X); key(K_X); }
static void su_slice(void) { sel = g_pad; cat_open(PC_SOUND); pump(1); slicing = 1; pump(1); }
static void su_export(void) { export_open(); pump(1); }
static void su_naming(void) { kombo(K_MENU, K_UP); naming_open(0); pump(1); }
static void su_picker(void) { sel = g_pad; key(K_EDIT); pump(1); }   // EDIT on an empty pad: which instrument
static void su_qpick(void) { qp_open(); pump(1); }
static void su_icons(void) { sel = g_pad; icon_open(); pump(1); }
static void su_theme(void) { kombo(K_MENU, K_UP); jcur = J_THEME; theming = 1; pump(1); }
// ---- a user at random: single presses and short combos; after each the frame checks; every so often Y must lead home ----
static int at_home(void) { return (tab == T_GRID && ui_cat < 0 && !scening) || v2_mixer() || (tab == T_GRID && scening); }
static void y_home(const char *trail) {
    char path[160]; ui_path(path, sizeof path); int i;
    for (i = 0; i < 10 && !(at_home() && !qb_focus && !strip_focus && !tab_focus && !side_foc && !qd_open); i++) { key(K_Y); pump(1); if (cfm || asking) key(K_Y); }
    redraw(); check_frame("Y");
    if (!at_home()) report(1, "[fuzz] dead end: Y x10 from \"%s\" stays on \"%s\" (tab %d cat %d) after %s", path, (ui_path(path, sizeof path), path), tab, ui_cat, trail);
}
static void fuzz(unsigned seed, int n) {
    static const int keys[] = {K_UP, K_DOWN, K_LEFT, K_RIGHT, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_EDIT, K_EDIT, K_Y, K_X, K_OPT, K_L1, K_R1, K_F1, K_F2, K_SHIFT, K_MENU};
    static const int mods[] = {K_EDIT, K_OPT, K_SHIFT, K_MENU, K_X};
    srand(seed); clean(); char trail[400] = ""; int tl = 0;
    for (int i = 0; i < n; i++) {
        snprintf(where, sizeof where, "fuzz %u #%d", seed, i); { char p[160]; ui_path(p, sizeof p); snprintf(where, sizeof where, "fuzz: %s", p); }
        int r = rand() % 100, k = keys[rand() % (int)(sizeof keys / sizeof *keys)];
        if (exporting || naming) { key(K_Y); exporting = naming = 0; continue; }   // no long renders, no typing
        if (r < 15) { int m = mods[rand() % 5], a = K_UP + rand() % 4; if (m == K_OPT && a == K_DOWN && rand() % 2) a = K_EDIT;
            tl += snprintf(trail + tl, sizeof trail - tl > 0 ? sizeof trail - tl : 0, "%s+%s ", kname(m), kname(a)); combo(m, a); }
        else { tl += snprintf(trail + tl, sizeof trail - tl > 0 ? sizeof trail - tl : 0, "%s ", kname(k)); tap(k); }
        if (tl > 300) { memmove(trail, trail + tl - 200, 201); tl = 200; }
        if (i % 40 == 39) { y_home(trail); tl = 0; trail[0] = 0; }
    }
    clean();
}
int main(int argc, char **argv) {
    srand(1); sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1);
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    set_tab(T_GRID); ui_cat = -1; sel = 0; pump(1);
    if (argc > 1) { snprintf(where, sizeof where, "home"); redraw(); dump("home");
        if (argc > 2) { for (int i = 0; i < 5 && !qb_focus; i++) key(K_UP); redraw(); dump("bar"); SDL_SaveBMP(sf, "dbg_bar.bmp"); return 0; } }
    walk("home", su_home); walk("scenes", su_scenes);
    { static const char *const mn[MXP_N] = {"mixer", "mixer_ret_a", "mixer_ret_b", "mixer_master", "mixer_freeze"};
      for (g_pg = 0; g_pg < MXP_N; g_pg++) walk(mn[g_pg], su_mixer); g_pg = 0; }
    walk("mixer_drawer", su_mxdrawer); walk("mixer_unit", su_mxunit);
    for (g_sec = 0; g_sec < JS_N; g_sec++) { char nm[64]; snprintf(nm, sizeof nm, "project_%s", prj_sec_name[g_sec]); walk(nm, su_prj); }
    walk("quick_bar", su_bar); walk("drawer", su_drawer);
    int done[4] = {0}, empty = -1, smp = -1, synth = -1;
    for (int s = 0; s < NSLOT; s++) { if (!has_sound(s)) { if (empty < 0) empty = s; continue; }
        int ty = song.t[s].type & 3; if (ty == 0 && smp < 0) smp = s; if (ty == 1 && synth < 0) synth = s; if (done[ty]) continue; done[ty] = 1;
        static const char *const en[4] = {"smp", "grand", "acid", "cloud"}; char nm[64]; g_pad = s;
        for (int c = 0; c < PC_N; c++) { g_pg = c; snprintf(nm, sizeof nm, "%s_%s", en[ty], cat_name[c]); walk(nm, su_cat); }
        { int np; snd_pages(song.t[s].type, &np); sel = s;
          for (g_sec = 0; g_sec < np; g_sec++) { snprintf(nm, sizeof nm, "%s_SOUND_%s", en[ty], snd_page_nm(g_sec)); walk(nm, su_sound); } }
        int m0 = cur_pat(s)->mode;
        for (g_sec = 1; g_sec <= 3; g_sec++) { snprintf(nm, sizeof nm, "%s_layout_%s", en[ty], (const char *[]){"", "ROLL", "RHYTHM", "CHORDS"}[g_sec]); walk(nm, su_layout); }
        clean(); cur_pat(s)->mode = (uint8_t)m0; rl_rhythm = 0;   // the clip as it was
        snprintf(nm, sizeof nm, "%s_AUTO", en[ty]); walk(nm, su_auto);
        snprintf(nm, sizeof nm, "%s_DUCK", en[ty]); walk(nm, su_duck);
        g_pg = PC_PATTERN; g_sec = 4; snprintf(nm, sizeof nm, "%s_PATTERNS", en[ty]); walk(nm, su_slot);
        g_pg = PC_PATTERN; g_sec = 7; snprintf(nm, sizeof nm, "%s_RANDOM", en[ty]); walk(nm, su_slot);
        g_pg = PC_SOUND; g_sec = 3; snprintf(nm, sizeof nm, "%s_%s", en[ty], ty == 1 || ty == 2 ? "PRESETS" : "BROWSER"); walk(nm, su_slot);
        walk(en[ty][0] == 's' ? "smp_ICON" : "", NULL);
    }
    if (smp >= 0) { g_pad = smp; walk("slice_editor", su_slice); walk("icons", su_icons); }
    if (empty >= 0) { g_pad = empty; walk("empty_picker", su_picker); for (int c = 0; c < PC_N; c++) { char nm[64]; g_pg = c; snprintf(nm, sizeof nm, "empty_%s", cat_name[c]); walk(nm, su_cat); } }
    // scenarios: a layout kept when the pad is left and entered again by the keys
    if (synth >= 0) { snprintf(where, sizeof where, "chords kept"); clean(); g_pad = synth; g_sec = 3; su_layout(); redraw();
        int ok0 = chording; if (!ok0) report(1, "[chords kept] LAYOUT EDIT+DOWN to CHORDS (the clip had notes, EDIT answers the question): mode %d but no CHORDS page", cur_pat(sel)->mode); ok0 = 1; for (int i = 0; i < 4 && ui_cat >= 0; i++) key(K_Y); key(K_EDIT); pump(2); redraw();
        if (ui_cat == PC_PATTERN && !chording && ok0) report(1, "[chords kept] CHORDS layout, Y to HOME, EDIT on the pad: PATTERN opens the ROLL (LAYOUT slot says CHORDS)");
        key(K_F2); key(K_F1); pump(2); redraw();
        if (ui_cat == PC_PATTERN && !chording && ok0) report(1, "[chords kept] CHORDS layout, F2 then F1: PATTERN opens the ROLL");
        lay_to(0); clean(); }
    for (unsigned sd = 1; sd <= (getenv("FUZZ") ? (unsigned)atoi(getenv("FUZZ")) : 4); sd++) if (!getenv("ONLY") || strstr("fuzz", getenv("ONLY"))) fuzz(sd, 1200);
    walk("export", su_export); walk("naming", su_naming); walk("bar_picker", su_qpick); walk("theme", su_theme);
    printf("%d presses, %d fails %d notes\n", npress, fails, warns);
    return 0;
}
