/* =============================================================================
 * platform.h — The Platform/Game Contract for Asteroids
 * =============================================================================
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * Every game must answer two questions:
 *   1. What does the GAME need the platform to do?   (game → platform calls)
 *   2. What does the PLATFORM need the game to do?   (platform → game calls)
 *
 * This header defines that two-way contract using function declarations and a
 * single inline helper.  The actual implementations live in:
 *   - main_x11.c     (Linux: Xlib + GLX + ALSA)
 *   - main_raylib.c  (cross-platform: raylib)
 *
 * DESIGN PRINCIPLE — Dependency inversion:
 * The game code (game.c, audio.c) must never #include anything platform-
 * specific (X11.h, raylib.h).  Instead, it depends ONLY on the types defined
 * in utils/backbuffer.h and utils/audio.h.  The platform implementations
 * depend on the game — never the other way around.
 *
 * JS equivalent:
 *   // A platform.h is like a TypeScript interface:
 *   interface Platform {
 *     init(): void;
 *     display(fb: Backbuffer): void;
 *     getInput(input: GameInput): void;
 *     shutdown(): void;
 *   }
 * =============================================================================
 */

#ifndef ASTEROIDS_PLATFORM_H
#define ASTEROIDS_PLATFORM_H

/* game.h brings in GameInput, AsteroidsBackbuffer, GameState, etc. */
#include "game.h"

/* ══════ Platform → Game (game.c exports these, platform calls them) ════════

   Four function pointers the platform expects the game to provide.
   Link all source files together and these symbols will resolve.         */

/* asteroids_init — one-time (or per-restart) game initialisation.
   Called:
     • Once from main() before the game loop starts.
     • Again from asteroids_update() when the player requests a restart.
   The function wipes GameState with memset and rebuilds the ship and
   asteroid models, so every new game has a freshly-randomised asteroid.
   COURSE NOTE: audio config and best_score survive the memset
   via a save/restore pattern — see the implementation in game.c.        */
void asteroids_init(GameState *state);

/* asteroids_update — advance the game simulation by one frame.
   Called once per frame BEFORE rendering.
   Parameters:
     state  — mutable game state (positions, velocities, score, phase, …)
     input  — current frame's button states (already prepared by
               prepare_input_frame)
     dt     — time elapsed since the last frame, in seconds (typically
               ~0.0167 for 60 fps)                                        */
void asteroids_update(GameState *state, const GameInput *input, float dt);

/* asteroids_render — draw the current frame into the pixel backbuffer.
   Called once per frame AFTER update.
   The platform will then copy backbuffer.pixels to the GPU / screen.    */
void asteroids_render(const GameState *state, AsteroidsBackbuffer *bb);

/* game_get_audio_samples — fill one audio period with PCM sample data.
   Called once per frame (or whenever the audio driver needs more data).
   The platform passes a pre-allocated AudioOutputBuffer; this function
   mixes all active sounds and writes 16-bit stereo PCM into it.         */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *out);


/* ══════ Game → Platform (platform.c / main.c exports these) ═══════════════

   platform_init and platform_shutdown are symmetric:
     platform_init()     — open window, create GL context, init audio
     platform_shutdown() — close audio, destroy GL context, close window

   COURSE NOTE: The reference source had no platform_shutdown.  We add it
   for correct resource cleanup (ALSA drain, GL context deletion, XDestroyWindow).
   Without shutdown, closing the window left the terminal hung in the
   original code — a real usability bug.                                  */
void platform_init(void);
void platform_shutdown(void);

/* ══════ Input Double-Buffer ════════════════════════════════════════════════

   WHY TWO INPUT BUFFERS?
   ──────────────────────
   The GameInput struct stores not just "is this key held?" (ended_down) but
   also "was it just pressed this frame?" (half_transition_count).  To detect
   "just pressed", we need to compare the current frame's state to the
   previous frame's state.  Keeping two buffers lets us do this without
   allocating new memory every frame.

   Call order each frame:
     1. platform_swap_input_buffers(&inputs[0], &inputs[1])
          Current becomes old; old's memory is now free to overwrite.
     2. prepare_input_frame(&inputs[0], &inputs[1])
          Carries ended_down from old into current as the "start" state.
     3. [platform reads hardware events into &inputs[1] (current)]
     4. asteroids_update(&state, &inputs[1], dt)
          Game logic reads from current.

   JS equivalent:
     // Frame N-1 state is like a closure captured from the previous tick.
     let prevKeys = { ...currentKeys };
     // Then diffing currentKeys vs prevKeys to find "just pressed".      */

/* prepare_input_frame — initialise current frame's input from last frame.
   Declared in game.h (game.c provides the implementation) so that the
   game logic and the platform code share the same preparation logic.     */

/* platform_swap_input_buffers — exchange two GameInput buffers in-place.
   After the swap inputs[0] (old) contains last frame's final state, and
   inputs[1] (current) contains a copy ready to be overwritten by new events.

   COURSE NOTE: Implemented as a static inline so the compiler eliminates
   the call overhead entirely — it becomes three assignments.             */
static inline void platform_swap_input_buffers(GameInput *a, GameInput *b) {
    /* JS equivalent:
         [a, b] = [b, a];   // array destructure swap
       In C we need a temporary because assignment is not atomic.         */
    GameInput temp = *a;
    *a = *b;
    *b = temp;
}

#endif /* ASTEROIDS_PLATFORM_H */
