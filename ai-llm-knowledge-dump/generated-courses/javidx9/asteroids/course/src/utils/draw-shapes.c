/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-shapes.c — Primitive Drawing (Rects + Lines)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 03 — draw_rect: pure rasterizer implementation.
 *             Float positions/colors, unified alpha blending, pitch-based
 *             row pointer, clipping.
 *
 * LESSON 05 — draw_line: Bresenham integer line algorithm.
 *             Used by draw_wireframe to render ship and asteroid outlines.
 *
 * Both functions are coordinate-system-agnostic: they take pixel values and
 * draw.  Any game-unit → pixel conversion happens at the call site.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./draw-shapes.h"
#include "./math.h"

/* ─────────────────────────────────────────────────────────────────────────
 * draw_rect
 * ─────────────────────────────────────────────────────────────────────────
 * Fill a rectangle at pixel coordinates with float colors.
 *
 * LESSON 03 — Three-tier alpha handling:
 *   1. a <= 0  → skip entirely (fully transparent)
 *   2. a >= 1  → direct pixel write (opaque fast path, no read-back)
 *   3. else    → per-pixel alpha composite: out = src*a + dst*(1-a)
 *
 * Float colors [0.0–1.0] are converted to 8-bit at rasterization time.
 */
void draw_rect(Backbuffer *backbuffer,
               float x, float y, float w, float h,
               float r, float g, float b, float a) {
  if (!backbuffer || !backbuffer->pixels) return;
  if (a <= 0.0f) return;

  float clamped_r = CLAMP(r, 0.0f, 1.0f);
  float clamped_g = CLAMP(g, 0.0f, 1.0f);
  float clamped_b = CLAMP(b, 0.0f, 1.0f);
  float clamped_a = CLAMP(a, 0.0f, 1.0f);

  int x0 = MAX((int)x, 0);
  int y0 = MAX((int)y, 0);
  int x1 = MIN((int)(x + w), backbuffer->width);
  int y1 = MIN((int)(y + h), backbuffer->height);

  if (x0 >= x1 || y0 >= y1) return;

  /* LESSON 03 — pitch / bytes_per_pixel gives the stride in uint32_t units.
   * Store once, then do one addition per row instead of one multiplication. */
  int stride = backbuffer->pitch / backbuffer->bytes_per_pixel;

  if (clamped_a >= 1.0f) {
    /* ── Opaque fast path ──────────────────────────────────────────────
     * No need to read the destination pixel. Convert float color to the
     * packed 0xAARRGGBB format and write directly. */
    uint32_t color = (uint32_t)(clamped_r * 255.0f)
                   | ((uint32_t)(clamped_g * 255.0f) << 8)
                   | ((uint32_t)(clamped_b * 255.0f) << 16)
                   | 0xFF000000u;

    for (int row = y0; row < y1; row++) {
      uint32_t *dst = backbuffer->pixels + row * stride + x0;
      for (int col = x0; col < x1; col++) {
        *dst++ = color;
      }
    }
  } else {
    /* ── Alpha blend path ──────────────────────────────────────────────
     * Blend formula (per channel):
     *   out = src * a/255 + dst * (1 - a/255)
     *       = (src * src_a + dst * inv_alpha) >> 8
     *
     * Integer approximation: >> 8 instead of / 255 (fast, ≈0.4% error).
     * Forces output alpha to 0xFF (fully opaque result). */
    int src_a = (int)(clamped_a * 255.0f);
    int inv_alpha = 255 - src_a;
    int src_r = (int)(clamped_r * 255.0f);
    int src_g = (int)(clamped_g * 255.0f);
    int src_b = (int)(clamped_b * 255.0f);

    for (int row = y0; row < y1; row++) {
      uint32_t *dst = backbuffer->pixels + row * stride + x0;
      for (int col = x0; col < x1; col++) {
        uint32_t dest_pixel = *dst;
        int dest_r = (dest_pixel      ) & 0xFF;
        int dest_g = (dest_pixel >>  8) & 0xFF;
        int dest_b = (dest_pixel >> 16) & 0xFF;

        int out_r = (src_r * src_a + dest_r * inv_alpha) >> 8;
        int out_g = (src_g * src_a + dest_g * inv_alpha) >> 8;
        int out_b = (src_b * src_a + dest_b * inv_alpha) >> 8;

        *dst++ = (uint32_t)out_r
               | ((uint32_t)out_g << 8)
               | ((uint32_t)out_b << 16)
               | 0xFF000000u;
      }
    }
  }
}

