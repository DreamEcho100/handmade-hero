# Lesson 07 — Slide, Settle, and Bake

## What you build

You extend the collision response from Lesson 06 so that grains slide along
sloped surfaces instead of stacking into vertical towers, bounce lightly on
hard impacts, come to rest after a few still frames, and finally get "baked"
into the `LineBitmap` — freeing their pool slot so the emitter can spawn more.

## Concepts introduced

- `GRAIN_SLIDE_REACH` — how far diagonally a grain searches for a free pixel
- The preferred-direction diagonal slide algorithm and path-clear guard
- `GRAIN_SLIDE_MIN` — minimum slide speed to prevent false "settled" detection
- `GRAIN_BOUNCE` / `GRAIN_BOUNCE_MIN` — coefficient of restitution and the
  threshold that stops micro-bounces
- Displacement-based settle detection (`dist² < (GRAIN_SETTLE_SPEED × dt)²`)
- `GRAIN_SETTLE_FRAMES = 8` — consecutive still frames before baking
- Baking: writing `grain_color + 2` into `LineBitmap` and freeing the slot
- Why baked grains are rendered from `LineBitmap`, *not* from the pool

## JS → C analogy

| JavaScript concept | C equivalent |
|--------------------|--------------|
| Matter.js `Body.setStatic(true)` after a body "sleeps" | `lb->pixels[by*W+bx] = color+2; p->active[i] = 0` |
| `body.speed < threshold` for sleep detection | `dist² < (GRAIN_SETTLE_SPEED*dt)²` (displacement-based, frame-rate independent) |
| `Math.abs(vy) * restitution` for bounce | `-fabsf(vy) * GRAIN_BOUNCE` |
| Checking `vx < 0.01` for "stopped" | Unreliable at uncapped FPS — use displacement instead |

## Step-by-step

### Step 1: Understand `GRAIN_SLIDE_REACH`

When a grain hits a surface (`IS_SOLID` fires on the destination pixel), it
first checks whether there is a free diagonal pixel it can slide to. The
constant `GRAIN_SLIDE_REACH = 2` controls how far it looks:

- `GRAIN_SLIDE_REACH = 1` → checks only the immediately adjacent pixel
  (±1). This gives an angle of repose of ~45°; grains stack like a wall.
- `GRAIN_SLIDE_REACH = 2` → checks up to ±2 pixels away. Angle of repose
  ~27°; grains flow more like sugar or sand, matching the original game.

The relevant constant in `game.h`:

```c
#define GRAIN_SLIDE_REACH   2   /* max lateral pixels checked for slide */
```

### Step 2: Implement the diagonal slide loop

This code goes inside the `if (iy != oy)` branch of the collision response,
replacing the stub from Lesson 06:

```c
int d1 = (p->vx[i] >= 0.0f) ? 1 : -1;  /* preferred direction = vx sign */
int d2 = -d1;                            /* fallback direction */
int slid = 0;

for (int dist = 1; dist <= GRAIN_SLIDE_REACH && !slid; dist++) {
    for (int attempt = 0; attempt < 2 && !slid; attempt++) {
        int d = (attempt == 0) ? d1 : d2;
        int sx = ox + d * dist;
        if (sx < 0 || sx >= W) continue;

        /* Path-clear check: every pixel between ox and sx at row iy
         * must be free — prevents the grain teleporting through a wall. */
        int path_clear = 1;
        for (int px = ox + d; px != sx + d; px += d) {
            if (IS_SOLID(px, iy)) { path_clear = 0; break; }
        }
        if (!path_clear) continue;

        if (!IS_SOLID(sx, iy)) {
            /* Slide speed: max(|vy|, GRAIN_SLIDE_MIN) */
            float impact_vy = fabsf(p->vy[i]);
            float spd = impact_vy;
            if (spd < GRAIN_SLIDE_MIN) spd = GRAIN_SLIDE_MIN;
            if (spd > MAX_VX) spd = MAX_VX;
            p->vx[i] = (float)d * spd;
            p->vy[i] = (impact_vy > GRAIN_BOUNCE_MIN)
                        ? -impact_vy * GRAIN_BOUNCE
                        : 0.0f;
            p->x[i] = (float)sx;
            p->y[i] = (float)iy;
            slid = 1;
        }
    }
}
```

Walk through each decision:

- **Preferred direction** (`d1`): if the grain is already moving right, try
  right first. This keeps momentum consistent and avoids jitter from
  constantly switching direction.
- **Fallback** (`d2`): if the preferred side is blocked, try the opposite.
- **`dist` loop**: tries 1, then 2 pixels away. Stops as soon as a free
  pixel is found (`slid = 1`).
- **Path-clear guard**: for `dist = 2`, the intermediate pixel at `ox + d`
  must also be free. Without this, a grain could jump over a 1-px wall —
  effectively tunnelling horizontally.

