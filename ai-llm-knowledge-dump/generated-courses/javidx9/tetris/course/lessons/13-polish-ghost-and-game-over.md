# Lesson 13: Polish — Ghost Piece & Game Over

## What we're building

Three finishing touches that transform our prototype into a complete game:

1. **Ghost piece** — a translucent preview showing exactly where the current piece will land. Drawn with a dimmed version of the piece colour before the current piece, so the active piece always renders on top.
2. **Game-over overlay** — a semi-transparent black rectangle with a red border, "GAME OVER" text, and restart/quit prompts. Uses `draw_rect_blend` for the alpha-blended background.
3. **Clean restart** — pressing R at any time (including on the game-over screen) calls `game_init()`, reloads the music, and plays the restart sound.

After this lesson the game is **complete**.

## What you'll learn

- Ghost piece algorithm: find the lowest valid Y for the current piece
- Multiple render passes (painter's algorithm): why order matters
- `draw_rect_blend` for alpha-blended overlays — CSS `rgba()` equivalent
- `is_game_over` flag: adding a field to `GameState`
- Why `game_init()` is the single source of truth for a fresh game state

## Prerequisites

Lesson 12 complete: the game has scoring, a text HUD, sound effects, and background music. It plays indefinitely — there is currently no game-over state.

---

## Step 1: The Ghost Piece Algorithm

The ghost piece shows where the active piece would land if you dropped it right now. The algorithm is a simple loop:

```c
int ghost_y = state->current_piece.y;
while (tetromino_does_piece_fit(state, state->current_piece.index,
                                 state->current_piece.rotate_x_value,
                                 state->current_piece.x, ghost_y + 1)) {
    ghost_y++;
}
/* Now ghost_y is the lowest row where the piece fits */
```

We reuse `tetromino_does_piece_fit` — the same collision function used by gravity. Starting at the current Y, we keep moving down by 1 until the piece would no longer fit. The last valid Y is where the ghost renders.

### Render order matters

```
Painter's algorithm (back → front):
  1. Clear to black
  2. Locked field cells
  3. Ghost piece  ← BEFORE current piece
  4. Current piece
  5. HUD sidebar
  6. Game-over overlay (if is_game_over)
```

The ghost MUST be drawn before the active piece so that where they overlap, the active piece is visible on top. If we drew the ghost after the current piece, the ghost's dimmed colour would paint over the brightly-coloured active piece at the spawn row — confusing to the player.

> **Why?** This is the **painter's algorithm** — the same principle as CSS `z-index` or canvas `globalCompositeOperation`. You draw further-back elements first; each subsequent draw can overwrite whatever is beneath it. In a game backbuffer there are no layers, no z-tests — only sequential pixel writes. Drawing order IS the depth order.
>
> JS equivalent: multiple `ctx.drawImage` calls in a specific order on a single canvas, or multiple absolutely-positioned `<div>` elements where the last one in the DOM sits on top.

### Ghost colour: dimmed piece colour

```c
/* Darken the piece colour by averaging each channel with dark gray */
uint32_t piece_color = get_tetromino_color(state->current_piece.index);
uint8_t r = ((piece_color >> 16) & 0xFF) / 4;  /* dim to 25% */
uint8_t g = ((piece_color >>  8) & 0xFF) / 4;
uint8_t b = ( piece_color        & 0xFF) / 4;
uint32_t ghost_color = GAME_RGB(r, g, b);
```

Dividing each channel by 4 gives 25% of the original brightness — clearly visible as a hint without competing with the actual piece.

---

## Step 2: The Game-Over Overlay

When the newly-spawned piece cannot fit at the spawn position, the game is over. We set `is_game_over = true` and stop the music.

The overlay is drawn using `draw_rect_blend` — a rectangle with an alpha value:

```c
draw_rect_blend(backbuffer, cx - 80, cy - 50, 160, 100, GAME_RGBA(0, 0, 0, 200));
```

`GAME_RGBA(0, 0, 0, 200)` is black with `alpha = 200 / 255 ≈ 78%` opacity. Each pixel in the box is blended:

```
out = (src * alpha + dst * (255 - alpha)) / 255
    = (black * 200 + field_pixel * 55) / 255
```

The field content is still slightly visible through the overlay (22%), which looks better than a fully opaque black box — it reminds the player of the final state they reached.

> **Why `draw_rect_blend` instead of `draw_rect`?**
> `draw_rect` ignores the alpha channel and always fully overwrites the destination pixel. `draw_rect_blend` reads the alpha byte from the colour and blends. They share the same clip logic; the only difference is in the per-pixel write.
>
> JS equivalent:
> ```js
> ctx.globalAlpha = 200 / 255;
> ctx.fillStyle = 'black';
> ctx.fillRect(cx - 80, cy - 50, 160, 100);
> ctx.globalAlpha = 1;
> ```
> Or CSS: `background-color: rgba(0, 0, 0, 0.78)`.

A red border is drawn with four thin `draw_rect` calls — top, bottom, left, right edges. These are fully opaque (no blending needed), giving the overlay a clear frame that stands out against the dimmed field.

---

## Step 3: Add `is_game_over` to `GameState`

One new field in `GameState`, in `game.h`:

```c
bool is_game_over;  /* true when new piece can't fit at spawn position */
```

Reset it in `game_init()`:

```c
state->is_game_over = false;
```

Game-over detection goes in `game_update`, after spawning the next piece:

```c
/* Game-over check: if the new piece can't fit at spawn, game is over */
if (!tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x,
                               state->current_piece.y)) {
    state->is_game_over = true;
    game_play_sound(&state->audio, SOUND_GAME_OVER);
    game_music_stop(&state->audio);
}
```

And the update loop returns early when the game is over, except for the music update (which keeps running) and the restart check:

```c
void game_update(GameState *state, GameInput *input, float delta_time) {
  game_audio_update(&state->audio, delta_time);  /* always advance music timer */

  if (state->is_game_over) {
    if (state->should_restart) {
      game_init(state);                             /* reset ALL state */
      game_music_play(&state->audio);               /* restart music   */
      game_play_sound(&state->audio, SOUND_RESTART);
    }
    return;  /* nothing else to update while game over */
  }
  /* ... rest of update unchanged ... */
```

> **Why call `game_init(state)` for restart?**
> `game_init` is the single function that knows what a fresh game looks like. Rather than writing a separate "reset" function or manually zeroing fields, we reuse the same initialization code that runs at startup. This is the Handmade Hero philosophy: one function, one responsibility, called once. If you later add a new field to `GameState`, you only need to initialize it in `game_init` — restarts automatically get the correct value.

---

## Step 4: Final `src/game.h`

**`src/game.h`** — complete final file (one new field: `is_game_over`):

```c
#ifndef GAME_H
#define GAME_H

#include "utils/audio.h"
#include "utils/backbuffer.h"
#include <stdbool.h>
#include <stdint.h>

/* ── Constants ──────────────────────────────────────────────────────────── */
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

/* ── Enums (unchanged) ──────────────────────────────────────────────────── */
typedef enum { TETROMINO_I_IDX, TETROMINO_J_IDX, TETROMINO_L_IDX,
               TETROMINO_O_IDX, TETROMINO_S_IDX, TETROMINO_T_IDX,
               TETROMINO_Z_IDX } TETROMINO_BY_IDX;

typedef enum { TETRIS_FIELD_EMPTY, TETRIS_FIELD_I, TETRIS_FIELD_J,
               TETRIS_FIELD_L, TETRIS_FIELD_O, TETRIS_FIELD_S,
               TETRIS_FIELD_T, TETRIS_FIELD_Z, TETRIS_FIELD_WALL,
               TETRIS_FIELD_TMP_FLASH } TETRIS_FIELD_CELL;

typedef enum { TETROMINO_R_0, TETROMINO_R_90,
               TETROMINO_R_180, TETROMINO_R_270 } TETROMINO_R_DIR;

typedef enum { TETROMINO_ROTATE_X_NONE, TETROMINO_ROTATE_X_GO_LEFT,
               TETROMINO_ROTATE_X_GO_RIGHT } TETROMINO_ROTATE_X_VALUE;

typedef struct {
  float timer;
  float initial_delay;
  float interval;
  bool is_active;
  bool passed_initial_delay;
} RepeatInterval;

typedef struct {
  int x, y;
  TETROMINO_BY_IDX index;
  TETROMINO_BY_IDX next_index;
  TETROMINO_R_DIR rotate_x_value;
  TETROMINO_ROTATE_X_VALUE next_rotate_x_value;
} CurrentPiece;

typedef struct { int half_transition_count; int ended_down; } GameButtonState;

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
 * GameState — final complete version
 *
 * NEW THIS LESSON: is_game_over
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {

  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
  CurrentPiece current_piece;

  int score;
  int pieces_count;
  int level;

  /* NEW: set true when the newly-spawned piece can't fit at spawn position.
   * game_render draws the overlay when this is true.
   * game_update ignores all input except restart when this is true.
   */
  bool is_game_over;

  struct {
    int indexes[TETROMINO_LAYER_COUNT];
    int count;
    RepeatInterval flash_timer;
  } completed_lines;

  struct {
    RepeatInterval move_left;
    RepeatInterval move_right;
    RepeatInterval move_down;
  } input_repeat;

  struct {
    RepeatInterval rotate_direction;
  } input_values;

  GameAudioState audio;

  int should_quit;
  int should_restart;

} GameState;

extern const char *TETROMINOES[7];

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

## Step 5: Final `src/game.c` — Complete

This is the complete final `game.c`. New in this lesson:
- `calculate_piece_pan` helper
- Ghost piece calculation and rendering in `game_render`
- Game-over overlay in `game_render`
- `is_game_over` check and game-over detection in `game_update`

**`src/game.c`** — complete final file (see `src/game.c` in the course repository):

```c
/* game.c — Final: Ghost Piece, Game Over, Complete Tetris */

#include "game.h"
#include "utils/draw-shapes.h"
#include "utils/draw-text.h"

#include <stdio.h>
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

/* ── Private Helpers ────────────────────────────────────────────────────── */
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
      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK)
        draw_cell(bb, field_col + px, field_row + py, color);
    }
  }
}

