# Lesson 05 — Input Abstraction

## What you'll learn
- `GameButtonState`: capturing both "held?" and "just pressed?"
- `GameInput` and the double-buffer swap pattern
- `prepare_input_frame()` — carrying state between frames
- Why `half_transition_count` matters for single-shot events

---

## The problem: detecting "just pressed"

If you only store "is the key held?", you can't tell the difference between:
- First frame of a key press (should fire one bullet)
- Continued hold (should not fire again)

You need to compare this frame's state against last frame's state.

---

## GameButtonState

```c
// game/game.h
typedef struct {
    int ended_down;            // 1 = key held at end of this frame
    int half_transition_count; // number of press/release events this frame
} GameButtonState;
```

Decoding:

| `ended_down` | `half_transition_count` | Meaning |
|---|---|---|
| 1 | 0 | Held (no new events) |
| 1 | > 0 | Just pressed this frame |
| 0 | > 0 | Just released this frame |
| 0 | 0 | Not held, no event |

"Just pressed" = `ended_down && half_transition_count > 0`

---

## GameInput

```c
typedef struct {
    union {
        GameButtonState buttons[BUTTON_COUNT];  // indexed access
        struct {
            GameButtonState left;   // rotate left
            GameButtonState right;  // rotate right
            GameButtonState up;     // thrust
            GameButtonState fire;   // shoot
        };
    };
    int quit;
} GameInput;
```

The anonymous union allows both named access (`input.left`) and indexed
access (`input.buttons[i]`) to the same memory.

---

## prepare_input_frame()

```c
// game/game.h (implemented in game.c)
void prepare_input_frame(const GameInput *old, GameInput *current);
```

Called at the start of each frame, before reading hardware:

```c
// Implementation:
void prepare_input_frame(const GameInput *old, GameInput *current) {
    for (int i = 0; i < BUTTON_COUNT; i++) {
        current->buttons[i].ended_down          = old->buttons[i].ended_down;
        current->buttons[i].half_transition_count = 0;
    }
    current->quit = old->quit;
}
```

It carries `ended_down` from last frame (keys that were held remain held)
and resets `half_transition_count` to 0 (no events yet this frame).

---

## Double-buffer swap

```c
// The platform maintains two GameInput structs:
GameInput inputs[2];
memset(inputs, 0, sizeof(inputs));

// Each frame:
platform_swap_input_buffers(&inputs[0], &inputs[1]); // swap
prepare_input_frame(&inputs[0], &inputs[1]);         // carry + reset
// ... read hardware events into inputs[1] ...
asteroids_update(&state, &inputs[1], dt);
```

After the swap, `inputs[0]` is "last frame" and `inputs[1]` is "current frame".
`prepare_input_frame` uses `inputs[0]` to initialise `inputs[1]`.

```c
// platform.h
static inline void platform_swap_input_buffers(GameInput *a, GameInput *b) {
    GameInput temp = *a;
    *a = *b;
    *b = temp;
}
```

---

## Raylib key mapping

```c
// platforms/raylib/main.c
#define MAP_KEY(BTN, KEY1, KEY2)                               \
    do {                                                        \
        int held = IsKeyDown(KEY1) || IsKeyDown(KEY2);         \
        if (held != (BTN).ended_down) {                        \
            (BTN).ended_down = held;                           \
            (BTN).half_transition_count++;                     \
        }                                                       \
    } while(0)

MAP_KEY(current->left,  KEY_LEFT,  KEY_A);
MAP_KEY(current->right, KEY_RIGHT, KEY_D);
MAP_KEY(current->up,    KEY_UP,    KEY_W);
MAP_KEY(current->fire,  KEY_SPACE, KEY_ENTER);
```

The macro detects state changes (held ≠ `ended_down`) and bumps
`half_transition_count` only when the state actually changes.

---

## Using input in game.c

```c
// Thrust: held → continuous acceleration
if (input->up.ended_down) {
    state->player.dx += sa * SHIP_THRUST_ACCEL * dt;
    state->player.dy += ca * SHIP_THRUST_ACCEL * dt;
}

// Fire: just pressed → single bullet
if (input->fire.ended_down && input->fire.half_transition_count > 0
    && state->fire_timer <= 0.0f) {
    add_bullet(state);
    state->fire_timer = FIRE_COOLDOWN;
}

// Restart: just pressed while dead → reinit
if (state->phase == PHASE_DEAD
    && input->fire.ended_down && input->fire.half_transition_count > 0
    && state->dead_timer <= 0.0f) {
    asteroids_init(state);
}
```

---

## Key takeaways

1. `GameButtonState` captures both held state and transition events.
2. "Just pressed" = `ended_down && half_transition_count > 0`.
3. `prepare_input_frame` carries `ended_down` and clears `half_transition_count`.
4. Two input buffers prevent aliasing: old (last frame) vs current (this frame).
5. `platform_swap_input_buffers` just swaps two structs by value.
