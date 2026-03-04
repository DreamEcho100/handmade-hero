# Lesson 01 — Window and Backbuffer

**Source files:** `src/main_x11.c`, `src/utils/backbuffer.h`
**Functions:** `platform_init()`, `platform_display_backbuffer()`, `main()`

---

## What We're Building

Open an X11 window, allocate a plain CPU pixel array (the "backbuffer"), write
coloured pixels into it, then upload that array to the GPU once per frame so it
appears on screen.

---

## The Concept

### JS analogy — `canvas + ImageData`

In JavaScript you draw to the screen like this:

```js
const canvas = document.getElementById('game');
const ctx    = canvas.getContext('2d');

// Allocate a pixel array
const imageData = ctx.createImageData(1200, 460);   // RGBA bytes
const pixels    = new Uint32Array(imageData.data.buffer); // 32-bit view

// Write a pixel
const r = 255, g = 0, b = 0;
pixels[py * 1200 + px] = (255 << 24) | (r << 16) | (g << 8) | b;

// Upload to the canvas
ctx.putImageData(imageData, 0, 0);
```

The C version is structurally identical:

| JS concept | C equivalent |
|---|---|
| `new Uint32Array(width * height)` | `malloc(width * height * sizeof(uint32_t))` |
| `ImageData` | `SnakeBackbuffer` struct |
| `ctx.putImageData()` | `glTexImage2D()` + `glXSwapBuffers()` |
| `canvas.getContext('2d')` | `glXCreateContext()` |

The key difference: the browser hides X11, OpenGL, and memory allocation from
you.  Here you write every layer yourself — which is why you can see exactly
what is happening.

---

## The Code

### `SnakeBackbuffer` — the pixel array struct (`src/utils/backbuffer.h`)

```c
typedef struct {
    uint32_t *pixels;  /* Flat array: width × height uint32_t values         */
    int width;         /* Buffer width in pixels                              */
    int height;        /* Buffer height in pixels                             */
    int pitch;         /* Bytes per row.  For a contiguous buffer: width * 4 */
} SnakeBackbuffer;
```

`pitch` is the number of **bytes** to advance from the start of one row to the
start of the next.  For our buffer it equals `width * 4`, but working with
pitch (not hardcoded `width * 4`) is a habit that pays off whenever you deal
with sub-textures or GPU-aligned memory — both of which add padding bytes
between rows.

### Writing a pixel

```c
/* Pixel at column px, row py: */
bb->pixels[py * (bb->pitch / 4) + px] = GAME_RGB(r, g, b);
```

Step by step:

- `bb->pitch / 4` — converts bytes-per-row to **pixels**-per-row (divides out
  the 4 bytes each `uint32_t` occupies).
- `py * (bb->pitch / 4)` — the index of the first pixel on row `py`.
- `+ px` — advance to column `px` within that row.

Equivalent JS: `pixels[py * imageData.width + px] = ...`

### Packing colour with `GAME_RGB` (`src/utils/backbuffer.h`)

```c
/* GAME_RGBA — pack (r, g, b, a) into a uint32_t whose in-memory bytes are
 * [R][G][B][A] on a little-endian CPU (the natural layout for both GL_RGBA
 * and Raylib's PIXELFORMAT_UNCOMPRESSED_R8G8B8A8).
 *
 * Integer value layout: 0xAABBGGRR
 *   bits  0– 7 → R  (lowest address byte in memory)
 *   bits  8–15 → G
 *   bits 16–23 → B
 *   bits 24–31 → A  (highest address byte in memory)
 */
#define GAME_RGBA(r, g, b, a)                   \
    (((uint32_t)(a) << 24) |                    \
     ((uint32_t)(b) << 16) |                    \
     ((uint32_t)(g) <<  8) |                    \
      (uint32_t)(r))

/* GAME_RGB — convenience wrapper; sets alpha = 0xFF (fully opaque). */
#define GAME_RGB(r, g, b)  GAME_RGBA((r), (g), (b), 0xFF)
```

Example: `GAME_RGB(255, 0, 0)` → integer `0xFF0000FF` (fully-opaque red).
In memory on a little-endian CPU the bytes are `[FF, 00, 00, FF]` = R, G, B, A
— exactly what `GL_RGBA` and Raylib's `R8G8B8A8` format expect.  Each colour
component occupies 8 bits.  Bit-shifting places them in the right lanes.

JS equivalent: `(255 << 24) | (r << 16) | (g << 8) | b` — the same expression,
just written explicitly.

### Allocating the backbuffer in `platform_init` (`src/main_x11.c`)

```c
void platform_init(const char *title, int width, int height) {
    /* … X11 window creation omitted for brevity … */

    /* Allocate and zero the CPU-side backbuffer */
    g_backbuffer.width  = width;
    g_backbuffer.height = height;
    g_backbuffer.pitch  = width * 4;   /* bytes per row */
    g_backbuffer.pixels = (uint32_t *)malloc(
        (size_t)(width * height) * sizeof(uint32_t));
    memset(g_backbuffer.pixels, 0,
        (size_t)(width * height) * sizeof(uint32_t));

    setup_gl();   /* creates the GPU texture object */
    setup_alsa(44100);
}
```

`malloc` returns a `void *`.  The cast `(uint32_t *)` tells the compiler we
want to treat this memory as an array of 32-bit integers.  `memset(..., 0, ...)`
zeroes every byte — the screen starts black.

