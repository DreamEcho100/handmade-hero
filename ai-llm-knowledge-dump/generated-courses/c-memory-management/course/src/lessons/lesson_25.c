/* lesson_25.c — Full Custom Debug Allocator
 * BUILD_DEPS: common/bench.c, lib/debug_alloc.c
 *
 * Extends the debug allocator from lesson 09 with:
 *   - Allocation log with __FILE__/__LINE__
 *   - Leak report at exit
 *   - Allocation size histogram
 *   - High-water mark tracking
 *
 * Run:   ./build-dev.sh --lesson=25 -r
 * Bench: ./build-dev.sh --lesson=25 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/debug_alloc.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Extended Debug Allocator — layered on top of lib/debug_alloc.h
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The base debug_alloc.h provides canary checking and fill patterns.
 * Here we add tracking infrastructure: an allocation log, size histogram,
 * and high-water mark.  In a real codebase you'd merge these, but keeping
 * them separate shows the layered approach.
 */

#define XDALLOC_MAX_LOG 4096

typedef struct XDAllocEntry {
  void       *ptr;
  size_t      size;
  const char *file;
  int         line;
  int         freed;  /* 1 = freed, 0 = still live */
} XDAllocEntry;

/* Size histogram buckets: [0-32], (32-128], (128-512], (512-4K], (4K+] */
#define HIST_BUCKETS 5
static const char *hist_labels[HIST_BUCKETS] = {
  "0-32 B", "33-128 B", "129-512 B", "513-4096 B", "4097+ B"
};

typedef struct XDAllocState {
  XDAllocEntry log[XDALLOC_MAX_LOG];
  size_t       log_count;
  size_t       current_bytes;
  size_t       high_water_bytes;
  size_t       current_count;
  size_t       high_water_count;
  size_t       total_allocs;
  size_t       total_frees;
  size_t       histogram[HIST_BUCKETS];
} XDAllocState;

static XDAllocState g_xda = {0};

static int hist_bucket(size_t size) {
  if (size <= 32)   return 0;
  if (size <= 128)  return 1;
  if (size <= 512)  return 2;
  if (size <= 4096) return 3;
  return 4;
}

static void *xdalloc(size_t size, const char *file, int line) {
  void *ptr = debug_alloc(size, file, line);
  if (!ptr) return NULL;

  /* Log the allocation */
  if (g_xda.log_count < XDALLOC_MAX_LOG) {
    XDAllocEntry *e = &g_xda.log[g_xda.log_count++];
    e->ptr  = ptr;
    e->size = size;
    e->file = file;
    e->line = line;
    e->freed = 0;
  }

  /* Update stats */
  g_xda.total_allocs++;
  g_xda.current_bytes += size;
  g_xda.current_count++;
  if (g_xda.current_bytes > g_xda.high_water_bytes)
    g_xda.high_water_bytes = g_xda.current_bytes;
  if (g_xda.current_count > g_xda.high_water_count)
    g_xda.high_water_count = g_xda.current_count;
  g_xda.histogram[hist_bucket(size)]++;

  return ptr;
}

static void xdfree(void *ptr, const char *file, int line) {
  if (!ptr) return;

  /* Find the log entry and mark it freed */
  for (size_t i = 0; i < g_xda.log_count; i++) {
    if (g_xda.log[i].ptr == ptr && !g_xda.log[i].freed) {
      g_xda.log[i].freed = 1;
      if (g_xda.current_bytes >= g_xda.log[i].size)
        g_xda.current_bytes -= g_xda.log[i].size;
      if (g_xda.current_count > 0)
        g_xda.current_count--;
      break;
    }
  }

  g_xda.total_frees++;
  debug_free(ptr, file, line);
}

#define XDALLOC(size) xdalloc((size), __FILE__, __LINE__)
#define XDFREE(ptr)   xdfree((ptr),  __FILE__, __LINE__)

/* ── Leak report ───────────────────────────────────────────────────────────*/

