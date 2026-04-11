/* =============================================================================
 * utils/backbuffer.h — Pixel Backbuffer (Software Framebuffer)
 * =============================================================================
 *
 * WHAT IS A BACKBUFFER?
 * ─────────────────────
 * A "backbuffer" is an array of pixels in RAM that we draw into before
 * sending the image to the screen.  Drawing into an off-screen buffer first
 * means the player never sees a half-rendered frame (no tearing).
 *
 * JS equivalent of the overall concept:
 *   const backbuffer = document.createElement('canvas');
 *   const ctx = backbuffer.getContext('2d');
 *   const imageData = ctx.getImageData(0, 0, width, height);
 *   // imageData.data is a Uint8ClampedArray — our "pixels" array
 *
 * C STRUCT vs JS OBJECT
 * ─────────────────────
 * In JS/TS:  interface Backbuffer { pixels: Uint32Array; width: number; height: number; pitch: number; }
 * In C:      struct AsteroidsBackbuffer { uint32_t *pixels; int width; int height; int pitch; };
 *
 * The key difference: in C, the struct is just a description of memory layout.
 * The pixels array lives somewhere else in memory; the struct only holds a
 * POINTER to it (which is why platform code allocates the pixel data).
 *
 * WHAT IS pitch?
 * ──────────────
 * "Pitch" is the number of BYTES between the start of one row and the start
 * of the next row.  For a tightly-packed RGBA image: pitch = width * 4.
 * Keeping it explicit (rather than assuming width*4) makes the code safe
 * if the GPU aligns rows to 64-byte boundaries (which some drivers do).
 *
 *   Row index formula:  pixel_ptr = pixels + row * (pitch / 4) + col
 *     ↑ dividing by 4 because pixels is uint32_t* (4 bytes per element),
 *       but pitch is measured in bytes.
 * =============================================================================
 */

#ifndef ASTEROIDS_BACKBUFFER_H
#define ASTEROIDS_BACKBUFFER_H

#include <stdint.h>  /* uint32_t — fixed-width type: always exactly 32 bits */

/* ══════ Screen Dimensions ═════════════════════════════════════════════════
   These match the original OneLoneCoder Asteroids demo (256×240).
   Using #define (compile-time constant) rather than const int means the
   values can be used in array sizes and switch statements.               */
#define SCREEN_W 512
#define SCREEN_H 480

/* ══════ AsteroidsBackbuffer ════════════════════════════════════════════════

   The software framebuffer passed between platform and game.
   platform code allocates + owns the pixel array; game code only writes.  */
typedef struct {
    uint32_t *pixels;  /* flat array of width*height pixels, row-major     */
    int       width;   /* pixels per row                                    */
    int       height;  /* number of rows                                    */
    int       pitch;   /* bytes per row (= width * 4 for this course)       */
} AsteroidsBackbuffer;

/* ══════ GAME_RGBA Colour Macro ══════════════════════════════════════════════
 *
 * VERY IMPORTANT — READ THIS CAREFULLY
 * ─────────────────────────────────────
 * Each pixel is stored as a 32-bit unsigned integer.  The byte layout is:
 *
 *   Memory (little-endian):  [ R ][ G ][ B ][ A ]
 *                              ↑ lowest address = lowest byte
 *
 *   As a uint32_t value:  0xAABBGGRR
 *     R occupies bits  0–7  (least-significant byte)
 *     G occupies bits  8–15
 *     B occupies bits 16–23
 *     A occupies bits 24–31 (most-significant byte)
 *
 * The macro packs four bytes into one uint32_t:
 *   GAME_RGBA(r, g, b, a) = (a << 24) | (b << 16) | (g << 8) | r
 *
 * CONSEQUENCE FOR OPENGL:
 *   X11/GLX backend:   glTexImage2D(..., GL_RGBA, GL_UNSIGNED_BYTE, pixels)
 *                       GL_RGBA tells OpenGL to read bytes in R,G,B,A order.
 *   *** DO NOT use GL_BGRA here — that would swap red and blue! ***
 *
 * CONSEQUENCE FOR RAYLIB:
 *   LoadImageFromMemory / UpdateTexture expects PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
 *   which matches our byte order exactly.
 *
 * WHY THIS LAYOUT?
 *   Most modern CPUs are little-endian.  Storing R in the low byte means
 *   the uint32_t value 0x000000FF is "opaque red" — easy to read in hex.
 *   Alpha-blending code can extract channels with simple shifts and masks.
 *
 * JS equivalent:
 *   // Uint8ClampedArray is always [R, G, B, A, R, G, B, A, ...]
 *   function rgba(r, g, b, a) {
 *     const buf = new ArrayBuffer(4);
 *     const view = new DataView(buf);
 *     view.setUint8(0, r); view.setUint8(1, g);
 *     view.setUint8(2, b); view.setUint8(3, a);
 *     return view.getUint32(0, true);  // true = little-endian
 *   }
 */
#define GAME_RGBA(r, g, b, a) \
    ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(g) << 8 | (uint32_t)(r))

/* ══════ Predefined Colours ════════════════════════════════════════════════ */
#define COLOR_BLACK       GAME_RGBA(0,   0,   0,   255)
#define COLOR_WHITE       GAME_RGBA(255, 255, 255, 255)
#define COLOR_RED         GAME_RGBA(255, 0,   0,   255)
#define COLOR_GREEN       GAME_RGBA(0,   255, 0,   255)
#define COLOR_BLUE        GAME_RGBA(0,   0,   255, 255)
#define COLOR_YELLOW      GAME_RGBA(255, 255, 0,   255)
#define COLOR_CYAN        GAME_RGBA(0,   255, 255, 255)
#define COLOR_ORANGE      GAME_RGBA(255, 165, 0,   255)
#define COLOR_DARK_GREY   GAME_RGBA(40,  40,  40,  255)
#define COLOR_GREY        GAME_RGBA(128, 128, 128, 255)

/* Semi-transparent overlays (for GAME OVER / HUD backgrounds) */
#define COLOR_OVERLAY_DIM GAME_RGBA(0, 0, 0, 160)

#endif /* ASTEROIDS_BACKBUFFER_H */
