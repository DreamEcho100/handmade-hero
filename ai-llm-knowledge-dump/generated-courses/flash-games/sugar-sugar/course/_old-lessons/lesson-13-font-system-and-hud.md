# Lesson 13 — Font System and HUD

## What you build

You render the full HUD: a level number in the top-right corner, cup fill progress bars with numeric countdowns, and an on-screen "RESET" button — all using a hand-rolled 8×8 bitmap font with no external library.

## Concepts introduced

- `font.h` 8×8 bitmap font — 96 glyphs for ASCII 0x20–0x7E
- `g_font8x8[c - 0x20]` glyph indexing
- Bit-test `glyph[row] & (1 << col)` to read individual pixels
- `draw_char(bb, x, y, c, color)` — renders one 8×8 character
- `draw_text(bb, x, y, str, color)` — renders a null-terminated string
- `draw_text_scaled(bb, x, y, str, color, scale)` — scaled version for bigger text
- `snprintf` for integer-to-string conversion (NOT `printf`)
- Cup progress bar: rising fill rectangle calculated from `collected / required_count`
- HUD layout constants shared between update and render functions

## JS → C analogy

| JavaScript                                               | C                                                          |
|----------------------------------------------------------|------------------------------------------------------------|
| `ctx.font = '8px monospace'; ctx.fillText('A', x, y)`   | `draw_char(bb, x, y, 'A', COLOR_UI_TEXT)`                 |
| Custom bitmap font: `(glyph[row] >> (7-col)) & 1`       | `glyph[row] & (1 << col)` — same bit, different notation  |
| `String(n)` to convert a number                          | `snprintf(buf, sizeof(buf), "%d", n)`                      |
| `console.log(n)` to print to the terminal                | `printf("%d\n", n)` — but this writes to stdout, not HUD  |
| `ctx.fillRect(x, fillY, w, fillH)` for a progress bar   | `draw_rect(bb, x, fillY, w, fillH, COLOR_CUP_FILL)`       |
| `element.style.color = 'green'` when full                | `full ? COLOR_CUP_FILL_FULL : COLOR_CUP_FILL`             |

## Step-by-step

### Step 1: Understand the font data (font.h)

`font.h` contains a single static array:

```c
static const uint8_t g_font8x8[96][8] = { … };
```

- **96 glyphs** covering printable ASCII: space (0x20) through tilde (0x7E).
- Each glyph is **8 bytes** — one byte per row, top to bottom.
- Within each byte, **bit 0 is the leftmost pixel** (the convention used by
  this font data).

To find the glyph for character `c`:

```c
const uint8_t *glyph = g_font8x8[(uint8_t)c - 0x20];
```

This works for any `c` in the range `0x20–0x7E`. The cast to `uint8_t`
prevents undefined behaviour if `c` is a `char` with a negative value for
extended characters.

### Step 2: Bit-test a glyph pixel (font.h comment vs. game.c)

The `font.h` header comment shows the convention as:

```c
if (glyph[row] & (0x80 >> col))  // bit 7 = leftmost pixel
```

However, the actual font data in `font.h` is stored with **bit 0 as
leftmost**. The `draw_char` implementation in `game.c` therefore uses:

```c
if (bits & (1 << col))    // bit 0 = leftmost pixel (col 0)
```

Both notations test the same physical pixel — the key is consistency between
the font data layout and the test expression. The source comment in
`game.c` explains:

```c
/* BIT 0 = leftmost pixel (the convention used by our font data).
 * Using (1<<col) tests bit 0 first → col 0 is leftmost. ✓
 * The previous (0x80>>col) tested bit 7 first → chars were mirrored. */
```

If you use `font.h` in a new project, always check which bit convention the
data uses before writing the inner loop.

### Step 3: Implement `draw_char` (game.c)

```c
static void draw_char(GameBackbuffer *bb, int x, int y, char c,
                      uint32_t color) {
    if (c < 0x20 || c > 0x7E)
        return;   /* silently skip non-printable characters */
    const uint8_t *glyph = g_font8x8[(uint8_t)c - 0x20];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++)
            if (bits & (1 << col))
                draw_pixel(bb, x + col, y + row, color);
    }
}
```

The inner `draw_pixel` call clips against canvas bounds so no additional
bounds-check is needed here.

### Step 4: Implement `draw_text` (game.c)

```c
static void draw_text(GameBackbuffer *bb, int x, int y, const char *str,
                      uint32_t color) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx  = x;
            y  += 12;          /* 8px glyph + 4px line gap */
        } else {
            draw_char(bb, cx, y, *str, color);
            cx += 9;           /* 8px glyph + 1px character gap */
        }
        str++;
    }
}
```

