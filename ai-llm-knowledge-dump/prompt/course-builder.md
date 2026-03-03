# Course Builder Prompt

Use this file with the Copilot CLI agent to convert a source file or project into a build-along course.

**Reference Implementation:** `games/tetris/` — A complete example following these patterns.

---

## How to use

Attach this file + the source file(s) or directory you want to convert, then say:

> "Build a course from @path/to/source based on @ai-llm-knowledge-dump/prompt/course-builder.md and output it to @path/to/course/"

---

## What to build

### Step 1 — Analyze & Plan

1. Read the source file(s) thoroughly.
2. Read `@ai-llm-knowledge-dump/llm.txt` to understand the student's background.
3. Identify the natural **build progression**: what is the smallest runnable first step? What do you add next? What are the logical milestones?
4. Write a `PLAN.md` next to the source file(s) covering:
   - What the program does overall
   - The proposed lesson sequence (one line per lesson: lesson number + what gets built + what the student sees)
   - The planned file structure for the course output
5. **Do not start writing lessons until PLAN.md exists.**

---

### Step 2 — Build the course files (Recommended file layout)

```
game-name/
├── build-dev.sh              # Build script with backend selection
├── src/
│   ├── utils/
│   │   ├── backbuffer.h      # Backbuffer type and color macros
│   │   ├── draw-shapes.c     # Rectangle, line primitives
│   │   ├── draw-shapes.h
│   │   ├── draw-text.c       # Bitmap font rendering
│   │   ├── draw-text.h
│   │   └── math.h            # MIN, MAX, CLAMP macros
│   ├── game.c                # All game logic
│   ├── game.h                # Game types, state, API
│   ├── platform.h            # Platform contract
│   ├── main_x11.c            # X11/OpenGL backend
│   └── main_raylib.c         # Raylib backend
└── build/                    # Build output (gitignored)
```

**Why `utils/`?** Drawing primitives and math helpers are reusable across games. Keep them separate from game-specific logic.

**Reference:** See `games/tetris/` for the complete structure.

---

## Build script

Use a build script with backend selection and run flags.

**Reference:** `games/tetris/build-dev.sh`

**Key features to include:**

- `--backend=x11|raylib` flag for selecting platform backend
- `-r` or `--run` flag to run after building
- `-d` or `--debug` flag for debug builds with sanitizers
- Separate `SOURCES` variable for common game files
- Backend-specific source and library additions via `case` statement

**Usage pattern:**

```bash
./build-dev.sh                      # Build with default backend (raylib)
./build-dev.sh --backend=x11        # Build with X11
./build-dev.sh --backend=raylib -r  # Build and run
./build-dev.sh -d -r                # Debug build and run
```

---

## Source file rules

The source code must be:

- Written in **C** (not C++), even if the original is C++
- Split into three layers:
  - **Game logic**: `game.c` — all update, render, and draw calls; zero platform API
  - **Shared header**: `game.h` — types, structs, enums, public function declarations
  - **Platform backends**: `main_x11.c`, `main_raylib.c` — one file per platform
- Connected via a **`platform.h` contract**
- Uses a **backbuffer pipeline**: game writes ARGB pixels into a `uint32_t *` array; platform uploads that array to the GPU each frame
- Buildable with `clang` using the provided build scripts
- Heavily commented — every non-obvious line gets a comment explaining the _why_

### Platform contract

**Reference:** `games/tetris/src/platform.h`

The minimal contract requires these functions:

```c
int    platform_init(PlatformGameProps *props);
double platform_get_time(void);
void   platform_get_input(GameInput *input, PlatformGameProps *props);
void   platform_shutdown(void);
```

For engine integration, wrap initialization state in `PlatformGameProps`:

- Window title, width, height
- Backbuffer struct
- `is_running` flag

**Look for in the reference:**

- `platform_game_props_init()` — allocates backbuffer, sets dimensions
- `platform_game_props_free()` — frees backbuffer
- How each backend implements the contract differently

**Principle:** Keep the contract minimal for simple games. Expand for engine integration. The game layer should never call platform-specific APIs directly.

---

## Architectural patterns

### Backbuffer architecture (mandatory)

**Reference:** `games/tetris/src/utils/backbuffer.h`

```
┌──────────────────────────────────────────┐
│  main_x11.c / main_raylib.c              │
│  platform_init → loop:                   │
│    platform_get_input                    │
│    game_update(state, input, dt)         │
│    game_render(backbuffer, state)        │
│    upload backbuffer to GPU              │
└──────────────────────────────────────────┘
                  ↓ Backbuffer *
┌──────────────────────────────────────────┐
│  game.c                                  │
│  game_render writes ARGB pixels          │
│  draw_rect / draw_rect_blend             │
│  draw_text / draw_char (bitmap font)     │
└──────────────────────────────────────────┘
```

**Look for in the reference:**

- `Backbuffer` struct with `pixels`, `width`, `height`, `pitch`
- `GAME_RGB()` and `GAME_RGBA()` macros for color packing
- How `game_render()` writes pixels, platform uploads them

**Why?** Game logic is 100% platform-independent. Adding a new backend only requires a new `main_*.c`.

### Color system (mandatory)

**Reference:** `games/tetris/src/utils/backbuffer.h` for macros, `games/tetris/src/game.h` for constants

**Pattern:**

```c
/* 0xAARRGGBB — alpha in high byte, blue in low byte */
#define GAME_RGBA(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 255)
```

Pre-define named color constants in `game.h`:

- `COLOR_BLACK`, `COLOR_WHITE`, `COLOR_RED`, etc.
- Game-specific colors (e.g., `COLOR_CYAN` for Tetris I-piece)

### Delta-time game loop (mandatory)

**Reference:** `games/tetris/src/main_x11.c` (main loop), `games/tetris/src/main_raylib.c` (main loop)

**Pattern:**

```c
double prev_time = platform_get_time();
while (running) {
    double curr_time = platform_get_time();
    float delta_time = (float)(curr_time - prev_time);
    prev_time = curr_time;
    if (delta_time > 0.1f) delta_time = 0.1f;  /* cap for debugger pauses */

    prepare_input_frame(old_input, current_input);
    platform_get_input(current_input, &props);
    game_update(&state, current_input, delta_time);
    game_render(&backbuffer, &state);
    /* platform uploads backbuffer */

    /* swap input buffers */
    current_index = 1 - current_index;
}
```

