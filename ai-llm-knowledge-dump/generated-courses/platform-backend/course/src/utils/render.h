#ifndef UTILS_RENDER_H
#define UTILS_RENDER_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/render.h — RenderContext, TextCursor, and make_render_context()
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 09 — Explicit coordinate helpers (render_explicit.h).
 *
 * This file provides:
 *   • RenderContext       — world→pixel conversion coefficients, baked
 * once/frame • TextCursor          — pixel-space cursor for flowing text layout
 *   • make_render_context — builds RenderContext from Backbuffer +
 * GameWorldConfig • make_cursor         — converts a world-space position to a
 * pixel TextCursor • cursor_newline      — advances cursor down one text line
 *   • cursor_gap          — advances cursor by a fractional line gap
 *
 * WHY BAKE THE COORD ORIGIN INTO FLOATS?
 * ───────────────────────────────────────
 * The switch in make_render_context() runs ONCE per frame and writes six floats
 * into RenderContext.  Every subsequent draw call is 3–5 float ops with no
 * branch.  The coord_origin choice has zero runtime impact after
 * make_render_context() returns.
 *
 * HH COMPARISON
 * ─────────────
 * Handmade Hero bakes y-flip into the software rasterizer implicitly.
 * This file makes it explicit and configurable — same performance, visible
 * boundary, teachable.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./backbuffer.h"
#include "./base.h"
#include "./draw-text.h" /* for GLYPH_H used in make_render_context */
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * RenderContext
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Precomputed world→pixel coefficients.  Built by make_render_context()
 * from a Backbuffer and GameWorldConfig.  Passed (by pointer) to all
 * Level 1 and Level 2 drawing helpers.
 *
 * Mental model:
 *   WORLD SPACE  (16×12 game units)
 *       ↓  make_render_context() — ONCE per frame
 *   RenderContext  (float scale + offset per axis)
 *       ↓  world_x/y/w/h() or draw_world_rect()
 *   PIXEL SPACE  (backbuffer pixels)
 *       ↓  draw_rect() / draw_text()
 *   BACKBUFFER
 *
 * Fields:
 *   world_to_px_x/y   — pixels per world unit, horizontal / vertical
 *   x_offset, y_offset — world position that maps to pixel 0 on each axis
 *   x_sign, y_sign    — +1 (same direction) or -1 (flipped)
 *   rect_top_wh_mul   — 1.0 (y-up: top edge = wy+wh) or 0.0 (y-down: top = wy)
 *                       eliminates the y-up/y-down branch in world_rect_px_y()
 *   rect_left_ww_mul  — 1.0 (x-left/RTL: left pixel edge = wx+ww) or 0.0 (LTR:
 * wx) symmetric to rect_top_wh_mul; eliminates RTL branch in world_rect_px_x()
 *   text_scale        — integer glyph multiplier (≥1), stored as float for math
 *   line_height       — pixels between text baselines
 *   px_w, px_h        — backbuffer pixel dimensions (used by
 * world_rect_is_visible)
 *
 * LESSON 17 — px_w/px_h added for off-screen culling.
 */
typedef struct RenderContext {
  float world_to_px_x;    /* pixels per world unit, horizontal  */
  float world_to_px_y;    /* pixels per world unit, vertical    */
  float x_offset;         /* world x that maps to pixel x=0     */
  float y_offset;         /* world y that maps to pixel y=0     */
  float x_sign;           /* axis direction: +1 (LTR) or -1 (RTL) */
  float y_sign;           /* axis direction: +1 or -1           */
  float rect_top_wh_mul;  /* 1.0 (y-up) or 0.0 (y-down)        */
  float rect_left_ww_mul; /* 1.0 (RTL)  or 0.0 (LTR)           */
  float text_scale;       /* glyph pixel multiplier (≥1.0)      */
  float line_height;      /* pixels between text baselines      */
  float px_w;             /* backbuffer width in pixels         */
  float px_h;             /* backbuffer height in pixels        */
} RenderContext;

/* ═══════════════════════════════════════════════════════════════════════════
 * TextCursor
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Tracks a text-layout position in PIXEL SPACE (already converted).
 * Create once with make_cursor() passing world coords; all subsequent
 * movement stays in pixels — no repeated world→pixel conversion.
 *
 * cursor_newline / cursor_gap operate in pixel space.
 */
