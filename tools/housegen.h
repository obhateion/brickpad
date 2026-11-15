// HOUSE demo v2 (2026-11-15, user: "more scenes, more instruments, effects, automation, LFOs: everything the Brick can").
// Built in code, not on the buttons: six scenes of twelve pads each, F minor, 124 BPM. Run: HOUSEGEN=OUT.bpp golden 0
// (in a folder that has samples/). Writes the project file the app reads: the first scene, then ScnHead and every scene.
#define HG_KIT "samples/808/"
enum { HG_KICK, HG_CLAP, HG_HAT, HG_OHAT, HG_BASS, HG_CHORD, HG_LEAD, HG_PERC, HG_RIDE, HG_ATMOS, HG_LOOP, HG_FX };
static const char *const hg_arp = "samples/CINEMATIC/Dark Cinematic Electronica/loops/arp_loops/dce_105_synth_loop_arp_mused_Dmin.wav";
static const char *const hg_atm = "samples/CINEMATIC/Dark Cinematic Electronica/loops/atmospheres_&_drones/dce_105_synth_loop_atmosphere_passage_Dmin.wav";
static const char *const hg_pad = "samples/CINEMATIC/Dark Cinematic Electronica/loops/pad_and_key_loops/dce_110_synth_loop_pad_forge_Dmin.wav";

