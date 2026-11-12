// WAV/OGG loading -> stereo float @ SR.
#include "bp.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define STB_VORBIS_NO_PUSHDATA_API
#include "../third_party/stb_vorbis.c"

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return p[0] | p[1] << 8; }

// Converts interleaved src (ch channels, sr) into a Sample at SR stereo (linear resampling).
static Sample *finish(const float *src, int n, int ch, int sr, const char *path) {
    if (n <= 0 || ch <= 0 || sr <= 0) return NULL;
    double step = (double)sr / SR;
    int m = (int)((n - 1) / step) + 1;
    Sample *s = calloc(1, sizeof *s);
    s->d = malloc(sizeof(float) * 2 * (size_t)m);
    if (!s->d) { free(s); return NULL; }
    for (int i = 0; i < m; i++) {
        double p = i * step; int a = (int)p; float f = (float)(p - a); int b = a + 1 < n ? a + 1 : a;
        for (int c = 0; c < 2; c++) {
            int cc = c < ch ? c : 0;
            float x = src[a * ch + cc], y = src[b * ch + cc];
            s->d[2 * i + c] = x + (y - x) * f;
        }
    }
    s->n = m;
    snprintf(s->path, sizeof s->path, "%s", path);
    const char *b = strrchr(path, '/'); b = b ? b + 1 : path;
    snprintf(s->name, sizeof s->name, "%s", b);
    char *dot = strrchr(s->name, '.'); if (dot) *dot = 0;
    return s;
}

static Sample *load_wav(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 44 || sz > 256L << 20) { fclose(f); return NULL; }
    uint8_t *buf = malloc(sz);
    if (fread(buf, 1, sz, f) != (size_t)sz) { fclose(f); free(buf); return NULL; }
    fclose(f);
    Sample *out = NULL;
    if (memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) goto done;
    int fmt = 0, ch = 0, sr = 0, bits = 0; const uint8_t *data = NULL; uint32_t dlen = 0;
    for (long p = 12; p + 8 <= sz;) {
        uint32_t len = rd32(buf + p + 4); const uint8_t *c = buf + p + 8;
        if (!memcmp(buf + p, "fmt ", 4) && len >= 16 && p + 8 + 16 <= sz) {
            fmt = rd16(c); ch = rd16(c + 2); sr = rd32(c + 4); bits = rd16(c + 14);
            if (fmt == 0xFFFE && len >= 26 && p + 8 + 26 <= sz) fmt = rd16(c + 24);
        } else if (!memcmp(buf + p, "data", 4)) {
            data = c; dlen = len; if (p + 8 + (long)dlen > sz) dlen = sz - p - 8;
        }
        p += 8 + (long)len + (len & 1);   // long: a length near 4 GB must not wrap to 0 and loop forever
    }
    if (!data || !ch || ch > 32 || sr < 1000 || sr > 768000 || bits < 8 || bits > 32) goto done;
    int bps = bits / 8, n = dlen / (bps * ch);
    float *tmp = malloc(sizeof(float) * (size_t)n * ch);
    if (!tmp) goto done;
    for (long i = 0; i < (long)n * ch; i++) {
        const uint8_t *q = data + i * bps; float v = 0;
        if (fmt == 3 && bits == 32) { memcpy(&v, q, 4); }
        else if (fmt == 1 && bits == 8) v = (q[0] - 128) / 128.f;
        else if (fmt == 1 && bits == 16) v = (int16_t)rd16(q) / 32768.f;
        else if (fmt == 1 && bits == 24) v = ((int32_t)(q[0] << 8 | q[1] << 16 | (uint32_t)q[2] << 24) >> 8) / 8388608.f;
        else if (fmt == 1 && bits == 32) v = (int32_t)rd32(q) / 2147483648.f;
        else { free(tmp); goto done; }
        tmp[i] = v;
    }
    out = finish(tmp, n, ch, sr, path);
    free(tmp);
done:
    free(buf);
    return out;
}

static Sample *load_ogg(const char *path) {
    int ch, sr; short *pcm;
    int n = stb_vorbis_decode_filename(path, &ch, &sr, &pcm);
    if (n <= 0) return NULL;
    float *tmp = malloc(sizeof(float) * (size_t)n * ch);
    if (!tmp) { free(pcm); return NULL; }
    for (long i = 0; i < (long)n * ch; i++) tmp[i] = pcm[i] / 32768.f;
    free(pcm);
    Sample *s = finish(tmp, n, ch, sr, path);
    free(tmp);
    return s;
}


#include <ctype.h>
#include <math.h>

