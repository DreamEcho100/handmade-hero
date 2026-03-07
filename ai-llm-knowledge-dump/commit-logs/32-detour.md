# 📝 Handmade Hero Detour: Learning Notes

## Why I Took a Detour from Handmade Hero (Day 32)

I paused **Handmade Hero** around Day 32 because you realized you needed to strengthen some **fundamentals** (like C/C++, math, graphics, or debugging) before continuing. Instead of blindly following the series, you chose to **step back, study the basics, and return later with a better understanding.**

## Sessions Summary

### 🧪 Detour Session 1: Tetris (Javidx9) — Platform Abstraction & Game Loop Foundation

**Focus:** Building a complete Tetris game with clean platform abstraction, demonstrating separation between game logic and rendering backends (X11 and Raylib).

---

#### 🗓️ Commits

| Date       | Commit     | What Changed                                           | Why I Designed It This Way                                                                                                                                                                                                                                                                                                                                                       |
| ---------- | ---------- | ------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2026-02-22 | `7d49224`  | Initial Tetris foundation (Day 1)                      | Started with dual backend (X11/Raylib) to practice platform abstraction from day 1. Defined tetromino shapes as const strings, following Casey's data-first approach. Used `draw_cell()` helper to abstract away coordinate math - each backend implements it once, game code never touches pixel coordinates.                                                                   |
| 2026-02-22 | `2efe1c0`  | Delta time implementation (Day 8 v2)                   | Replaced tick-based dropping with `delta_time` accumulation. This is frame-rate independent and more precise than counting frames. Added `platform_get_time()` abstraction - X11 uses `gettimeofday()`, Raylib uses `GetTime()`. The game logic is identical across platforms because timing is abstracted.                                                                      |
| 2026-02-23 | `b71527e`  | Refactor: Separated game/bootstrap code                | Split hot-reloadable code into two libraries: `game-bootstrap.so` (init/startup - loaded once) and `game-main.so` (update/render - hot-reloadable). This mirrors Casey's Day 21/22 pattern where you want minimal code in the reload path. Engine now has `GameMainCode` and `GameBootstrapCode` structs instead of monolithic `GameCode`.                                       |
| 2026-02-23 | `6eeb656`  | Update handmade-hero-game submodule                    | Synced submodule to latest game code changes.                                                                                                                                                                                                                                                                                                                                    |
| 2026-02-23 | `70872642` | Professional input system (Lesson 9.5 v2)              | Implemented Casey-style button state tracking: `GameButtonState` with `half_transition_count` and `ended_down`. This enables detecting "just pressed" vs "held" without platform-specific logic. Added `GameActionWithRepeat` for auto-repeat movement and `GameActionWithValue` for rotation direction. Each action has independent timing - no shared global timer.            |
| 2026-02-24 | `3e46f77`  | Line clearing with proper row collapse (Lesson 10)     | Added multi-line clear detection and collapse. Key insight: process completed rows bottom-to-top and adjust remaining indices after each collapse. Without this, collapsing row 15 would invalidate the stored index for row 14. Also added flash animation pause using `flash_timer` countdown.                                                                                 |
| 2026-02-25 | `508f346`  | X11 input fixes, testing approach documentation        | Fixed non-blocking input in X11 (was using blocking `XNextEvent`). Added comprehensive testing checklist to README. Documented the importance of testing multi-line clears specifically - they expose algorithmic bugs that single-line clears hide.                                                                                                                             |
| 2026-02-26 | `823b25b`  | Course material organization and documentation cleanup | Reorganized AI/LLM knowledge dump folder structure. Moved course lessons and discussions into clearer hierarchy. This reflects my workflow: I use AI (Claude) to help convert implementation experience into teaching material, but the code and architectural decisions are mine. The AI helps with formatting, alternative explanations, and catching edge cases I might miss. |

---

#### 🎮 Reverse Engineering Notes

| Mechanic                | Observed Behavior                                         | Implementation Hypothesis                                                                                                                |
| ----------------------- | --------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| Piece rotation          | Rotates around center, blocked by walls/pieces            | 4x4 bounding box, rotation is index remapping (`pi = 12 + py - px*4` for 90°). Collision check tries new rotation before committing.     |
| Line clearing           | White flash → pause → rows collapse → pieces above drop   | Flash timer counts down (8 frames), game pauses. On timer==0, copy rows down starting from completed row, clear top row.                 |
| Multiple lines at once  | I-piece can clear 1-4 rows simultaneously                 | Store up to 4 row indices in array, process bottom-to-top to avoid index invalidation.                                                   |
| Gravity                 | Piece drops at fixed interval, speeds up over time        | Accumulate `delta_time`, drop when `timer >= interval`. Decrease interval every N pieces.                                                |
| Lock delay (future)     | Piece waits briefly after landing before locking          | Not implemented yet, but plan: separate `landed_timer`, piece can still move/rotate until timer expires.                                 |
| Auto-repeat             | Hold left/right: move once, pause, then rapid repeat      | `GameActionRepeat` tracks timer per action. First press triggers immediately, then accumulates time for auto-repeat. Independent timers. |
| Soft drop               | Hold down: faster drop, not instant                       | Separate `move_down` action with shorter `repeat_interval` (50ms vs 100ms for horizontal).                                               |
| Rotation direction      | Two buttons for clockwise/counter-clockwise               | `GameActionWithValue` stores direction as int: 1=CW, -1=CCW. No auto-repeat - only fires on button press.                                |
| Game over detection     | New piece spawns overlapping existing blocks → game over  | After locking piece, check if new spawn position fits. If collision on spawn, set `game_over=true`.                                      |
| Score increase patterns | +25 per piece, +100/200/400/800 for 1/2/3/4 lines cleared | Will implement in next lesson. Pattern is exponential: `score += 100 * (2^(lines-1))` for line clears.                                   |

**Key Implementation Insight:** Javidx9's original uses tick counting (`speed_count++`), but converting to delta time revealed the underlying pattern: **accumulate time until threshold, then trigger event**. This same pattern appears everywhere in game code - bullet cooldowns, animation timers, spawn intervals. Understanding it once applies universally.

---

#### 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                     PLATFORM ABSTRACTION LAYER                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌───────────────────────┐         ┌───────────────────────┐       │
│  │   main_x11.c          │         │   main_raylib.c       │       │
│  │                       │         │                       │       │
│  │  - XOpenDisplay()     │         │  - InitWindow()       │       │
│  │  - gettimeofday()     │         │  - GetTime()          │       │
│  │  - XPending()         │         │  - IsKeyDown()        │       │
│  │  - nanosleep()        │         │  - GetFrameTime()     │       │
│  │  - XFillRectangle()   │         │  - DrawRectangle()    │       │
│  └───────────────────────┘         └───────────────────────┘       │
│           │                                   │                     │
│           └───────────┬───────────────────────┘                     │
│                       ↓                                             │
│  ┌───────────────────────────────────────────────────────────┐     │
│  │              platform.h (Abstract Interface)              │     │
│  │                                                           │     │
│  │  platform_init()                                          │     │
│  │  platform_get_input()                                     │     │
│  │  platform_render()                                        │     │
│  │  platform_get_time()                                      │     │
│  │  platform_sleep_ms()                                      │     │
│  │  platform_shutdown()                                      │     │
│  └───────────────────────────────────────────────────────────┘     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────────┐
│                        GAME LOGIC LAYER                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌───────────────────────────────────────────────────────────┐     │
│  │   tetris.c (Platform Independent)                         │     │
│  │                                                           │     │
│  │  game_init()                                              │     │
│  │  tetris_update(delta_time)                                │     │
│  │  tetris_apply_input(input)                                │     │
│  │  tetris_does_piece_fit()                                  │     │
│  │  tetromino_pos_value()                                    │     │
│  │                                                           │     │
│  │  GameState:                                               │     │
│  │    - field[FIELD_WIDTH * FIELD_HEIGHT]                    │     │
│  │    - current_piece                                        │     │
│  │    - drop_timer                                           │     │
│  │    - drop_interval                                        │     │
│  │    - game_over                                            │     │
│  └───────────────────────────────────────────────────────────┘     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘

Game Loop (Both Platforms):

  ┌───────────────────────────────────┐
  │ main()                            │
  ├───────────────────────────────────┤
  │ 1. platform_init()                │
  │ 2. game_init()                    │
  │                                   │
  │ while (running) {                 │
  │   3. delta = get_delta_time()     │
  │   4. platform_get_input(&input)   │
  │   5. tetris_update(&state,        │
  │                    &input,         │
  │                    delta)          │
  │   6. platform_render(&state)      │
  │   7. sleep_if_needed()            │
  │ }                                 │
  │                                   │
  │ 8. platform_shutdown()            │
  └───────────────────────────────────┘

Input Flow:

  Platform Events  →  GameInput struct  →  tetris_apply_input()

  X11:
    XNextEvent() → XLookupKeysym() → UPDATE_BUTTON() → GameButtonState

  Raylib:
    IsKeyDown() → UPDATE_BUTTON() → GameButtonState

  Game Logic:
    GameButtonState → handle_action_with_repeat() → should_trigger
                   → if (should_trigger) do_movement()

Render Flow:

  GameState  →  platform_render()  →  Backend-specific draw calls

  Game knows: "piece at (col=5, row=3), rotation=90, index=I"
  Platform knows: "draw 4x4 grid starting at pixel (150, 90)"

Delta Time Integration:

  Frame 1:  16ms → delta=0.016 → drop_timer=0.016
  Frame 2:  17ms → delta=0.017 → drop_timer=0.033
  ...
  Frame 60: 18ms → delta=0.018 → drop_timer=1.002 → DROP!
  Frame 61: 16ms → delta=0.016 → drop_timer=0.002 (remainder kept)
```

**Key Architectural Decisions:**

1. **No game logic in platform layer**: Platform only does I/O (display, input, timing). Game logic never calls X11/Raylib directly.

2. **Symmetric platform API**: Both backends implement the same 6 functions. This makes adding a third backend (e.g., SDL, Win32) trivial - just implement those 6 functions.

3. **Delta time at boundary**: Platform measures time, game consumes it. Game never calls `GetTime()` directly. This keeps timing concerns in the platform layer.

4. **Field as flat array**: `field[row * FIELD_WIDTH + col]` instead of `field[row][col]`. This matches Casey's pattern: data is contiguous, cache-friendly, easy to memset.

5. **Rotation as index transform**: No matrix math. Rotation is just an index remap function. Fast, deterministic, easy to debug.

---

#### 🎯 Core Concepts

| Concept                           | Implementation                                                | What I Learned                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| --------------------------------- | ------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Platform Abstraction**          | Functions vs pointers vs macros                               | Casey uses function pointers (`game->UpdateAndRender`). I used direct function calls (`platform_render()`). Both work, but direct calls are simpler when you're not hot-reloading the platform layer. The key insight: **the interface matters more than the mechanism**. As long as game code never includes `<X11/Xlib.h>` or `<raylib.h>`, you've won.                                                                                          |
| **Delta Time Accumulation**       | `drop_timer += delta_time; if (timer >= interval) { ... }`    | This pattern is **everywhere** in games. Bullet cooldowns, spawn timers, animation frames, physics steps. Understanding it once gives you a universal tool. The key: **keep the remainder** (`timer -= interval` not `timer = 0`). Without this, you get timing drift over hundreds of frames.                                                                                                                                                     |
| **Button State Tracking**         | `GameButtonState` with `half_transition_count` + `ended_down` | Casey's Handmade Hero pattern. A "half transition" is one state change (pressed→released or released→pressed). Why count transitions instead of just "pressed"? Because a **very fast tap** (press+release within one frame) has `transitions=2, ended_down=0`. If you only check `ended_down`, you miss the press entirely. This matters for things like frame-perfect fighting game inputs.                                                      |
| **Independent Action Timers**     | `GameActionWithRepeat` has its own `repeat_timer`             | **Anti-pattern**: Global movement timer shared by all directions. If you hold left, then also press down, down has to wait for left's timer. **Solution**: Each action gets its own timer. Now holding left+down works correctly - they're independent. This is why modern games feel responsive - every input has its own timing logic.                                                                                                           |
| **Just Pressed vs Held**          | `ended_down && half_transition_count > 0`                     | **Just pressed** = button is down AND it changed state this frame. **Held** = button is down AND it didn't change state. This distinction is critical: rotation should only fire on "just pressed", but movement can fire on "held". Without this, holding the rotate button spins the piece continuously (wrong feel).                                                                                                                            |
| **Flat Array for Grid**           | `field[row * FIELD_WIDTH + col]`                              | Why not `field[row][col]`? Three reasons: (1) Contiguous memory = cache-friendly. (2) Easy to memset/memcpy the entire grid. (3) Can iterate with a single loop: `for (int i = 0; i < FIELD_SIZE; i++)`. Casey does this in Handmade Hero tilemap code for the same reasons.                                                                                                                                                                       |
| **Rotation as Index Remap**       | `pi = 12 + py - (px * 4)` for 90° clockwise                   | No matrices, no trigonometry. Rotation is just shuffling indices in a 4x4 grid. This is **deterministic** (no floating point), **fast** (integer math), and **debuggable** (you can print the index transform and see exactly what's happening). The math was hard to derive the first time, but now it's a lookup table in my brain.                                                                                                              |
| **Collision Before Movement**     | `if (does_fit(new_pos)) { pos = new_pos; }`                   | Never move, then check. Always check, then move. This prevents "tunneling" where the piece phases through walls. Also makes rollback trivial - if the move is illegal, you never committed it. Casey does this in Handmade Hero's collision code.                                                                                                                                                                                                  |
| **Line Clear Index Adjustment**   | Process rows bottom-to-top, adjust remaining indices          | **Subtle bug**: If you store rows [14, 15] and process top-to-bottom, after clearing row 14, row 15 shifts to row 14 but your index still says 15 (wrong!). **Solution**: Process bottom-to-top. After clearing row 15, row 14 is still at row 14 (correct). Then after clearing row 14, adjust all remaining indices: `lines[j]++`. This is the kind of off-by-one error that takes 2 hours to debug if you don't understand the pattern upfront. |
| **Frame Rate Independence**       | Game speed same at 30fps, 60fps, 144fps                       | The **silver bullet** for timing. If your game runs at 60fps on your dev machine but 30fps on a slow laptop, delta time keeps behavior consistent. Casey mentions this in Handmade Hero Day 17 (game loop timing). The key: `movement = velocity * delta_time`, not `movement = velocity`. Scale everything by time.                                                                                                                               |
| **Stub Functions for Hot Reload** | `game_update_and_render_stub()` returns immediately           | When the game library fails to load (compile error, missing symbol), the engine falls back to stub functions. This prevents crashes during development. Casey's Day 21/22 pattern: **always have a valid function pointer, even if it does nothing**. Better to show a black screen than crash the editor.                                                                                                                                         |

---

#### 💻 Key Code Snippets

**1. Platform-Independent Rotation (tetris.c)**

```c
/* Rotate a 4x4 piece bounding box.
 * px, py: column and row (0-3)
 * r: rotation (0=0°, 1=90°, 2=180°, 3=270°)
 * Returns: flat array index (0-15)
 *
 * Why this works:
 * - 0°:   identity (py * 4 + px)
 * - 90°:  rotate clockwise (12 + py - px*4)
 * - 180°: flip both axes (15 - py*4 - px)
 * - 270°: rotate counter-clockwise (3 - py + px*4)
 */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r % 4) {
  case 0: return py * 4 + px;          /* 0° */
  case 1: return 12 + py - (px * 4);   /* 90° CW */
  case 2: return 15 - (py * 4) - px;   /* 180° */
  case 3: return 3 - py + (px * 4);    /* 270° CW (or 90° CCW) */
  }
  return 0; /* Unreachable */
}
```

**Why This is Good:**

- No floating point (deterministic)
- No branches inside hot loop (piece rendering calls this 16 times per frame)
- Easy to verify: print the 16 indices for each rotation, visually check they match
- Pattern generalizes: same math works for any grid rotation

**Data-Oriented Insight:** The tetromino is stored as a **flat string** (`"..X...X...X...X."`), not a 2D array. Rotation doesn't modify the data - it just changes how we index it. This is compression: store once, view many ways.

---

**2. Delta Time Game Loop (main_x11.c / main_raylib.c)**

```c
int main(void) {
  platform_init("Tetris", screen_width, screen_height);

  GameInput input = {0};
  GameState game_state = {0};
  game_init(&game_state);

  /* Timing setup */
  double last_time = platform_get_time();
  const double target_frame_time = 1.0 / 60.0;  /* 16.67ms */

  while (!WindowShouldClose()) {
    /* Measure actual frame time */
    double current_time = platform_get_time();
    float delta_time = (float)(current_time - last_time);
    last_time = current_time;

    /* Update game with real elapsed time */
    platform_get_input(&game_state, &input);
    tetris_update(&game_state, &input, delta_time);
    platform_render(&game_state);

    /* Sleep if frame finished early */
    double frame_time = platform_get_time() - current_time;
    if (frame_time < target_frame_time) {
      platform_sleep_ms((int)((target_frame_time - frame_time) * 1000.0));
    }
  }

  platform_shutdown();
  return 0;
}
```

**Why This is Good:**

- **Frame-rate independent**: Game runs same speed on fast/slow machines
- **Precise**: Sub-millisecond timing (`double` has enough precision for microseconds)
- **Adaptive**: If one frame takes 20ms, next frame gets `delta_time=0.020`, game logic compensates
- **Portable**: X11 uses `gettimeofday()`, Raylib uses `GetTime()`, game code is identical

**Casey's Influence:** This is the pattern from Handmade Hero Day 17. Casey explains why `delta_time` is better than counting frames: if you count frames and the frame rate drops, the game slows down. If you use delta time, the game logic adjusts to keep real-time behavior consistent.

---

**3. Independent Action Repeat System (tetris.c)**

```c
/* Auto-repeat for held buttons (e.g., movement).
 * action: button state + repeat timer
 * delta_time: time since last frame
 * should_trigger: output - set to 1 if action fires this frame
 */
