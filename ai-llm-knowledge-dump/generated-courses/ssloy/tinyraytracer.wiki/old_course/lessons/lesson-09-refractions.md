# Lesson 09 — Refractions

> **What you'll build:** By the end of this lesson, the glass sphere bends light through it, showing a distorted view of the scene behind it. This completes the four-component material model.

## Observable outcome

The fourth sphere (glass material, refractive_index=1.5) now visibly distorts the scene behind it. Objects seen through the glass appear shifted, magnified, and inverted — the hallmark of refraction. The glass sphere also retains specular highlights and a slight reflection (from L08), giving it a convincing transparent appearance.

## New concepts

- Snell's law `refract()`: computes the refracted (bent) direction using `eta * I + (eta * cos_i - cos_t) * N`
- Inside/outside detection: `dot(I, N) < 0` means outside the surface; flip normal and swap eta ratio when inside
- Total internal reflection check: when `1 - eta^2 * (1 - cos^2)` is negative, no refraction is possible
- Material blending extended with `albedo[3]`: `+ refract_color * albedo[3]`

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/refract.h` | Created | `refract()` function implementing Snell's law (header-only, static inline) |
| `game/raytracer.c` | Modified | Add refraction block after reflection; extend material blending to include `albedo[3]` |
| `game/scene.h` | Modified | Glass material now uses `refractive_index = 1.5` and `albedo[3] = 0.8` |

## Background — why this works

### JS analogy

In Three.js, you get refraction with `MeshPhysicalMaterial({ transmission: 1.0, ior: 1.5 })`. The GPU handles Snell's law internally in the shader. In our CPU raytracer, we implement Snell's law ourselves as a function that computes the bent ray direction from the incident direction, surface normal, and refractive index ratio.

### Snell's law

When light passes from one medium (air, index=1.0) into another (glass, index=1.5), it bends. The relationship between angles is:

```
n1 * sin(theta1) = n2 * sin(theta2)
```

Where `n1` and `n2` are the refractive indices, and `theta1`/`theta2` are the angles from the surface normal.

We never compute angles directly. Instead, the formula works with vectors:

```
refracted = eta * I + (eta * cos_i - sqrt(k)) * N
```

Where:
- `eta = n1 / n2` (ratio of refractive indices)
- `cos_i = -dot(I, N)` (cosine of incident angle)
- `k = 1 - eta^2 * (1 - cos_i^2)` (discriminant)

If `k < 0`, refraction is impossible (total internal reflection).

### Inside/outside detection

A ray can hit the glass sphere from outside (entering) or from inside (exiting). The sign of `dot(incident, normal)` tells us which:

- `dot(I, N) < 0`: ray comes from outside. Normal points away from ray. Use `eta = air/glass = 1.0/1.5`.
- `dot(I, N) > 0`: ray is inside the object. Flip the normal (negate it) and swap: `eta = glass/air = 1.5/1.0`.

This flip is essential. Without it, light enters the glass correctly but exits without bending, or bends in the wrong direction.

### Total internal reflection

When light travels from a denser medium to a less dense one (glass to air), at steep angles the math produces `k < 0` — no real solution exists. This is **total internal reflection**: the light bounces off the inside surface instead of exiting. In our implementation, `refract()` returns a zero vector in this case, and the normalize + ray cast produces a negligible contribution.

### The glass material

```c
/* glass — refractive + reflective */
s->materials[3] = (RtMaterial){
  .diffuse_color     = vec3_make(0.6f, 0.7f, 0.8f),
  .albedo            = {0.0f, 0.5f, 0.1f, 0.8f},
  .specular_exponent = 125.0f,
  .refractive_index  = 1.5f,
};
```

The albedo weights: no diffuse (transparent objects don't show their own color), strong specular (glass is shiny), slight reflection (10%), and dominant refraction (80%). Real glass has both reflection and refraction — the balance depends on viewing angle (Fresnel effect), but a fixed 10%/80% split looks convincing.

## Code walkthrough

### `game/refract.h` — complete file

```c
#ifndef GAME_REFRACT_H
#define GAME_REFRACT_H

#include "vec3.h"
#include <math.h>

static inline Vec3 refract(Vec3 incident, Vec3 normal, float refractive_index) {
  float cos_i = -vec3_dot(incident, normal);
  float eta_i = 1.0f;  /* air */
  float eta_t = refractive_index;

  /* If cos_i < 0, the ray is inside the object — flip everything. */
  if (cos_i < 0.0f) {
    cos_i  = -cos_i;
    normal = vec3_negate(normal);
    float tmp = eta_i;
    eta_i = eta_t;
    eta_t = tmp;
  }

  float eta = eta_i / eta_t;
  float k   = 1.0f - eta * eta * (1.0f - cos_i * cos_i);

  /* Total internal reflection — return zero vector (caller handles). */
  if (k < 0.0f) {
    return vec3_make(0.0f, 0.0f, 0.0f);
  }

  /* Snell's law: refracted = eta * I + (eta * cos_i - sqrt(k)) * N */
  return vec3_add(
    vec3_scale(incident, eta),
    vec3_scale(normal, eta * cos_i - sqrtf(k))
  );
}

