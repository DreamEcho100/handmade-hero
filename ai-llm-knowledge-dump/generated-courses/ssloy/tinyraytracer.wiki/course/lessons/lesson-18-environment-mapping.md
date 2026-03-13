# Lesson 18 — Environment Mapping

> **What you'll build:** By the end of this lesson, rays that miss all scene geometry sample a spherical environment map image (or a procedural sky gradient), replacing the flat background color with a realistic surrounding world.

## Observable outcome

The background changes from a plain gradient to either a loaded HDR/LDR panorama image (if `assets/envmap.jpg` exists) or a procedural three-zone sky (ground brown, horizon haze, blue zenith). The environment map is also visible in reflections — the mirror sphere now reflects the sky and ground instead of a solid color. Pressing **E** toggles between the envmap and the original flat gradient.

## New concepts

- `stb_image` loading — a single-header C library for decoding JPG/PNG/HDR images
- Spherical UV mapping (equirectangular projection) — converting a 3D ray direction to 2D texture coordinates
- Procedural sky fallback — a three-zone `vec3_lerp` gradient that works without any image file
- `envmap_sample` integration in `cast_ray` — sampling the environment when no geometry is hit

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/envmap.h` | Created | `EnvMap` struct; `envmap_init_procedural`, `envmap_load`, `envmap_free`, `envmap_sample` declarations |
| `game/envmap.c` | Created | Includes `stb_image.h` for declarations; spherical UV mapping; procedural sky; `envmap_sample` |
| `utils/stb_image.h` | Added | Single-header image loader (third-party library, declarations only in envmap.c) |
| `utils/stb_image_impl.c` | Created | Defines `STB_IMAGE_IMPLEMENTATION` + includes `stb_image.h` — compiled only for X11 backend |
| `game/scene.h` | Modified | `EnvMap envmap` field added to `Scene`; `envmap_load` called in `scene_init` |
| `game/raytracer.c` | Modified | `cast_ray` calls `envmap_sample` when no hit (replaces hardcoded background) |
| `game/settings.h` | Modified | `show_envmap` toggle added |
| `game/vec3.h` | Modified | `vec3_lerp` added (if not already present) |

## Background — why this works

### JS analogy

In Three.js you set `scene.background = new THREE.CubeTextureLoader().load(urls)` and the engine handles the mapping. In WebGL you would sample a cubemap in the fragment shader. In our C raytracer we do the same thing manually: convert the ray direction to UV coordinates and look up a pixel in a 2D image.

### Spherical UV mapping (equirectangular projection)

An equirectangular environment map is a 2D image that represents the full 360-degree sphere of directions. Given a normalized ray direction `(dx, dy, dz)`:

```
u = 0.5 + atan2(dz, dx) / (2 * PI)
v = 0.5 - asin(dy) / PI
```

This maps:
- **u** (horizontal): `atan2(dz, dx)` gives the azimuth angle (-PI to +PI), mapped to [0, 1]
- **v** (vertical): `asin(dy)` gives the elevation (-PI/2 to +PI/2), mapped to [0, 1]

The `0.5 +` and `0.5 -` offsets center the mapping so that `dir = (0, 0, -1)` (looking forward) maps to the center of the image.

### Why stb_image

`stb_image` is a single-header C library (~7000 lines) that decodes JPG, PNG, BMP, TGA, HDR, and more. We use it because:
1. No external dependencies — just `#include "stb_image.h"`
2. No build system integration needed — define `STB_IMAGE_IMPLEMENTATION` in one .c file
3. Simple API: `stbi_load(filename, &w, &h, &channels, desired_channels)` returns a pixel buffer

The Raylib backend already links stb_image internally, so we only compile the implementation for the X11 backend (via `utils/stb_image_impl.c`). The header `stb_image.h` is included directly by `envmap.c` for declarations.

### Procedural sky

When no image file is available, `envmap_sample` returns a three-zone gradient:

```c
static Vec3 procedural_sky(Vec3 dir) {
  float t = 0.5f * (dir.y + 1.0f);        /* 0=down, 1=up */
  Vec3 ground  = vec3_make(0.35f, 0.25f, 0.15f);  /* brown */
  Vec3 horizon = vec3_make(0.85f, 0.85f, 0.80f);  /* haze  */
  Vec3 zenith  = vec3_make(0.15f, 0.35f, 0.65f);  /* blue  */

  if (t < 0.5f) return vec3_lerp(ground, horizon, t * 2.0f);
  return vec3_lerp(horizon, zenith, (t - 0.5f) * 2.0f);
}
```