/* calculate_piece_pan: map piece column to stereo pan [-0.8, +0.8].
 * Pieces near the left wall pan left; near the right wall, pan right.
 * Cap at ±0.8 so audio is never completely absent from one ear.
 * See src/game.c:578-592 for full explanation.
 */
static float calculate_piece_pan(int piece_x) {
  float center     = (FIELD_WIDTH - 2) / 2.0f + 1.0f;
  float max_offset = (FIELD_WIDTH - 2) / 2.0f;
  float offset     = (float)piece_x - center;
  float pan        = (offset / max_offset) * 0.8f;
  if (pan < -1.0f) pan = -1.0f;
  if (pan >  1.0f) pan =  1.0f;
  return pan;
}

/* ════════════════════════════════════════════════════════════════════════════
 * game_render — Final version with ghost piece and game-over overlay
 * Render order: clear → field → ghost → active piece → HUD → overlay
 * (see src/game.c:159-282)
 * ════════════════════════════════════════════════════════════════════════════
 */
void game_render(Backbuffer *backbuffer, GameState *state) {

  /* 1. Clear to black */
  for (int i = 0; i < backbuffer->width * backbuffer->height; i++)
    backbuffer->pixels[i] = COLOR_BLACK;

  /* 2. Draw locked field cells */
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

  /* ── 3. NEW: Ghost Piece ─────────────────────────────────────────────────
   *
   * ALGORITHM (see Step 1):
   *   Start at current piece Y.
   *   Keep moving down until the piece would no longer fit.
   *   Draw at the last valid Y with a dimmed colour.
   *
   * Draw BEFORE the active piece so active piece renders on top at overlap.
   *
   * Ghost colour = 25% brightness of the piece colour.
   * Channel math: (packed_color >> shift) & 0xFF  extracts one channel byte.
   * Dividing by 4 (right-shift by 2) gives 25%.
   *
   * JS: const [r, g, b] = hexToRgb(pieceColor).map(c => c >> 2);
   */
  {
    int ghost_y = state->current_piece.y;
    while (tetromino_does_piece_fit(state,
                                    state->current_piece.index,
                                    state->current_piece.rotate_x_value,
                                    state->current_piece.x,
                                    ghost_y + 1)) {
      ghost_y++;
    }

    /* Only draw if ghost is below current piece position */
    if (ghost_y > state->current_piece.y) {
      uint32_t piece_color = get_tetromino_color(state->current_piece.index);
      uint8_t r = (uint8_t)(((piece_color >> 16) & 0xFF) / 4);
      uint8_t g = (uint8_t)(((piece_color >>  8) & 0xFF) / 4);
      uint8_t b = (uint8_t)(( piece_color        & 0xFF) / 4);
      uint32_t ghost_color = GAME_RGB(r, g, b);

      draw_piece(backbuffer,
                 state->current_piece.index,
                 state->current_piece.x,
                 ghost_y,
                 ghost_color,
                 state->current_piece.rotate_x_value);
    }
  }

  /* 4. Draw active falling piece (on top of ghost) */
  draw_piece(backbuffer,
             state->current_piece.index,
             state->current_piece.x,
             state->current_piece.y,
             get_tetromino_color(state->current_piece.index),
             state->current_piece.rotate_x_value);

  /* 5. HUD Sidebar (unchanged from lesson 10) */
  int sx = FIELD_WIDTH * CELL_SIZE + 10;
  int sy = 10;
  char buf[32];

  draw_text(backbuffer, sx, sy,      "SCORE",  COLOR_WHITE,  2);
  snprintf(buf, sizeof(buf), "%d", state->score);
  draw_text(backbuffer, sx, sy + 16, buf,      COLOR_YELLOW, 2);

  draw_text(backbuffer, sx,      sy + 40, "LEVEL",  COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->level);
  draw_text(backbuffer, sx,      sy + 56, buf,      COLOR_CYAN,  2);

  draw_text(backbuffer, sx + 80, sy + 40, "PIECES", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->pieces_count);
  draw_text(backbuffer, sx + 80, sy + 56, buf,      COLOR_CYAN,  2);

  draw_text(backbuffer, sx, sy + 85, "NEXT", COLOR_WHITE, 2);

  int preview_x         = sx;
  int preview_y         = sy + 105;
  int preview_cell_size = CELL_SIZE / 2;
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (TETROMINOES[state->current_piece.next_index]
                     [tetromino_pos_value(px, py, TETROMINO_R_0)] == TETROMINO_BLOCK) {
        uint32_t color = get_tetromino_color(state->current_piece.next_index);
        draw_rect(backbuffer,
                  preview_x + px * preview_cell_size + 1,
                  preview_y + py * preview_cell_size + 1,
                  preview_cell_size - 2, preview_cell_size - 2, color);
      }
    }
  }

  int hint_y = FIELD_HEIGHT * CELL_SIZE - 90;
  draw_text(backbuffer, sx, hint_y,      "CONTROLS",           COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 18, "{} MOVE LEFT/RIGHT", COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 28, "v  SOFT DROP",       COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 38, "Z  ROTATE LEFT",     COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 48, "X  ROTATE RIGHT",    COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 58, "R  RESTART",         COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 68, "Q  QUIT",            COLOR_DARK_GRAY, 1);

  /* ── 6. NEW: Game-Over Overlay ───────────────────────────────────────────
   *
   * Only rendered when state->is_game_over == true.
   * Draws on top of everything else (rendered last = always visible).
   *
   * cx, cy = centre of the field in pixels.
   * draw_rect_blend: alpha-blended rectangle.
   *   GAME_RGBA(0, 0, 0, 200) = black at 200/255 ≈ 78% opacity.
   *   The field content shows faintly through (22%) — better than solid black.
   *
   * JS equivalent:
   *   ctx.globalAlpha = 200/255; ctx.fillStyle='black'; ctx.fillRect(...)
   *   ctx.globalAlpha = 1;
   *
   * Red border: four thin draw_rect calls (top, bottom, left, right edges).
   * These are fully opaque — no blending needed.
   *
   * (see src/game.c:264-282)
   */
  if (state->is_game_over) {
    int cx = FIELD_WIDTH  * CELL_SIZE / 2;
    int cy = FIELD_HEIGHT * CELL_SIZE / 2;

    /* Semi-transparent black backdrop */
    draw_rect_blend(backbuffer, cx - 80, cy - 50, 160, 100, GAME_RGBA(0, 0, 0, 200));

    /* Red border (4 separate rects: top, bottom, left, right) */
    draw_rect(backbuffer, cx - 80, cy - 50, 160, 3,   COLOR_RED);
    draw_rect(backbuffer, cx - 80, cy + 47, 160, 3,   COLOR_RED);
    draw_rect(backbuffer, cx - 80, cy - 50,   3, 100, COLOR_RED);
    draw_rect(backbuffer, cx + 77, cy - 50,   3, 100, COLOR_RED);

    /* Text */
    draw_text(backbuffer, cx - 54, cy - 30, "GAME OVER", COLOR_RED,   2);
    draw_text(backbuffer, cx - 60, cy,      "R RESTART", COLOR_WHITE, 2);
    draw_text(backbuffer, cx - 45, cy + 20, "Q QUIT",    COLOR_WHITE, 2);
  }
}

