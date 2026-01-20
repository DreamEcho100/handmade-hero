#ifndef ENGINE_COMMON_MEMORY_H
#define ENGINE_COMMON_MEMORY_H
#include "base.h"

/**
 * @brief Virtual memory allocation flags.
 *
 * These flags describe the intended usage and access permissions of
 * a virtual memory block. They are translated internally to the
 * appropriate platform-specific virtual memory APIs.
 *
 * @note
 * Not all flags are supported equally on all platforms. Unsupported
 * flags are safely ignored.
 */
typedef enum PlatformMemoryFlags {
  PLATFORM_MEMORY_NONE = 0,

  /** Memory is readable */
  PLATFORM_MEMORY_READ = 1 << 0,

  /** Memory is writable */
  PLATFORM_MEMORY_WRITE = 1 << 1,

  /** Memory is executable */
  PLATFORM_MEMORY_EXECUTE = 1 << 2,

  /** Memory should be zero-initialized (best-effort) */
  PLATFORM_MEMORY_ZEROED = 1 << 3,

  /** Memory is intended for transient / short-lived usage */
  PLATFORM_MEMORY_TRANSIENT = 1 << 4,

  /** Memory should use large pages if available (best-effort) */
  PLATFORM_MEMORY_LARGE_PAGES = 1 << 5,

  /* Addressing semantics */
  PLATFORM_MEMORY_BASE_HINT = 1
                              << 8, /**< Try requested base, allow relocation */
  PLATFORM_MEMORY_BASE_FIXED = 1 << 9 /**< Must map exactly at requested base */
} PlatformMemoryFlags;

/**
 * @brief Represents a contiguous virtual memory allocation with guard pages.
 */
typedef struct PlatformMemoryBlock {
  void *base;                /**< Pointer to usable memory region */
  size_t size;               /**< Usable size (page-aligned) */
  size_t total_size;         /**< Total reserved size including guard pages */
  PlatformMemoryFlags flags; /**< Allocation flags */
} PlatformMemoryBlock;

/**
 * @brief Allocates a virtual memory block with optional base address control.
 *
 * @param base_hint
 * Optional base address preference.
 * - NULL → no preference
 * - non-NULL → interpreted according to flags
 *
 * @param size
 * Requested usable size in bytes.
 *
 * @param flags
 * Allocation and protection flags.
 *
 * @return PlatformMemoryBlock
 * If allocation fails, block.base == NULL.
 */
PlatformMemoryBlock platform_allocate_memory(void *base_hint, size_t size,
                                             PlatformMemoryFlags flags);

/**
 * @brief Frees a virtual memory block.
 */
// void platform_free_memory(void* base, size_t total_size);
void platform_free_memory(PlatformMemoryBlock *block);

#endif // ENGINE_COMMON_MEMORY_H