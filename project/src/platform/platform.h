#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

typedef struct {
  int width;
  int height;
  uint32_t *pixels;
} PlatformFrame;

int platform_main();

#endif
