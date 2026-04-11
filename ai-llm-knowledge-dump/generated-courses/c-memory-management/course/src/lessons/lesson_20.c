/* lesson_20.c — Stack Allocator LIFO
 * BUILD_DEPS: common/bench.c, lib/stack_alloc.c
 *
 * Demonstrates a LIFO stack allocator with per-allocation headers.
 * Push several allocations, pop in reverse order.  Shows header
 * overhead vs a plain arena.
 *
 * Run:   ./build-dev.sh --lesson=20 -r
 * Bench: ./build-dev.sh --lesson=20 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"
#include "lib/stack_alloc.h"

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 20 — Stack Allocator LIFO\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Basic push/pop ───────────────────────────────────────────────*/
  print_section("Basic Push and Pop");
  {
    uint8_t buffer[4096];
    StackAlloc sa;
    stack_alloc_init(&sa, buffer, sizeof(buffer));

    printf("  Stack allocator: %zu bytes of backing memory\n\n", sa.size);

    /* Push three allocations */
    int *a = (int *)stack_alloc_push(&sa, sizeof(int), _Alignof(int));
    *a = 111;
    printf("  push(int):    a=%p → %d   (offset=%zu, count=%zu)\n",
           (void *)a, *a, sa.offset, sa.alloc_count);

    double *b = (double *)stack_alloc_push(&sa, sizeof(double), _Alignof(double));
    *b = 3.14159;
    printf("  push(double): b=%p → %.5f (offset=%zu, count=%zu)\n",
           (void *)b, *b, sa.offset, sa.alloc_count);

    char *c = (char *)stack_alloc_push(&sa, 32, 1);
    snprintf(c, 32, "hello stack!");
    printf("  push(char*):  c=%p → \"%s\"  (offset=%zu, count=%zu)\n",
           (void *)c, c, sa.offset, sa.alloc_count);

    /* Pop in reverse order */
    printf("\n  Popping in LIFO order:\n");

    stack_alloc_pop(&sa);
    printf("  pop(): removed char*   (offset=%zu, count=%zu)\n",
           sa.offset, sa.alloc_count);

    stack_alloc_pop(&sa);
    printf("  pop(): removed double  (offset=%zu, count=%zu)\n",
           sa.offset, sa.alloc_count);

    stack_alloc_pop(&sa);
    printf("  pop(): removed int     (offset=%zu, count=%zu)\n",
           sa.offset, sa.alloc_count);

    printf("\n  Stack is empty.  offset=%zu (back to start)\n", sa.offset);
  }

  /* ── 2. Header overhead analysis ─────────────────────────────────────*/
  print_section("Header Overhead: Stack vs Arena");
  {
    uint8_t buffer[4096];
    StackAlloc sa;
    stack_alloc_init(&sa, buffer, sizeof(buffer));

    Arena arena = {0};
    arena.min_block_size = 4096;

    printf("  StackHeader size: %zu bytes per allocation\n\n",
           sizeof(StackHeader));

    /* Push 10 ints on both */
    for (int i = 0; i < 10; i++) {
      int *sp = (int *)stack_alloc_push(&sa, sizeof(int), _Alignof(int));
      *sp = i;
      int *ap = ARENA_PUSH_STRUCT(&arena, int);
      *ap = i;
    }

    printf("  10 ints (%zu bytes of data):\n", 10 * sizeof(int));
    printf("    Stack allocator used: %zu bytes (%.1f%% overhead)\n",
           sa.offset,
           ((double)sa.offset / (10.0 * sizeof(int)) - 1.0) * 100.0);
    printf("    Arena used:           %zu bytes (%.1f%% overhead)\n",
           arena_total_used(&arena),
           ((double)arena_total_used(&arena) / (10.0 * sizeof(int)) - 1.0) * 100.0);

    printf("\n  The stack header is the cost of supporting individual pop.\n");
    printf("  If you don't need pop, use a plain arena (zero overhead).\n");

    arena_free(&arena);
  }

  /* ── 3. Practical use: expression evaluation ─────────────────────────*/
  print_section("Practical Use: Temporary Computation Buffers");
  {
    uint8_t buffer[8192];
    StackAlloc sa;
    stack_alloc_init(&sa, buffer, sizeof(buffer));

    printf("  Simulating nested computation with temporary buffers:\n\n");

    /* Outer computation needs a buffer */
    float *outer = (float *)stack_alloc_push(&sa, 100 * sizeof(float),
                                              _Alignof(float));
    for (int i = 0; i < 100; i++) outer[i] = (float)i * 0.5f;
    printf("    Level 0: allocated 100 floats (offset=%zu)\n", sa.offset);

    /* Inner computation needs its own buffer */
    float *inner = (float *)stack_alloc_push(&sa, 50 * sizeof(float),
                                              _Alignof(float));
    for (int i = 0; i < 50; i++) inner[i] = outer[i] * 2.0f;
    printf("    Level 1: allocated 50 floats  (offset=%zu)\n", sa.offset);

    /* Deepest level */
    float *deep = (float *)stack_alloc_push(&sa, 25 * sizeof(float),
                                             _Alignof(float));
    for (int i = 0; i < 25; i++) deep[i] = inner[i] + 1.0f;
    printf("    Level 2: allocated 25 floats  (offset=%zu)\n", sa.offset);

    /* Unwind — pop in reverse */
    float result = deep[0] + deep[24];
    stack_alloc_pop(&sa);
    printf("    Pop level 2 (offset=%zu)  result=%.1f\n", sa.offset, result);

    result += inner[0] + inner[49];
    stack_alloc_pop(&sa);
    printf("    Pop level 1 (offset=%zu)\n", sa.offset);

    result += outer[0] + outer[99];
    stack_alloc_pop(&sa);
    printf("    Pop level 0 (offset=%zu)\n", sa.offset);

    printf("\n  Final result: %.1f\n", result);
    printf("  All memory reclaimed without any waste.\n");
  }

  /* ── 4. Stack vs Arena comparison table ──────────────────────────────*/
  print_section("Stack Allocator vs Arena");
  printf("  ┌─────────────────────┬──────────────────┬──────────────────┐\n");
  printf("  │ Feature             │ Stack Allocator   │ Arena            │\n");
  printf("  ├─────────────────────┼──────────────────┼──────────────────┤\n");
  printf("  │ Alloc               │ O(1) bump+header │ O(1) bump        │\n");
  printf("  │ Free individual     │ O(1) pop (LIFO)  │ No               │\n");
  printf("  │ Free all            │ O(1) reset       │ O(1) reset       │\n");
  printf("  │ Per-alloc overhead  │ %2zu bytes header  │ 0 bytes          │\n",
         sizeof(StackHeader));
  printf("  │ Free order          │ LIFO only        │ N/A              │\n");
  printf("  │ Use case            │ Scratch buffers  │ Lifetime groups  │\n");
  printf("  └─────────────────────┴──────────────────┴──────────────────┘\n");

  /* ── 5. Benchmark ────────────────────────────────────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 5000000;

    BENCH_SUITE(suite, "stack_push+pop vs arena_push (64 bytes)") {
      /* Stack push+pop */
      uint8_t stack_buf[1024];
      StackAlloc sa;
      stack_alloc_init(&sa, stack_buf, sizeof(stack_buf));
      BENCH_CASE(suite, "stack push+pop", i, N) {
        void *p = stack_alloc_push(&sa, 64, 8);
        *(volatile char *)p = 0;
        stack_alloc_pop(&sa);
      }
      BENCH_CASE_END(suite, N);

      /* Arena push (reset periodically) */
      Arena arena = {0};
      arena.min_block_size = 1024 * 1024;
      BENCH_CASE(suite, "arena push", i, N) {
        void *p = arena_push_size(&arena, 64, 8);
        *(volatile char *)p = 0;
        if (i % 10000 == 9999) arena_reset(&arena);
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
  bench_skip_notice("lesson_20");
#endif

  printf("\n");
  return 0;
}
