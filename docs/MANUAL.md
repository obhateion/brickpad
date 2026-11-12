# BrickPad 1.1 alpha: manual

BrickPad is a sampler, synthesizer and clip sequencer for the TrimUI Brick. Everything is done with the d-pad and
the buttons; each screen shows the buttons that matter in its footer.

## 1. The idea

A project has **12 pads**. A pad holds one instrument and up to **16 patterns**. A pattern that plays is a **clip**:
clips start and stop on the bar (the LAUNCH quantum) and loop until stopped or until a follow action moves on.

## 2. Buttons

| Button | Meaning |
|---|---|
| D-pad | move the cursor |
| EDIT (B) | tap: act on what is under the cursor. Hold + d-pad: change its value (left/right fine, up/down coarse) |
| OPTION | listen to the pad. OPTION+EDIT: clear, or back to the default |
| PLAY | GRID: a tap starts every clip, the next tap stops everything. Other screens: a tap turns the selected pad's clip on or off at once, on the launch quantum. There is no hold |
| F1 / F2 | previous / next screen. F1+F2: PROJECT |
| SHIFT | a second layer: see each screen |
| X, Y | per screen: icon, mute, DEEP page, back |
| L1 / R1 | lanes, pages, groups |
| MENU | GRID > PADS > KEYS. Hold 3 s: quit |
| SHIFT+L2 | undo |

## 3. Screens

### GRID
The 12 pads with name, icon, pattern, steps and state.
- EDIT: start or stop the pad's clip. On an empty pad: choose an engine.
- PLAY: every clip with notes starts; again: everything stops.
- SHIFT+d-pad: select a rectangle of pads. PLAY or EDIT then launches them together, Y mutes them, SHIFT+Y freezes them.
- Y mute. X icon. SHIFT+X name. EDIT+left/right volume. SHIFT+OPTION copy the pad, SHIFT+EDIT paste.
- MENU: **PADS** (every button is a pad: a pad with notes launches its clip, an empty pattern plays the sound;
  SHIFT+F1/F2 choose a scene = one pattern number for all pads) and **KEYS** (the buttons play notes of the scale).

### INSTRUMENT
Top row: the instrument (EDIT+arrows change the engine, EDIT opens presets or the file browser) and the pattern
(EDIT opens the pattern window). Below: the steps or the piano roll, then the engine's panel.
- Steps: EDIT sets a step. L1/R1 choose the lane: TRIG, NOTE, VELOCITY, CHANCE, REPEAT (ratchets), SLICE,
  ACCENT / SLIDE on ACID, and four automation lanes. EDIT+arrows change the value under the cursor.
- STEP / ROLL switches the pattern to a **piano roll**: EDIT adds or removes a note, EDIT+left/right its length,
  EDIT+up/down its pitch, X+up/down velocity, X+left/right chance, L1/R1 the grid, Y back.
- SHIFT+X: RANDOM, a generator for the pattern.
- Panel: move to a control, EDIT+arrows turn it, OPTION+EDIT default, hold EDIT to hear. Y or DOWN past the last
  row opens the **DEEP** page.

### Pattern window
EDIT chooses a pattern, EDIT again or PLAY plays it, OPTION+EDIT clears it. The FOLLOW row sets what happens after
the pattern has played: AFTER n times THEN next / previous / first / random / other / jump / stop, OR another action
with a chance.

### FX
The pad's two inserts with their parameters and a live picture; L1/R1 walk through the pads, return A (reverb),
return B (delay) and the master. Y opens **MOD**: four LFOs per pad (shape, rate or tempo division, depth, target,
start on a note of this or another pad). EDIT on a target lets you point at any knob.

### MIXER
One strip per pad: two inserts, sends A and B, DK (duck), pan, fader, M (mute), S (solo), F (freeze); then the
returns and the master (compressor, limiter). EDIT+arrows change, Y mute, X solo. "FX nn%" shows what the inserts
cost; the limit is 40 % of the processor.

