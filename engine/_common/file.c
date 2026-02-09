#include "file.h"
#include "base.h"
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

de100_file_scoped_fn inline De100FileErrorCode
win32_error_to_de100_file_error(DWORD error_code) {
  switch (error_code) {
  case ERROR_SUCCESS:
    return DE100_FILE_SUCCESS;

  case ERROR_FILE_NOT_FOUND:
  case ERROR_PATH_NOT_FOUND:
  case ERROR_INVALID_DRIVE:
    return DE100_FILE_ERROR_NOT_FOUND;

  case ERROR_ACCESS_DENIED:
  case ERROR_SHARING_VIOLATION:
  case ERROR_LOCK_VIOLATION:
  case ERROR_NETWORK_ACCESS_DENIED:
    return DE100_FILE_ERROR_ACCESS_DENIED;

  case ERROR_ALREADY_EXISTS:
  case ERROR_FILE_EXISTS:
    return DE100_FILE_ERROR_ALREADY_EXISTS;

  case ERROR_DIRECTORY:
  case ERROR_DIRECTORY_NOT_SUPPORTED:
    return DE100_FILE_ERROR_IS_DIRECTORY;

  case ERROR_DISK_FULL:
  case ERROR_HANDLE_DISK_FULL:
    return DE100_FILE_ERROR_DISK_FULL;

  case ERROR_READ_FAULT:
  case ERROR_CRC:
  case ERROR_SECTOR_NOT_FOUND:
    return DE100_FILE_ERROR_READ_FAILED;

  case ERROR_WRITE_FAULT:
  case ERROR_WRITE_PROTECT:
    return DE100_FILE_ERROR_WRITE_FAILED;

  case ERROR_INVALID_NAME:
  case ERROR_BAD_PATHNAME:
  case ERROR_FILENAME_EXCED_RANGE:
    return DE100_FILE_ERROR_INVALID_PATH;

  case ERROR_FILE_TOO_LARGE:
    return DE100_FILE_ERROR_TOO_LARGE;

  default:
    return DE100_FILE_ERROR_UNKNOWN;
  }
}

