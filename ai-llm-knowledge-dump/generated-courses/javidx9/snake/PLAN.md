# Snake Course — PLAN.md

## Final Game Description

The finished game is a classic Snake clone rendered entirely on a CPU pixel buffer: a 60×20 grid
(20 px cells, plus a 3-row score header) where the player steers a growing snake to eat food
pellets. Each pellet eaten adds 5 body segments, increments the score, triggers a short sound
effect, and slightly increases the snake's speed. The game ends on wall or self-collision (game-over
sound plays); a semi-transparent overlay shows "GAME OVER", the current score, and keyboard hints.
Pressing R restarts while preserving the best score. The game compiles for two platform backends
(X11/GLX and Raylib) via a single unified `build-dev.sh` script, sharing one platform-independent
`game.c`/`game.h` + `audio.c` layer that never calls X11 or Raylib directly.

---

## Lesson Sequence

| # | Title | What gets built | What the student runs/sees |
|---|-------|-----------------|---------------------------|
| 01 | Window + Backbuffer | Open a window; `SnakeBackbuffer` with `pitch`; `DEBUG_TRAP`/`ASSERT`; `platform_shutdown`; unified `build-dev.sh` | Solid-coloured window at ~60 fps |
| 02 | Drawing Primitives + Grid | `draw_rect` (inline in `game.c`); `GAME_RGB`/`GAME_RGBA`; pixel-index with `pitch`; border walls | Coloured rects; green wall border on black field |
| 03 | Input | `GameInput inputs[2]` double-buffer; `prepare_input_frame(old, current)`; `UPDATE_BUTTON`; move single square | Player-controlled white square on the grid |
| 04 | Game Structure | `game.h`/`game.c` split; `GameState`; `GAME_PHASE` enum; `snake_init`/`snake_update`/`snake_render` stubs | Same scene, now driven from `game.c` with phase state machine |
| 05 | Snake Data + Ring Buffer | `Segment segments[MAX_SNAKE]`, `head`/`tail`/`length`; `% MAX_SNAKE` wrap; hardcoded 10-seg snake | Static multi-segment snake drawn from ring buffer |
| 06 | Snake Movement | Delta-time tick accumulator; `SNAKE_DIR` enum; `DX`/`DY`; `move_interval`; `move_timer -= interval` | Snake glides across the grid automatically |
| 07 | Turn Input | `next_direction` staging; modular turn `(dir+1)%4`; 180° U-turn guard | Player can steer the moving snake |
| 08 | Food Spawning | `snake_spawn_food`; retry loop; `srand(time(NULL))`; eat → `grow_pending` → score | Food appears; snake grows on eat |
| 09 | Collision Detection | Wall + self collision; `GAME_PHASE_GAME_OVER`; `draw_text` + **8×8 ASCII-indexed** font; `snprintf` | Game ends on crash; static GAME OVER text shown |
| 10 | Restart + Speed Scaling | Phase restart; `best_score` save/restore around `memset`; `move_interval` decay; `HEADER_ROWS` panel | Full header; game speeds up; clean restart |
| 11 | Audio | `utils/audio.h`, `audio.c`; `SoundInstance`; food-eaten (panned by `food_x`) + game-over SFX; volume ramp | Audible spatial feedback on eat and death; no clicks |
| 12 | Polish + Utils Refactor | `draw_rect_blend` alpha overlay; refactor draw utils into `utils/`; final complete game | Complete, playable game with all visuals and audio |

---

## Final File Structure

```
ai-llm-knowledge-dump/generated-courses/javidx9/snake/
├── PLAN.md
├── README.md
├── PLAN-TRACKER.md
├── COURSE-BUILDER-IMPROVEMENTS.md        ← Phase 4
└── course/
    ├── build-dev.sh                       ← unified --backend x11|raylib flag
    └── src/
        ├── utils/
        │   ├── math.h                     ← CLAMP, MIN, MAX, ABS macros
        │   ├── backbuffer.h               ← SnakeBackbuffer (with pitch) + GAME_RGB/RGBA
        │   ├── draw-shapes.h              ← draw_rect / draw_rect_blend declarations
        │   ├── draw-shapes.c              ← clipped fill + alpha blend impl
        │   ├── draw-text.h                ← draw_char / draw_text declarations
        │   ├── draw-text.c                ← 8×8 ASCII-indexed font + draw_char/draw_text
        │   └── audio.h                    ← SoundDef, SoundInstance, AudioOutputBuffer, GameAudioState
        ├── game.h                         ← GameState (with GameAudioState), GameInput (union), GAME_PHASE, SNAKE_DIR, Segment, DEBUG_TRAP/ASSERT
        ├── game.c                         ← all snake logic; draw_cell; calls utils; GAME_PHASE dispatch
        ├── audio.c                        ← game_play_sound_at, game_play_sound, game_get_audio_samples
        ├── platform.h                     ← 4-function contract (init/get_time/get_input/shutdown)
        ├── main_x11.c                     ← X11 + GLX + ALSA backend + main()
        └── main_raylib.c                  ← Raylib backend + main()
    └── lessons/
        ├── 01-window-and-backbuffer.md
        ├── 02-drawing-primitives-and-grid.md
        ├── 03-input.md
        ├── 04-game-structure.md
        ├── 05-snake-data-and-ring-buffer.md
        ├── 06-snake-movement.md
        ├── 07-turn-input.md
        ├── 08-food-spawning.md
        ├── 09-collision-detection.md
        ├── 10-restart-and-speed-scaling.md
        ├── 11-audio.md
        └── 12-polish-and-utils-refactor.md
```

