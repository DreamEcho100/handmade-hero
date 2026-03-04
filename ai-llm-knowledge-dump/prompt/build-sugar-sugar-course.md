# Build the Sugar Sugar Course

## Source files to analyse

Read **all** of these before writing anything:

- `@ai-llm-knowledge-dump/generated-courses/flash-games/_ignore/sugar-sugar/course/build_x11.sh`
- `@ai-llm-knowledge-dump/generated-courses/flash-games/_ignore/sugar-sugar/course/build_raylib.sh`
- `@ai-llm-knowledge-dump/generated-courses/flash-games/_ignore/sugar-sugar/course/src/font.h`
- `@ai-llm-knowledge-dump/generated-courses/flash-games/_ignore/sugar-sugar/course/src/sounds.h`
- `@ai-llm-knowledge-dump/generated-courses/flash-games/_ignore/sugar-sugar/course/src/platform.h`
- `@ai-llm-knowledge-dump/generated-courses/flash-games/_ignore/sugar-sugar/course/src/game.h`
- `@ai-llm-knowledge-dump/generated-courses/flash-games/_ignore/sugar-sugar/course/src/game.c`
- `@ai-llm-knowledge-dump/generated-courses/flash-games/_ignore/sugar-sugar/course/src/levels.c`
- `@ai-llm-knowledge-dump/generated-courses/flash-games/_ignore/sugar-sugar/course/src/main_x11.c`
- `@ai-llm-knowledge-dump/generated-courses/flash-games/_ignore/sugar-sugar/course/src/main_raylib.c`
- `@ai-llm-knowledge-dump/prompt/course-builder.md` — your instruction manual
- `@ai-llm-knowledge-dump/llm.txt` — student background profile

---

## Output directory

`ai-llm-knowledge-dump/generated-courses/flash-games/sugar-sugar/`

All output goes here. Nothing outside this directory.

---

## Output structure

```
ai-llm-knowledge-dump/generated-courses/flash-games/sugar-sugar/
├── PLAN.md               # Analyse & plan before writing any lesson
├── README.md             # Human-readable course overview
├── PLAN-TRACKER.md       # Progress log; update after every file created
├── COURSE-BUILDER-IMPROVEMENTS.md  # Written at the very end
└── course/
    ├── build-dev.sh
    └── src/
        ├── font.h
        ├── sounds.h
        ├── platform.h
        ├── game.h
        ├── game.c
        ├── levels.c
        ├── main_x11.c
        └── main_raylib.c
```

---

## Build script

> **Note on build scripts:** The reference source uses two separate scripts (`build_x11.sh` and `build_raylib.sh`) rather than the unified `build-dev.sh` seen in Tetris. The generated course **replaces both with a unified `build-dev.sh --backend=x11|raylib -r -d`**, matching the architecture students encounter in Tetris. Document the trade-off in Lesson 01 and in `COURSE-BUILDER-IMPROVEMENTS.md`.

The unified `build-dev.sh` for this game must compile three source files:

```bash
SOURCES="src/game.c src/levels.c src/main_${BACKEND}.c"
```

Introduce `build-dev.sh` in Lesson 01 — unlike other games, even the minimal first-lesson skeleton has `levels.c` as a separate translation unit from the start.

---

## Course enhancements over the reference source

> **Audio:** The reference source declares `platform_play_sound(int)` and `platform_play_music(int)` in `platform.h` but all backends implement them as empty stubs — **no audio plays**. The generated course **promotes these stubs to a real Raylib audio implementation** in `main_raylib.c` (using `LoadSound`, `PlaySound`, `LoadMusicStream`) and documents the X11 stub trade-off. All changes are explained with `> **Course Note:**` callouts in the relevant lessons and documented in `COURSE-BUILDER-IMPROVEMENTS.md`.

> **Coordinate system and world units:** This game is a **`Y-down`, pixel-coordinate** exception to the course-builder's default `Y-up meters` rule. As a falling-sand / cellular automaton, the physics grid maps directly to screen pixels — using a `PIXELS_PER_METER` conversion would add indirection with no benefit. Document this exception explicitly in Lesson 04 (first physics lesson) and in the physics constants section of `game.h`, with a `> **Course Note:**` linking to the course-builder rule and explaining why grain simulations are exempt.

