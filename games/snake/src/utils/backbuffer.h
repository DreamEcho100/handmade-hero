#ifndef UTILS_BACKBUFFER_H
#define UTILS_BACKBUFFER_H
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Backbuffer - Platform Independent Rendering Target
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  uint32_t *pixels; /* RGBA pixel data (0xAARRGGBB format) */
  int width;
  int height;
  int pitch;           /* Bytes per row (usually width * 4) */
  int bytes_per_pixel; /* Should be 4 for RGBA8888 */
} Backbuffer;

/* Color helper - pack RGBA into uint32 */
#define GAME_RGBA(r, g, b, a)                                                  \
  (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) |      \
   (uint32_t)(r))

#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 255)

#endif /* UTILS_BACKBUFFER_H */
