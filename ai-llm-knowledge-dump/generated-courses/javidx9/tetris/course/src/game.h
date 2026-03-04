/* ═══════════════════════════════════════════════════════════════════════════
 * game.h  —  Phase 2 Course Version
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * This header declares every type and function that the game layer exposes.
 * The platform layer (main_x11.c / main_raylib.c) includes this and calls:
 *   game_init()   → once, at startup
 *   game_update() → every frame, with input + delta_time
 *   game_render() → every frame, writes to a Backbuffer
 *
 * Handmade Hero principle: The game is a DLL / translation unit.
 *   It knows nothing about the OS, windowing, or audio hardware.
 *   It receives data (GameInput, Backbuffer) and writes data back.
 *   The platform mediates everything else.
 *
 * JS analogy: think of game.h as the TypeScript interface that defines what
 * the game "module" exports. The platform is the Node/browser runtime that
 * calls those exports.
 */

#ifndef GAME_H   /* JS: equivalent to `if (!window.__gameH)` guard at top   */
#define GAME_H   /* C:  standard include-guard to prevent double-inclusion   */

/* ── Dependency Headers ───────────────────────────────────────────────────── */
#include "utils/audio.h"      /* AudioOutputBuffer, GameAudioState, SOUND_ID  */
#include "utils/backbuffer.h" /* Backbuffer (pixel memory for rendering)       */
#include <stdbool.h>          /* bool, true, false — added in C99              */
#include <stdint.h>           /* int16_t, uint8_t, etc.                        */

/* ═══════════════════════════════════════════════════════════════════════════
 * Field & Cell Constants
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * FIELD DIMENSIONS
 * ─────────────────
 * The "field" is the rectangular area where pieces fall.
 * It includes the left wall, right wall, and floor — drawn as WALL cells.
 *
 *   Total columns: FIELD_WIDTH  = 12  (columns 0 and 11 are walls)
 *   Total rows:    FIELD_HEIGHT = 18  (row 17 is the floor)
 *   Playable area: columns 1–10, rows 0–16
 *
 * FLAT 1D ARRAY FOR 2D FIELD
 * ───────────────────────────
 * The field is stored as a flat 1D array of FIELD_WIDTH × FIELD_HEIGHT bytes.
 *
 *   unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
 *
 * To access cell at (col, row):
 *   field[row * FIELD_WIDTH + col]
 *
 * ASCII diagram (FIELD_WIDTH=12, showing first few rows):
 *
 *   col:  0   1   2   3   4   5   6   7   8   9  10  11
 *   row 0: [W] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [W]
 *   row 1: [W] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [W]
 *   ...
 *   row17: [W] [W] [W] [W] [W] [W] [W] [W] [W] [W] [W] [W]  ← floor
 *
 *   field[0 * 12 + 0]  = row 0, col 0  = left wall
 *   field[0 * 12 + 11] = row 0, col 11 = right wall
 *   field[17 * 12 + 5] = row 17, col 5 = floor
 *
 * JS equivalent:
 *   const field: number[] = new Array(FIELD_WIDTH * FIELD_HEIGHT).fill(0);
 *   const cell = (col: number, row: number) => field[row * FIELD_WIDTH + col];
 */
#define FIELD_WIDTH  12     /* Total columns including walls (left + right)  */
#define FIELD_HEIGHT 18     /* Total rows including floor (bottom row)       */
#define CELL_SIZE    30     /* Pixels per cell when rendering                */

#define SIDEBAR_WIDTH 200   /* Extra pixels to the right of the field        */

/* ── Tetromino Shape Constants ──────────────────────────────────────────── */
#define TETROMINO_SPAN        '.'  /* Character meaning "empty cell" in shape string  */
#define TETROMINO_BLOCK       'X'  /* Character meaning "filled cell" in shape string */
#define TETROMINO_LAYER_COUNT 4    /* Each tetromino fits in a 4×4 grid               */
#define TETROMINO_SIZE        16   /* Total characters per shape: 4×4 = 16            */
#define TETROMINOS_COUNT      7    /* There are 7 classic tetrominoes                 */

/* ── Predefined Colors ──────────────────────────────────────────────────── */
/* GAME_RGB packs R, G, B bytes into a 32-bit pixel value (0xAARRGGBB).
   Defined in utils/backbuffer.h.  Listed here for game-layer convenience.   */
