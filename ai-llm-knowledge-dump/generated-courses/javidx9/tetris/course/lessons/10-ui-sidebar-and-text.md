# Lesson 10: UI — Sidebar & Text

## What we're building

A full HUD sidebar displaying score, level, piece count, and a next-piece preview. We introduce a hand-crafted **5×7 bitmap font** stored as arrays of bytes. Each character is 7 rows tall; each row is 5 bits packed into one byte. `draw_text()` iterates those bits and stamps scaled pixel-blocks into the backbuffer.

We also introduce `snprintf` — C's safe, type-checked way to format numbers into strings, equivalent to JavaScript's `String(n)` or template literals.

## What you'll learn

- `snprintf` for safe integer-to-string conversion
- 2D arrays in C: `unsigned char FONT_LETTERS[26][7]`
- Bit masking: `bitmap[row] & (0x10 >> col)` to read individual pixels
- `sizeof(buf)` — compile-time array size
- How a bitmap font encodes glyphs in 5 bits per row
- `extern` keyword: declaring a variable that lives in another `.c` file

## Prerequisites

Lesson 09 complete: `score`, `level`, and `pieces_count` exist in `GameState`. The game tracks them but doesn't display them yet.

---

## Step 1: Understand the Bitmap Font Encoding

Our font stores each character as an array of 7 `unsigned char` values — one byte per row. Only the **lowest 5 bits** of each byte are used; bit 4 is the leftmost pixel and bit 0 is the rightmost.

```
Byte layout (bits 7..0):
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│  7  │  6  │  5  │  4  │  3  │  2  │  1  │  0  │
│     unused     │ col0│ col1│ col2│ col3│ col4│
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
                   ↑left                     ↑right
```

Example — the letter **'A'**: `{ 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }`

```
0x0E = 01110  →  . # # # .    ← top arch
0x11 = 10001  →  # . . . #
0x11 = 10001  →  # . . . #
0x1F = 11111  →  # # # # #    ← crossbar
0x11 = 10001  →  # . . . #
0x11 = 10001  →  # . . . #
0x11 = 10001  →  # . . . #
```

> **Why store glyphs as bit-packed bytes?**
> A 5×7 font is tiny — only 35 bits per character. Packing 5 bits into one byte is the most compact representation: 7 bytes per character vs. 35 bytes if you stored each pixel as one byte. It also makes the bit-test loop elegant (one mask shift per column).
>
> JS equivalent: a canvas `fillText()` call delegates to the OS font rasteriser. Here we implement the same concept manually — pixel by pixel — which means zero dependencies and total visual control.

### Reading a pixel at (col, row)

```c
if (bitmap[row] & (0x10 >> col)) {
    /* this pixel is ON — draw it */
}
```

Why `0x10`? In hex, `0x10 = 0b00010000` — that's bit 4 set, which is the **leftmost** pixel (col 0). Shifting it right by `col` positions moves the test mask to the right column:

```
col=0: 0x10 >> 0 = 00010000  → tests bit 4 (leftmost)
col=1: 0x10 >> 1 = 00001000  → tests bit 3
col=2: 0x10 >> 2 = 00000100  → tests bit 2 (centre)
col=3: 0x10 >> 3 = 00000010  → tests bit 1
col=4: 0x10 >> 4 = 00000001  → tests bit 0 (rightmost)
```

Bitwise AND `&` returns the bit unchanged if the test bit is set, or zero if not. In C, any non-zero value is "truthy" — so `if (bitmap[row] & mask)` is idiomatic.

> **Why?** In JS, you'd write `(bitmap[row] & (0x10 >> col)) !== 0`. In C, the `!== 0` is unnecessary — C treats any non-zero integer as `true` inside an `if`. This is standard practice, not a shortcut.

---

## Step 2: Create `utils/draw-text.h`

This new header declares the font tables and the two drawing functions. Add it to `src/utils/`.

> **Why `extern`?**
> `extern const unsigned char FONT_DIGITS[10][7]` is a **declaration** — it promises the compiler "this array exists somewhere; I'll tell you where at link time." The actual definition (the bytes) lives in `draw-text.c`. Without `extern`, including this header in two `.c` files would create two definitions and a linker error: "duplicate symbol."
>
> JS equivalent: TypeScript `declare const FONT_DIGITS: readonly number[][]` in a `.d.ts` file — you're telling the type checker "this exists" without defining its value.

