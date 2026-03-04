# Lesson 06 — Grain Collision

## What you build

You implement the occupancy map and `IS_SOLID` macro that let grains collide
with player-drawn lines, level obstacles, and each other — without any grain
ever tunnelling through a one-pixel-thick wall.

## Concepts introduced

- The `LineBitmap` pixel encoding: `0` = empty, `1` = wall/obstacle,
  `255` = player-drawn line, `2–5` = baked settled grain
- The `IS_SOLID(x, y)` macro and why it checks two separate maps
- `s_occ[]` — the per-frame grain occupancy bitmap
- Sub-step integration to prevent fast grains from tunnelling
- Correct unmark / move / remark sequence for the occupancy map
- Screen-edge collision: OOB-X is solid; OOB-Y is handled separately

## JS → C analogy

| JavaScript                                                        | C                                                                          |
|-------------------------------------------------------------------|----------------------------------------------------------------------------|
| `const occ = new Uint8Array(W * H)`                               | `static uint8_t s_occ[CANVAS_W * CANVAS_H]`                               |
| `occ.fill(0)` each frame                                          | `__builtin_memset(s_occ, 0, sizeof(s_occ))`                               |
| `(x,y) => x<0 \|\| x>=W \|\| bitmap[y*W+x]!==0 \|\| occ[y*W+x]` | `#define IS_SOLID(xx,yy) ((xx)<0\|\|(xx)>=W\|\|lb->pixels[…]\|\|s_occ[…])` |
| `steps = Math.max(1, Math.ceil(Math.abs(dx)+Math.abs(dy)))`       | `int steps = (int)(abs_dx + abs_dy) + 1; if (steps > 32) steps = 32;`    |
| Array access `bitmap[-1]` returns `undefined`                     | `pixels[-1]` is **undefined behaviour** — may crash or corrupt memory      |

## Step-by-step

### Step 1: Understand the `LineBitmap` encoding

Open `game.h` and read the `LineBitmap` block comment. The struct holds a
single flat byte array:

```c
typedef struct {
    uint8_t pixels[CANVAS_W * CANVAS_H];
} LineBitmap;
```

Each byte is a pixel at `(x, y)` → index `y * CANVAS_W + x`. The values mean:

| Value | Meaning |
|-------|---------|
| `0`   | Empty — grains pass through |
| `1`   | Solid wall (obstacle rect or cup wall, stamped at level load) |
| `255` | Player-drawn brush stroke |
| `2`   | Baked `GRAIN_WHITE` grain |
| `3`   | Baked `GRAIN_RED` grain |
| `4`   | Baked `GRAIN_GREEN` grain |
| `5`   | Baked `GRAIN_ORANGE` grain |

The rule the physics engine cares about is simple: **any non-zero value is solid**.
The renderer uses the value to pick the right draw color (see Lesson 07).

### Step 2: Declare the occupancy map

Inside `update_grains()` in `game.c`, directly before the grain loop, you
need a second byte map that tracks where *active* grains currently sit.
Static storage duration keeps it off the stack (307 KB would overflow the
default stack on most platforms):

```c
static uint8_t s_occ[CANVAS_W * CANVAS_H];
__builtin_memset(s_occ, 0, sizeof(s_occ));

/* Seed the occupancy map with every active grain's current pixel. */
for (int i = 0; i < p->count; i++) {
    if (!p->active[i]) continue;
    int ix = (int)p->x[i], iy = (int)p->y[i];
    if (ix >= 0 && ix < W && iy >= 0 && iy < H)
        s_occ[iy * W + ix] = 1;
}
```

We rebuild it from scratch every frame.  There is no "remove from old slot,
add to new slot" bookkeeping across frames — the full `memset` + re-seed is
cheap at 307 KB with a modern CPU's memset throughput (~30 GB/s → ~10 µs).

### Step 3: Define `IS_SOLID`

Define the macro right after the occupancy map seed, before the grain loop:

```c
#define IS_SOLID(xx, yy)                                    \
    ((xx) < 0 || (xx) >= W                                  \
     || lb->pixels[(yy) * W + (xx)]                         \
     || s_occ[(yy) * W + (xx)])
```

Break down each clause:

1. `(xx) < 0` — left edge of canvas → solid (no wrapping in X).
2. `(xx) >= W` — right edge of canvas → solid.
3. `lb->pixels[(yy) * W + (xx)]` — any non-zero byte in `LineBitmap`:
   wall, player line, or baked grain. Note: **no bounds check on Y here**.
   The caller must guarantee `yy` is in `[0, H)` before this macro runs.
4. `s_occ[(yy) * W + (xx)]` — another active grain is sitting here.

Why no OOB check on Y inside the macro? Because Y out-of-bounds is treated
differently: `y < 0` hits the ceiling (stop the grain), and `y >= H` either
wraps the grain (cyclic level) or removes it. These special cases are handled
explicitly in the sub-step loop *before* calling `IS_SOLID`. Adding a Y guard
inside `IS_SOLID` would silently hide a logic bug where you accidentally call
it with an invalid Y.

> **C vs JS**: In JavaScript, `array[-1]` returns `undefined`, which
> coerces to `false` in a boolean context — a silent no-op. In C,
> `pixels[-1]` accesses memory *before* the array. That is **undefined
> behaviour**: the program may crash, silently read garbage, or corrupt
> other data. Always validate Y before calling `IS_SOLID`.

### Step 4: Unmark before moving

At the top of the per-grain loop, immediately unmark the grain's current pixel
so the grain doesn't block *its own* movement:

```c
for (int i = 0; i < p->count; i++) {
    if (!p->active[i]) continue;

    /* Clear this grain's old position so IS_SOLID won't block itself. */
    {
        int cx = (int)p->x[i], cy = (int)p->y[i];
        if (cx >= 0 && cx < W && cy >= 0 && cy < H)
            s_occ[cy * W + cx] = 0;
    }

    /* ... apply gravity, integrate, collision ... */
```

Without this, every grain would immediately treat its own current pixel as
solid and be unable to move.

### Step 5: Apply gravity and cap velocity

```c
p->vy[i] += grav * dt;          /* grav = GRAVITY * gravity_sign */
if (p->vy[i] >  MAX_VY) p->vy[i] =  MAX_VY;
if (p->vy[i] < -MAX_VY) p->vy[i] = -MAX_VY;
if (p->vx[i] >  MAX_VX) p->vx[i] =  MAX_VX;
if (p->vx[i] < -MAX_VX) p->vx[i] = -MAX_VX;
```

`GRAVITY = 280.0f` px/s². Without a terminal velocity cap, a grain falling
for 3 seconds would reach 840 px/s — faster than the canvas is tall and
guaranteed to tunnel.

### Step 6: Sub-step integration

A grain at `vy = 600 px/s` and `dt = 1/60 s` would want to move
`10 px` in one frame. A player-drawn line is 1 px thick. Without sub-steps,
the grain jumps straight through.

Split the displacement into steps of at most ~1 px each:

```c
float pre_x = p->x[i], pre_y = p->y[i];  /* snapshot for settle check */

float total_dx = p->vx[i] * dt;
float total_dy = p->vy[i] * dt;
float abs_dx = total_dx < 0 ? -total_dx : total_dx;
float abs_dy = total_dy < 0 ? -total_dy : total_dy;
int steps = (int)(abs_dx + abs_dy) + 1;
if (steps > 32) steps = 32;   /* cap for performance */
float sdx = total_dx / (float)steps;
float sdy = total_dy / (float)steps;
```

The `+1` ensures at least one step even for very slow grains. The cap of 32
bounds the worst case to 32 collision checks per grain per frame — plenty for
a 640×480 canvas.

### Step 7: Per-sub-step movement and boundary checks

Inside the sub-step loop, test each destination pixel before moving there:

```c
for (int s = 0; s < steps; s++) {
    float nx = p->x[i] + sdx;
    float ny = p->y[i] + sdy;
    int ix = (int)nx;
    int iy = (int)ny;

    /* Left / right canvas edge */
    if (ix < 0) { p->x[i] = 0.f; p->vx[i] = 0.f; break; }
    if (ix >= W) { p->x[i] = (float)(W-1); p->vx[i] = 0.f; break; }

    /* Ceiling */
    if (iy < 0) { p->y[i] = 0.f; p->vy[i] = 0.f; break; }

    /* Floor */
    if (iy >= H) {
        if (lv->is_cyclic) {
            p->y[i] = 1.f; p->x[i] = nx; continue;
        } else {
            p->active[i] = 0; goto next_grain;
        }
    }

    /* Free pixel → accept the move */
    if (!IS_SOLID(ix, iy)) {
        p->x[i] = nx; p->y[i] = ny; continue;
    }

    /* Solid → collision response (next step) */
    /* ... */
    break;  /* always stop sub-stepping after a collision */
}
```

