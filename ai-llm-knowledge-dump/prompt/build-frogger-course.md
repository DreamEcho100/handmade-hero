# Build the Frogger Course

## Source files to analyse

Read **all** of these before writing anything:

- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/frogger/course/build_x11.sh`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/frogger/course/build_raylib.sh`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/frogger/course/src/frogger.h`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/frogger/course/src/frogger.c`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/frogger/course/src/platform.h`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/frogger/course/src/main_x11.c`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/frogger/course/src/main_raylib.c`
- `@ai-llm-knowledge-dump/prompt/course-builder.md` — your instruction manual
- `@ai-llm-knowledge-dump/llm.txt` — student background profile

---

## Output directory

`ai-llm-knowledge-dump/generated-courses/javidx9/frogger/`

All output goes here. Nothing outside this directory.

---

## Example of the Output structure based on the course-builder patterns and the code referenced

```
ai-llm-knowledge-dump/generated-courses/javidx9/frogger/
├── PLAN.md               # Analyse & plan before writing any lesson
├── README.md             # Human-readable course overview
├── PLAN-TRACKER.md       # Progress log; update after every file created
├── COURSE-BUILDER-IMPROVEMENTS.md  # Written at the very end
└── course/
    ├── build-dev.sh
    ├── assets/               # .spr binary sprite files (copied from reference)
    │   ├── frog.spr
    │   ├── water.spr
    │   ├── pavement.spr
    │   ├── wall.spr
    │   ├── home.spr
    │   ├── log.spr
    │   ├── car1.spr
    │   ├── car2.spr
    │   └── bus.spr
    ├── src/
    │   ├── utils/
    │   │   ├── backbuffer.h      # Backbuffer type, GAME_RGBA macro, pitch
    │   │   ├── draw-shapes.c     # draw_rect, draw_rect_blend, draw_sprite_partial
    │   │   ├── draw-shapes.h
    │   │   ├── draw-text.c       # draw_char, draw_text using FONT_8X8
    │   │   ├── draw-text.h
    │   │   ├── math.h            # MIN, MAX, CLAMP macros
    │   │   └── audio.h           # SoundInstance, AudioChannel, audio helpers
    │   ├── game.h                # All game types, constants, GameState, public API
    │   ├── game.c                # frogger_init, frogger_tick, frogger_render
    │   ├── audio.c               # fill_audio_buffer, sound mixing, SFX helpers
    │   ├── platform.h            # 4-function platform contract
    │   ├── main_x11.c            # X11/OpenGL backend
    │   └── main_raylib.c         # Raylib backend
    └── lessons/
        ├── 01-window-and-backbuffer.md
        ├── 02-drawing-primitives.md
        ├── 03-input.md
        └── ...
