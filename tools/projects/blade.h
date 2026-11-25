// BLOOD RAVE (2026-11-25, user: "simple techno in the spirit of Blade": the film's club scene, late-90s driving techno).
// 130 BPM, E minor, few pads, 8 scenes of 8 bars. An original line in the style (four on the floor, a rolling off-beat
// bass, an acid squelch, minor stabs, open hats), not a copy of any record.
#define PRJ_NAME "BLOOD RAVE"
#define HT SM "HPERTRANCE/"
enum { KICK, CLAP, HATS, OPEN, RIDE, BASS, ACID, STAB, PAD, FX, E10, E11 };
static const char *const b_path[NSLOT] = {
    SM "HARD TECHNO/TKNVLT - Free Melodic Techno Kicks/Melodic Techno Kicks/TKNVLT_FREE_MELODIC_TECHNO_KICK_10.wav",
    HT "04 HTS Claps/HTS Claps/HTS Claps 04.wav", HT "03 HTS Cymbals/HTS Closed Hats/HTS Closed Hats 05.wav",
    HT "03 HTS Cymbals/HTS Open Hats/HTS Open Hats 01.wav", HT "03 HTS Cymbals/HTS Rides/HTS 909 Ride 01.wav",
    NULL, NULL, NULL, NULL, SM "BREAKCORE/SFX/impact.wav", NULL, NULL};
