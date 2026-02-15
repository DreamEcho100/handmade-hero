#include "memory.h"

#if DE100_INTERNAL && DE100_SLOW
#include <stdio.h>
#endif

// #if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||        \
//     defined(__unix__) || defined(__MACH__)
// #define _GNU_SOURCE
// #endif

#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM INCLUDES
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||      \
    defined(__unix__) || defined(__MACH__)
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#error "Unsupported platform"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// CACHED PAGE SIZE
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_global_var size_t g_page_size = 0;

size_t de100_memory_page_size(void) {
  if (g_page_size == 0) {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    g_page_size = (size_t)info.dwPageSize;
#elif defined(DE100_IS_GENERIC_POSIX)
    long ps = sysconf(_SC_PAGESIZE);
    g_page_size = (ps > 0) ? (size_t)ps : 4096;
#endif
  }
  return g_page_size;
}

// ═══════════════════════════════════════════════════════════════════════════
// ERROR MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_global_var const char *g_de100_memory_error_messages[] = {
    [De100_MEMORY_OK] = "Success",
    [De100_MEMORY_ERR_OUT_OF_MEMORY] = "Out of memory",
    [De100_MEMORY_ERR_INVALID_SIZE] = "Invalid size (zero or negative)",
    [De100_MEMORY_ERR_SIZE_OVERFLOW] = "Size overflow (too large)",
    [De100_MEMORY_ERR_INVALID_ADDRESS] = "Invalid address",
    [De100_MEMORY_ERR_ADDRESS_IN_USE] = "Address already in use",
    [De100_MEMORY_ERR_ALIGNMENT_FAILED] = "Alignment failed",
    [De100_MEMORY_ERR_PERMISSION_DENIED] = "Permission denied",
    [De100_MEMORY_ERR_PROTECTION_FAILED] = "Failed to set memory protection",
    [De100_MEMORY_ERR_NULL_BLOCK] = "NULL block pointer",
    [De100_MEMORY_ERR_INVALID_BLOCK] =
        "Invalid block (corrupted or uninitialized)",
    [De100_MEMORY_ERR_ALREADY_FREED] = "Block already freed",
    [De100_MEMORY_ERR_PAGE_SIZE_FAILED] = "Failed to get system page size",
    [De100_MEMORY_ERR_PLATFORM_ERROR] = "Platform-specific error",
};

const char *de100_memory_error_str(De100MemoryError error) {
  if (error >= 0 && error < De100_MEMORY_ERR_COUNT) {
    return g_de100_memory_error_messages[error];
  }
  return "Unknown error";
}

