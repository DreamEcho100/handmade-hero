
#include "./startup.h"
#include "../../_common/time.h"
#include "../../game/audio.h"
#include "../../game/backbuffer.h"
#include "../../game/base.h"
#include "../../game/config.h"
#include "../../game/input.h"
#include "../../game/memory.h"

int platform_game_startup(GameConfig *game_config, GameMemory *memory,
                          GameInput *old_game_input, GameInput *new_game_input,
                          GameBackBuffer *buffer,
                          GameAudioOutputBuffer *audio_buffer) {
  (void)memory;
  (void)new_game_input;
  (void)old_game_input;
  (void)buffer;
  (void)audio_buffer;

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

  PlatformMemoryBlock permanent_storage = platform_allocate_memory(
      base_address, game_config->permanent_storage_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(permanent_storage)) {
    fprintf(stderr, "❌ Could not allocate permanent storage\n");
    fprintf(stderr, "   Error: %s\n", permanent_storage.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(permanent_storage.error_code));
    return 1;
  }

  printf("[%.3fs] Allocating transient storage (%lu GB)...\n",
         get_wall_clock() - g_initial_game_time,
         game_config->transient_storage_size / (1024 * 1024 * 1024));

  // Place transient storage right after permanent (contiguous memory)
  void *transient_base =
      (uint8 *)permanent_storage.base + permanent_storage.size;

  PlatformMemoryBlock transient_storage = platform_allocate_memory(
      transient_base, game_config->transient_storage_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(transient_storage)) {
    fprintf(stderr, "❌ Could not allocate transient storage\n");
    fprintf(stderr, "   Error: %s\n", transient_storage.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(transient_storage.error_code));
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
  PlatformMemoryBlock new_game_input_block = platform_allocate_memory(
      NULL, sizeof(GameInput),
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(new_game_input_block)) {
    fprintf(stderr, "❌ Could not allocate new_game_input\n");
    fprintf(stderr, "   Error: %s\n", new_game_input_block.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(new_game_input_block.error_code));
    return 1;
  }
  new_game_input = (GameInput *)new_game_input_block.base;

  PlatformMemoryBlock old_game_input_block = platform_allocate_memory(
      NULL, sizeof(GameInput),
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);
  if (!platform_memory_is_valid(old_game_input_block)) {
    fprintf(stderr, "❌ Could not allocate old_game_input\n");
    fprintf(stderr, "   Error: %s\n", old_game_input_block.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(old_game_input_block.error_code));
    return 1;
  }
  old_game_input = (GameInput *)old_game_input_block.base;

  // buffer = {0};
  PlatformMemoryBlock buffer_block = platform_allocate_memory(
      NULL, sizeof(GameBackBuffer),
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);
  if (!platform_memory_is_valid(buffer_block)) {
    fprintf(stderr, "❌ Could not allocate buffer\n");
    fprintf(stderr, "   Error: %s\n", buffer_block.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(buffer_block.error_code));
    return 1;
  }
  buffer = (GameBackBuffer *)buffer_block.base;

  PlatformMemoryBlock audio_output_block = platform_allocate_memory(
      NULL, sizeof(GameAudioOutputBuffer),
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);
  if (!platform_memory_is_valid(audio_output_block)) {
    fprintf(stderr, "❌ Could not allocate audio output\n");
    fprintf(stderr, "   Error: %s\n", audio_output_block.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(audio_output_block.error_code));
    return 1;
  }
  audio_buffer = (GameAudioOutputBuffer *)audio_output_block.base;

  printf("Success game startup\n");

  return 0;
}