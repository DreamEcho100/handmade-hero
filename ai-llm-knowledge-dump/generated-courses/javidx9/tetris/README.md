# Tetris in C — Build-Along Course
### Inspired by javidx9 · Built the Handmade Hero way

> A step-by-step guide for JavaScript developers who want to build a complete
> Tetris clone in C from an empty file — no game engines, no frameworks, just
> the CPU, the OS, and you.

---

## What you will build

A fully playable, audible Tetris clone with:

- A custom software-rendered backbuffer pipeline (no SDL draw calls)
- Stereo sound effects with spatial panning
- A MIDI-driven background music sequencer
- Two independent platform backends: **Raylib** (Windows / macOS / Linux) and
  **X11 + ALSA** (Linux native)

All game logic compiles to the same binary on either backend without changing a
single line of `game.c`.

---

## Prerequisites

- You can write and run a C "Hello, World" program with `clang`
- You understand what a pointer is (roughly — the course explains it again)
- You are comfortable with JavaScript / TypeScript at an intermediate level
- Linux or macOS recommended; Windows works with Raylib

**No prior C game-programming experience required.**

---

## How to build and run

### Raylib backend (recommended for beginners)

```bash
# Install raylib first (Debian/Ubuntu):
sudo apt install libraylib-dev

# Build
./build-dev.sh --backend=raylib

# Build and run immediately
./build-dev.sh --backend=raylib -r
```

### X11 / ALSA backend (Linux only)

```bash
# Install dependencies:
sudo apt install libx11-dev libxkbcommon-dev libgl-dev libasound2-dev

# Build
./build-dev.sh --backend=x11

# Build and run
./build-dev.sh --backend=x11 -r
```

---

## Lessons

| # | Title | What you will see / hear |
|---|-------|--------------------------|
| [01](course/lessons/01-window-and-backbuffer.md) | Window & Backbuffer | A solid-coloured window at the game resolution |
| [02](course/lessons/02-drawing-primitives.md) | Drawing Primitives | Coloured rectangles; one semi-transparent overlay |
| [03](course/lessons/03-input.md) | Input | A square that moves with keyboard input |
| [04](course/lessons/04-game-structure.md) | Game Structure | Same square, code split into 3-layer architecture |
| [05](course/lessons/05-tetromino-data.md) | Tetromino Data | A single tetromino drawn in the centre of the field |
| [06](course/lessons/06-tetromino-movement.md) | Tetromino Movement | Fully controllable, collision-checked tetromino |
| [07](course/lessons/07-the-playfield.md) | The Playfield | Pieces stack on walls; game never crashes on land |
| [08](course/lessons/08-line-clearing.md) | Line Clearing | Completed rows flash white and vanish |
| [09](course/lessons/09-scoring-and-levels.md) | Scoring & Levels | Score and level update; speed visibly increases |
| [10](course/lessons/10-ui-sidebar-and-text.md) | UI — Sidebar & Text | Full HUD: score, level, piece count, next piece |
| [11](course/lessons/11-audio-sound-effects.md) | Audio — Sound Effects | Sound on every game event; stereo panning |
| [12](course/lessons/12-audio-background-music.md) | Audio — Background Music | Korobeiniki theme loops in the background |
| [13](course/lessons/13-polish-ghost-and-game-over.md) | Polish — Ghost & Game Over | Ghost piece; GAME OVER overlay; clean restart |

---

## What you will have achieved by the end

- **Written a complete game in C** — ~800 lines of game logic, zero external
  game-engine dependencies
- **Built a software renderer** — you understand pixel buffers, pitch, RGBA
  packing, and alpha blending at the byte level
- **Built a low-latency audio engine** — phase accumulators, PCM samples,
  stereo panning, and Casey's latency model
- **Understood the Handmade Hero architecture** — platform layer / game layer
  separation, double-buffered input, flat-array state, delta-time loops
- **Connected every C concept to its JavaScript equivalent** — pointers,
  structs, enums, unions, `static`, `extern`, `memset`, `#define` macros

---

## Controls

| Key | Action |
|-----|--------|
| ← / A | Move left |
| → / D | Move right |
| ↓ / S | Soft drop |
| Z | Rotate counter-clockwise |
| X | Rotate clockwise |
| R | Restart (after game over) |
| Q / Escape | Quit |

---

## Architecture overview

```
main_raylib.c  /  main_x11.c   ← platform layer
        │  platform_init()
        │  game loop:
        │    prepare_input_frame()
        │    platform_get_input()  → GameInput
        │    game_update(state, input, dt)
        │    game_render(backbuffer, state)
        │    upload backbuffer pixels to GPU
        │  platform_shutdown()
        ▼
     game.c  /  game.h          ← game layer (pure C, no platform APIs)
        │  game_init()
        │  game_update()  ← all game logic lives here
        │  game_render()  ← writes pixels to Backbuffer
        ▼
  utils/draw-shapes.c            ← pixel-level drawing primitives
  utils/draw-text.c              ← 5×7 bitmap font renderer
  audio.c                        ← PCM synthesis, sequencer
```
