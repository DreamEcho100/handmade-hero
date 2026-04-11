/* ═══════════════════════════════════════════════════════════════════════════
 * game/main.c — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Game loop: init, update, render, audio.
 * Player physics + camera follow + parallax background.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./main.h"
#include "./player.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../utils/sprite.h"
#include "../utils/particles.h"
#include "../utils/rng.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

/* ── Particle configurations ─────────────────────────────────────────── */
static const ParticleConfig EXHAUST_CONFIG = {
  .speed_min = 30.0f, .speed_max = 80.0f,
  .angle_min = 2.5f,  .angle_max = 3.8f,   /* Downward cone (π ± 0.6) */
  .life_min = 0.1f,   .life_max = 0.3f,
  .size_min = 1.0f,   .size_max = 3.0f,
  .color = 0xFFFF8833, /* Orange-yellow */
};

static const ParticleConfig DEATH_CONFIG = {
  .speed_min = 40.0f,  .speed_max = 120.0f,
  .angle_min = 0.0f,   .angle_max = 6.28f,  /* Full circle */
  .life_min = 0.3f,    .life_max = 0.8f,
  .size_min = 2.0f,    .size_max = 4.0f,
  .color = 0xFFFF3333, /* Red */
};

static const ParticleConfig SHIELD_BREAK_CONFIG = {
  .speed_min = 20.0f,  .speed_max = 60.0f,
  .angle_min = 0.0f,   .angle_max = 6.28f,
  .life_min = 0.2f,    .life_max = 0.5f,
  .size_min = 1.0f,    .size_max = 3.0f,
  .color = 0xFF33BBFF, /* Blue */
};

/* ── Background constants ────────────────────────────────────────────── */
/* Original Background.png is 342x180. Tiles seamlessly by mirroring.    */
#define BG_TILE_W 342
#define BG_PARALLAX_FACTOR 0.3f /* Background scrolls at 30% of camera speed */

/* ── Floor rendering ─────────────────────────────────────────────────── */
#define FLOOR_RENDER_Y FLOOR_SURFACE_Y /* Visual floor line at y=165 */

/* ═══════════════════════════════════════════════════════════════════════════
 * game_init
 * ═══════════════════════════════════════════════════════════════════════════
 */
/* Background thread function for audio loading */
static void *audio_load_thread_fn(void *arg) {
  GameAudio *audio = (GameAudio *)arg;
  game_audio_init(audio); /* Blocking — loads all 15 clips */
  printf("Audio thread: loading complete\n");
  return NULL;
}

void game_init(GameState *state, PlatformGameProps *props) {
  (void)props;
  memset(state, 0, sizeof(*state));
  state->phase = GAME_PHASE_PLAYING; /* Start playing immediately */
  state->is_running = 1;
  state->camera.x = 0.0f;
  state->camera.y = 0.0f;
  state->camera.zoom = 1.0f;

  /* Initialize player */
  if (player_init(&state->player) != 0) {
    fprintf(stderr, "Failed to initialize player\n");
  }

  /* Initialize obstacles */
  obstacles_init(&state->obstacles);

  /* Initialize audio state, then load clips on background thread */
  game_audio_init_state(&state->audio);
  {
    pthread_t audio_thread;
    pthread_create(&audio_thread, NULL, audio_load_thread_fn, &state->audio);
    pthread_detach(audio_thread); /* Fire and forget */
  }

  /* Initialize particles */
  particles_init(&state->particles);

  /* Load high scores */
  high_scores_init(&state->high_scores);
  high_scores_load(&state->high_scores, "highscores.txt");

  /* Load background */
  if (sprite_load("assets/sprites/Background.png", &state->bg_sprite) != 0) {
    fprintf(stderr, "WARNING: Failed to load Background.png\n");
  }
}

