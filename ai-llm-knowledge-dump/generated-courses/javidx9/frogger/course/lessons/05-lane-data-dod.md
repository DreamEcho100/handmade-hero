# Lesson 05: Lane Data (Data-Oriented Design)

## What we're building

All 10 Frogger lanes rendered as colour-coded rectangles — no scrolling yet,
but every lane has the right colour for its type (water = blue, road = dark grey,
pavement = grey, home = dark green).  Individual tile types within each lane
are visible as coloured cells: logs, cars, buses, home slots all show as
distinct colours.

The data behind this rendering uses **Data-Oriented Design** (DOD): two separate
arrays (`lane_speeds` and `lane_patterns`) instead of a `Lane` struct array.

---

## What you'll learn

- Why DOD uses parallel arrays (`{ids:[], speeds:[]}`) instead of an array of
  structs (`[{id, speed}]`)
- `lane_speeds[NUM_LANES]` — one `float` per lane (10 × 4 = 40 bytes, one cache line)
- `lane_patterns[NUM_LANES][LANE_PATTERN_LEN]` — a 2D char grid encoding tile types
- `tile_is_safe(c)` — maps a pattern character to a safe/unsafe boolean
- `tile_to_sprite(c, &src_x)` — maps a character to a sprite ID (sprite rendering in Lesson 08)
- How to render coloured tiles from a character grid without sprites
- `TILE_PX`, `TILE_CELLS`, `LANE_PATTERN_LEN`, `NUM_LANES` constants

---

## Prerequisites

- Lessons 01–04: window, drawing, input, game structure

---

## Concepts

### 1. What Is Data-Oriented Design?

Object-Oriented thinking groups related data together:

```c
// AoS (Array of Structs) — OOP style:
typedef struct {
    float speed;
    char  pattern[LANE_PATTERN_LEN + 1];
} Lane;
static Lane lanes[NUM_LANES];
```

Data-Oriented Design separates data by how it is **accessed**:

```c
// SoA (Struct of Arrays) — DOD style:
static const float lane_speeds[NUM_LANES];
static const char  lane_patterns[NUM_LANES][LANE_PATTERN_LEN + 1];
```

```js
// JS analogy:
// AoS: [ {speed: -3, pattern: ',,jllk,'}, {speed: 3, pattern: ',,jllk,'}, ... ]
// SoA: { speeds: [-3, 3, ...], patterns: [',,jllk,', ',,jllk,', ...] }
```

**Why the SoA layout is faster here:**

`game_update` reads `lane_speeds[]` for two purposes:
1. River drift — push the frog sideways when it rides a log
2. Danger buffer rebuild — compute `tile_start` and `px_offset` for each lane

It does **not** need `lane_patterns[]` for the river drift calculation.  With
the SoA layout, all 10 speeds fit in one 40-byte read (one CPU cache line).
With the AoS layout, each `Lane` struct is ~70 bytes (4 + 65 bytes rounded up),
so 10 structs = ~700 bytes — spanning more than 10 cache lines.

```
SoA lane_speeds read:
  cache miss → load 64-byte cache line → contains all 10 floats ✓

AoS lanes read (only speed field):
  lanes[0] → cache miss → load 64 bytes → get speed, discard 60 bytes
  lanes[1] → cache miss → load 64 bytes → get speed, discard 60 bytes
  ... × 10 = 10 cache misses ✗
```

`game_render` reads **both** arrays — but they are touched in distinct inner
loops, so keeping them separate still avoids cache pollution between those loops.

### 2. Tile Constants

```c
/* From utils/backbuffer.h (already defined):
   SCREEN_CELLS_W = 128 cells wide
   SCREEN_CELLS_H = 80 cells tall
   CELL_PX = 8 px per cell                                              */

#define TILE_CELLS        8
#define TILE_PX          (TILE_CELLS * CELL_PX)   /* 8 * 8 = 64 pixels per tile */
#define LANE_WIDTH       18    /* tiles visible + 1 wrap tile             */
#define LANE_PATTERN_LEN 64    /* tiles in the repeating pattern          */
#define NUM_LANES        10    /* home + 3 river + pavement + 4 road + pavement */
```

