# Lesson 15 -- Pellet Collectibles and Scoring

## Observable Outcome
Animated pellets appear at hand-placed positions within obstacle blocks. Walking or flying through a pellet collects it -- the pellet disappears, a sound plays, and the score increments. The score display shows three components combined: distance/10 + pellets*10 + near_misses*50. Dagger Block 1 has 16 pellets tracing diagonal paths through the safe gaps. Dagger Block 2 has 9 pellets. Missile Block 2 has 21 pellets in a wave pattern.

## New Concepts (max 3)
1. Entity pool pattern: fixed-size Pellet array with active flags
2. Hand-placed collectible positions from .tscn scene files
3. Composite scoring formula combining distance, pellets, and bonuses

## Prerequisites
- Lesson 12 (obstacle spawning and block lifecycle)
- Lesson 10 (AABB collision for collection detection)
- Lesson 08 (SpriteAnimation for pellet spin)

## Background

Pellets reward the player for taking risky paths through obstacle formations. In the original game, pellets are placed by hand in each .tscn scene file along paths that weave through the safe gaps between obstacles. Collecting them is optional but contributes significantly to the score.

The pellet system uses an entity pool -- a fixed-size array of MAX_PELLETS=64 Pellet structs, each with an `active` flag. When a block spawns, it pushes its pellets into the pool. When a pellet is collected or scrolls far behind the player, its active flag is set to 0. The pool is never compacted; iteration skips inactive entries.

Point.png is a 40x10 horizontal strip containing 4 frames of 10x10 pixels each. The animation loops at 4 fps, creating a spinning coin effect. Each pellet carries its own SpriteAnimation so their rotations are independent (staggered start times would look better, but the original game does not do this).

The scoring formula in the frozen source is: `score = (int)(total_distance / 10.0f) + pellet_count * 10 + near_miss_count * 50`. Distance dominates for long runs, but pellets and near-misses matter for high-score optimization.

## Code Walkthrough

### Step 1: Pellet struct in `game/types.h`

Each pellet has a world position, an active flag, a collision hitbox, and its own animation state:

```c
typedef struct {
  float x, y;
  int active;
  AABB hitbox;
  SpriteAnimation anim;
} Pellet;

#define MAX_PELLETS 64
```

### Step 2: Pellet constants in `obstacles.c`

```c
#define PELLET_HITBOX_W 10.0f
#define PELLET_HITBOX_H 10.0f
```

### Step 3: Hand-placed position tables

Pellet positions are extracted from the original .tscn files. Each array stores (x, y) pairs relative to the block origin:

```c
/* Dagger Block 1: 16 pellets tracing diagonals through safe gaps */
static const float DAGGER_BLOCK_1_PELLETS[][2] = {
  {81, 114}, {94, 107}, {107, 100},       /* left-bottom diagonal */
  {163, 64}, {176, 57}, {189, 50},        /* left-middle diagonal */
  {263, 86}, {277, 86}, {291, 86}, {305, 86}, /* center horizontal */
  {382, 50}, {395, 57}, {408, 64},        /* right-middle diagonal */
  {464, 100}, {477, 107}, {490, 114},     /* right-bottom diagonal */
};
#define DAGGER_BLOCK_1_PELLET_COUNT 16

/* Dagger Block 2: 9 pellets */
static const float DAGGER_BLOCK_2_PELLETS[][2] = {
  {84, 66}, {92, 80}, {100, 94},
  {189, 94}, {197, 80}, {205, 66},
  {320, 138}, {334, 138}, {348, 138},
};
#define DAGGER_BLOCK_2_PELLET_COUNT 9

/* Missile Block 2: 21 pellets in a wave pattern */
static const float MISSILE_BLOCK_2_PELLETS[][2] = {
  {60, 143}, {73, 143}, {86, 143},
  {130, 108}, {143, 108}, {156, 108},
  {200, 73}, {213, 73}, {226, 73},
  {270, 38}, {283, 38}, {296, 38},
  {340, 73}, {353, 73}, {366, 73},
  {410, 108}, {423, 108}, {436, 108},
  {480, 143}, {493, 143}, {506, 143},
};
#define MISSILE_BLOCK_2_PELLET_COUNT 21
```

### Step 4: Pellet spritesheet initialization

Point.png is a horizontal strip. Detect layout by comparing width to height:

```c
if (mgr->sprite_pellet.pixels) {
  int pw = mgr->sprite_pellet.width;
  int ph = mgr->sprite_pellet.height;
  if (pw > ph) {
    int frame_w = pw / 4;
    spritesheet_init(&mgr->sheet_pellet, mgr->sprite_pellet, frame_w, ph);
  } else {
    spritesheet_init(&mgr->sheet_pellet, mgr->sprite_pellet, pw, ph);
  }
}
```

