# WAVE: BrickPad's wavetable engine

Status (2026-11-25): **wired in. WAVE is Track.type 1 and replaces GRAND** (user's decision). Section 10 has the pages
as built, section 11 how it is wired and how an old GRAND pad becomes a WAVE sound. Everything here was written for BrickPad: the FFT, the table generators, the
filters (textbook TPT forms after Zavalishin, *The Art of VA Filter Design*), the envelope, the WAV reader. No code
or data from other synths; the built-in tables are made from formulas at load time. No third-party code, so there
is no THIRD_PARTY.md.

| file | what |
|---|---|
| `src/wave.h` | the engine, header-only and guarded like `synth.h`: tables, parameters, voice, instrument |
| `src/engine.c` | `wave[NSLOT]`, `wstore`, `start_wave`, the render into the pad bus, the ARP, the CPU guard |
| `src/main.c` | `Song.wv`, `wv_from_grand` / `wave_fix` (old files), presets, cards (`P_WV + k`), learn |
| `src/sound.h`, `src/wvui.h` | the SOUND pages, the pictures, the MATRIX list, the TABLE browser |
| `src/wpresets.h` | 20 factory presets |
| `tests/qa_wave.c` | the DSP checks; prints `ALL OK` |
| `qa/qa_wave_ui.c` | the pad: pages, notes, ARP, matrix learn, tables, GRAND conversion, save / load; prints `ALL OK` |
| `bench/wave_bench.c` | CPU per voice and for the target scene |

```
cc -O2 -std=gnu11 -Isrc tests/qa_wave.c -o qa_wave -lm && ./qa_wave
cc -O2 -std=gnu11 -Isrc bench/wave_bench.c -o wave_bench -lm && ./wave_bench      # [A53 factor, default 7]
# on the Brick / for the Brick, as build.sh does:
zig cc -target aarch64-linux-gnu.2.38 -O2 -mcpu=cortex_a53 -std=gnu11 -Isrc bench/wave_bench.c -o wave_bench -lm
```

## 1. Signal flow (one voice)

```
            +-- OSC A: table, POSITION morph, WARP, UNISON 1..8 (detune, stereo spread) --+
 note ----> +-- OSC B: the same; FM / AM / RM read the other oscillator                 --+--> FILTER --+--> x AMP --> out (stereo)
 (glide,    +-- SUB: sine / tri / square / saw, -1 or -2 oct ----------------- (DIRECT) --+     (SVF or  |    (ENV1 x velocity
  bend)     +-- NOISE: white / pink ------------------------------------------ (DIRECT) --+     ladder)  |     x MASTER)
                                                                     DIRECT sources skip the filter -+
 ENV1 (amp), ENV2 (filter ENV AMT), ENV3; LFO1-4; VELOCITY, NOTE, MACRO1-4, MODWHEEL --> 16-slot matrix --> any parameter marked "mod"
```

Audio runs per sample; modulation, envelopes, LFOs and every coefficient run at a control rate of **16 frames**
(`WV_BLK`, 3 kHz), with straight-line ramps per sample for the oscillator levels, the morph position, the sub and noise
levels and the output amplitude, so nothing zips. A 128-frame buffer is 8 control blocks. Notes start on a block
boundary of their own voice (sample-accurate start via the `delay` argument); a gate ends within a block (0.33 ms).

## 2. Wavetables

- **Frames** of 2048 samples, up to 256 per table. POSITION 0..100 maps onto frames 0..nf-1; between two frames the
  oscillator reads both at the same phase and crossfades (per sample, so a moving position is smooth).
- **Mipmaps, one per octave** (11 levels), made at load time with our FFT: level m keeps harmonics 1..1024>>m
  (level 0: 1..1023, DC and Nyquist dropped). Level sizes are 2048 down to 32 samples, never fewer than 32 samples per
  kept harmonic, so the straight-line (ECO) read stays clean; each level has wrap guards for the cubic read. One frame
  at all levels is 12289 floats (48 KB).
- **Level choice** (per oscillator, per block): the highest unison voice's frequency times what the warp does to the
  top harmonic (SYNC ratio, PWM 1/width, ASYM, BEND slope, FM) must keep the top harmonic under 28 kHz, so anything
  that folds lands above 20 kHz. All unison voices of an oscillator use that one level. Levels switch per octave
  (no crossfade between levels): at worst the top octave of content (14..20 kHz) drops out just past an octave line.
- **Store**: `WaveStore` holds 32 slots: 0..11 built in, 12..31 for user WAVs. `wt_build_all` makes the built-ins
  (12.4 MB, about 0.1 s on the x86 used here, so expect around 1 s on the A53; `wt_build(ws, k)` makes one, for lazy
  loading).
- **Built-in tables** (all procedural):

| # | name | frames | how |
|---|---|---|---|
| 0 | BASIC | 31 | sine -> triangle -> saw -> square (spectra crossfaded, frame 0 is a pure sine) |
| 1 | PWM | 32 | pulse width 50 % -> 3 % (a saw minus a shifted saw) |
| 2 | VOWEL | 17 | a 110 Hz voice through A E I O U formants (averaged textbook F1-F3) |
| 3 | HARMS | 32 | harmonics added one by one, 1..32, saw weights |
| 4 | NOISE | 16 | seeded random spectra, dark to bright |
| 5 | DIGITAL | 16 | a sine through 32..4 levels and 64..8 steps a cycle |
| 6 | SYNC | 32 | a saw hard-synced at 1..8 times |
| 7 | FM | 16 | a sine phase-modulated by its octave, index 0..5 |
| 8 | FOLD | 16 | a sine folded, gain 1..8 |
| 9 | ORGAN | 9 | drawbars 16' 8' 5 1/3' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1', pulled out one by one |
| 10 | COMB | 16 | a saw through a moving comb (phaser-like notches) |
| 11 | RESO | 32 | a decaying burst at 1..16 times the cycle (resonant sweep) |

  Built-ins are normalised per frame (level 0 peak = 1).
- **User WAVs** (`wt_load_wav`): RIFF WAVE, PCM 8/16/24/32-bit or float 32/64, mono (stereo and more are mixed
  down). The frame size is 2048, or what a `clm ` chunk says (the text `<!>` followed by the size; any power of two
  32..4096, resampled to 2048 through its harmonics), or the whole file when it is a single power-of-two cycle
  shorter than 2048. The length must be whole frames (else -4); more than 256 frames keeps the first 256. User
  tables are not normalised; a peak over 1 is brought down to 1. `wt_save_wav` writes a table (16/24/32-bit or float)
  with a `clm ` chunk, and `wt_from_frames` makes a table from raw frames in memory. A 256-frame table is 12.6 MB.
- **Threads**: tables are built and loaded on the UI / loader thread. To replace a table while it may be playing,
  load it into a spare `WaveTable`, swap it into the store between buffers (through the engine's command queue), and
  free the old one a buffer later.

## 3. Oscillators, unison, warp

Per oscillator: TABLE, POSITION, OCT/SEMI/FINE, LEVEL, PAN, PHASE (start phase), RAND (random start phase per unison
voice), UNISON 1..8, DETUNE (100 = voices spread over +-50 cents, evenly), WIDTH (unison voices spread across the
stereo field, equal-power pans), WARP and its amount. The phase is a 32-bit integer per unison voice (exact pitch,
no drift). Unison voices share the level as 1/sqrt(n).

| warp | what it does (a = amount 0..1) |
|---|---|
| SYNC | phase x (1 + 15a), wrapped: a hard-synced read of the frame |
| BEND+ / BEND- | phase bent by -/+ 0.9a sin(2 pi x) / 2 pi: the first half squeezed or stretched (smooth, monotonic) |
| PWM | the cycle squeezed into the first (1 - 0.95a) of the period, the rest holds the start value |
| ASYM+ / ASYM- | the half-way point moved to 0.5 -/+ 0.45a: one half faster, the other slower |
| FLIP | the output's sign flipped from phase (1 - a) to the end of the cycle |
| MIRROR | crossfade (a) with the frame read backwards: the wave becomes symmetric |
| QUANT | the output rounded to 2^(1 + 7(1 - a)) levels (256 -> 2) |
| FM | the other oscillator moves this one's phase, up to +-a cycles (phase modulation) |
| AM | amplitude modulation by the other oscillator (depth a) |
| RM | ring modulation by the other oscillator (mix a) |

FM / AM / RM read the other oscillator's output before its level (so B at LEVEL 0 is a pure modulator). If A reads B,
B is worked out first; if both read each other, B reads A from the block before (a 16-frame delay, which moves the
modulator's phase but not the spectrum). SYNC, PWM, FLIP and QUANT make jumps that are not band-limited, as on the
hardware that made them famous; the level choice above limits what folds back for SYNC/PWM/ASYM/BEND/FM.

**SUB**: sine, triangle, square, saw (square and saw with PolyBLEP), 1 or 2 octaves below the note, its own pan, and
DIRECT to skip the filter. **NOISE**: white, or pink (six one-pole low-passes two octaves apart, 1/sqrt(f) weights,
within about +-1 dB of -3 dB/octave from 50 Hz to 13 kHz), mono, DIRECT likewise.

## 4. Filter

One per voice, stereo. **SVF** (topology-preserving trapezoidal, Zavalishin's form): LP12, HP12, BP (unity peak),
NOTCH; **LP24** is a gentle SVF (Q 0.7) into the resonant one. Resonance takes the SVF to Q 12.5. **LADDER**: four TPT
one-poles with a soft clip at the input and a one-sample feedback, resonance k up to 3.9, level made up as it rises.
CUTOFF 0..100 = 20 Hz x 2^(v/10) (to 20 kHz); KEY TRK 100 follows the note 1:1 around C4; ENV AMT +-100 = +-8 octaves
from ENV2; DRIVE gains x1..x8 into a soft clip before the filter. The tan() of the cutoff is worked out only when the
cutoff moves. States are flushed to 0 under 1e-15 every block: no denormals.

## 5. Envelopes, LFOs, matrix

- **ENV1-3**: ADSR, times 0.5 ms .. 10 s (0.5 ms x 20000^(v/100)), CURVE -100..100 for all segments (> 0 starts fast
  like an RC, < 0 starts slow, 0 straight). Segments end exactly on time (the time left in a block carries into the
  next segment); a new note in a sounding voice attacks from where the envelope is (no click). ENV1 is the amplitude
  and ends the voice; ENV2 also feeds the filter's ENV AMT.
- **LFO1-4**: SINE TRI SAW UP SAW DN SQUARE S&H SMOOTH, rate 0.02..50 Hz free or synced (1/32 .. 8 bars, triplets;
  `WaveInst.bpm`), PHASE offset, TRIG FREE (a new voice joins the running LFO, so all voices agree), RETRIG (from 0 on
  each note) or ONE (one cycle, then it holds).
- **Matrix**: 16 slots of SRC, DEST, AMT, MODE. Sources: ENV1-3, LFO1-4, VELOCITY, NOTE ((note - 60) / 48),
  MACRO1-4, MODWHEEL. DEST is any parameter marked *mod* in the table below (stored as index + 1, 0 = none). AMT
  -100..100 is a share of the destination's whole range (A SEMI: 25 = 12 semitones; CUTOFF: 10 = an octave). MODE
  UNI uses the source as 0..1, BI as -1..1 (an LFO in UNI moves only up; a macro in BI swings around the knob). Slots
  add up; the result is clamped to the parameter's range. The matrix runs per voice per block, from the sources as
  the block before left them (so an envelope may modulate another envelope's times without a loop).

## 6. Voices

8 voices (`WV_POLY`); VOICES 1..8 caps them (1 = MONO). A new note takes a free voice, else the quietest released
one, else the oldest; a stolen voice fades to 0 over one block (0.33 ms) before the new note starts. The same key
again retriggers its own voice. MONO: one voice, a new note attacks again from the current level (GLIDE slides the
pitch if set); with LEGATO a note played while another is held only moves the pitch. In both, letting go of a key returns
to the newest key still held. GLIDE 0..2 s (about 95 % of the way in that time), in poly too (from the last note played).
BEND: `WaveInst.bend` -1..1 times BEND semitones.

## 7. Parameters

`int8_t wp[WP_N]` (256 bytes, `Song.wv[pad]`): 0..95 the sound (below), 96..100 the host's ARP (below), 128..191 the
matrix (16 x SRC 0..14, DST 0..96, AMT -100..100, MODE 0 UNI / 1 BI), 255 the mark (`WP_INIT` 0x57 once set up). The
other bytes are spare for later (they stay 0).

The host's bytes (2026-11-25; engine.c reads them, the voice does not): 96 ARP 0..1, 97 ARP MODE 0..3 (UP DOWN ORDER
UP/DN), 98 ARP OCT 0..2 (1..3 octaves), 99 ARP RATE 0..11 (a division of fx.h `div_beats`, default 2 = 1/16), 100 ARP
GATE 10..100 (default 50).
`wp_defaults` sets everything; `wp_info[k]` has name, range, default and whether it may be a matrix destination;
`wp_name`, `wp_range` and `wp_str` (the value as text, table names from the store) cover the matrix too.

| # | name | range | default | mod | meaning |
|---|---|---|---|---|---|
| 0 | A ON | 0..1 | 1 |  | oscillator on |
| 1 | A TABLE | 0..31 | 0 |  | table slot (0..11 built in, 12..31 user WAVs) |
| 2 | A POS | 0..100 | 0 | yes | position in the table (morphs between neighbouring frames) |
| 3 | A OCT | -4..4 | 0 | yes | octaves |
| 4 | A SEMI | -24..24 | 0 | yes | semitones |
| 5 | A FINE | -100..100 | 0 | yes | cents |
| 6 | A LEVEL | 0..100 | 75 | yes | level (unison voices share it, 1/sqrt(n)) |
| 7 | A PAN | -50..50 | 0 | yes | pan |
| 8 | A PHASE | 0..100 | 0 |  | start phase of every voice |
| 9 | A RAND | 0..100 | 100 |  | random start phase, per unison voice |
| 10 | A UNISON | 1..8 | 1 |  | unison voices |
| 11 | A DETUNE | 0..100 | 25 | yes | unison spread, 100 = +-50 cents |
| 12 | A WIDTH | 0..100 | 80 | yes | unison stereo spread |
| 13 | A WARP | 0..12 | 0 |  | warp mode (see Warp) |
| 14 | A W.AMT | 0..100 | 0 | yes | warp amount |
| 15 | (spare) | | | | |
| 16 | B ON | 0..1 | 0 |  | oscillator on |
| 17 | B TABLE | 0..31 | 0 |  | table slot (0..11 built in, 12..31 user WAVs) |
| 18 | B POS | 0..100 | 0 | yes | position in the table (morphs between neighbouring frames) |
| 19 | B OCT | -4..4 | 0 | yes | octaves |
| 20 | B SEMI | -24..24 | 0 | yes | semitones |
| 21 | B FINE | -100..100 | 0 | yes | cents |
| 22 | B LEVEL | 0..100 | 75 | yes | level (unison voices share it, 1/sqrt(n)) |
| 23 | B PAN | -50..50 | 0 | yes | pan |
| 24 | B PHASE | 0..100 | 0 |  | start phase of every voice |
| 25 | B RAND | 0..100 | 100 |  | random start phase, per unison voice |
| 26 | B UNISON | 1..8 | 1 |  | unison voices |
| 27 | B DETUNE | 0..100 | 25 | yes | unison spread, 100 = +-50 cents |
| 28 | B WIDTH | 0..100 | 80 | yes | unison stereo spread |
| 29 | B WARP | 0..12 | 0 |  | warp mode (see Warp) |
| 30 | B W.AMT | 0..100 | 0 | yes | warp amount |
| 31 | (spare) | | | | |
| 32 | SUB LEVEL | 0..100 | 0 | yes | sub level (0 = off) |
| 33 | SUB SHAPE | 0..3 | 0 |  | SINE TRI SQUARE SAW |
| 34 | SUB OCT | 1..2 | 1 |  | 1 or 2 octaves below the note |
| 35 | SUB PAN | -50..50 | 0 | yes | sub pan |
| 36 | SUB DIRECT | 0..1 | 0 |  | 1 = past the filter |
| 37 | NOISE LEVEL | 0..100 | 0 | yes | noise level |
| 38 | NOISE TYPE | 0..1 | 0 |  | WHITE PINK |
| 39 | NOISE DIRECT | 0..1 | 0 |  | 1 = past the filter |
| 40 | FILTER | 0..6 | 2 |  | OFF LP12 LP24 HP12 BP NOTCH LADDER |
| 41 | CUTOFF | 0..100 | 75 | yes | 20 Hz * 2^(v/10): 20 Hz .. 20 kHz |
| 42 | RESO | 0..100 | 15 | yes | resonance (SVF Q up to 12.5, ladder k up to 3.9) |
| 43 | DRIVE | 0..100 | 0 | yes | input gain x1..x8 into a soft clip |
| 44 | KEY TRK | 0..100 | 0 | yes | key tracking, 100 = 1 octave per octave (C4 = 0) |
| 45 | ENV AMT | -100..100 | 0 | yes | ENV2 to cutoff, 100 = +8 octaves |
| 46 | (spare) | | | | |
| 47 | (spare) | | | | |
| 48 | ENV1 ATT | 0..100 | 0 | yes | attack 0.5 ms * 20000^(v/100): 0.5 ms .. 10 s |
| 49 | ENV1 DEC | 0..100 | 50 | yes | decay, same scale |
| 50 | ENV1 SUS | 0..100 | 70 | yes | sustain level |
| 51 | ENV1 REL | 0..100 | 35 | yes | release, same scale |
| 52 | ENV1 CURVE | -100..100 | 40 | yes | curve: > 0 fast start (RC-like), < 0 slow start, 0 straight |
| 53 | ENV2 ATT | 0..100 | 0 | yes | attack 0.5 ms * 20000^(v/100): 0.5 ms .. 10 s |
| 54 | ENV2 DEC | 0..100 | 50 | yes | decay, same scale |
| 55 | ENV2 SUS | 0..100 | 30 | yes | sustain level |
| 56 | ENV2 REL | 0..100 | 35 | yes | release, same scale |
| 57 | ENV2 CURVE | -100..100 | 40 | yes | curve: > 0 fast start (RC-like), < 0 slow start, 0 straight |
| 58 | ENV3 ATT | 0..100 | 0 | yes | attack 0.5 ms * 20000^(v/100): 0.5 ms .. 10 s |
| 59 | ENV3 DEC | 0..100 | 50 | yes | decay, same scale |
| 60 | ENV3 SUS | 0..100 | 30 | yes | sustain level |
| 61 | ENV3 REL | 0..100 | 35 | yes | release, same scale |
| 62 | ENV3 CURVE | -100..100 | 40 | yes | curve: > 0 fast start (RC-like), < 0 slow start, 0 straight |
| 63 | (spare) | | | | |
| 64 | LFO1 SHAPE | 0..6 | 0 |  | SINE TRI SAW UP SAW DN SQUARE S&H SMOOTH |
| 65 | LFO1 RATE | 0..100 | 50 | yes | free rate 0.02 Hz * 2500^(v/100): 0.02 .. 50 Hz |
| 66 | LFO1 SYNC | 0..12 | 0 |  | 0 FREE, else 1/32 1/16T 1/16 1/8T 1/8 1/4T 1/4 1/2 1 2 4 8 bars |
| 67 | LFO1 TRIG | 0..2 | 0 |  | FREE (voices join the running LFO), RETRIG (each note), ONE (one cycle, then holds) |
| 68 | LFO1 PHASE | 0..100 | 0 |  | phase offset |
| 69 | LFO2 SHAPE | 0..6 | 0 |  | SINE TRI SAW UP SAW DN SQUARE S&H SMOOTH |
| 70 | LFO2 RATE | 0..100 | 50 | yes | free rate 0.02 Hz * 2500^(v/100): 0.02 .. 50 Hz |
| 71 | LFO2 SYNC | 0..12 | 0 |  | 0 FREE, else 1/32 1/16T 1/16 1/8T 1/8 1/4T 1/4 1/2 1 2 4 8 bars |
| 72 | LFO2 TRIG | 0..2 | 0 |  | FREE (voices join the running LFO), RETRIG (each note), ONE (one cycle, then holds) |
| 73 | LFO2 PHASE | 0..100 | 0 |  | phase offset |
| 74 | LFO3 SHAPE | 0..6 | 0 |  | SINE TRI SAW UP SAW DN SQUARE S&H SMOOTH |
| 75 | LFO3 RATE | 0..100 | 50 | yes | free rate 0.02 Hz * 2500^(v/100): 0.02 .. 50 Hz |
| 76 | LFO3 SYNC | 0..12 | 0 |  | 0 FREE, else 1/32 1/16T 1/16 1/8T 1/8 1/4T 1/4 1/2 1 2 4 8 bars |
| 77 | LFO3 TRIG | 0..2 | 0 |  | FREE (voices join the running LFO), RETRIG (each note), ONE (one cycle, then holds) |
| 78 | LFO3 PHASE | 0..100 | 0 |  | phase offset |
| 79 | LFO4 SHAPE | 0..6 | 0 |  | SINE TRI SAW UP SAW DN SQUARE S&H SMOOTH |
| 80 | LFO4 RATE | 0..100 | 50 | yes | free rate 0.02 Hz * 2500^(v/100): 0.02 .. 50 Hz |
| 81 | LFO4 SYNC | 0..12 | 0 |  | 0 FREE, else 1/32 1/16T 1/16 1/8T 1/8 1/4T 1/4 1/2 1 2 4 8 bars |
| 82 | LFO4 TRIG | 0..2 | 0 |  | FREE (voices join the running LFO), RETRIG (each note), ONE (one cycle, then holds) |
| 83 | LFO4 PHASE | 0..100 | 0 |  | phase offset |
| 84 | VOICES | 1..8 | 8 |  | voices, 1 = MONO |
| 85 | LEGATO | 0..1 | 0 |  | mono: held keys slide without a new attack |
| 86 | GLIDE | 0..100 | 0 | yes | 0..2 s (v/100)^2*2 |
| 87 | BEND | 0..24 | 2 |  | pitch wheel range, semitones |
| 88 | VELO | 0..100 | 60 | yes | how much velocity sets the level |
| 89 | MASTER | 0..100 | 70 | yes | voice level (squared) |
| 90 | QUALITY | 0..1 | 0 |  | ECO: linear reads, filter at 1x; HIGH: cubic reads, filter at 2x |
| 91 | MACRO 1 | 0..100 | 0 |  | a matrix source the UI sets |
| 92 | MACRO 2 | 0..100 | 0 |  | a matrix source the UI sets |
| 93 | MACRO 3 | 0..100 | 0 |  | a matrix source the UI sets |
| 94 | MACRO 4 | 0..100 | 0 |  | a matrix source the UI sets |
| 95 | MODWHEEL | 0..100 | 0 |  | the mod wheel (a matrix source) |

## 8. CPU

**Measured on the Brick** (2026-11-25, `wave_bench` built for it): one voice at unison 1, ECO, about 30 us per 128-frame
buffer; the target scene (8 voices x 2 oscillators x unison 4) 21 % of one A53 core in ECO, 40 % in HIGH. So the
defaults are QUALITY ECO and UNISON 1, and the engine's CPU guard (engine.c `wave_par`) acts under load: above 50 %
DSP load 6 voices a pad, above 70 % 4; above 60 % QUALITY ECO and each oscillator's unison halved (only above 2).
The estimates below were made before that measurement:

`bench/wave_bench.c`, x86 (Xeon 2.1 GHz, shared VM, gcc 13 -O2), 128-frame buffers (2667 us of audio). One voice,
both oscillators on, positions between frames (the two-frame morph read, the costly case), LP24 on. ns per voice
per buffer, best of 5:

| | ECO | HIGH |
|---|---|---|
| unison 1 (2 table reads a frame) | 3013 | 4313 |
| unison 4 (8 reads) | 4565 | 8941 |
| unison 8 (16 reads) | 7008 | 13469 |
| unison 4, warps FM + BEND+ | 9648 | 17033 |
| unison 4, LADDER | 6113 | 11303 |
| unison 4, + SUB + pink NOISE | 5464 | 9242 |

**Target scene**, 8 voices x 2 osc x unison 4, LP24, 4 matrix routes (LFO -> A POS, LFO -> CUTOFF, ENV2 -> B POS,
LFO -> A FINE): ECO 44.1 us per buffer (1.7 % of one x86 core), HIGH 80.6 us (3.0 %). With the A53 taken as
~7x slower per core (6-8x), that is **about 12 % (ECO) and 21 % (HIGH) of one A53 core**, against the
budget of ~25 %. This is an estimate: the bench was not run on a Brick (no aarch64 toolchain here); running
`wave_bench` there is the real measure, and the A53's smaller L1 (32 KB) may cost more on the table reads than on x86.

- **QUALITY** (`WP_QUALITY`): ECO reads tables with straight lines and runs the filter once a frame. HIGH reads with a
  4-point cubic (Catmull-Rom) and runs the filter twice a frame (the input held, the coefficient made for 96 kHz:
  better cutoff and resonance near the top; it is not a full oversampler of the oscillators, which the mipmaps
  already keep clean).
- Where the time goes: about 1.5 us a voice is fixed (8 control blocks: matrix, envelopes, LFOs, coefficients, the
  mix), the LP24 about 1 us, each unison voice of each oscillator about 0.3 us (ECO) at a moving position, less on a
  frame. A position sitting exactly on a frame reads one frame, not two.
- More room if needed: unison is the big lever (the engine can lower VOICES under load, as `pick_voice` does with
  `load_avg`), then QUALITY ECO, then a longer control block (`-DWV_BLK=32`).

## 9. Tests (`tests/qa_wave.c`)

| check | how | result here |
|---|---|---|
| pitch | sine (BASIC frame 0), notes 33..117 (55 Hz .. 7 kHz), ECO and HIGH; 2^17-point FFT, Gaussian peak interpolation; also OCT+SEMI+FINE | worst 0.15 cents (limit 1) |
| aliasing | a naive 2x-1 saw loaded as a user table, 100 Hz .. 10 kHz in 1.7-semitone steps plus 10 kHz exactly, ECO and HIGH; Blackman-Harris FFT; everything not within 7 bins of a harmonic, 20 Hz .. 20 kHz | worst -63.2 dB (limit -60) |
| unison width | unison 4, L/R correlation at WIDTH 0 / 50 / 100; WIDTH 0 must be exactly mono | 1.0000 / 0.843 / 0.454 |
| warp | 13 modes x both oscillators x 4 amounts x 3 notes x 2 qualities: finite, under 4, not silent, and at 100 different from 0 | all pass |
| envelopes | each stage's time against the setting for 4 x 3 curves (within a block); through a voice: attack heard, release ends the voice | stages within one block (0.33 ms), attack within 3 ms, voice end within 4 ms |
| matrix | MACRO / MODWHEEL / NOTE to pitch in UNI and BI (within 1 cent of the expected shift), LFO -> LEVEL, ENV2 -> CUTOFF (panel and matrix), VELOCITY -> MASTER | all pass |
| WAV | round trip 16 / 24 / 32-bit / float (all mipmap levels compared), stereo without `clm `, `clm ` 1024, a part frame refused, missing file | max error 4.3e-5 (16-bit) .. 1.5e-8 |
| voices | stealing (10 notes on 8), VOICES cap, mono legato + glide + return to held key, mono retrigger from the current level, gate, delay | all pass |
| NaN / denormal | 56 extreme setups (every filter at max resonance, drive, both cutoff ends, unison 8 x 2, random warps at 100, 16 matrix slots at +-100, 8 voices): finite and under 20; a 10 s resonant release: no subnormal state and no slowdown | all pass |

Also clean under `-fsanitize=address,undefined` and at -O0.

## 10. UI pages (as built, 2026-11-25)

The SOUND section of a WAVE pad, on the v3 pages: the pages as tabs (UP from the top row onto them, <> the page), a
picture on top, the cards below in the grid of `kg_cell` (ui.h), EDIT+arrows the value, OPT+EDIT the default. Switches
are `toggle_draw`, 3..4 named choices `seg_draw`, long lists (TABLE, WARP, FILTER TYPE, LFO SHAPE and SYNC, ARP RATE)
`val_draw`; every knob shows what moves it live (`mod_live` / `mod_rings`) when a pad LFO or automation lane is on it.

| page | picture | cards |
|---|---|---|
| **OSC A**, **OSC B** | the frame at POS (one cycle, morphed), the table's frames as a strip, POS as set (white) and as it plays (orange); EDIT on it: the TABLE browser; EDIT+<> on it: the next table | ON, TABLE, POS, WARP, W.AMT, LEVEL, OCT, SEMI, FINE, UNISON |
| **UNISON** | each oscillator's voices across pitch (DETUNE) and the stereo field (WIDTH, PAN) | A / B: DETUNE, WIDTH, PAN, PHASE, RAND |
| **SUB** | the sub's wave, the noise's colour | SUB, SHAPE, OCTAVE, SUB PAN, SUB DIRECT, NOISE, COLOUR, NS DIRECT |
| **FILTER** | the response on 20 Hz .. 20 kHz, the cutoff as set and as it is now | TYPE, CUTOFF, RESO, DRIVE, KEY TRK, ENV AMT, ENV2 ATT DEC SUS REL |
| **ENV** | the envelope, its level now | ENVELOPE (1 2 3: the cards follow), ATT, DEC, SUS, REL, CURVE |
| **LFO** | two cycles of the shape from PHASE | LFO (1..4), SHAPE, RATE (a division when synced), SYNC, TRIG, PHASE, TARGET |
| **MATRIX** | (the page is the list) MACRO 1-4 and MODWHEEL on top, then 8 of the 16 slots: SRC, DEST, AMT (a bar from the middle), MODE | ^v the slot, <> the column, EDIT+arrows the value, EDIT on DEST: point at a card, OPT+EDIT clears the slot |
| **VOICE** | the 8 voices' levels now, the ones VOICES leaves out dim | VOICES, LEGATO, GLIDE, BEND, VELO, MASTER, QUALITY, PITCH, VOLUME |
| **ARP** | the steps it plays | ARP, MODE, OCTAVES, RATE, GATE |

TARGET (LFO page) and DEST (matrix): EDIT starts pointing (LEARN, as the pad LFOs' TARGET): the SOUND pages open on OSC A,
any card a matrix may move takes it with EDIT (TARGET fills the next free slot with this LFO, BI, amount 25), Y cancels.
**TABLES**: the 12 built-ins, then the WAVs in `wavetables/` beside the binary (read on the UI thread with `wt_load_wav`
into a fresh table, swapped into a user slot 12..31 by the audio thread through the command queue, the old one freed on
the UI thread); `wavetables/slots.cfg` keeps which file is in which slot, and they load again at start-up.
The side slots keep LISTEN, MIDI, PRESET (20 factory presets, `src/wpresets.h`, and the user's: presets.cfg keeps a WAVE
preset as its whole block, `1 NAME|W ...`; an old GRAND line is converted when the file is read).

## 11. How it is wired (2026-11-25)

1. **Type 1**: `Track.type = 1` is WAVE (GRAND's number: the user's decision that WAVE replaces it). The block is
   `Song.wv[NSLOT][256]`, appended to Song; `STATE_MAGIC` 0xB2B20013. A file of 0x11 or 0x12 ends before it and loads
   whole (its scenes too: `scene_up`). GRAND's bytes (`Track.sp`, `Song.gx`, `Song.gp`) keep their place and are only
   read for the conversion.
2. **One instrument per pad**: `static WaveInst wave[NSLOT]; static WaveStore wstore;` in engine.c. `engine_wave_init`
   builds the 12 tables once, on the UI thread: from the first `song_defaults` (the app, every qa harness) or
   `engine_start`, before the device opens; nothing plays a WAVE pad before (`wave_ready`).
3. **Notes**: `start_wave(slot, vel, semis, delay)`: GRAND's pitch, `48 + track_key + PITCH + FINE / 100 + semis`, and its
   gate (a step's length, `synth_gate`, else an eighth). A held key (`engine_trig_key`, CMD_TRIG_HELD) starts with gate -1
   and its tag is remembered with the note (`whold`); `engine_release_key` sends `wv_note_off`. The ARP is GRAND's code
   path (`arp_add` / `arp_run`) reading the bytes 96..100. Mute and STOP ALL: `wv_kill` (one block down to 0). The pad's
   VOICE (MONO / POLY, used by the roll) follows VOICES.
4. **Rendering**: after the sample / ACID / CLOUD voices, each WAVE pad with a sounding voice: `bpm` from the tempo,
   `tune` from the pad's PITCH modulation, `wv_render` into the pad's bus (the strip, inserts, sends, duck, freeze and the
   export follow as for every pad; freeze and export render through the same callback).
   **CPU guard** (`wave_par`): load over 50 % 6 voices, over 70 % 4 (as `pick_voice`); over 60 % QUALITY ECO and unison
   halved. It changes only the copy the pad plays with.
5. **Pad LFOs and automation**: `m_wv[s]`, a copy per buffer with `mod_byte` on wp 0..55 (`LT_ENG + k`): OSC A, OSC B,
   SUB, NOISE, FILTER, ENV1. The targets offered are the parameters marked *mod*. PITCH goes to `WaveInst.tune`.
6. **What went with GRAND**: its DSP (`synth_frame`, the PATCH voice, the DEEP LFO, the SPRING reverb), its DEEP and PATCH
   pages and cables, its 84 factory presets. `synth.h` keeps ACID's voice and GRAND's parameter names and ranges.

### An old GRAND pad becomes WAVE (`wv_from_grand`, main.c)

| GRAND | WAVE |
|---|---|
| OSC 1 / OSC 2 wave TRI, SAW | OSC A / B on BASIC at POS 33 (triangle), 67 (saw) |
| SQR, PULSE | on PWM at POS 0 (50 %), 85 (10 %), so PWM can move them |
| MIX m | A LEVEL 100 (1 - m), B LEVEL 100 m; an oscillator at 0 is off |
| TUNE 2, DETUNE | B SEMI, B FINE (cents) |
| OSC 1 / 2 RANGE (DEEP) | A OCT = range - 8', B OCT likewise |
| HARD SYNC (DEEP) | B at A's pitch with WARP SYNC, W.AMT = (ratio - 1) / 15 |
| NOISE | NOISE (white) |
| CUTOFF, RESO | FILTER LADDER, CUTOFF x 0.98 (9.8 octaves against 10), RESO x 4.2 / 3.9 |
| ENV | ENV AMT x 0.75 (+-6 octaves against +-8), ENV2 = GRAND's ADSR |
| ATTACK DECAY SUSTAIN RELEASE | ENV1 and ENV2: the same seconds (decay and release x 1.4: GRAND's are exponential), CURVE 70 |
| KB TRACK OFF / HALF / FULL (DEEP) | KEY TRK 0 / 50 / 100 |
| VCA GATE / DRONE (DEEP) | ENV1: attack 2 ms, sustain 100 |
| GLIDE, VOICE MONO / POLY | GLIDE, VOICES 1 / 8 (LEGATO off: GRAND attacked again from the level it was at) |
| LFO RATE / SYNC / WAVE, PITCH / CUTOFF / PWM AMT (DEEP) | LFO1 (free running), matrix: LFO1 -> A SEMI and B SEMI, -> CUTOFF, -> POS of a PWM oscillator, BI |
| ARP, MODE, OCTAVES, RATE, GATE (DEEP) | the host's ARP bytes |
| pad LFO / automation on TUNE 2, DETUNE, MIX, NOISE, CUTOFF, RESO, ENV, ADSR | the same on B SEMI, B FINE (depth / 4), B LEVEL, NOISE, CUTOFF, RESO, ENV AMT, ENV1 |
| HP, SPRING, ATTENUATOR, PATCH cables, LFO on GRAND DEEP knobs | nothing (let go) |

VELO 100 (GRAND's level followed velocity 1:1), MASTER 98: NEON's four GRAND pads come out within about 2 dB of before.
A GRAND pad that was never touched converts from GRAND's defaults. A pad that becomes WAVE in the app starts from the
INIT preset (two saws into the LP24).
