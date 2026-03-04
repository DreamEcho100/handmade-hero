#ifndef UTILS_BACKBUFFER_H
#define UTILS_BACKBUFFER_H

/* =============================================================================
 * utils/backbuffer.h — Platform-Independent Rendering Target
 * =============================================================================
 *
 * A "backbuffer" (also called a "framebuffer" or "offscreen buffer") is simply
 * a flat array of pixels that we draw into. When a frame is complete we hand
 * this array to the OS/graphics library to display on screen.
 *
 * WHY "BACK" BUFFER?
 *   The GPU shows one buffer on screen (the "front" buffer) while the CPU draws
 *   the next frame into the other (the "back" buffer). When drawing is done,
 *   the two are swapped — this prevents the user from seeing half-drawn frames.
 *   This technique is called "double buffering".
 *
 * JS COMPARISON:
 *   In a browser you'd use a <canvas> element and draw via the Canvas 2D API
 *   (ctx.fillRect, ctx.drawImage, …) or WebGL. Here we bypass all of that and
 *   write raw pixel values directly into a flat array — it is conceptually
 *   identical to ImageData in the browser:
 *
 *   JS:  const img = new ImageData(width, height);  // img.data is Uint8ClampedArray
 *   C:   uint32_t *pixels = malloc(width * height * 4); // one uint32 per pixel
 *
 * =============================================================================
 */

/* JS: import { ... } from 'somewhere'
 * C:  #include <stdint.h>
 *
 * stdint.h defines portable integer types with explicit sizes:
 *   uint8_t   — unsigned  8-bit integer (0–255),       like a JS number clamped to 1 byte
 *   uint32_t  — unsigned 32-bit integer (0–4294967295), like a JS number that fits in 4 bytes
 * Without this header, "int" could be 16, 32, or 64 bits depending on the
 * platform. Using uint32_t guarantees exactly 32 bits everywhere.
 */
#include <stdint.h>

/* ══════ Pixel Memory Layout ══════
 *
 * Pixels are stored in a flat 1-D array, row by row (row-major order).
 * For a 4×3 buffer the memory layout looks like this:
 *
 *   Index:  [ 0  1  2  3 | 4  5  6  7 | 8  9 10 11 ]
 *   Screen:  [  row 0    |   row 1    |   row 2    ]
 *             (0,0)→(3,0)  (0,1)→(3,1)  (0,2)→(3,2)
 *
 * To get the pixel at screen position (x, y):
 *   index = y * width + x
 *
 * JS equivalent:
 *   const index = y * width + x;
 *   pixels[index] = color;
 *
 * C equivalent:
 *   uint32_t *pixel = bb->pixels + y * (bb->pitch / 4) + x;
 *   *pixel = color;
 *
 * Note: we use `pitch / 4` instead of `width` — see the `pitch` field below.
 *
 * ══════ ARGB vs RGBA Byte Order ══════
 *
 * Each pixel is a 32-bit unsigned integer. The 4 bytes encode alpha, red,
 * green, and blue channels.
 *
 * We use ARGB packing (most significant byte = alpha):
 *
 *   Bit positions inside a uint32_t (32 bits):
 *   ┌────────────┬────────────┬────────────┬────────────┐
 *   │  bits 31-24│  bits 23-16│  bits 15-8 │  bits 7-0  │
 *   │   Alpha    │    Red     │   Green    │    Blue    │
 *   └────────────┴────────────┴────────────┴────────────┘
 *   e.g.  0xFF_FF_00_00 = fully opaque red
 *         0x80_00_FF_00 = 50% transparent green
 *         0x00_00_00_FF = fully transparent blue (invisible)
 *
 * ARGB vs RGBA confusion:
 *   "RGBA" sometimes means the bytes are ordered R,G,B,A in memory (used by
 *   OpenGL textures and browser ImageData). "ARGB" means A,R,G,B in memory.
 *   In our 32-bit integer we always work with the value as a whole number and
 *   use bit-shifts to extract channels — the in-memory byte order depends on
 *   the CPU's endianness, but our shifts are defined by the integer value.
 *
 * JS equivalent using typed arrays (for comparison):
 *   // RGBA byte order (browser ImageData):
 *   data[i*4 + 0] = r;  // Red
 *   data[i*4 + 1] = g;  // Green
 *   data[i*4 + 2] = b;  // Blue
 *   data[i*4 + 3] = a;  // Alpha
 *
 *   // Our ARGB packed uint32 approach:
 *   pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
 */