```

> **Note on build scripts:** The reference Frogger source uses two separate build scripts (`build_x11.sh` and `build_raylib.sh`) rather than the unified `build-dev.sh` seen in Tetris. The generated course **replaces both with a unified `build-dev.sh --backend=x11|raylib -r -d`**, matching the architecture students will see in Tetris. Explain the trade-off in Lesson 01 and in `COURSE-BUILDER-IMPROVEMENTS.md`.

> **Course enhancements over the reference source:** The reference Frogger source puts all drawing utilities (including the bitmap font) directly in `frogger.c` and has no audio. The generated course **refactors** those utilities into `utils/` (teaching the same architecture students encounter in Tetris) and **adds a simple audio system** (`audio.c` + `utils/audio.h`) with sound effects for frog hop, splash/death, lane-change, and home-reached events. Additionally, the course upgrades the font from the reference's column-major 5×7 `font_glyphs[96][5]` to the course-builder standard **8×8 ASCII-indexed `FONT_8X8[128][8]`** with BIT7-left convention (`0x80 >> col`). All upgrades are documented in `COURSE-BUILDER-IMPROVEMENTS.md` and explained with `> **Course Note:**` callouts in the relevant lessons.

> **Pixel format note:** The reference `FROGGER_RGBA` macro produces `0xAABBGGRR` (bytes in memory as `[RR, GG, BB, AA]` on little-endian). The Tetris reference uses `GAME_RGBA` = `0xAARRGGBB`. **The course uses the Tetris convention** (`0xAARRGGBB`) for cross-game consistency, and documents the discrepancy with a `> **Course Note:**` callout in Lesson 02 alongside the cross-backend validation test (pure-red `draw_rect` on both X11 and Raylib before writing more rendering code).

> **Renaming convention:** The reference uses `frogger.h` / `frogger.c` / `FroggerBackbuffer` / `frogger_init` / `frogger_tick` / `frogger_render`. The course standardises on `game.h` / `game.c` / `Backbuffer` / `game_init` / `game_update` / `game_render` — matching the Tetris/Snake/Asteroids convention so students can navigate all courses without constantly context-switching naming.

---

## Reference source vs course-builder.md: Gap analysis

Run this analysis before generating any source files. All deviations below MUST be documented
in the relevant lesson and in `COURSE-BUILDER-IMPROVEMENTS.md`.

### What the reference source already follows ✅

- CPU backbuffer pipeline (`FroggerBackbuffer`, `uint32_t *pixels`, GPU upload per frame)
- 3-layer split: game logic / `platform.h` contract / `main_*.c` backends
- Delta-time loop with `delta_time > 0.1f` cap
- VSync via `GLX_EXT_swap_control` + `fprintf` fallback (X11 backend)
- `SetTargetFPS(0)` + VSync via `EndDrawing` (Raylib backend)
- `GameButtonState` (`half_transition_count`, `ended_down`); `UPDATE_BUTTON` macro
- `XkbSetDetectableAutoRepeat` (X11 auto-repeat suppress)
- `WM_DELETE_WINDOW` atom handling with `XInternAtom` (X11 backend)
- Separate `frogger_tick` / `frogger_render` — game code calls zero OS/library APIs
- DOD lane data split: `lane_speeds[NUM_LANES]` (floats) + `lane_patterns[NUM_LANES][LANE_PATTERN_LEN]` (char grids)
- `lane_scroll()` with correct floor-mod for negative speeds (fixes the sub-tile jump bug)
- `SpriteBank` fixed-pool sprite storage — no `malloc` after init
- `clang -std=c99` — C, not C++
- `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` in Raylib texture upload (correct format for byte layout)

### What the reference source does NOT follow (gaps to fix in the course) ❌

| #   | Gap                             | Reference                                                         | Course fix                                                                                      | Lesson |
| --- | ------------------------------- | ----------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- | ------ |
| 1   | Two build scripts               | `build_x11.sh` + `build_raylib.sh`                                | Unified `build-dev.sh --backend=x11\|raylib -r -d`                                              | 01     |
| 2   | No `pitch` in backbuffer        | `{pixels, width, height}`                                         | Add `pitch = width * 4`; use `py * (pitch/4) + px` throughout                                   | 01     |
| 3   | No `DEBUG_TRAP`/`ASSERT`        | (no debug macros)                                                 | Add to `game.h` under `#ifdef DEBUG`; `__builtin_trap()` on clang/gcc; `-DDEBUG` in debug build | 01     |
| 4   | `platform.h` only 3 functions   | No `platform_shutdown`                                            | Add `platform_shutdown` to contract; backends destroy GL context / Raylib window                | 01     |
| 5   | Non-standard pixel format       | `FROGGER_RGBA` = `0xAABBGGRR`                                     | Use `GAME_RGBA` = `0xAARRGGBB` (Tetris convention); document swap in Lesson 02                  | 02     |
| 6   | No named color constants        | No `COLOR_*` constants; uses `CONSOLE_PALETTE[16][3]` byte arrays | Define `COLOR_BLACK`, `COLOR_WHITE`, `COLOR_GREEN`, … in `game.h`                               | 02     |
| 7   | Single `GameInput` buffer       | `prepare_input_frame(GameInput *input)` — 1 arg                   | `inputs[2]` + `prepare_input_frame(old, current)` that copies `ended_down`                      | 03     |
| 8   | No `GameInput` union            | Separate named fields only                                        | `union { buttons[BUTTON_COUNT]; struct { move_up, move_down, move_left, move_right }; }`        | 03     |
| 9   | No `PlatformGameProps` wrapper  | Raw `platform_init(title, w, h)` args; `is_running` static        | Wrap in `PlatformGameProps` with `title`, `width`, `height`, `backbuffer`, `is_running`         | 01     |
| 10  | No letterbox centering          | Fixed non-resizable window; no resize handling                    | `ConfigureNotify`+`glViewport` (X11), `SetWindowState`+`DrawTexturePro` (Raylib) from Lesson 01 | 01     |
| 11  | Unguarded `fprintf` debug calls | Bare `fprintf(stderr, ...)` in `platform_init` and sprite loader  | Guard with `#ifdef DEBUG` or document as student-visible trace; remove from release builds      | 01     |
| 12  | No `utils/` split               | All draw code + font inline in `frogger.c`                        | Start inline (lessons 01–10), refactor to `utils/` in Lesson 12                                 | 12     |
| 13  | 5×7 column-major font           | `font_glyphs[96][5]` BIT0-left                                    | Upgrade to **8×8 ASCII-indexed** `FONT_8X8[128][8]`; `(0x80>>col)` BIT7 mask                    | 10     |
| 14  | No audio                        | (silence)                                                         | `audio.c` + `utils/audio.h`: hop, splash/death, home-reached, lane-change SFX with spatial pan  | 11     |
| 15  | Non-standard naming conventions | `FroggerBackbuffer`, `frogger_init`, `frogger_tick`               | Rename to `Backbuffer`, `game_init`, `game_update`, `game_render`; document in Lesson 04        | 04     |

