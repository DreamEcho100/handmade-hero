# Lesson 14 -- Sigil Obstacles and Beam Attacks

## Observable Outcome
Sigil Block 1 displays a single horizontal beam that charges for 2.5 seconds (thin pulsing line), fires for 3.0 seconds (full-width beam sprite spanning the screen), then powers down. Sigil Block 2 displays 5 sigils that fire in coordinated pairs: sigils 0+1 charge and fire first, then 2+3, then 4 fires alone. The beams are always screen-relative -- they lock to the camera center and stretch across the full viewport width. The player is damaged only during the FIRE state.

## New Concepts (max 3)
1. Four-state sigil machine: WAIT -> CHARGE -> FIRE -> POWERDOWN
2. Block-level fire_order coordinator for sequencing pairs
3. Screen-locked positioning: sigil X tracks camera_x + GAME_W/2

## Prerequisites
- Lesson 13 (missile state machines, sequential spawning)
- Lesson 12 (obstacle spawning)
- Lesson 03 (sprite_blit_alpha for semi-transparent charge effect)

## Background

Sigils are the most mechanically distinct obstacle in the game. Unlike daggers (static obstacles) and missiles (projectiles), sigils are screen-relative beams that fire horizontally across the entire viewport. The player cannot outrun them -- the only escape is to be at the right Y position when the beam fires.

The original Godot game uses a Sigil sprite that IS the beam itself. Sigil.png is 310x68, organized as 4 rows of 310x17 frames. Each frame is a full-width beam graphic. The sprite is rendered at x=0 spanning the screen width. SigilWait.png (310x34, 2 rows of 310x17) shows a thin charging line.

Sigil Block 1 is simple: one sigil waits briefly, charges for 2.5 seconds, fires for 3.0 seconds, then powers down. Sigil Block 2 is complex: 5 sigils at Y positions {31, 139, 58, 112, 85} coordinate via a `fire_order` state machine. The block progresses through 5 phases where pairs of sigils charge, fire, and power down in overlapping sequence. This creates a pattern where the player must dodge through gaps between sequentially-firing beams.

The key architectural insight is that sigil blocks use TWO levels of state machines. Each individual sigil entity has its own state (WAIT/CHARGE/FIRE/POWERDOWN). The block-level fire_order coordinator sets entity states at phase transitions. Sigil Block 1 manages its single entity directly. Sigil Block 2 overrides entity states from the block coordinator.

## Code Walkthrough

### Step 1: Sigil constants

```c
#define SIGIL_BLOCK_1_CHARGE_DURATION 2.5f
#define SIGIL_BLOCK_1_FIRE_DURATION   3.0f
#define SIGIL_BLOCK_2_CHARGE_DURATION 3.0f
#define SIGIL_BLOCK_2_FIRE_DURATION   3.0f
#define SIGIL_BEAM_W 300.0f
#define SIGIL_BEAM_H 8.0f
```

Y positions for Sigil Block 2's 5 beams:

```c
static const float SIGIL_BLOCK_2_Y[] = {31.0f, 139.0f, 58.0f, 112.0f, 85.0f};
#define SIGIL_BLOCK_2_COUNT 5
```

The Y order is not sequential -- it matches the original fire_order pattern where top and bottom fire first, then inner pairs, then center.

### Step 2: Spritesheet setup

Sigil.png is unusual: each frame is the full beam width (310px wide, 17px tall). Four frames stacked vertically:

```c
/* Sigil.png: 310x68 -- 4 rows of 310x17 */
if (mgr->sprite_sigil.pixels) {
  spritesheet_init(&mgr->sheet_sigil, mgr->sprite_sigil, 310, 17);
}

/* SigilWait.png: 310x34 -- 2 rows of 310x17 */
if (mgr->sprite_sigil_wait.pixels) {
  spritesheet_init(&mgr->sheet_sigil_wait, mgr->sprite_sigil_wait, 310, 17);
}
```

### Step 3: Spawn Sigil Block 1

A single entity with a brief wait period before charging:

```c
static void spawn_sigil_block_1(ObstacleBlock *block, float block_x) {
  block->entity_count = 1;
  ObstacleEntity *e = &block->entities[0];
  memset(e, 0, sizeof(*e));
  e->x = block_x;
  e->y = 85.0f;
  e->state = SIGIL_WAIT;
  e->timer = 1.0f;
  e->active = 1;
  e->hitbox = aabb_make(e->x, e->y, SIGIL_BEAM_W, SIGIL_BEAM_H);
}
```

### Step 4: Spawn Sigil Block 2 with fire_order initialization

All 5 sigils start in WAIT, but the block immediately sets sigils 0 and 1 to CHARGE:

```c
static void spawn_sigil_block_2(ObstacleBlock *block, float block_x) {
  block->entity_count = SIGIL_BLOCK_2_COUNT;
  for (int i = 0; i < SIGIL_BLOCK_2_COUNT; i++) {
    ObstacleEntity *e = &block->entities[i];
    memset(e, 0, sizeof(*e));
    e->x = block_x;
    e->y = SIGIL_BLOCK_2_Y[i];
    e->state = SIGIL_WAIT;
    e->timer = 0.0f;
    e->active = 1;
    e->hitbox = aabb_make(e->x, e->y, SIGIL_BEAM_W, SIGIL_BEAM_H);
  }
  /* Phase 0: sigils 0,1 begin charging */
  block->fire_order = 0;
  block->block_timer = SIGIL_BLOCK_2_CHARGE_DURATION;
  block->entities[0].state = SIGIL_CHARGE;
  block->entities[0].timer = SIGIL_BLOCK_2_CHARGE_DURATION;
  block->entities[1].state = SIGIL_CHARGE;
  block->entities[1].timer = SIGIL_BLOCK_2_CHARGE_DURATION;
}
```

### Step 5: Individual sigil state machine

For Sigil Block 1, each entity manages its own transitions:

```c
static void update_sigil(ObstacleEntity *e, float dt, ObstacleType block_type,
                         float camera_x) {
  e->x = camera_x + (float)GAME_W * 0.5f;

  float charge_dur = (block_type == OBS_SIGIL_BLOCK_1)
                         ? SIGIL_BLOCK_1_CHARGE_DURATION
                         : SIGIL_BLOCK_2_CHARGE_DURATION;
  float fire_dur = (block_type == OBS_SIGIL_BLOCK_1)
                       ? SIGIL_BLOCK_1_FIRE_DURATION
                       : SIGIL_BLOCK_2_FIRE_DURATION;

  switch (e->state) {
  case SIGIL_WAIT:
    e->timer -= dt;
    if (e->timer <= 0.0f) {
      e->state = SIGIL_CHARGE;
      e->timer = charge_dur;
    }
    break;
  case SIGIL_CHARGE:
    e->timer -= dt;
    if (e->timer <= 0.0f) {
      e->state = SIGIL_FIRE;
      e->timer = fire_dur;
    }
    break;
  case SIGIL_FIRE:
    e->timer -= dt;
    e->hitbox = aabb_make(e->x, e->y - SIGIL_BEAM_H * 0.5f,
                           SIGIL_BEAM_W, SIGIL_BEAM_H);
    if (e->timer <= 0.0f) {
      e->state = SIGIL_POWERDOWN;
      e->timer = 0.75f;
    }
    break;
  case SIGIL_POWERDOWN:
    e->timer -= dt;
    if (e->timer <= 0.0f) {
      e->active = 0;
    }
    break;
  }
}
```

Notice the first line: `e->x = camera_x + GAME_W * 0.5f`. Sigils are screen-locked. Their X position updates every frame to track the camera center.

### Step 6: Block-level fire_order coordinator

Sigil Block 2's coordinator manages phase transitions. When the block_timer expires, it advances to the next phase and overrides entity states:

```c
if (block->type == OBS_SIGIL_BLOCK_2) {
  block->block_timer -= dt;
  if (block->block_timer <= 0.0f) {
    block->fire_order++;
    switch (block->fire_order) {
    case 1:
      /* Sigils 0,1 FIRE; sigils 2,3 start CHARGE */
      block->entities[0].state = SIGIL_FIRE;
      block->entities[0].timer = SIGIL_BLOCK_2_FIRE_DURATION;
      block->entities[1].state = SIGIL_FIRE;
      block->entities[1].timer = SIGIL_BLOCK_2_FIRE_DURATION;
      block->entities[2].state = SIGIL_CHARGE;
      block->entities[2].timer = SIGIL_BLOCK_2_CHARGE_DURATION;
      block->entities[3].state = SIGIL_CHARGE;
      block->entities[3].timer = SIGIL_BLOCK_2_CHARGE_DURATION;
      block->block_timer = SIGIL_BLOCK_2_FIRE_DURATION;
      break;
    case 2:
      /* 0,1 POWERDOWN; 2,3 FIRE; 4 CHARGE */
      block->entities[0].state = SIGIL_POWERDOWN;
      block->entities[0].timer = 0.75f;
      block->entities[1].state = SIGIL_POWERDOWN;
      block->entities[1].timer = 0.75f;
      block->entities[2].state = SIGIL_FIRE;
      block->entities[2].timer = SIGIL_BLOCK_2_FIRE_DURATION;
      block->entities[3].state = SIGIL_FIRE;
      block->entities[3].timer = SIGIL_BLOCK_2_FIRE_DURATION;
      block->entities[4].state = SIGIL_CHARGE;
      block->entities[4].timer = SIGIL_BLOCK_2_CHARGE_DURATION;
      block->block_timer = SIGIL_BLOCK_2_FIRE_DURATION;
      break;
    case 3:
      /* 2,3 POWERDOWN; 4 FIRE */
      block->entities[2].state = SIGIL_POWERDOWN;
      block->entities[2].timer = 0.75f;
      block->entities[3].state = SIGIL_POWERDOWN;
      block->entities[3].timer = 0.75f;
      block->entities[4].state = SIGIL_FIRE;
      block->entities[4].timer = SIGIL_BLOCK_2_FIRE_DURATION;
      block->block_timer = SIGIL_BLOCK_2_FIRE_DURATION;
      break;
    case 4:
      /* 4 POWERDOWN; block done soon */
      block->entities[4].state = SIGIL_POWERDOWN;
      block->entities[4].timer = 0.75f;
      block->block_timer = 1.0f;
      break;
    default:
      block->active = 0;
      break;
    }
  }
}
```

### Step 7: Rendering -- beam sprite at screen position

Sigil sprites render at x=0 (screen left edge) and span the full width. The Y is centered on the sigil entity's world Y. Different states use different sprites:

```c
if (e->state == SIGIL_FIRE) {
  SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_sigil, 0);
  sprite_blit(bb, &mgr->sprite_sigil, rect, 0, sy - rect.h / 2);
} else if (e->state == SIGIL_CHARGE) {
  SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_sigil, 2);
  sprite_blit_alpha(bb, &mgr->sprite_sigil, rect, 0, sy - rect.h / 2);
} else if (e->state == SIGIL_WAIT) {
  SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_sigil_wait, 0);
  sprite_blit_alpha(bb, &mgr->sprite_sigil_wait, rect, 0, sy - rect.h / 2);
}
```

Note that CHARGE uses `sprite_blit_alpha` for a semi-transparent effect, hinting that the beam is powering up.

## Common Mistakes

1. **Making sigil hitboxes world-relative instead of camera-relative.** The hitbox during FIRE must cover the entire visible screen width. Use `aabb_make(camera_x, e->y - SIGIL_BEAM_H * 0.5f, (float)GAME_W, SIGIL_BEAM_H)` so the hitbox moves with the camera.

2. **Running both the block coordinator AND individual update_sigil for Block 2 entities.** Sigil Block 2 entities are managed by the fire_order coordinator. Only POWERDOWN timer decrements happen per-entity. Calling `update_sigil` on them would cause double state transitions.

3. **Rendering the beam at the entity's world X minus camera.** Sigil beams always start at screen x=0 regardless of world position. They are not world-space objects -- they are screen-space hazards.

4. **Forgetting to update sigil X positions every frame.** Sigils must track `camera_x + GAME_W * 0.5f` continuously. If you only set X at spawn time, the sigil drifts off-screen as the camera scrolls.
