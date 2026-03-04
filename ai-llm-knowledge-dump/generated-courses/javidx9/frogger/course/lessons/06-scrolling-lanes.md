# Lesson 06: Scrolling Lanes (`lane_scroll`)

## What we're building

The colour-coded tiles from Lesson 05 now **scroll**.  River lanes drift
left or right; road lanes carry vehicles in both directions.  The frog rides
the current on log tiles and drifts sideways even without pressing a key.

Most importantly, the scrolling is **sub-tile smooth** — tiles move one pixel
at a time, not 64 pixels at a time.

---

## What you'll learn

- Why the naïve `(int)(time * speed) % PATTERN_LEN` approach produces 64-pixel
  jumps instead of smooth scrolling
- Why C's `(int)` truncates toward zero (not toward −∞) and why that breaks
  negative-speed lanes
- The `lane_scroll()` floor-mod fix: work in pixels, not tiles; add
  `PATTERN_PX` when the result is negative
- How `tile_start` and `px_offset` work together to position tiles
- River drift: the frog's `frog_x` changes at `lane_speeds[fy]` rate when
  standing on a river row
- Why `game_update` and `game_render` MUST call `lane_scroll` with the same
  `state->time` (the consistency invariant)

---

## Prerequisites

- Lessons 01–05: window, drawing, input, game structure, lane data

---

## Concepts

### 1. The Naïve Approach and Why It Fails

**Attempt 1: tile-index scroll**

```c
// Naive: scroll by changing which tile index we start rendering from
int tile_start = (int)(time * speed) % LANE_PATTERN_LEN;
int px_offset  = 0;  // ← sub-tile offset completely missing
```

Problem A: `px_offset` is always 0, so tiles snap to 64-pixel boundaries.
With `speed=3` and 60 fps, `time * speed` increments by `3/60 = 0.05` per
frame — well below 1 tile.  The tile index stays the same for ~20 frames, then
jumps by 1 tile = 64 pixels.  **This is the 64-pixel jump bug.**

```js
// JS analogy — same problem:
// ctx.translate(-(Math.floor(time * speed) % 64) * 64, 0);
// ↑ "snaps" to multiples of 64 pixels
```

**Attempt 2: fractional tile scroll**

```c
// Fractional tile index — sounds right but has a different problem:
float tile_f  = time * speed;
int tile_start = (int)tile_f % LANE_PATTERN_LEN;   // ← (int) truncates toward 0
int px_offset  = (int)((tile_f - (int)tile_f) * TILE_PX);
```

Problem B: C's `(int)` cast **truncates toward zero**, not toward −∞.
For positive numbers this is the same as `floor()`.
For **negative** numbers they diverge:

```c
(int)(-4.5f)   = -4   // truncates toward 0 (wrong for scroll)
floorf(-4.5f)  = -5   // rounds toward -infinity (correct for scroll)
```

When `speed = -3` and `time` is such that `time * speed = -4.5`:
- Correct tile: `floor(-4.5) = -5` → pattern position 59 (wrapping)
- Actual C cast: `(int)(-4.5) = -4` → pattern position 60

The `tile_start` jumps one tile too far when the fractional part crosses 0,
producing a spurious one-tile snap.  **This is the C-truncation bug.**

### 2. The Fix: Work Entirely in Pixels