### Step 3: Handle the "completely blocked" case

When neither diagonal is free, the grain is resting flat (e.g. top of a pile).
Apply the bounce if the impact was fast enough, otherwise just stop:

```c
if (!slid) {
    float impact_vy = fabsf(p->vy[i]);
    p->vy[i] = (impact_vy > GRAIN_BOUNCE_MIN)
                ? -impact_vy * GRAIN_BOUNCE
                : 0.0f;
    p->vx[i] = 0.0f;
}
```

Constants from `game.h`:

```c
#define GRAIN_BOUNCE        0.25f   /* fraction of |vy| preserved on bounce */
#define GRAIN_BOUNCE_MIN   20.0f   /* px/s: only bounce if |vy| > this      */
```

Example chain for a grain landing at `vy = 200 px/s`:

| Bounce | `|vy|` before | `|vy|` after |
|--------|---------------|--------------|
| 1st    | 200           | 50 (upward)  |
| 2nd    | 50            | 12.5         |
| 3rd    | 12.5 < 20.0   | 0 (stopped)  |

### Step 4: Why `GRAIN_SLIDE_MIN` exists

After a grain slides diagonally, it gets `vx = spd` where
`spd = max(|vy|, GRAIN_SLIDE_MIN)`. The minimum of 25 px/s matters because:

- On very shallow contacts (grain barely grazed the surface), `|vy|` after
  gravity re-accumulation might be only 4–8 px/s.
- At 60 fps, `dist = 4 * 1/60 ≈ 0.07 px/frame`.
- The settle threshold is `GRAIN_SETTLE_SPEED * dt = 10 * 1/60 ≈ 0.17 px/frame`.
- `0.07 < 0.17` → the grain would immediately be counted as "still" even
  though it just found a free slide pixel and should keep moving.
- With `GRAIN_SLIDE_MIN = 25`, `dist = 25 * 1/60 ≈ 0.42 px >> 0.17 px` →
  the sliding grain is always detected as moving. Safe.

The minimum fires **only** inside the "free diagonal found" branch. When both
diagonals are blocked, the `!slid` path zeros the velocity regardless — so
the minimum never prevents a grain from settling on a flat pile.

### Step 5: Displacement-based settle detection

After the sub-step loop, compare the grain's displacement this frame against a
threshold that scales with `dt`. This is **frame-rate independent**:

```c
/* Snapshot pre_x, pre_y was captured before the sub-step loop. */
float settle_sq = GRAIN_SETTLE_SPEED * GRAIN_SETTLE_SPEED * dt * dt;
float ddx = p->x[i] - pre_x;
float ddy = p->y[i] - pre_y;
if (ddx*ddx + ddy*ddy < settle_sq) {
    if (p->still[i] < 255) p->still[i]++;
} else {
    p->still[i] = 0;
}
```

`GRAIN_SETTLE_SPEED = 10.0f` px/s.

**Why not check velocity?**
After a collision, `vy` is reset to zero — but the *next frame* gravity adds
`GRAVITY * dt ≈ 280/60 ≈ 4.67 px/s` back. Checking `|vy| < threshold` means
every grain appears to be moving (due to gravity re-accumulation) and never
settles. Displacement captures what actually happened: if the grain barely
moved, it is stuck.

**Why not a fixed pixel-distance threshold?**
At 1000 fps, `dt = 0.001 s`, so `dist = v * dt = 0.001 px/frame` for any
speed — even 1 px/s looks "still". A fixed threshold like `dist < 0.01 px`
would mark even fast-moving grains as settled at high frame rates. Scaling
the threshold by `dt` keeps it proportional to the expected displacement.

### Step 6: Bake into `LineBitmap` after `GRAIN_SETTLE_FRAMES`

```c
if (p->still[i] >= GRAIN_SETTLE_FRAMES) {
    int bx = (int)p->x[i], by = (int)p->y[i];
    if (bx >= 0 && bx < W && by >= 0 && by < H)
        lb->pixels[by * W + bx] = (uint8_t)(p->color[i] + 2);
    p->active[i] = 0;
    goto next_grain;
}
```

The encoding `color + 2`:

| `GRAIN_COLOR` value | `color + 2` stored | Meaning |
|---------------------|-------------------|---------|
| `GRAIN_WHITE  = 0`  | `2`               | Baked cream-colored grain |
| `GRAIN_RED    = 1`  | `3`               | Baked red grain |
| `GRAIN_GREEN  = 2`  | `4`               | Baked green grain |
| `GRAIN_ORANGE = 3`  | `5`               | Baked orange grain |

This overlaps with the range `1` (wall) and `255` (player line) but does not
collide with them: walls use `1` and lines use `255`, leaving `2–5` exclusively
for baked grains.

After baking:

1. The pixel at `(bx, by)` in `LineBitmap` is now non-zero → `IS_SOLID`
   returns true → future grains cannot enter that pixel.
2. `p->active[i] = 0` frees the pool slot for the emitter to reuse.
3. `goto next_grain` skips the remark step — the grain no longer exists in the
   dynamic simulation.

### Step 7: Rendering baked grains correctly

Baked grains are rendered by `render_lines()`, **not** `render_grains()`.
The pool slot has been freed (`active[i] = 0`) and will be reused for a brand
new grain with a different position and possibly a different color. Reading from
the pool for a baked grain would give wrong data.

In `render_lines()` (already present in `game.c`):

```c
for (int i = 0; i < total; i++) {
    uint8_t v = lb->pixels[i];
    if (!v) continue;
    bb->pixels[i] = (v >= 2 && v <= 5)
        ? g_grain_colors[v - 2]   /* baked grain: original color */
        : COLOR_LINE;             /* drawn line or obstacle wall */
}
```

`g_grain_colors[v - 2]` recovers the original `uint32_t` ARGB pixel color
from the compact byte encoding. The table is defined in `game.c`:

```c
static const uint32_t g_grain_colors[GRAIN_COLOR_COUNT] = {
    [GRAIN_WHITE]  = COLOR_CREAM,
    [GRAIN_RED]    = COLOR_RED,
    [GRAIN_GREEN]  = COLOR_GREEN,
    [GRAIN_ORANGE] = COLOR_ORANGE,
};
```

### Step 8: Horizontal drag

After every frame, the grain's horizontal velocity decays slightly:

```c
p->vx[i] *= GRAIN_HORIZ_DRAG;   /* GRAIN_HORIZ_DRAG = 0.995f */
```

`0.995^60 ≈ 0.74` per second — very gentle. This ensures a grain sliding off a
long ramp does not slide forever, and that small residual `vx` after a vertical
collision decays to zero within a few frames. Without drag, grains slide
indefinitely on a perfectly flat surface.

## Common mistakes to prevent

- **Rendering baked grains from the pool**: slot `i` is recycled immediately
  after `p->active[i] = 0`. Rendering it gives the color and position of
  whatever new grain occupies that slot next frame. Always render baked grains
  from `LineBitmap`.
- **Checking `vx < small_constant` for settle**: frame-rate dependent. A grain
  at 1 px/s moves 0.001 px/frame at 1000 fps (looks "still") but 0.1 px/frame
  at 10 fps (looks moving). Use `dist² < (GRAIN_SETTLE_SPEED * dt)²`.
- **Skipping the path-clear check for `GRAIN_SLIDE_REACH = 2`**: a grain can
  teleport horizontally through a 1-px wall if you only check the destination
  pixel without verifying the intermediate pixel is also free.
- **Setting `GRAIN_SLIDE_MIN` too high**: at 50 px/s, slides fly visibly away
  from the pile, forming "satellite" secondary heaps. The value 25 px/s keeps
  slide-off grains close enough to look cohesive.
- **Applying bounce unconditionally**: without `GRAIN_BOUNCE_MIN`, a grain
  at `|vy| = 1 px/s` bounces to `0.25 px/s`, then to `0.0625 px/s` forever —
  perpetual micro-bounces that prevent settling. Only bounce when
  `|vy| > GRAIN_BOUNCE_MIN = 20 px/s`.
- **Forgetting `goto next_grain` after baking**: if execution falls through to
  the remark step, `s_occ[by*W+bx] = 1` re-marks the baked pixel as occupied
  by a *dynamic* grain, causing the next grain that tries to settle there to be
  incorrectly deflected.

## What to run

```bash
./build-dev.sh
```

Draw a diagonal line from the top-left area down to the right. Pour sugar from
the emitter. Observe:

1. Grains slide along the diagonal slope and accumulate at the bottom of the
   ramp (not pile into a vertical tower).
2. When a grain lands on a flat pile and bounces, it bounces visibly once or
   twice then comes to rest.
3. After several frames of stillness the grain disappears from the dynamic sim
   and reappears as a colored pixel that is visually identical (baked).
4. The emitter continues spawning new grains — pool slots are being recycled.

## Summary

The slide algorithm turns the simulation from a "grain freezes on first
contact" demo into a convincing sand/sugar cellular automaton. Grains prefer
their current horizontal direction, check that the full path is clear before
sliding, and use `GRAIN_SLIDE_MIN` to stay detectable by the displacement
settle check. Bouncing with a restitution coefficient gives impacts a satisfying
lively feel without producing perpetual micro-bounces. Once `GRAIN_SETTLE_FRAMES`
consecutive still frames are counted, a grain is promoted to static geometry
by writing its color index into `LineBitmap` — freeing its pool slot and
becoming part of the collision mesh for all future grains.
