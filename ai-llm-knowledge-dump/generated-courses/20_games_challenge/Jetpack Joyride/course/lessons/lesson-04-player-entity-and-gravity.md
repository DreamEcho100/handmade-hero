# Lesson 04 -- Player Entity and Gravity

## Observable Outcome
The slime character appears at position (80, 145) and falls under gravity. It lands on the floor at y=165, snapping to the ground and stopping. The slime sits still on the floor surface with no jitter or visual overlap.

## New Concepts (max 3)
1. Player struct with position, physics state, and sprite references
2. Godot 4 double-delta physics model: `position += speed * dt * dt`
3. Floor collision detection using FLOOR_SURFACE_Y and PLAYER_H

## Prerequisites
- Lesson 03 (sprite blitting and atlas frame extraction)

## Background

The original SlimeEscape was built in Godot 4.3 using CharacterBody2D and GDScript. To faithfully reproduce its physics in our C software renderer, we need to understand exactly how Godot's `move_and_slide()` interacts with the GDScript code.

In Godot 4, `CharacterBody2D.velocity` is measured in pixels per second. The engine's `move_and_slide()` internally does `position += velocity * delta`. The original GDScript sets `velocity.x = speed * delta` (not just `speed`). This means the net displacement per physics frame is `speed * delta * delta` -- a double-delta model. The constants (PLAYER_MIN_SPEED=2000, PLAYER_MAX_SPEED=6000) were tuned for this double-delta behavior. If you use single-delta (`position += speed * dt`), the character will move absurdly fast because the constants assume the extra delta multiplication.

This is not a bug in the original game -- it is a quirk of how the GDScript author wrote the velocity assignment. We replicate it exactly so that all the tuned constants (gravity=4500, jump_vel=-4500, accel=250) produce the same feel.

The floor is positioned at y=165 in world coordinates. Since the player sprite is 20 pixels tall, the player's top-left y must be at most 145 (165 - 20) to sit exactly on the floor surface.

## Code Walkthrough

### Step 1: World constants in types.h

All physics constants are defined in one place, with comments tracing back to the original GDScript:

```c
#define FLOOR_SURFACE_Y 165.0f  /* Floor StaticBody2D at y=165 */
#define CEILING_Y       4.0f    /* Ceiling StaticBody2D at y=4 */
#define PLAYER_START_X  80.0f
#define PLAYER_START_Y  145.0f  /* Player top-left (bottom at 165 = floor) */
#define FLOOR_Y         (FLOOR_SURFACE_Y - (float)PLAYER_H)

#define PLAYER_MIN_SPEED   2000.0f  /* Initial horizontal speed */
#define PLAYER_MAX_SPEED   6000.0f  /* Maximum horizontal speed */
#define PLAYER_ACCEL       250.0f   /* Horizontal acceleration */
#define PLAYER_GRAVITY     4500.0f  /* Downward acceleration */
#define PLAYER_JUMP_VEL   -4500.0f  /* Upward velocity clamp */
#define PLAYER_VIRT_ACCEL  12000.0f /* Vertical acceleration rate */

#define PLAYER_W 20
#define PLAYER_H 20
```

### Step 2: The Player struct

The Player struct bundles position, physics, state flags, collision box, and sprite assets:

```c
typedef struct {
  float x, y;             /* Top-left of sprite in world coordinates */
  float speed;            /* Current horizontal speed (units/sec) */
  float virt_speed;       /* Vertical speed accumulator */

  PlayerState state;      /* NORMAL, DEAD, GAMEOVER */
  int is_on_floor;
  int is_on_ceiling;
  int has_shield;

  AABB hitbox;            /* Collision box (2px inset from sprite) */

  SpriteSheet sheet;      /* Slime.png atlas */
  SpriteAnimation anim_ground;  /* 2-frame walk at 3fps */
  Sprite sprite_slime;          /* Raw pixel data */
  /* ... death sprites, distance tracking ... */
} Player;
```

