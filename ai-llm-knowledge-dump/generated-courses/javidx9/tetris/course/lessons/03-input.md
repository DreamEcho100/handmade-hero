# Lesson 03: Input

## What we're building

A coloured 50 × 50 square that moves around the screen with arrow keys or
WASD. The square is bounded by the window edges. This lesson introduces the
complete input system the final Tetris game uses — nothing will be thrown away.

## What you'll learn

- `union` — two ways to view the same block of memory
- `do { } while(0)` — the safe multi-statement macro pattern
- `GameButtonState` — tracking both *held state* and *transition count*
- Double-buffered input (`inputs[2]`) — why we keep "this frame" and "last frame"
- `prepare_input_frame` — the bookkeeping call that separates frames
- Why tracking `half_transition_count` catches fast taps that `ended_down` misses

## Prerequisites

Lessons 01 and 02 — you need the backbuffer and drawing primitives.

---

## The key insight: why not just check `IsKeyDown`?

Imagine a 30 fps game. Each frame takes ~33 ms. A player taps a key for only
20 ms — the key goes **down** and then **up** within a single frame.

| Time | Event |
|------|-------|
| 0 ms | key DOWN |
| 20 ms | key UP |
| 33 ms | frame ends |

At frame end, `IsKeyDown` returns `false`. If you only check that, you miss
the input entirely. The player pressed a button and *nothing happened* — that
is a bug.

**The fix:** track how many times the button *changed state* this frame
(`half_transition_count`). One down+up = 2 transitions. Even though
`ended_down = 0`, you can still detect "this button was pressed this frame"
by checking `half_transition_count >= 2`.

JS equivalent: this is like the difference between:
```js
// BAD — misses fast taps:
const isDown = keys.has('ArrowLeft');

// GOOD — catches every transition:
const transitions = keyTransitions.get('ArrowLeft') ?? 0;
```

---

## Step 1: `game_input.h` — the input types and macros

Create `src/game_input.h`. This header defines everything needed to represent
and update button state.

