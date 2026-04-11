#ifndef UTILS_ARENA_H
#define UTILS_ARENA_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/arena.h — Multi-block growable arena allocator
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 16 — Arena allocator with TempMemory checkpoints.
 *
 * DESIGN PHILOSOPHY (from Handmade Hero)
 * ────────────────────────────────────────
 * Instead of asking "who owns this memory?", ask "which arena is it in?"
 * Each arena has a lifetime (perm = session, scratch = per-frame).
 * Allocating is O(1): advance a pointer.  Freeing is O(1): reset the arena.
 * No fragmentation.  No per-allocation overhead.
 *
 * LESSON 16 — ZII: "Zero Is Initialization"
 * ─────────────────────────────────────────
 * A zero-initialized Arena is valid but empty (current == NULL).
 * Calling arena_push_size on it allocates the first block automatically.
 *
 * PERM vs SCRATCH
 * ───────────────
 * perm   — game-init lifetime.  Allocate once; never reset.  Tilemaps,
 *          entity tables, loaded assets.  Reset only at program exit.
 * scratch — per-frame lifetime.  Wrap each frame in a TempMemory checkpoint;
 *           all frame allocations are freed by arena_end_temp at frame end.
 *           Growing a scratch arena is safe — overflow allocates a new block
 *           which arena_end_temp frees automatically.
 *
 * COMPARISON WITH HANDMADE HERO
 * ──────────────────────────────
 * HH uses VirtualAlloc / mmap for backing memory; this course uses malloc
 * for portability.  The arena interface is identical.  The production upgrade
 * path (mmap + guard pages) is documented in engine/_common/memory.h.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memset */

/* POSIX mmap headers — used by arena_alloc_mmap / arena_free_mmap.
 * On Windows we fall back to malloc (same arena interface, no guard pages). */
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
 *
 * This "header-in-allocation" trick avoids a separate pointer field —
 * the same technique used in Handmade Hero's platform_memory_block.
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
                               /* must be 0 at frame end — checked by       */
                               /* arena_check()                             */
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
 * Returns NULL on allocation failure (OOM).  The caller decides how to
 * handle failure — in a game context this typically means abort().
 *
 * `align` must be a power of two (1, 2, 4, 8, 16 …).
 * Use the ARENA_PUSH_* macros instead of calling this directly — they pass
 * the correct alignment automatically via _Alignof(Type).
 */
