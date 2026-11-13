# Changelog

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
