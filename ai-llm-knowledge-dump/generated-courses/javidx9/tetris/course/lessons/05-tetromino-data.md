# Lesson 05: Tetromino Data

## What we're building

In this lesson we encode all 7 classic Tetris pieces as 16-character strings,
implement the rotation math, and render a single I-piece at a fixed position on
screen. There is no movement yet — we just want to see the piece displayed
correctly in all four rotations.

```
  col: 0  1  2  3  4  5  6  7  8  9 10 11
row 0: [.][.][.][.][X][.][.][.][.][.][.][.]
row 1: [.][.][.][.][X][.][.][.][.][.][.][.]
row 2: [.][.][.][.][X][.][.][.][.][.][.][.]
row 3: [.][.][.][.][X][.][.][.][.][.][.][.]
```
*The I-piece rendered at field column 4, row 0.*

## What you'll learn

- How a 16-character string encodes a 4×4 grid (the "bitmap string" trick)
- 2D-to-1D index mapping: `index = row * width + col`
- `enum` for type-safe constants (vs magic integers)
- `switch` on an `enum` value
- `extern` to share data across files
- `static` functions for file-private helpers
- Why `const` matters for shared read-only data

## Prerequisites

- Lessons 01–04 complete (project compiles, backbuffer renders, game loop running)
- You can see a blank black window when you run `./build-dev.sh -r`

---

## Step 1: The Bitmap String — Encoding a 4×4 Grid as 16 Characters

In JavaScript you might represent a tetromino as an array of arrays:

```js
// JS: 4 rows × 4 cols as nested arrays
const TETROMINO_I = [
  ['.', '.', 'X', '.'],
  ['.', '.', 'X', '.'],
  ['.', '.', 'X', '.'],
  ['.', '.', 'X', '.'],
];
```

In C we use a single flat string. A string is just an array of `char` — the
characters sit in memory one after another, left-to-right. A 4×4 grid is 16
cells, so we use 16 characters, reading left-to-right, row-by-row:

```
"..X...X...X...X."
 ↑↑↑↑  row 0  (indices 0–3)
     ↑↑↑↑  row 1  (indices 4–7)
         ↑↑↑↑  row 2  (indices 8–11)
             ↑↑↑↑  row 3  (indices 12–15)
```

Mapping that to a grid:

```
index:  0  1  2  3   4  5  6  7   8  9 10 11  12 13 14 15
char:   .  .  X  .   .  .  X  .   .  .  X  .   .  .  X  .

grid (row, col):
  row 0:  [.][.][X][.]   ← cols 0-3, indices 0-3
  row 1:  [.][.][X][.]   ← cols 0-3, indices 4-7
  row 2:  [.][.][X][.]   ← cols 0-3, indices 8-11
  row 3:  [.][.][X][.]   ← cols 0-3, indices 12-15
```

The I-piece is a vertical bar at column 2 of the 4×4 box.

The conversion formula: `index = row * 4 + col`.

> **Why?** In JavaScript, `array[row][col]` hides this math behind two bracket operators. In C, `array[row * WIDTH + col]` is the same math written out explicitly. A 1D array with this index formula IS a 2D grid — it's just flattened. You'll see this pattern everywhere in C (images, game fields, matrices).

Here are all 7 tetrominoes visualised:

```
I: "..X...X...X...X."    J: "..X...X..XX....."
   . . X .                  . . X .
   . . X .                  . . X .
   . . X .                  . X X .
   . . X .                  . . . .

L: ".X...X...XX....."    O: ".....XX..XX....."
   . X . .                  . . . .
   . X . .                  . X X .
   . X X .                  . X X .
   . . . .                  . . . .

S: ".X...XX...X....."    T: "..X..XX...X....."
   . X . .                  . . X .
   . X X .                  . X X .
   . . X .                  . . X .
   . . . .                  . . . .

Z: "..X..XX..X......"
   . . X .
   . X X .
   . X . .
   . . . .
```

> **Why `const char TETROMINO_I[16]`?** The `const` keyword means "this data is read-only after initialisation." Just like JavaScript's `const TETROMINO_I = "...";` prevents reassignment, C's `const char[]` prevents the array's contents from being modified. The `16` is the array size — the compiler checks that you haven't accidentally written fewer or more characters. (Note: C string literals add a NUL terminator `\0`, but since we declare `[16]` and the literal is exactly 16 non-NUL chars, the terminator is deliberately excluded.)