**Game-specific considerations:**

- Tetris: Uses `delta_time` for drop timers, auto-repeat
- Physics games: May need fixed timestep with accumulator
- Turn-based: `delta_time` still useful for animations

### VSync with manual fallback

**Reference:** `games/tetris/src/main_x11.c` — `setup_vsync()` function

**Look for:**

- VSync extension detection (`GLX_EXT_swap_control`, `GLX_MESA_swap_control`)
- Fallback flag: `g_x11.vsync_enabled`
- Manual frame limiting when VSync unavailable:
  ```c
  if (!g_x11.vsync_enabled) {
      double frame_time = get_time() - current_time;
      if (frame_time < g_target_frame_time) {
          sleep_ms((int)((g_target_frame_time - frame_time) * 1000.0));
      }
  }
  ```

**Raylib:** Handles this internally with `SetTargetFPS(60)`.

---

## Input System (mandatory)

**Reference:** `games/tetris/src/game.h` (input types), `games/tetris/src/game.c` (`prepare_input_frame`, `handle_action_with_repeat`), `games/tetris/src/platform.h` (`platform_swap_input_buffers`)

### Double-Buffered Input

Why double-buffer? We need to know what happened **last frame** to detect transitions (pressed → held, held → released). A single buffer loses this history.

**Reference:** `games/tetris/src/main_raylib.c` (main loop), `games/tetris/src/main_x11.c` (main loop)

**Pattern (index-based swap):**

```c
/* In main(): */
GameInput inputs[2] = {0};
int current_index = 0;

while (running) {
    GameInput *current_input = &inputs[current_index];
    GameInput *old_input = &inputs[1 - current_index];

    prepare_input_frame(old_input, current_input);
    platform_get_input(current_input, &props);

    game_update(&state, current_input, delta_time);

    /* Swap by toggling index */
    current_index = 1 - current_index;
}
```

**Alternative (content swap):**

```c
/* In platform.h: */
static inline void platform_swap_input_buffers(GameInput *old_input,
                                               GameInput *current_input) {
  GameInput temp = *current_input;
  *current_input = *old_input;
  *old_input = temp;
}
```

**Warning:** Do NOT swap pointers — you must swap the actual contents:

```c
/* WRONG - swaps local pointer copies, not data! */
GameInput *temp = current_input;
*current_input = *old_input;  /* Copies data, but... */
*old_input = *temp;           /* temp is still old pointer! */

/* CORRECT - swaps contents */
GameInput temp = *current_input;  /* Copy CONTENTS to stack */
*current_input = *old_input;
*old_input = temp;
```

### Core Types

**Reference:** `games/tetris/src/game.h`

```c
typedef struct {
    int half_transition_count;  /* state changes this frame (0, 1, or 2) */
    int ended_down;             /* current state: 1=pressed, 0=released */
} GameButtonState;
```

**Why `half_transition_count`?**

- `0` = No change (button held or released entire frame)
- `1` = Changed once (normal press or release)
- `2` = Changed twice (pressed then released, or vice versa, within one frame)

At 30fps, each frame is 33ms. A quick tap might go down AND up within one frame.

**Button update macro:**

```c
#define UPDATE_BUTTON(button, is_down)                                         \
  do {                                                                         \
    if ((button).ended_down != (is_down)) {                                    \
      (button).half_transition_count++;                                        \
      (button).ended_down = (is_down);                                         \
    }                                                                          \
  } while (0)
```

**GameInput struct with union for iteration:**

```c
#define BUTTON_COUNT 4

typedef struct {
  union {
    GameButtonState buttons[BUTTON_COUNT];
    struct {
      GameButtonState move_left;
      GameButtonState move_right;
      GameButtonState move_down;
      GameButtonState rotate;
    };
  };
  int quit;
  int restart;
} GameInput;
```

**Why union?** Iterate with `buttons[i]` for bulk operations, access by name for readability.

### Preparing Each Frame

**Reference:** `games/tetris/src/game.c` — `prepare_input_frame()`

```c
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
  for (int btn = 0; btn < BUTTON_COUNT; btn++) {
    /* CRITICAL: Preserve ended_down from last frame! */
    current_input->buttons[btn].ended_down = old_input->buttons[btn].ended_down;
    /* Reset transition count for new frame */
    current_input->buttons[btn].half_transition_count = 0;
  }
  /* Reset one-shot inputs */
  current_input->quit = 0;
  current_input->restart = 0;
}
```

**Why preserve `ended_down`?** For event-based input (X11), if no event fires this frame, the button state should remain unchanged. Copying from `old_input` ensures held keys stay held.

### Event-Based vs Polling Input

**Reference:** `games/tetris/src/main_x11.c` (event-based), `games/tetris/src/main_raylib.c` (polling)

| Platform | Model       | How it works                                                 |
| -------- | ----------- | ------------------------------------------------------------ |
| X11/ALSA | Event-based | `KeyPress` fires once on press, `KeyRelease` once on release |
| Raylib   | Polling     | `IsKeyDown()` returns true every frame while held            |

**X11 (events):**

```c
while (XPending(display) > 0) {
    XNextEvent(display, &event);
    switch (event.type) {
    case KeyPress:
        KeySym key = XLookupKeysym(&event.xkey, 0);
        switch (key) {
        case XK_Left:
        case XK_a:
            UPDATE_BUTTON(input->move_left, 1);
            break;
        /* ... other keys ... */
        }
        break;
    case KeyRelease:
        KeySym key = XLookupKeysym(&event.xkey, 0);
        switch (key) {
        case XK_Left:
        case XK_a:
            UPDATE_BUTTON(input->move_left, 0);
            break;
        /* ... other keys ... */
        }
        break;
    }
}
/* No event = no change. ended_down preserved from prepare_input_frame */
```

**Raylib (polling):**

```c
/* Called every frame regardless of events */
UPDATE_BUTTON(current_input->move_left, IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
UPDATE_BUTTON(current_input->move_right, IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT));
UPDATE_BUTTON(current_input->move_down, IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));
```

