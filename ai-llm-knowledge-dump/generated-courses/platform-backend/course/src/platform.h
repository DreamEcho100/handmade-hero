#ifndef PLATFORM_H
#define PLATFORM_H

/* ═══════════════════════════════════════════════════════════════════════════
 * platform.h — Platform Foundation Course
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
#include <stdlib.h> /* size_t, malloc, free */
#include <string.h> /* memset */

/* Include shared types used on both sides of the platform contract. */
#include "./utils/audio.h"
#include "./utils/backbuffer.h"
#include "./utils/vec3.h"

/* LESSON 09 — game constants + WindowScaleMode enum live in utils/base.h.
 * This keeps the unit system and scale mode visible to both platforms
 * and the game layer without polluting either side.                         */
#include "./utils/base.h"

/* LESSON 16 — Arena allocator: ArenaBlock, Arena, TempMemory, macros. */
#include "./utils/arena.h"

/* ── Loop timing ─────────────────────────────────────────────────────────
 * LESSON 08 — TARGET_FPS drives the fixed timestep in both backends.       */
#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif

/* ── String/title helpers ─────────────────────────────────────────────────
 * LESSON 05 — Two-level macro trick: STRINGIFY does not expand; TOSTRING
 * forces expansion first.  TITLE is evaluated at compile time.             */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifndef GAME_TITLE
#define GAME_TITLE "Platform Foundation"
#endif
#ifndef GAME_VERSION
#define GAME_VERSION "1.0"
#endif
#define TITLE GAME_TITLE " v" GAME_VERSION

/* ── Utility macros ───────────────────────────────────────────────────────
 * LESSON 05 — ARRAY_LEN: safe compile-time array size (not sizeof a ptr).  */
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ═══════════════════════════════════════════════════════════════════════════
 * PlatformGameProps — shared state allocated identically by every backend
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 05 — Both Raylib and X11 backends need the same backbuffer, audio
 * buffer, and audio config.  Rather than duplicating the allocation code in
 * each main.c, we define a single struct and two shared inline functions:
 *   platform_game_props_init — allocate backbuffer + audio buffer
 *   platform_game_props_free — free both
 *
 * This follows the same pattern used in the Snake and Tetris game courses.
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  Backbuffer backbuffer;
  AudioOutputBuffer audio_buffer;
  PlatformAudioConfig audio_config;
  /* LESSON 09 — current window scaling mode; defaults to FIXED. */
  WindowScaleMode scale_mode;
  /* LESSON 09 — coordinate origin; ZII default = COORD_ORIGIN_LEFT_TOP. */
  GameWorldConfig world_config;
  /* LESSON 11 — perm arena: session lifetime (never reset until shutdown).
   * level arena: current scene lifetime (reset when the scene unloads).
   * scratch arena: per-frame lifetime (wrapped in TempMemory each frame).
   * Both backed by mmap + guard pages (arena_alloc in arena.h).
   * arena_free() releases everything — no separate tracking needed. */
  Arena perm;
  Arena level;
  Arena scratch;
} PlatformGameProps;

/* platform_game_props_init — allocate backbuffer pixels and audio sample
 * buffer.  Fills in all struct fields from GAME_W/H and AUDIO_* constants.
 * Returns 0 on success, -1 on allocation failure. */
static inline int platform_game_props_init(PlatformGameProps *props) {
  /* LESSON 09 — default to FIXED (letterbox) mode. */
  props->scale_mode = WINDOW_SCALE_MODE_FIXED;
  props->world_config.coord_origin = COORD_ORIGIN_LEFT_BOTTOM;
  /* LESSON 17 — camera_zoom=1.0 = no zoom; camera_x/y=0 = no pan.
   * Must be set explicitly — camera_zoom=0 produces a degenerate context. */
  props->world_config.camera_zoom = 1.0f;
  props->world_config.camera_x = 0.0f;
  props->world_config.camera_y = 0.0f;

  /* ── Backbuffer ─────────────────────────────────────────────────────── */
  props->backbuffer.width = GAME_W;
  props->backbuffer.height = GAME_H;
  props->backbuffer.bytes_per_pixel = 4;
  props->backbuffer.pitch = GAME_W * 4;
  props->backbuffer.pixels = (uint32_t *)malloc((size_t)(GAME_W * GAME_H) * 4);
  if (!props->backbuffer.pixels)
    return -1;
  memset(props->backbuffer.pixels, 0, (size_t)(GAME_W * GAME_H) * 4);

  /* ── Audio config ───────────────────────────────────────────────────── */
  props->audio_config.sample_rate = AUDIO_SAMPLE_RATE;
  props->audio_config.channels = AUDIO_CHANNELS;
  props->audio_config.chunk_size = AUDIO_CHUNK_SIZE;

  /* ── Audio buffer ───────────────────────────────────────────────────── *
   * Allocate for the largest possible format (F64 = 16 bytes/frame) so
   * the buffer works regardless of which backend format is chosen.       */
  props->audio_buffer.samples_per_second = AUDIO_SAMPLE_RATE;
  props->audio_buffer.sample_count = AUDIO_CHUNK_SIZE;
  props->audio_buffer.max_sample_count = AUDIO_CHUNK_SIZE;
  props->audio_buffer.format =
      AUDIO_FORMAT_F32; /* default, backend overrides */
  props->audio_buffer.is_initialized = 0;
  {
    int max_bps = audio_format_bytes_per_sample(AUDIO_FORMAT_F64);
    props->audio_buffer.samples_buffer =
        malloc((size_t)AUDIO_CHUNK_SIZE * (size_t)max_bps);
    if (!props->audio_buffer.samples_buffer) {
      free(props->backbuffer.pixels);
      props->backbuffer.pixels = NULL;
      return -1;
    }
    memset(props->audio_buffer.samples_buffer, 0,
           (size_t)AUDIO_CHUNK_SIZE * (size_t)max_bps);
  }

  /* ── LESSON 11 — Session / level / frame arenas ─────────────────────
   * arena_bootstrap() wraps zero-init + arena_alloc + arena_init_from_block
   * so the code can attach lifetime/debug metadata consistently.
   *
   * perm    : session lifetime   (assets, registries, long-lived state)
   * level   : scene lifetime     (map data, current-level entities)
   * scratch : frame lifetime     (temp strings, temp arrays, work buffers)
   */
  if (arena_bootstrap(&props->perm, ARENA_PERM_SIZE, ARENA_PERM_SIZE,
                      ARENA_LIFETIME_SESSION, "perm") != 0) {
    free(props->audio_buffer.samples_buffer);
    props->audio_buffer.samples_buffer = NULL;
    free(props->backbuffer.pixels);
    props->backbuffer.pixels = NULL;
    return -1;
  }

  if (arena_bootstrap(&props->level, ARENA_LEVEL_SIZE, ARENA_LEVEL_SIZE,
                      ARENA_LIFETIME_LEVEL, "level") != 0) {
    arena_free(&props->perm);
    free(props->audio_buffer.samples_buffer);
    props->audio_buffer.samples_buffer = NULL;
    free(props->backbuffer.pixels);
    props->backbuffer.pixels = NULL;
    return -1;
  }

  if (arena_bootstrap(&props->scratch, ARENA_SCRATCH_SIZE, ARENA_SCRATCH_SIZE,
                      ARENA_LIFETIME_FRAME, "scratch") != 0) {
    arena_free(&props->level);
    arena_free(&props->perm);
    free(props->audio_buffer.samples_buffer);
    props->audio_buffer.samples_buffer = NULL;
    free(props->backbuffer.pixels);
    props->backbuffer.pixels = NULL;
    return -1;
  }

  return 0;
}

