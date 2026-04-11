#ifndef UTILS_ARENA_H
#define UTILS_ARENA_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <unistd.h>
#endif

/* ── Debug fill patterns ─────────────────────────────────────────────────
 * In debug builds, freshly allocated memory is filled with 0xCD and
 * freed memory with 0xDD. This makes use-after-free and uninitialized
 * reads produce obviously wrong values instead of silently working. */
#ifndef NDEBUG
#define ARENA_DEBUG_FILL_ALLOC 0xCD
#define ARENA_DEBUG_FILL_FREE 0xDD
#endif

typedef struct ArenaBlock {
  struct ArenaBlock *prev;
  size_t size;
  size_t used;
} ArenaBlock;

typedef struct Arena {
  ArenaBlock *current;
  size_t min_block_size;
  int temp_count;
  void *backing_base;
  size_t backing_total;
  size_t page_size; /* cached from arena_alloc (0 if malloc-backed) */
} Arena;

typedef struct TempMemory {
  Arena *arena;
  ArenaBlock *block;
  size_t used;
} TempMemory;

static inline void *arena_push_size(Arena *arena, size_t size, size_t align) {
  assert(align > 0 && (align & (align - 1)) == 0 &&
         "align must be a power of two");

  //  Calculates how many padding bytes are needed so the returned pointer is
  //  aligned.
  // In other words, the allocator first finds the next raw free spot, then
  // nudges it forward if needed:
  size_t alignment_offset = 0;

  if (arena->current) {
    uintptr_t base = (uintptr_t)(
        // Skip past the ArenaBlock header — +1 on a ArenaBlock* advances by
        // sizeof(ArenaBlock) bytes. This is where usable data starts.
        (arena->current + 1) +
        // Jump past all bytes already allocated. This is where the NEXT
        // allocation will start.
        arena->current->used);

    // For power-of-two alignment, align - 1 gives a bitmask for the low bits.
    // E.g., align=8 → mask=0b111.
    // NOTE: we made sure it's a power-of-two with the assert at the start of
    // this function.
    uintptr_t mask = align - 1;

    // Checks if the low bits are non-zero (meaning NOT aligned). E.g., if
    // base=0x1003 and mask=0x7, then base & mask = 3 — we're 3 bytes past an
    // 8-byte boundary.
    if (base & mask) {
      alignment_offset =
          // How many bytes to add to reach the next aligned address. E.g., 8 -
          // 3 = 5 padding bytes.
          align - (base & mask);
    }
  }

  {
    size_t needed = alignment_offset + size;

    int needs_new_block =
        // No current block exists
        !arena->current ||
        // The request is larger than the whole block capacity
        needed > arena->current->size ||
        // Not enough remaining space from the current position
        arena->current->used > arena->current->size - needed;

    if (!arena->min_block_size) {
      arena->min_block_size = 1024u * 1024u; /* 1 MB default */
    }

    size_t block_size =
        size > arena->min_block_size ? size : arena->min_block_size;

    ArenaBlock *block = (ArenaBlock *)malloc(sizeof(ArenaBlock) + block_size);
    assert(block && "arena overflow: malloc failed (OOM)");
    if (!block) {
      return NULL;
    }

    block->prev = arena->current;
    block->size = block_size;
    block->used = 0;
    arena->current = block;

    uintptr_t new_base = (uintptr_t)block + 1;
    uintptr_t new_mask = align - 1;
    alignment_offset =
        (new_base & new_mask) ? align - (new_base & new_mask) : 0;
  }

  arena->current->used += alignment_offset;
  void *result = (uint8_t *)(arena->current + 1) + arena->current->used;
  arena->current->used += size;

#ifndef NDEBUG
  /// Fills the allocated memory region with a debug pattern to detect
  /// uninitialized memory usage during debugging. Uses the value 0xCD
  /// (ARENA_DEBUG_FILL_ALLOC) to mark newly allocated memory, making
  /// uninitialized reads easily identifiable. This is a debug-only practice and
  /// should be disabled in release builds. Note: This approach aligns with
  /// common debugging practices but may conflict with Zero-Initialization
  /// Invariant (ZII) principles that expect zero-initialized memory for
  /// security. Consider wrapping this in #ifdef ARENA_DEBUG guards and using
  /// memset(result, 0, size) for release builds.
  memset(result, ARENA_DEBUG_FILL_ALLOC, size);
#endif

  return result;
}

#endif // UTILS_ARENA_H