> **New technique: `lane_scroll()` — floor-mod in pixel space**
>
> **What:** Compute the scroll offset in pixels, not in tiles.  Use a
> positive-modulo clamp so the result is always in `[0, PATTERN_PX)`.
>
> **Why:** By working in pixel integers after the float multiply, we avoid
> the `(int)` truncation trap.  `sc < 0 ? sc += PATTERN_PX` ensures the
> value is positive before any division, so `tile_start` and `px_offset`
> are always correct for both positive and negative speeds.
>
> **JS equivalent:**
> ```js
> // Same positive-mod trick needed in JS:
> function laneScroll(time, speed, tilePx, patternLen) {
>   const patternPx = patternLen * tilePx;
>   let sc = Math.floor(time * speed * tilePx) % patternPx;
>   if (sc < 0) sc += patternPx;
>   return { tileStart: (sc / tilePx)|0, pxOffset: sc % tilePx };
> }
> ```
>
> **C idiom:**
> ```c
> static inline void lane_scroll(float time, float speed,
>                                 int *out_tile_start, int *out_px_offset) {
>     const int PATTERN_PX = LANE_PATTERN_LEN * TILE_PX;  /* 64 * 64 = 4096 */
>     int sc = (int)(time * speed * (float)TILE_PX) % PATTERN_PX;
>     if (sc < 0) sc += PATTERN_PX;     /* always [0, PATTERN_PX) */
>     *out_tile_start = sc / TILE_PX;
>     *out_px_offset  = sc % TILE_PX;   /* 0..63 — sub-tile smooth */
> }
> ```
>
> **Gotcha:** The `if (sc < 0) sc += PATTERN_PX` line is critical.
> C's `%` operator on negative integers preserves the sign — `-255 % 4096 = -255`,
> not `+3841`.  Without the clamp, `tile_start` would be negative and would
> index into invalid (or wrapped) pattern positions.

### 3. Concrete Numerical Example

Let's trace through both the broken and fixed approaches for:
- `speed = -3.0f` (lane 1, moving left)
- `time = 1.33s`
- `TILE_PX = 64`
- `LANE_PATTERN_LEN = 64`
- `PATTERN_PX = 64 * 64 = 4096`

**Broken (tile-index approach):**
```
tile_f = 1.33 * -3.0 = -3.99
(int)(-3.99) = -3   ← truncates toward 0, not toward -∞
tile_start = -3 % 64 = -3   ← negative, invalid!
px_offset  = 0   ← no sub-tile offset
```

**Broken (fractional approach with (int) cast):**
```
tile_f = -3.99
(int)(-3.99) = -3   ← should be -4 (floor)
tile_start = -3 % 64 = -3   ← wrong tile, negative
px_offset  = (-3.99 - (-3)) * 64 = -0.99 * 64 = -63.36 → (int) = -63   ← negative offset!
```

**Fixed (lane_scroll, pixel arithmetic):**
```
raw = (int)(1.33 * -3.0 * 64.0) = (int)(-255.36) = -255
sc  = -255 % 4096 = -255   ← C's % on negative value
sc += 4096 (because sc < 0) → sc = 3841   ← now in [0, 4096)
tile_start = 3841 / 64 = 60
px_offset  = 3841 % 64 = 1   ← 1 pixel offset: sub-tile smooth!
```

Result: tile 60 is drawn starting at pixel offset 1.  At the next frame
(`time = 1.350s`), `sc` would be 3856 → `px_offset = 16`.  The tile moves
exactly `15 pixels` in `0.02s` at `speed = 3` — smooth, continuous motion.

### 4. Rendering with `tile_start` and `px_offset`

```c
int tile_start, px_offset;
lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
int dest_py = y * TILE_PX;

for (i = 0; i < LANE_WIDTH; i++) {
    char c = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];

    /* -1 + i: start one tile to the LEFT of the screen edge
       This ensures no gap appears on the left when px_offset > 0.      */
    int dest_px = (-1 + i) * TILE_PX - px_offset;

    draw_rect(bb, dest_px, dest_py, TILE_PX, TILE_PX, tile_debug_color(c));
}
```

The `(-1 + i)` trick: with `LANE_WIDTH = 18`, we draw tiles from position
`-1` to `+16`.  Tile at position `-1` starts at `dest_px = -TILE_PX - px_offset`
— always off screen to the left.  As `px_offset` increases from 0 to 63, tiles
slide rightward by up to 63 pixels.  When `px_offset` wraps back to 0,
`tile_start` advances by 1 — the pattern advances seamlessly.

### 5. River Drift

When the frog is on a river row (rows 1–3), it rides the current — `frog_x`
drifts at the lane's speed:

