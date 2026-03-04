/* ═══════════════════════════════════════════════════════════════════════════
 * game.h  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * The shared type definitions, constants, and public API for the Snake game.
 *
 * This header is included by:
 *   game.c      — the game logic implementation
 *   audio.c     — the audio engine
 *   platform.h  — to declare the platform contract
 *   main_x11.c  — the X11 backend
 *   main_raylib.c — the Raylib backend
 *
 * game.h NEVER includes X11, Raylib, or OS-specific headers.
 * It must compile cleanly on any platform with any backend.
 *
 * JS analogy: think of this as the TypeScript types-only module (`.d.ts`)
 * that defines the contract between game logic and the platform host.
 */

#ifndef GAME_H
#define GAME_H

#include <stdint.h>  /* uint32_t — fixed-width integer types */
#include <stdlib.h>  /* malloc, free, rand, srand            */
#include <string.h>  /* memset, strlen                       */
#include <stdio.h>   /* snprintf (never sprintf — always bounds-checked!) */
#include <time.h>    /* time() — for srand seeding           */

#include "utils/backbuffer.h"  /* SnakeBackbuffer, GAME_RGB/RGBA, COLOR_* */
#include "utils/audio.h"       /* GameAudioState, AudioOutputBuffer, SOUND_ID */

/* ══════ Debug Assertions ════════════════════════════════════════════════════
 *
 * COURSE NOTE: The reference source has no debug assertions.  We add them
 * because students learning C need a way to catch invalid states early.
 * See Lesson 01 and COURSE-BUILDER-IMPROVEMENTS.md.
 *
 * USAGE:
 *   ASSERT(ptr != NULL);            // halts in debug, no-op in release
 *   ASSERT(index < MAX_SNAKE);
 *
 * In debug builds (-DDEBUG), ASSERT evaluates the expression and halts the
 * program at the exact line that failed — much better than a random crash
 * later.  __builtin_trap() causes the debugger to stop here and show a stack
 * trace.
 *
 * In release builds (no -DDEBUG), the expression is completely removed by
 * the preprocessor — zero runtime cost.
 *
 * JS equivalent: a conditional `debugger` statement, but with an expression:
 *   if (!expr) debugger;  // doesn't exist as a built-in in JS — closest is
 *                          // console.assert(expr, msg)
 */
#ifdef DEBUG
  #define DEBUG_TRAP()  __builtin_trap()  /* Halt here — debugger will break */
  #define ASSERT(expr)  do { if (!(expr)) { DEBUG_TRAP(); } } while(0)
#else
  #define DEBUG_TRAP()  /* no-op in release builds */
  #define ASSERT(expr)  /* no-op in release builds */
#endif

/* ══════ Grid & Window Constants ════════════════════════════════════════════
 *
 * These determine the visual layout of the game.
 * All values are derived from GRID_WIDTH, GRID_HEIGHT, and CELL_SIZE — change
 * those three to resize the entire game.
 *
 * JS equivalent: constants at the top of a module — same idea, but in C
 * they're resolved at compile time (no runtime object lookup).
 */
#define GRID_WIDTH   60       /* cells horizontally                           */
#define GRID_HEIGHT  20       /* cells vertically (play field)                */
#define CELL_SIZE    20       /* pixels per cell                              */
#define HEADER_ROWS   3       /* cell-rows occupied by the score panel        */

/* Maximum number of segments in the ring buffer.
 * Must be >= GRID_WIDTH * GRID_HEIGHT (the maximum possible snake length). */
#define MAX_SNAKE   1200      /* = GRID_WIDTH * GRID_HEIGHT                  */

/* Pixel dimensions of the window */
#define WINDOW_WIDTH   (GRID_WIDTH  * CELL_SIZE)           /* 1200 px */
#define WINDOW_HEIGHT  ((GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE)  /* 460 px  */

