# Lesson 15 — Autostereogram (SIRDS)

> **What you'll build:** By the end of this lesson, pressing **G** exports a random-dot autostereogram — a single image that reveals a hidden 3D scene when viewed with relaxed (wall-eyed) or crossed eyes.

## Observable outcome

A file `stereogram.ppm` is generated containing a pattern of colored noise. When you relax your focus (wall-eyed viewing) or cross your eyes (Shift+G for cross-eyed mode), the spheres, box, and floor emerge as a 3D depth image — no glasses needed. The depth is encoded entirely in horizontal pixel repetitions.

## New concepts

- Depth buffer rendering — converting the scene to per-pixel normalized distance values
- Parallax formula — computing the pixel separation from depth using `eye_separation * (1 - mu*z) / (2 - mu*z)`
- Union-find (disjoint set) — grouping constrained pixels so same-depth pixels share colors
- Per-row SIRDS generation — ssloy's constraint propagation algorithm for single-image stereograms
- Wall-eyed vs cross-eyed viewing — swapping constraint direction inverts perceived depth

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `game/stereogram.h` | Created | Constants (`STEREO_MU`, `STEREOGRAM_EYE_PX`); wall-eyed and cross-eyed function declarations |
| `game/stereogram.c` | Created | Union-find; parallax function; depth buffer; SIRDS core; both viewing modes |
| `game/main.c` | Modified | G key calls `render_autostereogram`; Shift+G calls cross-eyed variant |
| `game/base.h` | Modified | `export_stereogram` button added to `GameInput` |

## Background — why this works

### JS analogy

In JavaScript you might generate a stereogram with a canvas 2D context, iterating over rows and assigning pixel colors based on a depth map. The algorithm is identical — stereograms are a pure 2D pixel manipulation technique, not a 3D rendering concept. The only 3D part is generating the depth buffer, which our existing raytracer already knows how to do.

### How autostereograms encode depth (ssloy Part 2)

An autostereogram exploits the fact that your left and right eyes see different horizontal positions. If two pixels are separated by exactly the distance your eyes would see "through" a surface at depth `z`, and those two pixels have the same color, your brain interprets them as a single point at depth `z`.

The key insight from ssloy: "If two pixels at distance `parallax(z)` apart have the same color, the brain perceives them as a single point at depth z."

The algorithm works per-row:
1. For each pixel, compute the parallax (pixel separation) from the depth buffer.
2. The pixel at position `i - parallax/2` and the pixel at `i + parallax/2` must have the same color.
3. Use union-find to group all pixels that are constrained to match.
4. Assign each group the color of its root pixel from a random source pattern.

### The parallax formula

From ssloy Part 2, the parallax (pixel separation) for depth `z` (0=far, 1=near) is:

```
parallax = eye_separation_px * (1 - mu * z) / (2 - mu * z)
```

Where `mu` (0.33) is the near plane as a fraction of the far plane distance. The formula gives:
- At `z=0` (far): maximum parallax (widest repetition)
- At `z=1` (near): minimum parallax (tightest repetition)

The screen distance `d` cancels out of the formula — parallax depends only on depth and `mu`, not on how far you sit from the screen. This is why stereograms work at any viewing distance.

### Union-find: why we need it

Multiple depth values in a row create overlapping constraints. Pixel A must match pixel B (from one surface), and pixel B must match pixel C (from another surface). Transitively, A must match C. Union-find efficiently computes these transitive groups:

```c
static int uf_find(int *same, int x) {
  if (same[x] == x) return x;
  same[x] = uf_find(same, same[x]); /* path compression */
  return same[x];
}
```

Path compression makes repeated lookups nearly O(1) amortized. Without it, chains of constraints could degrade to O(n) per lookup.

### Wall-eyed vs cross-eyed (ssloy Part 2)

The same stereogram can be viewed two ways:
- **Wall-eyed:** eyes diverge (focus behind the screen) — near objects pop OUT toward you
- **Cross-eyed:** eyes converge (focus in front of the screen) — near objects push IN away from you

Using the wrong method inverts the depth — hills become valleys. The `cross_eyed` flag swaps the union direction: `uf_union(right, left)` instead of `uf_union(left, right)`, which reverses which pixel inherits which color and thus which eye sees which offset.

## Code walkthrough

### `game/stereogram.h` — complete file

```c
#ifndef GAME_STEREOGRAM_H
#define GAME_STEREOGRAM_H

#include "scene.h"
#include "render.h"

/* Near plane as fraction of far plane distance (ssloy uses 1/3). */
#define STEREO_MU         0.33f

/* Interpupillary distance in pixels (ssloy uses 400). */
#define STEREOGRAM_EYE_PX 400

/* Render a wall-eyed autostereogram (default: G key). */
void render_autostereogram(int width, int height,
                           const Scene *scene, const RtCamera *cam,
                           const char *filename, const RenderSettings *settings);

/* Render a cross-eyed autostereogram (Shift+G key).
 * Same algorithm but parallax constraint is reversed:
 * `uf_union(right, left)` instead of `uf_union(left, right)`.            */
void render_autostereogram_crosseyed(int width, int height,
                                     const Scene *scene, const RtCamera *cam,
                                     const char *filename,
                                     const RenderSettings *settings);

#endif /* GAME_STEREOGRAM_H */
```

