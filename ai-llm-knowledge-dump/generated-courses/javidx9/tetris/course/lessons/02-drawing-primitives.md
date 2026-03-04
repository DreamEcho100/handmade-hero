# Lesson 02: Drawing Primitives

## What we're building

A window showing a **5 × 5 grid of coloured cells** (a preview of the Tetris
field) with a **semi-transparent dark overlay** drawn on top of one column.
By the end of this lesson `draw_rect()` and `draw_rect_blend()` are fully
working — they are the only drawing functions the entire game uses.

## What you'll learn

- `#define` function-like macros (`MIN`, `MAX`) and why they exist
- Header guards (`#ifndef / #define / #endif`) — preventing double-inclusion
- Separation of declaration (`.h`) and implementation (`.c`)
- Passing a struct by pointer (`Backbuffer *bb`) to avoid copying
- Pointer arithmetic: `bb->pixels + y * (bb->pitch / 4)`
- Bit extraction: `(color >> 16) & 0xFF` to get a channel from a packed pixel
- Alpha blending: `out = (src * a + dst * (255 - a)) / 255`

## Prerequisites

Lesson 01 — Window & Backbuffer

---

## Step 1: `utils/math.h` — MIN, MAX, CLAMP

Create `src/utils/math.h`.

These three macros are used inside `draw-shapes.c` for clipping rectangles to
the buffer bounds, and later throughout the game.

```c
/* src/utils/math.h */
#ifndef UTILS_MATH_H
#define UTILS_MATH_H

/*
 * Header guards — why are they needed?
 *
 * JS: ES modules are evaluated once no matter how many times you import them.
 * C:  #include is pure text substitution. If two files both include math.h
 *     and they include each other, the struct/macro definitions get pasted
 *     twice and the compiler errors: "redefinition of ...".
 *
 * The guard pattern:
 *   #ifndef UTILS_MATH_H   — "if UTILS_MATH_H is NOT defined yet ..."
 *   #define UTILS_MATH_H   — "... define it now (mark as visited) ..."
 *   ... all your code ...
 *   #endif                 — "... end of the guarded block"
 *
 * Second time this file is included, UTILS_MATH_H is already defined, so the
 * preprocessor skips the entire file. Problem solved.
 *
 * UNIQUE name: use the file path in the macro name to avoid clashes with
 * other headers that might accidentally pick the same guard name.
 */

/*
 * MIN — return the smaller of two values.
 *
 * JS: Math.min(a, b)
 * C:  MIN(a, b)
 *
 * WHY a macro and not a function?
 *   A C function `int min(int a, int b)` only works for int.
 *   You'd need separate versions for float, double, unsigned, etc.
 *   A macro does text substitution before compilation, so the type is
 *   inferred from whatever you pass in — it works for any numeric type.
 *
 * WHY wrap each parameter in parentheses?
 *   Macros are text substitution. Without the parens, MIN(x & mask, y)
 *   expands to:  (x & mask < y ? x & mask : y)
 *   Because & has lower precedence than <, this is parsed as:
 *       x & (mask < y) ? ...  ← WRONG!
 *   With parens: ((x & mask) < (y) ? (x & mask) : (y)) ← correct.
 *
 * DOUBLE-EVALUATION HAZARD:
 *   MIN(++i, j) expands to ((++i) < (j) ? (++i) : (j))
 *   If ++i < j, i is incremented TWICE. Never pass expressions with side
 *   effects (++, --, function calls) to these macros. Use a temp variable:
 *     int tmp = expensive_call();
 *     int r   = MIN(tmp, limit);
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* MAX — return the larger of two values.
 * JS: Math.max(a, b)  */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* CLAMP — clamp val to the inclusive range [lo, hi].
 * JS: Math.min(Math.max(val, lo), hi)  */
#define CLAMP(val, lo, hi) (MAX((lo), MIN((hi), (val))))

#endif /* UTILS_MATH_H */
```

> **Why?** `Math.min` in JS is a built-in function that works with any number.
> C's standard library only has `fmin` (for doubles). For integers there's
> nothing. These macros fill the gap with zero runtime overhead — the compiler
> sees the expanded ternary expression directly.

---

## Step 2: `utils/draw-shapes.h` — declarations

