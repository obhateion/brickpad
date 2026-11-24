# Changelog

## Unreleased

- Side slots: four buttons on each side of a pad's page, the page narrower by them. The arrow that reaches the page's
  left or right edge goes onto the slot at that height, ^v walk a side, the arrow back returns where the cursor was.
  EDIT presses, EDIT+arrows turn. PATTERN (STEPS, PIANO ROLL, CHORDS): LAYOUT, LEN/GRID (EDIT+^v a bar, EDIT+<> the
  grid, EDIT the fine length), PATTERN / FOLLOW, SELECT (TRIAD or ACCENT on the roll, CHORD LENGTH on CHORDS), RECORD
  (arm / stop, as MENU+PLAY), LISTEN (a step or a note sounds when the cursor lands on it; kept in ui.cfg), DOUBLE
  (VELOCITY / CHANCE on the roll), RANDOM. SOUND: LISTEN, MIDI LEARN. FX: INSERT on/off. MOD: AUTOMATION or LFO, DUCK.
  OPTION's menu keeps UNDO, COPY, PASTE, CLEAR and the rare items; MIX lost MUTE and SOLO (its M and S buttons).
- STEPS: <> stay in the row of eight (its ends lead to the slots); v from the bottom row is the next bar and grows the
  clip a whole bar past its end, ^ from the top row the bar before. The "16 steps / 1/16" line went onto LEN/GRID.
- CHORDS: <> no longer wrap round the row.
- Quick bar picker: a line under each button saying what it does.
- METRONOME: its own PROJECT section. SOUND and VIBRATION apart (vibration alone is a silent metronome: the bar's first
  buzzes longer), VOLUME, the SOUND OF IT (CLICK, WOOD, BEEP), WHEN (always or only recording and its count-in), EVERY
  (1/4, 1/8, 1/16), VIBRATION TIMING (ms). MENU+X anywhere: OFF, SOUND, SOUND + VIBRATION, VIBRATION. A device setting
  (metro.cfg); it never clicks into an export or a freeze. The footer shows M (sound) and a buzzing phone (vibration).
- PROJECT regrouped by task: FILE, TEMPO, METRONOME, PLAY / REC, OUTPUT, MIDI, SETTINGS, HELP (was ABOUT).

## 1.2 Beta (2026-11-20)

