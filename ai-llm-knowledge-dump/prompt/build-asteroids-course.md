# Build the Asteroids Course

## Source files to analyse

Read **all** of these before writing anything:

- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/asteroids/course/build_x11.sh`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/asteroids/course/build_raylib.sh`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/asteroids/course/src/asteroids.h`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/asteroids/course/src/asteroids.c`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/asteroids/course/src/platform.h`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/asteroids/course/src/main_x11.c`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/asteroids/course/src/main_raylib.c`
- `@ai-llm-knowledge-dump/generated-courses/javidx9/_ignore/asteroids/OneLoneCoder_Asteroids.cpp` — original C++ reference
- `@ai-llm-knowledge-dump/prompt/course-builder.md` — your instruction manual
- `@ai-llm-knowledge-dump/llm.txt` — student background profile

---

## Output directory

`ai-llm-knowledge-dump/generated-courses/javidx9/asteroids/`

All output goes here. Nothing outside this directory.

---

## Example of the Output structure based on the course-builder patterns and the code referenced

```
ai-llm-knowledge-dump/generated-courses/javidx9/asteroids/
├── PLAN.md               # Analyse & plan before writing any lesson
├── README.md             # Human-readable course overview
├── PLAN-TRACKER.md       # Progress log; update after every file created
├── COURSE-BUILDER-IMPROVEMENTS.md  # Written at the very end
└── course/
    ├── build-dev.sh
    ├── src/
    │   ├── utils/
    │   │   ├── backbuffer.h      # AsteroidsBackbuffer type, GAME_RGBA macro, pitch
    │   │   ├── draw-shapes.c     # draw_pixel_w, draw_rect, draw_rect_blend, draw_line
    │   │   ├── draw-shapes.h
    │   │   ├── draw-text.c       # draw_char, draw_text using FONT_8X8
    │   │   ├── draw-text.h
    │   │   ├── math.h            # MIN, MAX, CLAMP, Vec2, rotation helpers
    │   │   └── audio.h           # AudioChannel, SoundInstance, audio helpers
    │   ├── game.h                # All game types, constants, GameState, public API
    │   ├── game.c                # asteroids_init, asteroids_update, asteroids_render
    │   ├── audio.c               # fill_audio_buffer, sound mixing, SFX helpers
    │   ├── platform.h            # 4-function platform contract
    │   ├── main_x11.c            # X11/OpenGL backend
    │   └── main_raylib.c         # Raylib backend
    └── lessons/
        ├── 01-window-and-backbuffer.md
        ├── 02-drawing-primitives.md
        ├── 03-line-drawing-bresenham.md
        └── ...
