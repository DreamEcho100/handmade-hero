/* lesson_22.c — Buddy Allocator Overview
 * BUILD_DEPS: common/bench.c
 *
 * A survey lesson: minimal self-contained buddy allocator implementation.
 * Power-of-two splitting and coalescing.  Focused on understanding the
 * algorithm, not production use.
 *
 * Run:   ./build-dev.sh --lesson=22 -r
 * Bench: ./build-dev.sh --lesson=22 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "common/print_utils.h"
#include "common/bench.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * SIMPLIFIED BUDDY ALLOCATOR
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The buddy system divides memory into power-of-two blocks.  When a block
 * is too large for a request, it splits in half.  When a block is freed,
 * if its "buddy" (sibling) is also free, they merge (coalesce) back into
 * a larger block.
 *
 * Our simplified version:
 *   - Total memory: must be a power of two
 *   - Minimum block: 32 bytes (MIN_ORDER = 5)
 *   - Free lists per order: free_lists[order] is a linked list
 *   - Bitmap tracks which blocks are split
 * ═══════════════════════════════════════════════════════════════════════════
 */

#define BUDDY_MIN_ORDER   5        /* 32 bytes minimum */
#define BUDDY_MAX_ORDER   20       /* 1 MB maximum (for this demo) */
#define BUDDY_NUM_ORDERS  (BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1)

/* Free list node — stored in the free block itself */
typedef struct BuddyNode {
  struct BuddyNode *next;
  struct BuddyNode *prev;
} BuddyNode;

typedef struct BuddyAlloc {
  uint8_t   *memory;                        /* backing memory             */
  size_t     total_size;                     /* total bytes (power of 2)   */
  int        max_order;                      /* log2(total_size)           */
  BuddyNode *free_lists[BUDDY_NUM_ORDERS];  /* one free list per order    */
  /* Track allocation order for each block (indexed by block offset >> MIN_ORDER) */
  uint8_t   *block_orders;                   /* order of each allocation   */
} BuddyAlloc;

/* ── Helpers ──────────────────────────────────────────────────────────────*/

static int buddy_log2(size_t n) {
  int r = 0;
  while ((1UL << r) < n) r++;
  return r;
}

static size_t buddy_order_size(int order) {
  return 1UL << order;
}

/* Get the index of a block in the block_orders array */
static size_t buddy_block_index(const BuddyAlloc *ba, const void *ptr) {
  return ((uint8_t *)ptr - ba->memory) >> BUDDY_MIN_ORDER;
}

/* Get the buddy's offset for a block at a given order */
static size_t buddy_offset(const BuddyAlloc *ba, const void *ptr, int order) {
  size_t off = (uint8_t *)ptr - ba->memory;
  return off ^ buddy_order_size(order);
}

/* ── Free list operations ────────────────────────────────────────────────*/

static void buddy_list_push(BuddyNode **list, BuddyNode *node) {
  node->prev = NULL;
  node->next = *list;
  if (*list) (*list)->prev = node;
  *list = node;
}

static void buddy_list_remove(BuddyNode **list, BuddyNode *node) {
  if (node->prev) node->prev->next = node->next;
  else *list = node->next;
  if (node->next) node->next->prev = node->prev;
  node->next = node->prev = NULL;
}

static int buddy_list_contains(BuddyNode *list, BuddyNode *node) {
  BuddyNode *n = list;
  while (n) {
    if (n == node) return 1;
    n = n->next;
  }
  return 0;
}

/* ── Init ────────────────────────────────────────────────────────────────*/

