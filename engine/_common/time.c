#include "time.h"

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM-SPECIFIC INCLUDES
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <errno.h>
#include <mach/mach_time.h>
#include <time.h>
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__unix__)
#include <errno.h>
#include <time.h>
#else
#error "Unsupported platform for time operations"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM-SPECIFIC CACHED STATE
// ═══════════════════════════════════════════════════════════════════════════
//
// These values are queried once and cached for performance.
// QueryPerformanceFrequency and mach_timebase_info are expensive syscalls.
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)

// Windows: Cache the performance counter frequency
de100_file_scoped_fn inline int64 win32_get_frequency(void) {
  // Thread-safe on Windows due to static initialization guarantees
  local_persist_var LARGE_INTEGER frequency = {0};
  local_persist_var bool initialized = false;

  if (!initialized) {
    QueryPerformanceFrequency(&frequency);
    initialized = true;
  }

  return frequency.QuadPart;
}

#elif defined(__APPLE__)

// macOS: Cache the mach timebase info
typedef struct {
  uint32 numer;
  uint32 denom;
  bool initialized;
} CachedTimebase;

de100_file_scoped_fn inline CachedTimebase macos_get_timebase(void) {
  local_persist_var CachedTimebase cached = {0};

  if (!cached.initialized) {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    cached.numer = info.numer;
    cached.denom = info.denom;
    cached.initialized = true;
  }

  return cached;
}

#endif

// ═══════════════════════════════════════════════════════════════════════════
// GET WALL CLOCK TIME
// ═══════════════════════════════════════════════════════════════════════════

real64 get_wall_clock(void) {
#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS: QueryPerformanceCounter
  // ─────────────────────────────────────────────────────────────────────
  // Returns counts since system boot. Divide by frequency to get seconds.
  // Frequency is typically 10MHz on modern systems.
  // ─────────────────────────────────────────────────────────────────────

  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);

  int64 frequency = win32_get_frequency();

  return (real64)counter.QuadPart / (real64)frequency;

#elif defined(__APPLE__)
  // ─────────────────────────────────────────────────────────────────────
  // MACOS: mach_absolute_time
  // ─────────────────────────────────────────────────────────────────────
  // Returns ticks in an arbitrary unit. Must convert using timebase.
  // numer/denom converts ticks to nanoseconds.
  // ─────────────────────────────────────────────────────────────────────

  uint64 time = mach_absolute_time();
  CachedTimebase tb = macos_get_timebase();

  // Convert to nanoseconds, then to seconds
  uint64 nanos = time * tb.numer / tb.denom;

  return (real64)nanos / 1000000000.0;

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__unix__)
  // ─────────────────────────────────────────────────────────────────────
  // LINUX/BSD: clock_gettime with CLOCK_MONOTONIC
  // ─────────────────────────────────────────────────────────────────────
  // CLOCK_MONOTONIC: Monotonically increasing, not affected by NTP.
  // CLOCK_MONOTONIC_RAW: Same but also not affected by adjtime().
  // We use CLOCK_MONOTONIC for broader compatibility.
  // ─────────────────────────────────────────────────────────────────────

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  return (real64)ts.tv_sec + (real64)ts.tv_nsec / 1000000000.0;

#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// SLEEP FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

