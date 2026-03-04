# Lesson 02 — Drawing Primitives and the Grid

**Source files:** `src/utils/draw-shapes.c`, `src/utils/draw-shapes.h`, `src/game.c`
**Functions:** `draw_rect()`, `draw_rect_blend()`, `draw_cell()`

---

## What We're Building

Two rectangle-drawing functions — one solid, one alpha-blended — plus a thin
wrapper that maps a grid cell coordinate to pixel space, taking into account the
header rows reserved for the score panel.

---

## The Concept

### JS analogy — `ctx.fillRect` and `ctx.globalAlpha`

In JavaScript canvas drawing you write:

```js
// Solid rectangle
ctx.fillStyle = 'rgb(50, 205, 50)';
ctx.fillRect(x, y, w, h);

// Semi-transparent overlay
ctx.globalAlpha = 0.7;
ctx.fillStyle = 'black';
ctx.fillRect(0, 0, canvas.width, canvas.height);
ctx.globalAlpha = 1.0;  // reset
```

The canvas API clips to the canvas bounds automatically and blends pixels using
the GPU.  Here we do both of those things ourselves in plain C loops — which
means you can see exactly how clipping and blending work.

The grid overlay is equivalent to drawing an HTML `<canvas>` that sits inside a
larger `<div>` which has a fixed header bar:

```js
// header bar = HEADER_ROWS * CELL_SIZE pixels tall
const FIELD_Y_OFFSET = HEADER_ROWS * CELL_SIZE; // 3 * 20 = 60 px

function drawCell(col, row, color) {
    ctx.fillStyle = color;
    ctx.fillRect(
        col  * CELL_SIZE + 1,
        FIELD_Y_OFFSET + row * CELL_SIZE + 1,
        CELL_SIZE - 2,
        CELL_SIZE - 2
    );
}
```

The C `draw_cell()` in `game.c` is a direct translation of that snippet.

---

## The Code

### `draw_rect` — solid fill with clipping (`src/utils/draw-shapes.c`)

```c
void draw_rect(SnakeBackbuffer *bb,
               int x, int y, int w, int h,
               uint32_t color) {

    /* ── Clipping ────────────────────────────────────────────────────────
     * Compute the clamped pixel range [x0, x1) × [y0, y1).
     * If the rectangle is fully outside the buffer, x0 >= x1 or y0 >= y1
     * and the inner loop body never executes — no crash, no garbage write.
     *
     * JS: Math.max(0, x) and Math.min(bb->width, x + w)              */
    int x0 = x < 0            ? 0           : x;
    int y0 = y < 0            ? 0           : y;
    int x1 = x + w > bb->width  ? bb->width  : x + w;
    int y1 = y + h > bb->height ? bb->height : y + h;

    int px, py;
    for (py = y0; py < y1; py++) {
        for (px = x0; px < x1; px++) {
            /* pixels_per_row = pitch / 4   (pitch is bytes; each pixel = 4 bytes)
             * JS: imageData.data[py * imageData.width + px] = color            */
            bb->pixels[py * (bb->pitch / 4) + px] = color;
        }
    }
}
```

**Why four clamp lines instead of bounds-checking inside the loop?**
Pre-computing `x0, y0, x1, y1` moves the check outside the hot loop.  The
inner loop runs with no branch — just a flat write — which the CPU can execute
at full memory bandwidth.  JS engines do this optimisation automatically; in C
you do it explicitly.

### `draw_rect_blend` — alpha blending (`src/utils/draw-shapes.c`)

```c
void draw_rect_blend(SnakeBackbuffer *bb,
                     int x, int y, int w, int h,
                     uint32_t color) {

    int alpha = (int)((color >> 24) & 0xFF);

    /* Fast paths — skip the blending math for trivial cases */
    if (alpha == 255) { draw_rect(bb, x, y, w, h, color); return; }
    if (alpha ==   0) return;

    /* Extract source colour channels */
    int src_r = (int)((color >> 16) & 0xFF);
    int src_g = (int)((color >>  8) & 0xFF);
    int src_b = (int)( color        & 0xFF);
    int inv_a = 255 - alpha;

    int x0 = x < 0            ? 0           : x;
    int y0 = y < 0            ? 0           : y;
    int x1 = x + w > bb->width  ? bb->width  : x + w;
    int y1 = y + h > bb->height ? bb->height : y + h;

    int px, py;
    for (py = y0; py < y1; py++) {
        for (px = x0; px < x1; px++) {
            uint32_t *dst = &bb->pixels[py * (bb->pitch / 4) + px];

            /* Read destination channels from the existing pixel */
            int dst_r = (int)((*dst >> 16) & 0xFF);
            int dst_g = (int)((*dst >>  8) & 0xFF);
            int dst_b = (int)( *dst        & 0xFF);

            /* "over" composite formula:
             *   out = (src * alpha + dst * (255 - alpha)) / 255
             * Integer math — /255 truncates by at most 1 LSB, imperceptible. */
            int out_r = (src_r * alpha + dst_r * inv_a) / 255;
            int out_g = (src_g * alpha + dst_g * inv_a) / 255;
            int out_b = (src_b * alpha + dst_b * inv_a) / 255;

            *dst = GAME_RGB(out_r, out_g, out_b);
        }
    }
}
```

The formula `(src * alpha + dst * (255 - alpha)) / 255` is the same one used
by every compositing system — CSS `opacity`, Photoshop layers, Web Audio gain
nodes.  With `alpha = 180` (~70% opaque), the source colour wins 70% and the
background bleeds through 30%.

