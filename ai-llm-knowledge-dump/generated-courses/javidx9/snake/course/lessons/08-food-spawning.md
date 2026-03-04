# Lesson 08 — Food Spawning

## What We're Building

Implement `snake_spawn_food` — a retry loop that places food on a random empty cell — and the food-eaten branch that increments the score, queues growth, scales speed, and respawns food.

---

## The Concept

### JS Analogy: `Array.prototype.find` on the Segments Array

In JavaScript, finding whether a position is occupied looks like:

```js
function isCellOccupied(segments, x, y) {
  return segments.some(seg => seg.x === x && seg.y === y);
}
```

`snake_spawn_food` does exactly the same thing — it just does it in a `do/while` retry loop, picks a random candidate first, then scans the ring buffer to confirm the cell is free.

```js
// JS mental model
function spawnFood(state) {
  let x, y;
  do {
    x = 1 + Math.floor(Math.random() * (GRID_WIDTH  - 2));
    y = 1 + Math.floor(Math.random() * (GRID_HEIGHT - 2));
  } while (state.segments.some(s => s.x === x && s.y === y));
  state.food_x = x;
  state.food_y = y;
}
```

The C version does the exact same work — the ring buffer walk replaces `.some()`.

---

### The Retry Loop and When It Terminates

The loop picks a candidate and then scans `length` segments. If any segment occupies that cell, `ok = 0` and we try again.

**Worst case:** the snake fills almost the entire grid (1198 of 1200 cells). Only 2 cells are free. Each random pick has a 2/1200 ≈ 0.17% chance of landing on a free cell — the loop runs about 600 iterations on average. At CPU speed this is still microseconds.

**Expected case:** at game start (10 segments), each pick has a 1186/1200 ≈ 99% chance of being free — the loop exits in one or two tries.

This O(N) approach is perfectly adequate for a grid this size. It avoids maintaining a separate "free cells" data structure at the cost of some random retries.

---

### `grow_pending` Defers Tail Advance

Every move step the snake adds a new head segment and normally removes the tail — net change: zero. `grow_pending` defers that tail removal:

```
Ate food → grow_pending += 5

Each tick while grow_pending > 0:
  add head   (length++)
  DON'T advance tail  (grow_pending--)

Once grow_pending == 0:
  add head   (length++)
  advance tail  (length--)    ← back to normal
```

The snake grows by one segment per tick for 5 ticks after eating. The tail stays put each time.

---

## The Code

### `snake_spawn_food` — File: `src/game.c`

```c
void snake_spawn_food(GameState *s) {
    int ok, idx, rem;
    do {
        /* Pick a random cell inside the wall border.
         * +1 and -2 keep food at least one cell from the wall.
         * JS: 1 + Math.floor(Math.random() * (GRID_WIDTH - 2)) */
        s->food_x = 1 + rand() % (GRID_WIDTH  - 2);
        s->food_y = 1 + rand() % (GRID_HEIGHT - 2);

        /* Walk ring buffer from tail to head, checking for collision.
         * JS: segments.some(seg => seg.x === food_x && seg.y === food_y) */
        ok  = 1;
        idx = s->tail;
        rem = s->length;
        while (rem > 0) {
            if (s->segments[idx].x == s->food_x &&
                s->segments[idx].y == s->food_y) {
                ok = 0;  /* occupied — retry */
                break;
            }
            idx = (idx + 1) % MAX_SNAKE;
            rem--;
        }
    } while (!ok);
}
```

**Ring buffer walk pattern** — used identically for food spawning, self-collision, and rendering:

```c
idx = s->tail;   // start at the oldest segment
rem = s->length; // visit exactly length segments
while (rem > 0) {
    // use s->segments[idx] here
    idx = (idx + 1) % MAX_SNAKE;  // advance, wrapping at MAX_SNAKE=1200
    rem--;
}
```

---

### Food-Eaten Branch — File: `src/game.c`, inside `snake_update`

```c
/* ── Food eaten ───────────────────────────────────────────────────────────
 * Check AFTER advancing head — the new head position is `new_x/new_y`. */
if (new_x == s->food_x && new_y == s->food_y) {
    s->score++;
    s->grow_pending += 5;  /* defer tail advance for 5 ticks */

    /* Speed up every 3 points, but don't go below floor */
    if (s->score % 3 == 0 && s->move_interval > 0.05f)
        s->move_interval -= 0.01f;

    /* Spatial audio pan: map food_x in [1, GRID_WIDTH-2] to [-1, +1].
     * food_x = 1           → pan ≈ -1.0  (full left)
     * food_x = GRID_WIDTH/2 → pan ≈  0.0  (centre)
     * food_x = GRID_WIDTH-2 → pan ≈ +1.0  (full right) */
    {
        float pan = ((float)s->food_x / (float)(GRID_WIDTH - 1)) * 2.0f - 1.0f;
        game_play_sound_at(&s->audio, SOUND_FOOD_EATEN, pan);
    }

    snake_spawn_food(s);
}
```

---

### Tail Advance Logic — File: `src/game.c`, inside `snake_update`

```c
/* ── Tail advance (unless growing) ───────────────────────────────
 * grow_pending > 0: skip tail advance this tick (snake grows by 1).
 * grow_pending == 0: advance tail and decrease length (constant length). */
if (s->grow_pending > 0) {
    s->grow_pending--;
    /* length stays incremented from the head write above — net +1 */
} else {
    s->tail = (s->tail + 1) % MAX_SNAKE;
    s->length--;
    /* net change: +1 (head) -1 (tail) = 0 — constant length */
}
```

---

## What To Notice

- **`grow_pending` is a counter, not a flag.** It counts *how many more ticks* the tail should stay put. Eating two food items back-to-back adds 10 to it — the snake grows for 10 ticks regardless of when the second food was eaten.
- **The food-eaten check is after head advance.** The new head position has already been written to `segments[head]` and `length` incremented. The food and collision checks both operate on `new_x / new_y`, not the array — no off-by-one confusion.
- **Speed scaling has a floor.** `move_interval > 0.05f` ensures the snake never moves faster than 20 steps per second (50 ms interval). Without the floor the game would become unplayable at high scores.
- **`srand` is called once in `snake_init`.** Calling it every frame would reset the PRNG sequence — food positions would repeat. `time(NULL)` gives a different seed each run.

---

## Exercises

1. **Visualise retry count.** Add a `debug_spawn_retries` field to `GameState` and increment it every time `snake_spawn_food` loops. Render it in the header next to the score. At what snake length do you first see retries > 1?

2. **Change growth amount.** Replace the constant `+= 5` with a formula: `grow_pending += 2 + s->score / 5`. Describe what effect this has on late-game play.

3. **Prevent infinite loop (extra credit).** If the grid is completely full (snake length == (GRID_WIDTH-2) × (GRID_HEIGHT-2)), `snake_spawn_food` would loop forever. Add a guard that detects this and transitions to a special `GAME_PHASE_WIN` phase instead.
