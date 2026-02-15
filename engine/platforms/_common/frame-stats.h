#ifndef DE100_PLATFORMS__COMMON_FRAME_STATS_H
#define DE100_PLATFORMS__COMMON_FRAME_STATS_H

#include "../../_common/base.h"

typedef struct {
  u32 frame_count;
  u32 missed_frames;
  f32 min_frame_time_ms;
  f32 max_frame_time_ms;
  f32 total_frame_time_ms;
} FrameStats;
extern FrameStats g_frame_stats;

void frame_stats_init(void);
void frame_stats_record(f32 frame_time_ms, f32 target_seconds_per_frame);
void frame_stats_print(void);

#endif // DE100_PLATFORMS__COMMON_FRAME_STATS_H
