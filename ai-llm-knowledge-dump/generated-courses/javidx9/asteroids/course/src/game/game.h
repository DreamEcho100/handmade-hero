#ifndef GAME_GAME_H
#define GAME_GAME_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/game.h — Asteroids Game Types & Public API
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Coordinates: COORD_ORIGIN_LEFT_BOTTOM (y-up Cartesian).
 *   (0,0) = bottom-left.  (GAME_UNITS_W, GAME_UNITS_H) = top-right.
 *   1 game unit = 50 pixels at the base 800×600 resolution.
 *
 * All gameplay constants use game units (GU) and GU/s for velocities.
 * The pixel scale is handled entirely by RenderContext in asteroids_render.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>

#include "../utils/audio.h"     /* GameAudioState, SoundID              */
#include "../utils/backbuffer.h"/* Backbuffer                           */
#include "../utils/base.h"      /* GAME_UNITS_W/H, GameWorldConfig      */
#include "../utils/math.h"      /* Vec2                                 */
#include "./base.h"             /* GameInput, GameCamera, DEBUG_* macros */

/* ── Pool limits ─────────────────────────────────────────────────────────
 * Fixed-size pools avoid malloc.  Sizes are conservatively large.        */
#define MAX_ASTEROIDS   32
#define MAX_BULLETS      8

/* Vertex counts */
#define SHIP_VERTS      3
#define ASTEROID_VERTS 20

/* ── Gameplay constants (GAME UNITS — 1 GU = 50 px at 800×600) ──────────
 *
 * Conversion from old pixel-based constants: value_px / 50 = value_GU
 *
 *   ASTEROID_LARGE_SIZE  = 20 px / 50 = 0.40 GU
 *   ASTEROID_MEDIUM_SIZE = 10 px / 50 = 0.20 GU
 *   ASTEROID_SMALL_SIZE  =  5 px / 50 = 0.10 GU
 *   BULLET_SPEED         = 200 px/s / 50 = 4.0 GU/s
 *   SHIP_THRUST_ACCEL    = 100 px/s² / 50 = 2.0 GU/s²
 *   SHIP_MAX_SPEED       = 220 px/s / 50 = 4.4 GU/s
 *
 * SHIP_RENDER_SCALE: the draw scale passed to draw_wireframe.
 *   Old draw scale was 3.5 px.  Ship model vertex max magnitude is ~1.0
 *   (unit-normalized).  3.5 px / 50 = 0.07 GU is tiny — Asteroids ships
 *   are typically 17–20 px radius.  We use 0.35 GU (17.5 px equivalent).  */
#define ASTEROID_LARGE_SIZE   0.40f  /* radius, GU                         */
#define ASTEROID_MEDIUM_SIZE  0.20f  /* radius, GU                         */
#define ASTEROID_SMALL_SIZE   0.10f  /* radius, GU                         */

#define SHIP_RENDER_SCALE     0.35f  /* draw_wireframe scale, GU           */
#define SHIP_COLLISION_SIZE   0.10f  /* player collision radius, GU        */

#define BULLET_SPEED          4.00f  /* GU/s                               */
#define BULLET_LIFETIME       1.50f  /* seconds                            */
#define SHIP_TURN_SPEED       3.50f  /* rad/s (unchanged)                  */
#define SHIP_THRUST_ACCEL     2.00f  /* GU/s²                              */
#define SHIP_MAX_SPEED        4.40f  /* GU/s cap                           */
#define SHIP_FRICTION         0.97f  /* velocity multiplier per frame      */
#define DEATH_DELAY           2.50f  /* seconds before restart allowed     */
#define FIRE_COOLDOWN         0.15f  /* seconds between consecutive shots  */

/* ── GamePhase ───────────────────────────────────────────────────────────
 * PLAYING — normal gameplay.
 * DEAD    — ship destroyed; waiting for player to restart.               */
typedef enum {
  PHASE_PLAYING = 0,
  PHASE_DEAD
} GamePhase;

/* ── SpaceObject ─────────────────────────────────────────────────────────
 * Unified struct for ship, asteroid, and bullet.
 *
 * For asteroids: size = collision radius (GU).
 * For bullets:   size = remaining lifetime (seconds); bullet dies when ≤ 0.
 * For ship:      size = collision radius (SHIP_COLLISION_SIZE).
 *
 * x, y are in GAME UNITS (y-up, origin at bottom-left).
 * dx, dy are in GU/s.
 * angle is in radians.  0 = nose pointing up (+y).  CW = increasing.    */
typedef struct {
  float x, y;      /* world-space position in game units                   */
  float dx, dy;    /* velocity in GU/s                                     */
  float angle;     /* heading in radians (0 = up/+y, CW = increasing)      */
  float size;      /* radius (GU) for asteroids/ship; lifetime (s) for bullets */
  int   active;    /* 1 = in use; 0 = free slot                            */
} SpaceObject;

/* ── GameState ───────────────────────────────────────────────────────────
 * All mutable game data in one struct.
 * Allocated on the stack in main (≈3–4 KB at 800×600 base).             */
typedef struct {
  SpaceObject  player;                     /* the ship                     */
  SpaceObject  asteroids[MAX_ASTEROIDS];   /* asteroid pool                */
  int          asteroid_count;
  SpaceObject  bullets[MAX_BULLETS];       /* bullet pool                  */
  int          bullet_count;

  int          score;
  int          best_score;                 /* survives asteroids_init()    */

  GamePhase    phase;
  float        dead_timer;                 /* countdown until restart OK   */
  float        fire_timer;                 /* fire cooldown counter        */

  /* Ship and asteroid models built in asteroids_init (local space, y-up). */
  Vec2         ship_model[SHIP_VERTS];
  Vec2         asteroid_model[ASTEROID_VERTS];

  GameAudioState audio;
} GameState;

/* ── Function declarations ───────────────────────────────────────────────
 *
 * asteroids_init:    full init (memset + model build + spawn asteroids).
 *                    Called once from main() and again on restart.
 *                    Saves/restores audio.samples_per_second and volumes.
 *
 * asteroids_update:  per-frame simulation, dt in seconds.
 *
 * asteroids_render:  draw all objects into bb using explicit coord mode.
 *                    Creates a RenderContext from world_config internally.
 *
 * Audio functions (implemented in game/audio.c):
 *   game_audio_init           — set default volume levels
 *   game_audio_update         — expire finished voices; called per game frame
 *   game_get_audio_samples    — synthesize num_frames PCM frames; audio loop
 *   game_play_sound           — queue one-shot at centre pan
 *   game_play_sound_panned    — queue one-shot with spatial pan
 *   game_sustain_sound        — keep a TRAPEZOID voice alive while key held
 *   game_is_sound_playing     — true if any voice of `id` is active         */
void asteroids_init(GameState *state);
void asteroids_update(GameState *state, const GameInput *input, float dt);
void asteroids_render(const GameState *state, Backbuffer *bb,
                      GameWorldConfig world_config);

void game_audio_init(GameAudioState *audio);
void game_audio_update(GameAudioState *audio, float dt_ms);
void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buf,
                            int num_frames);
void game_play_sound(GameAudioState *audio, SoundID id);
void game_play_sound_panned(GameAudioState *audio, SoundID id, float pan);
void game_sustain_sound(GameAudioState *audio, SoundID id);
int  game_is_sound_playing(const GameAudioState *audio, SoundID id);

#endif /* GAME_GAME_H */
