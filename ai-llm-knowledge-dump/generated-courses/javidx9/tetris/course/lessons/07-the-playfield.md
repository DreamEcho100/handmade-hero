# Lesson 07: The Playfield

## What we're building

We introduce the playfield: a flat array of bytes that stores every locked
piece permanently. The game gains gravity (the piece falls automatically),
collision against locked pieces and walls, piece locking when it can't fall
further, new-piece spawning, and game-over detection.

```
  col: 0  1  2  3  4  5  6  7  8  9 10 11
row 0: [W][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][W]
row 1: [W][ ][ ][ ][I][ ][ ][ ][ ][ ][ ][W]   ← piece falling
row 2: [W][ ][ ][ ][I][ ][ ][ ][ ][ ][ ][W]
      ...
row16: [W][Z][Z][J][J][J][L][ ][ ][ ][ ][W]   ← locked pieces
row17: [W][W][W][W][W][W][W][W][W][W][W][W]   ← floor
       ↑ left wall                    ↑ right wall
```

## What you'll learn

- `unsigned char` as a compact 1-byte value (0–255)
- The `TETRIS_FIELD_CELL` enum and why values start at 1 for piece cells
- `memset` for fast bulk initialisation
- The gravity timer: accumulate delta_time, drop at interval, subtract remainder
- Piece locking: writing `piece_index + 1` into the field
- Spawning the next piece and detecting game over

## Prerequisites

- Lesson 06 complete — piece moves with keyboard input
- You understand `tetromino_does_piece_fit()` and the 2D-in-1D index formula

---

## Step 1: `TETRIS_FIELD_CELL` — What Lives in Each Field Cell

Each cell of the field can hold one of several values. We model this as an enum:

```c
typedef enum {
  TETRIS_FIELD_EMPTY,   /* 0 — nothing here                             */
  TETRIS_FIELD_I,       /* 1 — locked I-piece cell                      */
  TETRIS_FIELD_J,       /* 2 — locked J-piece cell                      */
  TETRIS_FIELD_L,       /* 3 — locked L-piece cell                      */
  TETRIS_FIELD_O,       /* 4 — locked O-piece cell                      */
  TETRIS_FIELD_S,       /* 5 — locked S-piece cell                      */
  TETRIS_FIELD_T,       /* 6 — locked T-piece cell                      */
  TETRIS_FIELD_Z,       /* 7 — locked Z-piece cell                      */
  TETRIS_FIELD_WALL,    /* 8 — permanent boundary                       */
} TETRIS_FIELD_CELL;
```

Notice that **piece values start at 1** (`TETRIS_FIELD_I = 1`), not 0. Zero is
reserved for empty. This choice is intentional:

```
Locking the I-piece: field[fi] = TETROMINO_I_IDX + 1 = 0 + 1 = 1 = TETRIS_FIELD_I
Locking the Z-piece: field[fi] = TETROMINO_Z_IDX + 1 = 6 + 1 = 7 = TETRIS_FIELD_Z
```

When rendering, we recover the piece index to look up the colour:

```c
get_tetromino_color(cell_value - 1)
// TETRIS_FIELD_I - 1 = 0 = TETROMINO_I_IDX → COLOR_CYAN
// TETRIS_FIELD_Z - 1 = 6 = TETROMINO_Z_IDX → COLOR_RED
```

> **Why enum values as array indices?** In JavaScript you'd use a `Map` or a
> lookup object: `const COLORS = { [TETROMINO_I_IDX]: 'cyan', ... }`. In C,
> when an enum starts at 0 and increments by 1, its values ARE array indices.
> `get_tetromino_color(cell_value - 1)` is equivalent to `COLORS[cellValue - 1]`.
> The `- 1` shift is the only "trick" — it maps field cell values (1–7) to
> piece indices (0–6). Zero stays "empty."

---

## Step 2: `unsigned char field[]` — The Playfield Array

The entire field is a flat 1D array of bytes:

```c
unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
// FIELD_WIDTH * FIELD_HEIGHT = 12 * 18 = 216 bytes
```