static void handle_action_with_repeat(GameActionWithRepeat *action,
                                       float delta_time,
                                       int *should_trigger) {
  *should_trigger = 0;

  if (!action->button.ended_down) {
    /* Button released - reset timer */
    action->repeat.repeat_timer = 0.0f;
    return;
  }

  /* Button is down */
  if (action->button.half_transition_count > 0) {
    /* JUST pressed this frame - instant response */
    *should_trigger = 1;
    action->repeat.repeat_timer = 0.0f;
  } else {
    /* HELD from previous frame - check auto-repeat */
    action->repeat.repeat_timer += delta_time;

    if (action->repeat.repeat_timer >= action->repeat.repeat_interval) {
      /* Timer exceeded - trigger and keep remainder */
      action->repeat.repeat_timer -= action->repeat.repeat_interval;
      *should_trigger = 1;
    }
  }
}

/* Usage in game update: */
int should_move_left = 0;
handle_action_with_repeat(&input->move_left, delta_time, &should_move_left);

if (should_move_left) {
  /* Move piece left (only if collision check passes) */
  if (tetromino_does_piece_fit(..., piece_col - 1, ...)) {
    piece_col--;
  }
}
```

**Why This is Good:**

- **Instant first press**: Player presses left, piece moves immediately (no delay)
- **Smooth auto-repeat**: Hold left, piece moves once, pauses, then repeats at configurable rate
- **Independent timing**: Holding left + down works correctly - each has its own timer
- **Easy tuning**: `repeat_interval` is data, not hardcoded. Can be different for horizontal (100ms) vs soft drop (50ms)

**Comparison to Bad Pattern:**

```c
/* BAD: Global timer shared by all actions */
static float g_move_timer = 0.0f;

if (input->move_left || input->move_right || input->move_down) {
  g_move_timer += delta_time;
  if (g_move_timer >= 0.1f) {
    g_move_timer = 0.0f;
    /* Apply whichever button is pressed */
  }
}
/* Problem: If you press left, wait 0.09s, then also press down,
 * down has to wait for left's timer to finish. Feels laggy. */
```

**Data-Oriented Insight:** Each action is a struct with its own state. We don't have global variables or if-else chains. Loop over actions, call the same function, done. This scales: adding a 5th, 6th, 7th action doesn't complicate the code.

---

**4. Multi-Line Clear with Index Adjustment (tetris.c)**

```c
/* After piece locks, check for completed lines */
for (int py = 0; py < 4; py++) {
  int row_y = state->current_piece.row + py;

  /* Skip if outside playable area */
  if (row_y < 0 || row_y >= FIELD_HEIGHT - 1) continue;

  /* Check if row is complete */
  bool row_complete = true;
  for (int px = 1; px < FIELD_WIDTH - 1; px++) {
    if (state->field[row_y * FIELD_WIDTH + px] == 0) {
      row_complete = false;
      break;
    }
  }

  if (row_complete) {
    /* Mark row for clearing */
    state->lines[state->line_count++] = row_y;

    /* Flash the row (visual feedback) */
    for (int px = 1; px < FIELD_WIDTH - 1; px++) {
      state->field[row_y * FIELD_WIDTH + px] = 8;  /* white */
    }
  }
}

if (state->line_count > 0) {
  state->flash_timer = 8;  /* Pause for 8 frames (400ms at 60fps) */
}

/* Later, when flash_timer hits 0: */
if (state->flash_timer == 0 && state->line_count > 0) {
  /* Collapse lines BOTTOM-TO-TOP */
  for (int i = state->line_count - 1; i >= 0; i--) {
    int line_y = state->lines[i];

    /* Copy rows above this one down by 1 */
    for (int y = line_y; y > 0; y--) {
      for (int x = 1; x < FIELD_WIDTH - 1; x++) {
        state->field[y * FIELD_WIDTH + x] =
            state->field[(y - 1) * FIELD_WIDTH + x];
      }
    }

    /* Clear the top row (nothing above it to copy) */
    for (int x = 1; x < FIELD_WIDTH - 1; x++) {
      state->field[x] = 0;
    }

    /* CRITICAL: Adjust remaining line indices
     * All lines above the one we just collapsed have shifted down by 1 */
    for (int j = i - 1; j >= 0; j--) {
      state->lines[j]++;
    }
  }

  state->line_count = 0;
}
```

**Why This is Tricky:**

- If you process top-to-bottom and have rows [14, 15] cleared, after clearing row 14, row 15 becomes row 14 but your index still says 15 (off by one!)
- **Solution**: Process bottom-to-top. After clearing row 15, row 14 is still at row 14. After clearing row 14, increment all remaining indices.

**Bug I Hit:** Forgot the index adjustment loop. Symptom: Single-line clears worked, but double-line clears left one row floating in space. Took me 30 minutes to figure out because the bug only appeared with 2+ simultaneous clears.

**Data Structure Note:** `state->lines[4]` is a stack. Push completed row indices onto it, then pop and process. Maximum 4 because a piece is 4 rows tall - can't clear more than 4 at once.

---

**5. Non-Blocking Input Loop (main_x11.c)**

```c
/* X11 event polling - CRITICAL: must be non-blocking */
void platform_get_input(GameState *state, GameInput *input) {
  memset(input, 0, sizeof(GameInput));

  /* Process ALL pending events without blocking.
   * XPending() returns event count - if 0, skip the loop entirely.
   * This keeps the game running even when user isn't touching keyboard. */
  while (XPending(g_x11.display) > 0) {
    XNextEvent(g_x11.display, &g_x11.event);

    switch (g_x11.event.type) {
    case KeyPress: {
      KeySym key = XLookupKeysym(&g_x11.event.xkey, 0);

      switch (key) {
      case XK_Left:
      case XK_a:
        UPDATE_BUTTON(input->move_left.button, 1);
        break;
      /* ... other keys ... */
      }
      break;
    }

    case KeyRelease: {
      KeySym key = XLookupKeysym(&g_x11.event.xkey, 0);

      switch (key) {
      case XK_Left:
      case XK_a:
        UPDATE_BUTTON(input->move_left.button, 0);
        break;
      /* ... other keys ... */
      }
      break;
    }

    case ClientMessage:
      /* User clicked window close button */
      g_is_game_running = false;
      break;
    }
  }
}

/* Helper macro to update button state */
#define UPDATE_BUTTON(button, is_down) \
  do { \
    if ((button).ended_down != (is_down)) { \
      (button).half_transition_count++; \
      (button).ended_down = (is_down); \
    } \
  } while(0)
```

**Critical Bug I Fixed:**

- **Before**: Used `XNextEvent()` outside `XPending()` check. This **blocks** until an event arrives. If user doesn't press anything, game freezes.
- **After**: Only call `XNextEvent()` if `XPending() > 0`. Now the game runs at 60fps even when idle.

**Raylib Comparison:** Raylib's `IsKeyDown()` is non-blocking by design. X11 requires explicit non-blocking logic. This is why platform abstraction matters - hide this detail from game code.

---

#### 🏗️ Architecture Decisions

| Decision                          | Reason                                                                                                                                                                                                                                                                                                                                                                                      |
| --------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Dual backend from day 1**       | Forced me to think about platform abstraction upfront instead of retrofitting it later. X11 teaches low-level Linux APIs, Raylib teaches what a "good" high-level API looks like. Comparing them side-by-side reveals which concerns are platform-specific (windowing, input events) vs universal (game logic, collision, timing).                                                          |
| **Delta time over tick counting** | Javidx9's original counts ticks (`speed_count++; if (count==20) drop()`). This breaks on variable frame rates. Delta time works at 30fps, 60fps, 144fps identically. Also enables easy pause/slow-mo by just not passing time to update. Pattern mirrors Casey's Day 17 game loop.                                                                                                          |
| **Flat field array**              | `field[row * WIDTH + col]` instead of `field[row][col]`. Three wins: (1) Contiguous memory = cache-friendly. (2) Can memset entire grid in one call. (3) Pointer arithmetic is simple. Casey uses this for Handmade Hero tilemaps.                                                                                                                                                          |
| **Rotation as index remap**       | No matrices, no trig. Rotation is `px' = f(px, py, rotation)`. Faster than matrix multiply, deterministic (no floating point), debuggable (print indices and see exactly what's happening). Pattern comes from classic NES Tetris implementations.                                                                                                                                          |
| **Button state struct**           | `GameButtonState` with `half_transition_count` + `ended_down` instead of just boolean pressed/released. Enables detecting "just pressed", "just released", "held", "double-tap within one frame". Casey's Handmade Hero pattern - essential for responsive input. Fighting games use this for frame-perfect moves.                                                                          |
| **Independent action timers**     | Each action (`move_left`, `move_right`, `move_down`) has its own timer. **Anti-pattern**: Global timer shared by all. Without independent timers, holding left+down feels broken because they interfere. With independent timers, you can slide diagonally smoothly. This is why modern games feel responsive - every input is its own little state machine.                                |
| **Collision check before move**   | `if (does_fit(new_pos)) { pos = new_pos; }` not `pos = new_pos; if (!does_fit(pos)) { undo(); }`. Why? (1) Never enter invalid state. (2) No "undo" logic needed. (3) Prevents tunneling bugs. Casey's collision pattern from Handmade Hero Day 29-30.                                                                                                                                      |
| **Flash pause pattern**           | Game freezes during line clear animation (`flash_timer > 0 → return early`). This is **deliberate game feel**. Without pause, rows vanish instantly (feels abrupt). With pause, player sees what they achieved (feels satisfying). Classic arcade pattern - many games pause briefly on important events (big combo, boss death, etc.). Data-driven: `flash_timer` is just a countdown int. |
| **Hot reload split**              | `game-bootstrap.so` (init/startup) separate from `game-main.so` (update/render). Why? Bootstrap code rarely changes during development - you don't want to reload it every time. Main code changes constantly - hot reload saves seconds per iteration. Over 100 iterations/day, this is minutes saved. Pattern from Casey's Day 21/22.                                                     |

---

#### 🐛 Bugs & Pitfalls

| Issue                                    | Cause                                                 | Fix                                                                                                 | What I Learned                                                                                                                                            |
| ---------------------------------------- | ----------------------------------------------------- | --------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Game freezes when idle**               | X11 `XNextEvent()` blocks if no events                | Wrap in `while (XPending() > 0)` - only process events if they exist                                | Raylib/SDL handle this internally. X11 requires explicit non-blocking logic. Always check event queue size before draining it.                            |
| **Piece floats after double clear**      | Processed rows top-to-bottom, forgot index adjustment | Process bottom-to-top, add `for (int j = i-1; j >= 0; j--) lines[j]++;` after each collapse         | Off-by-one errors hide in edge cases. Single clears worked, multi-clears failed. **Always test multi-line clears** to expose this class of bug.           |
| **Rotation stutters**                    | Rotation auto-repeating on held button                | Use `half_transition_count > 0` check - only fire on button press, not hold                         | "Just pressed" vs "held" distinction is critical for game feel. Rotation should be **discrete** (one tap = one rotation), movement can be **continuous**. |
| **Timers drift over time**               | Reset timer to 0 on trigger instead of subtracting    | `timer -= interval` keeps remainder, prevents drift                                                 | After 100 drops, resetting to 0 can accumulate 200ms+ error. Subtracting keeps sub-frame precision. Matters for long play sessions.                       |
| **Move left while moving right glitchy** | Shared timer for all directions                       | Each action gets its own `GameActionRepeat` struct with independent timer                           | Global state is the enemy of responsive input. Every action should be its own isolated state machine.                                                     |
| **Flash animation too fast**             | Countdown in frames, forgot frame rate varies         | Use `flash_timer` as frame count at fixed 60fps. Could convert to seconds if targeting variable fps | Animation timing is tricky with variable frame rate. For now, assuming 60fps is acceptable. Future: store animation duration in seconds.                  |

---

#### ✅ Skills Strengthened

- ✅ **Platform abstraction design** - Created clean interface between game logic and OS-specific rendering/input (X11 vs Raylib)
- ✅ **Delta time integration** - Implemented frame-rate independent game loop using accumulated time
- ✅ **Input state machines** - Built comprehensive button state tracking (`half_transition_count`, `ended_down`) for detecting press/release/hold
- ✅ **Independent action timing** - Each input action has its own auto-repeat timer, enabling smooth simultaneous inputs
- ✅ **Grid-based collision** - Implemented piece-vs-field collision using bounding box overlap checks
- ✅ **Rotation math** - Mastered 4x4 grid rotation via index remapping (no matrices or trig)
- ✅ **Multi-line clear algorithm** - Solved the row collapse + index adjustment problem (process bottom-to-top)
- ✅ **Flat array 2D grids** - Used `field[row * WIDTH + col]` pattern for cache-friendly contiguous memory
- ✅ **Animation timing with pauses** - Implemented flash effect using countdown timer to freeze game state
- ✅ **Non-blocking event loops** - Fixed X11 event polling to avoid blocking main loop when no input
- ✅ **Hot reload architecture** - Split code into bootstrap (init/startup) vs main (update/render) for efficient iteration
- ✅ **Deterministic simulation** - All logic uses integer math or accumulated delta time (no random frame-dependent behavior)

---

#### 🔍 What This Taught Me for Handmade Hero

**1. Platform Abstraction Confidence**

Casey's Handmade Hero platform layer (Win32 wrapper) seemed mysterious at first. Now I've built two backends (X11, Raylib) implementing the same interface. I understand **why** he structures it that way:

