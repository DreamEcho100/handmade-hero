/* =============================================================================
 * game.h — Asteroids Game Types & Public API
 * =============================================================================
 *
 * This header is the single source of truth for:
 *   • All game-state types (SpaceObject, GameState, GamePhase, …)
 *   • Input abstraction (GameInput, GameButtonState)
 *   • The public API that platform code calls into
 *
 * Include order requirement (must include before game.h):
 *   None — game.h includes its own dependencies.
 *   Platform code only needs:  #include "platform.h"
 *   Game code only needs:      #include "game.h"
 * =============================================================================
 */

#ifndef ASTEROIDS_GAME_H
#define ASTEROIDS_GAME_H

#include <stdint.h>          /* uint32_t                        */
#include "utils/backbuffer.h" /* AsteroidsBackbuffer, GAME_RGBA  */
#include "utils/math.h"      /* Vec2, MIN, MAX, CLAMP            */
#include "utils/audio.h"     /* GameAudioState, AudioOutputBuffer */

/* ══════ Debug Utilities ════════════════════════════════════════════════════

   DEBUG_TRAP — triggers a CPU breakpoint so a debugger can catch it.
   ASSERT     — checks a condition; fires DEBUG_TRAP if it's false.

   Both are compiled out in release builds (no -DDEBUG flag).
   Use: ASSERT(ptr != NULL, "null pointer in update");

   C99 has no built-in assert-with-message; __assert_fail is glibc-specific.
   We use fprintf/abort for portability, or __builtin_trap for GCC/Clang.  */
#ifdef DEBUG
  #include <stdio.h>   /* fprintf, stderr                   */
  #include <stdlib.h>  /* abort                             */
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

   Fixed-size pools avoid dynamic allocation.  Maximum counts are conservatively
   large; a typical game uses far fewer.

   Why fixed pools instead of malloc?
     • Deterministic memory layout (no fragmentation, no allocation failures)
     • Simple to debug (all state is in one GameState struct)
     • In C, forgetting to free heap memory is a common source of bugs —
       stack/struct pools sidestep that class of error entirely.
     • JS equivalent:  "preallocated object pool" pattern for game entities.  */
#define MAX_ASTEROIDS    32
#define MAX_BULLETS      8

/* Model vertex counts:
     SHIP_VERTS      — triangle (3 points make the ship shape)
     ASTEROID_VERTS  — jagged polygon with 20 vertices for organic look     */
#define SHIP_VERTS      3
#define ASTEROID_VERTS  20

/* Gameplay constants */
#define ASTEROID_LARGE_SIZE   20.0f   /* radius for large asteroids  */
#define ASTEROID_MEDIUM_SIZE  10.0f   /* radius for medium asteroids */
#define ASTEROID_SMALL_SIZE    5.0f   /* radius for small asteroids  */

#define BULLET_SPEED        200.0f   /* pixels per second */
#define BULLET_LIFETIME       1.5f   /* seconds before a bullet disappears */
#define SHIP_TURN_SPEED       3.5f   /* radians per second */
#define SHIP_THRUST_ACCEL   100.0f   /* pixels/sec² while UP is held */
#define SHIP_MAX_SPEED      220.0f   /* pixels/sec velocity cap */
#define SHIP_FRICTION         0.97f  /* multiplied against velocity each frame */

#define DEATH_DELAY           2.5f   /* seconds before restart is allowed */
#define FIRE_COOLDOWN         0.15f  /* seconds between consecutive shots */

/* ══════ GamePhase ══════════════════════════════════════════════════════════

   JS equivalent:  enum GamePhase { PLAYING, DEAD };
   We use PLAYING (normal gameplay) and DEAD (ship destroyed, waiting to restart) */
typedef enum {
    PHASE_PLAYING = 0,
    PHASE_DEAD
} GAME_PHASE;

/* ══════ SpaceObject ════════════════════════════════════════════════════════

   Represents any entity in the game: ship, asteroid, or bullet.
   Using a single struct for all object types keeps the pools uniform
   and avoids type-tagging logic in the collision system.

   JS equivalent:
     interface SpaceObject {
       x: number; y: number;      // position
       dx: number; dy: number;    // velocity (pixels/sec)
       angle: number;             // heading in radians
       size: number;              // collision radius
       active: boolean;           // in use?
     }                                                                     */
typedef struct {
    float x, y;      /* world-space position (pixels from top-left)         */
    float dx, dy;    /* velocity (pixels per second, updated each frame)     */
    float angle;     /* heading in radians (0 = up, increases clockwise)     */
    float size;      /* collision radius (also controls draw scale)          */
    int   active;    /* 1 = in-use, 0 = slot is free                         */
} SpaceObject;

