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
    FILE_ERROR_SEEK_FAILED,      // NEW
    FILE_ERROR_EOF,              // NEW
    FILE_ERROR_INVALID_FD,       // NEW
    FILE_ERROR_UNKNOWN,
    
    FILE_ERROR_COUNT  // Sentinel for validation
} De100FileErrorCode;

// ═══════════════════════════════════════════════════════════════════════════
// RESULT STRUCTURES (Lean - No Error Message Buffers)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    bool success;
    De100FileErrorCode error_code;
} De100FileResult;

typedef struct {
    PlatformTimeSpec value;
    bool success;
    De100FileErrorCode error_code;
} De100FileTimeResult;

typedef struct {
    int64 value;  // -1 on error
    bool success;
    De100FileErrorCode error_code;
} De100FileSizeResult;

typedef struct {
    bool exists;
    bool success;  // false if check itself failed (e.g., permission error)
    De100FileErrorCode error_code;
} De100FileExistsResult;

typedef struct {
    int32 fd;       // -1 on error
    bool success;
    De100FileErrorCode error_code;
} De100FileOpenResult;

typedef struct {
    size_t bytes_processed;  // How many bytes were actually read/written
    bool success;
    De100FileErrorCode error_code;
} De100FileIOResult;

// ═══════════════════════════════════════════════════════════════════════════
// FILE OPEN FLAGS
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    DE100_FILE_READ      = 1 << 0,
    DE100_FILE_WRITE     = 1 << 1,
    DE100_FILE_CREATE    = 1 << 2,  // Create if doesn't exist
    DE100_FILE_TRUNCATE  = 1 << 3,  // Truncate if exists
    DE100_FILE_APPEND    = 1 << 4,
} De100FileOpenFlags;

// ═══════════════════════════════════════════════════════════════════════════
// FILE SEEK ORIGIN
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    DE100_SEEK_SET = 0,  // From beginning
    DE100_SEEK_CUR = 1,  // From current position
    DE100_SEEK_END = 2,  // From end
} De100FileSeekOrigin;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS - High Level (Path-based)
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get the last modification time of a file.
 */
De100FileTimeResult de100_file_get_mod_time(const char *filename);

/**
 * Compare two file modification times.
 */
real64 de100_file_time_diff(const PlatformTimeSpec *a, const PlatformTimeSpec *b);

/**
 * Copy a file from source to destination.
 */
De100FileResult de100_file_copy(const char *source, const char *dest);

/**
 * Check if a file exists (and is a regular file, not a directory).
 */
De100FileExistsResult de100_file_exists(const char *filename);

/**
 * Get the size of a file in bytes.
 */
De100FileSizeResult de100_file_get_size(const char *filename);

/**
 * Delete a file. Idempotent - returns success if file doesn't exist.
 */
De100FileResult de100_file_delete(const char *filename);

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS - Low Level (File Descriptor-based)
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Open a file and return a file descriptor.
 *
 * @param filename  Path to the file
 * @param flags     Combination of De100FileOpenFlags
 * @return          De100FileOpenResult with fd or error
 *
 * Usage:
 *   De100FileOpenResult r = de100_file_open("data.bin", DE100_FILE_READ);
 *   if (r.success) {
 *       // use r.fd
 *       de100_file_close(r.fd);
 *   }
 */
De100FileOpenResult de100_file_open(const char *filename, De100FileOpenFlags flags);

/**
 * Close a file descriptor.
 *
 * @param fd  File descriptor to close
 * @return    De100FileResult indicating success or failure
 */
De100FileResult de100_file_close(int32 fd);

/**
 * Read exactly `size` bytes from a file descriptor.
 *
 * Handles partial reads (retries until all bytes read or error/EOF).
 *
 * @param fd      File descriptor
 * @param buffer  Destination buffer
 * @param size    Number of bytes to read
 * @return        De100FileIOResult with bytes_processed and status
 *
 * Note: Returns FILE_ERROR_EOF if EOF reached before reading all bytes.
 */
De100FileIOResult de100_file_read_all(int32 fd, void *buffer, size_t size);

/**
 * Write exactly `size` bytes to a file descriptor.
 *
 * Handles partial writes (retries until all bytes written or error).
 *
 * @param fd      File descriptor
 * @param buffer  Source buffer
 * @param size    Number of bytes to write
 * @return        De100FileIOResult with bytes_processed and status
 */
De100FileIOResult de100_file_write_all(int32 fd, const void *buffer, size_t size);

/**
 * Seek to a position in a file.
 *
 * @param fd      File descriptor
 * @param offset  Offset in bytes
 * @param origin  Where to seek from (DE100_SEEK_SET, DE100_SEEK_CUR, DE100_SEEK_END)
 * @return        De100FileSizeResult with new position or error
 */
De100FileSizeResult de100_file_seek(int32 fd, int64 offset, De100FileSeekOrigin origin);

// ═══════════════════════════════════════════════════════════════════════════
// ERROR HANDLING
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get a human-readable error message for an error code.
 */
const char *de100_file_strerror(De100FileErrorCode code);

#endif // DE100_FILE_H
