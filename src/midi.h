// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// MIDI (2026-11-19, user: MIDI sync and control). Raw ALSA ports (/dev/snd/midiCxDy), no library. One thread reads the
// port and sends the clock; what it reads goes through a ring to the UI thread (midi_poll in main.c, at most 5 ms late),
// which plays the notes as KEYS or PADS do. The port is remembered by its card's name (midi.cfg beside the binary):
// card numbers change from boot to boot. CLOCK OUT: Start, 24 clocks a beat from the audio position, Stop.
#pragma once
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>

#define MIDI_MAXDEV 8
typedef struct { char path[32], name[24]; } MidiDev;
static MidiDev midi_dev[MIDI_MAXDEV]; static int midi_ndev;
static char midi_want[24];               // the card's name chosen in PROJECT ("" = off)
static int midi_clk_out, midi_notes, midi_ch, midi_clk_in;   // ... CLOCK IN: BrickPad follows another device's clock   // CLOCK OUT on/off; NOTES 0 KEYS (the open pad), 1 PADS (36..47 = pads 1-12); CH 0 ALL, 1..16
static int midi_fd = -1; static char midi_open_name[24];
static volatile int midi_run; static pthread_t midi_th;
static uint8_t midi_ring[256][3]; static volatile unsigned midi_wr, midi_rd;   // the reader writes, the UI reads
// CLOCK IN: the reader times the clocks (ns); the UI turns them into the tempo and keeps the phase (midi_follow in main.c)
static volatile int64_t mclk_last_ns, mclk_per_ns; static volatile long mclk_count;   // the last clock, the smoothed period, clocks since Start
static int64_t mono_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec; }
static void midi_push(uint8_t a, uint8_t b, uint8_t c) {
    unsigned w = midi_wr; if (w - midi_rd >= 256) return;
    midi_ring[w & 255][0] = a; midi_ring[w & 255][1] = b; midi_ring[w & 255][2] = c; __atomic_store_n(&midi_wr, w + 1, __ATOMIC_RELEASE);
}
// CC -> a parameter of a pad (MIDI LEARN: R2 on a SOUND card, then turn the knob). midimap.cfg beside the binary.
#define MMAP_N 64
typedef struct { uint8_t ch, cc, pad; int16_t par; } MidiMap;
static MidiMap mmap_[MMAP_N]; static int mmap_n, mlearn_pad = -1, mlearn_par = -1;
static void mmap_save(void) { char b[MMAP_N * 20 + 8]; int n = 0; for (int i = 0; i < mmap_n; i++) n += snprintf(b + n, sizeof b - n, "%d %d %d %d\n", mmap_[i].ch, mmap_[i].cc, mmap_[i].pad, mmap_[i].par); write_whole("midimap.cfg", b, n); }
static void mmap_load(void) { FILE *f = fopen("midimap.cfg", "r"); if (!f) return; int a, b, c, d; mmap_n = 0;
    while (mmap_n < MMAP_N && fscanf(f, "%d %d %d %d", &a, &b, &c, &d) == 4) if (c >= 0 && c < NSLOT && b >= 0 && b < 128) mmap_[mmap_n++] = (MidiMap){(uint8_t)a, (uint8_t)b, (uint8_t)c, (int16_t)d}; fclose(f); }
static int mmap_find(int pad, int par) { for (int i = 0; i < mmap_n; i++) if (mmap_[i].pad == pad && mmap_[i].par == par) return i; return -1; }

