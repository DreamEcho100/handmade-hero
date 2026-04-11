#ifndef UTILS_DRAW_TEXT_H
#define UTILS_DRAW_TEXT_H

#include "./backbuffer.h"
#include <stdint.h>

#define GLYPH_W 8
#define GLYPH_H 8

/* Font encoding: LSB-first — bit 0 = leftmost pixel (col 0).
 * To test pixel (col, row): FONT_8X8[c][row] & (1 << col)
 * Do NOT use (0x80 >> col) — that is BIT7-left and mirrors every glyph. */
extern const uint8_t FONT_8X8[128][8];

/* draw_char — render a single ASCII character.
 * x, y: position in pixel space (float for sub-pixel precision)
 * scale: pixel multiplier (1 = 8x8, 2 = 16x16, etc.)
 * r, g, b, a: color channels [0.0-1.0] */
void draw_char(Backbuffer *backbuffer, float x, float y, float scale, char c,
               float r, float g, float b, float a);

/* draw_text — render a null-terminated string.
 * Characters advance by GLYPH_W * scale pixels horizontally.
 * Newline ('\n') resets x and advances y by GLYPH_H * scale. */
void draw_text(Backbuffer *backbuffer, float x, float y, float scale,
               const char *text, float r, float g, float b, float a);

#endif /* UTILS_DRAW_TEXT_H */
