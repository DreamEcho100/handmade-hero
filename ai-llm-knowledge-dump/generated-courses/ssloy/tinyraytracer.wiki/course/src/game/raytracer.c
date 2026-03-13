/* ═══════════════════════════════════════════════════════════════════════════
 * game/raytracer.c — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 * Recursive cast_ray with reflections, refractions, checkerboard, voxels.
 * All features toggleable via RenderSettings.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "raytracer.h"
#include "intersect.h"
#include "lighting.h"
#include "refract.h"
#include "envmap.h"
#include <math.h>
#include <stddef.h>

Vec3 cast_ray(RtRay ray, const Scene *scene, int depth,
              const RenderSettings *settings) {
  HitRecord hit;
  Vec3 voxel_color;

  if (depth > MAX_RECURSION_DEPTH ||
      !scene_intersect(ray, scene, &hit, &voxel_color, settings)) {
    /* L18 — Sample environment map (image or procedural sky). */
    if (settings && settings->show_envmap)
      return envmap_sample(&scene->envmap, ray.direction);
    /* Fallback: simple gradient when envmap is toggled off. */
    float t = 0.5f * (ray.direction.y + 1.0f);
    return vec3_lerp(vec3_make(0.2f, 0.7f, 0.8f),
                     vec3_make(1.0f, 1.0f, 1.0f), t * 0.3f);
  }

  /* ── Determine material ──────────────────────────────────────────────── */
  RtMaterial mat;
  if (hit.material_index < 0) {
    /* Checkerboard floor. */
    int checker = ((int)floorf(hit.point.x) + (int)floorf(hit.point.z)) & 1;
    mat = (RtMaterial){
      .diffuse_color     = checker ? vec3_make(0.3f, 0.3f, 0.3f)
                                   : vec3_make(0.3f, 0.2f, 0.1f),
      .albedo            = {1.0f, 0.0f, 0.0f, 0.0f},
      .specular_exponent = 1.0f,
      .refractive_index  = 1.0f,
    };
  } else {
    mat = scene->materials[hit.material_index];
    /* Voxel per-cell palette override. */
    if (voxel_color.x >= 0.0f) {
      mat.diffuse_color = voxel_color;
    }
  }

  /* ── Lighting (diffuse + specular + shadows) ─────────────────────────── */
  Vec3 view_dir = vec3_negate(ray.direction);
  LightingResult lr = compute_lighting(hit.point, hit.normal, view_dir,
                                       &mat, scene, settings);

  Vec3 diffuse_color  = vec3_scale(mat.diffuse_color, lr.diffuse_intensity);
  Vec3 specular_color = vec3_make(lr.specular_intensity,
                                   lr.specular_intensity,
                                   lr.specular_intensity);

  /* ── Reflections (toggleable via R key) ──────────────────────────────── */
  Vec3 reflect_color = vec3_make(0.0f, 0.0f, 0.0f);
  if (mat.albedo[2] > 0.0f && (!settings || settings->show_reflections)) {
    Vec3 reflect_dir = vec3_normalize(vec3_reflect(ray.direction, hit.normal));
    Vec3 reflect_orig = (vec3_dot(reflect_dir, hit.normal) < 0.0f)
                          ? vec3_sub(hit.point, vec3_scale(hit.normal, 1e-3f))
                          : vec3_add(hit.point, vec3_scale(hit.normal, 1e-3f));
    RtRay reflect_ray = { .origin = reflect_orig, .direction = reflect_dir };
    reflect_color = cast_ray(reflect_ray, scene, depth + 1, settings);
  }

  /* ── Refractions (toggleable via T key) ──────────────────────────────── */
  Vec3 refract_color = vec3_make(0.0f, 0.0f, 0.0f);
  if (mat.albedo[3] > 0.0f && (!settings || settings->show_refractions)) {
    Vec3 refract_dir = vec3_normalize(refract(ray.direction, hit.normal,
                                              mat.refractive_index));
    Vec3 refract_orig = (vec3_dot(refract_dir, hit.normal) < 0.0f)
                          ? vec3_sub(hit.point, vec3_scale(hit.normal, 1e-3f))
                          : vec3_add(hit.point, vec3_scale(hit.normal, 1e-3f));
    RtRay refract_ray = { .origin = refract_orig, .direction = refract_dir };
    refract_color = cast_ray(refract_ray, scene, depth + 1, settings);
  }

  /* ── Material blending ─────────────────────────────────────────────── */
  return vec3_add(
    vec3_add(vec3_scale(diffuse_color,  mat.albedo[0]),
             vec3_scale(specular_color, mat.albedo[1])),
    vec3_add(vec3_scale(reflect_color,  mat.albedo[2]),
             vec3_scale(refract_color,  mat.albedo[3]))
  );
}
