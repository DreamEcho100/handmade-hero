/* ═══════════════════════════════════════════════════════════════════════════
 * game/game.c — Asteroids Game Logic
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Coordinate system: COORD_ORIGIN_LEFT_BOTTOM (y-up Cartesian).
 *   - (0,0) = bottom-left.  (GAME_UNITS_W, GAME_UNITS_H) = top-right.
 *   - 1 GU = 50 px at the 800×600 base resolution.
 *   - Rotation: CW = increasing angle (matches vec2_rotate in utils/math.h).
 *   - Thrust: dx += sin(a)*ACCEL*dt,  dy += cos(a)*ACCEL*dt.
 *     At angle=0 (nose up +y): (0,1)*ACCEL — moves up. ✓
 *     At angle=π/2 (nose right): (1,0)*ACCEL — moves right. ✓
 *
 * Ship model (unit-normalized, y-up):
 *   nose   = (  0.0, +1.0 ) — points up at angle=0
 *   left   = ( -0.6, -0.7 ) — back-left fin
 *   right  = ( +0.6, -0.7 ) — back-right fin
 *
 * Asteroid model: 20 vertices, each at sinf(t)*noise, cosf(t)*noise.
 *   Goes top→right→bottom→left (CW in y-up). ✓
 *
 * Explicit render mode:
 *   All world→pixel conversions use world_x/y/w/h() from render_explicit.h,
 *   never hardcoded pixel offsets.  make_render_context() is called once per
 *   frame; draw calls receive RenderContext + Backbuffer pointers.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "game.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../utils/draw-wireframe.h"
#include "../utils/render.h"
#include <math.h>    /* sinf, cosf, fabsf, sqrtf, fmodf */
#include <string.h>  /* memset, strlen                    */
#include <stdio.h>   /* snprintf                          */
#include <stdlib.h>  /* rand, srand                       */
#include <time.h>    /* time()                            */

#define PI 3.14159265358979323846f

/* ── compact_pool ──────────────────────────────────────────────────────────
 * Remove inactive objects using swap-with-tail (O(n), in-place).
 * Called AFTER all detection loops so no index is invalidated mid-loop.   */
static void compact_pool(SpaceObject *pool, int *count) {
  int i = 0;
  while (i < *count) {
    if (!pool[i].active) {
      pool[i] = pool[*count - 1];
      (*count)--;
    } else {
      i++;
    }
  }
}

/* ── add_asteroid ──────────────────────────────────────────────────────────
 * Append a new asteroid.  Returns 0 if the pool is full.                  */
static int add_asteroid(GameState *state,
                        float x,  float y,
                        float dx, float dy,
                        float size) {
  if (state->asteroid_count >= MAX_ASTEROIDS) return 0;
  SpaceObject *a = &state->asteroids[state->asteroid_count++];
  a->x = x;  a->y = y;
  a->dx = dx; a->dy = dy;
  a->size   = size;
  a->angle  = 0.0f;
  a->active = 1;
  return 1;
}

/* ── add_bullet ────────────────────────────────────────────────────────────
 * Fire a bullet from the ship nose in the current heading direction.
 *
 * CW rotation for y-up: (0,+1) → (sin_a, cos_a).
 * Bullet spawns slightly beyond SHIP_RENDER_SCALE so it clears the hull.  */
static int add_bullet(GameState *state) {
  if (state->bullet_count >= MAX_BULLETS) return 0;

  float ca = cosf(state->player.angle);
  float sa = sinf(state->player.angle);

  /* Nose tip = center + (sin_a, cos_a) * SHIP_RENDER_SCALE * 1.1 */
  float tip_x = state->player.x + sa * (SHIP_RENDER_SCALE * 1.1f);
  float tip_y = state->player.y + ca * (SHIP_RENDER_SCALE * 1.1f);

  SpaceObject *b = &state->bullets[state->bullet_count++];
  b->x      = tip_x;
  b->y      = tip_y;
  b->dx     = sa * BULLET_SPEED;
  b->dy     = ca * BULLET_SPEED;
  b->angle  = 0.0f;
  b->size   = BULLET_LIFETIME;   /* used as countdown timer */
  b->active = 1;
  return 1;
}

/* ── reset_game ─────────────────────────────────────────────────────────────
 * Reset gameplay state; preserve audio volumes and models.
 * Spawn player at world center and 4 large asteroids near the corners.    */
