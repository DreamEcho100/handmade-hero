#include "startup.h"

#if DE100_INTERNAL
#include "../../engine/_common/debug-file-io.h"
#endif

GAME_STARTUP(game_startup) {
  game_config->permanent_storage_size = MEGABYTES(64);
  game_config->transient_storage_size = GIGABYTES(1);
  // X11 - Unused yet
  game_config->game_flags = 0;
  game_config->window_width = 1280;
  game_config->window_height = 720;
  game_config->refresh_rate_hz = FPS_60;
  // X11 - Unused yet
  game_config->prefer_vsync = true;
  // X11 - Unused yet
  game_config->prefer_borderless = false;
  // X11 - Unused yet
  game_config->prefer_resizable = true;
  // X11 - Unused yet
  game_config->prefer_adaptive_fps = false;
  // X11 - Unused yet
  game_config->max_controllers = 5;
  game_config->initial_audio_sample_rate = 48000;
  // X11 - Unused yet
  game_config->audio_buffer_size_frames = 1024;
  game_config->audio_game_update_hz = FPS_30;
  game_config->refresh_rate_hz = FPS_60;
  game_config->target_seconds_per_frame =
      1.0f / (real32)game_config->refresh_rate_hz;

  return 0;
}
