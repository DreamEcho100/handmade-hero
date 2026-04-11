#ifndef UTILS_BACKBUFFER_H
#define UTILS_BACKBUFFER_H

#include <stdint.h>

typedef struct {
  uint32_t *pixels; // Flat pixel array
  int width;        // Canvas width (GAME_W)
  int height;       // Canvas height (GAME_H)
  int pitch;        // Bytes per row = width * bytes_per_pixel
  int bytes_per_pixel;
} Backbuffer;

#define GAME_RGBA(r, g, b, a)                                                  \
  (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) |      \
   (uint32_t)(r))
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 255)

#define COLOR_BLACK 0.0f, 0.0f, 0.0f, 1.0f
#define COLOR_WHITE 1.0f, 1.0f, 1.0f, 1.0f
#define COLOR_RED 0.863f, 0.196f, 0.196f, 1.0f
#define COLOR_GREEN 0.196f, 0.804f, 0.196f, 1.0f
#define COLOR_BLUE 0.196f, 0.392f, 0.863f, 1.0f
#define COLOR_YELLOW 1.0f, 0.843f, 0.0f, 1.0f
#define COLOR_CYAN 0.0f, 0.843f, 0.843f, 1.0f
#define COLOR_MAGENTA 0.863f, 0.196f, 0.863f, 1.0f
#define COLOR_ORANGE 1.0f, 0.549f, 0.0f, 1.0f
#define COLOR_BG 0.078f, 0.078f, 0.118f, 1.0f
#define COLOR_GRAY 0.5f, 0.5f, 0.5f, 1.0f

int backbuffer_resize(Backbuffer *backbuffer, int new_width, int new_height);

#endif // UTILS_BACKBUFFER_H