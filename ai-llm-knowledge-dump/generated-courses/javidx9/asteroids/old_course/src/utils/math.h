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
 * JS equivalent:
 *   Math.min, Math.max, Math.abs — built-in, generic, no trade-offs.
 *
 * WHY A Vec2 STRUCT?
 * ──────────────────
 * Asteroids is a vector-based game: the ship and all asteroids are defined
 * as lists of 2D vertices that get rotated and scaled each frame.
 * Grouping x and y into a struct makes the code read like maths (v.x, v.y)
 * and lets us pass pairs of floats as a single argument.
 *
 * JS equivalent:
 *   interface Vec2 { x: number; y: number; }
 *   const add = (a: Vec2, b: Vec2): Vec2 => ({ x: a.x + b.x, y: a.y + b.y });
 * =============================================================================
 */

#ifndef ASTEROIDS_MATH_H
#define ASTEROIDS_MATH_H

/* ══════ Scalar Macros ══════════════════════════════════════════════════════ */

/* MIN/MAX — return the smaller/larger of two values.
   WARNING: each argument is evaluated TWICE.
   Safe:    MIN(a, b)      (variables only)
   Unsafe:  MIN(a++, b)    (increments a twice!)                           */
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* CLAMP — restrict v to the range [lo, hi].
   Equivalent to Math.max(lo, Math.min(hi, v)) in JS.                    */
#ifndef CLAMP
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#endif

/* ABS — absolute value for any numeric type.
   For floats, prefer fabsf() (from <math.h>) to avoid implicit conversion. */
#ifndef ABS
#define ABS(v) ((v) < 0 ? -(v) : (v))
#endif

/* ══════ Vec2 — 2D Float Vector ════════════════════════════════════════════

   Used for:
     - SpaceObject positions (world-space x, y)
     - SpaceObject velocities (dx, dy per second)
     - Ship and asteroid model vertices (local-space coordinates)
     - Intermediate transformed vertices during rendering

   COURSE NOTE: In later lessons we use designated initialisers:
     Vec2 v = { .x = 1.0f, .y = 0.0f };
   This is a C99 feature; in older C89 you'd write { 1.0f, 0.0f }.       */
typedef struct {
    float x;
    float y;
} Vec2;

/* ══════ Vec2 Inline Helpers ════════════════════════════════════════════════

   `static inline` means:
     • static  — internal linkage, won't collide across translation units
     • inline  — hint to the compiler to paste the code at call sites
                  (avoids function-call overhead for simple 2-op functions)

   JS equivalent:  const add = (a, b) => ({ x: a.x+b.x, y: a.y+b.y });  */

/* vec2_add — translate point a by vector b.
   Used to move a vertex from local space to world space.                  */
static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
    return (Vec2){ a.x + b.x, a.y + b.y };
}

/* vec2_scale — multiply both components by scalar s.
   Used to resize an asteroid wireframe to large/medium/small.            */
static inline Vec2 vec2_scale(Vec2 v, float s) {
    return (Vec2){ v.x * s, v.y * s };
}

/* vec2_length — Euclidean distance from origin to v.
   Used for bullet-to-asteroid collision detection.                       */
static inline float vec2_length(Vec2 v) {
    /* #include <math.h> is expected in any file using this function.     */
    extern float sqrtf(float);
    return sqrtf(v.x * v.x + v.y * v.y);
}

/* vec2_rotate — rotate vector v by angle_rad radians counter-clockwise.
 *
 * This is the 2×2 rotation matrix multiplied out:
 *   [ cos θ  -sin θ ] [ vx ]   [ vx·cosθ - vy·sinθ ]
 *   [ sin θ   cos θ ] [ vy ] = [ vx·sinθ + vy·cosθ ]
 *
 * JS equivalent:
 *   const rotated = {
 *     x: v.x * Math.cos(a) - v.y * Math.sin(a),
 *     y: v.x * Math.sin(a) + v.y * Math.cos(a)
 *   };
 *
 * PERFORMANCE NOTE: The caller should compute cosf/sinf once per object
 * and pass the results, not call vec2_rotate in a tight loop with the
 * raw angle — computing trig is expensive.                               */
static inline Vec2 vec2_rotate(Vec2 v, float cos_a, float sin_a) {
    return (Vec2){
        v.x * cos_a - v.y * sin_a,
        v.x * sin_a + v.y * cos_a
    };
}

#endif /* ASTEROIDS_MATH_H */
