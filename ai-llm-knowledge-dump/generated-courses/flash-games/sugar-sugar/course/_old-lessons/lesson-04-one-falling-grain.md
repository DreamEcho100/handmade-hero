# Lesson 04 — One Falling Grain

## What you build

A single grain that spawns at the top of the canvas, accelerates downward under gravity, and stops when it hits the bottom — using Euler integration with a delta-time, a velocity cap, and the Y-DOWN coordinate system.

## Concepts introduced

- Delta-time (`dt`) and why it decouples physics from frame rate
- Euler integration: `x += vx * dt; y += vy * dt; vy += GRAVITY * dt`
- **Y-DOWN exception**: `GRAVITY` is positive, larger `y` means lower on screen
- Velocity clamping (`MAX_VY`) to prevent tunneling
- `dt` cap: `if (dt > 0.1f) dt = 0.1f`
- `float` positions for sub-pixel accuracy; `(int)` cast for pixel writes
- The `GrainPool` SoA layout preview (this lesson uses a single grain conceptually)
- Constants `GRAVITY`, `MAX_VY` from `game.c`

---

## JS → C analogy

| JavaScript / Canvas 2D                                     | C equivalent                                       |
|------------------------------------------------------------|---------------------------------------------------|
| `const elapsed = timestamp - prevTimestamp` (milliseconds) | `float dt = (float)(curr - prev)` (seconds)      |
| `const dt = Math.min(elapsed / 1000, 0.1)`                | `if (delta_time > 0.1f) delta_time = 0.1f;`      |
| `vy += gravity * dt`                                       | `p->vy[i] += grav * dt;`                         |
| `y += vy * dt`                                             | `p->y[i] += p->vy[i] * dt;` (inside sub-step)   |
| `vy = Math.min(vy, maxFallSpeed)`                          | `if (p->vy[i] > MAX_VY) p->vy[i] = MAX_VY;`     |
| `Math.floor(y)` for integer pixel                          | `(int)p->y[i]` (truncation)                      |
| `canvas.height - y` to flip Y axis                        | Not needed — Y-DOWN is native; larger y = lower  |

---

## Step-by-step

### Step 1: Why Y-DOWN is the right choice here

Most physics textbooks and many game engines use Y-UP: the origin is at the bottom-left, positive Y goes upward. This matches mathematical convention.

This game uses **Y-DOWN**: the origin is at the top-left, positive Y goes downward. This matches screen-pixel convention.

**Why is Y-DOWN the right choice for a falling-sand game?**

The `LineBitmap` and `GameBackbuffer` are both indexed as `pixels[y * width + x]`. Row 0 is the topmost row on screen. Row 479 is the bottommost row. If gravity pulled in the +Y direction and we mapped Y-UP, every single coordinate conversion would require:

```c
screen_y = CANVAS_H - 1 - (int)grain.y;
```

That conversion would appear in the physics, the rendering, the collision detection, the Bresenham line drawing, and every UI element. It adds complexity with zero benefit for a cellular-automaton grid that maps 1-to-1 to screen pixels.

**The practical consequence:** the gravity constant is **positive**:

```c
#define GRAVITY  280.0f   /* pixels / second² */
```

`vy += GRAVITY * dt` increases `vy`, which moves the grain to a higher row index (downward on screen). This is correct, natural, and intentional.

### Step 2: The `GRAVITY` and `MAX_VY` constants

Defined at the top of `src/game.c`:

```c
#define GRAVITY   280.0f   /* pixels / second² — lower than default 9.8×scale
                            * to give grains a "light sugar" feel: slow
                            * enough to follow drawn lines comfortably       */
#define MAX_VY    600.0f   /* terminal velocity (px/s)                       */
#define MAX_VX    200.0f   /* max horizontal slide speed (px/s)              */
```

**Why 280 px/s² instead of 9.8 m/s²?**

There is no concept of "meters" in a pixel canvas. `9.8 m/s²` at some pixel-per-meter scale would either make grains fall imperceptibly slowly or teleport several rows per frame. `280 px/s²` was tuned so that:
- At 60 fps, a grain starting from rest reaches row 240 (the middle of the screen) in about 1.3 seconds.
- Grains feel like light sugar rather than heavy marbles.
- Grains move slowly enough to follow drawn guide lines comfortably.