static const char *const b_name[NSLOT] = {"KICK", "CLAP", "HATS", "OPEN HAT", "RIDE", "BASS", "ACID", "STAB", "PAD", "IMPACT", "", ""};
static const char *const scn_title[8] = {"INTRO", "BUILD", "DROP", "BREAK", "BUILD 2", "DROP 2", "PEAK", "OUTRO"};
static void prj_scene(int sc) {
    g_path = b_path; g_name = b_name;
    song_defaults();
    song.tempo = 130; song.key = 4; song.scale = 2; song.swing = 0; song.launch = 1; song.master = 92; song.beats = 4;
    song.mix.ms[MS_COMP] = 1; song.mix.ms[MS_THRESH] = 70; song.mix.ms[MS_RATIO] = 30; song.mix.ms[MS_ATTACK] = 35; song.mix.ms[MS_RELEASE] = 30; song.mix.ms[MS_MAKEUP] = 50;
    song.mix.ms[MS_LIM] = 1; song.mix.ms[MS_CEIL] = 95;
    const int8_t rv[8] = {65, 55, 45, 20, 100, 30, 70, 0}, dl[8] = {5, 45, 55, 30, 1, 100, 60, 0};
    memcpy(song.mix.rv, rv, 8); memcpy(song.mix.dl, dl, 8);
    int drop = sc == 2 || sc == 5 || sc == 6;
    g_pad(KICK, 0, 80, 0); g_pad(CLAP, 0, 66, 0); g_pad(HATS, 0, 50, 8); g_pad(OPEN, 0, 56, -6); g_pad(RIDE, 0, 40, 12);
    g_pad(BASS, 2, 74, 0); g_preset(BASS, "LOW CRAWL");
    g_pad(ACID, 2, 60, 0); g_preset(ACID, "WET SQUELCH");
    g_pad(STAB, 1, 60, 0); g_preset(STAB, "BRASS");
    g_pad(PAD, 1, 54, 0); g_preset(PAD, "STRINGS");
    g_pad(FX, 0, 70, 0);
    { int8_t *a = song.t[BASS].ap; a[AP_CUTOFF] = 30; a[AP_RESO] = 40; a[AP_ENVMOD] = 35; a[AP_DECAY] = 45; }
    { int8_t *a = song.t[ACID].ap; a[AP_CUTOFF] = (int8_t)(int[]){15, 20, 35, 18, 25, 42, 50, 20}[sc]; a[AP_RESO] = 86; a[AP_DRIVE] = 55; }
    g_fx(KICK, 0, FX_EQ, (int[]){62, 46, 40, 50, 6, 100});
    g_fx(CLAP, 0, FX_EQ, (int[]){44, 56, 62, 66, 22, 100});
    g_fx(HATS, 0, FX_EQ, (int[]){40, 50, 60, 70, 35, 100});
    g_fx(BASS, 0, FX_DIST, (int[]){0, 30, 50, 55, 40, X});
    g_fx(ACID, 0, FX_DIST, (int[]){2, 45, 50, 50, 50, X});
    g_fx(ACID, 1, FX_DELAY, (int[]){7, 35, 55, 1, 30, 20});
    g_fx(STAB, 0, FX_EQ, (int[]){36, 54, 60, 64, 28, 100});
    g_fx(PAD, 0, FX_FILTER, (int[]){FILT_LP24, sc == 3 ? 70 : 50, 20, 0, 0, 0});
    song.mix.pad[CLAP].send_a = 25; song.mix.pad[STAB].send_a = 30; song.mix.pad[STAB].send_b = 30; song.mix.pad[PAD].send_a = 50; song.mix.pad[FX].send_a = 40; song.mix.pad[RIDE].send_a = 10;
    { static const int dk[5][3] = {{0, 0, 0}, {3, 8, 3}, {10, 36, -2}, {18, 56, 0}, {31, 63, 0}}; g_duck(BASS, KICK, 85, dk, 5); g_duck(PAD, KICK, 60, dk, 5); g_duck(STAB, KICK, 40, dk, 5); }
    g_lfo(ACID, 0, LT_ENG + AP_CUTOFF, LS_TRI, 1, 12, 16);
    g_lfo(HATS, 0, LT_VOL, LS_SH, 1, 2, 14);
    Pattern *k = g_pat(KICK, 128, 0), *c = g_pat(CLAP, 128, 0), *h = g_pat(HATS, 128, 0), *o = g_pat(OPEN, 128, 0), *ri = g_pat(RIDE, 128, 0);
    Pattern *bs = g_pat(BASS, 128, 0), *ac = g_pat(ACID, 128, 0), *st = g_pat(STAB, 128, 1), *pd = g_pat(PAD, 128, 1), *fx = g_pat(FX, 128, 0);
    static const int root[4] = {0, 0, -4, -2};                               // E, E, C, D: two bars each (semitones from E)
    static const int ch[4][3] = {{64, 67, 71}, {64, 67, 71}, {60, 64, 67}, {62, 66, 69}};   // Em Em C D
    for (int bar = 0; bar < 8; bar++) { int p = bar * 16, r = root[bar / 2];
        int kick = sc != 3 && !(sc == 0 && bar < 2) && !(sc == 7 && bar >= 6) && !(sc == 4 && bar < 4);
        if (kick) for (int q = 0; q < 4; q++) g_hit(k, p + 4 * q, q ? 116 : 124);
        if (sc == 4 && bar >= 6) for (int q = 0; q < 8; q++) g_hit(c, p + 2 * q, 70 + 4 * q);           // the clap roll
        if (drop || sc == 1 || sc == 7 || (sc == 4 && bar < 6)) { g_hit(c, p + 4, 104); g_hit(c, p + 12, 110); }
        if (sc != 3) for (int q = 0; q < 16; q += 2) g_hit(h, p + q + 1, q % 4 ? 70 : 90);
        if (drop || sc == 1 || sc == 7) for (int q = 0; q < 4; q++) g_hit(o, p + 4 * q + 2, 84);
        if (drop) for (int q = 0; q < 4; q++) g_hit(ri, p + 4 * q, 80);
        if (sc != 3 && !(sc == 0 && bar < 4)) for (int q = 0; q < 4; q++) {   // the rolling bass: three off-beat sixteenths each beat
            for (int j = 1; j < 4; j++) { int s = p + 4 * q + j; g_hit(bs, s, j == 2 ? 112 : 92); bs->note[s] = (int8_t)(r + (j == 3 && q == 3 ? 12 : 0)); bs->slice[s] = -1; } }
        if (drop || sc == 4 || sc == 1) {                                     // the acid: a short squelchy figure
            static const int at[7] = {0, 3, 6, 8, 10, 13, 14}, dg[7] = {0, 12, 0, 3, 7, 0, 10};
            for (int j = 0; j < 7; j++) { if (sc == 1 && j % 2) continue; int s = p + at[j]; g_hit(ac, s, j % 3 == 0 ? 120 : 86); ac->note[s] = (int8_t)(dg[j] + r);
                ac->slice[s] = (int8_t)(j == 1 || j == 5 ? ACID_SLIDE : j % 3 == 0 ? ACID_ACCENT : -1); }
        }
        if (in(sc, "2563") && bar % 2 == 1) for (int j = 0; j < 3; j++) { g_note(st, p + 6, 2, ch[bar / 2][j], 110); g_note(st, p + 10, 2, ch[bar / 2][j], 96); }   // minor stabs
        if (in(sc, "03467")) for (int j = 0; j < 3; j++) if (bar % 2 == 0) g_note(pd, p, 32, ch[bar / 2][j] - 12, 80);
        if (in(sc, "256") && bar == 0) g_hit(fx, p, 120);
    }
    if (sc == 1 || sc == 4) g_auto(ACID, 0, LT_ENG + AP_CUTOFF, (int[]){0, 12, 127, 55}, 2, (int[]){35, 0});
    if (sc == 0) g_auto(PAD, 0, LT_INS1 + 1, (int[]){0, 25, 127, 60}, 2, NULL);
    static const int col[NSLOT] = {1, 2, 3, 3, 3, 5, 6, 7, 8, 11, 0, 0};
    for (int s = 0; s < NSLOT; s++) track_set_icol(&song.t[s], col[s]);
}
