#ifndef UTILS_RENDER_H
#define UTILS_RENDER_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/render.h — RenderContext, TextCursor, and make_render_context()
 * ═══════════════════════════════════════════════════════════════════════════
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
 * ASTEROIDS USAGE
 * ───────────────
 * world_config.coord_origin = COORD_ORIGIN_LEFT_BOTTOM (y-up Cartesian).
 * world_config.camera_zoom  = 1.0f (no zoom).
 * world_config.camera_x/y  = 0.0f (no pan).
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
 * drawing helpers.
 *
 * Mental model:
 *   WORLD SPACE  (16×12 game units)
 *       ↓  make_render_context() — ONCE per frame
 *   RenderContext  (float scale + offset per axis)
 *       ↓  world_x/y/w/h() or world_rect_px_x/y()
 *   PIXEL SPACE  (backbuffer pixels)
 *       ↓  draw_rect() / draw_text() / draw_line()
 *   BACKBUFFER
 *
 * Fields:
 *   world_to_px_x/y   — pixels per world unit, horizontal / vertical
 *   x_offset, y_offset — world position that maps to pixel 0 on each axis
 *   x_sign, y_sign    — +1 (same direction) or -1 (flipped)
 *   rect_top_wh_mul   — 1.0 (y-up: top edge = wy+wh) or 0.0 (y-down: top = wy)
 *   rect_left_ww_mul  — 1.0 (RTL: left pixel edge = wx+ww) or 0.0 (LTR: wx)
 *   text_scale        — integer glyph multiplier (≥1), stored as float for math
 *   line_height       — pixels between text baselines
 *   px_w, px_h        — backbuffer pixel dimensions (used by
 * world_rect_is_visible)
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
 * ZII: a zero-initialised GameWorldConfig gives COORD_ORIGIN_LEFT_BOTTOM,
 * which is the Asteroids / Cartesian default.
 */
static inline RenderContext make_render_context(const Backbuffer *bb,
                                                GameWorldConfig cfg) {
  RenderContext ctx = {0};

  /* Apply camera zoom before computing the scale factors.
   * zoom=1.0  → same pixels-per-unit as before (no change).
   * zoom=2.0  → 2× more pixels per world unit (zoom in).
   * zoom=0.5  → half the pixels per world unit (zoom out).
   */
  ctx.world_to_px_x = (float)bb->width / GAME_UNITS_W * cfg.camera_zoom;
  ctx.world_to_px_y = (float)bb->height / GAME_UNITS_H * cfg.camera_zoom;

  /* Store backbuffer dimensions for world_rect_is_visible(). */
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
    ctx.x_offset = GAME_UNITS_W;
    ctx.y_offset = GAME_UNITS_H;
    ctx.x_sign = -1.0f;
    ctx.y_sign = -1.0f;
    ctx.rect_top_wh_mul = 1.0f;
    ctx.rect_left_ww_mul = 1.0f;
    break;

  case COORD_ORIGIN_RIGHT_TOP:
    ctx.x_offset = GAME_UNITS_W;
    ctx.y_offset = 0.0f;
    ctx.x_sign = -1.0f;
    ctx.y_sign = 1.0f;
    ctx.rect_top_wh_mul = 0.0f;
    ctx.rect_left_ww_mul = 1.0f;
    break;

  case COORD_ORIGIN_CENTER:
    /* y-up from screen center. (0,0) = center. */
    ctx.x_offset = GAME_UNITS_W * 0.5f;
    ctx.y_offset = GAME_UNITS_H * 0.5f;
    ctx.x_sign = 1.0f;
    ctx.y_sign = -1.0f;
    ctx.rect_top_wh_mul = 1.0f;
    ctx.rect_left_ww_mul = 0.0f;
    break;

  case COORD_ORIGIN_CUSTOM:
    ctx.x_offset = cfg.custom_x_offset;
    ctx.y_offset = cfg.custom_y_offset;
    ctx.x_sign = cfg.custom_x_sign;
    ctx.y_sign = cfg.custom_y_sign;
    ctx.rect_top_wh_mul = (cfg.custom_y_sign < 0.0f) ? 1.0f : 0.0f;
    ctx.rect_left_ww_mul = (cfg.custom_x_sign < 0.0f) ? 1.0f : 0.0f;
    break;
  }

  /* Apply camera pan.
   * x: camera_x shifts which world x maps to pixel 0.
   * y: multiply by y_sign so that "pan up" moves correctly in both y-up
   *    (y_sign=-1) and y-down (y_sign=+1) coordinate systems.             */
  ctx.x_offset -= cfg.camera_x;
  ctx.y_offset -= cfg.camera_y * ctx.y_sign;

  /* Text scale: 1.0 at base resolution (800 px wide, 50 px/unit).
   * Scales proportionally in DYNAMIC modes so glyphs stay legible.
   * Scale is derived from backbuffer WIDTH alone — NOT from camera_zoom.  */
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
