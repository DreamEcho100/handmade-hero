/* ═══════════════════════════════════════════════════════════════════════════
 * game.c  —  Phase 2 Course Version
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT THIS FILE DOES
 * ────────────────────
 * This is the game layer: all Tetris logic, rendering, and audio sequencing
 * lives here.  It has NO knowledge of the OS, window system, or audio driver.
 * The platform layer calls game_init(), game_update(), game_render(), and
 * game_get_audio_samples() — that's the entire interface.
 *
 * Handmade Hero principle: One frame = one call chain.
 *   game_update() drives ALL logic for one frame.  No callbacks, no threads,
 *   no event queues.  Everything is explicit and linear.
 *
 * JS analogy: think of game_update() as the body of requestAnimationFrame().
 * Every frame the browser calls your callback; every frame the platform calls
 * game_update().  The difference: here we own the math, the memory, and the
 * timing — the runtime owns nothing.
 */

#include "game.h"
#include "utils/draw-shapes.h"  /* draw_rect(), draw_rect_blend()           */
#include "utils/draw-text.h"    /* draw_text()                              */

#include <stdio.h>    /* snprintf() — safe string formatting              */
#include <stdlib.h>   /* rand(), srand() — pseudo-random numbers          */
#include <string.h>   /* memset(), memcpy()                                */
#include <time.h>     /* time() — seed the RNG from the wall clock        */

/* ═══════════════════════════════════════════════════════════════════════════
 * Tetromino Data
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Each tetromino is a 16-character string representing a 4×4 grid (row-major).
 *
 *   '.' = empty cell (TETROMINO_SPAN)
 *   'X' = filled cell (TETROMINO_BLOCK)
 *
 * The string is read left-to-right, top-to-bottom:
 *   index 0..3  = row 0 (top)
 *   index 4..7  = row 1
 *   index 8..11 = row 2
 *   index 12..15= row 3 (bottom)
 *
 * Example — I-piece (TETROMINO_I):
 *   "..X...X...X...X."
 *    0123 4567 89AB CDEF   (hex for readability)
 *
 *   Row 0:  . . X .     ← col 2 filled
 *   Row 1:  . . X .     ← col 2 filled
 *   Row 2:  . . X .     ← col 2 filled
 *   Row 3:  . . X .     ← col 2 filled
 *
 *   Rotate 90° → tetromino_pos_value() remaps indices so it becomes a row.
 *
 * Why store as a string and not a 2D array?
 *   A string is a flat array of chars — identical memory layout to char[16].
 *   Strings are easier to visualise in source code.  The rotation math
 *   (in tetromino_pos_value) treats it as a 4×4 matrix.
 *
 * JS equivalent:  const TETROMINO_I = "..X...X...X...X.";
 * C:  const char TETROMINO_I[TETROMINO_SIZE] = "..X...X...X...X.";
 *     (TETROMINO_SIZE = 16, so the char array is exactly 16 bytes)
 */
const char TETROMINO_I[TETROMINO_SIZE] = "..X...X...X...X."; /* I — vertical bar        */
const char TETROMINO_J[TETROMINO_SIZE] = "..X...X..XX....."; /* J — J-shape (mirror L)  */
const char TETROMINO_L[TETROMINO_SIZE] = ".X...X...XX....."; /* L — L-shape             */
const char TETROMINO_O[TETROMINO_SIZE] = ".....XX..XX....."; /* O — square (2×2 block)  */
const char TETROMINO_S[TETROMINO_SIZE] = ".X...XX...X....."; /* S — S-shape             */
const char TETROMINO_T[TETROMINO_SIZE] = "..X..XX...X....."; /* T — T-shape             */
const char TETROMINO_Z[TETROMINO_SIZE] = "..X..XX..X......"; /* Z — Z-shape             */

/* Array of pointers — lets us select a piece by index (TETROMINO_BY_IDX).
   JS: const TETROMINOES: string[] = [TETROMINO_I, TETROMINO_J, ...];
   C:  const char *TETROMINOES[TETROMINOS_COUNT] = { ... };
   `extern` in game.h declares this exists; here is the actual definition.  */
