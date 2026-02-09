#ifndef DE100_COMMON_De100_MEMORY_H
#define DE100_COMMON_De100_MEMORY_H

#include "base.h"
#include <stdbool.h>
#include <stddef.h>

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
  De100_MEMORY_OK = 0,

  // Allocation errors
  De100_MEMORY_ERR_OUT_OF_MEMORY,
  De100_MEMORY_ERR_INVALID_SIZE,
  De100_MEMORY_ERR_SIZE_OVERFLOW,

  // Address errors
  De100_MEMORY_ERR_INVALID_ADDRESS,
  De100_MEMORY_ERR_ADDRESS_IN_USE,
  De100_MEMORY_ERR_ALIGNMENT_FAILED,

  // Permission errors
  De100_MEMORY_ERR_PERMISSION_DENIED,
  De100_MEMORY_ERR_PROTECTION_FAILED,

  // Block errors
  De100_MEMORY_ERR_NULL_BLOCK,
  De100_MEMORY_ERR_INVALID_BLOCK,
  De100_MEMORY_ERR_ALREADY_FREED,

  // System errors
  De100_MEMORY_ERR_PAGE_SIZE_FAILED,
  De100_MEMORY_ERR_PLATFORM_ERROR,

  De100_MEMORY_ERR_COUNT // Sentinel for bounds checking
} De100MemoryError;

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY FLAGS
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
  De100_MEMORY_FLAG_NONE = 0,

  // Protection flags
  De100_MEMORY_FLAG_READ = 1 << 0,
  De100_MEMORY_FLAG_WRITE = 1 << 1,
  De100_MEMORY_FLAG_EXECUTE = 1 << 2,

  // Initialization flags
  De100_MEMORY_FLAG_ZEROED = 1 << 3,

  // Addressing flags
  De100_MEMORY_FLAG_BASE_HINT = 1 << 4,  // Try base, allow relocation
  De100_MEMORY_FLAG_BASE_FIXED = 1 << 5, // Must use exact base address

  // Optimization hints (best-effort)
  De100_MEMORY_FLAG_LARGE_PAGES = 1 << 6,
  De100_MEMORY_FLAG_TRANSIENT = 1 << 7,
} De100MemoryFlags;

// Common flag combinations
#define De100_MEMORY_FLAG_RW (De100_MEMORY_FLAG_READ | De100_MEMORY_FLAG_WRITE)
#define De100_MEMORY_FLAG_RWX                                                  \
  (De100_MEMORY_FLAG_READ | De100_MEMORY_FLAG_WRITE | De100_MEMORY_FLAG_EXECUTE)
#define De100_MEMORY_FLAG_RW_ZEROED                                            \
  (De100_MEMORY_FLAG_RW | De100_MEMORY_FLAG_ZEROED)

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY BLOCK
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  void *base;                  // Pointer to usable memory (after guard page)
  size_t size;                 // Usable size (page-aligned)
  size_t total_size;           // Total size including guard pages
  De100MemoryFlags flags;      // Flags used for allocation
  De100MemoryError error_code; // Error code (De100_MEMORY_OK if valid)
  uint32 generation; // Incremented on each realloc to detect stale refs
  bool is_valid;     // Quick validity check
} De100MemoryBlock;

// ═══════════════════════════════════════════════════════════════════════════
// CORE API
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Allocate virtual memory with guard pages.
 *
 * @param base_hint  Preferred base address (NULL for any)
 * @param size       Requested size in bytes (will be page-aligned)
 * @param flags      Protection and behavior flags
 * @return           De100MemoryBlock (check .is_valid or .error)
 *
 * Memory layout:
 *   [Guard Page][Usable Memory][Guard Page]
 *   └─ PROT_NONE ─┘└─ Your data ─┘└─ PROT_NONE ─┘
 */
De100MemoryBlock de100_memory_alloc(void *base_hint, size_t size,
                                    De100MemoryFlags flags);

/**
 * @brief Zero an existing memory block without reallocating.
 *
 * @param block Pointer to a valid memory block
 * @return De100_MEMORY_OK on success, error code otherwise
 */
De100MemoryError de100_memory_reset(De100MemoryBlock *block);

/**
 * @brief Resize a memory block.
 *
 * @param block Pointer to memory block (modified in place)
 * @param new_size New size in bytes
 * @param preserve_data If true, copy old data to new block (up to min size)
 * @return De100_MEMORY_OK on success, error code otherwise
 *
 * @note If new aligned size equals current, no reallocation occurs.
 * @note If preserve_data is false and sizes match, block is zeroed.
 * @note On failure, original block remains valid and unchanged.
 */
De100MemoryError de100_memory_realloc(De100MemoryBlock *block, size_t new_size,
                                      bool preserve_data);

/**
 * Free a memory block.
 *
 * @param block  Pointer to block (will be zeroed after free)
 * @return       Error code
 *
 * Idempotent: safe to call multiple times on same block.
 */
De100MemoryError de100_memory_free(De100MemoryBlock *block);

// ═══════════════════════════════════════════════════════════════════════════
// UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

/** Get system page size (cached after first call). */
size_t de100_memory_page_size(void);

/** Check if block is valid and usable. */
bool de100_memory_is_valid(De100MemoryBlock block);

/** Get human-readable error message. */
const char *de100_memory_error_str(De100MemoryError error);

// #if DE100_INTERNAL && DE100_SLOW
/** Get detailed error message with context (dev builds only). */
const char *de100_memory_error_str_detailed(De100MemoryError error);
// #endif

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════

/** Fill memory with byte value. Returns dest. */
void *de100_mem_set(void *dest, int value, size_t size);

/** Copy memory (non-overlapping). Returns dest. */
void *de100_mem_copy(void *dest, const void *src, size_t size);

/** Copy memory (overlapping safe). Returns dest. */
void *de100_mem_move(void *dest, const void *src, size_t size);

/** Zero memory (compiler won't optimize away). Returns dest. */
void *de100_mem_zero_secure(void *dest, size_t size);

#endif // DE100_COMMON_De100_MEMORY_H
