#ifndef DE100_GAME_BACKBUFFER_H
#define DE100_GAME_BACKBUFFER_H

#include "../_common/base.h"
#include "../_common/memory.h"

typedef struct {
  PlatformMemoryBlock memory; // Raw pixel memory (our canvas!)
  int width;                  // Current backbuffer dimensions
  int height;
  int pitch;
  int bytes_per_pixel;
} GameBackBuffer;

typedef enum {
  INIT_BACKBUFFER_STATUS_SUCCESS = 0,
  INIT_BACKBUFFER_STATUS_MMAP_FAILED = 1,
} INIT_BACKBUFFER_STATUS;
INIT_BACKBUFFER_STATUS init_backbuffer(GameBackBuffer *buffer, int width, int height, int bytes_per_pixel);

#endif // DE100_GAME_BACKBUFFER_H