const char *TETROMINOES[TETROMINOS_COUNT] = {
  TETROMINO_I, /* index 0 = TETROMINO_I_IDX */
  TETROMINO_J, /* index 1 = TETROMINO_J_IDX */
  TETROMINO_L, /* index 2 = TETROMINO_L_IDX */
  TETROMINO_O, /* index 3 = TETROMINO_O_IDX */
  TETROMINO_S, /* index 4 = TETROMINO_S_IDX */
  TETROMINO_T, /* index 5 = TETROMINO_T_IDX */
  TETROMINO_Z, /* index 6 = TETROMINO_Z_IDX */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Game-Specific Drawing Helpers
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * `static` functions are PRIVATE to this translation unit — not visible
 * outside game.c.
 * JS: private methods, or just functions not exported from a module.
 * C:  static void foo() { ... }  — file-scope private function.
 */

/* Map a TETROMINO_BY_IDX to its display colour. */
static uint32_t get_tetromino_color(TETROMINO_BY_IDX index) {
  /* `switch` on an enum value — JS: switch (index) { case ...: }
     The compiler warns if any enum case is unhandled (with -Wswitch).       */
  switch (index) {
    case TETROMINO_I_IDX: return COLOR_CYAN;
    case TETROMINO_J_IDX: return COLOR_BLUE;
    case TETROMINO_L_IDX: return COLOR_ORANGE;
    case TETROMINO_O_IDX: return COLOR_YELLOW;
    case TETROMINO_S_IDX: return COLOR_GREEN;
    case TETROMINO_T_IDX: return COLOR_MAGENTA;
    case TETROMINO_Z_IDX: return COLOR_RED;
    default:              return COLOR_GRAY;  /* Should never reach here     */
  }
}

/* Draw one game cell at (col, row) in field coordinates.
 * Leaves a 1-pixel gap around each cell for a grid look.                   */
static void draw_cell(Backbuffer *bb, int col, int row, uint32_t color) {
  int x = col * CELL_SIZE + 1;  /* +1 pixel inset from cell edge            */
  int y = row * CELL_SIZE + 1;
  int w = CELL_SIZE - 2;        /* −2 for the 1px gap on both sides         */
  int h = CELL_SIZE - 2;
  draw_rect(bb, x, y, w, h, color);
}

/* Draw all filled cells of a tetromino piece.
 * piece_index: which of the 7 tetrominoes to draw
 * field_col / field_row: top-left anchor in field coordinates
 * color: the fill colour
 * rotation: current rotation state (TETROMINO_R_DIR)
 *
 * Iterates the 4×4 piece grid; for each filled cell calls draw_cell()
 * at the corresponding field position.                                      */
static void draw_piece(Backbuffer *bb, int piece_index, int field_col,
                       int field_row, uint32_t color, TETROMINO_R_DIR rotation) {
  for (int py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (int px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      /* Get string index accounting for rotation */
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
        draw_cell(bb, field_col + px, field_row + py, color);
      }
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main Render Function  —  Called by Platform Layer Every Frame
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Writes directly into backbuffer->pixels (a flat array of uint32_t ARGB values).
 * No OS calls, no graphics API.  Pure pixel writes.
 *
 * Render order (painter's algorithm — back to front):
 *   1. Clear entire buffer to black
 *   2. Draw locked field cells
 *   3. Draw the active falling piece
 *   4. Draw HUD sidebar (score, level, next piece, controls)
 *   5. Draw game-over overlay (if is_game_over)
 */
void game_render(Backbuffer *backbuffer, GameState *state) {

  /* ── 1. Clear to black ── */
  for (int i = 0; i < backbuffer->width * backbuffer->height; i++) {
    backbuffer->pixels[i] = COLOR_BLACK;
  }

  /* ── 2. Draw locked field cells ── */
  for (int row = 0; row < FIELD_HEIGHT; row++) {
    for (int col = 0; col < FIELD_WIDTH; col++) {
      /* Read the cell value from the flat 1D array.
         row * FIELD_WIDTH + col maps (row,col) → array index.               */
      unsigned char cell_value = state->field[row * FIELD_WIDTH + col];

      switch (cell_value) {
        case TETRIS_FIELD_EMPTY:
          continue; /* Nothing to draw for empty cells                         */

        /* Locked piece cells — TETRIS_FIELD_I..Z map to TETROMINO_I..Z_IDX
           via (cell_value - 1) because field values are (piece_index + 1).   */
        case TETRIS_FIELD_I:
        case TETRIS_FIELD_J:
        case TETRIS_FIELD_L:
        case TETRIS_FIELD_O:
        case TETRIS_FIELD_S:
        case TETRIS_FIELD_T:
        case TETRIS_FIELD_Z:
          draw_cell(backbuffer, col, row, get_tetromino_color(cell_value - 1));
          break;

        case TETRIS_FIELD_WALL:
          draw_cell(backbuffer, col, row, COLOR_GRAY);
          break;

        case TETRIS_FIELD_TMP_FLASH:
          /* Completed line — shown in white during the flash animation        */
          draw_cell(backbuffer, col, row, COLOR_WHITE);
          break;
      }
    }
  }

  /* ── 3. Draw the current falling piece ── */
  draw_piece(backbuffer,
             state->current_piece.index,
             state->current_piece.x,
             state->current_piece.y,
             get_tetromino_color(state->current_piece.index),
             state->current_piece.rotate_x_value);

  /* ── 4. HUD Sidebar ── */
  int sx = FIELD_WIDTH * CELL_SIZE + 10; /* X position: just right of the field */
  int sy = 10;                            /* Y position: near top                 */
  char buf[32]; /* Temporary string buffer for number formatting (snprintf).
                   JS: const buf = String(value) — in C we need a fixed buffer. */

  /* Score label + value */
  draw_text(backbuffer, sx, sy, "SCORE", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->score);
  draw_text(backbuffer, sx, sy + 16, buf, COLOR_YELLOW, 2);

  /* Level label + value */
  draw_text(backbuffer, sx, sy + 40, "LEVEL", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->level);
  draw_text(backbuffer, sx, sy + 56, buf, COLOR_CYAN, 2);

  /* Pieces count label + value */
  draw_text(backbuffer, sx + 80, sy + 40, "PIECES", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->pieces_count);
  draw_text(backbuffer, sx + 80, sy + 56, buf, COLOR_CYAN, 2);

  /* Next piece preview label */
  draw_text(backbuffer, sx, sy + 85, "NEXT", COLOR_WHITE, 2);

  /* Next piece preview — drawn at half cell size */
  int preview_x = sx;
  int preview_y = sy + 105;
  int preview_cell_size = CELL_SIZE / 2; /* 15 px per mini-cell */

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      /* tetromino_pos_value with rotation 0 = canonical layout */
      if (TETROMINOES[state->current_piece.next_index]
                     [tetromino_pos_value(px, py, 0)] == TETROMINO_BLOCK) {
        uint32_t color = get_tetromino_color(state->current_piece.next_index);
        draw_rect(backbuffer,
                  preview_x + px * preview_cell_size + 1,
                  preview_y + py * preview_cell_size + 1,
                  preview_cell_size - 2,
                  preview_cell_size - 2,
                  color);
      }
    }
  }

  /* Controls hint at bottom of sidebar */
  int hint_y = FIELD_HEIGHT * CELL_SIZE - 90;
  draw_text(backbuffer, sx, hint_y,      "CONTROLS",          COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 18, "{} MOVE LEFT/RIGHT", COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 28, "v  SOFT DROP",       COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 38, "Z  ROTATE LEFT",     COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 48, "X  ROTATE RIGHT",    COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 58, "R  RESTART",         COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 68, "Q  QUIT",            COLOR_DARK_GRAY, 1);

  /* ── 5. Game Over Overlay ── */
  if (state->is_game_over) {
    int cx = FIELD_WIDTH * CELL_SIZE / 2;  /* Horizontal centre of the field */
    int cy = FIELD_HEIGHT * CELL_SIZE / 2; /* Vertical centre of the field   */

    /* Semi-transparent black rectangle (alpha=200 out of 255)               */
    draw_rect_blend(backbuffer, cx - 80, cy - 50, 160, 100, GAME_RGBA(0, 0, 0, 200));

    /* Red border lines (top, bottom, left, right) */
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

/* ═══════════════════════════════════════════════════════════════════════════
 * Game Initialization
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_init(GameState *state) {
  /* Handmade Hero principle: State is always visible.
     All state lives in GameState.  No hidden statics.
     We reset every field explicitly — no partial init surprises.            */

  state->score        = 0;
  state->is_game_over = false;
  state->pieces_count = 1;
  state->level        = 0;

  /* Clear line-clear state */
  state->completed_lines.count = 0;
  state->completed_lines.flash_timer.timer    = 0;
  state->completed_lines.flash_timer.interval = 0.4f; /* 400 ms flash window */
  memset(&state->completed_lines.indexes, 0, sizeof(int) * TETROMINO_LAYER_COUNT);

  /* Gravity timer
     .interval is the BASE drop interval in seconds.
     It gets shorter as the level rises (see game_update).                   */
  state->input_values.rotate_direction.timer    = 0.0f;
  state->input_values.rotate_direction.interval = 0.8f; /* 0.8s between drops at level 0 */

  /* DAS = Delayed Auto Shift (initial_delay before first auto-repeat)
     ARR = Auto Repeat Rate  (interval between subsequent repeats)
     See RepeatInterval in game.h for full explanation.                      */
  state->input_repeat.move_left.initial_delay  = 0.01f; /* 100 ms DAS */
  state->input_repeat.move_left.interval       = 0.05f; /*  50 ms ARR */
  state->input_repeat.move_left.passed_initial_delay = false;

  state->input_repeat.move_right.initial_delay  = 0.01f;
  state->input_repeat.move_right.interval       = 0.05f;
  state->input_repeat.move_right.passed_initial_delay = false;

  state->input_repeat.move_down.initial_delay  = 0.0f;  /* No DAS for soft drop */
  state->input_repeat.move_down.interval       = 0.03f; /* 33 ms ARR ≈ 30 Hz */
  state->input_repeat.move_down.passed_initial_delay = false;

  /* Build the boundary walls.
     Left wall (col 0), right wall (col FIELD_WIDTH-1), floor (row FIELD_HEIGHT-1)
     All other cells start empty.                                             */
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) {
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_WALL;
      } else {
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_EMPTY;
      }
    }
  }

  /* Seed the random number generator from the wall clock.
     JS: Math.random() is already seeded; in C we must call srand() once.
     time(NULL) returns seconds since Unix epoch — different every run.
     JS: Math.random() → C: (float)rand() / RAND_MAX for [0,1)             */
  srand((unsigned int)time(NULL));

  /* Spawn the first piece near the top-centre of the field */
  state->current_piece = (CurrentPiece){
    /* Compound literal initialisation — JS: { x: ..., y: ..., ... }
       In C, the cast (CurrentPiece){ .field = value } creates a temporary
       struct that we assign here.                                            */
    .x              = (int)((FIELD_WIDTH  * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
    .y              = 0,
    .index          = rand() % TETROMINOS_COUNT,      /* JS: Math.floor(Math.random() * 7) */
    .next_index     = rand() % TETROMINOS_COUNT,
    .rotate_x_value = TETROMINO_R_0,
  };
}

/* ═══════════════════════════════════════════════════════════════════════════
 * tetromino_pos_value  —  Rotation Index Formula
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Given a cell (px, py) in the 4×4 piece grid and a rotation `r`,
 * returns the index into the tetromino string where that cell's character is.
 *
 * WHY THIS WORKS: ROTATION AS A MATRIX TRANSFORMATION
 * ─────────────────────────────────────────────────────
 * The 4×4 grid is stored row-major: index = py * 4 + px
 *
 * Visualising the 4 rotations of index layout (N=4):
 *
 *   R_0   (0°):         R_90  (90° CW):      R_180 (180°):        R_270 (270° CW):
 *   0  1  2  3          12  8  4  0           15 14 13 12          3  7 11 15
 *   4  5  6  7          13  9  5  1           11 10  9  8          2  6 10 14
 *   8  9 10 11          14 10  6  2            7  6  5  4          1  5  9 13
 *  12 13 14 15          15 11  7  3            3  2  1  0          0  4  8 12
 *
 * FORMULAS (N=4):
 *   R_0:   py*4 + px
 *   R_90:  12 + py - (px*4)
 *   R_180: 15 - (py*4) - px
 *   R_270: 3 - py + (px*4)
 *
 * You can derive these by applying a 90° CW rotation matrix to (px, py):
 *   (px, py) → (N-1-py, px)  [90° CW]
 * and then computing the flat index of the new coordinates.
 *
 * EXAMPLE — I-piece at R_90:
 *   Original string: "..X...X...X...X."
 *                     px: 0123 0123 0123 0123
 *                     py: 0000 1111 2222 3333
 *   At (px=0,py=2), R_90: index = 12 + 2 - (0*4) = 14 → '.'
 *   At (px=2,py=2), R_90: index = 12 + 2 - (2*4) =  6 → '.'
 *   At (px=2,py=0), R_90: index = 12 + 0 - (2*4) =  4 → '.'
 *   Checking row py=0, all px → "..X." maps to a horizontal bar. ✓
 *
 * JS equivalent:
 *   function tetrominoPosValue(px, py, r) {
 *     if (r === 0)   return py * 4 + px;
 *     if (r === 1)   return 12 + py - px * 4;
 *     if (r === 2)   return 15 - py * 4 - px;
 *     if (r === 3)   return 3 - py + px * 4;
 *   }
 */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r) {
    case TETROMINO_R_0:   return py * TETROMINO_LAYER_COUNT + px;
    case TETROMINO_R_90:  return 12 + py - (px * TETROMINO_LAYER_COUNT);
    case TETROMINO_R_180: return 15 - (py * TETROMINO_LAYER_COUNT) - px;
    case TETROMINO_R_270: return 3  - py + (px * TETROMINO_LAYER_COUNT);
  }
  /* Should never reach here — all enum values are handled above.
     TODO: replace with ASSERT in a production build.                        */
  return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * tetromino_does_piece_fit
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Returns 1 if the given piece (at the given rotation and position) can be
 * placed without colliding with any non-empty field cell.  Returns 0 if any
 * solid piece cell would overlap a wall, locked piece, or floor.
 *
 * STEP-BY-STEP:
 *   For every cell (px, py) in the 4×4 piece bounding box:
 *     1. Look up the character in the piece string (accounting for rotation).
 *     2. If it's a '.' (empty span), skip — nothing to collide.
 *     3. Compute the field position: (field_x, field_y) = (pos_x+px, pos_y+py).
 *     4. Clamp check: ignore cells that hang outside field boundaries
 *        (the 4×4 box can legally overhang the top of the field during spawn).
 *     5. Read field[field_y * FIELD_WIDTH + field_x].
 *     6. If not TETRIS_FIELD_EMPTY → collision → return 0 immediately.
 *   If we reach the end without collisions → return 1 (fits).
 */
int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y) {
  /* Guard: invalid piece index */
  if (piece < 0 || piece >= TETROMINOS_COUNT)
    return 0; /* TODO: use ASSERT in production */

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      /* Step 1: index into the piece string at this rotation */
      int pi = tetromino_pos_value(px, py, rotation);

      /* Step 2: skip empty cells in the piece bounding box */
      if (TETROMINOES[piece][pi] == TETROMINO_SPAN)
        continue;

      /* Step 3: compute the target field position */
      int field_x = pos_x + px;
      int field_y = pos_y + py;

      /* Step 4: ignore cells that are completely outside the field.
         This allows pieces to spawn partially above the top (row < 0)
         and overhang to the left/right during rotation checks.              */
      if (field_x < 0 || field_x >= FIELD_WIDTH)  continue;
      if (field_y < 0 || field_y >= FIELD_HEIGHT)  continue;

      /* Step 5: compute flat index */
      int fi = field_y * FIELD_WIDTH + field_x;

      /* Step 6: any non-empty cell is a collision */
      if (state->field[fi] != TETRIS_FIELD_EMPTY)
        return 0; /* Collision detected — does not fit */
    }
  }
  return 1; /* All solid cells passed — piece fits */
}

