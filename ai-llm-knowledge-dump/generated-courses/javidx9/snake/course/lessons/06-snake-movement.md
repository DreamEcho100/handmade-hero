# Lesson 06 — Snake Movement

**Source files:** `src/game.c`
**Functions:** `snake_update()` — `GAME_PHASE_PLAYING` case

---

## What We're Building

The complete per-tick move sequence: accumulate delta-time, commit the queued
direction, compute the new head position, check wall and self-collisions, grow
or advance the tail, and scale speed as the score climbs.

---

## The Concept

### JS analogy — `setInterval` vs delta-time accumulation

A naïve JavaScript snake might use:

```js
setInterval(() => {
    moveSnake(state);
}, 150);  // move every 150 ms
```

This has a fundamental problem: `setInterval` fires independently of the render
loop.  If the tab is hidden for half a second and then becomes visible again,
the interval fires multiple times in rapid succession — the snake teleports.
On a slow machine the timer fires *after* the frame budget, causing missed moves.

The delta-time accumulator pattern solves both problems:

```js
function update(state, dt) {
    state.moveTimer += dt;
    if (state.moveTimer < state.moveInterval) return;  // not yet time

    state.moveTimer -= state.moveInterval;  // carry overshoot forward
    moveSnake(state);
}
```

This is **exactly** what the C code does.  The game moves at `moveInterval`
seconds per step regardless of frame rate.  If one frame takes 200 ms
(a stutter), the next frame's timer starts 50 ms ahead — not at zero — so
speed is correct on average.

Why subtract instead of zeroing:

```
Frame takes 170 ms, moveInterval = 150 ms
  moveTimer after +=  dt:  170 ms
  170 >= 150 → move fires
  moveTimer after -= 150:   20 ms   ← 20ms head-start on next step
  Next step fires after 130ms more instead of 150ms — compensated.
```

If we zeroed `moveTimer` instead, the 20ms overshoot would be discarded and
the snake would always lag a little behind where it should be.

---

## The Code

### The full move sequence (`src/game.c → snake_update → GAME_PHASE_PLAYING`)

```c
case GAME_PHASE_PLAYING: {

    /* ── 1. Process direction input ────────────────────────────────── */
    if (input->turn_right.ended_down &&
        input->turn_right.half_transition_count > 0) {
        SNAKE_DIR turned = (SNAKE_DIR)((s->direction + 1) % 4);  /* CW  */
        if (turned != (SNAKE_DIR)((s->direction + 2) % 4))       /* no U-turn */
            s->next_direction = turned;
    }
    if (input->turn_left.ended_down &&
        input->turn_left.half_transition_count > 0) {
        SNAKE_DIR turned = (SNAKE_DIR)((s->direction + 3) % 4);  /* CCW */
        if (turned != (SNAKE_DIR)((s->direction + 2) % 4))
            s->next_direction = turned;
    }

    /* ── 2. Delta-time accumulator ──────────────────────────────────── */
    s->move_timer += delta_time;
    if (s->move_timer < s->move_interval) break;  /* frame too short — wait */
    s->move_timer -= s->move_interval;             /* keep overshoot         */

    /* ── 3. Commit direction ────────────────────────────────────────── */
    s->direction = s->next_direction;

    /* ── 4. Compute new head position ──────────────────────────────── */
    int new_x = s->segments[s->head].x + DX[s->direction];
    int new_y = s->segments[s->head].y + DY[s->direction];

    /* ── 5. Wall collision ──────────────────────────────────────────── */
    if (new_x < 1 || new_x >= GRID_WIDTH - 1 ||
        new_y < 0 || new_y >= GRID_HEIGHT - 1) {
        if (s->score > s->best_score) s->best_score = s->score;
        s->phase = GAME_PHASE_GAME_OVER;
        game_play_sound(&s->audio, SOUND_GAME_OVER);
        break;
    }

    /* ── 6. Self-collision ──────────────────────────────────────────── */
    {
        int idx = s->tail;
        int rem = s->length;
        while (rem > 0) {
            if (s->segments[idx].x == new_x &&
                s->segments[idx].y == new_y) {
                if (s->score > s->best_score) s->best_score = s->score;
                s->phase = GAME_PHASE_GAME_OVER;
                game_play_sound(&s->audio, SOUND_GAME_OVER);
                return;
            }
            idx = (idx + 1) % MAX_SNAKE;
            rem--;
        }
    }

    /* ── 7. Advance head ────────────────────────────────────────────── */
    s->head = (s->head + 1) % MAX_SNAKE;
    s->segments[s->head].x = new_x;
    s->segments[s->head].y = new_y;
    s->length++;

    /* ── 8. Food check ──────────────────────────────────────────────── */
    if (new_x == s->food_x && new_y == s->food_y) {
        s->score++;
        s->grow_pending += 5;

        /* Speed scaling — every 3 points, reduce move_interval */
        if (s->score % 3 == 0 && s->move_interval > 0.05f)
            s->move_interval -= 0.01f;

        /* Spatial audio pan */
        float pan = ((float)s->food_x / (float)(GRID_WIDTH - 1)) * 2.0f - 1.0f;
        game_play_sound_at(&s->audio, SOUND_FOOD_EATEN, pan);

        snake_spawn_food(s);
    }

    /* ── 9. Tail advance (unless growing) ───────────────────────────── */
    if (s->grow_pending > 0) {
        s->grow_pending--;
    } else {
        s->tail = (s->tail + 1) % MAX_SNAKE;
        s->length--;
    }
    break;
}
```

