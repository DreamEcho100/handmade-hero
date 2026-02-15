#ifndef DE100_PLATFORMS__COMMON_FRAME_TIMING_FPS_H
#define DE100_PLATFORMS__COMMON_FRAME_TIMING_FPS_H

#include "../../_common/base.h"
#include "../../_common/time.h"

typedef struct {
  De100TimeSpec frame_start;
  De100TimeSpec work_end;
  De100TimeSpec frame_end;
  f32 work_seconds;
  f32 total_seconds;
  // f32 total_ms;
  f32 sleep_seconds;
#if DE100_INTERNAL
  u64 start_cycles;
  u64 end_cycles;
#endif
} FrameTiming;
extern FrameTiming g_frame_timing;

void frame_timing_begin(void);

void frame_timing_mark_work_done(void);

void frame_timing_sleep_until_target(f32 target_seconds);

void frame_timing_end(void);

f32 frame_timing_get_ms(void);

f32 frame_timing_get_fps(void);

#if DE100_INTERNAL
f32 frame_timing_get_mcpf(void);
#endif

#endif // DE100_PLATFORMS__COMMON_FRAME_TIMING_FPS_H
