#if DE100_INTERNAL

#include "debug-file-io.h"
#include "file.h"
#include "memory.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG ERROR DETAIL STORAGE (Thread-Local)
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_SLOW

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

#endif // DE100_SLOW

// ═══════════════════════════════════════════════════════════════════════════
// ERROR STRING TRANSLATION
// ═══════════════════════════════════════════════════════════════════════════

const char *debug_file_strerror(DebugFileErrorCode code) {
  switch (code) {
  case DEBUG_FILE_SUCCESS:
    return "Success";

  case DEBUG_FILE_ERROR_NULL_PATH:
    return "NULL or empty file path";

  case DEBUG_FILE_ERROR_NOT_FOUND:
    return "File not found";

  case DEBUG_FILE_ERROR_EMPTY_FILE:
    return "File is empty";

  case DEBUG_FILE_ERROR_TOO_LARGE:
    return "File too large (exceeds 4GB limit for debug I/O)";

  case DEBUG_FILE_ERROR_MEMORY_ALLOC:
    return "Memory allocation failed";

  case DEBUG_FILE_ERROR_READ_FAILED:
    return "Failed to read file contents";

  case DEBUG_FILE_ERROR_WRITE_FAILED:
    return "Failed to write file contents";

  case DEBUG_FILE_ERROR_NULL_DATA:
    return "NULL data pointer with non-zero size";

  case DEBUG_FILE_ERROR_OPEN_FAILED:
    return "Failed to open file";

  case DEBUG_FILE_ERROR_COUNT:
  default:
    return "Unknown debug file error";
  }
}

#if DE100_SLOW
const char *debug_file_get_last_error_detail(void) {
  if (g_last_error_detail[0] == '\0') {
    return NULL;
  }
  return g_last_error_detail;
}
#endif

// ═══════════════════════════════════════════════════════════════════════════
// SAFE TRUNCATION
// ═══════════════════════════════════════════════════════════════════════════

uint32 safe_truncate_uint64(int64 value) {
  // Negative values indicate an error (e.g., ftell() returns -1 on error)
  DEV_ASSERT(value >= 0);

  // Check for overflow - file too large for 32-bit size
  // UINT32_MAX = 4,294,967,295 = ~4GB
  DEV_ASSERT(value <= (int64)0xFFFFFFFF);

  return (uint32)value;
}

// ═══════════════════════════════════════════════════════════════════════════
// RESULT HELPERS
// ═══════════════════════════════════════════════════════════════════════════

file_scoped_fn DebugFileReadResult make_read_error(DebugFileErrorCode code) {
  DebugFileReadResult result = {0};
  result.size = 0;
  result.error_code = code;
  return result;
}

file_scoped_fn DebugFileWriteResult make_write_error(DebugFileErrorCode code) {
  DebugFileWriteResult result = {0};
  result.success = false;
  result.error_code = code;
  return result;
}

