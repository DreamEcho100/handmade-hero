#ifndef GAME_H
#define GAME_H

#include "platform/platform.h"
typedef struct {
  int offset_x;
  int offset_y;
} GradientState;

typedef struct {
  int offset_x;
  int offset_y;
} PixelState;

typedef struct {
  void *memory; // Raw pixel memory (our canvas!)
  int width;    // Current buffer dimensions
  int height;
  int pitch;
  int bytes_per_pixel;
} OffscreenBuffer;

void render_weird_gradient(OffscreenBuffer *, GradientState *,
                           PlatformPixelFormatShift *);
void testPixelAnimation(
    OffscreenBuffer *buffer, PixelState *pixel_state,
    int pixel_color);

#endif // GAME_H
