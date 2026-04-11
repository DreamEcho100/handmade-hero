#define _POSIX_C_SOURCE 200809L
/* lesson_13.c — Arena Growth: Virtual Memory
 * BUILD_DEPS: common/bench.c
 *
 * Implements an mmap-backed arena: reserve a large address range,
 * commit pages incrementally.  Guard pages for overflow detection.
 *
 * Run:   ./build-dev.sh --lesson=13 -r
 * Bench: ./build-dev.sh --lesson=13 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 13 — Arena Growth: Virtual Memory (mmap)\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  long page_size = sysconf(_SC_PAGESIZE);
  printf("\n  Page size: %ld bytes\n", page_size);

  /* ── 1. mmap-backed arena ───────────────────────────────────────────
   *
   * Ryan Fleury: "I'm going to reserve 256 GB of address space but
   * that's not a physical commit of memory... when I run out of the
   * space I've committed, I call back to the OS to commit more."
   *
   * Our implementation:
   *   1. mmap a large region with PROT_NONE (just reserves addresses)
   *   2. mprotect the initial portion to PROT_READ|PROT_WRITE
   *   3. Guard pages on both ends catch overflows
   */
  print_section("mmap-Backed Arena");
  {
    size_t reserve_size = 16 * 1024 * 1024;  /* 16 MB reservation */
    Arena arena = {0};
    void *mem = arena_init_mmap(&arena, reserve_size);
    if (!mem) {
      printf("  arena_init_mmap failed!\n");
      return 1;
    }

    printf("  Reserved:  %zu MB virtual address space\n",
           reserve_size / (1024 * 1024));
    printf("  Backing:   %p (mmap base, includes guard pages)\n",
           arena.backing_base);
    printf("  Usable:    %p\n", mem);
    printf("  Guard pages on both sides.\n\n");

    /* Push allocations — all within the single contiguous block */
    int *values[1000];
    for (int i = 0; i < 1000; i++) {
      values[i] = ARENA_PUSH_STRUCT(&arena, int);
      *values[i] = i * i;
    }
    printf("  Pushed 1000 ints.\n");
    printf("  values[0]   = %d (at %p)\n", *values[0],   (void *)values[0]);
    printf("  values[999] = %d (at %p)\n", *values[999], (void *)values[999]);
    printf("  All in one contiguous block (no chaining needed).\n");
    printf("  Used: %zu bytes of %zu available\n",
           arena_total_used(&arena), arena_total_allocated(&arena));

    /* The key advantage: contiguous memory, no copies, guard pages */
    printf("\n  Advantages of mmap-backed arena:\n");
    printf("    - Single contiguous block (better cache locality)\n");
    printf("    - No copies on growth (just commit more pages)\n");
    printf("    - Guard pages catch overflows immediately\n");
    printf("    - Physical memory only used for committed pages\n");

    arena_free(&arena);
    printf("\n  arena_free — entire region released.\n");
  }

  /* ── 2. Compare: malloc-backed vs mmap-backed ───────────────────────*/
  print_section("malloc-Backed vs mmap-Backed Arena");
  printf("  ┌────────────────────┬──────────────────┬──────────────────┐\n");
  printf("  │ Feature            │ malloc-backed    │ mmap-backed      │\n");
  printf("  ├────────────────────┼──────────────────┼──────────────────┤\n");
  printf("  │ Growth             │ Chained blocks   │ Commit more pages│\n");
  printf("  │ Contiguous memory  │ No (per-block)   │ Yes              │\n");
  printf("  │ Guard pages        │ No               │ Yes              │\n");
  printf("  │ Min granularity    │ Any size         │ Page size (4KB)  │\n");
  printf("  │ Portability        │ Everywhere       │ POSIX/Windows    │\n");
  printf("  │ Typical use        │ Libraries, small │ Application-wide │\n");
  printf("  └────────────────────┴──────────────────┴──────────────────┘\n");

  /* ── 3. Benchmark ───────────────────────────────────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 5000000;

    BENCH_SUITE(suite, "mmap arena vs malloc arena vs raw malloc") {
      /* mmap-backed arena */
      {
        size_t sz = 256 * 1024 * 1024;
        Arena arena = {0};
        void *mem = arena_init_mmap(&arena, sz);
        (void)mem;

        BENCH_CASE(suite, "mmap arena push", i, N) {
          void *p = arena_push_size(&arena, 64, 8);
          *(volatile char *)p = 0;
          if (i % 100000 == 99999) arena_reset(&arena);
        }
        BENCH_CASE_END(suite, N);
        arena_free(&arena);
      }

      /* malloc-backed arena (chained blocks) */
      {
        Arena arena = {0};
        arena.min_block_size = 1024 * 1024;
        BENCH_CASE(suite, "malloc arena push", i, N) {
          void *p = arena_push_size(&arena, 64, 8);
          *(volatile char *)p = 0;
          if (i % 100000 == 99999) arena_reset(&arena);
        }
        BENCH_CASE_END(suite, N);
        arena_free(&arena);
      }

      /* Raw malloc+free */
      BENCH_CASE(suite, "malloc+free", i, N) {
        void *p = malloc(64);
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);
    }
  }
#else
  bench_skip_notice("lesson_13");
#endif

  printf("\n");
  return 0;
}
