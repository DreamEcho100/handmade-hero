# Lesson 02 — Pixel Format and Drawing Rectangles

## What you'll learn
- The three-tier alpha model: skip / opaque / blend
- Toroidal `draw_pixel_w`: wrapping pixels at screen edges for toroidal geometry
- Bresenham `draw_line` and its Asteroids use case
- Float RGBA vs packed `uint32_t` colors

---

## Color constants

Two color families:

```c
// Float colors — used with draw_rect() and draw_text()
// Expand to four args: r, g, b, a  (each 0.0–1.0)
#define COLOR_WHITE      1.0f, 1.0f, 1.0f, 1.0f
#define COLOR_RED        0.86f, 0.20f, 0.20f, 1.0f
#define COLOR_CYAN       0.0f, 0.85f, 0.85f, 1.0f

// Packed colors — used with draw_line() and draw_pixel_w()
// GAME_RGBA packs r,g,b,a bytes into a uint32_t
#define WF_WHITE   GAME_RGBA(255, 255, 255, 255)
#define WF_CYAN    GAME_RGBA(0,   220, 220, 255)
#define WF_YELLOW  GAME_RGBA(255, 215, 0,   255)
```

`COLOR_*` macros are multi-argument macros: `draw_rect(bb, x, y, w, h, COLOR_RED)`
expands to `draw_rect(bb, x, y, w, h, 0.86f, 0.20f, 0.20f, 1.0f)`.

---

## draw_rect — three-tier alpha

```c
// utils/draw-shapes.h
void draw_rect(Backbuffer *bb,
               float x, float y, float w, float h,
               float r, float g, float b, float a);
```

Three paths based on `a`:

| Alpha | Action | Use case |
|-------|--------|----------|
| ≤ 0.0f | Skip entirely | transparent overlays, debug |
| ≥ 1.0f | Write directly (`=`) | opaque fill — fastest path |
| in between | Alpha-blend (`src*a + dst*(1-a)`) | semi-transparent overlays |

```c
// Typical uses:
draw_rect(bb, 0.f, 0.f, (float)bb->width, (float)bb->height, 0,0,0, 1.f);  // clear
draw_rect(bb, 0.f, 0.f, (float)bb->width, (float)bb->height, 0,0,0, 0.5f); // dim overlay
draw_rect(bb, px, py, 8.f, 8.f, COLOR_WHITE);  // opaque white square
```

Note: `draw_rect` takes **pixel** coordinates — it is a pure rasterizer.
Coordinate conversion (game units → pixels) happens at the call site.

---

## draw_pixel_w — toroidal pixel write

```c
void draw_pixel_w(Backbuffer *bb, int x, int y, uint32_t color);
```

"Wrapping pixel write" — pixels that fall outside the canvas wrap to the
opposite edge rather than being clipped.  This is essential for wireframe
objects that straddle a screen boundary in toroidal space.

```c
// Implementation sketch
x = ((x % bb->width)  + bb->width)  % bb->width;
y = ((y % bb->height) + bb->height) % bb->height;
bb->pixels[y * (bb->pitch / 4) + x] = color;
```

The double-mod pattern handles negative values correctly.

---

## draw_line — Bresenham

```c
void draw_line(Backbuffer *bb, int x0, int y0, int x1, int y1, uint32_t color);
```

Bresenham's line algorithm steps along the major axis one pixel at a time
and decides whether to step in the minor axis based on accumulated error.
Uses `draw_pixel_w` internally so lines wrap at screen edges.

This is what draws the wireframe asteroids and ship: each edge of the polygon
is a `draw_line` call.

---

## Screen clear and dim overlay

```c
// Clear to black every frame (full screen):
draw_rect(bb, 0.f, 0.f, (float)bb->width, (float)bb->height,
          0.f, 0.f, 0.f, 1.f);

// Game-over dim (semi-transparent black over everything):
draw_rect(bb, 0.f, 0.f, (float)bb->width, (float)bb->height,
          0.f, 0.f, 0.f, 0.5f);
```

The dim overlay passes `a = 0.5f` which triggers the blend path:
`out = src * 0.5 + dst * 0.5` — every pixel becomes half as bright.

---

## Why float coords for draw_rect?

The game will eventually pass pixel positions computed from game units via
`world_x()` / `world_y()` (floats), so accepting `float` avoids a cast at
every call site and keeps sub-pixel positions possible for smooth animation.

Internally, `draw_rect` truncates to `int` via `(int)x` before writing pixels.

---

## Key takeaways

1. `draw_rect` takes float pixel coords and float RGBA colors.
2. Three alpha tiers: skip / opaque / blend — opaque is the fast path.
3. `draw_pixel_w` wraps at screen edges — critical for toroidal wireframes.
4. `draw_line` builds on `draw_pixel_w` so edges wrap automatically.
5. `COLOR_*` = float multi-arg macros; `WF_*` = packed `uint32_t` constants.