### Font upgrade rationale

The reference uses a column-major 5×7 font with `font_glyphs[96][5]` indexed from ASCII 0x20 (space). course-builder.md recommends a single `FONT_8X8[128][8]` array indexed directly by ASCII value with the BIT7-left convention (`0x80 >> col`).

**Why upgrade:** Simpler indexing (direct ASCII, no `ch - 32` offset), larger glyph set (full 128 chars vs 96), consistent with the Tetris/Snake/Asteroids pattern across all courses. **Trade-off to document:** 8×8 font uses more memory (~1 KB vs ~480 B) and produces slightly blockier glyphs at small scale. Document in Lesson 10 and `COURSE-BUILDER-IMPROVEMENTS.md`.

### Unique Frogger techniques (not in Snake/Tetris/Asteroids)

These techniques appear in this game for the first time. Give each one a dedicated lesson section
with a JS analogy and a `> **Why?**` callout:

| Technique                                                            | Where introduced | JS analogy                                                            |
| -------------------------------------------------------------------- | ---------------- | --------------------------------------------------------------------- |
| `lane_scroll()` — pixel-accurate floor-mod for bi-directional scroll | Lesson 06        | `ctx.translate()` resets each frame; C accumulates and wraps          |
| DOD lane data split (speeds separate from pattern grids)             | Lesson 05        | Two parallel arrays like `{ids: [], names: []}` in JS                 |
| `draw_sprite_partial` — UV-clipped sprite rendering                  | Lesson 08        | `ctx.drawImage(img, sx, sy, sw, sh, dx, dy, dw, dh)` — explicit crop  |
| `SpriteBank` fixed pool with `.spr` binary loader                    | Lesson 08        | `fetch(url).arrayBuffer()` — but raw bytes from disk, no JSON         |
| `danger[SCREEN_CELLS_W * SCREEN_CELLS_H]` — flat 2D bitmask          | Lesson 07        | A 2D `Uint8Array` used as a collision grid                            |
| `CONSOLE_PALETTE[16][3]` — Windows console 4-bit color → RGB         | Lesson 08        | An enum-indexed CSS color palette — same idea, explicit byte values   |
| Bi-directional lane scrolling via negative speed values              | Lesson 06        | `setInterval` running at different rates left or right — but per-lane |

---

## Workflow — follow this order strictly

### Phase 0 — Analyse (no output yet)

1. Read every source file listed above.
2. Read the full `course-builder.md`.
3. Read `llm.txt`.
4. Identify the natural build progression: smallest runnable step → logical milestone sequence.

### Phase 1 — Planning files (create these first, nothing else)

Create `PLAN.md`, `README.md`, and `PLAN-TRACKER.md`. Do **not** create any lesson or source file until these exist.

**`PLAN.md`** must contain:

- One-paragraph description of what the final game does
- Lesson sequence table: `| # | Title | What gets built | What the student runs/sees |`
- Final file structure tree
- JS→C concept mapping: which new C concepts each lesson introduces, and the nearest JS equivalent
- Frogger-specific architecture notes: DOD lane data, danger buffer, sprite bank

**`README.md`** must contain:

- Course title and one-line description
- Prerequisites (recommend completing Snake first; Asteroids recommended for sprite/physics familiarity)
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
- For Lessons 06, 07, 08 — give each unique technique (`lane_scroll`, danger buffer, sprite bank) a full `> **New technique:**` block with:
  - What it does
  - Why we do it this way (vs the JS/browser approach)
  - The JS equivalent
  - One concrete worked example with numbers

### Phase 4 — Retrospective

Create `COURSE-BUILDER-IMPROVEMENTS.md` with:

- What patterns from `course-builder.md` worked well and why
- What was missing, unclear, or needed adjustment
- Concrete suggested edits with before/after diffs (quoted)
- Any C/game concepts that came up in the Frogger course but aren't covered in `course-builder.md`

---

## Quality bars

### Code quality (applies to every file in `course/src/`)