static int has_word(const char *low, const char *const *w) {
    for (; *w; w++) if (strstr(low, *w)) return 1;
    return 0;
}
// A whole word of the path: "breaks" and "breakbeat" count as "break", "breakcore" or "override" do not.
static int has_whole_word(const char *low, const char *const *w) {
    for (; *w; w++)
        for (const char *p = low; (p = strstr(p, *w)); p++) {
            const char *e = p + strlen(*w);
            if (p > low && isalnum((unsigned char)p[-1])) continue;
            if (*e == 's') e++; else if (!strncmp(e, "beat", 4)) { e += 4; if (*e == 's') e++; }
            if (!isalnum((unsigned char)*e)) return 1;
        }
    return 0;
}

// Root from a name token like Am, F#min, Cmaj, D#m7, C3 (letter must be uppercase). *mode: 2 minor, 1 major, 0 a note.
static int root_from_name_mode(const char *name, int *mode) {
    static const int pc[7] = {9, 11, 0, 2, 4, 5, 7};  // A..G
    char buf[64]; snprintf(buf, sizeof buf, "%s", name);
    int found = -1;
    for (char *tok = strtok(buf, "_- .()[]"); tok; tok = strtok(NULL, "_- .()[]")) {
        if (tok[0] < 'A' || tok[0] > 'G') continue;
        int r = pc[tok[0] - 'A']; const char *q = tok + 1;
        if (*q == '#') { r++; q++; } else if (*q == 'b') { r--; q++; }
        int md = 0;
        if (!strncmp(q, "minor", 5)) { q += 5; md = 2; } else if (!strncmp(q, "major", 5)) { q += 5; md = 1; }
        else if (!strncmp(q, "min", 3)) { q += 3; md = 2; } else if (!strncmp(q, "maj", 3)) { q += 3; md = 1; }
        else if (*q == 'm') { q++; md = 2; } else if (*q == 'M') { q++; md = 1; }
        while (isdigit((unsigned char)*q)) q++;
        if (!*q) { found = (r + 12) % 12; if (mode) *mode = md; }
    }
    return found;
}
static int root_from_name(const char *name) { return root_from_name_mode(name, NULL); }
static void root_by_sound(Sample *s);
// What the sound alone says, whatever the name: for tests and --analyze.
void sample_sound_root(const Sample *s, int *root, int *mode) {
    Sample t = *s; t.root = -1; t.mode = 0; t.root_by = 0;
    root_by_sound(&t); *root = t.root; *mode = t.mode;
}

