#include "draw-text.h"
#include "draw-shapes.h"

/* JS: import { ... } from 'somewhere' | C: #include <string.h>
 * string.h gives us strlen, memcpy, strcmp, etc. We need it for the
 * null-terminated string iteration in draw_text. */
#include <string.h>

/* =============================================================================
 * draw-text.c — Bitmap Font Data and Text Rendering
 * =============================================================================
 *
 * This file contains:
 *   1. The font data tables (FONT_DIGITS, FONT_LETTERS, FONT_SPECIAL).
 *   2. find_special_char — linear search through the special-char table.
 *   3. draw_char   — renders one character by iterating its 5×7 bitmap.
 *   4. draw_text   — renders a full string by calling draw_char in a loop.
 *
 * FONT ENCODING RECAP (see draw-text.h for the full explanation):
 *   Each character = 7 bytes (one per row).
 *   Each byte = 5 bits (one per column), bit 4 = left, bit 0 = right.
 *   A set bit (1) = draw a pixel; a clear bit (0) = skip.
 * =============================================================================
 */

/* ══════ Digit Bitmaps (0–9) ══════
 *
 * Index into this table with (char_digit - '0').
 * Example: '7' - '0' = 7, so FONT_DIGITS[7] is the bitmap for '7'.
 *
 * JS equivalent:
 *   const FONT_DIGITS: readonly number[][] = [
 *     [0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E],  // 0
 *     ...
 *   ];
 *
 * C uses a 2-D array: `unsigned char FONT_DIGITS[10][7]`.
 * [10] = 10 digits (0–9), [7] = 7 rows per digit.
 * `const` means the compiler can put this data in read-only memory (like a
 * string literal) — we never modify font data at runtime.
 */
const unsigned char FONT_DIGITS[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x0E, 0x10, 0x10, 0x1F}, /* 2 */
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
};

/* ══════ Letter Bitmaps (A–Z) ══════
 *
 * Index into this table with (char_letter - 'A').
 * Lowercase letters are mapped to uppercase in draw_char:
 *   'a' - 'a' == 'A' - 'A' == 0  → FONT_LETTERS[0] is 'A'
 *
 * JS equivalent:
 *   const FONT_LETTERS: readonly number[][] = [
 *     [0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],  // A
 *     ...
 *   ];
 */
const unsigned char FONT_LETTERS[26][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* C */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, /* D */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* E */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* F */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, /* G */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* H */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* I */
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x11, 0x0E}, /* J */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* K */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* L */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* M */
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* N */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* P */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* Q */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* R */
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, /* S */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* V */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, /* W */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* X */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* Y */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* Z */
};

/* ══════ Special Character Bitmaps ══════
 *
 * Punctuation and symbol characters that cannot be indexed directly.
 * The table is a linear array of FontSpecialChar structs, terminated by
 * a sentinel entry { 0, {0} } (character == 0 signals end of table).
 *
 * find_special_char() scans this table linearly — O(n). With ~26 entries
 * this is negligible. If you had hundreds of special chars a hash map
 * would be better, but for a simple game this is fine.
 */
const FontSpecialChar FONT_SPECIAL[] = {
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},  /* period */
    {',', {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08}},  /* comma */
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},  /* colon */
    {';', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08}},  /* semicolon */
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},  /* exclamation */
    {'?', {0x0E, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04}},  /* question */
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},  /* dash/minus */
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},  /* plus */
    {'=', {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}},  /* equals */
    {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},  /* slash */
    {'\\', {0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01}}, /* backslash */
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},  /* left paren */
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},  /* right paren */
    {'[', {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}},  /* left bracket */
    {']', {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}},  /* right bracket */
    {'<', {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}},  /* less than */
    {'>', {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}},  /* greater than */
    {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}},  /* underscore */
    {'*', {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00}},  /* asterisk */
    {'#', {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A}},  /* hash */
    {'@', {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E}},  /* at sign */
    {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},  /* percent */
    {'&', {0x08, 0x14, 0x14, 0x08, 0x15, 0x12, 0x0D}},  /* ampersand */
    {'\'', {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00}}, /* single quote */
    {'"', {0x0A, 0x0A, 0x14, 0x00, 0x00, 0x00, 0x00}},  /* double quote */
    /* Arrow symbols — mapped to printable ASCII characters for convenience */
    {'^',  {0x00, 0x04, 0x0E, 0x15, 0x04, 0x04, 0x00}}, /* up arrow ↑ */
    {'v',  {0x00, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00}}, /* down arrow ↓ (lowercase v) */
    {'{',  {0x00, 0x04, 0x08, 0x1E, 0x08, 0x04, 0x00}}, /* left arrow ← (use {) */
    {'}',  {0x00, 0x04, 0x02, 0x0F, 0x02, 0x04, 0x00}}, /* right arrow → (use }) */
    {0, {0}}                                             /* Sentinel — marks end of table */
};

