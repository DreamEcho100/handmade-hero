# Lesson 10 -- AABB Collision Detection

## Observable Outcome
The player now collides with a stationary test rectangle placed in the world. When the player's hitbox overlaps the test obstacle, the player flashes red and a debug message prints to the console. The test rectangle renders with a green outline when untouched and switches to red when the player is inside it. The AABB tests run at zero extra cost -- four comparisons per pair.

## New Concepts (max 3)
1. AABB struct with top-left origin (x, y, w, h) matching COORD_ORIGIN_LEFT_TOP
2. Overlap test via four separating-axis conditions
3. Near-miss detection by expanding the obstacle AABB by a margin

## Prerequisites
- Lesson 09 (player physics and camera follow working)
- Understanding of the coordinate system: y increases downward, (x, y) is the top-left corner

## Background

Every obstacle in the game needs to know whether it touches the player. The simplest collision shape for 2D pixel art is the Axis-Aligned Bounding Box (AABB) -- a rectangle whose edges are parallel to the screen axes. The original Godot game uses a mix of CircleShape2D and CapsuleShape2D colliders, but because our sprites are small (20x20 player, 22x22 daggers) the difference between a circle and an AABB at this scale is negligible. AABBs are faster and simpler.

Our AABB stores the top-left corner (x, y) plus width and height. This matches COORD_ORIGIN_LEFT_TOP -- the same convention every sprite uses for its position. We never need to convert between center-based and corner-based representations at the call site because aabb_from_center handles that conversion once.

The overlap test is the classic four-condition separating-axis check. Two boxes overlap if and only if neither is fully left-of, right-of, above, or below the other. We also add a point-in-box test (useful for mouse clicks later) and a near-miss test that will power the "+50 bonus" system in Lesson 16.

## Code Walkthrough

### Step 1: Define the AABB struct in `utils/collision.h`

Create a new header. The AABB carries four floats: top-left position and dimensions. All values are in world units (which are 1:1 with pixels at native resolution).

```c
#ifndef UTILS_COLLISION_H
#define UTILS_COLLISION_H

typedef struct {
  float x, y, w, h;
} AABB;
```

### Step 2: Constructor helpers

Two constructors cover both common cases. `aabb_make` takes top-left position directly (what sprites use). `aabb_from_center` takes a center point and half-extents (what CircleShape2D-style objects use).

```c
static inline AABB aabb_from_center(float cx, float cy, float hw, float hh) {
  AABB r = {cx - hw, cy - hh, hw * 2.0f, hh * 2.0f};
  return r;
}

static inline AABB aabb_make(float x, float y, float w, float h) {
  AABB r = {x, y, w, h};
  return r;
}
```

`aabb_from_center` subtracts the half-width/half-height from the center to find the top-left corner. This is used for daggers where the position from the .tscn file is the center of the sprite.

### Step 3: Overlap test -- the four-condition check

Two AABBs overlap when none of the four separation conditions holds. Written as the positive case: A's left edge is left of B's right edge, A's right edge is right of B's left edge, and likewise for top/bottom.

```c
static inline int aabb_overlap(AABB a, AABB b) {
  return (a.x < b.x + b.w) && (a.x + a.w > b.x) &&
         (a.y < b.y + b.h) && (a.y + a.h > b.y);
}
```

This uses strict less-than for the first/third conditions and strict greater-than for the second/fourth. Two boxes that are merely touching (edge-to-edge) return 0 -- they must share interior area to count as overlapping.

### Step 4: Point-in-box test

For completeness and future menu hit-testing, a point is inside the box if it falls within the half-open interval [x, x+w) on both axes:

```c
static inline int aabb_contains_point(AABB box, float px, float py) {
  return (px >= box.x) && (px < box.x + box.w) &&
         (py >= box.y) && (py < box.y + box.h);
}
```

### Step 5: Near-miss detection

The near-miss system awards bonus points when the player passes close to an obstacle without touching it. The algorithm: expand the obstacle box outward by `margin` pixels on every side, then check whether the player overlaps the expanded box but NOT the original box.

```c
static inline int aabb_near_miss(AABB entity, AABB obstacle, float margin) {
  AABB expanded = {
    obstacle.x - margin,
    obstacle.y - margin,
    obstacle.w + margin * 2.0f,
    obstacle.h + margin * 2.0f,
  };
  return aabb_overlap(entity, expanded) && !aabb_overlap(entity, obstacle);
}
```

If the entity is within the expanded zone but outside the real hitbox, that is a near miss. The margin value (12 pixels in the final game) controls how close "close" is.

### Step 6: Give the player a hitbox

In `player_init`, after setting the starting position, construct the player's hitbox. It is 2 pixels inset from the sprite edges on each side -- 16x16 effective hitbox inside a 20x20 sprite:

```c
player->hitbox = aabb_make(player->x + 2.0f, player->y + 2.0f,
                            (float)PLAYER_W - 4.0f, (float)PLAYER_H - 4.0f);
```

At the end of `player_update`, after position integration and floor/ceiling clamping, update the hitbox position to track the player:

```c
player->hitbox.x = player->x + 2.0f;
player->hitbox.y = player->y + 2.0f;
```

The size never changes -- only the position tracks the player each frame.

### Step 7: Test collision in `game_update`

Place a temporary test obstacle and check overlap each frame:

```c
AABB test_obstacle = aabb_make(300.0f, 100.0f, 30.0f, 30.0f);
if (aabb_overlap(state->player.hitbox, test_obstacle)) {
  printf("COLLISION!\n");
}
```

Render the test obstacle with a color that changes on collision:

```c
int hit = aabb_overlap(state->player.hitbox, test_obstacle);
float r = hit ? 1.0f : 0.0f;
float g = hit ? 0.0f : 1.0f;
draw_rect(bb, test_obstacle.x - camera_x, test_obstacle.y,
          test_obstacle.w, test_obstacle.h, r, g, 0.0f, 0.8f);
```

## Common Mistakes

1. **Forgetting to update the hitbox position each frame.** The AABB is a value struct, not a pointer to the player's position. If you set it once in init and never update it, it stays at the spawn position forever while the player scrolls away.

2. **Using `<=` instead of `<` in the overlap test.** With `<=`, two boxes that share an edge (touching but not overlapping) register as colliding. The frozen code uses strict inequalities so edge-touching returns false. This matters for the near-miss system which relies on the gap between "in expanded box" and "in real box".

3. **Mixing up center-based and corner-based positions.** Daggers use center positions from the .tscn files, so they need `aabb_from_center`. The player uses top-left position, so it uses `aabb_make`. Using the wrong constructor shifts the hitbox by half the sprite dimensions.

4. **Making the hitbox exactly the sprite size.** A pixel-perfect hitbox feels unfair to the player. The 2-pixel inset on each side gives a small grace margin that makes the game feel more forgiving without being visually noticeable.