```c
/* src/game_input.h */
#ifndef GAME_INPUT_H
#define GAME_INPUT_H

/*
 * BUTTON_COUNT must equal the number of named fields in the struct inside
 * GameInput's union. Keep them in sync or the loop in prepare_input_frame
 * will miss buttons (too small) or read garbage (too large).
 */
#define BUTTON_COUNT 4

/* ══════════════════════════════════════════════════════════════════════════
 * GameButtonState — per-button state with transition tracking
 * ══════════════════════════════════════════════════════════════════════════
 *
 * JS equivalent (TypeScript):
 *   interface GameButtonState {
 *     endedDown: boolean;        // is the key held at end of frame?
 *     halfTransitionCount: number; // how many state changes this frame?
 *   }
 *
 * C struct: groups two related fields into one named type.
 * typedef makes `GameButtonState` usable without writing `struct GameButtonState`.
 */
typedef struct {
  int half_transition_count; /* 0 = no change this frame
                                1 = pressed once (or released once)
                                2 = tapped: pressed then released within frame
                                Values > 2 are possible for very fast toggles */
  int ended_down;            /* 1 = button is held at end of frame
                                0 = button is released at end of frame      */
} GameButtonState;

/* ══════════════════════════════════════════════════════════════════════════
 * GameInput — all player buttons for one frame
 * ══════════════════════════════════════════════════════════════════════════
 *
 * THE UNION TRICK
 * ────────────────
 * We want two ways to access the same buttons:
 *   1. By NAME:  input->move_left   — readable code
 *   2. By INDEX: input->buttons[i]  — loop over all buttons to reset/copy
 *
 * A C `union` makes both views occupy the SAME memory.
 * Changing `buttons[0]` changes `move_left` — they ARE the same bytes.
 *
 *   Memory layout (4 buttons × 8 bytes each = 32 bytes total):
 *   ┌─────────────┬──────────────┬─────────────┬──────────────┐
 *   │  buttons[0] │  buttons[1]  │  buttons[2] │  buttons[3]  │
 *   │  move_left  │  move_right  │  move_down  │  action      │
 *   └─────────────┴──────────────┴─────────────┴──────────────┘
 *
 * JS has no direct equivalent. The closest analogy:
 *   // TypeScript: object that is ALSO an array (but they'd be separate memory)
 *   interface GameInput {
 *     buttons: GameButtonState[];
 *     move_left:  GameButtonState;
 *     move_right: GameButtonState;
 *     move_down:  GameButtonState;
 *     action:     GameButtonState;
 *   }
 *   // In JS the array and named props would be separate. In C they share memory.
 *
 * C: union { T array[N]; struct { T a; T b; ... }; }
 *    array[0] and .a are the exact same bytes.
 *
 * WHY anonymous struct inside union?
 *   C allows an anonymous struct as a union member. Its fields are accessed
 *   directly on the parent (input->move_left) without an extra level:
 *   input->named.move_left would be wrong;  input->move_left is correct.
 */
typedef struct {
  union {
    GameButtonState buttons[BUTTON_COUNT]; /* index access for loops */
    struct {
      GameButtonState move_left;   /* left arrow / A key  */
      GameButtonState move_right;  /* right arrow / D key */
      GameButtonState move_down;   /* down arrow  / S key */
      GameButtonState action;      /* space / Z / X key   */
    };
  };
} GameInput;

/* ══════════════════════════════════════════════════════════════════════════
 * UPDATE_BUTTON — record one key event into a GameButtonState
 * ══════════════════════════════════════════════════════════════════════════
 *
 * Usage:
 *   UPDATE_BUTTON(input->move_left, IsKeyDown(KEY_LEFT));
 *
 * When called with `is_down = 1` and the button was previously down (0):
 *   - half_transition_count++ (a transition happened)
 *   - ended_down = 1         (now held)
 *
 * When called with `is_down = 0` and the button was down:
 *   - half_transition_count++ (a transition happened)
 *   - ended_down = 0         (now released)
 *
 * If `is_down` equals the current `ended_down`, nothing changes.
 *
 * WHY `do { ... } while(0)`?
 *   Macros are text substitution. Without the wrapper:
 *
 *   #define UPDATE_BUTTON(b, d) \
 *     if (b.ended_down != d) { b.htc++; b.ended_down = d; }
 *
 *   This breaks with:
 *     if (cond) UPDATE_BUTTON(b, d); else something();
 *   which expands to:
 *     if (cond) if (b.ended_down != d) { ... }; else something();
 *   The `else` attaches to the INNER `if`, not the outer one. Silent bug.
 *
 *   `do { ... } while(0)` wraps everything in a single statement. The
 *   trailing semicolon in `UPDATE_BUTTON(...);` matches the `while(0)`.
 *   The loop body always runs exactly once (while(0) is always false).
 *
 *   JS: functions don't have this problem — just use a function.
 *   C: macros need this trick when they contain multiple statements.
 */
#define UPDATE_BUTTON(button, is_down)                             \
  do {                                                             \
    if ((button).ended_down != (is_down)) {                        \
      (button).half_transition_count++;  /* record the transition */ \
      (button).ended_down = (is_down);   /* update final state    */ \
    }                                                              \
  } while (0)

/* ══════════════════════════════════════════════════════════════════════════
 * prepare_input_frame — bookkeeping between frames
 * ══════════════════════════════════════════════════════════════════════════
 *
 * Call this BEFORE reading any new key events for the upcoming frame.
 *
 * What it does:
 *   For each button:
 *   1. Copy ended_down from old_input to new_input.
 *      — Buttons that were held last frame are still held this frame
 *        (until a key-up event arrives). This preserves held state.
 *   2. Reset half_transition_count to 0 in new_input.
 *      — We start fresh: no transitions have been recorded yet this frame.
 *
 * WHY call it BEFORE reading keys?
 *   If we read keys first and THEN copy old state, we'd overwrite the new
 *   key data with stale data. Order matters: prepare first, then read.
 *
 * JS analogy:
 *   // At frame start, before polling keyboard:
 *   prevKeys = { ...keys };   // save last frame
 *   keys = {};                // fresh slate for this frame
 *   // THEN read new key events into `keys`
 *
 * `static inline`:
 *   `static` — this function is private to the file/TU that includes this header.
 *              It won't be exported as a linker symbol, avoiding duplicate-symbol
 *              errors when multiple .c files include game_input.h.
 *   `inline` — hint to the compiler to paste the function body at the call site
 *              (no call overhead). Not a guarantee; the compiler decides.
 */
static inline void prepare_input_frame(GameInput *old_input,
                                       GameInput *new_input) {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    /* Persist held state — if a button was down, it stays down next frame
     * unless a key-up event arrives. */
    new_input->buttons[i].ended_down          = old_input->buttons[i].ended_down;
    /* Reset transition counter — starts clean for the new frame. */
    new_input->buttons[i].half_transition_count = 0;
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 * Helper: was_button_pressed_this_frame
 * ══════════════════════════════════════════════════════════════════════════
 *
 * Returns 1 if the button was pressed (transitioned from up to down) at any
 * point this frame, even if it was already released by frame end.
 *
 * "Pressed" means: there was at least one up→down transition.
 *   - If ended_down==1 and half_transition_count > 0: pressed and still held.
 *   - If ended_down==0 and half_transition_count >= 2: tapped (pressed+released).
 *
 * JS: equivalent to checking `keydown` event fired this frame.
 */
static inline int was_button_pressed(const GameButtonState *btn) {
  return (btn->ended_down && btn->half_transition_count > 0)
      || (btn->half_transition_count >= 2);
}

#endif /* GAME_INPUT_H */
```

