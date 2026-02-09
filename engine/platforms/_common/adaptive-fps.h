#ifndef DE100_PLATFORMS__COMMON_ADAPTIVE_FPS_H
#define DE100_PLATFORMS__COMMON_ADAPTIVE_FPS_H

#include "../../_common/base.h"
#include "../../game/base.h"
#include "../../game/config.h"
#include "config.h"

typedef struct {
  uint32 frames_sampled;
  uint32 frames_missed;
  // Time-based instead of frame-based
  real32 sample_window_seconds;
  real32 miss_threshold;
  real32 recover_threshold;
  uint32 frames_since_last_change;
  uint32 cooldown_frames;

  // Quick recovery tracking
  uint32 consecutive_good_frames;
  real32 recent_frame_times[10];
  int recent_frame_index;
  // Track valid samples
  int recent_frame_count;
} AdaptiveFPS;
extern AdaptiveFPS g_adaptive_fps;

void adaptive_fps_init();
void adaptive_fps_update(GameConfig *game_config, real32 frame_time_ms);

#endif // DE100_PLATFORMS__COMMON_ADAPTIVE_FPS_H
