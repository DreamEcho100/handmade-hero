/* =============================================================================
 * game/game.h — Asteroids Game Types & Public API
 * =============================================================================
 *
 * LESSON 08 — Game State Structure
 *
 * This header is the single source of truth for:
 *   • All game-state types (SpaceObject, GameState, GamePhase, …)
 *   • Input abstraction (GameInput, GameButtonState)
 *   • The public API that platform code calls into
 *
 * Include order:
 *   Platform code:  #include "platform.h"  (which includes this)
 *   Game code:      #include "game.h"
 * =============================================================================
 */

#ifndef ASTEROIDS_GAME_H
#define ASTEROIDS_GAME_H

#include <stdint.h>
#include "../utils/backbuffer.h"  /* Backbuffer, GAME_RGBA               */
#include "../utils/base.h"        /* GAME_UNITS_W/H, GameWorldConfig, … */
#include "../utils/math.h"        /* Vec2, MIN, MAX, CLAMP               */
#include "../utils/audio.h"       /* GameAudioState, AudioOutputBuffer   */

/* ══════ Debug Utilities ════════════════════════════════════════════════════
 *
 * DEBUG_TRAP — triggers a CPU breakpoint so a debugger can catch it.
 * ASSERT     — checks a condition; fires DEBUG_TRAP if it's false.
 * Both are compiled out in release builds (no -DDEBUG flag).              */
#ifdef DEBUG
  #include <stdio.h>
  #include <stdlib.h>
  #define DEBUG_TRAP()  __builtin_trap()
  #define ASSERT(cond, msg) \
      do { \
          if (!(cond)) { \
              fprintf(stderr, "ASSERT FAILED: %s\n  at %s:%d\n", \
                      (msg), __FILE__, __LINE__); \
              DEBUG_TRAP(); \
          } \
      } while(0)
#else
  #define DEBUG_TRAP()          ((void)0)
  #define ASSERT(cond, msg)     ((void)0)
#endif

/* ══════ Pool Limits ════════════════════════════════════════════════════════
 *
 * Fixed-size pools avoid dynamic allocation.  These values are conservatively
 * large; a typical game uses far fewer.
 *
 * Why fixed pools?
 *   • Deterministic layout (no fragmentation, no allocation failures)
 *   • All state in one GameState struct → trivial to debug
 *   • Sidesteps the entire class of malloc/free bugs
 *   • JS equivalent: "preallocated object pool" pattern                   */
#define MAX_ASTEROIDS    32
#define MAX_BULLETS       8

/* Model vertex counts */
#define SHIP_VERTS       3    /* triangle */
#define ASTEROID_VERTS  20    /* jagged polygon for organic look */

/* ══════ Gameplay Constants (in GAME UNITS) ════════════════════════════════
 *
 * LESSON 04 / LESSON 08 — All distances are in game units (not pixels).
 * Reference: GAME_W=512px ÷ GAME_UNITS_W=16 → 1 unit = 32 px at 512px.
 *
 * Conversion from old pixel values: divide by 32.
 * Example: BULLET_SPEED was 200 px/s → 200/32 = 6.25 units/s.
 *
 * WHY NOT PIXELS?
 *   If constants were in pixels, a 1024px-wide window would show a slow
 *   ship (200 px/s is fast at 512px but crawls at 1024px).  Game units
 *   decouple game feel from resolution — the same 6.25 units/s feels
 *   identical at any resolution because make_render_context scales it.   */

/* Asteroid collision radii (game units) */
#define ASTEROID_LARGE_SIZE   0.625f   /* was 20 px  → 20/32 = 0.625  */
#define ASTEROID_MEDIUM_SIZE  0.3125f  /* was 10 px  → 10/32 = 0.3125 */
#define ASTEROID_SMALL_SIZE   0.15625f /* was  5 px  →  5/32 = 0.15625 */

/* Physics constants (game units per second, or per second²) */
#define BULLET_SPEED          6.25f    /* was 200 px/s  → 200/32       */
#define SHIP_TURN_SPEED       3.5f     /* radians/s (angle, no change) */
#define SHIP_THRUST_ACCEL     3.125f   /* was 100 px/s² → 100/32       */
#define SHIP_MAX_SPEED        6.875f   /* was 220 px/s  → 220/32       */
#define SHIP_FRICTION         0.97f    /* multiplier per frame (no change) */

/* Ship wireframe render scale (game units per model-local unit).
   Old pixel scale was 3.5f.  In game units: 3.5/32 = 0.109375.
   Ship model nose at (0,+5) with scale 0.109375 → 5*0.109375 = 0.547 units
   = 17.5 px at reference resolution.                                      */
#define SHIP_RENDER_SCALE     0.109375f /* was 3.5 px-scale → 3.5/32  */

/* Ship collision radius (game units) */
#define SHIP_COLLISION_RADIUS 0.15625f  /* was 5 px → 5/32             */

/* Timing constants (seconds, no change) */
#define BULLET_LIFETIME       1.5f     /* initial bullet size (seconds) */
#define DEATH_DELAY           2.5f     /* wait before restart is allowed */
#define FIRE_COOLDOWN         0.15f    /* min time between shots        */

/* ══════ GamePhase ══════════════════════════════════════════════════════════
 *
 * JS equivalent:  type GamePhase = "PLAYING" | "DEAD";                    */
typedef enum {
    PHASE_PLAYING = 0,
    PHASE_DEAD
} GAME_PHASE;