static void midi_scan(void) {            // the ports there are now, with their cards' names
    midi_ndev = 0;
    for (int c = 0; c < 8 && midi_ndev < MIDI_MAXDEV; c++) for (int d = 0; d < 4 && midi_ndev < MIDI_MAXDEV; d++) {
        MidiDev *m = &midi_dev[midi_ndev]; snprintf(m->path, sizeof m->path, "/dev/snd/midiC%dD%d", c, d);
        if (access(m->path, F_OK)) continue;
        char p[48]; snprintf(p, sizeof p, "/proc/asound/card%d/id", c); FILE *f = fopen(p, "r");
        m->name[0] = 0; if (f) { if (fgets(m->name, sizeof m->name, f)) m->name[strcspn(m->name, "\r\n")] = 0; fclose(f); }
        if (!m->name[0]) snprintf(m->name, sizeof m->name, "CARD %d", c);
        if (d) { size_t l = strlen(m->name); snprintf(m->name + (l > 20 ? 20 : l), 4, " %d", d + 1); }
        midi_ndev++;
    }
}
static void midi_wr_bytes(const uint8_t *b, int n) { if (midi_fd >= 0 && write(midi_fd, b, (size_t)n) < 0) { /* full or gone: the next one tries again */ } }
static void *midi_thread(void *arg) {
    (void)arg; uint8_t st = 0, msg[3]; int need = 0, got = 0, was = 0; long sent = 0;
    while (midi_run) {
        struct pollfd pf = {midi_fd, POLLIN, 0};
        int r = poll(&pf, 1, 1);         // 1 ms: the clock goes out with a jitter of a buffer at most
        if (r > 0 && (pf.revents & (POLLERR | POLLHUP | POLLNVAL))) { midi_run = 0; break; }   // unplugged
        if (r > 0 && (pf.revents & POLLIN)) {
            uint8_t buf[64]; ssize_t n = read(midi_fd, buf, sizeof buf);
            for (ssize_t i = 0; i < n; i++) { uint8_t b = buf[i];
                if (b >= 0xF8) {                                // real-time, between any bytes
                    if (b == 0xF8) { int64_t t = mono_ns(), l = mclk_last_ns, d = t - l;
                        if (l && d > 500000 && d < 200000000) mclk_per_ns = mclk_per_ns ? mclk_per_ns + (d - mclk_per_ns) / 12 : d;   // smoothed over about half a beat
                        mclk_last_ns = t; mclk_count++; }
                    else if (b == 0xFA || b == 0xFB || b == 0xFC) { if (b != 0xFC) mclk_count = 0; midi_push(b, 0, 0); }
                    continue; }
                if (b & 0x80) { st = b >= 0xF0 ? 0 : b; got = 0; need = (b & 0xE0) == 0xC0 ? 1 : 2; continue; }   // a status (system ones dropped)
                if (!st) continue;
                msg[1 + got++] = b;
                if (got == need) { msg[0] = st; got = 0;           // running status: the next data bytes reuse st
                    unsigned w = midi_wr; if (w - midi_rd < 256) { memcpy(midi_ring[w & 255], msg, 3); if (need == 1) midi_ring[w & 255][2] = 0; __atomic_store_n(&midi_wr, w + 1, __ATOMIC_RELEASE); } } }
        }
        if (midi_clk_out) {              // 24 a beat = 3 per 1/32 step of the engine
            int pl = engine_playing();
            if (pl && !was) { uint8_t b = 0xFA; midi_wr_bytes(&b, 1); sent = 0; }
            if (!pl && was) { uint8_t b = 0xFC; midi_wr_bytes(&b, 1); }
            if (pl) { long t = (long)(engine_posf() * 3) + 1;
                if (t - sent > 6 || t < sent - 6) sent = t - 1;   // a jump (or a torn read): no burst of clocks
                while (sent < t) { uint8_t b = 0xF8; midi_wr_bytes(&b, 1); sent++; } }
            was = pl;
        } else was = 0;
    }
    return NULL;
}
static void midi_close(void) {
    if (midi_fd < 0) return;
    midi_run = 0; pthread_join(midi_th, NULL); close(midi_fd); midi_fd = -1; midi_open_name[0] = 0;
}
static int midi_open(void) {             // the wanted card, if it is there; 1 = open
    midi_close(); if (!midi_want[0]) return 0;
    midi_scan();
    for (int i = 0; i < midi_ndev; i++) if (!strcmp(midi_dev[i].name, midi_want)) {
        midi_fd = open(midi_dev[i].path, O_RDWR | O_NONBLOCK); if (midi_fd < 0) midi_fd = open(midi_dev[i].path, O_RDONLY | O_NONBLOCK);
        if (midi_fd < 0) return 0;
        snprintf(midi_open_name, sizeof midi_open_name, "%s", midi_want); midi_rd = midi_wr;
        midi_run = 1; if (pthread_create(&midi_th, NULL, midi_thread, NULL)) { close(midi_fd); midi_fd = -1; return 0; }
        return 1;
    }
    return 0;
}
static void midi_cfg_save(void) { char b[64]; int n = snprintf(b, sizeof b, "%d %d %d %d %s\n", midi_clk_out, midi_notes, midi_ch, midi_clk_in, midi_want); write_whole("midi.cfg", b, n); }
static void midi_cfg_load(void) {
    FILE *f = fopen("midi.cfg", "r"); if (!f) return;
    char nm[32] = ""; if (fscanf(f, "%d %d %d %d %31[^\n]", &midi_clk_out, &midi_notes, &midi_ch, &midi_clk_in, nm) >= 4) snprintf(midi_want, sizeof midi_want, "%s", nm);
    midi_clk_out = midi_clk_out != 0; midi_clk_in = midi_clk_in != 0; midi_notes = midi_notes != 0; midi_ch = midi_ch < 0 || midi_ch > 16 ? 0 : midi_ch; fclose(f);
}
static void midi_pick(int d) {           // EDIT+<> on MIDI PORT: OFF and the ports found now, round
    midi_scan(); int cur = -1;
    for (int i = 0; i < midi_ndev; i++) if (!strcmp(midi_dev[i].name, midi_want)) cur = i;
    cur = ((cur + 1 + d) % (midi_ndev + 1) + midi_ndev + 1) % (midi_ndev + 1) - 1;
    if (cur < 0) { midi_want[0] = 0; midi_close(); } else { snprintf(midi_want, sizeof midi_want, "%s", midi_dev[cur].name); midi_open(); }
    midi_cfg_save();
}
