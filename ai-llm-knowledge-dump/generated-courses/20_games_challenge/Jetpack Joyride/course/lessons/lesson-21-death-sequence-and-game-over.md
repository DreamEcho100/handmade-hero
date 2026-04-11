# Lesson 21 -- Death Sequence and Game Over

## Observable Outcome
When the player hits an obstacle without a shield, they enter a death state: the slime falls to the floor under gravity while drifting forward at 75% speed. On hitting the floor, a 4-frame death animation plays at 5 FPS. After the animation finishes (or a 3-second timeout), a semi-transparent dark overlay covers the screen with "GAME OVER" in red, the final score in gold, and "Press SPACE to restart" in gray-blue.

## New Concepts (max 3)
1. Multi-phase death state machine: NORMAL to DEAD (fall + drift) to GAMEOVER (frozen + overlay)
2. Play-once animation with a `finished` flag that triggers the next state transition
3. Game over overlay rendering in screen space (no camera offset, drawn on top of everything)

## Prerequisites
- Lesson 05 (player physics: gravity, floor collision)
- Lesson 08 (SpriteAnimation: anim_init, anim_update, anim_reset, finished flag)
- Lesson 10 (AABB collision detection triggering player_hurt)

## Background

A death sequence is more than just stopping the game. The transition from gameplay to game over needs to feel dramatic and give the player time to process what happened. In our implementation, death is a three-phase state machine: the player first falls out of the sky (DEAD phase, airborne), then plays a death animation on the floor (DEAD phase, grounded), then transitions to the frozen GAMEOVER state where the overlay appears.

The DEAD state sets `is_on_floor = 0` regardless of where the player was when they died. This forces the fall even if the player was running on the ground -- the death always begins with a dramatic drop. During the fall, the player drifts forward at 75% of their current speed, giving a sense of momentum carrying the corpse forward. The same double-delta physics model applies: `y += PLAYER_GRAVITY * dt * dt`, `x += speed * 0.75f * dt * dt`.

The 3-second end timer acts as a safety net. If the death animation somehow fails to complete (e.g., the player died at the exact floor height and the animation is already "finished" at frame 0), the timer ensures we always reach GAMEOVER. The transition happens when either `anim_dead.finished` becomes true OR `end_timer` reaches zero -- whichever comes first.

The game over overlay uses alpha-blended rectangles to darken the background. By drawing a black rectangle with 60% opacity over the entire screen, the gameplay scene remains visible but muted. Text is drawn on top in screen space (no camera offset) so it stays centered regardless of where the camera was when the player died.

## Code Walkthrough

### Step 1: PlayerState enum and death-related fields (types.h)

```c
typedef enum {
  PLAYER_STATE_NORMAL,
  PLAYER_STATE_DEAD,
  PLAYER_STATE_GAMEOVER,
} PlayerState;

typedef struct {
  /* ... position, physics ... */
  PlayerState state;
  int is_on_floor;
  float end_timer;   /* Seconds until game over after death */

  /* Animation */
  SpriteAnimation anim_dead; /* 4-frame death at 5fps, play-once */
  /* ... */
} Player;
```

### Step 2: Triggering death (player.c)

The `player_die` function sets up the death state:

```c
void player_die(Player *player) {
  player->state = PLAYER_STATE_DEAD;
  player->end_timer = 3.0f;
  player->is_on_floor = 0;  /* Force falling even if on ground */
  anim_reset(&player->anim_dead);
}
```

The `player_hurt` function is the entry point from collision detection. It checks for shield first:

```c
int player_hurt(Player *player) {
  if (player->hurted || player->state != PLAYER_STATE_NORMAL)
    return 0;

  player->hurted = 1;
  player->hurt_timer = 0.5f;

  if (player->has_shield) {
    player->has_shield = 0;
    return 0; /* Shield absorbed -- not dead */
  }

  player_die(player);
  return 1; /* Player died */
}
```

The `hurted` flag provides 500ms of invulnerability, preventing multiple hits in rapid succession from bypassing the shield check.

### Step 3: DEAD state update (player.c)

The DEAD state has two sub-phases: airborne (falling) and grounded (animating):

```c
case PLAYER_STATE_DEAD: {
  if (!player->is_on_floor) {
    /* Falling: gravity + forward drift at 75% speed */
    player->y += PLAYER_GRAVITY * dt * dt;
    player->x += player->speed * 0.75f * dt * dt;
  } else {
    /* On ground: play death animation */
    anim_update(&player->anim_dead, dt);
    if (player->anim_dead.finished) {
      player->state = PLAYER_STATE_GAMEOVER;
    }
  }

  /* Floor collision during fall */
  if (player->y + (float)PLAYER_H >= FLOOR_SURFACE_Y) {
    player->y = FLOOR_SURFACE_Y - (float)PLAYER_H;
    player->is_on_floor = 1;
  }

  /* Safety timeout */
  player->end_timer -= dt;
  if (player->end_timer <= 0.0f) {
    player->state = PLAYER_STATE_GAMEOVER;
  }
  break;
}
```