// #if DE100_INTERNAL && DE100_SLOW
de100_file_scoped_global_var const char *g_de100_memory_error_details[] = {
    [De100_MEMORY_OK] = "Operation completed successfully.",

    [De100_MEMORY_ERR_OUT_OF_MEMORY] =
        "The system cannot allocate the requested memory.\n"
        "Possible causes:\n"
        "  - Physical RAM exhausted\n"
        "  - Virtual address space exhausted (32-bit process)\n"
        "  - Per-process memory limit reached\n"
        "  - System commit limit reached (Windows)\n"
        "Try: Reduce allocation size or free unused memory.",

    [De100_MEMORY_ERR_INVALID_SIZE] =
        "Size parameter is invalid.\n"
        "Requirements:\n"
        "  - Size must be > 0\n"
        "  - Size will be rounded up to page boundary\n"
        "Check: Ensure you're not passing 0 or a "
        "negative value cast to size_t.",

    [De100_MEMORY_ERR_SIZE_OVERFLOW] =
        "Size calculation overflowed.\n"
        "The requested size plus guard pages exceeds SIZE_MAX.\n"
        "This typically means you're requesting an impossibly large "
        "allocation.\n"
        "Check: Verify size calculation doesn't overflow before calling.",

    [De100_MEMORY_ERR_INVALID_ADDRESS] =
        "The base address hint is invalid.\n"
        "Possible causes:\n"
        "  - Address not page-aligned\n"
        "  - Address in reserved system range\n"
        "  - Address conflicts with existing mapping\n"
        "Try: Use NULL for base_hint to let the OS choose.",

    [De100_MEMORY_ERR_ADDRESS_IN_USE] =
        "The requested address range is already mapped.\n"
        "This occurs with De100_MEMORY_FLAG_BASE_FIXED when the address is "
        "taken.\n"
        "Try: Use De100_MEMORY_FLAG_BASE_HINT instead, or choose different "
        "address.",

    [De100_MEMORY_ERR_ALIGNMENT_FAILED] =
        "Failed to align memory to required boundary.\n"
        "This is rare and indicates a system issue.\n"
        "Check: Verify page size is a power of 2.",

    [De100_MEMORY_ERR_PERMISSION_DENIED] =
        "Permission denied for memory operation.\n"
        "Possible causes:\n"
        "  - SELinux/AppArmor blocking mmap\n"
        "  - Trying to allocate executable memory without permission\n"
        "  - System policy restricting memory allocation\n"
        "Try: Check system security policies.",

    [De100_MEMORY_ERR_PROTECTION_FAILED] =
        "Failed to set memory protection flags.\n"
        "The memory was allocated but mprotect/VirtualProtect failed.\n"
        "Possible causes:\n"
        "  - Requesting EXECUTE on non-executable memory policy\n"
        "  - System security restrictions\n"
        "Note: Memory has been freed to prevent partial allocation.",

    [De100_MEMORY_ERR_NULL_BLOCK] =
        "NULL pointer passed for block parameter.\n"
        "The block pointer itself is NULL, not the block's base.\n"
        "Check: Ensure you're passing &block, not block.base.",

    [De100_MEMORY_ERR_INVALID_BLOCK] =
        "Block structure is invalid or corrupted.\n"
        "Possible causes:\n"
        "  - Uninitialized De100MemoryBlock variable\n"
        "  - Block was corrupted by buffer overflow\n"
        "  - Block from different allocator\n"
        "Check: Ensure block was returned by de100_memory_alloc().",

    [De100_MEMORY_ERR_ALREADY_FREED] =
        "Block has already been freed.\n"
        "Double-free detected. This is safe (idempotent) but indicates a bug.\n"
        "Check: Review ownership and lifetime of this block.",

    [De100_MEMORY_ERR_PAGE_SIZE_FAILED] =
        "Failed to determine system page size.\n"
        "This is a critical system error that should never happen.\n"
        "Possible causes:\n"
        "  - sysconf(_SC_PAGESIZE) failed on POSIX\n"
        "  - GetSystemInfo failed on Windows\n"
        "Check: System may be in an unstable state.",

    [De100_MEMORY_ERR_PLATFORM_ERROR] =
        "Platform-specific error occurred.\n"
        "The underlying OS call failed for an unmapped reason.\n"
        "Check: Use platform debugging tools (strace, Process Monitor).",
};

const char *de100_memory_error_str_detailed(De100MemoryError error) {
  if (error >= 0 && error < De100_MEMORY_ERR_COUNT) {
    return g_de100_memory_error_details[error];
  }
  return "Unknown error code. This indicates a bug in error handling.";
}
// #endif // DE100_INTERNAL && DE100_SLOW

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM HELPERS
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)

de100_file_scoped_fn inline De100MemoryError
win32_error_to_de100_memory_error(DWORD err) {
  switch (err) {
  case ERROR_NOT_ENOUGH_MEMORY:
  case ERROR_OUTOFMEMORY:
  case ERROR_COMMITMENT_LIMIT:
    return De100_MEMORY_ERR_OUT_OF_MEMORY;

  case ERROR_INVALID_ADDRESS:
  case ERROR_INVALID_PARAMETER:
    return De100_MEMORY_ERR_INVALID_ADDRESS;

  case ERROR_ACCESS_DENIED:
    return De100_MEMORY_ERR_PERMISSION_DENIED;

  case ERROR_ALREADY_EXISTS:
    return De100_MEMORY_ERR_ADDRESS_IN_USE;

  default:
    return De100_MEMORY_ERR_PLATFORM_ERROR;
  }
}

