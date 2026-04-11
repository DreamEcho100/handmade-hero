/* =============================================================================
 * utils/draw-shapes.h — Primitive Drawing (Points, Lines, Rects)
 * =============================================================================
 *
 * Declarations for the five low-level drawing primitives used throughout
 * the Asteroids renderer.
 *
 * TOROIDAL vs CLIPPED:
 *   draw_pixel_w  — TOROIDAL: wraps across screen edges.  Used for all
 *                   wireframe geometry (ship, bullets, asteroids) so that
 *                   objects cross screen boundaries seamlessly.
 *   draw_pixel    — CLIPPED: does nothing if (x, y) is off-screen.
 *   draw_line     — calls draw_pixel_w internally → lines wrap.
 *   draw_rect     — CLIPPED: used for HUD / overlay boxes that must stay
 *                   on screen.  Supports three-tier alpha handling:
 *                     a <= 0  → skip entirely (fully transparent)
 *                     a >= 1  → direct pixel write (opaque fast path)
 *                     else    → per-pixel alpha composite
 *
 * Why the split?  Toroidal wrap is correct for game objects (an asteroid that
 * exits the right edge should appear from the left).  But a "dim overlay"
 * rect covering the full screen should not wrap — it should just fill.
 * =============================================================================
 */

#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H

#include <stdint.h>
#include "backbuffer.h"

/* ── Toroidal pixel write ─────────────────────────────────────────────────
   Wraps x and y into [0, width) and [0, height) using modular arithmetic.
   This makes game objects seamlessly cross the screen edges.             */
void draw_pixel_w(Backbuffer *bb, int x, int y, uint32_t color);

/* ── Clipped pixel write ──────────────────────────────────────────────────
   Writes the pixel only if (x, y) is within the backbuffer bounds.
   Silent no-op for out-of-bounds coordinates.                            */
void draw_pixel(Backbuffer *bb, int x, int y, uint32_t color);

/* ── Bresenham line (toroidal) ────────────────────────────────────────────
   Draws a straight line using Bresenham's algorithm.  Each pixel is written
   via draw_pixel_w so long lines that cross the screen edge wrap around.  */
void draw_line(Backbuffer *bb, int x0, int y0, int x1, int y1, uint32_t color);

/* ── Clipped filled rectangle ─────────────────────────────────────────────
 *
 * LESSON 02 — Three-tier alpha handling (like Handmade Hero):
 *   1. a <= 0  → skip entirely (fully transparent)
 *   2. a >= 1  → direct pixel write (opaque fast path, no read-back)
 *   3. else    → per-pixel alpha composite: out = src*a + dst*(1-a)
 *
 * Float colors [0.0–1.0] are converted to 8-bit at rasterization time.
 * Float positions allow sub-pixel precision; truncation to int happens here.
 *
 * (x, y) is the top-left corner in PIXEL SPACE; w×h is width×height.
 * Coordinate conversion (game units → pixels) happens at the call site.  */
void draw_rect(Backbuffer *bb,
               float x, float y, float w, float h,
               float r, float g, float b, float a);

#endif /* UTILS_DRAW_SHAPES_H */
