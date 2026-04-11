# Lesson 07 — Hard Shadows

> **What you'll build:** By the end of this lesson, spheres cast dark shadows on each other and on the scene background. Areas blocked from a light source receive no illumination from that light.

## Observable outcome

Dark shadow regions appear where one sphere blocks light from reaching another surface. The red rubber sphere casts a visible shadow on the ivory sphere. Shadows on the ground between spheres are clearly visible. The scene now has significantly more depth and realism compared to the flat Phong shading from Lesson 06.

## New concepts

- Shadow ray: cast from the hit point toward each light source to test for occlusion
- Epsilon offset `point + N * 1e-3` (shadow acne prevention) — nudge the ray origin slightly off the surface
- Occlusion test: if `scene_intersect` hits something closer than the light, skip that light's contribution

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/lighting.c` | Modified | Add shadow ray test before accumulating each light's contribution |

## Background — why this works

### JS analogy

In a web-based scene (e.g., Three.js), shadows are computed automatically by the renderer when you enable `castShadow` / `receiveShadow` on objects. Under the hood, the GPU renders a depth map from each light's perspective (shadow mapping). In our CPU raytracer, the approach is simpler and more direct: for each hit point, we literally ask "can I see the light from here?" by casting a ray toward the light.

### The shadow ray algorithm

When computing lighting at a surface point, we already have the light direction and distance. Before adding that light's contribution, we cast a **shadow ray** from the hit point toward the light. If that ray hits any object before reaching the light, the point is in shadow (for that particular light).

```
     Light
       *
      /|
     / |
    /  |   <-- shadow ray
   /   |
  /    * Blocking sphere
 /
* Hit point (in shadow)
```

The key insight: we are not asking "where does the shadow fall?" (that is the rasterizer's approach). We are asking "is this specific point illuminated?" — the natural raytracing question.

### Shadow acne

A subtle but critical bug: if you cast the shadow ray starting exactly at the hit point, floating-point imprecision can cause the ray to intersect the *same* surface it just hit (at t very close to 0). The result is speckled noise called **shadow acne** — random dark dots all over every surface.

The fix is simple: offset the ray origin slightly along the surface normal:

```c
Vec3 shadow_orig = vec3_add(point, vec3_scale(normal, 1e-3f));
```

The `1e-3f` (0.001) is small enough to be invisible but large enough to clear floating-point imprecision. This constant is called `SHADOW_EPSILON`.

### Which direction to offset?

The offset direction depends on whether the light is on the same side as the normal. If `dot(light_dir, normal) < 0`, the light is on the *back* side of the surface — offset *away* from the normal. Otherwise, offset *toward* the normal (the common case):

```c
Vec3 shadow_orig = (vec3_dot(light_dir, normal) < 0.0f)
    ? vec3_sub(point, vec3_scale(normal, SHADOW_EPSILON))
    : vec3_add(point, vec3_scale(normal, SHADOW_EPSILON));
```

This bidirectional offset matters for refractive objects later (L09), where rays can hit a surface from inside.

### The occlusion test

After casting the shadow ray, check whether the nearest hit is closer than the light:

```c
if (scene_intersect(shadow_ray, scene, &shadow_hit)) {
  if (shadow_hit.t < light_dist)
    continue;  /* skip this light — occluded */
}
```

The `continue` skips to the next light in the loop. A point might be in shadow for one light but illuminated by another — each light is tested independently.

## Code walkthrough

### `game/lighting.c` — shadow ray addition

This is the simplified L07 version of `compute_lighting`. In later lessons (L12), a `RenderSettings *settings` parameter will be added to make shadows toggleable. For now, shadows are always on.

```c
#include "lighting.h"
#include "intersect.h"
#include "ray.h"
#include <math.h>

#define SHADOW_EPSILON 1e-3f

