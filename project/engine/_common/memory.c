#include "memory.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Platform-specific includes
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

size_t platform_get_page_size(void) {
#if defined(_WIN32)
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return (size_t)info.dwPageSize;
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size < 0) {
    return 0;
  }
  return (size_t)page_size;
#endif
}

#if defined(_WIN32)
/**
 * Convert Windows error code to our memory error code.
 */
file_scoped_fn PlatformMemoryError
win32_error_to_memory_error(DWORD error_code) {
  switch (error_code) {
  case ERROR_NOT_ENOUGH_MEMORY:
  case ERROR_OUTOFMEMORY:
  case ERROR_COMMITMENT_LIMIT:
    return PLATFORM_MEMORY_ERROR_OUT_OF_MEMORY;

  case ERROR_INVALID_ADDRESS:
  case ERROR_INVALID_PARAMETER:
    return PLATFORM_MEMORY_ERROR_INVALID_ADDRESS;

  case ERROR_ACCESS_DENIED:
    return PLATFORM_MEMORY_ERROR_PERMISSION_DENIED;

  case ERROR_ALREADY_EXISTS:
    return PLATFORM_MEMORY_ERROR_ADDRESS_IN_USE;

  default:
    return PLATFORM_MEMORY_ERROR_UNKNOWN;
  }
}

/**
 * Get Windows error message.
 */
