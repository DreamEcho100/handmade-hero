/* =============================================================================
 * game.c — Asteroids Game Logic
 * =============================================================================
 *
 * This file contains:
 *   - prepare_input_frame   input double-buffer preparation
 *   - compact_pool          dead-object compaction (swap-with-tail idiom)
 *   - add_asteroid          spawn a new asteroid into the pool
 *   - add_bullet            spawn a new bullet into the pool
 *   - draw_wireframe        rotate + scale + draw a polygon (game-specific)
 *   - reset_game            selective game logic reset (no memset)
 *   - asteroids_init        full init (memset + model build + reset_game)
 *   - asteroids_update      per-frame game simulation
 *   - asteroids_render      per-frame drawing
 *
 * NOTE: draw_wireframe stays here (not in utils/) because it uses the
 * asteroid/ship model arrays stored in GameState — it is inherently
 * game-specific.  The primitive drawing functions it calls (draw_line) live
 * in utils/draw-shapes.c.
 * =============================================================================
 */

#include "game.h"
#include "utils/draw-shapes.h"
#include "utils/draw-text.h"
#include <math.h>    /* sinf, cosf, fabsf, sqrtf */
#include <string.h>  /* memset                    */
#include <stdio.h>   /* snprintf                  */
#include <stdlib.h>  /* rand, srand               */
#include <time.h>    /* time()                    */

/* PI — C has no M_PI guarantee without _GNU_SOURCE, so we define our own. */
#define PI 3.14159265358979323846f

/* ══════ prepare_input_frame ════════════════════════════════════════════════
 *
 * WHY THIS FUNCTION?
 * ──────────────────
 * Between frames, keys that were held in the last frame are still held at
 * the start of the next frame — unless a key-up event arrives.  We "carry
 * forward" ended_down from the previous frame so that if no event arrives
 * this frame the button state correctly reads "still held".
 *
 * We reset half_transition_count to 0 because a new frame starts with zero
 * transitions; the platform event loop will increment it when events arrive.
 *
 * JS equivalent of the overall pattern:
 *   // Save previous state before overwriting
 *   const prevButtons = { ...currentButtons };
 *   // Reset counters
 *   for (const key of Object.keys(currentButtons)) {
 *     currentButtons[key].halfTransitions = 0;
 *     currentButtons[key].endedDown = prevButtons[key].endedDown;
 *   }
 */
void prepare_input_frame(const GameInput *old, GameInput *current) {
    for (int i = 0; i < BUTTON_COUNT; i++) {
        current->buttons[i].ended_down          = old->buttons[i].ended_down;
        current->buttons[i].half_transition_count = 0;
    }
    /* quit is a one-shot flag; don't carry it forward */
    current->quit = 0;
}

/* ══════ compact_pool ═══════════════════════════════════════════════════════
 *
 * Removes inactive (dead) objects from a pool using the swap-with-tail idiom:
 * replace the dead slot with the last live element, decrement count.
 * This avoids shifting elements and keeps compaction O(n).
 *
 * COURSE NOTE: The reference C code iterates and removes inline during
 * the update loop, which can cause double-updates of swapped elements.
 * The course separates detection (mark inactive) from removal (compact)
 * into two distinct steps to avoid that bug.
 *
 * JS equivalent:
 *   pool = pool.filter(obj => obj.active);   // O(n), creates new array
 *   // The C version is O(n) but mutates in-place with no allocation.
 */
static void compact_pool(SpaceObject *pool, int *count) {
    int i = 0;
    while (i < *count) {
        if (!pool[i].active) {
            /* Replace with the last element and shrink the pool by one.
               DO NOT increment i — the swapped element must be checked too. */
            pool[i] = pool[*count - 1];
            (*count)--;
        } else {
            i++;
        }
    }
}

/* ══════ add_asteroid ═══════════════════════════════════════════════════════
 *
 * Append a new asteroid to the pool.  Returns 0 if the pool is full.
 *
 * Parameters:
 *   state  — game state (asteroids[] pool + count)
 *   x, y   — spawn position
 *   dx, dy — initial velocity
 *   size   — collision radius (controls draw scale too)
 */
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
 * Append a bullet fired from the tip of the ship.
 * The ship angle is used to compute the bullet's direction.
 *
 * COURSE NOTE: The bullet spawns at the ship tip — the nose vertex of the
 * ship model (ship_model[0]) rotated by the ship's current angle.
 */