void platform_sleep_seconds(real64 seconds) {
  if (seconds <= 0.0) {
    return;
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS: Sleep() takes milliseconds
  // ─────────────────────────────────────────────────────────────────────
  // Note: Windows Sleep has ~15ms granularity by default.
  // For better precision, use timeBeginPeriod(1) at app startup.
  // ─────────────────────────────────────────────────────────────────────

  DWORD milliseconds = (DWORD)(seconds * 1000.0);
  if (milliseconds > 0) {
    Sleep(milliseconds);
  }

#elif defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) ||      \
    defined(__unix__)
  // ─────────────────────────────────────────────────────────────────────
  // POSIX: nanosleep() with EINTR handling
  // ─────────────────────────────────────────────────────────────────────
  // nanosleep can be interrupted by signals. When interrupted, it
  // stores the remaining time in the second argument. We loop until
  // the full sleep completes.
  // ─────────────────────────────────────────────────────────────────────

  struct timespec req;
  req.tv_sec = (time_t)seconds;
  req.tv_nsec = (long)((seconds - (real64)req.tv_sec) * 1000000000.0);

  // Handle signal interruption
  while (nanosleep(&req, &req) == -1 && errno == EINTR) {
    // Continue sleeping with remaining time
  }

#else
#error "platform_sleep_seconds() not implemented for this platform"

#endif
}

void platform_sleep_ms(uint32 milliseconds) {
  if (milliseconds == 0) {
    return;
  }
  platform_sleep_seconds((real64)milliseconds / 1000.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// GET TIMESPEC
// ═══════════════════════════════════════════════════════════════════════════

void platform_get_timespec(PlatformTimeSpec *out_time) {
  if (!out_time) {
    return;
  }

#if defined(_WIN32)
  // ─────────────────────────────────────────────────────────────────────
  // WINDOWS: Convert QPC to seconds + nanoseconds
  // ─────────────────────────────────────────────────────────────────────

  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);

  int64 frequency = win32_get_frequency();

  out_time->seconds = (int64)(counter.QuadPart / frequency);
  int64 remainder = counter.QuadPart % frequency;
  out_time->nanoseconds = (int64)(remainder * 1000000000 / frequency);

#elif defined(__APPLE__)
  // ─────────────────────────────────────────────────────────────────────
  // MACOS: Convert mach_absolute_time to seconds + nanoseconds
  // ─────────────────────────────────────────────────────────────────────

  uint64 time = mach_absolute_time();
  CachedTimebase tb = macos_get_timebase();

  uint64 nanos = time * tb.numer / tb.denom;

  out_time->seconds = (int64)(nanos / 1000000000ULL);
  out_time->nanoseconds = (int64)(nanos % 1000000000ULL);

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__unix__)
  // ─────────────────────────────────────────────────────────────────────
  // LINUX/BSD: Direct from clock_gettime
  // ─────────────────────────────────────────────────────────────────────

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  out_time->seconds = (int64)ts.tv_sec;
  out_time->nanoseconds = (int64)ts.tv_nsec;

#else
#error "platform_get_timespec() not implemented for this platform"
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// TIMESPEC UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

real64 platform_timespec_to_seconds(const PlatformTimeSpec *time) {
  if (!time) {
    return 0.0;
  }
  return (real64)time->seconds + (real64)time->nanoseconds / 1000000000.0;
}

real64 platform_timespec_diff_seconds(const PlatformTimeSpec *start,
                                      const PlatformTimeSpec *end) {
  if (!start || !end) {
    return 0.0;
  }

  // Calculate difference with proper handling of nanosecond underflow
  int64 sec_diff = end->seconds - start->seconds;
  int64 nsec_diff = end->nanoseconds - start->nanoseconds;

  // Handle nanosecond underflow
  if (nsec_diff < 0) {
    sec_diff -= 1;
    nsec_diff += 1000000000;
  }

  return (real64)sec_diff + (real64)nsec_diff / 1000000000.0;
}

int32 platform_timespec_compare(const PlatformTimeSpec *a,
                                const PlatformTimeSpec *b) {
  if (!a || !b) {
    // Treat NULL as "zero time" for comparison purposes
    if (!a && !b)
      return 0;
    if (!a)
      return -1; // NULL < non-NULL
    return 1;    // non-NULL > NULL
  }

  // Compare seconds first
  if (a->seconds < b->seconds)
    return -1;
  if (a->seconds > b->seconds)
    return 1;

  // Seconds equal, compare nanoseconds
  if (a->nanoseconds < b->nanoseconds)
    return -1;
  if (a->nanoseconds > b->nanoseconds)
    return 1;

  return 0;
}
