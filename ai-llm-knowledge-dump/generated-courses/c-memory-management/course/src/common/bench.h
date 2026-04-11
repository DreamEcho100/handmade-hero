#ifndef COMMON_BENCH_H
#define COMMON_BENCH_H

/* ═══════════════════════════════════════════════════════════════════════════
 * common/bench.h — Lightweight benchmark framework
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Usage:
 *   bench_suite_begin("Arena vs Malloc");
 *
 *   bench_case_begin("malloc+free");
 *   for (long i = 0; i < N; i++) { void *p = malloc(64); free(p); }
 *   bench_case_end(N);
 *
 *   bench_case_begin("arena_push");
 *   for (long i = 0; i < N; i++) { arena_push_size(&a, 64, 8); }
 *   bench_case_end(N);
 *
 *   bench_suite_end();   // prints comparison table
 *
 * In non-BENCH_MODE builds, bench_suite_begin prints a notice and all
 * other calls are no-ops.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include <stddef.h>

#define BENCH_MAX_CASES 16
#define BENCH_MAX_NAME  64

typedef struct BenchResult {
  char   name[BENCH_MAX_NAME];
  long   iterations;
  double total_ns;
  double per_op_ns;
} BenchResult;

typedef struct BenchSuite {
  char        title[128];
  BenchResult results[BENCH_MAX_CASES];
  int         count;
  uint64_t    case_start_ns;  /* set by bench_case_begin */
} BenchSuite;

/* Get monotonic time in nanoseconds. */
uint64_t bench_now_ns(void);

/* Suite lifecycle. */
void bench_suite_begin(BenchSuite *suite, const char *title);
void bench_suite_end(const BenchSuite *suite);

/* Case lifecycle — call begin before the hot loop, end after. */
void bench_case_begin(BenchSuite *suite, const char *name);
void bench_case_end(BenchSuite *suite, long iterations);

/* Print a message when benchmarks are disabled. */
void bench_skip_notice(const char *lesson_name);

/* ── Convenience macros ───────────────────────────────────────────────────
 *
 * BENCH_SUITE(suite, title) { ... cases ... }
 * BENCH_CASE(suite, name, iters) { ... hot loop body ... }
 *
 * Under BENCH_MODE these expand to the real benchmark calls.
 * Without BENCH_MODE they expand to nothing.
 */
#ifdef BENCH_MODE

#define BENCH_SUITE(suite_var, title_str)                                   \
  for (int _bs_once = (bench_suite_begin(&(suite_var), (title_str)), 0);   \
       _bs_once == 0; _bs_once = (bench_suite_end(&(suite_var)), 1))

#define BENCH_CASE(suite_var, name_str, iter_var, iter_count)               \
  bench_case_begin(&(suite_var), (name_str));                               \
  for (long iter_var = 0; iter_var < (long)(iter_count); iter_var++)

#define BENCH_CASE_END(suite_var, iter_count)                               \
  bench_case_end(&(suite_var), (long)(iter_count))

#else /* !BENCH_MODE */

#define BENCH_SUITE(suite_var, title_str) if (0)
#define BENCH_CASE(suite_var, name_str, iter_var, iter_count) if (0) for (long iter_var = 0; iter_var < 1; iter_var++)
#define BENCH_CASE_END(suite_var, iter_count) ((void)0)

#endif /* BENCH_MODE */

#endif /* COMMON_BENCH_H */
