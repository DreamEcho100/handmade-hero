# Lesson 04 — Game Structure and Phase Dispatch

**Source files:** `src/game.h`, `src/game.c`
**Functions:** `snake_update()`, `snake_init()`

---

## What We're Building

A single `GameState` struct that holds every piece of mutable state, a
`GAME_PHASE` enum that replaces the reference's `int game_over` flag, and a
`switch`-dispatch inside `snake_update` that routes each frame to the right
per-phase logic.

---

## The Concept

### JS analogy — plain object + `Object.freeze` enum

In JavaScript a typical game might look like:

```js
// The "enum" — a frozen object prevents mutation
const GAME_PHASE = Object.freeze({ PLAYING: 0, GAME_OVER: 1 });

// All state in one plain object
function createGameState() {
    return {
        phase:        GAME_PHASE.PLAYING,
        score:        0,
        bestScore:    0,
        direction:    SNAKE_DIR.RIGHT,
        nextDirection: SNAKE_DIR.RIGHT,
        moveTimer:    0,
        moveInterval: 0.15,
        // … snake body data …
    };
}

// Per-frame dispatch
function update(state, input, dt) {
    switch (state.phase) {
        case GAME_PHASE.PLAYING:   updatePlaying(state, input, dt); break;
        case GAME_PHASE.GAME_OVER: updateGameOver(state, input);    break;
    }
}
```

The C version is structurally identical.  The two differences are:

1. The struct lives on the **stack** in `main()` and is passed by **pointer**.
   There is no heap allocation for game state.
2. The `typedef enum` is a language-level type, not a runtime object.  The
   compiler catches incorrect usage (e.g. comparing a `GAME_PHASE` with a
   `SNAKE_DIR`) at compile time.

---

## The Code

### `GAME_PHASE` — upgrading from `int game_over` (`src/game.h`)

```c
typedef enum {
    GAME_PHASE_PLAYING   = 0,  /* Normal play — snake moves, input accepted    */
    GAME_PHASE_GAME_OVER = 1   /* Snake hit wall or itself — overlay shown     */
} GAME_PHASE;
```

**Course note — why this is better than the reference's `int game_over`:**

| Reference source | This course |
|---|---|
| `int game_over;` (0 or 1) | `GAME_PHASE phase;` (named enum) |
| `if (game_over) { … }` | `switch(s->phase) { case GAME_PHASE_PLAYING: … }` |
| Adding a PAUSE state requires a second `int pause` | Adding PAUSE = 2 to the enum, one new `case` |
| Compiler won't warn if you compare with a colour constant | Compiler warns on type mismatch |
| Self-documenting? Only if you remember `1 = game over` | Always reads as `GAME_PHASE_GAME_OVER` |

JS analogy: using a string union type `'playing' | 'game_over'` in TypeScript
instead of a boolean `gameOver: boolean`.  More phases fit naturally; the
type system catches typos.

### `GameState` — all state in one struct (`src/game.h`)

```c
typedef struct {
    /* Snake body — ring buffer (see Lesson 05) */
    Segment segments[MAX_SNAKE];
    int head, tail, length;

    /* Direction */
    SNAKE_DIR direction;       /* Applied each move tick                       */
    SNAKE_DIR next_direction;  /* Queued by input; applied at move time        */

    /* Movement timing */
    float move_timer;     /* Accumulates dt each frame                        */
    float move_interval;  /* Move when timer >= interval (seconds/step)       */

    /* Food & score */
    int food_x, food_y;
    int grow_pending;     /* Segments still to be added (5 per food eaten)    */
    int score;
    int best_score;       /* Preserved across snake_init()                    */

    /* Phase (replaces int game_over) */
    GAME_PHASE phase;

    /* Audio */
    GameAudioState audio;
} GameState;
```

**Handmade Hero principle: "State is always visible."**
Every mutable value lives in this one struct.  The platform passes a pointer
each frame.  At any moment you can attach a debugger and inspect all game
state without hunting through globals or closures.

