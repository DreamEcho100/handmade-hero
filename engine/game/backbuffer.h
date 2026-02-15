#ifndef DE100_GAME_BACKBUFFER_H
#define DE100_GAME_BACKBUFFER_H

#include "../_common/memory.h"

typedef struct {
  De100MemoryBlock memory; // Raw pixel memory (our canvas!)
  int width;               // Current backbuffer dimensions
  int height;
  int pitch;
  int bytes_per_pixel;
} GameBackBuffer;

#endif // DE100_GAME_BACKBUFFER_H
