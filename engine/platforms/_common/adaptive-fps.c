#include "adaptive-fps.h"
#include "./hooks/utils.h"

AdaptiveFPS g_adaptive_fps = {0};

void adaptive_fps_init() {
  g_adaptive_fps.frames_sampled = 0;
  g_adaptive_fps.frames_missed = 0;
  g_adaptive_fps.sample_window_seconds =
      1.5f; // Changed: 1.5 seconds of sampling
  g_adaptive_fps.miss_threshold = 0.10f;
  g_adaptive_fps.recover_threshold = 0.02f;
  g_adaptive_fps.frames_since_last_change = 0;
  g_adaptive_fps.cooldown_frames = 180;
  g_adaptive_fps.consecutive_good_frames = 0;
  g_adaptive_fps.recent_frame_index = 0;
  g_adaptive_fps.recent_frame_count = 0; // Added

  for (int i = 0; i < 10; i++) {
    g_adaptive_fps.recent_frame_times[i] = 0.0f;
  }
}

de100_file_scoped_fn inline void
adaptive_fps_record_frame(real32 frame_time_ms, real32 target_frame_time_ms) {
  g_adaptive_fps.frames_sampled++;
  g_adaptive_fps.frames_since_last_change++;

  // Track recent frame times
  g_adaptive_fps.recent_frame_times[g_adaptive_fps.recent_frame_index] =
      frame_time_ms;
  g_adaptive_fps.recent_frame_index =
      (g_adaptive_fps.recent_frame_index + 1) % 10;

  // Track valid sample count
  if (g_adaptive_fps.recent_frame_count < 10) {
    g_adaptive_fps.recent_frame_count++;
  }

  // Check if frame missed (with tolerance)
  bool frame_missed = (frame_time_ms > (target_frame_time_ms + 3.0f));

  if (frame_missed) {
    g_adaptive_fps.frames_missed++;
    g_adaptive_fps.consecutive_good_frames = 0;
  } else {
    g_adaptive_fps.consecutive_good_frames++;
  }
}

de100_file_scoped_fn inline real32 adaptive_fps_get_recent_median() {
  if (g_adaptive_fps.recent_frame_count == 0) {
    return 0.0f;
  }

  // Copy to temporary array for sorting
  real32 sorted[10];
  for (int i = 0; i < g_adaptive_fps.recent_frame_count; i++) {
    sorted[i] = g_adaptive_fps.recent_frame_times[i];
  }

  // Insertion sort (fast for small arrays)
  for (int i = 1; i < g_adaptive_fps.recent_frame_count; i++) {
    real32 key = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }

  // Return median
  if (g_adaptive_fps.recent_frame_count % 2 == 0) {
    int mid = g_adaptive_fps.recent_frame_count / 2;
    return (sorted[mid - 1] + sorted[mid]) / 2.0f;
  } else {
    return sorted[g_adaptive_fps.recent_frame_count / 2];
  }
}

de100_file_scoped_fn inline bool
adaptive_fps_should_increase(GameConfig *game_config,
                             real32 target_frame_time_ms) {
  // Quick recovery check
  if (g_adaptive_fps.consecutive_good_frames >= 30 &&
      game_config->target_refresh_rate_hz <
          game_config->max_allowed_refresh_rate_hz &&
      g_adaptive_fps.frames_since_last_change >= 90) {

    real32 median_recent = adaptive_fps_get_recent_median(); // Changed
    real32 headroom_threshold = target_frame_time_ms * 0.80f;

    if (median_recent < headroom_threshold) {
      return true;
    }
  }

  // Calculate required samples based on time window
  uint32 required_samples =
      (uint32)(g_adaptive_fps.sample_window_seconds *
               (real32)game_config->target_refresh_rate_hz);

  // Regular evaluation
  if (g_adaptive_fps.frames_sampled >= required_samples &&
      g_adaptive_fps.frames_since_last_change >=
          g_adaptive_fps.cooldown_frames) {

    real32 miss_rate = (real32)g_adaptive_fps.frames_missed /
                       (real32)g_adaptive_fps.frames_sampled;

    if (miss_rate < g_adaptive_fps.recover_threshold &&
        game_config->target_refresh_rate_hz <
            game_config->max_allowed_refresh_rate_hz) {
      return true;
    }
  }

  return false;
}

