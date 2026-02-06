#ifndef DE100_GAME_INPUTS_BASE_H
#define DE100_GAME_INPUTS_BASE_H

#include "../_common/base.h"

typedef struct {
  /**
   * Number of state changes this frame
   *
   * half_transition_count is NOT a boolean flag!
   *
   * It's a COUNTER that tracks:
   *   0 = No change this frame (button held or released)
   *   1 = Changed once (normal press or release)
   *   2 = Changed twice (press then release, or release then press)
   *   3 = Changed three times (press-release-press, extremely rare!)
   *
   * Why this matters:
   * ```c
   *   if (button.ended_down && button.half_transition_count > 0) {
   *       // Button JUST pressed (not held from last frame)
   *   }
   *
   *   if (button.half_transition_count > 1) {
   *       // User is mashing the button VERY fast!
   *       // Might be lag, or inputs recording glitch
   *   }
   * ```
   */
  int half_transition_count;
  /** Final state (true = pressed, false = released) */
  int ended_down;
} GameButtonState;

#endif // DE100_GAME_INPUTS_BASE_H