> **Audio pattern:** The reference uses a **callback pattern** (`platform_play_sound(id)` / `platform_play_music(id)`) rather than the Tetris ALSA audio-thread model. This is intentional and simpler for a mouse-driven puzzle game with infrequent discrete sound events (cup fill chime, level fanfare). Document this trade-off in Lesson 13 and in `COURSE-BUILDER-IMPROVEMENTS.md`.

---

## Reference source vs `course-builder.md`: Gap analysis

Run this analysis before generating any source files. All deviations below **must** be documented in the relevant lesson and in `COURSE-BUILDER-IMPROVEMENTS.md`.

### What the reference source already follows ✅

- CPU backbuffer pipeline (`GameBackbuffer`, `uint32_t *pixels`, GPU upload per frame)
- `GAME_RGB` / `GAME_RGBA` macros (`0xAARRGGBB` byte order)
- 3-layer split: game logic / `platform.h` contract / `main_*.c` backends
- `GameButtonState` (`half_transition_count`, `ended_down`); `UPDATE_BUTTON` macro
- `BUTTON_PRESSED` / `BUTTON_RELEASED` convenience macros
- `prepare_input_frame()` to reset per-frame transition counts
- `GAME_PHASE` enum (`PHASE_TITLE`, `PHASE_PLAYING`, `PHASE_LEVEL_COMPLETE`, `PHASE_FREEPLAY`) with `change_phase()` helper — state machine introduced from the start
- 8×8 ASCII-indexed bitmap font in a separate `font.h` (BIT7-left convention, identical to the Tetris standard)
- Platform contract minimal and stable (`platform_init`, `platform_get_time`, `platform_get_input`, `platform_display_backbuffer`)
- Data-driven level definitions isolated in `levels.c` with designated-initialiser macros (`EMITTER(...)`, `CUP(...)`, etc.)
- `= {0}` aggregate init; `memset` for full reset
- `should_quit` flag on `GameState` so the platform loop can exit cleanly without reaching into game internals
- `sounds.h` with typed `SOUND_ID` and `MUSIC_ID` enums (avoids magic integers in audio calls)
- `render_*` functions are pure reads of `GameState` — never mutate simulation data
- `pitch` field on `GameBackbuffer` (row stride, not `width × 4` assumed everywhere)
- `#define CANVAS_W 640` / `CANVAS_H 480` as fixed logical canvas size

### What the reference source deviates from ✅ → generate as improvements

| Deviation | Course-builder rule | Action |
| --------- | ------------------- | ------ |
| Two separate build scripts | Unified `build-dev.sh` mandatory | Replace with unified script; document in Lesson 01 |
| Audio stubs (no sound plays) | Sound set ≥ 6–8 sounds; real platform implementation | Promote Raylib implementation; keep X11 as documented stub |
| No VSync / frame-rate control in build scripts | VSync with manual fallback from Lesson 1 | Add `SetTargetFPS(60)` (Raylib) and `usleep`-based cap (X11) |
| No letterbox centering | Mandatory from Lesson 1 | Add letterbox in first rendering lesson |
| Coordinate system is Y-down pixels | Default is Y-up meters | Document the cellular-automaton exception explicitly |
| `GAME_PHASE` enum uses `PHASE_*` prefix, not `GAME_PHASE_*` | `GAME_PHASE_*` naming | Keep the reference naming (already consistent internally); note the difference |

---

## Course-unique patterns (not in Tetris or Frogger)

These concepts are introduced for the **first time** in this course. Give each a full explanation with JS analogies.

### 1. Struct-of-Arrays (SoA) grain pool

```c
typedef struct {
    float   x[MAX_GRAINS];
    float   y[MAX_GRAINS];
    float   vx[MAX_GRAINS];
    float   vy[MAX_GRAINS];
    uint8_t color[MAX_GRAINS];
    uint8_t active[MAX_GRAINS];
    int     count;
} GrainPool;
```

**JS analogy:** In JS you'd write `grains.push({ x, y, vx, vy, color })` — an Array of Objects. C can do the same (Array of Structs), but it wastes cache bandwidth when you only need `x` and `y` for a physics pass. SoA packs all `x` values together so the CPU prefetcher loads the next 16 floats automatically.

