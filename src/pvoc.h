// WARP COMPLEX: a phase vocoder with identity phase locking (Laroche & Dolson), included by engine.c only.
// Time and pitch are independent: the vocoder stretches by tr / rate into a stream y, the voice then reads y
// at the pitch rate. Stereo keeps its image: both channels get the phase change of the mid signal, so one
// complex FFT carries L and R together. At a transient of the sample (Sample.ons) the phases restart from
// the analysis, so attacks stay sharp. Runs in the audio callback: no allocation, no locks.
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#define PV_N 2048                // window, 43 ms at 48 kHz
#define PV_H 512                 // synthesis hop, 4x overlap
#define PV_B (PV_N / 2 + 1)      // bins
#define PV_YR 8192               // ring for the stretched stream, power of two
#define PV_NORM (1.f / 1.5f)
#define PV_LOAD 6.0              // COMPLEX at once, each voice counted by its pitch rate (at least 1): about 0.8 ms of 2.67
#define PV_BUDGET 6              // FFTs per buffer, about 1.3 ms; past it frames are plain     // Hann analysis * Hann synthesis at 4x overlap sums to 1.5

typedef struct {
    double ta;                   // centre of the next analysis frame, in sample frames
    int ta_prev;                 // centre of the previous one (rounded), INT32_MIN: none yet
    long yw;                     // frames of y that are final
    double yp, ha;               // read position in y; the analysis hop the voice asked for last
    int nh;                      // frames made so far
    int stale;                   // a plain frame was made: the next one restarts the phases
    float pa[PV_B], ps[PV_B];    // previous analysis / synthesis phase of the mid signal
    float acc[2][PV_N];          // overlap-add
    float ws[PV_N];              // window weight that reached acc so far: 1 once every frame is in
    float y[2][PV_YR];
} PVoc;

static PVoc pvs[NVOICE];
static float pv_win[PV_N], pv_twr[PV_N], pv_twi[PV_N];   // twiddles of stage len at len/2 + j
static int pv_rev[PV_N], pv_ready;
static int pv_cost, pv_cost_max, pv_cost_last;  // work done in this buffer: 1 per FFT; the most in one buffer
static float pv_xl[PV_N], pv_xr[PV_N], pv_re[PV_N], pv_im[PV_N], pv_mag[PV_B], pv_ph[PV_B], pv_rot[PV_B][2];
static int pv_peak[PV_B];

