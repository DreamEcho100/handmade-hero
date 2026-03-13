#ifndef GAME_GPU_UPLOAD_H
#define GAME_GPU_UPLOAD_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/gpu_upload.h — TinyRaytracer Course (L24)
 * ═══════════════════════════════════════════════════════════════════════════
 * Packs the Scene struct into flat float/int arrays matching the GLSL
 * uniform layout.  Platform backends call gpu_pack_scene() once at init
 * (for static geometry) and read the feature bitfield each frame.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "scene.h"
#include "settings.h"
#include "voxel.h"
#include <string.h>
#include <stdlib.h>

/* ── Feature bitfield encoding (matches GLSL uFeatures) ───────────────── */
#define GPU_FEAT_VOXELS     (1 << 0)
#define GPU_FEAT_FLOOR      (1 << 1)
#define GPU_FEAT_BOXES      (1 << 2)
#define GPU_FEAT_MESHES     (1 << 3)
#define GPU_FEAT_REFLECT    (1 << 4)
#define GPU_FEAT_REFRACT    (1 << 5)
#define GPU_FEAT_SHADOWS    (1 << 6)
#define GPU_FEAT_ENVMAP     (1 << 7)

typedef struct {
  /* Counts */
  int mat_count, sphere_count, light_count, box_count;
  int voxel_count, mesh_count;
  int features;     /* bitfield from RenderSettings */
  int env_mode;     /* EnvMapMode enum value */

  /* Materials (16 max) */
  float mat_color[16 * 3];
  float mat_albedo[16 * 4];
  float mat_spec[16];
  float mat_ior[16];

  /* Spheres (16 max): packed as vec4(center, radius) */
  float sphere[16 * 4];
  int   sphere_mat[16];

  /* Lights (8 max): packed as vec4(pos, intensity) */
  float light[8 * 4];

  /* Boxes (8 max) */
  float box_center[8 * 3];
  float box_half[8 * 3];
  int   box_mat[8];

  /* Voxel models (4 max) */
  float vox_pos[4 * 3];
  float vox_scale[4];
  int   vox_mat[4];
  float vox_bbox_center[4 * 3];
  float vox_bbox_half[4 * 3];
  int   vox_solid_count[4];
  uint32_t vox_bitfield[20]; /* 5 uint32 per model × 4 models */

  /* Mesh metadata (4 max) */
  float mesh_pos[4 * 3];
  float mesh_bbox_center[4 * 3];
  float mesh_bbox_half[4 * 3];
  int   mesh_mat[4];
  int   mesh_tri_count[4];
  int   mesh_data_offset[4]; /* texel offset into triangle texture */

  /* Mesh triangle texture data (heap-allocated) */
  float *tri_tex_data;  /* RGBA32F texels: 6 per triangle */
  int    tri_tex_width;
  int    tri_tex_total;
} GpuSceneData;

/* Build the feature bitfield from current RenderSettings. */
static inline int gpu_pack_features(const RenderSettings *r) {
  int f = 0;
  if (r->show_voxels)      f |= GPU_FEAT_VOXELS;
  if (r->show_floor)       f |= GPU_FEAT_FLOOR;
  if (r->show_boxes)       f |= GPU_FEAT_BOXES;
  if (r->show_meshes)      f |= GPU_FEAT_MESHES;
  if (r->show_reflections) f |= GPU_FEAT_REFLECT;
  if (r->show_refractions) f |= GPU_FEAT_REFRACT;
  if (r->show_shadows)     f |= GPU_FEAT_SHADOWS;
  if (r->show_envmap)      f |= GPU_FEAT_ENVMAP;
  return f;
}

