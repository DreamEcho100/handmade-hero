/* ═══════════════════════════════════════════════════════════════════════════
 * game/stereogram.c — TinyRaytracer Course (L15)
 * ═══════════════════════════════════════════════════════════════════════════
 * Autostereogram / SIRDS — Part 2 of ssloy's wiki.
 *
 * Algorithm (from ssloy's "Displaying 3D Images" paper):
 * 1. Render a depth buffer: per-pixel normalized distance (0=far, 1=near).
 * 2. For each row, compute parallax constraints: two pixels at distance
 *    `parallax(depth)` apart must have the same color.
 * 3. Use a union-find to group constrained pixels into clusters.
 * 4. Assign each cluster the color of its root pixel from a periodic
 *    random source image.
 *
 * WALL-EYED vs CROSS-EYED:
 * The `left` and `right` pixel offsets determine which viewing method
 * produces correct depth.  Swapping left/right inverts the parallax,
 * making the stereogram correct for cross-eyed viewers instead of
 * wall-eyed viewers.  Using the wrong method inverts depth (ssloy
 * Part 2: "remember positive and negative parallax?").
 * ═══════════════════════════════════════════════════════════════════════════
 */

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
 * For depth z ∈ [0,1] (0=far, 1=near):
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
          /* Cross-eyed: swap constraint direction → inverts depth. */
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
