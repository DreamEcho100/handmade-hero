# Lesson 20 -- Particle Effects

## Observable Outcome
Orange-yellow sparks trail downward from the player while the jetpack fires. A red burst of 20 particles explodes outward when the player dies. A blue sparkle of 12 particles radiates when the shield absorbs a hit. All particles fade out smoothly over their lifetime and drift downward under gravity.

## New Concepts (max 3)
1. Ring-buffer particle pool (fixed MAX_PARTICLES=128, write index wraps, oldest particles silently overwritten)
2. ParticleConfig for parameterized burst patterns (speed range, angle range, lifetime range, color)
3. Per-pixel alpha blending with the `((uint32_t)0xFF << 24)` pattern to avoid signed integer overflow

## Prerequisites
- Lesson 06 (bitmap font rendering, pixel-level backbuffer writes)
- Lesson 04 (draw_rect, understanding of RGBA pixel format)
- Lesson 18 (game events: death, shield break)

## Background

Particles are the cheapest way to add visual feedback to game events. A particle is a tiny colored square that spawns at a position, moves outward with some velocity, and fades out over its lifetime. By spawning many particles at once with randomized velocities and lifetimes, you create the illusion of an explosion, exhaust trail, or sparkle effect. The key insight is that each particle is independent -- no inter-particle forces, no complex physics. Just position, velocity, gravity, and a countdown timer.

Our particle system uses a ring buffer instead of a dynamic array. The pool holds MAX_PARTICLES (128) particles. A write index advances with each new emission, wrapping around when it hits the end. If we emit more particles than the pool can hold, the oldest particles are silently overwritten. This guarantees constant memory usage and O(1) emission cost. We never allocate or free memory at runtime.

The ParticleConfig struct parameterizes each burst pattern. Instead of hardcoding exhaust behavior in one function and death behavior in another, we define a config with speed range, angle range (in radians), lifetime range, size range, and color. The emission function reads this config and randomizes each particle's properties within the specified ranges. This data-driven approach means adding a new effect is just defining a new config struct -- no new code paths.

Alpha blending is the trickiest part of particle rendering. When a particle is half-faded, we need to blend its color with whatever is already in the backbuffer. The standard formula is: `out = src * alpha + dst * (1 - alpha)`. We compute this per-channel (R, G, B) and write the result back. The final pixel always gets full alpha (`0xFF << 24`) because the backbuffer represents opaque screen pixels -- the transparency was consumed by the blend operation.

## Code Walkthrough

### Step 1: Particle and ParticleConfig structs (particles.h)

```c
typedef struct {
  float x, y;          /* position */
  float vx, vy;        /* velocity */
  float life;          /* remaining life (seconds) */
  float max_life;      /* initial life (for alpha fade) */
  uint32_t color;      /* packed RGBA */
  float size;          /* pixel size */
} Particle;

#define MAX_PARTICLES 128

typedef struct {
  Particle pool[MAX_PARTICLES];
  int write_idx;       /* next write position (ring buffer) */
  int active_count;    /* number of alive particles */
} ParticleSystem;

typedef struct {
  float speed_min, speed_max;
  float angle_min, angle_max;   /* Emission cone (radians) */
  float life_min, life_max;
  float size_min, size_max;
  uint32_t color;
} ParticleConfig;
```

### Step 2: Three game-specific particle configs (game/main.c)

```c
static const ParticleConfig EXHAUST_CONFIG = {
  .speed_min = 30.0f, .speed_max = 80.0f,
  .angle_min = 2.5f,  .angle_max = 3.8f,   /* Downward cone (pi +/- 0.6) */
  .life_min = 0.1f,   .life_max = 0.3f,
  .size_min = 1.0f,   .size_max = 3.0f,
  .color = 0xFFFF8833, /* Orange-yellow */
};

static const ParticleConfig DEATH_CONFIG = {
  .speed_min = 40.0f,  .speed_max = 120.0f,
  .angle_min = 0.0f,   .angle_max = 6.28f,  /* Full circle */
  .life_min = 0.3f,    .life_max = 0.8f,
  .size_min = 2.0f,    .size_max = 4.0f,
  .color = 0xFFFF3333, /* Red */
};

static const ParticleConfig SHIELD_BREAK_CONFIG = {
  .speed_min = 20.0f,  .speed_max = 60.0f,
  .angle_min = 0.0f,   .angle_max = 6.28f,
  .life_min = 0.2f,    .life_max = 0.5f,
  .size_min = 1.0f,    .size_max = 3.0f,
  .color = 0xFF33BBFF, /* Blue */
};
```

The exhaust config restricts angles to a downward cone (approximately pi radians +/- 0.6) so sparks only shoot below the player. Death and shield break use the full circle (0 to 2*pi) for an omnidirectional burst.

### Step 3: Emission with ring buffer (particles.c)

```c
void particles_emit(ParticleSystem *ps, float x, float y, int count,
                    const ParticleConfig *config) {
  for (int i = 0; i < count; i++) {
    Particle *p = &ps->pool[ps->write_idx];

    float angle = randf(config->angle_min, config->angle_max);
    float speed = randf(config->speed_min, config->speed_max);

    p->x = x;
    p->y = y;
    p->vx = cosf(angle) * speed;
    p->vy = sinf(angle) * speed;
    p->life = randf(config->life_min, config->life_max);
    p->max_life = p->life;
    p->size = randf(config->size_min, config->size_max);
    p->color = config->color;

    ps->write_idx = (ps->write_idx + 1) % MAX_PARTICLES;
    if (ps->active_count < MAX_PARTICLES)
      ps->active_count++;
  }
}
```

