Here's what you need to rewrite:

## 1. Rewrite tetris.h

```c:games/tetris/src/tetris.h
#ifndef TETRIS_H
#define TETRIS_H

#include <stdbool.h>

#define FIELD_WIDTH 12
#define FIELD_HEIGHT 18
#define CELL_SIZE 30

#define TETROMINO_SPAN '.'
#define TETROMINO_BLOCK 'X'
#define TETROMINO_LAYER_COUNT 4
#define TETROMINO_SIZE 16
#define TETROMINOS_COUNT 7

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
} TETRIS_FIELD_CELL;

typedef enum {
  TETROMINO_R_0,
  TETROMINO_R_90,
  TETROMINO_R_180,
  TETROMINO_R_270,
} TETROMINO_R_DIR;

typedef struct {
  int col;                    /* starting column */
  int row;                    /* starting row    */
  TETROMINO_BY_IDX index;     /* which tetromino */
  TETROMINO_BY_IDX nextIndex; /* next tetromino */
  TETROMINO_R_DIR rotation;   /* current rotation: 0, 1, 2, or 3 */
} CurrentPiece;

typedef struct {
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]; /* the play field */
  CurrentPiece current_piece;
  int score;
  int pieces_count; /* total pieces locked — used for difficulty scaling */
  bool game_over;

  // Replace speed/speed_count with time-based fields
  float drop_timer;    // Accumulates delta time (in seconds)
  float drop_interval; // Time between drops (e.g., 1.0 = 1 second)
} GameState;

/* ── Input System ─────────────────────────────────────── */

typedef struct {
  /**
   * Number of state changes this frame
   * 0 = No change (button held or released)
   * 1 = Changed once (normal press or release)
   * 2 = Changed twice (press then release, or release then press)
   */
  int half_transition_count;

  /** Final state (true = pressed, false = released) */
  int ended_down;
} GameButtonState;

typedef struct {
  /** Auto-repeat timer for this specific action */
  float repeat_timer;

  /** Auto-repeat interval (time between repeats when held) */
  float repeat_interval;
} GameActionRepeat;

/* Action with auto-repeat capability */
typedef struct {
  GameButtonState button;
  GameActionRepeat repeat;
} GameActionWithRepeat;

/* Action with a directional value */
typedef struct {
  GameButtonState button;
  /**
   * Direction/value for this action
   * For rotation: 0 = no rotation, 1 = clockwise, -1 = counter-clockwise
   * Could be used for other valued actions too
   */
  int value;
} GameActionWithValue;

/* All the inputs the game cares about.
   platform_get_input() fills this in each frame. */
typedef struct {
  GameActionWithRepeat move_left;
  GameActionWithRepeat move_right;
  GameActionWithRepeat move_down;
  GameActionWithValue rotate;
} GameInput;

/* ── Piece data ─────────────────────────────────────── */

/* 7 tetrominoes, each a 4×4 grid of '.' (empty) and 'X' (solid).
   Defined in tetris.c — 'extern' tells the compiler
   "trust me, it exists somewhere else." */
extern const char *TETROMINOES[7];

void game_init(GameState *state);

/* ── Function declarations ──────────────────────────── */

/* Returns the index into a 4×4 piece string for rotation r.
   px = column (0–3), py = row (0–3), r = rotation counter. */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);

int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y);
void tetris_update(GameState *state, GameInput *input, float delta_time);

#endif // TETRIS_H
```

## 2. Rewrite tetris.c

```c:games/tetris/src/tetris.c
#include "tetris.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const char TETROMINO_I[TETROMINO_SIZE] =
    "..X...X...X...X."; /* 0: I — vertical bar       */
const char TETROMINO_J[TETROMINO_SIZE] =
    "..X...X..XX....."; /* 5: J — J-shape (mirror L) */
const char TETROMINO_L[TETROMINO_SIZE] =
    ".X...X...XX....."; /* 4: L — L-shape            */
const char TETROMINO_O[TETROMINO_SIZE] =
    ".....XX..XX....."; /* 6: O — square (uses 3×3) */
const char TETROMINO_S[TETROMINO_SIZE] =
    ".X...XX...X....."; /* 1: S — S-shape            */
const char TETROMINO_T[TETROMINO_SIZE] =
    "..X..XX...X....."; /* 3: T — T-shape            */
const char TETROMINO_Z[TETROMINO_SIZE] =
    "..X..XX..X......"; /* 2: Z — Z-shape            */

const char *TETROMINOES[TETROMINOS_COUNT] = {
    TETROMINO_I, //
    TETROMINO_J, //
    TETROMINO_L, //
    TETROMINO_O, //
    TETROMINO_S, //
    TETROMINO_T, //
    TETROMINO_Z, //
};

void game_init(GameState *state) {
  state->score = 0;
  state->game_over = false;
  state->pieces_count = 0;

  // Time-based dropping
  state->drop_timer = 0.0f;
  state->drop_interval = 1.0f; // 1 second between drops initially

  /* Build the boundary walls. */
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      /* Left wall, right wall, or floor → value 9 */
      if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) {
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_WALL;
      } else {
        /* Everything else stays 0 (empty) */
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_EMPTY;
      }
    }
  }

  /* Pick a random starting piece. */
  srand((unsigned int)time(NULL));
  state->current_piece = (CurrentPiece){
      /*  center-ish, start in the middle of the field */
      .col = (FIELD_WIDTH * 0.5) - (TETROMINO_LAYER_COUNT * 0.5),
      .row = 0,
      .index = rand() % TETROMINOS_COUNT,
      .nextIndex = rand() % TETROMINOS_COUNT,
      .rotation = TETROMINO_R_0,
  };
}

int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r) {
  case TETROMINO_R_0:
    return py * TETROMINO_LAYER_COUNT + px;
  case TETROMINO_R_90:
    return 12 + py - (px * TETROMINO_LAYER_COUNT);
  case TETROMINO_R_180:
    return 15 - (py * TETROMINO_LAYER_COUNT) - px;
  case TETROMINO_R_270:
    return 3 - py + (px * TETROMINO_LAYER_COUNT);
  }
}

int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y) {

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      /* Which character in the piece string? */
      int pi = tetromino_pos_value(px, py, rotation);

      /* Skip empty cells in the piece */
      if (TETROMINOES[piece][pi] == '.')
        continue;

      /* Where would this cell land in the field? */
      int field_x = pos_x + px;
      int field_y = pos_y + py;

      /* Skip if outside the field bounds
         (piece's 4×4 box can hang outside on the sides) */
      if (field_x < 0 || field_x >= FIELD_WIDTH)
        continue;
      if (field_y < 0 || field_y >= FIELD_HEIGHT)
        continue;

      /* Field index for this position */
      int fi = field_y * FIELD_WIDTH + field_x;

      /* If the field cell is not empty → doesn't fit */
      if (state->field[fi] != 0)
        return 0;
    }
  }
  return 1; /* all solid cells checked — it fits */
}

static void handle_action_with_repeat(GameActionWithRepeat *action,
                                       float delta_time,
                                       int *should_trigger) {
  *should_trigger = 0;

  if (!action->button.ended_down) {
    // Button released - reset timer
    action->repeat.repeat_timer = 0.0f;
    return;
  }

  // Button is down
  if (action->button.half_transition_count > 0) {
    // Just pressed this frame - instant trigger
    *should_trigger = 1;
    action->repeat.repeat_timer = 0.0f;
  } else {
    // Held from previous frame - check auto-repeat
    action->repeat.repeat_timer += delta_time;

    if (action->repeat.repeat_timer >= action->repeat.repeat_interval) {
      action->repeat.repeat_timer -= action->repeat.repeat_interval;
      *should_trigger = 1;
    }
  }
}

void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {

  /* Rotation - only on JUST PRESSED (no auto-repeat) */
  if (input->rotate.button.ended_down &&
      input->rotate.button.half_transition_count > 0 &&
      input->rotate.value != 0) {
    TETROMINO_R_DIR new_rotation;

    if (input->rotate.value == 1) {
      // Clockwise
      switch (state->current_piece.rotation) {
      case TETROMINO_R_0:
        new_rotation = TETROMINO_R_90;
        break;
      case TETROMINO_R_90:
        new_rotation = TETROMINO_R_180;
        break;
      case TETROMINO_R_180:
        new_rotation = TETROMINO_R_270;
        break;
      case TETROMINO_R_270:
        new_rotation = TETROMINO_R_0;
        break;
      }
    } else {
      // Counter-clockwise
      switch (state->current_piece.rotation) {
      case TETROMINO_R_0:
        new_rotation = TETROMINO_R_270;
        break;
      case TETROMINO_R_90:
        new_rotation = TETROMINO_R_0;
        break;
      case TETROMINO_R_180:
        new_rotation = TETROMINO_R_90;
        break;
      case TETROMINO_R_270:
        new_rotation = TETROMINO_R_180;
        break;
      }
    }

    if (tetromino_does_piece_fit(state, state->current_piece.index, new_rotation,
                                  state->current_piece.col, state->current_piece.row)) {
      state->current_piece.rotation = new_rotation;
    }
  }

  /* Horizontal movement - each direction independent */
  int should_move_left = 0;
  int should_move_right = 0;

  handle_action_with_repeat(&input->move_left, delta_time, &should_move_left);
  handle_action_with_repeat(&input->move_right, delta_time, &should_move_right);

  if (should_move_left && tetromino_does_piece_fit(state, state->current_piece.index,
                                                    state->current_piece.rotation,
                                                    state->current_piece.col - 1,
                                                    state->current_piece.row)) {
    state->current_piece.col--;
  }

  if (should_move_right && tetromino_does_piece_fit(state, state->current_piece.index,
                                                     state->current_piece.rotation,
                                                     state->current_piece.col + 1,
                                                     state->current_piece.row)) {
    state->current_piece.col++;
  }

  /* Soft drop - independent timing */
  int should_move_down = 0;
  handle_action_with_repeat(&input->move_down, delta_time, &should_move_down);

  if (should_move_down && tetromino_does_piece_fit(state, state->current_piece.index,
                                                    state->current_piece.rotation,
                                                    state->current_piece.col,
                                                    state->current_piece.row + 1)) {
    state->current_piece.row++;
  }
}

void tetris_update(GameState *state, GameInput *input, float delta_time) {
  if (state->game_over)
    return;

  /* ── Handle player input (always responsive) ── */
  tetris_apply_input(state, input, delta_time);

  /* ── Accumulate time for gravity ── */
  state->drop_timer += delta_time;

  /* ── Check if it's time to drop the piece ── */
  if (state->drop_timer >= state->drop_interval) {
    // Reset timer, keeping remainder for precision
    state->drop_timer -= state->drop_interval;

    /* Try to move the piece down one row */
    if (tetromino_does_piece_fit(
            state, state->current_piece.index, state->current_piece.rotation,
            state->current_piece.col, state->current_piece.row + 1)) {
      state->current_piece.row++;
    } else {
      /* If it doesn't fit, piece has landed - lock it in */
      for (int px = 0; px < TETROMINO_LAYER_COUNT; ++px) {
        for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
          int pi = tetromino_pos_value(px, py, state->current_piece.rotation);
          if (TETROMINOES[state->current_piece.index][pi] != TETROMINO_SPAN) {
            int fx = state->current_piece.col + px;
            int fy = state->current_piece.row + py;
            if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT) {
              state->field[fy * FIELD_WIDTH + fx] =
                  state->current_piece.index +
                  1; // Store piece index + 1 (0 is empty) for rendering
            }
          }
        }
      }

      /* Score and difficulty */
      state->score += 25;
      state->pieces_count++;
      if (state->pieces_count % 50 == 0 && state->drop_interval > 0.2f) {
        state->drop_interval -= 0.05f;
      }

      // reset drop-timer and current piece for next round
      state->drop_timer = 0.0f;
      state->current_piece = (CurrentPiece){
          .col = (FIELD_WIDTH * 0.5) - (TETROMINO_LAYER_COUNT * 0.5),
          .row = 0,
          .index = state->current_piece.nextIndex,
          .nextIndex = rand() % TETROMINOS_COUNT,
          .rotation = TETROMINO_R_0,
      };

      if (!tetromino_does_piece_fit(
              state, state->current_piece.index, state->current_piece.rotation,
              state->current_piece.col, state->current_piece.row)) {
        state->game_over = true;
      }
    }
  }
}
```