**`src/utils/draw-text.h`** — complete new file:

```c
#ifndef UTILS_DRAW_TEXT_H
#define UTILS_DRAW_TEXT_H

/*
 * utils/draw-text.h — Bitmap Font Declarations
 *
 * WHAT IS A BITMAP FONT?
 *   A bitmap font stores each character as a small grid of on/off pixels
 *   rather than a scalable outline. Our font uses 5 columns × 7 rows per
 *   character. Each row is encoded as 5 bits packed into one byte.
 *
 * ENCODING:
 *   Each character = 7 bytes (one per row).
 *   Bit 4 (0x10) = leftmost pixel; bit 0 = rightmost.
 *   To test pixel at (col, row): bitmap[row] & (0x10 >> col)
 *
 * EXAMPLE — letter 'A' = { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }
 *   0x0E = 01110 → . # # # .
 *   0x11 = 10001 → # . . . #
 *   0x1F = 11111 → # # # # #   ← crossbar
 */

#include "backbuffer.h"
#include <stdint.h>

/*
 * `extern` = "this array is defined in draw-text.c; link it in at compile time."
 * JS: declare const FONT_DIGITS: readonly number[][];
 *
 * C 2D array syntax: unsigned char FONT_DIGITS[10][7]
 *   [10] = 10 digits (0-9)
 *   [7]  = 7 rows per digit
 * Access: FONT_DIGITS[digit_value][row_index]
 */
extern const unsigned char FONT_DIGITS[10][7];
extern const unsigned char FONT_LETTERS[26][7];

/* Special characters (punctuation, arrows) stored as (char, bitmap) pairs.
 * The table ends with a { 0, {0} } sentinel entry.
 * JS: Array<{ character: string; bitmap: number[] }>
 */
typedef struct {
  char character;
  unsigned char bitmap[7];
} FontSpecialChar;

extern const FontSpecialChar FONT_SPECIAL[];

/* Find bitmap for special character `c`. Returns NULL if not found.
 * JS: FONT_SPECIAL.find(e => e.character === c)?.bitmap ?? null
 */
const unsigned char *find_special_char(char c);

/*
 * draw_char — Draw one character at pixel position (x, y).
 *   scale=1 → 5×7 px; scale=2 → 10×14 px; scale=3 → 15×21 px
 * Supports: A-Z, a-z (mapped to uppercase), 0-9, space, FONT_SPECIAL entries.
 */
void draw_char(Backbuffer *bb, int x, int y, char c, uint32_t color, int scale);

/*
 * draw_text — Draw a null-terminated string.
 * Calls draw_char for each character, advancing cursor by (6 * scale) px:
 *   5 px glyph width + 1 px inter-character gap, scaled.
 * JS: ctx.font = `${scale*7}px monospace`; ctx.fillText(text, x, y);
 */
void draw_text(Backbuffer *bb, int x, int y, const char *text,
               uint32_t color, int scale);

#endif /* UTILS_DRAW_TEXT_H */
```

---

## Step 3: Create `utils/draw-text.c`

**`src/utils/draw-text.c`** — complete new file:

```c
/* utils/draw-text.c — Bitmap Font Data and Text Rendering */

#include "draw-text.h"
#include "draw-shapes.h"
#include <string.h>

/* ══════ Digit Bitmaps (0-9) ══════
 *
 * C 2D array literal: const unsigned char FONT_DIGITS[10][7] = { {...}, ... };
 *   - Outer dimension [10]: 10 entries (digits 0 through 9)
 *   - Inner dimension [7]:  7 bytes per digit (one per row)
 *
 * Index with: FONT_DIGITS['5' - '0']  →  FONT_DIGITS[5]
 *   '5' in ASCII = 53, '0' = 48, so '5' - '0' = 5.  Classic C idiom.
 * JS equivalent: FONT_DIGITS[Number(c)]
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

/* ══════ Letter Bitmaps (A-Z) ══════
 *
 * Index with: FONT_LETTERS['A' - 'A'] → FONT_LETTERS[0]
 * Lowercase is auto-mapped to uppercase in draw_char (no distinct glyphs).
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
 * Linear table of (character, bitmap) pairs. Terminated by { 0, {0} } sentinel.
 * We also define arrow glyphs mapped to printable characters:
 *   '^' = up arrow, 'v' = down arrow, '{' = left arrow, '}' = right arrow
 */
const FontSpecialChar FONT_SPECIAL[] = {
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {',', {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08}},
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
    {'?', {0x0E, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
    {'<', {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}},
    {'>', {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}},
    {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}},
    {'*', {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00}},
    {'#', {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A}},
    {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},
    /* Arrow symbols mapped to printable chars for HUD use */
    {'^', {0x00, 0x04, 0x0E, 0x15, 0x04, 0x04, 0x00}},  /* up arrow    */
    {'v', {0x00, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00}},  /* down arrow  */
    {'{', {0x00, 0x04, 0x08, 0x1E, 0x08, 0x04, 0x00}},  /* left arrow  */
    {'}', {0x00, 0x04, 0x02, 0x0F, 0x02, 0x04, 0x00}},  /* right arrow */
    {0, {0}}  /* sentinel — loop stops when character == 0 */
};

/* ══════ find_special_char ══════ */
const unsigned char *find_special_char(char c) {
  /* Scan until sentinel (character == 0).
   * Returns pointer to the 7-byte bitmap array, or NULL if not found.
   * C: returning a pointer is NOT a copy — it's the address of data inside
   * FONT_SPECIAL. Efficient: 0 bytes copied.
   * JS: return FONT_SPECIAL.find(e => e.character === c)?.bitmap ?? null */
  for (int i = 0; FONT_SPECIAL[i].character != 0; i++) {
    if (FONT_SPECIAL[i].character == c)
      return FONT_SPECIAL[i].bitmap;
  }
  return NULL;
}

/* ══════ draw_char ══════ */
void draw_char(Backbuffer *bb, int x, int y, char c, uint32_t color,
               int scale) {

  /* Step 1: look up the bitmap for this character */
  const unsigned char *bitmap = NULL;

  if (c >= '0' && c <= '9') {
    /* 'c' - '0' converts ASCII digit to array index (0-9).
     * JS: Number(c) or parseInt(c, 10) */
    bitmap = FONT_DIGITS[c - '0'];

  } else if (c >= 'A' && c <= 'Z') {
    /* 'c' - 'A' converts uppercase letter to index (0-25). */
    bitmap = FONT_LETTERS[c - 'A'];

  } else if (c >= 'a' && c <= 'z') {
    /* Lowercase: same index as uppercase. We have no distinct lowercase glyphs. */
    bitmap = FONT_LETTERS[c - 'a'];

  } else if (c == ' ') {
    return; /* space = no glyph, cursor advance is handled in draw_text */

  } else {
    bitmap = find_special_char(c);
    if (!bitmap) return; /* !NULL → unknown character, skip silently */
  }

  /* Step 2: render the 5×7 bitmap as (scale × scale) pixel blocks.
   *
   * For each row (0-6) and column (0-4), test the corresponding bit.
   * If set, fill a scale×scale square at the screen position.
   *
   * Bit test: bitmap[row] & (0x10 >> col)
   *   0x10 = bit 4 = leftmost pixel (col 0)
   *   Shift right by `col` to move the test mask to the right column.
   *
   * JS equivalent:
   *   for (let row = 0; row < 7; row++) {
   *     for (let col = 0; col < 5; col++) {
   *       if (bitmap[row] & (0x10 >> col)) {
   *         ctx.fillRect(x + col*scale, y + row*scale, scale, scale);
   *       }
   *     }
   *   }
   */
  for (int row = 0; row < 7; row++) {
    for (int col = 0; col < 5; col++) {
      if (bitmap[row] & (0x10 >> col)) {
        draw_rect(bb, x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}

/* ══════ draw_text ══════ */
void draw_text(Backbuffer *bb, int x, int y, const char *text,
               uint32_t color, int scale) {
  int cursor_x = x;

  /* Iterate null-terminated C string.
   * `while (*text)` reads one byte at the current pointer.
   * The loop continues while that byte is non-zero (not the '\0' terminator).
   *
   * JS: for (const c of text) { ... }
   * C:  while (*text) { draw_char(..., *text, ...); cursor_x += 6*scale; text++; }
   *
   * Character width: 5 pixels of glyph + 1 pixel gap = 6, multiplied by scale.
   * At scale=2: each character occupies 12 pixels horizontally.
   */
  while (*text) {
    draw_char(bb, cursor_x, y, *text, color, scale);
    cursor_x += 6 * scale;
    text++;  /* advance pointer to next character */
  }
}
```

---

## Step 4: Update `game_render` to Draw the Sidebar

