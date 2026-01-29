#ifndef DE100_COMMON_TIME_H
#define DE100_COMMON_TIME_H

#include "base.h"

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM TIME STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════
// Cross-platform time representation with nanosecond precision.
// Replaces direct use of struct timespec / LARGE_INTEGER.
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
 * @return Current time in seconds with high precision
 *
 * Uses monotonic clock - never goes backwards, not affected by system time
 * changes. Ideal for measuring elapsed time and frame timing.
 */
real64 get_wall_clock(void);

/**
 * Calculate elapsed time between two wall clock readings.
 *
 * @param start Start time from get_wall_clock()
 * @param end End time from get_wall_clock()
 * @return Elapsed time in seconds
 */
real64 get_seconds_elapsed(real64 start, real64 end);

// ═══════════════════════════════════════════════════════════════════════════
// SLEEP FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Sleep for approximately the specified number of seconds.
 *
 * @param seconds Time to sleep in seconds
 *
 * Note: Actual sleep time may be longer due to OS scheduling.
 * For precise timing, use two-phase sleep (coarse sleep + spin-wait).
 */
void platform_sleep_seconds(real64 seconds);

/**
 * Sleep for approximately the specified number of milliseconds.
 *
 * @param milliseconds Time to sleep in milliseconds
 */
void platform_sleep_ms(uint32 milliseconds);

// ═══════════════════════════════════════════════════════════════════════════
// LOW-LEVEL TIMESPEC FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════
// For code that needs direct timespec-style access (e.g., frame timing loops)
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get current time as PlatformTimeSpec.
 *
 * @param out_time Pointer to store the current time
 */
void platform_get_timespec(PlatformTimeSpec *out_time);

/**
 * Convert PlatformTimeSpec to seconds.
 *
 * @param time Pointer to time structure
 * @return Time in seconds as real64
 */
real64 platform_timespec_to_seconds(const PlatformTimeSpec *time);

/**
 * Calculate difference between two PlatformTimeSpec values.
 *
 * @param start Start time
 * @param end End time
 * @return Elapsed time in seconds
 */
real64 platform_timespec_diff_seconds(const PlatformTimeSpec *start,
                                      const PlatformTimeSpec *end);

#endif // DE100_COMMON_TIME_H
