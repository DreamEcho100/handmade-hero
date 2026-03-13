/* ═══════════════════════════════════════════════════════════════════════════
 * game/voxel.c — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 * LESSON 16 — Voxel model intersection (Part 3 of ssloy's wiki).
 *
 * PERFORMANCE STRATEGY:
 * 1. Pre-compute cell centers + half-sizes once at init (not per ray).
 * 2. Test the model's AABB bounding box before any per-cell tests.
 *    Most rays miss the bunny entirely → 1 box test instead of ~60.
 * 3. Only iterate over solid cells (pre-filtered at init).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "voxel.h"
#include "intersect.h"
#include <float.h>
#include <math.h>

/* ── Pre-compute cell positions and bounding box ─────────────────────── */
void voxel_model_init(VoxelModel *model) {
  float vox_size = VOXEL_SIZE * model->scale;
  float half     = vox_size * 0.5f; /* cubes touch: no gaps, smooth silhouette */

  model->solid_count = 0;

  /* Track min/max for bounding box. */
  Vec3 bb_min = vec3_make( FLT_MAX,  FLT_MAX,  FLT_MAX);
  Vec3 bb_max = vec3_make(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  for (int i = 0; i < VOXEL_W; i++) {
    for (int j = 0; j < VOXEL_H; j++) {
      for (int k = 0; k < VOXEL_D; k++) {
        int cell_id = i + j * VOXEL_W + k * VOXEL_W * VOXEL_H;
        if (!voxel_is_solid(cell_id)) continue;

        Vec3 center = vec3_add(model->position, vec3_make(
          ((float)i - (float)VOXEL_W / 2.0f + 0.5f) * vox_size,
          ((float)j - (float)VOXEL_H / 2.0f + 0.5f) * vox_size,
          (-(float)k + (float)VOXEL_D / 2.0f - 0.5f) * vox_size
        ));

        VoxelCell *c = &model->cells[model->solid_count++];
        c->center    = center;
        c->half_size = vec3_make(half, half, half);
        c->cell_id   = cell_id;

        /* Expand bounding box. */
        if (center.x - half < bb_min.x) bb_min.x = center.x - half;
        if (center.y - half < bb_min.y) bb_min.y = center.y - half;
        if (center.z - half < bb_min.z) bb_min.z = center.z - half;
        if (center.x + half > bb_max.x) bb_max.x = center.x + half;
        if (center.y + half > bb_max.y) bb_max.y = center.y + half;
        if (center.z + half > bb_max.z) bb_max.z = center.z + half;
      }
    }
  }

  /* Store bounding box as center + half_size (same format as Box). */
  model->bbox_center    = vec3_scale(vec3_add(bb_min, bb_max), 0.5f);
  model->bbox_half_size = vec3_scale(vec3_sub(bb_max, bb_min), 0.5f);
}

/* ── AABB test (inline, no HitRecord needed — just yes/no) ──────────── */
static int aabb_test(RtRay ray, Vec3 center, Vec3 half_size) {
  /* Slab method: compute entry/exit t for each axis, check overlap. */
  float tmin = -FLT_MAX;
  float tmax =  FLT_MAX;

  for (int d = 0; d < 3; d++) {
    float dir_d = (&ray.direction.x)[d];
    float org_d = (&ray.origin.x)[d];
    float c_d   = (&center.x)[d];
    float h_d   = (&half_size.x)[d];

    if (fabsf(dir_d) < 1e-7f) {
      /* Ray parallel to slab — miss if origin outside. */
      if (org_d < c_d - h_d || org_d > c_d + h_d) return 0;
    } else {
      float inv = 1.0f / dir_d;
      float t1 = (c_d - h_d - org_d) * inv;
      float t2 = (c_d + h_d - org_d) * inv;
      if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
      if (t1 > tmin) tmin = t1;
      if (t2 < tmax) tmax = t2;
      if (tmin > tmax) return 0;
    }
  }
  return tmax >= 0.0f;
}

/* ── Ray-voxel model intersection ────────────────────────────────────── */
int voxel_model_intersect(RtRay ray, const VoxelModel *model,
                          HitRecord *hit, Vec3 *voxel_color_out) {
  /* OPTIMIZATION: bounding box early-out.
   * If the ray doesn't hit the model's AABB, skip all cell tests.
   * This turns ~60 box tests into 1 for the majority of rays.            */
  if (!aabb_test(ray, model->bbox_center, model->bbox_half_size))
    return 0;

  float best_t = FLT_MAX;
  int   found  = 0;
  int   best_cell_id = -1;

  /* Only iterate pre-computed solid cells (not all 144). */
  for (int s = 0; s < model->solid_count; s++) {
    const VoxelCell *cell = &model->cells[s];

    Box voxel_box = {
      .center    = cell->center,
      .half_size = cell->half_size,
      .material_index = model->material_index,
    };

    HitRecord tmp;
    if (box_intersect(ray, &voxel_box, &tmp) && tmp.t < best_t) {
      best_t       = tmp.t;
      *hit         = tmp;
      best_cell_id = cell->cell_id;
      found        = 1;
    }
  }

  if (found && voxel_color_out) {
    *voxel_color_out = voxel_color_from_id(best_cell_id);
  }

  return found;
}