/* ── prepare_input_frame ────────────────────────────────────────────────────
 * Carry the button "held" state from the previous frame into the new frame,
 * and clear the per-frame transition counts.
 *
 * Called BEFORE the platform polls key events for the new frame.
 *
 * WHY SWAP?
 *   The platform maintains two input buffers: old_input and current_input.
 *   At frame N end:  old_input = current_input from frame N
 *   At frame N+1:    current_input starts with ended_down from frame N
 *                    but half_transition_count = 0 (no new transitions yet)
 *
 * The platform then fires UPDATE_BUTTON() for each key event to build up
 * half_transition_count for frame N+1.
 */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
  /* Copy ended_down from old frame → new frame; reset transition counts.
     Iterating by index uses the union's array view (buttons[i]).            */
  for (int btn = 0; btn < BUTTON_COUNT; btn++) {
    current_input->buttons[btn].ended_down         = old_input->buttons[btn].ended_down;
    current_input->buttons[btn].half_transition_count = 0;
  }
}

/* ── handle_action_with_repeat ──────────────────────────────────────────────
 * Process one button's DAS/ARR logic and set *should_trigger if the action
 * should fire this frame.
 *
 * COURSE NOTE: We deviate from the reference here.
 *   The reference mutates repeat->initial_delay = 0.0f after crossing the
 *   DAS threshold to switch into ARR mode.  This destroys the configuration
 *   value and means DAS can never be used again for this action after the
 *   first key press.  Instead, we use the `passed_initial_delay` flag added
 *   to RepeatInterval in game.h to track this phase transition without
 *   touching initial_delay.  Resetting passed_initial_delay on button release
 *   restores full DAS behaviour for every subsequent key press.
 *
 * STATE MACHINE:
 *   IDLE        → button not held (is_active = false)
 *   PRESSED     → button just pressed this frame (half_transition_count > 0,
 *                 ended_down == 1): trigger once immediately if initial_delay == 0,
 *                 then enter DAS phase.
 *   DAS PHASE   → timer accumulates; no trigger until timer >= initial_delay.
 *   ARR PHASE   → passed_initial_delay = true; triggers every `interval` seconds.
 *   RELEASED    → reset everything, return to IDLE.
 */
