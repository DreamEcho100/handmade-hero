/* ═══════════════════════════════════════════════════════════════════════════
 * game/mesh.c — TinyRaytracer Course (L19)
 * ═══════════════════════════════════════════════════════════════════════════
 * Triangle mesh intersection (Moller-Trumbore) + .obj loading (fast_obj).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "mesh.h"

/* fast_obj is a single-header library; define FAST_OBJ_IMPLEMENTATION
 * before including to get the actual code compiled here.                   */
#define FAST_OBJ_IMPLEMENTATION
#include "../utils/fast_obj.h"

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ── Moller-Trumbore ray-triangle intersection ───────────────────────────
 * The standard CPU algorithm.  Returns 1 on hit, 0 on miss.
 * Cost: 1 cross product, 3 dot products, 1 division per triangle.
 *
 * Algorithm:
 *   edge1 = v1 - v0, edge2 = v2 - v0
 *   h = cross(dir, edge2)
 *   a = dot(edge1, h)          — determinant
 *   if |a| < epsilon → ray parallel to triangle
 *   f = 1/a
 *   s = origin - v0
 *   u = f * dot(s, h)          — first barycentric coord
 *   if u < 0 or u > 1 → miss
 *   q = cross(s, edge1)
 *   v = f * dot(dir, q)        — second barycentric coord
 *   if v < 0 or u+v > 1 → miss
 *   t = f * dot(edge2, q)      — distance along ray
 *   if t > epsilon → hit                                                  */
int triangle_intersect(RtRay ray, const Triangle *tri, HitRecord *hit) {
  Vec3 edge1 = vec3_sub(tri->v1, tri->v0);
  Vec3 edge2 = vec3_sub(tri->v2, tri->v0);

  Vec3  h = vec3_cross(ray.direction, edge2);
  float a = vec3_dot(edge1, h);
  if (fabsf(a) < 1e-7f) return 0; /* parallel */

  float f = 1.0f / a;
  Vec3  s = vec3_sub(ray.origin, tri->v0);
  float u = f * vec3_dot(s, h);
  if (u < 0.0f || u > 1.0f) return 0;

  Vec3  q = vec3_cross(s, edge1);
  float v = f * vec3_dot(ray.direction, q);
  if (v < 0.0f || u + v > 1.0f) return 0;

  float t = f * vec3_dot(edge2, q);
  if (t < 0.001f) return 0; /* behind ray origin */

  hit->t     = t;
  hit->point = ray_at(ray, t);

  /* Interpolate vertex normals using barycentric coordinates
   * for smooth shading (Phong interpolation).
   * w = 1 - u - v (weight for v0), u (for v1), v (for v2).               */
  float w = 1.0f - u - v;
  hit->normal = vec3_normalize(vec3_add(
    vec3_add(vec3_scale(tri->n0, w), vec3_scale(tri->n1, u)),
    vec3_scale(tri->n2, v)
  ));

  return 1;
}

