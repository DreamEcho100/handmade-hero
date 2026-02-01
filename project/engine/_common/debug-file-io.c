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

#endif // DE100_SLOW

// ═══════════════════════════════════════════════════════════════════════════
// ERROR STRING TRANSLATION
// ═══════════════════════════════════════════════════════════════════════════

const char *
de100_debug_platform_de100_file_strerror(De100DebugDe100FileErrorCode code) {
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

  case DEBUG_FILE_ERROR_De100_MEMORY_ALLOC:
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
const char *debug_de100_file_get_last_error_detail(void) {
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

de100_file_scoped_fn De100DebugDe100FileReadResult
make_read_error(De100DebugDe100FileErrorCode code) {
  De100DebugDe100FileReadResult result = {0};
  result.size = 0;
  result.error_code = code;
  return result;
}

de100_file_scoped_fn De100DebugFileWriteResult
make_write_error(De100DebugDe100FileErrorCode code) {
  De100DebugFileWriteResult result = {0};
  result.success = false;
  result.error_code = code;
  return result;
}

de100_file_scoped_fn De100DebugFileWriteResult make_write_success(void) {
  De100DebugFileWriteResult result = {0};
  result.success = true;
  result.error_code = DEBUG_FILE_SUCCESS;
  CLEAR_ERROR_DETAIL();
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// READ ENTIRE FILE
// ═══════════════════════════════════════════════════════════════════════════

De100DebugDe100FileReadResult
de100_debug_platform_read_entire_file(const char *filename) {
  De100DebugDe100FileReadResult result = {0};

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
  De100FileExistsResult exists_result = de100_file_exists(filename);
  if (!exists_result.success) {
    SET_ERROR_DETAIL("[debug_read] de100_file_exists() failed for '%s': %s",
                     filename, de100_file_strerror(exists_result.error_code));
    return make_read_error(DEBUG_FILE_ERROR_NOT_FOUND);
  }

  if (!exists_result.exists) {
    SET_ERROR_DETAIL("[debug_read] File not found: '%s'", filename);
    return make_read_error(DEBUG_FILE_ERROR_NOT_FOUND);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Get file size using file.h API
  // ─────────────────────────────────────────────────────────────────────
  De100FileSizeResult size_result = de100_file_get_size(filename);
  if (!size_result.success) {
    SET_ERROR_DETAIL("[debug_read] de100_file_get_size() failed for '%s': %s",
                     filename, de100_file_strerror(size_result.error_code));
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

  size_t de100_file_size = (size_t)size_result.value;

  // ─────────────────────────────────────────────────────────────────────
  // Allocate memory using memory.h API
  // ─────────────────────────────────────────────────────────────────────
  result.memory =
      de100_memory_alloc(NULL, de100_file_size,
                         De100_MEMORY_FLAG_READ | De100_MEMORY_FLAG_WRITE |
                             De100_MEMORY_FLAG_ZEROED);

  if (!de100_memory_is_valid(result.memory)) {
    SET_ERROR_DETAIL("[debug_read] de100_memory_alloc() failed for '%s': %s "
                     "(requested %zu bytes)",
                     filename, de100_memory_error_str(result.memory.error_code),
                     de100_file_size);
    return make_read_error(DEBUG_FILE_ERROR_De100_MEMORY_ALLOC);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Open and read file (stdio for simplicity - no de100_file_read() in file.h)
  // ─────────────────────────────────────────────────────────────────────
  FILE *file = fopen(filename, "rb");
  if (!file) {
    SET_ERROR_DETAIL("[debug_read] fopen() failed for '%s': %s", filename,
                     strerror(errno));
    de100_memory_free(&result.memory);
    return make_read_error(DEBUG_FILE_ERROR_OPEN_FAILED);
  }

  size_t bytes_read = fread(result.memory.base, 1, de100_file_size, file);

  if (bytes_read != de100_file_size) {
    SET_ERROR_DETAIL(
        "[debug_read] fread() failed for '%s': expected %zu, got %zu (%s)",
        filename, de100_file_size, bytes_read,
        ferror(file) ? strerror(errno) : "unexpected EOF");
    fclose(file);
    de100_memory_free(&result.memory);
    return make_read_error(DEBUG_FILE_ERROR_READ_FAILED);
  }

  fclose(file);

  // ─────────────────────────────────────────────────────────────────────
  // Success
  // ─────────────────────────────────────────────────────────────────────
  result.size = safe_truncate_uint64((int64)de100_file_size);
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

void de100_debug_platform_free_de100_file_memory(De100MemoryBlock *memory) {
  if (!memory) {
    return;
  }

  // Use memory.h API
  if (memory->base && memory->is_valid) {
    De100MemoryError err = de100_memory_free(memory);
    (void)err; // Ignore error in debug cleanup
  }

  // Ensure it's marked as freed
  memory->base = NULL;
  memory->is_valid = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// WRITE ENTIRE FILE
// ═══════════════════════════════════════════════════════════════════════════

De100DebugFileWriteResult
de100_debug_platform_write_entire_file(const char *filename, uint32 size,
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
  // Open file for writing (stdio - no de100_file_write() in file.h)
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
