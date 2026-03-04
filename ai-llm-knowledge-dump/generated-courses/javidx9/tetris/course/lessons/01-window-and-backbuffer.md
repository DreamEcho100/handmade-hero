# Lesson 01: Window & Backbuffer

## What we're building

A minimal Raylib program that opens a window at the correct Tetris resolution
(360 × 540 field + 200 px sidebar = **560 × 540**), allocates a flat array of
pixels on the heap, fills every pixel with a dark colour, and displays it at
60 fps. At the end of this lesson you have a blank canvas — every later lesson
just draws things on top of it.

## What you'll learn

- `#include` — importing declarations (like JS `import`)
- `#define` — compile-time constants and macros
- `typedef struct` — grouping related fields (like a JS plain object)
- `uint32_t` — fixed-width integer types from `<stdint.h>`
- `malloc` / `free` — explicit heap allocation (no garbage collector)
- Checking `NULL` — what happens when allocation fails
- `for` loop with pointer arithmetic — writing into a flat array
- The backbuffer pipeline: CPU writes pixels → `UpdateTexture` → GPU displays

## Prerequisites

None — this is the first lesson.

---

## Step 1: The backbuffer header

Create the file `src/utils/backbuffer.h`.

This header defines one struct (`Backbuffer`) and two macros (`GAME_RGB`,
`GAME_RGBA`). The struct is just a bundle of fields describing a flat pixel
array — it never allocates anything itself.

```c
/* src/utils/backbuffer.h */
#ifndef UTILS_BACKBUFFER_H
#define UTILS_BACKBUFFER_H

/*
 * stdint.h gives us portable fixed-width integer types.
 *
 * JS: there are no separate integer sizes — every number is a 64-bit float.
 * C:  `int` could be 16, 32, or 64 bits depending on the platform.
 *     uint32_t guarantees exactly 32 unsigned bits everywhere.
 *
 *   uint8_t  — unsigned  8-bit integer (0–255)
 *   uint32_t — unsigned 32-bit integer (0–4 294 967 295)
 */
#include <stdint.h>

/*
 * Backbuffer — a CPU-side array of pixels.
 *
 * JS equivalent:
 *   const backbuffer = {
 *     pixels: new Uint32Array(width * height),
 *     width:  560,
 *     height: 540,
 *     pitch:  560 * 4,      // bytes per row
 *     bytesPerPixel: 4,
 *   };
 *
 * C typedef struct:
 *   Creates a new *type name* (Backbuffer) for the struct.
 *   Without `typedef` you'd have to write `struct Backbuffer` every time.
 *   With `typedef` you just write `Backbuffer`.
 *   JS has no equivalent — every object in JS is already a named reference.
 */
typedef struct {
  /*
   * pixels — pointer to the first element of the flat pixel array.
   *
   * A *pointer* stores a memory address, not a value.
   * Think of it like a JS reference: the variable is small (8 bytes on
   * 64-bit) but it points to a much larger block of memory elsewhere.
   *
   * JS: pixels: new Uint32Array(width * height)
   * C:  uint32_t *pixels;   — "pointer to uint32_t"
   *
   * The actual memory is allocated with malloc() in main_raylib.c.
   * Here we just declare the field that will HOLD the address.
   */
  uint32_t *pixels;

  int width;           /* pixel columns  */
  int height;          /* pixel rows     */

  /*
   * pitch — bytes per row (may be > width * 4).
   *
   * Some graphics APIs pad each row to a multiple of 4/8/16 bytes for
   * alignment. The extra bytes at the end of a row are NOT pixels.
   * Always use pitch (not width) to step between rows:
   *
   *   uint32_t *row = bb->pixels + y * (bb->pitch / 4);
   *   row[x] = color;
   *
   * We divide pitch by 4 because bb->pixels is uint32_t* (4 bytes each).
   * Pointer arithmetic in C uses element size, not raw bytes.
   *
   * For our simple case pitch = width * 4 (no padding), but the correct
   * formula keeps us safe if we ever move to a platform that does pad.
   */
  int pitch;

  int bytes_per_pixel; /* almost always 4 (one uint32_t per pixel) */
} Backbuffer;

/*
 * GAME_RGBA — pack four 0-255 channel values into one uint32_t.
 *
 * Bit layout (ARGB):
 *   bits 31-24  Alpha
 *   bits 23-16  Red
 *   bits 15-8   Green
 *   bits  7-0   Blue
 *
 * JS equivalent:
 *   const packRGBA = (r, g, b, a) =>
 *     ((a << 24) | (r << 16) | (g << 8) | b) >>> 0;
 *
 * WHY (uint32_t) casts?
 *   Without them, if `a` were a signed negative int, left-shifting it
 *   is undefined behaviour in C. Casting to uint32_t first makes it safe.
 */
#define GAME_RGBA(r, g, b, a)                                               \
  (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | \
   (uint32_t)(b))

/*
 * GAME_RGB — convenience wrapper: fully opaque color (alpha = 255).
 *
 * JS: const packRGB = (r, g, b) => packRGBA(r, g, b, 255);
 */
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 255)

/* Common pre-built colors */
#define COLOR_BLACK      GAME_RGB(0,   0,   0  )
#define COLOR_DARK_BLUE  GAME_RGB(10,  10,  40 )
#define COLOR_WHITE      GAME_RGB(255, 255, 255)

#endif /* UTILS_BACKBUFFER_H */
```

