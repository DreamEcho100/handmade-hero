/* ═══════════════════════════════════════════════════════════════════════════
 * game/demo.c — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  HISTORICAL ARTIFACT — PRE-LESSON-15.  NOT COMPILED.  REFERENCE ONLY.  ║
 * ║                                                                          ║
 * ║  This file uses the old demo_render() signature from Lessons 03–09      ║
 * ║  (before the platform contract was split into demo_explicit.c /         ║
 * ║  demo_implicit.c in Lesson 09 and removed entirely in Lesson 15).       ║
 * ║                                                                          ║
 * ║  build-dev.sh does NOT compile this file.  It is kept here only as a   ║
 * ║  before/after reference snapshot showing the monolithic pre-split code. ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 *
 * LESSON 03 — demo_render() renders colored canvas.
 * LESSON 04 — Colored rectangles using unit coordinates.
 * LESSON 06 — "PLATFORM READY" text label.
 * LESSON 07 — Visual feedback for button presses (play_tone input).
 * LESSON 08 — FPS counter via snprintf + draw_text.
 * LESSON 09 — RenderContext + TextCursor: convert world→pixels ONCE per
 *             frame, then all layout stays in pixels.  Scale mode display.
 *
 * LESSON 15 — This file is REMOVED in the final template.  Game courses
 *             replace it with their own game/main.c containing
 *             game_init(), game_update(), game_render().
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdio.h>

#include "../game/base.h"
#include "../utils/backbuffer.h"
#include "../utils/base.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../utils/math.h"

/* ── World-space size (matches GAME_UNITS_W/H from utils/base.h) ─────────── */
#define WORLD_W GAME_UNITS_W
#define WORLD_H GAME_UNITS_H

/* ═══════════════════════════════════════════════════════════════════════════
 * RenderContext — LESSON 09
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Mental model:
 *   WORLD SPACE (16×12 grid)
 *       ↓  Convert ONCE at render boundary
 *   PIXEL SPACE (RenderContext holds the multipliers)
 *       ↓  Stay in pixels for all layout
 *   BACKBUFFER
 *
 * JS analogy: like a CSS `transform: scale()` applied once, then all child
 * elements use the already-transformed coordinate space.
 *
 * Why bother?  In DYNAMIC scale modes the backbuffer changes size each frame.
 * Without RenderContext you'd recompute units_to_pixels at every draw call —
 * fine for 5 calls, but fragile when the game has 500.  RenderContext makes
 * the conversion explicit and pays it once.
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  float world_to_px_x; /* horizontal: pixels per world unit */
  float world_to_px_y; /* vertical:   pixels per world unit */
  float text_scale;    /* glyph scale integer (>=1), cast to float for math */
  float line_height;   /* pixels between text lines */
} RenderContext;

static RenderContext make_render_context(Backbuffer *backbuffer) {
  RenderContext ctx;

  ctx.world_to_px_x = (float)backbuffer->width / WORLD_W;
  ctx.world_to_px_y = (float)backbuffer->height / WORLD_H;

  /* Text scale: 1 at the base resolution (800px wide = 50px/unit).
   * Grows proportionally in DYNAMIC modes so text stays legible.          */
  float base = (float)GAME_W / WORLD_W; /* 50.0 at 800×600 */
  float scale_factor = ctx.world_to_px_x / base;
  ctx.text_scale = MAX(scale_factor, 1.0f);

  ctx.line_height = GLYPH_H * ctx.text_scale * 1.4f;

  return ctx;
}

/* ── World → Pixel conversion helpers ────────────────────────────────────── */
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

/* ═══════════════════════════════════════════════════════════════════════════
 * TextCursor — LESSON 09
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * A cursor tracks a text position in PIXELS (already converted).
 * All movement operations (newline, gap) stay in pixel space — no repeated
 * world→pixel conversion.
 *
 * Usage:
 *   TextCursor text = make_cursor(&ctx, 1.0f, 4.0f); // world pos → pixels
 *   draw_text(bb, text.x, text.y, text.scale, "Hello", COLOR_WHITE);
 *   cursor_newline(&text);                             // advance in pixels
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  float x;      /* current X in pixels */
  float y;      /* current Y in pixels */
  float scale;  /* glyph scale */
  float line_h; /* line height in pixels */
} TextCursor;