Let's walk through each step in detail.

---

### Step 1 — Direction input and the U-turn guard

```c
SNAKE_DIR turned = (SNAKE_DIR)((s->direction + 1) % 4);  /* clockwise    */
if (turned != (SNAKE_DIR)((s->direction + 2) % 4))       /* not opposite */
    s->next_direction = turned;
```

The `SNAKE_DIR` enum has four values: UP=0, RIGHT=1, DOWN=2, LEFT=3.
Turn arithmetic on a 4-value cycle:

| Current | `+1 % 4` (right/CW) | `+3 % 4` (left/CCW) | `+2 % 4` (opposite/blocked) |
|---|---|---|---|
| UP (0) | RIGHT (1) | LEFT (3) | DOWN (2) |
| RIGHT (1) | DOWN (2) | UP (0) | LEFT (3) |
| DOWN (2) | LEFT (3) | RIGHT (1) | UP (0) |
| LEFT (3) | UP (0) | DOWN (2) | RIGHT (1) |

The guard `turned != opposite` blocks only the 180° reversal.  90° turns
are always allowed.

**Why stage in `next_direction` instead of applying to `direction` immediately?**
If direction were updated at input time, and the player pressed two keys in the
same frame (fast typing), the second key might reverse the first — including a
U-turn that the guard should have caught.  Staging to `next_direction` means
the guard runs once at move time against the confirmed current direction, not
against a potentially already-changed one.

---

### Step 2 — Delta-time accumulator

```c
s->move_timer += delta_time;
if (s->move_timer < s->move_interval) break;
s->move_timer -= s->move_interval;
```

- `delta_time` is capped at 0.05 s (50 ms) in `main()` — so a paused window
  resuming cannot cause a huge burst of moves.
- The `break` exits the `GAME_PHASE_PLAYING` case, returning control to the
  frame loop without moving the snake.
- The subtraction (not zeroing) preserves overshoot for accurate long-term
  pacing.

---

### Step 3 — Direction commit

```c
s->direction = s->next_direction;
```

This is the one moment per tick where `next_direction` becomes `direction`.
Input handling earlier in this function only writes `next_direction`.  This
ordering guarantees: "whatever the player pressed up until this tick is
applied to this move."

---

### Step 4 — `DX`/`DY` dispatch

```c
/* Defined at the top of game.c: */
static const int DX[4] = { 0,  1,  0, -1 };  /* UP, RIGHT, DOWN, LEFT */
static const int DY[4] = {-1,  0,  1,  0 };  /* UP, RIGHT, DOWN, LEFT */

int new_x = s->segments[s->head].x + DX[s->direction];
int new_y = s->segments[s->head].y + DY[s->direction];
```

Using a lookup table avoids a four-branch `switch` or `if/else` chain.
`DX[SNAKE_DIR_UP] = 0, DY[SNAKE_DIR_UP] = -1` — moving up means x stays the
same, y decreases (the screen coordinate system has y=0 at the top).

JS equivalent:
```js
const DELTAS = [{dx:0,dy:-1}, {dx:1,dy:0}, {dx:0,dy:1}, {dx:-1,dy:0}];
const newX = head.x + DELTAS[state.direction].dx;
const newY = head.y + DELTAS[state.direction].dy;
```

---

### Step 5 — Wall collision

```c
if (new_x < 1 || new_x >= GRID_WIDTH - 1 ||
    new_y < 0 || new_y >= GRID_HEIGHT - 1) {
```

- Columns 0 and `GRID_WIDTH-1` (= 59) are wall cells — the snake cannot enter
  them.  `new_x < 1` catches hitting the left wall; `new_x >= 59` catches the
  right.
- Rows 0 and `GRID_HEIGHT-1` (= 19) are wall cells.  Note `new_y < 0` (not
  `< 1`) — the top row is the wall, row 0 itself.
- On collision: save best score if improved, transition to
  `GAME_PHASE_GAME_OVER`, play sound effect, `break`.

---

### Step 6 — Self-collision

```c
int idx = s->tail;
int rem = s->length;
while (rem > 0) {
    if (s->segments[idx].x == new_x && s->segments[idx].y == new_y) {
        /* … game over … */
        return;   /* note: return, not break — exits snake_update entirely */
    }
    idx = (idx + 1) % MAX_SNAKE;
    rem--;
}
```

The scan checks every live segment including the current head.  Because the
tail will advance *after* this check (unless growing), the tail cell is a valid
"can the head enter here?" check target — if the head is about to land on the
current tail, the tail will have vacated that cell by the time it matters.