```c
int fy = (int)state->frog_y;
if (fy >= 1 && fy <= 3)
    state->frog_x -= lane_speeds[fy] * dt;
```

```js
// JS equivalent:
// if (frogRow >= 1 && frogRow <= 3)
//   frogX -= laneSpeeds[frogRow] * dt;
```

Note the **minus sign**: `lane_speeds[fy]` is the speed the tiles scroll.
A positive speed means tiles move rightward — the frog standing on those tiles
would drift rightward — so we subtract to move the frog opposite the tile
scroll direction.  (Think of it as: the frog is carried along with the log,
which moves in the direction opposite to the scroll.)

### 6. The Consistency Invariant

`game_update` uses `lane_scroll` to build the danger buffer (which cells are
safe to stand on).  `game_render` uses `lane_scroll` to draw the tiles.

**Both calls must pass the same `state->time`.**

If `game_update` uses `time = T` and `game_render` uses `time = T + dt`,
the danger buffer and the visual are offset by one frame's worth of scroll.
The frog dies on a car tile that visually appears to have passed already.
This is a ghost-collision bug — nearly impossible to debug.

The fix is architectural: `state->time` is incremented once in `game_update`
and used by both functions.  `game_render` never advances time.

```c
// game_update: accumulates time
state->time += dt;
lane_scroll(state->time, speed, &tile_start, &px_offset);  // builds danger[]

// game_render: reads the same time
lane_scroll(state->time, speed, &tile_start, &px_offset);  // draws tiles
// Both see identical tile_start and px_offset — guaranteed consistent.
```

---

## Step 1 — Updated `src/game.h` (Lesson 06 stage)

Add `lane_scroll` as a `static inline` function so both `game_update` and
`game_render` can call it without a separate compilation unit:

```c
/* game.h — Frogger Types (Lesson 06: lane_scroll) */
#ifndef FROGGER_GAME_H
#define FROGGER_GAME_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utils/backbuffer.h"

/* ── Debug ──────────────────────────────────────────────────────────── */
#ifdef DEBUG
  #define DEBUG_TRAP()  __builtin_trap()
  #define ASSERT(cond, msg) \
      do { if (!(cond)) { \
          fprintf(stderr, "ASSERT FAILED: %s\n  at %s:%d\n", \
                  (msg), __FILE__, __LINE__); \
          DEBUG_TRAP(); \
      } } while (0)
#else
  #define DEBUG_TRAP()        ((void)0)
  #define ASSERT(cond, msg)   ((void)0)
#endif

/* ── Input ───────────────────────────────────────────────────────────── */
#define BUTTON_COUNT 4
typedef struct { int ended_down; int half_transition_count; } GameButtonState;
#define UPDATE_BUTTON(button, is_down) \
    do { if ((button).ended_down != (is_down)) { \
        (button).half_transition_count++; (button).ended_down = (is_down); \
    } } while (0)
typedef struct {
    union {
        GameButtonState buttons[BUTTON_COUNT];
        struct {
            GameButtonState move_up, move_down, move_left, move_right;
        };
    };
    int quit; int restart;
} GameInput;

/* ── Tile / Lane Constants ─────────────────────────────────────────── */
#define TILE_CELLS        8
#define TILE_PX          (TILE_CELLS * CELL_PX)   /* 64 pixels */
#define LANE_WIDTH       18
#define LANE_PATTERN_LEN 64
#define NUM_LANES        10

/* ── Sprite IDs ──────────────────────────────────────────────────────── */
#define NUM_SPRITES  9
#define SPR_FROG     0
#define SPR_WATER    1
#define SPR_PAVEMENT 2
#define SPR_WALL     3
#define SPR_HOME     4
#define SPR_LOG      5
#define SPR_CAR1     6
#define SPR_CAR2     7
#define SPR_BUS      8

/* ── GAME_PHASE ─────────────────────────────────────────────────────── */
typedef enum { PHASE_PLAYING = 0, PHASE_DEAD, PHASE_WIN } GAME_PHASE;

/* ── GameState ───────────────────────────────────────────────────────── */
typedef struct {
    float      frog_x, frog_y, time;
    GAME_PHASE phase;
    int        homes_reached;
    float      dead_timer;
    int        score, lives, best_score;
} GameState;

/* ══════ lane_scroll ════════════════════════════════════════════════════
 *
 * Pixel-accurate scroll position for a lane.  Called by BOTH game_update
 * (danger buffer) and game_render (tile drawing) with the same state->time
 * so collision always matches what's on screen.
 *
 * The naïve (int)(time * speed) % PATTERN_LEN fails because:
 *   1. C (int) truncates toward zero, not toward -infinity.
 *      floor(-4.5) = -5 but (int)(-4.5) = -4.
 *      For negative speeds, tile_start jumps one tile too soon.
 *   2. Sub-tile offset would be in cell units (8-pixel steps), causing
 *      8-pixel jumps instead of pixel-smooth scrolling.
 *
 * THE FIX: work in pixels, use positive-modulo clamp.
 *
 *   sc = (int)(time * speed * TILE_PX) % PATTERN_PX  ← may be negative
 *   if (sc < 0) sc += PATTERN_PX                      ← always [0, PATTERN_PX)
 *   tile_start = sc / TILE_PX
 *   px_offset  = sc % TILE_PX                         ← 0..63 px (smooth)
 *
 * EXAMPLE (speed = -3, time = 1.33 s, TILE_PX = 64, PATTERN_PX = 4096):
 *   raw      = (int)(1.33 * -3.0 * 64.0) = (int)(-255.36) = -255
 *   sc       = -255 % 4096 = -255   (C preserves sign on %)
 *   sc after clamp: -255 + 4096 = 3841   (always non-negative)
 *   tile_start = 3841 / 64 = 60
 *   px_offset  = 3841 % 64 = 1    ← 1-pixel sub-tile offset — smooth!
 *
 * JS equivalent:
 *   function laneScroll(time, speed, tilePx, patternLen) {
 *     const PATTERN_PX = patternLen * tilePx;
 *     let sc = Math.floor(time * speed * tilePx) % PATTERN_PX;
 *     if (sc < 0) sc += PATTERN_PX;
 *     return { tileStart: (sc / tilePx)|0, pxOffset: sc % tilePx };
 *   }
 * ═══════════════════════════════════════════════════════════════════════ */
static inline void lane_scroll(float time, float speed,
                                int *out_tile_start, int *out_px_offset) {
    const int PATTERN_PX = LANE_PATTERN_LEN * TILE_PX;  /* 64 * 64 = 4096 */
    int sc = (int)(time * speed * (float)TILE_PX) % PATTERN_PX;
    if (sc < 0) sc += PATTERN_PX;          /* positive modulo clamp */
    *out_tile_start = sc / TILE_PX;
    *out_px_offset  = sc % TILE_PX;
}

/* ── Public API ──────────────────────────────────────────────────────── */
void prepare_input_frame(GameInput *old_input, GameInput *current_input);
void game_init(GameState *state);
void game_update(GameState *state, const GameInput *input, float dt);
void game_render(Backbuffer *bb, const GameState *state);

#endif /* FROGGER_GAME_H */
```

---

## Step 2 — Updated `src/game.c` (Lesson 06 stage)

`game_update` uses `lane_scroll` for river drift.
`game_render` uses `lane_scroll` for tile positioning.