The `dir.y` component ranges from -1 (straight down) to +1 (straight up). The `0.5 * (dir.y + 1.0)` maps this to [0, 1]. Below the horizon (t < 0.5), it blends from brown ground to hazy horizon. Above the horizon, it blends from haze to blue zenith.

## Code walkthrough

### `game/envmap.h` — complete file

```c
#ifndef GAME_ENVMAP_H
#define GAME_ENVMAP_H

#include "vec3.h"
#include <stddef.h>  /* NULL */

typedef struct {
  unsigned char *pixels; /* NULL = use procedural sky              */
  int width, height;
  int channels;          /* 3 (RGB) or 4 (RGBA)                   */
} EnvMap;

/* Initialize to procedural sky (no file needed). */
static inline void envmap_init_procedural(EnvMap *e) {
  e->pixels   = NULL;
  e->width    = 0;
  e->height   = 0;
  e->channels = 0;
}

/* Load a spherical environment map from an image file (JPG/PNG/HDR).
 * Returns 0 on success, -1 on failure (falls back to procedural sky).
 * Uses stb_image internally.                                              */
int envmap_load(EnvMap *e, const char *filename);

/* Free loaded image data. */
void envmap_free(EnvMap *e);

/* Sample the environment map by ray direction.
 * If no image is loaded, returns a procedural sky gradient.               */
Vec3 envmap_sample(const EnvMap *e, Vec3 direction);

#endif /* GAME_ENVMAP_H */
```

**Key design:** The `pixels` field being `NULL` signals "use procedural sky." This means the envmap always works — it gracefully degrades if no image file is found. No error handling is needed in `cast_ray`.

### `game/envmap.c` — complete file

```c
#include "envmap.h"

/* stb_image declarations — the implementation is compiled via
 * utils/stb_image_impl.c (X11 backend) or provided by Raylib internally
 * (Raylib backend already links stb_image).                                */
#include "../utils/stb_image.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int envmap_load(EnvMap *e, const char *filename) {
  int w, h, ch;
  unsigned char *data = stbi_load(filename, &w, &h, &ch, 3);
  if (!data) {
    fprintf(stderr, "envmap_load: cannot open '%s' — using procedural sky\n",
            filename);
    envmap_init_procedural(e);
    return -1;
  }
  e->pixels   = data;
  e->width    = w;
  e->height   = h;
  e->channels = 3;
  printf("Loaded envmap: %s (%dx%d)\n", filename, w, h);
  return 0;
}

void envmap_free(EnvMap *e) {
  if (e->pixels) {
    stbi_image_free(e->pixels);
    e->pixels = NULL;
  }
}

/* ── Procedural sky (no file needed) ─────────────────────────────────── */
static Vec3 procedural_sky(Vec3 dir) {
  float t = 0.5f * (dir.y + 1.0f);
  Vec3 ground  = vec3_make(0.35f, 0.25f, 0.15f);
  Vec3 horizon = vec3_make(0.85f, 0.85f, 0.80f);
  Vec3 zenith  = vec3_make(0.15f, 0.35f, 0.65f);

  if (t < 0.5f) return vec3_lerp(ground, horizon, t * 2.0f);
  return vec3_lerp(horizon, zenith, (t - 0.5f) * 2.0f);
}

/* ── Spherical UV mapping (equirectangular projection) ────────────────── */
static void direction_to_uv(Vec3 d, float *u, float *v) {
  *u = 0.5f + atan2f(d.z, d.x) / (2.0f * (float)M_PI);
  *v = 0.5f - asinf(fmaxf(-1.0f, fminf(1.0f, d.y))) / (float)M_PI;
}

Vec3 envmap_sample(const EnvMap *e, Vec3 direction) {
  if (!e->pixels) return procedural_sky(direction);

  float u, v;
  direction_to_uv(direction, &u, &v);

  int px = (int)(u * (float)e->width)  % e->width;
  int py = (int)(v * (float)e->height) % e->height;
  if (px < 0) px += e->width;
  if (py < 0) py += e->height;

  int idx = (py * e->width + px) * e->channels;
  return vec3_make(
    (float)e->pixels[idx + 0] / 255.0f,
    (float)e->pixels[idx + 1] / 255.0f,
    (float)e->pixels[idx + 2] / 255.0f
  );
}
```

**Key lines:**