Teach this in Lesson 05 (grain pool intro). Compare AoS vs SoA with a concrete cache-line diagram. This is the most novel memory-layout concept in the course.

### 2. `LineBitmap` — single-byte collision + render layer

```c
typedef struct {
    uint8_t pixels[CANVAS_W * CANVAS_H];
} LineBitmap;

/* Encoding:
 *   0        → empty
 *   1        → pre-set obstacle / cup wall (stamped at level load)
 *   255      → player-drawn line
 *   2..5     → settled grain, color = pixels[i] - 2
 */
```

One byte encodes three different layers: empty, solid, and settled-grain-with-color. The trick that makes this possible is that `2..5` maps directly to `GRAIN_COLOR` (0-based), so settled grain color is just `pixels[i] - 2`.

Teach this in Lesson 06 (collision). Contrast with the "naïve" approach of two separate arrays (one for solid flags, one for color), and show why the single-byte encoding is both smaller and faster.

**JS analogy:** Like a `Uint8Array` typed array used as a 2D flat map — each index is `y * width + x`.

### 3. Cellular automaton physics (falling-sand style)

Grain movement is **not** continuous integration. Each grain has `(x, y, vx, vy)` in float coordinates, but collision happens by sampling the `LineBitmap` at the **integer pixel** below and to both diagonals. This is a hybrid: smooth float positions + discrete pixel collision. The boundary is intentional — it lets grains pile realistically without complex contact resolution.

Key sub-concepts to teach:
- Gravity as `vy += GRAVITY * dt` (standard Euler)
- Bounce with `GRAIN_BOUNCE` coefficient (only when `|vy| > GRAIN_BOUNCE_MIN`)
- Diagonal slide with `GRAIN_SLIDE_REACH` (angle of repose control)
- **Settling and baking**: a grain that has been "still" for `GRAIN_SETTLE_FRAMES` consecutive frames is removed from the pool and written as a static byte into `LineBitmap`. This permanently converts active particles into part of the collision geometry — teaching the concept of "hot" vs "cold" data.

### 4. Mouse input: position + drag tracking

```c
typedef struct {
    int x, y;           /* current pixel position */
    int prev_x, prev_y; /* last frame's position  */
    GameButtonState left;
    GameButtonState right;
} MouseInput;
```

The platform reads `GetMousePosition()` (Raylib) or `MotionNotify` + `ButtonPress` (X11) and fills this struct. The game uses the delta `(x - prev_x, y - prev_y)` to draw a filled line between the two points using a Bresenham-style brush algorithm — so no gaps appear even when the mouse moves fast.

**JS analogy:** Like `mousemove` events with `event.movementX` / `event.movementY`, but polled once per frame instead of callback-driven.

Teach this in Lesson 03. Draw the Bresenham vs "just stamp at current position" comparison: stamping only the current position leaves gaps at high speed.

### 5. Data-driven levels with designated initialisers

```c
LevelDef g_levels[TOTAL_LEVELS] = {
    [0] = {
        .emitter_count = 1,
        .emitters      = { EMITTER(320, 20, 240) },
        .cup_count     = 1,
        .cups          = { CUP(278, 370, 85, 100, GRAIN_WHITE, 150) },
    },
    /* ... */
};
```

**JS analogy:** Like a `const levels = [{ emitters: [...], cups: [...] }]` array of plain objects, but zero heap allocation — the compiler bakes everything into the binary's `.data` segment.

Teach this in Lesson 09. Show how `designated initialisers` (`[0] = { .field = value }`) let you leave un-set fields as zero, and how the helper macros (`EMITTER(...)`, `CUP(...)`) make the data readable without struct-literal verbosity.

### 6. Color filter and teleporter mechanics

**Color filter:** A `ColorFilter` is a rectangle. When `update_grains()` detects that a grain's integer position lies inside a filter, it changes the grain's `color` field. No special data structure — just a bounds check inside the grain loop.

**Teleporter:** A `Teleporter` has two portal centres `(ax,ay)` and `(bx,by)` and a radius. A grain touching portal A is instantly moved to portal B (and vice-versa). A per-grain `tpcd` (teleport cooldown) byte prevents it from immediately re-entering the same portal.

