# Build the Snake Course

## Source files to analyse

Read **all** of these before writing anything:

- `@ai-llm-knowledge-dump/generated-courses/javidx9/_old/snake/course/build_x11.sh`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_old/snake/course/build_raylib.sh`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_old/snake/course/src/snake.h`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_old/snake/course/src/snake.c`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_old/snake/course/src/platform.h`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_old/snake/course/src/main_x11.c`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_old/snake/course/src/main_raylib.c`
- `@ai-llm-knowledge-dump/prompt/course-builder.md` — your instruction manual
- `@ai-llm-knowledge-dump/llm.txt` — student background profile

---

## Output directory

`ai-llm-knowledge-dump/generated-courses/javidx9/snake/`

All output goes here. Nothing outside this directory.

---

## Example of the Output structure based on the course-builder patterns and the code referenced

```
ai-llm-knowledge-dump/generated-courses/javidx9/snake/
├── PLAN.md               # Analyse & plan before writing any lesson
├── README.md             # Human-readable course overview
├── PLAN-TRACKER.md       # Progress log; update after every file created
├── COURSE-BUILDER-IMPROVEMENTS.md  # Written at the very end
└── course/
    ├── build-dev.sh
    ├── src/
    │   ├── utils/
    │   │   ├── backbuffer.h
    │   │   ├── draw-shapes.c
    │   │   ├── draw-shapes.h
    │   │   ├── draw-text.c
    │   │   ├── draw-text.h
    │   │   ├── math.h
    │   │   └── audio.h
    │   ├── game.h
    │   ├── game.c
    │   ├── audio.c
    │   ├── platform.h
    │   ├── main_x11.c
    │   └── main_raylib.c
    └── lessons/
        ├── 01-window-and-backbuffer.md
        ├── 02-drawing-primitives-and-grid.md
        └── ...
```

> **Note on build scripts:** Snake uses two separate build scripts (`build_x11.sh` and `build_raylib.sh`) rather than the unified `build-dev.sh` seen in Tetris _(which we will use)_. This is simpler for a first game. Explain the trade-off in the relevant lesson and in `COURSE-BUILDER-IMPROVEMENTS.md`.

> **Course enhancements over the reference source:** The reference snake source puts all drawing utilities directly in `game.c` and has no audio. The generated course **refactors** those utilities into `utils/` (teaching the same architecture students will encounter in Tetris) and **adds a simple audio system** (`audio.c` + `utils/audio.h`) with sound effects for food-eaten and game-over events. Both upgrades are documented in `COURSE-BUILDER-IMPROVEMENTS.md` and explained with `> **Course Note:**` callouts in the relevant lessons.

## Reference source vs course-builder.md: Gap analysis

Run this analysis before generating any source files. All deviations below MUST be documented
in the relevant lesson and in `COURSE-BUILDER-IMPROVEMENTS.md`.

### What the reference source already follows ✅

- CPU backbuffer pipeline (`SnakeBackbuffer`, `uint32_t *pixels`, GPU upload per frame)
- `GAME_RGB`/`GAME_RGBA` macros (`0xAARRGGBB` byte order)
- 3-layer split: game logic / `platform.h` contract / `main_*.c` backends
- Delta-time loop with `delta_time > 0.1f` cap
- VSync detection (`GLX_EXT_swap_control`, `GLX_MESA_swap_control`) + `sleep_ms` fallback
- `GameButtonState` (`half_transition_count`, `ended_down`); `UPDATE_BUTTON` macro
- `XkbSetDetectableAutoRepeat` (X11 auto-repeat fix)
- `typedef enum SNAKE_DIR` (typed, prevents magic integers)
- `= {0}` init; `memset` for full reset; `snprintf` (never `sprintf`)
- `draw_rect` + `draw_rect_blend` with bounds clipping and alpha blend
- Correct render layer order (clear → walls → food → snake body → head → overlays)
- `g_x11` prefix for platform state; `/* ═══ ... ═══ */` section headers

