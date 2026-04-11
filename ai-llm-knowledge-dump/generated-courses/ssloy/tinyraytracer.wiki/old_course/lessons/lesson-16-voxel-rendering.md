# Lesson 16 — Voxel Rendering

> **What you'll build:** By the end of this lesson, a colorful voxel bunny stands in the scene — built from ~60 tiny boxes packed into a 144-bit bitfield, with per-voxel palette colors and an AABB early-out optimization.

## Observable outcome

A small bunny model (6x6x4 grid) appears at position (-6, -1, -10) in the scene. Each voxel cell is a different color from a procedural palette. The bunny receives Phong shading, casts shadows, and appears in reflections. Despite being composed of ~60 individual boxes, the AABB bounding box test keeps the frame rate fast — rays that miss the bunny test only 1 box instead of 60.

## New concepts

- Bitfield packing — 144 voxel cells encoded as 5 `uint32_t` values (144 bits total)
- `voxel_is_solid(id)` — testing a single bit to check if a cell is occupied
- Triple nested loop — iterating over a 3D grid (i, j, k) to build world-space cell positions
- Per-voxel palette — deterministic hash function mapping cell ID to RGB color
- AABB early-out — testing a bounding box before any per-cell tests to skip ~60 box intersections

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/voxel.h` | Created | `BUNNY_BITFIELD`; `voxel_is_solid`; `VoxelCell`/`VoxelModel` structs; `voxel_color_from_id` palette; `voxel_model_init`/`voxel_model_intersect` declarations |
| `game/voxel.c` | Created | `voxel_model_init` (pre-compute cells + bbox); `aabb_test` (slab method); `voxel_model_intersect` (bbox early-out + per-cell loop) |
| `game/scene.h` | Modified | `VoxelModel` array added to `Scene`; bunny instance in `scene_init` |
| `game/intersect.c` | Modified | `scene_intersect` tests voxel models; passes `voxel_color_out` back |
| `game/raytracer.c` | Modified | `cast_ray` uses voxel color to override material diffuse color |
| `game/settings.h` | Modified | `show_voxels` toggle added |

## Background — why this works

### JS analogy

In JavaScript you might represent voxels as a 3D array: `let grid = new Uint8Array(6*6*4)`. In C we go further — packing each cell into a single bit to minimize memory. 144 cells need only 18 bytes (5 `uint32_t` values) instead of 144 bytes. This is overkill for a tiny model but demonstrates the technique used in real voxel engines where grids are millions of cells.

### Bitfield packing (ssloy Part 3, Step 8)

The bunny model is stored as a constant array of 5 `uint32_t` values:

```c
static const uint32_t BUNNY_BITFIELD[5] = {
  0xc30d0418u, 0x37dff3e0u, 0x7df71e0cu, 0x004183c3u, 0x00000400u
};
```

Each bit represents one voxel cell. The cell ID is computed from grid coordinates:

```
cell_id = i + j * VOXEL_W + k * VOXEL_W * VOXEL_H
```

Where `i` is the x-axis (0-5), `j` is the y-axis (0-5), and `k` is the z-axis (0-3). To test if a cell is solid:

```c
static inline int voxel_is_solid(int id) {
  if (id < 0 || id >= VOXEL_MAX_CELLS) return 0;
  return (BUNNY_BITFIELD[id / 32] & (1u << (id & 31))) != 0;
}
```

`id / 32` selects the `uint32_t` word. `id & 31` selects the bit within that word. `1u << (id & 31)` creates a mask with only that bit set. The `&` test checks if the bit is on.

### The bunny shape (from the header comment)

Unpacked layer by layer (k=0 front to k=3 back), viewed from +z looking toward -z:

```
k=0 (front):   k=1:           k=2:           k=3 (back):
. . . 1 . .    . . . 1 1 .    . 1 1 1 1 .    . . . . . .
. . 1 . . .    . 1 1 1 1 .    1 1 1 1 1 1    . . . . . .
. . . 1 . .    . 1 1 1 1 1    1 1 1 1 1 1    . . 1 . . .
1 1 . . . .    1 1 1 1 1 1    1 1 1 1 1 1    . . . . . .
. 1 1 . 1 1    1 1 1 1 1 1    . 1 1 1 1 .    . . . . . .
1 1 0 0 0 0    1 1 1 1 1 1    . 1 1 1 1 .    . . . . . .
```

The front layer (k=0) has the bunny's ears and feet. The middle layers (k=1, k=2) are mostly solid (the body). The back layer (k=3) has only a single voxel (the tail).

### AABB early-out optimization

The voxel model contains ~60 solid cells. Without optimization, every ray would test all 60 boxes — extremely expensive since `box_intersect` is already the most complex intersection function. The solution is a two-level test:

1. **Bounding box test** (cheap): Does the ray hit the model's AABB?
2. **Per-cell test** (expensive): If yes, test each solid cell individually.

For the vast majority of rays (those that miss the small bunny), step 1 returns false and step 2 is skipped entirely. This turns ~60 box tests into 1 for most rays.

### Per-voxel palette (ssloy Part 3, Step 9)

Each voxel cell gets a unique color from a deterministic hash function:

```c
static inline Vec3 voxel_color_from_id(int cell_id) {
  unsigned int h = (unsigned int)cell_id;
  h = ((h >> 16) ^ h) * 0x45d9f3bu;
  h = ((h >> 16) ^ h) * 0x45d9f3bu;
  h = (h >> 16) ^ h;
  float r = ((h >>  0) & 0xFF) / 255.0f * 0.6f + 0.2f;
  float g = ((h >>  8) & 0xFF) / 255.0f * 0.6f + 0.2f;
  float b = ((h >> 16) & 0xFF) / 255.0f * 0.6f + 0.2f;
  return vec3_make(r, g, b);
}
```

The hash (based on the integer hash by Thomas Mueller) distributes cell IDs into visually distinct colors. The `* 0.6 + 0.2` maps the range from [0, 1] to [0.2, 0.8], avoiding colors that are too dark or too bright.

## Code walkthrough

### `game/voxel.h` — key types

```c
#define VOXEL_W 6
#define VOXEL_H 6
#define VOXEL_D 4
#define VOXEL_SIZE 0.10f
#define VOXEL_MAX_CELLS (VOXEL_W * VOXEL_H * VOXEL_D) /* 144 */

