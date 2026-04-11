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
 * This header defines that two-way contract using:
 *   • PlatformGameProps    — data the platform owns and passes to the game
 *   • Function declarations for platform → game calls
 *   • Inline helpers (letterbox, input swap) used by both layers
 *
 * DESIGN PRINCIPLE — Dependency inversion:
 * The game code (game/game.c, game/audio.c) must NEVER #include anything
 * platform-specific (X11.h, raylib.h).  It depends only on the types in
 * utils/backbuffer.h and utils/audio.h.  Platforms depend on the game —
 * never the other way around.
 *
 * JS equivalent:
 *   // A platform.h is like a TypeScript interface:
 *   interface Platform {
 *     init(): void;
 *     display(props: PlatformGameProps): void;
 *     getInput(): GameInput;
 *     shutdown(): void;
 *   }
 * =============================================================================
 */

#ifndef ASTEROIDS_PLATFORM_H
#define ASTEROIDS_PLATFORM_H

#include "game/game.h"

/* ══════ PlatformGameProps — platform-owned data passed to the game ═════════
 *
 * LESSON 07 — Platform/game separation via PlatformGameProps.
 *
 * The platform owns the pixel buffer, the coordinate config, and the scale
 * mode.  Every frame it packs these into PlatformGameProps and passes them
 * to asteroids_render().  The game never touches the window directly.
 *
 * WHY NOT PASS EACH FIELD SEPARATELY?
 *   As more fields are added (letterbox, zoom, camera_x/y), a single struct
 *   is far easier to thread through function calls than a growing argument list.
 *   It also lets the platform change the struct layout without touching game.c.
 *
 * FIELDS:
 *   backbuffer   — the pixel array the game should render into
 *   world_config — coordinate origin + camera settings (set by the platform,
 *                  read by the game to build RenderContext each frame)
 *   scale_mode   — how the platform should scale the game canvas to the window
 */
typedef struct {
    Backbuffer      backbuffer;     /* pixel buffer the game renders into */
    GameWorldConfig world_config;   /* coordinate system config           */
    WindowScaleMode scale_mode;     /* how to scale game canvas to window */
} PlatformGameProps;

/* ══════ PlatformGameProps initialisation ═══════════════════════════════════
 *
 * LESSON 07 — Call ONCE at startup to set default values.
 * Zero-initialises everything, then sets sensible defaults:
 *   coord_origin = COORD_ORIGIN_LEFT_BOTTOM  (y-up, ZII default)
 *   camera_zoom  = 1.0                       (no zoom)
 *   scale_mode   = SCALE_ASPECT_FIT          (letterbox to fit window)
 */
static inline void platform_game_props_init(PlatformGameProps *props) {
    if (!props) return;
    /* Zero everything first */
    Backbuffer zero_bb = {0};
    props->backbuffer  = zero_bb;

    GameWorldConfig cfg = {0};
    cfg.coord_origin = COORD_ORIGIN_LEFT_BOTTOM;  /* y-up, Cartesian */
    cfg.camera_zoom  = 1.0f;
    cfg.camera_x     = 0.0f;
    cfg.camera_y     = 0.0f;
    props->world_config = cfg;

    props->scale_mode = WINDOW_SCALE_MODE_FIXED;
}

/* ══════ platform_compute_letterbox ─────────────────────────────────────────
 *
 * LESSON 08 — Compute a centered, aspect-preserving viewport inside a window.
 *
 * Given:
 *   game_w, game_h  — the fixed game canvas size (e.g. GAME_W × GAME_H)
 *   win_w, win_h    — the current window size (may be resized by user)
 * Outputs:
 *   *out_x, *out_y  — pixel offset from window top-left to viewport top-left
 *   *out_w, *out_h  — viewport width and height in window pixels
 *
 * The scale factor min(win_w/game_w, win_h/game_h) preserves aspect ratio.
 * Black bars fill the unused space (pillarboxing or letterboxing).         */
static inline void platform_compute_letterbox(int game_w, int game_h,
                                              int win_w,  int win_h,
                                              int *out_x, int *out_y,
                                              int *out_w, int *out_h) {
    float sx    = (float)win_w / (float)game_w;
    float sy    = (float)win_h / (float)game_h;
    float scale = (sx < sy) ? sx : sy;
    *out_w = (int)((float)game_w * scale);
    *out_h = (int)((float)game_h * scale);
    *out_x = (win_w - *out_w) / 2;
    *out_y = (win_h - *out_h) / 2;
}

/* ══════ Platform → Game (game/game.c exports these) ═══════════════════════
 *
 * LESSON 07 — These are the four functions the platform calls each frame.
 * The platform provides the window, input hardware, and timing; the game
 * provides the logic, rendering, and audio generation.
 */

/* asteroids_init — one-time (or per-restart) game initialisation.
   Called:
     • Once from main() before the game loop starts.
     • Again from asteroids_update() when the player requests a restart.
   Zeros GameState (preserving audio config) and spawns starting objects.  */
void asteroids_init(GameState *state);

/* asteroids_update — advance the game simulation by one frame.
   state  — mutable game state (positions, velocities, score, phase, …)
   input  — current frame's button states (already prepared)
   dt     — time elapsed since the last frame, in seconds (~0.0167 at 60fps) */
void asteroids_update(GameState *state, const GameInput *input, float dt);

/* asteroids_render — draw the current frame into the backbuffer.
   state        — game state to render (ships, asteroids, bullets, HUD)
   bb           — pixel buffer to write into
   world_config — coord system config; used to build RenderContext each frame */
void asteroids_render(const GameState *state, Backbuffer *bb,
                      GameWorldConfig world_config);

/* game_get_audio_samples — fill one audio period with PCM sample data.
   Mixes all active sounds and writes 16-bit stereo PCM into `out`.         */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *out);

/* ══════ Game → Platform (platform backends export these) ══════════════════
 *
 * platform_init()     — open window, create GL context, init audio
 * platform_shutdown() — close audio, destroy GL context, close window
 */
void platform_init(void);
void platform_shutdown(void);

/* ══════ Input Double-Buffer ════════════════════════════════════════════════
 *
 * LESSON 06 — WHY TWO INPUT BUFFERS?
 * ─────────────────────────────────────
 * GameInput stores "is this key held?" (ended_down) AND "was it just
 * pressed this frame?" (half_transition_count).  Detecting "just pressed"
 * requires comparing current-frame state to previous-frame state.  Two
 * buffers let us do this without allocating memory each frame.
 *
 * Call order each frame:
 *   1. platform_swap_input_buffers(&inputs[0], &inputs[1])
 *   2. prepare_input_frame(&inputs[0], &inputs[1])    ← carry ended_down
 *   3. [platform reads hardware events into inputs[1]]
 *   4. asteroids_update(&state, &inputs[1], dt)
 *
 * JS equivalent:
 *   let prevKeys = { ...currentKeys };                                     */
static inline void platform_swap_input_buffers(GameInput *a, GameInput *b) {
    GameInput temp = *a;
    *a = *b;
    *b = temp;
}

#endif /* ASTEROIDS_PLATFORM_H */
