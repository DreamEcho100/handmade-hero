#include "file.h"
#include <stdio.h>
#include <string.h>

// Platform-specific includes
#if defined(_WIN32)
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>

#define stat _stat

#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
/**
 * Convert Windows error code to our file error code.
 */
static enum de100_file_error_code win32_error_to_file_error(DWORD error_code) {
  switch (error_code) {
  case ERROR_FILE_NOT_FOUND:
  case ERROR_PATH_NOT_FOUND:
    return FILE_ERROR_NOT_FOUND;

  case ERROR_ACCESS_DENIED:
  case ERROR_SHARING_VIOLATION:
  case ERROR_LOCK_VIOLATION:
    return FILE_ERROR_ACCESS_DENIED;

  case ERROR_ALREADY_EXISTS:
  case ERROR_FILE_EXISTS:
    return FILE_ERROR_ALREADY_EXISTS;

  case ERROR_DIRECTORY:
    return FILE_ERROR_IS_DIRECTORY;

  case ERROR_DISK_FULL:
  case ERROR_HANDLE_DISK_FULL:
    return FILE_ERROR_DISK_FULL;

  case ERROR_INVALID_NAME:
  case ERROR_BAD_PATHNAME:
    return FILE_ERROR_INVALID_PATH;

  default:
    return FILE_ERROR_UNKNOWN;
  }
}

/**
 * Get Windows error message.
 */
static void win32_get_error_message(char *buffer, size_t buffer_size,
                                    DWORD error_code) {
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 buffer, (DWORD)buffer_size, NULL);

  // Remove trailing newline
  size_t len = strlen(buffer);
  if (len > 0 && buffer[len - 1] == '\n') {
    buffer[len - 1] = '\0';
    if (len > 1 && buffer[len - 2] == '\r') {
      buffer[len - 2] = '\0';
    }
  }
}
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
/**
 * Convert POSIX errno to our file error code.
 */
static enum de100_file_error_code errno_to_file_error(int err) {
  switch (err) {
  case ENOENT:
  case ENOTDIR:
    return FILE_ERROR_NOT_FOUND;

  case EACCES:
  case EPERM:
    return FILE_ERROR_ACCESS_DENIED;

  case EEXIST:
    return FILE_ERROR_ALREADY_EXISTS;

  case EISDIR:
    return FILE_ERROR_IS_DIRECTORY;

  case ENOSPC:
  case EDQUOT:
    return FILE_ERROR_DISK_FULL;

  case ENAMETOOLONG:
  case EINVAL:
    return FILE_ERROR_INVALID_PATH;

  case EIO:
    return FILE_ERROR_READ_FAILED;

  default:
    return FILE_ERROR_UNKNOWN;
  }
}
#endif

// ═══════════════════════════════════════════════════════════════════════════
// GET FILE MODIFICATION TIME
// ═══════════════════════════════════════════════════════════════════════════