> **Why?** A C `union` has no JS equivalent. The closest mental model is
> imagining that `input.buttons` and `input.move_left` are not two separate
> properties — they literally occupy the same bytes in memory. Reading
> `input.buttons[0]` is reading the exact same bits as `input.move_left`.
> This is different from a JS object where each property has its own storage.

---

## Step 2: `main_raylib.c` — the moving square demo

Replace `src/main_raylib.c` with the complete lesson-03 version. The square
starts at the centre, moves with arrow keys / WASD, and is clamped to the
window boundaries.

```c
/* src/main_raylib.c — Lesson 03 complete */
#include "utils/backbuffer.h"
#include "utils/draw-shapes.h"
#include "game_input.h"         /* GameButtonState, GameInput, UPDATE_BUTTON,
                                   prepare_input_frame, was_button_pressed  */

#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>

#define FIELD_WIDTH   12
#define FIELD_HEIGHT  18
#define CELL_SIZE     30
#define SIDEBAR_WIDTH 200

/* Player square size and speed */
#define SQUARE_SIZE  50
#define SPEED        200  /* pixels per second */

int main(void) {
  int screen_width  = (FIELD_WIDTH  * CELL_SIZE) + SIDEBAR_WIDTH;
  int screen_height =  FIELD_HEIGHT * CELL_SIZE;

  /* --- Backbuffer (same as Lessons 01-02) --- */
  Backbuffer bb = {0};
  bb.width           = screen_width;
  bb.height          = screen_height;
  bb.bytes_per_pixel = 4;
  bb.pitch           = screen_width * 4;
  bb.pixels = (uint32_t *)malloc(
      (size_t)screen_width * (size_t)screen_height * sizeof(uint32_t)
  );
  if (!bb.pixels) {
    fprintf(stderr, "ERROR: could not allocate pixel buffer\n");
    return 1;
  }

  InitWindow(screen_width, screen_height, "Tetris - Lesson 03: Input");
  SetTargetFPS(60);

  Image img = {
    .data    = bb.pixels,
    .width   = bb.width,
    .height  = bb.height,
    .mipmaps = 1,
    .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  Texture2D texture = LoadTextureFromImage(img);

  /* ── Double-buffered input ──────────────────────────────────────────────
   *
   * We keep TWO GameInput structs:
   *   current_input — button states for the frame currently being built
   *   old_input     — button states from the previous frame
   *
   * WHY two buffers?
   *   To detect "just pressed this frame" (rising edge) we need to compare
   *   this frame's state with last frame's state. If we only had one struct
   *   and overwrote it, we'd lose the previous state.
   *
   * C: GameInput inputs[2] = {0};  — array of two structs, zero-initialised.
   *    &inputs[0] is a pointer to the first element.
   *
   * JS analogy:
   *   const inputs = [new GameInput(), new GameInput()];
   *   let currentInput  = inputs[0];
   *   let oldInput      = inputs[1];
   *
   * `= {0}`: zero-initialises both structs — all ended_down = 0, all
   * half_transition_count = 0. Safe starting state.
   */
  GameInput inputs[2]      = {0};
  GameInput *current_input = &inputs[0];
  GameInput *old_input     = &inputs[1];

  /* Player square position (top-left corner), as floats for smooth movement */
  float player_x = (float)(screen_width  / 2 - SQUARE_SIZE / 2);
  float player_y = (float)(screen_height / 2 - SQUARE_SIZE / 2);

  while (!WindowShouldClose()) {
    /*
     * delta_time: seconds since the last frame.
     * At 60 fps ≈ 0.0167 s. Used to make movement frame-rate independent.
     *
     * JS: const deltaTime = (performance.now() - lastTime) / 1000;
     */
    float dt = GetFrameTime();

    /* ── Prepare input (MUST come before reading keys) ──────────────────
     *
     * prepare_input_frame copies ended_down from old to current and resets
     * half_transition_count. After this call, current_input is a clean slate
     * for this frame's key events, but still knows which buttons were held.
     */
    prepare_input_frame(old_input, current_input);

    /* ── Read key state into current_input ──────────────────────────────
     *
     * UPDATE_BUTTON(button, is_down)
     *   is_down: 1 if the key is currently held, 0 if released.
     *
     * Raylib's IsKeyDown() polls the current state (held = true every frame).
     * UPDATE_BUTTON translates that boolean into our transition-counting
     * GameButtonState. If the state didn't change from last frame, nothing
     * happens (no spurious transitions).
     *
     * The `||` allows multiple keys to trigger the same action (WASD + arrows).
     */
    UPDATE_BUTTON(current_input->move_left,
                  IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
    UPDATE_BUTTON(current_input->move_right,
                  IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D));
    UPDATE_BUTTON(current_input->move_down,
                  IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S));

    /* Action: detect single tap (not held) using was_button_pressed */
    UPDATE_BUTTON(current_input->action,
                  IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_Z));

    /* Quit on Escape */
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
      break;
    }

    /* ── Update player position ─────────────────────────────────────────
     *
     * Move at SPEED pixels per second, multiplied by delta_time so movement
     * is frame-rate independent.
     *
     * JS: player.x += speed * deltaTime * (input.moveLeft ? -1 : 0);
     *
     * We check `ended_down` to know if the button is currently held.
     * (For movement we want continuous motion while held, not just on press.)
     */
    if (current_input->move_left.ended_down)  player_x -= SPEED * dt;
    if (current_input->move_right.ended_down) player_x += SPEED * dt;
    if (current_input->move_down.ended_down)  player_y += SPEED * dt;

    /* Up arrow / W key moves up */
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) player_y -= SPEED * dt;

    /* ── Clamp to window boundaries ─────────────────────────────────────
     *
     * CLAMP(val, lo, hi): keep val in [lo, hi].
     * The square's right edge is player_x + SQUARE_SIZE, so the max x is
     * screen_width - SQUARE_SIZE.
     *
     * JS: player.x = Math.min(Math.max(player.x, 0), maxX);
     * C:  player_x = CLAMP(player_x, 0.0f, (float)(screen_width - SQUARE_SIZE));
     *
     * `(float)` is an explicit type cast — converts int to float.
     * In JS all numbers are float-like; in C you must be explicit when mixing
     * int and float to avoid truncation.
     */
    if (player_x < 0.0f)
      player_x = 0.0f;
    if (player_x > (float)(screen_width - SQUARE_SIZE))
      player_x = (float)(screen_width - SQUARE_SIZE);
    if (player_y < 0.0f)
      player_y = 0.0f;
    if (player_y > (float)(screen_height - SQUARE_SIZE))
      player_y = (float)(screen_height - SQUARE_SIZE);

    /* ── Render ─────────────────────────────────────────────────────────── */
    /* Clear to dark background */
    draw_rect(&bb, 0, 0, screen_width, screen_height, GAME_RGB(10, 10, 40));

    /*
     * Draw the player square. Color changes when the action button is held.
     *
     * (int) cast: draw_rect takes int coordinates; player_x is float.
     * Casting truncates the fractional part (rounds toward zero).
     */
    uint32_t sq_color = current_input->action.ended_down
      ? GAME_RGB(255, 255, 0)   /* yellow when action held */
      : GAME_RGB(255, 80,  80); /* red-ish normally        */

    draw_rect(&bb, (int)player_x, (int)player_y, SQUARE_SIZE, SQUARE_SIZE,
              sq_color);

    /* Draw a thin guide border around the window */
    draw_rect(&bb, 0, 0, screen_width, 2, GAME_RGB(80, 80, 80));  /* top */
    draw_rect(&bb, 0, screen_height-2, screen_width, 2, GAME_RGB(80, 80, 80)); /* bottom */
    draw_rect(&bb, 0, 0, 2, screen_height, GAME_RGB(80, 80, 80));  /* left */
    draw_rect(&bb, screen_width-2, 0, 2, screen_height, GAME_RGB(80, 80, 80)); /* right */

    /* Upload and display */
    UpdateTexture(texture, bb.pixels);
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(texture, 0, 0, WHITE);
    EndDrawing();

    /* ── Swap input buffers ─────────────────────────────────────────────
     *
     * At frame end, current becomes old so next frame can compare.
     *
     * WHY swap contents, not pointers?
     *   The `inputs` array and the `old_input` / `current_input` pointers
     *   exist as separate variables. Swapping the POINTER variables (like
     *   `tmp = current_input; current_input = old_input; old_input = tmp;`)
     *   only swaps the local pointer — the actual array is unchanged.
     *   The NEXT call to prepare_input_frame would read the wrong buffer.
     *
     *   Instead, swap the CONTENTS (struct copy):
     *     GameInput temp = *current_input;  // copy current to temp
     *     *current_input = *old_input;      // copy old into current slot
     *     *old_input     = temp;            // copy temp into old slot
     *
     *   After this: the memory at `inputs[0]` holds what was current_input,
     *   and the memory at `inputs[1]` holds what was old_input.
     *   The POINTERS (current_input, old_input) don't change.
     *
     * JS: structuredClone() swap:
     *   const temp = structuredClone(currentInput);
     *   Object.assign(currentInput, oldInput);
     *   Object.assign(oldInput, temp);
     */
    GameInput temp   = *current_input;
    *current_input   = *old_input;
    *old_input       = temp;
  }

  UnloadTexture(texture);
  CloseWindow();
  free(bb.pixels);
  bb.pixels = NULL;
  return 0;
}
```

