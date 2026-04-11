/* =============================================================================
 * game/game.c — Asteroids Game Logic
 * =============================================================================
 *
 * LESSONS COVERED:
 *   LESSON 04 — Game units: all positions in game units (not pixels)
 *   LESSON 04 — RenderContext: make_render_context, world_x/y helpers
 *   LESSON 05 — HUD rendering with TextCursor and HUD_TOP_Y
 *   LESSON 09 — Vec2, rotation matrix, draw_wireframe in y-up world space
 *   LESSON 10 — Ship physics: thrust, friction, toroidal wrap in game units
 *   LESSON 11 — Asteroid entity pool, size tiers, add_asteroid, reset_game
 *   LESSON 12 — Bullet pool, velocity from ship direction
 *   LESSON 13 — Circle collision, two-phase mark/compact, asteroid splitting
 *
 * COORDINATE CONVENTION (LESSON 04):
 *   All positions are in GAME UNITS with COORD_ORIGIN_LEFT_BOTTOM:
 *     • (0, 0)              = bottom-left corner of screen
 *     • (GAME_UNITS_W, 0)   = bottom-right
 *     • (0, GAME_UNITS_H)   = top-left
 *     • World y grows UPWARD (positive y = up on screen)
 *
 * ROTATION CONVENTION (LESSON 09):
 *   angle=0  → nose points UP (positive world y)
 *   angle>0  → clockwise rotation (positive = right turn)
 *
 *   Clockwise rotation matrix (y-up, matches screen CW appearance):
 *     rx = x * cos(a) + y * sin(a)
 *     ry = -x * sin(a) + y * cos(a)
 *
 *   WHY CLOCKWISE?  In y-up world space, the standard math rotation is CCW.
 *   But the game input says "right key = CW turn".  Using the CW formula
 *   makes positive angle = CW = right turn, matching user expectation.
 *
 * THRUST DIRECTION (LESSON 10):
 *   ship nose direction in world space: (sin(a), cos(a))
 *   dx += sin(a) * THRUST * dt
 *   dy += cos(a) * THRUST * dt     ← +cos (not -cos like y-down code!)
 *
 * NOTE: draw_wireframe stays in game.c (not utils/) because it depends on
 * GameState model arrays — it is inherently game-specific.
 * =============================================================================
 */

#include "game.h"
#include "../utils/render.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include <math.h>    /* sinf, cosf, fabsf, sqrtf */
#include <string.h>  /* memset, strlen             */
#include <stdio.h>   /* snprintf                   */
#include <stdlib.h>  /* rand, srand                */
#include <time.h>    /* time()                     */

#define PI 3.14159265358979323846f

/* ══════ prepare_input_frame ════════════════════════════════════════════════
 *
 * LESSON 06 — Carry last frame's ended_down into the new frame and reset
 * half_transition_count.  Without this, a held key would disappear each frame.
 *
 * JS equivalent:
 *   const prevButtons = { ...currentButtons };
 *   for (const key of Object.keys(currentButtons)) {
 *     currentButtons[key].halfTransitions = 0;
 *     currentButtons[key].endedDown = prevButtons[key].endedDown;
 *   }                                                                     */
void prepare_input_frame(const GameInput *old, GameInput *current) {
    for (int i = 0; i < BUTTON_COUNT; i++) {
        current->buttons[i].ended_down           = old->buttons[i].ended_down;
        current->buttons[i].half_transition_count = 0;
    }
    current->quit = 0;
}

/* ══════ compact_pool ═══════════════════════════════════════════════════════
 *
 * LESSON 13 — Remove dead objects using the swap-with-tail idiom.
 * Replace the dead slot with the last live element, decrement count.
 * O(n), mutates in-place, no allocation.
 *
 * Two-phase approach: detection (mark inactive) → removal (compact).
 * This avoids double-updating swapped elements mid-loop.
 *
 * JS equivalent:  pool = pool.filter(obj => obj.active);  // O(n), new array */
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