void game_init_audio(GameState *state) {
  (void)state; /* No longer used — audio loads on background thread */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_cleanup — free all resources before platform shutdown
 * ═══════════════════════════════════════════════════════════════════════════
 */
void game_cleanup(GameState *state) {
#ifdef DEBUG
  printf("game_cleanup: freeing all resources\n");
#endif
  player_free(&state->player);
  obstacles_free(&state->obstacles);
  game_audio_free(&state->audio);
  sprite_free(&state->bg_sprite);
#ifdef DEBUG
  printf("game_cleanup: done\n");
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_update
 * ═══════════════════════════════════════════════════════════════════════════
 */
void game_update(GameState *state, GameInput *input, float dt) {
  state->elapsed_time += dt;

#ifdef DEBUG
  /* Periodic debug logging — verify physics values every ~2 seconds */
  {
    static float debug_log_timer = 0.0f;
    debug_log_timer += dt;
    if (debug_log_timer >= 2.0f) {
      debug_log_timer -= 2.0f;
      printf("[%.1fs] x=%.0f speed=%.0f virt=%.0f dt=%.5f dx/frame=%.4f phase=%d\n",
             state->elapsed_time, state->player.x, state->player.speed,
             state->player.virt_speed, dt,
             state->player.speed * dt * dt, state->phase);
    }
  }
#endif

  if (input->buttons.quit.ended_down) {
    state->is_running = 0;
    return;
  }

  /* Check if background audio thread finished loading */
  if (state->audio.initialized && !state->audio.loaded.music_voice.active) {
    game_audio_play_music(&state->audio, CLIP_BG_MUSIC, 0.3f);
  }

  switch (state->phase) {
  case GAME_PHASE_LOADING:
    /* No longer used — audio loads on background thread */
    state->phase = GAME_PHASE_PLAYING;
    break;

  case GAME_PHASE_PLAYING: {
    /* Update player */
    player_update(&state->player, input, dt);

    /* Camera follows player X with offset */
    state->camera.x = state->player.x - CAMERA_OFFSET_X;
    if (state->camera.x < 0.0f) state->camera.x = 0.0f;

    /* Jump sound on action press */
    if (button_just_pressed(input->buttons.action) &&
        state->player.state == PLAYER_STATE_NORMAL) {
      game_audio_play(&state->audio, CLIP_JUMP);
    }

    /* Jetpack exhaust particles while flying */
    if (input->buttons.action.ended_down &&
        state->player.state == PLAYER_STATE_NORMAL &&
        !state->player.is_on_floor) {
      particles_emit(&state->particles,
                     state->player.x + 5.0f,
                     state->player.y + (float)PLAYER_H,
                     2, &EXHAUST_CONFIG);
    }

    /* Update obstacles */
    obstacles_update(&state->obstacles,
                     state->player.x, state->player.y,
                     state->player.speed, state->camera.x, dt);

    /* Collision: obstacles hit player */
    if (state->player.state == PLAYER_STATE_NORMAL &&
        !state->player.hurted) {
      if (obstacles_check_collision(&state->obstacles, state->player.hitbox)) {
        int died = player_hurt(&state->player);
        if (died) {
          game_audio_play(&state->audio, CLIP_FALL);
          particles_emit(&state->particles,
                         state->player.x + 10.0f,
                         state->player.y + 10.0f,
                         20, &DEATH_CONFIG);
        } else {
          game_audio_play(&state->audio, CLIP_POP);
          particles_emit(&state->particles,
                         state->player.x + 10.0f,
                         state->player.y + 10.0f,
                         12, &SHIELD_BREAK_CONFIG);
        }
      }
    }

    /* Collect pellets */
    int pellets = obstacles_collect_pellets(&state->obstacles,
                                            state->player.hitbox);
    if (pellets > 0) {
      game_audio_play(&state->audio, CLIP_PELLET);
    }
    state->pellet_count += pellets;

    /* Near-miss bonus */
    if (state->player.state == PLAYER_STATE_NORMAL) {
      int near = obstacles_check_near_miss(&state->obstacles,
                                           state->player.hitbox);
      state->near_miss_count += near;
    }

    /* Shield pickup */
    if (obstacles_collect_shield(&state->obstacles, state->player.hitbox)) {
      if (!state->player.has_shield) {
        state->player.has_shield = 1;
        game_audio_play(&state->audio, CLIP_SHIELD);
      } else {
        state->score += 50; /* Already have shield: bonus points */
      }
    }

    /* Update particles */
    particles_update(&state->particles, dt);

    /* Update score from distance */
    int distance_score = (int)(state->player.total_distance / 10.0f);
    state->score = distance_score + state->pellet_count * 10 +
                   state->near_miss_count * 50;

    /* Check for death → game over transition */
    if (state->player.state == PLAYER_STATE_GAMEOVER) {
      state->phase = GAME_PHASE_GAMEOVER;
    }

    break;
  }

  case GAME_PHASE_GAMEOVER:
    /* Wait for restart input */
    if (button_just_pressed(input->buttons.restart) ||
        button_just_pressed(input->buttons.action)) {
      /* Reset game state while preserving loaded resources.
       * DO NOT memset the whole state — that destroys sprite/audio pointers.
       * Instead, reset only the gameplay fields. */
      state->phase = GAME_PHASE_PLAYING;
      state->camera.x = 0.0f;
      state->camera.y = 0.0f;
      state->camera.zoom = 1.0f;
      state->score = 0;
      state->pellet_count = 0;
      state->near_miss_count = 0;

      /* Reset player state (keep loaded sprites) */
      state->player.x = PLAYER_START_X;
      state->player.y = PLAYER_START_Y;
      state->player.speed = PLAYER_MIN_SPEED;
      state->player.virt_speed = 0.0f;
      state->player.state = PLAYER_STATE_NORMAL;
      state->player.is_on_floor = 1;
      state->player.is_on_ceiling = 0;
      state->player.has_shield = 0;
      state->player.hurted = 0;
      state->player.hurt_timer = 0.0f;
      state->player.end_timer = 0.0f;
      state->player.total_distance = 0.0f;
      anim_reset(&state->player.anim_ground);
      anim_reset(&state->player.anim_dead);

      /* Reset obstacle spawning (keep loaded sprites) */
      for (int i = 0; i < MAX_OBSTACLE_BLOCKS; i++)
        state->obstacles.blocks[i].active = 0;
      state->obstacles.block_count = 0;
      state->obstacles.pellet_count = 0;
      for (int i = 0; i < MAX_PELLETS; i++)
        state->obstacles.pellets[i].active = 0;
      for (int i = 0; i < MAX_SHIELD_PICKUPS; i++)
        state->obstacles.shields[i].active = 0;
      for (int i = 0; i < MAX_NEAR_MISS_INDICATORS; i++)
        state->obstacles.near_misses[i].active = 0;
      state->obstacles.last_spawned_type = OBS_NONE;
      state->obstacles.next_spawn_x = PLAYER_START_X + 200.0f;
      state->obstacles.shield_timer =
          rng_float_range(&state->obstacles.rng, 20.0f, 40.0f);

      /* Reset particles */
      particles_init(&state->particles);

      /* Restart music */
      game_audio_play_music(&state->audio, CLIP_BG_MUSIC, 0.3f);
    }
    break;

  default:
    break;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * render_background — parallax scrolling background
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void render_background(GameState *state, Backbuffer *bb) {
  if (!state->bg_sprite.pixels) {
    /* Fallback: solid color if no background loaded */
    draw_rect(bb, 0.0f, 0.0f, (float)bb->width, (float)bb->height,
              0.078f, 0.047f, 0.110f, 1.0f);
    return;
  }

  /* Parallax: background scrolls at 30% of camera speed */
  float parallax_offset = state->camera.x * BG_PARALLAX_FACTOR;

  /* Calculate the starting tile position */
  int tile_w = state->bg_sprite.width;
  int start_x = -((int)parallax_offset % tile_w);
  if (start_x > 0) start_x -= tile_w;

  /* Draw enough tiles to cover the screen */
  SpriteRect full = {0, 0, state->bg_sprite.width, state->bg_sprite.height};
  for (int tx = start_x; tx < bb->width; tx += tile_w) {
    sprite_blit(bb, &state->bg_sprite, full, tx, 0);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * render_floor — simple floor line
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void render_floor(Backbuffer *bb) {
  /* Draw floor as a colored band at the bottom */
  draw_rect(bb, 0.0f, FLOOR_RENDER_Y, (float)bb->width,
            (float)bb->height - FLOOR_RENDER_Y,
            0.15f, 0.10f, 0.20f, 1.0f);

  /* Floor line */
  draw_rect(bb, 0.0f, FLOOR_RENDER_Y, (float)bb->width, 1.0f,
            0.30f, 0.25f, 0.40f, 1.0f);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * render_hud — score, distance, pellets
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void render_hud(GameState *state, Backbuffer *bb) {
  char buf[64];

  /* Distance (top-left) */
  int distance_m = (int)(state->player.total_distance / 10.0f);
  snprintf(buf, sizeof(buf), "Dist: %dm", distance_m);
  draw_text(bb, 4.0f, 4.0f, 1, buf, 1.0f, 1.0f, 1.0f, 1.0f);

  /* Score (top-right area) */
  snprintf(buf, sizeof(buf), "Score: %d", state->score);
  /* Calculate text width: 8px per char at scale 1 */
  int text_w = (int)strlen(buf) * 8;
  draw_text(bb, (float)(bb->width - text_w - 4), 4.0f, 1, buf,
            1.0f, 0.9f, 0.3f, 1.0f);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_render
 * ═══════════════════════════════════════════════════════════════════════════
 */
void game_render(GameState *state, Backbuffer *backbuffer,
                 GameWorldConfig world_config, Arena *scratch) {
  (void)world_config;
  (void)scratch;

  /* 1. Background (parallax) */
  render_background(state, backbuffer);

  /* 2. Floor */
  render_floor(backbuffer);

  /* 3. Obstacles */
  obstacles_render(&state->obstacles, backbuffer, state->camera.x);

  /* 4. Player */
  player_render(&state->player, backbuffer, state->camera.x);

  /* 5. Particles (above player, below HUD) */
  particles_draw(&state->particles, backbuffer, state->camera.x);

  /* 6. HUD */
  render_hud(state, backbuffer);

  /* 5. Game over overlay */
  if (state->phase == GAME_PHASE_GAMEOVER) {
    /* Semi-transparent dark overlay */
    draw_rect(backbuffer, 0.0f, 0.0f,
              (float)backbuffer->width, (float)backbuffer->height,
              0.0f, 0.0f, 0.0f, 0.6f);

    /* GAME OVER text */
    draw_text(backbuffer, 96.0f, 60.0f, 2, "GAME OVER",
              1.0f, 0.3f, 0.3f, 1.0f);

    /* Score */
    char buf[64];
    snprintf(buf, sizeof(buf), "Score: %d", state->score);
    int text_w = (int)strlen(buf) * 8;
    draw_text(backbuffer, (float)(backbuffer->width - text_w) * 0.5f, 90.0f,
              1, buf, 1.0f, 0.9f, 0.3f, 1.0f);

    /* Restart hint */
    draw_text(backbuffer, 64.0f, 120.0f, 1,
              "Press SPACE to restart", 0.7f, 0.7f, 0.8f, 1.0f);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * game_get_audio_samples
 * ═══════════════════════════════════════════════════════════════════════════
 */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buf,
                            int num_frames) {
  game_audio_mix(&state->audio, buf, num_frames);
}
