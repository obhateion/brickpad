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
- [~] 3 UI: SOUND pages for WAVE (sound.h `snd_wav`, pictures and MATRIX and TABLE browser in `src/wvui.h`) — written,
  compiling, not yet walked by a test.

## Next
- qa: build every qa/*.c, fix what used GRAND; add qa/qa_wave_ui.c; golden NEON (levels before/after below).
- docs: WAVE.md (wired), CHANGELOG, README.

## NEON levels (4 bars, each pad solo; this container's stand-in samples)
before (GRAND): pad 6 -19.3 dB, 7 -37.2, 8 -33.2, 9 -42.9
