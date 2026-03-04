# Lesson 09 — Collision Detection

## What We're Building

Detect wall collisions and self-collisions after each move step, and transition to `GAME_PHASE_GAME_OVER` when either triggers.

---

## The Concept

### JS Analogy: `Array.some()` + State Machine Transition

In JavaScript, wall collision and self-collision look like:

```js
// Wall check
function hitsWall(x, y) {
  return x < 1 || x >= GRID_WIDTH - 1 ||
         y < 0 || y >= GRID_HEIGHT - 1;
}

// Self-collision check
function hitsSelf(segments, x, y) {
  return segments.some(s => s.x === x && s.y === y);
}

// State machine transition
if (hitsWall(newX, newY) || hitsSelf(segments, newX, newY)) {
  state.phase = GAME_PHASE.GAME_OVER;
}
```

The C code does the same thing — the `.some()` becomes an explicit `while` loop over the ring buffer, and the state transition changes `s->phase`.

---

### The State Machine Principle

`GAME_PHASE` is a two-state machine:

```
PLAYING ──[wall or self-collision]──► GAME_OVER
GAME_OVER ──[restart input]──────────► PLAYING
```

Each phase has its own input rules. `GAME_PHASE_GAME_OVER` blocks all input except restart — exactly like a modal dialog that blocks keyboard events until you dismiss it. (See Lesson 10 for the restart path.)

---

### Why `GAME_PHASE` Instead of `int game_over`

The reference source uses `int game_over = 0`. The course upgrades to `GAME_PHASE` because:

1. **Self-documenting.** `phase == GAME_PHASE_GAME_OVER` vs `game_over == 1` — the enum name explains itself.
2. **Extensible.** Adding `GAME_PHASE_PAUSED` or `GAME_PHASE_MENU` requires no new boolean fields.
3. **Compiler warning on exhausted switch.** If a new phase is added and the `switch` in `snake_update` doesn't handle it, `-Wswitch` will warn.

---

## The Code

**File:** `src/game.c` — inside `snake_update`, `GAME_PHASE_PLAYING` case, after computing `new_x / new_y`.

### Wall Collision Check

```c
/* ── Wall collision ───────────────────────────────────────────────────────
 * Column 0 and GRID_WIDTH-1 are wall cells; same for row 0 and GRID_HEIGHT-1.
 * The snake is not allowed to enter those cells.
 *
 * new_y >= 0 is always true (it starts at 0) so the top wall check is:
 *   new_y < 0            (can't go above the grid)
 *   new_y >= GRID_HEIGHT-1 (can't enter the bottom wall row)
 *
 * JS: if (newX < 1 || newX >= GRID_WIDTH-1 || newY < 0 || newY >= GRID_HEIGHT-1) */
if (new_x < 1 || new_x >= GRID_WIDTH - 1 ||
    new_y < 0 || new_y >= GRID_HEIGHT - 1) {
    if (s->score > s->best_score) s->best_score = s->score;
    s->phase = GAME_PHASE_GAME_OVER;
    game_play_sound(&s->audio, SOUND_GAME_OVER);
    break;  /* exit the GAME_PHASE_PLAYING case */
}
```

**Why `new_x < 1` and not `< 0`?**  
Column 0 is the left wall cell. Entering it means the snake hit the wall. The playable columns are `[1, GRID_WIDTH-2]`.

---

### Self-Collision Check

```c
/* ── Self-collision ───────────────────────────────────────────────────────
 * Walk ring buffer from tail to head.  If new head position matches
 * any existing segment, it's a self-collision → game over.
 *
 * COMMON BUG NOTE: scan from TAIL, not from head.
 * If you start at head and walk length steps, you would include the slot
 * one past the current head — that slot holds stale data from a previous
 * game cycle.  Starting from tail walks exactly the live segments.
 *
 * JS: segments.some(s => s.x === newX && s.y === newY) */
idx = s->tail;
rem = s->length;
while (rem > 0) {
    if (s->segments[idx].x == new_x && s->segments[idx].y == new_y) {
        if (s->score > s->best_score) s->best_score = s->score;
        s->phase = GAME_PHASE_GAME_OVER;
        game_play_sound(&s->audio, SOUND_GAME_OVER);
        return;  /* exit snake_update entirely */
    }
    idx = (idx + 1) % MAX_SNAKE;
    rem--;
}
```

Note: wall collision uses `break` (exits the `switch` case); self-collision uses `return` (exits the function). Both are correct — the function has nothing left to do after a death. `return` is slightly more explicit about "we're done for this frame".

---

### On Death: Save Best Score and Play Sound

Both death paths share the same three actions:

```c
if (s->score > s->best_score) s->best_score = s->score;
s->phase = GAME_PHASE_GAME_OVER;
game_play_sound(&s->audio, SOUND_GAME_OVER);
```

`best_score` is updated **before** `snake_init()` would erase it on restart. If you updated it during `snake_init()`, it would always see `score == 0` (already reset). The save happens here, at the moment of death.

`game_play_sound` (centred, pan = 0.0) is used for game-over because the sound is not spatially linked to any grid position.

---

### ⚠ Common Bug: Scanning from Head Instead of Tail

Suppose `length = 10`, `head = 9`, `tail = 0`.

**Correct:** start at `tail=0`, walk 10 slots → indices 0..9. All valid segments.

**Buggy:** start at `head=9`, walk 10 slots → indices 9, 10, 11, ..., 18. Slot 10..18 contain **stale data** from the previous game cycle. You will false-positive on a garbage value.

The ring buffer walk pattern is always:

```c
idx = s->tail;
rem = s->length;
while (rem > 0) { ... idx = (idx+1)%MAX_SNAKE; rem--; }
```

---

## What To Notice

- **Wall geometry uses `>= GRID_WIDTH - 1`, not `> GRID_WIDTH - 2`.** Both are equivalent but the `>=` form reads as "reached or past the wall column", which matches how we think about it.
- **The self-collision check comes after the wall check.** If you reversed the order, a move into a corner would report both violations. Wall check first is the natural priority.
- **`best_score` is preserved across restarts** — `snake_init()` saves and restores it. The save at death time ensures the value is current before the next `snake_init()` call.
- **`GAME_PHASE_GAME_OVER` is a full stop.** No movement, no food checks, no collision. Only restart input is processed (see Lesson 10).

---

## Exercises

1. **Visualise the self-collision scan.** Add a debug overlay that draws each segment visited by the self-collision loop in a different colour for one frame after a death. Does it start at the tail and end at the head?

2. **Wrap-around walls (portal mode).** Instead of dying at a wall, teleport the snake to the opposite wall: `new_x = (new_x + GRID_WIDTH) % GRID_WIDTH`. Remove the wall collision check and update `snake_render` to skip drawing walls. Does the self-collision logic still work unchanged?

3. **Distinguish wall vs self-collision.** Add a `GAME_PHASE_DEAD_WALL` and `GAME_PHASE_DEAD_SELF` to the enum. Show different messages on the game-over overlay depending on which phase is active.