**Why this matters:** With event-based input, `prepare_input_frame` MUST copy `ended_down` from the previous frame. With polling, you overwrite it every frame anyway—but the copy doesn't hurt and keeps the code consistent.

### Auto-Repeat (DAS/ARR)

**Reference:** `games/tetris/src/game.h` — `RepeatInterval`, `games/tetris/src/game.c` — `handle_action_with_repeat()`

DAS = Delayed Auto Shift (initial delay before repeating starts)
ARR = Auto Repeat Rate (speed of repeating after DAS)

```c
typedef struct {
  float timer;               /* Accumulates delta time */
  float initial_delay;       /* DAS: time before first repeat (e.g., 0.15s) */
  float interval;            /* ARR: time between repeats (e.g., 0.05s) */
  bool is_active;            /* Currently in repeat sequence? */
  bool passed_initial_delay; /* Have we triggered the first repeat? */
} RepeatInterval;
```

**Key insight:** Repeat state lives in `GameState`, NOT in `GameInput`:

```c
/* In GameState: */
struct {
  RepeatInterval move_left;
  RepeatInterval move_right;
  RepeatInterval move_down;
} input_repeat;
```

**Why separate?** `GameInput` represents raw hardware state (what buttons are pressed). `RepeatInterval` represents game timing logic (when to trigger actions). Different concerns, different locations.

### Handle Action With Repeat

**Reference:** `games/tetris/src/game.c` — `handle_action_with_repeat()`

```c
static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval *repeat,
                                      float delta_time,
                                      int *should_trigger) {
  *should_trigger = 0;

  /* Released - reset everything */
  if (!button->ended_down) {
    repeat->timer = 0.0f;
    repeat->is_active = false;
    repeat->passed_initial_delay = false;
    return;
  }

  /* Just pressed this frame */
  if (button->half_transition_count > 0) {
    repeat->timer = 0.0f;
    repeat->is_active = true;
    repeat->passed_initial_delay = (repeat->initial_delay <= 0.0f);

    /* Trigger immediately only if no initial delay */
    if (repeat->initial_delay <= 0.0f) {
      *should_trigger = 1;
    }
    return;
  }

  /* Held from previous frame */
  if (!repeat->is_active) {
    return;
  }

  repeat->timer += delta_time;

  if (!repeat->passed_initial_delay) {
    /* Waiting for initial delay (DAS) */
    if (repeat->timer >= repeat->initial_delay) {
      *should_trigger = 1;
      repeat->timer = 0.0f;
      repeat->passed_initial_delay = true;
    }
  } else {
    /* In repeat phase (ARR) */
    if (repeat->timer >= repeat->interval) {
      *should_trigger = 1;
      repeat->timer -= repeat->interval;  /* Keep remainder */
    }
  }
}
```

**Usage:**

```c
int should_move_left = 0;
handle_action_with_repeat(&input->move_left, &state->input_repeat.move_left,
                          delta_time, &should_move_left);
if (should_move_left && piece_can_move_left(state)) {
    state->piece.x--;
}
```

### Initialize Repeat Intervals

**Reference:** `games/tetris/src/game.c` — `game_init()`

```c
void game_init(GameState *state, GameInput *input) {
  /* ... other init ... */

  /* Movement: DAS=150ms, ARR=50ms */
  state->input_repeat.move_left = (RepeatInterval){
    .timer = 0.0f,
    .initial_delay = 0.15f,
    .interval = 0.05f,
    .is_active = false,
    .passed_initial_delay = false
  };

  state->input_repeat.move_right = (RepeatInterval){
    .timer = 0.0f,
    .initial_delay = 0.15f,
    .interval = 0.05f,
    .is_active = false,
    .passed_initial_delay = false
  };

  /* Soft drop: No delay, fast repeat */
  state->input_repeat.move_down = (RepeatInterval){
    .timer = 0.0f,
    .initial_delay = 0.0f,   /* Trigger immediately */
    .interval = 0.03f,       /* Fast repeat */
    .is_active = false,
    .passed_initial_delay = false
  };
}
```

### Timing Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│  KEY PRESSED                          KEY RELEASED                      │
│      │                                     │                            │
│      ▼                                     ▼                            │
│  ════╪═══════════════════════════════════════════════════════════════   │
│      │                                                                  │
│      │   initial_delay (DAS)   interval (ARR)                          │
│      │◄─────────────────────►│◄──────────►│◄──────────►│               │
│      │         150ms          │    50ms    │    50ms    │               │
│      │                        │            │            │               │
│      ▼                        ▼            ▼            ▼               │
│   [nothing]              [trigger]    [trigger]    [trigger]           │
│   (waiting)              (1st repeat) (2nd repeat) (3rd repeat)        │
│                                                                         │
│  For move_down (initial_delay=0):                                      │
│      │                                                                  │
│      │   interval   interval   interval                                │
│      │◄──────────►│◄──────────►│◄──────────►│                          │
│      │    30ms     │    30ms    │    30ms    │                          │
│      ▼             ▼            ▼            ▼                          │
│   [trigger]    [trigger]   [trigger]    [trigger]                      │
│   (immediate)                                                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Common Input Bugs

| Symptom                                   | Cause                                               | Fix                                                                |
| ----------------------------------------- | --------------------------------------------------- | ------------------------------------------------------------------ |
| Held key acts like repeated presses       | `ended_down` not preserved in `prepare_input_frame` | Copy from `old_input` before resetting transitions                 |
| Key only works on first press (X11)       | Event-based input loses state between frames        | Ensure `prepare_input_frame` copies `ended_down`                   |
| Auto-repeat doesn't work                  | `passed_initial_delay` not tracked                  | Add flag to `RepeatInterval`, check in handler                     |
| Repeat triggers immediately despite delay | Early return when `half_transition_count > 0`       | Only trigger immediately if `initial_delay <= 0`                   |
| Input feels "sticky" after release        | Buffer swap not happening correctly                 | Verify swapping contents, not pointers                             |
| Down key doesn't repeat but left/right do | Different `initial_delay` values                    | Check initialization — `move_down` should have `initial_delay = 0` |

---

## Typed enums (mandatory)

**Reference:** `games/tetris/src/game.h` — all `typedef enum` declarations

**Pattern:**