| Rule                                       | Detail                                                                                                             |
| ------------------------------------------ | ------------------------------------------------------------------------------------------------------------------ |
| No `malloc` in the game loop               | `backbuffer.pixels` allocated once at startup; `SpriteBank` pool is a fixed array in `GameState`                   |
| No OOP patterns                            | No vtables, no function-pointer polymorphism for its own sake                                                      |
| Flat arrays, not linked lists              | Lane objects are implicit in pattern strings; frog position is a pair of floats — no `malloc` node per entity      |
| `memset` / `= {0}` for init                | Never read uninitialized memory — `GameInput input = {0}`, `GameState state = {0}`                                 |
| DOD lane data                              | `lane_speeds[NUM_LANES]` and `lane_patterns[NUM_LANES][LANE_PATTERN_LEN]` stay in separate arrays — no lane struct |
| `GAME_RGBA` = `0xAARRGGBB`                 | Tetris byte order; validated against both backends in Lesson 02                                                    |
| `pitch` in backbuffer                      | `pitch = width * 4`; pixel index = `py * (pitch/4) + px` everywhere                                                |
| `lane_scroll()` floor-mod                  | Always work in pixels; use positive modulo `if (sc < 0) sc += PATTERN_PX`; never cast float to int before modulo   |
| `GAME_PHASE` state machine                 | `typedef enum GAME_PHASE`: `PHASE_PLAYING`, `PHASE_DEAD`, `PHASE_WIN`; `switch` dispatch in `game_update`          |
| Input double-buffered                      | `GameInput inputs[2]` + index swap; `prepare_input_frame(old, current)` copies `ended_down`                        |
| `GameInput` union                          | `union { buttons[BUTTON_COUNT]; struct { move_up, move_down, move_left, move_right }; }` — `BUTTON_COUNT 4`        |
| 8×8 ASCII-indexed bitmap font              | `FONT_8X8[128][8]`; index by `(int)c`; BIT7-left mask `(0x80 >> col)` — test with 'N' first                        |
| Platform independence                      | `game.c` / `game.h` / `audio.c` must compile without X11 or Raylib headers                                         |
| `platform.h` 4-function contract           | `platform_init`, `platform_get_time`, `platform_get_input`, `platform_shutdown`                                    |
| `DEBUG_TRAP`/`ASSERT` macro                | In `game.h` under `#ifdef DEBUG`; `__builtin_trap()` on clang/gcc; `-DDEBUG` in debug build                        |
| Named color constants                      | `COLOR_BLACK`, `COLOR_WHITE`, `COLOR_GREEN`, … in `game.h`; no bare `GAME_RGB(0,0,0)` literals in render code      |
| Sprite rendering via `draw_sprite_partial` | Clip sprite source rect to lane pixel offset; never render whole sprite then clip at screen edge                   |
| Spatial audio pan                          | Hop sound: `pan = (frog_x / (SCREEN_PX_W - 1.0f)) * 2.0f - 1.0f`; splash panned the same way                       |
| Letterbox centering from day 1             | Both backends resize gracefully; `ConfigureNotify`+`glViewport` (X11), `DrawTexturePro` (Raylib)                   |

### Lesson quality

| Rule                                  | Detail                                                                                               |
| ------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| Each lesson compilable                | Student can build after every lesson, not just the last                                              |
| One major concept per lesson          | Don't introduce DOD data, lane_scroll, AND danger buffer in the same lesson                          |
| Runnable milestone each lesson        | Student sees something new on screen every lesson                                                    |
| JS analogies present                  | Every first use of a C concept has a JS counterpart                                                  |
| No forward references                 | Lesson N doesn't assume knowledge from Lesson N+2                                                    |
| Unique techniques get worked examples | `lane_scroll` floor-mod, danger buffer writes, `draw_sprite_partial` UV crop — all get numeric demos |

---

## Lesson progression (required milestones)

Follow this exact ordering. You may split a milestone into 2 lessons if it's getting too long, but do not reorder or skip:

1. **Window + backbuffer** — Open a resizable window, fill it with black. Introduce `Backbuffer` with `pitch`, unified `build-dev.sh`, `PlatformGameProps`, letterbox (`ConfigureNotify`+`glViewport` / `DrawTexturePro`), `platform_shutdown`, `DEBUG_TRAP`/`ASSERT`. Show the C equivalent of `document.createElement('canvas')`.

2. **Drawing primitives** — `draw_rect`, `draw_rect_blend` with alpha blend. Introduce `GAME_RGBA` = `0xAARRGGBB`; cross-backend pixel-format validation (pure-red rect on both X11 and Raylib). Explain how `0xAARRGGBB` differs from the reference's `FROGGER_RGBA` = `0xAABBGGRR` and why the course standardises on Tetris convention. Introduce named `COLOR_*` constants and the `CONSOLE_PALETTE` explain.

3. **Input** — Move a coloured rectangle around the screen with arrow keys. Introduce `GameButtonState`, `UPDATE_BUTTON`, `prepare_input_frame` with `inputs[2]` double-buffer, `GameInput` union. Explain X11 `XkbSetDetectableAutoRepeat`. Note that `RepeatInterval`/DAS-ARR is intentionally absent (frog moves on "just pressed" grid hops, not held-key repeat — explain the difference from Tetris).

