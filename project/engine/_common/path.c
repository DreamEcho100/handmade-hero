#include "path.h"

#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM-SPECIFIC INCLUDES
// ═══════════════════════════════════════════════════════════════════════════

#if defined(__linux__)
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <errno.h>
#include <mach-o/dyld.h>
#include <sys/syslimits.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#error "Unsupported platform for path operations"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG ERROR DETAIL STORAGE (Thread-Local)
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL && DE100_SLOW

#if defined(_WIN32)
de100_file_scoped_global_var __declspec(thread) char g_last_error_detail[1024];
#else
de100_file_scoped_global_var __thread char g_last_error_detail[1024];
#endif

#define SET_ERROR_DETAIL(...)                                                  \
  snprintf(g_last_error_detail, sizeof(g_last_error_detail), __VA_ARGS__)

#define CLEAR_ERROR_DETAIL() (g_last_error_detail[0] = '\0')

#else

#define SET_ERROR_DETAIL(...) ((void)0)
#define CLEAR_ERROR_DETAIL() ((void)0)

#endif // DE100_INTERNAL && DE100_SLOW

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM ERROR TRANSLATION
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)

de100_file_scoped_fn inline PathErrorCode
win32_error_to_path_error(DWORD error_code) {
  switch (error_code) {
  case ERROR_SUCCESS:
    return PATH_SUCCESS;

  case ERROR_INVALID_PARAMETER:
  case ERROR_INVALID_NAME:
  case ERROR_BAD_PATHNAME:
    return PATH_ERROR_INVALID_ARGUMENT;

  case ERROR_INSUFFICIENT_BUFFER:
  case ERROR_FILENAME_EXCED_RANGE:
    return PATH_ERROR_BUFFER_TOO_SMALL;

  case ERROR_FILE_NOT_FOUND:
  case ERROR_PATH_NOT_FOUND:
    return PATH_ERROR_NOT_FOUND;

  case ERROR_ACCESS_DENIED:
    return PATH_ERROR_PERMISSION_DENIED;

  default:
    return PATH_ERROR_UNKNOWN;
  }
}

#if DE100_INTERNAL && DE100_SLOW
de100_file_scoped_fn inline void win32_set_error_detail(const char *operation,
                                                        DWORD error_code) {
  char sys_msg[512];
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 sys_msg, sizeof(sys_msg), NULL);

  // Remove trailing newline
  size_t len = strlen(sys_msg);
  while (len > 0 && (sys_msg[len - 1] == '\n' || sys_msg[len - 1] == '\r')) {
    sys_msg[--len] = '\0';
  }

  SET_ERROR_DETAIL("[%s] %s (Win32 error %lu)", operation, sys_msg, error_code);
}
#endif

#else // POSIX

