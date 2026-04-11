/* ═══════════════════════════════════════════════════════════════════════════
 * game/demo_explicit.c — Level 1 (explicit) coordinate demo
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 09 — demo_render() using Level 1 explicit coordinate helpers.
 *
 * COORDINATE MODE: RENDER_COORD_MODE=1 (or 3 hybrid — both available)
 *
 * In explicit mode the pixel math is VISIBLE at every draw call site:
 *
 *   draw_rect(bb,
 *     world_rect_px_x(&ctx, 1.0f, 4.0f), world_rect_px_y(&ctx, 4.0f, 2.0f),
 *     world_w(&ctx, 4.0f),               world_h(&ctx, 2.0f), COLOR_RED);
 *
 * LESSON 17 — Camera: pan with Arrow keys, zoom with Q/E, reset with R.
 *
 *   • g_camera (static GameCamera) persists across frames.
 *   • GameWorldConfig is built each frame from origin + g_camera.
 *   • HUD uses a SEPARATE hud_ctx (camera_zoom=1.0, no pan) so labels stay
 *     fixed on screen while world geometry moves.
 *   • STRESS_RECTS (#ifdef): draws N extra rects using world_rect_is_visible()
 *     to skip off-screen ones — demonstrates the culling API.
 *
 * LESSON 16 — Arena *scratch is used for per-frame scratch allocations.
 * LESSON 15 — This file is removed in the final template.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <math.h> /* powf */
#include <stdio.h>

#include "../game/base.h"
#include "../utils/arena.h"
#include "../utils/backbuffer.h"
#include "../utils/base.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../utils/perf.h"
#include "../utils/render.h"

/* ── Camera pan speed ───────────────────────────────────────────────────────
 */
#define CAM_PAN_SPEED 3.0f  /* world units per second while key held  */
#define CAM_ZOOM_RATE 60.0f /* frames-equivalent zoom rate            */

#define DEMO_POOL_SLOT_COUNT 8
#define DEMO_POOL_RECT_COUNT 5
#define DEMO_SLAB_LABEL_COUNT 4
#define DEMO_SLAB_SLOTS_PER_PAGE 4

typedef struct {
  float x;
  float y;
  float w;
  float h;
  float r;
  float g;
  float b;
  float a;
} DemoPoolRect;

typedef struct {
  float wx;
  float wy;
  float r;
  float g;
  float b;
  float a;
  char text[48];
} DemoSlabLabel;

typedef struct {
  PoolAllocator rect_pool;
  SlabAllocator label_slab;
  DemoPoolRect *rects[DEMO_POOL_RECT_COUNT];
  DemoSlabLabel *labels[DEMO_SLAB_LABEL_COUNT];
  int rect_count;
  int label_count;
} DemoLevelState;

typedef struct {
  int level_rebuild_count;
  int pool_reuse_hits;
  int slab_reuse_hits;
} DemoMemoryState;

static void demo_fill_pool_rect(DemoPoolRect *rect, float x, float y, float w,
                                float h, float r, float g, float b, float a) {
  if (!rect)
    return;
  rect->x = x;
  rect->y = y;
  rect->w = w;
  rect->h = h;
  rect->r = r;
  rect->g = g;
  rect->b = b;
  rect->a = a;
}

static void demo_fill_slab_label(DemoSlabLabel *label, float wx, float wy,
                                 float r, float g, float b, float a,
                                 const char *text) {
  if (!label)
    return;
  label->wx = wx;
  label->wy = wy;
  label->r = r;
  label->g = g;
  label->b = b;
  label->a = a;
  snprintf(label->text, sizeof(label->text), "%s", text ? text : "label");
}

static DemoMemoryState *demo_get_memory_state(Arena *perm) {
  static DemoMemoryState *memory_state = NULL;
  if (!memory_state && perm)
    memory_state = ARENA_PUSH_ZERO_STRUCT(perm, DemoMemoryState);
  return memory_state;
}