/* Pre-computed voxel cell (computed once at scene_init). */
typedef struct {
  Vec3 center;     /* world-space center of this cell */
  Vec3 half_size;  /* half-extents of the cell box    */
  int  cell_id;    /* for palette color lookup         */
} VoxelCell;

typedef struct {
  Vec3  position;
  float scale;
  int   material_index;

  /* Pre-computed data (populated by voxel_model_init). */
  VoxelCell cells[VOXEL_MAX_CELLS]; /* only first solid_count are valid */
  int       solid_count;

  /* Bounding box for early-out test. */
  Vec3 bbox_center;
  Vec3 bbox_half_size;
} VoxelModel;
```

**Key design:** `cells[]` is sized for the maximum possible cells (144) but only `solid_count` entries are valid. This avoids dynamic allocation — the entire `VoxelModel` lives on the stack or in the `Scene` struct.

### `game/voxel.c` — `voxel_model_init` (complete function)

```c
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
```

**Key lines:**
- Line 2: `VOXEL_SIZE * model->scale` — base voxel size (0.1) times model scale (8.0) gives 0.8 world units per voxel.
- Line 3: `half = vox_size * 0.5f` — cubes touch exactly (no gaps between adjacent voxels), creating a smooth silhouette.
- Lines 17-20: Cell center computation. The grid is centered on `model->position`. The `- VOXEL_W/2 + 0.5` offset centers each cell, and the negative k on z makes the front layer face the camera.
- Lines 29-34: Bounding box expansion — track the min/max corner of all solid cells to build a tight AABB.

### `game/voxel.c` — `aabb_test` (slab method)

```c
static int aabb_test(RtRay ray, Vec3 center, Vec3 half_size) {
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
```

This is the classic **slab method**: compute the entry/exit `t` range for each axis, check if the ranges overlap. If at any point `tmin > tmax`, the ray misses. Unlike `box_intersect` (which returns a full `HitRecord`), this is a boolean-only test — we just need yes/no to decide whether to test individual cells.

### `game/voxel.c` — `voxel_model_intersect` (complete function)

```c
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
```

**Key lines:**
- Line 7: AABB early-out — the single most important optimization in the file.
- Line 15: Only iterates `solid_count` cells (pre-filtered), not all 144.
- Line 17-21: Creates a temporary `Box` for each cell to reuse `box_intersect`. This avoids duplicating the per-face intersection code.
- Line 32-34: The winning cell's ID is passed to `voxel_color_from_id` to get the palette color, which is returned via `voxel_color_out`.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Bunny is invisible | `voxel_model_init` not called before rendering | Call `voxel_model_init(&model)` after setting position/scale in `scene_init` |
| All voxels are the same color | Not passing `voxel_color_out` through `scene_intersect` to `cast_ray` | Check that `scene_intersect` populates `voxel_color_out` and `cast_ray` uses it |
| Bunny appears but no shadows/reflections | Voxels not tested in shadow ray's `scene_intersect` | The same `scene_intersect` is used for shadow rays — ensure `show_voxels` is on |
| Bunny is upside down or mirrored | Grid coordinate mapping inverted | Check the `-(float)k` negation on the z-axis in `voxel_model_init` |
| Huge performance drop | Missing AABB early-out in `voxel_model_intersect` | The `aabb_test` call must come first, before the per-cell loop |
| Bit test always returns 0 | Using `1 << bit` instead of `1u << bit` | Signed shift can produce undefined behavior for bit 31; use `1u <<` |

## Exercise

> Add a second voxel model: a 4x4x4 solid cube (all 64 bits set). Position it at `(6, 0, -14)` with scale 4.0. You will need to add a second bitfield array and modify `voxel_is_solid` to accept a bitfield pointer, or simply create a second set of constants.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `new Uint8Array(144)` for voxel grid | `uint32_t BUNNY_BITFIELD[5]` — 144 bits in 5 words | 1 bit per cell vs 1 byte; 18 bytes total vs 144 |
| `grid[id] !== 0` | `BUNNY_BITFIELD[id/32] & (1u << (id & 31))` | Bitwise test; must mask and shift |
| `for (let i...) for (let j...) for (let k...)` | Same triple loop | Identical logic; C version pre-computes at init time |
| `new THREE.BoxGeometry(...)` per voxel | `Box voxel_box = { .center, .half_size, ... }` | Temporary stack-allocated box; reuses `box_intersect` |
| BVH / octree acceleration | `aabb_test` bounding box early-out | Simple but effective; a BVH would be needed for thousands of voxels |