/* ══════ add_asteroid ═══════════════════════════════════════════════════════
 *
 * LESSON 11 — Append a new asteroid to the pool.
 * x, y   — spawn position in GAME UNITS
 * dx, dy — initial velocity in GAME UNITS/SECOND
 * size   — collision radius in GAME UNITS (ASTEROID_LARGE/MEDIUM/SMALL_SIZE) */
static int add_asteroid(GameState *state,
                        float x, float y,
                        float dx, float dy,
                        float size)
{
    if (state->asteroid_count >= MAX_ASTEROIDS) return 0;
    SpaceObject *a = &state->asteroids[state->asteroid_count++];
    a->x      = x;
    a->y      = y;
    a->dx     = dx;
    a->dy     = dy;
    a->size   = size;
    a->angle  = 0.0f;
    a->active = 1;
    return 1;
}

/* ══════ add_bullet ═════════════════════════════════════════════════════════
 *
 * LESSON 12 — Spawn a bullet from the ship nose.
 *
 * LESSON 09/12 — Y-UP nose direction:
 *   Model nose at (0, +5) (positive y = up in world space).
 *   CW rotation formula gives nose world direction: (sin(a), cos(a)).
 *   Nose world offset: (sin(a) * 5 * SHIP_RENDER_SCALE, cos(a) * 5 * SHIP_RENDER_SCALE)
 *   Bullet velocity:   (sin(a) * BULLET_SPEED, cos(a) * BULLET_SPEED)
 *                                                        ↑ +cos (not -cos) */
static int add_bullet(GameState *state) {
    if (state->bullet_count >= MAX_BULLETS) return 0;

    float ca = cosf(state->player.angle);
    float sa = sinf(state->player.angle);

    /* Nose model vertex is (0, +5) in local space; apply CW rotation + scale */
    /* CW: rx = x*ca + y*sa, ry = -x*sa + y*ca                               */
    /* For (0, +5): rx = 0*ca + 5*sa = 5*sa, ry = -0*sa + 5*ca = 5*ca       */
    float tip_x = state->player.x + 5.0f * sa * SHIP_RENDER_SCALE;
    float tip_y = state->player.y + 5.0f * ca * SHIP_RENDER_SCALE;

    SpaceObject *b = &state->bullets[state->bullet_count++];
    b->x      = tip_x;
    b->y      = tip_y;
    b->dx     =  sa * BULLET_SPEED;
    b->dy     = +ca * BULLET_SPEED;  /* +cos (y-up): positive y = up on screen */
    b->angle  = 0.0f;
    b->size   = BULLET_LIFETIME;     /* bullet "lives" shrink over time        */
    b->active = 1;
    return 1;
}

/* ══════ draw_wireframe ══════════════════════════════════════════════════════
 *
 * LESSON 09 — Draw a polygon wireframe from a local-space vertex model.
 *
 * Steps:
 *   1. Compute cos/sin once (expensive trig, amortised over all vertices)
 *   2. For each vertex: apply CW rotation → scale → translate to world pos
 *   3. Convert each world vertex to pixel coords via world_x/y(ctx, …)
 *   4. Draw Bresenham line between consecutive pixel vertices (toroidal wrap)
 *
 * CW ROTATION (y-up world space):
 *   rx = v.x * cos(a) + v.y * sin(a)
 *   ry = -v.x * sin(a) + v.y * cos(a)
 *
 * Y-UP MODEL VERTICES (ship nose points UP = positive world y):
 *   ship_model[0] = ( 0, +5) → nose
 *   ship_model[1] = (-3, -3.5) → left fin
 *   ship_model[2] = ( 3, -3.5) → right fin
 *
 * JS equivalent:
 *   ctx.save(); ctx.translate(cx, cy); ctx.rotate(angle);
 *   ctx.scale(scale, scale); ctx.stroke(path); ctx.restore();
 *
 * Parameters:
 *   bb         — pixel buffer to draw into
 *   ctx        — RenderContext (pre-computed world→pixel coefficients)
 *   model      — local-space vertex array
 *   vert_count — number of vertices
 *   cx, cy     — world-space center position (game units)
 *   angle      — rotation angle in radians
 *   scale      — world-unit scale (game-unit size per model-unit)
 *   color      — packed uint32_t RGBA wire color (WF_WHITE, WF_CYAN, etc.)  */