```c
/* game.c — Frogger Logic + Rendering (Lesson 06: scrolling) */
#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include "utils/draw-shapes.h"

/* ── Lane data (DOD: see Lesson 05) ─────────────────────────────────── */
static const float lane_speeds[NUM_LANES] = {
    0.0f, -3.0f, 3.0f, 2.0f, 0.0f, -3.0f, 3.0f, -4.0f, 2.0f, 0.0f,
};

static const char lane_patterns[NUM_LANES][LANE_PATTERN_LEN + 1] = {
    "wwwhhwwwhhwwwhhwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww",
    ",,,jllk,,jllllk,,,,,,,jllk,,,,,jk,,,jlllk,,,,jllllk,,,,jlllk,,,,",
    ",,,,jllk,,,,,jllk,,,,jllk,,,,,,,,,jllk,,,,,jk,,,,,,jllllk,,,,,,,",
    ",,jlk,,,,,jlk,,,,,jk,,,,,jlk,,,jlk,,,,jk,,,,jllk,,,,jk,,,,,,jk,,",
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
    "....asdf.......asdf....asdf..........asdf........asdf....asdf....",
    ".....ty..ty....ty....ty.....ty........ty..ty.ty......ty.......ty.",
    "..zx.....zx.........zx..zx........zx...zx...zx....zx...zx...zx..",
    "..ty.....ty.......ty.....ty......ty..ty.ty.......ty....ty........",
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
};

static int tile_is_safe(char c) {
    return (c == '.' || c == 'j' || c == 'l' || c == 'k' ||
            c == 'p' || c == 'h');
}

/* Debug colour for each tile type */
static uint32_t tile_debug_color(char c) {
    switch (c) {
        case 'w': return GAME_RGBA( 80,  50,  20, 255);
        case 'h': return GAME_RGBA( 20, 140,  20, 255);
        case ',': return GAME_RGBA( 30,  60, 180, 255);
        case 'p': return GAME_RGBA( 80,  80,  80, 255);
        case '.': return GAME_RGBA( 50,  50,  50, 255);
        case 'j': case 'l': case 'k':
            return GAME_RGBA(120, 80, 20, 255);
        case 'z': case 'x':
            return GAME_RGBA(200, 50, 50, 255);
        case 't': case 'y':
            return GAME_RGBA(200, 150, 50, 255);
        case 'a': case 's': case 'd': case 'f':
            return GAME_RGBA(180, 80, 180, 255);
        default:
            return GAME_RGBA(20, 20, 20, 255);
    }
}

/* ── prepare_input_frame ────────────────────────────────────────────── */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
    int btn;
    for (btn = 0; btn < BUTTON_COUNT; btn++) {
        current_input->buttons[btn].ended_down =
            old_input->buttons[btn].ended_down;
        current_input->buttons[btn].half_transition_count = 0;
    }
    current_input->quit = 0;
    current_input->restart = 0;
}

/* ── game_init ──────────────────────────────────────────────────────── */
void game_init(GameState *state) {
    int best = state->best_score;
    memset(state, 0, sizeof(GameState));
    state->best_score = best;
    state->frog_x = 8.0f;
    state->frog_y = 9.0f;
    state->phase  = PHASE_PLAYING;
    state->lives  = 3;
}

/* ── game_update ────────────────────────────────────────────────────── */
void game_update(GameState *state, const GameInput *input, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    if (input->restart) { game_init(state); return; }

    if (state->phase == PHASE_DEAD) {
        state->dead_timer -= dt;
        if (state->dead_timer <= 0.0f) {
            if (state->lives > 0) {
                state->frog_x = 8.0f; state->frog_y = 9.0f;
                state->phase  = PHASE_PLAYING;
            } else { state->dead_timer = 0.0f; }
        }
        return;
    }
    if (state->phase == PHASE_WIN) return;

    /* PHASE_PLAYING */
    state->time += dt;

#define JUST_PRESSED(btn) ((btn).ended_down && (btn).half_transition_count > 0)
    if (JUST_PRESSED(input->move_up)    && state->frog_y > 0.0f) {
        state->frog_y -= 1.0f; state->score += 10;
    }
    if (JUST_PRESSED(input->move_down)  && state->frog_y < 9.0f)
        state->frog_y += 1.0f;
    if (JUST_PRESSED(input->move_left)  && state->frog_x > 0.0f)
        state->frog_x -= 1.0f;
    if (JUST_PRESSED(input->move_right) && state->frog_x < 15.0f)
        state->frog_x += 1.0f;
#undef JUST_PRESSED

    /* ── River drift ──────────────────────────────────────────────────
       When the frog is on a river row (1–3), it drifts with the current.
       Positive speed → tiles scroll right → frog moves right → subtract
       (the frog is carried by the log, not moving independently).      */
    {
        int fy = (int)state->frog_y;
        if (fy >= 1 && fy <= 3)
            state->frog_x -= lane_speeds[fy] * dt;
    }

    /* Clamp frog to screen — boundary death handled properly in Lesson 07 */
    if (state->frog_x < 0.0f) {
        state->lives--;
        state->phase = PHASE_DEAD;
        state->dead_timer = 0.5f;
        return;
    }
    if (state->frog_x > 15.0f) {
        state->lives--;
        state->phase = PHASE_DEAD;
        state->dead_timer = 0.5f;
        return;
    }

    if ((int)state->frog_y == 0) {
        if (state->score > state->best_score)
            state->best_score = state->score;
        state->phase = PHASE_WIN;
    }
}

/* ── game_render ────────────────────────────────────────────────────── */
void game_render(Backbuffer *bb, const GameState *state) {
    int y, i;

    /* 1. Clear */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

    /* 2. Draw all 10 lanes with scrolling
       ────────────────────────────────────────────────────────────────
       lane_scroll() returns:
         tile_start: which pattern index starts at or before screen left
         px_offset:  how many pixels the tiles are shifted rightward

       The render loop draws LANE_WIDTH (18) tiles.  We start from tile
       position "-1" (one tile off the left edge) and subtract px_offset.
       This gives smooth sub-tile-pixel scrolling.

       IMPORTANT: same state->time as game_update — consistency invariant.
       If this called GetTime() instead, collision would ghost-mismatch.  */
    for (y = 0; y < NUM_LANES; y++) {
        int tile_start, px_offset;
        lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
        int dest_py = y * TILE_PX;

        for (i = 0; i < LANE_WIDTH; i++) {
            char c = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];

            /* (-1 + i): start one tile left of the screen, so we can
               slide it right by px_offset without revealing a black gap. */
            int dest_px = (-1 + i) * TILE_PX - px_offset;

            draw_rect(bb, dest_px, dest_py, TILE_PX, TILE_PX,
                      tile_debug_color(c));

            /* Subtle tile border for visibility */
            draw_rect(bb, dest_px, dest_py, TILE_PX, 1,
                      GAME_RGBA(0, 0, 0, 60));
            draw_rect(bb, dest_px, dest_py, 1, TILE_PX,
                      GAME_RGBA(0, 0, 0, 60));

            /* Safe/unsafe dot indicator */
            draw_rect(bb, dest_px + 2, dest_py + 2, 4, 4,
                      tile_is_safe(c) ? COLOR_GREEN : COLOR_RED);
        }
    }

    /* 3. Draw frog (with death flash) */
    {
        int show_frog = 1;
        if (state->phase == PHASE_DEAD) {
            int flash = (int)(state->dead_timer / 0.05f);
            show_frog = (flash % 2 == 0);
        }
        if (show_frog) {
            int fx = (int)(state->frog_x * (float)TILE_PX);
            int fy_px = (int)(state->frog_y * (float)TILE_PX);
            draw_rect(bb, fx + 8,  fy_px + 8,  48, 48, GAME_RGBA(0, 200, 0, 255));
            draw_rect(bb, fx + 16, fy_px + 4,   8,  8, GAME_RGBA(0, 220, 0, 255));
            draw_rect(bb, fx + 40, fy_px + 4,   8,  8, GAME_RGBA(0, 220, 0, 255));
        }
    }

    /* 4. Status bar */
    {
        uint32_t pc;
        switch (state->phase) {
            case PHASE_PLAYING: pc = COLOR_GREEN;  break;
            case PHASE_DEAD:    pc = COLOR_RED;    break;
            case PHASE_WIN:     pc = COLOR_YELLOW; break;
            default:            pc = COLOR_WHITE;  break;
        }
        draw_rect(bb, 0, bb->height - 16, bb->width, 16, pc);
        int j;
        for (j = 0; j < state->lives; j++)
            draw_rect(bb, bb->width - 20 - j * 20,
                      bb->height - 14, 16, 12, COLOR_CYAN);
    }

    if (state->phase == PHASE_WIN) {
        draw_rect_blend(bb, 0, 0, bb->width, bb->height, COLOR_OVERLAY_DIM);
        draw_rect(bb, bb->width/2-4, bb->height/2-32, 8, 64, COLOR_YELLOW);
        draw_rect(bb, bb->width/2-32, bb->height/2-4, 64, 8, COLOR_YELLOW);
    }
    if (state->phase == PHASE_DEAD && state->lives <= 0) {
        draw_rect_blend(bb, 0, 0, bb->width, bb->height, COLOR_OVERLAY_DIM);
        draw_rect(bb, bb->width/2-32, bb->height/2-4, 64, 8, COLOR_RED);
    }
}
```

