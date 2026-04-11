# Lesson 04 — Multiple Spheres + Materials

> **What you'll build:** By the end of this lesson, four distinct colored spheres appear on screen, each with its own material.

## Observable outcome

A window shows four spheres in a row against a blue background: ivory (beige), glass-tinted (pale blue), red rubber (dark red), and mirror-gray (white). Each sphere has a different color because each uses a different `RtMaterial`. The spheres overlap correctly in depth — closer spheres occlude farther ones. No lighting yet — just flat colors.

## New concepts

- `RtMaterial` struct — defines how a surface looks: diffuse color, specular exponent, albedo weights for diffuse/specular/reflect/refract, and refractive index
- `Scene` struct with fixed arrays — spheres, lights, and materials stored in fixed-size arrays with count fields (no `malloc`, no dynamic resizing)
- Nearest-hit traversal (`scene_intersect`) — loop through all spheres, keep the closest intersection (smallest `t`)

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/scene.h` | Created | `RtMaterial`, `Sphere`, `Scene` structs; `scene_init` populates 4 spheres + 4 materials |
| `game/intersect.h` | Modified | Add `scene_intersect` declaration |
| `game/intersect.c` | Modified | Add `scene_intersect` nearest-hit traversal |
| `game/main.h` | Modified | `RaytracerState` now contains a `Scene` |
| `game/main.c` | Modified | Call `scene_init` in `game_init`; render uses `scene_intersect` |

## Background — why this works

### JS analogy

In JavaScript, you'd create materials as objects: `{ color: [0.4, 0.4, 0.3], shininess: 50 }` and store spheres in an array: `const spheres = [new Sphere(...), ...]`. You'd use `spheres.push(s)` to add them and `for (const s of spheres)` to iterate.

In C, there are no dynamic arrays. We use fixed-size arrays with a count:
```c
Sphere spheres[MAX_SPHERES];  /* capacity: 16 */
int    sphere_count;           /* currently used: 4 */
```

This is a common C pattern — allocate the maximum capacity at compile time, track usage with a count. Simple, fast, and no `malloc`/`free` to manage.

### Why fixed arrays, not linked lists or dynamic arrays

Fixed arrays are:
1. **Cache-friendly** — all spheres sit contiguously in memory, so the CPU prefetcher loads them efficiently
2. **Simple** — no allocation, no deallocation, no pointer chasing
3. **Bounded** — you know the maximum memory usage at compile time

For a raytracer with 4-16 spheres, `MAX_SPHERES = 16` is plenty. A production raytracer with millions of objects would use a BVH (Bounding Volume Hierarchy), but that's a Lesson 17 topic.

### Material system design

Each material has four albedo weights that control how much each lighting component contributes:
- `albedo[0]` — diffuse weight (how much light scatters evenly from the surface)
- `albedo[1]` — specular weight (how much the shiny highlight contributes)
- `albedo[2]` — reflection weight (how much the surface reflects other objects)
- `albedo[3]` — refraction weight (how much light passes through the object)

For now, only `albedo[0]` matters (we have no lighting yet — just flat color). The other weights are set up for future lessons. Ivory uses `{0.6, 0.3, 0.1, 0.0}` — mostly diffuse with some specular and a hint of reflection. Red rubber uses `{0.9, 0.1, 0.0, 0.0}` — almost entirely diffuse.

### Nearest-hit traversal

When a ray is cast into the scene, it might hit multiple spheres. We want the closest one (smallest `t`). The algorithm:

1. Set `nearest = FLT_MAX` (a very large number from `<float.h>`)
2. For each sphere: test intersection. If hit and `t < nearest`, update `nearest` and save this hit
3. After all spheres, if we found a hit, the saved `HitRecord` contains the closest one

This is an O(n) linear scan. It works fine for 4 spheres. For thousands of objects, you'd use a spatial acceleration structure — but that's outside this course.

## Code walkthrough

### `game/scene.h` — structs and scene_init (simplified for this lesson)

```c
#ifndef GAME_SCENE_H
#define GAME_SCENE_H

#include "vec3.h"

#define MAX_SPHERES       16
#define MAX_LIGHTS         8
#define MAX_MATERIALS     16