**Terminal velocity `MAX_VY = 600 px/s`:**

Without a cap, a grain falling from the top of the screen would reach `vy ≈ GRAVITY * sqrt(2 * CANVAS_H / GRAVITY) ≈ 520 px/s` just before hitting the floor. The cap at 600 gives a little headroom. More importantly, it prevents runaway velocities if a grain somehow accumulates `dt` values larger than expected.

### Step 3: Delta-time — decoupling physics from frame rate

`delta_time` (called `dt` in the physics functions) is the number of seconds that elapsed since the last frame. The game loop computes it:

```c
double curr_time  = platform_get_time();
float  delta_time = (float)(curr_time - prev_time);
prev_time = curr_time;
```

**Without dt scaling:** `vy += 280.0f` each frame. At 60 fps that means 280 px/s² effect per frame, but at 120 fps it means 280 × 2 = 560 px/s² effect. The physics would run twice as fast on a faster machine.

**With dt scaling:** `vy += 280.0f * dt`. At 60 fps, `dt ≈ 0.0167 s`, so `vy` increases by `280 × 0.0167 ≈ 4.67 px/s per frame`. At 120 fps, `dt ≈ 0.0083 s`, so `vy` increases by `2.33 px/s per frame` — but twice as many frames run. The net change per second is identical.

**JS analogy:**

```js
function loop(timestamp) {
    const dt = Math.min((timestamp - prev) / 1000, 0.1); // seconds, capped
    prev = timestamp;
    vy += GRAVITY * dt;
    y  += vy * dt;
    requestAnimationFrame(loop);
}
```

The C version is structurally identical.

### Step 4: The `dt` cap

```c
/* Cap delta-time so a debugger pause doesn't explode the physics. */
if (delta_time > 0.1f) delta_time = 0.1f;
```

This appears in **two places**: the main loop in `main_x11.c` and at the top of `game_update` in `game.c`:

```c
void game_update(GameState *state, GameInput *input, float dt) {
  if (dt > 0.1f)
    dt = 0.1f;
  ...
}
```

**Why does it appear twice?**

The main loop cap protects the physics from receiving a huge `dt` that was computed from wall-clock time. The `game_update` cap is a second line of defense — if a platform backend forgot to cap, or passed in an uncapped value, `game_update` will still be safe.

**What happens without the cap?**

If you pause the program in a debugger for 5 seconds and then resume, `curr_time - prev_time = 5.0 s`. With `GRAVITY = 280 px/s²`:

```
vy_after = vy_before + 280 * 5.0 = +1400 px/s
```

In the next frame, `y_after = y + 1400 * 5.0 = y + 7000 pixels` — the grain teleports far below the canvas and disappears. The cap ensures `dt ≤ 0.1 s` regardless of pauses, so the worst case is a small visible "jump" rather than a physics explosion.

### Step 5: Euler integration

Euler integration is the simplest way to advance a physics simulation one time step:

```
velocity += acceleration * dt    (Newton's second law: F=ma → a=F/m)
position += velocity * dt        (kinematics)
```

For a grain with only gravity (no initial horizontal velocity):

```c
/* Apply gravity (grav = GRAVITY * gravity_sign, + = down) */
p->vy[i] += grav * dt;

/* Clamp to terminal velocity */
if (p->vy[i] >  MAX_VY) p->vy[i] =  MAX_VY;
if (p->vy[i] < -MAX_VY) p->vy[i] = -MAX_VY;

/* Move (inside sub-step loop, sdy = vy * dt / steps) */
float ny = p->y[i] + sdy;
```

The `sdy` is the per-sub-step vertical displacement. The actual full-frame displacement would be `p->vy[i] * dt`, but it is split into steps to prevent tunneling.

**Euler is not perfect** — it over-estimates position for accelerating objects (the grain moves slightly too far in the first half of the time step because the updated velocity is applied retroactively). For a game with coarse pixel positions and short time steps, this error is imperceptible.

