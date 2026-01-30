
#include "./startup.h"
#include "../../_common/file.h"
#include "../../_common/time.h"
#include "../../game/audio.h"
#include "../../game/backbuffer.h"
#include "../../game/base.h"
#include "../../game/config.h"
#include "../../game/input.h"
#include "../../game/memory.h"

int platform_game_startup(LoadGameCodeConfig *load_game_code_config,
                          GameCode *game, GameConfig *game_config,
                          GameMemory *memory, GameInput *old_game_input,
                          GameInput *new_game_input, GameBackBuffer *buffer,
                          GameAudioOutputBuffer *audio_buffer) {
  (void)new_game_input;
  (void)old_game_input;
  (void)audio_buffer;

  g_initial_game_time = get_wall_clock();
  printf("[%.3fs] Starting platform_main\n",
         get_wall_clock() - g_initial_game_time);

  load_game_code_config->game_main_lib_path = DE100_GAME_MAIN_SHARED_LIB_PATH;
  load_game_code_config->game_main_lib_tmp_path =
      DE100_GAME_MAIN_TMP_SHARED_LIB_PATH;
  load_game_code_config->game_startup_lib_path =
      DE100_GAME_STARTUP_SHARED_LIB_PATH;
  load_game_code_config->game_startup_lib_tmp_path =
      DE100_GAME_STARTUP_TMP_SHARED_LIB_PATH;
  load_game_code_config->game_init_lib_path = DE100_GAME_INIT_SHARED_LIB_PATH;
  load_game_code_config->game_init_lib_tmp_path =
      DE100_GAME_INIT_TMP_SHARED_LIB_PATH;

  load_game_code(game, load_game_code_config, GAME_CODE_CATEGORY_ANY);
  // exit(1);
  if (game->is_valid) {
    printf("✅ Game code loaded successfully\n");
    // NOTE: do on a separate thread
    file_delete(
        load_game_code_config->game_main_lib_tmp_path); // Clean up temp file
    file_delete(
        load_game_code_config->game_startup_lib_tmp_path); // Clean up temp file
    file_delete(
        load_game_code_config->game_init_lib_tmp_path); // Clean up temp file
  } else {
    printf("❌ Failed to load game code, using stubs\n");
  }

  game->startup(game_config);
  g_fps = game_config->refresh_rate_hz;

  // ═══════════════════════════════════════════════════════════════
  // 💾 ALLOCATE GAME MEMORY (Casey's Day 4 pattern)
  // ═══════════════════════════════════════════════════════════════
  // Two memory pools:
  // 1. Permanent (64MB) - game state, assets, never freed
  // 2. Transient (4GB) - per-frame scratch memory, level data
  // ═══════════════════════════════════════════════════════════════

#if DE100_INTERNAL
  // Debug: Reserve address space at 2TB mark (makes debugging easier)
  void *base_address = (void *)TERABYTES(2);
#else
  void *base_address = NULL; // Release: Let OS choose address
#endif

  printf("[%.3fs] Allocating permanent storage (%lu MB)...\n",
         get_wall_clock() - g_initial_game_time,
         game_config->permanent_storage_size / (1024 * 1024));

  MemoryBlock permanent_storage =
      memory_alloc(base_address, game_config->permanent_storage_size,
                   MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_ZEROED);

  if (!memory_is_valid(permanent_storage)) {
    fprintf(stderr, "❌ Could not allocate permanent storage\n");
    fprintf(stderr, "   Code: %s\n",
            memory_error_str(permanent_storage.error_code));
    return 1;
  }

  printf("[%.3fs] Allocating transient storage (%lu GB)...\n",
         get_wall_clock() - g_initial_game_time,
         game_config->transient_storage_size / (1024 * 1024 * 1024));

  // Place transient storage right after permanent (contiguous memory)
  void *transient_base =
      (uint8 *)permanent_storage.base + permanent_storage.size;

  MemoryBlock transient_storage =
      memory_alloc(transient_base, game_config->transient_storage_size,
                   MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_ZEROED);

  if (!memory_is_valid(transient_storage)) {
    fprintf(stderr, "❌ Could not allocate transient storage\n");
    fprintf(stderr, "   Code: %s\n",
            memory_error_str(transient_storage.error_code));
    return 1;
  }

  // Package memory blocks for game code
  // GameMemory memory = {0};
  memory->permanent_storage = permanent_storage;
  memory->transient_storage = transient_storage;
  memory->permanent_storage_size = permanent_storage.size;
  memory->transient_storage_size = transient_storage.size;

  printf("✅ Game memory allocated:\n");
  printf("   Permanent: %lu MB at %p\n",
         memory->permanent_storage.size / (1024 * 1024),
         memory->permanent_storage.base);
  printf("   Transient: %lu GB at %p\n",
         memory->transient_storage.size / (1024 * 1024 * 1024),
         memory->transient_storage.base);

  // ═══════════════════════════════════════════════════════════════
  // 🎮 INITIALIZE INPUT (Casey's Day 6 pattern)
  // ═══════════════════════════════════════════════════════════════
  // Double-buffered input: new_input (current) and old_input (previous)
  // We swap pointers each frame to preserve button press/release state
  // ═══════════════════════════════════════════════════════════════

  // GameInput *game_inputs[2];
  // = {0};
  MemoryBlock new_game_input_block =
      memory_alloc(NULL, sizeof(GameInput),
                   MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_ZEROED);

  if (!memory_is_valid(new_game_input_block)) {
    fprintf(stderr, "❌ Could not allocate new_game_input\n");
    fprintf(stderr, "   Code: %s\n",
            memory_error_str(new_game_input_block.error_code));
    return 1;
  }
  new_game_input = (GameInput *)new_game_input_block.base;

  MemoryBlock old_game_input_block =
      memory_alloc(NULL, sizeof(GameInput),
                   MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_ZEROED);
  if (!memory_is_valid(old_game_input_block)) {
    fprintf(stderr, "❌ Could not allocate old_game_input\n");
    fprintf(stderr, "   Code: %s\n",
            memory_error_str(old_game_input_block.error_code));
    return 1;
  }
  old_game_input = (GameInput *)old_game_input_block.base;

  MemoryBlock buffer_block =
      memory_alloc(NULL, sizeof(GameBackBuffer),
                   MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_ZEROED);
  if (!memory_is_valid(buffer_block)) {
    fprintf(stderr, "❌ Could not allocate buffer\n");
    fprintf(stderr, "   Code: %s\n", memory_error_str(buffer_block.error_code));
    return 1;
  }
  buffer->memory = (MemoryBlock)buffer_block;

  MemoryBlock audio_output_block =
      memory_alloc(NULL, sizeof(GameAudioOutputBuffer),
                   MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_ZEROED);
  if (!memory_is_valid(audio_output_block)) {
    fprintf(stderr, "❌ Could not allocate audio output\n");
    fprintf(stderr, "   Code: %s\n",
            memory_error_str(audio_output_block.error_code));
    return 1;
  }
  audio_buffer = (GameAudioOutputBuffer *)audio_output_block.base;

  printf("Success game startup\n");

  return 0;
}

int free_platform_game_startup(LoadGameCodeConfig *load_game_code_config,
                               GameCode *game, GameConfig *game_config,
                               GameMemory *memory, GameInput *old_game_input,
                               GameInput *new_game_input,
                               GameBackBuffer *buffer,
                               GameAudioOutputBuffer *audio_buffer) {
  (void)game;
  (void)game_config;
  (void)new_game_input;
  (void)old_game_input;
  (void)audio_buffer;

  file_delete(load_game_code_config->game_main_lib_tmp_path);
  file_delete(load_game_code_config->game_startup_lib_tmp_path);
  file_delete(load_game_code_config->game_init_lib_tmp_path);
  memory_free(&buffer->memory);
  memory_free(&memory->transient_storage);
  memory_free(&memory->permanent_storage);

  // memory_free(new_game_input);
  // memory_free(old_game_input);
  // memory_free(audio_buffer);

  return 0;
}