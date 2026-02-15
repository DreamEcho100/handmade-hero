#include "./frame-stats.h"
#include <stdio.h>

FrameStats g_frame_stats = {0};

void frame_stats_init(void) {
  g_frame_stats.frame_count = 0;
  g_frame_stats.missed_frames = 0;
  g_frame_stats.min_frame_time_ms = 0.0f;
  g_frame_stats.max_frame_time_ms = 0.0f;
  g_frame_stats.total_frame_time_ms = 0.0f;
}

void frame_stats_record(f32 frame_time_ms, f32 target_seconds_per_frame) {
  g_frame_stats.frame_count++;

  if (frame_time_ms < g_frame_stats.min_frame_time_ms ||
      g_frame_stats.frame_count == 1) {
    g_frame_stats.min_frame_time_ms = frame_time_ms;
  }
  if (frame_time_ms > g_frame_stats.max_frame_time_ms) {
    g_frame_stats.max_frame_time_ms = frame_time_ms;
  }

  g_frame_stats.total_frame_time_ms += frame_time_ms;

  if ((frame_time_ms / 1000.0f) > (target_seconds_per_frame + 0.002f)) {
    g_frame_stats.missed_frames++;
  }
}

void frame_stats_print(void) {
  printf("\n═══════════════════════════════════════════════════════════\n");
  printf("📊 FRAME TIME STATISTICS\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Total frames:   %u\n", g_frame_stats.frame_count);
  printf("Missed frames:  %u (%.2f%%)\n", g_frame_stats.missed_frames,
         (f32)g_frame_stats.missed_frames / g_frame_stats.frame_count * 100.0f);
  printf("Min frame time: %.2fms\n", g_frame_stats.min_frame_time_ms);
  printf("Max frame time: %.2fms\n", g_frame_stats.max_frame_time_ms);
  printf("Avg frame time: %.2fms\n",
         g_frame_stats.total_frame_time_ms / g_frame_stats.frame_count);
  printf("═══════════════════════════════════════════════════════════\n");
}
