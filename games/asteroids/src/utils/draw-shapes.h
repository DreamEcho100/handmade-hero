#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H

#include "backbuffer.h"

/* draw_rect: fill a rectangle in the backbuffer.
 *
 * Parameters:
 *   x, y, w, h  — position and size in PIXELS (float for sub-pixel precision)
 *   r, g, b     — color channels as floats in [0.0, 1.0]
 *   a            — alpha (opacity) as float: 0.0 = invisible, 1.0 = fully
 * opaque
 *
 * Behavior:
 *   a <= 0  → skip entirely (fully transparent)
 *   a >= 1  → direct write (fully opaque, fast path)
 *   0 < a < 1 → per-pixel alpha blend with existing backbuffer content
 *
 * Coordinates are clipped to the backbuffer bounds before writing.
 * The caller is responsible for unit-to-pixel conversion if using game units.
 */
void draw_rect(Backbuffer *backbuffer, float x, float y, float w, float h,
               float r, float g, float b, float a);

#endif // UTILS_DRAW_SHAPES_H