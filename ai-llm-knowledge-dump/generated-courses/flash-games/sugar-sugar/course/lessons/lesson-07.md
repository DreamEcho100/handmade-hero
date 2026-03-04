# Lesson 07: Slide, Settle, and Bake — Natural Grain Heaps

## What we're building

When a grain hits a solid surface it should slide sideways — like real sugar on a
slope.  When it has barely moved for several consecutive frames it is "settled" and
gets **baked** into the `LineBitmap`, freeing its pool slot for the next grain.

After this lesson, grains form natural heap shapes on drawn lines and the pool
never fills up no matter how long you run.

## What you'll learn

- The diagonal-slide algorithm and why ±2 pixels instead of ±1
- The `GRAIN_SLIDE_REACH` constant and the angle-of-repose it controls
- Displacement-based settle detection — frame-rate independent
- `GRAIN_SETTLE_FRAMES` — how many consecutive still frames before baking
- How baking works: store `color + 2` in `LineBitmap`, mark slot inactive

## Prerequisites

- Lesson 06 complete (IS_SOLID and player drawing working)

---

## Step 1: Slide on collision

Replace the simple `vy = -vy * bounce` collision handler from Lesson 06 with the
full Sugar Sugar slide logic.

```c
/* src/game.c  —  collision block inside update_grains inner sub-step loop
 * (replaces the simple bounce from Lesson 06) */

/* COLLISION — grain cannot enter (ix, iy).
 *
 * Sugar Sugar rule:
 *   If the grain has a VERTICAL velocity component (iy != oy) it is
 *   hitting a surface from above.  Like real sugar on a slope, try to
 *   slide to an adjacent free pixel at the same target row.
 *
 *   Preferred slide direction = current vx sign (preserve momentum).
 *   Fallback direction = opposite.
 *   Check up to GRAIN_SLIDE_REACH pixels to each side.
 *
 * Why ±2 (GRAIN_SLIDE_REACH = 2) instead of ±1?
 *   ±1 only lets a grain slide past one neighbour.  A 2-wide pile blocks it
 *   immediately — the heap forms at 45° (too steep, not sugar-like).
 *   ±2 lets grains hop over a single neighbour and land one pixel further,
 *   creating a shallower ~27° angle of repose — much more natural.
 *
 *   If the grain is moving ONLY HORIZONTALLY (iy == oy) it hit a vertical
 *   wall → kill vx, let gravity resume on the next frame. */
{
  int oy = (int)p->y[i];
  int ox = (int)p->x[i];

  if (iy != oy) {
    /* Surface hit from above (or below in reversed gravity) */
    int d1   = (p->vx[i] >= 0.0f) ? 1 : -1; /* preferred direction */
    int d2   = -d1;                           /* fallback direction  */
    int slid = 0;

    for (int dist = 1; dist <= GRAIN_SLIDE_REACH && !slid; dist++) {
      for (int attempt = 0; attempt < 2 && !slid; attempt++) {
        int d  = (attempt == 0) ? d1 : d2;
        int sx = ox + d * dist;

        if (sx < 0 || sx >= W) continue;

        /* Path-clear check: every pixel between ox and sx at row iy
         * must be free — prevents the grain from teleporting through a wall. */
        int path_clear = 1;
        for (int px2 = ox + d; px2 != sx + d; px2 += d) {
          if (IS_SOLID(px2, iy)) { path_clear = 0; break; }
        }
        if (!path_clear) continue;

        if (!IS_SOLID(sx, iy)) {
          /* Found a free diagonal slot — slide there. */
          float impact_vy = p->vy[i] < 0 ? -p->vy[i] : p->vy[i];
          float spd = (impact_vy < GRAIN_SLIDE_MIN) ? GRAIN_SLIDE_MIN : impact_vy;
          if (spd > MAX_VX) spd = MAX_VX;

          p->vx[i] = (float)d * spd;
          /* Small bounce up after sliding off a slope — lively feel */
          p->vy[i] = (impact_vy > GRAIN_BOUNCE_MIN)
                       ? -impact_vy * GRAIN_BOUNCE : 0.0f;
          p->x[i]  = (float)sx;
          p->y[i]  = (float)iy;
          slid = 1;
        }
      }
    }

    if (!slid) {
      /* Completely blocked (flat surface, no room to slide) */
      float imp = p->vy[i] < 0 ? -p->vy[i] : p->vy[i];
      p->vy[i] = (imp > GRAIN_BOUNCE_MIN) ? -imp * GRAIN_BOUNCE : 0.0f;
      p->vx[i] = 0.0f;
    }

  } else {
    /* Pure horizontal collision → vertical wall → stop sideways movement */
    p->vx[i] = 0.0f;
  }
}
break; /* always stop sub-stepping after any collision */
```

Add the new constants to `game.h`:

```c
/* src/game.h  —  slide and settle constants */

/* GRAIN_SLIDE_REACH: lateral pixels checked for a free diagonal slot.
 * 1 → 45° angle of repose (steep wall-like heap)
 * 2 → ~27° angle of repose (shallow, sugar-like)          */
#define GRAIN_SLIDE_REACH   2

/* GRAIN_SLIDE_MIN: minimum lateral speed assigned on a surface slide.
 * Ensures the grain is always fast enough to be detected as "moving"
 * by the displacement-based settle check, even on shallow impacts. */
#define GRAIN_SLIDE_MIN     25.0f

/* GRAIN_HORIZ_DRAG: multiply vx by this each frame.
 * 0.995 ≈ very gentle decay — grains slide slowly to a stop on flat surfaces. */
#define GRAIN_HORIZ_DRAG    0.995f
```