```c
typedef enum { DIR_NONE, DIR_LEFT, DIR_RIGHT } MOVE_DIR;
void piece_move(GameState *state, MOVE_DIR dir);  /* self-documenting */
```

**Look for in the reference:**

- `TETROMINO_BY_IDX` — which piece type
- `TETRIS_FIELD_CELL` — cell contents (empty, wall, piece types)
- `TETROMINO_R_DIR` — rotation states
- `TETROMINO_ROTATE_X_VALUE` — rotation input direction

**Why?** Prevents passing wrong integer values. Makes function signatures readable.

---

## Debug trap macro (mandatory for debug builds)

**Why:** When an assertion fails, you want the debugger to break at the exact line. A null pointer dereference (`*(int*)0 = 0`) works but is undefined behavior. Platform-specific traps are cleaner.

**Pattern:**

```c
/* In a shared header like game.h or utils/debug.h */
#ifdef DEBUG
  #if defined(_MSC_VER)
    #define DEBUG_TRAP() __debugbreak()
  #elif defined(__GNUC__) || defined(__clang__)
    #define DEBUG_TRAP() __builtin_trap()
  #else
    #define DEBUG_TRAP() (*(volatile int *)0 = 0)
  #endif

  #define ASSERT(expr) do { if (!(expr)) { DEBUG_TRAP(); } } while(0)
  #define ASSERT_MSG(expr, msg) do { if (!(expr)) { fprintf(stderr, "ASSERT: %s\n", msg); DEBUG_TRAP(); } } while(0)
#else
  #define DEBUG_TRAP()
  #define ASSERT(expr)
  #define ASSERT_MSG(expr, msg)
#endif
```

**Usage:**

```c
ASSERT(player_x >= 0);
ASSERT_MSG(buffer != NULL, "Buffer was not allocated");
```

**Add `-DDEBUG` to debug builds in your build script.**

---

## Coordinate and unit systems

### When to use which system

| Game Type                | Coordinate System      | Unit System                                 |
| ------------------------ | ---------------------- | ------------------------------------------- |
| Tetris, Pong, Breakout   | Y-down (matches grid)  | Tiles/cells directly                        |
| Platformer, physics game | Y-up (matches math)    | World units (meters)                        |
| RPG with scrolling       | Y-up + camera-relative | World units + hierarchical for large worlds |
| Infinite/procedural      | Y-up + hierarchical    | Chunk + offset                              |

### Y-up vs Y-down

**Current Tetris uses Y-down** because:

- Grid rows are naturally numbered top-to-bottom
- Pieces "fall down" = positive Y movement
- Screen coordinates match game coordinates

**For physics/platformer games, use Y-up:**

```c
/* In game logic: Y-up (0,0 is bottom-left) */
player.pos.y += velocity.y * dt;  /* positive = up */

/* In game_render(): convert to screen coords */
int screen_y = (backbuffer->height - 1) - (int)world_y;
```

**Rule:** Pick one convention and stick with it. Document your choice.

### Unit system

**Tetris uses tiles directly** — `CELL_SIZE` is pixels, positions are tile indices.

**For resolution-independent games:**

```c
#define PIXELS_PER_UNIT 32.0f  /* 1 world unit = 32 pixels */

/* Game logic uses world units */
player.pos.x += velocity.x * dt;  /* units per second */

/* Rendering converts to pixels */
int pixel_x = (int)(player.pos.x * PIXELS_PER_UNIT);
int pixel_y = (int)(player.pos.y * PIXELS_PER_UNIT);
```

**Reference:** `games/tetris/src/game.h` — `CELL_SIZE`, `FIELD_WIDTH`, `FIELD_HEIGHT`

---

## Drawing primitives

### Rectangle drawing

**Reference:** `games/tetris/src/utils/draw-shapes.c`

**Solid rectangle:**

```c
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
  /* Clip to backbuffer bounds */
  int x0 = (x < 0) ? 0 : x;
  int y0 = (y < 0) ? 0 : y;
  int x1 = (x + w > bb->width) ? bb->width : x + w;
  int y1 = (y + h > bb->height) ? bb->height : y + h;

  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * bb->width;
    for (int px = x0; px < x1; px++) {
      row[px] = color;
    }
  }
}
```

**Blended rectangle (for overlays):**

```c
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
  int x0 = (x < 0) ? 0 : x;
  int y0 = (y < 0) ? 0 : y;
  int x1 = (x + w > bb->width) ? bb->width : x + w;
  int y1 = (y + h > bb->height) ? bb->height : y + h;

  uint32_t src_a = (color >> 24) & 0xFF;
  uint32_t src_r = (color >> 16) & 0xFF;
  uint32_t src_g = (color >> 8) & 0xFF;
  uint32_t src_b = color & 0xFF;
  uint32_t inv_a = 255 - src_a;

  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * bb->width;
    for (int px = x0; px < x1; px++) {
      uint32_t dst = row[px];
      uint32_t dst_r = (dst >> 16) & 0xFF;
      uint32_t dst_g = (dst >> 8) & 0xFF;
      uint32_t dst_b = dst & 0xFF;

      uint32_t out_r = (src_r * src_a + dst_r * inv_a) / 255;
      uint32_t out_g = (src_g * src_a + dst_g * inv_a) / 255;
      uint32_t out_b = (src_b * src_a + dst_b * inv_a) / 255;

      row[px] = 0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
    }
  }
}
```

**Usage:**

```c
/* Solid red rectangle */
draw_rect(bb, 10, 10, 100, 50, COLOR_RED);

/* Semi-transparent black overlay for game over screen */
draw_rect_blend(bb, 0, 0, bb->width, bb->height, GAME_RGBA(0, 0, 0, 128));
```

### Bitmap font rendering

**Reference:** `games/tetris/src/utils/draw-text.c`, `games/tetris/src/utils/draw-text.h`

**Pattern:** Embed a simple bitmap font as a static array. Each character is an 8x8 grid stored as 8 bytes (one byte per row, bits are pixels).