static void draw_wireframe(Backbuffer *bb, const RenderContext *ctx,
                           const Vec2 *model, int vert_count,
                           float cx, float cy,
                           float angle, float scale,
                           uint32_t color)
{
    float ca = cosf(angle);
    float sa = sinf(angle);

    /* Transform vertices: local → world space */
    Vec2 world[64];
    for (int i = 0; i < vert_count; i++) {
        /* CW rotation (y-up): rx = x*cos + y*sin, ry = -x*sin + y*cos */
        float rx = model[i].x * ca + model[i].y * sa;
        float ry = -(model[i].x * sa) + model[i].y * ca;
        /* Scale then translate */
        world[i].x = cx + rx * scale;
        world[i].y = cy + ry * scale;
    }

    /* Draw edges in pixel space (world_x/y converts world → pixel coords) */
    for (int i = 0; i < vert_count; i++) {
        int j = (i + 1) % vert_count;
        int px0 = (int)world_x(ctx, world[i].x);
        int py0 = (int)world_y(ctx, world[i].y);
        int px1 = (int)world_x(ctx, world[j].x);
        int py1 = (int)world_y(ctx, world[j].y);
        draw_line(bb, px0, py0, px1, py1, color);
    }
}

/* ══════ reset_game ══════════════════════════════════════════════════════════
 *
 * LESSON 11 — Reset gameplay fields; preserve audio config, models, best_score.
 * Called from asteroids_init() (which has already done the memset).
 *
 * SPAWN POSITIONS (game units, y-up):
 *   Ship center: (GAME_UNITS_W/2, GAME_UNITS_H/2) = (8.0, 7.5)
 *   Asteroid corners are near the four edges, away from the ship center.
 *   Note: in y-up coords, y=1.875 is near BOTTOM, y=13.125 is near TOP.  */
static void reset_game(GameState *state) {
    /* Respawn player at world center, stationary */
    state->player.x      = GAME_UNITS_W * 0.5f;   /* 8.0 */
    state->player.y      = GAME_UNITS_H * 0.5f;   /* 7.5 */
    state->player.dx     = 0.0f;
    state->player.dy     = 0.0f;
    state->player.angle  = 0.0f;
    state->player.size   = SHIP_COLLISION_RADIUS;
    state->player.active = 1;

    state->asteroid_count = 0;
    state->bullet_count   = 0;
    state->fire_timer     = 0.0f;

    /* Spawn 4 large asteroids near the corners (1.875 units from each edge).
     * Velocity range: old ±10 px/s → ÷32 = ±0.3125 units/s
     * Formula: (rand() % 40 - 20) * 0.5f / 32.0f = (rand()%40-20) * 0.015625f */
    float corner = 1.875f;   /* 60 px / 32 = 1.875 game units */
    float wR = GAME_UNITS_W - corner;
    float hT = GAME_UNITS_H - corner;
    float vm = 0.015625f;    /* velocity multiplier (0.5/32) */

    add_asteroid(state, corner, corner,
                 (rand() % 40 - 20) * vm, (rand() % 40 - 20) * vm,
                 ASTEROID_LARGE_SIZE);
    add_asteroid(state, wR, corner,
                 (rand() % 40 - 20) * vm, (rand() % 40 - 20) * vm,
                 ASTEROID_LARGE_SIZE);
    add_asteroid(state, corner, hT,
                 (rand() % 40 - 20) * vm, (rand() % 40 - 20) * vm,
                 ASTEROID_LARGE_SIZE);
    add_asteroid(state, wR, hT,
                 (rand() % 40 - 20) * vm, (rand() % 40 - 20) * vm,
                 ASTEROID_LARGE_SIZE);

    state->score      = 0;
    state->phase      = PHASE_PLAYING;
    state->dead_timer = 0.0f;
}