For Point.png (40x10), this yields 4 frames of 10x10.

### Step 5: Spawn pellets when a block spawns

`spawn_pellets_placed` pushes pellets into the pool from a position table:

```c
static void spawn_pellets_placed(ObstacleManager *mgr, float block_x,
                                  const float positions[][2], int count) {
  for (int i = 0; i < count; i++) {
    if (mgr->pellet_count >= MAX_PELLETS) break;
    Pellet *p = &mgr->pellets[mgr->pellet_count++];
    memset(p, 0, sizeof(*p));
    p->x = block_x + positions[i][0];
    p->y = positions[i][1];
    p->active = 1;
    p->hitbox = aabb_make(p->x, p->y, PELLET_HITBOX_W, PELLET_HITBOX_H);
    if (mgr->sheet_pellet.sprite.pixels) {
      anim_init(&p->anim, &mgr->sheet_pellet, 0,
                mgr->sheet_pellet.cols * mgr->sheet_pellet.rows,
                4.0f, 1);
    }
  }
}
```

Call it from `spawn_block` after creating the obstacle entities:

```c
switch (type) {
case OBS_DAGGER_BLOCK_1:
  spawn_pellets_placed(mgr, block_x, DAGGER_BLOCK_1_PELLETS,
                        DAGGER_BLOCK_1_PELLET_COUNT);
  break;
case OBS_DAGGER_BLOCK_2:
  spawn_pellets_placed(mgr, block_x, DAGGER_BLOCK_2_PELLETS,
                        DAGGER_BLOCK_2_PELLET_COUNT);
  break;
case OBS_MISSILE_BLOCK_2:
  spawn_pellets_placed(mgr, block_x, MISSILE_BLOCK_2_PELLETS,
                        MISSILE_BLOCK_2_PELLET_COUNT);
  break;
default: break;
}
```

### Step 6: Update pellets -- animate and despawn

In `obstacles_update`, advance each pellet's animation and despawn those far behind:

```c
for (int i = 0; i < mgr->pellet_count; i++) {
  Pellet *p = &mgr->pellets[i];
  if (!p->active) continue;
  anim_update(&p->anim, dt);

  if (p->x < player_x - 400.0f) {
    p->active = 0;
  }
}
```

### Step 7: Collection check -- `obstacles_collect_pellets`

Returns the number of pellets collected this frame:

```c
int obstacles_collect_pellets(ObstacleManager *mgr, AABB player_hitbox) {
  int collected = 0;
  for (int i = 0; i < mgr->pellet_count; i++) {
    Pellet *p = &mgr->pellets[i];
    if (!p->active) continue;
    if (aabb_overlap(player_hitbox, p->hitbox)) {
      p->active = 0;
      collected++;
    }
  }
  return collected;
}
```

### Step 8: Score calculation in `game_update`

In the main game loop, collect pellets and update the score:

```c
int pellets = obstacles_collect_pellets(&state->obstacles,
                                        state->player.hitbox);
if (pellets > 0) {
  game_audio_play(&state->audio, CLIP_PELLET);
}
state->pellet_count += pellets;

/* Composite score formula */
int distance_score = (int)(state->player.total_distance / 10.0f);
state->score = distance_score + state->pellet_count * 10 +
               state->near_miss_count * 50;
```

### Step 9: Render pellets

Draw each active pellet using its animation frame, converting from world to screen coordinates:

```c
for (int i = 0; i < mgr->pellet_count; i++) {
  const Pellet *p = &mgr->pellets[i];
  if (!p->active) continue;

  int px = (int)(p->x - camera_x);
  int py = (int)p->y;

  if (mgr->sprite_pellet.pixels) {
    anim_draw(&p->anim, bb, px, py);
  } else {
    draw_rect(bb, (float)px, (float)py, 8.0f, 8.0f,
              1.0f, 0.9f, 0.2f, 1.0f);
  }
}
```

## Common Mistakes

1. **Using pellet_count as an active count instead of a high-water mark.** `pellet_count` only increases -- it tracks how many pellets have ever been pushed into the pool. Active pellets are identified by `p->active == 1`. If you use pellet_count to detect "no pellets left", you will always think there are pellets.

2. **Forgetting to offset pellet positions by block_x.** The position tables are block-relative. Without adding block_x, all pellets from every block stack at the same screen-left positions.

3. **Playing the pellet sound once per collection call instead of once per frame.** The function returns a count, but you only want one sound trigger per frame regardless of how many pellets were collected simultaneously. One call to `game_audio_play(CLIP_PELLET)` when `pellets > 0` is correct.

4. **Not capping the pool.** The `if (mgr->pellet_count >= MAX_PELLETS) break` guard prevents writing past the array bounds. Without it, a long run will overflow.