Create `src/utils/draw-shapes.h`. This file only *declares* the two drawing
functions — it tells the compiler "these functions exist and have these
signatures". The implementations live in `draw-shapes.c`.

```c
/* src/utils/draw-shapes.h */
#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H

/*
 * Including backbuffer.h here means any file that includes draw-shapes.h
 * automatically gets the Backbuffer type too. No double-include risk because
 * backbuffer.h has its own guard.
 *
 * JS: like re-exporting a type from an index file:
 *   export { Backbuffer } from './backbuffer';
 */
#include "backbuffer.h"
#include <stdint.h>  /* uint32_t */

/*
 * Function declarations (also called "prototypes").
 *
 * JS/TS equivalent:
 *   declare function drawRect(
 *     bb: Backbuffer, x: number, y: number,
 *     w: number, h: number, color: number
 *   ): void;
 *
 * C: void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);
 *
 * A declaration gives the compiler the function's NAME, PARAMETER TYPES, and
 * RETURN TYPE so it can typecheck call sites. The body (implementation) is in
 * draw-shapes.c and linked in at compile time.
 *
 * WHY Backbuffer *bb (pointer) instead of Backbuffer bb (value)?
 *   In C, passing a struct by VALUE copies every byte of it onto the stack.
 *   Backbuffer contains a pointer field (bb->pixels), so copying the struct
 *   doesn't copy the pixel array — but it does copy the metadata fields
 *   unnecessarily. More importantly, if draw_rect modified bb->width inside,
 *   the caller would not see the change (it got a copy).
 *   Passing a POINTER means the function operates on the caller's original
 *   struct. Zero copy overhead, changes are visible to the caller.
 *
 * JS objects are ALWAYS passed by reference — there is no such distinction.
 * In C you choose explicitly with * (pointer) or no * (value copy).
 */

/*
 * draw_rect — draw a solid filled rectangle.
 *
 * Parameters:
 *   bb    — pointer to the Backbuffer to draw into
 *   x, y  — top-left corner (screen pixels; (0,0) = top-left of window)
 *   w, h  — width and height in pixels
 *   color — packed ARGB from GAME_RGB() or GAME_RGBA(); alpha is IGNORED
 *
 * Clipping: coordinates outside [0, bb->width) × [0, bb->height) are
 * silently clamped — safe to pass negative or out-of-bounds values.
 *
 * JS: ctx.fillRect(x, y, w, h) with ctx.globalAlpha = 1
 */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/*
 * draw_rect_blend — draw a rectangle with alpha blending.
 *
 * Same as draw_rect but reads the alpha channel from `color` and blends each
 * pixel with the destination:
 *
 *   out = (src * alpha + dst * (255 - alpha)) / 255
 *
 * Special cases (optimised):
 *   alpha == 255 → delegates to draw_rect (no blending needed)
 *   alpha ==   0 → returns immediately (fully transparent)
 *
 * JS: ctx.globalAlpha = color.a / 255; ctx.fillRect(x, y, w, h);
 */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h,
                     uint32_t color);

#endif /* UTILS_DRAW_SHAPES_H */
```

---

## Step 3: `utils/draw-shapes.c` — implementations

Create `src/utils/draw-shapes.c`. This file has the actual pixel-writing loops.
Study it carefully — pointer arithmetic, bit manipulation, and the alpha blend
formula are all explained in the comments.

