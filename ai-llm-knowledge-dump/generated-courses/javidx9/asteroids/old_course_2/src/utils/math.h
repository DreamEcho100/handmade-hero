/* =============================================================================
 * utils/math.h — Maths Utilities (Macros + Vec2 Type)
 * =============================================================================
 *
 * WHAT'S IN HERE?
 *   1. Clamp/min/max/abs macros — work on any numeric type
 *   2. Vec2 struct — a 2D vector for positions, velocities, shape vertices
 *   3. Vec2 rotation helper — used to spin ship and asteroid wireframes
 *
 * WHY MACROS FOR MIN/MAX INSTEAD OF FUNCTIONS?
 * ─────────────────────────────────────────────
 * C (before C11) has no generics or function overloading.  A function named
 * `min_int` only works with `int`, while a macro works with any numeric type.
 * Trade-off: macros have no type safety and evaluate arguments twice, so
 * never pass side-effectful expressions like `MIN(i++, j++)`.
 *
 * WHY A Vec2 STRUCT?
 * ──────────────────
 * Asteroids is a vector-based game: the ship and all asteroids are defined
 * as lists of 2D vertices that get rotated and scaled each frame.
 * =============================================================================
 */

#ifndef UTILS_MATH_H
#define UTILS_MATH_H

#include <math.h>  /* sqrtf */

/* ── Scalar macros ───────────────────────────────────────────────────────── */
#ifndef MIN
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#endif
#ifndef CLAMP
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#endif
#ifndef ABS
#define ABS(v) ((v) < 0 ? -(v) : (v))
#endif

/* ── Vec2 — 2D float vector ──────────────────────────────────────────────── */
typedef struct {
  float x;
  float y;
} Vec2;

static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
  return (Vec2){ a.x + b.x, a.y + b.y };
}

static inline Vec2 vec2_scale(Vec2 v, float s) {
  return (Vec2){ v.x * s, v.y * s };
}

static inline float vec2_length(Vec2 v) {
  return sqrtf(v.x * v.x + v.y * v.y);
}

/* vec2_rotate — rotate v using pre-computed (cos_a, sin_a).
 * Standard 2D counterclockwise rotation matrix:
 *   [cos -sin] [x]   [x·cos - y·sin]
 *   [sin  cos] [y] = [x·sin + y·cos]
 * Pre-computing cos/sin amortises trig cost over all vertices of one object. */
static inline Vec2 vec2_rotate(Vec2 v, float cos_a, float sin_a) {
  return (Vec2){
    v.x * cos_a - v.y * sin_a,
    v.x * sin_a + v.y * cos_a
  };
}

#endif /* UTILS_MATH_H */
