#include "engine.h"
#include "./platforms/_common/hooks/utils.h"

#include "_common/memory.h"
#include "_common/path.h"
#include "_common/time.h"
#include "game/base.h"
#include "game/game-loader.h"
#include "platforms/_common/replay-buffer.h"

// ═══════════════════════════════════════════════════════════════════════════
// ENGINE INIT (Common across all platforms)
// ═══════════════════════════════════════════════════════════════════════════

int engine_init(EngineState *engine) {
  EngineGameState *game = &engine->game;
  EnginePlatformState *platform = &engine->platform;
  EngineAllocations *allocations = &engine->allocations;

  g_initial_game_time_ms = de100_get_wall_clock();

  // ─────────────────────────────────────────────────────────────────────
  // ZERO INITIALIZE
  // ─────────────────────────────────────────────────────────────────────

  *engine = (EngineState){0};
  game->inputs = &platform->inputs[0];
  platform->old_inputs = &platform->inputs[1];

  // ─────────────────────────────────────────────────────────────────────
  // LOAD GAME CODE
  // ─────────────────────────────────────────────────────────────────────

  De100PathResult exe_full_path = de100_path_get_executable();
  if (!exe_full_path.success) {
    fprintf(stderr, "❌ Failed to get executable directory: %s\n",
            de100_path_strerror(exe_full_path.error_code));
    return 1;
  }

  printf("✅ Executable path: %s\n", exe_full_path.path);

  De100PathResult exe_directory = de100_path_get_executable_directory();
  if (!exe_directory.success) {
    fprintf(stderr, "❌ Failed to get executable directory: %s\n",
            de100_path_strerror(exe_directory.error_code));
    return 1;
  }

  printf("✅ Executable directory: %s\n", exe_directory.path);
  platform->paths = (GameCodePaths){
      .game_main_lib_path = DE100_GAME_MAIN_SHARED_LIB_PATH,
      .game_main_lib_tmp_path = DE100_GAME_MAIN_TMP_SHARED_LIB_PATH,
      .game_bootstrap_lib_path = DE100_GAME_BOOTSTRAP_SHARED_LIB_PATH,
      .game_bootstrap_lib_tmp_path = DE100_GAME_BOOTSTRAP_TMP_SHARED_LIB_PATH,
      .exe_full_path = de100_path_get_executable(),
      .exe_directory = de100_path_get_executable_directory(),
  };

  load_game_bootstrap_code(&platform->game_bootstrap_code, &platform->paths);

  load_game_main_code(&platform->game_main_code, &platform->paths);
  if (!platform->game_main_code.is_valid) {
    fprintf(stderr, "❌ Failed to load game code\n");
    return 1;
  }

  printf("✅ Game code loaded\n");

  // ─────────────────────────────────────────────────────────────────────
  // GET GAME CONFIG
  // ─────────────────────────────────────────────────────────────────────

  game->config = get_default_game_config();
  // #if DE100_HOT_RELOAD
  platform->game_bootstrap_code.functions.startup(&game->config);
  // #else
  // game_startup(&game->config);
  // #endif

  u32 max_allowed_refresh_rate_hz =
      game->config.max_allowed_refresh_rate_hz != 0
          ? game->config.max_allowed_refresh_rate_hz
          : game->config.target_refresh_rate_hz;

  if (max_allowed_refresh_rate_hz == 0) {
    max_allowed_refresh_rate_hz = DE100_DEFAULT_TARGET_FPS;
  }

  if (game->config.target_refresh_rate_hz == 0) {
    game->config.target_refresh_rate_hz = max_allowed_refresh_rate_hz;
  }

  de100_set_target_fps(max_allowed_refresh_rate_hz);
  g_frame_counter = 0;

  // ─────────────────────────────────────────────────────────────────────
  // ALLOCATE GAME STATE MEMORY
  // ─────────────────────────────────────────────────────────────────────

#if DE100_INTERNAL
  void *base_address = (void *)TERABYTES(2);
#else
  void *base_address = NULL;
#endif

  u64 total_size =
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
  game->memory.transient_storage =
      (u8 *)allocations->game_state.base + game->config.permanent_storage_size;
  game->memory.permanent_storage_size = game->config.permanent_storage_size;
  game->memory.transient_storage_size = game->config.transient_storage_size;
  game->memory.is_initialized = false;

  platform->memory_state.total_size = total_size;
  platform->memory_state.game_memory = allocations->game_state.base;
  printf("✅ Game state: %lu MB\n", total_size / (1024 * 1024));

  // ─────────────────────────────────────────────────────────────────────
  // INITIALIZE REPLAY BUFFERS
  // ─────────────────────────────────────────────────────────────────────

  ReplayBufferInitResult replay_result = replay_buffers_init(
      platform->paths.exe_directory.path, platform->memory_state.game_memory,
      platform->memory_state.total_size, platform->memory_state.replay_buffers);

  if (!replay_result.success) {
    fprintf(stderr, "⚠️  Replay buffers failed to initialize: %s\n",
            replay_buffer_strerror(replay_result.error_code));
    fprintf(stderr, "   Input recording/playback will not work.\n");
    // Don't fail engine init - this is a debug feature
  } else {
    printf("✅ Replay buffers: %d/%d initialized\n",
           replay_result.buffers_initialized, MAX_REPLAY_BUFFERS);
  }

  // ─────────────────────────────────────────────────────────────────────
  // ALLOCATE BACKBUFFER
  // ─────────────────────────────────────────────────────────────────────

  int backbuffer_size =
      game->config.window_width * game->config.window_height * 4;

  game->backbuffer.memory =
      de100_memory_alloc(NULL, backbuffer_size, De100_MEMORY_FLAG_RW_ZEROED);

  if (!de100_memory_is_valid(game->backbuffer.memory)) {
    fprintf(stderr, "❌ Failed to allocate backbuffer\n");
    return 1;
  }

  game->backbuffer.width = game->config.window_width;
  game->backbuffer.height = game->config.window_height;
  game->backbuffer.bytes_per_pixel = 4;
  game->backbuffer.pitch = game->config.window_width * 4;
  printf("✅ Backbuffer: %dx%d\n", game->backbuffer.width,
         game->backbuffer.height);

  // ─────────────────────────────────────────────────────────────────────
  // ALLOCATE AUDIO BUFFER
  // ─────────────────────────────────────────────────────────────────────
  //
  // Buffer capacity is 3 frames — enough headroom for both ALSA (ring-buffer
  // model) and Raylib (fixed-chunk model). Each backend caps its per-call
  // request to game->audio.max_sample_count at call time.
  //
  // Backend-specific timing state (latency_samples, running_sample_index,
  // etc.) lives in each backend's private config struct, NOT here.

  // NOTE: Both x11 (ALSA S16_LE) and Raylib (miniaudio 16-bit) backends
  // currently set AUDIO_FORMAT_I16 during their init.  If a future backend
  // sets a wider format the allocation here should be updated to use:
  //   audio_format_bytes_per_sample(format) * max_sample_count
  // Allocate for the largest possible format (F64 stereo = 16 bytes/frame)
  // so the buffer works regardless of which backend format is chosen at init.
  const int bytes_per_sample =
      audio_format_bytes_per_sample(AUDIO_FORMAT_F64); // largest format
  const u32 audio_hz = game->config.audio_game_update_hz != 0
                           ? game->config.audio_game_update_hz
                           : 60;
  const int samples_per_frame =
      (int)game->config.initial_audio_sample_rate / (int)audio_hz;
  const int max_sample_count = samples_per_frame * 3;
  const int audio_size = max_sample_count * bytes_per_sample;

  allocations->audio_samples =
      de100_memory_alloc(NULL, audio_size, De100_MEMORY_FLAG_RW_ZEROED);

  if (!de100_memory_is_valid(allocations->audio_samples)) {
    fprintf(stderr, "❌ Failed to allocate audio buffer\n");
    return 1;
  }

  game->audio.samples_per_second = (i32)game->config.initial_audio_sample_rate;
  game->audio.max_sample_count = max_sample_count;
  game->audio.samples_buffer = allocations->audio_samples.base;
  game->audio.is_initialized = false; // backend sets true after its init

  printf("✅ Audio buffer: %d samples max\n", max_sample_count);

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
#if DE100_SANITIZE_WAVE_1_MEMORY
  EngineGameState *game = &engine->game;
  EngineAllocations *allocations = &engine->allocations;
#endif

  printf("[SHUTDOWN] Engine cleanup...\n");

  replay_buffers_shutdown(platform->memory_state.replay_buffers,
                          platform->memory_state.total_size);

#if DE100_SANITIZE_WAVE_1_MEMORY
  // Clean up temp files
  de100_file_delete(platform->paths.game_main_lib_tmp_path);
  de100_file_delete(platform->paths.game_startup_lib_tmp_path);
  de100_file_delete(platform->paths.game_init_lib_tmp_path);

  // Free allocations
  if (de100_memory_is_valid(allocations->audio_samples)) {
    de100_memory_free(&allocations->audio_samples);
  }
  if (de100_memory_is_valid(game->backbuffer.memory)) {
    de100_memory_free(&game->backbuffer.memory);
  }
  if (de100_memory_is_valid(allocations->game_state)) {
    de100_memory_free(&allocations->game_state);
  }

  input_recording_end(&engine->platform.memory_state);
  input_recording_playback_end(&engine->platform.memory_state);
#endif

  printf("[SHUTDOWN] Engine cleanup complete\n");
}