```
Screen width in tiles:  SCREEN_CELLS_W / TILE_CELLS = 128 / 8 = 16 tiles
Tile width in pixels:   TILE_CELLS * CELL_PX = 8 * 8 = 64 px
Pattern length:         64 tiles
Pattern pixel length:   64 * 64 = 4096 px
```

### 3. Lane Speed Array

```c
static const float lane_speeds[NUM_LANES] = {
     0.0f,  /* 0  home row    — stationary                               */
    -3.0f,  /* 1  river left  — logs drift left at 3 tiles/sec           */
     3.0f,  /* 2  river right — logs drift right at 3 tiles/sec          */
     2.0f,  /* 3  river right — logs drift right at 2 tiles/sec          */
     0.0f,  /* 4  pavement    — safe middle strip                        */
    -3.0f,  /* 5  road left   — bus moves left at 3 tiles/sec            */
     3.0f,  /* 6  road right  — car2 moves right at 3 tiles/sec          */
    -4.0f,  /* 7  road left   — car1 moves left at 4 tiles/sec (fast!)   */
     2.0f,  /* 8  road right  — car2 moves right at 2 tiles/sec          */
     0.0f,  /* 9  pavement    — safe start row                           */
};
```

Negative = moves left (the pattern scrolls leftward).
Positive = moves right (the pattern scrolls rightward).
Zero = stationary (home, pavement).

### 4. Lane Pattern Array

Each row is exactly `LANE_PATTERN_LEN` (64) characters.  The character encodes
the tile type at that position in the pattern:

```
w = wall (deadly)          h = home cell (safe goal)
, = water (deadly)         j/l/k = log start/mid/end (safe riding)
p = pavement (safe)        . = road surface (safe ground)
a/s/d/f = bus segments     z/x = car1 front/back
t/y = car2 front/back      (all vehicle tiles deadly)
```

```c
static const char lane_patterns[NUM_LANES][LANE_PATTERN_LEN + 1] = {
    /* 0 home row:  walls + 5 home slots, rest is wall                  */
    "wwwhhwwwhhwwwhhwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww",
    /* 1 river:  long logs + gaps of water                              */
    ",,,jllk,,jllllk,,,,,,,jllk,,,,,jk,,,jlllk,,,,jllllk,,,,jlllk,,,,",
    /* 2 river:  shorter logs going the other way                       */
    ",,,,jllk,,,,,jllk,,,,jllk,,,,,,,,,jllk,,,,,jk,,,,,,jllllk,,,,,,,",
    /* 3 river:  single logs + pairs                                    */
    ",,jlk,,,,,jlk,,,,,jk,,,,,jlk,,,jlk,,,,jk,,,,jllk,,,,jk,,,,,,jk,,",
    /* 4 pavement: full safe strip                                       */
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
    /* 5 road: buses (4-tile)                                            */
    "....asdf.......asdf....asdf..........asdf........asdf....asdf....",
    /* 6 road: small cars (2-tile)                                       */
    ".....ty..ty....ty....ty.....ty........ty..ty.ty......ty.......ty.",
    /* 7 road: single cars (2-tile, fast)                               */
    "..zx.....zx.........zx..zx........zx...zx...zx....zx...zx...zx..",
    /* 8 road: small cars again                                         */
    "..ty.....ty.......ty.....ty......ty..ty.ty.......ty....ty........",
    /* 9 pavement: full safe strip (start row)                          */
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
};
```

```js
// JS analogy:
// const lanePatterns = [
//   'wwwhhwwwhhwwwhhww...',  // row 0 (home)
//   ',,,jllk,,jllllk,...',   // row 1 (river)
//   // ...
// ];
// const tileAt = (lane, tileIndex) => lanePatterns[lane][tileIndex % 64];
```

### 5. `tile_is_safe` — Collision Helper

```c
static int tile_is_safe(char c) {
    return (c == '.' || c == 'j' || c == 'l' || c == 'k' ||
            c == 'p' || c == 'h');
}
```

```js
// const SAFE_TILES = new Set(['.', 'j', 'l', 'k', 'p', 'h']);
// function tileIsSafe(c) { return SAFE_TILES.has(c); }
```