#define COLOR_BLACK      GAME_RGB(0,   0,   0  )
#define COLOR_WHITE      GAME_RGB(255, 255, 255)
#define COLOR_GRAY       GAME_RGB(128, 128, 128)
#define COLOR_DARK_GRAY  GAME_RGB(64,  64,  64 )
#define COLOR_CYAN       GAME_RGB(0,   255, 255)  /* I-piece */
#define COLOR_BLUE       GAME_RGB(0,   0,   255)  /* J-piece */
#define COLOR_ORANGE     GAME_RGB(255, 165, 0  )  /* L-piece */
#define COLOR_YELLOW     GAME_RGB(255, 255, 0  )  /* O-piece */
#define COLOR_GREEN      GAME_RGB(0,   255, 0  )  /* S-piece */
#define COLOR_MAGENTA    GAME_RGB(255, 0,   255)  /* T-piece */
#define COLOR_RED        GAME_RGB(255, 0,   0  )  /* Z-piece */

/* ═══════════════════════════════════════════════════════════════════════════
 * Tetromino Types
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHY ENUMS INSTEAD OF MAGIC INTEGERS?
 * ──────────────────────────────────────
 * Imagine choosing a piece type with raw integers:
 *   state->current_piece.index = 3;  // What is 3? No idea without a lookup.
 *
 * With an enum:
 *   state->current_piece.index = TETROMINO_O_IDX;  // Obviously the O-piece!
 *
 * Enums also prevent category errors:
 *   // This compiles but is probably a bug (mixing field cell with piece index):
 *   int piece = TETRIS_FIELD_WALL;   // 8 — not a valid piece index!
 *
 *   // With enums, the compiler can warn about this mismatch.
 *
 * JS: TypeScript `const enum`:
 *   const enum TetrominoByIdx { I, J, L, O, S, T, Z }
 * C:  typedef enum { TETROMINO_I_IDX, ... } TETROMINO_BY_IDX;
 */

/* ── TETROMINO_BY_IDX — which piece to use ─────────────────────────────── */
typedef enum {            /* JS: const enum TetrominoByIdx { I=0, J, L, ... } */
  TETROMINO_I_IDX,        /* 0 — I-piece: cyan  horizontal/vertical bar       */
  TETROMINO_J_IDX,        /* 1 — J-piece: blue  mirror-L shape                */
  TETROMINO_L_IDX,        /* 2 — L-piece: orange L-shape                      */
  TETROMINO_O_IDX,        /* 3 — O-piece: yellow square                       */
  TETROMINO_S_IDX,        /* 4 — S-piece: green  S-shape                      */
  TETROMINO_T_IDX,        /* 5 — T-piece: magenta T-shape                     */
  TETROMINO_Z_IDX,        /* 6 — Z-piece: red   Z-shape                       */
} TETROMINO_BY_IDX;

/* ── TETRIS_FIELD_CELL — what lives in each field cell ──────────────────── */
/*
 * Values stored in state->field[].
 * 0 = empty, 1–7 = locked piece (maps to piece index + 1), 8 = wall, 9 = flash.
 *
 * NOTE: Piece cell values are (TETROMINO_BY_IDX + 1) so that 0 stays "empty".
 *   TETRIS_FIELD_I = 1 = TETROMINO_I_IDX + 1
 *   get_tetromino_color(cell_value - 1) recovers the piece index for coloring.
 */
typedef enum {
  TETRIS_FIELD_EMPTY,       /* 0 — no piece here, not a wall either            */
  TETRIS_FIELD_I,           /* 1 — locked I-piece cell                         */
  TETRIS_FIELD_J,           /* 2 — locked J-piece cell                         */
  TETRIS_FIELD_L,           /* 3 — locked L-piece cell                         */
  TETRIS_FIELD_O,           /* 4 — locked O-piece cell                         */
  TETRIS_FIELD_S,           /* 5 — locked S-piece cell                         */
  TETRIS_FIELD_T,           /* 6 — locked T-piece cell                         */
  TETRIS_FIELD_Z,           /* 7 — locked Z-piece cell                         */
  TETRIS_FIELD_WALL,        /* 8 — permanent boundary (left wall, right wall, floor) */
  TETRIS_FIELD_TMP_FLASH,   /* 9 — temporary: row completed, flashing white before collapse */
} TETRIS_FIELD_CELL;

/* ── TETROMINO_R_DIR — current rotation of a piece ──────────────────────── */
/*
 * Each tetromino can be in one of 4 rotation states, named by degrees.
 * TETROMINO_R_0 = spawn orientation (no rotation).
 * Stored as an integer index [0..3] used in tetromino_pos_value().
 */
typedef enum {
  TETROMINO_R_0,    /* 0° — default spawn orientation */
  TETROMINO_R_90,   /* 90° clockwise                  */
  TETROMINO_R_180,  /* 180°                            */
  TETROMINO_R_270,  /* 270° clockwise (= 90° CCW)     */
} TETROMINO_R_DIR;