static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval   *repeat,
                                      float             delta_time,
                                      int              *should_trigger) {
  *should_trigger = 0;

  /* ── RELEASED: reset and do nothing ── */
  if (!button->ended_down) {
    repeat->timer                = 0.0f;
    repeat->is_active            = false;
    repeat->passed_initial_delay = false; /* Ready for DAS on next press */
    return;
  }

  /* ── JUST PRESSED this frame ── */
  if (button->half_transition_count > 0) {
    repeat->timer                = 0.0f;
    repeat->is_active            = true;
    repeat->passed_initial_delay = false;

    /* Fire immediately only if there's no initial delay (soft drop).        */
    if (repeat->initial_delay <= 0.0f) {
      *should_trigger = 1;
    }
    return;
  }

  /* ── HELD from previous frame ── */
  if (!repeat->is_active) {
    return; /* Button held but active flag not set — ignore */
  }

  repeat->timer += delta_time;

  if (!repeat->passed_initial_delay) {
    /* ── DAS PHASE: waiting for initial_delay to expire ── */
    if (repeat->timer >= repeat->initial_delay) {
      *should_trigger              = 1;
      repeat->timer                = 0.0f; /* Reset timer for ARR phase     */
      repeat->passed_initial_delay = true; /* Transition to ARR phase       */
    }
  } else {
    /* ── ARR PHASE: fire every `interval` seconds ── */
    if (repeat->timer >= repeat->interval) {
      *should_trigger = 1;
      repeat->timer  -= repeat->interval; /* Keep remainder for precision   */
    }
  }
}

