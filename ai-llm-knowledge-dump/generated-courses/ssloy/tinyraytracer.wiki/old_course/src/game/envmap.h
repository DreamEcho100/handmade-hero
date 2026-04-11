#ifndef GAME_ENVMAP_H
#define GAME_ENVMAP_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/envmap.h — TinyRaytracer Course (L18 + L20)
 * ═══════════════════════════════════════════════════════════════════════════
 * Environment mapping: replace the solid background with an image or
 * procedural sky sampled by ray direction.
 *
 * THREE MODES:
 *   1. Procedural sky (always available, no file needed)
 *   2. Spherical (equirectangular) image loaded via stb_image (L18)
 *   3. Cube map — 6 separate face images loaded via stb_image (L20)
 *
 * SPHERICAL MAPPING (equirectangular):
 *   u = 0.5 + atan2(dz, dx) / (2*PI)
 *   v = 0.5 - asin(dy) / PI
 *
 * CUBE MAP MAPPING:
 *   1. Find the axis with the largest absolute component
 *   2. Use the other two components as UV on that face
 *   6 faces: +X (right), -X (left), +Y (up), -Y (down), +Z (front), -Z (back)
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "vec3.h"
#include <stddef.h> /* NULL */

/* ── Cube map face indices ─────────────────────────────────────────────── */
#define CUBEMAP_FACE_POS_X 0 /* right  */
#define CUBEMAP_FACE_NEG_X 1 /* left   */
#define CUBEMAP_FACE_POS_Y 2 /* up     */
#define CUBEMAP_FACE_NEG_Y 3 /* down   */
#define CUBEMAP_FACE_POS_Z 4 /* front  */
#define CUBEMAP_FACE_NEG_Z 5 /* back   */
#define CUBEMAP_NUM_FACES 6

/* ── Environment map modes ─────────────────────────────────────────────── */
typedef enum {
  ENVMAP_PROCEDURAL = 0, /* no image, use procedural sky gradient      */
  ENVMAP_EQUIRECT = 1,   /* single equirectangular panorama (L18)      */
  ENVMAP_CUBEMAP = 2,    /* 6 face images (L20)                        */
} EnvMapMode;

/* ── Per-face image data ───────────────────────────────────────────────── */
typedef struct {
  unsigned char *pixels; /* NULL if face not loaded                     */
  int width, height;
  int channels; /* 3 (RGB) or 4 (RGBA)                        */
} CubeMapFace;

/* ── Environment map ───────────────────────────────────────────────────── */
typedef struct {
  EnvMapMode mode;

  /* Equirectangular data (L18). */
  unsigned char *pixels; /* NULL = not loaded                           */
  int width, height;
  int channels; /* 3 (RGB) or 4 (RGBA)                        */

  /* Cube map data (L20). */
  CubeMapFace faces[CUBEMAP_NUM_FACES];
} EnvMap;

/* Initialize to procedural sky (no file needed). */
static inline void envmap_init_procedural(EnvMap *e) {
  e->mode = ENVMAP_PROCEDURAL;
  e->pixels = NULL;
  e->width = 0;
  e->height = 0;
  e->channels = 0;
  for (int i = 0; i < CUBEMAP_NUM_FACES; i++) {
    e->faces[i].pixels = NULL;
    e->faces[i].width = 0;
    e->faces[i].height = 0;
    e->faces[i].channels = 0;
  }
}

/* ── Equirectangular (L18) ─────────────────────────────────────────────── */

/* Load a spherical environment map from an image file (JPG/PNG/HDR).
 * Returns 0 on success, -1 on failure (falls back to procedural sky).
 * Uses stb_image internally.                                              */
int envmap_load(EnvMap *e, const char *filename);

/* ── Cube map (L20) ────────────────────────────────────────────────────── */

/* Load 6 cube map face images.  `face_files` is an array of 6 filenames
 * in order: +X, -X, +Y, -Y, +Z, -Z.
 * Returns 0 on success, -1 if any face fails (falls back to procedural).  */
int envmap_load_cubemap(EnvMap *e, const char *face_files[CUBEMAP_NUM_FACES]);

/* Free all loaded image data (equirectangular + cube map faces). */
void envmap_free(EnvMap *e);

/* Sample the environment map by ray direction.
 * Dispatches to equirectangular, cube map, or procedural sky based on mode. */
Vec3 envmap_sample(const EnvMap *e, Vec3 direction);

#endif /* GAME_ENVMAP_H */
