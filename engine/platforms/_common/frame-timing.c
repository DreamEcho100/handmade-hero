
#include "./frame-timing.h"

#if DE100_INTERNAL
#include <x86intrin.h> // For __rdtsc() CPU cycle counter
#endif

FrameTiming g_frame_timing = {0};

void frame_timing_begin(void) {
  de100_get_timespec(&g_frame_timing.frame_start);
#if DE100_INTERNAL
  g_frame_timing.start_cycles = __rdtsc();
#endif
}

void frame_timing_mark_work_done(void) {
  de100_get_timespec(&g_frame_timing.work_end);
  g_frame_timing.work_seconds = (f32)de100_timespec_diff_seconds(
      &g_frame_timing.frame_start, &g_frame_timing.work_end);
}

void frame_timing_sleep_until_target(f32 target_seconds) {
  f32 seconds_elapsed = g_frame_timing.work_seconds;

  if (seconds_elapsed < target_seconds) {
    f32 sleep_threshold = target_seconds - 0.003f;

    // Phase 1: Coarse sleep
    while (seconds_elapsed < sleep_threshold) {
      de100_sleep_ms(1);

      De100TimeSpec current;
      de100_get_timespec(&current);
      seconds_elapsed = (f32)de100_timespec_diff_seconds(
          &g_frame_timing.frame_start, &current);
    }

    // Phase 2: Spin-wait
    while (seconds_elapsed < target_seconds) {
      De100TimeSpec current;
      de100_get_timespec(&current);
      seconds_elapsed = (f32)de100_timespec_diff_seconds(
          &g_frame_timing.frame_start, &current);
    }
  }
}

void frame_timing_end(void) {
  de100_get_timespec(&g_frame_timing.frame_end);
#if DE100_INTERNAL
  g_frame_timing.end_cycles = __rdtsc();
#endif

  g_frame_timing.total_seconds = (f32)de100_timespec_diff_seconds(
      &g_frame_timing.frame_start, &g_frame_timing.frame_end);
  g_frame_timing.sleep_seconds =
      g_frame_timing.total_seconds - g_frame_timing.work_seconds;

  // g_frame_timing.total_ms = (f32)de100_timespec_diff_milliseconds(
  //     &g_frame_timing.frame_start, &g_frame_timing.frame_end);
}

f32 frame_timing_get_ms(void) { return g_frame_timing.total_seconds * 1000.0f; }

f32 frame_timing_get_fps(void) { return 1.0f / g_frame_timing.total_seconds; }

#if DE100_INTERNAL
f32 frame_timing_get_mcpf(void) {
  return (g_frame_timing.end_cycles - g_frame_timing.start_cycles) / 1000000.0f;
}
#endif