static int add_bullet(GameState *state) {
    if (state->bullet_count >= MAX_BULLETS) return 0;

    /* Compute ship nose direction */
    float ca = cosf(state->player.angle);
    float sa = sinf(state->player.angle);

    /* Ship nose is at model[0] = (0, -5) — rotated by ship angle.
       Nose direction in world space: (+sin(a), -cos(a)).
       tip_x = cx + sin(a)*5,  tip_y = cy + (-cos(a))*5               */
    float tip_x = state->player.x + ( sa * 5.0f);
    float tip_y = state->player.y + (-ca * 5.0f);

    SpaceObject *b = &state->bullets[state->bullet_count++];
    b->x      = tip_x;
    b->y      = tip_y;
    b->dx     =  sa * BULLET_SPEED;
    b->dy     = -ca * BULLET_SPEED;
    b->angle  = 0.0f;
    b->size   = 2.0f;
    b->active = 1;
    return 1;
}

/* ══════ draw_wireframe ══════════════════════════════════════════════════════
 *
 * Draw a polygon wireframe defined by a vertex model in local space.
 * Applies rotation and uniform scale, then translates to world-space position.
 *
 * Steps:
 *   1. Compute cos/sin of angle once (expensive trig, amortised over all verts)
 *   2. For each vertex v: rotate → scale → translate
 *   3. Draw a line from each transformed vertex to the next (closing at wrap)
 *
 * JS equivalent:
 *   ctx.save();
 *   ctx.translate(x, y);
 *   ctx.rotate(angle);
 *   ctx.scale(size, size);
 *   ctx.stroke(path);
 *   ctx.restore();
 *
 * COURSE NOTE: The reference source computes trig per draw_wireframe call.
 * For this course we keep the same approach; a performance version would
 * pre-compute cos/sin in the update step and store in SpaceObject.        */
static void draw_wireframe(AsteroidsBackbuffer *bb,
                           const Vec2 *model, int vert_count,
                           float cx, float cy,
                           float angle, float scale,
                           uint32_t color)
{
    /* Pre-compute rotation matrix entries (only once per object) */
    float ca = cosf(angle);
    float sa = sinf(angle);

    /* Transform all vertices from local to world space */
    Vec2 world[64];  /* 64 is safe for any model in this game */
    for (int i = 0; i < vert_count; i++) {
        /* Rotate using 2×2 matrix: [cos -sin] [x]   [x·cos - y·sin]
                                    [sin  cos] [y] = [x·sin + y·cos] */
        float rx = model[i].x * ca - model[i].y * sa;
        float ry = model[i].x * sa + model[i].y * ca;
        /* Scale then translate to world position */
        world[i].x = cx + rx * scale;
        world[i].y = cy + ry * scale;
    }

    /* Draw edges: connect each vertex to the next, close by connecting last→first */
    for (int i = 0; i < vert_count; i++) {
        int j = (i + 1) % vert_count;  /* % wraps the last vertex back to 0 */
        draw_line(bb,
                  (int)world[i].x, (int)world[i].y,
                  (int)world[j].x, (int)world[j].y,
                  color);
    }
}

/* ══════ reset_game ══════════════════════════════════════════════════════════
 *
 * Reset only game-logic fields, preserving audio config, models, best_score.
 * Called from asteroids_init() (which has already done the memset).
 *
 * On restart, asteroids_init() is called again (memset + model rebuild),
 * and then reset_game() re-spawns the starting asteroids.
 */
