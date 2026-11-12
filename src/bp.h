// BrickPad — shared declarations.
#pragma once
#include <stdint.h>
#include <math.h>

#define SR 48000
#define NSLOT 12
#define PREVIEW NSLOT   // extra engine slot for browser preview
#define NVOICE 32
#define NSTEP 128
#define NPAT 16
#define NOTE_NONE (-128)
#define STATE_MAGIC 0xB2B2000F   // 0xB2B2000E = without pname/smx/dkx/dkc (2026-11-11),  0xB2B2000D = automation without bends, 0xB2B2000C = step notes as scale degrees, 0xB2B2000B = without lnote/gx/gp, 0xB2B2000A = without ACID DEEP, 0xB2B20009 = without automation, 0xB2B20008 = without LFOs, 0xB2B20007 = without follow actions, 0xB2B20006 = without the mixer (prefixes of this layout), 0xB2B20005 = without the piano roll
#define NNOTE 96                 // piano-roll notes per pattern
#define RT 4                     // roll ticks per 1/32: note start and length are in 1/128 notes

typedef struct Sample {
    float *d;      // stereo interleaved, SR
    int n;         // frames
    char name[64];
    char path[512];
    int root;          // pitch class (tonic): from the name, else from the sound; -1 unknown
    int mode, root_by; // mode 0 a note / unknown, 1 major, 2 minor; root_by 0 none, 1 name, 2 sound
    int drum, loop;    // detected from name/folder
    float bpm;         // loops: from name or length
    int bars;
    int nons, ons[33]; // transient starts (frames), ons[0] = 0
    float npk, nld;    // NORMALIZE gains: PEAK (the loudest point to -1 dB), LOUD (the loud parts to one level, never past PEAK); 0 = not measured
} Sample;

// A piano-roll note. start/len in 1/128 notes (RT per 1/32) from the clip's step one; note = MIDI pitch (60 = C4).
// Slots never move: the UI writes len last when it adds a note and clears len first when it removes one.
typedef struct { uint16_t start, len; uint8_t note, vel, prob, flags; } Note;
#define NF_ACC 1                 // ACID accent
#define NF_SKIP 0x80             // overdub: heard while recording, not played again this pass (the engine clears it)
typedef struct {
    int8_t trig[NSTEP], note[NSTEP], vel[NSTEP], prob[NSTEP], rep[NSTEP], slice[NSTEP];  // note = scale degree; slice -1 = in order
    uint8_t len, div;            // len 1..NSTEP; div index 0..5 = 1/32 1/16 1/8 1/4 1/2 1/1
    uint8_t mode, spare;         // mode 0 STEPS, 1 ROLL (roll[] plays, the step lanes are kept but silent)
    Note roll[NNOTE];            // unsorted slots, len 0 = free
} Pattern;

typedef struct {
    Pattern pat[NPAT];
    int8_t cur, queued, on, mute;   // queued -1 = none
    int8_t pitch, fine, vol, pan;   // semis -24..24, cents -50..50, 0..100, -50..50
    int16_t start, end;             // per mille of the sample
    int8_t rev, voice;              // voice 0 AUTO(mono) 1 POLY
    int8_t root;                    // -1 AUTO (from name), 0..11 manual
    int8_t slices;                  // 0 OFF, 1..4 = 4/8/16/32 equal, 5 AUTO (transients), 6 EDIT (Song.cut)
    int8_t warp;                    // 0 OFF, 1 BEATS, 2 REPITCH, 3 TONES, 4 TEXTURE, 5 COMPLEX
    int8_t bars;                    // 0 AUTO, else 1..16
    int16_t bpm;                    // 0 AUTO (detected), else manual loop tempo
    int8_t grain, flux;             // TONES/TEXTURE grain size (ms, 0 = per mode) and randomness 0..100
    int8_t scale, key;              // this pad's own key: 0 follows the project, else scale index + 1, pitch class + 1
    uint8_t icon, icon_hi;          // 0 AUTO (by sound type), else icon id + 1 (src/icons.h); use track_icon/track_set_icon
    int8_t type;                    // 0 SAMPLER, 1 GRAND (parameters in sp), 2 ACID (ap), 3 CLOUD (cp, plays the sample)
    int8_t sp[16];                  // GRAND parameters, see SP_* in src/synth.h
    int8_t ap[12];                  // ACID parameters, see AP_* in src/acid.h (kept apart: switching loses nothing)
    int8_t cp[7];                   // CLOUD parameters, see CP_* in src/cloud.h (the last free bytes of the old reserved[])
    char path[256];
} Track;
static inline int track_icon(const Track *t) { return t->icon | t->icon_hi << 8; }
static inline void track_set_icon(Track *t, int v) { t->icon = (uint8_t)v; t->icon_hi = (uint8_t)(v >> 8); }