/* ══════ GameButtonState ════════════════════════════════════════════════════

   Tracks both "is the key held right now" (ended_down) and "how many
   transitions happened this frame" (half_transition_count).

   ended_down alone answers: "is this key currently held?"
   half_transition_count > 0 and ended_down answers: "was it just pressed?"
   half_transition_count > 0 and !ended_down answers: "was it just released?"

   JS equivalent:
     interface ButtonState {
       endedDown: boolean;
       halfTransitionCount: number;
     }                                                                     */
typedef struct {
    int ended_down;           /* 1 = held at end of frame, 0 = released     */
    int half_transition_count;/* number of press/release transitions        */
} GameButtonState;

/* ══════ GameInput ══════════════════════════════════════════════════════════

   All button states for one frame.  A C99 anonymous union lets callers
   access buttons either by name or by index:
     input.left.ended_down         (named access)
     input.buttons[0].ended_down   (indexed access, loops over all buttons)

   BUTTON_COUNT must match the number of named fields in the struct below.

   JS equivalent:
     interface GameInput {
       buttons: ButtonState[];
       left: ButtonState; right: ButtonState;
       up: ButtonState; fire: ButtonState;
       quit: boolean;
     }                                                                     */
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
    int quit;  /* set to 1 by platform when the user closes the window     */
} GameInput;

/* ══════ GameState — the entire game in one struct ══════════════════════════

   All mutable game data lives here.  No globals (except in platform code
   where necessary for OS callbacks).  Keeping everything in one struct
   makes save/restore trivial and simplifies reasoning about state.

   MEMORY NOTE: GameState is allocated on the stack in main().  At roughly
   3–4 KB for this game it is well within typical stack limits (~1 MB).    */
typedef struct {
    SpaceObject player;                    /* the ship                      */
    SpaceObject asteroids[MAX_ASTEROIDS];  /* asteroid pool                 */
    int         asteroid_count;            /* active entries in asteroids[] */
    SpaceObject bullets[MAX_BULLETS];      /* bullet pool                   */
    int         bullet_count;              /* active entries in bullets[]   */

    int        score;                      /* current game score            */
    int        best_score;                 /* all-time best (survives init) */

    GAME_PHASE phase;                      /* PLAYING or DEAD               */
    float      dead_timer;                 /* countdown until restart prompt*/
    float      fire_timer;                 /* cooldown between bullets      */

    /* Ship and asteroid wireframe models (local-space vertices).
       Built once in asteroids_init(); asteroid_model is randomised.       */
    Vec2 ship_model[SHIP_VERTS];
    Vec2 asteroid_model[ASTEROID_VERTS];

    GameAudioState audio;                  /* all SFX state                 */
} GameState;

/* ══════ prepare_input_frame ════════════════════════════════════════════════

   Called each frame BEFORE platform reads new hardware events.
   Copies ended_down from the previous frame into the current frame's
   initial state, and resets half_transition_count.
   This way, if no events arrive, current state = last state (key still held).

   Parameters:
     old     — input state from the PREVIOUS frame
     current — input state for the CURRENT frame (will be filled by platform)

   COURSE NOTE: The reference asteroids.c uses a single GameInput with no
   double-buffering.  The course upgrades to two-buffer input (matching the
   Snake course pattern) to correctly detect "just pressed" vs "held".    */
void prepare_input_frame(const GameInput *old, GameInput *current);

/* ══════ Audio helper (defined in audio.c) ══════════════════════════════════

   game_play_sound        — queue a sound at centre pan (one-shot)
   game_play_sound_panned — queue a sound with spatial pan (for explosions)
   game_sustain_sound     — sustain a looping sound while a key is held.
                            Call once per frame while the key is down:
                              start it if not playing (attack ramp); if
                              playing, keep samples_remaining above decay
                              threshold (amplitude stays at 1.0).
                            When the key is released, stop calling it:
                              remaining drains to 0; TRAPEZOID envelope
                              fades over last 10% of duration.
   game_is_sound_playing  — returns 1 if any instance of id is active       */
void game_play_sound(GameAudioState *audio, SOUND_ID id);
void game_play_sound_panned(GameAudioState *audio, SOUND_ID id, float pan);
void game_sustain_sound(GameAudioState *audio, SOUND_ID id);
int  game_is_sound_playing(const GameAudioState *audio, SOUND_ID id);
void game_audio_init(GameState *state);

/* ══════ Game Entry Points (implemented in game.c) ═════════════════════════

   (Full docs live in platform.h; listed here so game-internal translation
   units can also call asteroids_init without including platform.h.)     */
void asteroids_init(GameState *state);
void asteroids_update(GameState *state, const GameInput *input, float dt);
void asteroids_render(const GameState *state, AsteroidsBackbuffer *bb);
void game_get_audio_samples(GameState *state, AudioOutputBuffer *out);

#endif /* ASTEROIDS_GAME_H */
