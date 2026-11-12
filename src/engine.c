// Real-time audio engine: SDL audio callback, sample-accurate sequencer, scales, slices, warp.
// Lock-free command queue in, retired samples out.
#include "bp.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

Song song;
volatile float tempo_mul = 1.f;

// ---------- scales ----------
const char *scale_name[NSCALE] = {"CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN",
                                  "HARM MINOR", "MAJ PENTA", "MIN PENTA", "BLUES"};
const char *note_name[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const int8_t scales[NSCALE][13] = {
    {12, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, {7, 0, 2, 4, 5, 7, 9, 11}, {7, 0, 2, 3, 5, 7, 8, 10},
    {7, 0, 2, 3, 5, 7, 9, 10}, {7, 0, 1, 3, 5, 7, 8, 10}, {7, 0, 2, 4, 6, 7, 9, 11}, {7, 0, 2, 4, 5, 7, 9, 10},
    {7, 0, 2, 3, 5, 7, 8, 11}, {5, 0, 2, 4, 7, 9}, {5, 0, 3, 5, 7, 10}, {6, 0, 3, 5, 6, 7, 10}};
int scale_len(int sc) { return scales[sc % NSCALE][0]; }
int deg_semis(int sc, int deg) {
    int n = scale_len(sc), o = deg >= 0 ? deg / n : -((-deg + n - 1) / n), i = deg - o * n;
    return scales[sc % NSCALE][1 + i] + 12 * o;
}

typedef struct { int type, slot; float a, b; void *p; } Cmd;
enum { CMD_COUNT, CMD_TRIG, CMD_TRIG_HELD, CMD_TRIG_CHAIN, CMD_RELEASE, CMD_SET_SAMPLE, CMD_TRANSPORT, CMD_STOP_ALL };
#define QN 512
typedef struct { Cmd q[QN]; unsigned head, tail; } Queue;  // single producer, single consumer
static Queue cmdq, trashq;

static int q_push(Queue *q, Cmd c) {
    unsigned h = __atomic_load_n(&q->head, __ATOMIC_RELAXED), t = __atomic_load_n(&q->tail, __ATOMIC_ACQUIRE);
    if (h - t >= QN) return 0;
    q->q[h % QN] = c;
    __atomic_store_n(&q->head, h + 1, __ATOMIC_RELEASE);
    return 1;
}
static int q_pop(Queue *q, Cmd *c) {
    unsigned t = __atomic_load_n(&q->tail, __ATOMIC_RELAXED), h = __atomic_load_n(&q->head, __ATOMIC_ACQUIRE);
    if (t == h) return 0;
    *c = q->q[t % QN];
    __atomic_store_n(&q->tail, t + 1, __ATOMIC_RELEASE);
    return 1;
}

#include "synth.h"
#include "acid.h"
#include "cloud.h"
#include "fx.h"
#define GRAIN 2048
typedef struct {
    const Sample *s; double pos, rate, base, lo, hi, wbpm;
    float gl, gr, fade, fstep;
    int slot, on, delay, cut_in, warp;   // delay: frames before start; cut_in: frames until fade-out (-1 never)
    double gst[2]; int gph[2], gsize, flux;   // TONES/TEXTURE grains
    unsigned age;
    int held;                            // listen voice: stops when the button is released
    int synth; SynthV sy;                // 1 GRAND, 2 ACID (the synth makes the sound), 3 CLOUD (grains of s)
    CloudV cl;
    int fin, fout, el, cutx; double adv; // cutx: it was cut with room to fade, the end fade is not needed; sampler voices only (SAMPLER DEEP): fade-in and fade-out lengths in frames, frames played, what pos moves by per frame
} Voice;

static Sample *smp[NSLOT + 1];
static float gl_buf[NSLOT][1024], gl_sh[NSLOT]; static double gl_ph[NSLOT]; static int gl_on[NSLOT]; static volatile float gl_now[NSLOT];   // GRAND DEEP LFOs
static int gp_on[NSLOT];                // GRAND pads with PATCH cables
static Voice voices[NVOICE];
static unsigned age_ctr;
static int last_voice = -1;              // the voice start_voice just took, -1 none
static int playing, play_step[NSLOT];
static volatile int skip_step[NSLOT];    // step+1 to pass over once: already heard when it was recorded
static uint32_t pos32;
static uint32_t anchor[NSLOT];            // where each clip began (1/32): steps count from there, as a clip launched in Ableton
static volatile int8_t want[NSLOT] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};   // written by the UI, taken here
static volatile int anchor_set[NSLOT];    // UI asks: anchor = anchor_req (+1 so 0 means nothing)
static volatile uint32_t anchor_req[NSLOT];
static volatile int count_left = -1;      // count-in: clicks to come, -1 off
static double count_next;                 // frames to the next click, from the next buffer
static double next32;
static struct { int delay, t; float f, a; } click;
static float peak_acc, load_avg;
static float st_max, st_gap; static int st_over, st_n; static Uint64 st_last;
// Mixer (2026-11-09): every pad's voices sum into its own bus; the strip then applies VOL and PAN (ramped over a
// buffer, so moving them never clicks, and they act on notes already sounding), meters it and adds it to the mix.
// Invariant: a bus is all zero unless written in this callback (it is cleared right after its strip).
#define MAXN 4096
static float keyb[NSLOT][2 * MAXN]; static const float *fx_key[NINS];   // sidechain: the key pads' sound before their own strips
static float bus[NSLOT + 1][2 * MAXN];   // [PREVIEW] = the browser, no strip
static uint8_t bus_live[NSLOT + 1];
// FREEZE: see bp.h. frz_at = where in this buffer a loop of the recording begins (-1 not here), frz_qn = which pattern's.
typedef struct { float *d; int n; } FrzBuf;
static FrzBuf frz[NSLOT][NPAT]; static uint8_t frz_act[NSLOT], frz_snd[NSLOT];
static long frz_pos[NSLOT]; static int frz_at[NSLOT], frz_qn[NSLOT], frz_q[NSLOT], frz_seq = -1; static float frz_g[NSLOT];
static float frz_out[NSLOT][2 * MAXN];
static int fcap_s = -1; static float *fcap_buf; static long fcap_n, fcap_max;
static float strip_gl[NSLOT], strip_gr[NSLOT];
static float mpk[NSLOT + 3];             // meters: pads, return A, return B, master (peak since the UI last read)
#include "fxdsp.h"
static FxState fxs[NSLOT][NINS];
static SpringState spr[NSLOT]; static int spr_tail[NSLOT];   // GRAND's SPRING per pad, and how long it rings on
static Reverb rvb; static Delay dly; static MasterState mst;
static float snd_a[2 * MAXN], snd_b[2 * MAXN], ret[2 * MAXN];
static float send_ga[NSLOT], send_gb[NSLOT], master_g = -1;
static int strip_tail[NSLOT];            // frames an insert may still ring after the pad went silent
static uint8_t fx_live[NSLOT][NINS];     // the slot is on and within FX_BUDGET (the UI reads it to grey slots out)
// ---------- modulators: per buffer, each pad's LFOs make modulated copies of what they move; the song keeps its values ----------
typedef struct { double ph; float out, sh, prev; uint32_t rng; int armed; } LfoState;
static LfoState lfos[NSLOT][NLFO];
static volatile float lfo_view[NSLOT][NLFO];   // the UI draws them
static uint8_t lfo_note[NSLOT], has_mod[NSLOT];
static float mod_pitch[NSLOT] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, m_vol[NSLOT], m_pan[NSLOT], m_sa[NSLOT], m_sb[NSLOT];
static int8_t m_sp[NSLOT][16], m_ap[NSLOT][12], m_cp[NSLOT][8], m_cx[NSLOT][16], m_ax[NSLOT][16], m_gx[NSLOT][32];
static FxSlot m_ins[NSLOT][NINS];
static float lfo_beats(int i) { return i < NDIV ? div_beats[i] : i == NDIV ? 8.f : 16.f; }
static void eng_range(int type, int k, int *lo, int *hi) {   // an engine parameter's range (cloud: cp then cx)
    *lo = 0; *hi = 100;
    if (type == 1) sp_range(k, lo, hi); else if (type == 2) ap_range(k, lo, hi); else if (type == 3 && k >= CP_N) cx_range(k - CP_N, lo, hi);
}
static int8_t mod_byte(int base, float add, int lo, int hi) { int v = (int)lrintf(base + add); return (int8_t)(v < lo ? lo : v > hi ? hi : v); }
// The automation value of a lane at a fractional step (0..100), or -1: no value in the lane
static float auto_at(const AutoLane *a, int len, double stepf) {
    int k = (int)stepf % len, back = -1, fwd = -1;
    for (int i = 0; i < len; i++) { int j = (k - i + len) % len; if (a->v[j] != AUTO_EMPTY) { back = j; break; } }
    if (back < 0) return -1;
    if (!a->smooth) return a->v[back];
    for (int i = 1; i <= len; i++) { int j = (k + i) % len; if (a->v[j] != AUTO_EMPTY) { fwd = j; break; } }
    double from = stepf - ((k - back + len) % len) - (stepf - floor(stepf)), dist = (fwd - back + len) % len; if (dist <= 0) dist = len;
    double f = (stepf - from) / dist; if (f < 0) f = 0; if (f > 1) f = 1;
    return (float)(a->v[back] + (a->v[fwd] - a->v[back]) * f);
}
static volatile float auto_view[NSLOT][NAUTO];   // the UI draws the playhead's values
// DUCK (2026-11-11: the window, as Kickstart / ShaperBox): a curve of 16 points (Song.dkc) run once over LEN beats from
// each trigger: a note of the source pad, its sound passing THRESH (dk_pk, the peak of its last buffer), or a grid on
// the tempo. The strip's gain is 1 - AMOUNT * (1 - curve). A pad without the window's mark plays the old duck: notes,
// one beat, the classic curve.
const float dk_beats[DK_NLEN] = {0.25f, 0.5f, 1, 2, 4, 8};
static const uint8_t dk_classic[16] = {0, 13, 25, 36, 46, 56, 64, 72, 78, 84, 89, 93, 96, 98, 100, 100};   // 1 - (1 - x)^2
static float dk_ph[NSLOT], dk_g[NSLOT], dk_pk[NSLOT], dk_view[NSLOT]; static uint8_t dk_arm[NSLOT]; static unsigned dk_srcs;
static void lfo_run(int n, double tempo) {
    float bt = (float)(n / (SR * 60.0 / (tempo < 20 ? 20 : tempo))); unsigned srcs = 0;
    double dk_song = playing ? (pos32 - next32 / (SR * 60.0 / (tempo < 20 ? 20 : tempo) / 8)) / 8 : 0; if (dk_song < 0) dk_song = 0;   // beats since the count began
    for (int s = 0; s < NSLOT; s++) {
        const PadMix *pd = &song.mix.pad[s]; const int8_t *dx = song.dkx[s]; int src = pd->spare[DK_SRC], init = dx[DX_MARK] == DX_INIT;
        int mode = init ? dx[DX_MODE] % DM_N : DM_NOTE; float len = dk_beats[init ? dx[DX_LEN] % DK_NLEN : 2]; const uint8_t *cv = init ? song.dkc[s] : dk_classic;
        int on = mode == DM_GRID ? pd->spare[DK_AMT] > 0 : src > 0 && src <= NSLOT && src - 1 != s;
        if (!on) { dk_ph[s] = -1; dk_g[s] = 1; dk_view[s] = -1; dk_arm[s] = 1; continue; }
        if (mode == DM_GRID) dk_ph[s] = playing ? (float)fmod(dk_song, dk_beats[dx[DX_GRID] % DK_NLEN]) : -1;
        else if (mode == DM_NOTE) { if (lfo_note[src - 1]) dk_ph[s] = 0; }
        else { float thr = powf(10.f, (dx[DX_THRESH] - 100) * 0.03f), pk = dk_pk[src - 1]; srcs |= 1u << (src - 1);   // 0..100 = -60..0 dB; armed again under half of it
            if (pk >= thr && dk_arm[s]) { dk_ph[s] = 0; dk_arm[s] = 0; } else if (pk < thr * 0.5f) dk_arm[s] = 1; }
        float tg = 1;
        if (dk_ph[s] >= 0 && dk_ph[s] < len) { float x = dk_ph[s] / len * 15; int i = (int)x; float c = cv[i] + (cv[i < 15 ? i + 1 : 15] - cv[i]) * (x - i);
            tg = 1 - pd->spare[DK_AMT] / 100.f * (1 - c / 100.f); dk_view[s] = dk_ph[s] / len; }
        else { dk_view[s] = -1; if (mode != DM_GRID) dk_ph[s] = -1; }
        int sm = init ? dx[DX_SMOOTH] : 0;
        if (sm <= 0) dk_g[s] = tg; else dk_g[s] += (tg - dk_g[s]) * (1 - expf(-(n * 1000.f / SR) / (sm * 0.5f)));   // SMOOTH 100 = 50 ms
        if (dk_ph[s] >= 0 && mode != DM_GRID) dk_ph[s] += bt;
    }
    dk_srcs = srcs;
    double f32 = SR * 60.0 / (tempo < 20 ? 20 : tempo) / 8, now32 = pos32 - next32 / f32;   // where the count is at this buffer's start (1/32)
    for (int s = 0; s < NSLOT; s++) {
        const Track *t = &song.t[s]; int any = 0;
        float pitch = 0, vol = 0, pan = 0, sa = 0, sb = 0, ins[NINS][6] = {{0}}, eng[LT_ENGN] = {0};
        float ov_pitch = -1, ov_vol = -1, ov_pan = -1, ov_sa = -1, ov_sb = -1, ov_ins[NINS][6], ov_eng[LT_ENGN];
        for (int i = 0; i < NINS; i++) for (int k = 0; k < 6; k++) ov_ins[i][k] = -1;
        for (int k = 0; k < LT_ENGN; k++) ov_eng[k] = -1;
        if (playing && t->on && now32 >= anchor[s]) {   // the pattern's automation where its clip is now
            const Pattern *p = &t->pat[t->cur]; int d = 1 << (p->div % 6), len = p->len ? p->len : 16;   // div32[] comes later in the file
            double stepf = fmod((now32 - anchor[s]) / d, len);
            for (int a = 0; a < NAUTO; a++) {
                const AutoLane *al = &song.aut[s][t->cur][a]; auto_view[s][a] = -1;
                if (!al->target) continue;
                float v = auto_value(al, song.acurve[s][t->cur][a], len, stepf); auto_view[s][a] = v;
                if (v < 0) continue;
                any = 1; int tg = al->target;
                if (tg == LT_PITCH) ov_pitch = v; else if (tg == LT_VOL) ov_vol = v; else if (tg == LT_PAN) ov_pan = v;
                else if (tg == LT_SENDA) ov_sa = v; else if (tg == LT_SENDB) ov_sb = v;
                else if (tg >= LT_INS1 && tg < LT_INS1 + 6) ov_ins[0][tg - LT_INS1] = v; else if (tg >= LT_INS2 && tg < LT_INS2 + 6) ov_ins[1][tg - LT_INS2] = v;
                else if (tg >= LT_ENG && tg < LT_ENG + LT_ENGN) ov_eng[tg - LT_ENG] = v;
            }
        } else for (int a = 0; a < NAUTO; a++) auto_view[s][a] = -1;
        for (int j = 0; j < NLFO; j++) {
            const Lfo *L = &song.lfo[s][j]; LfoState *st = &lfos[s][j];
            if (!L->target || !L->depth) { lfo_view[s][j] = 0; continue; }
            any = 1;
            { int src = L->retrig >= 2 ? L->retrig - 2 : s; if (L->retrig && src < NSLOT && lfo_note[src]) { st->ph = 0; st->armed = 1; } }   // its own notes, or another pad's
            double hz = L->sync ? tempo / 60.0 / lfo_beats(L->rate % LFO_NSYNC) : 0.02 * pow(1000, (L->rate > 100 ? 100 : L->rate) / 100.0);
            double ph = st->ph + L->phase / 100.0; ph -= floor(ph);
            float o;
            switch (L->shape) {
            case LS_TRI: o = 1 - (float)(4 * fabs(ph - 0.5)); break;
            case LS_SAWUP: o = (float)(2 * ph - 1); break;
            case LS_SAWDN: o = (float)(1 - 2 * ph); break;
            case LS_SQUARE: o = ph < 0.5 ? 1.f : -1.f; break;
            case LS_DUCK: o = L->retrig && !st->armed ? 0 : -(float)((1 - ph) * (1 - ph)); break;   // free run: a pump on every cycle; with START: once per note, then at rest
            case LS_SH: o = st->sh; break;
            case LS_SMOOTH: o = st->prev + (st->sh - st->prev) * (float)(0.5 - 0.5 * cos(ph * M_PI)); break;
            default: o = (float)sin(ph * 2 * M_PI);
            }
            st->ph += hz * n / SR;
            if (st->ph >= 1) { st->ph -= floor(st->ph); st->armed = 0; if (!st->rng) st->rng = 0x9E3779B9u ^ (uint32_t)(s * 7 + j);
                st->prev = st->sh; st->sh = fx_rand(&st->rng); }
            st->out = o; lfo_view[s][j] = o;
            float a = o * L->depth / 100.f; int tg = L->target;
            if (tg == LT_PITCH) pitch += a * 12; else if (tg == LT_VOL) vol += a; else if (tg == LT_PAN) pan += a;
            else if (tg == LT_SENDA) sa += a; else if (tg == LT_SENDB) sb += a;
            else if (tg >= LT_INS1 && tg < LT_INS1 + 6) ins[0][tg - LT_INS1] += a; else if (tg >= LT_INS2 && tg < LT_INS2 + 6) ins[1][tg - LT_INS2] += a;
            else if (tg >= LT_ENG && tg < LT_ENG + LT_ENGN) eng[tg - LT_ENG] += a;
        }
        has_mod[s] = (uint8_t)any;   // lfo_note is cleared after the loop: a later pad may start on an earlier pad's note
        if (!any) { mod_pitch[s] = 1; m_vol[s] = m_pan[s] = m_sa[s] = m_sb[s] = 0; continue; }
        const PadMix *pmx = &song.mix.pad[s];   // automation sets the value outright (ov >= 0), LFOs add to it
        mod_pitch[s] = exp2f((pitch + (ov_pitch >= 0 ? (ov_pitch - 50) / 50 * 12 : 0)) / 12);
        m_vol[s] = vol + (ov_vol >= 0 ? ov_vol / 100 - t->vol / 100.f : 0);
        m_pan[s] = pan + (ov_pan >= 0 ? ov_pan / 50 - 1 - t->pan / 50.f : 0);
        m_sa[s] = sa + (ov_sa >= 0 ? ov_sa / 100 - pmx->send_a / 100.f : 0); m_sb[s] = sb + (ov_sb >= 0 ? ov_sb / 100 - pmx->send_b / 100.f : 0);
        int lo, hi;
#define BASE(ov, cur) ((ov) >= 0 ? lo + (ov) / 100.f * (hi - lo) : (float)(cur))
        for (int k = 0; k < 16; k++) { if (k < SP_N && t->type == 1) { eng_range(1, k, &lo, &hi); m_sp[s][k] = mod_byte((int)lrintf(BASE(ov_eng[k], t->sp[k])), eng[k] * (hi - lo), lo, hi); } else m_sp[s][k] = t->sp[k]; }
        for (int k = 0; k < 12; k++) { if (k < AP_N && t->type == 2) { eng_range(2, k, &lo, &hi); m_ap[s][k] = mod_byte((int)lrintf(BASE(ov_eng[k], t->ap[k])), eng[k] * (hi - lo), lo, hi); } else m_ap[s][k] = t->ap[k]; }
        for (int k = 0; k < 7; k++) { lo = 0; hi = 100; m_cp[s][k] = k < CP_N && t->type == 3 ? mod_byte((int)lrintf(BASE(ov_eng[k], t->cp[k])), eng[k] * 100, 0, 100) : t->cp[k]; }
        for (int k = 0; k < 16; k++) { if (k < CX_N && t->type == 3) { cx_range(k, &lo, &hi); m_cx[s][k] = mod_byte((int)lrintf(BASE(ov_eng[CP_N + k], song.cx[s][k])), eng[CP_N + k] * (hi - lo), lo, hi); } else m_cx[s][k] = song.cx[s][k]; }
        memcpy(m_ax[s], song.ax[s], 16); memcpy(m_gx[s], song.gx[s], 32);   // the DEEP pages: their knobs move too
        if (t->type == 2 && song.ax[s][AX_MARK] == AX_INIT) for (int k = 0; k < AX_N; k++) if (eng[LT_DEEP + k] != 0 || ov_eng[LT_DEEP + k] >= 0) { ax_range(k, &lo, &hi); m_ax[s][k] = mod_byte((int)lrintf(BASE(ov_eng[LT_DEEP + k], song.ax[s][k])), eng[LT_DEEP + k] * (hi - lo), lo, hi); }
        if (t->type == 1 && song.gx[s][GX_MARK] == GX_INIT) for (int k = 0; k < GX_N; k++) if (eng[LT_DEEP + k] != 0 || ov_eng[LT_DEEP + k] >= 0) { gx_range(k, &lo, &hi); m_gx[s][k] = mod_byte((int)lrintf(BASE(ov_eng[LT_DEEP + k], song.gx[s][k])), eng[LT_DEEP + k] * (hi - lo), lo, hi); }
        for (int i = 0; i < NINS; i++) { m_ins[s][i] = song.mix.pad[s].ins[i]; FxSlot *f = &m_ins[s][i];
            for (int k = 0; k < 6; k++) if (ins[i][k] != 0 || ov_ins[i][k] >= 0) { fx_range(f->type, k, &lo, &hi); f->p[k] = mod_byte((int)lrintf(BASE(ov_ins[i][k], f->p[k])), ins[i][k] * (hi - lo), lo, hi); } }
#undef BASE
    }
    memset(lfo_note, 0, sizeof lfo_note);   // seen by every pad's LFOs: forget them
}
static pthread_t audio_thread; static volatile int audio_rt; static volatile Uint32 cb_in;   // the watchdog's view
static int nvoices, frames_obt;
static const Sample *ui_samples[NSLOT + 1];
static SDL_AudioDeviceID dev;
static const Track preview_track = {.vol = 80, .end = 1000, .root = -1};
static const int div32[6] = {1, 2, 4, 8, 16, 32};

#define FADE_FRAMES 96
#include "pvoc.h"

void pattern_clear(Pattern *p) {
    memset(p, 0, sizeof *p);
    for (int i = 0; i < NSTEP; i++) { p->note[i] = NOTE_NONE; p->vel[i] = 100; p->prob[i] = 100; p->rep[i] = 1; p->slice[i] = -1; }
    p->len = 16; p->div = 1;
}

void song_defaults(void) {
    memset(&song, 0, sizeof song);
    // the user's own settings (2026-11-08): 120 BPM, C CHROMATIC, LAUNCH 1 BAR, no metronome, master 80, 4/4,
    // no count-in, record quantise 1/16, REC LENGTH beat
    song.magic = STATE_MAGIC; song.tempo = 120; song.master = 80; song.key = 0; song.scale = 0;
    song.beats = 4; song.rec_len = 2; song.launch = 1; song.rec_quant = 2; song.count_in = 0; song.metro = 0; song.swing = 0;
    for (int i = 0; i < NSLOT; i++) {
        Track *t = &song.t[i];
        for (int p = 0; p < NPAT; p++) pattern_clear(&t->pat[p]);
        t->queued = -1; t->vol = 75; t->end = 1000; t->root = -1;
        cx_defaults(song.cx[i]); ax_defaults(song.ax[i]); gx_defaults(song.gx[i]);
    }
    mix_defaults(&song.mix);
    for (int s = 0; s < NSLOT; s++) for (int q = 0; q < NPAT; q++) for (int a = 0; a < NAUTO; a++) memset(song.aut[s][q][a].v, AUTO_EMPTY, NSTEP);
}

// ---------- per-slot helpers (s = the sample the caller's thread sees) ----------
static const Track *trk(int slot) { return slot < NSLOT ? &song.t[slot] : &preview_track; }
int track_scale(int slot) { int v = trk(slot)->scale; return v > 0 ? v - 1 : song.scale; }
int track_key(int slot) { int v = trk(slot)->key; return v > 0 ? v - 1 : song.key; }
// A minor loop in a major key (or the other way) plays as its relative: Am in C major stays, not C minor.
static int scale_minor(int sc) { return sc == 2 || sc == 3 || sc == 4 || sc == 7 || sc == 9 || sc == 10; }   // MINOR DORIAN PHRYGIAN HARM MINOR MIN PENTA BLUES
static int root_of(int slot, const Sample *s) {
    const Track *t = trk(slot);
    if (t->root >= 0) return t->root;
    if (!s || s->drum || s->root < 0) return -1;
    int sc = track_scale(slot);
    if (sc == 0 || !s->mode) return s->root;                  // CHROMATIC or a note: as it is
    if (s->mode == 2 && !scale_minor(sc)) return (s->root + 3) % 12;
    if (s->mode == 1 && scale_minor(sc)) return (s->root + 9) % 12;
    return s->root;
}
static int semis_of(int slot, const Sample *s, int deg) {
    int r = root_of(slot, s), base = 0;
    if (r >= 0) { base = ((track_key(slot) - r) % 12 + 12) % 12; if (base > 6) base -= 12; }
    return base + deg_semis(track_scale(slot), deg);
}
static int nslices_of(int slot, const Sample *s) {
    int m = trk(slot)->slices;
    if (!s || !m) return 0;
    if (m == 6) return slot < NSLOT ? song.cut[slot].n : 0;
    return m == 5 ? s->nons : 4 << (m - 1);
}
static double slice_pos(int slot, const Sample *s, int k) {
    const Track *t = trk(slot);
    if (t->slices == 6) return k ? song.cut[slot].pos[k] : 0;   // pos[0] holds the follow mask
    if (t->slices == 5) return s->ons[k];
    double a = (double)s->n * t->start / 1000, b = (double)(s->n - 1) * t->end / 1000;
    return a + (b - a) / nslices_of(slot, s) * k;
}
static int bars_of(int slot, const Sample *s) {
    const Track *t = trk(slot);
    if (t->bars) return t->bars;
    if (t->bpm && s) { int b = (int)(s->n / (float)SR * t->bpm / 240 + 0.5f); return b < 1 ? 1 : b; }
    return s ? s->bars : 0;
}
static float bpm_of(int slot, const Sample *s) {
    if (trk(slot)->bpm) return trk(slot)->bpm;
    int b = bars_of(slot, s);
    return s && b ? b * 240.f / (s->n / (float)SR) : 0;
}
int track_root(int slot) { return root_of(slot, ui_samples[slot]); }
int hit_semis(int slot, int deg) { return semis_of(slot, ui_samples[slot], deg); }
// A piano-roll note (MIDI pitch) as the shift start_voice takes. Synths: GRAND's 0 is C3 + the pad's key,
// ACID's C2 + key. Samples: the sample's own pitch is its root nearest C4, or C4 when it has none.
static int note_semis_of(int slot, const Sample *s, int note) {
    int ty = slot < NSLOT ? song.t[slot].type : 0;
    if (ty == 1) return note - 48 - track_key(slot);
    if (ty == 2) return note - 36 - track_key(slot);
    const Track *t = trk(slot);              // the sample's own root: a written note keeps its pitch whatever the scale
    int r = t->root >= 0 ? t->root : s && !s->drum && s->root >= 0 ? s->root : -1;
    return note - 60 - (r < 0 ? 0 : r > 6 ? r - 12 : r);
}
int note_semis(int slot, int note) { return note_semis_of(slot, ui_samples[slot], note); }
int deg_midi(int slot, int deg) { const Sample *s = ui_samples[slot]; int m = 60 - note_semis_of(slot, s, 60) + semis_of(slot, s, deg); return m < 0 ? 0 : m > 127 ? 127 : m; }
int track_nslices(int slot) { return nslices_of(slot, ui_samples[slot]); }
int track_slice_pos(int slot, int k) { return (int)slice_pos(slot, ui_samples[slot], k); }
int track_follows(int slot, int k) {
    return song.t[slot].slices == 6 && k > 0 && k < song.cut[slot].n && ((uint32_t)song.cut[slot].pos[0] >> k & 1);
}
int track_bars(int slot) { return bars_of(slot, ui_samples[slot]); }
float track_bpm(int slot) { return bpm_of(slot, ui_samples[slot]); }

void relayout(int slot) {   // UI thread: lay the slices out on the current pattern
    Track *t = &song.t[slot]; const Sample *s = ui_samples[slot];
    Pattern *p = &t->pat[t->cur];
    int n = nslices_of(slot, s), bars = bars_of(slot, s);
    if (!n) return;
    if (bars < 1) bars = 1;
    pattern_clear(p);
    if (t->slices >= 5) {     // transients or edited points: place each at its time on a 1/16 grid
        int steps = bars * 16, dv = 1;
        while (steps > NSTEP) { steps /= 2; dv++; }
        p->len = steps; p->div = dv;
        for (int k = 0; k < n; k++) {
            if (track_follows(slot, k)) continue;   // plays inside the step of its parent
            int st = (int)(slice_pos(slot, s, k) / s->n * steps + 0.5);
            if (st < steps && !p->trig[st]) { p->trig[st] = 1; p->slice[st] = k; }
        }
    } else {
        int st32 = bars * 32 / n, dv = 0;
        while (dv < 5 && div32[dv] < st32) dv++;
        p->len = n > NSTEP ? NSTEP : n; p->div = dv;
        for (int k = 0; k < p->len; k++) p->trig[k] = 1;
    }
}

static void slice_bounds(int slot, const Sample *s, int n, int slice, double *lo, double *hi) {
    const Track *t = trk(slot);
    double a = (double)s->n * t->start / 1000, b = (double)(s->n - 1) * t->end / 1000;   // START/END bound every slice
    *lo = a; *hi = b;
    if (n && slice >= 0) {
        slice %= n;
        *lo = slice_pos(slot, s, slice); *hi = slice + 1 < n ? slice_pos(slot, s, slice + 1) : b;
        if (*lo < a) *lo = a;
        if (*hi > b) *hi = b;
        if (*lo > b) *lo = b;                 // START past a saved point: an empty slice, never a negative one
        if (*hi < *lo) *hi = *lo;
    }
}
static int synth_gate = -1;              // frames a sequencer step holds a synth note; -1: an eighth note
// ARP (GRAND DEEP): the notes a GRAND pad holds, played one by one in tempo (2026-11-10)
#define ARPN 16
typedef struct { float semis, vel; int left, wait, tag, seq; } ArpNote;   // left: frames to its end (-1 held by a key), wait: frames to its start
static ArpNote arp[NSLOT][ARPN]; static int arp_n[NSLOT], arp_seq[NSLOT], arp_idx[NSLOT], arp_emit, arp_last = -1; static double arp_t[NSLOT];
static int arp_on(int s) { return s >= 0 && s < NSLOT && song.t[s].type == 1 && song.gx[s][GX_MARK] == GX_INIT && song.gx[s][GX_ARP]; }
static void arp_add(int s, float vel, float semis, int delay, int gate) {
    int live = 0; for (int i = 0; i < arp_n[s]; i++) if (arp[s][i].left != 0) live = 1;
    if (!live) { arp_n[s] = 0; arp_t[s] = delay; arp_idx[s] = 0; }   // the first note: the arpeggio starts with it
    int i = arp_n[s] < ARPN ? arp_n[s]++ : 0;
    if (i == 0 && arp_n[s] == ARPN) { int o = 0; for (int j = 1; j < ARPN; j++) if (arp[s][j].seq < arp[s][o].seq) o = j; i = o; }   // full: the oldest goes
    arp[s][i] = (ArpNote){semis, vel, delay + gate, delay, 0, ++arp_seq[s]};
    arp_last = s * ARPN + i;
}
static int roll_gate = -1;               // a piano-roll note: frames until a sample voice fades out (-1 not a roll note)
static unsigned roll_age;                // voices younger than this were started by the same roll tick (a chord)
#define MAX_POLY 8                       // voices one pad may hold: a new one takes the pad's oldest
// A free voice, else the pad's oldest when it already has MAX_POLY, else the oldest anywhere.
static int pick_voice(int slot) {
    // 2026-11-11 (user: crackle on every 1/16 note and on chords): a full pad used to hand its oldest voice to the new
    // note at once, cutting the old sound mid-wave, a click each time. Now that voice fades out over 5 ms on its own
    // and the new note takes a free one. Under load a pad holds fewer voices (the oldest go first).
    int n = 0, mine = -1, fr = -1, fading = -1, old = -1; unsigned om = ~0u, oldest = ~0u;
    for (int i = 0; i < NVOICE; i++) {
        Voice *v = &voices[i];
        if (!v->on) { if (fr < 0) fr = i; continue; }
        if (v->fstep > 0) { if (fading < 0 || v->fade < voices[fading].fade) fading = i; continue; }   // on its way out already
        if (v->slot == slot) { n++; if (v->age < om) { om = v->age; mine = i; } }
        if (v->age < oldest) { oldest = v->age; old = i; }
    }
    int cap = load_avg > 0.7f ? 4 : load_avg > 0.5f ? 6 : MAX_POLY;   // 2026-11-11: sooner (the dense demo sat at 57 % with 9 voices on one pad)
    if (n >= cap && mine >= 0) {
        if (fr < 0 && fading < 0) return mine;
        voices[mine].fstep = 1.f / 256;
    }
    return fr >= 0 ? fr : fading >= 0 ? fading : old >= 0 ? old : 0;
}
static float synth_last[NSLOT];          // the pitch each pad's mono line was last at (Hz): GLIDE starts there
static int acid_flags = -1;              // the step being started: its ACCENT/SLIDE bits; -1 not a step (keys, pads)
static int acid_slid[NSLOT];             // the last ACID note of this pad slides into the next one
static void start_acid(int slot, float vel, float semis, int delay) {   // always mono, as the original
    const Track *t = trk(slot);
    if (t->mute) return;
    int fl = acid_flags < 0 ? 0 : acid_flags, acc = fl & ACID_ACCENT;
    float freq = (float)(440 * pow(2.0, (36 + track_key(slot) + t->pitch + t->fine / 100.0 + semis - 69) / 12.0));   // a bass: octave 2
    int gate = synth_gate > 0 ? synth_gate : (int)(SR * 30.0 / (song.tempo * tempo_mul));
    for (int i = 0; i < NVOICE; i++) {
        Voice *o = &voices[i];
        if (!o->on || o->synth != 2 || o->slot != slot || o->fstep) continue;
        if (o->delay > 0) { o->on = 0; continue; }
        // a slide from the last note, or keys played legato: no new attack; otherwise a new attack on the same voice
        int legato = acid_slid[slot] || (acid_flags < 0 && o->sy.stage != 3);
        o->sy.pf = freq; o->sy.pv = vel; o->sy.pg = gate; o->sy.pacc = acc; o->sy.pslide = legato; o->sy.pat = delay;
        o->age = ++age_ctr; o->held = 0; acid_slid[slot] = fl & ACID_SLIDE ? 1 : 0;
        last_voice = i; return;
    }
    int best = pick_voice(slot);
    float g = slot < NSLOT ? 1.f : t->vol / 100.f, p = slot < NSLOT ? 0.f : t->pan / 50.f, a = (p + 1) * (float)M_PI / 4;   // pads: VOL/PAN on the mixer strip
    Voice *v = &voices[best];
    memset(v, 0, sizeof *v);
    v->gl = g * cosf(a) * 1.41421f; v->gr = g * sinf(a) * 1.41421f; v->fade = 1; v->slot = slot; v->on = 1;
    v->delay = delay; v->cut_in = -1; v->age = ++age_ctr; v->synth = 2; v->sy.pat = -1; v->sy.rng = 0x2545F491u;
    acid_start(&v->sy, freq, vel, gate, acc, 0);
    acid_slid[slot] = fl & ACID_SLIDE ? 1 : 0;
    last_voice = best;
}
#define CL_VOICES 4                      // clouds at once: the oldest one gives way
static void start_cloud(int slot, float vel, float semis, int delay) {
    const Track *t = trk(slot); const Sample *s = smp[slot];
    if (t->mute || !s || s->n < 256) return;
    int n = 0, oldest = -1; unsigned age = ~0u;   // clouds still sounding (not fading out)
    for (int i = 0; i < NVOICE; i++) if (voices[i].on && voices[i].synth == 3 && voices[i].cl.stage != 3) { n++; if (voices[i].age < age) { age = voices[i].age; oldest = i; } }
    if (n >= CL_VOICES && oldest >= 0) voices[oldest].cl.stage = 3;
    int best = pick_voice(slot);
    float g = slot < NSLOT ? 1.f : t->vol / 100.f, p = slot < NSLOT ? 0.f : t->pan / 50.f, a = (p + 1) * (float)M_PI / 4;   // pads: VOL/PAN on the mixer strip
    Voice *v = &voices[best];
    memset(v, 0, sizeof *v);
    v->s = s; v->gl = g * cosf(a) * 1.41421f; v->gr = g * sinf(a) * 1.41421f; v->fade = 1; v->slot = slot; v->on = 1;
    v->delay = delay; v->cut_in = -1; v->age = ++age_ctr; v->synth = 3;
    int gate = synth_gate > 0 ? synth_gate : (int)(SR * 60.0 / (song.tempo * tempo_mul));   // a beat
    cl_start(&v->cl, (float)pow(2.0, (t->pitch + t->fine / 100.0 + semis) / 12.0), vel, gate, age_ctr * 2654435761u);
    last_voice = best;
}
static void start_synth(int slot, float vel, float semis, int delay) {
    const Track *t = trk(slot);
    if (t->mute) return;
    if (!arp_emit && arp_on(slot)) { arp_add(slot, vel, semis, delay, synth_gate > 0 ? synth_gate : (int)(SR * 30.0 / (song.tempo * tempo_mul))); return; }   // ARP: held, played by arp_run
    float freq = (float)(440 * pow(2.0, (48 + track_key(slot) + t->pitch + t->fine / 100.0 + semis - 69) / 12.0));
    int gate = synth_gate > 0 ? synth_gate : (int)(SR * 30.0 / (song.tempo * tempo_mul));
    if (t->voice == 0)                    // GATE = mono: the line stays on one voice, legato and GLIDE
        for (int i = 0; i < NVOICE; i++) {
            Voice *o = &voices[i];
            if (!o->on || o->synth != 1 || o->slot != slot || o->fstep) continue;
            if (o->delay > 0) { o->on = 0; continue; }   // not started yet: the new note replaces it
            o->sy.pf = freq; o->sy.pv = vel; o->sy.pg = gate; o->sy.pat = delay; o->age = ++age_ctr; o->held = 0;
            synth_last[slot] = freq;
            last_voice = i; return;
        }
    if (slot < NSLOT && song.gx[slot][GX_VCA] == 2 && song.gx[slot][GX_MARK] == GX_INIT)   // DRONE: a new note takes over, the old drones let go
        for (int i = 0; i < NVOICE; i++) if (voices[i].on && voices[i].synth == 1 && voices[i].slot == slot && voices[i].sy.stage == 3) voices[i].sy.stage = 4;
    int best = pick_voice(slot);
    float g = slot < NSLOT ? 1.f : t->vol / 100.f, p = slot < NSLOT ? 0.f : t->pan / 50.f, a = (p + 1) * (float)M_PI / 4;   // pads: VOL/PAN on the mixer strip
    Voice *v = &voices[best];
    memset(v, 0, sizeof *v);
    v->gl = g * cosf(a) * 1.41421f; v->gr = g * sinf(a) * 1.41421f; v->fade = 1; v->slot = slot; v->on = 1;
    v->delay = delay; v->cut_in = -1; v->age = ++age_ctr; v->synth = 1;
    synth_start(&v->sy, freq, vel, gate, 0);
    // As on the original, the pitch remembers the last note: after a rest the next one still glides from it.
    if (t->voice == 0 && t->sp[SP_GLIDE] > 0 && synth_last[slot] > 0) v->sy.freq = synth_last[slot];
    if (t->voice == 0) synth_last[slot] = freq;
    last_voice = best;
}
static void arp_run(int s, int n) {      // plays the arpeggio's notes that fall in this buffer, then ages the held set
    if (!arp_on(s)) { arp_n[s] = 0; return; }
    const int8_t *gx = song.gx[s];
    double step = div_beats[gx[GX_ARATE] % NDIV] * 60.0 / (song.tempo * tempo_mul) * SR; if (step < 64) step = 64;
    while (arp_t[s] < n) {
        int at = (int)arp_t[s]; ArpNote *cand[ARPN]; int c = 0;
        for (int i = 0; i < arp_n[s]; i++) { ArpNote *e = &arp[s][i]; if (e->wait <= at && (e->left < 0 || e->left > at)) cand[c++] = e; }
        if (!c) {                         // nothing held right now: the next note to start, else done for this buffer
            int nx = 1 << 30; for (int i = 0; i < arp_n[s]; i++) if (arp[s][i].wait > at && arp[s][i].wait < nx && arp[s][i].left != 0) nx = arp[s][i].wait;
            if (nx == 1 << 30) { arp_t[s] = n; arp_idx[s] = 0; break; }
            arp_t[s] = nx; arp_idx[s] = 0; continue;
        }
        int mode = gx[GX_AMODE] & 3;
        for (int i = 1; i < c; i++) for (int j = i; j > 0; j--) {   // a few notes: insertion sort by pitch (ORDER: as played)
            int sw = mode == 2 ? cand[j]->seq < cand[j - 1]->seq : cand[j]->semis < cand[j - 1]->semis;
            if (!sw) break; ArpNote *x = cand[j]; cand[j] = cand[j - 1]; cand[j - 1] = x; }
        int len = c * (gx[GX_AOCT] % 3 + 1), pos = arp_idx[s]++, i;
        if (mode == 1) i = len - 1 - pos % len;
        else if (mode == 3) { int per = len > 1 ? 2 * len - 2 : 1, q = pos % per; i = q < len ? q : per - q; }
        else i = pos % len;
        ArpNote *e = cand[i % c]; int keep = synth_gate;
        synth_gate = (int)(step * gx[GX_AGATE] / 100.0); if (synth_gate < 1) synth_gate = 1;
        arp_emit = 1; start_synth(s, e->vel, e->semis + 12 * (i / c), at); arp_emit = 0; synth_gate = keep;
        arp_t[s] += step;
    }
    arp_t[s] -= n;
    int m = 0;                            // age the set: what has ended goes
    for (int i = 0; i < arp_n[s]; i++) { ArpNote e = arp[s][i];
        e.wait = e.wait > n ? e.wait - n : 0;
        if (e.left > 0) { e.left -= n; if (e.left <= 0) continue; }
        if (e.left == 0) continue;
        arp[s][m++] = e; }
    arp_n[s] = m;
}
static void start_voice(int slot, float vel, float semis, int slice, int delay) {
    last_voice = -1; arp_last = -1;
    if (slot < NSLOT) lfo_note[slot] = 1;    // LFOs set to NOTE start again
    if (slot == frz_seq) return;             // a frozen pad's clip: the note still counts for sidechains, the recording is its sound
    if (slot < NSLOT && song.t[slot].type == 1) { start_synth(slot, vel, semis, delay); return; }
    if (slot < NSLOT && song.t[slot].type == 3) { start_cloud(slot, vel, semis, delay); return; }
    if (slot < NSLOT && song.t[slot].type == 2) {   // a slice number from the UI carries a step's ACCENT/SLIDE bits (audition)
        if (acid_flags < 0 && slice >= 0) { acid_flags = slice & 3; start_acid(slot, vel, semis, delay); acid_flags = -1; }
        else start_acid(slot, vel, semis, delay);
        return;
    }
    const Sample *s = smp[slot];
    if (!s || s->n < 2) return;
    const Track *t = trk(slot);
    if (slot < NSLOT && t->mute) return;
    double lo, hi;
    slice_bounds(slot, s, nslices_of(slot, s), slice, &lo, &hi);
    if (hi <= lo + 1) return;
    const int8_t *sx = slot < NSLOT ? song.smx[slot] : NULL; int prev = 0;
    int xfr = sx ? sx_frames(sx[SX_XFADE]) : 0, fir = sx ? sx_frames(sx[SX_FIN]) : 0, fou = sx ? sx_frames(sx[SX_FOUT]) : 0;
    for (int i = 0; i < NVOICE; i++) if (voices[i].on && voices[i].slot == slot && !voices[i].synth && voices[i].delay <= delay) prev = 1;   // something of this pad sounds: XFADE has a partner
    if (t->voice == 0)
        for (int i = 0; i < NVOICE; i++)
            if (voices[i].on && voices[i].slot == slot) {
                Voice *o = &voices[i];
                if (roll_gate > 0 && o->age > roll_age) continue;   // a chord of the same roll tick stays
                if (o->delay > delay) { o->on = 0; continue; }   // a follower that has not started yet
                if (xfr && !o->synth && o->adv > 0 && ((o->rate > 0 ? o->hi - 1 - o->pos : o->pos - o->lo) / o->adv) - delay <= o->fout) continue;   // XFADE: it is in its own fade already, or will be by then
                if (o->cut_in < 0 || o->cut_in > delay) o->cut_in = delay;
            }
    if (hi - lo < 1) return;                  // nothing to play, also keeps a reversed start inside the sample
    int best = pick_voice(slot);
    double rate = pow(2.0, (t->pitch + t->fine / 100.0 + semis) / 12.0);
    float g = vel * (slot < NSLOT ? 1.f : t->vol / 100.f), p = slot < NSLOT ? 0.f : t->pan / 50.f, a = (p + 1) * (float)M_PI / 4;   // pads: VOL/PAN on the mixer strip
    float wbpm = t->warp >= 2 ? bpm_of(slot, s) : 0;
    Voice *v = &voices[best];
    *v = (Voice){s, t->rev ? hi - 1 : lo, t->rev ? -rate : rate, rate, lo, hi, wbpm,
                 g * cosf(a) * 1.41421f, g * sinf(a) * 1.41421f, 1, 0, slot, 1, delay, -1,
                 wbpm > 0 ? t->warp : 0, {lo, lo}, {0, 0}, 0, 0, ++age_ctr};
    if (roll_gate > 0) v->cut_in = delay + roll_gate;   // a roll note ends at its length (or with the sample)
    v->fin = prev && xfr > fir ? xfr : fir; v->fout = xfr > fou ? xfr : fou;
    if (xfr) {                               // XFADE: it plays on past its end while it fades, under the next one's fade-in
        double ext = xfr * (wbpm > 0 && t->warp >= 3 ? song.tempo * tempo_mul / wbpm : rate);
        if (t->rev && !(wbpm > 0 && t->warp >= 3)) v->lo = lo - ext < 0 ? 0 : lo - ext; else v->hi = hi + ext > s->n ? s->n : hi + ext;
    }
    if (v->warp == 5) {   // COMPLEX: forward only; a voice that would pass PV_LOAD plays as TONES instead
        double load = rate > 1 ? rate : 1;   // its work grows with the pitch: frames are read faster
        for (int i = 0; i < NVOICE; i++)
            if (i != best && voices[i].on && voices[i].warp == 5 && !voices[i].fstep) load += voices[i].rate > 1 ? voices[i].rate : 1;
        if (load > PV_LOAD) v->warp = 3; else { v->rate = rate; v->pos = lo; pv_init(&pvs[best], lo); }
    }
    if (v->warp == 3 || v->warp == 4) {   // TONES / TEXTURE: forward only, grain size per mode or manual
        v->rate = rate; v->pos = lo;
        int ms = t->grain ? t->grain : v->warp == 3 ? 30 : 90;
        v->gsize = ms * SR / 1000; if (v->gsize < 256) v->gsize = 256;
        v->gph[1] = v->gsize / 2; v->flux = v->warp == 4 ? (t->flux ? t->flux : 40) : 0;
    }
    last_voice = best;
}
// A slice split in the editor keeps the rhythm: its followers start exactly when the slice before them ends.
static double slice_frames(int slot, const Sample *s, int n, int k, float semis) {
    const Track *t = trk(slot);
    double lo, hi; slice_bounds(slot, s, n, k, &lo, &hi);
    double rate = pow(2.0, (t->pitch + t->fine / 100.0 + semis) / 12.0), tempo = song.tempo * tempo_mul;
    double wbpm = t->warp >= 2 ? bpm_of(slot, s) : 0, tr = wbpm > 0 ? tempo / wbpm : 1;
    int warp = wbpm > 0 ? t->warp : 0;
    return (hi - lo) / (warp == 2 ? rate * tr : warp >= 3 ? tr : rate);
}
static void trig_chain(int slot, float vel, float semis, int slice, int delay) {
    start_voice(slot, vel, semis, slice, delay);
    const Sample *s = smp[slot]; const Track *t = trk(slot);
    if (!s || slice < 0 || slot >= NSLOT || t->slices != 6 || song.cut[slot].n < 1) return;
    int n = song.cut[slot].n; uint32_t mask = (uint32_t)song.cut[slot].pos[0];
    double o = delay;
    for (int k = slice % n; k + 1 < n && (mask >> (k + 1) & 1); k++) {
        o += slice_frames(slot, s, n, k, semis);
        start_voice(slot, vel, semis, k + 1, (int)o);
    }
}

int engine_quant32(void) {
    int bar = (song.beats > 0 ? song.beats : 4) * 8, v = song.launch;
    return v == 4 ? 0 : v == 2 ? 2 * bar : v == 3 ? 4 * bar : bar;
}
// A ROLL pattern: each of the RT roll ticks of this 1/32 in time order, its notes lowest first (a mono line ends on the
// highest). Each note is read once through an acquire load of len, so the UI's len-last writes give whole notes.
static void roll_tick(int s, Track *t, Pattern *p, uint32_t i, uint32_t rel, int d, double off, double f32) {
    uint32_t loop = (uint32_t)(p->len ? p->len : 16) * d, pos = rel % loop, lt = loop * RT;
    if (pos % d == 0) play_step[s] = pos / d;
    for (int r = 0; r < RT; r++) {
        Note nb[16]; int idx[16], n = 0; uint32_t at = pos * RT + r;
        for (int k = 0; k < NNOTE && n < 16; k++) {
            uint16_t L = __atomic_load_n(&p->roll[k].len, __ATOMIC_ACQUIRE);
            if (!L) continue;
            Note c = p->roll[k]; c.len = L;
            if (c.start != at) continue;
            int j = n++;
            while (j > 0 && nb[j - 1].note > c.note) { nb[j] = nb[j - 1]; idx[j] = idx[j - 1]; j--; }
            nb[j] = c; idx[j] = k;
        }
        roll_age = age_ctr;
        for (int j = 0; j < n; j++) {
            const Note *nt = &nb[j];
            if (nt->flags & NF_SKIP) { __atomic_fetch_and(&p->roll[idx[j]].flags, (uint8_t)~NF_SKIP, __ATOMIC_RELAXED); continue; }
            if (rand() % 100 >= nt->prob) continue;
            double o = off + r * f32 / RT;
            if (r == 0 && d <= 2 && i % 4 == 2) o += song.swing / 100.0 * f32;   // the same 1/16s as the steps swing
            uint32_t len = nt->len < lt - nt->start ? nt->len : lt - nt->start;   // a note ends with the loop
            roll_gate = synth_gate = (int)(len * f32 / RT); if (roll_gate < 1) roll_gate = synth_gate = 1;
            if (t->type == 2) {              // ACID: a note another one starts inside slides into it
                int sl = 0; uint32_t a = nt->start, b = nt->start + len;
                for (int k = 0; k < NNOTE && !sl; k++) {
                    if (k == idx[j]) continue;
                    uint16_t L = __atomic_load_n(&p->roll[k].len, __ATOMIC_ACQUIRE); uint32_t ms = p->roll[k].start;
                    if (!L || ms >= lt) continue;
                    if (ms < a) ms += lt;
                    sl = ms > a && ms < b;
                }
                acid_flags = (nt->flags & NF_ACC ? ACID_ACCENT : 0) | (sl ? ACID_SLIDE : 0);
            }
            trig_chain(s, nt->vel / 127.f, (float)note_semis_of(s, smp[s], nt->note), -1, (int)o);
            roll_gate = synth_gate = -1; acid_flags = -1;
        }
    }
}
// Follow actions: a clip that just played its pattern AFTER times goes on to another pattern (from its step one) or stops.
static int fol_n[NSLOT], fol_cur[NSLOT];
static int pat_has(const Pattern *p) {
    if (p->mode) { for (int k = 0; k < NNOTE; k++) if (__atomic_load_n(&p->roll[k].len, __ATOMIC_RELAXED)) return 1; return 0; }
    for (int k = 0; k < NSTEP; k++) if (p->trig[k]) return 1;
    return 0;
}
static int fol_pick(int s, int act, int cur) {   // the next pattern, -1 = stop, cur = stay
    const Track *t = &song.t[s]; int used[NPAT], n = 0;
    for (int q = 0; q < NPAT; q++) if (pat_has(&t->pat[q])) used[n++] = q;
    switch (act) {
    case FA_STOP: return -1;
    case FA_JUMP: return song.fol[s][cur][FO_JUMP] % NPAT;
    case FA_NEXT: for (int k = 1; k <= NPAT; k++) { int q = (cur + k) % NPAT; if (pat_has(&t->pat[q])) return q; } return cur;
    case FA_PREV: for (int k = 1; k <= NPAT; k++) { int q = (cur - k + NPAT) % NPAT; if (pat_has(&t->pat[q])) return q; } return cur;
    case FA_FIRST: return n ? used[0] : cur;
    case FA_RANDOM: return n ? used[rand() % n] : cur;
    case FA_OTHER: { if (n < 2) return cur; int q; do q = used[rand() % n]; while (q == cur); return q; }
    }
    return cur;
}
static int follow(int s, Track *t, uint32_t i) {   // at a loop's end: 1 if the clip changed (or stopped)
    const uint8_t *fo = song.fol[s][t->cur];
    if (!fo[FO_A] && !fo[FO_B]) { fol_n[s] = 0; return 0; }
    int after = fo[FO_AFTER] ? fo[FO_AFTER] : 1;
    if (++fol_n[s] < after) return 0;
    fol_n[s] = 0;
    int act = (rand() % 100) < fo[FO_BCH] ? fo[FO_B] : fo[FO_A];
    if (!act) return 0;
    int np = fol_pick(s, act, t->cur);
    if (np < 0) { t->on = 0; play_step[s] = -1; return 1; }
    if (np == t->cur) return 0;
    t->cur = (int8_t)np; fol_cur[s] = np; anchor[s] = i; return 1;
}
static void tick(uint32_t i, double off, double f32) {
    uint32_t bar = (song.beats > 0 ? song.beats : 4) * 8; (void)bar;
    uint32_t q = (uint32_t)engine_quant32(); int edge = !q || i % q == 0;   // a launch quantum starts here
    if (song.metro && i % 8 == 0) click = (typeof(click)){(int)off, 0, i % bar ? 1500.f : 2500.f, 0.35f};
    for (int s = 0; s < NSLOT; s++) {
        Track *t = &song.t[s];
        if (anchor_set[s]) { anchor[s] = anchor_req[s]; anchor_set[s] = 0; }
        if (edge && t->queued >= 0) { t->cur = t->queued; t->queued = -1; t->on = 1; anchor[s] = i; }
        if (edge && want[s] >= 0) { if (want[s]) { t->on = 1; anchor[s] = i; } else t->on = 0; want[s] = -1; }
        frz_seq = -1;
        if (!t->on || i < anchor[s]) continue;
        if (fol_cur[s] != t->cur) { fol_cur[s] = t->cur; fol_n[s] = 0; }   // another pattern (by hand): count again
        const Pattern *p = &t->pat[t->cur];
        int d = div32[p->div % 6];
        uint32_t rel = i - anchor[s];      // steps count from where the clip began
        { uint32_t L = (uint32_t)(p->len ? p->len : 16) * d;
          if (rel > 0 && rel % L == 0 && t->queued < 0 && follow(s, t, i)) {
              if (!t->on) continue;
              p = &t->pat[t->cur]; d = div32[p->div % 6]; rel = 0; }
          if (frz_act[s]) {                // frozen: the recording starts again with each loop; the notes below only mark time
              if (rel % ((uint32_t)(p->len ? p->len : 16) * d) == 0) { frz_at[s] = (int)off; frz_qn[s] = t->cur; }
              frz_seq = s; } }
        if (p->mode) { skip_step[s] = 0; roll_tick(s, t, &t->pat[t->cur], i, rel, d, off, f32); continue; }
        if (rel % d) continue;
        int st = (rel / d) % (p->len ? p->len : 16);
        play_step[s] = st;
        if (skip_step[s] == st + 1) { skip_step[s] = 0; continue; }
        if (!p->trig[st] || rand() % 100 >= p->prob[st]) continue;
        double o = off;
        if (d <= 2 && i % 4 == 2) o += song.swing / 100.0 * f32;
        int r = p->rep[st] < 1 ? 1 : p->rep[st];
        float semis = p->note[st] == NOTE_NONE ? 0 : note_semis_of(s, smp[s], (uint8_t)p->note[st]);   // a step holds a MIDI pitch (2026-11-09)
        int sl = nslices_of(s, smp[s]) ? (p->slice[st] < 0 ? st : p->slice[st]) : -1;
        synth_gate = (int)(d * f32 / r * 0.9);   // a synth note lasts most of its step
        if (t->type == 2) {                 // ACID: the step's ACCENT and SLIDE; a sliding step is held into the next one
            acid_flags = p->slice[st] >= 0 ? p->slice[st] & 3 : 0;
            if (acid_flags & ACID_SLIDE) synth_gate = (int)(d * f32 * 1.15);
            sl = -1;
        }
        for (int k = 0; k < r; k++) trig_chain(s, p->vel[st] / 127.f, semis, sl, (int)(o + k * d * f32 / r));
        synth_gate = -1; acid_flags = -1;
    }
    frz_seq = -1;
}

static inline void read2(const float *d, double pos, float *l, float *r) {
    int a = (int)pos; float f = (float)(pos - a);
    *l = d[2 * a] + (d[2 * a + 2] - d[2 * a]) * f;
    *r = d[2 * a + 1] + (d[2 * a + 3] - d[2 * a + 1]) * f;
}

static void callback(void *u, Uint8 *stream, int len) {
    (void)u;
    // SDL could not raise its audio thread, so other work (drawing, the library check) delayed it and the sound
    // broke up (PipeWire counted errors even at rest). Root may: real-time FIFO, below PipeWire's own 83.
    static int rt_done;
    if (!rt_done) { struct sched_param sp = {.sched_priority = 70}; audio_thread = pthread_self(); rt_done = 1 + pthread_setschedparam(audio_thread, SCHED_FIFO, &sp); audio_rt = rt_done == 1;
#if defined(__aarch64__)
        { uint64_t fpcr; __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr)); fpcr |= 1u << 24; __asm__ volatile("msr fpcr, %0" :: "r"(fpcr)); }   // flush denormals: effect tails never crawl
#endif
    }
    cb_in = SDL_GetTicks() | 1;          // the watchdog sees how long this call takes
    Uint64 t0 = SDL_GetPerformanceCounter();
    if (st_last) { float gp = (float)((double)(t0 - st_last) / SDL_GetPerformanceFrequency() * 1000.0); if (gp > st_gap) st_gap = gp; } st_last = t0;   // for the log: the longest wait between two calls
    Cmd c;
    while (q_pop(&cmdq, &c)) {
        switch (c.type) {
        case CMD_TRIG: start_voice(c.slot, c.a, c.b, (int)(intptr_t)c.p, 0); break;
        case CMD_TRIG_CHAIN: trig_chain(c.slot, c.a, c.b, (int)(intptr_t)c.p, 0); break;
        case CMD_TRIG_HELD: {   // p: slice in the low 16 bits (signed), the key's tag above (0 = OPTION's tag 1)
            intptr_t pv = (intptr_t)c.p; int tag = (int)(pv >> 16); if (!tag) tag = 1;
            start_voice(c.slot, c.a, c.b, (int16_t)(pv & 0xffff), 0);
            if (arp_last >= 0) { ArpNote *e = &arp[arp_last / ARPN][arp_last % ARPN]; e->tag = tag; e->left = -1; arp_last = -1; break; }   // ARP: held while the key is
            if (last_voice >= 0 && voices[last_voice].synth == 3) { voices[last_voice].held = tag; voices[last_voice].cl.gate = -1; }
            else if (last_voice >= 0) { voices[last_voice].held = tag; if (voices[last_voice].synth) { if (voices[last_voice].sy.pat >= 0) voices[last_voice].sy.pg = -1; else voices[last_voice].sy.gate = -1; } }
            break; }
        case CMD_RELEASE:
            for (int s = 0; s < NSLOT; s++) for (int j = 0; j < arp_n[s]; j++) if (arp[s][j].tag && arp[s][j].tag == (c.slot ? c.slot : 1)) { arp[s][j].left = 0; arp[s][j].tag = 0; }
            for (int i = 0; i < NVOICE; i++) if (voices[i].on && voices[i].held && voices[i].held == (c.slot ? c.slot : 1)) {
                voices[i].held = 0;
                if (voices[i].synth == 3) voices[i].cl.stage = 3;
                else if (voices[i].synth) voices[i].sy.stage = 3; else voices[i].cut_in = 0;   // a synth goes into its release
            }
            break;
        case CMD_SET_SAMPLE:
            for (int i = 0; i < NVOICE; i++) if (voices[i].slot == c.slot) voices[i].on = 0;
            if (smp[c.slot]) q_push(&trashq, (Cmd){0, 0, 0, 0, smp[c.slot]});
            smp[c.slot] = c.p; break;
        case CMD_COUNT:
            playing = 0; pos32 = 0; next32 = 0; count_next = 0; count_left = c.slot;
            for (int s = 0; s < NSLOT; s++) { play_step[s] = -1; skip_step[s] = 0; }
            break;
        case CMD_TRANSPORT:
            count_left = -1;
            playing = (int)c.a; pos32 = 0; next32 = 0;
            for (int s = 0; s < NSLOT; s++) for (int j = 0; j < NLFO; j++) if (song.lfo[s][j].sync) lfos[s][j].ph = 0;   // synced LFOs start with the bar
            for (int s = 0; s < NSLOT; s++) { play_step[s] = -1; skip_step[s] = 0; anchor[s] = 0; want[s] = -1; anchor_set[s] = 0; }
            if (c.slot > 0 && c.slot <= NSLOT) skip_step[c.slot - 1] = (int)c.b + 1;   // a hit that started it: heard already
            break;
        case CMD_STOP_ALL: for (int s = 0; s < NSLOT; s++) arp_n[s] = 0;
            for (int i = 0; i < NVOICE; i++) if (voices[i].on) { voices[i].fout = 0; if (voices[i].delay > 0) voices[i].on = 0; else if (voices[i].cut_in < 0) voices[i].cut_in = 0; } break;   // a stop is at once, whatever FADE OUT says
        }
    }
    float *o = (float *)stream; int n = len / 8;
    memset(o, 0, len);
    if (n > MAXN) n = MAXN;
    lfo_run(n, song.tempo * tempo_mul);
    double tempo = song.tempo * tempo_mul; if (tempo < 20) tempo = 20;
    if (count_left >= 0 && !playing) {
        double beat = SR * 60.0 / tempo;
        while (count_next < n) {
            if (count_left == 0) { playing = 1; pos32 = 0; next32 = count_next; count_left = -1; break; }
            click = (typeof(click)){(int)count_next, 0, 1500.f, 0.35f};
            count_left--; count_next += beat;
        }
        if (count_left >= 0) count_next -= n;
    }
    if (playing) {
        double f32 = SR * 60.0 / tempo / 8;
        while (next32 < n) { tick(pos32, next32, f32); pos32++; next32 += f32; }
        next32 -= n;
    }
    for (int s = 0; s < NSLOT; s++) if (arp_n[s]) arp_run(s, n);   // ARP: this buffer's notes
    for (int s = 0; s < NSLOT; s++) {      // GRAND DEEP: each GRAND pad's LFO, shared by its voices as on the mono original
        const int8_t *gx = has_mod[s] ? m_gx[s] : song.gx[s]; const uint8_t (*gp)[2] = (const uint8_t (*)[2])song.gp[s];
        gp_on[s] = song.t[s].type == 1 && gp_any(gp);
        gl_on[s] = song.t[s].type == 1 && (gx_lfo_on(gx) || (gp_on[s] && (gp_uses(gp, JO_LFO) || gp_uses(gp, JO_SH))));
        if (!gl_on[s]) continue;
        double hz = gx[GX_LSYNC] ? tempo / 60.0 / div_beats[(gx[GX_LSYNC] - 1) % NDIV] : gx_lfo_hz(gx[GX_LRATE]), inc = hz / SR;
        if (gp_on[s]) for (int c = 0; c < 8; c++) if (gp[c][1] == JI_LRATE && gp[c][0]) {   // a cable into LFO RATE: the newest voice's value, 1.0 = 3 octaves faster
            float lr = 0; unsigned ag = 0; for (int i = 0; i < NVOICE; i++) if (voices[i].on && voices[i].synth == 1 && voices[i].slot == s && voices[i].age >= ag) { ag = voices[i].age; lr = voices[i].sy.lri; }
            if (lr > 2) lr = 2; if (lr < -2) lr = -2; inc *= exp2(3.0 * lr); if (inc > 200.0 / SR) inc = 200.0 / SR; break; }
        for (int k = 0; k < n && k < 1024; k++) {
            double ph = gl_ph[s]; float y;
            switch (gx[GX_LWAVE] % 5) { case 0: y = (float)(ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph); break; case 1: y = ph < 0.5 ? 1.f : -1.f; break;
                case 2: y = (float)(1 - 2 * ph); break; case 3: y = (float)(2 * ph - 1); break; default: y = gl_sh[s]; }
            gl_buf[s][k] = y; gl_ph[s] += inc;
            if (gl_ph[s] >= 1) { gl_ph[s] -= 1; gl_sh[s] = (float)((rand() % 2001) - 1000) / 1000.f; }   // S&H: a new value each cycle
        }
        gl_now[s] = gl_buf[s][0];
    }
    int active = 0;
    for (int i = 0; i < NVOICE; i++) {
        Voice *v = &voices[i];
        if (!v->on) continue;
        active++;
        if (v->slot < NSLOT && trk(v->slot)->mute) {     // mute also stops what already sounds
            if (v->delay > 0) { v->on = 0; continue; }
            if (v->cut_in < 0) v->cut_in = 0;
        }
        if (v->delay >= n) { v->delay -= n; if (v->cut_in > 0) v->cut_in = v->cut_in > n ? v->cut_in - n : 0; continue; }
        int k = v->delay; if (v->cut_in > 0) v->cut_in = v->cut_in > k ? v->cut_in - k : 0; v->delay = 0;
        double tr = v->wbpm > 0 ? tempo / v->wbpm : 1;   // warp follows the live tempo
        if (v->warp == 2) v->rate = v->base * tr * (v->rate < 0 ? -1 : 1);
        const float *d = v->s ? v->s->d : NULL;
        // GRAND: what this voice needs, decided once per buffer, not per frame
        const int8_t *vgx = v->synth == 1 && v->slot < NSLOT ? (has_mod[v->slot] ? m_gx[v->slot] : song.gx[v->slot]) : NULL;
        int vpatch = vgx && gp_on[v->slot]; const float *vlfo = vgx && gl_on[v->slot] ? gl_buf[v->slot] : NULL; float vsh = vgx ? gl_sh[v->slot] : 0;
        if (vgx && vgx[GX_MARK] != GX_INIT) vgx = gx_def;
        if (vgx && vgx[GX_VCA] == 2 && v->sy.stage == 3 && !v->held && (!playing || !song.t[v->slot].on)) v->sy.stage = 4;   // DRONE lasts while the clip plays
        float *b = bus[v->slot <= NSLOT ? v->slot : NSLOT]; bus_live[v->slot <= NSLOT ? v->slot : NSLOT] = 1;
        double einv = 0; int smp_v = !v->synth, cutf = smp_v && v->fout > FADE_FRAMES ? v->fout : FADE_FRAMES;
        if (smp_v) { v->adv = v->warp >= 3 ? tr : fabs(v->rate) * (v->slot < NSLOT ? mod_pitch[v->slot] : 1); if (v->fout > 0 && v->adv > 0) einv = 1.0 / (v->adv * v->fout); }
        for (; k < n; k++) {
            if (v->cut_in == 0 && v->fstep == 0) {
                v->fstep = 1.f / cutf;
                if (smp_v && v->adv > 0) {       // a cut fades over FADE OUT / XFADE: with XFADE it may read on past its end for that
                    double rem = (v->rate > 0 ? v->hi - 1 - v->pos : v->pos - v->lo) / v->adv;
                    if (rem < cutf && v->slot < NSLOT && song.smx[v->slot][SX_XFADE] > 0) {
                        double need = (cutf - rem + 2) * v->adv;
                        if (v->rate > 0) { v->hi += need; if (v->hi > v->s->n) v->hi = v->s->n; } else { v->lo -= need; if (v->lo < 0) v->lo = 0; }
                        rem = (v->rate > 0 ? v->hi - 1 - v->pos : v->pos - v->lo) / v->adv + 2;
                    }
                    if (rem >= cutf) v->cutx = 1;
                }
            } else if (v->cut_in > 0) v->cut_in--;
            float l, r;
            if (v->synth == 3) {         // CLOUD: stereo grains
                if (!cl_frame(&v->cl, v->s, v->slot < NSLOT && has_mod[v->slot] ? m_cp[v->slot] : trk(v->slot)->cp, v->slot < NSLOT && has_mod[v->slot] ? m_cx[v->slot] : song.cx[v->slot], track_scale(v->slot), SR * 60.0 / tempo * (song.beats > 0 ? song.beats : 4), &l, &r)) { v->on = 0; break; }
            } else if (v->synth) {
                float x;
                int md = v->slot < NSLOT && has_mod[v->slot]; float pm = md ? mod_pitch[v->slot] : 1, f0 = v->sy.freq;
                if (pm != 1) { v->sy.freq = f0 * pm; v->sy.target *= pm; if (v->sy.pat >= 0) v->sy.pf *= pm; }   // pitch modulation around the note: GLIDE keeps working underneath
                int alive = v->synth == 2 ? acid_frame(&v->sy, md ? m_ap[v->slot] : trk(v->slot)->ap, md ? m_ax[v->slot] : song.ax[v->slot], &x)
                          : vpatch ? synth_frame_patch(&v->sy, md ? m_sp[v->slot] : trk(v->slot)->sp, vgx, (const uint8_t (*)[2])song.gp[v->slot], vlfo ? vlfo[k] : 0, vsh, &x)
                          : synth_frame(&v->sy, md ? m_sp[v->slot] : trk(v->slot)->sp, vgx, vlfo ? vlfo[k] : 0, &x);
                if (pm != 1) { v->sy.freq /= pm; v->sy.target /= pm; if (v->sy.pat >= 0) v->sy.pf /= pm; }
                if (!alive) { v->on = 0; break; }
                l = r = x;
            } else if (v->warp == 5) {   // COMPLEX: phase vocoder, input moves by tr, pitch by rate
                if (v->pos >= v->hi - 1) { v->on = 0; break; }
                pv_read(&pvs[i], v->s, (int)v->lo, (int)v->hi, v->rate * (v->slot < NSLOT ? mod_pitch[v->slot] : 1), PV_H * tr / v->rate, &l, &r);
                v->pos += tr;
            } else if (v->warp >= 3) {   // TONES/TEXTURE: two Hann grains at pitch rate, restarted at the stretched position
                if (v->pos >= v->hi - 1) { v->on = 0; break; }
                l = r = 0;
                for (int g = 0; g < 2; g++) {
                    double p = v->gst[g] + v->gph[g] * v->rate * (v->slot < NSLOT ? mod_pitch[v->slot] : 1);
                    if (p < v->hi - 1) {
                        float w = 0.5f - 0.5f * cosf(2 * (float)M_PI * v->gph[g] / v->gsize), a, b;
                        read2(d, p, &a, &b); l += a * w; r += b * w;
                    }
                    if (++v->gph[g] >= v->gsize) {
                        v->gph[g] = 0; v->gst[g] = v->pos;
                        if (v->flux) {   // TEXTURE: random offset up to flux% of a grain
                            v->gst[g] += ((rand() % 2001) - 1000) / 1000.0 * v->flux / 100.0 * v->gsize;
                            if (v->gst[g] < v->lo) v->gst[g] = v->lo;
                        }
                    }
                }
                v->pos += tr;
            } else {
                if (v->rate > 0 ? v->pos >= v->hi - 1 : v->pos <= v->lo) { v->on = 0; break; }
                read2(d, v->pos, &l, &r);
                v->pos += v->rate * (v->slot < NSLOT ? mod_pitch[v->slot] : 1);
            }
            float eg = v->fade;
            if (smp_v) {                     // SAMPLER DEEP: FADE IN from the start, FADE OUT before the end of what plays
                if (v->el < v->fin) eg *= (float)v->el / v->fin;
                v->el++;
                if (einv > 0 && !v->cutx) { double rm = (v->rate > 0 ? v->hi - 1 - v->pos : v->pos - v->lo) * einv; if (rm < 1) eg *= rm < 0 ? 0 : (float)rm; }
            }
            b[2 * k] += l * v->gl * eg; b[2 * k + 1] += r * v->gr * eg;
            if (v->fstep) { v->fade -= v->fstep; if (v->fade <= 0) { v->on = 0; break; } }
        }
    }
    {   // NORMALIZE: the pad's sample brought up to level, before the sidechain copies and the strip; a change glides over one buffer
        static float ng[NSLOT];
        for (int s = 0; s < NSLOT; s++) {
            int m = song.mix.pad[s].spare[PM_NORM], ty = song.t[s].type; const Sample *x = smp[s];
            float g = (ty == 0 || ty == 3) && x && m > 0 ? (m == 1 ? x->npk : x->nld) : 1.f, a = ng[s] > 0 ? ng[s] : 1.f;
            if (g <= 0) g = 1.f;
            ng[s] = g;
            if (!bus_live[s] || (g == 1.f && a == 1.f)) continue;
            float st = (g - a) / n, *b = bus[s];
            for (int k = 0; k < n; k++) { a += st; b[2 * k] *= a; b[2 * k + 1] *= a; }
        }
    }
    for (int s = 0; s < NSLOT; s++) {      // FREEZE: this buffer of each frozen pad's recording
        frz_snd[s] = 0;
        if (!frz_act[s]) { frz_g[s] = 0; frz_at[s] = -1; frz_pos[s] = -1; continue; }
        int act = playing && song.t[s].on, at = frz_at[s]; frz_at[s] = -1;
        if (!act && frz_g[s] <= 0) { frz_pos[s] = -1; continue; }
        if (act && frz_g[s] <= 0) frz_g[s] = 1;    // a start is not faded in: the first hit keeps its attack
        float g = frz_g[s], dg = act ? 0 : -g / n, *fb = frz_out[s]; long p = frz_pos[s]; const FrzBuf *F = &frz[s][frz_q[s] % NPAT];
        for (int k = 0; k < n; k++) {
            if (k == at) { p = 0; frz_q[s] = frz_qn[s]; F = &frz[s][frz_q[s] % NPAT]; }
            g += dg;
            if (F->d && p >= 0 && p < F->n) { fb[2 * k] = F->d[2 * p] * g; fb[2 * k + 1] = F->d[2 * p + 1] * g; } else fb[2 * k] = fb[2 * k + 1] = 0;
            if (p >= 0) p++;
        }
        frz_pos[s] = p; frz_g[s] = act ? 1 : 0; frz_snd[s] = 1;
    }
    for (int s = 0; s < NSLOT; s++) if (dk_srcs >> s & 1) {   // DUCK by sound: how loud each source pad is in this buffer
        float pk = 0;
        if (bus_live[s]) for (int k = 0; k < 2 * n; k++) { float a = fabsf(bus[s][k]); if (a > pk) pk = a; }
        if (frz_snd[s]) for (int k = 0; k < 2 * n; k++) { float a = fabsf(frz_out[s][k]); if (a > pk) pk = a; }
        dk_pk[s] = pk;
    }
    int solo_any = 0, budget = FX_BUDGET;
    {   // sidechain: copies of the key pads' sound, before any strip changes it
        unsigned need = 0;
        for (int s = 0; s < NSLOT; s++) for (int i = 0; i < NINS; i++) { int kp = song.mbx[s][i][5]; const FxSlot *fs = &song.mix.pad[s].ins[i]; if (fs->type == FX_COMP && fs->on && kp > 0 && kp <= NSLOT) need |= 1u << (kp - 1); }
        for (int s = 0; s < NSLOT; s++) if (need >> s & 1) { if (bus_live[s]) memcpy(keyb[s], bus[s], sizeof(float) * 2 * n); else memset(keyb[s], 0, sizeof(float) * 2 * n);
            if (frz_snd[s]) for (int k = 0; k < 2 * n; k++) keyb[s][k] += frz_out[s][k]; }   // a frozen key pad: its recording is the key
    }
    for (int s = 0; s < NSLOT; s++) solo_any |= song.mix.pad[s].solo;
    memset(snd_a, 0, sizeof(float) * 2 * n); memset(snd_b, 0, sizeof(float) * 2 * n);
    for (int s = 0; s < NSLOT; s++) {   // the strips: inserts, VOL/PAN/MUTE/SOLO (ramped), meter, sends
        const Track *t = &song.t[s]; const PadMix *pm = &song.mix.pad[s];
        for (int i = 0; i < NINS; i++) {
            const FxSlot *fs = &pm->ins[i]; int on = fs->on && fs->type && fs->type < FX_N;
            if (on && fx_weight[fs->type] > budget) on = 0; else if (on) budget -= fx_weight[fs->type];
            fx_live[s][i] = (uint8_t)on;
        }
        float pv = t->pan / 50.f + m_pan[s]; pv = pv < -1 ? -1 : pv > 1 ? 1 : pv;
        float gv = t->vol / 100.f + m_vol[s]; gv = gv < 0 ? 0 : gv > 1 ? 1 : gv; gv *= dk_g[s] > 0 ? dk_g[s] : 0;
        float pa = (pv + 1) * (float)M_PI / 4, g = t->mute || (solo_any && !pm->solo) ? 0 : gv;
        float tl = g * cosf(pa) * 1.41421f, tr = g * sinf(pa) * 1.41421f;
        float ea = pm->send_a / 100.f + m_sa[s], eb = pm->send_b / 100.f + m_sb[s]; ea = ea < 0 ? 0 : ea > 1 ? 1 : ea; eb = eb < 0 ? 0 : eb > 1 ? 1 : eb;
        float ta = ea * ea, tb = eb * eb;
        int spr_on = t->type == 1 && song.gx[s][GX_MARK] == GX_INIT && song.gx[s][GX_SPRING] > 0;
        int idle = 0;
        if (bus_live[s]) { strip_tail[s] = 4096; spr_tail[s] = spr_on ? SR * 4 : 0; }
        else if (strip_tail[s] > 0 && (fxs[s][0].xf > 0 || fxs[s][1].xf > 0)) strip_tail[s] -= n;
        else if (spr_on && spr_tail[s] > 0) spr_tail[s] -= n;   // the spring rings on after the last note
        else { strip_tail[s] = 0; idle = 1; }
        if (idle && fcap_s == s && fcap_buf) { long m = fcap_max - fcap_n < n ? fcap_max - fcap_n : n; if (m > 0) { memset(fcap_buf + 2 * fcap_n, 0, sizeof(float) * 2 * m); fcap_n += m; } }
        if (idle && !frz_snd[s]) { strip_gl[s] = tl; strip_gr[s] = tr; send_ga[s] = ta; send_gb[s] = tb; continue; }
        float *b = bus[s];
        if (!idle) {                           // (a frozen pad's inserts run only for what is played by hand on top)
        for (int i = 0; i < NINS; i++) fx_key[i] = NULL;
        for (int i = 0; i < NINS; i++) { int kp = song.mbx[s][i][5]; if (pm->ins[i].type == FX_COMP && kp > 0 && kp <= NSLOT && kp - 1 != s) fx_key[i] = keyb[kp - 1]; }   // a compressor listening to another pad (sidechain)
        if (spr_on) spring_run(&spr[s], b, n, has_mod[s] ? m_gx[s][GX_SPRING] : song.gx[s][GX_SPRING]);
        for (int i = 0; i < NINS; i++) { fx_keybuf = fx_key[i]; fx_insert_ex(&fxs[s][i], has_mod[s] ? &m_ins[s][i] : &pm->ins[i], song.mbx[s][i], fx_live[s][i], b, n, tempo); fx_keybuf = NULL; }
        { float chk = 0; for (int k = 0; k < 2 * n; k += 16) chk += b[k];
          if (!isfinite(chk)) { memset(b, 0, sizeof(float) * 2 * n); memset(fxs[s], 0, sizeof fxs[s]); } }   // an effect blew up: silence, start clean
        if (fcap_s == s && fcap_buf) { long m = fcap_max - fcap_n < n ? fcap_max - fcap_n : n; if (m > 0) { memcpy(fcap_buf + 2 * fcap_n, b, sizeof(float) * 2 * m); fcap_n += m; } }
        }
        if (frz_snd[s]) for (int k = 0; k < 2 * n; k++) b[k] += frz_out[s][k];
        float l0 = strip_gl[s], r0 = strip_gr[s], dl = (tl - l0) / n, dr = (tr - r0) / n, pk = 0;
        float a0 = send_ga[s], b0 = send_gb[s], da = (ta - a0) / n, db = (tb - b0) / n;
        for (int k = 0; k < n; k++) {
            float L = b[2 * k] * (l0 + dl * (k + 1)), R = b[2 * k + 1] * (r0 + dr * (k + 1));
            o[2 * k] += L; o[2 * k + 1] += R;
            float sa = a0 + da * (k + 1), sb = b0 + db * (k + 1);
            snd_a[2 * k] += L * sa; snd_a[2 * k + 1] += R * sa; snd_b[2 * k] += L * sb; snd_b[2 * k + 1] += R * sb;
            float m = fabsf(L) > fabsf(R) ? fabsf(L) : fabsf(R); if (m > pk) pk = m;
        }
        strip_gl[s] = tl; strip_gr[s] = tr; send_ga[s] = ta; send_gb[s] = tb; if (pk > mpk[s]) mpk[s] = pk;
        memset(b, 0, sizeof(float) * 2 * n); bus_live[s] = 0;
    }
    for (int rr = 0; rr < 2; rr++) {     // the returns: A reverb, B delay
        memset(ret, 0, sizeof(float) * 2 * n);
        if (rr == 0) reverb_run(&rvb, song.mix.rv, snd_a, ret, n); else delay_run(&dly, song.mix.dl, tempo, snd_b, ret, n);
        float pk = 0, chk = 0;
        for (int k = 0; k < 2 * n; k++) { chk += ret[k]; float a = fabsf(ret[k]); if (a > pk) pk = a; }
        if (!isfinite(chk)) { if (rr == 0) memset(&rvb, 0, sizeof rvb); else memset(&dly, 0, sizeof dly); continue; }
        for (int k = 0; k < 2 * n; k++) o[k] += ret[k];
        if (pk > mpk[NSLOT + rr]) mpk[NSLOT + rr] = pk;
    }
    if (bus_live[NSLOT]) { for (int k = 0; k < 2 * n; k++) o[k] += bus[NSLOT][k]; memset(bus[NSLOT], 0, sizeof(float) * 2 * n); bus_live[NSLOT] = 0; }
    pv_level();
    if (click.f) {
        for (int k = click.delay < 0 ? 0 : click.delay; k < n && click.t < 2400; k++, click.t++) {
            float x = sinf(click.t * click.f * 2 * (float)M_PI / SR) * click.a * expf(-click.t / 300.f);
            o[2 * k] += x; o[2 * k + 1] += x;
        }
        click.delay -= n;
        if (click.t >= 2400) click.f = 0;
    }
    {   // master: VOL (ramped), compressor, limiter, then the soft clip as a last safety
        float tg = song.master / 100.f; if (master_g < 0) master_g = tg;
        float dg = (tg - master_g) / n;
        for (int k = 0; k < n; k++) { float g = master_g + dg * (k + 1); o[2 * k] *= g; o[2 * k + 1] *= g; }
        master_g = tg;
        master_run(&mst, song.mix.ms, o, n);
    }
    float pk = 0;
    for (int k = 0; k < 2 * n; k++) {
        float x = o[k], ax = fabsf(x);
        if (ax > 0.9f) x = copysignf(0.9f + 0.1f * tanhf((ax - 0.9f) * 10), x);
        o[k] = x; if (ax > pk) pk = ax;
    }
    if (pk > peak_acc) peak_acc = pk;
    if (pk > mpk[NSLOT + 2]) mpk[NSLOT + 2] = pk;
    nvoices = active; frames_obt = n;
    float ld = (float)((double)(SDL_GetPerformanceCounter() - t0) / SDL_GetPerformanceFrequency() * SR / n);
    load_avg += (ld - load_avg) * 0.02f;
    if (ld > st_max) st_max = ld; if (ld > 1) st_over++; st_n++;
    cb_in = 0;
}
// A real-time thread that never returns would hold a core for good. The UI calls this every frame: a call
// longer than 250 ms loses the real-time priority (once), and the caller logs it.
int engine_watchdog(uint32_t now) {
    Uint32 t = cb_in;
    if (!audio_rt || !t || (Sint32)(now - t) < 250) return 0;
    struct sched_param sp = {.sched_priority = 0};
    pthread_setschedparam(audio_thread, SCHED_OTHER, &sp); audio_rt = 0;
    return (int)(now - t);
}

