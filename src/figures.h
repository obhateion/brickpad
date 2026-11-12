// Thin-line figures: white wire, one orange detail. Pure decoration, no state.
// Each is drawn once, supersampled, into a texture; after that drawing one is a single copy. Coordinates are -1..1.
// Included by main.c (needs R and <math.h>, which main.c includes before renaming logf).
#pragma once
#include <stdlib.h>
enum { FIG_GLOBE, FIG_WAVES, FIG_CHECK, FIG_TORUS, FIG_CUBE, FIG_TUNNEL, FIG_DOTS, FIG_HELIX, FIG_LISSA, FIG_ROSE, FIG_RADAR, FIG_FLOOR, FIG_N };
enum { FIG_SMALL, FIG_MID, FIG_BIG, FIG_SS = 3 };
static const int fig_size[3] = {104, 200, 340};
static const char *fig_name[FIG_N] = {"WIRE GLOBE", "WAVES", "CHECKER", "TORUS", "WIRE CUBE", "TUNNEL", "DOT SPHERE", "HELIX", "LISSAJOUS", "ROSE", "RADAR", "FLOOR"};

static unsigned fig_gen = 1;                // the theme's text or accent changed: textures are stale
static Uint8 *fig_mask[2];                  // coverage: white, orange
static int fig_w, fig_lay, fig_pen, fig_have;
static float fig_lx, fig_ly, fig_ca, fig_sa, fig_cb, fig_sb;
#define FIG_PI 3.14159265f

static void fig_stamp(int x, int y, int r) {
    for (int b = y - r; b <= y + r; b++) if (b >= 0 && b < fig_w)
        for (int a = x - r; a <= x + r; a++) if (a >= 0 && a < fig_w) fig_mask[fig_lay][b * fig_w + a] = 255;
}
static int fig_px(float v) { return (int)((v + 1) * 0.5f * (fig_w - 1)); }
static void fig_seg(float x0, float y0, float x1, float y1) {
    float dx = (x1 - x0) * 0.5f * fig_w, dy = (y1 - y0) * 0.5f * fig_w;
    int n = (int)(fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy)) + 1;
    for (int i = 0; i <= n; i++) fig_stamp(fig_px(x0 + (x1 - x0) * i / n), fig_px(y0 + (y1 - y0) * i / n), fig_pen);
}
static void fig_brk(void) { fig_have = 0; }
static void fig_pt(float x, float y) {
    if (fig_have) fig_seg(fig_lx, fig_ly, x, y);
    fig_lx = x; fig_ly = y; fig_have = 1;
}
static void fig_view(float ax, float ay) { fig_ca = cosf(ax); fig_sa = sinf(ax); fig_cb = cosf(ay); fig_sb = sinf(ay); }
static float fig_rot(float x, float y, float z, float *u, float *v) {   // returns depth, > 0 faces us
    float x1 = x * fig_cb + z * fig_sb, z1 = -x * fig_sb + z * fig_cb;
    *u = x1; *v = y * fig_ca - z1 * fig_sa;
    return y * fig_sa + z1 * fig_ca;
}
static void fig_pt3(float x, float y, float z, float k, int front) {
    float u, v, d = fig_rot(x, y, z, &u, &v);
    if (front && d < 0) { fig_brk(); return; }
    fig_pt(u * k, v * k);
}
static void fig_ell(float cx, float cy, float rx, float ry) {
    fig_brk();
    for (int i = 0; i <= 120; i++) { float t = i * 2 * FIG_PI / 120; fig_pt(cx + rx * cosf(t), cy + ry * sinf(t)); }
    fig_brk();
}