/* RtMaterial defines how a surface looks.
 * albedo has 4 weights: [0]=diffuse, [1]=specular, [2]=reflect, [3]=refract.
 * They control how much each lighting component contributes to final color. */
typedef struct {
  Vec3  diffuse_color;
  float albedo[4];         /* diffuse, specular, reflect, refract weights */
  float specular_exponent; /* Phong exponent: higher = tighter highlight  */
  float refractive_index;  /* glass = 1.5, air = 1.0                     */
} RtMaterial;

typedef struct {
  Vec3  center;
  float radius;
  int   material_index;
} Sphere;

typedef struct {
  RtMaterial materials[MAX_MATERIALS];
  int      material_count;
  Sphere   spheres[MAX_SPHERES];
  int      sphere_count;
} Scene;

/* scene_init — set up the default scene matching ssloy's tutorial.
 * 4 materials: ivory, red rubber, mirror, glass.
 * 4 spheres positioned in a row.                                          */
static inline void scene_init(Scene *s) {
  s->material_count = 0;
  s->sphere_count   = 0;

  /* ── Materials ─────────────────────────────────────────────────────── */
  /* 0: ivory — diffuse + moderate specular */
  s->materials[s->material_count++] = (RtMaterial){
    .diffuse_color     = vec3_make(0.4f, 0.4f, 0.3f),
    .albedo            = {0.6f, 0.3f, 0.1f, 0.0f},
    .specular_exponent = 50.0f,
    .refractive_index  = 1.0f,
  };
  /* 1: red rubber — mostly diffuse, low specular */
  s->materials[s->material_count++] = (RtMaterial){
    .diffuse_color     = vec3_make(0.3f, 0.1f, 0.1f),
    .albedo            = {0.9f, 0.1f, 0.0f, 0.0f},
    .specular_exponent = 10.0f,
    .refractive_index  = 1.0f,
  };
  /* 2: mirror — almost entirely reflective */
  s->materials[s->material_count++] = (RtMaterial){
    .diffuse_color     = vec3_make(1.0f, 1.0f, 1.0f),
    .albedo            = {0.0f, 10.0f, 0.8f, 0.0f},
    .specular_exponent = 1425.0f,
    .refractive_index  = 1.0f,
  };
  /* 3: glass — refractive + reflective */
  s->materials[s->material_count++] = (RtMaterial){
    .diffuse_color     = vec3_make(0.6f, 0.7f, 0.8f),
    .albedo            = {0.0f, 0.5f, 0.1f, 0.8f},
    .specular_exponent = 125.0f,
    .refractive_index  = 1.5f,
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
}

#endif /* GAME_SCENE_H */
```

**Key lines:**
- `RtMaterial` has `albedo[4]` — a plain C array of 4 floats. In JS you'd use `[0.6, 0.3, 0.1, 0.0]`. In C, you access them with `mat.albedo[0]`, `mat.albedo[1]`, etc.
- `(RtMaterial){ .diffuse_color = ..., .albedo = {...}, ... }` — C99 designated initializers. You name each field explicitly. This is clearer than positional initialization and avoids ordering bugs.
- `s->materials[s->material_count++]` — the `++` post-increment assigns to the current slot AND advances the count in one expression. This is idiomatic C for "append to fixed array."
- `Sphere.material_index` is an integer index into `Scene.materials[]`, not a pointer. This is simpler and avoids dangling pointer issues if the materials array ever moves in memory.
- Each sphere sits at a different Z depth (`-12`, `-16`, `-18`), so they overlap in the view — proving our nearest-hit traversal works.

### `game/intersect.c` — scene_intersect (simplified for this lesson)

```c
#include "intersect.h"
#include <float.h>

/* ── LESSON 04 — Nearest-hit traversal ──────────────────────────────────
 * Test ray against all spheres.
 * Keep the closest hit (smallest t > 0).                                  */
int scene_intersect(RtRay ray, const Scene *scene, HitRecord *hit) {
  float nearest = FLT_MAX;
  int   found   = 0;
  HitRecord tmp;

  /* Spheres */
  for (int i = 0; i < scene->sphere_count; i++) {
    if (sphere_intersect(ray, &scene->spheres[i], &tmp) && tmp.t < nearest) {
      nearest = tmp.t;
      *hit    = tmp;
      found   = 1;
    }
  }

  return found;
}
```

**Key lines:**
- `FLT_MAX` from `<float.h>` — the largest representable `float` value (~3.4 x 10^38). Any real intersection distance will be smaller than this. In JS, you'd use `Infinity`.
- `HitRecord tmp` — a temporary on the stack. We test each sphere into `tmp`, then only copy to `*hit` if it's the nearest so far. This avoids corrupting the best-so-far result with a farther hit.
- `*hit = tmp` — struct copy. In C, assigning one struct to another copies all fields. This is a value copy, not a reference — there's no aliasing concern.
- `found` tracks whether we hit anything at all. The caller checks: `if (scene_intersect(ray, scene, &hit)) { /* shade */ }`.
- The loop is O(n) where n = sphere count. For 4 spheres, this is trivial. The function is called once per pixel (800 x 600 = 480,000 times per frame).

### `game/render.c` — updated render loop (simplified for this lesson)

```c
void render_scene(Backbuffer *bb, const Scene *scene) {
  int stride = bb->pitch / 4;
  float fov  = M_PI / 3.0f;
  float aspect = (float)bb->width / (float)bb->height;

  for (int j = 0; j < bb->height; j++) {
    for (int i = 0; i < bb->width; i++) {
      float x =  (2.0f * (i + 0.5f) / (float)bb->width  - 1.0f)
                 * tanf(fov / 2.0f) * aspect;
      float y = -(2.0f * (j + 0.5f) / (float)bb->height - 1.0f)
                 * tanf(fov / 2.0f);

      Vec3 dir = vec3_normalize(vec3_make(x, y, -1.0f));
      RtRay ray = { .origin = vec3_make(0,0,0), .direction = dir };

      HitRecord hit;
      Vec3 color;
      if (scene_intersect(ray, scene, &hit)) {
        /* Use the material's diffuse color (flat shading for now). */
        color = scene->materials[hit.material_index].diffuse_color;
      } else {
        color = vec3_make(0.2f, 0.7f, 0.8f);  /* background */
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
- `scene_intersect(ray, scene, &hit)` replaces the single `sphere_intersect` from Lesson 03. The render loop doesn't need to know how many spheres there are — `scene_intersect` handles the traversal.
- `scene->materials[hit.material_index].diffuse_color` — material lookup by index. The sphere stores a material index, the scene stores the materials array. This indirection means many spheres can share the same material.
- The background color `(0.2, 0.7, 0.8)` is returned when no sphere is hit. This will later be replaced by an environment map (Lesson 18).

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Only one sphere visible | `scene_intersect` returns after first hit instead of finding nearest | Must loop ALL spheres and keep track of smallest `t` |
| Sphere appears in front of another incorrectly | Comparing `t` wrong (`>` instead of `<`) | Keep the hit with smallest `t`: `if (tmp.t < nearest)` |
| All spheres same color | Using `sphere_index` instead of `material_index` for material lookup | Use `hit.material_index`, not the loop index `i` |
| Crash or garbage pixels | `material_index` out of bounds | Ensure `material_index` in each `Sphere` matches a valid `materials[]` slot |
| `scene_init` only has 3 spheres | Off-by-one in `material_count++` | Count the `++` increments — should be 4 materials and 4 spheres |

## Exercise

> Add a fifth sphere with center `(0, -3, -10)`, radius 1.0, using material index 1 (red rubber). It should appear small and close to the camera, below the others. Verify it occludes parts of farther spheres.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `{ color: [r,g,b], shininess: 50 }` | `RtMaterial` struct with named fields | Fixed struct layout; `albedo` is a `float[4]` array, not a JS array |
| `new Sphere(center, radius)` | `(Sphere){ .center = c, .radius = r, .material_index = i }` | C designated initializer — no `new`, no heap allocation |
| `spheres.push(s)` | `scene.spheres[count] = s; scene.sphere_count++` | Fixed-size array; no dynamic resize; manual bounds management |
| `for (const sphere of spheres)` | `for (int i = 0; i < scene.sphere_count; i++)` | Manual index loop; no iterators or `for...of` |
| `Infinity` | `FLT_MAX` from `<float.h>` | Largest representable float; used to initialize nearest distance |
| `materials[sphere.materialId]` | `scene->materials[hit.material_index]` | Same pattern — index-based lookup into a flat array |
