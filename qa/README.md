# BrickPad test harnesses

Each `qa_*.c` includes `src/engine.c` and `src/main.c` and drives the real UI through `key_event()` (the same entry the
Brick's buttons use), with a software SDL renderer and fake time. Run them from a folder that holds `samples/` and
`projects/DEMO/NEON.bpp` (copy `demo/NEON.bpp` and `demo/samples`). On the Brick: aarch64 zig cc, one file at a time.

PC / CI build (Linux, libsdl2-dev): `cc -O0 -std=gnu11 -w -Isrc qa/qa_nav.c src/sample.c -lSDL2 -lm -lpthread -o qa_nav`.
`golden.c` + `run.sh` compare the export of NEON with `base-0-O0.txt` (the sound must not change).

WAVE (2026-11-25): `qa_wave_ui.c` plays a WAVE pad through the UI (every SOUND page, the matrix, the tables, held keys,
the ARP, the presets, pad LFOs), converts NEON's GRAND pads and a hand-written 0x12 file, saves and loads; prints ALL OK.
The DSP has its own checks in `tests/qa_wave.c` (`cc -O2 -std=gnu11 -Isrc tests/qa_wave.c -o qa_wave -lm`).