JS equivalent: `new Uint32Array(width * height).fill(0)`.

### The display loop — write → upload → swap (`src/main_x11.c`)

```c
/* After snake_render() has written pixels into g_backbuffer … */
static void platform_display_backbuffer(void) {
    /* Step 1: upload the CPU pixel array to a GPU texture */
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, g_texture_id);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8,
                 g_backbuffer.width, g_backbuffer.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, g_backbuffer.pixels);

    /* Step 2: draw a fullscreen quad textured with that texture */
    glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2i(0,                  0);                   glVertex2i(0,            0);
        glTexCoord2i(g_backbuffer.width, 0);                   glVertex2i(WINDOW_WIDTH, 0);
        glTexCoord2i(0,                  g_backbuffer.height); glVertex2i(0,            WINDOW_HEIGHT);
        glTexCoord2i(g_backbuffer.width, g_backbuffer.height); glVertex2i(WINDOW_WIDTH, WINDOW_HEIGHT);
    glEnd();

    /* Step 3: swap front/back GPU buffers → visible on screen */
    glXSwapBuffers(g_display, g_window);
}
```

Why `GL_RGBA`?  Our `GAME_RGBA` macro stores pixels so that bytes in memory are
`[R][G][B][A]` on a little-endian CPU.  `GL_RGBA` tells OpenGL to read each
pixel as four consecutive bytes in R, G, B, A order — an exact match.  Using
the wrong format flag (e.g. `GL_BGRA`) would swap red and blue, producing
visually wrong colours on the Raylib backend (which always uses R8G8B8A8).

Why `GL_TEXTURE_RECTANGLE_ARB` instead of `GL_TEXTURE_2D`?  Normal
`GL_TEXTURE_2D` requires power-of-two dimensions (512, 1024, 2048…).  Our
window is 1200×460, neither of which is a power of two.
`GL_TEXTURE_RECTANGLE_ARB` accepts arbitrary sizes and uses pixel coordinates
for texture lookups instead of the 0–1 normalised range.

### The game loop frame sequence (`src/main_x11.c → main()`)

```c
while (g_is_running) {
    /* 1. Time */
    current_time = platform_get_time();
    delta_time   = (float)(current_time - last_time);
    if (delta_time > 0.05f) delta_time = 0.05f;  /* cap at 50 ms */
    last_time    = current_time;

    /* 2–3. Input (covered in Lesson 03) */

    /* 4. Logic — snake_update writes nothing to pixels */
    snake_update(&state, &inputs[1], delta_time);

    /* 5. Render — snake_render writes into g_backbuffer.pixels */
    snake_render(&state, &g_backbuffer);

    /* 6. Audio */
    fill_audio(&state);

    /* 7. Display — upload pixels → GPU → screen */
    platform_display_backbuffer();
}
```

This is the classic **game loop**: update logic → render to backbuffer →
display backbuffer.  The game never touches the GPU directly — that is the
platform's job.

---

## What To Notice

- **pitch ≠ width** — `pitch` is bytes per row; `pitch / 4` is pixels per row.
  Our buffer has `pitch = width * 4`, so they differ by a factor of 4.  Use
  `pitch / 4` in all pixel-index math so the code stays correct if padding is
  ever added.

- **`malloc` is called once** — the backbuffer is allocated in
  `platform_init()` and freed in `platform_shutdown()`.  No allocation happens
  inside the game loop.  This is intentional: heap allocation in a tight loop
  causes unpredictable latency.

- **`GL_RGBA` matches memory layout** — the format flag tells OpenGL how the
  bytes are arranged in `pixels[]`.  Getting this wrong produces
  red-and-blue-swapped colours.  The `GAME_RGBA` macro stores bytes
  `[R][G][B][A]` in memory (least-significant byte = R on little-endian),
  which is exactly the `GL_RGBA` order.  Raylib's `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`
  uses the same byte order, so both backends agree.

- **The game layer never sees X11 or OpenGL** — `game.c` only receives a
  `SnakeBackbuffer *` and writes `uint32_t` values.  It compiles and runs
  identically whether the backend is X11+GLX, Raylib, or anything else.

---

## Exercises

1. **Change the background colour.**
   In `snake_render()` (`src/game.c`), the first draw call clears the
   backbuffer to black:
   ```c
   draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);
   ```
   Change `COLOR_BLACK` to `GAME_RGB(20, 20, 40)` (dark navy) and rebuild.
   Observe how the entire background changes — the render loop paints every
   pixel fresh each frame.

2. **Verify the pixel index formula.**
   Add a temporary snippet in `snake_render()` that paints a single bright
   pixel at `(px=100, py=200)` using the raw index formula:
   ```c
   bb->pixels[200 * (bb->pitch / 4) + 100] = GAME_RGB(255, 0, 255);
   ```
   Run the game and confirm a magenta dot appears at column 100, row 200.
   Then try `bb->pixels[200 * bb->width + 100]` instead and confirm both
   produce the same result (since `pitch = width * 4`, dividing by 4 cancels).

3. **Trace the full pipeline.**
   Add `printf("frame rendered\n");` just before `glXSwapBuffers()` in
   `platform_display_backbuffer()`.  Run the game for two seconds, then count
   the lines: you should see roughly 60 per second (the loop runs as fast as
   the display driver allows).  This is how you can measure real frame rate
   without a profiler.
