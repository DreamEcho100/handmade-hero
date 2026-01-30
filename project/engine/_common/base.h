#ifndef DE100_COMMON_BASE_H
#define DE100_COMMON_BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #define DE100_INTERNAL 0

#ifndef DE100_POSIX
#define DE100_POSIX                                                            \
  defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||          \
      defined(__unix__) || defined(__MACH__)
#endif

/* ========================= DEBUG BREAK ========================= */

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) ||       \
    defined(__CYGWIN__) || defined(__BORLANDC__)
#include <intrin.h>
#define DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#define DEBUG_BREAK() __builtin_trap()
#else
#define DEBUG_BREAK() abort()
#endif

/* ========================= ASSERTS ========================= */

#if !defined(NDEBUG)

#define ASSERT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr,                                                          \
              "ASSERTION FAILED\n"                                             \
              "  Expression: %s\n"                                             \
              "  File: %s\n"                                                   \
              "  Line: %d\n",                                                  \
              #expr, __FILE__, __LINE__);                                      \
      fflush(stderr);                                                          \
      DEBUG_BREAK();                                                           \
    }                                                                          \
  } while (0)

#define ASSERT_MSG(expr, msg)                                                  \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr,                                                          \
              "ASSERTION FAILED\n"                                             \
              "  Expression: %s\n"                                             \
              "  Message: %s\n"                                                \
              "  File: %s\n"                                                   \
              "  Line: %d\n",                                                  \
              #expr, msg, __FILE__, __LINE__);                                 \
      fflush(stderr);                                                          \
      DEBUG_BREAK();                                                           \
    }                                                                          \
  } while (0)

#else
#define ASSERT(expr) ((void)0)
#define ASSERT_MSG(e, msg) ((void)0)
#endif /* NDEBUG */

/* ========================= DEV / SLOW ASSERTS ========================= */

#if DE100_SLOW
#define DEV_DEBUG_BREAK() DEBUG_BREAK()
#define DEV_ASSERT(expr) ASSERT(expr)
#define DEV_ASSERT_MSG(expr, msg) ASSERT_MSG(expr, msg)
#else
#define DEV_DEBUG_BREAK() ((void)0)
#define DEV_ASSERT(expr) ((void)0)
#define DEV_ASSERT_MSG(e, msg) ((void)0)
#endif

/* ========================= FRAME RATE CONFIG ========================= */

#ifndef DE100_TARGET_FPS
#define DE100_TARGET_FPS 60
#endif

#define FPS_30 30
#define FPS_45 45
#define FPS_60 60
#define FPS_90 90
#define FPS_120 120
#define FPS_144 144
#define FPS_UNLIMITED 0

/* ========================= MATH ========================= */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define M_PI_DOUBLED (2.0 * M_PI)

/* ========================= UTILITIES ========================= */

#define ArraySize(Array) (sizeof(Array) / sizeof((Array)[0]))

#define KILOBYTES(value) ((value) * 1024LL)
#define MEGABYTES(value) (KILOBYTES(value) * 1024LL)
#define GIGABYTES(value) (MEGABYTES(value) * 1024LL)
#define TERABYTES(value) (GIGABYTES(value) * 1024LL)

/* ========================= AUDIO ========================= */

#ifndef FRAMES_OF_AUDIO_LATENCY
#define FRAMES_OF_AUDIO_LATENCY 2
#endif

/* ========================= ENGINE STYLE ========================= */

#define file_scoped_fn static
#define local_persist_var static
#define file_scoped_global_var static

typedef float real32;
typedef double real64;

typedef int32_t bool32;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#endif /* DE100_COMMON_BASE_H */