> **Why?** In JavaScript every `import` immediately gives you a working value.
> In C, `#include "backbuffer.h"` pastes the text of the header into the
> current file during a *preprocessing* step before compilation. The struct
> definition tells the compiler how big a `Backbuffer` is and where its fields
> sit in memory — it creates no data by itself, just like a TypeScript
> `interface` creates no runtime object.

> **Handmade Hero principle:** The `Backbuffer` struct is completely
> platform-independent. It knows nothing about Raylib, X11, or any OS. It is
> just a description of memory. The platform layer (`main_raylib.c`) creates
> one and fills it in; the game layer writes pixels into it. Neither side needs
> to know about the other's internals.

---

## Step 2: A window with no pixel buffer

Create `src/main_raylib.c`. At this stage it just opens a window and paints it
black using Raylib's built-in `ClearBackground`. No pixel buffer yet — this is
your "does Raylib work?" checkpoint.

```c
/* src/main_raylib.c — Lesson 01, Step 2 */

/*
 * #include <raylib.h>
 *
 * JS: import * as rl from 'raylib';
 * C:  #include <raylib.h>  — pastes Raylib's function/type declarations here.
 *
 * The angle brackets < > mean "look in the system/compiler include paths".
 * Quotes " " mean "look relative to the current file first".
 */
#include "utils/backbuffer.h"  /* Our Backbuffer type */

#include <raylib.h>
#include <stdlib.h>  /* malloc, free */
#include <stdio.h>   /* fprintf      */

/*
 * #define constants
 *
 * JS: const FIELD_WIDTH = 12;
 * C:  #define FIELD_WIDTH 12
 *
 * #define is a preprocessor text substitution. Before the compiler sees your
 * code, every occurrence of FIELD_WIDTH is replaced with the literal text 12.
 * There is no variable — no memory is allocated, no type is assigned.
 * This is different from a JS const: a C #define has no type and no scope
 * (it lasts until #undef or end of file).
 *
 * We use UPPERCASE names to signal "this is a compile-time constant".
 */
#define FIELD_WIDTH   12   /* total columns including left/right walls */
#define FIELD_HEIGHT  18   /* total rows    including floor            */
#define CELL_SIZE     30   /* pixels per cell                          */
#define SIDEBAR_WIDTH 200  /* extra pixels to the right of the field   */

int main(void) {
  /*
   * Compute window size from the game constants.
   *
   * JS: const screenWidth  = FIELD_WIDTH * CELL_SIZE + SIDEBAR_WIDTH;
   * C:  int screen_width   = FIELD_WIDTH * CELL_SIZE + SIDEBAR_WIDTH;
   *
   * `int` is a signed 32-bit integer on all modern platforms. We use it for
   * pixel dimensions because they fit comfortably (560, 540).
   */
  int screen_width  = (FIELD_WIDTH  * CELL_SIZE) + SIDEBAR_WIDTH; /* 360 + 200 = 560 */
  int screen_height =  FIELD_HEIGHT * CELL_SIZE;                   /* 18  *  30 = 540 */

  /* Open the window. Raylib creates an OpenGL context inside here. */
  InitWindow(screen_width, screen_height, "Tetris - Lesson 01");
  SetTargetFPS(60);

  /*
   * Main loop — runs until the OS signals the window should close.
   *
   * JS equivalent:
   *   function loop() {
   *     // ... draw ...
   *     requestAnimationFrame(loop);
   *   }
   *   requestAnimationFrame(loop);
   *
   * C: while (!WindowShouldClose()) { ... }
   *   WindowShouldClose() returns true when the user clicks the X button or
   *   presses Alt+F4. The loop runs until that happens.
   */
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);  /* Raylib fills the window with black */
    EndDrawing();             /* Present the frame (swap buffers)   */
  }

  CloseWindow();
  return 0; /* 0 = success (Unix convention; like process.exit(0)) */
}
```