static void xdalloc_leak_report(void) {
  printf("\n");
  printf("══════════════════════════════════════════════════════════════\n");
  printf("  Extended Debug Allocator — Leak Report\n");
  printf("══════════════════════════════════════════════════════════════\n\n");

  printf("  Total allocations:     %zu\n", g_xda.total_allocs);
  printf("  Total frees:           %zu\n", g_xda.total_frees);
  printf("  High-water mark:       %zu bytes (%zu allocations)\n",
         g_xda.high_water_bytes, g_xda.high_water_count);
  printf("  Currently live:        %zu bytes (%zu allocations)\n\n",
         g_xda.current_bytes, g_xda.current_count);

  /* Size histogram */
  printf("  Allocation size histogram:\n");
  size_t max_count = 0;
  for (int i = 0; i < HIST_BUCKETS; i++)
    if (g_xda.histogram[i] > max_count) max_count = g_xda.histogram[i];

  for (int i = 0; i < HIST_BUCKETS; i++) {
    int bar_len = (max_count > 0)
                ? (int)(g_xda.histogram[i] * 30 / max_count)
                : 0;
    printf("    %-12s [%5zu] ", hist_labels[i], g_xda.histogram[i]);
    for (int j = 0; j < bar_len; j++) printf("#");
    printf("\n");
  }
  printf("\n");

  /* List leaked allocations */
  size_t leak_count = 0;
  size_t leak_bytes = 0;
  for (size_t i = 0; i < g_xda.log_count; i++) {
    if (!g_xda.log[i].freed) {
      leak_count++;
      leak_bytes += g_xda.log[i].size;
    }
  }

  if (leak_count == 0) {
    printf("  No leaks detected. All allocations freed.\n\n");
  } else {
    printf("  LEAKED: %zu allocation(s), %zu bytes total:\n\n", leak_count, leak_bytes);
    size_t shown = 0;
    for (size_t i = 0; i < g_xda.log_count && shown < 20; i++) {
      if (!g_xda.log[i].freed) {
        printf("    #%zu: %zu bytes at %p — allocated at %s:%d\n",
               shown + 1,
               g_xda.log[i].size,
               g_xda.log[i].ptr,
               g_xda.log[i].file,
               g_xda.log[i].line);
        shown++;
      }
    }
    if (leak_count > 20)
      printf("    ... and %zu more\n", leak_count - 20);
    printf("\n");
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Demo workload
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 25 — Full Custom Debug Allocator\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Normal allocation/free cycle ────────────────────────────────── */
  print_section("Normal Alloc/Free Cycle");
  {
    void *a = XDALLOC(16);
    void *b = XDALLOC(64);
    void *c = XDALLOC(256);
    void *d = XDALLOC(1024);
    void *e = XDALLOC(8192);

    printf("  Allocated 5 blocks of varying sizes.\n");
    printf("  Current live: %zu bytes in %zu allocations\n",
           g_xda.current_bytes, g_xda.current_count);

    XDFREE(a);
    XDFREE(b);
    XDFREE(c);
    XDFREE(d);
    XDFREE(e);

    printf("  Freed all 5 blocks.\n");
    printf("  Current live: %zu bytes in %zu allocations\n",
           g_xda.current_bytes, g_xda.current_count);
  }

  /* ── 2. Simulated workload with varied sizes ────────────────────────── */
  print_section("Simulated Workload (200 allocations)");
  {
    #define WORKLOAD_N 200
    void *ptrs[WORKLOAD_N];
    size_t sizes[] = { 8, 16, 24, 32, 48, 64, 100, 128, 200, 256,
                       400, 512, 1024, 2048, 4096, 5000, 8192 };
    size_t nsizes = sizeof(sizes) / sizeof(sizes[0]);

    /* Allocate all */
    for (int i = 0; i < WORKLOAD_N; i++) {
      ptrs[i] = XDALLOC(sizes[(size_t)i % nsizes]);
      if (ptrs[i]) memset(ptrs[i], (char)i, sizes[(size_t)i % nsizes]);
    }

    printf("  Allocated %d blocks.\n", WORKLOAD_N);
    printf("  High-water mark: %zu bytes\n", g_xda.high_water_bytes);

    /* Free half — simulate realistic churn */
    int freed_count = 0;
    for (int i = 0; i < WORKLOAD_N; i += 2) {
      XDFREE(ptrs[i]);
      ptrs[i] = NULL;
      freed_count++;
    }
    printf("  Freed %d blocks (every other one).\n", freed_count);
    printf("  Current live: %zu bytes in %zu allocations\n",
           g_xda.current_bytes, g_xda.current_count);

    /* Free the rest */
    for (int i = 0; i < WORKLOAD_N; i++) {
      if (ptrs[i]) XDFREE(ptrs[i]);
    }
    printf("  Freed remaining blocks.\n");
  }

  /* ── 3. Intentional leaks for the leak report ──────────────────────── */
  print_section("Intentional Leaks (for demo)");
  {
    void *leak1 = XDALLOC(42);
    void *leak2 = XDALLOC(128);
    void *leak3 = XDALLOC(999);
    (void)leak1; (void)leak2; (void)leak3;
    printf("  Allocated 3 blocks and intentionally NOT freeing them.\n");
    printf("  The leak report below will identify each one.\n");
  }

  /* ── 4. Full leak report ────────────────────────────────────────────── */
  xdalloc_leak_report();

  /* Also print the base debug_alloc report */
  debug_alloc_report();

  /* ── 5. Benchmark ───────────────────────────────────────────────────── */
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 500000;

    BENCH_SUITE(suite, "Debug alloc vs raw malloc (64 bytes each)") {
      BENCH_CASE(suite, "malloc+free", i, N) {
        void *p = malloc(64);
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);

      BENCH_CASE(suite, "debug_alloc+free", i, N) {
        void *p = debug_alloc(64, "bench", 0);
        *(volatile char *)p = 0;
        debug_free(p, "bench", 0);
      }
      BENCH_CASE_END(suite, N);
    }

    printf("  Debug allocator is slower due to canary writes, fill patterns,\n");
    printf("  and bookkeeping.  That's the point — use it in debug builds only.\n\n");
  }
#else
  bench_skip_notice("lesson_25");
#endif

  printf("\n");
  return 0;
}
