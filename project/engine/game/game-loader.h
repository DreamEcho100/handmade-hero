#ifndef DE100_HERO_game_LOADER_H
#define DE100_HERO_game_LOADER_H

#include "base.h"

#include "../_common/dll.h"
#include "../_common/time.h"
#include "config.h"


#ifndef DE100_SHARED_LIB_PREFIX
#if defined(_WIN32)
#define DE100_SHARED_LIB_PREFIX ""
#elif defined(__APPLE__)
#define DE100_SHARED_LIB_PREFIX "lib"
#else
#define DE100_SHARED_LIB_PREFIX "lib"
#endif
#endif

#ifndef DE100_SHARED_LIB_EXT
#if defined(_WIN32)
#define DE100_SHARED_LIB_EXT "dll"
#elif defined(__APPLE__)
#define DE100_SHARED_LIB_EXT "dylib"
#else
#define DE100_SHARED_LIB_EXT "so"
#endif
#endif

#ifndef GAME_BUILD_DIR_PATH
#define GAME_BUILD_DIR_PATH "./build"
#endif

// DE100_GAME_BUILD_DIR_PATH="./build"

// DE100_GAME_MAIN_LIB_NAME="main"
// DE100_GAME_MAIN_TMP_LIB_NAME="main.tmp"
#ifndef DE100_GAME_MAIN_LIB_NAME
#define DE100_GAME_MAIN_LIB_NAME "main"
#endif
#ifndef DE100_GAME_MAIN_TMP_LIB_NAME
#define DE100_GAME_MAIN_TMP_LIB_NAME "main_tmp"
#endif

// DE100_GAME_STARTUP_LIB_NAME="startup"
// DE100_GAME_STARTUP_TMP_LIB_NAME="startup.tmp"
#ifndef DE100_GAME_STARTUP_LIB_NAME
#define DE100_GAME_STARTUP_LIB_NAME "startup"
#endif
#ifndef DE100_GAME_STARTUP_TMP_LIB_NAME
#define DE100_GAME_STARTUP_TMP_LIB_NAME "startup_tmp"
#endif

// DE100_GAME_INIT_LIB_NAME="init"
// DE100_GAME_INIT_TMP_LIB_NAME="init.tmp"
#ifndef DE100_GAME_INIT_LIB_NAME
#define DE100_GAME_INIT_LIB_NAME "init"
#endif
#ifndef DE100_GAME_INIT_TMP_LIB_NAME
#define DE100_GAME_INIT_TMP_LIB_NAME "init_tmp"
#endif

// Main
#ifndef DE100_GAME_MAIN_SHARED_LIB_PATH
#define DE100_GAME_MAIN_SHARED_LIB_PATH                                        \
  GAME_BUILD_DIR_PATH "/" DE100_SHARED_LIB_PREFIX DE100_GAME_MAIN_LIB_NAME     \
                      "." DE100_SHARED_LIB_EXT
#endif
#ifndef DE100_GAME_MAIN_TMP_SHARED_LIB_PATH
#define DE100_GAME_MAIN_TMP_SHARED_LIB_PATH                                    \
  GAME_BUILD_DIR_PATH "/" DE100_SHARED_LIB_PREFIX DE100_GAME_MAIN_TMP_LIB_NAME \
                      "." DE100_SHARED_LIB_EXT
#endif

// Startup
#ifndef DE100_GAME_STARTUP_SHARED_LIB_PATH
#define DE100_GAME_STARTUP_SHARED_LIB_PATH                                     \
  GAME_BUILD_DIR_PATH "/" DE100_SHARED_LIB_PREFIX DE100_GAME_STARTUP_LIB_NAME  \
                      "." DE100_SHARED_LIB_EXT
#endif
#ifndef DE100_GAME_STARTUP_TMP_SHARED_LIB_PATH
#define DE100_GAME_STARTUP_TMP_SHARED_LIB_PATH                                 \
  GAME_BUILD_DIR_PATH                                                          \
  "/" DE100_SHARED_LIB_PREFIX DE100_GAME_STARTUP_TMP_LIB_NAME                  \
  "." DE100_SHARED_LIB_EXT
#endif