> **Why `unsigned char` instead of `int`?** Our cell values are 0–8 (see
> `TETRIS_FIELD_CELL`). An `int` is typically 4 bytes, wasting 3 bytes per
> cell. `unsigned char` is exactly 1 byte and holds values 0–255 — more than
> enough. In JavaScript, the equivalent is `new Uint8Array(FIELD_WIDTH * FIELD_HEIGHT)`.
> A `Uint8Array` element is also 1 byte per cell. The difference: in C we just
> declare the array size and the compiler allocates it in `GameState` on the
> stack. No heap allocation, no `new`, no GC pressure.

> **Handmade Hero principle:** Fixed arrays, not dynamic allocation. `unsigned char field[W*H]` is part of `GameState`, which sits on the stack in `main`. It's allocated once when the program starts and freed automatically when `main` returns. There are no heap allocations in the game layer — no `malloc`, no `free`, no fragmentation.

The access formula, repeated from Lesson 01:

```c
field[row * FIELD_WIDTH + col]
// row=5, col=3: field[5 * 12 + 3] = field[63]
```

In `GameState`, the field sits alongside the gravity timer in a new sub-struct:

```c
typedef struct {
  /* ... existing fields from Lesson 06 ... */

  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]; /* NEW */
  bool is_game_over;                               /* NEW */

  struct {
    RepeatInterval rotate_direction; /* gravity timer (misnamed legacy field) */
  } input_values;                    /* NEW */
} GameState;
```

The `rotate_direction` name is a historical quirk in the reference code — it
actually stores the gravity drop timer. We keep the name for compatibility with
the final codebase.

---

## Step 3: Wall Initialisation in `game_init()`

After declaring the field, we need to fill it. The playable interior starts
empty; the border is all walls.

```c
for (int y = 0; y < FIELD_HEIGHT; y++) {
  for (int x = 0; x < FIELD_WIDTH; x++) {
    if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) {
      state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_WALL;
    } else {
      state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_EMPTY;
    }
  }
}
```

The conditions:
- `x == 0` → left wall column
- `x == FIELD_WIDTH - 1` (x == 11) → right wall column
- `y == FIELD_HEIGHT - 1` (y == 17) → floor row

There is no ceiling wall — pieces can spawn at row 0 and pieces can hang above
the visible field during spawn.

> **Why not use `memset`?** `memset(state->field, 0, sizeof(state->field))` would
> clear all cells to 0 (EMPTY) very efficiently — a single instruction that fills
> 216 bytes. We could use it to zero-fill first, then set walls in a second pass.
> But the explicit loop is clearer for learning. In `game_init` we also reset other
> state alongside the field, so a combined loop is fine. `memset` is most useful
> when you need to zero a large block fast without any conditional logic —
> for example, clearing `completed_lines.indexes` with
> `memset(&state->completed_lines.indexes, 0, sizeof(int) * TETROMINO_LAYER_COUNT)`.
> (see `src/game.c:315` in the final version)

---

## Step 4: Drawing the Field in `game_render()`

Now that the field exists, `game_render` draws each non-empty cell:

```c
for (int row = 0; row < FIELD_HEIGHT; row++) {
  for (int col = 0; col < FIELD_WIDTH; col++) {
    unsigned char cell_value = state->field[row * FIELD_WIDTH + col];

    switch (cell_value) {
      case TETRIS_FIELD_EMPTY:
        continue; /* nothing to draw */

      case TETRIS_FIELD_I: case TETRIS_FIELD_J: case TETRIS_FIELD_L:
      case TETRIS_FIELD_O: case TETRIS_FIELD_S: case TETRIS_FIELD_T:
      case TETRIS_FIELD_Z:
        draw_cell(backbuffer, col, row, get_tetromino_color(cell_value - 1));
        break;

      case TETRIS_FIELD_WALL:
        draw_cell(backbuffer, col, row, COLOR_GRAY);
        break;
    }
  }
}
```

The field is drawn first (locked pieces and walls), then the active piece on
top — this is the **painter's algorithm**: draw background layers first, then
foreground. (See `src/game.c:155-199` in the final version.)