```c
/* src/utils/draw-shapes.c */

/*
 * We include our own header first. This ensures the declaration in the
 * header matches the definition here — the compiler will catch any mismatch.
 *
 * JS: no direct equivalent. TS checks this automatically.
 */
#include "draw-shapes.h"
#include "backbuffer.h"
#include "math.h"

/* ════════════════════════════════════════════════════════════════════════════
 * draw_rect — solid filled rectangle
 * ════════════════════════════════════════════════════════════════════════════
 */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {

  /* ── Step 1: Clip ───────────────────────────────────────────────────────
   *
   * C does NOT do automatic array bounds checking. If we write to
   * bb->pixels[y * pitch + x] with a negative y, we are writing to memory
   * BEFORE the pixel array. In the best case another variable is corrupted;
   * in the worst case the OS kills the process (segfault).
   *
   * We compute inclusive start (x0,y0) and exclusive end (x1,y1):
   *   x0 = MAX(x, 0)         — clamp left edge to the buffer left
   *   x1 = MIN(x+w, bb->width) — clamp right edge to the buffer right
   *   (same for y/height)
   *
   * If x0 >= x1 (or y0 >= y1) the rectangle is entirely off-screen; the
   * loops simply don't execute.
   *
   * JS: Canvas clips silently. In C we must do it ourselves.
   */
  int x0 = MAX(x, 0);
  int y0 = MAX(y, 0);
  int x1 = MIN(x + w, bb->width);   /* exclusive end column */
  int y1 = MIN(y + h, bb->height);  /* exclusive end row    */

  /* ── Step 2: Pixel loop ─────────────────────────────────────────────────
   *
   * Outer loop: rows (py = pixel-y).
   */
  for (int py = y0; py < y1; py++) {

    /*
     * Compute a pointer to the first uint32_t in row py.
     *
     * bb->pixels is uint32_t* — pointer to the very first pixel.
     * Adding an integer N advances by N * sizeof(uint32_t) bytes (4 bytes).
     * This is C pointer arithmetic: the step size is the element size.
     *
     * py * (bb->pitch / 4):
     *   pitch is in BYTES (e.g. 2240 for 560-pixel-wide, 4-bpp buffer).
     *   We divide by 4 to get the number of uint32_t steps per row.
     *   If pitch = width * 4 (no padding), this equals py * width.
     *
     * JS: const rowStart = py * (bb.pitch / 4);  pixels[rowStart + px]
     * C:  uint32_t *row = bb->pixels + py * (bb->pitch / 4);  row[px]
     *
     * (see `src/utils/draw-shapes.c:55–73`)
     */
    uint32_t *row = bb->pixels + py * (bb->pitch / 4);

    /* Inner loop: columns. */
    for (int px = x0; px < x1; px++) {
      /*
       * row[px] is the same as *(row + px).
       * Assign the color directly — this is a simple overwrite, no blending.
       *
       * JS: pixels[py * width + px] = color;
       */
      row[px] = color;
    }
  }
}

/* ════════════════════════════════════════════════════════════════════════════
 * draw_rect_blend — rectangle with alpha blending
 * ════════════════════════════════════════════════════════════════════════════
 */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h,
                     uint32_t color) {

  /* ── Extract alpha from the packed color ────────────────────────────────
   *
   * Our color is packed as 0xAARRGGBB (see GAME_RGBA in backbuffer.h).
   * Alpha lives in bits 31-24.
   *
   * To extract it:
   *   1. Shift right 24: moves bits 31-24 down to bits 7-0.
   *   2. Mask with 0xFF: keep only the lowest 8 bits.
   *
   *   Example: color = 0x80FF0000 (50% alpha, red)
   *   >> 24  →  0x00000080  (128)
   *   & 0xFF →  0x80        (128)
   *
   * JS: const alpha = (color >>> 24) & 0xFF;   (>>> = unsigned right shift)
   * C:  uint8_t alpha = (color >> 24) & 0xFF;
   *
   * uint8_t ensures the result fits in one byte (0–255).
   *
   * (see `src/utils/draw-shapes.c:108`)
   */
  uint8_t alpha = (color >> 24) & 0xFF;

  /* Optimisation: fully opaque — skip blending, just overwrite. */
  if (alpha == 255) {
    draw_rect(bb, x, y, w, h, color);
    return;
  }

  /* Optimisation: fully transparent — nothing to draw. */
  if (alpha == 0) return;

  /* ── Extract source RGB channels ────────────────────────────────────────
   *
   * Same shift-and-mask pattern for red (bits 23-16), green (15-8), blue (7-0).
   *
   * JS: const srcR = (color >>> 16) & 0xFF;
   *     const srcG = (color >>>  8) & 0xFF;
   *     const srcB =  color         & 0xFF;
   */
  uint8_t src_r = (color >> 16) & 0xFF;
  uint8_t src_g = (color >>  8) & 0xFF;
  uint8_t src_b =  color        & 0xFF;

  /* Clip — same as draw_rect (see comments there). */
  int x0 = MAX(x, 0);
  int y0 = MAX(y, 0);
  int x1 = MIN(x + w, bb->width);
  int y1 = MIN(y + h, bb->height);

  /* ── Blend loop ─────────────────────────────────────────────────────────
   *
   * For each pixel: read destination, blend with source, write back.
   */
  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * (bb->pitch / 4);

    for (int px = x0; px < x1; px++) {
      /* Read existing (destination) pixel. */
      uint32_t dst = row[px];

      /* Extract destination RGB channels. */
      uint8_t dst_r = (dst >> 16) & 0xFF;
      uint8_t dst_g = (dst >>  8) & 0xFF;
      uint8_t dst_b =  dst        & 0xFF;

      /*
       * Alpha blend formula (Porter-Duff "source over"):
       *
       *   out = (src * alpha + dst * (255 - alpha)) / 255
       *
       * HOW IT WORKS:
       *   alpha=255 → out = src*255/255 = src  (fully opaque: source wins)
       *   alpha=0   → out = dst*255/255 = dst  (fully transparent: dest wins)
       *   alpha=128 → out ≈ 0.5*src + 0.5*dst (50/50 blend)
       *
       * WHY divide by 255 (not 256)?
       *   We work in 0–255 range. 255*255/255 = 255, correct.
       *   255*255/256 = 254 — slightly wrong (fully opaque looks darker).
       *
       * NOTE: src_r * alpha can be up to 255*255 = 65025 — does NOT fit in
       * uint8_t (max 255). C automatically "promotes" uint8_t operands to
       * int in arithmetic, so the intermediate result is computed as int.
       * The final (uint8_t) cast truncates back to 0-255 after the division.
       *
       * JS equivalent:
       *   const outR = Math.floor((srcR * a + dstR * (255-a)) / 255);
       *
       * (see `src/utils/draw-shapes.c:185–187`)
       */
      uint8_t out_r = (uint8_t)((src_r * alpha + dst_r * (255 - alpha)) / 255);
      uint8_t out_g = (uint8_t)((src_g * alpha + dst_g * (255 - alpha)) / 255);
      uint8_t out_b = (uint8_t)((src_b * alpha + dst_b * (255 - alpha)) / 255);

      /*
       * Pack blended channels back to uint32_t. Alpha is forced to 255
       * (fully opaque) because we've already applied the transparency —
       * the stored pixel is always solid.
       */
      row[px] = GAME_RGB(out_r, out_g, out_b);
    }
  }
}
```