/* Pack the entire Scene into GpuSceneData for uniform upload. */
static inline void gpu_pack_scene(GpuSceneData *d, const Scene *s,
                                  const RenderSettings *r) {
  memset(d, 0, sizeof(*d));

  /* ── Materials ─────────────────────────────────────────────────────── */
  d->mat_count = s->material_count;
  for (int i = 0; i < s->material_count && i < 16; i++) {
    d->mat_color[i * 3 + 0] = s->materials[i].diffuse_color.x;
    d->mat_color[i * 3 + 1] = s->materials[i].diffuse_color.y;
    d->mat_color[i * 3 + 2] = s->materials[i].diffuse_color.z;
    d->mat_albedo[i * 4 + 0] = s->materials[i].albedo[0];
    d->mat_albedo[i * 4 + 1] = s->materials[i].albedo[1];
    d->mat_albedo[i * 4 + 2] = s->materials[i].albedo[2];
    d->mat_albedo[i * 4 + 3] = s->materials[i].albedo[3];
    d->mat_spec[i] = s->materials[i].specular_exponent;
    d->mat_ior[i] = s->materials[i].refractive_index;
  }

  /* ── Spheres ───────────────────────────────────────────────────────── */
  d->sphere_count = s->sphere_count;
  for (int i = 0; i < s->sphere_count && i < 16; i++) {
    d->sphere[i * 4 + 0] = s->spheres[i].center.x;
    d->sphere[i * 4 + 1] = s->spheres[i].center.y;
    d->sphere[i * 4 + 2] = s->spheres[i].center.z;
    d->sphere[i * 4 + 3] = s->spheres[i].radius;
    d->sphere_mat[i] = s->spheres[i].material_index;
  }

  /* ── Lights ────────────────────────────────────────────────────────── */
  d->light_count = s->light_count;
  for (int i = 0; i < s->light_count && i < 8; i++) {
    d->light[i * 4 + 0] = s->lights[i].position.x;
    d->light[i * 4 + 1] = s->lights[i].position.y;
    d->light[i * 4 + 2] = s->lights[i].position.z;
    d->light[i * 4 + 3] = s->lights[i].intensity;
  }

  /* ── Boxes ─────────────────────────────────────────────────────────── */
  d->box_count = s->box_count;
  for (int i = 0; i < s->box_count && i < 8; i++) {
    d->box_center[i * 3 + 0] = s->boxes[i].center.x;
    d->box_center[i * 3 + 1] = s->boxes[i].center.y;
    d->box_center[i * 3 + 2] = s->boxes[i].center.z;
    d->box_half[i * 3 + 0] = s->boxes[i].half_size.x;
    d->box_half[i * 3 + 1] = s->boxes[i].half_size.y;
    d->box_half[i * 3 + 2] = s->boxes[i].half_size.z;
    d->box_mat[i] = s->boxes[i].material_index;
  }

  /* ── Voxels ────────────────────────────────────────────────────────── */
  d->voxel_count = s->voxel_model_count;
  for (int i = 0; i < s->voxel_model_count && i < 4; i++) {
    const VoxelModel *vm = &s->voxel_models[i];
    d->vox_pos[i * 3 + 0] = vm->position.x;
    d->vox_pos[i * 3 + 1] = vm->position.y;
    d->vox_pos[i * 3 + 2] = vm->position.z;
    d->vox_scale[i] = vm->scale;
    d->vox_mat[i] = vm->material_index;
    d->vox_bbox_center[i * 3 + 0] = vm->bbox_center.x;
    d->vox_bbox_center[i * 3 + 1] = vm->bbox_center.y;
    d->vox_bbox_center[i * 3 + 2] = vm->bbox_center.z;
    d->vox_bbox_half[i * 3 + 0] = vm->bbox_half_size.x;
    d->vox_bbox_half[i * 3 + 1] = vm->bbox_half_size.y;
    d->vox_bbox_half[i * 3 + 2] = vm->bbox_half_size.z;
    d->vox_solid_count[i] = vm->solid_count;
    /* Copy bitfield (same for all bunny instances) */
    for (int w = 0; w < 5; w++)
      d->vox_bitfield[i * 5 + w] = BUNNY_BITFIELD[w];
  }

  /* ── Meshes ────────────────────────────────────────────────────────── */
  d->mesh_count = s->mesh_count;
  int total_tris = 0;
  for (int i = 0; i < s->mesh_count && i < 4; i++) {
    const TriMesh *m = &s->meshes[i];
    d->mesh_pos[i * 3 + 0] = m->position.x;
    d->mesh_pos[i * 3 + 1] = m->position.y;
    d->mesh_pos[i * 3 + 2] = m->position.z;
    d->mesh_bbox_center[i * 3 + 0] = m->bbox_center.x;
    d->mesh_bbox_center[i * 3 + 1] = m->bbox_center.y;
    d->mesh_bbox_center[i * 3 + 2] = m->bbox_center.z;
    d->mesh_bbox_half[i * 3 + 0] = m->bbox_half_size.x;
    d->mesh_bbox_half[i * 3 + 1] = m->bbox_half_size.y;
    d->mesh_bbox_half[i * 3 + 2] = m->bbox_half_size.z;
    d->mesh_mat[i] = m->material_index;
    d->mesh_tri_count[i] = m->tri_count;
    d->mesh_data_offset[i] = total_tris * 6; /* 6 texels per triangle */
    total_tris += m->tri_count;
  }

  /* ── Mesh triangle texture ─────────────────────────────────────────── */
  d->tri_tex_total = total_tris * 6; /* 6 RGBA32F texels per triangle */
  d->tri_tex_width = d->tri_tex_total > 0 ? d->tri_tex_total : 1;
  d->tri_tex_data = NULL;
  if (total_tris > 0) {
    d->tri_tex_data = (float *)malloc((size_t)d->tri_tex_total * 4 * sizeof(float));
    if (d->tri_tex_data) {
      int texel = 0;
      for (int mi = 0; mi < s->mesh_count && mi < 4; mi++) {
        const TriMesh *m = &s->meshes[mi];
        for (int ti = 0; ti < m->tri_count; ti++) {
          const Triangle *tr = &m->triangles[ti];
          /* Texel 0-2: vertex positions */
          float *p = d->tri_tex_data + texel * 4;
          p[0] = tr->v0.x; p[1] = tr->v0.y; p[2] = tr->v0.z; p[3] = 0.0f;
          p += 4;
          p[0] = tr->v1.x; p[1] = tr->v1.y; p[2] = tr->v1.z; p[3] = 0.0f;
          p += 4;
          p[0] = tr->v2.x; p[1] = tr->v2.y; p[2] = tr->v2.z; p[3] = 0.0f;
          p += 4;
          /* Texel 3-5: vertex normals */
          p[0] = tr->n0.x; p[1] = tr->n0.y; p[2] = tr->n0.z; p[3] = 0.0f;
          p += 4;
          p[0] = tr->n1.x; p[1] = tr->n1.y; p[2] = tr->n1.z; p[3] = 0.0f;
          p += 4;
          p[0] = tr->n2.x; p[1] = tr->n2.y; p[2] = tr->n2.z; p[3] = 0.0f;
          texel += 6;
        }
      }
    }
  }

  /* ── Feature toggles + envmap mode ─────────────────────────────────── */
  d->features = gpu_pack_features(r);
  d->env_mode = (int)s->envmap.mode;
}

static inline void gpu_scene_data_free(GpuSceneData *d) {
  if (d->tri_tex_data) {
    free(d->tri_tex_data);
    d->tri_tex_data = NULL;
  }
}

#endif /* GAME_GPU_UPLOAD_H */
