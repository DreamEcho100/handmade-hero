#ifndef DE100_GAME_INPUT_H
#define DE100_GAME_INPUT_H

#include "../_common/memory.h"
#include "memory.h"

#define BASE_JOYSTICK_DEADZONE 0.4

#define MAX_CONTROLLER_COUNT 5
#define MAX_KEYBOARD_COUNT 1
#define MAX_JOYSTICK_COUNT (MAX_CONTROLLER_COUNT - MAX_KEYBOARD_COUNT)

extern int KEYBOARD_CONTROLLER_INDEX;

// ═══════════════════════════════════════════════════════════════
// TODO(Mazen) (Future Self - Engine Refactor):
// ═══════════════════════════════════════════════════════════════
// **Circular Include Dependency & Input Abstraction**
//
// Current Status (Day 16):
// ────────────────────────────────────────────────────────────────
// - Backends (X11, Raylib) directly translate physical inputs
//   (keys/buttons) to game-specific actions (move_up, jump, etc.)
// - This is the CORRECT approach for Handmade Hero Day 16!
// - Matches Casey's philosophy: "Solve problems you have, not
//   problems you might have."
//
// ```
// input.h needs GameInput struct
//     ↓
// GameInput defined in game.h
//     ↓
// game.h includes platform headers
//     ↓
// Platform headers include input.h
//     ↓
// 💀 CIRCULAR DEPENDENCY!
// ```
//
// Known Limitations (Acceptable for Now):
// ────────────────────────────────────────────────────────────────
// 1. **Backend Reuse:**
//    - Each backend is tightly coupled to this specific game
//    - To support multiple games, would need generic PhysicalKey
//      layer + per-game ActionBindings
//
// 2. **User Rebinding:**
//    - Hardcoded key → action mapping prevents runtime remapping
//    - Future: Introduce InputConfig table for user customization
//
// 3. **Multi-Game Support:**
//    - Switching games requires modifying backend code
//    - Future: Canonical input layer + game-specific binding tables
//
// 4. **Performance:**
//    - Current approach: Direct mapping (fastest possible!)
//    - Future: Consider cache-friendly binding arrays if needed
//
// Why NOT Fix This Now:
// ────────────────────────────────────────────────────────────────
// - Don't have multiple games to test abstraction against
// - Don't know what binding system users actually need
// - Premature abstraction would slow down learning
// - Current code is SIMPLE, READABLE, and WORKS
//
// When to Revisit:
// ────────────────────────────────────────────────────────────────
// ✅ When building a second game on this engine
// ✅ When implementing user-configurable controls
// ✅ When profiling reveals input processing bottleneck
// ✅ When you understand input systems MUCH better
//
// References for Future Self:
// ────────────────────────────────────────────────────────────────
// - Handmade Hero Episode 150+ (when Casey adds config system)
// - "Input Handling" chapter in Game Engine Architecture book
// - Your Day 16 implementation (this is your baseline!)
//
// Remember:
// ────────────────────────────────────────────────────────────────
// "Make it work, make it right, make it fast." - Kent Beck
// You're still on "make it work"! 🚀
// ═══════════════════════════════════════════════════════════════

#define CONTROLLER_DEADZONE 0

