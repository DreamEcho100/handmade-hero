#ifndef GAME_SCENE_H
#define GAME_SCENE_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/scene.h — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 * LESSON 04 — RtMaterial, Sphere, Scene structs + scene_init.
 * LESSON 05 — Light struct added, lights in scene_init.
 * LESSON 13 — Box struct added, boxes in scene_init.
 * LESSON 16 — VoxelModel added, voxel bunny in scene_init.
 * LESSON 18 — EnvMap for background.
 * LESSON 19 — TriMesh for triangle meshes.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "envmap.h"
#include "mesh.h"
#include "vec3.h"
#include "voxel.h"

#define MAX_SPHERES 16
#define MAX_LIGHTS 8
#define MAX_MATERIALS 16
#define MAX_BOXES 8
#define MAX_MESHES 4
#define MAX_VOXEL_MODELS 4

/* LESSON 04 — RtMaterial defines how a surface looks.
 * albedo has 4 weights: [0]=diffuse, [1]=specular, [2]=reflect, [3]=refract.
 * They control how much each lighting component contributes to final color. */
typedef struct {
  Vec3 diffuse_color;
  float albedo[4];         /* diffuse, specular, reflect, refract weights */
  float specular_exponent; /* Phong exponent: higher = tighter highlight  */
  float refractive_index;  /* glass ≈ 1.5, air = 1.0                     */
} RtMaterial;

typedef struct {
  Vec3 center;
  float radius;
  int material_index;
} Sphere;

/* LESSON 05 — Point light with position and intensity. */
typedef struct {
  Vec3 position;
  float intensity;
} Light;

/* LESSON 13 — Axis-aligned box with center and half-extents. */
typedef struct {
  Vec3 center;
  Vec3 half_size;
  int material_index;
} Box;

typedef struct {
  RtMaterial materials[MAX_MATERIALS];
  int material_count;
  Sphere spheres[MAX_SPHERES];
  int sphere_count;
  Light lights[MAX_LIGHTS];
  int light_count;
  Box boxes[MAX_BOXES];
  int box_count;
  VoxelModel voxel_models[MAX_VOXEL_MODELS];
  int voxel_model_count;
  TriMesh meshes[MAX_MESHES];
  int mesh_count;
  EnvMap envmap;
} Scene;

/* scene_init — set up the default scene matching ssloy's tutorial.
 * 4 materials: ivory, red rubber, mirror, glass.
 * 4 spheres positioned in a row.
 * 3 point lights.
 * 1 box (added in L13).                                                   */