static void reset_game(GameState *state) {
    /* Respawn player at screen centre, stationary */
    state->player.x      = SCREEN_W / 2.0f;
    state->player.y      = SCREEN_H / 2.0f;
    state->player.dx     = 0.0f;
    state->player.dy     = 0.0f;
    state->player.angle  = 0.0f;
    state->player.size   = 5.0f;
    state->player.active = 1;

    /* Clear pools */
    state->asteroid_count = 0;
    state->bullet_count   = 0;
    state->fire_timer     = 0.0f;

    /* Spawn 4 large asteroids at screen corners (avoid centre where ship is) */
    add_asteroid(state, 60.0f,            60.0f,
                 (rand() % 40 - 20) * 0.5f, (rand() % 40 - 20) * 0.5f,
                 ASTEROID_LARGE_SIZE);
    add_asteroid(state, SCREEN_W - 60.0f, 60.0f,
                 (rand() % 40 - 20) * 0.5f, (rand() % 40 - 20) * 0.5f,
                 ASTEROID_LARGE_SIZE);
    add_asteroid(state, 60.0f,            SCREEN_H - 60.0f,
                 (rand() % 40 - 20) * 0.5f, (rand() % 40 - 20) * 0.5f,
                 ASTEROID_LARGE_SIZE);
    add_asteroid(state, SCREEN_W - 60.0f, SCREEN_H - 60.0f,
                 (rand() % 40 - 20) * 0.5f, (rand() % 40 - 20) * 0.5f,
                 ASTEROID_LARGE_SIZE);

    state->score      = 0;
    state->phase      = PHASE_PLAYING;
    state->dead_timer = 0.0f;
}

/* ══════ asteroids_init ══════════════════════════════════════════════════════
 *
 * Full game initialisation.  Called:
 *   • Once from main() before the game loop
 *   • Again from asteroids_update() when the player triggers a restart
 *
 * CRITICAL: We use memset to zero the entire GameState, which also zeros
 * the audio config fields set by the platform (samples_per_second, volumes).
 * We must save those fields before the memset and restore them after.
 *
 * Save/restore pattern (learned from Snake course; avoids silent audio bug):
 *   int sps = state->audio.samples_per_second;
 *   memset(state, 0, ...);
 *   state->audio.samples_per_second = sps;   ← restore!
 *
 * WHY srand HERE?
 * ───────────────
 * srand seeds the random number generator.  Seeding with time(NULL) ensures
 * a different asteroid shape on each play.  We seed in asteroids_init rather
 * than main() so that each restart also gets a freshly randomised asteroid
 * model.  time() returns seconds since the Unix epoch.
 *
 * JS equivalent:  Math.random() is automatically seeded — no explicit call needed.
 */
void asteroids_init(GameState *state) {
    /* ── Save fields that must survive the memset ─────────────────────── */
    int   sps = state->audio.samples_per_second;
    float mv  = state->audio.master_volume;
    float sv  = state->audio.sfx_volume;
    int   bs  = state->best_score;

    /* ── Zero everything ─────────────────────────────────────────────── */
    memset(state, 0, sizeof(*state));

    /* ── Restore ─────────────────────────────────────────────────────── */
    state->audio.samples_per_second = sps;
    /* Use saved volumes; if this is the very first call they may be 0 —
       game_audio_init() (called from main() before this) sets the defaults,
       so normally mv/sv are already non-zero.  Guard anyway.             */
    state->audio.master_volume = (mv > 0.0f) ? mv : 1.0f;
    state->audio.sfx_volume    = (sv > 0.0f) ? sv : 1.0f;
    state->best_score          = bs;

    /* ── Seed RNG ────────────────────────────────────────────────────── */
    srand((unsigned int)time(NULL));

    /* ── Build ship model (triangle, local space, nose pointing up) ─── */
    state->ship_model[0] = (Vec2){  0.0f, -5.0f };   /* nose (top)     */
    state->ship_model[1] = (Vec2){ -3.0f,  3.5f };   /* left tail fin  */
    state->ship_model[2] = (Vec2){  3.0f,  3.5f };   /* right tail fin */

    /* ── Build asteroid model (jagged circle, local space, radius ~1) ──
       20 vertices at equally-spaced angles, each perturbed by a random
       noise factor in [0.8, 1.2].  The model is unit-sized (radius ≈ 1);
       draw_wireframe scales it by SpaceObject.size at render time.       */
    for (int i = 0; i < ASTEROID_VERTS; i++) {
        float noise = 0.8f + ((float)(rand() % 100) / 100.0f) * 0.4f;
        float t     = ((float)i / (float)ASTEROID_VERTS) * 2.0f * PI;
        state->asteroid_model[i].x = sinf(t) * noise;
        state->asteroid_model[i].y = cosf(t) * noise;
    }

    /* ── Reset game state and spawn starting asteroids ──────────────── */
    reset_game(state);
}