Both mechanics teach how to extend the grain loop with new feature branches — each feature is a few lines of code, not a new system.

---

## Lesson sequence (target: 15 lessons)

| # | Title | What gets built | What the student runs / sees |
|---|-------|-----------------|------------------------------|
| 01 | Window and canvas | `build-dev.sh`, both backends, blank canvas | Sky-blue window on both X11 and Raylib |
| 02 | Pixel rendering | `draw_rect`, `draw_circle`, `GAME_RGB` macros, color constants | Rectangles and circles drawn from `game_render()` |
| 03 | Mouse input and drawing | `MouseInput`, brush press/drag, `LineBitmap` write, Bresenham line | Player can draw dark lines on the canvas |
| 04 | One falling grain | `vx`, `vy`, gravity, bounce; no collision yet | A single grain falls and bounces off the bottom edge |
| 05 | Grain pool | SoA `GrainPool`, `MAX_GRAINS`, active flag, spawning | Hundreds of grains falling at once |
| 06 | Grain collision | `LineBitmap` solid check; grains land on player lines | Grains pile up on drawn lines |
| 07 | Slide, settle, bake | Diagonal slide, angle of repose, `GRAIN_SETTLE_FRAMES`, baking | Grains form realistic sand piles; baked grains free the pool |
| 08 | State machine | `GAME_PHASE`, `change_phase()`, title screen | Title screen with keyboard/mouse navigation |
| 09 | Level system | `LevelDef`, designated initialisers, emitters, cups, `level_load()` | Level 1 playable: grains pour and fill a cup |
| 10 | Win detection | `check_win()`, `PHASE_LEVEL_COMPLETE`, level advance | Completing a level shows a brief celebration screen and loads the next |
| 11 | Color filters | `GRAIN_COLOR`, `ColorFilter`, per-grain color field | Grains passing through a zone change color; colored cups require matching grains |
| 12 | Obstacles and level variety | Pre-set `Obstacle` rectangles stamped at load; levels 1–15 | More complex routing puzzles |
| 13 | Font system and HUD | `font.h` (8×8 bitmap), `draw_text`, level number, grain counts | On-screen level indicator, cup fill percentage |
| 14 | Teleporters and gravity flip | `Teleporter` with cooldown, `gravity_sign` toggle | Grains warp between portals; G key flips gravity |
| 15 | Audio and final polish | Raylib audio implementation, `sounds.h`, level-complete fanfare, all 30 levels | Complete Sugar Sugar with sound effects |

---

## Workflow — follow this order strictly

### Phase 0 — Analyse (no output yet)

1. Read every source file listed at the top.
2. Read the full `course-builder.md`.
3. Read `llm.txt`.
4. Identify the natural build progression using the lesson table above as a starting point. Adjust if the source file reveals a better ordering.

### Phase 1 — Planning files (create these first, nothing else)

Create `PLAN.md`, `README.md`, and `PLAN-TRACKER.md`. Do **not** create any lesson or source file until these exist.

**`PLAN.md`** must contain:

- One-paragraph description of what the final game does
- Lesson sequence table: `| # | Title | What gets built | What the student runs/sees |`
- Final file structure tree
- JS→C concept mapping: which new C concepts each lesson introduces, and the nearest JS equivalent
- A note on which course-builder.md rules this game follows and which are intentional exceptions

**`README.md`** must contain:

- Course title and one-line description
- Prerequisites (Tetris or Snake recommended first; this course introduces SoA and cellular-automaton physics)
- How to build and run (both backends)
- Lesson list with one-line summary each
- What the student will have achieved by the end

**`PLAN-TRACKER.md`** must contain:

| Task | Status | Notes |
|------|--------|-------|
| PLAN.md | ✅ Done | |
| README.md | ✅ Done | |
| course/build-dev.sh | | |
| course/src/font.h | | |
| … | | |

Update this file every time you complete a task.

**Stop here and wait for confirmation before continuing.**
_(If running autonomously, stop after Phase 1 and report what you created.)_

### Phase 2 — Source files

Create `course/build-dev.sh` and all `course/src/**` files.

Every source file must:

- Exactly reproduce the game's final behaviour
- Have more comments than the reference — explain **why** each decision was made
- Anchor C concepts to their nearest JavaScript equivalent in a `/* JS: ... | C: ... */` comment the first time they appear
- Use `/* ══════ Section Name ══════ */` style section headers

Follow the `course-builder.md` patterns exactly unless you deliberately improve on them. If you improve something, note it in `COURSE-BUILDER-IMPROVEMENTS.md`.

**Stop here and wait for confirmation before continuing.**

### Phase 3 — Lessons

Create one Markdown lesson file per row in the lesson sequence table. Lesson files go in `course/lessons/`:

```
course/lessons/
├── lesson-01-window-and-canvas.md
├── lesson-02-pixel-rendering.md
├── ...
└── lesson-15-audio-and-polish.md
```

Each lesson must:

1. **Start with a goal statement** — one sentence explaining what the student will build
2. **Show the JS analogy** for any new C concept introduced in this lesson
3. **Show the minimum runnable code** at the start, then extend it step by step
4. **Include a "What just happened?" section** explaining the key concept in plain language
5. **Show both backends** for any platform-specific code — or state "identical on both backends"
6. **Close with an exercise** the student can do before moving to the next lesson
7. **Reference `course-builder.md` patterns** for any architectural decision that follows a documented rule

For lessons that introduce course-unique patterns (SoA, LineBitmap, cellular physics, mouse drag), add a `> **Sugar Sugar Note:**` callout explaining why this game's approach differs from what a "standard" tutorial would show.

**Stop here and wait for confirmation before continuing.**

### Phase 4 — Final documents

Write `COURSE-BUILDER-IMPROVEMENTS.md` covering:

1. Every deviation from `course-builder.md` and the reason
2. Every pattern from this game worth back-porting to `course-builder.md`
3. Patterns this game demonstrates better than the Tetris reference

---

## Key architectural decisions to preserve from the reference

### `LineBitmap` encoding — do not simplify

The single-byte encoding (`0`=empty, `1`=wall, `255`=player-line, `2..5`=baked-grain-color) is a deliberate design. **Do not split it into two separate arrays** in the course code — that would teach a worse pattern. Instead, use the encoding as a lesson about packing multiple meanings into a byte without bit-fields.

### SoA grain pool — keep as SoA, not AoS

The course should show the AoS alternative first (as a "what you'd write in JS") and then explain why SoA is better for this workload. Do **not** rewrite the pool as AoS to make it "simpler" — the cache-locality lesson is a core learning objective.

### `levels.c` as a separate translation unit from Lesson 01

Unlike Tetris where game data grows into a separate file mid-course, Sugar Sugar separates `levels.c` from the very beginning. This is intentional: it teaches that **data files and logic files are different concerns**, and that the compiler treats a `.c` file as a unit of compilation, not just an organisational bucket. Introduce this in Lesson 01 when explaining the build command.

### `platform_play_sound` / `platform_play_music` pattern vs Tetris audio thread

The Tetris audio system uses a low-level ALSA callback thread to synthesise audio samples at interrupt time. Sugar Sugar's audio is much simpler: fire-and-forget OS calls that the platform handles internally. The course should teach **both** philosophies:

- Lesson 13 (audio): explain that Raylib's `PlaySound()` fires a system audio thread internally — you don't write the mixing loop. This is appropriate when you just want to play a WAV file.
- Reference the Tetris approach for games that need procedural audio (synthesised tones, reverb, 3D positioning).
- Add this trade-off to `COURSE-BUILDER-IMPROVEMENTS.md` as a pattern worth documenting in `course-builder.md`.

---

## Common mistakes to prevent

These mistakes appear frequently when porting this code to a course. Call them out explicitly in the relevant lesson.

