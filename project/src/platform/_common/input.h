#ifndef HANDMADE_COMMON_INPUT_H
#define HANDMADE_COMMON_INPUT_H
#include "../../base.h"
#include "../../game.h"

#define BASE_JOYSTICK_DEADZONE 0.4

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


//
void prepare_input_frame(GameInput *old_input, GameInput *new_input);
void process_game_button_state(bool is_down, GameButtonState *new_state);

#endif // HANDMADE_COMMON_INPUT_H