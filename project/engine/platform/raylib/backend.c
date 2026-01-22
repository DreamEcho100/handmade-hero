#include "backend.h"

#include "../../_common/base.h"
#include "../../game/backbuffer.h"
#include "../../game/base.h"
#include "../../game/game-loader.h"
#include "../../game/input.h"
#include "../../game/memory.h"
#include "../_common/audio.h"
#include "audio.h"
#include "inputs/joystick.h"
#include "inputs/keyboard.h"

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if HANDMADE_INTERNAL
#include "../../_common/debug.h"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// 🎨 RAYLIB-SPECIFIC TYPES
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  Texture2D texture;
  bool has_texture;
} OffscreenBufferMeta;

file_scoped_global_var OffscreenBufferMeta g_game_buffer_meta = {0};

// ═══════════════════════════════════════════════════════════════════════════
// 🖼️ BACKBUFFER MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

file_scoped_fn void resize_back_buffer(GameOffscreenBuffer *backbuffer,
                                       OffscreenBufferMeta *backbuffer_meta,
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

  // Free old memory
  if (backbuffer->memory.base && old_width > 0 && old_height > 0) {
    platform_free_memory(&backbuffer->memory);
  }

  // Free old texture
  if (backbuffer_meta->has_texture) {
    UnloadTexture(backbuffer_meta->texture);
    backbuffer_meta->has_texture = false;
  }

  // Allocate new memory
  int buffer_size = width * height * backbuffer->bytes_per_pixel;
  PlatformMemoryBlock backbuffer_memory = platform_allocate_memory(
      NULL, buffer_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(backbuffer_memory)) {
    fprintf(stderr, "❌ Failed to allocate backbuffer: %s\n",
            backbuffer_memory.error_message);
    fprintf(stderr, "   Error: %s\n", backbuffer_memory.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(backbuffer_memory.error_code));
    return;
  }

  backbuffer->memory = backbuffer_memory;

  // Create Raylib texture
  Image img = {.data = backbuffer->memory.base,
               .width = backbuffer->width,
               .height = backbuffer->height,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               .mipmaps = 1};

  backbuffer_meta->texture = LoadTextureFromImage(img);
  backbuffer_meta->has_texture = true;

  printf("✅ Raylib texture created successfully\n");
}

