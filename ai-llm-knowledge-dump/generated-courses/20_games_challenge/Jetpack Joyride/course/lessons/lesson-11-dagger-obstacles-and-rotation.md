# Lesson 11 -- Dagger Obstacles and Rotation

## Observable Outcome
Two dagger block formations appear in the world. Dagger Block 1 shows 18 daggers arranged in 5 clusters forming an S-curve the player must weave through. Dagger Block 2 shows 9 daggers in 3 vertical columns. Every dagger spins at 20 radians/second with a random starting angle. The daggers have square 22x22 hitboxes and collide with the player using the AABB system from Lesson 10.

## New Concepts (max 3)
1. sprite_blit_rotated -- inverse rotation per-pixel lookup for spinning sprites
2. Hand-placed obstacle layouts from .tscn scene data
3. ObstacleEntity and ObstacleBlock data structures

## Prerequisites
- Lesson 10 (AABB collision detection)
- Lesson 03 (sprite blitting and spritesheets)

## Background

Daggers are the first real obstacle the player encounters. In the original Godot game, each dagger is a Sprite2D with a constantly-rotating transform and a CircleShape2D collider with radius 11. We approximate the circular collider as a 22x22 AABB -- at this sprite scale, the difference is invisible.

The rotation itself is the interesting technical challenge. Our software renderer has no GPU to rotate a texture for free. We implement `sprite_blit_rotated` using the inverse-mapping approach: for each pixel in the output bounding box, we compute where that pixel came from in the source sprite by applying the inverse rotation transform. This is O(output_area) work per dagger, but since daggers are small sprites (about 16x16) the cost is trivial.

The dagger positions come directly from the original .tscn scene files. Dagger Block 1 (dagger_block_1.tscn) places 18 daggers in 5 clusters: bottom-left at y=135, middle-left at y=85, top-center at y=35 (6 daggers spanning the width), middle-right at y=85, and bottom-right at y=135. This creates a wave pattern the player must fly through. Dagger Block 2 (dagger_block_2.tscn) places 9 daggers in 3 vertical columns at x=40, x=145, and x=250.

Each obstacle block is represented as an ObstacleBlock containing an array of ObstacleEntity sub-objects. The block tracks its world X position and type; each entity within it has its own position, rotation, activity flag, and hitbox. MAX_ENTITIES_PER_BLOCK is 18 to accommodate Dagger Block 1, the largest formation.

## Code Walkthrough

### Step 1: Define obstacle data structures in `game/types.h`

The ObstacleEntity struct holds per-dagger state. The rotation field accumulates over time for the spinning effect:

```c
typedef struct {
  float x, y;
  float vx, vy;
  float rotation;       /* For daggers (radians) */
  float timer;          /* State machine timer */
  int state;            /* MissileState or SigilState */
  int active;
  int tracking;         /* 1 if missile tracks player Y */
  AABB hitbox;
} ObstacleEntity;

#define MAX_ENTITIES_PER_BLOCK 18  /* Dagger Block 1 has 18 daggers */
```

The ObstacleBlock groups entities together:

```c
typedef struct {
  ObstacleType type;
  float x;              /* Block world X position */
  int active;
  float spawn_distance;
  ObstacleEntity entities[MAX_ENTITIES_PER_BLOCK];
  int entity_count;
  /* ... missile/sigil fields added in later lessons ... */
} ObstacleBlock;
```

### Step 2: Dagger layout constants in `game/obstacles.c`

Hard-code the exact positions from the .tscn files. These positions are relative to the block's world X:

```c
static const float DAGGER_BLOCK_1_POS[][2] = {
  /* Bottom-left cluster (y=135) */
  {71, 135}, {89, 135}, {107, 135},
  /* Middle-left cluster (y=85) */
  {153, 85}, {171, 85}, {189, 85},
  /* Top cluster (y=35) -- 6 daggers spanning the middle */
  {231, 35}, {249, 35}, {267, 35}, {302, 35}, {320, 35}, {338, 35},
  /* Middle-right cluster (y=85) */
  {380, 85}, {398, 85}, {416, 85},
  /* Bottom-right cluster (y=135) */
  {462, 135}, {480, 135}, {498, 135},
};
#define DAGGER_BLOCK_1_COUNT 18
```

And Dagger Block 2 -- three vertical columns:

```c
static const float DAGGER_BLOCK_2_POS[][2] = {
  {40, 114}, {40, 130}, {40, 146},
  {145, 24}, {145, 40}, {145, 56},
  {250, 114}, {250, 130}, {250, 146},
};
#define DAGGER_BLOCK_2_COUNT 9
```

Define collision and rotation constants:

```c
#define DAGGER_HITBOX_W 22.0f
#define DAGGER_HITBOX_H 22.0f
#define DAGGER_ROTATION_SPEED 20.0f /* radians per second */
```

### Step 3: Spawn dagger entities

