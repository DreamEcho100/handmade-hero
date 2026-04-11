
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdlib.h> /* size_t, malloc, free */
#include <string.h> /* memset */
#include <sys/mman.h>

/* Include shared types used on both sides of the platform contract. */
#include "./utils/backbuffer.h"
#include "./utils/base.h"
#include "utils/base.h"
/* NOTE: audio.h is NOT included yet — that comes in L09. */

/* ── Loop timing ──────────────────────────────────────────────────────── */
#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif

/* ── String/title helpers — two-level macro trick ─────────────────────── */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifndef GAME_TITLE
#define GAME_TITLE "Asteroids"
#endif
#ifndef GAME_VERSION
#define GAME_VERSION "1.0"
#endif
#define TITLE GAME_TITLE " v" GAME_VERSION " - (" TOSTRING(BACKEND) ")"

/* ── Utility macros ───────────────────────────────────────────────────── */
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct {
  Backbuffer backbuffer;
  WindowScaleMode scale_mode;   /* LESSON 09 — defaults to FIXED */
  GameWorldConfig world_config; /* LESSON 09 — ZII = COORD_ORIGIN_LEFT_BOTTOM */
} PlatformGameProps;

static inline int platform_game_props_init(PlatformGameProps *props) {
  props->scale_mode = WINDOW_SCALE_MODE_FIXED;
  props->world_config.coord_origin = COORD_ORIGIN_LEFT_BOTTOM;

  props->backbuffer.width = GAME_W;
  props->backbuffer.height = GAME_H;
  props->backbuffer.bytes_per_pixel = 4;
  props->backbuffer.pitch = GAME_W * 4;
  void *result =
      mmap(NULL, (size_t)(GAME_W * GAME_H) * 4, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (result == MAP_FAILED) {
    return -1;
  }
  props->backbuffer.pixels = (uint32_t *)
      result; // page-aligned, safe cast, cast only after success to avoid `UB`
  return 0;
}

static inline void platform_game_props_free(PlatformGameProps *props) {
  if (props->backbuffer.pixels) {
    munmap(props->backbuffer.pixels, (size_t)(GAME_W * GAME_H) * 4);
    props->backbuffer.pixels = NULL;
  }
}

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

#endif /* PLATFORM_H */
