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
    int64 seconds;
    int64 nanoseconds;
} PlatformTimeSpec;

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
real64 get_wall_clock(void);

/**
 * Calculate elapsed time between two wall clock readings.
 *
 * @param start Start time from get_wall_clock()
 * @param end End time from get_wall_clock()
 * @return Elapsed time in seconds (end - start)
 *
 * Note: This is a trivial subtraction, provided for API symmetry.
 */
de100_file_scoped_fn inline real64 get_seconds_elapsed(real64 start, real64 end) {
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
void platform_sleep_seconds(real64 seconds);

/**
 * Sleep for approximately the specified number of milliseconds.
 *
 * @param milliseconds Time to sleep in milliseconds
 *
 * Convenience wrapper around platform_sleep_seconds().
 */
void platform_sleep_ms(uint32 milliseconds);

// ═══════════════════════════════════════════════════════════════════════════
// LOW-LEVEL TIMESPEC FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════
//
// For code that needs direct timespec-style access (e.g., file timestamps,
// frame timing loops with nanosecond precision).
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get current time as PlatformTimeSpec.
 *
 * @param out_time Pointer to store the current time (must not be NULL)
 */
void platform_get_timespec(PlatformTimeSpec *out_time);

/**
 * Convert PlatformTimeSpec to seconds.
 *
 * @param time Pointer to time structure (NULL returns 0.0)
 * @return Time in seconds as real64
 */
real64 platform_timespec_to_seconds(const PlatformTimeSpec *time);

/**
 * Calculate difference between two PlatformTimeSpec values.
 *
 * @param start Start time
 * @param end End time
 * @return Elapsed time in seconds (end - start)
 *
 * Note: Returns 0.0 if either pointer is NULL.
 */
real64 platform_timespec_diff_seconds(const PlatformTimeSpec *start,
                                      const PlatformTimeSpec *end);

/**
 * Compare two PlatformTimeSpec values.
 *
 * @param a First time
 * @param b Second time
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
int32 platform_timespec_compare(const PlatformTimeSpec *a,
                                const PlatformTimeSpec *b);

#endif // DE100_COMMON_TIME_H
