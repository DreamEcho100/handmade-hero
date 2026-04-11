/* lesson_10.c — Arena Basics: Push and Reset
 * BUILD_DEPS: common/bench.c
 *
 * Implements a minimal fixed-size arena and demonstrates O(1) allocation.
 * Benchmarks arena_push vs malloc+free.
 *
 * Run:   ./build-dev.sh --lesson=10 -r
 * Bench: ./build-dev.sh --lesson=10 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 10 — Arena Basics: Push and Reset\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Basic arena usage ───────────────────────────────────────────
   *
   * Ryan Fleury: "The basic principle is that the arena organizes
   * lifetimes together.  Push and push and push.  Then reset."
   */
  print_section("Basic Arena Usage");
  {
    Arena arena = {0};  /* ZII: zero-initialized is a valid empty arena */
    arena.min_block_size = 4096;  /* 4 KB for demo purposes */

    printf("  Arena created (ZII: zero-initialized).\n");
    printf("  min_block_size = %zu bytes\n\n", arena.min_block_size);

    /* Push some allocations */
    int *a = ARENA_PUSH_STRUCT(&arena, int);
    *a = 42;

    int *b = ARENA_PUSH_STRUCT(&arena, int);
    *b = 99;

    double *c = ARENA_PUSH_STRUCT(&arena, double);
    *c = 3.14159;

    printf("  Pushed 3 values:\n");
    printf("    a = %p → %d\n", (void *)a, *a);
    printf("    b = %p → %d\n", (void *)b, *b);
    printf("    c = %p → %.5f\n", (void *)c, *c);
    printf("\n  Allocated: %zu bytes used of %zu total\n",
           arena_total_used(&arena), arena_total_allocated(&arena));

    /* Reset — all allocations gone, memory reused */
    arena_reset(&arena);
    printf("\n  arena_reset() — all allocations freed in O(1)!\n");
    printf("  Used after reset: %zu bytes\n", arena_total_used(&arena));

    arena_free(&arena);
  }

  /* ── 2. Array allocation ────────────────────────────────────────────*/
  print_section("Array Allocation");
  {
    Arena arena = {0};
    arena.min_block_size = 64 * 1024;  /* 64 KB */

    /* Allocate an array of 1000 floats */
    float *positions = ARENA_PUSH_ARRAY(&arena, 1000, float);
    for (int i = 0; i < 1000; i++) positions[i] = (float)i * 0.1f;

    /* Allocate another array */
    int *indices = ARENA_PUSH_ARRAY(&arena, 500, int);
    for (int i = 0; i < 500; i++) indices[i] = i * 2;

    printf("  positions[0..2]: %.1f, %.1f, %.1f\n",
           positions[0], positions[1], positions[2]);
    printf("  indices[0..2]: %d, %d, %d\n",
           indices[0], indices[1], indices[2]);
    printf("  Total used: %zu bytes\n", arena_total_used(&arena));

    /* Reset frees both arrays at once */
    arena_reset(&arena);
    printf("  After reset: %zu bytes used\n", arena_total_used(&arena));

    arena_free(&arena);
  }

  /* ── 3. Why arenas beat malloc for bulk allocation ──────────────────
   *
   * Ryan Fleury: "When I learned about these, all of the traditional
   * concerns about memory management in C went away."
   *
   * malloc + free:
   *   - Each call searches free lists, updates metadata
   *   - Thread contention on the global allocator
   *   - Must remember to free EVERY allocation
   *
   * Arena push:
   *   - Bump a pointer (one addition)
   *   - No metadata per allocation
   *   - Free everything at once with reset
   */
  print_section("Arena vs malloc Conceptual Comparison");
  printf("  ┌───────────────────┬──────────────────┬──────────────────┐\n");
  printf("  │ Operation         │ malloc/free      │ Arena            │\n");
  printf("  ├───────────────────┼──────────────────┼──────────────────┤\n");
  printf("  │ Allocate          │ Search free list │ Bump pointer     │\n");
  printf("  │ Free one          │ Update metadata  │ N/A (or noop)    │\n");
  printf("  │ Free all          │ N free() calls   │ One reset()      │\n");
  printf("  │ Per-alloc overhead│ 16-32 bytes      │ 0 bytes          │\n");
  printf("  │ Thread safety     │ Locks/atomics    │ One arena/thread │\n");
  printf("  │ Fragmentation     │ Yes              │ No               │\n");
  printf("  └───────────────────┴──────────────────┴──────────────────┘\n");

  /* ── 4. Per-frame pattern (game development) ────────────────────────*/
  print_section("Per-Frame Arena Pattern");
  {
    Arena frame_arena = {0};
    frame_arena.min_block_size = 64 * 1024;

    printf("  Simulating 5 game frames:\n\n");
    for (int frame = 0; frame < 5; frame++) {
      arena_reset(&frame_arena);

      /* Each frame allocates temporary data */
      int num_entities = 100 + frame * 50;
      float *entity_distances = ARENA_PUSH_ARRAY(
        &frame_arena, num_entities, float);
      for (int i = 0; i < num_entities; i++)
        entity_distances[i] = (float)(i * frame);

      char *debug_str = (char *)arena_push_size(
        &frame_arena, 128, 1);
      snprintf(debug_str, 128, "Frame %d: %d entities", frame, num_entities);

      printf("    Frame %d: %s  (arena used: %zu bytes)\n",
             frame, debug_str, arena_total_used(&frame_arena));
    }
    /* Everything from all frames is gone after reset */
    printf("\n  No malloc/free calls.  No leaks.  No use-after-free.\n");
    printf("  Just push → use → reset.  That's it.\n");

    arena_free(&frame_arena);
  }

  /* ── 5. Benchmark ───────────────────────────────────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 5000000;

    BENCH_SUITE(suite, "Arena push vs malloc+free (64 bytes each)") {
      /* malloc+free */
      BENCH_CASE(suite, "malloc+free", i, N) {
        void *p = malloc(64);
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);

      /* Arena push (reset every 10000 allocs to avoid OOM) */
      Arena arena = {0};
      arena.min_block_size = 1024 * 1024;
      BENCH_CASE(suite, "arena_push", i, N) {
        void *p = arena_push_size(&arena, 64, 8);
        *(volatile char *)p = 0;
        if (i % 10000 == 9999) arena_reset(&arena);
      }
      BENCH_CASE_END(suite, N);
      arena_free(&arena);

      /* calloc+free for comparison */
      BENCH_CASE(suite, "calloc+free", i, N) {
        void *p = calloc(1, 64);
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);
    }
  }
#else
  bench_skip_notice("lesson_10");
#endif

  printf("\n");
  return 0;
}