---

## Step 2: TETROMINO_BY_IDX — Type-Safe Piece Selection

Instead of remembering that the I-piece is index 0 and the Z-piece is index 6, we
use an `enum`:

```c
typedef enum {
  TETROMINO_I_IDX,   /* 0 */
  TETROMINO_J_IDX,   /* 1 */
  TETROMINO_L_IDX,   /* 2 */
  TETROMINO_O_IDX,   /* 3 */
  TETROMINO_S_IDX,   /* 4 */
  TETROMINO_T_IDX,   /* 5 */
  TETROMINO_Z_IDX,   /* 6 */
} TETROMINO_BY_IDX;
```

> **Why `typedef enum`?** In JavaScript, you'd write `const enum` in TypeScript, or just `const TETROMINO_I_IDX = 0`. In C, an `enum` defines a set of named integer constants. `typedef` gives the type a single-word name (`TETROMINO_BY_IDX`) so you can use it without writing `enum TETROMINO_BY_IDX` everywhere. The compiler assigns sequential integers starting from 0 unless you specify otherwise. The benefit: `state->current_piece.index = TETROMINO_I_IDX` is self-documenting; `state->current_piece.index = 0` is not.

---

## Step 3: Rotation — `TETROMINO_R_DIR` and `tetromino_pos_value()`

Each piece can be in one of four rotations: 0°, 90°, 180°, 270° clockwise. We
model this with another `enum`:

```c
typedef enum {
  TETROMINO_R_0,    /* 0° — spawn orientation */
  TETROMINO_R_90,   /* 90° clockwise          */
  TETROMINO_R_180,  /* 180°                   */
  TETROMINO_R_270,  /* 270° clockwise         */
} TETROMINO_R_DIR;
```

The key insight: instead of storing four separate strings per piece, we store
ONE string and compute a **different index** into it for each rotation. The
function `tetromino_pos_value(px, py, r)` takes a cell position `(px, py)` in
the 4×4 box and returns the correct string index for rotation `r`.

Here is what each rotation does to the index layout (visualising the 16 indices):

```
R_0   (0°, default):      R_90  (90° clockwise):
 0  1  2  3               12  8  4  0
 4  5  6  7               13  9  5  1
 8  9 10 11               14 10  6  2
12 13 14 15               15 11  7  3

R_180 (180°):             R_270 (270° clockwise):
15 14 13 12                3  7 11 15
11 10  9  8                2  6 10 14
 7  6  5  4                1  5  9 13
 3  2  1  0                0  4  8 12
```

The formulas (N = TETROMINO_LAYER_COUNT = 4):

```
R_0:   index = py * 4 + px              (normal row-major)
R_90:  index = 12 + py - (px * 4)       (column becomes row, reversed)
R_180: index = 15 - (py * 4) - px       (full 180 flip)
R_270: index = 3 - py + (px * 4)        (row becomes column)
```

**Verification — I-piece at R_90** (should look like a horizontal bar in row 2):

