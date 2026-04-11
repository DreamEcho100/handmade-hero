#ifndef UTILS_DRAW_TEXT_H
#define UTILS_DRAW_TEXT_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-text.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 06 — FONT_8X8[128][8] bitmap font, LSB-first mask (1 << col),
 *             draw_char, draw_text, snprintf for integer→string.
 *
 * LESSON 08 — snprintf used for FPS counter string.
 *
 * Font encoding
 * ─────────────
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
 * GLYPH_W / GLYPH_H are the base glyph dimensions.
 * Pass scale > 1 to draw_char/draw_text to enlarge each glyph pixel.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include "./backbuffer.h"

#define GLYPH_W 8
#define GLYPH_H 8

/* LESSON 06 — Full 8×8 ASCII bitmap font table, indexed [c][row].
 * Exported so game courses can extend it (e.g., custom tile glyphs).      */
extern const uint8_t FONT_8X8[128][8];

/* draw_char — render a single ASCII character at pixel (x, y).
 * scale=1 → 8×8 pixels; scale=2 → 16×16 pixels, etc.
 * Non-printable or out-of-range c is rendered as a space. */
void draw_char(Backbuffer *bb, int x, int y, int scale, char c, uint32_t color);

/* draw_text — render a null-terminated string starting at (x, y).
 * Characters advance by GLYPH_W * scale horizontally.
 * Newline ('\n') is supported: y advances by GLYPH_H * scale, x resets. */
void draw_text(Backbuffer *bb, int x, int y, int scale, const char *text, uint32_t color);

#endif /* UTILS_DRAW_TEXT_H */