---

## Step 5: Upgrading `tetromino_does_piece_fit()` to Check the Field

In Lesson 06, `does_piece_fit` only checked bounds. Now it checks the actual
field cells too:

```c
int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y) {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece][pi] == TETROMINO_SPAN) continue;

      int field_x = pos_x + px;
      int field_y = pos_y + py;

      /* Allow cells above the top (spawn overhang) */
      if (field_x < 0 || field_x >= FIELD_WIDTH)  continue;
      if (field_y < 0 || field_y >= FIELD_HEIGHT)  continue;

      int fi = field_y * FIELD_WIDTH + field_x;

      /* Any non-empty field cell (wall, locked piece, floor) = collision */
      if (state->field[fi] != TETRIS_FIELD_EMPTY)
        return 0;
    }
  }
  return 1;
}
```

The key change: we no longer special-case walls with `< 1` and `>= FIELD_WIDTH - 1`.
Instead we let the field cells speak for themselves — the walls are stored as
`TETRIS_FIELD_WALL` in the array, so `field[fi] != TETRIS_FIELD_EMPTY` catches
them automatically. (See `src/game.c:432-490` in the final version.)

> **Common mistake:** Using `<= 0` instead of `< 0` in the out-of-bounds check.
> Field column 0 IS a valid field index (the left wall). If you skip it, pieces
> can walk through the left wall. The correct guard is `field_x < 0` (truly
> off the left edge of the array) — the left wall at `field_x == 0` will be
> caught by the `field[fi] != TETRIS_FIELD_EMPTY` check because `field[0] = TETRIS_FIELD_WALL`.

---

## Step 6: Gravity — The Drop Timer

Gravity is a timer that fires every N seconds. We reuse the `RepeatInterval`
struct (already in the codebase) for consistency:

```c
/* In game_init: */
state->input_values.rotate_direction.timer    = 0.0f;
state->input_values.rotate_direction.interval = 0.8f; /* 0.8 s per drop at level 0 */

/* In game_update (after input): */
state->input_values.rotate_direction.timer += delta_time;

float drop_interval = state->input_values.rotate_direction.interval;
/* Level scaling: each level adds ~10ms of speed */
/* (level scaling added fully in Lesson 08) */

if (state->input_values.rotate_direction.timer >= drop_interval) {
  state->input_values.rotate_direction.timer -= drop_interval; /* keep remainder */

  if (tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x,
                               state->current_piece.y + 1)) {
    state->current_piece.y++; /* fall one row */
  } else {
    /* PIECE LOCKED — lock it into the field */
  }
}
```

