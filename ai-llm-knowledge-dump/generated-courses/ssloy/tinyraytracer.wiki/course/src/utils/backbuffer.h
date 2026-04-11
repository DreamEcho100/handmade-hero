#ifndef UTILS_BACKBUFFER_H
#define UTILS_BACKBUFFER_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/backbuffer.h — TinyRaytracer Course
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
 *
 * COORDINATE SYSTEM
 * ─────────────────
 * draw_rect() takes pixel coordinates as floats — it is a pure rasterizer
 * that knows nothing about game units or coordinate systems.
 *
 * Game code defines a unit system (e.g., GAME_UNITS_W × GAME_UNITS_H) and
 * computes a scale factor (units_to_pixels).  Conversion happens at the
 * CALL SITE, like Handmade Hero's meters_to_pixels pattern:
 *   draw_rect(backbuffer, x * u2p, y * u2p, w * u2p, h * u2p, r, g, b, a);
 *
 * COLOR MODEL
 * ───────────
 * All drawing functions accept float colors in [0.0–1.0] per channel (r,g,b,a).
 * Alpha blending is handled automatically — no separate "blend" function needed.
 *   a >= 1.0  → opaque fast path (direct pixel write)
 *   a <= 0.0  → fully transparent (skip entirely)
 *   otherwise → per-pixel alpha composite: out = src*a + dst*(1-a)
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

/* GAME_RGBA — pack (r,g,b,a) as uint8 values into a uint32_t (0xAARRGGBB).
 * In memory on little-endian: [R][G][B][A] at bytes [0][1][2][3].
 * Used internally by the rasterizer; game code should use float colors. */
#define GAME_RGBA(r, g, b, a) \
  ( ((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
    ((uint32_t)(g) <<  8) |  (uint32_t)(r) )

#define GAME_RGB(r, g, b) GAME_RGBA((r), (g), (b), 255)

/* ── Float color constants ────────────────────────────────────────────────
 * Each macro expands to four float arguments: r, g, b, a  (all [0.0–1.0]).
 * Usage:  draw_rect(backbuffer, 0, 0, width, height, COLOR_RED);
 *
 * This "multi-arg macro" pattern keeps the API signature clean (separate
 * float params like Handmade Hero) while giving named colors for readability.
 * ──────────────────────────────────────────────────────────────────────── */
#define COLOR_BLACK      0.0f,   0.0f,   0.0f,   1.0f
#define COLOR_WHITE      1.0f,   1.0f,   1.0f,   1.0f
#define COLOR_RED        0.863f, 0.196f, 0.196f, 1.0f
#define COLOR_GREEN      0.196f, 0.804f, 0.196f, 1.0f
#define COLOR_BLUE       0.196f, 0.196f, 0.863f, 1.0f
#define COLOR_YELLOW     1.0f,   0.843f, 0.0f,   1.0f
#define COLOR_CYAN       0.0f,   0.863f, 0.863f, 1.0f
#define COLOR_MAGENTA    0.863f, 0.0f,   0.863f, 1.0f
#define COLOR_ORANGE     1.0f,   0.549f, 0.0f,   1.0f
#define COLOR_GRAY       0.502f, 0.502f, 0.502f, 1.0f
#define COLOR_DARK_GRAY  0.251f, 0.251f, 0.251f, 1.0f
#define COLOR_BG         0.078f, 0.078f, 0.118f, 1.0f

int backbuffer_resize(Backbuffer *backbuffer, int new_width, int new_height);

#endif /* UTILS_BACKBUFFER_H */