Safe tiles: road surface `.`, log start/mid/end `j/l/k`, pavement `p`, home `h`.
Everything else (water `,`, wall `w`, all vehicles) is deadly.

### 6. `tile_to_sprite` — Sprite Mapping

Used by `game_render` to translate a pattern character into a sprite ID and
horizontal offset within the sprite sheet.  The full sprite drawing arrives
in Lesson 08; for now, `tile_to_sprite` is implemented but only the safe/unsafe
colouring is used for rendering.

```c
static int tile_to_sprite(char c, int *src_x) {
    *src_x = 0;
    switch (c) {
        case 'w': return SPR_WALL;
        case 'h': return SPR_HOME;
        case ',': return SPR_WATER;
        case 'p': return SPR_PAVEMENT;
        case 'j': *src_x =  0; return SPR_LOG;
        case 'l': *src_x =  8; return SPR_LOG;
        case 'k': *src_x = 16; return SPR_LOG;
        case 'z': *src_x =  0; return SPR_CAR1;
        case 'x': *src_x =  8; return SPR_CAR1;
        case 't': *src_x =  0; return SPR_CAR2;
        case 'y': *src_x =  8; return SPR_CAR2;
        case 'a': *src_x =  0; return SPR_BUS;
        case 's': *src_x =  8; return SPR_BUS;
        case 'd': *src_x = 16; return SPR_BUS;
        case 'f': *src_x = 24; return SPR_BUS;
        default:  return -1;
    }
}
```

The log sprite sheet is 24 cells wide: `j` (start) at x=0, `l` (middle) at
x=8, `k` (end) at x=16.  Each segment is 8 cells = 64 pixels wide.
`src_x` is the cell offset within that sheet.

---

## Step 1 — Updated `src/game.h` (Lesson 05 stage)

```c
/* game.h — Frogger Types (Lesson 05: lane data constants) */
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

/* ── Tile / Lane Constants ─────────────────────────────────────────────
   TILE_CELLS / TILE_PX: each sprite tile = 8 cells = 64 pixels.
   LANE_PATTERN_LEN: the repeating tile pattern is 64 tiles long.
   NUM_LANES: 10 lanes (home + 3 river + pavement + 4 road + pavement). */
#define TILE_CELLS        8
#define TILE_PX          (TILE_CELLS * CELL_PX)  /* 64 pixels */
#define LANE_WIDTH       18
#define LANE_PATTERN_LEN 64
#define NUM_LANES        10

/* ── Sprite IDs (for tile_to_sprite; sprites loaded in Lesson 08) ──── */
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

/* ── Public API ──────────────────────────────────────────────────────── */
void prepare_input_frame(GameInput *old_input, GameInput *current_input);
void game_init(GameState *state);
void game_update(GameState *state, const GameInput *input, float dt);
void game_render(Backbuffer *bb, const GameState *state);

#endif /* FROGGER_GAME_H */
```

---

## Step 2 — Updated `src/game.c` (Lesson 05 stage)

