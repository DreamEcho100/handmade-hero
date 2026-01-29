#ifndef DE100_PATH_H
#define DE100_PATH_H

#include <stdbool.h>
#include <stddef.h>

// ═══════════════════════════════════════════════════════════════════════════
// PATH UTILITIES
// ═══════════════════════════════════════════════════════════════════════════
//
// These functions provide cross-platform path handling, equivalent to
// Casey's Day 22 path resolution code.
//
// Casey's Windows approach:
//   1. GetModuleFileNameA() to get EXE path
//   2. Scan for last backslash to find directory
//   3. CatStrings() to join directory + filename
//
// Our Linux approach:
//   1. readlink("/proc/self/exe") to get EXE path
//   2. Scan for last forward slash to find directory
//   3. de100_path_join() to join directory + filename
//
// ═══════════════════════════════════════════════════════════════════════════

// Maximum path length we support
#define DE100_PATH_MAX 4096

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

enum de100_path_error_code {
    PATH_SUCCESS = 0,
    PATH_ERROR_INVALID_ARGUMENT,
    PATH_ERROR_BUFFER_TOO_SMALL,
    PATH_ERROR_NOT_FOUND,
    PATH_ERROR_PERMISSION_DENIED,
    PATH_ERROR_UNKNOWN
};

// ═══════════════════════════════════════════════════════════════════════════
// RESULT STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    char path[DE100_PATH_MAX];
    size_t length;
    bool success;
    enum de100_path_error_code error_code;
    char error_message[256];
} de100_path_result_t;

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
 * @return Result containing executable path or error
 *
 * Example result: "/home/user/project/build/handmade"
 */
de100_path_result_t de100_get_executable_path(void);

/**
 * Get the directory containing the executable.
 *
 * This is equivalent to Casey's path extraction:
 *   for(char *Scan = EXEFileName; *Scan; ++Scan) {
 *       if(*Scan == '\\') OnePastLastSlash = Scan + 1;
 *   }
 *
 * @return Result containing directory path WITH trailing slash
 *
 * Example result: "/home/user/project/build/"
 */
de100_path_result_t de100_get_executable_directory(void);

/**
 * Join a directory path with a filename.
 *
 * This is equivalent to Casey's CatStrings function.
 * Handles whether directory has trailing slash or not.
 *
 * @param directory Directory path
 * @param filename Filename to append
 * @return Result containing joined path
 *
 * Example: de100_path_join("/home/user/build/", "libgame.so")
 *          → "/home/user/build/libgame.so"
 */
de100_path_result_t de100_path_join(const char *directory, const char *filename);

/**
 * Get human-readable error message for error code.
 */
const char *de100_path_strerror(enum de100_path_error_code code);

#endif // DE100_PATH_H