/* ══════ asteroids_init ══════════════════════════════════════════════════════
 *
 * LESSON 11 — Full game initialisation.
 *
 * CRITICAL: memset zeros the entire GameState, including audio config set
 * by the platform.  Save/restore pattern preserves those fields:
 *   int sps = state->audio.samples_per_second;
 *   memset(state, 0, sizeof(*state));
 *   state->audio.samples_per_second = sps;
 *
 * SHIP MODEL — Y-UP VERTICES (LESSON 09):
 *   (0, +5) → nose points up (positive world y = up on screen) ✓
 *   (-3, -3.5) → left fin (negative y = below center in y-up)
 *   ( 3, -3.5) → right fin
 *   Compared to y-down original: y-signs are flipped.                    */
void asteroids_init(GameState *state) {
    /* ── Save fields that survive memset ─────────────────────────────── */
    int   sps = state->audio.samples_per_second;
    float mv  = state->audio.master_volume;
    float sv  = state->audio.sfx_volume;
    int   bs  = state->best_score;

    memset(state, 0, sizeof(*state));

    /* ── Restore ─────────────────────────────────────────────────────── */
    state->audio.samples_per_second = sps;
    state->audio.master_volume = (mv > 0.0f) ? mv : 1.0f;
    state->audio.sfx_volume    = (sv > 0.0f) ? sv : 1.0f;
    state->best_score          = bs;

    /* ── Seed RNG — different asteroid shape each game ─────────────── */
    srand((unsigned int)time(NULL));

    /* ── Build ship model (y-up: nose at positive world y) ──────────── */
    state->ship_model[0] = (Vec2){  0.0f, +5.0f };   /* nose (top, y-up)  */
    state->ship_model[1] = (Vec2){ -3.0f, -3.5f };   /* left fin          */
    state->ship_model[2] = (Vec2){  3.0f, -3.5f };   /* right fin         */

    /* ── Build asteroid model (jagged circle, unit radius ≈ 1) ─────── */
    for (int i = 0; i < ASTEROID_VERTS; i++) {
        float noise = 0.8f + ((float)(rand() % 100) / 100.0f) * 0.4f;
        float t     = ((float)i / (float)ASTEROID_VERTS) * 2.0f * PI;
        state->asteroid_model[i].x = sinf(t) * noise;
        state->asteroid_model[i].y = cosf(t) * noise;
    }

    reset_game(state);
}

/* ══════ asteroids_update ════════════════════════════════════════════════════
 *
 * LESSON 10–13 — Advance the game simulation by dt seconds.
 *
 * Structure:
 *   1. Dead phase: wait for FIRE to restart
 *   2. Gameplay music loop
 *   3. Input → rotation (no unit conversion needed — radians)
 *   4. Input → thrust (dy += +cos(a)*THRUST — y-up sign)
 *   5. Physics: velocity integration + toroidal wrap in game units
 *   6. Bullet → asteroid collision
 *   7. Asteroid → ship collision
 *   8. Compact pools
 *   9. Win condition check
 */
