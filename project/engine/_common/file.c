#include "file.h"
#include "time.h"

#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM-SPECIFIC INCLUDES
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#error "Unsupported platform for file operations"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG ERROR DETAIL STORAGE (Thread-Local)
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL && DE100_SLOW

// Thread-local buffer for detailed error messages
// Only populated in debug builds, only valid immediately after error
#if defined(_WIN32)
static __declspec(thread) char g_last_error_detail[1024];
#else
static __thread char g_last_error_detail[1024];
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

file_scoped_fn inline FileErrorCode
win32_error_to_file_error(DWORD error_code) {
  switch (error_code) {
  case ERROR_SUCCESS:
    return FILE_SUCCESS;

  case ERROR_FILE_NOT_FOUND:
  case ERROR_PATH_NOT_FOUND:
  case ERROR_INVALID_DRIVE:
    return FILE_ERROR_NOT_FOUND;

  case ERROR_ACCESS_DENIED:
  case ERROR_SHARING_VIOLATION:
  case ERROR_LOCK_VIOLATION:
  case ERROR_NETWORK_ACCESS_DENIED:
    return FILE_ERROR_ACCESS_DENIED;

  case ERROR_ALREADY_EXISTS:
  case ERROR_FILE_EXISTS:
    return FILE_ERROR_ALREADY_EXISTS;

  case ERROR_DIRECTORY:
  case ERROR_DIRECTORY_NOT_SUPPORTED:
    return FILE_ERROR_IS_DIRECTORY;

  case ERROR_DISK_FULL:
  case ERROR_HANDLE_DISK_FULL:
    return FILE_ERROR_DISK_FULL;

  case ERROR_READ_FAULT:
  case ERROR_CRC:
  case ERROR_SECTOR_NOT_FOUND:
    return FILE_ERROR_READ_FAILED;

  case ERROR_WRITE_FAULT:
  case ERROR_WRITE_PROTECT:
    return FILE_ERROR_WRITE_FAILED;

  case ERROR_INVALID_NAME:
  case ERROR_BAD_PATHNAME:
  case ERROR_FILENAME_EXCED_RANGE:
    return FILE_ERROR_INVALID_PATH;

  case ERROR_FILE_TOO_LARGE:
    return FILE_ERROR_TOO_LARGE;

  default:
    return FILE_ERROR_UNKNOWN;
  }
}

// #if DE100_INTERNAL && DE100_SLOW
file_scoped_fn inline void win32_set_error_detail(const char *operation,
                                                  const char *path,
                                                  DWORD error_code) {
  char sys_msg[512];
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 sys_msg, sizeof(sys_msg), NULL);

  // Remove trailing newline from Windows messages
  size_t len = strlen(sys_msg);
  while (len > 0 && (sys_msg[len - 1] == '\n' || sys_msg[len - 1] == '\r')) {
    sys_msg[--len] = '\0';
  }

  SET_ERROR_DETAIL("[%s] '%s' failed: %s (Win32 error %lu)", operation,
                   path ? path : "(null)", sys_msg, error_code);
}
// #endif

#else // POSIX

file_scoped_fn inline FileErrorCode errno_to_file_error(int err) {
  switch (err) {
  case 0:
    return FILE_SUCCESS;

  case ENOENT:
  case ENOTDIR:
    return FILE_ERROR_NOT_FOUND;

  case EACCES:
  case EPERM:
  case EROFS:
    return FILE_ERROR_ACCESS_DENIED;

  case EEXIST:
    return FILE_ERROR_ALREADY_EXISTS;

  case EISDIR:
    return FILE_ERROR_IS_DIRECTORY;

  case ENOSPC:
#if defined(EDQUOT)
  case EDQUOT:
#endif
    return FILE_ERROR_DISK_FULL;

  case EIO:
    return FILE_ERROR_READ_FAILED;

  case ENAMETOOLONG:
  case EINVAL:
  case ELOOP:
    return FILE_ERROR_INVALID_PATH;

  case EFBIG:
#if defined(EOVERFLOW)
  case EOVERFLOW:
#endif
    return FILE_ERROR_TOO_LARGE;

  default:
    return FILE_ERROR_UNKNOWN;
  }
}