LightingResult compute_lighting(Vec3 point, Vec3 normal, Vec3 view_dir,
                                const RtMaterial *material,
                                const Scene *scene) {
  LightingResult result = {0.0f, 0.0f};

  for (int i = 0; i < scene->light_count; i++) {
    Vec3  light_dir  = vec3_normalize(vec3_sub(scene->lights[i].position, point));
    float light_dist = vec3_length(vec3_sub(scene->lights[i].position, point));

    /* ── Shadow ray ──────────────────────────────────────────────────── */
    Vec3 shadow_orig = (vec3_dot(light_dir, normal) < 0.0f)
                        ? vec3_sub(point, vec3_scale(normal, SHADOW_EPSILON))
                        : vec3_add(point, vec3_scale(normal, SHADOW_EPSILON));

    RtRay shadow_ray = { .origin = shadow_orig, .direction = light_dir };
    HitRecord shadow_hit;

    if (scene_intersect(shadow_ray, scene, &shadow_hit)) {
      if (shadow_hit.t < light_dist) continue;  /* occluded — skip light */
    }

    /* ── Diffuse (unchanged from L05) ────────────────────────────── */
    float diff = vec3_dot(light_dir, normal);
    if (diff > 0.0f) {
      result.diffuse_intensity += scene->lights[i].intensity * diff;
    }

    /* ── Specular (unchanged from L06) ───────────────────────────── */
    Vec3 reflect_dir = vec3_reflect(vec3_negate(light_dir), normal);
    float spec = vec3_dot(reflect_dir, view_dir);
    if (spec > 0.0f) {
      result.specular_intensity += powf(spec, material->specular_exponent)
                                   * scene->lights[i].intensity;
    }
  }

  return result;
}
```

**Key lines:**
- Line 8: `SHADOW_EPSILON 1e-3f` — the offset distance. Too small (1e-7) causes shadow acne; too large (0.1) visibly separates shadows from objects.
- Lines 18-20: Bidirectional epsilon offset. The ternary checks which side of the surface the light is on.
- Line 22: `RtRay shadow_ray` uses C99 designated initializer. The ray starts at the offset point and points toward the light.
- Line 25-26: The occlusion test. `shadow_hit.t < light_dist` means something is between the point and the light. `continue` skips to the next light — the diffuse and specular calculations below are never reached for this light.

**What does NOT change:** The `scene_intersect` call on line 25 uses the same function from L04 — it already finds the nearest hit across all spheres. No new intersection code is needed; shadows piggyback on existing infrastructure.

### Simplified function signatures (L07 vs final)

In this lesson, `compute_lighting` and `scene_intersect` have simpler signatures than the final codebase:

| Function | L07 (this lesson) | L12+ (final) |
|----------|-------------------|--------------|
| `compute_lighting(...)` | 5 args: `point, normal, view_dir, material, scene` | 6 args: adds `const RenderSettings *settings` |
| `scene_intersect(...)` | 3 args: `ray, scene, &hit` | 5 args: adds `&voxel_color, settings` |

The `RenderSettings` parameter (added in L12) allows toggling shadows on/off with the H key. For now, shadows are always computed.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Speckled dark dots on all surfaces (shadow acne) | Shadow ray origin is exactly on the surface | Add epsilon offset: `vec3_add(point, vec3_scale(normal, SHADOW_EPSILON))` |
| No shadows visible at all | Forgot the `continue` — light contributes even when occluded | Add `if (shadow_hit.t < light_dist) continue;` |
| Shadows appear on wrong side of objects | Epsilon offset always in one direction | Use bidirectional offset based on `dot(light_dir, normal)` sign |
| Scene is completely dark | Shadow ray cast in wrong direction (away from light) | `light_dir` should point *toward* the light: `normalize(light_pos - point)` |
| Compile error: `scene_intersect` argument mismatch | Using final signature with 5 args | At L07, `scene_intersect` takes 3 args: `ray, scene, &hit` |

## Exercise

> Temporarily set `SHADOW_EPSILON` to `0.0f` and observe the shadow acne pattern. Then try `0.1f` — you should see shadows detach slightly from object edges. Find the sweet spot where shadows look correct without visible artifacts. The default `1e-3f` is a good balance.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `mesh.castShadow = true` (Three.js) | Shadow ray cast per light per hit point | No GPU shadow maps; literal ray-intersection test |
| `if (isOccluded) continue;` | Same `continue` keyword | Identical loop control |
| `const EPSILON = 0.001` | `#define SHADOW_EPSILON 1e-3f` | C preprocessor constant; `1e-3f` is float scientific notation for 0.001 |
| `ray.origin.add(normal.multiplyScalar(eps))` | `vec3_add(point, vec3_scale(normal, SHADOW_EPSILON))` | Explicit function calls; no method chaining |