```c
/* 8x8 bitmap font - each char is 8 bytes */
static const uint8_t FONT_8X8[128][8] = {
  /* ASCII 32 (space) */
  [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  /* ASCII 65 'A' */
  ['A'] = {0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
  /* ... etc ... */
};

void draw_char(Backbuffer *bb, int x, int y, char c, uint32_t color, int scale) {
  if (c < 0 || c > 127) return;
  const uint8_t *glyph = FONT_8X8[(int)c];

  for (int row = 0; row < 8; row++) {
    uint8_t bits = glyph[row];
    for (int col = 0; col < 8; col++) {
      if (bits & (0x80 >> col)) {
        /* Draw scaled pixel */
        for (int sy = 0; sy < scale; sy++) {
          for (int sx = 0; sx < scale; sx++) {
            int px = x + col * scale + sx;
            int py = y + row * scale + sy;
            if (px >= 0 && px < bb->width && py >= 0 && py < bb->height) {
              bb->pixels[py * bb->width + px] = color;
            }
          }
        }
      }
    }
  }
}

void draw_text(Backbuffer *bb, int x, int y, const char *text, uint32_t color, int scale) {
  int cursor_x = x;
  while (*text) {
    draw_char(bb, cursor_x, y, *text, color, scale);
    cursor_x += 8 * scale;  /* 8 pixels per char, scaled */
    text++;
  }
}
```

**Usage:**

```c
draw_text(bb, 10, 10, "SCORE", COLOR_WHITE, 2);  /* 2x scale */
draw_text(bb, 10, 30, "12345", COLOR_YELLOW, 2);
```

---

## Audio system (optional)

**Reference:** `games/tetris/src/utils/audio.h`, `games/tetris/src/audio.c`

### Audio output buffer

```c
typedef struct {
  int16_t *samples;       /* Interleaved stereo: L0,R0,L1,R1,... */
  int samples_per_second; /* Usually 44100 or 48000 */
  int sample_count;       /* How many stereo pairs to generate */
} AudioOutputBuffer;
```

### Sound effect system

```c
typedef enum {
  SOUND_NONE = 0,
  SOUND_MOVE,
  SOUND_ROTATE,
  SOUND_DROP,
  SOUND_LINE_CLEAR,
  SOUND_TETRIS,
  SOUND_LEVEL_UP,
  SOUND_GAME_OVER,
  SOUND_COUNT
} SOUND_ID;

typedef struct {
  float frequency;      /* Starting frequency (Hz) */
  float frequency_end;  /* Ending frequency (Hz), 0 = no slide */
  float duration_ms;    /* Duration in milliseconds */
  float volume;         /* 0.0 to 1.0 */
} SoundDef;

/* Define sounds as data */
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
  [SOUND_NONE]       = {0, 0, 0, 0.0f},
  [SOUND_MOVE]       = {200, 150, 50, 0.3f},   /* Quick blip down */
  [SOUND_ROTATE]     = {300, 400, 80, 0.3f},   /* Quick blip up */
  [SOUND_DROP]       = {150, 50, 100, 0.5f},   /* Thud down */
  [SOUND_LINE_CLEAR] = {400, 800, 200, 0.6f},  /* Rising sweep */
  [SOUND_TETRIS]     = {300, 1200, 400, 0.8f}, /* Long rising sweep */
  [SOUND_LEVEL_UP]   = {440, 880, 300, 0.5f},  /* Octave up */
  [SOUND_GAME_OVER]  = {400, 100, 500, 0.7f},  /* Sad descend */
};
```

### Playing sounds

```c
#define MAX_SIMULTANEOUS_SOUNDS 4

typedef struct {
  SOUND_ID sound_id;
  float phase;
  int samples_remaining;
  float volume;
  float frequency;
  float frequency_slide;
} SoundInstance;

void game_play_sound(GameAudioState *audio, SOUND_ID sound) {
  if (sound == SOUND_NONE || sound >= SOUND_COUNT) return;

  /* Find a free slot */
  int slot = -1;
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
    if (audio->active_sounds[i].samples_remaining <= 0) {
      slot = i;
      break;
    }
  }

  /* No free slot - steal the oldest (slot 0) */
  if (slot < 0) slot = 0;

  const SoundDef *def = &SOUND_DEFS[sound];
  SoundInstance *inst = &audio->active_sounds[slot];

  inst->sound_id = sound;
  inst->phase = 0.0f;
  inst->frequency = def->frequency;
  inst->volume = def->volume;
  inst->samples_remaining = (int)(def->duration_ms * audio->samples_per_second / 1000.0f);

  if (def->frequency_end > 0 && inst->samples_remaining > 0) {
    inst->frequency_slide = (def->frequency_end - def->frequency) / inst->samples_remaining;
  } else {
    inst->frequency_slide = 0;
  }
}
```

### Generating samples

```c
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer) {
  GameAudioState *audio = &state->audio;
  int16_t *out = buffer->samples;
  int sample_count = buffer->sample_count;
  float inv_sample_rate = 1.0f / (float)buffer->samples_per_second;

  /* Clear buffer first */
  memset(out, 0, sample_count * 2 * sizeof(int16_t));

  for (int s = 0; s < sample_count; s++) {
    float mixed_sample = 0.0f;

    /* Mix all active sound effects */
    for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
      SoundInstance *inst = &audio->active_sounds[i];
      if (inst->samples_remaining <= 0) continue;

      /* Generate square wave (classic 8-bit sound) */
      float wave = (inst->phase < 0.5f) ? 1.0f : -1.0f;

      /* Apply envelope (simple linear fadeout) */
      float env = (float)inst->samples_remaining /
                  (SOUND_DEFS[inst->sound_id].duration_ms *
                   buffer->samples_per_second / 1000.0f);

      mixed_sample += wave * inst->volume * env * audio->sfx_volume;

      /* Advance phase */
      inst->phase += inst->frequency * inv_sample_rate;
      if (inst->phase >= 1.0f) inst->phase -= 1.0f;

      /* Apply frequency slide */
      inst->frequency += inst->frequency_slide;
      inst->samples_remaining--;
    }

    /* Apply master volume and convert to int16 */
    mixed_sample *= audio->master_volume * 16000.0f;
    int16_t sample = (mixed_sample > 32767.0f) ? 32767 :
                     (mixed_sample < -32768.0f) ? -32768 : (int16_t)mixed_sample;

    /* Write stereo (same sample to both channels) */
    *out++ = sample;  /* Left */
    *out++ = sample;  /* Right */
  }
}
```

### Platform audio integration