// Hand-edited slices: pos[k] = first frame of slice k (slice 0 starts at START).
// pos[0] = follow mask: bit k means slice k has no step of its own and plays right after slice k-1.
typedef struct { int32_t n, pos[32]; } Cuts;

// Mixer and effects (2026-11-09): per pad 2 insert slots, sends A (reverb) and B (delay); returns and master.
#define NINS 2
enum { FX_NONE, FX_COMP, FX_DIST, FX_CHORUS, FX_FLANGER, FX_PHASER, FX_FILTER, FX_CRUSH, FX_TAPE, FX_AUTOPAN, FX_RING, FX_MULTI, FX_N };
typedef struct { uint8_t type, on; int8_t p[6]; } FxSlot;            // on 0 = bypass, the params are kept
typedef struct { FxSlot ins[NINS]; uint8_t send_a, send_b, solo, flags; int8_t spare[4]; } PadMix;   // spare[0], spare[1]: DUCK (sidechain, 2026-11-11): the pad whose notes duck this one (0 off, 1..12) and how much (0..100)
#define DK_SRC 0
#define DK_AMT 1
#define PM_FRZ 3                 // PadMix.spare[3]: 1 = the pad is frozen (rendered again after the project loads)
#define PM_NORM 2                // PadMix.spare[2]: NORMALIZE of the pad's sample (SAMPLER, CLOUD): 0 OFF, 1 PEAK, 2 LOUD (2026-11-11)
enum { RV_SIZE, RV_DECAY, RV_DAMP, RV_PRE, RV_WIDTH, RV_LOWCUT, RV_LEVEL, RV_MUTE };
enum { DL_TIME, DL_FB, DL_TONE, DL_LOWCUT, DL_PING, DL_WIDTH, DL_LEVEL, DL_MUTE };
enum { MS_COMP, MS_THRESH, MS_RATIO, MS_ATTACK, MS_RELEASE, MS_MAKEUP, MS_LIM, MS_CEIL, MS_LIMREL, MS_DUCKSRC, MS_DUCKAMT, MS_DUCKREL };
typedef struct { PadMix pad[NSLOT]; int8_t rv[8], dl[8], ms[12]; uint8_t mark, spare[3]; } Mixer;
#define MIX_INIT 0x6D
// Follow actions (as in Ableton): after AFTER plays of a pattern, action A, or B with B CHANCE %. 0 everywhere = loop.
enum { FO_AFTER, FO_A, FO_B, FO_BCH, FO_JUMP, FO_SPARE };
enum { FA_NONE, FA_NEXT, FA_PREV, FA_FIRST, FA_RANDOM, FA_OTHER, FA_JUMP, FA_STOP, FA_N };
// Modulators (2026-11-09): 4 LFOs per pad. target: LT_* codes; depth -100..100 of the target's whole range (PITCH: 12 semis).
#define NLFO 4
typedef struct { uint8_t target, shape, sync, rate; int8_t depth; uint8_t retrig, phase, spare; } Lfo;   // retrig: 0 free run, 1 each note of this pad, 2 + n each note of pad n (2026-11-11: as the M8's trigger)
enum { LT_NONE, LT_PITCH, LT_VOL, LT_PAN, LT_SENDA, LT_SENDB, LT_INS1 = 10, LT_INS2 = 20, LT_ENG = 40, LT_DEEP = 24, LT_ENGN = 56 };   // DEEP pages: LT_ENG + LT_DEEP + k (ACID DEEP ax / GRAND DEEP gx)   // INS: +param 0..5, ENG: +engine param (cloud: cp 0..5, cx at +6)
enum { LS_SINE, LS_TRI, LS_SAWUP, LS_SAWDN, LS_SQUARE, LS_SH, LS_SMOOTH, LS_DUCK, LS_N };   // DUCK (2026-11-11): down at once, back up over the cycle; with a START note it runs once per note (sidechain)
#define LFO_NSYNC 14                     // the tempo divisions of fx.h plus 2 and 4 bars
// Automation (2026-11-09): per pattern 4 lanes, a value 0..100 per step (AUTO_EMPTY = no change), the same targets as
// the LFOs (LT_*). The value sets the target outright (PITCH: 50 = the note, 0/100 = -/+12), LFOs add on top.
#define NAUTO 4
#define AUTO_EMPTY (-1)
typedef struct { uint8_t target, smooth; int8_t v[NSTEP]; } AutoLane; 
typedef struct {
    uint32_t magic;
    int16_t tempo;                  // BPM
    int8_t swing, launch, metro, master;   // swing 0..75 %, launch = global quantisation: 0 and 1 = 1 BAR (0 was NOW before 2026-11-08), 2 = 2 BARS, 3 = 4 BARS, 4 = NONE
    int8_t key, scale;
    Track t[NSLOT];
    Cuts cut[NSLOT];                // from here on only append: an older state.bin simply ends earlier and these stay 0
    int8_t count_in, rec_quant;     // beats before a take (0 off); 0 clip step, 1..4 = 1/32 1/16 1/8 1/4
    int8_t beats, rec_len;          // beats per bar; a take's length rounds to 0 bar, 1 half bar, 2 beat, 3 step
    uint8_t len_min, len_max;       // clip length limits in steps (PROJECT MIN/MAX LENGTH), 0 = 1 and NSTEP
    uint8_t snap, spare1;           // snap: SCALE SNAP (EDIT+<> on a step walks the pad's scale only)
    int8_t cx[NSLOT][16];           // CLOUD's hidden CLOUD DEEP page, per pad (CX_* in src/cloud.h; 0 everywhere = not set yet)
    Mixer mix;                      // the mixer and effects (appended 2026-11-09)
    uint8_t fol[NSLOT][NPAT][6];    // follow actions per pattern (appended 2026-11-09): FO_* below
    Lfo lfo[NSLOT][NLFO];           // modulators per pad (appended 2026-11-09)
    AutoLane aut[NSLOT][NPAT][NAUTO];   // automation per pattern (appended 2026-11-09)
    int8_t ax[NSLOT][16];           // ACID's hidden ACID DEEP page per pad (AX_* in src/acid.h; appended 2026-11-09)
    int8_t lnote[NSLOT];            // the note OPTION and EDIT play on a pad's panel, semitones from its usual one (appended 2026-11-09)
    int8_t gx[NSLOT][32];           // GRAND's GRAND DEEP page (GX_* in src/synth.h; reserved 2026-11-09)
    uint8_t gp[NSLOT][8][2];        // GRAND's PATCH cables: out jack, in jack (0 none; reserved 2026-11-09)
    int8_t acurve[NSLOT][NPAT][NAUTO][NSTEP];   // automation: the BEND of the segment from each point to the next (appended 2026-11-10)
    int8_t mbx[NSLOT][NINS][6];     // MULTIBAND extras per insert: MB_* in src/fx.h, dB, 0 = as before (appended 2026-11-10)
    char pname[NSLOT][16];          // the pad's own name, "" = the instrument's or the sample's (appended 2026-11-11)
    int8_t smx[NSLOT][8];           // SAMPLER DEEP: SX_* below, 0 everywhere = as before (appended 2026-11-11)
    int8_t dkx[NSLOT][8];           // DUCK window: DX_* below (appended 2026-11-11); DX_MARK != DX_INIT = the old duck (notes, a beat, the classic curve)
    uint8_t dkc[NSLOT][16];         // DUCK curve: the level at 16 points along the dip, 0 = fully ducked .. 100 = untouched
} Song;
// SAMPLER DEEP: fades of a sampler voice, 0..100 -> v * v / 5 ms (0, 0.2 ms .. 2 s).
// FADE IN at the start; FADE OUT before the end of what plays (the sample, END, a slice, a cut note);
// XFADE: a voice plays on past its end for that long while fading, and the next one fades in as long: slices overlap.
enum { SX_FIN, SX_FOUT, SX_XFADE, SX_N };
static inline int sx_frames(int v) { return v <= 0 ? 0 : (int)((long)v * v * 48000 / 5000); }
// DUCK: MODE what starts the dip: the source pad's notes, its sound passing THRESH, or a grid on the tempo (no source);
// LEN how long the curve lasts and GRID how often it starts (indices into dk_beats); SMOOTH 0..100; SHAPE the preset
// the curve was filled from (DK_USER once a point was moved).
enum { DX_MODE, DX_LEN, DX_GRID, DX_SMOOTH, DX_THRESH, DX_SHAPE, DX_MARK, DX_SPARE };
enum { DM_NOTE, DM_AUDIO, DM_GRID, DM_N };
#define DX_INIT 0x5D
#define DK_NLEN 6
#define DK_USER 100

