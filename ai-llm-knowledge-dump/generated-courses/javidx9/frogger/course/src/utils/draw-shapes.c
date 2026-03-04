/* =============================================================================
 * utils/draw-shapes.c — Primitive Drawing (Rects + Sprite Partial)
 * =============================================================================
 *
 * Implements the three drawing primitives declared in draw-shapes.h.
 * Zero platform dependencies — only depends on Backbuffer and standard C.
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "draw-shapes.h"
#include <stdint.h>

/* ══════ draw_rect ══════════════════════════════════════════════════════════

   Solid filled rectangle, clamped to buffer bounds.

   Clamping instead of skipping: if the rect partially overlaps the screen
   we still draw the visible portion.  Use cases: lane backgrounds near the
   edge always extend exactly to the boundary.

   JS equivalent:
     ctx.fillStyle = colorToCSS(color);
     ctx.fillRect(x, y, w, h);                                           */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    /* Clamp source rectangle to buffer bounds */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > bb->width)  ? bb->width  : x + w;
    int y1 = (y + h > bb->height) ? bb->height : y + h;

    int row, col;
    int stride = bb->pitch / 4;  /* uint32_t elements per row */
    for (row = y0; row < y1; row++)
        for (col = x0; col < x1; col++)
            bb->pixels[row * stride + col] = color;
}

/* ══════ draw_rect_blend ════════════════════════════════════════════════════

   Alpha-blended rectangle.  The source alpha controls blending:
     out = (src * alpha + dst * (255 - alpha)) / 255

   Channel extraction uses shifts and masks matching our GAME_RGBA layout:
     R = bits  0–7  (low byte)
     G = bits  8–15
     B = bits 16–23
     A = bits 24–31 (high byte)

   JS equivalent:
     ctx.globalAlpha = alpha / 255;
     ctx.fillRect(x, y, w, h);
     ctx.globalAlpha = 1.0;                                              */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > bb->width)  ? bb->width  : x + w;
    int y1 = (y + h > bb->height) ? bb->height : y + h;

    /* Extract source channels from GAME_RGBA (R=low byte, A=high byte)  */
    uint8_t sa = (uint8_t)((color >> 24) & 0xFF);
    uint8_t sb = (uint8_t)((color >> 16) & 0xFF);
    uint8_t sg = (uint8_t)((color >>  8) & 0xFF);
    uint8_t sr = (uint8_t)( color        & 0xFF);

    int row, col;
    int stride = bb->pitch / 4;
    for (row = y0; row < y1; row++) {
        for (col = x0; col < x1; col++) {
            uint32_t dst = bb->pixels[row * stride + col];
            uint8_t db = (uint8_t)((dst >> 16) & 0xFF);
            uint8_t dg = (uint8_t)((dst >>  8) & 0xFF);
            uint8_t dr = (uint8_t)( dst         & 0xFF);

            /* Linear blend: out = (src * a + dst * (255-a)) / 255       */
            uint8_t or_ = (uint8_t)((sr * sa + dr * (255 - sa)) / 255);
            uint8_t og  = (uint8_t)((sg * sa + dg * (255 - sa)) / 255);
            uint8_t ob  = (uint8_t)((sb * sa + db * (255 - sa)) / 255);

            bb->pixels[row * stride + col] = GAME_RGB(or_, og, ob);
        }
    }
}

/* ══════ draw_sprite_partial ════════════════════════════════════════════════

   Renders a rectangular sub-region of a sprite sheet into the backbuffer.

   WHAT IS "PARTIAL"?
   ──────────────────
   A log sprite sheet is 24 cells wide (start | mid | end segments).
   Each lane tile only needs one 8-cell segment.  We specify the source
   region in cells (src_x, src_y, src_w, src_h) and render only that slice.

   JS equivalent:
     ctx.drawImage(spriteSheet, sx*8, sy*8, sw*8, sh*8, dx, dy, sw*8, sh*8);
   (exactly the 9-argument form of drawImage with a source rect)

   TRANSPARENCY:
     glyph == 0x0020 (space) means "transparent cell" — the backbuffer pixel
     underneath is preserved.  This gives the frog its non-rectangular shape.

   CELL_PX EXPANSION:
     Each source "cell" is CELL_PX × CELL_PX pixels (8×8).  So src_w=8 cells
     → 64 pixels wide in the backbuffer.                                  */
void draw_sprite_partial(
    Backbuffer *bb,
    const int16_t *colors,
    const int16_t *glyphs,
    const uint8_t  palette[16][3],
    int sheet_w,
    int src_x, int src_y, int src_w, int src_h,
    int dest_px_x, int dest_px_y)
{
    int sy, sx, py, px;
    int stride = bb->pitch / 4;

    for (sy = 0; sy < src_h; sy++) {
        for (sx = 0; sx < src_w; sx++) {
            int idx    = (src_y + sy) * sheet_w + (src_x + sx);
            int16_t gl = glyphs[idx];
            if (gl == 0x0020) continue;  /* transparent cell — skip */

            int16_t c  = colors[idx];
            int ci     = c & 0x0F;       /* low nibble = FG color index */

            /* Map 4-bit Windows console index → GAME_RGBA via CONSOLE_PALETTE */
            uint32_t color = GAME_RGB(
                palette[ci][0],
                palette[ci][1],
                palette[ci][2]);

            /* Each cell expands to CELL_PX × CELL_PX pixels in the backbuffer */
            for (py = 0; py < CELL_PX; py++) {
                for (px = 0; px < CELL_PX; px++) {
                    int bx = dest_px_x + sx * CELL_PX + px;
                    int by = dest_px_y + sy * CELL_PX + py;
                    if (bx >= 0 && bx < bb->width &&
                        by >= 0 && by < bb->height)
                        bb->pixels[by * stride + bx] = color;
                }
            }
        }
    }
}