de100_file_scoped_fn inline DWORD
win32_protection_flags(De100MemoryFlags flags) {
  bool r = (flags & De100_MEMORY_FLAG_READ) != 0;
  bool w = (flags & De100_MEMORY_FLAG_WRITE) != 0;
  bool x = (flags & De100_MEMORY_FLAG_EXECUTE) != 0;

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

#elif defined(DE100_IS_GENERIC_POSIX)

de100_file_scoped_fn inline De100MemoryError
posix_error_to_de100_memory_error(int err) {
  switch (err) {
  case ENOMEM:
    return De100_MEMORY_ERR_OUT_OF_MEMORY;

  case EINVAL:
    return De100_MEMORY_ERR_INVALID_ADDRESS;

  case EACCES:
  case EPERM:
    return De100_MEMORY_ERR_PERMISSION_DENIED;

  case EEXIST:
    return De100_MEMORY_ERR_ADDRESS_IN_USE;

  default:
    return De100_MEMORY_ERR_PLATFORM_ERROR;
  }
}

de100_file_scoped_fn inline int posix_protection_flags(De100MemoryFlags flags) {
  int prot = PROT_NONE;
  if (flags & De100_MEMORY_FLAG_READ)
    prot |= PROT_READ;
  if (flags & De100_MEMORY_FLAG_WRITE)
    prot |= PROT_WRITE;
  if (flags & De100_MEMORY_FLAG_EXECUTE)
    prot |= PROT_EXEC;
  return prot;
}

#endif

// ═══════════════════════════════════════════════════════════════════════════
// ALLOCATION
// ═══════════════════════════════════════════════════════════════════════════

De100MemoryBlock de100_memory_alloc(void *base_hint, size_t size,
                                    De100MemoryFlags flags) {
  De100MemoryBlock result = {0};

  // ─────────────────────────────────────────────────────────────────────
  // Validate size
  // ─────────────────────────────────────────────────────────────────────

  if (size == 0) {
    result.error_code = De100_MEMORY_ERR_INVALID_SIZE;
    return result;
  }

  size_t page_size = de100_memory_page_size();
  if (page_size == 0) {
    result.error_code = De100_MEMORY_ERR_PAGE_SIZE_FAILED;
    return result;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Calculate sizes (with overflow check)
  // ─────────────────────────────────────────────────────────────────────

  // Align to page boundary: (size + page_size - 1) & ~(page_size - 1)
  size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);

  // Total = aligned + 2 guard pages
  size_t total_size = aligned_size + (2 * page_size);

  // Overflow check
  if (total_size < aligned_size) {
    result.error_code = De100_MEMORY_ERR_SIZE_OVERFLOW;
    return result;
  }

#if defined(_WIN32)
  // ═════════════════════════════════════════════════════════════════════
  // WINDOWS
  // ═════════════════════════════════════════════════════════════════════

  void *request_addr = NULL;
  if (flags & (De100_MEMORY_FLAG_BASE_FIXED | De100_MEMORY_FLAG_BASE_HINT)) {
    request_addr = base_hint;
  }

  // Reserve entire range (including guard pages) with no access
  void *reserved =
      VirtualAlloc(request_addr, total_size, MEM_RESERVE, PAGE_NOACCESS);

  // If hint failed, try without specific address
  if (!reserved && (flags & De100_MEMORY_FLAG_BASE_HINT)) {
    reserved = VirtualAlloc(NULL, total_size, MEM_RESERVE, PAGE_NOACCESS);
  }

  if (!reserved) {
    result.error = win32_error_to_de100_memory_error(GetLastError());
    return result;
  }

  // Commit usable region (skip first guard page)
  void *usable = (uint8 *)reserved + page_size;
  DWORD protect = win32_protection_flags(flags);

  void *committed = VirtualAlloc(usable, aligned_size, MEM_COMMIT, protect);
  if (!committed) {
    result.error = win32_error_to_de100_memory_error(GetLastError());
    VirtualFree(reserved, 0, MEM_RELEASE);
    return result;
  }

  // Zero if requested (VirtualAlloc already zeros, but be explicit)
  if (flags & De100_MEMORY_FLAG_ZEROED) {
    ZeroMemory(committed, aligned_size);
  }

  result.base = committed;

#elif defined(DE100_IS_GENERIC_POSIX)
  // ═════════════════════════════════════════════════════════════════════
  // POSIX (Linux, macOS, BSD)
  // ═════════════════════════════════════════════════════════════════════

  int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
  if (flags & De100_MEMORY_FLAG_BASE_FIXED) {
    mmap_flags |= MAP_FIXED;
  }

  // Reserve entire range with no access (guard pages)
  void *reserved = mmap(base_hint, total_size, PROT_NONE, mmap_flags, -1, 0);

  if (reserved == MAP_FAILED) {
    result.error_code = posix_error_to_de100_memory_error(errno);
    return result;
  }

  // Set protection on usable region (skip first guard page)
  void *usable = (uint8 *)reserved + page_size;
  int prot = posix_protection_flags(flags);

  if (mprotect(usable, aligned_size, prot) != 0) {
    result.error_code = posix_error_to_de100_memory_error(errno);
    munmap(reserved, total_size);
    return result;
  }

  // Note: mmap with MAP_ANONYMOUS guarantees zero-initialized pages
  // No explicit zeroing needed

#if DE100_INTERNAL && DE100_SLOW
  // Verify zero-initialization in dev builds
  if (flags & De100_MEMORY_FLAG_ZEROED) {
    uint8 *p = (uint8 *)usable;
    size_t check_offsets[] = {0, aligned_size / 4, aligned_size / 2,
                              3 * aligned_size / 4, aligned_size - 1};
    for (size_t i = 0; i < ArraySize(check_offsets); i++) {
      DEV_ASSERT_MSG(p[check_offsets[i]] == 0,
                     "mmap returned non-zero memory!, offset %zu",
                     check_offsets[i]);
    }
  }
#endif

  result.base = usable;

#endif

  // ─────────────────────────────────────────────────────────────────────
  // Success
  // ─────────────────────────────────────────────────────────────────────

  result.size = aligned_size;
  result.total_size = total_size;
  result.flags = flags;
  result.error_code = De100_MEMORY_OK;
  result.is_valid = true;

  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// RESET (Zero existing block without reallocating)
// ═══════════════════════════════════════════════════════════════════════════

De100MemoryError de100_memory_reset(De100MemoryBlock *block) {
  if (!block) {
    return De100_MEMORY_ERR_NULL_BLOCK;
  }

  if (!block->base || !block->is_valid) {
    return De100_MEMORY_ERR_INVALID_BLOCK;
  }

  de100_mem_set(block->base, 0, block->size);

  return De100_MEMORY_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// REALLOC (Resize block with optional data preservation)
// ═══════════════════════════════════════════════════════════════════════════
//
// Updates struct IN PLACE - the De100MemoryBlock* remains valid.
// Increments generation so holders of old base pointers can detect staleness.
//
// ═══════════════════════════════════════════════════════════════════════════

De100MemoryError de100_memory_realloc(De100MemoryBlock *block, size_t new_size,
                                      bool preserve_data) {
  if (!block) {
    return De100_MEMORY_ERR_NULL_BLOCK;
  }

  if (new_size == 0) {
    return De100_MEMORY_ERR_INVALID_SIZE;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Case 1: Block is invalid/empty - allocate fresh, update in place
  // ─────────────────────────────────────────────────────────────────────
  if (!block->base || !block->is_valid) {
    De100MemoryBlock new_block =
        de100_memory_alloc(NULL, new_size, De100_MEMORY_FLAG_RW_ZEROED);

    // Update struct fields in place
    block->base = new_block.base;
    block->size = new_block.size;
    block->total_size = new_block.total_size;
    block->flags = new_block.flags;
    block->error_code = new_block.error_code;
    block->is_valid = new_block.is_valid;
    block->generation++;

    return block->error_code;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Calculate aligned sizes
  // ─────────────────────────────────────────────────────────────────────
  size_t page_size = de100_memory_page_size();
  if (page_size == 0) {
    block->error_code = De100_MEMORY_ERR_PAGE_SIZE_FAILED;
    return De100_MEMORY_ERR_PAGE_SIZE_FAILED;
  }

  size_t new_aligned = (new_size + page_size - 1) & ~(page_size - 1);
  size_t old_aligned = block->size;

  // ─────────────────────────────────────────────────────────────────────
  // Case 2: Same aligned size - no reallocation needed
  // ─────────────────────────────────────────────────────────────────────
  if (new_aligned == old_aligned) {
    if (!preserve_data) {
      de100_mem_set(block->base, 0, block->size);
    }
    block->error_code = De100_MEMORY_OK;
    return De100_MEMORY_OK;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Case 3: Different size - must reallocate
  // ─────────────────────────────────────────────────────────────────────

  // Save old state for copy/cleanup
  void *old_base = block->base;
  size_t old_size = block->size;
  size_t old_total_size = block->total_size;
  size_t copy_size = (old_size < new_aligned) ? old_size : new_aligned;

  // Allocate new memory
  De100MemoryBlock new_block = de100_memory_alloc(NULL, new_size, block->flags);
  if (!de100_memory_is_valid(new_block)) {
    // Original block unchanged on failure
    block->error_code = new_block.error_code;
    return new_block.error_code;
  }

  // Copy old data if requested
  if (preserve_data && old_base) {
    de100_mem_copy(new_block.base, old_base, copy_size);

    // Zero extra space if we grew
    if (new_block.size > old_size) {
      de100_mem_set((uint8 *)new_block.base + old_size, 0,
                    new_block.size - old_size);
    }
  }

  // Free old memory (calculate original reserved base)
  size_t guard_page_size = de100_memory_page_size();
  void *old_reserved_base = (uint8 *)old_base - guard_page_size;

#if defined(_WIN32)
  VirtualFree(old_reserved_base, 0, MEM_RELEASE);
#elif defined(DE100_IS_GENERIC_POSIX)
  munmap(old_reserved_base, old_total_size);
#endif

  // Update struct fields in place (pointer to block stays valid!)
  block->base = new_block.base;
  block->size = new_block.size;
  block->total_size = new_block.total_size;
  // block->flags stays the same
  block->error_code = De100_MEMORY_OK;
  block->is_valid = true;
  block->generation++;

  return De100_MEMORY_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// FREE
// ═══════════════════════════════════════════════════════════════════════════

De100MemoryError de100_memory_free(De100MemoryBlock *block) {
  // ─────────────────────────────────────────────────────────────────────
  // Validate
  // ─────────────────────────────────────────────────────────────────────

  if (!block) {
    return De100_MEMORY_ERR_NULL_BLOCK;
  }

  // Idempotent: already freed is OK
  if (!block->base || !block->is_valid) {
    block->base = NULL;
    block->is_valid = false;
    block->error_code = De100_MEMORY_OK;
#if DE100_INTERNAL && DE100_SLOW
    printf("de100_memory_free: Block already freed or invalid\n");
#endif
    return De100_MEMORY_OK;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Calculate original reserved base
  // ─────────────────────────────────────────────────────────────────────

  size_t page_size = de100_memory_page_size();
  if (page_size == 0) {
    block->error_code = De100_MEMORY_ERR_PAGE_SIZE_FAILED;
    return De100_MEMORY_ERR_PAGE_SIZE_FAILED;
  }

  void *reserved_base = (uint8 *)block->base - page_size;

#if defined(_WIN32)
  if (!VirtualFree(reserved_base, 0, MEM_RELEASE)) {
    block->error = win32_error_to_de100_memory_error(GetLastError());
    return block->error;
  }
#elif defined(DE100_IS_GENERIC_POSIX)
  if (munmap(reserved_base, block->total_size) != 0) {
    block->error_code = posix_error_to_de100_memory_error(errno);
    return block->error_code;
  }
#endif

  // ─────────────────────────────────────────────────────────────────────
  // Clear block
  // ─────────────────────────────────────────────────────────────────────

  block->base = NULL;
  block->size = 0;
  block->total_size = 0;
  block->is_valid = false;
  block->error_code = De100_MEMORY_OK;

  return De100_MEMORY_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

bool de100_memory_is_valid(De100MemoryBlock block) {
  return block.is_valid && block.base != NULL &&
         block.error_code == De100_MEMORY_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════

void *de100_mem_set(void *dest, int value, size_t size) {
  if (!dest || size == 0)
    return dest;
  return memset(dest, value, size);
}

void *de100_mem_copy(void *dest, const void *src, size_t size) {
  if (!dest || !src || size == 0)
    return dest;
  return memcpy(dest, src, size);
}

void *de100_mem_move(void *dest, const void *src, size_t size) {
  if (!dest || !src || size == 0)
    return dest;
  return memmove(dest, src, size);
}

void *de100_mem_zero_secure(void *dest, size_t size) {
  if (!dest || size == 0)
    return dest;

#if defined(_WIN32)
  SecureZeroMemory(dest, size);
#elif defined(__APPLE__) || defined(__FreeBSD__)
  memset_s(dest, size, 0, size);
#elif defined(_GNU_SOURCE) && defined(__GLIBC__) &&                            \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
  explicit_bzero(dest, size);
#else
  volatile uint8 *p = (volatile uint8 *)dest;
  while (size--)
    *p++ = 0;
#endif

  return dest;
}

// TODO: Should the following be implemented?
// //
// ═══════════════════════════════════════════════════════════════════════════
// // MEMORY OPERATIONS
// //
// ═══════════════════════════════════════════════════════════════════════════

// /**
//  * @brief Fill memory with a constant byte value.
//  *
//  * @param dest Destination memory address
//  * @param value Byte value to fill (converted to unsigned char)
//  * @param size Number of bytes to fill
//  * @return Pointer to dest, or NULL if dest is NULL
//  *
//  * @note This is a secure implementation that won't be optimized away
//  *       by the compiler, making it suitable for clearing sensitive data.
//  */
// void *de100_memset(void *dest, int value, size_t size) {
//   if (!dest || size == 0) {
//     return dest;
//   }

// #if defined(_WIN32)
//   // Windows provides SecureZeroMemory for security-critical zeroing
//   // For general memset, use FillMemory or standard approach
//   if (value == 0) {
//     // Use volatile to prevent compiler optimization
//     volatile unsigned char *p = (volatile unsigned char *)dest;
//     while (size--) {
//       *p++ = 0;
//     }
//   } else {
//     FillMemory(dest, size, (BYTE)value);
//   }
// #elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
//     defined(__unix__) || defined(__MACH__)
//   // POSIX: Use explicit_bzero for zeroing (glibc 2.25+, BSD)
//   // or memset_s (C11 Annex K) where available
//   if (value == 0) {
// #if defined(__GLIBC__) && defined(_DEFAULT_SOURCE) && \
//     (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
//     explicit_bzero(dest, size);
// #elif defined(__APPLE__) || defined(__FreeBSD__)
//     memset_s(dest, size, 0, size);
// #else
//     // Fallback: volatile pointer to prevent optimization
//     volatile unsigned char *p = (volatile unsigned char *)dest;
//     while (size--) {
//       *p++ = 0;
//     }
// #endif
//   } else {
//     memset(dest, value, size);
//   }
// #else
//   // Generic fallback
//   memset(dest, value, size);
// #endif

//   return dest;
// }

// /**
//  * @brief Securely zero memory (guaranteed not to be optimized away).
//  *
//  * @param dest Destination memory address
//  * @param size Number of bytes to zero
//  * @return Pointer to dest, or NULL if dest is NULL
//  */
// void *de100_secure_zero(void *dest, size_t size) {
//   if (!dest || size == 0) {
//     return dest;
//   }

// #if defined(_WIN32)
//   SecureZeroMemory(dest, size);
// #elif defined(__APPLE__) || defined(__FreeBSD__)
//   memset_s(dest, size, 0, size);
// #elif defined(__GLIBC__) && defined(_DEFAULT_SOURCE) && \
//     (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
//   explicit_bzero(dest, size);
// #else
//   // Volatile pointer prevents compiler from optimizing this away
//   volatile unsigned char *p = (volatile unsigned char *)dest;
//   while (size--) {
//     *p++ = 0;
//   }
//   // Memory barrier to ensure writes complete
//   __asm__ __volatile__("" : : "r"(dest) : "memory");
// #endif

//   return dest;
// }

// /**
//  * @brief Copy memory from source to destination.
//  *
//  * @param dest Destination memory address
//  * @param src Source memory address
//  * @param size Number of bytes to copy
//  * @return Pointer to dest, or NULL on error
//  *
//  * @note Behavior is undefined if regions overlap. Use de100_memmove for
//  *       overlapping regions.
//  */
// void *de100_memcpy(void *dest, const void *src, size_t size) {
//   if (!dest || !src || size == 0) {
//     return dest;
//   }

// #if defined(_WIN32)
//   CopyMemory(dest, src, size);
//   return dest;
// #else
//   return memcpy(dest, src, size);
// #endif
// }

// /**
//  * @brief Copy memory, handling overlapping regions correctly.
//  *
//  * @param dest Destination memory address
//  * @param src Source memory address
//  * @param size Number of bytes to copy
//  * @return Pointer to dest, or NULL on error
//  */
// void *de100_memmove(void *dest, const void *src, size_t size) {
//   if (!dest || !src || size == 0) {
//     return dest;
//   }

// #if defined(_WIN32)
//   MoveMemory(dest, src, size);
//   return dest;
// #else
//   return memmove(dest, src, size);
// #endif
// }
