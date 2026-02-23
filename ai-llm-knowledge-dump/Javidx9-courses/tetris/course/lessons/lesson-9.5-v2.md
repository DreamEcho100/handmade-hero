# Lesson 9.5 v2 — Professional Input System: Button States, Auto-Repeat & Action Types

---

## What You'll Have Working by the End

A professional-grade input system that:

1. **Tracks button state transitions** — knows if a button was just pressed, just released, or held
2. **Independent auto-repeat per action** — holding Left and Down simultaneously works correctly
3. **Separate action types** — movement uses auto-repeat, rotation uses single-press with direction value
4. **Frame-rate independent** — works identically at 30fps, 60fps, or 144fps

Press and hold Left — the piece moves once immediately, then auto-repeats after 100ms intervals. Press and hold Down — same behavior but faster (50ms intervals). Press X or Z — rotates once per press, no auto-repeat. Hold Left + Down together — both work independently!

---

## Where We're Starting

At the end of lesson 09:

- Pieces lock, spawn, and trigger game over correctly
- Input is simple boolean flags (`input->move_left = 1`)
- All movement shares the same timing — no independent control
- Holding a key either does nothing or moves every frame (too fast)
- No distinction between "just pressed" and "held"

---

## The Problem with Simple Boolean Input

Our current input system looks like this:

```c
typedef struct {
	  int move_left;   /* 1 if left arrow pressed this frame */
  int move_right;  /* 1 if right arrow pressed this frame */
  int move_down;   /* 1 if down arrow pressed this frame */
  int rotate_x;    /* 1 if X/Z pressed this frame */
} GameInput;
```

This has several problems:

### Problem 1: Can't Distinguish Press from Hold

```
Frame 1: User presses Left    → move_left = 1 → piece moves
Frame 2: User still holding   → move_left = 1 → piece moves AGAIN
Frame 3: User still holding   → move_left = 1 → piece moves AGAIN
...
Result: Piece flies across the screen at 60 moves per second!
```

We need to know: "Was this button JUST pressed this frame, or was it already down?"

### Problem 2: No Independent Timing

If we add a global timer to slow down movement:

```c
if (move_timer >= 0.1f) {
	  if (input->move_left) piece_col--;
  if (input->move_right) piece_col++;
  if (input->move_down) piece_row++;
  move_timer = 0;
}
```

Now ALL movement shares one timer. If you hold Left, wait 0.09s, then also press Down — Down has to wait for Left's timer to finish. They interfere with each other.

### Problem 3: Rotation Shouldn't Repeat

Holding X should rotate once, not spin continuously. But with our current system, we'd need special-case code for rotation vs. movement.

---

## The Solution: Rich Input State

We'll build a layered input system:

```
┌─────────────────────────────────────────────────────────────┐
│  GameInput                                                  │
│  ┌─────────────────────┐  ┌─────────────────────┐          │
│  │ GameActionWithRepeat│  │ GameActionWithValue │          │
│  │ (move_left)         │  │ (rotate)            │          │
│  │ ┌─────────────────┐ │  │ ┌─────────────────┐ │          │
│  │ │ GameButtonState │ │  │ │ GameButtonState │ │          │
│  │ │ - ended_down    │ │  │ │ - ended_down    │ │          │
│  │ │ - transitions   │ │  │ │ - transitions   │ │          │
│  │ └─────────────────┘ │  │ └─────────────────┘ │          │
│  │ ┌─────────────────┐ │  │ int value (1 or -1)│          │
│  │ │ GameActionRepeat│ │  │ (clockwise/counter)│          │
│  │ │ - repeat_timer  │ │  └─────────────────────┘          │
│  │ │ - repeat_interval│ │                                  │
│  │ └─────────────────┘ │                                   │
│  └─────────────────────┘                                   │
└─────────────────────────────────────────────────────────────┘
```

Each layer has a single responsibility:

- **GameButtonState** — raw hardware state (pressed/released/transitions)
- **GameActionRepeat** — auto-repeat behavior (timer + interval)
- **GameActionWithRepeat** — combines button + repeat for movement
- **GameActionWithValue** — combines button + direction for rotation

---

## Step 1: Add GameButtonState to tetris.h

Open `src/tetris.h`. After the `GameState` struct, add the input system types:

```c
/* ── Input System ─────────────────────────────────────── */

typedef struct {
	  /**
   * Number of state changes this frame.
   * 0 = No change (button held or released entire frame)
   * 1 = Changed once (normal press or release)
   * 2 = Changed twice (pressed then released, or vice versa)
   *
   * Why track this? Consider a 30fps game where the user taps a button
   * for 20ms. At 30fps, each frame is 33ms. The button went down AND up
   * within one frame — half_transition_count = 2.
   *
   * For gameplay, we usually care about: "Did the button GO DOWN this frame?"
   * That's true if: ended_down == 1 AND half_transition_count > 0
   */
  int half_transition_count;

  /**
   * Final state at end of frame.
   * 1 = button is currently pressed
   * 0 = button is currently released
   */
  int ended_down;
} GameButtonState;
```

### Understanding half_transition_count

This concept comes from Casey Muratori's Handmade Hero. A "half transition" is a change from pressed→released OR released→pressed. A full press-and-release cycle is TWO half transitions.

```
Timeline:
  Frame 1          Frame 2          Frame 3
  ─────────────────────────────────────────────
  [  press  ]      [ hold  ]        [ release ]
       ↓               ↓                 ↓
  transitions=1    transitions=0    transitions=1
  ended_down=1     ended_down=1     ended_down=0
```

Why not just use a boolean `just_pressed`? Because `half_transition_count` handles edge cases:

```
Very fast tap within one frame:
  Frame 1
  ─────────────────
  [press][release]
       ↓
  transitions=2
  ended_down=0

  We know a press HAPPENED even though the button is now up!
```

---

## Step 2: Add GameActionRepeat to tetris.h

Continue adding to `src/tetris.h`:

```c
typedef struct {
	  /** Accumulates delta time while button is held */
  float repeat_timer;

  /** Time between auto-repeats (e.g., 0.1 = 100ms) */
  float repeat_interval;
} GameActionRepeat;
```

Each action that can auto-repeat gets its own timer. This is why holding Left + Down works independently — they have separate timers.

---

## Step 3: Add Combined Action Types to tetris.h

Continue adding to `src/tetris.h`:

```c
/**
 * Action with auto-repeat capability.
 * Used for: move_left, move_right, move_down
 *
 * Behavior:
 * - First press: triggers immediately
 * - Hold: triggers again every repeat_interval seconds
 * - Release: stops triggering, resets timer
 */
typedef struct {
	  GameButtonState button;
  GameActionRepeat repeat;
} GameActionWithRepeat;

/**
 * Action with a directional value.
 * Used for: rotation
 *
 * Behavior:
 * - Press X: value = 1 (clockwise)
 * - Press Z: value = -1 (counter-clockwise)
 * - No auto-repeat — only triggers on initial press
 */
typedef struct {
	  GameButtonState button;

  /**
   * Direction or magnitude for this action.
   * For rotation: 1 = clockwise, -1 = counter-clockwise, 0 = no rotation
   */
  int value;
} GameActionWithValue;
```

---

## Step 4: Replace GameInput in tetris.h

Find the old `GameInput` struct and replace it entirely:

```c
/**
 * All inputs the game cares about.
 * platform_get_input() updates this each frame.
 *
 * Note: This struct persists across frames! The platform layer
 * updates button states, but repeat timers accumulate over time.
 * Don't zero this out every frame.
 */
typedef struct {
	  GameActionWithRepeat move_left;
  GameActionWithRepeat move_right;
  GameActionWithRepeat move_down;
  GameActionWithValue rotate;
} GameInput;
```

---

## Step 5: Implement handle_action_with_repeat() in tetris.c

Open `src/tetris.c`. Add this helper function before `tetris_apply_input()`:

```c
/**
 * Process an action with auto-repeat behavior.
 *
 * @param action     The action to process (modified in place)
 * @param delta_time Seconds since last frame
 * @param should_trigger Output: set to 1 if action should fire this frame
 *
 * Behavior:
 * - Button just pressed → trigger immediately, reset timer
 * - Button held → accumulate time, trigger when timer >= interval
 * - Button released → reset timer, don't trigger
 */
static void handle_action_with_repeat(GameActionWithRepeat *action,
                                       float delta_time,
                                       int *should_trigger) {
	  *should_trigger = 0;

  if (!action->button.ended_down) {
	    /* Button is up — reset timer, don't trigger */
    action->repeat.repeat_timer = 0.0f;
    return;
  }

  /* Button is down */
  if (action->button.half_transition_count > 0) {
	    /* Just pressed this frame — trigger immediately */
    *should_trigger = 1;
    action->repeat.repeat_timer = 0.0f;
  } else {
	    /* Held from previous frame — check auto-repeat timer */
    action->repeat.repeat_timer += delta_time;

    if (action->repeat.repeat_timer >= action->repeat.repeat_interval) {
	      /* Timer expired — trigger and reset (keeping remainder for precision) */
      action->repeat.repeat_timer -= action->repeat.repeat_interval;
      *should_trigger = 1;
    }
  }
}
```

### Walk through the logic

**Case 1: Button just pressed**

```
ended_down = 1, half_transition_count = 1
→ Trigger immediately
→ Reset timer to 0
```

**Case 2: Button held (not just pressed)**

```
ended_down = 1, half_transition_count = 0
→ Add delta_time to timer
→ If timer >= interval, trigger and subtract interval
```

**Case 3: Button released**

```
ended_down = 0
→ Reset timer to 0
→ Don't trigger
```

The key insight: `half_transition_count > 0` means the button state CHANGED this frame. Combined with `ended_down = 1`, that means it was JUST pressed.

---

## Step 6: Rewrite tetris_apply_input() in tetris.c

Replace the entire `tetris_apply_input()` function:

```c
void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {

  /* ── Rotation: only on JUST PRESSED (no auto-repeat) ── */
  if (input->rotate.button.ended_down &&
      input->rotate.button.half_transition_count > 0 &&
      input->rotate.value != 0) {

    TETROMINO_R_DIR new_rotation;

    if (input->rotate.value == 1) {
	      /* Clockwise */
      switch (state->current_piece.rotation) {
	      case TETROMINO_R_0:
        new_rotation = TETROMINO_R_90;
        break;
      case TETROMINO_R_90:
        new_rotation = TETROMINO_R_180;
        break;
      case TETROMINO_R_180:
        new_rotation = TETROMINO_R_270;
        break;
      case TETROMINO_R_270:
        new_rotation = TETROMINO_R_0;
        break;
      }
    } else {
	      /* Counter-clockwise */
      switch (state->current_piece.rotation) {
	      case TETROMINO_R_0:
        new_rotation = TETROMINO_R_270;
        break;
      case TETROMINO_R_90:
        new_rotation = TETROMINO_R_0;
        break;
      case TETROMINO_R_180:
        new_rotation = TETROMINO_R_90;
        break;
      case TETROMINO_R_270:
        new_rotation = TETROMINO_R_180;
        break;
      }
    }

    if (tetromino_does_piece_fit(state, state->current_piece.index, new_rotation,
                                  state->current_piece.col,
                                  state->current_piece.row)) {
	      state->current_piece.rotation = new_rotation;
    }
  }

  /* ── Horizontal movement: independent auto-repeat ── */
  int should_move_left = 0;
  int should_move_right = 0;

  handle_action_with_repeat(&input->move_left, delta_time, &should_move_left);
  handle_action_with_repeat(&input->move_right, delta_time, &should_move_right);

  if (should_move_left &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                                state->current_piece.rotation,
                                state->current_piece.col - 1,
                                state->current_piece.row)) {
	    state->current_piece.col--;
  }

  if (should_move_right &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                                state->current_piece.rotation,
                                state->current_piece.col + 1,
                                state->current_piece.row)) {
	    state->current_piece.col++;
  }

  /* ── Soft drop: independent auto-repeat (faster interval) ── */
  int should_move_down = 0;

  handle_action_with_repeat(&input->move_down, delta_time, &should_move_down);

  if (should_move_down &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                                state->current_piece.rotation,
                                state->current_piece.col,
                                state->current_piece.row + 1)) {
	    state->current_piece.row++;
  }
}
```

### Key differences from the old version

1. **Rotation checks `half_transition_count > 0`** — only fires on the frame the button was pressed, not while held

