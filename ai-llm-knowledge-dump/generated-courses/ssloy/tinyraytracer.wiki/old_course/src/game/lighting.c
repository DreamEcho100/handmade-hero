/* ═══════════════════════════════════════════════════════════════════════════
 * game/lighting.c — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 * Diffuse + specular + shadows.  Shadow rays toggleable via settings.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "lighting.h"
#include "intersect.h"
#include "ray.h"
#include <math.h>
#include <stddef.h>

#define SHADOW_EPSILON 1e-3f

LightingResult compute_lighting(Vec3 point, Vec3 normal, Vec3 view_dir,
                                const RtMaterial *material,
                                const Scene *scene,
                                const RenderSettings *settings) {
  LightingResult result = {0.0f, 0.0f};

  for (int i = 0; i < scene->light_count; i++) {
    Vec3  light_dir  = vec3_normalize(vec3_sub(scene->lights[i].position, point));
    float light_dist = vec3_length(vec3_sub(scene->lights[i].position, point));

    /* ── Shadows (toggleable via H key) ──────────────────────────────── */
    if (!settings || settings->show_shadows) {
      Vec3 shadow_orig = (vec3_dot(light_dir, normal) < 0.0f)
                          ? vec3_sub(point, vec3_scale(normal, SHADOW_EPSILON))
                          : vec3_add(point, vec3_scale(normal, SHADOW_EPSILON));

      RtRay shadow_ray = { .origin = shadow_orig, .direction = light_dir };
      HitRecord shadow_hit;

      if (scene_intersect(shadow_ray, scene, &shadow_hit, NULL, settings)) {
        if (shadow_hit.t < light_dist) continue;
      }
    }

    /* ── Diffuse ─────────────────────────────────────────────────────── */
    float diff = vec3_dot(light_dir, normal);
    if (diff > 0.0f) {
      result.diffuse_intensity += scene->lights[i].intensity * diff;
    }

    /* ── Specular ────────────────────────────────────────────────────── */
    Vec3 reflect_dir = vec3_reflect(vec3_negate(light_dir), normal);
    float spec = vec3_dot(reflect_dir, view_dir);
    if (spec > 0.0f) {
      result.specular_intensity += powf(spec, material->specular_exponent)
                                   * scene->lights[i].intensity;
    }
  }

  return result;
}