- **`#include "../utils/stb_image.h"` (line 7):** Includes the stb_image single-header library for declarations (`stbi_load`, `stbi_image_free`). The implementation is compiled separately: `utils/stb_image_impl.c` for X11, or Raylib's built-in copy for the Raylib backend.

- **`stbi_load(filename, &w, &h, &ch, 3)` (line 19):** The last argument `3` forces RGB output regardless of the source format. Even if the image is RGBA or grayscale, stb_image converts to 3 channels. This simplifies the sampling code.

- **`envmap_init_procedural(e)` on failure (line 23):** If the file cannot be loaded, the envmap silently falls back to procedural sky. No crash, no error propagation — just a `fprintf` to stderr for debugging.

- **`fmaxf(-1.0f, fminf(1.0f, d.y))` (line 55):** Clamps `d.y` to [-1, 1] before passing to `asinf`. Floating-point imprecision can produce values slightly outside this range (e.g., 1.0000001), which would make `asinf` return `NaN`.

- **Negative pixel index (lines 66-67):** In C, the `%` operator can return negative values for negative operands (unlike JavaScript's `%`). The `if (px < 0) px += e->width` fixup handles this.

- **Byte-to-float conversion (lines 70-72):** Dividing by 255.0f maps [0, 255] to [0.0, 1.0], matching the float color space used throughout the raytracer.

### Integration in `cast_ray`

```c
if (depth > MAX_RECURSION_DEPTH ||
    !scene_intersect(ray, scene, &hit, &voxel_color, settings)) {
  /* L18 — Sample environment map (image or procedural sky). */
  if (settings && settings->show_envmap)
    return envmap_sample(&scene->envmap, ray.direction);
  /* Fallback: simple gradient when envmap is toggled off. */
  float t = 0.5f * (ray.direction.y + 1.0f);
  return vec3_lerp(vec3_make(0.2f, 0.7f, 0.8f),
                   vec3_make(1.0f, 1.0f, 1.0f), t * 0.3f);
}
```

This replaces the hardcoded background gradient from earlier lessons. When `show_envmap` is on (default), the environment map is sampled. When toggled off (E key), the original gradient is used.

### Integration in `scene_init`

```c
/* ── Environment map (L18) ───────────────────────────────────────────
 * Try loading an image file; fall back to procedural sky if not found. */
envmap_load(&s->envmap, "assets/envmap.jpg");
```

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Background is all black | `envmap_sample` returning (0,0,0) | Check that `procedural_sky` is called when `pixels == NULL`; verify `vec3_lerp` |
| Image loads but appears rotated 90 degrees | UV mapping `u` and `v` are swapped | `u` maps to width (horizontal), `v` maps to height (vertical) |
| Seam line visible at the back of the sphere | `atan2f` discontinuity at +/-PI | Ensure `u = 0.5 + atan2f(dz, dx) / (2*PI)` — the 0.5 offset centers the seam behind the viewer |
| NaN pixels (white or black spots) | `asinf` domain error from unclamped `d.y` | Clamp: `fmaxf(-1.0f, fminf(1.0f, d.y))` before `asinf` |
| Link error: `stbi_load undefined` | Missing stb_image implementation | X11: ensure `utils/stb_image_impl.c` is in SOURCES. Raylib: stb_image is built-in |
| Reflections show flat color instead of envmap | `cast_ray` recursion not using `envmap_sample` | The envmap sample is in the base case of `cast_ray` — reflected rays that miss geometry should hit it automatically |

## Exercise

> Download an equirectangular panorama image (search for "free HDRI" or visit [Poly Haven](https://polyhaven.com/hdris)) and save it as `assets/envmap.jpg`. Run the raytracer and verify the sky, reflections, and refractions all show the panorama. Then rotate the camera with arrow keys and watch the envmap rotate with the view.

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `new THREE.TextureLoader().load(url)` | `stbi_load(filename, &w, &h, &ch, 3)` | Synchronous; returns raw pixel bytes, not a GPU texture |
| `scene.background = cubeTexture` | `scene->envmap = loaded_envmap;` in scene_init | Not a cubemap; equirectangular 2D image sampled by direction |
| `Math.atan2(z, x)` | `atan2f(d.z, d.x)` | Identical math; `f` suffix for float variant |
| `Math.asin(y)` | `asinf(fmaxf(-1, fminf(1, d.y)))` | Must clamp to avoid NaN in C (JS returns NaN silently) |
| `image.onload = () => ...` | `if (!data) { /* fallback */ }` | No async; stbi_load blocks until file is read or fails |
