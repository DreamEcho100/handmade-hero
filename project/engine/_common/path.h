#ifndef DE100_PATH_H
#define DE100_PATH_H

#include "base.h"
#include <stdbool.h>
#include <stddef.h>

// ═══════════════════════════════════════════════════════════════════════════
// PATH UTILITIES
// ═══════════════════════════════════════════════════════════════════════════
//
// Cross-platform path handling, equivalent to Casey's Day 22 path resolution.
//
// Casey's Windows approach:
//   1. GetModuleFileNameA() to get EXE path
//   2. Scan for last backslash to find directory
//   3. CatStrings() to join directory + filename
//
// Our approach:
//   1. Platform-specific executable path retrieval
//   2. Scan for last separator to find directory
//   3. path_join() to combine directory + filename
//
// ═══════════════════════════════════════════════════════════════════════════

// Maximum path length we support
#define PATH_MAX_LENGTH 4096

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    PATH_SUCCESS = 0,
    PATH_ERROR_INVALID_ARGUMENT,
    PATH_ERROR_BUFFER_TOO_SMALL,
    PATH_ERROR_NOT_FOUND,
    PATH_ERROR_PERMISSION_DENIED,
    PATH_ERROR_UNKNOWN,
    
    PATH_ERROR_COUNT  // Sentinel for validation
} PathErrorCode;

// ═══════════════════════════════════════════════════════════════════════════
// RESULT STRUCTURE (Lean - No Error Message Buffer)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    char path[PATH_MAX_LENGTH];
    size_t length;
    bool success;
    PathErrorCode error_code;
} PathResult;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get the full path to the currently running executable.
 *
 * Linux:   Reads /proc/self/exe symlink
 * macOS:   Uses _NSGetExecutablePath
 * Windows: Uses GetModuleFileNameA
 *
 * @return PathResult containing executable path or error code
 *
 * Example: "/home/user/project/build/handmade"
 */
PathResult path_get_executable(void);

/**
 * Get the directory containing the executable.
 *
 * Equivalent to Casey's path extraction:
 *   for(char *Scan = EXEFileName; *Scan; ++Scan) {
 *       if(*Scan == '\\') OnePastLastSlash = Scan + 1;
 *   }
 *
 * @return PathResult containing directory path WITH trailing slash
 *
 * Example: "/home/user/project/build/"
 */
PathResult path_get_executable_directory(void);

/**
 * Join a directory path with a filename.
 *
 * Equivalent to Casey's CatStrings function.
 * Handles whether directory has trailing slash or not.
 *
 * @param directory Directory path (can have trailing slash or not)
 * @param filename Filename to append
 * @return PathResult containing joined path
 *
 * Example: path_join("/home/user/build/", "libgame.so")
 *          → "/home/user/build/libgame.so"
 */
PathResult path_join(const char *directory, const char *filename);

// ═══════════════════════════════════════════════════════════════════════════
// ERROR HANDLING
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get a human-readable error message for an error code.
 *
 * @param code Error code from any path operation
 * @return Static string describing the error (never NULL)
 */
const char *path_strerror(PathErrorCode code);

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
const char *path_get_last_error_detail(void);

/**
 * Log a path operation result to stderr (debug builds only).
 */
void path_debug_log_result(const char *operation, PathResult result);

#endif // DE100_INTERNAL && DE100_SLOW

#endif // DE100_PATH_H