2. **Movement uses `handle_action_with_repeat()`** — each direction has independent timing

3. **`delta_time` is passed in** — timing is frame-rate independent

---

## Step 7: Update tetris_update() signature

The function now needs `delta_time` passed to `tetris_apply_input()`. Find `tetris_update()` and update the input handling line:

```c
void tetris_update(GameState *state, GameInput *input, float delta_time) {
	  if (state->game_over)
    return;

  /* ── Handle player input (always responsive) ── */
  tetris_apply_input(state, input, delta_time);  /* ← pass delta_time */

  /* ... rest of function unchanged ... */
}
```

---

## Step 8: Add UPDATE_BUTTON Macro

Both platform files need a way to update button state. We'll use a macro to avoid code duplication.

The macro goes inside each `platform_get_input()` function:

```c
/* Helper macro to update button state.
 * Tracks transitions and final state.
 *
 * Usage: UPDATE_BUTTON(input->move_left.button, is_key_down);
 */
#define UPDATE_BUTTON(button, is_down) \
  do { \
    if ((button).ended_down != (is_down)) { \
      (button).half_transition_count++; \
      (button).ended_down = (is_down); \
    } \
  } while(0)
```

### Why `do { ... } while(0)`?

This is a C idiom for multi-statement macros. It ensures the macro works correctly in all contexts:

```c
/* Without do-while, this breaks: */
if (condition)
  UPDATE_BUTTON(btn, 1);  /* Only first statement runs conditionally! */
else
  something_else();

/* With do-while, it's a single statement — works correctly */
```

---

## Step 9: Update platform_get_input() in main_raylib.c

Open `src/main_raylib.c`. Replace the entire `platform_get_input()` function:

```c
void platform_get_input(GameState *state, GameInput *input) {
	  /* Reset transition counts — they're per-frame */
  input->move_left.button.half_transition_count = 0;
  input->move_right.button.half_transition_count = 0;
  input->move_down.button.half_transition_count = 0;
  input->rotate.button.half_transition_count = 0;
  input->rotate.value = 0;

  /* Helper macro to update button state */
  #define UPDATE_BUTTON(button, is_down) \
    do { \
      if ((button).ended_down != (is_down)) { \
        (button).half_transition_count++; \
        (button).ended_down = (is_down); \
      } \
    } while(0)

  /* Quit handling */
  if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
	    /* Raylib handles this via WindowShouldClose() */
  }

  /* Rotation — set value based on which key */
  if (IsKeyPressed(KEY_X)) {
	    UPDATE_BUTTON(input->rotate.button, 1);
    input->rotate.value = 1;  /* Clockwise */
  } else if (IsKeyPressed(KEY_Z)) {
	    UPDATE_BUTTON(input->rotate.button, 1);
    input->rotate.value = -1;  /* Counter-clockwise */
  }

  /* Movement — can be held */
  UPDATE_BUTTON(input->move_left.button,
                IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
  UPDATE_BUTTON(input->move_right.button,
                IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT));
  UPDATE_BUTTON(input->move_down.button,
                IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));

  #undef UPDATE_BUTTON
}
```

### Why reset transition counts but not ended_down?

- `half_transition_count` is per-frame — "how many times did the state change THIS frame?"
- `ended_down` persists — "is the button currently pressed?"

If we reset `ended_down`, we'd lose track of held buttons between frames.

### Why #undef UPDATE_BUTTON?

The macro is only needed inside this function. `#undef` removes it so it doesn't pollute the rest of the file. Good hygiene.

---

## Step 10: Update main() in main_raylib.c

Find `main()` and update the input initialization:

```c
int main(void) {
	  int screen_width = FIELD_WIDTH * CELL_SIZE;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;

  if (platform_init("Tetris", screen_width, screen_height) != 0) {
	    return 1;
  }

  GameInput input = {0};

  /* Configure auto-repeat intervals */
  input.move_left.repeat.repeat_interval = 0.1f;   /* 100ms */
  input.move_right.repeat.repeat_interval = 0.1f;  /* 100ms */
  input.move_down.repeat.repeat_interval = 0.05f;  /* 50ms — faster for soft drop */
  /* Rotation has no repeat — it's a GameActionWithValue, not GameActionWithRepeat */

  GameState game_state = {0};
  game_init(&game_state);

  while (!WindowShouldClose()) {
	    float delta_time = GetFrameTime();

    platform_get_input(&game_state, &input);
    tetris_update(&game_state, &input, delta_time);
    platform_render(&game_state);
  }

  platform_shutdown();
  return 0;
}
```

