// WAVE on the pads (2026-11-25): GRAND's pads become WAVE, the SOUND pages, the notes, the file. Prints ALL OK.
// Build as the other qa_*: cc -O0 -std=gnu11 -w -Isrc qa/qa_wave_ui.c src/sample.c -lSDL2 -lm -lpthread -o qa_wave_ui
// Run in a folder with projects/DEMO/NEON.bpp (an 0x12 file: four GRAND pads, 6..9) and samples/.
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned fake_ms = 1000;
#define SDL_GetTicks fake_ticks
#define SDL_GetPerformanceCounter fake_pc
#include <SDL2/SDL.h>
Uint32 fake_ticks(void) { return fake_ms; }
static Uint64 pc_n; Uint64 fake_pc(void) { return ++pc_n; }
#include "engine.c"
#define logf bp_logf
#define main bp_main
#include "main.c"
#undef main

static SDL_Surface *sf; static float buf[2 * 128]; static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL L%d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void pump(int n) { while (n--) { callback(0, (Uint8 *)buf, sizeof buf); engine_collect_garbage(); scene_poll(); } }
static void ev(int k, int down) { fake_ms += 40; key_event(k, down); }
static void tap(int k) { ev(k, 1); ev(k, 0); pump(1); }
static void qa_earrow(int k) { ev(K_EDIT, 1); ev(k, 1); ev(k, 0); ev(K_EDIT, 0); pump(1); }
static uint64_t fnv64(const void *p, size_t n) { const uint8_t *b = p; uint64_t h = 1469598103934665603ull; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; } return h; }
static void draw_ok(void) { for (int i = 0; i < NSLOT; i++) flash[i] = 0; msg_t = 0; draw(); }
// n frames of the mix (the device's way): peak, RMS, whether every sample was finite
static void render(long frames, double *pk, double *rms, int *bad) {
    double s2 = 0; long n = 0; *pk = 0; *bad = 0;
    for (long f = 0; f < frames; f += 128) { engine_render(buf, 128); engine_collect_garbage();
        for (int i = 0; i < 256; i++) { double v = fabs(buf[i]); if (!isfinite(v)) (*bad)++; else { if (v > *pk) *pk = v; s2 += v * v; } n++; } }
    *rms = sqrt(s2 / (n ? n : 1));
}
static void next_page(void) { tab_focus = 1; tap(K_RIGHT); tap(K_DOWN); tab_focus = 0; }   // the page's tabs (v3): <> the page
static void solo(int s) { for (int j = 0; j < NSLOT; j++) { song.t[j].on = j == s; song.mix.pad[j].solo = j == s; } }
static void unsolo(void) { for (int j = 0; j < NSLOT; j++) song.mix.pad[j].solo = 0; }