static void hg_fresh(void) {             // a scene starts from nothing; the project's part is set the same in each
    song_defaults();
    song.tempo = 124; song.swing = 8; song.key = 5; song.scale = 2; song.launch = 1; song.master = 82;   // F MINOR
    Mixer *m = &song.mix;
    m->rv[RV_SIZE] = 62; m->rv[RV_DECAY] = 55; m->rv[RV_DAMP] = 45; m->rv[RV_PRE] = 18; m->rv[RV_LOWCUT] = 35; m->rv[RV_LEVEL] = 78;
    m->dl[DL_TIME] = 7; m->dl[DL_FB] = 42; m->dl[DL_TONE] = 55; m->dl[DL_LOWCUT] = 30; m->dl[DL_PING] = 1; m->dl[DL_LEVEL] = 70;   // 3/16 ping-pong
    m->ms[MS_COMP] = 1; m->ms[MS_THRESH] = 62; m->ms[MS_RATIO] = 25; m->ms[MS_ATTACK] = 35; m->ms[MS_RELEASE] = 45; m->ms[MS_MAKEUP] = 8;
    m->ms[MS_LIM] = 1; m->ms[MS_CEIL] = 90;
}
static void hg_name(int s, const char *n) { snprintf(song.pname[s], sizeof song.pname[s], "%s", n); }
static void hg_smp(int s, const char *path, const char *n, int vol, int pan) {
    Track *t = &song.t[s]; t->type = 0; snprintf(t->path, sizeof t->path, "%s", path); t->vol = (int8_t)vol; t->pan = (int8_t)pan; hg_name(s, n);
}
static void hg_eng(int s, int type, const char *preset, const char *n, int vol, int pan) {
    Track *t = &song.t[s]; t->type = (int8_t)type; t->vol = (int8_t)vol; t->pan = (int8_t)pan; hg_name(s, n);
    if (type == 1) synth_defaults(t->sp); else if (type == 2) acid_defaults(t->ap); else cl_defaults(t->cp);
    for (int i = 0; i < NPRESET; i++) if (presets[i].engine == type && !strcmp(presets[i].name, preset)) { preset_apply(s, presets[i].v); return; }
    fprintf(stderr, "housegen: no preset %s\n", preset);
}
static Pattern *hg_pat(int s, int q, int len) { Pattern *p = &song.t[s].pat[q]; pattern_clear(p); p->len = (uint8_t)len; p->div = 1; return p; }
// Steps from a string, one char a 1/16: x full, o 75, g ghost 45, ? half the time, r a double hit, 3 a triple; . rest
static void hg_steps(int s, int q, const char *str) {
    int n = (int)strlen(str); Pattern *p = hg_pat(s, q, n);
    for (int i = 0; i < n; i++) { char c = str[i]; if (c == '.' || c == ' ') continue;
        p->trig[i] = 1; p->vel[i] = c == 'o' ? 75 : c == 'g' ? 45 : 100; if (c == '?') { p->prob[i] = 50; p->vel[i] = 70; }
        if (c == 'r') p->rep[i] = 2; if (c == '3') p->rep[i] = 3; }
}
// ACID: notes as MIDI, 0 = rest; flags: 'a' accent, 's' slide, 'b' both, '.' none
static void hg_acid(int s, int q, int n, const int *notes, const char *fl) {
    Pattern *p = hg_pat(s, q, n);
    for (int i = 0; i < n; i++) { if (!notes[i]) continue; p->trig[i] = 1; p->note[i] = (int8_t)notes[i];
        char c = fl[i]; p->slice[i] = c == 'a' ? ACID_ACCENT : c == 's' ? ACID_SLIDE : c == 'b' ? 3 : -1; p->vel[i] = c == 'a' || c == 'b' ? 115 : 95; }
}
static Pattern *hg_roll(int s, int q, int steps) { Pattern *p = hg_pat(s, q, steps); p->mode = 1; return p; }
static void hg_note(Pattern *p, int st16, int len16, int note, int vel) { roll_add(p, st16 * 8, len16 * 8, note, vel); }
static void hg_chord(Pattern *p, int st16, int len16, const int *n, int vel) { for (int i = 0; i < 4 && n[i]; i++) hg_note(p, st16, len16, n[i], vel - i * 4); }
static void hg_ins(int s, int i, int type, int p0, int p1, int p2, int p3, int p4, int p5) {
    FxSlot *f = &song.mix.pad[s].ins[i]; fx_set_type(f, type); int v[6] = {p0, p1, p2, p3, p4, p5};
    for (int k = 0; k < 6; k++) if (v[k] >= 0) f->p[k] = (int8_t)v[k];
}
static void hg_send(int s, int a, int b) { song.mix.pad[s].send_a = (uint8_t)a; song.mix.pad[s].send_b = (uint8_t)b; }
static void hg_duck(int s, int src, int amt) { song.mix.pad[s].spare[DK_SRC] = (int8_t)(src + 1); song.mix.pad[s].spare[DK_AMT] = (int8_t)amt; }
// LFO j: sync rate = an index of div_beats (2 1/16, 5 1/8, 8 1/4, 10 1/2, 11 1 bar, 12 2 bars, 13 4 bars), or free 0..100
static void hg_lfo(int s, int j, int target, int shape, int sync, int rate, int depth, int retrig) {
    Lfo *l = &song.lfo[s][j]; l->target = (uint8_t)target; l->shape = (uint8_t)shape; l->sync = (uint8_t)sync; l->rate = (uint8_t)rate; l->depth = (int8_t)depth; l->retrig = (uint8_t)retrig;
}
// Automation lane a of pattern q: points (step, value 0..100), gliding between them when smooth
static void hg_auto(int s, int q, int a, int target, int smooth, int np, const int *pt) {
    AutoLane *l = &song.aut[s][q][a]; l->target = (uint8_t)target; l->smooth = (uint8_t)smooth; memset(l->v, AUTO_EMPTY, NSTEP);
    memset(song.acurve[s][q][a], smooth ? 0 : 100, NSTEP);
    for (int i = 0; i < np; i++) l->v[pt[2 * i] % NSTEP] = (int8_t)pt[2 * i + 1];
}
static void hg_fol(int s, int q, int after, int a, int b, int bch) { uint8_t *f = song.fol[s][q]; f[FO_AFTER] = (uint8_t)after; f[FO_A] = (uint8_t)a; f[FO_B] = (uint8_t)b; f[FO_BCH] = (uint8_t)bch; }
static void hg_icon(int s, int col) { (void)col; }

