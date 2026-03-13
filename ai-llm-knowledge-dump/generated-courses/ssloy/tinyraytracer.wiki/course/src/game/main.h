#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include "../utils/backbuffer.h"
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
void game_render(RaytracerState *state, Backbuffer *bb);

#endif /* GAME_MAIN_H */