/* ─────────────────────────────────────────────────────────────────────────
 * draw_line
 * ─────────────────────────────────────────────────────────────────────────
 * Draw a straight line using Bresenham's integer algorithm.
 *
 * LESSON 05 — Bresenham's algorithm:
 *   • No floating-point arithmetic in the inner loop — only integer adds.
 *   • The error accumulator decides when to step on the minor axis.
 *   • Handles all octants by swapping roles of x and y (the `steep` flag).
 *
 * Pixels are CLIPPED (not toroidal): a line that goes off-screen stops at
 * the boundary.  For Asteroids this means a wireframe model that straddles
 * the screen edge has a straight cut — acceptable since the game wraps
 * entire objects, not individual edges.
 */
void draw_line(Backbuffer *backbuffer,
               float x0f, float y0f, float x1f, float y1f,
               float r, float g, float b, float a) {
  if (!backbuffer || !backbuffer->pixels) return;
  if (a <= 0.0f) return;

  int ix0 = (int)x0f;
  int iy0 = (int)y0f;
  int ix1 = (int)x1f;
  int iy1 = (int)y1f;

  /* Convert float color to 8-bit once. */
  float cr = CLAMP(r, 0.0f, 1.0f);
  float cg = CLAMP(g, 0.0f, 1.0f);
  float cb = CLAMP(b, 0.0f, 1.0f);

  int src_a = (int)(CLAMP(a, 0.0f, 1.0f) * 255.0f);
  int inv_a = 255 - src_a;
  int src_r = (int)(cr * 255.0f);
  int src_g = (int)(cg * 255.0f);
  int src_b = (int)(cb * 255.0f);

  int stride = backbuffer->pitch / backbuffer->bytes_per_pixel;
  int bw = backbuffer->width;
  int bh = backbuffer->height;

  /* LESSON 05 — Bresenham setup.
   * If the line is "steep" (dy > dx) we swap x and y axes so the outer
   * loop always steps one unit in the dominant axis.                      */
  int steep = 0;
  {
    int adx = ix1 - ix0; if (adx < 0) adx = -adx;
    int ady = iy1 - iy0; if (ady < 0) ady = -ady;
    if (ady > adx) steep = 1;
  }

  if (steep) {
    int tmp;
    tmp = ix0; ix0 = iy0; iy0 = tmp;
    tmp = ix1; ix1 = iy1; iy1 = tmp;
  }

  /* Ensure we always step left-to-right (or top-to-bottom when steep). */
  if (ix0 > ix1) {
    int tmp;
    tmp = ix0; ix0 = ix1; ix1 = tmp;
    tmp = iy0; iy0 = iy1; iy1 = tmp;
  }

  int dx = ix1 - ix0;
  int dy = iy1 - iy0;
  int y_step = (dy < 0) ? -1 : 1;
  if (dy < 0) dy = -dy;

  int error = dx / 2;
  int y = iy0;

  for (int x = ix0; x <= ix1; x++) {
    /* Map back to screen coordinates. */
    int px = steep ? y : x;
    int py = steep ? x : y;

    /* Clip to backbuffer bounds. */
    if (px >= 0 && px < bw && py >= 0 && py < bh) {
      uint32_t *dst = backbuffer->pixels + py * stride + px;

      if (src_a >= 255) {
        /* Opaque fast path */
        *dst = (uint32_t)src_r
             | ((uint32_t)src_g << 8)
             | ((uint32_t)src_b << 16)
             | 0xFF000000u;
      } else {
        /* Alpha blend */
        uint32_t dest_pixel = *dst;
        int dest_r = (dest_pixel      ) & 0xFF;
        int dest_g = (dest_pixel >>  8) & 0xFF;
        int dest_b = (dest_pixel >> 16) & 0xFF;

        int out_r = (src_r * src_a + dest_r * inv_a) >> 8;
        int out_g = (src_g * src_a + dest_g * inv_a) >> 8;
        int out_b = (src_b * src_a + dest_b * inv_a) >> 8;

        *dst = (uint32_t)out_r
             | ((uint32_t)out_g << 8)
             | ((uint32_t)out_b << 16)
             | 0xFF000000u;
      }
    }

    error -= dy;
    if (error < 0) {
      y += y_step;
      error += dx;
    }
  }
}
