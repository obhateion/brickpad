// Reference projects (2026-11-25, user: LORN alone, the other references as one project). A harness like the qa ones
// (engine.c + main.c): build the project (PRJ: the header that describes it), write it as prj_save does, open it in
// BrickPad, then play its 8 scenes one after the other into OUT.wav (16-bit stereo) and print each pad's peak.
// Usage, in the BrickPad folder copy with samples/: gen OUT.wav
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
#define SM "/userdata/roms/ports/brickpad/samples/"
#define X (-128)
static const char *const *g_path, *const *g_name;
static void g_pad(int s, int type, int vol, int pan) {
    Track *t = &song.t[s]; t->type = (int8_t)type; t->vol = (int8_t)vol; t->pan = (int8_t)pan;
    snprintf(song.pname[s], sizeof song.pname[s], "%s", g_name[s]);
    if (g_path[s]) snprintf(t->path, sizeof t->path, "%s", g_path[s]);
    if (type == 2) acid_defaults(t->ap);
    if (type == 3) cl_defaults(t->cp);
}
static void g_preset(int s, const char *nm) {   // ACID / CLOUD factory preset, or WAVE's
    if (song.t[s].type == 1) { for (int i = 0; i < NWPRESET; i++) if (!strcmp(wpresets[i].name, nm)) { wpreset_apply(song.wv[s], i); song.wv[s][WP_MARK] = WP_INIT; song.t[s].voice = song.wv[s][WP_POLY] > 1; return; } }
    else for (int i = 0; i < NPRESET; i++) if (presets[i].engine == song.t[s].type && !strcmp(presets[i].name, nm)) { preset_apply(s, presets[i].v); return; }
    fprintf(stderr, "no preset %s\n", nm); exit(3);
}
static Pattern *g_pat(int s, int len, int mode) { Pattern *p = &song.t[s].pat[0]; pattern_clear(p); p->len = (uint8_t)len; p->div = 1; p->mode = (uint8_t)mode; return p; }
static void g_hit(Pattern *p, int st, int vel) { if (st < 0 || st >= NSTEP) return; p->trig[st] = 1; p->vel[st] = (int8_t)vel; p->prob[st] = 100; }
static void g_note(Pattern *p, double st16, double len16, int note, int vel) { roll_add(p, (int)lrint(st16 * 2 * RT), (int)lrint(len16 * 2 * RT), note, vel); }   // in 1/16 steps
static void g_fx(int s, int k, int type, const int *v) { FxSlot *f = &song.mix.pad[s].ins[k]; fx_set_type(f, type); if (v) for (int i = 0; i < 6; i++) if (v[i] >= -100) f->p[i] = (int8_t)v[i]; }
static void g_auto(int s, int lane, int target, const int *pts, int n, const int *bend) {   // pts: step, value pairs
    AutoLane *a = &song.aut[s][0][lane]; a->target = (uint8_t)target; a->smooth = 1; memset(a->v, AUTO_EMPTY, NSTEP);
    for (int i = 0; i < n; i++) { a->v[pts[2 * i]] = (int8_t)pts[2 * i + 1]; if (bend) song.acurve[s][0][lane][pts[2 * i]] = (int8_t)bend[i]; }
}
static void g_lfo(int s, int i, int target, int shape, int sync, int rate, int depth) { song.lfo[s][i] = (Lfo){(uint8_t)target, (uint8_t)shape, (uint8_t)sync, (uint8_t)rate, (int8_t)depth, 0, 0, 0}; }
static void g_duck(int s, int src, int amt, const int pts[][3], int n) {   // DUCK as points: x 0..31, level 0..63, bend -8..7
    song.mix.pad[s].spare[DK_SRC] = (int8_t)(src + 1); song.mix.pad[s].spare[DK_AMT] = (int8_t)amt; duck_init(s);
    int8_t *dx = song.dkx[s]; dx[DX_SPARE] = DK_PTS; dx[DX_SHAPE] = DK_USER; dx[DX_SMOOTH] = 20; memset(song.dkc[s], 0, 16);
    for (int i = 0; i < n; i++) dk_pt_put(song.dkc[s], i, (DkPt){1, pts[i][0], pts[i][1], pts[i][2]});
}
static int in(int sc, const char *who) { return strchr(who, '0' + sc) != NULL; }   // "0125": scenes 0, 1, 2, 5
#include PRJ
static Song g_data[8];
static int g_write(const char *file) {
    prj_scene(0);                        // (sets g_path and g_name)
    for (int i = 0; i < NSLOT; i++) if (g_path[i]) { Sample *x = sample_load(g_path[i]); if (!x || x->n < 2) { fprintf(stderr, "missing %s\n", g_path[i]); return 1; } sample_free(x); }
    for (int i = 0; i < 8; i++) { prj_scene(i); song_sanitize(); for (int s = 0; s < NSLOT; s++) { song.t[s].cur = 0; song.t[s].on = 0; } g_data[i] = song; }
    FILE *f = fopen(file, "wb"); if (!f) return 1;
    ScnHead h = {SCN_MAGIC, 8, 0, (int32_t)sizeof(Song)};
    int ok = fwrite(&g_data[0], sizeof(Song), 1, f) == 1 && fwrite(&h, sizeof h, 1, f) == 1 && fwrite(g_data, sizeof(Song), 8, f) == 8;
    return fclose(f) != 0 || !ok;
}
static void wav_head(FILE *f, uint32_t frames) {
    uint32_t db = frames * 4, v; uint16_t w;
    fwrite("RIFF", 1, 4, f); v = 36 + db; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    w = 1; fwrite(&w, 2, 1, f); w = 2; fwrite(&w, 2, 1, f); v = SR; fwrite(&v, 4, 1, f); v = SR * 4; fwrite(&v, 4, 1, f);
    w = 4; fwrite(&w, 2, 1, f); w = 16; fwrite(&w, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&db, 4, 1, f);
}
int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : "render.wav"; char file[96]; snprintf(file, sizeof file, "projects/%s.bpp", PRJ_NAME);
    srand(1); logf = stderr; song_defaults(); mkdir("projects", 0777);
    if (g_write(file)) { printf("FAILED to write %s\n", file); return 1; }
    prj_startup(); prj_load(file); printf("opened %s: %d scenes, %d BPM, magic %08x\n", file, scn_n, song.tempo, song.magic);
    if (scn_n != 8 || song.magic != STATE_MAGIC) return 1;
    if (getenv("SOLO")) {                // each pad of one scene alone, 4 bars, into solo_N.wav (to measure it)
        int sc = atoi(getenv("SOLO"));
        for (int k = 0; k < NSLOT; k++) {
            song = g_data[sc]; song_sanitize(); sync_samples(); if (!(has_sound(k) && pat_used(cur_pat(k)))) continue;
            for (int s = 0; s < NSLOT; s++) song.t[s].on = (int8_t)(s == k || s == 0); for (int s = 0; s < NSLOT; s++) song.mix.pad[s].solo = s == k;
            char fn[32]; snprintf(fn, sizeof fn, "solo_%02d.wav", k); FILE *w = fopen(fn, "wb"); wav_head(w, 0); uint32_t fr = 0; static float b2[256];
            engine_transport(1); long len = (long)(SR * 60.0 / song.tempo * 4 * 4);
            for (long f = 0; f < len; f += 128) { engine_render(b2, 128); engine_collect_garbage(); int16_t o[256]; for (int i = 0; i < 256; i++) o[i] = (int16_t)lrintf(fmaxf(-1.f, fminf(1.f, b2[i])) * 32767); fwrite(o, 2, 256, w); fr += 128; }
            engine_transport(0); engine_stop_all(); for (int i = 0; i < 200; i++) engine_render(b2, 128);
            fseek(w, 0, SEEK_SET); wav_head(w, fr); fclose(w); printf("solo %d %s\n", k, g_name[k]);
        }
        return 0;
    }
    FILE *w = fopen(out, "wb"); wav_head(w, 0); uint32_t frames = 0;
    int fails = 0; static float buf[2 * 128]; float m[NSLOT + 3];
    for (int sc = 0; sc < 8; sc++) {
        song = g_data[sc]; song_sanitize(); sync_samples();
        for (int s = 0; s < NSLOT; s++) song.t[s].on = (int8_t)(has_sound(s) && pat_used(cur_pat(s)));
        engine_transport(1); engine_meters(m);
        float pk[NSLOT + 3] = {0}, mpk = 0; double s2 = 0; int bad = 0, nb = 0;
        long len = (long)(SR * 60.0 / song.tempo * 4 * 8);
        for (long f = 0; f < len; f += 128) {
            engine_render(buf, 128); engine_collect_garbage(); nb++;
            int16_t o[256]; for (int i = 0; i < 256; i++) { float v = buf[i]; if (!isfinite(v)) { bad = 1; v = 0; } if (fabsf(v) > mpk) mpk = fabsf(v); s2 += v * v;
                o[i] = (int16_t)lrintf(fmaxf(-1.f, fminf(1.f, v)) * 32767); }
            fwrite(o, 2, 256, w); frames += 128;
            if (nb % 16 == 0) { engine_meters(m); for (int s = 0; s < NSLOT + 3; s++) if (m[s] > pk[s]) pk[s] = m[s]; }
        }
        engine_transport(0);
        printf("SCENE %d %-9s peak %.2f rms %.1f dB%s\n   pads:", sc + 1, scn_title[sc], mpk, 10 * log10(s2 / (len * 2.0) + 1e-12), bad ? "  NaN!" : "");
        for (int s = 0; s < NSLOT; s++) { int want = song.t[s].on; if (!want) continue; printf(" %s=%.2f", g_name[s], pk[s]); if (pk[s] < 0.003f) { printf("(SILENT!)"); fails++; } }
        printf("\n"); if (bad) fails++;
    }
    fseek(w, 0, SEEK_SET); wav_head(w, frames); fclose(w);
    printf(fails ? "FAILED (%d)\n" : "ALL OK (%d)\n", fails);
    return fails != 0;
}
