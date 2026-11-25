// ACID WRECK (2026-11-25, user: one project averaged from T78 & Roberto Molinaro "Bombacid" (139.5 BPM, a quiet intro,
// then the drop), Mike Klubnika "Bump 9" (135.5, broken industrial kicks, the sub in front) and usedcvnt "aesthetics of
// self-destruction" (152, loud, digital chaos)). 140 BPM, A minor; the measured average: rms about -6 dBFS, crest 10 dB,
// sub -3, bass -5, low-mid -11, mid -12.5, high -14. 8 scenes of 8 bars.
#define PRJ_NAME "ACID WRECK"
#define BC SM "BREAKCORE/"
#define HT SM "HPERTRANCE/"
enum { KICK, CLAP, HATS, OPEN, BREAK, INDUS, ACID, SUB, SAW, CLOUD, VOX, FX };
static const char *const w_path[NSLOT] = {
    SM "HARD TECHNO/TKNVLT - Free Techno Rumble Kicks/Rumble Kicks/TKNVLT_FREE_KICK_18_140.wav", HT "04 HTS Claps/HTS Claps/HTS Claps 04.wav",
    HT "03 HTS Cymbals/HTS Closed Hats/HTS Closed Hats 05.wav", HT "03 HTS Cymbals/HTS Open Hats/HTS Open Hats 01.wav",
    BC "Amen-Think-Breakbeat Loops/140bpm Break 8.wav", SM "HARD TECHNO/FREE HARD TECHNO SAMPLE PACK - KNALWERK/KICKS/KNALWERK_INDUSTRIAL_KICK_02.wav",
    NULL, NULL, NULL, BC "Textures/FX - Texture (Glitchy Time Synced).wav", BC "Vox/Bad Vox.wav", BC "SFX/impact.wav"};
