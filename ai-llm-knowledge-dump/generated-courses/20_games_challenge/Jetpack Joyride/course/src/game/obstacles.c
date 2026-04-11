/* ═══════════════════════════════════════════════════════════════════════════
 * game/obstacles.c — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Obstacle spawning, state machines, rendering, and collision for all 6
 * obstacle types. Faithfully adapted from SlimeEscape GDScript sources.
 *
 * Spawn distances (from original):
 *   Dagger Block 1: 520px    Missile Block 1: 520px
 *   Sigil Block 1:  240px    Missile Block 2: 525px
 *   Sigil Block 2:  320px    Dagger Block 2:  360px
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./obstacles.h"
#include "../utils/base.h"
#include "../utils/collision.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../utils/sprite.h"

#include <stdio.h>
#include <string.h>

/* ── Spawn distance per block type ───────────────────────────────────── */
static const float SPAWN_DISTANCES[OBS_TYPE_COUNT] = {
  0.0f,   /* OBS_NONE */
  520.0f, /* OBS_DAGGER_BLOCK_1 */
  520.0f, /* OBS_MISSILE_BLOCK_1 */
  240.0f, /* OBS_SIGIL_BLOCK_1 */
  525.0f, /* OBS_MISSILE_BLOCK_2 */
  320.0f, /* OBS_SIGIL_BLOCK_2 */
  360.0f, /* OBS_DAGGER_BLOCK_2 */
};

/* ── Dagger layout positions (from dagger_block_1.tscn, exact) ───────── */
/* 18 daggers in 5 clusters of 3, with gaps for the player to navigate.  */
static const float DAGGER_BLOCK_1_POS[][2] = {
  /* Bottom-left cluster (y=135) */
  {71, 135}, {89, 135}, {107, 135},
  /* Middle-left cluster (y=85) */
  {153, 85}, {171, 85}, {189, 85},
  /* Top cluster (y=35) — 6 daggers spanning the middle */
  {231, 35}, {249, 35}, {267, 35}, {302, 35}, {320, 35}, {338, 35},
  /* Middle-right cluster (y=85) */
  {380, 85}, {398, 85}, {416, 85},
  /* Bottom-right cluster (y=135) */
  {462, 135}, {480, 135}, {498, 135},
};
#define DAGGER_BLOCK_1_COUNT 18

/* Dagger Block 1 pellets (16 pellets, from dagger_block_1.tscn) */
static const float DAGGER_BLOCK_1_PELLETS[][2] = {
  {81, 114}, {94, 107}, {107, 100},       /* left-bottom diagonal */
  {163, 64}, {176, 57}, {189, 50},        /* left-middle diagonal */
  {263, 86}, {277, 86}, {291, 86}, {305, 86}, /* center horizontal */
  {382, 50}, {395, 57}, {408, 64},        /* right-middle diagonal */
  {464, 100}, {477, 107}, {490, 114},     /* right-bottom diagonal */
};
#define DAGGER_BLOCK_1_PELLET_COUNT 16

/* ── Dagger Block 2 (from dagger_block_2.tscn, exact) ───────────────── */
/* 9 daggers in 3 vertical columns with gaps between them.               */
static const float DAGGER_BLOCK_2_POS[][2] = {
  /* Left column (x=40) */
  {40, 114}, {40, 130}, {40, 146},
  /* Middle column (x=145) */
  {145, 24}, {145, 40}, {145, 56},
  /* Right column (x=250) */
  {250, 114}, {250, 130}, {250, 146},
};
#define DAGGER_BLOCK_2_COUNT 9

/* Dagger Block 2 pellets (9 pellets, from dagger_block_2.tscn) */
static const float DAGGER_BLOCK_2_PELLETS[][2] = {
  {84, 66}, {92, 80}, {100, 94},          /* left triangle */
  {189, 94}, {197, 80}, {205, 66},        /* middle triangle */
  {320, 138}, {334, 138}, {348, 138},     /* right row */
};
#define DAGGER_BLOCK_2_PELLET_COUNT 9

/* ── Missile Block 2 pellets (21 pellets, from missile_block_2.tscn) ── */
static const float MISSILE_BLOCK_2_PELLETS[][2] = {
  {60, 143}, {73, 143}, {86, 143},        /* bottom row */
  {130, 108}, {143, 108}, {156, 108},     /* lower-middle */
  {200, 73}, {213, 73}, {226, 73},        /* middle left */
  {270, 38}, {283, 38}, {296, 38},        /* upper */
  {340, 73}, {353, 73}, {366, 73},        /* middle right */
  {410, 108}, {423, 108}, {436, 108},     /* upper-middle */
  {480, 143}, {493, 143}, {506, 143},     /* top row */
};
#define MISSILE_BLOCK_2_PELLET_COUNT 21

/* ── Missile Y positions ─────────────────────────────────────────────── */
static const float MISSILE_BLOCK_1_Y[] = {31.0f, 58.0f, 85.0f, 112.0f, 139.0f};
#define MISSILE_BLOCK_1_COUNT 5

/* ── Sigil Y positions for Block 2 ───────────────────────────────────── */
static const float SIGIL_BLOCK_2_Y[] = {31.0f, 139.0f, 58.0f, 112.0f, 85.0f};
#define SIGIL_BLOCK_2_COUNT 5