/* ── TETROMINO_ROTATE_X_VALUE — player's requested rotation direction ────── */
typedef enum {
  TETROMINO_ROTATE_X_NONE,       /* No rotation input this frame          */
  TETROMINO_ROTATE_X_GO_LEFT,    /* Player pressed rotate-left  (Z key)   */
  TETROMINO_ROTATE_X_GO_RIGHT,   /* Player pressed rotate-right (X key)   */
} TETROMINO_ROTATE_X_VALUE;

/* ═══════════════════════════════════════════════════════════════════════════
 * RepeatInterval  — DAS / ARR Auto-Repeat
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Tetris uses two auto-repeat timings for horizontal movement:
 *
 *   DAS = Delayed Auto Shift
 *         How long you must HOLD a key before it starts repeating.
 *         `initial_delay` controls this.  Standard value: ~133–167 ms.
 *
 *   ARR = Auto Repeat Rate
 *         How fast it repeats once DAS has triggered.
 *         `interval` controls this.  Standard value: ~33–50 ms.
 *
 * EXAMPLE (move_left, initial_delay=0.15, interval=0.05):
 *   t=0.00  Key pressed → immediate move left (first trigger)
 *   t=0.15  DAS expires  → second move left
 *   t=0.20  ARR fires    → third move left
 *   t=0.25  ARR fires    → fourth move left   (continues at 20 Hz)
 *   ...
 *
 * COURSE NOTE: The reference code mutates `initial_delay = 0.0f` after
 * crossing the DAS threshold to switch into ARR mode.  This is a bug —
 * it destroys the configuration value and means DAS can never be used
 * again if the piece resets.  We fix this with a `passed_initial_delay`
 * flag that tracks the phase transition without touching the config.
 *
 * JS equivalent: like a debounce/throttle combo with two thresholds:
 *   const das = useRef(false);  // passed_initial_delay
 *   const timer = useRef(0);    // timer
 */
typedef struct {
  float timer;          /* Accumulates delta_time while button is held.
                           Resets to 0.0 on button press and after each trigger. */

  float initial_delay;  /* DAS: seconds to wait before first auto-repeat.
                           e.g., 0.15 = 150 ms.  0.0 = no initial delay.       */

  float interval;       /* ARR: seconds between auto-repeats after DAS.
                           e.g., 0.05 = 50 ms = repeats 20 times per second.   */

  bool is_active;       /* true while the button is held and DAS/ARR is running */

  /* COURSE NOTE: We deviate from the reference here.
   * The reference mutates initial_delay = 0.0f after the DAS threshold is
   * crossed, which destroys the configuration value.  Instead, we use this
   * flag to record that we've passed the initial delay phase, without ever
   * touching initial_delay.  Resetting this field on button release is all
   * that's needed to restore normal behaviour for the next key press.        */
  bool passed_initial_delay; /* true once the DAS threshold has been crossed  */
} RepeatInterval;

/* ═══════════════════════════════════════════════════════════════════════════
 * CurrentPiece — the active falling tetromino
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  int x;                             /* Column of the piece's top-left corner */
  int y;                             /* Row    of the piece's top-left corner  */
  TETROMINO_BY_IDX index;            /* Which of the 7 tetrominoes is active   */
  TETROMINO_BY_IDX next_index;       /* Which piece spawns next (shown in HUD) */
  TETROMINO_R_DIR rotate_x_value;    /* Current rotation state [0..3]          */
  TETROMINO_ROTATE_X_VALUE next_rotate_x_value; /* Pending rotation request    */
} CurrentPiece;

/* ═══════════════════════════════════════════════════════════════════════════
 * Input System
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* ── GameButtonState — per-button state with transition tracking ──────────
 *
 * HALF_TRANSITION_COUNT EXPLAINED
 * ────────────────────────────────
 * A "half transition" is one state change: up→down or down→up.
 *
 * Consider a 30 fps game (each frame ≈ 33 ms).
 * A player taps a key for exactly 20 ms:
 *   - The key goes DOWN and then UP within a single 33 ms frame.
 *   - At frame end: ended_down = 0  (key is released)
 *   - But: half_transition_count = 2  (went down once, then up once)
 *
 * If we only checked ended_down, we'd miss this input entirely!
 *
 * "Did the button GO DOWN this frame?"
 *   → (ended_down == 1) && (half_transition_count > 0)
 *   OR more generally:
 *   → half_transition_count is odd means ended_down reflects a new state.
 *
 * "Was the button pressed at all this frame (even briefly)?"
 *   → half_transition_count >= 2  (down + up = 2 transitions)
 *   OR ended_down == 1 && half_transition_count > 0
 *
 * JS equivalent: like tracking both `mousedown` AND `mouseup` events within
 * a requestAnimationFrame callback, instead of just polling button state.
 */
