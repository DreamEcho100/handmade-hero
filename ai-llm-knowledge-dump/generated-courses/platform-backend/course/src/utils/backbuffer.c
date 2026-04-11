#include "./backbuffer.h"
#include <stddef.h>
#include <stdlib.h>

int backbuffer_resize(Backbuffer *backbuffer, int new_width, int new_height) {
  if (new_width == backbuffer->width && new_height == backbuffer->height) {
    return 0;
  }

  size_t new_size = (size_t)new_width * (size_t)new_height * 4;
  uint32_t *new_pixels = (uint32_t *)realloc(backbuffer->pixels, new_size);
  if (!new_pixels) {
    return -1;
  }

  backbuffer->pixels = new_pixels;
  backbuffer->width = new_width;
  backbuffer->height = new_height;
  backbuffer->bytes_per_pixel = 4;
  backbuffer->pitch = new_width * 4;

  return 0;
}
