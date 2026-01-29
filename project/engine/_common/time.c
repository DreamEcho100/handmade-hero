#include "./time.h"

// Platform-specific includes
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#include <time.h>
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__unix__)
#include <errno.h>
#include <time.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// GET WALL CLOCK TIME
// ═══════════════════════════════════════════════════════════════════════════

real64 get_wall_clock(void) {
#if defined(_WIN32)
  static LARGE_INTEGER frequency = {0};
  static int frequency_initialized = 0;

  if (!frequency_initialized) {
    QueryPerformanceFrequency(&frequency);
    frequency_initialized = 1;
  }

  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);

  return (real64)counter.QuadPart / (real64)frequency.QuadPart;

#elif defined(__APPLE__)
  static mach_timebase_info_data_t timebase = {0};
  static int timebase_initialized = 0;

  if (!timebase_initialized) {
    mach_timebase_info(&timebase);
    timebase_initialized = 1;
  }

  uint64_t time = mach_absolute_time();
  uint64_t nanos = time * timebase.numer / timebase.denom;

  return (real64)nanos / 1000000000.0;

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__unix__)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  return (real64)ts.tv_sec + (real64)ts.tv_nsec / 1000000000.0;

#else
#error "get_wall_clock() not implemented for this platform"
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// GET ELAPSED SECONDS
// ═══════════════════════════════════════════════════════════════════════════

real64 get_seconds_elapsed(real64 start, real64 end) { return end - start; }

// ═══════════════════════════════════════════════════════════════════════════
// SLEEP SECONDS
// ═══════════════════════════════════════════════════════════════════════════

void platform_sleep_seconds(real64 seconds) {
  if (seconds <= 0.0) {
    return;
  }

#if defined(_WIN32)
  DWORD milliseconds = (DWORD)(seconds * 1000.0);
  if (milliseconds > 0) {
    Sleep(milliseconds);
  }

#elif defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) ||      \
    defined(__unix__)
  struct timespec req;
  req.tv_sec = (time_t)seconds;
  req.tv_nsec = (long)((seconds - (real64)req.tv_sec) * 1000000000.0);

  while (nanosleep(&req, &req) == -1 && errno == EINTR) {
    // Continue sleeping if interrupted by signal
  }

#else
#error "platform_sleep_seconds() not implemented for this platform"
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// SLEEP MILLISECONDS
// ═══════════════════════════════════════════════════════════════════════════

void platform_sleep_ms(uint32 milliseconds) {
  platform_sleep_seconds((real64)milliseconds / 1000.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// GET TIMESPEC
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
void platform_get_timespec(PlatformTimeSpec *out_time) {
  if (!out_time) {
    return;
  }

  static LARGE_INTEGER frequency = {0};
  static int frequency_initialized = 0;

  if (!frequency_initialized) {
    QueryPerformanceFrequency(&frequency);
    frequency_initialized = 1;
  }

  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);

  out_time->seconds = (int64)(counter.QuadPart / frequency.QuadPart);
  int64 remainder = counter.QuadPart % frequency.QuadPart;
  out_time->nanoseconds = (int64)(remainder * 1000000000 / frequency.QuadPart);
}

#elif defined(__APPLE__)
void platform_get_timespec(PlatformTimeSpec *out_time) {
  if (!out_time) {
    return;
  }

  static mach_timebase_info_data_t timebase = {0};
  static int timebase_initialized = 0;

  if (!timebase_initialized) {
    mach_timebase_info(&timebase);
    timebase_initialized = 1;
  }

  uint64_t time = mach_absolute_time();
  uint64_t nanos = time * timebase.numer / timebase.denom;

  out_time->seconds = (int64)(nanos / 1000000000ULL);
  out_time->nanoseconds = (int64)(nanos % 1000000000ULL);
}

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__unix__)
void platform_get_timespec(PlatformTimeSpec *out_time) {
  if (!out_time) {
    return;
  }
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  out_time->seconds = ts.tv_sec;
  out_time->nanoseconds = ts.tv_nsec;
}
#endif

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
  real64 start_sec = platform_timespec_to_seconds(start);
  real64 end_sec = platform_timespec_to_seconds(end);
  return end_sec - start_sec;
}
