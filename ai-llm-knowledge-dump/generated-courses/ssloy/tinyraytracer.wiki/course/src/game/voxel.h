#ifndef GAME_VOXEL_H
#define GAME_VOXEL_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/voxel.h — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 * LESSON 16 — Voxel model rendering (Part 3 of ssloy's wiki, Steps 8-9).
 *
 * A voxel model is a 3D grid of boolean cells packed into a uint32_t
 * bitfield.  Each cell that is "on" is rendered as a small axis-aligned
 * box.  The bunny model is 6×6×4 = 144 cells packed into 5 uint32_t values.
 *
 * PERFORMANCE: The model pre-computes cell centers and a bounding box at
 * init time.  At render time, the bounding box is tested first — if the
 * ray misses it, all ~60 per-cell tests are skipped.  This eliminates
 * the voxel cost for the vast majority of rays.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "ray.h"
#include "vec3.h"
#include <stdint.h>

/* ── Voxel model dimensions (ssloy's bunny) ────────────────────────────── */
#define VOXEL_W 6
#define VOXEL_H 6
#define VOXEL_D 4
#define VOXEL_SIZE 0.10f
#define VOXEL_MAX_CELLS (VOXEL_W * VOXEL_H * VOXEL_D) /* 144 */

/* ── Bunny bitfield (from ssloy Part 3 Step 8) ──────────────────────────
 * 6×6×4 = 144 cells packed into 5 uint32_t values (144 bits → 5 × 32).
 * Each bit = 1 voxel.  bit index = i + j*6 + k*36.
 *
 * The bunny shape, unpacked layer by layer (k=0 front → k=3 back):
 *
 *   k=0 (front):   k=1:           k=2:           k=3 (back):
 *   . . . 1 . .    . . . 1 1 .    . 1 1 1 1 .    . . . . . .
 *   . . 1 . . .    . 1 1 1 1 .    1 1 1 1 1 1    . . . . . .
 *   . . . 1 . .    . 1 1 1 1 1    1 1 1 1 1 1    . . 1 . . .
 *   1 1 . . . .    1 1 1 1 1 1    1 1 1 1 1 1    . . . . . .
 *   . 1 1 . 1 1    1 1 1 1 1 1    . 1 1 1 1 .    . . . . . .
 *   1 1 0 0 0 0    1 1 1 1 1 1    . 1 1 1 1 .    . . . . . .
 *
 * (1 = solid voxel, . = empty.  Viewed from +z looking toward -z.)
 *
 * Bit packing: cell_id = i + j*VOXEL_W + k*VOXEL_W*VOXEL_H
 * Test: BUNNY_BITFIELD[cell_id / 32] & (1u << (cell_id & 31))            */
static const uint32_t BUNNY_BITFIELD[5] = {
    0xc30d0418u, 0x37dff3e0u, 0x7df71e0cu, 0x004183c3u, 0x00000400u};

static inline int voxel_is_solid(int id) {
  if (id < 0 || id >= VOXEL_MAX_CELLS)
    return 0;
  return (BUNNY_BITFIELD[id / 32] & (1u << (id & 31))) != 0;
}

/* ── Pre-computed voxel cell (computed once at scene_init) ───────────── */
typedef struct {
  Vec3 center;    /* world-space center of this cell */
  Vec3 half_size; /* half-extents of the cell box    */
  int cell_id;    /* for palette color lookup         */
} VoxelCell;

/* ── Octant sub-AABB (L22: CPU-OPT spatial acceleration) ──────────────── */
#define VOXEL_OCTANTS 8
typedef struct {
  Vec3 center;
  Vec3 half_size;
  int cell_indices[VOXEL_MAX_CELLS]; /* indices into parent cells[] array */
  int cell_count;
} VoxelOctant;

/* ── Voxel model instance ──────────────────────────────────────────────── */
typedef struct {
  Vec3 position;
  float scale;
  int material_index;

  /* Pre-computed data (populated by voxel_model_init). */
  VoxelCell cells[VOXEL_MAX_CELLS]; /* only first solid_count are valid */
  int solid_count;

  /* Bounding box for early-out test. */
  Vec3 bbox_center;
  Vec3 bbox_half_size;

  /* Octant sub-AABBs for CPU-OPT mode (populated by voxel_model_init). */
  VoxelOctant octants[VOXEL_OCTANTS];
} VoxelModel;

/* Initialize pre-computed cell data and bounding box.
 * Must be called once after setting position/scale/material_index.        */
void voxel_model_init(VoxelModel *model);

/* Per-voxel palette color (from ssloy Part 3 Step 9). */
static inline Vec3 voxel_color_from_id(int cell_id) {
  unsigned int h = (unsigned int)cell_id;
  h = ((h >> 16) ^ h) * 0x45d9f3bu;
  h = ((h >> 16) ^ h) * 0x45d9f3bu;
  h = (h >> 16) ^ h;
  float r = ((h >> 0) & 0xFF) / 255.0f * 0.6f + 0.2f;
  float g = ((h >> 8) & 0xFF) / 255.0f * 0.6f + 0.2f;
  float b = ((h >> 16) & 0xFF) / 255.0f * 0.6f + 0.2f;
  return vec3_make(r, g, b);
}

/* Ray-voxel model intersection with AABB early-out.
 * Tests bounding box first; skips all per-cell tests if the ray misses.   */
int voxel_model_intersect(RtRay ray, const VoxelModel *model, HitRecord *hit,
                          Vec3 *voxel_color_out);

/* L22 CPU-OPT: octant-BVH accelerated intersection.
 * Tests 8 sub-AABBs before per-cell tests; skips empty octants.           */
int voxel_model_intersect_bvh(RtRay ray, const VoxelModel *model,
                              HitRecord *hit, Vec3 *voxel_color_out);

#endif /* GAME_VOXEL_H */
