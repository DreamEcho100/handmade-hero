#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include "../utils/backbuffer.h"
#include "../utils/base.h"
#include "base.h"
#include "scene.h"
#include "render.h"
#include "settings.h"

typedef struct {
  Scene          scene;
  RtCamera       camera;
  RenderSettings settings;
  /* Adaptive quality state. */
  int            still_frames;
  int            active_scale;
  /* Performance metrics. */
  float          frame_time_ms;     /* last frame render time              */
  float          fps_smoothed;      /* exponential moving average FPS      */
  int            ray_count_k;       /* rays traced this frame (thousands)  */
  float          rays_per_sec_m;    /* throughput (millions/sec)           */
  float          cpu_util_pct;      /* render_ms / target_ms * 100        */
  int            thread_count;      /* active render threads               */
  /* GPU state (set by platform at init). */
  int            gpu_available;     /* 1 if GL 3.3+ shader compiled OK     */
  float          gpu_compile_ms;    /* shader compile time                 */
  char           gpu_renderer[128]; /* glGetString(GL_RENDERER)            */
  /* HUD visibility (F1 toggle, default on). */
  int            show_hud;
  /* Export flags. */
  int            export_ppm_requested;
  int            export_anaglyph_requested;
  int            export_sidebyside_requested;
  int            export_stereogram_requested;
  int            export_stereogram_cross_requested;
  int            export_glsl_requested;
} RaytracerState;

void game_init(RaytracerState *state);
void game_update(RaytracerState *state, GameInput *input, float delta_time);
void game_render(RaytracerState *state, Backbuffer *bb, GameWorldConfig world_config);

#endif /* GAME_MAIN_H */
