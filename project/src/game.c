#include "game.h"
#include "base.h"

void render_weird_gradient(
    OffscreenBuffer *buffer, GradientState *gradient_state,
    PlatformPixelFormatShift *platform_pixel_format_shift) {
  uint8_t *row = (uint8_t *)buffer->memory;

  // The following is correct for X11
  for (int y = 0; y < buffer->height; ++y) {
    uint32_t *pixels = (uint32_t *)row;
    for (int x = 0; x < buffer->width; ++x) {
      uint8_t red = 0; // Default red value (for both backends)
      uint8_t green = (y + gradient_state->offset_y);
      uint8_t blue = (x + gradient_state->offset_x);
      uint8_t alpha = 255; // Full opacity for Raylib

      *pixels++ = ((alpha << platform_pixel_format_shift->ALPHA_SHIFT) |
                   (red << platform_pixel_format_shift->RED_SHIFT) |
                   (green << platform_pixel_format_shift->GREEN_SHIFT) |
                   (blue << platform_pixel_format_shift->BLUE_SHIFT));
    }
    row += buffer->pitch;
  }
}

void testPixelAnimation(OffscreenBuffer *buffer, PixelState *pixel_state,
                        int pixelColor) {
  // Test pixel animation
  uint32_t *pixels = (uint32_t *)buffer->memory;
  int total_pixels = buffer->width * buffer->height;

  int test_offset =
      pixel_state->offset_y * buffer->width + pixel_state->offset_x;

  if (test_offset < total_pixels) {
    pixels[test_offset] = pixelColor;
  }

  if (pixel_state->offset_x + 1 < buffer->width - 1) {
    pixel_state->offset_x += 1;
  } else {
    pixel_state->offset_x = 0;
    if (pixel_state->offset_y + 75 < buffer->height - 1) {
      pixel_state->offset_y += 75;
    } else {
      pixel_state->offset_y = 0;
    }
  }
}