static void fig_globe(void) {
    const float k = 0.74f;
    fig_view(0.2f, 0.35f);
    for (int la = -2; la <= 2; la++) {
        float a = la * FIG_PI / 6, r = cosf(a), y = sinf(a);
        fig_brk();
        for (int i = 0; i <= 96; i++) { float t = i * 2 * FIG_PI / 96; fig_pt3(r * cosf(t), y, r * sinf(t), k, 1); }
    }
    for (int lo = 0; lo < 8; lo++) {
        float a = lo * FIG_PI / 8;
        fig_brk();
        for (int i = 0; i <= 96; i++) { float t = i * 2 * FIG_PI / 96; fig_pt3(cosf(t) * cosf(a), sinf(t), cosf(t) * sinf(a), k, 1); }
    }
    fig_ell(0, 0, k, k);
    fig_lay = 1;
    fig_view(0.26f, 0.0f);
    fig_brk();
    for (int i = 0; i <= 160; i++) {
        float t = i * 2 * FIG_PI / 160, x = 1.3f * cosf(t), z = 1.3f * sinf(t), u, v;
        fig_rot(x, 0, z, &u, &v);
        float c = 0.96f, s = -0.28f;                // lay the ring over
        fig_pt((u * c - v * s) * k, (u * s + v * c) * k);
    }
}
static void fig_waves(void) {
    for (int l = 0; l < 11; l++) {
        float y = -0.8f + l * 0.16f;
        fig_lay = l == 6; fig_brk();
        for (int i = 0; i <= 120; i++) {
            float x = -1 + i * 2.0f / 120, e = exp(-x * x * 3.2f);
            fig_pt(x * 0.96f, y + e * 0.12f * sinf(x * 9 + l * 0.55f) * (0.6f + 0.4f * sinf(l * 0.7f)));
        }
    }
}
static void fig_check(void) {
    const float k = 0.84f, ax = 0.35f, ay = 0.3f; fig_view(ax, ay);
    for (int b = 0; b < fig_w; b++) for (int a = 0; a < fig_w; a++) {
        float u = (a * 2.0f / (fig_w - 1) - 1) / k, v = (b * 2.0f / (fig_w - 1) - 1) / k, q = 1 - u * u - v * v;
        if (q <= 0) continue;
        float n = sqrtf(q);                         // undo the view: screen normal back to the sphere
        float y = v * fig_ca + n * fig_sa, z1 = -v * fig_sa + n * fig_ca, x = u * fig_cb - z1 * fig_sb, z = u * fig_sb + z1 * fig_cb;
        int lo = (int)floorf(atan2f(z, x) / (FIG_PI / 6)), la = (int)floorf(asinf(y > 1 ? 1 : y < -1 ? -1 : y) / (FIG_PI / 6));
        if ((lo + la) & 1) fig_mask[0][b * fig_w + a] = (Uint8)(50 + 120 * n);
    }
    fig_ell(0, 0, k, k);
    fig_lay = 1; fig_stamp(fig_px(0.86f), fig_px(-0.86f), fig_w / 40);
}
static void fig_torus(void) {
    const float R0 = 0.62f, r0 = 0.28f; fig_view(0.9f, 0.25f);
    for (int m = 0; m < 20; m++) {
        float a = m * 2 * FIG_PI / 20; fig_lay = 0; fig_brk();
        for (int i = 0; i <= 40; i++) { float t = i * 2 * FIG_PI / 40, q = R0 + r0 * cosf(t); fig_pt3(q * cosf(a), r0 * sinf(t), q * sinf(a), 1, 1); }
    }
    fig_lay = 1; fig_brk();
    for (int i = 0; i <= 96; i++) { float t = i * 2 * FIG_PI / 96; fig_pt3((R0 + r0) * cosf(t), 0, (R0 + r0) * sinf(t), 1, 0); }
}
static void fig_box(float s) {
    static const signed char e[12][6] = {{-1,-1,-1,1,-1,-1},{1,-1,-1,1,1,-1},{1,1,-1,-1,1,-1},{-1,1,-1,-1,-1,-1},{-1,-1,1,1,-1,1},{1,-1,1,1,1,1},{1,1,1,-1,1,1},{-1,1,1,-1,-1,1},{-1,-1,-1,-1,-1,1},{1,-1,-1,1,-1,1},{1,1,-1,1,1,1},{-1,1,-1,-1,1,1}};
    for (int i = 0; i < 12; i++) {
        fig_brk(); fig_pt3(e[i][0] * s, e[i][1] * s, e[i][2] * s, 1, 0); fig_pt3(e[i][3] * s, e[i][4] * s, e[i][5] * s, 1, 0);
    }
    fig_brk();
}
static void fig_cube(void) { fig_view(0.6f, 0.7f); fig_box(0.56f); fig_lay = 1; fig_box(0.24f); }
static void fig_tunnel(void) {
    float s = 0.94f;
    for (int i = 0; i < 9; i++, s *= 0.72f) {
        fig_lay = i == 4; fig_brk();
        fig_pt(-s, -s); fig_pt(s, -s); fig_pt(s, s); fig_pt(-s, s); fig_pt(-s, -s); fig_brk();
    }
    fig_lay = 0;
    for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2) { fig_brk(); fig_pt(i * 0.94f, j * 0.94f); fig_pt(i * 0.06f, j * 0.06f); }
    fig_brk(); fig_pt(0, -0.94f); fig_pt(0, -0.06f); fig_brk(); fig_pt(0, 0.94f); fig_pt(0, 0.06f);
    fig_brk(); fig_pt(-0.94f, 0); fig_pt(-0.06f, 0); fig_brk(); fig_pt(0.94f, 0); fig_pt(0.06f, 0); fig_brk();
}
static void fig_dots(void) {
    fig_view(0.3f, 0.2f);
    for (int i = 0; i < 420; i++) {          // Fibonacci sphere, nearer dots bigger
        float y = 1 - (i + 0.5f) * 2 / 420, r = sqrtf(1 - y * y), t = i * 2.39996f, u, v, d = fig_rot(r * cosf(t), y, r * sinf(t), &u, &v);
        if (d < 0) continue;
        fig_lay = i % 37 == 0;
        fig_stamp(fig_px(u * 0.9f), fig_px(v * 0.9f), 1 + (int)(d * fig_w / 150));
    }
}
static void fig_helix(void) {
    for (int s = 0; s < 2; s++) {
        fig_lay = s; fig_brk();
        for (int i = 0; i <= 140; i++) { float y = -0.94f + i * 1.88f / 140; fig_pt((s ? -1 : 1) * 0.5f * sinf(y * 7), y); }
    }
    fig_lay = 0;
    for (int i = 0; i < 16; i++) { float y = -0.88f + i * 0.1175f, x = 0.5f * sinf(y * 7); fig_brk(); fig_pt(x, y); fig_pt(-x, y); }
    fig_brk();
}
static void fig_lissa(void) {
    fig_brk();
    for (int i = 0; i <= 360; i++) { float t = i * 2 * FIG_PI / 360; fig_pt(0.92f * sinf(3 * t + FIG_PI / 2), 0.92f * sinf(2 * t)); }
    fig_lay = 1; fig_brk(); fig_pt(-0.12f, 0); fig_pt(0.12f, 0); fig_brk(); fig_pt(0, -0.12f); fig_pt(0, 0.12f); fig_brk();
}
static void fig_rose(void) {
    fig_brk();
    for (int i = 0; i <= 360; i++) { float t = i * 2 * FIG_PI / 360, r = 0.92f * cosf(5 * t); fig_pt(r * cosf(t), r * sinf(t)); }
    fig_lay = 1; fig_ell(0, 0, 0.3f, 0.3f);
}
static void fig_radar(void) {
    for (int i = 1; i <= 4; i++) fig_ell(0, 0, i * 0.235f, i * 0.235f);
    fig_brk(); fig_pt(-0.98f, 0); fig_pt(0.98f, 0); fig_brk(); fig_pt(0, -0.98f); fig_pt(0, 0.98f); fig_brk();
    for (int i = 0; i < 36; i++) { float t = i * FIG_PI / 18; fig_brk(); fig_pt(0.94f * cosf(t), 0.94f * sinf(t)); fig_pt(0.99f * cosf(t), 0.99f * sinf(t)); }
    fig_lay = 1; fig_brk(); fig_pt(0, 0); fig_pt(0.66f, -0.66f); fig_brk();
    fig_stamp(fig_px(0.32f), fig_px(-0.5f), fig_w / 45);
}
static void fig_floor(void) {
    const float hz = -0.1f;
    for (int i = -8; i <= 8; i++) { fig_brk(); fig_pt(i * 0.03f, hz); fig_pt(i * 0.24f, 0.95f); }
    float y = 0.95f;
    for (int i = 0; i < 9; i++, y = hz + (y - hz) * 0.68f) { fig_brk(); fig_pt(-1, y); fig_pt(1, y); }
    fig_brk(); fig_lay = 1; fig_ell(0, -0.5f, 0.32f, 0.32f);
}

