/* lesson_19.c — Free List on Arena
 * BUILD_DEPS: common/bench.c, lib/freelist.c
 *
 * Demonstrates a free list overlay on an arena for variable-size
 * allocations that need individual deallocation.  Builds a linked list
 * with add/remove nodes.
 *
 * Run:   ./build-dev.sh --lesson=19 -r
 * Bench: ./build-dev.sh --lesson=19 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"
#include "lib/freelist.h"

/* ── A simple linked list node ─────────────────────────────────────────────*/
typedef struct ListNode {
  int              value;
  struct ListNode *next;
} ListNode;

/* Helper: print a linked list */
static void print_list(const char *label, const ListNode *head) {
  printf("  %s: ", label);
  const ListNode *n = head;
  while (n) {
    printf("%d", n->value);
    if (n->next) printf(" → ");
    n = n->next;
  }
  if (!head) printf("(empty)");
  printf("\n");
}

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 19 — Free List on Arena\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Basic free list usage ────────────────────────────────────────*/
  print_section("Basic Free List Usage");
  {
    Arena arena = {0};
    arena.min_block_size = 4096;
    FreeList fl;
    freelist_init(&fl, &arena);

    printf("  Allocating 3 blocks of different sizes:\n");
    void *a = freelist_alloc(&fl, 32, 8);
    void *b = freelist_alloc(&fl, 64, 8);
    void *c = freelist_alloc(&fl, 128, 8);
    printf("    a (32B)  = %p\n", a);
    printf("    b (64B)  = %p\n", b);
    printf("    c (128B) = %p\n", c);

    printf("\n  Freeing b (64 bytes)...\n");
    freelist_free(&fl, b, 64);

    printf("  Allocating 48 bytes (should reuse b's 64-byte slot):\n");
    void *d = freelist_alloc(&fl, 48, 8);
    printf("    d (48B)  = %p  (same as b? %s)\n",
           d, (d == b) ? "YES — reused!" : "no");

    arena_free(&arena);
  }

  /* ── 2. Linked list with add/remove ──────────────────────────────────*/
  print_section("Linked List: Add and Remove Nodes");
  {
    Arena arena = {0};
    arena.min_block_size = 4096;
    FreeList fl;
    freelist_init(&fl, &arena);

    ListNode *head = NULL;

    /* Build list: 10 → 20 → 30 → 40 → 50 */
    for (int v = 50; v >= 10; v -= 10) {
      ListNode *n = (ListNode *)freelist_alloc(&fl, sizeof(ListNode),
                                                _Alignof(ListNode));
      n->value = v;
      n->next = head;
      head = n;
    }
    print_list("Built", head);

    /* Remove node with value 30 */
    printf("\n  Removing node with value 30:\n");
    ListNode *prev = NULL;
    ListNode *cur = head;
    while (cur) {
      if (cur->value == 30) {
        if (prev) prev->next = cur->next;
        else head = cur->next;
        freelist_free(&fl, cur, sizeof(ListNode));
        break;
      }
      prev = cur;
      cur = cur->next;
    }
    print_list("After remove 30", head);

    /* Remove head (10) */
    printf("  Removing head (10):\n");
    ListNode *old_head = head;
    head = head->next;
    freelist_free(&fl, old_head, sizeof(ListNode));
    print_list("After remove 10", head);

    /* Add 15 — should reuse freed memory */
    printf("\n  Adding 15 (should reuse freed slot):\n");
    ListNode *n15 = (ListNode *)freelist_alloc(&fl, sizeof(ListNode),
                                                _Alignof(ListNode));
    n15->value = 15;
    n15->next = head;
    head = n15;
    print_list("After add 15", head);

    /* Show arena usage */
    printf("\n  Arena total used: %zu bytes\n", arena_total_used(&arena));
    printf("  Even though we freed/reused nodes, the arena memory is\n");
    printf("  compact.  When we're done, arena_free frees everything.\n");

    arena_free(&arena);
  }

  /* ── 3. Mixed-size allocation pattern ────────────────────────────────*/
  print_section("Mixed-Size Alloc/Free Pattern");
  {
    Arena arena = {0};
    arena.min_block_size = 64 * 1024;
    FreeList fl;
    freelist_init(&fl, &arena);

    /* Allocate various sizes, free some, reallocate */
    void *ptrs[20];
    size_t sizes[] = {16, 32, 64, 128, 256, 16, 32, 64, 128, 256,
                      48, 96, 200, 24, 72, 160, 48, 96, 200, 24};

    printf("  Allocating 20 blocks of varying sizes...\n");
    for (int i = 0; i < 20; i++) {
      ptrs[i] = freelist_alloc(&fl, sizes[i], 8);
    }

    printf("  Freeing even-indexed blocks (0, 2, 4, ...)...\n");
    for (int i = 0; i < 20; i += 2) {
      freelist_free(&fl, ptrs[i], sizes[i]);
      ptrs[i] = NULL;
    }

    printf("  Reallocating 10 blocks of size 64...\n");
    int reused = 0;
    for (int i = 0; i < 20; i += 2) {
      void *old = ptrs[i]; /* NULL */
      ptrs[i] = freelist_alloc(&fl, 64, 8);
      (void)old;
      /* Check if we got an address within the original allocation range */
      if (ptrs[i] >= ptrs[1] && ptrs[i] <= ptrs[19]) reused++;
    }
    printf("  Addresses reused from free list: ~%d/10\n", reused);

    arena_free(&arena);
  }

  /* ── 4. When to use a free list ──────────────────────────────────────*/
  print_section("When to Use a Free List on Arena");
  printf("  USE a free list when:\n");
  printf("    - You need individual deallocation (not just bulk reset)\n");
  printf("    - Objects have varying sizes\n");
  printf("    - Objects have unpredictable lifetimes\n");
  printf("    - Example: nodes in a graph, entities in a scene\n\n");
  printf("  DON'T use a free list when:\n");
  printf("    - All objects are the same size → use a Pool instead\n");
  printf("    - All objects die together → use a plain Arena\n");
  printf("    - You need O(1) guaranteed → free list search is O(n)\n");

  /* ── 5. Benchmark ────────────────────────────────────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 2000000;

    BENCH_SUITE(suite, "freelist_alloc+free vs malloc+free (64 bytes)") {
      /* Free list on arena */
      Arena arena = {0};
      arena.min_block_size = 16 * 1024 * 1024;
      FreeList fl;
      freelist_init(&fl, &arena);

      BENCH_CASE(suite, "freelist alloc+free", i, N) {
        void *p = freelist_alloc(&fl, 64, 8);
        *(volatile char *)p = 0;
        freelist_free(&fl, p, 64);
      }
      BENCH_CASE_END(suite, N);
      arena_free(&arena);

      /* malloc+free */
      BENCH_CASE(suite, "malloc+free", i, N) {
        void *p = malloc(64);
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);
    }
  }
#else
  bench_skip_notice("lesson_19");
#endif

  printf("\n");
  return 0;
}