void asteroids_update(GameState *state, const GameInput *input, float dt) {

    /* ── Dead Phase ─────────────────────────────────────────────────── */
    if (state->phase == PHASE_DEAD) {
        state->dead_timer -= dt;
        if (state->dead_timer <= 0.0f &&
            input->fire.half_transition_count > 0 &&
            input->fire.ended_down)
        {
            if (state->score > state->best_score)
                state->best_score = state->score;
            asteroids_init(state);
            game_play_sound(&state->audio, SOUND_MUSIC_RESTART);
            return;
        }
        return;
    }

    /* ── Gameplay music loop ─────────────────────────────────────────── */
    if (!game_is_sound_playing(&state->audio, SOUND_MUSIC_GAMEPLAY) &&
        !game_is_sound_playing(&state->audio, SOUND_MUSIC_RESTART)) {
        game_play_sound(&state->audio, SOUND_MUSIC_GAMEPLAY);
    }

    /* ── Input: Rotate ────────────────────────────────────────────────
     * Radians are unit-less — no conversion needed.                    */
    if (input->left.ended_down)
        state->player.angle -= SHIP_TURN_SPEED * dt;
    if (input->right.ended_down)
        state->player.angle += SHIP_TURN_SPEED * dt;

    if (input->left.ended_down || input->right.ended_down)
        game_sustain_sound(&state->audio, SOUND_ROTATE);

    /* ── Input: Thrust ────────────────────────────────────────────────
     * LESSON 10 — Y-UP SIGN: nose direction is (sin(a), cos(a)).
     * dy += +cos(a) * THRUST * dt  (positive = up in world space)
     * Compare: old y-down code used -cos(a) because screen y was inverted. */
    if (input->up.ended_down) {
        float ca = cosf(state->player.angle);
        float sa = sinf(state->player.angle);
        state->player.dx +=  sa * SHIP_THRUST_ACCEL * dt;
        state->player.dy += +ca * SHIP_THRUST_ACCEL * dt;  /* +cos in y-up! */

        float speed = sqrtf(state->player.dx * state->player.dx +
                            state->player.dy * state->player.dy);
        if (speed > SHIP_MAX_SPEED) {
            float inv = SHIP_MAX_SPEED / speed;
            state->player.dx *= inv;
            state->player.dy *= inv;
        }

        game_sustain_sound(&state->audio, SOUND_THRUST);
    }

    state->player.dx *= SHIP_FRICTION;
    state->player.dy *= SHIP_FRICTION;

    /* ── Input: Fire ─────────────────────────────────────────────────── */
    state->fire_timer -= dt;
    if (state->fire_timer < 0.0f) state->fire_timer = 0.0f;

    if (input->fire.half_transition_count > 0 && input->fire.ended_down
        && state->fire_timer <= 0.0f)
    {
        if (add_bullet(state)) {
            game_play_sound(&state->audio, SOUND_FIRE);
            state->fire_timer = FIRE_COOLDOWN;
        }
    }

    /* ── Physics: Integrate & Wrap ────────────────────────────────────
     * LESSON 10 — Toroidal wrap in game units:
     *   x = fmodf(x + GAME_UNITS_W, GAME_UNITS_W)
     * Same formula as pixel version, just uses GAME_UNITS_W/H instead.
     * Works for y-up and y-down equally — wrap is always in [0, bound). */

    state->player.x += state->player.dx * dt;
    state->player.y += state->player.dy * dt;
    state->player.x = fmodf(state->player.x + GAME_UNITS_W, GAME_UNITS_W);
    state->player.y = fmodf(state->player.y + GAME_UNITS_H, GAME_UNITS_H);

    for (int i = 0; i < state->asteroid_count; i++) {
        SpaceObject *a = &state->asteroids[i];
        a->x += a->dx * dt;
        a->y += a->dy * dt;
        a->angle += 0.5f * dt;   /* slow constant spin (radians/s) */
        a->x = fmodf(a->x + GAME_UNITS_W, GAME_UNITS_W);
        a->y = fmodf(a->y + GAME_UNITS_H, GAME_UNITS_H);
    }

    for (int i = 0; i < state->bullet_count; i++) {
        SpaceObject *b = &state->bullets[i];
        b->x    += b->dx * dt;
        b->y    += b->dy * dt;
        b->size -= dt;    /* shrinks over BULLET_LIFETIME seconds */
        if (b->size <= 0.0f) b->active = 0;
        b->x = fmodf(b->x + GAME_UNITS_W, GAME_UNITS_W);
        b->y = fmodf(b->y + GAME_UNITS_H, GAME_UNITS_H);
    }

    /* ── Collision: Bullets vs Asteroids ─────────────────────────────
     * LESSON 13 — Circle collision: dist < r_asteroid + r_bullet.
     * Bullet hit radius = 0.03125 units (~1 pixel at reference res).
     * IMPORTANT: both loops mark inactive; compact runs AFTER both loops.*/
    for (int bi = 0; bi < state->bullet_count; bi++) {
        if (!state->bullets[bi].active) continue;

        for (int ai = 0; ai < state->asteroid_count; ai++) {
            if (!state->asteroids[ai].active) continue;

            SpaceObject *b = &state->bullets[bi];
            SpaceObject *a = &state->asteroids[ai];

            float ddx  = b->x - a->x;
            float ddy  = b->y - a->y;
            float dist = sqrtf(ddx * ddx + ddy * ddy);

            if (dist < a->size + 0.03125f) {
                b->active = 0;
                a->active = 0;

                /* Spatial pan: asteroid x / world width → [-1, +1] */
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
                break;  /* one bullet destroys one asteroid */
            }
        }
    }

    /* ── Collision: Asteroids vs Ship ────────────────────────────────── */
    for (int ai = 0; ai < state->asteroid_count; ai++) {
        if (!state->asteroids[ai].active) continue;

        SpaceObject *a = &state->asteroids[ai];
        float ddx  = state->player.x - a->x;
        float ddy  = state->player.y - a->y;
        float dist = sqrtf(ddx * ddx + ddy * ddy);

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

    compact_pool(state->bullets,   &state->bullet_count);
    compact_pool(state->asteroids, &state->asteroid_count);

    /* ── Win condition: all asteroids cleared ────────────────────────── */
    if (state->asteroid_count == 0 && state->phase == PHASE_PLAYING) {
        int bs = state->best_score;
        int sc = state->score;
        asteroids_init(state);
        state->score = sc;
        if (sc > bs) bs = sc;
        state->best_score = bs;
    }
}

/* ══════ asteroids_render ════════════════════════════════════════════════════
 *
 * LESSON 04/05/09 — Draw everything for the current frame.
 *
 * RENDER CONTEXTS (LESSON 04/05):
 *   world_ctx — game camera applied (for game objects: ship, asteroids)
 *   hud_ctx   — camera stripped (HUD stays fixed regardless of camera pan)
 *   Both built from world_config, which the platform passed in.
 *
 * DRAW ORDER:
 *   1. Clear screen (black rect in pixel space)
 *   2. Asteroids (wireframe via world_ctx)
 *   3. Bullets (pixel dots via world_ctx)
 *   4. Ship (wireframe via world_ctx)
 *   5. HUD score/best (text via hud_ctx + TextCursor)
 *   6. Game-over overlay (semi-transparent rect + text, pixel space)    */
void asteroids_render(const GameState *state, Backbuffer *bb,
                      GameWorldConfig world_config)
{
    /* ── Build render contexts ────────────────────────────────────────
     * make_render_context runs once; all subsequent draw calls are
     * branch-free float math.                                           */
    RenderContext world_ctx = make_render_context(bb, world_config);

    /* HUD context: same origin, no camera pan or zoom */
    GameWorldConfig hud_cfg = world_config;
    hud_cfg.camera_x    = 0.0f;
    hud_cfg.camera_y    = 0.0f;
    hud_cfg.camera_zoom = 1.0f;
    RenderContext hud_ctx = make_render_context(bb, hud_cfg);

    /* ── 1. Clear screen ──────────────────────────────────────────────
     * draw_rect expects PIXEL coordinates — pass the full backbuffer size. */
    draw_rect(bb, 0.0f, 0.0f, (float)bb->width, (float)bb->height,
              0.0f, 0.0f, 0.0f, 1.0f);

    /* ── 2. Asteroids ────────────────────────────────────────────────── */
    for (int i = 0; i < state->asteroid_count; i++) {
        const SpaceObject *a = &state->asteroids[i];
        if (!a->active) continue;
        draw_wireframe(bb, &world_ctx,
                       state->asteroid_model, ASTEROID_VERTS,
                       a->x, a->y,
                       a->angle, a->size,
                       WF_WHITE);
    }

    /* ── 3. Bullets ───────────────────────────────────────────────────── */
    for (int i = 0; i < state->bullet_count; i++) {
        const SpaceObject *b = &state->bullets[i];
        if (!b->active) continue;
        int px = (int)world_x(&world_ctx, b->x);
        int py = (int)world_y(&world_ctx, b->y);
        draw_pixel_w(bb, px,   py,   WF_YELLOW);
        draw_pixel_w(bb, px+1, py,   WF_YELLOW);
        draw_pixel_w(bb, px,   py+1, WF_YELLOW);
    }

    /* ── 4. Ship ──────────────────────────────────────────────────────── */
    if (state->player.active) {
        draw_wireframe(bb, &world_ctx,
                       state->ship_model, SHIP_VERTS,
                       state->player.x, state->player.y,
                       state->player.angle, SHIP_RENDER_SCALE,
                       WF_CYAN);
    }

    /* ── 5. HUD (score + best) ────────────────────────────────────────
     * LESSON 05 — Use make_cursor() to place text in world space.
     * HUD_TOP_Y(offset) = GAME_UNITS_H - offset → near top of screen.
     * make_cursor() converts world coords to pixel coords once.
     * text_scale is guaranteed ≥ 1.0 by make_render_context().        */
    {
        char buf[32];
        int  sc = (int)hud_ctx.text_scale;

        /* Score at top-left */
        TextCursor score_cur = make_cursor(&hud_ctx, 0.25f, HUD_TOP_Y(0.25f));
        snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
        draw_text(bb, score_cur.x, score_cur.y, sc,
                  buf, COLOR_WHITE);

        /* Best at top-right (right-aligned).
         * Cursor marks the right edge; subtract text pixel width to
         * find the left edge.  text_w = strlen * GLYPH_W * scale.    */
        snprintf(buf, sizeof(buf), "BEST: %d", state->best_score);
        TextCursor best_cur = make_cursor(&hud_ctx,
                                          GAME_UNITS_W - 0.25f, HUD_TOP_Y(0.25f));
        int best_w = (int)strlen(buf) * GLYPH_W * sc;
        draw_text(bb, best_cur.x - (float)best_w, best_cur.y, sc,
                  buf, COLOR_GREY);
    }

    /* ── 6. Game-Over Overlay ─────────────────────────────────────────── */
    if (state->phase == PHASE_DEAD) {
        /* Semi-transparent black overlay — pixel space, full screen */
        draw_rect(bb, 0.0f, 0.0f, (float)bb->width, (float)bb->height,
                  0.0f, 0.0f, 0.0f, 0.5f);

        TextCursor center = make_cursor(&hud_ctx, GAME_UNITS_W * 0.5f,
                                        GAME_UNITS_H * 0.5f);
        int sc = (int)center.scale;

        /* "GAME OVER" centred */
        const char *msg1 = "GAME OVER";
        int w1 = (int)strlen(msg1) * GLYPH_W * sc;
        draw_text(bb, center.x - (float)w1 * 0.5f,
                  center.y - (float)(GLYPH_H * sc * 2),
                  sc, msg1, COLOR_RED);

        /* Score line */
        char score_buf[32];
        snprintf(score_buf, sizeof(score_buf), "SCORE: %d", state->score);
        int ws = (int)strlen(score_buf) * GLYPH_W * sc;
        draw_text(bb, center.x - (float)ws * 0.5f,
                  center.y - (float)(GLYPH_H * sc),
                  sc, score_buf, COLOR_WHITE);

        /* Restart prompt (shown after death delay) */
        if (state->dead_timer <= 0.0f) {
            const char *msg2 = "PRESS FIRE TO RESTART";
            int w2 = (int)strlen(msg2) * GLYPH_W * sc;
            draw_text(bb, center.x - (float)w2 * 0.5f,
                      center.y + (float)(GLYPH_H * sc),
                      sc, msg2, COLOR_YELLOW);
        }
    }
}