// Init
#ifndef DE100_GAME_INIT_SHARED_LIB_PATH
#define DE100_GAME_INIT_SHARED_LIB_PATH                                        \
  GAME_BUILD_DIR_PATH "/" DE100_SHARED_LIB_PREFIX DE100_GAME_INIT_LIB_NAME     \
                      "." DE100_SHARED_LIB_EXT
#endif
#ifndef DE100_GAME_INIT_TMP_SHARED_LIB_PATH
#define DE100_GAME_INIT_TMP_SHARED_LIB_PATH                                    \
  GAME_BUILD_DIR_PATH "/" DE100_SHARED_LIB_PREFIX DE100_GAME_INIT_TMP_LIB_NAME \
                      "." DE100_SHARED_LIB_EXT
#endif

// ═══════════════════════════════════════════════════════════════════════════
// GAME CODE FUNCTION SIGNATURES
// ═══════════════════════════════════════════════════════════════════════════

// Can be used for very early, one-time setup (e.g., static data, global
// resources, or things that must happen before memory allocation)
#define GAME_STARTUP(name)                                                     \
  int name(GameConfig *game_config)
typedef GAME_STARTUP(game_startup_t);

// Can be used for per-session or per-level initialization, possibly with access
// to memory arenas or engine services.
#define GAME_INIT(name)                                                        \
  void name(GameMemory *memory, GameInput *input, GameBackBuffer *buffer)
typedef GAME_INIT(game_init_t);

// Called once per frame - updates game logic and renders graphics
#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(GameMemory *memory, GameInput *input, GameBackBuffer *buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_t);

// Called to fill audio buffer - may be called multiple times per frame
#define GAME_GET_AUDIO_SAMPLES(name)                                           \
  void name(GameMemory *memory, GameAudioOutputBuffer *audio_buffer)
typedef GAME_GET_AUDIO_SAMPLES(game_get_audio_samples_t);

// ═══════════════════════════════════════════════════════════════════════════
// GAME CODE STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  struct de100_dll_t game_code_lib;
  PlatformTimeSpec last_write_time;

  game_update_and_render_t *update_and_render;
  game_get_audio_samples_t *get_audio_samples;
  game_startup_t *startup;
  game_init_t *init;

  bool32 is_valid;
} GameCode;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
  GAME_CODE_CATEGORY_NONE = 0,
  GAME_CODE_CATEGORY_MAIN = 1 << 0,
  GAME_CODE_CATEGORY_INIT = 1 << 1,
  GAME_CODE_CATEGORY_STARTUP = 1 << 2,
  GAME_CODE_CATEGORY_ANY = GAME_CODE_CATEGORY_MAIN | GAME_CODE_CATEGORY_INIT |
                           GAME_CODE_CATEGORY_STARTUP
} GAME_CODE_CATEGORIES;

typedef struct {
  char *game_main_lib_path;
  char *game_main_lib_tmp_path;

  char *game_startup_lib_path;
  char *game_startup_lib_tmp_path;

  char *game_init_lib_path;
  char *game_init_lib_tmp_path;
} LoadGameCodeConfig;

/**
 * Load game code from a dynamic library.
 *
 * @param game_code Pointer to GameCode structure to populate
 * @param source_lib_name Path to the source library file
 * @param temp_lib_name Path where temporary copy will be created
 * @param category Category of game code to load
 *
 * This function copies the library to allow hot-reloading while the game runs.
 * Check result.is_valid to determine if loading succeeded.
 *
 * Never exits or crashes - always returns a valid GameCode structure.
 * On error, uses stub functions and sets is_valid to false.
 */
int load_game_code(GameCode *game_code, LoadGameCodeConfig *config,
                   GAME_CODE_CATEGORIES category);

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
bool32 game_main_code_needs_reload(const GameCode *game_code,
                                   char *source_lib_name);

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
GAME_GET_AUDIO_SAMPLES(game_get_audio_samples_stub);

/**
 * Stub function for game_startup.
 * Used when game code fails to load.
 */
GAME_STARTUP(game_startup_stub);

/**
 * Stub function for game_init.
 * Used when game code fails to load.
 */
GAME_INIT(game_init_stub);

#endif // DE100_HERO_game_LOADER_H
