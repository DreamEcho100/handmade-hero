# Lesson 03: Mouse Input — Double-Buffering, UPDATE_BUTTON, and Letterbox Transform

## What we're building

Draw a small circle that follows the mouse cursor, and fill a circle on the canvas when
the left mouse button is held.  This proves the full input pipeline works — OS events →
`GameInput` → `game_update` → visible change — on both Raylib and X11.

## What you'll learn

- The `GameButtonState` double-buffer pattern (origin: Handmade Hero ep. 4)
- Why `ended_down` and `half_transition_count` are both needed
- The `UPDATE_BUTTON` and `BUTTON_PRESSED` convenience macros
- `prepare_input_frame` — why you reset counters but not button state
- How Raylib's letterbox scaling affects mouse coordinates (and how to fix it)
- Why X11 needs a start-of-frame `prev_x` snapshot before the event loop

## Prerequisites

- Lesson 02 complete (drawing primitives working on both backends)

---

## Step 1: Input types in `game.h`

```c
/* src/game.h  —  Input system (replaces the stub GameInput) */

/* -----------------------------------------------------------------------
 * GameButtonState
 *
 * JS analogy: imagine you have BOTH onKeyDown and onKeyUp handlers that
 * both fire into this struct.  At the start of each frame you ask:
 *   "was it pressed this frame?"  → BUTTON_PRESSED(b)
 *   "is it currently held?"       → b.ended_down
 *   "was it released this frame?" → BUTTON_RELEASED(b)
 *
 * half_transition_count counts state changes (press=+1, release=+1).
 * Two changes in one frame = pressed and released in the same frame.
 * That would be lost if we only stored ended_down — hence the counter.
 * ----------------------------------------------------------------------- */
typedef struct {
  int half_transition_count; /* +1 for every press OR release this frame */
  int ended_down;            /* 1 = currently held, 0 = currently released */
} GameButtonState;

/* Call this for every OS press/release event.
 * Only increments the counter when the state ACTUALLY changes —
 * prevents auto-repeat events from inflating the count. */
#define UPDATE_BUTTON(button, is_down)                      \
  do {                                                      \
    if ((button).ended_down != (is_down)) {                 \
      (button).half_transition_count++;                     \
      (button).ended_down = (is_down);                      \
    }                                                       \
  } while (0)

/* "Pressed this frame" = at least one transition AND currently held. */
#define BUTTON_PRESSED(b)  ((b).half_transition_count > 0 && (b).ended_down)
/* "Released this frame" = at least one transition AND not held. */
#define BUTTON_RELEASED(b) ((b).half_transition_count > 0 && !(b).ended_down)

/* Mouse state — position + left and right buttons. */
typedef struct {
  int x, y;             /* current canvas-space position this frame   */
  int prev_x, prev_y;   /* position last frame (for drawing lines)    */
  GameButtonState left;
  GameButtonState right;
} MouseInput;

/* Full input state — one GameInput lives in main(), reset each frame. */
typedef struct {
  MouseInput mouse;
  GameButtonState escape;   /* Escape — quit / back to title  */
  GameButtonState reset;    /* R key  — restart level         */
  GameButtonState gravity;  /* G key  — flip gravity          */
  GameButtonState enter;    /* Enter / Space — confirm        */
} GameInput;

/* -----------------------------------------------------------------------
 * prepare_input_frame
 * Call at the TOP of each frame, BEFORE platform_get_input().
 * Resets half_transition_count on every button so "pressed this frame"
 * semantics are fresh.  Does NOT reset ended_down — that is persistent state.
 * ----------------------------------------------------------------------- */
void prepare_input_frame(GameInput *input);
```

---

## Step 2: `prepare_input_frame` in `game.c`

```c
/* src/game.c */
void prepare_input_frame(GameInput *input) {
  /* Reset "transitions this frame" counters — leave ended_down alone.
   *
   * Why NOT reset ended_down?
   *   ended_down tracks whether the key is physically held right now.
   *   That state is persistent across frames.  If we zeroed it, a held
   *   key would look released every frame — input would stop working. */
  input->mouse.left.half_transition_count  = 0;
  input->mouse.right.half_transition_count = 0;
  input->escape.half_transition_count  = 0;
  input->reset.half_transition_count   = 0;
  input->gravity.half_transition_count = 0;
  input->enter.half_transition_count   = 0;
}
```

---

## Step 3: Raylib input — filling `platform_get_input`

