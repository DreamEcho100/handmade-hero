#ifndef UTILS_PERF_H
#define UTILS_PERF_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/perf.h — Lightweight in-game per-function profiler
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Zero-cost in release builds when ENABLE_PERF is not defined.
 *
 * BUILD
 * ─────
 * Standard dev build (ASAN, O0, no profiling):
 *   ./build-dev.sh --backend=raylib
 *
 * Bench build (O2, no ASAN, profiler active, auto-exit after N seconds):
 *   ./build-dev.sh --backend=raylib --bench=25
 * ↑ Adds: -O2 -DENABLE_PERF -DBENCH_DURATION_S=25  and  src/utils/perf.c
 *
 * USAGE
 * ─────
 * 1. #include "../utils/perf.h"  in any .c file you want to profile.
 * 2. Wrap sections with PERF_BEGIN(tag) ... PERF_END(tag).
 *    `tag` must be a valid C identifier (use underscores, no spaces/slashes).
 * 3. Optionally use PERF_BEGIN_NAMED(tag, "display/name") for a prettier
 *    string in the output (the tag is still an identifier).
 * 4. Call perf_print_all() at program exit — or let BENCH_DURATION_S trigger
 *    it automatically from the platform main loop.
 *
 * EXAMPLE
 * ───────
 *   void demo_render(...) {
 *       PERF_BEGIN_NAMED(dr_total, "demo_render/total");
 *
 *       PERF_BEGIN(make_ctx);
 *       RenderContext ctx = make_render_context(bb, cfg);
 *       PERF_END(make_ctx);
 *
 *       PERF_BEGIN(draw_bg);
 *       draw_rect(bb, 0, 0, ...);
 *       PERF_END(draw_bg);
 *
 *       PERF_END(dr_total);
 *   }
 *
 * DESIGN
 * ──────
 * • Each (tag × call-site) pair is assigned a slot index the first time it
 *   runs.  The index is stored in a `static int` so the slot lookup happens
 *   exactly once per call-site over the lifetime of the process.
 * • PERF_BEGIN captures clock_gettime(CLOCK_MONOTONIC) into a stack variable.
 * • PERF_END computes elapsed nanoseconds and updates total/min/max/count.
 * • PERF_MAX_TIMERS is the maximum number of distinct timer slots (64 is
 *   enough for any single game-demo use case).
 *
 * NESTING
 * ───────
 * Timers may nest freely as long as each pair uses a distinct tag:
 *   PERF_BEGIN(outer);
 *     PERF_BEGIN(inner_a); ...; PERF_END(inner_a);
 *     PERF_BEGIN(inner_b); ...; PERF_END(inner_b);
 *   PERF_END(outer);
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#ifdef ENABLE_PERF

#include <limits.h>   /* LLONG_MAX */
#include <stdio.h>
#include <time.h>     /* clock_gettime, CLOCK_MONOTONIC, struct timespec */

#define PERF_MAX_TIMERS 64

/* ── PerfTimer ──────────────────────────────────────────────────────────── */
typedef struct {
  const char *name;        /* display name (stringified tag or PERF_BEGIN_NAMED) */
  long long   total_ns;    /* cumulative nanoseconds across all calls             */
  long long   min_ns;      /* fastest single recorded call                        */
  long long   max_ns;      /* slowest single recorded call                        */
  long long   call_count;  /* how many PERF_END calls have been recorded          */
} PerfTimer;

extern PerfTimer g_perf[PERF_MAX_TIMERS];
extern int       g_perf_count;

/* ── PERF_BEGIN(tag) ────────────────────────────────────────────────────────
 * Declare a static slot index (lazily initialised on first call) and
 * capture the start timestamp.  `tag` must be a valid C identifier.
 *
 * The static int persists across calls — the slot is grabbed once, reused
 * for every subsequent call.  The timespec is on the stack — fresh each call.
 */
#define PERF_BEGIN(tag)                                                         \
    static int _perf_idx_##tag = -1;                                            \
    if (_perf_idx_##tag < 0 && g_perf_count < PERF_MAX_TIMERS) {               \
        _perf_idx_##tag = g_perf_count++;                                       \
        g_perf[_perf_idx_##tag].name    = #tag;                                 \
        g_perf[_perf_idx_##tag].min_ns  = LLONG_MAX;                            \
    }                                                                           \
    struct timespec _perf_s_##tag;                                              \
    clock_gettime(CLOCK_MONOTONIC, &_perf_s_##tag)

/* ── PERF_BEGIN_NAMED(tag, display_name) ────────────────────────────────────
 * Same as PERF_BEGIN but stores a custom string as the display name.
 * Useful when the desired display name contains characters not valid in C
 * identifiers (spaces, slashes):
 *   PERF_BEGIN_NAMED(dr_total, "demo_render/total");
 */
#define PERF_BEGIN_NAMED(tag, display_name)                                     \
    static int _perf_idx_##tag = -1;                                            \
    if (_perf_idx_##tag < 0 && g_perf_count < PERF_MAX_TIMERS) {               \
        _perf_idx_##tag = g_perf_count++;                                       \
        g_perf[_perf_idx_##tag].name    = (display_name);                       \
        g_perf[_perf_idx_##tag].min_ns  = LLONG_MAX;                            \
    }                                                                           \
    struct timespec _perf_s_##tag;                                              \
    clock_gettime(CLOCK_MONOTONIC, &_perf_s_##tag)

/* ── PERF_END(tag) ──────────────────────────────────────────────────────────
 * Capture end timestamp, compute elapsed ns, update the slot's stats.
 * The `tag` must match the corresponding PERF_BEGIN.
 */
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

/* ── perf_print_all ─────────────────────────────────────────────────────────
 * Print a summary table of all recorded timers to stdout.
 * Called automatically when BENCH_DURATION_S elapses (from the platform
 * main loop); also safe to call manually at program exit.
 */
void perf_print_all(void);

#else  /* !ENABLE_PERF — zero-cost no-ops */

#define PERF_BEGIN(tag)                      do {} while(0)
#define PERF_BEGIN_NAMED(tag, display_name)  do {} while(0)
#define PERF_END(tag)                        do {} while(0)
static inline void perf_print_all(void) {}

#endif /* ENABLE_PERF */

#endif /* UTILS_PERF_H */