### PROJECT
SONG (tempo, swing, key, scale), PLAY (launch quantum, metronome, master), RECORD (count-in, quantise), FILE (open,
save, save as, export WAV or stems, new), SYSTEM (theme, library, hints).
- **Tap tempo**: on TEMPO press EDIT in time.

## 4. Engines

- **SAMPLER**: WAV / OGG. PITCH, FINE, ROOT (found from the name or the sound), GATE / POLY, direction, SLICES (equal,
  by transients, or edited by hand), WARP to the project tempo (BEATS, REPITCH, TONES, TEXTURE, COMPLEX), BARS and
  BPM of a loop (EDIT on BPM taps it), NORM (peak or loudness). DEEP: FADE IN, FADE OUT, XFADE between slices.
- **GRAND**: two oscillators, noise, ladder filter, envelope, glide, MONO / POLY. DEEP: LFO, sync, high-pass, key
  tracking, spring reverb, arpeggiator. PATCH: a patch bay with cables.
- **ACID**: a 303-style line: saw / square, filter with envelope, ACCENT and SLIDE per step, four distortions.
  DEEP: sub oscillator and more filter and amp controls.
- **CLOUD**: granular: POSITION, SPRAY, SIZE, DENSITY, SHIMMER, MOTION. DEEP: window, reverse, pitch, intervals,
  sync, freeze, attack, release, tone.

Every synth engine has factory presets (EDIT on the instrument row) and room for your own.

## 5. Effects, modulation, sidechain

Inserts: compressor (can be keyed from another pad), distortion, chorus, flanger, phaser, filter, bitcrusher, tape,
autopan, ring modulator, multiband. Sends: reverb and tempo delay.

Automation: four lanes per pattern, any knob as the target, straight or bent segments. LFOs add on top.

**DUCK window** (EDIT on a strip's DK row): a curve of 16 points of volume. LEFT/RIGHT choose a point, EDIT+up/down
set it, EDIT+left/right draw on. Fields: TRIGGER (NOTE of the source pad, SOUND passing a threshold, GRID on the
tempo), SOURCE, AMOUNT, LENGTH, EVERY or THRESHOLD, SMOOTH, SHAPE (eight ready curves).

## 6. Freeze

SHIFT+Y on GRID or the F row of the mixer renders a pad, with its effects and modulation, into audio. A frozen pad
costs almost nothing; volume, pan, sends, mute, duck and follow actions stay live. Editing the pad thaws it.
Recordings are kept on the card, so a project opens without rendering again.

## 7. Recording and files

MENU+PLAY records what you play into the pattern, with a count-in if set; what was played without recording can be
captured afterwards. Projects are saved under a name typed on the on-screen keyboard (arrows choose a key, EDIT
types, OPTION deletes, L1/R1 move the cursor, PLAY or OK finishes). EXPORT writes a WAV of the song or one per pad.

## 8. Samples

Put WAV or OGG files in `samples/` in any folders. Key, tempo and bars are read from file names where they are
written (`120bpm`, `Amin`) and from the sound otherwise.

## 9. Install, update, remove

1. Unpack `brickpad-1.1-alpha-trimui-brick.zip`. Copy `BrickPad.sh` and the folder `brickpad` into `roms/ports/` on
   the card (on the Brick: `/userdata/roms/ports/`).
2. Put WAV or OGG files into `roms/ports/brickpad/samples/`. No samples are included.
3. Update the game list on the Brick and start BrickPad from Ports. Hold MENU for 3 seconds to quit.

Your work stays in `roms/ports/brickpad/`: `state.bin` (autosave), `projects/`, `exports/`, `frozen/`, `themes/`.
To update, replace `BrickPad.sh` and `brickpad/brickpad` only. To remove BrickPad, delete both.

The demo project `DEMO/NEON` uses the packs *Electroclash* and *Dark Cinematic Electronica* in
`samples/ELECTROCLASH` and `samples/CINEMATIC`; without them only its synth pads sound.