#endif /* GAME_REFRACT_H */
```

**Key lines:**
- Line 8: `cos_i = -vec3_dot(incident, normal)` — the incident ray points *toward* the surface, so the dot product with the outward normal is negative. The negation gives us the positive cosine of the angle.
- Lines 13-19: Inside/outside detection. If `cos_i < 0` after the negation, the ray was already inside the object. We flip the normal and swap `eta_i`/`eta_t`. This is a simple `tmp` swap — C has no destructuring assignment.
- Line 22: `k = 1 - eta^2 * (1 - cos_i^2)` — this is `sin^2(theta_t)` derived from Snell's law. If negative, the refracted angle would exceed 90 degrees, which is impossible.
- Lines 26-27: Total internal reflection fallback. Returning zero is safe because the caller normalizes the result; if zero, the refracted ray contributes nothing meaningful.
- Lines 31-33: The actual Snell's law computation. The result is the direction the refracted ray travels.

### `game/raytracer.c` — refraction added to cast_ray

Below is the refraction block added after the reflection block from L08. The full material blending now includes all four albedo components.

```c
  /* ── Reflections (from L08) ────────────────────────────────────────── */
  Vec3 reflect_color = vec3_make(0.0f, 0.0f, 0.0f);
  if (mat.albedo[2] > 0.0f) {
    Vec3 reflect_dir = vec3_normalize(vec3_reflect(ray.direction, hit.normal));
    Vec3 reflect_orig = (vec3_dot(reflect_dir, hit.normal) < 0.0f)
                          ? vec3_sub(hit.point, vec3_scale(hit.normal, 1e-3f))
                          : vec3_add(hit.point, vec3_scale(hit.normal, 1e-3f));
    RtRay reflect_ray = { .origin = reflect_orig, .direction = reflect_dir };
    reflect_color = cast_ray(reflect_ray, scene, depth + 1);
  }

  /* ── Refractions (new in L09) ──────────────────────────────────────── */
  Vec3 refract_color = vec3_make(0.0f, 0.0f, 0.0f);
  if (mat.albedo[3] > 0.0f) {
    Vec3 refract_dir = vec3_normalize(refract(ray.direction, hit.normal,
                                              mat.refractive_index));
    Vec3 refract_orig = (vec3_dot(refract_dir, hit.normal) < 0.0f)
                          ? vec3_sub(hit.point, vec3_scale(hit.normal, 1e-3f))
                          : vec3_add(hit.point, vec3_scale(hit.normal, 1e-3f));
    RtRay refract_ray = { .origin = refract_orig, .direction = refract_dir };
    refract_color = cast_ray(refract_ray, scene, depth + 1);
  }

  /* ── Material blending (extended with albedo[3]) ───────────────────── */
  return vec3_add(
    vec3_add(vec3_scale(diffuse_color,  mat.albedo[0]),
             vec3_scale(specular_color, mat.albedo[1])),
    vec3_add(vec3_scale(reflect_color,  mat.albedo[2]),
             vec3_scale(refract_color,  mat.albedo[3]))
  );
```

**Key lines:**
- Line 14: `mat.albedo[3] > 0.0f` — only glass has a non-zero refraction weight. Ivory, rubber, and mirror skip this block.
- Lines 15-16: `refract()` computes the bent direction. `vec3_normalize` ensures unit length (the math can produce slightly non-unit vectors due to floating-point).
- Lines 17-19: Same epsilon offset pattern as reflections (L08) and shadows (L07). For refraction, the offset direction is critical — a refracted ray enters the object, so the origin must be nudged *into* the surface (opposite the normal).
- Line 21: Another recursive `cast_ray` call. The refracted ray continues through the glass and exits the other side, where it hits `refract()` again (this time from inside, with flipped eta).
- Lines 25-29: Full four-component blending. The `vec3_add` nesting is verbose but clear — each component is independently scaled and summed.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Glass sphere is opaque (shows diffuse color) | `albedo[3]` is 0.0 | Set glass `albedo = {0.0, 0.5, 0.1, 0.8}` — the 0.8 enables refraction |
| Glass shows no distortion (objects pass straight through) | `refractive_index` is 1.0 (same as air) | Set glass `refractive_index = 1.5` |
| Dark band around glass edge | Missing inside/outside detection | Check `cos_i < 0` and flip normal + swap eta |
| Glass sphere is completely black | Refracted ray origin is on wrong side of surface | Use bidirectional epsilon offset based on `dot(refract_dir, hit.normal)` |
| `refract.h` not found | Missing `#include "refract.h"` in `raytracer.c` | Add the include at the top of `raytracer.c` |

## Exercise

> Change the glass sphere's `refractive_index` from 1.5 (glass) to 2.42 (diamond) and observe how the distortion changes — diamond bends light much more dramatically. Then try 1.0 (air) — the sphere becomes invisible (no bending at all). Restore to 1.5 when done.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `MeshPhysicalMaterial({ transmission: 1.0, ior: 1.5 })` | `albedo[3] = 0.8`, `refractive_index = 1.5` | Explicit separate controls for weight and index |
| `Math.sqrt(k)` | `sqrtf(k)` | `sqrtf` is the float variant; requires `-lm` |
| `[eta_i, eta_t] = [eta_t, eta_i]` (destructuring swap) | `float tmp = eta_i; eta_i = eta_t; eta_t = tmp;` | C has no destructuring; use a temp variable |
| `if (k < 0) return null` (total internal reflection) | `if (k < 0.0f) return vec3_make(0,0,0)` | C has no null for value types; return zero vector |