> **Why `int main(void)`?** Every C program starts at `main`. The OS calls it
> once. The return value is the exit code (0 = OK, non-zero = error). `void`
> means this function takes no parameters. JS's entry point is the top level
> of the module; C always needs `main`.

---

## Step 3: The complete pixel-buffer pipeline

Now add the `Backbuffer`, `malloc`, the fill loop, and the
`Image → Texture2D → UpdateTexture` pipeline. This is the complete lesson-01
version of `main_raylib.c`. Replace the file with this:

```c
/* src/main_raylib.c — Lesson 01 complete */
#include "utils/backbuffer.h"

#include <raylib.h>
#include <stdlib.h>   /* malloc, free        */
#include <stdio.h>    /* fprintf, stderr     */

#define FIELD_WIDTH   12
#define FIELD_HEIGHT  18
#define CELL_SIZE     30
#define SIDEBAR_WIDTH 200

int main(void) {
  int screen_width  = (FIELD_WIDTH  * CELL_SIZE) + SIDEBAR_WIDTH; /* 560 */
  int screen_height =  FIELD_HEIGHT * CELL_SIZE;                   /* 540 */

  /* ── Allocate the pixel buffer ─────────────────────────────────────────
   *
   * JS: const pixels = new Uint32Array(screen_width * screen_height);
   * C:  uint32_t *ptr = (uint32_t *)malloc(count * sizeof(uint32_t));
   *
   * malloc() allocates a contiguous block of bytes on the HEAP and returns
   * a void* (pointer to void — "I don't know the type yet"). We cast it to
   * uint32_t* so the compiler knows each element is 4 bytes.
   *
   * sizeof(uint32_t) = 4  → each pixel is 4 bytes (ARGB)
   *
   * WHY multiply by sizeof(uint32_t)?
   *   malloc takes a byte count. We want (width * height) uint32_t elements.
   *   Each uint32_t is 4 bytes, so we need width * height * 4 bytes.
   *
   * WHY (size_t)?
   *   size_t is an unsigned integer type guaranteed to hold any byte count.
   *   Multiplying two ints could overflow for very large buffers; casting
   *   to size_t first avoids that.
   */
  Backbuffer bb = {0}; /* {0} zero-initialises every field — safe starting state */
  bb.width           = screen_width;
  bb.height          = screen_height;
  bb.bytes_per_pixel = 4;
  bb.pitch           = screen_width * 4; /* bytes per row (no padding here) */
  bb.pixels = (uint32_t *)malloc(
      (size_t)screen_width * (size_t)screen_height * sizeof(uint32_t)
  );

  /* ── NULL check ────────────────────────────────────────────────────────
   *
   * malloc returns NULL if allocation fails (out of memory, or invalid size).
   * NULL is a special pointer value meaning "points to nothing".
   *
   * JS has no equivalent — if `new Uint32Array` fails, JS throws an exception.
   * C has no exceptions. If we skip this check and then use bb.pixels, the
   * program crashes with a segmentation fault (writing through a NULL pointer).
   *
   * Always check malloc for NULL. It's cheap and saves hours of debugging.
   */
  if (!bb.pixels) {
    fprintf(stderr, "ERROR: failed to allocate pixel buffer (%d x %d)\n",
            screen_width, screen_height);
    return 1; /* exit with error code */
  }

  /* ── Fill the buffer with dark blue ────────────────────────────────────
   *
   * JS: pixels.fill(COLOR_DARK_BLUE);
   * C:  for (int i = 0; i < total; i++) { bb.pixels[i] = COLOR_DARK_BLUE; }
   *
   * bb.pixels[i] is exactly the same as *(bb.pixels + i) — pointer arithmetic.
   * bb.pixels is a pointer to the first element; adding i gives a pointer to
   * the i-th element; the * dereferences it (reads/writes the value there).
   */
  int total_pixels = screen_width * screen_height;
  for (int i = 0; i < total_pixels; i++) {
    bb.pixels[i] = COLOR_DARK_BLUE;
  }

  /* ── Open the Raylib window ─────────────────────────────────────────── */
  InitWindow(screen_width, screen_height, "Tetris - Lesson 01");
  SetTargetFPS(60);

  /* ── Image → Texture2D (upload once) ───────────────────────────────────
   *
   * Our pixel array lives in CPU memory. To display it, we need it on the GPU.
   *
   * The pipeline:
   *   1. Image        — wraps our existing pixel pointer (NO copy).
   *                     It just describes the memory layout to Raylib.
   *   2. LoadTextureFromImage() — copies pixels to the GPU once, creates an
   *                     OpenGL texture object (g_texture).
   *   3. Every frame:  UpdateTexture()  — re-copies changed pixels to GPU.
   *   4.               DrawTexture()    — GPU blits the texture to screen.
   *
   * JS analogy:
   *   const imageData = new ImageData(pixels, width, height); // step 1
   *   ctx.putImageData(imageData, 0, 0);                       // step 2-3
   *   // every frame: ctx.putImageData(imageData, 0, 0);       // step 3
   *
   * PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: tells Raylib each pixel is 4 bytes.
   * mipmaps = 1: no mipmaps (we're doing pixel-art, not 3D).
   */
  Image img = {
    .data    = bb.pixels,
    .width   = bb.width,
    .height  = bb.height,
    .mipmaps = 1,
    .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  Texture2D texture = LoadTextureFromImage(img);

  /* ── Main loop ────────────────────────────────────────────────────────── */
  while (!WindowShouldClose()) {

    /* Write pixels each frame (here we just fill solid — later lessons draw
     * game objects here). */
    for (int i = 0; i < total_pixels; i++) {
      bb.pixels[i] = COLOR_DARK_BLUE;
    }

    /* Upload changed CPU pixels to the GPU texture */
    UpdateTexture(texture, bb.pixels);

    BeginDrawing();
    ClearBackground(BLACK);          /* black letterbox around our texture */
    DrawTexture(texture, 0, 0, WHITE); /* WHITE tint = draw as-is */
    EndDrawing();
  }

  /* ── Cleanup ────────────────────────────────────────────────────────────
   *
   * Unlike JavaScript there is no garbage collector.
   * Every malloc must be paired with a free.
   * Every LoadTexture must be paired with UnloadTexture.
   * Skipping these causes memory/resource leaks — the OS usually reclaims
   * them on process exit, but it's good practice to be explicit.
   */
  UnloadTexture(texture);
  CloseWindow();
  free(bb.pixels);   /* release heap memory */
  bb.pixels = NULL;  /* set to NULL so accidental re-use crashes loudly */

  return 0;
}
```