**Key lines:**
- `STEREO_MU 0.33f` — the near plane is 1/3 of the far plane distance. This controls how much depth range the stereogram can encode.
- `STEREOGRAM_EYE_PX 400` — eye separation in pixels (not meters). This determines the maximum repetition width in the pattern.

### `game/stereogram.c` — complete file

```c
#include "stereogram.h"
#include "intersect.h"
#include "ray.h"
#include "ppm.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ── Union-Find (from ssloy Part 2) ─────────────────────────────────────
 * Two functions, ~3 lines each.  `same[x] = x` means x is its own root.
 * Path compression in uf_find makes repeated lookups nearly O(1).         */

static int uf_find(int *same, int x) {
  if (same[x] == x) return x;
  same[x] = uf_find(same, same[x]); /* path compression */
  return same[x];
}

static void uf_union(int *same, int x, int y) {
  x = uf_find(same, x);
  y = uf_find(same, y);
  if (x != y) same[x] = y;
}

/* ── Parallax function (from ssloy Part 2) ───────────────────────────────
 * For depth z in [0,1] (0=far, 1=near):
 *   parallax = eye_separation * (1 - mu*z) / (2 - mu*z)
 *
 * Derivation (ssloy): "p/e = (d - d*mu*z) / (2d - d*mu*z)"
 * The d (screen distance) cancels out: "p/e = (1 - mu*z) / (2 - mu*z)"
 * So parallax depends only on depth and mu, not viewing distance.         */
static int parallax(float z) {
  float p = (float)STEREOGRAM_EYE_PX
            * (1.0f - z * STEREO_MU) / (2.0f - z * STEREO_MU);
  return (int)(p + 0.5f);
}

/* ── Depth buffer rendering ──────────────────────────────────────────── */
static void render_depth_buffer(float *zbuffer, int width, int height,
                                const Scene *scene, const RtCamera *cam,
                                const RenderSettings *settings) {
  float aspect   = (float)width / (float)height;
  float half_fov = tanf(cam->fov / 2.0f);

  Vec3 forward  = vec3_normalize(vec3_sub(cam->target, cam->position));
  Vec3 world_up = vec3_make(0.0f, 1.0f, 0.0f);
  Vec3 right    = vec3_normalize(vec3_cross(forward, world_up));
  Vec3 up       = vec3_cross(right, forward);

  float max_dist = 0.0f;

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      float x =  (2.0f * ((float)i + 0.5f) / (float)width  - 1.0f) * half_fov * aspect;
      float y = -(2.0f * ((float)j + 0.5f) / (float)height - 1.0f) * half_fov;

      Vec3 dir = vec3_normalize(vec3_add(
        vec3_add(vec3_scale(right, x), vec3_scale(up, y)), forward));

      RtRay ray = { .origin = cam->position, .direction = dir };
      HitRecord hit;

      if (scene_intersect(ray, scene, &hit, NULL, settings)) {
        zbuffer[j * width + i] = hit.t;
        if (hit.t > max_dist) max_dist = hit.t;
      } else {
        zbuffer[j * width + i] = FLT_MAX;
      }
    }
  }

  /* Normalize: 0 = far (background), 1 = near (closest object). */
  if (max_dist < 0.001f) max_dist = 1.0f;
  for (int i = 0; i < width * height; i++) {
    if (zbuffer[i] >= FLT_MAX * 0.5f) {
      zbuffer[i] = 0.0f;
    } else {
      zbuffer[i] = 1.0f - (zbuffer[i] / max_dist);
      if (zbuffer[i] < 0.0f) zbuffer[i] = 0.0f;
      if (zbuffer[i] > 1.0f) zbuffer[i] = 1.0f;
    }
  }
}

/* ── Core SIRDS rendering (shared by wall-eyed and cross-eyed) ───────── */
static void render_sirds_internal(int width, int height,
                                  const Scene *scene, const RtCamera *cam,
                                  const char *filename,
                                  const RenderSettings *settings,
                                  int cross_eyed) {
  int pixel_count = width * height;

  float   *zbuffer = (float *)  malloc((size_t)pixel_count * sizeof(float));
  uint8_t *source  = (uint8_t *)malloc((size_t)pixel_count * 3);
  uint8_t *output  = (uint8_t *)malloc((size_t)pixel_count * 3);
  int     *same    = (int *)    malloc((size_t)width * sizeof(int));

  if (!zbuffer || !source || !output || !same) {
    free(zbuffer); free(source); free(output); free(same);
    return;
  }

  render_depth_buffer(zbuffer, width, height, scene, cam, settings);

  /* Prepare source image: periodic random pattern with sinf stripe hints.
   * Stripes spaced at max parallax width to help the viewer converge.     */
  srand(42);
  int max_par = parallax(0.0f);
  if (max_par < 1) max_par = 1;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int idx = (j * width + i) * 3;
      float stripe = sinf((float)i / (float)max_par * (float)M_PI * 2.0f);
      source[idx + 0] = (uint8_t)((int)(128.0f + 80.0f * stripe) & 0xFF);
      source[idx + 1] = (uint8_t)(rand() & 0xFF);
      source[idx + 2] = (uint8_t)(rand() & 0xFF);
    }
  }

  memcpy(output, source, (size_t)pixel_count * 3);

  /* ── Per-row constraint propagation (ssloy's algorithm) ────────────── */
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) same[i] = i;

    for (int i = 0; i < width; i++) {
      int par      = parallax(zbuffer[j * width + i]);
      int left_px  = i - par / 2;
      int right_px = left_px + par;
      if (left_px >= 0 && right_px < width) {
        if (cross_eyed) {
          /* Cross-eyed: swap constraint direction -> inverts depth. */
          uf_union(same, right_px, left_px);
        } else {
          /* Wall-eyed (default): normal constraint direction. */
          uf_union(same, left_px, right_px);
        }
      }
    }

    for (int i = 0; i < width; i++) {
      int root    = uf_find(same, i);
      int src_idx = (j * width + root) * 3;
      int dst_idx = (j * width + i)    * 3;
      output[dst_idx + 0] = source[src_idx + 0];
      output[dst_idx + 1] = source[src_idx + 1];
      output[dst_idx + 2] = source[src_idx + 2];
    }
  }

  ppm_export_rgb(output, width, height, filename);

  free(zbuffer);
  free(source);
  free(output);
  free(same);
}

void render_autostereogram(int width, int height,
                           const Scene *scene, const RtCamera *cam,
                           const char *filename,
                           const RenderSettings *settings) {
  render_sirds_internal(width, height, scene, cam, filename, settings, 0);
}

void render_autostereogram_crosseyed(int width, int height,
                                     const Scene *scene, const RtCamera *cam,
                                     const char *filename,
                                     const RenderSettings *settings) {
  render_sirds_internal(width, height, scene, cam, filename, settings, 1);
}
```

