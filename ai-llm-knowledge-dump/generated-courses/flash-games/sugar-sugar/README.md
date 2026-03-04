# Sugar, Sugar — C Game Course

A hands-on course that re-implements the classic Newgrounds puzzle game
**Sugar, Sugar** (Bart Bonte, 2009) in C, using the same CPU back-buffer +
platform-abstraction pattern established in the Tetris, Snake, Asteroids, and
Frogger courses.

---

## What you will build

| Feature | What it teaches |
|---------|----------------|
| 640 × 480 pixel canvas (both X11 & Raylib) | Letterbox layout, pixel-format, pitch |
| Falling-sand cellular automaton | Euler integration, dt-capping, sub-step physics |
| Struct-of-Arrays grain pool | Cache-friendly SoA vs. AoS performance discussion |
| `LineBitmap` wall system | Single-byte per-pixel encoding, bounds-safe `IS_SOLID` macro |
| Mouse brush with Bresenham fill | Mouse input model, delta-drag, half-transition counts |
| Color filters, cups, obstacles | Data-driven level system, designated initialisers |
| Teleporters + gravity flip | Entity cooldown, sign-toggle gravity, level variety |
| 8 × 8 bitmap font HUD | Bit-test font rendering, on-screen counters |
| Real Raylib audio | `LoadSound` / `PlaySound`, `LoadMusicStream`, sound IDs |

---

## Prerequisites

- Completed the **Asteroids** course (or equivalent familiarity with the
  `platform.h` contract, `GAME_PHASE` state machine, `GameButtonState`)
- C99 compiler (`gcc` / `clang`)
- Raylib 4.x installed (for the Raylib backend); X11 dev libraries for the X11 backend

---

## Build instructions

```bash
cd course

# Debug build — X11
./build-dev.sh --backend=x11 -d

# Debug build — Raylib
./build-dev.sh --backend=raylib -d

# Release build
./build-dev.sh --backend=x11 -r
./build-dev.sh --backend=raylib -r
```

---

## Lesson list

| # | Title | Key concept |
|---|-------|-------------|
| 01 | Window and canvas | `GameBackbuffer`, `pitch`, letterbox, dual backends |
| 02 | Pixel rendering | `GAME_RGB`, `draw_rect`, `draw_circle` |
| 03 | Mouse input and drawing | `MouseInput`, `prepare_input_frame`, Bresenham |
| 04 | One falling grain | Euler integration, dt cap, **Y-down exception** |
| 05 | Grain pool | SoA layout, high-watermark `count`, spawning |
| 06 | Grain collision | `LineBitmap`, `IS_SOLID`, occupancy map |
| 07 | Slide, settle, bake | Angle of repose, `GRAIN_SETTLE_FRAMES`, baking |
| 08 | State machine | `GAME_PHASE`, `change_phase()` |
| 09 | Level system | `LevelDef`, designated initialisers, `level_load()` |
| 10 | Win detection | `check_win()`, cup fill counter |
| 11 | Color filters | `GRAIN_COLOR`, per-grain color byte, filter zones |
| 12 | Obstacles and level variety | Obstacle baking, levels 1–15 |
| 13 | Font system and HUD | 8×8 bitmap font, `draw_text`, bit-test rendering |
| 14 | Teleporters and gravity flip | Cooldown byte, `gravity_sign` toggle |
| 15 | Audio and final polish | `platform_play_sound`, Raylib `LoadSound`/`PlaySound` |

---

## Architecture overview

```
     main_x11.c          main_raylib.c
         │                    │
         └──────┬─────────────┘
                │  platform.h contract
                │  (4 mandatory functions)
              game.c ──── levels.c
                │
              game.h (all types)
              font.h  sounds.h
```

The **game layer** (`game.h` / `game.c` / `levels.c`) has zero platform
dependencies.  Both backends compile the same game code unchanged.

---

## Key design decisions

- **Y-down pixels, not Y-up meters** — falling-sand physics maps grain position
  directly to screen pixel coordinates.  No unit conversion needed.
- **SoA GrainPool** — keeping `x[]`, `y[]`, `vx[]`, `vy[]` as separate flat
  arrays maximises cache locality when iterating all 4 096 grains.
- **`LineBitmap` single-byte encoding** — `0` = empty, `1` = wall, `255` =
  player-drawn line, `2–5` = baked grain color.  One array serves drawing,
  collision, and rendering.
- **Audio via `change_phase()`** — all sound triggers live inside one function,
  mirroring the pattern from Asteroids and Frogger.
