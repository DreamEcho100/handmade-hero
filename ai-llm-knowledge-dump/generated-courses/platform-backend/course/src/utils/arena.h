#ifndef UTILS_ARENA_H
#define UTILS_ARENA_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/arena.h — Arena allocator with TempMemory checkpoints
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 11 — Arena allocator.
 *
 * Instead of "who owns this memory?", ask "which arena is it in?"
 * Each arena has a lifetime (perm = session, scratch = per-frame).
 * Allocating is O(1): advance a pointer.  Freeing is O(1): reset.
 * No fragmentation.  No per-allocation overhead.
 *
 * ZII: a zero-initialized Arena is valid but empty.
 *
 * THREAD SAFETY: Arenas are NOT thread-safe. Use one arena per thread
 * or provide external synchronization (mutex). The typical game pattern
 * — one perm + one scratch on the main thread — needs no locking.
 *
 * OOM: All push operations may return NULL on allocation failure.
 * In debug builds (NDEBUG not defined), an assert fires on OOM so
 * the failure is caught immediately. In release, the caller must
 * check the return value.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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

/* ── ArenaBlock ──────────────────────────────────────────────────────────
 * A single contiguous block. Blocks form a linked list (newest first).
 * Header lives at the start of the allocation; data at (ArenaBlock*)+1.
 */
typedef struct ArenaBlock {
  struct ArenaBlock *prev;
  size_t size;
  size_t used;
} ArenaBlock;

typedef enum ArenaLifetimeKind {
  ARENA_LIFETIME_UNKNOWN = 0,
  ARENA_LIFETIME_SESSION,
  ARENA_LIFETIME_LEVEL,
  ARENA_LIFETIME_FRAME,
  ARENA_LIFETIME_TEMP,
  ARENA_LIFETIME_CUSTOM,
} ArenaLifetimeKind;

typedef struct ArenaStats {
  size_t used_bytes;
  size_t peak_used_bytes;
  size_t reserved_bytes;
  size_t peak_reserved_bytes;
  size_t total_requested_bytes;
  size_t total_alignment_bytes;
  size_t allocation_count;
  size_t block_count;
  size_t peak_block_count;
  size_t overflow_block_count;
  size_t peak_temp_scope_count;
} ArenaStats;

/* ── Arena ───────────────────────────────────────────────────────────────
 * The arena handle. ZII: zero-initialized Arena is valid and empty.
 *
 * backing_base/backing_total track the initial mmap region for cleanup.
 * If backing_total == 0, the initial block came from malloc.
 * page_size caches sysconf(_SC_PAGESIZE) from arena_alloc so
 * arena_free doesn't need to call sysconf again.
 */
typedef struct Arena {
  ArenaBlock *current;
  size_t min_block_size;
  int temp_count;
  void *backing_base;
  size_t backing_total;
  size_t page_size; /* cached from arena_alloc (0 if malloc-backed) */
  ArenaLifetimeKind lifetime_kind;
  const char *debug_name;
  ArenaStats stats;
} Arena;

/* ── TempMemory ──────────────────────────────────────────────────────────
 * Saved checkpoint. arena_end_temp frees everything allocated since.
 *
 * NOTE: TempMemory on an empty arena (current == NULL) is valid but
 * has no rewind effect — the first allocation after begin_temp behaves
 * like a fresh arena. This is the expected ZII behavior.
 */
typedef struct TempMemory {
  Arena *arena;
  ArenaBlock *block;
  size_t used;
} TempMemory;

typedef enum ArenaInitMode { ARENA_INIT_RAW, ARENA_INIT_ZERO } ArenaInitMode;

static inline void *arena_push_size(Arena *arena, size_t size, size_t align);
static inline void *arena_push_zero(Arena *arena, size_t size, size_t align);

typedef struct PoolFreeNode {
  struct PoolFreeNode *next;
} PoolFreeNode;

