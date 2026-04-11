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
 *                   on screen.
 *   draw_rect_blend — same as draw_rect but alpha-blends with the existing
 *                   pixel (used for the dim overlay behind GAME OVER text).
 *
 * Why the split?  Toroidal wrap is correct for game objects (an asteroid that
 * exits the right edge should appear from the left).  But a "dim overlay"
 * rect covering the full screen should not wrap — it should just fill.
 * =============================================================================
 */

#ifndef ASTEROIDS_DRAW_SHAPES_H
#define ASTEROIDS_DRAW_SHAPES_H

#include <stdint.h>
#include "backbuffer.h"

/* ── Toroidal pixel write ─────────────────────────────────────────────────
   Wraps x and y into [0, width) and [0, height) using modular arithmetic.
   This makes game objects seamlessly cross the screen edges.             */
void draw_pixel_w(AsteroidsBackbuffer *bb, int x, int y, uint32_t color);

/* ── Clipped pixel write ──────────────────────────────────────────────────
   Writes the pixel only if (x, y) is within the backbuffer bounds.
   Silent no-op for out-of-bounds coordinates.                            */
void draw_pixel(AsteroidsBackbuffer *bb, int x, int y, uint32_t color);

/* ── Bresenham line (toroidal) ────────────────────────────────────────────
   Draws a straight line using Bresenham's algorithm.  Each pixel is written
   via draw_pixel_w so long lines that cross the screen edge wrap around.  */
void draw_line(AsteroidsBackbuffer *bb, int x0, int y0, int x1, int y1, uint32_t color);

/* ── Clipped filled rectangle ─────────────────────────────────────────────
   Draws a solid-colour rectangle clamped to the backbuffer bounds.
   (x, y) is the top-left corner; w×h is width×height in pixels.         */
void draw_rect(AsteroidsBackbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* ── Alpha-blended rectangle ──────────────────────────────────────────────
   Like draw_rect but the colour's alpha channel (bits 24-31) controls
   how much it covers the existing pixel:
     alpha=255  → fully opaque (same as draw_rect)
     alpha=128  → 50% blend
     alpha=0    → no-op (destination unchanged)
   Used for the translucent overlay behind GAME OVER text.               */
void draw_rect_blend(AsteroidsBackbuffer *bb, int x, int y, int w, int h, uint32_t color);

#endif /* ASTEROIDS_DRAW_SHAPES_H */
