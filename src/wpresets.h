// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// WAVE's factory presets (2026-11-25, hand-written for BrickPad): each one is wp_defaults (src/wave.h) and then the
// pairs it lists, parameter then value (-1 ends the list). The matrix slots are WP_MTX + 4 * slot + WM_*.
// Times: 40 = 26 ms, 50 = 70 ms, 60 = 190 ms, 70 = 0.5 s, 80 = 1.4 s. CUTOFF 50 = 640 Hz, 60 = 1.3 kHz, 70 = 2.6 kHz.
// LFO RATE 30 = 0.2 Hz, 50 = 1 Hz, 70 = 4.8 Hz. Tables: 0 BASIC (pos 0 sine, 33 tri, 67 saw, 100 square), 1 PWM, 2 VOWEL,
// 3 HARMS, 4 NOISE, 5 DIGITAL, 6 SYNC, 7 FM, 8 FOLD, 9 ORGAN, 10 COMB, 11 RESO. Kept light for the Brick: unison <= 4.
#ifndef BP_WPRESETS_H
#define BP_WPRESETS_H
#include "wave.h"
typedef struct { const char *name; uint8_t cat; int16_t kv[48]; } WPreset;   // cat: preset_cat[] (presets.h)
#define WA(k) (WP_A + WO_##k)
#define WB(k) (WP_B + WO_##k)
#define E1(k) (WP_ENV1 + WE_##k)
#define E2(k) (WP_ENV2 + WE_##k)
#define E3(k) (WP_ENV3 + WE_##k)
#define L1(k) (WP_LFO1 + WL_##k)
#define L2(k) (WP_LFO2 + WL_##k)
#define MX(s, src, dst, amt, bi) WP_MTX + 4 * (s), src, WP_MTX + 4 * (s) + 1, (dst) + 1, WP_MTX + 4 * (s) + 2, amt, WP_MTX + 4 * (s) + 3, bi
static const WPreset wpresets[] = {
    // the sound a new WAVE pad starts with: two saws a few cents apart into the LP24, a short filter envelope
    {"INIT", 2, {WA(POS), 67, WB(ON), 1, WB(POS), 67, WB(FINE), 8, WB(LEVEL), 60, WP_F_CUT, 58, WP_F_RES, 20, WP_F_ENV, 30,
        E2(DEC), 55, E2(SUS), 25, WP_F_KEY, 40, -1}},
    {"RUBBER", 0, {WA(TABLE), 1, WA(POS), 25, WP_SUB_LEVEL, 55, WP_F_TYPE, WF_LADDER, WP_F_CUT, 34, WP_F_RES, 45, WP_F_ENV, 45,
        E2(DEC), 52, E2(SUS), 10, E1(SUS), 85, E1(REL), 25, WP_POLY, 1, WP_GLIDE, 12, WP_VELSENS, 80, -1}},
    {"REESE", 0, {WA(POS), 67, WA(UNI), 3, WA(DET), 30, WB(ON), 1, WB(POS), 67, WB(FINE), -14, WB(UNI), 2, WB(DET), 20, WB(LEVEL), 70,
        WP_F_CUT, 46, WP_F_RES, 25, L1(RATE), 28, MX(0, WS_LFO1, WP_F_CUT, 8, 1), E1(SUS), 90, E1(REL), 30, WP_POLY, 1, -1}},
    {"WOBBLE", 0, {WA(POS), 67, WB(ON), 1, WB(TABLE), 1, WB(POS), 20, WB(OCT), -1, WP_SUB_LEVEL, 50, WP_F_TYPE, WF_LADDER, WP_F_CUT, 30,
        WP_F_RES, 55, L1(SYNC), 5, L1(TRIG), 1, MX(0, WS_LFO1, WP_F_CUT, 40, 0), E1(SUS), 100, E1(REL), 30, WP_POLY, 1, -1}},
    {"FOLD BASS", 0, {WA(TABLE), 8, WA(POS), 10, MX(0, WS_ENV2, WA(POS), 70, 0), E2(DEC), 50, E2(SUS), 0, WP_SUB_LEVEL, 40,
        WP_F_CUT, 70, WP_F_RES, 10, E1(SUS), 80, E1(REL), 28, WP_POLY, 1, -1}},
    {"SUB DROP", 1, {WA(POS), 0, WA(LEVEL), 85, WP_SUB_LEVEL, 60, WP_F_TYPE, WF_OFF, MX(0, WS_ENV3, WA(SEMI), 25, 0), E3(DEC), 42, E3(SUS), 0,
        E1(DEC), 70, E1(SUS), 100, E1(REL), 32, WP_POLY, 1, -1}},
    {"SUPERSAW", 2, {WA(POS), 67, WA(UNI), 4, WA(DET), 45, WA(WIDTH), 100, WB(ON), 1, WB(POS), 67, WB(OCT), -1, WB(UNI), 3, WB(DET), 35,
        WB(LEVEL), 50, WP_F_TYPE, WF_LP12, WP_F_CUT, 78, E1(ATT), 25, E1(SUS), 80, E1(REL), 55, WP_POLY, 6, -1}},
    {"SQUARE LEAD", 2, {WA(TABLE), 1, WA(POS), 0, WB(ON), 1, WB(TABLE), 1, WB(POS), 30, WB(FINE), 6, WB(LEVEL), 55, L1(RATE), 40,
        MX(0, WS_LFO1, WA(POS), 40, 0), WP_F_TYPE, WF_LADDER, WP_F_CUT, 60, WP_F_RES, 30, WP_POLY, 1, WP_LEGATO, 1, WP_GLIDE, 22, -1}},
    {"DIGI ARP", 2, {WA(TABLE), 5, WA(POS), 30, MX(0, WS_ENV2, WA(POS), 50, 0), E2(DEC), 45, E2(SUS), 0, WP_F_CUT, 80,
        E1(DEC), 50, E1(SUS), 0, E1(REL), 45, WP_ARP, 1, WP_ARP_MODE, 3, WP_ARP_OCT, 1, WP_ARP_RATE, 2, WP_ARP_GATE, 60, -1}},
    {"VOWEL PAD", 3, {WA(TABLE), 2, WA(UNI), 2, WA(DET), 20, L1(RATE), 24, MX(0, WS_LFO1, WA(POS), 100, 0), WB(ON), 1, WB(TABLE), 2,
        WB(POS), 50, WB(SEMI), 7, WB(LEVEL), 45, WP_F_CUT, 72, E1(ATT), 62, E1(DEC), 60, E1(SUS), 80, E1(REL), 72, -1}},
    {"GLASS PAD", 3, {WA(TABLE), 3, WA(POS), 40, WA(UNI), 3, WA(DET), 18, WB(ON), 1, WB(TABLE), 7, WB(POS), 30, WB(OCT), 1, WB(LEVEL), 40,
        L2(RATE), 20, MX(0, WS_LFO2, WB(POS), 60, 0), WP_F_TYPE, WF_LP12, WP_F_CUT, 74, E1(ATT), 58, E1(SUS), 85, E1(REL), 74, -1}},
    {"STRINGS", 7, {WA(POS), 67, WA(UNI), 4, WA(DET), 25, WB(ON), 1, WB(TABLE), 1, WB(POS), 15, WB(UNI), 2, WB(LEVEL), 50,
        L1(RATE), 66, MX(0, WS_LFO1, WA(FINE), 6, 1), WP_F_CUT, 62, E1(ATT), 56, E1(DEC), 55, E1(SUS), 85, E1(REL), 64, -1}},
    {"BRASS", 6, {WA(POS), 67, WB(ON), 1, WB(POS), 67, WB(FINE), 5, WB(LEVEL), 70, WP_F_CUT, 40, WP_F_RES, 15, WP_F_ENV, 45,
        E2(ATT), 52, E2(DEC), 58, E2(SUS), 50, E1(ATT), 42, E1(SUS), 85, E1(REL), 45, -1}},
    {"PLUCK", 5, {WA(POS), 67, WB(ON), 1, WB(TABLE), 6, WB(POS), 40, WB(LEVEL), 50, WP_F_CUT, 30, WP_F_RES, 30, WP_F_ENV, 70,
        E2(DEC), 56, E2(SUS), 0, E1(DEC), 62, E1(SUS), 0, E1(REL), 56, WP_VELSENS, 80, -1}},
    {"E-PIANO", 4, {WA(TABLE), 7, WA(POS), 20, MX(0, WS_VEL, WA(POS), 40, 0), MX(1, WS_ENV2, WA(POS), 25, 0), E2(DEC), 62, E2(SUS), 0,
        WB(ON), 1, WB(POS), 0, WB(OCT), 1, WB(LEVEL), 28, WP_F_TYPE, WF_LP12, WP_F_CUT, 86, E1(DEC), 74, E1(SUS), 20, E1(REL), 58, WP_VELSENS, 85, -1}},
    {"ORGAN", 4, {WA(TABLE), 9, WA(POS), 75, WP_F_TYPE, WF_OFF, E1(ATT), 10, E1(SUS), 100, E1(REL), 40, L1(RATE), 68, MX(0, WS_LFO1, WA(PAN), 30, 1), -1}},
    {"BELL", 4, {WA(TABLE), 7, WA(POS), 60, WB(ON), 1, WB(POS), 0, WB(OCT), 2, WB(SEMI), 7, WB(FINE), 40, WB(LEVEL), 35, WP_F_TYPE, WF_OFF,
        E1(DEC), 80, E1(SUS), 0, E1(REL), 78, E1(CURVE), 80, -1}},
    {"RESO SWEEP", 16, {WA(TABLE), 11, WA(UNI), 2, WA(DET), 15, L1(RATE), 18, L1(SHAPE), WLS_TRI, MX(0, WS_LFO1, WA(POS), 100, 0), WP_F_CUT, 76,
        E1(ATT), 50, E1(SUS), 90, E1(REL), 66, -1}},
    {"NOISE WIND", 9, {WA(ON), 0, WP_NOISE_LEVEL, 80, WP_NOISE_TYPE, 1, WP_F_TYPE, WF_BP, WP_F_CUT, 45, WP_F_RES, 60, L1(RATE), 26,
        MX(0, WS_LFO1, WP_F_CUT, 25, 1), E1(ATT), 66, E1(SUS), 100, E1(REL), 72, -1}},
    {"COMB DRONE", 3, {WA(TABLE), 10, WA(POS), 30, WA(UNI), 3, WA(DET), 12, L1(RATE), 16, MX(0, WS_LFO1, WA(POS), 70, 0), WP_SUB_LEVEL, 30,
        WP_F_CUT, 64, E1(ATT), 60, E1(SUS), 100, E1(REL), 76, -1}},
};
#define NWPRESET (int)(sizeof wpresets / sizeof wpresets[0])
static void wpreset_apply(int8_t *wp, int i) {   // a factory preset onto a pad's block (values clamped to their ranges)
    wp_defaults(wp); if (i < 0 || i >= NWPRESET) return;
    for (const int16_t *k = wpresets[i].kv; k < wpresets[i].kv + 48 && k[0] >= 0; k += 2) {
        int lo, hi; wp_range(k[0], &lo, &hi); if (lo < hi || k[0] >= WP_MTX) wp[k[0]] = (int8_t)(k[1] < lo ? lo : k[1] > hi ? hi : k[1]); }
}
#undef WA
#undef WB
#undef E1
#undef E2
#undef E3
#undef L1
#undef L2
#undef MX
#endif
