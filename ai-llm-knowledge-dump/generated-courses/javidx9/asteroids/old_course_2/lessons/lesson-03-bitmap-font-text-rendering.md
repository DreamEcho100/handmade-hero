# Lesson 03 — Bitmap Font and Text Rendering

## What you'll learn
- How a CP437/VGA-style 8×8 bitmap font is stored and addressed
- LSB-first bit encoding: why bit 0 is the leftmost pixel
- How `draw_char` and `draw_text` work with the `scale` parameter
- `TextCursor` for flowing multi-column HUD text

---

## FONT_8X8 — glyph storage

```c
// utils/draw-text.c
extern const uint8_t FONT_8X8[128][8];
```

128 glyphs × 8 rows per glyph × 1 byte per row = 1 KiB total.
Each byte encodes which pixels in that row are lit:

```
Glyph 'A' (char 65):
  row 0: 0b00011000 = 0x18  →  . . . # # . . .
  row 1: 0b00100100 = 0x24  →  . . # . . # . .
  row 2: 0b01000010 = 0x42  →  . # . . . . # .
  row 3: 0b01111110 = 0x7E  →  . # # # # # # .   ← cross-bar
  row 4: 0b01000010 = 0x42  →  . # . . . . # .
  row 5: 0b01000010 = 0x42  →  . # . . . . # .
  row 6: 0b00000000 = 0x00  →  (descender empty)
  row 7: 0b00000000 = 0x00
```

---

## LSB-first encoding

Bit 0 (least significant) = leftmost pixel in the row.
The correct mask is `(1 << col)`:

```c
// draw_char — key loop
for (int row = 0; row < 8; row++) {
    uint8_t bits = FONT_8X8[char_index][row];
    for (int col = 0; col < 8; col++) {
        if (bits & (1 << col)) {          // LSB = leftmost
            draw_rect(bb,
                      x + (float)(col * scale),
                      y + (float)(row * scale),
                      (float)scale, (float)scale,
                      r, g, b, a);
        }
    }
}
```

**Common mistake:** using `(0x80 >> col)` assumes MSB = leftmost (BIG7 encoding).
The font data here uses LSB-first (BIT0-left) — always use `(1 << col)`.

---

## draw_char and draw_text

```c
// utils/draw-text.h
void draw_char(Backbuffer *bb, float x, float y, int scale, char c,
               float r, float g, float b, float a);

void draw_text(Backbuffer *bb, float x, float y, int scale,
               const char *text, float r, float g, float b, float a);
```

`scale` is an integer pixel multiplier: scale=1 → 8×8 px, scale=2 → 16×16 px.

`draw_text` calls `draw_char` in a loop, advancing `x` by `8 * scale` each char:

```c
void draw_text(Backbuffer *bb, float x, float y, int scale,
               const char *text, float r, float g, float b, float a) {
    float cx = x;
    while (*text) {
        draw_char(bb, cx, y, scale, *text++, r, g, b, a);
        cx += (float)(8 * scale);
    }
}
```

---

## TextCursor — flowing HUD layout

```c
// utils/render.h
typedef struct {
    float x;   /* pixel x of next character to write */
    float y;   /* pixel y of baseline (top-left of glyph) */
} TextCursor;

static inline TextCursor make_cursor(const RenderContext *ctx, float wx, float wy) {
    TextCursor c;
    c.x = world_x(ctx, wx);
    c.y = world_y(ctx, wy);
    return c;
}
```

`make_cursor` converts world coordinates (game units) to pixel coordinates
using a `RenderContext`.  You feed the result to `draw_text`:

```c
// HUD score (top-left, 0.25 units from corner):
TextCursor cur = make_cursor(&hud_ctx, 0.25f, HUD_TOP_Y(0.25f));
draw_text(bb, cur.x, cur.y, 2, "SCORE: 1234", COLOR_WHITE);
```

---

## Character measurement

To centre text horizontally:
```c
// utils/draw-text.h
int measure_text_px(const char *text, int scale);
// Returns: strlen(text) * 8 * scale
```

Usage for centred game-over banner:
```c
int sc = 2;
int w = measure_text_px("GAME OVER", sc);
float cx = world_x(&hud_ctx, GAME_UNITS_W * 0.5f);  // screen centre
float cy = world_y(&hud_ctx, GAME_UNITS_H * 0.5f);
draw_text(bb, cx - (float)(w / 2), cy, sc, "GAME OVER", COLOR_RED);
```

---

## Key takeaways

1. `FONT_8X8[128][8]`: 128 glyphs × 8 row-bytes = 1 KiB.
2. LSB-first: `(1 << col)` — bit 0 is the leftmost pixel.
3. `scale` multiplies each pixel: scale=2 gives 16×16 px glyphs.
4. `draw_text` advances `x` by `8 * scale` per character.
5. `make_cursor` converts world coords to pixel coords via `RenderContext`.