### Step 3: player_init

Initialization sets the starting position, loads sprites, and configures the hitbox:

```c
int player_init(Player *player) {
  memset(player, 0, sizeof(*player));

  player->x = PLAYER_START_X;      /* 80.0 */
  player->y = PLAYER_START_Y;      /* 145.0 */
  player->speed = PLAYER_MIN_SPEED; /* 2000.0 */
  player->virt_speed = 0.0f;
  player->state = PLAYER_STATE_NORMAL;
  player->is_on_floor = 1;

  if (sprite_load("assets/sprites/Slime.png",
                   &player->sprite_slime) != 0) {
    fprintf(stderr, "Failed to load Slime.png\n");
    return -1;
  }
  spritesheet_init(&player->sheet, player->sprite_slime,
                   PLAYER_W, PLAYER_H);

  /* Hitbox: 2px inset on all sides for forgiving collision */
  player->hitbox = aabb_make(player->x + 2.0f, player->y + 2.0f,
                              (float)PLAYER_W - 4.0f,
                              (float)PLAYER_H - 4.0f);
  return 0;
}
```

The hitbox is 16x16, inset 2 pixels from the 20x20 sprite edges. This makes collisions feel fair -- the player can visually graze an obstacle without dying.

### Step 4: The double-delta physics integration

In player_update, position integration uses `dt * dt`:

```c
/* Integrate position -- Godot's move_and_slide() applies velocity * delta.
 * The GDScript sets velocity = speed * delta, so net = speed * dt * dt. */
player->x += player->speed * dt * dt;
player->y += player->virt_speed * dt * dt;
```

Velocity and acceleration updates use single-delta (standard):

```c
player->speed = clampf(player->speed + PLAYER_ACCEL * dt,
                       PLAYER_MIN_SPEED, PLAYER_MAX_SPEED);
```

At 60fps (dt ~= 0.0167), horizontal displacement per frame = 2000 * 0.0167 * 0.0167 ~= 0.56 pixels. This feels like a gentle scroll that accelerates over time.

### Step 5: Floor collision

After position integration, we check if the player's bottom edge has passed the floor:

```c
if (player->y + (float)PLAYER_H >= FLOOR_SURFACE_Y) {
  player->y = FLOOR_SURFACE_Y - (float)PLAYER_H;
  player->virt_speed = 0.0f;
  player->is_on_floor = 1;
} else {
  player->is_on_floor = 0;
}
```

This snaps the player to the floor surface and zeroes vertical velocity. The `is_on_floor` flag will drive animation selection in Lesson 08.

### Step 6: Hitbox update

At the end of each frame, the hitbox follows the player:

```c
player->hitbox.x = player->x + 2.0f;
player->hitbox.y = player->y + 2.0f;
```

## Common Mistakes

**Using single-delta physics (`position += speed * dt`).** With PLAYER_MIN_SPEED=2000 and single-delta, the player moves 2000*0.0167 = 33 pixels per frame -- about 2000 pixels per second. The game becomes unplayably fast. The double-delta model yields ~0.56 pixels per frame, which is correct.

**Placing PLAYER_START_Y at FLOOR_SURFACE_Y.** The player position is the top-left corner, not the bottom. Starting at y=165 means the bottom of the sprite is at y=185, which is 20 pixels below the floor. You must subtract PLAYER_H: `PLAYER_START_Y = FLOOR_SURFACE_Y - PLAYER_H = 145`.

**Not zeroing virt_speed on floor collision.** Without this reset, the vertical accumulator keeps growing each frame the player is on the ground. The next time they jump, they start with a large downward velocity that fights the upward thrust.

**Forgetting memset in player_init.** If the Player struct has uninitialized fields (flags, timers), you get unpredictable behavior on the first frame. The `memset(player, 0, sizeof(*player))` ensures all fields start at zero before explicit initialization.
