#ifndef DE100_COMMON_MEMORY_H
#define DE100_COMMON_MEMORY_H

#include "base.h"
#include <stdbool.h>
#include <stddef.h>

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    MEMORY_OK = 0,
    
    // Allocation errors
    MEMORY_ERR_OUT_OF_MEMORY,
    MEMORY_ERR_INVALID_SIZE,
    MEMORY_ERR_SIZE_OVERFLOW,
    
    // Address errors
    MEMORY_ERR_INVALID_ADDRESS,
    MEMORY_ERR_ADDRESS_IN_USE,
    MEMORY_ERR_ALIGNMENT_FAILED,
    
    // Permission errors
    MEMORY_ERR_PERMISSION_DENIED,
    MEMORY_ERR_PROTECTION_FAILED,
    
    // Block errors
    MEMORY_ERR_NULL_BLOCK,
    MEMORY_ERR_INVALID_BLOCK,
    MEMORY_ERR_ALREADY_FREED,
    
    // System errors
    MEMORY_ERR_PAGE_SIZE_FAILED,
    MEMORY_ERR_PLATFORM_ERROR,
    
    MEMORY_ERR_COUNT  // Sentinel for bounds checking
} MemoryError;

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY FLAGS
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    MEMORY_FLAG_NONE = 0,
    
    // Protection flags
    MEMORY_FLAG_READ    = 1 << 0,
    MEMORY_FLAG_WRITE   = 1 << 1,
    MEMORY_FLAG_EXECUTE = 1 << 2,
    
    // Initialization flags
    MEMORY_FLAG_ZEROED  = 1 << 3,
    
    // Addressing flags
    MEMORY_FLAG_BASE_HINT  = 1 << 4,  // Try base, allow relocation
    MEMORY_FLAG_BASE_FIXED = 1 << 5,  // Must use exact base address
    
    // Optimization hints (best-effort)
    MEMORY_FLAG_LARGE_PAGES = 1 << 6,
    MEMORY_FLAG_TRANSIENT   = 1 << 7,
} MemoryFlags;

// Common flag combinations
#define MEMORY_FLAG_RW (MEMORY_FLAG_READ | MEMORY_FLAG_WRITE)
#define MEMORY_FLAG_RWX (MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_EXECUTE)
#define MEMORY_FLAG_RW_ZEROED (MEMORY_FLAG_RW | MEMORY_FLAG_ZEROED)

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY BLOCK
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    void        *base;        // Pointer to usable memory (after guard page)
    size_t       size;        // Usable size (page-aligned)
    size_t       total_size;  // Total size including guard pages
    MemoryFlags  flags;       // Flags used for allocation
    MemoryError  error_code;       // Error code (MEMORY_OK if valid)
    bool         is_valid;    // Quick validity check
} MemoryBlock;

// ═══════════════════════════════════════════════════════════════════════════
// CORE API
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Allocate virtual memory with guard pages.
 *
 * @param base_hint  Preferred base address (NULL for any)
 * @param size       Requested size in bytes (will be page-aligned)
 * @param flags      Protection and behavior flags
 * @return           MemoryBlock (check .is_valid or .error)
 *
 * Memory layout:
 *   [Guard Page][Usable Memory][Guard Page]
 *   └─ PROT_NONE ─┘└─ Your data ─┘└─ PROT_NONE ─┘
 */
MemoryBlock memory_alloc(void *base_hint, size_t size, MemoryFlags flags);

/**
 * Free a memory block.
 *
 * @param block  Pointer to block (will be zeroed after free)
 * @return       Error code
 *
 * Idempotent: safe to call multiple times on same block.
 */
MemoryError memory_free(MemoryBlock *block);

// ═══════════════════════════════════════════════════════════════════════════
// UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

/** Get system page size (cached after first call). */
size_t memory_page_size(void);

/** Check if block is valid and usable. */
bool memory_is_valid(MemoryBlock block);

/** Get human-readable error message. */
const char *memory_error_str(MemoryError error);

// #if DE100_INTERNAL && DE100_SLOW
/** Get detailed error message with context (dev builds only). */
const char *memory_error_str_detailed(MemoryError error);
// #endif

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════

/** Fill memory with byte value. Returns dest. */
void *mem_set(void *dest, int value, size_t size);

/** Copy memory (non-overlapping). Returns dest. */
void *mem_copy(void *dest, const void *src, size_t size);

/** Copy memory (overlapping safe). Returns dest. */
void *mem_move(void *dest, const void *src, size_t size);

/** Zero memory (compiler won't optimize away). Returns dest. */
void *mem_zero_secure(void *dest, size_t size);

#endif // DE100_COMMON_MEMORY_H
