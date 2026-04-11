#ifndef GAME_BASE_H
#define GAME_BASE_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/base.h — Asteroids game base types
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 04 — Double-buffered input: GameButtonState, GameInput, UPDATE_BUTTON,
 *             prepare_input_frame, platform_swap_input_buffers.
 *
 * JS analogy: Think of GameInput as two snapshots of a keyboard/gamepad:
 *   prev_input: "what was pressed last frame" (read-only, game uses this)
 *   curr_input: "accumulating new events right now" (platform writes here)
 * After all events are processed, we swap so the game gets fresh state.
 *
 * ASTEROIDS BUTTONS
 * ─────────────────
 * 7 buttons:
 *   quit           — ESC: close window
 *   cycle_scale_mode — TAB: cycle render scale modes
 *   thrust         — Arrow Up / W: apply thrust forward
 *   rotate_left    — Arrow Left / A: rotate ship CCW
 *   rotate_right   — Arrow Right / D: rotate ship CW
 *   fire           — Space: fire bullet
 *   restart        — Enter / R: restart game (on GAME OVER screen)
 *
 * ── DEBUG_TRAP / DEBUG_ASSERT / ASSERT_MSG ────────────────────────────────
 * Debug assertions that break in the debugger (not just abort).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>

/* ── Debug trap / assert ──────────────────────────────────────────────────
 * These expand to nothing in release builds (-DNDEBUG).
 * In debug builds (-DDEBUG), DEBUG_ASSERT fires when the condition is false
 * and DEBUG_TRAP can be used to breakpoint without a condition.
 * ASSERT_MSG(cond, msg) accepts a string literal for documentation.         */
#if defined(DEBUG)
#  if defined(__i386__) || defined(__x86_64__)
#    define DEBUG_TRAP()   __asm__ volatile("int3")
#  elif defined(__aarch64__)
#    define DEBUG_TRAP()   __asm__ volatile("brk #0")
#  else
#    include <signal.h>
#    define DEBUG_TRAP()   raise(SIGTRAP)
#  endif
#  define DEBUG_ASSERT(cond)  do { if (!(cond)) { DEBUG_TRAP(); } } while (0)
#  define ASSERT_MSG(cond, msg) \
     do { if (!(cond)) { (void)(msg); DEBUG_TRAP(); } } while (0)
#else
#  define DEBUG_TRAP()         ((void)0)
#  define DEBUG_ASSERT(cond)   ((void)(cond))
#  define ASSERT_MSG(cond, msg) ((void)(cond))
#endif

/* ── GameButtonState ─────────────────────────────────────────────────────
 * Tracks one button across two frames.
 *
 * `ended_down`: current physical state of the button.
 * `half_transitions`: how many times it changed state this frame
 *   (0 = held/untouched, 1 = pressed OR released, 2 = tapped, etc.)    */
typedef struct {
  int ended_down;        /* 1 if button is physically down at frame end.   */
  int half_transitions;  /* Number of state changes this frame.            */
} GameButtonState;

/* button_just_pressed — true if now down AND had at least one transition. */
static inline int button_just_pressed(GameButtonState button) {
  return button.ended_down && (button.half_transitions > 0);
}

/* button_just_released — true if now up AND had at least one transition.  */
static inline int button_just_released(GameButtonState button) {
  return !button.ended_down && (button.half_transitions > 0);
}

/* ── GameCamera ────────────────────────────────────────────────────────────
 * Persistent camera state owned by the game layer.
 * For Asteroids this is always (0, 0, 1.0) — no camera movement.
 * Initialized explicitly — do NOT rely on ZII for zoom (0.0 is invalid). */
typedef struct {
  float x;    /* world x of viewport left edge  */
  float y;    /* world y of viewport bottom edge (y-up) */
  float zoom; /* 1.0 = normal; > 1.0 = zoom in; < 1.0 = zoom out */
} GameCamera;

/* ── GameInput ───────────────────────────────────────────────────────────
 * One input snapshot.  Two of these form the double buffer (curr / prev).
 *
 * The buttons union allows both named access (buttons.thrust) and indexed
 * access (buttons.all[i]) for processing all buttons in a loop.
 *
 * BUTTON_COUNT = 7 (Asteroids-specific).                                 */
#define BUTTON_COUNT 7

typedef struct {
  union {
    GameButtonState all[BUTTON_COUNT]; /* Indexed access for loops.        */
    struct {
      GameButtonState quit;             /* ESC — close window.             */
      GameButtonState cycle_scale_mode; /* TAB — cycle render scale modes. */
      GameButtonState thrust;           /* Arrow Up / W — apply thrust.    */
      GameButtonState rotate_left;      /* Arrow Left / A — rotate CW.     */
      GameButtonState rotate_right;     /* Arrow Right / D — rotate CCW.   */
      GameButtonState fire;             /* Space — fire bullet.             */
      GameButtonState restart;          /* Enter / R — restart on GAME OVER */
    };
  } buttons;
} GameInput;

/* ── UPDATE_BUTTON ───────────────────────────────────────────────────────
 * Macro called by the platform event loop to record one button event.
 * `is_down` is 1 if the key is now pressed, 0 if released.
 *
 * `half_transitions` counts state changes — for a simple press+release in
 * the same frame we'd get 2 transitions. */
#define UPDATE_BUTTON(btn_ptr, is_down_val)                                  \
  do {                                                                        \
    GameButtonState *_btn = (btn_ptr);                                        \
    if (_btn->ended_down != (is_down_val)) {                                  \
      _btn->ended_down = (is_down_val);                                       \
      _btn->half_transitions++;                                                \
    }                                                                         \
  } while (0)

/* ── prepare_input_frame ─────────────────────────────────────────────────
 * Called at the START of each frame's event loop.
 * Carries forward the previous `ended_down` states but resets
 * `half_transitions` to 0 so each frame starts with a clean count.       */
static inline void prepare_input_frame(GameInput *curr, const GameInput *prev) {
  int button_count = (int)(sizeof(curr->buttons.all) / sizeof(curr->buttons.all[0]));
  for (int i = 0; i < button_count; i++) {
    curr->buttons.all[i].ended_down       = prev->buttons.all[i].ended_down;
    curr->buttons.all[i].half_transitions = 0;
  }
}

/* ── platform_swap_input_buffers ────────────────────────────────────────
 * Swaps current and previous input pointers at end of frame.
 * The next frame's "prev" is this frame's "curr".                          */
static inline void platform_swap_input_buffers(GameInput **curr, GameInput **prev) {
  GameInput *tmp = *curr;
  *curr = *prev;
  *prev = tmp;
}

#endif /* GAME_BASE_H */