JS equivalent: `createGameState()` returns a plain object.  The difference is
that `GameState` lives on the C stack — it is declared as a local variable in
`main()`, not heap-allocated with `malloc`.

### Why `next_direction` exists

```c
SNAKE_DIR direction;       /* Current direction — applied each move tick      */
SNAKE_DIR next_direction;  /* Queued by input — applied at the NEXT move      */
```

Without the queue, a player could press two keys in the same frame (e.g., UP
while moving RIGHT, then RIGHT again before the move tick fires).  If both
inputs were applied immediately, the second press could appear to cancel the
first.  More dangerously: if the player presses the *opposite* direction
(a 180° U-turn) in the same tick the move fires, the new head would land on
the second body segment — instant self-collision.

By staging direction changes in `next_direction` and committing only at move
time, the U-turn guard always runs at the right moment:

```c
/* In snake_update, input handling: */
if (input->turn_right.ended_down &&
    input->turn_right.half_transition_count > 0) {
    SNAKE_DIR turned = (SNAKE_DIR)((s->direction + 1) % 4);  /* CW */
    /* (s->direction + 2) % 4 = opposite direction = U-turn = blocked */
    if (turned != (SNAKE_DIR)((s->direction + 2) % 4))
        s->next_direction = turned;
}
/* … same for turn_left (CCW = +3 mod 4) … */

/* Later, at move time: */
s->direction = s->next_direction;   /* commit queued direction */
```

JS equivalent:
```js
// Input time — queue but don't apply:
if (justPressed('ArrowRight')) {
    const turned = (state.direction + 1) % 4;
    if (turned !== (state.direction + 2) % 4)  // not a U-turn
        state.nextDirection = turned;
}

// Move time — apply:
state.direction = state.nextDirection;
```

### `switch(s->phase)` dispatch in `snake_update` (`src/game.c`)

```c
void snake_update(GameState *s, const GameInput *input, float delta_time) {
    switch (s->phase) {

    case GAME_PHASE_PLAYING: {
        /* Process turn input */
        /* Accumulate move timer */
        /* If timer >= interval: move the snake, check collisions */
        break;
    }

    case GAME_PHASE_GAME_OVER: {
        if (input->restart)
            snake_init(s);  /* resets phase to GAME_PHASE_PLAYING */
        break;
    }

    } /* end switch */
}
```

The `switch` is a state machine.  Each `case` handles exactly one phase.
Adding a new phase (e.g., `GAME_PHASE_PAUSED`) means:
1. Add a constant to the enum.
2. Add a `case` to the switch.
3. Add a condition that transitions to it.

Nothing else needs to change.  Compare this to the reference's approach:

```c
/* Reference source: flat function with nested ifs */
void snake_update(…) {
    if (!game_over) {
        // handle input…
        // move…
        if (wall_hit || self_hit) game_over = 1;
    } else {
        // restart…
    }
}
```

With more phases (pause, menu, level transitions) the nested-if version
becomes hard to follow.  The switch-dispatch scales without adding nesting.

### `snake_init` — resetting state while preserving the best score (`src/game.c`)

```c
void snake_init(GameState *s) {
    int saved_best = s->best_score;          /* save before zeroing */
    int saved_sps  = s->audio.samples_per_second;
    int i;

    /* Zero the ENTIRE struct in one call.
     * JS: s = { ...defaultState }          */
    memset(s, 0, sizeof(*s));

    /* Restore what we saved */
    s->best_score               = saved_best;
    s->audio.samples_per_second = saved_sps;

    /* Place initial 10-segment snake, columns 10–19, centred vertically */
    s->head   = 9;
    s->tail   = 0;
    s->length = 10;
    for (i = 0; i < 10; i++) {
        s->segments[i].x = 10 + i;
        s->segments[i].y = GRID_HEIGHT / 2;  /* row 10 */
    }

    s->direction      = SNAKE_DIR_RIGHT;
    s->next_direction = SNAKE_DIR_RIGHT;
    s->move_timer     = 0.0f;
    s->move_interval  = 0.15f;   /* 150 ms/step starting speed */
    s->phase          = GAME_PHASE_PLAYING;

    /* Seed random number generator.
     * JS: Math.random() is pre-seeded; in C you must seed manually.   */
    srand((unsigned)time(NULL));
    snake_spawn_food(s);
}
```