extern Song song;
// An automation lane's value at a fractional step (0..100), -1: no point in the lane. bend[]: the segment's shape.
static inline float auto_bend(float f, int c) {          // 0..1 along a segment -> 0..1 of the way to the next point
    if (c >= 100) return f >= 1 ? 1.f : 0.f;               // a right angle: hold, then the next point
    if (c <= -100) return f > 0 ? 1.f : 0.f;               // the other one: the next value at once
    if (c == 0) return f;
    float p = expf(fabsf((float)c) / 100.f * 3.f);
    return c > 0 ? powf(f, p) : 1.f - powf(1.f - f, p);
}
static inline float auto_value(const AutoLane *a, const int8_t *bend, int len, double stepf) {
    int k = (int)stepf % len, back = -1, fwd = -1;
    for (int i = 0; i < len; i++) { int j = (k - i + len) % len; if (a->v[j] != AUTO_EMPTY) { back = j; break; } }
    if (back < 0) return -1;
    for (int i = 1; i <= len; i++) { int j = (k + i) % len; if (a->v[j] != AUTO_EMPTY) { fwd = j; break; } }
    double from = stepf - ((k - back + len) % len) - (stepf - floor(stepf)), dist = (fwd - back + len) % len; if (dist <= 0) dist = len;
    double f = (stepf - from) / dist; if (f < 0) f = 0; if (f > 1) f = 1;
    return a->v[back] + (a->v[fwd] - a->v[back]) * auto_bend((float)f, bend ? bend[back] : 0);
}
         // shared: UI writes, audio thread reads
