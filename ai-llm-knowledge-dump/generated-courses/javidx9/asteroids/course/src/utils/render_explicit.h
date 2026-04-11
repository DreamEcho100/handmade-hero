#ifndef UTILS_RENDER_EXPLICIT_H
#define UTILS_RENDER_EXPLICIT_H

#include "render.h"  /* RenderContext — no-op if render.h is already processing */

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/render_explicit.h — Explicit coordinate helpers (world → pixel)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 02 — "Explicit" mode: game code calls conversion helpers directly.
 * Pixel math is visible at every draw call site — ideal for teaching the
 * world→pixel boundary.
 *
 * Usage (game.c pattern):
 *
 *   draw_rect(bb,
 *     world_rect_px_x(&ctx, obj.x, obj.w), world_rect_px_y(&ctx, obj.y, obj.h),
 *     world_w(&ctx, obj.w),                world_h(&ctx, obj.h), COLOR_WHITE);
 *
 * All functions are static inline — zero overhead; the compiler sees only
 * float multiply + add after inlining.
 *
 * Do NOT include this file directly — include render.h which auto-includes
 * this file when RENDER_COORD_MODE == 1.
 * ═══════════════════════════════════════════════════════════════════════════
 */


/* ── Axis helpers ────────────────────────────────────────────────────────── */

/* world_x — convert world X position to pixel X.
 * x grows right for LTR origins (x_sign=+1). */
static inline float world_x(const RenderContext *ctx, float wx) {
  return (ctx->x_offset + ctx->x_sign * wx) * ctx->world_to_px_x;
}

/* world_y — convert world Y position to pixel Y.
 * y-up origins (BOTTOM_LEFT) return a flipped pixel Y. */
static inline float world_y(const RenderContext *ctx, float wy) {
  return (ctx->y_offset + ctx->y_sign * wy) * ctx->world_to_px_y;
}

/* world_w — convert world width to pixel width. */
static inline float world_w(const RenderContext *ctx, float ww) {
  return ww * ctx->world_to_px_x;
}

/* world_h — convert world height to pixel height. */
static inline float world_h(const RenderContext *ctx, float wh) {
  return wh * ctx->world_to_px_y;
}

/* ── Rect helpers ─────────────────────────────────────────────────────────
 *
 * draw_rect() expects the TOP-LEFT corner in pixel space.
 *
 * Y AXIS — y-up origins must add wh before flipping to find the screen-top:
 *   y-up  (BOTTOM_LEFT, CENTER): rect_top_wh_mul=1.0 → top_edge = wy + wh
 *   y-down (TOP_LEFT):           rect_top_wh_mul=0.0 → top_edge = wy
 *
 * X AXIS — RTL origins must add ww before flipping to find the screen-left:
 *   LTR (x_sign>0):  rect_left_ww_mul=0.0 → left_edge = wx
 *   RTL (x_sign<0):  rect_left_ww_mul=1.0 → left_edge = wx + ww
 */

/* world_rect_px_x — pixel LEFT edge of a rect at world (wx, ww).
 * For LTR origins (rect_left_ww_mul=0) this is just world_x(wx).
 *
 * Example (Asteroids, BOTTOM_LEFT, wx=4, ww=2, scale=50):
 *   (0 + 1*(4 + 2*0.0)) * 50 = 200 px ✓                                  */
static inline float world_rect_px_x(const RenderContext *ctx, float wx, float ww) {
  return (ctx->x_offset + ctx->x_sign * (wx + ww * ctx->rect_left_ww_mul))
       * ctx->world_to_px_x;
}

/* world_rect_px_y — pixel TOP edge of a rect at world (wy, wh).
 * Handles y-up / y-down automatically via rect_top_wh_mul.
 *
 * Example (Asteroids, BOTTOM_LEFT, wy=4, wh=2, GAME_UNITS_H=12, scale=50):
 *   (12 + (-1)*(4 + 2*1.0)) * 50 = 6 * 50 = 300 px (top edge) ✓         */
static inline float world_rect_px_y(const RenderContext *ctx, float wy, float wh) {
  return (ctx->y_offset + ctx->y_sign * (wy + wh * ctx->rect_top_wh_mul))
       * ctx->world_to_px_y;
}

/* ── Visibility culling ───────────────────────────────────────────────────
 *
 * world_rect_is_visible — returns 1 if any pixel of the world-space rect
 * overlaps the backbuffer; 0 if it is fully off-screen.
 *
 * Call before draw_rect to skip entities that have scrolled outside the
 * viewport.  Works correctly for all coord origins.
 *
 * Example:
 *   if (world_rect_is_visible(&ctx, e.x, e.y, e.w, e.h))
 *     draw_rect(bb, world_rect_px_x(&ctx, e.x, e.w), ...);
 */
static inline int world_rect_is_visible(const RenderContext *ctx,
                                        float wx, float wy,
                                        float ww, float wh) {
  float px = world_rect_px_x(ctx, wx, ww);
  float py = world_rect_px_y(ctx, wy, wh);
  float pw = world_w(ctx, ww);
  float ph = world_h(ctx, wh);
  return (px + pw > 0.0f) && (px < ctx->px_w) &&
         (py + ph > 0.0f) && (py < ctx->px_h);
}

/* ── Origin-aware movement helpers ───────────────────────────────────────
 *
 * camera_pan — pan the camera so content moves in the SCREEN direction.
 *   dx > 0 : content shifts RIGHT  (viewport pans right)
 *   dy > 0 : content shifts UP     (viewport pans up)
 *   y_sign : world_config_y_sign(&world_config)
 */
static inline void camera_pan(float *cam_x, float *cam_y,
                               float dx, float dy,
                               float y_sign, float dt) {
  *cam_x -= dx * dt;
  *cam_y += dy * y_sign * dt;
}

/* screen_move — move a world entity in SCREEN-SPACE direction.
 *   sdx > 0 : entity moves RIGHT on screen  (regardless of x_sign)
 *   sdy > 0 : entity moves UP   on screen   (regardless of y_sign)
 */
static inline void screen_move(float *wx, float *wy,
                                float sdx, float sdy,
                                float x_sign, float y_sign, float dt) {
  *wx += sdx * x_sign * dt;
  *wy -= sdy * y_sign * dt;
}

#endif /* UTILS_RENDER_EXPLICIT_H */