file_scoped_fn void
update_window_from_backbuffer(GameOffscreenBuffer *backbuffer,
                              OffscreenBufferMeta *backbuffer_meta) {

  if (!backbuffer_meta->has_texture || !backbuffer->memory.base) {
    return;
  }

  UpdateTexture(backbuffer_meta->texture, backbuffer->memory.base);
  DrawTexture(backbuffer_meta->texture, 0, 0, WHITE);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🚀 MAIN PLATFORM ENTRY POINT
// ═══════════════════════════════════════════════════════════════════════════

int platform_main() {
  real64 t_start = GetTime();
  printf("[%.3fs] Starting platform_main\n", GetTime() - t_start);
  fflush(stdout);

  // ═══════════════════════════════════════════════════════════════════
  // 💾 ALLOCATE GAME MEMORY
  // ═══════════════════════════════════════════════════════════════════

#if HANDMADE_INTERNAL
  void *base_address = (void *)TERABYTES(2);
#else
  void *base_address = NULL;
#endif

  uint64_t permanent_storage_size = MEGABYTES(64);
  uint64_t transient_storage_size = GIGABYTES(1);

  PlatformMemoryBlock permanent_storage = platform_allocate_memory(
      base_address, permanent_storage_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(permanent_storage)) {
    fprintf(stderr, "❌ ERROR: Could not allocate permanent storage\n");
    platform_free_memory(&permanent_storage);
    fprintf(stderr, "   Error: %s\n", permanent_storage.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(permanent_storage.error_code));
    return 1;
  }

  void *transient_base =
      (uint8_t *)permanent_storage.base + permanent_storage.size;
  PlatformMemoryBlock transient_storage = platform_allocate_memory(
      transient_base, transient_storage_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(transient_storage)) {
    fprintf(stderr, "❌ ERROR: Could not allocate transient storage\n");
    platform_free_memory(&permanent_storage);
    fprintf(stderr, "   Error: %s\n", transient_storage.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(transient_storage.error_code));
    return 1;
  }

  GameMemory game_memory = {.permanent_storage = permanent_storage,
                            .transient_storage = transient_storage,
                            .permanent_storage_size = permanent_storage.size,
                            .transient_storage_size = transient_storage.size};

  printf("✅ Game memory allocated\n");

  // ═══════════════════════════════════════════════════════════════════
  // 🎮 INITIALIZE INPUT BUFFERS
  // ═══════════════════════════════════════════════════════════════════

  static GameInput game_inputs[2] = {0};
  GameInput *new_game_input = &game_inputs[0];
  GameInput *old_game_input = &game_inputs[1];

  // ═══════════════════════════════════════════════════════════════════
  // 🪟 CREATE WINDOW
  // ═══════════════════════════════════════════════════════════════════

  InitWindow(1280, 720, "Handmade Hero");
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetExitKey(KEY_NULL);
  int32_t target_fps = 60;
  SetTargetFPS(target_fps);

#if HANDMADE_INTERNAL
  g_frame_log_counter = 0;
  g_debug_fps = target_fps;
#endif

  printf("✅ Window created\n");

  // ═══════════════════════════════════════════════════════════════════
  // 🎮 INITIALIZE GAMEPAD
  // ═══════════════════════════════════════════════════════════════════

  raylib_init_gamepad(old_game_input->controllers, new_game_input->controllers);

  // ═══════════════════════════════════════════════════════════════════
  // 🔊 INITIALIZE AUDIO (MIRRORS X11 PATTERN)
  // ═══════════════════════════════════════════════════════════════════

  GameAudioOutputBuffer game_audio_output = {0};
  PlatformAudioConfig platform_audio_config = {0};

  // Audio parameters (same as X11)
  int32_t samples_per_second = 48000;
  int32_t buffer_size_frames = 4096; // ~85ms at 48kHz
  int32_t game_update_hz = 30;

  bool audio_initialized =
      raylib_init_audio(&game_audio_output, &platform_audio_config,
                        samples_per_second, buffer_size_frames, game_update_hz);

  if (!audio_initialized) {
    fprintf(stderr,
            "⚠️  Audio failed to initialize, continuing without sound\n");
  }

  // ═══════════════════════════════════════════════════════════════════
  // 🖼️ INITIALIZE BACKBUFFER
  // ═══════════════════════════════════════════════════════════════════

  GameOffscreenBuffer game_buffer = {0};
  int init_backbuffer_status = init_backbuffer(&game_buffer, 1280, 720, 4);
  if (init_backbuffer_status != 0) {
    fprintf(stderr, "❌ Failed to initialize backbuffer\n");
    return init_backbuffer_status;
  }

  resize_back_buffer(&game_buffer, &g_game_buffer_meta, game_buffer.width,
                     game_buffer.height);

  // ═══════════════════════════════════════════════════════════════════
  // 🎮 LOAD GAME CODE
  // ═══════════════════════════════════════════════════════════════════

  char *game_so_path = "build/libgame.so";
  char *game_temp_so_path = "build/libgame_temp.so";
  GameCode game = load_game_code(game_so_path, game_temp_so_path);

  printf("✅ Entering main loop...\n");

  // ═══════════════════════════════════════════════════════════════════
  // 🔁 MAIN GAME LOOP
  // ═══════════════════════════════════════════════════════════════════

  while (!WindowShouldClose() && is_game_running) {

    // ─────────────────────────────────────────────────────────────
    // INPUT: Prepare frame
    // ─────────────────────────────────────────────────────────────
    prepare_input_frame(old_game_input, new_game_input);

    // ─────────────────────────────────────────────────────────────
    // HANDLE WINDOW RESIZE
    // ─────────────────────────────────────────────────────────────
    if (IsWindowResized()) {
      resize_back_buffer(&game_buffer, &g_game_buffer_meta, GetScreenWidth(),
                         GetScreenHeight());
    }

    // ─────────────────────────────────────────────────────────────
    // INPUT: Keyboard
    // ─────────────────────────────────────────────────────────────
    handle_keyboard_inputs(&platform_audio_config, new_game_input);

    // ─────────────────────────────────────────────────────────────
    // INPUT: Gamepad
    // ─────────────────────────────────────────────────────────────
    raylib_poll_gamepad(new_game_input);

    // ─────────────────────────────────────────────────────────────
    // SKIP UPDATES IF PAUSED
    // ─────────────────────────────────────────────────────────────
    if (g_game_is_paused) {
      BeginDrawing();
      ClearBackground(BLACK);
      update_window_from_backbuffer(&game_buffer, &g_game_buffer_meta);
      DrawText("PAUSED", 10, 10, 40, RED);
      EndDrawing();

      GameInput *temp = new_game_input;
      new_game_input = old_game_input;
      old_game_input = temp;
      continue;
    }

    // ─────────────────────────────────────────────────────────────
    // GAME UPDATE + RENDER
    // ─────────────────────────────────────────────────────────────
    if (game_buffer.memory.base) {
      game.update_and_render(&game_memory, new_game_input, &game_buffer);
    }

    // ─────────────────────────────────────────────────────────────
    // AUDIO: Only generate when Raylib needs more data
    // ─────────────────────────────────────────────────────────────
    if (platform_audio_config.is_initialized) {
      // Check if Raylib's buffer is ready for more samples
      int32_t samples_to_write = raylib_get_samples_to_write(
          &platform_audio_config, &game_audio_output);

      if (samples_to_write > 0) {
        // Configure buffer for game
        game_audio_output.sample_count = samples_to_write;

        // Game generates samples
        game.get_audio_samples(&game_memory, &game_audio_output);

        // Platform sends to hardware
        raylib_send_samples(&platform_audio_config, &game_audio_output);
      }
      // If samples_to_write == 0, Raylib's buffer is still full
      // This is normal - we don't write every frame!
    }

    // ─────────────────────────────────────────────────────────────
    // DISPLAY
    // ─────────────────────────────────────────────────────────────
    BeginDrawing();
    ClearBackground(BLACK);
    update_window_from_backbuffer(&game_buffer, &g_game_buffer_meta);
    EndDrawing();

#if HANDMADE_INTERNAL
    g_frame_log_counter++;
#endif

    // ─────────────────────────────────────────────────────────────
    // SWAP INPUT BUFFERS
    // ─────────────────────────────────────────────────────────────
    GameInput *temp = new_game_input;
    new_game_input = old_game_input;
    old_game_input = temp;

    // ─────────────────────────────────────────────────────────────
    // DEBUG: FPS logging
    // ─────────────────────────────────────────────────────────────
#if HANDMADE_INTERNAL
    if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
      printf("[Raylib] %.2fms/f, %.0ff/s\n", GetFrameTime() * 1000.0f,
             (float)GetFPS());
    }
#endif
  }

#if HANDMADE_INTERNAL
  if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
    raylib_debug_audio_overlay();
  }
#endif

  // ═══════════════════════════════════════════════════════════════════
  // 🧹 CLEANUP
  // ═══════════════════════════════════════════════════════════════════

#if HANDMADE_SANITIZE_WAVE_1_MEMORY
  printf("Cleaning up...\n");

  if (g_game_buffer_meta.has_texture) {
    UnloadTexture(g_game_buffer_meta.texture);
  }

  if (game_buffer.memory.base) {
    platform_free_memory(&game_buffer.memory);
  }

  raylib_shutdown_audio(&game_audio_output, &platform_audio_config);

  platform_free_memory(&transient_storage);
  platform_free_memory(&permanent_storage);

  CloseWindow();
  printf("✅ Cleanup complete\n");
#endif

  printf("Goodbye!\n");
  return 0;
}
