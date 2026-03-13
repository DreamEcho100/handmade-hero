# Lesson 10 — Checkerboard Floor

> **What you'll build:** By the end of this lesson, a checkerboard floor extends beneath the spheres toward the horizon. The floor receives shadows, reflects in the mirror sphere, and refracts through the glass sphere.

## Observable outcome

An infinite-looking checkerboard floor appears at y=0, extending to the horizon beneath all four spheres. The floor alternates between dark orange-brown and dark gray tiles. Sphere shadows are visible on the floor tiles. The mirror sphere reflects the checkerboard pattern. The glass sphere shows a distorted view of the floor through refraction. The scene now has a ground plane that anchors the spheres in space.

## New concepts

- Ray-plane intersection: `t = -origin.y / dir.y` for the y=0 horizontal plane
- Procedural checkerboard pattern: `((int)floor(x) + (int)floor(z)) & 1` alternates tile colors
- `material_index = -1` convention: the floor uses a special "inline" material instead of indexing into `scene.materials[]`

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/intersect.c` | Modified | Add `plane_intersect` function; extend `scene_intersect` to test floor after spheres |
| `game/intersect.h` | Modified | Add `plane_intersect` declaration |
| `game/raytracer.c` | Modified | Handle `material_index == -1` with inline checkerboard material in `cast_ray` |

## Background — why this works

### JS analogy

In Three.js, a checkerboard floor would be a `PlaneGeometry` with a repeating checker texture. In our raytracer, there is no geometry mesh and no texture file. The floor is defined mathematically (the plane y=0) and the pattern is computed procedurally — a pure function of the hit point's x and z coordinates.

### Ray-plane intersection

A horizontal plane at y=0 is the simplest possible surface: every point on it has `y = 0`. To find where a ray hits it:

```
ray: P(t) = origin + t * direction
plane: y = 0

Solve for t:
origin.y + t * direction.y = 0
t = -origin.y / direction.y
```

That is it — one division. Compare this to the sphere intersection from L03 which required a quadratic formula. Planes are trivially simple.

Guard conditions:
- If `direction.y` is near zero, the ray is parallel to the floor — no hit
- If `t < 0`, the floor is behind the camera — no hit
- If `t > 1000`, the floor is too far away — avoids floating-point noise at the horizon

### The checkerboard pattern

Given a hit point `(x, y, z)` on the floor (where `y = 0`), the checkerboard pattern is:

```c
int checker = ((int)floorf(hit.point.x) + (int)floorf(hit.point.z)) & 1;
```

This works because `floorf` maps each unit square to an integer. Adding the x and z integers and checking the last bit (`& 1`) alternates between 0 and 1 in a checkerboard pattern:

```
 z=2:  0  1  0  1  0
 z=1:  1  0  1  0  1
 z=0:  0  1  0  1  0
 z=-1: 1  0  1  0  1
       x=-2 -1  0  1  2
```

The `checker` value selects between two colors: `0` = dark brown `(0.3, 0.2, 0.1)`, `1` = dark gray `(0.3, 0.3, 0.3)`.

### The material_index = -1 convention

The floor does not have a pre-defined material in `scene.materials[]`. Instead, `plane_intersect` sets `hit.material_index = -1` as a sentinel value. In `cast_ray`, we check for this special case and construct an inline material:

```c
if (hit.material_index < 0) {
  /* Checkerboard floor — inline material. */
  int checker = ((int)floorf(hit.point.x) + (int)floorf(hit.point.z)) & 1;
  mat = (RtMaterial){
    .diffuse_color     = checker ? vec3_make(0.3f, 0.3f, 0.3f)
                                 : vec3_make(0.3f, 0.2f, 0.1f),
    .albedo            = {1.0f, 0.0f, 0.0f, 0.0f},
    .specular_exponent = 1.0f,
    .refractive_index  = 1.0f,
  };
} else {
  mat = scene->materials[hit.material_index];
}
```

The floor material has `albedo = {1.0, 0.0, 0.0, 0.0}` — pure diffuse, no specular, no reflection, no refraction. This makes it a simple matte surface.

## Code walkthrough

### `game/intersect.c` — plane_intersect (new function)

```c
/* Ray-plane intersection (y = 0 floor).
 * t = -origin.y / dir.y.  Only accept hits in front of the camera
 * and within a reasonable distance (avoid rendering to infinity). */