typedef struct PoolAllocator {
  void *memory;
  PoolFreeNode *free_list;
  size_t slot_size;
  size_t slot_count;
  size_t free_count;
  size_t high_watermark;
  int owns_memory;
  Arena *backing_arena;
  const char *debug_name;
} PoolAllocator;

typedef struct SlabPage {
  struct SlabPage *next;
  void *memory;
  PoolFreeNode *free_list;
  size_t slot_count;
  size_t free_count;
} SlabPage;

typedef struct SlabAllocator {
  SlabPage *pages;
  size_t slot_size;
  size_t slots_per_page;
  size_t page_count;
  size_t total_slots;
  size_t free_slots;
  size_t high_watermark;
  Arena *backing_arena;
  int owns_pages;
  const char *debug_name;
} SlabAllocator;

static inline int arena_is_power_of_two(size_t value) {
  return value > 0 && (value & (value - 1)) == 0;
}

static inline size_t arena_align_size_up(size_t value, size_t align) {
  assert(arena_is_power_of_two(align) && "align must be a power of two");
  return (value + (align - 1)) & ~(align - 1);
}

static inline size_t arena_default_slot_alignment(void) {
  return _Alignof(max_align_t);
}

static inline const char *arena_lifetime_name(ArenaLifetimeKind kind) {
  switch (kind) {
  case ARENA_LIFETIME_SESSION:
    return "session";
  case ARENA_LIFETIME_LEVEL:
    return "level";
  case ARENA_LIFETIME_FRAME:
    return "frame";
  case ARENA_LIFETIME_TEMP:
    return "temp";
  case ARENA_LIFETIME_CUSTOM:
    return "custom";
  case ARENA_LIFETIME_UNKNOWN:
  default:
    return "unknown";
  }
}

static inline void arena_debug_configure(Arena *arena,
                                         ArenaLifetimeKind lifetime_kind,
                                         const char *debug_name) {
  arena->lifetime_kind = lifetime_kind;
  arena->debug_name = debug_name;
}

static inline const ArenaStats *arena_get_stats(const Arena *arena) {
  return &arena->stats;
}

static inline void arena_dump_stats(FILE *out, const Arena *arena,
                                    const char *label) {
  const char *name =
      label ? label : (arena->debug_name ? arena->debug_name : "arena");
  FILE *dst = out ? out : stderr;
  fprintf(dst,
          "[arena:%s] lifetime=%s used=%zu peak_used=%zu reserved=%zu "
          "peak_reserved=%zu allocs=%zu requested=%zu padding=%zu blocks=%zu "
          "peak_blocks=%zu overflow_blocks=%zu peak_temp_scopes=%zu\n",
          name, arena_lifetime_name(arena->lifetime_kind),
          arena->stats.used_bytes, arena->stats.peak_used_bytes,
          arena->stats.reserved_bytes, arena->stats.peak_reserved_bytes,
          arena->stats.allocation_count, arena->stats.total_requested_bytes,
          arena->stats.total_alignment_bytes, arena->stats.block_count,
          arena->stats.peak_block_count, arena->stats.overflow_block_count,
          arena->stats.peak_temp_scope_count);
}

static inline size_t pool_slot_size_aligned(size_t slot_size) {
  size_t min_size =
      slot_size < sizeof(PoolFreeNode) ? sizeof(PoolFreeNode) : slot_size;
  return arena_align_size_up(min_size, arena_default_slot_alignment());
}

static inline void pool_build_free_list(PoolAllocator *pool) {
  size_t index;
  pool->free_list = NULL;
  for (index = pool->slot_count; index > 0; --index) {
    uint8_t *slot = (uint8_t *)pool->memory + ((index - 1) * pool->slot_size);
    PoolFreeNode *node = (PoolFreeNode *)slot;
    node->next = pool->free_list;
    pool->free_list = node;
  }
  pool->free_count = pool->slot_count;
  pool->high_watermark = 0;
}

