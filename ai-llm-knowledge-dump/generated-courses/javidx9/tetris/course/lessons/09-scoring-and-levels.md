# Lesson 09: Scoring & Levels

## What we're building

Every locked piece awards **25 points**. Clearing multiple lines simultaneously multiplies the reward exponentially — a Tetris (4 lines) is worth 1600 points. The game also tracks how many pieces you've placed and levels up every 25 pieces, speeding up the drop timer so the game gets harder over time.

## What you'll learn

- Adding fields to an existing `struct` in C
- Bit-shift operator `1 << n` — the power-of-2 scoring formula
- Modulo `%` operator for periodic events (every N pieces)
- Why we subtract from the timer instead of resetting it to zero
- How C enums prevent "magic number" bugs in game logic

## Prerequisites

Lesson 08 complete: you have a working Tetris with piece gravity, DAS/ARR input, flash animation, and line collapse. The game loop runs but there is no score display, no level progression, and the drop speed never changes.

---

## Step 1: Add Score Fields to `GameState`

Open `src/game.h`. Find the `GameState` struct and add three new fields.

> **Why?** In JavaScript you'd write `let score = 0` at module scope. In C **all mutable state lives in one struct** (`GameState`) rather than scattered in global variables. This is the Handmade Hero principle: "State is always visible." When you want to know every variable the game can touch, there is exactly one place to look.

> **Handmade Hero principle:** Enums prevent category errors. Using `GAME_PHASE` or named enum constants instead of raw integers like `phase == 2` means the compiler can warn you when you accidentally mix up two different numeric domains (e.g., assigning a field-cell value to a piece-index variable).

**`src/game.h`** — complete file at this stage:

```c
#ifndef GAME_H
#define GAME_H

#include "utils/backbuffer.h"
#include <stdbool.h>
#include <stdint.h>

/* ── Field & Cell Constants ─────────────────────────────────────────────── */
#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30
#define SIDEBAR_WIDTH 200

/* ── Tetromino Shape Constants ──────────────────────────────────────────── */
#define TETROMINO_SPAN        '.'
#define TETROMINO_BLOCK       'X'
#define TETROMINO_LAYER_COUNT 4
#define TETROMINO_SIZE        16
#define TETROMINOS_COUNT      7

/* ── Predefined Colors ──────────────────────────────────────────────────── */
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

/* ── Tetromino type enums ───────────────────────────────────────────────── */
typedef enum {
  TETROMINO_I_IDX,
  TETROMINO_J_IDX,
  TETROMINO_L_IDX,
  TETROMINO_O_IDX,
  TETROMINO_S_IDX,
  TETROMINO_T_IDX,
  TETROMINO_Z_IDX,
} TETROMINO_BY_IDX;

typedef enum {
  TETRIS_FIELD_EMPTY,
  TETRIS_FIELD_I,
  TETRIS_FIELD_J,
  TETRIS_FIELD_L,
  TETRIS_FIELD_O,
  TETRIS_FIELD_S,
  TETRIS_FIELD_T,
  TETRIS_FIELD_Z,
  TETRIS_FIELD_WALL,
  TETRIS_FIELD_TMP_FLASH,
} TETRIS_FIELD_CELL;

typedef enum {
  TETROMINO_R_0,
  TETROMINO_R_90,
  TETROMINO_R_180,
  TETROMINO_R_270,
} TETROMINO_R_DIR;

typedef enum {
  TETROMINO_ROTATE_X_NONE,
  TETROMINO_ROTATE_X_GO_LEFT,
  TETROMINO_ROTATE_X_GO_RIGHT,
} TETROMINO_ROTATE_X_VALUE;

/* ── RepeatInterval (DAS/ARR) ───────────────────────────────────────────── */
typedef struct {
  float timer;
  float initial_delay;
  float interval;
  bool is_active;
  bool passed_initial_delay;
} RepeatInterval;

/* ── CurrentPiece ───────────────────────────────────────────────────────── */
typedef struct {
  int x;
  int y;
  TETROMINO_BY_IDX index;
  TETROMINO_BY_IDX next_index;
  TETROMINO_R_DIR rotate_x_value;
  TETROMINO_ROTATE_X_VALUE next_rotate_x_value;
} CurrentPiece;

/* ── Input ──────────────────────────────────────────────────────────────── */
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

/* ═══════════════════════════════════════════════════════════════════════════
 * GameState — complete mutable state of the Tetris game
 *
 * NEW THIS LESSON: score, pieces_count, level
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {

  /* ── Play Field ── */
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];

  /* ── Active Piece ── */
  CurrentPiece current_piece;

  /* ── NEW: Scoring & Progression ─────────────────────────────────────────
   * score:        accumulated points (25 per lock + bonus for lines)
   * pieces_count: total locked pieces; triggers level-up at multiples of 25
   * level:        current difficulty; 0 = normal, higher = faster drops
   *
   * JS equivalent:
   *   state.score       = 0;
   *   state.piecesCount = 0;
   *   state.level       = 0;
   */
  int score;
  int pieces_count;
  int level;

  /* ── Line Clear Animation ── */
  struct {
    int indexes[TETROMINO_LAYER_COUNT];
    int count;
    RepeatInterval flash_timer;
  } completed_lines;

  /* ── Input Auto-Repeat (DAS/ARR) ── */
  struct {
    RepeatInterval move_left;
    RepeatInterval move_right;
    RepeatInterval move_down;
  } input_repeat;

  /* ── Gravity Timer ── */
  struct {
    RepeatInterval rotate_direction;
  } input_values;

  /* ── Platform Signals ── */
  int should_quit;
  int should_restart;

} GameState;

/* ── Tetromino Shape Data ───────────────────────────────────────────────── */
extern const char *TETROMINOES[7];

/* ── Function Declarations ──────────────────────────────────────────────── */
void game_init(GameState *state);
void prepare_input_frame(GameInput *old_input, GameInput *new_input);
int  tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);
int  tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                               int pos_x, int pos_y);
void game_update(GameState *state, GameInput *input, float delta_time);
void game_render(Backbuffer *backbuffer, GameState *state);

#endif /* GAME_H */
```