typedef struct TextCursor {
  float x;      /* current X in pixels */
  float y;      /* current Y in pixels */
  float scale;  /* glyph scale (passed to draw_text) */
  float line_h; /* line height in pixels              */
} TextCursor;

/* ═══════════════════════════════════════════════════════════════════════════
 * make_render_context — build RenderContext from backbuffer + coord config
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Call ONCE per frame at the render boundary.  The switch runs once; all
 * subsequent draw calls are branch-free float math.
 *
 * GameWorldConfig cfg is passed by the game layer (via PlatformGameProps).
 * ZII: a zero-initialised GameWorldConfig gives COORD_ORIGIN_LEFT_BOTTOM,
 * which is the Handmade Hero / Cartesian default.
 */
static inline RenderContext make_render_context(const Backbuffer *bb,
                                                GameWorldConfig cfg) {
  RenderContext ctx = {0};

  /* LESSON 17 — Apply camera zoom before computing the scale factors.
   * zoom=1.0  → same pixels-per-unit as before (no change).
   * zoom=2.0  → 2× more pixels per world unit (zoom in, fewer units visible).
   * zoom=0.5  → half the pixels per world unit (zoom out, more units visible).
   */
  ctx.world_to_px_x = (float)bb->width / GAME_UNITS_W * cfg.camera_zoom;
  ctx.world_to_px_y = (float)bb->height / GAME_UNITS_H * cfg.camera_zoom;

  /* LESSON 17 — Store backbuffer dimensions for world_rect_is_visible(). */
  ctx.px_w = (float)bb->width;
  ctx.px_h = (float)bb->height;

  switch (cfg.coord_origin) {
  case COORD_ORIGIN_LEFT_BOTTOM:
    /* y-up, Cartesian. (0,0) = bottom-left.
     * pixel_y = (GAME_UNITS_H - wy) * world_to_px_y                       */
    ctx.x_offset = 0.0f;
    ctx.y_offset = GAME_UNITS_H;
    ctx.x_sign = 1.0f;
    ctx.y_sign = -1.0f;
    ctx.rect_top_wh_mul = 1.0f;  /* top edge = wy + wh */
    ctx.rect_left_ww_mul = 0.0f; /* LTR: left edge = wx */
    break;

  case COORD_ORIGIN_LEFT_TOP:
    /* y-down, screen convention. (0,0) = top-left.
     * pixel_y = wy * world_to_px_y                                         */
    ctx.x_offset = 0.0f;
    ctx.y_offset = 0.0f;
    ctx.x_sign = 1.0f;
    ctx.y_sign = 1.0f;
    ctx.rect_top_wh_mul = 0.0f;  /* top edge = wy */
    ctx.rect_left_ww_mul = 0.0f; /* LTR: left edge = wx */
    break;

  case COORD_ORIGIN_RIGHT_BOTTOM:
    /* RTL + y-up. (0,0) = bottom-right corner.
     * pixel_x = (GAME_UNITS_W - wx) * world_to_px_x
     * pixel_y = (GAME_UNITS_H - wy) * world_to_px_y                       */
    ctx.x_offset = GAME_UNITS_W;
    ctx.y_offset = GAME_UNITS_H;
    ctx.x_sign = -1.0f;
    ctx.y_sign = -1.0f;
    ctx.rect_top_wh_mul = 1.0f;  /* y-up: top edge = wy + wh */
    ctx.rect_left_ww_mul = 1.0f; /* RTL:  left edge = wx + ww */
    break;

  case COORD_ORIGIN_RIGHT_TOP:
    /* RTL + y-down. (0,0) = top-right corner.
     * pixel_x = (GAME_UNITS_W - wx) * world_to_px_x
     * pixel_y = wy * world_to_px_y                                         */
    ctx.x_offset = GAME_UNITS_W;
    ctx.y_offset = 0.0f;
    ctx.x_sign = -1.0f;
    ctx.y_sign = 1.0f;
    ctx.rect_top_wh_mul = 0.0f;  /* y-down: top edge = wy */
    ctx.rect_left_ww_mul = 1.0f; /* RTL:    left edge = wx + ww */
    break;

  case COORD_ORIGIN_CENTER:
    /* y-up from screen center. (0,0) = center.
     * pixel_y = (GAME_UNITS_H/2 - wy) * world_to_px_y                     */
    ctx.x_offset = GAME_UNITS_W * 0.5f;
    ctx.y_offset = GAME_UNITS_H * 0.5f;
    ctx.x_sign = 1.0f;
    ctx.y_sign = -1.0f;
    ctx.rect_top_wh_mul = 1.0f;  /* y-up from center */
    ctx.rect_left_ww_mul = 0.0f; /* LTR: left edge = wx */
    break;

  case COORD_ORIGIN_CUSTOM:
    /* Fully custom axes — supports RTL (x_sign=-1), y-flip, arbitrary origin.
     * rect_left_ww_mul: 1.0 when x_sign < 0 (RTL) so the screen-left edge of
     * a rect is computed from wx+ww (its rightmost world edge), identical to
     * the y-up trick used by rect_top_wh_mul.                               */
    ctx.x_offset = cfg.custom_x_offset;
    ctx.y_offset = cfg.custom_y_offset;
    ctx.x_sign = cfg.custom_x_sign;
    ctx.y_sign = cfg.custom_y_sign;
    ctx.rect_top_wh_mul = (cfg.custom_y_sign < 0.0f) ? 1.0f : 0.0f;
    ctx.rect_left_ww_mul = (cfg.custom_x_sign < 0.0f) ? 1.0f : 0.0f;
    break;
  }

  /* LESSON 17 — Apply camera pan.
   * The formula that works for all origins (derived from axis math):
   *   x: camera_x shifts which world x maps to pixel 0.
   *   y: multiply by y_sign so that "pan up" moves correctly in both y-up
   *      (y_sign=-1) and y-down (y_sign=+1) coordinate systems.
   *
   * Example — BOTTOM_LEFT (y_offset=12, y_sign=-1), camera_y=1:
   *   y_offset = 12 - (1 * -1) = 13  →  world y=1 maps to pixel 600 ✓
   * Example — TOP_LEFT (y_offset=0, y_sign=+1), camera_y=1:
   *   y_offset = 0 - (1 * 1) = -1    →  world y=1 maps to pixel 0 ✓      */
  ctx.x_offset -= cfg.camera_x;
  ctx.y_offset -= cfg.camera_y * ctx.y_sign;

  /* Text scale: 1.0 at base resolution (GAME_W px wide = 50 px/unit at
   * 800×600). Scales proportionally in DYNAMIC modes so glyphs stay legible at
   * any size. scale_factor is derived from backbuffer WIDTH alone — NOT from
   * camera_zoom.
   *
   * HUD zoom-independence: to keep HUD text the same apparent size regardless
   * of camera zoom, callers build a separate hud_ctx with camera_zoom=1.0f.
   * That ctx has no camera zoom, so text_scale is unaffected by world zoom. */
  float base = (float)GAME_W / GAME_UNITS_W; /* 50.0 at 800 px      */
  float scale_factor = ((float)bb->width / GAME_UNITS_W) / base;
  ctx.text_scale = (scale_factor < 1.0f) ? 1.0f : scale_factor;
  ctx.line_height = (float)GLYPH_H * ctx.text_scale * 1.4f;

  return ctx;
}