// #if DE100_INTERNAL && DE100_SLOW
file_scoped_fn inline void posix_set_error_detail(const char *operation,
                                                  const char *path, int err) {
  SET_ERROR_DETAIL("[%s] '%s' failed: %s (errno %d)", operation,
                   path ? path : "(null)", strerror(err), err);
}
// #endif

#endif // Platform selection

// ═══════════════════════════════════════════════════════════════════════════
// RESULT HELPERS
// ═══════════════════════════════════════════════════════════════════════════

file_scoped_fn inline FileResult make_success(void) {
  CLEAR_ERROR_DETAIL();
  return (FileResult){.success = true, .error_code = FILE_SUCCESS};
}

file_scoped_fn inline FileResult make_error(FileErrorCode code) {
  return (FileResult){.success = false, .error_code = code};
}

// ═══════════════════════════════════════════════════════════════════════════
// GET FILE MODIFICATION TIME
// ═══════════════════════════════════════════════════════════════════════════

FileTimeResult file_get_mod_time(const char *filename) {
  FileTimeResult result = {0};

  // ─────────────────────────────────────────────────────────────────────
  // Validate input
  // ─────────────────────────────────────────────────────────────────────
  if (!filename) {
    result.error_code = FILE_ERROR_INVALID_PATH;
    SET_ERROR_DETAIL("[file_get_mod_time] NULL filename provided");
    return result;
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS
  // ─────────────────────────────────────────────────────────────────────
  WIN32_FILE_ATTRIBUTE_DATA file_info;

  if (!GetFileAttributesExA(filename, GetFileExInfoStandard, &file_info)) {
    DWORD error_code = GetLastError();
    result.error_code = win32_error_to_file_error(error_code);

#if DE100_INTERNAL && DE100_SLOW
    win32_set_error_detail("file_get_mod_time", filename, error_code);
#endif
    return result;
  }

  // Convert FILETIME (100-nanosecond intervals since 1601) to our format
  // Note: We keep Windows epoch, only care about relative comparisons
  ULARGE_INTEGER ull;
  ull.LowPart = file_info.ftLastWriteTime.dwLowDateTime;
  ull.HighPart = file_info.ftLastWriteTime.dwHighDateTime;

  result.value.seconds = (int64)(ull.QuadPart / 10000000ULL);
  result.value.nanoseconds = (int64)((ull.QuadPart % 10000000ULL) * 100);
  result.success = true;
  result.error_code = FILE_SUCCESS;

  CLEAR_ERROR_DETAIL();

#elif defined(__APPLE__)
  // ─────────────────────────────────────────────────────────────────────
  // MACOS (uses st_mtimespec)
  // ─────────────────────────────────────────────────────────────────────
  struct stat file_stat;

  if (stat(filename, &file_stat) != 0) {
    int err = errno;
    result.error_code = errno_to_file_error(err);

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("file_get_mod_time", filename, err);
#endif
    return result;
  }

  result.value.seconds = (int64)file_stat.st_mtime;
  result.value.nanoseconds = (int64)file_stat.st_mtimespec.tv_nsec;
  result.success = true;
  result.error_code = FILE_SUCCESS;

  CLEAR_ERROR_DETAIL();

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__unix__)
  // ─────────────────────────────────────────────────────────────────────
  // LINUX / BSD / UNIX (uses st_mtim)
  // ─────────────────────────────────────────────────────────────────────
  struct stat file_stat;

  if (stat(filename, &file_stat) != 0) {
    int err = errno;
    result.error_code = errno_to_file_error(err);

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("file_get_mod_time", filename, err);
#endif
    return result;
  }

  result.value.seconds = (int64)file_stat.st_mtime;
  result.value.nanoseconds = (int64)file_stat.st_mtim.tv_nsec;
  result.success = true;
  result.error_code = FILE_SUCCESS;

  CLEAR_ERROR_DETAIL();

#else
#error "file_get_mod_time not implemented for this platform"
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// COMPARE FILE MODIFICATION TIMES
// ═══════════════════════════════════════════════════════════════════════════

real64 file_time_diff(const PlatformTimeSpec *a, const PlatformTimeSpec *b) {
  if (!a || !b) {
    return 0.0;
  }
  // Returns (a - b) in seconds
  // Positive = a is newer, Negative = b is newer
  return platform_timespec_diff_seconds(b, a);
}

// ═══════════════════════════════════════════════════════════════════════════
// COPY FILE
// ═══════════════════════════════════════════════════════════════════════════

FileResult file_copy(const char *source, const char *dest) {
  // ─────────────────────────────────────────────────────────────────────
  // Validate input
  // ─────────────────────────────────────────────────────────────────────
  if (!source || !dest) {
    SET_ERROR_DETAIL("[file_copy] NULL path provided (source=%p, dest=%p)",
                     (void *)source, (void *)dest);
    return make_error(FILE_ERROR_INVALID_PATH);
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS - Use CopyFileA (handles everything for us)
  // ─────────────────────────────────────────────────────────────────────
  if (!CopyFileA(source, dest, FALSE)) {
    DWORD error_code = GetLastError();

#if DE100_INTERNAL && DE100_SLOW
    char detail[1024];
    snprintf(detail, sizeof(detail), "CopyFile '%s' -> '%s'", source, dest);
    win32_set_error_detail("file_copy", detail, error_code);
#endif
    return make_error(win32_error_to_file_error(error_code));
  }

  return make_success();

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX - Manual copy with read/write
  // ─────────────────────────────────────────────────────────────────────
  int source_fd = -1;
  int dest_fd = -1;

  // Open source file
  source_fd = open(source, O_RDONLY);
  if (source_fd < 0) {
    int err = errno;
#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("file_copy:open_source", source, err);
#endif
    return make_error(errno_to_file_error(err));
  }

  // Get source file info
  struct stat source_stat;
  if (fstat(source_fd, &source_stat) < 0) {
    int err = errno;
    close(source_fd);
#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("file_copy:fstat", source, err);
#endif
    return make_error(errno_to_file_error(err));
  }

  // Verify source is a regular file
  if (S_ISDIR(source_stat.st_mode)) {
    close(source_fd);
    SET_ERROR_DETAIL("[file_copy] Source '%s' is a directory", source);
    return make_error(FILE_ERROR_IS_DIRECTORY);
  }

  if (!S_ISREG(source_stat.st_mode)) {
    close(source_fd);
    SET_ERROR_DETAIL("[file_copy] Source '%s' is not a regular file", source);
    return make_error(FILE_ERROR_NOT_A_FILE);
  }

  // Create destination file with same permissions
  dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, source_stat.st_mode);
  if (dest_fd < 0) {
    int err = errno;
    close(source_fd);
#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("file_copy:open_dest", dest, err);
#endif
    return make_error(errno_to_file_error(err));
  }

  // Copy data in chunks
  // 8KB is a reasonable balance between syscall overhead and stack usage
  // @warn: File Copy Uses Small Buffer
  // 8KB is reasonable for most cases
  // But for large files, you're doing many read/write syscalls
  // Stack allocation is fine here (cold path)
  // Verdict: 🟡 Acceptable for a learning project. Production would use
  // sendfile() on Linux or memory-mapped I/O.
  char buffer[8192];
  ssize_t bytes_read;
  ssize_t total_copied = 0;

  while ((bytes_read = read(source_fd, buffer, sizeof(buffer))) > 0) {
    ssize_t bytes_written = write(dest_fd, buffer, (size_t)bytes_read);

    if (bytes_written != bytes_read) {
      int err = errno;
      close(source_fd);
      close(dest_fd);

#if DE100_INTERNAL && DE100_SLOW
      SET_ERROR_DETAIL(
          "[file_copy] Write failed: wrote %zd of %zd bytes to '%s': %s",
          bytes_written, bytes_read, dest, strerror(err));
#endif
      return make_error(FILE_ERROR_WRITE_FAILED);
    }

    total_copied += bytes_written;
  }

  // Check for read error
  if (bytes_read < 0) {
    int err = errno;
    close(source_fd);
    close(dest_fd);

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("file_copy:read", source, err);
#endif
    return make_error(FILE_ERROR_READ_FAILED);
  }

  close(source_fd);
  close(dest_fd);

  // Verify size matches (paranoid check)
  if (total_copied != source_stat.st_size) {
    SET_ERROR_DETAIL(
        "[file_copy] Size mismatch: copied %zd bytes, expected %lld bytes",
        total_copied, (long long)source_stat.st_size);
    return make_error(FILE_ERROR_SIZE_MISMATCH);
  }

  return make_success();
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// CHECK IF FILE EXISTS
// ═══════════════════════════════════════════════════════════════════════════

FileExistsResult file_exists(const char *filename) {
  FileExistsResult result = {0};

  if (!filename) {
    result.error_code = FILE_ERROR_INVALID_PATH;
    SET_ERROR_DETAIL("[file_exists] NULL filename provided");
    return result;
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS
  // ─────────────────────────────────────────────────────────────────────
  DWORD attrib = GetFileAttributesA(filename);

  if (attrib == INVALID_FILE_ATTRIBUTES) {
    DWORD error_code = GetLastError();

    // "Not found" is a valid answer, not an error
    if (error_code == ERROR_FILE_NOT_FOUND ||
        error_code == ERROR_PATH_NOT_FOUND) {
      result.exists = false;
      result.success = true;
      result.error_code = FILE_SUCCESS;
      CLEAR_ERROR_DETAIL();
    } else {
      result.exists = false;
      result.success = false;
      result.error_code = win32_error_to_file_error(error_code);
#if DE100_INTERNAL && DE100_SLOW
      win32_set_error_detail("file_exists", filename, error_code);
#endif
    }
  } else {
    // File/directory exists - check if it's a file (not directory)
    result.exists = !(attrib & FILE_ATTRIBUTE_DIRECTORY);
    result.success = true;
    result.error_code = FILE_SUCCESS;
    CLEAR_ERROR_DETAIL();

#if DE100_INTERNAL && DE100_SLOW
    if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
      SET_ERROR_DETAIL("[file_exists] '%s' exists but is a directory",
                       filename);
    }
#endif
  }

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX
  // ─────────────────────────────────────────────────────────────────────
  struct stat file_stat;

  if (stat(filename, &file_stat) == 0) {
    // Path exists - check what it is
    result.success = true;
    result.error_code = FILE_SUCCESS;

    if (S_ISREG(file_stat.st_mode)) {
      result.exists = true;
      CLEAR_ERROR_DETAIL();
    } else if (S_ISDIR(file_stat.st_mode)) {
      result.exists = false;
#if DE100_INTERNAL && DE100_SLOW
      SET_ERROR_DETAIL("[file_exists] '%s' exists but is a directory",
                       filename);
#endif
    } else {
      result.exists = false;
#if DE100_INTERNAL && DE100_SLOW
      SET_ERROR_DETAIL(
          "[file_exists] '%s' exists but is not a regular file (mode=0x%x)",
          filename, file_stat.st_mode);
#endif
    }
  } else {
    int err = errno;

    // "Not found" is a valid answer, not an error
    if (err == ENOENT || err == ENOTDIR) {
      result.exists = false;
      result.success = true;
      result.error_code = FILE_SUCCESS;
      CLEAR_ERROR_DETAIL();
    } else {
      result.exists = false;
      result.success = false;
      result.error_code = errno_to_file_error(err);
#if DE100_INTERNAL && DE100_SLOW
      posix_set_error_detail("file_exists", filename, err);
#endif
    }
  }
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// GET FILE SIZE
// ═══════════════════════════════════════════════════════════════════════════

FileSizeResult file_get_size(const char *filename) {
  FileSizeResult result = {.value = -1};

  if (!filename) {
    result.error_code = FILE_ERROR_INVALID_PATH;
    SET_ERROR_DETAIL("[file_get_size] NULL filename provided");
    return result;
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS
  // ─────────────────────────────────────────────────────────────────────
  WIN32_FILE_ATTRIBUTE_DATA file_info;

  if (!GetFileAttributesExA(filename, GetFileExInfoStandard, &file_info)) {
    DWORD error_code = GetLastError();
    result.error_code = win32_error_to_file_error(error_code);
#if DE100_INTERNAL && DE100_SLOW
    win32_set_error_detail("file_get_size", filename, error_code);
#endif
    return result;
  }

  // Check if it's a directory
  if (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    result.error_code = FILE_ERROR_IS_DIRECTORY;
    SET_ERROR_DETAIL("[file_get_size] '%s' is a directory", filename);
    return result;
  }

  LARGE_INTEGER size;
  size.HighPart = (LONG)file_info.nFileSizeHigh;
  size.LowPart = file_info.nFileSizeLow;

  result.value = (int64)size.QuadPart;
  result.success = true;
  result.error_code = FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX
  // ─────────────────────────────────────────────────────────────────────
  struct stat file_stat;

  if (stat(filename, &file_stat) != 0) {
    int err = errno;
    result.error_code = errno_to_file_error(err);
#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("file_get_size", filename, err);
#endif
    return result;
  }

  if (S_ISDIR(file_stat.st_mode)) {
    result.error_code = FILE_ERROR_IS_DIRECTORY;
    SET_ERROR_DETAIL("[file_get_size] '%s' is a directory", filename);
    return result;
  }

  result.value = (int64)file_stat.st_size;
  result.success = true;
  result.error_code = FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// DELETE FILE
// ═══════════════════════════════════════════════════════════════════════════

FileResult file_delete(const char *filename) {
  if (!filename) {
    SET_ERROR_DETAIL("[file_delete] NULL filename provided");
    return make_error(FILE_ERROR_INVALID_PATH);
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS
  // ─────────────────────────────────────────────────────────────────────
  if (!DeleteFileA(filename)) {
    DWORD error_code = GetLastError();

    // Idempotent: already deleted = success
    if (error_code == ERROR_FILE_NOT_FOUND ||
        error_code == ERROR_PATH_NOT_FOUND) {
      return make_success();
    }

#if DE100_INTERNAL && DE100_SLOW
    win32_set_error_detail("file_delete", filename, error_code);
#endif
    return make_error(win32_error_to_file_error(error_code));
  }

  return make_success();

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX
  // ─────────────────────────────────────────────────────────────────────
  if (unlink(filename) != 0) {
    int err = errno;

    // Idempotent: already deleted = success
    if (err == ENOENT) {
      return make_success();
    }

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("file_delete", filename, err);
#endif
    return make_error(errno_to_file_error(err));
  }

  return make_success();
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// ERROR STRING TRANSLATION
// ═══════════════════════════════════════════════════════════════════════════

const char *file_strerror(FileErrorCode code) {
  switch (code) {
  case FILE_SUCCESS:
    return "Success";

  case FILE_ERROR_NOT_FOUND:
    return "File or path not found";

  case FILE_ERROR_ACCESS_DENIED:
    return "Access denied (permission error, file locked, or read-only)";

  case FILE_ERROR_ALREADY_EXISTS:
    return "File already exists";

  case FILE_ERROR_IS_DIRECTORY:
    return "Path is a directory, expected a file";

  case FILE_ERROR_NOT_A_FILE:
    return "Path exists but is not a regular file (symlink, device, socket, "
           "etc.)";

  case FILE_ERROR_DISK_FULL:
    return "Disk full or quota exceeded";

  case FILE_ERROR_READ_FAILED:
    return "Read operation failed (I/O error)";

  case FILE_ERROR_WRITE_FAILED:
    return "Write operation failed (I/O error or write-protected)";

  case FILE_ERROR_INVALID_PATH:
    return "Invalid file path (NULL, too long, or contains invalid characters)";

  case FILE_ERROR_TOO_LARGE:
    return "File too large for operation";

  case FILE_ERROR_SIZE_MISMATCH:
    return "File size mismatch after operation (possible corruption)";

  case FILE_ERROR_UNKNOWN:
  default:
    return "Unknown file error";
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL && DE100_SLOW

const char *file_get_last_error_detail(void) {
  if (g_last_error_detail[0] == '\0') {
    return NULL;
  }
  return g_last_error_detail;
}

void file_debug_log_result(const char *operation, const char *path,
                           FileResult result) {
  if (result.success) {
    fprintf(stderr, "[FILE] %s('%s') = OK\n", operation,
            path ? path : "(null)");
  } else {
    const char *detail = file_get_last_error_detail();
    fprintf(stderr, "[FILE] %s('%s') = FAILED: %s\n", operation,
            path ? path : "(null)", file_strerror(result.error_code));
    if (detail) {
      fprintf(stderr, "       Detail: %s\n", detail);
    }
  }
}

#endif // DE100_INTERNAL && DE100_SLOW