---

## Step 2: Initialize the New Fields in `game_init`

In `game_init`, set `score`, `pieces_count`, and `level` to their starting values. This ensures restart always works cleanly — if the player presses R, `game_init` is called again and every field goes back to zero.

> **Common mistake:** Forgetting to reset `score` and `level` in `game_init`. Without it, restarting the game inherits the previous session's score. Always initialize every field of `GameState` in one function — that's your single source of truth for "what does a fresh game look like?"

---

## Step 3: Add Scoring Logic to `game_update`

After a piece locks and line detection finishes, we add two blocks. The first calculates points; the second checks for a level-up.

### The bit-shift scoring formula

```
1 line:  (1 << 1) * 100 = 200 points
2 lines: (1 << 2) * 100 = 400 points
3 lines: (1 << 3) * 100 = 800 points
4 lines: (1 << 4) * 100 = 1600 points  ← "Tetris!"
```

> **Why `1 << n` instead of `pow(2, n)`?**
> `1 << n` is the integer bit-shift operator. It shifts the bit pattern of `1` left by `n` positions:
> ```
> 1 in binary:   00000001
> 1 << 1:        00000010  =  2
> 1 << 2:        00000100  =  4
> 1 << 3:        00001000  =  8
> 1 << 4:        00010000  = 16
> ```
> In JavaScript you can write `1 << n` or `2 ** n` or `Math.pow(2, n)` — they produce identical results for small `n`. In C, `1 << n` is preferred for integer math: it's a single CPU instruction (no floating-point needed) and it's exact.

### The level-up formula

```c
if (state->pieces_count % 25 == 0) {
    state->level++;
}
```