### Why different intervals?

- **Horizontal (100ms)**: Slow enough to tap for single moves, fast enough to hold for rapid movement
- **Soft drop (50ms)**: Faster because players expect Down to be responsive for speed play

These values are tunable. Competitive Tetris players often want even faster rates (30-50ms for horizontal).

---

## Step 11: Update platform_get_input() in main_x11.c

Open `src/main_x11.c`. Replace the entire `platform_get_input()` function:

```c
void platform_get_input(GameState *state, GameInput *input) {
	  /* Reset transition counts — they're per-frame */
  input->move_left.button.half_transition_count = 0;
  input->move_right.button.half_transition_count = 0;
  input->move_down.button.half_transition_count = 0;
  input->rotate.button.half_transition_count = 0;
  input->rotate.value = 0;

  /* Helper macro to update button state */
  #define UPDATE_BUTTON(button, is_down) \
    do { \
      if ((button).ended_down != (is_down)) { \
        (button).half_transition_count++; \
        (button).ended_down = (is_down); \
      } \
    } while(0)

  while (XPending(g_x11.display) > 0) {
	    XNextEvent(g_x11.display, &g_x11.event);

    switch (g_x11.event.type) {
	    case KeyPress: {
	      KeySym key = XLookupKeysym(&g_x11.event.xkey, 0);

      switch (key) {
	      case XK_q:
      case XK_Q:
      case XK_Escape:
        g_is_game_running = false;
        break;

      case XK_x:
      case XK_X:
        UPDATE_BUTTON(input->rotate.button, 1);
        input->rotate.value = 1;  /* Clockwise */
        break;

      case XK_z:
      case XK_Z:
        UPDATE_BUTTON(input->rotate.button, 1);
        input->rotate.value = -1;  /* Counter-clockwise */
        break;

      case XK_Left:
      case XK_A:
      case XK_a:
        UPDATE_BUTTON(input->move_left.button, 1);
        break;

      case XK_Right:
      case XK_D:
      case XK_d:
        UPDATE_BUTTON(input->move_right.button, 1);
        break;

      case XK_Down:
      case XK_S:
      case XK_s:
        UPDATE_BUTTON(input->move_down.button, 1);
        break;
      }
      break;
    }

    case KeyRelease: {
	      KeySym key = XLookupKeysym(&g_x11.event.xkey, 0);

      switch (key) {
	      case XK_x:
      case XK_X:
      case XK_z:
      case XK_Z:
        UPDATE_BUTTON(input->rotate.button, 0);
        input->rotate.value = 0;
        break;

      case XK_Left:
      case XK_A:
      case XK_a:
        UPDATE_BUTTON(input->move_left.button, 0);
        break;

      case XK_Right:
      case XK_D:
      case XK_d:
        UPDATE_BUTTON(input->move_right.button, 0);
        break;

      case XK_Down:
      case XK_S:
      case XK_s:
        UPDATE_BUTTON(input->move_down.button, 0);
        break;
      }
      break;
    }

    case ClientMessage:
      g_is_game_running = false;
      break;
    }
  }

  #undef UPDATE_BUTTON
}
```

### X11 vs Raylib difference

Raylib gives us `IsKeyDown()` which we can poll every frame. X11 gives us discrete KeyPress/KeyRelease events. The UPDATE_BUTTON macro handles both patterns — it tracks state changes regardless of how we detect them.

---

## Step 12: Update main() in main_x11.c

Find `main()` and update the input initialization:

```c
int main(void) {
	  int screen_width = FIELD_WIDTH * CELL_SIZE;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;

  if (platform_init("Tetris", screen_width, screen_height) != 0) {
	    return 1;
  }

  GameInput input = {0};

  /* Configure auto-repeat intervals */
  input.move_left.repeat.repeat_interval = 0.1f;   /* 100ms */
  input.move_right.repeat.repeat_interval = 0.1f;  /* 100ms */
  input.move_down.repeat.repeat_interval = 0.05f;  /* 50ms — faster for soft drop */

  GameState game_state = {0};
  game_init(&game_state);

  while (g_is_game_running) {
	    double current_time = platform_get_time();
    double delta_time = current_time - g_last_frame_time;
    g_last_frame_time = current_time;

    platform_get_input(&game_state, &input);
    tetris_update(&game_state, &input, (float)delta_time);
    platform_render(&game_state);

    double frame_time = platform_get_time() - current_time;
    if (frame_time < g_target_frame_time) {
	      platform_sleep_ms((int)((g_target_frame_time - frame_time) * 1000.0));
    }
  }

  platform_shutdown();
  return 0;
}
```

---

## Step 13: Build and Run

```bash
./build_x11.sh && ./tetris_x11
```

Or for Raylib:

```bash
./build_raylib.sh && ./tetris_raylib
```

### What to Test

| Action               | Expected Result                                     |
| -------------------- | --------------------------------------------------- |
| Tap Left once        | Piece moves one cell left                           |
| Hold Left            | Piece moves once, pauses briefly, then auto-repeats |
| Tap Down once        | Piece moves one cell down                           |
| Hold Down            | Piece drops faster than horizontal (50ms vs 100ms)  |
| Hold Left + Down     | Both work independently — piece moves diagonally    |
| Tap X                | Piece rotates clockwise once                        |
| Hold X               | Piece rotates once only — no auto-repeat            |
| Tap Z                | Piece rotates counter-clockwise once                |
| Tap X then Z quickly | Rotates clockwise, then counter-clockwise           |

### Common Issues

**Piece doesn't move at all:**
Check that `repeat_interval` is set in `main()`. If it's 0, the timer condition `>= 0` is always true but the subtraction creates weird behavior.

**Piece moves on key release:**
Check that KeyRelease sets `ended_down = 0`, not `1`.

**Rotation auto-repeats:**
Check that rotation uses `half_transition_count > 0` check, not just `ended_down`.

---

## Key Concept: Separation of Concerns

The input system now has clear layers:

```
Hardware Events (X11/Raylib)
         ↓
   GameButtonState (raw state + transitions)
         ↓
   GameActionRepeat (timing behavior)
         ↓
   GameActionWithRepeat / GameActionWithValue (combined)
         ↓
   Game Logic (tetris_apply_input)
```

Each layer has one job:

- **Platform layer**: Convert OS events to button states
- **Button state**: Track pressed/released and transitions
- **Action repeat**: Handle timing for auto-repeat
- **Game logic**: Apply actions to game state

This separation means you can:

- Change the repeat interval without touching input code
- Add new action types (e.g., `GameActionWithCharge` for a future hard drop)
- Port to a new platform by only rewriting the platform layer

---

## Mental Model: Why half_transition_count?

Think of a button press as a waveform:

```
        ┌───────────────┐
        │               │
────────┘               └────────
   ↑                        ↑
 Press                   Release
 (transition)            (transition)
```

Each edge (up or down) is a "half transition". A full press-release cycle has 2 half transitions.

In a single frame, we might see:

```
Frame boundary          Frame boundary
      │                       │
      │   ┌───┐               │
      │   │   │               │
──────│───┘   └───────────────│──
      │                       │
      │← half_transition_count = 2
      │   ended_down = 0
```

The button went down AND up within one frame. Without tracking transitions, we'd miss the press entirely (ended_down = 0 at frame end).

This is why fighting games and rhythm games track transitions — they need frame-perfect input detection even when the frame rate is lower than the player's button speed.

---

## Exercise: Add Initial Delay (DAS)

Modern Tetris has "Delayed Auto-Shift" (DAS): when you hold a direction, there's a longer delay before auto-repeat starts, then faster repeats after.

```
Current:  Press → move → 100ms → move → 100ms → move ...
With DAS: Press → move → 170ms → move → 50ms → move → 50ms ...
                         ↑ initial delay    ↑ faster repeat
```

Try adding a `das_delay` field to `GameActionRepeat`:

```c
typedef struct {
	  float repeat_timer;
  float repeat_interval;  /* Time between repeats (ARR) */
  float das_delay;        /* Initial delay before repeating (DAS) */
  int das_charged;        /* Has the initial delay passed? */
} GameActionRepeat;
```

