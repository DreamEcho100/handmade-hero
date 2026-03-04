# Lesson 07: Danger Buffer + Frog Collision

## What we're building

A flat 2D collision grid — the **danger buffer** — that tells us every frame
which tiles are safe and which will kill the frog.  We rebuild it from scratch
each frame using `lane_scroll()`, then check whether the frog's current cell
is marked safe or deadly.  When the frog lands on a deadly tile we enter
`PHASE_DEAD`, flash the frog, and respawn at the start position.

## What you'll learn

- Flat 2D arrays as collision grids: `danger[row * width + col]`
- Per-frame grid rebuild with `memset` + selective clear
- Cell-coordinate collision: pixels → cells → danger lookup
- The **critical invariant**: `lane_scroll()` must be called with the *same*
  `state->time` in both `game_update` (danger write) and `game_render` (draw)
- What happens when that invariant is broken (ghost-collision demo)
- Death flash timer (`dead_timer`) and respawn state machine

## Prerequisites

Lessons 01–06 complete and building.  `lane_scroll()` working correctly
with positive-modulo floor-mod arithmetic.

---

## Step 1 — Understand the danger buffer shape

```
Screen:  SCREEN_CELLS_W × SCREEN_CELLS_H = 128 × 80 = 10,240 cells

uint8_t danger[SCREEN_CELLS_W * SCREEN_CELLS_H];
          ↑ one byte per cell; 0 = safe, 1 = deadly
```

**Flat 2D indexing:**

```c
/* Write cell at (col, row) */
danger[row * SCREEN_CELLS_W + col] = 1;  /* deadly */

/* Read cell under frog at pixel position (px_x, px_y) */
int cx = px_x / CELL_PX;
int cy = px_y / CELL_PX;
int is_dead = danger[cy * SCREEN_CELLS_W + cx];
```

**JS analogy** — this is a `Uint8Array` used as a 2D collision map:

```js
const danger = new Uint8Array(SCREEN_CELLS_W * SCREEN_CELLS_H);
danger.fill(1);  // start: everything deadly
// then mark safe cells:
danger[row * SCREEN_CELLS_W + col] = 0;
```

---

## Step 2 — Per-frame rebuild algorithm

The danger buffer is rebuilt **every single frame** in `game_update()`.
This is cheaper than it looks: 10,240 bytes × 1 instruction = ~2 µs on
any modern CPU.  No incremental updates, no event system — pure recalculation.

```c
void game_update(GameState *state, const GameInput *input, float dt) {
    /* 1. Assume everything is deadly */
    memset(state->danger, 1, sizeof(state->danger));

    /* 2. For each lane, mark its safe tiles */
    for (int lane = 0; lane < NUM_LANES; lane++) {
        int tile_start, px_offset;
        lane_scroll(state->time, lane_speeds[lane],
                    &tile_start, &px_offset);

        int row_top  = lane * TILE_CELLS;  /* cell row of lane top */
        /* ... mark cells covered by safe tile types ... */
    }

    /* 3. Check frog position against danger */
    int cx = (int)(state->frog_x + 0.5f);  /* tile position, not pixels */
    int cy = (int)(state->frog_y + 0.5f);
    int px_cx = cx * TILE_PX + TILE_PX / 2;  /* pixel centre of frog tile */
    int cell_cx = px_cx / CELL_PX;
    int cell_cy = cy * TILE_PX / CELL_PX;
    if (state->danger[cell_cy * SCREEN_CELLS_W + cell_cx]) {
        /* frog is on a deadly cell → die */
    }
}
```

---

## Step 3 — The critical invariant (and the ghost-collision bug)

> **New technique: danger buffer consistency invariant**
>
> **What:** `lane_scroll()` must be called with the *same* `state->time` in
> both `game_update()` (writes the danger buffer) and `game_render()` (draws
> the sprites).  If the two calls use different time values, the collision
> grid will silently mismatch what is drawn on screen — the frog dies on
> tiles that look empty, or survives tiles that look solid.
>
> **Why it matters:** `lane_scroll` maps `time → pixel_offset`.  A difference
> of even 0.001 s at speed 3 = 0.192 px → 0 or 1 tile-cell shift.  During
> fast scrolling this shift causes a one-cell mismatch that produces hits
> on "empty air" and misses on actual cars.
>
> **JS equivalent:** If you called `ctx.translate()` with one value and
> `yourCollisionGrid.shift()` with another, sprites and hitboxes would diverge.
>
> **C idiom:** Pass `state->time` to both functions; never call
> `platform_get_time()` between the two to avoid the frame-delta gap.
>
> **Gotcha:** Adding a `dt` accumulation inside `game_render()` would be
> a classic way to introduce this bug.  `game_render()` must be *purely
> a function of `state`* with no side effects on timing.

**Demonstration — introduce the bug deliberately:**

```c
/* BAD: game_render reads its own clock, causing divergence */
static void game_render(Backbuffer *bb, GameState *state) {
    double render_time = platform_get_time();  /* ← BUG: different time! */
    for (int lane = 0; lane < NUM_LANES; lane++) {
        int tile_start, px_offset;
        lane_scroll((float)render_time, lane_speeds[lane],
                    &tile_start, &px_offset);
        /* sprites drawn at slightly different offset than danger buffer */
    }
}
```

Build and run — you'll see the frog die while apparently standing on a safe
log.  Revert to `state->time` and it disappears.

---

## Step 4 — Death flash and respawn

The frog enters `PHASE_DEAD` when the danger check fires.  We flash it for
~1.2 seconds, then respawn at the start position.

```c
/* In game_update — death check */
if (state->phase == PHASE_PLAYING &&
    state->danger[cell_cy * SCREEN_CELLS_W + cell_cx]) {
    state->phase      = PHASE_DEAD;
    state->dead_timer = 1.2f;
    state->lives--;
    game_play_sound(state, SOUND_SPLASH, 0.0f);
}

/* PHASE_DEAD — countdown, then respawn or game over */
if (state->phase == PHASE_DEAD) {
    state->dead_timer -= dt;
    if (state->dead_timer <= 0.0f) {
        if (state->lives <= 0) {
            /* game over — stay in PHASE_DEAD, game_render shows overlay */
        } else {
            state->frog_x = SCREEN_CELLS_W / (2 * TILE_CELLS);
            state->frog_y = SCREEN_CELLS_H / TILE_CELLS - 1;
            state->phase  = PHASE_PLAYING;
        }
    }
}
```

```c
/* In game_render — flash: only draw the frog when timer is above threshold
   or when it pulses (use (int)(timer * 6) % 2 for a 6 Hz blink) */
if (state->phase != PHASE_DEAD ||
    (int)(state->dead_timer * 6.0f) % 2 == 0) {
    /* draw frog */
}
```

---

## Build and run

```bash
# X11 backend
./build-dev.sh --backend=x11 -r

# Raylib backend
./build-dev.sh --backend=raylib -r
```

---

## Expected result

- Move the frog onto a road tile or into the river without a log → frog
  flashes and respawns at the bottom.
- Lives counter decrements on each death.
- After three deaths, the game-over overlay appears.

## Exercises

1. Change `dead_timer = 1.2f` to `0.5f` — how does it feel different?
2. Add a `#ifdef DEBUG` print that shows `cx, cy, danger[…]` when the frog
   dies — use it to verify the cell coordinates are correct.
3. Add a "safe row" at the very bottom (the starting pavement) so the frog
   can stand there without dying.

## What's next

Lesson 08 replaces the coloured rectangles with pixel-art sprites loaded from
`.spr` binary files — the `SpriteBank` fixed pool and `draw_sprite_partial`
UV-crop.