- Game code never includes platform headers
- Platform layer is small (6-8 functions)
- Adding another platform is just implementing those functions

When I return to Handmade Hero, the Win32 platform layer will make complete sense. I'll also be able to port it to X11 easily.

**2. Delta Time Intuition**

Casey introduces delta time in Day 17. Before this detour, I understood the concept abstractly. Now I've **lived** the problem:

- Tick-based code breaks on slow machines
- Delta time fixes it
- Keeping the remainder prevents drift
- Pattern applies to cooldowns, animations, spawn timers

This isn't just "Tetris timing" - it's a universal game programming pattern.

**3. Input System Depth**

Casey's button state code in Handmade Hero was intimidating. Now I've implemented `GameButtonState` with `half_transition_count` myself. I understand:

- Why transitions matter (frame-perfect detection)
- Why independent timers matter (responsive feel)
- Why "just pressed" needs state change tracking

When Casey refines input handling in later episodes, I'll recognize the patterns immediately.

**4. Collision Before Movement**

Casey's collision code in Days 29-30 follows the pattern: **check fit, then move**. I've internalized why:

- Never enter invalid state
- No rollback logic needed
- Prevents tunneling bugs
- Easier to reason about

The Tetris collision system is simpler than Handmade Hero's (Tetris is discrete grid, HH is continuous 2D), but the principle is identical.

**5. Data-Oriented Structure**

The entire Tetris codebase follows patterns from early Handmade Hero:

- Flat arrays (`field[]`) instead of pointers
- Structs of state, not classes with methods
- Functions take explicit parameters, not implicit `this`
- No hidden state or singletons

This isn't "doing C wrong" - it's **compression-oriented programming**. The fewer pointers, inheritance hierarchies, and dynamic allocations, the easier the code is to hold in your head.

**6. Hot Reload Architecture**

Casey's Day 21/22 hot reload seemed complex. Now I've split code into:

- `game-bootstrap.so` - loads once (init, startup)
- `game-main.so` - reloads constantly (update, render)

I understand **why** he does this: you don't want to reinitialize memory/resources on every code change. Only the update loop needs hot reload. When I return to Handmade Hero, the `game.dll` architecture will be obvious.

**7. Debugging Frame-Rate Issues**

I hit bugs that only appeared at specific frame rates:

- Timers drifting after 100 frames
- Input lag on slow frames
- Row collapse off-by-one only with 2+ lines

These are **exactly** the kinds of bugs Casey debugs in later Handmade Hero episodes. Now I know what to look for and how to reason about timing-dependent bugs.

---

**Overall:** This detour gave me **mechanical intuition** for patterns Casey uses throughout Handmade Hero. When I return to Day 32+, I'll recognize:

- Why he splits platform/game code
- Why he uses delta time
- Why he tracks button transitions
- Why he does collision checks before movement
- Why he prefers flat arrays over nested structures

The detour isn't a distraction - it's **building muscle memory** so I can follow Casey's more complex implementations without getting lost in syntax. I've proven I can implement these patterns myself, not just copy his code.

### 🧪 Detour Session 2: Snake (Javidx9) — Ring Buffer Entity System & Direction State Machine

**Focus:** Implementing a ring buffer for snake body segments, a direction state machine with quaternion-like rotation logic, and frame-independent auto-repeat movement using delta time accumulation.

---

#### 🗓️ Commits

| Date       | Commit    | What Changed                                                                                   | Why I Designed It This Way                                                                                                                                                                                                                                                                                                                                                                                                                            |
| ---------- | --------- | ---------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2026-02-28 | `b958768` | Initial Snake implementation: ring buffer, direction enum, basic rendering                     | **Ring buffer choice:** Snake body is a classic ring buffer use case—fixed capacity, elements never individually deleted, head/tail pointers wrap. This is more efficient than shifting arrays or linked lists. **Direction enum:** Used typed enum `SNAKE_DIR` (0-3) instead of raw ints to enable modular arithmetic for rotation: `(dir + 1) % 4` = turn right, `(dir + 3) % 4` = turn left. Pattern inspired by unit circle quaternion rotations. |
| 2026-03-02 | `cad7fb9` | Moved commit logs, cleaned up documentation structure                                          | Reorganized commit history tracking—moved early Handmade Hero logs (Days 0-22) into separate file since detour phase is architecturally distinct. **Why split?** Main Handmade Hero follows Casey's engine-building arc; detour projects are complete standalone games. Different documentation needs.                                                                                                                                                |
| 2026-03-02 | `cd0477c` | Tetris audio implementation: volume ramping, panning, ALSA init, Raylib double-buffering       | Not Snake-specific, but **learned Casey's audio latency model** during Tetris. Applied same patterns to Snake (though Snake currently has no audio). Key insight: always pre-fill buffers with silence before starting playback (prevents initial clicks). Raylib needs larger buffers than ALSA because you can't query exact delay. This will apply when adding Snake sound effects.                                                                |
| 2026-03-03 | `1808217` | Raylib audio buffer fix, simplified init, removed redundant config fields                      | Cleaned up audio config—removed `buffer_size_bytes` (redundant with `samples_per_second * bytes_per_sample`). **Why?** Reducing derived state prevents desyncs. Only store source values, calculate on demand. Pattern from Casey's Day 24 refactor.                                                                                                                                                                                                  |
| 2026-03-03 | `8551f04` | Updated handmade-hero-game submodule (audio changes)                                           | Synced submodule pointer after audio refactor. Git submodules = pain, but necessary for separating engine vs game repos.                                                                                                                                                                                                                                                                                                                              |
| 2026-03-03 | `f335ea1` | Added comprehensive audio system documentation to course-builder.md                            | Wrote 600-line audio architecture guide: latency model, click prevention, panning, stereo mixing, platform differences (ALSA vs Raylib). **Why document during detour?** Solidifies understanding. Teaching forces you to explain "why", not just "what". Will reuse this material when documenting Handmade Hero's audio (Day 7-10).                                                                                                                 |
| 2026-03-03 | `46d978f` | Updated handmade-hero-game submodule (volume ramping)                                          | Another submodule sync—game now uses `SoundSource.current_volume` for smooth transitions.                                                                                                                                                                                                                                                                                                                                                             |
| 2026-03-03 | `8aeccf0` | Tetris input refactor: double-buffered input, DAS/ARR, union for button iteration              | **Critical pattern learned:** Input needs TWO buffers to detect transitions. Current frame vs previous frame. Without this, you can't distinguish "just pressed" from "held". Also implemented **DAS (Delayed Auto Shift)** and **ARR (Auto Repeat Rate)**—the professional input timing used in competitive Tetris. Snake doesn't need this (no auto-repeat), but the button state machine is identical.                                             |
| 2026-03-04 | `e2643bb` | Updated course-builder.md: added double-buffered input, button state machine, timing diagrams  | Documented the "why" behind double-buffering. Key insight: **half_transition_count** captures rapid taps within one frame (pressed+released in 16ms). Without counting transitions, fast taps are lost. This is critical for fighting games, rhythm games, any frame-perfect input. Snake doesn't need frame-perfect input, but pattern is universal.                                                                                                 |
| 2026-03-04 | `0e83081` | Sugar Sugar: Course builder improvements, Y-down exception, SoA patterns, audio tier selection | Not Snake, but **documented exceptions to Y-up coordinate rule**. Falling sand games (Sugar Sugar) use Y-down pixel coordinates because each grain IS a pixel. No unit conversion. Snake uses Y-down tile coordinates for same reason—tiles are grid indices, not world units. Added rule: "Y-down allowed when simulation domain is discrete grid."                                                                                                  |

---

#### 🎮 Reverse Engineering Notes

| Mechanic                      | Observed Behavior                                                                      | Implementation Hypothesis                                                                                                                                                        |
| ----------------------------- | -------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Snake body representation** | Body segments follow head in sequence, tail disappears when head moves                 | Ring buffer: `segments[MAX_SNAKE]`, `head` and `tail` indices. On move: advance head, write new position, advance tail (deletes oldest). No array shifting or reallocation.      |
| **Direction control**         | Left/Right turn relative to current direction, not absolute WASD                       | Direction stored as enum (0-3). Turn left = `(dir + 3) % 4`. Turn right = `(dir + 1) % 4`. Like rotating a 2D vector by 90°, but using modular arithmetic instead of trig.       |
| **Movement timing**           | Snake moves in discrete steps, one tile per fixed interval                             | Accumulate `delta_time` in `move_timer`. When `timer >= interval`, advance snake, subtract interval (keep remainder for precision). Same pattern as Tetris piece dropping.       |
| **Turn buffering**            | Pressing Left then Right quickly executes both turns smoothly                          | Store `next_direction` on key press. Apply `next_direction → direction` at movement step (not immediately). This prevents losing inputs between frames.                          |
| **Collision detection**       | Snake dies when head hits wall or body segment                                         | Check new head position against: (1) grid bounds [0, WIDTH-1], (2) all body segments. Dies if match found. No spatial grid needed—linear search is fine for ~100 segments.       |
| **Self-collision**            | Head can pass through body if snake is short, but hits if length > 5                   | Collision checks skip first N segments near head (too close to collide). Or: only check segments where `index < head - 2` (minimum separation).                                  |
| **Apple spawning**            | Apple appears at random empty tile after being eaten                                   | Generate random (x, y), check if occupied by snake. If yes, retry. Once empty found, place apple. **Potential issue:** Infinite loop if grid is full (edge case).                |
| **Score increase**            | Score increases by 10 per apple, speed increases every 5 apples, length increases by 1 | Each apple: `score += 10`, `length++`, `apples_eaten++`. If `apples_eaten % 5 == 0`: decrease `move_interval` by 10% (clamp to minimum). Length added by NOT advancing tail.     |
| **Death animation**           | Game freezes briefly, snake flashes red, then "Game Over" text                         | State machine: `PLAYING → DEATH_ANIM → GAME_OVER`. Death anim phase: decrement `death_timer`, flash color using `(frame_count % 2)`. When timer hits 0, transition to game over. |

**Key Implementation Insight:** The "turn left/right relative to current direction" mechanic is the core of Snake's control feel. If you use absolute WASD (e.g., A = move left regardless of facing), the game feels wrong. The direction enum + modular arithmetic pattern is the elegant solution.

---

#### 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SNAKE GAME ARCHITECTURE                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │  INPUT SYSTEM (from Tetris pattern)                            │    │
│  ├────────────────────────────────────────────────────────────────┤    │
│  │  GameInput inputs[2];  // Double-buffered                      │    │
│  │  GameInput *current = &inputs[current_index];                  │    │
│  │  GameInput *old = &inputs[1 - current_index];                  │    │
│  │                                                                 │    │
│  │  prepare_input_frame(old, current);  // Copy ended_down        │    │
│  │  platform_get_input(current);        // Update transitions     │    │
│  │                                                                 │    │
│  │  // Detect turn:                                               │    │
│  │  if (current->turn_left.ended_down &&                          │    │
│  │      current->turn_left.half_transition_count > 0) {           │    │
│  │    snake->next_direction = (snake->direction + 3) % 4;         │    │
│  │  }                                                              │    │
│  └────────────────────────────────────────────────────────────────┘    │
│                            ↓                                            │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │  DIRECTION STATE MACHINE                                       │    │
│  ├────────────────────────────────────────────────────────────────┤    │
│  │  typedef enum {                                                │    │
│  │    SNAKE_DIR_UP = 0,    // -Y                                  │    │
│  │    SNAKE_DIR_RIGHT = 1, // +X                                  │    │
│  │    SNAKE_DIR_DOWN = 2,  // +Y                                  │    │
│  │    SNAKE_DIR_LEFT = 3   // -X                                  │    │
│  │  } SNAKE_DIR;                                                  │    │
│  │                                                                 │    │
│  │  // Rotation table (like unit circle quaternions):              │    │
│  │  int dx[4] = { 0,  1,  0, -1 };  // cos(90° * dir)             │    │
│  │  int dy[4] = {-1,  0,  1,  0 };  // sin(90° * dir)             │    │
│  │                                                                 │    │
│  │  // Turn left: CCW rotation = subtract 1 mod 4                 │    │
│  │  next_dir = (current_dir + 3) % 4;                             │    │
│  │                                                                 │    │
│  │  // Turn right: CW rotation = add 1 mod 4                      │    │
│  │  next_dir = (current_dir + 1) % 4;                             │    │
│  └────────────────────────────────────────────────────────────────┘    │
│                            ↓                                            │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │  SNAKE RING BUFFER                                             │    │
│  ├────────────────────────────────────────────────────────────────┤    │
│  │  typedef struct {                                              │    │
│  │    Segment segments[MAX_SNAKE];  // Fixed capacity             │    │
│  │    int head;   // Index of head (newest)                       │    │
│  │    int tail;   // Index of tail (oldest)                       │    │
│  │    int length; // Active segment count                         │    │
│  │  } Snake;                                                      │    │
│  │                                                                 │    │
│  │  // Movement logic:                                            │    │
│  │  1. Calculate new_x = head.x + dx[direction]                   │    │
│  │  2. Calculate new_y = head.y + dy[direction]                   │    │
│  │  3. head = (head + 1) % MAX_SNAKE  // Wrap index               │    │
│  │  4. segments[head] = {new_x, new_y}                            │    │
│  │  5. tail = (tail + 1) % MAX_SNAKE  // Delete oldest            │    │
│  │                                                                 │    │
│  │  // Eating apple:                                              │    │
│  │  - DON'T advance tail (keeps old segment)                      │    │
│  │  - length++                                                    │    │
│  └────────────────────────────────────────────────────────────────┘    │
│                            ↓                                            │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │  MOVEMENT TIMING (Delta Time Accumulation)                     │    │
│  ├────────────────────────────────────────────────────────────────┤    │
│  │  typedef struct {                                              │    │
│  │    float timer;     // Accumulated time                        │    │
│  │    float interval;  // Time between steps (e.g., 0.15s)        │    │
│  │  } GameActionRepeat;                                           │    │
│  │                                                                 │    │
│  │  snake->move_forward.timer += delta_time;                      │    │
│  │                                                                 │    │
│  │  if (snake->move_forward.timer >= snake->move_forward.interval) { │    │
│  │    snake->move_forward.timer -= snake->move_forward.interval;  │    │
│  │                                                                 │    │
│  │    // Apply next_direction → direction                         │    │
│  │    snake->direction = snake->next_direction;                   │    │
│  │                                                                 │    │
│  │    // Move head forward                                        │    │
│  │    int dx = DIRECTION_X[snake->direction];                     │    │
│  │    int dy = DIRECTION_Y[snake->direction];                     │    │
│  │    int new_x = snake->segments[snake->head].x + dx;            │    │
│  │    int new_y = snake->segments[snake->head].y + dy;            │    │
│  │                                                                 │    │
│  │    snake->head = (snake->head + 1) % MAX_SNAKE;                │    │
│  │    snake->segments[snake->head] = {new_x, new_y};              │    │
│  │                                                                 │    │
│  │    snake->tail = (snake->tail + 1) % MAX_SNAKE;                │    │
│  │  }                                                              │    │
│  └────────────────────────────────────────────────────────────────┘    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Why Ring Buffer?**

Snake's body is a perfect ring buffer use case:

- Fixed capacity (e.g., 400 segments max)
- Elements are never individually deleted (only tail advances)
- FIFO order (oldest segment removed when new head added)
- No reallocation, no shifting

**Alternative data structures and why they're worse:**

