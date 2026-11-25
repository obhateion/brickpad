// Golden run for the UI v2 refactor: the same input script on two source trees must give the same hashes.
// Build: sh build.sh SRC_DIR OUT. Run in a scratch folder in /tmp with samples/ and projects/DEMO/NEON.bpp (run.sh does it).
// Prints one line per step: step, key, song hash, screen hash; then the hash of a 4-bar WAV export of NEON.
// Time is faked (SDL_GetTicks advances 40 ms per event), rand() is seeded, the library thread is never started.
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned fake_ms = 1000;
#define SDL_GetTicks fake_ticks
#define SDL_GetPerformanceCounter fake_pc
#include <SDL2/SDL.h>
Uint32 fake_ticks(void) { return fake_ms; }
static Uint64 pc_n; Uint64 fake_pc(void) { return ++pc_n; }   // DSP load reads as 0: no timing on screen
#include "engine.c"
#define logf bp_logf
#define main bp_main
#include "main.c"
#undef main
#include <time.h>
#include "housegen.h"

#ifdef OLD                                // 1.2: the pad is the selected one
#define PAR_ADJUST(pad, p, d, big) par_adjust(p, d, big)
#define PAR_STR(pad, p, b, n) par_str(p, b, n)
#else
#define PAR_ADJUST par_adjust
#define PAR_STR par_str
#endif
static SDL_Surface *sf; static float buf[2 * 128];
static void pump(int n) { while (n--) { callback(0, (Uint8 *)buf, sizeof buf); engine_collect_garbage(); scene_poll(); } }
static uint64_t gfnv(const void *p, size_t n) {
    const uint8_t *b = p; uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}
static uint64_t screen_hash(void) {
    for (int i = 0; i < NSLOT; i++) flash[i] = 0;
    msg_t = 0; draw();
    const uint32_t *px = sf->pixels; uint64_t h = 0;
    for (int i = 0; i < W * H; i++) h = h * 31 + px[i];
    return h;
}
static uint32_t lcg = 12345;
static int grnd(int n) { lcg = lcg * 1103515245u + 12345u; return (int)((lcg >> 8) % (unsigned)n); }
static void ev(int k, int down) { fake_ms += 40; key_event(k, down); }

