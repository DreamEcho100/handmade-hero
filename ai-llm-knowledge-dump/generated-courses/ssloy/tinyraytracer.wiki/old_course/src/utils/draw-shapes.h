#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-shapes.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 04 — draw_rect (solid fill), draw_rect_blend (alpha composite).
 *             Pitch-based row pointer arithmetic.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include "./backbuffer.h"

/* draw_rect — fill axis-aligned rectangle with a solid color.
 * Clips to backbuffer bounds automatically.
 *
 * LESSON 04: Why we use `bb->pitch / 4` (not `bb->width`) to advance rows:
 * pitch is the number of *bytes* per row; dividing by bytes-per-pixel gives
 * the number of uint32_t slots per row, which may differ from the logical
 * width if the platform added padding. */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* draw_rect_blend — fill rectangle with per-channel alpha composite.
 * Uses the alpha byte of `color` to blend against the existing pixel.
 *
 * LESSON 04: src_a / 255 blend formula explained. */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

#endif /* UTILS_DRAW_SHAPES_H */
