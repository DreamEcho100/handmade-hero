# Lesson 09 -- Ground Detection and Walk Cycle

## Observable Outcome
The slime transitions smoothly between ground and air states. When the player releases the action button while airborne, the slime falls and lands on the floor with no jitter at the floor boundary. On landing, the walk animation resumes immediately from where it left off. The is_on_floor and is_on_ceiling flags update correctly each frame, preventing physics edge cases at the boundaries. The PlayerState enum (NORMAL, DEAD, GAMEOVER) gates the entire update and render logic.

## New Concepts (max 3)
1. is_on_floor / is_on_ceiling flags derived from position + sprite height vs. boundary constants
2. PlayerState enum gating update and render behavior
3. Velocity zeroing on boundary contact to prevent accumulation artifacts

## Prerequisites
- Lesson 05 (flight controls and virt_speed accumulator)
- Lesson 08 (sprite animation system and ground/air frame selection)

## Background

Ground detection in a side-scroller seems trivial -- just check if the player's bottom edge is at or below the floor -- but the details matter. The floor check must run after position integration each frame, and it must set the is_on_floor flag that drives animation selection, physics branching, and the ground walk cycle. Getting the order wrong (checking before integration, or integrating after the check) produces one-frame jitter at the floor boundary where the player alternates between "on floor" and "falling" states.

The original Godot game uses CharacterBody2D.is_on_floor(), which is computed by the physics engine after move_and_slide(). We replicate this by checking the position after integration and clamping it to the boundary when exceeded. This is a "resolve after penetration" approach -- the simplest correct collision response for axis-aligned boundaries.

The PlayerState enum adds another layer of control. In PLAYER_STATE_NORMAL, the full physics model runs (flight, gravity, acceleration). In PLAYER_STATE_DEAD, the player falls with gravity and drifts forward at 75% speed. In PLAYER_STATE_GAMEOVER, the player is frozen. Each state has its own update and render behavior, preventing impossible state combinations.

## Code Walkthrough

### Step 1: The PlayerState enum

Defined in types.h:

```c
typedef enum {
  PLAYER_STATE_NORMAL,
  PLAYER_STATE_DEAD,
  PLAYER_STATE_GAMEOVER,
} PlayerState;
```

The player_update function is structured as a switch on this state:

```c
void player_update(Player *player, GameInput *input, float dt) {
  switch (player->state) {
  case PLAYER_STATE_NORMAL: {
    /* Full physics: flight, gravity, acceleration */
    /* ... */
    break;
  }
  case PLAYER_STATE_DEAD: {
    /* Falling with gravity, death animation */
    /* ... */
    break;
  }
  case PLAYER_STATE_GAMEOVER:
    /* Frozen -- waiting for game over handling */
    break;
  }
  /* Hurt timer runs in all states */
}
```

### Step 2: Floor detection -- the complete sequence

After position integration in PLAYER_STATE_NORMAL, the floor check runs:

```c
    /* Integrate position */
    player->x += player->speed * dt * dt;
    player->y += player->virt_speed * dt * dt;

    /* Floor collision: player bottom touches floor at y=165 */
    if (player->y + (float)PLAYER_H >= FLOOR_SURFACE_Y) {
      player->y = FLOOR_SURFACE_Y - (float)PLAYER_H;
      player->virt_speed = 0.0f;
      player->is_on_floor = 1;
    } else {
      player->is_on_floor = 0;
    }
```

Three things happen on floor contact:
1. **Position snap**: y is set to exactly FLOOR_SURFACE_Y - PLAYER_H (145.0). This prevents the player from sinking into the floor by even one pixel.
2. **Velocity zero**: virt_speed is reset to 0. Without this, the accumulator would hold whatever downward velocity was accumulated during the fall, causing a "sticky floor" effect where the next jump starts fighting residual downward momentum.
3. **Flag set**: is_on_floor = 1. This drives animation selection (walk vs. air) and physics branching (whether to apply gravity).

When the player is NOT touching the floor, is_on_floor = 0 unconditionally. This means the flag is recomputed every frame -- it is a derived state, not a latched flag.

### Step 3: Ceiling detection

Identical structure, checking the opposite boundary:

```c
    if (player->y < CEILING_Y) {
      player->y = CEILING_Y;
      player->virt_speed = 0.0f;
      player->is_on_ceiling = 1;
    } else {
      player->is_on_ceiling = 0;
    }
```

The ceiling at y=4 prevents the player from flying off the top of the screen. Zeroing virt_speed means the player does not "stick" to the ceiling -- they start falling immediately when they release the action button.

### Step 4: How the flags drive the flight physics

In Lesson 05, we saw the physics branching. Here is how is_on_floor gates the gravity branch:

```c
    if (!player->is_on_floor && !action_pressed) {
      /* Falling: ramp virt_speed toward +4500 */
      if (player->is_on_ceiling) {
        player->virt_speed = 0.0f;
      }
      player->virt_speed = clampf(
          player->virt_speed + PLAYER_VIRT_ACCEL * dt,
          PLAYER_JUMP_VEL, PLAYER_GRAVITY);
    }
```