/* ── Initialization ─────────────────────────────────────────────────────── */
void game_init(GameState *state) {
  state->score        = 0;
  state->pieces_count = 1;
  state->level        = 0;
  state->is_game_over = false;  /* NEW */

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
      state->field[y * FIELD_WIDTH + x] =
        (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1)
        ? TETRIS_FIELD_WALL : TETRIS_FIELD_EMPTY;
    }
  }

  srand((unsigned int)time(NULL));
  state->current_piece = (CurrentPiece){
    .x              = (int)((FIELD_WIDTH * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
    .y              = 0,
    .index          = rand() % TETROMINOS_COUNT,
    .next_index     = rand() % TETROMINOS_COUNT,
    .rotate_x_value = TETROMINO_R_0,
  };
}

/* ── Rotation & Collision ───────────────────────────────────────────────── */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r) {
    case TETROMINO_R_0:   return py * TETROMINO_LAYER_COUNT + px;
    case TETROMINO_R_90:  return 12 + py - (px * TETROMINO_LAYER_COUNT);
    case TETROMINO_R_180: return 15 - (py * TETROMINO_LAYER_COUNT) - px;
    case TETROMINO_R_270: return 3  - py + (px * TETROMINO_LAYER_COUNT);
  }
  return 0;
}

int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                              int pos_x, int pos_y) {
  if (piece < 0 || piece >= TETROMINOS_COUNT) return 0;
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece][pi] == TETROMINO_SPAN) continue;
      int field_x = pos_x + px, field_y = pos_y + py;
      if (field_x < 0 || field_x >= FIELD_WIDTH)  continue;
      if (field_y < 0 || field_y >= FIELD_HEIGHT)  continue;
      if (state->field[field_y * FIELD_WIDTH + field_x] != TETRIS_FIELD_EMPTY)
        return 0;
    }
  }
  return 1;
}

