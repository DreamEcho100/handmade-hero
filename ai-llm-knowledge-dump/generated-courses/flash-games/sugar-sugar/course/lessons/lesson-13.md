# Lesson 13: Font System and HUD

## What we're building

Add the bitmap font system and use it to render all text in the game: the big title,
the level-select grid numbers, in-game HUD labels (Reset, Gravity Flip, Level N),
and the cup fill-percentage labels.

## What you'll learn

- How an 8×8 bitmap font is stored as bytes (one byte per row, bit = pixel)
- `draw_char()` — glyph indexing and bit extraction
- `draw_text()` and `draw_text_scaled()` — newlines, scale blocks
- `draw_int()` — integer-to-string without `printf`
- Why we use `(1 << col)` instead of `(0x80 >> col)` for bit extraction
- The full title-screen render and level-select grid
- In-game HUD: level number, button labels, cup fill text

## Prerequisites

- Lesson 12 complete (buttons, teleporters, gravity flip in place)

---

## Step 1: The font data (`utils/font.h`)

The entire font lives in a single header — no `.c` file, no build step.

```c
/* src/utils/font.h  —  full file (first introduction) */

/*
 * font.h  —  Public-domain 8×8 bitmap font (ASCII 0x20–0x7E)
 *
 * Each character is 8 bytes — one byte per row, top-to-bottom.
 * Within each byte, bit 0 is the LEFTMOST pixel.
 *
 *   g_font8x8[c - 0x20]  gives the glyph for ASCII character c.
 */
#ifndef FONT_H
#define FONT_H

#include <stdint.h>

/* 96 glyphs covering ASCII 0x20 (' ') through 0x7E ('~') */
static const uint8_t g_font8x8[96][8] = {
    /* 0x20  ' ' */  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x21  '!' */  {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    /* ... (96 entries total) ... */
    /* 0x7E  '~' */  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

#endif /* FONT_H */
```

**JS analogy:** `g_font8x8` is like a lookup table (`Map`) where the key is
`charCode - 32` and the value is an 8-element Uint8Array.  Each element is a
bitmask describing one row of pixels.

Include it in `game.c`:

```c
/* src/game.c  —  at the top (already present) */
#include "utils/font.h"
```

---

## Step 2: `draw_char` — single glyph

```c
/* src/game.c  —  draw_char */

static void draw_char(GameBackbuffer *bb, int x, int y, char c,
                      uint32_t color) {
  /* Silently skip characters outside the font range (avoids out-of-bounds). */
  if (c < 0x20 || c > 0x7E)
    return;

  /* Each character occupies one row of 96 entries: index = c - 0x20 */
  const uint8_t *glyph = g_font8x8[(uint8_t)c - 0x20];

  for (int row = 0; row < 8; row++) {
    uint8_t bits = glyph[row];
    for (int col = 0; col < 8; col++) {
      /* IMPORTANT: use (1 << col), NOT (0x80 >> col).
       *
       * Our font data uses bit 0 = leftmost pixel.
       *   (1 << 0) = 0x01 → tests bit 0 first → col 0 is leftmost. ✓
       *   (0x80 >> 0) = 0x80 → tests bit 7 first → characters appear mirrored. ✗
       *
       * This was a real bug we encountered during development. */
      if (bits & (1 << col))
        draw_pixel(bb, x + col, y + row, color);
    }
  }
}
```

---

## Step 3: `draw_text` — string at 1× scale

```c
/* src/game.c  —  draw_text */

static void draw_text(GameBackbuffer *bb, int x, int y, const char *str,
                      uint32_t color) {
  int cx = x;  /* cursor x — advances right per character */
  while (*str) {
    if (*str == '\n') {
      cx = x;    /* reset to start column */
      y += 12;   /* 8px glyph + 4px leading */
    } else {
      draw_char(bb, cx, y, *str, color);
      cx += 9;   /* 8px glyph + 1px kerning gap */
    }
    str++;
  }
}
```

**JS analogy:**

```js
function drawText(ctx, x, y, str, color) {
  let cx = x;
  for (const ch of str) {
    if (ch === '\n') { cx = x; y += 12; }
    else            { drawChar(ctx, cx, y, ch, color); cx += 9; }
  }
}
```

---

## Step 4: `draw_text_scaled` — large UI text

For readable HUD text, each pixel becomes a `scale × scale` filled rectangle.