Actually, this is a subtle correctness point: self-collision is checked
*before* head is advanced.  The new head position is not yet in the buffer.
If the new head would land on any current segment (except the tail which is
about to leave), it's a collision.  Because `rem = s->length` and the scan
starts at `s->tail`, it scans all live segments including the tail-about-to-leave.
For a non-growing snake this is slightly conservative (the tail *will* leave
before the head arrives), but it's safe — missing a valid cell by one step is
better than missing a collision.

---

### Step 7 — Head advance

```c
s->head = (s->head + 1) % MAX_SNAKE;
s->segments[s->head].x = new_x;
s->segments[s->head].y = new_y;
s->length++;
```

Only reaches here if no collision was detected.  The new position is written at
the next ring buffer slot.  Length increases by 1 (matched by length-- in step 9
when the tail advances).

---

### Step 8 — Food eaten, speed scaling, and spatial audio

```c
if (new_x == s->food_x && new_y == s->food_y) {
    s->score++;
    s->grow_pending += 5;

    /* Speed scaling formula:
     * Every 3 score points (score % 3 == 0), decrease move_interval by 0.01s.
     * Floor at 0.05s (50ms/step = max speed).
     * Starting at 0.15s, max speed is reached at score 30:
     *   0.15 - (30/3) * 0.01 = 0.15 - 0.10 = 0.05                          */
    if (s->score % 3 == 0 && s->move_interval > 0.05f)
        s->move_interval -= 0.01f;

    /* Spatial audio: map food_x to stereo pan [-1, +1]
     * food_x = 0         → pan = -1.0 (full left)
     * food_x = (W-1)/2   → pan =  0.0 (centre)
     * food_x = W-1 = 59  → pan = +1.0 (full right)                          */
    float pan = ((float)s->food_x / (float)(GRID_WIDTH - 1)) * 2.0f - 1.0f;
    game_play_sound_at(&s->audio, SOUND_FOOD_EATEN, pan);

    snake_spawn_food(s);
}
```

Speed scaling makes the game progressively harder without changing any other
mechanic.  The formula:

```
move_interval = 0.15f - (score / 3) * 0.01f   (integer division — steps every 3 points)
```

At score 0:  0.15 s/step  (~6.7 steps/s)
At score 9:  0.12 s/step  (~8.3 steps/s)
At score 30: 0.05 s/step  (20 steps/s — the floor)

The floor `> 0.05f` prevents the interval going to zero (infinite speed) or
negative (backwards time).

---

### Step 9 — Tail advance

```c
if (s->grow_pending > 0) {
    s->grow_pending--;          /* Skip tail advance — snake grows by 1 */
} else {
    s->tail = (s->tail + 1) % MAX_SNAKE;
    s->length--;                /* Constant length when not growing     */
}
```

Combined with step 7 (`length++` on head advance):

- Not growing: `length++` then `length--` → net change = 0 (constant length).
- Growing: `length++` and no `length--` → net change = +1 per grow tick.

---

## What To Notice

- **Move steps are decoupled from frame rate** — the delta-time accumulator
  means speed is measured in real seconds, not frame counts.  The snake behaves
  identically at 30 FPS and 120 FPS.

- **Collision check uses `new_x/new_y`, not the ring buffer** — wall and self-
  collision are checked against the *prospective* position before it is written.
  This avoids a self-collision against the segment the snake is about to leave.

- **Speed floor of 0.05s** — without the `> 0.05f` guard, the interval could
  underflow to zero or negative, making the move timer always trigger and
  running the snake at thousands of steps per second.

- **`break` vs `return` on game-over** — wall collision uses `break` (exits the
  switch case cleanly); self-collision uses `return` (exits `snake_update`
  entirely, skipping the tail-advance step at the bottom of the case).  Both
  achieve game-over, but `return` is used after the collision scan to avoid
  advancing the tail of a dead snake.

---

## Exercises

1. **Observe speed scaling.**
   Add a `printf("interval=%.3f\n", s->move_interval);` just after the
   `s->move_interval -= 0.01f` line.  Eat three items and verify the interval
   drops from 0.15 to 0.14.  Eat nine more items (total score 12) and confirm
   it reaches 0.11.

2. **Modify the speed formula.**
   Change `s->move_interval -= 0.01f` to `s->move_interval -= 0.005f` (slower
   progression) and the condition from `score % 3 == 0` to `score % 2 == 0`
   (speed up every 2 points instead of 3).  Play the game and feel the
   difference.  Restore the originals when done.

3. **Trace one complete move tick.**
   Set a breakpoint in `snake_update` at the `s->move_timer -= s->move_interval`
   line.  In the debugger, print `new_x`, `new_y`, `s->head`, `s->tail`, and
   `s->length` before and after the head advance and tail advance steps.
   Confirm that `length` stays constant (head++ and tail++ cancel) when
   `grow_pending == 0`, and increases by 1 when `grow_pending > 0`.
