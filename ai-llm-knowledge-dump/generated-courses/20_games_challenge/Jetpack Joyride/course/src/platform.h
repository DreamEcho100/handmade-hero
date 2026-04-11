#ifndef PLATFORM_H
#define PLATFORM_H

/* ═══════════════════════════════════════════════════════════════════════════
 * platform.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Platform contract: types and function signatures shared by ALL backends
 * (Raylib, X11) and the game layer. Adapted from Platform Foundation Course.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include <stdlib.h> /* size_t, malloc, free */
#include <string.h> /* memset */

#include "./utils/audio.h"
#include "./utils/backbuffer.h"
#include "./utils/vec3.h"
#include "./utils/base.h"
#include "./utils/arena.h"

/* ── Loop timing ──────────────────────────────────────────────────────── */
#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif

/* ── String/title helpers ─────────────────────────────────────────────── */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifndef GAME_TITLE
#define GAME_TITLE "Jetpack Joyride"
#endif
#ifndef GAME_VERSION
#define GAME_VERSION "1.0"
#endif
#define TITLE GAME_TITLE " v" GAME_VERSION

/* ── Utility macros ──────────────────────────────────────────────────── */
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ═══════════════════════════════════════════════════════════════════════════
 * PlatformGameProps — shared state allocated identically by every backend
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  Backbuffer backbuffer;
  AudioOutputBuffer audio_buffer;
  PlatformAudioConfig audio_config;
  WindowScaleMode scale_mode;
  GameWorldConfig world_config;
  Arena perm;
  Arena scratch;
  ArenaMmap perm_mmap;
  ArenaMmap scratch_mmap;
} PlatformGameProps;

static inline int platform_game_props_init(PlatformGameProps *props) {
  /* Default to FIXED (letterbox) mode with LEFT_TOP origin. */
  props->scale_mode = WINDOW_SCALE_MODE_FIXED;
  props->world_config.coord_origin = COORD_ORIGIN_LEFT_TOP;
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

  /* ── Perm arena (4 MB, session lifetime) ────────────────────────────── */
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

/* ── Letterbox ──────────────────────────────────────────────────────────── */
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

/* ── Platform-provided audio functions ──────────────────────────────────── */
int platform_audio_init(PlatformAudioConfig *config);
void platform_audio_shutdown(void);
int platform_audio_get_samples_to_write(PlatformAudioConfig *config);
void platform_audio_write(AudioOutputBuffer *buffer, int num_frames,
                          PlatformAudioConfig *config);

#endif /* PLATFORM_H */
