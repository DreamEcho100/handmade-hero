/* =============================================================================
 * utils/draw-text.h — Bitmap Font Text Rendering
 * =============================================================================
 *
 * Renders text using an 8×8 pixel bitmap font stored in the source code.
 * No font files, no freetype, no external assets — the entire character set
 * is encoded as a 128-entry table of 8-byte glyph bitmaps.
 *
 * GLYPH CONSTANTS
 * ───────────────
 * GLYPH_W (8) and GLYPH_H (8) are the pixel dimensions of each character.
 * When you call draw_text("SCORE", x, y, …), successive characters are
 * placed GLYPH_W pixels apart.  Use these constants to compute layout:
 *
 *   JS equivalent:
 *     const textWidth = text.length * GLYPH_W;
 *     const centeredX = (SCREEN_W - textWidth) / 2;
 * =============================================================================
 */

#ifndef ASTEROIDS_DRAW_TEXT_H
#define ASTEROIDS_DRAW_TEXT_H

#include <stdint.h>
#include "backbuffer.h"

/* Glyph dimensions in pixels */
#define GLYPH_W 8
#define GLYPH_H 8

/* draw_char — render a single ASCII character at pixel (x, y).
   Characters outside [0x20, 0x7E] (non-printable or out-of-range) are
   rendered as blank space.                                               */
void draw_char(AsteroidsBackbuffer *bb, int x, int y, char c, uint32_t color);

/* draw_text — render a null-terminated string starting at (x, y).
   Characters are laid out left-to-right with no wrapping; advance by
   GLYPH_W pixels per character.                                          */
void draw_text(AsteroidsBackbuffer *bb, int x, int y,
               const char *text, uint32_t color);

#endif /* ASTEROIDS_DRAW_TEXT_H */
