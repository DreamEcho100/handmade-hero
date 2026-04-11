# Lesson 11 — Bullets

## What you'll learn
- `add_bullet()`: spawning from the ship's nose tip
- Y-up bullet velocity direction (`+ca` not `-ca`)
- Bullet lifetime using `size` as a shrinking radius
- Fire cooldown timer

---

## Bullet pool

```c
#define MAX_BULLETS 8  // maximum simultaneous bullets

// In GameState:
SpaceObject bullets[MAX_BULLETS];
int         bullet_count;
```

8 bullets is more than enough for Asteroids gameplay (fire rate is
limited to one shot every 0.15s = ~6.67/s, but bullets expire after 1.5s,
capping simultaneous bullets at ~10 — the pool covers it).

---

## add_bullet() — spawning from the nose

The bullet spawns at the ship's nose tip, not its centre.  The nose tip
is model vertex `(0, +5)` scaled by `SHIP_RENDER_SCALE`:

```c
static void add_bullet(GameState *state) {
    if (state->bullet_count >= MAX_BULLETS) return;

    float sa = sinf(state->player.angle);
    float ca = cosf(state->player.angle);

    // Nose tip in world space:
    // Model vertex (0, +5) rotated and scaled
    float tip_x = state->player.x + 5.0f * sa * SHIP_RENDER_SCALE;
    float tip_y = state->player.y + 5.0f * ca * SHIP_RENDER_SCALE;

    SpaceObject *b = &state->bullets[state->bullet_count++];
    b->x      = tip_x;
    b->y      = tip_y;
    b->dx     =  sa * BULLET_SPEED;    // x component of heading
    b->dy     = +ca * BULLET_SPEED;    // y component — +ca for y-up!
    b->angle  = state->player.angle;
    b->size   = BULLET_LIFETIME;       // size starts at 1.5 = full lifetime
    b->active = 1;
}
```

**Y-up sign:** `dy = +ca * BULLET_SPEED` (not `-ca`).  Reason is identical to
thrust: at `angle=0` the ship faces up (+y), so the bullet must fly in the
+y direction.  `cosf(0) = 1`, so `dy = +1 * BULLET_SPEED = +BULLET_SPEED`.

The bullet inherits the ship's heading but NOT its velocity.  Classic arcade
Asteroids ignores the ship's current speed for bullet velocity.

---

## Bullet lifetime via size decay

```c
#define BULLET_LIFETIME 1.5f  // seconds

// In asteroids_update():
for (int i = 0; i < state->bullet_count; i++) {
    SpaceObject *b = &state->bullets[i];
    if (!b->active) continue;

    b->size -= dt;          // size decays at 1.0 per second
    if (b->size <= 0.0f) {
        b->active = 0;       // mark for removal
        continue;
    }

    b->x += b->dx * dt;
    b->y += b->dy * dt;
    // wrap...
}
```

`size` starts at `BULLET_LIFETIME = 1.5`.  It decays by `dt` each frame.
When it reaches 0, the bullet is expired.  This is an elegant trick: size
doubles as a countdown timer, and if you want visible shrinking bullets
you can draw a small circle scaled to `b->size` directly.

---

## Rendering bullets

```c
for (int i = 0; i < state->bullet_count; i++) {
    SpaceObject *b = &state->bullets[i];
    if (!b->active) continue;

    // Draw as a small wireframe diamond (4 verts from draw_wireframe)
    // OR simply draw a 2×2 pixel at the bullet's world position:
    int px = (int)world_x(&world_ctx, b->x);
    int py = (int)world_y(&world_ctx, b->y);
    draw_pixel_w(bb, px, py, WF_WHITE);
    draw_pixel_w(bb, px+1, py, WF_WHITE);
    draw_pixel_w(bb, px, py+1, WF_WHITE);
    draw_pixel_w(bb, px+1, py+1, WF_WHITE);
}
```

A 2×2 cluster is visible without being distracting.

---

## Fire cooldown

```c
#define FIRE_COOLDOWN 0.15f  // seconds between shots

// Update cooldown timer each frame:
if (state->fire_timer > 0.0f)
    state->fire_timer -= dt;

// Fire: just-pressed + cooldown expired + alive
if (input->fire.ended_down &&
    input->fire.half_transition_count > 0 &&
    state->fire_timer <= 0.0f &&
    state->phase == PHASE_PLAYING)
{
    add_bullet(state);
    state->fire_timer = FIRE_COOLDOWN;
    game_play_sound(&state->audio, SOUND_FIRE);
}
```

`half_transition_count > 0` ensures one bullet per key press, not per frame.
Without it, a 60fps held-down key would fire 8 bullets in the first 0.13s.

---

## Key takeaways

1. Bullet spawns at the nose tip: `tip = player.pos + (0,5) * sa/ca * SHIP_RENDER_SCALE`.
2. `dy = +ca * BULLET_SPEED` in y-up — not `-ca`.
3. `size` = remaining lifetime (decays by dt/s); doubles as visual radius.
4. Pool compaction same as asteroids: swap-with-tail when `active=0`.
5. Fire cooldown + `half_transition_count > 0` = one shot per keypress.