---

## JS → C Concept Mapping (per lesson)

| Lesson | New C concept | Nearest JS equivalent |
|--------|--------------|----------------------|
| 01 | `uint32_t` fixed-width integer | `number` (JS has no bit-width guarantee) |
| 01 | `malloc` / `free` for pixel buffer | `new ArrayBuffer(n)` — but manual reclaim here |
| 01 | `= {0}` zero-initialisation | `= {}` / spreading defaults |
| 01 | `pitch = width * 4` stride field | No equivalent — JS hides stride in typed arrays |
| 01 | `#ifdef DEBUG` / `__builtin_trap()` | No equivalent — closest is `debugger` statement |
| 02 | `#define GAME_RGB(r,g,b)` macro | Inline function / constant — but no type |
| 02 | Pixel-index `py * (pitch/4) + px` | `ctx.getImageData` flat-array index math |
| 02 | Function pointer-free clipping | `Math.max`/`Math.min` bounds check |
| 03 | `typedef struct GameButtonState` | Plain JS object `{endedDown, halfTransitionCount}` |
| 03 | `#define UPDATE_BUTTON(btn, is_down)` macro | Inline helper function |
| 03 | `GameInput inputs[2]` double-buffer + index swap | Redux double-dispatch / flip-buffer pattern |
| 03 | `XkbSetDetectableAutoRepeat` (X11) | `keydown` event `repeat` property check |
| 04 | Header file (`game.h`) + implementation split | ES module `export` declarations vs `.js` file |
| 04 | `void snake_init(GameState *s)` — pointer param | Mutating an object passed by reference |
| 04 | `typedef enum GAME_PHASE` | `const STATE = Object.freeze({PLAYING:0, GAME_OVER:1})` |
| 05 | Fixed-size ring buffer array | Circular queue backed by a fixed JS `Array` |
| 05 | `% MAX_SNAKE` wrap-around | `% array.length` |
| 06 | `delta_time` accumulator pattern | `requestAnimationFrame` timestamp difference |
| 06 | `typedef enum SNAKE_DIR` | `Object.freeze({UP:0, RIGHT:1, DOWN:2, LEFT:3})` |
| 07 | `next_direction` staging field | "Pending action" in a state machine |
| 08 | `srand(time(NULL))` seeding | `Math.random()` (already seeded by the JS runtime) |
| 08 | `rand() % N` modular random | `Math.floor(Math.random() * N)` |
| 09 | `snprintf(buf, sizeof(buf), …)` safe format | Template literal / `String(val)` |
| 09 | 8×8 font: `FONT_8X8[128][8]` indexed by ASCII | No direct equivalent; canvas pixel ops closest |
| 09 | `(0x80 >> col)` BIT7-left bit-mask | No direct equivalent; bitwise right-shift `>> n` |
| 10 | `memset(s, 0, sizeof(*s))` full zero-reset | `Object.assign(state, initialState)` |
| 10 | `best_score` save before `memset`, restore after | Selective preserve on reset: `const bs = state.best; reset(state); state.best = bs` |
| 11 | Phase accumulator `phase += freq / sample_rate` | Web Audio `AudioContext` oscillator — done manually here |
| 11 | Volume ramping to prevent clicks | `AudioParam.linearRampToValueAtTime` |
| 11 | `game_play_sound_at(audio, id, pan)` spatial pan | `PannerNode.positionX` in Web Audio |
| 11 | `SoundInstance` slot array (no malloc in audio fill) | Pre-allocated `AudioBufferSourceNode` pool |
| 12 | `draw_rect_blend` alpha compositing formula | `ctx.globalAlpha` — done per-pixel manually here |
| 12 | `utils/` header+implementation split | Splitting utilities into separate `utils.js` modules |

