#ifndef ENGINE_COMMON_MEMORY_H
#define ENGINE_COMMON_MEMORY_H

#include "base.h"
#include <stddef.h>
#include <stdbool.h>

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    PLATFORM_MEMORY_SUCCESS = 0,
    PLATFORM_MEMORY_ERROR_OUT_OF_MEMORY,
    PLATFORM_MEMORY_ERROR_INVALID_SIZE,
    PLATFORM_MEMORY_ERROR_INVALID_ADDRESS,
    PLATFORM_MEMORY_ERROR_PERMISSION_DENIED,
    PLATFORM_MEMORY_ERROR_ADDRESS_IN_USE,
    PLATFORM_MEMORY_ERROR_ALIGNMENT_FAILED,
    PLATFORM_MEMORY_ERROR_PROTECTION_FAILED,
    PLATFORM_MEMORY_ERROR_INVALID_BLOCK,
    PLATFORM_MEMORY_ERROR_ALREADY_FREED,
    PLATFORM_MEMORY_ERROR_UNKNOWN
} PlatformMemoryError;

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY FLAGS
// ═══════════════════════════════════════════════════════════════════════════

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
    /** Try requested base, allow relocation */
    PLATFORM_MEMORY_BASE_HINT = 1 << 8,
    
    /** Must map exactly at requested base */
    PLATFORM_MEMORY_BASE_FIXED = 1 << 9
} PlatformMemoryFlags;

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY BLOCK STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Represents a contiguous virtual memory allocation with guard pages.
 */
typedef struct PlatformMemoryBlock {
    void *base;                     /**< Pointer to usable memory region */
    size_t size;                    /**< Usable size (page-aligned) */
    size_t total_size;              /**< Total reserved size including guard pages */
    PlatformMemoryFlags flags;      /**< Allocation flags */
    PlatformMemoryError error_code; /**< Error code (SUCCESS if valid) */
    char error_message[512];        /**< Detailed error message */
    bool is_valid;                  /**< True if allocation succeeded */
} PlatformMemoryBlock;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Allocates a virtual memory block with optional base address control.
 *
 * @param base_hint
 * Optional base address preference.
 * - NULL → no preference
 * - non-NULL → interpreted according to flags
 *
 * @param size
 * Requested usable size in bytes. Must be > 0.
 *
 * @param flags
 * Allocation and protection flags.
 *
 * @return PlatformMemoryBlock
 * Check block.is_valid to determine success.
 * On error, block.base == NULL and error_code/error_message are set.
 * 
 * @note
 * The returned block includes guard pages. Use block.base for the usable region.
 * The total allocation size is block.total_size.
 */
PlatformMemoryBlock platform_allocate_memory(void *base_hint, size_t size,
                                             PlatformMemoryFlags flags);

/**
 * @brief Frees a virtual memory block.
 * 
 * @param block Pointer to the memory block to free
 * @return Error code indicating success or failure
 * 
 * This function is idempotent - calling it multiple times on the same
 * block is safe (subsequent calls will return success immediately).
 * 
 * After freeing, block->base will be set to NULL and is_valid to false.
 */
PlatformMemoryError platform_free_memory(PlatformMemoryBlock *block);

/**
 * @brief Get a human-readable error message for an error code.
 * 
 * @param error Error code
 * @return Static string describing the error
 */
const char *platform_memory_strerror(PlatformMemoryError error);

/**
 * @brief Check if a memory block is valid.
 * 
 * @param block Memory block to check
 * @return true if the block is valid and usable, false otherwise
 */
bool platform_memory_is_valid(PlatformMemoryBlock block);

/**
 * @brief Get the system page size.
 * 
 * @return Page size in bytes, or 0 on error
 */
size_t platform_get_page_size(void);

#endif // ENGINE_COMMON_MEMORY_H