/* platform_level_arena_reset — clear all scene-lifetime allocations.
 * Call when unloading/reloading a level.  Session data in perm survives.
 */
static inline void platform_level_arena_reset(PlatformGameProps *props) {
  arena_reset(&props->level);
}

/* platform_game_props_free — release all allocated memory. */
static inline void platform_game_props_free(PlatformGameProps *props) {
#ifndef NDEBUG
  arena_dump_stats(stderr, &props->scratch, props->scratch.debug_name);
  arena_dump_stats(stderr, &props->level, props->level.debug_name);
  arena_dump_stats(stderr, &props->perm, props->perm.debug_name);
#endif
  /* Free arenas first (they outlive backbuffer).
   * arena_free handles both mmap and malloc paths automatically. */
  arena_free(&props->scratch);
  arena_free(&props->level);
  arena_free(&props->perm);

  if (props->audio_buffer.samples_buffer) {
    free(props->audio_buffer.samples_buffer);
    props->audio_buffer.samples_buffer = NULL;
  }
  if (props->backbuffer.pixels) {
    free(props->backbuffer.pixels);
    props->backbuffer.pixels = NULL;
  }
}

/* LESSON 08 — platform_compute_letterbox: scale + offset to fit canvas into
 * window preserving aspect ratio (no distortion).
 *
 * Algorithm:
 *   scale = min(window_w / canvas_w, window_h / canvas_h)   (fit inside)
 *   offset_x = (window_w - canvas_w * scale) / 2             (center x)
 *   offset_y = (window_h - canvas_h * scale) / 2             (center y)
 *
 * JS analogy: CSS `object-fit: contain` on a <canvas> element.           */
static inline void platform_compute_letterbox(int win_w, int win_h,
                                              int canvas_w, int canvas_h,
                                              float *scale, int *off_x,
                                              int *off_y) {
  float sx = (float)win_w / (float)canvas_w;
  float sy = (float)win_h / (float)canvas_h;
  *scale = (sx < sy) ? sx : sy;
  *off_x = (int)((win_w - canvas_w * *scale) * 0.5f);
  *off_y = (int)((win_h - canvas_h * *scale) * 0.5f);
}

/* LESSON 09 — platform_compute_aspect_size: compute backbuffer dimensions
 * that fit inside win_w × win_h while preserving aspect_w:aspect_h.
 *
 * Used by WINDOW_SCALE_MODE_DYNAMIC_ASPECT to keep a 4:3 backbuffer even
 * as the window grows or shrinks.
 *
 * Algorithm:
 *   if window is wider than target → clamp height = win_h, scale width
 *   else                           → clamp width  = win_w, scale height */
static inline void platform_compute_aspect_size(int win_w, int win_h,
                                                float aspect_w, float aspect_h,
                                                int *out_w, int *out_h) {
  float target_aspect = aspect_w / aspect_h;
  float window_aspect = (float)win_w / (float)win_h;

  if (window_aspect > target_aspect) {
    *out_h = win_h;
    *out_w = (int)(win_h * target_aspect);
  } else {
    *out_w = win_w;
    *out_h = (int)(win_w / target_aspect);
  }
}

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
int platform_audio_init(PlatformAudioConfig *config);

/* Shut down platform audio. Call once on program exit. */
void platform_audio_shutdown(void);

/* How many sample-frames can be written right now without blocking.
 * A "sample-frame" = one left sample + one right sample.
 * Returns 0 if not ready or an error occurred. */
int platform_audio_get_samples_to_write(PlatformAudioConfig *config);

/* Write `num_frames` sample-frames from `buf->samples_buffer` to the audio
 * device. num_frames must be ≤ buf->sample_count. */
void platform_audio_write(AudioOutputBuffer *buffer, int num_frames,
                          PlatformAudioConfig *config);

#endif /* PLATFORM_H */
