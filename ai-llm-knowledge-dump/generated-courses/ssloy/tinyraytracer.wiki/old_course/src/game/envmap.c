/* ═══════════════════════════════════════════════════════════════════════════
 * game/envmap.c — TinyRaytracer Course (L18 + L20)
 * ═══════════════════════════════════════════════════════════════════════════
 * Environment mapping — equirectangular, cube map, or procedural sky.
 * ═══════════════════════════════════════════════════════════════════════════
 */

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

/* ═══════════════════════════════════════════════════════════════════════════
 * Equirectangular loading (L18)
 * ═══════════════════════════════════════════════════════════════════════════ */

int envmap_load(EnvMap *e, const char *filename) {
  int w, h, ch;
  unsigned char *data = stbi_load(filename, &w, &h, &ch, 3);
  if (!data) {
    fprintf(stderr, "envmap_load: cannot open '%s' — using procedural sky\n",
            filename);
    envmap_init_procedural(e);
    return -1;
  }
  e->mode     = ENVMAP_EQUIRECT;
  e->pixels   = data;
  e->width    = w;
  e->height   = h;
  e->channels = 3;
  printf("Loaded envmap (equirectangular): %s (%dx%d)\n", filename, w, h);
  return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Cube map loading (L20)
 * ═══════════════════════════════════════════════════════════════════════════ */

int envmap_load_cubemap(EnvMap *e, const char *face_files[CUBEMAP_NUM_FACES]) {
  static const char *face_names[CUBEMAP_NUM_FACES] = {
    "+X (right)", "-X (left)", "+Y (up)", "-Y (down)", "+Z (front)", "-Z (back)"
  };

  for (int i = 0; i < CUBEMAP_NUM_FACES; i++) {
    int w, h, ch;
    unsigned char *data = stbi_load(face_files[i], &w, &h, &ch, 3);
    if (!data) {
      fprintf(stderr, "envmap_load_cubemap: cannot open '%s' [face %s] — "
              "falling back to procedural sky\n", face_files[i], face_names[i]);
      /* Free any already-loaded faces. */
      for (int j = 0; j < i; j++) {
        if (e->faces[j].pixels) {
          stbi_image_free(e->faces[j].pixels);
          e->faces[j].pixels = NULL;
        }
      }
      envmap_init_procedural(e);
      return -1;
    }
    e->faces[i].pixels   = data;
    e->faces[i].width    = w;
    e->faces[i].height   = h;
    e->faces[i].channels = 3;
    printf("Loaded cubemap face %s: %s (%dx%d)\n", face_names[i], face_files[i], w, h);
  }
  e->mode = ENVMAP_CUBEMAP;
  printf("Cube map loaded successfully (6 faces)\n");
  return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Free
 * ═══════════════════════════════════════════════════════════════════════════ */

void envmap_free(EnvMap *e) {
  if (e->pixels) {
    stbi_image_free(e->pixels);
    e->pixels = NULL;
  }
  for (int i = 0; i < CUBEMAP_NUM_FACES; i++) {
    if (e->faces[i].pixels) {
      stbi_image_free(e->faces[i].pixels);
      e->faces[i].pixels = NULL;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Procedural sky (no file needed)
 * ═══════════════════════════════════════════════════════════════════════════ */

static Vec3 procedural_sky(Vec3 dir) {
  float t = 0.5f * (dir.y + 1.0f);
  Vec3 ground  = vec3_make(0.35f, 0.25f, 0.15f);
  Vec3 horizon = vec3_make(0.85f, 0.85f, 0.80f);
  Vec3 zenith  = vec3_make(0.15f, 0.35f, 0.65f);

  if (t < 0.5f)
    return vec3_lerp(ground, horizon, t * 2.0f);
  return vec3_lerp(horizon, zenith, (t - 0.5f) * 2.0f);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Equirectangular sampling (L18)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void direction_to_uv(Vec3 d, float *u, float *v) {
  *u = 0.5f + atan2f(d.z, d.x) / (2.0f * (float)M_PI);
  *v = 0.5f - asinf(fmaxf(-1.0f, fminf(1.0f, d.y))) / (float)M_PI;
}

static Vec3 sample_equirect(const EnvMap *e, Vec3 direction) {
  float u, v;
  direction_to_uv(direction, &u, &v);

  int px = (int)(u * (float)e->width) % e->width;
  int py = (int)(v * (float)e->height) % e->height;
  if (px < 0)
    px += e->width;
  if (py < 0)
    py += e->height;

  int idx = (py * e->width + px) * e->channels;
  return vec3_make((float)e->pixels[idx + 0] / 255.0f,
                   (float)e->pixels[idx + 1] / 255.0f,
                   (float)e->pixels[idx + 2] / 255.0f);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Cube map sampling (L20)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Given a direction (dx, dy, dz), determine which face it hits:
 *   1. Find the axis with the largest absolute component
 *   2. Use the sign to pick +/- face
 *   3. Divide the other two components by the absolute major axis value
 *      to get UV in [-1, 1], then remap to [0, 1]
 *
 * Face layout (OpenGL convention):
 *   +X (right):  u = -dz/|dx|, v = -dy/|dx|
 *   -X (left):   u =  dz/|dx|, v = -dy/|dx|
 *   +Y (up):     u =  dx/|dy|, v =  dz/|dy|
 *   -Y (down):   u =  dx/|dy|, v = -dz/|dy|
 *   +Z (front):  u =  dx/|dz|, v = -dy/|dz|
 *   -Z (back):   u = -dx/|dz|, v = -dy/|dz|
 * ═══════════════════════════════════════════════════════════════════════════ */

static Vec3 sample_face(const CubeMapFace *face, float u, float v) {
  if (!face->pixels)
    return vec3_make(0.0f, 0.0f, 0.0f);

  /* Remap [-1,1] → [0,1]. */
  u = 0.5f * (u + 1.0f);
  v = 0.5f * (v + 1.0f);

  int px = (int)(u * (float)face->width) % face->width;
  int py = (int)(v * (float)face->height) % face->height;
  if (px < 0)
    px += face->width;
  if (py < 0)
    py += face->height;

  int idx = (py * face->width + px) * face->channels;
  return vec3_make((float)face->pixels[idx + 0] / 255.0f,
                   (float)face->pixels[idx + 1] / 255.0f,
                   (float)face->pixels[idx + 2] / 255.0f);
}

static Vec3 sample_cubemap(const EnvMap *e, Vec3 d) {
  float ax = fabsf(d.x), ay = fabsf(d.y), az = fabsf(d.z);
  int face_idx;
  float u, v, ma; /* ma = major axis absolute value */

  if (ax >= ay && ax >= az) {
    /* X is dominant. */
    ma = ax;
    if (d.x > 0.0f) {
      face_idx = CUBEMAP_FACE_POS_X;
      u = -d.z / ma;
      v = -d.y / ma;
    } else {
      face_idx = CUBEMAP_FACE_NEG_X;
      u =  d.z / ma;
      v = -d.y / ma;
    }
  } else if (ay >= ax && ay >= az) {
    /* Y is dominant. */
    ma = ay;
    if (d.y > 0.0f) {
      face_idx = CUBEMAP_FACE_POS_Y;
      u =  d.x / ma;
      v =  d.z / ma;
    } else {
      face_idx = CUBEMAP_FACE_NEG_Y;
      u =  d.x / ma;
      v = -d.z / ma;
    }
  } else {
    /* Z is dominant. */
    ma = az;
    if (d.z > 0.0f) {
      face_idx = CUBEMAP_FACE_POS_Z;
      u =  d.x / ma;
      v = -d.y / ma;
    } else {
      face_idx = CUBEMAP_FACE_NEG_Z;
      u = -d.x / ma;
      v = -d.y / ma;
    }
  }

  return sample_face(&e->faces[face_idx], u, v);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main dispatch
 * ═══════════════════════════════════════════════════════════════════════════ */

Vec3 envmap_sample(const EnvMap *e, Vec3 direction) {
  switch (e->mode) {
    case ENVMAP_EQUIRECT:
      return sample_equirect(e, direction);
    case ENVMAP_CUBEMAP:
      return sample_cubemap(e, direction);
    case ENVMAP_PROCEDURAL:
    default:
      return procedural_sky(direction);
  }
}
