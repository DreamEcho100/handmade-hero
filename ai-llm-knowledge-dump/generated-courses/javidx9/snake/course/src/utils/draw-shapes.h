/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-shapes.h  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * Public API declarations for the rectangle drawing primitives.
 * Implementations live in draw-shapes.c.
 *
 * WHY SEPARATE .h AND .c?
 * ────────────────────────
 * The header contains only declarations (what functions exist, their types).
 * The .c file contains the implementation (what the functions actually do).
 *
 * Any .c file that needs to call these functions includes this header.
 * The linker then connects the call site to the implementation.
 *
 * JS equivalent: like exporting just the function signatures from a TypeScript
 * .d.ts file, separate from the .js implementation.
 */

#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H

#include <stdint.h>           /* uint32_t */
#include "backbuffer.h"       /* SnakeBackbuffer */

/* draw_rect — fill a solid rectangle with `color`.
 *
 * Clips the rectangle to the backbuffer bounds automatically — if (x, y)
 * is off-screen or (w, h) extends past the edge, only the visible portion
 * is drawn.  This prevents buffer overruns when drawing near the edges.
 *
 * Coordinate system: (0, 0) is top-left.  x increases right; y increases down.
 *
 * @param bb     The pixel buffer to draw into
 * @param x      Left edge in pixels (may be negative — clipping handles it)
 * @param y      Top edge in pixels
 * @param w      Width in pixels
 * @param h      Height in pixels
 * @param color  Packed ARGB colour (use GAME_RGB() or a COLOR_* constant)
 *
 * JS equivalent: ctx.fillRect(x, y, w, h) — but operating on raw pixels.
 */
void draw_rect(SnakeBackbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* draw_rect_blend — alpha-blend a rectangle over the existing pixels.
 *
 * Uses standard "over" compositing: each output pixel is a mix of the source
 * color and the destination pixel, weighted by the source alpha.
 *
 *   out = (src * alpha + dst * (255 - alpha)) / 255
 *
 * If `color` has alpha = 255 (fully opaque), delegates to draw_rect for speed.
 * If `color` has alpha = 0  (fully transparent), returns immediately (no-op).
 *
 * Used for the semi-transparent game-over overlay in Lesson 12.
 *
 * JS equivalent: ctx.globalAlpha = alpha/255; ctx.fillRect(…) — but applied
 * manually per pixel so we control exactly which buffer it writes to.
 */
void draw_rect_blend(SnakeBackbuffer *bb, int x, int y, int w, int h, uint32_t color);

#endif /* UTILS_DRAW_SHAPES_H */