The spawn function initializes each entity with its position (offset by the block's world X), a random starting rotation, and an AABB hitbox built from the center point:

```c
static void spawn_dagger_block(ObstacleBlock *block, const float positions[][2],
                                int count, float block_x, RNG *rng) {
  block->entity_count = count;
  for (int i = 0; i < count; i++) {
    ObstacleEntity *e = &block->entities[i];
    memset(e, 0, sizeof(*e));
    e->x = block_x + positions[i][0];
    e->y = positions[i][1];
    e->rotation = rng_float_range(rng, 0.0f, 6.2832f);
    e->active = 1;
    e->hitbox = aabb_from_center(e->x, e->y,
                                  DAGGER_HITBOX_W * 0.5f, DAGGER_HITBOX_H * 0.5f);
  }
}
```

Notice that `aabb_from_center` is used because dagger positions are center-based (matching the Godot Sprite2D convention).

### Step 4: Update -- spin the dagger

Each frame, increment the rotation. The hitbox position never changes because daggers are stationary:

```c
static void update_dagger(ObstacleEntity *e, float dt) {
  e->rotation += DAGGER_ROTATION_SPEED * dt;
  e->hitbox = aabb_from_center(e->x, e->y,
                                DAGGER_HITBOX_W * 0.5f, DAGGER_HITBOX_H * 0.5f);
}
```

### Step 5: Implement `sprite_blit_rotated` in `utils/sprite.c`

The rotation algorithm iterates over every pixel in the output bounding box. For each output pixel, it applies the inverse rotation to find the corresponding source pixel:

```c
void sprite_blit_rotated(Backbuffer *bb, const Sprite *sprite, SpriteRect src,
                         int dst_cx, int dst_cy, float angle) {
  float cos_a = cosf(angle);
  float sin_a = sinf(angle);

  /* Source center */
  float scx = (float)src.w * 0.5f;
  float scy = (float)src.h * 0.5f;

  /* Output bounding box from rotated corners */
  float abs_cos = cos_a < 0 ? -cos_a : cos_a;
  float abs_sin = sin_a < 0 ? -sin_a : sin_a;
  int out_w = (int)((float)src.w * 0.5f * abs_cos +
                    (float)src.h * 0.5f * abs_sin + 1.0f) * 2;
  int out_h = (int)((float)src.w * 0.5f * abs_sin +
                    (float)src.h * 0.5f * abs_cos + 1.0f) * 2;

  int out_x0 = dst_cx - out_w / 2;
  int out_y0 = dst_cy - out_h / 2;
```

The inner loop performs the inverse rotation per pixel:

```c
  for (int oy = 0; oy < out_h; oy++) {
    int py = out_y0 + oy;
    if (py < 0 || py >= bb->height) continue;

    for (int ox = 0; ox < out_w; ox++) {
      int px = out_x0 + ox;
      if (px < 0 || px >= bb->width) continue;

      /* Inverse rotation: find source pixel */
      float rx = (float)(ox - out_w / 2);
      float ry = (float)(oy - out_h / 2);
      float sx_f = cos_a * rx + sin_a * ry + scx;
      float sy_f = -sin_a * rx + cos_a * ry + scy;

      int sx = (int)sx_f;
      int sy = (int)sy_f;
      if (sx < 0 || sx >= src.w || sy < 0 || sy >= src.h) continue;

      uint32_t pixel = sprite->pixels[(src.y + sy) * sprite->width + src.x + sx];
      unsigned int a = (pixel >> 24) & 0xFF;
      if (a >= 128) {
        bb->pixels[py * (bb->pitch / 4) + px] = pixel;
      }
    }
  }
}
```

Key insight: the output bounding box is larger than the source sprite because rotating a square by 45 degrees makes it wider. The formula `half_w * |cos| + half_h * |sin|` gives the exact half-extent of the rotated rectangle.

### Step 6: Render spinning daggers

In `obstacles_render`, draw each dagger using the rotated blit. The full sprite is used as the source rectangle:

```c
case OBS_DAGGER_BLOCK_1:
case OBS_DAGGER_BLOCK_2:
  if (mgr->sprite_dagger.pixels) {
    SpriteRect full = {0, 0, mgr->sprite_dagger.width,
                        mgr->sprite_dagger.height};
    sprite_blit_rotated(bb, &mgr->sprite_dagger, full, sx, sy,
                        e->rotation);
  } else {
    draw_rect(bb, (float)sx - 8.0f, (float)sy - 8.0f, 16.0f, 16.0f,
              1.0f, 0.2f, 0.2f, 1.0f);
  }
  break;
```

The fallback red square ensures the game is playable even if the sprite fails to load.

## Common Mistakes

1. **Using forward rotation instead of inverse.** Forward mapping (source to destination) leaves gaps in the output because multiple source pixels can map to the same destination. Inverse mapping (destination to source) guarantees every output pixel is filled exactly once.

2. **Forgetting to offset dagger positions by block_x.** The positions in the const array are relative to the block origin. Without adding `block_x`, all daggers pile up at the left side of the world.

3. **Skipping the output bounding box calculation.** If you use the source sprite dimensions as the output size, a 45-degree rotation will crop the corners. The rotated bounding box is always >= the source dimensions.

4. **Using a hitbox centered on (0,0) instead of the dagger position.** Since dagger positions are center-based, you must use `aabb_from_center(e->x, e->y, ...)` not `aabb_make(e->x, e->y, ...)`.
