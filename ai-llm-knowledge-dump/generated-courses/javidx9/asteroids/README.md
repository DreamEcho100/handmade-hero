# Asteroids — Build-Along Course

A step-by-step C course building the classic Asteroids arcade game from scratch, targeting Linux
with both X11/OpenGL and Raylib backends.

Based on [javidx9 / OneLoneCoder](https://github.com/OneLoneCoder) — *Code-It-Yourself! Asteroids*
(original C++ / Windows console). Ported to C with pixel-buffer rendering, a proper platform
abstraction layer, and added audio. All code is educational and original C++ is used for study only.

---

## What you will build

A wireframe Asteroids game where:

- A triangular ship rotates and thrusts with Newtonian physics
- Bullets fire in the ship's facing direction (one shot per key-press)
- Asteroids drift and tumble; shooting splits them large→medium→small→gone
- All objects wrap toroidally (exit right, reappear left — same top/bottom)
- Explosions play a stereo-panned sound effect based on screen position
- Score accumulates; clearing all asteroids advances the wave
- A bitmap-font HUD shows SCORE in the top-left corner

---

## Prerequisites

- Completed the **Snake course** (or equivalent): you should be comfortable with the
  backbuffer pipeline, `GameButtonState` input model, `GAME_PHASE` enum, and `DEBUG_TRAP`
- C compiler: `clang` (or `gcc`)
- Linux with either:
  - **X11 backend**: `sudo apt install libx11-dev libxkbcommon-dev libasound2-dev`
  - **Raylib backend**: `sudo apt install libraylib-dev`
- OpenGL/GLX is provided by your GPU driver or mesa

---

## How to build and run

```sh
# From the course/ directory:

# Raylib backend (default) — build only
./build-dev.sh

# Raylib backend — build and run immediately
./build-dev.sh -r

# X11 backend — build and run
./build-dev.sh --backend=x11 -r

# Debug build with AddressSanitizer + UBSan
./build-dev.sh -d -r
```

The binary is placed in `build/asteroids`.

---

## Controls

| Key | Action |
|-----|--------|
| `←` / `A` | Rotate counter-clockwise |
| `→` / `D` | Rotate clockwise |
| `↑` / `W` | Thrust forward (hold for continuous) |
| `Space` | Fire bullet (one shot per press — holding does not rapid-fire) |
| `Q` / `Esc` | Quit |

---

## Lesson list

| # | Title | One-line summary |
|---|-------|-----------------|
| 01 | Window + backbuffer | Open a black window; meet `AsteroidsBackbuffer`, `pitch`, `build-dev.sh`, `DEBUG_TRAP` |
| 02 | Drawing primitives | `draw_rect`, `draw_rect_blend`; validate `GAME_RGBA` pixel format on both backends |
| 03 | Line drawing (Bresenham) | Integer line algorithm; `draw_pixel_w` toroidal wrapper; triangle test |
| 04 | Input | `GameButtonState`, double-buffer `inputs[2]`, `GameInput` union with 4 buttons |
| 05 | Game structure | `GameState`, `GAME_PHASE` enum, `asteroids_init`/`update`/`render` skeleton |
| 06 | Vec2 + rotation matrix | `Vec2`, `draw_wireframe`, `cos`/`sin` once per object, `srand(time(NULL))` |
| 07 | Ship physics + toroidal wrap | `vx += sin(angle)*THRUST*dt`; `draw_pixel_w` wraps lines seamlessly at edges |
| 08 | Asteroids entity pool | `SpaceObject[MAX_ASTEROIDS]`, `compact_pool` swap-with-last O(1) removal |
| 09 | Bullets | `bullets[MAX_BULLETS]`, fire "just pressed", timer expiry, `compact_pool` |
| 10 | Collision detection | `dx²+dy²<r²` (no sqrt), bullet-asteroid hit+split, player death, wave clear |
| 11 | Font + UI | `FONT_8X8[128][8]` BIT7-left, `draw_text`, `snprintf`, score + death overlay |
| 12 | Audio | Thrust/fire/explode/death SFX; spatial pan by `asteroid.x`; ALSA + Raylib audio |
| 13 | Polish + `utils/` refactor | Refactor to `utils/`; wave scaling; high score; `COURSE-BUILDER-IMPROVEMENTS.md` |

---

## What you will have achieved by the end

- A fully playable Asteroids game running natively on Linux (both X11 and Raylib)
- Implemented from scratch: Bresenham's line drawing, 2D rotation matrix, Euler physics,
  toroidal wrapping, entity pool management, circle collision, bitmap font rendering,
  procedural audio with stereo panning
- Understood the Handmade Hero platform-abstraction architecture: game logic never touches
  X11, Raylib, or any OS API — the platform layer is a thin translator
- Written code that passes `-Wall -Wextra` with zero warnings on both backends
