#ifndef DE100_REPLAY_BUFFER_H
#define DE100_REPLAY_BUFFER_H

#include "../../_common/base.h"
#include "./memory.h"
#include <stdbool.h>

// ═══════════════════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════

// // Forward declare to avoid circular dependency with game/memory.h
// typedef struct GameMemoryState GameMemoryState;

// ═══════════════════════════════════════════════════════════════════════════
// CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════

#define MAX_REPLAY_BUFFERS 4
#define REPLAY_BUFFER_FILENAME_MAX 256

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
  REPLAY_BUFFER_SUCCESS = 0,
  REPLAY_BUFFER_ERROR_NULL_STATE,
  REPLAY_BUFFER_ERROR_INVALID_SLOT,
  REPLAY_BUFFER_ERROR_NO_GAME_MEMORY,
  REPLAY_BUFFER_ERROR_FILE_CREATE_FAILED,
  REPLAY_BUFFER_ERROR_FILE_RESIZE_FAILED,
  REPLAY_BUFFER_ERROR_MMAP_FAILED,
  REPLAY_BUFFER_ERROR_BUFFER_NOT_VALID,
  REPLAY_BUFFER_ERROR_SAVE_FAILED,
  REPLAY_BUFFER_ERROR_RESTORE_FAILED,

  REPLAY_BUFFER_ERROR_COUNT
} ReplayBufferErrorCode;

// ═══════════════════════════════════════════════════════════════════════════
// REPLAY BUFFER STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  int32 file_fd;                              // File descriptor
  void *memory_block;                         // mmap'd region (or allocated block on Windows)
  size_t mapped_size;                         // Size of the mapped region
  char filename[REPLAY_BUFFER_FILENAME_MAX];  // Path to backing file
  bool is_valid;                              // Ready for use?
  ReplayBufferErrorCode last_error;           // Last error for this buffer
} ReplayBuffer;

// ═══════════════════════════════════════════════════════════════════════════
// RESULT STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  bool success;
  ReplayBufferErrorCode error_code;
  int32 buffers_initialized;  // How many buffers were successfully initialized
} ReplayBufferInitResult;

typedef struct {
  bool success;
  ReplayBufferErrorCode error_code;
} ReplayBufferResult;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Initialize all replay buffers.
 *
 * Creates memory-mapped files for fast state snapshots.
 * Must be called AFTER game memory is allocated.
 *
 * @param exe_directory  Directory where replay files will be created
 * @param game_memory    Pointer to the game memory block
 * @param total_size     Total size of game memory (permanent + transient)
 * @param out_buffers    Array of MAX_REPLAY_BUFFERS to initialize
 * @return               Result with success status and count of initialized buffers
 */
ReplayBufferInitResult replay_buffers_init(const char *exe_directory,
                                           void *game_memory,
                                           uint64 total_size,
                                           ReplayBuffer *out_buffers);

/**
 * Shutdown all replay buffers.
 *
 * Unmaps memory and closes file descriptors.
 * Safe to call multiple times (idempotent).
 *
 * @param buffers     Array of replay buffers to shutdown
 * @param total_size  Size that was mapped (needed for munmap)
 */
void replay_buffers_shutdown(ReplayBuffer *buffers, uint64 total_size);

/**
 * Get a specific replay buffer with bounds checking.
 *
 * @param buffers     Array of replay buffers
 * @param slot_index  Which slot (0 to MAX_REPLAY_BUFFERS-1)
 * @return            Pointer to buffer, or NULL if invalid index
 */
ReplayBuffer *replay_buffer_get(ReplayBuffer *buffers, int32 slot_index);

/**
 * Save game state to a replay buffer.
 *
 * Fast memcpy from game memory to mmap'd region.
 * ~50-100ms for 1GB vs 2-5 seconds with file I/O.
 *
 * @param buffer       Target replay buffer
 * @param game_memory  Source game memory
 * @param total_size   Size to copy
 * @return             Result with success status
 */
ReplayBufferResult replay_buffer_save_state(ReplayBuffer *buffer,
                                            const void *game_memory,
                                            uint64 total_size);

/**
 * Restore game state from a replay buffer.
 *
 * Fast memcpy from mmap'd region to game memory.
 * ~50-100ms for 1GB vs 2-5 seconds with file I/O.
 *
 * @param buffer       Source replay buffer
 * @param game_memory  Target game memory
 * @param total_size   Size to copy
 * @return             Result with success status
 */
ReplayBufferResult replay_buffer_restore_state(const ReplayBuffer *buffer,
                                               void *game_memory,
                                               uint64 total_size);

/**
 * Check if a replay buffer is valid and ready for use.
 *
 * @param buffer  Buffer to check
 * @return        true if valid, false otherwise
 */
bool replay_buffer_is_valid(const ReplayBuffer *buffer);

/**
 * Get human-readable error message.
 *
 * @param code  Error code
 * @return      Static string describing the error
 */
const char *replay_buffer_strerror(ReplayBufferErrorCode code);

#endif // DE100_REPLAY_BUFFER_H