### What the reference source does NOT follow (gaps to fix in the course) ❌

| # | Gap | Reference | Course fix | Lesson |
|---|-----|-----------|-----------|--------|
| 1 | Single `GameInput` buffer | `prepare_input_frame(GameInput *input)` — 1 arg | `inputs[2]` + `prepare_input_frame(old, current)` that copies `ended_down` | 03 |
| 2 | No `GameInput` union | Separate named fields | `union { buttons[BUTTON_COUNT]; struct { turn_left, turn_right }; }` | 03 |
| 3 | `int game_over` boolean | Plain flag | `GAME_PHASE` enum + `switch` dispatch | 04 |
| 4 | `game_over` naming | Not `is_` prefixed | Use `GAME_PHASE` (avoids the boolean entirely) | 04 |
| 5 | `platform.h` only 3 functions | No `platform_shutdown` | Add `platform_shutdown` to contract | 01 |
| 6 | Two build scripts | `build_x11.sh` + `build_raylib.sh` | Unified `build-dev.sh --backend=x11\|raylib -r -d` | 01 |
| 7 | No `utils/` split | All draw code inline in `snake.c` | Start inline (lessons 01–10), refactor in Lesson 12 | 12 |
| 8 | No audio | (silence) | `audio.c` + `utils/audio.h` with food-eaten + game-over SFX | 11 |
| 9 | 5×7 custom font | Separate `FONT_DIGITS`/`FONT_LETTERS` + `find_special_char` scan | Upgrade to **8×8 ASCII-indexed** `FONT_8X8[128][8]`; `(0x80>>col)` BIT7 mask | 09 |
| 10 | No `pitch` in backbuffer | `{pixels, width, height}` | Add `pitch = width * 4`; use `py * (pitch/4) + px` | 01 |
| 11 | No `DEBUG_TRAP`/`ASSERT` | (no debug macros) | Add to `game.h` under `#ifdef DEBUG` | 01 |
| 12 | No spatial audio | (no audio) | Pan food-eaten by `food_x`: `(food_x/(GRID_WIDTH-1))*2-1` | 11 |

### Font upgrade rationale

The reference uses three separate tables (`FONT_DIGITS`, `FONT_LETTERS`, `FONT_SPECIAL`) with a
`find_special_char` linear scan. course-builder.md recommends a single `FONT_8X8[128][8]` array
indexed by ASCII value with the BIT7-left convention (`0x80 >> col`).

**Why upgrade:** Simpler code (index by `(int)c` directly), larger glyph set, matches course-builder.md
teaching convention. **Trade-off to document:** The 8×8 font uses more memory (~1 KB vs ~450 B)
and produces slightly blockier glyphs at small scale. Document in Lesson 09 and
`COURSE-BUILDER-IMPROVEMENTS.md`.

---

### Phase 0 — Analyse (no output yet)

1. Read every source file listed above.
2. Read the full `course-builder.md`.
3. Read `llm.txt`.
4. Identify the natural build progression for this game.

### Phase 1 — Planning files (create these first, nothing else)

Create `PLAN.md`, `README.md`, and `PLAN-TRACKER.md`. Do **not** create any lesson or source file until these exist.

**`PLAN.md`** must contain:

- One-paragraph description of what the final game does
- Lesson sequence table: `| # | Title | What gets built | What the student runs/sees |`
- Final file structure tree
- JS→C concept mapping: which new C concepts each lesson introduces, and the nearest JS equivalent

**`README.md`** must contain:

- Course title and one-line description
- Prerequisites
- How to build and run (both backends)
- Lesson list with one-line summary each
- What the student will have achieved by the end

**`PLAN-TRACKER.md`** must contain a table:

| Task      | Status  | Notes |
| --------- | ------- | ----- |
| PLAN.md   | ✅ Done |       |
| README.md | ✅ Done |       |
| ...       |         |       |

Update this file every time you complete a task.

