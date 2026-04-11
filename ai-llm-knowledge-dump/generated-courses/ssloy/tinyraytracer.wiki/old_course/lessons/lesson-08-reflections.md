# Lesson 08 — Reflections

> **What you'll build:** By the end of this lesson, the mirror sphere reflects the entire scene — other spheres, the background, everything. You will extract ray casting into its own recursive function.

## Observable outcome

The third sphere (mirror material, `albedo[2] = 0.8`) now shows a reflected image of the other three spheres and the background gradient. The mirror sphere looks metallic and reflective. You can see the ivory and red rubber spheres reflected in its surface. Reflections are recursive up to depth 3, so the mirror shows reflections of reflections (though these are faint due to energy loss at each bounce).

## New concepts

- Recursive `cast_ray(ray, scene, depth+1)` with `MAX_RECURSION_DEPTH = 3` — the ray bounces off surfaces
- Reflect direction + origin offset: reuse `vec3_reflect` from L06, offset like shadow rays
- Material blending with albedo weights: `diffuse * albedo[0] + specular * albedo[1] + reflect_color * albedo[2]`

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/raytracer.h` | Created | `cast_ray` declaration + `MAX_RECURSION_DEPTH` constant |
| `game/raytracer.c` | Created | Recursive `cast_ray` implementation with reflection |
| `game/main.c` | Modified | Call `cast_ray` instead of inline lighting; add `raytracer.c` to build |

## Background — why this works

### JS analogy

In Three.js, you get reflections by using `MeshStandardMaterial({ metalness: 1.0, roughness: 0.0 })` combined with an environment map or a `CubeCamera`. The engine renders the scene from the mirror's perspective and maps the result onto the surface. In our raytracer, reflections are more elegant: when a ray hits a mirror surface, we simply cast *another* ray in the reflected direction and use whatever color it returns. Recursion handles the rest.

### Why a new file?

Until now, ray casting was a simple "cast ray, find hit, compute lighting" sequence inside `main.c` or `render.c`. With reflections, the ray casting function needs to call *itself* — it is now recursive. This is complex enough to warrant its own file: `raytracer.h` (declaration) and `raytracer.c` (implementation).

The `cast_ray` function becomes the central entry point for all color computation. It encapsulates:
1. Scene intersection (from L04)
2. Material lookup
3. Lighting computation (from L05-L07, shadows included)
4. **Reflection** (new in this lesson)
5. Material blending — combining all components

### The reflection algorithm

When a ray hits a reflective surface (`albedo[2] > 0`):

1. Compute the reflected direction using `vec3_reflect(ray.direction, hit.normal)` — the same formula from L06 but applied to the incoming ray direction instead of the light direction
2. Offset the reflection ray origin (same epsilon trick as shadow rays, L07)
3. Recursively call `cast_ray` with the reflected ray and `depth + 1`
4. Blend the reflected color into the final result weighted by `albedo[2]`

```
      incoming ray          reflected ray
           \                    /
            \     normal       /
             \       |        /
              \      |       /
               ------*------   <-- mirror surface
```

### Material blending with albedo weights

Each material has an `albedo[4]` array that controls how much each lighting component contributes to the final color:

| Index | Component | Meaning |
|-------|-----------|---------|
| `albedo[0]` | Diffuse | How much of the surface's own color shows |
| `albedo[1]` | Specular | How bright the Phong highlight is |
| `albedo[2]` | Reflection | How much of the reflected scene shows |
| `albedo[3]` | Refraction | How much of the refracted scene shows (L09) |

The final color formula:
```
color = diffuse_color  * albedo[0]
      + specular_color * albedo[1]
      + reflect_color  * albedo[2]
```

For the mirror material: `albedo = {0.0, 10.0, 0.8, 0.0}` — almost no diffuse, strong specular highlight, and 80% reflected scene. The high specular value (10.0) creates an intense, tight highlight that makes the mirror look polished.

For ivory: `albedo = {0.6, 0.3, 0.1, 0.0}` — mostly diffuse color, moderate specular, and a subtle 10% reflection that gives it a slight shine.

### Why recursion depth matters

Without a depth limit, two mirrors facing each other would recurse forever. `MAX_RECURSION_DEPTH = 3` means a ray can bounce at most 3 times before returning the background color. At depth 3, the reflected image is already quite dim (0.8^3 = 0.512 of the original energy for a mirror), so deeper bounces add negligible visual quality.

## Code walkthrough

### `game/raytracer.h` — complete file

```c
#ifndef GAME_RAYTRACER_H
#define GAME_RAYTRACER_H

#include "vec3.h"
#include "ray.h"
#include "scene.h"

#define MAX_RECURSION_DEPTH 3

Vec3 cast_ray(RtRay ray, const Scene *scene, int depth);

#endif /* GAME_RAYTRACER_H */
```

**Key lines:**
- Line 9: `MAX_RECURSION_DEPTH 3` — stopping condition for recursion. Each bounce increments `depth`.
- Line 11: `cast_ray` returns a `Vec3` color (RGB as floats 0.0-1.0). The `depth` parameter starts at 0 and increments on each reflection bounce.

### `game/raytracer.c` — complete file (L08 version)

This is the simplified L08 version without `RenderSettings` (added in L12) and without refractions (added in L09).

```c
#include "raytracer.h"
#include "intersect.h"
#include "lighting.h"
#include <math.h>

