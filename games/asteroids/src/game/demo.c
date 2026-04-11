#include "./demo.h"
#include "../utils/backbuffer.h"
#include "../utils/base.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include <stdio.h>

/* ── World Space Definition ─────────────────────────────────────────── */
#define WORLD_W 16.0f
#define WORLD_H 12.0f

/* ── Render Context: Conversion Factors ─────────────────────────────── */
typedef struct {
  // /* World → Pixels */
  // float world_to_px_x;
  // float world_to_px_y;

  // /* Text rendering (already in pixels) */
  // float text_scale;
  // float line_height; /* pixels */

  float world_to_px_x;    /* pixels per world unit, horizontal  */
  float world_to_px_y;    /* pixels per world unit, vertical    */
  float x_offset;         /* world x that maps to pixel x=0    */
  float y_offset;         /* world y that maps to pixel y=0    */
  float x_sign;           /* +1 (LTR) or -1 (RTL)             */
  float y_sign;           /* +1 (y-down) or -1 (y-up)         */
  float rect_top_wh_mul;  /* 1.0 (y-up) or 0.0 (y-down)       */
  float rect_left_ww_mul; /* 1.0 (RTL)  or 0.0 (LTR)          */
  float text_scale;       /* glyph pixel multiplier (≥1.0)     */
  float line_height;      /* pixels between text baselines     */
} RenderContext;