## 3. Rewrite main_raylib.c

```c:games/tetris/src/main_raylib.c
#include "platform.h"
#include "tetris.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

static void draw_cell(int col, int row, Color color) {
  int x = col * CELL_SIZE + 1;
  int y = row * CELL_SIZE + 1;
  int w = CELL_SIZE - 1;
  int h = CELL_SIZE - 1;

  DrawRectangle(x, y, w, h, color);
}

static Color get_tetromino_color_by_index(TETROMINO_BY_IDX piece_index) {
  switch (piece_index) {
  case TETROMINO_I_IDX: {
    return (Color){0, 255, 255, 255}; // Cyan color for I pieces
  }
  case TETROMINO_J_IDX: {
    return (Color){0, 0, 255, 255}; // Blue color for J pieces
  }
  case TETROMINO_L_IDX: {
    return (Color){255, 165, 0, 255}; // Orange color for L pieces
  }
  case TETROMINO_O_IDX: {
    return (Color){255, 255, 0, 255}; // Yellow color for O pieces
  }
  case TETROMINO_S_IDX: {
    return (Color){0, 255, 0, 255}; // Green color for S pieces
  }
  case TETROMINO_T_IDX: {
    return (Color){128, 0, 128, 255}; // Purple color for T pieces
  }
  case TETROMINO_Z_IDX: {
    return (Color){255, 0, 0, 255}; // Red color for Z pieces
  }
  default: {
    // Impossible, but just in case, return gray for invalid piece index
    printf(
        "Warning: Invalid piece index %d in get_tetromino_color_by_index()\n",
        piece_index);
    return (Color){128, 128, 128, 255};
  }
  }
}

static void draw_piece(int piece_index, int field_col, int field_row,
                       Color color, TETROMINO_R_DIR rotation) {
  int px, py;

  for (py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      int pi = tetromino_pos_value(px, py, rotation);

      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
        draw_cell(field_col + px, field_row + py, color);
      }
    }
  }
}

int platform_init(const char *title, int width, int height) {
  InitWindow(FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE, "Tetris");
  SetTargetFPS(60);
  return 0;
}

void platform_get_input(GameState *state, GameInput *input) {
  // Reset transition counts (they're per-frame)
  input->move_left.button.half_transition_count = 0;
  input->move_right.button.half_transition_count = 0;
  input->move_down.button.half_transition_count = 0;
  input->rotate.button.half_transition_count = 0;
  input->rotate.value = 0;

  if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
    // Signal window should close - handled by WindowShouldClose()
  }

  // Helper macro to update button state
  #define UPDATE_BUTTON(button, is_down) \
    do { \
      if ((button).ended_down != (is_down)) { \
        (button).half_transition_count++; \
        (button).ended_down = (is_down); \
      } \
    } while(0)

  // Rotation - check which key and set value
  if (IsKeyPressed(KEY_X)) {
    UPDATE_BUTTON(input->rotate.button, 1);
    input->rotate.value = 1;  // Clockwise
  } else if (IsKeyPressed(KEY_Z)) {
    UPDATE_BUTTON(input->rotate.button, 1);
    input->rotate.value = -1; // Counter-clockwise
  }

  // Movement (can be held)
  UPDATE_BUTTON(input->move_left.button, IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
  UPDATE_BUTTON(input->move_right.button, IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT));
  UPDATE_BUTTON(input->move_down.button, IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));

  #undef UPDATE_BUTTON
}

void platform_render(GameState *state) {
  BeginDrawing();
  ClearBackground(BLACK);

  for (int row = 0; row < FIELD_HEIGHT; row++) {
    for (int col = 0; col < FIELD_WIDTH; col++) {
      unsigned char cell_value = state->field[row * FIELD_WIDTH + col];

      switch (cell_value) {
      case TETRIS_FIELD_EMPTY: {
        continue; // Skip empty cells
      }

      case TETRIS_FIELD_I:
      case TETRIS_FIELD_J:
      case TETRIS_FIELD_L:
      case TETRIS_FIELD_O:
      case TETRIS_FIELD_S:
      case TETRIS_FIELD_T:
      case TETRIS_FIELD_Z: {
        draw_cell(
            col, row,
            get_tetromino_color_by_index(
                cell_value - 1)); // Draw pieces based on their index (subtract
                                  // 1 because field values are piece index + 1)
        break;
      }

      case TETRIS_FIELD_WALL: {
        draw_cell(col, row, (Color){128, 128, 128, 255}); // Draw walls in gray
        break;
      }
      }
    }
  }

  draw_piece(state->current_piece.index, state->current_piece.col,
             state->current_piece.row,
             get_tetromino_color_by_index(state->current_piece.index),
             state->current_piece.rotation);

  int font_size = 16;
  int sx = FIELD_WIDTH * CELL_SIZE - font_size * 6;
  int base_pos_y = 95;
  char buf[32];
  DrawText("PIECES", sx, base_pos_y, font_size, WHITE);
  snprintf(buf, sizeof(buf), "%d", state->pieces_count);
  DrawText(buf, sx, base_pos_y + font_size * 1.25, font_size,
           (Color){255, 255, 0, 255});

  /* Game over overlay */
  if (state->game_over) {
    int cx = FIELD_WIDTH * CELL_SIZE / 2;
    int cy = FIELD_HEIGHT * CELL_SIZE / 2;
    DrawRectangle(cx - 70, cy - 36, 140, 72, (Color){0, 0, 0, 200});
    DrawText("GAME OVER", cx - 52, cy - 22, 24, RED);
    DrawText("R = Restart", cx - 46, cy + 4, 14, WHITE);
    DrawText("Q/Esc = Quit", cx - 46, cy + 22, 14, WHITE);
  }

  EndDrawing();
}

void platform_shutdown(void) { CloseWindow(); }

int main(void) {
  int screen_width = FIELD_WIDTH * CELL_SIZE;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;
  if (platform_init("Tetris", screen_width, screen_height) != 0) {
    return 1;
  }

  GameInput input = {0};

  // Set repeat intervals for movement actions
  input.move_left.repeat.repeat_interval = 0.1f;   // 100ms
  input.move_right.repeat.repeat_interval = 0.1f;  // 100ms
  input.move_down.repeat.repeat_interval = 0.05f;  // 50ms (faster for down)
  // Rotation doesn't need repeat interval (no auto-repeat)

  GameState game_state = {0};
  game_init(&game_state);

  while (!WindowShouldClose()) {
    float delta_time = GetFrameTime();

    platform_get_input(&game_state, &input);
    tetris_update(&game_state, &input, delta_time);

    platform_render(&game_state);
  }

  platform_shutdown();
  return 0;
}
```