file_scoped_fn void win32_get_error_message(char *buffer, size_t buffer_size,
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

/**
 * Convert memory flags to Windows protection flags.
 */
file_scoped_fn DWORD win32_protection_from_flags(PlatformMemoryFlags flags) {
  int r = (flags & PLATFORM_MEMORY_READ) != 0;
  int w = (flags & PLATFORM_MEMORY_WRITE) != 0;
  int x = (flags & PLATFORM_MEMORY_EXECUTE) != 0;

  if (x) {
    if (w)
      return PAGE_EXECUTE_READWRITE;
    if (r)
      return PAGE_EXECUTE_READ;
    return PAGE_EXECUTE;
  } else {
    if (w)
      return PAGE_READWRITE;
    if (r)
      return PAGE_READONLY;
  }

  return PAGE_NOACCESS;
}

#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
/**
 * Convert POSIX errno to our memory error code.
 */
file_scoped_fn PlatformMemoryError errno_to_memory_error(int err) {
  switch (err) {
  case ENOMEM:
    return PLATFORM_MEMORY_ERROR_OUT_OF_MEMORY;

  case EINVAL:
    return PLATFORM_MEMORY_ERROR_INVALID_ADDRESS;

  case EACCES:
  case EPERM:
    return PLATFORM_MEMORY_ERROR_PERMISSION_DENIED;

  case EEXIST:
    return PLATFORM_MEMORY_ERROR_ADDRESS_IN_USE;

  default:
    return PLATFORM_MEMORY_ERROR_UNKNOWN;
  }
}

/**
 * Convert memory flags to POSIX protection flags.
 */
file_scoped_fn int posix_protection_from_flags(PlatformMemoryFlags flags) {
  int prot = PROT_NONE;

  if (flags & PLATFORM_MEMORY_READ)
    prot |= PROT_READ;
  if (flags & PLATFORM_MEMORY_WRITE)
    prot |= PROT_WRITE;
  if (flags & PLATFORM_MEMORY_EXECUTE)
    prot |= PROT_EXEC;

  return prot;
}
#endif

// ═══════════════════════════════════════════════════════════════════════════
// ALLOCATE MEMORY
// ═══════════════════════════════════════════════════════════════════════════

PlatformMemoryBlock platform_allocate_memory(void *base_hint, size_t size,
                                             PlatformMemoryFlags flags) {
  PlatformMemoryBlock result = {0};

  // ─────────────────────────────────────────────────────────────────────
  // Validate input
  // ─────────────────────────────────────────────────────────────────────

  if (size == 0) {
    result.is_valid = false;
    result.error_code = PLATFORM_MEMORY_ERROR_INVALID_SIZE;
    snprintf(result.error_message, sizeof(result.error_message),
             "Invalid size: 0 bytes requested");
    return result;
  }

  size_t page_size = platform_get_page_size();
  if (page_size == 0) {
    result.is_valid = false;
    result.error_code = PLATFORM_MEMORY_ERROR_UNKNOWN;
    snprintf(result.error_message, sizeof(result.error_message),
             "Failed to get system page size");
    return result;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Calculate sizes
  // ─────────────────────────────────────────────────────────────────────

  // Align size to page boundary
  size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);

  // Total size includes guard pages (one before, one after)
  size_t total_size = aligned_size + 2 * page_size;

  // Check for overflow
  if (total_size < aligned_size) {
    result.is_valid = false;
    result.error_code = PLATFORM_MEMORY_ERROR_INVALID_SIZE;
    snprintf(result.error_message, sizeof(result.error_message),
             "Size too large: would overflow (requested: %zu bytes)", size);
    return result;
  }

#if defined(_WIN32)
  // ═════════════════════════════════════════════════════════════════════
  // WINDOWS IMPLEMENTATION
  // ═════════════════════════════════════════════════════════════════════

  DWORD protect = win32_protection_from_flags(flags);

  void *request_base = NULL;
  if (flags & (PLATFORM_MEMORY_BASE_FIXED | PLATFORM_MEMORY_BASE_HINT)) {
    request_base = base_hint;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Reserve address space
  // ─────────────────────────────────────────────────────────────────────

  void *reserved =
      VirtualAlloc(request_base, total_size, MEM_RESERVE, PAGE_NOACCESS);

  if (!reserved && (flags & PLATFORM_MEMORY_BASE_HINT)) {
    // If hint failed, try without specific address
    reserved = VirtualAlloc(NULL, total_size, MEM_RESERVE, PAGE_NOACCESS);
  }

  if (!reserved) {
    DWORD error_code = GetLastError();
    result.is_valid = false;
    result.error_code = win32_error_to_memory_error(error_code);

    char sys_msg[256];
    win32_get_error_message(sys_msg, sizeof(sys_msg), error_code);
    snprintf(result.error_message, sizeof(result.error_message),
             "VirtualAlloc(RESERVE) failed for %zu bytes: %s", total_size,
             sys_msg);
    return result;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Commit usable region (skip first guard page)
  // ─────────────────────────────────────────────────────────────────────

  void *committed = VirtualAlloc((uint8 *)reserved + page_size, aligned_size,
                                 MEM_COMMIT, protect);

  if (!committed) {
    DWORD error_code = GetLastError();
    VirtualFree(reserved, 0, MEM_RELEASE);

    result.is_valid = false;
    result.error_code = win32_error_to_memory_error(error_code);

    char sys_msg[256];
    win32_get_error_message(sys_msg, sizeof(sys_msg), error_code);
    snprintf(result.error_message, sizeof(result.error_message),
             "VirtualAlloc(COMMIT) failed for %zu bytes: %s", aligned_size,
             sys_msg);
    return result;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Zero memory if requested
  // ─────────────────────────────────────────────────────────────────────

  if (flags & PLATFORM_MEMORY_ZEROED) {
    ZeroMemory(committed, aligned_size);
  }

  // ─────────────────────────────────────────────────────────────────────
  // Success
  // ─────────────────────────────────────────────────────────────────────

  result.base = committed;
  result.size = aligned_size;
  result.total_size = total_size;
  result.flags = flags;
  result.is_valid = true;
  result.error_code = PLATFORM_MEMORY_SUCCESS;
  snprintf(result.error_message, sizeof(result.error_message),
           "Successfully allocated %zu bytes (%zu total with guards) at %p",
           aligned_size, total_size, committed);

#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
  // ═════════════════════════════════════════════════════════════════════
  // POSIX IMPLEMENTATION (Linux, macOS, BSD, etc.)
  // ═════════════════════════════════════════════════════════════════════

  int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
  if (flags & PLATFORM_MEMORY_BASE_FIXED) {
    mmap_flags |= MAP_FIXED;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Reserve address space with no access (guard pages included)
  // ─────────────────────────────────────────────────────────────────────

  void *reserved = mmap(base_hint, total_size, PROT_NONE, mmap_flags, -1, 0);

  if (reserved == MAP_FAILED) {
    result.is_valid = false;
    result.error_code = errno_to_memory_error(errno);
    snprintf(result.error_message, sizeof(result.error_message),
             "mmap failed for %zu bytes: %s", total_size, strerror(errno));
    return result;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Set protection on usable region (skip first guard page)
  // ─────────────────────────────────────────────────────────────────────

  int prot = posix_protection_from_flags(flags);
  if (mprotect((uint8 *)reserved + page_size, aligned_size, prot) != 0) {
    int saved_errno = errno;
    munmap(reserved, total_size);

    result.is_valid = false;
    result.error_code = errno_to_memory_error(saved_errno);
    snprintf(result.error_message, sizeof(result.error_message),
             "mprotect failed for %zu bytes: %s", aligned_size,
             strerror(saved_errno));
    return result;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Verify zero-initialization (debug builds only)
  // ─────────────────────────────────────────────────────────────────────
  // On Linux/POSIX, mmap with MAP_ANONYMOUS guarantees zero-initialized pages

#if defined(DE100_SLOW) && DE100_SLOW
  if (flags & PLATFORM_MEMORY_ZEROED) {
    uint8 *base = (uint8 *)reserved + page_size;
    size_t offsets[] = {0, aligned_size / 4, aligned_size / 2,
                        3 * aligned_size / 4, aligned_size - 1};

    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
      if (base[offsets[i]] != 0) {
        munmap(reserved, total_size);
        result.is_valid = false;
        result.error_code = PLATFORM_MEMORY_ERROR_UNKNOWN;
        snprintf(result.error_message, sizeof(result.error_message),
                 "Memory not zero-initialized at offset %zu", offsets[i]);
        return result;
      }
    }
  }
#endif

  // ─────────────────────────────────────────────────────────────────────
  // Success
  // ─────────────────────────────────────────────────────────────────────

  result.base = (uint8 *)reserved + page_size;
  result.size = aligned_size;
  result.total_size = total_size;
  result.flags = flags;
  result.is_valid = true;
  result.error_code = PLATFORM_MEMORY_SUCCESS;
  snprintf(result.error_message, sizeof(result.error_message),
           "Successfully allocated %zu bytes (%zu total with guards) at %p",
           aligned_size, total_size, result.base);
#endif

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// FREE MEMORY
// ═══════════════════════════════════════════════════════════════════════════

PlatformMemoryError platform_free_memory(PlatformMemoryBlock *block) {
  // ─────────────────────────────────────────────────────────────────────
  // Validate input
  // ─────────────────────────────────────────────────────────────────────

  if (!block) {
    return PLATFORM_MEMORY_ERROR_INVALID_BLOCK;
  }

  // Already freed or never allocated - idempotent operation
  if (!block->base || !block->is_valid) {
    block->base = NULL;
    block->is_valid = false;
    block->error_code = PLATFORM_MEMORY_SUCCESS;
    snprintf(block->error_message, sizeof(block->error_message),
             "Block already freed or never allocated");
    return PLATFORM_MEMORY_SUCCESS;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Calculate reserved base address (before first guard page)
  // ─────────────────────────────────────────────────────────────────────

  size_t page_size = platform_get_page_size();
  if (page_size == 0) {
    block->error_code = PLATFORM_MEMORY_ERROR_UNKNOWN;
    snprintf(block->error_message, sizeof(block->error_message),
             "Failed to get system page size during free");
    return PLATFORM_MEMORY_ERROR_UNKNOWN;
  }

  uint8 *reserved_base = (uint8 *)block->base - page_size;

#if defined(_WIN32)
  // ═════════════════════════════════════════════════════════════════════
  // WINDOWS IMPLEMENTATION
  // ═════════════════════════════════════════════════════════════════════

  if (!VirtualFree(reserved_base, 0, MEM_RELEASE)) {
    DWORD error_code = GetLastError();
    block->error_code = win32_error_to_memory_error(error_code);

    char sys_msg[256];
    win32_get_error_message(sys_msg, sizeof(sys_msg), error_code);
    snprintf(block->error_message, sizeof(block->error_message),
             "VirtualFree failed for %zu bytes at %p: %s", block->total_size,
             (void *)reserved_base, sys_msg);

    return block->error_code;
  }

#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
  // ═════════════════════════════════════════════════════════════════════
  // POSIX IMPLEMENTATION
  // ═════════════════════════════════════════════════════════════════════

  if (munmap(reserved_base, block->total_size) != 0) {
    block->error_code = errno_to_memory_error(errno);
    snprintf(block->error_message, sizeof(block->error_message),
             "munmap failed for %zu bytes at %p: %s", block->total_size,
             (void *)reserved_base, strerror(errno));

    return block->error_code;
  }
#endif

  // ─────────────────────────────────────────────────────────────────────
  // Mark block as freed
  // ─────────────────────────────────────────────────────────────────────

  block->base = NULL;
  block->is_valid = false;
  block->error_code = PLATFORM_MEMORY_SUCCESS;
  snprintf(block->error_message, sizeof(block->error_message),
           "Successfully freed %zu bytes", block->total_size);

  return PLATFORM_MEMORY_SUCCESS;
}

// ═══════════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

const char *platform_memory_strerror(PlatformMemoryError error) {
  switch (error) {
  case PLATFORM_MEMORY_SUCCESS:
    return "Success";
  case PLATFORM_MEMORY_ERROR_OUT_OF_MEMORY:
    return "Out of memory or address space exhausted";
  case PLATFORM_MEMORY_ERROR_INVALID_SIZE:
    return "Invalid size parameter";
  case PLATFORM_MEMORY_ERROR_INVALID_ADDRESS:
    return "Invalid address or alignment";
  case PLATFORM_MEMORY_ERROR_PERMISSION_DENIED:
    return "Permission denied or insufficient privileges";
  case PLATFORM_MEMORY_ERROR_ADDRESS_IN_USE:
    return "Address already in use";
  case PLATFORM_MEMORY_ERROR_ALIGNMENT_FAILED:
    return "Memory alignment failed";
  case PLATFORM_MEMORY_ERROR_PROTECTION_FAILED:
    return "Failed to set memory protection";
  case PLATFORM_MEMORY_ERROR_INVALID_BLOCK:
    return "Invalid memory block";
  case PLATFORM_MEMORY_ERROR_ALREADY_FREED:
    return "Memory already freed";
  case PLATFORM_MEMORY_ERROR_UNKNOWN:
  default:
    return "Unknown error";
  }
}

bool platform_memory_is_valid(PlatformMemoryBlock block) {
  return block.is_valid && block.base != NULL &&
         block.error_code == PLATFORM_MEMORY_SUCCESS;
}

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Fill memory with a constant byte value.
 *
 * @param dest Destination memory address
 * @param value Byte value to fill (converted to unsigned char)
 * @param size Number of bytes to fill
 * @return Pointer to dest, or NULL if dest is NULL
 *
 * @note This is a secure implementation that won't be optimized away
 *       by the compiler, making it suitable for clearing sensitive data.
 */
void *platform_memset(void *dest, int value, size_t size) {
  if (!dest || size == 0) {
    return dest;
  }

#if defined(_WIN32)
  // Windows provides SecureZeroMemory for security-critical zeroing
  // For general memset, use FillMemory or standard approach
  if (value == 0) {
    // Use volatile to prevent compiler optimization
    volatile unsigned char *p = (volatile unsigned char *)dest;
    while (size--) {
      *p++ = 0;
    }
  } else {
    FillMemory(dest, size, (BYTE)value);
  }
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
  // POSIX: Use explicit_bzero for zeroing (glibc 2.25+, BSD)
  // or memset_s (C11 Annex K) where available
  if (value == 0) {
#if defined(__GLIBC__) && defined(_DEFAULT_SOURCE) &&                          \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
    explicit_bzero(dest, size);
#elif defined(__APPLE__) || defined(__FreeBSD__)
    memset_s(dest, size, 0, size);
#else
    // Fallback: volatile pointer to prevent optimization
    volatile unsigned char *p = (volatile unsigned char *)dest;
    while (size--) {
      *p++ = 0;
    }
#endif
  } else {
    memset(dest, value, size);
  }
#else
  // Generic fallback
  memset(dest, value, size);
#endif

  return dest;
}

/**
 * @brief Securely zero memory (guaranteed not to be optimized away).
 *
 * @param dest Destination memory address
 * @param size Number of bytes to zero
 * @return Pointer to dest, or NULL if dest is NULL
 */
void *platform_secure_zero(void *dest, size_t size) {
  if (!dest || size == 0) {
    return dest;
  }

#if defined(_WIN32)
  SecureZeroMemory(dest, size);
#elif defined(__APPLE__) || defined(__FreeBSD__)
  memset_s(dest, size, 0, size);
#elif defined(__GLIBC__) && defined(_DEFAULT_SOURCE) &&                        \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
  explicit_bzero(dest, size);
#else
  // Volatile pointer prevents compiler from optimizing this away
  volatile unsigned char *p = (volatile unsigned char *)dest;
  while (size--) {
    *p++ = 0;
  }
  // Memory barrier to ensure writes complete
  __asm__ __volatile__("" : : "r"(dest) : "memory");
#endif

  return dest;
}

/**
 * @brief Copy memory from source to destination.
 *
 * @param dest Destination memory address
 * @param src Source memory address
 * @param size Number of bytes to copy
 * @return Pointer to dest, or NULL on error
 *
 * @note Behavior is undefined if regions overlap. Use platform_memmove for
 *       overlapping regions.
 */
void *platform_memcpy(void *dest, const void *src, size_t size) {
  if (!dest || !src || size == 0) {
    return dest;
  }

#if defined(_WIN32)
  CopyMemory(dest, src, size);
  return dest;
#else
  return memcpy(dest, src, size);
#endif
}

/**
 * @brief Copy memory, handling overlapping regions correctly.
 *
 * @param dest Destination memory address
 * @param src Source memory address
 * @param size Number of bytes to copy
 * @return Pointer to dest, or NULL on error
 */
void *platform_memmove(void *dest, const void *src, size_t size) {
  if (!dest || !src || size == 0) {
    return dest;
  }

#if defined(_WIN32)
  MoveMemory(dest, src, size);
  return dest;
#else
  return memmove(dest, src, size);
#endif
}
