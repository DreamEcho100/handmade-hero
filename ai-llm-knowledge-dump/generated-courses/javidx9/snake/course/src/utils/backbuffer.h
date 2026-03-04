/* ═══════════════════════════════════════════════════════════════════════════
 * utils/backbuffer.h  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * Defines the `SnakeBackbuffer` type and the colour-packing macros.
 *
 * THE BACKBUFFER PIPELINE (core Handmade Hero concept)
 * ─────────────────────────────────────────────────────
 * Instead of calling OS drawing functions directly, the game writes raw
 * pixel data into a plain C array (the "backbuffer"):
 *
 *   ┌────────────────────────┐     ┌─────────────────────────┐
 *   │  game.c                │     │  main_x11.c /           │
 *   │  snake_render() writes │────▶│  main_raylib.c          │
 *   │  into pixels[]         │     │  uploads pixels[] to GPU│
 *   └────────────────────────┘     └─────────────────────────┘
 *
 * Benefits:
 *  • Platform independence — game.c never calls X11 or Raylib.
 *  • Testability — you can inspect the pixel array directly.
 *  • No blitting overhead — one GPU upload per frame.
 *
 * JS equivalent: drawing to an off-screen OffscreenCanvas, then blitting it
 * to the visible canvas with drawImage() — except we do it at the byte level.
 *
 * COURSE NOTE: We add a `pitch` field (bytes per row) that the reference
 * source omits.  This teaches the stride concept — row N starts at byte
 * `pitch * N`, not `width * 4 * N` (they're equal for contiguous buffers,
 * but stride-awareness is a skill you'll need for sub-textures and aligned
 * memory).  See Lesson 01 for the full explanation.
 */

#ifndef UTILS_BACKBUFFER_H
#define UTILS_BACKBUFFER_H

#include <stdint.h>  /* uint32_t — JS: no equivalent; JS numbers are always 64-bit float */

/* ══════ SnakeBackbuffer ════════════════════════════════════════════════════
 *
 * One SnakeBackbuffer lives in main() on the stack; its pixels are malloc'd
 * once at startup and freed at exit.  Every field is set once in main() and
 * then read-only (only `pixels` contents change each frame).
 *
 * MEMORY LAYOUT — flat 2-D array of 32-bit ARGB pixels:
 *
 *   Row 0:  pixels[0]         pixels[1]         … pixels[width-1]
 *   Row 1:  pixels[pitch/4]   pixels[pitch/4+1] … pixels[pitch/4+width-1]
 *   Row N:  pixels[N*(pitch/4)]  …
 *
 *   Pixel index formula:  py * (pitch / 4) + px
 *     py     = row    (0 = top)
 *     px     = column (0 = left)
 *     pitch  = bytes per row (= width * 4 for a contiguous buffer)
 *
 * JS equivalent: an ImageData object (ctx.getImageData) where data[] is
 * interleaved RGBA bytes.  Here we store ARGB in a single uint32_t per pixel.
 */
typedef struct {
    uint32_t *pixels;  /* Pointer to the flat pixel array.
                          Allocated once with malloc() in main(); freed at exit.
                          JS: new Uint32Array(width * height)                  */
    int width;         /* Width of the buffer in pixels                        */
    int height;        /* Height of the buffer in pixels                       */
    int pitch;         /* Bytes per row.  For a contiguous buffer: width * 4.
                          Using pitch instead of width * 4 in pixel index math
                          makes the code correct for aligned / sub-textures too.
                          JS: no equivalent — ImageData always uses contiguous layout. */
} SnakeBackbuffer;