- CHORDS on a clip that has notes asks first: YES starts on an empty clip, NO puts the chords over the notes, Y stays.
- Insert costs measured again on the Brick (tools/fxcost.c): REVERB 28, DELAY 10, FILTER 9, CHORUS 11, FLANGER and PHASER 10, MULTIBAND 24.
- PROJECT > ABOUT: the version, the new keys (grid, SUB / PRB, the clip's MODE, DUCK points).
- STEPS: EDIT puts the note as it goes down; held, the arrows change it. Drag-painting is gone.
- Headphones (R2) in STEPS and the piano roll: hear the note being put, or not.
- PATTERN list: a RANDOM card. EQUALIZER insert (shelves, a bell, low and high cut).
- MIDI (PROJECT > MIDI): the USB port (remembered by name, found again when plugged in), CLOCK OUT (Start, Stop, 24 a
  beat), notes in as KEYS (the open pad at their pitch) or PADS (36-47 = pads 1-12) with velocity, a channel or ALL.
- AUTOMATION drawn as DUCK: hollow points, the one under the cursor filled, a bend dot half way along each stretch (orange when bent).
- PRB modes: EDIT on a step of the PRB lane: NOTE (the chance plays it), SUB (the chance splits it into its cells), JUMP (another step's note or slice, or a scale tone / slice of its own, from a LIST of 3 with their chances or a RANGE). Format 0x11, older files load whole.
- SUB (was REP): a step of up to 8 cells; EDIT on a step of the SUB lane opens them: EDIT on/off, EDIT+^v velocity, EDIT+<> note (slice), L1/R1 how many, OPT hears, OPT+EDIT as the step. Format 0x10: older files load whole, scenes too.
- CHORDS (the clip's MODE on PATTERN's first card: EDIT + arrows go round STEPS > PIANO ROLL > CHORDS, EDIT opens it; L2 no longer switches): 7 chords of the pad's key and scale as triads, sevenths, sus4; the clip in slots of LENGTH (X); EDIT puts the chord and goes on, OPT hears, R2 inversion, EDIT+^v octave, OPT+EDIT clears a slot. The chords are roll notes.
- DUCK curve as up to 8 points (as ShaperBox): <> chooses, EDIT+^v level, EDIT+<> move, X a new point, X+^v the bend to the next, OPT+EDIT deletes; the pad's sound drawn under the curve as it passes. Presets stay tables until a point is touched.
- STEPS: SHIFT+L1/R1 on the NOTE lane changes the grid (the clip's DIVISION 1/32..1/1) on the spot.
- RING MOD SOURCE (right of FREQ): OSC or any pad; that pad's sound is the carrier (mute stops it, volume 0 keeps it).
- FILTER: 12 modes (LOW, HIGH, BAND, NOTCH, LOW 24, HIGH 24, BAND 24, PEAK, COMB, VOWEL, DJ, LADDER); the old four keep their numbers.
- REVERB and DELAY as pad inserts (the returns' DSP: SIZE DECAY DAMP PREDELAY LOW CUT MIX / TIME FEEDBACK TONE PING-PONG LOW CUT MIX); up to 4 of each at once, more stay dry.
- The count (and so the MIDI clock and the metronome) stops as soon as no clip plays or waits; the next clip starts the bar at the press. Not with CLOCK IN.
- Footer metronome again (as in 1.2): the tempo now and a light per beat, the first in orange.
- Browser: R2 turns LISTEN on/off (off: the arrows are silent, OPTION hears one), kept in listen.cfg.
- OUTPUT TRIM (PROJECT > PLAY): 0..+24 dB after the limiter and the clip, to the speaker only (exports stay as they are).
- TEMPO: L1/R1 nudge shows FASTER/SLOWER, the percent and the BPM it gives; TAP averages up to 12 taps (a straight line through them).
- EXPORT menu with settings as in Ableton (what, scenes, loop, tail, bits, mono, normalize).
- MIDI CLOCK IN: BrickPad follows the other device (tempo from the clocks, the phase pulled to theirs, its Start and
  Stop). MIDI knobs: R2 on a SOUND card, then turn a knob on the controller (midimap.cfg); R2 again unlinks.
- brickpad.log keeps the last run as brickpad.log.1, writes a crash with its call stack, and the free memory every 30 s.
- MODULATION: CHORUS, FLANGER and PHASER are one effect with a MODE card; old projects play as before.

## 1.1 Beta (2026-11-18)

- Global and pad stops land on the PROJECT → LAUNCH grid. Press again before the boundary to cancel a queued stop.

- SWITCHBACK: an original electro/breakbeat demo at 124 BPM with four scenes and its own bundled drum kit. It replaces HOUSE. NEON is included.

- OPTION+EDIT on a whole thing asks YES / NO first (NO is under the cursor), then puts it back as in an empty project:
  a pad on HOME (or every selected pad): its sound, effects, patterns, mix, look and name on this scene; a card inside a
  pad: PATTERN (all 16 patterns), SOUND (no instrument, no sample), FX, MOD, MIX, LOOK; PATTERN's list: STEPS (this
  pattern), SELECT (all 16), RECORD (its settings); MOD's list: the LFOs, the automation, the duck; a PROJECT section
  (SONG, PLAY, RECORD, INTERFACE, MIXER, SYSTEM = the theme). SHIFT+L2 undoes it. On a PROJECT setting OPTION+EDIT sets
  its default at once.
- A scene is deleted after YES (was OPTION+EDIT twice); the open one hands over to the next scene, then goes.
- F1+F2 held 3 s goes to the simple view; the MENU quit is a red ring.

## 1.0 beta (2026-11-16)

A new interface and scenes. The numbering starts again with this beta.

- Interface: HOME (PADS, SCENES, MIXER) and inside a pad its categories PATTERN, SOUND, FX, MOD, MIX, LOOK, each with
  pages of four cards; breadcrumbs on top, the keys of the screen in the footer, Y back everywhere, F1+F2 PROJECT.
  SHIFT+arrow inside a pad jumps to a category; PAD MENU can be a tree; PROJECT > ABOUT lists every key.
- Scenes: a scene is its own set of twelve pads (sounds, effects, mix, patterns, follow actions). It changes on the
  bar without a stop; new empty, copy, delete; up to 16. SCENE MIX shares the mix of the same sound between scenes.
  Export renders every scene in turn.
- Mixer: inserts, sends, fader dB and level dB on every strip; a drawer with reverb, delay, master and the pad's
  effects, on the right or the left; strips slide under it.
- Live pictures on SOUND and FX pages; the FX filter follows its mode and shows Q.
- Simple view with PLAY, NOTES and SOUND, a kid lock, five themes, pad icons with colours, a pad strip (hidden, thin,
  big).
- Demo HOUSE: six scenes on all four engines.
- Licence: all rights reserved, personal use (LICENSE).

## 1.2 alpha (2026-11-13)

- Sampler: the note lane is called PITCH. With a sliced sample the lanes start with SLICE (SLICE, PITCH, VELOCITY,
  CHANCE, REPEAT) and the cursor starts there.
- A step is removed only on its PITCH / NOTE lane. On the other lanes a tap or OPTION+EDIT leaves the step alone.
- A new step starts from the last value set on each lane of the pad (pitch, velocity, chance, repeats, slice).
- OPTION+EDIT on an automation lane's target unlinks it and keeps its points; on an LFO's TARGET it unlinks the LFO.
- Compressor: the sidechain field is called SRC.
- Sound: high PITCH no longer crackles. Pitch from automation and LFOs moves smoothly inside each audio buffer, and a
  sample played much faster than its own speed is read with a simple anti-aliasing filter.
- The PROJECT screen shows the version, V1.2.

## 1.1 alpha (2026-11-12)

- Tap tempo: press EDIT in time on PROJECT > TEMPO, and on the sampler's BPM plate for a loop's own tempo.
- PLAY on INSTRUMENT, FX and MIXER: a tap turns the selected pad's clip on or off at once, on the launch quantum.
  There is no hold. On GRID PLAY still starts every clip and stops everything.
- Fixes: footer hints that ran off the screen; a saved project no longer changes when it is loaded.
- The PROJECT screen shows the version, V1.1.

## 1.0 alpha (2026-11-12)

First public version.

- Four engines: SAMPLER, GRAND, ACID, CLOUD, each with a drawn panel and a DEEP page.
- Step sequencer and piano roll, 16 patterns per pad, follow actions, scenes.
- Group selection on GRID: launch, mute and freeze several pads at once.
- Mixer with 11 insert effects, reverb and delay sends, master compressor and limiter. The insert limit follows the
  measured cost of each effect (up to 40 % of the CPU).
- LFOs, automation with bends, sidechain; the DUCK window with a 16-point curve and NOTE / SOUND / GRID triggers.
- Freeze of pads with their effects and automation; recordings cached on the card.
- Sampler: fade in, fade out and crossfade between slices; normalize (peak or loudness).
- Projects, WAV and stems export, themes, presets, pad icons and names, an on-screen keyboard for names.