**Stop here and wait for confirmation before continuing.**
_(If running autonomously, stop after Phase 1 and report what you created.)_

### Phase 2 — Source files

Create `course/build-dev.sh` and all `course/src/**` files.

Every source file must:

- Exactly reproduce the game's final behaviour
- Have more comments than the reference — explain **why** each decision was made
- Anchor C concepts to their nearest JavaScript equivalent in a `/* JS: ... | C: ... */` comment the first time they appear
- Have section headers using `/* ══════ Section Name ══════ */` style

Follow the `course-builder.md` patterns exactly unless you deliberately improve on them. If you improve something:

- Leave a comment in the source: `/* COURSE NOTE: We deviate from course-builder.md here because ... */`
- Add a `> **Course Note:**` callout in the lesson that covers that file

### Phase 3 — Lesson files

One `.md` file per lesson in `course/lessons/`. Filename: `NN-kebab-title.md`.

Each lesson follows the template from `course-builder.md`:

```
# Lesson NN: Title

## What we're building
## What you'll learn
## Prerequisites
---
## Step 1 ...
## Step 2 ...
---
## Build and run
## Expected result
## Exercises
## What's next
```

Additional requirements:

- Every code block must be the **complete file at that lesson stage**, not just diffs — the student should be able to copy-paste and compile
- Every non-obvious C pattern gets a `> **Why?**` callout explaining it in JS terms
- Every new concept gets one **"Common mistake"** note showing the wrong version and why it fails
- Use `> **Handmade Hero principle:**` callouts when a decision follows Casey's architecture
- Reference specific line ranges in the final source: `(see \`src/game.c:45–67\`)`

### Phase 4 — Retrospective

Create `COURSE-BUILDER-IMPROVEMENTS.md` with:

- What patterns from `course-builder.md` worked well and why
- What was missing, unclear, or needed adjustment
- Concrete suggested edits with before/after diffs (quoted)
- Any C/game concepts that came up in the Snake course but aren't covered in `course-builder.md`

---

## Quality bars

### Code quality (applies to every file in `course/src/`)

| Rule                          | Detail                                                                                            |
| ----------------------------- | ------------------------------------------------------------------------------------------------- |
| No `malloc` in the game loop  | `backbuffer.pixels` is allocated once at startup; the ring buffer lives in `GameState` statically |
| No OOP patterns               | No vtables, no function-pointer polymorphism for its own sake                                     |
| Flat arrays, not linked lists | `Segment segments[MAX_SNAKE]` not `Segment *head`; no `next` pointer, no `malloc` per node        |
| `memset` / `= {0}` for init   | Never read uninitialized memory — `GameInput input = {0}`, `GameState state = {0}`                |
| Fixed-size ring buffer        | `Segment segments[MAX_SNAKE]` with `head`/`tail` indices — not a linked list or `realloc`'d array |
| Enums over magic integers     | `SNAKE_DIR_UP` not `direction == 0`; modular arithmetic explained explicitly                      |
| `GAME_PHASE` state machine    | Replace `int game_over` with `typedef enum GAME_PHASE` + `switch` dispatch in `snake_update`     |
| Input double-buffered         | `GameInput inputs[2]` + index swap; `prepare_input_frame(old, current)` copies `ended_down`      |
| `GameInput` union             | `union { buttons[BUTTON_COUNT]; struct { turn_left, turn_right }; }` — `BUTTON_COUNT 2`           |
| 8×8 ASCII-indexed bitmap font | `FONT_8X8[128][8]`; index by `(int)c`; BIT7-left mask `(0x80 >> col)` — test with 'N' first      |
| `pitch` in backbuffer         | `SnakeBackbuffer.pitch = width * 4`; pixel index = `py * (pitch/4) + px`                         |
| Platform independence         | `game.c` / `game.h` / `audio.c` must compile without X11 or Raylib headers                       |
| `platform.h` 4-function contract | `platform_init`, `platform_get_time`, `platform_get_input`, `platform_shutdown`                |
| `DEBUG_TRAP`/`ASSERT` macro   | In `game.h` under `#ifdef DEBUG`; `__builtin_trap()` on clang/gcc; `-DDEBUG` in debug build       |
| Spatial audio pan             | Food-eaten sound: `pan = (food_x / (GRID_WIDTH - 1.0f)) * 2.0f - 1.0f`; call `game_play_sound_at` |