**Reference:** `games/tetris/src/main_raylib.c`, `games/tetris/src/main_x11.c`

**Raylib:**

```c
int platform_audio_init(PlatformAudioConfig *config) {
  InitAudioDevice();
  if (!IsAudioDeviceReady()) {
    config->is_initialized = 0;
    return 1;
  }

  config->samples_per_second = 48000;
  config->buffer_size_samples = 1024;

  g_raylib.stream = LoadAudioStream(config->samples_per_second, 16, 2);
  g_raylib.buffer_size = config->buffer_size_samples;
  g_raylib.sample_buffer = malloc(g_raylib.buffer_size * 2 * sizeof(int16_t));

  PlayAudioStream(g_raylib.stream);
  config->is_initialized = 1;
  return 0;
}

static void platform_audio_update(GameState *game_state, PlatformAudioConfig *config) {
  if (!config->is_initialized) return;

  if (IsAudioStreamProcessed(g_raylib.stream)) {
    AudioOutputBuffer buffer = {
      .samples = g_raylib.sample_buffer,
      .samples_per_second = config->samples_per_second,
      .sample_count = g_raylib.buffer_size
    };
    game_get_audio_samples(game_state, &buffer);
    UpdateAudioStream(g_raylib.stream, buffer.samples, buffer.sample_count);
  }
}
```

**X11/ALSA:**

```c
int platform_audio_init(PlatformAudioConfig *config) {
  int err;
  config->samples_per_second = 48000;
  config->buffer_size_samples = 1024;

  err = snd_pcm_open(&g_x11.pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    config->is_initialized = 0;
    return 1;
  }

  /* Configure ALSA hardware parameters */
  snd_pcm_hw_params_t *hw_params;
  snd_pcm_hw_params_alloca(&hw_params);
  snd_pcm_hw_params_any(g_x11.pcm_handle, hw_params);
  snd_pcm_hw_params_set_access(g_x11.pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(g_x11.pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(g_x11.pcm_handle, hw_params, 2);

  unsigned int sample_rate = config->samples_per_second;
  snd_pcm_hw_params_set_rate_near(g_x11.pcm_handle, hw_params, &sample_rate, 0);

  snd_pcm_hw_params(g_x11.pcm_handle, hw_params);
  snd_pcm_prepare(g_x11.pcm_handle);

  g_x11.buffer_size = config->buffer_size_samples;
  g_x11.sample_buffer = malloc(g_x11.buffer_size * 2 * sizeof(int16_t));

  config->is_initialized = 1;
  return 0;
}

static void platform_audio_update(GameState *game_state, PlatformAudioConfig *config) {
  if (!config->is_initialized || !g_x11.pcm_handle) return;

  snd_pcm_sframes_t frames_avail = snd_pcm_avail_update(g_x11.pcm_handle);
  if (frames_avail < 0) {
    if (frames_avail == -EPIPE) {
      snd_pcm_prepare(g_x11.pcm_handle);
      frames_avail = snd_pcm_avail_update(g_x11.pcm_handle);
    } else {
      return;
    }
  }

  if (frames_avail < g_x11.buffer_size) return;

  AudioOutputBuffer buffer = {
    .samples = g_x11.sample_buffer,
    .samples_per_second = g_x11.samples_per_second,
    .sample_count = g_x11.buffer_size
  };

  game_get_audio_samples(game_state, &buffer);

  snd_pcm_sframes_t frames_written = snd_pcm_writei(g_x11.pcm_handle, g_x11.sample_buffer, g_x11.buffer_size);
  if (frames_written < 0) {
    snd_pcm_recover(g_x11.pcm_handle, frames_written, 0);
  }
}
```

### Music sequencer (optional)

**Reference:** `games/tetris/src/audio.c` — `MusicSequencer`, `game_audio_update()`

```c
#define MUSIC_PATTERN_LENGTH 16
#define MUSIC_NUM_PATTERNS 4

typedef struct {
  uint8_t patterns[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH];
  int current_pattern;
  int current_step;
  float step_timer;
  float step_duration;  /* Seconds per step (tempo) */
  ToneGenerator tone;
  int is_playing;
} MusicSequencer;

void game_audio_update(GameAudioState *audio, float delta_time) {
  if (!audio->music.is_playing) return;

  MusicSequencer *seq = &audio->music;
  seq->step_timer += delta_time;

  if (seq->step_timer >= seq->step_duration) {
    seq->step_timer -= seq->step_duration;

    uint8_t note = seq->patterns[seq->current_pattern][seq->current_step];
    if (note > 0) {
      seq->tone.frequency = midi_to_freq(note);
      seq->tone.is_playing = 1;
    } else {
      seq->tone.is_playing = 0;
    }

    seq->current_step++;
    if (seq->current_step >= MUSIC_PATTERN_LENGTH) {
      seq->current_step = 0;
      seq->current_pattern = (seq->current_pattern + 1) % MUSIC_NUM_PATTERNS;
    }
  }
}

/* MIDI note to frequency conversion */
static inline float midi_to_freq(int note) {
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}
```

---

## Game state management

### State struct pattern

**Reference:** `games/tetris/src/game.h` — `GameState`

```c
typedef struct {
  /* Core game data */
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
  CurrentPiece current_piece;
  int score;
  int level;
  int pieces_count;
  bool is_game_over;

  /* Timing */
  struct {
    float timer;
    float interval;
  } drop_timer;

  /* Input repeat state (separate from raw input!) */
  struct {
    RepeatInterval move_left;
    RepeatInterval move_right;
    RepeatInterval move_down;
  } input_repeat;

  /* Line clear animation */
  struct {
    int indexes[4];
    int count;
    float flash_timer;
  } completed_lines;

  /* Audio state */
  GameAudioState audio;
} GameState;
```

**Principles:**

1. Group related fields with anonymous structs
2. Keep timing state separate from game logic state
3. Input repeat state lives here, not in `GameInput`
4. Audio state is part of game state (persists across frames)

### Initialization

**Reference:** `games/tetris/src/game.c` — `game_init()`

