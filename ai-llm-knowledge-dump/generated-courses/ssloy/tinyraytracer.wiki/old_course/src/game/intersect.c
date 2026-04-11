/* ═══════════════════════════════════════════════════════════════════════════
 * game/intersect.c — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 * LESSON 03 — sphere_intersect (geometric ray-sphere intersection).
 * LESSON 04 — scene_intersect (nearest-hit traversal across all spheres).
 * LESSON 10 — plane_intersect (y=0 checkerboard floor).
 * LESSON 13 — box_intersect (per-face ray-AABB intersection).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "intersect.h"
#include "mesh.h"
#include "voxel.h"
#include <float.h>
#include <math.h>

/* ── LESSON 03 — Geometric ray-sphere intersection ───────────────────────
 * Algorithm (from ssloy):
 *   L = sphere.center - ray.origin
 *   tca = dot(L, ray.direction)       (projection of L onto ray)
 *   d² = dot(L,L) - tca²              (squared distance from center to ray)
 *   if d² > radius² → miss
 *   thc = sqrt(radius² - d²)
 *   t0 = tca - thc, t1 = tca + thc    (two intersection distances)
 *   take the nearest positive t.                                          */
int sphere_intersect(RtRay ray, const Sphere *sphere, HitRecord *hit) {
  Vec3 L = vec3_sub(sphere->center, ray.origin);
  float tca = vec3_dot(L, ray.direction);
  float d2 = vec3_dot(L, L) - tca * tca;
  float r2 = sphere->radius * sphere->radius;

  if (d2 > r2)
    return 0;

  float thc = sqrtf(r2 - d2);
  float t0 = tca - thc;
  float t1 = tca + thc;

  if (t0 < 0.0f)
    t0 = t1;
  if (t0 < 0.0f)
    return 0;

  hit->t = t0;
  hit->point = ray_at(ray, t0);
  /* Surface normal: outward-pointing, normalized. */
  hit->normal = vec3_normalize(vec3_sub(hit->point, sphere->center));
  hit->material_index = sphere->material_index;
  return 1;
}

/* ── LESSON 10 — Ray-plane intersection (y = 0 floor) ───────────────────
 * The floor is the infinite plane y = 0.
 * t = -origin.y / dir.y.  Only accept hits in front of the camera
 * and within a reasonable distance (avoid rendering to infinity).         */
int plane_intersect(RtRay ray, HitRecord *hit) {
  /* Avoid division by zero when ray is parallel to the floor. */
  if (fabsf(ray.direction.y) < 1e-5f)
    return 0;

  float t = -ray.origin.y / ray.direction.y;
  if (t < 0.001f)
    return 0; /* behind camera */
  if (t > 1000.0f)
    return 0; /* too far away  */

  Vec3 pt = ray_at(ray, t);

  /* Limit the floor to a reasonable area to avoid floating-point noise. */
  if (fabsf(pt.x) > 30.0f || fabsf(pt.z) > 50.0f)
    return 0;

  hit->t = t;
  hit->point = pt;
  hit->normal = vec3_make(0.0f, 1.0f, 0.0f); /* floor normal points up */
  hit->material_index = -1; /* special: checkerboard handled in cast_ray */
  return 1;
}

/* ── LESSON 13 — Per-face ray-AABB intersection ─────────────────────────
 * Algorithm (from ssloy Part 3): check each of the 3 axis pairs.
 * For each axis d, compute the intersection with the near face.
 * If the hit point is within the other two axes' bounds, it's a hit.
 *
 * This is simpler (and more intuitive) than the slab method:
 * we check each face individually rather than computing overlapping ranges. */