```
For py=2, px=0: index = 12 + 2 - (0*4) = 14 → '.'
For py=2, px=1: index = 12 + 2 - (1*4) =  10 → '.'
For py=2, px=2: index = 12 + 2 - (2*4) =  6  → 'X' ✓
For py=2, px=3: index = 12 + 2 - (3*4) =  2  → 'X' ✓

At R_90, row py=2 of the bounding box is: . . X X   (partially horizontal)
At R_90, full scan shows a horizontal row across py=2: ....X...X...X...
wait — actually the horizontal row appears at py=2 in the rotated I:
"..X...X...X...X." rotated 90° → "................XXXX" — let's check py=1:
For py=1, all px: 12+1-(0*4)=13→'.', 12+1-(1*4)=9→'.', 12+1-(2*4)=5→'.', 12+1-(3*4)=1→'.'
For py=2, all px: 12+2-0=14→'.', 12+2-4=10→'.', 12+2-8=6→'.', 12+2-12=2→'X'
For py=0, all px: 12+0-0=12→'.', 12+0-4=8→'.', 12+0-8=4→'.', 12+0-12=0→'.'

Actually let me verify the formula correctly:
String "..X...X...X...X." has 'X' at indices 2, 6, 10, 14.
At R_90 with py=0: indices = 12,8,4,0 → chars '.','.','.','.' (all empty)
At R_90 with py=1: indices = 13,9,5,1 → chars '.','.','.','.'
At R_90 with py=2: indices = 14,10,6,2 → chars '.','.','.','X' — one X!

Hmm, that gives only one 'X' in row 2. The I-piece at 90° should be a
horizontal bar. Let me check: "..X...X...X...X." = index 2='X',6='X',10='X',14='X'
At R_90, px=3 for py=0: 12+0-(3*4)=0→'.' 
At R_90, px=3 for py=1: 12+1-12=1→'.'
At R_90, px=3 for py=2: 12+2-12=2→'X' ✓ (only one X in py=2)
...but we need 4 X's in a row!

Let me try R_90 with the I-piece properly:
The I-piece needs to show as horizontal. Let me scan all py and px:
py=0: px=0→12, px=1→8, px=2→4, px=3→0   → '.','.','.','.''
py=1: px=0→13, px=1→9, px=2→5, px=3→1   → '.','.','.','.''
py=2: px=0→14, px=1→10, px=2→6, px=3→2  → '.','.','.','.''  ← wait
Hmm, indices 14='.', 10='.', 6='.', 2='X'. So py=2: '.','.','.','X'

That's only one X. But we need 4 X's in a row for a horizontal I...

Oh wait, I made an error. The I-piece string is "..X...X...X...X."
Indices: 0='.', 1='.', 2='X', 3='.', 4='.', 5='.', 6='X', 7='.', 8='.', 9='.', 10='X', 11='.', 12='.', 13='.', 14='X', 15='.'

So 'X' at indices 2, 6, 10, 14.
At R_90:
py=0, px=0: 12+0-0=12 → '.'
py=0, px=1: 12+0-4=8  → '.'
py=0, px=2: 12+0-8=4  → '.'
py=0, px=3: 12+0-12=0 → '.'

py=1, px=0: 12+1-0=13 → '.'
py=1, px=1: 12+1-4=9  → '.'
py=1, px=2: 12+1-8=5  → '.'
py=1, px=3: 12+1-12=1 → '.'

py=2, px=0: 12+2-0=14 → 'X' ✓
py=2, px=1: 12+2-4=10 → 'X' ✓
py=2, px=2: 12+2-8=6  → 'X' ✓
py=2, px=3: 12+2-12=2 → 'X' ✓

py=3, px=0: 12+3-0=15 → '.'
py=3, px=1: 12+3-4=11 → '.'
py=3, px=2: 12+3-8=7  → '.'
py=3, px=3: 12+3-12=3 → '.'

So at R_90, row py=2 has 4 X's → horizontal bar ✓
I was wrong above, px=0 gives 14='X', not '.'. Great.
```

So the I-piece at `R_90` looks like:

```
. . . .   (py=0)
. . . .   (py=1)
X X X X   (py=2) ← the horizontal bar
. . . .   (py=3)
```

The `switch` in `tetromino_pos_value` maps the enum to the right formula:

```c
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r) {
    case TETROMINO_R_0:   return py * TETROMINO_LAYER_COUNT + px;
    case TETROMINO_R_90:  return 12 + py - (px * TETROMINO_LAYER_COUNT);
    case TETROMINO_R_180: return 15 - (py * TETROMINO_LAYER_COUNT) - px;
    case TETROMINO_R_270: return 3  - py + (px * TETROMINO_LAYER_COUNT);
  }
  return 0;
}
```

> **Why `switch` on an enum?** In JavaScript you'd use `if/else if` or a lookup object. In C, `switch` on an enum is idiomatic and efficient. More importantly, compilers can warn when you handle only _some_ enum values (with `-Wswitch`), catching bugs at compile time. For example, if you add `TETROMINO_R_45` to the enum later and forget to add a `case`, the compiler will remind you. (See `src/game.c:410-426`.)

---

## Step 4: `extern` — Sharing Data Across Files

The tetromino strings are **defined** in `game.c` but need to be **visible** to
any code that includes `game.h`. In JavaScript, you'd `export const TETROMINOES = [...]`
from one module and `import { TETROMINOES }` in another. In C, this is done
with `extern`:

```c
// game.h — DECLARATION (tells compiler "trust me, this exists somewhere")
extern const char *TETROMINOES[7];

// game.c — DEFINITION (the actual data)
const char *TETROMINOES[TETROMINOS_COUNT] = {
  TETROMINO_I,  /* index 0 */
  TETROMINO_J,  /* index 1 */
  ...
};
```

> **Why `extern`?** In C, every variable must be defined in exactly one `.c` file. If `game.h` had the full definition and was included by both `game.c` and `main_raylib.c`, you'd get a "multiple definition" linker error. `extern` in a header says "this is declared but defined elsewhere — the linker will find it". Think of it as an import promise.

---

## Step 5: Static Drawing Helpers

`get_tetromino_color`, `draw_cell`, and `draw_piece` are `static` functions —
private to `game.c`:

```c
static uint32_t get_tetromino_color(TETROMINO_BY_IDX index) { ... }
static void draw_cell(Backbuffer *bb, int col, int row, uint32_t color) { ... }
static void draw_piece(Backbuffer *bb, int piece_index, int field_col,
                       int field_row, uint32_t color, TETROMINO_R_DIR rotation) { ... }
```

> **Why `static`?** A `static` function at file scope is invisible outside that `.c` file — the linker cannot see it from other translation units. This is exactly like a private function in a JavaScript module (a function that's defined but not exported). It prevents name collisions and signals intent: "this is an implementation detail." In game.c, `draw_cell` and `draw_piece` are helpers only `game_render` needs — the platform layer has no business calling them directly.

---

## Step 6: Complete `game.h` for Lesson 05

Replace your `src/game.h` with this complete file:

```c
/* src/game.h — Lesson 05: Tetromino Data */
#ifndef GAME_H
#define GAME_H

#include "utils/audio.h"      /* GameAudioState — for platform audio calls  */
#include "utils/backbuffer.h" /* Backbuffer — pixel memory for rendering     */
#include <stdbool.h>          /* bool, true, false                           */
#include <stdint.h>           /* uint32_t                                    */

/* ── Field & Cell Constants ── */
#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30
#define SIDEBAR_WIDTH 200

/* ── Tetromino Shape Constants ── */
#define TETROMINO_SPAN        '.'
#define TETROMINO_BLOCK       'X'
#define TETROMINO_LAYER_COUNT 4
#define TETROMINO_SIZE        16
#define TETROMINOS_COUNT      7

/* ── Predefined Colors ── */
#define COLOR_BLACK      GAME_RGB(0,   0,   0  )
#define COLOR_WHITE      GAME_RGB(255, 255, 255)
#define COLOR_GRAY       GAME_RGB(128, 128, 128)
#define COLOR_DARK_GRAY  GAME_RGB(64,  64,  64 )
#define COLOR_CYAN       GAME_RGB(0,   255, 255)
#define COLOR_BLUE       GAME_RGB(0,   0,   255)
#define COLOR_ORANGE     GAME_RGB(255, 165, 0  )
#define COLOR_YELLOW     GAME_RGB(255, 255, 0  )
#define COLOR_GREEN      GAME_RGB(0,   255, 0  )
#define COLOR_MAGENTA    GAME_RGB(255, 0,   255)
#define COLOR_RED        GAME_RGB(255, 0,   0  )

/* ── TETROMINO_BY_IDX — which of the 7 pieces ── */
/* JS: const enum TetrominoByIdx { I=0, J, L, O, S, T, Z } */
typedef enum {
  TETROMINO_I_IDX,  /* 0 — cyan  horizontal/vertical bar */
  TETROMINO_J_IDX,  /* 1 — blue  mirror-L                */
  TETROMINO_L_IDX,  /* 2 — orange L-shape                */
  TETROMINO_O_IDX,  /* 3 — yellow square                 */
  TETROMINO_S_IDX,  /* 4 — green  S-shape                */
  TETROMINO_T_IDX,  /* 5 — magenta T-shape               */
  TETROMINO_Z_IDX,  /* 6 — red   Z-shape                 */
} TETROMINO_BY_IDX;

/* ── TETROMINO_R_DIR — rotation state ── */
typedef enum {
  TETROMINO_R_0,    /* 0° — spawn orientation */
  TETROMINO_R_90,   /* 90° clockwise          */
  TETROMINO_R_180,  /* 180°                   */
  TETROMINO_R_270,  /* 270° clockwise         */
} TETROMINO_R_DIR;

/* ── CurrentPiece — the active falling tetromino ── */
/* Lesson 05: only index and rotation. x/y added in Lesson 06. */
typedef struct {
  TETROMINO_BY_IDX index;           /* Which of the 7 tetrominoes is active */
  TETROMINO_R_DIR  rotate_x_value;  /* Current rotation state [0..3]        */
} CurrentPiece;

/* ── GameButtonState ── */
/* JS: { endedDown: boolean, halfTransitionCount: number } */
typedef struct {
  int half_transition_count;
  int ended_down;
} GameButtonState;

/* ── GameInput ── */
#define BUTTON_COUNT 4
typedef struct {
  union {
    GameButtonState buttons[BUTTON_COUNT];
    struct {
      GameButtonState move_left;
      GameButtonState move_right;
      GameButtonState move_down;
      GameButtonState rotate_x;
    };
  };
} GameInput;

#define UPDATE_BUTTON(button, is_down)                        \
  do {                                                        \
    if ((button).ended_down != (is_down)) {                   \
      (button).half_transition_count++;                       \
      (button).ended_down = (is_down);                        \
    }                                                         \
  } while (0)

/* ── GameState — all mutable game state ── */
/* Lesson 05: minimal. Field, movement, scoring added in later lessons. */
typedef struct {
  CurrentPiece current_piece;   /* The piece currently displayed        */
  GameAudioState audio;         /* Audio state (used by platform layer) */
  int should_quit;              /* 1 = platform should exit             */
  int should_restart;           /* 1 = reinitialise game state          */
} GameState;

/* ── Tetromino Data — defined in game.c ── */
/* JS: export const TETROMINOES: string[] */
extern const char *TETROMINOES[7];

/* ── Function Declarations ── */
void game_init(GameState *state);
void prepare_input_frame(GameInput *old_input, GameInput *new_input);
int  tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);
void game_update(GameState *state, GameInput *input, float delta_time);
void game_render(Backbuffer *backbuffer, GameState *state);

/* Audio — implemented in src/audio.c, called by platform and game_update */
void game_audio_init(GameAudioState *audio, int samples_per_second);
void game_play_sound(GameAudioState *audio, SOUND_ID sound);
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound, float pan_position);
void game_music_play(GameAudioState *audio);
void game_music_stop(GameAudioState *audio);
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer);
void game_audio_update(GameAudioState *audio, float delta_time);

#endif /* GAME_H */
```

---

## Step 7: Complete `game.c` for Lesson 05

Replace your `src/game.c` with this complete file:

```c
/* src/game.c — Lesson 05: Tetromino Data */

#include "game.h"
#include "utils/draw-shapes.h"  /* draw_rect()                        */
#include <stdlib.h>             /* rand(), srand()                    */
#include <string.h>             /* memset()                           */
#include <time.h>               /* time() — seed RNG from wall clock  */

/* ═══════════════════════════════════════════════════════════════════════════
 * Tetromino Data
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Each string is 16 chars representing a 4×4 grid, row-major.
 * '.' = empty, 'X' = filled block.
 * Index formula: char_at[row * 4 + col]
 *
 * JS: const TETROMINO_I = "..X...X...X...X.";
 * C:  const char TETROMINO_I[TETROMINO_SIZE] = "..X...X...X...X.";
 */
const char TETROMINO_I[TETROMINO_SIZE] = "..X...X...X...X."; /* vertical bar  */
const char TETROMINO_J[TETROMINO_SIZE] = "..X...X..XX....."; /* J-shape        */
const char TETROMINO_L[TETROMINO_SIZE] = ".X...X...XX....."; /* L-shape        */
const char TETROMINO_O[TETROMINO_SIZE] = ".....XX..XX....."; /* square         */
const char TETROMINO_S[TETROMINO_SIZE] = ".X...XX...X....."; /* S-shape        */
const char TETROMINO_T[TETROMINO_SIZE] = "..X..XX...X....."; /* T-shape        */
const char TETROMINO_Z[TETROMINO_SIZE] = "..X..XX..X......"; /* Z-shape        */

/* Array of pointers — selects a piece by TETROMINO_BY_IDX value.
 * JS: const TETROMINOES: string[] = [TETROMINO_I, TETROMINO_J, ...]
 * C:  const char *TETROMINOES[] = { ... };  (extern in game.h)              */
const char *TETROMINOES[TETROMINOS_COUNT] = {
  TETROMINO_I, /* 0 = TETROMINO_I_IDX */
  TETROMINO_J, /* 1 = TETROMINO_J_IDX */
  TETROMINO_L, /* 2 = TETROMINO_L_IDX */
  TETROMINO_O, /* 3 = TETROMINO_O_IDX */
  TETROMINO_S, /* 4 = TETROMINO_S_IDX */
  TETROMINO_T, /* 5 = TETROMINO_T_IDX */
  TETROMINO_Z, /* 6 = TETROMINO_Z_IDX */
};

/* ── get_tetromino_color — map piece index to ARGB color ──────────────── */
/* `static`: private to game.c. Not callable from main_raylib.c.
   JS: a function not exported from a module.                              */
static uint32_t get_tetromino_color(TETROMINO_BY_IDX index) {
  switch (index) {
    case TETROMINO_I_IDX: return COLOR_CYAN;
    case TETROMINO_J_IDX: return COLOR_BLUE;
    case TETROMINO_L_IDX: return COLOR_ORANGE;
    case TETROMINO_O_IDX: return COLOR_YELLOW;
    case TETROMINO_S_IDX: return COLOR_GREEN;
    case TETROMINO_T_IDX: return COLOR_MAGENTA;
    case TETROMINO_Z_IDX: return COLOR_RED;
    default:              return COLOR_GRAY;
  }
}

/* ── draw_cell — draw one game cell at field (col, row) ─────────────────
 * Leaves a 1-pixel border around each cell for a grid look.
 * (col, row) are in field coordinates, NOT pixel coordinates.
 * Pixel position = col * CELL_SIZE, row * CELL_SIZE.                     */
static void draw_cell(Backbuffer *bb, int col, int row, uint32_t color) {
  int x = col * CELL_SIZE + 1;   /* +1 inset from cell edge */
  int y = row * CELL_SIZE + 1;
  int w = CELL_SIZE - 2;          /* -2 for 1px gap each side */
  int h = CELL_SIZE - 2;
  draw_rect(bb, x, y, w, h, color);
}

/* ── draw_piece — draw all filled cells of a tetromino ──────────────────
 * Iterates the 4×4 bounding box; for each filled cell calls draw_cell()
 * at the corresponding field position.
 *
 * piece_index: which tetromino (TETROMINO_BY_IDX value)
 * field_col, field_row: top-left anchor in field coordinates
 * color: fill colour
 * rotation: which of the 4 orientations to draw                          */
static void draw_piece(Backbuffer *bb, int piece_index, int field_col,
                       int field_row, uint32_t color, TETROMINO_R_DIR rotation) {
  for (int py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (int px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      /* tetromino_pos_value remaps (px,py) for the current rotation */
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
        draw_cell(bb, field_col + px, field_row + py, color);
      }
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_render — Draw the entire game to the backbuffer.
 * Called by the platform layer every frame.
 *
 * Lesson 05: just clears to black and draws the current piece at a fixed
 * position. Field, walls, and HUD come in later lessons.
 * ═══════════════════════════════════════════════════════════════════════════ */
void game_render(Backbuffer *backbuffer, GameState *state) {
  /* Clear every pixel to black */
  for (int i = 0; i < backbuffer->width * backbuffer->height; i++) {
    backbuffer->pixels[i] = COLOR_BLACK;
  }

  /* Draw the current piece at a fixed spawn position.
   * col=4, row=0 is roughly the top-centre of a 12-column field.
   * In Lesson 06 we'll replace (4, 0) with state->current_piece.x/y. */
  draw_piece(backbuffer,
             state->current_piece.index,
             4, 0,   /* fixed position — x/y added in Lesson 06 */
             get_tetromino_color(state->current_piece.index),
             state->current_piece.rotate_x_value);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_init — Reset all game state.
 * ═══════════════════════════════════════════════════════════════════════════ */
void game_init(GameState *state) {
  /* Seed the RNG once per game session.
   * JS: Math.random() is pre-seeded; in C you must call srand() yourself.
   * time(NULL) returns seconds since Unix epoch — different on every run. */
  srand((unsigned int)time(NULL));

  /* Start with the I-piece at 0° rotation */
  state->current_piece.index          = TETROMINO_I_IDX;
  state->current_piece.rotate_x_value = TETROMINO_R_0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_update — Main logic. Called once per frame.
 *
 * Lesson 05: minimal — just handles restart.
 * Movement, gravity, and line-clearing added in later lessons.
 * ═══════════════════════════════════════════════════════════════════════════ */
void game_update(GameState *state, GameInput *input, float delta_time) {
  /* Suppress unused-parameter warnings while we build up game_update. */
  (void)input;
  (void)delta_time;

  /* Restart: re-initialise all state if R was pressed. */
  if (state->should_restart) {
    game_init(state);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * tetromino_pos_value — Map (px, py, rotation) to string index.
 *
 * This is the core rotation math for the entire game.
 * See the ASCII rotation diagrams in the lesson notes above.
 * (see src/game.c:410-426 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r) {
    case TETROMINO_R_0:   return py * TETROMINO_LAYER_COUNT + px;
    case TETROMINO_R_90:  return 12 + py - (px * TETROMINO_LAYER_COUNT);
    case TETROMINO_R_180: return 15 - (py * TETROMINO_LAYER_COUNT) - px;
    case TETROMINO_R_270: return 3  - py + (px * TETROMINO_LAYER_COUNT);
  }
  return 0; /* unreachable — all enum values handled */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * prepare_input_frame — Carry button state into the next frame.
 *
 * Called by the platform BEFORE it reads new key events.
 * Copies ended_down from old frame; clears transition counts.
 * (see src/game.c:493-505 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
  for (int btn = 0; btn < BUTTON_COUNT; btn++) {
    current_input->buttons[btn].ended_down          = old_input->buttons[btn].ended_down;
    current_input->buttons[btn].half_transition_count = 0;
  }
}
```

---

## Build and Run

```bash
cd /path/to/course
./build-dev.sh -r
```

Or manually:
```bash
clang -Wall -Wextra -g -O0 \
  -o build/game \
  src/game.c src/audio.c \
  src/utils/draw-shapes.c src/utils/draw-text.c \
  src/main_raylib.c \
  -lraylib -lpthread -ldl -lm
./build/game
```

**Files changed in this lesson:** `src/game.h`, `src/game.c`  
**Files unchanged:** `src/main_raylib.c`, `src/platform.h`, `src/audio.c`, `src/utils/*`

---

## Expected Result

You should see a cyan I-piece (four squares in a vertical column) near the
top-left of the field area. The rest of the screen is black. Nothing moves.

Press `Q` to quit.

> **Common mistake:** Forgetting that `TETROMINOES[piece_index][pi]` indexes TWICE — first into the array of pointers (which piece), then into the string (which cell). If you write `TETROMINOES[pi]` you're indexing the wrong dimension, selecting a *different tetromino* instead of a different *cell*. The types don't catch this because both are valid array accesses.

> **Common mistake:** Writing `const char TETROMINO_I[17] = "..X...X...X...X."` — the `17` includes space for the NUL terminator `\0`. Our code accesses exactly indices 0–15, so the NUL at index 16 is harmless. But if you write `[15]`, the compiler will complain because the string literal is 16 characters. Use `TETROMINO_SIZE` (`16`) or let the compiler infer the size with `[]`.

---

## Exercises

1. **Change the starting piece.** In `game_init`, change `TETROMINO_I_IDX` to
   `TETROMINO_T_IDX` and rebuild. Verify the T-piece appears.

2. **Change the starting rotation.** Change `TETROMINO_R_0` to `TETROMINO_R_90`
   in `game_init`. What does the I-piece look like at 90°? Does it match the
   rotation diagram in Step 3?

3. **Add a second piece.** In `game_render`, after the first `draw_piece` call,
   add a second call with `TETROMINO_O_IDX` and a different position (e.g.,
   col=8, row=2). What colour does it appear?

4. **Verify rotation math.** Add `printf("%d\n", tetromino_pos_value(2, 2, TETROMINO_R_90))` inside `game_update` (include `<stdio.h>`). Does it print 6? Cross-check with the R_90 grid diagram in Step 3.

---

## What's Next

Lesson 06 adds two things: `(x, y)` position to `CurrentPiece`, and keyboard
input handling with DAS/ARR timing. The piece will move left, right, and down
with the arrow keys, and rotate with Z/X. We'll also implement
`tetromino_does_piece_fit()` so the piece can't pass through the field edges.
