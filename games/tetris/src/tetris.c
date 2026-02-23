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

void game_init(GameState *state, GameInput *input) {
  state->score = 0;
  state->game_over = false;
  state->pieces_count = 0;

  // Time-based dropping
  state->tetromino_drop.timer = 0.0f;
  state->tetromino_drop.interval = 1.0f; // 1 second between drops initially

  /* Configure auto-repeat intervals */
  input->move_left.repeat.interval = 0.05f;
  input->move_right.repeat.interval = 0.05f;
  input->move_down.repeat.interval = 0.03f;

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

/* Reset transition counts — they're per-frame */
void prepare_input_frame(GameInput *input) {
  input->move_left.button.half_transition_count = 0;
  input->move_right.button.half_transition_count = 0;
  input->move_down.button.half_transition_count = 0;
  input->rotate_x.button.half_transition_count = 0;
  input->rotate_x.value = 0;
};

/**
 * Process an action with auto-repeat behavior.
 *
 * @param action     The action to process (modified in place)
 * @param delta_time Seconds since last frame
 * @param should_trigger Output: set to 1 if action should fire this frame
 *
 * Behavior:
 * - Button just pressed → trigger immediately, reset timer
 * - Button held → accumulate time, trigger when timer >= interval
 * - Button released → reset timer, don't trigger
 */
static void handle_action_with_repeat(GameActionWithRepeat *action,
                                      float delta_time, int *should_trigger) {
  *should_trigger = 0;

  if (!action->button.ended_down) {
    /* Button is up — reset timer, don't trigger */
    action->repeat.timer = 0.0f;
    return;
  }

  /* Button is down */
  if (action->button.half_transition_count > 0) {
    /* Just pressed this frame — trigger immediately */
    *should_trigger = 1;
    action->repeat.timer = 0.0f;
  } else {
    /* Held from previous frame — check auto-repeat timer */
    action->repeat.timer += delta_time;

    if (action->repeat.timer >= action->repeat.interval) {
      /* Timer expired — trigger and reset (keeping remainder for precision) */
      action->repeat.timer -= action->repeat.interval;
      *should_trigger = 1;
    }
  }
}

void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {

  /* Rotate clockwise: try rotation + 1. (% 4 wraps 3 back to 0.) */
  if (input->rotate_x.button.ended_down &&
      input->rotate_x.button.half_transition_count != 0 &&
      input->rotate_x.value != TETROMINO_ROTATE_X_NONE) {

    TETROMINO_R_DIR new_rotation;
    if (input->rotate_x.value == TETROMINO_ROTATE_X_GO_RIGHT) {
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
    } else if (input->rotate_x.value == TETROMINO_ROTATE_X_GO_LEFT) {
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
    int does_piece_fit = tetromino_does_piece_fit(
        state, state->current_piece.index, new_rotation,
        state->current_piece.col, state->current_piece.row);

    if (does_piece_fit) {
      state->current_piece.rotation = new_rotation;
    }

    // input->rotate_x.value = 0;
  }

  /* ── Horizontal movement: independent auto-repeat ── */
  int should_move_left = 0;
  int should_move_right = 0;

  handle_action_with_repeat(&input->move_left, delta_time, &should_move_left);
  handle_action_with_repeat(&input->move_right, delta_time, &should_move_right);

  /* Move left: try current_piece.col - 1 */
  if (should_move_left &&
      tetromino_does_piece_fit(
          state, state->current_piece.index, state->current_piece.rotation,
          state->current_piece.col - 1, state->current_piece.row)) {
    state->current_piece.col--;
  }

  /* Move right: try current_piece.col + 1 */
  if (should_move_right &&
      tetromino_does_piece_fit(
          state, state->current_piece.index, state->current_piece.rotation,
          state->current_piece.col + 1, state->current_piece.row)) {
    state->current_piece.col++;
  }

  /* ── Soft drop: independent auto-repeat (faster interval) ── */
  int should_move_down = 0;

  handle_action_with_repeat(&input->move_down, delta_time, &should_move_down);
  /* Soft drop: try current_piece.row + 1 (down = positive Y) */
  if (should_move_down &&
      tetromino_does_piece_fit(
          state, state->current_piece.index, state->current_piece.rotation,
          state->current_piece.col, state->current_piece.row + 1)) {
    state->current_piece.row++;
  }
}

void tetris_update(GameState *state, GameInput *input, float delta_time) {
  if (state->game_over)
    return;

  /* ── Handle player input (always responsive) ── */
  tetris_apply_input(state, input, delta_time);

  /* ── Accumulate time for gravity ── */
  state->tetromino_drop.timer += delta_time;

  /* ── Check if it's time to drop the piece ── */
  if (state->tetromino_drop.timer >= state->tetromino_drop.interval) {
    // Reset timer, keeping remainder for precision
    state->tetromino_drop.timer -= state->tetromino_drop.interval;

    /* Try to move the piece down one row */
    if (tetromino_does_piece_fit(
            state, state->current_piece.index, state->current_piece.rotation,
            state->current_piece.col, state->current_piece.row + 1)) {
      state->current_piece.row++;
    } else {
      /* If it doesn't fit, piece has landed - locking in lesson 09 */
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

      /* 2. Score and difficulty */
      state->score += 25;
      state->pieces_count++;
      if (state->pieces_count % 50 == 0 &&
          state->tetromino_drop.interval > 0.2f) {
        state->tetromino_drop.interval -= 0.05f;
      }

      // reset drop-timer and current piece for next round
      state->tetromino_drop.timer = 0.0f;
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
    //
  }
}

// 1. Precision
// ```c
// // Old: Loses precision
// state->tetromino_drop.timer -= state->tetromino_drop.interval;  // Keeps
// remainder!

// // vs tick-based
// state->speed_count = 0; // Loses any "extra" time
// ```

// 2. Easy Difficulty Scaling
// ```c
// // Make game faster as player progresses
// state->tetromino_drop.interval = 1.0f / (1.0f + state->piece_count * 0.05f);
// // piece 0:  1.0 second
// // piece 10: 0.67 seconds
// // piece 20: 0.5 seconds
// ```

// 3. Smooth Slow-Motion
// ```c
// // Just multiply delta_time
// float slow_mo_factor = 0.5f;  // Half speed
// tetris_update(&game_state, &input, delta_time * slow_mo_factor);
// ```

// 4. Pause is Trivial
// ```c
// if (!paused) {
//   tetris_update(&game_state, &input, delta_time);
// }
// // When paused, just don't call update - timer doesn't advance
// ```