# Lesson 17 -- Shield Pickup and Damage Pipeline

## Observable Outcome
A bouncing shield pickup appears ahead of the player every 20-40 seconds (10-30 seconds for subsequent spawns). The shield oscillates vertically between Y=59 and Y=109 while drifting forward at half the player's speed. Collecting the shield plays CLIP_SHIELD and grants the player one hit of protection. When the shielded player touches an obstacle, the shield breaks with blue particles and a POP sound -- the player survives. Without a shield, the same hit triggers the death sequence: the player falls, plays the death animation, and the game transitions to GAME_PHASE_GAMEOVER.

## New Concepts (max 3)
1. ShieldPickup bouncing motion (dt*dt vertical, dt*dt horizontal at half speed)
2. Damage pipeline: player_hurt -> shield check -> shield break OR player_die
3. Timed spawning with randomized intervals

## Prerequisites
- Lesson 15 (pellet collection pattern, AABB overlap for pickups)
- Lesson 10 (AABB collision)
- Lesson 08 (SpriteAnimation for shield sprite)

## Background

The shield is the only defensive pickup in the game. It absorbs exactly one hit, then breaks. This creates a risk/reward dynamic: the player must fly to the shield's position (which may be near obstacles) to collect it, but the insurance of surviving one mistake is valuable.

The ShieldPickup entity bounces vertically between two Y bounds while moving forward at half the player's speed. The vertical motion uses the same dt*dt integration model as the player physics (consistent with the Godot move_and_slide origin). The bounce is not physics-based -- it is a simple direction toggle when the entity reaches either bound.

The damage pipeline in `player_hurt` follows a clear priority chain:

1. Check invulnerability: if `hurted` is already true (within the 0.5s invulnerability window), ignore the hit.
2. Check shield: if `has_shield` is true, consume the shield (set to 0), trigger blue particles and POP sound, and return 0 (survived).
3. No shield: call `player_die`, trigger red particles and FALL sound, and return 1 (died).

This separation means the game loop does not need to know about shields. It calls `player_hurt` and gets back a boolean: did the player die? The sound and particle effects differ based on which branch was taken.

The shield spawn timer uses randomized intervals to prevent predictability. The first shield appears between 20-40 seconds into the run. Subsequent shields spawn 10-30 seconds after the previous one was collected or despawned.

## Code Walkthrough

### Step 1: ShieldPickup struct in `game/types.h`

```c
typedef struct {
  float x, y;
  float vy;            /* Bounce velocity */
  int active;
  int bounce_dir;      /* 1 = down, -1 = up */
  AABB hitbox;
  SpriteAnimation anim;
} ShieldPickup;

#define MAX_SHIELD_PICKUPS 2
```

Two slots allow for edge cases where a shield spawns before the previous one despawns.

### Step 2: Shield constants in `obstacles.c`

```c
#define SHIELD_SPAWN_OFFSET_X 320.0f
#define SHIELD_SPAWN_Y 84.0f
#define SHIELD_BOUNCE_MIN_Y 59.0f
#define SHIELD_BOUNCE_MAX_Y 109.0f
```

The spawn offset places the shield one full screen-width ahead of the player. The bounce range (59-109) keeps the shield in the middle band of the playfield.

### Step 3: Spritesheet setup

ShieldPickup.png is 32x18, containing 2 frames of 16x18:

```c
/* ShieldPickup.png: 32x18 = 2 frames of 16x18 */
if (mgr->sprite_shield.pixels) {
  spritesheet_init(&mgr->sheet_shield, mgr->sprite_shield, 16, 18);
}
```

### Step 4: Shield spawn timer

In `obstacles_init`, set the first spawn timer:

```c
mgr->shield_timer = rng_float_range(&mgr->rng, 20.0f, 40.0f);
```

In `obstacles_update`, count down and spawn when ready:

```c
mgr->shield_timer -= dt;
if (mgr->shield_timer <= 0.0f) {
  mgr->shield_timer = rng_float_range(&mgr->rng, 10.0f, 30.0f);

  for (int i = 0; i < MAX_SHIELD_PICKUPS; i++) {
    if (!mgr->shields[i].active) {
      ShieldPickup *s = &mgr->shields[i];
      memset(s, 0, sizeof(*s));
      s->x = player_x + SHIELD_SPAWN_OFFSET_X;
      s->y = SHIELD_SPAWN_Y;
      s->vy = -200.0f;  /* Initial upward bounce */
      s->bounce_dir = -1;
      s->active = 1;
      s->hitbox = aabb_make(s->x, s->y, 16.0f, 18.0f);
      break;
    }
  }
}
```

The initial vy=-200 sends the shield upward. When it hits SHIELD_BOUNCE_MIN_Y, the velocity flips to +200 (downward).

### Step 5: Shield update -- bounce and drift

The bouncing motion uses dt*dt integration matching the player physics model. Horizontal drift keeps the shield moving forward at half the player's speed so it does not immediately scroll off-screen:

```c
for (int i = 0; i < MAX_SHIELD_PICKUPS; i++) {
  ShieldPickup *s = &mgr->shields[i];
  if (!s->active) continue;

  /* Vertical bounce -- same dt*dt model as player */
  s->y += s->vy * dt * dt;
  if (s->y <= SHIELD_BOUNCE_MIN_Y) {
    s->y = SHIELD_BOUNCE_MIN_Y;
    s->vy = 200.0f;
  } else if (s->y >= SHIELD_BOUNCE_MAX_Y) {
    s->y = SHIELD_BOUNCE_MAX_Y;
    s->vy = -200.0f;
  }

  /* Horizontal drift at half player speed */
  s->x += player_speed * 0.5f * dt * dt;

  s->hitbox = aabb_make(s->x, s->y, 16.0f, 18.0f);

  /* Despawn if far behind */
  if (s->x < player_x - 400.0f) {
    s->active = 0;
  }
}
```

