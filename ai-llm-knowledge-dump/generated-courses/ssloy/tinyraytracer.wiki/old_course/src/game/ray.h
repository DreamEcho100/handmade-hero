#ifndef GAME_RAY_H
#define GAME_RAY_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/ray.h — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 * LESSON 03 — RtRay and HitRecord structs.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "vec3.h"

typedef struct {
  Vec3  origin;
  Vec3  direction;
} RtRay;

/* HitRecord stores the result of a ray-object intersection test.
 * material_index is an index into Scene.materials[].                      */
typedef struct {
  float t;              /* distance along ray to hit point                */
  Vec3  point;          /* world-space hit position                       */
  Vec3  normal;         /* surface normal at hit (outward-facing)         */
  int   material_index; /* index into Scene.materials[]                   */
} HitRecord;

static inline Vec3 ray_at(RtRay r, float t) {
  return vec3_add(r.origin, vec3_scale(r.direction, t));
}

#endif /* GAME_RAY_H */