static inline int pool_contains(const PoolAllocator *pool, const void *ptr) {
  const uint8_t *base = (const uint8_t *)pool->memory;
  const uint8_t *end = base + (pool->slot_count * pool->slot_size);
  const uint8_t *value = (const uint8_t *)ptr;
  if (!pool->memory || !ptr)
    return 0;
  if (value < base || value >= end)
    return 0;
  return (((size_t)(value - base)) % pool->slot_size) == 0;
}

static inline int pool_init_from_memory(PoolAllocator *pool, void *memory,
                                        size_t slot_size, size_t slot_count,
                                        const char *debug_name) {
  if (!pool || !memory || slot_count == 0)
    return -1;
  memset(pool, 0, sizeof(*pool));
  pool->memory = memory;
  pool->slot_size = pool_slot_size_aligned(slot_size);
  pool->slot_count = slot_count;
  pool->debug_name = debug_name;
  pool_build_free_list(pool);
  return 0;
}

static inline int pool_init_heap(PoolAllocator *pool, size_t slot_size,
                                 size_t slot_count, const char *debug_name) {
  size_t aligned = pool_slot_size_aligned(slot_size);
  void *memory;
  if (!pool || slot_count == 0)
    return -1;
  memory = malloc(aligned * slot_count);
  if (!memory)
    return -1;
  if (pool_init_from_memory(pool, memory, aligned, slot_count, debug_name) !=
      0) {
    free(memory);
    return -1;
  }
  pool->owns_memory = 1;
  return 0;
}

static inline int pool_init_in_arena(PoolAllocator *pool, Arena *arena,
                                     size_t slot_size, size_t slot_count,
                                     const char *debug_name) {
  size_t aligned = pool_slot_size_aligned(slot_size);
  void *memory;
  if (!pool || !arena || slot_count == 0)
    return -1;
  memory = arena_push_zero(arena, aligned * slot_count,
                           arena_default_slot_alignment());
  if (!memory)
    return -1;
  if (pool_init_from_memory(pool, memory, aligned, slot_count, debug_name) != 0)
    return -1;
  pool->backing_arena = arena;
  return 0;
}

static inline void *pool_alloc(PoolAllocator *pool) {
  PoolFreeNode *node;
  size_t used;
  if (!pool || !pool->free_list)
    return NULL;
  node = pool->free_list;
  pool->free_list = node->next;
  --pool->free_count;
  used = pool->slot_count - pool->free_count;
  if (used > pool->high_watermark)
    pool->high_watermark = used;
  return (void *)node;
}

static inline void pool_free(PoolAllocator *pool, void *ptr) {
  PoolFreeNode *node;
  assert(pool && "pool_free: pool must not be NULL");
  assert(pool_contains(pool, ptr) &&
         "pool_free: pointer does not belong to pool");
  node = (PoolFreeNode *)ptr;
  node->next = pool->free_list;
  pool->free_list = node;
  assert(pool->free_count < pool->slot_count &&
         "pool_free: double free or corrupt free_count");
  ++pool->free_count;
}

static inline void pool_reset(PoolAllocator *pool) {
  if (!pool || !pool->memory)
    return;
  pool_build_free_list(pool);
}

static inline void pool_destroy(PoolAllocator *pool) {
  if (!pool)
    return;
  if (pool->owns_memory && pool->memory)
    free(pool->memory);
  memset(pool, 0, sizeof(*pool));
}

