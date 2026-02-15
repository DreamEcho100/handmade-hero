#ifndef DE100_COMMON_DEBUG_FILE_IO_H
#define DE100_COMMON_DEBUG_FILE_IO_H

#include "../_common/base.h"
#include "../_common/file.h"
#include "../_common/memory.h"
#include "./thread.h"

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG FILE I/O
// ═══════════════════════════════════════════════════════════════════════════
//
// These functions are for DEBUG BUILDS ONLY (DE100_INTERNAL).
// They provide simple file I/O for development purposes:
//   - Loading test assets
//   - Saving debug dumps
//   - Hot reload file checks
//
// This module uses the common APIs:
//   - file.h: de100_file_exists(), de100_file_get_size(), de100_file_strerror()
//   - memory.h: de100_memory_alloc(), de100_memory_free(),
//   de100_memory_error_str()
//
// For production file I/O, use the proper asset loading system.
//
// Casey's Day 15 pattern:
//   De100DebugDe100FileReadResult file =
//   de100_debug_read_entire_file(__FILE__); if (file.size > 0) {
//       de100_debug_write_entire_file("test.out", file.size,
//       file.memory.base);
//       de100_debug_free_de100_file_memory(&file.memory);
//   }
// ═══════════════════════════════════════════════════════════════════════════

// #if DE100_INTERNAL

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
  DEBUG_FILE_SUCCESS = 0,
  DEBUG_DE100_FILE_ERROR_NULL_PATH,
  DEBUG_DE100_FILE_ERROR_NOT_FOUND,
  DEBUG_DE100_FILE_ERROR_EMPTY_FILE,
  DEBUG_DE100_FILE_ERROR_TOO_LARGE,
  DEBUG_DE100_FILE_ERROR_DE100_MEMORY_ALLOC,
  DEBUG_DE100_FILE_ERROR_READ_FAILED,
  DEBUG_DE100_FILE_ERROR_WRITE_FAILED,
  DEBUG_DE100_FILE_ERROR_NULL_DATA,
  DEBUG_DE100_FILE_ERROR_OPEN_FAILED,

  DEBUG_DE100_FILE_ERROR_COUNT
} De100DebugDe100FileErrorCode;

// ═══════════════════════════════════════════════════════════════════════════
// RESULT STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  De100MemoryBlock memory; // Memory block containing file data
  u32 size;                // Size of file in bytes (0 on failure)
  De100DebugDe100FileErrorCode error_code;
} De100DebugDe100FileReadResult;

typedef struct {
  bool32 success;
  De100DebugDe100FileErrorCode error_code;
} De100DebugFileWriteResult;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Read an entire file into memory.
 *
 * Uses de100_file_get_size() to determine size, then allocates via
 * de100_memory_alloc().
 *
 * @param filename Path to the file to read
 * @return Result with memory block and size, or size=0 on failure
 *
 * The returned memory must be freed with
 * de100_debug_free_de100_file_memory().
 *
 * Example:
 *   De100DebugDe100FileReadResult file =
 * de100_debug_read_entire_file("test.txt"); if (file.size > 0 &&
 * de100_memory_is_valid(file.memory)) {
 *       // Use file.memory.base, file.size
 *       de100_debug_free_de100_file_memory(&file.memory);
 *   }
 */
De100DebugDe100FileReadResult
de100_debug_read_entire_file(ThreadContext *thread_context,
                             const char *filename);

/**
 * Free memory allocated by de100_debug_read_entire_file().
 *
 * Uses de100_memory_free() internally.
 *
 * @param memory Pointer to the memory block to free
 *
 * Safe to call multiple times (idempotent).
 */
void de100_debug_free_de100_file_memory(ThreadContext *thread_context,
                                        De100MemoryBlock *memory);

/**
 * Write data to a file (creates or overwrites).
 *
 * @param filename Path to the file to write
 * @param size Number of bytes to write
 * @param data Pointer to data
 * @return Result with success flag and error code
 *
 * Example:
 *   const char *data = "Hello, World!";
 *   De100DebugFileWriteResult r =
 * de100_debug_write_entire_file("out.txt", 13, data); if (!r.success)
 * { printf("Error: %s\n",
 * de100_debug_de100_file_strerror(r.error_code));
 *   }
 */
De100DebugFileWriteResult
de100_debug_write_entire_file(ThreadContext *thread_context,
                              const char *filename, u32 size, const void *data);

// ═══════════════════════════════════════════════════════════════════════════
// ERROR HANDLING
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get a human-readable error message for an error code.
 *
 * @param code Error code from any debug file operation
 * @return Static string describing the error (never NULL)
 */
const char *de100_debug_de100_file_strerror(De100DebugDe100FileErrorCode code);

// #endif // DE100_INTERNAL

#endif // DE100_COMMON_DEBUG_FILE_IO_H