## 4. Rewrite main_x11.c

```c:games/tetris/src/main_x11.c
#define _XOPEN_SOURCE 700 /* for nanosleep() */
#define _POSIX_C_SOURCE 200809L

#include "platform.h"
#include "tetris.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

typedef struct {
  Display *display;
  Window window;
  GC gc;
  XEvent event;
  int screen;
  int width;
  int height;
  bool is_running;
  struct {
    unsigned long black;
    unsigned long white;
    unsigned long gray;

    unsigned long cyan;
    unsigned long blue;
    unsigned long orange;
    unsigned long yellow;
    unsigned long green;
    unsigned long magenta;
    unsigned long red;
  } alloc_colors;
} X11_State;

bool g_is_game_running = true;
X11_State g_x11 = {0};
double g_last_frame_time = 0.0;
int g_fps = 60;
double g_target_frame_time = 1.0 / 60.0;

static double g_start_time = 0.0;

static double get_current_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

void platform_sleep_ms(int ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

double platform_get_time(void) {
  if (g_start_time == 0.0) {
    g_start_time = get_current_time();
  }
  return get_current_time() - g_start_time;
}

static unsigned long alloc_color(const char *name) {
  XColor color, exact;
  Colormap cmap = DefaultColormap(g_x11.display, g_x11.screen);
  XAllocNamedColor(g_x11.display, cmap, name, &color, &exact);
  return color.pixel;
}

static void draw_cell(int col, int row, unsigned long color) {
  int x = col * CELL_SIZE + 1;
  int y = row * CELL_SIZE + 1;
  int w = CELL_SIZE - 1;
  int h = CELL_SIZE - 1;

  XSetForeground(g_x11.display, g_x11.gc, color);
  XFillRectangle(g_x11.display, g_x11.window, g_x11.gc, x, y, w, h);
}

unsigned long get_tetromino_color_by_index(TETROMINO_BY_IDX piece_index) {
  switch (piece_index) {
  case TETROMINO_I_IDX: {
    return g_x11.alloc_colors.cyan;
  }
  case TETROMINO_J_IDX: {
    return g_x11.alloc_colors.blue;
  }
  case TETROMINO_L_IDX: {
    return g_x11.alloc_colors.orange;
  }
  case TETROMINO_O_IDX: {
    return g_x11.alloc_colors.yellow;
  }
  case TETROMINO_S_IDX: {
    return g_x11.alloc_colors.green;
  }
  case TETROMINO_T_IDX: {
    return g_x11.alloc_colors.magenta;
  }
  case TETROMINO_Z_IDX: {
    return g_x11.alloc_colors.red;
  }
  default: {
    printf(
        "Warning: Invalid piece index %d in get_tetromino_color_by_index()\n",
        piece_index);
    return g_x11.alloc_colors.gray;
  }
  }
}

static void draw_piece(int piece_index, int field_col, int field_row,
                       unsigned long color, TETROMINO_R_DIR rotation) {
  int px, py;

  for (py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      int pi = tetromino_pos_value(px, py, rotation);

      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
        draw_cell(field_col + px, field_row + py, color);
      }
    }
  }
}

int platform_init(const char *title, int width, int height) {
  g_last_frame_time = platform_get_time();
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "Failed to open display\n");
    return 1;
  }

  g_x11.screen = DefaultScreen(g_x11.display);

  g_x11.alloc_colors.black = BlackPixel(g_x11.display, g_x11.screen);
  g_x11.alloc_colors.white = WhitePixel(g_x11.display, g_x11.screen);
  g_x11.alloc_colors.gray = alloc_color("gray50");
  g_x11.alloc_colors.cyan = alloc_color("cyan");
  g_x11.alloc_colors.magenta = alloc_color("magenta");
  g_x11.alloc_colors.yellow = alloc_color("yellow");
  g_x11.alloc_colors.green = alloc_color("green");
  g_x11.alloc_colors.red = alloc_color("red");
  g_x11.alloc_colors.blue = alloc_color("blue");
  g_x11.alloc_colors.orange = alloc_color("orange");

  g_x11.window = XCreateSimpleWindow(
      g_x11.display,
      RootWindow(g_x11.display, g_x11.screen),
      100, 100,
      width, height,
      1,
      g_x11.alloc_colors.black,
      g_x11.alloc_colors.black
  );

  g_x11.gc = XCreateGC(g_x11.display, g_x11.window, 0, NULL);

  XStoreName(g_x11.display, g_x11.window, title);
  XSelectInput(g_x11.display, g_x11.window,
               ExposureMask | KeyPressMask | KeyReleaseMask);
  XMapWindow(g_x11.display, g_x11.window);
  XFlush(g_x11.display);

  return 0;
}

void platform_get_input(GameState *state, GameInput *input) {
  // Reset transition counts (they're per-frame)
  input->move_left.button.half_transition_count = 0;
  input->move_right.button.half_transition_count = 0;
  input->move_down.button.half_transition_count = 0;
  input->rotate.button.half_transition_count = 0;
  input->rotate.value = 0;

  // Helper macro to update button state
  #define UPDATE_BUTTON(button, is_down) \
    do { \
      if ((button).ended_down != (is_down)) { \
        (button).half_transition_count++; \
        (button).ended_down = (is_down); \
      } \
    } while(0)

  while (XPending(g_x11.display) > 0) {
    XNextEvent(g_x11.display, &g_x11.event);
    switch (g_x11.event.type) {
    case KeyPress: {
      KeySym key = XLookupKeysym(&g_x11.event.xkey, 0);
      switch (key) {
      case XK_q:
      case XK_Q:
      case XK_Escape: {
        g_is_game_running = false;
        break;
      }
      case XK_x:
      case XK_X: {
        UPDATE_BUTTON(input->rotate.button, 1);
        input->rotate.value = 1;  // Clockwise
        break;
      }
      case XK_z:
      case XK_Z: {
        UPDATE_BUTTON(input->rotate.button, 1);
        input->rotate.value = -1; // Counter-clockwise
        break;
      }
      case XK_Left:
      case XK_A:
      case XK_a: {
        UPDATE_BUTTON(input->move_left.button, 1);
        break;
      }
      case XK_Right:
      case XK_D:
      case XK_d: {
        UPDATE_BUTTON(input->move_right.button, 1);
        break;
      }
      case XK_Down:
      case XK_S:
      case XK_s: {
        UPDATE_BUTTON(input->move_down.button, 1);
        break;
      }
      }
      break;
    }

    case KeyRelease: {
      KeySym key = XLookupKeysym(&g_x11.event.xkey, 0);
      switch (key) {
      case XK_x:
      case XK_X:
      case XK_z:
      case XK_Z: {
        UPDATE_BUTTON(input->rotate.button, 0);
        input->rotate.value = 0;
        break;
      }
      case XK_Left:
      case XK_A:
      case XK_a: {
        UPDATE_BUTTON(input->move_left.button, 0);
        break;
      }
      case XK_Right:
      case XK_D:
      case XK_d: {
        UPDATE_BUTTON(input->move_right.button, 0);
        break;
      }
      case XK_Down:
      case XK_S:
      case XK_s: {
        UPDATE_BUTTON(input->move_down.button, 0);
        break;
      }
      }
      break;
    }

    case ClientMessage: {
      g_is_game_running = false;
      break;
    }
    }
  }

  #undef UPDATE_BUTTON
}

void platform_render(GameState *state) {
  XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.black);
  XFillRectangle(g_x11.display, g_x11.window, g_x11.gc, 0, 0,
                 FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE);

  for (int row = 0; row < FIELD_HEIGHT; row++) {
    for (int col = 0; col < FIELD_WIDTH; col++) {
      unsigned char cell_value = state->field[row * FIELD_WIDTH + col];

      switch (cell_value) {
      case TETRIS_FIELD_EMPTY: {
        continue;
      }

      case TETRIS_FIELD_I:
      case TETRIS_FIELD_J:
      case TETRIS_FIELD_L:
      case TETRIS_FIELD_O:
      case TETRIS_FIELD_S:
      case TETRIS_FIELD_T:
      case TETRIS_FIELD_Z: {
        draw_cell(col, row, get_tetromino_color_by_index(cell_value - 1));
        break;
      }

      case TETRIS_FIELD_WALL: {
        draw_cell(col, row, g_x11.alloc_colors.gray);
        break;
      }
      }
    }
  }

  draw_piece(state->current_piece.index, state->current_piece.col,
             state->current_piece.row,
             get_tetromino_color_by_index(state->current_piece.index),
             state->current_piece.rotation);

  int font_size = 16;
  int sx = FIELD_WIDTH * CELL_SIZE - font_size * 6;
  int base_pos_y = 95;
  char buf[32];
  XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.white);
  XDrawString(g_x11.display, g_x11.window, g_x11.gc, sx, base_pos_y, "PIECES", 6);
  XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.yellow);
  snprintf(buf, sizeof(buf), "%d", state->pieces_count);
  XDrawString(g_x11.display, g_x11.window, g_x11.gc, sx, base_pos_y + font_size,
              buf, (int)strlen(buf));

  if (state->game_over) {
    int cx = FIELD_WIDTH * CELL_SIZE / 2;
    int cy = FIELD_HEIGHT * CELL_SIZE / 2;
    XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.black);
    XFillRectangle(g_x11.display, g_x11.window, g_x11.gc, cx - 60, cy - 30, 120, 60);
    XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.red);
    XDrawString(g_x11.display, g_x11.window, g_x11.gc, cx - 28, cy - 8,
                "GAME OVER", 9);
    XSetForeground(g_x11.display, g_x11.gc, g_x11.alloc_colors.white);
    XDrawString(g_x11.display, g_x11.window, g_x11.gc, cx - 42, cy + 12,
                "R=Restart  Q=Quit", 17);
  }

  XFlush(g_x11.display);
}

void platform_shutdown(void) {
  XFreeGC(g_x11.display, g_x11.gc);
  XDestroyWindow(g_x11.display, g_x11.window);
  XCloseDisplay(g_x11.display);
}

int main(void) {
  int screen_width = FIELD_WIDTH * CELL_SIZE;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;

  if (platform_init("Tetris", screen_width, screen_height) != 0) {
    return 1;
  }

  GameInput input = {0};

  // Set repeat intervals for movement actions
  input.move_left.repeat.repeat_interval = 0.1f;   // 100ms
  input.move_right.repeat.repeat_interval = 0.1f;  // 100ms
  input.move_down.repeat.repeat_interval = 0.05f;  // 50ms (faster for down)
  // Rotation doesn't need repeat interval (no auto-repeat)

  GameState game_state = {0};
  game_init(&game_state);

  while (g_is_game_running) {
    double current_time = platform_get_time();
    double delta_time = current_time - g_last_frame_time;
    g_last_frame_time = current_time;

    platform_get_input(&game_state, &input);
    tetris_update(&game_state, &input, (float)delta_time);
    platform_render(&game_state);

    double frame_time = platform_get_time() - current_time;
    if (frame_time < g_target_frame_time) {
      platform_sleep_ms((int)((g_target_frame_time - frame_time) * 1000.0));
    }
  }

  platform_shutdown();
  return 0;
}
```

