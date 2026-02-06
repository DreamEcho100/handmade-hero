#ifndef DE100_PLATFORMS__COMMON_FRAME_STATS_H
#define DE100_PLATFORMS__COMMON_FRAME_STATS_H

#include "../../_common/base.h"
#include "../../_common/time.h"

typedef struct {
  uint32 frame_count;
  uint32 missed_frames;
  real32 min_frame_time_ms;
  real32 max_frame_time_ms;
  real32 total_frame_time_ms;
} FrameStats;
extern FrameStats g_frame_stats;

void frame_stats_init();
void frame_stats_record(real32 frame_time_ms, real32 target_seconds_per_frame);
void frame_stats_print();

#endif // DE100_PLATFORMS__COMMON_FRAME_STATS_H