| Mistake | Where it bites | Prevention |
|---------|---------------|------------|
| Stamping all grains at the same `(em->x, em->y)` pixel | Lesson 05 | Show `EMITTER_SPREAD` jitter — grains must start at slightly different x positions or they pile into 3 discrete streams |
| Re-triggering grain slide every frame instead of using `GRAIN_SLIDE_REACH` | Lesson 07 | Explain angle-of-repose control; show what `REACH=1` vs `REACH=2` looks like visually |
| Checking `pixels[y * CANVAS_W + x]` without `x >= 0 && x < CANVAS_W` bounds check | Lesson 06 | Any grain near the canvas edge will read/write out-of-bounds; add explicit bounds guards |
| Using `width * 4` as the row stride instead of `pitch` | Lesson 01 | Introduce `pitch = width * sizeof(uint32_t)` in the `GameBackbuffer` section; use it from the first pixel write |
| Drawing grains by iterating the `GrainPool` for every active grain | Lesson 07 | After grains bake into `LineBitmap`, the renderer reads from `LineBitmap` — not the pool — so baked grains cost zero render time |
| Teleporter allows immediate re-entry (infinite loop) | Lesson 14 | The `tpcd` (teleport cooldown) byte must be set after teleporting; decrement it each frame |
| `GRAIN_SETTLE_FRAMES` too low → grains bake while still moving on a slope | Lesson 07 | Use the settled-speed threshold (`GRAIN_SETTLE_SPEED`) consistently; baking should only happen when `dist² < threshold` for N consecutive frames |
| `platform_play_sound()` called from the grain physics loop | Lesson 13 | Sound fires once per cup-fill event in `change_phase()`, never from the per-grain loop — the loop runs 4096 × 60 fps |

---

## `COURSE-BUILDER-IMPROVEMENTS.md` topics (write at the end)

At minimum, document these items:

1. **`build_x11.sh` + `build_raylib.sh` → unified `build-dev.sh`**: trade-off between simplicity (two scripts, no flag parsing) and consistency across courses
2. **Y-down pixel coordinates as an intentional exception**: cellular automaton / falling-sand games are exempt from the Y-up / world-units rule; the course-builder should document this exception explicitly
3. **`platform_play_sound` / `platform_play_music` callback pattern**: lighter alternative to ALSA threads for simple event-driven audio; worth adding to `course-builder.md` as an "Audio approach decision" section
4. **SoA grain pool**: this is the most advanced memory-layout lesson in any course so far; worth adding a `## Struct-of-Arrays (SoA) pools` section to `course-builder.md`
5. **`LineBitmap` single-byte packed layers**: a reusable technique for any cellular automaton or tilemap that needs to pack type + data into one byte without bit-fields
6. **`levels.c` as a separate translation unit from Day 1**: contrast with games that grow into separate files mid-course; add a guideline to `course-builder.md` about separating read-only data from logic from the start

---

## Checklist before declaring the course done

- [ ] PLAN.md exists and matches final lesson structure
- [ ] `build-dev.sh` compiles with `--backend=x11` and `--backend=raylib` without errors
- [ ] All code compiles with `-Wall -Wextra` without warnings on both backends
- [ ] `prepare_input_frame()` is called each frame before `platform_get_input()`
- [ ] `game_render()` is a pure read of `GameState` — no physics updates inside it
- [ ] `change_phase()` is the single location for all audio triggers and phase-specific init
- [ ] `LineBitmap` encoding (0/1/255/2-5) documented in comments and in a lesson
- [ ] SoA grain pool documented with JS AoS comparison in Lesson 05
- [ ] Grain bounds checks (`x >= 0 && x < CANVAS_W`) present in all `LineBitmap` reads/writes
- [ ] `pitch` field used for all row-stride calculations (never `width * 4` hardcoded)
- [ ] Mouse drag draws with Bresenham line (no gaps at high speed)
- [ ] Grain settle/bake tested at multiple frame rates (cap `dt` at `0.1f`)
- [ ] Teleporter cooldown (`tpcd`) prevents re-entry loops
- [ ] Audio fires from `change_phase()`, not from the grain physics loop
- [ ] Raylib backend implements real audio (`LoadSound`/`PlaySound`/`LoadMusicStream`)
- [ ] X11 backend audio stubs documented clearly with a note on how to extend them
- [ ] All 30 levels compile and the level-select grid is fully navigable
- [ ] Y-down / pixel-coordinate exception documented in Lesson 04 and in `PLAN.md`
- [ ] `COURSE-BUILDER-IMPROVEMENTS.md` written and covers all 6 topics above
- [ ] PLAN-TRACKER.md reflects final status of all tasks