That's all four files completely rewritten! The new system features:

✅ **Separated concerns**: Button state vs. repeat behavior vs. value/direction  
✅ **Independent timers**: Each action has its own auto-repeat timer  
✅ **Flexible actions**: `GameActionWithRepeat` for movement, `GameActionWithValue` for rotation  
✅ **No interference**: Holding left+down works perfectly, each repeats independently  
✅ **Clean architecture**: Easy to extend with new action types

---

# Summary of Targeted Changes (Features)

## What We Changed

### 1. **Frame-Independent Input System**

- **Before**: Simple boolean flags (pressed/not pressed)
- **After**: Rich button state tracking with transition counts
- **Why**: Detect single presses vs. holds, enable frame-perfect inputs even at varying frame rates

### 2. **Independent Auto-Repeat Per Action**

- **Before**: All movement shared one global speed counter
- **After**: Each action (left, right, down) has its own repeat timer and interval
- **Why**:
  - Down can repeat faster than horizontal movement (feels more responsive)
  - Holding left+down works correctly - both repeat independently
  - Easy to tune each action separately for game feel

### 3. **Time-Based Instead of Frame-Based**

- **Before**: "Every N frames, do X" (breaks at different frame rates)
- **After**: "Every N seconds, do X" using delta_time
- **Why**:
  - Game plays identically at 30fps, 60fps, or 144fps
  - Smooth difficulty scaling (drop interval decreases gradually)
  - Precision - no lost time between frames

### 4. **Separated Button State from Action Behavior**

- **Before**: Input struct mixed raw input with game logic
- **After**:
  - `GameButtonState` = raw hardware state (pressed/released/transitions)
  - `GameActionRepeat` = behavior config (repeat timing)
  - `GameActionWithRepeat` / `GameActionWithValue` = combined actions
- **Why**:
  - Same button can be used for different actions with different behaviors
  - Rotation doesn't auto-repeat (single press only)
  - Movement auto-repeats with custom intervals
  - Easy to add new action types (charge attacks, combos, etc.)

### 5. **Value-Based Actions**

