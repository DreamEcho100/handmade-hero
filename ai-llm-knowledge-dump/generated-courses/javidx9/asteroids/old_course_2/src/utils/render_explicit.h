#ifndef UTILS_RENDER_EXPLICIT_H
#define UTILS_RENDER_EXPLICIT_H

#include "render.h"  /* RenderContext — no-op if render.h already included */

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/render_explicit.h — Explicit coordinate helpers
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 04 — "Explicit" mode: game code calls conversion helpers directly.
 * Pixel math is visible at every draw call site — ideal for teaching the
 * world→pixel boundary.
 *
 * Usage (Asteroids pattern):
 *
 *   // Convert wireframe vertex from game units to pixel coords:
 *   int px = (int)world_x(&ctx, vertex.x);
 *   int py = (int)world_y(&ctx, vertex.y);
 *   draw_line(bb, prev_px, prev_py, px, py, WF_WHITE);
 *
 *   // Convert HUD element:
 *   TextCursor cur = make_cursor(&hud_ctx, 0.25f, HUD_TOP_Y(0.25f));
 *   draw_text(bb, cur.x, cur.y, (int)cur.scale, "SCORE: 0", COLOR_WHITE);
 *
 * Every call site tells you: "I am converting world→pixels here."
 * The boundary is explicit — the same philosophy as Handmade Hero's
 * meters_to_pixels pattern, but configurable per CoordOrigin.
 *
 * All functions are static inline — zero overhead; the compiler sees only
 * float multiply + add after inlining.
 *
 * Do NOT include this file directly — include render.h which selects the
 * right mode based on RENDER_COORD_MODE.
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* ── Axis helpers ────────────────────────────────────────────────────────── */

/* world_x — convert world X position to pixel X. */
static inline float world_x(const RenderContext *ctx, float wx) {
  return (ctx->x_offset + ctx->x_sign * wx) * ctx->world_to_px_x;
}

/* world_y — convert world Y position to pixel Y.
 * y-up origins return a flipped pixel Y (bottom of screen = large pixel_y). */
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

/* ── Rect helpers ──────────────────────────────────────────────────────────
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

/* world_rect_px_x — pixel LEFT edge of a rect at world (wx, ww). */
static inline float world_rect_px_x(const RenderContext *ctx, float wx, float ww) {
  return (ctx->x_offset + ctx->x_sign * (wx + ww * ctx->rect_left_ww_mul))
       * ctx->world_to_px_x;
}

/* world_rect_px_y — pixel TOP edge of a rect at world (wy, wh). */
static inline float world_rect_px_y(const RenderContext *ctx, float wy, float wh) {
  return (ctx->y_offset + ctx->y_sign * (wy + wh * ctx->rect_top_wh_mul))
       * ctx->world_to_px_y;
}

/* ── Visibility culling ───────────────────────────────────────────────────
 *
 * world_rect_is_visible — returns 1 if any pixel of the world-space rect
 * overlaps the backbuffer; 0 if it is fully off-screen.
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

#endif /* UTILS_RENDER_EXPLICIT_H */
