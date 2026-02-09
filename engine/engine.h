
#ifndef DE100_ENGINE_H
#define DE100_ENGINE_H

#include "_common/memory.h"
#include "game/audio.h"
#include "game/backbuffer.h"
#include "game/config.h"
#include "game/thread.h"
#include "game/game-loader.h"
#include "game/inputs.h"
#include "game/memory.h"
#include "platforms/_common/adaptive-fps.h"
#include "platforms/_common/config.h"
#include "platforms/_common/frame-stats.h"
#include "platforms/_common/frame-timing.h"

// ═══════════════════════════════════════════════════════════════════════════
// ENGINE STATE
// ═══════════════════════════════════════════════════════════════════════════
//
// Single struct containing ALL engine state.
//
// Organization:
//   - game: State passed to game functions
//   - platform: State used by platform layer
//   - allocations: Memory blocks owned by engine
//
// Usage:
//   EngineState engine = {0};
//   engine_init(&engine);
//   // ... main loop ...
//   engine_shutdown(&engine);
//
// ═══════════════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────
// GAME CONTEXT
// ─────────────────────────────────────────────────────────────────────
// Passed to game functions. Game code only sees these types.
// ─────────────────────────────────────────────────────────────────────

typedef struct {
  GameMemory memory;
  GameBackBuffer backbuffer;
  GameAudioOutputBuffer audio;
  GameConfig config;
  // This frame's inputs (new_input), Only current inputs
  // (what game sees)
  GameInput *inputs;

  ThreadContext thread_context;

} EngineGameState;

// ─────────────────────────────────────────────────────────────────────
// PLATFORM CONTEXT
// ─────────────────────────────────────────────────────────────────────
// Used by platform layer. Game code doesn't touch these.
// ─────────────────────────────────────────────────────────────────────

typedef struct {
  PlatformConfig config;
  GameCode code;
  GameCodePaths paths;
  GameMemoryState memory_state; // Recording/playback

  // Double-buffered inputs (platform manages swap)
  GameInput inputs[2];
  // Last frame's inputs (old_input)
  // ← Platform uses for state preservation
  GameInput *old_inputs;

  // Platform-specific extension (X11State*, Win32State*, etc.)
  void *backend;
} EnginePlatformState;

typedef struct {
  De100MemoryBlock game_state;    // Permanent + Transient
  De100MemoryBlock audio_samples; // Audio sample buffer
} EngineAllocations;

typedef struct {
  // ─────────────────────────────────────────────────────────────────────
  // GAME CONTEXT
  // ─────────────────────────────────────────────────────────────────────
  // Passed to game functions. Game code only sees these types.
  // ─────────────────────────────────────────────────────────────────────

  EngineGameState game;

  // ────────────────────────────────────────────────────────────────────
  // PLATFORM CONTEXT
  // ─────────────────────────────────────────────────────────────────────
  // Used by platform layer. Game code doesn't touch these.
  // ─────────────────────────────────────────────────────────────────────

  EnginePlatformState platform;

  // ─────────────────────────────────────────────────────────────────────
  // MEMORY ALLOCATIONS
  // ─────────────────────────────────────────────────────────────────────
  // Heap blocks owned by engine. Freed in engine_shutdown().
  // ─────────────────────────────────────────────────────────────────────

  EngineAllocations allocations;
} EngineState;

// ═══════════════════════════════════════════════════════════════════════════
// ENGINE LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Initialize engine state.
 *
 * Allocates game memory, backbuffer, audio buffer.
 * Loads game code and calls game.startup().
 *
 * @param engine  Pointer to engine state (can be stack-allocated)
 * @return 0 on success, non-zero on failure
 */
int engine_init(EngineState *engine);

/**
 * Shutdown engine and free all resources.
 *
 * @param engine  Pointer to engine state
 */
void engine_shutdown(EngineState *engine);

// ═══════════════════════════════════════════════════════════════════════════
// ENGINE HELPERS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Swap inputs buffers at end of frame.
 *
 * After calling:
 *   - engine->game.current points to fresh inputs buffer
 *   - engine->game.previous points to last frame's inputs
 */
de100_file_scoped_fn inline void engine_swap_inputs(EngineState *engine) {
  GameInput *temp = engine->game.inputs;
  engine->game.inputs = engine->platform.old_inputs;
  engine->platform.old_inputs = temp;
}

/**
 * Check if engine is ready to run.
 */
de100_file_scoped_fn inline bool engine_is_valid(EngineState *engine) {
  return engine && engine->platform.code.is_valid &&
         de100_memory_is_valid(engine->allocations.game_state);
}

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM BACKEND HELPERS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get platform-specific backend state.
 *
 * Usage (in X11 backend):
 *   X11State *x11 = engine_get_backend(engine, X11State);
 */
#define engine_get_backend(engine, Type) ((Type *)(engine)->platform.backend)

/**
 * Set platform-specific backend state.
 *
 * Usage (in X11 backend):
 *   X11State *x11 = calloc(1, sizeof(X11State));
 *   engine_set_backend(engine, x11);
 */
#define engine_set_backend(engine, ptr)                                        \
  ((engine)->platform.backend = (void *)(ptr))

#endif // DE100_ENGINE_H