```c
void game_init(GameState *state, GameInput *input) {
  /* Zero everything first */
  memset(state, 0, sizeof(GameState));

  /* Set non-zero defaults */
  state->pieces_count = 1;
  state->drop_timer.interval = 0.8f;

  /* Initialize field with walls */
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) {
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_WALL;
      }
    }
  }

  /* Initialize input repeat intervals */
  state->input_repeat.move_left = (RepeatInterval){
    .initial_delay = 0.15f,
    .interval = 0.05f
  };
  state->input_repeat.move_right = (RepeatInterval){
    .initial_delay = 0.15f,
    .interval = 0.05f
  };
  state->input_repeat.move_down = (RepeatInterval){
    .initial_delay = 0.0f,
    .interval = 0.03f
  };

  /* Spawn first piece */
  srand((unsigned int)time(NULL));
  state->current_piece.index = rand() % TETROMINOS_COUNT;
  state->current_piece.next_index = rand() % TETROMINOS_COUNT;
  state->current_piece.x = FIELD_WIDTH / 2 - 2;
  state->current_piece.y = 0;
}
```

---

## Game update pattern

**Reference:** `games/tetris/src/game.c` — `game_update()`

```c
void game_update(GameState *state, GameInput *input, float delta_time) {
  /* Update audio sequencer (always, even when paused) */
  game_audio_update(&state->audio, delta_time);

  /* Handle game over state */
  if (state->is_game_over) {
    if (input->restart) {
      game_init(state, input);
      game_music_play(&state->audio);
    }
    return;  /* Don't process game logic */
  }

  /* Handle animation states (line clear flash) */
  if (state->completed_lines.flash_timer > 0) {
    state->completed_lines.flash_timer -= delta_time;
    if (state->completed_lines.flash_timer <= 0) {
      /* Animation done - collapse lines */
      collapse_completed_lines(state);
    }
    return;  /* Freeze game logic during animation */
  }

  /* Process input */
  apply_input(state, input, delta_time);

  /* Update game timers */
  state->drop_timer.timer += delta_time;
  if (state->drop_timer.timer >= state->drop_timer.interval) {
    state->drop_timer.timer -= state->drop_timer.interval;
    try_drop_piece(state);
  }
}
```

**Key patterns:**

1. Audio updates even during pause/game over (music keeps playing)
2. Early returns for special states (game over, animations)
3. Input processing is separate from timer-based logic
4. Timer uses subtraction to keep remainder (precision)

---

## Rendering pattern

**Reference:** `games/tetris/src/game.c` — `game_render()`

```c
void game_render(Backbuffer *bb, GameState *state) {
  /* 1. Clear background */
  for (int i = 0; i < bb->width * bb->height; i++) {
    bb->pixels[i] = COLOR_BLACK;
  }

  /* 2. Draw static elements (field, walls) */
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      unsigned char cell = state->field[y * FIELD_WIDTH + x];
      if (cell != TETRIS_FIELD_EMPTY) {
        draw_cell(bb, x, y, get_cell_color(cell));
      }
    }
  }

  /* 3. Draw dynamic elements (current piece) */
  draw_piece(bb, &state->current_piece);

  /* 4. Draw UI (score, level, next piece) */
  draw_ui(bb, state);

  /* 5. Draw overlays (game over screen) */
  if (state->is_game_over) {
    draw_game_over_overlay(bb);
  }
}
```

**Layer order:**

1. Background (clear or fill)
2. Static game elements
3. Dynamic game elements
4. UI elements
5. Overlays (semi-transparent)

---

## Lesson structure

Each lesson should follow this structure:

### Lesson file template

````markdown
# Lesson XX: [Title]

## What we're building

[One paragraph describing what the student will have at the end of this lesson]

## What you'll learn

- [Concept 1]
- [Concept 2]
- [Concept 3]

## Prerequisites

- Completed Lesson XX-1
- [Any other requirements]

---

## Step 1: [First major step]