int main(int ac, char **av) {
    int steps = ac > 1 ? atoi(av[1]) : 2000, params = ac > 2 && !strcmp(av[2], "params"), shots = ac > 2 && !strcmp(av[2], "shots");
    srand(1); setvbuf(stdout, NULL, _IOLBF, 0);   // a crash keeps the lines before it
    sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    song_defaults(); prj_startup(); pump(1);
    if (getenv("HOUSEGEN")) {             // build HOUSE, open it, render every scene: level and the time it takes
        if (!getenv("HGLOAD") && house_gen(getenv("HOUSEGEN"))) return 1;
        prj_load(getenv("HOUSEGEN")); pump(4); printf("loaded: scenes %d\n", scn_n);
        for (int k = 0; k < scn_n; k++) {
            if (k != scn_cur) { scene_go(k); engine_scene_force(); pump(1); scene_poll(); }
            for (int s = 0; s < NSLOT; s++) { Track *t = &song.t[s]; t->on = has_sound(s) && (pat_used(&t->pat[0]) || roll_any(&t->pat[0])); }
            engine_transport(1); float b2[256]; double pk = 0, ss = 0; long n = 0; int clip = 0;
            long frames = (long)(4 * 4 * 60.0 / song.tempo * SR); struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
            for (long f = 0; f < frames; f += 128) { engine_render(b2, 128); for (int i = 0; i < 256; i++) { double v = fabs(b2[i]); if (v > pk) pk = v; ss += v * v; n++; if (v > 0.999) clip++; } }
            clock_gettime(CLOCK_MONOTONIC, &t1); double el = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
            int on = 0; for (int s = 0; s < NSLOT; s++) on += song.t[s].on;
            printf("scene %d: pads on %d, peak %.3f rms %.3f (%.1f dB), clipped %d, render %.2fx of real time (-O0)\n", k + 1, on, pk, sqrt(ss / n), 20 * log10(sqrt(ss / n) + 1e-9), clip, el / (frames / (double)SR));
            engine_transport(0); engine_stop_all(); pump(8);
        }
        return 0;
    }
    prj_load(getenv("PRJ") ? getenv("PRJ") : "projects/DEMO/NEON.bpp"); pump(4);
    printf("load song %016llx screen %016llx\n", (unsigned long long)gfnv(&song, sizeof song), (unsigned long long)screen_hash());
    if (getenv("SCRIPT")) {               // a played session: tokens split by spaces. KEY a tap, A+B hold A and tap B, +KEY / -KEY down / up,
        static const char *const kn[K_N] = {"UP", "DOWN", "LEFT", "RIGHT", "OPT", "EDIT", "X", "Y", "L1", "L2", "R1", "R2", "MENU", "PLAY", "SHIFT", "F1", "F2"};
        char *sc = strdup(getenv("SCRIPT")), b[32]; int n = 0;   // KEY*N a tap N times, wN wait N tenths of a second, ! a screenshot
        char *sv = NULL;
        for (char *t = strtok_r(sc, " \n", &sv); t; t = strtok_r(NULL, " \n", &sv)) {   // the app uses strtok itself
            if (!strcmp(t, "!")) { pump(8); screen_hash(); snprintf(b, sizeof b, "shot_%02d.bmp", n++); SDL_SaveBMP(sf, b); continue; }
            if (t[0] == 'w' && t[1] >= '0' && t[1] <= '9') { int k = atoi(t + 1); while (k--) { fake_ms += 100; pump(35); } continue; }
            int updown = t[0] == '+' ? 1 : t[0] == '-' ? 2 : 0, rep = 1, hold[4], nh = 0; if (updown) t++;
            char *st = strchr(t, '*'); if (st) { *st = 0; rep = atoi(st + 1); }
            for (char *q = t; ; ) { char *pl = strchr(q, '+'); if (pl) *pl = 0;
                int k = -1; for (int i = 0; i < K_N; i++) if (!strcmp(q, kn[i])) k = i;
                if (k < 0) { fprintf(stderr, "script: unknown key %s\n", q); return 2; }
                if (pl) { hold[nh++] = k; ev(k, 1); q = pl + 1; continue; }
                for (int r = 0; r < rep; r++) { if (updown != 2) ev(k, 1); if (updown != 1) ev(k, 0); pump(2); }
                break; }
            while (nh) ev(hold[--nh], 0);
            pump(4);
        }
        printf("script done, %d shots, song %016llx\n", n, (unsigned long long)gfnv(&song, sizeof song));
        if (getenv("SCEXP")) {            // scenes: export them all, then a save and a load of the project
            int c0 = scn_cur; set_tab(T_PRJ); prj_goto(J_EXPORT); key_event(K_EDIT, 1); key_event(K_EDIT, 0);
            ex_bars = 2; ex_row = EX_GO; key_event(K_EDIT, 1); key_event(K_EDIT, 0);
            while (ex_run) export_tick();
            printf("scenes export: frames %ld peak %.3f, scenes %d, open %d (was %d)\n", ex_done, ex_peak, scn_n, scn_cur + 1, c0 + 1);
            if (scn_n > 1) scn_rest(scn[scn_cur ? 0 : 1]);
            uint64_t h1 = gfnv(&song, sizeof song), h2 = scn_n > 1 ? gfnv(scn[scn_cur ? 0 : 1], sizeof(Song)) : 0;
            prj_save("SCT"); save_state(); prj_load("projects/SCT.bpp"); pump(4);
            printf("reload: scenes %d open %d same %d %d\n", scn_n, scn_cur + 1, h1 == gfnv(&song, sizeof song), scn_n > 1 && h2 == gfnv(scn[scn_cur ? 0 : 1], sizeof(Song)));
            scenes_read(SCN_FILE, 0); printf("scenes.bin: scenes %d open %d same %d\n", scn_n, scn_cur + 1, scn_n > 1 && h2 == gfnv(scn[scn_cur ? 0 : 1], sizeof(Song)));
        }
        return 0;
    }
    if (shots) {                          // screens for a look: shot_NN.bmp in the run folder, moved out by run.sh
        const char *keys = getenv("SHOTS"); if (!keys) keys = "g f f f f p M S E";   // g GRID, f F2, e EDIT, y Y, j SHIFT+RIGHT, u UP+LEFT, d DOWN, p PROJECT, M PADS, S SHIFT held, E EDIT+RIGHT held
        int n = 0; char b[32];
        for (const char *c = keys; ; c++) {
            if (*c == ' ') continue;
            if (*c == 'f') { ev(K_F2, 1); ev(K_F2, 0); }
            if (*c == 'e') { ev(K_EDIT, 1); ev(K_EDIT, 0); }
            if (*c == 'C') song.count_in = 4;
            if (*c == 'l') { ev(K_LEFT, 1); ev(K_LEFT, 0); }
            if (*c == 'r') { ev(K_RIGHT, 1); ev(K_RIGHT, 0); }
            if (*c == 'R') { ev(K_R1, 1); ev(K_R1, 0); }
            if (*c == 'P') { ev(K_PLAY, 1); ev(K_PLAY, 0); pump(300); }
            if (*c == 'y') { ev(K_Y, 1); ev(K_Y, 0); }
            if (*c == 'j') { ev(K_SHIFT, 1); ev(K_RIGHT, 1); ev(K_RIGHT, 0); ev(K_SHIFT, 0); }
            if (*c == 'u') { ev(K_UP, 1); ev(K_UP, 0); ev(K_LEFT, 1); ev(K_LEFT, 0); }
            if (*c == 'd') { ev(K_DOWN, 1); ev(K_DOWN, 0); }
            if (*c == 'p') { ev(K_F1, 1); ev(K_F2, 1); ev(K_F2, 0); ev(K_F1, 0); }
            if (*c == 'M') { ev(K_F1, 1); ev(K_F2, 1); ev(K_F2, 0); ev(K_F1, 0); ev(K_MENU, 1); ev(K_MENU, 0); }
            if (*c == 'S') { ev(K_MENU, 1); ev(K_MENU, 0); ev(K_MENU, 1); ev(K_MENU, 0); ev(K_F2, 1); ev(K_F2, 0); ev(K_SHIFT, 1); fake_ms += 800; }
            if (*c == 'E') { ev(K_SHIFT, 0); ev(K_DOWN, 1); ev(K_DOWN, 0); ev(K_DOWN, 1); ev(K_DOWN, 0); ev(K_DOWN, 1); ev(K_DOWN, 0); ev(K_EDIT, 1); ev(K_RIGHT, 1); ev(K_RIGHT, 0); }
            pump(40); screen_hash(); snprintf(b, sizeof b, "shot_%02d.bmp", n++); SDL_SaveBMP(sf, b);
            if (!*c || !c[1]) break;
        }
        return 0;
    }
    if (params) {                         // every parameter of every pad, up and down to its ends, small and big steps
        for (int pad = 0; pad < NSLOT; pad++) for (int p = 0; p < P_N; p++) {
            if (p == P_SAMPLE || p == P_TYPE || p == P_SRC) continue;
            uint64_t h = 0;
            for (int r = 0; r < 4; r++) {
                for (int i = 0; i < 40; i++) {
                    char t[96]; sel = pad; PAR_ADJUST(pad, p, r & 1 ? -1 : 1, r >= 2); PAR_STR(pad, p, t, sizeof t); h = h * 31 + gfnv(t, strlen(t));
                }
                h = h * 31 + gfnv(&song, sizeof song);
            }
            char b[96]; PAR_STR(pad, p, b, sizeof b);
            printf("pad %d p %d hash %016llx text %s\n", pad, p, (unsigned long long)h, b);
            undo_armed = 0;
        }
        return 0;
    }
    // keys without MENU-hold quitting (main loop only) and without SHIFT+PLAY (left unbound on purpose)
    static const int pool[] = {K_UP, K_DOWN, K_LEFT, K_RIGHT, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_EDIT, K_EDIT, K_OPT, K_X, K_Y, K_Y,
                               K_L1, K_R1, K_L2, K_R2, K_F1, K_F2, K_MENU, K_PLAY};
    static const int mods[] = {K_EDIT, K_OPT, K_SHIFT, K_X, K_Y, K_EDIT, K_SHIFT};
    for (int s = 0; s < steps; s++) {
        int k = pool[grnd(sizeof pool / sizeof *pool)], m = -1;
        if (grnd(4) == 0) m = mods[grnd(sizeof mods / sizeof *mods)];
        if (m == k || (m == K_SHIFT && k == K_PLAY)) m = -1;
        if (k == K_F1 && grnd(6) == 0) m = K_F2;                    // F1+F2: PROJECT
        if (exporting && k == K_EDIT) k = K_Y;                     // no long renders inside the script
        if (m >= 0) ev(m, 1);
        ev(k, 1); ev(k, 0);
        if (m >= 0) ev(m, 0);
        if (s % 7 == 0) pump(8);
        printf("%d %d %d song %016llx screen %016llx\n", s, k, m, (unsigned long long)gfnv(&song, sizeof song), (unsigned long long)screen_hash());
    }
    // the sound: NEON as loaded, 4 bars to a WAV
    if (exporting) export_key(K_Y, 1), export_key(K_Y, 0);
    naming = asking = browsing = 0;
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    set_tab(T_PRJ); prj_goto(J_EXPORT); key_event(K_EDIT, 1); key_event(K_EDIT, 0);
    ex_bars = 2; ex_all = 1; ex_row = EX_GO; key_event(K_EDIT, 1); key_event(K_EDIT, 0);
    while (ex_run) export_tick();
    printf("export done\n");
    return 0;
}