| Structure         | Why It's Bad for Snake                                                                                 |
| ----------------- | ------------------------------------------------------------------------------------------------------ |
| **Dynamic array** | Requires `realloc` on grow, `memmove` on remove. Slow, fragile, unnecessary                            |
| **Linked list**   | Cache-unfriendly (each segment malloc'd separately), harder to iterate, more pointer chasing           |
| **Deque**         | Overkill—ring buffer is simpler, no STL dependency                                                     |
| **Shift array**   | Moving body: `memmove(&segments[1], &segments[0], length * sizeof(Segment))` every frame—**O(n) pain** |

**Ring buffer performance:** All operations O(1) (add head, remove tail, iterate), contiguous memory (cache-friendly), deterministic (no fragmentation).

---

#### 🎯 Core Concepts

| Concept                         | Implementation                                                                  | What I Learned                                                                                                                                                                                                                                                                                                                                                                                                             |
| ------------------------------- | ------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Ring Buffer Mechanics**       | `head = (head + 1) % MAX_SNAKE` advances, wraps at end                          | **Key insight:** Modulo operator (`%`) is the secret sauce. Without it, you'd need explicit `if (head == MAX_SNAKE) head = 0`. Modulo handles wrap automatically. **Pattern applies to:** circular audio buffers, command histories, frame timing logs, any fixed-capacity FIFO queue.                                                                                                                                     |
| **Direction as Rotation**       | Direction is enum 0-3, turns are `(dir ± 1) % 4`                                | **Quaternion intuition without quaternions:** This is mathematically equivalent to 2D rotation by 90°, but using integer arithmetic instead of trig. `(dir + 1) % 4` = rotate CW, `(dir + 3) % 4` = rotate CCW (same as `(dir - 1 + 4) % 4` to avoid negative modulo). **Pattern from:** unit circle, complex number rotation, but implemented as lookup table.                                                            |
| **Delta Time Accumulation**     | `timer += dt; if (timer >= interval) { timer -= interval; move(); }`            | **Same pattern as Tetris dropping:** Accumulate real time, trigger discrete event when threshold crossed, subtract (not zero) to keep sub-frame precision. **Why subtract?** If interval = 0.15s and timer = 0.16s, zeroing timer loses 0.01s every step—after 100 steps, you're 1 second off! Subtracting preserves accuracy.                                                                                             |
| **Turn Buffering**              | Key press sets `next_direction`, applied at movement step                       | **Problem:** If you apply direction change immediately on key press, rapid Left+Right can cause 180° reversal (snake eats itself). **Solution:** Buffer pending direction in `next_direction`, apply only when snake actually moves. This is a **1-frame input buffer**—prevents instant reversal, feels smoother. **Pattern from:** Fighting game input buffering (store last N inputs, execute on frame-perfect timing). |
| **Direction Validity Check**    | `new_dir != (current_dir + 2) % 4` prevents 180° turn                           | **Why needed:** If snake is moving RIGHT (1) and player presses LEFT (3), that's `(1 + 2) % 4 = 3`—a 180° reversal. Snake would instantly hit its own neck. **Solution:** Reject `next_direction` if it equals `(direction + 2) % 4`. Only allow 90° turns or straight. **Alternative:** Check `abs(new_dir - current_dir) != 2`—same logic, different notation.                                                           |
| **Collision Detection**         | Linear search through `segments[tail..head]` for overlap with new head position | **No spatial grid needed:** Snake has max ~400 segments, linear search is fast enough. Check `new_x == segments[i].x && new_y == segments[i].y` for all active segments. **Optimization:** Skip first 2 segments after head (can't collide with own neck). **When to use spatial grid:** Only if entities >> 1000 OR collision checks per frame >> 10,000.                                                                 |
| **Growth Without Tail Advance** | On apple: don't increment `tail`, increment `length`, keep oldest segment       | **Elegant trick:** Normal move = advance head, advance tail (length unchanged). Growth = advance head, DON'T advance tail (length++). The old tail segment stays, making snake longer. No special "insert segment" logic needed—ring buffer handles it naturally.                                                                                                                                                          |
| **Frame-Independent Speed**     | `interval` in seconds, not frames                                               | **Anti-pattern:** `if (frame_count % 10 == 0) move();` breaks if FPS changes. **Correct:** `interval = 0.15f` (150ms), accumulate `delta_time`. Works identically at 30fps, 60fps, 144fps. **Casey's insight:** Time-based simulation decouples game logic from rendering. This is why Handmade Hero uses `dt` everywhere.                                                                                                 |
| **Double-Buffered Input**       | `GameInput inputs[2]`, swap `current_index` each frame                          | **Why?** To detect transitions (pressed this frame vs held from last frame), you need TWO frames of state. `current_input` = this frame's keys, `old_input` = last frame's keys. Compare to detect state change. **Without this:** Can't distinguish "just pressed" from "held"—all input feels mushy. **Pattern from:** Handmade Hero Day 10, also used in fighting games for frame-perfect cancels.                      |
| **Grid Coordinate System**      | Y-down, (0,0) = top-left, grid is `[0..WIDTH-1][0..HEIGHT-1]`                   | **Exception to Y-up rule:** Tile-based games use Y-down because grid indices naturally map to screen pixels. No unit conversion needed. **Rule from Sugar Sugar lesson:** Discrete grid simulati                                                                                                                                                                                                                           |

ons (Snake, Tetris, falling sand) → Y-down pixel/tile coords. Continuous world simulations (Handmade Hero, physics engines) → Y-up meter/unit coords. |

---

#### 💻 Key Code Snippets

**1. Ring Buffer Snake Body (Core Data Structure)**

```c
#define GRID_WIDTH 60
#define GRID_HEIGHT 20
#define MAX_SNAKE (GRID_WIDTH * GRID_HEIGHT)  // 1200 max segments

typedef struct {
  int x;
  int y;
} Segment;

typedef struct {
  Segment segments[MAX_SNAKE];  // Fixed capacity ring buffer
  int head;   // Index of newest segment (where snake's head is)
  int tail;   // Index of oldest segment (where tail will be removed)
  int length; // Number of active segments
  SNAKE_DIR direction;
  SNAKE_DIR next_direction;
} Snake;

// Movement (called every `move_interval` seconds):
void snake_move(Snake *snake) {
  // Apply buffered turn
  snake->direction = snake->next_direction;

  // Calculate new head position
  int dx = DIRECTION_X[snake->direction];
  int dy = DIRECTION_Y[snake->direction];
  int new_x = snake->segments[snake->head].x + dx;
  int new_y = snake->segments[snake->head].y + dy;

  // Advance head (wrap with modulo)
  snake->head = (snake->head + 1) % MAX_SNAKE;
  snake->segments[snake->head] = (Segment){new_x, new_y};

  // Advance tail UNLESS we just ate an apple
  if (!just_ate_apple) {
    snake->tail = (snake->tail + 1) % MAX_SNAKE;
  } else {
    snake->length++;  // Keep old tail, grow snake
  }
}
```

**Why this works:**

- `head` and `tail` are indices, not pointers—simple integer math
- Modulo handles wrap automatically (no `if (head >= MAX_SNAKE) head = 0`)
- Adding new segment = write to `segments[head]`, increment `head`
- Removing old segment = increment `tail` (overwrites next time head reaches that index)

**Data-Oriented Win:** All segments contiguous in memory → cache-friendly iteration when rendering/collision checking.

---

**2. Direction Enum and Rotation Logic (Quaternion-Style)**

```c
typedef enum {
  SNAKE_DIR_UP = 0,      // -Y
  SNAKE_DIR_RIGHT = 1,   // +X
  SNAKE_DIR_DOWN = 2,    // +Y
  SNAKE_DIR_LEFT = 3     // -X
} SNAKE_DIR;

// Direction vectors (lookup table, not trig!)
#define DIRECTION_SNAKE_SIZE 4
static const int DIRECTION_X[DIRECTION_SNAKE_SIZE] = {0,  1,  0, -1};
static const int DIRECTION_Y[DIRECTION_SNAKE_SIZE] = {-1, 0,  1,  0};

// Turn left (counter-clockwise): subtract 1, wrap with modulo
// Example: RIGHT (1) → UP (0)
// (1 + 3) % 4 = 4 % 4 = 0 ✓
SNAKE_DIR turn_left(SNAKE_DIR current) {
  return (current + 3) % 4;  // +3 is same as -1 in mod 4
}

// Turn right (clockwise): add 1, wrap with modulo
// Example: RIGHT (1) → DOWN (2)
// (1 + 1) % 4 = 2 ✓
SNAKE_DIR turn_right(SNAKE_DIR current) {
  return (current + 1) % 4;
}

// 180° reversal check (reject invalid turns)
bool is_opposite_direction(SNAKE_DIR current, SNAKE_DIR proposed) {
  return proposed == (current + 2) % 4;
}
```

**Why typed enum + modulo arithmetic?**

- **No switch statement needed:** Direction change is pure arithmetic
- **No trigonometry:** Faster than `cos/sin`, deterministic (no float precision issues)
- **Pattern scales:** Same math works for 8-way movement (just use `% 8`)
- **Inspired by:** 2D rotation matrices `[cos θ, -sin θ; sin θ, cos θ]`, but implemented as integer lookup table

**Comparison to naive approach:**

```c
// ❌ BAD: Absolute WASD (feels wrong in Snake)
if (key == KEY_A) snake->direction = SNAKE_DIR_LEFT;
if (key == KEY_D) snake->direction = SNAKE_DIR_RIGHT;
// Problem: If snake is moving UP and player presses A, snake instantly moves LEFT
// (not left relative to facing, but absolute left). Feels broken.

// ✅ GOOD: Relative turn (feels natural)
if (key == KEY_A) snake->next_direction = turn_left(snake->direction);
if (key == KEY_D) snake->next_direction = turn_right(snake->direction);
// Now if snake is moving UP and player presses A, snake turns to LEFT (correct!)
```

---

**3. Delta Time Movement Timing (Frame-Independent)**

```c
typedef struct {
  float timer;     // Accumulated time since last move
  float interval;  // Time between moves (e.g., 0.15s = 150ms)
} GameActionRepeat;

typedef struct {
  Snake snake;
  GameActionRepeat move_forward;
} GameState;

void game_init(GameState *state) {
  state->move_forward.timer = 0.0f;
  state->move_forward.interval = 0.15f;  // 6.67 moves/second at start
}

void game_update(GameState *state, float delta_time) {
  state->move_forward.timer += delta_time;

  if (state->move_forward.timer >= state->move_forward.interval) {
    // Subtract (not zero!) to keep remainder for precision
    state->move_forward.timer -= state->move_forward.interval;

    // Execute discrete move
    snake_move(&state->snake);
  }
}
```

**Why this timing pattern?**

- **Frame-rate independent:** Works identically at 30fps, 60fps, 144fps
- **Sub-frame precision:** If `delta_time = 0.0167s` (60fps) and `interval = 0.15s`, after 9 frames `timer = 0.1503s`—the extra 0.0003s is preserved, not lost
- **Speed-up trivial:** Just decrease `interval`. Example: `interval *= 0.9` = 10% faster. After 10 apples, snake moves at 2.3× original speed.

**Comparison to frame-based timing:**

```c
// ❌ BAD: Frame counting (breaks if FPS changes)
if (frame_count % 10 == 0) snake_move();
// At 60fps: moves every 10 frames = 6 moves/second
// At 30fps: moves every 10 frames = 3 moves/second (half speed!)

// ✅ GOOD: Time-based (always 6.67 moves/second)
timer += delta_time;
if (timer >= 0.15f) { timer -= 0.15f; snake_move(); }
```

---

**4. Turn Buffering (Input Smoothing)**

```c
void game_update(GameState *state, GameInput *input, float delta_time) {
  Snake *snake = &state->snake;

  // Detect turn input (only on key press, not hold)
  if (input->turn_left.ended_down &&
      input->turn_left.half_transition_count > 0) {
    SNAKE_DIR proposed = turn_left(snake->direction);

    // Reject 180° turns (would hit own neck)
    if (!is_opposite_direction(snake->direction, proposed)) {
      snake->next_direction = proposed;  // Buffer turn
    }
  }

  if (input->turn_right.ended_down &&
      input->turn_right.half_transition_count > 0) {
    SNAKE_DIR proposed = turn_right(snake->direction);

    if (!is_opposite_direction(snake->direction, proposed)) {
      snake->next_direction = proposed;
    }
  }

  // Movement timing (every 0.15s or so)
  state->move_forward.timer += delta_time;
  if (state->move_forward.timer >= state->move_forward.interval) {
    state->move_forward.timer -= state->move_forward.interval;

    // APPLY buffered turn here (not at key press!)
    snake->direction = snake->next_direction;

    // Then move in new direction
    int dx = DIRECTION_X[snake->direction];
    int dy = DIRECTION_Y[snake->direction];
    // ... rest of movement logic
  }
}
```

**Why buffer turns?**

**Problem:** If you apply direction change immediately on key press:

```
Frame 1: Snake moving RIGHT, player presses LEFT
  → direction = LEFT (instant change)
  → Next frame, head moves LEFT, collides with neck → DEATH
```

**Solution:** Store pending direction in `next_direction`, apply only when snake actually moves:

```
Frame 1: Snake moving RIGHT, player presses LEFT
  → next_direction = UP (buffered, not applied yet)
Frame 5: Movement timer triggers
  → direction = next_direction (apply buffered turn)
  → Head moves UP, no collision
```

**Bonus:** This smooths rapid input. If player presses LEFT then RIGHT quickly (within one movement step), only the last input (RIGHT) is applied. Feels responsive, not jittery.

---

**5. Collision Detection (Simple Linear Search)**

```c
bool check_self_collision(Snake *snake, int new_x, int new_y) {
  // Iterate through active segments (from tail to head)
  int i = snake->tail;
  int segments_checked = 0;

  while (segments_checked < snake->length - 2) {  // Skip head + 1 segment
    if (snake->segments[i].x == new_x && snake->segments[i].y == new_y) {
      return true;  // Collision!
    }

    i = (i + 1) % MAX_SNAKE;  // Ring buffer iteration
    segments_checked++;
  }

  return false;  // No collision
}
```

**Why linear search is fine:**

- Snake has max ~400 segments (typical game: 50-100)
- Collision check = ONE comparison per segment = ~100 comparisons/frame
- Modern CPU: ~4 billion operations/sec. 100 comparisons = nothing.

**When to use spatial grid:** Only if:

- Entities >> 1000 **AND**
- Checks per frame >> 10,000

Snake doesn't meet either threshold. **Premature optimization avoided.**

---

#### 🏗️ Architecture Decisions

| Decision                            | Reason                                                                                                                                                                                                                                              |
| ----------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Ring buffer over dynamic array**  | Snake body = fixed capacity, FIFO order, no individual deletes. Ring buffer = all operations O(1), no realloc, no memmove, cache-friendly. Dynamic array would require `memmove` on remove (O(n) every frame).                                      |
| **Direction enum 0-3**              | Enables modular arithmetic for rotation: `(dir + 1) % 4` = turn right. No switch statement needed. Pattern inspired by unit circle / quaternion rotations, but integer math only.                                                                   |
| **Turn buffering (next_direction)** | Prevents 180° instant reversal (would cause death). Also smooths rapid input—applies last input within movement window. Pattern from fighting game input buffers.                                                                                   |
| **Delta time accumulation**         | Frame-rate independence. Works identically at 30/60/144fps. Sub-frame precision (remainder preserved). Same pattern as Tetris piece dropping, audio sample generation.                                                                              |
| **Y-down tile coordinates**         | Snake is discrete grid (tiles, not meters). No unit conversion needed. Matches screen pixels directly. Exception to Y-up rule (see Sugar Sugar lesson: discrete grids → Y-down, continuous worlds → Y-up).                                          |
| **Double-buffered input**           | Detect transitions (just pressed vs held). Need current frame + previous frame to calculate `half_transition_count`. Pattern from Handmade Hero Day 10, Tetris input refactor.                                                                      |
| **Linear collision search**         | Snake max ~400 segments, linear search = fast enough. No spatial grid needed (premature optimization). If entities >> 1000, THEN consider grid.                                                                                                     |
| **No auto-repeat on movement**      | Snake always moves forward automatically (`move_forward` timer is always running). Turns are discrete (one press = one 90° turn). No DAS/ARR needed (unlike Tetris). Auto-repeat would feel wrong—snake movement is constant, not player-triggered. |
| **Rejection of opposite direction** | If moving RIGHT (1), can't turn LEFT (3) directly—that's `(1 + 2) % 4 = 3`. Must turn UP or DOWN first. Prevents instant self-collision.                                                                                                            |
| **Growth by skipping tail advance** | Normal move: advance head, advance tail (length unchanged). Apple eaten: advance head, DON'T advance tail (length++). Elegant—no "insert segment" logic, ring buffer naturally handles growth.                                                      |
| **Fixed `MAX_SNAKE` capacity**      | Grid is 60×20 = 1200 tiles. Snake can theoretically fill entire grid. `MAX_SNAKE = 1200` handles worst case. No dynamic allocation needed—stack-allocated array works fine.                                                                         |

---

#### 🐛 Bugs & Pitfalls

| Issue                                           | Cause                                                  | Fix                                                                   |
| ----------------------------------------------- | ------------------------------------------------------ | --------------------------------------------------------------------- | --- | --------------------- |
| **Snake eats itself on 180° turn**              | Applied direction change immediately on key press      | Buffer turn in `next_direction`, apply only at movement step          |
| **Rapid Left+Right causes jitter**              | Both turns applied within one frame                    | Only apply last buffered direction at movement step                   |
| **Ring buffer overflow (head == tail)**         | Didn't check `length < MAX_SNAKE` before growing       | Add check before eating apple: `if (length >= MAX_SNAKE) return;`     |
| **Collision check includes head**               | Checked all segments including snake->head             | Skip last 2 segments: `while (checked < length - 2)`                  |
| **Movement stutters at high FPS**               | Used `if (timer >= interval)` but zeroed timer         | Subtract interval instead: `timer -= interval` (keeps remainder)      |
| **Direction vector wrong (UP = +Y not -Y)**     | Forgot coordinate system is Y-down                     | Double-check: UP = (0, -1), DOWN = (0, 1), grid is [0..H-1] from top  |
| **Snake wraps around screen edges by accident** | Didn't clamp `new_x` / `new_y` to grid bounds          | Add bounds check before move: `if (x < 0                              |     | x >= W) game_over();` |
| **Timer precision loss after 1000 moves**       | Zeroed timer on trigger instead of subtracting         | Always subtract: `timer -= interval`. After 1000 steps, error < 0.01s |
| **Frame-perfect input lost**                    | Only checked `ended_down`, not `half_transition_count` | Use both: `ended_down && half_transition_count > 0` = "just pressed"  |
| **Rendering flickers at screen edge**           | Drew cells at grid coords without offset for header    | Add header offset: `screen_y = (grid_y * CELL_SIZE) + HEADER_HEIGHT`  |

---

#### ✅ Skills Strengthened

- ✅ **Ring buffer implementation** — fixed-capacity FIFO queue with head/tail indices and modulo wrapping
- ✅ **Modular arithmetic for rotation** — `(dir + 1) % 4` pattern, inspired by quaternions but integer-only
- ✅ **Delta time accumulation** — frame-independent timing, sub-frame precision via remainder preservation
- ✅ **Direction state machine** — enum-based direction control, relative turns (CW/CCW) instead of absolute
- ✅ **Turn buffering** — input smoothing via `next_direction`, applied at discrete movement steps
- ✅ **Double-buffered input** — detecting transitions (just pressed vs held) using current + old frame state
- ✅ **Collision detection** — linear search through ring buffer, self-collision and bounds checking
- ✅ **Game loop synchronization** — linking input detection, movement timing, and rendering in deterministic order
- ✅ **Y-down coordinate system** — tile-based discrete grid, exception to Y-up rule (matches screen pixels directly)
- ✅ **Data-oriented ring buffer** — contiguous memory layout, cache-friendly iteration for rendering/collision
- ✅ **Growth without reallocation** — snake length increases by NOT advancing tail, no special insert logic

---

#### 🔍 What This Taught Me for Handmade Hero

**1. Ring Buffers Are Everywhere**

Snake's body is a textbook ring buffer. Now I recognize this pattern **all over Handmade Hero**:

- **Audio ring buffer (Day 7-10):** Platform writes samples into ring, sound card consumes from other end
- **Command history:** Store last N player commands for replay/undo
- **Frame timing log:** Store last 60 frame times for FPS graph

The `(index + 1) % capacity` pattern is universal. Snake was the simplest possible introduction.

**2. Direction as Integer Rotation**

Casey doesn't explicitly do this in early Handmade Hero (he uses velocity vectors), but the **modular arithmetic rotation pattern** appears later when implementing projectile angles or camera rotation. By learning it in Snake (where it's simple), I'll recognize it immediately when Casey uses it for more complex systems.

**3. Turn Buffering = Input Lag Mitigation**

Snake's `next_direction` buffer is a **1-frame input buffer**—stores pending input, applies it when system is ready. This same pattern appears in Handmade Hero when Casey implements:

- **Attack buffering:** Store "attack pressed" even if animation hasn't finished, execute when ready
- **Jump buffering:** Store "jump pressed" for a few frames, execute if player lands within that window (feels responsive)

Fighting games use 10-20 frame buffers for special moves. Snake uses 1-frame buffer. Same concept, different scale.

**4. Delta Time Accumulation is The Way**

Every detour project (Tetris, Snake, Sugar Sugar) uses the **exact same timing pattern:**

```c
timer += delta_time;
if (timer >= interval) { timer -= interval; action(); }
```

This is Casey's core game loop pattern (Handmade Hero Day 17). By seeing it in 3 different contexts, I've **internalized** it:

- **Tetris:** Piece drop interval
- **Snake:** Movement interval
- **Sugar Sugar:** Particle spawn interval
- **Handmade Hero:** Entity update intervals, animation timing, cooldowns

It's not just "a pattern"—it's **THE pattern** for time-based simulation.

**5. Ring Buffer + Modulo = Zero Branches**

Snake movement logic has **zero if statements** in the hot path:

```c
head = (head + 1) % MAX_SNAKE;  // No if (head >= MAX_SNAKE) head = 0;
direction = (direction + 1) % 4;  // No if (direction > 3) direction = 0;
```

Casey emphasizes **branchless code** for performance (CPUs love predictable execution). This is the first time I **felt** why modulo is better than conditionals:

- Faster (no branch prediction penalty)
- Fewer lines (less cognitive load)
- More obvious (intent is clear: wrap)

When I return to Handmade Hero, I'll watch for Casey's branchless patterns. I now know **why** he structures loops that way.

**6. Data-Oriented Beats Object-Oriented**

Snake could be implemented as:

```c
// OOP style (BAD)
typedef struct Segment { int x, y; struct Segment *next; } Segment;
Segment *head;  // Linked list
```

Instead, I used:

```c
// Data-oriented style (GOOD)
Segment segments[MAX_SNAKE];  // Flat array
int head, tail;
```

**Why better?**

- **Cache-friendly:** All segments contiguous in memory
- **Predictable:** No pointer chasing, CPU prefetcher can load next segment
- **Simple:** No malloc/free, no memory leaks, no fragmentation

This is **Casey's entire philosophy** in Handmade Hero. By experiencing the performance win firsthand (Snake runs at 60fps with zero hiccups, even with 400 segments), I understand **why** he avoids pointers.

---

**Overall:** Snake solidified three patterns I'll see again and again in Handmade Hero:

1. **Ring buffers** (audio, input history, frame logs)
2. **Delta time accumulation** (every timed system)
3. **Data-oriented contiguous arrays** (entities, particles, tiles)

The detour isn't just practice—it's **building muscle memory** so I can follow Casey's more complex implementations without getting lost in syntax. By Day 33+, I'll recognize these patterns instantly.

### 🧪 Detour Session 3: Handmade Hero + Snake Audio Integration — PCM Mixer Architecture & Platform Audio Abstraction

**Focus:** Building a production-grade audio system with proper abstraction layers, click-free waveform generation, sample-accurate sequencing, and dual-backend support (X11/ALSA + Raylib), then applying those patterns to integrate music and sound effects into the Snake game.

---

#### 🗓️ Commits

| Date       | Commit    | What Changed                                                                                                                              | Why I Designed It This Way                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| ---------- | --------- | ----------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 2026-03-05 | `ca05286` | Desktop Tower Defense course planning complete (21 lessons, full topic inventory, skill map)                                              | **Meta-work before implementation:** Wrote comprehensive course plan for DTD covering BFS pathfinding, tower types, wave system, economy, and audio. This planning phase ensures every concept has a home before writing a single line of code. Pattern learned from Tetris/Snake courses—planning prevents architectural refactors mid-build. DTD will become the reference for "complex state machine + data-driven gameplay" when I return to Handmade Hero's entity system (Days 40+).                               |
| 2026-03-05 | `d63a936` | Snake audio system complete: sequencer, SFX, dual-backend integration, smoothstep fade curves                                             | **Sample-accurate sequencing:** Moved sequencer advancement from `game_audio_update()` (frame-based) to `game_get_audio_samples()` (sample-based). This eliminates timing drift—notes trigger at exact sample boundaries, not approximate frame boundaries. **Smoothstep fade:** Replaced linear ramps with `t²(3-2t)` S-curve—eliminates derivative discontinuity at fade endpoints (removes audible clicks on REST notes). **Dual backend verified:** Both X11/ALSA and Raylib now produce identical click-free audio. |
| 2026-03-07 | `dcc3e1b` | Snake platform refactor: unified `platform.h`, `GameProps` ownership, audio buffer allocation in platform init                            | **Ownership clarity:** Audio buffer was allocated in engine (engine.c) but passed to game—messy ownership. Now platform owns `GameProps` (contains `backbuffer` + `audio`), allocates both at init, passes to game. Game never allocates; platform never touches game state. Clean separation. **Bug fix:** Added missing `#define _POSIX_C_SOURCE 200809L` to X11 backend (required for `nanosleep` on strict POSIX). This is the kind of portability detail Casey handles in Handmade Hero's platform layer.           |
| 2026-03-07 | `6419c0f` | Engine audio architecture documentation: 1010-line guide coveringlatency model, click prevention, format negotiation, backend differences | **Knowledge capture before it's forgotten:** Wrote comprehensive audio architecture doc explaining ALSA ring buffer vs Raylib push model, `sample_clock` for A/V sync, why `is_initialized` is load-bearing, how to add SDL3/WASAPI/CoreAudio backends. This is the "missing manual" for the engine's audio—without it, adding a third backend would require reverse-engineering the existing two. Pattern from Casey: document architectural decisions, not just API signatures.                                        |
| 2026-03-07 | `a96b756` | Handmade Hero game: audio debug fix, swapped platform-specific calls for portable `game_state->audio` properties                          | **Layering violation fixed:** Game adapter code (keyboard.c) was calling `linux_debug_audio_latency()` directly—hardcoded X11 dependency. Now reads `game_state->audio.samples_per_second` and `game_state->audio.is_initialized` (portable fields). Raylib adapter does same. **Pattern:** Debug info belongs in shared contract (`GameAudioOutputBuffer`), not platform-private structs. This is the same lesson as `PlatformAudioConfig` removal—keep platform-specific state private.                                |

---

#### 🎮 Reverse Engineering Notes

| Mechanic                          | Observed Behavior                                                                                      | Implementation Hypothesis                                                                                                                                                                                                                                                   |
| --------------------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Click-free music transitions**  | Original Snake (if it had music) would not click when changing notes—smooth legato between steps       | **Smoothstep S-curve fade:** `t²(3-2t)` produces zero derivative at both endpoints (t=0, t=1). Linear ramps have discontinuous derivative—instant velocity change = click. Smoothstep = continuous velocity = no click. Same principle as Photoshop's "ease in/out" curves. |
| **Sample-accurate note timing**   | Music sequencers (DAWs, trackers) trigger notes at exact sample positions, not approximate frame times | **Sequencer in PCM loop:** Advance `step_samples_remaining` inside `game_get_audio_samples()`, not in `game_audio_update()`. Frame-based sequencing drifts by ±8ms per note (half a frame at 60fps). Sample-based = zero drift.                                             |
| **Dual audio backend support**    | Games ship with one audio backend, or use middleware (FMOD, Wwise) to hide differences                 | **Abstract `AudioOutputBuffer` contract:** Game code only touches 4 fields (`samples`, `samples_per_second`, `sample_count`, `is_initialized`). ALSA ring buffer and Raylib push buffer are 100% hidden. Adding SDL3 backend = implement 4 platform functions, done.        |
| **Platform-independent debug UI** | Casey's Handmade Hero debug overlay works on Win32, Linux, macOS without platform-specific code        | **Shared debug fields in contract:** `AudioOutputBuffer` should expose `sample_clock`, `buffer_fill_percent`, `latency_ms`—populated by backend, read by game overlay. No `#ifdef PLATFORM_X11` in game code. Currently missing—added to architecture doc's "What's next".  |

**Key Implementation Insight:** The "audio just works" feeling comes from **three layers working together but never crossing boundaries:**

1. **Platform layer** (ALSA/Raylib) owns hardware, fills `AudioOutputBuffer`, increments `sample_clock`
2. **Shared contract** (`AudioOutputBuffer`) is the **only** thing both layers touch
3. **Game layer** reads contract, writes interleaved `i16` stereo, never calls `snd_pcm_writei` or `UpdateAudioStream`

If any layer knows about the other two, the abstraction has failed. This is the same principle as Casey's `platform_api.h` in Handmade Hero—thin contract, thick implementations.

---

#### 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       AUDIO SYSTEM ARCHITECTURE                             │
│         (Three-layer model: Platform → Contract → Game)                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │  PLATFORM LAYER (Backend-specific, NOT visible to game)            │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │  X11/ALSA:                                                          │   │
│  │    snd_pcm_t *g_pcm;                  // ALSA handle               │   │
│  │    int latency_samples;               // Ring buffer headroom      │   │
│  │    int running_sample_index;          // A/V sync cursor           │   │
│  │                                                                     │   │
│  │  Raylib:                                                            │   │
│  │    AudioStream g_audio_stream;        // Raylib double-buffer      │   │
│  │    int buffer_size_frames;            // Fixed at init (4096)      │   │
│  │                                                                     │   │
│  │  Both implement 4 functions:                                       │   │
│  │    platform_audio_init(&audio_output, sample_rate, update_hz)     │   │
│  │    platform_audio_shutdown(&audio_output)                          │   │
│  │    platform_audio_get_samples_to_write(&audio_output) → count     │   │
│  │    platform_audio_send_samples(&audio_output)                      │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                              ↓                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │  SHARED CONTRACT (AudioOutputBuffer — visible to both)             │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │  typedef struct {                                                   │   │
│  │    i16  *samples;            // Interleaved stereo i16 buffer      │   │
│  │    i32   samples_per_second; // e.g., 48000                        │   │
│  │    i32   sample_count;       // Frames to generate THIS call       │   │
│  │    i32   max_sample_count;   // Buffer capacity (never exceed)     │   │
│  │    bool  is_initialized;     // Backend ready? (device changes)    │   │
│  │    // Missing (added in arch doc): u64 sample_clock;               │   │
│  │  } AudioOutputBuffer;                                               │   │
│  │                                                                     │   │
│  │  Platform sets:  samples_per_second, sample_count, is_initialized  │   │
│  │  Game reads:     samples_per_second, sample_count, is_initialized  │   │
│  │  Game writes:    samples[0..sample_count*2-1]  (stereo pairs)      │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                              ↓                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │  GAME LAYER (Platform-independent, never touches ALSA/Raylib)      │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │  game_get_audio_samples(GameState *s, AudioOutputBuffer *out) {    │   │
│  │    i16 *samples = out->samples;                                     │   │
│  │    i32 count    = out->sample_count;                                │   │
│  │    f32 inv_sr   = 1.0f / out->samples_per_second;                  │   │
│  │                                                                     │   │
│  │    for (int i = 0; i < count; i++) {                               │   │
│  │      float L = 0.0f, R = 0.0f;                                     │   │
│  │                                                                     │   │
│  │      // Mix SFX voices (sample-accurate, independent fade envelopes) │   │
│  │      for (int v = 0; v < MAX_SFX; v++) {                           │   │
│  │        if (!sfx[v].active) continue;                               │   │
│  │        float wave = (sfx[v].phase < 0.5f) ? 1.0f : -1.0f;          │   │
│  │        float env  = compute_envelope(&sfx[v]);                     │   │
│  │        float samp = wave * sfx[v].volume * env;                    │   │
│  │        L += samp * sfx[v].pan_left;                                │   │
│  │        R += samp * sfx[v].pan_right;                               │   │
│  │        sfx[v].phase += sfx[v].frequency * inv_sr;                  │   │
│  │        if (sfx[v].phase >= 1.0f) sfx[v].phase -= 1.0f;             │   │
│  │        sfx[v].samples_remaining--;                                 │   │
│  │      }                                                              │   │
│  │                                                                     │   │
│  │      // Mix music sequencer (sample-accurate step advancement)     │   │
│  │      if (music.is_playing) {                                       │   │
│  │        if (music.step_samples_remaining-- <= 0) {                  │   │
│  │          advance_to_next_note(&music);  // Trigger at EXACT sample │   │
│  │          music.step_samples_remaining = music.step_duration_samples; │ │
│  │        }                                                            │   │
│  │        float wave = (music.tone.phase < 0.5f) ? 1.0f : -1.0f;      │   │
│  │        float samp = wave * music.tone.current_volume;              │   │
│  │        L += samp; R += samp;                                        │   │
│  │        music.tone.phase += music.tone.frequency * inv_sr;          │   │
│  │        if (music.tone.phase >= 1.0f) music.tone.phase -= 1.0f;     │   │
│  │      }                                                              │   │
│  │                                                                     │   │
│  │      samples[i*2+0] = clamp_i16(L * master_volume * 16000.0f);     │   │
│  │      samples[i*2+1] = clamp_i16(R * master_volume * 16000.0f);     │   │
│  │    }                                                                │   │
│  │  }                                                                  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Main Loop Integration:**

```
Every frame (both X11 and Raylib):

1. platform_audio_get_samples_to_write(&game.audio) → n
   ↓
2. If n > 0:
     game.audio.sample_count = n
     game_get_audio_samples(&game, &game.audio)
   ↓
3. platform_audio_send_samples(&game.audio)
   ↓
4. Backend increments sample_clock (future):
     audio_output->sample_clock += audio_output->sample_count
```

**Why This Architecture is Correct:**

| Design Principle                      | How Snake Audio Implements It                                                                                                                                                                                                   | What Goes Wrong If You Violate It                                                                                                                                                                             |
| ------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Platform owns hardware**            | ALSA code never calls game functions; Raylib code never calls game functions. They only fill `AudioOutputBuffer` and call `game_get_audio_samples()` through a function pointer (or direct call if not hot-reloading).          | If game calls `snd_pcm_writei` directly → X11-only builds, Raylib port breaks.                                                                                                                                |
| **Contract is minimal**               | Only 5 fields in `AudioOutputBuffer`. No `latency_samples`, no `running_sample_index`, no backend-specific config.                                                                                                              | `PlatformAudioConfig` in old engine had ALSA-specific fields → Raylib backend had to carry dead fields. Fixed by removing it entirely.                                                                        |
| **Game never allocates audio memory** | Platform allocates `game.audio.samples` at init. Game writes to it. Platform frees at shutdown. Game's `GameAudioState` is just oscillator/voice state (phases, volumes, timers)—allocated with `GameState` in frame allocator. | If game calls `malloc` for audio buffer → hot-reload wipes it, audio dies mid-session.                                                                                                                        |
| **Timing source is hardware**         | Platform increments `sample_clock` (future) or maintains `running_sample_index` (ALSA). Game derives time from samples, not frame timer.                                                                                        | If game uses frame timer for audio events → timing drifts under load. Subtitles, cutscenes, animation sync all break.                                                                                         |
| **No platform #ifdef in game code**   | `game_get_audio_samples()` has zero `#ifdef PLATFORM_X11`. It reads `AudioOutputBuffer` fields only.                                                                                                                            | If game has `#ifdef PLATFORM_X11 snd_pcm_something()` → every new platform requires game code changes. SDL3 port = modify game layer. Wrong.                                                                  |
| **Debug data flows through contract** | (Currently missing, but documented) `AudioOutputBuffer.latency_ms`, `.buffer_fill_percent` would be set by backend, read by game debug overlay.                                                                                 | If debug overlay calls `linux_debug_audio_latency()` directly → Raylib build can't show debug (or worse, crashes). Fixed in commit `a96b756` by reading `game_state->audio` fields instead of platform calls. |
| **Backend state is opaque**           | `LinuxAudioConfig` lives in `platforms/x11/audio.h`, not `platforms/_common/config.h`. Raylib has `g_audio_stream` in `main_raylib.c` as a static. Neither is visible to game.                                                  | If game can see `LinuxAudioConfig` → game code tempted to check `latency_samples` or `running_sample_index` → accidental coupling.                                                                            |

**Comparison to Prior Art:**

| System                                 | How it abstracts audio                                                                                                        | Strengths                                                             | Weaknesses                                                                                 |
| -------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| **Handmade Hero (Days 7-10)**          | `platform_api.h` defines `game_get_sound_samples()`. Win32 fills DirectSound ring buffer. Linux/SDL versions follow same API. | Identical to Snake's approach—thin contract, thick backends.          | Casey doesn't document _why_ the layers are split this way—you learn by watching.          |
| **Raylib (monolithic)**                | `InitAudioDevice()` + `PlaySound()`. Single backend (miniaudio) handles all platforms.                                        | Simple for users—no abstraction needed.                               | Can't swap backends; hard to add custom mixers; monolithic = hard to understand internals. |
| **SDL2 audio**                         | `SDL_AudioSpec` + callback. `SDL_OpenAudioDevice()` abstracts ALSA/PulseAudio/CoreAudio/WASAPI.                               | Mature, battle-tested, handles device hot-plug correctly.             | Callback runs on audio thread → game must be thread-safe. Snake avoids this (main-thread). |
| **FMOD / Wwise (middleware)**          | High-level API (`playSound()`, `setVolume()`). Black-box mixer handles everything.                                            | Feature-rich (3D audio, DSP, streaming, compression).                 | Closed-source, expensive licenses, massive DLLs. Overkill for Snake.                       |
| **stb_vorbis + custom mixer**          | Decode OGG to PCM, mix in game code, push to platform. (Pattern used in many indie games.)                                    | Full control, deterministic, educational.                             | Must write your own mixer, sequencer, effects. Snake does this—intentionally educational.  |
| **OpenAL (deprecated but still used)** | 3D positional audio API. `alGenSources()`, `alSourcePlay()`. Backends for many platforms.                                     | Good for FPS games (3D sound).                                        | API design pre-dates modern low-latency requirements. Dated.                               |
| **Snake audio architecture**           | `AudioOutputBuffer` contract + platform-specific ring buffer (ALSA) or push buffer (Raylib). Game fills PCM samples.          | Clean layers, no middleware, fully deterministic, easy to understand. | Manual—game must implement mixer, sequencer, SFX player. Not a weakness for learning.      |

---

#### 🎯 Core Concepts

| Concept                                  | Implementation                                                                                     | What I Learned                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| ---------------------------------------- | -------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Smoothstep Fade Curves**               | `t²(3-2t)` instead of linear `t`                                                                   | **Click elimination via calculus:** Linear fade = instant velocity change at endpoints → discontinuous derivative → audible click. Smoothstep S-curve = zero derivative at t=0 and t=1 → continuous acceleration → no click. Same principle as Bezier easing in animation. **Why games use it:** CSS `ease-in-out`, Photoshop fade, Unity animation curves—all use S-curves for smooth organic motion. Snake music now has this—original used linear (clicked on every REST note).                                                                                                                               |
| **Sample-Accurate Sequencing**           | Advance sequencer inside `game_get_audio_samples()`, not `game_audio_update()`                     | **Frame-based = drift:** At 60fps, each frame = 800 samples at 48kHz. If sequencer ticks once per frame, note timing has ±400 sample jitter (±8ms). **Sample-based = exact:** Decrement `step_samples_remaining` inside the PCM loop—notes trigger at exact sample boundaries. After 100 notes, frame-based sequencer drifts by ~800ms. Sample-based = zero drift. **Pattern from:** DAWs (Ableton, FL Studio), game audio middleware (Wwise Event system), rhythm games (Guitar Hero note timing).                                                                                                              |
| **Audio Buffer Ownership**               | Platform allocates `audio.samples`, game writes to it, platform frees it                           | **Hot-reload safety:** If game allocates audio buffer with `malloc`, hot-reload wipes the pointer → audio dies. If platform allocates, buffer survives reload. **Memory lifetime rule:** Whoever creates the memory owns it. Platform created window/audio device → platform owns buffers for them. Game created entity pools → game owns them. **Casey's pattern:** `platform_api.h` defines buffer allocation; game never calls `malloc` for cross-boundary data.                                                                                                                                              |
| **AudioOutputBuffer as Contract**        | Only 5 fields; no backend-specific state                                                           | **Thin interface principle:** The fewer fields in shared contract, the easier it is to add new backends. `AudioOutputBuffer` has 5 fields. Adding SDL3 backend = implement 4 functions (`init`, `shutdown`, `get_samples_to_write`, `send_samples`). Done. **Anti-pattern:** Old `PlatformAudioConfig` had 8 fields, 4 ALSA-specific → Raylib backend carried dead weight. **Result:** Contract refactor (removed `PlatformAudioConfig` entirely) made Raylib backend 30% smaller.                                                                                                                               |
| **Platform-Independent Debug**           | Debug overlay reads `audio.samples_per_second`, `.is_initialized` (shared fields), not backend API | **Layering violation caught:** keyboard.c called `linux_debug_audio_latency()` directly—X11-specific function in game adapter code. **Fix:** Read `game_state->audio.samples_per_second` instead (portable field). Now Raylib build can show same debug info. **Pattern:** If debug code has `#ifdef PLATFORM_X`, you've coupled debug to platform. Move debug-relevant data to shared contract, populate from backend, read from game.                                                                                                                                                                          |
| **Why `is_initialized` is Load-Bearing** | Check `audio->is_initialized` before writing samples; backend flips it on device change            | **Device hot-plug scenario:** User unplugs headphones mid-game → audio device changes → backend tears down, re-inits → `is_initialized` flips `false` → `true`. If game doesn't check `is_initialized`, writing to `audio->samples` while backend is re-initializing = undefined behavior (crash or garbage audio). **Platform differences:** Windows/macOS fire device-change events constantly (Bluetooth, USB DACs). ALSA/Raylib don't surface this easily, but the check **must** be there for correctness. Documented in arch doc's "Missing: Device hot-plug handling".                                    |
| **Backend Latency Models**               | ALSA: write-ahead ring buffer with `latency_samples`. Raylib: push to double-buffer, no control.   | **ALSA model:** You write samples ahead of playback cursor. `latency_samples = sample_rate / update_hz * 3` = ~100ms headroom at 30Hz. You query `snd_pcm_avail_update()` → write exactly that many samples → hardware never starves. **Raylib model:** You write fixed `buffer_size_frames` (4096) per `IsAudioStreamProcessed()` call. Raylib's internal thread handles timing. You can't query exact latency—just write when asked. **Consequence:** ALSA requires `running_sample_index` for A/V sync. Raylib can't provide sample-accurate sync without adding `sample_clock` to contract (future work).    |
| **Click Prevention Ladder**              | Three levels: DC offset removal → envelope fade → S-curve smoothing                                | **Level 1 (DC offset):** If waveform average ≠ 0 over time, speaker cone drifts → pop when sound stops. Solution: high-pass filter or ensure symmetric waveform. Square wave is symmetric → no DC. **Level 2 (Linear fade):** Instant on/off = discontinuity → click. Fade in/out over ~2ms = smooth amplitude → no pop. **Level 3 (S-curve):** Linear fade still has discontinuous _velocity_ → audible artifact on fast note changes. Smoothstep = continuous velocity → completely smooth. **Snake uses all three:** Square wave (symmetric), fade envelope (2ms), smoothstep curve (S). Result: zero clicks. |
| **Why `pitch` != `frequency`**           | Pitch is musical (A4 = 440 Hz). Frequency is oscillator speed (cycles/second).                     | **MIDI note math:** `frequency = 440 × 2^((note - 69) / 12)`. A4 = MIDI 69 = 440 Hz. Every 12 semitones = octave = 2× frequency. **Why games store frequency, not MIDI:** Oscillator phase accumulator needs Hz, not note number. Convert once at note trigger, store Hz. **Pitch bends / vibrato:** Modulate frequency ±5% around base. Storing MIDI note doesn't help—you'd convert every sample. Store Hz, modulate directly.                                                                                                                                                                                 |
| **`AudioOutputBuffer.sample_clock`**     | (Missing, documented) Backend increments by `sample_count` after each fill                         | **A/V sync use case:** Display frame at time `t = sample_clock / sample_rate`. Subtitle appears at sample 96000 → display at 2.0s (48kHz). **Without it:** Game uses frame timer → `t = frame_count * (1.0 / 60.0)` → drifts under load. After 10 minutes, subtitles are 2 seconds late. **With it:** Hardware-accurate time, zero drift. **Why not added yet:** Requires touching both backends simultaneously. Documented in arch doc so it's not forgotten when next backend (SDL3) is added.                                                                                                                 |

---

#### 💻 Key Code Snippets

**1. Smoothstep Fade Curve (Click Elimination)**

```c
/* games/snake/src/game/audio.c — Smoothstep S-curve for fade in/out */

/* PROBLEM: Linear fade produces audible clicks on REST note transitions.
 *
 * Root cause: Linear ramp y = t has discontinuous derivative at endpoints:
 *   At t=0: dy/dt jumps from 0 (silence) to 1 (full ramp rate) → click
 *   At t=1: dy/dt jumps from 1 (ramp) to 0 (target reached) → click
 *
 * SOLUTION: Smoothstep S(t) = t²(3 - 2t) has:
 *   S'(0) = 0  (smooth start)
 *   S'(1) = 0  (smooth end)
 *   S'(0.5) = 1.5 (steeper middle, compensates for slower edges)
 *
 * Result: Continuous velocity → no audible artifact.
 */

if (tone->fade_samples_remaining > 0) {
    float fade_progress = 1.0f - ((float)tone->fade_samples_remaining /
                                  (float)MUSIC_FADE_SAMPLES);

    /* Smoothstep S(t) = t²(3 - 2t) */
    float t = fade_progress;
    float smooth = t * t * (3.0f - 2.0f * t);

    float fade_env;
    if (tone->is_playing) {
        fade_env = smooth;         /* Fade IN:  0 → 1 with S-curve */
    } else {
        fade_env = 1.0f - smooth;  /* Fade OUT: 1 → 0 with inverted S-curve */
    }

    tone->current_volume = tone->volume * fade_env;
    tone->fade_samples_remaining--;
} else {
    tone->current_volume = tone->is_playing ? tone->volume : 0.0f;
}
```

**Why This is Good:**

- **Derivative continuity:** `S'(t) = 6t - 6t²`, which equals 0 at t=0 and t=1. No acceleration spike.
- **No conditional branching per sample:** Compute once per fade frame, not per audio sample.
- **Symmetric curve:** Fade-in and fade-out use same math (one inverted). Less code, less bugs.

**Comparison to broken linear fade:**

```c
/* ❌ BAD: Linear fade (what Snake had before commit d63a936) */
float linear = fade_progress;  /* Just t */
fade_env = is_playing ? linear : (1.0f - linear);
/* Problem: Instant velocity change at t=0 and t=1 → click on every REST */
```

**Data-Oriented Insight:** The fade curve is **stateless**—it's a pure function of `fade_progress`. We don't store 512 pre-computed curve values; we compute `t²(3-2t)` on-demand. This is acceptable because (1) it's 2 multiplies + 1 add (cheap), (2) only computed once per fade frame (1/60s), not per sample (1/48000s). If we needed this per-sample, **then** we'd use a lookup table.

---

**2. Sample-Accurate Sequencer (Zero Timing Drift)**

```c
/* games/snake/src/game/audio.c — Sequencer advancement inside PCM loop */

void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buffer) {
    int16_t *out = buffer->samples;
    int sample_count = buffer->sample_count;
    float inv_sample_rate = 1.0f / (float)buffer->samples_per_second;

    for (int s = 0; s < sample_count; s++) {
        float mixed_left = 0.0f;
        float mixed_right = 0.0f;

        /* ─── Music Sequencer (sample-based timing) ─── */
        if (audio->music.is_playing) {
            MusicSequencer *seq = &audio->music;
            ToneGenerator *tone = &seq->tone;

            /* CRITICAL: Advance step counter INSIDE the sample loop.
             * Decrement happens at EXACT sample boundaries, not approximate
             * frame boundaries. This eliminates timing drift. */
            if (seq->step_samples_remaining <= 0) {
                /* Trigger next note at EXACT sample index */
                uint8_t note = seq->patterns[seq->current_pattern][seq->current_step];

                if (note > 0) {
                    float new_freq = midi_to_freq(note);

                    /* Only reset phase when coming from silence (REST → NOTE).
                     * Continuous notes (NOTE → NOTE) preserve phase for legato. */
                    if (!tone->is_playing || tone->frequency == 0.0f) {
                        tone->phase = 0.0f;  /* Hard reset = note attack */
                        tone->fade_samples_remaining = MUSIC_FADE_SAMPLES;
                    }

                    tone->frequency = new_freq;
                    tone->is_playing = 1;
                } else {
                    /* REST: fade out */
                    tone->is_playing = 0;
                    tone->fade_samples_remaining = MUSIC_FADE_SAMPLES;
                }

                /* Reset step counter (in samples, not frames) */
                seq->step_samples_remaining = seq->step_duration_samples;

                /* Advance to next step */
                seq->current_step++;
                if (seq->current_step >= MUSIC_PATTERN_LENGTH) {
                    seq->current_step = 0;
                    seq->current_pattern = (seq->current_pattern + 1) % MUSIC_NUM_PATTERNS;
                }
            }

            seq->step_samples_remaining--;  /* Decrement EVERY sample */

            /* Generate this sample's waveform using current state */
            if (tone->current_volume > 0.0001f) {
                float wave = (tone->phase < 0.5f) ? 1.0f : -1.0f;
                float sample = wave * tone->current_volume * audio->music_volume;
                mixed_left += sample;
                mixed_right += sample;

                tone->phase += tone->frequency * inv_sample_rate;
                if (tone->phase >= 1.0f) tone->phase -= 1.0f;
            }
        }

        /* … SFX mixing … */

        /* Final output */
        *out++ = audio_clamp_sample(mixed_left * audio->master_volume * 16000.0f);
        *out++ = audio_clamp_sample(mixed_right * audio->master_volume * 16000.0f);
    }
}
```

**Why This is Good:**

- **Zero drift:** After 1000 notes, timing error < 1 sample (0.02ms at 48kHz). Frame-based sequencer drifts by seconds.
- **No race conditions:** Sequencer state (`step_samples_remaining`) is only touched in `game_get_audio_samples()`, which runs on main thread (not audio thread).
- **Deterministic:** Same input state → same audio output, sample-for-sample. Replay recordings are trivial.

**Comparison to frame-based sequencer (broken):**

```c
/* ❌ BAD: Tick sequencer in game_audio_update() (frame-based) */
void game_audio_update(GameAudioState *audio, float dt) {
    audio->music.step_timer += dt;
    if (audio->music.step_timer >= audio->music.step_duration) {
        audio->music.step_timer -= audio->music.step_duration;
        /* Trigger note here */
    }
}
/* Problem: dt varies per frame (16ms ± 2ms at 60fps).
 * After 100 steps, cumulative error = ±200ms.
 * Musical timing sounds "drunk". */
```

**Pattern from:** Every sample-accurate audio system (DAWs, VST plugins, game audio middleware). Casey's Handmade Hero audio (Days 7-10) ticks sound events inside `GameGetSoundSamples()`, not in `GameUpdateAndRender()`.

---

**3. Platform Audio Init (Ownership Clarity)**

```c
/* games/snake/src/platforms/raylib/main.c — Platform owns audio buffer */

static int platform_audio_init(AudioOutputBuffer *audio_buffer) {
    InitAudioDevice();

    if (!IsAudioDeviceReady()) {
        fprintf(stderr, "⚠ Raylib: Audio device not ready\n");
        audio_buffer->is_initialized = false;
        return 1;
    }

    /* Buffer size: 4096 samples ≈ 85 ms at 48 kHz.
     * Raylib's audio thread consumes at ~46.875 Hz (48000/1024), so with a
     * 60 Hz game loop the average is 0.78 buffers/frame. 4096 samples of
     * headroom means starvation requires ~5 missed frames — essentially never. */
    int buffer_size = 4096;
    g_raylib.buffer_size_frames = buffer_size;

    SetAudioStreamBufferSizeDefault(buffer_size);
    g_raylib.audio_stream = LoadAudioStream(audio_buffer->samples_per_second, 16, 2);

    if (!IsAudioStreamValid(g_raylib.audio_stream)) {
        fprintf(stderr, "⚠ Raylib: Failed to create audio stream\n");
        CloseAudioDevice();
        audio_buffer->is_initialized = false;
        return 1;
    }

    /* Pre-fill BOTH buffers with silence (Raylib double-buffers).
     * Without this, the first frame contains garbage → initial click. */
    memset(audio_buffer->samples, 0, buffer_size * 2 * sizeof(int16_t));
    UpdateAudioStream(g_raylib.audio_stream, audio_buffer->samples, buffer_size);
    UpdateAudioStream(g_raylib.audio_stream, audio_buffer->samples, buffer_size);

    PlayAudioStream(g_raylib.audio_stream);

    audio_buffer->is_initialized = true;
    g_raylib.audio_initialized = true;

    printf("═══════════════════════════════════════════════════════════\n");
    printf("🔊 RAYLIB AUDIO\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Sample rate:  %d Hz\n", audio_buffer->samples_per_second);
    printf("Buffer size:  %d samples (%.1f ms)\n", buffer_size,
           buffer_size * 1000.0f / audio_buffer->samples_per_second);
    printf("Initialized:  ✓\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    return 0;
}
```

**Why This is Good:**

- **Ownership clear:** `AudioOutputBuffer *audio_buffer` is allocated by platform (`PlatformGameProps`), passed to this function. Platform owns it; game writes to it.
- **Pre-fill prevents initial click:** Raylib double-buffers internally. If you only fill once, first playback buffer is uninitialized → garbage → loud pop. Fill twice = both buffers silenced.
- **`is_initialized` set last:** Only flip to `true` after **all** init steps succeed. If any step fails, `is_initialized` stays `false` → game writes silence instead of crashing.

**Comparison to bad ownership pattern:**

```c
/* ❌ BAD: Game allocates audio buffer */
void game_init(GameState *s) {
    s->audio.samples = malloc(MAX_SAMPLES * sizeof(i16) * 2);  /* WRONG */
}
/* Problem: Hot-reload wipes game DLL's heap → audio buffer pointer invalid
 * → next audio frame writes to freed memory → crash or garbage. */
```

**Memory lifetime rule:** If data crosses DLL boundary (platform ↔ game), the **non-reloadable side** must own it. Platform never reloads → platform allocates buffers. Game reloads constantly → game must **never** allocate cross-boundary data.

---

**4. Debug Info Through Shared Contract (Layering Fix)**

```c
/* games/handmade-hero-game/src/adapters/x11/inputs/keyboard.c — BEFORE (broken) */
case XK_F1: {
    printf("F1 pressed - showing audio debug\n");
    linux_debug_audio_latency(&platform_state->config.audio);  /* ❌ X11-specific call */
    break;
}

/* games/handmade-hero-game/src/adapters/x11/inputs/keyboard.c — AFTER (fixed) */
case XK_F1: {
    printf("F1 pressed - showing audio debug\n");
    printf("[AUDIO] rate=%d initialized=%d sample_count=%d max=%d\n",
           game_state->audio.samples_per_second,       /* ✓ Portable field */
           (int)game_state->audio.is_initialized,      /* ✓ Portable field */
           game_state->audio.sample_count,             /* ✓ Portable field */
           game_state->audio.max_sample_count);        /* ✓ Portable field */
    break;
}
```

**Why This is Good:**

- **Portable:** Same code works on X11 and Raylib. No `#ifdef PLATFORM_X11`.
- **No coupling:** Game adapter code (`inputs/keyboard.c`) never includes `platforms/x11/audio.h`. Clean layer separation.
- **Backend-agnostic debug:** If we add SDL3 backend, debug output "just works"—no code changes in game layer.

**Pattern from:** Casey's Handmade Hero debug overlay (`DEBUGTextLine()`) reads from `debug_state`, not platform-specific structs. Debug data flows **up** from platform → contract → game, never **down** from game → platform.

---

**5. Audio Contract Allocation (Platform Owns, Game Writes)**

```c
/* games/snake/src/platform.h — Platform allocates audio buffer at init */

typedef struct {
    Backbuffer backbuffer;
    AudioOutputBuffer audio;
} GameProps;

typedef struct {
    const char *title;
    int width, height;
    bool is_running;
    GameProps game;  /* ← Platform owns this entire struct */
} PlatformGameProps;

/* games/snake/src/platforms/raylib/main.c — Platform init allocates */
static inline int platform_game_props_init(PlatformGameProps *props) {
    /* Backbuffer allocation */
    props->game.backbuffer.pixels = (uint32_t *)malloc(props->width * props->height * sizeof(uint32_t));
    if (!props->game.backbuffer.pixels) {
        fprintf(stderr, "❌ Failed to allocate backbuffer\n");
        return 1;
    }

    /* Audio buffer allocation */
    props->game.audio.samples_per_second = 48000;
    int max_samples = (props->game.audio.samples_per_second / 60) * 8;  /* ~6400 samples */
    props->game.audio.samples = (int16_t *)malloc(max_samples * sizeof(int16_t) * 2);  /* Stereo */
    if (!props->game.audio.samples) {
        fprintf(stderr, "❌ Failed to allocate audio buffer\n");
        free(props->game.backbuffer.pixels);
        return 1;
    }

    printf("✓ Audio buffer: %d max samples\n", max_samples);
    return 0;
}

/* games/snake/src/game/main.c — Game writes to buffer, never allocates */
void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buffer) {
    int16_t *out = buffer->samples;  /* ← Buffer owned by platform, passed to game */
    /* … fill samples … */
}
```

**Why This is Good:**

- **Hot-reload safe:** Audio buffer survives game DLL reload—pointer stored in platform struct (never reloaded).
- **No double-free:** Only one `free()` call—in `platform_game_props_free()` at shutdown. Game never calls `free()` on audio buffer.
- **Clear ownership:** `GameProps` is **owned by platform**, **passed to game**. Game is a **user** of the contract, not an **owner**.

**Comparison to old broken pattern (engine):**

```c
/* ❌ OLD (engine/engine.c — before refactor) */
platform->config.audio.buffer_size_bytes = game->config.initial_audio_sample_rate * bytes_per_sample;
/* Problem: Platform config (`platform->config.audio`) contains game-derived values
 * (`game->config.initial_audio_sample_rate`). Circular dependency. */

/* ✓ NEW (Snake platform layer) */
props->game.audio.samples_per_second = 48000;  /* Platform decides sample rate */
props->game.audio.samples = malloc(...);       /* Platform allocates buffer */
/* Game reads sample rate from `AudioOutputBuffer`, writes samples. Clean separation. */
```

---

#### 🏗️ Architecture Decisions

| Decision                                        | Reason                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ----------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Sample-accurate sequencer (not frame-based)** | **Frame-based = drift:** 60fps frame = 800 samples at 48kHz. If sequencer ticks once per frame, note timing has ±8ms jitter (+400 samples per note). After 100 notes, cumulative error = ±800ms. Unplayable. **Sample-based = exact:** Decrement `step_samples_remaining` inside PCM loop—notes trigger at exact sample boundaries. After 1000 notes, error < 1 sample (0.02ms). Pattern from DAWs (Ableton, FL Studio), game audio middleware (Wwise), rhythm games (Guitar Hero).                            |
| **Smoothstep fade curve (not linear)**          | **Linear = click:** `y = t` has discontinuous derivative at endpoints → instant velocity change → audible pop on every REST note. **Smoothstep = smooth:** `y = t²(3-2t)` has `y'(0) = y'(1) = 0` → continuous acceleration → no artifact. Same math as CSS `ease-in-out`, Photoshop fade, Unity animation curves. Snake's music had audible clicks before commit `d63a936`—fixed by replacing linear ramps with smoothstep.                                                                                   |
| **Platform owns audio buffer, game writes it**  | **Hot-reload safety:** If game allocates audio buffer, hot-reload wipes pointer → audio dies. If platform allocates, buffer survives. **Memory lifetime rule:** Cross-boundary data owned by non-reloadable side. Platform never reloads → platform owns `GameProps` (contains `backbuffer` + `audio`). Game reloads constantly → game owns entity pools (`towers[]`, `creeps[]`), allocated from `GameMemory` (also platform-owned). Pattern from Casey's Handmade Hero `platform_api.h`.                     |
| **`AudioOutputBuffer` is minimal (5 fields)**   | **Thin interface = easy to extend:** 5 fields (`samples`, `samples_per_second`, `sample_count`, `max_sample_count`, `is_initialized`) cover 100% of game needs. Adding SDL3 backend = implement 4 platform functions, done. **Anti-pattern:** Old `PlatformAudioConfig` had 8 fields, 4 ALSA-specific (`latency_samples`, `safety_samples`, `running_sample_index`, `total_samples_written`) → Raylib backend carried dead weight. Fix: remove it entirely. Backends store private state in their own structs. |
| **`is_initialized` checked every frame**        | **Device hot-plug correctness:** User unplugs headphones → audio device changes → backend tears down, re-inits → `is_initialized` flips `false` → `true`. If game only checks once at startup, writing to buffer during re-init = crash. **Windows/macOS reality:** Bluetooth headphones, USB DACs trigger device changes constantly. ALSA/Raylib don't surface this easily, but **the check must be there**. Pattern from professional audio software (DAWs, plugins).                                        |
| **No `#ifdef PLATFORM_X11` in game code**       | **Portability:** Game code includes `platform.h` (platform-independent API), never `platforms/x11/audio.h`. If game has `#ifdef PLATFORM_X11 snd_pcm_something()` → every new platform (SDL3, WASAPI) requires modifying game layer. Wrong. **Correct:** Backend-specific code lives in `platforms/x11/`, `platforms/raylib/`. Game sees only `AudioOutputBuffer`. Pattern from Casey's Handmade Hero—game DLL never includes `<windows.h>`.                                                                   |
| **Debug data flows through contract**           | **Broken pattern (commit a96b756 fixed this):** Game adapter called `linux_debug_audio_latency()` directly—X11-specific function in game layer. **Fix:** Read `game_state->audio.samples_per_second` (portable field). Raylib adapter does same. **Future:** Add `AudioOutputBuffer.latency_ms`, `.buffer_fill_percent` (populated by backend, read by game debug overlay). Pattern from Casey's debug overlay—reads from `debug_state`, not platform structs.                                                 |
| **Backends are symmetric (same 4 functions)**   | **Consistency:** Both X11 and Raylib implement: `platform_audio_init()`, `platform_audio_shutdown()`, `platform_audio_get_samples_to_write()`, `platform_audio_send_samples()`. Same function names, same signatures. Adding SDL3 = copy Raylib's function stubs, replace Raylib calls with SDL3 calls. **Anti-pattern:** X11 has `linux_init_audio()`, Raylib has `raylib_audio_setup()` (different names) → confusion when comparing. Symmetry = clarity.                                                    |
| **No threading (main-thread audio fill)**       | **Simplicity:** `game_get_audio_samples()` runs on main thread, called from `platform_audio_update()` in main loop. No mutexes, no atomics, no thread sync. **Trade-off:** Can't use callback-based backends (CoreAudio, SFML `SoundStream`) without ring buffer shim. **Acceptable:** ALSA and Raylib both support push model. If we add CoreAudio later, platform implements lock-free ring buffer—game code unchanged. Pattern from Casey's early Handmade Hero (Days 7-10)—no threading until Day 120+.    |

---

#### 🐛 Bugs & Pitfalls

| Issue                                                   | Cause                                                                                                    | Fix                                                                                                                  | What I Learned                                                                                                                                                                                                                                                                                                  |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Audible click on every REST note**                    | Linear fade curve has discontinuous derivative at endpoints                                              | Replace `fade_env = t` with `fade_env = t²(3-2t)` (smoothstep S-curve). Continuous velocity = no click.              | Calculus matters in audio. Derivative discontinuity = audible artifact. Same principle as Bezier easing in animation—S-curves feel organic because acceleration is continuous.                                                                                                                                  |
| **Music timing drifts by seconds after 100 notes**      | Sequencer ticked in `game_audio_update()` (frame-based), not in PCM loop (sample-based)                  | Move sequencer advancement inside `game_get_audio_samples()`. Decrement `step_samples_remaining` every sample.       | Frame-based timing = cumulative error. Sample-based timing = zero drift. This is why DAWs tick sequencers at audio sample rate, not video frame rate.                                                                                                                                                           |
| **Audio dies after hot-reload**                         | Game allocated audio buffer with `malloc`; hot-reload wiped DLL's heap → pointer invalid                 | Platform allocates buffer in `platform_game_props_init()`. Game writes to it. Platform frees at shutdown.            | Memory lifetime rule: cross-boundary data owned by non-reloadable side. Platform never reloads → platform owns buffers.                                                                                                                                                                                         |
| **Loud pop at audio init**                              | Raylib double-buffers internally; first playback buffer uninitialized if only filled once                | Pre-fill **both** buffers with silence before calling `PlayAudioStream()`. Call `UpdateAudioStream()` twice at init. | Always pre-fill audio buffers with silence. Uninitialized memory = random bits = maximum amplitude noise = speaker damage risk.                                                                                                                                                                                 |
| **X11 build missing `nanosleep`**                       | `#define _POSIX_C_SOURCE 200809L` missing from `main_x11.c`                                              | Add `#define _POSIX_C_SOURCE 200809L` at top of file (before any `#include`).                                        | POSIX feature test macros must be defined **before** including system headers. Without them, strict POSIX functions (`nanosleep`, `clock_gettime`) are not declared. This is a **portability landmine**—works on some Linux distros, breaks on others.                                                          |
| **Raylib adapter calls X11-specific debug function**    | `handleEventKeyPress()` in keyboard.c called `linux_debug_audio_latency()`                               | Read `game_state->audio.samples_per_second` (portable field) instead of calling backend-specific function.           | Layering violation: game adapter (shared across backends) calling platform-specific function. Fix: move debug-relevant data to shared contract (`AudioOutputBuffer`). Pattern from Casey—debug overlay reads from `debug_state`, not platform structs.                                                          |
| **Engine audio init order wrong**                       | `game_audio_init()` called **before** `platform_audio_init()` → `samples_per_second` = 0                 | Call `platform_audio_init()` first (sets `samples_per_second`), **then** `game_audio_init()`.                        | Init order matters when contract is shared. Platform fills contract (`samples_per_second`) → game reads it (`step_duration_samples = 0.15s × sample_rate`). Reverse order = division by zero or garbage timing. Documented in arch doc's boot sequence.                                                         |
| **Desktop Tower Defense audio silence (never started)** | `game_audio_init()` never called in `game_init()`; `game_audio_update()` never called in `game_update()` | Add both calls to DTD codebase. `game_init()` → `game_audio_init()`. `game_update()` → `game_audio_update()`.        | **Wiring bugs = silent failure.** Audio system compiles, loads, runs—but produces no sound because init/update never called. This is why test early: "Does a sine wave play?" If no, check wiring **before** adding SFX/music. Documented in DTD course Lesson 21 as "Critical Wiring" pitfalls.                |
| **`AudioOutputBuffer.sample_clock` missing**            | No field in contract for A/V sync; games re-implement with backend-specific `running_sample_index`       | (Future work) Add `u64 sample_clock` to `AudioOutputBuffer`. Backends increment by `sample_count` after each fill.   | A/V sync requires hardware-accurate time source. Frame timer drifts under load → subtitles late. Sample clock = exact. **Why not added yet:** Requires touching both backends simultaneously. Documented in arch doc so it's not forgotten when SDL3 backend is added. This is a **known gap** in the contract. |

---

#### ✅ Skills Strengthened

- ✅ **Audio contract design** — minimal shared interface (`AudioOutputBuffer`), platform-specific state hidden, dual-backend support (ALSA + Raylib)
- ✅ **Sample-accurate sequencing** — moved sequencer tick from frame-based (`game_audio_update`) to sample-based (`game_get_audio_samples`), eliminated timing drift
- ✅ **Click prevention math** — smoothstep S-curve fade (`t²(3-2t)`) for continuous derivative, zero audible artifacts on note transitions
- ✅ **Hot-reload-safe memory** — platform allocates audio buffer, game writes to it, survives DLL reload
- ✅ **Layering discipline** — fixed debug code calling platform-specific functions, now reads portable `AudioOutputBuffer` fields
- ✅ **Platform audio integration** — ALSA ring buffer (`snd_pcm_writei`, latency model) vs Raylib push buffer (`UpdateAudioStream`, double-buffer pre-fill)
- ✅ **Init sequencing correctness** — documented boot order (platform init → audio init → game init), caught missing calls (`game_audio_init`, `game_audio_update`)
- ✅ **Portability pitfalls** — `_POSIX_C_SOURCE` feature test macros, missing `#include`, strict POSIX compliance
- ✅ **Architectural documentation** — wrote 1010-line audio architecture guide covering latency model, backend differences, format negotiation, missing features
- ✅ **Waveform synthesis** — phase accumulator oscillator, square wave generation, stereo panning, envelope fade-in/fade-out
- ✅ **Audio debugging** — portable debug overlay design, moving debug data from platform-private structs to shared contract

---

#### 🔍 What This Taught Me for Handmade Hero

**1. Why Casey Splits Audio Into Three Layers**

Casey's Handmade Hero audio system (Days 7-10) has the same three-layer split:

1. **Platform layer** (Win32/Linux) fills `game_sound_output_buffer`
2. **Shared contract** (`game_sound_output_buffer`) is the **only** thing both layers touch
3. **Game layer** (`GameGetSoundSamples`) writes PCM samples, never touches DirectSound/ALSA

Before this detour, I understood the **code** but not the **necessity**. Now I've experienced what happens when layers are mixed:

- `PlatformAudioConfig` had ALSA-specific fields → Raylib backend carried dead weight (fixed by removing it)
- Game adapter calling `linux_debug_audio_latency()` → Raylib build couldn't show debug (fixed by reading `AudioOutputBuffer` fields)
- Audio buffer allocated in engine → ownership unclear → risk of hot-reload bugs

Casey's three-layer split isn't over-engineering—it's the **minimum viable abstraction** to support multiple backends without leaking platform-specific details. By building two backends myself (ALSA + Raylib), I now recognize this pattern when Casey refactors platform code in later episodes.

---

**2. Sample-Accurate Timing is Non-Negotiable**

Before this detour, I thought "frame-based sequencing is close enough." After measuring the drift (±8ms per note, cumulative), I understand why Casey's audio callbacks fill **exactly** the requested sample count, not "approximately one frame's worth."

**Frame-based sequencing = drift:**

- 60fps frame = 800 samples at 48kHz
- Sequencer ticks once per frame → ±400 sample jitter per note
- After 100 notes, cumulative error = ±40,000 samples = ±833ms
- Musical timing sounds "drunk"

**Sample-based sequencing = exact:**

- Decrement `step_samples_remaining` inside PCM loop
- Note triggers at exact sample boundary
- After 1000 notes, error < 1 sample (0.02ms)

This is why DAWs, VST plugins, and rhythm games tick sequencers at **audio sample rate**, not video frame rate. When I return to Handmade Hero and Casey implements entity timers (Days 40+), I'll recognize the same principle—**physics timestep must be independent of frame rate**, not "close enough."

---

**3. Smoothstep Curves Are Everywhere**

Smoothstep S-curve (`t²(3-2t)`) appeared twice in this session:

1. **Audio fades** (commit `d63a936`) — eliminated clicks on REST notes by replacing linear ramps
2. **Animation easing** (mentioned in arch doc, not yet implemented in Snake) — same curve used for camera transitions, UI movement

Before this detour, I knew `ease-in-out` curves from CSS/web animation but didn't connect them to **derivative continuity** in math. Now I understand:

- Linear `y = t` has `y' = 1` (constant velocity) → instant velocity change at endpoints → artifact (audio click, animation jerk)
- Smoothstep `y = t²(3-2t)` has `y'(0) = y'(1) = 0` → continuous acceleration → smooth

When Casey implements camera lerp or entity movement easing in Handmade Hero, I'll recognize this immediately. It's not "fancy animation polish"—it's **physics correctness** (continuous velocity = no discontinuities).

---

**4. Memory Lifetime Rules Are Load-Bearing**

The "platform owns buffers, game writes them" rule prevents **three classes of bugs:**

| Bug Class                 | Symptom                                                      | Prevented By                                             |
| ------------------------- | ------------------------------------------------------------ | -------------------------------------------------------- |
| **Hot-reload corruption** | Audio dies mid-session; backbuffer pointer invalid           | Platform allocates `GameProps`, survives DLL reload      |
| **Double-free**           | Crash at shutdown; heap corruption                           | Only one owner calls `free()` (platform, not game)       |
| **Ownership ambiguity**   | Who frees this? Both layers? Neither? (memory leak or crash) | Clear rule: platform allocates/frees, game never touches |

Casey's Handmade Hero `platform_api.h` enforces this at the type level—`game_memory` is allocated by platform, passed to game as `void *`. Game casts it to `GameState *`, but **never** calls `VirtualAlloc` or `mmap`. Same principle.

When I return to Handmade Hero and Casey introduces asset streaming (Days 50+), this rule will matter even more—platform loads files, game reads them. If game tried to `fopen` directly, cross-platform builds break (path separators, file handles differ).

---

**5. `is_initialized` is a State Machine, Not a Boolean**

Before this detour, I thought `is_initialized` was a "did init succeed?" flag checked once at startup. After writing the arch doc's device hot-plug section, I understand it's a **state machine edge** that transitions during runtime:

```
Device connected → backend inits → is_initialized = TRUE
User unplugs headphones → backend detects → is_initialized = FALSE
Backend re-inits on new device → is_initialized = TRUE
```

If game only checks `is_initialized` once at startup, writing to audio buffer during hot-plug = **undefined behavior** (crash or garbage audio). The check must happen **every frame** in `game_get_audio_samples()`.

Windows/macOS fire device-change events constantly (Bluetooth headphones, USB DACs). Linux/ALSA doesn't surface this easily, but **the check must be there for correctness**, even if current backends don't trigger it.

When I return to Handmade Hero and Casey implements renderer device-loss handling (later episodes, GPU context recreation), I'll recognize the same pattern—`render_context_valid` is a state machine, not a one-time check.

---

**6. Debug Data Belongs in the Contract**

The debug layering violation (commit `a96b756`) taught me: **if debug overlay has `#ifdef PLATFORM_X`, you've coupled debug to platform**.

**Broken pattern:**

```c
/* Game adapter code (should be portable) */
#ifdef PLATFORM_X11
linux_debug_audio_latency(&platform_state->config.audio);  /* X11-specific call */
#endif
```

**Correct pattern:**

```c
/* Game debug overlay (portable) */
printf("[AUDIO] rate=%d initialized=%d\n",
       game_state->audio.samples_per_second,    /* Shared contract field */
       (int)game_state->audio.is_initialized);  /* Shared contract field */
```

The fix: **move debug-relevant data to shared contract**. Backend populates `AudioOutputBuffer.latency_ms`, `.buffer_fill_percent`. Game reads them. Zero `#ifdef` in game layer.

Casey's Handmade Hero debug overlay (`DEBUGTextLine`, `DEBUGDrawRectangle`) reads from `debug_state`, not platform-private structs. Same principle. When I return to Handmade Hero and Casey adds debug visualization (performance counters, memory allocators), I'll recognize this pattern—**debug is a shared service, not a platform detail**.

---

**7. Init Sequencing is Fragile Without Documentation**

The "Critical Wiring" bugs in Desktop Tower Defense (`game_audio_init()` never called, `game_audio_update()` never called) would have been **impossible to diagnose without source code**. Symptoms:

- Audio system compiles clean
- No assertions, no errors
- Audio buffer allocated, backend inited
- Complete silence (no sound at all)

**Root cause:** `memset(s, 0, sizeof(*s))` in `game_init()` zeroed `audio.master_volume` → every sample multiplied by zero → silence. Without calling `game_audio_init()`, volume stays zero.

The fix: add **two function calls** (5 lines of code). But finding the bug took 30 minutes because **nothing failed loudly**.

**Lesson:** Document init sequence in architecture guide **before** building complex systems. DTD arch doc now has "Boot Sequence" section:

```
1. platform_init()        → create window, alloc buffers
2. platform_audio_init()  → set samples_per_second
3. game_init()            → call game_audio_init() (volume setup)
4. Main loop starts
```

When I return to Handmade Hero, Casey's platform init order (Days 5-10) will make more sense—he's not just "organizing code," he's **enforcing data flow dependencies** (audio device must exist before game reads `samples_per_second`).

---

**Overall:** This detour session solidified the **three-layer audio architecture** (platform → contract → game) and exposed **subtle bugs** that only appear when building multiple backends. The skills here—contract design, hot-reload safety, init sequencing, derivative-continuous math—are the **exact patterns** Casey uses throughout Handmade Hero. By experiencing the bugs and fixes myself, I've built **mechanical intuition** that will let me follow Casey's more complex refactors (audio streaming, asset pipeline, hot-reload improvements) without getting lost.
