#include "draw-shapes.h"
#include "backbuffer.h"
#include "math.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Drawing Primitives
 * ═══════════════════════════════════════════════════════════════════════════
 */

void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
  /* Clip to backbuffer bounds */
  int x0 = MAX(x, 0);
  int y0 = MAX(y, 0);
  int x1 = MIN(x + w, bb->width);
  int y1 = MIN(y + h, bb->height);

  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * (bb->pitch / 4);
    for (int px = x0; px < x1; px++) {
      row[px] = color;
    }
  }
}

void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h,
                     uint32_t color) {
  // Extract alpha
  uint8_t alpha = (color >> 24) & 0xFF;

  // If fully opaque, just draw the rect, we don't need to blend
  if (alpha == 255) {
    draw_rect(bb, x, y, w, h, color);
    return;
  }

  // If fully transparent, skip drawing
  if (alpha == 0)
    return;

  // Extract source RGB components
  uint8_t src_r = color & 0xFF;
  uint8_t src_g = (color >> 8) & 0xFF;
  uint8_t src_b = (color >> 16) & 0xFF;

  // Clip to backbuffer bounds
  int x0 = MAX(x, 0);
  int y0 = MAX(y, 0);
  int x1 = MIN(x + w, bb->width);
  int y1 = MIN(y + h, bb->height);

  // Draw the rectangle
  for (int py = y0; py < y1; py++) {
    // Pointer to the start of the current row in the backbuffer
    uint32_t *row = bb->pixels + py * (bb->pitch / 4);
    // Iterate over the pixels in the current row
    for (int px = x0; px < x1; px++) {
      // Extract the destination RGB components
      uint32_t dst = row[px];
      uint8_t dst_r = (dst >> 16) & 0xFF;
      uint8_t dst_g = (dst >> 8) & 0xFF;
      uint8_t dst_b = dst & 0xFF;

      // Simple alpha blend: out = src * alpha + dst * (1 - alpha)
      // How it works?!!
      //   src * alpha
      //   + dst * (1 - alpha)
      //   -----------------------------
      //   out = (src * alpha + dst * (255 - alpha)) / 255
      // 255 is the max alpha value, so we divide by it to normalize back to
      // 0-255 range
      // Example: if alpha is 128 (50% transparent), then:
      //   out = (src * 128 + dst * 127) / 255
      // This means the output color will be a mix of 50% source and 50%
      // destination colors
      // If alpha is 64 (75% transparent), then:
      //   out = (src * 64 + dst * 191) / 255
      // This means the output color will be a mix of 25% source and 75%
      // destination colors
      // This is a simple linear interpolation between the source and
      // destination colors based on the alpha value
      uint8_t out_r = (src_r * alpha + dst_r * (255 - alpha)) / 255;
      uint8_t out_g = (src_g * alpha + dst_g * (255 - alpha)) / 255;
      uint8_t out_b = (src_b * alpha + dst_b * (255 - alpha)) / 255;

      row[px] = GAME_RGB(out_r, out_g, out_b);
    }
  }
}