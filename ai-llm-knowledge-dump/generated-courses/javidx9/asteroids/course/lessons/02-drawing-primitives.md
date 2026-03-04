# Lesson 02 — Drawing Primitives

## What you'll build
Functions to write pixels, fill rectangles, and alpha-blend a semi-transparent
overlay.  You'll draw coloured rectangles on screen and use the pure-red test
to confirm GAME_RGBA byte ordering works on both backends.

---

## Core Concepts

### 1. Pixel Index Formula

```c
// Row-major layout: rows are contiguous in memory
// y rows down → skip y * (pitch/4) elements
// x columns across → add x
bb->pixels[y * (bb->pitch / 4) + x] = color;

// JS equivalent (Uint8ClampedArray):
// const i = (y * width + x) * 4;
// data[i]=r; data[i+1]=g; data[i+2]=b; data[i+3]=a;
```

`pitch / 4` converts byte-stride to `uint32_t` element stride.  Since our
backbuffer has `pitch = width * 4`, `pitch/4 == width`.  We keep the formula
explicit for GPU-alignment safety.

### 2. `draw_pixel` — Clipped Write

```c
void draw_pixel(AsteroidsBackbuffer *bb, int x, int y, uint32_t color) {
    if (x < 0 || x >= bb->width || y < 0 || y >= bb->height) return;
    bb->pixels[y * (bb->pitch / 4) + x] = color;
}
```

Silent no-op for out-of-bounds.  No crash, no wrap — just skip.

### 3. `draw_rect` — Filled Rectangle

```c
void draw_rect(AsteroidsBackbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > bb->width  ? bb->width  : (x + w);
    int y1 = (y + h) > bb->height ? bb->height : (y + h);
    int stride = bb->pitch / 4;
    for (int py = y0; py < y1; py++)
        for (int px = x0; px < x1; px++)
            bb->pixels[py * stride + px] = color;
}
```

Clamp both ends of the rectangle to avoid writing outside the buffer.

### 4. `draw_rect_blend` — Alpha Compositing

The "over" compositing formula (Porter-Duff):
```
result = src * (alpha/255)  +  dst * (1 - alpha/255)
```

In C with integer arithmetic:
```c
// For each component (using R as example):
uint8_t sr = (color >> 0) & 0xFF;   // R is in LOW byte
uint8_t sa = (color >> 24) & 0xFF;  // A is in HIGH byte
uint8_t dr = (dst >> 0) & 0xFF;
uint8_t rr = (uint8_t)((sr * sa + dr * (255 - sa) + 127) / 255);
```

The `+127` rounds to the nearest integer instead of always truncating
downward, giving more accurate alpha blending.

**Channel extraction summary (GAME_RGBA = 0xAABBGGRR):**

| Channel | Bits   | Extract                     |
|---------|--------|-----------------------------|
| R       | 0–7    | `(color >> 0) & 0xFF`       |
| G       | 8–15   | `(color >> 8) & 0xFF`       |
| B       | 16–23  | `(color >> 16) & 0xFF`      |
| A       | 24–31  | `(color >> 24) & 0xFF`      |

> **Tip:** When R is in the low byte, `0x000000FF` is pure red — easy to
> confirm visually.

---

## The Pure-Red Test

Add this to your render loop to confirm both backends show identical colours:

```c
// Full-screen deep red fill
draw_rect(bb, 0, 0, SCREEN_W, SCREEN_H, GAME_RGBA(200, 0, 0, 255));

// Bright white square in the corner
draw_rect(bb, 10, 10, 50, 50, GAME_RGBA(255, 255, 255, 255));

// Semi-transparent dark overlay over right half
draw_rect_blend(bb, SCREEN_W/2, 0, SCREEN_W/2, SCREEN_H,
                GAME_RGBA(0, 0, 0, 128));
```

Expected result on **both X11 and Raylib**:
- Left half: solid dark red
- Right half: darker red (50% alpha overlay)
- White square in top-left corner

If left=red and right=blue on one backend, the `GL_RGBA` vs `GL_BGRA` flag
is wrong — a lesson from the Snake course.

---

## Verify

```bash
./build-dev.sh --backend=raylib -r   # both should look identical
./build-dev.sh --backend=x11 -r
```

---

## Summary

| Concept | C function | JS equivalent |
|---------|-----------|---------------|
| Write one pixel | `draw_pixel` | `imageData.data[i] = ...` |
| Fill rectangle | `draw_rect` | `ctx.fillRect(x, y, w, h)` |
| Alpha blend | `draw_rect_blend` | `ctx.globalAlpha = 0.5` |
| Colour channels | `(color >> 8) & 0xFF` | `(color >>> 8) & 0xFF` |