[Explanation of what we're doing and why]

```c
/* Code block with heavy comments */
```
````

**What's happening:**

- [Bullet point explanation]
- [Another point]

---

## Step 2: [Second major step]

[Continue pattern...]

---

## Build and run

```bash
./build-dev.sh --backend=raylib -r
```

**Expected output:**
[Description or screenshot of what student should see]

---

## Exercises

1. [Easy exercise]
2. [Medium exercise]
3. [Challenge exercise]

---

## What's next

In Lesson XX+1, we'll [preview of next lesson].

### Lesson progression guidelines

1. **Lesson 1**: Window + colored background (proof of life)
2. **Lesson 2**: Draw a single shape (rectangle, sprite)
3. **Lesson 3**: Input handling (move the shape)
4. **Lesson 4**: Game state struct + basic logic
5. **Lesson 5+**: Game-specific features

**Each lesson should:**

- Add ONE major concept
- Be completable in 15-30 minutes
- End with something visible/playable
- Include exercises for reinforcement

---

## Quick reference table

| Need                           | Pattern                                                        |
| ------------------------------ | -------------------------------------------------------------- |
| Platform-independent rendering | Backbuffer with `uint32_t *pixels`                             |
| Color constants                | `GAME_RGB(r,g,b)` macro + named constants                      |
| Frame timing                   | `delta_time` from platform, cap at 0.1f                        |
| Button state                   | `GameButtonState` with `ended_down` + `half_transition_count`  |
| Previous frame's input         | Double-buffered `GameInput` with swap                          |
| Auto-repeat with delay         | `RepeatInterval` with `initial_delay` + `passed_initial_delay` |
| Event-based input (X11)        | Preserve `ended_down` in `prepare_input_frame`                 |
| Polling input (Raylib)         | Call `UPDATE_BUTTON` every frame with current state            |
| Type-safe constants            | `typedef enum { ... } NAME;`                                   |
| Debug assertions               | `ASSERT()` macro with `DEBUG_TRAP()`                           |
| Sound effects                  | `SoundDef` data + `SoundInstance` mixer                        |
| Background music               | `MusicSequencer` with pattern data                             |
| Semi-transparent overlay       | `draw_rect_blend()` with alpha                                 |
| Text rendering                 | 8x8 bitmap font + `draw_text()`                                |
| Resolution independence        | World units + `PIXELS_PER_UNIT` conversion                     |
| VSync with fallback            | Detect extension, manual sleep if unavailable                  |

---

## Common bugs and fixes

| Symptom                                   | Cause                                               | Fix                                                |
| ----------------------------------------- | --------------------------------------------------- | -------------------------------------------------- |
| Held key acts like repeated presses       | `ended_down` not preserved in `prepare_input_frame` | Copy from `old_input` before resetting transitions |
| Key only works on first press (X11)       | Event-based input loses state between frames        | Ensure `prepare_input_frame` copies `ended_down`   |
| Auto-repeat doesn't work                  | `passed_initial_delay` not tracked                  | Add flag to `RepeatInterval`, check in handler     |
| Repeat triggers immediately despite delay | Early return when `half_transition_count > 0`       | Only trigger immediately if `initial_delay <= 0`   |
| Input feels "sticky" after release        | Buffer swap not happening correctly                 | Verify swapping contents, not pointers             |
| Colors look wrong                         | RGBA vs ARGB byte order mismatch                    | Check platform expects `0xAARRGGBB`                |
| Game runs too fast without VSync          | No frame limiting                                   | Add manual sleep when VSync unavailable            |
| Audio clicks/pops                         | Phase discontinuity or buffer underrun              | Don't reset phase; check buffer fill timing        |
| Game freezes during line clear            | Animation timer not decrementing                    | Ensure `delta_time` subtraction in animation state |
| Piece spawns inside wall                  | Initial position not accounting for piece size      | Center based on piece width, not field center      |

---

## Checklist before submitting course

- [ ] PLAN.md exists and matches final lesson structure
- [ ] Each lesson builds and runs independently
- [ ] All code compiles with `-Wall -Wextra` without warnings
- [ ] Both X11 and Raylib backends work
- [ ] Comments explain WHY, not just WHAT
- [ ] Exercises are included and tested
- [ ] Build script has `--backend` and `-r` flags
- [ ] Input system uses double-buffering with proper swap
- [ ] `prepare_input_frame` preserves `ended_down`
- [ ] Auto-repeat uses `RepeatInterval` with all fields
- [ ] Audio (if included) works on both platforms

---

---

## Testing checklist

### Input testing

- [ ] Tap key once → action triggers once
- [ ] Hold key → action triggers after delay, then repeats
- [ ] Release key → action stops immediately
- [ ] Press two keys → both actions work independently
- [ ] Quick tap (< 1 frame) → action still triggers
- [ ] Works identically on X11 and Raylib

### Audio testing

- [ ] Sound effects play when triggered
- [ ] Multiple sounds can play simultaneously
- [ ] Sounds don't click/pop at start or end
- [ ] Music loops correctly
- [ ] Volume controls work
- [ ] No audio when game is muted/closed

### Rendering testing

- [ ] Colors appear correct (not swapped channels)
- [ ] No flickering or tearing
- [ ] UI elements are readable
- [ ] Overlays blend correctly
- [ ] Window resize doesn't break rendering

### Game logic testing

- [ ] Game initializes to correct state
- [ ] Restart resets everything properly
- [ ] Score/level increment correctly
- [ ] Game over triggers at right time
- [ ] Animations play at correct speed
- [ ] Delta time works (game speed independent of frame rate)

---

## Style guide

### Code formatting

```c
/* Function declarations: return type on same line */
void game_update(GameState *state, GameInput *input, float delta_time);

/* Braces: opening brace on same line */
if (condition) {
    /* code */
} else {
    /* code */
}

/* Switch: cases indented, break aligned with case */
switch (value) {
case CASE_A:
    do_something();
    break;
case CASE_B:
    do_other();
    break;
default:
    break;
}

/* Pointer declarations: star with type */
int *pointer;
GameState *state;

/* Constants: UPPER_SNAKE_CASE */
#define MAX_PLAYERS 4
#define CELL_SIZE 30

/* Types: PascalCase */
typedef struct { ... } GameState;
typedef enum { ... } MOVE_DIR;

/* Functions: snake_case */
void game_update(...);
void draw_rect(...);

/* Local variables: snake_case */
int current_index = 0;
float delta_time = 0.016f;
```

### Comment style

```c
/* Single-line comments use this style */

/*
 * Multi-line comments use this style.
 * Each line starts with a star.
 */

/* ═══════════════════════════════════════════════════════════════════════════
 * Section headers use box drawing characters
 * ═══════════════════════════════════════════════════════════════════════════
 */

void function(void) {
    /* Explain WHY, not WHAT */
    x += 1;  /* WRONG: increment x */
    x += 1;  /* RIGHT: move to next cell for collision check */

    /* Group related operations with blank lines and comments */

    /* Calculate new position */
    float new_x = pos_x + vel_x * dt;
    float new_y = pos_y + vel_y * dt;

    /* Check bounds */
    if (new_x < 0) new_x = 0;
    if (new_y < 0) new_y = 0;

    /* Apply position */
    pos_x = new_x;
    pos_y = new_y;
}
```

### File organization

```c
/* 1. Include guards (headers only) */
#ifndef GAME_H
#define GAME_H

/* 2. System includes */
#include <stdint.h>
#include <stdbool.h>

/* 3. Local includes */
#include "utils/backbuffer.h"

/* 4. Constants and macros */
#define FIELD_WIDTH 12
#define FIELD_HEIGHT 18

/* 5. Type definitions (enums first, then structs) */
typedef enum { ... } CellType;
typedef struct { ... } GameState;

/* 6. Function declarations */
void game_init(GameState *state);
void game_update(GameState *state, GameInput *input, float dt);

/* 7. End include guard */
#endif /* GAME_H */
```

---

## Final notes

### Philosophy

1. **Simplicity over cleverness** — Write code a beginner can follow
2. **Explicit over implicit** — Name things clearly, avoid magic numbers
3. **Platform independence** — Game logic never calls platform APIs
4. **Data-oriented** — Prefer structs of arrays, avoid deep hierarchies
5. **Comments explain WHY** — Code shows what, comments show reasoning

### When to deviate

These patterns work for simple 2D games. Deviate when:

- Performance requires it (profile first!)
- Game mechanics demand different architecture
- Platform limitations force changes

Document deviations clearly and explain the reasoning.

### Resources

- Handmade Hero (Casey Muratori) — Low-level game programming
- Raylib examples — Simple game patterns
- Lazy Foo SDL tutorials — Cross-platform basics
- Game Programming Patterns (Robert Nystrom) — Architecture patterns