static void buddy_init(BuddyAlloc *ba, size_t size) {
  /* Round up to power of two */
  int order = buddy_log2(size);
  if (order < BUDDY_MIN_ORDER) order = BUDDY_MIN_ORDER;
  if (order > BUDDY_MAX_ORDER) order = BUDDY_MAX_ORDER;

  ba->total_size = buddy_order_size(order);
  ba->max_order  = order;
  ba->memory     = (uint8_t *)malloc(ba->total_size);
  assert(ba->memory && "buddy_init: allocation failed");
  memset(ba->memory, 0, ba->total_size);

  /* Allocate block order tracking */
  size_t num_blocks = ba->total_size >> BUDDY_MIN_ORDER;
  ba->block_orders = (uint8_t *)calloc(num_blocks, 1);

  /* Initialize free lists */
  for (int i = 0; i < BUDDY_NUM_ORDERS; i++) {
    ba->free_lists[i] = NULL;
  }

  /* The entire memory is one big free block */
  int list_idx = order - BUDDY_MIN_ORDER;
  BuddyNode *node = (BuddyNode *)ba->memory;
  node->next = NULL;
  node->prev = NULL;
  ba->free_lists[list_idx] = node;
}

/* ── Alloc ───────────────────────────────────────────────────────────────*/

static void *buddy_alloc(BuddyAlloc *ba, size_t size) {
  if (size == 0) return NULL;

  /* Find the smallest order that fits */
  size_t needed = size;
  if (needed < (1UL << BUDDY_MIN_ORDER))
    needed = 1UL << BUDDY_MIN_ORDER;
  int order = buddy_log2(needed);

  /* Find a free block (may need to split larger blocks) */
  int found_order = -1;
  for (int o = order; o <= ba->max_order; o++) {
    int idx = o - BUDDY_MIN_ORDER;
    if (idx >= 0 && idx < BUDDY_NUM_ORDERS && ba->free_lists[idx]) {
      found_order = o;
      break;
    }
  }

  if (found_order < 0) return NULL;  /* out of memory */

  /* Pop from the found order's free list */
  int found_idx = found_order - BUDDY_MIN_ORDER;
  BuddyNode *block = ba->free_lists[found_idx];
  buddy_list_remove(&ba->free_lists[found_idx], block);

  /* Split down to the target order */
  while (found_order > order) {
    found_order--;
    int split_idx = found_order - BUDDY_MIN_ORDER;
    /* The second half becomes a free buddy */
    BuddyNode *buddy = (BuddyNode *)((uint8_t *)block +
                                       buddy_order_size(found_order));
    buddy_list_push(&ba->free_lists[split_idx], buddy);
  }

  /* Record the order for this allocation */
  ba->block_orders[buddy_block_index(ba, block)] = (uint8_t)order;
  memset(block, 0, buddy_order_size(order));
  return (void *)block;
}

/* ── Free ────────────────────────────────────────────────────────────────*/

static void buddy_free(BuddyAlloc *ba, void *ptr) {
  if (!ptr) return;

  int order = ba->block_orders[buddy_block_index(ba, ptr)];
  assert(order >= BUDDY_MIN_ORDER && order <= ba->max_order);

  /* Try to coalesce with buddy */
  while (order < ba->max_order) {
    size_t boff = buddy_offset(ba, ptr, order);
    if (boff >= ba->total_size) break;  /* buddy out of range */

    BuddyNode *buddy_node = (BuddyNode *)(ba->memory + boff);
    int list_idx = order - BUDDY_MIN_ORDER;

    /* Check if buddy is free at this order */
    if (!buddy_list_contains(ba->free_lists[list_idx], buddy_node)) break;

    /* Remove buddy from free list and merge */
    buddy_list_remove(&ba->free_lists[list_idx], buddy_node);

    /* The merged block starts at the lower address */
    if ((uint8_t *)buddy_node < (uint8_t *)ptr) {
      ptr = (void *)buddy_node;
    }
    order++;
  }

  /* Add the (possibly merged) block to the free list */
  ba->block_orders[buddy_block_index(ba, ptr)] = (uint8_t)order;
  int list_idx = order - BUDDY_MIN_ORDER;
  BuddyNode *node = (BuddyNode *)ptr;
  buddy_list_push(&ba->free_lists[list_idx], node);
}

/* ── Destroy ─────────────────────────────────────────────────────────────*/

