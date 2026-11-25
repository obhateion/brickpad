// LORN (2026-11-25, user: a project after Lorn's "Acid Rain", sped-up version; measured 110 BPM, dark, rms -11 dBFS,
// crest 13 dB, a thick low-middle). A minor, 8 scenes of 8 bars: INTRO, BUILD, DROP, BREAK, BUILD 2, DROP 2, PEAK, OUTRO.
// Half-time drums (snare on 3), a wobbling WAVE bass over a WAVE sub, the DCE "reality lost" kit (110, A minor),
// CLOUD on a slow A-minor atmosphere, ACID low and dark in the second half.
#define PRJ_NAME "LORN"
#define DCE SM "CINEMATIC/Dark Cinematic Electronica/"
#define KIT DCE "loops/music_kits/dce_110_music_kit_realitylost_Amin/dce_110_kit_loop_realitylost_"
enum { KICK, SNARE, HATS, BOOM, TEXT, SUB, WOB, LEAD, CHORD, CLOUD, ACID, ARP };
static const char *const l_path[NSLOT] = {
    SM "SKRILLEX DRUMS/FUS KICKS/KICK 5 (SLICKMAN).wav", SM "SKRILLEX DRUMS/FUS SNARES/SNARE 22 (BOOSTER).wav",
    SM "SKRILLEX DRUMS/FUS CYMBALS/HAT 13 (GULAB XX).wav", DCE "one_shots/drum_one_shots/dce_drum_one_shot_cinematic_boom.wav",
    KIT "sfx_1_Amin.wav", NULL, NULL, KIT "lead_1_Amin.wav", NULL,
    DCE "loops/atmospheres_&_drones/dce_70_synth_loop_atmosphere_fluid_Amin.wav", NULL, KIT "arp_1_Amin.wav"};
