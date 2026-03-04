# Lesson 08: Line Clearing

## What we're building

After a piece locks, we check the rows it covered for completed lines. If any
row is full, we:
1. Mark it white (`TETRIS_FIELD_TMP_FLASH`) and freeze all game logic
2. Wait 400 ms (the flash)
3. Collapse the completed rows from bottom to top
4. Clear the top row(s)
5. Award points and check for level-up

We also add the full HUD sidebar: score, level, piece count, and next-piece
preview. The game-over overlay is added to `game_render` as well.

```
Flash phase (400 ms):
  row 14: [W][X][X][X][X][X][X][X][X][X][X][W]  ← white (FLASH)
  row 15: [W][.][.][.][.][.][.][.][.][.][.][W]  ← normal
  row 16: [W][X][X][X][X][X][X][X][X][X][X][W]  ← white (FLASH)

Collapse (after timer):
  → row 16 cleared: rows above shift down
  → row 14 cleared: rows above shift down
  → top rows become empty
```

## What you'll learn

- Struct-within-struct: `state->completed_lines.flash_timer`
- An array of integers as a compact "set": `completed_lines.indexes[4]`
- The bottom-to-top collapse order and why top-down is wrong
- `bool completed` as a short-circuit loop flag
- Scoring with bit-shifts: `(1 << count) * 100`
- Level progression and speed scaling

## Prerequisites

- Lesson 07 complete — pieces fall, lock, and the field draws correctly
- You understand the field array and the `row * FIELD_WIDTH + col` formula

---

## Step 1: `completed_lines` — Struct-Within-Struct

We need to track which rows completed this lock event and whether we're currently
in the flash animation. All of this lives in a named anonymous struct inside
`GameState`:

```c
struct {
  int indexes[TETROMINO_LAYER_COUNT]; /* Row numbers of completed lines (max 4) */
  int count;                          /* How many entries are valid             */
  RepeatInterval flash_timer;         /* Active while flashing (timer > 0)      */
} completed_lines;
```

> **Why store row indices instead of re-scanning?** When a piece locks, it
> touches at most 4 rows (its 4×4 bounding box). Scanning only those rows is
> `O(4 × 10)` = 40 comparisons. Re-scanning the entire field on every frame
> during the flash would be `O(18 × 12)` = 216 comparisons per frame for no
> benefit. Storing the indices also makes the collapse loop trivial — we know
> exactly which rows to remove.