// ---- the parts, shared by scenes ----
static const char *const HAT16 = "xgoxxgox xgoxxgor";
static void hg_drums(int full) {         // kick, clap, hats, open hat: the 808 kit
    hg_smp(HG_KICK, HG_KIT "808_01_kick.wav", "KICK", 92, 0);
    hg_steps(HG_KICK, 0, "x...x...x...x...");
    hg_steps(HG_KICK, 1, "x...x...x...x.g.");             // a ghost before the bar, then back
    hg_fol(HG_KICK, 0, 3, FA_NEXT, 0, 0); hg_fol(HG_KICK, 1, 1, FA_FIRST, 0, 0);
    hg_ins(HG_KICK, 0, FX_COMP, 55, 45, 10, 35, 15, 100);
    hg_smp(HG_CLAP, HG_KIT "808_04_clap.wav", "CLAP", 72, 0); hg_send(HG_CLAP, 28, 0);
    hg_steps(HG_CLAP, 0, "....x.......x...");
    hg_steps(HG_CLAP, 1, "....x.......x..g");
    hg_fol(HG_CLAP, 0, 3, FA_NEXT, 0, 0); hg_fol(HG_CLAP, 1, 1, FA_FIRST, 0, 0);
    hg_smp(HG_HAT, HG_KIT "808_06_chh.wav", "HATS", full ? 58 : 50, 10);
    { int n = 16; Pattern *p = hg_pat(HG_HAT, 0, n); for (int i = 0; i < n; i++) { p->trig[i] = 1; p->vel[i] = i % 4 == 2 ? 100 : i % 2 ? 55 : 72; if (i % 4 == 3) p->prob[i] = 70; } p->rep[15] = 2; }
    hg_lfo(HG_HAT, 0, LT_PAN, LS_SINE, 1, 11, 35, 0);               // the hats drift left and right over a bar
    hg_lfo(HG_HAT, 1, LT_VOL, LS_SAWDN, 1, 5, 25, 0);              // and breathe on the eighths
    hg_smp(HG_OHAT, HG_KIT "808_07_ohh.wav", "OPEN HAT", 55, -12);
    hg_steps(HG_OHAT, 0, "..x...x...x...x."); hg_send(HG_OHAT, 0, 12);
    hg_duck(HG_OHAT, HG_KICK, 35);
}
static const int BASS_A[16] = {41, 0, 41, 53, 0, 41, 0, 44, 41, 0, 41, 53, 0, 48, 0, 46};   // F2 offbeat roll, up to C3 / Bb2
static const int BASS_B[16] = {41, 0, 41, 53, 0, 44, 0, 46, 48, 0, 48, 60, 0, 46, 44, 41};
static void hg_bass(int q0, int openness) {   // ACID on the offbeats, ducked by the kick; its LFO and a drive insert
    hg_eng(HG_BASS, 2, "WAREHOUSE", "BASS", 78, 0);
    hg_acid(HG_BASS, q0, 16, BASS_A, "a..s.a...a.b...s");
    hg_acid(HG_BASS, q0 + 1, 16, BASS_B, "a..b.....a.s.as.");
    hg_fol(HG_BASS, q0, 3, FA_NEXT, 0, 0); hg_fol(HG_BASS, q0 + 1, 1, FA_PREV, FA_FIRST, 25);
    song.t[HG_BASS].ap[AP_CUTOFF] = (int8_t)openness;
    hg_lfo(HG_BASS, 0, LT_ENG + AP_CUTOFF, LS_TRI, 1, 12, 22, 0);   // the filter swells over two bars
    hg_lfo(HG_BASS, 1, LT_ENG + AP_ENVMOD, LS_SH, 1, 2, 18, 0);    // and the envelope jumps every sixteenth
    hg_ins(HG_BASS, 0, FX_DIST, 1, 35, 55, 55, 45, -1);
    hg_duck(HG_BASS, HG_KICK, 62);
}
static const int CH_FM9[4] = {53, 56, 60, 67}, CH_DB[4] = {49, 53, 56, 60}, CH_EB[4] = {51, 55, 58, 62}, CH_CM[4] = {48, 51, 55, 58};
static void hg_chords(int stabs) {       // GRAND, poly: Fm9 Db Eb Cm7, stabs on the offbeats or held pads
    hg_eng(HG_CHORD, 1, stabs ? "SOFT CHORD" : "WARM HALO", "CHORDS", stabs ? 62 : 58, 0);
    song.t[HG_CHORD].voice = 1;
    Pattern *p = hg_roll(HG_CHORD, 0, 64); const int *ch[4] = {CH_FM9, CH_DB, CH_EB, CH_CM};
    for (int b = 0; b < 4; b++) {
        if (stabs) { hg_chord(p, b * 16 + 2, 2, ch[b], 96); hg_chord(p, b * 16 + 7, 1, ch[b], 80); hg_chord(p, b * 16 + 10, 3, ch[b], 92); }
        else hg_chord(p, b * 16, 16, ch[b], 84); }
    hg_send(HG_CHORD, stabs ? 22 : 40, stabs ? 30 : 12);
    hg_ins(HG_CHORD, 0, FX_CHORUS, 25, 45, -1, -1, 100, 45);
    hg_ins(HG_CHORD, 1, FX_FILTER, 0, 70, 25, 0, 0, 0);            // a low-pass the automation opens
    hg_lfo(HG_CHORD, 0, LT_ENG + SP_CUTOFF, LS_SINE, 1, 13, 20, 0);
    hg_lfo(HG_CHORD, 1, LT_PAN, LS_TRI, 1, 12, 20, 0);
    hg_duck(HG_CHORD, HG_KICK, 40);
}
static void hg_lead(void) {              // GRAND pluck arpeggio through the delay; TAPE for age
    hg_eng(HG_LEAD, 1, "ZAP HARP", "LEAD", 55, 15);
    static const int arp[4][4] = {{65, 68, 72, 75}, {61, 65, 68, 72}, {63, 67, 70, 74}, {60, 63, 67, 70}};
    Pattern *p = hg_roll(HG_LEAD, 0, 64);
    for (int b = 0; b < 4; b++) for (int i = 0; i < 16; i++) if (i % 3 != 2) hg_note(p, b * 16 + i, 1, arp[b][(i * 3 + b) % 4] + (i >= 12 ? 12 : 0), i % 4 == 0 ? 105 : 78);
    hg_send(HG_LEAD, 18, 45);
    hg_ins(HG_LEAD, 0, FX_TAPE, 25, 30, 25, 60, 35, 100);
    hg_lfo(HG_LEAD, 0, LT_ENG + SP_CUTOFF, LS_SAWUP, 1, 11, 25, 0);
    hg_lfo(HG_LEAD, 1, LT_SENDB, LS_SINE, 1, 13, 30, 0);
}
static void hg_perc(void) {              // congas with pitches, by chance
    hg_smp(HG_PERC, "samples/505/505_11_hiconga.wav", "CONGA", 52, -25);
    Pattern *p = hg_pat(HG_PERC, 0, 32); static const char *s = "..x..x.g..x.x..g..x..x.g.x..x.?.";
    for (int i = 0; i < 32; i++) if (s[i] != '.') { p->trig[i] = 1; p->vel[i] = s[i] == 'g' ? 50 : 85; p->prob[i] = s[i] == '?' ? 40 : i % 5 == 0 ? 75 : 100; p->note[i] = (int8_t)(60 + (i % 7 == 0 ? 5 : i % 3 == 0 ? -2 : 0)); }
    hg_send(HG_PERC, 15, 20); hg_ins(HG_PERC, 0, FX_PHASER, 20, 55, 50, 40, 1, 40);
    hg_lfo(HG_PERC, 0, LT_PAN, LS_SH, 1, 5, 40, 0);
}
static void hg_ride(const char *path, const char *n) {
    hg_smp(HG_RIDE, path, n, 42, 22); hg_steps(HG_RIDE, 0, "..x...x...x...xg"); hg_send(HG_RIDE, 10, 0);
    hg_ins(HG_RIDE, 0, FX_AUTOPAN, 10, 45, 0, 0, -1, -1);
}
static void hg_atmos(const char *preset, int vol) {   // CLOUD over a dark atmosphere, one long note a bar
    hg_eng(HG_ATMOS, 3, preset, "ATMOS", vol, 0); snprintf(song.t[HG_ATMOS].path, sizeof song.t[HG_ATMOS].path, "%s", hg_atm);
    Pattern *p = hg_roll(HG_ATMOS, 0, 64); hg_note(p, 0, 32, 53, 90); hg_note(p, 32, 32, 49, 90);
    hg_send(HG_ATMOS, 45, 10);
    hg_lfo(HG_ATMOS, 0, LT_ENG + CP_POS, LS_SMOOTH, 1, 13, 35, 0);   // the grains wander through the file
    hg_lfo(HG_ATMOS, 1, LT_ENG + CP_SPRAY, LS_SINE, 1, 12, 25, 0);
}
static void hg_loop(void) {              // the arp loop, cut to the tempo, filtered
    hg_smp(HG_LOOP, hg_arp, "ARP LOOP", 50, -10); song.t[HG_LOOP].warp = 1;
    hg_steps(HG_LOOP, 0, "x...............................................................");
    hg_ins(HG_LOOP, 0, FX_FILTER, 1, 40, 30, 0, 0, 0);             // high-pass, opened by the automation
    hg_ins(HG_LOOP, 1, FX_CRUSH, 60, 25, 0, 80, 30, -1);
    hg_send(HG_LOOP, 12, 25); hg_duck(HG_LOOP, HG_KICK, 30);
}
static void hg_crash(void) { hg_smp(HG_FX, "samples/505/505_15_crash.wav", "CRASH", 48, 0); hg_steps(HG_FX, 0, "x..............................................................."); hg_send(HG_FX, 30, 0); }

