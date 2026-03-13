# Lesson 19 — Triangle Meshes

> **What you'll build:** By the end of this lesson, a procedural icosahedron (20 triangles) appears in the scene with smooth-shaded normals, and the codebase can load arbitrary .obj mesh files (duck, teapot, etc.) using the fast_obj library.

## Observable outcome

A smooth icosahedron sits at position (4, 1, -10) with ivory material. Despite being made of only 20 flat triangles, it appears rounded thanks to per-vertex normal interpolation (Phong shading). The mesh receives full lighting, shadows, and appears in reflections. Pressing **M** toggles mesh visibility. If an .obj file is placed at `assets/duck.obj`, it loads and renders automatically.

## New concepts

- Moller-Trumbore ray-triangle intersection — the industry-standard CPU algorithm (1 cross, 3 dots, 1 divide)
- Barycentric coordinates — (u, v, w) weights that describe where a point lies within a triangle
- Smooth normal interpolation — blending per-vertex normals using barycentric weights for curved appearance
- `fast_obj` .obj loading — a single-header library for parsing Wavefront OBJ files
- Fan triangulation — converting quads and n-gons into triangles by fanning from vertex 0
- AABB bounding box for meshes — same early-out strategy as voxels (Lesson 16)
- Procedural icosahedron — generating a regular 20-face polyhedron from the golden ratio

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/mesh.h` | Created | `Triangle` struct (3 vertices + 3 normals); `TriMesh` struct (array + bbox); function declarations |
| `game/mesh.c` | Created | Moller-Trumbore; AABB test; mesh intersect; .obj loading; icosahedron generator |
| `game/scene.h` | Modified | `TriMesh meshes[]` array added to `Scene`; icosahedron created in `scene_init` |
| `game/intersect.c` | Modified | `scene_intersect` tests meshes with `mesh_intersect` |
| `game/settings.h` | Modified | `show_meshes` toggle added |
| `utils/fast_obj.h` | Added | Single-header .obj parser (third-party library) |

## Background — why this works

### JS analogy

In Three.js you load meshes with `new THREE.OBJLoader().load('model.obj', callback)` and the engine handles vertex buffers, normals, and GPU upload. In our C raytracer, we load the same .obj format but keep the triangles in a CPU array. Each ray tests triangles individually (brute force with AABB early-out). This is slow for large meshes (>1000 triangles) but correct and educational.

### Moller-Trumbore algorithm

This is the standard CPU ray-triangle intersection algorithm, used in virtually every software raytracer. Given a ray `(origin, dir)` and a triangle `(v0, v1, v2)`:

1. Compute edge vectors: `edge1 = v1 - v0`, `edge2 = v2 - v0`
2. Compute the determinant: `h = cross(dir, edge2)`, `a = dot(edge1, h)`
3. If `|a| < epsilon`, the ray is parallel to the triangle (miss)
4. Compute first barycentric coordinate: `s = origin - v0`, `u = dot(s, h) / a`
5. If `u < 0` or `u > 1`, the point is outside the triangle (miss)
6. Compute second barycentric coordinate: `q = cross(s, edge1)`, `v = dot(dir, q) / a`
7. If `v < 0` or `u + v > 1`, the point is outside the triangle (miss)
8. Compute distance: `t = dot(edge2, q) / a`
9. If `t > epsilon`, it is a hit

The algorithm's elegance is that it computes the intersection point and barycentric coordinates simultaneously — no separate "point in triangle" test is needed.

### Barycentric coordinates and smooth shading

For a triangle with vertices v0, v1, v2, any point P inside the triangle can be expressed as:

```
P = w * v0 + u * v1 + v * v2   where   w = 1 - u - v
```

The values (w, u, v) are the barycentric coordinates. Moller-Trumbore gives us `u` and `v` directly; `w = 1 - u - v`.

For smooth shading, we interpolate the per-vertex normals using the same weights:

```c
float w = 1.0f - u - v;
hit->normal = vec3_normalize(
  vec3_add(vec3_add(vec3_scale(n0, w), vec3_scale(n1, u)),
           vec3_scale(n2, v))
);
```

This produces a smoothly varying normal across the triangle face, making flat triangles appear curved under lighting. This is the same technique (Phong interpolation) used by GPU rasterizers.

### Fan triangulation

OBJ files can contain faces with any number of vertices (quads, pentagons, etc.). We convert them to triangles using **fan triangulation**: for a face with vertices `(v0, v1, v2, v3, ...)`, generate triangles `(v0, v1, v2)`, `(v0, v2, v3)`, `(v0, v3, v4)`, etc. This works correctly for convex polygons.

```
Quad (v0, v1, v2, v3) → Triangle (v0, v1, v2) + Triangle (v0, v2, v3)
Pentagon → 3 triangles, all sharing v0
N-gon → (N-2) triangles
```

### The golden ratio and the icosahedron

A regular icosahedron (20 equilateral triangular faces) has 12 vertices that lie on three mutually perpendicular golden rectangles. The golden ratio `phi = (1 + sqrt(5)) / 2 ≈ 1.618` defines the rectangle proportions:

```c
float t = (1.0f + sqrtf(5.0f)) / 2.0f; /* golden ratio */
float s = radius / sqrtf(1.0f + t * t); /* normalize to desired radius */
```

The 12 vertices are all permutations of `(+-s, +-t*s, 0)`, `(0, +-s, +-t*s)`, and `(+-t*s, 0, +-s)`.

## Code walkthrough

### `game/mesh.h` — key types

```c
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
  Vec3      bbox_center;
  Vec3      bbox_half_size;
} TriMesh;
```

**Key design:** Unlike `VoxelModel` (which uses a fixed-size array), `TriMesh` uses a dynamically allocated `triangles` pointer. This is because mesh sizes vary wildly (20 triangles for an icosahedron, 100,000 for a detailed model). The caller must free with `mesh_free`.

### `game/mesh.c` — `triangle_intersect` (Moller-Trumbore, complete)

```c
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
```

**Key lines:**
- Line 3: `vec3_cross(ray.direction, edge2)` — the first cross product. This is the most expensive operation per triangle.
- Line 4: `vec3_dot(edge1, h)` — the determinant. Near-zero means the ray is parallel to the triangle plane.
- Line 7: `f = 1.0f / a` — precompute the reciprocal to avoid three separate divisions.
- Lines 9-10: Early exit if `u` is outside [0, 1] — the hit point is outside the triangle.
- Lines 13-14: Early exit if `v < 0` or `u + v > 1` — the point is outside the triangle in the other direction.
- Lines 24-28: Barycentric interpolation. `w` is the weight for v0 (the "remaining" weight). The result is normalized to handle non-unit input normals.

### `game/mesh.c` — `mesh_intersect` (complete)

```c
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
```

This follows the same brute-force-with-AABB pattern as voxels. For meshes with >1000 triangles, a BVH (discussed in Lesson 17) would be essential.

### `game/mesh.c` — `mesh_load_obj` (complete, via fast_obj)

```c
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
```

**Key lines:**
- Line 10-12: First pass counts triangles — an n-gon produces `n - 2` triangles.
- Line 28-29: `i0` is the fan center vertex, shared by all triangles of this face.
- Line 30-33: Vertex positions are offset by `mesh->position` to place the mesh in world space.
- Lines 34-36: If the .obj file has no normals (`i0.n == 0`), use a default up vector `(0, 1, 0)`. This prevents black faces from zero normals.
- Line 38: Fan triangulation: vertex 0 is shared, and we sweep through consecutive vertex pairs.
- Line 65: `fast_obj_destroy(obj)` frees the parser's internal data. Our `mesh->triangles` array remains valid (it is our allocation).

### `game/mesh.c` — `mesh_create_icosahedron` (complete)

```c
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
```

**Key insight on normals:** For a convex shape centered at `center`, the per-vertex normal is simply the normalized direction from center to vertex. This is the same formula as sphere normals — and for good reason: the icosahedron approximates a sphere.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Mesh appears but is completely flat-shaded | Using face normals instead of vertex normals | Use barycentric interpolation: `w*n0 + u*n1 + v*n2` in `triangle_intersect` |
| Mesh is invisible | AABB bounding box is zero-sized | Call `mesh_compute_bbox` after loading/creating the mesh |
| .obj mesh appears at origin instead of desired position | Forgot to add `mesh->position` to vertex coordinates | `vec3_add(mesh->position, vertex)` during loading |
| Mesh has holes (some triangles missing) | Fan triangulation fails on concave polygons | Fan triangulation only works for convex faces; most .obj files use convex faces |
| .obj normals are missing (dark faces) | Normal index is 0 (OBJ indices are 1-based; 0 means absent) | Check `i0.n != 0` before reading normals; fall back to `(0, 1, 0)` |
| Crash on large .obj file | Too many triangles for brute-force intersection | Consider adding a BVH (Lesson 17), or limit mesh to <1000 triangles |
| Mesh appears inside-out (normals pointing inward) | Winding order reversed in .obj file | Try negating the interpolated normal: `vec3_negate(hit->normal)` |

## Exercise

> Download a simple .obj model (a low-poly duck or teapot from a free 3D model site). Place it in `assets/duck.obj` and uncomment the `mesh_load_obj` call in `scene_init`. Adjust the position and verify it renders with proper shading. If the model has no normals, compute face normals from the cross product of the two edges: `normalize(cross(edge1, edge2))`.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `new THREE.OBJLoader().load(url, cb)` | `mesh_load_obj(mesh, "file.obj")` — `fast_obj_read` + manual triangle extraction | Synchronous; no callback; must triangulate manually |
| `geometry.attributes.position` | `mesh->triangles[i].v0, .v1, .v2` | No GPU buffers; triangles stored as structs in RAM |
| `geometry.attributes.normal` | `mesh->triangles[i].n0, .n1, .n2` | Per-vertex normals stored alongside positions |
| `THREE.BufferGeometry` index buffer | `faces[20][3]` index array (icosahedron) | OBJ loader inlines vertices; icosahedron uses explicit indices |
| `new THREE.IcosahedronGeometry(r, 0)` | `mesh_create_icosahedron(mesh, center, radius)` | Same golden-ratio vertices; ours is ~30 lines |
| `raycaster.intersectObject(mesh)` | `mesh_intersect(ray, mesh, &hit)` — Moller-Trumbore per triangle | Brute force; no BVH (Three.js uses one internally) |
