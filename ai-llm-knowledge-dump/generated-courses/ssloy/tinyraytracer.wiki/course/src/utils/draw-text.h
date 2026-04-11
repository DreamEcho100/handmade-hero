#ifndef UTILS_DRAW_TEXT_H
#define UTILS_DRAW_TEXT_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/draw-text.h — TinyRaytracer Course
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
 * GLYPH_W / GLYPH_H are the base glyph dimensions in pixels.
 * Pass scale > 1 to draw_char/draw_text to enlarge each glyph pixel.
 *
 * Like draw_rect, these functions take pixel coordinates as floats.
 * Coordinate conversion (game units → pixels) happens at the call site.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include "./backbuffer.h"

#define GLYPH_W 8
#define GLYPH_H 8

/* LESSON 06 — Full 8×8 ASCII bitmap font table, indexed [c][row].
 * Exported so game courses can extend it (e.g., custom tile glyphs).      */
extern const uint8_t FONT_8X8[128][8];

/* draw_char — render a single ASCII character.
 * x, y: position in pixel space (float for sub-pixel precision)
 * scale: pixel multiplier (1 → 8×8, 2 → 16×16, etc.)
 * r, g, b, a: color channels [0.0–1.0], alpha blending handled automatically.
 * Non-printable or out-of-range c is rendered as '?'. */
void draw_char(Backbuffer *backbuffer, float x, float y, int scale, char c,
               float r, float g, float b, float a);

/* draw_text — render a null-terminated string.
 * x, y: starting position in pixel space (float)
 * scale: pixel multiplier for glyph size
 * r, g, b, a: color channels [0.0–1.0]
 * Characters advance by GLYPH_W * scale pixels horizontally.
 * Newline ('\n') is supported: y advances by GLYPH_H * scale, x resets. */
void draw_text(Backbuffer *backbuffer, float x, float y, int scale,
               const char *text, float r, float g, float b, float a);

#endif /* UTILS_DRAW_TEXT_H */
