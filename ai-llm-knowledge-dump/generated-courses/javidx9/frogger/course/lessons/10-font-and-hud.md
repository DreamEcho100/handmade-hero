# Lesson 10: Font + HUD

## What we're building

Add a bitmap font and heads-up display (HUD) — score, lives counter, homes
filled, and a time countdown — drawn every frame using `draw_text()` and
`snprintf()`.  We introduce the `FONT_8X8[128][8]` BIT7-left convention and
explain why it is an improvement over the reference source's 5×7 font.

## What you'll learn

- `FONT_8X8[128][8]` BIT7-left bitmap font convention
- `draw_char()` and `draw_text()` pixel-level implementations
- Bit testing: `FONT_8X8[c][row] & (0x80 >> col)`
- `snprintf()` for composing HUD strings in C (JS `template literals` analogy)
- Why inlining the font in the binary beats loading from a file

## Prerequisites

Lessons 01–09 complete and building.

---

## Step 1 — FONT_8X8[128][8]: BIT7-left convention

The font is a 128-entry static array — one entry per ASCII character (indexed
directly by ASCII value, no `ch - 32` offset).  Each entry is 8 bytes, one
byte per row.  Within each byte, **bit 7 is the leftmost pixel**:

```
FONT_8X8[c][row] & (0x80 >> col)   →  1 if pixel (col, row) is lit
```

Example — letter `'A'` (ASCII 0x41 = 65):

```
Row 0:  0x18  = 0001 1000  →  ...##...
Row 1:  0x3C  = 0011 1100  →  ..####..
Row 2:  0x66  = 0110 0110  →  .##..##.
Row 3:  0x7E  = 0111 1110  →  .######.
Row 4:  0x66  = 0110 0110  →  .##..##.
Row 5:  0x66  = 0110 0110  →  .##..##.
Row 6:  0x66  = 0110 0110  →  .##..##.
Row 7:  0x00  = 0000 0000  →  ........
```

**JS analogy:**

```js
// BIT7-left: test if pixel at (col, row) of char c is lit
const isLit = (FONT_8X8[c][row] & (0x80 >> col)) !== 0;
```

> **Course Note (gap #13):** The reference Frogger source uses a 5×7
> column-major `font_glyphs[96][5]` font (96 printable chars starting at
> space, column-major = each byte is a column).  The course upgrades to
> `FONT_8X8[128][8]` (full 128-char ASCII, row-major BIT7-left) for three
> reasons:
> 1. Direct ASCII indexing — no `ch - 32` offset, no bounds check.
> 2. Larger glyph set — control characters 0–31 usable as custom symbols.
> 3. Consistent with Tetris/Snake/Asteroids — same font across all courses.
>
> Trade-off: `FONT_8X8` uses ~1 KB vs ~480 B for the reference font.
> At game scales this difference is negligible.  Documented in
> `COURSE-BUILDER-IMPROVEMENTS.md`.

---

## Step 2 — draw_char and draw_text

```c
/* From utils/draw-text.h */
#define GLYPH_W 8
#define GLYPH_H 8

void draw_char(Backbuffer *bb, char c, int x, int y, uint32_t color);
void draw_text(Backbuffer *bb, const char *text, int x, int y, uint32_t color);
```

Implementation of `draw_char`:

```c
void draw_char(Backbuffer *bb, char c, int x, int y, uint32_t color) {
    unsigned char uc = (unsigned char)c;
    if (uc >= 128) return;  /* only 0–127 defined */

    for (int row = 0; row < GLYPH_H; row++) {
        uint8_t bits = FONT_8X8[uc][row];
        for (int col = 0; col < GLYPH_W; col++) {
            if (bits & (0x80 >> col)) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < bb->width &&
                    py >= 0 && py < bb->height)
                    bb->pixels[py * bb->width + px] = color;
            }
        }
    }
}
```

`draw_text` loops over each character and advances `x` by `GLYPH_W`:

```c
void draw_text(Backbuffer *bb, const char *text, int x, int y, uint32_t color) {
    while (*text) {
        draw_char(bb, *text, x, y, color);
        x += GLYPH_W;
        text++;
    }
}
```

**JS analogy:**

```js
// ctx.fillText() does the same in one call; draw_text() does it pixel by pixel
ctx.fillStyle = '#fff';
ctx.fillText('SCORE: 000', x, y);
```

---

## Step 3 — snprintf for HUD strings

`snprintf()` writes a formatted string into a fixed-size char buffer — the C
equivalent of template literals:

```c
char buf[32];
snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
draw_text(bb, buf, 8, 8, COLOR_WHITE);

snprintf(buf, sizeof(buf), "LIVES: %d", state->lives);
draw_text(bb, buf, 8, 18, COLOR_WHITE);

snprintf(buf, sizeof(buf), "HOMES: %d/5", state->homes_reached);
draw_text(bb, buf, 8, 28, COLOR_WHITE);

snprintf(buf, sizeof(buf), "BEST: %d", state->best_score);
draw_text(bb, buf, SCREEN_W - 72, 8, COLOR_YELLOW);
```

**JS equivalent:**

```js
ctx.fillText(`SCORE: ${score}`, 8, 8);
ctx.fillText(`LIVES: ${lives}`, 8, 18);
```

**Why `snprintf` not `sprintf`?**  `snprintf` takes a maximum length
(`sizeof(buf)`) and never writes past it.  `sprintf` will overflow the buffer
if the formatted string is longer than expected — a classic C security bug.
Always use `snprintf`.

---

## Step 4 — Why inline the font?

The font data (`FONT_8X8[128][8]`) is defined as a `static const uint8_t`
array directly in `utils/draw-text.c`.  This means:

- **No file I/O at startup** — font is always available, zero latency.
- **No error handling** — no missing asset, no path config.
- **Compile-time constant** — compiler can optimise away the bit tests.

Trade-off: adding or editing a character requires recompiling.  For a game
font this is fine — the glyph set is fixed.

---

## Build and run

```bash
# X11 backend
./build-dev.sh --backend=x11 -r

# Raylib backend
./build-dev.sh --backend=raylib -r
```

---

## Expected result

Top-left HUD shows: `SCORE: 0`, `LIVES: 3`, `HOMES: 0/5`.  Top-right shows
`BEST: 0` (updates after each win).  All text updates live as gameplay
progresses.

## Exercises

1. Change the score display to zero-pad to 6 digits: `"SCORE: %06d"`.
2. Add a `TIME: %d` countdown (start at 60, decrement with `state->time`).
3. Draw the HUD text with a 1-pixel black shadow by calling `draw_text` twice:
   once at `(x+1, y+1)` in black, then at `(x, y)` in white.

## What's next

Lesson 11 adds audio — procedural sound synthesis for hop, splash, home-reached,
and game-over events, with spatial panning based on the frog's X position.