/* Convert world position ONCE → pixel cursor. */
static TextCursor make_cursor(RenderContext *ctx, float wx, float wy) {
  return (TextCursor){
      .x = world_x(ctx, wx),
      .y = world_y(ctx, wy),
      .scale = ctx->text_scale,
      .line_h = ctx->line_height,
  };
}

/* Advance cursor down one line (stays in pixels). */
static void cursor_newline(TextCursor *c) { c->y += c->line_h; }

/* Advance cursor by a fractional line gap (e.g. 0.5 for half-line). */
static void cursor_gap(TextCursor *c, float multiplier) {
  c->y += c->line_h * multiplier;
}

/* ── Scale mode name helper ─────────────────────────────────────────────────
 */
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

/* ═══════════════════════════════════════════════════════════════════════════
 * demo_render — main entry point
 * ═══════════════════════════════════════════════════════════════════════════
 */
void demo_render(Backbuffer *backbuffer, GameInput *input, int fps,
                 WindowScaleMode scale_mode) {
  RenderContext ctx = make_render_context(backbuffer);

  /* ── LESSON 03 — Background ─────────────────────────────────────────── */
  draw_rect(backbuffer, 0, 0, (float)backbuffer->width,
            (float)backbuffer->height, COLOR_BG);

  /* ── LESSON 04 — Game objects in world space ────────────────────────── */
  draw_rect(backbuffer, world_x(&ctx, 1.0f), world_y(&ctx, 1.0f),
            world_w(&ctx, 4.0f), world_h(&ctx, 2.0f), COLOR_RED);
  draw_rect(backbuffer, world_x(&ctx, 6.0f), world_y(&ctx, 1.0f),
            world_w(&ctx, 4.0f), world_h(&ctx, 2.0f), COLOR_GREEN);
  draw_rect(backbuffer, world_x(&ctx, 11.0f), world_y(&ctx, 1.0f),
            world_w(&ctx, 4.0f), world_h(&ctx, 2.0f), COLOR_BLUE);

  /* Semi-transparent yellow overlay (alpha blend demo). */
  draw_rect(backbuffer, world_x(&ctx, 3.0f), world_y(&ctx, 0.6f),
            world_w(&ctx, 4.0f), world_h(&ctx, 2.8f), 1.0f, 0.843f, 0.0f,
            0.47f);

  /* ── LESSON 09 — Text UI using TextCursor (world pos → pixel layout) ── */
  TextCursor text = make_cursor(&ctx, 1.0f, 4.0f);
  float large_scale = ctx.text_scale * 2.0f;

  /* LESSON 06 — Title */
  draw_text(backbuffer, text.x, text.y, large_scale, "PLATFORM READY",
            COLOR_WHITE);
  text.y += GLYPH_H * large_scale * 1.4f; /* large line height */

  /* LESSON 07 — Input feedback */
  if (input->buttons.play_tone.ended_down &&
      input->buttons.play_tone.half_transitions > 0) {
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

  /* LESSON 09 — Scale mode controls */
  draw_text(backbuffer, text.x, text.y, text.scale, "TAB: cycle scale mode",
            COLOR_GRAY);
  cursor_newline(&text);

  draw_text(backbuffer, text.x, text.y, text.scale, get_mode_name(scale_mode),
            COLOR_CYAN);
  cursor_newline(&text);

  /* LESSON 09 — Debug info: backbuffer size + distortion warning */
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

  /* ── LESSON 08 — FPS counter (screen-space: top-right) ─────────────── */
  char fps_str[32];
  snprintf(fps_str, sizeof(fps_str), "FPS: %d", fps);

  float fps_width = 0;
  for (const char *ch = fps_str; *ch; ch++)
    fps_width += GLYPH_W * text.scale;

  draw_text(backbuffer, (float)backbuffer->width - fps_width - 8.0f, 8.0f,
            text.scale, fps_str, COLOR_GREEN);
}