static void reset_game(GameState *state) {
  state->player.x      = GAME_UNITS_W * 0.5f;  /* 8.0 GU */
  state->player.y      = GAME_UNITS_H * 0.5f;  /* 6.0 GU */
  state->player.dx     = 0.0f;
  state->player.dy     = 0.0f;
  state->player.angle  = 0.0f;
  state->player.size   = SHIP_COLLISION_SIZE;
  state->player.active = 1;

  state->asteroid_count = 0;
  state->bullet_count   = 0;
  state->fire_timer     = 0.0f;

  /* Spawn 4 large asteroids near each corner (1.2 GU = 60 px from edge).
   * Random velocity in [-0.2, +0.2] GU/s each axis.
   * Formula: (rand() % 40 - 20) * 0.01f → range [-0.20, 0.19] GU/s.      */
#define RAND_VEL() ((float)(rand() % 40 - 20) * 0.01f)
  add_asteroid(state, 1.2f,                    1.2f,
               RAND_VEL(), RAND_VEL(), ASTEROID_LARGE_SIZE);
  add_asteroid(state, GAME_UNITS_W - 1.2f,     1.2f,
               RAND_VEL(), RAND_VEL(), ASTEROID_LARGE_SIZE);
  add_asteroid(state, 1.2f,                    GAME_UNITS_H - 1.2f,
               RAND_VEL(), RAND_VEL(), ASTEROID_LARGE_SIZE);
  add_asteroid(state, GAME_UNITS_W - 1.2f,     GAME_UNITS_H - 1.2f,
               RAND_VEL(), RAND_VEL(), ASTEROID_LARGE_SIZE);
#undef RAND_VEL

  state->score      = 0;
  state->phase      = PHASE_PLAYING;
  state->dead_timer = 0.0f;
}

/* ── asteroids_init ─────────────────────────────────────────────────────────
 * Full init: memset → restore audio/score → seed RNG → build models → reset.
 *
 * CRITICAL save/restore:
 *   memset zeros the entire GameState including audio.samples_per_second
 *   and volume fields that the platform set before this call.  We save them
 *   first and restore after the memset.                                     */
void asteroids_init(GameState *state) {
  /* Save fields that must survive the memset */
  int   sps = state->audio.samples_per_second;
  float mv  = state->audio.master_volume;
  float sv  = state->audio.sfx_volume;
  float mu  = state->audio.music_volume;
  int   bs  = state->best_score;

  memset(state, 0, sizeof(*state));

  /* Restore */
  state->audio.samples_per_second = sps;
  state->audio.master_volume = (mv > 0.0f) ? mv : 1.0f;
  state->audio.sfx_volume    = (sv > 0.0f) ? sv : 1.0f;
  state->audio.music_volume  = (mu > 0.0f) ? mu : 0.5f;
  state->best_score          = bs;

  /* Seed RNG — different asteroid shape each session */
  srand((unsigned int)time(NULL));

  /* Ship model (local space, y-up, unit-normalized):
   *   nose at (0, +1) — "up" at angle=0.
   *   fins at (±0.6, -0.7) — behind and to each side.
   * The whole model is then scaled by SHIP_RENDER_SCALE in draw_wireframe. */
  state->ship_model[0] = (Vec2){  0.0f, +1.0f };
  state->ship_model[1] = (Vec2){ -0.6f, -0.7f };
  state->ship_model[2] = (Vec2){ +0.6f, -0.7f };

  /* Asteroid model: 20 vertices at evenly spaced angles, perturbed by noise.
   * sinf(t) → x, cosf(t) → y gives CW order in y-up (top→right→bottom→left).
   * Unit radius (≈1 GU); scaled by SpaceObject.size at render time.        */
  for (int i = 0; i < ASTEROID_VERTS; i++) {
    float noise = 0.8f + ((float)(rand() % 100) * 0.004f); /* [0.8, 1.2) */
    float t     = ((float)i / (float)ASTEROID_VERTS) * 2.0f * PI;
    state->asteroid_model[i].x = sinf(t) * noise;
    state->asteroid_model[i].y = cosf(t) * noise;
  }

  reset_game(state);
}

/* ── asteroids_update ───────────────────────────────────────────────────────
 * Advance the simulation by dt seconds.
 * Input uses the new 7-button layout (game/base.h).
 *
 * Structure:
 *   1. PHASE_DEAD: count down, restart on RESTART button
 *   2. Music loop management
 *   3. Rotation (rotate_left / rotate_right)
 *   4. Thrust (thrust button)
 *   5. Fire   (fire button + cooldown)
 *   6. Physics integration + toroidal wrap
 *   7. Collision: bullets vs asteroids
 *   8. Collision: asteroids vs ship
 *   9. Compact pools
 *   10. Win condition (all asteroids cleared)                               */
