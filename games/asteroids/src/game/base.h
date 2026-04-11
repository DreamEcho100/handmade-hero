#ifndef GAME_BASE_H
#define GAME_BASE_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/base.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 07 — Double-buffered input: GameButtonState, GameInput, UPDATE_BUTTON,
 *             prepare_input_frame, platform_swap_input_buffers.
 *
 * JS analogy: Think of GameInput as two snapshots of a keyboard/gamepad:
 *   prev_input: "what was pressed last frame" (read-only, game uses this)
 *   curr_input: "accumulating new events right now" (platform writes here)
 * After all events are processed, we swap so the game gets fresh state.
 *
 * WHY platform_swap_input_buffers LIVES HERE (not platform.h)
 * ────────────────────────────────────────────────────────────
 * This function only touches `GameInput` structs — no OS handles, no platform
 * APIs.  It belongs next to `prepare_input_frame` which it's always paired
 * with.  Both are pure game-layer logic.
 *
 * ── DEBUG_TRAP / DEBUG_ASSERT ──────────────────────────────────────────────
 * LESSON 05 — Debug assertions that break in the debugger (not just abort).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>

/* ── Debug trap / assert ──────────────────────────────────────────────────
 * LESSON 05 — These expand to nothing in release builds.
 * In debug builds (-DDEBUG), DEBUG_ASSERT fires when the condition is false
 * and DEBUG_TRAP can be used to breakpoint without a condition.            */
#if defined(DEBUG)
#if defined(__i386__) || defined(__x86_64__)
#define DEBUG_TRAP() __asm__ volatile("int3")
#elif defined(__aarch64__)
#define DEBUG_TRAP() __asm__ volatile("brk #0")
#else
#include <signal.h>
#define DEBUG_TRAP() raise(SIGTRAP)
#endif
#define DEBUG_ASSERT(cond)                                                     \
  do {                                                                         \
    if (!(cond)) {                                                             \
      DEBUG_TRAP();                                                            \
    }                                                                          \
  } while (0)
#else
#define DEBUG_TRAP() ((void)0)
#define DEBUG_ASSERT(cond) ((void)(cond))
#endif

/* ── GameButtonState ─────────────────────────────────────────────────────
 * Tracks one button across two frames.
 *
 * `ended_down`: current physical state of the button.
 * `half_transitions`: how many times it changed state this frame
 * (0 = held/untouched, 1 = pressed OR released, 2 = tapped,
 *  higher = held then tapped repeatedly within one frame).              */
typedef struct {
  int ended_down;       /* 1 if button is physically down at frame end.   */
  int half_transitions; /* Number of state changes this frame.            */
} GameButtonState;

/* Helper: was this button just pressed this frame?
 * True if it is now down AND had at least one half-transition.            */
static inline int button_just_pressed(GameButtonState button) {
  return button.ended_down && (button.half_transitions > 0);
}

/* Helper: was this button just released this frame? */
static inline int button_just_released(GameButtonState button) {
  return !button.ended_down && (button.half_transitions > 0);
}

/* ── GameInput ───────────────────────────────────────────────────────────
 * One input snapshot.  Two of these form the double buffer (curr / prev).
 *
 * The buttons union allows both named access (buttons.quit) and indexed
 * access (buttons.all[i]) for processing all buttons in a loop.
 *
 * GAME COURSES: extend this struct to add movement/action buttons.        */
typedef struct {
  union {
    GameButtonState all[3]; /* Indexed access for loops.                  */
    struct {
      GameButtonState quit;      /* Esc / close button.                   */
      GameButtonState play_tone; /* Space / A — triggers test tone demo.  */
      GameButtonState cycle_scale_mode; /* Tab — cycles render scale modes. */
    };
  } buttons;
} GameInput;

/* ── UPDATE_BUTTON ───────────────────────────────────────────────────────
 * Macro called by the platform event loop to record one button event.
 * `is_down_val` is 1 if the key is now pressed, 0 if released.
 *
 * `half_transitions` counts state changes — for a simple press+release in
 * the same frame we'd get 2 transitions.  Games usually only care about
 * ended_down or button_just_pressed(), but the count enables future replay. */
#define UPDATE_BUTTON(btn_ptr, is_down_val)                                    \
  do {                                                                         \
    GameButtonState *_btn = (btn_ptr);                                         \
    if (_btn->ended_down != (is_down_val)) {                                   \
      _btn->ended_down = (is_down_val);                                        \
      _btn->half_transitions++;                                                \
    }                                                                          \
  } while (0)

/* ── prepare_input_frame ─────────────────────────────────────────────────
 * Called at the START of each frame's event loop.
 * Carries forward the previous `ended_down` states but resets
 * `half_transitions` to 0 so each frame starts with a clean transition
 * count.
 *
 * JS analogy: like `Object.assign({}, prevInput, { halfTransitions: 0 })`. */
static inline void prepare_input_frame(GameInput *curr, const GameInput *prev) {
  int button_count =
      (int)(sizeof(curr->buttons.all) / sizeof(curr->buttons.all[0]));
  for (int i = 0; i < button_count; i++) {
    curr->buttons.all[i].ended_down = prev->buttons.all[i].ended_down;
    curr->buttons.all[i].half_transitions = 0;
  }
}

/* ── platform_swap_input_buffers ────────────────────────────────────────
 * Swaps current and previous input pointers at end of frame.
 * The next frame's "prev" is this frame's "curr".
 *
 * NOTE: This function is pure game-layer logic (only touches GameInput
 * structs, no OS calls).  It lives in game/base.h alongside
 * prepare_input_frame, NOT in platform.h.                                  */
static inline void platform_swap_input_buffers(GameInput **curr,
                                               GameInput **prev) {
  GameInput *tmp = *curr;
  *curr = *prev;
  *prev = tmp;
}

#endif // GAME_BASE_H