```c
/* src/main_raylib.c  —  replace the stub platform_get_input */

void platform_get_input(GameInput *input) {
    /* ---------------------------------------------------------------
     * Mouse position — transform from window-space to canvas-space.
     *
     * GetMousePosition() returns window-space coordinates.  When the
     * window is resized the canvas is letterboxed (scaled to fit with
     * black borders).  We must undo scale + offset to get the true
     * canvas pixel.
     *
     * g_scale and g_offset_* are updated in platform_display_backbuffer
     * on every frame before this function is called.
     * --------------------------------------------------------------- */
    Vector2 pos = GetMousePosition();

    /* Save last-frame position before overwriting. */
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;

    /* Map window → canvas.  Clamp so out-of-canvas coords stay on edge. */
    int cx = (int)((pos.x - (float)g_offset_x) / g_scale);
    int cy = (int)((pos.y - (float)g_offset_y) / g_scale);
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx >= CANVAS_W) cx = CANVAS_W - 1;
    if (cy >= CANVAS_H) cy = CANVAS_H - 1;
    input->mouse.x = cx;
    input->mouse.y = cy;

    /* Buttons — Raylib is poll-based, so we call UPDATE_BUTTON each frame
     * with the current button state (down or up). */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        UPDATE_BUTTON(input->mouse.left, 1);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        UPDATE_BUTTON(input->mouse.left, 0);

    /* Keys */
    if (IsKeyPressed(KEY_ESCAPE))   UPDATE_BUTTON(input->escape,  1);
    if (IsKeyReleased(KEY_ESCAPE))  UPDATE_BUTTON(input->escape,  0);
    if (IsKeyPressed(KEY_R))        UPDATE_BUTTON(input->reset,   1);
    if (IsKeyReleased(KEY_R))       UPDATE_BUTTON(input->reset,   0);
    if (IsKeyPressed(KEY_G))        UPDATE_BUTTON(input->gravity, 1);
    if (IsKeyReleased(KEY_G))       UPDATE_BUTTON(input->gravity, 0);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        UPDATE_BUTTON(input->enter, 1);
    if (IsKeyReleased(KEY_ENTER) || IsKeyReleased(KEY_SPACE))
        UPDATE_BUTTON(input->enter, 0);
}
```

**What's happening:**

- Raylib sends distinct `Pressed` / `Released` events per frame.  We translate each into an `UPDATE_BUTTON` call which only increments the counter when the state actually changes.
- The letterbox transform: `canvas_x = (window_x - g_offset_x) / g_scale`.  Without it, clicking in the black border area would give coordinates outside the canvas.

---

## Step 4: X11 input — event loop with prev_x snapshot

```c
/* src/main_x11.c  —  replace the stub platform_get_input */

void platform_get_input(GameInput *input) {
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);

    /* ---------------------------------------------------------------
     * Snapshot prev position BEFORE processing events.
     *
     * X11 queues many MotionNotify events between frames (e.g. 10+
     * when the mouse moves fast).  If we updated prev_x inside the
     * event loop, by the end prev_x would hold the second-to-last
     * event's position — all earlier positions are lost.
     *
     * We draw brush lines from (prev_x, prev_y) to (x, y).  With the
     * snapshot approach, that line covers the ENTIRE path the mouse
     * traveled this frame — gapless, regardless of event count.
     * --------------------------------------------------------------- */
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;

    while (XPending(g_display)) {
        XEvent event;
        XNextEvent(g_display, &event);

        switch (event.type) {

        case MotionNotify:
            /* Update current position only — prev is already snapshotted. */
            input->mouse.x = event.xmotion.x;
            input->mouse.y = event.xmotion.y;
            break;

        case ButtonPress:
            if (event.xbutton.button == Button1) {
                /* On click, align prev to click pos so the first brush
                 * stamp is right where the user clicked, not where the
                 * mouse was before the button went down. */
                input->mouse.prev_x = event.xbutton.x;
                input->mouse.prev_y = event.xbutton.y;
                input->mouse.x      = event.xbutton.x;
                input->mouse.y      = event.xbutton.y;
                UPDATE_BUTTON(input->mouse.left, 1);
            }
            break;

        case ButtonRelease:
            if (event.xbutton.button == Button1)
                UPDATE_BUTTON(input->mouse.left, 0);
            break;

        case KeyPress: {
            KeySym sym = XLookupKeysym(&event.xkey, 0);
            if (sym == XK_Escape)                    UPDATE_BUTTON(input->escape,  1);
            if (sym == XK_r || sym == XK_R)          UPDATE_BUTTON(input->reset,   1);
            if (sym == XK_g || sym == XK_G)          UPDATE_BUTTON(input->gravity, 1);
            if (sym == XK_Return || sym == XK_space) UPDATE_BUTTON(input->enter,   1);
            break;
        }

        case KeyRelease: {
            KeySym sym = XLookupKeysym(&event.xkey, 0);
            if (sym == XK_Escape)                    UPDATE_BUTTON(input->escape,  0);
            if (sym == XK_r || sym == XK_R)          UPDATE_BUTTON(input->reset,   0);
            if (sym == XK_g || sym == XK_G)          UPDATE_BUTTON(input->gravity, 0);
            if (sym == XK_Return || sym == XK_space) UPDATE_BUTTON(input->enter,   0);
            break;
        }

        case ClientMessage:
            if ((Atom)event.xclient.data.l[0] == wm_delete)
                g_should_quit = 1;
            break;

        default:
            break;
        }
    }
}
```

