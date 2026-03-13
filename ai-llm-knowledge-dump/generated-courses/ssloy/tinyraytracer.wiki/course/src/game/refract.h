#ifndef GAME_REFRACT_H
#define GAME_REFRACT_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/refract.h — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 * LESSON 09 — Snell's law refraction.
 *
 * Snell's law: n1 * sin(theta1) = n2 * sin(theta2)
 * The refracted direction is computed from the incident direction,
 * the surface normal, and the ratio of refractive indices (eta).
 *
 * Inside/outside detection: if dot(I, N) < 0, the ray hits from outside
 * (normal points away from ray). If dot(I, N) > 0, the ray is inside
 * the object — flip the normal and swap the eta ratio.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "vec3.h"
#include <math.h>

static inline Vec3 refract(Vec3 incident, Vec3 normal, float refractive_index) {
  float cos_i = -vec3_dot(incident, normal);
  float eta_i = 1.0f;  /* air */
  float eta_t = refractive_index;

  /* If cos_i < 0, the ray is inside the object — flip everything. */
  if (cos_i < 0.0f) {
    cos_i  = -cos_i;
    normal = vec3_negate(normal);
    float tmp = eta_i;
    eta_i = eta_t;
    eta_t = tmp;
  }

  float eta = eta_i / eta_t;
  float k   = 1.0f - eta * eta * (1.0f - cos_i * cos_i);

  /* Total internal reflection — return zero vector (caller handles). */
  if (k < 0.0f) {
    return vec3_make(0.0f, 0.0f, 0.0f);
  }

  /* Snell's law: refracted = eta * I + (eta * cos_i - sqrt(k)) * N */
  return vec3_add(
    vec3_scale(incident, eta),
    vec3_scale(normal, eta * cos_i - sqrtf(k))
  );
}

#endif /* GAME_REFRACT_H */
