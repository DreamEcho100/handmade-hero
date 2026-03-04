# Lesson 03 — The Double-Buffered Input Model

**Source files:** `src/main_x11.c`, `src/game.h`, `src/game.c`, `src/platform.h`
**Functions:** `platform_get_input()`, `prepare_input_frame()`, `platform_swap_input_buffers()`

---

## What We're Building

A two-frame input system that correctly tracks whether a key was *just pressed*
this frame, *held* from a previous frame, or *released* — using only the
press/release events that X11 delivers.

---

## The Concept

### JS analogy — manual keydown/keyup tracking

In a browser game you might write:

```js
const keys = {};
window.addEventListener('keydown', e => { if (!e.repeat) keys[e.code] = true;  });
window.addEventListener('keyup',   e => {                keys[e.code] = false; });

// Each frame:
function gameLoop() {
    const justPressedLeft = keys['ArrowLeft'] && !prevKeys['ArrowLeft'];
    Object.assign(prevKeys, keys);   // copy current → previous
    // ...
}
```

The Handmade Hero input model does exactly this but in C, with two important
upgrades:

1. **Sub-frame taps are counted.**  If a key is pressed *and released* within
   a single 16 ms frame, JS's `e.repeat` trick loses it.  The
   `half_transition_count` field captures it.

2. **The OS event stream is explicit.**  X11 sends raw `KeyPress` / `KeyRelease`
   events.  The platform drains them and calls `UPDATE_BUTTON` for each one.
   There are no magic browser APIs hiding the mechanics.

---

## The Code

### `GameButtonState` — one button's state for one frame (`src/game.h`)

```c
typedef struct {
    int half_transition_count;  /* How many press/release events happened this frame */
    int ended_down;             /* 1 = key is held RIGHT NOW; 0 = key is up          */
} GameButtonState;
```

`half_transition_count` is called "half" transition because a full transition
is a press *and* a release (two halves).  A normal key tap during a 16 ms frame
produces count = 2 (press → release).  A key held across frames produces
count = 0 every frame except the first (the first frame: press event → count = 1).

**"Just pressed this frame"** check:

```c
button.ended_down && button.half_transition_count > 0
```

- `ended_down` = 1 confirms the key is (or was) down.
- `half_transition_count > 0` confirms something changed *this* frame, so
  this is not just a hold-from-last-frame.

JS equivalent:
```js
const justPressed = keys[code] && !prevKeys[code];
```

### `UPDATE_BUTTON` — recording an event (`src/game.h`)

```c
#define UPDATE_BUTTON(button, is_down)                     \
    do {                                                   \
        if ((button).ended_down != (is_down)) {            \
            (button).half_transition_count++;              \
            (button).ended_down = (is_down);               \
        }                                                  \
    } while (0)
```

The `if` guard prevents double-counting: if the OS sends a second `KeyPress`
event for the same key (e.g. due to auto-repeat before we disable it),
`ended_down` is already 1, so nothing is recorded.

JS equivalent:
```js
function updateButton(btn, isDown) {
    if (btn.endedDown !== isDown) {
        btn.halfTransitionCount++;
        btn.endedDown = isDown;
    }
}
```

### `GameInput` — the union trick (`src/game.h`)

```c
typedef struct {
    union {
        GameButtonState buttons[BUTTON_COUNT];   /* Array access for loops     */
        struct {
            GameButtonState turn_left;   /* Left arrow or A                    */
            GameButtonState turn_right;  /* Right arrow or D                   */
        };
    };
    int restart;  /* R or Space — one-shot, not a held button                  */
    int quit;     /* Q or Escape — one-shot                                    */
} GameInput;
```

The anonymous `union` means `input->buttons[0]` and `input->turn_left` refer
to the **same memory**.  This is a C idiom for getting both array iteration and
named access without duplication.

JS analogy:
```js
class GameInput {
    constructor() {
        this.buttons = [new ButtonState(), new ButtonState()]; // array access
    }
    get turn_left()  { return this.buttons[0]; }  // named access
    get turn_right() { return this.buttons[1]; }
}
```

### X11 event draining — `platform_get_input` (`src/main_x11.c`)

