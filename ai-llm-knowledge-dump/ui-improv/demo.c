#include "./demo.h"
#include "../utils/backbuffer.h"
#include "../utils/base.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../utils/render_context.h"
#include <stdio.h>

/* ── Static State ───────────────────────────────────────────────────── */
static ScreenShake g_shake = {0};
static float g_time = 0.0f;

/* ── Mode Name Helper ───────────────────────────────────────────────── */
static const char *get_window_scale_mode_name(WindowScaleMode mode) {
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
void demo_render(Backbuffer *backbuffer, GameInput *input, int fps,
                 WindowScaleMode scale_mode) {

  /* Update time (assuming 60fps, should use actual dt) */
  float dt = 1.0f / 60.0f;
  g_time += dt;

  /* Create render context */
  RenderContext ctx = make_render_context(backbuffer);

  /* Update and apply screen shake */
  shake_update(&g_shake, dt);
  if (shake_is_active(&g_shake)) {
    camera_apply_shake(&ctx, &g_shake);
  }

  /* Trigger shake on space press */
  if (input && input->buttons.play_tone.ended_down &&
      input->buttons.play_tone.half_transitions > 0) {
    g_shake = shake_make(0.3f, 0.3f, 30.0f);
  }

  /* ══════════════════════════════════════════════════════════════════
   *  BACKGROUND
   * ══════════════════════════════════════════════════════════════════ */
  draw_rect(backbuffer, 0, 0, ctx.screen_w, ctx.screen_h, COLOR_BG);

  /* ══════════════════════════════════════════════════════════════════
   *  GAME OBJECTS (World Space)
   *  These use world coordinates (16×12 units)
   * ══════════════════════════════════════════════════════════════════ */

  /* Red rectangle at world (1, 1), size (4, 2) */
  draw_rect(backbuffer, W2S_X(&ctx, 1.0f), W2S_Y(&ctx, 1.0f), W2S_W(&ctx, 4.0f),
            W2S_H(&ctx, 2.0f), COLOR_RED);

  /* Green rectangle at world (6, 1) */
  draw_rect(backbuffer, W2S_X(&ctx, 6.0f), W2S_Y(&ctx, 1.0f), W2S_W(&ctx, 4.0f),
            W2S_H(&ctx, 2.0f), COLOR_GREEN);

  /* Blue rectangle at world (11, 1) */
  draw_rect(backbuffer, W2S_X(&ctx, 11.0f), W2S_Y(&ctx, 1.0f),
            W2S_W(&ctx, 4.0f), W2S_H(&ctx, 2.0f), COLOR_BLUE);

  /* Semi-transparent yellow overlay */
  draw_rect(backbuffer, W2S_X(&ctx, 3.0f), W2S_Y(&ctx, 0.6f), W2S_W(&ctx, 4.0f),
            W2S_H(&ctx, 2.8f), 1.0f, 0.843f, 0.0f, 0.47f);

  /* Animated circle (using world coordinates) */
  float circle_x = 8.0f + sinf(g_time * 2.0f) * 3.0f;
  float circle_y = 9.0f + cosf(g_time * 1.5f) * 1.5f;
  draw_rect(backbuffer, W2S_X(&ctx, circle_x - 0.5f),
            W2S_Y(&ctx, circle_y - 0.5f), W2S_W(&ctx, 1.0f), W2S_H(&ctx, 1.0f),
            COLOR_CYAN);

  /* ══════════════════════════════════════════════════════════════════
   *  UI TEXT (Reference Space + TextCursor)
   *  Starts at world position, flows in screen pixels
   * ══════════════════════════════════════════════════════════════════ */

  TextCursor text = make_text_cursor_world(&ctx, 1.0f, 4.0f);

  /* Title (large) */
  float large_scale = ctx.text_scale * 2.0f;
  float large_line_h = GLYPH_H * large_scale * 1.4f;

  draw_text(backbuffer, text.x, text.y, large_scale, "PLATFORM READY",
            COLOR_WHITE);
  text.y += large_line_h;

  /* Input feedback */
  if (input && input->buttons.play_tone.ended_down) {
    float highlight_h = GLYPH_H * text.scale * 1.2f;
    draw_rect(backbuffer, text.x, text.y, W2S_W(&ctx, 6.0f), highlight_h,
              COLOR_YELLOW);
    draw_text(backbuffer, text.x + 4.0f, text.y + 2.0f, text.scale,
              "SPACE: play tone (SHAKE!)", COLOR_BLACK);
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

  draw_text(backbuffer, text.x, text.y, text.scale,
            get_window_scale_mode_name(scale_mode), COLOR_CYAN);
  cursor_newline(&text);

  /* Debug info */
  char dim_str[64];
  snprintf(dim_str, sizeof(dim_str), "Backbuffer: %dx%d", backbuffer->width,
           backbuffer->height);
  draw_text(backbuffer, text.x, text.y, text.scale, dim_str, COLOR_CYAN);
  cursor_newline(&text);

  char scale_str[64];
  snprintf(scale_str, sizeof(scale_str), "Scale: %.2f (X:%.2f Y:%.2f) %s",
           ctx.scale_uniform, ctx.scale_x, ctx.scale_y,
           (ctx.scale_x != ctx.scale_y) ? "DISTORTED!" : "OK");
  draw_text(backbuffer, text.x, text.y, text.scale, scale_str,
            (ctx.scale_x != ctx.scale_y) ? COLOR_RED : COLOR_GREEN);
  cursor_newline(&text);

  /* Viewport info */
  char vp_str[64];
  snprintf(vp_str, sizeof(vp_str), "Viewport: (%.0f,%.0f) %.0fx%.0f",
           ctx.viewport.x, ctx.viewport.y, ctx.viewport.w, ctx.viewport.h);
  draw_text(backbuffer, text.x, text.y, text.scale, vp_str, COLOR_GRAY);

  /* ══════════════════════════════════════════════════════════════════
   *  ANCHORED UI (Normalized Space)
   *  These stay in corners regardless of resolution
   * ══════════════════════════════════════════════════════════════════ */

  /* FPS counter - top right */
  {
    char fps_str[32];
    snprintf(fps_str, sizeof(fps_str), "FPS: %d", fps);
    float fps_w = text_width(fps_str, ctx.text_scale);

    Vec2 pos = anchor_pivot(&ctx, ANCHOR_TOP_RIGHT, fps_w, ctx.line_height,
                            1.0f, 0.0f,  /* pivot: right edge, top */
                            8.0f, 8.0f); /* margin */

    draw_text(backbuffer, pos.x, pos.y, ctx.text_scale, fps_str, COLOR_GREEN);
  }

  /* Version - bottom left */
  {
    const char *version = "v1.0.0";
    Vec2 pos = anchor_pivot(
        &ctx, ANCHOR_BOTTOM_LEFT, text_width(version, ctx.text_scale),
        ctx.line_height, 0.0f, 1.0f, /* pivot: left edge, bottom */
        8.0f, 8.0f);                 /* margin */

    draw_text(backbuffer, pos.x, pos.y, ctx.text_scale, version, COLOR_GRAY);
  }

  /* Center message (animated) */
  {
    float alpha = (sinf(g_time * 3.0f) + 1.0f) * 0.5f; /* 0 to 1 */
    if (alpha > 0.3f) {
      const char *msg = "PRESS SPACE";
      float msg_w = text_width(msg, ctx.text_scale * 1.5f);
      float msg_h = GLYPH_H * ctx.text_scale * 1.5f;

      Vec2 pos = anchor_pivot(&ctx, ANCHOR_BOTTOM_CENTER, msg_w, msg_h, 0.5f,
                              1.0f,         /* pivot: center, bottom */
                              0.0f, 50.0f); /* margin */

      draw_text(backbuffer, pos.x, pos.y, ctx.text_scale * 1.5f, msg, 1.0f,
                1.0f, 1.0f, alpha);
    }
  }

  /* ══════════════════════════════════════════════════════════════════
   *  DEBUG OVERLAYS
   * ══════════════════════════════════════════════════════════════════ */
#ifdef DEBUG
  /* Uncomment to visualize coordinate systems */
  // debug_draw_world_grid(&ctx, backbuffer, 1.0f, 1.0f, 1.0f, 0.1f);
  // debug_draw_viewport(&ctx, backbuffer, 1.0f, 0.0f, 1.0f, 0.5f);
  // debug_draw_safe_area(&ctx, backbuffer, 0.0f, 1.0f, 0.0f, 0.5f);
  // debug_draw_anchors(&ctx, backbuffer, 1.0f, 1.0f, 0.0f, 0.8f);
#endif
}