- **Before**: Rotation was just a flag
- **After**: Rotation has direction value (1 = clockwise, -1 = counter-clockwise)
- **Why**:
  - One action struct handles both rotation directions
  - Extensible to other valued inputs (analog sticks, pressure-sensitive buttons)

---

# How Other Games Handle These Issues

## Fighting Games (Street Fighter, Tekken, Guilty Gear)

### Input Buffering

```
Problem: 60fps game, human can't press frame-perfect
Solution:
- Store last 10-15 frames of input history
- Check for move patterns across buffer
- "Quarter-circle forward + punch" works even if timing is off by 3-4 frames
```

### Priority System

```
Problem: Multiple buttons pressed simultaneously
Solution:
- Heavy attack > Medium attack > Light attack
- Special moves > Normal moves
- Last 3 frames of input have higher priority than older inputs
```

### Negative Edge

```
Feature: Button RELEASE can trigger moves too
Example: Charge flash kick on button UP, not just DOWN
Implementation: Track both press AND release transitions
```

## Platformers (Celeste, Hollow Knight, Super Meat Boy)

### Coyote Time

```
Problem: Player runs off ledge, presses jump "too late"
Solution:
- Allow jump for 0.1-0.15 seconds AFTER leaving ground
- Feels more responsive, reduces frustration
```

### Jump Buffering

```
Problem: Player presses jump before landing
Solution:
- Store jump input for 0.1 seconds
- If player lands within that window, auto-jump
- Makes fast-paced platforming feel smooth
```

### Variable Jump Height

```
Implementation:
- Short tap = small jump
- Hold button = high jump
- Track button hold duration, apply different gravity
```

## Shooters (DOOM, Quake, CS:GO)

### Input Sampling

```
Problem: Mouse moves between frames
Solution:
- Sample mouse multiple times per frame
- Average or use most recent
- Reduces input lag perception
```

### Prediction & Rollback

```
For online play:
1. Apply input immediately (optimistic)
2. Server confirms or corrects
3. If wrong, "rollback" and replay with correct input
4. Player never notices if ping < 50ms
```

## Racing Games (Gran Turismo, Forza)

### Analog Input Smoothing

```
Problem: Steering wheel jitter, sudden inputs
Solution:
- Low-pass filter on analog inputs
- Smooth over 2-3 frames
- Prevents car from twitching
```

### Dead Zones

```
Problem: Analog stick drift, slight movements
Solution:
- Ignore inputs < 10% of max range
- Scale remaining 90% to full range
- Configurable per-player
```

## RPGs (Dark Souls, Monster Hunter)

### Commitment-Based Input

```
Design: Once action starts, can't cancel
Implementation:
- Ignore new inputs during animation
- Track "action state" (idle, attacking, rolling)
- Only accept inputs in "idle" state
```

### Input Queuing

```
Problem: Player presses next attack during current attack
Solution:
- Store ONE next input
- Execute it when current action finishes
- Allows smooth combos without button mashing
```

## Rhythm Games (Guitar Hero, Beat Saber)

### Hit Windows

```
Timing grades:
- Perfect: ±16ms (1 frame at 60fps)
- Great: ±33ms (2 frames)
- Good: ±50ms (3 frames)
- Miss: >50ms

Implementation:
- Store note timestamps
- Compare input time to note time
- Grade based on delta
```

### Audio Latency Compensation

```
Problem: Display lag, audio processing delay
Solution:
- Calibration mode: measure system latency
- Offset all hit windows by measured delay
- Per-system configuration
```

## Strategy Games (StarCraft, Age of Empires)

### Command Queuing

```
Feature: Shift+Click to queue actions
Implementation:
- Store list of commands per unit
- Execute one per frame/interval
- Visual feedback (waypoint lines)
```

### Smart Casting

```
Problem: Click ability, click target = slow
Solution:
- Hold key, cursor shows range
- Release key = cast at cursor
- No confirmation click needed
```

---

# Common Patterns Across All Games

## 1. **Separate Input Reading from Input Processing**

```
Frame loop:
1. Read raw hardware state
2. Store in input buffer
3. Game logic processes buffer
4. Clear/update buffer for next frame
```

## 2. **Delta Time for Everything**

```
Movement: position += velocity * delta_time
Timers: timer += delta_time
Cooldowns: cooldown -= delta_time
```

## 3. **Input Buffering (Short-Term Memory)**

```
Store last N frames of input
Check patterns across buffer
Allows "sloppy" timing to feel tight
```

## 4. **Configurable Timing Windows**

```
Easy mode: Larger windows, more buffering
Hard mode: Smaller windows, less forgiveness
Pro mode: Frame-perfect required
```

## 5. **Visual/Audio Feedback**

```
Button press → immediate visual response
Even if game action is delayed
Makes input feel responsive
```

## 6. **Prediction + Correction**

```
For networked games:
- Show optimistic result immediately
- Correct if server disagrees
- Hide correction with animation blending
```

---

# What Makes Input Feel "Good"

1. **Responsiveness**: Action starts within 1-2 frames of input
2. **Consistency**: Same input = same result every time
3. **Forgiveness**: Small timing errors don't punish player
4. **Feedback**: Clear visual/audio confirmation
5. **Control**: Player feels in command, not fighting the game

Your Tetris implementation now has **#1, #2, and #4** nailed down! 🎮

---

Great question! Let me explain what's **missing** and what we could **add**:

---

## Why We DON'T Have Full Forgiveness Yet

### 1. **No Rotation Buffering**

```
Current behavior:
- Press X while piece is mid-air → rotates immediately ✓
- Press X during lock-in animation → IGNORED ✗

What forgiveness would add:
- Press X slightly before piece spawns → queued, executes on spawn
- Press X during lock-in → queued, applies to NEXT piece
```

**Example from other games:**

- Tetris Effect: Buffers rotation input for ~100ms
- If you press rotate while piece is locking, it applies to the next piece
- Feels smoother, less "dropped inputs"

### 2. **No Move Buffering**

```
Current behavior:
- Press Left while piece is locking → IGNORED
- Must wait for new piece, then press Left

What forgiveness would add:
- Press Left during lock → new piece spawns already moving left
- "I meant to move that piece immediately!"
```

### 3. **No "Coyote Time" for Rotation**

```
Current behavior:
- Piece hits wall → rotation fails → no feedback

What forgiveness would add:
- Try to rotate near wall → auto-shift piece 1 cell if it would fit
- Called "wall kicks" in Tetris
- Makes tight spaces more forgiving
```

### 4. **Hard Drop Not Implemented**

```
Missing feature:
- Press Space → piece instantly drops to bottom
- Common in modern Tetris for speed

Without it:
- Must hold Down and wait
- Less control over precise timing
```

---

## Why We DON'T Have Full Control Yet

### 1. **No Lock Delay**

```
Current behavior:
- Piece touches ground → IMMEDIATELY locks
- No time to adjust position

What control would add:
- Piece touches ground → 0.5 second grace period
- Can still move/rotate during grace period
- Resets if piece moves down
- Called "Infinity" in Tetris guidelines
```

**Why this matters:**

```
Without lock delay:
┌─────────┐
│    ▓▓   │  Piece lands here
│    ▓▓   │  ← Locked instantly!
│  ▓▓▓▓   │  Can't slide it left anymore
└─────────┘

With lock delay:
┌─────────┐
│    ▓▓   │  Piece lands
│    ▓▓   │  ← 0.5s to adjust!
│  ▓▓▓▓   │  Can slide left/right/rotate
└─────────┘  Locks after 0.5s OR when you stop moving
```

### 2. **No Hold Piece Feature**

```
Missing feature:
- Press C → swap current piece with "held" piece
- Store one piece for later
- Strategic depth

Without it:
- Stuck with whatever piece spawns
- Less player agency
```

### 3. **No Ghost Piece (Shadow)**

```
Missing visual aid:
- Show where piece WILL land
- Faint outline at bottom
- Helps planning

Without it:
- Must mentally calculate landing spot
- More guesswork, less precision
```

### 4. **No Fine-Tuned DAS (Delayed Auto-Shift)**

