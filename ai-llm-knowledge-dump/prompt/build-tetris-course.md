# Build the Tetris Course

## Source files to analyse

Read **all** of these before writing anything:

- `@games/tetris/build-dev.sh`
- `@games/tetris/src/audio.c`
- `@games/tetris/src/game.c`
- `@games/tetris/src/game.h`
- `@games/tetris/src/main_raylib.c`
- `@games/tetris/src/main_x11.c`
- `@games/tetris/src/platform.h`
- `@games/tetris/src/utils/audio.h`
- `@games/tetris/src/utils/backbuffer.h`
- `@games/tetris/src/utils/draw-shapes.c`
- `@games/tetris/src/utils/draw-shapes.h`
- `@games/tetris/src/utils/draw-text.c`
- `@games/tetris/src/utils/draw-text.h`
- `@games/tetris/src/utils/math.h`
- `@ai-llm-knowledge-dump/prompt/course-builder.md` — your instruction manual
- `@ai-llm-knowledge-dump/llm.txt` — student background profile

---

## Output directory

`ai-llm-knowledge-dump/generated-courses/javidx9/tetris/`

All output goes here. Nothing outside this directory.

---

## Example of the Output structure based on the course-builder patterns and the code referenced

```
ai-llm-knowledge-dump/generated-courses/javidx9/tetris/
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
    │   │   └── math.h
    │   ├── game.h
    │   ├── game.c
    │   ├── audio.c
    │   ├── platform.h
    │   ├── main_x11.c
    │   └── main_raylib.c
    └── lessons/
        ├── 01-window-and-backbuffer.md
        ├── 02-drawing-primitives.md
        └── ...
```

---

## Workflow — follow this order strictly

### Phase 0 — Analyse (no output yet)

1. Read every source file listed above.
2. Read the full `course-builder.md`.
3. Read `llm.txt`.
4. Identify the natural build progression for the game.

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
- Any C/game concepts that came up in the Tetris course but aren't covered in `course-builder.md`

---

## Quality bars

### Code quality (applies to every file in `course/src/`)

| Rule                          | Detail                                                                     |
| ----------------------------- | -------------------------------------------------------------------------- |
| No `malloc` in the game loop  | Allocate at startup only                                                   |
| No OOP patterns               | No vtables, no function-pointer polymorphism for its own sake              |
| Flat arrays over linked lists | `Entity entities[MAX_N]` not `Entity *head`                                |
| Enums over magic integers     | `GAME_PHASE_PLAYING` not `phase == 2`                                      |
| `memset` / `= {0}` for init   | Never read uninitialized memory                                            |
| Input double-buffered         | `GameInput inputs[2]` + index swap                                         |
| Audio phase never reset       | `inst->phase` ticks continuously — never set to 0 on slot reuse            |
| Platform independence         | `game.c` / `game.h` / `audio.c` must compile without X11 or Raylib headers |

### Lesson quality

| Rule                           | Detail                                                          |
| ------------------------------ | --------------------------------------------------------------- |
| Each lesson compilable         | Student can build after every lesson, not just the last         |
| One major concept per lesson   | Don't introduce structs, pointers, and enums in the same lesson |
| Runnable milestone each lesson | Student sees something new on screen or hears something new     |
| JS analogies present           | Every first use of a C concept has a JS counterpart             |
| No forward references          | Lesson N doesn't assume knowledge from Lesson N+2               |

---

## Lesson progression (required milestones)

Follow this exact ordering. You may split a milestone into 2 lessons if it's getting too long, but do not reorder or skip:

1. **Window + backbuffer** — Open a window, fill it with a solid colour. Introduce the backbuffer pipeline.
2. **Drawing primitives** — `draw_rect()`, `draw_rect_blend()`. Introduce pixel coordinates, RGBA packing.
3. **Input** — Move a coloured rectangle with the keyboard. Introduce `GameButtonState`, `UPDATE_BUTTON`, `prepare_input_frame`, double-buffering.
4. **Game structure** — Introduce `GameState`, `game_init()`, `game_update()`, `game_render()` split. No game logic yet — just the skeleton.
5. **Tetromino data** — Render one tetromino in a fixed position. Introduce the `char[16]` bitmap representation, `TETROMINO_R_DIR`, rotation indexing.
6. **Tetromino movement** — Move and rotate with keyboard. Introduce `RepeatInterval` / DAS-ARR. Introduce `tetromino_does_piece_fit()`.
7. **The playfield** — Add walls and the locked-piece field. Pieces lock on landing. Introduce `TETRIS_FIELD_CELL` enum.
8. **Line clearing** — Detect full rows, flash animation, collapse. Introduce `GAME_PHASE`-style state separation (even if using a boolean now — explain the pattern and why the enum is the upgrade).
9. **Scoring and levels** — Score based on lines cleared, speed increase. Introduce `drop_timer.interval` scaling.
10. **UI** — Sidebar: score, level, next piece. Introduce `draw_text()`, the bitmap font, `draw_char()`.
11. **Audio** — Sound effects on move/rotate/drop/clear. Introduce `AudioOutputBuffer`, `SoundInstance`, `game_play_sound_at()`, click prevention, stereo panning. Full explanation of Casey's latency model for the platform side.
12. **Music** — Background Tetris theme via `MusicSequencer` + `ToneGenerator`. MIDI→frequency.
13. **Polish** — Ghost piece, game-over overlay, restart. Final state: complete game.

---

## Deviation policy

The `course-builder.md` is a **strong guideline**, not a rigid spec. Deviate when:

- The actual Tetris code does something better-structured than the pattern in `course-builder.md`
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

- **One frame = one call chain.** Nothing fires outside `game_update()` + `game_render()`.
- **State is always visible.** All important state lives in `GameState`. No hidden static locals.
- **Fixed arrays, not dynamic allocation.** Pool = `Type arr[MAX_N]` + linear search for free slot.
- **Enums prevent category errors.** `GAME_PHASE`, `SOUND_ID`, `TETROMINO_R_DIR` are not magic ints.
- **The platform layer is a thin translator.** It maps OS events → `GameInput`, OS timer → `delta_time`, OS audio → `AudioOutputBuffer`. It calls game functions; game functions never call platform functions.
- **Audio phase is continuous.** `inst->phase` is never reset between buffer fills or on slot reuse. Resetting it causes a click.
- **Comments explain _why_.** Not what — the code already shows what.
