#ifndef UTILS_DRAW_TEXT_H
#define UTILS_DRAW_TEXT_H

/* =============================================================================
 * utils/draw-text.h — Bitmap Font Declarations
 * =============================================================================
 *
 * This file declares the data and functions needed to draw text into a
 * Backbuffer using a hand-crafted 5×7 bitmap font.
 *
 * WHAT IS A BITMAP FONT?
 *   A bitmap font stores each character as a small grid of on/off pixels,
 *   rather than as a scalable outline (like a TrueType font). It's simple,
 *   fast, and requires no external font libraries — perfect for game UIs.
 *
 *   Our font uses 5 columns × 7 rows per character. Each row is encoded as
 *   5 bits packed into one byte. We store 7 bytes per character (one per row).
 *
 * =============================================================================
 *
 * ══════ Bitmap Font Encoding Explained ══════
 *
 * Each character's shape is stored as an array of 7 bytes: one byte per row.
 * Only the lowest 5 bits of each byte are used (bits 4-0). Bit 4 is the
 * LEFTMOST pixel; bit 0 is the RIGHTMOST pixel.
 *
 *   Bit positions within a byte:
 *   ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
 *   │  7   │  6   │  5   │  4   │  3   │  2   │  1   │  0   │
 *   │  (unused)   │ (unused) │ col0 │ col1 │ col2 │ col3 │ col4 │
 *   └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
 *                            ↑left                      ↑right
 *
 * Reading a row byte as pixels:
 *   Byte value  Binary   Screen pixels (. = off, # = on)
 *   ─────────   ──────   ─────────────────────────────────
 *   0x0E      = 01110  =  . # # # .     (top of letter A)
 *   0x11      = 10001  =  # . . . #
 *   0x1F      = 11111  =  # # # # #     (crossbar of A)
 *   0x00      = 00000  =  . . . . .     (blank row)
 *
 * Full example — letter 'A' = { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }:
 *
 *   0x0E = 01110 =  . # # # .
 *   0x11 = 10001 =  # . . . #
 *   0x11 = 10001 =  # . . . #
 *   0x1F = 11111 =  # # # # #
 *   0x11 = 10001 =  # . . . #
 *   0x11 = 10001 =  # . . . #
 *   0x11 = 10001 =  # . . . #
 *
 * Another example — digit '1' = { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E }:
 *
 *   0x04 = 00100 =  . . # . .
 *   0x0C = 01100 =  . # # . .
 *   0x04 = 00100 =  . . # . .
 *   0x04 = 00100 =  . . # . .
 *   0x04 = 00100 =  . . # . .
 *   0x04 = 00100 =  . . # . .
 *   0x0E = 01110 =  . # # # .   ← base/underline of the 1
 *
 * WHY HEX?
 *   0x0E is easier to write and read than 0b01110 (binary literals are
 *   non-standard in older C). You'll quickly learn to visualise hex → binary
 *   for the 5-bit range: 0x0=0000, 0x8=1000, 0xF=1111, etc.
 * =============================================================================
 */

#include "backbuffer.h"
#include <stdint.h>

/* ══════ Font Data Arrays ══════
 *
 * `extern` means "this variable is defined in another .c file; just trust
 * that it exists". It's like a TypeScript `declare const` — you're telling
 * the compiler "this exists, I'll provide it at link time (in draw-text.c)".
 *
 * JS equivalent:
 *   export declare const FONT_DIGITS: readonly number[][];  // 10 chars × 7 rows
 *   export declare const FONT_LETTERS: readonly number[][]; // 26 chars × 7 rows
 *
 * C:
 *   extern const unsigned char FONT_DIGITS[10][7];
 *   //                                     ^^  ^
 *   //                              10 digits  7 rows each
 *
 * `unsigned char` is an 8-bit value (0–255). We use it here instead of
 * uint8_t because font data is traditionally stored this way and it avoids
 * an extra #include in the font tables.
 */
extern const unsigned char FONT_DIGITS[10][7];
extern const unsigned char FONT_LETTERS[26][7];

/* ══════ Special Characters ══════
 *
 * Digits and letters use simple array indexing:
 *   FONT_DIGITS['5' - '0']  →  FONT_DIGITS[5]
 *   FONT_LETTERS['A' - 'A'] →  FONT_LETTERS[0]
 *
 * Special characters (punctuation, symbols) can't be indexed that simply, so
 * we use a lookup table: an array of (character, bitmap) pairs, terminated by
 * a {0, {0}} sentinel entry.
 *
 * JS equivalent:
 *   const FONT_SPECIAL: Array<{ character: string; bitmap: number[] }> = [
 *     { character: '.', bitmap: [0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C] },
 *     ...
 *     { character: '\0', bitmap: [0,0,0,0,0,0,0] } // sentinel
 *   ];
 */
typedef struct {
  char character;           /* The ASCII character this entry represents */
  unsigned char bitmap[7];  /* 7-row bitmap, same encoding as FONT_DIGITS */
} FontSpecialChar;

extern const FontSpecialChar FONT_SPECIAL[];

/* Helper: search FONT_SPECIAL for character `c` and return a pointer to its
 * 7-byte bitmap array, or NULL if not found.
 *
 * JS equivalent:
 *   const findSpecialChar = (c: string) =>
 *     FONT_SPECIAL.find(e => e.character === c)?.bitmap ?? null;
 */
const unsigned char *find_special_char(char c);

/* ══════ Drawing Functions ══════ */

/* draw_char — Draw a single character at pixel position (x, y).
 *
 * Parameters:
 *   bb     — Backbuffer to draw into.
 *   x, y   — Top-left corner of the character cell.
 *   c      — ASCII character to draw.
 *   color  — Packed ARGB color (use GAME_RGB / GAME_RGBA).
 *   scale  — Pixel scale factor.
 *             scale=1 → each "font pixel" = 1 screen pixel (tiny, 5×7 px)
 *             scale=2 → each "font pixel" = 2×2 screen pixels (10×14 px)
 *             scale=3 → each "font pixel" = 3×3 screen pixels (15×21 px)
 *
 * Supported characters: A-Z, a-z (mapped to uppercase), 0-9, space, and all
 * entries in FONT_SPECIAL. Unknown characters are silently skipped.
 */
void draw_char(Backbuffer *bb, int x, int y, char c, uint32_t color, int scale);

/* draw_text — Draw a null-terminated string of characters.
 *
 * Calls draw_char for each character in `text`, advancing the horizontal
 * cursor by (6 * scale) pixels per character:
 *   5 pixels of character width + 1 pixel of inter-character gap, scaled.
 *
 * JS equivalent:
 *   ctx.font = `${scale * 7}px monospace`;
 *   ctx.fillText(text, x, y);
 *
 * Parameters are the same as draw_char; `text` is a C string (char array
 * terminated with the null byte '\0').
 */
void draw_text(Backbuffer *bb, int x, int y, const char *text, uint32_t color,
               int scale);

#endif /* UTILS_DRAW_TEXT_H */
