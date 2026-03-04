/* =============================================================================
 * utils/backbuffer.h — Pixel Backbuffer (Software Framebuffer)
 * =============================================================================
 *
 * WHAT IS A BACKBUFFER?
 * ─────────────────────
 * A "backbuffer" is a flat array of pixels in RAM that we draw into before
 * sending the image to the screen.  Drawing off-screen first means the player
 * never sees a half-rendered frame (no tearing).
 *
 * JS equivalent of the overall concept:
 *   const canvas = document.createElement('canvas');
 *   const ctx = canvas.getContext('2d');
 *   const imageData = ctx.getImageData(0, 0, width, height);
 *   // imageData.data is a Uint8ClampedArray — our "pixels" array
 *
 * In C:
 *   - The struct just describes the memory layout (like a TS interface).
 *   - The pixel array lives elsewhere; the struct holds a POINTER to it.
 *   - Platform code allocates the pixel data; game code only writes into it.
 *
 * WHAT IS pitch?
 * ──────────────
 * "Pitch" is the number of BYTES between the start of one pixel row and the
 * start of the next.  For a tightly-packed RGBA image:  pitch = width * 4.
 * Keeping it explicit (rather than hardcoding width*4) is safe if the GPU
 * ever aligns rows to 64-byte boundaries (some drivers do).
 *
 *   Row-major index:  pixels[row * (pitch / 4) + col]
 *   (divide by 4 because pixels is uint32_t* — 4 bytes per element —
 *    but pitch is measured in bytes)
 * =============================================================================
 */

#ifndef FROGGER_BACKBUFFER_H
#define FROGGER_BACKBUFFER_H

#include <stdint.h>  /* uint32_t — always exactly 32 bits, any platform */

/* ══════ Screen Dimensions ══════════════════════════════════════════════════
   Frogger uses the same logical canvas as the original OneLoneCoder port:
     128 cells × 80 cells, each cell = 8 pixels → 1024 × 640 px.
   Using #define (compile-time constant) so they can be used in array sizes. */
#define SCREEN_CELLS_W  128
#define SCREEN_CELLS_H   80
#define CELL_PX           8
#define SCREEN_W  (SCREEN_CELLS_W * CELL_PX)   /* 1024 pixels */
#define SCREEN_H  (SCREEN_CELLS_H * CELL_PX)   /* 640 pixels  */

/* ══════ Backbuffer ═════════════════════════════════════════════════════════

   Passed by pointer from platform code to game_render().
   Platform allocates once at startup; game code only reads/writes pixels.  */
typedef struct {
    uint32_t *pixels;  /* flat array: row-major, width * height elements   */
    int       width;   /* pixels per row (= SCREEN_W after init)           */
    int       height;  /* number of rows  (= SCREEN_H after init)          */
    int       pitch;   /* bytes per row   (= width * 4 for this course)    */
} Backbuffer;

/* ══════ GAME_RGBA Colour Macro ═════════════════════════════════════════════
 *
 * BYTE ORDER — READ THIS CAREFULLY
 * ─────────────────────────────────
 * Each pixel is a uint32_t packed as:
 *
 *   Memory (little-endian):  [R byte][G byte][B byte][A byte]
 *                              ↑ lowest address
 *
 *   As a uint32_t (hex):  0xAABBGGRR
 *     R = bits  0–7   (least-significant byte)
 *     G = bits  8–15
 *     B = bits 16–23
 *     A = bits 24–31  (most-significant byte)
 *
 * The macro packs four bytes:
 *   GAME_RGBA(r, g, b, a) = (a << 24) | (b << 16) | (g << 8) | r
 *
 * CONSEQUENCE FOR OPENGL (X11 backend):
 *   glTexImage2D(…, GL_RGBA, GL_UNSIGNED_BYTE, pixels)
 *   OpenGL reads bytes in memory as R, G, B, A — matches our layout.
 *   DO NOT use GL_BGRA here.
 *
 * CONSEQUENCE FOR RAYLIB:
 *   UpdateTexture expects PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 — matches.
 *
 * > Course Note: The reference frogger.h uses FROGGER_RGBA which packs
 * > bytes identically (same formula).  The course renames it GAME_RGBA
 * > for consistency with the Tetris/Snake/Asteroids courses.
 *
 * JS equivalent:
 *   function rgba(r, g, b, a) {
 *     const buf = new Uint8Array([r, g, b, a]);
 *     return new DataView(buf.buffer).getUint32(0, true); // little-endian
 *   }
 */
#define GAME_RGBA(r, g, b, a) \
    ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(g) << 8 | (uint32_t)(r))

#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 255)

/* ══════ Named Colour Constants ═════════════════════════════════════════════
   Named colours prevent bare GAME_RGB(0,0,0) literals scattered through
   render code.  Add game-specific colours in game.h.                     */
#define COLOR_BLACK        GAME_RGBA(  0,   0,   0, 255)
#define COLOR_WHITE        GAME_RGBA(255, 255, 255, 255)
#define COLOR_RED          GAME_RGBA(255,   0,   0, 255)
#define COLOR_GREEN        GAME_RGBA(  0, 255,   0, 255)
#define COLOR_BLUE         GAME_RGBA(  0,   0, 255, 255)
#define COLOR_YELLOW       GAME_RGBA(255, 255,   0, 255)
#define COLOR_CYAN         GAME_RGBA(  0, 255, 255, 255)
#define COLOR_ORANGE       GAME_RGBA(255, 165,   0, 255)
#define COLOR_GREY         GAME_RGBA(128, 128, 128, 255)
#define COLOR_DARK_GREY    GAME_RGBA( 40,  40,  40, 255)

/* Semi-transparent overlays (for DEAD / WIN / GAME OVER backdrops) */
#define COLOR_OVERLAY_DIM  GAME_RGBA(  0,   0,   0, 160)

#endif /* FROGGER_BACKBUFFER_H */