static inline void scene_init(Scene *s) {
  s->material_count = 0;
  s->sphere_count = 0;
  s->light_count = 0;
  s->box_count = 0;
  s->voxel_model_count = 0;

  /* ── RtMaterials ─────────────────────────────────────────────────────── */
  /* 0: ivory — diffuse + moderate specular */
  s->materials[s->material_count++] = (RtMaterial){
      .diffuse_color = vec3_make(0.4f, 0.4f, 0.3f),
      .albedo = {0.6f, 0.3f, 0.1f, 0.0f},
      .specular_exponent = 50.0f,
      .refractive_index = 1.0f,
  };
  /* 1: red rubber — mostly diffuse, low specular */
  s->materials[s->material_count++] = (RtMaterial){
      .diffuse_color = vec3_make(0.3f, 0.1f, 0.1f),
      .albedo = {0.9f, 0.1f, 0.0f, 0.0f},
      .specular_exponent = 10.0f,
      .refractive_index = 1.0f,
  };
  /* 2: mirror — almost entirely reflective */
  s->materials[s->material_count++] = (RtMaterial){
      .diffuse_color = vec3_make(1.0f, 1.0f, 1.0f),
      .albedo = {0.0f, 10.0f, 0.8f, 0.0f},
      .specular_exponent = 1425.0f,
      .refractive_index = 1.0f,
  };
  /* 3: glass — refractive + reflective */
  s->materials[s->material_count++] = (RtMaterial){
      .diffuse_color = vec3_make(0.6f, 0.7f, 0.8f),
      .albedo = {0.0f, 0.5f, 0.1f, 0.8f},
      .specular_exponent = 125.0f,
      .refractive_index = 1.5f,
  };
  /* 4: box material — teal, moderate specular */
  s->materials[s->material_count++] = (RtMaterial){
      .diffuse_color = vec3_make(0.2f, 0.5f, 0.5f),
      .albedo = {0.6f, 0.3f, 0.1f, 0.0f},
      .specular_exponent = 50.0f,
      .refractive_index = 1.0f,
  };
  /* 5: voxel material — glossy with moderate reflection for polished look */
  s->materials[s->material_count++] = (RtMaterial){
      .diffuse_color = vec3_make(1.0f, 1.0f, 1.0f), /* overridden by palette */
      .albedo = {0.5f, 0.4f, 0.15f, 0.0f},
      .specular_exponent = 80.0f,
      .refractive_index = 1.0f,
  };

  /* ── Spheres (matching ssloy's layout) ────────────────────────────── */
  s->spheres[s->sphere_count++] = (Sphere){
      .center = vec3_make(-3.0f, 0.0f, -16.0f),
      .radius = 2.0f,
      .material_index = 0, /* ivory */
  };
  s->spheres[s->sphere_count++] = (Sphere){
      .center = vec3_make(-1.0f, -1.5f, -12.0f),
      .radius = 2.0f,
      .material_index = 3, /* glass */
  };
  s->spheres[s->sphere_count++] = (Sphere){
      .center = vec3_make(1.5f, -0.5f, -18.0f),
      .radius = 3.0f,
      .material_index = 1, /* red rubber */
  };
  s->spheres[s->sphere_count++] = (Sphere){
      .center = vec3_make(7.0f, 5.0f, -18.0f),
      .radius = 4.0f,
      .material_index = 2, /* mirror */
  };

  /* ── Lights ────────────────────────────────────────────────────────── */
  s->lights[s->light_count++] = (Light){
      .position = vec3_make(-20.0f, 20.0f, 20.0f),
      .intensity = 1.5f,
  };
  s->lights[s->light_count++] = (Light){
      .position = vec3_make(30.0f, 50.0f, -25.0f),
      .intensity = 1.8f,
  };
  s->lights[s->light_count++] = (Light){
      .position = vec3_make(30.0f, 20.0f, 30.0f),
      .intensity = 1.7f,
  };

  /* ── Boxes (L13) ───────────────────────────────────────────────────── */
  s->boxes[s->box_count++] = (Box){
      .center = vec3_make(3.0f, -2.0f, -14.0f),
      .half_size = vec3_make(1.0f, 1.0f, 1.0f),
      .material_index = 4, /* teal */
  };

  /* ── Voxel Models (L16) ──────────────────────────────────────────── */
  /* Bunny voxel model positioned to the right of the spheres.
   * Scale 8.0 makes the 6×6×4 grid (0.1 base size) large enough
   * to be clearly visible in the scene.                                 */
  s->voxel_models[s->voxel_model_count] = (VoxelModel){
      .position = vec3_make(-6.0f, -1.0f, -10.0f),
      .scale = 8.0f,
      .material_index =
          5, /* glossy voxel material (diffuse overridden by palette) */
  };
  /* Pre-compute cell positions + bounding box for fast intersection. */
  voxel_model_init(&s->voxel_models[s->voxel_model_count]);
  s->voxel_model_count++;

  /* ── Meshes (L19) ────────────────────────────────────────────────────
   * Procedural icosahedron for testing without .obj file.
   * To load a real mesh: mesh_load_obj(&s->meshes[0], "assets/duck.obj") */
  s->mesh_count = 0;
  s->meshes[s->mesh_count] = (TriMesh){
      .position = vec3_make(4.0f, 1.0f, -10.0f),
      .material_index = 0, /* ivory */
  };
  mesh_create_icosahedron(&s->meshes[s->mesh_count],
                          s->meshes[s->mesh_count].position, 1.5f);
  s->mesh_count++;

  /* ── Environment map (L18 + L20) ─────────────────────────────────────
   * Try loading cube map first (6 images); if not found, try equirectangular;
   * fall back to procedural sky if neither exists.                          */
  {
    const char *cubemap_faces[CUBEMAP_NUM_FACES] = {
        "assets/textures/cube/Park3Med/px.jpg", /* +X right  */
        "assets/textures/cube/Park3Med/nx.jpg", /* -X left   */
        "assets/textures/cube/Park3Med/py.jpg", /* +Y up     */
        "assets/textures/cube/Park3Med/ny.jpg", /* -Y down   */
        "assets/textures/cube/Park3Med/pz.jpg", /* +Z front  */
        "assets/textures/cube/Park3Med/nz.jpg", /* -Z back   */
    };
    if (envmap_load_cubemap(&s->envmap, cubemap_faces) != 0) {
      /* Cube map not found — try equirectangular. */
      envmap_load(&s->envmap, "assets/envmap.jpg");
    }
  }
}

#endif /* GAME_SCENE_H */