typedef struct {          /* JS: like a plain { endedDown: bool, transitions: number } */
  int half_transition_count; /* 0 = no change; 1 = pressed/released once;
                                2 = tapped (down+up) within one frame.         */
  int ended_down;            /* Final state at frame end: 1 = held, 0 = released */
} GameButtonState;

/* ── GameInput — all player buttons for one frame ────────────────────────
 *
 * THE UNION TRICK
 * ────────────────
 * We want TWO ways to access the same buttons:
 *   1. By NAME:  input->move_left  (readable code)
 *   2. By INDEX: input->buttons[i] (loop over all buttons to reset them)
 *
 * A C `union` makes both views occupy the SAME memory.
 * Changing buttons[0] changes move_left — they ARE the same bytes.
 *
 * JS: union { ... } has no direct equivalent.  The closest analogy is:
 *   // TypeScript: make the struct indexable AND named:
 *   interface GameInput {
 *     buttons: GameButtonState[];  // index access
 *     move_left:  GameButtonState; // named access
 *     move_right: GameButtonState;
 *     ...
 *   }
 *   // But in TS both arrays are separate allocations; in C they share memory.
 *
 * C: union { T array[N]; struct { T a; T b; ... }; }
 *    array[0] and .a point to the exact same bytes.
 */
#define BUTTON_COUNT 4  /* Must equal the number of named fields in the struct below */

typedef struct {
  union {               /* JS: (no direct equivalent) | C: union — one block of memory,
                           two views of it simultaneously.                           */
    GameButtonState buttons[BUTTON_COUNT]; /* Iterate: for (i=0; i<BUTTON_COUNT; i++) */
    struct {
      GameButtonState move_left;   /* Left  arrow key / gamepad left */
      GameButtonState move_right;  /* Right arrow key / gamepad right */
      GameButtonState move_down;   /* Down  arrow key / soft drop    */
      GameButtonState rotate_x;    /* X or Z key / rotate button     */
    };
  };
} GameInput;

/* ── UPDATE_BUTTON macro ─────────────────────────────────────────────────
 * Platform calls this once per key event to update a button's state.
 *
 * Tracks: (a) whether the button changed state this frame, and
 *         (b) what state it's in at the end of the frame.
 *
 * Usage:   UPDATE_BUTTON(input->move_left, is_left_key_down);
 *
 * The `do { ... } while(0)` wrapper is a common C macro pattern that
 * makes the macro behave like a single statement (safe inside `if` bodies).
 * JS equivalent: just a function call — macros don't exist in JS/TS.
 */
#define UPDATE_BUTTON(button, is_down)                        \
  do {                                                        \
    if ((button).ended_down != (is_down)) {                   \
      (button).half_transition_count++;  /* Record transition  */ \
      (button).ended_down = (is_down);   /* Update final state */ \
    }                                                         \
  } while (0)

/* ═══════════════════════════════════════════════════════════════════════════
 * GameState — the entire mutable state of the game
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Handmade Hero principle: State is always visible.
 *   All mutable state lives here.  No hidden static variables, no singletons,
 *   no global state.  Passing &state everywhere makes data flow explicit.
 *
 * JS equivalent: like a single top-level Redux store object — one source of truth.
 *   const state: GameState = { field: [], currentPiece: {...}, score: 0, ... }
 */