file_scoped_fn DebugFileWriteResult make_write_success(void) {
  DebugFileWriteResult result = {0};
  result.success = true;
  result.error_code = DEBUG_FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// READ ENTIRE FILE
// ═══════════════════════════════════════════════════════════════════════════

DebugFileReadResult debug_platform_read_entire_file(const char *filename) {
  DebugFileReadResult result = {0};

  // ─────────────────────────────────────────────────────────────────────
  // Validate input
  // ─────────────────────────────────────────────────────────────────────
  if (!filename || filename[0] == '\0') {
    SET_ERROR_DETAIL("[debug_read] NULL or empty filename");
    return make_read_error(DEBUG_FILE_ERROR_NULL_PATH);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Check if file exists using file.h API
  // ─────────────────────────────────────────────────────────────────────
  FileExistsResult exists_result = file_exists(filename);
  if (!exists_result.success) {
    SET_ERROR_DETAIL("[debug_read] file_exists() failed for '%s': %s", filename,
                     file_strerror(exists_result.error_code));
    return make_read_error(DEBUG_FILE_ERROR_NOT_FOUND);
  }

  if (!exists_result.exists) {
    SET_ERROR_DETAIL("[debug_read] File not found: '%s'", filename);
    return make_read_error(DEBUG_FILE_ERROR_NOT_FOUND);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Get file size using file.h API
  // ─────────────────────────────────────────────────────────────────────
  FileSizeResult size_result = file_get_size(filename);
  if (!size_result.success) {
    SET_ERROR_DETAIL("[debug_read] file_get_size() failed for '%s': %s",
                     filename, file_strerror(size_result.error_code));
    return make_read_error(DEBUG_FILE_ERROR_READ_FAILED);
  }

  if (size_result.value <= 0) {
    SET_ERROR_DETAIL("[debug_read] File is empty: '%s'", filename);
    return make_read_error(DEBUG_FILE_ERROR_EMPTY_FILE);
  }

  // Check for 32-bit overflow (debug I/O limited to 4GB)
  if (size_result.value > (int64)0xFFFFFFFF) {
    SET_ERROR_DETAIL("[debug_read] File too large: '%s' (%lld bytes, max 4GB)",
                     filename, (long long)size_result.value);
    return make_read_error(DEBUG_FILE_ERROR_TOO_LARGE);
  }

  size_t file_size = (size_t)size_result.value;

  // ─────────────────────────────────────────────────────────────────────
  // Allocate memory using memory.h API
  // ─────────────────────────────────────────────────────────────────────
  result.memory =
      memory_alloc(NULL, file_size,
                   MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_ZEROED);

  if (!memory_is_valid(result.memory)) {
    SET_ERROR_DETAIL(
        "[debug_read] memory_alloc() failed for '%s': %s (requested %zu bytes)",
        filename, memory_error_str(result.memory.error_code), file_size);
    return make_read_error(DEBUG_FILE_ERROR_MEMORY_ALLOC);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Open and read file (stdio for simplicity - no file_read() in file.h)
  // ─────────────────────────────────────────────────────────────────────
  FILE *file = fopen(filename, "rb");
  if (!file) {
    SET_ERROR_DETAIL("[debug_read] fopen() failed for '%s': %s", filename,
                     strerror(errno));
    memory_free(&result.memory);
    return make_read_error(DEBUG_FILE_ERROR_OPEN_FAILED);
  }

  size_t bytes_read = fread(result.memory.base, 1, file_size, file);

  if (bytes_read != file_size) {
    SET_ERROR_DETAIL(
        "[debug_read] fread() failed for '%s': expected %zu, got %zu (%s)",
        filename, file_size, bytes_read,
        ferror(file) ? strerror(errno) : "unexpected EOF");
    fclose(file);
    memory_free(&result.memory);
    return make_read_error(DEBUG_FILE_ERROR_READ_FAILED);
  }

  fclose(file);

  // ─────────────────────────────────────────────────────────────────────
  // Success
  // ─────────────────────────────────────────────────────────────────────
  result.size = safe_truncate_uint64((int64)file_size);
  result.error_code = DEBUG_FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();

#if DE100_SLOW
  fprintf(stderr, "[DEBUG FILE] Loaded '%s' (%u bytes)\n", filename,
          result.size);
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// FREE FILE MEMORY
// ═══════════════════════════════════════════════════════════════════════════

void debug_platform_free_file_memory(MemoryBlock *memory) {
  if (!memory) {
    return;
  }

  // Use memory.h API
  if (memory->base && memory->is_valid) {
    MemoryError err = memory_free(memory);
    (void)err; // Ignore error in debug cleanup
  }

  // Ensure it's marked as freed
  memory->base = NULL;
  memory->is_valid = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// WRITE ENTIRE FILE
// ═══════════════════════════════════════════════════════════════════════════

DebugFileWriteResult debug_platform_write_entire_file(const char *filename,
                                                      uint32 size,
                                                      const void *data) {
  // ─────────────────────────────────────────────────────────────────────
  // Validate input
  // ─────────────────────────────────────────────────────────────────────
  if (!filename || filename[0] == '\0') {
    SET_ERROR_DETAIL("[debug_write] NULL or empty filename");
    return make_write_error(DEBUG_FILE_ERROR_NULL_PATH);
  }

  if (!data && size > 0) {
    SET_ERROR_DETAIL("[debug_write] NULL data with size=%u", size);
    return make_write_error(DEBUG_FILE_ERROR_NULL_DATA);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Open file for writing (stdio - no file_write() in file.h)
  // ─────────────────────────────────────────────────────────────────────
  FILE *file = fopen(filename, "wb");
  if (!file) {
    SET_ERROR_DETAIL("[debug_write] fopen() failed for '%s': %s", filename,
                     strerror(errno));
    return make_write_error(DEBUG_FILE_ERROR_OPEN_FAILED);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Write data
  // ─────────────────────────────────────────────────────────────────────
  if (size > 0) {
    size_t bytes_written = fwrite(data, 1, size, file);

    if (bytes_written != size) {
      SET_ERROR_DETAIL(
          "[debug_write] fwrite() failed for '%s': expected %u, wrote %zu (%s)",
          filename, size, bytes_written, strerror(errno));
      fclose(file);
      return make_write_error(DEBUG_FILE_ERROR_WRITE_FAILED);
    }
  }

  // ─────────────────────────────────────────────────────────────────────
  // Flush and close
  // ─────────────────────────────────────────────────────────────────────
  if (fflush(file) != 0) {
    SET_ERROR_DETAIL("[debug_write] fflush() failed for '%s': %s", filename,
                     strerror(errno));
    fclose(file);
    return make_write_error(DEBUG_FILE_ERROR_WRITE_FAILED);
  }

  fclose(file);

#if DE100_SLOW
  fprintf(stderr, "[DEBUG FILE] Wrote '%s' (%u bytes)\n", filename, size);
#endif

  return make_write_success();
}

#endif // DE100_INTERNAL
