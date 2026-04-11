#ifndef GAME_MAIN_H
#define GAME_MAIN_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/main.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "../platform.h"
#include "../utils/backbuffer.h"
#include "../utils/sprite.h"
#include "./base.h"
#include "./types.h"
#include "./obstacles.h"
#include "./audio.h"
#include "./high_scores.h"
#include "../utils/particles.h"

/* ── Game phases ──────────────────────────────────────────────────────── */
typedef enum {
  GAME_PHASE_LOADING,
  GAME_PHASE_MENU,
  GAME_PHASE_PLAYING,
  GAME_PHASE_PAUSED,
  GAME_PHASE_DYING,
  GAME_PHASE_GAMEOVER,
} GamePhase;

/* ── GameState ────────────────────────────────────────────────────────── */
typedef struct {
  GamePhase phase;
  int is_running;

  /* Camera */
  GameCamera camera;

  /* Player */
  Player player;

  /* Background */
  Sprite bg_sprite;
  float bg_scroll_offset; /* Parallax scroll position */

  /* Obstacles */
  ObstacleManager obstacles;

  /* Audio */
  GameAudio audio;

  /* Particles */
  ParticleSystem particles;

  /* High scores */
  HighScoreTable high_scores;

  /* Scoring */
  int score;
  int pellet_count;
  int near_miss_count;

  float elapsed_time;

  /* Loading state */
  int loading_clip_index; /* Next audio clip to load (0 to CLIP_COUNT-1) */
} GameState;

/* ── Game entry points ────────────────────────────────────────────────── */
void game_init(GameState *state, PlatformGameProps *props);
void game_init_audio(GameState *state); /* Call after first frame renders */
void game_cleanup(GameState *state);    /* Call before platform shutdown */
void game_update(GameState *state, GameInput *input, float dt);
void game_render(GameState *state, Backbuffer *backbuffer,
                 GameWorldConfig world_config, Arena *scratch);
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buf,
                            int num_frames);

#endif /* GAME_MAIN_H */
