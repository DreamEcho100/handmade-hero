#include "adaptive-fps.h"
#include "config.h"

void adaptive_fps_init(AdaptiveFPS *adaptive) {
  adaptive->frames_sampled = 0;
  adaptive->frames_missed = 0;
  adaptive->sample_window = 90;
  adaptive->miss_threshold = 0.10f;
  adaptive->recover_threshold = 0.02f;
  adaptive->frames_since_last_change = 0;
  adaptive->cooldown_frames = 180;
  adaptive->consecutive_good_frames = 0;
  adaptive->recent_frame_index = 0;

  for (int i = 0; i < 10; i++) {
    adaptive->recent_frame_times[i] = 0.0f;
  }
}

file_scoped_fn inline void
adaptive_fps_record_frame(AdaptiveFPS *adaptive, real32 frame_time_ms,
                          real32 target_frame_time_ms) {
  adaptive->frames_sampled++;
  adaptive->frames_since_last_change++;

  // Track recent frame times
  adaptive->recent_frame_times[adaptive->recent_frame_index] = frame_time_ms;
  adaptive->recent_frame_index = (adaptive->recent_frame_index + 1) % 10;

  // Check if frame missed (with tolerance)
  bool frame_missed = (frame_time_ms > (target_frame_time_ms + 3.0f));

  if (frame_missed) {
    adaptive->frames_missed++;
    adaptive->consecutive_good_frames = 0;
  } else {
    adaptive->consecutive_good_frames++;
  }
}

file_scoped_fn inline real32
adaptive_fps_get_recent_average(AdaptiveFPS *adaptive) {
  real32 sum = 0.0f;
  for (int i = 0; i < 10; i++) {
    sum += adaptive->recent_frame_times[i];
  }
  return sum / 10.0f;
}

file_scoped_fn inline bool
adaptive_fps_should_increase(AdaptiveFPS *adaptive, GameConfig *game_config,
                             PlatformConfig *platform_config,
                             real32 target_frame_time_ms) {
  // Quick recovery check
  if (adaptive->consecutive_good_frames >= 30 &&
      game_config->refresh_rate_hz < platform_config->monitor_refresh_hz &&
      adaptive->frames_since_last_change >= 90) {

    real32 avg_recent = adaptive_fps_get_recent_average(adaptive);
    real32 headroom_threshold = target_frame_time_ms * 0.80f;

    if (avg_recent < headroom_threshold) {
      return true;
    }
  }

  // Regular evaluation
  if (adaptive->frames_sampled >= adaptive->sample_window &&
      adaptive->frames_since_last_change >= adaptive->cooldown_frames) {

    real32 miss_rate =
        (real32)adaptive->frames_missed / (real32)adaptive->frames_sampled;

    if (miss_rate < adaptive->recover_threshold &&
        game_config->refresh_rate_hz < platform_config->monitor_refresh_hz) {
      return true;
    }
  }

  return false;
}

file_scoped_fn inline bool adaptive_fps_should_decrease(AdaptiveFPS *adaptive) {
  if (adaptive->frames_sampled >= adaptive->sample_window &&
      adaptive->frames_since_last_change >= adaptive->cooldown_frames) {

    real32 miss_rate =
        (real32)adaptive->frames_missed / (real32)adaptive->frames_sampled;

    if (miss_rate > adaptive->miss_threshold) {
      return true;
    }
  }

  return false;
}

file_scoped_fn inline uint32 adaptive_fps_get_next_higher(uint32 current_fps,
                                                          uint32 monitor_hz) {
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

file_scoped_fn inline uint32 adaptive_fps_get_next_lower(uint32 current_fps) {
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

file_scoped_fn inline void adaptive_fps_apply_change(AdaptiveFPS *adaptive,
                                                     GameConfig *game_config,
                                                     uint32 new_fps) {
  game_config->refresh_rate_hz = new_fps;
  game_config->target_seconds_per_frame = 1.0f / (real32)new_fps;

  adaptive->frames_since_last_change = 0;
  adaptive->frames_sampled = 0;
  adaptive->frames_missed = 0;
  adaptive->consecutive_good_frames = 0;

  g_fps = new_fps;
}

file_scoped_fn inline void adaptive_fps_reset_window(AdaptiveFPS *adaptive) {
  adaptive->frames_sampled = 0;
  adaptive->frames_missed = 0;
}

void adaptive_fps_update(AdaptiveFPS *adaptive, GameConfig *game_config,
                         PlatformConfig *platform_config,
                         real32 frame_time_ms) {
  real32 target_frame_time_ms = game_config->target_seconds_per_frame * 1000.0f;

  adaptive_fps_record_frame(adaptive, frame_time_ms, target_frame_time_ms);

  if (adaptive_fps_should_increase(adaptive, game_config, platform_config,
                                   target_frame_time_ms)) {
    uint32 old_fps = game_config->refresh_rate_hz;
    uint32 new_fps = adaptive_fps_get_next_higher(
        old_fps, platform_config->monitor_refresh_hz);

    if (new_fps != old_fps) {
      adaptive_fps_apply_change(adaptive, game_config, new_fps);
#if DE100_INTERNAL
      printf("✅ ADAPTIVE: %d → %d Hz\n", old_fps, new_fps);
#endif
    }
  } else if (adaptive_fps_should_decrease(adaptive)) {
    uint32 old_fps = game_config->refresh_rate_hz;
    uint32 new_fps = adaptive_fps_get_next_lower(old_fps);

    if (new_fps != old_fps) {
      adaptive_fps_apply_change(adaptive, game_config, new_fps);
      printf("⚠️  ADAPTIVE: %d → %d Hz\n", old_fps, new_fps);
    }
  } else if (adaptive->frames_sampled >= adaptive->sample_window) {
    adaptive_fps_reset_window(adaptive);
  }
}