4. **Game structure + GAME_PHASE** — Introduce `GameState`, `game_init`, `game_update`, `game_render` split. Show the `GAME_PHASE` enum (`PHASE_PLAYING`, `PHASE_DEAD`, `PHASE_WIN`). Explain the naming change from `frogger_init/tick/render` to `game_init/update/render` and why the course standardises. No game logic yet — frog stands at start position, phases shown as debug text.

5. **Lane data (DOD)** — Add full `lane_speeds[NUM_LANES]` and `lane_patterns[NUM_LANES][LANE_PATTERN_LEN]` arrays. Render all lanes as coloured rectangles (no scrolling yet). Introduce DOD layout: two parallel arrays vs an array of structs. JS analogy: `{ids: [], names: []}` vs `[{id, name}]`. Explain why separating "physics data" from "pattern data" lets the CPU pipeline each independently.

6. **Scrolling lanes (`lane_scroll`)** — Implement `lane_scroll()` with pixel-accurate floor-mod. Render pattern tiles scrolling at correct per-lane speeds. Deep-dive the bug the reference fixed: C truncates `(int)` toward zero, not toward negative infinity — diagonal shear if you use `width` instead of floor-mod. Show the fix step by step with real numbers. Tiles scroll smoothly left and right per lane.

7. **Danger buffer + frog collision** — Add `danger[SCREEN_CELLS_W * SCREEN_CELLS_H]` flat 2D array. Each `frogger_tick`, zero the buffer then write `1` to every unsafe cell using the same `lane_scroll` offsets used for rendering — guaranteeing the collision grid always matches the screen. Introduce the frog position and check against danger on each hop. Frog dies on unsafe cells, respawns at start.

8. **Sprites (`SpriteBank` + `draw_sprite_partial`)** — Load `.spr` binary files during `game_init` into a flat `SpriteBank` pool. Implement `draw_sprite_partial` for UV-cropped sprite rendering into the backbuffer. Replace the coloured rectangle lane tiles with actual sprites using the `CONSOLE_PALETTE` to map sprite color indices to RGBA. Render the frog sprite. Explain `.spr` binary format (width, height, then packed color bytes).

9. **Homes + win condition** — Add the five home cells at the top of the screen. Track `homes_reached` in `GameState`. Hopping into a home cell: mark it filled, increment counter, respawn frog. When all five are filled: `PHASE_WIN`. Add a simple win overlay with `draw_rect_blend`.

10. **Font + UI** — Score display, timer, and lives counter. Introduce upgraded `FONT_8X8[128][8]` with BIT7-left mask. Show the `> **Course Note:**` callout explaining the upgrade from the reference's 5×7 column-major font. Add `SCORE: 000`, time, and `LIVES: 3` with `snprintf` for integer-to-string. Color the score panel.

11. **Audio** — Sound effects: frog hop (on any successful move), splash/death (on unsafe landing), home-reached (on fifth home), game-over (lives exhausted). Introduce `AudioOutputBuffer`, `SoundInstance`, `fill_audio_buffer`, click prevention, stereo pan `(frog_x / SCREEN_PX_W) * 2.0f - 1.0f`. Full explanation of Casey's latency model for the platform side (ALSA `start_threshold = 1` + pre-fill silence, Raylib 2048-frame buffer).

12. **Polish + `utils/` refactor** — Refactor drawing code to `utils/draw-shapes.c`, `utils/draw-text.c`, math helpers to `utils/math.h`, audio helpers to `utils/audio.h`. Explain the utils/ architecture (same as Tetris). Add frog respawn animation (flicker for 0.5 s after death), game-over overlay with semi-transparent backdrop, restart on any key after game over. Final state: complete, playable game.

---

## game.c architecture notes

The **reference** frogger source puts all utility code (drawing primitives, bitmap font, sprite drawing) directly in `frogger.c`. The **generated course** starts that way for the first few lessons (fewer files = less cognitive overhead), then **refactors** those utilities into `utils/` as part of the lesson progression, teaching students exactly when and why the split pays off. Explain the starting trade-off in Lesson 02, the refactoring motivation in Lesson 12, and document both in `COURSE-BUILDER-IMPROVEMENTS.md`.

Key internal structures to highlight:

| Symbol                                       | Kind            | What to emphasise                                                                                       |
| -------------------------------------------- | --------------- | ------------------------------------------------------------------------------------------------------- |
| `lane_speeds[NUM_LANES]`                     | float array     | DOD: speeds and patterns are separate; `frogger_tick` reads speeds, `frogger_render` reads both         |
| `lane_patterns[NUM_LANES][LANE_PATTERN_LEN]` | char 2D array   | Each row is a 64-char pattern string — `' '` = safe, `'L'` = log, `'C'` = car, etc.                     |
| `lane_scroll()`                              | inline function | Floor-mod in pixels — positive modulo handles negative speeds; explain why `(int)` cast truncates wrong |
| `danger[SCREEN_CELLS_W * SCREEN_CELLS_H]`    | uint8 flat 2D   | Written during tick, read during tick — always consistent with what `render` will draw                  |
| `SpriteBank` + `.spr` loader                 | fixed pool      | All sprite data in one flat array; `sprite_load()` reads raw bytes from disk during init                |
| `draw_sprite_partial`                        | function        | UV crop: source rect is clipped to the pattern pixel offset; destination is screen pixels               |
| `CONSOLE_PALETTE[16][3]`                     | static array    | 4-bit Windows console color index → `{R, G, B}` byte triple; converted to `GAME_RGBA` on draw           |
| `tile_is_safe` / `tile_to_sprite`            | functions       | Pattern char → boolean / sprite ID; keep beside pattern data, not scattered through render              |
| `GameButtonState` + `prepare_input_frame`    | struct + fn     | Same pattern as Tetris/Handmade Hero — cross-reference explicitly; "just pressed" for hops              |
| `GAME_PHASE`                                 | enum            | Replaces `int dead` flag; `switch` dispatch in `game_update` and `game_render`                          |

---

## Deviation policy

The `course-builder.md` is a **strong guideline**, not a rigid spec. Deviate when:

- The actual Frogger code does something better-structured than the pattern in `course-builder.md`
- A pattern would confuse a JS developer more than help them
- A simplification hides something important about how C/games actually work

**When you deviate:**

1. Add `/* COURSE NOTE: ... */` in the source file
2. Add a `> **Course Note:**` callout in the relevant lesson
3. Add an entry to `COURSE-BUILDER-IMPROVEMENTS.md`

Do **not** deviate just to simplify. Complexity that teaches something important should stay.

Known deviations and enhancements to document from the start:

- **Two build scripts → unified `build-dev.sh`** — reference has `build_x11.sh` + `build_raylib.sh`; course replaces both. Explain in Lesson 01.
- **`FROGGER_RGBA` → `GAME_RGBA`** — reference packs `0xAABBGGRR`; course uses `0xAARRGGBB`. Document swap in Lesson 02 with a pure-red validation test on both backends.
- **Naming standardised** — `frogger_init/tick/render` → `game_init/update/render`; `FroggerBackbuffer` → `Backbuffer`. Document in Lesson 04.
- **`PlatformGameProps` added** — reference uses raw `platform_init(title, w, h)` args; course wraps in `PlatformGameProps`. Document in Lesson 01.
- **Letterbox from lesson 1** — reference uses a fixed non-resizable window; course makes it resizable from lesson 1. Document in Lesson 01.
- **Utilities start in `game.c`, then refactor to `utils/`** — Lessons 01–11 keep everything inline. Lesson 12 teaches the split. Document in Lesson 02 (trade-off) and Lesson 12 (refactor).
- **Audio added** — reference has no sound; course adds `audio.c` + `utils/audio.h` with hop, death, home-reached SFX (spatially panned by `frog_x`). Document in Lesson 11.
- **`inputs[2]` double-buffer** — reference uses single `GameInput`; course uses the course-builder.md two-buffer pattern preserving `ended_down`. Document in Lesson 03.
- **8×8 ASCII-indexed font** — reference uses 5×7 column-major `font_glyphs[96][5]`; course uses `FONT_8X8[128][8]` with `(0x80>>col)` BIT7 mask. Document in Lesson 10.
- **`GAME_PHASE` enum** — reference uses `int dead` flag; course uses `typedef enum GAME_PHASE`. Document in Lesson 04.
- **`pitch` in `Backbuffer`** — reference omits pitch; course adds it. Document in Lesson 01.
- **`DEBUG_TRAP`/`ASSERT` macro** — not in reference; course adds it. Document in Lesson 01.
- **`platform_shutdown` in contract** — reference has only 3 functions; course adds the 4th. Document in Lesson 01.
- **No `RepeatInterval`** — frog moves one cell per key press ("just pressed" only). Document explicitly in Lesson 03 as the contrast to Tetris DAS-ARR.

---

## Common bugs and fixes

