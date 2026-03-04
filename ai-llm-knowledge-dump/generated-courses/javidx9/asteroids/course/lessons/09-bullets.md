# Lesson 09 — Bullets

## What you'll build
`SpaceObject bullets[MAX_BULLETS]` pool, bullet spawn on FIRE "just pressed",
per-bullet lifetime decay, toroidal wrap, and `compact_pool` cleanup.
The ship fires white pixel bullets that travel in the facing direction.

---

## Core Concepts

### 1. `add_bullet` — Spawn from Ship Nose

```c
static int add_bullet(GameState *state) {
    if (state->bullet_count >= MAX_BULLETS) return 0;

    float ca = cosf(state->player.angle);
    float sa = sinf(state->player.angle);

    // Nose is at local (0, -5) → rotated: (-sa*5, -ca*5) offset from ship
    float tip_x = state->player.x + (-sa * 5.0f);
    float tip_y = state->player.y + (-ca * 5.0f);

    SpaceObject *b = &state->bullets[state->bullet_count++];
    b->x     = tip_x;
    b->y     = tip_y;
    b->dx    = -sa * BULLET_SPEED;   // velocity in nose direction
    b->dy    = -ca * BULLET_SPEED;
    b->size  = BULLET_LIFETIME;      // COURSE NOTE: we reuse `size` as lifetime
    b->active = 1;
    return 1;
}
```

**COURSE NOTE:** In the lesson stage we repurpose `b->size` as the bullet's
remaining lifetime (seconds).  Each frame we decrement it by `dt`; when it
reaches 0 we set `b->active = 0`.  In a larger codebase you'd add a
dedicated `lifetime` field — but reusing size keeps the struct minimal for
these lessons.

### 2. Fire Cooldown

Without a cooldown, holding FIRE fills the 8-bullet pool instantly.  We
add a minimum gap between shots:

```c
#define FIRE_COOLDOWN 0.15f   // seconds between shots

state->fire_timer -= dt;
if (state->fire_timer < 0.0f) state->fire_timer = 0.0f;

if (input->fire.half_transition_count > 0 && input->fire.ended_down
    && state->fire_timer <= 0.0f)
{
    add_bullet(state);
    state->fire_timer = FIRE_COOLDOWN;
}
```

Using `half_transition_count > 0` ensures we detect the key-down transition
(not just "is held"), preventing holding FIRE from rapid-firing at the
cooldown rate.

### 3. Bullet Update

```c
for (int i = 0; i < state->bullet_count; i++) {
    SpaceObject *b = &state->bullets[i];
    b->x    += b->dx * dt;
    b->y    += b->dy * dt;
    b->size -= dt;              // count down lifetime
    if (b->size <= 0.0f) b->active = 0;
    b->x = fmodf(b->x + SCREEN_W, SCREEN_W);
    b->y = fmodf(b->y + SCREEN_H, SCREEN_H);
}
```

### 4. Bullet Rendering

A bullet is a 2×2 cluster of pixels — big enough to see, small enough to
look like a laser dot:

```c
for (int i = 0; i < state->bullet_count; i++) {
    if (!state->bullets[i].active) continue;
    int bx = (int)state->bullets[i].x;
    int by = (int)state->bullets[i].y;
    draw_pixel_w(bb, bx,   by,   COLOR_YELLOW);
    draw_pixel_w(bb, bx+1, by,   COLOR_YELLOW);
    draw_pixel_w(bb, bx,   by+1, COLOR_YELLOW);
}
```

### 5. Compact After Each Frame

```c
compact_pool(state->bullets, &state->bullet_count);
```

Call this **after** the bullet update loop (not inside it).  If we removed
elements during the loop, `compact_pool`'s swap-with-tail would move unchecked
elements under the current index.

---

## Verify

1. Pressing SPACE spawns a yellow bullet that travels in the ship's facing direction.
2. Holding SPACE fires at the cooldown rate (~6-7 shots/second), not faster.
3. Bullets expire after ~1.5 seconds.
4. Bullets wrap around screen edges.
5. `bullet_count` never exceeds `MAX_BULLETS`.

---

## Summary

| Concept | C | JS equivalent |
|---------|---|---------------|
| Bullet spawn | `add_bullet` appends to pool | `bullets.push({ ... })` |
| Lifetime via size | `b->size -= dt; if ≤0 deactivate` | `b.lifetime -= dt` |
| Rate limiting | `fire_timer` cooldown | `lastFired + cooldown > Date.now()` |
| Compact | After update loop | `bullets = bullets.filter(b => b.active)` |
