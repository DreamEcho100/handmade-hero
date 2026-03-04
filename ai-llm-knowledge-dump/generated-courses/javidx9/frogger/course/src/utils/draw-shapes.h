/* =============================================================================
 * utils/draw-shapes.h — Primitive Drawing (Rects + Sprite Partial)
 * =============================================================================
 *
 * Low-level pixel-writing routines used by game_render().
 * All functions write directly into a Backbuffer — no GPU API calls.
 *
 * Functions:
 *   draw_rect           — solid clipped rectangle
 *   draw_rect_blend     — alpha-blended clipped rectangle (for overlays)
 *   draw_sprite_partial — UV-cropped sprite tile into backbuffer
 *
 * CLIPPING vs TOROIDAL WRAP:
 *   Frogger uses clipped drawing throughout — tiles at the lane edge are
 *   simply not drawn if they fall outside the buffer bounds.  No toroidal
 *   wrap is needed (objects don't cross screen edges seamlessly in Frogger).
 * =============================================================================
 */

#ifndef FROGGER_DRAW_SHAPES_H
#define FROGGER_DRAW_SHAPES_H

#include <stdint.h>
#include "backbuffer.h"

/* ── Solid filled rectangle (clipped) ────────────────────────────────────
   Draws a solid-colour rectangle, clamped to the backbuffer bounds.
   (x, y) = top-left corner; w×h = width×height in pixels.              */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* ── Alpha-blended rectangle (clipped) ───────────────────────────────────
   Like draw_rect but uses the colour's alpha byte (bits 24–31) to blend
   with the existing pixel:
     alpha=255 → fully opaque (same result as draw_rect)
     alpha=128 → 50% blend
     alpha=0   → no-op (destination unchanged)
   Used for the translucent DEAD / WIN overlays.                         */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* ── UV-cropped sprite partial ────────────────────────────────────────────
   Renders a rectangular sub-region of a sprite sheet into the backbuffer.
   The source region is in CELL units (each cell = CELL_PX pixels).
   The destination is in PIXEL units (screen pixels).
   Transparent cells (glyph == 0x0020) are skipped — the backbuffer pixel
   underneath is preserved.

   Parameters:
     colors[]    — color-index array for the sprite sheet (int16_t per cell)
     glyphs[]    — glyph array (0x2588 = solid block, 0x0020 = transparent)
     palette     — CONSOLE_PALETTE[16][3] array pointer
     sheet_w     — width of the full sprite sheet in cells
     src_x/y     — top-left of source region in cells
     src_w/h     — size of source region in cells
     dest_px_x/y — destination in pixels (top-left, screen coords)       */
void draw_sprite_partial(
    Backbuffer *bb,
    const int16_t *colors,
    const int16_t *glyphs,
    const uint8_t  palette[16][3],
    int sheet_w,
    int src_x, int src_y, int src_w, int src_h,
    int dest_px_x, int dest_px_y);

#endif /* FROGGER_DRAW_SHAPES_H */