Apply the drag in `update_grains` before the sub-step loop:

```c
p->vx[i] *= GRAIN_HORIZ_DRAG;
```

---

## Step 2: Displacement-based settle detection

A grain should bake when it has barely moved for several consecutive frames.

**Why displacement, not velocity?**

Using `if (vy == 0 && vx == 0)` fails because gravity re-adds ~4 px/s every frame.
Even a stationary grain on a flat surface has `vy ≈ 4` each frame (gravity increments
it, then the collision sets it to 0 — but we've already applied drag and gravity by
the time we check).

**Why not a fixed pixel threshold?**

At 1000 fps, `dt = 0.001 s` and a grain moving at 10 px/s travels only 0.01 px —
always below any fixed threshold, so ALL grains would immediately bake even while
still moving at full speed.

**The correct approach** — compare displacement to a threshold that scales with `dt`:

```
speed ≈ dist / dt   →   dist < GRAIN_SETTLE_SPEED * dt
```

In squared form (avoids `sqrtf` in the hot path):

```
dist² < (GRAIN_SETTLE_SPEED * dt)²
```

```c
/* src/game.h */
#define GRAIN_SETTLE_SPEED  10.0f  /* px/s: grain is "still" below this speed */
#define GRAIN_SETTLE_FRAMES  8     /* consecutive still frames → bake          */
```

In `update_grains`, capture position BEFORE the sub-step loop:

```c
float pre_x = p->x[i], pre_y = p->y[i]; /* snapshot before integration */
```

After the sub-step loop (and before re-marking `s_occ`), check displacement:

```c
/* src/game.c  —  settle check (after sub-step loop, before s_occ re-mark) */
{
  float settle_sq = GRAIN_SETTLE_SPEED * GRAIN_SETTLE_SPEED * dt * dt;
  float ddx = p->x[i] - pre_x;
  float ddy = p->y[i] - pre_y;

  if (ddx * ddx + ddy * ddy < settle_sq) {
    /* Grain barely moved this frame — increment still counter */
    if (p->still[i] < 255) p->still[i]++;
  } else {
    /* Grain moved — reset still counter */
    p->still[i] = 0;
  }

  /* Bake if still for enough consecutive frames */
  if (p->still[i] >= GRAIN_SETTLE_FRAMES) {
    int bx = (int)p->x[i], by = (int)p->y[i];
    if (bx >= 0 && bx < W && by >= 0 && by < H) {
      /* Store color as (color + 2) in the line bitmap.
       * 2 = GRAIN_WHITE baked, 3 = GRAIN_RED baked, etc.
       * The renderer reads v - 2 as the color index. */
      lb->pixels[by * W + bx] = (uint8_t)(p->color[i] + 2);
    }
    p->active[i] = 0;  /* free the pool slot */
    goto next_grain;
  }
}
```

---

## Step 3: Result — natural heaps

With slide + settle + bake, the simulation now:

1. Grain hits a surface → slides to the nearest free diagonal pixel
2. After sliding, continues to fall/bounce/settle
3. Once displacement < `GRAIN_SETTLE_SPEED × dt` for 8 frames → baked into `LineBitmap`
4. Pool slot freed → new grain can spawn

The pool stays bounded because baking recycles slots.  Grains form sloped heaps
that match the natural angle of repose (≈27° with `GRAIN_SLIDE_REACH = 2`).

---

## Full `update_grains` structure

```
update_grains(state, dt):
  rebuild s_occ from current positions
  for each active grain i:
    unmark s_occ[current position]   ← don't block self
    pre_x = x[i], pre_y = y[i]       ← snapshot for settle check
    apply gravity + clamp
    apply horiz drag
    sub-step loop:
      for each sub-step s:
        new_x = x + sdx, new_y = y + sdy
        if new_x out of bounds → wall collision, break
        if new_y < 0 → ceiling, break
        if new_y >= H → remove grain, goto next_grain
        if not IS_SOLID(new_x, new_y) → move, continue sub-step
        COLLISION:
          if vertical component: diagonal slide attempt ±GRAIN_SLIDE_REACH
          else: horizontal wall, kill vx
        break
    settle check: dist² vs threshold
    if still >= GRAIN_SETTLE_FRAMES → bake + free
    re-mark s_occ[final position]
  next_grain:
```

---

## Build and run

```bash
./build-dev.sh -r
```

**Expected output:** Grains form smooth sloped heaps on drawn lines.  The pile height stabilises (baking keeps the pool healthy).  Grains on steep surfaces slide to flatter positions.

---

## Exercises

1. Change `GRAIN_SLIDE_REACH` to `1` and observe the steep 45° heap shape.  Change it to `3` — what happens to very shallow lines?
2. Change `GRAIN_SETTLE_FRAMES` to `1` (bake immediately).  Do grains still form natural heaps, or do they freeze mid-air?
3. Remove `GRAIN_HORIZ_DRAG` (set to 1.0f).  Do grains slide forever on flat surfaces?  What minimum drag value keeps them from crossing the entire canvas?

---

## What's next

In Lesson 08 we add the game's state machine — the `GAME_PHASE` enum and
`change_phase()` — turning the simulation sandbox into a structured game with
a title screen, playing phase, and level-complete celebration.
