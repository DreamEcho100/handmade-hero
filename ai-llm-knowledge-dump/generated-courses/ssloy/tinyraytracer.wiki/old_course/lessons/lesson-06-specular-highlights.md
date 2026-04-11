# Lesson 06 — Specular Highlights

> **What you'll build:** By the end of this lesson, shiny spheres have bright white highlights — the specular component of the Phong reflection model.

## Observable outcome

The ivory sphere now has bright white spots where the light reflects directly toward the camera. These highlights are small and tight (specular exponent = 50). The red rubber sphere has very faint, broad highlights (specular exponent = 10) — it looks matte. The mirror sphere has extremely tight highlights (exponent = 1425) — almost pinpoint. The difference between "shiny" and "matte" is now visible.

## New concepts

- `vec3_reflect(I, N)` — the reflection formula `I - 2 * dot(I, N) * N`, which mirrors a vector across a surface normal
- Phong specular — `pow(max(0, dot(R, V)), specular_exponent) * intensity`, where R is the reflected light direction and V is the view direction
- `specular_exponent` in `RtMaterial` — controls highlight tightness: high values (50-1425) give small, sharp highlights; low values (10) give broad, dull ones

## Files changed

| File               | Change type | Summary                                             |
| ------------------ | ----------- | --------------------------------------------------- |
| `game/vec3.h`      | Modified    | Add `vec3_reflect` function                         |
| `game/lighting.c`  | Modified    | Add specular computation to `compute_lighting` loop |
| `game/raytracer.c` | Modified    | Use both diffuse and specular from `LightingResult` |

## Background — why this works

### JS analogy

In Three.js, you'd set `material.shininess = 50` and the engine computes specular highlights automatically. In our raytracer, we implement the same Phong model by hand: reflect the light direction across the surface normal, dot it with the view direction, and raise to a power.

### The reflection formula

Given an incident vector `I` pointing TOWARD a surface and a surface normal `N`, the reflected vector `R` is:

```
R = I - 2 * dot(I, N) * N
```

Visually: `dot(I, N)` measures how much `I` goes "into" the surface. Multiplying by `2 * N` gives the component that needs to be flipped. Subtracting it from `I` mirrors the vector across the normal.

This formula appears twice in our raytracer:

1. **Here in L06** — reflecting the light direction to compute specular highlights
2. **Later in L08** — reflecting the view ray to trace recursive reflections

### Phong specular — why `pow` creates highlights

The specular term is:

```
specular = pow(max(0, dot(R, V)), exponent) * intensity
```

Where:

- `R` = reflected light direction (light bounced off the surface)
- `V` = view direction (from surface toward camera)
- `exponent` = specular exponent from the material

The dot product `dot(R, V)` is close to 1.0 when the reflected light points directly at the camera, and falls off to 0 as the angle increases. `pow(x, 50)` makes this falloff extremely steep — at 50 degrees off-axis, `pow(cos(50), 50)` is nearly zero. This creates a small, bright spot.

Comparing materials:

- **Ivory** (exponent = 50): medium highlight. Looks like a polished stone.
- **Red rubber** (exponent = 10): broad, faint highlight. Looks like rubber or plastic.
- **Mirror** (exponent = 1425): pinpoint highlight. Looks like polished metal.

### The complete Phong model so far

After this lesson, each pixel's color is:

```
final_color = diffuse_color * diffuse_intensity * albedo[0]
            + specular_white * specular_intensity * albedo[1]
```

The specular highlight is always white (or light-colored) because it represents a mirror-like reflection of the light source itself, not the surface color. This is why `specular_color` is `vec3_make(spec, spec, spec)` — a uniform gray/white.

## Code walkthrough

### `game/vec3.h` — vec3_reflect (added to existing file)

```c
/* ── LESSON 06 — Reflect ─────────────────────────────────────────────────
 * reflect(I, N) = I - 2*dot(I,N)*N
 * Used for Phong specular highlights (L06) and recursive reflections (L08).
 *
 * The formula: given incident direction I and surface normal N,
 * the reflected direction mirrors I across N.                             */
static inline Vec3 vec3_reflect(Vec3 incident, Vec3 normal) {
  return vec3_sub(incident, vec3_scale(normal, 2.0f * vec3_dot(incident, normal)));
}
```

**Key lines:**

- `vec3_dot(incident, normal)` — projection of the incident vector onto the normal. For a vector pointing INTO the surface, this is negative (the vectors point in opposite directions).
- `2.0f * dot(I, N)` — twice the projection. This is the "overshoot" needed to flip the component.
- `vec3_scale(normal, 2 * dot)` — the correction vector along the normal.
- `vec3_sub(incident, correction)` — subtract to flip the normal component, keeping the tangential component unchanged.
- The function takes `incident` by value (12 bytes) — small enough for register passing. No allocation needed.

### `game/lighting.c` — specular added to compute_lighting (complete function)