/* ── Input ──────────────────────────────────────────────────────────────── */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
  for (int btn = 0; btn < BUTTON_COUNT; btn++) {
    current_input->buttons[btn].ended_down            = old_input->buttons[btn].ended_down;
    current_input->buttons[btn].half_transition_count = 0;
  }
}

static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval *repeat,
                                      float delta_time, int *should_trigger) {
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

static void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {
  float pan = calculate_piece_pan(state->current_piece.x);

  if (input->rotate_x.ended_down && input->rotate_x.half_transition_count != 0 &&
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

  int sl = 0, sr = 0;
  handle_action_with_repeat(&input->move_left,  &state->input_repeat.move_left,  delta_time, &sl);
  handle_action_with_repeat(&input->move_right, &state->input_repeat.move_right, delta_time, &sr);
  if (sl && tetromino_does_piece_fit(state, state->current_piece.index,
       state->current_piece.rotate_x_value, state->current_piece.x - 1, state->current_piece.y)) {
    state->current_piece.x--;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }
  if (sr && tetromino_does_piece_fit(state, state->current_piece.index,
       state->current_piece.rotate_x_value, state->current_piece.x + 1, state->current_piece.y)) {
    state->current_piece.x++;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }
  int sd = 0;
  handle_action_with_repeat(&input->move_down, &state->input_repeat.move_down, delta_time, &sd);
  if (sd && tetromino_does_piece_fit(state, state->current_piece.index,
       state->current_piece.rotate_x_value, state->current_piece.x, state->current_piece.y + 1)) {
    state->current_piece.y++;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }
}

/* ════════════════════════════════════════════════════════════════════════════
 * game_update — Final version
 * (see src/game.c:699-921)
 * ════════════════════════════════════════════════════════════════════════════
 */
void game_update(GameState *state, GameInput *input, float delta_time) {

  /* Music always advances, even when game is paused or over */
  game_audio_update(&state->audio, delta_time);

  /* ── NEW: Handle game-over state ──
   * While game over: only restart input is processed.
   * game_init() resets ALL state — the single source of truth for a new game.
   */
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

  /* Flash animation pause */
  if (state->completed_lines.flash_timer.timer > 0) {
    state->completed_lines.flash_timer.timer -= delta_time;
    if (state->completed_lines.flash_timer.timer <= 0) {
      for (int i = state->completed_lines.count - 1; i >= 0; i--) {
        int line_index = state->completed_lines.indexes[i];
        for (int py = line_index; py > 0; --py)
          for (int px = 1; px < FIELD_WIDTH - 1; ++px)
            state->field[py * FIELD_WIDTH + px] = state->field[(py-1) * FIELD_WIDTH + px];
        for (int px = 1; px < FIELD_WIDTH - 1; px++) state->field[px] = TETRIS_FIELD_EMPTY;
        for (int j = i - 1; j >= 0; --j)
          if (state->completed_lines.indexes[j] < line_index)
            state->completed_lines.indexes[j]++;
      }
      /* Play sound after collapse */
      if (state->completed_lines.count == 4)
        game_play_sound(&state->audio, SOUND_TETRIS);
      else if (state->completed_lines.count > 0)
        game_play_sound(&state->audio, SOUND_LINE_CLEAR);
      state->completed_lines.count = 0;
    }
    return;
  }

  /* Player input */
  tetris_apply_input(state, input, delta_time);

  /* Gravity */
  state->input_values.rotate_direction.timer += delta_time;
  int game_speed  = state->level;
  float drop_interval = state->input_values.rotate_direction.interval
                       - (0.01f + game_speed * 0.01f);
  if (drop_interval < 0.1f) drop_interval = 0.1f;

  if (state->input_values.rotate_direction.timer >= drop_interval) {
    state->input_values.rotate_direction.timer -= drop_interval;

    if (tetromino_does_piece_fit(state, state->current_piece.index,
         state->current_piece.rotate_x_value,
         state->current_piece.x, state->current_piece.y + 1)) {
      state->current_piece.y++;
    } else {
      /* Lock piece into field */
      for (int px = 0; px < TETROMINO_LAYER_COUNT; ++px) {
        for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
          int pi = tetromino_pos_value(px, py, state->current_piece.rotate_x_value);
          if (TETROMINOES[state->current_piece.index][pi] != TETROMINO_SPAN) {
            int fx = state->current_piece.x + px, fy = state->current_piece.y + py;
            if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT)
              state->field[fy * FIELD_WIDTH + fx] = state->current_piece.index + 1;
          }
        }
      }

      float pan = calculate_piece_pan(state->current_piece.x);
      game_play_sound_at(&state->audio, SOUND_DROP, pan);

      /* Detect completed lines */
      state->completed_lines.count = 0;
      for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
        int row_y = state->current_piece.y + py;
        if (row_y < 0 || row_y >= FIELD_HEIGHT - 1) continue;
        bool completed = true;
        for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
          if (state->field[row_y * FIELD_WIDTH + px] == TETRIS_FIELD_EMPTY) {
            completed = false; break;
          }
        }
        if (completed) {
          for (int px = 1; px < FIELD_WIDTH - 1; ++px)
            state->field[row_y * FIELD_WIDTH + px] = TETRIS_FIELD_TMP_FLASH;
          state->completed_lines.indexes[state->completed_lines.count++] = row_y;
        }
      }
      if (state->completed_lines.count > 0)
        state->completed_lines.flash_timer.timer = state->completed_lines.flash_timer.interval;

      /* Scoring */
      state->pieces_count++;
      state->score += 25;
      if (state->completed_lines.count > 0)
        state->score += (1 << state->completed_lines.count) * 100;
      if (state->pieces_count % 25 == 0) {
        state->level++;
        game_play_sound(&state->audio, SOUND_LEVEL_UP);
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

      /* ── NEW: Game-Over Check ─────────────────────────────────────────────
       * If the new piece doesn't fit at its spawn position, the field is full.
       * Set is_game_over; future frames skip all logic except restart.
       * (see src/game.c:910-918)
       */
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
clang -Wall -Wextra -g -O0 \
  -o build/game \
  src/game.c src/audio.c src/utils/draw-shapes.c src/utils/draw-text.c \
  src/main_raylib.c \
  -lraylib -lpthread -ldl -lm

./build/game
```

Or use the build script: `./build-dev.sh`

---

## Expected Result

The game is now **complete**:

- A **dim ghost piece** shows where the active piece will land
- The field fills up and eventually a newly-spawned piece can't fit
- A **semi-transparent "GAME OVER" overlay** appears over the field with a red border
- The music stops on game over
- Pressing **R** restarts: game state resets, music plays from the beginning, a restart sound plays
- Pressing **Q** or **Escape** quits

---

## Common Mistakes

> **Common mistake:** Drawing the ghost piece AFTER the active piece. The active piece's cells get overwritten by the dimmed ghost colour at the spawn area — the piece looks faded and confusing. The ghost MUST come before the active piece in the render order.

> **Handmade Hero principle:** State is always visible. `is_game_over` lives in `GameState` alongside every other piece of game state. There is no hidden flag, no static variable inside a function, no event queue saying "game ended". Any code that can read `state` can check whether the game is over — and that code is `game_update` and `game_render`, which already hold `state`.

> **Common mistake:** Calling `game_init(state)` without re-calling `game_music_play` and `game_play_sound(SOUND_RESTART)` afterward. `game_init` resets `audio` fields (via `memset(0)`) so the music sequencer is at step 0 with `is_playing = 0`. You must explicitly call `game_music_play` to re-arm the sequencer.

> **Common mistake:** Putting the game-over check BEFORE spawning the next piece. If you check game-over with the OLD piece still as current, `tetromino_does_piece_fit` returns 0 (the piece is sitting somewhere else on the field). You must spawn the new piece first, THEN check if it fits.

---

## Exercises

1. Make the ghost piece blink (flash on/off) by tracking a small timer. Flip a `bool ghost_visible` every 0.5 seconds. Where is the cleanest place to put this timer?
2. Add a high score that persists across restarts. The `game_init` function resets most state — how do you preserve `high_score` across calls to `game_init`? (Hint: save it before calling `game_init`, restore it after.)
3. Change the overlay to use `alpha = 128` (50%) instead of 200. How does the visual feel change? What's the trade-off between readability and immersion?
4. Add a "PAUSED" overlay: when the player presses P, freeze the game loop (don't call `game_update`) and display a "PAUSED" overlay. Note that you should still call `game_render` each frame.

---

## 🎉 Course Complete!

You have built a **complete Tetris game in C**, from scratch. Here is everything you built across lessons 01-13:

### Engine layer (platform-independent)
- **Custom software renderer**: pixel-perfect rect drawing, alpha blending
- **Bitmap font**: 5×7 hand-crafted glyphs, bit-packed into bytes
- **Backbuffer** (framebuffer): flat `uint32_t` pixel array, pitch-correct row access
- **Platform abstraction**: `GameState`, `GameInput`, `Backbuffer`, `AudioOutputBuffer` — zero OS dependencies

### Game logic (game.c)
- **Tetromino data**: 7 pieces as 16-character strings; rotation via index math
- **Collision detection**: `tetromino_does_piece_fit` with bounds clipping
- **Gravity and locking**: delta-time gravity timer, piece stamping
- **Line clear**: detection, flash animation, bottom-to-top collapse
- **DAS/ARR input**: Delayed Auto Shift + Auto Repeat Rate for smooth movement
- **Scoring**: 25 pts per lock + exponential line-clear bonus (`1 << n`) * 100
- **Levels**: speed scaling every 25 pieces, drop-interval floor at 0.1s
- **HUD**: `snprintf` number formatting, `draw_text` sidebar, next-piece preview
- **Ghost piece**: real-time collision-based drop preview
- **Game over**: spawn-collision detection, overlay, clean restart via `game_init`

### Audio (audio.c)
- **Phase accumulator oscillator**: the universal synth primitive
- **Square wave synthesis**: `(phase < 0.5) ? +1 : -1`
- **Frequency slide**: pitch bend for dramatic SFX
- **Fade-in envelope**: 96-sample attack to eliminate click artefacts
- **Stereo panning**: linear pan law, piece-position-based spatial audio
- **Slot management**: 4-voice polyphony with oldest-first steal
- **Korobeiniki sequencer**: MIDI-to-Hz conversion, 4 patterns × 16 steps
- **Volume ramping**: 0.002/sample smooth transitions to prevent note clicks

### C concepts learned
`struct`, `enum`, `typedef`, `union`, `static`, `extern`, `inline`, pointers and pointer arithmetic, flat 2D arrays (`row * width + col`), bit manipulation (`<<`, `>>`, `&`, `|`), `memset`, `memcpy`, `snprintf`, `malloc`/`free`, `int16_t`/`uint8_t`/`uint32_t`, `sizeof`, `NULL` checks, null-terminated strings, designated array initializers, compound literals, `static` local functions, header include guards, separate translation units, the C linker model.

### What to explore next
- **Wall kicks**: standard Tetris SRS rotation system (pieces can shift to avoid collisions when rotating near walls)
- **Hard drop**: instant drop to ghost position on Space key
- **Hold piece**: store one piece for later use
- **Save to file**: persist high score using `fopen`/`fprintf`/`fscanf`
- **X11 backend**: build the same game targeting Linux's raw X11 — compare the platform layer to Raylib
- **Handmade Hero**: watch Casey Muratori build a full game engine from scratch using the same principles you just practiced