/* ── TextCursor helpers ────────────────────────────────────────────────────
 *
 * make_cursor inlines the conversion math directly rather than delegating
 * to world_x/y() from render_explicit.h — avoids a forward-declaration
 * dependency while producing identical code after inlining.
 */

/* make_cursor — convert a world position ONCE → pixel TextCursor. */
static inline TextCursor make_cursor(const RenderContext *ctx, float wx,
                                     float wy) {
  return (TextCursor){
      .x = (ctx->x_offset + ctx->x_sign * wx) * ctx->world_to_px_x,
      .y = (ctx->y_offset + ctx->y_sign * wy) * ctx->world_to_px_y,
      .scale = ctx->text_scale,
      .line_h = ctx->line_height,
  };
}

/* cursor_newline — advance cursor down one text line (pixel space). */
static inline void cursor_newline(TextCursor *c) { c->y += c->line_h; }

/* cursor_gap — advance cursor by `mul` lines (e.g. 0.5 = half-line gap). */
static inline void cursor_gap(TextCursor *c, float mul) {
  c->y += c->line_h * mul;
}

#ifndef RENDER_COORD_MODE
#define RENDER_COORD_MODE 1
#endif

#if RENDER_COORD_MODE == 1
#include "render_explicit.h"
#endif

#endif /* UTILS_RENDER_H */
