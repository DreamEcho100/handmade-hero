#ifndef DE100_GAME_MEMORY_H
#define DE100_GAME_MEMORY_H

#include "../_common/memory.h"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════
// 🧠 DAY 14: EXPLICIT GAME MEMORY
// ═══════════════════════════════════════════════════════════════
// Casey's Day 14 addition: Platform allocates, game manages!
//
// MEMORY LAYOUT:
// ┌─────────────────────────────────────────────────────────────┐
// │ PermanentStorage (64MB)                                     │
// │ ┌───────────────────────────────────────────────────────┐   │
// │ │ game_state (your actual game data)                   │   │
// │ │ - gradient_state                                      │   │
// │ │ - pixel_state                                         │   │
// │ │ - speed                                               │   │
// │ └───────────────────────────────────────────────────────┘   │
// │ [Rest of 64MB available for:]                               │
// │ - Save game data                                            │
// │ - Player stats                                              │
// │ - Settings/options                                          │
// └─────────────────────────────────────────────────────────────┘
// ┌─────────────────────────────────────────────────────────────┐
// │ TransientStorage (4GB)                                      │
// │ [Ready for:]                                                │
// │ - Level geometry                                            │
// │ - Particle systems                                          │
// │ - Temporary buffers                                         │
// └─────────────────────────────────────────────────────────────┘
// ═══════════════════════════════════════════════════════════════

/**
 * 🧠 GAME MEMORY
 * ───────────────────────────────────────────────────────────────
 * Platform allocates this ONCE at startup using mmap().
 * Game receives pointer and casts to game_state*.
 *
 * Casey's Day 14 pattern:
 *   - IsInitialized flag for first-run detection
 *   - PermanentStorage for save data, settings
 *   - TransientStorage for level data, temp buffers
 * ───────────────────────────────────────────────────────────────
 */
typedef struct {
  // A permanent block of memory that the game can use between calls to
  // `game_update_and_render`. This is where you should store all your game
  // state!
  PlatformMemoryBlock permanent_storage;
  // A temporary block of memory that the game can use between calls to
  // `game_update_and_render`. This is where you should store all your
  // scratch data!
  PlatformMemoryBlock transient_storage;
  // Size of the permanent storage block in bytes
  uint64 permanent_storage_size;
  // Size of the temporary storage block in bytes
  uint64 transient_storage_size;
  // Has this memory been initialized?
  bool32 is_initialized;
} GameMemory;

typedef struct GameState GameState;

#endif // DE100_GAME_MEMORY_H