### Step 6: Sub-step integration to prevent tunneling

If `vy = 600 px/s` and `dt = 0.016 s`, the grain wants to move `600 × 0.016 = 9.6` pixels in one frame. If a wall is 5 pixels away, a naive single-step integration would move the grain 9.6 pixels, skipping past the 5-pixel wall entirely (tunneling).

The sub-step loop in `update_grains` splits the motion into at most 1-pixel steps:

```c
float total_dx = p->vx[i] * dt;
float total_dy = p->vy[i] * dt;
float abs_dx = total_dx < 0 ? -total_dx : total_dx;
float abs_dy = total_dy < 0 ? -total_dy : total_dy;
int steps = (int)(abs_dx + abs_dy) + 1;
if (steps > 32) steps = 32;           /* performance cap */
float sdx = total_dx / (float)steps;
float sdy = total_dy / (float)steps;

for (int s = 0; s < steps; s++) {
    float nx = p->x[i] + sdx;
    float ny = p->y[i] + sdy;
    int ix = (int)nx;
    int iy = (int)ny;
    /* ... bounds and collision checks ... */
    if (!IS_SOLID(ix, iy)) {
        p->x[i] = nx;
        p->y[i] = ny;
        continue;
    }
    /* handle collision */
    break;
}
```

Each sub-step moves at most `1 / steps` of the total displacement — roughly 1 pixel per sub-step at maximum velocity. The collision check fires at each sub-step position, so a 1-pixel-thick wall is never tunneled through.

### Step 7: `float` positions and `(int)` cast for pixel writes

Grains store their position as `float`:

```c
float x[MAX_GRAINS];   /* horizontal position, pixels, sub-pixel precision */
float y[MAX_GRAINS];   /* vertical   position                               */
```

This allows sub-pixel velocities to accumulate correctly. A grain falling at `vy = 4.67 px/s` advances `0.078 px/frame` at 60 fps. An integer position would stay at the same row for 12–13 frames before jumping by 1 — visually jerky.

When writing to the backbuffer or checking the `LineBitmap`, the float is truncated to an integer:

```c
int ix = (int)p->x[i];
int iy = (int)p->y[i];
```

`(int)` in C truncates toward zero: `(int)3.9f = 3`, `(int)-0.1f = 0`. For pixel positions that are always `>= 0`, this is equivalent to `floor`.

**Common mistake — using integer positions for velocity accumulation:**

```c
/* WRONG: integer position */
int x = 320, y = 0;
int vyi = 0;  /* integer velocity */

vyi += (int)(GRAVITY * dt);  /* rounds 4.67 → 4 */
y   += vyi;                  /* moves 4 px/frame at 60fps instead of 4.67 */
```

At 60 fps, the grain accumulates `4 px/frame` instead of `4.67`, reaching the floor ~12% slower than physics says it should. Worse, if `dt` is large enough that `GRAVITY * dt < 1.0f`, the velocity never increases at all — the grain appears frozen in mid-air.

### Step 8: Gravity sign (normal vs flipped)

The `gravity_sign` field in `GameState` allows flipping gravity:

```c
int gravity_sign; /* +1 = down (normal), -1 = up (flipped) */
```

In `update_grains`:

```c
float grav = GRAVITY * (float)state->gravity_sign;
```

When `gravity_sign = -1`, `grav = -280.0f`. Grains accelerate upward (decreasing `y`). The floor check becomes the ceiling check and vice versa. This is the G-key mechanic seen in some Sugar Sugar levels.

### Step 9: Floor and ceiling handling

After the sub-step integration computes `(ix, iy)`, boundary checks run before the solid check:

```c
/* ---- Screen boundary: ceiling ---- */
if (iy < 0) {
    p->y[i] = 0.f;
    p->vy[i] = 0.f;
    break;
}

/* ---- Screen boundary: floor → wrap or remove ---- */
if (iy >= H) {
    if (lv->is_cyclic) {
        p->y[i] = 1.f;   /* wrap to top */
        p->x[i] = nx;
        continue;
    } else {
        p->active[i] = 0; /* remove grain */
        goto next_grain;
    }
}
```