---

## Reference Source Analysis: What Follows course-builder.md ✅

| Feature | Reference state | course-builder.md section |
|---------|----------------|--------------------------|
| CPU backbuffer pipeline (`SnakeBackbuffer`) | ✅ Follows | Backbuffer architecture |
| `GAME_RGB`/`GAME_RGBA` macros (`0xAARRGGBB`) | ✅ Follows | Color system |
| 3-layer split (game / platform.h / main_*.c) | ✅ Follows | Source file rules |
| Delta-time loop, cap at 0.1f | ✅ Follows | Delta-time game loop |
| VSync detection + manual `sleep_ms` fallback (X11) | ✅ Follows | VSync with manual fallback |
| `GameButtonState` (`half_transition_count`, `ended_down`) | ✅ Follows | Input system — Core types |
| `UPDATE_BUTTON` macro | ✅ Follows | Input system — Button update macro |
| `XkbSetDetectableAutoRepeat` (X11) | ✅ Follows | Input system — Event-based |
| `typedef enum SNAKE_DIR` | ✅ Follows | Typed enums |
| `= {0}` zero-init, `memset` for reset | ✅ Follows | State management |
| `snprintf` (never `sprintf`) | ✅ Follows | Bitmap font rendering |
| `draw_rect` + `draw_rect_blend` with clipping | ✅ Follows | Drawing primitives |
| Render layer order (clear → static → dynamic → overlay) | ✅ Follows | Rendering pattern |
| `g_x11` prefix for platform state | ✅ Follows | Naming conventions |
| Section headers `/* ═══ ... ═══ */` | ✅ Follows | Comment style |

## Reference Source Analysis: What Does NOT Follow course-builder.md ❌

| # | Gap | Reference behaviour | course-builder.md requirement | Course fix |
|---|-----|--------------------|-----------------------------|-----------|
| 1 | **Single `GameInput` buffer** | `prepare_input_frame(GameInput *input)` — 1 arg, no `old_input` | `GameInput inputs[2]`; `prepare_input_frame(old, current)` copies `ended_down` | Upgrade to 2-buffer pattern in Lesson 03 |
| 2 | **No `GameInput` union** | Separate named fields only | `union { buttons[BUTTON_COUNT]; struct { ... }; }` for iteration | Add union + `BUTTON_COUNT 2` in `game.h` |
| 3 | **`int game_over` boolean** | Plain `int game_over` | `GAME_PHASE` enum + `switch` dispatch for games with multiple states | Add `GAME_PHASE` in Lesson 04 |
| 4 | **Naming: `game_over`** | `game_over` field | `is_` prefix for booleans (`is_game_over`) | Use `GAME_PHASE` (avoids the bool entirely) |
| 5 | **`platform.h` only 3 functions** | No `platform_shutdown` in contract | `platform_init`, `platform_get_time`, `platform_get_input`, `platform_shutdown` | Add `platform_shutdown` to contract |
| 6 | **Two build scripts** | `build_x11.sh`, `build_raylib.sh` | Unified `build-dev.sh --backend=x11\|raylib -r -d` | Unified `build-dev.sh` in Lesson 01 |
| 7 | **No `utils/` split** | All draw code inline in `snake.c` | `utils/draw-shapes.h/c`, `draw-text.h/c`, `math.h`, `backbuffer.h` | Start inline; refactor in Lesson 12 |
| 8 | **No audio system** | No sound at all | `audio.c` + `utils/audio.h`, `SoundDef`, `SoundInstance`, `GameAudioState` | Add in Lesson 11 |
| 9 | **5×7 custom font** | Separate `FONT_DIGITS`/`FONT_LETTERS` tables + `find_special_char` linear scan | `FONT_8X8[128][8]` indexed by ASCII value (`(0x80 >> col)` mask) | Upgrade to 8×8 ASCII-indexed font in `draw-text.c` |
| 10 | **No `pitch` in backbuffer** | `SnakeBackbuffer { pixels, width, height }` — no stride field | Tetris `Backbuffer` has `pitch` (bytes per row) | Add `pitch` for consistency; document why |
| 11 | **No `DEBUG_TRAP`/`ASSERT` macro** | No debug assertions | `#ifdef DEBUG` `ASSERT(expr)` + `__builtin_trap()` | Add to `game.h` |
| 12 | **No spatial audio** | (no audio) | `game_play_sound_at(audio, id, pan)` for position-based pan | Pan food-eaten by `food_x` in Lesson 11 |

## Course Upgrades Over Reference Source