static void buddy_destroy(BuddyAlloc *ba) {
  free(ba->block_orders);
  free(ba->memory);
  ba->memory = NULL;
  ba->block_orders = NULL;
}

/* ── Diagnostics ─────────────────────────────────────────────────────────*/

static void buddy_print_free_lists(const BuddyAlloc *ba) {
  printf("  Free lists:\n");
  for (int o = BUDDY_MIN_ORDER; o <= ba->max_order; o++) {
    int idx = o - BUDDY_MIN_ORDER;
    int count = 0;
    BuddyNode *n = ba->free_lists[idx];
    while (n) { count++; n = n->next; }
    if (count > 0) {
      printf("    Order %2d (%6zu bytes): %d free block%s\n",
             o, buddy_order_size(o), count, count > 1 ? "s" : "");
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * LESSON
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 22 — Buddy Allocator Overview\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. How the buddy system works ──────────────────────────────────*/
  print_section("How the Buddy System Works");
  printf("  The buddy allocator divides memory into power-of-two blocks.\n\n");
  printf("  Example: 1024 bytes, request 100 bytes (needs 128-byte block)\n\n");
  printf("  Step 1: Start with one 1024-byte block\n");
  printf("    [████████████████████████ 1024 ████████████████████████]\n\n");
  printf("  Step 2: Split 1024 → two 512s\n");
  printf("    [████████ 512 ████████][         512 (free)           ]\n\n");
  printf("  Step 3: Split 512 → two 256s\n");
  printf("    [████ 256 ████][ 256 (free) ][    512 (free)          ]\n\n");
  printf("  Step 4: Split 256 → two 128s\n");
  printf("    [128 USED][128 free][  256 free  ][     512 free      ]\n\n");
  printf("  On free: merge (coalesce) with buddy if buddy is also free.\n");

  /* ── 2. Basic buddy usage ───────────────────────────────────────────*/
  print_section("Basic Buddy Allocator Usage");
  {
    BuddyAlloc ba;
    buddy_init(&ba, 1024);

    printf("  Initialized: %zu bytes, max_order=%d\n\n",
           ba.total_size, ba.max_order);
    buddy_print_free_lists(&ba);

    printf("\n  Allocating 100 bytes:\n");
    void *a = buddy_alloc(&ba, 100);
    printf("    a = %p (got %zu-byte block, order %d)\n",
           a, buddy_order_size(ba.block_orders[buddy_block_index(&ba, a)]),
           ba.block_orders[buddy_block_index(&ba, a)]);
    buddy_print_free_lists(&ba);

    printf("\n  Allocating 200 bytes:\n");
    void *b = buddy_alloc(&ba, 200);
    printf("    b = %p (got %zu-byte block, order %d)\n",
           b, buddy_order_size(ba.block_orders[buddy_block_index(&ba, b)]),
           ba.block_orders[buddy_block_index(&ba, b)]);
    buddy_print_free_lists(&ba);

    printf("\n  Freeing a (100 bytes):\n");
    buddy_free(&ba, a);
    buddy_print_free_lists(&ba);

    printf("\n  Freeing b (200 bytes):\n");
    buddy_free(&ba, b);
    buddy_print_free_lists(&ba);
    printf("\n  After freeing both, the whole block coalesced back!\n");

    buddy_destroy(&ba);
  }

  /* ── 3. Splitting and coalescing in action ──────────────────────────*/
  print_section("Splitting and Coalescing Demo");
  {
    BuddyAlloc ba;
    buddy_init(&ba, 512);

    printf("  512 bytes total.  Allocating 4 x 64-byte blocks:\n\n");

    void *blks[4];
    for (int i = 0; i < 4; i++) {
      blks[i] = buddy_alloc(&ba, 64);
      printf("  alloc[%d] = %p  (offset=%zu)\n",
             i, blks[i], (uint8_t *)blks[i] - ba.memory);
    }
    printf("\n");
    buddy_print_free_lists(&ba);

    printf("\n  Freeing blocks 0 and 1 (should coalesce into 128-byte block):\n");
    buddy_free(&ba, blks[0]);
    buddy_free(&ba, blks[1]);
    buddy_print_free_lists(&ba);

    printf("\n  Freeing blocks 2 and 3 (should coalesce up to 512-byte block):\n");
    buddy_free(&ba, blks[2]);
    buddy_free(&ba, blks[3]);
    buddy_print_free_lists(&ba);

    buddy_destroy(&ba);
  }

  /* ── 4. Internal fragmentation ──────────────────────────────────────*/
  print_section("Internal Fragmentation (The Trade-off)");
  {
    BuddyAlloc ba;
    buddy_init(&ba, 4096);

    printf("  Power-of-two rounding means wasted space:\n\n");
    printf("  ┌──────────────┬──────────────┬──────────────┐\n");
    printf("  │ Requested    │ Allocated    │ Waste        │\n");
    printf("  ├──────────────┼──────────────┼──────────────┤\n");

    size_t requests[] = {33, 65, 100, 200, 500, 1000, 2000};
    for (int i = 0; i < 7; i++) {
      int order = buddy_log2(requests[i]);
      if (order < BUDDY_MIN_ORDER) order = BUDDY_MIN_ORDER;
      size_t actual = buddy_order_size(order);
      size_t waste = actual - requests[i];
      printf("  │ %10zu B │ %10zu B │ %8zu B (%2zu%%) │\n",
             requests[i], actual, waste, (waste * 100) / actual);
    }
    printf("  └──────────────┴──────────────┴──────────────┘\n");
    printf("\n  Average ~25%% internal fragmentation — the cost of\n");
    printf("  fast splitting and coalescing.\n");

    buddy_destroy(&ba);
  }

  /* ── 5. Comparison table ────────────────────────────────────────────*/
  print_section("Buddy vs Other Allocators");
  printf("  ┌───────────────────┬──────────────┬──────────────┬──────────────┐\n");
  printf("  │ Feature           │ Buddy        │ Free List    │ Slab/Pool    │\n");
  printf("  ├───────────────────┼──────────────┼──────────────┼──────────────┤\n");
  printf("  │ Alloc speed       │ O(log n)     │ O(n)         │ O(1)         │\n");
  printf("  │ Free speed        │ O(log n)     │ O(1)         │ O(1)         │\n");
  printf("  │ Fragmentation     │ Internal     │ External     │ None         │\n");
  printf("  │ Variable sizes    │ Yes (po2)    │ Yes (any)    │ No (fixed)   │\n");
  printf("  │ Coalescing        │ Yes          │ Manual       │ N/A          │\n");
  printf("  │ Used in           │ Linux kernel │ General      │ Kernel obj   │\n");
  printf("  └───────────────────┴──────────────┴──────────────┴──────────────┘\n");
  printf("\n  The Linux kernel uses buddy allocation for page-level management,\n");
  printf("  with slab allocators built on top for smaller fixed-size objects.\n");

  /* ── 6. Benchmark ───────────────────────────────────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 2000000;

    BENCH_SUITE(suite, "buddy_alloc+free vs malloc+free (128 bytes)") {
      /* Buddy allocator */
      BuddyAlloc ba;
      buddy_init(&ba, 1024 * 1024);  /* 1 MB */
      BENCH_CASE(suite, "buddy alloc+free", i, N) {
        void *p = buddy_alloc(&ba, 128);
        if (p) {
          *(volatile char *)p = 0;
          buddy_free(&ba, p);
        }
      }
      BENCH_CASE_END(suite, N);
      buddy_destroy(&ba);

      /* malloc+free */
      BENCH_CASE(suite, "malloc+free", i, N) {
        void *p = malloc(128);
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);
    }
  }
#else
  bench_skip_notice("lesson_22");
#endif

  printf("\n");
  return 0;
}
