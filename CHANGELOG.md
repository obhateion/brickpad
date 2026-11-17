# Changelog

## Unreleased

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
