# PLAN-TRACKER

Progress log — updated after every file is created.

## Planning phase

| Task                        | Status    | Notes                                      |
|-----------------------------|-----------|---------------------------------------------|
| PLAN.md                     | ✅ Done   | Lesson table, file tree, JS→C mapping       |
| README.md                   | ✅ Done   | Course overview, build instructions         |
| PLAN-TRACKER.md             | ✅ Done   | This file                                   |

## Phase 2 — Source files

| Task                        | Status    | Notes                                      |
|-----------------------------|-----------|---------------------------------------------|
| course/build-dev.sh         | ✅ Done   | Added --debug flag with ASan/UBSan          |
| course/src/utils/math.h     | ✅ Done   | Added CLAMP, double-eval hazard explanation |
| course/src/utils/backbuffer.h | ✅ Done | ASCII memory diagram, ARGB explanation      |
| course/src/utils/draw-shapes.h | ✅ Done | Parameter docs with JS ctx.fillRect equiv  |
| course/src/utils/draw-shapes.c | ✅ Done | Clipping, pitch/4, alpha blend explained   |
| course/src/utils/draw-text.h  | ✅ Done  | Full bitmap encoding diagrams               |
| course/src/utils/draw-text.c  | ✅ Done  | Bit-masking explained, cursor advancement   |
| course/src/platform.h       | ✅ Done   | Casey latency model diagram, swap warning   |
| course/src/utils/audio.h    | ✅ Done   | Phase accumulator, CRITICAL phase note      |
| course/src/game.h           | ✅ Done   | Added passed_initial_delay, cleaned debug   |
| course/src/game.c           | ✅ Done   | Fixed DAS bug, removed debug prints         |
| course/src/audio.c          | ✅ Done   | Procedural SFX + sequencer explained        |
| course/src/main_raylib.c    | ✅ Done   | Cleaned up debug code, Casey model          |
| course/src/main_x11.c       | ✅ Done   | XkbDetectableAutoRepeat, VSync explained    |

## Phase 3 — Lesson files

| Task                                                          | Status    | Notes                          |
|---------------------------------------------------------------|-----------|--------------------------------|
| course/lessons/01-window-and-backbuffer.md                    | ✅ Done   | ~20KB, full pipeline           |
| course/lessons/02-drawing-primitives.md                       | ✅ Done   | ~25KB, clipping, alpha blend   |
| course/lessons/03-input.md                                    | ✅ Done   | ~27KB, union, double-buffer    |
| course/lessons/04-game-structure.md                           | ✅ Done   | ~38KB, 3-layer architecture    |
| course/lessons/05-tetromino-data.md                           | ✅ Done   | 681 lines, rotation diagrams   |
| course/lessons/06-tetromino-movement.md                       | ✅ Done   | 850 lines, DAS/ARR timing      |
| course/lessons/07-the-playfield.md                            | ✅ Done   | 943 lines, gravity + locking   |
| course/lessons/08-line-clearing.md                            | ✅ Done   | 1159 lines, bottom-to-top      |
| course/lessons/09-scoring-and-levels.md                       | ✅ Done   | 765 lines, bit-shift scoring   |
| course/lessons/10-ui-sidebar-and-text.md                      | ✅ Done   | 840 lines, bitmap font         |
| course/lessons/11-audio-sound-effects.md                      | ✅ Done   | 839 lines, phase accumulator   |
| course/lessons/12-audio-background-music.md                   | ✅ Done   | 517 lines, MIDI sequencer      |
| course/lessons/13-polish-ghost-and-game-over.md               | ✅ Done   | 959 lines, ghost + game over   |

## Phase 4 — Retrospective

| Task                            | Status    | Notes                                    |
|---------------------------------|-----------|------------------------------------------|
| COURSE-BUILDER-IMPROVEMENTS.md  | ✅ Done   | 6 patterns, 6 gaps, 5 edits, 12 concepts |