/* ── Dagger collision size (original: CircleShape2D radius=11) ────────── */
#define DAGGER_HITBOX_W 22.0f
#define DAGGER_HITBOX_H 22.0f
#define DAGGER_ROTATION_SPEED 20.0f /* radians per second */

/* ── Missile constants ───────────────────────────────────────────────── */
#define MISSILE_SPEED -250.0f
#define MISSILE_WARNING_DURATION 3.0f
#define MISSILE_HITBOX_W 6.0f   /* Original: CapsuleShape2D radius=3, height=17 */
#define MISSILE_HITBOX_H 17.0f
#define MISSILE_BLOCK_1_INTERVAL 0.75f /* Seconds between missile spawns */
#define MISSILE_BLOCK_2_INTERVAL 4.0f  /* Seconds between tracking missiles */

/* ── Sigil constants (from sigil_block_1.tscn / sigil_block_2.tscn) ── */
#define SIGIL_BLOCK_1_CHARGE_DURATION 2.5f
#define SIGIL_BLOCK_1_FIRE_DURATION   3.0f
#define SIGIL_BLOCK_2_CHARGE_DURATION 3.0f
#define SIGIL_BLOCK_2_FIRE_DURATION   3.0f
#define SIGIL_BEAM_W 300.0f
#define SIGIL_BEAM_H 8.0f

/* ── Shield constants ────────────────────────────────────────────────── */
#define SHIELD_SPAWN_OFFSET_X 320.0f
#define SHIELD_SPAWN_Y 84.0f
#define SHIELD_BOUNCE_MIN_Y 59.0f
#define SHIELD_BOUNCE_MAX_Y 109.0f

/* ── Pellet constants ────────────────────────────────────────────────── */
#define PELLET_HITBOX_W 10.0f
#define PELLET_HITBOX_H 10.0f

/* ── Near-miss margin ────────────────────────────────────────────────── */
#define NEAR_MISS_MARGIN 12.0f

/* ═══════════════════════════════════════════════════════════════════════════
 * obstacles_init
 * ═══════════════════════════════════════════════════════════════════════════
 */
int obstacles_init(ObstacleManager *mgr) {
  memset(mgr, 0, sizeof(*mgr));

  rng_seed(&mgr->rng, 42);
  rng_seed(&mgr->effects_rng, 137);

  mgr->next_spawn_x = PLAYER_START_X + 200.0f; /* First block spawns 200px ahead */
  mgr->shield_timer = rng_float_range(&mgr->rng, 20.0f, 40.0f);

  /* Load obstacle sprites */
  if (sprite_load("assets/sprites/Daggerpng.png", &mgr->sprite_dagger) != 0)
    fprintf(stderr, "WARNING: Failed to load Daggerpng.png\n");
  if (sprite_load("assets/sprites/MagicMissile.png", &mgr->sprite_missile) != 0)
    fprintf(stderr, "WARNING: Failed to load MagicMissile.png\n");
  if (sprite_load("assets/sprites/Warning.png", &mgr->sprite_warning) != 0)
    fprintf(stderr, "WARNING: Failed to load Warning.png\n");
  if (sprite_load("assets/sprites/boom.png", &mgr->sprite_boom) != 0)
    fprintf(stderr, "WARNING: Failed to load boom.png\n");
  if (sprite_load("assets/sprites/Sigil.png", &mgr->sprite_sigil) != 0)
    fprintf(stderr, "WARNING: Failed to load Sigil.png\n");
  if (sprite_load("assets/sprites/SigilWait.png", &mgr->sprite_sigil_wait) != 0)
    fprintf(stderr, "WARNING: Failed to load SigilWait.png\n");
  if (sprite_load("assets/sprites/Point.png", &mgr->sprite_pellet) != 0)
    fprintf(stderr, "WARNING: Failed to load Point.png\n");
  if (sprite_load("assets/sprites/ShieldPickup.png", &mgr->sprite_shield) != 0)
    fprintf(stderr, "WARNING: Failed to load ShieldPickup.png\n");
  if (sprite_load("assets/sprites/Near.png", &mgr->sprite_near) != 0)
    fprintf(stderr, "WARNING: Failed to load Near.png\n");

  /* Initialize spritesheet for pellet (4-frame, assume 40x10 or similar) */
  if (mgr->sprite_pellet.pixels) {
    int pw = mgr->sprite_pellet.width;
    int ph = mgr->sprite_pellet.height;
    /* Point.png is a single animated strip — detect layout */
    if (pw > ph) {
      /* Horizontal strip */
      int frame_w = pw / 4;
      spritesheet_init(&mgr->sheet_pellet, mgr->sprite_pellet, frame_w, ph);
    } else {
      spritesheet_init(&mgr->sheet_pellet, mgr->sprite_pellet, pw, ph);
    }
  }

  /* MagicMissile.png: 75x19 = 3 frames of 25x19 */
  if (mgr->sprite_missile.pixels) {
    spritesheet_init(&mgr->sheet_missile, mgr->sprite_missile, 25, 19);
  }

  /* Warning.png: 48x24 = 2 frames of 24x24 */
  if (mgr->sprite_warning.pixels) {
    spritesheet_init(&mgr->sheet_warning, mgr->sprite_warning, 24, 24);
  }

  /* Sigil.png: 310x68 — 4 rows of 310x17 (full-width beam frames).
   * Atlas: Rect2(0,0,310,17), Rect2(0,17,310,17), etc.
   * Each frame is the ENTIRE beam graphic (310px wide, 17px tall). */
  if (mgr->sprite_sigil.pixels) {
    spritesheet_init(&mgr->sheet_sigil, mgr->sprite_sigil, 310, 17);
  }

  /* SigilWait.png: 310x34 — 2 rows of 310x17 */
  if (mgr->sprite_sigil_wait.pixels) {
    spritesheet_init(&mgr->sheet_sigil_wait, mgr->sprite_sigil_wait, 310, 17);
  }

  /* ShieldPickup.png: 32x18 = 2 frames of 16x18 */
  if (mgr->sprite_shield.pixels) {
    spritesheet_init(&mgr->sheet_shield, mgr->sprite_shield, 16, 18);
  }

  /* Boom spritesheet (boom.png: 95x19 = 5 frames of 19x19) */
  if (mgr->sprite_boom.pixels) {
    int bw = mgr->sprite_boom.width;
    int bh = mgr->sprite_boom.height;
    if (bw > bh) {
      spritesheet_init(&mgr->sheet_boom, mgr->sprite_boom, bh, bh); /* Square frames */
    } else {
      spritesheet_init(&mgr->sheet_boom, mgr->sprite_boom, bw, bh);
    }
  }

  return 0;
}