de100_file_scoped_fn inline bool
adaptive_fps_should_decrease(GameConfig *game_config) {
  // Calculate required samples based on time window
  uint32 required_samples =
      (uint32)(g_adaptive_fps.sample_window_seconds *
               (real32)game_config->target_refresh_rate_hz);

  if (g_adaptive_fps.frames_sampled >= required_samples &&
      g_adaptive_fps.frames_since_last_change >=
          g_adaptive_fps.cooldown_frames) {

    real32 miss_rate = (real32)g_adaptive_fps.frames_missed /
                       (real32)g_adaptive_fps.frames_sampled;

    if (miss_rate > g_adaptive_fps.miss_threshold) {
      return true;
    }
  }

  return false;
}

de100_file_scoped_fn inline uint32
adaptive_fps_get_next_higher(uint32 current_fps, uint32 monitor_hz) {
  uint32 next_fps;

  switch (current_fps) {
  case FPS_30:
    next_fps = FPS_45;
    break;
  case FPS_45:
    next_fps = FPS_60;
    break;
  case FPS_60:
    next_fps = FPS_90;
    break;
  case FPS_90:
    next_fps = FPS_120;
    break;
  default:
    next_fps = monitor_hz;
    break;
  }

  return (next_fps > monitor_hz) ? monitor_hz : next_fps;
}

de100_file_scoped_fn inline uint32
adaptive_fps_get_next_lower(uint32 current_fps) {
  switch (current_fps) {
  case FPS_120:
    return FPS_90;
  case FPS_90:
    return FPS_60;
  case FPS_60:
    return FPS_45;
  case FPS_45:
    return FPS_30;
  default:
    return FPS_30;
  }
}

de100_file_scoped_fn inline void
adaptive_fps_apply_change(GameConfig *game_config, uint32 new_fps) {
  game_config->target_refresh_rate_hz = new_fps;
  game_config->target_seconds_per_frame = 1.0f / (real32)new_fps;

  g_adaptive_fps.frames_since_last_change = 0;
  g_adaptive_fps.frames_sampled = 0;
  g_adaptive_fps.frames_missed = 0;
  g_adaptive_fps.consecutive_good_frames = 0;
  g_adaptive_fps.recent_frame_count = 0; // Added: reset valid sample count

  de100_set_target_fps(new_fps);
}

de100_file_scoped_fn inline void adaptive_fps_reset_window() {
  g_adaptive_fps.frames_sampled = 0;
  g_adaptive_fps.frames_missed = 0;
}

void adaptive_fps_update(GameConfig *game_config, real32 frame_time_ms) {
  if (!game_config->prefer_adaptive_fps) {
    return;
  }

  real32 target_frame_time_ms = game_config->target_seconds_per_frame * 1000.0f;

  adaptive_fps_record_frame(frame_time_ms, target_frame_time_ms);

  // Calculate required samples for window reset check
  uint32 required_samples =
      (uint32)(g_adaptive_fps.sample_window_seconds *
               (real32)game_config->target_refresh_rate_hz);

  if (adaptive_fps_should_increase(game_config, target_frame_time_ms)) {
    uint32 old_fps = game_config->target_refresh_rate_hz;
    uint32 new_fps = adaptive_fps_get_next_higher(
        old_fps, game_config->max_allowed_refresh_rate_hz);

    if (new_fps != old_fps) {
      adaptive_fps_apply_change(game_config, new_fps);
#if DE100_INTERNAL
      printf("✅ ADAPTIVE: %d → %d Hz\n", old_fps, new_fps);
#endif
    }
  } else if (adaptive_fps_should_decrease(
                 game_config)) { // Changed: added game_config
    uint32 old_fps = game_config->target_refresh_rate_hz;
    uint32 new_fps = adaptive_fps_get_next_lower(old_fps);

    if (new_fps != old_fps) {
      adaptive_fps_apply_change(game_config, new_fps);
      printf("⚠️  ADAPTIVE: %d → %d Hz\n", old_fps, new_fps);
    }
  } else if (g_adaptive_fps.frames_sampled >=
             required_samples) { // Changed: use calculated samples
    adaptive_fps_reset_window();
  }
}