Then modify `handle_action_with_repeat()` to use the initial delay:

```c
static void handle_action_with_repeat(GameActionWithRepeat *action,
                                       float delta_time,
                                       int *should_trigger) {
  *should_trigger = 0;

  if (!action->button.ended_down) {
    /* Button is up — reset everything */
    action->repeat.repeat_timer = 0.0f;
    action->repeat.das_charged = 0;
    return;
  }

  /* Button is down */
  if (action->button.half_transition_count > 0) {
    /* Just pressed this frame — trigger immediately */
    *should_trigger = 1;
    action->repeat.repeat_timer = 0.0f;
    action->repeat.das_charged = 0;
  } else {
    /* Held from previous frame */
    action->repeat.repeat_timer += delta_time;

    if (!action->repeat.das_charged) {
      /* Still in DAS delay period */
      if (action->repeat.repeat_timer >= action->repeat.das_delay) {
        action->repeat.das_charged = 1;
        action->repeat.repeat_timer = 0.0f;
        *should_trigger = 1;  /* First auto-repeat after DAS */
      }
    } else {
      /* DAS charged — use faster repeat interval (ARR) */
      if (action->repeat.repeat_timer >= action->repeat.repeat_interval) {
        action->repeat.repeat_timer -= action->repeat.repeat_interval;
        *should_trigger = 1;
      }
    }
  }
}
```

Then in `main()`, configure both values:

```c
/* DAS = 170ms initial delay, ARR = 50ms repeat */
input.move_left.repeat.das_delay = 0.17f;
input.move_left.repeat.repeat_interval = 0.05f;

input.move_right.repeat.das_delay = 0.17f;
input.move_right.repeat.repeat_interval = 0.05f;

/* Soft drop: shorter DAS, same fast repeat */
input.move_down.repeat.das_delay = 0.1f;
input.move_down.repeat.repeat_interval = 0.03f;
```

This gives you the "sticky then fast" feel of modern Tetris games!

---

## Checkpoint

Files changed in this lesson:

| File                | What Changed                                                                                                                                |
| ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| `src/tetris.h`      | Added `GameButtonState`, `GameActionRepeat`, `GameActionWithRepeat`, `GameActionWithValue`; replaced `GameInput` struct                     |
| `src/tetris.c`      | Added `handle_action_with_repeat()`; rewrote `tetris_apply_input()` to use new input system; updated `tetris_update()` to pass `delta_time` |
| `src/main_x11.c`    | Rewrote `platform_get_input()` with `UPDATE_BUTTON` macro; configured repeat intervals in `main()`                                          |
| `src/main_raylib.c` | Rewrote `platform_get_input()` with `UPDATE_BUTTON` macro; configured repeat intervals in `main()`                                          |

---

## Summary: What We Built

### Before (Simple Booleans)

```c
typedef struct {
  int move_left;   /* 1 or 0 */
  int move_right;
  int move_down;
  int rotate_x;
} GameInput;
```

**Problems:**

- Can't tell press from hold
- All actions share timing
- Rotation auto-repeats unintentionally
- Frame-rate dependent

### After (Rich Input State)

```c
typedef struct {
  GameActionWithRepeat move_left;   /* Button + independent timer */
  GameActionWithRepeat move_right;
  GameActionWithRepeat move_down;
  GameActionWithValue rotate;       /* Button + direction value */
} GameInput;
```

**Benefits:**

- Detects "just pressed" vs "held"
- Each action has independent auto-repeat
- Rotation only fires once per press
- Frame-rate independent via delta_time
- Easy to tune per-action (different intervals)
- Extensible for future features (DAS, hold piece, etc.)

---

## What's Next

This input system is the foundation for professional game feel. Future lessons can build on it:

- **Lesson 10**: Line clearing and scoring (uses this input system unchanged)
- **Lesson 11**: Next piece preview and hold piece (add new `GameActionWithValue` for hold)
- **Lesson 12**: Lock delay (piece stays moveable briefly after landing)
- **Lesson 13**: Wall kicks (rotation tries alternate positions if blocked)

The input system you built today handles all of these without modification — you just add game logic on top.

**Next:** Lesson 10 — Line Clearing & Scoring
