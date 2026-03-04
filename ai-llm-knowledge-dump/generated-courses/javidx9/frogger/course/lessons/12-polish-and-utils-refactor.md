# Lesson 12: Polish + utils/ Refactor

## What we're building

This final lesson has two goals:

1. **Polish** — add the respawn flicker effect, game-over overlay, win overlay,
   and restart-on-any-key flow to complete the playable game.
2. **`utils/` refactor** — move drawing, text, and math helpers out of `game.c`
   into dedicated `utils/` files.  This mirrors the architecture students saw
   in the Tetris/Snake/Asteroids courses and teaches how to split a multi-file
   C project.

By the end of this lesson the game is fully playable and the code is organized
the way a real project would be.

## What you'll learn

- Multi-file C project structure: header/source pairs, `#include` guards
- `static` in C — local to translation unit (not a class method)
- `extern` declarations vs definitions
- Death flash flicker via `(int)(dead_timer * 6) % 2`
- Semi-transparent overlays with `draw_rect_blend`
- The `utils/` split rationale: separation of concerns

## Prerequisites

Lessons 01–11 complete.  The complete game already exists in `game.c` —
this lesson explains its final structure.

---

## Step 1 — The utils/ split

> **Course Note (gap #12):** The reference Frogger source puts all drawing
> utilities (including the bitmap font) directly in `frogger.c` — one large
> file.  The course refactors these into `utils/` to teach the same
> architecture students encounter in Tetris:
>
> | Reference | Course |
> |-----------|--------|
> | `frogger.c` (all-in-one) | `game.c` + `utils/draw-shapes.c` + `utils/draw-text.c` + `utils/math.h` |
>
> The refactor is **already done** in the course source (Phase 2).  This
> lesson explains *why* the split is architecturally important.

**The four utils files:**

```
utils/
├── backbuffer.h      — Backbuffer type, GAME_RGBA macro, screen constants
├── math.h            — MIN, MAX, CLAMP, ABS macros (header-only)
├── audio.h           — SOUND_ID enum, SoundInstance, GameAudioState types
├── draw-shapes.h     — draw_rect, draw_rect_blend, draw_sprite_partial decls
├── draw-shapes.c     — implementation of the three draw functions
├── draw-text.h       — GLYPH_W/H, draw_char, draw_text declarations
└── draw-text.c       — FONT_8X8[128][8] data + render functions
```

**JS analogy:**

```js
// Before refactor: everything in game.js
// After refactor:
import { drawRect, drawSprite } from './utils/drawShapes.js';
import { drawText }             from './utils/drawText.js';
import { clamp, min, max }      from './utils/math.js';
```

---

## Step 2 — Multi-file C: the rules

**Rule 1: Every symbol is defined exactly once.**
`draw_rect` is *declared* in `draw-shapes.h` (`.h`) and *defined* in
`draw-shapes.c` (`.c`).  Any `.c` that calls `draw_rect` includes the header.

```c
/* draw-shapes.h */
#ifndef FROGGER_DRAW_SHAPES_H
#define FROGGER_DRAW_SHAPES_H
#include "backbuffer.h"
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);
#endif

/* draw-shapes.c */
#include "draw-shapes.h"
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    /* ... */
}
```

**Rule 2: `static` limits visibility to the current translation unit.**
Helper functions like `generate_wave()` in `audio.c` are declared `static` —
they're only callable from within `audio.c`.

```c
static float generate_wave(float phase, int wave_type) { … }
```

**JS analogy:** `static` in C ≈ an unexported function in an ES module.

**Rule 3: Include guards prevent double inclusion.**
Every `.h` file starts with `#ifndef SYMBOL / #define SYMBOL` and ends with
`#endif`.  Without guards, including the same header twice causes
"redefinition" errors.

---

## Step 3 — Respawn flicker

A death flash is created by toggling the frog's visibility during `PHASE_DEAD`:

```c
/* In game_render */
int draw_frog = 1;
if (state->phase == PHASE_DEAD) {
    /* Blink at 6 Hz: visible when timer × 6 is even */
    draw_frog = ((int)(state->dead_timer * 6.0f) % 2 == 0);
}

if (draw_frog) {
    /* draw frog sprite at frog_x * TILE_PX, frog_y * TILE_PX */
    draw_sprite_partial(&bb, …);
}
```

The `dead_timer` counts down from 1.2 s.  At 6 Hz × 1.2 s = 7.2 flashes —
just enough to see the frog die and understand it needs to respawn.

---

## Step 4 — Semi-transparent overlays

Both the win screen and game-over screen use `draw_rect_blend` to darken
the game behind the overlay:

```c
/* Win overlay */
if (state->phase == PHASE_WIN) {
    draw_rect_blend(bb, 0, 0, SCREEN_W, SCREEN_H, COLOR_BLACK, 160);
    draw_text(bb, "YOU WIN!",          SCREEN_W/2 - 32, SCREEN_H/2 - 16, COLOR_YELLOW);
    draw_text(bb, "PRESS R TO PLAY AGAIN", SCREEN_W/2 - 88, SCREEN_H/2 + 8, COLOR_WHITE);
}

/* Game-over overlay (PHASE_DEAD with lives == 0) */
if (state->phase == PHASE_DEAD && state->lives <= 0) {
    draw_rect_blend(bb, 0, 0, SCREEN_W, SCREEN_H, COLOR_BLACK, 160);
    draw_text(bb, "GAME OVER",         SCREEN_W/2 - 36, SCREEN_H/2 - 16, COLOR_RED);
    draw_text(bb, "PRESS R TO RETRY",  SCREEN_W/2 - 64, SCREEN_H/2 + 8, COLOR_WHITE);
}
```

`draw_rect_blend(bb, x, y, w, h, color, alpha)` — the `alpha` parameter
controls how much of the existing pixels shows through.  At `alpha = 160`
(63% of 255) the game is clearly dimmed but still visible behind the overlay.

---

## Step 5 — Gaps addressed across this course

This lesson closes gap #12.  Here is the full list of all 15 gaps addressed:

| # | Gap | Lesson |
|---|-----|--------|
| 1  | Two build scripts → unified `build-dev.sh` | 01 |
| 2  | No `pitch` in backbuffer | 01 |
| 3  | No `DEBUG_TRAP`/`ASSERT` | 01 |
| 4  | `platform.h` only 3 functions → added `platform_shutdown` | 01 |
| 5  | `FROGGER_RGBA` = `0xAABBGGRR` → `GAME_RGBA` = `0xAARRGGBB` | 02 |
| 6  | No named `COLOR_*` constants | 02 |
| 7  | Single `GameInput` buffer → `inputs[2]` double-buffer | 03 |
| 8  | No `GameInput` union | 03 |
| 9  | Raw `platform_init(title, w, h)` → `PlatformGameProps` | 01 |
| 10 | Fixed non-resizable window → letterbox from Lesson 01 | 01 |
| 11 | Bare `fprintf` debug calls → guarded by `#ifdef DEBUG` | 01 |
| 12 | All draw code inline in `frogger.c` → `utils/` split | 12 |
| 13 | 5×7 column-major font → `FONT_8X8[128][8]` BIT7-left | 10 |
| 14 | No audio → `audio.c` + `utils/audio.h` | 11 |
| 15 | `frogger_init/tick/render` naming → `game_init/update/render` | 04 |

For the rationale behind each decision, see
`COURSE-BUILDER-IMPROVEMENTS.md` (Phase 4 output).

---

## Build and run — final game

```bash
# X11 backend
./build-dev.sh --backend=x11 -r

# Raylib backend
./build-dev.sh --backend=raylib -r

# Debug build (AddressSanitizer)
./build-dev.sh --backend=x11 --debug -r
```

---

## Expected result

The complete Frogger game:

- 10 scrolling lanes (road + river + safe zones)
- Frog moves one tile per keypress (up/down/left/right + WASD)
- Death flash + respawn on unsafe tiles (3 lives)
- Five homes to fill; win screen at all five
- HUD: score, lives, homes, best score
- Spatial audio on all four SFX events
- Resizable window with letterbox centering

## Exercises

1. Add a high-score that persists to a file using `fopen(path, "w")` and
   `fprintf(f, "%d\n", best_score)` on quit, then `fscanf` on load.
2. Add an animated frog sprite (two frames, alternating every 0.1 s).
3. Port the game to a third backend — read `platform.h` and implement
   `platform_init/get_time/get_input/shutdown` using SDL3.

## What's next

You've completed the Frogger course.  You now know:

- CPU backbuffer pipeline (pixels → texture → GPU)
- Procedural audio synthesis (no WAV files)
- Data-Oriented Design for lane data
- `lane_scroll()` floor-mod for bi-directional scrolling
- Danger buffer collision detection
- Binary file loading without dynamic allocation
- Multi-file C project organisation

The next game in the series builds on all of these foundations.