```
Current implementation:
- Auto-repeat starts immediately
- Fixed interval (0.1s for horizontal)

What control would add:
- DAS: Delay before auto-repeat starts (e.g., 0.15s)
- ARR: Auto-repeat rate once started (e.g., 0.03s)
- Prevents accidental over-movement
```

**Example:**

```
Current:
Press Left → move | wait 0.1s | move | wait 0.1s | move...
         ↑ immediate    ↑ same speed throughout

Better DAS/ARR:
Press Left → move | wait 0.15s (DAS) | move | 0.03s | move | 0.03s...
         ↑ immediate    ↑ delay      ↑ fast repeat

Gives player time to release for single tap vs. hold for rapid movement
```

---

## What We COULD Add for Full Forgiveness + Control

### Code Addition 1: **Input Buffering**

```c:games/tetris/src/tetris.h
#define INPUT_BUFFER_SIZE 5  // Store last 5 frames of input

typedef struct {
  GameInput inputs[INPUT_BUFFER_SIZE];
  int write_index;
  float buffer_window;  // How long to keep inputs (e.g., 0.1s)
} InputBuffer;

typedef struct {
  // ... existing fields ...

  InputBuffer input_buffer;
  float lock_delay_timer;     // Grace period when piece touches ground
  float lock_delay_max;       // Max grace period (e.g., 0.5s)
  bool piece_on_ground;       // Is current piece touching ground?
} GameState;
```

```c:games/tetris/src/tetris.c
void buffer_input(InputBuffer *buffer, GameInput *input) {
  buffer->inputs[buffer->write_index] = *input;
  buffer->write_index = (buffer->write_index + 1) % INPUT_BUFFER_SIZE;
}

GameInput* get_buffered_input(InputBuffer *buffer, float max_age) {
  // Return most recent input within max_age window
  // Allows "I pressed rotate 0.05s ago, still valid!"
}
```

### Code Addition 2: **Lock Delay (Infinity)**

```c:games/tetris/src/tetris.c
void tetris_update(GameState *state, GameInput *input, float delta_time) {
  // ... existing input handling ...

  // Check if piece is on ground
  bool on_ground = !tetromino_does_piece_fit(
      state, state->current_piece.index, state->current_piece.rotation,
      state->current_piece.col, state->current_piece.row + 1);

  if (on_ground) {
    if (!state->piece_on_ground) {
      // Just landed - start lock delay timer
      state->piece_on_ground = true;
      state->lock_delay_timer = 0.0f;
    }

    state->lock_delay_timer += delta_time;

    // If player moved/rotated, reset timer (up to a limit)
    if (input->move_left.button.half_transition_count > 0 ||
        input->move_right.button.half_transition_count > 0 ||
        input->rotate.button.half_transition_count > 0) {
      state->lock_delay_timer = 0.0f;  // Reset grace period
    }

    // Lock piece after grace period expires
    if (state->lock_delay_timer >= state->lock_delay_max) {
      lock_piece(state);  // Your existing lock logic
    }
  } else {
    state->piece_on_ground = false;
    state->lock_delay_timer = 0.0f;
  }
}
```

### Code Addition 3: **DAS/ARR Tuning**

```c:games/tetris/src/tetris.h
typedef struct {
  GameButtonState button;
  GameActionRepeat repeat;

  float das_delay;      // Delay before auto-repeat starts
  float arr_interval;   // Interval during auto-repeat
  bool das_active;      // Has DAS delay passed?
} GameActionWithDAS;

typedef struct {
  GameActionWithDAS move_left;
  GameActionWithDAS move_right;
  GameActionWithDAS move_down;
  GameActionWithValue rotate;
} GameInput;
```

```c:games/tetris/src/tetris.c
static void handle_action_with_das(GameActionWithDAS *action,
                                    float delta_time,
                                    int *should_trigger) {
  *should_trigger = 0;

  if (!action->button.ended_down) {
    action->repeat.repeat_timer = 0.0f;
    action->das_active = false;
    return;
  }

  // Just pressed - immediate trigger
  if (action->button.half_transition_count > 0) {
    *should_trigger = 1;
    action->repeat.repeat_timer = 0.0f;
    action->das_active = false;
    return;
  }

  // Held - check DAS delay first
  action->repeat.repeat_timer += delta_time;

  if (!action->das_active) {
    // Still in DAS delay period
    if (action->repeat.repeat_timer >= action->das_delay) {
      action->das_active = true;
      action->repeat.repeat_timer = 0.0f;
      *should_trigger = 1;  // First auto-repeat
    }
  } else {
    // DAS passed, use ARR for repeats
    if (action->repeat.repeat_timer >= action->arr_interval) {
      action->repeat.repeat_timer -= action->arr_interval;
      *should_trigger = 1;
    }
  }
}
```

### Code Addition 4: **Hold Piece**

```c:games/tetris/src/tetris.h
typedef struct {
  // ... existing fields ...

  TETROMINO_BY_IDX held_piece_index;
  bool has_held_piece;
  bool can_hold;  // Prevent holding multiple times per piece
} GameState;

typedef struct {
  // ... existing actions ...
  GameActionWithValue hold;  // C key
} GameInput;
```

```c:games/tetris/src/tetris.c
void handle_hold_piece(GameState *state) {
  if (!state->can_hold) return;

  if (!state->has_held_piece) {
    // First hold - store current, spawn next
    state->held_piece_index = state->current_piece.index;
    state->has_held_piece = true;
    spawn_next_piece(state);
  } else {
    // Swap current with held
    TETROMINO_BY_IDX temp = state->current_piece.index;
    state->current_piece.index = state->held_piece_index;
    state->held_piece_index = temp;

    // Reset position
    state->current_piece.col = (FIELD_WIDTH * 0.5) - (TETROMINO_LAYER_COUNT * 0.5);
    state->current_piece.row = 0;
    state->current_piece.rotation = TETROMINO_R_0;
  }

  state->can_hold = false;  // Can't hold again until next piece locks
}
```

---

## Summary: What's Missing

| Feature          | Current            | Forgiveness Impact        | Control Impact               |
| ---------------- | ------------------ | ------------------------- | ---------------------------- |
| **Lock Delay**   | ❌ Instant lock    | ❌ No time to adjust      | ❌ Can't fine-tune placement |
| **Input Buffer** | ❌ No buffering    | ❌ Dropped inputs         | ❌ Must time perfectly       |
| **Wall Kicks**   | ❌ No kicks        | ❌ Rotation fails harshly | ❌ Less positioning options  |
| **Hold Piece**   | ❌ Not implemented | -                         | ❌ No strategic storage      |
| **Ghost Piece**  | ❌ No preview      | -                         | ❌ Hard to judge landing     |
| **DAS/ARR**      | ⚠️ Basic repeat    | ⚠️ Can over-move          | ⚠️ Less precise control      |
| **Hard Drop**    | ❌ Not implemented | -                         | ❌ Slow piece placement      |

**Your current system has the FOUNDATION for all of these!** The input system is solid - you just need to add these game-specific features on top.

Would you like me to implement any of these? Lock delay + DAS/ARR would give you the biggest "feel" improvement! 🎮

---

Great question! Let's compare the two approaches:

---

## Casey's Handmade Hero Approach (Double Buffering)

```c
// Engine has TWO input buffers
GameInput inputs[2];
GameInput *old_inputs;  // Last frame
GameInput *new_inputs;  // This frame (game sees this)

// Frame loop:
while (running) {
    // 1. PREPARE: Copy old state to new, reset transitions
    prepare_input_frame(old_inputs, new_inputs);

    // 2. POLL: Update new_inputs with this frame's events
    platform_get_input(new_inputs);

    // 3. UPDATE: Game uses new_inputs
    game_update(new_inputs);

    // 4. SWAP: Flip pointers for next frame
    engine_swap_inputs(&engine);
    // Now old_inputs points to what was new_inputs
    // And new_inputs points to fresh buffer
}
```

**Why two buffers?**

```
Frame N:
  old_inputs = { move_left: ended_down=1, transitions=0 }  ← was pressed last frame
  new_inputs = { move_left: ended_down=1, transitions=0 }  ← still pressed

Frame N+1 (after swap):
  old_inputs = { move_left: ended_down=1, transitions=0 }  ← Frame N's state
  new_inputs = { move_left: ended_down=0, transitions=1 }  ← released this frame!

Game can compare: "Was it down last frame AND up this frame? → Just released!"
```

