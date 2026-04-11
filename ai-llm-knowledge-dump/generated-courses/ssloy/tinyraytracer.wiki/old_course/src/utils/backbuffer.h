#ifndef UTILS_BACKBUFFER_H
#define UTILS_BACKBUFFER_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/backbuffer.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 03 — Backbuffer struct, pitch, GAME_RGB/RGBA macro, color constants.
 *
 * The backbuffer is a plain CPU pixel array. Every frame:
 *   1. Game code writes pixels into `pixels` using draw_rect / draw_text.
 *   2. Platform uploads the array to the GPU (glTexSubImage2D / UpdateTexture).
 *   3. GPU stretches the texture to fill the window (letterbox in L08).
 *
 * JS analogy: `new Uint32Array(imageData.data.buffer)` from a 2D canvas.
 *
 * PIXEL FORMAT — in-memory [R][G][B][A] = 0xAARRGGBB as uint32_t
 * ────────────────────────────────────────────────────────────────
 * GAME_RGB/RGBA pack (r,g,b,a) into a uint32_t.  On a little-endian CPU the
 * in-memory bytes are [R][G][B][A] (lowest address = R).
 *
 * The integer value is 0xAARRGGBB:
 *   bits  0– 7 → R  (byte 0, lowest address)
 *   bits  8–15 → G  (byte 1)
 *   bits 16–23 → B  (byte 2)
 *   bits 24–31 → A  (byte 3, highest address)
 *
 * PLATFORM UPLOAD FORMAT
 * ──────────────────────
 * Both backends read our memory layout as [R][G][B][A] — no swap needed:
 *
 *   X11/GLX:  GL_RGBA in glTexImage2D / glTexSubImage2D.
 *             GL_RGBA reads byte[0]=R, byte[1]=G, byte[2]=B, byte[3]=A.
 *             Matches our layout exactly.  (GL_BGRA would swap R↔B → WRONG.)
 *
 *   Raylib:   PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 in UpdateTexture.
 *             Same byte interpretation as GL_RGBA — also correct.
 *
 * Both platforms use the SAME in-memory layout and the SAME upload format.
 * There is NO R↔B swap needed in this course.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>

typedef struct {
  uint32_t *pixels;    /* Flat pixel array; platform allocates via malloc.   */
  int       width;     /* Canvas width in pixels (GAME_W).                   */
  int       height;    /* Canvas height in pixels (GAME_H).                  */
  int       pitch;     /* Bytes per row.  Usually width * bytes_per_pixel.   */
                       /* Use pitch, not hardcoded width*4, for correctness. */
  int       bytes_per_pixel; /* 4 for RGBA8888.                              */
} Backbuffer;

/* GAME_RGBA — pack (r,g,b,a) into a uint32_t (0xAARRGGBB on little-endian).
 * In memory on little-endian: [R][G][B][A] at bytes [0][1][2][3].
 *
 * LESSON 03 — why the macro packs in this order, and why BOTH platforms
 * use GL_RGBA / PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 with no extra swap. */
#define GAME_RGBA(r, g, b, a) \
  ( ((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
    ((uint32_t)(g) <<  8) |  (uint32_t)(r) )

#define GAME_RGB(r, g, b) GAME_RGBA((r), (g), (b), 255)

/* Named color constants — add more as needed for each game course. */
#define COLOR_BLACK      GAME_RGB(  0,   0,   0)
#define COLOR_WHITE      GAME_RGB(255, 255, 255)
#define COLOR_RED        GAME_RGB(220,  50,  50)
#define COLOR_GREEN      GAME_RGB( 50, 205,  50)
#define COLOR_BLUE       GAME_RGB( 50,  50, 220)
#define COLOR_YELLOW     GAME_RGB(255, 215,   0)
#define COLOR_CYAN       GAME_RGB(  0, 220, 220)
#define COLOR_MAGENTA    GAME_RGB(220,   0, 220)
#define COLOR_ORANGE     GAME_RGB(255, 140,   0)
#define COLOR_GRAY       GAME_RGB(128, 128, 128)
#define COLOR_DARK_GRAY  GAME_RGB( 64,  64,  64)
#define COLOR_BG         GAME_RGB( 20,  20,  30)  /* Default canvas background */

#endif /* UTILS_BACKBUFFER_H */
