/* =============================================================================
 * utils/draw-text.h — Bitmap Font Text Rendering
 * =============================================================================
 *
 * Renders text using an 8×8 pixel bitmap font stored in the source code.
 * No font files, no freetype, no external assets — the entire character set
 * is encoded as a 128-entry table of 8-byte glyph bitmaps.
 *
 * FONT ENCODING — LSB-FIRST
 * ─────────────────────────
 * FONT_8X8 is a 128-entry table (ASCII 0–127).
 * Each entry is 8 bytes; byte[row] is a bitmask where bit 0 (0x01) is the
 * leftmost pixel of that row, bit 7 (0x80) is the rightmost.
 * This is LSB-first encoding: the least significant bit is the leftmost pixel.
 *
 * To test pixel (col, row) of character c:
 *   if (FONT_8X8[c][row] & (1 << col)) { ... }
 *
 * IMPORTANT: Do NOT use (0x80 >> col).  That assumes BIT7-left encoding and
 * will mirror every glyph horizontally.
 *
 * GLYPH CONSTANTS
 * ───────────────
 * GLYPH_W (8) and GLYPH_H (8) are the base glyph pixel dimensions.
 * Pass scale > 1 to enlarge each glyph pixel:
 *   scale=1 → 8×8 pixels per glyph
 *   scale=2 → 16×16 pixels per glyph
 *
 * To compute text width in pixels:
 *   int text_w = strlen(text) * GLYPH_W * scale;
 * =============================================================================
 */

#ifndef UTILS_DRAW_TEXT_H
#define UTILS_DRAW_TEXT_H

#include <stdint.h>
#include "./backbuffer.h"

/* Glyph dimensions in pixels (base, before scale) */
#define GLYPH_W 8
#define GLYPH_H 8

/* FONT_8X8[128][8] — full 8×8 ASCII bitmap font, indexed [c][row].
   Exported so callers can compute text widths with GLYPH_W / GLYPH_H.    */
extern const uint8_t FONT_8X8[128][8];

/* draw_char — render a single ASCII character at pixel (x, y).
 * x, y:  position in pixel space (float for sub-pixel precision)
 * scale: pixel multiplier (1 → 8×8, 2 → 16×16, etc.)
 * r,g,b,a: color channels [0.0–1.0]; alpha blending via draw_rect.
 * Non-printable or out-of-range c is rendered as '?'.                    */
void draw_char(Backbuffer *bb, float x, float y, int scale, char c,
               float r, float g, float b, float a);

/* draw_text — render a null-terminated string starting at pixel (x, y).
 * scale: pixel multiplier for glyph size
 * r,g,b,a: color channels [0.0–1.0]
 * Characters advance by GLYPH_W * scale pixels horizontally.
 * Newline ('\n') advances y by GLYPH_H * scale and resets x.            */
void draw_text(Backbuffer *bb, float x, float y, int scale,
               const char *text, float r, float g, float b, float a);

#endif /* UTILS_DRAW_TEXT_H */