static const char *const w_name[NSLOT] = {"KICK", "CLAP", "HATS", "OPEN HAT", "AMEN", "INDUSTRY", "ACID", "SUB", "SUPERSAW", "GLITCH", "VOX", "IMPACT"};
static const char *const scn_title[8] = {"INTRO", "BUILD", "DROP", "BREAK", "BUILD 2", "DROP 2", "CHAOS", "OUTRO"};
static void prj_scene(int sc) {
    g_path = w_path; g_name = w_name;
    song_defaults();
    song.tempo = 140; song.key = 9; song.scale = 2; song.swing = 0; song.launch = 1; song.master = 100; song.beats = 4;
    song.mix.ms[MS_COMP] = 1; song.mix.ms[MS_THRESH] = 70; song.mix.ms[MS_RATIO] = 32; song.mix.ms[MS_ATTACK] = 35; song.mix.ms[MS_RELEASE] = 30; song.mix.ms[MS_MAKEUP] = 62;   // a gentle glue, then the limiter makes it loud
    song.mix.ms[MS_LIM] = 1; song.mix.ms[MS_CEIL] = 96;
    const int8_t rv[8] = {60, 55, 45, 20, 100, 30, 70, 0}, dl[8] = {5, 50, 55, 30, 1, 100, 65, 0};   // a short room, a 1/8 dotted
    memcpy(song.mix.rv, rv, 8); memcpy(song.mix.dl, dl, 8);
    int drop = sc == 2 || sc == 5 || sc == 6, four = drop || sc == 0 || sc == 1 || sc == 7 || sc == 4;

    g_pad(KICK, 0, 76, 0); g_pad(CLAP, 0, 64, 0); g_pad(HATS, 0, 46, 8); g_pad(OPEN, 0, 52, -8);
    g_pad(BREAK, 0, sc == 3 ? 66 : 58, 0); g_pad(INDUS, 0, 64, 0);
    g_pad(ACID, 2, 70, 0); g_preset(ACID, sc == 6 ? "SCREAM VALVE" : "WAREHOUSE");
    g_pad(SUB, 1, 92, 0); g_preset(SUB, "SUB DROP");
    g_pad(SAW, 1, 58, 0); g_preset(SAW, "SUPERSAW");
    g_pad(CLOUD, 3, 100, 0); g_preset(CLOUD, sc == 6 ? "BUFFER SKIP" : "STUTTER 32");
    g_pad(VOX, 0, 62, 0); g_pad(FX, 0, 70, 0);
    song.t[BREAK].slices = 4;                                                 // 32 slices over its 2 bars: a slice a sixteenth
    { int8_t *a = song.t[ACID].ap; a[AP_CUTOFF] = (int8_t)(int[]){12, 22, 40, 20, 30, 46, 52, 24}[sc]; a[AP_RESO] = 84; a[AP_ENVMOD] = 62; a[AP_DECAY] = 38; a[AP_ACCENT] = 85; a[AP_DRIVE] = sc == 6 ? 80 : 55; }
    song.t[CLOUD].cp[CP_DENSITY] = 65; song.t[CLOUD].cp[CP_SPRAY] = 30;

    g_fx(KICK, 0, FX_EQ, (int[]){60, 46, 40, 50, 6, 100});
    g_fx(KICK, 1, FX_FILTER, (int[]){FILT_HP24, sc == 0 ? 50 : 0, 20, 0, 0, 0});   // the intro's kick starts thin (automation opens it)
    g_fx(CLAP, 0, FX_EQ, (int[]){44, 56, 62, 66, 22, 100});
    g_fx(HATS, 0, FX_EQ, (int[]){40, 50, 60, 70, 35, 100});
    g_fx(OPEN, 0, FX_EQ, (int[]){40, 50, 60, 66, 35, 100});
    g_fx(BREAK, 0, FX_CRUSH, (int[]){sc == 6 ? 40 : 65, sc == 6 ? 40 : 15, 10, 70, sc == 6 ? 70 : 35, X});   // usedcvnt: the digital grit
    g_fx(BREAK, 1, FX_EQ, (int[]){38, 56, 60, 64, 30, 100});
    g_fx(INDUS, 0, FX_DIST, (int[]){1, 55, 55, 45, 70, X});                  // Klubnika: hard, metallic
    g_fx(INDUS, 1, FX_FILTER, (int[]){FILT_COMB, 48, 40, 10, 0, 0});
    g_fx(ACID, 0, FX_DIST, (int[]){2, 50, 50, 50, 55, X});
    g_fx(ACID, 1, FX_DELAY, (int[]){7, 35, 55, 1, 30, sc == 3 ? 35 : 18});
    g_fx(SUB, 0, FX_EQ, (int[]){60, 44, 40, 40, 2, 30});
    g_fx(SAW, 0, FX_DIST, (int[]){0, 30, 60, 50, 40, X});
    g_fx(SAW, 1, FX_EQ, (int[]){36, 54, 60, 66, 28, 100});
    g_fx(CLOUD, 0, FX_FILTER, (int[]){FILT_HP24, 34, 20, 0, 0, 0});
    g_fx(VOX, 0, FX_COMP, (int[]){45, 50, 20, 40, 20, 100}); song.mbx[VOX][0][5] = KICK + 1;   // pumped by the kick
    g_fx(VOX, 1, FX_CRUSH, (int[]){60, 20, 10, 70, 40, X});
    g_fx(FX, 0, FX_REVERB, (int[]){80, 60, 40, 5, 25, 30});

    song.mix.pad[CLAP].send_a = 25; song.mix.pad[SAW].send_a = sc == 3 ? 55 : 30; song.mix.pad[SAW].send_b = 25; song.mix.pad[VOX].send_b = 40;
    song.mix.pad[CLOUD].send_a = 35; song.mix.pad[OPEN].send_b = 10; song.mix.pad[FX].send_a = 40; song.mix.pad[ACID].send_b = sc == 3 ? 30 : 12;
    { static const int dk[5][3] = {{0, 0, 0}, {3, 8, 3}, {10, 36, -2}, {18, 56, 0}, {31, 63, 0}}; g_duck(SUB, KICK, 95, dk, 5); g_duck(SAW, KICK, 70, dk, 5); g_duck(CLOUD, KICK, 55, dk, 5); g_duck(BREAK, KICK, 40, dk, 5); }
    g_lfo(ACID, 0, LT_ENG + AP_CUTOFF, LS_TRI, 1, 12, sc == 6 ? 25 : 14);    // 2 bars of breathing
    g_lfo(HATS, 0, LT_VOL, LS_SH, 1, 2, 16);
    g_lfo(CLOUD, 0, LT_ENG + CP_POS, LS_SAWUP, 1, 11, 40);
    g_lfo(SAW, 0, LT_PAN, LS_SINE, 1, 10, 18);

    Pattern *k = g_pat(KICK, 128, 0), *c = g_pat(CLAP, 128, 0), *h = g_pat(HATS, 128, 0), *o = g_pat(OPEN, 128, 0), *b = g_pat(BREAK, 128, 0);
    Pattern *in_ = g_pat(INDUS, 128, 0), *ac = g_pat(ACID, 128, 0), *sb = g_pat(SUB, 128, 1), *sw = g_pat(SAW, 128, 1), *cl = g_pat(CLOUD, 128, 1);
    Pattern *vx = g_pat(VOX, 128, 0), *fx = g_pat(FX, 128, 0);
    static const int root[4] = {45, 45, 41, 43};                              // A A F G, two bars each
    for (int bar = 0; bar < 8; bar++) { int p = bar * 16, r = root[bar / 2], last = bar == 7;
        if (four && !(sc == 4 && bar < 4) && !(sc == 7 && bar >= 6)) for (int q = 0; q < 4; q++) g_hit(k, p + 4 * q, q ? 116 : 124);
        if (sc == 4 && bar >= 6) for (int q = 0; q < 8; q++) g_hit(k, p + 2 * q, 100 + 2 * q);              // the build's kick doubles
        if ((drop || sc == 1 || (sc == 4 && bar < 4)) && !(sc == 1 && bar < 4)) { g_hit(c, p + 4, 104); g_hit(c, p + 12, 110); }
        if (sc != 3) for (int q = 0; q < 16; q++) { if (q % 4 == 2) g_hit(h, p + q, 96); else if (drop && q % 2) g_hit(h, p + q, 64); else if (drop || sc == 1) { g_hit(h, p + q, 44); h->prob[p + q] = 60; } }
        if (drop || sc == 7) g_hit(o, p + 2, 80), g_hit(o, p + 6, 76), g_hit(o, p + 10, 80), g_hit(o, p + 14, 76);
        if (drop && bar % 2) {                                                // Klubnika: the broken industrial kick between the beats
            static const int at[5] = {3, 6, 9, 11, 15}; for (int j = 0; j < 5; j++) g_hit(in_, p + at[j], 90 + (j % 2) * 20);
        } else if (drop) { g_hit(in_, p + 7, 100); g_hit(in_, p + 14, 90); }
        if (sc == 6) for (int q = 1; q < 16; q += 2) g_hit(in_, p + q, 70 + q);
        if (in(sc, "356") || (sc == 1 && bar >= 4) || (sc == 4 && bar < 6)) for (int q = 0; q < 16; q++) {   // the AMEN, chopped
            int sl = (bar % 2) * 16 + q;
            if (sc == 6 && bar % 2) sl = (int[]){0, 0, 4, 4, 8, 9, 8, 9, 12, 12, 12, 12, 14, 15, 14, 15}[q] + 16 * (q > 11);   // usedcvnt stutters
            if (sc == 4) sl = q % 4 == 0 ? 4 : 12;                           // the snare slices, a roll
            if (sc == 1 && q % 2) continue;
            g_hit(b, p + q, q % 4 == 0 ? 110 : 88); b->slice[p + q] = (int8_t)sl;
        }
        if (sc == 4 && bar >= 6) for (int q = 0; q < 16; q++) { g_hit(b, p + q, 70 + q * 3); b->slice[p + q] = 12; }
        if (sc != 3 || bar >= 4) {                                            // Bombacid: the sixteenth acid line, accents and slides
            static const int at[11] = {0, 2, 3, 5, 6, 8, 10, 11, 12, 14, 15}, dg[11] = {0, 0, 12, 0, 3, 0, 7, 0, 12, 10, 3};
            for (int j = 0; j < 11; j++) { if (sc == 0 && j % 2) continue; int s = p + at[j]; g_hit(ac, s, j % 3 == 0 ? 122 : 86); ac->note[s] = (int8_t)(dg[j] + (r - 45));
                ac->slice[s] = (int8_t)(j == 2 || j == 8 ? ACID_SLIDE : j % 3 == 0 ? ACID_ACCENT : -1); }
        }
        if (drop || sc == 7) for (int q = 0; q < 4; q++) g_note(sb, p + 4 * q + 2, 2, r - 24, 112);   // the sub: off-beat, under the kick's rumble
        if (in(sc, "35")) { static const int ch[4][3] = {{57, 60, 64}, {57, 60, 64}, {53, 57, 60}, {55, 59, 62}};
            if (sc == 3) for (int j = 0; j < 3; j++) g_note(sw, p, 16, ch[bar / 2][j], 90);
            else for (int q = 0; q < 3; q++) for (int j = 0; j < 3; j++) g_note(sw, p + (int[]){0, 3, 6}[q] + (bar % 2) * 8, 2, ch[bar / 2][j] + 12, 100); }
        if (in(sc, "0367") && bar == 0) g_note(cl, p, 128, 69, 90);           // the texture: one 8-bar note
        if (sc == 3 && bar % 4 == 0) g_hit(vx, p, 100);
        if (sc == 5 && bar % 4 == 2) g_hit(vx, p + 8, 96);
        if (in(sc, "256") && bar == 0) g_hit(fx, p, 120);
        if (sc == 3 && last) g_hit(fx, p + 12, 90);
    }
    if (sc == 0) g_auto(KICK, 0, LT_INS2 + 1, (int[]){0, 50, 120, 0}, 2, (int[]){30, 0});          // the intro's kick fills out
    if (sc == 1 || sc == 4) g_auto(ACID, 0, LT_ENG + AP_CUTOFF, (int[]){0, 18, 127, 60}, 2, (int[]){35, 0});
    if (sc == 7) g_auto(ACID, 0, LT_VOL, (int[]){0, 70, 127, 0}, 2, (int[]){50, 0});
    if (sc == 4) g_auto(CLOUD, 0, LT_INS1 + 1, (int[]){0, 30, 127, 90}, 2, NULL);
    static const int col[NSLOT] = {1, 2, 3, 3, 4, 4, 6, 5, 7, 9, 10, 11};
    for (int s = 0; s < NSLOT; s++) track_set_icol(&song.t[s], col[s]);
}
