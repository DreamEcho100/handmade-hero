#ifndef PLATFORM_H
#define PLATFORM_H

/* ═══════════════════════════════════════════════════════════════════════════
 * platform.h — The Platform/Game Contract for Asteroids
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * This header defines the two-way contract between platform backends
 * (Raylib, X11) and the game layer.
 *
 * DESIGN RULE
 * ───────────
 * Game code (game/game.c, game/audio.c) must never #include anything
 * platform-specific (X11.h, raylib.h).  It depends ONLY on the types
 * defined in the utils/ headers below.
 *
 * JS analogy: platform.h is like a TypeScript interface that every backend
 * must implement.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include <stdlib.h> /* size_t, malloc, free */
#include <string.h> /* memset */

/* Shared types used on both sides of the platform contract. */
#include "./utils/arena.h"
#include "./utils/audio.h"
#include "./utils/backbuffer.h"
#include "./utils/base.h"

/* ── Loop timing ─────────────────────────────────────────────────────────
 * TARGET_FPS drives the fixed timestep in both backends.                 */
#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif

/* ── String/title helpers ─────────────────────────────────────────────────
 * Two-level macro trick: STRINGIFY does not expand; TOSTRING forces
 * expansion first.  TITLE is evaluated at compile time.                  */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifndef GAME_TITLE
#define GAME_TITLE "Asteroids"
#endif
#ifndef GAME_VERSION
#define GAME_VERSION "1.0"
#endif
#define TITLE GAME_TITLE " v" GAME_VERSION

/* ── Utility macros ───────────────────────────────────────────────────────
 * ARRAY_LEN: safe compile-time array size (not sizeof a ptr).             */
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ═══════════════════════════════════════════════════════════════════════════
 * PlatformGameProps — shared state allocated identically by every backend
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Both Raylib and X11 backends need the same backbuffer, audio buffer,
 * audio config, arenas, and world config.  Rather than duplicating the
 * allocation code in each main.c, we define a single struct and two shared
 * inline functions:
 *   platform_game_props_init — allocate backbuffer + audio buffer + arenas
 *   platform_game_props_free — free all
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  Backbuffer backbuffer;
  AudioOutputBuffer audio_buffer;
  PlatformAudioConfig audio_config;
  /* Current window scaling mode; defaults to FIXED (letterbox). */
  WindowScaleMode scale_mode;
  /* Coordinate origin; ZII default = COORD_ORIGIN_LEFT_TOP. */
  GameWorldConfig world_config;
  /* perm arena: session lifetime (never reset until shutdown).
   * scratch arena: per-frame lifetime (wrapped in TempMemory each frame).
   * Both arenas are backed by mmap + guard pages (see arena.h). */
  Arena perm;
  Arena scratch;
  /* mmap metadata for cleanup in platform_game_props_free. */
  ArenaMmap perm_mmap;
  ArenaMmap scratch_mmap;
} PlatformGameProps;

/* platform_game_props_init — allocate backbuffer pixels, audio sample
 * buffer, and arenas.  Fills in all struct fields from GAME_W/H and
 * AUDIO_* constants.
 * Returns 0 on success, -1 on allocation failure. */
static inline int platform_game_props_init(PlatformGameProps *props) {
  /* Default to FIXED (letterbox) mode. */
  props->scale_mode = WINDOW_SCALE_MODE_FIXED;
  props->world_config.coord_origin = COORD_ORIGIN_LEFT_BOTTOM;
  /* camera_zoom=1.0 = no zoom; camera_x/y=0 = no pan.
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

  /* ── Audio buffer ───────────────────────────────────────────────────── */
  props->audio_buffer.sample_count = AUDIO_CHUNK_SIZE;
  props->audio_buffer.max_sample_count = AUDIO_CHUNK_SIZE;
  props->audio_buffer.samples =
      (float *)malloc((size_t)AUDIO_CHUNK_SIZE * AUDIO_BYTES_PER_FRAME);
  if (!props->audio_buffer.samples) {
    free(props->backbuffer.pixels);
    props->backbuffer.pixels = NULL;
    return -1;
  }
  memset(props->audio_buffer.samples, 0,
         (size_t)AUDIO_CHUNK_SIZE * AUDIO_BYTES_PER_FRAME);

  /* ── Perm arena (1 MB, session lifetime) ───────────────────────────── */
  void *perm_mem = arena_alloc_mmap(ARENA_PERM_SIZE, &props->perm_mmap);
  if (!perm_mem) {
    free(props->audio_buffer.samples);
    props->audio_buffer.samples = NULL;
    free(props->backbuffer.pixels);
    props->backbuffer.pixels = NULL;
    return -1;
  }
  arena_init_from_block(&props->perm, perm_mem, ARENA_PERM_SIZE,
                        ARENA_PERM_SIZE);

  /* ── Scratch arena (256 KB, per-frame lifetime) ─────────────────────── */
  void *scratch_mem =
      arena_alloc_mmap(ARENA_SCRATCH_SIZE, &props->scratch_mmap);
  if (!scratch_mem) {
    arena_free_mmap(&props->perm, props->perm_mmap);
    free(props->audio_buffer.samples);
    props->audio_buffer.samples = NULL;
    free(props->backbuffer.pixels);
    props->backbuffer.pixels = NULL;
    return -1;
  }
  arena_init_from_block(&props->scratch, scratch_mem, ARENA_SCRATCH_SIZE,
                        ARENA_SCRATCH_SIZE);

  return 0;
}

/* platform_game_props_free — release all allocated memory. */
static inline void platform_game_props_free(PlatformGameProps *props) {
  arena_free_mmap(&props->scratch, props->scratch_mmap);
  arena_free_mmap(&props->perm, props->perm_mmap);

  if (props->audio_buffer.samples) {
    free(props->audio_buffer.samples);
    props->audio_buffer.samples = NULL;
  }
  if (props->backbuffer.pixels) {
    free(props->backbuffer.pixels);
    props->backbuffer.pixels = NULL;
  }
}

/* platform_compute_letterbox: scale + offset to fit canvas into
 * window preserving aspect ratio (no distortion).
 *
 * Algorithm:
 *   scale = min(window_w / canvas_w, window_h / canvas_h)
 *   offset_x = (window_w - canvas_w * scale) / 2
 *   offset_y = (window_h - canvas_h * scale) / 2           */
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

/* platform_compute_aspect_size: compute backbuffer dimensions
 * that fit inside win_w × win_h while preserving aspect_w:aspect_h.  */
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
 * Prototypes for functions implemented in platforms/x11/audio.c or
 * the raylib backend.  Both backends must implement all four.            */

/* Initialize platform audio device and stream.
 * Returns 0 on success, -1 on failure. */
int platform_audio_init(PlatformAudioConfig *config);

/* Shut down platform audio. Call once on program exit. */
void platform_audio_shutdown(void);

/* How many sample-frames can be written right now without blocking.
 * A "sample-frame" = one left sample + one right sample.
 * Returns 0 if not ready or an error occurred. */
int platform_audio_get_samples_to_write(PlatformAudioConfig *config);

/* Write `num_frames` sample-frames from `buf->samples` to the audio device.
 * num_frames must be ≤ buf->sample_count.                                   */
void platform_audio_write(AudioOutputBuffer *buffer, int num_frames,
                          PlatformAudioConfig *config);

#endif /* PLATFORM_H */