/* ══════ Colour Macros ═════════════════════════════════════════════════════
 *
 * HOW ARGB PACKING WORKS
 * ─────────────────────────────────────────────────────────────────────────
 * Each pixel is stored so that in memory (little-endian) the bytes are [R][G][B][A].
 * The uint32_t value is 0xAABBGGRR:
 *
 *   Bits 31-24: Alpha (A) — 0 = transparent, 255 = opaque
 *   Bits 23-16: Blue  (B)
 *   Bits 15-8:  Green (G)
 *   Bits 7-0:   Red   (R)
 *
 * Packing example: pure red, fully opaque
 *   GAME_RGB(255, 0, 0)
 *   = GAME_RGBA(255, 0, 0, 0xFF)
 *   = (0xFF << 24) | (0 << 16) | (0 << 8) | 255
 *   = 0xFF0000FF
 *   In memory (LE): [0xFF][0x00][0x00][0xFF] = [R=255][G=0][B=0][A=255] ✓
 *
 * The platform uploads this array with GL_RGBA / PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
 * — both read byte[0] as Red, byte[1] as Green, etc. → colours match on both backends.
 *
 * JS equivalent: ctx.fillStyle = 'rgba(r, g, b, a)' — but stored as a
 * plain integer, which is much faster to write per-pixel in a tight loop.
 *
 * WHY MACROS NOT FUNCTIONS?
 *   The compiler inlines these — no call overhead.
 *   They also work on compile-time constants (COLOR_BLACK = GAME_RGB(0,0,0)
 *   becomes a single integer literal; the CPU never computes it at runtime).
 */

/* GAME_RGBA — pack (r, g, b, a) so that on little-endian x86 the bytes sit in
 * memory as [R][G][B][A] (lowest address = R).
 *
 * The uint32 VALUE is therefore 0xAABBGGRR:
 *   bits  0- 7 = R   (r in the low byte  → memory byte 0)
 *   bits  8-15 = G   (g                  → memory byte 1)
 *   bits 16-23 = B   (b                  → memory byte 2)
 *   bits 24-31 = A   (a in the high byte → memory byte 3)
 *
 * WHY THIS LAYOUT?
 *   Both platform backends read the pixel array as RGBA:
 *   • X11/GL:   glTexImage2D(..., GL_RGBA, GL_UNSIGNED_BYTE, pixels)
 *               → GL reads byte[0]=R, byte[1]=G, byte[2]=B, byte[3]=A ✓
 *   • Raylib:   UpdateTexture with PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
 *               → Raylib reads byte[0]=R, byte[1]=G, byte[2]=B, byte[3]=A ✓
 *
 * The previously used layout 0xAARRGGBB stored bytes as [B][G][R][A] (BGRA in
 * memory on LE), which matched GL_BGRA but broke Raylib's R8G8B8A8 format —
 * causing R and B channels to be swapped (e.g. GREEN snake appeared BLUE).
 */
#define GAME_RGBA(r, g, b, a)                                                  \
    (((uint32_t)(a) << 24) |                                                   \
     ((uint32_t)(b) << 16) |                                                   \
     ((uint32_t)(g) <<  8) |                                                    \
      (uint32_t)(r))

/* GAME_RGB — convenience wrapper; sets alpha = 0xFF (fully opaque). */
#define GAME_RGB(r, g, b)  GAME_RGBA((r), (g), (b), 0xFF)

/* ══════ Predefined Colours ════════════════════════════════════════════════
 *
 * Compile-time constants — these become immediate integers in the binary.
 * Using named constants prevents typos like GAME_RGB(256, 0, 0) (bug!) and
 * makes the rendering code self-documenting.
 */
#define COLOR_BLACK     GAME_RGB(  0,   0,   0)
#define COLOR_WHITE     GAME_RGB(255, 255, 255)
#define COLOR_RED       GAME_RGB(220,  50,  50)
#define COLOR_DARK_RED  GAME_RGB(139,   0,   0)
#define COLOR_GREEN     GAME_RGB( 50, 205,  50)
#define COLOR_YELLOW    GAME_RGB(255, 215,   0)
#define COLOR_DARK_GRAY GAME_RGB( 64,  64,  64)
#define COLOR_GRAY      GAME_RGB(128, 128, 128)

#endif /* UTILS_BACKBUFFER_H */