### Lesson quality

| Rule                           | Detail                                                         |
| ------------------------------ | -------------------------------------------------------------- |
| Each lesson compilable         | Student can build after every lesson, not just the last        |
| One major concept per lesson   | Don't introduce ring buffers, enums, and structs in one lesson |
| Runnable milestone each lesson | Student sees something new on screen every lesson              |
| JS analogies present           | Every first use of a C concept has a JS counterpart            |
| No forward references          | Lesson N doesn't assume knowledge from Lesson N+2              |

---

## Lesson progression (required milestones)

Follow this exact ordering. You may split a milestone into 2 lessons if it's getting too long, but do not reorder or skip:

1. **Window + backbuffer** — Open a window, fill it with a solid colour. Introduce the CPU backbuffer pipeline: `SnakeBackbuffer`, `uint32_t` pixels, the platform upload loop.
2. **Drawing primitives + grid** — Draw coloured rectangles. Introduce `draw_rect()`, `GAME_RGB`/`GAME_RGBA` colour packing, pixel-coordinate arithmetic, and the grid-to-pixel mapping (`col * CELL_SIZE`, `row * CELL_SIZE`). Draw an empty grid.
3. **Input** — Move a single coloured square around the grid with arrow keys. Introduce `GameButtonState`, `UPDATE_BUTTON`, `prepare_input_frame`, input double-buffering. Explain the X11 `XkbSetDetectableAutoRepeat` trick in a callout.
4. **Game structure** — Introduce `GameState`, `snake_init()`, `snake_update()`, `snake_render()` split across `game.h` / `game.c`. No snake logic yet — draw a single segment at a hardcoded position.
5. **Snake data + ring buffer** — Represent the snake as a fixed-size ring buffer: `Segment segments[MAX_SNAKE]`, `head`, `tail`, `length`. Render a multi-segment snake from hardcoded data. Deep-dive on ring buffer arithmetic and why it beats `memmove`.
6. **Snake movement** — Grid-based tick movement: `move_timer` accumulates `delta_time`; when it exceeds `move_interval`, advance `head` by one cell, advance `tail`. Introduce `SNAKE_DIR` enum and the modular-arithmetic turn formula: `(dir + 1) % 4`.
7. **Turn input** — Wire keyboard left/right to `next_direction`. Explain the `direction` vs `next_direction` split (prevents 180° reversal). Add the U-turn guard.
8. **Food spawning** — Place food at a random grid cell not occupied by the snake. Introduce `snake_spawn_food()`, linear scan + retry, `srand(time(NULL))` seeding. Snake eats food: grows by 1, score increments, food respawns.
9. **Collision detection** — Wall collision (boundary check) and self-collision (scan ring buffer for head == any body segment). Set `game_over`. Show a static "GAME OVER" text. Introduce `draw_text()` and the embedded 5×7 bitmap font.
10. **Restart + speed scaling** — `restart` button resets state while preserving `best_score`. `move_interval` decreases each time food is eaten (up to a minimum cap). Introduce the `HEADER_ROWS` score panel at the top.
11. **Audio** — Add sound effects for food-eaten and game-over using `AudioOutputBuffer` and `SoundInstance`. Introduce `game_play_sound()`, click prevention via volume ramping, and Casey's latency model for the platform audio callback. No music sequencer — just event-triggered one-shots. Introduce `audio.c` and `utils/audio.h`.
12. **Polish** — `draw_rect_blend()` for the food pulse / overlay tint. `best_score` displayed persistently. Full game-over overlay with semi-transparent backdrop. Final state: complete, playable game.

---