static inline RenderContext make_render_context(const Backbuffer *bb,
                                                GameWorldConfig cfg) {
  RenderContext ctx = {0};
  ctx.world_to_px_x = (float)bb->width / GAME_UNITS_W;
  ctx.world_to_px_y = (float)bb->height / GAME_UNITS_H;

  switch (cfg.coord_origin) {
  case COORD_ORIGIN_LEFT_BOTTOM:
    ctx.x_offset = 0.0f;
    ctx.y_offset = GAME_UNITS_H;
    ctx.x_sign = 1.0f;
    ctx.y_sign = -1.0f;
    ctx.rect_top_wh_mul = 1.0f;  /* y-up: top_edge = wy + wh  */
    ctx.rect_left_ww_mul = 0.0f; /* LTR: left_edge = wx       */
    break;
  case COORD_ORIGIN_LEFT_TOP:
    ctx.x_offset = 0.0f;
    ctx.y_offset = 0.0f;
    ctx.x_sign = 1.0f;
    ctx.y_sign = 1.0f;
    ctx.rect_top_wh_mul = 0.0f; /* y-down: top_edge = wy     */
    ctx.rect_left_ww_mul = 0.0f;
    break;
  case COORD_ORIGIN_RIGHT_BOTTOM: /* RTL + y-up  */
    ctx.x_offset = GAME_UNITS_W;
    ctx.y_offset = GAME_UNITS_H;
    ctx.x_sign = -1.0f;
    ctx.y_sign = -1.0f;
    ctx.rect_top_wh_mul = 1.0f;  /* y-up:  top_edge  = wy + wh  */
    ctx.rect_left_ww_mul = 1.0f; /* RTL:   left_edge = wx + ww  */
    break;
  case COORD_ORIGIN_RIGHT_TOP: /* RTL + y-down */
    ctx.x_offset = GAME_UNITS_W;
    ctx.y_offset = 0.0f;
    ctx.x_sign = -1.0f;
    ctx.y_sign = 1.0f;
    ctx.rect_top_wh_mul = 0.0f;  /* y-down: top_edge  = wy      */
    ctx.rect_left_ww_mul = 1.0f; /* RTL:    left_edge = wx + ww */
    break;
  case COORD_ORIGIN_CENTER:
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

  /* Text scale: 1.0 at base resolution; scales up for larger backbuffers. */
  float base = (float)GAME_W / GAME_UNITS_W; /* 50.0 at 800 px */
  float scale_factor = ctx.world_to_px_x / base;
  ctx.text_scale = (scale_factor < 1.0f) ? 1.0f : scale_factor;
  ctx.line_height = (float)GLYPH_H * ctx.text_scale * 1.4f;
  return ctx;
}

/* ── Conversion Helpers (World → Pixels) ────────────────────────────── */
static inline float world_x(RenderContext *ctx, float x) {
  return x * ctx->world_to_px_x;
}

static inline float world_y(RenderContext *ctx, float y) {
  return y * ctx->world_to_px_y;
}

static inline float world_w(RenderContext *ctx, float w) {
  return w * ctx->world_to_px_x;
}

static inline float world_h(RenderContext *ctx, float h) {
  return h * ctx->world_to_px_y;
}

/* ── Text Cursor: Tracks Position in Pixels ─────────────────────────── */
typedef struct {
  float x;      /* current X in pixels */
  float y;      /* current Y in pixels */
  float scale;  /* text scale */
  float line_h; /* line height in pixels */
} TextCursor;

/* Create cursor from world position - converts ONCE */
static TextCursor make_cursor(RenderContext *ctx, float wx, float wy) {
  return (TextCursor){.x = world_x(ctx, wx),
                      .y = world_y(ctx, wy),
                      .scale = ctx->text_scale,
                      .line_h = ctx->line_height};
}

/* Movement in pixels - no conversion needed */
static void cursor_newline(TextCursor *c) { c->y += c->line_h; }

static void cursor_gap(TextCursor *c, float multiplier) {
  c->y += c->line_h * multiplier;
}

/* ── Helper Functions ───────────────────────────────────────────────── */
static const char *get_mode_name(WindowScaleMode mode) {
  switch (mode) {
  case WINDOW_SCALE_MODE_FIXED:
    return "FIXED (800x600 + GPU scale)";
  case WINDOW_SCALE_MODE_DYNAMIC_MATCH:
    return "DYNAMIC (match window)";
  case WINDOW_SCALE_MODE_DYNAMIC_ASPECT:
    return "DYNAMIC (preserve 4:3)";
  default:
    return "UNKNOWN";
  }
}

/* ── Main Render Function ───────────────────────────────────────────── */
void demo_render(Backbuffer *backbuffer, GameInput *input,
                 GameWorldConfig world_config, WindowScaleMode scale_mode,
                 int fps) {
  RenderContext ctx = make_render_context(backbuffer, world_config);

  /* ── Background (full backbuffer) ─────────────────────────────────── */
  draw_rect(backbuffer, 0, 0, (float)backbuffer->width,
            (float)backbuffer->height, COLOR_BG);

  /* ── Game Objects (world space → convert at render) ───────────────── */
  /* Red: position (1,1), size (4,2) in world units */
  draw_rect(backbuffer, world_x(&ctx, 1.0f), world_y(&ctx, 1.0f),
            world_w(&ctx, 4.0f), world_h(&ctx, 2.0f), COLOR_RED);

  /* Green: position (6,1), size (4,2) */
  draw_rect(backbuffer, world_x(&ctx, 6.0f), world_y(&ctx, 1.0f),
            world_w(&ctx, 4.0f), world_h(&ctx, 2.0f), COLOR_GREEN);

  /* Blue: position (11,1), size (4,2) */
  draw_rect(backbuffer, world_x(&ctx, 11.0f), world_y(&ctx, 1.0f),
            world_w(&ctx, 4.0f), world_h(&ctx, 2.0f), COLOR_BLUE);

  /* Yellow overlay: position (3,0.6), size (4,2.8), 47% alpha */
  draw_rect(backbuffer, world_x(&ctx, 3.0f), world_y(&ctx, 0.6f),
            world_w(&ctx, 4.0f), world_h(&ctx, 2.8f), 1.0f, 0.843f, 0.0f,
            0.47f);

  /* ── Text UI (world position → then pixel layout) ─────────────────── */
  TextCursor text = make_cursor(&ctx, 1.0f, 4.0f);
  float large_scale = ctx.text_scale * 2.0f;

  /* Title - large text */
  draw_text(backbuffer, text.x, text.y, large_scale, "PLATFORM READY",
            COLOR_WHITE);
  text.y += GLYPH_H * large_scale * 1.4f; /* large line height */

  /* Input feedback */
  if (input && input->buttons.play_tone.ended_down) {
    float highlight_h = GLYPH_H * text.scale * 1.2f;
    draw_rect(backbuffer, text.x, text.y, world_w(&ctx, 6.0f), highlight_h,
              COLOR_YELLOW);
    draw_text(backbuffer, text.x + 4.0f, text.y + 2.0f, text.scale,
              "SPACE: play tone", COLOR_BLACK);
  } else {
    draw_text(backbuffer, text.x, text.y, text.scale, "SPACE: play tone",
              COLOR_GRAY);
  }
  cursor_newline(&text);
  cursor_gap(&text, 0.5f);

  /* Mode controls */
  draw_text(backbuffer, text.x, text.y, text.scale, "TAB: cycle scale mode",
            COLOR_GRAY);
  cursor_newline(&text);

  draw_text(backbuffer, text.x, text.y, text.scale, get_mode_name(scale_mode),
            COLOR_CYAN);
  cursor_newline(&text);

  /* Debug info */
  char buf[64];

  snprintf(buf, sizeof(buf), "Backbuffer: %dx%d", backbuffer->width,
           backbuffer->height);
  draw_text(backbuffer, text.x, text.y, text.scale, buf, COLOR_CYAN);
  cursor_newline(&text);

  snprintf(buf, sizeof(buf), "Scale X:%.1f Y:%.1f %s", ctx.world_to_px_x,
           ctx.world_to_px_y,
           (ctx.world_to_px_x != ctx.world_to_px_y) ? "(DISTORTED!)" : "(OK)");
  draw_text(backbuffer, text.x, text.y, text.scale, buf,
            (ctx.world_to_px_x != ctx.world_to_px_y) ? COLOR_RED : COLOR_GREEN);

  /* ── FPS Counter (screen-space: top-right) ────────────────────────── */
  snprintf(buf, sizeof(buf), "FPS: %d", fps);

  float fps_width = 0;
  for (const char *ch = buf; *ch; ch++)
    fps_width += GLYPH_W * text.scale;

  draw_text(backbuffer, (float)backbuffer->width - fps_width - 8.0f, 8.0f,
            text.scale, buf, COLOR_GREEN);
}
