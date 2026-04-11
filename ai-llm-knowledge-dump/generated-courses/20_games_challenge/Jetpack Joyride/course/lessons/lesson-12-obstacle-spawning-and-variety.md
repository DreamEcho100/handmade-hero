# Lesson 12 -- Obstacle Spawning and Variety

## Observable Outcome
Obstacles now appear dynamically as the player scrolls forward. A random obstacle type is selected each time the player approaches the next spawn point. No two consecutive obstacle blocks are the same type. Blocks that scroll far behind the player are automatically despawned to reclaim their slot. The console prints spawn events showing the type and world position.

## New Concepts (max 3)
1. Xorshift32 pseudo-random number generator (deterministic, seedable)
2. No-repeat spawn rule via do-while reroll
3. Trigger-based spawning: player_x + GAME_W >= next_spawn_x

## Prerequisites
- Lesson 11 (dagger blocks, ObstacleBlock data structure)
- Lesson 10 (AABB collision)

## Background

Up to this point we have been placing test obstacles at fixed positions. A real game needs procedural spawning -- obstacles appear ahead of the player at intervals, chosen from a pool of types. The original SlimeEscape picks a random obstacle type with the constraint that no two consecutive blocks share the same type (to prevent impossible patterns).

We need a random number generator. The C standard `rand()` is non-deterministic across platforms and not seedable in a portable way. Instead we use Xorshift32, a fast PRNG that produces good-quality randomness from three XOR-shift operations. It occupies a single uint32_t of state, seeds deterministically, and runs identically on every platform. We keep two separate RNG instances: one for gameplay (obstacle selection) and one for visual effects (particle randomness), so replays stay deterministic even if the effects change.

The spawn system uses a distance-trigger model. Each obstacle type has a spawn distance that defines how far ahead it occupies. When the player's visible right edge (player_x + GAME_W) reaches or passes next_spawn_x, a new block is spawned and next_spawn_x advances by that block's spawn distance. Blocks are despawned when they fall more than 400 pixels behind the player.

## Code Walkthrough

### Step 1: The RNG in `utils/rng.h`

Create a header-only RNG. The state must be nonzero (zero is a fixed point of xorshift):

```c
typedef struct {
  uint32_t state;
} RNG;

static inline void rng_seed(RNG *r, uint32_t seed) {
  r->state = seed ? seed : 1;
}

static inline uint32_t rng_next(RNG *r) {
  uint32_t x = r->state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  r->state = x;
  return x;
}
```

Three shift constants (13, 17, 5) are from Marsaglia's Xorshift paper. The specific triple produces a full-period generator (2^32 - 1 values before repeating).

Range helpers:

```c
static inline int rng_range(RNG *r, int min, int max) {
  if (min >= max) return min;
  uint32_t range = (uint32_t)(max - min + 1);
  return min + (int)(rng_next(r) % range);
}

static inline float rng_float(RNG *r) {
  return (float)(rng_next(r) & 0x00FFFFFF) / (float)0x01000000;
}

static inline float rng_float_range(RNG *r, float min, float max) {
  return min + rng_float(r) * (max - min);
}
```

### Step 2: ObstacleManager in `game/obstacles.h`

The obstacle manager holds the block pool, pellet pool, shield pickups, near-miss indicators, and two RNG instances:

```c
typedef struct {
  ObstacleBlock blocks[MAX_OBSTACLE_BLOCKS];
  int block_count;

  ObstacleType last_spawned_type;
  float next_spawn_x;

  RNG rng;
  RNG effects_rng;

  /* ... sprite assets, pellets, shields, etc ... */
} ObstacleManager;
```

MAX_OBSTACLE_BLOCKS is 4 -- enough for 3-4 overlapping blocks at any time.

### Step 3: Spawn distance table

Each obstacle type has a fixed spawn distance defining the horizontal space it occupies. Measured from the original .tscn files:

```c
static const float SPAWN_DISTANCES[OBS_TYPE_COUNT] = {
  0.0f,   /* OBS_NONE */
  520.0f, /* OBS_DAGGER_BLOCK_1 */
  520.0f, /* OBS_MISSILE_BLOCK_1 */
  240.0f, /* OBS_SIGIL_BLOCK_1 */
  525.0f, /* OBS_MISSILE_BLOCK_2 */
  320.0f, /* OBS_SIGIL_BLOCK_2 */
  360.0f, /* OBS_DAGGER_BLOCK_2 */
};
```

Dagger blocks are the widest (520px) because they contain the most sub-entities spread over horizontal space. Sigil Block 1 is the narrowest (240px) because it is a single beam.

