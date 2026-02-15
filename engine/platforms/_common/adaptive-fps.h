#ifndef DE100_PLATFORMS__COMMON_ADAPTIVE_FPS_H
#define DE100_PLATFORMS__COMMON_ADAPTIVE_FPS_H

#include "../../_common/base.h"
#include "../../game/config.h"

typedef struct {
  u32 frames_sampled;
  u32 frames_missed;
  // Time-based instead of frame-based
  f32 sample_window_seconds;
  f32 miss_threshold;
  f32 recover_threshold;
  u32 frames_since_last_change;
  u32 cooldown_frames;

  // Quick recovery tracking
  u32 consecutive_good_frames;
  f32 recent_frame_times[10];
  int recent_frame_index;
  // Track valid samples
  int recent_frame_count;
} AdaptiveFPS;
extern AdaptiveFPS g_adaptive_fps;

void adaptive_fps_init(void);
void adaptive_fps_update(GameConfig *game_config, f32 frame_time_ms);

#endif // DE100_PLATFORMS__COMMON_ADAPTIVE_FPS_H