/* ══════ Direction Enum ═════════════════════════════════════════════════════
 *
 * WHY AN ENUM INSTEAD OF PLAIN INTEGERS?
 *   Without an enum, you might write `direction = 0` and later forget what 0
 *   means.  With a typed enum, `direction = SNAKE_DIR_UP` is self-documenting
 *   and the compiler catches category errors (e.g., passing a direction where
 *   a bool is expected).
 *
 * TURN ARITHMETIC:
 *   Turn right (CW):  new_dir = (dir + 1) % 4
 *   Turn left (CCW):  new_dir = (dir + 3) % 4   (same as (dir - 1 + 4) % 4)
 *   U-turn guard:     opposite = (dir + 2) % 4  — block if input equals this
 *
 * JS equivalent: Object.freeze({ UP:0, RIGHT:1, DOWN:2, LEFT:3 })
 */
typedef enum {
    SNAKE_DIR_UP    = 0,   /* ↑ y decreases */
    SNAKE_DIR_RIGHT = 1,   /* → x increases */
    SNAKE_DIR_DOWN  = 2,   /* ↓ y increases */
    SNAKE_DIR_LEFT  = 3    /* ← x decreases */
} SNAKE_DIR;

/* ══════ Game Phase Enum ════════════════════════════════════════════════════
 *
 * COURSE NOTE: The reference source uses `int game_over` — a boolean flag.
 * We upgrade to a typed enum and switch-dispatch.  Benefits:
 *   • Scales to more phases (PAUSED, MENU) without adding more booleans.
 *   • The compiler warns if a switch is missing a case.
 *   • Self-documenting: `phase == GAME_PHASE_PLAYING` vs `game_over == 0`.
 * See Lesson 04 and COURSE-BUILDER-IMPROVEMENTS.md for the full comparison.
 *
 * JS equivalent: const GAME_PHASE = Object.freeze({ PLAYING:0, GAME_OVER:1 })
 */
typedef enum {
    GAME_PHASE_PLAYING   = 0,  /* Normal play — snake moves, input accepted    */
    GAME_PHASE_GAME_OVER = 1   /* Snake hit a wall or itself — overlay shown   */
} GAME_PHASE;

/* ══════ Segment ════════════════════════════════════════════════════════════
 *
 * One cell position in the snake's body.  The snake is an ARRAY of these,
 * not a linked list — no `next` pointer, no `malloc` per segment.
 *
 * JS equivalent: { x: number, y: number } — but with fixed C integer types
 * and no heap allocation.
 */
typedef struct {
    int x, y;  /* Grid coordinates: x in [0, GRID_WIDTH-1], y in [0, GRID_HEIGHT-1] */
} Segment;

/* ══════ Input System ═══════════════════════════════════════════════════════
 *
 * The Handmade Hero input model (Casey Muratori).
 *
 * GameButtonState tracks both the current key state AND the number of
 * transitions within one frame, so sub-frame taps (pressed + released within
 * a single 16ms frame) are never lost.
 *
 * FIELDS:
 *   ended_down            1 = key is held down NOW; 0 = key is up.
 *   half_transition_count Number of state changes this frame:
 *     0 = no change (held idle the whole frame)
 *     1 = one transition (normal press or release)
 *     2 = full tap (pressed AND released within one frame)
 *
 * "Just pressed this frame" check:
 *   button.ended_down && button.half_transition_count > 0
 *
 * JS equivalent: tracking `keydown` with `event.repeat === false` — but
 * the browser manages all that; here we track it in the struct ourselves.
 */
typedef struct {
    int half_transition_count;  /* Transitions (press/release events) this frame */
    int ended_down;             /* 1 = currently held; 0 = currently released    */
} GameButtonState;

/* UPDATE_BUTTON — record a press or release event.
 *
 * Increments half_transition_count only when the state changes (prevents
 * double-counting the same event).
 *
 * Usage:
 *   On KeyPress:   UPDATE_BUTTON(input->turn_left, 1);
 *   On KeyRelease: UPDATE_BUTTON(input->turn_left, 0);
 *
 * JS equivalent: `addEventListener('keydown', e => { ... })`
 */
#define UPDATE_BUTTON(button, is_down)                                         \
    do {                                                                       \
        if ((button).ended_down != (is_down)) {                                \
            (button).half_transition_count++;                                  \
            (button).ended_down = (is_down);                                   \
        }                                                                      \
    } while (0)