### Step 4: Initialize the spawn system

In `obstacles_init`, seed both RNGs and set the first spawn point 200 pixels ahead of the player start:

```c
int obstacles_init(ObstacleManager *mgr) {
  memset(mgr, 0, sizeof(*mgr));
  rng_seed(&mgr->rng, 42);
  rng_seed(&mgr->effects_rng, 137);
  mgr->next_spawn_x = PLAYER_START_X + 200.0f;
  mgr->shield_timer = rng_float_range(&mgr->rng, 20.0f, 40.0f);
  /* ... load sprites ... */
  return 0;
}
```

### Step 5: The spawn trigger and no-repeat rule

At the top of `obstacles_update`, check whether the player's visible area has reached the next spawn point. The `while` loop handles the case where the player moves so fast that multiple blocks need spawning in one frame:

```c
while (player_x + (float)GAME_W >= mgr->next_spawn_x) {
  ObstacleType type;
  do {
    type = (ObstacleType)rng_range(&mgr->rng, 1, (int)OBS_TYPE_COUNT - 1);
  } while (type == mgr->last_spawned_type);

  spawn_block(mgr, type, mgr->next_spawn_x);
  mgr->last_spawned_type = type;
  mgr->next_spawn_x += SPAWN_DISTANCES[type];
}
```

The do-while reroll is the no-repeat rule. It generates a random type, and if it matches the last one, it tries again. Since there are 6 valid types and only 1 is excluded, the expected number of rolls is 6/5 = 1.2 -- virtually free.

### Step 6: Find a free block slot

The block pool is a fixed-size array. `find_free_block` scans for the first inactive slot:

```c
static ObstacleBlock *find_free_block(ObstacleManager *mgr) {
  for (int i = 0; i < MAX_OBSTACLE_BLOCKS; i++) {
    if (!mgr->blocks[i].active) return &mgr->blocks[i];
  }
  return NULL;
}
```

If no slot is free (should not happen with only 4 slots and proper despawning), we skip the spawn with a debug warning.

### Step 7: Dispatch to type-specific spawn functions

`spawn_block` clears the block, sets its type, and dispatches:

```c
static void spawn_block(ObstacleManager *mgr, ObstacleType type, float block_x) {
  ObstacleBlock *block = find_free_block(mgr);
  if (!block) return;

  memset(block, 0, sizeof(*block));
  block->type = type;
  block->x = block_x;
  block->active = 1;
  block->spawn_distance = SPAWN_DISTANCES[type];

  switch (type) {
  case OBS_DAGGER_BLOCK_1:
    spawn_dagger_block(block, DAGGER_BLOCK_1_POS,
                       DAGGER_BLOCK_1_COUNT, block_x, &mgr->rng);
    break;
  case OBS_DAGGER_BLOCK_2:
    spawn_dagger_block(block, DAGGER_BLOCK_2_POS,
                       DAGGER_BLOCK_2_COUNT, block_x, &mgr->rng);
    break;
  /* ... missile and sigil cases added in later lessons ... */
  default: break;
  }
}
```

### Step 8: Despawn blocks behind the player

In the update loop, after spawning, iterate active blocks and deactivate any that have scrolled far behind:

```c
for (int b = 0; b < MAX_OBSTACLE_BLOCKS; b++) {
  ObstacleBlock *block = &mgr->blocks[b];
  if (!block->active) continue;

  if (block->x + block->spawn_distance + 200.0f < player_x - 400.0f) {
    block->active = 0;
    continue;
  }
  /* ... update entities ... */
}
```

The 400px buffer behind the player ensures blocks are not despawned while still partially visible.

## Common Mistakes

1. **Seeding the RNG with zero.** Xorshift32 has a fixed point at zero (0 XOR anything = 0). The seed function guards against this by converting 0 to 1, but if you bypass the seed function and set state directly, you get an infinite stream of zeros.

2. **Using a single check instead of a while loop for spawning.** At high player speeds, the player may cross multiple spawn thresholds in a single frame. An `if` would miss the extra spawns; a `while` handles them all.

3. **Not recording last_spawned_type before advancing next_spawn_x.** If you advance first and record second, a crash between the two operations leaves the no-repeat rule broken. Always record the type immediately after spawning.

4. **Forgetting to set block->active = 1 in spawn_block.** The block will be skipped by every update and render loop, making the obstacle invisible.

5. **Using rand() from stdlib.** It is not portable across compilers, not seedable in a deterministic way, and its quality varies wildly. Xorshift32 gives consistent results everywhere.