/* ══════ Backbuffer Struct ══════
 *
 * JS: new Uint32Array(width * height) | C: uint32_t *pixels
 *
 * A struct in C is like a plain object (POJO) in JS — it groups related
 * fields together under one name.
 *
 * JS equivalent:
 *   const backbuffer = {
 *     pixels: new Uint32Array(width * height), // flat pixel array
 *     width: 800,
 *     height: 600,
 *     pitch: 800 * 4,      // bytes per row
 *     bytes_per_pixel: 4,
 *   };
 */
typedef struct {
  /* Pointer to the flat array of packed ARGB pixels.
   * JS: new Uint32Array(width * height)
   * C:  uint32_t *pixels   — a pointer to the first element of the array.
   *
   * "Pointer" means this field stores a memory ADDRESS, not the data itself.
   * Think of it as a reference in JS — pixels itself is small (8 bytes on
   * 64-bit), but it points to a much larger block of memory elsewhere. */
  uint32_t *pixels;

  int width;  /* Pixel columns — e.g. 800 */
  int height; /* Pixel rows    — e.g. 600 */

  /* pitch: Bytes per row (may be > width * 4!).
   *
   * You might expect pitch == width * bytes_per_pixel, and usually it does.
   * BUT some graphics APIs (X11, Windows GDI, DirectDraw) pad each row to a
   * multiple of 4, 8, or 16 bytes for alignment/SIMD performance. The extra
   * padding bytes at the end of a row are NOT pixels — they must be skipped.
   *
   * Example: width=5, bytes_per_pixel=4
   *   Actual data: 5 * 4 = 20 bytes
   *   Padded to 32-byte alignment: pitch = 32 bytes
   *   Padding per row: 12 unused bytes
   *
   * To index pixel (x, y) SAFELY you must use pitch (not width):
   *   uint32_t *row = bb->pixels + y * (bb->pitch / 4);
   *   row[x] = color;
   *
   * We divide pitch by 4 because pitch is in BYTES and pixels is uint32_t*
   * (4 bytes each). Pointer arithmetic in C uses the element size, not bytes.
   */
  int pitch;

  /* How many bytes per pixel — nearly always 4 (one uint32_t). */
  int bytes_per_pixel;
} Backbuffer;

/* ══════ Color Packing Macros ══════ */

/* GAME_RGBA — Pack r, g, b, a channel values (0-255 each) into one uint32_t.
 *
 * Bit-shift breakdown:
 *   (a << 24)  — shift alpha 24 bits left  → bits 31-24
 *   (r << 16)  — shift red   16 bits left  → bits 23-16
 *   (g <<  8)  — shift green  8 bits left  → bits 15-8
 *   (b)        — blue stays at bits 7-0
 *   |           — bitwise OR combines all four into one 32-bit value
 *
 * JS equivalent:
 *   const packRGBA = (r, g, b, a) =>
 *     ((a << 24) | (r << 16) | (g << 8) | b) >>> 0; // >>> 0 to get uint32
 *
 * C macro:
 *   GAME_RGBA(r, g, b, a)
 *
 * The (uint32_t) casts ensure each channel is treated as unsigned before
 * shifting. Without them, if `a` were a signed negative int and you shifted
 * it left, you'd get undefined behaviour in C (signed overflow).
 *
 * Example:
 *   GAME_RGBA(255, 128, 0, 255)
 *   = (255 << 24) | (255 << 16) | (128 << 8) | (0)
 *   = 0xFF_FF_80_00   ← fully opaque orange
 */
#define GAME_RGBA(r, g, b, a)                                                  \
  (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) |    \
   (uint32_t)(b))

/* GAME_RGB — Convenience wrapper for fully opaque colors (alpha = 255).
 * Use this when you don't need transparency.
 *
 * JS equivalent:
 *   const packRGB = (r, g, b) => packRGBA(r, g, b, 255);
 */
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 255)

#endif /* UTILS_BACKBUFFER_H */
