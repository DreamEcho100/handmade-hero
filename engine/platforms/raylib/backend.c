#include "../_common/backend.h"
#include "../../_common/base.h"
#include "../../engine.h"
#include "../../game/backbuffer.h"
#include "../../game/base.h"
#include "../../game/game-loader.h"
#include "../../game/inputs.h"
#include "../_common/adaptive-fps.h"
#include "../_common/inputs-recording.h"
#include "./audio.h"
#include "./hooks/inputs/joystick.h"
#include "./hooks/inputs/keyboard.h"
#include "./inputs/mouse.h"

#include <raylib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#if DE100_INTERNAL
#include "../_common/frame-stats.h"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// State
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  Texture2D texture;
  bool has_texture;
} BackBufferMeta;

de100_file_scoped_global_var BackBufferMeta g_game_buffer_meta = {0};
de100_file_scoped_global_var bool g_was_focused = true;

// ═══════════════════════════════════════════════════════════════════════════
// Backbuffer Management
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline void resize_back_buffer(GameBackBuffer *backbuffer,
                                                    int width, int height) {
  printf("Resizing backbuffer → %dx%d\n", width, height);

  if (width <= 0 || height <= 0) {
    printf("⚠️  Rejected resize: invalid size\n");
    return;
  }

  int old_width = backbuffer->width;
  int old_height = backbuffer->height;

  backbuffer->width = width;
  backbuffer->height = height;
  backbuffer->pitch = backbuffer->width * backbuffer->bytes_per_pixel;

  if (de100_memory_is_valid(backbuffer->memory) && old_width > 0 &&
      old_height > 0) {
    int buffer_size = width * height * backbuffer->bytes_per_pixel;
    de100_memory_realloc(&backbuffer->memory, buffer_size, 1);
  }

  if (g_game_buffer_meta.has_texture) {
    UnloadTexture(g_game_buffer_meta.texture);
    g_game_buffer_meta.has_texture = false;
  }

  Image img = {.data = backbuffer->memory.base,
               .width = backbuffer->width,
               .height = backbuffer->height,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               .mipmaps = 1};

  g_game_buffer_meta.texture = LoadTextureFromImage(img);
  g_game_buffer_meta.has_texture = true;

  printf("✅ Raylib texture created successfully\n");
}

de100_file_scoped_fn inline void
update_window_from_backbuffer(GameBackBuffer *backbuffer) {
  if (!g_game_buffer_meta.has_texture ||
      !de100_memory_is_valid(backbuffer->memory)) {
    return;
  }

  int window_width = GetScreenWidth();
  int window_height = GetScreenHeight();

  int offset_x = (window_width - backbuffer->width) / 2;
  int offset_y = (window_height - backbuffer->height) / 2;

  UpdateTexture(g_game_buffer_meta.texture, backbuffer->memory.base);
  DrawTexture(g_game_buffer_meta.texture, offset_x, offset_y, WHITE);
}

// ═══════════════════════════════════════════════════════════════════════════
// Audio
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline void
audio_generate_and_send(EngineGameState *game, GameMainCode *game_main_code) {
  u32 samples_to_generate = raylib_get_samples_to_write(&game->audio);
#if DE100_INTERNAL
  if (FRAME_LOG_EVERY_ONE_SECONDS_CHECK) {
    raylib_debug_audio_latency(&game->audio);
  }
#endif

  if (samples_to_generate == 0)
    return;

  if (samples_to_generate > (u32)game->audio.max_sample_count) {
    samples_to_generate = (u32)game->audio.max_sample_count;
  }
  // Cap per-frame work to prevent cascade: if the ring drained too much,
  // recover over multiple frames instead of generating a huge batch at once
  // (which would make the mixer even slower and cause more underruns).
  u32 max_per_frame = g_raylib_audio_output.latency_frames; // ~2 game frames
  if (samples_to_generate > max_per_frame) {
    samples_to_generate = max_per_frame;
  }

  game->audio.sample_count = (i32)samples_to_generate;
  game_main_code->functions.get_audio_samples(&game->memory, &game->audio);
  raylib_send_samples(&game->audio);
}

