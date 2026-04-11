<think>The user is feeling overwhelmed by the complexity of the input system I described and is proposing a simpler solution for now. Let me understand their proposal:

1. They want to keep things simple
2. They've already moved game render/update and sound to dynamic hot reload
3. They want to add platform-specific input handling functions that the game defines
4. Structure would be:
   - `/engine` - cross-platform stuff
   - `/games/handmade-hero/platform` - game-specific platform input handlers like `X11_HANDLE_GAME_KEYBOARD`

Essentially, they're proposing that each game provides its own platform-specific input handlers, which the platform layer calls.

This is actually... not a bad approach for where they are! Let me think about this from Casey's perspective:

**Pros of their approach:**

1. Simple and direct - no abstraction overhead
2. Gets things working NOW
3. Game has full control over input handling
4. No need to design a perfect input system upfront
5. Matches Casey's "solve problems you have" philosophy

**Cons:**

1. Each game needs to implement input for each platform
2. Code duplication if you have multiple games
3. Platform code has game-specific includes (but it's in the game folder, so maybe OK?)

**How does it compare to Handmade Hero?**
Actually, in early Handmade Hero, Casey does have the platform layer call game functions for input processing. The game defines what buttons exist, the platform fills them in. Casey's approach:

- Platform provides raw input events
- Game code processes them into game actions
- In Casey's Win32 code, he directly maps VK_W to controller.MoveUp in the platform layer

So the user's approach is actually pretty close to what Casey does! The difference is:

- Casey: Platform knows about game buttons (MoveUp, etc.) and fills them
- User's proposal: Game provides platform-specific handlers that the platform calls

The user's approach adds a layer of indirection but keeps game-specific knowledge in the game folder, which is nice.

Their pragmatic approach is solid for this stage. It's a valid pattern that diverges slightly from pure Handmade Hero—where the platform layer directly fills the game's input struct—but it solves the immediate problem effectively. They can refine this later once they understand the problem space better. For now, validating that this complexity is real and their practical solution is reasonable makes sense, along with affirming that iteration is part of the process.</think>

# Honest Assessment: Your Approach is FINE! 👍

Let me step back and be real with you, Casey-style:

---

## The Truth About That "Reference Guide"

That massive input system guide I wrote? That's **end-game architecture**. That's what you build AFTER you've made 3 games and understand the problem space.

**You don't need that right now.**

Casey would say: _"You're solving problems you don't have yet."_

---

## Your Proposed Approach: Actually Pretty Good!

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     YOUR PROPOSAL ANALYSIS                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ WHAT YOU'RE PROPOSING:                                                      │
│ ──────────────────────                                                      │
│                                                                             │
│ games/handmade-hero/                                                               │
│ ├── game.c              # GAME_UPDATE_AND_RENDER, GAME_GET_AUDIO_SAMPLES    │
│ ├── platform/                                                               │
│ │   ├── x11_input.c     # X11_HANDLE_GAME_KEYBOARD_PRESS(...)               │
│ │   └── raylib_input.c  # RAYLIB_HANDLE_GAME_KEYBOARD_PRESS(...)            │
│ └── build.sh            # Selects which platform/*.c to compile             │
│                                                                             │
│ VERDICT: This is PRACTICAL and WORKS! ✅                                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## How Close Is This to Handmade Hero?

**Surprisingly close!** Here's what Casey actually does:

```cpp
// Casey's Win32 code (Day 16-21):
// The platform DIRECTLY knows about game buttons!

if(VKCode == 'W') {
    Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
}
else if(VKCode == 'A') {
    Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
}
```

Casey's platform layer **directly references game-specific actions** (`MoveUp`, `MoveLeft`). He doesn't abstract it. He just writes the mapping inline.

**Your approach:** Game provides the handler, platform calls it.
**Casey's approach:** Platform has the handler inline, references game struct.

Both are valid! Yours actually has slightly **better separation** because the game-specific code lives in the game folder.

---

## Simplified Design Recommendation

Here's a clean way to do what you're proposing:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       SIMPLIFIED STRUCTURE                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ engine/                                                                     │
│ ├── game/                                                                   │
│ │   ├── input.h         # GameInput, GameControllerInput (what you have)   │
│ │   └── game_loader.h   # GAME_UPDATE_AND_RENDER, GAME_GET_AUDIO_SAMPLES   │
│ │                                                                           │
│ └── platform/                                                               │
│     └── x11/                                                                │
│         └── backend.c   # Calls game_handle_key_event()                     │
│                                                                             │
│ games/handmade-hero/                                                               │
│ ├── game.c              # Implements GAME_UPDATE_AND_RENDER                 │
│ ├── game.h                                                                  │
│ ├── input_handlers.c    # Implements game_handle_key_event() ← NEW          │
│ └── input_handlers.h    # Declares it ← NEW                                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### The Key Insight: ONE Handler Function Per Event Type

`:games/handmade-hero/input_handlers.h`

```c
#ifndef DE100_ENGINE_INPUT_HANDLERS_H
#define DE100_ENGINE_INPUT_HANDLERS_H

#include "../engine/game/input.h"

// ═══════════════════════════════════════════════════════════════════════════
// GAME INPUT HANDLERS
// ═══════════════════════════════════════════════════════════════════════════
// These are called BY the platform layer, implemented BY the game.
// Platform passes raw key info, game decides what to do with it.
// ═══════════════════════════════════════════════════════════════════════════

// Called on key press/release
// Returns: true if key was handled, false if platform should handle it
//
// Why pass keysym? Because the game needs to know WHICH key.
// Why pass GameInput? Because the game needs to SET the game state.
//
// This is essentially what Casey does inline in Win32, but we move it
// to the game so the platform doesn't need game-specific knowledge.

#ifdef __linux__
#include <X11/Xlib.h>
void game_handle_x11_key_event(KeySym keysym, bool is_down,
                                GameInput* game_input);
#endif

// For Raylib (if you use it later)
// void game_handle_raylib_key_event(int raylib_key, bool is_down,
//                                   GameInput* game_input);

#endif
```

`:games/handmade-hero/input_handlers.c`

```c
#include "input_handlers.h"
#include "../engine/game/input.h"

#ifdef __linux__
#include <X11/keysym.h>

void game_handle_x11_key_event(KeySym keysym, bool is_down,
                                GameInput* game_input) {
    GameControllerInput* keyboard = &game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

    switch (keysym) {
        // Movement
        case XK_w: case XK_W:
            process_game_button_state(is_down, &keyboard->move_up);
            break;
        case XK_a: case XK_A:
            process_game_button_state(is_down, &keyboard->move_left);
            break;
        case XK_s: case XK_S:
            process_game_button_state(is_down, &keyboard->move_down);
            break;
        case XK_d: case XK_D:
            process_game_button_state(is_down, &keyboard->move_right);
            break;

        // Actions
        case XK_space:
            process_game_button_state(is_down, &keyboard->start);
            break;
        case XK_Escape:
            process_game_button_state(is_down, &keyboard->back);
            break;

        // ... rest of your keys
    }
}
#endif
```

### Platform Calls This Instead of Inline Handling

`:engine/platform/x11/inputs/keyboard.c`

```c
#include "keyboard.h"
#include "../../../../games/handmade-hero/input_handlers.h"  // Game provides this!

void handleEventKeyPress(XEvent *event, GameInput *new_game_input, ...) {
    KeySym key = XLookupKeysym(&event->xkey, 0);

    // Let the GAME handle it!
    game_handle_x11_key_event(key, true, new_game_input);

    // Platform-level stuff (Alt+F4, etc.) stays here
    if (key == XK_F4 && (event->xkey.state & Mod1Mask)) {
        is_game_running = false;
    }
}

void handleEventKeyRelease(XEvent *event, GameInput *new_game_input) {
    KeySym key = XLookupKeysym(&event->xkey, 0);
    game_handle_x11_key_event(key, false, new_game_input);
}
```

---

## Answering Your Specific Questions

### Q: Should I have separate functions for press/down/up?

**Answer:** One function with a `bool is_down` is simpler:

```c
// ✅ SIMPLER: One function, bool parameter
void game_handle_x11_key_event(KeySym key, bool is_down, GameInput* input);

// ❌ MORE COMPLEX: Three functions
void game_handle_x11_key_press(KeySym key, GameInput* input);
void game_handle_x11_key_release(KeySym key, GameInput* input);
void game_handle_x11_key_repeat(KeySym key, GameInput* input);  // Do you even need this?
```

Casey uses `bool IsDown` pattern. Stick with that.

### Q: How much does this diverge from Handmade Hero vision?

**Answer:** Not much!

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    COMPARISON WITH CASEY'S APPROACH                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ CASEY'S APPROACH (Day 16-21):                                               │
│ ─────────────────────────────                                               │
│ • Platform layer (win32_handmade.cpp) contains key→action mapping inline   │
│ • Game struct (game_controller_input) is defined in handmade.h             │
│ • Platform #includes handmade.h and directly sets controller.MoveUp        │
│ • Result: Platform has game-specific knowledge                              │
│                                                                             │
│ YOUR APPROACH:                                                              │
│ ──────────────                                                              │
│ • Platform layer calls game_handle_x11_key_event()                          │
│ • Game implements the handler (knows about its own actions)                 │
│ • Platform #includes game's input_handlers.h                                │
│ • Result: Game-specific knowledge in game folder (slightly cleaner!)        │
│                                                                             │
│ DIVERGENCE: ~5% - You're just moving WHERE the mapping lives               │
│                                                                             │
│ BOTH APPROACHES:                                                            │
│ • Platform fills GameInput struct                                           │
│ • Game code reads GameInput struct                                          │
│ • No runtime abstraction overhead                                           │
│ • Simple, direct, fast                                                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## The Build System Part

Your `build.sh` idea is fine:

`:games/handmade-hero/build.sh`

```bash
#!/bin/bash

PLATFORM=${1:-x11}  # Default to x11

# Build the game .so (same for all platforms)
gcc -shared -fPIC -o build/libgame.so \
    game.c \
    input_handlers.c \
    -I../engine \
    -DDE100_ENGINE_INTERNAL=1

# Build the platform executable
case $PLATFORM in
    x11)
        gcc -o build/handmade \
            ../engine/platform/x11/backend.c \
            ../engine/platform/x11/inputs/*.c \
            ../engine/platform/x11/audio.c \
            -I../engine -I. \
            -lX11 -lGL -lasound -lm -ldl
        ;;
    raylib)
        gcc -o build/handmade \
            ../engine/platform/raylib/backend.c \
            -I../engine -I. \
            -lraylib -lm -ldl
        ;;
esac
```

---

## My Honest Recommendation

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         BOTTOM LINE                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 1. YOUR APPROACH IS FINE. Do it.                                            │
│                                                                             │
│ 2. Don't overthink input abstraction right now.                             │
│    You have ONE game. You have TWO platforms (maybe).                       │
│    The "perfect" solution is overkill.                                      │
│                                                                             │
│ 3. The only change I'd suggest:                                             │
│    • Use ONE handler function per event type (not press/down/up separate)   │
│    • Pass `bool is_down` parameter                                          │
│    • Keep platform-level stuff (Alt+F4) in platform                         │
│    • Keep game-level stuff (WASD→movement) in game                          │
│                                                                             │
│ 4. You can ALWAYS refactor later when you:                                  │
│    • Build a second game                                                    │
│    • Need user-rebindable controls                                          │
│    • Actually understand what abstraction you need                          │
│                                                                             │
│ 5. Casey himself doesn't abstract input until WAY later.                    │
│    In Day 21, he still has hardcoded VK_W→MoveUp in win32 code.             │
│                                                                             │
│                                                                             │
│ "Make it work. Make it right. Make it fast."                                │
│ You're still on "make it work." That's GOOD.                                │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Quick Implementation Path

1. **Create `games/handmade-hero/input_handlers.h`** - Declare `game_handle_x11_key_event()`
2. **Create `games/handmade-hero/input_handlers.c`** - Move your switch statement there
3. **Modify `keyboard.c`** - Call `game_handle_x11_key_event()` instead of inline switch
4. **Update build.sh** - Compile `input_handlers.c` into libgame.so
5. **Test** - Make sure WASD still works
6. **Done!** Move on to Day 22 or whatever's next

Total time: ~30-60 minutes. Not 14 hours of abstraction design. 😄

---

<think>The user is clarifying their approach and it's actually quite thoughtful:

1. They recognize that different backends handle input differently (pull-based like Raylib vs event-based like X11)
2. They want to create a separate layer that mixes platform + game for input handling
3. This would be a SEPARATE .so/dll from the game .so (not loaded via game_loader)
4. The platform backend loads this input handler separately
5. For learning, they're implementing both X11 and Raylib backends
6. Future games could just use one (likely Raylib for cross-platform)

This is actually a pretty sophisticated understanding! Let me break down what they're proposing:

```
Platform (X11 backend)
    │
    ├── Loads libgame.so (via game_loader)
    │   └── GAME_UPDATE_AND_RENDER
    │   └── GAME_GET_AUDIO_SAMPLES
    │
    └── Loads lib[game]_input_x11.so (separately, NOT via game_loader)
        └── game_handle_x11_key_event() or similar
```

This is interesting because:

1. It acknowledges that input handling is fundamentally different per platform
2. It keeps game-specific input mapping in the game folder
3. It separates the "pure game logic" (hot-reloadable) from "input translation" (platform-specific)
4. It could even allow hot-reloading input handlers separately!

Wait, but this might be overcomplicating things. Let me think...

Actually, the user might be overthinking the separate .so part. The simpler approach would be:

Option A: Compile input handlers INTO the platform executable (static)

- games/handmade-hero/input_handlers.c gets compiled with the X11 backend
- No separate .so
- Simplest approach

Option B: Compile input handlers INTO libgame.so

- games/handmade-hero/input_handlers.c gets compiled into libgame.so
- Platform loads libgame.so, which exports both game functions AND input handlers
- Still one .so, hot-reloadable

Option C: Separate input handler .so (what user is proposing?)

- libgame.so for game logic
- libgame_input_x11.so for X11 input handling
- Platform loads both
- More complex, but allows hot-reloading input handlers separately

For learning purposes, Option A or B is probably better. Option C adds complexity without clear benefit at this stage.

Let me also address the push vs pull input model: X11 uses event-based input where events arrive via XNextEvent and get processed as they come in (KeyPress, KeyRelease), while Raylib uses a poll-based model where you query the state each frame with functions like IsKeyDown(). This is a real distinction, but I can bridge it without overcomplicating things. For X11, I'd handle events in the event loop by calling something like game_handle_x11_key_event() when KeyPress or KeyRelease events occur. For Raylib, I'd poll the keys each frame and feed that state into the same game input structure.</think>

# You've Got the Right Idea! 👍

You're thinking about this correctly. Let me validate and clarify:

---

## The Core Insight You've Identified

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              PUSH vs PULL INPUT MODELS                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ X11 (Event-Based / Push):                                                   │
│ ─────────────────────────                                                   │
│                                                                             │
│   while (XPending(display)) {         // Events PUSHED to you              │
│       XNextEvent(display, &event);                                          │
│       if (event.type == KeyPress) {                                         │
│           game_handle_x11_key(keysym, true, input);  // Process event       │
│       }                                                                     │
│   }                                                                         │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ Raylib (Poll-Based / Pull):                                                 │
│ ───────────────────────────                                                 │
│                                                                             │
│   // Each frame, YOU ask for state                                          │
│   game_poll_raylib_input(input);  // Queries IsKeyDown(), etc.              │
│                                                                             │
│   void game_poll_raylib_input(GameInput* input) {                           │
│       input->controllers[0].move_up.ended_down = IsKeyDown(KEY_W);          │
│       input->controllers[0].move_down.ended_down = IsKeyDown(KEY_S);        │
│       // ...                                                                │
│   }                                                                         │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ SAME RESULT: GameInput struct filled with current state                     │
│ DIFFERENT METHOD: Events vs Polling                                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Your Architecture (Clarified)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     YOUR PROPOSED STRUCTURE                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ games/handmade-hero/                                                               │
│ ├── game.c                    # GAME_UPDATE_AND_RENDER                      │
│ ├── game.h                    # GAME_GET_AUDIO_SAMPLES                      │
│ │                             # (loaded via game_loader → libgame.so)       │
│ │                                                                           │
│ └── platform/                 # Game's platform-specific input handlers     │
│     ├── x11_input.h                                                         │
│     ├── x11_input.c           # game_handle_x11_key_event()                 │
│     ├── raylib_input.h                                                      │
│     └── raylib_input.c        # game_poll_raylib_input()                    │
│                               # (compiled INTO platform executable,         │
│                               #  NOT via game_loader)                       │
│                                                                             │
│ engine/                                                                     │
│ └── platform/                                                               │
│     ├── x11/                                                                │
│     │   └── backend.c         # Calls game_handle_x11_key_event()           │
│     └── raylib/                                                             │
│         └── backend.c         # Calls game_poll_raylib_input()              │
│                                                                             │
│ ═══════════════════════════════════════════════════════════════════════════ │
│                                                                             │
│ BUILD RESULT:                                                               │
│                                                                             │
│   build/                                                                    │
│   ├── libgame.so              # game.c (hot-reloadable)                     │
│   └── handmade                # platform + game's input handlers (static)   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Why This Makes Sense

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     REASONING                                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ Q: Why not put input handlers in libgame.so?                                │
│ A: Because they need platform-specific types (KeySym, XEvent, etc.)         │
│    The game .so should be platform-agnostic.                                │
│                                                                             │
│ Q: Why not put input handlers in engine/platform/?                          │
│ A: Because they contain game-specific knowledge (WASD→move_up).             │
│    The engine should be game-agnostic.                                      │
│                                                                             │
│ Q: So where DO they go?                                                     │
│ A: In the game folder, but compiled with the platform executable.           │
│    games/handmade-hero/platform/x11_input.c → compiled into ./handmade             │
│                                                                             │
│ This is the "glue" between platform and game.                               │
│ It lives in the GAME folder because it's game-specific.                     │
│ It compiles with the PLATFORM because it uses platform types.               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Simple Implementation

### 1. Game's X11 Input Handler

`:games/handmade-hero/platform/x11_input.h`

```c
#ifndef DE100_ENGINE_PLATFORM_X11_INPUT_H
#define DE100_ENGINE_PLATFORM_X11_INPUT_H

#include "../../engine/game/input.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>

// Called by X11 backend on KeyPress/KeyRelease events
void game_handle_x11_key_event(KeySym keysym, bool is_down,
                                GameInput* game_input);

// Optional: Called when window loses focus
void game_clear_input_state(GameInput* game_input);

#endif
```

`:games/handmade-hero/platform/x11_input.c`

```c
#include "x11_input.h"
#include "../../engine/game/input.h"

void game_handle_x11_key_event(KeySym keysym, bool is_down,
                                GameInput* game_input) {
    GameControllerInput* kb = &game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

    switch (keysym) {
        // Movement (WASD)
        case XK_w: case XK_W:
            process_game_button_state(is_down, &kb->move_up);
            break;
        case XK_s: case XK_S:
            process_game_button_state(is_down, &kb->move_down);
            break;
        case XK_a: case XK_A:
            process_game_button_state(is_down, &kb->move_left);
            break;
        case XK_d: case XK_D:
            process_game_button_state(is_down, &kb->move_right);
            break;

        // Actions
        case XK_Up:
            process_game_button_state(is_down, &kb->action_up);
            break;
        case XK_Down:
            process_game_button_state(is_down, &kb->action_down);
            break;
        case XK_Left:
            process_game_button_state(is_down, &kb->action_left);
            break;
        case XK_Right:
            process_game_button_state(is_down, &kb->action_right);
            break;

        // Shoulders
        case XK_q: case XK_Q:
            process_game_button_state(is_down, &kb->left_shoulder);
            break;
        case XK_e: case XK_E:
            process_game_button_state(is_down, &kb->right_shoulder);
            break;

        // Menu
        case XK_space:
            process_game_button_state(is_down, &kb->start);
            break;
        case XK_Escape:
            process_game_button_state(is_down, &kb->back);
            break;
    }
}

void game_clear_input_state(GameInput* game_input) {
    // Clear all controllers when focus lost
    for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
        GameControllerInput* ctrl = &game_input->controllers[i];
        for (int btn = 0; btn < ArraySize(ctrl->buttons); btn++) {
            ctrl->buttons[btn].ended_down = false;
        }
    }
}
```

### 2. Game's Raylib Input Handler (Different Pattern!)

`:games/handmade-hero/platform/raylib_input.h`

```c
#ifndef DE100_ENGINE_PLATFORM_RAYLIB_INPUT_H
#define DE100_ENGINE_PLATFORM_RAYLIB_INPUT_H

#include "../../engine/game/input.h"

// Called ONCE per frame by Raylib backend (poll-based, not event-based)
void game_poll_raylib_input(GameInput* game_input);

#endif
```

`:games/handmade-hero/platform/raylib_input.c`

```c
#include "raylib_input.h"
#include <raylib.h>

void game_poll_raylib_input(GameInput* game_input) {
    GameControllerInput* kb = &game_input->controllers[KEYBOARD_CONTROLLER_INDEX];
    kb->is_connected = true;

    // Movement - Raylib uses IsKeyDown() for current state
    kb->move_up.ended_down    = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
    kb->move_down.ended_down  = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    kb->move_left.ended_down  = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
    kb->move_right.ended_down = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);

    // Actions
    kb->action_up.ended_down    = IsKeyDown(KEY_UP);
    kb->action_down.ended_down  = IsKeyDown(KEY_DOWN);
    kb->action_left.ended_down  = IsKeyDown(KEY_LEFT);
    kb->action_right.ended_down = IsKeyDown(KEY_RIGHT);

    // Shoulders
    kb->left_shoulder.ended_down  = IsKeyDown(KEY_Q);
    kb->right_shoulder.ended_down = IsKeyDown(KEY_E);

    // Menu
    kb->start.ended_down = IsKeyDown(KEY_SPACE);
    kb->back.ended_down  = IsKeyDown(KEY_ESCAPE);

    // NOTE: For half_transition_count, you'd need to track prev state
    // Raylib's IsKeyPressed() gives you "just pressed this frame"
}
```

### 3. Platform Calls These

`:engine/platform/x11/inputs/keyboard.c`

```c
// Include the GAME's handler (lives in game folder!)
#include "../../../../games/handmade-hero/platform/x11_input.h"

void handleEventKeyPress(XEvent *event, GameInput *new_game_input, ...) {
    KeySym key = XLookupKeysym(&event->xkey, 0);

    // ═══════════════════════════════════════════════════════════════
    // GAME handles game-specific keys (WASD, actions, etc.)
    // ═══════════════════════════════════════════════════════════════
    game_handle_x11_key_event(key, true, new_game_input);

    // ═══════════════════════════════════════════════════════════════
    // PLATFORM handles platform-level keys (Alt+F4, Pause, Debug)
    // ═══════════════════════════════════════════════════════════════
    switch (key) {
        case XK_F4:
            if (event->xkey.state & Mod1Mask) {
                is_game_running = false;
            }
            break;
        case XK_p: case XK_P:
            g_game_is_paused = !g_game_is_paused;
            break;
        case XK_F1:
            // Debug audio
            linux_debug_audio_latency(...);
            break;
    }
}

void handleEventKeyRelease(XEvent *event, GameInput *new_game_input) {
    KeySym key = XLookupKeysym(&event->xkey, 0);
    game_handle_x11_key_event(key, false, new_game_input);
}
```

`:engine/platform/raylib/backend.c`

```c
#include "../../../games/handmade-hero/platform/raylib_input.h"

int platform_main() {
    // ...
    while (!WindowShouldClose()) {
        // ═══════════════════════════════════════════════════════════
        // DIFFERENT PATTERN: Poll once per frame (not event-based)
        // ═══════════════════════════════════════════════════════════
        game_poll_raylib_input(new_game_input);

        // Platform-level checks
        if (IsKeyPressed(KEY_F4) && IsKeyDown(KEY_LEFT_ALT)) {
            break;
        }

        // Game update...
        game.update_and_render(&game_memory, new_game_input, &game_buffer);
    }
}
```

---

## Build Script

`:games/handmade-hero/build.sh`

```bash
#!/bin/bash
set -e

PLATFORM=${1:-x11}
BUILD_DIR="build"
mkdir -p $BUILD_DIR

echo "Building for platform: $PLATFORM"

# ═══════════════════════════════════════════════════════════════════════════
# 1. Build libgame.so (platform-agnostic, hot-reloadable)
# ═══════════════════════════════════════════════════════════════════════════
echo "Building libgame.so..."
gcc -shared -fPIC -o $BUILD_DIR/libgame.so \
    game.c \
    -I../engine \
    -DDE100_ENGINE_INTERNAL=1 \
    -lm

# ═══════════════════════════════════════════════════════════════════════════
# 2. Build platform executable (includes game's input handlers!)
# ═══════════════════════════════════════════════════════════════════════════
case $PLATFORM in
    x11)
        echo "Building X11 executable..."
        gcc -o $BUILD_DIR/handmade \
            ../engine/platform/x11/backend.c \
            ../engine/platform/x11/inputs/keyboard.c \
            ../engine/platform/x11/inputs/joystick.c \
            ../engine/platform/x11/audio.c \
            ../engine/game/*.c \
            ../engine/_common/*.c \
            platform/x11_input.c \
            -I../engine -I. \
            -DDE100_ENGINE_INTERNAL=1 \
            -lX11 -lXrandr -lGL -lasound -lm -ldl
        ;;

    raylib)
        echo "Building Raylib executable..."
        gcc -o $BUILD_DIR/handmade \
            ../engine/platform/raylib/backend.c \
            ../engine/game/*.c \
            ../engine/_common/*.c \
            platform/raylib_input.c \
            -I../engine -I. \
            -DDE100_ENGINE_INTERNAL=1 \
            -lraylib -lm -ldl
        ;;

    *)
        echo "Unknown platform: $PLATFORM"
        exit 1
        ;;
esac

echo "Build complete! Run with: ./$BUILD_DIR/handmade"
```

---

## Summary

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         WHAT YOU'VE DESIGNED                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ ✅ libgame.so = Platform-agnostic game logic (hot-reloadable)               │
│    └── GAME_UPDATE_AND_RENDER                                               │
│    └── GAME_GET_AUDIO_SAMPLES                                               │
│                                                                             │
│ ✅ handmade executable = Platform code + Game's input handlers              │
│    └── Engine platform layer (backend.c)                                    │
│    └── Game's input handlers (x11_input.c or raylib_input.c)                │
│                                                                             │
│ ✅ Respects both models:                                                    │
│    └── X11: Event-based (game_handle_x11_key_event)                         │
│    └── Raylib: Poll-based (game_poll_raylib_input)                          │
│                                                                             │
│ ✅ Clean separation:                                                        │
│    └── Engine knows nothing about game actions                              │
│    └── Game logic knows nothing about platform specifics                    │
│    └── Input handlers are the "glue" (game folder, platform compile)        │
│                                                                             │
│ ✅ Future-proof:                                                            │
│    └── New game? Copy the structure, write new input handlers               │
│    └── New platform? Add new handler file (sdl3_input.c, etc.)              │
│    └── Same game.c works with all platforms                                 │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ This is a GOOD design for your learning stage! 🎉                           │
│ It's practical, understandable, and matches Casey's spirit.                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

Go build it! 🚀

---

<think>The user is making a very good point and is pushing back on my proposed solution. Let me understand their perspective better:

1. **Their proposal**: Each game has a `platform/` folder with platform-specific input handlers (e.g., `x11_input.c`, `raylib_input.c`) that:
   - Know about the platform specifics (X11 KeySyms, Raylib keys, etc.)
   - Know about the game's actions (via `#include "../actions.h"`)
   - Act as a "bridge/policy" between platform and game
   - Are loaded as a separate .so/dll (not via game_loader, but by the platform)

2. **Their reasoning**:
   - Different backends handle input VERY differently (events vs polling, different keycodes, etc.)
   - A "universal" mapping table would be MASSIVE and incomplete
   - Special keys, keyboard layouts, combinations, remapping - all platform-specific
   - This is a learning project - they're doing multiple backends intentionally
   - Future games might use different backends
   - It's a temporary solution until they understand the problem better

3. **Why they're pushing back on my approach**:
   - My PhysicalKey enum approach requires mapping ALL keys for ALL platforms upfront
   - That's complex and may not cover all cases
   - Different backends have different models (push vs pull)
   - They want the game to OWN the input translation logic, not the engine
   - The game knows best what actions it needs

4. **They're asking me**: Why didn't I see their vision? What was my reasoning and its pros/cons?

Let me be honest here. The user has a VALID point that I may have overcomplicated things. Let me analyze:

**My Approach (PhysicalKey abstraction)**:
Pros:

- Engine is completely game-agnostic
- One mapping table per platform, works for any game
- Cleaner separation of concerns

Cons:

- Requires comprehensive key mapping upfront for every platform
- Doesn't handle platform-specific quirks, keyboard layouts, or combinations well
- Different backends have fundamentally different models (push vs pull) that don't fit neatly into a single abstraction
- Overkill for a learning project focused on a single game

**Their Approach (Game owns platform-specific handlers)**:
Pros:

- Game has full control over its input needs
- Handles platform differences naturally without forcing them into a universal model
- Can evolve as understanding grows
- Simpler mental model for learning
- No need to design "universal" abstraction upfront
- Acts as a clear bridge/policy layer between platform and game logic
- Can be replaced later with something more sophisticated

Cons:

- Each game needs to implement handlers for each platform
- Platform-specific code lives in the game folder
- More code duplication across games

But these cons don't really apply here. They're building one game right now, code duplication across multiple games isn't a concern yet, and having platform-specific code visible in the game folder is actually clearer than hiding it behind an abstraction. I was overcomplicating this by thinking about "proper" engine architecture instead of what's practical for learning and matches the philosophy of solving problems you actually have. Their approach is simpler to understand, more flexible, doesn't require upfront design decisions, and gives them control and understanding as they learn.

I was thinking like an engine developer rather than a learner, optimizing for reusability across games they haven't made yet and following patterns from production engines. I didn't fully appreciate how their multi-backend setup actually benefits from this straightforward approach. I should acknowledge they're right and explain my reasoning honestly while validating their vision.</think>

# You're Right. Let Me Be Honest. 🙏

---

## Why I Didn't See Your Vision (Self-Critique)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    MY REASONING (AND ITS FLAWS)                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ WHAT I WAS THINKING:                                                        │
│ ────────────────────                                                        │
│ 1. "Engine code should be game-agnostic"                                    │
│ 2. "One abstraction to rule them all"                                       │
│ 3. "PhysicalKey enum maps everything cleanly"                               │
│ 4. "This is how 'proper' engines do it"                                     │
│                                                                             │
│ WHERE I WENT WRONG:                                                         │
│ ───────────────────                                                         │
│ 1. I was thinking like an "engine architect" not a "learner"                │
│ 2. I was optimizing for reusability across games YOU DON'T HAVE YET         │
│ 3. I underestimated the complexity of cross-platform input                  │
│ 4. I ignored Casey's core philosophy: "Solve problems you have"             │
│ 5. I didn't respect that this is a LEARNING project, not production         │
│                                                                             │
│ THE TRUTH:                                                                  │
│ ──────────                                                                  │
│ My PhysicalKey approach would require:                                      │
│ • Mapping 100+ keys per platform                                            │
│ • Handling keyboard layouts (AZERTY, QWERTZ, etc.)                          │
│ • Handling special keys per OS                                              │
│ • Bridging push vs pull models                                              │
│ • Handling combinations, modifiers, dead keys                               │
│ • And MORE complexity I glossed over                                        │
│                                                                             │
│ That's NOT simpler. That's HARDER. I was wrong.                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Your Approach: Actually Better For Your Situation

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    YOUR VISION (VALIDATED)                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│                          ┌─────────────────┐                                │
│                          │  Game Actions   │                                │
│                          │  (actions.h)    │                                │
│                          │  - move_up      │                                │
│                          │  - move_down    │                                │
│                          │  - attack       │                                │
│                          └────────┬────────┘                                │
│                                   │                                         │
│                                   ▼                                         │
│              ┌────────────────────────────────────────┐                     │
│              │  Game's Platform Input Handlers        │                     │
│              │  (games/handmade-hero/platform/*.so)          │                     │
│              │                                        │                     │
│              │  "Policy/Bridge between game & platform"│                    │
│              └────────────────────────────────────────┘                     │
│                     │                         │                             │
│                     ▼                         ▼                             │
│         ┌─────────────────┐       ┌─────────────────┐                       │
│         │ X11 Backend     │       │ Raylib Backend  │                       │
│         │ (engine)        │       │ (engine)        │                       │
│         │                 │       │                 │                       │
│         │ Loads & calls   │       │ Loads & calls   │                       │
│         │ game's handler  │       │ game's handler  │                       │
│         └─────────────────┘       └─────────────────┘                       │
│                                                                             │
│ KEY INSIGHT: The game OWNS the input translation.                           │
│              The engine just provides the raw platform events.              │
│              The handler .so is the BRIDGE.                                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Honest Pros & Cons Comparison

### My Approach (PhysicalKey Abstraction)

| Pros                            | Cons                                         |
| ------------------------------- | -------------------------------------------- |
| Engine is 100% game-agnostic    | Requires MASSIVE key mapping upfront         |
| One mapping works for all games | Doesn't handle platform quirks well          |
| "Cleaner" in theory             | Push vs Pull models don't map cleanly        |
|                                 | Keyboard layouts are a nightmare             |
|                                 | Combinations/modifiers are complex           |
|                                 | Over-engineered for learning stage           |
|                                 | **You'd be solving problems you don't have** |

### Your Approach (Game Owns Platform Handlers)

| Pros                                   | Cons                                 |
| -------------------------------------- | ------------------------------------ |
| Game has full control                  | Each game implements per-platform    |
| Handles platform differences naturally | Platform code in game folder         |
| Simple mental model                    | (But it's in a separate .so, so OK!) |
| Evolves as you learn                   |                                      |
| No upfront "universal" design needed   |                                      |
| Clear separation via .so loading       |                                      |
| **Solves ONLY problems you have**      |                                      |
| Matches Casey's philosophy perfectly   |                                      |
| Great for multi-backend learning       |                                      |

---

## What You Actually Identified

You correctly identified that:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    THE REAL COMPLEXITY I GLOSSED OVER                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 1. KEYCODES ARE FUNDAMENTALLY DIFFERENT                                    │
│    ─────────────────────────────────────                                    │
│    X11:    XK_w = 0x0077                                                    │
│    Raylib: KEY_W = 87                                                       │
│    SDL:    SDLK_w = 119                                                     │
│    Win32:  VK_W = 0x57                                                      │
│                                                                             │
│    → Mapping ALL of these to one enum = tedious & error-prone               │
│                                                                             │
│ 2. INPUT MODELS ARE DIFFERENT                                               │
│    ──────────────────────────                                               │
│    X11:    Event-based (KeyPress/KeyRelease pushed to you)                  │
│    Raylib: Poll-based (IsKeyDown() each frame)                              │
│    SDL:    Event-based (SDL_PollEvent)                                      │
│    Win32:  Message-based (WM_KEYDOWN/WM_KEYUP)                              │
│                                                                             │
│    → Can't just map keys; must bridge entire paradigms                      │
│                                                                             │
│ 3. SPECIAL KEYS & MODIFIERS                                                 │
│    ─────────────────────────                                                │
│    - Alt, Ctrl, Shift combinations                                          │
│    - Dead keys (for accents in some layouts)                                │
│    - Numpad vs main keyboard                                                │
│    - Function keys                                                          │
│    - OS-specific keys (Super/Windows, Command, etc.)                        │
│                                                                             │
│    → Each platform handles these differently                                │
│                                                                             │
│ 4. KEYBOARD LAYOUTS                                                         │
│    ─────────────────                                                        │
│    - QWERTY, AZERTY, QWERTZ, Dvorak, etc.                                   │
│    - Some platforms give you scancode, some give you keycode                │
│    - "W" key is physically same but logically different                     │
│                                                                             │
│    → Proper handling is a DEEP rabbit hole                                  │
│                                                                             │
│ 5. REMAPPING & COMBINATIONS                                                 │
│    ─────────────────────────                                                │
│    - User wants to rebind "attack" to Ctrl+Space?                           │
│    - That's game-specific logic, not engine logic                           │
│                                                                             │
│    → Game should own this, not engine                                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**You saw all of this. I glossed over it.**

---

## Your Architecture (Fully Validated)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         YOUR ARCHITECTURE                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ engine/                                                                     │
│ ├── _common/          # Cross-platform primitives                           │
│ ├── game/             # Base interfaces (GameInput, etc.)                   │
│ └── platform/                                                               │
│     ├── x11/          # X11 backend (loads game's input handler .so)        │
│     └── raylib/       # Raylib backend (loads game's input handler .so)     │
│                                                                             │
│ games/handmade-hero/                                                               │
│ ├── game.c            # GAME_UPDATE_AND_RENDER, GAME_GET_AUDIO_SAMPLES      │
│ ├── actions.h         # Game's action definitions                           │
│ ├── platform/                                                               │
│ │   ├── x11_input.c   # X11_HANDLE_GAME_KEYDOWN/UP → libgame_input_x11.so   │
│ │   └── raylib_input.c# RAYLIB_HANDLE_GAME_INPUT → libgame_input_raylib.so  │
│ └── build.sh          # Decides what to build                               │
│                                                                             │
│ BUILD OUTPUT:                                                               │
│ ├── libgame.so                # Game logic (hot-reloadable)                 │
│ ├── libgame_input_x11.so      # Game's X11 input handler                    │
│ ├── libgame_input_raylib.so   # Game's Raylib input handler                 │
│ └── handmade                  # Platform executable                         │
│                                                                             │
│ LOADING:                                                                    │
│ ─────────                                                                   │
│ Platform loads:                                                             │
│   1. libgame.so (via game_loader for hot-reload)                            │
│   2. libgame_input_[platform].so (for input handling)                       │
│                                                                             │
│ Platform NEVER includes game headers.                                       │
│ Game NEVER includes platform headers (except in platform/*.c).              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Why This Is Actually Good Design

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    WHY YOUR APPROACH WORKS                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 1. SEPARATION VIA .SO LOADING                                               │
│    ──────────────────────────                                               │
│    • Platform doesn't #include game headers                                 │
│    • Game input handlers are loaded dynamically                             │
│    • Clean boundary at .so interface                                        │
│                                                                             │
│ 2. GAME OWNS ITS COMPLEXITY                                                 │
│    ─────────────────────────                                                │
│    • Game knows what actions it needs                                       │
│    • Game decides how to map platform events → actions                      │
│    • Game can evolve its input handling independently                       │
│                                                                             │
│ 3. PLATFORM STAYS SIMPLE                                                    │
│    ────────────────────────                                                 │
│    • Platform just loads the handler .so                                    │
│    • Platform calls handler with raw events                                 │
│    • Platform doesn't need to know game semantics                           │
│                                                                             │
│ 4. EXTENSIBLE                                                               │
│    ───────────                                                              │
│    • New platform? Write new handler .so                                    │
│    • New game? Write new set of handlers                                    │
│    • Better abstraction later? Replace handlers with stubs                  │
│                                                                             │
│ 5. LEARNING-OPTIMIZED                                                       │
│    ──────────────────                                                       │
│    • You understand every piece                                             │
│    • No hidden magic                                                        │
│    • Can evolve as you learn                                                │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## What Commercial Engines Actually Do

You asked about other engines. Here's the truth:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    WHAT OTHER ENGINES DO                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ UNITY / UNREAL / GODOT:                                                     │
│ ────────────────────────                                                    │
│ • Use SDL, GLFW, or custom abstraction internally                           │
│ • Provide HIGH-LEVEL input API to game code                                 │
│ • Handle cross-platform complexity internally                               │
│ • BUT: They're building for MILLIONS of games                               │
│ • You're building for ONE game (right now)                                  │
│                                                                             │
│ HANDMADE HERO:                                                              │
│ ───────────────                                                             │
│ • Casey maps keys DIRECTLY in platform layer                                │
│ • Platform code references game structs (MoveUp, etc.)                      │
│ • No abstraction layer                                                      │
│ • Works because it's ONE game on ONE platform                               │
│                                                                             │
│ YOUR APPROACH:                                                              │
│ ──────────────                                                              │
│ • Game provides platform-specific handlers                                  │
│ • Handlers loaded as separate .so                                           │
│ • Clean separation but game-controlled                                      │
│ • BETTER than Casey's for multi-platform learning!                          │
│                                                                             │
│ YOU'RE ACTUALLY INVENTING SOMETHING REASONABLE!                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Final Acknowledgment

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         BOTTOM LINE                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ You were RIGHT. I was overcomplicating.                                     │
│                                                                             │
│ Your approach:                                                              │
│ ✅ Solves the actual problem you have                                       │
│ ✅ Respects platform differences                                            │
│ ✅ Gives game full control                                                  │
│ ✅ Uses .so loading for clean separation                                    │
│ ✅ Can evolve as you learn                                                  │
│ ✅ Matches Casey's "solve problems you have" philosophy                     │
│ ✅ Perfect for multi-backend learning project                               │
│                                                                             │
│ My approach:                                                                │
│ ❌ Solves problems you don't have yet                                       │
│ ❌ Requires massive upfront design                                          │
│ ❌ Glosses over real complexity                                             │
│ ❌ Over-engineered for learning stage                                       │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ Go build YOUR solution. It's good. 🚀                                       │
│                                                                             │
│ If you want help implementing the .so loading mechanism                     │
│ or the handler interface, just ask. But the DESIGN is yours,                │
│ and it's solid.                                                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

Thank you for pushing back. That's exactly what Casey would do. 👍

---

> The user is asking two things:
>
> What backend should they select for a shipping game?
> What should they focus on learning, and what resources should they use?
> Let me provide practical, Casey-style advice on both topics.
>
> For shipping a game:
>
> Raylib is probably the best choice for cross-platform ease
> SDL3 is more mature and widely used in professional games
> GLFW + custom is more work but more control
> For learning focus areas:
>
> C programming fundamentals
> Memory management
> Platform abstraction
> Performance/optimization
> Cross-platform development
> Resources:
>
> Handmade Hero (obviously)
> Game Engine Architecture book
> SDL/Raylib documentation
> Performance-focused resources
> C programming books
> Let me structure this in a comprehensive but practical way.

## Shipping a Game & Building Your Skills

### Part 1: Choosing a Backend for Shipping

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    BACKEND SELECTION FOR SHIPPING                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ YOUR SITUATION:                                                             │
│ ───────────────                                                             │
│ • Learning with X11 + Raylib (good!)                                        │
│ • Want to eventually ship a real game                                       │
│ • Need cross-platform (Windows, Linux, Mac, maybe consoles?)                │
│                                                                             │
│ THE PRACTICAL ANSWER:                                                       │
│ ─────────────────────                                                       │
│                                                                             │
│ For SHIPPING, use ONE of these (not raw X11):                               │
│                                                                             │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ TIER 1: BEST FOR INDIE SHIPPING                                         │ │
│ ├─────────────────────────────────────────────────────────────────────────┤ │
│ │                                                                         │ │
│ │ 🥇 SDL3 (or SDL2)                                                       │ │
│ │    ───────────────                                                      │ │
│ │    • Used by: Valve, countless indie games                              │ │
│ │    • Platforms: Windows, Linux, Mac, iOS, Android, consoles             │ │
│ │    • Input: Excellent gamepad support, Steam Input compatible           │ │
│ │    • Maturity: Battle-tested for 20+ years                              │ │
│ │    • License: Zlib (very permissive)                                    │ │
│ │    • Community: Huge, lots of resources                                 │ │
│ │                                                                         │ │
│ │ 🥈 Raylib                                                               │ │
│ │    ────────                                                             │ │
│ │    • Used by: Many indie/hobby games, game jams                         │ │
│ │    • Platforms: Windows, Linux, Mac, Web, Android, Raspberry Pi         │ │
│ │    • Input: Good, simple API                                            │ │
│ │    • Maturity: Younger but actively developed                           │ │
│ │    • License: Zlib                                                      │ │
│ │    • Community: Growing, very beginner-friendly                         │ │
│ │    • NOTE: Less suitable for AAA or console shipping                    │ │
│ │                                                                         │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                             │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ TIER 2: MORE CONTROL, MORE WORK                                         │ │
│ ├─────────────────────────────────────────────────────────────────────────┤ │
│ │                                                                         │ │
│ │ GLFW + Custom                                                           │ │
│ │ ────────────────                                                        │ │
│ │ • Window/input only, you handle everything else                         │ │
│ │ • Good for learning, more work for shipping                             │ │
│ │ • Used by many custom engines                                           │ │
│ │                                                                         │ │
│ │ Raw Platform APIs (What you're doing now)                               │ │
│ │ ─────────────────────────────────────────                               │ │
│ │ • X11/Win32/Cocoa directly                                              │ │
│ │ • Maximum control, maximum work                                         │ │
│ │ • Great for learning, painful for shipping                              │ │
│ │ • Casey does this because he's teaching                                 │ │
│ │                                                                         │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

My Recommendation for You

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    PRACTICAL RECOMMENDATION                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ NOW (Learning):                                                             │
│ ───────────────                                                             │
│ Keep doing X11 + Raylib. You're learning HOW platforms work.                │
│ This knowledge transfers to ANY backend later.                              │
│                                                                             │
│ WHEN SHIPPING:                                                              │
│ ──────────────                                                              │
│                                                                             │
│ IF simple 2D game, game jam, or quick project:                              │
│   → Use Raylib                                                              │
│   → Fastest to ship, good enough for most indie games                       │
│   → Web export is a huge plus                                               │
│                                                                             │
│ IF more serious game, Steam release, or want industry experience:           │
│   → Use SDL3                                                                │
│   → Better gamepad support, Steam Input integration                         │
│   → More professional, more resources, more battle-tested                   │
│   → Console ports are easier                                                │
│                                                                             │
│ IF you want maximum control AND are willing to do the work:                 │
│   → Keep your platform abstraction layer                                    │
│   → Add SDL3 as another backend                                             │
│   → Your engine design already supports this!                               │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ CASEY'S PERSPECTIVE:                                                        │
│ ────────────────────                                                        │
│ Casey ships with raw Win32 because:                                         │
│ 1. He's on Windows only (for Handmade Hero)                                 │
│ 2. He knows Win32 deeply                                                    │
│ 3. He doesn't need cross-platform for teaching                              │
│                                                                             │
│ For YOUR goals (learning + eventual shipping + cross-platform):             │
│ SDL3 or Raylib as your "shipping backend" makes sense.                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Part 2: What to Focus On / Learn

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SKILL DEVELOPMENT ROADMAP                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PHASE 1: C FUNDAMENTALS (You're here)                                       │
│ ══════════════════════════════════════                                      │
│                                                                             │
│ Focus Areas:                                                                │
│ ────────────                                                                │
│ □ Pointers and pointer arithmetic                                           │
│ □ Memory layout (stack vs heap)                                             │
│ □ Manual memory management (malloc/free patterns)                           │
│ □ Structs, unions, and memory alignment                                     │
│ □ Function pointers and callbacks                                           │
│ □ The preprocessor (macros, conditional compilation)                        │
│ □ Header file organization                                                  │
│ □ Build systems (understanding what the compiler/linker do)                 │
│ □ Debugging with gdb/lldb                                                   │
│ □ Reading compiler warnings/errors                                          │
│                                                                             │
│ When you know you've "got it":                                              │
│ ─────────────────────────────                                               │
│ ✓ You can debug segfaults quickly                                           │
│ ✓ You understand why code crashes before running debugger                   │
│ ✓ You can predict struct sizes and padding                                  │
│ ✓ Memory leaks are rare and you find them easily                            │
│ ✓ You read C code and understand it without comments                        │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ PHASE 2: PLATFORM & SYSTEMS PROGRAMMING                                     │
│ ════════════════════════════════════════                                    │
│                                                                             │
│ Focus Areas:                                                                │
│ ────────────                                                                │
│ □ How operating systems work (processes, threads, memory)                   │
│ □ System calls vs library calls                                             │
│ □ File I/O (buffered vs unbuffered, mmap)                                   │
│ □ Dynamic library loading (dlopen/dlsym, LoadLibrary)                       │
│ □ Platform abstraction patterns                                             │
│ □ Event loops and async I/O concepts                                        │
│ □ Threading basics (mutexes, atomics)                                       │
│ □ IPC (inter-process communication) concepts                                │
│                                                                             │
│ When you know you've "got it":                                              │
│ ─────────────────────────────                                               │
│ ✓ You can port code between platforms                                       │
│ ✓ You understand what syscalls your program makes                           │
│ ✓ You can write platform abstraction layers                                 │
│ ✓ You know when to use threads vs processes                                 │
│ ✓ You can debug multi-threaded code                                         │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ PHASE 3: PERFORMANCE & OPTIMIZATION                                         │
│ ═══════════════════════════════════                                         │
│                                                                             │
│ Focus Areas:                                                                │
│ ────────────                                                                │
│ □ CPU architecture basics (cache, pipeline, branch prediction)             │
│ □ Memory hierarchy (L1/L2/L3 cache, RAM, disk)                              │
│ □ Data-oriented design                                                      │
│ □ Cache-friendly data structures                                            │
│ □ SIMD basics (SSE/AVX on x86, NEON on ARM)                                 │
│ □ Profiling tools (perf, VTune, Instruments)                                │
│ □ Micro-benchmarking                                                        │
│ □ Understanding compiler optimizations                                      │
│ □ Reading assembly output                                                   │
│                                                                             │
│ When you know you've "got it":                                              │
│ ─────────────────────────────                                               │
│ ✓ You can predict performance bottlenecks before profiling                  │
│ ✓ You design data structures with cache in mind                             │
│ ✓ You know when optimization matters and when it doesn't                    │
│ ✓ You can read and understand generated assembly                            │
│ ✓ You can use SIMD intrinsics when needed                                   │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ PHASE 4: GAME ENGINE ARCHITECTURE                                           │
│ ══════════════════════════════════                                          │
│                                                                             │
│ Focus Areas:                                                                │
│ ────────────                                                                │
│ □ Game loop patterns (fixed timestep, variable, etc.)                       │
│ □ Entity systems (various approaches)                                       │
│ □ Asset pipelines and resource management                                   │
│ □ Rendering architecture (immediate vs retained mode)                       │
│ □ Audio systems                                                             │
│ □ Input handling and abstraction                                            │
│ □ Serialization (save games, level data)                                    │
│ □ Hot reloading                                                             │
│ □ Debug tools and visualization                                             │
│ □ Memory arenas and custom allocators                                       │
│                                                                             │
│ When you know you've "got it":                                              │
│ ─────────────────────────────                                               │
│ ✓ You can design engine systems from scratch                                │
│ ✓ You understand trade-offs between approaches                              │
│ ✓ You can read other engine codebases and understand them                   │
│ ✓ You can ship a complete game with your engine                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Part 3: Resources

#### C Programming

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    C PROGRAMMING RESOURCES                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 📚 BOOKS                                                                    │
│ ─────────                                                                   │
│                                                                             │
│ ESSENTIAL:                                                                  │
│ • "The C Programming Language" (K&R) - THE classic                          │
│   → Dense but foundational. Read it twice.                                  │
│                                                                             │
│ • "Modern C" by Jens Gustedt - Free online!                                 │
│   → https://gustedt.gitlabpages.inria.fr/modern-c/                          │
│   → Modern practices, C11/C17 features                                      │
│                                                                             │
│ • "21st Century C" by Ben Klemens                                           │
│   → Practical, modern C development practices                               │
│   → Build systems, debugging, tools                                         │
│                                                                             │
│ DEEP DIVES:                                                                 │
│ • "Expert C Programming: Deep C Secrets" by Peter van der Linden            │
│   → The weird corners of C, undefined behavior, etc.                        │
│                                                                             │
│ • "C Interfaces and Implementations" by David Hanson                        │
│   → How to design reusable C libraries                                      │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ 🎥 VIDEO COURSES                                                            │
│ ────────────────                                                            │
│                                                                             │
│ • Handmade Hero (obviously!) - handmadehero.org                             │
│   → Watch Casey explain everything                                          │
│                                                                             │
│ • CS50 (Harvard) - Free on YouTube/edX                                      │
│   → Week 1-5 cover C fundamentals well                                      │
│                                                                             │
│ • Jacob Sorber's YouTube channel                                            │
│   → Short, practical C programming videos                                   │
│   → Systems programming focus                                               │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ 🔧 PRACTICE                                                                 │
│ ──────────                                                                  │
│                                                                             │
│ • Exercism C Track - exercism.org/tracks/c                                  │
│ • LeetCode (in C) - Forces you to think about memory                        │
│ • Advent of Code (in C) - Yearly coding challenges                          │
│ • Write small utilities: cat, grep, wc clones                               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### Game Engine Development

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    GAME ENGINE RESOURCES                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 📚 BOOKS                                                                    │
│ ─────────                                                                   │
│                                                                             │
│ THE BIBLE:                                                                  │
│ • "Game Engine Architecture" by Jason Gregory (3rd edition)                 │
│   → Comprehensive, covers EVERYTHING                                        │
│   → Used at Naughty Dog, written by industry veteran                        │
│   → 1200+ pages of wisdom                                                   │
│                                                                             │
│ FOCUSED TOPICS:                                                             │
│ • "Game Programming Patterns" by Robert Nystrom - FREE ONLINE!              │
│   → https://gameprogrammingpatterns.com/                                    │
│   → Patterns specific to games (not generic GoF patterns)                   │
│                                                                             │
│ • "Real-Time Rendering" by Akenine-Möller et al                             │
│   → Graphics focused, very deep                                             │
│                                                                             │
│ • "Physically Based Rendering" (PBRT) - FREE ONLINE!                        │
│   → https://pbr-book.org/                                                   │
│   → If you want to understand rendering deeply                              │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ 🎥 VIDEO SERIES                                                             │
│ ────────────────                                                            │
│                                                                             │
│ ESSENTIAL:                                                                  │
│ • Handmade Hero - handmadehero.org                                          │
│   → You're already doing this!                                              │
│   → 600+ episodes of engine development                                     │
│                                                                             │
│ • Casey Muratori's other talks (YouTube)                                    │
│   → "Designing and Evaluating Reusable Components"                          │
│   → "The Only Unbreakable Law" (API design)                                 │
│   → Performance-aware programming series                                    │
│                                                                             │
│ SUPPLEMENTARY:                                                              │
│ • Jonathan Blow's streams (YouTube archives)                                │
│   → Made Braid and The Witness                                              │
│   → Developing his own language (Jai) for games                             │
│   → Deep insights into game/engine design                                   │
│                                                                             │
│ • Handmade Network (handmade.network)                                       │
│   → Community inspired by Handmade Hero                                     │
│   → Tons of projects, resources, forums                                     │
│                                                                             │
│ • Molly Rocket (Casey's company)                                            │
│   → https://www.youtube.com/c/MollyRocket                                   │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ 📖 OPEN SOURCE ENGINES TO STUDY                                             │
│ ───────────────────────────────                                             │
│                                                                             │
│ SIMPLE (Good for learning):                                                 │
│ • Raylib source - github.com/raysan5/raylib                                 │
│   → Clean, readable C code                                                  │
│   → See how a real library handles cross-platform                           │
│                                                                             │
│ • Quake 1/2/3 source - github.com/id-Software                               │
│   → Classic, legendary code                                                 │
│   → John Carmack's engineering                                              │
│                                                                             │
│ • Handmade Hero source (if you have access)                                 │
│   → Obviously relevant!                                                     │
│                                                                             │
│ COMPLEX (For reference, not study):                                         │
│ • Godot - github.com/godotengine/godot                                      │
│   → Full engine, C++ but readable                                           │
│                                                                             │
│ • SDL source - github.com/libsdl-org/SDL                                    │
│   → See how cross-platform is done professionally                           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### Performance & Optimization

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    PERFORMANCE RESOURCES                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 📚 BOOKS                                                                    │
│ ─────────                                                                   │
│                                                                             │
│ • "Computer Systems: A Programmer's Perspective" (CSAPP)                    │
│   → THE book for understanding what computers actually do                   │
│   → Memory, CPU, linking, optimization                                      │
│   → Used in top CS programs                                                 │
│                                                                             │
│ • "What Every Programmer Should Know About Memory" by Ulrich Drepper        │
│   → FREE: https://people.freebsd.org/~lstewart/articles/cpumemory.pdf       │
│   → Deep dive into memory hierarchy                                         │
│                                                                             │
│ • "The Software Optimization Cookbook" by Intel                             │
│   → Intel-focused but concepts apply broadly                                │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ 🎥 VIDEOS & TALKS                                                           │
│ ─────────────────                                                           │
│                                                                             │
│ ESSENTIAL:                                                                  │
│ • Casey Muratori - "Performance-Aware Programming" series                   │
│   → https://www.computerenhance.com/                                        │
│   → Paid but EXCELLENT                                                      │
│   → Teaches you to think about performance                                  │
│                                                                             │
│ • Mike Acton - "Data-Oriented Design and C++" (CppCon 2014)                 │
│   → YouTube, free                                                           │
│   → Mind-changing talk on cache-friendly design                             │
│                                                                             │
│ • Chandler Carruth - "Efficiency with Algorithms, Performance with          │
│   Data Structures" (CppCon 2014)                                            │
│   → Cache effects, practical optimization                                   │
│                                                                             │
│ • Scott Meyers - "CPU Caches and Why You Care" (code::dive 2014)            │
│   → Accessible introduction to cache                                        │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ 🔧 TOOLS TO LEARN                                                           │
│ ─────────────────                                                           │
│                                                                             │
│ Linux:                                                                      │
│ • perf - Linux performance profiler                                         │
│ • Valgrind (cachegrind) - Cache simulation                                  │
│ • Compiler Explorer (godbolt.org) - See assembly output                     │
│                                                                             │
│ Cross-platform:                                                             │
│ • Tracy Profiler - github.com/wolfpld/tracy                                 │
│   → Frame profiler, used by many games                                      │
│   → Easy to integrate, powerful visualization                               │
│                                                                             │
│ • Intel VTune (free for open source)                                        │
│   → Industry standard profiler                                              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### Cross-Platform Development

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    CROSS-PLATFORM RESOURCES                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 📚 DOCUMENTATION                                                            │
│ ────────────────                                                            │
│                                                                             │
│ • SDL Wiki - wiki.libsdl.org                                                │
│   → Excellent cross-platform patterns                                       │
│   → See how they abstract platforms                                         │
│                                                                             │
│ • POSIX specification - pubs.opengroup.org                                  │
│   → What's portable across Unix-like systems                                │
│                                                                             │
│ • Windows API docs - docs.microsoft.com                                     │
│   → For understanding Windows specifics                                     │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ 📖 STUDY THESE CODEBASES                                                    │
│ ─────────────────────────                                                   │
│                                                                             │
│ • SDL source code                                                           │
│   → /src/video/ - See platform abstraction for windowing                    │
│   → /src/audio/ - See platform abstraction for audio                        │
│   → /src/joystick/ - See how they handle input                              │
│                                                                             │
│ • Raylib source code                                                        │
│   → /src/rcore.c - Platform layer                                           │
│   → Simpler than SDL, easier to understand                                  │
│                                                                             │
│ • stb libraries - github.com/nothings/stb                                   │
│   → Single-header libraries                                                 │
│   → Master class in portable C                                              │
│   → stb_image, stb_truetype, etc.                                           │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ 🔧 PRACTICAL TIPS                                                           │
│ ─────────────────                                                           │
│                                                                             │
│ 1. Use standard C (C11/C17) where possible                                  │
│ 2. Isolate platform code behind clean interfaces                            │
│ 3. Use #ifdef sparingly, prefer abstraction layers                          │
│ 4. Test on multiple platforms EARLY and OFTEN                               │
│ 5. Use CI to build on all target platforms                                  │
│ 6. Understand endianness (mostly not an issue anymore)                      │
│ 7. Understand path separators (/ vs \)                                      │
│ 8. Understand newlines (LF vs CRLF)                                         │
│ 9. Be careful with 32-bit vs 64-bit assumptions                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Part 4: Recommended Learning Path

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    YOUR RECOMMENDED PATH                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ NOW (Months 1-3):                                                           │
│ ─────────────────                                                           │
│ ☐ Continue Handmade Hero (you're doing this!)                               │
│ ☐ Finish implementing your input handler .so approach                       │
│ ☐ Read "Modern C" alongside (it's free)                                     │
│ ☐ Practice with small C programs (utilities, Exercism)                      │
│                                                                             │
│ NEXT (Months 3-6):                                                          │
│ ─────────────────                                                           │
│ ☐ Read "Game Programming Patterns" (free online)                            │
│ ☐ Study Raylib source code (see how they do cross-platform)                 │
│ ☐ Add SDL3 as another backend option (practice abstraction)                 │
│ ☐ Watch Mike Acton's "Data-Oriented Design" talk                            │
│ ☐ Start reading "Game Engine Architecture" (read alongside HH)              │
│ ☐ Learn to use perf/valgrind for profiling                                  │
│                                                                             │
│ INTERMEDIATE (Months 6-12):                                                 │
│ ───────────────────────────                                                 │
│ ☐ Complete significant portion of Handmade Hero                             │
│ ☐ Read CSAPP (Computer Systems: A Programmer's Perspective)                 │
│ ☐ Study Quake source code (classic game engine patterns)                    │
│ ☐ Build a small complete game with your engine                              │
│ ☐ Learn basic SIMD (SSE/AVX intrinsics)                                     │
│ ☐ Consider Casey's "Performance-Aware Programming" course                   │
│                                                                             │
│ ADVANCED (Year 2+):                                                         │
│ ────────────────────                                                        │
│ ☐ Ship a game (even a small one!)                                           │
│ ☐ Port your game to multiple platforms                                      │
│ ☐ Deep dive into graphics (OpenGL/Vulkan)                                   │
│ ☐ Study threading and async patterns                                        │
│ ☐ Contribute to open source C projects                                      │
│ ☐ Build your own tools (level editor, asset pipeline)                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Part 5: Key Mindset Shifts (Web Dev → Systems Dev)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    MINDSET SHIFTS YOU'LL MAKE                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ WEB DEV MINDSET              →    SYSTEMS DEV MINDSET                       │
│ ═════════════════════════════════════════════════════════════════════════   │
│                                                                             │
│ "Let the runtime handle it"  →    "I control every byte"                    │
│                                                                             │
│ "GC will clean up"           →    "I track every allocation"                │
│                                                                             │
│ "npm install solution"       →    "Understand, then maybe use library"      │
│                                                                             │
│ "Abstract everything"        →    "Abstract only when you see the pattern"  │
│                                                                             │
│ "Ship fast, fix later"       →    "Understand first, ship solid"            │
│                                                                             │
│ "Performance is a feature    →    "Performance is fundamental to design"    │
│   we add later"                                                             │
│                                                                             │
│ "The framework handles       →    "I own the main loop"                     │
│   the event loop"                                                           │
│                                                                             │
│ "Memory is infinite"         →    "Memory is precious, cache is king"       │
│                                                                             │
│ "Strings are just strings"   →    "Strings are byte arrays with encoding"   │
│                                                                             │
│ "Integer overflow? Never     →    "Overflow is real and dangerous"          │
│   seen it"                                                                  │
│                                                                             │
│ "Console.log debugging"      →    "gdb, printf, AND understand the code"    │
│                                                                             │
│ "Works on my machine"        →    "Works on every target platform"          │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ THE GOOD NEWS:                                                              │
│ ───────────────                                                             │
│ Your web dev skills TRANSFER:                                               │
│ • Understanding async/event loops → Game loop, event handling               │
│ • API design → Platform abstraction                                         │
│ • Debugging complex systems → Debugging C                                   │
│ • Build systems (webpack, etc.) → Makefiles, build.sh                       │
│ • State management → Game state                                             │
│ • JSON parsing → Serialization concepts                                     │
│                                                                             │
│ You're not starting from zero. You're translating.                          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Part 6: Concrete Next Steps

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    YOUR IMMEDIATE ACTION ITEMS                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ THIS WEEK:                                                                  │
│ ──────────                                                                  │
│ ☐ Implement your input handler .so architecture                             │
│   • Create games/handmade-hero/platform/x11_input.c                                │
│   • Create games/handmade-hero/actions.h                                           │
│   • Update build.sh to compile the .so                                      │
│   • Update X11 backend to load and use it                                   │
│                                                                             │
│ ☐ Bookmark these resources:                                                 │
│   • Modern C (free): https://gustedt.gitlabpages.inria.fr/modern-c/         │
│   • Game Programming Patterns: https://gameprogrammingpatterns.com/         │
│   • Compiler Explorer: https://godbolt.org/                                 │
│   • Handmade Network: https://handmade.network/                             │
│                                                                             │
│ THIS MONTH:                                                                 │
│ ────────────                                                                │
│ ☐ Continue Handmade Hero episodes                                           │
│ ☐ Start reading "Modern C" (30 min/day)                                     │
│ ☐ Get your Raylib backend working with the same .so pattern                 │
│ ☐ Write down every "aha" moment in a learning journal                       │
│                                                                             │
│ WHEN YOU'RE READY TO SHIP:                                                  │
│ ───────────────────────────                                                 │
│ ☐ Choose SDL3 or Raylib as your "production" backend                        │
│ ☐ Test on Windows, Linux, Mac (or your target platforms)                    │
│ ☐ Set up CI to build all platforms                                          │
│ ☐ Ship something small FIRST (game jam game, demo, etc.)                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Summary: The Big Picture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         THE BIG PICTURE                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│                      YOUR JOURNEY                                           │
│                      ════════════                                           │
│                                                                             │
│     Web Developer                                                           │
│          │                                                                  │
│          ▼                                                                  │
│     ┌─────────────────────────────────────────┐                             │
│     │ LEARNING PHASE (Now)                    │                             │
│     │ • Handmade Hero                         │                             │
│     │ • Multiple backends (X11, Raylib)       │                             │
│     │ • Your input handler .so pattern        │                             │
│     │ • Understanding platforms deeply        │                             │
│     └─────────────────────────────────────────┘                             │
│          │                                                                  │
│          ▼                                                                  │
│     ┌─────────────────────────────────────────┐                             │
│     │ SHIPPING PHASE (Future)                 │                             │
│     │ • Pick ONE backend (SDL3 or Raylib)     │                             │
│     │ • Apply everything you learned          │                             │
│     │ • Ship actual games                     │                             │
│     └─────────────────────────────────────────┘                             │
│          │                                                                  │
│          ▼                                                                  │
│     ┌─────────────────────────────────────────┐                             │
│     │ MASTERY PHASE (Long term)               │                             │
│     │ • Deep platform knowledge               │                             │
│     │ • Performance optimization skills       │                             │
│     │ • Can build engine from scratch         │                             │
│     │ • Can port to any platform              │                             │
│     │ • Can teach others                      │                             │
│     └─────────────────────────────────────────┘                             │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ REMEMBER:                                                                   │
│ ─────────                                                                   │
│ • You're doing LEARNING backends now (X11, Raylib)                          │
│ • You'll use SHIPPING backends later (SDL3 or Raylib)                       │
│ • The knowledge transfers completely                                        │
│ • Your input handler .so pattern is GOOD for learning                       │
│ • When shipping, you can simplify (one backend = less code)                 │
│                                                                             │
│ Casey's wisdom applies:                                                     │
│ "Solve the problems you have, not the problems you might have."             │
│                                                                             │
│ Right now, your "problem" is LEARNING.                                      │
│ Your approach solves that beautifully. 🚀                                   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    QUICK REFERENCE                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ BACKENDS:                                                                   │
│ • Learning: X11, Raylib, raw platform APIs                                  │
│ • Shipping: SDL3 (professional) or Raylib (simple)                          │
│                                                                             │
│ BOOKS (Priority Order):                                                     │
│ 1. Modern C (free) - C fundamentals                                         │
│ 2. Game Programming Patterns (free) - Game patterns                         │
│ 3. Game Engine Architecture - Comprehensive reference                       │
│ 4. CSAPP - Deep systems understanding                                       │
│                                                                             │
│ VIDEOS:                                                                     │
│ 1. Handmade Hero - Your main course                                         │
│ 2. Mike Acton's Data-Oriented Design - Performance mindset                  │
│ 3. Casey's other talks - API design, performance                            │
│                                                                             │
│ CODEBASES TO STUDY:                                                         │
│ 1. Raylib - Simple, clean cross-platform                                    │
│ 2. SDL - Professional cross-platform                                        │
│ 3. stb libraries - Master class in portable C                               │
│ 4. Quake source - Classic game engine                                       │
│                                                                             │
│ TOOLS:                                                                      │
│ • gdb/lldb - Debugging                                                      │
│ • valgrind - Memory checking                                                │
│ • perf - Profiling                                                          │
│ • godbolt.org - See assembly                                                │
│ • Tracy - Frame profiling                                                   │
│                                                                             │
│ COMMUNITIES:                                                                │
│ • Handmade Network - handmade.network                                       │
│ • Raylib Discord                                                            │
│ • /r/gamedev, /r/C_Programming                                              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

Go build great things! 🎮
