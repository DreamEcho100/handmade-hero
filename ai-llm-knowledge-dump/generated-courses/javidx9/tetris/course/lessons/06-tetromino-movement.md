# Lesson 06: Tetromino Movement

## What we're building

The piece now responds to keyboard input. Left/right arrow keys (or A/D) move
the piece horizontally. Down arrow (or S) soft-drops it. Z rotates
counter-clockwise; X rotates clockwise. We also implement DAS/ARR timing so
holding a key feels smooth.

```
t=0.00  Press LEFT  → piece moves left immediately
t=0.15  Hold expires (DAS) → piece moves left again
t=0.20  ARR fires   → piece moves left again
t=0.25  ARR fires   → piece moves left again   (20 Hz auto-repeat)
...
```

There is still no gravity and no playfield. The piece floats at its position
until the player moves it — it will never fall on its own.

## What you'll learn

- Adding fields to an existing struct
- `RepeatInterval`: a reusable timer struct for DAS/ARR auto-repeat
- `tetromino_does_piece_fit()`: step-by-step collision detection
- `static` functions as file-private helpers (revisited)
- Output parameters: `int *should_trigger` as a "second return value"
- `bool` from `<stdbool.h>` in practice

## Prerequisites

- Lesson 05 complete and running — you see a fixed I-piece on screen
- You understand `tetromino_pos_value()` and the rotation formulas

---

## Step 1: Extending `CurrentPiece` with Position

The piece needs an `(x, y)` position so it can be drawn at different field
coordinates. We also add `next_index` (for the "next piece" preview, used in
Lesson 08) and `next_rotate_x_value` (what rotation the player just requested):

```c
/* BEFORE (Lesson 05): */
typedef struct {
  TETROMINO_BY_IDX index;
  TETROMINO_R_DIR  rotate_x_value;
} CurrentPiece;

/* AFTER (Lesson 06): */
typedef struct {
  int x;                             /* Field column of the top-left corner  */
  int y;                             /* Field row    of the top-left corner   */
  TETROMINO_BY_IDX index;            /* Which tetromino is active             */
  TETROMINO_BY_IDX next_index;       /* Piece that spawns next                */
  TETROMINO_R_DIR  rotate_x_value;   /* Current rotation state [0..3]         */
  TETROMINO_ROTATE_X_VALUE next_rotate_x_value; /* Pending rotation request   */
} CurrentPiece;
```

`x` and `y` are **field coordinates**, not pixels. Field column 0 is the left
wall, row 0 is the top. The pixel position is `col * CELL_SIZE, row * CELL_SIZE`.

In `game_render` we replace the hardcoded `(4, 0)` from Lesson 05 with
`state->current_piece.x` and `state->current_piece.y`.

We also need a new enum for the rotation request direction:

```c
typedef enum {
  TETROMINO_ROTATE_X_NONE,       /* No rotation this frame          */
  TETROMINO_ROTATE_X_GO_LEFT,    /* Player pressed Z (CCW)          */
  TETROMINO_ROTATE_X_GO_RIGHT,   /* Player pressed X (CW)           */
} TETROMINO_ROTATE_X_VALUE;
```

---

## Step 2: `RepeatInterval` — DAS/ARR Auto-Repeat

**DAS** (Delayed Auto Shift) and **ARR** (Auto Repeat Rate) are the two timing
constants every Tetris implementation needs:

```
Press LEFT:
  t=0.00  Key down  → trigger immediately (first move)
  ...     Holding   → nothing (waiting for DAS)
  t=0.15  DAS expires  → trigger (second move), enter ARR mode
  t=0.20  ARR fires    → trigger (third move)
  t=0.25  ARR fires    → trigger (fourth move, 20 Hz)
  t=?     Key up    → stop, reset timer
```

We store all the timing state in a single reusable struct called `RepeatInterval`:

```c
typedef struct {
  float timer;               /* Accumulates delta_time while held          */
  float initial_delay;       /* DAS: seconds before first auto-repeat      */
  float interval;            /* ARR: seconds between auto-repeats          */
  bool  is_active;           /* true while button is held                  */
  bool  passed_initial_delay;/* true once DAS threshold has been crossed   */
} RepeatInterval;
```

> **Why store it in `GameState`, not `GameInput`?** `GameInput` represents **hardware state** — what buttons are physically held right now. `RepeatInterval` represents **game timing** — how long the game has been responding to that held button. The hardware doesn't know about DAS/ARR. The game does. Mixing them would couple your input system to game-specific logic, making it harder to reuse across different games or control schemes.

> **Why `bool` instead of `int`?** `bool` from `<stdbool.h>` is an alias for `_Bool`, a one-byte type that can only hold `0` or `1`. Using `bool` makes intent explicit — it's not a counter, it's a flag. In JavaScript `true`/`false` are always booleans; in C you must `#include <stdbool.h>` to get the `bool` type and `true`/`false` keywords. Without it, C only has 0 (false) and non-zero (true) integers.

> **Handmade Hero principle:** The reference implementation mutates `repeat->initial_delay = 0.0f` to "switch modes" from DAS to ARR. This destroys the configuration value — you can never use the DAS delay again after the first key press, because the threshold has been overwritten with zero. Our `passed_initial_delay` flag records the phase transition without touching the config. Reset the flag on key release to restore full DAS behaviour for the next press.

---

## Step 3: `tetromino_does_piece_fit()` — Collision Detection

This function answers: "can this piece be placed at this position without
clipping outside the field?"

In Lesson 06 we don't have a playfield array yet, so we check **field bounds
only** — the piece can't go past the left edge (column < 1), right edge
(column ≥ FIELD_WIDTH-1), or bottom (row ≥ FIELD_HEIGHT-1). In Lesson 07 we
upgrade this to also check against locked pieces in the field array.

Here is the step-by-step logic:

```
For each of the 16 cells (px, py) in the 4×4 bounding box:
  1. Look up the character in the piece string (accounting for rotation).
  2. If it's '.' (empty), skip — nothing to check here.
  3. Compute the field position: field_x = pos_x + px, field_y = pos_y + py.
  4. Bounds check:
       If field_x < 1         → collision with left wall → return 0
       If field_x >= FIELD_WIDTH - 1  → collision with right wall → return 0
       If field_y >= FIELD_HEIGHT - 1 → collision with floor → return 0
       If field_y < 0          → skip (piece hanging above the top is OK)
5. No collision found → return 1 (piece fits)
```

Note step 4: cells above the top of the field (`field_y < 0`) are **skipped,
not blocked**. This allows a piece to spawn partially above row 0, which is
normal Tetris behaviour for tall pieces like the I-piece.

```c
int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y) {
  (void)state; /* field not checked yet — added in Lesson 07 */

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece][pi] == TETROMINO_SPAN) continue;

      int field_x = pos_x + px;
      int field_y = pos_y + py;

      if (field_y < 0)                        continue; /* above top: OK    */
      if (field_x < 1)                        return 0; /* left wall        */
      if (field_x >= FIELD_WIDTH  - 1)        return 0; /* right wall       */
      if (field_y >= FIELD_HEIGHT - 1)        return 0; /* floor            */
    }
  }
  return 1;
}
```

