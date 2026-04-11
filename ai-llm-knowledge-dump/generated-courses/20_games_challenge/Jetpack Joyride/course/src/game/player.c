/* ═══════════════════════════════════════════════════════════════════════════
 * game/player.c — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Player physics faithfully adapted from slimeTest.gd.
 *
 * Key physics model:
 *   virtSpeed accumulates in [-4500, +4500] via virtAccel (12000/sec).
 *   When action pressed: virtSpeed decreases (flies up).
 *   When released: virtSpeed increases (falls down).
 *   velocity.y = virtSpeed * delta (Godot CharacterBody2D style).
 *   velocity.x = speed * delta (auto-scroll, accelerating).
 *
 * Godot 4 physics model (DEFINITIVE):
 *   - CharacterBody2D.velocity is in pixels/second
 *   - move_and_slide() internally does: position += velocity * delta
 *   - The GDScript sets: velocity.x = speed * delta (NOT just speed!)
 *   - So net displacement = speed * delta * delta per physics frame
 *   - Constants (2000-6000) are tuned for this double-delta model
 *
 * In our C code: position += speed * dt * dt
 * Velocity/acceleration updates stay single dt: virt_speed += accel * dt
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./player.h"
#include "../utils/sprite.h"

#include <stdio.h>
#include <string.h>

/* ── Clamp helper ────────────────────────────────────────────────────── */
static inline float clampf(float val, float lo, float hi) {
  if (val < lo) return lo;
  if (val > hi) return hi;
  return val;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * player_init
 * ═══════════════════════════════════════════════════════════════════════════
 */
int player_init(Player *player) {
  memset(player, 0, sizeof(*player));

  player->x = PLAYER_START_X;
  player->y = PLAYER_START_Y;
  player->speed = PLAYER_MIN_SPEED;
  player->virt_speed = 0.0f;
  player->state = PLAYER_STATE_NORMAL;
  player->is_on_floor = 1;
  player->has_shield = 0;
  player->total_distance = 0.0f;

  /* Load slime spritesheet (40x40 atlas: 2 cols x 2 rows of 20x20) */
  if (sprite_load("assets/sprites/Slime.png", &player->sprite_slime) != 0) {
    fprintf(stderr, "Failed to load Slime.png\n");
    return -1;
  }
  spritesheet_init(&player->sheet, player->sprite_slime, PLAYER_W, PLAYER_H);

  /* Ground walk animation: frames 0-1 (bottom row), 3 fps, loop */
  anim_init(&player->anim_ground, &player->sheet, 2, 2, 3.0f, 1);

  /* Load dead spritesheet (80x20: 4 cols x 1 row of 20x20) */
  if (sprite_load("assets/sprites/Dead.png", &player->sprite_dead) != 0) {
    fprintf(stderr, "Failed to load Dead.png\n");
    return -1;
  }
  spritesheet_init(&player->sheet_dead, player->sprite_dead, PLAYER_W, PLAYER_H);

  /* Death animation: 4 frames at 5fps, play once */
  anim_init(&player->anim_dead, &player->sheet_dead, 0, 4, 5.0f, 0);

  /* Initial hitbox */
  player->hitbox = aabb_make(player->x + 2.0f, player->y + 2.0f,
                              (float)PLAYER_W - 4.0f, (float)PLAYER_H - 4.0f);

  return 0;
}

void player_free(Player *player) {
  sprite_free(&player->sprite_slime);
  sprite_free(&player->sprite_dead);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * player_update — one frame of physics
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Faithfully replicates the _physics_process from slimeTest.gd.
 */
void player_update(Player *player, GameInput *input, float dt) {
  switch (player->state) {
  case PLAYER_STATE_NORMAL: {
    int action_pressed = input->buttons.action.ended_down;

    /* Vertical physics — accumulate virtSpeed */
    if (!player->is_on_floor && !action_pressed) {
      /* Falling: virtSpeed increases (gravity) */
      if (player->is_on_ceiling) {
        player->virt_speed = 0.0f;
      }
      player->virt_speed = clampf(
          player->virt_speed + PLAYER_VIRT_ACCEL * dt,
          PLAYER_JUMP_VEL, PLAYER_GRAVITY);
    }

    if (action_pressed) {
      /* Flying: virtSpeed decreases (thrust) */
      if (player->is_on_floor) {
        player->virt_speed = 0.0f;
      }
      player->virt_speed = clampf(
          player->virt_speed - PLAYER_VIRT_ACCEL * dt,
          PLAYER_JUMP_VEL, PLAYER_GRAVITY);
    }

    /* Horizontal: accelerate up to max speed */
    player->speed = clampf(player->speed + PLAYER_ACCEL * dt,
                           PLAYER_MIN_SPEED, PLAYER_MAX_SPEED);

    /* Integrate position — Godot's move_and_slide() applies velocity * delta.
     * The GDScript sets velocity = speed * delta, so net = speed * dt * dt.
     * This is the definitive model matching the original game's constants.  */
    player->x += player->speed * dt * dt;
    player->y += player->virt_speed * dt * dt;

    /* Track distance */
    player->total_distance += player->speed * dt * dt;

    /* Floor collision: player bottom touches floor surface at y=165 */
    if (player->y + (float)PLAYER_H >= FLOOR_SURFACE_Y) {
      player->y = FLOOR_SURFACE_Y - (float)PLAYER_H;
      player->virt_speed = 0.0f;
      player->is_on_floor = 1;
    } else {
      player->is_on_floor = 0;
    }

    if (player->y < CEILING_Y) {
      player->y = CEILING_Y;
      player->virt_speed = 0.0f;
      player->is_on_ceiling = 1;
    } else {
      player->is_on_ceiling = 0;
    }

    /* Update animation */
    if (player->is_on_floor) {
      anim_update(&player->anim_ground, dt);
    }

    break;
  }

  case PLAYER_STATE_DEAD: {
    if (!player->is_on_floor) {
      /* Fall with gravity, drift forward at 75% speed — same dt*dt model */
      player->y += PLAYER_GRAVITY * dt * dt;
      player->x += player->speed * 0.75f * dt * dt;
    } else {
      /* Stopped on ground, play death animation */
      anim_update(&player->anim_dead, dt);
      if (player->anim_dead.finished) {
        player->state = PLAYER_STATE_GAMEOVER;
      }
    }

    /* Floor collision during death fall */
    if (player->y + (float)PLAYER_H >= FLOOR_SURFACE_Y) {
      player->y = FLOOR_SURFACE_Y - (float)PLAYER_H;
      player->is_on_floor = 1;
    }

    /* End timer */
    player->end_timer -= dt;
    if (player->end_timer <= 0.0f) {
      player->state = PLAYER_STATE_GAMEOVER;
    }

    break;
  }

  case PLAYER_STATE_GAMEOVER:
    /* Frozen — waiting for game over screen handling */
    break;
  }

  /* Hurt invulnerability timer */
  if (player->hurted) {
    player->hurt_timer -= dt;
    if (player->hurt_timer <= 0.0f) {
      player->hurted = 0;
    }
  }

  /* Update hitbox position */
  player->hitbox.x = player->x + 2.0f;
  player->hitbox.y = player->y + 2.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * player_render
 * ═══════════════════════════════════════════════════════════════════════════
 */
void player_render(const Player *player, Backbuffer *bb, float camera_x) {
  int screen_x = (int)(player->x - camera_x);
  int screen_y = (int)player->y;

  switch (player->state) {
  case PLAYER_STATE_NORMAL: {
    if (player->is_on_floor) {
      /* Ground walk animation */
      int frame = player->anim_ground.start_frame +
                  player->anim_ground.current_frame;
      SpriteRect rect = spritesheet_frame_rect(&player->sheet, frame);
      sprite_blit(bb, &player->sprite_slime, rect, screen_x, screen_y);
    } else {
      /* Air: frame 0 if action pressed, frame 1 if falling */
      /* We use frames from top row: 0 = flying up, 1 = falling */
      int frame = (player->virt_speed < 0.0f) ? 0 : 1;
      SpriteRect rect = spritesheet_frame_rect(&player->sheet, frame);
      sprite_blit(bb, &player->sprite_slime, rect, screen_x, screen_y);
    }
    break;
  }

  case PLAYER_STATE_DEAD: {
    if (!player->is_on_floor) {
      /* Falling: air frame 1 */
      SpriteRect rect = spritesheet_frame_rect(&player->sheet, 1);
      sprite_blit(bb, &player->sprite_slime, rect, screen_x, screen_y);
    } else {
      /* On ground: death animation */
      int frame = player->anim_dead.current_frame;
      SpriteRect rect = spritesheet_frame_rect(&player->sheet_dead, frame);
      sprite_blit(bb, &player->sprite_dead, rect, screen_x, screen_y);
    }
    break;
  }

  case PLAYER_STATE_GAMEOVER: {
    /* Show last death frame */
    int last_frame = player->anim_dead.frame_count - 1;
    SpriteRect rect = spritesheet_frame_rect(&player->sheet_dead, last_frame);
    sprite_blit(bb, &player->sprite_dead, rect, screen_x, screen_y);
    break;
  }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * player_hurt / player_die
 * ═══════════════════════════════════════════════════════════════════════════
 */
int player_hurt(Player *player) {
  if (player->hurted || player->state != PLAYER_STATE_NORMAL)
    return 0;

  player->hurted = 1;
  player->hurt_timer = 0.5f; /* 500ms invulnerability */

  if (player->has_shield) {
    player->has_shield = 0;
    return 0; /* Shield absorbed the hit */
  }

  player_die(player);
  return 1; /* Player died */
}

void player_die(Player *player) {
  player->state = PLAYER_STATE_DEAD;
  player->end_timer = 3.0f; /* 3 seconds until game over */
  player->is_on_floor = 0;  /* Start falling */
  anim_reset(&player->anim_dead);
}