| Upgrade | What changes | Rationale |
|---------|-------------|-----------|
| `GameInput inputs[2]` double-buffer | `prepare_input_frame(old, current)` copies `ended_down` | Prevents X11 key-state loss between frames; matches Tetris |
| `GAME_PHASE` enum | Replaces `int game_over` flag | Scales to future states (pause, menu); avoids boolean sprawl |
| 8×8 ASCII-indexed bitmap font | `FONT_8X8[128][8]`; no lookup; `(0x80 >> col)` | Simpler code: index by char directly; larger glyph set; teaches BIT7 convention |
| `pitch` in `SnakeBackbuffer` | `pitch = width * 4` (explicit) | Consistency with Tetris; teaches stride concept |
| `DEBUG_TRAP` / `ASSERT` macro | In `game.h` under `#ifdef DEBUG` | Students need a debugger break pattern from Lesson 1 |
| `platform_shutdown` in contract | Added to `platform.h` | Proper resource cleanup; good habit |
| `GameInput` union with `BUTTON_COUNT` | `union { buttons[2]; struct { turn_left, turn_right }; }` | Enables `prepare_input_frame` loop; consistent with Tetris |
| Spatial audio pan | Food-eaten: `pan = (food_x / GRID_WIDTH) * 2.0f - 1.0f` | Teaches `game_play_sound_at`; audible spatial feedback |

## Known Deviations from course-builder.md (intentional)

| # | Deviation | Where documented |
|---|-----------|-----------------|
| 1 | Course uses unified `build-dev.sh`; reference has two scripts | Lesson 01, COURSE-BUILDER-IMPROVEMENTS.md |
| 2 | Utilities start inline in `game.c` (lessons 01–10), then refactored into `utils/` in Lesson 12 | Lesson 02 (trade-off), Lesson 12 (refactor), COURSE-BUILDER-IMPROVEMENTS.md |
| 3 | `audio.c` + `utils/audio.h` added — reference source has no audio | Lesson 11, COURSE-BUILDER-IMPROVEMENTS.md |
| 4 | `GameInput inputs[2]` double-buffer swap; reference uses single buffer | Lesson 03, COURSE-BUILDER-IMPROVEMENTS.md |
| 5 | 8×8 ASCII-indexed font replaces reference's 5×7 custom tables | Lesson 09, COURSE-BUILDER-IMPROVEMENTS.md |
| 6 | `GAME_PHASE` enum replaces `int game_over` flag | Lesson 04, COURSE-BUILDER-IMPROVEMENTS.md |
| 7 | `pitch` field added to `SnakeBackbuffer` | Lesson 01, COURSE-BUILDER-IMPROVEMENTS.md |
| 8 | `DEBUG_TRAP`/`ASSERT` macro added | Lesson 01, COURSE-BUILDER-IMPROVEMENTS.md |
| 9 | `platform_shutdown` added to `platform.h` contract | Lesson 01, COURSE-BUILDER-IMPROVEMENTS.md |
| 10 | Spatial audio pan for food-eaten by `food_x` | Lesson 11, COURSE-BUILDER-IMPROVEMENTS.md |

---

## Key Constants (quick reference)

```c
GRID_WIDTH    = 60      /* cells */
GRID_HEIGHT   = 20      /* cells */
CELL_SIZE     = 20      /* pixels per cell */
HEADER_ROWS   = 3       /* score panel rows above play field */
MAX_SNAKE     = 1200    /* ring buffer capacity = GRID_WIDTH * GRID_HEIGHT */
WINDOW_WIDTH  = 1200    /* px = GRID_WIDTH * CELL_SIZE */
WINDOW_HEIGHT =  460    /* px = (GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE */

move_interval initial = 0.15f   /* seconds per step */
move_interval floor   = 0.05f
move_interval step    = -0.01f  every 3 score points

grow_pending per food = 5       /* tail defers N ticks before advancing */
```

---

## Architecture Diagram

```
┌───────────────────────────────────────────────────────────────┐
│  game.c / game.h  (platform-independent)                      │
│  snake_init → snake_update → snake_render                     │
│  Writes to SnakeBackbuffer (CPU RAM) via utils/ draw helpers  │
└────────────┬──────────────────────────────────────────────────┘
             │
┌────────────┴──────────────────────────────────────────────────┐
│  audio.c  (platform-independent)                              │
│  game_play_sound  /  game_fill_audio_buffer                   │
│  Phase accumulator; no malloc; no X11/Raylib headers          │
└────────────┬──────────────────────────────────────────────────┘
             │ SnakeBackbuffer.pixels + AudioOutputBuffer
    ┌────────┴────────┐
    │                 │
main_x11.c       main_raylib.c
X11 + GLX        Raylib
ALSA audio       Raylib audio callback
```