`memset(s, 0, sizeof(*s))` zeroes every byte in the struct.  For integers and
pointers this is equivalent to setting everything to 0 / NULL.  For floats on
any IEEE-754 platform, all-zero bytes produces 0.0f.  We then explicitly
assign every field we care about, so the zeroing just provides a clean slate —
no field is accidentally left with a stale value from the previous game.

### Delta-time accumulator — why subtract instead of zero

```c
s->move_timer += delta_time;
if (s->move_timer < s->move_interval) break;  /* not yet time to move */

/* KEY: subtract, don't zero */
s->move_timer -= s->move_interval;
```

If we wrote `s->move_timer = 0.0f` instead:

- Suppose `move_interval = 0.15f` and the last frame took `0.17f` seconds.
- Overshoot = `0.17 - 0.15 = 0.02f`.
- Zeroing discards the 0.02 — it's gone.
- Next step fires at 0.15 seconds from *zero*, not 0.15 from when it should
  have fired.
- Result: occasional skipped steps, perceived as irregular movement speed.

Subtracting keeps the overshoot.  Next frame the timer already starts at 0.02.
The next step fires after `0.15 - 0.02 = 0.13f` more seconds — the correct
compensated interval.

JS analogy: the same pattern in `setInterval` vs delta-time loops:
```js
// BAD: ignores the overshoot
if (moveTimer >= moveInterval) { moveTimer = 0; moveSnake(); }

// GOOD: carries the overshoot forward
if (moveTimer >= moveInterval) { moveTimer -= moveInterval; moveSnake(); }
```

---

## What To Notice

- **`GAME_PHASE` scales; `int game_over` does not** — adding PAUSED requires
  one enum constant and one switch case.  With `int game_over`, adding PAUSED
  forces a second `int paused` and `if/else` chains.

- **All state in one struct, on the stack** — `GameState state;` in `main()`
  means no heap allocation for game state, no hidden globals, and trivially
  inspectable by a debugger.

- **`next_direction` is the U-turn guard** — direction is committed at move
  time, giving the input system exactly one full tick to validate the turn
  before it becomes permanent.

- **`memset` + selective restore** — a clean `memset` is simpler than
  individually zeroing 15+ fields.  The two-line save/restore pattern is a
  common C idiom for "reset everything except a few preserved values".

---

## Exercises

1. **Add a `GAME_PHASE_PAUSED` state.**
   Add `GAME_PHASE_PAUSED = 2` to the `GAME_PHASE` enum.  Add `case XK_p`
   to `platform_get_input` setting `input->pause = 1` (you'll need to add
   `int pause` to `GameInput` and reset it in `prepare_input_frame`).
   In `snake_update`, add:
   - In the `GAME_PHASE_PLAYING` case: if `input->pause`, set `s->phase =
     GAME_PHASE_PAUSED; break;` before the timer accumulation.
   - A new `case GAME_PHASE_PAUSED:` that returns to `GAME_PHASE_PLAYING` when
     `input->pause` is pressed again.
   The game should freeze on `P` and resume on a second `P`.

2. **Test the U-turn guard.**
   While the snake is moving right, press Left as quickly as possible.
   Confirm the snake does NOT reverse — it continues right until the next
   valid turn.  Then add a `printf` in the guard condition to see when it fires:
   ```c
   if (turned == (SNAKE_DIR)((s->direction + 2) % 4)) {
       printf("U-turn blocked!\n");
   }
   ```

3. **Inspect `GameState` in the debugger.**
   Build with `./build-dev.sh --backend=x11 --debug` and run under `gdb`.
   Set a breakpoint on `snake_update`.  When it hits, type
   `print *s` to dump the entire `GameState` struct.  Observe how `move_timer`
   grows each frame until it exceeds `move_interval`.