int engine_start(int buffer_frames) {
    engine_warm(); cl_tables();
    for (int s = 0; s < NSLOT; s++) play_step[s] = -1;
    SDL_AudioSpec want = {0}, got;
    want.freq = SR; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = buffer_frames;
    want.callback = callback;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (!dev) return -1;
    SDL_PauseAudioDevice(dev, 0);
    return 0;
}
void engine_stop(void) { if (dev) SDL_CloseAudioDevice(dev); dev = 0; }

static void send(Cmd c) { while (!q_push(&cmdq, c)) SDL_Delay(1); }
void engine_trig(int slot, float vel, float semis, int slice) { send((Cmd){CMD_TRIG, slot, vel, semis, (void *)(intptr_t)slice}); }
void engine_trig_chain(int slot, float vel, float semis, int slice) { send((Cmd){CMD_TRIG_CHAIN, slot, vel, semis, (void *)(intptr_t)slice}); }
void engine_trig_held(int slot, float vel, float semis, int slice) { send((Cmd){CMD_TRIG_HELD, slot, vel, semis, (void *)(intptr_t)(slice & 0xffff)}); }
void engine_release(void) { send((Cmd){CMD_RELEASE, 1, 0, 0, 0}); }
void engine_trig_key(int slot, float vel, float semis, int slice, int tag) {
    send((Cmd){CMD_TRIG_HELD, slot, vel, semis, (void *)(((intptr_t)tag << 16) | (slice & 0xffff))});
}
int engine_synth_live(int slot, float *env, float *fc) {   // the newest sounding synth voice of a slot (UI reads, may race harmlessly)
    int best = -1; unsigned age = 0;
    for (int i = 0; i < NVOICE; i++) if (voices[i].on && voices[i].synth && voices[i].synth != 3 && voices[i].slot == slot && voices[i].delay <= 0 && voices[i].age >= age) { age = voices[i].age; best = i; }
    if (best < 0) return 0;
    const SynthV *y = &voices[best].sy;
    *env = voices[best].synth == 2 ? y->menv : y->env;
    *fc = voices[best].synth == 2 ? acid_cutoff(trk(slot)->ap, song.ax[slot], y->menv, y->accap) : sy_cutoff(trk(slot)->sp, y->env, y->freq);
    return 1;
}
int engine_sample_live(int slot, float *pos, int max) {   // UI reads, may race harmlessly
    int n = 0;
    for (int i = 0; i < NVOICE && n < max; i++) { const Voice *v = &voices[i];
        if (v->on && !v->synth && v->slot == slot && v->delay <= 0 && v->s && v->s->n > 0) { float f = (float)(v->pos / v->s->n); if (f >= 0 && f <= 1) pos[n++] = f; } }
    return n;
}
int engine_cloud_grains(int slot, float *pos, int max, float *env, float *scan) {   // the newest cloud of a slot: where its grains were taken (0..1)
    int best = -1; unsigned age = 0;
    for (int i = 0; i < NVOICE; i++) if (voices[i].on && voices[i].synth == 3 && voices[i].slot == slot && voices[i].delay <= 0 && voices[i].age >= age) { age = voices[i].age; best = i; }
    if (best < 0) return 0;
    const CloudV *c = &voices[best].cl; const Sample *s = voices[best].s; int n = 0;
    for (int g = 0; g < CL_GRAINS && n < max; g++) if (c->g[g].len && s && s->n > 0) pos[n++] = c->g[g].off;   // its place in the band, from the centre (POSITION + MOTION)
    *env = c->env; *scan = (float)c->scan;
    return n;
}
void engine_release_key(int tag) { send((Cmd){CMD_RELEASE, tag, 0, 0, 0}); }
void engine_set_sample(int slot, Sample *s) { ui_samples[slot] = s; send((Cmd){CMD_SET_SAMPLE, slot, 0, 0, s}); }
void engine_launch(int slot, int on) { if (slot >= 0 && slot < NSLOT) want[slot] = (int8_t)on; }
int engine_fol_n(int slot) { return slot >= 0 && slot < NSLOT ? fol_n[slot] : 0; }
void engine_pause(int on) { if (dev) SDL_PauseAudioDevice(dev, on); }
float engine_duck_view(int slot, float *gain) { if (gain) *gain = dk_g[slot]; return dk_view[slot]; }   // where along its curve the pad's dip is (0..1, -1 at rest)
void engine_lock(int on) { if (dev) { if (on) SDL_LockAudioDevice(dev); else SDL_UnlockAudioDevice(dev); } }
float *engine_freeze_buf(int slot, int pat, float *d, int n) { float *o = frz[slot][pat].d; frz[slot][pat].n = d ? n : 0; frz[slot][pat].d = d; return o; }
const float *engine_freeze_get(int slot, int pat, int *n) { *n = frz[slot][pat].n; return frz[slot][pat].d; }
void engine_freeze_on(int slot, int on) { frz_act[slot] = (uint8_t)on; if (!on) { frz_g[slot] = 0; frz_pos[slot] = -1; frz_at[slot] = -1; } }
void engine_capture(int slot, float *buf, long max) { fcap_s = slot; fcap_buf = buf; fcap_max = max; fcap_n = 0; }
long engine_captured(void) { return fcap_n; }
void engine_render(float *out, int frames) { callback(0, (Uint8 *)out, frames * 2 * (int)sizeof(float)); }
float engine_glfo(int slot) { return slot >= 0 && slot < NSLOT && gl_on[slot] ? gl_now[slot] : 0; }
int engine_want(int slot) { return slot >= 0 && slot < NSLOT ? want[slot] : -1; }
uint32_t engine_anchor(int slot) { return slot >= 0 && slot < NSLOT ? anchor[slot] : 0; }
void engine_set_anchor(int slot, uint32_t pos) { if (slot >= 0 && slot < NSLOT) { anchor_req[slot] = pos; anchor_set[slot] = 1; } }
void engine_transport_skip(int slot, int step) { send((Cmd){CMD_TRANSPORT, slot + 1, 1.f, (float)step, 0}); }
void engine_transport(int play) { send((Cmd){CMD_TRANSPORT, 0, (float)play, 0, 0}); }
void engine_stop_all(void) { send((Cmd){CMD_STOP_ALL, 0, 0, 0, 0}); }
void engine_collect_garbage(void) { Cmd c; while (q_pop(&trashq, &c)) sample_free(c.p); }
float engine_peak(void) { float p = peak_acc; peak_acc = 0; return p; }
float engine_master_gr(void) { return master_gr; }
float engine_lfo(int s, int j) { return lfo_view[s][j]; }
float engine_auto(int s, int a) { return auto_view[s][a]; }
void engine_fx_view(int slot, int i, float *v) { memcpy(v, fxs[slot][i].view, 6 * sizeof *v); }
int engine_fx_live(int slot, int i) { return fx_live[slot][i]; }
void engine_meters(float *out) { for (int i = 0; i < NSLOT + 3; i++) { out[i] = mpk[i]; mpk[i] = 0; } }   // pads, return A, B, master: peaks since the last call
float engine_load(void) { return load_avg; }
// What the audio thread did since the last call (the UI logs it every 30 s): the dearest call as a share of its time,
// how many calls ran over it (each is a gap in the sound), the longest wait between two calls in ms, calls counted.
void engine_stats(float *mx, int *over, float *gap, int *n) { *mx = st_max; *over = st_over; *gap = st_gap; *n = st_n; st_max = 0; st_over = 0; st_gap = 0; st_n = 0; }
int engine_voices(void) { return nvoices; }
int engine_frames(void) { return frames_obt; }
int engine_playing(void) { return playing; }
int engine_step(int slot) { return play_step[slot]; }
double engine_posf(void) {
    double tempo = song.tempo * tempo_mul; if (tempo < 20) tempo = 20;
    double p = pos32 - next32 / (SR * 60.0 / tempo / 8);
    return p < 0 ? 0 : p;
}
void engine_count(int beats) { send((Cmd){CMD_COUNT, beats, 0, 0, 0}); }
int engine_counting(void) { return count_left; }
double engine_count_left(void) {
    double tempo = song.tempo * tempo_mul; if (tempo < 20) tempo = 20;
    return count_left * 8 + count_next / (SR * 60.0 / tempo / 8);
}
void engine_skip(int slot, int step) { skip_step[slot] = step + 1; }
int engine_step32(const Pattern *p) { return div32[p->div % 6]; }
uint32_t engine_pos32(void) { return pos32; }
const Sample *engine_sample(int slot) { return ui_samples[slot]; }