/* ── calculate_piece_pan ────────────────────────────────────────────────────
 * Map the piece's horizontal field position to a stereo pan value.
 *
 * Pieces near the left wall pan slightly left; near the right wall, slightly
 * right.  This gives a spatial audio sense of where pieces are falling.
 * We cap at ±0.8 (not ±1.0) so sound is never completely absent from
 * one ear.
 *
 * Returns: −1.0 (full left) … 0.0 (centre) … 1.0 (full right)
 */
static float calculate_piece_pan(int piece_x) {
  /* Playable columns are 1 … (FIELD_WIDTH − 2) = 1 … 10.
     Centre of playable area: FIELD_CENTER ≈ 6.0 (defined in utils/audio.h). */
  float center     = (FIELD_WIDTH - 2) / 2.0f + 1.0f; /* ≈ 6.0              */
  float max_offset = (FIELD_WIDTH - 2) / 2.0f;         /* ≈ 5.0              */

  float offset = (float)piece_x - center;
  float pan    = (offset / max_offset) * 0.8f; /* Scale to ±0.8, not ±1.0  */

  /* Clamp to valid range */
  if (pan < -1.0f) pan = -1.0f;
  if (pan >  1.0f) pan =  1.0f;

  return pan;
}

/* ── tetris_apply_input ─────────────────────────────────────────────────────
 * Handle all player input for this frame: rotation, left/right move, soft drop.
 * Called from game_update() only when no flash animation is active.
 */