// ---------- root by sound (when the name says nothing) ----------
// A short one-shot is a note: harmonic sum over a semitone grid, its pitch class wins. Anything longer or a
// loop gets a key: a chromagram against the Krumhansl-Kessler major and minor profiles. Unclear sound (noise,
// drums, a flat chroma) stays unknown: no transposition is safer than a wrong one.
#define KN 4096
static void key_fft(float *re, float *im) {
    for (int i = 1, j = 0; i < KN; i++) {
        int b = KN >> 1; for (; j & b; b >>= 1) j ^= b;
        j |= b;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= KN; len <<= 1) {
        double a = -2 * M_PI / len; float wr = (float)cos(a), wi = (float)sin(a);
        for (int i = 0; i < KN; i += len) {
            float cr = 1, ci = 0;
            for (int j = 0; j < len / 2; j++) {
                int p = i + j, q = p + len / 2;
                float xr = re[q] * cr - im[q] * ci, xi = re[q] * ci + im[q] * cr;
                re[q] = re[p] - xr; im[q] = im[p] - xi; re[p] += xr; im[p] += xi;
                float t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
}
// Magnitude spectra of the first nf frames (frame f covers [f*KN, f*KN + KN)), two real frames per complex FFT.
static int key_spectra(const Sample *s, int nf, float *mag /* nf x KN/2 */) {
    static float re[KN], im[KN], win[KN]; static int ready;
    if (!ready) { for (int n = 0; n < KN; n++) win[n] = 0.5f - 0.5f * cosf(2 * (float)M_PI * n / KN); ready = 1; }
    for (int f = 0; f < nf; f += 2) {
        for (int n = 0; n < KN; n++) {
            long a = (long)f * KN + n, b = a + KN;
            re[n] = a < s->n ? (s->d[2 * a] + s->d[2 * a + 1]) * win[n] : 0;
            im[n] = f + 1 < nf && b < s->n ? (s->d[2 * b] + s->d[2 * b + 1]) * win[n] : 0;
        }
        key_fft(re, im);
        for (int k = 0; k < KN / 2; k++) {
            int m = (KN - k) & (KN - 1);
            float ar = (re[k] + re[m]) * .5f, ai = (im[k] - im[m]) * .5f, br = (im[k] + im[m]) * .5f, bi = (re[m] - re[k]) * .5f;
            mag[(long)f * (KN / 2) + k] = sqrtf(ar * ar + ai * ai);
            if (f + 1 < nf) mag[(long)(f + 1) * (KN / 2) + k] = sqrtf(br * br + bi * bi);
        }
    }
    return nf;
}
static float key_corr(const float *x, const float *p, int rot) {
    double mx = 0, mp = 0, sxy = 0, sxx = 0, spp = 0;
    for (int i = 0; i < 12; i++) { mx += x[i]; mp += p[i]; }
    mx /= 12; mp /= 12;
    for (int i = 0; i < 12; i++) {
        double a = x[(i + rot) % 12] - mx, b = p[i] - mp;
        sxy += a * b; sxx += a * a; spp += b * b;
    }
    return sxx > 0 ? (float)(sxy / sqrt(sxx * spp)) : 0;
}
// Pitch salience of one frame on a semitone grid (MIDI 24..108): each spectral peak, at its interpolated
// frequency, counts for the notes it can be a harmonic of (h = 1..8, weight 0.8^(h-1)). A tone's own
// harmonics pile up on its fundamental; overtones alone do not make a note.
static void frame_salience(const float *m, float *sal) {
    const float bin = (float)SR / KN; float top = 0;
    for (int k = 2; k < KN / 2 - 1; k++) if (m[k] > top) top = m[k];
    if (top <= 0) return;
    for (int k = 2; k < KN / 2 - 1; k++) {
        if (!(m[k] > m[k - 1] && m[k] >= m[k + 1] && m[k] > top * 0.01f)) continue;   // peaks down to -40 dB
        float a = logf(m[k - 1] + 1e-9f), b = logf(m[k] + 1e-9f), c = logf(m[k + 1] + 1e-9f), den = a - 2 * b + c;
        float p = den < 0 ? 0.5f * (a - c) / den : 0, fr = (k + p) * bin, amp = sqrtf(m[k] / top), w = 1;
        if (fr < 30 || fr > 5000) continue;
        for (int h = 1; h <= 8; h++, w *= 0.8f) {
            float f0 = fr / h; if (f0 < 30) break;
            float midi = 69 + 12 * log2f(f0 / 440); int r = (int)lrintf(midi); float d = fabsf(midi - r);
            if (d < 0.4f && r >= 24 && r <= 108) sal[r] += w * amp * (1 - d / 0.4f);
        }
    }
}
float key_last_corr;                    // the last decision's strength (tests read it)
float key_last_pc[12], key_last_bass[12];   // the chroma behind it, and its part below 260 Hz (tests read them)
static void root_by_sound(Sample *s) {
    static const float maj[12] = {6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};
    static const float minr[12] = {6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};
    float secs = s->n / (float)SR;
    int note = !s->loop && secs <= 4.f && s->nons <= 2;   // one attack: a note; several: a phrase, it has a key
    int nf = s->n / KN; if (nf > 140) nf = 140;            // 12 s is enough to hear a key
    if (note && nf > 18) nf = 18;                         // 1.5 s: the note, not its tail
    if (nf < 2) return;
    float *mag = malloc(sizeof(float) * (size_t)nf * (KN / 2));
    if (!mag) return;
    nf = key_spectra(s, nf, mag);
    key_last_corr = 0;
    float sal[128] = {0}, pc[12] = {0};
    for (int f = 0; f < nf; f++) {
        float fs[128] = {0}, tot = 0;
        frame_salience(mag + (long)f * (KN / 2), fs);
        for (int i = 0; i < 128; i++) tot += fs[i];
        if (tot <= 0) continue;
        for (int i = 0; i < 128; i++) sal[i] += fs[i] / tot;   // every frame counts the same, loud or quiet
    }
    free(mag);
    for (int i = 24; i <= 108; i++) pc[i % 12] += sal[i];
    for (int i = 0; i < 12; i++) { key_last_pc[i] = pc[i]; key_last_bass[i] = 0; }
    for (int i = 24; i < 60; i++) key_last_bass[i % 12] += sal[i];
    if (note) {
        float best = 0, tot = 0; int b = -1;
        for (int i = 0; i < 12; i++) { tot += pc[i]; if (pc[i] > best) { best = pc[i]; b = i; } }
        key_last_corr = tot > 0 ? best / (tot / 12) : 0;
        if (b >= 0 && key_last_corr > 2.5f) { s->root = b; s->mode = 0; s->root_by = 2; }   // a clear winner only
    } else {
        float best = -2; int bt = -1, bm = 0;
        for (int t = 0; t < 12; t++)
            for (int md = 1; md <= 2; md++) {
                float r = key_corr(pc, md == 1 ? maj : minr, t);
                if (r > best) { best = r; bt = t; bm = md; }
            }
        key_last_corr = best;
        if (bt >= 0 && best > 0.6f) { s->root = bt; s->mode = bm; s->root_by = 2; }
    }
}

static float bpm_from(const char *low) {
    for (const char *p = low; (p = strstr(p, "bpm")); p++) {      // "150 bpm", "150bpm"
        const char *q = p; while (q > low && q[-1] == ' ') q--;
        const char *e = q; while (q > low && (isdigit((unsigned char)q[-1]) || q[-1] == '.')) q--;   // 107.4bpm
        while (q < e && *q == '.') q++;
        if (e - q >= 2) { float b = (float)atof(q); if (b >= 50 && b <= 250) return b; }
    }
    const char *b = strrchr(low, '/'); b = b ? b + 1 : low;       // "110_..." at the start
    if (isdigit((unsigned char)b[0]) && isdigit((unsigned char)b[1])) {
        float v = (float)atof(b); if (v >= 60 && v <= 200) return v;
    }
    return 0;
}

// NORMALIZE (2026-11-11): what to multiply the sample by. PEAK: its loudest point comes to -1 dB. LOUD: the mean
// level of its loud parts (blocks within 20 dB of the loudest block) comes to -15 dB, but never louder than PEAK,
// so nothing clips. At most +36 dB: a file of near silence is not turned into noise.
static void sample_levels(Sample *s) {
    enum { B = 2048 };
    float pk = 0, top = 0; double sum = 0; int cnt = 0;
    s->npk = s->nld = 1.f;
    for (int i = 0; i < 2 * s->n; i++) { float v = fabsf(s->d[i]); if (v > pk) pk = v; }
    if (pk < 1e-4f) return;
    for (int pass = 0; pass < 2; pass++)
        for (int a = 0; a < s->n; a += B) {
            int e = a + B < s->n ? a + B : s->n; double m = 0;
            for (int i = 2 * a; i < 2 * e; i++) m += (double)s->d[i] * s->d[i];
            m /= 2.0 * (e - a);
            if (!pass) { if (m > top) top = (float)m; } else if (m >= top * 0.01f) { sum += m; cnt++; }
        }
    float g = 0.891f / pk, rms = cnt ? sqrtf((float)(sum / cnt)) : 0;
    s->npk = g > 63.f ? 63.f : g;
    s->nld = rms > 1e-5f && 0.178f / rms < s->npk ? 0.178f / rms : s->npk;
}
static void detect_onsets(Sample *s) {
    const int hop = 256; int nh = s->n / hop;
    s->ons[0] = 0; s->nons = 1;
    if (nh < 8) return;
    float *e = malloc(sizeof(float) * nh), *fx = malloc(sizeof(float) * nh);
    for (int h = 0; h < nh; h++) {   // energy of the first difference = emphasises attacks
        double acc = 0;
        for (int k = h * hop + 1; k < (h + 1) * hop; k++) {
            float m = s->d[2 * k] + s->d[2 * k + 1], m0 = s->d[2 * k - 2] + s->d[2 * k - 1];
            acc += (m - m0) * (m - m0);
        }
        e[h] = logf((float)acc + 1e-6f);
    }
    double mean = 0, sd = 0;
    for (int h = 0; h < nh; h++) { fx[h] = h ? fmaxf(0, e[h] - e[h - 1]) : 0; mean += fx[h]; }
    mean /= nh;
    for (int h = 0; h < nh; h++) sd += (fx[h] - mean) * (fx[h] - mean);
    sd = sqrt(sd / nh);
    int gap = SR / 16 / hop, last = 0;   // >= 62 ms apart
    for (int h = 1; h < nh - 1 && s->nons < 32; h++)
        if (fx[h] > mean + 1.5 * sd && fx[h] >= fx[h - 1] && fx[h] >= fx[h + 1] && h - last >= gap) {
            int f = h * hop - hop / 2; if (f < 0) f = 0;
            if (f > SR / 50) s->ons[s->nons++] = f;
            last = h;
        }
    free(e); free(fx);
}

static void analyze(Sample *s) {
    static const char *const drums[] = {"kick", "snare", "hat", "clap", "perc", "tom", "cymbal", "drum", "break",
                                        "rim", "crash", "ride", "shaker", "cowbell", "conga", "bongo", "808", "909", 0};
    static const char *const loops[] = {"loop", "break", "beat", "groove", "fill", "top", "arp", "sequence", 0};
    char low[512]; int i;
    for (i = 0; s->path[i] && i < 511; i++) low[i] = (char)tolower((unsigned char)s->path[i]);
    low[i] = 0;
    const char *base = strrchr(low, '/'); base = base ? base + 1 : low;
    static const char *const folder[] = {"hihat", "hh", "oneshot drums", 0};
    char dir[512]; snprintf(dir, sizeof dir, "%.*s", (int)(base - low), low);   // the folders only
    s->drum = has_word(base, drums) || has_whole_word(dir, drums) || has_whole_word(dir, folder);   // folders: whole words
    s->mode = 0; s->root_by = 0;
    s->root = s->drum ? -1 : root_from_name_mode(s->name, &s->mode);
    if (s->root >= 0) s->root_by = 1;
    float bpm = bpm_from(base); if (!bpm) bpm = bpm_from(low);
    float secs = s->n / (float)SR;
    s->loop = (has_word(base, loops) || bpm > 0) && secs > 0.5f;
    s->bars = 0; s->bpm = 0;
    if (s->loop) {
        if (bpm > 0) { s->bpm = bpm; s->bars = (int)(secs * bpm / 240 + 0.5f); if (s->bars < 1) s->bars = 1; }
        else {
            for (int b = 1; b <= 16; b *= 2) { float v = b * 240 / secs; if (v >= 86 && v < 172) { s->bars = b; s->bpm = v; break; } }
            if (!s->bars) { s->bars = (int)(secs / 2 + 0.5f); if (s->bars < 1) s->bars = 1; s->bpm = s->bars * 240 / secs; }
        }
    }
    if (secs > 0.3f) detect_onsets(s); else { s->ons[0] = 0; s->nons = 1; }
    if (!s->drum && s->root < 0 && secs > 0.15f) root_by_sound(s);
    sample_levels(s);
}

static int is_audio(const char *nm) {
    const char *e = strrchr(nm, '.');
    return nm[0] != '.' && e && (!strcasecmp(e, ".wav") || !strcasecmp(e, ".ogg"));
}

static Sample *load_any(const char *path) {
    const char *e = strrchr(path, '.');
    if (!e) return NULL;
    return !strcasecmp(e, ".ogg") ? load_ogg(path) : load_wav(path);
}
static int audio_ok(const Sample *s) {   // too short, silent or garbage in the data counts as broken
    if (s->n < 2) return 0;
    float peak = 0;
    for (long i = 0; i < 2L * s->n; i++) {
        float v = s->d[i];
        if (!isfinite(v)) return 0;
        if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    return peak >= 0.0005f;
}
Sample *sample_load(const char *path) {
    Sample *s = load_any(path);
    if (s && !audio_ok(s)) { sample_free(s); s = NULL; }
    if (s) analyze(s);
    return s;
}
int sample_verify(const char *path) {    // no name analysis: safe on another thread
    Sample *s = load_any(path);
    int ok = s && audio_ok(s);
    if (s) sample_free(s);
    return ok;
}
void sample_name_info(const char *path, float *bpm, int *root) {
    const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
    char low[256]; int i;
    for (i = 0; base[i] && i < 255; i++) low[i] = (char)tolower((unsigned char)base[i]);
    low[i] = 0;
    *bpm = bpm_from(low);
    *root = root_from_name(base);
}

void sample_free(Sample *s) { if (s) { free(s->d); free(s); } }

static int cmp(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    if (x->dir != y->dir) return y->dir - x->dir;
    return strcasecmp(x->name, y->name);
}

static int has_ext(const char *nm, const char *ext) { const char *e = strrchr(nm, '.'); return e && !strcasecmp(e, ext); }
int list_dir(const char *dir, Entry *out, int max) { return list_dir_ext(dir, out, max, NULL); }
int list_dir_ext(const char *dir, Entry *out, int max, const char *ext) {   // ext NULL: audio files
    DIR *d = opendir(dir); if (!d) return 0;
    int n = 0; struct dirent *e;
    while ((e = readdir(d)) && n < max) {
        if (e->d_name[0] == '.') continue;
        int isdir = e->d_type == DT_DIR;
        if (e->d_type == DT_UNKNOWN) {
            char p[1024]; struct stat st; snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
            isdir = !stat(p, &st) && S_ISDIR(st.st_mode);
        }
        if (!isdir && (ext ? !has_ext(e->d_name, ext) : !is_audio(e->d_name))) continue;
        snprintf(out[n].name, sizeof out[n].name, "%s", e->d_name); out[n++].dir = isdir;
    }
    closedir(d);
    qsort(out, n, sizeof *out, cmp);
    return n;
}