```c
void platform_get_input(GameInput *input) {
    /* XPending() returns the number of events waiting in the queue.
     * We drain ALL of them — not just one — so no event is missed this frame.
     * JS: while (eventQueue.length > 0) { const ev = eventQueue.shift(); ... }  */
    while (XPending(g_display)) {
        XEvent ev;
        XNextEvent(g_display, &ev);   /* Remove one event from the queue */

        if (ev.type == KeyPress || ev.type == KeyRelease) {
            int    is_down = (ev.type == KeyPress) ? 1 : 0;
            KeySym ks      = XkbKeycodeToKeysym(g_display,
                                 ev.xkey.keycode, 0, 0); /* base keysym */

            switch (ks) {
                case XK_Left:  case XK_a:
                    UPDATE_BUTTON(input->turn_left,  is_down); break;
                case XK_Right: case XK_d:
                    UPDATE_BUTTON(input->turn_right, is_down); break;
                case XK_r:     case XK_space:
                    if (is_down) input->restart = 1; break;
                case XK_q:     case XK_Escape:
                    if (is_down) { input->quit = 1; g_is_running = 0; } break;
                default: break;
            }
        }
    }
}
```

**Why `XkbSetDetectableAutoRepeat`?**

Without it, holding a key causes X11 to fire rapid fake `KeyRelease` + `KeyPress`
pairs (auto-repeat).  Each pair would call `UPDATE_BUTTON` twice per repeat
event, incrementing `half_transition_count` and toggling `ended_down`
needlessly — making it look like the key is being tapped rapidly even though
it's held.  `XkbSetDetectableAutoRepeat(display, True, NULL)` tells X11 to
suppress those fake release events.  Only the real release at the end fires.

This is called in `platform_init()`:

```c
XkbSetDetectableAutoRepeat(g_display, True, NULL);
```

**Why `XkbKeycodeToKeysym` instead of `XLookupKeysym`?**

`XLookupKeysym(&ev.xkey, 0)` can return inconsistent results when XKB (the
modern X keyboard extension) is active — particularly with detectable
auto-repeat enabled.  `XkbKeycodeToKeysym(display, keycode, group=0, level=0)`
always returns the base unshifted keysym for the physical key, regardless of
caps lock, shift state, or auto-repeat mode — which is exactly what a game's
input layer needs.

```c
/* Wrong — may misbehave with XkbSetDetectableAutoRepeat */
KeySym ks = XLookupKeysym(&ev.xkey, 0);

/* Correct — always base keysym for the physical key */
KeySym ks = XkbKeycodeToKeysym(g_display, ev.xkey.keycode, 0, 0);
```

**Handling the window close button (`WM_DELETE_WINDOW`)**

When the user clicks the window's X (close) button, the window manager sends a
`ClientMessage` event rather than destroying the window directly — but only if
the application has opted in by calling `XSetWMProtocols`.  Without opting in,
the WM destroys the window while the game loop is still running, causing the
next `glXSwapBuffers` call to block forever — the terminal hangs and only
`CTRL+C` kills it.

The fix is two lines: register the protocol in `platform_init`, and handle the
message in the event loop:

```c
/* In platform_init — register the protocol */
Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
XSetWMProtocols(g_display, g_window, &wm_delete, 1);
g_wm_delete_window = wm_delete;
```

```c
/* In platform_get_input — handle the close message */
} else if (ev.type == ClientMessage) {
    if ((Atom)ev.xclient.data.l[0] == g_wm_delete_window) {
        input->quit  = 1;
        g_is_running = 0;
    }
}
```

With `g_is_running = 0` set, the game loop exits cleanly on the next iteration
and `platform_shutdown` frees all resources before the process ends.

### `prepare_input_frame` — preserving held state (`src/game.c`)

```c
/* COURSE NOTE: The reference source uses a 1-arg version:
 *   prepare_input_frame(GameInput *input)
 * which ONLY resets half_transition_count but does NOT copy ended_down.
 * That works on Raylib (which polls IsKeyDown every frame) but BREAKS on
 * X11 (which only sends one KeyPress event, not a continuous stream).
 * Our 2-arg version is correct for event-driven backends.              */
void prepare_input_frame(const GameInput *old_input, GameInput *current_input) {
    int i;
    for (i = 0; i < BUTTON_COUNT; i++) {
        /* Step 1: carry held state forward — without this, a held key
         * appears "up" on every frame after the first press event.    */
        current_input->buttons[i].ended_down =
            old_input->buttons[i].ended_down;

        /* Step 2: clear event count — each frame starts fresh.        */
        current_input->buttons[i].half_transition_count = 0;
    }
    /* One-shot flags: reset to 0 so they only fire for one frame.     */
    current_input->restart = 0;
    current_input->quit    = 0;
}
```

JS equivalent:
```js
function prepareInputFrame(oldInput, currentInput) {
    for (let i = 0; i < BUTTON_COUNT; i++) {
        currentInput.buttons[i].endedDown          = oldInput.buttons[i].endedDown;
        currentInput.buttons[i].halfTransitionCount = 0;
    }
    currentInput.restart = false;
    currentInput.quit    = false;
}
```

### `platform_swap_input_buffers` — the double-buffer swap (`src/platform.h`)

