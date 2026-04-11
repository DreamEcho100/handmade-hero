#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-shapes.h — TinyRaytracer Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 04 — draw_rect: pure rasterizer with float positions and colors.
 *             Pitch-based row pointer arithmetic, unified alpha blending.
 *
 * draw_rect takes pixel coordinates as floats and float RGBA colors.
 * It knows NOTHING about coordinate systems or game units — it just fills
 * pixels.  Coordinate conversion (game units → pixels) happens at the
 * CALL SITE, using a scale factor like `units_to_pixels` (see demo.c).
 *
 * This follows the Handmade Hero pattern:
 *   draw_rect(backbuffer, x * m2p, y * m2p, w * m2p, h * m2p, r, g, b, a);
 *                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *                 conversion at call site, not inside draw_rect
 *
 * Alpha blending is handled automatically:
 *   a >= 1.0 → opaque (direct write, fast path)
 *   a <= 0.0 → transparent (skip)
 *   else     → per-pixel blend: out = src*a + dst*(1-a)
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./backbuffer.h"

/* draw_rect — fill axis-aligned rectangle (pure rasterizer).
 *
 * x, y: top-left corner in pixel space (float for sub-pixel precision)
 * w, h: size in pixels (float)
 * r, g, b, a: color channels [0.0–1.0]
 *
 * Clips to backbuffer bounds automatically.
 *
 * LESSON 04: Why we use `backbuffer->pitch / 4` (not `backbuffer->width`) to advance rows:
 * pitch is the number of *bytes* per row; dividing by bytes-per-pixel gives
 * the number of uint32_t slots per row, which may differ from the logical
 * width if the platform added padding. */
void draw_rect(Backbuffer *backbuffer,
               float x, float y, float w, float h,
               float r, float g, float b, float a);

#endif /* UTILS_DRAW_SHAPES_H */
