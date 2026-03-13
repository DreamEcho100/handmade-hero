/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-shapes.c — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 04 — draw_rect, draw_rect_blend implementation.
 *             Pitch-based row pointer, clipping, alpha composite formula.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./draw-shapes.h"
#include "./math.h"

/* ─────────────────────────────────────────────────────────────────────────
 * draw_rect
 * ─────────────────────────────────────────────────────────────────────────
 * Clip incoming rect to buffer bounds, then fill each row.
 *
 * LESSON 04 — Two-step clip:
 *   1. Compute {x1,y1} end points (exclusive).
 *   2. Clamp both origin and end to [0, width/height].
 *   3. Re-derive w and h; skip if either is zero.
 */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
  if (!bb || !bb->pixels) return;

  int x0 = MAX(x, 0);
  int y0 = MAX(y, 0);
  int x1 = MIN(x + w, bb->width);
  int y1 = MIN(y + h, bb->height);

  if (x0 >= x1 || y0 >= y1) return;

  /* LESSON 04 — pitch / bytes_per_pixel gives the stride in uint32_t units.
   * We store this once, then do one addition per row instead of one
   * multiplication. */
  int stride = bb->pitch / bb->bytes_per_pixel;

  for (int row = y0; row < y1; row++) {
    uint32_t *dst = bb->pixels + row * stride + x0;
    for (int col = x0; col < x1; col++) {
      *dst++ = color;
    }
  }
}

/* ─────────────────────────────────────────────────────────────────────────
 * draw_rect_blend
 * ─────────────────────────────────────────────────────────────────────────
 * Alpha-composite `color` over the existing pixels using the alpha byte of
 * `color` (the top 8 bits of the 0xAARRGGBB value).
 *
 * LESSON 04 — Blend formula (per channel):
 *   out = src * a/255 + dst * (1 - a/255)
 *       = (src * a + dst * inv_a) / 255
 *
 * Integer approximation: divide by 255 using >> 8 (fast but slightly off).
 * For a course we accept the ≈0.4% error; production code uses /255 or a
 * lookup table.
 */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
  if (!bb || !bb->pixels) return;

  int src_a = (color >> 24) & 0xFF;
  if (src_a == 0)   return;          /* Fully transparent — nothing to do.  */
  if (src_a == 255) {                /* Fully opaque — fast path.           */
    draw_rect(bb, x, y, w, h, color);
    return;
  }

  int x0 = MAX(x, 0);
  int y0 = MAX(y, 0);
  int x1 = MIN(x + w, bb->width);
  int y1 = MIN(y + h, bb->height);

  if (x0 >= x1 || y0 >= y1) return;

  int inv_a  = 255 - src_a;
  int src_r  = (color       ) & 0xFF;
  int src_g  = (color >>  8 ) & 0xFF;
  int src_b  = (color >> 16 ) & 0xFF;

  int stride = bb->pitch / bb->bytes_per_pixel;

  for (int row = y0; row < y1; row++) {
    uint32_t *dst = bb->pixels + row * stride + x0;
    for (int col = x0; col < x1; col++) {
      uint32_t d = *dst;
      int dr = (d       ) & 0xFF;
      int dg = (d >>  8 ) & 0xFF;
      int db = (d >> 16 ) & 0xFF;

      int out_r = (src_r * src_a + dr * inv_a) >> 8;
      int out_g = (src_g * src_a + dg * inv_a) >> 8;
      int out_b = (src_b * src_a + db * inv_a) >> 8;

      *dst++ = (uint32_t)out_r | ((uint32_t)out_g << 8) | ((uint32_t)out_b << 16) | 0xFF000000u;
    }
  }
}
