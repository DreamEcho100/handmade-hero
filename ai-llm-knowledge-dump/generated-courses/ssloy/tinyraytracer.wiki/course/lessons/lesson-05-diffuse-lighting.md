# Lesson 05 — Diffuse Lighting

> **What you'll build:** By the end of this lesson, spheres look three-dimensional — lit from one side and dark on the other.

## Observable outcome

The four spheres are no longer flat-colored circles. Each sphere now has shading: the side facing the lights is bright, and the side facing away is dark. Three point lights illuminate the scene from different positions. The ivory sphere appears warm beige on its lit side and dark on the shadow side. The red rubber sphere shows the same light/dark pattern in dark red. The spheres now look like real 3D objects instead of colored discs.

## New concepts

- `Light` struct — a point light with a position in world space and a scalar intensity
- Lambertian diffuse shading — `max(0, dot(light_dir, N)) * intensity` gives the cosine falloff of light on a surface
- Surface normal computation — at any point on a sphere, the normal is `normalize(hit_point - sphere_center)`, pointing outward

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/lighting.h` | Created | `LightingResult` struct, `compute_lighting` declaration |
| `game/lighting.c` | Created | Diffuse lighting computation (loop over lights) |
| `game/scene.h` | Modified | Add `Light` struct, `MAX_LIGHTS`, lights array in `Scene`, 3 lights in `scene_init` |
| `game/main.c` | Modified | Render loop now calls `compute_lighting` and uses result to shade pixels |

## Background — why this works

### JS analogy

In JavaScript (e.g., Three.js), you'd write `new THREE.PointLight(0xffffff, 1.5)` and the engine handles all the math. Under the hood, it's doing exactly what we'll implement: for each pixel, compute the angle between the surface normal and the light direction, then multiply by intensity.

In C, we do this explicitly. There's no lighting engine — just `vec3_dot(light_dir, normal)` and some multiplication.

### Lambertian diffuse — why the dot product gives realistic shading

The dot product of two unit vectors equals the cosine of the angle between them:

```
dot(A, B) = cos(theta)
```

When a surface faces directly toward a light (`theta = 0`), `cos(0) = 1.0` — full brightness. When the surface is at 90 degrees to the light, `cos(90) = 0.0` — no light. When the surface faces away (`theta > 90`), `cos` goes negative — we clamp to 0 (no negative light).

This cosine falloff is called **Lambert's cosine law**. It models how real surfaces scatter light: a surface hit at a grazing angle receives less light per unit area than one hit head-on. This single dot product is why shaded spheres look round.

### The lighting formula

For each light in the scene:
```
diffuse_contribution = light.intensity * max(0, dot(light_dir, surface_normal))
```

We sum contributions from all lights. Multiple lights create richer shading — an area lit by two lights is brighter than one lit by just one.

### Where does the surface normal come from?

For a sphere, the normal at any point is trivial: it points from the center to the hit point, normalized to length 1:

```c
normal = normalize(hit_point - sphere_center)
```

We already compute this in `sphere_intersect` (Lesson 03) and store it in `hit->normal`. No extra work needed — the intersection code provides everything the lighting code needs.

### Light direction

The light direction at a surface point is:
```c
light_dir = normalize(light.position - hit_point)
```

This is the direction FROM the surface point TOWARD the light. We need this direction because `dot(light_dir, normal)` gives us the cosine of the angle between the light and the surface.

## Code walkthrough

### `game/scene.h` — Light struct and lights in scene_init (additions)

```c
/* LESSON 05 — Point light with position and intensity. */
typedef struct {
  Vec3  position;
  float intensity;
} Light;

#define MAX_LIGHTS 8

/* Added to Scene struct: */
typedef struct {
  RtMaterial materials[MAX_MATERIALS];
  int      material_count;
  Sphere   spheres[MAX_SPHERES];
  int      sphere_count;
  Light    lights[MAX_LIGHTS];
  int      light_count;
} Scene;

/* Added to scene_init: */
static inline void scene_init(Scene *s) {
  /* ... existing material and sphere setup ... */
  s->light_count = 0;

  /* ── Lights ────────────────────────────────────────────────────────── */
  s->lights[s->light_count++] = (Light){
    .position  = vec3_make(-20.0f, 20.0f, 20.0f),
    .intensity = 1.5f,
  };
  s->lights[s->light_count++] = (Light){
    .position  = vec3_make(30.0f, 50.0f, -25.0f),
    .intensity = 1.8f,
  };
  s->lights[s->light_count++] = (Light){
    .position  = vec3_make(30.0f, 20.0f, 30.0f),
    .intensity = 1.7f,
  };
}
```

**Key lines:**
- `Light` has just two fields: `position` (where the light is in world space) and `intensity` (how bright it is). A point light radiates equally in all directions.
- Three lights create interesting shading: light from the left, above-right, and right-behind. This avoids the flat look of a single light and creates subtle shading gradients.
- `intensity` values above 1.0 are valid — they mean the light is "brighter than normal." The final pixel color gets clamped to [0, 1] before conversion to bytes.

### `game/lighting.h` — complete file (simplified for this lesson)

```c
#ifndef GAME_LIGHTING_H
#define GAME_LIGHTING_H

