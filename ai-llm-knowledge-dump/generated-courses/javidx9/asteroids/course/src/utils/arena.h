#ifndef UTILS_ARENA_H
#define UTILS_ARENA_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/arena.h — Multi-block growable arena allocator
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * DESIGN PHILOSOPHY (from Handmade Hero)
 * ────────────────────────────────────────
 * Instead of asking "who owns this memory?", ask "which arena is it in?"
 * Each arena has a lifetime (perm = session, scratch = per-frame).
 * Allocating is O(1): advance a pointer.  Freeing is O(1): reset the arena.
 * No fragmentation.  No per-allocation overhead.
 *
 * ZII: "Zero Is Initialization"
 * ─────────────────────────────
 * A zero-initialized Arena is valid but empty (current == NULL).
 * Calling arena_push_size on it allocates the first block automatically.
 *
 * PERM vs SCRATCH
 * ───────────────
 * perm   — game-init lifetime.  Allocate once; never reset.  Entity tables,
 *          loaded assets.  Reset only at program exit.
 * scratch — per-frame lifetime.  Wrap each frame in a TempMemory checkpoint;
 *           all frame allocations are freed by arena_end_temp at frame end.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memset */

/* POSIX mmap headers — used by arena_alloc_mmap / arena_free_mmap. */
#if !defined(_WIN32)
#  include <sys/mman.h>  /* mmap, munmap, mprotect, PROT_*, MAP_* */
#  include <unistd.h>    /* sysconf, _SC_PAGESIZE                  */
#endif

/* ── ArenaBlock ───────────────────────────────────────────────────────────
 *
 * A single contiguous slab of backing memory.  Blocks form a singly-linked
 * list (newest first via `prev`).  The block header lives at the start of
 * the malloc'd allocation; usable data starts at `(ArenaBlock *) + 1`.
 *
 * Layout in memory:
 *   [ ArenaBlock header | ........ usable data ........ ]
 *   ^                    ^
 *   malloc'd ptr         (ArenaBlock*) + 1
 */
typedef struct ArenaBlock {
  struct ArenaBlock *prev;  /* previous (older) block, or NULL              */
  size_t             size;  /* usable bytes in this block (after the header) */
  size_t             used;  /* bytes consumed so far in this block           */
} ArenaBlock;

/* ── Arena ────────────────────────────────────────────────────────────────
 *
 * The arena handle.  Small enough to pass by pointer everywhere.
 * ZII: a zero-initialized Arena is valid and empty.
 */
typedef struct Arena {
  ArenaBlock *current;         /* head of block chain (most recent block)   */
  size_t      min_block_size;  /* minimum size for newly allocated blocks   */
  int         temp_count;      /* outstanding arena_begin_temp calls        */
} Arena;

/* ── TempMemory ───────────────────────────────────────────────────────────
 *
 * A saved checkpoint into an Arena.  Created by arena_begin_temp().
 * Restoring with arena_end_temp() frees everything allocated since the
 * checkpoint, including any overflow blocks.
 *
 * Usage pattern (per-frame scratch):
 *   TempMemory frame = arena_begin_temp(&props.scratch);
 *   // ... frame allocations ...
 *   arena_end_temp(frame);    // all frame allocs freed
 *   arena_check(&props.scratch);  // assert no orphaned begin_temp calls
 */
typedef struct TempMemory {
  Arena      *arena;  /* the arena this checkpoint belongs to               */
  ArenaBlock *block;  /* block that was current at checkpoint time          */
  size_t      used;   /* used-count in that block at checkpoint time        */
} TempMemory;

/* ── arena_push_size ──────────────────────────────────────────────────────
 *
 * The core allocator.  Returns a pointer to `size` bytes aligned to `align`.
 * If the current block is full (or there is no current block), a new block
 * is allocated via malloc and prepended to the chain.
 *
 * Returns NULL on allocation failure (OOM).
 * `align` must be a power of two (1, 2, 4, 8, 16 …).
 */
static inline void *arena_push_size(Arena *arena, size_t size, size_t align) {
  size_t alignment_offset = 0;

  if (arena->current) {
    uintptr_t base = (uintptr_t)(arena->current + 1) + arena->current->used;
    uintptr_t mask = align - 1;
    if (base & mask)
      alignment_offset = align - (base & mask);
  }

  if (!arena->current ||
      arena->current->used + alignment_offset + size > arena->current->size) {

    if (!arena->min_block_size)
      arena->min_block_size = 1024u * 1024u;  /* 1 MB default */

    size_t block_size = size > arena->min_block_size
                      ? size : arena->min_block_size;

    ArenaBlock *block = (ArenaBlock *)malloc(sizeof(ArenaBlock) + block_size);
    if (!block) return NULL;

    block->prev    = arena->current;
    block->size    = block_size;
    block->used    = 0;
    arena->current = block;
    alignment_offset = 0;
  }

  arena->current->used += alignment_offset;
  void *result = (uint8_t *)(arena->current + 1) + arena->current->used;
  arena->current->used += size;
  return result;
}

/* arena_push_zero — like arena_push_size but zeroes the allocation. */
static inline void *arena_push_zero(Arena *arena, size_t size, size_t align) {
  void *p = arena_push_size(arena, size, align);
  if (p) memset(p, 0, size);
  return p;
}

/* ── Type-safe push macros ────────────────────────────────────────────────
 *
 * ARENA_PUSH_STRUCT(arena, Type)         — uninitialized struct allocation
 * ARENA_PUSH_ARRAY(arena, Count, Type)   — uninitialized array allocation
 * ARENA_PUSH_ZERO_STRUCT / ARENA_PUSH_ZERO_ARRAY — zero-initialized versions
 *
 * Example:
 *   Entity *e = ARENA_PUSH_ZERO_STRUCT(&arena, Entity);
 *   float  *buf = ARENA_PUSH_ARRAY(&scratch, 1024, float);
 */
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

