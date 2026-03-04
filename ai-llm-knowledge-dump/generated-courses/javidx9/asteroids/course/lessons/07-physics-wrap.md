# Lesson 07 — Ship Physics + Toroidal Wrap

## What you'll build
Newtonian ship movement: thrust accumulates velocity, friction decays it,
and the ship wraps around screen edges seamlessly.  `draw_pixel_w`'s
toroidal modular arithmetic means wireframe lines also wrap.

---

## Core Concepts

### 1. Euler Integration

Euler integration updates position and velocity step-by-step each frame:

```c
// Thrust: add acceleration in the ship's facing direction
float ca = cosf(state->player.angle);
float sa = sinf(state->player.angle);

// Ship nose vertex is (0, -5) in local space.
// After rotation, nose offset = (+sa*5, -ca*5).
// Nose DIRECTION unit vector = (+sin(a), -cos(a)).
//   angle=0:    (0, -1)  → moves UP   ✓
//   angle=π/2:  (1,  0)  → moves RIGHT ✓
if (input->up.ended_down) {
    state->player.dx +=  sa * SHIP_THRUST_ACCEL * dt;  // ← +sa, NOT -sa
    state->player.dy += -ca * SHIP_THRUST_ACCEL * dt;
}

// Friction: bleed off velocity each frame
state->player.dx *= SHIP_FRICTION;  // e.g. 0.97
state->player.dy *= SHIP_FRICTION;

// Integrate position
state->player.x += state->player.dx * dt;
state->player.y += state->player.dy * dt;
```

> **Common sign bug:** using `-sa` for `dx` causes the ship to thrust in the
> mirror direction after any rotation.  The correct derivation:
> rotate nose `(0, -5)` by angle `a`:
> `rx = 0·cos(a) − (−5)·sin(a) = +5·sin(a)` → dx uses `+sa`.

JS equivalent:
```js
vx +=  Math.sin(angle) * THRUST_ACCEL * dt;
vy += -Math.cos(angle) * THRUST_ACCEL * dt;
vx *= FRICTION; vy *= FRICTION;
x += vx * dt; y += vy * dt;
```

**Why `*= dt`?**  
Multiplying by `dt` (seconds per frame, ~0.0167 for 60fps) makes movement
frame-rate independent.  If the game runs at 30fps, `dt ≈ 0.033`; at 60fps,
`dt ≈ 0.0167`.  The same `THRUST_ACCEL` constant works for both.

### 2. Speed Cap

Without a speed cap the ship accelerates without bound.  We cap it:

```c
float speed = sqrtf(state->player.dx * state->player.dx +
                    state->player.dy * state->player.dy);
if (speed > SHIP_MAX_SPEED) {
    float inv = SHIP_MAX_SPEED / speed;   // normalise then scale
    state->player.dx *= inv;
    state->player.dy *= inv;
}
```

`inv = max/speed` scales the velocity vector to have exactly `SHIP_MAX_SPEED`
magnitude while preserving direction.

### 3. Toroidal Position Wrap

When the ship exits one edge it should reappear from the opposite edge:

```c
// fmodf(x, m) gives the floating-point remainder
// Adding SCREEN_W before fmodf handles negative positions
state->player.x = fmodf(state->player.x + (float)SCREEN_W, (float)SCREEN_W);
state->player.y = fmodf(state->player.y + (float)SCREEN_H, (float)SCREEN_H);
```

**Why add `SCREEN_W` before `fmodf`?**  
`fmodf(-1, 512)` = `-1.0` (negative!) because C's `fmod` has the sign of
the dividend.  Adding `SCREEN_W` first ensures the argument is always
positive before taking the remainder.

### 4. Seamless Line Wrap via `draw_pixel_w`

`draw_line` uses `draw_pixel_w` for every pixel it writes.  `draw_pixel_w`
applies `((x % w) + w) % w` to wrap the coordinates.  This means:

- If the ship's nose is at x=511 and its left fin is at x=2, the line from
  511→2 will cross the right edge and appear from the left edge.
