#ifndef GAME_TYPES_H
#define GAME_TYPES_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/types.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * All game-specific type definitions: Player, Obstacle, Pellet, Shield, etc.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "../utils/sprite.h"
#include "../utils/collision.h"

/* ── World constants ─────────────────────────────────────────────────── */
/* From original SlimeEscape (Godot 4.3, test.tscn): */
#define FLOOR_SURFACE_Y 165.0f /* Floor StaticBody2D at y=165 */
#define CEILING_Y       4.0f   /* Ceiling StaticBody2D at y=4 */
#define PLAYER_START_X  80.0f
#define PLAYER_START_Y  145.0f /* Player top-left (bottom at 165 = floor) */
/* Player top-left Y when standing: FLOOR_SURFACE_Y - PLAYER_H = 145 */
#define FLOOR_Y         (FLOOR_SURFACE_Y - (float)PLAYER_H)

/* Physics (from slimeTest.gd) */
#define PLAYER_MIN_SPEED   2000.0f  /* Initial horizontal speed (units/sec) */
#define PLAYER_MAX_SPEED   6000.0f  /* Maximum horizontal speed */
#define PLAYER_ACCEL       250.0f   /* Horizontal acceleration (units/sec²) */
#define PLAYER_GRAVITY     4500.0f  /* Downward acceleration when falling */
#define PLAYER_JUMP_VEL   -4500.0f  /* Upward velocity clamp */
#define PLAYER_VIRT_ACCEL  12000.0f /* Vertical acceleration rate */

/* Player sprite dimensions */
#define PLAYER_W 20
#define PLAYER_H 20

/* Camera */
#define CAMERA_OFFSET_X 80.0f  /* Player appears 80px from left edge */

/* ── Player state ────────────────────────────────────────────────────── */
typedef enum {
  PLAYER_STATE_NORMAL,
  PLAYER_STATE_DEAD,
  PLAYER_STATE_GAMEOVER,
} PlayerState;

typedef struct {
  /* Position (top-left of sprite in world coordinates) */
  float x, y;

  /* Physics */
  float speed;       /* Current horizontal speed (units/sec) */
  float virt_speed;  /* Vertical speed accumulator */

  /* State */
  PlayerState state;
  int is_on_floor;
  int is_on_ceiling;
  int has_shield;
  int hurted;        /* Invulnerability timer active */
  float hurt_timer;  /* Seconds remaining for invulnerability */
  float end_timer;   /* Seconds until game over after death */

  /* Collision */
  AABB hitbox;

  /* Sprites & Animation */
  SpriteSheet sheet;
  SpriteAnimation anim_ground; /* 2-frame walk at 3fps */
  SpriteAnimation anim_dead;   /* 4-frame death at 5fps */
  /* Air frames are static (no animation, just frame selection) */

  /* Sprite assets loaded separately */
  Sprite sprite_slime;     /* Slime.png — 40x40 (2x2 grid of 20x20) */
  Sprite sprite_dead;      /* Dead.png — 80x20 (4x1 grid of 20x20) */
  SpriteSheet sheet_dead;

  /* Distance tracking */
  float total_distance;    /* Total distance traveled (world units) */
} Player;

/* ── Obstacle types ──────────────────────────────────────────────────── */
typedef enum {
  OBS_NONE = 0,
  OBS_DAGGER_BLOCK_1,
  OBS_MISSILE_BLOCK_1,
  OBS_SIGIL_BLOCK_1,
  OBS_MISSILE_BLOCK_2,
  OBS_SIGIL_BLOCK_2,
  OBS_DAGGER_BLOCK_2,
  OBS_TYPE_COUNT
} ObstacleType;

/* Missile states (WARNING → FIRE → BOOM) */
typedef enum {
  MISSILE_WARNING,
  MISSILE_FIRE,
  MISSILE_BOOM,
} MissileState;

/* Sigil states (WAIT → CHARGE → FIRE → POWERDOWN) */
typedef enum {
  SIGIL_WAIT,
  SIGIL_CHARGE,
  SIGIL_FIRE,
  SIGIL_POWERDOWN,
} SigilState;

/* Individual sub-entity within an obstacle block */
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

/* An obstacle block — a group of sub-entities spawned together */
typedef struct {
  ObstacleType type;
  float x;              /* Block world X position */
  int active;
  float spawn_distance; /* How far ahead of player to spawn */

  ObstacleEntity entities[MAX_ENTITIES_PER_BLOCK];
  int entity_count;

  /* Sigil Block 2 coordination (fireOrder state machine from original) */
  int fire_order;       /* Current phase (0-4) */
  float block_timer;    /* Timer for phase transitions */

  /* Missile Block sequential spawning */
  int next_missile_idx; /* Next missile to spawn */
  float missile_timer;  /* Timer until next missile spawn */
} ObstacleBlock;

#define MAX_OBSTACLE_BLOCKS 4  /* At most 3-4 active blocks at once */

/* ── Pellet ──────────────────────────────────────────────────────────── */
typedef struct {
  float x, y;
  int active;
  AABB hitbox;
  SpriteAnimation anim; /* 4-frame spinning animation */
} Pellet;

#define MAX_PELLETS 64

/* ── Shield Pickup ───────────────────────────────────────────────────── */
typedef struct {
  float x, y;
  float vy;            /* Bounce velocity */
  int active;
  int bounce_dir;      /* 1 = down, -1 = up */
  AABB hitbox;
  SpriteAnimation anim;
} ShieldPickup;

#define MAX_SHIELD_PICKUPS 2

/* ── Near-miss indicator ─────────────────────────────────────────────── */
typedef struct {
  float x, y;
  float timer;         /* Display duration remaining */
  int active;
} NearMissIndicator;

#define MAX_NEAR_MISS_INDICATORS 4

#endif /* GAME_TYPES_H */
