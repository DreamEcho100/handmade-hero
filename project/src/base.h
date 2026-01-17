#ifndef BASE_H
#define BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════
// 🎯 FRAME RATE CONFIGURATION (Day 18)
// ═══════════════════════════════════════════════════════════════

#ifndef HANDMADE_TARGET_FPS
  #define HANDMADE_TARGET_FPS 60  // Default to 60 FPS
#endif

// Convenience macros for common targets
#define FPS_30  30
#define FPS_45  45
#define FPS_60  60
#define FPS_90  90
#define FPS_120 120
#define FPS_144 144
#define FPS_UNLIMITED 0  // No frame limiting (for benchmarking)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif // M_PI

#ifndef M_PI_DOUBLED
#define M_PI_DOUBLED (2.0f * M_PI)
#endif // M_PI_DOUBLED

#ifndef ArraySize
#define ArraySize(Array) (sizeof(Array) / sizeof((Array)[0]))
#endif

#if HANDMADE_SLOW
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) ||       \
    defined(__CYGWIN__) || defined(__BORLANDC__)
#include <intrin.h>
#define DebugTrap() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define DebugTrap() __builtin_trap()
#else
#define DebugTrap()                                                            \
  {                                                                            \
    *(volatile int *)0 = 0;                                                    \
  }
#endif
#define ASSERT(expression)                                                     \
  if (!(expression)) {                                                         \
    DebugTrap();                                                               \
  }
#else
#define ASSERT(expression)
#endif

#define KILOBYTES(value) ((value) * 1024LL)
#define MEGABYTES(value) (KILOBYTES(value) * 1024LL)
#define GIGABYTES(value) (MEGABYTES(value) * 1024LL)
#define TERABYTES(value) (GIGABYTES(value) * 1024LL)

// ═══════════════════════════════════════════════════════════════
// 🎯 FRAME RATE CONFIGURATION (Day 18)
// ═══════════════════════════════════════════════════════════════
// 
// Convenience macros for common targets
#define FPS_30  30
#define FPS_60  60
#define FPS_120 120
#define FPS_144 144
#define FPS_UNLIMITED 0  // No frame limiting (for benchmarking)

// ═══════════════════════════════════════════════════════════════
// 🔊 AUDIO LATENCY CONFIGURATION (Day 19)
// ═══════════════════════════════════════════════════════════════
// Casey's Day 19 change: Audio latency tied to frame rate!
//
// WHAT THIS MEANS:
// - At 30 FPS, 3 frames = 100ms latency
// - At 60 FPS, 3 frames = 50ms latency
// - At 120 FPS, 3 frames = 25ms latency
//
// WHY 3 FRAMES?
// - 1 frame: Too tight, might underrun (clicks/pops)
// - 2 frames: Still risky on slower machines
// - 3 frames: Safe buffer for most systems
// - 4+ frames: Too much input lag
//
// Casey's rule: Start with 3, adjust if needed
// ═══════════════════════════════════════════════════════════════

#ifndef FRAMES_OF_AUDIO_LATENCY
  // 3 causes noticeable ticks/pops/clicks, so we reduce to 2
  #define FRAMES_OF_AUDIO_LATENCY 2 // Default to 2 frames of latency
#endif

#define file_scoped_fn static
#define local_persist_var static
#define file_scoped_global_var static
#define real32 float
#define real64 double

typedef int32_t bool32;

#endif // BASE_H
