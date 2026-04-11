/* =============================================================================
 * utils/draw-shapes.c — Primitive Drawing Implementations
 * =============================================================================
 *
 * PIXEL INDEXING — READ THIS FIRST
 * ─────────────────────────────────
 * Pixels are stored in a flat (1-D) array, row by row, left to right.
 * To access pixel at column x, row y:
 *
 *   index = y * (bb->pitch / 4) + x
 *
 * Why (pitch / 4)?
 *   pitch is the byte width of one row (e.g. 512 * 4 = 2048 bytes).
 *   bb->pixels is uint32_t* (4 bytes per element).
 *   Dividing pitch by 4 converts from byte-offset to uint32_t element-offset.
 *
 * Toroidal modular arithmetic:
 *   ((x % w) + w) % w   →  always in [0, w-1], works for negative x too.
 *   JavaScript equivalent:  ((x % w) + w) % w  (same formula!)
 * =============================================================================
 */

#include "draw-shapes.h"

/* ══════ draw_pixel_w — Toroidal Pixel Write ════════════════════════════════

   Wraps x and y into the backbuffer using modular arithmetic.
   COURSE NOTE: The reference asteroids.c uses `x % width` directly, which
   in C gives a negative result for negative x (undefined behaviour on some
   platforms before C99).  The double-mod trick is the portable fix.      */
void draw_pixel_w(AsteroidsBackbuffer *bb, int x, int y, uint32_t color) {
    /* Wrap x: handle negative values with the double-mod trick.
       Same formula as JS:  ((x % w) + w) % w                             */
    x = ((x % bb->width)  + bb->width)  % bb->width;
    y = ((y % bb->height) + bb->height) % bb->height;

    /* Row-major index: y rows down, then x columns across.
       Dividing pitch (bytes) by 4 gives uint32_t element offset.         */
    bb->pixels[y * (bb->pitch / 4) + x] = color;
}

/* ══════ draw_pixel — Clipped Pixel Write ═══════════════════════════════════ */
void draw_pixel(AsteroidsBackbuffer *bb, int x, int y, uint32_t color) {
    if (x < 0 || x >= bb->width || y < 0 || y >= bb->height) return;
    bb->pixels[y * (bb->pitch / 4) + x] = color;
}

/* ══════ draw_line — Bresenham's Line Algorithm (Toroidal) ══════════════════
 *
 * Bresenham's algorithm draws a rasterised line using only integer arithmetic.
 * It tracks the accumulated error between the ideal (floating-point) line and
 * the nearest grid pixel, stepping the "minor" axis when the error overflows.
 *
 * Key variables:
 *   dx, dy   — total extent in x and y (always positive)
 *   sx, sy   — step direction (+1 or -1)
 *   err      — accumulated error × 2 (multiplied to avoid fractions)
 *
 * Why toroidal?  Each asteroid, ship, and bullet is drawn as a wireframe
 * (series of line segments).  If an asteroid drifts off the right edge, the
 * line connecting its last vertex to its first must wrap around so the shape
 * appears seamlessly from the left edge.
 */
void draw_line(AsteroidsBackbuffer *bb,
               int x0, int y0, int x1, int y1,
               uint32_t color)
{
    /* Compute absolute extent */
    int dx = x1 - x0;  if (dx < 0) dx = -dx;
    int dy = y1 - y0;  if (dy < 0) dy = -dy;

    /* Step direction: +1 if moving right/down, -1 if moving left/up */
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    /* Initial error: start at midpoint between first and second pixel */
    int err = dx - dy;

    /* Draw pixels until we reach the end point */
    for (;;) {
        /* Each pixel goes through the toroidal writer so lines wrap. */
        draw_pixel_w(bb, x0, y0, color);

        /* Are we done? */
        if (x0 == x1 && y0 == y1) break;

        /* Double the error to keep it in integer space.               */
        int e2 = err * 2;

        /* Step x if the accumulated error is large enough in x-axis.  */
        if (e2 > -dy) { err -= dy; x0 += sx; }

        /* Step y if the accumulated error is large enough in y-axis.  */
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

/* ══════ draw_rect — Clipped Solid Rectangle ════════════════════════════════ */
void draw_rect(AsteroidsBackbuffer *bb,
               int x, int y, int w, int h,
               uint32_t color)
{
    /* Clamp to backbuffer bounds (no toroidal wrap — HUD must stay on screen) */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;  if (x1 > bb->width)  x1 = bb->width;
    int y1 = y + h;  if (y1 > bb->height) y1 = bb->height;

    int stride = bb->pitch / 4;  /* uint32_t elements per row */
    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            bb->pixels[py * stride + px] = color;
        }
    }
}

/* ══════ draw_rect_blend — Alpha-Blended Rectangle ══════════════════════════
 *
 * Standard "over" compositing (Porter-Duff):
 *   result = src_alpha/255 * src_colour  +  (1 - src_alpha/255) * dst_colour
 *
 * CHANNEL EXTRACTION (CRITICAL — matches GAME_RGBA byte layout):
 *   uint32_t value = 0xAABBGGRR  (little-endian memory: [R][G][B][A])
 *   R = bits  0-7  → (color >>  0) & 0xFF
 *   G = bits  8-15 → (color >>  8) & 0xFF
 *   B = bits 16-23 → (color >> 16) & 0xFF
 *   A = bits 24-31 → (color >> 24) & 0xFF
 *
 * JS equivalent (Canvas 2D API):
 *   ctx.globalAlpha = alpha / 255;
 *   ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
 *   ctx.fillRect(x, y, w, h);
 */
void draw_rect_blend(AsteroidsBackbuffer *bb,
                     int x, int y, int w, int h,
                     uint32_t color)
{
    /* Extract source channels — R is the LOW byte (bits 0-7) */
    uint8_t sa = (uint8_t)((color >> 24) & 0xFF);  /* alpha         */
    uint8_t sr = (uint8_t)((color >>  0) & 0xFF);  /* red   (low)   */
    uint8_t sg = (uint8_t)((color >>  8) & 0xFF);  /* green (mid)   */
    uint8_t sb = (uint8_t)((color >> 16) & 0xFF);  /* blue  (high)  */

    if (sa == 0) return;    /* fully transparent — nothing to draw */

    /* Clamp to backbuffer bounds */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;  if (x1 > bb->width)  x1 = bb->width;
    int y1 = y + h;  if (y1 > bb->height) y1 = bb->height;

    int stride = bb->pitch / 4;

    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            uint32_t dst = bb->pixels[py * stride + px];

            /* Extract destination channels (same byte layout) */
            uint8_t dr = (uint8_t)((dst >>  0) & 0xFF);
            uint8_t dg = (uint8_t)((dst >>  8) & 0xFF);
            uint8_t db = (uint8_t)((dst >> 16) & 0xFF);

            /* Linear blend:  result = src * (alpha/255) + dst * (1 - alpha/255)
               Integer arithmetic: multiply by alpha, then divide by 255.
               +127 rounds to nearest rather than truncating.              */
            uint8_t rr = (uint8_t)((sr * sa + dr * (255 - sa) + 127) / 255);
            uint8_t rg = (uint8_t)((sg * sa + dg * (255 - sa) + 127) / 255);
            uint8_t rb = (uint8_t)((sb * sa + db * (255 - sa) + 127) / 255);

            /* Pack result back into GAME_RGBA byte order (R in low byte)  */
            bb->pixels[py * stride + px] =
                ((uint32_t)255 << 24) |
                ((uint32_t)rb  << 16) |
                ((uint32_t)rg  <<  8) |
                ((uint32_t)rr  <<  0);
        }
    }
}