## game.c architecture notes

The **reference** snake source puts all utility code directly in `game.c` — the bitmap font tables, `draw_rect`, `draw_char`, `draw_text`, `draw_cell`. The **generated course** starts that way for the first few lessons (fewer files = less cognitive overhead), then **refactors** those utilities into `utils/` as part of the lesson progression, teaching students exactly when and why the split pays off. Explain the starting trade-off in Lesson 2, explain the refactoring in the lesson where the split happens, and document both in `COURSE-BUILDER-IMPROVEMENTS.md`.

Key internal structures to highlight:

| Symbol                          | Kind          | What to emphasise                                                 |
| ------------------------------- | ------------- | ----------------------------------------------------------------- |
| `SNAKE_DIR`                     | enum          | Modular arithmetic for turns; why an enum beats `int dir`         |
| `Segment`                       | struct        | Minimal — just `{int x, y}`. Not a node; no `next` pointer        |
| `segments[MAX_SNAKE]`           | ring buffer   | `head`/`tail` indices; wrap with `% MAX_SNAKE`; why not `memmove` |
| `move_timer` / `move_interval`  | floats        | Accumulator pattern; scaling `move_interval` for difficulty       |
| `direction` vs `next_direction` | fields        | Staged commit: buffer the turn, apply it on the next tick         |
| `FONT_DIGITS` / `FONT_LETTERS`  | static arrays | 5×7 bitmaps; bit-masking to render pixels                         |
| `UPDATE_BUTTON`                 | macro         | Same pattern as Tetris/Handmade Hero — cross-reference explicitly |
| `prepare_input_frame`           | function      | Must be called before `platform_get_input` every frame            |

---

## Deviation policy

The `course-builder.md` is a **strong guideline**, not a rigid spec. Deviate when:

- The actual Snake code does something better-structured than the pattern in `course-builder.md`
- A pattern would confuse a JS developer more than help them
- A simplification hides something important about how C/games actually work

**When you deviate:**

1. Add `/* COURSE NOTE: ... */` in the source file
2. Add a `> **Course Note:**` callout in the relevant lesson
3. Add an entry to `COURSE-BUILDER-IMPROVEMENTS.md`

Do **not** deviate just to simplify. Complexity that teaches something important should stay.

Known deviations and enhancements to document from the start:

- **Two build scripts → unified `build-dev.sh`** — reference has `build_x11.sh` + `build_raylib.sh`; course uses `build-dev.sh --backend=x11|raylib -r -d`. Explain the upgrade in Lesson 01.
- **Utilities start in `game.c`, then refactor to `utils/`** — Lessons 01–10 keep everything inline for simplicity. Lesson 12 teaches the split, showing exactly when and why it pays off. Document in Lesson 02 (trade-off) and Lesson 12 (refactor).
- **Audio added** — reference has no sound; course adds `audio.c` + `utils/audio.h` with food-eaten (spatially panned by `food_x`) and game-over SFX. No music sequencer (that's Tetris). Document in Lesson 11.
- **`GameInput inputs[2]` double-buffer** — reference uses single `GameInput`; course uses the course-builder.md two-buffer pattern. Document in Lesson 03.
- **8×8 ASCII-indexed font** — reference uses 5×7 custom tables; course uses `FONT_8X8[128][8]` indexed by ASCII with `(0x80>>col)` BIT7 mask. Simpler, larger glyph set. Document in Lesson 09.
- **`GAME_PHASE` enum** — reference uses `int game_over`; course uses `typedef enum GAME_PHASE`. Document in Lesson 04.
- **`pitch` in `SnakeBackbuffer`** — reference omits pitch; course adds it for consistency with Tetris and to teach stride. Document in Lesson 01.
- **`DEBUG_TRAP`/`ASSERT` macro** — not in reference; course adds it. Document in Lesson 01.
- **`platform_shutdown` in contract** — reference has only 3 functions; course adds the 4th. Document in Lesson 01.

---

## Checklist before submitting course

- [ ] `PLAN.md` exists and matches final lesson structure
- [ ] Each lesson builds and runs independently
- [ ] All code compiles with `-Wall -Wextra` without warnings on both backends
- [ ] Both X11 and Raylib backends work
- [ ] Comments explain WHY, not just WHAT
- [ ] Exercises are included and tested
- [ ] `build-dev.sh` has `--backend` and `-r` flags
- [ ] Input system uses double-buffering with proper content swap (not pointer swap)
- [ ] `prepare_input_frame` preserves `ended_down` from previous frame
- [ ] `PLAN-TRACKER.md` is up to date at the time of submission
- [ ] Naming follows conventions (`PascalCase` structs, `UPPER_SNAKE` enums/macros, `snake_case` functions)
- [ ] No magic integers — all directions use `SNAKE_DIR_*` enum values
- [ ] Ring buffer indices always wrapped with `% MAX_SNAKE` — no bounds-check `if`
- [ ] `game.c` has zero `#include <X11/...>` or `#include <raylib.h>` lines
- [ ] `audio.c` has zero `#include <X11/...>` or `#include <raylib.h>` lines
- [ ] Audio `inst->phase` is never reset to 0 between frames or on slot reuse
- [ ] Volume ramping is applied to prevent clicks on sound start/end

---

## Common bugs and fixes

| Symptom                                     | Cause                                                   | Fix                                                                          |
| ------------------------------------------- | ------------------------------------------------------- | ---------------------------------------------------------------------------- |
| Held key acts like repeated presses         | `ended_down` not preserved in `prepare_input_frame`     | Copy from `old_input` before resetting transitions                           |
| Key only works on first press (X11)         | Event-based input loses state between frames            | Ensure `prepare_input_frame(old, current)` copies `ended_down`               |
| Snake teleports instead of moving smoothly  | Grid position updated every frame instead of every tick | Accumulate `move_timer`; only advance when `>= move_interval`                |
| 180° reversal possible                      | `next_direction` applied without U-turn guard           | Check `(dir + 2) % 4 != next_dir` before committing the turn                 |
| Snake grows even when not eating            | `grow_pending` not reset after tail advance             | Only skip tail advance once; clear flag after use                            |
| Self-collision triggers on head's own cell  | Ring buffer scan starts at `head` instead of `head+1`   | Start body scan one slot past the current head                               |
| Food spawns on snake body                   | `snake_spawn_food()` doesn't check body occupancy       | Linear scan all segments; retry until a free cell is found                   |
| Score resets on restart but best score does | `memset(&state, 0, ...)` clears `best_score`            | Save `best_score` before `memset`, restore after                             |
| Colors look wrong (R/B swapped)             | RGBA vs ARGB byte order mismatch                        | Check platform expects `0xAARRGGBB`; verify with a pure-red test rect        |
| Game runs too fast without VSync            | No frame limiting                                       | Add manual sleep when VSync unavailable                                      |
| Window too fast uncapped — `dt` too small   | Uncapped loop, `delta_time` near zero                   | Add `nanosleep` fallback; cap `delta_time` floor at `1/60`                   |
| Bitmap font mirror-imaged                   | Bitmask direction wrong — wrong font width convention   | Course uses 8×8 font: `(0x80 >> col)` BIT7-left; verify with asymmetric 'N' |
| Bitmap font: wrong char displayed           | Font indexed by wrong value (not ASCII)                 | `FONT_8X8[128]` indexed by `(int)c`; no lookup tables needed                 |
| Text integers show garbage                  | `sprintf` used without bounds                           | Use `snprintf(buf, sizeof(buf), "%d", val)`                                  |
| Click/pop on every sound trigger            | `inst->phase` reset to 0 when slot is reused            | Never reset `phase`; let it accumulate continuously across fills             |
| Volume ramp never reaches full              | Ramp step too small or ramp logic inverted              | Check `inst->volume += ramp_step` runs inside the sample loop, not outside   |
| Audio silent on one backend only            | Platform audio buffer fill not wired up in one backend  | Ensure both `main_x11.c` and `main_raylib.c` call `game_fill_audio_buffer()` |
| Food-eaten sound doesn't pan spatially      | `platform_play_sound` called without pan arg            | Use `game_play_sound_at(audio, SOUND_FOOD_EATEN, pan)` with computed `pan`   |
| `GAME_PHASE` dispatch never reaches OVER    | Condition compares `int` not `GAME_PHASE` enum          | Use `state->phase == GAME_PHASE_GAME_OVER` in `switch`; never use raw `int`  |

---

## Testing checklist

### Input testing

- [ ] Tap key once → action triggers once
- [ ] Works identically on X11 and Raylib
- [ ] X11: no phantom key-repeat events (`XkbSetDetectableAutoRepeat` active)
- [ ] Press two direction keys simultaneously → last pressed wins (no lockup)
- [ ] Quick tap (< 1 frame) → direction change still registers
- [ ] U-turn blocked (pressing opposite direction has no effect)
- [ ] Restart key resets state and preserves `best_score`
- [ ] Quit key exits cleanly

### Rendering testing

- [ ] Colors appear correct (not swapped channels)
- [ ] No flickering or tearing
- [ ] Grid cells align correctly with `CELL_SIZE` — no 1px gaps or overlaps
- [ ] Header score panel does not overlap the playfield
- [ ] Game-over overlay blends correctly (semi-transparent, not solid black)
- [ ] `best_score` persists across restarts and displays correctly
- [ ] Bitmap font glyphs are not mirror-imaged (test with 'N' or 'F')

### Game logic testing

- [ ] Game initializes to correct state (snake centered, food placed, score 0)
- [ ] Snake grows by exactly 1 segment per food eaten
- [ ] Score increments by the correct amount each food eaten
- [ ] `move_interval` decreases as score increases (up to minimum cap)
- [ ] Wall collision triggers game over
- [ ] Self-collision triggers game over
- [ ] Food never spawns on the snake body
- [ ] Restart resets everything except `best_score`
- [ ] Delta time works — game speed is independent of frame rate

### Audio testing

- [ ] Food-eaten sound triggers exactly once per food eaten (not every frame)
- [ ] Game-over sound triggers exactly once per game-over (not on every restart)
- [ ] No click or pop on sound start or end (volume ramping active)
- [ ] Audio works on both X11 and Raylib backends
- [ ] `audio.c` does not include any X11 or Raylib headers
- [ ] Silence when no sound is playing (no DC offset hum)

---

## Engineering principles to embed throughout

These come from Casey Muratori's Handmade Hero series. Reference them by name in callouts:

- **One frame = one call chain.** Nothing fires outside `snake_update()` + `snake_render()`.
- **State is always visible.** All important state lives in `GameState`. No hidden `static` locals inside functions.
- **Fixed arrays, not dynamic allocation.** The ring buffer is `Segment segments[MAX_SNAKE]` — no `malloc`, no `realloc`, no linked list.
- **Enums prevent category errors.** `SNAKE_DIR_UP` is not `0`; the type system enforces correct use.
- **The platform layer is a thin translator.** It maps OS events → `GameInput`, OS timer → `delta_time`, CPU pixels → GPU texture. `game.c` never calls `XNextEvent` or `IsKeyPressed`.
- **Input double-buffering.** `inputs[2]` + `prepare_input_frame(old, current)` copies `ended_down` from the old buffer, then resets `half_transition_count` in the new buffer. Sub-frame taps are never lost.
- **`GAME_PHASE` state machine.** Replace boolean flags with `typedef enum GAME_PHASE`. `switch (state->phase)` in `snake_update` is the single authoritative dispatch point.
- **Audio phase is continuous.** `inst->phase` is never reset between buffer fills or on slot reuse. Resetting it causes a click.
- **Comments explain _why_.** Not what — the code already shows what.
