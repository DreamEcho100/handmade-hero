#ifndef UTILS_BACKBUFFER_H
#define UTILS_BACKBUFFER_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/backbuffer.h — Software Pixel Framebuffer
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 01 — The backbuffer is a plain CPU pixel array.  Every frame:
 *   1. Game code writes pixels into `pixels` using draw_rect / draw_text.
 *   2. Platform uploads the array to the GPU (glTexSubImage2D / UpdateTexture).
 *   3. GPU stretches the texture to fill the window (letterbox).
 *
 * JS analogy: `new Uint32Array(imageData.data.buffer)` from a 2D canvas.
 *
 * PIXEL FORMAT — in-memory [R][G][B][A] = 0xAABBGGRR as uint32_t
 * ────────────────────────────────────────────────────────────────
 * GAME_RGBA packs (r,g,b,a) into a uint32_t.  On a little-endian CPU the
 * in-memory bytes are [R][G][B][A] (lowest address = R).
 *
 *   bits  0– 7 → R  (byte 0, lowest address)
 *   bits  8–15 → G  (byte 1)
 *   bits 16–23 → B  (byte 2)
 *   bits 24–31 → A  (byte 3, highest address)
 *
 * PLATFORM UPLOAD FORMAT
 * ──────────────────────
 *   X11/GLX:  GL_RGBA in glTexImage2D / glTexSubImage2D.
 *             GL_RGBA reads byte[0]=R, byte[1]=G, byte[2]=B, byte[3]=A.
 *             Matches our layout exactly.  (GL_BGRA would swap R↔B → WRONG.)
 *   Raylib:   PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 — same byte order as GL_RGBA.
 *
 * COORDINATE SYSTEM
 * ─────────────────
 * draw_rect() takes pixel coordinates as floats — it is a pure rasterizer that
 * knows nothing about game units.  Coordinate conversion (game units → pixels)
 * happens at the call site, using world_x/y() from render.h / render_explicit.h.
 *
 * COLOR MODEL
 * ───────────
 * draw_rect and draw_text accept float colors [0.0–1.0] per channel (r,g,b,a).
 * draw_line and draw_pixel_w accept packed uint32_t colors (GAME_RGBA format).
 * The COLOR_* float macros work with draw_rect/draw_text.
 * The WF_* packed constants work with draw_line/draw_pixel_w (wireframe drawing).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>

typedef struct {
  uint32_t *pixels;        /* Flat pixel array; platform allocates via malloc. */
  int       width;         /* Canvas width in pixels (GAME_W).                 */
  int       height;        /* Canvas height in pixels (GAME_H).                */
  int       pitch;         /* Bytes per row.  Usually width * bytes_per_pixel. */
  int       bytes_per_pixel; /* 4 for RGBA8888.                                */
} Backbuffer;

/* GAME_RGBA — pack (r,g,b,a) as uint8 values into a uint32_t.
 * In memory on little-endian: [R][G][B][A] at bytes [0][1][2][3].
 * Used for wireframe colors and internal rasterizer packing. */
#define GAME_RGBA(r, g, b, a) \
  ( ((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
    ((uint32_t)(g) <<  8) |  (uint32_t)(r) )

#define GAME_RGB(r, g, b) GAME_RGBA((r), (g), (b), 255)

/* ── Float color constants ───────────────────────────────────────────────────
 * Each macro expands to four float arguments: r, g, b, a  (all [0.0–1.0]).
 * Usage:  draw_rect(bb, x, y, w, h, COLOR_RED);
 *         draw_text(bb, x, y, scale, text, COLOR_WHITE);
 *
 * This "multi-arg macro" pattern keeps the API signature clean (separate float
 * params) while giving named colors for readability.                         */
#define COLOR_BLACK      0.0f,  0.0f,  0.0f,  1.0f
#define COLOR_WHITE      1.0f,  1.0f,  1.0f,  1.0f
#define COLOR_RED        0.86f, 0.20f, 0.20f, 1.0f
#define COLOR_GREEN      0.20f, 0.80f, 0.20f, 1.0f
#define COLOR_BLUE       0.20f, 0.20f, 0.86f, 1.0f
#define COLOR_YELLOW     1.0f,  0.84f, 0.0f,  1.0f
#define COLOR_CYAN       0.0f,  0.85f, 0.85f, 1.0f
#define COLOR_ORANGE     1.0f,  0.55f, 0.0f,  1.0f
#define COLOR_GREY       0.50f, 0.50f, 0.50f, 1.0f
#define COLOR_DARK_GREY  0.25f, 0.25f, 0.25f, 1.0f

/* ── Packed uint32_t colors for wireframe drawing ───────────────────────────
 * Used with draw_line(), draw_pixel_w() — the rasterizer receives packed RGBA.
 * Naming: WF_ prefix = wireframe color (packed, not float multi-arg).       */
#define WF_WHITE   GAME_RGBA(255, 255, 255, 255)
#define WF_CYAN    GAME_RGBA(0,   220, 220, 255)
#define WF_YELLOW  GAME_RGBA(255, 215, 0,   255)
#define WF_RED     GAME_RGBA(220, 50,  50,  255)
#define WF_GREY    GAME_RGBA(128, 128, 128, 255)

/* backbuffer_resize — reallocate pixel buffer for dynamic scaling.
 * Returns 0 on success, -1 on allocation failure.
 * Implementation in utils/backbuffer.c.                                      */
int backbuffer_resize(Backbuffer *backbuffer, int new_width, int new_height);

#endif /* UTILS_BACKBUFFER_H */