static inline SlabPage *slab_alloc_page(SlabAllocator *allocator) {
  SlabPage *page;
  size_t index;
  size_t bytes = allocator->slot_size * allocator->slots_per_page;

  if (allocator->backing_arena) {
    page = (SlabPage *)arena_push_zero(allocator->backing_arena,
                                       sizeof(SlabPage), _Alignof(SlabPage));
    if (!page)
      return NULL;
    page->memory = arena_push_zero(allocator->backing_arena, bytes,
                                   arena_default_slot_alignment());
    if (!page->memory)
      return NULL;
  } else {
    page = (SlabPage *)calloc(1, sizeof(SlabPage));
    if (!page)
      return NULL;
    page->memory = calloc(1, bytes);
    if (!page->memory) {
      free(page);
      return NULL;
    }
  }

  page->slot_count = allocator->slots_per_page;
  page->free_count = allocator->slots_per_page;
  page->free_list = NULL;
  for (index = allocator->slots_per_page; index > 0; --index) {
    uint8_t *slot =
        (uint8_t *)page->memory + ((index - 1) * allocator->slot_size);
    PoolFreeNode *node = (PoolFreeNode *)slot;
    node->next = page->free_list;
    page->free_list = node;
  }

  page->next = allocator->pages;
  allocator->pages = page;
  ++allocator->page_count;
  allocator->total_slots += allocator->slots_per_page;
  allocator->free_slots += allocator->slots_per_page;
  return page;
}

static inline int slab_page_contains(const SlabAllocator *allocator,
                                     const SlabPage *page, const void *ptr) {
  const uint8_t *base = (const uint8_t *)page->memory;
  const uint8_t *end = base + (allocator->slot_size * page->slot_count);
  const uint8_t *value = (const uint8_t *)ptr;
  if (!page->memory || !ptr)
    return 0;
  if (value < base || value >= end)
    return 0;
  return (((size_t)(value - base)) % allocator->slot_size) == 0;
}

static inline void slab_link_free_slot(SlabPage *page, void *ptr) {
  PoolFreeNode *node = (PoolFreeNode *)ptr;
  node->next = page->free_list;
  page->free_list = node;
  ++page->free_count;
}

static inline void slab_free_empty_pages(SlabAllocator *allocator) {
  SlabPage *prev = NULL;
  SlabPage *page = allocator->pages;
  while (page) {
    SlabPage *next = page->next;
    if (!allocator->backing_arena && allocator->page_count > 1 &&
        page->free_count == page->slot_count) {
      if (prev)
        prev->next = next;
      else
        allocator->pages = next;
      free(page->memory);
      free(page);
      --allocator->page_count;
      allocator->total_slots -= allocator->slots_per_page;
      allocator->free_slots -= allocator->slots_per_page;
    } else {
      prev = page;
    }
    page = next;
  }
}

static inline int slab_init(SlabAllocator *allocator, size_t slot_size,
                            size_t slots_per_page, const char *debug_name) {
  if (!allocator || slot_size == 0 || slots_per_page == 0)
    return -1;
  memset(allocator, 0, sizeof(*allocator));
  allocator->slot_size = pool_slot_size_aligned(slot_size);
  allocator->slots_per_page = slots_per_page;
  allocator->debug_name = debug_name;
  allocator->owns_pages = 1;
  return slab_alloc_page(allocator) ? 0 : -1;
}

static inline int slab_init_in_arena(SlabAllocator *allocator, Arena *arena,
                                     size_t slot_size, size_t slots_per_page,
                                     const char *debug_name) {
  if (!allocator || !arena || slot_size == 0 || slots_per_page == 0)
    return -1;
  memset(allocator, 0, sizeof(*allocator));
  allocator->slot_size = pool_slot_size_aligned(slot_size);
  allocator->slots_per_page = slots_per_page;
  allocator->backing_arena = arena;
  allocator->debug_name = debug_name;
  allocator->owns_pages = 0;
  return slab_alloc_page(allocator) ? 0 : -1;
}

static inline void *slab_alloc(SlabAllocator *allocator) {
  SlabPage *page;
  PoolFreeNode *node;
  size_t used;
  if (!allocator)
    return NULL;
  for (page = allocator->pages; page; page = page->next) {
    if (page->free_list)
      break;
  }
  if (!page) {
    page = slab_alloc_page(allocator);
    if (!page)
      return NULL;
  }
  node = page->free_list;
  page->free_list = node->next;
  --page->free_count;
  --allocator->free_slots;
  used = allocator->total_slots - allocator->free_slots;
  if (used > allocator->high_watermark)
    allocator->high_watermark = used;
  return (void *)node;
}

