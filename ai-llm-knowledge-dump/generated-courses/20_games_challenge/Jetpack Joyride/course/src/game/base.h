#ifndef GAME_BASE_H
#define GAME_BASE_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/base.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Double-buffered input: GameButtonState, GameInput, UPDATE_BUTTON,
 * prepare_input_frame, platform_swap_input_buffers.
 *
 * Adapted from Platform Foundation Course — button names changed to
 * match this game's input needs.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>

/* ── Debug trap / assert ────────────────────────────────────────────────── */
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

/* ── GameButtonState ───────────────────────────────────────────────────── */
typedef struct {
  int ended_down;        /* 1 if button is physically down at frame end.   */
  int half_transitions;  /* Number of state changes this frame.            */
} GameButtonState;

static inline int button_just_pressed(GameButtonState button) {
  return button.ended_down && (button.half_transitions > 0);
}

static inline int button_just_released(GameButtonState button) {
  return !button.ended_down && (button.half_transitions > 0);
}

/* ── GameCamera ────────────────────────────────────────────────────────── */
typedef struct {
  float x;    /* world x of viewport left edge  */
  float y;    /* world y of viewport top edge (y-down) */
  float zoom; /* 1.0 = normal */
} GameCamera;

/* ── Button count ──────────────────────────────────────────────────────── */
#define GAME_BUTTON_COUNT 6

/* ── GameInput ──────────────────────────────────────────────────────────
 * Buttons for Jetpack Joyride:
 *   action  — SPACE / mouse click: fly (hold) / menu select
 *   quit    — ESC: close window / back to menu
 *   pause   — P: toggle pause
 *   restart — R: restart game
 *   left    — LEFT / A: menu navigation
 *   right   — RIGHT / D: menu navigation
 */
typedef struct {
  union {
    GameButtonState all[GAME_BUTTON_COUNT];
    struct {
      GameButtonState action;   /* SPACE / Click — fly / select         */
      GameButtonState quit;     /* ESC — close / back                   */
      GameButtonState pause;    /* P — toggle pause                     */
      GameButtonState restart;  /* R — restart game                     */
      GameButtonState left;     /* LEFT / A — menu navigation           */
      GameButtonState right;    /* RIGHT / D — menu navigation          */
    };
  } buttons;
  /* Mouse position in window coordinates (for menu interaction) */
  float mouse_x;
  float mouse_y;
  int mouse_down;  /* 1 if left mouse button is down */
} GameInput;

#define UPDATE_BUTTON(btn_ptr, is_down_val)                                  \
  do {                                                                        \
    GameButtonState *_btn = (btn_ptr);                                        \
    if (_btn->ended_down != (is_down_val)) {                                  \
      _btn->ended_down = (is_down_val);                                       \
      _btn->half_transitions++;                                                \
    }                                                                         \
  } while (0)

static inline void prepare_input_frame(GameInput *curr, const GameInput *prev) {
  for (int i = 0; i < GAME_BUTTON_COUNT; i++) {
    curr->buttons.all[i].ended_down       = prev->buttons.all[i].ended_down;
    curr->buttons.all[i].half_transitions = 0;
  }
  /* Carry forward mouse state */
  curr->mouse_x = prev->mouse_x;
  curr->mouse_y = prev->mouse_y;
  curr->mouse_down = 0;
}

static inline void platform_swap_input_buffers(GameInput **curr, GameInput **prev) {
  GameInput *tmp = *curr;
  *curr = *prev;
  *prev = tmp;
}

#endif /* GAME_BASE_H */
