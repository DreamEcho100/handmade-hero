#ifndef DE100_FILE_H
#define DE100_FILE_H

#include <time.h>
#include <stdbool.h>

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

enum de100_file_error_code {
    FILE_SUCCESS = 0,
    FILE_ERROR_NOT_FOUND,
    FILE_ERROR_ACCESS_DENIED,
    FILE_ERROR_ALREADY_EXISTS,
    FILE_ERROR_IS_DIRECTORY,
    FILE_ERROR_DISK_FULL,
    FILE_ERROR_READ_FAILED,
    FILE_ERROR_WRITE_FAILED,
    FILE_ERROR_INVALID_PATH,
    FILE_ERROR_TOO_LARGE,
    FILE_ERROR_UNKNOWN
};

// ═══════════════════════════════════════════════════════════════════════════
// RESULT STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    bool success;
    enum de100_file_error_code error_code;
    char error_message[512];
} de100_file_result_t;

typedef struct {
    time_t value;
    bool success;
    enum de100_file_error_code error_code;
    char error_message[512];
} de100_file_time_result_t;

typedef struct {
    long long value;
    bool success;
    enum de100_file_error_code error_code;
    char error_message[512];
} de100_file_size_result_t;

typedef struct {
    bool exists;
    bool success;
    enum de100_file_error_code error_code;
    char error_message[512];
} de100_de100_file_exists_result_t;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get the last modification time of a file.
 * 
 * @param filename Path to the file
 * @return Result containing modification time or error information
 * 
 * On success: result.success = true, result.value = modification time
 * On error: result.success = false, result.error_code and error_message set
 */
de100_file_time_result_t de100_file_get_mod_time(const char *filename);

/**
 * Copy a file from source to destination.
 * 
 * @param source Path to source file
 * @param dest Path to destination file
 * @return Result indicating success or failure with error details
 * 
 * Overwrites destination if it exists.
 * On error, provides specific reason (file not found, access denied, etc.)
 */
de100_file_result_t de100_file_copy(const char *source, const char *dest);

/**
 * Check if a file exists.
 * 
 * @param filename Path to the file
 * @return Result indicating if file exists and any errors encountered
 * 
 * result.exists = true if file exists
 * result.success = false if an error occurred during check
 */
de100_de100_file_exists_result_t de100_file_exists(const char *filename);

/**
 * Get the size of a file in bytes.
 * 
 * @param filename Path to the file
 * @return Result containing file size or error information
 * 
 * On success: result.success = true, result.value = file size in bytes
 * On error: result.success = false, result.error_code and error_message set
 */
de100_file_size_result_t de100_file_get_size(const char *filename);

/**
 * Delete a file.
 * 
 * @param filename Path to the file
 * @return Result indicating success or failure with error details
 * 
 * Returns success if file doesn't exist (idempotent operation).
 * On error, provides specific reason (access denied, is directory, etc.)
 */
de100_file_result_t de100_file_delete(const char *filename);

/**
 * Get a human-readable error message for an error code.
 * 
 * @param code Error code
 * @return Static string describing the error
 */
const char *de100_file_strerror(enum de100_file_error_code code);

#endif // DE100_FILE_H