> **Why `Backbuffer bb = {0}`?** In C, local variables are NOT
> zero-initialised by default — they contain whatever garbage was on the stack.
> `= {0}` explicitly zeroes every field. `bb.pixels` starts as `NULL`, the
> ints start as 0. This is a safer habit than leaving fields uninitialised.
> In JS every object field you don't set is `undefined`; in C it's random bytes.

> **Why separate `malloc` from `InitWindow`?** We allocate the pixel buffer
> *before* opening the window so we can detect out-of-memory errors and exit
> cleanly before touching any GPU resources. This order also means the
> backbuffer is just a plain byte array — it has no dependency on Raylib at all.

---

## Build and run

Install Raylib if you haven't already (`sudo apt install libraylib-dev` on
Debian/Ubuntu, or `brew install raylib` on macOS).

```bash
# From the src/ directory:
clang -I. main_raylib.c -lraylib -lm -o tetris
./tetris
```

Or with GCC:
```bash
gcc -I. main_raylib.c -lraylib -lm -o tetris
./tetris
```

**Flag breakdown:**
| Flag | Meaning |
|------|---------|
| `-I.` | Add `.` (current dir) to header search path so `"utils/backbuffer.h"` resolves |
| `-lraylib` | Link the Raylib library |
| `-lm` | Link the C math library (Raylib needs it) |
| `-o tetris` | Output binary named `tetris` |