> **Why `timer -= drop_interval` instead of `timer = 0`?**
> Imagine `drop_interval = 0.8s` and your frame took `0.85s` (big spike).
> `timer = 0` discards the 0.05s overshoot — the next drop is 0.05s late.
> `timer -= 0.8` leaves `timer = 0.05` — the next drop fires at the correct time.
> This technique (keeping the accumulator's remainder) is used everywhere in
> time-based game logic. You'll see it in particle systems, animation timers,
> and network tick rates. The JS equivalent: `timer %= interval`.

---

## Step 7: Piece Locking and Spawning

When the piece can't fall further, we "stamp" it into the field array and spawn
a new piece:

```c
/* Stamp the piece into the field */
for (int px = 0; px < TETROMINO_LAYER_COUNT; ++px) {
  for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
    int pi = tetromino_pos_value(px, py, state->current_piece.rotate_x_value);
    if (TETROMINOES[state->current_piece.index][pi] != TETROMINO_SPAN) {
      int fx = state->current_piece.x + px;
      int fy = state->current_piece.y + py;
      if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT) {
        /* Store piece_index + 1 so that 0 stays "empty" */
        state->field[fy * FIELD_WIDTH + fx] = state->current_piece.index + 1;
      }
    }
  }
}
```

The `+ 1` offset: `TETROMINO_I_IDX = 0`, but `TETRIS_FIELD_EMPTY = 0` too.
We can't use 0 to mean both "empty" and "I-piece locked here". Adding 1 shifts
piece values to 1–7, leaving 0 free for empty.

After locking, spawn the next piece:

```c
state->input_values.rotate_direction.timer = 0.0f;
state->current_piece = (CurrentPiece){
  .x              = (int)((FIELD_WIDTH  * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
  .y              = 0,
  .index          = state->current_piece.next_index,   /* promote next */
  .next_index     = rand() % TETROMINOS_COUNT,          /* draw new next */
  .rotate_x_value = TETROMINO_R_0,
};
```

Then immediately check if the new piece fits:

```c
if (!tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x,
                               state->current_piece.y)) {
  state->is_game_over = true;
}
```

If the new piece doesn't fit at spawn position, the field is full — game over.
(See `src/game.c:795-810` in the final version.)

---

## Step 8: Complete `game.h` for Lesson 07

Replace `src/game.h` with this complete file:

```c
/* src/game.h — Lesson 07: The Playfield */
#ifndef GAME_H
#define GAME_H

#include "utils/audio.h"
#include "utils/backbuffer.h"
#include <stdbool.h>
#include <stdint.h>

/* ── Constants ── */
#define FIELD_WIDTH   12
#define FIELD_HEIGHT  18
#define CELL_SIZE     30
#define SIDEBAR_WIDTH 200

#define TETROMINO_SPAN        '.'
#define TETROMINO_BLOCK       'X'
#define TETROMINO_LAYER_COUNT 4
#define TETROMINO_SIZE        16
#define TETROMINOS_COUNT      7

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

/* ── Piece Enums ── */
typedef enum {
  TETROMINO_I_IDX, TETROMINO_J_IDX, TETROMINO_L_IDX, TETROMINO_O_IDX,
  TETROMINO_S_IDX, TETROMINO_T_IDX, TETROMINO_Z_IDX,
} TETROMINO_BY_IDX;

/* NEW in Lesson 07: what value lives in each field cell */
typedef enum {
  TETRIS_FIELD_EMPTY,    /* 0 — nothing here                     */
  TETRIS_FIELD_I,        /* 1 — locked I-piece (TETROMINO_I_IDX + 1) */
  TETRIS_FIELD_J,        /* 2 — locked J-piece                   */
  TETRIS_FIELD_L,        /* 3 — locked L-piece                   */
  TETRIS_FIELD_O,        /* 4 — locked O-piece                   */
  TETRIS_FIELD_S,        /* 5 — locked S-piece                   */
  TETRIS_FIELD_T,        /* 6 — locked T-piece                   */
  TETRIS_FIELD_Z,        /* 7 — locked Z-piece                   */
  TETRIS_FIELD_WALL,     /* 8 — permanent boundary               */
  /* TETRIS_FIELD_TMP_FLASH = 9 added in Lesson 08 */
} TETRIS_FIELD_CELL;

typedef enum {
  TETROMINO_R_0, TETROMINO_R_90, TETROMINO_R_180, TETROMINO_R_270,
} TETROMINO_R_DIR;

typedef enum {
  TETROMINO_ROTATE_X_NONE,
  TETROMINO_ROTATE_X_GO_LEFT,
  TETROMINO_ROTATE_X_GO_RIGHT,
} TETROMINO_ROTATE_X_VALUE;

/* ── RepeatInterval ── */
typedef struct {
  float timer;
  float initial_delay;
  float interval;
  bool  is_active;
  bool  passed_initial_delay;
} RepeatInterval;

/* ── CurrentPiece ── */
typedef struct {
  int x;
  int y;
  TETROMINO_BY_IDX          index;
  TETROMINO_BY_IDX          next_index;
  TETROMINO_R_DIR           rotate_x_value;
  TETROMINO_ROTATE_X_VALUE  next_rotate_x_value;
} CurrentPiece;

/* ── Input Types ── */
typedef struct {
  int half_transition_count;
  int ended_down;
} GameButtonState;

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

/* ── GameState ── */
typedef struct {

  /* NEW in Lesson 07 */
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
  bool is_game_over;

  CurrentPiece current_piece;

  struct {
    RepeatInterval move_left;
    RepeatInterval move_right;
    RepeatInterval move_down;
  } input_repeat;

  /* NEW in Lesson 07: gravity drop timer */
  struct {
    RepeatInterval rotate_direction; /* stores gravity timer; name is legacy */
  } input_values;

  GameAudioState audio;
  int should_quit;
  int should_restart;

} GameState;

/* ── Tetromino data ── */
extern const char *TETROMINOES[7];

/* ── Function Declarations ── */
void game_init(GameState *state);
void prepare_input_frame(GameInput *old_input, GameInput *new_input);
int  tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);
int  tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                              int pos_x, int pos_y);
void game_update(GameState *state, GameInput *input, float delta_time);
void game_render(Backbuffer *backbuffer, GameState *state);

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

## Step 9: Complete `game.c` for Lesson 07

Replace `src/game.c` with this complete file:

```c
/* src/game.c — Lesson 07: The Playfield */

#include "game.h"
#include "utils/draw-shapes.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Tetromino Data ── */
const char TETROMINO_I[TETROMINO_SIZE] = "..X...X...X...X.";
const char TETROMINO_J[TETROMINO_SIZE] = "..X...X..XX.....";
const char TETROMINO_L[TETROMINO_SIZE] = ".X...X...XX.....";
const char TETROMINO_O[TETROMINO_SIZE] = ".....XX..XX.....";
const char TETROMINO_S[TETROMINO_SIZE] = ".X...XX...X.....";
const char TETROMINO_T[TETROMINO_SIZE] = "..X..XX...X.....";
const char TETROMINO_Z[TETROMINO_SIZE] = "..X..XX..X......";

const char *TETROMINOES[TETROMINOS_COUNT] = {
  TETROMINO_I, TETROMINO_J, TETROMINO_L, TETROMINO_O,
  TETROMINO_S, TETROMINO_T, TETROMINO_Z,
};

/* ── Static Helpers ── */
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

static void draw_cell(Backbuffer *bb, int col, int row, uint32_t color) {
  draw_rect(bb, col * CELL_SIZE + 1, row * CELL_SIZE + 1,
            CELL_SIZE - 2, CELL_SIZE - 2, color);
}

static void draw_piece(Backbuffer *bb, int piece_index, int field_col,
                       int field_row, uint32_t color, TETROMINO_R_DIR rotation) {
  for (int py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (int px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
        draw_cell(bb, field_col + px, field_row + py, color);
      }
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_render
 * NEW: Draws field cells (walls + locked pieces) before the active piece.
 * (see src/game.c:125-290 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
void game_render(Backbuffer *backbuffer, GameState *state) {
  /* 1. Clear to black */
  for (int i = 0; i < backbuffer->width * backbuffer->height; i++) {
    backbuffer->pixels[i] = COLOR_BLACK;
  }

  /* 2. Draw locked field cells (walls + locked pieces) */
  for (int row = 0; row < FIELD_HEIGHT; row++) {
    for (int col = 0; col < FIELD_WIDTH; col++) {
      unsigned char cell_value = state->field[row * FIELD_WIDTH + col];

      switch (cell_value) {
        case TETRIS_FIELD_EMPTY:
          continue;

        case TETRIS_FIELD_I: case TETRIS_FIELD_J: case TETRIS_FIELD_L:
        case TETRIS_FIELD_O: case TETRIS_FIELD_S: case TETRIS_FIELD_T:
        case TETRIS_FIELD_Z:
          /* cell_value - 1 recovers TETROMINO_BY_IDX for colour lookup */
          draw_cell(backbuffer, col, row, get_tetromino_color(cell_value - 1));
          break;

        case TETRIS_FIELD_WALL:
          draw_cell(backbuffer, col, row, COLOR_GRAY);
          break;
      }
    }
  }

  /* 3. Draw the active falling piece on top of the field */
  if (!state->is_game_over) {
    draw_piece(backbuffer,
               state->current_piece.index,
               state->current_piece.x,
               state->current_piece.y,
               get_tetromino_color(state->current_piece.index),
               state->current_piece.rotate_x_value);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_init
 * NEW: Initialises field with walls, sets up gravity timer.
 * (see src/game.c:294-360 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
void game_init(GameState *state) {
  srand((unsigned int)time(NULL));

  state->is_game_over = false;

  /* Build field: walls on left (col 0), right (col 11), floor (row 17).
   * All other cells start empty.                                             */
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) {
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_WALL;
      } else {
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_EMPTY;
      }
    }
  }

  /* Gravity timer: drop every 0.8 seconds */
  state->input_values.rotate_direction.timer    = 0.0f;
  state->input_values.rotate_direction.interval = 0.8f;

  /* DAS/ARR for movement */
  state->input_repeat.move_left.initial_delay        = 0.15f;
  state->input_repeat.move_left.interval             = 0.05f;
  state->input_repeat.move_left.passed_initial_delay = false;

  state->input_repeat.move_right.initial_delay        = 0.15f;
  state->input_repeat.move_right.interval             = 0.05f;
  state->input_repeat.move_right.passed_initial_delay = false;

  state->input_repeat.move_down.initial_delay        = 0.0f;
  state->input_repeat.move_down.interval             = 0.03f;
  state->input_repeat.move_down.passed_initial_delay = false;

  /* Spawn first piece */
  state->current_piece = (CurrentPiece){
    .x              = (int)((FIELD_WIDTH  * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
    .y              = 0,
    .index          = rand() % TETROMINOS_COUNT,
    .next_index     = rand() % TETROMINOS_COUNT,
    .rotate_x_value = TETROMINO_R_0,
    .next_rotate_x_value = TETROMINO_ROTATE_X_NONE,
  };
}

/* ── tetromino_pos_value (unchanged) ── */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r) {
    case TETROMINO_R_0:   return py * TETROMINO_LAYER_COUNT + px;
    case TETROMINO_R_90:  return 12 + py - (px * TETROMINO_LAYER_COUNT);
    case TETROMINO_R_180: return 15 - (py * TETROMINO_LAYER_COUNT) - px;
    case TETROMINO_R_270: return 3  - py + (px * TETROMINO_LAYER_COUNT);
  }
  return 0;
}

/* ── prepare_input_frame (unchanged) ── */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
  for (int btn = 0; btn < BUTTON_COUNT; btn++) {
    current_input->buttons[btn].ended_down          = old_input->buttons[btn].ended_down;
    current_input->buttons[btn].half_transition_count = 0;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * tetromino_does_piece_fit
 * UPGRADED: now checks field cells as well as bounds.
 * (see src/game.c:432-490 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y) {
  if (piece < 0 || piece >= TETROMINOS_COUNT) return 0;

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece][pi] == TETROMINO_SPAN) continue;

      int field_x = pos_x + px;
      int field_y = pos_y + py;

      /* Out-of-bounds cells above the top are allowed (spawn overhang) */
      if (field_x < 0 || field_x >= FIELD_WIDTH)  continue;
      if (field_y < 0 || field_y >= FIELD_HEIGHT)  continue;

      /* Any non-empty cell: wall, locked piece, floor → collision */
      if (state->field[field_y * FIELD_WIDTH + field_x] != TETRIS_FIELD_EMPTY)
        return 0;
    }
  }
  return 1;
}

/* ── handle_action_with_repeat (static, unchanged from Lesson 06) ── */
static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval   *repeat,
                                      float             delta_time,
                                      int              *should_trigger) {
  *should_trigger = 0;

  if (!button->ended_down) {
    repeat->timer = 0.0f; repeat->is_active = false;
    repeat->passed_initial_delay = false;
    return;
  }
  if (button->half_transition_count > 0) {
    repeat->timer = 0.0f; repeat->is_active = true;
    repeat->passed_initial_delay = false;
    if (repeat->initial_delay <= 0.0f) *should_trigger = 1;
    return;
  }
  if (!repeat->is_active) return;
  repeat->timer += delta_time;
  if (!repeat->passed_initial_delay) {
    if (repeat->timer >= repeat->initial_delay) {
      *should_trigger = 1; repeat->timer = 0.0f;
      repeat->passed_initial_delay = true;
    }
  } else {
    if (repeat->timer >= repeat->interval) {
      *should_trigger = 1;
      repeat->timer  -= repeat->interval;
    }
  }
}

