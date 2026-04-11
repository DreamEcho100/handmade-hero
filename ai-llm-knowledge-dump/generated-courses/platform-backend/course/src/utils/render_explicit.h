#ifndef UTILS_RENDER_EXPLICIT_H
#define UTILS_RENDER_EXPLICIT_H

#include "render.h"  /* RenderContext — no-op if render.h is already processing */

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/render_explicit.h — Level 1: Explicit coordinate helpers
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 09 — "Explicit" mode: game code calls conversion helpers directly.
 * Pixel math is visible at every draw call site — ideal for teaching the
 * world→pixel boundary.
 *
 * Usage (demo_explicit.c pattern):
 *
 *   draw_rect(bb,
 *     world_rect_px_x(&ctx, 1.0f, 4.0f), world_rect_px_y(&ctx, 4.0f, 2.0f),
 *     world_w(&ctx, 4.0f),               world_h(&ctx, 2.0f), COLOR_RED);
 *
 * Every draw call tells you: "I am converting world→pixels here."
 * The boundary is explicit — the same philosophy as Handmade Hero's
 * meters_to_pixels, but configurable per CoordOrigin.
 *
 * All functions are static inline — zero overhead; the compiler sees only
 * float multiply + add after inlining.
 *
 * Do NOT include this file directly — include render.h which selects the
 * right level based on RENDER_COORD_MODE.
 * ═══════════════════════════════════════════════════════════════════════════
 */


/* ── Axis helpers ────────────────────────────────────────────────────────── */

/* world_x — convert world X position to pixel X.
 * x grows right for all standard origins. */
static inline float world_x(const RenderContext *ctx, float wx) {
  return (ctx->x_offset + ctx->x_sign * wx) * ctx->world_to_px_x;
}

/* world_y — convert world Y position to pixel Y.
 * The direction depends on origin: y-up origins return a flipped pixel Y. */
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
 *
 * Both use the same trick: multiply the size by a 0/1 precomputed flag so
 * there is NO branch in the hot path — identical structure, both axes.
 */

/* world_rect_px_x — pixel LEFT edge of a rect at world (wx, ww).
 * For LTR origins (rect_left_ww_mul=0) this is just world_x(wx).
 * For RTL origins (rect_left_ww_mul=1) this uses wx+ww as the screen-left.
 *
 * Example: CUSTOM RTL, x_offset=16, x_sign=-1, wx=1, ww=4, scale=50:
 *   (16 + (-1)*(1 + 4*1.0)) * 50 = (16-5)*50 = 550  ← pixel x of left edge ✓ */
static inline float world_rect_px_x(const RenderContext *ctx, float wx, float ww) {
  return (ctx->x_offset + ctx->x_sign * (wx + ww * ctx->rect_left_ww_mul))
       * ctx->world_to_px_x;
}

/* world_rect_px_y — pixel TOP edge of a rect at world (wy, wh).
 * Handles y-up / y-down automatically via rect_top_wh_mul.
 *
 * Example: BOTTOM_LEFT, wy=4.0, wh=2.0, GAME_UNITS_H=12, scale=50:
 *   (12 + (-1)*(4 + 2*1.0)) * 50 = 6 * 50 = 300  ← pixel y of top edge ✓ */
static inline float world_rect_px_y(const RenderContext *ctx, float wy, float wh) {
  return (ctx->y_offset + ctx->y_sign * (wy + wh * ctx->rect_top_wh_mul))
       * ctx->world_to_px_y;
}

