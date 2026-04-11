# Lesson 08 — Vec2, Rotation, and Wireframe Rendering

## What you'll learn
- `Vec2` and basic 2D operations
- CW rotation formula for y-up coordinates
- Why ship model vertices are y-positive for "up"
- `draw_wireframe()` with RenderContext

---

## Vec2

```c
// utils/math.h
typedef struct { float x, y; } Vec2;

static inline Vec2 vec2_add   (Vec2 a, Vec2 b);
static inline Vec2 vec2_scale (Vec2 v, float s);
static inline float vec2_length(Vec2 v);
static inline Vec2 vec2_rotate (Vec2 v, float angle);
```

---

## Y-up coordinate convention and ship model

In `COORD_ORIGIN_LEFT_BOTTOM` (y-up), the screen top is at large y.
The ship's nose points "up" = positive world y.

```c
// asteroids_init — ship model (local space)
state->ship_model[0] = (Vec2){  0.0f, +5.0f };   // nose (up = +y)
state->ship_model[1] = (Vec2){ -3.0f, -3.5f };   // left fin
state->ship_model[2] = (Vec2){  3.0f, -3.5f };   // right fin
```

At scale `SHIP_RENDER_SCALE = 0.109375`:
- Nose tip:  `5.0 × 0.109375 = 0.547` units above ship centre
- Wing tips: `3.0 × 0.109375 = 0.328` units to each side

**Why not y-negative for the nose?**
Old code used y-down screen coordinates where "up" on screen = negative y.
In y-up, "up on screen" = positive y.  If we kept `(0, -5)` for the nose,
it would point downward on screen.

---

## CW rotation formula (y-up)

Standard 2D rotation matrix (CCW positive):
```
x' =  x*cos(θ) - y*sin(θ)
y' =  x*sin(θ) + y*cos(θ)
```

For "angle = 0 points up, positive = CW visual turn" in y-up:

```c
float ca = cosf(angle);
float sa = sinf(angle);

// Clockwise rotation (y-up, angle=0 → nose points up)
float rx = vx * ca + vy * sa;
float ry = -(vx * sa) + vy * ca;
```

Derivation: the standard CCW formula with `θ` negated (`sin(-θ) = -sin(θ)`)
gives clockwise rotation.  `angle=0` → `ca=1, sa=0` → vertex unchanged.
`angle=π/2` (90° CW) → right turn.

---

## draw_wireframe()

```c
// game/game.c (static helper)
static void draw_wireframe(Backbuffer *bb, const RenderContext *ctx,
                           const Vec2 *model, int vert_count,
                           float cx, float cy,
                           float angle, float scale,
                           uint32_t color) {
    float ca = cosf(angle);
    float sa = sinf(angle);

    // Transform model vertices → world space
    Vec2 world[64];  // stack array (vert_count ≤ 64)
    for (int i = 0; i < vert_count; i++) {
        // CW rotation (y-up): rx = x*cos + y*sin, ry = -x*sin + y*cos
        float rx = model[i].x * ca + model[i].y * sa;
        float ry = -(model[i].x * sa) + model[i].y * ca;
        world[i].x = cx + rx * scale;
        world[i].y = cy + ry * scale;
    }

    // Draw edges using RenderContext for world→pixel conversion
    for (int i = 0; i < vert_count; i++) {
        int j = (i + 1) % vert_count;
        int px0 = (int)world_x(ctx, world[i].x);
        int py0 = (int)world_y(ctx, world[i].y);
        int px1 = (int)world_x(ctx, world[j].x);
        int py1 = (int)world_y(ctx, world[j].y);
        draw_line(bb, px0, py0, px1, py1, color);
    }
}
```

`world_x/y()` from `render_explicit.h` handle the y-flip internally —
the game only works in y-up world space.

`draw_line` uses `draw_pixel_w` which wraps at screen edges, so asteroids
that straddle the toroidal boundary render correctly.

---

## Rendering the ship

```c
// In asteroids_render():
if (state->player.active) {
    draw_wireframe(bb, &world_ctx,
                   state->ship_model, SHIP_VERTS,
                   state->player.x, state->player.y,
                   state->player.angle, SHIP_RENDER_SCALE,
                   WF_CYAN);
}
```

---

## Rendering asteroids

```c
for (int i = 0; i < state->asteroid_count; i++) {
    const SpaceObject *a = &state->asteroids[i];
    if (!a->active) continue;
    draw_wireframe(bb, &world_ctx,
                   state->asteroid_model, ASTEROID_VERTS,
                   a->x, a->y,
                   a->angle, a->size,
                   WF_WHITE);
}
```

The asteroid's `size` field (0.625, 0.3125, or 0.15625 units) is passed
as the scale argument — large asteroids are large, small are small, using
the same model.

---

## Key takeaways

1. Ship model: nose at `(0, +5)` for y-up — positive y = up on screen.
2. CW rotation formula: `rx = x*ca + y*sa; ry = -(x*sa) + y*ca`.
3. `angle=0` means pointing straight up (positive y).
4. `draw_wireframe` takes `cx, cy, angle, scale` as individual floats — not a `SpaceObject *`.
5. Ship color = `WF_CYAN`; asteroid color = `WF_WHITE`.
6. `draw_wireframe` transforms model→world, then world→pixel via `world_x/y()`.
7. `draw_line` wraps at screen edges (toroidal) via `draw_pixel_w`.
8. Asteroid `size` doubles as draw scale and collision radius.