---

## Expected result

A 560 × 540 window titled "Tetris - Lesson 01" filled entirely with a dark
blue-black colour. No controls, no game content. Close it with the window X
button.

---

## Common mistakes

> **Common mistake:**
> ```c
> uint32_t *pixels = malloc(screen_width * screen_height);
> ```
> This fails because: `malloc` takes a *byte* count. This allocates only
> `width * height` bytes (1 byte per pixel), but we need 4 bytes per pixel.
> The program will run but write garbage pixels and possibly crash.
>
> **Correct version:**
> ```c
> uint32_t *pixels = (uint32_t *)malloc(
>     (size_t)screen_width * (size_t)screen_height * sizeof(uint32_t)
> );
> ```

---

> **Common mistake:**
> ```c
> bb.pixels = malloc(...);
> /* no NULL check */
> for (int i = 0; i < total; i++) bb.pixels[i] = 0;
> ```
> This fails because: if `malloc` returns `NULL` (e.g. out of memory) and you
> dereference it, you get a **segmentation fault** — an immediate crash with no
> error message. C does not throw exceptions.
>
> **Correct version:** Always check `if (!bb.pixels) { /* handle error */ }` immediately after `malloc`.

---

> **Common mistake:**
> ```c
> Image img = {
>   bb.pixels, bb.width, bb.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
> };
> ```
> This fails because: positional struct initialisation in C requires you to know
> the exact field order (and Raylib's `Image` struct field order may not be
> obvious). If Raylib adds a field in a future version, this silently breaks.
>
> **Correct version:** Use designated initialisers:
> ```c
> Image img = {
>   .data = bb.pixels, .width = bb.width, .height = bb.height,
>   .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
> };
> ```

---

## Exercises

1. **Change the background colour.** Replace `COLOR_DARK_BLUE` with
   `GAME_RGB(40, 0, 0)` (dark red). Recompile and verify the window changes.
   Trace the `GAME_RGBA` macro by hand: what is the uint32_t value for
   `GAME_RGB(40, 0, 0)`?

2. **Draw a gradient.** Instead of filling every pixel with the same colour,
   make the brightness increase from top to bottom:
   ```c
   for (int y = 0; y < screen_height; y++) {
     for (int x = 0; x < screen_width; x++) {
       int idx = y * screen_width + x;
       uint8_t brightness = (uint8_t)((y * 255) / screen_height);
       bb.pixels[idx] = GAME_RGB(0, 0, brightness);
     }
   }
   ```
   Notice the nested loop vs the flat loop: both produce the same result
   because `idx = y * screen_width + x` maps 2D coordinates to 1D.

3. **Print the backbuffer size.** Before the main loop, add:
   ```c
   printf("Backbuffer: %d x %d = %d pixels = %d KB\n",
          bb.width, bb.height,
          bb.width * bb.height,
          bb.width * bb.height * 4 / 1024);
   ```
   Verify the calculation: 560 × 540 × 4 ÷ 1024 = **1181 KB ≈ 1.15 MB**.

---

## What's next

In **Lesson 02** we add `draw_rect()` and `draw_rect_blend()` — functions that
draw coloured rectangles (clipped to the buffer bounds) and implement
**alpha blending**. That gives us the building block for every piece of the
Tetris UI. We'll also introduce `utils/math.h` with the `MIN`/`MAX` macros
that make clipping safe and readable.
