#include "engine.h"

#include "_common/file.h"
#include "_common/memory.h"
#include "_common/time.h"

// ═══════════════════════════════════════════════════════════════════════════
// ENGINE INIT (Common across all platforms)
// ═══════════════════════════════════════════════════════════════════════════

int engine_init(EngineState *engine) {
  EngineGameState *game = &engine->game;
  EnginePlatformState *platform = &engine->platform;
  EngineAllocations *allocations = &engine->allocations;

  g_initial_game_time = get_wall_clock();

  // ─────────────────────────────────────────────────────────────────────
  // ZERO INITIALIZE
  // ─────────────────────────────────────────────────────────────────────

  // *engine = (EngineState){0};
  game->input = &platform->inputs[0];
  platform->old_input = &platform->inputs[1];

  // ─────────────────────────────────────────────────────────────────────
  // LOAD GAME CODE
  // ─────────────────────────────────────────────────────────────────────

  platform->code_paths = (GameCodePaths){
      .game_main_lib_path = DE100_GAME_MAIN_SHARED_LIB_PATH,
      .game_main_lib_tmp_path = DE100_GAME_MAIN_TMP_SHARED_LIB_PATH,
      .game_startup_lib_path = DE100_GAME_STARTUP_SHARED_LIB_PATH,
      .game_startup_lib_tmp_path = DE100_GAME_STARTUP_TMP_SHARED_LIB_PATH,
      .game_init_lib_path = DE100_GAME_INIT_SHARED_LIB_PATH,
      .game_init_lib_tmp_path = DE100_GAME_INIT_TMP_SHARED_LIB_PATH,
  };

  load_game_code(&platform->code, &platform->code_paths,
                 GAME_CODE_CATEGORY_ANY);

  if (!platform->code.is_valid) {
    fprintf(stderr, "❌ Failed to load game code\n");
    return 1;
  }

  printf("✅ Game code loaded\n");

  // ─────────────────────────────────────────────────────────────────────
  // GET GAME CONFIG
  // ─────────────────────────────────────────────────────────────────────

  game->config = get_default_game_config();
  // #if DE100_HOT_RELOAD
  platform->code.startup(&game->config);
  // #else
  //   game_init(&game->config);
  // #endif
  g_fps = game->config.refresh_rate_hz;

  // ─────────────────────────────────────────────────────────────────────
  // ALLOCATE GAME STATE MEMORY
  // ─────────────────────────────────────────────────────────────────────

#if DE100_INTERNAL
  void *base_address = (void *)TERABYTES(2);
#else
  void *base_address = NULL;
#endif

  uint64 total_size =
      game->config.permanent_storage_size + game->config.transient_storage_size;

  allocations->game_state =
      de100_memory_alloc(base_address, total_size,
                         De100_MEMORY_FLAG_READ | De100_MEMORY_FLAG_WRITE |
                             De100_MEMORY_FLAG_ZEROED);

  if (!de100_memory_is_valid(allocations->game_state)) {
    fprintf(stderr, "❌ Failed to allocate game state\n");
    return 1;
  }

  game->memory.permanent_storage = allocations->game_state.base;
  game->memory.transient_storage = (uint8 *)allocations->game_state.base +
                                   game->config.permanent_storage_size;
  game->memory.permanent_storage_size = game->config.permanent_storage_size;
  game->memory.transient_storage_size = game->config.transient_storage_size;
  game->memory.is_initialized = false;

  platform->memory_state.total_size = total_size;
  platform->memory_state.game_memory = allocations->game_state.base;
  printf("✅ Game state: %lu MB\n", total_size / (1024 * 1024));

  // ─────────────────────────────────────────────────────────────────────
  // ALLOCATE BACKBUFFER
  // ─────────────────────────────────────────────────────────────────────

  int backbuffer_size =
      game->config.window_width * game->config.window_height * 4;

  allocations->backbuffer =
      de100_memory_alloc(NULL, backbuffer_size, De100_MEMORY_FLAG_RW_ZEROED);

  if (!de100_memory_is_valid(allocations->backbuffer)) {
    fprintf(stderr, "❌ Failed to allocate backbuffer\n");
    return 1;
  }

  game->backbuffer.memory = allocations->backbuffer.base;
  game->backbuffer.width = game->config.window_width;
  game->backbuffer.height = game->config.window_height;
  game->backbuffer.bytes_per_pixel = 4;
  game->backbuffer.pitch = game->config.window_width * 4;
  printf("✅ Backbuffer: %dx%d\n", game->backbuffer.width,
         game->backbuffer.height);

  // ─────────────────────────────────────────────────────────────────────
  // ALLOCATE AUDIO BUFFER
  // ─────────────────────────────────────────────────────────────────────

  int bytes_per_sample = sizeof(int16) * 2;
  // TODO: should it be on the `game->audio` instead?
  platform->config.audio.bytes_per_sample = bytes_per_sample;
  int samples_per_frame = game->config.initial_audio_sample_rate /
                          game->config.audio_game_update_hz;
  platform->config.audio.max_samples_per_call = samples_per_frame * 3;
  int audio_size =
      platform->config.audio.max_samples_per_call * bytes_per_sample;

  allocations->audio_samples =
      de100_memory_alloc(NULL, audio_size, De100_MEMORY_FLAG_RW_ZEROED);

  if (!de100_memory_is_valid(allocations->audio_samples)) {
    fprintf(stderr, "❌ Failed to allocate audio buffer\n");
    return 1;
  }

  game->audio.samples_per_second = game->config.initial_audio_sample_rate;
  game->audio.samples = allocations->audio_samples.base;

  platform->config.audio.buffer_size_bytes =
      game->config.initial_audio_sample_rate * bytes_per_sample;

  printf("✅ Audio buffer: %d samples max\n",
         platform->config.audio.max_samples_per_call);

  // ─────────────────────────────────────────────────────────────────────
  // RECORDING STATE
  // ─────────────────────────────────────────────────────────────────────

  platform->memory_state.recording_fd = -1;
  platform->memory_state.playback_fd = -1;
  platform->memory_state.input_recording_index = 0;
  platform->memory_state.input_playing_index = 0;

  printf("✅ Engine initialized\n");
  return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// ENGINE SHUTDOWN
// ═══════════════════════════════════════════════════════════════════════════

void engine_shutdown(EngineState *engine) {
  EnginePlatformState *platform = &engine->platform;
  EngineAllocations *allocations = &engine->allocations;

  printf("[SHUTDOWN] Engine cleanup...\n");

  // Clean up temp files
  de100_file_delete(platform->code_paths.game_main_lib_tmp_path);
  de100_file_delete(platform->code_paths.game_startup_lib_tmp_path);
  de100_file_delete(platform->code_paths.game_init_lib_tmp_path);

  // Free allocations
  if (de100_memory_is_valid(allocations->audio_samples)) {
    de100_memory_free(&allocations->audio_samples);
  }
  if (de100_memory_is_valid(allocations->backbuffer)) {
    de100_memory_free(&allocations->backbuffer);
  }
  if (de100_memory_is_valid(allocations->game_state)) {
    de100_memory_free(&allocations->game_state);
  }

  printf("[SHUTDOWN] Engine cleanup complete\n");
}