static void pv_tables(void) {
    if (pv_ready) return;
    for (int n = 0; n < PV_N; n++) pv_win[n] = 0.5f - 0.5f * cosf(2 * (float)M_PI * n / PV_N);
    for (int len = 2; len <= PV_N; len <<= 1)
        for (int j = 0; j < len / 2; j++) { pv_twr[len / 2 + j] = cosf(2 * (float)M_PI * j / len); pv_twi[len / 2 + j] = sinf(2 * (float)M_PI * j / len); }
    int bits = 0; while ((1 << bits) < PV_N) bits++;
    for (int n = 0; n < PV_N; n++) { int r = 0; for (int b = 0; b < bits; b++) if (n >> b & 1) r |= 1 << (bits - 1 - b); pv_rev[n] = r; }
    pv_ready = 1;
}
static void pv_fft(float *re, float *im, int inv) {   // in place, radix 2; inverse without the 1/N
    for (int n = 0; n < PV_N; n++) {
        int r = pv_rev[n];
        if (r > n) { float t = re[n]; re[n] = re[r]; re[r] = t; t = im[n]; im[n] = im[r]; im[r] = t; }
    }
    for (int n = 0; n < PV_N; n += 2) {   // first stage: no twiddle
        float ar = re[n], ai = im[n], br = re[n + 1], bi = im[n + 1];
        re[n] = ar + br; im[n] = ai + bi; re[n + 1] = ar - br; im[n + 1] = ai - bi;
    }
    float sg = inv ? 1.f : -1.f;
    for (int len = 4; len <= PV_N; len <<= 1) {
        int half = len >> 1; const float *wr = pv_twr + half, *wi = pv_twi + half;
        for (int i = 0; i < PV_N; i += len) {
            float *restrict ar = re + i, *restrict ai = im + i, *restrict br = re + i + half, *restrict bi = im + i + half;
            int j = 0;
#ifdef __ARM_NEON
            float32x4_t vs = vdupq_n_f32(sg);
            for (; j + 4 <= half; j += 4) {
                float32x4_t c = vld1q_f32(wr + j), sn = vmulq_f32(vs, vld1q_f32(wi + j));
                float32x4_t xr0 = vld1q_f32(br + j), xi0 = vld1q_f32(bi + j), yr = vld1q_f32(ar + j), yi = vld1q_f32(ai + j);
                float32x4_t xr = vmlsq_f32(vmulq_f32(xr0, c), xi0, sn), xi = vmlaq_f32(vmulq_f32(xr0, sn), xi0, c);
                vst1q_f32(br + j, vsubq_f32(yr, xr)); vst1q_f32(bi + j, vsubq_f32(yi, xi));
                vst1q_f32(ar + j, vaddq_f32(yr, xr)); vst1q_f32(ai + j, vaddq_f32(yi, xi));
            }
#endif
            for (; j < half; j++) {
                float c = wr[j], s = sg * wi[j];
                float xr = br[j] * c - bi[j] * s, xi = br[j] * s + bi[j] * c;
                br[j] = ar[j] - xr; bi[j] = ai[j] - xi; ar[j] += xr; ai[j] += xi;
            }
        }
    }
}
static inline float pv_atan2(float y, float x) {   // max error 2e-4 rad, twice as fast as atan2f
    float ax = fabsf(x), ay = fabsf(y), mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
    if (mx == 0) return 0;
    float a = mn / mx, q = a * a;
    float r = ((-0.0464964749f * q + 0.15931422f) * q - 0.327622764f) * q * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    return y < 0 ? -r : r;
}
static inline float pv_wrap(float x) { return x - 2 * (float)M_PI * floorf((x + (float)M_PI) / (2 * (float)M_PI)); }

static void engine_warm(void) {   // tables and every page of the voice state, before the audio starts
    pv_tables(); memset(pvs, 0, sizeof pvs);
}
static void pv_init(PVoc *p, double lo) {
    pv_tables();
    p->ta = lo - (PV_N / 2 - PV_H);       // the first frame just reaches lo; the second is centred on it
    p->ta_prev = INT32_MIN; p->yw = 0; p->yp = PV_N - PV_H; p->nh = 0; p->ha = PV_H; p->stale = 0;   // y position of lo
    memset(p->acc, 0, sizeof p->acc); memset(p->ws, 0, sizeof p->ws);
}