void asteroids_update(GameState *state, const GameInput *input, float dt) {

  /* ── Dead phase ──────────────────────────────────────────────────────── */
  if (state->phase == PHASE_DEAD) {
    state->dead_timer -= dt;
    if (state->dead_timer <= 0.0f &&
        button_just_pressed(input->buttons.restart)) {
      if (state->score > state->best_score)
        state->best_score = state->score;
      asteroids_init(state);
      game_play_sound(&state->audio, SOUND_MUSIC_RESTART);
      return;
    }
    return;
  }

  /* ── Background music loop ───────────────────────────────────────────── */
  if (!game_is_sound_playing(&state->audio, SOUND_MUSIC_GAMEPLAY) &&
      !game_is_sound_playing(&state->audio, SOUND_MUSIC_RESTART)) {
    game_play_sound(&state->audio, SOUND_MUSIC_GAMEPLAY);
  }

  /* ── Rotate ──────────────────────────────────────────────────────────── */
  if (input->buttons.rotate_left.ended_down)
    state->player.angle -= SHIP_TURN_SPEED * dt;
  if (input->buttons.rotate_right.ended_down)
    state->player.angle += SHIP_TURN_SPEED * dt;

  /* Rotate SFX: sustain while any rotation key is held */
  if (input->buttons.rotate_left.ended_down ||
      input->buttons.rotate_right.ended_down) {
    game_sustain_sound(&state->audio, SOUND_ROTATE);
  }

  /* ── Thrust ──────────────────────────────────────────────────────────── */
  if (input->buttons.thrust.ended_down) {
    float ca = cosf(state->player.angle);
    float sa = sinf(state->player.angle);
    /* y-up thrust: dx = sin(a)*ACCEL, dy = cos(a)*ACCEL
     * At angle=0 (nose up): (0, ACCEL) → moves up ✓
     * At angle=π/2 (nose right): (ACCEL, 0) → moves right ✓             */
    state->player.dx += sa * SHIP_THRUST_ACCEL * dt;
    state->player.dy += ca * SHIP_THRUST_ACCEL * dt;

    /* Speed cap */
    float speed = sqrtf(state->player.dx * state->player.dx +
                        state->player.dy * state->player.dy);
    if (speed > SHIP_MAX_SPEED) {
      float inv = SHIP_MAX_SPEED / speed;
      state->player.dx *= inv;
      state->player.dy *= inv;
    }

    game_sustain_sound(&state->audio, SOUND_THRUST);
  }

  /* Friction (space-y deceleration) */
  state->player.dx *= SHIP_FRICTION;
  state->player.dy *= SHIP_FRICTION;

  /* ── Fire ────────────────────────────────────────────────────────────── */
  state->fire_timer -= dt;
  if (state->fire_timer < 0.0f) state->fire_timer = 0.0f;

  if (button_just_pressed(input->buttons.fire) &&
      state->fire_timer <= 0.0f) {
    if (add_bullet(state)) {
      game_play_sound(&state->audio, SOUND_FIRE);
      state->fire_timer = FIRE_COOLDOWN;
    }
  }

  /* ── Physics + toroidal wrap ─────────────────────────────────────────── */

  /* Ship */
  state->player.x += state->player.dx * dt;
  state->player.y += state->player.dy * dt;
  state->player.x = fmodf(state->player.x + GAME_UNITS_W, GAME_UNITS_W);
  state->player.y = fmodf(state->player.y + GAME_UNITS_H, GAME_UNITS_H);

  /* Asteroids */
  for (int i = 0; i < state->asteroid_count; i++) {
    SpaceObject *a = &state->asteroids[i];
    a->x += a->dx * dt;
    a->y += a->dy * dt;
    a->angle += 0.5f * dt;  /* slow CW spin */
    a->x = fmodf(a->x + GAME_UNITS_W, GAME_UNITS_W);
    a->y = fmodf(a->y + GAME_UNITS_H, GAME_UNITS_H);
  }

  /* Bullets */
  for (int i = 0; i < state->bullet_count; i++) {
    SpaceObject *b = &state->bullets[i];
    b->x += b->dx * dt;
    b->y += b->dy * dt;
    b->size -= dt;   /* lifetime countdown; bullet dies when size ≤ 0 */
    if (b->size <= 0.0f) b->active = 0;
    b->x = fmodf(b->x + GAME_UNITS_W, GAME_UNITS_W);
    b->y = fmodf(b->y + GAME_UNITS_H, GAME_UNITS_H);
  }

  /* ── Collision: bullets vs asteroids ─────────────────────────────────── */
  for (int bi = 0; bi < state->bullet_count; bi++) {
    if (!state->bullets[bi].active) continue;

    for (int ai = 0; ai < state->asteroid_count; ai++) {
      if (!state->asteroids[ai].active) continue;

      SpaceObject *b = &state->bullets[bi];
      SpaceObject *a = &state->asteroids[ai];

      float dx   = b->x - a->x;
      float dy   = b->y - a->y;
      float dist = sqrtf(dx * dx + dy * dy);

      if (dist < a->size + 0.02f) {  /* 0.02 GU ≈ 1 px bullet radius */
        b->active = 0;
        a->active = 0;

        /* Spatial pan: asteroid at left = -1 (full left) */
        float pan = (a->x / GAME_UNITS_W) * 2.0f - 1.0f;

        if (a->size >= ASTEROID_LARGE_SIZE) {
          state->score += 20;
          game_play_sound_panned(&state->audio, SOUND_EXPLODE_LARGE, pan);
          add_asteroid(state, a->x, a->y,
                        a->dy * 0.6f + a->dx * 0.4f,
                       -a->dx * 0.6f + a->dy * 0.4f,
                       ASTEROID_MEDIUM_SIZE);
          add_asteroid(state, a->x, a->y,
                       -a->dy * 0.6f + a->dx * 0.4f,
                        a->dx * 0.6f + a->dy * 0.4f,
                       ASTEROID_MEDIUM_SIZE);
        } else if (a->size >= ASTEROID_MEDIUM_SIZE) {
          state->score += 50;
          game_play_sound_panned(&state->audio, SOUND_EXPLODE_MEDIUM, pan);
          add_asteroid(state, a->x, a->y,
                        a->dy * 0.8f + a->dx * 0.3f,
                       -a->dx * 0.8f + a->dy * 0.3f,
                       ASTEROID_SMALL_SIZE);
          add_asteroid(state, a->x, a->y,
                       -a->dy * 0.8f + a->dx * 0.3f,
                        a->dx * 0.8f + a->dy * 0.3f,
                       ASTEROID_SMALL_SIZE);
        } else {
          state->score += 100;
          game_play_sound_panned(&state->audio, SOUND_EXPLODE_SMALL, pan);
        }

        break; /* one bullet hits one asteroid */
      }
    }
  }

  /* ── Collision: asteroids vs ship ─────────────────────────────────────── */
  for (int ai = 0; ai < state->asteroid_count; ai++) {
    if (!state->asteroids[ai].active) continue;
    SpaceObject *a = &state->asteroids[ai];

    float dx   = state->player.x - a->x;
    float dy   = state->player.y - a->y;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist < a->size + state->player.size) {
      state->player.active = 0;
      game_play_sound(&state->audio, SOUND_SHIP_DEATH);
      if (state->score > state->best_score)
        state->best_score = state->score;
      state->phase      = PHASE_DEAD;
      state->dead_timer = DEATH_DELAY;
      break;
    }
  }

  /* ── Compact pools ───────────────────────────────────────────────────── */
  compact_pool(state->bullets,   &state->bullet_count);
  compact_pool(state->asteroids, &state->asteroid_count);

  /* ── Win condition ────────────────────────────────────────────────────── */
  if (state->asteroid_count == 0 && state->phase == PHASE_PLAYING) {
    /* New wave: keep score, rebuild asteroid model with fresh RNG seed     */
    int bs = state->best_score;
    int sc = state->score;
    asteroids_init(state);
    state->score = sc;
    state->best_score = (sc > bs) ? sc : bs;
  }
}

