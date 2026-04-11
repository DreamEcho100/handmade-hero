# Lesson 06 -- Camera Follow and Scrolling

## Observable Outcome
The player stays at a fixed screen x-position (80 pixels from the left edge) while the world scrolls rightward. The scroll starts at a gentle pace (matching the player's initial speed of 2000 units/sec) and accelerates over time toward 6000 units/sec. The camera never scrolls left of world x=0. All entities (floor, obstacles, background) move in world coordinates and are rendered relative to the camera.

## New Concepts (max 3)
1. Camera follow with fixed offset: `camera.x = player.x - CAMERA_OFFSET_X`
2. Screen-space rendering: `screen_x = world_x - camera_x`
3. Auto-scroll acceleration driven by player speed

## Prerequisites
- Lesson 05 (flight controls with horizontal speed acceleration)

## Background

In a side-scrolling game, the player moves in an infinitely long world but the camera shows only a 320-pixel-wide window. The standard pattern is to lock the camera to the player with an offset: the player always appears at a fixed horizontal screen position while the world scrolls past.

We set CAMERA_OFFSET_X to 80 pixels, placing the player one-quarter from the left edge. This gives the player plenty of forward visibility to react to incoming obstacles. The camera's x position is simply `player.x - 80`. Since the player's x increases every frame (auto-scroll), the camera scrolls automatically.

The camera does not follow the player's y position. In this game, vertical movement happens entirely within the viewport (the play area is only 180 pixels tall). The camera y stays at 0 -- what you see is the full vertical extent of the world.

When rendering any world-space entity, we subtract camera.x from its world x to get the screen position. Entities to the left of the camera have negative screen x and are clipped by the sprite blit function. Entities beyond the right edge are similarly clipped. This is the standard 2D scrolling pattern used in virtually every side-scroller.

## Code Walkthrough

### Step 1: GameCamera struct

Defined in game/base.h, the camera tracks position and zoom:

```c
typedef struct {
  float x;    /* world x of viewport left edge */
  float y;    /* world y of viewport top edge (y-down) */
  float zoom; /* 1.0 = normal */
} GameCamera;
```

The camera is stored in GameState:

```c
typedef struct {
  GamePhase phase;
  GameCamera camera;
  Player player;
  /* ... */
} GameState;
```

### Step 2: Camera initialization

In game_init, the camera starts at the origin:

```c
void game_init(GameState *state, PlatformGameProps *props) {
  /* ... */
  state->camera.x = 0.0f;
  state->camera.y = 0.0f;
  state->camera.zoom = 1.0f;
  /* ... */
}
```

### Step 3: Camera follow in game_update

Each frame during GAME_PHASE_PLAYING, the camera tracks the player:

```c
case GAME_PHASE_PLAYING: {
    player_update(&state->player, input, dt);

    /* Camera follows player X with offset */
    state->camera.x = state->player.x - CAMERA_OFFSET_X;
    if (state->camera.x < 0.0f) state->camera.x = 0.0f;
    /* ... */
}
```

The clamp to 0 prevents the camera from showing negative world space at game start. CAMERA_OFFSET_X is defined in types.h:

```c
#define CAMERA_OFFSET_X 80.0f
```

### Step 4: Syncing camera to the platform world config

The Raylib backend syncs the game camera to the platform's GameWorldConfig each frame. This allows the render context system to account for camera position:

```c
/* In platforms/raylib/main.c, main loop: */
props.world_config.camera_x = game_state.camera.x;
props.world_config.camera_y = game_state.camera.y;
props.world_config.camera_zoom = game_state.camera.zoom;
```

However, the game's actual rendering bypasses the render context for direct pixel-space drawing. The camera offset is applied manually at each draw call.

### Step 5: World-to-screen conversion in player_render

The player render function converts world position to screen position by subtracting camera.x:

```c
void player_render(const Player *player, Backbuffer *bb,
                   float camera_x) {
  int screen_x = (int)(player->x - camera_x);
  int screen_y = (int)player->y;
  /* ... sprite_blit at (screen_x, screen_y) ... */
}
```

Since `camera.x = player.x - 80`, the screen_x computes to:
```
screen_x = player.x - (player.x - 80) = 80
```

The player always appears at screen x=80, regardless of how far they have traveled in world space.

Note that screen_y does NOT subtract camera.y because the camera's y is always 0. The full vertical world is always visible.

### Step 6: Auto-scroll acceleration

The scroll speed is implicit in the player's horizontal speed. In player_update:

```c
player->speed = clampf(player->speed + PLAYER_ACCEL * dt,
                       PLAYER_MIN_SPEED, PLAYER_MAX_SPEED);
player->x += player->speed * dt * dt;
```

The player starts at 2000 units/sec and accelerates by 250 units/sec^2. The camera follows, so the scroll accelerates at the same rate. At the double-delta scale, this produces a gradual, barely-noticeable acceleration that makes the game feel progressively more urgent.

### Step 7: Rendering other entities relative to camera

Every entity that exists in world space must subtract camera_x when rendering. The floor is an exception -- it is always the full screen width:

```c
static void render_floor(Backbuffer *bb) {
  draw_rect(bb, 0.0f, FLOOR_RENDER_Y, (float)bb->width,
            (float)bb->height - FLOOR_RENDER_Y,
            0.15f, 0.10f, 0.20f, 1.0f);
}
```

The floor renders at screen coordinates (0, 165) and fills to the bottom. It does not scroll because it has no world x-position -- it is always under the player.

Obstacles (Lesson 10+) use the same pattern as the player:
```c
int screen_x = (int)(entity.x - camera_x);
```

## Common Mistakes

**Forgetting to subtract camera_x when rendering.** If you draw a world entity at its raw world.x position, it will fly off the right edge of the screen within seconds as the player scrolls forward. Every world-positioned draw call must convert to screen space.

**Applying camera follow to the y-axis.** This game's play area fits within the 180-pixel viewport. If you follow the player's y, the floor and ceiling will shift on screen as the player flies, which breaks the visual design.

**Not clamping camera.x to zero at game start.** The player starts at x=80, so `camera.x = 80 - 80 = 0`. But if PLAYER_START_X were less than CAMERA_OFFSET_X, the camera would go negative, revealing empty space to the left. The clamp prevents this.

**Mixing up world coordinates and screen coordinates in collision detection.** Collisions happen in world space (both entities share the same coordinate system). Only rendering converts to screen space. If you subtract camera_x during collision checks, entities will stop colliding once you scroll far enough.
