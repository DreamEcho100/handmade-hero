/* lesson_26.c — Cache Locality and Access Patterns
 * BUILD_DEPS: common/bench.c
 *
 * Demonstrates how memory access patterns affect performance:
 *   1. Row-major vs column-major matrix traversal
 *   2. Sequential vs random array access
 *   3. Array size sweep showing L1/L2/L3 cache effects
 *
 * Run:   ./build-dev.sh --lesson=26 -r
 * Bench: ./build-dev.sh --lesson=26 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "common/bench.h"

/* ── Simple LCG for deterministic random ──────────────────────────────────*/
static uint32_t lcg_state = 12345;
__attribute__((unused))
static uint32_t lcg_next(void) {
  lcg_state = lcg_state * 1103515245u + 12345u;
  return lcg_state;
}

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 26 — Cache Locality and Access Patterns\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Row-major vs column-major ────────────────────────────────────── */
  print_section("Row-Major vs Column-Major Traversal");
  {
    printf("  A 2D array in C is stored in row-major order:\n");
    printf("    arr[0][0], arr[0][1], ..., arr[0][N-1],\n");
    printf("    arr[1][0], arr[1][1], ..., arr[1][N-1], ...\n\n");
    printf("  Row-major traversal (arr[i][j], j varies fast):\n");
    printf("    Accesses consecutive memory — cache-friendly.\n\n");
    printf("  Column-major traversal (arr[j][i], i varies fast):\n");
    printf("    Jumps by N*sizeof(int) each access — cache-hostile.\n\n");

    #define MAT_N 1024
    int (*mat)[MAT_N] = malloc(MAT_N * MAT_N * sizeof(int));
    if (!mat) { fprintf(stderr, "  OOM\n"); return 1; }

    /* Initialize */
    for (int i = 0; i < MAT_N; i++)
      for (int j = 0; j < MAT_N; j++)
        mat[i][j] = i + j;

    printf("  Matrix size: %d x %d (%zu KB)\n",
           MAT_N, MAT_N, (size_t)MAT_N * MAT_N * sizeof(int) / 1024);
    printf("  L1 cache is typically 32-64 KB — this matrix does NOT fit.\n");
    printf("  So access pattern matters enormously.\n");

    free(mat);
  }

  /* ── 2. Sequential vs random access ──────────────────────────────────── */
  print_section("Sequential vs Random Access");
  {
    printf("  Sequential: arr[0], arr[1], arr[2], ...\n");
    printf("    Hardware prefetcher detects the pattern and loads ahead.\n\n");
    printf("  Random: arr[rand()], arr[rand()], ...\n");
    printf("    Every access is a potential cache miss.\n");
    printf("    Prefetcher can't help.\n\n");
    printf("  For large arrays (> L3), random access can be 10-100x slower.\n");
  }

  /* ── 3. Cache size effects ───────────────────────────────────────────── */
  print_section("Cache Hierarchy");
  {
    printf("  Typical x86-64 cache hierarchy:\n\n");
    printf("  ┌──────────┬───────────┬──────────────┬──────────────┐\n");
    printf("  │ Level    │ Size      │ Latency      │ Bandwidth    │\n");
    printf("  ├──────────┼───────────┼──────────────┼──────────────┤\n");
    printf("  │ Register │ ~1 KB     │ 0 cycles     │ —            │\n");
    printf("  │ L1 cache │ 32-64 KB  │ ~4 cycles    │ ~100 GB/s    │\n");
    printf("  │ L2 cache │ 256-512KB │ ~12 cycles   │ ~50 GB/s     │\n");
    printf("  │ L3 cache │ 4-32 MB   │ ~40 cycles   │ ~30 GB/s     │\n");
    printf("  │ DRAM     │ GBs       │ ~200 cycles  │ ~10 GB/s     │\n");
    printf("  └──────────┴───────────┴──────────────┴──────────────┘\n\n");
    printf("  The benchmark below sweeps array sizes to reveal these boundaries.\n");
  }

  /* ── 4. Benchmarks ──────────────────────────────────────────────────── */
