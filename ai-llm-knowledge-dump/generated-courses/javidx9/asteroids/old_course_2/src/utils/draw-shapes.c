/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-shapes.c — Primitive Drawing Implementations
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * PIXEL INDEXING — READ THIS FIRST
 * ─────────────────────────────────
 * Pixels are stored in a flat (1-D) array, row by row, left to right.
 * To access pixel at column x, row y:
 *
 *   index = y * stride + x      where  stride = pitch / bytes_per_pixel
 *
 * Why (pitch / bytes_per_pixel)?
 *   pitch is the byte width of one row (e.g. 512 * 4 = 2048 bytes).
 *   backbuffer->pixels is uint32_t* (4 bytes per element).
 *   Dividing pitch by 4 (bytes_per_pixel) converts from byte-offset to
 *   uint32_t element-offset.
 *
 * Toroidal modular arithmetic:
 *   ((x % w) + w) % w   →  always in [0, w-1], works for negative x too.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./draw-shapes.h"
#include "./math.h"

/* ── draw_rect ─────────────────────────────────────────────────────────────
 *
 * LESSON 01 — Three-tier alpha handling (like Handmade Hero):
 *   1. a <= 0  → skip entirely (fully transparent)
 *   2. a >= 1  → direct pixel write (opaque fast path, no read-back)
 *   3. else    → per-pixel alpha composite: out = src*a + dst*(1-a)
 *
 * Float colors [0.0–1.0] are converted to 8-bit at rasterization time.
 * Float positions allow sub-pixel precision; truncation to int happens here.
 */
void draw_rect(Backbuffer *bb,
               float x, float y, float w, float h,
               float r, float g, float b, float a) {
  if (!bb || !bb->pixels) return;
  if (a <= 0.0f) return;

  float cr = CLAMP(r, 0.0f, 1.0f);
  float cg = CLAMP(g, 0.0f, 1.0f);
  float cb = CLAMP(b, 0.0f, 1.0f);
  float ca = CLAMP(a, 0.0f, 1.0f);

  int x0 = MAX((int)x, 0);
  int y0 = MAX((int)y, 0);
  int x1 = MIN((int)(x + w), bb->width);
  int y1 = MIN((int)(y + h), bb->height);

  if (x0 >= x1 || y0 >= y1) return;

  /* pitch / bytes_per_pixel gives stride in uint32_t units. */
  int stride = bb->pitch / bb->bytes_per_pixel;

  if (ca >= 1.0f) {
    /* ── Opaque fast path: no read-back needed ────────────────────────── */
    uint32_t color = (uint32_t)(cr * 255.0f)
                   | ((uint32_t)(cg * 255.0f) << 8)
                   | ((uint32_t)(cb * 255.0f) << 16)
                   | 0xFF000000u;
    for (int row = y0; row < y1; row++) {
      uint32_t *dst = bb->pixels + row * stride + x0;
      for (int col = x0; col < x1; col++) {
        *dst++ = color;
      }
    }
  } else {
    /* ── Alpha blend path ─────────────────────────────────────────────── */
    int src_a   = (int)(ca * 255.0f);
    int inv_a   = 255 - src_a;
    int src_r   = (int)(cr * 255.0f);
    int src_g   = (int)(cg * 255.0f);
    int src_b   = (int)(cb * 255.0f);

    for (int row = y0; row < y1; row++) {
      uint32_t *dst = bb->pixels + row * stride + x0;
      for (int col = x0; col < x1; col++) {
        uint32_t dp = *dst;
        int dr = (dp      ) & 0xFF;
        int dg = (dp >>  8) & 0xFF;
        int db = (dp >> 16) & 0xFF;

        int or = (src_r * src_a + dr * inv_a) >> 8;
        int og = (src_g * src_a + dg * inv_a) >> 8;
        int ob = (src_b * src_a + db * inv_a) >> 8;

        *dst++ = (uint32_t)or
               | ((uint32_t)og << 8)
               | ((uint32_t)ob << 16)
               | 0xFF000000u;
      }
    }
  }
}

/* ── draw_pixel_w — Toroidal Pixel Write ────────────────────────────────────
 *
 * Wraps x and y into the backbuffer using modular arithmetic.
 * The double-mod trick handles negative x correctly (C99 guarantees
 * ((x % w) + w) % w is in [0, w-1] for w > 0).                           */
void draw_pixel_w(Backbuffer *bb, int x, int y, uint32_t color) {
  x = ((x % bb->width)  + bb->width)  % bb->width;
  y = ((y % bb->height) + bb->height) % bb->height;
  bb->pixels[y * (bb->pitch / 4) + x] = color;
}

/* ── draw_pixel — Clipped Pixel Write ───────────────────────────────────── */
void draw_pixel(Backbuffer *bb, int x, int y, uint32_t color) {
  if (x < 0 || x >= bb->width || y < 0 || y >= bb->height) return;
  bb->pixels[y * (bb->pitch / 4) + x] = color;
}

/* ── draw_line — Bresenham's Line Algorithm (Toroidal) ──────────────────────
 *
 * Bresenham's algorithm draws a rasterised line using only integer arithmetic.
 * It tracks the accumulated error between the ideal (floating-point) line and
 * the nearest grid pixel, stepping the "minor" axis when the error overflows.
 *
 * Key variables:
 *   dx, dy  — total extent in x and y (always positive)
 *   sx, sy  — step direction (+1 or -1)
 *   err     — accumulated error × 2 (multiplied to avoid fractions)
 *
 * Each pixel is written via draw_pixel_w so the line wraps toroidally.
 * This handles wireframe objects whose vertices straddle a screen edge.   */
void draw_line(Backbuffer *bb,
               int x0, int y0, int x1, int y1,
               uint32_t color) {
  int dx = x1 - x0; if (dx < 0) dx = -dx;
  int dy = y1 - y0; if (dy < 0) dy = -dy;
  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx - dy;

  for (;;) {
    draw_pixel_w(bb, x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = err * 2;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 <  dx) { err += dx; y0 += sy; }
  }
}
