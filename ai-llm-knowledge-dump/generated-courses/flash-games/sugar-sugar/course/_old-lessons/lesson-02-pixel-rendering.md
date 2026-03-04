# Lesson 02 — Pixel Rendering

## What you build

Colored rectangles, circles, and a fully composited frame rendered into the backbuffer each tick — the complete primitive drawing toolkit used throughout the game.

## Concepts introduced

- `0xAARRGGBB` pixel format and the bit-shift layout
- `GAME_RGB` / `GAME_RGBA` macros
- Little-endian byte order in memory
- Why X11 reads `0xAARRGGBB` correctly without a swap
- Why Raylib requires an R↔B swap (and where it happens)
- `draw_pixel` with bounds clamping
- `draw_rect` — the double loop and row-pointer optimisation
- `draw_circle` — the `dx²+dy²≤r²` rasterisation test
- `game_render` is read-only: `const GameState *state`
- Named `COLOR_*` constants

---

## JS → C analogy

| JavaScript / Canvas 2D                   | C equivalent                              |
|------------------------------------------|-------------------------------------------|
| `ctx.fillStyle = 'rgb(255,0,0)'`         | `GAME_RGB(255, 0, 0)` = `0xFFFF0000`     |
| `ctx.fillStyle = 'rgba(255,0,0,0.5)'`    | `GAME_RGBA(255, 0, 0, 128)` = `0x80FF0000` |
| `imageData.data[i*4+0]` (R byte)         | `(color >> 16) & 0xFF` (R from `0xAARRGGBB`) |
| `imageData.data[i*4+1]` (G byte)         | `(color >> 8) & 0xFF`                    |
| `imageData.data[i*4+2]` (B byte)         | `color & 0xFF`                           |
| `imageData.data[i*4+3]` (A byte)         | `(color >> 24) & 0xFF`                   |
| `ctx.fillRect(x, y, w, h)`               | `draw_rect(bb, x, y, w, h, color)`       |
| `ctx.arc(cx, cy, r, 0, Math.PI*2)` + fill | `draw_circle(bb, cx, cy, r, color)`    |
| `const img = { data: new Uint32Array() }` | `GameBackbuffer { uint32_t *pixels }`   |

---

## Step-by-step

### Step 1: Understand `0xAARRGGBB` bit layout

Every pixel in the `GameBackbuffer` is a 32-bit unsigned integer packed as:

```
Bit  31..24   23..16   15..8    7..0
     AAAAAAAA RRRRRRRR GGGGGGGG BBBBBBBB
```

The `GAME_RGBA` macro constructs this value:

```c
#define GAME_RGBA(r, g, b, a)                                       \
  (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) |                  \
   ((uint32_t)(g) << 8)  | (uint32_t)(b))
```

And `GAME_RGB` fills alpha with 255 (fully opaque):

```c
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 0xFF)
```

**Example — red:**

```c
GAME_RGB(255, 0, 0)
= GAME_RGBA(255, 0, 0, 0xFF)
= (0xFF << 24) | (0xFF << 16) | (0x00 << 8) | 0x00
= 0xFFFF0000
```

**Example — `COLOR_BG` (sky blue, R=135 G=195 B=225):**

```c
GAME_RGB(135, 195, 225)
= (0xFF << 24) | (0x87 << 16) | (0xC3 << 8) | 0xE1
= 0xFF87C3E1
```

### Step 2: How bytes sit in memory (little-endian)

On x86/x86-64 (little-endian), a `uint32_t` value is stored with the **least-significant byte first**:

```
Value:  0xFF87C3E1   (COLOR_BG)
Memory: [0xE1, 0xC3, 0x87, 0xFF]
          B     G     R     A
```

This is the byte order X11's `XCreateImage` reads. X11 interprets them directly as `[B, G, R, A]` (TrueColor visual on x86 Linux has `blue_mask = 0xFF`, `red_mask = 0xFF0000`) — so the display comes out with exactly the right color. No conversion needed.

### Step 3: Why Raylib needs an R↔B swap

Raylib uses `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` which expects bytes in memory as `[R, G, B, A]`. Our buffer stores them as `[B, G, R, A]`. Red and blue are swapped.

The `platform_display_backbuffer` in `src/main_raylib.c` fixes this with an in-place swap before uploading to the GPU, then swaps back so the game buffer remains in the canonical `0xAARRGGBB` format:

```c
/* Swap R and B channels: ARGB → ABGR (Raylib reads as RGBA ✓) */
for (int i = 0; i < total; i++) {
    uint32_t c = px[i];
    px[i] = (c & 0xFF00FF00u)          /* keep A and G in place */
          | ((c & 0x00FF0000u) >> 16)   /* move R to B position  */
          | ((c & 0x000000FFu) << 16);  /* move B to R position  */
}

UpdateTexture(g_texture, backbuffer->pixels);

/* Swap back so game state is unaffected */
for (int i = 0; i < total; i++) { /* identical swap is its own inverse */ ... }
```