// #if DE100_INTERNAL && DE100_SLOW
de100_file_scoped_fn inline void win32_set_error_detail(const char *operation,
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

de100_file_scoped_fn inline De100FileErrorCode
errno_to_de100_file_error(int32 err) {
  switch (err) {
  case 0:
    return DE100_FILE_SUCCESS;

  case ENOENT:
  case ENOTDIR:
    return DE100_FILE_ERROR_NOT_FOUND;

  case EACCES:
  case EPERM:
  case EROFS:
    return DE100_FILE_ERROR_ACCESS_DENIED;

  case EEXIST:
    return DE100_FILE_ERROR_ALREADY_EXISTS;

  case EISDIR:
    return DE100_FILE_ERROR_IS_DIRECTORY;

  case ENOSPC:
#if defined(EDQUOT)
  case EDQUOT:
#endif
    return DE100_FILE_ERROR_DISK_FULL;

  case EIO:
    return DE100_FILE_ERROR_READ_FAILED;

  case ENAMETOOLONG:
  case EINVAL:
  case ELOOP:
    return DE100_FILE_ERROR_INVALID_PATH;

  case EFBIG:
#if defined(EOVERFLOW)
  case EOVERFLOW:
#endif
    return DE100_FILE_ERROR_TOO_LARGE;

  default:
    return DE100_FILE_ERROR_UNKNOWN;
  }
}

// #if DE100_INTERNAL && DE100_SLOW
de100_file_scoped_fn inline void
posix_set_error_detail(const char *operation, const char *path, int32 err) {
  SET_ERROR_DETAIL("[%s] '%s' failed: %s (errno %d)", operation,
                   path ? path : "(null)", strerror(err), err);
}
// #endif

#endif // Platform selection

// ═══════════════════════════════════════════════════════════════════════════
// RESULT HELPERS
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline De100FileResult make_success(void) {
  CLEAR_ERROR_DETAIL();
  return (De100FileResult){.success = true, .error_code = DE100_FILE_SUCCESS};
}

de100_file_scoped_fn inline De100FileResult
make_error(De100FileErrorCode code) {
  return (De100FileResult){.success = false, .error_code = code};
}

// ═══════════════════════════════════════════════════════════════════════════
// GET FILE MODIFICATION TIME
// ═══════════════════════════════════════════════════════════════════════════

De100FileTimeResult de100_file_get_mod_time(const char *filename) {
  De100FileTimeResult result = {0};

  // ─────────────────────────────────────────────────────────────────────
  // Validate inputs
  // ─────────────────────────────────────────────────────────────────────
  if (!filename) {
    result.error_code = DE100_FILE_ERROR_INVALID_PATH;
    SET_ERROR_DETAIL("[de100_file_get_mod_time] NULL filename provided");
    return result;
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS
  // ─────────────────────────────────────────────────────────────────────
  WIN32_FILE_ATTRIBUTE_DATA de100_file_info;

  if (!GetFileAttributesExA(filename, GetDe100FileExInfoStandard,
                            &de100_file_info)) {
    DWORD error_code = GetLastError();
    result.error_code = win32_error_to_de100_file_error(error_code);

#if DE100_INTERNAL && DE100_SLOW
    win32_set_error_detail("de100_file_get_mod_time", filename, error_code);
#endif
    return result;
  }

  // Convert FILETIME (100-nanosecond intervals since 1601) to our format
  // Note: We keep Windows epoch, only care about relative comparisons
  ULARGE_INTEGER ull;
  ull.LowPart = de100_file_info.ftLastWriteTime.dwLowDateTime;
  ull.HighPart = de100_file_info.ftLastWriteTime.dwHighDateTime;

  result.value.seconds = (int64)(ull.QuadPart / 10000000ULL);
  result.value.nanoseconds = (int64)((ull.QuadPart % 10000000ULL) * 100);
  result.success = true;
  result.error_code = DE100_FILE_SUCCESS;

  CLEAR_ERROR_DETAIL();

#elif defined(__APPLE__)
  // ─────────────────────────────────────────────────────────────────────
  // MACOS (uses st_mtimespec)
  // ─────────────────────────────────────────────────────────────────────
  struct stat de100_file_stat;

  if (stat(filename, &de100_file_stat) != 0) {
    int32 err = errno;
    result.error_code = errno_to_de100_file_error(err);

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_file_get_mod_time", filename, err);
#endif
    return result;
  }

  result.value.seconds = (int64)de100_file_stat.st_mtime;
  result.value.nanoseconds = (int64)de100_file_stat.st_mtimespec.tv_nsec;
  result.success = true;
  result.error_code = DE100_FILE_SUCCESS;

  CLEAR_ERROR_DETAIL();

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__unix__)
  // ─────────────────────────────────────────────────────────────────────
  // LINUX / BSD / UNIX (uses st_mtim)
  // ─────────────────────────────────────────────────────────────────────
  struct stat de100_file_stat;

  if (stat(filename, &de100_file_stat) != 0) {
    int32 err = errno;
    result.error_code = errno_to_de100_file_error(err);

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_file_get_mod_time", filename, err);
#endif
    return result;
  }

  result.value.seconds = (int64)de100_file_stat.st_mtime;
  result.value.nanoseconds = (int64)de100_file_stat.st_mtim.tv_nsec;
  result.success = true;
  result.error_code = DE100_FILE_SUCCESS;

  CLEAR_ERROR_DETAIL();

#else
#error "de100_file_get_mod_time not implemented for this platform"
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// COMPARE FILE MODIFICATION TIMES
// ═══════════════════════════════════════════════════════════════════════════

real64 de100_file_time_diff(const PlatformTimeSpec *a,
                            const PlatformTimeSpec *b) {
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

De100FileResult de100_file_copy(const char *source, const char *dest) {
  // ─────────────────────────────────────────────────────────────────────
  // Validate inputs
  // ─────────────────────────────────────────────────────────────────────
  if (!source || !dest) {
    SET_ERROR_DETAIL(
        "[de100_file_copy] NULL path provided (source=%p, dest=%p)",
        (void *)source, (void *)dest);
    return make_error(DE100_FILE_ERROR_INVALID_PATH);
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
    win32_set_error_detail("de100_file_copy", detail, error_code);
#endif
    return make_error(win32_error_to_de100_file_error(error_code));
  }

  return make_success();

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX - Manual copy with read/write
  // ─────────────────────────────────────────────────────────────────────
  int32 source_fd = -1;
  int32 dest_fd = -1;

  // Open source file
  source_fd = open(source, O_RDONLY);
  if (source_fd < 0) {
    int32 err = errno;
#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_file_copy:open_source", source, err);
#endif
    return make_error(errno_to_de100_file_error(err));
  }

  // Get source file info
  struct stat source_stat;
  if (fstat(source_fd, &source_stat) < 0) {
    int32 err = errno;
    close(source_fd);
#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_file_copy:fstat", source, err);
#endif
    return make_error(errno_to_de100_file_error(err));
  }

  // Verify source is a regular file
  if (S_ISDIR(source_stat.st_mode)) {
    close(source_fd);
    SET_ERROR_DETAIL("[de100_file_copy] Source '%s' is a directory", source);
    return make_error(DE100_FILE_ERROR_IS_DIRECTORY);
  }

  if (!S_ISREG(source_stat.st_mode)) {
    close(source_fd);
    SET_ERROR_DETAIL("[de100_file_copy] Source '%s' is not a regular file",
                     source);
    return make_error(DE100_FILE_ERROR_NOT_A_FILE);
  }

  // Create destination file with same permissions
  dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, source_stat.st_mode);
  if (dest_fd < 0) {
    int32 err = errno;
    close(source_fd);
#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_file_copy:open_dest", dest, err);
#endif
    return make_error(errno_to_de100_file_error(err));
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
      int32 err = errno;
      close(source_fd);
      close(dest_fd);

#if DE100_INTERNAL && DE100_SLOW
      SET_ERROR_DETAIL(
          "[de100_file_copy] Write failed: wrote %zd of %zd bytes to '%s': %s",
          bytes_written, bytes_read, dest, strerror(err));
#endif
      return make_error(DE100_FILE_ERROR_WRITE_FAILED);
    }

    total_copied += bytes_written;
  }

  // Check for read error
  if (bytes_read < 0) {
    int32 err = errno;
    close(source_fd);
    close(dest_fd);

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_file_copy:read", source, err);
#endif
    return make_error(DE100_FILE_ERROR_READ_FAILED);
  }

  close(source_fd);
  close(dest_fd);

  // Verify size matches (paranoid check)
  if (total_copied != source_stat.st_size) {
    SET_ERROR_DETAIL("[de100_file_copy] Size mismatch: copied %zd bytes, "
                     "expected %lld bytes",
                     total_copied, (long long)source_stat.st_size);
    return make_error(DE100_FILE_ERROR_SIZE_MISMATCH);
  }

  return make_success();
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// CHECK IF FILE EXISTS
// ═══════════════════════════════════════════════════════════════════════════

De100FileExistsResult de100_file_exists(const char *filename) {
  De100FileExistsResult result = {0};

  if (!filename) {
    result.error_code = DE100_FILE_ERROR_INVALID_PATH;
    SET_ERROR_DETAIL("[de100_file_exists] NULL filename provided");
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
      result.error_code = DE100_FILE_SUCCESS;
      CLEAR_ERROR_DETAIL();
    } else {
      result.exists = false;
      result.success = false;
      result.error_code = win32_error_to_de100_file_error(error_code);
#if DE100_INTERNAL && DE100_SLOW
      win32_set_error_detail("de100_file_exists", filename, error_code);
#endif
    }
  } else {
    // File/directory exists - check if it's a file (not directory)
    result.exists = !(attrib & FILE_ATTRIBUTE_DIRECTORY);
    result.success = true;
    result.error_code = DE100_FILE_SUCCESS;
    CLEAR_ERROR_DETAIL();

#if DE100_INTERNAL && DE100_SLOW
    if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
      SET_ERROR_DETAIL("[de100_file_exists] '%s' exists but is a directory",
                       filename);
    }
#endif
  }

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX
  // ─────────────────────────────────────────────────────────────────────
  struct stat de100_file_stat;

  if (stat(filename, &de100_file_stat) == 0) {
    // Path exists - check what it is
    result.success = true;
    result.error_code = DE100_FILE_SUCCESS;

    if (S_ISREG(de100_file_stat.st_mode)) {
      result.exists = true;
      CLEAR_ERROR_DETAIL();
    } else if (S_ISDIR(de100_file_stat.st_mode)) {
      result.exists = false;
#if DE100_INTERNAL && DE100_SLOW
      SET_ERROR_DETAIL("[de100_file_exists] '%s' exists but is a directory",
                       filename);
#endif
    } else {
      result.exists = false;
#if DE100_INTERNAL && DE100_SLOW
      SET_ERROR_DETAIL("[de100_file_exists] '%s' exists but is not a regular "
                       "file (mode=0x%x)",
                       filename, de100_file_stat.st_mode);
#endif
    }
  } else {
    int32 err = errno;

    // "Not found" is a valid answer, not an error
    if (err == ENOENT || err == ENOTDIR) {
      result.exists = false;
      result.success = true;
      result.error_code = DE100_FILE_SUCCESS;
      CLEAR_ERROR_DETAIL();
    } else {
      result.exists = false;
      result.success = false;
      result.error_code = errno_to_de100_file_error(err);
#if DE100_INTERNAL && DE100_SLOW
      posix_set_error_detail("de100_file_exists", filename, err);
#endif
    }
  }
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// GET FILE SIZE
// ═══════════════════════════════════════════════════════════════════════════

De100FileSizeResult de100_file_get_size(const char *filename) {
  De100FileSizeResult result = {.value = -1};

  if (!filename) {
    result.error_code = DE100_FILE_ERROR_INVALID_PATH;
    SET_ERROR_DETAIL("[de100_file_get_size] NULL filename provided");
    return result;
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS
  // ─────────────────────────────────────────────────────────────────────
  WIN32_FILE_ATTRIBUTE_DATA de100_file_info;

  if (!GetFileAttributesExA(filename, GetDe100FileExInfoStandard,
                            &de100_file_info)) {
    DWORD error_code = GetLastError();
    result.error_code = win32_error_to_de100_file_error(error_code);
#if DE100_INTERNAL && DE100_SLOW
    win32_set_error_detail("de100_file_get_size", filename, error_code);
#endif
    return result;
  }

  // Check if it's a directory
  if (de100_file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    result.error_code = DE100_FILE_ERROR_IS_DIRECTORY;
    SET_ERROR_DETAIL("[de100_file_get_size] '%s' is a directory", filename);
    return result;
  }

  LARGE_INTEGER size;
  size.HighPart = (LONG)de100_file_info.nDe100FileSizeHigh;
  size.LowPart = de100_file_info.nDe100FileSizeLow;

  result.value = (int64)size.QuadPart;
  result.success = true;
  result.error_code = DE100_FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX
  // ─────────────────────────────────────────────────────────────────────
  struct stat de100_file_stat;

  if (stat(filename, &de100_file_stat) != 0) {
    int32 err = errno;
    result.error_code = errno_to_de100_file_error(err);
#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_file_get_size", filename, err);
#endif
    return result;
  }

  if (S_ISDIR(de100_file_stat.st_mode)) {
    result.error_code = DE100_FILE_ERROR_IS_DIRECTORY;
    SET_ERROR_DETAIL("[de100_file_get_size] '%s' is a directory", filename);
    return result;
  }

  result.value = (int64)de100_file_stat.st_size;
  result.success = true;
  result.error_code = DE100_FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// DELETE FILE
// ═══════════════════════════════════════════════════════════════════════════

De100FileResult de100_file_delete(const char *filename) {
  if (!filename) {
    SET_ERROR_DETAIL("[de100_file_delete] NULL filename provided");
    return make_error(DE100_FILE_ERROR_INVALID_PATH);
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
    win32_set_error_detail("de100_file_delete", filename, error_code);
#endif
    return make_error(win32_error_to_de100_file_error(error_code));
  }

  return make_success();

#else
  // ─────────────────────────────────────────────────────────────────────
  // POSIX
  // ─────────────────────────────────────────────────────────────────────
  if (unlink(filename) != 0) {
    int32 err = errno;

    // Idempotent: already deleted = success
    if (err == ENOENT) {
      return make_success();
    }

#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_file_delete", filename, err);
#endif
    return make_error(errno_to_de100_file_error(err));
  }

  return make_success();
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// FILE OPEN
// ═══════════════════════════════════════════════════════════════════════════

De100FileOpenResult de100_file_open(const char *filename,
                                    De100FileOpenFlags flags) {
  De100FileOpenResult result = {.fd = -1, .success = false};

  if (!filename) {
    result.error_code = DE100_FILE_ERROR_INVALID_PATH;
    SET_ERROR_DETAIL("[de100_file_open] NULL filename provided");
    return result;
  }

#if defined(_WIN32)
  // Windows implementation
  DWORD access = 0;
  DWORD creation = OPEN_EXISTING;

  if (flags & DE100_FILE_READ)
    access |= GENERIC_READ;
  if (flags & DE100_FILE_WRITE)
    access |= GENERIC_WRITE;

  if (flags & DE100_FILE_CREATE) {
    if (flags & DE100_FILE_TRUNCATE) {
      creation = CREATE_ALWAYS;
    } else {
      creation = OPEN_ALWAYS;
    }
  } else if (flags & DE100_FILE_TRUNCATE) {
    creation = TRUNCATE_EXISTING;
  }

  HANDLE handle = CreateFileA(filename, access, FILE_SHARE_READ, NULL, creation,
                              FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    DWORD error_code = GetLastError();
    result.error_code = win32_error_to_de100_file_error(error_code);
#if DE100_INTERNAL && DE100_SLOW
    win32_set_error_detail("de100_file_open", filename, error_code);
#endif
    return result;
  }

  result.fd = _open_osfhandle((intptr_t)handle, 0);
  if (result.fd == -1) {
    CloseHandle(handle);
    result.error_code = DE100_FILE_ERROR_UNKNOWN;
    return result;
  }

#else
  // POSIX implementation
  int32 posix_flags = 0;

  if ((flags & DE100_FILE_READ) && (flags & DE100_FILE_WRITE)) {
    posix_flags = O_RDWR;
  } else if (flags & DE100_FILE_WRITE) {
    posix_flags = O_WRONLY;
  } else {
    posix_flags = O_RDONLY;
  }

  if (flags & DE100_FILE_CREATE)
    posix_flags |= O_CREAT;
  if (flags & DE100_FILE_TRUNCATE)
    posix_flags |= O_TRUNC;
  if (flags & DE100_FILE_APPEND)
    posix_flags |= O_APPEND;

  // 0644 = rw-r--r-- permissions for new files
  result.fd = open(filename, posix_flags, 0644);
  if (result.fd < 0) {
    int32 err = errno;
    result.error_code = errno_to_de100_file_error(err);
#if DE100_INTERNAL && DE100_SLOW
    posix_set_error_detail("de100_file_open", filename, err);
#endif
    return result;
  }
#endif

  result.success = true;
  result.error_code = DE100_FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// FILE CLOSE
// ═══════════════════════════════════════════════════════════════════════════

De100FileResult de100_file_close(int32 fd) {
  if (fd < 0) {
    SET_ERROR_DETAIL("[de100_file_close] Invalid file descriptor: %d", fd);
    return make_error(DE100_FILE_ERROR_INVALID_FD);
  }

#if defined(_WIN32)
  if (_close(fd) != 0) {
    return make_error(DE100_FILE_ERROR_UNKNOWN);
  }
#else
  if (close(fd) != 0) {
    int32 err = errno;
#if DE100_INTERNAL && DE100_SLOW
    SET_ERROR_DETAIL("[de100_file_close] close() failed: %s", strerror(err));
#endif
    return make_error(errno_to_de100_file_error(err));
  }
#endif

  return make_success();
}

// ═══════════════════════════════════════════════════════════════════════════
// READ ALL BYTES
// ═══════════════════════════════════════════════════════════════════════════

De100FileIOResult de100_file_read_all(int32 fd, void *buffer, size_t size) {
  De100FileIOResult result = {.bytes_processed = 0, .success = false};

  if (fd < 0) {
    result.error_code = DE100_FILE_ERROR_INVALID_FD;
    SET_ERROR_DETAIL("[de100_file_read_all] Invalid file descriptor: %d", fd);
    return result;
  }

  if (!buffer && size > 0) {
    result.error_code =
        DE100_FILE_ERROR_INVALID_PATH; // Reusing for invalid buffer
    SET_ERROR_DETAIL("[de100_file_read_all] NULL buffer with size %zu", size);
    return result;
  }

  uint8 *ptr = (uint8 *)buffer;
  size_t remaining = size;

  while (remaining > 0) {
#if defined(_WIN32)
    int32 bytes_read = _read(fd, ptr, (uint32)remaining);
#else
    ssize_t bytes_read = read(fd, ptr, remaining);
#endif

    if (bytes_read < 0) {
#if defined(_WIN32)
      int32 err = errno;
#else
      int32 err = errno;
      if (err == EINTR) {
        // Interrupted by signal, retry
        continue;
      }
#endif
      result.error_code = errno_to_de100_file_error(err);
#if DE100_INTERNAL && DE100_SLOW
      SET_ERROR_DETAIL("[de100_file_read_all] read() failed: %s",
                       strerror(err));
#endif
      return result;
    }

    if (bytes_read == 0) {
      // EOF - didn't get all the bytes we wanted
      result.error_code = DE100_FILE_ERROR_EOF;
      SET_ERROR_DETAIL("[de100_file_read_all] EOF after %zu bytes, wanted %zu",
                       result.bytes_processed, size);
      return result;
    }

    ptr += bytes_read;
    remaining -= (size_t)bytes_read;
    result.bytes_processed += (size_t)bytes_read;
  }

  result.success = true;
  result.error_code = DE100_FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// WRITE ALL BYTES
// ═══════════════════════════════════════════════════════════════════════════
//
// Writes `size` bytes from `buffer` to the file descriptor `fd`.
//
// APPEND vs OVERWRITE BEHAVIOR:
// This function writes from the current file position. The behavior depends on
// how the file was opened:
//
//   - DEFAULT (OVERWRITE): If opened without DE100_FILE_APPEND, writes start
//     at the current position (typically 0 for newly opened files), overwriting
//     existing data.
//
//   - APPEND MODE: If opened with DE100_FILE_APPEND flag, the OS automatically
//     repositions writes to the end of the file before each write (atomic).
//
// NOTE: This is a "write all or fail" function - it retries partial writes
// until all bytes are written or an error occurs.
//
// RETURNS: De100FileIOResult with bytes_processed set to total bytes written.
// ═════════════════════════════════════════════════════════════════════════

De100FileIOResult de100_file_write_all(int32 fd, const void *buffer,
                                       size_t size) {
  De100FileIOResult result = {.bytes_processed = 0, .success = false};

  if (fd < 0) {
    result.error_code = DE100_FILE_ERROR_INVALID_FD;
    SET_ERROR_DETAIL("[de100_file_write_all] Invalid file descriptor: %d", fd);
    return result;
  }

  if (!buffer && size > 0) {
    result.error_code = DE100_FILE_ERROR_INVALID_PATH;
    SET_ERROR_DETAIL("[de100_file_write_all] NULL buffer with size %zu", size);
    return result;
  }

  const uint8 *ptr = (const uint8 *)buffer;
  size_t remaining = size;

  while (remaining > 0) {
#if defined(_WIN32)
    int32 bytes_written = _write(fd, ptr, (uint32)remaining);
#else
    ssize_t bytes_written = write(fd, ptr, remaining);
#endif

    if (bytes_written < 0) {
#if defined(_WIN32)
      int32 err = errno;
#else
      int32 err = errno;
      if (err == EINTR) {
        // Interrupted by signal, retry
        continue;
      }
#endif
      result.error_code = errno_to_de100_file_error(err);
#if DE100_INTERNAL && DE100_SLOW
      SET_ERROR_DETAIL("[de100_file_write_all] write() failed: %s",
                       strerror(err));
#endif
      return result;
    }

    ptr += bytes_written;
    remaining -= (size_t)bytes_written;
    result.bytes_processed += (size_t)bytes_written;
  }

  result.success = true;
  result.error_code = DE100_FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// FILE SEEK
// ═══════════════════════════════════════════════════════════════════════════

De100FileSizeResult de100_file_seek(int32 fd, int64 offset,
                                    De100FileSeekOrigin origin) {
  De100FileSizeResult result = {.value = -1, .success = false};

  if (fd < 0) {
    result.error_code = DE100_FILE_ERROR_INVALID_FD;
    SET_ERROR_DETAIL("[de100_file_seek] Invalid file descriptor: %d", fd);
    return result;
  }

  int32 whence;
  switch (origin) {
  case DE100_SEEK_SET:
    whence = SEEK_SET;
    break;
  case DE100_SEEK_CUR:
    whence = SEEK_CUR;
    break;
  case DE100_SEEK_END:
    whence = SEEK_END;
    break;
  default:
    result.error_code = DE100_FILE_ERROR_UNKNOWN;
    SET_ERROR_DETAIL("[de100_file_seek] Invalid origin: %d", origin);
    return result;
  }

#if defined(_WIN32)
  int64 new_pos = _lseeki64(fd, offset, whence);
#else
  off_t new_pos = lseek(fd, (off_t)offset, whence);
#endif

  if (new_pos < 0) {
    int32 err = errno;
    result.error_code = DE100_FILE_ERROR_SEEK_FAILED;
#if DE100_INTERNAL && DE100_SLOW
    SET_ERROR_DETAIL("[de100_file_seek] lseek() failed: %s", strerror(err));
#endif
    return result;
  }

  result.value = (int64)new_pos;
  result.success = true;
  result.error_code = DE100_FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// ERROR STRING TRANSLATION
// ═══════════════════════════════════════════════════════════════════════════

const char *de100_file_strerror(De100FileErrorCode code) {
  switch (code) {
  case DE100_FILE_SUCCESS:
    return "Success";

  case DE100_FILE_ERROR_NOT_FOUND:
    return "File or path not found";

  case DE100_FILE_ERROR_ACCESS_DENIED:
    return "Access denied (permission error, file locked, or read-only)";

  case DE100_FILE_ERROR_ALREADY_EXISTS:
    return "File already exists";

  case DE100_FILE_ERROR_IS_DIRECTORY:
    return "Path is a directory, expected a file";

  case DE100_FILE_ERROR_NOT_A_FILE:
    return "Path exists but is not a regular file";

  case DE100_FILE_ERROR_DISK_FULL:
    return "Disk full or quota exceeded";

  case DE100_FILE_ERROR_READ_FAILED:
    return "Read operation failed (I/O error)";

  case DE100_FILE_ERROR_WRITE_FAILED:
    return "Write operation failed (I/O error or write-protected)";

  case DE100_FILE_ERROR_INVALID_PATH:
    return "Invalid file path (NULL, too long, or contains invalid characters)";

  case DE100_FILE_ERROR_TOO_LARGE:
    return "File too large for operation";

  case DE100_FILE_ERROR_SIZE_MISMATCH:
    return "File size mismatch after operation (possible corruption)";

  case DE100_FILE_ERROR_SEEK_FAILED:
    return "Seek operation failed";

  case DE100_FILE_ERROR_EOF:
    return "Unexpected end of file";

  case DE100_FILE_ERROR_INVALID_FD:
    return "Invalid file descriptor";

  case DE100_FILE_ERROR_UNKNOWN:
  default:
    return "Unknown file error";
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL && DE100_SLOW

const char *de100_file_get_last_error_detail(void) {
  if (g_last_error_detail[0] == '\0') {
    return NULL;
  }
  return g_last_error_detail;
}

void de100_file_debug_log_result(const char *operation, const char *path,
                                 De100FileResult result) {
  if (result.success) {
    fprintf(stderr, "[FILE] %s('%s') = OK\n", operation,
            path ? path : "(null)");
  } else {
    const char *detail = de100_file_get_last_error_detail();
    fprintf(stderr, "[FILE] %s('%s') = FAILED: %s\n", operation,
            path ? path : "(null)", de100_file_strerror(result.error_code));
    if (detail) {
      fprintf(stderr, "       Detail: %s\n", detail);
    }
  }
}

#endif // DE100_INTERNAL && DE100_SLOW