- This makes the ship appear to exist simultaneously on both edges while
  crossing them — correct retro Asteroids behaviour.

### 5. Asteroid Movement

Asteroids use identical physics but never thrust or steer:

```c
for (int i = 0; i < state->asteroid_count; i++) {
    SpaceObject *a = &state->asteroids[i];
    a->x     += a->dx    * dt;
    a->y     += a->dy    * dt;
    a->angle += 0.5f     * dt;  // constant rotation
    a->x = fmodf(a->x + SCREEN_W, SCREEN_W);
    a->y = fmodf(a->y + SCREEN_H, SCREEN_H);
}
```

---

## Platform: Frame Timing

### Raylib

```c
float dt = GetFrameTime();      // seconds since last frame (Raylib measures internally)
if (dt > 0.066f) dt = 0.066f;  // cap: prevents teleport after pause/focus loss
```

`SetTargetFPS(60)` makes Raylib sleep to maintain ~60fps automatically.

### X11

There is no built-in frame timer.  Use `clock_gettime(CLOCK_MONOTONIC, …)`:

```c
struct timespec ts_last, ts_now;
clock_gettime(CLOCK_MONOTONIC, &ts_last);

while (g_is_running) {
    clock_gettime(CLOCK_MONOTONIC, &ts_now);
    double dt = (ts_now.tv_sec  - ts_last.tv_sec) +
                (ts_now.tv_nsec - ts_last.tv_nsec) * 1e-9;
    ts_last = ts_now;
    if (dt > 0.066) dt = 0.066;
    // ...
}
```

`CLOCK_MONOTONIC` is the right choice because:
- It never goes backwards (unlike `CLOCK_REALTIME` which can jump on NTP sync)
- It measures elapsed time, not wall-clock time
- `tv_nsec` is nanoseconds (10⁻⁹ s), so `* 1e-9` converts to seconds

In X11 the loop runs as fast as possible; ALSA's blocking `snd_pcm_writei`
acts as the natural frame-rate limiter (~43–60fps depending on period size).

| Feature | Raylib | X11 |
|---------|--------|-----|
| Delta time source | `GetFrameTime()` | `clock_gettime(CLOCK_MONOTONIC)` |
| FPS limiter | `SetTargetFPS(60)` | ALSA blocking write (implicit) |
| dt unit | `float` seconds | `double` seconds (cast to float for game) |

---

## Constants to Tune

```c
#define SHIP_THRUST_ACCEL  100.0f   // pixels/s² while UP held
#define SHIP_MAX_SPEED     220.0f   // pixels/s cap
#define SHIP_FRICTION        0.97f  // velocity multiplier per frame
#define SHIP_TURN_SPEED      3.5f   // radians/s
```

Try changing `SHIP_FRICTION` from 0.97 to 0.5 or 0.999 to feel the difference.
At 0.999 the ship barely slows down; at 0.5 it stops almost instantly.

---

## Verify

1. UP key accelerates the ship forward (in the direction the nose points).
2. After rotating 90° clockwise, UP moves the ship to the **right** — not up.
3. Releasing UP, the ship coasts then slowly decelerates.
4. Ship exits right edge and reappears at left edge (and all four edges).
5. Wireframe lines crossing edges don't "snap" — they wrap pixel-by-pixel.

---

## Summary

| Concept | C | JS equivalent |
|---------|---|---------------|
| Euler integration | `v += a*dt; p += v*dt` | Same in JS game loops |
| Thrust direction | `dx += +sa*accel*dt`, `dy += -ca*accel*dt` | `dx += sin(a)*accel*dt` |
| Speed cap | `v *= maxSpeed / speed` | Normalise + scale |
| Toroidal float wrap | `fmodf(x + W, W)` | `((x % W) + W) % W` |
| Constant angular velocity | `a->angle += 0.5f * dt` | Same |
| X11 frame timing | `clock_gettime(CLOCK_MONOTONIC)` | `performance.now()` |
