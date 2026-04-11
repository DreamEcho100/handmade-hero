/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-shapes.c — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 04 — draw_rect: pure rasterizer implementation.
 *             Float positions/colors, unified alpha blending, pitch-based
 *             row pointer, clipping.
 *
 * This function is coordinate-system-agnostic: it takes pixel values and
 * draws.  Any game-unit → pixel conversion happens at the call site.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./draw-shapes.h"
#include "./math.h"

/* ─────────────────────────────────────────────────────────────────────────
 * draw_rect
 * ─────────────────────────────────────────────────────────────────────────
 * Fill a rectangle at pixel coordinates with float colors.
 *
 * LESSON 04 — Three-tier alpha handling (like Handmade Hero):
 *   1. a <= 0  → skip entirely (fully transparent)
 *   2. a >= 1  → direct pixel write (opaque fast path, no read-back)
 *   3. else    → per-pixel alpha composite: out = src*a + dst*(1-a)
 *
 * Float colors [0.0–1.0] are converted to 8-bit at rasterization time.
 * Float positions allow sub-pixel precision; truncation to int happens here.
 *
 * LESSON 04 — Two-step clip:
 *   1. Compute pixel bounds from float position + size.
 *   2. Clamp to [0, width/height].
 *   3. Skip if clipped area is zero.
 */
void draw_rect(Backbuffer *backbuffer,
               float x, float y, float w, float h,
               float r, float g, float b, float a) {
  if (!backbuffer || !backbuffer->pixels) return;
  if (a <= 0.0f) return;

  /* Clamp color channels to valid range. */
  float clamped_r = CLAMP(r, 0.0f, 1.0f);
  float clamped_g = CLAMP(g, 0.0f, 1.0f);
  float clamped_b = CLAMP(b, 0.0f, 1.0f);
  float clamped_a = CLAMP(a, 0.0f, 1.0f);

  /* Convert float positions to pixel bounds and clip. */
  int x0 = MAX((int)x, 0);
  int y0 = MAX((int)y, 0);
  int x1 = MIN((int)(x + w), backbuffer->width);
  int y1 = MIN((int)(y + h), backbuffer->height);

  if (x0 >= x1 || y0 >= y1) return;

  /* LESSON 04 — pitch / bytes_per_pixel gives the stride in uint32_t units.
   * We store this once, then do one addition per row instead of one
   * multiplication. */
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
     * LESSON 04 — Blend formula (per channel):
     *   out = src * a/255 + dst * (1 - a/255)
     *       = (src * src_a + dst * inv_alpha) / 255
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