/* ── asteroids_render ───────────────────────────────────────────────────────
 * Draw into bb using explicit world→pixel conversions.
 * Explicit coord mode (RENDER_COORD_MODE=1): all draw calls go through
 * world_x/y/w/h() from render_explicit.h, which handle the y-up flip.
 *
 * Draw order:
 *   1. Clear screen (black)
 *   2. Asteroids (white wireframe)
 *   3. Bullets   (yellow 3×3 dot)
 *   4. Ship      (cyan wireframe)
 *   5. HUD       (score top-left, best score top-right)
 *   6. Game-Over overlay (if PHASE_DEAD)                                    */
void asteroids_render(const GameState *state, Backbuffer *bb,
                      GameWorldConfig world_config) {
  RenderContext ctx = make_render_context(bb, world_config);

  /* ── 1. Clear ─────────────────────────────────────────────────────────── */
  draw_rect(bb, 0.0f, 0.0f, (float)bb->width, (float)bb->height, COLOR_BLACK);

  /* ── 2. Asteroids ────────────────────────────────────────────────────── */
  for (int i = 0; i < state->asteroid_count; i++) {
    const SpaceObject *a = &state->asteroids[i];
    if (!a->active) continue;
    float ca = cosf(a->angle), sa = sinf(a->angle);
    draw_wireframe(bb, &ctx,
                   state->asteroid_model, ASTEROID_VERTS,
                   a->x, a->y, ca, sa, a->size,
                   COLOR_WHITE);
  }

  /* ── 3. Bullets ──────────────────────────────────────────────────────── */
  for (int i = 0; i < state->bullet_count; i++) {
    const SpaceObject *b = &state->bullets[i];
    if (!b->active) continue;
    /* 3×3 dot centered on bullet world position */
    float px = world_x(&ctx, b->x);
    float py = world_y(&ctx, b->y);
    draw_rect(bb, px - 1.0f, py - 1.0f, 3.0f, 3.0f, COLOR_YELLOW);
  }

  /* ── 4. Ship ─────────────────────────────────────────────────────────── */
  if (state->player.active) {
    float ca = cosf(state->player.angle), sa = sinf(state->player.angle);
    draw_wireframe(bb, &ctx,
                   state->ship_model, SHIP_VERTS,
                   state->player.x, state->player.y, ca, sa,
                   SHIP_RENDER_SCALE,
                   COLOR_CYAN);
  }

  /* ── 5. HUD ──────────────────────────────────────────────────────────── */
  {
    char buf[32];

    /* Score (top-left): HUD_TOP_Y(0.5) = GAME_UNITS_H - 0.5 = 11.5 GU.
     * In y-up: pixel_y = (12 - 11.5) * 50 = 25 px from top edge.        */
    snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
    TextCursor sc = make_cursor(&ctx, 0.2f, HUD_TOP_Y(0.5f));
    draw_text(bb, sc.x, sc.y, (int)sc.scale, buf, COLOR_WHITE);

    /* Best (top-right, right-aligned) */
    snprintf(buf, sizeof(buf), "BEST: %d", state->best_score);
    float text_px_w = (float)(strlen(buf) * GLYPH_W) * sc.scale;
    TextCursor bc = make_cursor(&ctx, GAME_UNITS_W - 0.2f, HUD_TOP_Y(0.5f));
    draw_text(bb, bc.x - text_px_w, bc.y, (int)bc.scale, buf, COLOR_GRAY);
  }

  /* ── 6. Game-Over Overlay ────────────────────────────────────────────── */
  if (state->phase == PHASE_DEAD) {
    /* Semi-transparent dim overlay (50% black) */
    draw_rect(bb, 0.0f, 0.0f, (float)bb->width, (float)bb->height,
              0.0f, 0.0f, 0.0f, 0.55f);

    /* World center → pixel center */
    TextCursor c = make_cursor(&ctx,
                               GAME_UNITS_W * 0.5f,
                               GAME_UNITS_H * 0.5f);

    /* "GAME OVER" (centred horizontally, 1.5 lines above centre) */
    const char *title = "GAME OVER";
    float tw = (float)(strlen(title) * GLYPH_W) * c.scale;
    draw_text(bb, c.x - tw * 0.5f, c.y - c.line_h * 1.5f,
              (int)c.scale, title, COLOR_RED);

    /* Score line (centred, 0.3 lines above centre) */
    char score_buf[32];
    snprintf(score_buf, sizeof(score_buf), "SCORE: %d", state->score);
    float sw = (float)(strlen(score_buf) * GLYPH_W) * c.scale;
    draw_text(bb, c.x - sw * 0.5f, c.y - c.line_h * 0.3f,
              (int)c.scale, score_buf, COLOR_WHITE);

    /* Restart prompt (shown after death timer expires) */
    if (state->dead_timer <= 0.0f) {
      const char *prompt = "PRESS ENTER/R TO RESTART";
      float pw = (float)(strlen(prompt) * GLYPH_W) * c.scale;
      draw_text(bb, c.x - pw * 0.5f, c.y + c.line_h * 0.7f,
                (int)c.scale, prompt, COLOR_YELLOW);
    }
  }
}