typedef struct {

  /* ── The Play Field ── */
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
                      /* Flat 1D array representing the 2D grid.
                         See FLAT 1D ARRAY diagram above in the constants section.
                         Values: TETRIS_FIELD_CELL enum (0=empty, 8=wall, etc.)
                         Access: field[row * FIELD_WIDTH + col]                 */

  /* ── Active Piece ── */
  CurrentPiece current_piece; /* The piece currently falling                    */

  /* ── Scoring & Progression ── */
  int score;          /* Player's total score                                   */
  int pieces_count;   /* Total pieces locked so far — used for level scaling    */
  int level;          /* Current difficulty level (increments every 25 pieces)  */
  bool is_game_over;  /* true when new piece can't fit at spawn position        */

  /* ── Line Clear Animation ── */
  struct {
    int indexes[TETROMINO_LAYER_COUNT]; /* Row numbers of completed lines this lock.
                                           Max 4 lines can be cleared at once (Tetris). */
    int count;                          /* How many entries in indexes[] are valid     */
    RepeatInterval flash_timer;         /* While timer > 0, game logic is frozen and
                                           completed rows are shown in white (flash).  */
  } completed_lines;

  /* ── Input Auto-Repeat (DAS/ARR) ── */
  struct {
    RepeatInterval move_left;   /* DAS/ARR state for the left-move action        */
    RepeatInterval move_right;  /* DAS/ARR state for the right-move action       */
    RepeatInterval move_down;   /* DAS/ARR state for soft-drop (faster interval) */
    /* Rotation does NOT use auto-repeat — one press = one rotation.             */
  } input_repeat;

  /* ── Gravity Timer ── */
  struct {
    RepeatInterval rotate_direction; /* Misnamed legacy field; actually stores the
                                        gravity (auto-drop) timer.
                                        .timer    = time accumulated since last drop
                                        .interval = base drop interval (0.8s)      */
  } input_values;

  /* ── Audio ── */
  GameAudioState audio;   /* All sound/music state; see utils/audio.h            */

  /* ── Platform Signals ── */
  int should_quit;    /* 1 = window closed or Escape pressed; platform will exit */
  int should_restart; /* 1 = R pressed; game_update() reinitialises on next frame */

} GameState;

/* ═══════════════════════════════════════════════════════════════════════════
 * Tetromino Shape Data  —  extern declaration
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The 7 tetromino strings are DEFINED in game.c but DECLARED here.
 * `extern` tells the compiler: "trust me, this variable exists in another
 * .c file; link it at compile time."
 *
 * JS: export const TETROMINOES: string[] = [...] in game.ts
 * C:  extern const char *TETROMINOES[7];   in game.h
 *     const char *TETROMINOES[7] = {...};  in game.c  (the actual definition)
 *
 * Each string is 16 characters representing a 4×4 grid (row-major):
 *   "..X...X...X...X."
 *    ^--- row 0 ---^
 *         ^--- row 1 ---^
 *              ...
 * '.' = empty cell, 'X' = filled cell.
 */
extern const char *TETROMINOES[7];  /* JS: export const | C: extern — lives elsewhere */

/* ═══════════════════════════════════════════════════════════════════════════
 * Function Declarations
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* ── Initialization ── */
void game_init(GameState *state);
/* Reset ALL game state: field, piece, score, timers, audio.
   Call once at program start; also called on restart.                        */

void prepare_input_frame(GameInput *old_input, GameInput *new_input);
/* Copy button held-state from old_input into new_input for the next frame,
   then clear all half_transition_count values.
   Must be called BEFORE the platform processes key events for the new frame. */

/* ── Game Logic ── */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);
/* Given a cell position (px,py) in a 4×4 piece grid and rotation r,
   return the index into the tetromino string.  Implements the 4 rotation
   formulas so we never need 4 separate piece arrays.                        */

int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y);
/* Returns 1 if `piece` at `rotation` can be placed at field position
   (pos_x, pos_y) without overlapping any non-empty cell or wall.
   Returns 0 if any solid piece cell collides.                               */

void game_update(GameState *state, GameInput *input, float delta_time);
/* Main game update: called once per frame.
   Processes input, advances gravity, detects line clears, updates score.
   delta_time = seconds since last frame (e.g., 0.016 at 60 fps).           */

/* ── Rendering — Platform Independent ── */
void game_render(Backbuffer *backbuffer, GameState *state);
/* Draw the entire game to backbuffer->pixels.  Pure pixel writes —
   no OS calls, no OpenGL, works on any platform.                            */

/* ── Audio ── */
void game_audio_init(GameAudioState *audio, int samples_per_second);
/* Set up audio volumes, patterns and sequencer defaults.
   Call from game_init() after setting audio.samples_per_second.             */

void game_play_sound(GameAudioState *audio, SOUND_ID sound);
/* Queue a sound effect at centre pan (convenience wrapper).                 */

void game_play_sound_at(GameAudioState *audio, SOUND_ID sound,
                        float pan_position);
/* Queue a sound effect at the given stereo pan position.
   pan_position: −1.0 = full left, 0.0 = centre, 1.0 = full right.         */

void game_music_play(GameAudioState *audio);
void game_music_stop(GameAudioState *audio);
/* Start or stop the background music sequencer.                             */

void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer);
/* Platform calls this every audio buffer period.  The game synthesises
   PCM samples and writes them into buffer->samples.
   Called from the audio thread — must be fast, no dynamic allocation.       */

void game_audio_update(GameAudioState *audio, float delta_time);
/* Advance the music sequencer by delta_time seconds.
   Called from game_update() on the main thread (not audio thread).          */

#endif /* GAME_H */