/* BUTTON_COUNT — number of named buttons in GameInput (used by prepare_input_frame).
 * Must match the number of fields in the union's array. */
#define BUTTON_COUNT 2

/* GameInput — one frame's worth of player input.
 *
 * DOUBLE-BUFFER PATTERN (course upgrade over reference source):
 *   The platform maintains TWO GameInput structs: old_input and current_input.
 *   Each frame:
 *     1. Swap: old_input ← current_input content; current_input ← old_input content
 *     2. prepare_input_frame(old_input, current_input):
 *          copies ended_down from old → current (preserves key-held state)
 *          resets half_transition_count in current to 0
 *     3. platform_get_input(&current_input) fills in events from OS
 *
 * Why two buffers instead of one?
 *   X11 is event-based: you only receive state changes (press/release events),
 *   not continuous "is key held?" polls.  Without copying ended_down from the
 *   previous frame, a held key would appear as "not held" every frame after
 *   the first — breaking the "held down" detection in game logic.
 *
 * UNION — named fields AND array access:
 *   union { buttons[BUTTON_COUNT]; struct { turn_left, turn_right; }; }
 *   This allows:
 *     for (int i = 0; i < BUTTON_COUNT; i++)
 *       input->buttons[i].half_transition_count = 0;
 *   AND:
 *     input->turn_left.ended_down   (named access — no array-index guessing)
 *
 * JS equivalent: an array-backed object like
 *   { buttons: [...], get turn_left() { return this.buttons[0]; } }
 */
typedef struct {
    union {
        GameButtonState buttons[BUTTON_COUNT];  /* Array access (for loops)   */
        struct {
            GameButtonState turn_left;   /* CCW: Left arrow or A              */
            GameButtonState turn_right;  /* CW:  Right arrow or D             */
        };
    };
    int restart;  /* R or Space — fires once per press; not a GameButtonState  */
    int quit;     /* Q or Escape — fires once; platform sets g_is_running = 0  */
} GameInput;

/* ══════ GameState ══════════════════════════════════════════════════════════
 *
 * All mutable game state lives in ONE struct.  No global variables.
 *
 * Handmade Hero principle: "State is always visible."
 *   The platform passes a pointer to this struct every frame.  You can inspect
 *   every field with a debugger at any point in time.  No hidden state.
 *
 * JS equivalent: a plain object returned by createGameState() — but
 * it lives on the stack in main() and is passed by pointer, not reference.
 */
typedef struct {
    /* ── Snake Body — Ring Buffer ─────────────────────────────────────────
     * The snake's body is stored in segments[] as a RING BUFFER.
     *
     * Ring buffer mechanics:
     *   head  = index of the most-recently-written segment (the snake's head).
     *   tail  = index of the oldest segment (the snake's tail tip).
     *   length = number of valid segments.
     *
     * Advancing head: head = (head + 1) % MAX_SNAKE
     * Advancing tail: tail = (tail + 1) % MAX_SNAKE
     * Reading from tail to head: idx = tail; for rem = length; rem-- { … idx = (idx+1)%MAX_SNAKE }
     *
     * WHY A RING BUFFER instead of memmove or a linked list?
     *   • No memmove: moving 1000 elements every step = O(N). Ring = O(1).
     *   • No malloc per segment: fixed array, zero allocation in the loop.
     *   • Cache-friendly: contiguous memory, sequential access patterns.
     *
     * JS equivalent: a circular buffer backed by a fixed JS Array.
     */
    Segment segments[MAX_SNAKE];
    int head;    /* Index of the head segment                                  */
    int tail;    /* Index of the tail-tip segment                              */
    int length;  /* Current snake length (number of valid segments)            */

    /* ── Direction ───────────────────────────────────────────────────────── */
    SNAKE_DIR direction;       /* Current moving direction (applied each tick) */
    SNAKE_DIR next_direction;  /* Queued by player input; applied next tick.
                                  WHY TWO FIELDS?
                                  If direction were updated immediately on input,
                                  the snake might try to reverse (180° U-turn)
                                  within the same tick it was checked — causing
                                  instant self-collision.  Staging the change in
                                  next_direction and applying it at move time gives
                                  the U-turn guard one full tick to reject it. */

    /* ── Movement Timing ─────────────────────────────────────────────────── */
    float move_timer;     /* Accumulates delta_time each frame.               */
    float move_interval;  /* Move when timer >= interval (seconds/step).
                             Decreases as score grows → game speeds up.        */

    /* ── Food & Score ────────────────────────────────────────────────────── */
    int food_x, food_y;  /* Grid position of current food                     */
    int grow_pending;    /* Segments still to be added before tail advances.
                            Incremented by 5 each time food is eaten.
                            While > 0, tail stays put — snake length grows.    */
    int score;           /* Current game score                                */
    int best_score;      /* Highest score ever; preserved across snake_init().  */

    /* ── Phase (replaces `int game_over`) ───────────────────────────────── */
    GAME_PHASE phase;    /* Current game phase — dispatch in snake_update()    */

    /* ── Audio ───────────────────────────────────────────────────────────── */
    GameAudioState audio;  /* All audio state (see utils/audio.h)              */

} GameState;