Now we wire up `draw_text()` and `snprintf()` in `game_render`. The render function grows a new section: the HUD sidebar to the right of the field.

### `snprintf` — C's safe number formatter

> **Why `snprintf` instead of `sprintf`?**
> `sprintf(buf, "%d", n)` writes into `buf` with no size limit — if `n` formats to more characters than `buf` can hold, it overflows into adjacent memory (a classic buffer overrun bug). `snprintf(buf, sizeof(buf), "%d", n)` takes the buffer size as its second argument and **always null-terminates**. It never writes more than `sizeof(buf)` bytes.
>
> JS equivalent: `String(n)` or `` `${n}` `` — automatic, no buffer to overflow. In C you must explicitly bound every string write.
>
> **`sizeof(buf)` — compile-time size:**
> `char buf[32]` allocates 32 bytes on the stack. `sizeof(buf)` returns `32` at compile time — it's not a function call, it's a constant the compiler substitutes in. If you later change `buf[32]` to `buf[64]`, the `sizeof(buf)` in `snprintf` updates automatically. Never hardcode the size: `snprintf(buf, 32, ...)` — if you change the array later you'll forget to update the number.

### Next-piece preview at half cell size

The preview draws the next tetromino at `CELL_SIZE / 2` (15 px) per cell:

```c
int preview_cell_size = CELL_SIZE / 2;
for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
        if (TETROMINOES[state->current_piece.next_index]
                       [tetromino_pos_value(px, py, 0)] == TETROMINO_BLOCK) {
            draw_rect(backbuffer,
                      preview_x + px * preview_cell_size + 1,
                      preview_y + py * preview_cell_size + 1,
                      preview_cell_size - 2,
                      preview_cell_size - 2,
                      get_tetromino_color(state->current_piece.next_index));
        }
    }
}
```

We reuse `tetromino_pos_value(px, py, 0)` — rotation 0 is the canonical spawn orientation.

> **Handmade Hero principle:** Separate translation units keep compilation fast and dependencies explicit. `draw-text.c` is its own `.c` file — not `#included` into `game.c`. The linker resolves `draw_text` at link time. This means you can recompile only the changed `.c` file, not the entire codebase. One `.c` file = one unit of change.

> **Common mistake:** Including `utils/draw-text.c` in game.h or game.c via `#include`. Never `#include` a `.c` file. Add `draw-text.c` to your **compile command** (or `SOURCES` in the build script) as a separate translation unit. Including `.c` files causes duplicate symbol errors.

---

## Step 5: Updated `src/game.c` with Sidebar

Only the `game_render` function and the new `#include` change from lesson 09. Every other function is identical.

**`src/game.c`** — complete file at the end of lesson 10 (showing changed/new sections; all other functions from lesson 09 are unchanged):

