# Lesson 26 -- Polish and Game Feel

## Observable Outcome
The complete Jetpack Joyride (SlimeEscape) game plays with full polish: background parallax scrolls at 30% of camera speed creating depth; jetpack exhaust particles trail the player; pitch-varied sound effects prevent repetition fatigue; death produces a dramatic red particle burst; the shield break produces a blue sparkle; scoring rewards aggressive play via near-miss bonuses; and the physics feel tight with constants faithfully adapted from the original Godot source (gravity=4500, thrust=-4500, virtAccel=12000, speed 2000-6000, accel=250).

## New Concepts (max 3)
1. Game feel as the sum of many small systems working together: physics tuning, audio variation, particle feedback, parallax depth
2. Parallax scrolling at a fractional camera rate for the illusion of background depth
3. Constant tuning methodology: transferring physics values from a reference implementation (Godot) to a C software renderer

## Prerequisites
- Lessons 18-19 (audio system with pitch variation)
- Lesson 20 (particle effects)
- Lesson 21-23 (death, HUD, restart flow)
- Lesson 05 (player physics, double-delta model)

## Background

"Game feel" is not a single feature -- it is the emergent result of every system in the game working together. A game with perfect physics but no audio feedback feels hollow. A game with rich audio but stiff physics feels unresponsive. Polish is the process of ensuring every system provides appropriate feedback for every game event, and that no event passes without acknowledgment.

This lesson examines the complete game holistically, showing how the systems from Lessons 18-25 combine with earlier systems (physics, rendering, collision) to create a polished experience. We focus on five areas: parallax depth, physics constants, audio juice, particle feedback, and the scoring feedback loop.

The physics constants in our game are not arbitrary. They are direct translations from the original SlimeEscape Godot 4.3 project. The Godot source uses `CharacterBody2D.velocity` which is in pixels/second. The GDScript sets `velocity.x = speed * delta` (not just `speed`), and `move_and_slide()` internally applies `position += velocity * delta`. This means the net displacement per frame is `speed * delta * delta` -- a double-delta model. Our C code replicates this exactly: `player->x += player->speed * dt * dt`. The constants (gravity=4500, jump_vel=-4500, virt_accel=12000, min_speed=2000, max_speed=6000, accel=250) transfer directly without scaling.

Parallax scrolling creates depth with zero additional art. Our background texture tiles seamlessly at 342 pixels wide. Instead of scrolling it at camera speed (1:1), we scroll at 30% (`BG_PARALLAX_FACTOR = 0.3f`). This makes the background appear farther away, creating a layered visual depth. The math is simple: `parallax_offset = camera.x * 0.3f`. The tile starting position wraps with modulo arithmetic so the background repeats infinitely.

## Code Walkthrough

### Step 1: Parallax background rendering (game/main.c)

```c
#define BG_TILE_W 342
#define BG_PARALLAX_FACTOR 0.3f

static void render_background(GameState *state, Backbuffer *bb) {
  if (!state->bg_sprite.pixels) {
    draw_rect(bb, 0.0f, 0.0f, (float)bb->width, (float)bb->height,
              0.078f, 0.047f, 0.110f, 1.0f);
    return;
  }

  float parallax_offset = state->camera.x * BG_PARALLAX_FACTOR;

  int tile_w = state->bg_sprite.width;
  int start_x = -((int)parallax_offset % tile_w);
  if (start_x > 0) start_x -= tile_w;

  SpriteRect full = {0, 0, state->bg_sprite.width, state->bg_sprite.height};
  for (int tx = start_x; tx < bb->width; tx += tile_w) {
    sprite_blit(bb, &state->bg_sprite, full, tx, 0);
  }
}
```

The starting tile position is computed as the negative of the parallax offset modulo the tile width. The `if (start_x > 0) start_x -= tile_w` correction ensures we always start rendering from a position left of the screen edge, preventing a gap on the left. The loop draws tiles until we cover the entire screen width.

### Step 2: Physics constants (types.h)

All physics constants are defined in one place with comments tracing back to the original source:

```c
/* Physics (from slimeTest.gd) */
#define PLAYER_MIN_SPEED   2000.0f  /* Initial horizontal speed */
#define PLAYER_MAX_SPEED   6000.0f  /* Maximum horizontal speed */
#define PLAYER_ACCEL       250.0f   /* Horizontal acceleration */
#define PLAYER_GRAVITY     4500.0f  /* Downward acceleration */
#define PLAYER_JUMP_VEL   -4500.0f  /* Upward velocity clamp */
#define PLAYER_VIRT_ACCEL  12000.0f /* Vertical acceleration rate */
```

The double-delta integration in player_update:

```c
player->x += player->speed * dt * dt;
player->y += player->virt_speed * dt * dt;
```

This is not a bug -- it is the faithful reproduction of the Godot physics model where `velocity = speed * delta` and `position += velocity * delta`, giving `position += speed * delta * delta`.

### Step 3: Audio juice -- pitch variation summary

