# Lesson 03 — Line Drawing (Bresenham's Algorithm)

## What you'll build
`draw_line` using Bresenham's integer line algorithm, and `draw_pixel_w` — a
toroidal pixel writer that wraps around screen edges.  You'll draw a triangle
from three `draw_line` calls, then see a line cross the screen edge and appear
from the other side.

---

## Core Concepts

### 1. Why Integer Line Drawing?

Floating-point line drawing (lerp + round each pixel) works but introduces
rounding artefacts and is slower.  Bresenham's algorithm draws a pixel-perfect
line using only **integer addition and comparison** — no multiplication, no
division, no floating-point.

The key insight: instead of computing exact positions, track an **accumulated
error** that tells you when to step the minor axis.

### 2. Bresenham's Algorithm

```c
void draw_line(AsteroidsBackbuffer *bb,
               int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);  // abs(dx)
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);  // abs(dy)
    int sx = (x0 < x1) ? 1 : -1;   // step direction in x
    int sy = (y0 < y1) ? 1 : -1;   // step direction in y
    int err = dx - dy;              // initial error

    for (;;) {
        draw_pixel_w(bb, x0, y0, color);          // write pixel
        if (x0 == x1 && y0 == y1) break;          // reached the end
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }   // step x
        if (e2 <  dx) { err += dx; y0 += sy; }   // step y
    }
}
```

**Trace through a simple example** (`x0=0,y0=0` to `x1=4,y1=2`):

| Iteration | x  | y  | err  | e2   | Step |
|-----------|----|----|------|------|------|
| 0         | 0  | 0  | 2    | 4    | x    |
| 1         | 1  | 0  | 0    | 0    | x+y  |
| 2         | 2  | 1  | -2   | -4   | y    |
| 3         | 2  | 2  | 2    | 4    | x    |
| 4         | 3  | 2  | 0    | 0    | x (end) |

Each row of the table advances `(x0,y0)` one pixel closer to `(x1,y1)`.

### 3. `draw_pixel_w` — Toroidal Pixel Write

Asteroids objects drift off one edge and reappear from the other.  For lines
to cross screen edges seamlessly, `draw_line` calls `draw_pixel_w` instead
of `draw_pixel`:

```c
void draw_pixel_w(AsteroidsBackbuffer *bb, int x, int y, uint32_t color) {
    // Double-mod handles negative x correctly:
    //   e.g. x=-1, width=512 → ((-1 % 512) + 512) % 512 = 511
    x = ((x % bb->width)  + bb->width)  % bb->width;
    y = ((y % bb->height) + bb->height) % bb->height;
    bb->pixels[y * (bb->pitch / 4) + x] = color;
}
```

**Why the double-mod?**  
In C, `x % w` with a negative `x` gives a negative result.  Adding `w` first
and then taking `% w` again maps any negative value into `[0, w-1]`.

```
// JS:  ((x % w) + w) % w  — same formula, same reason
```

### 4. Drawing a Triangle

Three calls to `draw_line` — connect three points in order, close by
connecting the last back to the first:

```c
int x0 = 100, y0 = 50;   // top
int x1 = 70,  y1 = 130;  // bottom-left
int x2 = 130, y2 = 130;  // bottom-right
uint32_t col = GAME_RGBA(255, 255, 255, 255);

draw_line(bb, x0, y0, x1, y1, col);   // left side
draw_line(bb, x1, y1, x2, y2, col);   // bottom
draw_line(bb, x2, y2, x0, y0, col);   // right side
```

This is exactly how wireframe polygons will be drawn in later lessons.

---

## Experiment: Toroidal Wrap Demo

```c
// Draw a horizontal line starting near the right edge
draw_line(bb, SCREEN_W - 30, 240, SCREEN_W + 40, 240, COLOR_YELLOW);
```

With `draw_pixel_w`, the right segment wraps and appears from the left edge.
With `draw_pixel` (clipped), it would simply stop at `SCREEN_W - 1`.

---

## Verify

```bash
./build-dev.sh -r
```

You should see:
1. White triangle in the upper-left area
2. A yellow horizontal line that exits the right edge and continues from
   the left edge

---

## Summary

| Concept | C | JS equivalent |
|---------|---|---------------|
| Integer line drawing | Bresenham error accumulation | Canvas `lineTo` (float, slower) |
| Toroidal wrap | `((x % w) + w) % w` | Same formula |
| No floating point | `int dx, dy, err` | — (JS is always float) |
