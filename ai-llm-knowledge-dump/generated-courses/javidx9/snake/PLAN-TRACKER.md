# Snake Course — PLAN-TRACKER.md

Progress log. Updated after every file is created.

---

## Phase 1 — Planning files

| Task | Status | Notes |
|------|--------|-------|
| `PLAN.md` | ✅ Done | 12-lesson table, file tree, JS→C map, full analysis vs course-builder.md |
| `README.md` | ✅ Done | Course overview, build instructions, lesson list, architecture |
| `PLAN-TRACKER.md` | ✅ Done | This file |
| Source vs course-builder.md analysis | ✅ Done | 15 conformances, 12 gaps, 10 planned upgrades documented in PLAN.md |

---

## Phase 2 — Source files

| Task | Status | Notes |
|------|--------|-------|
| `course/build-dev.sh` | ✅ Done | `--backend x11\|raylib`, `-r`, `-d`/`--debug` (ASan+UBSan), `-DDEBUG` |
| `course/src/utils/math.h` | ✅ Done | `CLAMP`, `MIN`, `MAX`, `ABS` macros |
| `course/src/utils/backbuffer.h` | ✅ Done | `SnakeBackbuffer` with `pitch`; `GAME_RGB`/`GAME_RGBA`; colour constants |
| `course/src/utils/draw-shapes.h` | ✅ Done | `draw_rect` / `draw_rect_blend` declarations |
| `course/src/utils/draw-shapes.c` | ✅ Done | Clipped fill loop; `py * (pitch/4) + px`; alpha blend |
| `course/src/utils/draw-text.h` | ✅ Done | `draw_char` / `draw_text` declarations |
| `course/src/utils/draw-text.c` | ✅ Done | **8×8 ASCII-indexed font** `FONT_8X8[128][8]`; `(0x80>>col)` BIT7 mask |
| `course/src/utils/audio.h` | ✅ Done | `SoundDef`, `SOUND_ID` enum, `SoundInstance`, `AudioOutputBuffer`, `GameAudioState` |
| `course/src/game.h` | ✅ Done | `GAME_PHASE` enum, `SNAKE_DIR`, `Segment`, `GameButtonState`, `GameInput` (union + `BUTTON_COUNT 2`), `GameState` (with `GameAudioState audio`), `DEBUG_TRAP`/`ASSERT`, public API |
| `course/src/game.c` | ✅ Done | Snake logic; `GAME_PHASE` dispatch; `draw_cell`; calls utils/; spatial audio pan |
| `course/src/audio.c` | ✅ Done | `SOUND_DEFS` table; `game_play_sound_at`; `game_play_sound`; `game_get_audio_samples` |
| `course/src/platform.h` | ✅ Done | **4-function** contract: `platform_init`, `platform_get_time`, `platform_get_input`, `platform_shutdown` |
| `course/src/main_x11.c` | ✅ Done | X11 + GLX + ALSA + VSync + main() |
| `course/src/main_raylib.c` | ✅ Done | Raylib + audio stream callback + main() |

**Build verified:** `./build-dev.sh --backend=x11` → 0 warnings, 0 errors.

---

## Phase 3 — Lesson files

| Task | Status | Notes |
|------|--------|-------|
| `lessons/01-window-and-backbuffer.md` | ✅ Done | X11 window, SnakeBackbuffer, GAME_RGB, glTexImage2D loop |
| `lessons/02-drawing-primitives-and-grid.md` | ✅ Done | draw_rect clipping, alpha blend, draw_cell, FIELD_Y_OFFSET |
| `lessons/03-input.md` | ✅ Done | GameButtonState, UPDATE_BUTTON, XPending drain, 2-arg prepare_input_frame |
| `lessons/04-game-structure.md` | ✅ Done | GameState, GAME_PHASE enum, switch dispatch, next_direction, dt accumulator |
| `lessons/05-snake-data-and-ring-buffer.md` | ✅ Done | Ring buffer mechanics, grow_pending, snake_spawn_food, snake_init |
| `lessons/06-snake-movement.md` | ✅ Done | Move tick, direction commit, DX/DY, wall/self-collision, speed scaling |
| `lessons/07-turn-input.md` | ✅ Done | Direction queuing, U-turn guard, CW/CCW arithmetic |
| `lessons/08-food-spawning.md` | ✅ Done | Retry loop, ring buffer scan, grow_pending tail defer |
| `lessons/09-collision-detection.md` | ✅ Done | Bounds check, self-collision scan, GAME_PHASE transition on death |
| `lessons/10-restart-and-speed-scaling.md` | ✅ Done | Game-over modal, memset+preserve pattern, speed formula, overlay rendering |
| `lessons/11-audio.md` | ✅ Done | Phase accumulator, square wave, envelopes, frequency slide, spatial pan |
| `lessons/12-polish-and-utils-refactor.md` | ✅ Done | Painter's algorithm, GLYPH_STRIDE, snprintf, utils/ refactor, 10-upgrade table |

---

## Phase 4 — Retrospective

| Task | Status | Notes |
|------|--------|-------|
| `COURSE-BUILDER-IMPROVEMENTS.md` | ✅ Done | 10 upgrades; 10 preserved conformances; JS analogies for each |
