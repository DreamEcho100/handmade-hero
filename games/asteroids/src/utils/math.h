#ifndef UTILS_MATH_H
#define UTILS_MATH_H

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(val, lo, hi) MIN(MAX((val), (lo)), (hi))
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define CEIL_DIV(a, b) (((a) + (b) - 1) / (b))

static inline int roundf_to_int(float f) {
  return (int)(f + (f > 0 ? 0.5f : -0.5f));
}

#endif // UTILS_MATH_H