```

> **Note on build scripts:** The reference Asteroids source uses two separate build scripts (`build_x11.sh` and `build_raylib.sh`) rather than the unified `build-dev.sh` seen in Tetris. The generated course **replaces both with a unified `build-dev.sh --backend=x11|raylib -r -d`**, matching the architecture students will see in Tetris. Explain the trade-off in Lesson 01 and in `COURSE-BUILDER-IMPROVEMENTS.md`.

> **Course enhancements over the reference source:** The reference Asteroids source puts all drawing and math utilities directly in `asteroids.c` and has no audio. The generated course **refactors** those utilities into `utils/` (teaching the same architecture students encounter in Tetris) and **adds a simple audio system** (`audio.c` + `utils/audio.h`) with sound effects for thrusting, bullet fire, asteroid explosions, and ship death. Additionally, the course upgrades the font from the reference's column-major 5×7 `font_glyphs[96][5]` to the course-builder standard **8×8 ASCII-indexed `FONT_8X8[128][8]`** with BIT7-left convention (`0x80 >> col`). All upgrades are documented in `COURSE-BUILDER-IMPROVEMENTS.md` and explained with `> **Course Note:**` callouts in the relevant lessons.

> **Pixel format note:** The reference `ASTEROIDS_RGBA` macro produces `0xAABBGGRR` (bytes in memory as `[RR, GG, BB, AA]` on little-endian). The Tetris reference uses `GAME_RGBA` = `0xAARRGGBB`. **The course uses the Tetris convention** (`0xAARRGGBB`) for cross-game consistency, and documents the discrepancy with a `> **Course Note:**` callout in Lesson 02 alongside the cross-backend validation test (pure-red `draw_rect` on both X11 and Raylib before writing more rendering code).

---

## Reference source vs course-builder.md: Gap analysis

Run this analysis before generating any source files. All deviations below MUST be documented
in the relevant lesson and in `COURSE-BUILDER-IMPROVEMENTS.md`.

### What the reference source already follows ✅

- CPU backbuffer pipeline (`AsteroidsBackbuffer`, `uint32_t *pixels`, GPU upload per frame)
- 3-layer split: game logic / `platform.h` contract / `main_*.c` backends
- Delta-time loop with `delta_time > 0.1f` cap
- VSync detection (`GLX_EXT_swap_control`, `GLX_MESA_swap_control`) + `sleep_ms` fallback
- `GameButtonState` (`half_transition_count`, `ended_down`); `UPDATE_BUTTON` macro
- `XkbSetDetectableAutoRepeat` (X11 auto-repeat fix)
- `XkbKeycodeToKeysym` (modifier-safe keysym lookup) ✅
- `WM_DELETE_WINDOW` protocol handling (`XInternAtom`) ✅
- `typedef enum GAME_PHASE` (typed, prevents magic integers)
- `= {0}` init; `memset` for full reset; `snprintf` (never `sprintf`)
- `draw_rect` + `draw_rect_blend` with bounds clipping and alpha blend
- `g_x11` prefix for platform state; `/* ═══ ... ═══ */` section headers
- Fixed-size entity pools (`SpaceObject asteroids[MAX_ASTEROIDS]`, `bullets[MAX_BULLETS]`) — no `malloc` in game loop
- `compact_pool` swap-with-last O(1) removal
- `GAME_PHASE` state machine (`PHASE_PLAYING`, `PHASE_DEAD`)
- `setup_vsync()` present in X11 backend
- `SetTargetFPS(0)` + VSync via `EndDrawing` in Raylib backend
- `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` in Raylib (correct texture format for the byte layout)

### What the reference source does NOT follow (gaps to fix in the course) ❌

| #   | Gap                           | Reference                                              | Course fix                                                                              | Lesson |
| --- | ----------------------------- | ------------------------------------------------------ | --------------------------------------------------------------------------------------- | ------ |
| 1   | No `pitch` in backbuffer      | `{pixels, width, height}`                              | Add `pitch = width * 4`; use `py * (pitch/4) + px` throughout                           | 01     |
| 2   | Single `GameInput` buffer     | `prepare_input_frame(GameInput *)` — 1 arg             | `inputs[2]` + `prepare_input_frame(old, current)` that copies `ended_down`              | 04     |
| 3   | No `GameInput` union          | Separate named fields only                             | `union { buttons[BUTTON_COUNT]; struct { left, right, up, fire }; }` — `BUTTON_COUNT 4` | 04     |
| 4   | `platform.h` only 3 functions | No `platform_shutdown`                                 | Add `platform_shutdown` to contract; both backends free GL context / Raylib window      | 01     |
| 5   | Two build scripts             | `build_x11.sh` + `build_raylib.sh`                     | Unified `build-dev.sh --backend=x11\|raylib -r -d`                                      | 01     |
| 6   | No `utils/` split             | All draw + math code inline in `asteroids.c`           | Start inline (lessons 01–11), refactor to `utils/` in Lesson 13 (Polish)                | 13     |
| 7   | No audio                      | (silence)                                              | `audio.c` + `utils/audio.h`: thrust, fire, explode-large/medium/small, ship-death SFX   | 12     |
| 8   | 5×7 column-major font         | `font_glyphs[96][5]` BIT0-left                         | Upgrade to **8×8 ASCII-indexed** `FONT_8X8[128][8]`; `(0x80>>col)` BIT7 mask            | 11     |
| 9   | Non-standard pixel format     | `ASTEROIDS_RGBA` = `0xAABBGGRR`                        | Use `GAME_RGBA` = `0xAARRGGBB` (Tetris convention); document swap in Lesson 02          | 02     |
| 10  | No `DEBUG_TRAP`/`ASSERT`      | (no debug macros)                                      | Add to `game.h` under `#ifdef DEBUG`; `-DDEBUG` in debug build                          | 01     |
| 11  | No spatial audio              | (no audio)                                             | Pan explode sound by `asteroid.x`: `(x / SCREEN_W) * 2.0f - 1.0f`; `game_play_sound_at` | 12     |
| 12  | `srand` in `asteroids_init`   | Seeds once at init (good), but undocumented            | Add `/* JS: Math.random() seeds automatically; C rand() needs manual seeding */`        | 06     |
| 13  | No `RepeatInterval`           | Not needed for Asteroids (fire is "just pressed" only) | Document why it's NOT used here vs Tetris DAS-ARR; note in Lesson 04                    | 04     |