/* ── Visibility culling ───────────────────────────────────────────────────
 *
 * world_rect_is_visible — returns 1 if any pixel of the world-space rect
 * overlaps the backbuffer; 0 if it is fully off-screen.
 *
 * LESSON 17 — Call before draw_rect / draw_world_rect to skip entities
 * that have scrolled or zoomed outside the viewport.  Works correctly for
 * all coord origins and with camera pan/zoom because it uses the same
 * world_rect_px_* helpers as the draw calls themselves.
 *
 * Uses ctx->px_w / ctx->px_h set by make_render_context() — no extra args.
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

/* ── Origin-aware movement helpers — LESSON 17 ───────────────────────────────
 *
 * These two helpers absorb the x_sign / y_sign bookkeeping so that "press
 * right" and "press up" behave correctly regardless of which CoordOrigin is
 * active.  Both are static inline — zero overhead after inlining.
 *
 * WHY THE SIGNS DIFFER BY AXIS
 * ─────────────────────────────
 * The pixel formula is identical on both axes:
 *   pixel_x = (x_offset - cam_x + x_sign * wx) * sx
 *   pixel_y = (y_offset - cam_y*y_sign + y_sign * wy) * sy
 *
 * In pixel space, "decrease" = LEFT on X but = UP on Y.  So the same
 * "+= speed" applied to both camera fields produces OPPOSITE visible
 * directions.  Multiplying by the axis sign normalises this in one place.
 *
 *   cam_x -= dx*dt     →  pixel_x increases → content RIGHT  ✓  (all x_signs)
 *   cam_y += dy*y_sign*dt:
 *     y_sign=+1 (TOP_LEFT):    cam_y↑ → pixel_y↓ → content UP  ✓
 *     y_sign=-1 (BOTTOM_LEFT): cam_y↓ → y_offset↓ → pixel_y↓ → content UP  ✓
 *
 * Obtain y_sign from world_config_y_sign(&world_config) (utils/base.h).
 * Obtain x_sign from world_config_x_sign(&world_config) or ctx->x_sign.
 */

/* camera_pan — pan the camera so content moves in the SCREEN direction.
 *   dx > 0 : content shifts RIGHT  (viewport pans right)
 *   dy > 0 : content shifts UP     (viewport pans up)
 *   y_sign : world_config_y_sign(&world_config)
 *
 * Example (panning in response to input):
 *   float cam_dx = (right_held ? CAM_PAN_SPEED : 0.0f) - (left_held ? CAM_PAN_SPEED : 0.0f);
 *   float cam_dy = (up_held    ? CAM_PAN_SPEED : 0.0f) - (down_held ? CAM_PAN_SPEED : 0.0f);
 *   camera_pan(&g_camera.x, &g_camera.y, cam_dx, cam_dy,
 *              world_config_y_sign(&world_config), dt);                     */
static inline void camera_pan(float *cam_x, float *cam_y,
                               float dx, float dy,
                               float y_sign, float dt) {
  *cam_x -= dx * dt;
  *cam_y += dy * y_sign * dt;
}

/* screen_move — move a world entity in SCREEN-SPACE direction.
 *   sdx > 0 : entity moves RIGHT on screen  (regardless of x_sign)
 *   sdy > 0 : entity moves UP   on screen   (regardless of y_sign)
 *   x_sign, y_sign: world_config_x/y_sign(&world_config) or ctx->x_sign/y_sign
 *
 * Use this for INPUT-DRIVEN entity movement (player pressing arrow keys).
 * For PHYSICS / AI movement (gravity, pathfinding) use plain world-space:
 *   entity.x += speed * dt;   ← always in world logic, origin-agnostic
 *
 * Example (player character controlled by arrow keys):
 *   float x_sign = world_config_x_sign(&world_config);
 *   float y_sign = world_config_y_sign(&world_config);
 *   screen_move(&player.x, &player.y,
 *               (right_held ? SPEED : left_held ? -SPEED : 0.0f),
 *               (up_held    ? SPEED : down_held ? -SPEED : 0.0f),
 *               x_sign, y_sign, dt);                                        */
static inline void screen_move(float *wx, float *wy,
                                float sdx, float sdy,
                                float x_sign, float y_sign, float dt) {
  *wx += sdx * x_sign * dt;
  *wy -= sdy * y_sign * dt;
}

#endif /* UTILS_RENDER_EXPLICIT_H */