Each character advances 9 pixels horizontally (8px glyph + 1px gap).
Newlines reset `cx` to the original `x` and advance `y` by 12.

### Step 5: Implement `draw_text_scaled` (game.c)

For readable UI labels (the 8px glyphs are tiny at 640×480), use the scaled
variant. Each font pixel becomes a `scale × scale` block:

```c
static void draw_text_scaled(GameBackbuffer *bb, int x, int y, const char *str,
                             uint32_t color, int scale) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx  = x;
            y  += (8 + 1) * scale;
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

At `scale=2` each glyph is 16px tall and the advance is 18px per character.
At `scale=3` it is 24px tall — used for the game title on the title screen.

### Step 6: Integer-to-string conversion — use `snprintf`, not `printf`

`printf` writes to `stdout` (the terminal). That is useless for the HUD.
`snprintf` writes to a `char` buffer that you then pass to `draw_text`:

```c
char buf[8];
snprintf(buf, sizeof(buf), "%d", remaining);   /* safe: bounded by sizeof(buf) */
draw_text_scaled(bb, tx, cup->y - 20, buf, COLOR_UI_TEXT, 2);
```

`snprintf(buf, n, fmt, …)` always null-terminates and never writes more
than `n` bytes — it is the safe version of `sprintf`. Using `snprintf`
instead of manual digit extraction also handles edge cases (negative
numbers, zero) automatically.

> **Why NOT `printf`?**  
> `printf("%d\n", n)` writes to the terminal (`stdout`), which is useful
> for debugging but produces no pixels on the game canvas. The renderer only
> reads `bb->pixels`. Always use `snprintf` when you need a string in a
> pixel buffer.

### Step 7: Render the level number (game.c)

In `render_ui_buttons()`, the level number is drawn in the top-right corner
at 2× scale:

```c
draw_text_scaled(bb, CANVAS_W - 120, 8, "LEVEL", COLOR_UI_TEXT, 2);

/* Draw the digit(s) right-aligned */
int lvl    = state->current_level + 1;   /* 0-based index → 1-based display */
int digits = (lvl >= 10) ? 2 : 1;
int nx     = CANVAS_W - digits * 18 - 8; /* 18 = 9 * scale(2) per digit     */
char buf[4];
/* integer-to-string (manual loop identical to snprintf for small ints) */
…
draw_text_scaled(bb, nx, 8, buf, COLOR_UI_TEXT, 2);
```

`CANVAS_W - digits * 18 - 8` right-aligns the number: a 2-digit number
occupies 36px; a 1-digit number occupies 18px, both with an 8px right margin.

### Step 8: Render cup progress bars (game.c)

```c
static void render_cups(const LevelDef *lv, GameBackbuffer *bb) {
    for (int c = 0; c < lv->cup_count; c++) {
        const Cup *cup = &lv->cups[c];
        int full = (cup->collected >= cup->required_count);

        /* ---- Interior fill (progress bar rising from bottom) ---- */
        draw_rect(bb, cup->x + 1, cup->y, cup->w - 2, cup->h - 1,
                  COLOR_CUP_EMPTY);

        if (cup->required_count > 0) {
            int filled_h = full
                ? (cup->h - 1)
                : (cup->collected * (cup->h - 1)) / cup->required_count;
            draw_rect(bb,
                      cup->x + 1,
                      cup->y + (cup->h - 1) - filled_h,  /* y: top of fill */
                      cup->w - 2,
                      filled_h,
                      full ? COLOR_CUP_FILL_FULL : COLOR_CUP_FILL);
        }

        /* ---- Cup walls ---- */
        draw_rect(bb, cup->x,            cup->y, 1, cup->h, COLOR_CUP_BORDER);
        draw_rect(bb, cup->x + cup->w-1, cup->y, 1, cup->h, COLOR_CUP_BORDER);
        draw_rect(bb, cup->x, cup->y + cup->h-1, cup->w, 1, COLOR_CUP_BORDER);
    }
}
```

The `filled_h` formula in plain English:

```
filled_h = (collected / required_count) * (cup_height - 1)
```

In integer arithmetic: `(cup->collected * (cup->h - 1)) / cup->required_count`.
The `-1` reserves one pixel for the solid bottom wall stamped by
`stamp_cups()`.

### Step 9: Render the count above each cup (game.c)

When the cup is not yet full, the remaining count is displayed above it:

```c
if (full) {
    int tx = cup->x + cup->w / 2 - 9 * 2;  /* "OK" = 2 chars × 9 × 2 */
    draw_text_scaled(bb, tx, cup->y - 20, "OK", COLOR_CUP_FILL_FULL, 2);
} else {
    int remaining = cup->required_count - cup->collected;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", remaining);
    /* measure width to centre the number */
    int bi  = (int)strlen(buf);   /* NOTE: strlen needs <string.h> */
    int tw  = bi * 18;            /* 18px per char at scale 2 */
    int tx  = cup->x + (cup->w - tw) / 2;
    draw_text_scaled(bb, tx, cup->y - 20, buf, COLOR_UI_TEXT, 2);
}
```

The actual `game.c` source uses a manual digit-counting loop to avoid
`#include <string.h>` in the render path. `snprintf` is the cleaner
approach when `<stdio.h>` is already included.

