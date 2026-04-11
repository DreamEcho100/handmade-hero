#ifndef LIB_ARENA_H
#define LIB_ARENA_H

/* ═══════════════════════════════════════════════════════════════════════════
 * lib/arena.h — Arena allocator
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Built progressively across Lessons 10-14:
 *   L10: arena_push_size, arena_reset  (basic push + reset)
 *   L11: alignment support, ARENA_PUSH_STRUCT/ARRAY macros
 *   L12: chained-block growth (ArenaBlock linked list)
 *   L13: mmap-backed arena with guard pages
 *   L14: TempMemory checkpoints (begin/end/keep/check)
 *
 * NOT thread-safe. Use one arena per thread or external synchronization.
 * All push operations may return NULL on OOM. In debug builds (NDEBUG
 * not defined), an assert fires to catch OOM immediately.
 *
 * ZII: A zero-initialized Arena is valid but empty.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <unistd.h>
#endif

/* ── Alignment utility ───────────────────────────────────────────────────
 * Shared by arena, pool, slab, stack allocators. */
static inline size_t align_forward(size_t offset, size_t align) {
  assert(align > 0 && (align & (align - 1)) == 0 && "align must be power of 2");
  return (offset + align - 1) & ~(align - 1);
}

/* ── ArenaBlock ──────────────────────────────────────────────────────────*/
typedef struct ArenaBlock {
  struct ArenaBlock *prev;
  size_t size;
  size_t used;
} ArenaBlock;

/* ── Arena ────────────────────────────────────────────────────────────────*/
typedef struct Arena {
  ArenaBlock *current;
  size_t min_block_size;
  int temp_count;
  /* mmap backing (set by arena_init_mmap, used by arena_free).
   * If backing_total == 0, the initial block came from malloc. */
  void *backing_base;
  size_t backing_total;
  size_t page_size;
} Arena;

/* ── TempMemory ──────────────────────────────────────────────────────────
 * TempMemory on an empty arena (current == NULL) is valid but has no
 * rewind effect — the first allocation after begin_temp behaves like
 * a fresh arena. */
typedef struct TempMemory {
  Arena *arena;
  ArenaBlock *block;
  size_t used;
} TempMemory;

#define ARENA_DEFAULT_BLOCK_SIZE (1024u * 1024u) /* 1 MB */

/* ── arena_push_size ──────────────────────────────────────────────────────
 * Core allocator. `align` MUST be a power of two (asserted).
 * Returns NULL on OOM (also asserted in debug). */
static inline void *arena_push_size(Arena *arena, size_t size, size_t align) {
  assert(arena && "arena_push_size: NULL arena");
  assert(align > 0 && (align & (align - 1)) == 0 &&
         "align must be a power of two");

  size_t alignment_offset = 0;

  if (arena->current) {
    uintptr_t base = (uintptr_t)(arena->current + 1) + arena->current->used;
    uintptr_t mask = align - 1;
    if (base & mask)
      alignment_offset = align - (base & mask);
  }

  /* Overflow-safe capacity check */
  {
    size_t needed = alignment_offset + size;
    int needs_new = !arena->current ||
                    needed > arena->current->size ||
                    arena->current->used > arena->current->size - needed;

    if (needs_new) {
      if (!arena->min_block_size)
        arena->min_block_size = ARENA_DEFAULT_BLOCK_SIZE;

      /* Overflow check: size + align must not wrap */
      if (size > SIZE_MAX - align) {
        assert(0 && "arena_push_size: allocation too large");
        return NULL;
      }

      size_t block_size = size + align > arena->min_block_size
                        ? size + align
                        : arena->min_block_size;

      ArenaBlock *block = (ArenaBlock *)malloc(sizeof(ArenaBlock) + block_size);
      assert(block && "arena_push_size: malloc failed (OOM)");
      if (!block)
        return NULL;

      block->prev = arena->current;
      block->size = block_size;
      block->used = 0;
      arena->current = block;

      uintptr_t new_base = (uintptr_t)(block + 1);
      uintptr_t new_mask = align - 1;
      alignment_offset =
          (new_base & new_mask) ? align - (new_base & new_mask) : 0;
    }
  }

  arena->current->used += alignment_offset;
  void *result = (uint8_t *)(arena->current + 1) + arena->current->used;
  arena->current->used += size;
  return result;
}

static inline void *arena_push_zero(Arena *arena, size_t size, size_t align) {
  void *p = arena_push_size(arena, size, align);
  if (p)
    memset(p, 0, size);
  return p;
}

/* ── Type-safe macros ─────────────────────────────────────────────────────*/
#define ARENA_PUSH_STRUCT(arena, Type) \
  ((Type *)arena_push_size((arena), sizeof(Type), _Alignof(Type)))

#define ARENA_PUSH_ARRAY(arena, Count, Type) \
  ((Type *)arena_push_size((arena), (size_t)(Count) * sizeof(Type), \
                           _Alignof(Type)))

#define ARENA_PUSH_ZERO_STRUCT(arena, Type) \
  ((Type *)arena_push_zero((arena), sizeof(Type), _Alignof(Type)))

#define ARENA_PUSH_ZERO_ARRAY(arena, Count, Type) \
  ((Type *)arena_push_zero((arena), (size_t)(Count) * sizeof(Type), \
                           _Alignof(Type)))