**Key insight:** `game.c` never knows which backend is running. It always writes `0xAARRGGBB`. The platform layer absorbs the format difference.

### Step 4: `draw_pixel` — the building block

Every higher-level primitive calls `draw_pixel` internally:

```c
static inline void draw_pixel(GameBackbuffer *bb, int x, int y, uint32_t c) {
  if ((unsigned)x < (unsigned)bb->width && (unsigned)y < (unsigned)bb->height)
    bb->pixels[y * bb->width + x] = c;
}
```

Two things to notice:

1. **Bounds check via unsigned cast:** Casting `int` to `unsigned` before comparison is a common C trick. A negative signed integer becomes a huge unsigned number, which is always `>= bb->width`. So one comparison handles both `x < 0` and `x >= width` simultaneously.

2. **Index formula:** `y * bb->width + x` — **not** `x * bb->height + y`. Pixels are stored row by row (row-major). Row `y` starts at offset `y * width`. Pixel `x` within that row is at `+x`. This mirrors how a 2D array `pixels[y][x]` would be laid out.

### Step 5: `draw_rect` — the double loop

```c
static void draw_rect(GameBackbuffer *bb, int x, int y, int w, int h,
                      uint32_t color) {
  int x0 = x < 0 ? 0 : x;
  int y0 = y < 0 ? 0 : y;
  int x1 = (x + w) > bb->width  ? bb->width  : (x + w);
  int y1 = (y + h) > bb->height ? bb->height : (y + h);
  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * bb->width;
    for (int px = x0; px < x1; px++)
      row[px] = color;
  }
}
```

**Clipping:** Before the loop, `x0/y0/x1/y1` are clamped to the canvas boundaries. A rect that extends beyond the canvas is silently clipped — no crash, no wrap-around.

**Row pointer optimisation:** `uint32_t *row = bb->pixels + py * bb->width` hoists the row-start multiply out of the inner loop. The inner loop then just increments an index, which compiles to a single store instruction.