---

## Build and Run

```bash
./build-dev.sh -r               # Raylib
./build-dev.sh --backend=x11 -r # X11
```

---

## Expected Result

All 10 lanes scroll:

| Lane | Direction | Visual                                            |
|------|-----------|---------------------------------------------------|
| 0    | Still     | Wall/home tiles fixed                             |
| 1    | Left      | Log tiles drift left; water gaps appear/disappear |
| 2    | Right     | Log tiles drift right                             |
| 3    | Right     | Slower log drift                                  |
| 4    | Still     | Pavement — fixed                                  |
| 5    | Left      | Purple bus segments scroll left (fast)            |
| 6    | Right     | Orange cars scroll right                          |
| 7    | Left      | Red cars scroll left (fastest)                    |
| 8    | Right     | Orange cars scroll right (slow)                   |
| 9    | Still     | Pavement — fixed                                  |

**No 64-pixel jumps** — tiles slide smoothly by 1 pixel at a time.

Move the frog to row 1, 2, or 3 and stand still — the frog drifts
sideways with the log current.  Slide off the screen edge → death flash → respawn.

---

## Verifying the Fix Manually

To see the 64-pixel jump bug, temporarily change `lane_scroll` to the naïve version:

```c
/* BROKEN version — demonstrates the jump bug */
static inline void lane_scroll_BROKEN(float time, float speed,
                                       int *out_tile_start, int *out_px_offset) {
    *out_tile_start = (int)(time * speed) % LANE_PATTERN_LEN;
    if (*out_tile_start < 0) *out_tile_start += LANE_PATTERN_LEN;
    *out_px_offset = 0;   /* ← sub-tile offset missing */
}
```

