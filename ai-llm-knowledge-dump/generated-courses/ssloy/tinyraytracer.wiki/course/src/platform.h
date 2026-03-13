#ifndef PLATFORM_H
#define PLATFORM_H

/* ═══════════════════════════════════════════════════════════════════════════
 * platform.h — TinyRaytracer Course (adapted from Platform Foundation)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 05 — The platform contract: types and function signatures shared
 *             by ALL backends (Raylib, X11) and the game layer.
 *
 * JS analogy: This is the "interface" that every backend must implement,
 * like a TypeScript interface for a Node.js server vs. a browser runtime.
 *
 * DESIGN RULE
 * ───────────
 * This header contains ONLY:
 *   • Types both sides see (Backbuffer, AudioOutputBuffer, PlatformAudioConfig,
 *     GameInput — the latter is in game/base.h)
 *   • Platform-provided function prototypes (platform_audio_*)
 *   • Shared constants (GAME_W, GAME_H, TARGET_FPS, etc.)
 *   • Utility macros (STRINGIFY, TOSTRING, TITLE, ARRAY_LEN)
 *
 * It does NOT contain game logic. Game logic lives in game/base.h.
 *
 * LESSON 05 — STRINGIFY / TOSTRING / TITLE macros
 * ─────────────────────────────────────────────────
 * STRINGIFY(x)  → turns x into the string literal "x" (pre-expansion)
 * TOSTRING(x)   → expands x first, then stringifies (handles macros)
 * TITLE         → produces "GameTitle v1.0" at compile time, zero cost
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include <stdlib.h>        /* size_t, malloc, free */

/* Include shared types used on both sides of the platform contract. */
#include "./utils/backbuffer.h"
#include "./utils/audio.h"

/* ── Canvas dimensions ───────────────────────────────────────────────────
 * Game courses override GAME_W / GAME_H via -DGAME_W=N -DGAME_H=N flags.
 * LESSON 05 — why fixed canvas + scalable window (not fixed window).        */
#ifndef GAME_W
#  define GAME_W 800
#endif
#ifndef GAME_H
#  define GAME_H 600
#endif

/* ── Loop timing ─────────────────────────────────────────────────────────
 * LESSON 08 — TARGET_FPS drives the fixed timestep in both backends.       */
#ifndef TARGET_FPS
#  define TARGET_FPS 60
#endif

/* ── String/title helpers ─────────────────────────────────────────────────
 * LESSON 05 — Two-level macro trick: STRINGIFY does not expand; TOSTRING
 * forces expansion first.  TITLE is evaluated at compile time.             */
#define STRINGIFY(x)  #x
#define TOSTRING(x)   STRINGIFY(x)

#ifndef GAME_TITLE
#  define GAME_TITLE  "TinyRaytracer"
#endif
#ifndef GAME_VERSION
#  define GAME_VERSION  "1.0"
#endif
#define TITLE  GAME_TITLE " v" GAME_VERSION

/* ── Utility macros ───────────────────────────────────────────────────────
 * LESSON 05 — ARRAY_LEN: safe compile-time array size (not sizeof a ptr).  */
#define ARRAY_LEN(arr)  (sizeof(arr) / sizeof((arr)[0]))

/* ── Platform-provided audio functions ────────────────────────────────────
 * Prototypes for functions implemented in platforms/audio.c.
 * Both backends must implement all four.
 *
 * LESSON 09 (minimal), LESSON 11/12 (full implementations).
 *
 * NOTE: platform_swap_input_buffers and prepare_input_frame are game-layer
 * helpers (not OS-specific) and live in game/base.h.                       */

/* Initialize platform audio device and stream.
 * Returns 0 on success, -1 on failure. */
int  platform_audio_init(PlatformAudioConfig *cfg);

/* Shut down platform audio. Call once on program exit. */
void platform_audio_shutdown(void);

/* How many sample-frames can be written right now without blocking.
 * A "sample-frame" = one left sample + one right sample.
 * Returns 0 if not ready or an error occurred. */
int  platform_audio_get_samples_to_write(PlatformAudioConfig *cfg);

/* Write `num_frames` sample-frames from `buf->samples` to the audio device.
 * num_frames must be ≤ buf->sample_count.                                   */
void platform_audio_write(AudioOutputBuffer *buf, int num_frames,
                          PlatformAudioConfig *cfg);

#endif /* PLATFORM_H */
