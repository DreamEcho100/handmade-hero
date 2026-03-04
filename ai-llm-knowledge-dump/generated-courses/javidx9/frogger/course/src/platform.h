/* =============================================================================
 * platform.h — Platform API Contract (4 functions)
 * =============================================================================
 *
 * Every backend (X11, Raylib, SDL3, …) MUST implement exactly these four
 * functions.  Game logic in game.c / audio.c never calls any OS or library
 * API directly — only these functions.
 *
 * ADDING A NEW BACKEND
 * ────────────────────
 * 1. Create a new main_*.c file.
 * 2. Implement all four functions below.
 * 3. Add a case to build-dev.sh's backend selector.
 * That's it.  No changes to game.c.
 *
 * JS analogy: think of this like a TypeScript interface:
 *   interface Platform {
 *     init(props: PlatformGameProps): void;
 *     getTime(): number;
 *     getInput(input: GameInput, props: PlatformGameProps): void;
 *     shutdown(): void;
 *   }
 *
 * > Course Note: The reference frogger source uses only three functions
 * > (platform_init, platform_get_time, platform_get_input) with a raw
 * > title+width+height signature.  The course adds:
 * >   (a) platform_shutdown — for clean resource teardown.
 * >   (b) PlatformGameProps — a wrapper struct so all window/backbuffer
 * >       state travels together.
 * > This matches the pattern used in Tetris/Snake/Asteroids.
 * =============================================================================
 */

#ifndef FROGGER_PLATFORM_H
#define FROGGER_PLATFORM_H

#include "game.h"  /* Backbuffer, GameInput, GameAudioState */

/* ══════ PlatformGameProps ══════════════════════════════════════════════════

   Passed to platform_init; fields are set during init and read every frame.
   Centralises all "what does the window look like right now" state that would
   otherwise be scattered across static globals in each backend.

   is_running:  1 = game loop should continue; 0 = quit.
                Platforms set this to 0 when the window is closed.
   audio:       pointer to the game's audio state — platforms use it to
                call game_get_audio_samples() on each audio callback.     */
typedef struct {
    const char *title;
    int         width;
    int         height;
    Backbuffer  backbuffer;
    int         is_running;
} PlatformGameProps;

/* ══════ Platform Contract ══════════════════════════════════════════════════

   Implementations live in main_x11.c and main_raylib.c.
   These functions are the ONLY platform API called by game code.          */

/* Open the window and initialise all platform resources (GL context, audio
   device, etc.).  Sets props->backbuffer.pixels to the allocated pixel array.
   Returns 1 on success, 0 on failure.                                    */
int platform_init(PlatformGameProps *props);

/* Return the current time in seconds (monotonic clock).
   Used to compute delta_time each frame.                                  */
double platform_get_time(void);

/* Fill *input with key events accumulated since the last call.
   Uses GameButtonState so sub-frame taps are never lost.
   Also updates props->is_running to 0 when quit/close occurs.            */
void platform_get_input(GameInput *input, PlatformGameProps *props);

/* Release all platform resources: GL context, X11 display, audio device.
   Called once at program exit.                                            */
void platform_shutdown(void);

/* ══════ platform_swap_input_buffers ═══════════════════════════════════════

   Exchanges two GameInput buffers in-place.  After the swap:
     inputs[0] (old)     = last frame's final state
     inputs[1] (current) = copy ready to be overwritten by new events

   JS equivalent: [a, b] = [b, a];

   Each backend calls this at the start of every frame:
     1. platform_swap_input_buffers(&inputs[0], &inputs[1])
     2. prepare_input_frame(&inputs[0], &inputs[1])
     3. [platform reads hardware events into &inputs[1]]
     4. game_update(&state, &inputs[1], dt)                               */
static inline void platform_swap_input_buffers(GameInput *a, GameInput *b) {
    GameInput temp = *a;
    *a = *b;
    *b = temp;
}

#endif /* FROGGER_PLATFORM_H */
