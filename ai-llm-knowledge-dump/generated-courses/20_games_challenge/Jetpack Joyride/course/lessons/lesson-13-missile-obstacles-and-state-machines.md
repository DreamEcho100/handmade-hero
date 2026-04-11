# Lesson 13 -- Missile Obstacles and State Machines

## Observable Outcome
Missile Block 1 fires 5 missiles one at a time at 0.75-second intervals. Each missile appears as a warning icon at the right edge of the screen, then transitions to a fast-moving projectile flying left at 250 units/second, then explodes in a brief boom animation before deactivating. Missile Block 2 fires 2 tracking missiles 4 seconds apart -- these follow the player's Y position during the warning phase. Missiles collide with the player only during the FIRE state.

## New Concepts (max 3)
1. Three-state finite state machine: WARNING -> FIRE -> BOOM
2. Sequential spawning via missile_timer (staggered entity creation)
3. Spritesheet frame dimensions: MagicMissile.png 75x19 (3x25x19), Warning.png 48x24 (2x24x24), boom.png 95x19 (5x19x19)

## Prerequisites
- Lesson 12 (obstacle spawning and ObstacleManager)
- Lesson 10 (AABB collision)
- Lesson 08 (SpriteAnimation and anim_update)

## Background

Missiles are the first obstacle with a state machine. Unlike daggers which are always dangerous and always visible, missiles progress through three distinct phases: WARNING (visible but harmless, alerts the player), FIRE (moving and lethal), and BOOM (explosion visual, no collision). This pattern -- entity behavior driven by an enum state and a countdown timer -- appears throughout the game.

The original Godot game spawns missiles sequentially, not all at once. Missile Block 1 fires from 5 fixed Y positions (31, 58, 85, 112, 139 -- evenly spaced from top to bottom), one every 0.75 seconds. Missile Block 2 fires only 2 tracking missiles, spaced 4 seconds apart. Tracking missiles lock to the player's Y position during the WARNING phase, then freeze their Y and fire horizontally.

A key physics detail: missiles use `move_and_collide(Vector2(speed * delta, 0))` in the original GDScript. Unlike the player's `move_and_slide` which produces a double-delta model (position += speed * dt * dt), `move_and_collide` takes a displacement vector directly, so it is a single-delta model (position += speed * dt). This is why MISSILE_SPEED is -250 -- a modest value that produces fast visual movement with single dt integration.

## Code Walkthrough

### Step 1: Missile constants

Define speeds, timing, and hitbox dimensions:

```c
#define MISSILE_SPEED -250.0f
#define MISSILE_WARNING_DURATION 3.0f
#define MISSILE_HITBOX_W 6.0f   /* CapsuleShape2D radius=3, height=17 */
#define MISSILE_HITBOX_H 17.0f
#define MISSILE_BLOCK_1_INTERVAL 0.75f
#define MISSILE_BLOCK_2_INTERVAL 4.0f
```

The hitbox is narrow (6x17) because the original uses a CapsuleShape2D oriented vertically. Missiles fly horizontally but their collision shape is tall and thin.

Fixed Y positions for Missile Block 1:

```c
static const float MISSILE_BLOCK_1_Y[] = {31.0f, 58.0f, 85.0f, 112.0f, 139.0f};
#define MISSILE_BLOCK_1_COUNT 5
```

### Step 2: Spritesheet setup in `obstacles_init`

Initialize the three missile-related spritesheets:

```c
/* MagicMissile.png: 75x19 = 3 frames of 25x19 */
if (mgr->sprite_missile.pixels) {
  spritesheet_init(&mgr->sheet_missile, mgr->sprite_missile, 25, 19);
}

/* Warning.png: 48x24 = 2 frames of 24x24 */
if (mgr->sprite_warning.pixels) {
  spritesheet_init(&mgr->sheet_warning, mgr->sprite_warning, 24, 24);
}

/* boom.png: 95x19 = 5 frames of 19x19 (square frames) */
if (mgr->sprite_boom.pixels) {
  int bh = mgr->sprite_boom.height;
  spritesheet_init(&mgr->sheet_boom, mgr->sprite_boom, bh, bh);
}
```

The boom spritesheet uses the height as the frame width (19x19 square frames) because the atlas is a horizontal strip.

### Step 3: Block-level spawn -- metadata only

Missile blocks do NOT create all entities at spawn time. They set up metadata for sequential spawning:

```c
static void spawn_missile_block_1(ObstacleBlock *block, float block_x) {
  (void)block_x;
  block->entity_count = 0;
  block->next_missile_idx = 0;
  block->missile_timer = MISSILE_BLOCK_1_INTERVAL;
}
```

Missile Block 2 creates the first tracking missile immediately:

```c
static void spawn_missile_block_2(ObstacleBlock *block, float block_x,
                                   float player_y) {
  block->entity_count = 1;
  ObstacleEntity *e = &block->entities[0];
  memset(e, 0, sizeof(*e));
  e->x = block_x + 293.0f;
  e->y = player_y + (float)PLAYER_H * 0.5f;
  e->state = MISSILE_WARNING;
  e->timer = MISSILE_WARNING_DURATION;
  e->tracking = 1;
  e->active = 1;
  e->hitbox = aabb_make(e->x, e->y - MISSILE_HITBOX_H * 0.5f,
                         MISSILE_HITBOX_W, MISSILE_HITBOX_H);
  block->next_missile_idx = 1;
  block->missile_timer = MISSILE_BLOCK_2_INTERVAL;
}
```