> **Why `%` (modulo)?**
> `a % b` gives the remainder of `a / b`. It's exactly the same as JavaScript's `%` operator:
> ```js
> 25 % 25 === 0  // true  → level up!
> 26 % 25 === 1  // false → not yet
> 50 % 25 === 0  // true  → level up again!
> ```
> Every 25 pieces, `pieces_count % 25` equals zero, triggering a level increment.

### The drop interval formula

```c
int game_speed = state->level;
float drop_interval = state->input_values.rotate_direction.interval
                    - (0.01f + game_speed * 0.01f);
if (drop_interval < 0.1f) drop_interval = 0.1f;
```

We **subtract** the speed bonus from the base interval (0.8 seconds). We do **not** do `timer = 0` after each drop — we use `timer -= drop_interval` to keep the fractional remainder. Here's why that matters:

Imagine the drop interval is 0.5 seconds, but your last frame took 0.51 seconds:
```
timer = 0.51  →  timer >= 0.5  →  drop fires
timer = 0     ← "reset" approach: 0.01s of overshoot lost
timer -= 0.5  ← "subtract" approach: timer = 0.01, next drop fires 0.01s sooner
```
Over thousands of frames, the subtract approach keeps the timing tight. The reset approach accumulates drift.

> **Why `0.1f` as the minimum?** The `f` suffix tells the C compiler this is a `float` literal, not a `double`. Writing `0.1` (without `f`) would be a `double`, causing an implicit conversion warning with `-Wall`. In JS, all numbers are 64-bit floats; in C you must be explicit.

---

## Step 4: Complete `src/game.c` at This Stage

**`src/game.c`** — complete file at the end of lesson 09:

```c
/* game.c — Lesson 09: Scoring & Levels */

#include "game.h"
#include "utils/draw-shapes.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Tetromino Shape Data ───────────────────────────────────────────────── */
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

/* ── Private Drawing Helpers ────────────────────────────────────────────── */
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

/* ── Render ─────────────────────────────────────────────────────────────── */
/*
 * Lesson 09: no sidebar text yet — that comes in lesson 10.
 * We draw the field and the current piece; the score/level live in
 * state->score and state->level but we haven't wired up draw_text yet.
 */
void game_render(Backbuffer *backbuffer, GameState *state) {
  /* Clear */
  for (int i = 0; i < backbuffer->width * backbuffer->height; i++) {
    backbuffer->pixels[i] = COLOR_BLACK;
  }

  /* Draw field cells */
  for (int row = 0; row < FIELD_HEIGHT; row++) {
    for (int col = 0; col < FIELD_WIDTH; col++) {
      unsigned char cell_value = state->field[row * FIELD_WIDTH + col];
      switch (cell_value) {
        case TETRIS_FIELD_EMPTY: continue;
        case TETRIS_FIELD_I: case TETRIS_FIELD_J: case TETRIS_FIELD_L:
        case TETRIS_FIELD_O: case TETRIS_FIELD_S: case TETRIS_FIELD_T:
        case TETRIS_FIELD_Z:
          draw_cell(backbuffer, col, row, get_tetromino_color(cell_value - 1));
          break;
        case TETRIS_FIELD_WALL:
          draw_cell(backbuffer, col, row, COLOR_GRAY);
          break;
        case TETRIS_FIELD_TMP_FLASH:
          draw_cell(backbuffer, col, row, COLOR_WHITE);
          break;
      }
    }
  }

  /* Draw falling piece */
  draw_piece(backbuffer,
             state->current_piece.index,
             state->current_piece.x,
             state->current_piece.y,
             get_tetromino_color(state->current_piece.index),
             state->current_piece.rotate_x_value);
}

/* ── Initialization ─────────────────────────────────────────────────────── */
void game_init(GameState *state) {
  /* NEW: Reset scoring state */
  state->score        = 0;
  state->pieces_count = 1;
  state->level        = 0;

  state->completed_lines.count = 0;
  state->completed_lines.flash_timer.timer    = 0;
  state->completed_lines.flash_timer.interval = 0.4f;
  memset(&state->completed_lines.indexes, 0, sizeof(int) * TETROMINO_LAYER_COUNT);

  state->input_values.rotate_direction.timer    = 0.0f;
  state->input_values.rotate_direction.interval = 0.8f;

  state->input_repeat.move_left.initial_delay        = 0.15f;
  state->input_repeat.move_left.interval             = 0.05f;
  state->input_repeat.move_left.passed_initial_delay = false;

  state->input_repeat.move_right.initial_delay        = 0.15f;
  state->input_repeat.move_right.interval             = 0.05f;
  state->input_repeat.move_right.passed_initial_delay = false;

  state->input_repeat.move_down.initial_delay        = 0.0f;
  state->input_repeat.move_down.interval             = 0.03f;
  state->input_repeat.move_down.passed_initial_delay = false;

  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) {
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_WALL;
      } else {
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_EMPTY;
      }
    }
  }

  srand((unsigned int)time(NULL));
  state->current_piece = (CurrentPiece){
    .x              = (int)((FIELD_WIDTH  * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
    .y              = 0,
    .index          = rand() % TETROMINOS_COUNT,
    .next_index     = rand() % TETROMINOS_COUNT,
    .rotate_x_value = TETROMINO_R_0,
  };
}

/* ── Rotation Math ──────────────────────────────────────────────────────── */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r) {
    case TETROMINO_R_0:   return py * TETROMINO_LAYER_COUNT + px;
    case TETROMINO_R_90:  return 12 + py - (px * TETROMINO_LAYER_COUNT);
    case TETROMINO_R_180: return 15 - (py * TETROMINO_LAYER_COUNT) - px;
    case TETROMINO_R_270: return 3  - py + (px * TETROMINO_LAYER_COUNT);
  }
  return 0;
}

/* ── Collision Detection ────────────────────────────────────────────────── */
int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                              int pos_x, int pos_y) {
  if (piece < 0 || piece >= TETROMINOS_COUNT) return 0;
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece][pi] == TETROMINO_SPAN) continue;
      int field_x = pos_x + px;
      int field_y = pos_y + py;
      if (field_x < 0 || field_x >= FIELD_WIDTH)  continue;
      if (field_y < 0 || field_y >= FIELD_HEIGHT)  continue;
      if (state->field[field_y * FIELD_WIDTH + field_x] != TETRIS_FIELD_EMPTY)
        return 0;
    }
  }
  return 1;
}

/* ── Input Buffering ────────────────────────────────────────────────────── */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
  for (int btn = 0; btn < BUTTON_COUNT; btn++) {
    current_input->buttons[btn].ended_down            = old_input->buttons[btn].ended_down;
    current_input->buttons[btn].half_transition_count = 0;
  }
}

/* ── DAS/ARR Helper ─────────────────────────────────────────────────────── */
static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval   *repeat,
                                      float             delta_time,
                                      int              *should_trigger) {
  *should_trigger = 0;
  if (!button->ended_down) {
    repeat->timer = 0.0f;
    repeat->is_active = false;
    repeat->passed_initial_delay = false;
    return;
  }
  if (button->half_transition_count > 0) {
    repeat->timer = 0.0f;
    repeat->is_active = true;
    repeat->passed_initial_delay = false;
    if (repeat->initial_delay <= 0.0f) *should_trigger = 1;
    return;
  }
  if (!repeat->is_active) return;
  repeat->timer += delta_time;
  if (!repeat->passed_initial_delay) {
    if (repeat->timer >= repeat->initial_delay) {
      *should_trigger = 1;
      repeat->timer = 0.0f;
      repeat->passed_initial_delay = true;
    }
  } else {
    if (repeat->timer >= repeat->interval) {
      *should_trigger = 1;
      repeat->timer -= repeat->interval;
    }
  }
}

/* ── Player Input ───────────────────────────────────────────────────────── */
/*
 * Lesson 09: no audio calls yet; those arrive in lesson 11.
 */
static void tetris_apply_input(GameState *state, GameInput *input,
                                float delta_time) {
  /* Rotation — one press = one turn */
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

  int should_move_down = 0;
  handle_action_with_repeat(&input->move_down, &state->input_repeat.move_down,
                             delta_time, &should_move_down);
  if (should_move_down &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x, state->current_piece.y + 1))
    state->current_piece.y++;
}

/* ── Main Update ────────────────────────────────────────────────────────── */
void game_update(GameState *state, GameInput *input, float delta_time) {

  /* Restart */
  if (state->should_restart) {
    game_init(state);
    return;
  }

  /* Flash animation — freeze all logic while completed rows flash white */
  if (state->completed_lines.flash_timer.timer > 0) {
    state->completed_lines.flash_timer.timer -= delta_time;
    if (state->completed_lines.flash_timer.timer <= 0) {
      /* Collapse completed rows, bottom-to-top (see lesson 08 notes) */
      for (int i = state->completed_lines.count - 1; i >= 0; i--) {
        int line_index = state->completed_lines.indexes[i];
        for (int py = line_index; py > 0; --py) {
          for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
            state->field[py * FIELD_WIDTH + px] =
                state->field[(py - 1) * FIELD_WIDTH + px];
          }
        }
        for (int px = 1; px < FIELD_WIDTH - 1; px++) {
          state->field[px] = TETRIS_FIELD_EMPTY;
        }
        for (int j = i - 1; j >= 0; --j) {
          if (state->completed_lines.indexes[j] < line_index)
            state->completed_lines.indexes[j]++;
        }
      }
      state->completed_lines.count = 0;
    }
    return;
  }

  /* Player input */
  tetris_apply_input(state, input, delta_time);

  /* Gravity timer */
  state->input_values.rotate_direction.timer += delta_time;

  /* NEW: Level-based drop speed
   *
   * At level 0: drop_interval = 0.8 - 0.01 = 0.79s
   * At level 5: drop_interval = 0.8 - 0.06 = 0.74s
   * At level 70: drop_interval = 0.8 - 0.71 = 0.09s → capped to 0.1s
   *
   * We SUBTRACT the speed bonus to make the interval SHORTER as level rises.
   * The floor of 0.1s (10 drops/second) prevents the game from becoming
   * impossibly fast.
   *
   * See src/game.c:796-801 for the final version of this logic.
   */
  int game_speed  = state->level;
  float drop_interval = state->input_values.rotate_direction.interval
                       - (0.01f + game_speed * 0.01f);
  if (drop_interval < 0.1f) drop_interval = 0.1f;

  if (state->input_values.rotate_direction.timer >= drop_interval) {
    state->input_values.rotate_direction.timer -= drop_interval;  /* keep remainder */

    if (tetromino_does_piece_fit(state, state->current_piece.index,
                                  state->current_piece.rotate_x_value,
                                  state->current_piece.x,
                                  state->current_piece.y + 1)) {
      state->current_piece.y++;
    } else {
      /* ── Piece Lock ── */
      for (int px = 0; px < TETROMINO_LAYER_COUNT; ++px) {
        for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
          int pi = tetromino_pos_value(px, py, state->current_piece.rotate_x_value);
          if (TETROMINOES[state->current_piece.index][pi] != TETROMINO_SPAN) {
            int fx = state->current_piece.x + px;
            int fy = state->current_piece.y + py;
            if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT)
              state->field[fy * FIELD_WIDTH + fx] = state->current_piece.index + 1;
          }
        }
      }

      /* Detect completed lines */
      state->completed_lines.count = 0;
      for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
        int row_y = state->current_piece.y + py;
        if (row_y < 0 || row_y >= FIELD_HEIGHT - 1) continue;
        bool completed = true;
        for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
          if (state->field[row_y * FIELD_WIDTH + px] == TETRIS_FIELD_EMPTY) {
            completed = false;
            break;
          }
        }
        if (completed) {
          for (int px = 1; px < FIELD_WIDTH - 1; ++px)
            state->field[row_y * FIELD_WIDTH + px] = TETRIS_FIELD_TMP_FLASH;
          state->completed_lines.indexes[state->completed_lines.count++] = row_y;
        }
      }
      if (state->completed_lines.count > 0) {
        state->completed_lines.flash_timer.timer =
            state->completed_lines.flash_timer.interval;
      }

      /* ════════════════════════════════════════════════════════════════════
       * NEW: Scoring  (see src/game.c:887-898)
       * ════════════════════════════════════════════════════════════════════
       *
       * SCORE FORMULA: (1 << completed_lines.count) * 100
       *   0 lines cleared → no bonus
       *   1 line:  (1 << 1) * 100 =  200
       *   2 lines: (1 << 2) * 100 =  400
       *   3 lines: (1 << 3) * 100 =  800
       *   4 lines: (1 << 4) * 100 = 1600  ← Tetris!
       *
       * In JS: (1 << count) * 100  works identically.
       */
      state->pieces_count++;
      state->score += 25;   /* flat reward every lock */
      if (state->completed_lines.count > 0) {
        state->score += (1 << state->completed_lines.count) * 100;
      }

      /* NEW: Level up every 25 pieces */
      if (state->pieces_count % 25 == 0) {
        state->level++;
      }

      /* Spawn next piece */
      state->input_values.rotate_direction.timer = 0.0f;
      state->current_piece = (CurrentPiece){
        .x              = (int)((FIELD_WIDTH * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
        .y              = 0,
        .index          = state->current_piece.next_index,
        .next_index     = rand() % TETROMINOS_COUNT,
        .rotate_x_value = TETROMINO_R_0,
      };
      /* Note: game-over detection arrives in lesson 13 */
    }
  }
}
```