/* ══════ Public Game API ════════════════════════════════════════════════════
 *
 * These are the ONLY functions the platform calls into the game layer.
 * The platform never accesses GameState fields directly — it treats the
 * struct as opaque and lets game.c manage it.
 *
 * Calling order each frame:
 *   prepare_input_frame(old_input, current_input);  // reset per-frame counters
 *   platform_get_input(current_input);              // fill new events
 *   snake_update(&state, current_input, delta_time);
 *   snake_render(&state, &backbuffer);
 *   game_get_audio_samples(&state, &audio_buffer);  // fill audio
 */

/* prepare_input_frame — prepare input buffers for the next frame.
 *
 * WHAT IT DOES:
 *   1. Copies ended_down from old_input → current_input (preserves held state).
 *   2. Resets half_transition_count in current_input to 0 (fresh event count).
 *
 * MUST be called every frame, BEFORE platform_get_input().
 *
 * COURSE NOTE: The reference source calls this with a single arg:
 *   prepare_input_frame(GameInput *input)  — only resets transitions.
 * We upgrade to two-arg to properly preserve ended_down.
 * See Lesson 03 and COURSE-BUILDER-IMPROVEMENTS.md.
 */
void prepare_input_frame(const GameInput *old_input, GameInput *current_input);

/* snake_init — reset the game state for a new game.
 * Preserves best_score across calls. */
void snake_init(GameState *s);

/* snake_spawn_food — place food at a random grid cell not occupied by the snake.
 * Uses a retry loop — calls rand() until a free cell is found.
 * srand() must have been called before the first invocation (done in snake_init). */
void snake_spawn_food(GameState *s);

/* snake_update — advance game logic by delta_time seconds.
 * Reads current_input, accumulates move_timer, dispatches on state->phase. */
void snake_update(GameState *s, const GameInput *input, float delta_time);

/* snake_render — draw the full frame into backbuffer.
 * Does NOT touch audio state — that is handled by game_get_audio_samples(). */
void snake_render(const GameState *s, SnakeBackbuffer *backbuffer);

/* game_audio_init — initialize audio state.
 * Call once before the game loop, after platform sets state.audio.samples_per_second. */
void game_audio_init(GameState *s);

/* game_music_start — begin (or restart) the background melody.
 * Resets the sequencer to note 0; sets music_volume to default.
 * Called automatically at the end of snake_init() — both on first start
 * and on every player restart. */
void game_music_start(GameAudioState *audio);

/* game_music_stop — silence the music channel (e.g. on game over). */
void game_music_stop(GameAudioState *audio);

/* game_play_sound_at — trigger a sound effect at a given stereo pan position.
 * pan = -1.0 (full left) … 0.0 (centre) … +1.0 (full right). */
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound, float pan);

/* game_play_sound — trigger a centred (pan = 0.0) sound effect. */
void game_play_sound(GameAudioState *audio, SOUND_ID sound);

/* game_get_audio_samples — fill an AudioOutputBuffer with PCM samples.
 * Called by the platform backend once per audio callback or frame. */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer);

#endif /* GAME_H */