static inline void *arena_push_size(Arena *arena, size_t size, size_t align) {
  size_t alignment_offset = 0;

  if (arena->current) {
    /* Compute how many padding bytes we need to satisfy the alignment
     * requirement at the current write position.                           */
    uintptr_t base = (uintptr_t)(arena->current + 1) + arena->current->used;
    uintptr_t mask = align - 1;
    if (base & mask)
      alignment_offset = align - (base & mask);
  }

  /* If the current block is full (or absent), allocate a new one. */
  if (!arena->current ||
      arena->current->used + alignment_offset + size > arena->current->size) {

    if (!arena->min_block_size)
      arena->min_block_size = 1024u * 1024u;  /* 1 MB default */

    /* New block is at least min_block_size; can be larger for big requests. */
    size_t block_size = size > arena->min_block_size
                      ? size : arena->min_block_size;

    ArenaBlock *block = (ArenaBlock *)malloc(sizeof(ArenaBlock) + block_size);
    if (!block) return NULL;  /* OOM — caller handles */

    block->prev    = arena->current;
    block->size    = block_size;
    block->used    = 0;
    arena->current = block;
    alignment_offset = 0;  /* malloc guarantees max alignment at block start */
  }

  /* Advance past alignment padding, then claim the requested bytes. */
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
 * ARENA_PUSH_STRUCT(arena, Type)
 *   — uninitialized struct allocation (fast; caller fills every field)
 *
 * ARENA_PUSH_ARRAY(arena, Count, Type)
 *   — uninitialized array allocation
 *
 * ARENA_PUSH_ZERO_STRUCT / ARENA_PUSH_ZERO_ARRAY
 *   — zero-initialized versions (safe default for complex structs)
 *
 * All macros use _Alignof(Type) so alignment is always correct.
 * Cast the result to the expected type automatically.
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
 *
 * arena_keep_temp use case: conditional asset loading —
 *   TempMemory t = arena_begin_temp(&perm);
 *   Asset *a = load_asset(&perm, path);
 *   if (a) arena_keep_temp(t);   // keep it
 *   else   arena_end_temp(t);    // roll back on failure
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

  /* Free any blocks that were allocated after the checkpoint. */
  while (arena->current != t.block) {
    ArenaBlock *to_free = arena->current;
    arena->current = to_free->prev;
    free(to_free);
  }

  /* Restore used-count in the checkpoint block. */
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
 * Initialise an arena with a pre-allocated backing block (from malloc or
 * a static array).  Used by platform_game_props_init to set up perm and
 * scratch arenas from the initial malloc in platform.h.
 *
 * The arena will grow automatically (via malloc) if the initial block fills.
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
 * Free every block in the chain.  After this call the arena is empty
 * (equivalent to a zero-initialized Arena).
 *
 * CAUTION: do NOT call this on an arena whose first block was provided by
 * the caller (e.g. a static array or arena_alloc_mmap) — use only when
 * ALL blocks came from malloc/arena_push_size overflow.
 * For mmap-backed arenas use arena_free_mmap instead.
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
 * LESSON 16 — Production upgrade path
 * ────────────────────────────────────
 * The arena interface (Arena, TempMemory, macros) is identical whether the
 * backing memory comes from malloc or mmap.  The only difference is in how
 * the initial block is allocated and freed.
 *
 * arena_alloc_mmap replaces `malloc` in platform_game_props_init.
 * arena_free_mmap  replaces `arena_free_all` in platform_game_props_free.
 *
 * WHY GUARD PAGES?
 * ────────────────
 * malloc allocates from the heap: adjacent heap objects are live memory, so
 * a write past the end of an arena block silently corrupts them.  The bug
 * shows up far from the overrun — if at all.
 *
 * mmap can place an unmapped (PROT_NONE) page immediately after the usable
 * region.  The first byte written past the end causes an immediate SIGSEGV,
 * pointing directly at the offending instruction.  No silent corruption.
 *
 * Memory layout of a single mmap'd arena backing block:
 *
 *   ┌─────────────┬──────────────────────────────────┬─────────────┐
 *   │  GUARD PAGE │  ArenaBlock hdr + data (PROT_RW) │  GUARD PAGE │
 *   │  PROT_NONE  │                                  │  PROT_NONE  │
 *   └─────────────┴──────────────────────────────────┴─────────────┘
 *   ^              ^                                               ^
 *   mmap_base      usable  (passed to arena_init_from_block)  mmap_base + mmap_total
 *
 * OVERFLOW BLOCKS
 * ───────────────
 * If the initial block fills (unlikely for correctly-sized arenas), the
 * arena_push_size overflow path allocates extra blocks via malloc.  These
 * do NOT have guard pages — they are transient scratch overflow and freed
 * immediately by arena_end_temp.  Only the initial backing block uses mmap.
 *
 * WINDOWS FALLBACK
 * ────────────────
 * On Windows the implementation falls back to malloc (no guard pages).
 * The arena interface is identical; the upgrade is transparent.
 */

/* ArenaMmap — tracks the mmap region for cleanup.
 * Stored alongside the Arena in PlatformGameProps.                       */
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
 *
 * Typical usage:
 *   ArenaMmap info;
 *   void *mem = arena_alloc_mmap(ARENA_PERM_SIZE, &info);
 *   if (!mem) return -1;
 *   arena_init_from_block(&props->perm, mem, ARENA_PERM_SIZE, ARENA_PERM_SIZE);
 *   props->perm_mmap = info;  // saved for cleanup
 */
static inline void *arena_alloc_mmap(size_t data_size, ArenaMmap *out) {
#if defined(_WIN32)
  /* Windows fallback: malloc, same interface, no guard pages. */
  size_t total = sizeof(ArenaBlock) + data_size;
  void *mem = malloc(total);
  if (!mem) { out->mmap_base = NULL; out->mmap_total = 0; out->usable = NULL; return NULL; }
  out->mmap_base  = mem;
  out->mmap_total = 0;    /* sentinel: 0 = malloc path, not mmap */
  out->usable     = mem;
  return mem;
#else
  /* POSIX: mmap with one guard page on each side. */
  long ps_long = sysconf(_SC_PAGESIZE);
  size_t ps = (ps_long > 0) ? (size_t)ps_long : 4096u;

  /* Round the usable region (ArenaBlock header + data) up to a page boundary.
   * This ensures the trailing guard page starts exactly at a page edge.    */
  size_t raw    = sizeof(ArenaBlock) + data_size;
  size_t usable = ((raw + ps - 1) / ps) * ps;
  size_t total  = ps + usable + ps;  /* guard + data + guard */

  /* Reserve the full region as PROT_NONE (inaccessible).                  */
  void *map = mmap(NULL, total, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (map == MAP_FAILED) {
    out->mmap_base = NULL; out->mmap_total = 0; out->usable = NULL;
    return NULL;
  }

  /* Make only the middle region readable + writable.                      */
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

/* arena_free_mmap — release an arena backed by arena_alloc_mmap.
 *
 * Frees any overflow blocks (malloc'd by arena_push_size) by walking the
 * chain, then releases the initial mmap'd block via munmap (or free on
 * Windows).  After this call the arena is equivalent to a ZII Arena.
 *
 * info: the ArenaMmap filled by arena_alloc_mmap at init time.
 */
static inline void arena_free_mmap(Arena *arena, ArenaMmap info) {
  /* Free overflow blocks (malloc'd) — stop before the initial mmap block. */
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
