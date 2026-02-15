#ifndef DE100_COMMON_TIME_H
#define DE100_COMMON_TIME_H

#include "base.h"

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM TIME STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════
//
// Cross-platform time representation with nanosecond precision.
// Replaces direct use of struct timespec / LARGE_INTEGER / mach_absolute_time.
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  i64 seconds;
  i64 nanoseconds;
} De100TimeSpec;

// ═══════════════════════════════════════════════════════════════════════════
// HIGH-LEVEL TIME FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get current wall clock time in seconds.
 *
 * Uses monotonic clock - never goes backwards, not affected by
 * system time changes. Ideal for measuring elapsed time and frame timing.
 *
 * @return Current time in seconds with high precision (typically ~1ns)
 */
f64 de100_get_wall_clock(void);

/**
 * Calculate elapsed time between two wall clock readings.
 *
 * @param start Start time from de100_get_wall_clock()
 * @param end End time from de100_get_wall_clock()
 * @return Elapsed time in seconds (end - start)
 *
 * Note: This is a trivial subtraction, provided for API symmetry.
 */
de100_file_scoped_fn inline f64 de100_get_seconds_elapsed(f64 start, f64 end) {
  return end - start;
}

// ═══════════════════════════════════════════════════════════════════════════
// SLEEP FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Sleep for approximately the specified number of seconds.
 *
 * @param seconds Time to sleep in seconds (can be fractional)
 *
 * Note: Actual sleep time may be longer due to OS scheduling.
 * For precise timing, use two-phase approach:
 *   1. Sleep for most of the time (coarse)
 *   2. Spin-wait for the remainder (precise)
 */
void de100_sleep_seconds(f64 seconds);

/**
 * Sleep for approximately the specified number of milliseconds.
 *
 * @param milliseconds Time to sleep in milliseconds
 *
 * Convenience wrapper around de100_sleep_seconds().
 */
void de100_sleep_ms(u32 milliseconds);

// ═══════════════════════════════════════════════════════════════════════════
// LOW-LEVEL TIMESPEC FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════
//
// For code that needs direct timespec-style access (e.g., file timestamps,
// frame timing loops with nanosecond precision).
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get current time as De100TimeSpec.
 *
 * @param out_time Pointer to store the current time (must not be NULL)
 */
void de100_get_timespec(De100TimeSpec *out_time);

/**
 * Convert De100TimeSpec to seconds.
 *
 * @param time Pointer to time structure (NULL returns 0.0)
 * @return Time in seconds as f64
 */
f64 de100_timespec_to_seconds(const De100TimeSpec *time);

/**
 * Calculate difference between two De100TimeSpec values.
 *
 * @param start Start time
 * @param end End time
 * @return Elapsed time in seconds (end - start)
 *
 * Note: Returns 0.0 if either pointer is NULL.
 */
f64 de100_timespec_diff_seconds(const De100TimeSpec *start,
                                const De100TimeSpec *end);

f64 de100_timespec_diff_milliseconds(const De100TimeSpec *start,
                                     const De100TimeSpec *end);

/**
 * Compare two De100TimeSpec values.
 *
 * @param a First time
 * @param b Second time
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
i32 de100_timespec_compare(const De100TimeSpec *a, const De100TimeSpec *b);

#endif // DE100_COMMON_TIME_H
