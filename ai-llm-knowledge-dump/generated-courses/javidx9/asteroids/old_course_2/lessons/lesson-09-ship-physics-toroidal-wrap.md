# Lesson 09 — Ship Physics and Toroidal Wrap

## What you'll learn
- Thrust, friction, and speed cap in game units
- Y-up sign convention: why `+cos` for dy thrust
- `fmodf` toroidal wrap for all three object types
- The game loop order: input → update → render

---

## Euler integration

Each frame, positions are advanced by velocity × dt:

```c
obj->x += obj->dx * dt;
obj->y += obj->dy * dt;
```

`dt` (delta time) is the elapsed seconds since the last frame (~0.0167s at 60fps).
Using dt makes physics frame-rate independent: an object at 6 units/s moves
the same world distance per second regardless of frame rate.

---

## Ship rotation

```c
// Rotate left/right (held key)
if (input->left.ended_down)
    state->player.angle -= SHIP_TURN_SPEED * dt;  // CCW in angle space = left turn
if (input->right.ended_down)
    state->player.angle += SHIP_TURN_SPEED * dt;  // CW in angle space = right turn
```

`SHIP_TURN_SPEED = 3.5` radians/second.  At 60fps, each frame rotates
`3.5 / 60 ≈ 0.058` radians (~3.3°).

---

## Thrust — the y-up sign

In y-up Cartesian space, `angle=0` means the nose points toward positive y.

With `sa = sin(angle)` and `ca = cos(angle)`:
- `angle=0` → `sa=0, ca=1` → thrust applies force in `(0, +1)` direction = up ✓
- `angle=π/2` → `sa=1, ca=0` → thrust applies force in `(+1, 0)` direction = right ✓

```c
float sa = sinf(state->player.angle);
float ca = cosf(state->player.angle);

if (input->up.ended_down) {
    state->player.dx +=  sa * SHIP_THRUST_ACCEL * dt;
    state->player.dy += +ca * SHIP_THRUST_ACCEL * dt;  // ← +ca, not -ca!
}
```

**The common mistake:** Old y-down code uses `-cos` for dy (because screen y
increases downward, so "forward" in angle=0 means decreasing y).  In y-up,
forward at angle=0 means increasing y → `+ca`.

---

## Friction and speed cap

```c
// Friction: exponential decay (applied every frame)
state->player.dx *= SHIP_FRICTION;  // 0.97 per frame → ~16% drop per second
state->player.dy *= SHIP_FRICTION;

// Speed cap
float speed = sqrtf(state->player.dx * state->player.dx +
                    state->player.dy * state->player.dy);
if (speed > SHIP_MAX_SPEED) {
    float s = SHIP_MAX_SPEED / speed;
    state->player.dx *= s;
    state->player.dy *= s;
}
```

`SHIP_FRICTION = 0.97` per frame.  Over 60 frames (1 second):
`0.97^60 ≈ 0.161` — the ship loses ~84% of its speed per second when
thrust is released.

`SHIP_MAX_SPEED = 6.875` units/s (was 220 px/s ÷ 32).

---

## Toroidal wrap

Asteroids has a toroidal world: objects that leave one edge reappear on
the opposite side.

```c
// Formula: fmodf(x + WIDTH, WIDTH) handles both positive and negative overflows
state->player.x = fmodf(state->player.x + GAME_UNITS_W, GAME_UNITS_W);
state->player.y = fmodf(state->player.y + GAME_UNITS_H, GAME_UNITS_H);
```

Adding `GAME_UNITS_W` before the `fmodf` ensures the result is always positive
even if `x` went slightly negative (e.g., `x = -0.1`).

Applied to all three object types each frame:

```c
// Apply to player
state->player.x = fmodf(state->player.x + GAME_UNITS_W, GAME_UNITS_W);
state->player.y = fmodf(state->player.y + GAME_UNITS_H, GAME_UNITS_H);

// Apply to all active asteroids
for (int i = 0; i < state->asteroid_count; i++) {
    if (!state->asteroids[i].active) continue;
    state->asteroids[i].x = fmodf(state->asteroids[i].x + GAME_UNITS_W, GAME_UNITS_W);
    state->asteroids[i].y = fmodf(state->asteroids[i].y + GAME_UNITS_H, GAME_UNITS_H);
}

// Apply to all active bullets
for (int i = 0; i < state->bullet_count; i++) {
    if (!state->bullets[i].active) continue;
    state->bullets[i].x = fmodf(state->bullets[i].x + GAME_UNITS_W, GAME_UNITS_W);
    state->bullets[i].y = fmodf(state->bullets[i].y + GAME_UNITS_H, GAME_UNITS_H);
}
```

---

## Dt clamping

```c
// In the platform's main loop
float dt = GetFrameTime();
if (dt > 0.066f) dt = 0.066f;  // cap at ~4 frames (~15fps)
if (dt < 0.0f)   dt = 0.0f;
```

Without clamping, if the window is minimised for 10 seconds and then
restored, `dt = 10.0` would teleport the ship across the world.

---

## Key takeaways

1. Euler integration: `pos += vel * dt` — frame-rate independent.
2. Y-up thrust: `dy += +ca * SHIP_THRUST_ACCEL * dt` (not `-ca`).
3. `sinf(angle)` drives dx, `cosf(angle)` drives dy at angle=0 → points up.
4. Friction `0.97^60 ≈ 0.16` — ship bleeds 84% speed per second.
5. Toroidal wrap: `fmodf(x + GAME_UNITS_W, GAME_UNITS_W)` — always positive.
6. Clamp dt to `0.066s` to prevent teleportation after pause/resize.