### Step 10: Render the RESET button (game.c)

```c
static void render_ui_buttons(const GameState *state, GameBackbuffer *bb) {
    int rx = UI_RESET_X, ry = UI_BTN_Y, rw = UI_RESET_W, rh = UI_BTN_H;
    uint32_t bg = state->reset_hover ? COLOR_BTN_HOVER : COLOR_BTN_NORMAL;
    draw_rect(bb, rx, ry, rw, rh, bg);
    draw_rect_outline(bb, rx, ry, rw, rh, COLOR_BTN_BORDER);
    draw_text(bb, rx + 8, ry + (rh - 8) / 2, "RESET", COLOR_UI_TEXT);
}
```

The layout constants are `#define`d at the top of `game.c`:

```c
#define UI_BTN_Y    (CANVAS_H - 38)   /* bottom strip */
#define UI_BTN_H    28
#define UI_RESET_X  10
#define UI_RESET_W  70
#define UI_GRAV_X   88
#define UI_GRAV_W   100
```

Using `#define` constants — rather than magic numbers scattered in both
`update_playing` and `render_ui_buttons` — ensures the hover detection
rectangle exactly matches the drawn button. If the constants differ, the
button appears not to respond when clicked.

### Step 11: Colored cup indicator dot (game.c)

For cups with a `required_color != GRAIN_WHITE`, a small dot inside the
cup shows the expected color:

```c
if (cup->required_color != GRAIN_WHITE) {
    draw_circle(bb,
                cup->x + cup->w / 2,
                cup->y + (cup->h / 2) + 4,
                6,
                g_grain_colors[cup->required_color]);
}
```

This gives the player a visual reminder of which color goes where without
text.

## Common mistakes to prevent

- **Calling `printf` instead of `snprintf`**: `printf` writes to the
  terminal; no pixels change on the canvas.
- **Forgetting `c < 0x20` guard in `draw_char`**: tab, newline, and
  control characters would index `g_font8x8` at negative or near-zero
  offsets, causing out-of-bounds reads.
- **Using `0x80 >> col` when the font stores bit-0-left**: characters render
  mirrored. Always match the bit-test to the font data's convention.
- **Integer overflow in `filled_h`**: `cup->collected * cup->h` can overflow
  `int` if both are large. Keep operands reasonable (cup height is at most
  480; `required_count` is at most ~300).
- **Layout constants defined only in the render function**: the hover
  detection in `update_playing` must use the same rectangle. Use shared
  `#define` constants or a shared layout struct.
- **Off-by-one in bar height**: the bottom wall occupies one pixel
  (`y + h - 1`). The fill should use `cup->h - 1` as the full height, not
  `cup->h`, or the bar will overwrite the bottom border pixel.

## What to run

```bash
./build-dev.sh
./sugar_x11     # or ./sugar_raylib
```

Verify:
1. Level number appears top-right in readable size (2× scale).
2. Cup countdown number decreases as grains are collected.
3. When a cup is full, "OK" appears above it in green.
4. The progress bar rises from the bottom of the cup as grains arrive.
5. RESET button highlights on hover and erases lines when clicked.
6. Colored cups show a small colored dot inside.

## Summary

The HUD is built entirely from two primitives: `draw_rect` (colored
rectangles) and a hand-rolled 8×8 bitmap font stored as 96 bytes of bit
patterns.  `draw_char` walks the 8×8 grid bit by bit; `draw_text` chains
characters with a 9px advance; `draw_text_scaled` grows each bit into a
`scale×scale` block for legibility.  The critical rule is to use `snprintf`
(not `printf`) when you need a number as a string — `snprintf` writes into a
`char` buffer in memory, while `printf` writes to the terminal.  Cup
progress bars, countdown numbers, and the level indicator are all
constructed from these same primitives, with layout constants shared between
the update and render passes to guarantee pixel-perfect hover detection.