int box_intersect(RtRay ray, const Box *box, HitRecord *hit) {
  float best_t = FLT_MAX;
  int found = 0;

  for (int d = 0; d < 3; d++) {
    /* Access Vec3 components by index: 0=x, 1=y, 2=z. */
    float dir_d = (&ray.direction.x)[d];
    float orig_d = (&ray.origin.x)[d];
    float center_d = (&box->center.x)[d];
    float half_d = (&box->half_size.x)[d];

    if (fabsf(dir_d) < 1e-5f)
      continue; /* ray parallel to this face pair */

    /* Check both faces on this axis (near and far). */
    for (int side = -1; side <= 1; side += 2) {
      float face_d = center_d + (float)side * half_d;
      float t = (face_d - orig_d) / dir_d;
      if (t < 0.001f || t >= best_t)
        continue;

      Vec3 pt = ray_at(ray, t);

      /* Check if hit point is within the other two axes' bounds. */
      int d1 = (d + 1) % 3;
      int d2 = (d + 2) % 3;
      float p1 = (&pt.x)[d1];
      float p2 = (&pt.x)[d2];
      float c1 = (&box->center.x)[d1];
      float c2 = (&box->center.x)[d2];
      float h1 = (&box->half_size.x)[d1];
      float h2 = (&box->half_size.x)[d2];

      if (p1 >= c1 - h1 && p1 <= c1 + h1 && p2 >= c2 - h2 && p2 <= c2 + h2) {
        best_t = t;
        hit->t = t;
        hit->point = pt;
        /* Axis-aligned normal: unit vector on the hit axis × sign. */
        hit->normal = vec3_make(0.0f, 0.0f, 0.0f);
        (&hit->normal.x)[d] = (float)side;
        hit->material_index = box->material_index;
        found = 1;
      }
    }
  }

  return found;
}

/* ── LESSON 04 — Nearest-hit traversal ──────────────────────────────────
 * Test ray against all spheres, boxes, and the floor plane.
 * Keep the closest hit (smallest t > 0).                                  */
int scene_intersect(RtRay ray, const Scene *scene, HitRecord *hit,
                    Vec3 *voxel_color_out, const RenderSettings *settings) {
  float nearest = FLT_MAX;
  int found = 0;
  int is_voxel_hit = 0;
  HitRecord tmp;
  Vec3 tmp_voxel_color = vec3_make(0.0f, 0.0f, 0.0f);

  /* Spheres (always on — core geometry) */
  for (int i = 0; i < scene->sphere_count; i++) {
    if (sphere_intersect(ray, &scene->spheres[i], &tmp) && tmp.t < nearest) {
      nearest = tmp.t;
      *hit = tmp;
      found = 1;
      is_voxel_hit = 0;
    }
  }

  /* Boxes (L13) — toggleable via B key */
  if (!settings || settings->show_boxes) {
    for (int i = 0; i < scene->box_count; i++) {
      if (box_intersect(ray, &scene->boxes[i], &tmp) && tmp.t < nearest) {
        nearest = tmp.t;
        *hit = tmp;
        found = 1;
        is_voxel_hit = 0;
      }
    }
  }

  /* Voxel models (L16) — toggleable via V key
   * L22 CPU-OPT: use octant-BVH acceleration instead of brute-force. */
  if (!settings || settings->show_voxels) {
    for (int i = 0; i < scene->voxel_model_count; i++) {
      Vec3 vc;
      int vhit;
      if (settings && settings->render_mode == RENDER_CPU_OPT)
        vhit =
            voxel_model_intersect_bvh(ray, &scene->voxel_models[i], &tmp, &vc);
      else
        vhit = voxel_model_intersect(ray, &scene->voxel_models[i], &tmp, &vc);
      if (vhit && tmp.t < nearest) {
        nearest = tmp.t;
        *hit = tmp;
        found = 1;
        is_voxel_hit = 1;
        tmp_voxel_color = vc;
      }
    }
  }

  /* Triangle meshes (L19) — toggleable via M key */
  if (!settings || settings->show_meshes) {
    for (int i = 0; i < scene->mesh_count; i++) {
      if (mesh_intersect(ray, &scene->meshes[i], &tmp) && tmp.t < nearest) {
        nearest = tmp.t;
        *hit = tmp;
        found = 1;
        is_voxel_hit = 0;
      }
    }
  }

  /* Floor plane (L10) — toggleable via F key */
  if ((!settings || settings->show_floor) && plane_intersect(ray, &tmp) &&
      tmp.t < nearest) {
    nearest = tmp.t;
    *hit = tmp;
    found = 1;
    is_voxel_hit = 0;
  }

  /* Pass back voxel color if the nearest hit was a voxel. */
  if (voxel_color_out) {
    *voxel_color_out =
        is_voxel_hit ? tmp_voxel_color : vec3_make(-1.0f, -1.0f, -1.0f);
  }

  return found;
}