| Symptom                                         | Cause                                                          | Fix                                                                                                   |
| ----------------------------------------------- | -------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| Red/blue channels swapped                       | Using `FROGGER_RGBA` = `0xAABBGGRR` instead of `GAME_RGBA`     | Standardise on `GAME_RGBA`; test with pure-red rect on both backends before Lesson 03                 |
| Diagnoal shear in rendering                     | Using `width` instead of `pitch/4` as row stride               | Add `pitch` field; use `py * (bb->pitch/4) + px`                                                      |
| Sprites jump 8 pixels at a time (not smooth)    | Using cell offsets (`CELL_PX` steps) for sub-tile position     | Work entirely in pixels; `px_offset = sc % TILE_PX`; `tile_start = sc / TILE_PX`                      |
| Negative-speed lanes jump an extra tile         | `(int)(time * speed)` truncates toward zero, not −∞            | Use `lane_scroll()` floor-mod: compute `sc` in pixels, `if (sc < 0) sc += PATTERN_PX`                 |
| Collision doesn't match screen                  | Tick and render compute scroll with different time values      | Call `lane_scroll()` once per lane per frame; pass the same output to both paths                      |
| Frog walks through hazards                      | `danger[]` buffer not written on the same frame it's read      | Zero `danger[]` at top of `game_update`, write unsafe cells, then check frog position in same call    |
| Letterbox missing on X11 resize                 | `g_win_w`/`g_win_h` not updated on `ConfigureNotify`           | Handle `ConfigureNotify` in event loop; update globals; recompute viewport in `display_backbuffer`    |
| Sprite pixels wrong color                       | `CONSOLE_PALETTE` mapped but `FROGGER_RGBA` byte order used    | Convert palette entries with `GAME_RGBA(r, g, b, 0xFF)`; verify with a known-color test sprite        |
| Sprite renders with hard screen-edge cut        | `draw_sprite_partial` not clipping source UV correctly         | Clip source rect to `[0, TILE_PX - px_offset)`; subtract offset from source X, not destination X      |
| `prepare_input_frame` loses held-key state      | Single-buffer: `ended_down` zeroed before platform reads it    | Use `inputs[2]`; `prepare_input_frame(old, current)` copies `ended_down` from old before clearing     |
| Frog moves multiple cells per key press (X11)   | `XkbSetDetectableAutoRepeat` not called; fake repeats fire     | Call `XkbSetDetectableAutoRepeat(display, True, &supported)` in `platform_init`                       |
| Window close doesn't exit (X11)                 | Missing `WM_DELETE_WINDOW` atom registration                   | `XInternAtom("WM_DELETE_WINDOW")` + `ClientMessage` handler in event loop                             |
| ALSA audio silent at startup                    | `start_threshold` left at default — waits for buffer fill      | Set `snd_pcm_sw_params_set_start_threshold(pcm, sw, 1)` after `snd_pcm_sw_params_current`             |
| Raylib audio crackling                          | `sample_count` in callback < `buffer_size_frames` registered   | Generate exactly `buffer_size_frames = 2048` samples every callback; use `SAMPLES_PER_FRAME` constant |
| `memset` wipes audio config on restart          | `memset(&state, 0, sizeof(state))` clears `samples_per_second` | Save audio config before `memset`; restore after                                                      |
| Hop sound fires continuously while held         | Triggering sound every frame `ended_down` is true              | Use `half_transition_count > 0 && ended_down` ("just pressed") check for all frog movement sounds     |
| Black square instead of transparent sprite cell | Alpha channel not honoured; `CONSOLE_PALETTE` space=0          | Character `0x0020` (space) = transparent cell — skip it in `draw_sprite_partial`; don't overdraw      |
| Score resets on respawn but lives don't         | `memset`-ing entire `GameState` on death instead of respawn    | Only reset frog position and `dead_timer`; preserve `score`, `lives`, `homes_reached`                 |

---

## Quick reference

| Pattern                    | Correct form                                               | Wrong form                            | Notes                                  |
| -------------------------- | ---------------------------------------------------------- | ------------------------------------- | -------------------------------------- |
| Pixel index                | `py*(bb->pitch/4)+px`                                      | `py*bb->width+px`                     | pitch handles row padding              |
| Color macro                | `GAME_RGBA(r,g,b,a)` = `0xAARRGGBB`                        | `FROGGER_RGBA` = `0xAABBGGRR`         | Match Tetris convention                |
| Lane scroll                | work in pixels; `if (sc < 0) sc += PATTERN_PX`             | `(int)(time * speed) % PATTERN_LEN`   | Floor-mod, not truncate-mod            |
| Sprite transparent cell    | skip draw when palette index == space char                 | overdraw with black                   | Preserve backbuffer pixels behind frog |
| draw_sprite_partial UV     | clip source to `px_offset`…`TILE_PX`; offset source X      | clip destination rect at screen edge  | Source crop, not destination crop      |
| Danger buffer              | write in same tick call that reads it                      | write during render; read during tick | Tick-render consistency                |
| "Just pressed"             | `ended_down && half_transition_count > 0`                  | `ended_down` alone                    | Prevents held-key multi-hop            |
| ALSA threshold             | `snd_pcm_sw_params_current` then `set_start_threshold = 1` | default                               | Prevents silent startup                |
| Raylib audio buffer        | `buffer_size_frames = 2048`; always generate exact count   | 735-frame default                     | Overflow = frame skips                 |
| Audio config after restart | save `samples_per_second` before `memset`; restore after   | `memset` entire state                 | Preserves audio init                   |
| X11 keysym                 | `XkbLookupKeySym(dpy, code, 0, NULL, &key)`                | `XLookupKeysym(&ev.xkey, 0)`          | Modifier-safe                          |
| Window close               | `XInternAtom` + `ClientMessage` check                      | (missing)                             | Required for × button                  |