When is_on_floor is true and action is not pressed, this branch does NOT run. No gravity accumulation on the ground. This is why the player stays perfectly still on the floor rather than oscillating between "falling one pixel into the floor" and "being snapped back up."

### Step 5: Launching from the floor

When the player presses action while on the floor:

```c
    if (action_pressed) {
      if (player->is_on_floor) {
        player->virt_speed = 0.0f;
      }
      player->virt_speed = clampf(
          player->virt_speed - PLAYER_VIRT_ACCEL * dt,
          PLAYER_JUMP_VEL, PLAYER_GRAVITY);
    }
```

The floor reset (`virt_speed = 0.0f`) ensures a clean launch. On the next frame, the integration `y += virt_speed * dt * dt` moves the player upward by a small amount. Since `y + PLAYER_H < FLOOR_SURFACE_Y` after that move, is_on_floor becomes 0, and the player is now airborne.

### Step 6: Landing -- walk cycle resumes

When the player lands, is_on_floor transitions from 0 to 1. The animation update in player_update only runs when on the floor:

```c
    if (player->is_on_floor) {
      anim_update(&player->anim_ground, dt);
    }
```

Because the animation's elapsed time was preserved while airborne (anim_update was not called), the walk cycle resumes from wherever it was when the player jumped. This creates a natural feel where the walk does not "reset" on every landing.

### Step 7: DEAD state ground detection

During death, the player falls with gravity and has their own floor check:

```c
  case PLAYER_STATE_DEAD: {
    if (!player->is_on_floor) {
      player->y += PLAYER_GRAVITY * dt * dt;
      player->x += player->speed * 0.75f * dt * dt;
    } else {
      anim_update(&player->anim_dead, dt);
      if (player->anim_dead.finished) {
        player->state = PLAYER_STATE_GAMEOVER;
      }
    }

    if (player->y + (float)PLAYER_H >= FLOOR_SURFACE_Y) {
      player->y = FLOOR_SURFACE_Y - (float)PLAYER_H;
      player->is_on_floor = 1;
    }

    player->end_timer -= dt;
    if (player->end_timer <= 0.0f) {
      player->state = PLAYER_STATE_GAMEOVER;
    }
    break;
  }
```

Two exit conditions lead to GAMEOVER: either the death animation finishes or the 3-second end_timer expires. The death sequence is: player gets hit, state becomes DEAD, player falls to floor, death animation plays (4 frames at 5fps = 0.8 seconds), state becomes GAMEOVER.

### Step 8: Rendering per state

```c
void player_render(const Player *player, Backbuffer *bb,
                   float camera_x) {
  int screen_x = (int)(player->x - camera_x);
  int screen_y = (int)player->y;

  switch (player->state) {
  case PLAYER_STATE_NORMAL:
    /* is_on_floor: walk anim; else: static air frame */
    break;
  case PLAYER_STATE_DEAD:
    /* !is_on_floor: falling frame; is_on_floor: death anim */
    break;
  case PLAYER_STATE_GAMEOVER:
    /* Last death frame frozen */
    break;
  }
}
```

Each state knows exactly which sprite and frame to display. The GAMEOVER state draws the last frame of the death animation, keeping the dead slime visible while the game over overlay appears.

### Step 9: Hitbox follows position

Regardless of state, the hitbox updates every frame:

```c
  player->hitbox.x = player->x + 2.0f;
  player->hitbox.y = player->y + 2.0f;
```

This runs outside the state switch, ensuring the hitbox is always valid for collision detection.

## Common Mistakes

**Checking floor BEFORE integration.** If you check `y + PLAYER_H >= FLOOR_SURFACE_Y` before applying `y += virt_speed * dt * dt`, the player can be one frame below the floor before being corrected. This causes a one-frame visual glitch where the sprite overlaps the floor.

**Latching is_on_floor instead of recomputing.** If you set `is_on_floor = 1` when landing but never set it back to 0, the player will be permanently "on floor" after the first landing. The flag must be recalculated from position every frame.

**Not zeroing virt_speed on floor contact during DEAD state.** In the death fall, if virt_speed is not zeroed on landing (the code uses position snapping only), the dead body may bounce or oscillate. The current code avoids this by not using virt_speed in the DEAD branch -- it applies PLAYER_GRAVITY directly.

**Resetting the walk animation on every landing.** Calling `anim_reset(&anim_ground)` when is_on_floor transitions from 0 to 1 would restart the walk from frame 0 every time. The current design lets the animation continue from its paused state, which feels more natural.

**Forgetting to handle GAMEOVER in the render switch.** Without a GAMEOVER case, the renderer would fall through (or hit a default that draws nothing), making the player disappear when they die. The GAMEOVER case draws the last death frame to keep the body visible.
