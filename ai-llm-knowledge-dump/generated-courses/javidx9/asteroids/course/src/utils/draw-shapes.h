#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-shapes.h — Primitive Drawing (Rects + Lines)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 03 — draw_rect: pure rasterizer with float positions and colors.
 *             Pitch-based row pointer arithmetic, unified alpha blending.
 *
 * LESSON 05 — draw_line: Bresenham integer line algorithm for wireframe
 *             geometry (ship, asteroids, bullets).
 *
 * Both functions take pixel coordinates as floats and float RGBA colors.
 * They know NOTHING about coordinate systems or game units — they just fill
 * pixels.  Coordinate conversion (game units → pixels) happens at the
 * CALL SITE via render_explicit.h:
 *
 *   draw_rect(bb,
 *     world_rect_px_x(&ctx, wx, ww), world_rect_px_y(&ctx, wy, wh),
 *     world_w(&ctx, ww), world_h(&ctx, wh),
 *     COLOR_WHITE);
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
 * pitch / bytes_per_pixel gives stride in uint32_t units.               */
void draw_rect(Backbuffer *backbuffer,
               float x, float y, float w, float h,
               float r, float g, float b, float a);

/* draw_line — draw a straight line using Bresenham's algorithm.
 *
 * x0, y0: start point in pixel space (float, truncated to int internally)
 * x1, y1: end   point in pixel space (float, truncated to int internally)
 * r, g, b, a: color channels [0.0–1.0]
 *
 * Clips each pixel to backbuffer bounds (clipped, NOT toroidal).
 * Used by draw_wireframe to render ship and asteroid outlines.
 *
 * LESSON 05 — Bresenham's algorithm rasterizes a line with no floating-
 * point arithmetic in the inner loop — only integer additions.           */
void draw_line(Backbuffer *backbuffer,
               float x0, float y0, float x1, float y1,
               float r, float g, float b, float a);

#endif /* UTILS_DRAW_SHAPES_H */