/* ══════ asteroids_update ════════════════════════════════════════════════════
 *
 * Advance the game simulation by dt seconds.
 * Called once per frame, BEFORE asteroids_render().
 *
 * Structure:
 *   1. Input → ship movement (rotation, thrust, fire)
 *   2. Physics: integrate velocity, wrap positions
 *   3. Bullet → asteroid collision
 *   4. Asteroid → ship collision
 *   5. Compact pools (remove dead objects)
 *   6. Win/lose condition checks
 *
 * dt (delta time) is the time elapsed since the last frame in seconds.
 * Multiplying speeds and accelerations by dt makes movement frame-rate
 * independent.  JS game loops do the same thing.
 */
void asteroids_update(GameState *state, const GameInput *input, float dt) {

    /* ── Dead Phase: wait for FIRE to restart ──────────────────────── */
    if (state->phase == PHASE_DEAD) {
        state->dead_timer -= dt;
        if (state->dead_timer <= 0.0f && input->fire.half_transition_count > 0
            && input->fire.ended_down)
        {
            /* Save best score before wipe */
            if (state->score > state->best_score)
                state->best_score = state->score;
            asteroids_init(state);   /* full reset, keeps best_score */
            /* Play restart jingle AFTER init (init memsets audio state) */
            game_play_sound(&state->audio, SOUND_MUSIC_RESTART);
            return;
        }
        return;  /* no other logic while dead */
    }

    /* ══════ Gameplay music loop ═════════════════════════════════════════
       Re-trigger the background drone whenever it expires.  Don't start
       it while the restart jingle is still playing (they would overlap). */
    if (!game_is_sound_playing(&state->audio, SOUND_MUSIC_GAMEPLAY) &&
        !game_is_sound_playing(&state->audio, SOUND_MUSIC_RESTART)) {
        game_play_sound(&state->audio, SOUND_MUSIC_GAMEPLAY);
    }

    /* ══════ Input: Rotate ═══════════════════════════════════════════════ */
    if (input->left.ended_down)
        state->player.angle -= SHIP_TURN_SPEED * dt;
    if (input->right.ended_down)
        state->player.angle += SHIP_TURN_SPEED * dt;

    /* Rotate SFX: sustain while any rotation key is held; fades on release */
    if (input->left.ended_down || input->right.ended_down)
        game_sustain_sound(&state->audio, SOUND_ROTATE);

    /* ══════ Input: Thrust ═══════════════════════════════════════════════
       Acceleration is along the nose direction (angle=0 → nose points up).
       Rotation matrix maps nose (0,-5) → world offset (+sin(a)*5, -cos(a)*5).
       So the thrust unit vector is (+sin(a), -cos(a)).                    */
    if (input->up.ended_down) {
        float ca = cosf(state->player.angle);
        float sa = sinf(state->player.angle);
        /* Nose direction: (+sin(a), -cos(a)) — right component is +sa,
           up/down component is -ca.  At angle=0: (0,-1) → moves up ✓
           At angle=π/2: (1,0) → moves right ✓                            */
        state->player.dx +=  sa * SHIP_THRUST_ACCEL * dt;
        state->player.dy += -ca * SHIP_THRUST_ACCEL * dt;

        /* Clamp speed to maximum */
        float speed = sqrtf(state->player.dx * state->player.dx +
                            state->player.dy * state->player.dy);
        if (speed > SHIP_MAX_SPEED) {
            float inv = SHIP_MAX_SPEED / speed;
            state->player.dx *= inv;
            state->player.dy *= inv;
        }

        /* Thrust SFX: sustain while UP held; fades on release (engine stop) */
        game_sustain_sound(&state->audio, SOUND_THRUST);
    }

    /* Apply friction (space-y deceleration even without thrust) */
    state->player.dx *= SHIP_FRICTION;
    state->player.dy *= SHIP_FRICTION;

    /* ══════ Input: Fire ═════════════════════════════════════════════════ */
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

    /* ══════ Physics: Integrate & Wrap ══════════════════════════════════= */

    /* Ship */
    state->player.x += state->player.dx * dt;
    state->player.y += state->player.dy * dt;
    /* Toroidal wrap: JS equivalent: x = ((x % W) + W) % W */
    state->player.x = fmodf(state->player.x + (float)SCREEN_W, (float)SCREEN_W);
    state->player.y = fmodf(state->player.y + (float)SCREEN_H, (float)SCREEN_H);

    /* Asteroids */
    for (int i = 0; i < state->asteroid_count; i++) {
        SpaceObject *a = &state->asteroids[i];
        a->x += a->dx * dt;
        a->y += a->dy * dt;
        a->angle += 0.5f * dt;  /* slow constant spin for visual interest */
        a->x = fmodf(a->x + (float)SCREEN_W, (float)SCREEN_W);
        a->y = fmodf(a->y + (float)SCREEN_H, (float)SCREEN_H);
    }

    /* Bullets */
    for (int i = 0; i < state->bullet_count; i++) {
        SpaceObject *b = &state->bullets[i];
        b->x += b->dx * dt;
        b->y += b->dy * dt;
        b->size -= dt;  /* bullets shrink over time; when size ≤ 0 they die */
        if (b->size <= 0.0f) b->active = 0;
        b->x = fmodf(b->x + (float)SCREEN_W, (float)SCREEN_W);
        b->y = fmodf(b->y + (float)SCREEN_H, (float)SCREEN_H);
    }

    /* ══════ Collision: Bullets vs Asteroids ═════════════════════════════
       For each active bullet, test against every active asteroid.
       Collision: distance between centres < sum of radii.
       On hit: destroy bullet, split asteroid (or remove if small), add score.

       IMPORTANT: Both loops mark objects inactive; actual pool compaction
       happens AFTER both loops to avoid invalidating indices mid-loop.    */
    for (int bi = 0; bi < state->bullet_count; bi++) {
        if (!state->bullets[bi].active) continue;

        for (int ai = 0; ai < state->asteroid_count; ai++) {
            if (!state->asteroids[ai].active) continue;

            SpaceObject *b = &state->bullets[bi];
            SpaceObject *a = &state->asteroids[ai];

            float dx   = b->x - a->x;
            float dy   = b->y - a->y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist < a->size + 1.0f) {   /* +1 for bullet "radius" */
                b->active = 0;   /* destroy bullet */
                a->active = 0;   /* destroy asteroid */

                /* Spatial pan: asteroid at left edge → pan = -1 (full left) */
                float pan = (a->x / (float)SCREEN_W) * 2.0f - 1.0f;

                /* Split asteroid or remove, award points */
                if (a->size >= ASTEROID_LARGE_SIZE) {
                    state->score += 20;
                    game_play_sound_panned(&state->audio, SOUND_EXPLODE_LARGE, pan);
                    /* Spawn 2 medium asteroids at a perpendicular angle */
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
                    /* Spawn 2 small asteroids */
                    add_asteroid(state, a->x, a->y,
                                 a->dy * 0.8f + a->dx * 0.3f,
                                -a->dx * 0.8f + a->dy * 0.3f,
                                 ASTEROID_SMALL_SIZE);
                    add_asteroid(state, a->x, a->y,
                                -a->dy * 0.8f + a->dx * 0.3f,
                                 a->dx * 0.8f + a->dy * 0.3f,
                                 ASTEROID_SMALL_SIZE);
                } else {
                    /* Small asteroid — destroyed completely */
                    state->score += 100;
                    game_play_sound_panned(&state->audio, SOUND_EXPLODE_SMALL, pan);
                }

                /* One bullet can only destroy one asteroid */
                break;
            }
        }
    }

    /* ══════ Collision: Asteroids vs Ship ════════════════════════════════ */
    for (int ai = 0; ai < state->asteroid_count; ai++) {
        if (!state->asteroids[ai].active) continue;

        SpaceObject *a = &state->asteroids[ai];
        float dx   = state->player.x - a->x;
        float dy   = state->player.y - a->y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist < a->size + state->player.size) {
            /* Ship is hit */
            state->player.active = 0;
            game_play_sound(&state->audio, SOUND_SHIP_DEATH);

            /* Update best score */
            if (state->score > state->best_score)
                state->best_score = state->score;

            state->phase      = PHASE_DEAD;
            state->dead_timer = DEATH_DELAY;
            break;
        }
    }

    /* ══════ Compact pools ════════════════════════════════════════════════
       Now safe to compact: all detection loops are done.
       The compact_pool function uses swap-with-tail, which is O(n).      */
    compact_pool(state->bullets,   &state->bullet_count);
    compact_pool(state->asteroids, &state->asteroid_count);

    /* ══════ Win condition: all asteroids cleared ════════════════════════ */
    if (state->asteroid_count == 0 && state->phase == PHASE_PLAYING) {
        /* Advance to next wave: re-run init (new asteroid model + more spawn) */
        /* COURSE NOTE: A full implementation would have a proper wave counter.
           For simplicity we just restart; students can add wave progression.  */
        int bs = state->best_score;
        int sc = state->score;
        asteroids_init(state);
        /* Carry score and best-score into the new wave */
        state->score = sc;
        if (sc > bs) bs = sc;
        state->best_score = bs;
    }
}