/* ── AABB test (slab method) ─────────────────────────────────────────── */
static int aabb_hit(RtRay ray, Vec3 center, Vec3 half_size) {
  float tmin = -FLT_MAX, tmax = FLT_MAX;
  for (int d = 0; d < 3; d++) {
    float dir_d = (&ray.direction.x)[d];
    float org_d = (&ray.origin.x)[d];
    float c_d   = (&center.x)[d];
    float h_d   = (&half_size.x)[d];
    if (fabsf(dir_d) < 1e-7f) {
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

int mesh_intersect(RtRay ray, const TriMesh *mesh, HitRecord *hit) {
  if (!mesh->triangles || mesh->tri_count == 0) return 0;

  /* AABB early-out — skip all triangles if ray misses the bounding box. */
  if (!aabb_hit(ray, mesh->bbox_center, mesh->bbox_half_size)) return 0;

  float best_t = FLT_MAX;
  int   found  = 0;
  HitRecord tmp;

  for (int i = 0; i < mesh->tri_count; i++) {
    if (triangle_intersect(ray, &mesh->triangles[i], &tmp) && tmp.t < best_t) {
      best_t = tmp.t;
      *hit   = tmp;
      hit->material_index = mesh->material_index;
      found = 1;
    }
  }
  return found;
}

void mesh_compute_bbox(TriMesh *mesh) {
  if (!mesh->triangles || mesh->tri_count == 0) {
    mesh->bbox_center    = mesh->position;
    mesh->bbox_half_size = vec3_make(0.01f, 0.01f, 0.01f);
    return;
  }
  Vec3 mn = mesh->triangles[0].v0;
  Vec3 mx = mn;
  for (int i = 0; i < mesh->tri_count; i++) {
    const Triangle *t = &mesh->triangles[i];
    const Vec3 *verts[3] = { &t->v0, &t->v1, &t->v2 };
    for (int j = 0; j < 3; j++) {
      if (verts[j]->x < mn.x) mn.x = verts[j]->x;
      if (verts[j]->y < mn.y) mn.y = verts[j]->y;
      if (verts[j]->z < mn.z) mn.z = verts[j]->z;
      if (verts[j]->x > mx.x) mx.x = verts[j]->x;
      if (verts[j]->y > mx.y) mx.y = verts[j]->y;
      if (verts[j]->z > mx.z) mx.z = verts[j]->z;
    }
  }
  mesh->bbox_center    = vec3_scale(vec3_add(mn, mx), 0.5f);
  mesh->bbox_half_size = vec3_scale(vec3_sub(mx, mn), 0.5f);
}

/* ── Load .obj via fast_obj ──────────────────────────────────────────── */
int mesh_load_obj(TriMesh *mesh, const char *filename) {
  fastObjMesh *obj = fast_obj_read(filename);
  if (!obj) {
    fprintf(stderr, "mesh_load_obj: cannot open '%s'\n", filename);
    return 0;
  }

  /* Count total triangles (triangulate quads/n-gons). */
  int total_tris = 0;
  for (unsigned int f = 0; f < obj->face_count; f++) {
    unsigned int nv = obj->face_vertices[f];
    if (nv >= 3) total_tris += (int)(nv - 2);
  }

  mesh->triangles = (Triangle *)malloc((size_t)total_tris * sizeof(Triangle));
  if (!mesh->triangles) {
    fast_obj_destroy(obj);
    return 0;
  }

  int ti = 0;
  unsigned int idx_offset = 0;

  for (unsigned int f = 0; f < obj->face_count; f++) {
    unsigned int nv = obj->face_vertices[f];

    /* Get first vertex (v0) for fan triangulation. */
    fastObjIndex i0 = obj->indices[idx_offset];
    Vec3 p0 = vec3_add(mesh->position, vec3_make(
      obj->positions[i0.p * 3 + 0],
      obj->positions[i0.p * 3 + 1],
      obj->positions[i0.p * 3 + 2]));
    Vec3 n0 = (i0.n) ? vec3_make(
      obj->normals[i0.n * 3 + 0],
      obj->normals[i0.n * 3 + 1],
      obj->normals[i0.n * 3 + 2]) : vec3_make(0, 1, 0);

    /* Fan triangulation: (v0, v_{k}, v_{k+1}) for k = 1..nv-2. */
    for (unsigned int k = 1; k + 1 < nv; k++) {
      fastObjIndex i1 = obj->indices[idx_offset + k];
      fastObjIndex i2 = obj->indices[idx_offset + k + 1];

      Vec3 p1 = vec3_add(mesh->position, vec3_make(
        obj->positions[i1.p * 3 + 0],
        obj->positions[i1.p * 3 + 1],
        obj->positions[i1.p * 3 + 2]));
      Vec3 p2 = vec3_add(mesh->position, vec3_make(
        obj->positions[i2.p * 3 + 0],
        obj->positions[i2.p * 3 + 1],
        obj->positions[i2.p * 3 + 2]));

      Vec3 n1 = (i1.n) ? vec3_make(
        obj->normals[i1.n * 3 + 0],
        obj->normals[i1.n * 3 + 1],
        obj->normals[i1.n * 3 + 2]) : vec3_make(0, 1, 0);
      Vec3 n2 = (i2.n) ? vec3_make(
        obj->normals[i2.n * 3 + 0],
        obj->normals[i2.n * 3 + 1],
        obj->normals[i2.n * 3 + 2]) : vec3_make(0, 1, 0);

      mesh->triangles[ti++] = (Triangle){
        .v0 = p0, .v1 = p1, .v2 = p2,
        .n0 = n0, .n1 = n1, .n2 = n2,
      };
    }
    idx_offset += nv;
  }

  mesh->tri_count = ti;
  fast_obj_destroy(obj);

  mesh_compute_bbox(mesh);
  printf("Loaded mesh: %s (%d triangles)\n", filename, ti);
  return ti;
}

void mesh_free(TriMesh *mesh) {
  free(mesh->triangles);
  mesh->triangles = NULL;
  mesh->tri_count = 0;
}

/* ── Procedural icosahedron (no .obj file needed) ────────────────────── */
void mesh_create_icosahedron(TriMesh *mesh, Vec3 center, float radius) {
  /* 12 vertices of a regular icosahedron. */
  float t = (1.0f + sqrtf(5.0f)) / 2.0f; /* golden ratio */
  float s = radius / sqrtf(1.0f + t * t);
  Vec3 v[12] = {
    vec3_add(center, vec3_make(-s,  t*s, 0)),
    vec3_add(center, vec3_make( s,  t*s, 0)),
    vec3_add(center, vec3_make(-s, -t*s, 0)),
    vec3_add(center, vec3_make( s, -t*s, 0)),
    vec3_add(center, vec3_make(0, -s,  t*s)),
    vec3_add(center, vec3_make(0,  s,  t*s)),
    vec3_add(center, vec3_make(0, -s, -t*s)),
    vec3_add(center, vec3_make(0,  s, -t*s)),
    vec3_add(center, vec3_make( t*s, 0, -s)),
    vec3_add(center, vec3_make( t*s, 0,  s)),
    vec3_add(center, vec3_make(-t*s, 0, -s)),
    vec3_add(center, vec3_make(-t*s, 0,  s)),
  };

  /* 20 faces (indices into v[]). */
  static const int faces[20][3] = {
    {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
    {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
    {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
    {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1},
  };

  mesh->tri_count = 20;
  mesh->triangles = (Triangle *)malloc(20 * sizeof(Triangle));
  for (int i = 0; i < 20; i++) {
    Vec3 p0 = v[faces[i][0]];
    Vec3 p1 = v[faces[i][1]];
    Vec3 p2 = v[faces[i][2]];
    /* For an icosahedron centered at `center`, the vertex normal
     * is just the normalized (vertex - center) vector.                    */
    Vec3 n0 = vec3_normalize(vec3_sub(p0, center));
    Vec3 n1 = vec3_normalize(vec3_sub(p1, center));
    Vec3 n2 = vec3_normalize(vec3_sub(p2, center));
    mesh->triangles[i] = (Triangle){
      .v0 = p0, .v1 = p1, .v2 = p2,
      .n0 = n0, .n1 = n1, .n2 = n2,
    };
  }
  mesh->position = center;
  mesh_compute_bbox(mesh);
}
