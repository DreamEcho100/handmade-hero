#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H

/* =============================================================================
 * utils/draw-shapes.h — Rectangle Drawing Declarations
 * =============================================================================
 *
 * This header declares the functions for drawing filled rectangles into a
 * Backbuffer. There are two variants:
 *   draw_rect       — solid (ignores alpha, always overwrites destination)
 *   draw_rect_blend — respects the alpha channel (blends with what's underneath)
 *
 * JS equivalent:
 *   draw_rect       ≈ ctx.fillRect(x, y, w, h)  with ctx.globalAlpha = 1
 *   draw_rect_blend ≈ ctx.fillRect(x, y, w, h)  with ctx.globalAlpha = a/255
 * =============================================================================
 */

/* Include backbuffer.h so the Backbuffer type is visible to any file that
 * includes this header. Consumers of draw-shapes.h don't need to separately
 * include backbuffer.h. */
#include "backbuffer.h"

/* stdint.h for uint32_t used in the color parameter. */
#include <stdint.h>

/* ══════ Function Declarations ══════
 *
 * In C a "declaration" (also called a "prototype") tells the compiler the
 * function's name, its parameter types, and its return type — but NOT the
 * body. The body lives in the corresponding .c file.
 *
 * JS equivalent: TypeScript interface / .d.ts declaration file.
 *   declare function drawRect(bb: Backbuffer, x: number, y: number,
 *                             w: number, h: number, color: number): void;
 */

/* draw_rect — Draw a solid filled rectangle.
 *
 * Parameters:
 *   bb     — Pointer to the Backbuffer to draw into.
 *            JS: passing an object by reference (objects are always refs in JS).
 *            C:  Backbuffer* means "pointer to a Backbuffer". We pass a pointer
 *                so the function can modify the backbuffer's pixels directly
 *                without copying the struct.
 *
 *   x, y   — Top-left corner of the rectangle in screen pixels.
 *             (0,0) is the top-left of the screen; x grows right, y grows down.
 *
 *   w, h   — Width and height of the rectangle in pixels.
 *
 *   color  — Packed ARGB color created with GAME_RGB() or GAME_RGBA().
 *            The alpha channel is IGNORED by this function — use draw_rect_blend
 *            if you need transparency.
 *
 * Returns: void (nothing).
 *
 * Note: coordinates outside the backbuffer bounds are clipped automatically.
 * Passing negative x/y or x+w > bb->width is safe.
 */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* draw_rect_blend — Draw a rectangle with alpha blending.
 *
 * Same as draw_rect, but reads the alpha byte from `color` and blends each
 * pixel with what is already in the backbuffer:
 *
 *   out = (src * alpha + dst * (255 - alpha)) / 255
 *
 * where src = the color argument, dst = the existing pixel value.
 *
 * Special cases (optimised):
 *   alpha == 255 → delegates to draw_rect (no blending needed, fully opaque)
 *   alpha ==   0 → early return (fully transparent, nothing to draw)
 *
 * JS equivalent:
 *   ctx.globalAlpha = color.a / 255;
 *   ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
 *   ctx.fillRect(x, y, w, h);
 *   ctx.globalAlpha = 1; // reset
 */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h,
                     uint32_t color);

#endif /* UTILS_DRAW_SHAPES_H */
