# Snake in C — A Handmade Hero-Style Game Course

Build a complete Snake clone from a blank window to a polished, playable game with audio —
while learning the C patterns used in Casey Muratori's Handmade Hero series.

---

## Prerequisites

- Basic C syntax: variables, loops, functions, structs
- Ability to read pointer notation (`*ptr`, `ptr->field`)
- Linux with `clang` installed
- One of the two display backends (see Build section)

No prior game programming or graphics experience required.

---

## What you'll build

- 60×20 cell grid, 20 px cells → 1200×460 px window
- CPU pixel buffer rendered to GPU texture each frame
- Ring-buffer snake: no `memmove`, no linked list, no `malloc` in the game loop
- Delta-time tick-based movement with speed scaling
- Score + best-score header panel
- Wall and self-collision with game-over overlay
- Procedural sound effects for food-eaten and game-over (no audio files needed)
- Restart preserving best score
- Two backends: X11/GLX (Linux native) and Raylib (portable)

---

## Build instructions

### X11/GLX backend

```bash
sudo apt install libx11-dev libxkbcommon-dev libasound2-dev
cd course/
chmod +x build-dev.sh
./build-dev.sh --backend x11
./build/snake_x11
```

### Raylib backend

```bash
sudo apt install libraylib-dev   # Ubuntu 22.04+
# or build from source: https://github.com/raysan5/raylib
cd course/
./build-dev.sh --backend raylib
./build/snake_raylib
```

### Debug build (AddressSanitizer + UBSanitizer)

```bash
./build-dev.sh --backend x11 --debug
```

### Controls

| Key | Action |
|-----|--------|
| Left / A | Turn left (CCW) |
| Right / D | Turn right (CW) |
| R / Space | Restart after game over |
| Q / Escape | Quit |

---

## Lesson list

| # | File | Summary |
|---|------|---------|
| 01 | `01-window-and-backbuffer.md` | Open a window; allocate a CPU pixel buffer; upload to GPU each frame |
| 02 | `02-drawing-primitives-and-grid.md` | `draw_rect`; colour packing with `GAME_RGB`; pixel-index math; draw grid border |
| 03 | `03-input.md` | `GameButtonState`; `UPDATE_BUTTON`; double-buffered `GameInput inputs[2]` |
| 04 | `04-game-structure.md` | Split into `game.h` / `game.c`; `GameState`; init/update/render stubs |
| 05 | `05-snake-data-and-ring-buffer.md` | `Segment[MAX_SNAKE]` ring buffer; `head` / `tail` / `length` wrap arithmetic |
| 06 | `06-snake-movement.md` | Delta-time accumulator; `SNAKE_DIR` enum; `DX`/`DY`; `move_interval` |
| 07 | `07-turn-input.md` | `next_direction` staging; modular turn formula; 180° U-turn guard |
| 08 | `08-food-spawning.md` | `snake_spawn_food`; retry loop; `srand`; eat → `grow_pending` → score |
| 09 | `09-collision-detection.md` | Wall + self collision; `game_over`; `draw_text` + 5×7 bitmap font |
| 10 | `10-restart-and-speed-scaling.md` | R restarts; `best_score` preserved; speed decay; `HEADER_ROWS` panel |
| 11 | `11-audio.md` | `SoundInstance` phase accumulator; food-eaten + game-over SFX; volume ramping |
| 12 | `12-polish-and-utils-refactor.md` | `draw_rect_blend` overlay; refactor draw utils into `utils/`; final game |

---

## Architecture

```
game.c  ──▶  SnakeBackbuffer (CPU RAM)  ──▶  GPU texture  ──▶  screen
audio.c ──▶  AudioOutputBuffer          ──▶  ALSA / Raylib ──▶  speakers
```

`game.c` and `audio.c` never call X11 or Raylib. The platform backends
(`main_x11.c`, `main_raylib.c`) are thin translators: OS events → `GameInput`,
OS timer → `delta_time`, CPU pixels → GPU texture, CPU audio → OS audio.

---

## What you'll have achieved by the end

- A **complete, playable Snake game** compiled from first principles
- Deep understanding of the **CPU backbuffer → GPU texture pipeline**
- Working mental model of **ring buffers** (no `memmove`, no linked list)
- Familiarity with the **platform-independence pattern** from Handmade Hero
- Experience with **delta-time accumulation**, enums, fixed arrays, manual alpha blending
- Introduction to **procedural audio** (phase accumulators, volume ramping, click prevention)
- A clear map from JS patterns (`requestAnimationFrame`, `Array`, event listeners) to C equivalents
- Foundation for the companion **Tetris course**, which extends this with a full MIDI sequencer

---

## Companion course

**Tetris** — extends this architecture with:
- Full MIDI music sequencer in `audio.c`
- More complex piece-rotation game logic
- `COURSE-BUILDER-IMPROVEMENTS.md` comparing both courses

Located at: `ai-llm-knowledge-dump/generated-courses/javidx9/tetris/`