// ═══════════════════════════════════════════════════════════════════════════
// Focus handling (pause/resume audio)
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline void handle_focus_change(void) {
  bool is_focused = IsWindowFocused();
  if (is_focused != g_was_focused) {
    if (is_focused) {
      printf("Window gained focus\n");
      raylib_resume_audio();
    } else {
      printf("Window lost focus\n");
      raylib_pause_audio();
    }
    g_was_focused = is_focused;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Initialization
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline int raylib_init(EngineState *engine) {
  InitWindow(engine->game.config.window_width,
             engine->game.config.window_height,
             engine->game.config.window_title);
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetExitKey(KEY_NULL);
  SetTargetFPS(engine->game.config.target_refresh_rate_hz);

  printf("✅ Window created\n");

  raylib_game_initpad(engine->platform.old_inputs->controllers,
                      engine->game.inputs->controllers);

  bool audio_initialized = raylib_init_audio(
      &engine->game.audio, engine->game.config.initial_audio_sample_rate,
      engine->game.config.audio_game_update_hz);

  if (!audio_initialized) {
    fprintf(stderr,
            "⚠️  Audio failed to initialize, continuing without sound\n");
    return 1;
  }
  resize_back_buffer(&engine->game.backbuffer, engine->game.backbuffer.width,
                     engine->game.backbuffer.height);

  return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Main Loop
// ═══════════════════════════════════════════════════════════════════════════

int platform_main(void) {
  EngineState engine = {0};
  engine.platform.game_main_code = (GameMainCode){0};

  if (engine_init(&engine)) {
    return 1;
  }

  if (raylib_init(&engine) != 0) {
    engine_shutdown(&engine);
    return 1;
  }
  engine.platform.game_bootstrap_code.functions.init(
      &engine.game.thread_context, &engine.game.memory, engine.game.inputs,
      &engine.game.backbuffer);

  printf("✅ Entering main loop...\n");

  while (!WindowShouldClose() && is_game_running) {
    // Hot-reload: clear audio buffer on reload to prevent glitches
    bool reloaded = handle_game_reload_check(&engine.platform.game_main_code,
                                             &engine.platform.paths);
    if (reloaded) {
      raylib_clear_audio_buffer(&engine.game.audio);
    }

    handle_focus_change();

    prepare_input_frame(engine.platform.old_inputs, engine.game.inputs);

    handle_keyboard_inputs(&engine.platform, &engine.game);
    raylib_poll_gamepad(engine.game.inputs);
    raylib_poll_mouse(engine.game.inputs);

    if (input_recording_is_recording(&engine.platform.memory_state)) {
      input_recording_record_frame(&engine.platform.memory_state,
                                   engine.game.inputs);
    }

    if (input_recording_is_playing(&engine.platform.memory_state)) {
      input_recording_playback_frame(&engine.platform.memory_state,
                                     engine.game.inputs);
    }

    engine.platform.game_main_code.functions.update_and_render(
        &engine.game.thread_context, &engine.game.memory, engine.game.inputs,
        &engine.game.backbuffer);

    audio_generate_and_send(&engine.game, &engine.platform.game_main_code);

    BeginDrawing();
    ClearBackground(BLACK);
    update_window_from_backbuffer(&engine.game.backbuffer);
    EndDrawing();

    f32 frame_time_ms = GetFrameTime() * 1000.0f;
    f32 target_frame_time_ms =
        engine.game.config.target_seconds_per_frame * 1000.0f;

    if (frame_time_ms > (target_frame_time_ms + 5.0f)) {
      printf("⚠️  MISSED FRAME! %.2fms (target: %.2fms, over by: %.2fms)\n",
             frame_time_ms, target_frame_time_ms,
             frame_time_ms - target_frame_time_ms);
    }

#if DE100_INTERNAL
    frame_stats_record(frame_time_ms,
                       engine.game.config.target_seconds_per_frame);
#endif

    g_frame_counter++;

#if DE100_INTERNAL
    if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
      printf("[Raylib] %.2fms/f, %.2df/s (GetFrameTime: %.2fms)\n",
             frame_time_ms, GetFPS(), GetFrameTime() * 1000.0f);
    }
#endif

    if (engine.game.config.prefer_adaptive_fps) {
      adaptive_fps_update(&engine.game.config, frame_time_ms);
    }

    engine_swap_inputs(&engine);
  }

  // ═══════════════════════════════════════════════════════════════════
  // Cleanup
  // ═══════════════════════════════════════════════════════════════════

  printf("[%.3fs] Exiting, freeing memory...\n",
         de100_get_wall_clock() - g_initial_game_time_ms);
#if DE100_SANITIZE_WAVE_1_MEMORY

  if (g_game_buffer_meta.has_texture) {
    UnloadTexture(g_game_buffer_meta.texture);
  }
  raylib_shutdown_audio(&engine.game.audio);
  CloseWindow();

#endif
  engine_shutdown(&engine);

  printf("✅ Cleanup complete\n");
  printf("[%.3fs] Memory freed\n",
         de100_get_wall_clock() - g_initial_game_time_ms);

#if DE100_INTERNAL
  frame_stats_print();
#endif

  printf("Goodbye!\n");
  return 0;
}
