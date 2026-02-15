#ifndef DE100_COMMON_BASE_H
#define DE100_COMMON_BASE_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#if (__STDC_VERSION__ >= 202311L)
#define HAS_C23 1
#define HAS_C17 1
#define HAS_C11 1
#elif (__STDC_VERSION__ >= 201710L)
#define HAS_C17 1
#define HAS_C11 1
#elif (__STDC_VERSION__ >= 201112L)
#define HAS_C11 1
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// #if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
// #error "C11 or newer required."
// #endif

#ifndef DE100_IS_GENERIC_POSIX
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||        \
    defined(__unix__) || defined(__MACH__)
#define DE100_IS_GENERIC_POSIX 1
#else
#define DE100_IS_GENERIC_POSIX 0
#endif
#endif

#ifndef DE100_IS_GENERIC_WINDOWS
#if defined(_WIN32)
#define DE100_IS_GENERIC_WINDOWS 1
#else
#define DE100_IS_GENERIC_WINDOWS 0
#endif
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

#if !defined(DE100_NO_ASSERTS)
#include <stdio.h>

#define ASSERT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr,                                                          \
              "ASSERTION FAILED\n"                                             \
              "  Expression: %s\n"                                             \
              "  File: %s\n"                                                   \
              "  Line: %d\n"                                                   \
              "  File:Line: %s:%d\n",                                          \
              #expr, __FILE__, __LINE__, __FILE__, __LINE__);                  \
      fflush(stderr);                                                          \
      DEBUG_BREAK();                                                           \
    }                                                                          \
  } while (0)

#define ASSERT_MSG(expr, fmt, ...)                                             \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr,                                                          \
              "ASSERTION FAILED\n"                                             \
              "  Expression: %s\n"                                             \
              "  Message: " fmt "\n"                                           \
              "  File: %s\n"                                                   \
              "  Line: %d\n"                                                   \
              "  File:Line: %s:%d\n",                                          \
              #expr, __VA_ARGS__, __FILE__, __LINE__, __FILE__, __LINE__);     \
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
#define DEV_ASSERT_MSG(expr, fmt, ...) ASSERT_MSG(expr, fmt, __VA_ARGS__)
#else
#define DEV_DEBUG_BREAK() ((void)0)
#define DEV_ASSERT(expr) ((void)0)
#define DEV_ASSERT_MSG(expr, fmt, ...) ((void)0)
#endif

/* ========================= FRAME RATE CONFIG ========================= */

#define DE100_DEFAULT_TARGET_FPS 60
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

#define de100_file_scoped_fn static
#define local_persist_var static
#define de100_file_scoped_global_var static

typedef float f32;
typedef double f64;

typedef int32_t bool32;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#endif /* DE100_COMMON_BASE_H */