---

## Build and Run

```bash
clang -Wall -Wextra -g -O0 \
  -o build/game \
  src/game.c src/utils/draw-shapes.c src/main_raylib.c \
  -lraylib -lpthread -ldl -lm

./build/game
```

Or use the build script: `./build-dev.sh`

---

## Expected Result

The game plays identically to lesson 08 — pieces fall, rotate, lock, lines clear. But now the **score**, **level**, and **piece count** are tracked in memory. You can verify them by adding a temporary `printf` in `game_update` right after the scoring block:

```c
printf("score=%d level=%d pieces=%d\n",
       state->score, state->level, state->pieces_count);
```

Remove the `printf` after verifying. We'll display these values properly in lesson 10.

---

## Common Mistakes

> **Common mistake:** Writing `1 << completed_lines.count` when `completed_lines.count` is `0` (no lines cleared). `1 << 0 = 1`, so you'd award `100` points even when no lines were cleared. Always guard with `if (state->completed_lines.count > 0)`.

> **Common mistake:** Using `int` for `(1 << n) * 100` when `n` could be 31 or 32. Shifting a signed `int` by 31+ bits is undefined behaviour in C. For our case `n` is at most 4 (four lines), so `1 << 4 = 16` — safe. But note that `1 << 31` on a 32-bit `int` is UB. Use `(1u << n)` (unsigned) if you ever need large shifts.

> **Common mistake:** Forgetting that `pieces_count` starts at `1` in `game_init`. This means the first level-up triggers after 24 additional pieces, not 25. You can adjust this to `0` if you prefer exactly 25 pieces per level — pick one and be consistent.

---

## Exercises

1. Change the score formula so 4-line clears give 2400 pts instead of 1600. Where is the formula? How small is the change?
2. Make the level-up threshold 10 pieces instead of 25 and observe the speed curve.
3. Add a `high_score` field to `GameState`. Update it whenever `score > high_score`. Persist it through restarts (don't reset it in `game_init`).
4. Print the drop interval to stdout at each level-up to verify the speed curve is what you expect.

---

## What's Next

The score and level exist in memory, but players can't see them. **Lesson 10** introduces the bitmap font and `draw_text()` so we can render a full HUD sidebar: score, level, piece count, and a next-piece preview.
