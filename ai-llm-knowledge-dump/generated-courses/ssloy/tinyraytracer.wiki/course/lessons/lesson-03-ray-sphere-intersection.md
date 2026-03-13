# Lesson 03 — Ray-Sphere Intersection

> **What you'll build:** By the end of this lesson, a single sphere appears on screen — your first raytraced 3D object.

## Observable outcome

A window shows a single ivory-colored circle on a blue background. The circle is actually a sphere rendered by casting rays from the camera through each pixel. If the ray hits the sphere, the pixel is colored ivory; otherwise, it's the background color. This proves the ray-sphere intersection algorithm works.

## New concepts

- `RtRay`/`HitRecord` structs — a ray has an origin and direction; a hit records where the ray struck a surface
- Geometric ray-sphere intersection algorithm — project the sphere center onto the ray, compute the discriminant, solve for the two intersection distances
- Pixel-to-ray direction — convert pixel coordinates to Normalized Device Coordinates (NDC) using FOV and aspect ratio (ssloy's formula)

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/ray.h` | Created | `RtRay`, `HitRecord` structs, `ray_at` helper |
| `game/intersect.h` | Created | `sphere_intersect` declaration |
| `game/intersect.c` | Created | Geometric ray-sphere intersection implementation |
| `game/render.c` | Modified | Per-pixel loop now casts rays and tests sphere intersection |

## Background — why this works

### JS analogy

In JavaScript, you'd represent a ray as a simple object: `{ origin: {x,y,z}, direction: {x,y,z} }`. The intersection test would be a method: `sphere.intersect(ray)`. In C, we use two plain structs (`RtRay` and `HitRecord`) and a free function (`sphere_intersect`). No methods, no `this` — just data and functions that operate on it.

### What is a ray?

A ray is a half-line: it starts at an origin point and extends infinitely in one direction. Any point along the ray can be computed as:

```
point = origin + t * direction
```

where `t >= 0` is the distance. At `t=0`, you're at the origin. As `t` increases, you move along the direction. This is the fundamental equation of raytracing — everything else is about finding the value of `t` where the ray hits something.

### The geometric ray-sphere algorithm

Given a ray and a sphere (center + radius), we want to find whether and where they intersect. The algorithm (from ssloy's tutorial) works geometrically:

1. Compute `L` = vector from ray origin to sphere center
2. Project `L` onto the ray direction: `tca = dot(L, direction)` — this is how far along the ray the closest approach to the center occurs
3. Compute the squared perpendicular distance from the center to the ray: `d^2 = dot(L,L) - tca^2`
4. If `d^2 > radius^2`, the ray misses the sphere entirely
5. Otherwise, compute `thc = sqrt(radius^2 - d^2)` — the half-chord length
6. The two intersection distances are `t0 = tca - thc` (entry) and `t1 = tca + thc` (exit)
7. Take the nearest positive `t` (we don't want hits behind the camera)

### Pixel-to-ray direction (NDC)

To cast a ray through each pixel, we convert pixel coordinates `(i, j)` to a direction vector. The formula (from ssloy) is:

```c
float x =  (2.0f * (i + 0.5f) / width  - 1.0f) * tan(fov/2) * aspect;
float y = -(2.0f * (j + 0.5f) / height - 1.0f) * tan(fov/2);
Vec3 dir = vec3_normalize(vec3_make(x, y, -1.0f));
```

This maps pixel `(0,0)` (top-left) to the upper-left corner of the view frustum, and pixel `(width-1, height-1)` to the lower-right. The `- 1.0f` z-component means the camera looks down the negative Z axis. The `+ 0.5f` centers the ray within each pixel.

The `tan(fov/2)` factor controls how wide the field of view is. At `fov = PI/3` (60 degrees), `tan(30 degrees) ≈ 0.577`, so the view extends about 0.577 units left/right for every 1 unit of depth.

## Code walkthrough

### `game/ray.h` — complete file

```c
#ifndef GAME_RAY_H
#define GAME_RAY_H

#include "vec3.h"

typedef struct {
  Vec3  origin;
  Vec3  direction;
} RtRay;

/* HitRecord stores the result of a ray-object intersection test.
 * material_index is an index into Scene.materials[].                      */
typedef struct {
  float t;              /* distance along ray to hit point                */
  Vec3  point;          /* world-space hit position                       */
  Vec3  normal;         /* surface normal at hit (outward-facing)         */
  int   material_index; /* index into Scene.materials[]                   */
} HitRecord;

static inline Vec3 ray_at(RtRay r, float t) {
  return vec3_add(r.origin, vec3_scale(r.direction, t));
}

#endif /* GAME_RAY_H */
```

**Key lines:**
- `RtRay` uses the `Rt` prefix to avoid colliding with Raylib's built-in `Ray` type.
- `HitRecord` stores everything the renderer needs about an intersection: where it happened (`t`, `point`), the surface orientation (`normal`), and which material to shade with (`material_index`).
- `ray_at(r, t)` implements `origin + t * direction` — the parametric ray equation. `static inline` because it's called per-intersection-test, potentially millions of times.
- `t` is the distance along the ray, not a time value. When comparing hits, smaller `t` means closer to the camera.

### `game/intersect.h` — declaration (simplified for this lesson)

```c
#ifndef GAME_INTERSECT_H
#define GAME_INTERSECT_H

#include "ray.h"

int sphere_intersect(RtRay ray, const Sphere *sphere, HitRecord *hit);

#endif /* GAME_INTERSECT_H */
```

### `game/intersect.c` — sphere intersection (complete function)

```c
#include "intersect.h"
#include <math.h>

/* ── LESSON 03 — Geometric ray-sphere intersection ───────────────────────
 * Algorithm (from ssloy):
 *   L = sphere.center - ray.origin
 *   tca = dot(L, ray.direction)       (projection of L onto ray)
 *   d^2 = dot(L,L) - tca^2            (squared distance from center to ray)
 *   if d^2 > radius^2 -> miss
 *   thc = sqrt(radius^2 - d^2)
 *   t0 = tca - thc, t1 = tca + thc    (two intersection distances)
 *   take the nearest positive t.                                          */
int sphere_intersect(RtRay ray, const Sphere *sphere, HitRecord *hit) {
  Vec3  L   = vec3_sub(sphere->center, ray.origin);
  float tca = vec3_dot(L, ray.direction);
  float d2  = vec3_dot(L, L) - tca * tca;
  float r2  = sphere->radius * sphere->radius;

  if (d2 > r2) return 0;

  float thc = sqrtf(r2 - d2);
  float t0  = tca - thc;
  float t1  = tca + thc;

  if (t0 < 0.0f) t0 = t1;
  if (t0 < 0.0f) return 0;

  hit->t     = t0;
  hit->point = ray_at(ray, t0);
  /* Surface normal: outward-pointing, normalized. */
  hit->normal = vec3_normalize(vec3_sub(hit->point, sphere->center));
  hit->material_index = sphere->material_index;
  return 1;
}
```

**Key lines:**
- `L = sphere.center - ray.origin` — vector from the ray's start to the sphere's center.
- `tca = dot(L, direction)` — how far along the ray the center projects. If `tca < 0` and the ray origin is outside the sphere, the sphere is behind the camera. But we don't reject here because the origin might be inside the sphere.
- `d2 = dot(L,L) - tca*tca` — Pythagorean theorem. `dot(L,L)` is `|L|^2`, minus `tca^2` gives the squared perpendicular distance. This avoids computing an actual `sqrt` for the miss test.
- `if (d2 > r2) return 0` — fast rejection: the ray passes farther from the center than the radius. This is the common case (most rays miss most spheres).
- `if (t0 < 0.0f) t0 = t1` — if the near intersection is behind the camera, try the far one (we might be inside the sphere looking out).
- `hit->normal = normalize(hit.point - sphere.center)` — the surface normal at any point on a sphere points from the center to that point. Normalized to length 1 for lighting calculations.
- Returns `1` for hit, `0` for miss — C convention for boolean in functions (no `bool` type in older C).

### `game/render.c` — per-pixel ray casting (simplified for this lesson)

```c
/* Simplified render loop for Lesson 03.
 * Camera is fixed at the origin, looking down -Z. */
void render_scene(Backbuffer *bb, const Scene *scene) {
  int stride = bb->pitch / 4;
  float fov  = M_PI / 3.0f;  /* 60 degrees */
  float aspect = (float)bb->width / (float)bb->height;

  /* One test sphere. */
  Sphere test_sphere = {
    .center = vec3_make(0.0f, 0.0f, -16.0f),
    .radius = 2.0f,
    .material_index = 0,
  };

  for (int j = 0; j < bb->height; j++) {
    for (int i = 0; i < bb->width; i++) {
      /* Convert pixel (i,j) to ray direction (ssloy's NDC formula). */
      float x =  (2.0f * (i + 0.5f) / (float)bb->width  - 1.0f)
                 * tanf(fov / 2.0f) * aspect;
      float y = -(2.0f * (j + 0.5f) / (float)bb->height - 1.0f)
                 * tanf(fov / 2.0f);

      Vec3 dir = vec3_normalize(vec3_make(x, y, -1.0f));
      RtRay ray = { .origin = vec3_make(0,0,0), .direction = dir };

      HitRecord hit;
      Vec3 color;
      if (sphere_intersect(ray, &test_sphere, &hit)) {
        color = vec3_make(0.4f, 0.4f, 0.3f);  /* ivory */
      } else {
        color = vec3_make(0.2f, 0.7f, 0.8f);  /* sky blue background */
      }

      bb->pixels[j * stride + i] = GAME_RGB(
        (int)(color.x * 255.0f),
        (int)(color.y * 255.0f),
        (int)(color.z * 255.0f)
      );
    }
  }
}
```

**Key lines:**
- `(2.0f * (i + 0.5f) / width - 1.0f)` — maps pixel `i` from `[0, width)` to `[-1, +1]`. The `+ 0.5f` centers the ray in the pixel (not at the corner).
- The `y` component is negated because screen coordinates go top-to-bottom, but world Y goes bottom-to-top.
- `tanf(fov / 2.0f)` — converts FOV angle to the width of the view plane at distance 1. At 60 degrees FOV, `tan(30) ≈ 0.577`.
- `* aspect` on the x-component — corrects for non-square pixels. Without this, the sphere would appear stretched on a 800x600 window.
- `vec3_make(x, y, -1.0f)` — the `-1` Z means the camera looks down the negative Z axis (standard convention).
- `ray.origin = (0,0,0)` — the camera is at the world origin for now. This becomes a movable camera in later lessons.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| No sphere visible (all blue) | Sphere is at wrong Z position (too far or positive Z) | Sphere Z should be negative (e.g., `-16.0f`) — camera looks down -Z |
| Sphere appears as a flat ellipse | Missing `* aspect` in the x-direction | Multiply x by `(float)width / (float)height` |
| Sphere appears but is off-center | NDC formula missing `+ 0.5f` or signs wrong | Check: `(2*(i+0.5)/w - 1)` for x, negate for y |
| Black circle instead of ivory | Color values not multiplied by 255 | `GAME_RGB` expects 0-255 integers, not 0.0-1.0 floats |
| Sphere is inside-out (ring shape) | Normal computed as `center - point` instead of `point - center` | Normal must point outward: `normalize(hit.point - sphere.center)` |

## Exercise

> Move the sphere to `(3.0, 0.0, -16.0)` and increase the radius to 4.0. The sphere should appear shifted right and larger. Then add a second sphere at `(-3.0, 0.0, -16.0)` with radius 2.0 — test both and draw whichever is closer (smaller `t`). This is a preview of Lesson 04's nearest-hit traversal.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `{ origin: {x,y,z}, direction: {x,y,z} }` | `RtRay` struct with `Vec3 origin`, `Vec3 direction` | Fixed struct layout; no dynamic properties |
| `sphere.intersect(ray)` | `sphere_intersect(ray, &sphere, &hit)` | Free function; result written to `hit` via pointer |
| `return { hit: true, t, point, normal }` | `hit->t = t0; ... return 1;` | Output via pointer parameter; return value is just hit/miss |
| `const dir = new Vec3(x, y, -1).normalize()` | `vec3_normalize(vec3_make(x, y, -1.0f))` | Nested function calls instead of method chaining |
| `Math.tan(fov / 2)` | `tanf(fov / 2.0f)` | `tanf` for `float`; `-lm` required |
| `canvas.width / canvas.height` (aspect ratio) | `(float)bb->width / (float)bb->height` | Explicit float cast to avoid integer division |
