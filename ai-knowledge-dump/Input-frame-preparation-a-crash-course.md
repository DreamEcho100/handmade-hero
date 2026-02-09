# Input Frame Preparation: A Crash Course

## What `prepare_input_frame()` Does

```c
void prepare_input_frame(GameInput *old_input, GameInput *new_input) {
    // 1. Copy ended_down state from old → new
    // 2. Reset half_transition_count to 0
}
```

## The Problem It Solves

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  WITHOUT prepare_input_frame():                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Frame 1: User presses D                                                    │
│           → KeyPress event fires                                            │
│           → ended_down = true ✓                                             │
│                                                                             │
│  Frame 2: User HOLDS D (no event!)                                          │
│           → No KeyPress event (key already down)                            │
│           → No KeyRelease event (key not released)                          │
│           → new_input starts as zeroed memory                               │
│           → ended_down = false ✗ WRONG!                                     │
│                                                                             │
│  Frame 3: User still holding D                                              │
│           → ended_down = false ✗ WRONG!                                     │
│                                                                             │
│  RESULT: Player moves for 1 frame, then stops!                              │
│          Input feels "broken" and unresponsive.                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│  WITH prepare_input_frame():                                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Frame 1: User presses D                                                    │
│           → KeyPress event fires                                            │
│           → ended_down = true ✓                                             │
│                                                                             │
│  Frame 2: prepare_input_frame() runs FIRST                                  │
│           → Copies old ended_down (true) → new ended_down (true)            │
│           → User holds D (no event)                                         │
│           → ended_down = true ✓ CORRECT!                                    │
│                                                                             │
│  Frame 3: prepare_input_frame() runs FIRST                                  │
│           → Copies old ended_down (true) → new ended_down (true)            │
│           → ended_down = true ✓ CORRECT!                                    │
│                                                                             │
│  Frame 4: User releases D                                                   │
│           → KeyRelease event fires                                          │
│           → ended_down = false ✓                                            │
│                                                                             │
│  RESULT: Player moves smoothly while key held!                              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Why Events Are "Sparse"

| Input System | Event Firing                                                                    |
| ------------ | ------------------------------------------------------------------------------- |
| **X11**      | KeyPress once, KeyRelease once, silence in between                              |
| **Raylib**   | Polls each frame (doesn't need this for buttons, but we use it for consistency) |
| **Windows**  | WM_KEYDOWN once, WM_KEYUP once                                                  |

**Key insight:** Events fire on _transitions_, not on _state_.

## The Two Fields Explained

```c
typedef struct {
    int half_transition_count;  // How many times state changed THIS frame
    bool32 ended_down;          // Final state at end of frame
} GameButtonState;
```

| Scenario                        | half_transition_count | ended_down |
| ------------------------------- | --------------------- | ---------- |
| Not pressed, no change          | 0                     | false      |
| Just pressed                    | 1                     | true       |
| Held down                       | 0                     | true       |
| Just released                   | 1                     | false      |
| Pressed AND released same frame | 2                     | false      |
| Released AND pressed same frame | 2                     | true       |

## What You'd Handle Without It

```c
// ❌ WITHOUT prepare_input_frame(), you'd need to:

// Option A: Poll EVERY key EVERY frame (expensive, platform-specific)
new_input->move_right.ended_down = IsKeyDown(KEY_D);  // Raylib
new_input->move_right.ended_down = (XQueryKeymap(...) & D_MASK);  // X11

// Option B: Track state manually in event handlers (messy)
static bool g_d_key_down = false;  // Global state 😱
void on_key_press(KeySym key) {
    if (key == XK_d) g_d_key_down = true;
}
void on_key_release(KeySym key) {
    if (key == XK_d) g_d_key_down = false;
}
// Then copy to input struct each frame
```

## The Elegant Solution

```c
// ✅ WITH prepare_input_frame():

// 1. Start of frame: inherit previous state
prepare_input_frame(old_input, new_input);

// 2. Process events: only CHANGES update the state
case KeyPress:
    process_game_button_state(true, &new_input->move_right);
    break;
case KeyRelease:
    process_game_button_state(false, &new_input->move_right);
    break;

// 3. End of frame: swap buffers
swap(old_input, new_input);
```

## Summary

| Without `prepare_input_frame()` | With `prepare_input_frame()` |
| ------------------------------- | ---------------------------- |
| Must poll every key every frame | Only handle events           |
| Or track global state manually  | State persists automatically |
| Platform-specific polling code  | Clean event-driven design    |
| Held keys "flicker"             | Held keys stay held          |
| Complex, error-prone            | Simple, robust               |

**One-liner:** `prepare_input_frame()` makes event-driven input feel like polled input by carrying state forward between frames.