> **Why `(color >> 16) & 0xFF`?** This is the standard "shift then mask"
> pattern to extract one byte from a packed integer.
>
> In JS you'd write `(color >>> 16) & 0xFF` — the `>>>` is an *unsigned*
> right shift, needed because JS integers are 32-bit signed two's complement.
> In C, `color` is already `uint32_t` (unsigned), so plain `>>` is safe.
> The `& 0xFF` mask is needed in both languages to discard any high bits left
> over from the shift.

---

## Step 4: Updated `main_raylib.c` — draw the demo grid

Replace `src/main_raylib.c` with this version. It draws a 5 × 5 grid of
coloured squares and one semi-transparent black overlay to show blending.

```c
/* src/main_raylib.c — Lesson 02 complete */
#include "utils/backbuffer.h"
#include "utils/draw-shapes.h"   /* draw_rect, draw_rect_blend */

#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>

#define FIELD_WIDTH   12
#define FIELD_HEIGHT  18
#define CELL_SIZE     30
#define SIDEBAR_WIDTH 200

/*
 * A small palette of colours for the demo grid.
 *
 * GAME_RGB(r, g, b) packs R, G, B into a uint32_t with alpha = 255.
 * We define them as local constants for readability.
 */
#define COLOR_CYAN    GAME_RGB(0,   255, 255)
#define COLOR_BLUE    GAME_RGB(0,   0,   255)
#define COLOR_ORANGE  GAME_RGB(255, 165, 0  )
#define COLOR_YELLOW  GAME_RGB(255, 255, 0  )
#define COLOR_GREEN   GAME_RGB(0,   255, 0  )

int main(void) {
  int screen_width  = (FIELD_WIDTH  * CELL_SIZE) + SIDEBAR_WIDTH;
  int screen_height =  FIELD_HEIGHT * CELL_SIZE;

  /* --- Backbuffer allocation (same as Lesson 01) --- */
  Backbuffer bb = {0};
  bb.width           = screen_width;
  bb.height          = screen_height;
  bb.bytes_per_pixel = 4;
  bb.pitch           = screen_width * 4;
  bb.pixels = (uint32_t *)malloc(
      (size_t)screen_width * (size_t)screen_height * sizeof(uint32_t)
  );
  if (!bb.pixels) {
    fprintf(stderr, "ERROR: could not allocate pixel buffer\n");
    return 1;
  }

  InitWindow(screen_width, screen_height, "Tetris - Lesson 02");
  SetTargetFPS(60);

  Image img = {
    .data    = bb.pixels,
    .width   = bb.width,
    .height  = bb.height,
    .mipmaps = 1,
    .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  Texture2D texture = LoadTextureFromImage(img);

  /*
   * Palette for our 5-colour demo grid.
   *
   * C arrays: uint32_t palette[5] declares an array of 5 uint32_t values
   * on the stack.  Array elements are accessed with palette[i].
   *
   * JS: const palette: number[] = [COLOR_CYAN, ...];
   */
  uint32_t palette[5] = {
    COLOR_CYAN,
    COLOR_BLUE,
    COLOR_ORANGE,
    COLOR_YELLOW,
    COLOR_GREEN,
  };

  while (!WindowShouldClose()) {
    /* ── Fill background ──────────────────────────────────────────────── */
    draw_rect(&bb, 0, 0, screen_width, screen_height, COLOR_DARK_BLUE);

    /* ── Draw a 5 × 5 grid of coloured cells ────────────────────────────
     *
     * CELL_SIZE pixels per cell, 2-pixel gap between cells.
     *
     * Outer loop: row (0..4)
     * Inner loop: column (0..4)
     * Color: palette[col % 5] cycles through the 5 colours
     */
    int cell  = CELL_SIZE;   /* alias for readability */
    int gap   = 2;
    int start_x = 20;        /* left offset */
    int start_y = 20;        /* top offset  */

    for (int row = 0; row < 5; row++) {
      for (int col = 0; col < 5; col++) {
        int px = start_x + col * (cell + gap);
        int py = start_y + row * (cell + gap);
        draw_rect(&bb, px, py, cell, cell, palette[col % 5]);
      }
    }

    /* ── Semi-transparent overlay on the first column ───────────────────
     *
     * GAME_RGBA(0, 0, 0, 128) = black at 50% alpha.
     * draw_rect_blend blends this with whatever is underneath.
     *
     * Notice: we pass &bb (pointer to bb), not bb (value copy).
     *
     * JS: ctx.globalAlpha = 0.5; ctx.fillStyle = 'black'; ctx.fillRect(...)
     */
    draw_rect_blend(
      &bb,
      start_x, start_y,          /* x, y: top-left of overlay */
      cell, 5 * (cell + gap),     /* w, h: one column wide, 5 rows tall */
      GAME_RGBA(0, 0, 0, 128)     /* 50% transparent black */
    );

    /* ── Upload & display ──────────────────────────────────────────────── */
    UpdateTexture(texture, bb.pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(texture, 0, 0, WHITE);
    EndDrawing();
  }

  UnloadTexture(texture);
  CloseWindow();
  free(bb.pixels);
  bb.pixels = NULL;

  return 0;
}
```