/* ── TempMemory functions ─────────────────────────────────────────────────
 *
 * arena_begin_temp  — save checkpoint, increment temp_count.
 * arena_end_temp    — restore to checkpoint; free overflow blocks; decrement.
 * arena_keep_temp   — commit allocations (don't restore); just decrement.
 * arena_check       — assert temp_count == 0 (call at frame end).
 */

static inline TempMemory arena_begin_temp(Arena *arena) {
  TempMemory t;
  t.arena = arena;
  t.block = arena->current;
  t.used  = arena->current ? arena->current->used : 0;
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

/* arena_check — assert no outstanding TempMemory regions.
 * Call at the end of each frame to catch mismatched begin/end pairs.      */
static inline void arena_check(const Arena *arena) {
  assert(arena->temp_count == 0 &&
         "arena_check: unmatched arena_begin_temp / arena_end_temp");
}

/* ── arena_init_from_block ────────────────────────────────────────────────
 *
 * Initialise an arena with a pre-allocated backing block.
 * Used by platform_game_props_init to set up perm and scratch arenas.
 */
static inline void arena_init_from_block(Arena *arena, void *mem,
                                         size_t total_bytes,
                                         size_t min_block_size) {
  ArenaBlock *block = (ArenaBlock *)mem;
  block->prev  = NULL;
  block->size  = total_bytes - sizeof(ArenaBlock);
  block->used  = 0;
  arena->current        = block;
  arena->min_block_size = min_block_size;
  arena->temp_count     = 0;
}

/* ── arena_free_all ───────────────────────────────────────────────────────
 *
 * Free every block in the chain.  After this call the arena is empty.
 * CAUTION: do NOT call this on mmap-backed arenas — use arena_free_mmap.
 */
static inline void arena_free_all(Arena *arena) {
  while (arena->current) {
    ArenaBlock *to_free = arena->current;
    arena->current = to_free->prev;
    free(to_free);
  }
}

/* ── Production backing: mmap + guard pages ───────────────────────────────
 *
 * arena_alloc_mmap replaces `malloc` in platform_game_props_init.
 * arena_free_mmap  replaces `arena_free_all` in platform_game_props_free.
 *
 * WHY GUARD PAGES?
 * mmap places an unmapped (PROT_NONE) page immediately after the usable
 * region.  The first byte written past the end causes an immediate SIGSEGV,
 * pointing directly at the offending instruction.  No silent corruption.
 *
 * Memory layout:
 *   ┌─────────────┬──────────────────────────────────┬─────────────┐
 *   │  GUARD PAGE │  ArenaBlock hdr + data (PROT_RW) │  GUARD PAGE │
 *   │  PROT_NONE  │                                  │  PROT_NONE  │
 *   └─────────────┴──────────────────────────────────┴─────────────┘
 */

/* ArenaMmap — tracks the mmap region for cleanup. */
typedef struct ArenaMmap {
  void  *mmap_base;  /* base of the full mmap region (passed to munmap)  */
  size_t mmap_total; /* total size including guard pages (for munmap)     */
  void  *usable;     /* usable region start = initial ArenaBlock pointer  */
} ArenaMmap;

/* arena_alloc_mmap — allocate arena backing memory with guard pages.
 *
 * data_size: the amount of usable data bytes needed (ARENA_PERM_SIZE etc.)
 * out:       filled with cleanup information; pass to arena_free_mmap later.
 *
 * Returns the usable region pointer (pass directly to arena_init_from_block),
 * or NULL on failure.
 */
static inline void *arena_alloc_mmap(size_t data_size, ArenaMmap *out) {
#if defined(_WIN32)
  size_t total = sizeof(ArenaBlock) + data_size;
  void *mem = malloc(total);
  if (!mem) { out->mmap_base = NULL; out->mmap_total = 0; out->usable = NULL; return NULL; }
  out->mmap_base  = mem;
  out->mmap_total = 0;
  out->usable     = mem;
  return mem;
#else
  long ps_long = sysconf(_SC_PAGESIZE);
  size_t ps = (ps_long > 0) ? (size_t)ps_long : 4096u;

  size_t raw    = sizeof(ArenaBlock) + data_size;
  size_t usable = ((raw + ps - 1) / ps) * ps;
  size_t total  = ps + usable + ps;

  void *map = mmap(NULL, total, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (map == MAP_FAILED) {
    out->mmap_base = NULL; out->mmap_total = 0; out->usable = NULL;
    return NULL;
  }

  void *start = (uint8_t *)map + ps;
  if (mprotect(start, usable, PROT_READ | PROT_WRITE) != 0) {
    munmap(map, total);
    out->mmap_base = NULL; out->mmap_total = 0; out->usable = NULL;
    return NULL;
  }

  out->mmap_base  = map;
  out->mmap_total = total;
  out->usable     = start;
  return start;
#endif
}

/* arena_free_mmap — release an arena backed by arena_alloc_mmap. */
static inline void arena_free_mmap(Arena *arena, ArenaMmap info) {
  while (arena->current && arena->current != (ArenaBlock *)info.usable) {
    ArenaBlock *b = arena->current;
    arena->current = b->prev;
    free(b);
  }
  arena->current = NULL;

#if defined(_WIN32)
  if (info.mmap_base) free(info.mmap_base);
#else
  if (info.mmap_base && info.mmap_total > 0)
    munmap(info.mmap_base, info.mmap_total);
#endif
}

#endif /* UTILS_ARENA_H */
