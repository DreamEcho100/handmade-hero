# Lesson 07 — Turn Input

## What We're Building

Queue a direction change on key press and apply it one tick later, with a U-turn guard that prevents the snake from reversing into itself.

---

## The Concept

### JS Analogy: A Queued Action That Fires Next Tick

In JavaScript you might write:

```js
document.addEventListener('keydown', e => {
  if (e.key === 'ArrowRight') state.direction = turnRight(state.direction);
});
```

That mutates `direction` immediately. If the game logic runs in the same tick, the snake moves in the new direction before the collision check has seen the old one. That creates a window where a fast double-tap can make the snake reverse — instant death.

The fix is to **stage** the change in a second field:

```js
// input handler — queues, does not apply
if (e.key === 'ArrowRight') state.nextDirection = turnRight(state.direction);

// game tick — applies the queued direction before moving
state.direction = state.nextDirection;
const newX = head.x + DX[state.direction];
```

`direction` = what the snake is **currently doing**.  
`next_direction` = what the player **asked for**. Applied at the start of each move step.

---

### Turn Arithmetic (CW / CCW)

The `SNAKE_DIR` enum values are laid out so modular arithmetic does all the work:

```
UP=0  RIGHT=1  DOWN=2  LEFT=3
```

| Operation | Formula | Example (currently RIGHT=1) |
|-----------|---------|----------------------------|
| Turn CW   | `(dir + 1) % 4` | `(1+1)%4 = 2` → DOWN |
| Turn CCW  | `(dir + 3) % 4` | `(1+3)%4 = 0` → UP   |
| Opposite  | `(dir + 2) % 4` | `(1+2)%4 = 3` → LEFT |

`(dir + 3) % 4` is equivalent to `(dir - 1 + 4) % 4` — the `+4` keeps the value positive before the modulo so we never get `-1 % 4` (which is implementation-defined in C89).

### The U-Turn Guard

If the player is going RIGHT and taps LEFT, the opposite direction is LEFT `(1+2)%4 = 3`. Allowing that move would place the new head exactly where `segments[head-1]` sits — instant self-collision on the very next tick.

The guard compares the **candidate** direction against the current opposite:

```
candidate = turnRight(direction)         // or turnLeft
if (candidate != opposite(direction))    // only then store
    next_direction = candidate
```

Because `next_direction` defaults to `direction`, a blocked input is silently ignored — the snake keeps going straight.

---

## The Code

**File:** `src/game.c` — inside `snake_update`, `GAME_PHASE_PLAYING` case.

```c
/* ── Process direction input ──────────────────────────────────────
 * "Just pressed this frame" = ended_down=1 AND half_transition_count>0.
 * Using just ended_down would fire every frame while held (not desirable).
 * Using half_transition_count>0 ensures each turn fires exactly once.
 *
 * JS: button.isDown && button.justPressed  (manual event tracking) */
if (input->turn_right.ended_down &&
    input->turn_right.half_transition_count > 0) {
    turned = (SNAKE_DIR)((s->direction + 1) % 4);  /* CW */
    /* U-turn guard: block 180° reversal.
     * Opposite direction = (dir + 2) % 4.
     * Applying the opposite would move the head into the second segment. */
    if (turned != (SNAKE_DIR)((s->direction + 2) % 4))
        s->next_direction = turned;
}
if (input->turn_left.ended_down &&
    input->turn_left.half_transition_count > 0) {
    turned = (SNAKE_DIR)((s->direction + 3) % 4);  /* CCW */
    if (turned != (SNAKE_DIR)((s->direction + 2) % 4))
        s->next_direction = turned;
}
```

Then, at the start of the move step:

```c
/* Commit the queued direction */
s->direction = s->next_direction;

/* Compute the new head position */
new_x = s->segments[s->head].x + DX[s->direction];
new_y = s->segments[s->head].y + DY[s->direction];
```

`next_direction` is initialised to `direction` in `snake_init()`:

```c
s->direction      = SNAKE_DIR_RIGHT;
s->next_direction = SNAKE_DIR_RIGHT;
```

### Why `half_transition_count > 0` and Not Just `ended_down`?

`ended_down` is 1 for every frame the key is physically held. If you tested only `ended_down`, one real keypress that spans three frames would generate **three** turn events — the snake would spiral.

`half_transition_count > 0` is only true in the frame where the key actually changed state (down → up or up → down). For a normal tap, that is exactly one frame. This is the Handmade Hero sub-frame input model from `game.h`.

---

## What To Notice

- **Two fields, one purpose.** `direction` is the snake's current velocity. `next_direction` is the player's *intent*. They converge at move time.
- **The guard is a no-op for valid turns.** Turning from RIGHT to DOWN is fine: `(1+2)%4 = 3 (LEFT)` ≠ `2 (DOWN)`. The check passes.
- **`(dir+3)%4` not `(dir-1)%4`.** C's `%` on a negative number is implementation-defined. `+3` gives the same result on a 4-item ring and is always non-negative.
- **`next_direction` is reset in `snake_init`.** If it weren't, a game-over → restart would inherit the direction from the last game.

---

## Exercises

1. **Verify the arithmetic.** Print `(dir + 1) % 4` for each value 0–3 and confirm the sequence is UP→RIGHT→DOWN→LEFT→UP.

2. **Add a "hold to spin" debug mode.** While a key is *held* (`ended_down == 1`, no transition check), accumulate `move_timer` twice as fast. What happens? Can you use this to expose the U-turn guard working?

3. **Support a four-direction keypad.** The current input has only `turn_left` / `turn_right`. Add `turn_up` and `turn_down` buttons to `GameInput` and handle them in `snake_update`. Make the guard still block 180° reversals in all four cases.
