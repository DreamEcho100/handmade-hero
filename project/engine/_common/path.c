#include "path.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

// Platform-specific includes
#if defined(__linux__)
#include <limits.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/syslimits.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// HELPER: Convert errno to our error code
// ═══════════════════════════════════════════════════════════════════════════

static enum de100_path_error_code errno_to_path_error(int err) {
  switch (err) {
  case EINVAL:
    return PATH_ERROR_INVALID_ARGUMENT;
  case ENAMETOOLONG:
    return PATH_ERROR_BUFFER_TOO_SMALL;
  case ENOENT:
  case ENOTDIR:
    return PATH_ERROR_NOT_FOUND;
  case EACCES:
  case EPERM:
    return PATH_ERROR_PERMISSION_DENIED;
  default:
    return PATH_ERROR_UNKNOWN;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// GET EXECUTABLE PATH
// ═══════════════════════════════════════════════════════════════════════════

de100_path_result_t de100_get_executable_path(void) {
  de100_path_result_t result = {0};

#if defined(__linux__)
  // ─────────────────────────────────────────────────────────────────────
  // LINUX IMPLEMENTATION
  // ─────────────────────────────────────────────────────────────────────
  //
  // /proc/self/exe is a symbolic link maintained by the Linux kernel.
  // It always points to the executable of the current process.
  //
  // readlink() reads the TARGET of a symlink (where it points to)
  // without following the link. It returns the number of bytes written.
  //
  // IMPORTANT: readlink() does NOT null-terminate the result!
  // We must do that ourselves.
  //
  // Web Dev Analogy:
  //   This is like fs.readlinkSync('/proc/self/exe') in Node.js
  // ─────────────────────────────────────────────────────────────────────

  ssize_t len =
      readlink("/proc/self/exe", result.path, sizeof(result.path) - 1);

  if (len == -1) {
    result.success = false;
    result.error_code = errno_to_path_error(errno);
    snprintf(result.error_message, sizeof(result.error_message),
             "readlink(/proc/self/exe) failed: %s", strerror(errno));
    return result;
  }

  // Null-terminate (readlink doesn't do this!)
  result.path[len] = '\0';
  result.length = (size_t)len;
  result.success = true;
  result.error_code = PATH_SUCCESS;

#elif defined(__APPLE__)
  // ─────────────────────────────────────────────────────────────────────
  // MACOS IMPLEMENTATION
  // ─────────────────────────────────────────────────────────────────────

  uint32_t size = sizeof(result.path);
  if (_NSGetExecutablePath(result.path, &size) != 0) {
    result.success = false;
    result.error_code = PATH_ERROR_BUFFER_TOO_SMALL;
    snprintf(result.error_message, sizeof(result.error_message),
             "Buffer too small, need %u bytes", size);
    return result;
  }

  result.length = strlen(result.path);
  result.success = true;
  result.error_code = PATH_SUCCESS;

#elif defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS IMPLEMENTATION
  // ─────────────────────────────────────────────────────────────────────
  //
  // This is what Casey uses in Day 22:
  //   GetModuleFileNameA(0, EXEFileName, sizeof(EXEFileName));
  //
  // The first parameter (0 or NULL) means "get path of current process"
  // ─────────────────────────────────────────────────────────────────────

  DWORD len = GetModuleFileNameA(NULL, result.path, sizeof(result.path));

  if (len == 0) {
    result.success = false;
    result.error_code = PATH_ERROR_UNKNOWN;
    snprintf(result.error_message, sizeof(result.error_message),
             "GetModuleFileNameA failed with error %lu", GetLastError());
    return result;
  }

  result.length = (size_t)len;
  result.success = true;
  result.error_code = PATH_SUCCESS;

#else
  result.success = false;
  result.error_code = PATH_ERROR_UNKNOWN;
  snprintf(result.error_message, sizeof(result.error_message),
           "Unsupported platform");
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// GET EXECUTABLE DIRECTORY
// ═══════════════════════════════════════════════════════════════════════════

de100_path_result_t de100_get_executable_directory(void) {
  de100_path_result_t result = {0};

  // Step 1: Get full executable path
  de100_path_result_t exe_path = de100_get_executable_path();

  if (!exe_path.success) {
    result.success = false;
    result.error_code = exe_path.error_code;
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to get executable path: %s", exe_path.error_message);
    return result;
  }

  // Step 2: Copy the path
  strncpy(result.path, exe_path.path, sizeof(result.path) - 1);
  result.path[sizeof(result.path) - 1] = '\0';

  // ─────────────────────────────────────────────────────────────────────
  // Step 3: Find the last directory separator
  // ─────────────────────────────────────────────────────────────────────
  //
  // This is Casey's algorithm:
  //
  //   char *OnePastLastSlash = EXEFileName;
  //   for(char *Scan = EXEFileName; *Scan; ++Scan) {
  //       if(*Scan == '\\') {
  //           OnePastLastSlash = Scan + 1;
  //       }
  //   }
  //
  // We adapt it for Unix (forward slashes) and keep Windows support.
  //
  // Example:
  //   Input:  "/home/user/project/build/handmade"
  //   Output: "/home/user/project/build/"
  //                                     ↑
  //                          We truncate HERE (after last /)
  // ─────────────────────────────────────────────────────────────────────

  char *last_separator = NULL;

  for (char *scan = result.path; *scan != '\0'; ++scan) {
#if defined(_WIN32)
    // Windows: accept both \ and /
    if (*scan == '\\' || *scan == '/') {
      last_separator = scan;
    }
#else
    // Unix: only /
    if (*scan == '/') {
      last_separator = scan;
    }
#endif
  }

  if (last_separator != NULL) {
    // Truncate AFTER the separator (keep the trailing slash)
    *(last_separator + 1) = '\0';
    result.length = (size_t)(last_separator - result.path + 1);
  } else {
    // No separator found - use current directory
    // This shouldn't happen for absolute paths, but handle it anyway
    result.path[0] = '.';
#if defined(_WIN32)
    result.path[1] = '\\';
#else
    result.path[1] = '/';
#endif
    result.path[2] = '\0';
    result.length = 2;
  }

  result.success = true;
  result.error_code = PATH_SUCCESS;
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// PATH JOIN
// ═══════════════════════════════════════════════════════════════════════════

de100_path_result_t de100_path_join(const char *directory,
                                    const char *filename) {
  de100_path_result_t result = {0};

  // ─────────────────────────────────────────────────────────────────────
  // Validate inputs
  // ─────────────────────────────────────────────────────────────────────

  if (!directory || !filename) {
    result.success = false;
    result.error_code = PATH_ERROR_INVALID_ARGUMENT;
    snprintf(result.error_message, sizeof(result.error_message),
             "NULL argument: directory=%p, filename=%p", (void *)directory,
             (void *)filename);
    return result;
  }

  size_t dir_len = strlen(directory);
  size_t file_len = strlen(filename);

  // ─────────────────────────────────────────────────────────────────────
  // Check if directory already ends with separator
  // ─────────────────────────────────────────────────────────────────────

  bool has_trailing_separator = false;
  if (dir_len > 0) {
    char last_char = directory[dir_len - 1];
#if defined(_WIN32)
    has_trailing_separator = (last_char == '\\' || last_char == '/');
#else
    has_trailing_separator = (last_char == '/');
#endif
  }

  // ─────────────────────────────────────────────────────────────────────
  // Calculate required length and check bounds
  // ─────────────────────────────────────────────────────────────────────

  size_t separator_len = has_trailing_separator ? 0 : 1;
  size_t total_len = dir_len + separator_len + file_len;

  if (total_len >= sizeof(result.path)) {
    result.success = false;
    result.error_code = PATH_ERROR_BUFFER_TOO_SMALL;
    snprintf(result.error_message, sizeof(result.error_message),
             "Path too long: need %zu bytes, have %zu", total_len + 1,
             sizeof(result.path));
    return result;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Build the joined path
  // ─────────────────────────────────────────────────────────────────────
  //
  // This is equivalent to Casey's CatStrings:
  //
  //   CatStrings(OnePastLastSlash - EXEFileName, EXEFileName,
  //              sizeof(SourceGameCodeDLLFilename) - 1,
  //              SourceGameCodeDLLFilename, sizeof(SourceGameCodeDLLFullPath),
  //              SourceGameCodeDLLFullPath);
  //
  // We use snprintf for safety and clarity.
  // ─────────────────────────────────────────────────────────────────────

  if (has_trailing_separator) {
    snprintf(result.path, sizeof(result.path), "%s%s", directory, filename);
  } else {
#if defined(_WIN32)
    snprintf(result.path, sizeof(result.path), "%s\\%s", directory, filename);
#else
    snprintf(result.path, sizeof(result.path), "%s/%s", directory, filename);
#endif
  }

  result.length = strlen(result.path);
  result.success = true;
  result.error_code = PATH_SUCCESS;
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// ERROR STRING
// ═══════════════════════════════════════════════════════════════════════════

const char *de100_path_strerror(enum de100_path_error_code code) {
  switch (code) {
  case PATH_SUCCESS:
    return "Success";
  case PATH_ERROR_INVALID_ARGUMENT:
    return "Invalid argument";
  case PATH_ERROR_BUFFER_TOO_SMALL:
    return "Buffer too small";
  case PATH_ERROR_NOT_FOUND:
    return "Path not found";
  case PATH_ERROR_PERMISSION_DENIED:
    return "Permission denied";
  case PATH_ERROR_UNKNOWN:
  default:
    return "Unknown error";
  }
}