---

## Testing checklist

### Input testing

- [ ] Tap arrow key once → frog hops exactly one cell
- [ ] Hold arrow key → frog does NOT repeat-hop (no DAS-ARR)
- [ ] Works identically on X11 and Raylib
- [ ] X11: no phantom key-repeat events (`XkbSetDetectableAutoRepeat` active)
- [ ] Quit key exits cleanly on both backends
- [ ] Window × button exits cleanly (X11 `WM_DELETE_WINDOW`)
- [ ] Restart key resets score to 0 but preserves `best_score`

### Rendering testing

- [ ] Colors appear correct (not R/B swapped) — pure-red rect passes on both backends
- [ ] Lanes scroll smoothly at per-lane pixel speeds — no 8-pixel jumps
- [ ] Negative-speed lanes scroll right; positive-speed lanes scroll left — no extra-tile jump
- [ ] Frog sprite alpha is transparent on background cells (no black halo)
- [ ] Game-over overlay blends correctly (semi-transparent, not solid black)
- [ ] Window resize letterbox-scales correctly on both backends
- [ ] Bitmap font glyphs are not mirror-imaged (test with 'N' or 'F')
- [ ] Score panel does not overlap the lane area

### Game logic testing

- [ ] Frog spawns at correct start position
- [ ] Frog dies on unsafe (non-safe) cell tile
- [ ] Frog dies and `lives` decrements; game over when `lives == 0`
- [ ] Hopping onto water without a log = death
- [ ] Hopping onto a log = frog rides log (moves with it)
- [ ] Reaching a home cell: `homes_reached` increments; home visually fills
- [ ] Reaching all 5 homes: `PHASE_WIN` triggers
- [ ] Danger buffer matches what is visually on screen (no ghost collisions)
- [ ] Delta time works — scrolling speed is rate-independent of frame rate

### Audio testing

- [ ] Hop sound triggers once per hop (not held every frame)
- [ ] Splash/death sound triggers once per death
- [ ] Home-reached sound triggers once per home filled
- [ ] No click or pop on sound start or end (volume ramping active)
- [ ] Hop sound pans left/right based on `frog_x`
- [ ] Audio works on both X11 and Raylib backends
- [ ] `audio.c` does not include any X11 or Raylib headers
- [ ] Silence when no sound is playing (no DC offset hum)

---

## Engineering principles to embed throughout

These come from Casey Muratori's Handmade Hero series. Reference them by name in callouts:

- **One frame = one call chain.** Nothing fires outside `game_update()` + `game_render()`.
- **State is always visible.** All game state lives in `GameState`. No hidden `static` locals except constant data tables (`lane_speeds`, `lane_patterns`, `CONSOLE_PALETTE`).
- **Fixed arrays, not dynamic allocation.** `SpriteBank` is a flat buffer in `GameState`; lane objects are implicit in pattern strings — no per-entity `malloc`.
- **Data-Oriented Design for lane data.** `lane_speeds[N]` + `lane_patterns[N][L]` are separate arrays because the CPU pipeline can work on each field independently — never wrap them in a `Lane` struct.
- **Collision must match rendering.** The `danger[]` buffer and the sprite-drawing loop share the `lane_scroll()` result for the same `time` — if they diverge, the player sees ghosts.
- **Enums prevent category errors.** `GAME_PHASE`, `SOUND_ID` are not magic ints; `PHASE_PLAYING` not `phase == 0`.
- **The platform layer is a thin translator.** It maps OS events → `GameInput`, OS timer → `delta_time`, OS audio → `AudioOutputBuffer`. It calls game functions; game functions never call platform functions.
- **Input double-buffering.** `inputs[2]` + `prepare_input_frame(old, current)` copies `ended_down` from the old buffer, then resets `half_transition_count`. Sub-frame taps are never lost.
- **Audio phase is continuous.** `inst->phase` is never reset between buffer fills or on slot reuse. Resetting it causes a click.
- **Comments explain _why_.** Not what — the code already shows what.
- **`lane_scroll` is the ground truth.** Both tick (collision) and render (drawing) call it for the same lane and the same `state->time`. Never compute scroll position twice.
