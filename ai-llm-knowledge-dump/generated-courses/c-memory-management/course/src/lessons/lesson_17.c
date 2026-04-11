/* lesson_17.c — Stretchy Buffer Dynamic Array
 * BUILD_DEPS: common/bench.c
 *
 * Demonstrates a type-generic dynamic array using the stb-style stretchy
 * buffer technique, backed by an arena.  Shows growth pattern and benchmarks
 * against naive realloc-every-push.
 *
 * Run:   ./build-dev.sh --lesson=17 -r
 * Bench: ./build-dev.sh --lesson=17 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"
#include "lib/stretchy_buf.h"

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 17 — Stretchy Buffer Dynamic Array\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Basic usage ──────────────────────────────────────────────────*/
  print_section("Basic Stretchy Buffer Usage");
  {
    Arena arena = {0};
    arena.min_block_size = 4096;

    int *nums = NULL;  /* starts empty */
    printf("  Initial: len=%zu  cap=%zu\n", buf_len(nums), buf_cap(nums));

    /* Push some values */
    for (int i = 1; i <= 10; i++) {
      buf_push(&arena, nums, i * 10);
    }

    printf("  After pushing 10 values: len=%zu  cap=%zu\n",
           buf_len(nums), buf_cap(nums));
    printf("  Contents: ");
    for (size_t i = 0; i < buf_len(nums); i++) {
      printf("%d ", nums[i]);
    }
    printf("\n");
    printf("  Last element: %d\n", buf_last(nums));

    /* Pop a few */
    buf_pop(nums);
    buf_pop(nums);
    printf("  After 2 pops: len=%zu  last=%d\n",
           buf_len(nums), buf_last(nums));

    buf_free(nums);
    printf("  After buf_free: buf is %s\n", nums ? "non-NULL" : "NULL");

    arena_free(&arena);
  }

  /* ── 2. Watch the growth pattern ─────────────────────────────────────*/
  print_section("Growth Pattern (1000 pushes)");
  {
    Arena arena = {0};
    arena.min_block_size = 64 * 1024;

    int *buf = NULL;
    size_t prev_cap = 0;
    int growth_count = 0;

    printf("  ┌─────────┬──────────┬──────────┬──────────────────────┐\n");
    printf("  │ Push #   │ Length   │ Capacity │ Event                │\n");
    printf("  ├─────────┼──────────┼──────────┼──────────────────────┤\n");

    for (int i = 0; i < 1000; i++) {
      buf_push(&arena, buf, i);
      if (buf_cap(buf) != prev_cap) {
        printf("  │ %7d │ %8zu │ %8zu │ GROW (%zu → %zu)%*s│\n",
               i + 1, buf_len(buf), buf_cap(buf),
               prev_cap, buf_cap(buf),
               (int)(14 - snprintf(NULL, 0, "%zu → %zu", prev_cap, buf_cap(buf))),
               "");
        prev_cap = buf_cap(buf);
        growth_count++;
      }
    }
    printf("  └─────────┴──────────┴──────────┴──────────────────────┘\n");
    printf("\n  1000 pushes required only %d growths (doubling strategy).\n",
           growth_count);
    printf("  Arena used: %zu bytes\n", arena_total_used(&arena));

    arena_free(&arena);
  }

  /* ── 3. Type genericity ──────────────────────────────────────────────*/
  print_section("Type Genericity (works with any type)");
  {
    Arena arena = {0};
    arena.min_block_size = 4096;

    /* Doubles */
    double *dbls = NULL;
    buf_push(&arena, dbls, 3.14);
    buf_push(&arena, dbls, 2.718);
    buf_push(&arena, dbls, 1.414);
    printf("  double buf: ");
    for (size_t i = 0; i < buf_len(dbls); i++)
      printf("%.3f ", dbls[i]);
    printf(" (len=%zu)\n", buf_len(dbls));

    /* Structs */
    typedef struct { float x, y; } Vec2;
    Vec2 *points = NULL;
    buf_push(&arena, points, ((Vec2){1.0f, 2.0f}));
    buf_push(&arena, points, ((Vec2){3.0f, 4.0f}));
    buf_push(&arena, points, ((Vec2){5.0f, 6.0f}));
    printf("  Vec2 buf:   ");
    for (size_t i = 0; i < buf_len(points); i++)
      printf("(%.0f,%.0f) ", points[i].x, points[i].y);
    printf(" (len=%zu)\n", buf_len(points));

    arena_free(&arena);
  }

  /* ── 4. The stb trick explained ──────────────────────────────────────*/
  print_section("How It Works: Header Before Data");
  {
    Arena arena = {0};
    arena.min_block_size = 4096;

    int *buf = NULL;
    buf_push(&arena, buf, 42);
    buf_push(&arena, buf, 99);

    BufHeader *hdr = buf__hdr(buf);
    printf("  User pointer (buf):    %p\n", (void *)buf);
    printf("  Hidden header (hdr):   %p\n", (void *)hdr);
    printf("  hdr + 1 == buf?        %s\n",
           ((void *)(hdr + 1) == (void *)buf) ? "YES" : "no");
    printf("  Header offset:         %zu bytes before buf\n",
           (size_t)((uint8_t *)buf - (uint8_t *)hdr));
    printf("\n  Memory layout:\n");
    printf("    [BufHeader: len=%zu cap=%zu] [%d] [%d] ...\n",
           hdr->len, hdr->cap, buf[0], buf[1]);

    arena_free(&arena);
  }

  /* ── 5. Benchmark ────────────────────────────────────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 1000000;

    BENCH_SUITE(suite, "Stretchy buf_push vs realloc-every-push (int)") {
      /* Stretchy buffer with arena */
      Arena arena = {0};
      arena.min_block_size = 16 * 1024 * 1024;
      int *sbuf = NULL;
      BENCH_CASE(suite, "buf_push (arena)", i, N) {
        buf_push(&arena, sbuf, (int)i);
      }
      BENCH_CASE_END(suite, N);
      arena_free(&arena);

      /* Naive realloc every push */
      int *rbuf = NULL;
      size_t rlen = 0;
      BENCH_CASE(suite, "realloc every push", i, N) {
        rlen++;
        rbuf = (int *)realloc(rbuf, rlen * sizeof(int));
        rbuf[rlen - 1] = (int)i;
      }
      BENCH_CASE_END(suite, N);
      free(rbuf);

      /* realloc with doubling (standard approach) */
      int *dbuf = NULL;
      size_t dlen = 0, dcap = 0;
      BENCH_CASE(suite, "realloc (doubling)", i, N) {
        if (dlen >= dcap) {
          dcap = dcap ? dcap * 2 : 16;
          dbuf = (int *)realloc(dbuf, dcap * sizeof(int));
        }
        dbuf[dlen++] = (int)i;
      }
      BENCH_CASE_END(suite, N);
      free(dbuf);
    }
  }
#else
  bench_skip_notice("lesson_17");
#endif

  printf("\n");
  return 0;
}