static SDL_Texture *fig_build(int id, int size) {
    fig_w = size * FIG_SS; fig_pen = size >= 300 ? 2 : 1; fig_lay = 0; fig_have = 0;
    fig_mask[0] = calloc(fig_w * fig_w, 1); fig_mask[1] = calloc(fig_w * fig_w, 1);
    Uint32 *px = malloc(size * size * 4);
    SDL_Texture *tx = NULL;
    if (fig_mask[0] && fig_mask[1] && px) {
        switch (id) {
        case FIG_GLOBE: fig_globe(); break; case FIG_WAVES: fig_waves(); break; case FIG_CHECK: fig_check(); break;
        case FIG_TORUS: fig_torus(); break; case FIG_CUBE: fig_cube(); break; case FIG_TUNNEL: fig_tunnel(); break;
        case FIG_DOTS: fig_dots(); break; case FIG_HELIX: fig_helix(); break; case FIG_LISSA: fig_lissa(); break;
        case FIG_ROSE: fig_rose(); break; case FIG_RADAR: fig_radar(); break; default: fig_floor(); break;
        }
        for (int y = 0; y < size; y++) for (int x = 0; x < size; x++) {   // box filter down to screen size
            int w = 0, o = 0;
            for (int b = 0; b < FIG_SS; b++) for (int a = 0; a < FIG_SS; a++) {
                int k = (y * FIG_SS + b) * fig_w + x * FIG_SS + a;
                w += fig_mask[0][k]; o += fig_mask[1][k];
            }
            w /= FIG_SS * FIG_SS; o /= FIG_SS * FIG_SS;
            int s = w + o, al = s > 255 ? 255 : s;
            Col a = C_WHITE, b = C_ORANGE;           // the theme's text and accent
            int r = s ? (w * a.r + o * b.r) / s : 0, g = s ? (w * a.g + o * b.g) / s : 0, bl = s ? (w * a.b + o * b.b) / s : 0;
            px[y * size + x] = (Uint32)al << 24 | r << 16 | g << 8 | bl;
        }
        tx = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, size, size);
        if (tx) { SDL_UpdateTexture(tx, NULL, px, size * 4); SDL_SetTextureBlendMode(tx, SDL_BLENDMODE_BLEND); }
    }
    free(fig_mask[0]); free(fig_mask[1]); free(px);
    return tx;
}
// alpha 0..255: dim in the background, full when it matters
static void fig_put(int id, int sz, int x, int y, int alpha) {
    static SDL_Texture *cache[FIG_N][3]; static unsigned made[FIG_N][3];
    if (made[id][sz] != fig_gen) {
        if (cache[id][sz]) SDL_DestroyTexture(cache[id][sz]);
        made[id][sz] = fig_gen; cache[id][sz] = fig_build(id, fig_size[sz]);
    }
    SDL_Texture *t = cache[id][sz];
    if (!t) return;                          // decoration only: without memory the screen just goes without it
    int s = fig_size[sz]; SDL_Rect d = {x, y, s, s};
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    SDL_RenderCopy(R, t, NULL, &d);
}