> **Why `draw_rect(&bb, ...)` with `&`?** The `&` operator takes the *address*
> of a variable — it produces a pointer. `draw_rect` expects `Backbuffer *bb`
> (a pointer), not `Backbuffer bb` (a value copy). In JS you would just pass
> the object (`drawRect(bb, ...)`) because JS objects are always references.
> In C you explicitly control whether you pass by value or by pointer.

> **Handmade Hero principle:** `draw_rect` and `draw_rect_blend` know nothing
> about Raylib, OpenGL, or the OS. They take a `Backbuffer *` and write
> `uint32_t` values into it. This is the entire rendering model: fill a plain
> byte array, then upload it once per frame. The game layer never calls any
> graphics API — that's the platform layer's job.

---

## Build and run

```bash
# From the src/ directory — note: draw-shapes.c is now a second source file
clang -I. main_raylib.c utils/draw-shapes.c -lraylib -lm -o tetris
./tetris
```

> **Why list `utils/draw-shapes.c` explicitly?** In C there is no automatic
> module system. The compiler needs to know about every `.c` file that contains
> code you use. `draw-shapes.h` gave the compiler the *declarations* (function
> signatures); `draw-shapes.c` contains the *definitions* (actual code). You
> link them together by listing both `.c` files on the command line.
>
> JS equivalent: Node.js resolves `require()` paths automatically. C needs you
> to be explicit — think of it as running
> `node main.js utils/draw-shapes.js` (if Node worked that way).

