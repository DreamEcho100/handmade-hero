/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-shapes.c  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * Implements draw_rect() and draw_rect_blend() — the two lowest-level drawing
 * functions in the game.  Everything the player sees is built from these two.
 *
 * COURSE NOTE: In lessons 01–10, this code lives inline inside game.c for
 * simplicity.  Lesson 12 refactors it here, teaching when and why the
 * utils/ split pays off.  See COURSE-BUILDER-IMPROVEMENTS.md for the rationale.
 */

#include "draw-shapes.h"  /* Our own declarations                            */

/* ══════ draw_rect ══════════════════════════════════════════════════════════
 *
 * PIXEL INDEX FORMULA — why `py * (pitch/4) + px`?
 * ──────────────────────────────────────────────────
 * The backbuffer stores pixels in a single flat 1-D array (no 2-D array).
 * To address pixel at column `px`, row `py`:
 *
 *   index = py * pixels_per_row + px
 *         = py * (pitch / 4)   + px
 *
 * `pitch` is bytes per row.  Each pixel is 4 bytes (uint32_t).
 * Dividing pitch by 4 gives pixels per row.
 *
 * Example: backbuffer is 1200×460, pitch = 1200×4 = 4800 bytes.
 *   Pixel at column 5, row 10:  index = 10 * 1200 + 5 = 12005
 *
 * JS equivalent: the flat index into a Uint32Array:
 *   index = py * imageData.width + px
 *
 * CLIPPING — why clip?
 * ─────────────────────
 * Without clipping, drawing a rect that extends past the edge writes pixels
 * beyond the end of the array — undefined behaviour in C (and a crash or
 * corruption in practice).  Pre-computing the clamped bounds means the inner
 * loop never needs to check — it just runs as fast as the CPU allows.
 *
 * JS equivalent: the canvas clips automatically; in C we must do it ourselves.
 */
void draw_rect(SnakeBackbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    /* Clamp rectangle to backbuffer bounds.
     * x0/y0 = clamped start (inclusive), x1/y1 = clamped end (exclusive).
     * JS: Math.max(0, x) and Math.min(bb->width, x + w) */
    int x0 = x < 0       ? 0          : x;
    int y0 = y < 0       ? 0          : y;
    int x1 = x + w > bb->width  ? bb->width  : x + w;
    int y1 = y + h > bb->height ? bb->height : y + h;

    int px, py;
    for (py = y0; py < y1; py++) {
        for (px = x0; px < x1; px++) {
            /* Write one pixel.  `pitch / 4` converts byte-stride to pixel-stride.
             * JS: imageData.data[py * imageData.width + px] = color */
            bb->pixels[py * (bb->pitch / 4) + px] = color;
        }
    }
}

/* ══════ draw_rect_blend ════════════════════════════════════════════════════
 *
 * ALPHA BLENDING — the "over" compositing formula
 * ──────────────────────────────────────────────────
 * Standard linear blend (same formula used everywhere from CSS to Photoshop):
 *
 *   out = (src × alpha + dst × (255 - alpha)) / 255
 *
 * where alpha is the source alpha byte (0 = transparent, 255 = opaque).
 * We apply this independently to R, G, and B channels.
 *
 * Example: 50% transparent black overlay (GAME_RGBA(0,0,0,128)) over white:
 *   out_r = (0 × 128 + 255 × 127) / 255 ≈ 127  → 50% grey
 *
 * WHY INTEGER MATH?
 *   Floating-point division in a per-pixel loop adds up.  Integer arithmetic
 *   is faster and the /255 loss of precision is imperceptible at 8-bit colour.
 *
 * OPTIMISATION — early exit for opaque/transparent
 *   alpha == 255: fully opaque — delegate to draw_rect (no blending needed).
 *   alpha ==   0: fully transparent — nothing to draw.
 *   These two fast paths avoid the blending math in the common cases.
 *
 * JS equivalent: ctx.globalAlpha followed by fillRect — done manually here
 * so we control exactly which pixels are affected.
 */
void draw_rect_blend(SnakeBackbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    int alpha = (int)((color >> 24) & 0xFF);

    /* Fast path: fully opaque — just fill, no blending math */
    if (alpha == 255) {
        draw_rect(bb, x, y, w, h, color);
        return;
    }
    /* Fast path: fully transparent — nothing to draw */
    if (alpha == 0) return;

    /* Extract source RGB channels.
     * With GAME_RGBA storing 0xAABBGGRR:  R = bits 0-7, G = bits 8-15, B = bits 16-23 */
    int src_r = (int)( color        & 0xFF);  /* R in low byte   */
    int src_g = (int)((color >>  8) & 0xFF);  /* G in next byte  */
    int src_b = (int)((color >> 16) & 0xFF);  /* B in high byte  */
    int inv_a = 255 - alpha;  /* complement: weight of the destination */

    /* Clip to buffer bounds (same as draw_rect) */
    int x0 = x < 0       ? 0          : x;
    int y0 = y < 0       ? 0          : y;
    int x1 = x + w > bb->width  ? bb->width  : x + w;
    int y1 = y + h > bb->height ? bb->height : y + h;

    int px, py;
    for (py = y0; py < y1; py++) {
        for (px = x0; px < x1; px++) {
            uint32_t *dst = &bb->pixels[py * (bb->pitch / 4) + px];

            /* Read destination pixel channels (same layout: R=bits 0-7, G=8-15, B=16-23) */
            int dst_r = (int)( *dst        & 0xFF);
            int dst_g = (int)((*dst >>  8) & 0xFF);
            int dst_b = (int)((*dst >> 16) & 0xFF);

            /* Blend: out = (src * a + dst * (255-a)) / 255 */
            int out_r = (src_r * alpha + dst_r * inv_a) / 255;
            int out_g = (src_g * alpha + dst_g * inv_a) / 255;
            int out_b = (src_b * alpha + dst_b * inv_a) / 255;

            /* Write back as fully-opaque packed pixel */
            *dst = GAME_RGB(out_r, out_g, out_b);
        }
    }
}