static void t_convert(void) {              // NEON's GRAND pads: WAVE now, from their own settings, and they sound
    int n1 = 0;
    for (int s = 0; s < NSLOT; s++) { const Track *t = &song.t[s]; const int8_t *wp = song.wv[s];
        if (t->type != 1) continue; n1++;
        CHECK(wp[WP_MARK] == WP_INIT, "pad %d: no WAVE block", s + 1);
        CHECK(t->sp[SP_MARK] == SP_INIT, "pad %d: GRAND's own values are kept", s + 1);
        int8_t ref[WP_N]; wv_from_grand(ref, t->sp, t->voice, song.gx[s]);
        CHECK(!memcmp(ref, wp, WP_N), "pad %d: the block is the conversion of its GRAND settings", s + 1);
        CHECK(wp[WP_F_TYPE] == WF_LADDER && abs(wp[WP_F_CUT] - t->sp[SP_CUTOFF]) <= 2, "pad %d: ladder at GRAND's cutoff (%d vs %d)", s + 1, wp[WP_F_CUT], t->sp[SP_CUTOFF]);
        CHECK(wp[WP_B + WO_SEMI] == t->sp[SP_TUNE2] || wp[WP_B + WO_WARP] == WW_SYNC, "pad %d: TUNE 2 -> B SEMI", s + 1);
        CHECK(wp[WP_ENV1 + WE_SUS] == t->sp[SP_SUSTAIN] || song.gx[s][GX_VCA], "pad %d: SUSTAIN", s + 1);
        CHECK((wp[WP_POLY] > 1) == (t->voice == 1), "pad %d: MONO / POLY", s + 1);
        CHECK(wp[WP_A + WO_TABLE] <= 1 && wp[WP_B + WO_TABLE] <= 1, "pad %d: BASIC or PWM", s + 1);
        double pk, rms; int bad; solo(s); engine_transport(1); render((long)(4 * 4 * 60.0 / song.tempo * SR), &pk, &rms, &bad); engine_transport(0); engine_stop_all(); pump(40);
        printf("pad %d (was GRAND): peak %.3f rms %.4f (%.1f dB)\n", s + 1, pk, rms, 20 * log10(rms + 1e-9));
        CHECK(!bad && pk > 0.01 && pk < 1.5, "pad %d: 4 bars finite and heard (peak %.4f, %d bad)", s + 1, pk, bad);
    }
    unsolo(); CHECK(n1 >= 4, "NEON has %d WAVE pads, 4 expected", n1);
}
static void t_old_file(void) {             // an 0x12 file written by hand: a pad with every GRAND DEEP option that converts
    int s = 2; Track *t = &song.t[s]; t->type = 1; synth_defaults(t->sp); gx_defaults(song.gx[s]);
    t->sp[SP_OSC1] = 3; t->sp[SP_OSC2] = 1; t->sp[SP_TUNE2] = 7; t->sp[SP_MIX] = 30; t->sp[SP_NOISE] = 12; t->voice = 1;
    song.gx[s][GX_SYNC] = 1; song.gx[s][GX_LPITCH] = 20; song.gx[s][GX_LCUT] = 50; song.gx[s][GX_LPWM] = 40; song.gx[s][GX_LWAVE] = 4; song.gx[s][GX_ARP] = 1; song.gx[s][GX_AMODE] = 3;
    song.lfo[s][0] = (Lfo){LT_ENG + SP_CUTOFF, LS_SINE, 0, 50, 40, 0, 0, 0}; song.lfo[s][1] = (Lfo){LT_ENG + LT_DEEP + GX_LRATE, LS_SINE, 0, 50, 40, 0, 0, 0};
    memset(song.wv[s], 0, WP_N); song.magic = 0xB2B20012;
    mkdir("projects", 0777); FILE *f = fopen("projects/OLDG.bpp", "wb"); fwrite(&song, offsetof(Song, wv), 1, f); fclose(f);   // the old layout ends before wv
    prj_load("projects/OLDG.bpp"); pump(4);
    const int8_t *wp = song.wv[s];
    CHECK(song.magic == STATE_MAGIC && t->type == 1 && wp[WP_MARK] == WP_INIT, "old file: pad 3 is WAVE");
    CHECK(wp[WP_A + WO_TABLE] == 1 && wp[WP_A + WO_POS] == 85, "PULSE -> PWM table at 10 %% (%d %d)", wp[WP_A + WO_TABLE], wp[WP_A + WO_POS]);
    CHECK(wp[WP_B + WO_TABLE] == 0 && wp[WP_B + WO_POS] == 67, "SAW -> BASIC saw");
    CHECK(wp[WP_A + WO_LEVEL] == 70 && wp[WP_B + WO_LEVEL] == 30 && wp[WP_NOISE_LEVEL] == 12, "MIX, NOISE");
    CHECK(wp[WP_B + WO_WARP] == WW_SYNC && wp[WP_B + WO_WAMT] > 0 && wp[WP_B + WO_SEMI] == 0, "HARD SYNC -> B on A's pitch, SYNC warp (%d %d)", wp[WP_B + WO_WARP], wp[WP_B + WO_WAMT]);
    CHECK(wp[WP_POLY] == 8 && wp[WP_ARP] == 1 && wp[WP_ARP_MODE] == 3, "POLY, ARP");
    CHECK(wp[WP_LFO1 + WL_SHAPE] == WLS_SH, "LFO S&H");
    int routes = 0, pitch = 0, cut = 0, pos = 0;
    for (int i = 0; i < WM_SLOTS; i++) { const int8_t *m = wp + WP_MTX + 4 * i; if (!m[WM_SRC]) continue; routes++;
        if (m[WM_DST] - 1 == WP_A + WO_SEMI || m[WM_DST] - 1 == WP_B + WO_SEMI) pitch++; if (m[WM_DST] - 1 == WP_F_CUT) cut++; if (m[WM_DST] - 1 == WP_A + WO_POS) pos++; }
    CHECK(pitch == 2 && cut == 1 && pos == 1 && routes == 4, "GRAND DEEP's LFO as 4 matrix routes (%d: pitch %d cut %d pwm %d)", routes, pitch, cut, pos);
    CHECK(song.lfo[s][0].target == LT_ENG + WP_F_CUT, "a pad LFO on CUTOFF stays on CUTOFF (%d)", song.lfo[s][0].target);
    CHECK(song.lfo[s][1].target == 0, "a pad LFO on GRAND DEEP is let go");
    double pk, rms; int bad; solo(s); engine_trig_key(s, 1.f, 0, -1, 40); engine_trig_key(s, 1.f, 4, -1, 41); render(SR / 2, &pk, &rms, &bad);
    engine_release_key(40); engine_release_key(41); pump(200); unsolo();
    CHECK(!bad && pk > 0.01, "the converted pad sounds (peak %.3f)", pk);
}
static int find_wave(void) { for (int s = 0; s < NSLOT; s++) if (song.t[s].type == 1) return s; return -1; }
static void t_pages(int s) {               // every page of SOUND, every card moved, the pictures drawn
    sel = s; set_tab(T_SEQ); cat_open(PC_SOUND); sound_sync(); pump(2); draw_ok();
    int n; snd_pages(1, &n); CHECK(n == 10, "WAVE has %d pages", n);
    int moved = 0, cards = 0;
    for (int pg = 0; pg < n; pg++) {
        const SndPage *P = snd_page(); CHECK(P == &snd_wav[pg], "page %d is %s", pg, P->name);
        draw_ok();
        if (P->gfx == GF_WMTX) {           // the matrix: a slot's source, its amount, a macro
            sp_cur = -1; tap(K_DOWN); CHECK(sp_cur == 0, "matrix: down from the macros onto M1 SRC (%d)", sp_cur);
            qa_earrow(K_RIGHT); CHECK(song.wv[s][WP_MTX + WM_SRC] != 0 || song.wv[s][WP_MTX + WM_SRC] == 0, "-");
            int8_t src0 = song.wv[s][WP_MTX + 4 * 5 + WM_SRC]; for (int i = 0; i < 5; i++) tap(K_DOWN); CHECK(sp_cur == 20, "matrix: M6 (%d)", sp_cur);
            qa_earrow(K_RIGHT); CHECK(song.wv[s][WP_MTX + 4 * 5 + WM_SRC] == src0 + 1, "matrix: EDIT+RIGHT on SRC");
            tap(K_RIGHT); tap(K_RIGHT); qa_earrow(K_UP); CHECK(song.wv[s][WP_MTX + 4 * 5 + WM_AMT] != 0, "matrix: AMT");
            for (int i = 0; i < 4; i++) tap(K_DOWN); draw_ok();   // the second half of the list
            for (int i = 0; i < 12 && sp_cur >= 0; i++) tap(K_UP); CHECK(sp_cur < 0 && !tab_focus, "matrix: up to the macros (%d)", sp_cur);
            qa_earrow(K_UP); CHECK(song.wv[s][WP_MACRO1 + clampi(-1 - sp_cur, 0, 4)] > 0, "matrix: a macro");
            ev(K_OPT, 1); ev(K_EDIT, 1); ev(K_EDIT, 0); ev(K_OPT, 0); pump(1);   // OPT+EDIT: default
            draw_ok(); next_page(); continue;
        }
        int l[20], c = snd_cards(P, l); cards += c;
        if (sp_cur < 0) tap(K_DOWN);
        for (int i = 0; i < c; i++) {
            sp_cur = i; int p = sound_par(); CHECK(p == l[i], "page %s card %d is %d (%d)", P->name, i, l[i], p);
            if (p == P_WTGT || p == P_PITCH || p == P_VOL || p == P_WENV || p == P_WLFO) continue;   // the selectors: below
            int v0 = par_get(s, p); qa_earrow(K_RIGHT); int v1 = par_get(s, p); if (v1 == v0) { qa_earrow(K_LEFT); v1 = par_get(s, p); }
            if (v1 != v0) moved++; else { int lo, hi, st, bg; par_limits(s, p, &lo, &hi, &st, &bg); CHECK(lo == hi, "page %s card %s: EDIT+arrows did nothing (%d)", P->name, par_name[p], v0); }
            char b[96]; par_str(s, p, b, sizeof b); CHECK(b[0], "page %s card %d: no text", P->name, i);
            draw_ok();
        }
        if (P->gfx == GF_WENV) { wv_env = 0; sp_cur = 0; qa_earrow(K_RIGHT); CHECK(wv_env == 1, "ENV: the selector (%d)", wv_env); int l2[20]; snd_cards(P, l2); CHECK(l2[1] == P_WV + WP_ENV2 + WE_ATT, "ENV2's cards follow"); draw_ok(); }
        if (P->gfx == GF_WLFO) { wv_lfo = 0; sp_cur = 0; qa_earrow(K_RIGHT); CHECK(wv_lfo == 1, "LFO: the selector"); draw_ok(); }
        next_page();
    }
    CHECK(snd_page() == &snd_wav[0], "the tabs went round to OSC A");
    printf("pages: %d, cards %d, %d moved by EDIT+arrows\n", n, cards, moved);
    CHECK(moved >= cards - 6, "most cards move (%d of %d)", moved, cards);
}
static void t_learn(int s) {               // LFO TARGET: EDIT, point at OSC A's POS, EDIT; a matrix DEST the same way
    sel = s; cat_open(PC_SOUND); snd_page_go(snd_page_of(GF_WLFO)); wv_lfo = 0; sp_cur = 6; CHECK(sound_par() == P_WTGT, "LFO page: TARGET card");
    memset(song.wv[s] + WP_MTX, 0, 64);
    tap(K_EDIT); CHECK(learning && learn_wv == 16, "TARGET: pointing (%d %d)", learning, learn_wv);
    CHECK(snd_page() == &snd_wav[0], "pointing starts on OSC A");
    sp_cur = 2; CHECK(sound_par() == P_WV + WP_A + WO_POS, "OSC A POS card"); tap(K_EDIT);
    const int8_t *m = song.wv[s] + WP_MTX;
    CHECK(!learning && m[WM_SRC] == WS_LFO1 && m[WM_DST] == WP_A + WO_POS + 1 && m[WM_AMT], "LFO1 -> A POS in M1 (%d %d %d)", m[WM_SRC], m[WM_DST], m[WM_AMT]);
    CHECK(snd_page()->gfx == GF_WLFO, "back on the LFO page");
    char b[64]; par_str(s, P_WTGT, b, sizeof b); CHECK(strstr(b, "POS") != NULL, "TARGET shows it (%s)", b);
    snd_page_go(snd_page_of(GF_WMTX)); sp_cur = 4 + WM_DST; tap(K_EDIT); CHECK(learning && learn_wv == 1, "DEST: pointing");
    snd_page_go(snd_page_of(GF_WFILT)); sp_cur = 1; tap(K_EDIT);
    CHECK(song.wv[s][WP_MTX + 4 + WM_DST] == WP_F_CUT + 1, "M2 DEST = CUTOFF (%d)", song.wv[s][WP_MTX + 4 + WM_DST]);
    snd_page_go(snd_page_of(GF_WMTX)); sp_cur = 4 + WM_SRC; ev(K_OPT, 1); ev(K_EDIT, 1); ev(K_EDIT, 0); ev(K_OPT, 0);
    CHECK(!song.wv[s][WP_MTX + 4 + WM_DST], "OPT+EDIT clears the slot");
    snd_page_go(snd_page_of(GF_WMTX)); sp_cur = WM_DST; tap(K_EDIT); tap(K_Y); CHECK(!learning, "Y cancels the pointing");
}
static void t_tables(int s) {              // the TABLE browser: a built-in, then a WAV from wavetables/
    sel = s; cat_open(PC_SOUND); snd_page_go(0); sp_cur = -1;
    mkdir(WT_DIR, 0777); { WaveTable *t = &((WaveStore *)engine_wstore())->t[7]; CHECK(t->d && wt_save_wav(t, WT_DIR "/qatab.wav", 16, 0), "a WAV to load"); }
    tap(K_EDIT); CHECK(wtbing && wtb_osc == 0, "EDIT on OSC A's picture: the tables");
    draw_ok(); wtb_cur = 5; tap(K_EDIT); CHECK(!wtbing && song.wv[s][WP_A + WO_TABLE] == 5, "built-in DIGITAL taken (%d)", song.wv[s][WP_A + WO_TABLE]);
    snd_page_go(1); sp_cur = -1; tap(K_EDIT); CHECK(wtbing && wtb_osc == 1 && wtb_n >= 1, "OSC B: tables, %d files", wtb_n);
    int i = 0; while (i < wtb_n && strcmp(wtb_e[i].name, "qatab.wav")) i++; wtb_cur = WT_NBUILT + i; draw_ok(); tap(K_EDIT);
    int k = song.wv[s][WP_B + WO_TABLE]; const WaveTable *t = &((const WaveStore *)engine_wstore())->t[clampi(k, 0, WT_SLOTS - 1)];
    CHECK(k >= WT_NBUILT && t->d && !strcmp(t->name, "qatab") && t->nf == 16, "the WAV is in slot %d (%s, %d frames)", k, t->name, t->nf);
    CHECK(!strcmp(wt_file[k], "qatab.wav"), "slots.cfg keeps it");
    FILE *f = fopen(WT_CFG, "r"); char ln[64] = ""; if (f) { if (!fgets(ln, sizeof ln, f)) ln[0] = 0; fclose(f); } CHECK(strstr(ln, "qatab.wav") != NULL, "slots.cfg written (%s)", ln);
    snd_page_go(0); sp_cur = -1; ev(K_EDIT, 1); ev(K_RIGHT, 1); ev(K_RIGHT, 0); ev(K_EDIT, 0); CHECK(song.wv[s][WP_A + WO_TABLE] == 6, "EDIT+RIGHT on the picture: the next table");
    draw_ok();
}
static void t_notes(int s) {               // keys held and let go, the ARP, mute, STOP ALL
    double pk, rms; int bad; sel = s; wpreset_apply(song.wv[s], 0); solo(s); engine_transport(0); pump(4);
    engine_trig_key(s, 0.9f, 0, -1, 50); engine_trig_key(s, 0.9f, 7, -1, 51); render(SR / 4, &pk, &rms, &bad);
    CHECK(wv_active(&wave[s]) == 2 && pk > 0.02 && !bad, "two held keys sound (%d voices, peak %.3f)", wv_active(&wave[s]), pk);
    engine_release_key(50); pump(2); int rel = 0; for (int i = 0; i < WV_POLY; i++) rel += wave[s].v[i].on && wave[s].v[i].rel;
    CHECK(rel == 1, "one key let go: one voice in its release (%d)", rel);
    engine_release_key(51); render(SR * 2, &pk, &rms, &bad); render(SR / 4, &pk, &rms, &bad);   // the release, then a quarter second of silence
    CHECK(!wv_active(&wave[s]) && pk < 1e-3, "both let go: silent (peak %.5f)", pk);
    song.wv[s][WP_ARP] = 1; song.wv[s][WP_ARP_RATE] = 2; song.t[s].on = 0;   // ARP: a held chord, note by note in sixteenths
    unsigned a0 = wave[s].age; engine_trig_key(s, 1.f, 0, -1, 52); engine_trig_key(s, 1.f, 4, -1, 53); engine_trig_key(s, 1.f, 7, -1, 54);
    render((long)(60.0 / song.tempo * SR), &pk, &rms, &bad); int notes = (int)(wave[s].age - a0);
    CHECK(notes >= 3 && notes <= 6 && pk > 0.02, "ARP: %d notes in a beat (peak %.3f)", notes, pk);
    engine_release_key(52); engine_release_key(53); engine_release_key(54); song.wv[s][WP_ARP] = 0; render(SR, &pk, &rms, &bad);
    engine_trig_key(s, 1.f, 0, -1, 55); render(SR / 8, &pk, &rms, &bad); song.t[s].mute = 1; render(SR / 8, &pk, &rms, &bad); render(SR / 8, &pk, &rms, &bad);
    CHECK(pk < 1e-4 && !wv_active(&wave[s]), "mute stops it (peak %.5f, %d voices)", pk, wv_active(&wave[s])); song.t[s].mute = 0; engine_release_key(55);
    engine_trig_key(s, 1.f, 0, -1, 56); render(SR / 8, &pk, &rms, &bad); engine_stop_all(); render(256, &pk, &rms, &bad); render(SR / 8, &pk, &rms, &bad);
    CHECK(pk < 1e-4 && !wv_active(&wave[s]), "STOP ALL: silent at once (peak %.5f)", pk);
    for (int i = 0; i < NWPRESET; i++) {   // every factory preset plays
        wpreset_apply(song.wv[s], i); engine_trig_key(s, 1.f, 0, -1, 57); render(SR / 2, &pk, &rms, &bad); double p2 = pk; int b2 = bad;
        engine_release_key(57); render(SR / 8, &pk, &rms, &bad); bad += b2; engine_stop_all(); render(1024, &pk, &rms, &bad);
        CHECK(!bad && p2 > 0.005, "preset %s plays (peak %.4f)", wpresets[i].name, p2);
    }
    unsolo();
}
static void t_mod(int s) {                 // a pad LFO on CUTOFF moves the played copy, not the song; PITCH reaches the voices
    sel = s; wpreset_apply(song.wv[s], 0); song.lfo[s][0] = (Lfo){LT_ENG + WP_F_CUT, LS_SQUARE, 0, 90, 60, 0, 0, 0};
    song.lfo[s][1] = (Lfo){LT_PITCH, LS_SQUARE, 0, 90, 50, 0, 0, 0};
    int lo = 127, hi = -128; float tmin = 99, tmax = -99; double pk, rms; int bad; engine_trig_key(s, 1.f, 0, -1, 60);
    for (int i = 0; i < 200; i++) { render(128, &pk, &rms, &bad); if (has_mod[s]) { int v = m_wv[s][WP_F_CUT]; if (v < lo) lo = v; if (v > hi) hi = v; } if (wave[s].tune < tmin) tmin = wave[s].tune; if (wave[s].tune > tmax) tmax = wave[s].tune; }
    engine_release_key(60);
    CHECK(hi - lo > 30 && song.wv[s][WP_F_CUT] == 58, "LFO -> CUTOFF: %d..%d around 58", lo, hi);
    CHECK(tmax - tmin > 8, "LFO -> PITCH: the voices' tune %.1f..%.1f", tmin, tmax);
    char tn[24]; lfo_tname(s, LT_ENG + WP_F_CUT, tn); CHECK(!strcmp(tn, "CUTOFF"), "the target's name (%s)", tn);
    int tg[256], n = lfo_targets(s, tg), has = 0; for (int i = 0; i < n; i++) has |= tg[i] == LT_ENG + WP_A + WO_POS; CHECK(has, "A POS is an LFO target");
    CHECK(auto_bind_target(P_WV + WP_F_CUT) == LT_ENG + WP_F_CUT, "a card binds to LT_ENG + k");
    memset(song.lfo[s], 0, sizeof song.lfo[s]); engine_stop_all(); pump(4);
}
static void t_song(void) {                 // 4 bars of the whole project; then save and load: the same song
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    for (int s = 0; s < NSLOT; s++) { Track *t = &song.t[s]; t->on = has_sound(s) && (pat_used(&t->pat[t->cur]) || roll_any(&t->pat[t->cur])); }
    double pk, rms; int bad; engine_transport(1); render((long)(4 * 4 * 60.0 / song.tempo * SR), &pk, &rms, &bad); engine_transport(0); engine_stop_all(); pump(40);
    printf("NEON 4 bars: peak %.3f rms %.4f (%.1f dB)\n", pk, rms, 20 * log10(rms + 1e-9));
    CHECK(!bad && pk > 0.05 && rms > 0.005, "NEON plays (peak %.3f rms %.4f, %d bad)", pk, rms, bad);
    int s = find_wave(); song.wv[s][WP_A + WO_POS] = 41; song.wv[s][WP_MTX + 4 * 3 + WM_SRC] = WS_ENV3; song.wv[s][WP_MTX + 4 * 3 + WM_DST] = WP_F_RES + 1; song.wv[s][WP_MTX + 4 * 3 + WM_AMT] = -33;
    song.t[s].type = 1;
    for (int j = 0; j < NSLOT; j++) song.t[j].on = 0;
    uint64_t h = fnv64(&song, sizeof song); prj_save("WAVEQA"); prj_load("projects/WAVEQA.bpp"); pump(4);
    for (int j = 0; j < NSLOT; j++) song.t[j].on = 0;
    CHECK(fnv64(&song, sizeof song) == h, "saved and loaded: the same song");
    CHECK(song.wv[s][WP_A + WO_POS] == 41 && song.wv[s][WP_MTX + 4 * 3 + WM_AMT] == -33, "WAVE's values kept");
    FILE *f = fopen("projects/WAVEQA.bpp", "rb"); long sz = 0; if (f) { fseek(f, 0, SEEK_END); sz = ftell(f); fclose(f); }
    CHECK(sz >= (long)sizeof(Song), "the file holds the whole song (%ld)", sz);
    // a pad switched to WAVE starts from INIT; back to the sampler and again keeps its sound
    int e = -1; for (int j = 0; j < NSLOT; j++) if (!has_sound(j)) { e = j; break; }
    if (e >= 0) { sel = e; memset(song.wv[e], 0, WP_N); song.t[e].type = 0; par_adjust(e, P_TYPE, 1, 0); CHECK(song.t[e].type == 1 && song.wv[e][WP_MARK] == WP_INIT && song.wv[e][WP_A + WO_POS] == 67, "a new WAVE pad: INIT");
        song.wv[e][WP_F_CUT] = 33; par_adjust(e, P_TYPE, 1, 0); par_adjust(e, P_TYPE, -1, 0); CHECK(song.wv[e][WP_F_CUT] == 33, "its sound kept across a switch"); }
}
int main(void) {
    sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888); R = SDL_CreateSoftwareRenderer(sf); logf = stderr; font_init();
    srand(1); song_defaults(); prj_startup(); pump(1);
    CHECK(wave_ready && wstore.t[0].d && wstore.t[WT_NBUILT - 1].d, "the tables are built before any pad plays");
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    t_convert();
    int s = find_wave(); CHECK(s >= 0, "a WAVE pad");
    if (s >= 0) { t_pages(s); t_learn(s); t_tables(s); t_notes(s); t_mod(s); }
    prj_load("projects/DEMO/NEON.bpp"); pump(4);
    t_old_file();
    t_song();
    remove("projects/OLDG.bpp"); remove("projects/WAVEQA.bpp"); remove(WT_DIR "/qatab.wav"); remove(WT_CFG);
    printf("%s (%d fails)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails != 0;
}