#include "vec3.h"
#include "scene.h"

typedef struct {
  float diffuse_intensity;
  float specular_intensity;
} LightingResult;

LightingResult compute_lighting(Vec3 point, Vec3 normal, Vec3 view_dir,
                                const RtMaterial *material,
                                const Scene *scene);

#endif /* GAME_LIGHTING_H */
```

**Key lines:**
- `LightingResult` returns two separate intensities: diffuse and specular. For this lesson, only `diffuse_intensity` is computed; `specular_intensity` stays 0. Lesson 06 fills it in.
- `view_dir` is the direction from the surface toward the camera. Not used yet (it's needed for specular), but the signature is designed for the full Phong model.
- `const RtMaterial *material` is passed by pointer (not by value) because the struct is 36+ bytes — too large for efficient register passing.

### `game/lighting.c` — diffuse computation (simplified for this lesson)

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
  }

  return result;
}
```

**Key lines:**
- `vec3_sub(light.position, point)` — vector from the surface point to the light. `vec3_normalize` makes it a unit vector so the dot product gives the pure cosine.
- `vec3_dot(light_dir, normal)` — the heart of diffuse shading. If positive, the surface faces the light. If negative or zero, the surface faces away.
- `if (diff > 0.0f)` — the `max(0, ...)` clamp. We skip negative contributions rather than adding negative light (which would darken the surface incorrectly).
- `result.diffuse_intensity +=` — accumulate contributions from all lights. This is a sum over all lights, not a maximum.
- `(void)view_dir; (void)material;` — these parameters are unused in this lesson's diffuse-only version but are part of the function signature for the specular addition in Lesson 06.

### `game/render.c` — using lighting in the render loop (relevant changes)

```c
/* After scene_intersect finds a hit: */
if (scene_intersect(ray, scene, &hit)) {
  RtMaterial mat = scene->materials[hit.material_index];
  Vec3 view_dir = vec3_negate(ray.direction);

  LightingResult lr = compute_lighting(hit.point, hit.normal, view_dir,
                                       &mat, scene);

  Vec3 diffuse_color = vec3_scale(mat.diffuse_color, lr.diffuse_intensity);

  /* Clamp to [0,1] and write pixel. */
  color = diffuse_color;
}
```

**Key lines:**
- `vec3_negate(ray.direction)` — the view direction points FROM the surface TOWARD the camera. Since `ray.direction` points from camera to surface, negating it gives us the view direction.
- `vec3_scale(mat.diffuse_color, lr.diffuse_intensity)` — multiply the material's base color by the lighting intensity. Ivory `(0.4, 0.4, 0.3)` lit with intensity 2.0 becomes `(0.8, 0.8, 0.6)`. If the intensity exceeds 1.0/component, we clamp when converting to bytes.
- `compute_lighting` returns raw intensity values. The caller decides how to combine them with material properties. This separation lets us add specular, reflections, and refractions in later lessons without changing `compute_lighting`.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Spheres are completely black | `light_dir` computed backwards: `point - light.position` instead of `light.position - point` | Direction must be FROM surface TOWARD light: `vec3_sub(light.position, point)` |
| Spheres have inverted shading (dark side faces light) | Normal points inward: `center - hit_point` instead of `hit_point - center` | Normal in `sphere_intersect` must be `normalize(hit_point - center)` |
| Only one side of sphere is lit | Only one light in `scene_init` | Add all 3 lights; check `light_count` increments |
| Spheres are too bright (washed out white) | Missing clamp before `GAME_RGB` | Clamp each channel to 255 max: `(int)fminf(color.x * 255.0f, 255.0f)` |
| Lighting looks flat (no gradient on sphere) | `light_dir` not normalized | Must `vec3_normalize` the direction; un-normalized dot gives wrong cosine |

## Exercise

> Move all three lights to the same position `(0, 0, 20)` (behind the camera). The spheres should be evenly lit from the front with no dark sides. Then move the lights to `(0, 0, -30)` (behind the spheres). The spheres should appear nearly black because they face away from the light. This demonstrates why light position matters.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `new THREE.PointLight(0xffffff, 1.5)` | `(Light){ .position = ..., .intensity = 1.5f }` | No class; compound literal; manual lighting math |
| `normal.dot(lightDir)` | `vec3_dot(light_dir, normal)` | Free function, not a method |
| `Math.max(0, dotResult)` | `if (diff > 0.0f) { ... }` | Conditional instead of `fmaxf` — same effect, slightly clearer intent |
| `color.multiplyScalar(intensity)` | `vec3_scale(mat.diffuse_color, lr.diffuse_intensity)` | Explicit function call; returns new vector (no mutation) |
| `lights.forEach(light => { ... })` | `for (int i = 0; i < scene->light_count; i++)` | Manual index loop; no callbacks or closures |