### Step 4: Sequential spawning in `obstacles_update`

The update loop decrements the missile_timer. When it reaches zero, spawn the next missile:

```c
if (block->type == OBS_MISSILE_BLOCK_1) {
  block->missile_timer -= dt;
  if (block->missile_timer <= 0.0f &&
      block->next_missile_idx < MISSILE_BLOCK_1_COUNT) {
    spawn_one_missile(block, MISSILE_BLOCK_1_Y[block->next_missile_idx],
                      player_x, 0);
    block->next_missile_idx++;
    block->missile_timer = (block->next_missile_idx < MISSILE_BLOCK_1_COUNT)
                               ? MISSILE_BLOCK_1_INTERVAL
                               : 12.0f;
  }
}
```

The `spawn_one_missile` helper creates a single entity at the given Y position:

```c
static void spawn_one_missile(ObstacleBlock *block, float y, float player_x,
                               int tracking) {
  if (block->entity_count >= MAX_ENTITIES_PER_BLOCK) return;
  ObstacleEntity *e = &block->entities[block->entity_count];
  memset(e, 0, sizeof(*e));
  e->x = player_x + 200.0f;  /* Right edge of screen */
  e->y = y;
  e->state = MISSILE_WARNING;
  e->timer = MISSILE_WARNING_DURATION;
  e->tracking = tracking;
  e->active = 1;
  e->hitbox = aabb_make(e->x, e->y - MISSILE_HITBOX_H * 0.5f,
                         MISSILE_HITBOX_W, MISSILE_HITBOX_H);
  block->entity_count++;
}
```

### Step 5: Per-entity state machine in `update_missile`

The three-state machine drives each missile's behavior:

```c
static void update_missile(ObstacleEntity *e, float player_x, float player_y,
                            float dt) {
  switch (e->state) {
  case MISSILE_WARNING:
    e->timer -= dt;
    e->x = player_x + 200.0f;
    if (e->tracking) {
      e->y = player_y + (float)PLAYER_H * 0.5f;
    }
    if (e->timer <= 0.0f) {
      e->state = MISSILE_FIRE;
    }
    break;

  case MISSILE_FIRE:
    e->x += MISSILE_SPEED * dt;  /* Single dt -- move_and_collide model */
    e->hitbox = aabb_make(e->x, e->y - MISSILE_HITBOX_H * 0.5f,
                           MISSILE_HITBOX_W, MISSILE_HITBOX_H);
    if (e->x < player_x - 100.0f) {
      e->state = MISSILE_BOOM;
      e->timer = 0.5f;
    }
    break;

  case MISSILE_BOOM:
    e->timer -= dt;
    if (e->timer <= 0.0f) {
      e->active = 0;
    }
    break;
  }
}
```

During WARNING, the missile sticks to the right edge of the visible screen (player_x + 200). Tracking missiles also follow player Y. During FIRE, the missile moves left at a constant speed using single-dt integration. During BOOM, it shows the explosion for 0.5 seconds then deactivates.

### Step 6: Collision check -- FIRE state only

Missiles only damage the player during FIRE:

```c
if ((block->type == OBS_MISSILE_BLOCK_1 ||
     block->type == OBS_MISSILE_BLOCK_2) &&
    e->state == MISSILE_FIRE) {
  if (aabb_overlap(player_hitbox, e->hitbox))
    return 1;
}
```

### Step 7: Render with state-dependent sprites

Each state draws a different sprite:

```c
if (e->state == MISSILE_WARNING) {
  SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_warning, 0);
  sprite_blit(bb, &mgr->sprite_warning, rect, sx, sy);
} else if (e->state == MISSILE_FIRE) {
  SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_missile, 0);
  sprite_blit(bb, &mgr->sprite_missile, rect, sx, sy);
} else if (e->state == MISSILE_BOOM) {
  SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_boom, 0);
  sprite_blit(bb, &mgr->sprite_boom, rect, sx, sy);
}
```

## Common Mistakes

1. **Using dt*dt for missile movement.** Missiles use `move_and_collide` in the original, which takes a displacement (single dt), not `move_and_slide` which produces double-dt. Using dt*dt would make missiles move absurdly slowly at 60fps.

2. **Spawning all 5 missiles at once.** The original fires them sequentially with a timer. Spawning all at once removes the dramatic tension and makes them trivial to dodge (they all arrive simultaneously).

3. **Forgetting to deactivate the block after all missiles finish.** Without cleanup, the block slot stays occupied forever, eventually filling the 4-slot pool and blocking new obstacles from spawning.

4. **Not updating the hitbox position during FIRE.** The hitbox must track the missile's moving X position. If you only set it at spawn time, the collision check uses the stale spawn position.

5. **Letting tracking missiles track during FIRE.** Tracking only applies during WARNING. Once the missile fires, its Y is frozen -- otherwise it becomes an unavoidable homing missile.
