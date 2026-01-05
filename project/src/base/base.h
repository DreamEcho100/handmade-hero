#ifndef BASE_H
#define BASE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef ArraySize
#define ArraySize(Array) (sizeof(Array) / sizeof((Array)[0]))
#endif

#if HANDMADE_SLOW
#define Assert(expression) \
  if (!(expression))    \
  {                     \
    *(int *)0 = 0;     \
  }
#else
#define Assert(expression)
#endif

#define KILOBYTES(value) ((value)*1024LL)
#define MEGABYTES(value) (KILOBYTES(value) * 1024LL)
#define GIGABYTES(value) (MEGABYTES(value) * 1024LL)
#define TERABYTES(value) (GIGABYTES(value) * 1024LL)

#define file_scoped_fn static
#define local_persist_var static
#define file_scoped_global_var static
#define real32 float
#define real64 double

typedef int32_t bool32;

/**
* Platform-agnostic pixel composer function
*
* Platform sets this once, game just calls it
*/
typedef uint32_t (*pixel_composer_fn)(uint8_t r, uint8_t g, uint8_t b,
                                      uint8_t a);

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
  PLATFORM_MEMORY_BASE_HINT   = 1 << 8,  /**< Try requested base, allow relocation */
  PLATFORM_MEMORY_BASE_FIXED  = 1 << 9   /**< Must map exactly at requested base */
} PlatformMemoryFlags;

/**
 * @brief Represents a contiguous virtual memory allocation with guard pages.
 */
typedef struct PlatformMemoryBlock {
    void*  base;        /**< Pointer to usable memory region */
    size_t size;        /**< Usable size (page-aligned) */
    size_t total_size;  /**< Total reserved size including guard pages */
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
PlatformMemoryBlock
platform_allocate_memory(void* base_hint, size_t size, PlatformMemoryFlags flags);

/**
 * @brief Frees a virtual memory block.
 */
// void platform_free_memory(void* base, size_t total_size);
void platform_free_memory(PlatformMemoryBlock *block);

#endif // BASE_H