Pitch variation makes repetitive sounds feel organic:

| Clip | Pitch Range | Effect |
|------|-------------|--------|
| Jump | 0.9 - 1.1 | Each jump sounds slightly different |
| Pellet | 1.0 - 1.2 | Collecting feels sparkly and varied |
| POP (shield break) | 0.4 - 0.6 | Deep, boomy destruction feel |
| MissileMove | 0.35 (fixed) | Ominous low rumble |
| MissileBoom | 0.3 (fixed) | Deep explosion impact |

Volume adjustments prevent quiet sounds from drowning and loud sounds from clipping:

| Clip | Volume | Rationale |
|------|--------|-----------|
| Fall | 0.1 | Death sound should not be jarring |
| Dagger | 0.1 | Many daggers at once would clip |
| Warning | 0.3 | Alert, not alarm |
| SigilCharge | 0.3 | Ambient tension |

### Step 4: Particle feedback for every key event

Every significant game event has an associated particle effect:

```c
/* Jetpack exhaust: 2 particles per frame while flying */
if (input->buttons.action.ended_down &&
    state->player.state == PLAYER_STATE_NORMAL &&
    !state->player.is_on_floor) {
  particles_emit(&state->particles,
                 state->player.x + 5.0f,
                 state->player.y + (float)PLAYER_H,
                 2, &EXHAUST_CONFIG);
}

/* Death: 20-particle red burst */
particles_emit(&state->particles, ..., 20, &DEATH_CONFIG);

/* Shield break: 12-particle blue sparkle */
particles_emit(&state->particles, ..., 12, &SHIELD_BREAK_CONFIG);
```

The exhaust emits from the bottom of the player sprite (`y + PLAYER_H`) to simulate thrust. Death and shield particles emit from the center of the player (`x + 10, y + 10` for a 20x20 sprite).

### Step 5: Scoring feedback loop

The scoring system rewards aggressive play:

```c
state->score = distance_score + state->pellet_count * 10 +
               state->near_miss_count * 50;
```

- **Distance** provides passive score growth -- just surviving earns points.
- **Pellets (10 pts each)** reward exploration and path planning through pellet fields.
- **Near-misses (50 pts each)** are the "skill multiplier" -- dodging obstacles within the near-miss margin (defined in collision.h's `aabb_near_miss`) earns 5x a pellet. This encourages playing dangerously close to obstacles rather than safely avoiding them.

### Step 6: Game over overlay as damage flash

The dark semi-transparent overlay in GAMEOVER serves double duty -- it is both the game over screen background and a visual "damage flash" that signals the end of the run:

```c
draw_rect(backbuffer, 0.0f, 0.0f,
          (float)backbuffer->width, (float)backbuffer->height,
          0.0f, 0.0f, 0.0f, 0.6f);
```

The 60% opacity (`alpha = 0.6f`) darkens the gameplay scene without fully obscuring it. The player can still see where they died and what killed them, which is important for learning.

### Step 7: The complete render pipeline

The render order creates visual layers that feel cohesive:

1. **Background** (parallax at 30%) -- the deepest layer
2. **Floor** (static band) -- the ground plane
3. **Obstacles** (camera-relative) -- the danger layer
4. **Player** (camera-relative) -- the focus
5. **Particles** (camera-relative) -- visual feedback on top of gameplay
6. **HUD** (screen-space) -- information layer
7. **Game over overlay** (screen-space) -- modal layer on top of everything

Each layer has a clear Z-order purpose. Particles above the player ensure exhaust and explosions are visible. HUD above particles ensures scores are always readable.

## Common Mistakes

**Tuning physics constants without understanding the integration model.** If you change from double-delta (`speed * dt * dt`) to single-delta (`speed * dt`), all constants need to be re-tuned. The original values (2000-6000 for speed) are calibrated for double-delta. Under single-delta, the player would move thousands of pixels per second and fly off-screen instantly.

**Setting parallax factor too high.** A factor of 1.0 means the background scrolls with the camera (no parallax effect). A factor of 0.0 means the background is completely static. Values above 1.0 make the background scroll faster than the foreground, which looks wrong (inverted parallax). The sweet spot for a single background layer is 0.2-0.4.

**Adding screen shake without capping the offset.** Screen shake (random camera offset that decays over time) is a powerful game feel tool, but unbounded offsets can push the viewport outside the rendered area, revealing blank pixels. Always clamp shake offset to a maximum of a few pixels.

**Forgetting to emit particles on shield break.** Without the blue sparkle, the shield absorb looks identical to "nothing happened." The player cannot distinguish between missing the obstacle and having the shield absorb it. Particle color codes the event: red = death, blue = shield saved you.

**Playing all sounds at full volume.** With 16 simultaneous voice slots, several daggers and a missile can fire at once. If all play at volume 1.0, the mixed output exceeds 1.0 and hard-clips. The per-clip volume adjustments (dagger 0.1, warning 0.3) and the master volume (0.6) work together to keep the mix clean.