static inline void slab_free(SlabAllocator *allocator, void *ptr) {
  SlabPage *page;
  assert(allocator && "slab_free: allocator must not be NULL");
  for (page = allocator->pages; page; page = page->next) {
    if (slab_page_contains(allocator, page, ptr)) {
      slab_link_free_slot(page, ptr);
      ++allocator->free_slots;
      if (!allocator->backing_arena)
        slab_free_empty_pages(allocator);
      return;
    }
  }
  assert(!"slab_free: pointer does not belong to allocator");
}

static inline void slab_reset(SlabAllocator *allocator) {
  SlabPage *page;
  if (!allocator)
    return;
  allocator->free_slots = 0;
  for (page = allocator->pages; page; page = page->next) {
    size_t index;
    page->free_list = NULL;
    page->free_count = page->slot_count;
    for (index = page->slot_count; index > 0; --index) {
      uint8_t *slot =
          (uint8_t *)page->memory + ((index - 1) * allocator->slot_size);
      PoolFreeNode *node = (PoolFreeNode *)slot;
      node->next = page->free_list;
      page->free_list = node;
    }
    allocator->free_slots += page->slot_count;
  }
}

static inline void slab_destroy(SlabAllocator *allocator) {
  SlabPage *page;
  SlabPage *next;
  if (!allocator)
    return;
  if (!allocator->backing_arena) {
    for (page = allocator->pages; page; page = next) {
      next = page->next;
      free(page->memory);
      free(page);
    }
  }
  memset(allocator, 0, sizeof(*allocator));
}

/* ── arena_push_impl ─────────────────────────────────────────────────────
 * Shared implementation for raw and zeroed allocations.
 * RAW allocations are poisoned with 0xCD in debug builds.
 * ZERO allocations are explicitly memset to 0.
 * `align` MUST be a power of two (asserted in debug builds).
 * Returns NULL on OOM (asserted in debug builds).
 */