JS equivalent:
```js
// Per-pixel alpha blend (what the browser's compositing engine does for you)
const outR = (srcR * alpha + dstR * (255 - alpha)) / 255 | 0;
```

**Why integer maths and not floats?**  Floating-point division in a loop over
hundreds of thousands of pixels adds latency.  Integer `/255` is an exact
operation on integer inputs and produces the same visible result.  The maximum
rounding error is 1 colour step, which is below the threshold of perception.

### `draw_cell` — grid coordinate to pixel (`src/game.c`)

```c
static void draw_cell(SnakeBackbuffer *bb, int col, int row, uint32_t color) {
    /* HEADER_ROWS * CELL_SIZE = 3 * 20 = 60 px — the score panel height.
     * Grid row 0 starts at pixel row 60, not pixel row 0.              */
    int field_y_offset = HEADER_ROWS * CELL_SIZE;

    draw_rect(bb,
              col  * CELL_SIZE + 1,                   /* left edge + 1px gap  */
              field_y_offset + row * CELL_SIZE + 1,   /* top edge  + 1px gap  */
              CELL_SIZE - 2,                           /* width  = 18 px       */
              CELL_SIZE - 2,                           /* height = 18 px       */
              color);
}
```

### The grid layout — dimensions and layers

```
Window (1200 × 460 px)
┌──────────────────────────────────────────────────────────────────────────┐
│  Header (HEADER_ROWS=3 cell-rows × CELL_SIZE=20 = 60 px tall)            │
│  SCORE:0                        SNAKE                          BEST:0    │
│  ──────────────────────────── green line ────────────────────────────── │ ← row 40px
├──────────────────────────────────────────────────────────────────────────┤ ← y=60px
│  Play field (GRID_HEIGHT=20 × CELL_SIZE=20 = 400 px tall)                │
│  GRID_WIDTH=60 columns × CELL_SIZE=20 = 1200 px wide                     │
│                                                                          │
│  each cell: 20×20 px visible as 18×18 px (1px gap all sides)            │
└──────────────────────────────────────────────────────────────────────────┘
```

Constants used:

| Constant | Value | Meaning |
|---|---|---|
| `GRID_WIDTH` | 60 | Cell columns in the play field |
| `GRID_HEIGHT` | 20 | Cell rows in the play field |
| `CELL_SIZE` | 20 | Pixels per cell side |
| `HEADER_ROWS` | 3 | Cell-rows reserved for the score panel |
| `WINDOW_WIDTH` | 1200 | `GRID_WIDTH * CELL_SIZE` |
| `WINDOW_HEIGHT` | 460 | `(GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE` |

`WINDOW_HEIGHT = (20 + 3) × 20 = 23 × 20 = 460 px`.

### The header separator line (`src/game.c → snake_render`)

```c
/* Background for header rows */
draw_rect(bb, 0, 0, bb->width, HEADER_ROWS * CELL_SIZE, COLOR_DARK_GRAY);

/* 2-pixel green separator between header and play field */
draw_rect(bb, 0, (HEADER_ROWS - 1) * CELL_SIZE, bb->width, 2, COLOR_GREEN);
```

`(HEADER_ROWS - 1) * CELL_SIZE = 2 * 20 = 40 px` — the separator sits at
pixel row 40, inside the header zone, 20 pixels above the play field.

---

## What To Notice

- **Clipping is the caller's job** — the canvas API clips silently; `draw_rect`
  clips explicitly.  Because the clamping runs *before* the loop, the inner
  loop body is a single memory write with no branches — very fast.

- **`field_y_offset` must be added everywhere** — if any code draws to a grid
  coordinate without this offset, cells appear 60 pixels too high, overlapping
  the header.  The encapsulation in `draw_cell` means only that one function
  needs to know about the offset.

- **`CELL_SIZE - 2` inset** — drawing 18×18 instead of 20×20 leaves a 1-pixel
  gap on every side.  Adjacent cells are visually separated even when they share
  a colour (e.g., a snake body that fills a row shows individual segments).

- **Alpha blending reads the destination** — `draw_rect_blend` reads each
  existing pixel before writing the blended result.  Call order matters: the
  snake must be rendered *before* the game-over overlay, or the overlay has
  nothing to blend over.

---

## Exercises

1. **Draw a full-window test gradient.**
   Temporarily replace the `draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK)`
   clear call in `snake_render()` with two nested loops that write
   `GAME_RGB(px * 255 / bb->width, py * 255 / bb->height, 128)` for every
   pixel.  Confirm a smooth gradient appears behind the game graphics.
   Remove the loops before moving on.

2. **Change the cell inset.**
   In `draw_cell()`, change both `CELL_SIZE - 2` arguments to `CELL_SIZE - 4`
   (2-pixel gap all sides) and rebuild.  Notice how the grid looks more spaced.
   Restore to `CELL_SIZE - 2` afterwards.

3. **Add a translucent pause overlay.**
   In `snake_render()`, after drawing everything else but while the game is
   in `GAME_PHASE_PLAYING`, call:
   ```c
   draw_rect_blend(bb, 0, HEADER_ROWS * CELL_SIZE,
                   WINDOW_WIDTH, GRID_HEIGHT * CELL_SIZE,
                   GAME_RGBA(0, 0, 180, 80));
   ```
   Observe how the blue tint overlays the play field while leaving the header
   unchanged.  Undo before the next lesson.