#ifdef BENCH_MODE
  {
    /* ── 4a. Row-major vs column-major ── */
    BenchSuite suite1;
    {
      int (*mat)[MAT_N] = malloc(MAT_N * MAT_N * sizeof(int));
      if (!mat) { fprintf(stderr, "OOM\n"); return 1; }
      memset(mat, 0, MAT_N * MAT_N * sizeof(int));

      long ops = (long)MAT_N * MAT_N;

      BENCH_SUITE(suite1, "Row-major vs Column-major (1024x1024 int)") {
        /* Row-major: arr[i][j] */
        BENCH_CASE(suite1, "row-major", iter, ops) {
          int i = (int)(iter / MAT_N);
          int j = (int)(iter % MAT_N);
          mat[i][j] += 1;
        }
        BENCH_CASE_END(suite1, ops);

        /* Column-major: arr[j][i] */
        BENCH_CASE(suite1, "column-major", iter, ops) {
          int j = (int)(iter / MAT_N);
          int i = (int)(iter % MAT_N);
          mat[i][j] += 1;
        }
        BENCH_CASE_END(suite1, ops);
      }
      free(mat);
    }

    /* ── 4b. Sequential vs random ── */
    BenchSuite suite2;
    {
      #define ARR_SIZE (4 * 1024 * 1024)  /* 4M ints = 16 MB */
      int *arr = malloc(ARR_SIZE * sizeof(int));
      if (!arr) { fprintf(stderr, "OOM\n"); return 1; }
      memset(arr, 0, ARR_SIZE * sizeof(int));

      /* Build random index table */
      uint32_t *indices = malloc(ARR_SIZE * sizeof(uint32_t));
      if (!indices) { fprintf(stderr, "OOM\n"); free(arr); return 1; }
      lcg_state = 42;
      for (int i = 0; i < ARR_SIZE; i++)
        indices[i] = lcg_next() % ARR_SIZE;

      long ops = ARR_SIZE;

      BENCH_SUITE(suite2, "Sequential vs Random (16 MB array)") {
        BENCH_CASE(suite2, "sequential", iter, ops) {
          arr[iter] += 1;
        }
        BENCH_CASE_END(suite2, ops);

        BENCH_CASE(suite2, "random", iter, ops) {
          arr[indices[iter]] += 1;
        }
        BENCH_CASE_END(suite2, ops);
      }
      free(arr);
      free(indices);
    }

    /* ── 4c. Array size sweep ── */
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  BENCHMARK: Array Size Sweep (cache boundary detection)     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    /* Sizes: 4K, 8K, 16K, 32K, 64K, 128K, 256K, 512K, 1M, 4M, 16M */
    size_t sweep_sizes[] = {
      4*1024, 8*1024, 16*1024, 32*1024, 64*1024,
      128*1024, 256*1024, 512*1024, 1024*1024,
      4*1024*1024, 16*1024*1024
    };
    int nsweep = (int)(sizeof(sweep_sizes) / sizeof(sweep_sizes[0]));

    printf("  %-12s  %-12s  %-12s\n", "Array Size", "Stride Walk", "Notes");
    printf("  %-12s  %-12s  %-12s\n", "----------", "-----------", "-----");

    for (int s = 0; s < nsweep; s++) {
      size_t sz = sweep_sizes[s];
      size_t count = sz / sizeof(int);
      int *buf = malloc(sz);
      if (!buf) continue;
      memset(buf, 0, sz);

      /* Stride-1 walk, multiple passes to get stable timing */
      long passes = (long)(64 * 1024 * 1024 / sz);
      if (passes < 2) passes = 2;

      uint64_t t0 = bench_now_ns();
      for (long p = 0; p < passes; p++) {
        for (size_t i = 0; i < count; i += 16) {
          buf[i] += 1;
        }
      }
      uint64_t t1 = bench_now_ns();

      long total_accesses = passes * (long)(count / 16);
      double ns_per = (double)(t1 - t0) / (double)total_accesses;

      const char *note = "";
      if (sz <= 32*1024)        note = "<= L1 (fast)";
      else if (sz <= 256*1024)  note = "L2 range";
      else if (sz <= 8*1024*1024) note = "L3 range";
      else                       note = "> L3 (DRAM)";

      char size_str[32];
      if (sz >= 1024*1024)
        snprintf(size_str, sizeof(size_str), "%zu MB", sz / (1024*1024));
      else
        snprintf(size_str, sizeof(size_str), "%zu KB", sz / 1024);

      printf("  %-12s  %8.2f ns   %s\n", size_str, ns_per, note);
      free(buf);
    }
    printf("\n  Look for latency jumps at L1->L2 and L2->L3 boundaries.\n\n");
  }
#else
  bench_skip_notice("lesson_26");
#endif

  printf("\n");
  return 0;
}