> **Why `indexes[TETROMINO_LAYER_COUNT]` (size 4) and not `indexes[18]`?** A
> piece is at most 4 rows tall, so it can only complete at most 4 lines at once.
> (That's the Tetris — all four rows of the I-piece simultaneously.) Allocating
> 18 would waste memory and mislead the reader about the intended invariant.

> **Handmade Hero principle:** State is always visible. The flash phase is stored
> in `GameState.completed_lines.flash_timer`, not a hidden static variable or a
> callback registered somewhere else. If you pause the game and inspect `state`,
> you can see exactly how many milliseconds of flash remain. Nothing is hidden.

Access syntax:
```c
/* JS: state.completedLines.flashTimer.timer */
state->completed_lines.flash_timer.timer = 0.4f;
state->completed_lines.indexes[0] = 14;
state->completed_lines.count = 1;
```

> **Why `struct { ... } completed_lines` instead of a separate `typedef`?**
> This field is only ever used inside `GameState`. Giving it a separate typedef
> would imply it's reusable across the codebase — it isn't. Anonymous struct
> members directly in the parent struct are idiomatic C for "this sub-grouping
> belongs exclusively to its parent."

---

## Step 2: `TETRIS_FIELD_TMP_FLASH` — Adding to an Existing Enum

We add one more value to `TETRIS_FIELD_CELL`:

```c
typedef enum {
  TETRIS_FIELD_EMPTY,      /* 0 */
  TETRIS_FIELD_I,          /* 1 */
  /* ...J, L, O, S, T, Z = 2-7 */
  TETRIS_FIELD_Z,          /* 7 */
  TETRIS_FIELD_WALL,       /* 8 */
  TETRIS_FIELD_TMP_FLASH,  /* 9 — NEW: row completed, flashing white */
} TETRIS_FIELD_CELL;
```

When a completed row is detected, we overwrite all its non-wall cells with
`TETRIS_FIELD_TMP_FLASH`. The renderer draws those cells as white. When the
timer expires, we collapse and replace with real cells from above.

---

## Step 3: Line Detection After Locking

Immediately after stamping the piece into the field, we scan the 4 rows the
piece occupied:

```c
state->completed_lines.count = 0;

for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
  int row_y = state->current_piece.y + py;

  /* Skip rows outside the playable area */
  if (row_y < 0 || row_y >= FIELD_HEIGHT - 1) continue;

  /* Check every playable column in this row */
  bool completed = true;
  for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
    if (state->field[row_y * FIELD_WIDTH + px] == TETRIS_FIELD_EMPTY) {
      completed = false;
      break; /* early exit — one empty cell is enough to disqualify */
    }
  }

  if (completed) {
    /* Mark row as flashing */
    for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
      state->field[row_y * FIELD_WIDTH + px] = TETRIS_FIELD_TMP_FLASH;
    }
    state->completed_lines.indexes[state->completed_lines.count++] = row_y;
  }
}

/* Start flash timer if any lines were completed */
if (state->completed_lines.count > 0) {
  state->completed_lines.flash_timer.timer =
      state->completed_lines.flash_timer.interval; /* 0.4f */
}
```

> **Why `bool completed = true` and break on empty?** This is the classic
> "assume true, disprove" pattern. We start with `completed = true` and look
> for a single counter-example. The moment we find an empty cell, we set
> `completed = false` and `break` — no need to check the remaining cells.
> In JavaScript: `const isComplete = row.every(cell => cell !== EMPTY)`.
> In C, `every()` doesn't exist on arrays, so we write the loop manually.

(see `src/game.c:780-820` in the final version)

---

## Step 4: The Flash Timer — Freezing Game Logic

While the flash is active, we don't process input, gravity, or anything else.
The pattern is an early return in `game_update`:

```c
if (state->completed_lines.flash_timer.timer > 0) {
  state->completed_lines.flash_timer.timer -= delta_time;

  if (state->completed_lines.flash_timer.timer <= 0) {
    /* Timer just expired — collapse rows now */
    /* ... collapse logic ... */
    state->completed_lines.count = 0;
  }
  return; /* ← early return: skip ALL other game logic this frame */
}

/* Normal game logic resumes below */
tetris_apply_input(...);
gravity logic...
```

> **Why `return` instead of an `if/else`?** An early return makes the "frozen"
> state visually obvious. Everything after the `if` block is "normal game".
> The alternative is nesting all normal game logic inside an `else` — this
> creates deep indentation and obscures the control flow. The early return is
> the idiomatic C pattern for "guard clauses." In JavaScript you'd do the same:
> `if (flashTimer > 0) { update timer; return; }` inside `requestAnimationFrame`.

Notice: the `return` fires even if the timer just expired this frame
(`timer <= 0`). This is intentional — we complete the collapse in one frame
and then return. Normal gameplay resumes next frame. This prevents a one-frame
glitch where the game tries to process input with stale rows.

---

## Step 5: Collapse — Bottom-to-Top Order

When the timer expires, we collapse each completed row by shifting all rows
above it down by one. **The order matters critically.**

**Why bottom-to-top?**

Suppose two rows completed: row 14 and row 16.
```
Before:
  row 13: [.][J][J][.][.][.][.][.][.][.][.][.]
  row 14: [W][W][W][W][W][W][W][W][W][W][W][W]  ← FLASH (to remove)
  row 15: [.][.][.][L][L][.][.][.][.][.][.][.]
  row 16: [W][W][W][W][W][W][W][W][W][W][W][W]  ← FLASH (to remove)
```

**If we collapse top-down (row 14 first):**
```
After collapsing row 14 (shifting rows 0-13 down by 1):
  row 14 is now what was row 13: [.][J][J][.][.][.][.][.][.][.][.][.]
  row 15 is now what was row 14: (just been collapsed — empty now? no...)
  row 16: still [W][W][...] — but our stored index says "16"
  After the shift, the former row 15 is now at row 15 (unshifted!),
  and former row 16 is still at 16.
  PROBLEM: we try to collapse "row 16" next — but it's in the right place.
  However, former row 15 content is now at row 15 (unchanged), so the
  data is correct. BUT: in general with multiple gaps, indices become stale.
```

**If we collapse bottom-up (row 16 first):**
```
After collapsing row 16:
  rows 0-15 each shift down by one position
  former row 15 → now row 16
  former row 14 → now row 15 (its index changed from 14 to 15!)

After adjusting: we increment indexes[0]=14 → becomes 15.
Now collapse row 15 (which is the original row 14):
  rows 0-14 shift down → former row 13 → now at row 14
  etc.
→ Correct result: both flash rows removed, content above falls into place.
```

```c
/* Process from bottom (highest index) to top (lowest index) */
for (int i = state->completed_lines.count - 1; i >= 0; i--) {
  int line_index = state->completed_lines.indexes[i];

  /* Shift rows above line_index down by one */
  for (int py = line_index; py > 0; --py) {
    for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
      state->field[py * FIELD_WIDTH + px] =
          state->field[(py - 1) * FIELD_WIDTH + px];
    }
  }

  /* Clear the top row (nothing above it to copy from) */
  for (int px = 1; px < FIELD_WIDTH - 1; px++) {
    state->field[px] = TETRIS_FIELD_EMPTY;  /* row 0 */
  }

  /* Adjust remaining stored indices: rows above line_index shifted down by 1 */
  for (int j = i - 1; j >= 0; --j) {
    if (state->completed_lines.indexes[j] < line_index) {
      state->completed_lines.indexes[j]++;
    }
  }
}
```

(see `src/game.c:718-775` in the final version)

---

## Step 6: Scoring

After all collapses complete, award points:

```c
state->pieces_count++;
state->score += 25; /* flat reward per piece locked */

if (state->completed_lines.count > 0) {
  state->score += (1 << state->completed_lines.count) * 100;
}
```

The `(1 << n)` formula gives exponential rewards for clearing multiple lines:
```
1 line:  (1 << 1) * 100 = 200
2 lines: (1 << 2) * 100 = 400
3 lines: (1 << 3) * 100 = 800
4 lines: (1 << 4) * 100 = 1600  ← Tetris!
```

> **Why bit-shift for scoring?** `1 << n` is identical to `2^n` (powers of 2).
> In JavaScript, `1 << n` works the same way — it's a left bit-shift. Clearing
> 4 lines is worth 8× a single line, not 4×. This exponential bonus strongly
> incentivises stacking for Tetris clears, which is fundamental to high-level
> Tetris strategy.

Level progression:

```c
if (state->pieces_count % 25 == 0) {
  state->level++;
  game_play_sound(&state->audio, SOUND_LEVEL_UP);
}
```

Level affects the drop speed:

```c
int game_speed = state->level > 0 ? state->level : 0;
float drop_interval = state->input_values.rotate_direction.interval
                      + (0.01f + game_speed * 0.01f);
if (drop_interval < 0.1f) drop_interval = 0.1f; /* minimum 10 drops/second */
```

> **Why is the interval calculation additive instead of the speed being stored directly?** The base interval (0.8s) remains untouched — we compute the effective interval each frame. This means changing the base in `game_init` automatically affects all levels. If we stored the speed directly, we'd have to recalculate it every level-up.

---

## Step 7: Rendering the Flash and HUD

`game_render` gains two additions:

**1. `TETRIS_FIELD_TMP_FLASH` → white:**

```c
case TETRIS_FIELD_TMP_FLASH:
  draw_cell(backbuffer, col, row, COLOR_WHITE);
  break;
```

This is a single new `case` in the existing `switch` over `cell_value`.

**2. HUD Sidebar** (requires `utils/draw-text.h`):

```c
#include "utils/draw-text.h"  /* draw_text() */
```

```c
int sx = FIELD_WIDTH * CELL_SIZE + 10;  /* X: just right of field */
int sy = 10;
char buf[32];

draw_text(backbuffer, sx, sy, "SCORE", COLOR_WHITE, 2);
snprintf(buf, sizeof(buf), "%d", state->score);
draw_text(backbuffer, sx, sy + 16, buf, COLOR_YELLOW, 2);

draw_text(backbuffer, sx, sy + 40, "LEVEL", COLOR_WHITE, 2);
snprintf(buf, sizeof(buf), "%d", state->level);
draw_text(backbuffer, sx, sy + 56, buf, COLOR_CYAN, 2);

/* Next piece preview */
draw_text(backbuffer, sx, sy + 85, "NEXT", COLOR_WHITE, 2);
/* ... draw mini piece ... */
```

> **Why `snprintf` instead of `sprintf`?** `snprintf(buf, sizeof(buf), "%d", value)` is the safe version of `sprintf`. It writes at most `sizeof(buf)` bytes, preventing buffer overflow. `sprintf(buf, "%d", value)` has no length limit — if `value` were unexpectedly large and the string longer than `buf`, it would write past the end of the array. In JavaScript, `String(value)` allocates as much memory as needed. In C, we pre-allocate a fixed buffer and must guard against overflow. `snprintf` is always correct; `sprintf` is always a potential bug.

**3. Game-over overlay:**

```c
if (state->is_game_over) {
  int cx = FIELD_WIDTH * CELL_SIZE / 2;
  int cy = FIELD_HEIGHT * CELL_SIZE / 2;
  draw_rect_blend(backbuffer, cx - 80, cy - 50, 160, 100, GAME_RGBA(0, 0, 0, 200));
  draw_rect(backbuffer, cx - 80, cy - 50, 160, 3, COLOR_RED);   /* top border    */
  draw_rect(backbuffer, cx - 80, cy + 47, 160, 3, COLOR_RED);   /* bottom border */
  draw_rect(backbuffer, cx - 80, cy - 50, 3, 100, COLOR_RED);   /* left border   */
  draw_rect(backbuffer, cx + 77, cy - 50, 3, 100, COLOR_RED);   /* right border  */
  draw_text(backbuffer, cx - 54, cy - 30, "GAME OVER", COLOR_RED,   2);
  draw_text(backbuffer, cx - 60, cy,      "R RESTART", COLOR_WHITE, 2);
  draw_text(backbuffer, cx - 45, cy + 20, "Q QUIT",    COLOR_WHITE, 2);
}
```

(see `src/game.c:264-289` in the final version)

---

## Step 8: Complete `game.h` for Lesson 08 (Final)

This is the final `game.h`. Replace `src/game.h`:

```c
/* src/game.h — Lesson 08: Line Clearing (Final) */
#ifndef GAME_H
#define GAME_H

#include "utils/audio.h"
#include "utils/backbuffer.h"
#include <stdbool.h>
#include <stdint.h>

/* ── Field & Cell Constants ── */
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

/* ── Enums ── */
typedef enum {
  TETROMINO_I_IDX, TETROMINO_J_IDX, TETROMINO_L_IDX, TETROMINO_O_IDX,
  TETROMINO_S_IDX, TETROMINO_T_IDX, TETROMINO_Z_IDX,
} TETROMINO_BY_IDX;

/* FINAL: includes TETRIS_FIELD_TMP_FLASH = 9 */
typedef enum {
  TETRIS_FIELD_EMPTY,       /* 0 — no piece, not a wall             */
  TETRIS_FIELD_I,           /* 1 — locked I-piece cell              */
  TETRIS_FIELD_J,           /* 2 — locked J-piece cell              */
  TETRIS_FIELD_L,           /* 3 — locked L-piece cell              */
  TETRIS_FIELD_O,           /* 4 — locked O-piece cell              */
  TETRIS_FIELD_S,           /* 5 — locked S-piece cell              */
  TETRIS_FIELD_T,           /* 6 — locked T-piece cell              */
  TETRIS_FIELD_Z,           /* 7 — locked Z-piece cell              */
  TETRIS_FIELD_WALL,        /* 8 — permanent boundary               */
  TETRIS_FIELD_TMP_FLASH,   /* 9 — completed line, flashing white   */
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

/* ── GameState — final version ── */
typedef struct {

  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
  CurrentPiece  current_piece;

  /* NEW in Lesson 08: scoring and progression */
  int score;
  int pieces_count;
  int level;
  bool is_game_over;

  /* NEW in Lesson 08: line-clear animation */
  struct {
    int indexes[TETROMINO_LAYER_COUNT]; /* row indices of completed lines    */
    int count;                          /* valid entries in indexes[]         */
    RepeatInterval flash_timer;         /* while timer > 0, freeze + flash    */
  } completed_lines;

  struct {
    RepeatInterval move_left;
    RepeatInterval move_right;
    RepeatInterval move_down;
  } input_repeat;

  struct {
    RepeatInterval rotate_direction; /* gravity drop timer (legacy name) */
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

## Step 9: Complete `game.c` for Lesson 08 (Final)

This is the final `game.c`. Replace `src/game.c`:

```c
/* src/game.c — Lesson 08: Line Clearing (Final) */

#include "game.h"
#include "utils/draw-shapes.h"
#include "utils/draw-text.h"    /* draw_text() — NEW in Lesson 08 */
#include <stdio.h>              /* snprintf()                      */
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

/* ── calculate_piece_pan — stereo position for audio ── */
static float calculate_piece_pan(int piece_x) {
  float center     = (FIELD_WIDTH - 2) / 2.0f + 1.0f;
  float max_offset = (FIELD_WIDTH - 2) / 2.0f;
  float offset     = (float)piece_x - center;
  float pan        = (offset / max_offset) * 0.8f;
  if (pan < -1.0f) pan = -1.0f;
  if (pan >  1.0f) pan =  1.0f;
  return pan;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_render — Final version with flash, HUD, and game-over overlay.
 * (see src/game.c:125-290 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
void game_render(Backbuffer *backbuffer, GameState *state) {
  /* 1. Clear to black */
  for (int i = 0; i < backbuffer->width * backbuffer->height; i++) {
    backbuffer->pixels[i] = COLOR_BLACK;
  }

  /* 2. Draw field cells (walls, locked pieces, flash) */
  for (int row = 0; row < FIELD_HEIGHT; row++) {
    for (int col = 0; col < FIELD_WIDTH; col++) {
      unsigned char cell_value = state->field[row * FIELD_WIDTH + col];
      switch (cell_value) {
        case TETRIS_FIELD_EMPTY:   continue;
        case TETRIS_FIELD_I: case TETRIS_FIELD_J: case TETRIS_FIELD_L:
        case TETRIS_FIELD_O: case TETRIS_FIELD_S: case TETRIS_FIELD_T:
        case TETRIS_FIELD_Z:
          draw_cell(backbuffer, col, row, get_tetromino_color(cell_value - 1));
          break;
        case TETRIS_FIELD_WALL:
          draw_cell(backbuffer, col, row, COLOR_GRAY);
          break;
        case TETRIS_FIELD_TMP_FLASH:  /* NEW in Lesson 08 */
          draw_cell(backbuffer, col, row, COLOR_WHITE);
          break;
      }
    }
  }

  /* 3. Draw active piece */
  if (!state->is_game_over) {
    draw_piece(backbuffer,
               state->current_piece.index,
               state->current_piece.x,
               state->current_piece.y,
               get_tetromino_color(state->current_piece.index),
               state->current_piece.rotate_x_value);
  }

  /* 4. HUD Sidebar — NEW in Lesson 08 */
  int sx = FIELD_WIDTH * CELL_SIZE + 10;
  int sy = 10;
  char buf[32];

  draw_text(backbuffer, sx, sy, "SCORE", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->score);
  draw_text(backbuffer, sx, sy + 16, buf, COLOR_YELLOW, 2);

  draw_text(backbuffer, sx, sy + 40, "LEVEL", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->level);
  draw_text(backbuffer, sx, sy + 56, buf, COLOR_CYAN, 2);

  draw_text(backbuffer, sx + 80, sy + 40, "PIECES", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->pieces_count);
  draw_text(backbuffer, sx + 80, sy + 56, buf, COLOR_CYAN, 2);

  draw_text(backbuffer, sx, sy + 85, "NEXT", COLOR_WHITE, 2);
  int preview_x = sx;
  int preview_y = sy + 105;
  int preview_cell = CELL_SIZE / 2;
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (TETROMINOES[state->current_piece.next_index]
                     [tetromino_pos_value(px, py, TETROMINO_R_0)] == TETROMINO_BLOCK) {
        uint32_t color = get_tetromino_color(state->current_piece.next_index);
        draw_rect(backbuffer,
                  preview_x + px * preview_cell + 1,
                  preview_y + py * preview_cell + 1,
                  preview_cell - 2, preview_cell - 2, color);
      }
    }
  }

  int hint_y = FIELD_HEIGHT * CELL_SIZE - 90;
  draw_text(backbuffer, sx, hint_y,       "CONTROLS",           COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 18,  "{} MOVE LEFT/RIGHT", COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 28,  "v  SOFT DROP",       COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 38,  "Z  ROTATE LEFT",     COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 48,  "X  ROTATE RIGHT",    COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 58,  "R  RESTART",         COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 68,  "Q  QUIT",            COLOR_DARK_GRAY, 1);

  /* 5. Game-over overlay */
  if (state->is_game_over) {
    int cx = FIELD_WIDTH * CELL_SIZE / 2;
    int cy = FIELD_HEIGHT * CELL_SIZE / 2;
    draw_rect_blend(backbuffer, cx - 80, cy - 50, 160, 100, GAME_RGBA(0, 0, 0, 200));
    draw_rect(backbuffer, cx - 80, cy - 50, 160,   3, COLOR_RED);
    draw_rect(backbuffer, cx - 80, cy + 47, 160,   3, COLOR_RED);
    draw_rect(backbuffer, cx - 80, cy - 50,   3, 100, COLOR_RED);
    draw_rect(backbuffer, cx + 77, cy - 50,   3, 100, COLOR_RED);
    draw_text(backbuffer, cx - 54, cy - 30, "GAME OVER", COLOR_RED,   2);
    draw_text(backbuffer, cx - 60, cy,      "R RESTART", COLOR_WHITE, 2);
    draw_text(backbuffer, cx - 45, cy + 20, "Q QUIT",    COLOR_WHITE, 2);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_init — Final version.
 * (see src/game.c:294-360 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
void game_init(GameState *state) {
  srand((unsigned int)time(NULL));

  state->score        = 0;
  state->is_game_over = false;
  state->pieces_count = 1;
  state->level        = 0;

  /* Clear line-clear state */
  state->completed_lines.count = 0;
  state->completed_lines.flash_timer.timer    = 0;
  state->completed_lines.flash_timer.interval = 0.4f;
  memset(&state->completed_lines.indexes, 0, sizeof(int) * TETROMINO_LAYER_COUNT);

  /* Gravity timer */
  state->input_values.rotate_direction.timer    = 0.0f;
  state->input_values.rotate_direction.interval = 0.8f;

  /* DAS/ARR */
  state->input_repeat.move_left.initial_delay        = 0.15f;
  state->input_repeat.move_left.interval             = 0.05f;
  state->input_repeat.move_left.passed_initial_delay = false;

  state->input_repeat.move_right.initial_delay        = 0.15f;
  state->input_repeat.move_right.interval             = 0.05f;
  state->input_repeat.move_right.passed_initial_delay = false;

  state->input_repeat.move_down.initial_delay        = 0.0f;
  state->input_repeat.move_down.interval             = 0.03f;
  state->input_repeat.move_down.passed_initial_delay = false;

  /* Field: left wall, right wall, floor */
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1)
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_WALL;
      else
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_EMPTY;
    }
  }

  state->current_piece = (CurrentPiece){
    .x              = (int)((FIELD_WIDTH  * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
    .y              = 0,
    .index          = rand() % TETROMINOS_COUNT,
    .next_index     = rand() % TETROMINOS_COUNT,
    .rotate_x_value = TETROMINO_R_0,
    .next_rotate_x_value = TETROMINO_ROTATE_X_NONE,
  };
}

/* ── tetromino_pos_value ── */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r) {
    case TETROMINO_R_0:   return py * TETROMINO_LAYER_COUNT + px;
    case TETROMINO_R_90:  return 12 + py - (px * TETROMINO_LAYER_COUNT);
    case TETROMINO_R_180: return 15 - (py * TETROMINO_LAYER_COUNT) - px;
    case TETROMINO_R_270: return 3  - py + (px * TETROMINO_LAYER_COUNT);
  }
  return 0;
}

/* ── prepare_input_frame ── */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
  for (int btn = 0; btn < BUTTON_COUNT; btn++) {
    current_input->buttons[btn].ended_down          = old_input->buttons[btn].ended_down;
    current_input->buttons[btn].half_transition_count = 0;
  }
}

