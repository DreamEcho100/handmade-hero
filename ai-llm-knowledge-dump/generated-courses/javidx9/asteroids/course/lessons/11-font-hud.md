# Lesson 11 — Bitmap Font + HUD

## What you'll build
An 8×8 pixel bitmap font stored as C data (no font files), `draw_char` and
`draw_text` functions, a score panel in the top-left corner, and a GAME OVER
overlay with restart prompt.

---

## Core Concepts

### 1. `FONT_8X8[128][8]` — The Font Table

The entire printable ASCII character set is encoded as a 2D array:

```c
static const uint8_t FONT_8X8[128][8];
// Indexed by: FONT_8X8[ascii_char][row]
// Each byte: 8 pixels — bit 7 = leftmost column, bit 0 = rightmost
```

**BIT7-LEFT convention:**
- Bit 7 (0x80) = column 0 (leftmost pixel)
- Bit 6 (0x40) = column 1
- …
- Bit 0 (0x01) = column 7 (rightmost pixel)

To test if pixel at `(row, col)` is lit:
```c
if (FONT_8X8[ch][row] & (0x80 >> col))  // shift mask right to reach col
    draw pixel
```

Example — letter 'A' (ASCII 0x41):
```
Row 0: 0x18 = 0001 1000  →  ...##...
Row 1: 0x3C = 0011 1100  →  ..####..
Row 2: 0x66 = 0110 0110  →  .##..##.
Row 3: 0x7E = 0111 1110  →  .######.
Row 4: 0x66 = 0110 0110  →  .##..##.
Row 5: 0x66 = 0110 0110  →  .##..##.
Row 6: 0x66 = 0110 0110  →  .##..##.
Row 7: 0x00 = 0000 0000  →  ........  (blank row for spacing)
```

### 2. `draw_char`

```c
void draw_char(AsteroidsBackbuffer *bb, int x, int y, char c, uint32_t color) {
    unsigned int idx = (unsigned int)(unsigned char)c;
    if (idx >= 128) return;   // out of font range — skip

    for (int row = 0; row < GLYPH_H; row++) {
        uint8_t bits = FONT_8X8[idx][row];
        for (int col = 0; col < GLYPH_W; col++) {
            if (bits & (0x80 >> col)) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < bb->width && py >= 0 && py < bb->height)
                    bb->pixels[py * (bb->pitch / 4) + px] = color;
            }
        }
    }
}
```

Note: text uses **clipped** pixel writes (not toroidal), because score text
must stay on screen, not wrap to the other side.

### 3. `draw_text`

```c
void draw_text(AsteroidsBackbuffer *bb, int x, int y,
               const char *text, uint32_t color) {
    for (int i = 0; text[i] != '\0'; i++)
        draw_char(bb, x + i * GLYPH_W, y, text[i], color);
}
```

Each character advances by `GLYPH_W = 8` pixels.

### 4. `snprintf` — Formatting Numbers

```c
#include <stdio.h>

char buf[32];
snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
draw_text(bb, 8, 8, buf, COLOR_WHITE);
```

`snprintf` is the C equivalent of JavaScript's template literals:
```js
`SCORE: ${score}`
```

The key difference: in C, `snprintf` writes into a pre-allocated buffer.
The `sizeof(buf)` argument prevents buffer overflow.

### 5. Centring Text

```c
const char *msg = "GAME OVER";
int text_width = (int)(strlen(msg) * GLYPH_W);   // 9 chars × 8 px = 72 px
int cx = (bb->width - text_width) / 2;           // horizontally centred
draw_text(bb, cx, bb->height / 2, msg, COLOR_RED);
```

JS equivalent: `(SCREEN_W - text.length * GLYPH_W) / 2`

### 6. GAME OVER Overlay

```c
if (state->phase == PHASE_DEAD) {
    // Dim overlay
    draw_rect_blend(bb, 0, 0, bb->width, bb->height, COLOR_OVERLAY_DIM);

    draw_text(bb, cx - w1/2, cy - 20, "GAME OVER",    COLOR_RED);
    draw_text(bb, cx - ws/2, cy - 4,  score_buf,      COLOR_WHITE);

    if (state->dead_timer <= 0.0f)
        draw_text(bb, cx - w2/2, cy + 16, "PRESS FIRE TO RESTART", COLOR_YELLOW);
}
```

`COLOR_OVERLAY_DIM = GAME_RGBA(0, 0, 0, 160)` — 63% opacity black overlay.

---

## Testing Font Accuracy

A quick sanity check: render all printable characters in a grid:
```c
for (int i = 0x20; i <= 0x7E; i++) {
    int col = (i - 0x20) % 16;
    int row = (i - 0x20) / 16;
    draw_char(bb, 10 + col * 10, 10 + row * 12, (char)i, COLOR_WHITE);
}
```

You should see 5 rows × 16 columns of readable ASCII characters.

---

## Verify

1. "SCORE: 0" appears in the top-left on a black background.
2. "BEST: 0" appears top-right.
3. After death: dimmed screen + "GAME OVER" centred + "PRESS FIRE TO RESTART".
4. Numbers update correctly as score increases.

---

## Summary

| Concept | C | JS equivalent |
|---------|---|---------------|
| Bitmap font | `uint8_t FONT_8X8[128][8]` inline | Canvas `fillText` / font file |
| Bit test | `bits & (0x80 >> col)` | `(bits >> (7-col)) & 1` |
| Number to string | `snprintf(buf, size, "%d", n)` | `` `${n}` `` |
| String length | `strlen(msg)` | `msg.length` |
