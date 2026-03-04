/* ═══════════════════════════════════════════════════════════════════════════
 * platform.h  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * The CONTRACT between the game layer (game.c / audio.c) and the platform
 * layer (main_x11.c / main_raylib.c).
 *
 * "Contract" means:
 *   • game.c CALLS these functions — but does NOT define them.
 *   • main_x11.c / main_raylib.c DEFINE these functions — one per backend.
 *   • Only ONE backend is compiled into any given executable.
 *
 * The four functions declared here are the ONLY OS-touching calls that
 * game.c ever makes.  Everything else (X11, ALSA, Raylib) is hidden
 * inside the respective main_*.c file and never leaks into game.c.
 *
 * HANDMADE HERO PRINCIPLE: "The platform serves the game; the game
 * doesn't care which platform it's running on."
 *
 * COMPARISON WITH REFERENCE SOURCE:
 *   Reference platform.h: 3 functions (no shutdown)
 *   Course  platform.h:   4 functions (adds platform_shutdown)
 *   Rationale: explicit shutdown makes cleanup traceable in lessons.
 *   See COURSE-BUILDER-IMPROVEMENTS.md.
 *
 * COMPARISON WITH TETRIS COURSE:
 *   Tetris platform.h has PlatformGameProps and PlatformAudioConfig structs,
 *   plus 7+ functions, for a more feature-rich engine.
 *   Snake is simpler: no PlatformGameProps, no music config, 4 functions only.
 *
 * JS analogy: a TypeScript interface that a platform "adapter" class must
 * implement.  The game uses the interface; it never depends on the concrete class.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "game.h"  /* GameInput, GameState, SnakeBackbuffer, AudioOutputBuffer */

/* ══════ Platform Contract (4 functions) ════════════════════════════════════
 *
 * Every backend (X11, Raylib, future SDL2, …) MUST implement these 4 functions
 * with EXACTLY these signatures.
 */

/* platform_init — open the window and set up the audio device.
 *
 * Called ONCE at the start of main().
 * The platform must:
 *   - Create a window of `width` × `height` pixels with title `title`.
 *   - Initialise the audio device (ALSA / Raylib AudioDevice).
 *   - Allocate the backbuffer pixel array (width × height × 4 bytes).
 *
 * Game logic does NOT call platform_init(); only main() does.
 */
void platform_init(const char *title, int width, int height);

/* platform_get_time — return the current time in seconds.
 *
 * Used to compute delta_time each frame:
 *   double now   = platform_get_time();
 *   float  dt    = (float)(now - last_time);
 *   last_time    = now;
 *
 * Must be monotonic (always increases, never jumps backward).
 * Resolution must be at least milliseconds.
 *
 * X11 backend:   clock_gettime(CLOCK_MONOTONIC)
 * Raylib backend: GetTime()
 */
double platform_get_time(void);

/* platform_get_input — poll or flush OS events into `input`.
 *
 * MUST be called AFTER prepare_input_frame() each frame.
 * Writes to `input->buttons[i].ended_down` and increments
 * `input->buttons[i].half_transition_count` for each press/release event.
 * Sets `input->restart` and `input->quit` for one-shot actions.
 *
 * X11 backend:
 *   Drains the XPending() event queue, calling UPDATE_BUTTON() per event.
 *
 * Raylib backend:
 *   IsKeyPressed / IsKeyDown polls; calls UPDATE_BUTTON() appropriately.
 */
void platform_get_input(GameInput *input);

/* platform_shutdown — clean up all platform resources.
 *
 * Called ONCE at the end of main() before exit.
 * Must:
 *   - Close the audio device.
 *   - Destroy the window / GL context.
 *   - Free the backbuffer pixel array.
 *
 * Calling order in main():
 *   … game loop …
 *   platform_shutdown();
 *   return 0;
 *
 * COURSE NOTE: The reference source has no equivalent function.
 * Explicit shutdown is good practice — it makes resource lifetimes visible
 * and allows valgrind to report a clean exit.
 */
void platform_shutdown(void);

/* ══════ platform_swap_input_buffers ════════════════════════════════════════
 *
 * Swap the CONTENTS of two GameInput structs so that:
 *   old_input    ← what current_input contained last frame
 *   current_input ← gets a copy of old_input (ready for prepare_input_frame)
 *
 * WHY SWAP CONTENTS INSTEAD OF SWAPPING POINTERS?
 *   Swapping pointers would require the caller to track which pointer is
 *   "old" and which is "current" every frame — easy to confuse.  Swapping
 *   contents keeps the variable names stable: old_input is always the
 *   previous frame, current_input is always the frame being built.
 *
 * IMPLEMENTATION: This is a static inline (not a regular function) because:
 *   • It's small (3 lines of assignment).
 *   • The compiler will almost certainly inline it — no call overhead.
 *   • It's defined in the header so both backends get the same implementation
 *     without needing an extra .c file.
 *
 * CALLING ORDER each frame:
 *   platform_swap_input_buffers(&inputs[0], &inputs[1]);  // old ↔ current swap
 *   prepare_input_frame(&inputs[0], &inputs[1]);          // reset transitions
 *   platform_get_input(&inputs[1]);                       // fill events
 *   snake_update(&state, &inputs[1], delta_time);         // game logic
 *
 * JS equivalent: [a, b] = [b, a]  — destructuring swap on the input objects.
 */
static inline void platform_swap_input_buffers(GameInput *old_input,
                                                GameInput *current_input) {
    GameInput temp   = *old_input;      /* copy old to temp                     */
    *old_input       = *current_input;  /* old  ← current (save current as old) */
    *current_input   = temp;            /* current ← temp  (restore old to current for prepare) */
}

#endif /* PLATFORM_H */