/* ── tetromino_does_piece_fit ── */
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

/* ── handle_action_with_repeat (static) ── */
static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval   *repeat,
                                      float             delta_time,
                                      int              *should_trigger) {
  *should_trigger = 0;
  if (!button->ended_down) {
    repeat->timer = 0.0f; repeat->is_active = false;
    repeat->passed_initial_delay = false; return;
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
      *should_trigger = 1; repeat->timer -= repeat->interval;
    }
  }
}

/* ── tetris_apply_input ── */
static void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {
  float pan = calculate_piece_pan(state->current_piece.x);

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
      game_play_sound_at(&state->audio, SOUND_ROTATE, pan);
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
                               state->current_piece.x - 1, state->current_piece.y)) {
    state->current_piece.x--;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }
  if (should_move_right &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x + 1, state->current_piece.y)) {
    state->current_piece.x++;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }

  int should_move_down = 0;
  handle_action_with_repeat(&input->move_down, &state->input_repeat.move_down,
                             delta_time, &should_move_down);
  if (should_move_down &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x, state->current_piece.y + 1)) {
    state->current_piece.y++;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_update — Final version with flash, collapse, and scoring.
 * (see src/game.c:680-922 in the final version)
 * ═══════════════════════════════════════════════════════════════════════════ */
void game_update(GameState *state, GameInput *input, float delta_time) {
  game_audio_update(&state->audio, delta_time);

  if (state->is_game_over) {
    if (state->should_restart) {
      game_init(state);
      if (state->audio.samples_per_second > 0) {
        game_music_play(&state->audio);
        game_play_sound(&state->audio, SOUND_RESTART);
      }
    }
    return;
  }

  /* ── Flash Animation Phase ── */
  if (state->completed_lines.flash_timer.timer > 0) {
    state->completed_lines.flash_timer.timer -= delta_time;

    if (state->completed_lines.flash_timer.timer <= 0) {
      /* Collapse completed rows, bottom-to-top */
      for (int i = state->completed_lines.count - 1; i >= 0; i--) {
        int line_index = state->completed_lines.indexes[i];

        /* Shift all rows above line_index down by one */
        for (int py = line_index; py > 0; --py) {
          for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
            state->field[py * FIELD_WIDTH + px] =
                state->field[(py - 1) * FIELD_WIDTH + px];
          }
        }
        /* Clear row 0 (no content above it) */
        for (int px = 1; px < FIELD_WIDTH - 1; px++) {
          state->field[px] = TETRIS_FIELD_EMPTY;
        }
        /* Adjust remaining indices: rows above line_index shifted down */
        for (int j = i - 1; j >= 0; --j) {
          if (state->completed_lines.indexes[j] < line_index) {
            state->completed_lines.indexes[j]++;
          }
        }
      }

      if (state->completed_lines.count == 4) {
        game_play_sound(&state->audio, SOUND_TETRIS);
      } else if (state->completed_lines.count > 0) {
        game_play_sound(&state->audio, SOUND_LINE_CLEAR);
      }

      state->completed_lines.count = 0;
    }
    return; /* freeze all logic while flash is active */
  }

  if (state->should_restart) {
    game_init(state);
    return;
  }

  /* ── Player Input ── */
  tetris_apply_input(state, input, delta_time);

  /* ── Gravity ── */
  state->input_values.rotate_direction.timer += delta_time;

  int   game_speed    = state->level > 0 ? state->level : 0;
  float drop_interval = state->input_values.rotate_direction.interval
                        + (0.01f + game_speed * 0.01f);
  if (drop_interval < 0.1f) drop_interval = 0.1f;

  if (state->input_values.rotate_direction.timer >= drop_interval) {
    state->input_values.rotate_direction.timer -= drop_interval;

    if (tetromino_does_piece_fit(state, state->current_piece.index,
                                 state->current_piece.rotate_x_value,
                                 state->current_piece.x,
                                 state->current_piece.y + 1)) {
      state->current_piece.y++;

    } else {
      /* ── Lock Piece ── */
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

      float pan = calculate_piece_pan(state->current_piece.x);
      game_play_sound_at(&state->audio, SOUND_DROP, pan);

      /* ── Detect Completed Lines ── */
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
          for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
            state->field[row_y * FIELD_WIDTH + px] = TETRIS_FIELD_TMP_FLASH;
          }
          state->completed_lines.indexes[state->completed_lines.count++] = row_y;
        }
      }

      if (state->completed_lines.count > 0) {
        state->completed_lines.flash_timer.timer =
            state->completed_lines.flash_timer.interval;
      }

      /* ── Scoring ── */
      state->pieces_count++;
      state->score += 25;
      if (state->completed_lines.count > 0) {
        state->score += (1 << state->completed_lines.count) * 100;
      }
      if (state->pieces_count % 25 == 0) {
        state->level++;
        game_play_sound(&state->audio, SOUND_LEVEL_UP);
      }

      /* ── Spawn Next Piece ── */
      state->input_values.rotate_direction.timer = 0.0f;
      state->current_piece = (CurrentPiece){
        .x              = (int)((FIELD_WIDTH  * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
        .y              = 0,
        .index          = state->current_piece.next_index,
        .next_index     = rand() % TETROMINOS_COUNT,
        .rotate_x_value = TETROMINO_R_0,
        .next_rotate_x_value = TETROMINO_ROTATE_X_NONE,
      };

      /* ── Game Over Check ── */
      if (!tetromino_does_piece_fit(state, state->current_piece.index,
                                    state->current_piece.rotate_x_value,
                                    state->current_piece.x,
                                    state->current_piece.y)) {
        state->is_game_over = true;
        game_play_sound(&state->audio, SOUND_GAME_OVER);
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
These are now identical to the final `src/game.h` and `src/game.c` in the repository.

---

## Expected Result

- Pieces fall, move, rotate, and lock as in Lesson 07.
- When a row is completed, it flashes white for ~400 ms.
- After the flash, the row disappears and all rows above collapse down.
- The score updates in the sidebar: +25 per piece, +200/400/800/1600 for 1/2/3/4 lines.
- After 25 pieces, "LEVEL" increments and pieces fall slightly faster.
- The "NEXT" piece is shown in the sidebar.
- On game over, a red-bordered overlay shows "GAME OVER / R RESTART / Q QUIT".
- Pressing R restarts with score and level reset to 0.

> **Common mistake:** Lines clear but rows don't collapse correctly — pieces
> "teleport" to the wrong rows. This is the stale-index bug from collapsing
> top-down. Make sure the outer loop goes `for (int i = count - 1; i >= 0; i--)`,
> not `for (int i = 0; i < count; i++)`. And make sure the inner index-adjustment
> loop increments indices LESS THAN `line_index`, not greater than.

> **Common mistake:** Flash shows for a split second and immediately collapses.
> Check that `flash_timer.interval` is set to `0.4f` in `game_init` and that
> the render draws `TETRIS_FIELD_TMP_FLASH` as white BEFORE the field-drawing
> loop runs. If TMP_FLASH cells aren't rendered, you won't see the flash even
> though the timer fires correctly.

> **Common mistake:** Score never increments. Verify `state->pieces_count++`
> runs in the locking branch, not in the gravity-tick branch. It should be
> inside the `else { /* Piece Locked */ }` block, not after the outer
> `if (timer >= drop_interval)` check.

---

## Exercises

1. **Change the flash duration.** In `game_init`, change
   `state->completed_lines.flash_timer.interval` from `0.4f` to `0.1f`. Does
   the flash feel too fast? Try `1.5f`. At what duration does it start to feel
   frustrating?

2. **Verify bottom-to-top collapse.** Fill rows 14 and 16 in `game_init` (leave
   row 15 partially empty). Play until you clear exactly rows 14 and 16 in one
   lock event. Pause with a printf inside the collapse loop to trace the
   `indexes[]` array before and after each collapse.

3. **Tetris score.** Stack an I-piece repeatedly to set up a Tetris (4 lines
   simultaneously). Verify the score jumps by exactly 1625 (25 pieces + 1600
   Tetris bonus).

4. **Level scaling.** Log `drop_interval` to stdout on every gravity tick. At
   level 0, is it 0.81? At level 5, does it approach the 0.1 minimum? What
   level causes the interval to hit the floor?

---

## What's Next

The core game loop is complete. Future lessons could cover:

- **Lesson 09: Game Phases** — refactor `is_game_over` and `flash_timer > 0`
  into a proper `GAME_PHASE` enum (`PLAYING`, `FLASHING`, `GAME_OVER`,
  `PAUSED`). Right now we're using a boolean timer as a proxy for a phase flag;
  the enum is the natural upgrade that makes adding more phases trivial.

- **Lesson 10: Audio Sequencer** — how `game_get_audio_samples` synthesises PCM
  directly, the MIDI-to-frequency formula, and the music pattern sequencer in
  `src/audio.c`.

- **Lesson 11: Platform Layer Deep-Dive** — why `main_raylib.c` and `main_x11.c`
  are interchangeable, how `GameInput`'s union works at the memory level, and
  why the platform never calls back into the game.