// One frame: analyse at p->ta, place it PV_H after the last one, then move the analysis by ha.
// Frame 0 only reaches lo: plain, no FFT. Frame 1 is centred on lo and starts the phases (forward FFT only),
// from frame 2 on the hop is the stretched one, so timing is exact from the first sample. A frame on a
// transient needs no inverse FFT either.
static void pv_hop(PVoc *p, const Sample *s, int lo, int hi, double ha, int cheap) {
    int c = (int)lrint(p->ta), i0 = c - PV_N / 2;
    const float *d = s->d;
    for (int n = 0; n < PV_N; n++) {
        int k = i0 + n;
        if (k >= lo && k < hi) { pv_xl[n] = d[2 * k] * pv_win[n]; pv_xr[n] = d[2 * k + 1] * pv_win[n]; }
        else pv_xl[n] = pv_xr[n] = 0;
    }
    // Over budget: a plain frame (the windowed input, like TONES) costs no FFT; the sound gets rougher for
    // a moment but never late. The frame after it restarts the phases.
    int plain = p->nh < 1 || cheap;
    int reset = p->nh <= 1 || p->stale || plain, ha_eff = p->ta_prev == INT32_MIN ? PV_H : c - p->ta_prev;
    if (ha_eff < 1) ha_eff = 1;
    if (!reset) for (int k = 1; k < s->nons; k++) if (s->ons[k] > p->ta_prev && s->ons[k] <= c) { reset = 1; break; }
    pv_cost += plain ? 0 : reset ? 1 : 2;
    for (int n = 0; n < PV_N; n++) p->ws[n] += pv_win[n] * pv_win[n] * PV_NORM;
    if (!plain) {
        memcpy(pv_re, pv_xl, sizeof pv_re); memcpy(pv_im, pv_xr, sizeof pv_im);
        pv_fft(pv_re, pv_im, 0);
        // mid spectrum: L + R, where X = FFT(L + iR) gives L_k = (X_k + X*_{N-k}) / 2 and R_k = (X_k - X*_{N-k}) / 2i
        for (int k = 0; k < PV_B; k++) {
            int m = (PV_N - k) & (PV_N - 1);
            float lr = (pv_re[k] + pv_re[m]) * .5f, li = (pv_im[k] - pv_im[m]) * .5f;
            float rr = (pv_im[k] + pv_im[m]) * .5f, ri = (pv_re[m] - pv_re[k]) * .5f;
            float mr = lr + rr, mi = li + ri;
            pv_mag[k] = mr * mr + mi * mi; pv_ph[k] = pv_atan2(mi, mr);
            if (pv_mag[k] < 1e-12f) pv_mag[k] = 0;
        }
    }
    if (reset) {
        if (!plain) memcpy(p->ps, pv_ph, sizeof p->ps);
        for (int n = 0; n < PV_N; n++) { p->acc[0][n] += pv_xl[n] * pv_win[n] * PV_NORM; p->acc[1][n] += pv_xr[n] * pv_win[n] * PV_NORM; }
    } else {
        int np = 0;
        for (int k = 2; k < PV_B - 2; k++)
            if (pv_mag[k] > 0 && pv_mag[k] > pv_mag[k - 1] && pv_mag[k] >= pv_mag[k + 1] && pv_mag[k] > pv_mag[k - 2] && pv_mag[k] >= pv_mag[k + 2])
                pv_peak[np++] = k;
        float hs_ha = (float)PV_H / ha_eff;
        int from = 0;
        for (int q = 0; q < np; q++) {
            int k = pv_peak[q], to = q + 1 < np ? (k + pv_peak[q + 1]) / 2 : PV_B - 1;   // bins up to halfway belong to this peak
            float w = 2 * (float)M_PI * k / PV_N;
            float dev = pv_wrap(pv_ph[k] - p->pa[k] - ha_eff * w);
            float sp = p->ps[k] + hs_ha * (ha_eff * w + dev), th = sp - pv_ph[k];
            float cr = cosf(th), ci = sinf(th);
            for (int j = from; j <= to; j++) { pv_rot[j][0] = cr; pv_rot[j][1] = ci; p->ps[j] = pv_wrap(pv_ph[j] + th); }
            from = to + 1;
        }
        for (int j = from; j < PV_B; j++) { pv_rot[j][0] = 1; pv_rot[j][1] = 0; p->ps[j] = pv_ph[j]; }
        pv_rot[0][0] = pv_rot[PV_B - 1][0] = 1; pv_rot[0][1] = pv_rot[PV_B - 1][1] = 0;   // DC and Nyquist stay real
        // rotate L and R by the same angle: Y = L' + iR' keeps both channels in one inverse FFT
        for (int k = 0; k < PV_B; k++) {
            int m = (PV_N - k) & (PV_N - 1);
            float lr = (pv_re[k] + pv_re[m]) * .5f, li = (pv_im[k] - pv_im[m]) * .5f;
            float rr = (pv_im[k] + pv_im[m]) * .5f, ri = (pv_re[m] - pv_re[k]) * .5f;
            float cr = pv_rot[k][0], ci = pv_rot[k][1];
            float a = lr * cr - li * ci, b = lr * ci + li * cr;      // L'
            float e = rr * cr - ri * ci, f = rr * ci + ri * cr;      // R'
            pv_re[k] = a - f; pv_im[k] = b + e;                      // L' + iR'
            if (m != k) { pv_re[m] = a + f; pv_im[m] = e - b; }      // conj(L') + i conj(R')
        }
        pv_fft(pv_re, pv_im, 1);
        float g = PV_NORM / PV_N;
        for (int n = 0; n < PV_N; n++) { p->acc[0][n] += pv_re[n] * pv_win[n] * g; p->acc[1][n] += pv_im[n] * pv_win[n] * g; }
    }
    if (!plain) memcpy(p->pa, pv_ph, sizeof p->pa);
    p->stale = plain;
    for (int n = 0; n < PV_H; n++) {
        long w = (p->yw + n) & (PV_YR - 1);
        p->y[0][w] = p->acc[0][n]; p->y[1][w] = p->acc[1][n];
    }
    p->yw += PV_H;
    for (int ch = 0; ch < 2; ch++) { memmove(p->acc[ch], p->acc[ch] + PV_H, (PV_N - PV_H) * sizeof(float)); memset(p->acc[ch] + PV_N - PV_H, 0, PV_H * sizeof(float)); }
    memmove(p->ws, p->ws + PV_H, (PV_N - PV_H) * sizeof(float)); memset(p->ws + PV_N - PV_H, 0, PV_H * sizeof(float));
    p->ta_prev = c;
    p->ta += p->nh < 1 ? PV_H : ha;   // frame 0 to frame 1 lands exactly on lo
    p->nh++;
}

