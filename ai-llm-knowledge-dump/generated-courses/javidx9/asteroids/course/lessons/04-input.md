# Lesson 04 — Input Abstraction

## What you'll build
A `GameInput` struct with a C99 anonymous union, a double-buffer system for
correctly detecting "just pressed" vs "held", and four buttons (left, right,
up, fire).  You'll move and rotate a triangle on screen with arrow keys.

---

## Core Concepts

### 1. `GameButtonState` — More than On/Off

A simple boolean ("is the key held?") can't distinguish:
- "key was held last frame and still held now" (repeat — do NOT retrigger)
- "key was up, now it's down" (just pressed — trigger once)

We track both:

```c
typedef struct {
    int ended_down;            // 1 if held at end of frame
    int half_transition_count; // number of press/release events this frame
} GameButtonState;

// JS equivalent:
// interface ButtonState { endedDown: boolean; halfTransitions: number; }

// "Just pressed" check:
// bool justPressed = btn.endedDown && btn.half_transition_count > 0;
```

### 2. `GameInput` — Anonymous Union

C99 allows a struct with a **union** inside it with no name.  This lets the
same memory be accessed two different ways:

```c
#define BUTTON_COUNT 4

typedef struct {
    union {                             // anonymous union — no name needed
        GameButtonState buttons[BUTTON_COUNT];
        struct {                        // anonymous struct inside the union
            GameButtonState left;       // buttons[0]
            GameButtonState right;      // buttons[1]
            GameButtonState up;         // buttons[2]
            GameButtonState fire;       // buttons[3]
        };
    };
    int quit;
} GameInput;

// Named access:   input.up.ended_down
// Indexed access: input.buttons[2].ended_down  (same memory)
```

The indexed form is useful for loops (e.g., in `prepare_input_frame`).  
The named form is self-documenting in game logic.

### 3. `prepare_input_frame` — The Double-Buffer Pattern

```c
void prepare_input_frame(const GameInput *old, GameInput *current) {
    for (int i = 0; i < BUTTON_COUNT; i++) {
        // Carry forward the held state from last frame
        current->buttons[i].ended_down = old->buttons[i].ended_down;
        // Reset transition counter — platform will re-increment as events arrive
        current->buttons[i].half_transition_count = 0;
    }
    current->quit = 0;  // quit is one-shot, never carry forward
}
```

**Why two buffers?**  
After `prepare_input_frame`, if no events arrive `current` still reflects the
correct "key held" state from last frame.  This is crucial: in a 60fps loop,
a key held for 1 second spans 60 frames but generates only 2 OS events
(key-down + key-up).  Without carrying forward `ended_down`, buttons would
appear pressed only on the one frame the key-down event arrived.

### 4. Call Order Each Frame

```c
// inputs[0] = "old",  inputs[1] = "current"
platform_swap_input_buffers(&inputs[0], &inputs[1]); // current → old
prepare_input_frame(&inputs[0], &inputs[1]);          // carry + reset
// ... platform reads OS events into inputs[1] ...
game_update(&state, &inputs[1], dt);                  // game reads current
```

`platform_swap_input_buffers` is a simple struct swap (three assignments).

### 5. Reading Input in Game Logic

```c
// Rotate ship
if (input->left.ended_down)   ship.angle -= TURN_SPEED * dt;
if (input->right.ended_down)  ship.angle += TURN_SPEED * dt;

// Fire — trigger only on "just pressed", not while held
if (input->fire.ended_down && input->fire.half_transition_count > 0)
    add_bullet(&state);

// Quit
if (input->quit) running = 0;
```

---

## Platform Input — X11 vs Raylib

The `GameInput` struct and `prepare_input_frame` are platform-agnostic.
What differs is how each backend reads OS events and fills the buttons.

### Raylib input (simple)

```c
static void process_input(const GameInput *old, GameInput *current) {
    prepare_input_frame(old, current);

    // IsKeyDown → still held; detect transition by comparing to last frame
    #define MAP_KEY(BTN, KEY1, KEY2)                           \
        do {                                                    \
            int held = IsKeyDown(KEY1) || IsKeyDown(KEY2);     \
            if (held != (BTN).ended_down) {                    \
                (BTN).ended_down = held;                       \
                (BTN).half_transition_count++;                 \
            }                                                   \
        } while(0)

    MAP_KEY(current->left,  KEY_LEFT,  KEY_A);
    MAP_KEY(current->right, KEY_RIGHT, KEY_D);
    MAP_KEY(current->up,    KEY_UP,    KEY_W);
    MAP_KEY(current->fire,  KEY_SPACE, KEY_ENTER);
    #undef MAP_KEY

    if (WindowShouldClose() || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q))
        current->quit = 1;
}
```

Raylib abstracts the OS event loop internally.  `IsKeyDown(key)` returns 1
while the physical key is held.

### X11 input (manual event loop)

X11 delivers raw events via `XNextEvent`.  We must decode keycodes manually:

```c
static void process_events(GameInput *input) {
    while (XPending(g_display) > 0) {
        XEvent event;
        XNextEvent(g_display, &event);

        switch (event.type) {

            // Window × button → ClientMessage with WM_DELETE_WINDOW atom
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == g_wm_delete) {
                    input->quit = 1;
                    g_is_running = 0;
                }
                break;

            // Window resized → update g_win_w / g_win_h for letterbox
            case ConfigureNotify:
                g_win_w = event.xconfigure.width;
                g_win_h = event.xconfigure.height;
                break;

            case KeyPress:
            case KeyRelease: {
                int is_press = (event.type == KeyPress);
                // XkbKeycodeToKeysym: hardware keycode → logical key name
                // The two 0s mean: default group, default level (shift state)
                KeySym sym = XkbKeycodeToKeysym(g_display,
                                                event.xkey.keycode, 0, 0);
                GameButtonState *btn = NULL;
                switch (sym) {
                    case XK_Left:  case XK_a: btn = &input->left;  break;
                    case XK_Right: case XK_d: btn = &input->right; break;
                    case XK_Up:    case XK_w: btn = &input->up;    break;
                    case XK_space: case XK_Return: btn = &input->fire; break;
                    case XK_Escape: case XK_q:
                        input->quit = 1;  g_is_running = 0;  break;
                }
                if (btn && btn->ended_down != is_press) {
                    btn->ended_down = is_press;
                    btn->half_transition_count++;
                }
                break;
            }
        }
    }
}
```

**Three X11-specific fixes applied (learned from Snake course):**

| Fix | Without it | With it |
|-----|-----------|---------|
| `XkbKeycodeToKeysym` instead of `XLookupKeysym` | Wrong keys on non-QWERTY layouts | Correct key names |
| `XkbSetDetectableAutoRepeat(display, True, NULL)` | OS fires fake KeyRelease+KeyPress pairs while held | Single KeyPress, no flicker |
| `WM_DELETE_WINDOW` + `XSetWMProtocols` | Window × button kills process → ALSA left open → terminal hangs | Clean shutdown path |

### Frame timing: X11 vs Raylib

| Backend | dt source | Why |
|---------|-----------|-----|
| Raylib | `GetFrameTime()` | Raylib measures internally, capped by `SetTargetFPS(60)` |
| X11 | `clock_gettime(CLOCK_MONOTONIC, …)` | Must measure manually; `CLOCK_MONOTONIC` never goes backwards |

```c
// X11 timing (in main loop):
struct timespec ts_last, ts_now;
clock_gettime(CLOCK_MONOTONIC, &ts_last);
// ...
clock_gettime(CLOCK_MONOTONIC, &ts_now);
double dt = (ts_now.tv_sec  - ts_last.tv_sec) +
            (ts_now.tv_nsec - ts_last.tv_nsec) * 1e-9;
ts_last = ts_now;
if (dt > 0.066) dt = 0.066;  // cap: prevents teleport after pause/focus loss
```

---

## Implement It

At this stage, in `main.c` (lesson stage, before full game.c split):

```c
GameInput inputs[2] = {0};
float ship_angle = 0.0f;
int ship_x = SCREEN_W / 2, ship_y = SCREEN_H / 2;

while (game_running) {
    float dt = get_delta_time();

    platform_swap_input_buffers(&inputs[0], &inputs[1]);
    prepare_input_frame(&inputs[0], &inputs[1]);
    platform_get_input(&inputs[1]);   // OS-specific event loop

    if (inputs[1].quit) break;

    // Simple ship movement
    if (inputs[1].left.ended_down)  ship_angle -= 2.0f * dt;
    if (inputs[1].right.ended_down) ship_angle += 2.0f * dt;

    if (inputs[1].fire.half_transition_count > 0 && inputs[1].fire.ended_down)
        fprintf(stderr, "FIRE!\n");

    // Clear screen
    draw_rect(bb, 0, 0, SCREEN_W, SCREEN_H, COLOR_BLACK);
    // Draw triangle at ship position rotated by ship_angle
    draw_triangle_wireframe(bb, ship_x, ship_y, ship_angle, COLOR_WHITE);
}
```

---

## Verify

- Arrow keys rotate/move the triangle
- Space (or Enter) prints "FIRE!" to the terminal **once** per keypress,
  not continuously while held
- Both Raylib and X11 backends produce identical behaviour

---

## Summary

| Concept | C | JS equivalent |
|---------|---|---------------|
| "Just pressed" detection | `ended_down && half_transition_count > 0` | `keydown` event fires once |
| Double buffer | `inputs[2]` + swap + prepare | `prevKeys = {...currentKeys}` |
| Union fields | `input.up` = `input.buttons[2]` | Computed property / alias |
| X11 keycode decode | `XkbKeycodeToKeysym` | `event.key` in `addEventListener` |
| X11 auto-repeat fix | `XkbSetDetectableAutoRepeat` | No equivalent (browser handles it) |
| X11 clean close | `WM_DELETE_WINDOW` protocol | `window.addEventListener('beforeunload')` |