static void hg_scene(int k) {
    hg_fresh();
    switch (k) {
    case 0: {                            // INTRO: hats, congas, the atmosphere; the bass's filter opens over four bars
        hg_drums(0);
        hg_steps(HG_KICK, 0, "x...x...x...x..."); hg_steps(HG_KICK, 1, "x...x...x...x...");
        hg_pat(HG_CLAP, 0, 16); hg_pat(HG_CLAP, 1, 16); hg_pat(HG_OHAT, 0, 16);
        hg_bass(0, 18); hg_pat(HG_BASS, 1, 16); hg_fol(HG_BASS, 0, 0, 0, 0, 0);
        { Pattern *b = &song.t[HG_BASS].pat[0]; Pattern c = *b; hg_pat(HG_BASS, 0, 64); for (int r = 0; r < 4; r++) for (int i = 0; i < 16; i++) {
            b->trig[r * 16 + i] = c.trig[i]; b->note[r * 16 + i] = c.note[i]; b->slice[r * 16 + i] = c.slice[i]; b->vel[r * 16 + i] = c.vel[i]; } }
        static const int op[] = {0, 10, 63, 70}; hg_auto(HG_BASS, 0, 0, LT_ENG + AP_CUTOFF, 1, 2, op);
        static const int rs[] = {0, 30, 48, 55, 63, 72}; hg_auto(HG_BASS, 0, 1, LT_ENG + AP_RESO, 1, 3, rs);
        hg_perc(); hg_ride(HG_KIT "808_12_cymbal.wav", "CYMBAL");
        hg_atmos("WIDE SKY", 60);
        hg_crash();
        break; }
    case 1:                              // GROOVE: the whole kit, the bass, chord stabs
        hg_drums(1); hg_bass(0, 45); hg_chords(1); hg_perc();
        hg_ride("samples/505/505_16_ride.wav", "RIDE");
        hg_atmos("SOFT BLANKET", 42); hg_crash();
        break;
    case 2: {                            // BREAK: no kick; held chords open up, the lead arrives, a riser, a clap roll
        hg_drums(0);
        hg_pat(HG_KICK, 0, 16); hg_pat(HG_KICK, 1, 16); hg_pat(HG_OHAT, 0, 16);
        hg_steps(HG_CLAP, 0, "................................................x...x...x.x.xxxx");
        song.t[HG_CLAP].pat[0].rep[60] = 2; song.t[HG_CLAP].pat[0].rep[62] = 3; song.t[HG_CLAP].pat[0].rep[63] = 4; hg_pat(HG_CLAP, 1, 16);
        static const int cv[] = {48, 30, 52, 60, 56, 90, 60, 100}; hg_auto(HG_CLAP, 0, 0, LT_VOL, 1, 4, cv);   // the roll swells
        hg_chords(0);
        static const int co[] = {0, 15, 32, 45, 63, 95}; hg_auto(HG_CHORD, 0, 0, LT_INS2 + 1, 1, 3, co);   // FILTER CUTOFF of insert 2
        static const int cs[] = {0, 20, 63, 70}; hg_auto(HG_CHORD, 0, 1, LT_SENDA, 1, 2, cs);
        hg_lead(); static const int lv[] = {0, 0, 31, 0, 32, 60, 63, 75}; hg_auto(HG_LEAD, 0, 0, LT_VOL, 1, 4, lv);   // silent for two bars, then in
        hg_atmos("CRYSTAL RISE", 62);
        hg_eng(HG_FX, 1, "RISER", "RISER", 45, 0); { Pattern *p = hg_roll(HG_FX, 0, 64); hg_note(p, 32, 32, 53, 100); }
        static const int rp[] = {32, 40, 63, 95}; hg_auto(HG_FX, 0, 0, LT_PITCH, 1, 2, rp);
        hg_send(HG_FX, 35, 25); hg_ins(HG_FX, 0, FX_FLANGER, 30, 70, 40, 60, 100, 50);
        break; }
    case 3:                              // DROP: everything, the arp loop through a sweeping filter
        hg_drums(1); hg_bass(0, 55); hg_chords(1); hg_lead(); hg_perc();
        hg_ride("samples/505/505_16_ride.wav", "RIDE"); hg_loop(); hg_crash();
        static const int lo[] = {0, 70, 31, 20, 32, 10, 63, 55}; hg_auto(HG_LOOP, 0, 0, LT_INS1 + 1, 1, 4, lo);
        hg_atmos("SCATTER FIELD", 38);
        song.t[HG_LEAD].vol = 46;
        break;
    case 4: {                            // DUB: the 707 kit, chords only as echoes, the bass deep and slow
        hg_drums(1);
        hg_smp(HG_KICK, "samples/707/707_01_kick.wav", "KICK 707", 92, 0); hg_smp(HG_CLAP, "samples/707/707_06_clap.wav", "CLAP 707", 66, 0);
        hg_smp(HG_HAT, "samples/707/707_12_chh.wav", "HATS 707", 50, 10); hg_smp(HG_OHAT, "samples/707/707_08_tamb.wav", "TAMB", 46, -15);
        hg_send(HG_CLAP, 15, 45);
        hg_bass(0, 30); hg_eng(HG_BASS, 2, "DUB MOSS", "BASS DUB", 80, 0);
        hg_chords(1); hg_send(HG_CHORD, 20, 75); song.t[HG_CHORD].vol = 50;
        hg_ins(HG_CHORD, 1, FX_FILTER, 1, 45, 40, 0, 6, 45);          // a high-pass with its own LFO: thin echoes
        static const int dfb[] = {0, 30, 15, 70, 16, 30, 31, 75}; hg_auto(HG_CHORD, 0, 0, LT_SENDB, 0, 4, dfb);
        hg_perc(); hg_ride("samples/707/707_15_ride.wav", "RIDE 707");
        hg_eng(HG_ATMOS, 3, "UNDERGROUND", "DRONE", 50, 0); snprintf(song.t[HG_ATMOS].path, sizeof song.t[HG_ATMOS].path, "%s", hg_pad);
        { Pattern *p = hg_roll(HG_ATMOS, 0, 64); hg_note(p, 0, 64, 53, 90); } hg_send(HG_ATMOS, 40, 25);
        hg_lfo(HG_ATMOS, 0, LT_ENG + CP_POS, LS_TRI, 1, 13, 40, 0);
        hg_crash();
        break; }
    case 5: {                            // OUTRO: kick and hats, the bass closes, the atmosphere stays
        hg_drums(0); hg_pat(HG_CLAP, 0, 16); hg_pat(HG_CLAP, 1, 16);
        hg_bass(0, 50); hg_fol(HG_BASS, 0, 0, 0, 0, 0); hg_pat(HG_BASS, 1, 16);
        { Pattern *b = &song.t[HG_BASS].pat[0]; Pattern c = *b; hg_pat(HG_BASS, 0, 64); for (int r = 0; r < 4; r++) for (int i = 0; i < 16; i++) {
            b->trig[r * 16 + i] = c.trig[i]; b->note[r * 16 + i] = c.note[i]; b->slice[r * 16 + i] = c.slice[i]; b->vel[r * 16 + i] = c.vel[i]; } }
        static const int cl[] = {0, 65, 63, 8}; hg_auto(HG_BASS, 0, 0, LT_ENG + AP_CUTOFF, 1, 2, cl);
        static const int hv[] = {0, 60, 47, 60, 63, 15}; hg_auto(HG_HAT, 0, 0, LT_VOL, 1, 3, hv);
        Pattern c = song.t[HG_HAT].pat[0]; Pattern *h = hg_pat(HG_HAT, 0, 64); for (int i = 0; i < 64; i++) { h->trig[i] = c.trig[i % 16]; h->vel[i] = c.vel[i % 16]; h->prob[i] = c.prob[i % 16]; h->rep[i] = c.rep[i % 16]; }
        hg_atmos("STILL LAKE", 58); hg_ride(HG_KIT "808_12_cymbal.wav", "CYMBAL");
        break; }
    }
    for (int s = 0; s < NSLOT; s++) { Track *t = &song.t[s]; t->cur = 0; t->queued = -1; t->on = 0; }
    song.mix.mark = MIX_INIT;
}
static int house_gen(const char *out) {
    enum { N = 6 }; static Song sc[N];
    for (int k = 0; k < N; k++) { hg_scene(k); sc[k] = song; }
    FILE *f = fopen(out, "wb"); if (!f) return 1;
    ScnHead h = {SCN_MAGIC, N, 0, 0};
    int ok = fwrite(&sc[0], sizeof(Song), 1, f) == 1 && fwrite(&h, sizeof h, 1, f) == 1 && fwrite(sc, sizeof(Song), N, f) == N;
    fclose(f); printf("housegen: %s %s, %d scenes\n", out, ok ? "written" : "FAILED", N);
    return !ok;
}