Key points:

- Floor and ceiling checks happen **before** `IS_SOLID` — they handle the OOB-Y
  case that `IS_SOLID` does not guard against.
- `goto next_grain` jumps past the mark-at-new-position and settle check for
  inactive grains. This is one of the rare legitimate uses of `goto` in C: it
  breaks out of multiple nested loops cleanly.

### Step 8: Vertical collision response

When `IS_SOLID(ix, iy)` fires and the grain was moving vertically (`iy != oy`),
stop the downward motion. Slide / bounce logic (Lesson 07) goes here too, but
at minimum you need:

```c
{
    int oy = (int)p->y[i];
    if (iy != oy) {
        /* Hit a surface — zero vy so gravity re-accumulates from zero. */
        p->vy[i] = 0.0f;
        p->vx[i] = 0.0f;
    } else {
        /* Pure horizontal collision → vertical wall. */
        p->vx[i] = 0.0f;
    }
}
break;
```

You'll replace these stubs with the full slide/bounce algorithm in Lesson 07.

### Step 9: Remark at new position

After the sub-step loop (label `next_grain:` is after this block for active
grains), mark the grain's final pixel so subsequent grains see it as solid:

```c
{
    int fx = (int)p->x[i], fy = (int)p->y[i];
    if (fx >= 0 && fx < W && fy >= 0 && fy < H)
        s_occ[fy * W + fx] = 1;
}
```

The order matters:

1. **Unmark** old position (Step 4).
2. Move (Steps 5–8).
3. **Remark** new position (this step).

All three happen within a single grain's iteration, so subsequent grains in
the same frame see the correct, up-to-date occupancy map.

### Step 10: Undef the macro

At the end of `update_grains()`, undef `IS_SOLID` to prevent accidental use
elsewhere (it captures `W`, `lb`, and `s_occ` from local scope):

```c
#undef IS_SOLID
}
```

## Common mistakes to prevent

- **Forgetting the Y bounds check before `IS_SOLID`**: `lb->pixels[(-1)*W+x]`
  accesses memory before the array — undefined behaviour that can corrupt
  adjacent variables or crash the program. Always check `iy >= 0 && iy < H`
  before calling the macro.
- **Not unmarking the current position**: without the unmark at the start of
  the loop, every grain blocks itself and none of them move.
- **Rebuilding `s_occ` after each grain instead of before the loop**: if you
  rebuild inside the loop, grain N never sees grain N+1 at its old position
  and grains can stack into the same pixel.
- **Putting `s_occ` on the stack**: `static uint8_t s_occ[307200]` is 300 KB.
  The default stack is usually 1–8 MB, but in practice the stack is already
  occupied by the call chain. Use `static` to put it in BSS (zero-initialized
  global memory).
- **Capping sub-steps too low**: a cap of 4 steps still lets a fast grain
  (300 px/s, dt=1/60 s → 5 px displacement) tunnel through a 1-px wall.
  Use at least `(int)(abs_dx + abs_dy) + 1` with a cap of 32.

## What to run

```bash
# From the repo root
./build-dev.sh

# Raylib window opens — draw a horizontal line across the middle of the canvas.
# Pour sugar from the emitter. Grains should:
#   1. Fall freely until they hit your line.
#   2. Stop on the line (not pass through).
#   3. Pile up on top of each other (not overlap).
```

Verify that grains stop on both the level obstacles (stamped at load time) and
on lines you draw yourself. If grains tunnel through a line you drew slowly,
check the sub-step formula. If grains overlap each other in a pile, check that
the `s_occ` unmark/remark sequence is correct.

## Summary

This lesson adds the two-map collision system at the heart of the simulation.
`LineBitmap` handles static geometry (walls, obstacles, baked grains) and
`s_occ` handles dynamic grain-on-grain collisions; the `IS_SOLID` macro unifies
them into a single boolean query. Sub-step integration guarantees no grain ever
teleports through a one-pixel wall regardless of frame rate, and the careful
unmark-before/remark-after pattern ensures grains respond to each other within
the same frame while never blocking their own movement.
