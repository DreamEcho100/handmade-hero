#include "./replay-buffer.h"
#include "../../_common/file.h"
#include "../../_common/memory.h"

#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM-SPECIFIC INCLUDES (Only for mmap - no abstraction exists yet)
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <io.h> // For _get_osfhandle
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#error "Unsupported platform for replay buffer"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// ERROR MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_global_var const char *g_replay_buffer_error_messages[] = {
    [REPLAY_BUFFER_SUCCESS] = "Success",
    [REPLAY_BUFFER_ERROR_NULL_STATE] = "NULL state or buffers pointer",
    [REPLAY_BUFFER_ERROR_INVALID_SLOT] = "Invalid slot index",
    [REPLAY_BUFFER_ERROR_NO_GAME_MEMORY] =
        "Game memory not allocated or size is zero",
    [REPLAY_BUFFER_ERROR_FILE_CREATE_FAILED] =
        "Failed to create replay buffer file",
    [REPLAY_BUFFER_ERROR_FILE_RESIZE_FAILED] =
        "Failed to resize replay buffer file",
    [REPLAY_BUFFER_ERROR_MMAP_FAILED] =
        "Failed to memory-map replay buffer file",
    [REPLAY_BUFFER_ERROR_BUFFER_NOT_VALID] = "Replay buffer is not valid",
    [REPLAY_BUFFER_ERROR_SAVE_FAILED] = "Failed to save state to replay buffer",
    [REPLAY_BUFFER_ERROR_RESTORE_FAILED] =
        "Failed to restore state from replay buffer",
};

const char *replay_buffer_strerror(ReplayBufferErrorCode code) {
  if (code >= 0 && code < REPLAY_BUFFER_ERROR_COUNT) {
    return g_replay_buffer_error_messages[code];
  }
  return "Unknown replay buffer error";
}

// ═══════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline void get_state_filename(const char *exe_directory,
                                                    int32 slot_index,
                                                    char *buffer,
                                                    size_t buffer_size) {
  snprintf(buffer, buffer_size, "%sloop_edit_%d_state.hmi", exe_directory,
           slot_index);
}

de100_file_scoped_fn inline ReplayBufferResult
make_result(bool success, ReplayBufferErrorCode code) {
  return (ReplayBufferResult){.success = success, .error_code = code};
}

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM-SPECIFIC MMAP WRAPPERS
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)

/**
 * Windows memory-mapping implementation.
 * Uses CreateFileMapping + MapViewOfFile.
 */
de100_file_scoped_fn void *
platform_mmap_file(int32 fd, uint64 size, ReplayBufferErrorCode *out_error) {
  *out_error = REPLAY_BUFFER_SUCCESS;

  // Get Windows HANDLE from file descriptor
  HANDLE file_handle = (HANDLE)_get_osfhandle(fd);
  if (file_handle == INVALID_HANDLE_VALUE) {
    *out_error = REPLAY_BUFFER_ERROR_MMAP_FAILED;
    return NULL;
  }

  // Create file mapping
  LARGE_INTEGER map_size;
  map_size.QuadPart = (LONGLONG)size;

  HANDLE mapping = CreateFileMappingA(file_handle,
                                      NULL,           // Default security
                                      PAGE_READWRITE, // Read/write access
                                      map_size.HighPart, map_size.LowPart,
                                      NULL // No name
  );

  if (!mapping) {
    *out_error = REPLAY_BUFFER_ERROR_MMAP_FAILED;
    return NULL;
  }

  // Map view of file
  void *ptr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, // Offset
                            (SIZE_T)size);

  // Close mapping handle (view keeps it alive)
  CloseHandle(mapping);

  if (!ptr) {
    *out_error = REPLAY_BUFFER_ERROR_MMAP_FAILED;
    return NULL;
  }

  return ptr;
}

de100_file_scoped_fn void platform_munmap_file(void *ptr, uint64 size) {
  (void)size; // Windows doesn't need size for unmap
  if (ptr) {
    UnmapViewOfFile(ptr);
  }
}