int plane_intersect(RtRay ray, HitRecord *hit) {
  /* Avoid division by zero when ray is parallel to the floor. */
  if (fabsf(ray.direction.y) < 1e-5f) return 0;

  float t = -ray.origin.y / ray.direction.y;
  if (t < 0.001f) return 0;   /* behind camera */
  if (t > 1000.0f) return 0;  /* too far away  */

  Vec3 pt = ray_at(ray, t);

  /* Limit the floor to a reasonable area to avoid floating-point noise. */
  if (fabsf(pt.x) > 30.0f || fabsf(pt.z) > 50.0f) return 0;

  hit->t      = t;
  hit->point  = pt;
  hit->normal = vec3_make(0.0f, 1.0f, 0.0f); /* floor normal points up */
  hit->material_index = -1; /* special: checkerboard handled in cast_ray */
  return 1;
}
```

**Key lines:**
- Line 6: `fabsf(ray.direction.y) < 1e-5f` — rays nearly parallel to the floor will never hit it. `fabsf` is the float absolute value.
- Line 8: `t = -ray.origin.y / ray.direction.y` — the entire intersection is one division. If the camera is at y=2 and the ray points downward (direction.y < 0), then `t = -2 / (-0.5) = 4` — the floor is 4 units along the ray.
- Line 9: `t < 0.001f` — a small positive threshold (not 0) to avoid self-intersection noise, similar to shadow epsilon.
- Line 15: Capping x and z prevents rendering tiles that are so far away they shimmer due to floating-point aliasing.
- Line 20: `hit->normal = vec3_make(0.0f, 1.0f, 0.0f)` — the floor normal always points straight up. Unlike spheres, a plane has the same normal everywhere.
- Line 21: `material_index = -1` — the sentinel value that tells `cast_ray` to use the checkerboard material instead of looking up `scene.materials[]`.

### `game/intersect.c` — scene_intersect extended

The floor check is added at the end of `scene_intersect`, after all spheres have been tested:

```c
int scene_intersect(RtRay ray, const Scene *scene, HitRecord *hit) {
  float nearest = FLT_MAX;
  int   found   = 0;
  HitRecord tmp;

  /* Spheres (from L04) */
  for (int i = 0; i < scene->sphere_count; i++) {
    if (sphere_intersect(ray, &scene->spheres[i], &tmp) && tmp.t < nearest) {
      nearest = tmp.t;
      *hit    = tmp;
      found   = 1;
    }
  }

  /* Floor plane (new in L10) */
  if (plane_intersect(ray, &tmp) && tmp.t < nearest) {
    nearest = tmp.t;
    *hit    = tmp;
    found   = 1;
  }

  return found;
}
```

**Key point:** The floor competes with spheres for the nearest hit. If a sphere is in front of the floor tile (closer t), the sphere wins. If the floor is closer (e.g., at the camera's feet), the floor wins. The `tmp.t < nearest` check handles this automatically.

### `game/raytracer.c` — checkerboard in cast_ray

The material lookup at the top of `cast_ray` is extended with the `material_index < 0` check:

```c
  /* ── Determine material ──────────────────────────────────────────── */
  RtMaterial mat;
  if (hit.material_index < 0) {
    /* Checkerboard floor. */
    int checker = ((int)floorf(hit.point.x) + (int)floorf(hit.point.z)) & 1;
    mat = (RtMaterial){
      .diffuse_color     = checker ? vec3_make(0.3f, 0.3f, 0.3f)
                                   : vec3_make(0.3f, 0.2f, 0.1f),
      .albedo            = {1.0f, 0.0f, 0.0f, 0.0f},
      .specular_exponent = 1.0f,
      .refractive_index  = 1.0f,
    };
  } else {
    mat = scene->materials[hit.material_index];
  }
```

**Key lines:**
- Line 5: `floorf` is essential — it rounds toward negative infinity. Regular integer truncation rounds toward zero, which produces a different pattern for negative coordinates (double-width stripes at x=0).
- Line 5: `& 1` is a bitwise AND that extracts the last bit — equivalent to `% 2` but faster and works correctly for negative integers (while `%` in C can produce negative results for negative operands).
- Lines 7-8: Two muted colors. Bright white/black would be distracting; these earth tones complement the sphere colors.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| No floor visible | Camera is below y=0 or looking upward | Camera at positive y, pointing slightly downward |
| Checkerboard has double-width stripes at x=0 | Using `(int)` cast instead of `(int)floorf()` | `floorf(-0.5f)` = -1, but `(int)(-0.5f)` = 0. Always use `floorf`. |
| Floor appears but is solid color (no pattern) | Forgot `& 1` or wrong coordinates | Use `((int)floorf(x) + (int)floorf(z)) & 1` — both x and z contribute |
| Crash: accessing `scene->materials[-1]` | Not checking `material_index < 0` before array access | Add the `if (hit.material_index < 0)` branch in `cast_ray` |
| Floor has no shadows | `plane_intersect` must run in `scene_intersect` so shadow rays also see the floor | Verify floor is checked in `scene_intersect`, not only for camera rays |

## Exercise

> Change the checkerboard tile size by dividing the coordinates before flooring: `(int)floorf(hit.point.x * 0.5f)` produces tiles twice as large. Try `* 2.0f` for tiles half as large. Observe how the floor pattern scales. Restore to the original `* 1.0f` (no scaling) when done.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `new PlaneGeometry(100, 100)` | `plane_intersect()` — mathematical plane, no vertices | No geometry mesh; the plane is implicit (y=0) |
| `new TextureLoader().load('checker.png')` | `((int)floorf(x) + (int)floorf(z)) & 1` | No texture file; pattern is a pure function of coordinates |
| `Math.floor(x)` | `floorf(x)` | Float version of floor; from `<math.h>` with `-lm` |
| `material === null ? defaultMat : material` | `hit.material_index < 0 ? inline_mat : scene->materials[idx]` | Sentinel value -1 instead of null reference |
| `x & 1` (bitwise AND) | Same syntax in C | Identical bitwise operation |
