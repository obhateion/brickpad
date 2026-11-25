# WAVE replaces GRAND (type 1): progress

Branch `synth-integrate` (from workflow-v3 + synth-wave). Updated with every push.

## Done
- [x] 1 Engine: `src/engine.c` plays type 1 through `WaveInst wave[NSLOT]` + one `WaveStore` (built in `engine_wave_init`:
  first `song_defaults` / `engine_start`, before the device). `start_wave` (GRAND's pitch and gate), held keys
  (CMD_TRIG_HELD tags -> `wv_note_off`), ARP (bytes WP_ARP.. in the block), mute / STOP ALL (`wv_kill`), render into
  the pad bus, CPU guard (`wave_par`: fewer voices, ECO, half unison under load), pad LFO / automation on wp 0..55
  (`m_wv`, LT_ENG + k) and PITCH (`WaveInst.tune`), user tables swapped in through the queue (CMD_WTABLE).
  GRAND's DSP, DEEP LFO, PATCH and SPRING are gone (synth.h keeps ACID's voice and GRAND's parameter names for the conversion).
- [x] 2 Data: `Song.wv[NSLOT][256]` appended, STATE_MAGIC 0x13; 0x11/0x12 files load whole (scenes too); `wave_fix`
  converts every type-1 pad from its GRAND settings (`wv_from_grand`, main.c) incl. its LFO / automation targets;
  20 factory presets (`src/wpresets.h`); old GRAND user presets convert on load; WAVE user presets keep the whole block.
- [x] 3 UI: SOUND pages for WAVE (sound.h `snd_wav`: OSC A, OSC B, UNISON, SUB, FILTER, ENV, LFO, MATRIX, VOICE, ARP;
  pictures, the MATRIX list and the TABLE browser in `src/wvui.h`); LFO TARGET / matrix DEST by pointing at a card
  (LEARN); names, footers, HELP, hints say WAVE.
- [x] 4a qa/qa_wave_ui.c ALL OK (now with a FREEZE of a WAVE pad); all 17 qa/*.c ALL OK, tests/qa_wave.c ALL OK (fixed on the way, both older than this branch: `lane_list` read past
  its lists since SUB went (qa_nav, qa_quick crashed); OPT+EDIT on a SOUND card of a roll pad cleared the roll).

- [x] 5 docs: docs/WAVE.md (sections 7, 8, 10, 11 as wired, with the GRAND -> WAVE table), CHANGELOG, README (EN, RU),
  MANUAL, qa/README; docs/screenshots/wave.png replaces grand.png.

## Next (not done)
- On the Brick: `sh qa/run.sh` for the real golden base (this one was made with stand-in samples); listen to NEON's
  pads 6..9 against GRAND; time the start-up (the 12 tables, ~1 s estimated on the A53).
- No spring reverb, HP filter or patch cables in WAVE: a converted pad that used them sounds drier / different.

- [x] 4b golden: the NEON export is finite and not silent (whole mix -19.0 dB RMS before, -18.8 dB after, peak 0.890 both),
  WAVE pads audible (below); qa/base-0-O0.txt regenerated HERE, with stand-ins for NEON's four CINEMATIC samples (this
  container has no Brick card): on the Brick run `sh qa/run.sh` once and take its output as the base. A 2000-step random
  golden run (UI fuzz) passes; qa_wave_ui is clean under -fsanitize=address,undefined.

## NEON levels (4 bars, each pad solo; this container's stand-in samples)
before (GRAND): pad 6 -19.3 dB, 7 -37.2, 8 -33.2, 9 -42.9
after (WAVE):   pad 6 -18.9 dB, 7 -39.5, 8 -33.2, 9 -40.9 (peaks 0.243/0.104/0.067/0.066 -> 0.257/0.071/0.075/0.089)
