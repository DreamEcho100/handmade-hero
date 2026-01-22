#ifndef HANDMADE_HERO_game_LOADER_H
#define HANDMADE_HERO_game_LOADER_H

#include "../_common/dll.h"
#include "base.h"
#include <time.h>

// ═══════════════════════════════════════════════════════════════════════════
// GAME CODE FUNCTION SIGNATURES
// ═══════════════════════════════════════════════════════════════════════════

// Called once per frame - updates game logic and renders graphics
#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(GameMemory *memory, GameInput *input, GameOffscreenBuffer *buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_t);

// Called to fill audio buffer - may be called multiple times per frame
#define GAME_GET_SOUND_SAMPLES(name)                                           \
  void name(GameMemory *memory, GameAudioOutputBuffer *sound_buffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_audio_samples_t);

// ═══════════════════════════════════════════════════════════════════════════
// GAME CODE STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  struct de100_dll_t game_code_lib;
  time_t last_write_time;

  game_update_and_render_t *update_and_render;
  game_get_audio_samples_t *get_audio_samples;

  bool32 is_valid;
} GameCode;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Load game code from a dynamic library.
 *
 * @param source_lib_name Path to the source library file
 * @param temp_lib_name Path where temporary copy will be created
 * @return GameCode structure with loaded functions or stubs on error
 *
 * This function copies the library to allow hot-reloading while the game runs.
 * Check result.is_valid to determine if loading succeeded.
 *
 * Never exits or crashes - always returns a valid GameCode structure.
 * On error, uses stub functions and sets is_valid to false.
 */
GameCode load_game_code(const char *source_lib_name, const char *temp_lib_name);

/**
 * Unload game code and free resources.
 *
 * @param game_code Pointer to GameCode structure to unload
 *
 * After calling this, game_code will use stub functions.
 * Safe to call multiple times (idempotent).
 * Safe to call with NULL pointer (no-op with warning).
 */
void unload_game_code(GameCode *game_code);

/**
 * Check if game code needs to be reloaded.
 *
 * @param game_code Current GameCode structure
 * @param source_lib_name Path to the source library file
 * @return true if the source file has been modified, false otherwise
 *
 * Safe to call with NULL pointers (returns false with warning).
 * Returns false if file doesn't exist or can't be accessed.
 */
bool32 game_code_needs_reload(const GameCode *game_code,
                              const char *source_lib_name);

// ═══════════════════════════════════════════════════════════════════════════
// STUB FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Stub function for game_update_and_render.
 * Used when game code fails to load.
 */
GAME_UPDATE_AND_RENDER(game_update_and_render_stub);

/**
 * Stub function for game_get_audio_samples.
 * Used when game code fails to load.
 */
GAME_GET_SOUND_SAMPLES(game_get_audio_samples_stub);

#endif // HANDMADE_HERO_game_LOADER_H