/* ══════ SpaceObject ════════════════════════════════════════════════════════
 *
 * Represents any entity: ship, asteroid, or bullet.  Using one struct type
 * for all entities keeps pools uniform and avoids type-tagging complexity.
 *
 * COORDINATE CONVENTION (LESSON 04):
 *   x, y are in GAME UNITS, origin COORD_ORIGIN_LEFT_BOTTOM (y-up).
 *   (0,0) = bottom-left, (GAME_UNITS_W, GAME_UNITS_H) = top-right.
 *   dx, dy are game-units per second (not pixels per second).
 *
 * JS equivalent:
 *   interface SpaceObject {
 *     x: number; y: number;       // position in game units
 *     dx: number; dy: number;     // velocity in units/sec
 *     angle: number;              // heading in radians (0=up, CW positive)
 *     size: number;               // collision radius in game units
 *     active: boolean;
 *   }                                                                     */
typedef struct {
    float x, y;      /* world-space position (game units, y-up)            */
    float dx, dy;    /* velocity (game units/second)                        */
    float angle;     /* heading in radians (0 = up, + = clockwise)          */
    float size;      /* collision radius in game units (also draw scale)    */
    int   active;    /* 1 = in-use, 0 = free slot                           */
} SpaceObject;

/* ══════ GameButtonState ════════════════════════════════════════════════════
 *
 * LESSON 06 — Double-buffered input.
 *
 * ended_down alone: "is this key currently held?"
 * half_transition_count > 0 && ended_down:  "just pressed this frame?"
 * half_transition_count > 0 && !ended_down: "just released this frame?"
 *
 * JS equivalent:
 *   interface ButtonState { endedDown: boolean; halfTransitionCount: number; }
 */
typedef struct {
    int ended_down;            /* 1 = held at end of frame, 0 = released   */
    int half_transition_count; /* press/release transitions this frame      */
} GameButtonState;

/* ══════ GameInput ══════════════════════════════════════════════════════════
 *
 * LESSON 06 — All button states for one frame.
 * A C99 anonymous union allows both named and indexed access:
 *   input.left.ended_down       (named)
 *   input.buttons[0].ended_down (indexed, for loops)
 *
 * BUTTON_COUNT must match the number of named fields below.              */
#define BUTTON_COUNT 4

typedef struct {
    union {
        GameButtonState buttons[BUTTON_COUNT];
        struct {
            GameButtonState left;   /* buttons[0]: rotate left              */
            GameButtonState right;  /* buttons[1]: rotate right             */
            GameButtonState up;     /* buttons[2]: thrust forward           */
            GameButtonState fire;   /* buttons[3]: fire bullet              */
        };
    };
    int quit;  /* platform sets to 1 when the user closes the window       */
} GameInput;

/* ══════ GameState — the entire game in one struct ══════════════════════════
 *
 * LESSON 08 — All mutable game data lives here.  No globals (except in
 * platform code where necessary for OS callbacks).  One struct keeps
 * save/restore trivial and simplifies state reasoning.
 *
 * Memory: GameState is stack-allocated in main() — roughly 3–4 KB,
 * well within typical stack limits (~1 MB).                               */
typedef struct {
    SpaceObject player;                    /* the ship                      */
    SpaceObject asteroids[MAX_ASTEROIDS];  /* asteroid pool                 */
    int         asteroid_count;            /* live entries in asteroids[]   */
    SpaceObject bullets[MAX_BULLETS];      /* bullet pool                   */
    int         bullet_count;             /* live entries in bullets[]      */

    int        score;                      /* current game score            */
    int        best_score;                 /* all-time best (survives init) */

    GAME_PHASE phase;                      /* PLAYING or DEAD               */
    float      dead_timer;                 /* countdown until restart prompt*/
    float      fire_timer;                 /* cooldown between bullets      */

    /* Ship and asteroid wireframe models (local-space vertices).
       Built once in asteroids_init(); asteroid_model is randomised.
       Model vertices are in local space; draw_wireframe applies scale.    */
    Vec2 ship_model[SHIP_VERTS];
    Vec2 asteroid_model[ASTEROID_VERTS];

    GameAudioState audio;                  /* all SFX state                 */
} GameState;

/* ══════ prepare_input_frame ════════════════════════════════════════════════
 *
 * LESSON 06 — Called each frame BEFORE platform reads new hardware events.
 * Copies ended_down from the previous frame so keys that were held remain
 * held; resets half_transition_count to 0 for the new frame.            */
void prepare_input_frame(const GameInput *old, GameInput *current);

/* ══════ Audio helpers (defined in audio.c) ════════════════════════════════
 *
 * game_play_sound        — queue a sound at centre pan (one-shot)
 * game_play_sound_panned — queue a sound with spatial pan (for explosions)
 * game_sustain_sound     — sustain a looping sound while a key is held
 * game_is_sound_playing  — returns 1 if any instance of id is active
 * game_audio_init        — set default volume levels (call before init)   */
void game_play_sound(GameAudioState *audio, SOUND_ID id);
void game_play_sound_panned(GameAudioState *audio, SOUND_ID id, float pan);
void game_sustain_sound(GameAudioState *audio, SOUND_ID id);
int  game_is_sound_playing(const GameAudioState *audio, SOUND_ID id);
void game_audio_init(GameState *state);

/* ══════ Game Entry Points (implemented in game.c) ═════════════════════════
 *
 * Full documentation lives in platform.h; listed here so game-internal
 * translation units can call asteroids_init without including platform.h. */
void asteroids_init(GameState *state);
void asteroids_update(GameState *state, const GameInput *input, float dt);
void asteroids_render(const GameState *state, Backbuffer *bb,
                      GameWorldConfig world_config);
void game_get_audio_samples(GameState *state, AudioOutputBuffer *out);

#endif /* ASTEROIDS_GAME_H */
