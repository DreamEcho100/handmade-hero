#define _POSIX_C_SOURCE 200809L
/* ═══════════════════════════════════════════════════════════════════════════
 * common/bench.c — Benchmark framework implementation
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "common/bench.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ── Monotonic clock ──────────────────────────────────────────────────────*/

uint64_t bench_now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ── Suite ────────────────────────────────────────────────────────────────*/

void bench_suite_begin(BenchSuite *suite, const char *title) {
  memset(suite, 0, sizeof(*suite));
  snprintf(suite->title, sizeof(suite->title), "%s", title);
  printf("\n");
  printf("╔══════════════════════════════════════════════════════════════╗\n");
  printf("║  BENCHMARK: %-48s║\n", title);
  printf("╚══════════════════════════════════════════════════════════════╝\n");
}

void bench_suite_end(const BenchSuite *suite) {
  if (suite->count == 0) return;

  printf("\n");
  printf("┌────────────────────────┬──────────────┬──────────────┬──────────┐\n");
  printf("│ %-22s │ %12s │ %12s │ %8s │\n",
         "Case", "Total (ms)", "Per-op (ns)", "Relative");
  printf("├────────────────────────┼──────────────┼──────────────┼──────────┤\n");

  double baseline = suite->results[0].per_op_ns;

  for (int i = 0; i < suite->count; i++) {
    const BenchResult *r = &suite->results[i];
    double rel = (baseline > 0.0) ? r->per_op_ns / baseline : 0.0;
    printf("│ %-22s │ %12.2f │ %12.2f │ %7.2fx │\n",
           r->name,
           r->total_ns / 1e6,
           r->per_op_ns,
           rel);
  }

  printf("└────────────────────────┴──────────────┴──────────────┴──────────┘\n");

  /* Summary: fastest vs slowest */
  if (suite->count >= 2) {
    double fastest = suite->results[0].per_op_ns;
    int    fastest_idx = 0;
    double slowest = suite->results[0].per_op_ns;
    int    slowest_idx = 0;
    for (int i = 1; i < suite->count; i++) {
      if (suite->results[i].per_op_ns < fastest) {
        fastest = suite->results[i].per_op_ns;
        fastest_idx = i;
      }
      if (suite->results[i].per_op_ns > slowest) {
        slowest = suite->results[i].per_op_ns;
        slowest_idx = i;
      }
    }
    if (fastest > 0.0 && fastest_idx != slowest_idx) {
      printf("\n  >> \"%s\" is %.1fx faster than \"%s\"\n",
             suite->results[fastest_idx].name,
             slowest / fastest,
             suite->results[slowest_idx].name);
    }
  }
  printf("\n");
}

/* ── Case ─────────────────────────────────────────────────────────────────*/

void bench_case_begin(BenchSuite *suite, const char *name) {
  if (suite->count >= BENCH_MAX_CASES) return;
  snprintf(suite->results[suite->count].name,
           BENCH_MAX_NAME, "%s", name);
  suite->case_start_ns = bench_now_ns();
}

void bench_case_end(BenchSuite *suite, long iterations) {
  uint64_t end = bench_now_ns();
  if (suite->count >= BENCH_MAX_CASES) return;

  BenchResult *r = &suite->results[suite->count];
  r->iterations = iterations;
  r->total_ns   = (double)(end - suite->case_start_ns);
  r->per_op_ns  = (iterations > 0) ? r->total_ns / (double)iterations : 0.0;
  suite->count++;

  printf("  %-22s  %ld iters  %.2f ms  (%.2f ns/op)\n",
         r->name, iterations, r->total_ns / 1e6, r->per_op_ns);
}

/* ── Skip notice ──────────────────────────────────────────────────────────*/

void bench_skip_notice(const char *lesson_name) {
  printf("[%s] Benchmarks disabled. Rebuild with --bench to enable:\n",
         lesson_name);
  printf("  ./build-dev.sh --lesson=NN --bench -r\n\n");
}