/* ══════ find_special_char ══════ */

/* Search FONT_SPECIAL for character `c`. Returns a pointer to its 7-byte
 * bitmap array, or NULL if not found.
 *
 * JS: const ptr: number[] | null
 * C:  const unsigned char *   — pointer to the first byte of the bitmap array,
 *                               or NULL (a special pointer value meaning "no data").
 *
 * Returning a POINTER (not a copy) is efficient: we don't copy the 7 bytes,
 * we just hand back the address of the existing data inside FONT_SPECIAL.
 */
const unsigned char *find_special_char(char c) {
  /* Loop until we hit the sentinel entry (character == 0). */
  for (int i = 0; FONT_SPECIAL[i].character != 0; i++) {
    if (FONT_SPECIAL[i].character == c) {
      /* Return a pointer to the bitmap field of this struct entry.
       * JS equivalent: return FONT_SPECIAL[i].bitmap;
       * C: return FONT_SPECIAL[i].bitmap; — bitmap decays to a pointer to
       *    its first element, equivalent to &FONT_SPECIAL[i].bitmap[0]. */
      return FONT_SPECIAL[i].bitmap;
    }
  }
  return NULL; /* Not found — caller must check for NULL before using result. */
}

/* ══════ draw_char ══════ */

void draw_char(Backbuffer *bb, int x, int y, char c, uint32_t color,
               int scale) {

  /* ── Step 1: Look Up the Bitmap ─────────────────────────────────────────
   *
   * `bitmap` will point to a 7-element array of unsigned char values.
   * We initialise it to NULL so the compiler (and sanitizers) can detect if
   * we accidentally try to use it without setting it.
   *
   * JS equivalent: let bitmap: number[] | null = null;
   * C:  const unsigned char *bitmap = NULL;
   *     — a pointer variable that currently points to nothing.
   */
  const unsigned char *bitmap = NULL;

  if (c >= '0' && c <= '9') {
    /* Digit: index into FONT_DIGITS.
     * `c - '0'` converts the ASCII character to its numeric value.
     *   '0' in ASCII is 48, '9' is 57.
     *   '7' - '0' = 55 - 48 = 7  → FONT_DIGITS[7]
     *
     * JS equivalent: const bitmap = FONT_DIGITS[Number(c)];
     */
    bitmap = FONT_DIGITS[c - '0'];

  } else if (c >= 'A' && c <= 'Z') {
    /* Uppercase letter: index into FONT_LETTERS.
     * 'A' - 'A' = 0 → FONT_LETTERS[0], 'Z' - 'A' = 25 → FONT_LETTERS[25].
     *
     * JS equivalent: const bitmap = FONT_LETTERS[c.charCodeAt(0) - 65];
     */
    bitmap = FONT_LETTERS[c - 'A'];

  } else if (c >= 'a' && c <= 'z') {
    /* Lowercase letter: map to uppercase by using the same index.
     * 'a' - 'a' == 'A' - 'A' == 0, so lowercase and uppercase share bitmaps.
     * Our font has no distinct lowercase glyphs — this is common in tiny
     * bitmap fonts where 5×7 pixels can't distinguish e.g. 'A' from 'a'.
     */
    bitmap = FONT_LETTERS[c - 'a'];

  } else if (c == ' ') {
    /* Space character: draw nothing — the cursor advance in draw_text handles
     * the horizontal gap. We just return early. */
    return;

  } else {
    /* Try the special-character table for punctuation/symbols. */
    bitmap = find_special_char(c);
    if (!bitmap) {
      /* NULL pointer = character not found. Skip silently.
       * JS equivalent: if (!bitmap) return;
       * C: if (!bitmap) return;  — !NULL is true (NULL == 0 in C). */
      return;
    }
  }

  /* ── Step 2: Render the 5×7 Bitmap ─────────────────────────────────────
   *
   * Iterate over 7 rows and 5 columns. For each cell, test the corresponding
   * bit in the bitmap byte. If the bit is set, draw a (scale × scale) pixel
   * block at the appropriate screen position.
   */
  for (int row = 0; row < 7; row++) {
    for (int col = 0; col < 5; col++) {

      /* Test whether the pixel at (col, row) is "on".
       *
       * Bit manipulation: bitmap[row] & (0x10 >> col)
       *
       * WHY 0x10?
       *   0x10 = 0001 0000 in binary = bit 4 set.
       *   Bit 4 is the LEFTMOST pixel (col 0).
       *   Shifting right by `col` moves the test bit to the right:
       *     col=0: 0x10 >> 0 = 0001 0000  → tests bit 4 (leftmost)
       *     col=1: 0x10 >> 1 = 0000 1000  → tests bit 3
       *     col=2: 0x10 >> 2 = 0000 0100  → tests bit 2 (centre)
       *     col=3: 0x10 >> 3 = 0000 0010  → tests bit 1
       *     col=4: 0x10 >> 4 = 0000 0001  → tests bit 0 (rightmost)
       *
       * The `&` (bitwise AND) returns non-zero if that bit is set in
       * bitmap[row], zero otherwise.
       *
       * JS equivalent:
       *   const pixelOn = (bitmap[row] & (0x10 >> col)) !== 0;
       *
       * C:
       *   if (bitmap[row] & (0x10 >> col)) { ... }
       *   — In C, any non-zero integer is "truthy", so no != 0 needed.
       */
      if (bitmap[row] & (0x10 >> col)) {
        /* Draw a (scale × scale) filled square for this font pixel.
         * x + col * scale  → screen x position of this column block
         * y + row * scale  → screen y position of this row block
         * scale, scale     → width and height of the block
         *
         * At scale=1: one pixel per font dot. At scale=3: 3×3 pixels per dot.
         *
         * JS equivalent:
         *   ctx.fillRect(x + col * scale, y + row * scale, scale, scale);
         */
        draw_rect(bb, x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}

/* ══════ draw_text ══════ */

void draw_text(Backbuffer *bb, int x, int y, const char *text, uint32_t color,
               int scale) {

  /* cursor_x tracks the horizontal position for the next character.
   * It starts at x and advances right after each character is drawn.
   *
   * JS equivalent: let cursorX = x;
   */
  int cursor_x = x;

  /* Iterate over the string character by character.
   *
   * JS: for (const c of text) { ... }
   * C:  while (*text) { ... }
   *
   * `text` is a `const char *` — a pointer to the first character of the
   * string. In C, strings are null-terminated: they end with the byte '\0'
   * (value 0). The `while (*text)` condition:
   *   - Dereferences the pointer: `*text` reads the byte at the current address.
   *   - Tests it: the loop continues while the byte is non-zero (not '\0').
   * When we reach the terminator '\0', `*text` == 0 which is falsy → loop ends.
   */
  while (*text) {
    draw_char(bb, cursor_x, y, *text, color, scale);

    /* Advance the cursor by (6 * scale) pixels:
     *   - 5 pixels for the character glyph width (font columns)
     *   - 1 pixel for the inter-character gap
     *   - multiplied by scale for scaled rendering
     *
     * JS equivalent: cursorX += 6 * scale;
     *
     * Example at scale=2:
     *   Each glyph is 10 pixels wide (5 cols × 2), gap is 2 pixels → 12 total.
     */
    cursor_x += 6 * scale;

    /* Advance the pointer to the next character.
     * `text++` increments the pointer by one char (one byte).
     * After this, `*text` reads the next character on the next iteration.
     *
     * JS equivalent: (handled by the for..of loop automatically)
     * C equivalent: text++;  — move the pointer forward one element.
     */
    text++;
  }
}
