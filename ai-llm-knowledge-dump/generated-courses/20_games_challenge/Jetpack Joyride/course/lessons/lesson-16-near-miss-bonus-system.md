# Lesson 16 -- Near-Miss Bonus System

## Observable Outcome
When the player flies within 12 pixels of a dangerous obstacle without touching it, a green "+50" indicator appears at the player's position, floats upward, and fades over 0.8 seconds. The score increments by 50 for each near miss. Multiple near-miss indicators can be active simultaneously (up to 4). The near-miss check runs against daggers (always) and missiles (during FIRE state only) -- sigils are excluded because their screen-spanning beams make near-misses meaningless.

## New Concepts (max 3)
1. aabb_near_miss: expanded hitbox minus real hitbox = proximity zone
2. NearMissIndicator entity pool with float-up and fade-out behavior
3. Selective hazard filtering -- only certain obstacle types qualify

## Prerequisites
- Lesson 10 (AABB collision, aabb_near_miss function)
- Lesson 15 (scoring formula, obstacles_check_near_miss integration point)
- Lesson 06 (draw_text for rendering the "+50" text)

## Background

Near-miss bonuses reward skillful play. The mechanic is simple: if the player's hitbox enters a zone around an obstacle but does not overlap the obstacle's actual hitbox, that counts as a near miss. The zone is defined by expanding the obstacle's AABB outward by NEAR_MISS_MARGIN (12 pixels) on all four sides.

The detection uses the `aabb_near_miss` function from Lesson 10: expand the obstacle, check overlap with the expanded box, then confirm NO overlap with the original box. This two-part test has an important implication -- a near miss is detected ONLY in the gap between the expanded zone and the real hitbox. If the player is overlapping the real hitbox, that is a collision (handled by the damage system), not a near miss.

Not all obstacles generate near-miss events. Daggers are always eligible because they have well-defined spatial hitboxes the player can fly past. Missiles are eligible only during FIRE state (when they are actually dangerous). Sigils are excluded -- their beams span the full screen width, so there is no meaningful "near" position.

The visual feedback uses a NearMissIndicator pool of 4 slots. Each indicator stores a position, a timer counting down from 0.8 seconds, and an active flag. The indicator floats upward at 20 pixels/second and is rendered as green text with alpha proportional to remaining time.

## Code Walkthrough

### Step 1: NearMissIndicator struct in `game/types.h`

```c
typedef struct {
  float x, y;
  float timer;         /* Display duration remaining */
  int active;
} NearMissIndicator;

#define MAX_NEAR_MISS_INDICATORS 4
```

Add the pool to ObstacleManager:

```c
NearMissIndicator near_misses[MAX_NEAR_MISS_INDICATORS];
```

### Step 2: Near-miss margin constant

```c
#define NEAR_MISS_MARGIN 12.0f
```

12 pixels is roughly half a dagger radius -- close enough to feel dangerous, far enough to be achievable with practice.

### Step 3: `obstacles_check_near_miss` -- detection and indicator creation

This function iterates all active entities, filters for dangerous ones, and calls `aabb_near_miss`. When a near miss is detected, it also creates a visual indicator:

```c
int obstacles_check_near_miss(ObstacleManager *mgr, AABB player_hitbox) {
  int count = 0;
  for (int b = 0; b < MAX_OBSTACLE_BLOCKS; b++) {
    ObstacleBlock *block = &mgr->blocks[b];
    if (!block->active) continue;

    for (int i = 0; i < block->entity_count; i++) {
      ObstacleEntity *e = &block->entities[i];
      if (!e->active) continue;

      int is_dangerous = 0;
      if (block->type == OBS_DAGGER_BLOCK_1 ||
          block->type == OBS_DAGGER_BLOCK_2)
        is_dangerous = 1;
      if ((block->type == OBS_MISSILE_BLOCK_1 ||
           block->type == OBS_MISSILE_BLOCK_2) &&
          e->state == MISSILE_FIRE)
        is_dangerous = 1;

      if (is_dangerous &&
          aabb_near_miss(player_hitbox, e->hitbox, NEAR_MISS_MARGIN)) {
        count++;

        /* Create visual indicator */
        for (int n = 0; n < MAX_NEAR_MISS_INDICATORS; n++) {
          if (!mgr->near_misses[n].active) {
            mgr->near_misses[n].x = player_hitbox.x;
            mgr->near_misses[n].y = player_hitbox.y - 15.0f;
            mgr->near_misses[n].timer = 0.8f;
            mgr->near_misses[n].active = 1;
            break;
          }
        }
      }
    }
  }
  return count;
}
```

