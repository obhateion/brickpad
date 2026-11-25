# Explorer QA (cloud run 2026-11-25)

`qa/qa_explore.c` walks every screen and focusable element with single presses and short combos, hooks the draw
calls and checks: cursor stuck / far jumps, outlines over other elements or text, text outside its card, crashes.

## Findings (most severe first)

1. **CRASH: PATTERN on an ACID pad (and GRAND/CLOUD, sliced samples) reads past the lane list.** `lane_list()` kept
   the lane counts from before SUB was removed (ACID 6 for 5 lanes, GRAND 4 for 3, sliced 5 for 4), so the tab row got
   a garbage lane from the next static array; on x86 gcc -O0 `qa_nav` segfaulted in `lane_full()` drawing ACID's
   PATTERN tabs (`qa_quick` too). Fix: the counts match the lists. `src/main.c:1217`.

2. **CHORDS layout shows the ROLL.** PATTERN of a pad whose clip has notes, LEFT onto LAYOUT, EDIT+DOWN three times
   (ROLL, RHYTHM, CHORDS): the "CLEAR P1 FOR CHORDS?" question comes; after EDIT (YES or NO) the clip is CHORDS (the
   LAYOUT slot says CHORDS) but the screen stays the ROLL. Leaving the pad (Y) and entering again (EDIT on the pad, or
   F2/F1) also opened the ROLL for a CHORDS clip: `cat_open()` only called `roll_open()`. Expected: the CHORDS page.
   Fix: `cat_open` opens CHORDS for mode 2 (`src/ui.h:309`), and both answers of the question open it
   (`src/main.c` `cfm_do` CF_CHORD and `cfm_key`'s NO). Test: qa_explore "chords kept".

3. **Two cursors when the quick bar has it.** HOME (and MIXER, PATTERN STEPS, RHYTHM, CHORDS, AUTOMATION, PATTERNS):
   UP to the bar, or X: the bar's button gets the white outline and the page's cursor keeps its identical white outline,
   so two places look focused. Expected: one cursor. Fix: while `qb_focus || qd_open` the page's white outlines are drawn
   dim (`cur_dim` set around the page chain in `draw()`, `src/main.c` ~4962/4990; `rframe()` dims, `src/ui.h:76`).
4. **ROLL: arrows inside a long note show nothing.** PATTERN of GRAND 06 (a roll): the cursor on the 6-step note, RIGHT
   x3 moves the roll's time cursor but the screen does not change (the whole note stays framed, "bar 1.1" stays). Fix: a
   white mark under the cursor's cell inside the framed note (`draw_roll`, `src/main.c` ~4253).
5. **RHYTHM: the cursor's outline under the next notes.** RHYTHM layout, ACID 05: the selected note's white outline was
   drawn in the notes loop, so the following notes covered its right side. Fix: drawn after the loop
   (`draw_rhythm`, `src/main.c` ~4198). Also the bar number of the clip's end line ("3") was drawn past the card's right
   edge: no number where it does not fit (`src/main.c:4185`).
6. **ACID SOUND MAIN: the A1/L1 badges over "CUTOFF".** Two badges (A1 automation, L1 LFO) on a knob card covered the
   right half of the centred label. Fix: the label moves left of the badges (`knob_draw` `kd_right`, `src/sound.h:262`).
7. **Drawer: the MORE card's title under the first tile's outline.** X X, the cursor on RESAMPLE: the outline (y 76)
   cut into "MORE" (y 62..79). Fix: the tiles start at y 84 like MASTER's (`qd_layout`, `src/quick.h:275`).
8. **UTF-8 file names: blank cells before the name, cut too early.** NEON's pads 1-3 are "✧SPECTRE", "✧CYBRSNARE",
   "✧UFO": `ftext()` skipped the 3 bytes of ✧ but still advanced 3 cells, and `ftext_w()`/`fit()` counted them, so HOME
   showed "   SPECTRE" and "CYBRS..". Fix: bytes >= 128 take no room in `ftext()` and `ftext_w()` (`src/ui.h:46,62`).