Because the shield moves at half speed and the player moves at full speed, the player will eventually catch up to and pass the shield. If the player misses it, the shield scrolls behind and despawns.

### Step 6: Shield collection

`obstacles_collect_shield` checks AABB overlap and deactivates the pickup:

```c
int obstacles_collect_shield(ObstacleManager *mgr, AABB player_hitbox) {
  for (int i = 0; i < MAX_SHIELD_PICKUPS; i++) {
    ShieldPickup *s = &mgr->shields[i];
    if (!s->active) continue;
    if (aabb_overlap(player_hitbox, s->hitbox)) {
      s->active = 0;
      return 1;
    }
  }
  return 0;
}
```

In `game_update`, collecting a shield either grants protection or awards bonus points if the player already has one:

```c
if (obstacles_collect_shield(&state->obstacles, state->player.hitbox)) {
  if (!state->player.has_shield) {
    state->player.has_shield = 1;
    game_audio_play(&state->audio, CLIP_SHIELD);
  } else {
    state->score += 50;  /* Bonus for redundant shield */
  }
}
```

### Step 7: The damage pipeline in `player_hurt`

This is the central damage function. It returns 1 if the player died, 0 if they survived:

```c
int player_hurt(Player *player) {
  if (player->hurted || player->state != PLAYER_STATE_NORMAL)
    return 0;

  player->hurted = 1;
  player->hurt_timer = 0.5f;  /* 500ms invulnerability */

  if (player->has_shield) {
    player->has_shield = 0;
    return 0;  /* Shield absorbed the hit */
  }

  player_die(player);
  return 1;  /* Player died */
}
```

The invulnerability timer prevents multiple hits within 0.5 seconds. This matters for dense obstacle fields where the player might overlap several entities simultaneously.

### Step 8: Death sequence

`player_die` transitions the player to the DEAD state:

```c
void player_die(Player *player) {
  player->state = PLAYER_STATE_DEAD;
  player->end_timer = 3.0f;
  player->is_on_floor = 0;  /* Start falling */
  anim_reset(&player->anim_dead);
}
```

During PLAYER_STATE_DEAD, the player falls with gravity and drifts forward at 75% speed. Once they hit the floor, the 4-frame death animation plays. After the end_timer expires (3 seconds), the state transitions to PLAYER_STATE_GAMEOVER.

### Step 9: Particle effects based on damage outcome

In `game_update`, the collision handler branches on the return value:

```c
if (obstacles_check_collision(&state->obstacles, state->player.hitbox)) {
  int died = player_hurt(&state->player);
  if (died) {
    game_audio_play(&state->audio, CLIP_FALL);
    particles_emit(&state->particles,
                   state->player.x + 10.0f,
                   state->player.y + 10.0f,
                   20, &DEATH_CONFIG);
  } else {
    game_audio_play(&state->audio, CLIP_POP);
    particles_emit(&state->particles,
                   state->player.x + 10.0f,
                   state->player.y + 10.0f,
                   12, &SHIELD_BREAK_CONFIG);
  }
}
```

DEATH_CONFIG emits 20 red particles in a full circle. SHIELD_BREAK_CONFIG emits 12 blue particles -- fewer and in a cooler color to signal "protected" rather than "destroyed".

### Step 10: Render the shield pickup

```c
for (int i = 0; i < MAX_SHIELD_PICKUPS; i++) {
  const ShieldPickup *s = &mgr->shields[i];
  if (!s->active) continue;

  int sx = (int)(s->x - camera_x);
  int sy = (int)s->y;

  if (mgr->sheet_shield.sprite.pixels) {
    SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_shield, 0);
    sprite_blit(bb, &mgr->sprite_shield, rect, sx, sy);
  } else {
    draw_rect(bb, (float)sx, (float)sy, 16.0f, 18.0f,
              0.2f, 0.6f, 1.0f, 0.9f);
  }
}
```

The fallback renders a blue rectangle -- the universal color signaling "shield" or "protection".

## Common Mistakes

1. **Checking `has_shield` before setting `hurted`.** The invulnerability flag must be set FIRST, before the shield check. Otherwise, two overlapping obstacles in the same frame would: first hit breaks the shield (hurted=1, has_shield=0), second hit is ignored by the hurted check. If you check shield first and reset hurted only after, the second hit kills the player.

2. **Using single-dt for shield movement when the player uses dt*dt.** The shield must use the same physics model as the player so their relative speeds are consistent. With single-dt, the shield would move much faster at 60fps and the player would never catch it.

3. **Spawning shields at a fixed interval.** A fixed 30-second timer makes shield appearances predictable. The randomized range (20-40 initial, 10-30 subsequent) keeps the player uncertain about when the next shield will appear.

4. **Not despawning shields behind the player.** Without the `x < player_x - 400` cleanup, old shields accumulate in the pool. With only 2 slots, this quickly prevents new shields from spawning.

5. **Applying damage during PLAYER_STATE_DEAD.** The `player->state != PLAYER_STATE_NORMAL` guard at the top of `player_hurt` prevents this. Without it, the falling death animation would be interrupted by additional obstacle collisions.
