# Lesson 06 — Vec2 + Rotation Matrix

## What you'll build
The `Vec2` type, a `draw_wireframe` function that rotates and scales any
polygon model, the ship wireframe model (`ship_model[3]`), and the first
visible ship that rotates in response to left/right keys.

---

## Core Concepts

### 1. `Vec2` — a Pair of Floats

```c
typedef struct { float x; float y; } Vec2;

// JS equivalent:
// interface Vec2 { x: number; y: number; }
// const v: Vec2 = { x: 1.0, y: 0.0 };
```

In C, `struct` initialisation with field names uses "designated initialisers"
(C99):
```c
Vec2 v = { .x = 1.0f, .y = -5.0f };   // C99
Vec2 v = { 1.0f, -5.0f };             // C89 (positional)
```

The `f` suffix on float literals (`1.0f`) prevents implicit promotion to
`double`, which would generate compiler warnings.

### 2. The 2×2 Rotation Matrix

To rotate point `(x, y)` by angle `θ` (counter-clockwise):

```
x' = x·cos θ − y·sin θ
y' = x·sin θ + y·cos θ
```

In C:
```c
float ca = cosf(angle);  // compute ONCE per object
float sa = sinf(angle);
// Apply to each vertex:
float rx = v.x * ca - v.y * sa;
float ry = v.x * sa + v.y * ca;
```

**Why compute `cos/sin` once and loop?**  
`cosf` and `sinf` are expensive (~100 ns each).  A ship model has 3 vertices;
an asteroid model has 20.  Pre-computing and reusing the two values reduces
20 expensive calls to just 2.

### 3. `draw_wireframe` — Transform + Draw

The wireframe function ties together Vec2, rotation, scale, and `draw_line`:

```c
static void draw_wireframe(AsteroidsBackbuffer *bb,
                           const Vec2 *model, int vert_count,
                           float cx, float cy,          // world position
                           float angle, float scale,    // transform
                           uint32_t color)
{
    float ca = cosf(angle), sa = sinf(angle);

    Vec2 world[64];  // transformed vertices, local stack allocation
    for (int i = 0; i < vert_count; i++) {
        float rx = model[i].x * ca - model[i].y * sa;
        float ry = model[i].x * sa + model[i].y * ca;
        world[i].x = cx + rx * scale;
        world[i].y = cy + ry * scale;
    }

    // Draw edges: connect each vertex to the next
    for (int i = 0; i < vert_count; i++) {
        int j = (i + 1) % vert_count;   // % vert_count closes the polygon
        draw_line(bb,
                  (int)world[i].x, (int)world[i].y,
                  (int)world[j].x, (int)world[j].y,
                  color);
    }
}
```

`(i + 1) % vert_count` wraps the last index back to 0, connecting the last
vertex to the first to close the polygon — the same as closing a path in Canvas 2D.

### 4. Ship Model

The ship is a triangle in **local space** (centred at origin, nose pointing up):

```c
// Three vertices, nose up (y is negative = up in screen space)
state->ship_model[0] = (Vec2){  0.0f, -5.0f };   // nose (top)
state->ship_model[1] = (Vec2){ -3.0f,  3.5f };   // left fin
state->ship_model[2] = (Vec2){  3.0f,  3.5f };   // right fin
```

This model has a radius of ~5 units.  When `draw_wireframe` is called with
`scale = 3.5f`, the ship appears ~17 pixels tall on screen.

**Why local space?**  
Defining the model centred at the origin means rotation always spins around
the ship's centre.  No need to subtract a pivot point first.

JS equivalent: defining a path relative to `(0,0)` before `ctx.translate(x,y)`.

### 5. `srand(time(NULL))` — Random Seeding

```c
srand((unsigned int)time(NULL));
// After this, rand() returns pseudo-random integers in [0, RAND_MAX]
```

Called once in `asteroids_init`.  `time(NULL)` returns seconds since the
Unix epoch — different each run, so asteroids get a new random shape each game.

JS equivalent: `Math.random()` is automatically seeded; you never call
`srand` in JavaScript.

---

## Implement It

In `asteroids_update`, add rotation and rotate SFX:
```c
if (input->left.ended_down)  state->player.angle -= SHIP_TURN_SPEED * dt;
if (input->right.ended_down) state->player.angle += SHIP_TURN_SPEED * dt;

// Re-trigger rotate tick while any rotation key is held
if ((input->left.ended_down || input->right.ended_down) &&
    !game_is_sound_playing(&state->audio, SOUND_ROTATE)) {
    game_play_sound(&state->audio, SOUND_ROTATE);
}
```

In `asteroids_render`, replace the 3-pixel placeholder with a wireframe:
```c
if (state->player.active) {
    draw_wireframe(bb,
                   state->ship_model, SHIP_VERTS,
                   state->player.x, state->player.y,
                   state->player.angle, 3.5f,
                   COLOR_CYAN);
}
```

---

## Thrust Direction — Common Sign Bug

The ship's nose vertex is at `(0, -5)` in local space (negative Y = up on screen).
After rotation by angle `a`, the nose lies at world offset:
```
rx = 0·cos(a) − (−5)·sin(a) = +5·sin(a)
ry = 0·sin(a) + (−5)·cos(a) = −5·cos(a)
```
So the **nose direction unit vector** is `(+sin(a), −cos(a))`.

At angle=0 (nose pointing straight up): `(0, −1)` → moves upward ✓  
At angle=π/2 (nose pointing right): `(1, 0)` → moves right ✓

The thrust code must use `+sa` for `dx`, NOT `−sa`:
```c
float ca = cosf(state->player.angle);
float sa = sinf(state->player.angle);
// CORRECT — nose direction (+sa, -ca):
state->player.dx +=  sa * SHIP_THRUST_ACCEL * dt;
state->player.dy += -ca * SHIP_THRUST_ACCEL * dt;

// WRONG (common mistake) — would thrust in the WRONG direction after rotation:
// state->player.dx += -sa * SHIP_THRUST_ACCEL * dt;  ← sign error on X!
```

The same applies to `add_bullet`: tip_x offset and bullet `dx` both use `+sa`.

---

## Verify

The cyan ship triangle rotates left/right at the screen centre.
After rotating 90° clockwise, pressing thrust should move the ship to the **right**.

---

## Summary

| Concept | C | JS equivalent |
|---------|---|---------------|
| 2D rotation | `x'=x·cos−y·sin` | `ctx.rotate(angle)` |
| Pre-compute trig | `float ca = cosf(a)` once | Cache `Math.cos(a)` |
| Local space model | vertices centred at `(0,0)` | Path before `ctx.translate` |
| Nose direction | `(+sin a, −cos a)` | Derived from rotated nose vertex |
| `srand/rand` | seed + use | `Math.random()` (auto-seeded) |