```c
/* game.c — Frogger Logic + Rendering (Lesson 05: lane data DOD) */
#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include "utils/draw-shapes.h"

/* ═══════════════════════════════════════════════════════════════════════
 * LANE DATA (DOD: Struct-of-Arrays layout)
 *
 * Two parallel arrays, NOT a Lane struct.
 * lane_speeds: 10 floats = 40 bytes = fits in one cache line.
 *   game_update reads this for river drift + danger buffer.
 * lane_patterns: char grid, read only during rendering.
 *   game_render reads both arrays in separate inner loops.
 *
 * JS equivalent:
 *   // AoS (don't do this):  [{speed, pattern}, {speed, pattern}, ...]
 *   // SoA (DOD, do this):   {speeds: [...], patterns: [...]}
 * ═══════════════════════════════════════════════════════════════════════ */
static const float lane_speeds[NUM_LANES] = {
     0.0f,  /* 0 home      */
    -3.0f,  /* 1 river L   */
     3.0f,  /* 2 river R   */
     2.0f,  /* 3 river R   */
     0.0f,  /* 4 pavement  */
    -3.0f,  /* 5 road L    */
     3.0f,  /* 6 road R    */
    -4.0f,  /* 7 road L    */
     2.0f,  /* 8 road R    */
     0.0f,  /* 9 pavement  */
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

/* ── Tile helpers ──────────────────────────────────────────────────── */
static int tile_is_safe(char c) {
    return (c == '.' || c == 'j' || c == 'l' || c == 'k' ||
            c == 'p' || c == 'h');
}

/* Maps a tile character to a sprite ID and cell offset in the sheet.
   Not yet used for drawing (Lesson 08), but available for reference.   */
static int tile_to_sprite(char c, int *src_x) {
    *src_x = 0;
    switch (c) {
        case 'w': return SPR_WALL;
        case 'h': return SPR_HOME;
        case ',': return SPR_WATER;
        case 'p': return SPR_PAVEMENT;
        case 'j': *src_x =  0; return SPR_LOG;
        case 'l': *src_x =  8; return SPR_LOG;
        case 'k': *src_x = 16; return SPR_LOG;
        case 'z': *src_x =  0; return SPR_CAR1;
        case 'x': *src_x =  8; return SPR_CAR1;
        case 't': *src_x =  0; return SPR_CAR2;
        case 'y': *src_x =  8; return SPR_CAR2;
        case 'a': *src_x =  0; return SPR_BUS;
        case 's': *src_x =  8; return SPR_BUS;
        case 'd': *src_x = 16; return SPR_BUS;
        case 'f': *src_x = 24; return SPR_BUS;
        default:  return -1;
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
    current_input->quit = 0; current_input->restart = 0;
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
    if ((int)state->frog_y == 0) {
        if (state->score > state->best_score)
            state->best_score = state->score;
        state->phase = PHASE_WIN;
    }
}

/* ── Colour map for lesson-05 tile rendering (no sprites yet) ──────── */
/* Returns a colour that represents each tile type.
   In Lesson 08 these will be replaced by actual sprite drawing.        */
static uint32_t tile_debug_color(char c) {
    switch (c) {
        case 'w': return GAME_RGBA( 80,  50,  20, 255);  /* wall   = brown  */
        case 'h': return GAME_RGBA( 20, 140,  20, 255);  /* home   = green  */
        case ',': return GAME_RGBA( 30,  60, 180, 255);  /* water  = blue   */
        case 'p': return GAME_RGBA( 80,  80,  80, 255);  /* paving = grey   */
        case '.': return GAME_RGBA( 50,  50,  50, 255);  /* road   = dark   */
        case 'j': case 'l': case 'k':
            return GAME_RGBA(120,  80,  20, 255);         /* log    = tan    */
        case 'z': case 'x':
            return GAME_RGBA(200,  50,  50, 255);         /* car1   = red    */
        case 't': case 'y':
            return GAME_RGBA(200, 150,  50, 255);         /* car2   = orange */
        case 'a': case 's': case 'd': case 'f':
            return GAME_RGBA(180,  80, 180, 255);         /* bus    = purple */
        default:
            return GAME_RGBA( 20,  20,  20, 255);         /* unknown= near-black */
    }
}

/* ── game_render ────────────────────────────────────────────────────── */
void game_render(Backbuffer *bb, const GameState *state) {
    int y, i;

    /* 1. Clear */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

    /* 2. Render all 10 lanes.
       At this stage: no scrolling, no sprites — just colour-coded tiles.
       tile_start = 0: we read pattern from the beginning each frame.
       Scrolling is added in Lesson 06 (lane_scroll).                   */
    for (y = 0; y < NUM_LANES; y++) {
        int dest_py = y * TILE_PX;   /* pixel Y of this lane's top edge */

        /* Draw LANE_WIDTH tiles starting from pattern position 0 */
        for (i = 0; i < LANE_WIDTH; i++) {
            /* Pattern position: wrap around at LANE_PATTERN_LEN         */
            char c = lane_patterns[y][i % LANE_PATTERN_LEN];

            /* Pixel X of this tile's left edge (no offset yet)         */
            int dest_px = i * TILE_PX;

            /* Draw a solid tile using the debug colour map             */
            uint32_t col = tile_debug_color(c);
            draw_rect(bb, dest_px, dest_py, TILE_PX, TILE_PX, col);

            /* Draw a 1-px dark border to make tiles visually distinct  */
            draw_rect(bb, dest_px, dest_py, TILE_PX, 1,
                      GAME_RGBA(0, 0, 0, 80));
            draw_rect(bb, dest_px, dest_py, 1, TILE_PX,
                      GAME_RGBA(0, 0, 0, 80));
        }
    }

    /* 3. Mark safe vs deadly with a small indicator dot on each tile    */
    for (y = 0; y < NUM_LANES; y++) {
        int dest_py = y * TILE_PX;
        for (i = 0; i < LANE_WIDTH; i++) {
            char c    = lane_patterns[y][i % LANE_PATTERN_LEN];
            int safe  = tile_is_safe(c);
            int dest_px = i * TILE_PX;
            /* Green dot = safe, Red dot = deadly                        */
            draw_rect(bb, dest_px + 2, dest_py + 2, 4, 4,
                      safe ? COLOR_GREEN : COLOR_RED);
        }
    }

    /* 4. Draw frog placeholder */
    {
        int show_frog = 1;
        if (state->phase == PHASE_DEAD) {
            int flash = (int)(state->dead_timer / 0.05f);
            show_frog = (flash % 2 == 0);
        }
        if (show_frog) {
            int fx = (int)(state->frog_x * (float)TILE_PX);
            int fy = (int)(state->frog_y * (float)TILE_PX);
            draw_rect(bb, fx + 8, fy + 8, 48, 48, GAME_RGBA(0, 200, 0, 255));
            draw_rect(bb, fx + 16, fy + 4, 8, 8,  GAME_RGBA(0, 220, 0, 255));
            draw_rect(bb, fx + 40, fy + 4, 8, 8,  GAME_RGBA(0, 220, 0, 255));
        }
    }

    /* 5. Status bar */
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
}
```