Key details: the indicator spawns at the player's position (not the obstacle), 15 pixels above the hitbox top. The `break` after creating an indicator ensures only one indicator per near-miss event.

### Step 4: Update indicators -- float and fade

In `obstacles_update`, tick down each indicator's timer and move it upward:

```c
for (int i = 0; i < MAX_NEAR_MISS_INDICATORS; i++) {
  NearMissIndicator *nm = &mgr->near_misses[i];
  if (!nm->active) continue;
  nm->timer -= dt;
  nm->y -= 20.0f * dt;  /* Float upward at 20 px/sec */
  if (nm->timer <= 0.0f) {
    nm->active = 0;
  }
}
```

The upward float is simple constant velocity -- no acceleration needed for a brief 0.8-second display.

### Step 5: Render indicators

Draw "+50" text in green with fading alpha. The alpha is `timer * 2.0f` which maps 0.8s -> 1.6 (clamped to 1.0 by draw_text) at spawn and fades to 0.0 at expiry:

```c
for (int i = 0; i < MAX_NEAR_MISS_INDICATORS; i++) {
  const NearMissIndicator *nm = &mgr->near_misses[i];
  if (!nm->active) continue;

  int nx = (int)(nm->x - camera_x);
  int ny = (int)nm->y;

  draw_text(bb, (float)nx, (float)ny, 1, "+50",
            0.2f, 1.0f, 0.4f, nm->timer * 2.0f);
}
```

The color (0.2, 1.0, 0.4) is a bright green that contrasts against the dark background.

### Step 6: Integration in `game_update`

Call the near-miss check after collision checking (the player must be alive):

```c
if (state->player.state == PLAYER_STATE_NORMAL) {
  int near = obstacles_check_near_miss(&state->obstacles,
                                       state->player.hitbox);
  state->near_miss_count += near;
}
```

The guard `PLAYER_STATE_NORMAL` prevents accumulating near-misses during the death animation.

### Step 7: Score contribution

Near misses contribute to the composite score at 50 points each:

```c
state->score = distance_score + state->pellet_count * 10 +
               state->near_miss_count * 50;
```

At 50 points per event, a skilled player who consistently threads between obstacles can significantly boost their score beyond what distance alone provides.

## Common Mistakes

1. **Checking near-miss on the same frame as collision.** The game checks collision first. If the player is hit, they enter the hurt/dead state. The near-miss check only runs if the player is in PLAYER_STATE_NORMAL. This prevents the absurd case of getting both a near-miss bonus and taking damage from the same obstacle.

2. **Including sigils in the near-miss check.** Sigil beams span the full screen width. The expanded hitbox would cover the entire viewport, making every frame a "near miss" when any sigil is firing. The is_dangerous filter intentionally excludes sigils.

3. **Not limiting the indicator pool.** Without the MAX_NEAR_MISS_INDICATORS cap and the `break` after creating one, a frame with many near-misses could overflow. The pool of 4 is sufficient because indicators last only 0.8 seconds.

4. **Placing the indicator at the obstacle position instead of the player position.** The indicator should appear where the player IS (the action point), not where the obstacle is. The player might be hundreds of pixels away from the obstacle's world position by the time the indicator finishes displaying.

5. **Using absolute alpha instead of timer-proportional alpha.** A fixed alpha makes the indicator pop in and pop out. The `timer * 2.0f` formula creates a fade-out that starts bright and dims to zero, giving a polished floating-text effect.