### Font upgrade rationale

The reference uses a column-major 5×7 font with `font_glyphs[96][5]` indexed from ASCII 0x20 (space). course-builder.md recommends a single `FONT_8X8[128][8]` array indexed directly by ASCII value with the BIT7-left convention (`0x80 >> col`).

**Why upgrade:** Simpler indexing (direct ASCII, no `ch - 32` offset), larger glyph set (full 128 chars vs 96), consistent with the Tetris utils/ pattern. **Trade-off to document:** 8×8 font uses more memory (~1 KB vs ~480 B) and produces slightly blockier glyphs at small scale. Document in Lesson 11 and `COURSE-BUILDER-IMPROVEMENTS.md`.

### Unique Asteroids techniques (not in Snake/Tetris)

These techniques appear in this game for the first time. Give each one a dedicated lesson section
with a JS analogy and a `> **Why?**` callout:

| Technique                                           | Where introduced | JS analogy                                                    |
| --------------------------------------------------- | ---------------- | ------------------------------------------------------------- |
| `draw_line` (Bresenham's algorithm)                 | Lesson 03        | `ctx.lineTo()` — but integer, exact, CPU                      |
| 2D rotation matrix (`cos/sin`)                      | Lesson 06        | `ctx.rotate()` — but applied per vertex in code               |
| Toroidal wrapping in `draw_pixel_w`                 | Lesson 07        | Modulo on canvas coord — but on write, not read               |
| `compact_pool` swap-with-last O(1)                  | Lesson 08        | `arr.splice(i,1)` — O(n); swap-with-last is O(1)              |
| Circle collision `is_point_inside_circle` (no sqrt) | Lesson 09        | `Math.hypot(dx,dy) < r` — but avoids sqrt with `dx²+dy² < r²` |
| Floating-point physics (velocity + acceleration)    | Lesson 07        | `vx += ax * dt` — same concept, different precision model     |
| `srand(time(NULL))` seeding                         | Lesson 06        | `Math.random()` auto-seeded                                   |

---

## Workflow — follow this order strictly

### Phase 0 — Analyse (no output yet)

1. Read every source file listed above.
2. Read the full `course-builder.md`.
3. Read `llm.txt`.
4. Read `OneLoneCoder_Asteroids.cpp` to understand the original C++ structure and what the C port changed.
5. Identify the natural build progression: smallest runnable first step → logical milestone sequence.

### Phase 1 — Planning files (create these first, nothing else)

Create `PLAN.md`, `README.md`, and `PLAN-TRACKER.md`. Do **not** create any lesson or source file until these exist.

**`PLAN.md`** must contain:

- One-paragraph description of what the final game does
- Lesson sequence table: `| # | Title | What gets built | What the student runs/sees |`
- Final file structure tree
- JS→C concept mapping: which new C concepts each lesson introduces, and the nearest JS equivalent
- Comparison table: C++ original features → C port equivalents (e.g. `std::vector` → fixed array + `compact_pool`)

**`README.md`** must contain:

- Course title and one-line description
- Prerequisites (recommend completing Snake first for input/backbuffer familiarity)
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
- For Lessons 03, 06, 07, 08, 09 — give each unique technique (Bresenham, rotation matrix, toroidal wrap, compact_pool, circle collision) a full `> **New technique:**` block with:
  - What it does
  - Why we do it this way (vs the JS/browser approach)
  - The JS equivalent
  - One concrete worked example with numbers

### Phase 4 — Retrospective

Create `COURSE-BUILDER-IMPROVEMENTS.md` with:

- What patterns from `course-builder.md` worked well and why
- What was missing, unclear, or needed adjustment
- Concrete suggested edits with before/after diffs (quoted)
- Any C/game concepts that came up in the Asteroids course but aren't covered in `course-builder.md`

---

## Quality bars

### Code quality (applies to every file in `course/src/`)

| Rule                                         | Detail                                                                                      |
| -------------------------------------------- | ------------------------------------------------------------------------------------------- |
| No `malloc` in the game loop                 | `backbuffer.pixels` allocated once at startup; entity pools are fixed arrays in `GameState` |
| No OOP patterns                              | No vtables, no function-pointer polymorphism for its own sake                               |
| Flat arrays, not linked lists                | `SpaceObject asteroids[MAX_ASTEROIDS]` not `std::vector<SpaceObject>`                       |
| `memset` / `= {0}` for init                  | Never read uninitialized memory — `GameInput input = {0}`, `GameState state = {0}`          |
| `compact_pool` over ordered removal          | Swap dead entry with last, decrement count — O(1) vs O(n) shift                             |
| Enums over magic integers                    | `GAME_PHASE`, `SOUND_ID` are not magic ints; `PHASE_PLAYING` not `phase == 0`               |
| `GAME_PHASE` state machine                   | `switch (state->phase)` dispatch in both `update` and `render`                              |
| Input double-buffered                        | `GameInput inputs[2]` + index swap; `prepare_input_frame(old, current)` copies `ended_down` |
| `GameInput` union                            | `union { buttons[BUTTON_COUNT]; struct { left, right, up, fire }; }` — `BUTTON_COUNT 4`     |
| `pitch` in backbuffer                        | `pitch = width * 4`; pixel index = `py * (pitch/4) + px` everywhere                         |
| 8×8 ASCII-indexed bitmap font                | `FONT_8X8[128][8]`; index by `(int)c` direct; BIT7-left mask `(0x80 >> col)`                |
| `GAME_RGBA` = `0xAARRGGBB`                   | Tetris byte order; validated against both backends in Lesson 02                             |
| Platform independence                        | `game.c` / `game.h` / `audio.c` must compile without X11 or Raylib headers                  |
| `platform.h` 4-function contract             | `platform_init`, `platform_get_time`, `platform_get_input`, `platform_shutdown`             |
| `DEBUG_TRAP`/`ASSERT` macro                  | In `game.h` under `#ifdef DEBUG`; `__builtin_trap()` on clang/gcc; `-DDEBUG` in debug build |
| Spatial audio pan                            | Explosion sound: `pan = (asteroid.x / SCREEN_W) * 2.0f - 1.0f`; call `game_play_sound_at`   |
| Toroidal wrapping via `draw_pixel_w`         | Clip-then-wrap in the lowest-level pixel writer; all higher-level draws get it for free     |
| No `sqrt` in collision hot path              | `dx*dx + dy*dy < r*r` — document the JS `Math.hypot` equivalent and why sqrt is avoided     |
| Rotation matrix, not trigonometry every draw | Pre-compute `cos(angle)`, `sin(angle)` once per object per frame; apply to each vertex      |
| `srand(time(NULL))` called exactly once      | In `asteroids_init`, documented with JS analogy                                             |

### Lesson quality

| Rule                                  | Detail                                                                                             |
| ------------------------------------- | -------------------------------------------------------------------------------------------------- |
| Each lesson compilable                | Student can build after every lesson, not just the last                                            |
| One major concept per lesson          | Don't introduce rotation matrix, entity pools, AND collision in the same lesson                    |
| Runnable milestone each lesson        | Student sees something new on screen every lesson                                                  |
| JS analogies present                  | Every first use of a C concept has a JS counterpart                                                |
| No forward references                 | Lesson N doesn't assume knowledge from Lesson N+2                                                  |
| Unique techniques get worked examples | Bresenham, rotation matrix, compact_pool, circle collision each get a step-by-step numeric example |

---

## Lesson progression (required milestones)

Follow this exact ordering. You may split a milestone into 2 lessons if it's getting too long, but do not reorder or skip:

1. **Window + backbuffer** — Open a window, fill it with black. Introduce `AsteroidsBackbuffer` with `pitch`, unified `build-dev.sh`, `platform_shutdown`, `DEBUG_TRAP`/`ASSERT`. Show the C equivalent of `document.createElement('canvas')`.

2. **Drawing primitives** — `draw_pixel`, `draw_rect`, `draw_rect_blend` with alpha blend. Introduce `GAME_RGBA` = `0xAARRGGBB`; cross-backend pixel-format validation (pure-red rect on both X11 and Raylib). Explain how `0xAARRGGBB` differs from the reference `ASTEROIDS_RGBA` = `0xAABBGGRR` and why the course standardises on Tetris convention.

3. **Line drawing (Bresenham's algorithm)** — `draw_line` with Bresenham's integer algorithm. Compare to `ctx.lineTo()`. Explain why the game uses integer line drawing for wireframe graphics instead of floating-point. Show the diagonal line test artifact fixed by Bresenham vs naïve slope. Draw a triangle with three `draw_line` calls.

4. **Input** — Move a triangle around the screen with keyboard. Introduce `GameButtonState`, `UPDATE_BUTTON`, `prepare_input_frame` with `inputs[2]` double-buffer, input union. Note that `RepeatInterval`/DAS-ARR is intentionally absent here (fire is "just pressed", not held) — explain the difference from Tetris.

5. **Game structure** — Introduce `GameState`, `asteroids_init`, `asteroids_update`, `asteroids_render` split. Show how `GAME_PHASE` enum drives the `switch` dispatch in both functions. No actual game logic yet — just the skeleton with a triangle that renders and a placeholder update.

6. **Vec2 and the rotation matrix** — The ship wireframe model as `Vec2 ship_model[3]`. Build the `draw_wireframe` function: compute `cos(angle)` / `sin(angle)` once, loop over vertices applying `x' = x*c - y*s`, `y' = x*s + y*c`. Introduce `srand(time(NULL))` and the JS analogy. Draw the ship at screen center, rotating with left/right keys.

7. **Ship physics and toroidal wrapping** — Add thrust acceleration: `dx += cos(angle) * THRUST_ACCEL * dt`. Introduce toroidal `draw_pixel_w` that wraps pixel coordinates with `% width / % height` so lines cross edges seamlessly. Ship flies around the screen, wrapping at edges, damping optional. This is the first frame where the student has a "real" game object.

8. **Asteroids — entity pool and wrapping** — Add `SpaceObject asteroids[MAX_ASTEROIDS]` pool. Introduce `compact_pool` swap-with-last. Spawn 3 large asteroids at `asteroids_init`. Render them with `draw_wireframe` using the jagged `asteroid_model[20]`. Asteroids wrap toroidally. Demonstrate removing an entry vs `splice`.

9. **Bullets — fire and lifetime** — Add `SpaceObject bullets[MAX_BULLETS]` pool. Fire on `fire` button ("just pressed" check): spawn bullet at ship nose, velocity = `ship.dx + cos(angle)*BULLET_SPEED`. `compact_pool` bullets whose `timer <= 0`. Draw bullets as single pixels via `draw_pixel_w`.

10. **Collision detection** — `is_point_inside_circle`: check bullet vs asteroid (`dx²+dy² < r²`). On hit: award score, split large→2 medium, medium→2 small, small→nothing via `add_asteroid`. Compact dead objects. Introduce player-asteroid collision (same formula). Show "just pressed to restart" in `PHASE_DEAD`.

11. **Font + UI** — Score display in top-left. Introduce upgraded `FONT_8X8[128][8]` with BIT7-left mask. Show the `> **Course Note:**` callout explaining the upgrade from the reference's 5×7 column-major font. Add `SCORE: 000` and level counter. Show death overlay with "PRESS FIRE TO RESTART". Add `snprintf` for integer-to-string.

12. **Audio** — Sound effects: thrust loop (while `up.ended_down`), bullet fire (on fire "just pressed"), asteroid explosion (3 sizes, panned by `x` position), ship death. Introduce `AudioOutputBuffer`, `SoundInstance`, `fill_audio_buffer`, click prevention, stereo pan `(x / SCREEN_W) * 2.0f - 1.0f`. Full explanation of Casey's latency model for the platform side (ALSA `start_threshold` + pre-fill silence, Raylib `buffer_size_frames`).

13. **Polish + `utils/` refactor** — Refactor drawing code to `utils/draw-shapes.c`, `utils/draw-text.c`, math helpers to `utils/math.h`, audio helpers to `utils/audio.h`. Explain the utils/ architecture (same as Tetris). Add high score persistence in `GameState`. Increase initial asteroid count each wave (level scaling). Final state: complete, shippable game.

---

## Deviation policy

The `course-builder.md` is a **strong guideline**, not a rigid spec. Deviate when:

- The actual Asteroids code does something better-structured than the pattern in `course-builder.md`
- A pattern would confuse a JS developer more than help them
- A simplification hides something important about how C/games actually work

**When you deviate:**

1. Add `/* COURSE NOTE: ... */` in the source file
2. Add a `> **Course Note:**` callout in the relevant lesson
3. Add an entry to `COURSE-BUILDER-IMPROVEMENTS.md`

Do **not** deviate just to simplify. Complexity that teaches something important should stay.

---

## Engineering principles to embed throughout

These come from Casey Muratori's Handmade Hero series. Reference them by name in callouts:

- **One frame = one call chain.** Nothing fires outside `asteroids_update()` + `asteroids_render()`.
- **State is always visible.** All game state lives in `GameState`. No hidden static locals except `font_glyphs` (constant data, not mutable state).
- **Fixed arrays, not dynamic allocation.** Pool = `SpaceObject arr[MAX_N]` + `count` + `compact_pool`. Never `malloc` in the game loop.
- **Enums prevent category errors.** `GAME_PHASE`, `SOUND_ID` are not magic ints.
- **The platform layer is a thin translator.** It maps OS events → `GameInput`, OS timer → `delta_time`, OS audio → `AudioOutputBuffer`. It calls game functions; game functions never call platform functions.
- **Audio phase is continuous.** `inst->phase` is never reset between buffer fills or on slot reuse. Resetting it causes a click.
- **Comments explain _why_.** Not what — the code already shows what.
- **No sqrt in hot paths.** Use squared-distance comparisons for collision. `dx*dx + dy*dy < r*r` is equivalent to `hypot < r` and avoids the sqrt entirely.
- **Compute trig once per object per frame.** `cos(angle)` and `sin(angle)` are expensive; cache them in locals before the vertex loop.

---

## Common bugs reference

| Bug                             | Symptom                                    | Root cause                                                                                   | Fix                                                                                   |
| ------------------------------- | ------------------------------------------ | -------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| Red/blue channels swapped       | Red objects appear blue on X11 or Raylib   | Using reference `ASTEROIDS_RGBA` = `0xAABBGGRR` instead of Tetris `GAME_RGBA` = `0xAARRGGBB` | Standardise on `GAME_RGBA`; test with pure-red rect on both backends before Lesson 03 |
| Diagonal shear in rendering     | Lines look diagonally offset               | Using `width` instead of `pitch/4` as row stride                                             | Add `pitch` field; use `py * (bb->pitch/4) + px`                                      |
| Asteroids not wrapping at edges | Objects vanish off-screen                  | Using screen-edge clamp instead of toroidal wrap                                             | Implement `draw_pixel_w` with `% width`; apply to all draw_line calls                 |
| Entity pool corruption          | Random crash after destroying asteroid     | Using `count--` without compacting; reading stale entries                                    | Use `compact_pool` swap-with-last pattern                                             |
| Rotation drift                  | Ship rotates slightly wrong over time      | Accumulating angle with `+=` and never normalizing                                           | `fmodf(angle + delta, 2.0f * M_PI)` or let it drift (acceptable for Asteroids)        |
| Input "sticky" on key release   | Fire button fires multiple times per press | Using `ended_down` alone instead of "just pressed" check                                     | Check `ended_down && half_transition_count > 0`                                       |
| Wrong keysym on X11             | Non-ASCII keys don't respond               | Using `XLookupKeysym` with `index=0`                                                         | Use `XkbKeycodeToKeysym` with full modifier handling                                  |
| Window close doesn't exit       | Clicking × does nothing                    | Missing `WM_DELETE_WINDOW` `XInternAtom` registration                                        | Add `XInternAtom("WM_DELETE_WINDOW")` + `ClientMessage` event check                   |
| ALSA audio silent               | No sound on X11                            | `start_threshold` left at default (waits for buffer fill)                                    | Set `snd_pcm_sw_params_set_start_threshold` to `hw_buffer_size`; pre-fill silence     |
| Raylib audio frame skip         | Crackling audio on Raylib                  | `sample_count` in callback doesn't match registered `buffer_size_frames`                     | Match `sample_count` exactly to registered size                                       |
| `memset` wipes audio config     | Audio stops after game restart             | `memset(&state, 0, sizeof(state))` zeroes `samples_per_second`                               | Save audio config before `memset`; restore after                                      |
| Collision misses                | Bullet passes through asteroid             | Using `sqrt` + float comparison with epsilon                                                 | Switch to `dx*dx + dy*dy < r*r` (no epsilon needed)                                   |
| Double-seeding                  | Same asteroid pattern every run            | `srand` called in `asteroids_update` instead of `asteroids_init`                             | Seed once in `asteroids_init` only                                                    |

---

## Quick reference

| Pattern                  | Correct form                                                  | Wrong form                      | Notes                       |
| ------------------------ | ------------------------------------------------------------- | ------------------------------- | --------------------------- |
| Pixel index              | `py*(bb->pitch/4)+px`                                         | `py*bb->width+px`               | pitch handles row padding   |
| Color macro              | `GAME_RGBA(r,g,b,a)` = `0xAARRGGBB`                           | `ASTEROIDS_RGBA` = `0xAABBGGRR` | Match Tetris convention     |
| Compact pool             | swap `arr[i]` ↔ `arr[--count]`                                | `memmove` to shift left         | O(1) vs O(n)                |
| Circle hit               | `dx*dx+dy*dy < r*r`                                           | `sqrt(dx*dx+dy*dy) < r`         | No sqrt in hot path         |
| Rotation matrix          | `x'=x*c-y*s`, `y'=x*s+y*c`                                    | `atan2` each frame              | Compute c,s once per object |
| Toroidal wrap            | `((px % w) + w) % w`                                          | `if (px < 0) px += w`           | Handles negative coords     |
| "Just pressed"           | `ended_down && half_transition_count > 0`                     | `ended_down` alone              | Avoids fire-held repeat     |
| RNG seed                 | `srand((unsigned)time(NULL))` once in `asteroids_init`        | `srand` in loop                 | Seed once only              |
| X11 keysym               | `XkbKeycodeToKeysym(dpy, code, 0, 0)`                         | `XLookupKeysym(&ev.xkey, 0)`    | Modifier-safe               |
| Window close             | `XInternAtom` + `ClientMessage` check                         | (missing)                       | Required for X button       |
| ALSA threshold           | `snd_pcm_sw_params_set_start_threshold(pcm, sw, hw_buf_size)` | default                         | Prevents silent start       |
| Raylib audio             | `sample_count == buffer_size_frames`                          | mismatched sizes                | Frame skip if wrong         |
| Audio config after reset | save `samples_per_second` before `memset`; restore after      | `memset` entire state           | Preserves audio init        |
