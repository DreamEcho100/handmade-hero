#ifndef UTILS_PERF_H
#define UTILS_PERF_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/perf.h — Lightweight per-function profiler for Asteroids
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Zero-cost in standard dev builds (ENABLE_PERF not defined).
 * Active only when building with --bench=N (adds -DENABLE_PERF -DBENCH_DURATION_S=N).
 *
 * USAGE
 * ─────
 * 1. #include "../utils/perf.h"  in any .c file you want to profile.
 * 2. Wrap sections with PERF_BEGIN(tag) / PERF_END(tag).
 *    `tag` must be a valid C identifier.
 * 3. Use PERF_BEGIN_NAMED(tag, "display/name") for prettier output.
 * 4. Call perf_print_all() at exit — or BENCH_DURATION_S triggers it
 *    automatically from the platform main loop.
 *
 * EXAMPLE
 * ───────
 *   PERF_BEGIN_NAMED(asteroids_update_total, "asteroids_update/total");
 *   asteroids_update(&state, curr_input, dt);
 *   PERF_END(asteroids_update_total);
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#ifdef ENABLE_PERF

#include <limits.h>   /* LLONG_MAX */
#include <stdio.h>
#include <time.h>

#define PERF_MAX_TIMERS 64

typedef struct {
  const char *name;
  long long   total_ns;
  long long   min_ns;
  long long   max_ns;
  long long   call_count;
} PerfTimer;

extern PerfTimer g_perf[PERF_MAX_TIMERS];
extern int       g_perf_count;

#define PERF_BEGIN(tag)                                                         \
    static int _perf_idx_##tag = -1;                                            \
    if (_perf_idx_##tag < 0 && g_perf_count < PERF_MAX_TIMERS) {               \
        _perf_idx_##tag = g_perf_count++;                                       \
        g_perf[_perf_idx_##tag].name    = #tag;                                 \
        g_perf[_perf_idx_##tag].min_ns  = LLONG_MAX;                            \
    }                                                                           \
    struct timespec _perf_s_##tag;                                              \
    clock_gettime(CLOCK_MONOTONIC, &_perf_s_##tag)

#define PERF_BEGIN_NAMED(tag, display_name)                                     \
    static int _perf_idx_##tag = -1;                                            \
    if (_perf_idx_##tag < 0 && g_perf_count < PERF_MAX_TIMERS) {               \
        _perf_idx_##tag = g_perf_count++;                                       \
        g_perf[_perf_idx_##tag].name    = (display_name);                       \
        g_perf[_perf_idx_##tag].min_ns  = LLONG_MAX;                            \
    }                                                                           \
    struct timespec _perf_s_##tag;                                              \
    clock_gettime(CLOCK_MONOTONIC, &_perf_s_##tag)

#define PERF_END(tag) do {                                                      \
    if (_perf_idx_##tag >= 0) {                                                 \
        struct timespec _perf_e_##tag;                                          \
        clock_gettime(CLOCK_MONOTONIC, &_perf_e_##tag);                         \
        long long _perf_dt_##tag =                                              \
            (_perf_e_##tag.tv_sec  - _perf_s_##tag.tv_sec)  * 1000000000LL +   \
            (_perf_e_##tag.tv_nsec - _perf_s_##tag.tv_nsec);                   \
        g_perf[_perf_idx_##tag].total_ns   += _perf_dt_##tag;                  \
        g_perf[_perf_idx_##tag].call_count++;                                   \
        if (_perf_dt_##tag < g_perf[_perf_idx_##tag].min_ns)                   \
            g_perf[_perf_idx_##tag].min_ns = _perf_dt_##tag;                   \
        if (_perf_dt_##tag > g_perf[_perf_idx_##tag].max_ns)                   \
            g_perf[_perf_idx_##tag].max_ns = _perf_dt_##tag;                   \
    }                                                                           \
} while(0)

void perf_print_all(void);

#else  /* !ENABLE_PERF — zero-cost no-ops */

#define PERF_BEGIN(tag)                      do {} while(0)
#define PERF_BEGIN_NAMED(tag, display_name)  do {} while(0)
#define PERF_END(tag)                        do {} while(0)
static inline void perf_print_all(void) {}

#endif /* ENABLE_PERF */

#endif /* UTILS_PERF_H */