```c
#include "lighting.h"
#include <math.h>

LightingResult compute_lighting(Vec3 point, Vec3 normal, Vec3 view_dir,
                                const RtMaterial *material,
                                const Scene *scene) {
  LightingResult result = {0.0f, 0.0f};

  for (int i = 0; i < scene->light_count; i++) {
    Vec3  light_dir = vec3_normalize(vec3_sub(scene->lights[i].position, point));

    /* ── Diffuse ─────────────────────────────────────────────────────── */
    float diff = vec3_dot(light_dir, normal);
    if (diff > 0.0f) {
      result.diffuse_intensity += scene->lights[i].intensity * diff;
    }

    /* ── Specular ────────────────────────────────────────────────────── */
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

- `vec3_negate(light_dir)` — the reflect formula expects the incident vector pointing TOWARD the surface. `light_dir` points FROM the surface toward the light, so we negate it.
- `vec3_reflect(negated_light, normal)` — the reflected light direction. This is the direction the light would bounce if the surface were a perfect mirror.
- `vec3_dot(reflect_dir, view_dir)` — how closely the reflected light points at the camera. Close to 1.0 = bright highlight. Close to 0.0 = no highlight.
- `powf(spec, material->specular_exponent)` — the power function sharpens the falloff. `powf(0.9, 10) = 0.35` (broad highlight), `powf(0.9, 50) = 0.005` (tight highlight). `powf` is the float version of `pow` (requires `-lm`).
- `* scene->lights[i].intensity` — brighter lights produce brighter highlights.
- The specular term is accumulated separately from diffuse. The caller combines them with the material's albedo weights.

### `game/raytracer.c` — using both diffuse and specular (relevant changes)

```c
if (scene_intersect(ray, scene, &hit)) {
  RtMaterial mat = scene->materials[hit.material_index];
  Vec3 view_dir = vec3_negate(ray.direction);

  LightingResult lr = compute_lighting(hit.point, hit.normal, view_dir,
                                       &mat, scene);

  Vec3 diffuse_color  = vec3_scale(mat.diffuse_color, lr.diffuse_intensity);
  Vec3 specular_color = vec3_make(lr.specular_intensity,
                                   lr.specular_intensity,
                                   lr.specular_intensity);

  /* Material blending: diffuse * albedo[0] + specular * albedo[1] */
  color = vec3_add(vec3_scale(diffuse_color,  mat.albedo[0]),
                   vec3_scale(specular_color, mat.albedo[1]));
}
```

**Key lines:**

- `specular_color` is a uniform `(spec, spec, spec)` — white/gray. Specular highlights reflect the light source color, not the surface color. Since our lights are white, the highlight is white.
- `mat.albedo[0]` scales the diffuse contribution. Ivory has `albedo[0] = 0.6` — 60% of the final color comes from diffuse.
- `mat.albedo[1]` scales the specular contribution. Ivory has `albedo[1] = 0.3` — 30% from specular. Red rubber has `albedo[1] = 0.1` — only 10%, so highlights are faint.
- `vec3_add(diffuse, specular)` — the two components are simply added together. Specular adds ON TOP of diffuse, which is why highlights can push the color above 1.0 (it gets clamped at the pixel-write step).

## Common mistakes

| Symptom                                         | Cause                                                     | Fix                                                                                                             |
| ----------------------------------------------- | --------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| No highlights visible                           | `spec` is always <= 0 because `vec3_negate` was forgotten | Must negate `light_dir` before passing to `vec3_reflect` — the formula expects incident pointing toward surface |
| Highlights on the wrong side                    | Normal is flipped (points inward)                         | Check `sphere_intersect`: normal = `normalize(hit_point - center)`, not `center - hit_point`                    |
| Highlights are too broad (everything looks wet) | `specular_exponent` is too low (e.g., 1.0)                | Ivory should be 50.0, rubber 10.0 — check `scene_init` material values                                          |
| Highlights look the same on ivory and rubber    | Both materials have the same `albedo[1]` weight           | Ivory `albedo[1] = 0.3`, rubber `albedo[1] = 0.1` — different specular weights                                  |
| `powf` produces `NaN`                           | Negative base passed to `powf` (spec < 0 not clamped)     | The `if (spec > 0.0f)` guard prevents this — ensure it's present                                                |

## Exercise

> Change the ivory material's `specular_exponent` from 50 to 500. The highlight should become a tiny bright pinpoint. Then change it to 5 — the highlight should spread across half the sphere. This demonstrates the exponent's visual effect. Try to match the look of materials around you: a plastic bottle (exp ~30), a metal spoon (exp ~200), a rubber eraser (exp ~5).

## JS ↔ C concept map

| JS / Web concept                             | C equivalent in this lesson                            | Key difference                                                      |
| -------------------------------------------- | ------------------------------------------------------ | ------------------------------------------------------------------- |
| `THREE.MeshPhongMaterial({ shininess: 50 })` | `mat.specular_exponent = 50.0f` in `RtMaterial`        | Manual Phong implementation; no engine magic                        |
| `reflect(incident, normal)` (GLSL built-in)  | `vec3_reflect(incident, normal)`                       | Same formula: `I - 2*dot(I,N)*N`; C has no built-in, so we write it |
| `Math.pow(base, exp)`                        | `powf(base, exp)`                                      | `powf` for float (not `pow` which is double); `-lm` required        |
| `Math.max(0, dot)`                           | `if (spec > 0.0f) { ... }`                             | Guard clause instead of `fmaxf`                                     |
| `color = diffuse + specular`                 | `vec3_add(vec3_scale(diff, w0), vec3_scale(spec, w1))` | Weighted sum; each component scaled by its albedo weight            |