Vec3 cast_ray(RtRay ray, const Scene *scene, int depth) {
  HitRecord hit;

  /* Base case: too deep or no hit → background color. */
  if (depth > MAX_RECURSION_DEPTH ||
      !scene_intersect(ray, scene, &hit)) {
    /* Simple gradient background (sky-like). */
    float t = 0.5f * (ray.direction.y + 1.0f);
    return vec3_lerp(vec3_make(0.2f, 0.7f, 0.8f),
                     vec3_make(1.0f, 1.0f, 1.0f), t * 0.3f);
  }

  /* ── Material lookup ─────────────────────────────────────────────── */
  RtMaterial mat = scene->materials[hit.material_index];

  /* ── Lighting (diffuse + specular + shadows, from L05-L07) ──────── */
  Vec3 view_dir = vec3_negate(ray.direction);
  LightingResult lr = compute_lighting(hit.point, hit.normal, view_dir,
                                       &mat, scene);

  Vec3 diffuse_color  = vec3_scale(mat.diffuse_color, lr.diffuse_intensity);
  Vec3 specular_color = vec3_make(lr.specular_intensity,
                                   lr.specular_intensity,
                                   lr.specular_intensity);

  /* ── Reflections (new in L08) ────────────────────────────────────── */
  Vec3 reflect_color = vec3_make(0.0f, 0.0f, 0.0f);
  if (mat.albedo[2] > 0.0f) {
    Vec3 reflect_dir = vec3_normalize(vec3_reflect(ray.direction, hit.normal));
    Vec3 reflect_orig = (vec3_dot(reflect_dir, hit.normal) < 0.0f)
                          ? vec3_sub(hit.point, vec3_scale(hit.normal, 1e-3f))
                          : vec3_add(hit.point, vec3_scale(hit.normal, 1e-3f));
    RtRay reflect_ray = { .origin = reflect_orig, .direction = reflect_dir };
    reflect_color = cast_ray(reflect_ray, scene, depth + 1);  /* RECURSION */
  }

  /* ── Material blending ──────────────────────────────────────────── */
  return vec3_add(
    vec3_add(vec3_scale(diffuse_color,  mat.albedo[0]),
             vec3_scale(specular_color, mat.albedo[1])),
    vec3_scale(reflect_color, mat.albedo[2])
  );
}
```

**Key lines:**
- Lines 10-11: Two base cases combined. `depth > MAX_RECURSION_DEPTH` stops infinite recursion. `!scene_intersect(...)` returns background for rays that miss all objects.
- Lines 14-16: Background gradient. `ray.direction.y` maps vertical position to a color blend. This is what the mirror reflects when looking at "sky".
- Line 33: `mat.albedo[2] > 0.0f` — only compute reflections for materials that are reflective. Red rubber (`albedo[2] = 0.0`) skips this block entirely.
- Line 34: `vec3_reflect` reuses the same formula from L06 (Phong specular), but here the input is the *ray direction*, not the *light direction*.
- Lines 35-37: Epsilon offset, identical technique to shadow rays (L07). Prevents the reflected ray from intersecting the surface it just bounced off.
- Line 39: **The recursive call.** `depth + 1` increments the bounce counter. The returned color is whatever the reflected ray "sees" — another object, the background, or (if it hits another mirror) the result of yet another reflection.
- Lines 43-46: Material blending. Three components weighted by `albedo[0..2]`. The mirror material has `albedo = {0.0, 10.0, 0.8}`, so only specular and reflection contribute.

### Simplified function signatures (L08 vs final)

| Function | L08 (this lesson) | L12+ (final) |
|----------|-------------------|--------------|
| `cast_ray(ray, scene, depth)` | 3 args | 4 args: adds `const RenderSettings *settings` |
| `scene_intersect(ray, scene, &hit)` | 3 args | 5 args: adds `&voxel_color, settings` |
| `compute_lighting(...)` | 5 args | 6 args: adds `settings` |

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Segfault or stack overflow | No depth limit on recursion | Check `depth > MAX_RECURSION_DEPTH` before recursing |
| Mirror sphere is black | `albedo[2]` is 0.0 in material definition | Set mirror material: `albedo = {0.0, 10.0, 0.8, 0.0}` |
| Reflected image appears distorted/noisy | Missing epsilon offset on reflection ray | Add offset: `vec3_add(hit.point, vec3_scale(hit.normal, 1e-3f))` |
| Mirror shows only background (no objects) | Reflecting with `hit.normal` instead of using `vec3_reflect` | Reflect direction: `vec3_reflect(ray.direction, hit.normal)` |
| Non-mirror spheres now look darker | Forgot `albedo[0]` weight on diffuse color | Multiply: `vec3_scale(diffuse_color, mat.albedo[0])` |

## Exercise

> Change `MAX_RECURSION_DEPTH` from 3 to 1 and observe how the mirror sphere looks different — reflections of reflections disappear. Then try depth 0 — the mirror becomes a flat background-colored sphere. Restore to 3 when done.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `CubeCamera` + `envMap` (Three.js reflections) | `cast_ray(reflect_ray, scene, depth+1)` — recursive ray | No render-to-texture; recursion is the reflection mechanism |
| `material.metalness = 1.0` | `albedo[2] = 0.8` — reflection weight in material | Explicit weight per component instead of PBR metalness/roughness |
| `function castRay(ray, depth) { ... return castRay(reflectRay, depth+1); }` | Identical structure in C — recursion works the same | C recursion uses the call stack; 3 levels of `Vec3` + `HitRecord` is ~200 bytes, well within limits |
| `if (depth > MAX_DEPTH) return background` | Same pattern — base case stops recursion | Identical recursion control |
