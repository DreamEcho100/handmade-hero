#ifndef DE100_PLATFORM__COMMON_ADAPTIVE_FPS_H
#define DE100_PLATFORM__COMMON_ADAPTIVE_FPS_H

#include "../../_common/base.h"
#include "../../game/base.h"
#include "../../game/config.h"
#include "config.h"

typedef struct {
  uint32 frames_sampled;
  uint32 frames_missed;
  uint32 sample_window;
  real32 miss_threshold;
  real32 recover_threshold;
  uint32 frames_since_last_change;
  uint32 cooldown_frames;

  // Quick recovery tracking
  uint32 consecutive_good_frames;
  real32 recent_frame_times[10];
  int recent_frame_index;
} AdaptiveFPS;

void adaptive_fps_init(AdaptiveFPS *adaptive);
void adaptive_fps_update(AdaptiveFPS *adaptive, GameConfig *game_config,
                         PlatformConfig *platform_config, real32 frame_time_ms);

#endif // DE100_PLATFORM__COMMON_ADAPTIVE_FPS_H