---

## Our Current Tetris Approach (Single Buffer)

```c
// Single input buffer
GameInput input = {0};

// Frame loop:
while (running) {
    // 1. RESET transitions only (not ended_down)
    input.move_left.button.half_transition_count = 0;
    // ... etc

    // 2. POLL: Update input with this frame's events
    platform_get_input(&input);

    // 3. UPDATE: Game uses input
    tetris_update(&state, &input, delta_time);

    // No swap needed — single buffer
}
```

**Why this works for Tetris:**

We only need to know:

- Is button down RIGHT NOW? → `ended_down`
- Did it CHANGE this frame? → `half_transition_count > 0`

We don't need to compare "last frame vs this frame" because `half_transition_count` already tells us if a transition happened.

---

## Side-by-Side Comparison

| Aspect                       | Casey (Double Buffer)   | Tetris (Single Buffer)           |
| ---------------------------- | ----------------------- | -------------------------------- |
| **Buffers**                  | 2 (`old` + `new`)       | 1                                |
| **Memory**                   | 2× input struct size    | 1× input struct size             |
| **Swap needed?**             | Yes, every frame        | No                               |
| **Can compare frames?**      | Yes (`old` vs `new`)    | No (only current state)          |
| **Detects "just pressed"?**  | `new.down && !old.down` | `ended_down && transitions > 0`  |
| **Detects "just released"?** | `!new.down && old.down` | `!ended_down && transitions > 0` |
| **Complexity**               | Higher                  | Lower                            |

---

## When Would We Need Double Buffering?

### Case 1: Comparing Previous Frame State

```c
// "Was the player holding jump LAST frame but not THIS frame?"
// Useful for: variable jump height (release = stop rising)

// Casey's way:
if (old_input->jump.ended_down && !new_input->jump.ended_down) {
    // Just released jump — cut upward velocity
}

// Our way (would need to track manually):
if (!input->jump.button.ended_down &&
    input->jump.button.half_transition_count > 0) {
    // Just released — same result!
}
```

For this case, **both approaches work**.

### Case 2: Input Recording/Playback

```c
// Casey's engine records inputs for replay:
record_input(new_input);  // Save this frame's input
// Later:
playback_input(new_input);  // Overwrite with recorded input

// Need old_input preserved to know what CHANGED
// If we only had one buffer, playback would lose transition info
```

For recording/playback, **double buffering is cleaner**.

### Case 3: Rollback Netcode (Fighting Games)

```c
// Need to store N frames of input history
// Roll back game state, replay with corrected inputs

InputHistory history[MAX_ROLLBACK_FRAMES];
// Each frame stores complete input state
// Double buffering makes this easier to manage
```

For netcode, **double buffering (or N-buffering) is essential**.

---

## Should We Add Double Buffering to Tetris?

**For current Tetris: No, not needed.**

Our single-buffer approach handles everything we need:

- Just pressed detection ✓
- Auto-repeat timing ✓
- Independent action timers ✓

**If we added these features, we'd want double buffering:**

- Input recording/playback (for replays)
- "Hold to charge, release to fire" mechanics
- Complex input sequences (fighting game combos)
- Netcode with rollback

---

## How We'd Add It (If Needed)

### Step 1: Add second buffer to main()

```c
int main(void) {
    GameInput inputs[2] = {0};
    GameInput *old_input = &inputs[0];
    GameInput *new_input = &inputs[1];

    // Configure repeat intervals on BOTH
    old_input->move_left.repeat.repeat_interval = 0.1f;
    new_input->move_left.repeat.repeat_interval = 0.1f;
    // ... etc
```

### Step 2: Add prepare_input_frame()

```c
void prepare_input_frame(GameInput *old_input, GameInput *new_input) {
    // Copy button states (preserve ended_down)
    new_input->move_left.button.ended_down =
        old_input->move_left.button.ended_down;
    new_input->move_right.button.ended_down =
        old_input->move_right.button.ended_down;
    // ... etc

    // Reset transition counts
    new_input->move_left.button.half_transition_count = 0;
    new_input->move_right.button.half_transition_count = 0;
    // ... etc

    // Preserve repeat timers (they accumulate across frames)
    new_input->move_left.repeat.repeat_timer =
        old_input->move_left.repeat.repeat_timer;
    // ... etc
}
```

### Step 3: Add swap at end of frame

```c
while (running) {
    prepare_input_frame(old_input, new_input);
    platform_get_input(new_input);
    tetris_update(&state, new_input, delta_time);
    platform_render(&state);

    // Swap pointers
    GameInput *temp = old_input;
    old_input = new_input;
    new_input = temp;
}
```

---

## Summary

| Question                                  | Answer                                                                   |
| ----------------------------------------- | ------------------------------------------------------------------------ |
| Are we doing something similar?           | **Partially** — we track transitions like Casey, but use single buffer   |
| Why doesn't Tetris need double buffering? | `half_transition_count` gives us "just pressed" without comparing frames |
| When would we need it?                    | Recording/playback, charge-release mechanics, netcode                    |
| Is our approach wrong?                    | **No** — it's simpler and sufficient for Tetris                          |
| Is Casey's approach better?               | **More powerful**, but more complex — overkill for simple games          |

Your instinct to compare the approaches is good! Casey's double-buffering is the "industrial strength" solution. Our single-buffer approach is the "right-sized" solution for Tetris. Both are valid — it depends on what features you need. 🎮

---

Excellent question! Yes, this is a real challenge. Let me explain the problem and solutions:

---

## The Problem: Two Conflicting Input Modes

### Game Mode (Actions)

```
Hold "W" to move forward:
  - Need: "Is W currently down?" (physical state)
  - Want: KeyPress once, then nothing until KeyRelease
  - Auto-repeat: BAD (would cause stuttering movement)
```

### Text Mode (Typing)

```
Hold "A" to type "aaaaaaa":
  - Need: "Give me characters as they're typed"
  - Want: KeyPress, KeyPress, KeyPress... (repeated)
  - Auto-repeat: GOOD (expected behavior)
```

**You can't have both simultaneously with one setting!**

---

## Solution 1: Toggle Auto-Repeat On Mode Change

```c:games/tetris/src/main_x11.c
typedef enum {
    INPUT_MODE_GAME,
    INPUT_MODE_TEXT,
} InputMode;

static InputMode g_input_mode = INPUT_MODE_GAME;

void set_input_mode(InputMode mode) {
    if (mode == g_input_mode) return;

    Bool supported;
    if (mode == INPUT_MODE_GAME) {
        /* Disable auto-repeat for game input */
        XkbSetDetectableAutoRepeat(g_x11.display, True, &supported);
        printf("Input mode: GAME (auto-repeat disabled)\n");
    } else {
        /* Enable auto-repeat for text input */
        XkbSetDetectableAutoRepeat(g_x11.display, False, &supported);
        printf("Input mode: TEXT (auto-repeat enabled)\n");
    }
    g_input_mode = mode;
}

/* Usage: */
void open_chat(void) {
    set_input_mode(INPUT_MODE_TEXT);
    // Show chat UI...
}

void close_chat(void) {
    set_input_mode(INPUT_MODE_GAME);
    // Hide chat UI...
}
```

**Problem:** There's a brief moment during the switch where input might be weird. Also, `XkbSetDetectableAutoRepeat` affects the entire display connection.

---

## Solution 2: Separate Input Streams (Better)

Handle **actions** and **text** through different X11 mechanisms:

### Actions: KeyPress/KeyRelease Events (Filtered)