de100_file_time_result_t de100_file_get_mod_time(const char *filename) {
  de100_file_time_result_t result = {0};

  if (!filename) {
    result.success = false;
    result.error_code = FILE_ERROR_INVALID_PATH;
    snprintf(result.error_message, sizeof(result.error_message),
             "NULL filename provided");
    return result;
  }

  struct stat file_stat;
  if (stat(filename, &file_stat) == 0) {
    result.success = true;
    result.value = file_stat.st_mtime;
    result.error_code = FILE_SUCCESS;
    snprintf(result.error_message, sizeof(result.error_message), "Success");
  } else {
#if defined(_WIN32)
    DWORD error_code = GetLastError();
    result.error_code = win32_error_to_file_error(error_code);
    win32_get_error_message(result.error_message, sizeof(result.error_message),
                            error_code);
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
    result.error_code = errno_to_file_error(errno);
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to stat '%s': %s", filename, strerror(errno));
#endif
    result.success = false;
    result.value = 0;
  }

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// COPY FILE
// ═══════════════════════════════════════════════════════════════════════════

de100_file_result_t de100_file_copy(const char *source, const char *dest) {
  de100_file_result_t result = {0};

  if (!source || !dest) {
    result.success = false;
    result.error_code = FILE_ERROR_INVALID_PATH;
    snprintf(result.error_message, sizeof(result.error_message),
             "NULL path provided (source: %p, dest: %p)", (void *)source,
             (void *)dest);
    return result;
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS
  // ─────────────────────────────────────────────────────────────────────
  if (!CopyFileA(source, dest, FALSE)) {
    DWORD error_code = GetLastError();
    result.success = false;
    result.error_code = win32_error_to_file_error(error_code);

    char sys_msg[256];
    win32_get_error_message(sys_msg, sizeof(sys_msg), error_code);
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to copy '%s' to '%s': %s", source, dest, sys_msg);
    return result;
  }

  result.success = true;
  result.error_code = FILE_SUCCESS;
  snprintf(result.error_message, sizeof(result.error_message), "Success");
  return result;

#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
  // ─────────────────────────────────────────────────────────────────────
  // UNIX/LINUX/MACOS
  // ─────────────────────────────────────────────────────────────────────

  // Open source file
  int source_fd = open(source, O_RDONLY);
  if (source_fd < 0) {
    result.success = false;
    result.error_code = errno_to_file_error(errno);
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to open source file '%s': %s", source, strerror(errno));
    return result;
  }

  // Get source file size and permissions
  struct stat source_stat;
  if (fstat(source_fd, &source_stat) < 0) {
    result.success = false;
    result.error_code = errno_to_file_error(errno);
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to stat source file '%s': %s", source, strerror(errno));
    close(source_fd);
    return result;
  }

  // Check if source is a directory
  if (S_ISDIR(source_stat.st_mode)) {
    result.success = false;
    result.error_code = FILE_ERROR_IS_DIRECTORY;
    snprintf(result.error_message, sizeof(result.error_message),
             "Source '%s' is a directory, not a file", source);
    close(source_fd);
    return result;
  }

  // Create/truncate destination file with same permissions
  int dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, source_stat.st_mode);
  if (dest_fd < 0) {
    result.success = false;
    result.error_code = errno_to_file_error(errno);
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to create dest file '%s': %s", dest, strerror(errno));
    close(source_fd);
    return result;
  }

  // Copy data in chunks
  char buffer[8192];
  ssize_t bytes_read;
  ssize_t total_copied = 0;

  while ((bytes_read = read(source_fd, buffer, sizeof(buffer))) > 0) {
    ssize_t bytes_written = write(dest_fd, buffer, bytes_read);
    if (bytes_written != bytes_read) {
      result.success = false;
      result.error_code = FILE_ERROR_WRITE_FAILED;
      snprintf(result.error_message, sizeof(result.error_message),
               "Write error copying to '%s': %s", dest, strerror(errno));
      close(source_fd);
      close(dest_fd);
      return result;
    }
    total_copied += bytes_written;
  }

  if (bytes_read < 0) {
    result.success = false;
    result.error_code = FILE_ERROR_READ_FAILED;
    snprintf(result.error_message, sizeof(result.error_message),
             "Read error copying from '%s': %s", source, strerror(errno));
    close(source_fd);
    close(dest_fd);
    return result;
  }

  close(source_fd);
  close(dest_fd);

  // Verify size matches
  if (total_copied != source_stat.st_size) {
    result.success = false;
    result.error_code = FILE_ERROR_WRITE_FAILED;
    snprintf(result.error_message, sizeof(result.error_message),
             "Size mismatch: copied %zd bytes, expected %ld bytes",
             total_copied, (long)source_stat.st_size);
    return result;
  }

  result.success = true;
  result.error_code = FILE_SUCCESS;
  snprintf(result.error_message, sizeof(result.error_message), "Success");
  return result;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// CHECK IF FILE EXISTS
// ═══════════════════════════════════════════════════════════════════════════

de100_de100_file_exists_result_t de100_file_exists(const char *filename) {
  de100_de100_file_exists_result_t result = {0};

  if (!filename) {
    result.exists = false;
    result.success = false;
    result.error_code = FILE_ERROR_INVALID_PATH;
    snprintf(result.error_message, sizeof(result.error_message),
             "NULL filename provided");
    return result;
  }

#if defined(_WIN32)
  DWORD attrib = GetFileAttributesA(filename);

  if (attrib == INVALID_FILE_ATTRIBUTES) {
    DWORD error_code = GetLastError();

    if (error_code == ERROR_FILE_NOT_FOUND ||
        error_code == ERROR_PATH_NOT_FOUND) {
      // File doesn't exist - this is not an error
      result.exists = false;
      result.success = true;
      result.error_code = FILE_SUCCESS;
      snprintf(result.error_message, sizeof(result.error_message),
               "File does not exist");
    } else {
      // Actual error occurred
      result.exists = false;
      result.success = false;
      result.error_code = win32_error_to_file_error(error_code);
      win32_get_error_message(result.error_message,
                              sizeof(result.error_message), error_code);
    }
  } else {
    // File exists
    result.exists = !(attrib & FILE_ATTRIBUTE_DIRECTORY);
    result.success = true;
    result.error_code = FILE_SUCCESS;

    if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
      snprintf(result.error_message, sizeof(result.error_message),
               "Path is a directory, not a file");
    } else {
      snprintf(result.error_message, sizeof(result.error_message), "Success");
    }
  }
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
  struct stat file_stat;

  if (stat(filename, &file_stat) == 0) {
    // Path exists
    result.exists = S_ISREG(file_stat.st_mode);
    result.success = true;
    result.error_code = FILE_SUCCESS;

    if (S_ISDIR(file_stat.st_mode)) {
      snprintf(result.error_message, sizeof(result.error_message),
               "Path is a directory, not a file");
    } else if (!S_ISREG(file_stat.st_mode)) {
      snprintf(result.error_message, sizeof(result.error_message),
               "Path is not a regular file");
    } else {
      snprintf(result.error_message, sizeof(result.error_message), "Success");
    }
  } else {
    if (errno == ENOENT) {
      // File doesn't exist - this is not an error
      result.exists = false;
      result.success = true;
      result.error_code = FILE_SUCCESS;
      snprintf(result.error_message, sizeof(result.error_message),
               "File does not exist");
    } else {
      // Actual error occurred
      result.exists = false;
      result.success = false;
      result.error_code = errno_to_file_error(errno);
      snprintf(result.error_message, sizeof(result.error_message),
               "Failed to check file '%s': %s", filename, strerror(errno));
    }
  }
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// GET FILE SIZE
// ═══════════════════════════════════════════════════════════════════════════

de100_file_size_result_t de100_file_get_size(const char *filename) {
  de100_file_size_result_t result = {0};

  if (!filename) {
    result.success = false;
    result.error_code = FILE_ERROR_INVALID_PATH;
    result.value = -1;
    snprintf(result.error_message, sizeof(result.error_message),
             "NULL filename provided");
    return result;
  }

#if defined(_WIN32)
  WIN32_FILE_ATTRIBUTE_DATA file_info;

  if (!GetFileAttributesExA(filename, GetFileExInfoStandard, &file_info)) {
    DWORD error_code = GetLastError();
    result.success = false;
    result.error_code = win32_error_to_file_error(error_code);
    result.value = -1;

    char sys_msg[256];
    win32_get_error_message(sys_msg, sizeof(sys_msg), error_code);
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to get size of '%s': %s", filename, sys_msg);
    return result;
  }

  LARGE_INTEGER size;
  size.HighPart = file_info.nFileSizeHigh;
  size.LowPart = file_info.nFileSizeLow;

  result.success = true;
  result.error_code = FILE_SUCCESS;
  result.value = size.QuadPart;
  snprintf(result.error_message, sizeof(result.error_message), "Success");

#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
  struct stat file_stat;

  if (stat(filename, &file_stat) != 0) {
    result.success = false;
    result.error_code = errno_to_file_error(errno);
    result.value = -1;
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to get size of '%s': %s", filename, strerror(errno));
    return result;
  }

  if (S_ISDIR(file_stat.st_mode)) {
    result.success = false;
    result.error_code = FILE_ERROR_IS_DIRECTORY;
    result.value = -1;
    snprintf(result.error_message, sizeof(result.error_message),
             "'%s' is a directory, not a file", filename);
    return result;
  }

  result.success = true;
  result.error_code = FILE_SUCCESS;
  result.value = (long long)file_stat.st_size;
  snprintf(result.error_message, sizeof(result.error_message), "Success");
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// DELETE FILE
// ═══════════════════════════════════════════════════════════════════════════

de100_file_result_t de100_file_delete(const char *filename) {
  de100_file_result_t result = {0};

  if (!filename) {
    result.success = false;
    result.error_code = FILE_ERROR_INVALID_PATH;
    snprintf(result.error_message, sizeof(result.error_message),
             "NULL filename provided");
    return result;
  }

#if defined(_WIN32)
  if (!DeleteFileA(filename)) {
    DWORD error_code = GetLastError();

    // File not found is considered success (idempotent)
    if (error_code == ERROR_FILE_NOT_FOUND) {
      result.success = true;
      result.error_code = FILE_SUCCESS;
      snprintf(result.error_message, sizeof(result.error_message),
               "File already deleted or does not exist");
      return result;
    }

    result.success = false;
    result.error_code = win32_error_to_file_error(error_code);

    char sys_msg[256];
    win32_get_error_message(sys_msg, sizeof(sys_msg), error_code);
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to delete '%s': %s", filename, sys_msg);
    return result;
  }

  result.success = true;
  result.error_code = FILE_SUCCESS;
  snprintf(result.error_message, sizeof(result.error_message), "Success");

#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
  if (unlink(filename) != 0) {
    // File not found is considered success (idempotent)
    if (errno == ENOENT) {
      result.success = true;
      result.error_code = FILE_SUCCESS;
      snprintf(result.error_message, sizeof(result.error_message),
               "File already deleted or does not exist");
      return result;
    }

    result.success = false;
    result.error_code = errno_to_file_error(errno);
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to delete '%s': %s", filename, strerror(errno));
    return result;
  }

  result.success = true;
  result.error_code = FILE_SUCCESS;
  snprintf(result.error_message, sizeof(result.error_message), "Success");
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// ERROR HANDLING UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

const char *de100_file_strerror(enum de100_file_error_code code) {
  switch (code) {
  case FILE_SUCCESS:
    return "Success";
  case FILE_ERROR_NOT_FOUND:
    return "File or path not found";
  case FILE_ERROR_ACCESS_DENIED:
    return "Access denied or permission error";
  case FILE_ERROR_ALREADY_EXISTS:
    return "File already exists";
  case FILE_ERROR_IS_DIRECTORY:
    return "Path is a directory, not a file";
  case FILE_ERROR_DISK_FULL:
    return "Disk full or quota exceeded";
  case FILE_ERROR_READ_FAILED:
    return "Read operation failed";
  case FILE_ERROR_WRITE_FAILED:
    return "Write operation failed";
  case FILE_ERROR_INVALID_PATH:
    return "Invalid file path";
  case FILE_ERROR_TOO_LARGE:
    return "File too large";
  case FILE_ERROR_UNKNOWN:
  default:
    return "Unknown error";
  }
}
