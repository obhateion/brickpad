# BrickPad workflow v3 (draft, 2026-11-20)

Source: the user's text of 2026-11-20 and two form answers. Everything here waits for the user's approval of step 1.

## Goals
1. A pad starts with its engine. The engine decides what every other section shows.
2. Strict M8 key model: EDIT, OPTION, SHIFT, PLAY, the d-pad and their combos. Nothing else navigates.
3. Exceptions: F1/F2 page the tabs. L1/R1 and L2/R2 do the same everywhere except PADS and KEYS play modes.
4. When combos run out, the cursor goes onto the thing on screen and EDIT changes it. No extra combos.
5. The same combo does different things on different screens (context), as on M8.

## Screen frame
```
[MET][VIB][MON][CAP][<<][>>][PADS][KEYS] 120 >    quick bar (top by default, bottom selectable in SETTINGS)
[01][02][03]>04<[05][06][07][08][09][10][11][12]   pad strip (hideable in SETTINGS)
 PATTERN  >SOUND<  FX  MOD  MIX  LOOK              section strip (hideable in SETTINGS)
+------------------------------------------------+
|                  section page                  |
+------------------------------------------------+
```
- Pad: SHIFT+LEFT/RIGHT. Section: F1/F2, L/R1, L/R2.
- The quick bar is reached with UP past the top of the page (DOWN past the bottom when the bar is at the bottom). EDIT
  presses the button under the cursor.
- Quick bar buttons: MET (metronome sound), VIB (metronome vibro), MON (monitoring/pre-listen), CAP (MIDI capture),
  << and >> (nudge back/forward for live sync without MIDI clock), PADS and KEYS (play modes), tempo, play state.
- PADS/KEYS play modes: every key is a pad or a note. Way out: the same quick bar button or SHIFT+OPTION.

## SOUND
- Round knobs like Serum on every engine (SAMPLER GRAND ACID CLOUD), with a ring for modulation depth and a mark for
  automation. The drawn Moog/303 panels go.
- The main curves (wave, filter response, envelopes) on one screen. No 5-page MORE.
- Engine switch: hold EDIT with the cursor on the SOUND tab of the section strip. Not a row inside the page.

## PATTERN
- Layout picker: STEP SEQUENCER (default, 16 steps), PIANO ROLL, CHORD PLAYER.
- Layout settings (length, lanes, range) open from inside the layout.
- Growing grid: the cursor past the last step adds a whole bar (16 steps, up to 64); an empty bar goes when the cursor
  leaves it backwards. The piano roll's note range grows the same way.
- Pattern settings stay apart from clip launch and scene logic.

## Proposed key map (step 1, needs the user's approval)
| Now | v3 |
|---|---|
| Y = back | MENU+arrows = M8-style screen map: MENU+UP/DOWN = PROJECT <-> HOME <-> pad; MENU+LEFT/RIGHT = pad sections (PATTERN SOUND FX MOD MIX); pad stays SHIFT+LEFT/RIGHT |
| enter a pad from HOME | EDIT (as now) |
| X combos (triad, velocity, chance, bend, new point) | the value becomes a column/lane under the cursor, EDIT+arrows |
| SHIFT+X random, double, clear, copy | OPTION tap = context menu of the thing under the cursor (approved) |
| X / Y mute, solo, insert on/off | drawn M and S buttons on each pad/strip: cursor + EDIT; inserts: cursor + EDIT |
| range select (X+arrows) | OPTION+arrows |
| MENU+PLAY record (once rec, twice capture) | stays MENU+PLAY; CAP also on the quick bar |
| MENU+X metronome | MET / VIB on the quick bar |
| MENU = PADS mode | PADS / KEYS on the quick bar |
| MENU held = exit BrickPad | SHIFT+PLAY HELD = quit with the same hold animation as MENU now (user 2026-11-20), also PROJECT > FILE > QUIT; Knulli softreset rule untouched |

## Steps
1. Key model + quick bar + two strips. Remove old X/Y/MENU paths after the user's check.
2. SOUND on round knobs for all engines + engine switch on the SOUND tab. Remove the Moog/303 panels and MORE pages.
3. PATTERN layouts with the growing grid.
Each step: build, install to roms/ports/brickpad, the user's check, then old code removal, commit.