---

## Step 5: Wire input into the game loop (both backends)

Update `game.c` to add a minimal `game_update` that uses the input:

```c
/* src/game.c  —  minimal game_update for Lesson 03 */
void game_update(GameState *state, GameInput *input, float delta_time) {
  (void)delta_time; /* unused in this lesson */

  /* Quit on Escape */
  if (BUTTON_PRESSED(input->escape))
    state->should_quit = 1;
}
```

Update `game_render` to draw a cursor circle:

```c
/* src/game.c  —  game_render for Lesson 03 */
void game_render(const GameState *state, GameBackbuffer *bb) {
  /* Clear canvas */
  int total = bb->width * bb->height;
  for (int i = 0; i < total; i++)
    bb->pixels[i] = COLOR_BG;

  /* Draw a small circle at the mouse position to verify input works.
   * We store mouse position in state for rendering — add a mouse_x/y field
   * to GameState, or pass input separately.  For simplicity, keep it in
   * GameState in this lesson. */
  draw_circle(bb, state->mouse_x, state->mouse_y, 5, COLOR_LINE);
}
```

Store mouse position in `GameState` (add two `int` fields and update `game_update`):

```c
/* src/game.h  —  add to GameState */
typedef struct {
  int should_quit;
  int mouse_x, mouse_y;   /* current canvas-space mouse position */
} GameState;

/* src/game.c  —  game_update: copy mouse coords into state */
void game_update(GameState *state, GameInput *input, float delta_time) {
  (void)delta_time;
  state->mouse_x = input->mouse.x;
  state->mouse_y = input->mouse.y;
  if (BUTTON_PRESSED(input->escape))
    state->should_quit = 1;
}
```

Update `main()` in both backends:

```c
/* In main(), inside the game loop */
prepare_input_frame(&input);
platform_get_input(&input);
game_update(&state, &input, delta_time);
game_render(&state, &bb);
platform_display_backbuffer(&bb);
```

---

## Platform Coverage Checklist

| Concern | Raylib | X11 |
|---------|--------|-----|
| Mouse position | `GetMousePosition()` → letterbox transform | `event.xmotion.x/y` |
| Mouse click | `IsMouseButtonPressed/Released` | `ButtonPress/ButtonRelease` event |
| Key press/release | `IsKeyPressed/Released` | `KeyPress/KeyRelease` event + `XLookupKeysym` |
| Prev position snapshot | Before `GetMousePosition` call | Before event loop |
| Window close | `WindowShouldClose()` | `WM_DELETE_WINDOW` ClientMessage |

---

## Build and run

```bash
# Raylib (add src/game.c to the compile command now)
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG -fsanitize=address,undefined \
      -o build/game src/main_raylib.c src/game.c \
      -lraylib -lm -lpthread -ldl
./build/game

# X11
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG -fsanitize=address,undefined \
      -o build/game src/main_x11.c src/game.c \
      -lX11 -lm
./build/game
```

**Expected output:** A sky-blue canvas with a small dark circle that follows the mouse cursor.  Press Escape to quit.

---

## Exercises

1. Add a "left button held" indicator: draw a bright-orange circle at the cursor only when `input->mouse.left.ended_down` is true.
2. What happens if you hold a key, alt-tab away, and come back?  Test it — does `ended_down` get stuck?  (Hint: X11 sends a `FocusOut` event you can use to reset all buttons.)
3. Add a `GameButtonState` for the right mouse button and light up a red circle when it is pressed.

---

## What's next

In Lesson 04 we spawn a single grain of sand, give it gravity, velocity, and a
floor collision — the foundation of the entire simulation.