// Next output frame at pitch rate; ha = analysis hop for the current tempo ratio.
static inline void pv_read(PVoc *p, const Sample *s, int lo, int hi, double rate, double ha, float *l, float *r) {
    p->ha = ha;
    long a = (long)p->yp; float f = (float)(p->yp - a);
    while (a + 1 >= p->yw) {
        // A new voice plays from the overlap-add so far, scaled by the window weight it has: its first frames are
        // plain, so that is the input itself, and the start costs one FFT.
        long i = a - p->yw;
        if (p->nh <= 2 && i >= 0 && i + 1 < PV_N && p->ws[i] >= 0.05f && p->ws[i + 1] >= 0.05f) {
            float l0 = p->acc[0][i] / p->ws[i], l1 = p->acc[0][i + 1] / p->ws[i + 1];
            float r0 = p->acc[1][i] / p->ws[i], r1 = p->acc[1][i + 1] / p->ws[i + 1];
            *l = l0 + (l1 - l0) * f; *r = r0 + (r1 - r0) * f;
            p->yp += rate; return;
        }
        pv_hop(p, s, lo, hi, ha, pv_cost >= PV_BUDGET);
    }
    long i = a & (PV_YR - 1), j = (a + 1) & (PV_YR - 1);
    *l = p->y[0][i] + (p->y[0][j] - p->y[0][i]) * f;
    *r = p->y[1][i] + (p->y[1][j] - p->y[1][i]) * f;
    p->yp += rate;
}

// Spread the work: after the mix, voices that are least ahead make a frame in advance with what is left of the
// budget, so chords that started together do not all need their next frame in the same buffer.
#define PV_AHEAD (3 * PV_H)
static void pv_level(void) {
    while (pv_cost < PV_BUDGET) {
        int best = -1; double lead = PV_AHEAD;
        for (int i = 0; i < NVOICE; i++) {
            const Voice *v = &voices[i];
            if (!v->on || v->warp != 5 || v->delay > 0 || v->fstep) continue;
            double l = pvs[i].yw - pvs[i].yp - (pvs[i].nh <= 2 ? PV_N : 0);   // a voice still on its start frames first
            if (l < lead) { lead = l; best = i; }
        }
        if (best < 0) break;
        const Voice *v = &voices[best];
        pv_hop(&pvs[best], v->s, (int)v->lo, (int)v->hi, pvs[best].ha, 0);
    }
    if (pv_cost > pv_cost_max) pv_cost_max = pv_cost;
    pv_cost_last = pv_cost;
    pv_cost = 0;
}