> **Why `GameInput inputs[2] = {0}` instead of two separate variables?** It is
> purely a style choice here. Grouping them in an array makes it obvious they
> are a pair. `&inputs[0]` is a pointer to the first element; `&inputs[1]` is
> a pointer to the second. Either approach works — the final game code in
> `main_raylib.c` uses the array form.

> **Handmade Hero principle:** Input is explicit data, not event callbacks.
> There are no `addEventListener` calls, no lambdas, no hidden event queues.
> Every frame, the platform reads raw key state from Raylib and packs it into
> `GameButtonState` values. The game receives a plain `GameInput *` and reads
> fields. Clean, predictable, debuggable.

---

## Build and run

```bash
# From src/
clang -I. main_raylib.c utils/draw-shapes.c -lraylib -lm -o tetris
./tetris
```

`game_input.h` is a header-only file (it only contains `static inline`
functions and macros) — it does NOT need its own `.c` file in the build command.

---

## Expected result

- Dark blue-black background with a grey border
- A red 50 × 50 square at the window centre
- Arrow keys or WASD move the square smoothly (frame-rate independent)
- Holding Space or Z turns the square yellow
- The square is clamped and cannot leave the window
- Escape or Q closes the window

---

## Common mistakes

> **Common mistake:**
> ```c
> #define UPDATE_BUTTON(button, is_down)          \
>   if ((button).ended_down != (is_down)) {       \
>     (button).half_transition_count++;            \
>     (button).ended_down = (is_down);             \
>   }
> ```
> This fails because: without `do { } while(0)`, using this macro in an
> `if/else` chain breaks:
> ```c
> if (cond) UPDATE_BUTTON(b, 1); else do_something();
> ```
> expands to two statements before the `else`, and the `else` attaches to the
> wrong `if`. The `do { } while(0)` wrapper makes the entire macro a single
> statement so the trailing semicolon is consumed by `while(0)`.
>
> **Correct version:**
> ```c
> #define UPDATE_BUTTON(button, is_down)     \
>   do {                                     \
>     if ((button).ended_down != (is_down))  \
>       { ... }                              \
>   } while (0)
> ```

