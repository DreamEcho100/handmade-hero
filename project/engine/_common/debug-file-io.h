#ifndef DE100_COMMON_DEBUG_FILE_IO_H
#define DE100_COMMON_DEBUG_FILE_IO_H

#include "base.h"
#include "file.h"
#include "memory.h"

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
//   - file.h: file_exists(), file_get_size(), file_strerror()
//   - memory.h: memory_alloc(), memory_free(), memory_error_str()
//
// For production file I/O, use the proper asset loading system.
//
// Casey's Day 15 pattern:
//   DebugFileReadResult file = debug_platform_read_entire_file(__FILE__);
//   if (file.size > 0) {
//       debug_platform_write_entire_file("test.out", file.size, file.memory.base);
//       debug_platform_free_file_memory(&file.memory);
//   }
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    DEBUG_FILE_SUCCESS = 0,
    DEBUG_FILE_ERROR_NULL_PATH,
    DEBUG_FILE_ERROR_NOT_FOUND,
    DEBUG_FILE_ERROR_EMPTY_FILE,
    DEBUG_FILE_ERROR_TOO_LARGE,
    DEBUG_FILE_ERROR_MEMORY_ALLOC,
    DEBUG_FILE_ERROR_READ_FAILED,
    DEBUG_FILE_ERROR_WRITE_FAILED,
    DEBUG_FILE_ERROR_NULL_DATA,
    DEBUG_FILE_ERROR_OPEN_FAILED,
    
    DEBUG_FILE_ERROR_COUNT
} DebugFileErrorCode;

// ═══════════════════════════════════════════════════════════════════════════
// RESULT STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    MemoryBlock memory;          // Memory block containing file data
    uint32 size;                 // Size of file in bytes (0 on failure)
    DebugFileErrorCode error_code;
} DebugFileReadResult;

typedef struct {
    bool32 success;
    DebugFileErrorCode error_code;
} DebugFileWriteResult;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Read an entire file into memory.
 *
 * Uses file_get_size() to determine size, then allocates via memory_alloc().
 *
 * @param filename Path to the file to read
 * @return Result with memory block and size, or size=0 on failure
 *
 * The returned memory must be freed with debug_platform_free_file_memory().
 *
 * Example:
 *   DebugFileReadResult file = debug_platform_read_entire_file("test.txt");
 *   if (file.size > 0 && memory_is_valid(file.memory)) {
 *       // Use file.memory.base, file.size
 *       debug_platform_free_file_memory(&file.memory);
 *   }
 */
DebugFileReadResult debug_platform_read_entire_file(const char *filename);

/**
 * Free memory allocated by debug_platform_read_entire_file().
 *
 * Uses memory_free() internally.
 *
 * @param memory Pointer to the memory block to free
 *
 * Safe to call multiple times (idempotent).
 */
void debug_platform_free_file_memory(MemoryBlock *memory);

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
 *   DebugFileWriteResult r = debug_platform_write_entire_file("out.txt", 13, data);
 *   if (!r.success) {
 *       printf("Error: %s\n", debug_file_strerror(r.error_code));
 *   }
 */
DebugFileWriteResult debug_platform_write_entire_file(const char *filename, 
                                                       uint32 size, 
                                                       const void *data);

// ═══════════════════════════════════════════════════════════════════════════
// ERROR HANDLING
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get a human-readable error message for an error code.
 *
 * @param code Error code from any debug file operation
 * @return Static string describing the error (never NULL)
 */
const char *debug_file_strerror(DebugFileErrorCode code);

// ═══════════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Safely truncate a 64-bit value to 32-bit.
 *
 * @param value Value to truncate (must be >= 0 and <= UINT32_MAX)
 * @return Truncated 32-bit value
 *
 * In debug builds (DE100_SLOW), asserts if value is out of range.
 * Used for file sizes that we know fit in 32 bits.
 */
uint32 safe_truncate_uint64(int64 value);

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG UTILITIES (DE100_SLOW only)
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_SLOW

/**
 * Get detailed error information from the last failed operation.
 * Thread-local storage - only valid immediately after a failed call.
 *
 * @return Detailed error string, or NULL if no detail available
 */
const char *debug_file_get_last_error_detail(void);

#endif // DE100_SLOW

#endif // DE100_INTERNAL

#endif // DE100_COMMON_DEBUG_FILE_IO_H
