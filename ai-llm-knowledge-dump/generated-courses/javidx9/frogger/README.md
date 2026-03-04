# Frogger — C Game Development Course

> A hands-on course for JavaScript/TypeScript developers learning C through game programming.  
> Port of OneLoneCoder's (javidx9) Frogger from C++/Windows to C99/Linux.

---

## What is this course?

You'll build a complete, playable Frogger game in C99 that runs on both X11/OpenGL and Raylib. By the end you will have written every line: the pixel renderer, input system, scrolling lane engine, collision grid, sprite loader, bitmap font, and a stereo-panned audio system — all without a game engine.

The course follows the same architecture used in the **Tetris**, **Snake**, and **Asteroids** courses (CPU backbuffer pipeline, thin platform layer, Data-Oriented Design) and introduces three techniques new to Frogger:

| Technique                    | What it teaches                                         |
|------------------------------|---------------------------------------------------------|
| `lane_scroll()` floor-mod    | Pixel-accurate bidirectional scrolling without C truncation bugs |
| Danger buffer                | Flat 2D collision grid rebuilt every frame              |
| `draw_sprite_partial` UV crop | Rendering a sub-region of a sprite sheet                |

---

## Prerequisites

| Requirement        | Notes                                                                 |
|--------------------|-----------------------------------------------------------------------|
| **Snake course**   | Required — assumes familiarity with the CPU backbuffer, `GameButtonState`, and delta-time loop |
| **Asteroids course** | Recommended — introduces sprites and audio; Frogger builds on both  |
| C99 compiler       | `clang` recommended; `gcc` works                                      |
| Linux              | X11 + OpenGL or Raylib; both backends build on Ubuntu / Debian        |

Install dependencies:

```bash
# X11 backend
sudo apt install clang libx11-dev libxkbcommon-dev libgl-dev

# Raylib backend
sudo apt install clang libraylib-dev
```

---

## Build and run

```bash
cd course/

# X11 backend (debug)
./build-dev.sh --backend=x11

# X11 backend (release)
./build-dev.sh --backend=x11 -r

# Raylib backend (debug)
./build-dev.sh --backend=raylib

# Raylib backend (release)
./build-dev.sh --backend=raylib -r
```

Then run from the `course/` directory so the `assets/` path resolves:

```bash
./build/frogger_x11       # or ./build/frogger_raylib
```

---

## Lessons

| #  | Title                              | Summary                                                             |
|----|------------------------------------|---------------------------------------------------------------------|
| 01 | Window + Backbuffer                | Open a resizable window; allocate a CPU pixel buffer with `pitch`; unified build script; letterbox scaling |
| 02 | Drawing Primitives                 | `draw_rect`, `draw_rect_blend`; `GAME_RGBA` pixel format; named color constants; cross-backend validation |
| 03 | Input                              | `GameButtonState`; double-buffered `inputs[2]`; `GameInput` union; X11 auto-repeat suppression |
| 04 | Game Structure + `GAME_PHASE`      | `GameState`; `game_init/update/render` split; `GAME_PHASE` enum state machine |
| 05 | Lane Data (DOD)                    | `lane_speeds[]` + `lane_patterns[][]` as separate arrays; Data-Oriented Design rationale |
| 06 | Scrolling Lanes (`lane_scroll`)    | Pixel-accurate floor-mod scrolling; bidirectional lanes; sub-tile smoothness |
| 07 | Danger Buffer + Frog Collision     | Flat 2D `danger[]` bitmask; per-frame rebuild; frog death and respawn |
| 08 | Sprites (`SpriteBank`)             | `.spr` binary file loader; `SpriteBank` fixed pool; `draw_sprite_partial` UV crop |
| 09 | Homes + Win Condition              | Five home cells; `homes_reached` counter; `PHASE_WIN` trigger; win overlay |
| 10 | Font + UI                          | `FONT_8X8[128][8]` BIT7-left bitmap font; `draw_text`; `snprintf` HUD |
| 11 | Audio                              | Hop / death / home-reached SFX; stereo pan by `frog_x`; ALSA + Raylib buffer model |
| 12 | Polish + `utils/` Refactor         | Refactor draw code to `utils/`; respawn flicker; game-over overlay; restart key |

---

## What you will have built by the end

- A complete, playable Frogger running natively on Linux at 60 fps
- A CPU pixel renderer that works identically on two completely different graphics backends
- A scrolling tile engine with correct floor-mod arithmetic for bidirectional lanes
- A collision system where the grid is provably consistent with what is drawn on screen
- A sprite system that loads binary `.spr` files and renders UV-cropped tiles
- A stereo-panned audio system driven entirely by synthesised PCM data
- A `utils/` module split matching the architecture used across all courses

---

## Relationship to other courses

```
Tetris  ─┐
Snake   ─┤── same backbuffer + platform pattern
Asteroids─┤── adds sprites + audio
Frogger ─┘── adds lane_scroll + danger buffer + spatial audio
```

Each course is self-contained but shares the same `game.h`/`platform.h`/`audio.c` architecture. Building all four gives you a reusable mental model for any 2D game in C.