de100_file_scoped_fn bool platform_file_resize(int32 fd, uint64 size) {
  HANDLE file_handle = (HANDLE)_get_osfhandle(fd);
  if (file_handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  LARGE_INTEGER li;
  li.QuadPart = (LONGLONG)size;

  if (!SetFilePointerEx(file_handle, li, NULL, FILE_BEGIN)) {
    return false;
  }

  return SetEndOfFile(file_handle) != 0;
}

#else // POSIX

/**
 * POSIX memory-mapping implementation.
 * Uses mmap directly.
 */
de100_file_scoped_fn void *
platform_mmap_file(int32 fd, uint64 size, ReplayBufferErrorCode *out_error) {
  *out_error = REPLAY_BUFFER_SUCCESS;

  void *ptr = mmap(NULL, // Let OS choose address
                   (size_t)size, PROT_READ | PROT_WRITE,
                   MAP_SHARED, // Changes written to file
                   fd,
                   0 // Offset
  );

  if (ptr == MAP_FAILED) {
#if DE100_INTERNAL && DE100_SLOW
    fprintf(stderr, "[REPLAY BUFFER] mmap failed: %s\n", strerror(errno));
#endif
    *out_error = REPLAY_BUFFER_ERROR_MMAP_FAILED;
    return NULL;
  }

  return ptr;
}

de100_file_scoped_fn void platform_munmap_file(void *ptr, uint64 size) {
  if (ptr && ptr != MAP_FAILED) {
    munmap(ptr, (size_t)size);
  }
}

de100_file_scoped_fn bool platform_file_resize(int32 fd, uint64 size) {
  if (ftruncate(fd, (off_t)size) != 0) {
#if DE100_INTERNAL && DE100_SLOW
    fprintf(stderr, "[REPLAY BUFFER] ftruncate failed: %s\n", strerror(errno));
#endif
    return false;
  }
  return true;
}

#endif // Platform selection

// ═══════════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

ReplayBufferInitResult replay_buffers_init(const char *exe_directory,
                                           void *game_memory, uint64 total_size,
                                           ReplayBuffer *out_buffers) {
  ReplayBufferInitResult result = {0};

  // ─────────────────────────────────────────────────────────────────────
  // Validate inputs
  // ─────────────────────────────────────────────────────────────────────

  if (!out_buffers) {
    result.error_code = REPLAY_BUFFER_ERROR_NULL_STATE;
    return result;
  }

  if (!game_memory || total_size == 0) {
    result.error_code = REPLAY_BUFFER_ERROR_NO_GAME_MEMORY;
    return result;
  }

  if (!exe_directory) {
    exe_directory = "./"; // Default to current directory
  }

#if DE100_INTERNAL
  printf("[REPLAY BUFFER] Initializing %d buffers (%.2f MB each)\n",
         MAX_REPLAY_BUFFERS, (double)total_size / (1024.0 * 1024.0));
#endif

  // ─────────────────────────────────────────────────────────────────────
  // Initialize each buffer
  // ─────────────────────────────────────────────────────────────────────

  for (int32 slot = 0; slot < MAX_REPLAY_BUFFERS; ++slot) {
    ReplayBuffer *buffer = &out_buffers[slot];

    // Initialize to invalid state
    buffer->file_fd = -1;
    buffer->memory_block = NULL;
    buffer->mapped_size = 0;
    buffer->is_valid = false;
    buffer->last_error = REPLAY_BUFFER_SUCCESS;
    buffer->filename[0] = '\0';

    // Generate filename
    get_state_filename(exe_directory, slot, buffer->filename,
                       sizeof(buffer->filename));

    // ─────────────────────────────────────────────────────────────────
    // Step 1: Create/open file using de100_file_open
    // ─────────────────────────────────────────────────────────────────

    De100FileOpenResult open_result = de100_file_open(
        buffer->filename, DE100_FILE_READ | DE100_FILE_WRITE |
                              DE100_FILE_CREATE | DE100_FILE_TRUNCATE);

    if (!open_result.success) {
#if DE100_INTERNAL && DE100_SLOW
      fprintf(stderr, "[REPLAY BUFFER] Failed to create '%s': %s\n",
              buffer->filename, de100_file_strerror(open_result.error_code));
#endif
      buffer->last_error = REPLAY_BUFFER_ERROR_FILE_CREATE_FAILED;
      continue;
    }

    buffer->file_fd = open_result.fd;

    // ─────────────────────────────────────────────────────────────────
    // Step 2: Resize file to required size
    // ─────────────────────────────────────────────────────────────────
    // CRITICAL: mmap requires the file to be the right size FIRST!
    // ─────────────────────────────────────────────────────────────────

    if (!platform_file_resize(buffer->file_fd, total_size)) {
      buffer->last_error = REPLAY_BUFFER_ERROR_FILE_RESIZE_FAILED;
      de100_file_close(buffer->file_fd);
      buffer->file_fd = -1;
      continue;
    }

    // ─────────────────────────────────────────────────────────────────
    // Step 3: Memory-map the file
    // ─────────────────────────────────────────────────────────────────

    ReplayBufferErrorCode mmap_error;
    buffer->memory_block =
        platform_mmap_file(buffer->file_fd, total_size, &mmap_error);

    if (!buffer->memory_block) {
      buffer->last_error = mmap_error;
      de100_file_close(buffer->file_fd);
      buffer->file_fd = -1;
      continue;
    }

    // ─────────────────────────────────────────────────────────────────
    // Success!
    // ─────────────────────────────────────────────────────────────────

    buffer->mapped_size = (size_t)total_size;
    buffer->is_valid = true;
    buffer->last_error = REPLAY_BUFFER_SUCCESS;
    result.buffers_initialized++;

#if DE100_INTERNAL
    printf("[REPLAY BUFFER] ✅ Slot %d ready: %s\n", slot, buffer->filename);
#endif
  }

  // ─────────────────────────────────────────────────────────────────────
  // Set overall result
  // ─────────────────────────────────────────────────────────────────────

  if (result.buffers_initialized > 0) {
    result.success = true;
    result.error_code = REPLAY_BUFFER_SUCCESS;
  } else {
    result.error_code = REPLAY_BUFFER_ERROR_MMAP_FAILED;
  }

#if DE100_INTERNAL
  if (result.success) {
    printf("[REPLAY BUFFER] ✅ %d/%d buffers initialized\n",
           result.buffers_initialized, MAX_REPLAY_BUFFERS);
  } else {
    printf("[REPLAY BUFFER] ❌ All buffers failed to initialize\n");
  }
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// SHUTDOWN
// ═══════════════════════════════════════════════════════════════════════════

void replay_buffers_shutdown(ReplayBuffer *buffers, uint64 total_size) {
  if (!buffers) {
    return;
  }

#if DE100_INTERNAL
  printf("[REPLAY BUFFER] Shutting down...\n");
#endif

  for (int32 slot = 0; slot < MAX_REPLAY_BUFFERS; ++slot) {
    ReplayBuffer *buffer = &buffers[slot];

    // Unmap memory
    if (buffer->memory_block) {
      platform_munmap_file(buffer->memory_block, total_size);
      buffer->memory_block = NULL;
    }

    // Close file
    if (buffer->file_fd >= 0) {
      de100_file_close(buffer->file_fd);
      buffer->file_fd = -1;
    }

    buffer->mapped_size = 0;
    buffer->is_valid = false;
  }

#if DE100_INTERNAL
  printf("[REPLAY BUFFER] ✅ Shutdown complete\n");
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// GET BUFFER
// ═══════════════════════════════════════════════════════════════════════════

ReplayBuffer *replay_buffer_get(ReplayBuffer *buffers, int32 slot_index) {
  if (!buffers) {
    return NULL;
  }

  if (slot_index < 0 || slot_index >= MAX_REPLAY_BUFFERS) {
#if DE100_INTERNAL && DE100_SLOW
    fprintf(stderr, "[REPLAY BUFFER] Invalid slot index: %d (max %d)\n",
            slot_index, MAX_REPLAY_BUFFERS - 1);
#endif
    return NULL;
  }

  return &buffers[slot_index];
}

// ═══════════════════════════════════════════════════════════════════════════
// SAVE STATE
// ═══════════════════════════════════════════════════════════════════════════

ReplayBufferResult replay_buffer_save_state(ReplayBuffer *buffer,
                                            const void *game_memory,
                                            uint64 total_size) {
  if (!buffer) {
    return make_result(false, REPLAY_BUFFER_ERROR_NULL_STATE);
  }

  if (!buffer->is_valid || !buffer->memory_block) {
    buffer->last_error = REPLAY_BUFFER_ERROR_BUFFER_NOT_VALID;
    return make_result(false, REPLAY_BUFFER_ERROR_BUFFER_NOT_VALID);
  }

  if (!game_memory || total_size == 0) {
    buffer->last_error = REPLAY_BUFFER_ERROR_NO_GAME_MEMORY;
    return make_result(false, REPLAY_BUFFER_ERROR_NO_GAME_MEMORY);
  }

  // ─────────────────────────────────────────────────────────────────────
  // THE MAGIC: Just a memcpy!
  // ─────────────────────────────────────────────────────────────────────
  // This copies from game memory to the memory-mapped region.
  // ~50-100ms for 1GB vs 2-5 seconds with file I/O.
  // ─────────────────────────────────────────────────────────────────────

  de100_mem_copy(buffer->memory_block, game_memory, (size_t)total_size);

#if DE100_INTERNAL
  printf("[REPLAY BUFFER] 📸 Saved state (%.2f MB)\n",
         (double)total_size / (1024.0 * 1024.0));
#endif

  buffer->last_error = REPLAY_BUFFER_SUCCESS;
  return make_result(true, REPLAY_BUFFER_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// RESTORE STATE
// ═══════════════════════════════════════════════════════════════════════════

ReplayBufferResult replay_buffer_restore_state(const ReplayBuffer *buffer,
                                               void *game_memory,
                                               uint64 total_size) {
  if (!buffer) {
    return make_result(false, REPLAY_BUFFER_ERROR_NULL_STATE);
  }

  if (!buffer->is_valid || !buffer->memory_block) {
    return make_result(false, REPLAY_BUFFER_ERROR_BUFFER_NOT_VALID);
  }

  if (!game_memory || total_size == 0) {
    return make_result(false, REPLAY_BUFFER_ERROR_NO_GAME_MEMORY);
  }

  // ─────────────────────────────────────────────────────────────────────
  // THE MAGIC: Just a memcpy!
  // ─────────────────────────────────────────────────────────────────────

  de100_mem_copy(game_memory, buffer->memory_block, (size_t)total_size);

#if DE100_INTERNAL
  printf("[REPLAY BUFFER] 🔄 Restored state (%.2f MB)\n",
         (double)total_size / (1024.0 * 1024.0));
#endif

  return make_result(true, REPLAY_BUFFER_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// VALIDITY CHECK
// ═══════════════════════════════════════════════════════════════════════════

bool replay_buffer_is_valid(const ReplayBuffer *buffer) {
  return buffer && buffer->is_valid && buffer->memory_block != NULL &&
         buffer->file_fd >= 0 && buffer->mapped_size > 0;
}
