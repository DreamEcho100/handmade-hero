#include "./config.h"

GameConfig get_default_game_config() {
  GameConfig config = {0};

  /* =========================
     MEMORY
     ========================= */

  config.permanent_storage_size = MEGABYTES(64);
  config.transient_storage_size = GIGABYTES(1);

  /* =========================
     GAME / BUILD FLAGS
     ========================= */

  config.game_flags = 0;

  /* =========================
     WINDOW / DISPLAY
     ========================= */

  config.window_width = 1280;
  config.window_height = 720;
  config.refresh_rate_hz = 60;

  config.prefer_vsync = true;
  config.prefer_borderless = false;
  config.prefer_resizable = true;
  config.prefer_adaptive_fps = false;

  /* =========================
     INPUT
     ========================= */

  config.max_controllers = 5;

  /* =========================
     AUDIO
     ========================= */

  config.initial_audio_sample_rate = 48000;
  config.audio_buffer_size_frames = 1024;
  config.audio_game_update_hz = 30;

  /* =========================
     TIMING
     ========================= */

  config.target_seconds_per_frame = 1.0f / (real32)config.refresh_rate_hz;

  return config;
}