---

> **Common mistake:**
> ```c
> /* Reading keys BEFORE prepare_input_frame */
> UPDATE_BUTTON(current_input->move_left, IsKeyDown(KEY_LEFT));
> prepare_input_frame(old_input, current_input);  /* WRONG ORDER */
> ```
> This fails because: `prepare_input_frame` resets `half_transition_count` to
> 0 and copies `ended_down` from old to new. If you call it AFTER reading keys,
> it wipes out the transitions you just recorded.
>
> **Correct order:** `prepare_input_frame` first, then `UPDATE_BUTTON`.

---

> **Common mistake:** confusing `ended_down` with "was pressed this frame".
> ```c
> /* Want: "did the player just press space?" */
> if (current_input->action.ended_down) { /* WRONG — true for entire hold */ }
> ```
> This will fire every frame the key is held, not just the frame it was pressed.
>
> **Correct version:**
> ```c
> if (was_button_pressed(&current_input->action)) {
>   /* fires only on the frame the key transitions from up to down */
> }
> ```

---

> **Common mistake:**
> ```c
> player_x += SPEED;  /* no delta_time */
> ```
> This moves SPEED *pixels per frame*, not per second. At 30 fps the player
> moves half as fast as at 60 fps — gameplay depends on the frame rate.
>
> **Correct version:** `player_x += SPEED * dt;` — multiply by delta_time to
> get a consistent speed regardless of frame rate.

