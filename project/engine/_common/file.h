#ifndef DE100_FILE_H
#define DE100_FILE_H

#include "base.h"
#include "time.h"
#include <stdbool.h>

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    FILE_SUCCESS = 0,
    FILE_ERROR_NOT_FOUND,
    FILE_ERROR_ACCESS_DENIED,
    FILE_ERROR_ALREADY_EXISTS,
    FILE_ERROR_IS_DIRECTORY,
    FILE_ERROR_NOT_A_FILE,
    FILE_ERROR_DISK_FULL,
    FILE_ERROR_READ_FAILED,
    FILE_ERROR_WRITE_FAILED,
    FILE_ERROR_INVALID_PATH,
    FILE_ERROR_TOO_LARGE,
    FILE_ERROR_SIZE_MISMATCH,
    FILE_ERROR_UNKNOWN,
    
    FILE_ERROR_COUNT  // Sentinel for validation
} FileErrorCode;

// ═══════════════════════════════════════════════════════════════════════════
// RESULT STRUCTURES (Lean - No Error Message Buffers)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    bool success;
    FileErrorCode error_code;
} FileResult;

typedef struct {
    PlatformTimeSpec value;
    bool success;
    FileErrorCode error_code;
} FileTimeResult;

typedef struct {
    int64 value;  // -1 on error
    bool success;
    FileErrorCode error_code;
} FileSizeResult;

typedef struct {
    bool exists;
    bool success;  // false if check itself failed (e.g., permission error)
    FileErrorCode error_code;
} FileExistsResult;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get the last modification time of a file.
 *
 * @param filename Path to the file
 * @return FileTimeResult with modification time or error code
 *
 * Usage:
 *   FileTimeResult r = file_get_mod_time("game.so");
 *   if (!r.success) {
 *       printf("Error: %s\n", file_strerror(r.error_code));
 *   }
 */
FileTimeResult file_get_mod_time(const char *filename);

/**
 * Compare two file modification times.
 *
 * @param a First time value
 * @param b Second time value
 * @return Difference in seconds (a - b). Positive if a is newer.
 */
real64 file_time_diff(const PlatformTimeSpec *a, const PlatformTimeSpec *b);

/**
 * Copy a file from source to destination.
 * Overwrites destination if it exists.
 *
 * @param source Path to source file
 * @param dest Path to destination file
 * @return FileResult indicating success or failure
 */
FileResult file_copy(const char *source, const char *dest);

/**
 * Check if a file exists (and is a regular file, not a directory).
 *
 * @param filename Path to the file
 * @return FileExistsResult with exists flag and any error
 *
 * Note: result.success can be true even if result.exists is false
 *       (file doesn't exist is not an error, just a fact)
 */
FileExistsResult file_exists(const char *filename);

/**
 * Get the size of a file in bytes.
 *
 * @param filename Path to the file
 * @return FileSizeResult with size or error code
 */
FileSizeResult file_get_size(const char *filename);

/**
 * Delete a file. Idempotent - returns success if file doesn't exist.
 *
 * @param filename Path to the file
 * @return FileResult indicating success or failure
 */
FileResult file_delete(const char *filename);

// ═══════════════════════════════════════════════════════════════════════════
// ERROR HANDLING
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get a human-readable error message for an error code.
 *
 * @param code Error code from any file operation
 * @return Static string describing the error (never NULL)
 */
const char *file_strerror(FileErrorCode code);

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG UTILITIES (DE100_INTERNAL && DE100_SLOW only)
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL && DE100_SLOW

/**
 * Get detailed error information from the last failed operation.
 * Thread-local storage - only valid immediately after a failed call.
 *
 * @return Detailed error string with platform-specific info, or NULL
 */
const char *file_get_last_error_detail(void);

/**
 * Log a file operation result to stderr (debug builds only).
 *
 * @param operation Name of the operation (e.g., "file_copy")
 * @param path Primary path involved
 * @param result The result to log
 */
void file_debug_log_result(const char *operation, const char *path, FileResult result);

#endif // DE100_INTERNAL && DE100_SLOW

#endif // DE100_FILE_H