```c
/* game.c — Lesson 10: UI Sidebar & Text
 *
 * Changes from lesson 09:
 *   + #include "utils/draw-text.h"
 *   + #include <stdio.h>  (for snprintf)
 *   + game_render: full sidebar with score, level, pieces, next-piece preview
 */

#include "game.h"
#include "utils/draw-shapes.h"
#include "utils/draw-text.h"   /* NEW: draw_text(), draw_char() */

#include <stdio.h>    /* NEW: snprintf() */
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Tetromino Shape Data (unchanged) ──────────────────────────────────── */
const char TETROMINO_I[TETROMINO_SIZE] = "..X...X...X...X.";
const char TETROMINO_J[TETROMINO_SIZE] = "..X...X..XX.....";
const char TETROMINO_L[TETROMINO_SIZE] = ".X...X...XX.....";
const char TETROMINO_O[TETROMINO_SIZE] = ".....XX..XX.....";
const char TETROMINO_S[TETROMINO_SIZE] = ".X...XX...X.....";
const char TETROMINO_T[TETROMINO_SIZE] = "..X..XX...X.....";
const char TETROMINO_Z[TETROMINO_SIZE] = "..X..XX..X......";

const char *TETROMINOES[TETROMINOS_COUNT] = {
  TETROMINO_I, TETROMINO_J, TETROMINO_L, TETROMINO_O,
  TETROMINO_S, TETROMINO_T, TETROMINO_Z,
};

/* ── Private Drawing Helpers (unchanged from lesson 09) ─────────────────── */
static uint32_t get_tetromino_color(TETROMINO_BY_IDX index) {
  switch (index) {
    case TETROMINO_I_IDX: return COLOR_CYAN;
    case TETROMINO_J_IDX: return COLOR_BLUE;
    case TETROMINO_L_IDX: return COLOR_ORANGE;
    case TETROMINO_O_IDX: return COLOR_YELLOW;
    case TETROMINO_S_IDX: return COLOR_GREEN;
    case TETROMINO_T_IDX: return COLOR_MAGENTA;
    case TETROMINO_Z_IDX: return COLOR_RED;
    default:              return COLOR_GRAY;
  }
}

static void draw_cell(Backbuffer *bb, int col, int row, uint32_t color) {
  draw_rect(bb, col * CELL_SIZE + 1, row * CELL_SIZE + 1,
            CELL_SIZE - 2, CELL_SIZE - 2, color);
}

static void draw_piece(Backbuffer *bb, int piece_index, int field_col,
                       int field_row, uint32_t color, TETROMINO_R_DIR rotation) {
  for (int py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (int px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK)
        draw_cell(bb, field_col + px, field_row + py, color);
    }
  }
}

/* ════════════════════════════════════════════════════════════════════════════
 * game_render — NEW: full HUD sidebar  (see src/game.c:159-282)
 * ════════════════════════════════════════════════════════════════════════════
 */
void game_render(Backbuffer *backbuffer, GameState *state) {

  /* 1. Clear */
  for (int i = 0; i < backbuffer->width * backbuffer->height; i++)
    backbuffer->pixels[i] = COLOR_BLACK;

  /* 2. Draw locked field cells */
  for (int row = 0; row < FIELD_HEIGHT; row++) {
    for (int col = 0; col < FIELD_WIDTH; col++) {
      unsigned char cell_value = state->field[row * FIELD_WIDTH + col];
      switch (cell_value) {
        case TETRIS_FIELD_EMPTY: continue;
        case TETRIS_FIELD_I: case TETRIS_FIELD_J: case TETRIS_FIELD_L:
        case TETRIS_FIELD_O: case TETRIS_FIELD_S: case TETRIS_FIELD_T:
        case TETRIS_FIELD_Z:
          draw_cell(backbuffer, col, row, get_tetromino_color(cell_value - 1));
          break;
        case TETRIS_FIELD_WALL:
          draw_cell(backbuffer, col, row, COLOR_GRAY);
          break;
        case TETRIS_FIELD_TMP_FLASH:
          draw_cell(backbuffer, col, row, COLOR_WHITE);
          break;
      }
    }
  }

  /* 3. Draw falling piece */
  draw_piece(backbuffer,
             state->current_piece.index,
             state->current_piece.x,
             state->current_piece.y,
             get_tetromino_color(state->current_piece.index),
             state->current_piece.rotate_x_value);

  /* ── 4. HUD Sidebar ──────────────────────────────────────────────────────
   *
   * sx = pixel X just right of the field
   * sy = pixel Y starting near top
   *
   * char buf[32]: stack-allocated string buffer for number formatting.
   * snprintf fills it with a formatted integer, null-terminated.
   * sizeof(buf) = 32 at compile time — always matches the array declaration.
   *
   * JS: const buf = String(state.score)
   * C:  snprintf(buf, sizeof(buf), "%d", state->score)
   *   "%d" = format as a decimal integer.
   *   sizeof(buf) = 32 = maximum bytes to write (including the '\0').
   */
  int sx = FIELD_WIDTH * CELL_SIZE + 10;
  int sy = 10;
  char buf[32];

  /* Score */
  draw_text(backbuffer, sx, sy, "SCORE", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->score);
  draw_text(backbuffer, sx, sy + 16, buf, COLOR_YELLOW, 2);

  /* Level */
  draw_text(backbuffer, sx, sy + 40, "LEVEL", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->level);
  draw_text(backbuffer, sx, sy + 56, buf, COLOR_CYAN, 2);

  /* Pieces count */
  draw_text(backbuffer, sx + 80, sy + 40, "PIECES", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->pieces_count);
  draw_text(backbuffer, sx + 80, sy + 56, buf, COLOR_CYAN, 2);

  /* Next piece label */
  draw_text(backbuffer, sx, sy + 85, "NEXT", COLOR_WHITE, 2);

  /* Next piece preview — drawn at half cell size (CELL_SIZE / 2 = 15 px).
   * We use tetromino_pos_value with rotation 0 — canonical spawn orientation.
   * No separate draw_piece call needed: we draw mini-cells directly.
   */
  int preview_x         = sx;
  int preview_y         = sy + 105;
  int preview_cell_size = CELL_SIZE / 2;

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (TETROMINOES[state->current_piece.next_index]
                     [tetromino_pos_value(px, py, TETROMINO_R_0)] == TETROMINO_BLOCK) {
        uint32_t color = get_tetromino_color(state->current_piece.next_index);
        draw_rect(backbuffer,
                  preview_x + px * preview_cell_size + 1,
                  preview_y + py * preview_cell_size + 1,
                  preview_cell_size - 2,
                  preview_cell_size - 2,
                  color);
      }
    }
  }

  /* Controls hint at bottom of sidebar */
  int hint_y = FIELD_HEIGHT * CELL_SIZE - 90;
  draw_text(backbuffer, sx, hint_y,      "CONTROLS",          COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 18, "{} MOVE LEFT/RIGHT", COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 28, "v  SOFT DROP",       COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 38, "Z  ROTATE LEFT",     COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 48, "X  ROTATE RIGHT",    COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 58, "R  RESTART",         COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 68, "Q  QUIT",            COLOR_DARK_GRAY, 1);
}

/* ── All other functions unchanged from lesson 09 ────────────────────────── */

void game_init(GameState *state) {
  state->score        = 0;
  state->pieces_count = 1;
  state->level        = 0;
  state->completed_lines.count = 0;
  state->completed_lines.flash_timer.timer    = 0;
  state->completed_lines.flash_timer.interval = 0.4f;
  memset(&state->completed_lines.indexes, 0, sizeof(int) * TETROMINO_LAYER_COUNT);
  state->input_values.rotate_direction.timer    = 0.0f;
  state->input_values.rotate_direction.interval = 0.8f;
  state->input_repeat.move_left.initial_delay        = 0.15f;
  state->input_repeat.move_left.interval             = 0.05f;
  state->input_repeat.move_left.passed_initial_delay = false;
  state->input_repeat.move_right.initial_delay        = 0.15f;
  state->input_repeat.move_right.interval             = 0.05f;
  state->input_repeat.move_right.passed_initial_delay = false;
  state->input_repeat.move_down.initial_delay        = 0.0f;
  state->input_repeat.move_down.interval             = 0.03f;
  state->input_repeat.move_down.passed_initial_delay = false;
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      state->field[y * FIELD_WIDTH + x] =
        (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1)
        ? TETRIS_FIELD_WALL : TETRIS_FIELD_EMPTY;
    }
  }
  srand((unsigned int)time(NULL));
  state->current_piece = (CurrentPiece){
    .x = (int)((FIELD_WIDTH * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
    .y = 0,
    .index          = rand() % TETROMINOS_COUNT,
    .next_index     = rand() % TETROMINOS_COUNT,
    .rotate_x_value = TETROMINO_R_0,
  };
}

int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r) {
    case TETROMINO_R_0:   return py * TETROMINO_LAYER_COUNT + px;
    case TETROMINO_R_90:  return 12 + py - (px * TETROMINO_LAYER_COUNT);
    case TETROMINO_R_180: return 15 - (py * TETROMINO_LAYER_COUNT) - px;
    case TETROMINO_R_270: return 3  - py + (px * TETROMINO_LAYER_COUNT);
  }
  return 0;
}

int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                              int pos_x, int pos_y) {
  if (piece < 0 || piece >= TETROMINOS_COUNT) return 0;
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece][pi] == TETROMINO_SPAN) continue;
      int field_x = pos_x + px, field_y = pos_y + py;
      if (field_x < 0 || field_x >= FIELD_WIDTH)  continue;
      if (field_y < 0 || field_y >= FIELD_HEIGHT)  continue;
      if (state->field[field_y * FIELD_WIDTH + field_x] != TETRIS_FIELD_EMPTY)
        return 0;
    }
  }
  return 1;
}

void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
  for (int btn = 0; btn < BUTTON_COUNT; btn++) {
    current_input->buttons[btn].ended_down            = old_input->buttons[btn].ended_down;
    current_input->buttons[btn].half_transition_count = 0;
  }
}

static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval *repeat,
                                      float delta_time, int *should_trigger) {
  *should_trigger = 0;
  if (!button->ended_down) {
    repeat->timer = 0.0f; repeat->is_active = false;
    repeat->passed_initial_delay = false; return;
  }
  if (button->half_transition_count > 0) {
    repeat->timer = 0.0f; repeat->is_active = true;
    repeat->passed_initial_delay = false;
    if (repeat->initial_delay <= 0.0f) *should_trigger = 1;
    return;
  }
  if (!repeat->is_active) return;
  repeat->timer += delta_time;
  if (!repeat->passed_initial_delay) {
    if (repeat->timer >= repeat->initial_delay) {
      *should_trigger = 1; repeat->timer = 0.0f;
      repeat->passed_initial_delay = true;
    }
  } else {
    if (repeat->timer >= repeat->interval) {
      *should_trigger = 1; repeat->timer -= repeat->interval;
    }
  }
}

static void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {
  if (input->rotate_x.ended_down && input->rotate_x.half_transition_count != 0 &&
      state->current_piece.next_rotate_x_value != TETROMINO_ROTATE_X_NONE) {
    TETROMINO_R_DIR new_rotation;
    if (state->current_piece.next_rotate_x_value == TETROMINO_ROTATE_X_GO_RIGHT) {
      switch (state->current_piece.rotate_x_value) {
        case TETROMINO_R_0:   new_rotation = TETROMINO_R_90;  break;
        case TETROMINO_R_90:  new_rotation = TETROMINO_R_180; break;
        case TETROMINO_R_180: new_rotation = TETROMINO_R_270; break;
        case TETROMINO_R_270: new_rotation = TETROMINO_R_0;   break;
        default:              new_rotation = TETROMINO_R_0;   break;
      }
    } else {
      switch (state->current_piece.rotate_x_value) {
        case TETROMINO_R_0:   new_rotation = TETROMINO_R_270; break;
        case TETROMINO_R_90:  new_rotation = TETROMINO_R_0;   break;
        case TETROMINO_R_180: new_rotation = TETROMINO_R_90;  break;
        case TETROMINO_R_270: new_rotation = TETROMINO_R_180; break;
        default:              new_rotation = TETROMINO_R_0;   break;
      }
    }
    if (tetromino_does_piece_fit(state, state->current_piece.index, new_rotation,
                                  state->current_piece.x, state->current_piece.y))
      state->current_piece.rotate_x_value = new_rotation;
  }
  int sl = 0, sr = 0;
  handle_action_with_repeat(&input->move_left,  &state->input_repeat.move_left,  delta_time, &sl);
  handle_action_with_repeat(&input->move_right, &state->input_repeat.move_right, delta_time, &sr);
  if (sl && tetromino_does_piece_fit(state, state->current_piece.index,
       state->current_piece.rotate_x_value, state->current_piece.x - 1, state->current_piece.y))
    state->current_piece.x--;
  if (sr && tetromino_does_piece_fit(state, state->current_piece.index,
       state->current_piece.rotate_x_value, state->current_piece.x + 1, state->current_piece.y))
    state->current_piece.x++;
  int sd = 0;
  handle_action_with_repeat(&input->move_down, &state->input_repeat.move_down, delta_time, &sd);
  if (sd && tetromino_does_piece_fit(state, state->current_piece.index,
       state->current_piece.rotate_x_value, state->current_piece.x, state->current_piece.y + 1))
    state->current_piece.y++;
}

void game_update(GameState *state, GameInput *input, float delta_time) {
  if (state->should_restart) { game_init(state); return; }
  if (state->completed_lines.flash_timer.timer > 0) {
    state->completed_lines.flash_timer.timer -= delta_time;
    if (state->completed_lines.flash_timer.timer <= 0) {
      for (int i = state->completed_lines.count - 1; i >= 0; i--) {
        int line_index = state->completed_lines.indexes[i];
        for (int py = line_index; py > 0; --py)
          for (int px = 1; px < FIELD_WIDTH - 1; ++px)
            state->field[py * FIELD_WIDTH + px] = state->field[(py-1) * FIELD_WIDTH + px];
        for (int px = 1; px < FIELD_WIDTH - 1; px++) state->field[px] = TETRIS_FIELD_EMPTY;
        for (int j = i - 1; j >= 0; --j)
          if (state->completed_lines.indexes[j] < line_index)
            state->completed_lines.indexes[j]++;
      }
      state->completed_lines.count = 0;
    }
    return;
  }
  tetris_apply_input(state, input, delta_time);
  state->input_values.rotate_direction.timer += delta_time;
  int game_speed = state->level;
  float drop_interval = state->input_values.rotate_direction.interval
                       - (0.01f + game_speed * 0.01f);
  if (drop_interval < 0.1f) drop_interval = 0.1f;
  if (state->input_values.rotate_direction.timer >= drop_interval) {
    state->input_values.rotate_direction.timer -= drop_interval;
    if (tetromino_does_piece_fit(state, state->current_piece.index,
         state->current_piece.rotate_x_value,
         state->current_piece.x, state->current_piece.y + 1)) {
      state->current_piece.y++;
    } else {
      for (int px = 0; px < TETROMINO_LAYER_COUNT; ++px) {
        for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
          int pi = tetromino_pos_value(px, py, state->current_piece.rotate_x_value);
          if (TETROMINOES[state->current_piece.index][pi] != TETROMINO_SPAN) {
            int fx = state->current_piece.x + px, fy = state->current_piece.y + py;
            if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT)
              state->field[fy * FIELD_WIDTH + fx] = state->current_piece.index + 1;
          }
        }
      }
      state->completed_lines.count = 0;
      for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
        int row_y = state->current_piece.y + py;
        if (row_y < 0 || row_y >= FIELD_HEIGHT - 1) continue;
        bool completed = true;
        for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
          if (state->field[row_y * FIELD_WIDTH + px] == TETRIS_FIELD_EMPTY) { completed = false; break; }
        }
        if (completed) {
          for (int px = 1; px < FIELD_WIDTH - 1; ++px)
            state->field[row_y * FIELD_WIDTH + px] = TETRIS_FIELD_TMP_FLASH;
          state->completed_lines.indexes[state->completed_lines.count++] = row_y;
        }
      }
      if (state->completed_lines.count > 0)
        state->completed_lines.flash_timer.timer = state->completed_lines.flash_timer.interval;
      state->pieces_count++;
      state->score += 25;
      if (state->completed_lines.count > 0)
        state->score += (1 << state->completed_lines.count) * 100;
      if (state->pieces_count % 25 == 0) state->level++;
      state->input_values.rotate_direction.timer = 0.0f;
      state->current_piece = (CurrentPiece){
        .x = (int)((FIELD_WIDTH * 0.5f) - (TETROMINO_LAYER_COUNT * 0.5f)),
        .y = 0,
        .index      = state->current_piece.next_index,
        .next_index = rand() % TETROMINOS_COUNT,
        .rotate_x_value = TETROMINO_R_0,
      };
    }
  }
}
```