**Usage example from game.c** (drawing a cup's empty area):

```c
draw_rect(bb, cup->x + 1, cup->y, cup->w - 2, cup->h - 1, COLOR_CUP_EMPTY);
```

### Step 6: `draw_circle` — rasterisation with the circle equation

```c
static void draw_circle(GameBackbuffer *bb, int cx, int cy, int r,
                        uint32_t color) {
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++)
      if (dx * dx + dy * dy <= r * r)
        draw_pixel(bb, cx + dx, cy + dy, color);
}
```

This is the simplest correct circle fill: iterate over every pixel in the bounding square and paint it if it falls inside the circle equation `x² + y² ≤ r²`. No trigonometry, no floating-point — pure integer arithmetic.

**JS analogy:**

```js
// Canvas 2D equivalent
ctx.beginPath();
ctx.arc(cx, cy, r, 0, Math.PI * 2);
ctx.fillStyle = colorString;
ctx.fill();
```

The C version gives you exact pixel control at the cost of a O(r²) loop. For the small radii used in this game (r ≤ 10) this is negligible.

**Used in game.c for grain emitter nozzle:**

```c
draw_circle(bb, ex, ey, 5, COLOR_UI_TEXT);
```

And for portal rendering:

```c
draw_circle(bb, tp->ax, tp->ay, tp->radius, COLOR_PORTAL_A);
```

### Step 7: `game_render` is read-only

The signature is:

```c
void game_render(const GameState *state, GameBackbuffer *bb);
```

`state` is `const` — `game_render` promises **never** to modify the simulation. This enforces the clean separation between update (write) and render (read). If you accidentally tried to write `state->grains.x[i] = 0` inside a render function, the compiler would emit an error:

```
error: assignment of member 'x' in read-only object
```

This is a compile-time guarantee that rendering never causes physics side-effects.

### Step 8: Named `COLOR_*` constants

Rather than scattering magic numbers throughout render code, all colors are defined in `game.h`:

```c
#define COLOR_BG          GAME_RGB(135, 195, 225)  /* sky blue             */
#define COLOR_LINE        GAME_RGB(50,  50,  50)   /* player-drawn lines   */
#define COLOR_OBSTACLE    GAME_RGB(130, 110, 80)   /* level obstacles      */
#define COLOR_CUP_BORDER  GAME_RGB(60,  60,  100)  /* cup outline          */
#define COLOR_CUP_FILL    GAME_RGB(180, 210, 255)  /* cup fill progress    */
#define COLOR_CUP_FILL_FULL GAME_RGB(80, 200, 100) /* fully filled (green) */
#define COLOR_RED         GAME_RGB(229, 57,  53)   /* grain/filter red     */
#define COLOR_GREEN       GAME_RGB(67,  160, 71)   /* grain/filter green   */
#define COLOR_ORANGE      GAME_RGB(251, 140, 0)    /* grain/filter orange  */
#define COLOR_CREAM       GAME_RGB(232, 213, 183)  /* default white grain  */
```

These are `#define` constants, not `const int` variables — they are substituted by the preprocessor before compilation and incur zero runtime cost.

### Step 9: Render the grain layer

Each active grain is drawn as a 2×2 block in `game.c`'s grain render loop:

```c
for (int i = 0; i < p->count; i++) {
    if (!p->active[i]) continue;
    int x = (int)p->x[i];
    int y = (int)p->y[i];
    uint32_t c = g_grain_colors[p->color[i]];
    /* Draw a 2×2 block; draw_pixel clips against canvas bounds. */
    draw_pixel(bb, x,     y,     c);
    draw_pixel(bb, x + 1, y,     c);
    draw_pixel(bb, x,     y + 1, c);
    draw_pixel(bb, x + 1, y + 1, c);
}
```

The `g_grain_colors` lookup table maps `GRAIN_COLOR` enum values to pixel colors:

```c
static const uint32_t g_grain_colors[GRAIN_COLOR_COUNT] = {
    [GRAIN_WHITE]  = COLOR_CREAM,
    [GRAIN_RED]    = COLOR_RED,
    [GRAIN_GREEN]  = COLOR_GREEN,
    [GRAIN_ORANGE] = COLOR_ORANGE,
};
```

**Designated initializers** (`[GRAIN_WHITE] = ...`) are a C99 feature that lets you initialize array elements by index. Even if the enum values are reordered, the color mapping stays correct.

### Step 10: The full render call sequence

Each frame, `game_render` in `game.c` dispatches to a phase-specific renderer:

```c
void game_render(const GameState *state, GameBackbuffer *bb) {
  switch (state->phase) {
  case PHASE_TITLE:         render_title(state, bb);          break;
  case PHASE_PLAYING:       render_playing(state, bb);        break;
  case PHASE_LEVEL_COMPLETE: render_level_complete(state, bb); break;
  case PHASE_FREEPLAY:      render_freeplay(state, bb);       break;
  case PHASE_COUNT:         break;
  }
}
```

`render_playing` follows this order:
1. Clear to `COLOR_BG`
2. Draw `LineBitmap` pixels (player lines, obstacles, baked grains)
3. Draw `ColorFilter` zones (semi-transparent blend)
4. Draw `Teleporter` circles
5. Draw `Cup` boxes and fill indicators
6. Draw active `Grain` particles (2×2 blocks)
7. Draw `Emitter` nozzles
8. Draw UI buttons and score

---

## Common mistakes to prevent

1. **Swapping `x` and `y` in the index formula.**
   `pixels[x * height + y]` is **wrong**. The correct formula is `pixels[y * width + x]`. Pixels are stored row-first; `y` selects the row, `x` selects the column within it.

2. **Forgetting the `const` on `game_render`'s `state` parameter.**
   Without `const`, you might accidentally modify physics state inside a render function. Let the compiler enforce the read-only contract.

3. **Using `(int)` cast on a negative float before the index.**
   In C, `(int)-0.7f` is `0` (truncation toward zero), but `(int)-1.3f` is `-1`. If a grain position is slightly negative, `(int)y` can be `-1`, causing an out-of-bounds write. `draw_pixel`'s bounds check catches this — but it means the grain visually disappears one row early. Use `>= 0` guards before trusting the cast for physics.

4. **`GAME_RGB(b, g, r)` — swapping argument order.**
   The macro is `GAME_RGB(r, g, b)`. Passing B first makes colors appear as their channel-swapped complement. `GAME_RGB(0, 0, 255)` is blue; `GAME_RGB(255, 0, 0)` is red.

5. **Using `int` instead of `uint32_t` for pixel values.**
   Bit-shifting a signed `int` left by 24 bits is undefined behaviour in C if the value would overflow the signed representation. Always use `uint32_t` (or cast to it) when building pixel values.

6. **Drawing outside the clipping zone in `draw_rect`.**
   Always clip before the loop (compute `x0/x1/y0/y1` from canvas bounds). Relying on `draw_pixel`'s per-pixel check in a rect loop adds thousands of unnecessary comparisons per frame.

---

## What to run

```bash
./build-dev.sh
./sugar_x11
```

You should see the title screen with the level-select grid rendered using `draw_rect` for buttons, `draw_text` for labels, and `COLOR_BG` as the background. Switch to Raylib to confirm colors match:

```bash
./build-dev.sh --backend=raylib
./sugar_raylib
```

Both should show identical colors — proof that the R↔B swap in `main_raylib.c` is working correctly.

---

## Summary

Every visible pixel in Sugar Sugar flows through a handful of primitives: `draw_pixel` (single pixel, bounds-checked), `draw_rect` (filled rectangle, clipped), and `draw_circle` (filled disc, integer `dx²+dy²≤r²` test). All of them write `uint32_t` values in `0xAARRGGBB` format — alpha in the high byte, blue in the low byte — which lands in memory as `[B,G,R,A]` on little-endian x86, matching X11's native format exactly. Raylib's one-time R↔B swap before the GPU upload is the only platform-specific concession, and it lives entirely in `main_raylib.c` — `game.c` remains format-agnostic. The `const GameState *state` signature on `game_render` enforces at compile time that rendering is a pure read from game state, never a write.
