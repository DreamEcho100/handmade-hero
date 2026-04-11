#ifndef UTILS_VEC3_H
#define UTILS_VEC3_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/vec3.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 3D vector math used by 3D game courses (TinyRaytracer, future 3D games).
 * Include this header when your game needs Vec3 operations.
 *
 * All functions are static inline — called per-pixel (millions of times per
 * frame) so the compiler inlines them at the call site with no overhead.
 *
 * JS analogy: In JS you'd write `class Vec3 { add(other) { ... } }` with
 * methods.  C has no methods or operator overloading — every operation is
 * an explicit function: vec3_add(a, b) instead of a + b or a.add(b).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <math.h>

/* ── Vec3 struct ────────────────────────────────────────────────────────── */
typedef struct {
  float x, y, z;
} Vec3;

/* ── Constructor + basic arithmetic ─────────────────────────────────────
 * vec3_make uses a C99 compound literal: (Vec3){x, y, z}.
 * JS equivalent: `new Vec3(x, y, z)` or `{ x, y, z }`.                  */

static inline Vec3 vec3_make(float x, float y, float z) {
  return (Vec3){x, y, z};
}

static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
  return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
  return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline Vec3 vec3_scale(Vec3 v, float s) {
  return (Vec3){v.x * s, v.y * s, v.z * s};
}

static inline Vec3 vec3_negate(Vec3 v) {
  return (Vec3){-v.x, -v.y, -v.z};
}

/* Component-wise multiply (used for color blending: material * light). */
static inline Vec3 vec3_mul(Vec3 a, Vec3 b) {
  return (Vec3){a.x * b.x, a.y * b.y, a.z * b.z};
}

/* ── Dot product, length, normalize ─────────────────────────────────────
 * vec3_dot returns a scalar: a·b = ax*bx + ay*by + az*bz.
 * Used for lighting (cos of angle), intersection tests, and more.       */

static inline float vec3_dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float vec3_length(Vec3 v) {
  return sqrtf(vec3_dot(v, v));
}

static inline Vec3 vec3_normalize(Vec3 v) {
  float len = vec3_length(v);
  if (len > 0.0f) {
    float inv = 1.0f / len;
    return (Vec3){v.x * inv, v.y * inv, v.z * inv};
  }
  return v;
}

/* ── Reflect ─────────────────────────────────────────────────────────────
 * reflect(I, N) = I - 2*dot(I,N)*N
 * Given incident direction I and surface normal N, mirrors I across N.  */
static inline Vec3 vec3_reflect(Vec3 incident, Vec3 normal) {
  return vec3_sub(incident, vec3_scale(normal, 2.0f * vec3_dot(incident, normal)));
}

/* ── Cross product ───────────────────────────────────────────────────────
 * cross(a, b) produces a vector perpendicular to both a and b.
 * Used for computing the camera "right" vector from forward + world-up.  */
static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
  return (Vec3){
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x,
  };
}

/* ── Linear interpolation ────────────────────────────────────────────────
 * lerp(a, b, t) = a + t*(b - a).  t=0 → a, t=1 → b.                    */
static inline Vec3 vec3_lerp(Vec3 a, Vec3 b, float t) {
  return vec3_add(a, vec3_scale(vec3_sub(b, a), t));
}

#endif /* UTILS_VEC3_H */
