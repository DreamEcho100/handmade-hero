/* =============================================================================
 * utils/math.h — Math Utilities (Macros + Vec2 Type)
 * =============================================================================
 *
 * WHAT'S IN HERE?
 *   1. Clamp/min/max/abs macros — work on any numeric type
 *   2. Vec2 struct — a 2D vector for positions, velocities, shape vertices
 *   3. Vec2 helpers — add, scale, length, rotate (CW for y-up convention)
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
 * Grouping x and y into a struct makes the code read like maths (v.x, v.y)
 * and lets us pass pairs of floats as a single argument.
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

/* CLAMP — restrict v to the range [lo, hi].                              */
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
     - SpaceObject positions (world-space x, y in game units)
     - SpaceObject velocities (dx, dy per second in game units)
     - Ship and asteroid model vertices (local-space coordinates)
     - Intermediate transformed vertices during rendering           */
typedef struct {
    float x;
    float y;
} Vec2;

/* ══════ Vec2 Inline Helpers ════════════════════════════════════════════════

   `static inline` means:
     • static  — internal linkage, won't collide across translation units
     • inline  — hint to the compiler to paste the code at call sites
                  (avoids function-call overhead for simple 2-op functions) */

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
    extern float sqrtf(float);
    return sqrtf(v.x * v.x + v.y * v.y);
}

/* vec2_rotate — rotate vector v by angle CLOCKWISE (for y-up Cartesian).
 *
 * COORDINATE SYSTEM NOTE:
 * In a y-up Cartesian system (COORD_ORIGIN_LEFT_BOTTOM), "ship nose points
 * up" means the nose is at (0, +1) in local space.  Increasing the angle
 * should rotate the ship CLOCKWISE on screen (standard arcade convention).
 *
 * The CW rotation matrix for y-up is:
 *   [ cos θ   sin θ ] [ vx ]   [ vx·cosθ + vy·sinθ ]
 *   [-sin θ   cos θ ] [ vy ] = [-vx·sinθ + vy·cosθ ]
 *
 * Verification:
 *   nose = (0, +1), angle=0  → (0, 1) ✓  (still pointing up)
 *   nose = (0, +1), angle=π/2 → (+1, 0) ✓  (pointing right — CW from up)
 *
 * NOTE: `cos_a` and `sin_a` are pre-computed by the caller (once per
 * object, not per vertex) since cosf/sinf are expensive.
 *
 * Contrast with CCW (y-down screen convention):
 *   rx = x*cos - y*sin
 *   ry = x*sin + y*cos
 * Do NOT use CCW here — it would rotate the ship backwards in y-up space. */
static inline Vec2 vec2_rotate(Vec2 v, float cos_a, float sin_a) {
    return (Vec2){
         v.x * cos_a + v.y * sin_a,
        -v.x * sin_a + v.y * cos_a
    };
}

#endif /* ASTEROIDS_MATH_H */
