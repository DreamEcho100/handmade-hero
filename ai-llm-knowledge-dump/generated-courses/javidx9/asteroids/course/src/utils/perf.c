#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/perf.c — Global timer storage + perf_print_all()
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Only compiled when ENABLE_PERF is defined (--bench=N flag in build-dev.sh).
 * Provides definitions for g_perf[], g_perf_count, and perf_print_all().
 *
 * When ENABLE_PERF is not defined this file compiles to an empty translation unit.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#ifdef ENABLE_PERF

#include <stdio.h>
#include <stdlib.h>
#include "perf.h"

PerfTimer g_perf[PERF_MAX_TIMERS];
int       g_perf_count = 0;

static int cmp_by_total_desc(const void *a, const void *b) {
  const PerfTimer *ta = (const PerfTimer *)a;
  const PerfTimer *tb = (const PerfTimer *)b;
  if (tb->total_ns > ta->total_ns) return  1;
  if (tb->total_ns < ta->total_ns) return -1;
  return 0;
}

void perf_print_all(void) {
  if (g_perf_count == 0) {
    printf("\n[perf] No timers recorded.\n");
    return;
  }

  PerfTimer sorted[PERF_MAX_TIMERS];
  for (int i = 0; i < g_perf_count; i++) sorted[i] = g_perf[i];
  qsort(sorted, (size_t)g_perf_count, sizeof(PerfTimer), cmp_by_total_desc);

  printf("\n");
  printf("══════════════════════════════════════════════════════════════════════════\n");
  printf("  ASTEROIDS PROFILER RESULTS\n");
#ifdef BENCH_DURATION_S
  printf("  (bench duration: %d s, sorted by total time descending)\n",
         BENCH_DURATION_S);
#endif
  printf("══════════════════════════════════════════════════════════════════════════\n");
  printf("  %-32s %8s %9s %9s %9s %10s\n",
         "Function", "calls", "avg µs", "min µs", "max µs", "total ms");
  printf("  %-32s %8s %9s %9s %9s %10s\n",
         "────────────────────────────────",
         "────────", "─────────", "─────────", "─────────", "──────────");

  for (int i = 0; i < g_perf_count; i++) {
    const PerfTimer *t = &sorted[i];
    if (t->call_count == 0) continue;

    double avg_us   = (double)(t->total_ns / t->call_count) / 1000.0;
    double min_us   = (double)t->min_ns / 1000.0;
    double max_us   = (double)t->max_ns / 1000.0;
    double total_ms = (double)t->total_ns / 1e6;

    printf("  %-32s %8lld %9.2f %9.2f %9.2f %10.2f\n",
           t->name,
           (long long)t->call_count,
           avg_us, min_us, max_us, total_ms);
  }

  printf("══════════════════════════════════════════════════════════════════════════\n");
  printf("  avg/min/max in microseconds (µs);  total in milliseconds (ms)\n");
  printf("  Build flags: -O2 -DENABLE_PERF (no ASan)\n");
  printf("══════════════════════════════════════════════════════════════════════════\n\n");
}

#endif /* ENABLE_PERF */