```c
static inline void platform_swap_input_buffers(GameInput *old_input,
                                                GameInput *current_input) {
    GameInput temp   = *old_input;      /* copy old → temp               */
    *old_input       = *current_input;  /* old  ← current                */
    *current_input   = temp;            /* current ← temp (former old)   */
}
```

JS equivalent: `[oldInput, currentInput] = [currentInput, oldInput]` — the
destructuring swap idiom.

### The correct per-frame call order (`src/main_x11.c → main`)

```c
/* Each frame: */

/* Step 1: swap contents — inputs[0] gets last frame's data,
 *                          inputs[1] gets a copy ready for prepare.    */
platform_swap_input_buffers(&inputs[0], &inputs[1]);

/* Step 2: copy ended_down old → current; clear transition counts.
 * Must come AFTER swap so inputs[0] is definitely "old".              */
prepare_input_frame(&inputs[0], &inputs[1]);

/* Step 3: fill new events into inputs[1].
 * Must come AFTER prepare so the cleared counts are ready.            */
platform_get_input(&inputs[1]);

/* Step 4: game logic reads inputs[1] — the fully built current frame. */
snake_update(&state, &inputs[1], delta_time);
```

Violating this order produces bugs that are hard to diagnose:
- Swapping *after* prepare means `ended_down` from the wrong frame is copied.
- Calling `get_input` before `prepare` means transition counts from the previous
  frame still pollute the current one.

---

## What To Notice

- **The 1-arg vs 2-arg `prepare_input_frame`** — the reference source's 1-arg
  version only resets counters.  On Raylib it works because Raylib's `IsKeyDown`
  is a poll.  On X11 you only get one `KeyPress` event per actual press, so
  `ended_down` must be explicitly carried forward.  Our 2-arg version is
  correct for both backends.

- **`UPDATE_BUTTON` only fires on state *changes*** — the `if ended_down != is_down`
  guard means pressing the same key twice without releasing only counts once.
  This matches real hardware: a key can only be up or down, not "more down".

- **One-shot vs held buttons** — `restart` and `quit` are plain `int` fields
  (not `GameButtonState`) because they are edge-triggered (fire once per press)
  and the game doesn't need to know *how long* they've been held.  Using
  `GameButtonState` for them would work but would be over-engineered.

- **`XPending` drains the full queue** — if you called `XNextEvent` without the
  `while (XPending(...))` loop, events would back up one-per-frame, introducing
  multi-frame input lag on fast typing.

- **`XkbKeycodeToKeysym` is more reliable than `XLookupKeysym`** — the Xkb
  variant always returns the base unshifted keysym for a physical key, which is
  the correct semantic for game input.  `XLookupKeysym` can return inconsistent
  results when detectable auto-repeat is enabled.

- **`WM_DELETE_WINDOW` prevents the terminal hang** — without registering this
  protocol, clicking the window's X button causes the WM to forcefully destroy
  the window while `glXSwapBuffers` is still running, blocking forever.  Always
  register `WM_DELETE_WINDOW` and handle `ClientMessage` events in any X11 app.

---

## Exercises

1. **Observe sub-frame taps.**
   Add a temporary `printf` in `snake_update()` inside the turn-right check:
   ```c
   if (input->turn_right.ended_down &&
       input->turn_right.half_transition_count > 0) {
       printf("turn_right: ended_down=%d htc=%d\n",
              input->turn_right.ended_down,
              input->turn_right.half_transition_count);
   }
   ```
   Press the right arrow rapidly and observe the output.  You should see
   `htc=1` on the press frame and nothing (no print) on frames where the key is
   held without being newly pressed.

2. **Break the 1-arg model.**
   Temporarily change `prepare_input_frame` to the reference's 1-arg version:
   ```c
   void prepare_input_frame_broken(GameInput *current_input) {
       int i;
       for (i = 0; i < BUTTON_COUNT; i++)
           current_input->buttons[i].half_transition_count = 0;
       current_input->restart = 0;
       current_input->quit = 0;
   }
   ```
   Call it with only `&inputs[1]`.  Hold the right arrow key and notice that
   the snake stops turning after the first press event — `ended_down` is no
   longer preserved.  This is the X11 held-key bug the 2-arg version fixes.
   Restore the 2-arg version before continuing.

3. **Add a new one-shot key.**
   Map the `P` key to a `pause` field in `GameInput`.  In `platform_get_input`,
   add:
   ```c
   case XK_p: if (is_down) input->pause = 1; break;
   ```
   In `prepare_input_frame`, add `current_input->pause = 0;`.
   Print a message in `snake_update` when `input->pause` is set.
   Notice that the message prints exactly once per key press, not every frame.