static inline void *arena_push_impl(Arena *arena, size_t size, size_t align,
                                    ArenaInitMode mode) {
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

  /* Overflow-safe capacity check: rearranged to avoid size_t wraparound.
   * Instead of: used + offset + size > capacity (can overflow)
   * We check:   used > capacity - needed       (no overflow if needed <=
   * capacity) */
  {
    size_t needed = alignment_offset + size;
    int needs_new_block = !arena->current || needed > arena->current->size ||
                          arena->current->used > arena->current->size - needed;

    if (needs_new_block) {
      if (!arena->min_block_size) {
        arena->min_block_size = 1024u * 1024u;
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

      ++arena->stats.block_count;
      if (arena->stats.block_count > arena->stats.peak_block_count)
        arena->stats.peak_block_count = arena->stats.block_count;
      arena->stats.reserved_bytes += block_size;
      if (arena->stats.reserved_bytes > arena->stats.peak_reserved_bytes)
        arena->stats.peak_reserved_bytes = arena->stats.reserved_bytes;
      if (block->prev)
        ++arena->stats.overflow_block_count;

      uintptr_t new_base = (uintptr_t)(block + 1);
      uintptr_t new_mask = align - 1;
      alignment_offset =
          (new_base & new_mask) ? align - (new_base & new_mask) : 0;
    }
  }

  arena->current->used += alignment_offset;
  void *result = (uint8_t *)(arena->current + 1) + arena->current->used;
  arena->current->used += size;

  arena->stats.used_bytes += alignment_offset + size;
  if (arena->stats.used_bytes > arena->stats.peak_used_bytes)
    arena->stats.peak_used_bytes = arena->stats.used_bytes;
  arena->stats.total_requested_bytes += size;
  arena->stats.total_alignment_bytes += alignment_offset;
  ++arena->stats.allocation_count;

  if (mode == ARENA_INIT_ZERO) {
    memset(result, 0, size);
  }
#ifndef NDEBUG
  else {
    memset(result, ARENA_DEBUG_FILL_ALLOC, size);
  }
#endif

  return result;
}

/* ── arena_push_size ─────────────────────────────────────────────────────
 * Raw allocation path. Memory contents are unspecified; in debug builds
 * the returned bytes are poisoned with 0xCD to catch uninitialized reads.
 */
static inline void *arena_push_size(Arena *arena, size_t size, size_t align) {
  return arena_push_impl(arena, size, align, ARENA_INIT_RAW);
}

/* ── arena_push_zero ─────────────────────────────────────────────────────
 * Zero-initialized allocation path. Returned bytes are guaranteed to be 0.
 */
static inline void *arena_push_zero(Arena *arena, size_t size, size_t align) {
  return arena_push_impl(arena, size, align, ARENA_INIT_ZERO);
}

/* ── Type-safe push macros ───────────────────────────────────────────── */

#define ARENA_PUSH_STRUCT(arena, Type)                                         \
  ((Type *)arena_push_size((arena), sizeof(Type), _Alignof(Type)))

#define ARENA_PUSH_ARRAY(arena, Count, Type)                                   \
  ((Type *)arena_push_size((arena), (size_t)(Count) * sizeof(Type),            \
                           _Alignof(Type)))

#define ARENA_PUSH_ZERO_STRUCT(arena, Type)                                    \
  ((Type *)arena_push_zero((arena), sizeof(Type), _Alignof(Type)))

#define ARENA_PUSH_ZERO_ARRAY(arena, Count, Type)                              \
  ((Type *)arena_push_zero((arena), (size_t)(Count) * sizeof(Type),            \
                           _Alignof(Type)))

/* ── arena_push_string — copy a C string into the arena ──────────────── */
static inline char *arena_push_string(Arena *arena, const char *str) {
  if (!str)
    return NULL;
  size_t len = strlen(str) + 1; /* +1 for null terminator */
  char *copy = (char *)arena_push_size(arena, len, 1);
  if (copy)
    memcpy(copy, str, len);
  return copy;
}

/* ── TempMemory functions ────────────────────────────────────────────── */

static inline TempMemory arena_begin_temp(Arena *arena) {
  TempMemory t;
  t.arena = arena;
  t.block = arena->current;
  t.used = arena->current ? arena->current->used : 0;
  ++arena->temp_count;
  if ((size_t)arena->temp_count > arena->stats.peak_temp_scope_count)
    arena->stats.peak_temp_scope_count = (size_t)arena->temp_count;
  return t;
}

static inline void arena_end_temp(TempMemory t) {
  Arena *arena = t.arena;
  size_t freed_consumed = 0;
  while (arena->current != t.block) {
    ArenaBlock *to_free = arena->current;
    arena->current = to_free->prev;
    freed_consumed += to_free->used;
    if (arena->stats.block_count > 0)
      --arena->stats.block_count;
    if (arena->stats.reserved_bytes >= to_free->size)
      arena->stats.reserved_bytes -= to_free->size;
#ifndef NDEBUG
    memset((uint8_t *)(to_free + 1), ARENA_DEBUG_FILL_FREE, to_free->used);
#endif
    free(to_free);
  }
  if (arena->current) {
#ifndef NDEBUG
    /* Fill freed region with debug pattern */
    size_t freed_bytes = arena->current->used - t.used;
    if (freed_bytes > 0)
      memset((uint8_t *)(arena->current + 1) + t.used, ARENA_DEBUG_FILL_FREE,
             freed_bytes);
#endif
    freed_consumed += arena->current->used - t.used;
    arena->current->used = t.used;
  }
  if (arena->stats.used_bytes >= freed_consumed)
    arena->stats.used_bytes -= freed_consumed;
  else
    arena->stats.used_bytes = 0;
  assert(arena->temp_count > 0);
  --arena->temp_count;
}

static inline void arena_keep_temp(TempMemory t) {
  assert(t.arena->temp_count > 0);
  --t.arena->temp_count;
}

static inline void arena_check(const Arena *arena) {
  assert(arena->temp_count == 0 &&
         "arena_check: unmatched arena_begin_temp / arena_end_temp");
}

/* ── arena_init_from_block ───────────────────────────────────────────── */

static inline void arena_init_from_block(Arena *arena, void *mem,
                                         size_t total_bytes,
                                         size_t min_block_size) {
  ArenaBlock *block = (ArenaBlock *)mem;
  block->prev = NULL;
  block->size = total_bytes - sizeof(ArenaBlock);
  block->used = 0;
  arena->current = block;
  arena->min_block_size = min_block_size;
  arena->temp_count = 0;
  arena->stats.used_bytes = 0;
  arena->stats.peak_used_bytes = 0;
  arena->stats.reserved_bytes = block->size;
  arena->stats.peak_reserved_bytes = block->size;
  arena->stats.total_requested_bytes = 0;
  arena->stats.total_alignment_bytes = 0;
  arena->stats.allocation_count = 0;
  arena->stats.block_count = 1;
  arena->stats.peak_block_count = 1;
  arena->stats.overflow_block_count = 0;
  arena->stats.peak_temp_scope_count = 0;
  /* backing_base, backing_total, page_size preserved from arena_alloc */
}

static inline void arena_reset(Arena *arena) {
  while (arena->current && arena->current->prev) {
    ArenaBlock *b = arena->current;
    arena->current = b->prev;
    free(b);
  }
  if (arena->current)
    arena->current->used = 0;
  arena->temp_count = 0; /* prevent false arena_check failures after reset */
  arena->stats.used_bytes = 0;
  arena->stats.reserved_bytes = arena->current ? arena->current->size : 0;
  arena->stats.block_count = arena->current ? 1u : 0u;
}

/* ── arena_alloc — allocate backing memory with guard pages ──────────── */

static inline void *arena_alloc(Arena *arena, size_t data_size) {
#if defined(_WIN32)
  size_t total = sizeof(ArenaBlock) + data_size;
  void *mem = malloc(total);
  if (!mem)
    return NULL;
  arena->backing_base = mem;
  arena->backing_total = 0;
  arena->page_size = 0;
  return mem;
#else
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
  return start;
#endif
}

static inline int arena_bootstrap(Arena *arena, size_t data_size,
                                  size_t min_block_size,
                                  ArenaLifetimeKind lifetime_kind,
                                  const char *debug_name) {
  void *mem;
  if (!arena)
    return -1;
  memset(arena, 0, sizeof(*arena));
  mem = arena_alloc(arena, data_size);
  if (!mem)
    return -1;
  arena_init_from_block(arena, mem, data_size, min_block_size);
  arena_debug_configure(arena, lifetime_kind, debug_name);
  return 0;
}

/* ── arena_free — release everything ─────────────────────────────────── */

static inline void arena_free(Arena *arena) {
  void *initial_block = NULL;
  if (arena->backing_base) {
    if (arena->backing_total > 0 && arena->page_size > 0) {
      /* mmap path: usable region = backing_base + page_size */
      initial_block = (uint8_t *)arena->backing_base + arena->page_size;
    } else {
      /* malloc path: initial block IS backing_base */
      initial_block = arena->backing_base;
    }
  }

  while (arena->current) {
    ArenaBlock *b = arena->current;
    arena->current = b->prev;
    if ((void *)b != initial_block)
      free(b);
  }

#if defined(_WIN32)
  if (arena->backing_base)
    free(arena->backing_base);
#else
  if (arena->backing_base && arena->backing_total > 0)
    munmap(arena->backing_base, arena->backing_total);
  else if (arena->backing_base)
    free(arena->backing_base);
#endif

  memset(arena, 0, sizeof(*arena));
}

#endif /* UTILS_ARENA_H */