de100_file_scoped_fn inline PathErrorCode errno_to_path_error(int err) {
  switch (err) {
  case 0:
    return PATH_SUCCESS;

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

#if DE100_INTERNAL && DE100_SLOW
de100_file_scoped_fn inline void posix_set_error_detail(const char *operation,
                                                        int err) {
  SET_ERROR_DETAIL("[%s] %s (errno %d)", operation, strerror(err), err);
}
#endif

#endif // Platform selection

// ═══════════════════════════════════════════════════════════════════════════
// RESULT HELPERS
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline PathResult make_path_error(PathErrorCode code) {
  PathResult result = {0};
  result.success = false;
  result.error_code = code;
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// GET EXECUTABLE PATH
// ═══════════════════════════════════════════════════════════════════════════

PathResult path_get_executable(void) {
  PathResult result = {0};

#if defined(__linux__)
  // ─────────────────────────────────────────────────────────────────────
  // LINUX: Read /proc/self/exe symlink
  // ─────────────────────────────────────────────────────────────────────
  //
  // /proc/self/exe is a symbolic link maintained by the kernel.
  // It always points to the executable of the current process.
  //
  // readlink() reads the TARGET of a symlink without following it.
  // IMPORTANT: readlink() does NOT null-terminate!
  // ─────────────────────────────────────────────────────────────────────

  ssize_t len =
      readlink("/proc/self/exe", result.path, sizeof(result.path) - 1);

  if (len == -1) {
    int err = errno;
    result.error_code = errno_to_path_error(err);

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("path_get_executable:readlink", err);
#endif
    return result;
  }

  // Null-terminate (readlink doesn't!)
  result.path[len] = '\0';
  result.length = (size_t)len;
  result.success = true;
  result.error_code = PATH_SUCCESS;

  CLEAR_ERROR_DETAIL();

#elif defined(__APPLE__)
  // ─────────────────────────────────────────────────────────────────────
  // MACOS: Use _NSGetExecutablePath
  // ─────────────────────────────────────────────────────────────────────

  uint32_t size = sizeof(result.path);

  if (_NSGetExecutablePath(result.path, &size) != 0) {
    result.error_code = PATH_ERROR_BUFFER_TOO_SMALL;

    SET_ERROR_DETAIL("[path_get_executable] Buffer too small, need %u bytes",
                     size);
    return result;
  }

  result.length = strlen(result.path);
  result.success = true;
  result.error_code = PATH_SUCCESS;

  CLEAR_ERROR_DETAIL();

#elif defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS: Use GetModuleFileNameA
  // ─────────────────────────────────────────────────────────────────────
  //
  // Casey's Day 22:
  //   GetModuleFileNameA(0, EXEFileName, sizeof(EXEFileName));
  //
  // First parameter NULL = get path of current process
  // ─────────────────────────────────────────────────────────────────────

  DWORD len = GetModuleFileNameA(NULL, result.path, sizeof(result.path));

  if (len == 0) {
    DWORD error_code = GetLastError();
    result.error_code = win32_error_to_path_error(error_code);

#if DE100_INTERNAL && DE100_SLOW
    win32_set_error_detail("path_get_executable:GetModuleFileNameA",
                           error_code);
#endif
    return result;
  }

  // Check for truncation
  if (len >= sizeof(result.path) - 1) {
    result.error_code = PATH_ERROR_BUFFER_TOO_SMALL;
    SET_ERROR_DETAIL("[path_get_executable] Path truncated at %lu chars", len);
    return result;
  }

  result.length = (size_t)len;
  result.success = true;
  result.error_code = PATH_SUCCESS;

  CLEAR_ERROR_DETAIL();

#else
#error "path_get_executable not implemented for this platform"
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// GET EXECUTABLE DIRECTORY
// ═══════════════════════════════════════════════════════════════════════════

PathResult path_get_executable_directory(void) {
  PathResult result = {0};

  // Step 1: Get full executable path
  PathResult exe_path = path_get_executable();

  if (!exe_path.success) {
    result.error_code = exe_path.error_code;

#if DE100_INTERNAL && DE100_SLOW
    const char *detail = path_get_last_error_detail();
    if (detail) {
      SET_ERROR_DETAIL(
          "[path_get_executable_directory] Failed to get exe path: %s", detail);
    }
#endif
    return result;
  }

  // Step 2: Copy the path
  // Using memcpy + explicit null-term is faster than snprintf for known-good
  // data
  size_t copy_len = exe_path.length;
  if (copy_len >= sizeof(result.path)) {
    copy_len = sizeof(result.path) - 1;
  }
  memcpy(result.path, exe_path.path, copy_len);
  result.path[copy_len] = '\0';

  // ─────────────────────────────────────────────────────────────────────
  // Step 3: Find the last directory separator
  // ─────────────────────────────────────────────────────────────────────
  //
  // Casey's algorithm (Day 22):
  //
  //   char *OnePastLastSlash = EXEFileName;
  //   for(char *Scan = EXEFileName; *Scan; ++Scan) {
  //       if(*Scan == '\\') {
  //           OnePastLastSlash = Scan + 1;
  //       }
  //   }
  //
  // We adapt for Unix (forward slashes) and keep Windows support.
  //
  // Example:
  //   Input:  "/home/user/project/build/handmade"
  //   Output: "/home/user/project/build/"
  //                                     ↑
  //                          Truncate HERE (after last /)
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
    // No separator found - use current directory notation
    // This shouldn't happen for absolute paths, but handle it
    result.path[0] = '.';
#if defined(_WIN32)
    result.path[1] = '\\';
#else
    result.path[1] = '/';
#endif
    result.path[2] = '\0';
    result.length = 2;

    SET_ERROR_DETAIL("[path_get_executable_directory] No separator found in "
                     "'%s', using './'",
                     exe_path.path);
  }

  result.success = true;
  result.error_code = PATH_SUCCESS;

  // Only clear if we didn't set a warning above
  if (last_separator != NULL) {
    CLEAR_ERROR_DETAIL();
  }

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// PATH JOIN
// ═══════════════════════════════════════════════════════════════════════════

PathResult path_join(const char *directory, const char *filename) {
  PathResult result = {0};

  // ─────────────────────────────────────────────────────────────────────
  // Validate inputs
  // ─────────────────────────────────────────────────────────────────────

  if (!directory || !filename) {
    SET_ERROR_DETAIL("[path_join] NULL argument: directory=%p, filename=%p",
                     (void *)directory, (void *)filename);
    return make_path_error(PATH_ERROR_INVALID_ARGUMENT);
  }

  size_t dir_len = strlen(directory);
  size_t de100_file_len = strlen(filename);

  // Empty directory is invalid
  if (dir_len == 0) {
    SET_ERROR_DETAIL("[path_join] Empty directory string");
    return make_path_error(PATH_ERROR_INVALID_ARGUMENT);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Check if directory already ends with separator
  // ─────────────────────────────────────────────────────────────────────

  char last_char = directory[dir_len - 1];
  bool has_trailing_separator = false;

#if defined(_WIN32)
  has_trailing_separator = (last_char == '\\' || last_char == '/');
  char separator = '\\';
#else
  has_trailing_separator = (last_char == '/');
  char separator = '/';
#endif

  // ─────────────────────────────────────────────────────────────────────
  // Calculate required length and check bounds
  // ─────────────────────────────────────────────────────────────────────

  size_t separator_len = has_trailing_separator ? 0 : 1;
  size_t total_len = dir_len + separator_len + de100_file_len;

  if (total_len >= sizeof(result.path)) {
    SET_ERROR_DETAIL("[path_join] Path too long: need %zu bytes, have %zu",
                     total_len + 1, sizeof(result.path));
    return make_path_error(PATH_ERROR_BUFFER_TOO_SMALL);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Build the joined path
  // ─────────────────────────────────────────────────────────────────────
  //
  // Using memcpy for performance (we already validated lengths)
  // ─────────────────────────────────────────────────────────────────────

  char *write_ptr = result.path;

  // Copy directory
  memcpy(write_ptr, directory, dir_len);
  write_ptr += dir_len;

  // Add separator if needed
  if (!has_trailing_separator) {
    *write_ptr++ = separator;
  }

  // Copy filename
  memcpy(write_ptr, filename, de100_file_len);
  write_ptr += de100_file_len;

  // Null-terminate
  *write_ptr = '\0';

  result.length = (size_t)(write_ptr - result.path);
  result.success = true;
  result.error_code = PATH_SUCCESS;

  CLEAR_ERROR_DETAIL();

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// ERROR STRING TRANSLATION
// ═══════════════════════════════════════════════════════════════════════════

const char *path_strerror(PathErrorCode code) {
  switch (code) {
  case PATH_SUCCESS:
    return "Success";

  case PATH_ERROR_INVALID_ARGUMENT:
    return "Invalid argument (NULL pointer or empty string)";

  case PATH_ERROR_BUFFER_TOO_SMALL:
    return "Path buffer too small (path exceeds maximum length)";

  case PATH_ERROR_NOT_FOUND:
    return "Path not found (file or directory does not exist)";

  case PATH_ERROR_PERMISSION_DENIED:
    return "Permission denied (insufficient privileges to access path)";

  case PATH_ERROR_UNKNOWN:
  default:
    return "Unknown path error";
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL && DE100_SLOW

const char *path_get_last_error_detail(void) {
  if (g_last_error_detail[0] == '\0') {
    return NULL;
  }
  return g_last_error_detail;
}

void path_debug_log_result(const char *operation, PathResult result) {
  if (result.success) {
    fprintf(stderr, "[PATH] %s = '%s' (len=%zu)\n", operation, result.path,
            result.length);
  } else {
    const char *detail = path_get_last_error_detail();
    fprintf(stderr, "[PATH] %s = FAILED: %s\n", operation,
            path_strerror(result.error_code));
    if (detail) {
      fprintf(stderr, "       Detail: %s\n", detail);
    }
  }
}

#endif // DE100_INTERNAL && DE100_SLOW
