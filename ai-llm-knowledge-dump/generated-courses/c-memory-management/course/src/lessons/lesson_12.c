/* lesson_12.c — Arena Growth: Chained Blocks
 * BUILD_DEPS: common/bench.c
 *
 * Demonstrates how the arena grows by chaining blocks, and proves
 * that pointers remain stable across block boundaries.
 *
 * Run:  ./build-dev.sh --lesson=12 -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 12 — Arena Growth: Chained Blocks\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Watch the arena chain blocks ────────────────────────────────
   *
   * Ryan Fleury: "You can allocate a new block and start pushing onto
   * that block.  The blocks are discontiguous but you can still keep
   * pushing onto the arena."
   */
  print_section("Block Chaining in Action");
  {
    Arena arena = {0};
    arena.min_block_size = 256;  /* tiny blocks to force chaining quickly */

    printf("  Arena with 256-byte blocks (tiny, for demonstration).\n\n");

    /* Allocate enough to force multiple blocks */
    void *ptrs[20];
    for (int i = 0; i < 20; i++) {
      ptrs[i] = arena_push_size(&arena, 64, 8);
      printf("  alloc[%2d] = %p", i, ptrs[i]);

      /* Show when we're in a new block */
      if (i > 0) {
        ptrdiff_t diff = (uint8_t *)ptrs[i] - (uint8_t *)ptrs[i-1];
        if (diff < 0 || diff > 256) {
          printf("  ← NEW BLOCK (jumped %td bytes)", diff);
        }
      }
      printf("\n");
    }

    /* Count blocks */
    int block_count = 0;
    ArenaBlock *b = arena.current;
    while (b) { block_count++; b = b->prev; }
    printf("\n  Total blocks chained: %d\n", block_count);
    printf("  Total allocated: %zu bytes across all blocks\n",
           arena_total_allocated(&arena));
    printf("  Total used: %zu bytes\n", arena_total_used(&arena));

    arena_free(&arena);
  }

  /* ── 2. Pointer stability proof ─────────────────────────────────────
   *
   * Ryan Fleury: "Those pointers all remain stable.  You can take
   * pointers to them before, after the growth, doesn't matter."
   *
   * This is a KEY advantage over realloc / std::vector / Go slices,
   * which invalidate all pointers when they grow.
   */
  print_section("Pointer Stability Proof");
  {
    Arena arena = {0};
    arena.min_block_size = 128;  /* very small to force growth */

    /* Store pointers and values BEFORE growth */
    int *first  = ARENA_PUSH_STRUCT(&arena, int);
    *first = 11111;
    int *second = ARENA_PUSH_STRUCT(&arena, int);
    *second = 22222;

    printf("  Before growth:\n");
    printf("    first  = %p → %d\n", (void *)first,  *first);
    printf("    second = %p → %d\n", (void *)second, *second);

    /* Force multiple block growths */
    for (int i = 0; i < 100; i++) {
      arena_push_size(&arena, 64, 8);
    }

    printf("\n  After 100 more allocations (forced block growth):\n");
    printf("    first  = %p → %d  %s\n",
           (void *)first, *first,
           (*first == 11111) ? "STABLE" : "CORRUPTED!");
    printf("    second = %p → %d  %s\n",
           (void *)second, *second,
           (*second == 22222) ? "STABLE" : "CORRUPTED!");

    printf("\n  Pointers to earlier allocations remain valid!\n");
    printf("  This is because new blocks don't move old data.\n");
    printf("  (Unlike realloc, which copies and frees the old buffer.)\n");

    arena_free(&arena);
  }

  /* ── 3. Block chain visualization ───────────────────────────────────*/
  print_section("Block Chain Visualization");
  {
    Arena arena = {0};
    arena.min_block_size = 512;

    /* Push enough to get 4-5 blocks */
    for (int i = 0; i < 10; i++) {
      arena_push_size(&arena, 256, 8);
    }

    printf("  Arena block chain (newest → oldest):\n\n");
    ArenaBlock *block = arena.current;
    int idx = 0;
    while (block) {
      void *data_start = (void *)(block + 1);
      void *data_end   = (uint8_t *)data_start + block->size;
      printf("  Block %d: %p  size=%zu  used=%zu  [%p — %p]\n",
             idx, (void *)block, block->size, block->used,
             data_start, data_end);
      if (block->prev) {
        printf("    ↓ prev\n");
      } else {
        printf("    ↓ NULL (first block)\n");
      }
      block = block->prev;
      idx++;
    }

    arena_free(&arena);
  }

  /* ── 4. Chained blocks vs realloc ───────────────────────────────────*/
  print_section("Why Chaining Beats realloc");
  printf("  realloc (C stdlib) / std::vector (C++) / Go slices:\n");
  printf("    1. Allocate new, larger buffer\n");
  printf("    2. Copy ALL old data to new buffer\n");
  printf("    3. Free old buffer\n");
  printf("    4. ALL pointers to old data are INVALIDATED\n\n");
  printf("  Arena chained blocks:\n");
  printf("    1. Allocate a new block\n");
  printf("    2. Link it to the chain\n");
  printf("    3. Start pushing onto the new block\n");
  printf("    4. Old pointers remain VALID — old blocks untouched\n\n");
  printf("  Trade-off: chained blocks aren't contiguous in memory.\n");
  printf("  But for an arena (push-only, bulk-free), this is fine.\n");
  printf("  The allocator's job is lifetime management, not layout.\n");

  printf("\n");
  return 0;
}