The write index wraps with modulo. If the pool is full, the new particle overwrites the oldest one at `write_idx`. The active_count is capped at MAX_PARTICLES -- it can never exceed the pool size.

### Step 4: Update with gravity (particles.c)

```c
void particles_update(ParticleSystem *ps, float dt) {
  int alive = 0;
  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle *p = &ps->pool[i];
    if (p->life <= 0.0f) continue;

    p->life -= dt;
    if (p->life <= 0.0f) { p->life = 0.0f; continue; }

    p->x += p->vx * dt;
    p->y += p->vy * dt;
    p->vy += 50.0f * dt; /* Gravity */

    alive++;
  }
  ps->active_count = alive;
}
```

The particle gravity (50.0f) is much lighter than the player gravity (4500.0f) because particles should drift gently, not plummet. We iterate the entire pool, not just the "active" range, because the ring buffer may have alive particles scattered throughout.

### Step 5: Rendering with alpha blend (particles.c)

```c
void particles_draw(const ParticleSystem *ps, Backbuffer *bb, float camera_x) {
  int bb_pitch = bb->pitch / 4;

  for (int i = 0; i < MAX_PARTICLES; i++) {
    const Particle *p = &ps->pool[i];
    if (p->life <= 0.0f) continue;

    float alpha = p->life / p->max_life;
    if (alpha > 1.0f) alpha = 1.0f;

    int px = (int)(p->x - camera_x);
    int py = (int)p->y;
    int sz = (int)p->size;
    if (sz < 1) sz = 1;

    uint32_t c = p->color;
    unsigned int cr = (c >> 16) & 0xFF;
    unsigned int cg = (c >>  8) & 0xFF;
    unsigned int cb =  c        & 0xFF;
    unsigned int ca = (unsigned int)(alpha * 255.0f);

    for (int dy = 0; dy < sz; dy++) {
      int ry = py + dy;
      if (ry < 0 || ry >= bb->height) continue;
      for (int dx = 0; dx < sz; dx++) {
        int rx = px + dx;
        if (rx < 0 || rx >= bb->width) continue;

        if (ca >= 200) {
          bb->pixels[ry * bb_pitch + rx] =
              ((uint32_t)0xFF << 24) | (cr << 16) | (cg << 8) | cb;
        } else if (ca > 0) {
          uint32_t dst = bb->pixels[ry * bb_pitch + rx];
          unsigned int dr = (dst >> 16) & 0xFF;
          unsigned int dg = (dst >>  8) & 0xFF;
          unsigned int db =  dst        & 0xFF;
          unsigned int inv_a = 255 - ca;
          unsigned int or_ = (cr * ca + dr * inv_a) / 255;
          unsigned int og  = (cg * ca + dg * inv_a) / 255;
          unsigned int ob  = (cb * ca + db * inv_a) / 255;
          bb->pixels[ry * bb_pitch + rx] =
              ((uint32_t)0xFF << 24) | (or_ << 16) | (og << 8) | ob;
        }
      }
    }
  }
}
```

The alpha threshold at 200 is an optimization: when a particle is nearly opaque, we skip the blend and write directly. The `((uint32_t)0xFF << 24)` cast is deliberate -- without the `(uint32_t)` cast, `0xFF << 24` would be a signed left shift into the sign bit, which is undefined behavior in C. The cast ensures we shift in unsigned 32-bit arithmetic.

### Step 6: Triggering particles from game events (game/main.c)

Exhaust particles emit every frame while the jetpack fires:

```c
if (input->buttons.action.ended_down &&
    state->player.state == PLAYER_STATE_NORMAL &&
    !state->player.is_on_floor) {
  particles_emit(&state->particles,
                 state->player.x + 5.0f,
                 state->player.y + (float)PLAYER_H,
                 2, &EXHAUST_CONFIG);
}
```

Death and shield break particles are one-shot bursts:

```c
if (died) {
  particles_emit(&state->particles, state->player.x + 10.0f,
                 state->player.y + 10.0f, 20, &DEATH_CONFIG);
} else {
  /* Shield absorbed */
  particles_emit(&state->particles, state->player.x + 10.0f,
                 state->player.y + 10.0f, 12, &SHIELD_BREAK_CONFIG);
}
```

## Common Mistakes

**Using `int` instead of `unsigned int` for color channel arithmetic.** The alpha blend multiplications (e.g., `cr * ca`) can exceed 255*255 = 65025. This fits in `unsigned int` but may cause issues with signed overflow on narrow types. Always use `unsigned int` for intermediate color math.

**Forgetting camera_x when computing screen position.** Particles exist in world space but are drawn in screen space. Without `p->x - camera_x`, particles appear to scroll with the camera instead of staying fixed in the world. Exhaust sparks would slide backward relative to the player.

**Setting particle gravity equal to player gravity.** Player gravity is 4500 units/sec^2 -- tuned for the double-delta physics model. Particle gravity at that magnitude would make particles slam to the floor in a single frame. Particle gravity (50.0f) is intentionally two orders of magnitude lighter for a gentle drift effect.

**Not clamping alpha to [0, 1].** If `life` somehow exceeds `max_life` due to floating-point accumulation, `alpha` would exceed 1.0. The `if (alpha > 1.0f) alpha = 1.0f` guard prevents this from corrupting the color channels.