void obstacles_free(ObstacleManager *mgr) {
  sprite_free(&mgr->sprite_dagger);
  sprite_free(&mgr->sprite_missile);
  sprite_free(&mgr->sprite_warning);
  sprite_free(&mgr->sprite_boom);
  sprite_free(&mgr->sprite_sigil);
  sprite_free(&mgr->sprite_sigil_wait);
  sprite_free(&mgr->sprite_pellet);
  sprite_free(&mgr->sprite_shield);
  sprite_free(&mgr->sprite_near);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * spawn_block — create a new obstacle block
 * ═══════════════════════════════════════════════════════════════════════════
 */
static ObstacleBlock *find_free_block(ObstacleManager *mgr) {
  for (int i = 0; i < MAX_OBSTACLE_BLOCKS; i++) {
    if (!mgr->blocks[i].active) return &mgr->blocks[i];
  }
  return NULL;
}

static void spawn_dagger_block(ObstacleBlock *block, const float positions[][2],
                                int count, float block_x, RNG *rng) {
  block->entity_count = count;
  for (int i = 0; i < count; i++) {
    ObstacleEntity *e = &block->entities[i];
    memset(e, 0, sizeof(*e));
    e->x = block_x + positions[i][0];
    e->y = positions[i][1];
    e->rotation = rng_float_range(rng, 0.0f, 6.2832f); /* Random start rotation */
    e->active = 1;
    e->hitbox = aabb_from_center(e->x, e->y,
                                  DAGGER_HITBOX_W * 0.5f, DAGGER_HITBOX_H * 0.5f);
  }
}

/* Missile blocks spawn missiles ONE AT A TIME via timer.
 * spawn_missile_block only initializes the block metadata.
 * Actual missile entities are created in obstacles_update. */
static void spawn_missile_block_1(ObstacleBlock *block, float block_x) {
  (void)block_x; /* Block position not needed — missiles spawn at player position */
  block->entity_count = 0;
  block->next_missile_idx = 0;
  block->missile_timer = MISSILE_BLOCK_1_INTERVAL; /* 0.75s to first missile */
}

static void spawn_missile_block_2(ObstacleBlock *block, float block_x,
                                   float player_y) {
  /* First tracking missile spawns immediately */
  block->entity_count = 1;
  ObstacleEntity *e = &block->entities[0];
  memset(e, 0, sizeof(*e));
  e->x = block_x + 293.0f;
  e->y = player_y + (float)PLAYER_H * 0.5f;
  e->state = MISSILE_WARNING;
  e->timer = MISSILE_WARNING_DURATION;
  e->tracking = 1;
  e->active = 1;
  e->hitbox = aabb_make(e->x, e->y - MISSILE_HITBOX_H * 0.5f,
                         MISSILE_HITBOX_W, MISSILE_HITBOX_H);
  block->next_missile_idx = 1;
  block->missile_timer = MISSILE_BLOCK_2_INTERVAL; /* 4s to second missile */
}

/* Spawn a single missile entity within a block */
static void spawn_one_missile(ObstacleBlock *block, float y, float player_x,
                               int tracking) {
  if (block->entity_count >= MAX_ENTITIES_PER_BLOCK) return;
  ObstacleEntity *e = &block->entities[block->entity_count];
  memset(e, 0, sizeof(*e));
  e->x = player_x + 200.0f; /* Right edge of screen */
  e->y = y;
  e->state = MISSILE_WARNING;
  e->timer = MISSILE_WARNING_DURATION;
  e->tracking = tracking;
  e->active = 1;
  e->hitbox = aabb_make(e->x, e->y - MISSILE_HITBOX_H * 0.5f,
                         MISSILE_HITBOX_W, MISSILE_HITBOX_H);
  block->entity_count++;
}

static void spawn_sigil_block_1(ObstacleBlock *block, float block_x) {
  block->entity_count = 1;
  ObstacleEntity *e = &block->entities[0];
  memset(e, 0, sizeof(*e));
  e->x = block_x;
  e->y = 85.0f; /* Center of screen */
  e->state = SIGIL_WAIT;
  e->timer = 1.0f; /* Wait before charging */
  e->active = 1;
  e->hitbox = aabb_make(e->x, e->y, SIGIL_BEAM_W, SIGIL_BEAM_H);
}

static void spawn_sigil_block_2(ObstacleBlock *block, float block_x) {
  block->entity_count = SIGIL_BLOCK_2_COUNT;
  /* All sigils start in WAIT state. The block's fire_order state machine
   * coordinates which sigils charge/fire/powerdown at each phase.
   * Original: sigil_block_2.gd fireOrder pattern. */
  for (int i = 0; i < SIGIL_BLOCK_2_COUNT; i++) {
    ObstacleEntity *e = &block->entities[i];
    memset(e, 0, sizeof(*e));
    e->x = block_x;
    e->y = SIGIL_BLOCK_2_Y[i];
    e->state = SIGIL_WAIT;
    e->timer = 0.0f;
    e->active = 1;
    e->hitbox = aabb_make(e->x, e->y, SIGIL_BEAM_W, SIGIL_BEAM_H);
  }
  /* Start phase 0: sigils 0,1 begin charging immediately */
  block->fire_order = 0;
  block->block_timer = SIGIL_BLOCK_2_CHARGE_DURATION;
  block->entities[0].state = SIGIL_CHARGE;
  block->entities[0].timer = SIGIL_BLOCK_2_CHARGE_DURATION;
  block->entities[1].state = SIGIL_CHARGE;
  block->entities[1].timer = SIGIL_BLOCK_2_CHARGE_DURATION;
}

/* Spawn pellets at hand-placed positions from the original .tscn files. */
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

static const char *OBS_TYPE_NAMES[] = {
  "NONE", "DAGGER_1", "MISSILE_1", "SIGIL_1",
  "MISSILE_2", "SIGIL_2", "DAGGER_2"
};

static void spawn_block(ObstacleManager *mgr, ObstacleType type, float block_x) {
  ObstacleBlock *block = find_free_block(mgr);
  if (!block) {
#ifdef DEBUG
    printf("[SPAWN] ERROR: no free block slot for type %s at x=%.0f\n",
           OBS_TYPE_NAMES[type], block_x);
#endif
    return;
  }

#ifdef DEBUG
  printf("[SPAWN] %s at world x=%.0f (next_spawn=%.0f)\n",
         OBS_TYPE_NAMES[type], block_x, mgr->next_spawn_x);
#endif

  memset(block, 0, sizeof(*block));
  block->type = type;
  block->x = block_x;
  block->active = 1;
  block->spawn_distance = SPAWN_DISTANCES[type];

  switch (type) {
  case OBS_DAGGER_BLOCK_1:
    spawn_dagger_block(block, DAGGER_BLOCK_1_POS, DAGGER_BLOCK_1_COUNT, block_x, &mgr->rng);
    break;
  case OBS_DAGGER_BLOCK_2:
    spawn_dagger_block(block, DAGGER_BLOCK_2_POS, DAGGER_BLOCK_2_COUNT, block_x, &mgr->rng);
    break;
  case OBS_MISSILE_BLOCK_1:
    spawn_missile_block_1(block, block_x);
    break;
  case OBS_MISSILE_BLOCK_2:
    spawn_missile_block_2(block, block_x, 85.0f); /* player_y placeholder */
    break;
  case OBS_SIGIL_BLOCK_1:
    spawn_sigil_block_1(block, block_x);
    break;
  case OBS_SIGIL_BLOCK_2:
    spawn_sigil_block_2(block, block_x);
    break;
  default:
    break;
  }

  /* Spawn hand-placed pellets per block type */
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
  default:
    /* Other blocks: no pellets defined yet (can be added later) */
    break;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * update_entity — advance one obstacle entity's state machine
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void update_dagger(ObstacleEntity *e, float dt) {
  e->rotation += DAGGER_ROTATION_SPEED * dt;
  /* Update hitbox position (daggers don't move) */
  e->hitbox = aabb_from_center(e->x, e->y,
                                DAGGER_HITBOX_W * 0.5f, DAGGER_HITBOX_H * 0.5f);
}

static void update_missile(ObstacleEntity *e, float player_x, float player_y,
                            float dt) {
  switch (e->state) {
  case MISSILE_WARNING:
    e->timer -= dt;
    /* During WARNING: position at ~293px ahead of player (right edge of screen) */
    e->x = player_x + 200.0f;
    /* Track player Y position for tracking missiles */
    if (e->tracking) {
      e->y = player_y + (float)PLAYER_H * 0.5f; /* Center on player */
    }
    if (e->timer <= 0.0f) {
      e->state = MISSILE_FIRE;
    }
    break;

  case MISSILE_FIRE:
    /* Godot: move_and_collide(Vector2(speed * delta, 0)) — same dt*dt model */
    /* move_and_collide takes displacement directly (not velocity).
     * Original: move_and_collide(Vector2(speed * delta, 0)) — single dt. */
    e->x += MISSILE_SPEED * dt;
    e->hitbox = aabb_make(e->x, e->y - MISSILE_HITBOX_H * 0.5f,
                           MISSILE_HITBOX_W, MISSILE_HITBOX_H);
    /* Transition to BOOM when missile is 100px past (left of) player */
    if (e->x < player_x - 100.0f) {
      e->state = MISSILE_BOOM;
      e->timer = 0.5f; /* Boom display time */
    }
    break;

  case MISSILE_BOOM:
    e->timer -= dt;
    if (e->timer <= 0.0f) {
      e->active = 0;
    }
    break;
  }
}

static void update_sigil(ObstacleEntity *e, float dt, ObstacleType block_type,
                         float camera_x) {
  /* Sigils lock X to camera center (screen-relative, not world-relative) */
  e->x = camera_x + (float)GAME_W * 0.5f;

  float charge_dur = (block_type == OBS_SIGIL_BLOCK_1)
                         ? SIGIL_BLOCK_1_CHARGE_DURATION
                         : SIGIL_BLOCK_2_CHARGE_DURATION;
  float fire_dur = (block_type == OBS_SIGIL_BLOCK_1)
                       ? SIGIL_BLOCK_1_FIRE_DURATION
                       : SIGIL_BLOCK_2_FIRE_DURATION;

  switch (e->state) {
  case SIGIL_WAIT:
    e->timer -= dt;
    if (e->timer <= 0.0f) {
      e->state = SIGIL_CHARGE;
      e->timer = charge_dur;
    }
    break;

  case SIGIL_CHARGE:
    e->timer -= dt;
    if (e->timer <= 0.0f) {
      e->state = SIGIL_FIRE;
      e->timer = fire_dur;
    }
    break;

  case SIGIL_FIRE:
    e->timer -= dt;
    /* Hitbox active during fire */
    e->hitbox = aabb_make(e->x, e->y - SIGIL_BEAM_H * 0.5f,
                           SIGIL_BEAM_W, SIGIL_BEAM_H);
    if (e->timer <= 0.0f) {
      e->state = SIGIL_POWERDOWN;
      e->timer = 0.75f;
    }
    break;

  case SIGIL_POWERDOWN:
    e->timer -= dt;
    if (e->timer <= 0.0f) {
      e->active = 0;
    }
    break;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * obstacles_update
 * ═══════════════════════════════════════════════════════════════════════════
 */
void obstacles_update(ObstacleManager *mgr, float player_x, float player_y,
                      float player_speed, float camera_x, float dt) {
  /* ── Spawn new blocks when player approaches next_spawn_x ────────── */
  while (player_x + (float)GAME_W >= mgr->next_spawn_x) {
    /* Pick random type, no repeats */
    ObstacleType type;
    do {
      type = (ObstacleType)rng_range(&mgr->rng, 1, (int)OBS_TYPE_COUNT - 1);
    } while (type == mgr->last_spawned_type);

    spawn_block(mgr, type, mgr->next_spawn_x);
    mgr->last_spawned_type = type;
    mgr->next_spawn_x += SPAWN_DISTANCES[type]; /* No extra gap — matches original */
  }

  /* ── Update active blocks ────────────────────────────────────────── */
  for (int b = 0; b < MAX_OBSTACLE_BLOCKS; b++) {
    ObstacleBlock *block = &mgr->blocks[b];
    if (!block->active) continue;

    /* Despawn blocks far behind the player */
    if (block->x + block->spawn_distance + 200.0f < player_x - 400.0f) {
      block->active = 0;
      continue;
    }

    /* Missile blocks: spawn missiles ONE AT A TIME via timer.
     * Original: missile_block_1.gd fires every 0.75s, 5 missiles total.
     * missile_block_2.gd: first missile immediately, second after 4s. */
    if (block->type == OBS_MISSILE_BLOCK_1) {
      block->missile_timer -= dt;
      if (block->missile_timer <= 0.0f &&
          block->next_missile_idx < MISSILE_BLOCK_1_COUNT) {
        spawn_one_missile(block, MISSILE_BLOCK_1_Y[block->next_missile_idx],
                          player_x, 0);
        block->next_missile_idx++;
        /* After 5th missile, long pause before block cleanup */
        block->missile_timer = (block->next_missile_idx < MISSILE_BLOCK_1_COUNT)
                                   ? MISSILE_BLOCK_1_INTERVAL
                                   : 12.0f;
      }
      /* Deactivate block after all missiles are done and timer expires */
      if (block->next_missile_idx >= MISSILE_BLOCK_1_COUNT) {
        int all_done = 1;
        for (int i = 0; i < block->entity_count; i++) {
          if (block->entities[i].active) { all_done = 0; break; }
        }
        if (all_done) block->active = 0;
      }
    }

    if (block->type == OBS_MISSILE_BLOCK_2) {
      block->missile_timer -= dt;
      if (block->missile_timer <= 0.0f && block->next_missile_idx < 2) {
        spawn_one_missile(block, player_y + (float)PLAYER_H * 0.5f,
                          player_x, 1);
        block->next_missile_idx++;
        block->missile_timer = 999.0f; /* No more spawns */
      }
      /* Deactivate when all missiles done */
      if (block->next_missile_idx >= 2) {
        int all_done = 1;
        for (int i = 0; i < block->entity_count; i++) {
          if (block->entities[i].active) { all_done = 0; break; }
        }
        if (all_done) block->active = 0;
      }
    }

    /* Sigil Block 2: block-level fire_order coordinator.
     * Manages which sigils charge/fire/powerdown at each phase.
     * Original: sigil_block_2.gd fireOrder state machine. */
    if (block->type == OBS_SIGIL_BLOCK_2) {
      block->block_timer -= dt;
      if (block->block_timer <= 0.0f) {
        block->fire_order++;
        switch (block->fire_order) {
        case 1:
          /* Phase 1: sigils 0,1 FIRE; sigils 2,3 start CHARGE */
          block->entities[0].state = SIGIL_FIRE;
          block->entities[0].timer = SIGIL_BLOCK_2_FIRE_DURATION;
          block->entities[1].state = SIGIL_FIRE;
          block->entities[1].timer = SIGIL_BLOCK_2_FIRE_DURATION;
          block->entities[2].state = SIGIL_CHARGE;
          block->entities[2].timer = SIGIL_BLOCK_2_CHARGE_DURATION;
          block->entities[3].state = SIGIL_CHARGE;
          block->entities[3].timer = SIGIL_BLOCK_2_CHARGE_DURATION;
          block->block_timer = SIGIL_BLOCK_2_FIRE_DURATION;
          break;
        case 2:
          /* Phase 2: sigils 0,1 POWERDOWN; sigils 2,3 FIRE; sigil 4 CHARGE */
          block->entities[0].state = SIGIL_POWERDOWN;
          block->entities[0].timer = 0.75f;
          block->entities[1].state = SIGIL_POWERDOWN;
          block->entities[1].timer = 0.75f;
          block->entities[2].state = SIGIL_FIRE;
          block->entities[2].timer = SIGIL_BLOCK_2_FIRE_DURATION;
          block->entities[3].state = SIGIL_FIRE;
          block->entities[3].timer = SIGIL_BLOCK_2_FIRE_DURATION;
          block->entities[4].state = SIGIL_CHARGE;
          block->entities[4].timer = SIGIL_BLOCK_2_CHARGE_DURATION;
          block->block_timer = SIGIL_BLOCK_2_FIRE_DURATION;
          break;
        case 3:
          /* Phase 3: sigils 2,3 POWERDOWN; sigil 4 FIRE */
          block->entities[2].state = SIGIL_POWERDOWN;
          block->entities[2].timer = 0.75f;
          block->entities[3].state = SIGIL_POWERDOWN;
          block->entities[3].timer = 0.75f;
          block->entities[4].state = SIGIL_FIRE;
          block->entities[4].timer = SIGIL_BLOCK_2_FIRE_DURATION;
          block->block_timer = SIGIL_BLOCK_2_FIRE_DURATION;
          break;
        case 4:
          /* Phase 4: sigil 4 POWERDOWN; block done */
          block->entities[4].state = SIGIL_POWERDOWN;
          block->entities[4].timer = 0.75f;
          block->block_timer = 1.0f; /* Short delay before deactivation */
          break;
        default:
          /* All phases done — deactivate block */
          block->active = 0;
          break;
        }
      }
      /* Update sigil X positions to track camera center */
      for (int i = 0; i < block->entity_count; i++) {
        ObstacleEntity *e = &block->entities[i];
        e->x = camera_x + (float)GAME_W * 0.5f;
        /* Update hitbox only when firing */
        if (e->state == SIGIL_FIRE) {
          e->hitbox = aabb_make(camera_x, e->y - SIGIL_BEAM_H * 0.5f,
                                 (float)GAME_W, SIGIL_BEAM_H);
        }
      }
    }

    /* Update each entity in the block */
    for (int i = 0; i < block->entity_count; i++) {
      ObstacleEntity *e = &block->entities[i];
      if (!e->active) continue;

      switch (block->type) {
      case OBS_DAGGER_BLOCK_1:
      case OBS_DAGGER_BLOCK_2:
        update_dagger(e, dt);
        break;
      case OBS_MISSILE_BLOCK_1:
      case OBS_MISSILE_BLOCK_2:
        update_missile(e, player_x, player_y, dt);
        break;
      case OBS_SIGIL_BLOCK_1:
        /* Sigil Block 1: individual sigil update (single sigil) */
        update_sigil(e, dt, block->type, camera_x);
        break;
      case OBS_SIGIL_BLOCK_2:
        /* Sigil Block 2: managed by block coordinator above.
         * Only update timer countdown for POWERDOWN deactivation. */
        if (e->state == SIGIL_POWERDOWN) {
          e->timer -= dt;
          if (e->timer <= 0.0f) {
            e->active = 0;
          }
        }
        break;
      default:
        break;
      }
    }
  }

  /* ── Update pellets ──────────────────────────────────────────────── */
  for (int i = 0; i < mgr->pellet_count; i++) {
    Pellet *p = &mgr->pellets[i];
    if (!p->active) continue;
    anim_update(&p->anim, dt);

    /* Despawn pellets far behind */
    if (p->x < player_x - 400.0f) {
      p->active = 0;
    }
  }

  /* ── Shield spawn timer ──────────────────────────────────────────── */
  mgr->shield_timer -= dt;
  if (mgr->shield_timer <= 0.0f) {
    mgr->shield_timer = rng_float_range(&mgr->rng, 10.0f, 30.0f);

    /* Find free shield slot */
    for (int i = 0; i < MAX_SHIELD_PICKUPS; i++) {
      if (!mgr->shields[i].active) {
        ShieldPickup *s = &mgr->shields[i];
        memset(s, 0, sizeof(*s));
        s->x = player_x + SHIELD_SPAWN_OFFSET_X;
        s->y = SHIELD_SPAWN_Y;
        s->vy = -200.0f; /* Initial upward bounce */
        s->bounce_dir = -1;
        s->active = 1;
        s->hitbox = aabb_make(s->x, s->y, 16.0f, 18.0f);
        break;
      }
    }
  }

  /* ── Update shields ──────────────────────────────────────────────── */
  for (int i = 0; i < MAX_SHIELD_PICKUPS; i++) {
    ShieldPickup *s = &mgr->shields[i];
    if (!s->active) continue;

    /* Bounce between min and max Y — same dt*dt model as player physics */
    s->y += s->vy * dt * dt;
    if (s->y <= SHIELD_BOUNCE_MIN_Y) {
      s->y = SHIELD_BOUNCE_MIN_Y;
      s->vy = 200.0f;
    } else if (s->y >= SHIELD_BOUNCE_MAX_Y) {
      s->y = SHIELD_BOUNCE_MAX_Y;
      s->vy = -200.0f;
    }

    /* Move with half player speed */
    s->x += player_speed * 0.5f * dt * dt;

    s->hitbox = aabb_make(s->x, s->y, 16.0f, 18.0f);

    /* Despawn if far behind */
    if (s->x < player_x - 400.0f) {
      s->active = 0;
    }
  }

  /* ── Update near-miss indicators ─────────────────────────────────── */
  for (int i = 0; i < MAX_NEAR_MISS_INDICATORS; i++) {
    NearMissIndicator *nm = &mgr->near_misses[i];
    if (!nm->active) continue;
    nm->timer -= dt;
    nm->y -= 20.0f * dt; /* Float upward */
    if (nm->timer <= 0.0f) {
      nm->active = 0;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * obstacles_render
 * ═══════════════════════════════════════════════════════════════════════════
 */
void obstacles_render(const ObstacleManager *mgr, Backbuffer *bb,
                      float camera_x) {
  /* Render obstacle blocks */
  for (int b = 0; b < MAX_OBSTACLE_BLOCKS; b++) {
    const ObstacleBlock *block = &mgr->blocks[b];
    if (!block->active) continue;

    for (int i = 0; i < block->entity_count; i++) {
      const ObstacleEntity *e = &block->entities[i];
      if (!e->active) continue;

      int sx = (int)(e->x - camera_x);
      int sy = (int)e->y;

      switch (block->type) {
      case OBS_DAGGER_BLOCK_1:
      case OBS_DAGGER_BLOCK_2:
        /* Render spinning dagger */
        if (mgr->sprite_dagger.pixels) {
          SpriteRect full = {0, 0, mgr->sprite_dagger.width,
                              mgr->sprite_dagger.height};
          sprite_blit_rotated(bb, &mgr->sprite_dagger, full, sx, sy,
                              e->rotation);
        } else {
          /* Fallback: red square */
          draw_rect(bb, (float)sx - 8.0f, (float)sy - 8.0f, 16.0f, 16.0f,
                    1.0f, 0.2f, 0.2f, 1.0f);
        }
        break;

      case OBS_MISSILE_BLOCK_1:
      case OBS_MISSILE_BLOCK_2:
        if (e->state == MISSILE_WARNING) {
          /* Warning indicator — use first frame of warning spritesheet */
          if (mgr->sheet_warning.sprite.pixels) {
            SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_warning, 0);
            sprite_blit(bb, &mgr->sprite_warning, rect, sx, sy);
          } else {
            draw_rect(bb, (float)sx, (float)sy, 20.0f, 16.0f,
                      1.0f, 1.0f, 0.0f, 0.6f);
          }
        } else if (e->state == MISSILE_FIRE) {
          /* Active missile — use first frame of missile spritesheet */
          if (mgr->sheet_missile.sprite.pixels) {
            SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_missile, 0);
            sprite_blit(bb, &mgr->sprite_missile, rect, sx, sy);
          } else {
            draw_rect(bb, (float)sx, (float)sy, 20.0f, 12.0f,
                      1.0f, 0.4f, 0.0f, 1.0f);
          }
        } else if (e->state == MISSILE_BOOM) {
          /* Explosion — use first frame of boom spritesheet */
          if (mgr->sheet_boom.sprite.pixels) {
            SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_boom, 0);
            sprite_blit(bb, &mgr->sprite_boom, rect, sx, sy);
          } else {
            draw_rect(bb, (float)sx - 10.0f, (float)sy - 10.0f,
                      24.0f, 24.0f, 1.0f, 0.8f, 0.0f, 0.8f);
          }
        }
        break;

      case OBS_SIGIL_BLOCK_1:
      case OBS_SIGIL_BLOCK_2:
        /* Sigil sprites ARE the beam (310x17 full-width).
         * Render centered on the sigil's Y position, anchored to screen left. */
        if (e->state == SIGIL_FIRE) {
          /* Fire: render beam sprite (frame 0) at screen position */
          if (mgr->sheet_sigil.sprite.pixels) {
            SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_sigil, 0);
            sprite_blit(bb, &mgr->sprite_sigil, rect,
                        0, sy - rect.h / 2);
          } else {
            draw_rect(bb, 0.0f, (float)sy - SIGIL_BEAM_H * 0.5f,
                      (float)bb->width, SIGIL_BEAM_H,
                      0.8f, 0.1f, 0.9f, 0.7f);
          }
        } else if (e->state == SIGIL_CHARGE) {
          /* Charge: render loading animation (frame 0 of fire sprite, dimmer) */
          if (mgr->sheet_sigil.sprite.pixels) {
            SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_sigil, 2);
            sprite_blit_alpha(bb, &mgr->sprite_sigil, rect,
                              0, sy - rect.h / 2);
          } else {
            draw_rect(bb, 0.0f, (float)sy - 4.0f, (float)bb->width, 8.0f,
                      0.5f, 0.1f, 0.7f, 0.4f);
          }
        } else if (e->state == SIGIL_WAIT) {
          /* Wait: render wait sprite (thin horizontal line) */
          if (mgr->sheet_sigil_wait.sprite.pixels) {
            SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_sigil_wait, 0);
            sprite_blit_alpha(bb, &mgr->sprite_sigil_wait, rect,
                              0, sy - rect.h / 2);
          }
        }
        break;

      default:
        break;
      }
    }
  }

  /* Render pellets */
  for (int i = 0; i < mgr->pellet_count; i++) {
    const Pellet *p = &mgr->pellets[i];
    if (!p->active) continue;

    int px = (int)(p->x - camera_x);
    int py = (int)p->y;

    if (mgr->sprite_pellet.pixels) {
      /* Use animation frame */
      anim_draw(&p->anim, bb, px, py);
    } else {
      /* Fallback: yellow dot */
      draw_rect(bb, (float)px, (float)py, 8.0f, 8.0f,
                1.0f, 0.9f, 0.2f, 1.0f);
    }
  }

  /* Render shield pickups */
  for (int i = 0; i < MAX_SHIELD_PICKUPS; i++) {
    const ShieldPickup *s = &mgr->shields[i];
    if (!s->active) continue;

    int sx = (int)(s->x - camera_x);
    int sy = (int)s->y;

    if (mgr->sheet_shield.sprite.pixels) {
      SpriteRect rect = spritesheet_frame_rect(&mgr->sheet_shield, 0);
      sprite_blit(bb, &mgr->sprite_shield, rect, sx, sy);
    } else {
      draw_rect(bb, (float)sx, (float)sy, 16.0f, 18.0f,
                0.2f, 0.6f, 1.0f, 0.9f);
    }
  }

  /* Render near-miss indicators */
  for (int i = 0; i < MAX_NEAR_MISS_INDICATORS; i++) {
    const NearMissIndicator *nm = &mgr->near_misses[i];
    if (!nm->active) continue;

    int nx = (int)(nm->x - camera_x);
    int ny = (int)nm->y;

    draw_text(bb, (float)nx, (float)ny, 1, "+50",
              0.2f, 1.0f, 0.4f, nm->timer * 2.0f);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Collision checks
 * ═══════════════════════════════════════════════════════════════════════════
 */
int obstacles_check_collision(ObstacleManager *mgr, AABB player_hitbox) {
  for (int b = 0; b < MAX_OBSTACLE_BLOCKS; b++) {
    ObstacleBlock *block = &mgr->blocks[b];
    if (!block->active) continue;

    for (int i = 0; i < block->entity_count; i++) {
      ObstacleEntity *e = &block->entities[i];
      if (!e->active) continue;

      /* Daggers: always dangerous */
      if (block->type == OBS_DAGGER_BLOCK_1 ||
          block->type == OBS_DAGGER_BLOCK_2) {
        if (aabb_overlap(player_hitbox, e->hitbox))
          return 1;
      }

      /* Missiles: dangerous only during FIRE state */
      if ((block->type == OBS_MISSILE_BLOCK_1 ||
           block->type == OBS_MISSILE_BLOCK_2) &&
          e->state == MISSILE_FIRE) {
        if (aabb_overlap(player_hitbox, e->hitbox))
          return 1;
      }

      /* Sigils: dangerous only during FIRE state */
      if ((block->type == OBS_SIGIL_BLOCK_1 ||
           block->type == OBS_SIGIL_BLOCK_2) &&
          e->state == SIGIL_FIRE) {
        if (aabb_overlap(player_hitbox, e->hitbox))
          return 1;
      }
    }
  }
  return 0;
}

int obstacles_check_near_miss(ObstacleManager *mgr, AABB player_hitbox) {
  int count = 0;
  for (int b = 0; b < MAX_OBSTACLE_BLOCKS; b++) {
    ObstacleBlock *block = &mgr->blocks[b];
    if (!block->active) continue;

    for (int i = 0; i < block->entity_count; i++) {
      ObstacleEntity *e = &block->entities[i];
      if (!e->active) continue;

      int is_dangerous = 0;
      if (block->type == OBS_DAGGER_BLOCK_1 || block->type == OBS_DAGGER_BLOCK_2)
        is_dangerous = 1;
      if ((block->type == OBS_MISSILE_BLOCK_1 || block->type == OBS_MISSILE_BLOCK_2) &&
          e->state == MISSILE_FIRE)
        is_dangerous = 1;

      if (is_dangerous && aabb_near_miss(player_hitbox, e->hitbox, NEAR_MISS_MARGIN)) {
        count++;

        /* Create near-miss indicator */
        for (int n = 0; n < MAX_NEAR_MISS_INDICATORS; n++) {
          if (!mgr->near_misses[n].active) {
            mgr->near_misses[n].x = player_hitbox.x;
            mgr->near_misses[n].y = player_hitbox.y - 15.0f;
            mgr->near_misses[n].timer = 0.8f;
            mgr->near_misses[n].active = 1;
            break;
          }
        }
      }
    }
  }
  return count;
}

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

int obstacles_collect_shield(ObstacleManager *mgr, AABB player_hitbox) {
  for (int i = 0; i < MAX_SHIELD_PICKUPS; i++) {
    ShieldPickup *s = &mgr->shields[i];
    if (!s->active) continue;
    if (aabb_overlap(player_hitbox, s->hitbox)) {
      s->active = 0;
      return 1;
    }
  }
  return 0;
}
