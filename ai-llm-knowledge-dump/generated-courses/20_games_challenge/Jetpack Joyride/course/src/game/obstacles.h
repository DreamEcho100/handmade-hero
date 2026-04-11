#ifndef GAME_OBSTACLES_H
#define GAME_OBSTACLES_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/obstacles.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Obstacle spawning, state machine updates, rendering, and collision.
 * Manages all 6 obstacle types: daggers, missiles, sigils.
 * Also manages pellets and shield pickups.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./types.h"
#include "./base.h"
#include "../utils/backbuffer.h"
#include "../utils/rng.h"

/* ── Obstacle Manager ────────────────────────────────────────────────── */
/* Central state for obstacle spawning and lifecycle management.          */
typedef struct {
  ObstacleBlock blocks[MAX_OBSTACLE_BLOCKS];
  int block_count;

  Pellet pellets[MAX_PELLETS];
  int pellet_count;

  ShieldPickup shields[MAX_SHIELD_PICKUPS];
  float shield_timer;     /* Countdown to next shield spawn */

  NearMissIndicator near_misses[MAX_NEAR_MISS_INDICATORS];

  ObstacleType last_spawned_type;
  float next_spawn_x;    /* World X where next block should spawn */

  RNG rng;               /* Obstacle variety RNG */
  RNG effects_rng;       /* Visual effects RNG (entropy isolated) */

  /* Sprite assets */
  Sprite sprite_dagger;
  Sprite sprite_missile;
  Sprite sprite_warning;
  Sprite sprite_boom;
  Sprite sprite_sigil;
  Sprite sprite_sigil_wait;
  Sprite sprite_pellet;
  Sprite sprite_shield;
  Sprite sprite_near;

  SpriteSheet sheet_pellet;    /* 4-frame pellet animation */
  SpriteSheet sheet_boom;      /* 4-5 frame missile explosion */
  SpriteSheet sheet_missile;   /* 3-frame missile animation */
  SpriteSheet sheet_warning;   /* 2-frame warning animation */
  SpriteSheet sheet_sigil;     /* multi-frame sigil fire animation */
  SpriteSheet sheet_sigil_wait; /* multi-frame sigil wait animation */
  SpriteSheet sheet_shield;    /* 2-frame shield animation */
} ObstacleManager;

/* Initialize the obstacle manager. Load sprites and seed RNG. */
int obstacles_init(ObstacleManager *mgr);

/* Free all obstacle sprites. */
void obstacles_free(ObstacleManager *mgr);

/* Update all obstacles: spawn new blocks, advance state machines,
 * despawn off-screen blocks. player_x/y used for spawning and tracking. */
void obstacles_update(ObstacleManager *mgr, float player_x, float player_y,
                      float player_speed, float camera_x, float dt);

/* Render all active obstacles. camera_x is the current camera offset. */
void obstacles_render(const ObstacleManager *mgr, Backbuffer *bb,
                      float camera_x);

/* Check collision between player hitbox and all active obstacles.
 * Returns 1 if player was hit, 0 otherwise.                            */
int obstacles_check_collision(ObstacleManager *mgr, AABB player_hitbox);

/* Check near-miss with all active obstacles. Returns number of near-misses. */
int obstacles_check_near_miss(ObstacleManager *mgr, AABB player_hitbox);

/* Check pellet collection. Returns number of pellets collected. */
int obstacles_collect_pellets(ObstacleManager *mgr, AABB player_hitbox);

/* Check shield pickup. Returns 1 if shield was collected. */
int obstacles_collect_shield(ObstacleManager *mgr, AABB player_hitbox);

#endif /* GAME_OBSTACLES_H */
