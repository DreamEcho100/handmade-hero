# Lesson 13 — Ray-Box Intersection

> **What you'll build:** By the end of this lesson, an axis-aligned cube appears in the scene alongside the spheres — shaded, shadowed, and reflective.

## Observable outcome

A teal-colored cube sits at position (3, -2, -14) in front of the checkerboard floor. It receives full Phong shading (diffuse + specular), casts shadows onto the floor, and appears in the mirror sphere's reflections. The cube is a new geometry type — the first non-sphere, non-plane object in the raytracer.

## New concepts

- `Box` struct — center + half_size (half-extents) representation for axis-aligned boxes
- Per-face ray-AABB intersection — testing each of the 6 faces individually (simpler than the slab method)
- Axis-aligned face normals — the normal of any AABB face is a unit vector along exactly one axis
- `(&vec.x)[d]` trick — accessing Vec3 components by index (0=x, 1=y, 2=z) using pointer arithmetic

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/scene.h` | Modified | `Box` struct added; `boxes[]` array + `box_count` in `Scene`; box material + box instance in `scene_init` |
| `game/intersect.h` | Modified | `box_intersect` declaration added |
| `game/intersect.c` | Modified | `box_intersect` function; `scene_intersect` updated to test boxes |
| `game/settings.h` | Modified | `show_boxes` toggle added to `RenderSettings` |

## Background — why this works

### JS analogy

In a JS raytracer you might write `class Box extends Intersectable { intersect(ray) { ... } }` using inheritance. In C there are no classes and no virtual dispatch. Instead, `box_intersect` is a standalone function with the same signature pattern as `sphere_intersect`:

```c
int box_intersect(RtRay ray, const Box *box, HitRecord *hit);
```

Adding a new geometry type means: (1) define the struct, (2) write an intersect function, (3) loop over instances in `scene_intersect`. No vtable, no interface — just explicit code.

### How the per-face algorithm works

ssloy's Part 3 uses a per-face approach rather than the more common slab method. The idea is direct:

1. A box has 6 faces — 2 per axis (near and far).
2. For each axis `d` (x, y, z), compute where the ray hits the near face and the far face.
3. For each candidate hit, check if the hit point falls within the other two axes' bounds.
4. Keep the closest valid hit across all 6 faces.

This is more intuitive than the slab method because you can visualize each face test individually. The slab method computes overlapping ranges of `t` values — mathematically equivalent but harder to debug.

### The `(&vec.x)[d]` trick

To loop over axes generically, we need to access Vec3 components by index. Since `Vec3` is `{ float x, y, z; }` — three contiguous floats — we can use pointer arithmetic:

```c
float dir_d = (&ray.direction.x)[d];  /* d=0 → x, d=1 → y, d=2 → z */
```

This works because C guarantees struct members declared consecutively with the same type are laid out contiguously in memory. It avoids a 3-way `switch` statement in a tight inner loop.

### Why axis-aligned normals are trivial

For an AABB face, the outward normal is a unit vector along the axis perpendicular to that face. If the ray hits the +x face, the normal is `(1, 0, 0)`. If it hits the -y face, the normal is `(0, -1, 0)`. We construct this by zeroing a Vec3 and setting component `d` to the `side` value (+1 or -1):

```c
hit->normal = vec3_make(0.0f, 0.0f, 0.0f);
(&hit->normal.x)[d] = (float)side;
```

## Code walkthrough

### `game/scene.h` — Box struct (additions)

```c
/* LESSON 13 — Axis-aligned box with center and half-extents. */
typedef struct {
  Vec3 center;
  Vec3 half_size;
  int  material_index;
} Box;
```

**Key design:** `half_size` stores half the width/height/depth of the box. A box with `center = (3, -2, -14)` and `half_size = (1, 1, 1)` extends from `(2, -3, -15)` to `(4, -1, -13)` — a 2x2x2 cube.

The Scene struct gains:

```c
#define MAX_BOXES 8

/* Inside Scene: */
Box  boxes[MAX_BOXES];
int  box_count;
```

And `scene_init` adds a teal box:

```c
/* 4: box material — teal, moderate specular */
s->materials[s->material_count++] = (RtMaterial){
  .diffuse_color     = vec3_make(0.2f, 0.5f, 0.5f),
  .albedo            = {0.6f, 0.3f, 0.1f, 0.0f},
  .specular_exponent = 50.0f,
  .refractive_index  = 1.0f,
};

