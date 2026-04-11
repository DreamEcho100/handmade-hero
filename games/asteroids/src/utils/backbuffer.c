#include "./backbuffer.h"
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

int backbuffer_resize(Backbuffer *backbuffer, int new_width, int new_height) {
  if (new_width == backbuffer->width && new_height == backbuffer->height) {
    return 0;
  }

  size_t new_size = (size_t)new_width * (size_t)new_height * 4;
  uint32_t *new_pixels =
      (uint32_t *)mmap(NULL, new_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (!new_pixels) {
    return -1;
  }
  if (backbuffer->pixels) {
    munmap(backbuffer->pixels, backbuffer->pitch * backbuffer->height);
  }

  backbuffer->pixels = new_pixels;
  backbuffer->width = new_width;
  backbuffer->height = new_height;
  backbuffer->pitch = new_width * 4;

  return 0;
}