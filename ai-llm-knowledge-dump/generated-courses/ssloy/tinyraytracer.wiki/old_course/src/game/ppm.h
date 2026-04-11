#ifndef GAME_PPM_H
#define GAME_PPM_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/ppm.h — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 * LESSON 02 — PPM P6 binary writer.
 *
 * PPM P6 format:
 *   "P6\n<width> <height>\n255\n" followed by width*height RGB triplets.
 *   Each component is one byte (0-255).
 *
 * JS analogy: fs.writeFileSync('out.ppm', buffer) — but in C we use
 * fopen/fwrite/fclose (three steps, must close the file ourselves).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "../utils/backbuffer.h"

/* Clamp a float [0,1] to a byte [0,255]. */
static inline uint8_t float_to_byte(float v) {
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  return (uint8_t)(v * 255.0f + 0.5f);
}

/* Export the backbuffer as a PPM P6 file.
 * The backbuffer stores pixels as 0xAARRGGBB (GAME_RGB format).           */
static inline void ppm_export(const Backbuffer *bb, const char *filename) {
  FILE *f = fopen(filename, "wb");
  if (!f) {
    fprintf(stderr, "ppm_export: cannot open '%s'\n", filename);
    return;
  }

  fprintf(f, "P6\n%d %d\n255\n", bb->width, bb->height);

  int stride = bb->pitch / 4;
  for (int y = 0; y < bb->height; y++) {
    for (int x = 0; x < bb->width; x++) {
      uint32_t pixel = bb->pixels[y * stride + x];
      /* GAME_RGB stores as 0x00RRGGBB on little-endian:
       * byte[0]=R, byte[1]=G, byte[2]=B, byte[3]=A.
       * Extract R, G, B from the packed uint32_t.                         */
      uint8_t r = (uint8_t)((pixel >>  0) & 0xFF);
      uint8_t g = (uint8_t)((pixel >>  8) & 0xFF);
      uint8_t b = (uint8_t)((pixel >> 16) & 0xFF);
      uint8_t rgb[3] = {r, g, b};
      fwrite(rgb, 1, 3, f);
    }
  }

  fclose(f);
  printf("Exported %s (%dx%d)\n", filename, bb->width, bb->height);
}

/* Export a raw RGB byte buffer (not a backbuffer) as PPM P6.
 * Used by stereo and stereogram exporters that produce their own pixels.  */
static inline void ppm_export_rgb(const uint8_t *pixels, int width, int height,
                                  const char *filename) {
  FILE *f = fopen(filename, "wb");
  if (!f) {
    fprintf(stderr, "ppm_export_rgb: cannot open '%s'\n", filename);
    return;
  }
  fprintf(f, "P6\n%d %d\n255\n", width, height);
  fwrite(pixels, 1, (size_t)(width * height * 3), f);
  fclose(f);
  printf("Exported %s (%dx%d)\n", filename, width, height);
}

#endif /* GAME_PPM_H */