static const char *const l_name[NSLOT] = {"KICK", "SNARE", "HATS", "BOOM", "TEXTURE", "SUB", "WOBBLE", "LEAD", "CHORDS", "CLOUD", "ACID", "KIT"};
static const char *l_kit[8] = {KIT "arp_1_Amin.wav", KIT "arp_1_Amin.wav", KIT "bass_1_Amin.wav", KIT "arp_2_Amin.wav", KIT "arp_2_Amin.wav", KIT "bass_1_Amin.wav", KIT "bass_2_Amin.wav", KIT "arp_1_Amin.wav"};   // pad 12 plays the kit's arp or its bass, by scene
static const char *const scn_title[8] = {"INTRO", "BUILD", "DROP", "BREAK", "BUILD 2", "DROP 2", "PEAK", "OUTRO"};
static void prj_scene(int sc) {
    static const char *path[NSLOT]; memcpy(path, l_path, sizeof path); path[ARP] = l_kit[sc]; g_path = path; g_name = l_name;
    song_defaults();
    song.tempo = 110; song.key = 9; song.scale = 2; song.swing = 8; song.launch = 1; song.master = 86; song.beats = 4;
    song.mix.ms[MS_COMP] = 1; song.mix.ms[MS_THRESH] = 58; song.mix.ms[MS_RATIO] = 25; song.mix.ms[MS_ATTACK] = 35; song.mix.ms[MS_RELEASE] = 45; song.mix.ms[MS_MAKEUP] = 12;
    song.mix.ms[MS_LIM] = 1; song.mix.ms[MS_CEIL] = 90;
    const int8_t rv[8] = {85, sc == 3 || sc == 7 ? 88 : 70, 50, 15, 100, 25, 80, 0}, dl[8] = {7, 45, 50, 30, 1, 100, 60, 0};   // a dark hall, a dotted delay
    memcpy(song.mix.rv, rv, 8); memcpy(song.mix.dl, dl, 8);
    int drop = sc == 2 || sc == 5 || sc == 6, beat = drop || sc == 1 || sc == 4;

    g_pad(KICK, 0, 82, 0); g_pad(SNARE, 0, 82, 0); g_pad(HATS, 0, 88, 10); g_pad(BOOM, 0, 66, 0); g_pad(TEXT, 0, sc == 0 || sc == 7 ? 88 : 50, -15);
    g_pad(SUB, 1, 74, 0); g_preset(SUB, "SUB DROP");
    g_pad(WOB, 1, 88, 0); g_preset(WOB, "WOBBLE"); song.wv[WOB][WP_F_CUT] = 42; song.wv[WOB][WP_SUB_LEVEL] = 0; song.wv[WOB][WP_B + WO_OCT] = 0;   // the SUB pad holds the low end
    g_pad(LEAD, 0, 50, 12); g_pad(CHORD, 1, sc == 0 || sc == 7 ? 84 : 58, 0); g_preset(CHORD, "VOWEL PAD");
    g_pad(CLOUD, 3, sc == 0 || sc == 7 ? 96 : 74, 0); g_preset(CLOUD, sc == 3 ? "FROZEN MOMENT" : "BLACK TIDE");
    g_pad(ACID, 2, 52, 0); g_preset(ACID, "ACID RAIN"); g_pad(ARP, 0, in(sc, "256") ? 70 : 46, in(sc, "256") ? 0 : -12);
    { int8_t *a = song.t[ACID].ap; a[AP_CUTOFF] = (int8_t)(sc == 6 ? 34 : 22); a[AP_RESO] = 80; a[AP_DRIVE] = 60; }
    song.t[CLOUD].cp[CP_DENSITY] = 55; song.t[CLOUD].cp[CP_SIZE] = 70; song.t[CLOUD].cp[CP_SPRAY] = 35;
    song.smx[TEXT][SX_FIN] = 30; song.smx[LEAD][SX_FIN] = 10;

    g_fx(KICK, 0, FX_EQ, (int[]){66, 42, 36, 48, 4, 100});                  // weight up, boxiness down
    g_fx(KICK, 1, FX_DIST, (int[]){0, 30, 45, 55, 35, X});                  // a soft saturation
    g_fx(SNARE, 0, FX_EQ, (int[]){46, 62, 66, 82, 18, 100});
    g_fx(SNARE, 1, FX_TAPE, (int[]){45, 10, 8, 55, 40, 80});
    g_fx(HATS, 0, FX_CRUSH, (int[]){70, 10, 4, 90, 25, X});
    g_fx(HATS, 1, FX_EQ, (int[]){40, 56, 70, 76, 30, 100});
    g_fx(WOB, 0, FX_DIST, (int[]){0, sc == 6 ? 75 : 62, 68, 45, 80, X});   // Lorn's bass: driven
    g_fx(WOB, 1, FX_EQ, (int[]){62, 60, 40, 70, 12, 100});                    // no sub (the SUB pad has it), no fizz
    g_fx(SUB, 0, FX_EQ, (int[]){58, 44, 40, 40, 2, 30});
    g_fx(CHORD, 0, FX_EQ, (int[]){44, 52, 60, 66, 20, 100});   // (two inserts a pad: the brightness here, the filter after it)
    g_fx(CHORD, 1, FX_FILTER, (int[]){FILT_LP24, sc == 0 ? 40 : 72, 20, 0, 0, 0});
    g_fx(CLOUD, 0, FX_EQ, (int[]){44, 50, 50, 46, 14, 80});
    g_fx(ACID, 0, FX_DIST, (int[]){2, 40, 40, 45, 50, X});
    g_fx(ACID, 1, FX_DELAY, (int[]){7, 40, 45, 1, 25, 25});
    g_fx(LEAD, 0, FX_TAPE, (int[]){30, 25, 15, 70, 45, 100});
    g_fx(TEXT, 0, FX_FILTER, (int[]){FILT_HP24, 30, 20, 0, 0, 0});
    g_fx(BOOM, 0, FX_REVERB, (int[]){90, 70, 50, 5, 25, 35});

    song.mix.pad[SNARE].send_a = 45; song.mix.pad[CHORD].send_a = sc == 3 ? 60 : 35; song.mix.pad[LEAD].send_a = 35; song.mix.pad[LEAD].send_b = 25;
    song.mix.pad[CLOUD].send_a = 50; song.mix.pad[ARP].send_b = 35; song.mix.pad[TEXT].send_a = 40; song.mix.pad[HATS].send_b = 15;
    { static const int dk[5][3] = {{0, 2, 0}, {4, 12, 3}, {12, 40, -2}, {22, 58, 0}, {31, 63, 0}}; g_duck(SUB, KICK, 90, dk, 5); g_duck(WOB, KICK, 70, dk, 5); g_duck(ARP, KICK, 60, dk, 5); }
    { static const int dk[3][3] = {{0, 24, 0}, {14, 50, 2}, {31, 63, 0}}; g_duck(CHORD, KICK, 45, dk, 3); }
    g_lfo(WOB, 0, LT_ENG + WP_F_CUT, LS_SINE, 1, sc == 6 ? 3 : 6, 40);       // the wobble: 1/4T, 1/8T at the peak
    g_lfo(HATS, 0, LT_VOL, LS_SH, 1, 2, 20);
    g_lfo(CLOUD, 0, LT_ENG + CP_POS, LS_SMOOTH, 0, 14, 35);
    g_lfo(ACID, 0, LT_ENG + AP_CUTOFF, LS_TRI, 1, 12, 14);
    g_lfo(LEAD, 0, LT_PAN, LS_SINE, 1, 11, 20);

    Pattern *k = g_pat(KICK, 128, 0), *sn = g_pat(SNARE, 128, 0), *h = g_pat(HATS, 128, 0), *bo = g_pat(BOOM, 128, 0), *tx = g_pat(TEXT, 128, 0);
    Pattern *sb = g_pat(SUB, 128, 1), *wb = g_pat(WOB, 128, 1), *ld = g_pat(LEAD, 128, 0), *ch = g_pat(CHORD, 128, 1), *cl = g_pat(CLOUD, 128, 1);
    Pattern *ac = g_pat(ACID, 128, 0), *ar = g_pat(ARP, 128, 0);
    static const int root[4] = {45, 41, 38, 40};                              // A, F, D, E: two bars each
    static const int chord[4][3] = {{57, 60, 64}, {53, 57, 60}, {50, 53, 57}, {52, 56, 59}};   // Am F Dm E
    for (int bar = 0; bar < 8; bar++) { int p = bar * 16, r = root[bar / 2], last = bar == 7;
        if (beat && !(sc == 1 && bar < 2)) {
            g_hit(k, p, 124);
            if (drop || sc == 4) g_hit(k, p + 10, 108);
            if (drop && bar % 2) g_hit(k, p + 7, 96);
            if (sc == 4 && bar >= 6) { g_hit(k, p + 4, 110); g_hit(k, p + 12, 110); }
        }
        if (drop || (sc == 4 && bar < 4)) { g_hit(sn, p + 8, 118); if (drop && last) { g_hit(sn, p + 14, 80); g_hit(sn, p + 15, 92); } }
        if (sc == 4 && bar >= 4) for (int s = 0; s < 16; s += bar >= 6 ? 1 : 2) g_hit(sn, p + s, 70 + s * 3 + (bar - 4) * 6);   // the roll
        if (beat || (sc == 3 && bar >= 6)) for (int s = 2; s < 16; s += 4) g_hit(h, p + s, 92);
        if (drop || sc == 4) for (int s = 0; s < 16; s += 2) if (s % 4) g_hit(h, p + s + 1, 58);
        if (drop) { g_hit(h, p + 7, 60); g_hit(h, p + 13, 58); if (bar % 4 == 3) for (int s = 12; s < 16; s++) g_hit(h, p + s, 54 + 8 * (s - 12)); }
        if (in(sc, "256") && bar == 0) g_hit(bo, p, 120);
        if (sc == 3 && bar == 7) g_hit(bo, p + 12, 100);
        if (in(sc, "0167") && bar == 0) g_hit(tx, p, 100);
        if ((beat && !(sc == 1 && bar < 4)) || sc == 4) g_note(sb, p, 16, r - 12, 112);                  // the sub: the root, a bar long
        if (drop) { g_note(wb, p, 6, r, 120); g_note(wb, p + 6, 4, r, 100); g_note(wb, p + 10, 6, (bar % 2 ? r + 3 : r), 110); }   // the wobble riff
        if (in(sc, "012347")) for (int j = 0; j < 3; j++) g_note(ch, p, 16, chord[bar / 2][j], sc == 3 ? 96 : 80);
        if (in(sc, "0347") || (sc == 1 && bar < 4)) { if (bar % 2 == 0) g_note(cl, p, 32, 57 + (bar / 2 == 1 ? -4 : bar / 2 == 2 ? -7 : bar / 2 == 3 ? -5 : 0), 90); }
        if (in(sc, "56") || (sc == 4 && bar >= 2)) {                         // ACID: a dark sixteenth line in A minor
            static const int at[8] = {0, 2, 3, 6, 8, 10, 11, 14}, dg[8] = {0, 0, 12, 3, 0, 7, 8, 0};
            for (int j = 0; j < 8; j++) { int s = p + at[j]; g_hit(ac, s, j % 4 == 0 ? 120 : 84); ac->note[s] = (int8_t)(dg[j] + (r - 45));
                ac->slice[s] = (int8_t)(j == 2 || j == 6 ? ACID_SLIDE : j % 4 == 0 ? ACID_ACCENT : -1); }
        }
    }
    if (in(sc, "356")) g_hit(ld, 0, 100);                                     // the kit's loops: 8 bars each, one scene
    if (in(sc, "1256")) g_hit(ar, 0, 100);
    if (sc == 0) g_auto(CHORD, 0, LT_INS2 + 1, (int[]){0, 40, 127, 74}, 2, (int[]){40, 0});            // the intro opens
    if (sc == 4) g_auto(ACID, 0, LT_ENG + AP_CUTOFF, (int[]){32, 10, 127, 55}, 2, (int[]){30, 0});     // the build lifts the acid
    if (sc == 4) g_auto(TEXT, 0, LT_INS1 + 1, (int[]){0, 20, 127, 80}, 2, NULL);
    if (sc == 7) g_auto(CHORD, 0, LT_VOL, (int[]){0, 54, 127, 0}, 2, (int[]){60, 0});                  // the outro fades
    if (sc == 7) g_auto(CLOUD, 0, LT_VOL, (int[]){0, 72, 127, 10}, 2, (int[]){60, 0});
    static const int col[NSLOT] = {1, 2, 3, 4, 9, 5, 6, 7, 8, 9, 10, 11};
    for (int s = 0; s < NSLOT; s++) track_set_icol(&song.t[s], col[s]);
}