void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {
  float pan = calculate_piece_pan(state->current_piece.x);

  /* ── Rotation ──
     Only triggers on the FRAME the key is first pressed (half_transition_count > 0).
     We check next_rotate_x_value which the platform set based on Z/X keys.    */
  if (input->rotate_x.ended_down &&
      input->rotate_x.half_transition_count != 0 &&
      state->current_piece.next_rotate_x_value != TETROMINO_ROTATE_X_NONE) {

    TETROMINO_R_DIR new_rotation;

    if (state->current_piece.next_rotate_x_value == TETROMINO_ROTATE_X_GO_RIGHT) {
      /* Clockwise: 0→90→180→270→0 */
      switch (state->current_piece.rotate_x_value) {
        case TETROMINO_R_0:   new_rotation = TETROMINO_R_90;  break;
        case TETROMINO_R_90:  new_rotation = TETROMINO_R_180; break;
        case TETROMINO_R_180: new_rotation = TETROMINO_R_270; break;
        case TETROMINO_R_270: new_rotation = TETROMINO_R_0;   break;
        default:              new_rotation = TETROMINO_R_0;   break;
      }
    } else {
      /* Counter-clockwise: 0→270→180→90→0 */
      switch (state->current_piece.rotate_x_value) {
        case TETROMINO_R_0:   new_rotation = TETROMINO_R_270; break;
        case TETROMINO_R_90:  new_rotation = TETROMINO_R_0;   break;
        case TETROMINO_R_180: new_rotation = TETROMINO_R_90;  break;
        case TETROMINO_R_270: new_rotation = TETROMINO_R_180; break;
        default:              new_rotation = TETROMINO_R_0;   break;
      }
    }

    /* Only apply the rotation if the piece fits in the new orientation       */
    if (tetromino_does_piece_fit(state, state->current_piece.index,
                                 new_rotation,
                                 state->current_piece.x,
                                 state->current_piece.y)) {
      state->current_piece.rotate_x_value = new_rotation;
      game_play_sound_at(&state->audio, SOUND_ROTATE, pan);
    }
  }

  /* ── Horizontal Movement (with DAS/ARR) ── */
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
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }

  if (should_move_right &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x + 1,
                               state->current_piece.y)) {
    state->current_piece.x++;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }

  /* ── Soft Drop (with ARR, no DAS) ── */
  int should_move_down = 0;
  handle_action_with_repeat(&input->move_down, &state->input_repeat.move_down,
                             delta_time, &should_move_down);

  if (should_move_down &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x,
                               state->current_piece.y + 1)) {
    state->current_piece.y++;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_update  —  Main Game Logic, Called Once Per Frame
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Handmade Hero principle: One frame = one call chain.
 *   game_update() drives all logic.  The platform calls this exactly once per
 *   frame with:
 *     state     — the entire game state (mutable)
 *     input     — player buttons for THIS frame
 *     delta_time — seconds since the last frame (e.g., 0.016667 at 60 fps)
 *
 * Why delta_time instead of a fixed tick counter?
 *   Time-based logic is portable across frame rates and machines.
 *   Pause is trivial: just don't call game_update() when paused.
 *   Slow-motion is trivial: pass delta_time * 0.5f.
 *   Replay is trivial: record/playback (delta_time, input) pairs.
 */
void game_update(GameState *state, GameInput *input, float delta_time) {

  /* Always update the music sequencer so the melody doesn't stutter
     even when game logic is frozen (e.g., flash animation).               */
  game_audio_update(&state->audio, delta_time);

  /* ── Handle restart (works even on game over screen) ── */
  if (state->is_game_over) {
    if (state->should_restart) {
      game_init(state);
      if (state->audio.samples_per_second > 0) {
        game_music_play(&state->audio);
        game_play_sound(&state->audio, SOUND_RESTART);
      }
    }
    return; /* Nothing else to update while game over */
  }

  /* ── Flash Animation Phase ──
     When a line completes, we flash it white for `flash_timer.interval` seconds
     before collapsing it.  During the flash, ALL game logic (input, gravity)
     is frozen.  The flash timer counts down to zero.                        */
  if (state->completed_lines.flash_timer.timer > 0) {
    state->completed_lines.flash_timer.timer -= delta_time;

    if (state->completed_lines.flash_timer.timer <= 0) {
      /* Timer reached zero → collapse all completed rows now.
       *
       * IMPORTANT: Process lines from BOTTOM to TOP (highest row index first).
       *
       * WHY BOTTOM-TO-TOP?
       *   When we collapse a row, every row ABOVE it shifts DOWN by 1.
       *   If we processed top-to-bottom, the stored row indices would become
       *   stale after the first collapse.
       *
       *   Example: lines[0]=14, lines[1]=15  (two adjacent completed rows)
       *   If we collapse row 14 first:
       *     → row 15 shifts to row 14
       *     → but lines[1] still says 15 — WRONG!  We'd skip it.
       *
       *   By collapsing row 15 first (bottom row), row 14 is unaffected.
       *   After collapsing row 15, row 14 is still at row 14 — correct!
       *
       *   Additionally, after collapsing each row, we increment the stored
       *   indices of all remaining (not-yet-collapsed) rows, because they
       *   all shifted down by 1.
       */
      for (int i = state->completed_lines.count - 1; i >= 0; i--) {
        int line_index = state->completed_lines.indexes[i];

        /* Collapse this row: shift every row above it down by one.
           Start at line_index and work upward (decreasing py).              */
        for (int py = line_index; py > 0; --py) {
          for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
            /* Copy the cell from the row above (py-1) to the current row (py) */
            state->field[py * FIELD_WIDTH + px] =
                state->field[(py - 1) * FIELD_WIDTH + px];
          }
        }

        /* Clear the top row (row 0) — no row above it to copy from          */
        for (int px = 1; px < FIELD_WIDTH - 1; px++) {
          state->field[px] = TETRIS_FIELD_EMPTY;
        }

        /* Adjust remaining stored row indices.
           All rows we haven't collapsed yet (indices 0..i-1) are ABOVE
           line_index and have just shifted down by 1.  Increment them so
           our subsequent collapses target the correct rows.                 */
        for (int j = i - 1; j >= 0; --j) {
          if (state->completed_lines.indexes[j] < line_index) {
            state->completed_lines.indexes[j]++;
          }
        }
      }

      /* Play sound AFTER all collapses are done */
      if (state->completed_lines.count == 4) {
        game_play_sound(&state->audio, SOUND_TETRIS);      /* Tetris! */
      } else if (state->completed_lines.count > 0) {
        game_play_sound(&state->audio, SOUND_LINE_CLEAR);
      }

      state->completed_lines.count = 0; /* Lines processed — clear state    */
    }
    return; /* Freeze all logic while flashing (even if timer just hit zero) */
  }

  /* ── Player Input ── */
  tetris_apply_input(state, input, delta_time);

  /* ── Gravity Timer ── */
  state->input_values.rotate_direction.timer += delta_time;

  /* Drop speed increases with level:
     Base interval is 0.8s.  Each level adds a small speed bonus.
     Capped at 0.1s minimum (10 drops/second).                              */
  int game_speed = state->level > 0 ? state->level : 0;
  float drop_interval = state->input_values.rotate_direction.interval
                        + (0.01f + game_speed * 0.01f);
  if (drop_interval < 0.1f) {
    drop_interval = 0.1f; /* Floor: never faster than 10 drops/second       */
  }

  /* ── Gravity Drop ── */
  if (state->input_values.rotate_direction.timer >= drop_interval) {
    /* Subtract interval and keep remainder for timing precision.
       JS: timer %= interval  — but subtraction keeps fractional "overflow". */
    state->input_values.rotate_direction.timer -= drop_interval;

    /* Try to move piece down one row */
    if (tetromino_does_piece_fit(state, state->current_piece.index,
                                 state->current_piece.rotate_x_value,
                                 state->current_piece.x,
                                 state->current_piece.y + 1)) {
      state->current_piece.y++; /* Piece falls one row */

    } else {
      /* ── Piece Locked ── */
      /* Stamp the current piece into the field.
         Store (piece_index + 1) so that 0 remains the "empty" sentinel.    */
      for (int px = 0; px < TETROMINO_LAYER_COUNT; ++px) {
        for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
          int pi = tetromino_pos_value(px, py, state->current_piece.rotate_x_value);
          if (TETROMINOES[state->current_piece.index][pi] != TETROMINO_SPAN) {
            int fx = state->current_piece.x + px;
            int fy = state->current_piece.y + py;
            if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT) {
              state->field[fy * FIELD_WIDTH + fx] =
                  state->current_piece.index + 1; /* +1: 0 reserved for empty */
            }
          }
        }
      }

      float pan = calculate_piece_pan(state->current_piece.x);
      game_play_sound_at(&state->audio, SOUND_DROP, pan);

      /* ── Detect Completed Lines ── */
      state->completed_lines.count = 0;

      /* Check each row covered by the piece's 4×4 bounding box              */
      for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
        int row_y = state->current_piece.y + py;

        /* Skip if outside the playable area (above top or at/below floor)   */
        if (row_y < 0 || row_y >= FIELD_HEIGHT - 1) continue;

        /* Check all playable columns in this row (skip walls at 0 and N-1) */
        bool completed = true;
        for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
          if (state->field[row_y * FIELD_WIDTH + px] == TETRIS_FIELD_EMPTY) {
            completed = false;
            break; /* Early exit — one empty cell means row is not complete  */
          }
        }

        if (completed) {
          /* Tag row as TMP_FLASH — renderer draws it white until timer fires */
          for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
            state->field[row_y * FIELD_WIDTH + px] = TETRIS_FIELD_TMP_FLASH;
          }
          /* Store the row index for later collapse                           */
          state->completed_lines.indexes[state->completed_lines.count++] = row_y;
        }
      }

      /* Start the flash timer if any lines were completed                    */
      if (state->completed_lines.count > 0) {
        state->completed_lines.flash_timer.timer =
            state->completed_lines.flash_timer.interval;
      }

      /* ── Scoring ──
       *
       * 25 points for every piece locked (always).
       * Line-clear bonus:  (1 << completed_lines.count) * 100
       *
       * SCORE FORMULA EXPLAINED: (1 << count) * 100
       *   1 line:  (1 << 1) * 100 = 200
       *   2 lines: (1 << 2) * 100 = 400
       *   3 lines: (1 << 3) * 100 = 800
       *   4 lines: (1 << 4) * 100 = 1600  (Tetris!)
       *
       * The bit-shift `1 << n` is equivalent to 2^n (powers of 2).
       * This makes clearing more lines simultaneously worth exponentially more,
       * matching classic Tetris scoring.
       * JS: (1 << count) works identically in JavaScript.
       */
      state->pieces_count++;
      state->score += 25; /* Flat reward per piece locked */
      if (state->completed_lines.count > 0) {
        state->score += (1 << state->completed_lines.count) * 100;
      }

      /* Level up every 25 pieces */
      if (state->pieces_count % 25 == 0) {
        state->level++;
        game_play_sound(&state->audio, SOUND_LEVEL_UP);
      }

      /* Reset gravity timer and spawn next piece */
      state->input_values.rotate_direction.timer = 0.0f;
      state->current_piece = (CurrentPiece){
        .x              = (int)((FIELD_WIDTH  * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
        .y              = 0,
        .index          = state->current_piece.next_index, /* Promote next piece */
        .next_index     = rand() % TETROMINOS_COUNT,       /* Draw new next piece */
        .rotate_x_value = TETROMINO_R_0,
      };

      /* ── Game Over Check ──
         If the newly spawned piece doesn't fit, the field is full.          */
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