---

## Exercises

1. **Detect a single press.** Add a counter that increments by 1 each time
   the action button (Space) is *pressed* (not held). Display it in the
   terminal with `printf("Action presses: %d\n", count);`. Hold the key and
   observe that `ended_down` fires every frame, but `was_button_pressed`
   increments only once per press.

2. **Add an "up" button.** Currently `KEY_UP`/`W` are handled directly with
   `IsKeyDown`. Refactor to add an `up` field to the `GameInput` struct (and
   increase `BUTTON_COUNT` to 5). Pipe `KEY_UP`/`W` through `UPDATE_BUTTON`.
   Confirm the behaviour is identical to the direct `IsKeyDown` version.

3. **Trace the union layout.** Add this debug print after declaring `inputs`:
   ```c
   printf("sizeof GameInput       = %zu bytes\n", sizeof(GameInput));
   printf("sizeof buttons[0]      = %zu bytes\n", sizeof(inputs[0].buttons[0]));
   printf("&buttons[0] == &move_left? %s\n",
          (void*)&inputs[0].buttons[0] == (void*)&inputs[0].move_left
          ? "YES" : "NO");
   ```
   Confirm that `buttons[0]` and `move_left` have the same address — that is
   the union at work.

---

## What's next

In **Lesson 04** we split the code into three files: `game.h` (types),
`game.c` (logic), and `main_raylib.c` (platform). You'll learn about header
guards, multi-file compilation, `extern` declarations, and `= {0}`
zero-initialisation. The moving square stays — we just reorganise *where* the
code lives to match the architecture the full Tetris game uses.