```c
/* For game actions — filter auto-repeat */
case KeyPress: {
    /* Check for auto-repeat */
    if (XEventsQueued(display, QueuedAfterReading)) {
        XEvent next;
        XPeekEvent(display, &next);
        if (next.type == KeyRelease &&
            next.xkey.keycode == event->xkey.keycode &&
            next.xkey.time == event->xkey.time) {
            /* Auto-repeat — skip for game actions */
            XNextEvent(display, &next);  /* consume KeyRelease */

            /* But still process for text! */
            if (g_input_mode == INPUT_MODE_TEXT) {
                handle_text_input(event);
            }
            continue;
        }
    }

    /* Real key press — update game input */
    handle_game_input_press(event, input);

    /* Also handle text if in text mode */
    if (g_input_mode == INPUT_MODE_TEXT) {
        handle_text_input(event);
    }
    break;
}
```

### Text: XLookupString or XIM (Proper Way)

```c
/* For text input — use XLookupString to get actual characters */
void handle_text_input(XEvent *event) {
    char buffer[32];
    KeySym keysym;
    int len = XLookupString(&event->xkey, buffer, sizeof(buffer), &keysym, NULL);

    if (len > 0) {
        /* Append to text buffer */
        for (int i = 0; i < len; i++) {
            text_buffer_append(buffer[i]);
        }
    }

    /* Handle special keys */
    switch (keysym) {
    case XK_BackSpace:
        text_buffer_delete_char();
        break;
    case XK_Return:
        text_buffer_submit();
        break;
    case XK_Escape:
        text_buffer_cancel();
        break;
    }
}
```

---

## Solution 3: Always Filter, Simulate Text Repeat (Most Control)

Keep auto-repeat disabled, but **simulate it yourself** for text input:

```c:games/tetris/src/main_x11.c
typedef struct {
    char text_buffer[256];
    int text_length;

    /* For simulated auto-repeat */
    KeySym held_key;
    char held_char;
    float repeat_timer;
    float initial_delay;    /* e.g., 0.4 seconds */
    float repeat_interval;  /* e.g., 0.05 seconds */
    bool initial_fired;
} TextInputState;

static TextInputState g_text_input = {
    .initial_delay = 0.4f,
    .repeat_interval = 0.05f,
};

void update_text_input(float delta_time) {
    if (g_input_mode != INPUT_MODE_TEXT) return;
    if (g_text_input.held_key == NoSymbol) return;

    g_text_input.repeat_timer += delta_time;

    if (!g_text_input.initial_fired) {
        /* Wait for initial delay */
        if (g_text_input.repeat_timer >= g_text_input.initial_delay) {
            g_text_input.initial_fired = true;
            g_text_input.repeat_timer = 0.0f;
            text_buffer_append(g_text_input.held_char);
        }
    } else {
        /* Repeat at interval */
        while (g_text_input.repeat_timer >= g_text_input.repeat_interval) {
            g_text_input.repeat_timer -= g_text_input.repeat_interval;
            text_buffer_append(g_text_input.held_char);
        }
    }
}

void handle_key_press_for_text(XKeyEvent *event) {
    char buffer[8];
    KeySym keysym;
    int len = XLookupString(event, buffer, sizeof(buffer), &keysym, NULL);

    if (len > 0 && buffer[0] >= 32 && buffer[0] < 127) {
        /* Printable character */
        text_buffer_append(buffer[0]);

        /* Start repeat timer */
        g_text_input.held_key = keysym;
        g_text_input.held_char = buffer[0];
        g_text_input.repeat_timer = 0.0f;
        g_text_input.initial_fired = false;
    }
}

void handle_key_release_for_text(XKeyEvent *event) {
    KeySym keysym = XLookupKeysym(event, 0);

    if (keysym == g_text_input.held_key) {
        /* Stop repeating */
        g_text_input.held_key = NoSymbol;
    }
}
```

**Advantages:**

- Full control over repeat timing (match OS settings or customize)
- No mode switching at X11 level
- Works consistently across platforms
- Can have different repeat rates for different contexts

---

## Solution 4: XIM (X Input Method) — The "Proper" Way

For serious text input (international keyboards, Unicode, IME for CJK languages):

```c
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static XIM g_xim;
static XIC g_xic;

void init_text_input(Display *display, Window window) {
    /* Open input method */
    g_xim = XOpenIM(display, NULL, NULL, NULL);
    if (!g_xim) {
        fprintf(stderr, "Warning: Could not open X Input Method\n");
        return;
    }

    /* Create input context */
    g_xic = XCreateIC(g_xim,
                      XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                      XNClientWindow, window,
                      XNFocusWindow, window,
                      NULL);
    if (!g_xic) {
        fprintf(stderr, "Warning: Could not create X Input Context\n");
        XCloseIM(g_xim);
        g_xim = NULL;
    }
}

void handle_key_event_with_xim(XKeyEvent *event) {
    if (!g_xic) {
        /* Fallback to XLookupString */
        handle_key_press_for_text(event);
        return;
    }

    char buffer[64];
    KeySym keysym;
    Status status;

    int len = XmbLookupString(g_xic, event, buffer, sizeof(buffer) - 1,
                               &keysym, &status);

    if (status == XLookupChars || status == XLookupBoth) {
        buffer[len] = '\0';
        /* Append UTF-8 string to text buffer */
        text_buffer_append_utf8(buffer, len);
    }

    if (status == XLookupKeySym || status == XLookupBoth) {
        /* Handle special keys */
        switch (keysym) {
        case XK_BackSpace:
            text_buffer_delete_char();
            break;
        case XK_Return:
            text_buffer_submit();
            break;
        /* ... */
        }
    }
}

void shutdown_text_input(void) {
    if (g_xic) XDestroyIC(g_xic);
    if (g_xim) XCloseIM(g_xim);
}
```

**Advantages:**

- Handles international keyboards correctly
- Supports IME (Input Method Editor) for Chinese, Japanese, Korean
- Proper Unicode/UTF-8 support
- Auto-repeat handled by the input method

---

## What Real Games Do

### Simple Games (Tetris, Platformers)

```
- Disable auto-repeat globally
- No text input needed, or very minimal (high score initials)
- For initials: just check "key just pressed" each frame
```

### Games with Chat (MMOs, Multiplayer)

```
- Two input modes: GAME and TEXT
- GAME mode: auto-repeat disabled, physical button state
- TEXT mode: XIM or XLookupString with auto-repeat
- Toggle when chat opens/closes
```

### Professional Engines (Unity, Unreal, Godot)

```
- Abstract input layer handles platform differences
- Separate "Action" input (buttons) and "Text" input (characters)
- Text input uses platform-native text services
- Games subscribe to whichever they need
```

---

## Recommended Approach for Your Engine

```c
/* In your engine's input system */

typedef struct {
    /* Game input — physical button state */
    GameInput game;

    /* Text input — character stream */
    struct {
        char buffer[256];
        int length;
        int cursor;
        bool active;
    } text;
} InputState;

/* Platform layer provides both */
void platform_process_input(InputState *input) {
    /* Always update game input (filtered, no auto-repeat) */
    process_game_input(&input->game);

    /* If text input is active, also capture characters */
    if (input->text.active) {
        process_text_input(&input->text);
    }
}

/* Game code uses whichever it needs */
void game_update(InputState *input) {
    if (chat_is_open) {
        /* Use text input */
        if (input->text.length > 0) {
            chat_append(input->text.buffer, input->text.length);
        }

        /* But still check for Escape to close chat */
        if (was_just_pressed(&input->game.escape)) {
            close_chat();
        }
    } else {
        /* Use game input */
        if (input->game.move_left.ended_down) {
            player.x -= speed * dt;
        }
    }
}
```

---

## Summary

| Approach                            | Complexity | Best For                                    |
| ----------------------------------- | ---------- | ------------------------------------------- |
| Toggle `XkbSetDetectableAutoRepeat` | Low        | Simple games with occasional text           |
| Filter auto-repeat + XLookupString  | Medium     | Games with chat/naming                      |
| Simulate repeat yourself            | Medium     | Full control, cross-platform consistency    |
| XIM (X Input Method)                | High       | International support, professional engines |

**For Tetris:** Just disable auto-repeat. No text input needed.

**For a game with chat:** Use Solution 2 or 3 — filter auto-repeat for game actions, use XLookupString for text, simulate repeat if needed.

**For a professional engine:** Implement Solution 4 (XIM) with fallback to Solution 3, abstracted behind a clean API that games can use without knowing the platform details.
