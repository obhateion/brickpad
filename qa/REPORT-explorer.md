# Explorer QA (cloud run 2026-11-25)

`qa/qa_explore.c` walks every screen and focusable element with single presses and short combos, hooks the draw
calls and checks: cursor stuck / far jumps, outlines over other elements or text, text outside its card, crashes.

## Findings (most severe first)

1. **CRASH: PATTERN on an ACID pad (and GRAND/CLOUD, sliced samples) reads past the lane list.** `lane_list()` kept
   the lane counts from before SUB was removed (ACID 6 for 5 lanes, GRAND 4 for 3, sliced 5 for 4), so the tab row got
   a garbage lane from the next static array; on x86 gcc -O0 `qa_nav` segfaulted in `lane_full()` drawing ACID's
   PATTERN tabs (`qa_quick` too). Fix: the counts match the lists. `src/main.c:1217`.