---

## Build and Run

```bash
./build-dev.sh -r              # Raylib
./build-dev.sh --backend=x11 -r  # X11
```

---

## Expected Result

Ten horizontal bands fill the 1024×640 window:

| Row | Colour        | Content                       |
|-----|---------------|-------------------------------|
| 0   | Mixed brown/green | Wall tiles + green home slots |
| 1–3 | Blue + tan    | Water gaps + log tiles        |
| 4   | Grey          | All pavement                  |
| 5–8 | Dark grey     | Road + coloured vehicle tiles |
| 9   | Grey          | All pavement (start row)      |

Each tile has a small green dot (safe) or red dot (unsafe) in its top-left
corner.  The green frog placeholder sits at tile `(8, 9)` on the bottom
pavement row.  Arrow keys move it one tile per press.  The patterns are
**not scrolling** — that's Lesson 06.

---

## Verify the DOD Layout

Add this temporary check to `game_update` to confirm the speed array fits in
one cache line (for educational purposes):

```c
/* In game_update, once, after dt clamp: */
ASSERT(sizeof(lane_speeds) == NUM_LANES * sizeof(float),
       "lane_speeds must be a flat float array");
/* 10 * 4 = 40 bytes — fits in a 64-byte cache line */
```

---

## Exercises

1. Add a new lane type `PHASE_FROZEN` where all speeds are 0.  Map it in
   `lane_speeds` and see the tile colours correctly show no movement.
2. Change the road colour for tiles with speed > 3 (the fast lane at index 7)
   to bright orange — demonstrating that the speed array is separate from the
   pattern data and can be read independently.
3. Count how many safe tiles are in each river lane pattern and print to stderr
   — notice that lane 3 has fewer safe cells (harder to traverse).
4. Rename `tile_to_sprite` to `tile_type` and return an `enum TileType` instead
   of a sprite ID.  Does this change any game behaviour?  (It shouldn't.)

---

## What's next

**Lesson 06** adds `lane_scroll()` — the floor-mod pixel arithmetic that makes
lanes scroll smoothly left and right.  You'll see concretely why the naïve
`(int)(time * speed) % PATTERN_LEN` formula fails for negative speeds, and
how the pixel-based positive-modulo fix produces sub-tile-smooth scrolling
with no 64-pixel jumps.