> **Why `(void)state` for now?** In C, using a parameter name without using the value triggers a `-Wunused-parameter` warning (and with `-Wextra` it's shown even if not -Werror). The `(void)state` cast explicitly silences the warning by "using" the parameter as a void expression. This is a common idiom for stub functions that will use the parameter in a later step.

---

## Step 4: `handle_action_with_repeat()` — Output Parameters

This `static` function processes one button's DAS/ARR state for the current
frame and tells the caller whether the action should fire this frame.

The challenge: a C function can only `return` one value. We need to "return" a
boolean — but we also need to mutate the `RepeatInterval` struct. The solution
is an **output parameter**: a pointer to a variable that the function writes
into.

```c
static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval   *repeat,
                                      float             delta_time,
                                      int              *should_trigger) {
  *should_trigger = 0;  /* default: don't trigger */

  /* ... state machine logic ... */
  
  /* Sets *should_trigger = 1 if the action should fire this frame */
}
```

> **Why an output parameter instead of a return value?** In JavaScript you'd return an object: `const { trigger, newRepeat } = handleRepeat(button, repeat, dt)`. In C, returning structs by value is possible but copies the data. Using a pointer (`int *should_trigger`) is idiomatic for "secondary outputs". The caller passes the ADDRESS of a local variable: `handle_action_with_repeat(&btn, &rep, dt, &should_move_left)`. The function writes through the pointer. JS analogy: passing a callback `(trigger) => { if (trigger) moveLeft(); }` — except in C, the "callback" is just a write through a pointer.

The state machine has three phases:

```
RELEASED:
  reset timer, is_active = false, passed_initial_delay = false
  → do NOT trigger

JUST PRESSED (half_transition_count > 0 AND ended_down):
  reset timer, is_active = true, passed_initial_delay = false
  → trigger immediately ONLY if initial_delay == 0 (no DAS, e.g. soft drop)
  → return (don't accumulate timer yet on the first frame)

HELD from previous frame:
  timer += delta_time

  if NOT passed_initial_delay:
    if timer >= initial_delay:
      trigger, reset timer, set passed_initial_delay = true
  else:
    if timer >= interval:
      trigger, timer -= interval  ← NOTE: subtract, don't reset
```

> **Why `timer -= interval` instead of `timer = 0`?** Suppose `interval = 0.05s`
> and your frame takes `0.067s` (a brief spike). If you reset the timer to 0,
> you lose the `0.017s` overshoot — the next trigger fires `0.017s` late. By
> subtracting instead of resetting, the remainder carries forward and the next
> trigger fires exactly `0.05s` after the last one should have. This is the
> same reason game physics uses `position += velocity * dt` rather than
> snapping to rounded positions. Small errors don't accumulate.

The full implementation (see `src/game.c:518-574`):

```c
static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval   *repeat,
                                      float             delta_time,
                                      int              *should_trigger) {
  *should_trigger = 0;

  if (!button->ended_down) {
    repeat->timer                = 0.0f;
    repeat->is_active            = false;
    repeat->passed_initial_delay = false;
    return;
  }

  if (button->half_transition_count > 0) {
    repeat->timer                = 0.0f;
    repeat->is_active            = true;
    repeat->passed_initial_delay = false;
    if (repeat->initial_delay <= 0.0f) {
      *should_trigger = 1;  /* no DAS — fire immediately */
    }
    return;
  }

  if (!repeat->is_active) return;

  repeat->timer += delta_time;

  if (!repeat->passed_initial_delay) {
    if (repeat->timer >= repeat->initial_delay) {
      *should_trigger              = 1;
      repeat->timer                = 0.0f;
      repeat->passed_initial_delay = true;
    }
  } else {
    if (repeat->timer >= repeat->interval) {
      *should_trigger = 1;
      repeat->timer  -= repeat->interval; /* keep remainder for precision */
    }
  }
}
```

---

## Step 5: `tetris_apply_input()` — Putting It Together

`tetris_apply_input` is a non-static function called from `game_update`. It
processes rotation, horizontal movement, and soft drop for one frame:

```
1. Rotation (Z/X):
   - Check: input->rotate_x.ended_down AND half_transition_count != 0
   - Compute new rotation (CW or CCW from current)
   - Call tetromino_does_piece_fit with new rotation
   - If fits: apply rotation

2. Left/Right movement (with DAS/ARR):
   - Call handle_action_with_repeat for move_left and move_right
   - If should trigger AND piece fits: move x--  or x++

3. Soft drop (Down key, no DAS):
   - Call handle_action_with_repeat for move_down
   - If should trigger AND piece fits: move y++
```

Rotation uses a `switch` to cycle through the four states:

```c
/* Clockwise rotation: 0 → 90 → 180 → 270 → 0 */
switch (state->current_piece.rotate_x_value) {
  case TETROMINO_R_0:   new_rotation = TETROMINO_R_90;  break;
  case TETROMINO_R_90:  new_rotation = TETROMINO_R_180; break;
  case TETROMINO_R_180: new_rotation = TETROMINO_R_270; break;
  case TETROMINO_R_270: new_rotation = TETROMINO_R_0;   break;
  default:              new_rotation = TETROMINO_R_0;   break;
}
```

`game_init` now initialises `x` and `y`:

```c
state->current_piece = (CurrentPiece){
  .x              = (int)((FIELD_WIDTH  * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
  .y              = 0,
  .index          = rand() % TETROMINOS_COUNT,
  .next_index     = rand() % TETROMINOS_COUNT,
  .rotate_x_value = TETROMINO_R_0,
};
```

> **Why the compound literal `(CurrentPiece){ .field = value }`?** This is a C99 "compound literal" — it creates a temporary struct value. The `.fieldname = value` syntax is "designated initialiser" notation, equivalent to `{ x: 4, y: 0, index: TETROMINO_I_IDX }` in JavaScript. Assigning it to `state->current_piece` copies all fields at once, avoiding partial initialisation bugs where you set `x` but forget `y`.

And `GameState` gains an `input_repeat` sub-struct:

```c
struct {
  RepeatInterval move_left;
  RepeatInterval move_right;
  RepeatInterval move_down;
} input_repeat;
```

`game_init` configures the DAS/ARR timings:

```c
state->input_repeat.move_left.initial_delay  = 0.15f;  /* 150 ms DAS */
state->input_repeat.move_left.interval       = 0.05f;  /*  50 ms ARR */
state->input_repeat.move_right.initial_delay = 0.15f;
state->input_repeat.move_right.interval      = 0.05f;
state->input_repeat.move_down.initial_delay  = 0.0f;   /* No DAS: fire on press */
state->input_repeat.move_down.interval       = 0.03f;  /*  30 ms ARR */
```

---

## Step 6: Complete `game.h` for Lesson 06

Replace `src/game.h` with this complete file:

```c
/* src/game.h — Lesson 06: Tetromino Movement */
#ifndef GAME_H
#define GAME_H

#include "utils/audio.h"
#include "utils/backbuffer.h"
#include <stdbool.h>
#include <stdint.h>

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

/* ── Colors ── */
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

/* ── Enums ── */
typedef enum {
  TETROMINO_I_IDX, TETROMINO_J_IDX, TETROMINO_L_IDX, TETROMINO_O_IDX,
  TETROMINO_S_IDX, TETROMINO_T_IDX, TETROMINO_Z_IDX,
} TETROMINO_BY_IDX;

typedef enum {
  TETROMINO_R_0, TETROMINO_R_90, TETROMINO_R_180, TETROMINO_R_270,
} TETROMINO_R_DIR;

/* NEW in Lesson 06: direction the player wants to rotate */
typedef enum {
  TETROMINO_ROTATE_X_NONE,
  TETROMINO_ROTATE_X_GO_LEFT,   /* Z key — counter-clockwise */
  TETROMINO_ROTATE_X_GO_RIGHT,  /* X key — clockwise         */
} TETROMINO_ROTATE_X_VALUE;

/* ── RepeatInterval — DAS/ARR auto-repeat timer ── */
/* NEW in Lesson 06.
 * JS: think of this as the state object for a debounce/throttle combo.
 * C:  a plain struct — no methods, just data fields.                    */
typedef struct {
  float timer;               /* Accumulated held time (seconds)           */
  float initial_delay;       /* DAS: seconds before first auto-repeat     */
  float interval;            /* ARR: seconds between subsequent repeats   */
  bool  is_active;           /* true while button is held                 */
  bool  passed_initial_delay;/* true once DAS threshold crossed           */
} RepeatInterval;

/* ── CurrentPiece ── */
/* UPDATED in Lesson 06: adds x, y, next_index, next_rotate_x_value */
typedef struct {
  int x;                             /* Field column, top-left of 4×4 box */
  int y;                             /* Field row,    top-left of 4×4 box */
  TETROMINO_BY_IDX index;
  TETROMINO_BY_IDX next_index;
  TETROMINO_R_DIR  rotate_x_value;
  TETROMINO_ROTATE_X_VALUE next_rotate_x_value;
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
/* UPDATED in Lesson 06: adds input_repeat */
typedef struct {
  CurrentPiece current_piece;

  /* DAS/ARR state for each movement action — NEW in Lesson 06 */
  struct {
    RepeatInterval move_left;
    RepeatInterval move_right;
    RepeatInterval move_down;
  } input_repeat;

  GameAudioState audio;
  int should_quit;
  int should_restart;
} GameState;

/* ── Tetromino data — defined in game.c ── */
extern const char *TETROMINOES[7];

/* ── Function Declarations ── */
void game_init(GameState *state);
void prepare_input_frame(GameInput *old_input, GameInput *new_input);
int  tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);

/* NEW in Lesson 06 */
int  tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                              int pos_x, int pos_y);

void game_update(GameState *state, GameInput *input, float delta_time);
void game_render(Backbuffer *backbuffer, GameState *state);

/* Audio — in src/audio.c */
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

## Step 7: Complete `game.c` for Lesson 06

Replace `src/game.c` with this complete file:

```c
/* src/game.c — Lesson 06: Tetromino Movement */

#include "game.h"
#include "utils/draw-shapes.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Tetromino Data (unchanged from Lesson 05) ── */
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

/* ── Static Helpers (unchanged from Lesson 05) ── */
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

/* ── game_render ── */
/* UPDATED: uses current_piece.x/y instead of hardcoded (4,0) */
void game_render(Backbuffer *backbuffer, GameState *state) {
  /* Clear to black */
  for (int i = 0; i < backbuffer->width * backbuffer->height; i++) {
    backbuffer->pixels[i] = COLOR_BLACK;
  }

  /* Draw current piece at its actual position */
  draw_piece(backbuffer,
             state->current_piece.index,
             state->current_piece.x,
             state->current_piece.y,
             get_tetromino_color(state->current_piece.index),
             state->current_piece.rotate_x_value);
}

/* ── game_init ── */
/* UPDATED: initialises x, y, next_index, and DAS/ARR timers */
void game_init(GameState *state) {
  srand((unsigned int)time(NULL));

  /* Spawn piece near top-centre of the 12-column field.
   * (FIELD_WIDTH * 0.5) - (TETROMINO_LAYER_COUNT * 0.5) = 6 - 2 = 4       */
  state->current_piece = (CurrentPiece){
    .x              = (int)((FIELD_WIDTH  * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
    .y              = 0,
    .index          = rand() % TETROMINOS_COUNT,
    .next_index     = rand() % TETROMINOS_COUNT,
    .rotate_x_value = TETROMINO_R_0,
    .next_rotate_x_value = TETROMINO_ROTATE_X_NONE,
  };

  /* DAS = 150 ms initial delay before auto-repeat begins.
   * ARR = 50 ms between repeats (20 Hz) once DAS expires.
   *
   * Soft drop (move_down) has no DAS — it fires on the very first frame
   * the key is pressed. ARR = 30 ms (≈ 33 Hz).                             */
  state->input_repeat.move_left.initial_delay        = 0.15f;
  state->input_repeat.move_left.interval             = 0.05f;
  state->input_repeat.move_left.passed_initial_delay = false;

  state->input_repeat.move_right.initial_delay        = 0.15f;
  state->input_repeat.move_right.interval             = 0.05f;
  state->input_repeat.move_right.passed_initial_delay = false;

  state->input_repeat.move_down.initial_delay        = 0.0f;
  state->input_repeat.move_down.interval             = 0.03f;
  state->input_repeat.move_down.passed_initial_delay = false;
}

/* ── tetromino_pos_value (unchanged from Lesson 05) ── */
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
 * tetromino_does_piece_fit — Collision Detection
 *
 * Lesson 06: bounds-only check (no field array yet).
 * Lesson 07 upgrades this to also check locked cells in state->field[].
 * (see src/game.c:432-490 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y) {
  (void)state; /* field array check added in Lesson 07 */

  if (piece < 0 || piece >= TETROMINOS_COUNT) return 0;

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece][pi] == TETROMINO_SPAN) continue;

      int field_x = pos_x + px;
      int field_y = pos_y + py;

      /* Cells above the top row are allowed (spawn can overhang) */
      if (field_y < 0)                   continue;
      /* Walls and floor are treated as hard boundaries */
      if (field_x < 1)                   return 0;
      if (field_x >= FIELD_WIDTH  - 1)   return 0;
      if (field_y >= FIELD_HEIGHT - 1)   return 0;
    }
  }
  return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * handle_action_with_repeat — DAS/ARR State Machine
 *
 * `static`: private to game.c — only tetris_apply_input calls this.
 * Output param: *should_trigger is set to 1 if the action fires this frame.
 * (see src/game.c:518-574 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval   *repeat,
                                      float             delta_time,
                                      int              *should_trigger) {
  *should_trigger = 0;

  /* RELEASED: reset everything */
  if (!button->ended_down) {
    repeat->timer                = 0.0f;
    repeat->is_active            = false;
    repeat->passed_initial_delay = false;
    return;
  }

  /* JUST PRESSED this frame (half_transition_count > 0) */
  if (button->half_transition_count > 0) {
    repeat->timer                = 0.0f;
    repeat->is_active            = true;
    repeat->passed_initial_delay = false;
    /* Fire immediately only if no initial delay (e.g. soft drop) */
    if (repeat->initial_delay <= 0.0f) {
      *should_trigger = 1;
    }
    return;
  }

  /* HELD from previous frame */
  if (!repeat->is_active) return;

  repeat->timer += delta_time;

  if (!repeat->passed_initial_delay) {
    /* DAS phase: waiting for initial_delay to expire */
    if (repeat->timer >= repeat->initial_delay) {
      *should_trigger              = 1;
      repeat->timer                = 0.0f;
      repeat->passed_initial_delay = true;
    }
  } else {
    /* ARR phase: fire every `interval` seconds */
    if (repeat->timer >= repeat->interval) {
      *should_trigger = 1;
      repeat->timer  -= repeat->interval; /* subtract not reset — precision */
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * tetris_apply_input — Process player input for one frame.
 * (see src/game.c:600-660 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {

  /* ── Rotation (Z/X keys — single press per action) ── */
  if (input->rotate_x.ended_down &&
      input->rotate_x.half_transition_count != 0 &&
      state->current_piece.next_rotate_x_value != TETROMINO_ROTATE_X_NONE) {

    TETROMINO_R_DIR new_rotation;

    if (state->current_piece.next_rotate_x_value == TETROMINO_ROTATE_X_GO_RIGHT) {
      /* CW: 0→90→180→270→0 */
      switch (state->current_piece.rotate_x_value) {
        case TETROMINO_R_0:   new_rotation = TETROMINO_R_90;  break;
        case TETROMINO_R_90:  new_rotation = TETROMINO_R_180; break;
        case TETROMINO_R_180: new_rotation = TETROMINO_R_270; break;
        case TETROMINO_R_270: new_rotation = TETROMINO_R_0;   break;
        default:              new_rotation = TETROMINO_R_0;   break;
      }
    } else {
      /* CCW: 0→270→180→90→0 */
      switch (state->current_piece.rotate_x_value) {
        case TETROMINO_R_0:   new_rotation = TETROMINO_R_270; break;
        case TETROMINO_R_90:  new_rotation = TETROMINO_R_0;   break;
        case TETROMINO_R_180: new_rotation = TETROMINO_R_90;  break;
        case TETROMINO_R_270: new_rotation = TETROMINO_R_180; break;
        default:              new_rotation = TETROMINO_R_0;   break;
      }
    }

    if (tetromino_does_piece_fit(state, state->current_piece.index,
                                 new_rotation,
                                 state->current_piece.x,
                                 state->current_piece.y)) {
      state->current_piece.rotate_x_value = new_rotation;
    }
  }

  /* ── Horizontal Movement (DAS/ARR) ── */
  int should_move_left  = 0;
  int should_move_right = 0;

  handle_action_with_repeat(&input->move_left,  &state->input_repeat.move_left,
                             delta_time, &should_move_left);
  handle_action_with_repeat(&input->move_right, &state->input_repeat.move_right,
                             delta_time, &should_move_right);

  if (should_move_left &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x - 1,
                               state->current_piece.y)) {
    state->current_piece.x--;
  }

  if (should_move_right &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x + 1,
                               state->current_piece.y)) {
    state->current_piece.x++;
  }

  /* ── Soft Drop (ARR only, no DAS) ── */
  int should_move_down = 0;
  handle_action_with_repeat(&input->move_down, &state->input_repeat.move_down,
                             delta_time, &should_move_down);

  if (should_move_down &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x,
                               state->current_piece.y + 1)) {
    state->current_piece.y++;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_update — Main logic. Called once per frame.
 * ═══════════════════════════════════════════════════════════════════════════ */
void game_update(GameState *state, GameInput *input, float delta_time) {
  /* Keep audio sequencer running */
  game_audio_update(&state->audio, delta_time);

  if (state->should_restart) {
    game_init(state);
    return;
  }

  /* Process player input — rotation, left/right, soft drop */
  tetris_apply_input(state, input, delta_time);

  /* Gravity and locking added in Lesson 07 */
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

- A random tetromino appears near the top-centre of the screen.
- Left/Right arrows (or A/D): piece moves left/right.
- Holding left/right: after 150 ms, piece auto-repeats at ~20 Hz.
- Down arrow (or S): piece moves down each frame you hold it.
- Z: rotate counter-clockwise. X: rotate clockwise.
- Piece cannot move past the left or right edge of the field area.
- The piece never falls on its own — gravity is Lesson 07.

> **Common mistake:** The piece moves but passes through where the walls *should* be (col 0 and col 11). This is because in Lesson 06, `tetromino_does_piece_fit` checks against field column 1 and `FIELD_WIDTH - 1` (10), so the wall columns themselves are inaccessible. If your piece stops at the wrong column, check that the bounds check uses `< 1` (not `< 0`) and `>= FIELD_WIDTH - 1` (not `> FIELD_WIDTH - 1`).

> **Common mistake:** Holding a key and noticing the piece moves twice on the first frame. This happens if `handle_action_with_repeat` fires on `half_transition_count > 0` AND the timer accumulates on the same frame. The `return` after handling the "just pressed" case prevents this — the timer starts accumulating on the NEXT frame.

---

## Exercises

1. **Tune DAS/ARR.** Try `initial_delay = 0.0f` for `move_left` and `move_right` in `game_init`. What happens when you press left? Why does the piece move immediately and then pause? Read the DAS timing diagram again.

2. **Add instant drop.** Make the spacebar immediately move the piece down until it hits the floor. Hint: in `tetris_apply_input`, check `input->move_down` for a single press (like rotation) and call `tetromino_does_piece_fit` in a loop, incrementing `y` until it returns 0.

3. **Prevent wrapping.** Comment out the rotation piece-fit check. Press X repeatedly on the I-piece near the right wall. What happens when you rotate at the edge? Re-enable the check and notice how it prevents the piece from being placed outside the field.

---

## What's Next

Lesson 07 introduces the playfield: a `unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]` array where locked pieces are stored permanently. We'll add gravity (automatic downward movement every 0.8 seconds) and piece locking (writing the piece into the field when it can't fall further). We'll also upgrade `tetromino_does_piece_fit` to check against locked cells in the field.