```c
/* src/game.c  —  draw_text_scaled */

static void draw_text_scaled(GameBackbuffer *bb, int x, int y, const char *str,
                             uint32_t color, int scale) {
  int cx = x;
  while (*str) {
    if (*str == '\n') {
      cx = x;
      y += (8 + 1) * scale;   /* 8px glyph + 1px leading, all scaled */
    } else if (*str >= 0x20 && *str <= 0x7E) {
      const uint8_t *glyph = g_font8x8[(uint8_t)*str - 0x20];
      for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
          if (bits & (1 << col))
            draw_rect(bb,
                      cx + col * scale,
                      y  + row * scale,
                      scale, scale, color);
        }
      }
      cx += 9 * scale;
    }
    str++;
  }
}
```

Measuring text width before drawing (for centering):

```c
/* Measure width of a string at a given scale — no font API needed */
int text_w = strlen("SUGAR, SUGAR") * 9 * scale;  /* 12 * 9 * 3 = 324 px */
int tx = (CANVAS_W - text_w) / 2;
draw_text_scaled(bb, tx, 20, "SUGAR, SUGAR", COLOR_UI_TEXT, 3);
```

---

## Step 5: `draw_int` — integer without `printf`

```c
/* src/game.c  —  draw_int
 *
 * Converts int n to a decimal string in a local buffer (no heap, no stdio).
 * Works for n = -2 147 483 648 to +2 147 483 647.
 *
 * Build digits in reverse order in tmp[], then copy in correct order to buf[]. */
static void draw_int(GameBackbuffer *bb, int x, int y, int n, uint32_t color) {
  char buf[12];
  int i = 0;
  if (n == 0) {
    buf[i++] = '0';
  } else {
    if (n < 0) { buf[i++] = '-'; n = -n; }
    char tmp[10]; int ti = 0;
    while (n > 0) { tmp[ti++] = (char)('0' + (n % 10)); n /= 10; }
    while (ti > 0) buf[i++] = tmp[--ti];
  }
  buf[i] = '\0';
  draw_text(bb, x, y, buf, color);
}
```

**JS analogy:**

```js
function drawInt(ctx, x, y, n, color) {
  drawText(ctx, x, y, String(n), color);
}
```

(In C we cannot call `sprintf` in the hot rendering path without pulling in libc
and risking subtle locale/threading issues.  A small inline converter is safer and
faster for small numbers.)

---

## Step 6: Title screen rendering

```c
/* src/game.c  —  render_title */

static void render_title(const GameState *state, GameBackbuffer *bb) {
  /* Background */
  draw_rect(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_BG);

  /* ---- Big title "SUGAR, SUGAR" at 3× scale (24 px tall) ---- */
  int tx = (CANVAS_W - 12 * 9 * 3) / 2;  /* 12 chars * 9px * scale3 = 324px */
  draw_text_scaled(bb, tx, 20, "SUGAR, SUGAR", COLOR_UI_TEXT, 3);

  /* ---- Subtitle ---- */
  draw_text(bb, (CANVAS_W - 15 * 9) / 2, 76, "Select a level:", COLOR_UI_TEXT);

  /* ---- Thin separator line ---- */
  draw_rect(bb, 40, 95, CANVAS_W - 80, 1, COLOR_BTN_BORDER);

  /* ---- Level select grid (TITLE_COLS columns, rows auto) ---- */
  for (int i = 0; i < TOTAL_LEVELS; i++) {
    int col = i % TITLE_COLS;
    int row = i / TITLE_COLS;
    int bx  = TITLE_GRID_X + col * (TITLE_BTN_W + TITLE_BTN_GAP);
    int by  = TITLE_GRID_Y + row * (TITLE_BTN_H + TITLE_BTN_GAP);

    int locked   = (i >= state->unlocked_count);
    uint32_t bg  = locked                      ? GAME_RGB(200, 195, 190)
                 : (state->title_hover == i)   ? COLOR_BTN_HOVER
                                               : COLOR_BTN_NORMAL;
    uint32_t tcol = locked ? GAME_RGB(155, 150, 145) : COLOR_UI_TEXT;

    draw_rect(bb, bx, by, TITLE_BTN_W, TITLE_BTN_H, bg);
    draw_rect_outline(bb, bx, by, TITLE_BTN_W, TITLE_BTN_H, COLOR_BTN_BORDER);

    /* Level number centered in button at 2× scale */
    {
      int lvl    = i + 1;                        /* 1-based for display */
      int digits = (lvl >= 10) ? 2 : 1;
      int ntx    = bx + (TITLE_BTN_W - digits * 9 * 2) / 2;
      int nty    = by + (TITLE_BTN_H - 8 * 2) / 2;
      /* int-to-string inline (draw_int is 1× only) */
      char buf[4]; int bi = 0;
      char tmp[4]; int ti = 0;
      int  n = lvl;
      while (n > 0) { tmp[ti++] = (char)('0' + (n % 10)); n /= 10; }
      while (ti > 0) buf[bi++] = tmp[--ti];
      buf[bi] = '\0';
      draw_text_scaled(bb, ntx, nty, buf, tcol, 2);
    }
  }

  /* ---- Footer hints ---- */
  draw_text(bb, 20,             CANVAS_H - 18, "ESC: quit",             COLOR_UI_TEXT);
  draw_text(bb, CANVAS_W - 18 * 9, CANVAS_H - 18, "Click a number to play", COLOR_UI_TEXT);
}
```

