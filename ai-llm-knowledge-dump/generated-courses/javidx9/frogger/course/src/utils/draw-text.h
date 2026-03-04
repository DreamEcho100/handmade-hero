/* =============================================================================
 * utils/draw-text.h — Bitmap Font Text Rendering
 * =============================================================================
 *
 * Renders text using an 8×8 pixel bitmap font stored inline in the source.
 * No font files, no freetype — the full 128-character ASCII set is encoded
 * as a table of 8-byte glyph bitmaps.
 *
 * GLYPH CONSTANTS
 * ───────────────
 * GLYPH_W and GLYPH_H are the pixel dimensions of each character.
 * Successive characters are placed GLYPH_W pixels apart.
 *
 *   JS equivalent:
 *     const textWidth  = text.length * GLYPH_W;
 *     const centeredX  = (SCREEN_W - textWidth) / 2;
 *
 * > Course Note: The reference frogger.c uses a 5×7 column-major font
 * > (font_glyphs[96][5]) indexed from ASCII 0x20.  The course upgrades
 * > to FONT_8X8[128][8] with BIT7-left convention, matching the pattern
 * > used in the Tetris/Snake/Asteroids courses.  Trade-off: ~1 KB vs ~480 B,
 * > but simpler indexing (direct ASCII, no `ch - 32` offset) and a larger
 * > glyph set (128 vs 96 characters).
 * =============================================================================
 */

#ifndef FROGGER_DRAW_TEXT_H
#define FROGGER_DRAW_TEXT_H

#include <stdint.h>
#include "backbuffer.h"

/* Glyph dimensions in pixels */
#define GLYPH_W 8
#define GLYPH_H 8

/* draw_char — render a single ASCII character at pixel (x, y).          */
void draw_char(Backbuffer *bb, int x, int y, char c, uint32_t color);

/* draw_text — render a null-terminated string starting at (x, y).
   Characters are laid out left-to-right; advance by GLYPH_W per char.  */
void draw_text(Backbuffer *bb, int x, int y, const char *text, uint32_t color);

#endif /* FROGGER_DRAW_TEXT_H */
