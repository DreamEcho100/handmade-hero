#ifndef UTILS_MATH_H
#define UTILS_MATH_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/math.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 04 — MIN/MAX/CLAMP/ABS macros; why we avoid <math.h> functions for
 *             simple integer clamping to keep platform dependencies minimal.
 *
 * LESSON 08 — Letterbox scale and offset use MIN/CLAMP.
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* Safe integer min/max — evaluate each argument once. */
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

/* Clamp a value into [lo, hi]. */
#define CLAMP(val, lo, hi)  MIN(MAX((val), (lo)), (hi))

/* Absolute value — works for any numeric type. */
#define ABS(x)  ((x) < 0 ? -(x) : (x))

/* Integer ceil-division: ceil(a / b) without float. b > 0 required. */
#define CEIL_DIV(a, b)  (((a) + (b) - 1) / (b))

#endif /* UTILS_MATH_H */