For a single falling grain test: when `is_cyclic = 0`, the grain simply deactivates when it exits the bottom of the canvas. For cyclic levels (the `is_cyclic` flag), it wraps back to the top — sugar never leaves the screen.

### Step 10: Trace a single grain through one frame

Starting conditions: `x=320, y=0, vx=0, vy=40` (initial downward nudge from emitter), `dt=0.016 s`.

```
Step A: apply gravity
  vy = 40 + 280 * 0.016 = 40 + 4.48 = 44.48 px/s

Step B: compute total displacement
  total_dy = 44.48 * 0.016 = 0.712 px
  steps = (int)(0 + 0.712) + 1 = 1

Step C: sub-step (1 step)
  sdy = 0.712
  ny  = 0 + 0.712 = 0.712
  iy  = (int)0.712 = 0     ← still row 0!

Step D: IS_SOLID(320, 0)? — no (empty canvas)
  y = 0.712  ← grain moved 0.712 pixels down (sub-pixel)

After 13 frames at ~4.5 px/s:  y ≈ 13 * 0.712 = 9.3 → row 9
After 1 second at avg 180 px/s: y ≈ 180 px down from start
```

Without float positions, the grain would stay on row 0 for the first frame (and many more), never visibly moving until enough velocity accumulated to exceed 1 px/frame.

---

## Common mistakes to prevent

1. **Using `GRAVITY` as negative (Y-UP intuition).**
   In this engine, positive `vy` means moving downward. `GRAVITY = +280.0f` is correct. Writing `vy -= GRAVITY * dt` makes grains fly upward.

2. **Integer velocity accumulation.**
   `vyi += (int)(GRAVITY * dt)` rounds away small increments. At low speed or high frame rate, the grain never moves. Always use `float` for velocity.

3. **Missing the `dt` cap.**
   Without `if (dt > 0.1f) dt = 0.1f`, pausing in the debugger causes a physics explosion the moment you resume. Add the cap in both the game loop and `game_update`.

4. **Skipping sub-step integration.**
   A single-step `y += vy * dt` with `vy = 600 px/s` and `dt = 0.016 s` moves the grain 9.6 pixels in one frame — potentially tunneling through 1-pixel-thick drawn lines. The sub-step loop is not optional.

5. **Applying the `(int)` cast to a negative float.**
   `(int)(-0.1f)` is `0` in C, not `-1`. A grain at `y = -0.1f` would write to row 0 rather than being caught by the `iy < 0` check. Ensure grains are always clamped to `y >= 0` when they exit the top boundary.

6. **Forgetting `goto next_grain` after deactivating.**
   After `p->active[i] = 0`, the code must jump past the rest of that grain's update (the settle check, the s_occ update). Without the `goto`, the deactivated grain's position is still used in `s_occ`, blocking other grains.

---

## What to run

```bash
./build-dev.sh
./sugar_x11
```

Start any level. Watch the sugar pour from the emitter nozzle — grains should fall smoothly, accelerating as they drop. Try pressing G to flip gravity (if the level supports it) — grains should immediately arc upward instead.

To see the dt cap in action:
```bash
./build-dev.sh -d
./sugar_x11_dbg
# Attach a debugger, pause for ~5 seconds, resume
# Grains should snap to a slightly lower position rather than teleporting off-screen
```

---

## Summary

A falling grain is pure Euler integration: accumulate `vy += GRAVITY * dt` every frame, then advance `y += vy * dt`. The Y-DOWN coordinate system means `GRAVITY = +280.0f` — larger `y` means lower on screen, matching the `pixels[y * width + x]` layout of both `LineBitmap` and `GameBackbuffer` without any coordinate flip. `dt` must always be capped at `0.1 s` to prevent debugger-pause physics explosions. Positions are stored as `float` so sub-pixel velocities accumulate correctly — a grain starting from rest does not visibly teleport from row to row. The sub-step integration loop further prevents tunneling by splitting large displacements into at most 1-pixel increments, ensuring every pixel of a drawn line is a valid collision point regardless of grain speed.