Floor collision uses the same check as normal gameplay: `player->y + PLAYER_H >= FLOOR_SURFACE_Y`. When the bottom of the sprite reaches the floor surface at y=165, the player snaps to the floor and `is_on_floor` becomes true.

### Step 4: Death animation setup (player.c)

The death spritesheet (Dead.png) is an 80x20 image: 4 columns of 20x20 frames. The animation plays once at 5 FPS:

```c
/* Load dead spritesheet */
sprite_load("assets/sprites/Dead.png", &player->sprite_dead);
spritesheet_init(&player->sheet_dead, player->sprite_dead, PLAYER_W, PLAYER_H);

/* 4 frames at 5fps, play-once (loop=0) */
anim_init(&player->anim_dead, &player->sheet_dead, 0, 4, 5.0f, 0);
```

At 5 FPS with 4 frames, the animation takes 0.8 seconds -- well within the 3-second timeout.

### Step 5: Death rendering (player.c)

During the death fall, we show the normal air frame. Once on the ground, we switch to the death animation frames. In GAMEOVER state, we freeze on the last death frame:

```c
case PLAYER_STATE_DEAD: {
  if (!player->is_on_floor) {
    SpriteRect rect = spritesheet_frame_rect(&player->sheet, 1);
    sprite_blit(bb, &player->sprite_slime, rect, screen_x, screen_y);
  } else {
    int frame = player->anim_dead.current_frame;
    SpriteRect rect = spritesheet_frame_rect(&player->sheet_dead, frame);
    sprite_blit(bb, &player->sprite_dead, rect, screen_x, screen_y);
  }
  break;
}

case PLAYER_STATE_GAMEOVER: {
  int last_frame = player->anim_dead.frame_count - 1;
  SpriteRect rect = spritesheet_frame_rect(&player->sheet_dead, last_frame);
  sprite_blit(bb, &player->sprite_dead, rect, screen_x, screen_y);
  break;
}
```

### Step 6: Game over overlay (game/main.c)

The GAMEOVER phase in game_render draws the overlay after all game elements:

```c
if (state->phase == GAME_PHASE_GAMEOVER) {
  /* Semi-transparent dark overlay */
  draw_rect(backbuffer, 0.0f, 0.0f,
            (float)backbuffer->width, (float)backbuffer->height,
            0.0f, 0.0f, 0.0f, 0.6f);

  /* "GAME OVER" in red, scale 2 */
  draw_text(backbuffer, 96.0f, 60.0f, 2, "GAME OVER",
            1.0f, 0.3f, 0.3f, 1.0f);

  /* Score in gold, centered */
  char buf[64];
  snprintf(buf, sizeof(buf), "Score: %d", state->score);
  int text_w = (int)strlen(buf) * 8;
  draw_text(backbuffer, (float)(backbuffer->width - text_w) * 0.5f, 90.0f,
            1, buf, 1.0f, 0.9f, 0.3f, 1.0f);

  /* Restart hint */
  draw_text(backbuffer, 64.0f, 120.0f, 1,
            "Press SPACE to restart", 0.7f, 0.7f, 0.8f, 1.0f);
}
```

The overlay uses pixel coordinates (not camera-relative), so it stays fixed on screen. The "GAME OVER" text at scale 2 renders as 16x16 pixels per character. The score is horizontally centered by computing `(screen_width - text_pixel_width) / 2`.

### Step 7: GamePhase transition (game/main.c)

The game update loop detects the player reaching GAMEOVER state and transitions the game phase:

```c
if (state->player.state == PLAYER_STATE_GAMEOVER) {
  state->phase = GAME_PHASE_GAMEOVER;
}
```

## Common Mistakes

**Using memset to reset the game state on restart.** A memset of the entire GameState struct zeroes out the loaded sprite and audio pointers, causing crashes when the game tries to draw or play sounds after restart. Only reset the gameplay fields (position, speed, score, etc.) while preserving resource pointers.

**Not resetting anim_dead before each death.** Without `anim_reset(&player->anim_dead)` in `player_die`, the animation starts from wherever it stopped last time. If the player died once before, the animation's `finished` flag is still true, causing an instant transition to GAMEOVER with no visible death animation.

**Applying camera offset to the game over overlay.** The overlay must be in screen space. Drawing "GAME OVER" at `96.0f - camera_x` makes the text slide off-screen as the camera moves. The HUD and overlay coordinates are absolute pixel positions within the backbuffer.

**Checking `anim_dead.finished` without also checking `end_timer`.** If you only use the animation finished flag, a rare edge case where the player dies at exactly the floor height could skip the animation entirely. The timer provides a guaranteed fallback path to GAMEOVER.