/* ── arena_push_string ────────────────────────────────────────────────────*/
static inline char *arena_push_string(Arena *arena, const char *str) {
  if (!str) return NULL;
  size_t len = strlen(str) + 1;
  char *copy = (char *)arena_push_size(arena, len, 1);
  if (copy) memcpy(copy, str, len);
  return copy;
}

/* ── TempMemory functions ─────────────────────────────────────────────────*/

static inline TempMemory arena_begin_temp(Arena *arena) {
  TempMemory t;
  t.arena = arena;
  t.block = arena->current;
  t.used = arena->current ? arena->current->used : 0;
  ++arena->temp_count;
  return t;
}

static inline void arena_end_temp(TempMemory t) {
  Arena *arena = t.arena;
  while (arena->current != t.block) {
    ArenaBlock *to_free = arena->current;
    arena->current = to_free->prev;
    free(to_free);
  }
  if (arena->current)
    arena->current->used = t.used;
  assert(arena->temp_count > 0);
  --arena->temp_count;
}

static inline void arena_keep_temp(TempMemory t) {
  assert(t.arena->temp_count > 0);
  --t.arena->temp_count;
}

static inline void arena_check(const Arena *arena) {
  assert(arena->temp_count == 0 &&
         "arena_check: unmatched begin_temp / end_temp");
}

/* ── arena_reset ──────────────────────────────────────────────────────────
 * Reset to empty, keep the oldest block for reuse. Resets temp_count. */
static inline void arena_reset(Arena *arena) {
  ArenaBlock *block = arena->current;
  while (block) {
    block->used = 0;
    if (!block->prev) {
      arena->current = block;
      break;
    }
    ArenaBlock *prev = block->prev;
    free(block);
    block = prev;
  }
  if (arena->current)
    arena->current->prev = NULL;
  arena->temp_count = 0;
}

/* ── arena_free ───────────────────────────────────────────────────────────
 * Release everything. Handles both mmap-backed and malloc-backed arenas
 * automatically (mmap or malloc). After this call
 * the Arena is equivalent to zero-initialized (ZII). */
static inline void arena_free(Arena *arena) {
#if !defined(_WIN32)
  /* If mmap-backed, identify the initial block so we don't free() it */
  void *initial_block = NULL;
  if (arena->backing_base && arena->page_size > 0)
    initial_block = (uint8_t *)arena->backing_base + arena->page_size;

  /* Free overflow blocks (malloc'd by arena_push_size) */
  while (arena->current) {
    ArenaBlock *b = arena->current;
    arena->current = b->prev;
    if ((void *)b != initial_block)
      free(b);
  }

  /* Release the backing */
  if (arena->backing_base && arena->backing_total > 0) {
    /* mmap path */
    munmap(arena->backing_base, arena->backing_total);
  } else {
    /* malloc path — free all remaining blocks */
    /* (already freed in the loop above since initial_block is NULL) */
  }
#else
  /* No mmap on Windows — just free all blocks */
  while (arena->current) {
    ArenaBlock *to_free = arena->current;
    arena->current = to_free->prev;
    free(to_free);
  }
#endif

  arena->current = NULL;
  arena->backing_base = NULL;
  arena->backing_total = 0;
  arena->page_size = 0;
  arena->temp_count = 0;
}

/* ── Diagnostics ──────────────────────────────────────────────────────────*/
static inline size_t arena_total_allocated(const Arena *arena) {
  size_t total = 0;
  const ArenaBlock *block = arena->current;
  while (block) { total += block->size; block = block->prev; }
  return total;
}

static inline size_t arena_total_used(const Arena *arena) {
  size_t total = 0;
  const ArenaBlock *block = arena->current;
  while (block) { total += block->used; block = block->prev; }
  return total;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MMAP-BACKED ARENA (Lesson 13) — guard pages for overflow detection
 *
 * arena_init_mmap: allocate backing via mmap with guard pages AND init the
 *                  arena in one call. Sets backing_base/backing_total/page_size.
 * arena_free:      handles both mmap and malloc automatically (see above).
 * ═══════════════════════════════════════════════════════════════════════════*/
#if !defined(_WIN32)

static inline void *arena_init_mmap(Arena *arena, size_t data_size) {
  long ps_long = sysconf(_SC_PAGESIZE);
  size_t ps = (ps_long > 0) ? (size_t)ps_long : 4096u;

  size_t raw = sizeof(ArenaBlock) + data_size;
  size_t usable = ((raw + ps - 1) / ps) * ps;
  size_t total = ps + usable + ps;

  void *map = mmap(NULL, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (map == MAP_FAILED)
    return NULL;

  void *start = (uint8_t *)map + ps;
  if (mprotect(start, usable, PROT_READ | PROT_WRITE) != 0) {
    munmap(map, total);
    return NULL;
  }

  arena->backing_base = map;
  arena->backing_total = total;
  arena->page_size = ps;

  ArenaBlock *block = (ArenaBlock *)start;
  block->prev = NULL;
  block->size = usable - sizeof(ArenaBlock);
  block->used = 0;
  arena->current = block;
  arena->min_block_size = data_size;
  arena->temp_count = 0;

  return start;
}

#endif /* !_WIN32 */

#endif /* LIB_ARENA_H */
