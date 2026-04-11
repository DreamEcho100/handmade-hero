#ifndef GAME_MESH_H
#define GAME_MESH_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/mesh.h — TinyRaytracer Course (L19)
 * ═══════════════════════════════════════════════════════════════════════════
 * Triangle mesh rendering — ssloy Part 1, Step 10 (Assignment 2):
 *   "We can render both spheres and planes. So let's draw triangle meshes!
 *    I've added a ray-triangle intersection function to it. Now adding
 *    the duck to our scene should be quite trivial."
 *
 * RAY-TRIANGLE INTERSECTION — Moller-Trumbore algorithm:
 *   Given ray (origin, dir) and triangle (v0, v1, v2):
 *   Compute edge vectors, determinant, and barycentric coordinates.
 *   If all barycentric coords are in [0,1] and sum ≤ 1, the ray hits.
 *   This is the industry-standard CPU ray-triangle test.
 *
 * MESH LOADING — fast_obj library:
 *   Loads .obj files (Wavefront OBJ format) with vertex positions,
 *   normals, texture coordinates, and material assignments.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "vec3.h"
#include "ray.h"
#include <stddef.h>

/* A single triangle — 3 vertices + 3 normals (smooth shading). */
typedef struct {
  Vec3 v0, v1, v2;   /* vertex positions */
  Vec3 n0, n1, n2;   /* per-vertex normals (for smooth shading) */
} Triangle;

/* Triangle mesh with AABB bounding box for early-out. */
typedef struct {
  Triangle *triangles;
  int       tri_count;
  int       material_index;
  Vec3      position;     /* world-space translation */
  /* AABB bounding box (pre-computed by mesh_compute_bbox). */
  Vec3      bbox_center;
  Vec3      bbox_half_size;
} TriMesh;

/* Ray-triangle intersection using Moller-Trumbore algorithm.
 * Returns 1 if hit, fills hit->t, hit->point, hit->normal.               */
int triangle_intersect(RtRay ray, const Triangle *tri, HitRecord *hit);

/* Ray-mesh intersection: AABB test + brute-force triangle loop.
 * For large meshes (>1000 triangles), a BVH would be needed.             */
int mesh_intersect(RtRay ray, const TriMesh *mesh, HitRecord *hit);

/* Compute AABB bounding box from triangle vertices. Call after loading.  */
void mesh_compute_bbox(TriMesh *mesh);

/* Load a .obj file using fast_obj. Returns number of triangles loaded.
 * Triangulates faces (quads → 2 triangles, n-gons → n-2 triangles).
 * Caller must free mesh->triangles when done.                             */
int mesh_load_obj(TriMesh *mesh, const char *filename);

/* Free triangle array. */
void mesh_free(TriMesh *mesh);

/* Create a simple procedural icosahedron (for testing without .obj file). */
void mesh_create_icosahedron(TriMesh *mesh, Vec3 center, float radius);

#endif /* GAME_MESH_H */