/**
 * 🎮 DAY 13: PLATFORM-INDEPENDENT INPUT ABSTRACTION
 * ═══════════════════════════════════════════════════════════════
 *
 * These structures replace ALL platform-specific input handling.
 * Game layer ONLY sees these - no X11 KeySym, no joystick events!
 *
 * Casey's Day 13 pattern: Abstract button state + analog sticks
 * ═══════════════════════════════════════════════════════════════
 *
 * BUTTON STATE (replaces raw bool flags)
 * ───────────────────────────────────────────────────────────────
 *
 * Tracks BOTH current state AND transitions (press/release events).
 *
 * Casey's pattern:
 *   EndedDown = final state this frame
 *   HalfTransitionCount = how many times it changed
 *
 * Examples:
 *   HalfTransitionCount=0, EndedDown=false → Button not pressed, no change
 *   HalfTransitionCount=1, EndedDown=true  → Button JUST pressed!
 *   HalfTransitionCount=0, EndedDown=true  → Button held down
 *   HalfTransitionCount=1, EndedDown=false → Button JUST released!
 *   HalfTransitionCount=2, EndedDown=true  → Pressed THEN released THEN pressed
 *   (same frame!)
 * ───────────────────────────────────────────────────────────────
 */
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
   *       // Might be lag, or input recording glitch
   *   }
   * ```
   */
  int half_transition_count;
  /** Final state (true = pressed, false = released) */
  bool32 ended_down;
} GameButtonState;

/**
 * CONTROLLER INPUT (replaces your GameControls struct)
 * ───────────────────────────────────────────────────────────────
 *
 * Platform-agnostic controller abstraction.
 * Works with:
 *   - Xbox controllers (via joystick API)
 *   - PlayStation controllers (via joystick API)
 *   - Keyboard (converted to analog values)
 *   - Future: Steam Deck, Switch Pro, etc.
 *
 * Casey's design: Analog sticks normalized to -1.0 to +1.0
 *
 * ────────────────────
 */
typedef struct {
  /**
   * Can access as:
   *   - Array: for(int i=0; i<6; i++) buttons[i]...
   *   - Named: if(controller->up.ended_down) ...
   *
   * SAME MEMORY, TWO ACCESS PATTERNS! ✨
   * ─────────────────────────────────────────────────────────────
   */
  union {
    GameButtonState buttons[12];
    struct {
      // MOVEMENT buttons (stick/WASD)
      GameButtonState move_up;    // ← Was "Up" in Day 16
      GameButtonState move_down;  // ← Was "Down"
      GameButtonState move_left;  // ← Was "Left"
      GameButtonState move_right; // ← Was "Right"

      // ACTION buttons (face buttons/arrows)
      GameButtonState action_up;    // ← Y button / Arrow Up
      GameButtonState action_down;  // ← A button / Arrow Down
      GameButtonState action_left;  // ← X button / Arrow Left
      GameButtonState action_right; // ← B button / Arrow Right

      // SHOULDER buttons (unchanged)
      GameButtonState left_shoulder;
      GameButtonState right_shoulder;

      // MENU buttons
      GameButtonState back;  // ← Select button / Space
      GameButtonState start; // ← Start button / Escape

      // NOTE: All buttons must be added above this line
      GameButtonState terminator; // ← Sentinel for validation
    };
  };
  // TODO: Maybe add alt for letting the game forward deff the buttons
  /*
  btns struct {
          size_t count;
          GameButtonState states[];
  }
  */

  real32 stick_avg_x; // ← Renamed from EndX (clearer name!)
  real32 stick_avg_y; // ← Renamed from EndY

  // id: useful for debugging multiple controllers and for joystick file
  // descriptor
  // int id;
  int controller_index; // Which controller slot (0-3)
  /** `true` = joystick, `false` = keyboard (digital) */
  bool32 is_analog;
  // int fd;               // File descriptor for /dev/input/jsX
  bool is_connected; // Is this controller active

} GameControllerInput;

/**
 * GAME INPUT (replaces your GameControls struct)
 * ───────────────────────────────────────────────────────────────
 *
 * Supports up to 4 controllers (local multiplayer ready!)
 *
 * Casey's pattern:
 *   Controllers[0] = Player 1
 *   Controllers[1] = Player 2 (future)
 *   Controllers[2] = Player 3 (future)
 *   Controllers[3] = Player 4 (future)
 *
 * ───────────────────────────────────────────────────────────────
 */
typedef struct game_input {
  GameControllerInput controllers[5];
} GameInput;

//
void prepare_input_frame(GameInput *old_input, GameInput *new_input);
void process_game_button_state(bool is_down, GameButtonState *new_state);

#endif // DE100_GAME_INPUT_H