extern volatile float tempo_mul;   // temporary nudge (PROJECT L1/R1)

// sample.c
Sample *sample_load(const char *path);   // NULL on failure
void sample_free(Sample *s);
int sample_verify(const char *path);      // 1 = plays; 0 = unreadable, silent or garbage (any thread)
void sample_name_info(const char *path, float *bpm, int *root);   // from the file name only
void sample_sound_root(const Sample *s, int *root, int *mode);   // from the sound only (mode 1 major, 2 minor)
extern float key_last_corr, key_last_pc[12], key_last_bass[12];
typedef struct { char name[200]; int dir; } Entry;
int list_dir(const char *dir, Entry *out, int max);
int list_dir_ext(const char *dir, Entry *out, int max, const char *ext);   // files ending in ext (NULL: audio), dirs first   // dirs first, then wav/ogg; sorted; skips ._*

// engine.c
void song_defaults(void);
void pattern_clear(Pattern *p);
int engine_start(int buffer_frames);      // 0 = ok
void engine_stop(void);
void engine_trig(int slot, float vel, float semis, int slice);   // slice -1 = whole sample
void engine_trig_chain(int slot, float vel, float semis, int slice);   // the slice and the slices that follow it
void engine_trig_held(int slot, float vel, float semis, int slice);    // sounds until engine_release()
void engine_release(void);
void engine_trig_key(int slot, float vel, float semis, int slice, int tag);   // held until engine_release_key(tag), tag >= 2
void engine_release_key(int tag);
int engine_quant32(void);                 // the launch quantum in 1/32, 0 = none
void engine_launch(int slot, int on);     // count running: start (1, from step one) or stop (0) at the next quantum; -1 cancels
int engine_fol_n(int slot);              // loops the current pattern has played towards its follow action
void engine_pause(int on);               // the audio device stops calling back (export renders by hand)
// FREEZE (2026-11-11): a pad plays a recording of itself (engine, notes, inserts, modulation) instead of working it out.
// One buffer per pattern, a whole loop long, rendered by the UI with the device paused; the strip (VOL PAN sends MUTE
// DUCK) stays live. Call these three only with the device paused or locked (engine_lock).
float engine_duck_view(int slot, float *gain);
extern const float dk_beats[DK_NLEN];    // DUCK lengths and grid steps in beats: 1/16 1/8 1/4 1/2 1 BAR 2 BARS
void engine_lock(int on);                // the callback is kept out while pointers change
float *engine_freeze_buf(int slot, int pat, float *d, int n);   // gives the pattern its recording (NULL none); returns the old one to free
const float *engine_freeze_get(int slot, int pat, int *n);   // the pattern's recording (NULL none)
void engine_freeze_on(int slot, int on); // the pad plays its recordings / plays live
void engine_capture(int slot, float *buf, long max);   // slot's sound after its inserts, before the strip, into buf from now on (-1 stops)
long engine_captured(void);
void engine_render(float *out, int frames);   // stereo float frames, as the device would get them (with the device paused)
float engine_glfo(int slot);             // a GRAND pad's GRAND DEEP LFO now, -1..1 (0 when off)
int engine_want(int slot);                // what waits for the next quantum: 1 start, 0 stop, -1 nothing
uint32_t engine_anchor(int slot);         // the 1/32 position where the slot's clip began: its step one
void engine_set_anchor(int slot, uint32_t pos32);
void engine_transport_skip(int slot, int step);   // start the transport; that slot's step was just heard, it does not play again
int engine_watchdog(uint32_t now);        // >0: the audio call was stuck that many ms, its real-time priority is gone
int engine_synth_live(int slot, float *env, float *fc);
int engine_sample_live(int slot, float *pos, int max);   // the sampler: where each sounding voice of the slot reads, 0..1 of the sample
int engine_cloud_grains(int slot, float *pos, int max, float *env, float *scan);   // CLOUD: each sounding grain's place in the SPRAY band (offset from its centre); *scan: MOTION so far   // 1 if a synth voice sounds: its envelope and cutoff (Hz)
// tonal / slicing helpers (engine.c)
#define NSCALE 11
extern const char *scale_name[NSCALE], *note_name[12];
int scale_len(int sc);
int deg_semis(int sc, int deg);          // degree -> semitones above the tonic
int track_scale(int slot);               // the pad's own scale or the project's
int track_key(int slot);                 // the same for the tonic
int note_semis(int slot, int note);      // shift for a piano-roll note (MIDI pitch) on this slot
int deg_midi(int slot, int deg);         // the MIDI pitch a scale degree plays on this slot (KEYS, RANDOM, old projects)
int hit_semis(int slot, int deg);        // semitone shift for a degree on this slot (root-aware)
int track_root(int slot);                // effective root pitch class, -1 none (drums/unknown)
int track_nslices(int slot);
int track_slice_pos(int slot, int k);    // first frame of slice k, in any slicing mode
int track_follows(int slot, int k);      // slice k plays right after slice k-1
int track_bars(int slot);                // effective bars of a loop (0 = not a loop)
float track_bpm(int slot);
void relayout(int slot);                 // lay slices out on the current pattern
void engine_set_sample(int slot, Sample *s);  // ownership passes to engine; NULL clears
void engine_transport(int play);          // 1 start from bar 1, 0 stop
void engine_stop_all(void);
void engine_collect_garbage(void);        // frees retired samples (UI thread)
float engine_peak(void);
void engine_meters(float *out);
float engine_master_gr(void);
float engine_lfo(int slot, int j);
float engine_auto(int slot, int lane);      // an automation lane's value now (0..100), -1 none          // an LFO's output now, -1..1             // dB the master compressor/limiter takes off now
void engine_fx_view(int slot, int i, float *v);   // what an insert does now, for its screen (6 values, by type)
int engine_fx_live(int slot, int i);      // the insert is on and within FX_BUDGET           // NSLOT + 3 peaks (pads, return A, return B, master) since the last call
float engine_load(void);
void engine_stats(float *mx, int *over, float *gap, int *n);
int engine_voices(void);
int engine_frames(void);
int engine_playing(void);
int engine_step(int slot);                // last played step of the slot's clip, -1 none
void engine_count(int beats);             // count-in clicks, then the transport starts from bar 1
int engine_counting(void);                // beats still to come, -1 not counting
double engine_count_left(void);           // time to bar 1 in 1/32 notes
double engine_posf(void);                 // transport position in 1/32 notes, with the fraction
void engine_skip(int slot, int step);     // the sequencer passes over this step once
int engine_step32(const Pattern *p);      // length of the pattern's step in 1/32 notes
uint32_t engine_pos32(void);              // transport position in 1/32 notes
const Sample *engine_sample(int slot);    // UI-side mirror
