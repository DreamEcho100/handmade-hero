/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-text.h  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * Public API for bitmap font rendering.  Implementations in draw-text.c.
 *
 * THE 8×8 ASCII-INDEXED BITMAP FONT
 * ────────────────────────────────────
 * The font is stored as a 2-D byte array: FONT_8X8[128][8].
 * Index into it directly with the ASCII code of the character:
 *
 *   FONT_8X8['A']  →  8 bytes describing the glyph for 'A'
 *   FONT_8X8['5']  →  8 bytes describing the glyph for '5'
 *   FONT_8X8[' ']  →  all zeros → blank glyph
 *
 * COURSE NOTE: The reference source uses three separate lookup tables
 * (FONT_DIGITS, FONT_LETTERS, FONT_SPECIAL) with a linear scan for special
 * characters.  We upgrade to a single 128-entry table indexed by ASCII value
 * — simpler code, faster lookup, larger glyph set.
 * See Lesson 09 and COURSE-BUILDER-IMPROVEMENTS.md for the full comparison.
 *
 * BIT CONVENTION — BIT7-LEFT
 * ───────────────────────────
 * In each font row byte, bit 7 is the leftmost pixel:
 *
 *   Bit:  7  6  5  4  3  2  1  0
 *   Col:  0  1  2  3  4  5  6  7   (0 = leftmost pixel)
 *
 * To check if column `col` is set in row `row`:
 *   if (bitmap[row] & (0x80 >> col)) { // pixel is lit
 *   }
 *
 * Example — row byte 0x70 = 0111 0000:
 *   Col 0: (0x80 >> 0) = 0x80 = 1000 0000 → 0x70 & 0x80 = 0 → blank
 *   Col 1: (0x80 >> 1) = 0x40 = 0100 0000 → 0x70 & 0x40 = 1 → pixel!
 *   Col 2: (0x80 >> 2) = 0x20 = 0010 0000 → 0x70 & 0x20 = 1 → pixel!
 *   Col 3: (0x80 >> 3) = 0x10 = 0001 0000 → 0x70 & 0x10 = 1 → pixel!
 *   Col 4: (0x80 >> 4) = 0x08 = 0000 1000 → 0x70 & 0x08 = 0 → blank
 *   Pattern: .###....   (matches the top of '0', 'O', etc.)
 *
 * VERIFYING THE FONT — use 'N' as your test glyph
 *   'N' is asymmetric (not the same left-to-right as right-to-left), so a
 *   mirrored rendering is immediately obvious.  If 'N' looks correct, the
 *   bit convention is correct.
 */

#ifndef UTILS_DRAW_TEXT_H
#define UTILS_DRAW_TEXT_H

#include <stdint.h>      /* uint32_t, uint8_t */
#include "backbuffer.h"  /* SnakeBackbuffer   */

/* draw_char — render a single character at pixel position (x, y).
 *
 * Each glyph occupies 8×8 pixels, scaled by `scale` (scale=1 → 8×8 px,
 * scale=2 → 16×16 px, etc.).  Unknown characters (not in FONT_8X8) are
 * silently skipped.
 *
 * @param bb     Destination backbuffer
 * @param x      Left edge of the glyph in pixels
 * @param y      Top edge of the glyph in pixels
 * @param c      The character to draw (ASCII 0–127)
 * @param color  Packed ARGB colour (GAME_RGB or COLOR_* constant)
 * @param scale  Pixel scale factor (1 = 8px, 2 = 16px, …)
 */
void draw_char(SnakeBackbuffer *bb, int x, int y, char c, uint32_t color, int scale);

/* draw_text — render a null-terminated ASCII string.
 *
 * Calls draw_char() for each character, advancing the cursor by
 * `(8 + 1) * scale` pixels per character (8 px glyph + 1 px gap).
 *
 * @param bb     Destination backbuffer
 * @param x      Left edge of the first character
 * @param y      Top edge of all characters (same baseline)
 * @param text   Null-terminated ASCII string
 * @param color  Packed ARGB colour
 * @param scale  Pixel scale factor
 *
 * JS equivalent: ctx.fillText(text, x, y) — but operating on raw pixels.
 */
void draw_text(SnakeBackbuffer *bb, int x, int y, const char *text, uint32_t color, int scale);

/* GLYPH_WIDTH  — pixels per character column at scale 1 */
#define GLYPH_WIDTH   8
/* GLYPH_HEIGHT — pixels per character row at scale 1 */
#define GLYPH_HEIGHT  8
/* GLYPH_STRIDE — horizontal advance per character (glyph + 1px gap) at scale 1 */
#define GLYPH_STRIDE  9

#endif /* UTILS_DRAW_TEXT_H */