/* ── Boxes (L13) ───────────────────────────────────────────────────── */
s->boxes[s->box_count++] = (Box){
  .center    = vec3_make(3.0f, -2.0f, -14.0f),
  .half_size = vec3_make(1.0f, 1.0f, 1.0f),
  .material_index = 4, /* teal */
};
```

### `game/intersect.c` — `box_intersect` (complete function)

```c
/* ── LESSON 13 — Per-face ray-AABB intersection ─────────────────────────
 * Algorithm (from ssloy Part 3): check each of the 3 axis pairs.
 * For each axis d, compute the intersection with the near face.
 * If the hit point is within the other two axes' bounds, it's a hit.
 *
 * This is simpler (and more intuitive) than the slab method:
 * we check each face individually rather than computing overlapping ranges. */
int box_intersect(RtRay ray, const Box *box, HitRecord *hit) {
  float best_t = FLT_MAX;
  int   found  = 0;

  for (int d = 0; d < 3; d++) {
    /* Access Vec3 components by index: 0=x, 1=y, 2=z. */
    float dir_d    = (&ray.direction.x)[d];
    float orig_d   = (&ray.origin.x)[d];
    float center_d = (&box->center.x)[d];
    float half_d   = (&box->half_size.x)[d];

    if (fabsf(dir_d) < 1e-5f) continue; /* ray parallel to this face pair */

    /* Check both faces on this axis (near and far). */
    for (int side = -1; side <= 1; side += 2) {
      float face_d = center_d + (float)side * half_d;
      float t = (face_d - orig_d) / dir_d;
      if (t < 0.001f || t >= best_t) continue;

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

      if (p1 >= c1 - h1 && p1 <= c1 + h1 &&
          p2 >= c2 - h2 && p2 <= c2 + h2) {
        best_t = t;
        hit->t     = t;
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
```

**Key lines:**
- Line 5: `best_t = FLT_MAX` — tracks the closest face hit. We must test all 6 faces because the closest one could be on any axis.
- Line 14: `fabsf(dir_d) < 1e-5f` — if the ray is parallel to a face pair (direction component near zero), skip it. Division by near-zero would produce garbage.
- Line 17: `side = -1` and `side = +1` — test both the near face (center - half) and far face (center + half) on this axis.
- Line 18: `t = (face_d - orig_d) / dir_d` — standard ray-plane intersection along one axis. The face is an infinite plane at `x = face_d` (or y, or z).
- Line 19: `t < 0.001f` — behind the camera. `t >= best_t` — a closer face already found.
- Lines 22-23: `d1 = (d + 1) % 3` — the two other axes. If we're testing the x-face, check that the hit point's y and z are within bounds.
- Line 32: Normal construction — zero vector with one component set to +1 or -1.

### `game/intersect.c` — `scene_intersect` box loop (addition)

```c
/* Boxes (L13) — toggleable via B key */
if (!settings || settings->show_boxes) {
  for (int i = 0; i < scene->box_count; i++) {
    if (box_intersect(ray, &scene->boxes[i], &tmp) && tmp.t < nearest) {
      nearest = tmp.t;
      *hit    = tmp;
      found   = 1;
      is_voxel_hit = 0;
    }
  }
}
```

This follows the exact same pattern as the sphere loop — test each box, keep the nearest hit. The `is_voxel_hit = 0` ensures that if a box is closer than a voxel, the voxel color override is cleared.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Box invisible (rays pass through) | Forgot to add box loop in `scene_intersect` | Add the box loop between spheres and floor in `scene_intersect` |
| Box appears but lighting is wrong (flat gray) | Normal is zero vector (forgot to set the axis component) | Ensure `(&hit->normal.x)[d] = (float)side` after zeroing the normal |
| Box has dark seams at edges | Epsilon too large in `t < 0.001f` | Use `0.001f` not `0.1f` — large epsilon hides geometry at close range |
| Box renders inside-out (dark faces visible, lit faces hidden) | `side` sign is inverted | `side = -1` is the min face, `side = +1` is the max face; normal sign must match |
| Compile error: `FLT_MAX` undeclared | Missing `#include <float.h>` | Add `#include <float.h>` to intersect.c |

## Exercise

> Add a second box to the scene, positioned at `(-5, -1, -12)` with `half_size = (0.5, 2.0, 0.5)` and material index 1 (red rubber). This creates a tall red pillar. Verify it receives shadows from the spheres.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `class Box extends Shape { intersect(ray) {...} }` | `int box_intersect(RtRay, const Box*, HitRecord*)` | No inheritance; standalone function with same signature pattern |
| `shapes.push(new Box(...))` | `s->boxes[s->box_count++] = (Box){...}` | Fixed-size array; no dynamic allocation |
| `obj.x`, `obj.y`, `obj.z` property access | `(&vec.x)[d]` index access via pointer arithmetic | C has no computed property names; pointer math gives index access |
| `Math.abs(x) < epsilon` | `fabsf(dir_d) < 1e-5f` | `fabsf` is the float variant; `fabs` is for double |