/* ══════ asteroids_render ════════════════════════════════════════════════════
 *
 * Draw everything for the current frame into bb.
 * Called AFTER asteroids_update(); never reads or modifies GameState (except
 * the `const` qualifier is relaxed due to draw_wireframe needing the models).
 *
 * Draw order:
 *   1. Clear screen (black)
 *   2. Asteroids
 *   3. Bullets
 *   4. Ship
 *   5. HUD (score, best)
 *   6. Game over overlay (if dead)
 */
void asteroids_render(const GameState *state, AsteroidsBackbuffer *bb) {
    /* ── 1. Clear screen ──────────────────────────────────────────────── */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

    /* ── 2. Asteroids ─────────────────────────────────────────────────── */
    for (int i = 0; i < state->asteroid_count; i++) {
        const SpaceObject *a = &state->asteroids[i];
        if (!a->active) continue;
        /* The asteroid model has unit radius; multiply by a->size to scale */
        draw_wireframe(bb,
                       state->asteroid_model, ASTEROID_VERTS,
                       a->x, a->y,
                       a->angle, a->size,
                       COLOR_WHITE);
    }

    /* ── 3. Bullets ──────────────────────────────────────────────────── */
    for (int i = 0; i < state->bullet_count; i++) {
        const SpaceObject *b = &state->bullets[i];
        if (!b->active) continue;
        /* Bullets are just single toroidal pixels; they're too small for
           a full wireframe.  The size field controls their brightness as
           they fade — we skip drawing if effectively invisible.          */
        draw_pixel_w(bb, (int)b->x, (int)b->y, COLOR_YELLOW);
        draw_pixel_w(bb, (int)b->x+1, (int)b->y, COLOR_YELLOW);
        draw_pixel_w(bb, (int)b->x, (int)b->y+1, COLOR_YELLOW);
    }

    /* ── 4. Ship ─────────────────────────────────────────────────────── */
    if (state->player.active) {
        draw_wireframe(bb,
                       state->ship_model, SHIP_VERTS,
                       state->player.x, state->player.y,
                       state->player.angle, 3.5f,
                       COLOR_CYAN);
    }

    /* ── 5. HUD ──────────────────────────────────────────────────────── */
    {
        char buf[32];

        /* Score (top-left) */
        snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
        draw_text(bb, 8, 8, buf, COLOR_WHITE);

        /* Best (top-right) */
        snprintf(buf, sizeof(buf), "BEST: %d", state->best_score);
        int best_x = bb->width - (int)(strlen(buf) * GLYPH_W) - 8;
        draw_text(bb, best_x, 8, buf, COLOR_GREY);
    }

    /* ── 6. Game-Over Overlay ─────────────────────────────────────────── */
    if (state->phase == PHASE_DEAD) {
        /* Dim overlay across full screen */
        draw_rect_blend(bb, 0, 0, bb->width, bb->height, COLOR_OVERLAY_DIM);

        int cx = bb->width  / 2;
        int cy = bb->height / 2;

        /* "GAME OVER" centred */
        const char *msg1 = "GAME OVER";
        int w1 = (int)(strlen(msg1) * GLYPH_W);
        draw_text(bb, cx - w1/2, cy - 20, msg1, COLOR_RED);

        /* Score line */
        char score_buf[32];
        snprintf(score_buf, sizeof(score_buf), "SCORE: %d", state->score);
        int ws = (int)(strlen(score_buf) * GLYPH_W);
        draw_text(bb, cx - ws/2, cy - 4, score_buf, COLOR_WHITE);

        /* Restart prompt (shown after death timer expires) */
        if (state->dead_timer <= 0.0f) {
            const char *msg2 = "PRESS FIRE TO RESTART";
            int w2 = (int)(strlen(msg2) * GLYPH_W);
            draw_text(bb, cx - w2/2, cy + 16, msg2, COLOR_YELLOW);
        }
    }
}