The grid layout constants (set in `game.h`) keep the grid centred on the canvas:

```c
/* src/game.h  —  title grid layout */
#define TITLE_COLS     6
#define TITLE_BTN_W    72
#define TITLE_BTN_H    44
#define TITLE_BTN_GAP   8
/* Centre the grid horizontally */
#define TITLE_GRID_X  ((CANVAS_W - TITLE_COLS * (TITLE_BTN_W + TITLE_BTN_GAP) + TITLE_BTN_GAP) / 2)
#define TITLE_GRID_Y  110
```

---

## Step 7: In-game HUD with text

The UI buttons already draw their background rectangles (Lesson 12).  Now add text
labels and the level-number indicator:

```c
/* src/game.c  —  render_ui_buttons: label additions */

/* Reset button label */
draw_text(bb, rx + 8, ry + (rh - 8) / 2, "RESET", COLOR_UI_TEXT);

/* Gravity button label — shows current direction */
const char *label = (state->gravity_sign > 0) ? "FLIP v" : "FLIP ^";
draw_text(bb, gx + 8, gy + (gh - 8) / 2, label, COLOR_UI_TEXT);

/* Level number indicator (top-right corner) */
draw_text_scaled(bb, CANVAS_W - 120, 8, "LEVEL", COLOR_UI_TEXT, 2);
{
  int lvl    = state->current_level + 1;
  int digits = (lvl >= 10) ? 2 : 1;
  int nx     = CANVAS_W - digits * 18 - 8;   /* 9px * 2scale = 18px/digit */
  char buf[4]; int bi = 0;
  char tmp[4]; int ti = 0;
  int  n = lvl;
  while (n > 0) { tmp[ti++] = (char)('0' + (n % 10)); n /= 10; }
  while (ti > 0) buf[bi++] = tmp[--ti];
  buf[bi] = '\0';
  draw_text_scaled(bb, nx, 8, buf, COLOR_UI_TEXT, 2);
}
```

Cup fill-percentage labels above each cup:

```c
/* src/game.c  —  render_cups: text labels */

char buf[8];
if (cup->collected >= cup->required_count) {
  /* Centred "OK" in green */
  int tx = cup->x + (cup->w - 2 * 9 * 2) / 2;
  draw_text_scaled(bb, tx, cup->y - 20, "OK", COLOR_CUP_FILL_FULL, 2);
} else {
  /* "N%" percentage label */
  int pct = (cup->collected * 100) / cup->required_count;
  int i = 0;
  char tmp[4]; int ti = 0;
  int  n = pct;
  if (n == 0) { buf[i++] = '0'; }
  else { while (n > 0) { tmp[ti++] = (char)('0' + (n % 10)); n /= 10; } while (ti > 0) buf[i++] = tmp[--ti]; }
  buf[i++] = '%'; buf[i] = '\0';
  int tx = cup->x + (cup->w - (int)strlen(buf) * 9 * 2) / 2;
  draw_text_scaled(bb, tx, cup->y - 20, buf, COLOR_UI_TEXT, 2);
}
```

---

## Build and run (both backends)

```bash
# Raylib
./build-dev.sh --backend=raylib -r

# X11
./build-dev.sh --backend=x11 -r
```

**Expected output — both backends identical:**
- Title screen: "SUGAR, SUGAR" in large pixels, level grid with numbers 1–30.
- Locked levels show greyed numbers; unlocked levels show dark text; hovered level highlights.
- In-game: "RESET" and "FLIP v/^" labels on HUD buttons; "LEVEL N" indicator top-right; cup labels showing "42%" or "OK".

---

## Exercises

1. Add a second line to the subtitle that shows `"X / 30 levels completed"` using `draw_text`.  How do you format the integer without `printf`?
2. `draw_text_scaled` at `scale=1` should be identical to `draw_text`.  Verify this by temporarily replacing a `draw_text` call with `draw_text_scaled(..., 1)` and checking the output is pixel-identical.
3. Make the locked-level buttons show a lock symbol (`#`) instead of a number.  What changes are needed in the render loop?

---

## What's next

Lesson 14 covers the full gameplay polish: the level-complete overlay, the freeplay
mode, keyboard shortcuts (R/G/Escape), and the complete `render_playing` function
with all entities visible.