static DemoLevelState *demo_rebuild_level_state(Arena *level,
                                                DemoMemoryState *memory_state) {
  static const float rect_layout[DEMO_POOL_RECT_COUNT][8] = {
      {1.5f, 0.75f, 1.4f, 1.0f, 0.925f, 0.353f, 0.235f, 1.0f},
      {3.3f, 1.10f, 1.1f, 0.9f, 0.992f, 0.702f, 0.153f, 1.0f},
      {5.0f, 0.65f, 1.3f, 1.1f, 0.173f, 0.627f, 0.173f, 1.0f},
      {6.8f, 1.20f, 1.2f, 0.8f, 0.235f, 0.510f, 0.965f, 1.0f},
      {8.5f, 0.85f, 1.5f, 1.0f, 0.741f, 0.325f, 0.914f, 1.0f},
  };
  DemoLevelState *level_state;
  DemoPoolRect *reused_rect;
  DemoSlabLabel *reused_label;
  int i;

  if (!level)
    return NULL;

  arena_reset(level);
  level_state = ARENA_PUSH_ZERO_STRUCT(level, DemoLevelState);
  if (!level_state)
    return NULL;
  if (pool_init_in_arena(&level_state->rect_pool, level, sizeof(DemoPoolRect),
                         DEMO_POOL_SLOT_COUNT, "demo_rect_pool") != 0)
    return level_state;
  if (slab_init_in_arena(&level_state->label_slab, level, sizeof(DemoSlabLabel),
                         DEMO_SLAB_SLOTS_PER_PAGE, "demo_label_slab") != 0)
    return level_state;

  level_state->rect_count = DEMO_POOL_RECT_COUNT;
  for (i = 0; i < DEMO_POOL_RECT_COUNT; ++i) {
    level_state->rects[i] = (DemoPoolRect *)pool_alloc(&level_state->rect_pool);
    demo_fill_pool_rect(level_state->rects[i], rect_layout[i][0],
                        rect_layout[i][1], rect_layout[i][2], rect_layout[i][3],
                        rect_layout[i][4], rect_layout[i][5], rect_layout[i][6],
                        rect_layout[i][7]);
  }

  if (level_state->rects[2]) {
    DemoPoolRect *released_rect = level_state->rects[2];
    pool_free(&level_state->rect_pool, released_rect);
    reused_rect = (DemoPoolRect *)pool_alloc(&level_state->rect_pool);
    if (reused_rect == released_rect && memory_state)
      memory_state->pool_reuse_hits++;
    level_state->rects[2] = reused_rect;
    demo_fill_pool_rect(level_state->rects[2], 5.0f, 0.65f, 1.3f, 1.1f, 0.133f,
                        0.733f, 0.455f, 1.0f);
  }

  level_state->label_count = DEMO_SLAB_LABEL_COUNT;
  for (i = 0; i < DEMO_SLAB_LABEL_COUNT; ++i) {
    char text[48];
    snprintf(text, sizeof(text), "slab label %d", i + 1);
    level_state->labels[i] =
        (DemoSlabLabel *)slab_alloc(&level_state->label_slab);
    demo_fill_slab_label(level_state->labels[i], rect_layout[i][0],
                         rect_layout[i][1] + rect_layout[i][3] + 0.25f,
                         COLOR_WHITE, text);
  }

  if (level_state->labels[1]) {
    DemoSlabLabel *released_label = level_state->labels[1];
    slab_free(&level_state->label_slab, released_label);
    reused_label = (DemoSlabLabel *)slab_alloc(&level_state->label_slab);
    if (reused_label == released_label && memory_state)
      memory_state->slab_reuse_hits++;
    level_state->labels[1] = reused_label;
    demo_fill_slab_label(level_state->labels[1], 3.3f, 2.25f, COLOR_YELLOW,
                         "slab label 2 reused");
  }

  if (memory_state)
    memory_state->level_rebuild_count++;

  return level_state;
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
 * demo_render — entry point (Level 1 / explicit style)
 * ═══════════════════════════════════════════════════════════════════════════
 */
void demo_render(Backbuffer *backbuffer, GameInput *input, int fps,
                 WindowScaleMode scale_mode, GameWorldConfig world_config,
                 Arena *perm, Arena *level, Arena *scratch, float dt) {

  PERF_BEGIN_NAMED(dr_explicit_total, "demo_render/explicit/total");

  /* ── LESSON 17 — Persistent camera state ───────────────────────────────
   * g_camera lives in static storage: survives across frames, reset once.
   * Initialized to {0, 0, 1.0f}: no pan, normal zoom.                    */
  static GameCamera g_camera = {0.0f, 0.0f, 1.0f};
  static DemoLevelState *g_level_state = NULL;
  DemoMemoryState *memory_state = demo_get_memory_state(perm);

  /* ── LESSON 17 — Update camera from input ───────────────────────────── */
  if (input->buttons.cam_reset.ended_down &&
      input->buttons.cam_reset.half_transitions > 0) {
    g_camera.x = 0.0f;
    g_camera.y = 0.0f;
    g_camera.zoom = 1.0f;
    g_level_state = demo_rebuild_level_state(level, memory_state);
  }
  /* LESSON 17 — camera_pan() handles the sign difference between X and Y
   * so this code is correct for ALL CoordOrigins, not just TOP_LEFT.     */
  float cam_dx = (input->buttons.cam_right.ended_down ? CAM_PAN_SPEED : 0.0f) +
                 (input->buttons.cam_left.ended_down ? -CAM_PAN_SPEED : 0.0f);
  float cam_dy = (input->buttons.cam_up.ended_down ? CAM_PAN_SPEED : 0.0f) +
                 (input->buttons.cam_down.ended_down ? -CAM_PAN_SPEED : 0.0f);
  camera_pan(&g_camera.x, &g_camera.y, cam_dx, cam_dy,
             world_config_y_sign(&world_config), dt);
  if (input->buttons.zoom_in.ended_down)
    g_camera.zoom *= powf(1.05f, dt * CAM_ZOOM_RATE);
  if (input->buttons.zoom_out.ended_down)
    g_camera.zoom *= powf(0.95f, dt * CAM_ZOOM_RATE);
  /* Clamp zoom to a sane range */
  if (g_camera.zoom < 0.1f)
    g_camera.zoom = 0.1f;
  if (g_camera.zoom > 20.0f)
    g_camera.zoom = 20.0f;

  /* ── LESSON 17 — Build two render contexts ──────────────────────────────
   *
   * world_ctx: origin + camera applied → world geometry moves with camera.
   * hud_ctx:   same origin, zoom=1.0, no pan → HUD stays fixed on screen.
   *
   * Key teaching moment: you can call make_render_context() multiple times
   * per frame with different configs.  The switch runs once per call;
   * the draw calls themselves have no branches.                            */
  PERF_BEGIN_NAMED(dr_explicit_ctx, "demo_render/explicit/make_render_ctx");

  GameWorldConfig world_cfg = world_config;
  world_cfg.camera_x = g_camera.x;
  world_cfg.camera_y = g_camera.y;
  world_cfg.camera_zoom = g_camera.zoom;
  RenderContext ctx = make_render_context(backbuffer, world_cfg);

  /* HUD context — same origin, no camera effects */
  GameWorldConfig hud_cfg = world_config; /* copies origin */
  hud_cfg.camera_x = 0.0f;
  hud_cfg.camera_y = 0.0f;
  hud_cfg.camera_zoom = 1.0f;
  RenderContext hud = make_render_context(backbuffer, hud_cfg);

  PERF_END(dr_explicit_ctx);

  /* ── LESSON 03 — Background ─────────────────────────────────────────── */
  PERF_BEGIN_NAMED(dr_explicit_bg, "demo_render/explicit/draw_background");
  draw_rect(backbuffer, 0, 0, (float)backbuffer->width,
            (float)backbuffer->height, COLOR_BG);
  PERF_END(dr_explicit_bg);

  if (!g_level_state)
    g_level_state = demo_rebuild_level_state(level, memory_state);

  /* ── LESSON 04 — Colored rects in world space ───────────────────────────
   *
   * LESSON 17 — world_rect_is_visible() skips rects outside the viewport.
   * As you pan/zoom, rects disappear when they leave the backbuffer bounds.
   * The culling cost is 4 float ops — negligible vs the pixel fill saved.  */
  PERF_BEGIN_NAMED(dr_explicit_rects, "demo_render/explicit/world_rects_L1");

#define DRAW_WORLD_RECT_CULLED(wx, wy, ww, wh, ...)                            \
  if (world_rect_is_visible(&ctx, (wx), (wy), (ww), (wh)))                     \
  draw_rect(backbuffer, world_rect_px_x(&ctx, (wx), (ww)),                     \
            world_rect_px_y(&ctx, (wy), (wh)), world_w(&ctx, (ww)),            \
            world_h(&ctx, (wh)), __VA_ARGS__)

  DRAW_WORLD_RECT_CULLED(1.0f, 4.0f, 4.0f, 2.0f, COLOR_RED);
  DRAW_WORLD_RECT_CULLED(6.0f, 4.0f, 4.0f, 2.0f, COLOR_GREEN);
  DRAW_WORLD_RECT_CULLED(11.0f, 4.0f, 4.0f, 2.0f, COLOR_BLUE);
  DRAW_WORLD_RECT_CULLED(3.0f, 3.5f, 4.0f, 3.0f, 1.0f, 0.843f, 0.0f, 0.47f);

#ifdef STRESS_RECTS
  /* LESSON 17 — Stress test: N extra rects, each culled before draw.
   * Pan the camera far right to see most become invisible — frame time
   * stays near baseline because pixel work is skipped.                    */
  int visible_count = 0;
  for (int _si = 0; _si < STRESS_RECTS; _si++) {
    float _t = (float)_si / (float)STRESS_RECTS;
    float _wx = _t * (GAME_UNITS_W - 1.5f);
    float _wy = (float)(_si % 6) * 2.0f;
    if (world_rect_is_visible(&ctx, _wx, _wy, 1.5f, 1.5f)) {
      draw_rect(backbuffer, world_rect_px_x(&ctx, _wx, 1.5f),
                world_rect_px_y(&ctx, _wy, 1.5f), world_w(&ctx, 1.5f),
                world_h(&ctx, 1.5f), _t, 1.0f - _t, 0.5f, 1.0f);
      visible_count++;
    }
  }
#endif

#undef DRAW_WORLD_RECT_CULLED

  if (g_level_state) {
    int i;
    for (i = 0; i < g_level_state->rect_count; ++i) {
      DemoPoolRect *rect = g_level_state->rects[i];
      if (!rect)
        continue;
      if (world_rect_is_visible(&ctx, rect->x, rect->y, rect->w, rect->h)) {
        draw_rect(backbuffer, world_rect_px_x(&ctx, rect->x, rect->w),
                  world_rect_px_y(&ctx, rect->y, rect->h),
                  world_w(&ctx, rect->w), world_h(&ctx, rect->h), rect->r,
                  rect->g, rect->b, rect->a);
      }
    }

    for (i = 0; i < g_level_state->label_count; ++i) {
      DemoSlabLabel *label = g_level_state->labels[i];
      if (!label)
        continue;
      draw_text(backbuffer, world_x(&ctx, label->wx), world_y(&ctx, label->wy),
                (int)hud.text_scale, label->text, label->r, label->g, label->b,
                label->a);
    }
  }
  PERF_END(dr_explicit_rects);

  /* ── LESSON 09 — Text HUD (world pos → pixel cursor) ───────────────────
   *
   * LESSON 17 — HUD uses hud_ctx (no camera) so text stays fixed.         */
  PERF_BEGIN_NAMED(dr_explicit_text, "demo_render/explicit/draw_text_hud");
  TextCursor text = make_cursor(&hud, 1.0f, 9.0f);
  float large_scale = hud.text_scale * 2.0f;
  TempMemory hud_temp = arena_begin_temp(scratch);

  draw_text(backbuffer, text.x, text.y, (int)large_scale, "PLATFORM READY",
            COLOR_WHITE);
  text.y += (float)GLYPH_H * large_scale * 1.4f;

  draw_text(backbuffer, text.x, text.y, (int)hud.text_scale,
            "[EXPLICIT coords]", COLOR_CYAN);
  cursor_newline(&text);
  cursor_gap(&text, 0.4f);

  if (input->buttons.play_tone.ended_down) {
    float highlight_h = (float)GLYPH_H * text.scale * 1.2f;
    draw_rect(backbuffer, text.x, text.y, world_w(&hud, 6.0f), highlight_h,
              COLOR_YELLOW);
    draw_text(backbuffer, text.x + 4.0f, text.y + 2.0f, (int)text.scale,
              "SPACE: play tone", COLOR_BLACK);
  } else {
    draw_text(backbuffer, text.x, text.y, (int)text.scale, "SPACE: play tone",
              COLOR_GRAY);
  }
  cursor_newline(&text);
  cursor_gap(&text, 0.3f);

  draw_text(backbuffer, text.x, text.y, (int)text.scale,
            "TAB: cycle scale mode", COLOR_GRAY);
  cursor_newline(&text);
  draw_text(backbuffer, text.x, text.y, (int)text.scale,
            get_mode_name(scale_mode), COLOR_CYAN);
  cursor_newline(&text);
  cursor_gap(&text, 0.3f);

  /* LESSON 17 — Camera state display */
  draw_text(backbuffer, text.x, text.y, (int)text.scale,
            "Arrows:pan  Q/E:zoom  R:reset camera+level", COLOR_GRAY);
  cursor_newline(&text);

  char *buf = ARENA_PUSH_ARRAY(scratch, 96, char);
  if (buf) {
    snprintf(buf, 96, "Camera x:%.2f y:%.2f zoom:%.2f", g_camera.x, g_camera.y,
             g_camera.zoom);
    draw_text(
        backbuffer, text.x, text.y, (int)text.scale, buf,
        (g_camera.zoom != 1.0f || g_camera.x != 0.0f || g_camera.y != 0.0f)
            ? COLOR_YELLOW
            : COLOR_CYAN);
  }
  cursor_newline(&text);

  if (buf) {
    snprintf(buf, 96, "Backbuffer: %dx%d", backbuffer->width,
             backbuffer->height);
    draw_text(backbuffer, text.x, text.y, (int)text.scale, buf, COLOR_CYAN);
  }
  cursor_newline(&text);

  if (buf) {
    snprintf(
        buf, 96, "Scale X:%.1f Y:%.1f %s", ctx.world_to_px_x, ctx.world_to_px_y,
        (ctx.world_to_px_x != ctx.world_to_px_y) ? "(DISTORTED!)" : "(OK)");
    draw_text(backbuffer, text.x, text.y, (int)text.scale, buf,
              (ctx.world_to_px_x != ctx.world_to_px_y) ? COLOR_RED
                                                       : COLOR_GREEN);
  }
  cursor_newline(&text);

  if (buf && g_level_state) {
    snprintf(buf, 96, "perm:%d level rebuilds:%d pool used:%zu/%zu",
             memory_state ? 1 : 0,
             memory_state ? memory_state->level_rebuild_count : 0,
             g_level_state->rect_pool.slot_count -
                 g_level_state->rect_pool.free_count,
             g_level_state->rect_pool.slot_count);
    draw_text(backbuffer, text.x, text.y, (int)text.scale, buf, COLOR_CYAN);
  }
  cursor_newline(&text);

  if (buf && g_level_state) {
    snprintf(buf, 96, "slab pages:%zu used:%zu reuse p:%d s:%d",
             g_level_state->label_slab.page_count,
             g_level_state->label_slab.total_slots -
                 g_level_state->label_slab.free_slots,
             memory_state ? memory_state->pool_reuse_hits : 0,
             memory_state ? memory_state->slab_reuse_hits : 0);
    draw_text(backbuffer, text.x, text.y, (int)text.scale, buf, COLOR_CYAN);
  }
  cursor_newline(&text);

#ifdef STRESS_RECTS
  if (buf) {
    snprintf(buf, 96, "Stress: %d / %d visible", visible_count, STRESS_RECTS);
    draw_text(backbuffer, text.x, text.y, (int)text.scale, buf, COLOR_CYAN);
  }
#endif

  PERF_END(dr_explicit_text);

  /* ── LESSON 08 — FPS counter (screen-space: top-right, HUD ctx) ─────── */
  char *fps_str = ARENA_PUSH_ARRAY(scratch, 32, char);
  if (fps_str) {
    snprintf(fps_str, 32, "FPS: %d", fps);
    float fps_width = 0;
    for (const char *ch = fps_str; *ch; ch++)
      fps_width += (float)GLYPH_W * text.scale;
    draw_text(backbuffer, (float)backbuffer->width - fps_width - 8.0f, 8.0f,
              (int)text.scale, fps_str, COLOR_GREEN);
  }

  arena_end_temp(hud_temp);

  PERF_END(dr_explicit_total);
}