/* ── tetris_apply_input (static, unchanged from Lesson 06 except state is now used) ── */
static void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {
  /* Rotation */
  if (input->rotate_x.ended_down &&
      input->rotate_x.half_transition_count != 0 &&
      state->current_piece.next_rotate_x_value != TETROMINO_ROTATE_X_NONE) {

    TETROMINO_R_DIR new_rotation;
    if (state->current_piece.next_rotate_x_value == TETROMINO_ROTATE_X_GO_RIGHT) {
      switch (state->current_piece.rotate_x_value) {
        case TETROMINO_R_0:   new_rotation = TETROMINO_R_90;  break;
        case TETROMINO_R_90:  new_rotation = TETROMINO_R_180; break;
        case TETROMINO_R_180: new_rotation = TETROMINO_R_270; break;
        case TETROMINO_R_270: new_rotation = TETROMINO_R_0;   break;
        default:              new_rotation = TETROMINO_R_0;   break;
      }
    } else {
      switch (state->current_piece.rotate_x_value) {
        case TETROMINO_R_0:   new_rotation = TETROMINO_R_270; break;
        case TETROMINO_R_90:  new_rotation = TETROMINO_R_0;   break;
        case TETROMINO_R_180: new_rotation = TETROMINO_R_90;  break;
        case TETROMINO_R_270: new_rotation = TETROMINO_R_180; break;
        default:              new_rotation = TETROMINO_R_0;   break;
      }
    }
    if (tetromino_does_piece_fit(state, state->current_piece.index, new_rotation,
                                 state->current_piece.x, state->current_piece.y)) {
      state->current_piece.rotate_x_value = new_rotation;
    }
  }

  /* Horizontal movement */
  int should_move_left = 0, should_move_right = 0;
  handle_action_with_repeat(&input->move_left,  &state->input_repeat.move_left,
                             delta_time, &should_move_left);
  handle_action_with_repeat(&input->move_right, &state->input_repeat.move_right,
                             delta_time, &should_move_right);

  if (should_move_left &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x - 1, state->current_piece.y))
    state->current_piece.x--;

  if (should_move_right &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x + 1, state->current_piece.y))
    state->current_piece.x++;

  /* Soft drop */
  int should_move_down = 0;
  handle_action_with_repeat(&input->move_down, &state->input_repeat.move_down,
                             delta_time, &should_move_down);
  if (should_move_down &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x, state->current_piece.y + 1))
    state->current_piece.y++;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_update
 * NEW: gravity, piece locking, spawning, game-over detection.
 * (see src/game.c:680-922 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
void game_update(GameState *state, GameInput *input, float delta_time) {
  game_audio_update(&state->audio, delta_time);

  /* Handle restart (works on game over screen too) */
  if (state->is_game_over) {
    if (state->should_restart) {
      game_init(state);
      game_music_play(&state->audio);
    }
    return;
  }

  if (state->should_restart) {
    game_init(state);
    return;
  }

  /* Player input */
  tetris_apply_input(state, input, delta_time);

  /* Gravity timer */
  state->input_values.rotate_direction.timer += delta_time;

  /* Drop interval: 0.8s base. Level scaling added in Lesson 08. */
  float drop_interval = state->input_values.rotate_direction.interval;

  if (state->input_values.rotate_direction.timer >= drop_interval) {
    state->input_values.rotate_direction.timer -= drop_interval;

    /* Try to fall one row */
    if (tetromino_does_piece_fit(state, state->current_piece.index,
                                 state->current_piece.rotate_x_value,
                                 state->current_piece.x,
                                 state->current_piece.y + 1)) {
      state->current_piece.y++;

    } else {
      /* ── Piece Locked ── */

      /* Stamp piece into field: store piece_index + 1 (0 = empty sentinel) */
      for (int px = 0; px < TETROMINO_LAYER_COUNT; ++px) {
        for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
          int pi = tetromino_pos_value(px, py, state->current_piece.rotate_x_value);
          if (TETROMINOES[state->current_piece.index][pi] != TETROMINO_SPAN) {
            int fx = state->current_piece.x + px;
            int fy = state->current_piece.y + py;
            if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT) {
              state->field[fy * FIELD_WIDTH + fx] =
                  (unsigned char)(state->current_piece.index + 1);
            }
          }
        }
      }

      game_play_sound_at(&state->audio, SOUND_DROP,
                         ((float)state->current_piece.x / FIELD_WIDTH) * 2.0f - 1.0f);

      /* Line clearing will be added in Lesson 08 */

      /* Reset gravity timer and spawn next piece */
      state->input_values.rotate_direction.timer = 0.0f;
      state->current_piece = (CurrentPiece){
        .x              = (int)((FIELD_WIDTH  * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
        .y              = 0,
        .index          = state->current_piece.next_index,
        .next_index     = rand() % TETROMINOS_COUNT,
        .rotate_x_value = TETROMINO_R_0,
        .next_rotate_x_value = TETROMINO_ROTATE_X_NONE,
      };

      /* Game over: new piece doesn't fit at spawn position */
      if (!tetromino_does_piece_fit(state, state->current_piece.index,
                                    state->current_piece.rotate_x_value,
                                    state->current_piece.x,
                                    state->current_piece.y)) {
        state->is_game_over = true;
        game_music_stop(&state->audio);
      }
    }
  }
}
```

---

## Build and Run

```bash
./build-dev.sh -r
```

**Files changed:** `src/game.h`, `src/game.c`

---

## Expected Result

- You see gray walls on the left, right, and bottom of the field area.
- A random piece spawns at the top and falls every 0.8 seconds.
- You can move it left/right and rotate it. It stops when it hits the walls or floor.
- When the piece locks, a new piece spawns immediately.
- When the field fills to the top and a piece can't spawn, the active piece
  disappears and further input is ignored (game over — no overlay yet, that's
  Lesson 08).
- No line clearing yet — completed rows stay filled.

> **Common mistake:** The piece passes through the left/right walls after upgrading `tetromino_does_piece_fit`. Check that you removed the old bounds-only version entirely. The new version relies on `field[fi] != TETRIS_FIELD_EMPTY` — this only works if `game_init` actually wrote `TETRIS_FIELD_WALL` into columns 0 and 11. If the field is all zeros, the wall check silently passes.

> **Common mistake:** Gravity fires twice on the first frame. This happens if `drop_interval` is 0 or negative because `input_values.rotate_direction.interval` was not initialised. Make sure `game_init` sets `.interval = 0.8f`.

---

## Exercises

1. **Speed it up.** Change the drop interval in `game_init` from `0.8f` to `0.1f`.
   Does the piece fall at 10 Hz? Now try `0.01f` — the piece should lock
   immediately on spawn. Why? (The piece's starting y=0, floor is at y=17 — 17
   drops faster than one frame.)

2. **Draw the next piece.** In `game_render`, after drawing the current piece,
   add a `draw_piece` call using `state->current_piece.next_index` at a fixed
   sidebar position (e.g., col=14, row=4). You'll need to choose a side panel
   position that's visible. Full HUD comes in Lesson 08.

3. **Fill the field manually.** In `game_init`, after the wall loop, add:
   ```c
   for (int col = 1; col < FIELD_WIDTH - 1; col++) {
     if (col != 5) state->field[16 * FIELD_WIDTH + col] = TETRIS_FIELD_Z;
   }
   ```
   This fills row 16 with Z-pieces leaving a gap at col 5. Run the game — does
   the first piece lock on top of this row? Does the locked piece use the
   correct colour?

---

## What's Next

Lesson 08 completes the game loop: after a piece locks, we scan the covered rows
for complete lines, mark them white (`TETRIS_FIELD_TMP_FLASH`), wait 400 ms,
then collapse them from bottom to top. We also add scoring, level progression,
and the full HUD sidebar with score, level, and next-piece preview.