Run it and stare at a fast-moving lane.  The tiles snap by 64 pixels instead
of scrolling.  Then restore the real `lane_scroll` and the difference is
immediately obvious.

---

## Exercises

1. **Debug output**: Print `tile_start` and `px_offset` to `stderr` once per
   second for lane 1.  Confirm `px_offset` sweeps 0–63 smoothly, never jumping.
2. **Pause test**: Add a keyboard shortcut (spacebar) that stops `state->time`
   from incrementing.  All lanes freeze instantly — verify no tile is in a
   split-pixel position when paused.
3. **Speed experiment**: Change `lane_speeds[7]` from `-4.0f` to `-0.1f`.
   Notice the fast lane becomes slow; the smooth scroll still works.
4. **Wrap boundary**: Set `time = 10000.0f` directly in `game_init`.
   Confirm no crash, no visual glitch — the modulo wraps cleanly.
5. **Negative-modulo trace**: Add a `fprintf` inside `lane_scroll` that prints
   the value of `sc` before and after the `if (sc < 0)` clamp for lane 1.
   Verify that `sc` is negative at the start and becomes positive after the clamp.

---

## What's next

**Lesson 07** uses the same `lane_scroll` call to build the **danger buffer**:
a flat 2D bitmask (`state->danger[SCREEN_CELLS_W * SCREEN_CELLS_H]`) rebuilt
every frame in `game_update`.  The frog's center-cell position is checked
against the buffer, and walking into an unsafe cell triggers `PHASE_DEAD`
with the death flash from Lesson 04.