---

## Expected result

- Background: dark blue-black field
- A 5 × 5 grid of cyan/blue/orange/yellow/green squares (28 × 28 px each)
- The leftmost column of the grid is visibly darker — the 50% black overlay
  blended on top of it

---

## Common mistakes

> **Common mistake:**
> ```c
> draw_rect(bb, 0, 0, 10, 10, COLOR_CYAN);  /* no & */
> ```
> This fails because: `draw_rect` expects `Backbuffer *` (a pointer). Passing
> `bb` (a struct value) is a type error — the compiler will report
> "passing `Backbuffer` to parameter of type `Backbuffer *`".
>
> **Correct version:** `draw_rect(&bb, 0, 0, 10, 10, COLOR_CYAN);`

---

> **Common mistake:**
> ```c
> /* In draw_rect: */
> uint32_t *row = bb->pixels + py * bb->width;
> ```
> This fails because: `bb->width` is in pixels, but if `bb->pitch` is
> larger than `bb->width * 4` (row padding exists), this row pointer will be
> wrong — each row will be computed at the wrong offset, causing visual
> corruption or a crash.
>
> **Correct version:** `uint32_t *row = bb->pixels + py * (bb->pitch / 4);`
> We use `pitch / 4` because pitch is in *bytes* and `bb->pixels` steps in
> *uint32_t* units (4 bytes each).

---

> **Common mistake:**
> ```c
> uint8_t out_r = (src_r * alpha + dst_r * (255 - alpha)) / 255;
> ```
> This *appears* to work but `uint8_t` arithmetic wraps at 255. The expression
> `src_r * alpha` can be up to 255 × 255 = 65025, which overflows `uint8_t`.
> C will silently promote `uint8_t` operands to `int` in arithmetic, so this
> particular case actually works — but the explicit `(uint8_t)` cast on the
> *result* makes the intent clear and documents that you know the final value
> fits in a byte.
>
> **Correct version:**
> ```c
> uint8_t out_r = (uint8_t)((src_r * alpha + dst_r * (255 - alpha)) / 255);
> ```

---

## Exercises

1. **Vary the alpha.** Change the overlay alpha from 128 to each of: 0, 64,
   192, 255. Describe what you see. For alpha = 255, does the code still call
   the blend loop or does it take the fast path? (Hint: look at the `if
   (alpha == 255)` branch in `draw_rect_blend`.)

2. **Draw a border.** Draw the Tetris field outline — a rectangle using only
   the field dimensions:
   ```c
   /* field outline: no fill, just a 2-pixel border */
   draw_rect(&bb, 0, 0, FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE,
             COLOR_GRAY);
   draw_rect(&bb, 2, 2,
             FIELD_WIDTH * CELL_SIZE - 4, FIELD_HEIGHT * CELL_SIZE - 4,
             COLOR_DARK_BLUE);
   ```
   This is the "hollow rectangle" trick: draw a solid rectangle, then draw a
   smaller solid rectangle on top.

3. **Trace the blend formula by hand.** Given:
   - `src = GAME_RGBA(255, 0, 0, 128)` (50% red)
   - `dst = GAME_RGB(0, 0, 255)` (fully opaque blue)
   - `alpha = 128`
   Compute `out_r`, `out_g`, `out_b` manually using
   `out = (src * alpha + dst * (255 - alpha)) / 255`.
   What colour do you expect to see?

---

## What's next

In **Lesson 03** we wire up keyboard input. You'll learn about C `union`s
(two views of the same memory), the `do { } while(0)` macro pattern, and why
we track *transitions* (how many times a key changed state) rather than just
whether it's currently held. A coloured square will move around the screen
with arrow keys.