**Key sections explained:**

**Depth buffer (lines 41-82):** Re-uses the same ray generation as `render_scene`, but instead of computing color, stores `hit.t` (distance along ray). After all pixels are traced, the buffer is normalized: the farthest hit becomes 0.0, the nearest becomes 1.0. Background pixels (no hit) are set to 0.0 (maximum depth).

**Source pattern (lines 98-107):** The random pattern uses `srand(42)` for reproducibility. The red channel has sinusoidal stripes at the maximum parallax width — these act as convergence aids, giving the viewer's eyes a periodic pattern to lock onto. Green and blue channels are pure random noise.

**Constraint propagation (lines 112-131):** For each pixel `i` in a row:
1. Compute the parallax `par` from the depth at that pixel.
2. The pixel at `i - par/2` and the pixel at `i + par/2` must share a color.
3. `uf_union` merges these two pixels into the same group.
4. After processing all pixels in the row, each pixel inherits the color of its union-find root.

**Cross-eyed swap (lines 119-122):** The only difference between wall-eyed and cross-eyed is which pixel becomes the root in `uf_union`. Swapping the arguments reverses the parallax direction, making the stereogram correct for cross-eyed viewing.

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Stereogram looks like random noise (no depth) | Depth buffer is all zeros (no objects hit) | Check that `scene_intersect` returns hits; verify camera is pointed at the scene |
| Depth is inverted (near objects are concave) | Using wall-eyed viewing on a cross-eyed stereogram (or vice versa) | Try both G and Shift+G; or try relaxing vs crossing your eyes |
| Vertical seams / discontinuities | `same[]` array not reset between rows | Ensure `for (int i = 0; i < width; i++) same[i] = i;` runs at the start of each row |
| Stack overflow in `uf_find` | Very long union chains without path compression | Ensure `same[x] = uf_find(same, same[x])` is present (path compression) |
| Stereogram is too small to view | Window resolution too low for `STEREOGRAM_EYE_PX = 400` | Increase resolution to at least 800 wide, or reduce `STEREOGRAM_EYE_PX` |
| Repeating pattern is too wide (can't converge) | `STEREOGRAM_EYE_PX` too large for screen/viewing distance | Try 300 instead of 400 for smaller screens |

## Exercise

> Modify the source pattern to use a horizontal color gradient instead of random noise (keep the sinusoidal red stripe). Compare the visual quality — is it easier or harder to perceive depth with a structured pattern vs random dots?

## JS ↔ C concept map

| JS / Web concept | C equivalent in this lesson | Key difference |
|---|---|---|
| `new Float32Array(w * h)` | `float *zbuffer = malloc(pixel_count * sizeof(float))` | Manual allocation + free; no GC |
| `class UnionFind { find(x) {...} }` | `static int uf_find(int *same, int x)` | Array-based; no class wrapper |
| `Math.atan2(dz, dx)` | Not used here (used in L18 envmap) | Stereograms are purely pixel-based, no angle math |
| `ctx.putImageData(imageData, 0, 0)` | `ppm_export_rgb(output, width, height, filename)` | File export, not canvas display |
| `Array(width).fill(0).map((_, i) => i)` | `for (int i = 0; i < width; i++) same[i] = i;` | Manual identity initialization |