---

## Build and Run

```bash
clang -Wall -Wextra -g -O0 \
  -o build/game \
  src/game.c src/utils/draw-shapes.c src/utils/draw-text.c src/main_raylib.c \
  -lraylib -lpthread -ldl -lm

./build/game
```

Note: `draw-text.c` is a new translation unit — you must add it to the compile command.

---

## Expected Result

The right side of the window now shows:
- **SCORE** label + current score in yellow
- **LEVEL** + **PIECES** labels + values in cyan
- **NEXT** label + a small preview of the upcoming piece
- Control hints in dark gray at the bottom

Score jumps by 25 each lock, and by 200/400/800/1600 when you clear 1/2/3/4 lines simultaneously.

---

## Common Mistakes

> **Common mistake:** `draw_text(bb, x, y, state->score, ...)` — you cannot pass an `int` where `const char *` is expected. Always convert numbers to strings first: `snprintf(buf, sizeof(buf), "%d", state->score); draw_text(bb, x, y, buf, ...);`

> **Common mistake:** Forgetting to add `draw-text.c` to the compile command. The header `draw-text.h` only declares the functions; the linker needs the `.c` file to find their bodies. Error message: `undefined reference to 'draw_text'`.

---

## Exercises

1. Add a **HIGH SCORE** row to the sidebar that shows the best score this session. It should persist across R restarts.
2. Change the score display color based on the level: white at level 0, yellow at level 3+, red at level 7+. Where is the cleanest place to put this conditional?
3. Display the next piece's name (I, J, L, O, S, T, Z) as text below the preview. How do you map `TETROMINO_BY_IDX` to a string?
4. Try `scale=1` vs `scale=3` for the score number. What is the rendered pixel size of "9999" at each scale?

## What's Next

The game looks great but sounds like nothing. **Lesson 11** adds procedural sound effects synthesized entirely in software — no `.wav` files, no libraries beyond what we already have. Every piece move, rotation, and line clear will have its own audio signature.
